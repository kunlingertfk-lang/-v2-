#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGRUNNER_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGRUNNER_H

#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"
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
    RegisteredClassificationFeatureRegion region;
    // Retained only to keep pre-Task-3 callers source compatible; training converts it to V2 region.
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    int classId = -1;
};

struct RegisteredClassificationTrainingKnnThresholds : RegisteredClassificationKnnThresholds
{
    // Existing UI callers still populate these values. KNN training deliberately ignores them.
    int minScore = 80;
    int rejectScore = 60;
    int top2Gap = 0;
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
    RegisteredClassificationKnnParams knn;
    RegisteredClassificationTrainingKnnThresholds thresholds;
    // Legacy caller fields are ignored. They avoid an unrelated UI/adapter source edit in this task.
    RegisteredClassificationMlpParams mlp;
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
