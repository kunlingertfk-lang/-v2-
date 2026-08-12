#ifndef ALGORITHMS_PRESENCE_CONTOURPRESENCEHALCONRUNNER_H
#define ALGORITHMS_PRESENCE_CONTOURPRESENCEHALCONRUNNER_H

#include "toolcore/PositionCorrectionConsumer.h"
#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <opencv2/core.hpp>

struct ContourPresenceHalconConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QRectF templateRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> templatePolygonNormalized;
    QString templateSource = QStringLiteral("referenceImage");
    QString detectRegionType = QStringLiteral("rect");
    QVector<QPointF> detectPolygonNormalized;
    QString templateShapeType = QStringLiteral("rect");
    bool enablePositionCorrection = true;
    QString positionCorrectionSource;
    PositionCorrectionContext positionCorrection;
    double minScore = 0.5;
    QString polarity;
    QString thresholdType;
    QString scaleMode = QStringLiteral("auto");
    int speedScale = 5;
    int featureScale = 1;
    QString thresholdMode = QStringLiteral("auto");
    int grayThreshold = 15;
    QString chainMode = QStringLiteral("auto");
    int minChainLength = 4;
    double scaleMin = 100.0;
    double scaleMax = 100.0;
    double angleStart = -45.0;
    double angleExtent = 90.0;
    int timeoutMs = 2000;
    bool showContourPoints = false;
    QString sortMode;
    QString judgeBasis = QStringLiteral("presence");
    bool existOk = true;
    double scoreThreshold = 0.5;
    bool referenceTest = false;
};

struct ContourPresenceHalconResult
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

class ContourPresenceHalconRunner
{
public:
    ContourPresenceHalconResult run(const cv::Mat &image,
                                    const cv::Mat &referenceImage,
                                    const ContourPresenceHalconConfig &config);
};

#endif // ALGORITHMS_PRESENCE_CONTOURPRESENCEHALCONRUNNER_H
