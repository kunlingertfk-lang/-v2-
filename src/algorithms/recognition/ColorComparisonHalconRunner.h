#ifndef ALGORITHMS_RECOGNITION_COLORCOMPARISONHALCONRUNNER_H
#define ALGORITHMS_RECOGNITION_COLORCOMPARISONHALCONRUNNER_H

#include "algorithms/recognition/ColorComparisonModel.h"
#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <opencv2/core.hpp>

struct ColorComparisonHalconConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString templateRegionMode = QStringLiteral("custom");
    QRectF templateRoiNormalized = QRectF(0.0, 0.0, 0.5, 0.5);
    QVector<QPointF> templateMaskPolygonNormalized;
    QRectF detectRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString detectRegionType = QStringLiteral("rectangle");
    QPointF detectCircleCenterNormalized;
    double detectCircleRadiusNormalized = 0.0;
    QVector<QPointF> detectMaskPolygonNormalized;
    ColorComparisonModelV2 model;
    ColorComparisonInputSignature inputSignature;
    QString sensitivity = QStringLiteral("medium");
    bool brightnessCompensation = false;
    bool positionCorrectionRequested = false;
    QString positionCorrectionSourceId;
    int minScore = 80;
};

struct ColorComparisonTemplateBuildResult
{
    bool success = false;
    QString status;
    QString message;
    ColorComparisonModelV2 model;
    QJsonObject payload;
};

struct ColorComparisonHalconResult
{
    bool success = false;
    bool ok = false;
    bool measurementValid = false;
    QString status;
    QString message;
    double score = 0.0;
    double similarity = 0.0;
    qint64 elapsedMs = 0;
    QVector<double> detectFeature;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class ColorComparisonHalconRunner
{
public:
    ColorComparisonTemplateBuildResult buildTemplateModel(
            const cv::Mat &referenceImage,
            const ColorComparisonHalconConfig &config) const;
    ColorComparisonHalconResult run(
            const cv::Mat &image,
            const ColorComparisonHalconConfig &config) const;
};

#endif // ALGORITHMS_RECOGNITION_COLORCOMPARISONHALCONRUNNER_H
