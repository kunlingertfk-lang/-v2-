#ifndef ALGORITHMS_RECOGNITION_COLORRECOGNITIONHALCONRUNNER_H
#define ALGORITHMS_RECOGNITION_COLORRECOGNITIONHALCONRUNNER_H

#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <opencv2/core.hpp>

//roi标签及id
struct ColorRecognitionHalconLabel
{
    QString name;
    int classId = 0;
};

//模板roi
struct ColorRecognitionHalconSample
{
    QString label;
    int classId = 0;
    QVector<double> feature;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
};

//
struct ColorRecognitionHalconConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString detectRegionType = QStringLiteral("rectangle");
    QPointF detectCircleCenterNormalized;
    double detectCircleRadiusNormalized = 0.0;
    QRectF detectCircleBoundingRectNormalized;
    QString colorDecisionMode = QStringLiteral("dominant_ratio");
    QString featureType = QStringLiteral("histogram");
    QString sensitivity = QStringLiteral("medium");
    bool brightnessEnabled = true;
    int knnK = 3;
    QString knnDistance = QStringLiteral("halcon_default");
    QVector<QPointF> detectMaskPolygonNormalized;
    QVector<ColorRecognitionHalconLabel> labels;
    QVector<ColorRecognitionHalconSample> samples;
    QString judgeMode = QStringLiteral("min_score");
    int minScore = 80;
    QString expectedLabel;
};

struct ColorRecognitionHalconFeatureResult
{
    bool success = false;
    QString status;
    QString message;
    QVector<double> feature;
    qint64 elapsedMs = 0;
    QJsonObject payload;
};

struct ColorRecognitionHalconResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    QString predictedLabel;
    int predictedClassId = -1;
    double score = 0.0;
    double rating = 0.0;
    int sampleCount = 0;
    qint64 elapsedMs = 0;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class ColorRecognitionHalconRunner
{
public:
    ColorRecognitionHalconFeatureResult extractFeature(
            const cv::Mat &image,
            const ColorRecognitionHalconConfig &config) const;
    ColorRecognitionHalconResult run(
            const cv::Mat &image,
            const ColorRecognitionHalconConfig &config) const;
};

#endif // ALGORITHMS_RECOGNITION_COLORRECOGNITIONHALCONRUNNER_H
