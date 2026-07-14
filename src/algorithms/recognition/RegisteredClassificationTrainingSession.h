#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGSESSION_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGSESSION_H

#include <QJsonObject>
#include <QString>
#include <QVector>

#include <opencv2/core.hpp>

struct RegisteredClassificationTrainingSessionAsset
{
    QString relativePath;
    cv::Mat image;
};

struct RegisteredClassificationTrainingSessionPayload
{
    int schemaVersion = 0;
    QJsonObject manifest;
    QVector<RegisteredClassificationTrainingSessionAsset> assets;
};

struct RegisteredClassificationTrainingSessionResult
{
    bool success = false;
    QString status;
    QString message;
    QJsonObject payload;
};

RegisteredClassificationTrainingSessionResult writeRegisteredClassificationTrainingSession(
        const QString &sessionRoot,
        const QJsonObject &manifest,
        const QVector<RegisteredClassificationTrainingSessionAsset> &assets);

RegisteredClassificationTrainingSessionResult readRegisteredClassificationTrainingSession(
        const QString &sessionRoot,
        RegisteredClassificationTrainingSessionPayload *payload);

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGSESSION_H
