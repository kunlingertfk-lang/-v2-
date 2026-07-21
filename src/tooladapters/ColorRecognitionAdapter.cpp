#include "tooladapters/ColorRecognitionAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "toolcore/PositionCorrection.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>
#include <QtGlobal>

#include <cmath>

namespace {

// 从配置对象中读取整型参数，兼容数字和字符串两种历史保存格式。
int intParam(const QJsonObject &object, const QString &key, int defaultValue)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble())
        return value.toInt(defaultValue);

    bool ok = false;
    const int parsed = value.toString().trimmed().toInt(&ok);
    return ok ? parsed : defaultValue;
}

double doubleParam(const QJsonObject &object, const QString &key, double defaultValue)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble())
        return value.toDouble(defaultValue);
    bool ok = false;
    const double parsed = value.toString().trimmed().toDouble(&ok);
    return ok ? parsed : defaultValue;
}

// 从配置对象中读取布尔参数，兼容 true/false、1/0、yes/no 文本。
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

// 从配置对象中读取非空字符串，空值时使用调用方提供的默认值。
QString stringParam(const QJsonObject &object, const QString &key, const QString &defaultValue = QString())
{
    const QString value = object.value(key).toString().trimmed();
    return value.isEmpty() ? defaultValue : value;
}

// 将保存的归一化矩形 JSON 还原为 QRectF，缺字段时保留 fallback。
QRectF rectFromJson(const QJsonObject &json, const QRectF &fallback)
{
    if (json.isEmpty())
        return fallback;

    return QRectF(json.value(QStringLiteral("x")).toDouble(fallback.x()),
                  json.value(QStringLiteral("y")).toDouble(fallback.y()),
                  json.value(QStringLiteral("width")).toDouble(fallback.width()),
                  json.value(QStringLiteral("height")).toDouble(fallback.height()));
}

// 将保存的归一化点 JSON 还原为 QPointF。
QPointF pointFromJson(const QJsonObject &json, const QPointF &fallback = QPointF())
{
    return QPointF(json.value(QStringLiteral("x")).toDouble(fallback.x()),
                   json.value(QStringLiteral("y")).toDouble(fallback.y()));
}

// 解析屏蔽多边形点集，并裁剪到 0-1 归一化图像坐标。
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

// 解析模板样本中已保存的颜色直方图特征。
QVector<double> featureFromJson(const QJsonArray &array)
{
    QVector<double> feature;
    feature.reserve(array.size());
    for (const QJsonValue &value : array)
        feature.append(value.toDouble());
    return feature;
}

// 解析模板标签列表，供 runner 将 classId 映射为显示类别名。
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

// 解析模板样本列表，只保留具备标签、类别 id 和特征向量的可用样本。
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
        sample.featureSignature = json.value(QStringLiteral("featureSignature")).toString().trimmed();
        sample.roiNormalized = rectFromJson(json.value(QStringLiteral("roiNormalized")).toObject(),
                                            sample.roiNormalized);
        if (!sample.label.isEmpty() && sample.classId > 0 && !sample.feature.isEmpty())
            samples.append(sample);
    }
    return samples;
}

// 从 colorModel 中选出当前激活模板；无激活 id 时回退到第一个模板。
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

// 将 ToolConfig 的 params/judgeRule 解析为 HALCON runner 需要的强类型配置。
ColorRecognitionHalconConfig toRunnerConfig(const ToolConfig &config,
                                            const QJsonObject &runtimeContext)
{
    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;
    const QJsonObject colorModel = params.value(QStringLiteral("colorModel")).toObject();
    const QJsonObject colorTemplate = activeTemplateObject(colorModel);
    const QJsonObject modelSource = colorTemplate.isEmpty() ? colorModel : colorTemplate;
    const QJsonObject hsvConfig = modelSource.value(QStringLiteral("backendConfigs"))
            .toObject().value(QStringLiteral("hsvHistogram")).toObject();

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
    runnerConfig.colorDecisionMode = stringParam(params,
                                                 QStringLiteral("colorDecisionMode"),
                                                 runnerConfig.colorDecisionMode);
    if (hsvConfig.contains(QStringLiteral("classifierVersion"))) {
        runnerConfig.classifierVersion = stringParam(
                    hsvConfig, QStringLiteral("classifierVersion"), QString());
        runnerConfig.classifierParamsHash = stringParam(
                    hsvConfig, QStringLiteral("classifierParamsHash"), QString());
    } else {
        runnerConfig.classifierVersion = colorRecognitionHsvLegacyClassifierVersion();
        runnerConfig.classifierParamsHash = colorRecognitionHsvClassifierParamsHash(
                    runnerConfig.classifierVersion);
    }
    runnerConfig.detectMaskPolygonNormalized =
            pointsFromJson(params.value(QStringLiteral("detectMaskPolygon")).toArray());
    if (runnerConfig.detectMaskPolygonNormalized.size() < 3)
        runnerConfig.detectMaskPolygonNormalized.clear();
    const PositionCorrectionConfig positionCorrection = PositionCorrection::fromParams(params);
    runnerConfig.enablePositionCorrection = positionCorrection.enabled;
    runnerConfig.positionCorrectionSourceId = positionCorrection.sourceId;
    runnerConfig.positionCorrectionSource = positionCorrection.source;
    const QJsonObject inputMetadata = runtimeContext.value(QStringLiteral("input")).toObject();
    runnerConfig.pixelFormat = inputMetadata.value(QStringLiteral("pixelFormat")).toString().trimmed();
    runnerConfig.validBits = inputMetadata.value(QStringLiteral("validBits")).toInt(-1);
    runnerConfig.bitShift = inputMetadata.value(QStringLiteral("bitShift")).toInt(-1);
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

ColorRecognitionGmmRunConfig toGmmRunnerConfig(const ToolConfig &config,
                                               const QJsonObject &runtimeContext,
                                               const QJsonObject &colorTemplate)
{
    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;
    const QJsonObject gmmConfig = colorTemplate.value(QStringLiteral("backendConfigs")).toObject()
            .value(QStringLiteral("cielabGmm")).toObject();
    const QJsonObject model = colorTemplate.value(QStringLiteral("backendModels")).toObject()
            .value(QStringLiteral("cielabGmm")).toObject();
    ColorRecognitionGmmRunConfig result;
    result.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                stringParam(params, QStringLiteral("halconSoPath")), &result.halconSoPathCandidates);
    result.roiNormalized = config.roiNormalized;
    result.detectRegionType = stringParam(params, QStringLiteral("detectRegionType"), result.detectRegionType);
    const QJsonObject circle = params.value(QStringLiteral("detectCircleNormalized")).toObject();
    result.detectCircleCenterNormalized = pointFromJson(circle.value(QStringLiteral("center")).toObject());
    result.detectCircleRadiusNormalized = circle.value(QStringLiteral("radius")).toDouble();
    result.detectCircleBoundingRectNormalized = rectFromJson(circle.value(QStringLiteral("boundingRect")).toObject(), result.roiNormalized);
    result.detectMaskPolygonNormalized = pointsFromJson(params.value(QStringLiteral("detectMaskPolygon")).toArray());
    const PositionCorrectionConfig correction = PositionCorrection::fromParams(params);
    result.enablePositionCorrection = correction.enabled;
    result.positionCorrectionSourceId = correction.sourceId;
    result.positionCorrectionSource = correction.source;
    const QJsonObject input = runtimeContext.value(QStringLiteral("input")).toObject();
    result.pixelFormat = input.value(QStringLiteral("pixelFormat")).toString().trimmed();
    result.validBits = input.value(QStringLiteral("validBits")).toInt(-1);
    result.bitShift = input.value(QStringLiteral("bitShift")).toInt(-1);
    result.modelState = model.value(QStringLiteral("state")).toString(QStringLiteral("empty"));
    result.algorithmVersion = model.value(QStringLiteral("algorithmVersion")).toString();
    result.featureSchemaVersion = model.value(QStringLiteral("featureSchemaVersion")).toString();
    result.colorChannels = model.value(QStringLiteral("colorChannels")).toString(
                gmmConfig.value(QStringLiteral("colorChannels")).toString(QStringLiteral("ab")));
    result.trainingDataHash = model.value(QStringLiteral("trainingDataHash")).toString();
    result.buildParamsHash = model.value(QStringLiteral("buildParamsHash")).toString();
    result.maxSamplesPerClass = intParam(gmmConfig, QStringLiteral("maxSamplesPerClass"), 10000);
    result.gmmRejectionThreshold = doubleParam(
                gmmConfig, QStringLiteral("gmmRejectionThreshold"),
                kColorRecognitionGmmDefaultRejectionThreshold);
    for (const QJsonValue &value : colorTemplate.value(QStringLiteral("labels")).toArray()) {
        const QJsonObject json = value.toObject();
        const QString name = json.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty()) result.labels.append(ColorRecognitionGmmLabel{name, json.value(QStringLiteral("classId")).toInt()});
    }
    for (const QJsonValue &value : model.value(QStringLiteral("classIdOrder")).toArray())
        result.classIdOrder.append(value.toInt());
    for (const QJsonValue &value : model.value(QStringLiteral("classes")).toArray()) {
        const QJsonObject json = value.toObject();
        ColorRecognitionGmmClassDiagnostics item;
        item.classId = json.value(QStringLiteral("classId")).toInt();
        item.label = json.value(QStringLiteral("label")).toString();
        item.roiCount = json.value(QStringLiteral("roiCount")).toInt();
        item.availablePixels = static_cast<qint64>(json.value(QStringLiteral("availablePixels")).toDouble());
        item.requestedTrainingPixels = json.value(QStringLiteral("requestedTrainingPixels"))
                .toInt(json.value(QStringLiteral("trainingPixels")).toInt());
        item.trainingPixels = json.value(QStringLiteral("trainingPixels")).toInt();
        item.minCenters = json.value(QStringLiteral("minCenters")).toInt(1);
        item.maxCenters = json.value(QStringLiteral("maxCenters")).toInt(1);
        result.classes.append(item);
    }
    result.artifact.serializedGmmBase64 = model.value(QStringLiteral("serializedGmmBase64")).toString();
    result.artifact.serializedSize = static_cast<qint64>(model.value(QStringLiteral("serializedSize")).toDouble());
    result.artifact.serializedSha256 = model.value(QStringLiteral("serializedSha256")).toString();
    result.judgeMode = stringParam(judgeRule, QStringLiteral("mode"), result.judgeMode);
    if (result.judgeMode == QStringLiteral("category")) result.judgeMode = QStringLiteral("expected_class");
    result.minScore = intParam(judgeRule, QStringLiteral("minScore"), result.minScore);
    result.minCategoryConfidence = intParam(judgeRule, QStringLiteral("minCategoryConfidence"), result.minCategoryConfidence);
    result.minClassifiedCoverage = intParam(judgeRule, QStringLiteral("minClassifiedCoverage"), result.minClassifiedCoverage);
    result.expectedClassId = intParam(judgeRule, QStringLiteral("expectedClassId"), -1);
    result.expectedLabel = stringParam(judgeRule, QStringLiteral("expectedLabel"));
    return result;
}

// 在 Adapter 层生成统一的颜色识别错误 ToolResult。
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

// ToolEngine 分发前调用，用于确认该 Adapter 是否处理颜色识别工具。
bool ColorRecognitionAdapter::supports(ToolType type) const
{
    return type == ToolType::ColorRecognition;
}

// 颜色识别运行桥接：校验工具类型、调用 HALCON runner，并转换为统一 ToolResult。
ToolResult ColorRecognitionAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::ColorRecognition) {
        return makeColorRecognitionError(
                    config,
                    QStringLiteral("invalid_tool_type"),
                    QStringLiteral("ColorRecognitionAdapter only supports ToolType::ColorRecognition."));
    }

    const QJsonObject colorModel = config.params.value(QStringLiteral("colorModel")).toObject();
    const QJsonObject colorTemplate = activeTemplateObject(colorModel);
    const QString recognitionBackend = stringParam(
                colorTemplate,
                QStringLiteral("recognitionBackend"),
                stringParam(config.params,
                            QStringLiteral("recognitionBackend"),
                            QStringLiteral("hsv_histogram")));
    if (recognitionBackend == QStringLiteral("cielab_gmm")) {
        const ColorRecognitionGmmRunResult runnerResult = m_runner.runGmmModel(
                    request.image, toGmmRunnerConfig(config, request.runtimeContext, colorTemplate));
        ToolResult result;
        result.toolId = config.toolId;
        result.toolType = ToolType::ColorRecognition;
        result.success = runnerResult.success;
        result.ok = runnerResult.ok;
        result.status = runnerResult.status;
        result.message = runnerResult.message;
        result.score = runnerResult.score;
        result.value = runnerResult.categoryConfidence * 100.0;
        result.count = runnerResult.classes.size();
        result.elapsedMs = runnerResult.elapsedMs;
        result.text = runnerResult.predictedLabel;
        result.overlays = runnerResult.overlays;
        result.payload = runnerResult.payload;
        result.payload.insert(QStringLiteral("recognitionBackend"), recognitionBackend);
        return result;
    }
    if (recognitionBackend != QStringLiteral("hsv_histogram")) {
        ToolResult result = makeColorRecognitionError(
                    config,
                    QStringLiteral("unsupported_recognition_backend"),
                    QStringLiteral("Unsupported color recognition backend: %1").arg(recognitionBackend));
        result.payload.insert(QStringLiteral("recognitionBackend"), recognitionBackend);
        return result;
    }

    const ColorRecognitionHalconResult runnerResult =
            m_runner.run(request.image, toRunnerConfig(config, request.runtimeContext));

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
