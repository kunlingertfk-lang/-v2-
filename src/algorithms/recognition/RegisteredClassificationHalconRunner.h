#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONHALCONRUNNER_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONHALCONRUNNER_H

#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

struct RegisteredClassificationHalconConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString modelPath;
    QString modelName;
    QString modelType = QStringLiteral("halcon_knn_registered_classification");
    QString detectRegionType = QStringLiteral("full");
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    PositionCorrectionConfig positionCorrection;
    int topK = 1;
    QString judgeMode = QStringLiteral("class_match");
    QString expectedLabel;
    int minScore = 80;
    int minSimilarity = 80;
    int minMargin = 8;
};

struct RegisteredClassificationClassScore
{
    QString label;
    int classId = -1;
    double score = 0.0;
    double sampleSimilarity = 0.0;
    double centerSimilarity = 0.0;
    double centerDistance = 0.0;
};

struct RegisteredClassificationHalconResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    QString predictedLabel;
    int predictedClassId = -1;
    double score = 0.0;
    double sampleSimilarity = 0.0;
    double centerSimilarity = 0.0;
    double centerDistance = 0.0;
    double secondScore = 0.0;
    double scoreMargin = 0.0;
    double classRadius = 0.0;
    bool radiusEnabled = false;
    bool rejected = false;
    QString rejectionReason;
    QVector<RegisteredClassificationClassScore> topClasses;
    qint64 elapsedMs = 0;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class RegisteredClassificationHalconRunner
{
public:
    RegisteredClassificationHalconResult run(
            const cv::Mat &image,
            const RegisteredClassificationHalconConfig &config) const;
};

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONHALCONRUNNER_H
