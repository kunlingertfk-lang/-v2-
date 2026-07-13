#ifndef ALGORITHMS_RECOGNITION_COLORCOMPARISONHALCONRUNNER_H
#define ALGORITHMS_RECOGNITION_COLORCOMPARISONHALCONRUNNER_H

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
    QRectF templateRoiNormalized = QRectF(0.0, 0.0, 0.5, 0.5);
    QVector<QPointF> templateMaskPolygonNormalized;
    QVector<double> templateFeature;
    QRectF detectRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString detectRegionType = QStringLiteral("rectangle");
    QPointF detectCircleCenterNormalized;
    double detectCircleRadiusNormalized = 0.0;
    QRectF detectCircleBoundingRectNormalized;
    QVector<QPointF> detectMaskPolygonNormalized;
    QString comparisonMode = QStringLiteral("dominant_hue_coverage");
    QString featureType = QStringLiteral("histogram");
    QString sensitivity = QStringLiteral("medium");
    bool brightnessEnabled = false;
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
    int minScore = 52;
};

struct ColorComparisonHalconResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    double score = 0.0;
    double similarity = 0.0;
    qint64 elapsedMs = 0;
    QVector<double> templateFeature;
    QVector<double> detectFeature;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

struct ColorComparisonHsvSimilarity
{
    double hue = 0.0;
    double saturation = 0.0;
    double value = 0.0;
    double combined = 0.0;
    int bins = 0;
    bool brightnessUsed = false;
};

struct ColorComparisonHs2dCoverage
{
    double coverage = 0.0;
    double normalizedCoverage = 0.0;
    double rawCoverage = 0.0;
    double templateSelfCoverage = 0.0;
    int bins = 0;
    int templateHuePeakBin = -1;
    int templateSaturationPeakBin = -1;
    double hueSigma = 0.0;
    double saturationSigma = 0.0;
};

ColorComparisonHsvSimilarity compareColorComparisonHsvHistograms(
        const QVector<double> &templateFeature,
        const QVector<double> &detectFeature,
        int bins,
        bool brightnessEnabled);

ColorComparisonHs2dCoverage compareColorComparisonHs2dTemplateCoverage(
        const QVector<double> &templateFeature,
        const QVector<double> &detectFeature,
        int bins,
        const QString &sensitivity);

class ColorComparisonHalconRunner
{
public:
    ColorComparisonHalconResult run(const cv::Mat &image,
                                    const ColorComparisonHalconConfig &config) const;
};

#endif // ALGORITHMS_RECOGNITION_COLORCOMPARISONHALCONRUNNER_H
