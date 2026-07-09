#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <iostream>

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

    if (g_failures > 0)
        return 1;
    std::cout << "registered_classification_mlp_backend_smoke: metadata checks passed" << std::endl;
    return 0;
}
