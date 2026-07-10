#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QtMath>

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

QString smokeModelDir(const QString &name)
{
    const QString path = QDir(QDir::tempPath()).filePath(
            QStringLiteral("registered_classification_feature_v2_smoke/%1").arg(name));
    QDir(path).removeRecursively();
    QDir().mkpath(path);
    return path;
}

bool createEmptyFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate);
}

bool writeSentinelFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write("gnc-sentinel") > 0;
}

bool writeMetadataJson(const QString &modelDir, const QJsonObject &metadata)
{
    QFile file(registeredClassificationMetadataPath(modelDir));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(QJsonDocument(metadata).toJson(QJsonDocument::Compact)) > 0;
}

RegisteredClassificationKnnModelMetadata validMetadata()
{
    RegisteredClassificationKnnModelMetadata metadata;
    metadata.modelType = registeredClassificationKnnModelType();
    metadata.schemaVersion = 2;
    metadata.featureVersion = registeredClassificationFeatureVersionV2();
    metadata.featureNames = registeredClassificationFeatureNamesV2();
    metadata.featureLength = metadata.featureNames.size();
    metadata.classLabels = {{0, QStringLiteral("A")}, {1, QStringLiteral("B")}};
    metadata.trainingSampleCount = 6;
    return metadata;
}

RegisteredClassificationClassStatsDocument validClassStats()
{
    RegisteredClassificationClassStatsDocument document;
    document.schemaVersion = 2;
    document.featureVersion = registeredClassificationFeatureVersionV2();

    RegisteredClassificationClassStats first;
    first.classId = 0;
    first.sampleCount = 3;
    first.radiusEnabled = true;
    first.radius = 0.35;
    first.meanDistance = 0.20;
    first.stdDevDistance = 0.05;
    first.maxDistance = 0.30;
    document.classes.append(first);

    RegisteredClassificationClassStats second;
    second.classId = 1;
    second.sampleCount = 3;
    document.classes.append(second);
    return document;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    check(registeredClassificationKnnModelType()
          == QStringLiteral("halcon_knn_registered_classification"),
          "V2 model type must be HALCON KNN");
    check(registeredClassificationLegacyMlpModelType()
          == QStringLiteral("halcon_mlp_registered_classification"),
          "legacy MLP model type must remain recognizable");
    check(registeredClassificationFeatureVersionV2()
          == QStringLiteral("halcon_registered_feature_v2"),
          "V2 feature version must be stable");

    const QStringList featureNames = registeredClassificationFeatureNamesV2();
    check(featureNames.size() == 59, "V2 feature contract must contain 59 names");
    check(QSet<QString>(featureNames.cbegin(), featureNames.cend()).size() == featureNames.size(),
          "V2 feature names must be unique");
    check(registeredClassificationFeatureGroupDimensions() == QVector<int>({13, 16, 18, 6, 6}),
          "V2 feature groups must be 13/16/18/6/6");

    const QVector<double> groupWeights = registeredClassificationFeatureGroupWeights();
    double weightSum = 0.0;
    for (double weight : groupWeights)
        weightSum += weight;
    check(qAbs(weightSum - 1.0) < 1e-9, "V2 feature group weights must sum to one");

    QVector<double> unit(59, 1.0);
    check(normalizeRegisteredClassificationFeature(&unit),
          "non-zero V2 vector must normalize");
    check(qAbs(registeredClassificationFeatureDistance(unit, unit)) < 1e-9,
          "identical vectors must have zero distance");
    check(qAbs(registeredClassificationSimilarityFromDistance(0.0) - 1.0) < 1e-9,
          "zero distance must map to similarity one");

    QVector<double> zero(59, 0.0);
    check(!normalizeRegisteredClassificationFeature(&zero), "zero V2 vector must be rejected");
    check(qIsNaN(registeredClassificationFeatureDistance(QVector<double>(59, 1.0),
                                                          QVector<double>(58, 1.0))),
          "feature distance must reject different lengths");

    const QVector<QVector<double>> twoSamples = {
        QVector<double>(59, 1.0),
        QVector<double>(59, 0.5)
    };
    const QVector<double> center = registeredClassificationClassCenter(twoSamples);
    check(center.size() == 59, "class center must have V2 length");
    const RegisteredClassificationRadiusStats disabledRadius =
            registeredClassificationRadiusStats(twoSamples, center);
    check(!disabledRadius.enabled, "radius must be disabled below three samples");

    const QString modelDir = smokeModelDir(QStringLiteral("schema_2"));
    const RegisteredClassificationKnnModelMetadata metadata = validMetadata();
    check(writeRegisteredClassificationKnnMetadata(modelDir, metadata).success,
          "schema 2 metadata must write");

    RegisteredClassificationKnnModelMetadata loadedMetadata;
    check(readRegisteredClassificationKnnMetadata(modelDir, &loadedMetadata).success,
          "schema 2 metadata must round-trip");
    check(loadedMetadata.featureNames == metadata.featureNames,
          "schema 2 feature names must round-trip");

    QFile metadataFile(registeredClassificationMetadataPath(modelDir));
    check(metadataFile.open(QIODevice::ReadOnly), "schema 2 metadata JSON must be readable");
    const QJsonObject metadataJson = metadataFile.isOpen()
            ? QJsonDocument::fromJson(metadataFile.readAll()).object()
            : QJsonObject();
    const QJsonObject knnJson = metadataJson.value(QStringLiteral("knn")).toObject();
    check(knnJson.value(QStringLiteral("method")).toString()
                  == QStringLiteral("classes_distance"),
          "schema 2 metadata must persist KNN method");
    check(knnJson.contains(QStringLiteral("normalization"))
                  && !knnJson.value(QStringLiteral("normalization")).toBool(true),
          "schema 2 metadata must persist disabled KNN normalization");

    RegisteredClassificationKnnModelMetadata badSchema = metadata;
    badSchema.schemaVersion = 1;
    check(!writeRegisteredClassificationKnnMetadata(
                  smokeModelDir(QStringLiteral("bad_schema")), badSchema).success,
          "schema 1 must be rejected by the schema 2 writer");

    RegisteredClassificationKnnModelMetadata badFeatureLength = metadata;
    --badFeatureLength.featureLength;
    const RegisteredClassificationModelPackageResult badFeatureResult =
            writeRegisteredClassificationKnnMetadata(
                    smokeModelDir(QStringLiteral("bad_feature_length")), badFeatureLength);
    check(!badFeatureResult.success, "schema 2 feature length mismatch must fail");
    check(badFeatureResult.status == QStringLiteral("feature_length_mismatch"),
          "schema 2 feature length mismatch must identify its error");

    const RegisteredClassificationClassStatsDocument stats = validClassStats();
    check(writeRegisteredClassificationClassStats(modelDir, stats).success,
          "class stats must write");
    RegisteredClassificationClassStatsDocument loadedStats;
    check(readRegisteredClassificationClassStats(modelDir, &loadedStats).success,
          "class stats must round-trip");
    check(loadedStats.classes.size() == 2, "class stats classes must round-trip");
    check(loadedStats.classes.value(0).radiusEnabled,
          "class stats radius enabled flag must round-trip");
    check(qAbs(loadedStats.classes.value(0).radius - 0.35) < 1e-9,
          "class stats radius must round-trip");

    const RegisteredClassificationModelPackageResult incompletePackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!incompletePackage.success,
          "schema 2 package without both KNN files must be incomplete");
    check(incompletePackage.status == QStringLiteral("model_package_incomplete"),
          "missing KNN files must report an incomplete package");

    check(createEmptyFile(registeredClassificationSampleKnnPath(modelDir)),
          "sample KNN file fixture must write");
    check(createEmptyFile(registeredClassificationCenterKnnPath(modelDir)),
          "center KNN file fixture must write");
    const RegisteredClassificationModelPackageResult zeroBytePackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!zeroBytePackage.success,
          "zero-byte KNN files must leave the schema 2 package incomplete");
    check(zeroBytePackage.status == QStringLiteral("model_package_incomplete"),
          "zero-byte KNN files must report an incomplete package");
    const RegisteredClassificationModelInspection zeroByteInspection =
            inspectRegisteredClassificationModelPackage(modelDir);
    check(!zeroByteInspection.runnable,
          "zero-byte KNN files must not be inspected as runnable");

    check(writeSentinelFile(registeredClassificationSampleKnnPath(modelDir)),
          "sample KNN sentinel fixture must write");
    check(writeSentinelFile(registeredClassificationCenterKnnPath(modelDir)),
          "center KNN sentinel fixture must write");
    check(validateRegisteredClassificationKnnPackage(modelDir).success,
          "complete schema 2 package must validate");

    QJsonObject alteredMethodMetadata = metadataJson;
    QJsonObject alteredMethodKnn = alteredMethodMetadata.value(QStringLiteral("knn")).toObject();
    alteredMethodKnn.insert(QStringLiteral("method"), QStringLiteral("nearest_neighbor"));
    alteredMethodMetadata.insert(QStringLiteral("knn"), alteredMethodKnn);
    check(writeMetadataJson(modelDir, alteredMethodMetadata),
          "altered KNN method fixture must write");
    const RegisteredClassificationModelPackageResult alteredMethodPackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!alteredMethodPackage.success,
          "altered KNN method must invalidate the package");
    check(alteredMethodPackage.status == QStringLiteral("invalid_knn_parameters"),
          "altered KNN method must report actionable parameter status");

    QJsonObject alteredNormalizationMetadata = metadataJson;
    QJsonObject alteredNormalizationKnn = alteredNormalizationMetadata.value(QStringLiteral("knn")).toObject();
    alteredNormalizationKnn.insert(QStringLiteral("normalization"), true);
    alteredNormalizationMetadata.insert(QStringLiteral("knn"), alteredNormalizationKnn);
    check(writeMetadataJson(modelDir, alteredNormalizationMetadata),
          "altered KNN normalization fixture must write");
    const RegisteredClassificationModelPackageResult alteredNormalizationPackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!alteredNormalizationPackage.success,
          "altered KNN normalization must invalidate the package");
    check(alteredNormalizationPackage.status == QStringLiteral("invalid_knn_parameters"),
          "altered KNN normalization must report actionable parameter status");

    const QString legacyDir = smokeModelDir(QStringLiteral("legacy"));
    RegisteredClassificationModelMetadata legacyMetadata;
    legacyMetadata.modelType = registeredClassificationMlpModelType();
    legacyMetadata.schemaVersion = 1;
    legacyMetadata.featureVersion = registeredClassificationFeatureVersionV1();
    legacyMetadata.featureNames = registeredClassificationFeatureNamesV1();
    legacyMetadata.featureLength = legacyMetadata.featureNames.size();
    legacyMetadata.classLabels = {{0, QStringLiteral("A")}, {1, QStringLiteral("B")}};
    legacyMetadata.trainingSampleCount = 4;
    check(writeRegisteredClassificationMetadata(legacyDir, legacyMetadata).success,
          "schema 1 metadata fixture must write");
    RegisteredClassificationKnnModelMetadata legacyAsKnn;
    const RegisteredClassificationModelPackageResult legacyRead =
            readRegisteredClassificationKnnMetadata(legacyDir, &legacyAsKnn);
    check(!legacyRead.success, "schema 1 metadata must not pass the V2 reader");
    check(legacyRead.status == QStringLiteral("legacy_model_requires_retraining"),
          "schema 1 V2 read must require retraining");
    const RegisteredClassificationModelInspection legacyInspection =
            inspectRegisteredClassificationModelPackage(legacyDir);
    check(legacyInspection.success, "legacy package inspection must parse metadata");
    check(legacyInspection.legacy, "schema 1 package must be marked legacy");
    check(!legacyInspection.runnable, "schema 1 package must not be runnable");
    check(legacyInspection.status == QStringLiteral("legacy_model_requires_retraining"),
          "schema 1 inspection must require retraining");
    check(legacyInspection.classCount == 2 && legacyInspection.trainingSampleCount == 4,
          "legacy inspection must preserve class and sample counts");

    if (g_failures != 0)
        return 1;

    std::cout << "registered_classification_feature_v2_smoke: feature contract and schema 2 package checks passed"
              << std::endl;
    return 0;
}
