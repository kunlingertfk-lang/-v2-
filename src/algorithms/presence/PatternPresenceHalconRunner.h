#ifndef ALGORITHMS_PRESENCE_PATTERNPRESENCEHALCONRUNNER_H
#define ALGORITHMS_PRESENCE_PATTERNPRESENCEHALCONRUNNER_H

#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <opencv2/core.hpp>

struct PatternPresenceHalconConfig
{
    QString toolId;
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QRectF templateRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString templateSource = QStringLiteral("referenceImage");
    QString templateImagePath;
    QString modelPath;
    bool modelAutoCreate = true;
    QString modelCacheKey;
    QString templateShapeType = QStringLiteral("rectangle");
    QVector<QPointF> templatePolygonNormalized;
    QString templateSensitivityMode = QStringLiteral("auto");
    int templateSensitivity = 2;
    QString detectRegionType = QStringLiteral("rectangle");
    QVector<QPointF> detectPolygonNormalized;
    bool enablePositionCorrection = true;
    QString positionCorrectionSource;
    int minScore = 50;
    QString polarity = QStringLiteral("consider");
    int scaleMin = 100;
    int scaleMax = 100;
    int angleStart = -45;
    int angleExtent = 90;
    int timeoutMs = 2000;
    bool showContourPoints = false;
    bool debugPolygonLog = false;
    QString sortMode;
    QString judgeBasis = QStringLiteral("presence");
    bool existOk = true;
    int scoreThreshold = 50;
    int maxCount = 1;
    int expectedCount = 1;
};

struct PatternPresenceHalconResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    QString text;
    double score = 0.0;
    int count = 0;
    qint64 elapsedMs = 0;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class PatternPresenceHalconRunner
{
public:
    PatternPresenceHalconResult run(const cv::Mat &image,
                                    const cv::Mat &referenceImage,
                                    const PatternPresenceHalconConfig &config);
};

#endif // ALGORITHMS_PRESENCE_PATTERNPRESENCEHALCONRUNNER_H
