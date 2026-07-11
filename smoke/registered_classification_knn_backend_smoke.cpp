#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"
#include "algorithms/recognition/RegisteredClassificationHalconRunner.h"
#include "algorithms/recognition/RegisteredClassificationKnnRuntime.h"
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "algorithms/recognition/RegisteredClassificationTrainingRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <cmath>
#include <iostream>

#include <opencv2/imgproc.hpp>

namespace {

int g_failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++g_failures;
    }
}

QString smokeDir(const QString &name)
{
    const QString root = QDir(QDir::tempPath()).filePath(
                QStringLiteral("registered_classification_knn_backend_smoke/%1").arg(name));
    QDir(root).removeRecursively();
    QDir().mkpath(root);
    return root;
}

QString nonHalconSharedLibraryPath()
{
    const QStringList candidates = {
        QStringLiteral("/lib/x86_64-linux-gnu/libc.so.6"),
        QStringLiteral("/usr/lib/x86_64-linux-gnu/libc.so.6"),
        QStringLiteral("/lib64/libc.so.6")
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

cv::Mat makePartImage(bool bright, int angle, const QSize &size = QSize(120, 100))
{
    cv::Mat image(size.height(), size.width(), CV_8UC3,
                  bright ? cv::Scalar(25, 25, 25) : cv::Scalar(230, 230, 230));
    const cv::Scalar part = bright ? cv::Scalar(230, 230, 230) : cv::Scalar(25, 25, 25);
    if (bright) {
        cv::RotatedRect rectangle(cv::Point2f(size.width() * 0.5F, size.height() * 0.5F),
                                  cv::Size2f(size.width() * 0.38F, size.height() * 0.32F),
                                  static_cast<float>(angle));
        cv::Point2f points[4];
        rectangle.points(points);
        std::vector<cv::Point> contour;
        for (const cv::Point2f &point : points)
            contour.emplace_back(cvRound(point.x), cvRound(point.y));
        cv::fillConvexPoly(image, contour, part);
    } else {
        cv::circle(image, cv::Point(size.width() / 2, size.height() / 2), size.height() / 5, part, -1);
    }
    return image;
}

RegisteredClassificationFeatureRegion regionFor(const QRectF &rect)
{
    RegisteredClassificationFeatureRegion region;
    region.type = QStringLiteral("rectangle");
    region.rectNormalized = rect;
    return region;
}

RegisteredClassificationTrainingRequest makeTrainingRequest(const QString &modelDir,
                                                             int samplesPerClass = 3)
{
    RegisteredClassificationTrainingRequest request;
    request.outputModelDir = modelDir;
    request.classLabels = {{0, QStringLiteral("Bright")}, {1, QStringLiteral("Dark")}};
    request.thresholds.minSimilarity = 80;
    request.thresholds.minMargin = 8;
    for (int index = 0; index < samplesPerClass; ++index) {
        const QRectF roi = (index == 1) ? QRectF(0.16, 0.18, 0.68, 0.64)
                                        : (index == 2) ? QRectF(0.22, 0.22, 0.56, 0.56)
                                                       : QRectF(0.20, 0.20, 0.60, 0.60);
        RegisteredClassificationTrainingSample bright;
        bright.image = makePartImage(true, index * 16);
        bright.region = regionFor(roi);
        bright.classId = 0;
        request.samples.append(bright);
        RegisteredClassificationTrainingSample dark;
        dark.image = makePartImage(false, -index * 13);
        dark.region = regionFor(roi);
        dark.classId = 1;
        request.samples.append(dark);
    }
    return request;
}

RegisteredClassificationHalconConfig makeInferenceConfig(const QString &modelDir)
{
    RegisteredClassificationHalconConfig config;
    config.modelPath = modelDir;
    config.modelName = QStringLiteral("smoke_knn");
    config.modelType = registeredClassificationKnnModelType();
    config.detectRegionType = QStringLiteral("rectangle");
    config.roiNormalized = QRectF(0.2, 0.2, 0.6, 0.6);
    config.topK = 2;
    config.judgeMode = QStringLiteral("class_match");
    config.expectedLabel = QStringLiteral("Bright");
    config.minSimilarity = 0;
    config.minMargin = 0;
    return config;
}

cv::Mat makeOutOfRadiusImage()
{
    cv::Mat image(100, 120, CV_8UC3, cv::Scalar(25, 25, 25));
    cv::RotatedRect thinPart(cv::Point2f(60.0F, 50.0F), cv::Size2f(82.0F, 11.0F), 24.0F);
    cv::Point2f points[4];
    thinPart.points(points);
    std::vector<cv::Point> contour;
    for (const cv::Point2f &point : points)
        contour.emplace_back(cvRound(point.x), cvRound(point.y));
    cv::fillConvexPoly(image, contour, cv::Scalar(230, 230, 230));
    return image;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString trainedModelDir = smokeDir(QStringLiteral("trained"));
    RegisteredClassificationTrainingRunner trainer;
    const RegisteredClassificationTrainingResult trainResult =
            trainer.train(makeTrainingRequest(trainedModelDir));
    check(trainResult.success, "dual KNN training must succeed");
    check(trainResult.status == QStringLiteral("ok"), "dual KNN training status must be ok");
    check(trainResult.sampleCount == 6, "same-class ROI samples must count independently");

    check(QFileInfo(registeredClassificationSampleKnnPath(trainedModelDir)).exists(),
          "training must create model.gnc");
    check(QFileInfo(registeredClassificationCenterKnnPath(trainedModelDir)).exists(),
          "training must create class_centers.gnc");
    check(QFileInfo(registeredClassificationClassStatsPath(trainedModelDir)).exists(),
          "training must create class_stats.json");
    check(QFileInfo(registeredClassificationTrainingReportPath(trainedModelDir)).size() > 0,
          "training must persist a non-empty training report before promotion");
    QFile trainingReportFile(registeredClassificationTrainingReportPath(trainedModelDir));
    check(trainingReportFile.open(QIODevice::ReadOnly),
          "persisted training report must be readable");
    QJsonParseError trainingReportParseError;
    const QJsonDocument trainingReportDocument = QJsonDocument::fromJson(
                trainingReportFile.readAll(), &trainingReportParseError);
    trainingReportFile.close();
    check(trainingReportParseError.error == QJsonParseError::NoError
                  && trainingReportDocument.isObject(),
          "persisted training report must parse as a JSON object");
    const QJsonObject trainingReport = trainingReportDocument.object();
    const QJsonObject validSamplesByClass = trainingReport.value(
                QStringLiteral("validSamplesByClass")).toObject();
    const QJsonObject invalidSamplesByClass = trainingReport.value(
                QStringLiteral("invalidSamplesByClass")).toObject();
    const QJsonObject reportClassStats = trainingReport.value(
                QStringLiteral("classStats")).toObject();
    check(trainingReport.value(QStringLiteral("sampleCount")).toInt(-1) == 6
                  && validSamplesByClass.value(QStringLiteral("Bright")).toInt(-1) == 3
                  && validSamplesByClass.value(QStringLiteral("Dark")).toInt(-1) == 3
                  && invalidSamplesByClass.value(QStringLiteral("Bright")).toInt(-1) == 0
                  && invalidSamplesByClass.value(QStringLiteral("Dark")).toInt(-1) == 0
                  && reportClassStats.value(QStringLiteral("0")).toObject()
                     .value(QStringLiteral("sampleCount")).toInt(-1) == 3
                  && reportClassStats.value(QStringLiteral("0")).toObject()
                     .value(QStringLiteral("radiusEnabled")).toBool(false)
                  && reportClassStats.value(QStringLiteral("1")).toObject()
                     .value(QStringLiteral("sampleCount")).toInt(-1) == 3
                  && reportClassStats.value(QStringLiteral("1")).toObject()
                     .value(QStringLiteral("radiusEnabled")).toBool(false)
                  && trainingReport.value(QStringLiteral("sampleKnnBuildMs")).isDouble()
                  && trainingReport.value(QStringLiteral("sampleKnnBuildMs")).toDouble(-1.0) >= 0.0
                  && trainingReport.value(QStringLiteral("centerKnnBuildMs")).isDouble()
                  && trainingReport.value(QStringLiteral("centerKnnBuildMs")).toDouble(-1.0) >= 0.0
                  && trainingReport.value(QStringLiteral("elapsedMs")).isDouble()
                  && trainingReport.value(QStringLiteral("elapsedMs")).toDouble(-1.0) >= 0.0
                  && trainingReport.value(QStringLiteral("warnings")).isArray(),
          "persisted training report must retain the complete training diagnostics");
    check(!QFileInfo(QDir(trainedModelDir).filePath(QStringLiteral("model.gmc"))).exists(),
          "V2 package must not contain model.gmc");
    check(validateRegisteredClassificationKnnPackage(trainedModelDir).success,
          "trained package must pass the schema 2 contract");

    RegisteredClassificationClassStatsDocument classStats;
    check(readRegisteredClassificationClassStats(trainedModelDir, &classStats).success,
          "class stats must be readable");
    for (const RegisteredClassificationClassStats &stat : classStats.classes) {
        check(stat.sampleCount == 3, "each class must retain every valid ROI sample");
        check(stat.radiusEnabled, "three samples must enable a class radius");
    }

    RegisteredClassificationTrainingRequest oneSample = makeTrainingRequest(
                smokeDir(QStringLiteral("one_sample")), 1);
    const RegisteredClassificationTrainingResult oneSampleResult = trainer.train(oneSample);
    check(oneSampleResult.success, "one sample per class must train");
    RegisteredClassificationClassStatsDocument oneSampleStats;
    check(readRegisteredClassificationClassStats(oneSample.outputModelDir, &oneSampleStats).success,
          "one-sample stats must be readable");
    for (const RegisteredClassificationClassStats &stat : oneSampleStats.classes)
        check(!stat.radiusEnabled && stat.radius == 0.0, "one sample must disable radius rejection");

    RegisteredClassificationHalconRunner runner;
    const RegisteredClassificationHalconConfig config = makeInferenceConfig(trainedModelDir);
    const RegisteredClassificationHalconResult classified = runner.run(makePartImage(true, 31), config);
    check(classified.success && classified.ok, "transformed query must classify successfully");
    check(classified.predictedClassId == 0 && classified.predictedLabel == QStringLiteral("Bright"),
          "transformed Bright query must retain its class");
    check(classified.topClasses.size() == 2, "classification must preserve top K candidates");
    check(classified.payload.contains(QStringLiteral("sampleSimilarity")) &&
          classified.payload.contains(QStringLiteral("centerSimilarity")) &&
          classified.payload.contains(QStringLiteral("centerDistance")) &&
          classified.payload.contains(QStringLiteral("scoreMargin")) &&
          classified.payload.contains(QStringLiteral("classRadius")) &&
          classified.payload.contains(QStringLiteral("radiusEnabled")),
          "classification payload must contain fused-score and radius diagnostics");

    RegisteredClassificationHalconConfig lowSimilarityConfig = config;
    lowSimilarityConfig.minSimilarity = 100;
    const RegisteredClassificationHalconResult lowSimilarity = runner.run(makePartImage(true, 47), lowSimilarityConfig);
    check(lowSimilarity.success && !lowSimilarity.ok &&
          lowSimilarity.status == QStringLiteral("classification_rejected_low_similarity"),
          "low similarity must reject before all other rejection states");

    RegisteredClassificationHalconConfig ambiguousConfig = config;
    ambiguousConfig.minMargin = 100;
    const RegisteredClassificationHalconResult ambiguous = runner.run(makePartImage(true, 31), ambiguousConfig);
    check(ambiguous.success && !ambiguous.ok &&
          ambiguous.status == QStringLiteral("classification_rejected_ambiguous"),
          "ambiguous query must reject after low-similarity checks");

    for (RegisteredClassificationClassStats &stat : classStats.classes) {
        stat.radiusEnabled = true;
        stat.radius = 0.10;
    }
    check(writeRegisteredClassificationClassStats(trainedModelDir, classStats).success,
          "tight class radius rewrite must succeed");
    const RegisteredClassificationHalconResult outOfRadius = runner.run(makeOutOfRadiusImage(), config);
    check(outOfRadius.success && !outOfRadius.ok &&
          outOfRadius.status == QStringLiteral("classification_rejected_out_of_radius"),
          "out-of-radius query must reject after similarity and ambiguity checks");
    check(outOfRadius.predictedLabel == QStringLiteral("UNKNOWN")
                  && outOfRadius.predictedClassId == -1
                  && !outOfRadius.topClasses.isEmpty(),
          "UNKNOWN must retain the candidate payload");
    check(outOfRadius.payload.value(QStringLiteral("bestCandidateClassId")).toInt(-1) >= 0,
          "UNKNOWN payload must retain the best known candidate separately");

    RegisteredClassificationHalconConfig explicitLegacyConfig = config;
    explicitLegacyConfig.modelType = registeredClassificationLegacyMlpModelType();
    const RegisteredClassificationHalconResult explicitLegacy =
            runner.run(makePartImage(true, 0), explicitLegacyConfig);
    check(!explicitLegacy.success
                  && explicitLegacy.status == QStringLiteral("legacy_model_requires_retraining"),
          "explicit legacy model type must require retraining");

    const QString legacyOnlyDir = smokeDir(QStringLiteral("legacy_only"));
    QFile legacyOnlyFile(registeredClassificationMlpPath(legacyOnlyDir));
    check(legacyOnlyFile.open(QIODevice::WriteOnly | QIODevice::Truncate),
          "legacy-only model fixture must open");
    check(legacyOnlyFile.write("legacy") == 6, "legacy-only model fixture must write");
    legacyOnlyFile.close();
    const RegisteredClassificationModelInspection legacyOnlyInspection =
            inspectRegisteredClassificationModelPackage(legacyOnlyDir);
    check(legacyOnlyInspection.legacy && !legacyOnlyInspection.runnable
                  && legacyOnlyInspection.status == QStringLiteral("legacy_model_requires_retraining"),
          "model.gmc-only package inspection must require retraining");
    RegisteredClassificationHalconConfig legacyOnlyConfig = config;
    legacyOnlyConfig.modelPath = legacyOnlyDir;
    const RegisteredClassificationHalconResult legacyOnly =
            runner.run(makePartImage(true, 0), legacyOnlyConfig);
    check(!legacyOnly.success
                  && legacyOnly.status == QStringLiteral("legacy_model_requires_retraining"),
          "model.gmc-only runtime must require retraining");

    const QString centerPath = registeredClassificationCenterKnnPath(trainedModelDir);
    const QString savedCenterPath = centerPath + QStringLiteral(".saved");
    QFile::remove(savedCenterPath);
    check(QFile::rename(centerPath, savedCenterPath), "center model must be movable for missing-model coverage");
    const RegisteredClassificationHalconResult missingCenter = runner.run(makePartImage(true, 0), config);
    check(!missingCenter.success && missingCenter.status == QStringLiteral("model_package_incomplete"),
          "missing center model must be actionable");
    check(QFile::rename(savedCenterPath, centerPath), "center model must be restored after missing-model coverage");

    RegisteredClassificationKnnRuntime runtime;
    RegisteredClassificationKnnBuildRequest mismatchRequest;
    mismatchRequest.outputPath = registeredClassificationCenterKnnPath(trainedModelDir);
    mismatchRequest.featureLength = registeredClassificationFeatureNamesV2().size();
    mismatchRequest.maxNumClasses = 2;
    mismatchRequest.samples = {{QVector<double>(59, 1.0 / std::sqrt(59.0)), 0},
                               {QVector<double>(59, 1.0 / std::sqrt(59.0)), 2}};
    const RegisteredClassificationKnnRuntimeResult mismatchWrite = runtime.buildAndWrite({}, mismatchRequest);
    check(mismatchWrite.success, "mismatched center model fixture must write");
    const RegisteredClassificationHalconResult mismatch = runner.run(makePartImage(true, 0), config);
    check(!mismatch.success && mismatch.status == QStringLiteral("knn_model_mismatch"),
          "different KNN class-id sets must reject inference");

    const QString nonHalconLibrary = nonHalconSharedLibraryPath();
    check(!nonHalconLibrary.isEmpty(), "missing-symbol coverage needs a non-HALCON library");
    if (!nonHalconLibrary.isEmpty()) {
        RegisteredClassificationKnnBuildRequest missingSymbolRequest;
        missingSymbolRequest.outputPath = QDir(smokeDir(QStringLiteral("missing_symbol"))).filePath(QStringLiteral("model.gnc"));
        missingSymbolRequest.featureLength = registeredClassificationFeatureNamesV2().size();
        missingSymbolRequest.maxNumClasses = 2;
        missingSymbolRequest.samples = {{QVector<double>(59, 1.0 / std::sqrt(59.0)), 0},
                                        {QVector<double>(59, 1.0 / std::sqrt(59.0)), 1}};
        RegisteredClassificationKnnRuntimeConfig missingSymbolConfig;
        missingSymbolConfig.halconSoPath = nonHalconLibrary;
        const RegisteredClassificationKnnRuntimeResult missingSymbolResult =
                runtime.buildAndWrite(missingSymbolConfig, missingSymbolRequest);
        check(!missingSymbolResult.success && missingSymbolResult.status == QStringLiteral("halcon_symbol_missing"),
              "missing KNN symbol must return halcon_symbol_missing");
    }

    const RegisteredClassificationHalconResult repeated = runner.run(makePartImage(true, 0), config);
    check(!repeated.success && repeated.status == QStringLiteral("knn_model_mismatch"),
          "repeated inference must clean KNN handles deterministically");

    if (g_failures > 0)
        return 1;
    std::cout << "registered_classification_knn_backend_smoke: dual KNN training and inference checks passed" << std::endl;
    return 0;
}
