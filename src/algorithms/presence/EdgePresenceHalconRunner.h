#ifndef ALGORITHMS_PRESENCE_EDGEPRESENCEHALCONRUNNER_H
#define ALGORITHMS_PRESENCE_EDGEPRESENCEHALCONRUNNER_H

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

struct EdgePresenceHalconConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString detectRegionType = QStringLiteral("line_band");
    QPointF searchLineP1 = QPointF(0.15, 0.5);
    QPointF searchLineP2 = QPointF(0.85, 0.5);
    double searchBandWidth = 0.08;
    int sensitivity = 60;
    QString edgePolarity = QStringLiteral("any");
    bool existOk = true;
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
    PositionCorrectionContext positionCorrection;
    int timeoutMsInternalDefault = 1000;
};

struct EdgePresenceHalconResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    QString text;
    double score = 0.0;
    double value = 0.0;
    int count = 0;
    qint64 elapsedMs = 0;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class EdgePresenceHalconRunner
{
public:
    EdgePresenceHalconResult run(const cv::Mat &image,
                                 const EdgePresenceHalconConfig &config);
};

#endif // ALGORITHMS_PRESENCE_EDGEPRESENCEHALCONRUNNER_H
