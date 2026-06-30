#include "tooladapters/ColorRecognitionAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>
#include <QtGlobal>

#include <cmath>

namespace {

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

QString stringParam(const QJsonObject &object, const QString &key, const QString &defaultValue = QString())
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
        const QJsonObject json = value.toObject();
        const QPointF point(json.value(QStringLiteral("x")).toDouble(),
                            json.value(QStringLiteral("y")).toDouble());
        if (std::isfinite(point.x()) && std::isfinite(point.y())) {
            points.append(QPointF(qBound(0.0, point.x(), 1.0),
                                  qBound(0.0, point.y(), 1.0)));
        }
    }
    return points;
}

QVector<double> featureFromJson(const QJsonArray &array)
{
    QVector<double> feature;
    feature.reserve(array.size());
    for (const QJsonValue &value : array)
        feature.append(value.toDouble());
    return feature;
}

QVector<ColorRecognitionHalconLabel> labelsFromJson(const QJsonArray &array)
{
    QVector<ColorRecognitionHalconLabel> labels;
    labels.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject json = value.toObject();
        ColorRecognitionHalconLabel label;
        label.name = json.value(QStringLiteral("name")).toString().trimmed();
        label.classId = json.value(QStringLiteral("classId")).toInt();
        if (!label.name.isEmpty())
            labels.append(label);
    }
    return labels;
}

QVector<ColorRecognitionHalconSample> samplesFromJson(const QJsonArray &array)
{
    QVector<ColorRecognitionHalconSample> samples;
    samples.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject json = value.toObject();
        ColorRecognitionHalconSample sample;
        sample.label = json.value(QStringLiteral("label")).toString().trimmed();
        sample.classId = json.value(QStringLiteral("classId")).toInt();
        sample.feature = featureFromJson(json.value(QStringLiteral("feature")).toArray());
        sample.roiNormalized = rectFromJson(json.value(QStringLiteral("roiNormalized")).toObject(),
                                            sample.roiNormalized);
        if (!sample.label.isEmpty() && sample.classId > 0 && !sample.feature.isEmpty())
            samples.append(sample);
    }
    return samples;
}

QJsonObject activeTemplateObject(const QJsonObject &colorModel)
{
    const QJsonArray templates = colorModel.value(QStringLiteral("templates")).toArray();
    if (templates.isEmpty())
        return QJsonObject();

    const QString activeTemplateId =
            colorModel.value(QStringLiteral("activeTemplateId")).toString().trimmed();
    for (const QJsonValue &value : templates) {
        const QJsonObject candidate = value.toObject();
        if (!activeTemplateId.isEmpty() &&
            candidate.value(QStringLiteral("templateId")).toString() == activeTemplateId) {
            return candidate;
        }
    }
    return templates.first().toObject();
}

ColorRecognitionHalconConfig toRunnerConfig(const ToolConfig &config)
{
    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;
    const QJsonObject colorModel = params.value(QStringLiteral("colorModel")).toObject();
    const QJsonObject colorTemplate = activeTemplateObject(colorModel);
    const QJsonObject modelSource = colorTemplate.isEmpty() ? colorModel : colorTemplate;

    ColorRecognitionHalconConfig runnerConfig;
    const QString requestedHalconSoPath = stringParam(params, QStringLiteral("halconSoPath"));
    runnerConfig.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                requestedHalconSoPath,
                &runnerConfig.halconSoPathCandidates);
    if (config.roiNormalized.width() > 0.0 && config.roiNormalized.height() > 0.0)
        runnerConfig.roiNormalized = config.roiNormalized;
    runnerConfig.detectRegionType = stringParam(params,
                                                QStringLiteral("detectRegionType"),
                                                runnerConfig.detectRegionType);
    const QJsonObject circleJson = params.value(QStringLiteral("detectCircleNormalized")).toObject();
    runnerConfig.detectCircleCenterNormalized =
            pointFromJson(circleJson.value(QStringLiteral("center")).toObject());
    runnerConfig.detectCircleRadiusNormalized =
            circleJson.value(QStringLiteral("radius")).toDouble(0.0);
    runnerConfig.detectCircleBoundingRectNormalized =
            rectFromJson(circleJson.value(QStringLiteral("boundingRect")).toObject(),
                         runnerConfig.roiNormalized);
    runnerConfig.detectMaskPolygonNormalized =
            pointsFromJson(params.value(QStringLiteral("detectMaskPolygon")).toArray());
    if (runnerConfig.detectMaskPolygonNormalized.size() < 3)
        runnerConfig.detectMaskPolygonNormalized.clear();
    runnerConfig.featureType = stringParam(modelSource,
                                           QStringLiteral("featureType"),
                                           stringParam(params,
                                                       QStringLiteral("featureType"),
                                                       runnerConfig.featureType));
    runnerConfig.sensitivity = stringParam(modelSource,
                                           QStringLiteral("sensitivity"),
                                           stringParam(params,
                                                       QStringLiteral("sensitivity"),
                                                       runnerConfig.sensitivity));
    runnerConfig.brightnessEnabled = boolParam(modelSource,
                                               QStringLiteral("brightnessEnabled"),
                                               boolParam(params,
                                                         QStringLiteral("brightnessEnabled"),
                                                         runnerConfig.brightnessEnabled));
    runnerConfig.knnK = qMax(1, intParam(modelSource,
                                         QStringLiteral("knnK"),
                                         intParam(params,
                                                  QStringLiteral("knnK"),
                                                  runnerConfig.knnK)));
    runnerConfig.knnDistance = stringParam(modelSource,
                                           QStringLiteral("knnDistance"),
                                           stringParam(params,
                                                       QStringLiteral("knnDistance"),
                                                       runnerConfig.knnDistance));
    runnerConfig.labels = labelsFromJson(modelSource.value(QStringLiteral("labels")).toArray());
    runnerConfig.samples = samplesFromJson(modelSource.value(QStringLiteral("samples")).toArray());
    runnerConfig.judgeMode = stringParam(judgeRule, QStringLiteral("mode"), runnerConfig.judgeMode);
    runnerConfig.minScore = qBound(0,
                                   intParam(judgeRule,
                                            QStringLiteral("minScore"),
                                            runnerConfig.minScore),
                                   100);
    runnerConfig.expectedLabel = stringParam(judgeRule,
                                             QStringLiteral("expectedLabel"),
                                             runnerConfig.expectedLabel);
    return runnerConfig;
}

ToolResult makeColorRecognitionError(const ToolConfig &config,
                                     const QString &status,
                                     const QString &message)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::ColorRecognition;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    return result;
}

} // namespace

bool ColorRecognitionAdapter::supports(ToolType type) const
{
    return type == ToolType::ColorRecognition;
}

ToolResult ColorRecognitionAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::ColorRecognition) {
        return makeColorRecognitionError(
                    config,
                    QStringLiteral("invalid_tool_type"),
                    QStringLiteral("ColorRecognitionAdapter only supports ToolType::ColorRecognition."));
    }

    const ColorRecognitionHalconResult runnerResult = m_runner.run(request.image, toRunnerConfig(config));

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::ColorRecognition;
    result.success = runnerResult.success;
    result.ok = runnerResult.ok;
    result.status = runnerResult.status;
    result.message = runnerResult.message;
    result.score = runnerResult.score;
    result.value = runnerResult.rating;
    result.count = runnerResult.sampleCount;
    result.elapsedMs = runnerResult.elapsedMs;
    result.text = runnerResult.predictedLabel;
    result.overlays = runnerResult.overlays;
    result.payload = runnerResult.payload;
    return result;
}
