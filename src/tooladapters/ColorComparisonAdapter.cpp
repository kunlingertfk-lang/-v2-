#include "tooladapters/ColorComparisonAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>

#include <cmath>

namespace {

bool finiteValue(qreal value)
{
    return std::isfinite(static_cast<double>(value));
}

int intParam(const QJsonObject &object, const QString &key, int defaultValue)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble())
        return value.toInt(defaultValue);

    bool ok = false;
    const int parsed = value.toString().trimmed().toInt(&ok);
    return ok ? parsed : defaultValue;
}

bool boolParam(const QJsonObject &object, const QString &key, bool defaultValue)
{
    const QJsonValue value = object.value(key);
    if (value.isBool())
        return value.toBool(defaultValue);

    const QString text = value.toString().trimmed().toLower();
    if (text == QStringLiteral("true") || text == QStringLiteral("1") || text == QStringLiteral("yes"))
        return true;
    if (text == QStringLiteral("false") || text == QStringLiteral("0") || text == QStringLiteral("no"))
        return false;
    return defaultValue;
}

QString stringParam(const QJsonObject &object,
                    const QString &key,
                    const QString &defaultValue = QString())
{
    const QString value = object.value(key).toString().trimmed();
    return value.isEmpty() ? defaultValue : value;
}

QRectF rectFromJson(const QJsonObject &json, const QRectF &fallback)
{
    if (json.isEmpty())
        return fallback;

    return QRectF(json.value(QStringLiteral("x")).toDouble(fallback.x()),
                  json.value(QStringLiteral("y")).toDouble(fallback.y()),
                  json.value(QStringLiteral("width")).toDouble(fallback.width()),
                  json.value(QStringLiteral("height")).toDouble(fallback.height()));
}

QPointF pointFromJson(const QJsonObject &json, const QPointF &fallback = QPointF())
{
    return QPointF(json.value(QStringLiteral("x")).toDouble(fallback.x()),
                   json.value(QStringLiteral("y")).toDouble(fallback.y()));
}

QVector<QPointF> pointsFromJson(const QJsonArray &array)
{
    QVector<QPointF> points;
    points.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QPointF point = pointFromJson(value.toObject());
        if (finiteValue(point.x()) && finiteValue(point.y())) {
            points.append(QPointF(qBound(0.0, point.x(), 1.0),
                                  qBound(0.0, point.y(), 1.0)));
        }
    }
    return points.size() >= 3 ? points : QVector<QPointF>();
}

QVector<double> featureFromJson(const QJsonArray &array)
{
    QVector<double> feature;
    feature.reserve(array.size());
    for (const QJsonValue &value : array)
        feature.append(value.toDouble());
    return feature;
}

ColorComparisonHalconConfig toRunnerConfig(const ToolConfig &config)
{
    const QJsonObject params = config.params;
    const QJsonObject colorComparison = params.value(QStringLiteral("colorComparison")).toObject();
    const QJsonObject judgeRule = config.judgeRule;

    ColorComparisonHalconConfig runnerConfig;
    const QString requestedHalconSoPath =
            stringParam(colorComparison,
                        QStringLiteral("halconSoPath"),
                        stringParam(params, QStringLiteral("halconSoPath")));
    runnerConfig.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                requestedHalconSoPath,
                &runnerConfig.halconSoPathCandidates);
    runnerConfig.templateRoiNormalized =
            rectFromJson(colorComparison.value(QStringLiteral("templateRoiNormalized")).toObject(),
                         runnerConfig.templateRoiNormalized);
    runnerConfig.templateMaskPolygonNormalized =
            pointsFromJson(colorComparison.value(QStringLiteral("templateMaskPolygon")).toArray());
    runnerConfig.templateFeature =
            featureFromJson(colorComparison.value(QStringLiteral("templateFeature")).toArray());
    runnerConfig.detectRoiNormalized = config.roiNormalized.width() > 0.0 &&
            config.roiNormalized.height() > 0.0
            ? config.roiNormalized
            : rectFromJson(colorComparison.value(QStringLiteral("detectRoiNormalized")).toObject(),
                           runnerConfig.detectRoiNormalized);
    runnerConfig.detectRegionType =
            stringParam(colorComparison, QStringLiteral("detectRegionType"), runnerConfig.detectRegionType);

    const QJsonObject circleJson =
            colorComparison.value(QStringLiteral("detectCircleNormalized")).toObject();
    runnerConfig.detectCircleCenterNormalized =
            pointFromJson(circleJson.value(QStringLiteral("center")).toObject());
    runnerConfig.detectCircleRadiusNormalized =
            circleJson.value(QStringLiteral("radius")).toDouble(0.0);
    runnerConfig.detectCircleBoundingRectNormalized =
            rectFromJson(circleJson.value(QStringLiteral("boundingRect")).toObject(),
                         runnerConfig.detectRoiNormalized);
    runnerConfig.detectMaskPolygonNormalized =
            pointsFromJson(colorComparison.value(QStringLiteral("detectMaskPolygon")).toArray());
    runnerConfig.comparisonMode =
            stringParam(colorComparison, QStringLiteral("comparisonMode"), runnerConfig.comparisonMode);
    runnerConfig.featureType =
            stringParam(colorComparison, QStringLiteral("featureType"), runnerConfig.featureType);
    runnerConfig.sensitivity =
            stringParam(colorComparison, QStringLiteral("sensitivity"), runnerConfig.sensitivity);
    runnerConfig.brightnessEnabled =
            boolParam(colorComparison, QStringLiteral("brightnessEnabled"), runnerConfig.brightnessEnabled);
    runnerConfig.enablePositionCorrection =
            boolParam(colorComparison,
                      QStringLiteral("enablePositionCorrection"),
                      runnerConfig.enablePositionCorrection);
    runnerConfig.positionCorrectionSource =
            stringParam(colorComparison,
                        QStringLiteral("positionCorrectionSource"),
                        runnerConfig.positionCorrectionSource);
    runnerConfig.minScore = qBound(0,
                                   intParam(judgeRule, QStringLiteral("minScore"),
                                            intParam(colorComparison,
                                                     QStringLiteral("minScore"),
                                                     runnerConfig.minScore)),
                                   100);
    return runnerConfig;
}

ToolResult makeError(const ToolConfig &config, const QString &status, const QString &message)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::ColorComparison;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    return result;
}

} // namespace

bool ColorComparisonAdapter::supports(ToolType type) const
{
    return type == ToolType::ColorComparison;
}

ToolResult ColorComparisonAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::ColorComparison) {
        return makeError(config,
                         QStringLiteral("invalid_tool_type"),
                         QStringLiteral("ColorComparisonAdapter only supports ToolType::ColorComparison."));
    }

    const ColorComparisonHalconResult runnerResult =
            m_runner.run(request.image, toRunnerConfig(config));

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::ColorComparison;
    result.success = runnerResult.success;
    result.ok = runnerResult.ok;
    result.status = runnerResult.status;
    result.message = runnerResult.message;
    result.score = runnerResult.score;
    result.value = runnerResult.similarity;
    result.count = runnerResult.templateFeature.isEmpty() ? 0 : 1;
    result.elapsedMs = runnerResult.elapsedMs;
    result.text = QString::number(runnerResult.score, 'f', 2);
    result.overlays = runnerResult.overlays;
    result.payload = runnerResult.payload;
    return result;
}
