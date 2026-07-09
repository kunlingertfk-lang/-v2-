#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"
#include "algorithms/recognition/RegisteredClassificationTrainingRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
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
    return QDir::tempPath();
}

QString smokeModelDir(const QString &name)
{
    QDir dir(tempModelDir());
    const QString path = dir.filePath(QStringLiteral("registered_classification_mlp_backend_smoke_model/%1").arg(name));
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

RegisteredClassificationTrainingRequest makeTrainingRequest(const QString &modelDir)
{
    RegisteredClassificationTrainingRequest request;
    request.halconSoPath = QString();
    request.outputModelDir = modelDir;
    request.classLabels = {
        {0, QStringLiteral("Bright")},
        {1, QStringLiteral("Dark")}
    };
    request.mlp.numHidden = 8;
    request.mlp.maxIterations = 100;
    request.mlp.randSeed = 42;
    for (int i = 0; i < 4; ++i) {
        cv::Mat bright(80, 100, CV_8UC3, cv::Scalar(30, 30, 30));
        cv::rectangle(bright, cv::Rect(20, 20, 40, 30), cv::Scalar(220, 220, 220), -1);
        request.samples.append({bright, QRectF(0.2, 0.2, 0.4, 0.4), 0});

        cv::Mat dark(80, 100, CV_8UC3, cv::Scalar(220, 220, 220));
        cv::rectangle(dark, cv::Rect(20, 20, 40, 30), cv::Scalar(30, 30, 30), -1);
        request.samples.append({dark, QRectF(0.2, 0.2, 0.4, 0.4), 1});
    }
    return request;
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

    const QString modelDir = smokeModelDir(QStringLiteral("metadata"));
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

    const QString trainedModelDir = smokeModelDir(QStringLiteral("trained"));
    RegisteredClassificationTrainingRunner trainer;
    const RegisteredClassificationTrainingResult trainResult =
            trainer.train(makeTrainingRequest(trainedModelDir));
    check(trainResult.success, "MLP training must succeed");
    check(trainResult.status == QStringLiteral("ok"), "training status must be ok");

    RegisteredClassificationTrainingRequest oneClass = makeTrainingRequest(
                smokeModelDir(QStringLiteral("one_class")));
    oneClass.classLabels = {{0, QStringLiteral("Only")}};
    const RegisteredClassificationTrainingResult oneClassResult = trainer.train(oneClass);
    check(!oneClassResult.success, "one-class training must fail");
    check(oneClassResult.status == QStringLiteral("training_not_enough_classes"),
          "one-class training status must be training_not_enough_classes");

    check(QFileInfo(registeredClassificationMlpPath(trainedModelDir)).exists(),
          "training must create model.gmc");
    check(QFileInfo(registeredClassificationMetadataPath(trainedModelDir)).exists(),
          "training must create metadata.json");
    check(QFileInfo(registeredClassificationTrainingReportPath(trainedModelDir)).exists(),
          "training must create training_report.json");
    QFile trainingReportFile(registeredClassificationTrainingReportPath(trainedModelDir));
    check(trainingReportFile.open(QIODevice::ReadOnly), "training_report.json must be readable");
    if (trainingReportFile.isOpen()) {
        const QJsonObject trainingReport =
                QJsonDocument::fromJson(trainingReportFile.readAll()).object();
        trainingReportFile.close();
        check(trainingReport.contains(QStringLiteral("Error")),
              "training_report.json must contain Error");
        check(trainingReport.contains(QStringLiteral("ErrorLog")),
              "training_report.json must contain ErrorLog");
    }

    if (g_failures > 0)
        return 1;
    std::cout << "registered_classification_mlp_backend_smoke: metadata, feature, and training checks passed" << std::endl;
    return 0;
}
