#include "tooladapters/ContourPresenceAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>
#include <QtGlobal>

namespace {

double doubleParam(const QJsonObject &params, const QString &key, const double defaultValue)
{
    const QJsonValue value = params.value(key);
    if (value.isDouble())
        return value.toDouble(defaultValue);

    bool ok = false;
    const double parsed = value.toString().trimmed().toDouble(&ok);
    return ok ? parsed : defaultValue;
}

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

double normalizedScoreParam(const QJsonObject &params,
                            const QString &key,
                            const double defaultValue)
{
    const double value = doubleParam(params, key, defaultValue);
    if (value > 1.0)
        return qBound(0.0, value / 100.0, 1.0);
    return qBound(0.0, value, 1.0);
}

ContourPresenceHalconConfig toHalconConfig(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;

    ContourPresenceHalconConfig halconConfig;
    halconConfig.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                stringParam(params, QStringLiteral("halconSoPath"), QString()),
                &halconConfig.halconSoPathCandidates);
    halconConfig.roiNormalized = config.roiNormalized;
    halconConfig.templateRoiNormalized = rectParam(params,
                                                   QStringLiteral("templateRoiNormalized"),
                                                   halconConfig.templateRoiNormalized);
    halconConfig.templatePolygonNormalized = pointsParam(params,
                                                         QStringLiteral("templatePolygonNormalized"));
    halconConfig.templateSource = stringParam(params,
                                              QStringLiteral("templateSource"),
                                              halconConfig.templateSource);
    halconConfig.detectRegionType = stringParam(params,
                                                QStringLiteral("detectRegionType"),
                                                halconConfig.detectRegionType);
    halconConfig.detectPolygonNormalized = pointsParam(params,
                                                       QStringLiteral("detectPolygonNormalized"));
    halconConfig.templateShapeType = stringParam(params,
                                                 QStringLiteral("templateShapeType"),
                                                 halconConfig.templateShapeType);
    halconConfig.enablePositionCorrection = boolParam(params,
                                                      QStringLiteral("enablePositionCorrection"),
                                                      halconConfig.enablePositionCorrection);
    halconConfig.positionCorrectionSource = stringParam(params,
                                                        QStringLiteral("positionCorrectionSource"),
                                                        halconConfig.positionCorrectionSource);
    halconConfig.minScore = normalizedScoreParam(params,
                                                 QStringLiteral("minScore"),
                                                 halconConfig.minScore);
    halconConfig.polarity = stringParam(params, QStringLiteral("polarity"), halconConfig.polarity);
    halconConfig.thresholdType = stringParam(params,
                                             QStringLiteral("thresholdType"),
                                             halconConfig.thresholdType);
    halconConfig.scaleMode = stringParam(params,
                                         QStringLiteral("scaleMode"),
                                         halconConfig.scaleMode);
    halconConfig.speedScale = qBound(1,
                                     intParam(params,
                                              QStringLiteral("speedScale"),
                                              halconConfig.speedScale),
                                     999);
    halconConfig.featureScale = qBound(1,
                                       intParam(params,
                                                QStringLiteral("featureScale"),
                                                halconConfig.featureScale),
                                       999);
    halconConfig.thresholdMode = stringParam(params,
                                             QStringLiteral("thresholdMode"),
                                             halconConfig.thresholdMode);
    halconConfig.grayThreshold = qBound(0,
                                        intParam(params,
                                                 QStringLiteral("grayThreshold"),
                                                 halconConfig.grayThreshold),
                                        255);
    halconConfig.chainMode = stringParam(params,
                                         QStringLiteral("chainMode"),
                                         halconConfig.chainMode);
    halconConfig.minChainLength = qBound(1,
                                         intParam(params,
                                                  QStringLiteral("minChainLength"),
                                                  halconConfig.minChainLength),
                                         9999);
    halconConfig.scaleMin = qBound(0.0,
                                   doubleParam(params,
                                               QStringLiteral("scaleMin"),
                                               halconConfig.scaleMin),
                                   999.0);
    halconConfig.scaleMax = qBound(halconConfig.scaleMin,
                                   doubleParam(params,
                                               QStringLiteral("scaleMax"),
                                               halconConfig.scaleMax),
                                   999.0);
    halconConfig.angleStart = qBound(-180.0,
                                     doubleParam(params,
                                                 QStringLiteral("angleStart"),
                                                 halconConfig.angleStart),
                                     180.0);
    halconConfig.angleExtent = qBound(0.0,
                                      doubleParam(params,
                                                  QStringLiteral("angleExtent"),
                                                  halconConfig.angleExtent),
                                      360.0);
    halconConfig.timeoutMs = qMax(0,
                                  intParam(params,
                                           QStringLiteral("timeoutMs"),
                                           halconConfig.timeoutMs));
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
    halconConfig.scoreThreshold = normalizedScoreParam(params,
                                                       QStringLiteral("scoreThreshold"),
                                                       normalizedScoreParam(judgeRule,
                                                                            QStringLiteral("scoreThreshold"),
                                                                            halconConfig.scoreThreshold));
    halconConfig.referenceTest = boolParam(request.runtimeContext,
                                           QStringLiteral("referenceTest"),
                                           halconConfig.referenceTest);
    return halconConfig;
}

ToolResult makeContourPresenceError(const ToolConfig &config,
                                    const QString &status,
                                    const QString &message)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::ContourPresence;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.text = QStringLiteral("error");
    return result;
}

} // namespace

bool ContourPresenceAdapter::supports(ToolType type) const
{
    return type == ToolType::ContourPresence;
}

ToolResult ContourPresenceAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::ContourPresence) {
        return makeContourPresenceError(config,
                                        QStringLiteral("invalid_tool_type"),
                                        QStringLiteral("ContourPresenceAdapter only supports ToolType::ContourPresence."));
    }

    const ContourPresenceHalconConfig halconConfig = toHalconConfig(request);
    const ContourPresenceHalconResult runnerResult = m_runner.run(request.image,
                                                                  request.referenceImage,
                                                                  halconConfig);

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::ContourPresence;
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
