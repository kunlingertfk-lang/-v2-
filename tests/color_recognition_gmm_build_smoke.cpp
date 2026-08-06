#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/recognition/ColorRecognitionHalconRunner.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonArray>

#include <cmath>
#include <iostream>

namespace {

QString imageHash(const cv::Mat &image)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArray::number(image.rows) + 'x' + QByteArray::number(image.cols) + ':' +
                 QByteArray::number(image.type()) + ':');
    const int rowBytes = image.cols * static_cast<int>(image.elemSize());
    for (int row = 0; row < image.rows; ++row)
        hash.addData(reinterpret_cast<const char *>(image.ptr(row)), rowBytes);
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(hash.result().toHex()));
}

ColorRecognitionGmmBuildSample sample8(const QString &id,
                                       int classId,
                                       const QString &label,
                                       const cv::Scalar &bgr,
                                       int width = 32,
                                       int height = 32)
{
    ColorRecognitionGmmBuildSample sample;
    sample.sampleId = id;
    sample.classId = classId;
    sample.label = label;
    sample.image = cv::Mat(height, width, CV_8UC3, bgr).clone();
    sample.pixelFormat = QStringLiteral("BGR8");
    sample.validBits = 8;
    sample.bitShift = 0;
    sample.imageSha256 = imageHash(sample.image);
    return sample;
}

ColorRecognitionGmmBuildSample sample16(const QString &id,
                                        int classId,
                                        const QString &label,
                                        const cv::Scalar &bgr)
{
    ColorRecognitionGmmBuildSample sample;
    sample.sampleId = id;
    sample.classId = classId;
    sample.label = label;
    sample.image = cv::Mat(32, 32, CV_16UC3, bgr).clone();
    sample.pixelFormat = QStringLiteral("BGR16");
    sample.validBits = 12;
    sample.bitShift = 0;
    sample.imageSha256 = imageHash(sample.image);
    return sample;
}

ColorRecognitionGmmBuildConfig config()
{
    ColorRecognitionGmmBuildConfig config;
    QStringList tried;
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(QString(), &tried);
    config.halconSoPathCandidates = tried;
    config.colorChannels = QStringLiteral("ab");
    config.labels = {{QStringLiteral("red"), 10}, {QStringLiteral("green"), 20}};
    return config;
}

int fail(const ColorRecognitionGmmBuildResult &result, const char *message)
{
    std::cerr << message << ": status=" << result.status.toStdString()
              << ", message=" << result.message.toStdString() << std::endl;
    return 1;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ColorRecognitionHalconRunner runner;
    const ColorRecognitionGmmBuildConfig abConfig = config();
    const QVector<ColorRecognitionGmmBuildSample> oneRoiPerClass = {
        sample8(QStringLiteral("red-1"), 10, QStringLiteral("red"), cv::Scalar(0, 0, 255)),
        sample8(QStringLiteral("green-1"), 20, QStringLiteral("green"), cv::Scalar(0, 255, 0))
    };

    const ColorRecognitionGmmBuildResult built =
            runner.buildGmmTemplateModel(oneRoiPerClass, abConfig);
    if (!built.success || built.state != QStringLiteral("ReadyWithWarning") ||
        built.payload.value(QStringLiteral("samplingAlgorithmVersion")).toString() !=
            colorRecognitionGmmSamplingAlgorithmVersion() ||
        colorRecognitionGmmSamplingAlgorithmVersion() !=
            QStringLiteral("halcon_region_grid_points_v3") ||
        built.classIdOrder != QVector<int>({10, 20}) || built.classes.size() != 2 ||
        built.classes.at(0).requestedTrainingPixels != built.classes.at(0).trainingPixels ||
        built.classes.at(0).trainingPixels != built.classes.at(1).trainingPixels ||
        built.payload.value(QStringLiteral("roiSamplingDiagnostics")).toArray().size() != 2 ||
        built.artifact.serializedBytes.isEmpty() || built.artifact.serializedSize <= 0 ||
        !built.artifact.serializedSha256.startsWith(QStringLiteral("sha256:")) ||
        QByteArray::fromBase64(built.artifact.serializedGmmBase64.toLatin1()) !=
            built.artifact.serializedBytes) {
        return fail(built, "Two-class ab GMM build failed");
    }

    const ColorRecognitionGmmArtifactValidationResult validArtifact =
            runner.validateGmmModelArtifact(built.artifact, abConfig);
    if (!validArtifact.success || validArtifact.serializedSize != built.artifact.serializedSize)
        return fail(built, "Built GMM artifact must validate through HALCON deserialization");

    ColorRecognitionGmmArtifact tamperedArtifact = built.artifact;
    tamperedArtifact.serializedSha256 = QStringLiteral("sha256:tampered");
    const ColorRecognitionGmmArtifactValidationResult invalidArtifact =
            runner.validateGmmModelArtifact(tamperedArtifact, abConfig);
    if (invalidArtifact.success || invalidArtifact.status != QStringLiteral("model_artifact_invalid"))
        return fail(built, "Tampered GMM artifact hash must be rejected");

    const ColorRecognitionGmmBuildResult repeated =
            runner.buildGmmTemplateModel(oneRoiPerClass, abConfig);
    if (!repeated.success || repeated.trainingDataHash != built.trainingDataHash ||
        repeated.buildParamsHash != built.buildParamsHash) {
        return fail(repeated, "Repeated build must keep deterministic hashes");
    }

    ColorRecognitionGmmBuildConfig gridConfig = abConfig;
    gridConfig.maxSamplesPerClass = 256;
    const ColorRecognitionGmmBuildResult gridBuilt = runner.buildGmmTemplateModel({
        sample8(QStringLiteral("wide-red"), 10, QStringLiteral("red"),
                cv::Scalar(0, 0, 255), 80, 40),
        sample8(QStringLiteral("tall-green"), 20, QStringLiteral("green"),
                cv::Scalar(0, 255, 0), 40, 80)
    }, gridConfig);
    if (!gridBuilt.success || gridBuilt.classes.size() != 2 ||
        gridBuilt.classes.at(0).requestedTrainingPixels != 256 ||
        std::abs(gridBuilt.classes.at(0).trainingPixels - 256) > 16 ||
        std::abs(gridBuilt.classes.at(1).trainingPixels - 256) > 16 ||
        gridBuilt.classes.at(0).trainingPixels != gridBuilt.classes.at(1).trainingPixels ||
        gridBuilt.payload.value(QStringLiteral("roiSamplingDiagnostics")).toArray().size() != 2) {
        return fail(gridBuilt, "HALCON native grid sampling must stay within count tolerance");
    }

    ColorRecognitionGmmBuildConfig labConfig = abConfig;
    labConfig.colorChannels = QStringLiteral("lab");
    const ColorRecognitionGmmBuildResult labBuilt =
            runner.buildGmmTemplateModel(oneRoiPerClass, labConfig);
    if (!labBuilt.success || labBuilt.buildParamsHash == built.buildParamsHash ||
        labBuilt.colorChannels != QStringLiteral("lab")) {
        return fail(labBuilt, "Lab build contract must differ from ab");
    }

    QVector<ColorRecognitionGmmBuildSample> threeRois;
    for (int index = 0; index < 3; ++index) {
        threeRois.append(sample8(QStringLiteral("red-%1").arg(index), 10,
                                 QStringLiteral("red"), cv::Scalar(0, index, 250)));
        threeRois.append(sample8(QStringLiteral("green-%1").arg(index), 20,
                                 QStringLiteral("green"), cv::Scalar(index, 250, 0)));
    }
    const ColorRecognitionGmmBuildResult ready = runner.buildGmmTemplateModel(threeRois, abConfig);
    if (!ready.success || ready.state != QStringLiteral("Ready") ||
        ready.classes.at(0).maxCenters != 1) {
        return fail(ready, "Three ROI per class must produce Ready with one center");
    }

    for (const int roiCount : {5, 10}) {
        QVector<ColorRecognitionGmmBuildSample> samples;
        for (int index = 0; index < roiCount; ++index) {
            samples.append(sample8(QStringLiteral("red-center-%1-%2").arg(roiCount).arg(index),
                                   10, QStringLiteral("red"),
                                   cv::Scalar(index % 4, index % 7, 240 + index % 12)));
            samples.append(sample8(QStringLiteral("green-center-%1-%2").arg(roiCount).arg(index),
                                   20, QStringLiteral("green"),
                                   cv::Scalar(index % 5, 240 + index % 12, index % 3)));
        }
        const ColorRecognitionGmmBuildResult centerResult =
                runner.buildGmmTemplateModel(samples, abConfig);
        const int expectedMaxCenters = roiCount == 5 ? 2 : 3;
        if (!centerResult.success || centerResult.state != QStringLiteral("Ready") ||
            centerResult.classes.at(0).maxCenters != expectedMaxCenters ||
            centerResult.classes.at(1).maxCenters != expectedMaxCenters) {
            return fail(centerResult, "ROI-count center policy failed");
        }
    }

    const QVector<ColorRecognitionGmmBuildSample> samples16 = {
        sample16(QStringLiteral("red16"), 10, QStringLiteral("red"), cv::Scalar(0, 0, 4095)),
        sample16(QStringLiteral("green16"), 20, QStringLiteral("green"), cv::Scalar(0, 4095, 0))
    };
    const ColorRecognitionGmmBuildResult built16 = runner.buildGmmTemplateModel(samples16, abConfig);
    if (!built16.success)
        return fail(built16, "Valid 12-bit-in-16-bit samples must build");

    QVector<ColorRecognitionGmmBuildSample> mono = oneRoiPerClass;
    mono[0].image = cv::Mat(32, 32, CV_8UC1, cv::Scalar(128)).clone();
    mono[0].imageSha256 = imageHash(mono[0].image);
    const ColorRecognitionGmmBuildResult monoResult = runner.buildGmmTemplateModel(mono, abConfig);
    if (monoResult.success || monoResult.status !=
            QStringLiteral("unsupported_mono_for_color_recognition")) {
        return fail(monoResult, "Mono GMM sample must be rejected");
    }

    QVector<ColorRecognitionGmmBuildSample> outOfRange = samples16;
    outOfRange[0].image.setTo(cv::Scalar(0, 0, 5000));
    outOfRange[0].imageSha256 = imageHash(outOfRange[0].image);
    const ColorRecognitionGmmBuildResult rangeResult =
            runner.buildGmmTemplateModel(outOfRange, abConfig);
    if (rangeResult.success || rangeResult.status != QStringLiteral("pixel_value_out_of_range"))
        return fail(rangeResult, "Out-of-range 16-bit sample must be rejected");

    QVector<ColorRecognitionGmmBuildSample> missingMetadata = samples16;
    missingMetadata[0].validBits = -1;
    missingMetadata[0].bitShift = -1;
    const ColorRecognitionGmmBuildResult metadataResult =
            runner.buildGmmTemplateModel(missingMetadata, abConfig);
    if (metadataResult.success || metadataResult.status !=
            QStringLiteral("missing_pixel_format_metadata")) {
        return fail(metadataResult, "Missing 16-bit metadata must be rejected");
    }

    QVector<ColorRecognitionGmmBuildSample> hashMismatch = oneRoiPerClass;
    hashMismatch[0].imageSha256 = QStringLiteral("sha256:tampered");
    const ColorRecognitionGmmBuildResult hashResult =
            runner.buildGmmTemplateModel(hashMismatch, abConfig);
    if (hashResult.success || hashResult.status != QStringLiteral("gmm_sample_hash_mismatch"))
        return fail(hashResult, "Sample hash mismatch must be rejected");

    ColorRecognitionGmmBuildConfig missingSymbolConfig = abConfig;
    missingSymbolConfig.halconSoPath = QStringLiteral("/lib/x86_64-linux-gnu/libc.so.6");
    const ColorRecognitionGmmBuildResult missingSymbol =
            runner.buildGmmTemplateModel(oneRoiPerClass, missingSymbolConfig);
    if (missingSymbol.success || missingSymbol.status !=
            QStringLiteral("halcon_symbol_profile_missing")) {
        return fail(missingSymbol, "Missing GMM symbol profile must be explicit");
    }

    std::cout << "color_recognition_gmm_build_smoke: PASS" << std::endl;
    return 0;
}
