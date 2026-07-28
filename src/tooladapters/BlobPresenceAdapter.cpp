#include "tooladapters/BlobPresenceAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/PositionCorrectionConsumer.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>
#include <QtGlobal>

#include <cmath>

namespace {

int intParam(const QJsonObject &params, const QString &key, const int defaultValue)
{
    const QJsonValue value = params.value(key);
    if (value.isDouble())
        return value.toInt(defaultValue);

    bool ok = false;
    const int parsed = value.toString().trimmed().toInt(&ok);
    return ok ? parsed : defaultValue;
}

QString stringParam(const QJsonObject &params,
                    const QString &key,
                    const QString &defaultValue = QString())
{
    const QString value = params.value(key).toString().trimmed();
    return value.isEmpty() ? defaultValue : value;
}

bool boolParam(const QJsonObject &params, const QString &key, const bool defaultValue)
{
    const QJsonValue value = params.value(key);
    if (value.isBool())
        return value.toBool(defaultValue);

    const QString text = value.toString().trimmed().toLower();
    if (text == QStringLiteral("true") || text == QStringLiteral("1") || text == QStringLiteral("yes"))
        return true;
    if (text == QStringLiteral("false") || text == QStringLiteral("0") || text == QStringLiteral("no"))
        return false;
    return defaultValue;
}

bool isFiniteRoi(const QRectF &rect)
{
    return std::isfinite(rect.x()) &&
           std::isfinite(rect.y()) &&
           std::isfinite(rect.width()) &&
           std::isfinite(rect.height()) &&
           rect.width() > 0.0 &&
           rect.height() > 0.0;
}

QRectF rectParam(const QJsonObject &params, const QString &key, const QRectF &defaultValue)
{
    const QJsonObject json = params.value(key).toObject();
    if (json.isEmpty())
        return defaultValue;

    return QRectF(json.value(QStringLiteral("x")).toDouble(defaultValue.x()),
                  json.value(QStringLiteral("y")).toDouble(defaultValue.y()),
                  json.value(QStringLiteral("width")).toDouble(defaultValue.width()),
                  json.value(QStringLiteral("height")).toDouble(defaultValue.height()));
}

QVector<QPointF> pointsParam(const QJsonObject &params, const QString &key)
{
    const QJsonArray array = params.value(key).toArray();
    QVector<QPointF> points;
    points.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject json = value.toObject();
        points.append(QPointF(json.value(QStringLiteral("x")).toDouble(),
                              json.value(QStringLiteral("y")).toDouble()));
    }
    return points;
}

BlobPresenceHalconConfig toHalconConfig(const ToolConfig &config)
{
    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;

    BlobPresenceHalconConfig halconConfig;
    const QString requestedHalconSoPath = stringParam(params, QStringLiteral("halconSoPath"));
    halconConfig.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                requestedHalconSoPath,
                &halconConfig.halconSoPathCandidates);
    if (isFiniteRoi(config.roiNormalized))
        halconConfig.roiNormalized = config.roiNormalized;
    halconConfig.detectRegionType = stringParam(params,
                                                QStringLiteral("detectRegionType"),
                                                halconConfig.detectRegionType);
    halconConfig.detectPolygonNormalized = pointsParam(params,
                                                       QStringLiteral("detectPolygonNormalized"));
    const QJsonObject circleJson = params.value(QStringLiteral("detectCircleNormalized")).toObject();
    const QJsonObject circleCenterJson = circleJson.value(QStringLiteral("center")).toObject();
    halconConfig.detectCircleCenterNormalized = QPointF(
                circleCenterJson.value(QStringLiteral("x")).toDouble(),
                circleCenterJson.value(QStringLiteral("y")).toDouble());
    halconConfig.detectCircleRadiusNormalized =
            circleJson.value(QStringLiteral("radius")).toDouble();
    halconConfig.detectCircleBoundingRectNormalized =
            rectParam(circleJson,
                      QStringLiteral("boundingRect"),
                      halconConfig.detectCircleBoundingRectNormalized);
    halconConfig.enablePositionCorrection = boolParam(params,
                                                      QStringLiteral("enablePositionCorrection"),
                                                      halconConfig.enablePositionCorrection);
    halconConfig.positionCorrectionSource = stringParam(params,
                                                        QStringLiteral("positionCorrectionSource"),
                                                        halconConfig.positionCorrectionSource);
    halconConfig.grayMin = qBound(0,
                                  intParam(params, QStringLiteral("grayMin"), halconConfig.grayMin),
                                  255);
    halconConfig.grayMax = qBound(0,
                                  intParam(params, QStringLiteral("grayMax"), halconConfig.grayMax),
                                  255);
    halconConfig.invertRange = boolParam(params,
                                         QStringLiteral("invertRange"),
                                         halconConfig.invertRange);
    halconConfig.areaMin = qMax(0,
                                intParam(params, QStringLiteral("areaMin"), halconConfig.areaMin));
    halconConfig.areaMax = qMax(0,
                                intParam(params, QStringLiteral("areaMax"), halconConfig.areaMax));
    halconConfig.maskOutputEnabled = boolParam(params,
                                               QStringLiteral("maskOutputEnabled"),
                                               halconConfig.maskOutputEnabled);
    halconConfig.judgeBasis = stringParam(params,
                                          QStringLiteral("judgeBasis"),
                                          stringParam(judgeRule,
                                                      QStringLiteral("mode"),
                                                      halconConfig.judgeBasis));
    halconConfig.existOk = boolParam(params,
                                     QStringLiteral("existOk"),
                                     boolParam(judgeRule,
                                               QStringLiteral("existOk"),
                                               halconConfig.existOk));
    halconConfig.timeoutMs = qMax(0,
                                  intParam(params, QStringLiteral("timeoutMs"), halconConfig.timeoutMs));
    return halconConfig;
}

ToolResult makeBlobPresenceError(const ToolConfig &config,
                                 const QString &status,
                                 const QString &message)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::BlobPresence;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    return result;
}

} // namespace

bool BlobPresenceAdapter::supports(ToolType type) const
{
    return type == ToolType::BlobPresence;
}

ToolResult BlobPresenceAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::BlobPresence) {
        return makeBlobPresenceError(config,
                                     QStringLiteral("invalid_tool_type"),
                                     QStringLiteral("BlobPresenceAdapter only supports ToolType::BlobPresence."));
    }

    BlobPresenceHalconConfig halconConfig = toHalconConfig(config);
    QString stableSourceId = config.params
            .value(QStringLiteral("positionCorrectionSourceId"))
            .toString().trimmed();
    if (stableSourceId.isEmpty())
        stableSourceId = halconConfig.positionCorrectionSource.trimmed();
    stableSourceId = PositionCorrection::normalizedSourceId(stableSourceId);

    PositionCorrectionConsumerOptions correctionOptions;
    correctionOptions.requested = halconConfig.enablePositionCorrection;
    correctionOptions.sourceId = stableSourceId;
    correctionOptions.showMatchContour = boolParam(
                config.params,
                QStringLiteral("showPositionCorrectionMatchContour"),
                true);
    const PositionCorrectionResolveResult correction =
            PositionCorrectionConsumer::resolve(request, correctionOptions);
    if (!correction.success) {
        return makeBlobPresenceError(config,
                                     correction.status,
                                     correction.message);
    }
    halconConfig.positionCorrection = correction.context;
    const BlobPresenceHalconResult runnerResult = m_runner.run(request.image, halconConfig);

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::BlobPresence;
    result.success = runnerResult.success;
    result.ok = runnerResult.ok;
    result.status = runnerResult.status;
    result.message = runnerResult.message;
    result.text = runnerResult.text;
    result.score = runnerResult.score;
    result.value = runnerResult.count;
    result.count = runnerResult.count;
    result.elapsedMs = runnerResult.elapsedMs;
    result.overlays = runnerResult.overlays;
    result.payload = runnerResult.payload;
    return result;
}
