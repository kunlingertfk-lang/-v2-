#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QStringList>
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

QString tempModelDir()
{
    QDir dir(QDir::tempPath());
    const QString path = dir.filePath(QStringLiteral("registered_classification_mlp_backend_smoke_model"));
    QDir(path).removeRecursively();
    dir.mkpath(path);
    return path;
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

cv::Mat makeFeatureImage()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(20, 20, 20));
    cv::rectangle(image, cv::Rect(20, 20, 40, 30), cv::Scalar(220, 220, 220), -1);
    return image;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    RegisteredClassificationModelMetadata metadata;
    metadata.modelType = registeredClassificationMlpModelType();
    metadata.schemaVersion = 1;
    metadata.featureVersion = registeredClassificationFeatureVersionV1();
    metadata.featureNames = registeredClassificationFeatureNamesV1();
    metadata.featureLength = metadata.featureNames.size();
    metadata.classLabels = {
        {0, QStringLiteral("OK")},
        {1, QStringLiteral("NG")}
    };
    metadata.thresholds.minScore = 80;
    metadata.thresholds.rejectScore = 60;
    metadata.thresholds.top2Gap = 0;
    metadata.mlp.numHidden = 16;
    metadata.mlp.maxIterations = 100;
    metadata.mlp.randSeed = 42;
    metadata.trainingSampleCount = 6;

    const QString modelDir = tempModelDir();
    const RegisteredClassificationModelPackageResult writeResult =
            writeRegisteredClassificationMetadata(modelDir, metadata);
    check(writeResult.success, "metadata write must succeed");
    check(QFileInfo(QDir(modelDir).filePath(QStringLiteral("metadata.json"))).exists(),
          "metadata.json must exist");

    RegisteredClassificationModelMetadata loaded;
    const RegisteredClassificationModelPackageResult readResult =
            readRegisteredClassificationMetadata(modelDir, &loaded);
    check(readResult.success, "metadata read must succeed");
    check(loaded.modelType == registeredClassificationMlpModelType(), "modelType must round-trip");
    check(loaded.featureVersion == registeredClassificationFeatureVersionV1(), "featureVersion must round-trip");
    check(loaded.featureLength == 28, "featureLength must be 28 for v1");
    check(loaded.featureNames.size() == loaded.featureLength, "featureNames count must match featureLength");
    check(loaded.classLabels.size() == 2, "classLabels must round-trip");
    check(loaded.classLabels.value(1).name == QStringLiteral("NG"), "class label name must round-trip");
    check(loaded.thresholds.rejectScore == 60, "rejectScore must round-trip");

    RegisteredClassificationModelMetadata invalid = metadata;
    invalid.featureLength = 27;
    const RegisteredClassificationModelPackageResult invalidWrite =
            writeRegisteredClassificationMetadata(QDir(modelDir).filePath(QStringLiteral("invalid")), invalid);
    check(!invalidWrite.success, "featureLength mismatch must fail");
    check(invalidWrite.status == QStringLiteral("feature_length_mismatch"),
          "featureLength mismatch status must be feature_length_mismatch");

    RegisteredClassificationModelMetadata missing;
    const RegisteredClassificationModelPackageResult missingRead =
            readRegisteredClassificationMetadata(QDir(modelDir).filePath(QStringLiteral("missing")), &missing);
    check(!missingRead.success, "missing metadata must fail");
    check(missingRead.status == QStringLiteral("model_file_not_found"),
          "missing metadata status must be model_file_not_found");

    RegisteredClassificationFeatureExtractor extractor;
    RegisteredClassificationFeatureConfig featureConfig;
    featureConfig.halconSoPath = QString();
    featureConfig.halconSoPathCandidates.clear();

    const RegisteredClassificationFeatureResult featureResult =
            extractor.extract(makeFeatureImage(), QRectF(0.1, 0.1, 0.7, 0.7), featureConfig);
    check(featureResult.success, "feature extraction must succeed on synthetic image");
    check(featureResult.feature.size() == 28, "feature vector must have v1 length");
    check(featureResult.featureNames == registeredClassificationFeatureNamesV1(),
          "feature names must match v1 metadata");
    check(featureResult.feature.value(0) > 0.0, "roiAspect must be positive");
    check(featureResult.feature.value(12) >= 0.0, "grayHist00 must be non-negative");

    const RegisteredClassificationFeatureResult emptyFeature =
            extractor.extract(cv::Mat(), QRectF(0, 0, 1, 1), featureConfig);
    check(!emptyFeature.success, "empty image feature extraction must fail");
    check(emptyFeature.status == QStringLiteral("image_empty"), "empty image status must be image_empty");

    const RegisteredClassificationFeatureResult invalidRoi =
            extractor.extract(makeFeatureImage(), QRectF(0, 0, 0.001, 0.001), featureConfig);
    check(!invalidRoi.success, "tiny ROI must fail");
    check(invalidRoi.status == QStringLiteral("invalid_roi"), "tiny ROI status must be invalid_roi");

    const QString nonHalconLibraryPath = nonHalconSharedLibraryPath();
    check(!nonHalconLibraryPath.isEmpty(),
          "must find a non-HALCON shared library candidate for symbol-missing coverage");
    if (!nonHalconLibraryPath.isEmpty()) {
        RegisteredClassificationFeatureConfig missingSymbolConfig = featureConfig;
        missingSymbolConfig.halconSoPath = nonHalconLibraryPath;

        const RegisteredClassificationFeatureResult missingSymbols =
                extractor.extract(makeFeatureImage(), QRectF(0.1, 0.1, 0.7, 0.7), missingSymbolConfig);
        check(!missingSymbols.success, "non-HALCON shared library must fail feature extraction");
        check(missingSymbols.status == QStringLiteral("halcon_symbol_missing"),
              "missing HALCON symbols status must be halcon_symbol_missing");
    }

    if (g_failures > 0)
        return 1;
    std::cout << "registered_classification_mlp_backend_smoke: metadata and feature checks passed" << std::endl;
    return 0;
}
