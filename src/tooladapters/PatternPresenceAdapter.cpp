#include "tooladapters/PatternPresenceAdapter.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>

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

PatternPresenceHalconConfig toHalconConfig(const ToolConfig &config)
{
    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;

    PatternPresenceHalconConfig halconConfig;
    halconConfig.halconSoPath = stringParam(params,
                                            QStringLiteral("halconSoPath"),
                                            halconConfig.halconSoPath);
    halconConfig.roiNormalized = config.roiNormalized;
    halconConfig.templateRoiNormalized = rectParam(params,
                                                   QStringLiteral("templateRoiNormalized"),
                                                   halconConfig.templateRoiNormalized);
    halconConfig.templateSource = stringParam(params,
                                              QStringLiteral("templateSource"),
                                              halconConfig.templateSource);
    halconConfig.templateImagePath = stringParam(params,
                                                 QStringLiteral("templateImagePath"),
                                                 halconConfig.templateImagePath);
    halconConfig.modelPath = stringParam(params,
                                         QStringLiteral("modelPath"),
                                         halconConfig.modelPath);
    halconConfig.modelAutoCreate = boolParam(params,
                                             QStringLiteral("modelAutoCreate"),
                                             halconConfig.modelAutoCreate);
    halconConfig.modelCacheKey = stringParam(params,
                                             QStringLiteral("modelCacheKey"),
                                             halconConfig.modelCacheKey);
    halconConfig.templateShapeType = stringParam(params,
                                                 QStringLiteral("templateShapeType"),
                                                 halconConfig.templateShapeType);
    halconConfig.templateSensitivityMode = stringParam(params,
                                                       QStringLiteral("templateSensitivityMode"),
                                                       halconConfig.templateSensitivityMode);
    halconConfig.templateSensitivity = qBound(1,
                                              intParam(params,
                                                       QStringLiteral("templateSensitivity"),
                                                       halconConfig.templateSensitivity),
                                              10);
    halconConfig.detectRegionType = stringParam(params,
                                                QStringLiteral("detectRegionType"),
                                                halconConfig.detectRegionType);
    halconConfig.enablePositionCorrection = boolParam(params,
                                                      QStringLiteral("enablePositionCorrection"),
                                                      halconConfig.enablePositionCorrection);
    halconConfig.positionCorrectionSource = stringParam(params,
                                                        QStringLiteral("positionCorrectionSource"),
                                                        halconConfig.positionCorrectionSource);
    halconConfig.minScore = qBound(0,
                                   intParam(params, QStringLiteral("minScore"), halconConfig.minScore),
                                   100);
    halconConfig.polarity = stringParam(params, QStringLiteral("polarity"), halconConfig.polarity);
    halconConfig.scaleMin = qBound(1,
                                   intParam(params, QStringLiteral("scaleMin"), halconConfig.scaleMin),
                                   999);
    halconConfig.scaleMax = qBound(halconConfig.scaleMin,
                                   intParam(params, QStringLiteral("scaleMax"), halconConfig.scaleMax),
                                   999);
    halconConfig.angleStart = qBound(-180,
                                     intParam(params, QStringLiteral("angleStart"), halconConfig.angleStart),
                                     180);
    halconConfig.angleExtent = qBound(0,
                                      intParam(params, QStringLiteral("angleExtent"), halconConfig.angleExtent),
                                      360);
    halconConfig.timeoutMs = qMax(0,
                                  intParam(params, QStringLiteral("timeoutMs"), halconConfig.timeoutMs));
    halconConfig.showContourPoints = boolParam(params,
                                               QStringLiteral("showContourPoints"),
                                               halconConfig.showContourPoints);
    halconConfig.sortMode = stringParam(params, QStringLiteral("sortMode"), halconConfig.sortMode);
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
    halconConfig.scoreThreshold = qBound(0,
                                         intParam(params,
                                                  QStringLiteral("scoreThreshold"),
                                                  intParam(judgeRule,
                                                           QStringLiteral("scoreThreshold"),
                                                           halconConfig.scoreThreshold)),
                                         100);
    halconConfig.maxCount = qMax(1,
                                 intParam(params,
                                          QStringLiteral("maxCount"),
                                          intParam(params,
                                                   QStringLiteral("max_count"),
                                                   intParam(judgeRule,
                                                            QStringLiteral("maxCount"),
                                                            intParam(judgeRule,
                                                                     QStringLiteral("max_count"),
                                                                     halconConfig.maxCount)))));
    halconConfig.expectedCount = qMax(1,
                                      intParam(params,
                                               QStringLiteral("expectedCount"),
                                               intParam(params,
                                                        QStringLiteral("expected_count"),
                                                        intParam(judgeRule,
                                                                 QStringLiteral("expectedCount"),
                                                                 intParam(judgeRule,
                                                                          QStringLiteral("expected_count"),
                                                                          halconConfig.expectedCount)))));
    return halconConfig;
}

ToolResult makePatternPresenceError(const ToolConfig &config,
                                    const QString &status,
                                    const QString &message)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::PatternPresence;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    return result;
}

} // namespace

bool PatternPresenceAdapter::supports(ToolType type) const
{
    return type == ToolType::PatternPresence;
}

ToolResult PatternPresenceAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::PatternPresence)
        return makePatternPresenceError(config,
                                        QStringLiteral("invalid_tool_type"),
                                        QStringLiteral("PatternPresenceAdapter only supports ToolType::PatternPresence."));

    const PatternPresenceHalconConfig halconConfig = toHalconConfig(config);
    const PatternPresenceHalconResult runnerResult = m_runner.run(request.image,
                                                                  request.referenceImage,
                                                                  halconConfig);

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::PatternPresence;
    result.success = runnerResult.success;
    result.ok = runnerResult.ok;
    result.status = runnerResult.status;
    result.message = runnerResult.message;
    result.text = runnerResult.text;
    result.score = runnerResult.score;
    result.value = runnerResult.score;
    result.count = runnerResult.count;
    result.elapsedMs = runnerResult.elapsedMs;
    result.overlays = runnerResult.overlays;
    result.payload = runnerResult.payload;
    return result;
}
