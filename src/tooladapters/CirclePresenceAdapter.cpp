#include "tooladapters/CirclePresenceAdapter.h"

#include <QJsonObject>
#include <QJsonValue>
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

CirclePresenceHalconConfig toHalconConfig(const ToolConfig &config)
{
    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;

    CirclePresenceHalconConfig halconConfig;
    halconConfig.halconSoPath = stringParam(params,
                                            QStringLiteral("halconSoPath"),
                                            halconConfig.halconSoPath);
    if (isFiniteRoi(config.roiNormalized))
        halconConfig.roiNormalized = config.roiNormalized;
    halconConfig.detectRegionType = stringParam(params,
                                                QStringLiteral("detectRegionType"),
                                                halconConfig.detectRegionType);
    halconConfig.sensitivity = qBound(0,
                                      intParam(params,
                                               QStringLiteral("sensitivity"),
                                               halconConfig.sensitivity),
                                      100);
    halconConfig.roundness = qBound(0,
                                    intParam(params,
                                             QStringLiteral("roundness"),
                                             halconConfig.roundness),
                                    100);
    halconConfig.edgePolarity = stringParam(params,
                                            QStringLiteral("edgePolarity"),
                                            halconConfig.edgePolarity);
    halconConfig.edgeType = stringParam(params,
                                        QStringLiteral("edgeType"),
                                        halconConfig.edgeType);
    halconConfig.existOk = boolParam(params,
                                     QStringLiteral("existOk"),
                                     boolParam(judgeRule,
                                               QStringLiteral("existOk"),
                                               halconConfig.existOk));
    halconConfig.timeoutMs = qMax(0,
                                  intParam(params,
                                           QStringLiteral("timeoutMs"),
                                           halconConfig.timeoutMs));
    halconConfig.enablePositionCorrection = boolParam(params,
                                                      QStringLiteral("enablePositionCorrection"),
                                                      halconConfig.enablePositionCorrection);
    halconConfig.positionCorrectionSource = stringParam(params,
                                                        QStringLiteral("positionCorrectionSource"),
                                                        halconConfig.positionCorrectionSource);
    return halconConfig;
}

ToolResult makeCirclePresenceError(const ToolConfig &config,
                                   const QString &status,
                                   const QString &message)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::CirclePresence;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    return result;
}

} // namespace

bool CirclePresenceAdapter::supports(ToolType type) const
{
    return type == ToolType::CirclePresence;
}

ToolResult CirclePresenceAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::CirclePresence) {
        return makeCirclePresenceError(config,
                                       QStringLiteral("invalid_tool_type"),
                                       QStringLiteral("CirclePresenceAdapter only supports ToolType::CirclePresence."));
    }

    const CirclePresenceHalconConfig halconConfig = toHalconConfig(config);
    const CirclePresenceHalconResult runnerResult = m_runner.run(request.image, halconConfig);

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::CirclePresence;
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
