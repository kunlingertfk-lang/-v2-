#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QtMath>

#include <iostream>
#include <limits>

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

bool writeClassStatsJson(const QString &modelDir, const QJsonObject &classStats)
{
    QFile file(registeredClassificationClassStatsPath(modelDir));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(QJsonDocument(classStats).toJson(QJsonDocument::Compact)) > 0;
}

QJsonObject classStatsWithFirstClassField(const QJsonObject &classStats,
                                          const QString &field,
                                          const QJsonValue &value)
{
    QJsonObject mutated = classStats;
    QJsonArray classes = mutated.value(QStringLiteral("classes")).toArray();
    QJsonObject firstClass = classes.at(0).toObject();
    firstClass.insert(field, value);
    classes.replace(0, firstClass);
    mutated.insert(QStringLiteral("classes"), classes);
    return mutated;
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

    RegisteredClassificationClassStatsDocument nanClassStats = stats;
    nanClassStats.classes[0].meanDistance = qQNaN();
    const RegisteredClassificationModelPackageResult nanClassStatsResult =
            writeRegisteredClassificationClassStats(
                    smokeModelDir(QStringLiteral("nan_class_stats")), nanClassStats);
    check(!nanClassStatsResult.success,
          "non-finite class stats values must be rejected by the validator");
    check(nanClassStatsResult.status == QStringLiteral("invalid_class_stats"),
          "non-finite class stats values must report invalid class stats");

    RegisteredClassificationClassStatsDocument infiniteClassStats = stats;
    infiniteClassStats.classes[0].maxDistance = std::numeric_limits<double>::infinity();
    const RegisteredClassificationModelPackageResult infiniteClassStatsResult =
            writeRegisteredClassificationClassStats(
                    smokeModelDir(QStringLiteral("infinite_class_stats")), infiniteClassStats);
    check(!infiniteClassStatsResult.success,
          "infinite class stats values must be rejected by the validator");
    check(infiniteClassStatsResult.status == QStringLiteral("invalid_class_stats"),
          "infinite class stats values must report invalid class stats");

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

    QFile classStatsFile(registeredClassificationClassStatsPath(modelDir));
    check(classStatsFile.open(QIODevice::ReadOnly), "class stats JSON must be readable");
    const QJsonObject classStatsJson = classStatsFile.isOpen()
            ? QJsonDocument::fromJson(classStatsFile.readAll()).object()
            : QJsonObject();

    const auto checkInvalidClassStats = [&](const QJsonObject &mutatedClassStats,
                                            const char *message) {
        check(writeClassStatsJson(modelDir, mutatedClassStats),
              "mutated class stats fixture must write");
        const RegisteredClassificationModelPackageResult packageResult =
                validateRegisteredClassificationKnnPackage(modelDir);
        check(!packageResult.success, message);
        check(packageResult.status == QStringLiteral("invalid_class_stats"),
              "malformed class stats must report actionable contract status");
    };
    const auto checkMissingClassStatsField = [&](const QString &field) {
        QJsonObject missingFieldClassStats = classStatsJson;
        missingFieldClassStats.remove(field);
        checkInvalidClassStats(missingFieldClassStats,
                               "missing top-level class stats field must invalidate the package");
    };
    checkMissingClassStatsField(QStringLiteral("schemaVersion"));
    checkMissingClassStatsField(QStringLiteral("featureVersion"));
    checkMissingClassStatsField(QStringLiteral("classes"));

    QJsonObject wrongSchemaType = classStatsJson;
    wrongSchemaType.insert(QStringLiteral("schemaVersion"), QStringLiteral("2"));
    checkInvalidClassStats(wrongSchemaType,
                           "string class stats schema version must invalidate the package");
    QJsonObject wrongFeatureType = classStatsJson;
    wrongFeatureType.insert(QStringLiteral("featureVersion"), 2);
    checkInvalidClassStats(wrongFeatureType,
                           "numeric class stats feature version must invalidate the package");
    QJsonObject wrongClassesType = classStatsJson;
    wrongClassesType.insert(QStringLiteral("classes"), QJsonObject());
    checkInvalidClassStats(wrongClassesType,
                           "object class stats classes field must invalidate the package");

    const auto checkMissingPerClassField = [&](const QString &field) {
        QJsonObject missingFieldClassStats = classStatsJson;
        QJsonArray classes = missingFieldClassStats.value(QStringLiteral("classes")).toArray();
        QJsonObject firstClass = classes.at(0).toObject();
        firstClass.remove(field);
        classes.replace(0, firstClass);
        missingFieldClassStats.insert(QStringLiteral("classes"), classes);
        checkInvalidClassStats(missingFieldClassStats,
                               "missing per-class stats field must invalidate the package");
    };
    checkMissingPerClassField(QStringLiteral("classId"));
    checkMissingPerClassField(QStringLiteral("sampleCount"));
    checkMissingPerClassField(QStringLiteral("radiusEnabled"));
    checkMissingPerClassField(QStringLiteral("radius"));
    checkMissingPerClassField(QStringLiteral("meanDistance"));
    checkMissingPerClassField(QStringLiteral("stdDevDistance"));
    checkMissingPerClassField(QStringLiteral("maxDistance"));

    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("classId"), QStringLiteral("0")),
                           "string class id must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("sampleCount"), true),
                           "boolean sample count must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("radiusEnabled"), QStringLiteral("true")),
                           "string radius enabled flag must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("radius"), false),
                           "boolean radius must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("meanDistance"), QStringLiteral("0.2")),
                           "string mean distance must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("stdDevDistance"), QJsonObject()),
                           "object standard deviation must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("maxDistance"), QJsonArray()),
                           "array max distance must invalidate the package");

    QJsonObject nonObjectClass = classStatsJson;
    QJsonArray nonObjectClasses = nonObjectClass.value(QStringLiteral("classes")).toArray();
    nonObjectClasses.replace(0, QStringLiteral("not an object"));
    nonObjectClass.insert(QStringLiteral("classes"), nonObjectClasses);
    checkInvalidClassStats(nonObjectClass,
                           "non-object class stats entry must invalidate the package");

    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("classId"), 0.5),
                           "non-integral class id must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("sampleCount"), 3.5),
                           "non-integral sample count must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("sampleCount"), 0),
                           "zero sample count must invalidate the package");
    const QStringList distanceFields = {
        QStringLiteral("radius"),
        QStringLiteral("meanDistance"),
        QStringLiteral("stdDevDistance"),
        QStringLiteral("maxDistance")
    };
    for (const QString &field : distanceFields) {
        checkInvalidClassStats(classStatsWithFirstClassField(classStatsJson, field, -0.01),
                               "negative class stats distance must invalidate the package");
    }
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("radius"), 0.09),
                           "enabled radius below minimum must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("radius"), 2.01),
                           "enabled radius above maximum must invalidate the package");

    QJsonObject duplicateClassIds = classStatsJson;
    QJsonArray duplicateClasses = duplicateClassIds.value(QStringLiteral("classes")).toArray();
    QJsonObject secondClass = duplicateClasses.at(1).toObject();
    secondClass.insert(QStringLiteral("classId"), 0);
    duplicateClasses.replace(1, secondClass);
    duplicateClassIds.insert(QStringLiteral("classes"), duplicateClasses);
    checkInvalidClassStats(duplicateClassIds,
                           "duplicate class ids must invalidate the package");

    QJsonObject unsupportedSchema = classStatsJson;
    unsupportedSchema.insert(QStringLiteral("schemaVersion"), 3);
    check(writeClassStatsJson(modelDir, unsupportedSchema),
          "unsupported class stats schema fixture must write");
    const RegisteredClassificationModelPackageResult unsupportedSchemaResult =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!unsupportedSchemaResult.success,
          "unsupported class stats schema must invalidate the package");
    check(unsupportedSchemaResult.status == QStringLiteral("unsupported_schema_version"),
          "unsupported class stats schema must retain its existing status");

    QJsonObject unsupportedFeature = classStatsJson;
    unsupportedFeature.insert(QStringLiteral("featureVersion"), QStringLiteral("other_feature"));
    check(writeClassStatsJson(modelDir, unsupportedFeature),
          "unsupported class stats feature fixture must write");
    const RegisteredClassificationModelPackageResult unsupportedFeatureResult =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!unsupportedFeatureResult.success,
          "unsupported class stats feature must invalidate the package");
    check(unsupportedFeatureResult.status == QStringLiteral("unsupported_feature_version"),
          "unsupported class stats feature must retain its existing status");

    check(writeClassStatsJson(modelDir, classStatsJson),
          "valid class stats fixture must restore after mutation checks");

    const auto checkMissingFixedContractField = [&](const QString &section,
                                                    const QString &field) {
        QJsonObject missingFieldMetadata = metadataJson;
        QJsonObject sectionJson = missingFieldMetadata.value(section).toObject();
        sectionJson.remove(field);
        missingFieldMetadata.insert(section, sectionJson);
        check(writeMetadataJson(modelDir, missingFieldMetadata),
              "missing fixed contract field fixture must write");
        const RegisteredClassificationModelPackageResult missingFieldPackage =
                validateRegisteredClassificationKnnPackage(modelDir);
        check(!missingFieldPackage.success,
              "missing fixed KNN/fusion/rejection field must invalidate the package");
        check(missingFieldPackage.status == QStringLiteral("invalid_knn_parameters"),
              "missing fixed contract field must report actionable parameter status");
    };
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("method"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("normalization"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("numTrees"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("numChecks"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("epsilon"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("sampleWeight"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("centerWeight"));
    checkMissingFixedContractField(QStringLiteral("thresholds"), QStringLiteral("minSimilarity"));
    checkMissingFixedContractField(QStringLiteral("thresholds"), QStringLiteral("minMargin"));

    QJsonObject malformedNumberMetadata = metadataJson;
    QJsonObject malformedNumberKnn = malformedNumberMetadata.value(QStringLiteral("knn")).toObject();
    malformedNumberKnn.insert(QStringLiteral("numTrees"), QStringLiteral("4"));
    malformedNumberMetadata.insert(QStringLiteral("knn"), malformedNumberKnn);
    check(writeMetadataJson(modelDir, malformedNumberMetadata),
          "malformed KNN number fixture must write");
    const RegisteredClassificationModelPackageResult malformedNumberPackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!malformedNumberPackage.success,
          "string KNN number must invalidate the package");
    check(malformedNumberPackage.status == QStringLiteral("invalid_knn_parameters"),
          "string KNN number must report actionable parameter status");

    QJsonObject malformedBoolMetadata = metadataJson;
    QJsonObject malformedBoolKnn = malformedBoolMetadata.value(QStringLiteral("knn")).toObject();
    malformedBoolKnn.insert(QStringLiteral("normalization"), QStringLiteral("false"));
    malformedBoolMetadata.insert(QStringLiteral("knn"), malformedBoolKnn);
    check(writeMetadataJson(modelDir, malformedBoolMetadata),
          "malformed KNN boolean fixture must write");
    const RegisteredClassificationModelPackageResult malformedBoolPackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!malformedBoolPackage.success,
          "string KNN boolean must invalidate the package");
    check(malformedBoolPackage.status == QStringLiteral("invalid_knn_parameters"),
          "string KNN boolean must report actionable parameter status");

    QJsonObject malformedThresholdMetadata = metadataJson;
    QJsonObject malformedThresholds = malformedThresholdMetadata.value(
            QStringLiteral("thresholds")).toObject();
    malformedThresholds.insert(QStringLiteral("minSimilarity"), QStringLiteral("80"));
    malformedThresholdMetadata.insert(QStringLiteral("thresholds"), malformedThresholds);
    check(writeMetadataJson(modelDir, malformedThresholdMetadata),
          "malformed threshold number fixture must write");
    const RegisteredClassificationModelPackageResult malformedThresholdPackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!malformedThresholdPackage.success,
          "string rejection threshold must invalidate the package");
    check(malformedThresholdPackage.status == QStringLiteral("invalid_knn_parameters"),
          "string rejection threshold must report actionable parameter status");

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
