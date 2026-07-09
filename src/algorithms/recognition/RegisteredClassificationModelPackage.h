#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONMODELPACKAGE_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONMODELPACKAGE_H

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

QString registeredClassificationMlpModelType();
QString registeredClassificationFeatureVersionV1();
QStringList registeredClassificationFeatureNamesV1();
QString registeredClassificationMetadataPath(const QString &modelDir);
QString registeredClassificationMlpPath(const QString &modelDir);
QString registeredClassificationTrainingReportPath(const QString &modelDir);

RegisteredClassificationModelPackageResult validateRegisteredClassificationMetadata(
        const RegisteredClassificationModelMetadata &metadata);
RegisteredClassificationModelPackageResult writeRegisteredClassificationMetadata(
        const QString &modelDir,
        const RegisteredClassificationModelMetadata &metadata);
RegisteredClassificationModelPackageResult readRegisteredClassificationMetadata(
        const QString &modelDir,
        RegisteredClassificationModelMetadata *metadata);

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONMODELPACKAGE_H
