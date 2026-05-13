#ifndef ALGORITHMS_PRESENCE_BLOBPRESENCEHALCONRUNNER_H
#define ALGORITHMS_PRESENCE_BLOBPRESENCEHALCONRUNNER_H

#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <opencv2/core.hpp>

struct BlobPresenceHalconConfig
{
    QString halconSoPath = QStringLiteral("/home/hjl-ubuntu/MVTec/HALCON-24.11-Progress-Steady/lib/x64-linux/libhalconc.so.24.11.2");
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString detectRegionType = QStringLiteral("rect");
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
    int grayMin = 0;
    int grayMax = 255;
    bool invertRange = false;
    int areaMin = 10;
    int areaMax = 999999;
    bool maskOutputEnabled = false;
    QString judgeBasis = QStringLiteral("presence");
    bool existOk = true;
    int timeoutMs = 1000;
};

struct BlobPresenceHalconResult
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

class BlobPresenceHalconRunner
{
public:
    BlobPresenceHalconResult run(const cv::Mat &image,
                                 const BlobPresenceHalconConfig &config);
};

#endif // ALGORITHMS_PRESENCE_BLOBPRESENCEHALCONRUNNER_H
