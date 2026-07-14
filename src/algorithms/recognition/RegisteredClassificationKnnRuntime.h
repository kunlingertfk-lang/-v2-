#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONKNNRUNTIME_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONKNNRUNTIME_H

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct RegisteredClassificationKnnSample
{
    QVector<double> feature;
    int classId = -1;
};

struct RegisteredClassificationKnnDistance
{
    int classId = -1;
    double distance = 0.0;
};

struct RegisteredClassificationKnnRuntimeConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
};

struct RegisteredClassificationKnnBuildRequest
{
    QVector<RegisteredClassificationKnnSample> samples;
    int featureLength = 59;
    int numTrees = 4;
    int k = 1;
    int maxNumClasses = 1;
    QString outputPath;
};

struct RegisteredClassificationKnnRuntimeResult
{
    bool success = false;
    QString status;
    QString message;
    QVector<RegisteredClassificationKnnDistance> sampleDistances;
    QVector<RegisteredClassificationKnnDistance> centerDistances;
    QJsonObject payload;
};

class RegisteredClassificationKnnRuntime
{
public:
    RegisteredClassificationKnnRuntimeResult buildAndWrite(
            const RegisteredClassificationKnnRuntimeConfig &config,
            const RegisteredClassificationKnnBuildRequest &request) const;
    RegisteredClassificationKnnRuntimeResult classifyPair(
            const RegisteredClassificationKnnRuntimeConfig &config,
            const QString &sampleModelPath,
            const QString &centerModelPath,
            const QVector<double> &feature) const;
};

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONKNNRUNTIME_H
