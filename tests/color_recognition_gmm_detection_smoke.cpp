#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/recognition/ColorRecognitionHalconRunner.h"

#include <QCoreApplication>
#include <QCryptographicHash>

#include <iostream>
#include <utility>

namespace {
QString imageHash(const cv::Mat &image)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArray::number(image.rows) + 'x' + QByteArray::number(image.cols) + ':' +
                 QByteArray::number(image.type()) + ':');
    for (int row = 0; row < image.rows; ++row)
        hash.addData(reinterpret_cast<const char *>(image.ptr(row)),
                     image.cols * static_cast<int>(image.elemSize()));
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(hash.result().toHex()));
}

ColorRecognitionGmmBuildSample sample(const QString &id, int classId,
                                      const QString &label, const cv::Scalar &bgr)
{
    ColorRecognitionGmmBuildSample result;
    result.sampleId = id; result.classId = classId; result.label = label;
    result.image = cv::Mat(40, 40, CV_8UC3, bgr).clone();
    result.pixelFormat = QStringLiteral("BGR8"); result.validBits = 8; result.bitShift = 0;
    result.imageSha256 = imageHash(result.image);
    return result;
}

int fail(const QString &message, const ColorRecognitionGmmRunResult &result = {})
{
    std::cerr << message.toStdString() << ": " << result.status.toStdString()
              << " / " << result.message.toStdString() << std::endl;
    return 1;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ColorRecognitionHalconRunner runner;
    ColorRecognitionGmmBuildConfig buildConfig;
    buildConfig.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(), &buildConfig.halconSoPathCandidates);
    buildConfig.colorChannels = QStringLiteral("ab");
    buildConfig.labels = {{QStringLiteral("red"), 10}, {QStringLiteral("green"), 20}};
    const ColorRecognitionGmmBuildResult built = runner.buildGmmTemplateModel({
        sample(QStringLiteral("red"), 10, QStringLiteral("red"), cv::Scalar(0, 0, 255)),
        sample(QStringLiteral("green"), 20, QStringLiteral("green"), cv::Scalar(0, 255, 0))
    }, buildConfig);
    if (!built.success) return fail(QStringLiteral("build failed"));

    ColorRecognitionGmmRunConfig config;
    if (config.gmmRejectionThreshold != kColorRecognitionGmmDefaultRejectionThreshold)
        return fail(QStringLiteral("GMM K-sigma default threshold is incorrect"));
    config.halconSoPath = buildConfig.halconSoPath;
    config.halconSoPathCandidates = buildConfig.halconSoPathCandidates;
    config.pixelFormat = QStringLiteral("BGR8"); config.validBits = 8; config.bitShift = 0;
    config.modelState = built.state;
    config.algorithmVersion = built.algorithmVersion;
    config.featureSchemaVersion = built.featureSchemaVersion;
    config.colorChannels = built.colorChannels;
    config.trainingDataHash = built.trainingDataHash;
    config.buildParamsHash = built.buildParamsHash;
    config.maxSamplesPerClass = buildConfig.maxSamplesPerClass;
    config.classIdOrder = built.classIdOrder;
    config.labels = buildConfig.labels;
    config.classes = built.classes;
    config.artifact = built.artifact;
    config.gmmRejectionThreshold = 0.0;
    config.minScore = 90; config.minCategoryConfidence = 90; config.minClassifiedCoverage = 90;

    const cv::Mat red(48, 48, CV_8UC3, cv::Scalar(0, 0, 255));
    ColorRecognitionGmmRunResult result = runner.runGmmModel(red, config);
    if (!result.success || !result.ok || result.predictedClassId != 10 || result.score < 99.0 ||
        result.categoryConfidence < 0.99 || result.classifiedCoverage < 0.99)
        return fail(QStringLiteral("pure red classification failed"), result);
    if (result.overlays.size() < 2 ||
        result.overlays.last().label != QStringLiteral("color_result_text") ||
        result.overlays.last().extra.value(QStringLiteral("status")).toString() != QStringLiteral("OK") ||
        !result.overlays.last().extra.value(QStringLiteral("anchorRect")).isObject())
        return fail(QStringLiteral("GMM OK text overlay must match the HSV display contract"), result);

    cv::Mat mixed(40, 40, CV_8UC3, cv::Scalar(0, 255, 0));
    mixed(cv::Rect(0, 0, 10, 40)).setTo(cv::Scalar(0, 0, 255));
    config.minScore = 80;
    result = runner.runGmmModel(mixed, config);
    if (!result.success || result.ok || result.predictedClassId != 20 || result.score < 74.0 || result.score > 76.0 ||
        !result.failureReasons.contains(QStringLiteral("score_below_minimum")))
        return fail(QStringLiteral("mixed area judgment failed"), result);
    if (result.overlays.size() < 2 ||
        result.overlays.last().label != QStringLiteral("color_result_text") ||
        result.overlays.last().extra.value(QStringLiteral("status")).toString() != QStringLiteral("NG"))
        return fail(QStringLiteral("GMM NG text overlay must match the HSV display contract"), result);

    config.gmmRejectionThreshold = 0.5;
    config.minScore = 0; config.minCategoryConfidence = 0; config.minClassifiedCoverage = 90;
    const cv::Mat blue(40, 40, CV_8UC3, cv::Scalar(255, 0, 0));
    result = runner.runGmmModel(blue, config);
    if (!result.success || result.ok || result.classifiedCoverage >= 0.1 ||
        !result.failureReasons.contains(QStringLiteral("classified_coverage_below_minimum")))
        return fail(QStringLiteral("rejection coverage judgment failed"), result);
    config.gmmRejectionThreshold = 0.0;

    config.judgeMode = QStringLiteral("expected_class"); config.expectedClassId = 10;
    config.minScore = 0; config.minCategoryConfidence = 0; config.minClassifiedCoverage = 0;
    result = runner.runGmmModel(mixed, config);
    if (!result.success || result.ok || result.failureReasons.value(0) != QStringLiteral("expected_class_mismatch"))
        return fail(QStringLiteral("expected class judgment failed"), result);

    config.enablePositionCorrection = true;
    config.positionCorrection.requested = true;
    config.positionCorrection.applied = true;
    config.positionCorrection.sourceId =
            QStringLiteral("reference.positionCorrection");
    config.positionCorrection.referenceToRunHomMat2D =
            {1.0, 0.0, 0.0, 0.0, 1.0, 5.0};
    result = runner.runGmmModel(red, config);
    if (!result.success ||
        !result.payload.value(QStringLiteral("positionCorrectionApplied"))
        .toBool(false) ||
        result.overlays.isEmpty() ||
        result.overlays.first().extra.value(QStringLiteral("role")).toString()
        != QStringLiteral("detect_roi"))
        return fail(QStringLiteral("GMM position correction failed"), result);
    config.enablePositionCorrection = false;
    config.positionCorrection = PositionCorrectionContext();

    const cv::Mat mono(32, 32, CV_8UC1, cv::Scalar(128));
    result = runner.runGmmModel(mono, config);
    if (result.success || result.status != QStringLiteral("unsupported_mono_for_color_recognition"))
        return fail(QStringLiteral("mono boundary failed"), result);

    cv::Mat red16(32, 32, CV_16UC3, cv::Scalar(0, 0, 4095));
    config.pixelFormat = QStringLiteral("BGR16"); config.validBits = 12; config.bitShift = 0;
    config.judgeMode = QStringLiteral("min_score"); config.minScore = 90;
    config.minCategoryConfidence = 90; config.minClassifiedCoverage = 90;
    result = runner.runGmmModel(red16, config);
    if (!result.success || !result.ok || result.predictedClassId != 10)
        return fail(QStringLiteral("16-bit normalized detection failed"), result);

    config.pixelFormat = QStringLiteral("BGR8"); config.validBits = 8; config.bitShift = 0;
    config.detectMaskPolygonNormalized = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    result = runner.runGmmModel(red, config);
    if (result.success || result.status != QStringLiteral("masked_roi_empty"))
        return fail(QStringLiteral("fully masked ROI boundary failed"), result);
    config.detectMaskPolygonNormalized.clear();

    const QString currentBuildParamsHash = config.buildParamsHash;
    config.buildParamsHash = QStringLiteral("sha256:legacy_sampling_contract");
    result = runner.runGmmModel(red, config);
    if (result.success || result.status != QStringLiteral("model_signature_mismatch"))
        return fail(QStringLiteral("legacy sampling signature must require rebuild"), result);
    config.buildParamsHash = currentBuildParamsHash;

    config.pixelFormat = QStringLiteral("BGRA8");
    result = runner.runGmmModel(red, config);
    if (result.success || result.status != QStringLiteral("invalid_pixel_format_metadata") ||
        result.payload.value(QStringLiteral("declaredPixelFormat")).toString() != QStringLiteral("BGRA8") ||
        result.payload.value(QStringLiteral("actualMatDepth")).toInt() != CV_8U ||
        result.payload.value(QStringLiteral("actualChannelCount")).toInt() != 3)
        return fail(QStringLiteral("input metadata diagnostics boundary failed"), result);
    config.pixelFormat = QStringLiteral("BGR8");

    std::swap(config.classIdOrder[0], config.classIdOrder[1]);
    result = runner.runGmmModel(red, config);
    if (result.success || result.status != QStringLiteral("invalid_gmm_class_order"))
        return fail(QStringLiteral("tampered class order boundary failed"), result);

    std::cout << "color_recognition_gmm_detection_smoke: PASS" << std::endl;
    return 0;
}
