#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGRUNNER_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGRUNNER_H

#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "algorithms/recognition/RegisteredClassificationTrainingSession.h"

#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <opencv2/core.hpp>

struct RegisteredClassificationTrainingSample
{
    cv::Mat image;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    int classId = -1;
};

struct RegisteredClassificationTrainingRequest
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString outputModelDir;
    QVector<RegisteredClassificationClassLabel> classLabels;
    QVector<RegisteredClassificationTrainingSample> samples;
    QJsonObject trainingSessionManifest;
    QVector<RegisteredClassificationTrainingSessionAsset> trainingSessionAssets;
    RegisteredClassificationMlpParams mlp;
    RegisteredClassificationThresholds thresholds;
};

struct RegisteredClassificationTrainingResult
{
    bool success = false;
    QString status;
    QString message;
    QString modelDir;
    int sampleCount = 0;
    QJsonObject payload;
};

class RegisteredClassificationTrainingRunner
{
public:
    RegisteredClassificationTrainingResult train(
            const RegisteredClassificationTrainingRequest &request) const;
};

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGRUNNER_H
