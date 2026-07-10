#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONMODELPACKAGE_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONMODELPACKAGE_H

#include "algorithms/recognition/RegisteredClassificationFeatureSpace.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct RegisteredClassificationClassLabel
{
    int id = -1;
    QString name;
};

struct RegisteredClassificationMlpParams
{
    int numHidden = 16;
    int maxIterations = 200;
    int randSeed = 42;
};

struct RegisteredClassificationThresholds
{
    int minScore = 80;
    int rejectScore = 60;
    int top2Gap = 0;
};

struct RegisteredClassificationModelMetadata
{
    QString modelType;
    int schemaVersion = 1;
    QString featureVersion;
    QString halconVersion;
    QVector<RegisteredClassificationClassLabel> classLabels;
    QStringList featureNames;
    int featureLength = 0;
    QJsonObject preprocess;
    RegisteredClassificationMlpParams mlp;
    RegisteredClassificationThresholds thresholds;
    int trainingSampleCount = 0;
};

struct RegisteredClassificationModelPackageResult
{
    bool success = false;
    QString status;
    QString message;
    QJsonObject payload;
};

struct RegisteredClassificationKnnParams
{
    QString method = QStringLiteral("classes_distance");
    bool normalization = false;
    int numTrees = 4;
    int numChecks = 0;
    double epsilon = 0.0;
    double sampleWeight = 0.70;
    double centerWeight = 0.30;
};

struct RegisteredClassificationKnnThresholds
{
    int minSimilarity = 80;
    int minMargin = 8;
};

struct RegisteredClassificationClassStats
{
    int classId = -1;
    int sampleCount = 0;
    bool radiusEnabled = false;
    double radius = 0.0;
    double meanDistance = 0.0;
    double stdDevDistance = 0.0;
    double maxDistance = 0.0;
};

QJsonObject registeredClassificationSegmentationContractV2();
QJsonObject registeredClassificationCanonicalizationContractV2();
QJsonObject registeredClassificationFeatureGroupsContractV2();

struct RegisteredClassificationKnnModelMetadata
{
    QString modelType;
    int schemaVersion = 2;
    QString featureVersion;
    QString halconVersion;
    QVector<RegisteredClassificationClassLabel> classLabels;
    QStringList featureNames;
    int featureLength = 0;
    QJsonObject segmentation = registeredClassificationSegmentationContractV2();
    QJsonObject canonicalization = registeredClassificationCanonicalizationContractV2();
    QJsonObject featureGroups = registeredClassificationFeatureGroupsContractV2();
    RegisteredClassificationKnnParams knn;
    RegisteredClassificationKnnThresholds thresholds;
    int trainingSampleCount = 0;
};

struct RegisteredClassificationClassStatsDocument
{
    int schemaVersion = 2;
    QString featureVersion;
    QVector<RegisteredClassificationClassStats> classes;
};

struct RegisteredClassificationModelInspection
{
    bool success = false;
    bool runnable = false;
    bool legacy = false;
    bool hasTrainingSession = false;
    QString status;
    QString message;
    QString modelType;
    QString featureVersion;
    int classCount = 0;
    int trainingSampleCount = 0;
    RegisteredClassificationKnnModelMetadata metadata;
};

QString registeredClassificationMlpModelType();
QString registeredClassificationKnnModelType();
QString registeredClassificationLegacyMlpModelType();
QString registeredClassificationFeatureVersionV1();
QStringList registeredClassificationFeatureNamesV1();
QString registeredClassificationMetadataPath(const QString &modelDir);
QString registeredClassificationMlpPath(const QString &modelDir);
QString registeredClassificationTrainingReportPath(const QString &modelDir);
QString registeredClassificationSampleKnnPath(const QString &modelDir);
QString registeredClassificationCenterKnnPath(const QString &modelDir);
QString registeredClassificationClassStatsPath(const QString &modelDir);

RegisteredClassificationModelPackageResult validateRegisteredClassificationMetadata(
        const RegisteredClassificationModelMetadata &metadata);
RegisteredClassificationModelPackageResult writeRegisteredClassificationMetadata(
        const QString &modelDir,
        const RegisteredClassificationModelMetadata &metadata);
RegisteredClassificationModelPackageResult readRegisteredClassificationMetadata(
        const QString &modelDir,
        RegisteredClassificationModelMetadata *metadata);
RegisteredClassificationModelPackageResult validateRegisteredClassificationKnnMetadata(
        const RegisteredClassificationKnnModelMetadata &metadata);
RegisteredClassificationModelPackageResult writeRegisteredClassificationKnnMetadata(
        const QString &modelDir,
        const RegisteredClassificationKnnModelMetadata &metadata);
RegisteredClassificationModelPackageResult readRegisteredClassificationKnnMetadata(
        const QString &modelDir,
        RegisteredClassificationKnnModelMetadata *metadata);
RegisteredClassificationModelPackageResult writeRegisteredClassificationClassStats(
        const QString &modelDir,
        const RegisteredClassificationClassStatsDocument &document);
RegisteredClassificationModelPackageResult readRegisteredClassificationClassStats(
        const QString &modelDir,
        RegisteredClassificationClassStatsDocument *document);
RegisteredClassificationModelPackageResult validateRegisteredClassificationKnnPackage(
        const QString &modelDir);
RegisteredClassificationModelInspection inspectRegisteredClassificationModelPackage(
        const QString &modelDir);

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONMODELPACKAGE_H
