#ifndef ALGORITHMS_PRESENCE_CIRCLEPRESENCEHALCONRUNNER_H
#define ALGORITHMS_PRESENCE_CIRCLEPRESENCEHALCONRUNNER_H

#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <opencv2/core.hpp>

struct CirclePresenceHalconConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString detectRegionType = QStringLiteral("rect");
    QVector<QPointF> detectPolygonNormalized;
    QPointF detectCircleCenterNormalized;
    double detectCircleRadiusNormalized = 0.0;
    QRectF detectCircleBoundingRectNormalized;
    int sensitivity = 60;
    int roundness = 25;
    QString edgePolarity = QStringLiteral("any");
    QString edgeType = QStringLiteral("strongest");
    bool existOk = true;
    int timeoutMs = 1000;
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
};

struct CirclePresenceHalconResult
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

class CirclePresenceHalconRunner
{
public:
    CirclePresenceHalconResult run(const cv::Mat &image,
                                   const CirclePresenceHalconConfig &config);
};

#endif // ALGORITHMS_PRESENCE_CIRCLEPRESENCEHALCONRUNNER_H
