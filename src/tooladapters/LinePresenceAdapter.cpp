#include "tooladapters/LinePresenceAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"

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

double doubleParam(const QJsonObject &params, const QString &key, const double defaultValue)
{
    const QJsonValue value = params.value(key);
    if (value.isDouble())
        return value.toDouble(defaultValue);

    bool ok = false;
    const double parsed = value.toString().trimmed().toDouble(&ok);
    return ok ? parsed : defaultValue;
}

QString stringParam(const QJsonObject &params,
                    const QString &key,
                    const QString &defaultValue = QString())
{
    const QString value = params.value(key).toString().trimmed();
    return value.isEmpty() ? defaultValue : value;
}

QPointF pointParam(const QJsonObject &params, const QString &key, const QPointF &defaultValue)
{
    const QJsonObject json = params.value(key).toObject();
    if (json.isEmpty())
        return defaultValue;

    return QPointF(json.value(QStringLiteral("x")).toDouble(defaultValue.x()),
                   json.value(QStringLiteral("y")).toDouble(defaultValue.y()));
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

QString normalizedEdgePolarity(const QString &polarity)
{
    const QString key = polarity.trimmed().toLower();
    if (key == QStringLiteral("black_to_white") || key.contains(QStringLiteral("黑到白")))
        return QStringLiteral("black_to_white");
    if (key == QStringLiteral("white_to_black") || key.contains(QStringLiteral("白到黑")))
        return QStringLiteral("white_to_black");
    return QStringLiteral("any");
}

QString normalizedEdgeType(const QString &edgeType)
{
    const QString key = edgeType.trimmed().toLower();
    if (key == QStringLiteral("first") || key.contains(QStringLiteral("第一")))
        return QStringLiteral("first");
    if (key == QStringLiteral("last") || key.contains(QStringLiteral("最后")))
        return QStringLiteral("last");
    if (key == QStringLiteral("manual") || key.contains(QStringLiteral("手动")))
        return QStringLiteral("manual");
    return QStringLiteral("strongest");
}

LinePresenceHalconConfig toHalconConfig(const ToolConfig &config)
{
    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;

    LinePresenceHalconConfig halconConfig;
    halconConfig.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(),
                &halconConfig.halconSoPathCandidates);
    if (isFiniteRoi(config.roiNormalized))
        halconConfig.roiNormalized = config.roiNormalized;
    halconConfig.detectRegionType = stringParam(params,
                                                QStringLiteral("detectRegionType"),
                                                halconConfig.detectRegionType);
    halconConfig.searchLineP1 = pointParam(params,
                                           QStringLiteral("searchLineP1"),
                                           halconConfig.searchLineP1);
    halconConfig.searchLineP2 = pointParam(params,
                                           QStringLiteral("searchLineP2"),
                                           halconConfig.searchLineP2);
    halconConfig.searchBandWidth = qBound(0.001,
                                          doubleParam(params,
                                                      QStringLiteral("searchBandWidth"),
                                                      halconConfig.searchBandWidth),
                                          1.0);
    halconConfig.enablePositionCorrection = boolParam(params,
                                                      QStringLiteral("enablePositionCorrection"),
                                                      halconConfig.enablePositionCorrection);
    halconConfig.positionCorrectionSource = stringParam(params,
                                                        QStringLiteral("positionCorrectionSource"),
                                                        halconConfig.positionCorrectionSource);
    halconConfig.sensitivity = qBound(0,
                                      intParam(params, QStringLiteral("sensitivity"), halconConfig.sensitivity),
                                      100);
    halconConfig.lineDegree = qBound(0,
                                     intParam(params, QStringLiteral("lineDegree"), halconConfig.lineDegree),
                                     100);
    halconConfig.edgePolarity = normalizedEdgePolarity(stringParam(params,
                                                                   QStringLiteral("edgePolarity"),
                                                                   halconConfig.edgePolarity));
    halconConfig.edgeType = normalizedEdgeType(stringParam(params,
                                                           QStringLiteral("edgeType"),
                                                           halconConfig.edgeType));
    halconConfig.existOk = boolParam(params,
                                     QStringLiteral("existOk"),
                                     boolParam(judgeRule,
                                               QStringLiteral("existOk"),
                                               halconConfig.existOk));
    halconConfig.timeoutMsInternalDefault = 1000;
    return halconConfig;
}

ToolResult makeLinePresenceError(const ToolConfig &config,
                                 const QString &status,
                                 const QString &message)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::LinePresence;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    return result;
}

} // namespace

bool LinePresenceAdapter::supports(ToolType type) const
{
    return type == ToolType::LinePresence;
}

ToolResult LinePresenceAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::LinePresence) {
        return makeLinePresenceError(config,
                                     QStringLiteral("invalid_tool_type"),
                                     QStringLiteral("LinePresenceAdapter only supports ToolType::LinePresence."));
    }

    const LinePresenceHalconConfig halconConfig = toHalconConfig(config);
    const LinePresenceHalconResult runnerResult = m_runner.run(request.image, halconConfig);

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::LinePresence;
    result.success = runnerResult.success;
    result.ok = runnerResult.ok;
    result.status = runnerResult.status;
    result.message = runnerResult.message;
    result.text = runnerResult.text;
    result.score = runnerResult.score;
    result.value = runnerResult.value;
    result.count = runnerResult.count;
    result.elapsedMs = runnerResult.elapsedMs;
    result.overlays = runnerResult.overlays;
    result.payload = runnerResult.payload;
    return result;
}
