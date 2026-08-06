#ifndef ALGORITHMS_LOCATION_TEMPLATELOCATIONHALCONRUNNER_H
#define ALGORITHMS_LOCATION_TEMPLATELOCATIONHALCONRUNNER_H

#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <opencv2/core.hpp>

struct TemplateLocationHalconConfig
{
    QString toolId;
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString modelCacheKey;
    QString templateRegionType = QStringLiteral("rectangle");
    QRectF templateRoiNormalized;
    QVector<QPointF> templatePolygonNormalized;
    QString templateMaskRegionType = QStringLiteral("none");
    QRectF templateMaskRoiNormalized;
    QVector<QPointF> templateMaskPolygonNormalized;
    QPointF templateMaskCircleCenterNormalized;
    double templateMaskCircleRadiusNormalized = 0.0;
    QString searchRegionType = QStringLiteral("full");
    QRectF searchRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> searchPolygonNormalized;
    QPointF searchCircleCenterNormalized;
    double searchCircleRadiusNormalized = 0.0;
    int minScore = 50;
    int angleStart = -45;
    int angleExtent = 90;
    int scaleMin = 100;
    int scaleMax = 100;
    QString polarity = QStringLiteral("use_polarity");
    QString contrastMode = QStringLiteral("auto");
    int contrast = 40;
    int minContrast = 10;
    int numLevels = 0;
    QString subPixel = QStringLiteral("least_squares");
    double greediness = 0.5;
    int timeoutMs = 2000;
    int maxMatches = 1;
    int minMatchCount = 1;
    int maxMatchCount = 1;
    double maxOverlap = 0.5;
    QString originMode = QStringLiteral("centroid");
    QPointF customOriginNormalized;
};

struct TemplateLocationHalconResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    double score = 0.0;
    int count = 0;
    qint64 elapsedMs = 0;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class TemplateLocationHalconRunner
{
public:
    static bool clearPersistentCache(const QString &modelCacheKey);
    TemplateLocationHalconResult run(const cv::Mat &image,
                                     const cv::Mat &referenceImage,
                                     const TemplateLocationHalconConfig &config);
};

#endif // ALGORITHMS_LOCATION_TEMPLATELOCATIONHALCONRUNNER_H
