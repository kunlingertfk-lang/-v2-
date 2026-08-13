#include "toolcore/PositionCorrection.h"

#include "algorithms/location/TemplateLocationConfig.h"

#include <QJsonValue>
#include <QStringList>

namespace {

// 兼容读取 JSON 布尔值及旧配置中的 true/false、1/0、yes/no 字符串。
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

bool exactReferenceBankVersion(const QJsonValue &value, int *version = nullptr)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (number != 4.0 && number != 5.0)
        return false;
    if (version)
        *version = static_cast<int>(number);
    return true;
}

// 读取并清理字符串参数；字段缺失或为空时返回调用方提供的默认值。
QString stringParam(const QJsonObject &object,
                    const QString &key,
                    const QString &defaultValue)
{
    const QString value = object.value(key).toString().trimmed();
    return value.isEmpty() ? defaultValue : value;
}

QString displayNameForTool(const ToolConfig &tool, int index)
{
    const QString name = tool.displayName.trimmed().isEmpty()
            ? (tool.toolName.trimmed().isEmpty()
               ? toolTypeToString(tool.toolType)
               : tool.toolName.trimmed())
            : tool.displayName.trimmed();
    return QStringLiteral("%1 %2").arg(index + 1).arg(name);
}

QJsonObject oldBinding(const QJsonObject &object, const QString &key)
{
    return object.value(key).toObject();
}

bool oldBindingMatches(const QJsonObject &binding,
                       const QString &producerId,
                       const QString &outputKey)
{
    return binding.value(QStringLiteral("producerId")).toString().trimmed() == producerId
            && binding.value(QStringLiteral("outputKey")).toString().trimmed() == outputKey;
}

QJsonObject legacyBinding(const QString &producerId,
                          const QString &outputKey,
                          const QString &displayPath)
{
    return QJsonObject{
        {QStringLiteral("producerId"), producerId},
        {QStringLiteral("outputKey"), outputKey},
        {QStringLiteral("displayPath"), displayPath}
    };
}

} // namespace

// 提供给界面及旧配置兼容字段使用的基准图来源显示文本。
QString PositionCorrection::defaultSource()
{
    return QStringLiteral("0 基准图.位置修正信息");
}

// 返回不会随界面序号变化的方案级基准图固定来源 ID。
QString PositionCorrection::defaultSourceId()
{
    return QStringLiteral("reference.positionCorrection");
}

// 显示编号从 1 统一为 0 后，历史方案仍应解析到同一个固定来源 ID。
QString PositionCorrection::normalizedSourceId(const QString &sourceIdOrDisplayText)
{
    const QString source = sourceIdOrDisplayText.trimmed();
    if (source.isEmpty()
            || source == defaultSourceId()
            || source == defaultSource()
            || source == QStringLiteral("1 基准图.位置修正信息")) {
        return defaultSourceId();
    }
    return source;
}

// 返回 UI 阶段统一的未实现原因，防止上层伪造位置修正成功状态。
QString PositionCorrection::notImplementedReason()
{
    return QStringLiteral("not implemented");
}

// 从 ToolConfig.params 解析消费配置，并为旧方案补齐稳定来源 ID。
PositionCorrectionConfig PositionCorrection::fromParams(
        const QJsonObject &params,
        bool defaultEnabled,
        const QString &defaultSourceText)
{
    PositionCorrectionConfig config;
    config.enabled = boolParam(params,
                               QStringLiteral("enablePositionCorrection"),
                               defaultEnabled);
    config.sourceId = normalizedSourceId(stringParam(
                                             params,
                                             QStringLiteral("positionCorrectionSourceId"),
                                             defaultSourceId()));
    config.source = stringParam(params,
                                QStringLiteral("positionCorrectionSource"),
                                defaultSourceText);
    if (config.sourceId == defaultSourceId())
        config.source = defaultSource();
    return config;
}

// 将消费配置写回参数对象，同时保留显示文本以兼容旧版本方案。
void PositionCorrection::writeParams(const PositionCorrectionConfig &config,
                                     QJsonObject *params)
{
    if (!params)
        return;

    params->insert(QStringLiteral("enablePositionCorrection"), config.enabled);
    const QString sourceId = normalizedSourceId(config.sourceId);
    params->insert(QStringLiteral("positionCorrectionSourceId"), sourceId);
    params->insert(QStringLiteral("positionCorrectionSource"),
                   sourceId == defaultSourceId()
                   ? defaultSource()
                   : config.source.trimmed().isEmpty() ? sourceId : config.source);
}

// 写入明确的“未应用”结果，供后端未接入期间的 UI 和日志展示。
void PositionCorrection::writeNotAppliedPayload(const PositionCorrectionConfig &config,
                                                QJsonObject *payload)
{
    if (!payload)
        return;

    payload->insert(QStringLiteral("enablePositionCorrection"), config.enabled);
    payload->insert(QStringLiteral("positionCorrectionSourceId"),
                    config.sourceId.trimmed().isEmpty() ? defaultSourceId() : config.sourceId);
    payload->insert(QStringLiteral("positionCorrectionSource"),
                    config.source.trimmed().isEmpty() ? defaultSource() : config.source);
    payload->insert(QStringLiteral("positionCorrectionApplied"), false);
    payload->insert(QStringLiteral("positionCorrectionReason"), notImplementedReason());
}

// 按执行顺序收集基准图及消费工具之前已启用的位置修正实例。
QVector<PositionCorrectionSource> PositionCorrection::sourcesBefore(
        const QVector<ToolConfig> &tools,
        int consumerIndex,
        bool referenceEnabled)
{
    QVector<PositionCorrectionSource> sources;
    if (referenceEnabled) {
        sources.append(PositionCorrectionSource{defaultSourceId(),
                                                defaultSource(),
                                                -1,
                                                true});
    }

    const int limit = qBound(0, consumerIndex, tools.size());
    for (int index = 0; index < limit; ++index) {
        const ToolConfig &tool = tools.at(index);
        if (!tool.enabled || tool.toolType != ToolType::PositionCorrection
                || tool.toolId.trimmed().isEmpty()) {
            continue;
        }

        const QString name = tool.displayName.trimmed().isEmpty()
                ? QStringLiteral("位置修正")
                : tool.displayName.trimmed();
        sources.append(PositionCorrectionSource{
                           tool.toolId,
                           QStringLiteral("%1 %2.位置修正信息").arg(index + 1).arg(name),
                           index,
                           false});
    }
    return sources;
}

// 使用稳定来源 ID 检查已保存引用是否仍然有效，不按显示文本匹配。
bool PositionCorrection::isSourceAvailable(
        const QVector<PositionCorrectionSource> &sources,
        const QString &sourceId)
{
    const QString id = sourceId.trimmed();
    for (const PositionCorrectionSource &source : sources) {
        if (source.sourceId == id)
            return true;
    }
    return false;
}

QVector<PositionPoseField> PositionCorrection::poseFieldsForTool(
        const ToolConfig &tool)
{
    if (!tool.enabled || tool.toolType != ToolType::TemplateLocation)
        return {};

    return QVector<PositionPoseField>{
        {QStringLiteral("x"), QStringLiteral("运行点X"), QStringLiteral("coordinate")},
        {QStringLiteral("y"), QStringLiteral("运行点Y"), QStringLiteral("coordinate")},
        {QStringLiteral("angle"), QStringLiteral("运行角度"), QStringLiteral("angle")}
    };
}

QVector<PositionPoseProducer> PositionCorrection::poseProducersBefore(
        const QVector<ToolConfig> &tools,
        int consumerIndex)
{
    QVector<PositionPoseProducer> producers;
    const int limit = qBound(0, consumerIndex, tools.size());
    for (int index = 0; index < limit; ++index) {
        const ToolConfig &tool = tools.at(index);
        if (tool.toolId.trimmed().isEmpty())
            continue;
        const QVector<PositionPoseField> fields = poseFieldsForTool(tool);
        if (fields.isEmpty())
            continue;
        producers.append(PositionPoseProducer{
                             tool.toolId,
                             displayNameForTool(tool, index),
                             index,
                             fields});
    }
    return producers;
}

PositionRunPoseSource PositionCorrection::runPoseSourceFromConfig(
        const QJsonObject &positionCorrection)
{
    PositionRunPoseSource source;
    const QJsonObject runPose =
            positionCorrection.value(QStringLiteral("runPoseSource")).toObject();
    if (!runPose.isEmpty()) {
        source.producerId = runPose.value(QStringLiteral("producerId")).toString().trimmed();
        source.xKey = stringParam(runPose, QStringLiteral("xKey"), QStringLiteral("x"));
        source.yKey = stringParam(runPose, QStringLiteral("yKey"), QStringLiteral("y"));
        source.angleKey = stringParam(runPose, QStringLiteral("angleKey"), QStringLiteral("angle"));
        source.scaleKey = stringParam(runPose, QStringLiteral("scaleKey"), QStringLiteral("scale"));
        source.displayText = runPose.value(QStringLiteral("displayText")).toString().trimmed();
        source.valid = !source.producerId.isEmpty()
                && !source.xKey.isEmpty()
                && !source.yKey.isEmpty()
                && !source.angleKey.isEmpty()
                && !source.scaleKey.isEmpty();
        source.errorCode = source.valid ? QString() : QStringLiteral("incomplete_input_binding");
        return source;
    }

    const QJsonObject xBinding = oldBinding(positionCorrection, QStringLiteral("runPointX"));
    const QJsonObject yBinding = oldBinding(positionCorrection, QStringLiteral("runPointY"));
    const QJsonObject angleBinding = oldBinding(positionCorrection, QStringLiteral("runAngle"));
    const QString producerId =
            xBinding.value(QStringLiteral("producerId")).toString().trimmed();
    if (producerId.isEmpty()) {
        source.errorCode = QStringLiteral("incomplete_input_binding");
        return source;
    }

    if (!oldBindingMatches(xBinding, producerId, QStringLiteral("x")) ||
            !oldBindingMatches(yBinding, producerId, QStringLiteral("y")) ||
            !oldBindingMatches(angleBinding, producerId, QStringLiteral("angle"))) {
        source.inconsistent = true;
        source.errorCode = QStringLiteral("inconsistent_pose_source");
        return source;
    }

    source.producerId = producerId;
    source.xKey = QStringLiteral("x");
    source.yKey = QStringLiteral("y");
    source.angleKey = QStringLiteral("angle");
    source.scaleKey = QStringLiteral("scale");
    const QString displayPath =
            xBinding.value(QStringLiteral("displayPath")).toString().trimmed();
    const int separator = displayPath.indexOf(QLatin1Char('.'));
    source.displayText = separator > 0 ? displayPath.left(separator) : displayPath;
    source.valid = true;
    return source;
}

void PositionCorrection::writeRunPoseSource(
        const PositionRunPoseSource &source,
        QJsonObject *positionCorrection)
{
    if (!positionCorrection)
        return;

    QJsonObject runPose;
    runPose.insert(QStringLiteral("producerId"), source.producerId);
    runPose.insert(QStringLiteral("xKey"),
                   source.xKey.trimmed().isEmpty() ? QStringLiteral("x") : source.xKey);
    runPose.insert(QStringLiteral("yKey"),
                   source.yKey.trimmed().isEmpty() ? QStringLiteral("y") : source.yKey);
    runPose.insert(QStringLiteral("angleKey"),
                   source.angleKey.trimmed().isEmpty() ? QStringLiteral("angle") : source.angleKey);
    runPose.insert(QStringLiteral("scaleKey"),
                   source.scaleKey.trimmed().isEmpty() ? QStringLiteral("scale") : source.scaleKey);
    if (!source.displayText.trimmed().isEmpty())
        runPose.insert(QStringLiteral("displayText"), source.displayText.trimmed());
    positionCorrection->insert(QStringLiteral("version"), 2);
    positionCorrection->insert(QStringLiteral("runPoseSource"), runPose);

    const QString nodeText = source.displayText.trimmed().isEmpty()
            ? source.producerId
            : source.displayText.trimmed();
    positionCorrection->insert(QStringLiteral("runPointX"),
                               legacyBinding(source.producerId,
                                             QStringLiteral("x"),
                                             QStringLiteral("%1.运行点X").arg(nodeText)));
    positionCorrection->insert(QStringLiteral("runPointY"),
                               legacyBinding(source.producerId,
                                             QStringLiteral("y"),
                                             QStringLiteral("%1.运行点Y").arg(nodeText)));
    positionCorrection->insert(QStringLiteral("runAngle"),
                               legacyBinding(source.producerId,
                                             QStringLiteral("angle"),
                                             QStringLiteral("%1.运行角度").arg(nodeText)));
}

// 解析方案级基准图位置修正配置，缺失字段按向后兼容默认值处理。
ReferencePositionCorrectionConfig PositionCorrection::referenceFromJson(
        const QJsonObject &json)
{
    ReferencePositionCorrectionConfig config;
    config.extra = json;
    config.version = json.value(QStringLiteral("version")).toInt(1);
    config.enabled = json.value(QStringLiteral("enabled")).toBool(false);
    config.locator = json.value(QStringLiteral("locator")).toObject();
    config.referencePosesByTemplateId = json.value(
                QStringLiteral("referencePosesByTemplateId")).toObject();
    config.templateRegionType = stringParam(json,
                                            QStringLiteral("templateRegionType"),
                                            QStringLiteral("rectangle"));
    const QJsonObject roi = json.value(QStringLiteral("templateRoiNormalized")).toObject();
    config.templateRoiNormalized = QRectF(roi.value(QStringLiteral("x")).toDouble(),
                                          roi.value(QStringLiteral("y")).toDouble(),
                                          roi.value(QStringLiteral("width")).toDouble(),
                                          roi.value(QStringLiteral("height")).toDouble());
    config.templatePolygonNormalized =
            json.value(QStringLiteral("templatePolygonNormalized")).toArray();
    config.templateMaskRegionType = stringParam(
                json, QStringLiteral("templateMaskRegionType"),
                json.value(QStringLiteral("templateMaskPolygonNormalized"))
                    .toArray().isEmpty()
                    ? QStringLiteral("none") : QStringLiteral("polygon"));
    const QJsonObject maskRoi =
            json.value(QStringLiteral("templateMaskRoiNormalized")).toObject();
    config.templateMaskRoiNormalized = QRectF(
                maskRoi.value(QStringLiteral("x")).toDouble(),
                maskRoi.value(QStringLiteral("y")).toDouble(),
                maskRoi.value(QStringLiteral("width")).toDouble(),
                maskRoi.value(QStringLiteral("height")).toDouble());
    config.templateMaskPolygonNormalized =
            json.value(QStringLiteral("templateMaskPolygonNormalized")).toArray();
    const QJsonObject maskCircleCenter = json.value(
                QStringLiteral("templateMaskCircleCenterNormalized")).toObject();
    config.templateMaskCircleCenterNormalized = QPointF(
                maskCircleCenter.value(QStringLiteral("x")).toDouble(
                    config.templateMaskRoiNormalized.center().x()),
                maskCircleCenter.value(QStringLiteral("y")).toDouble(
                    config.templateMaskRoiNormalized.center().y()));
    config.templateMaskCircleRadiusNormalized = json.value(
                QStringLiteral("templateMaskCircleRadiusNormalized")).toDouble(
                qMin(config.templateMaskRoiNormalized.width(),
                     config.templateMaskRoiNormalized.height()) / 2.0);
    config.originMode = stringParam(json,
                                    QStringLiteral("originMode"),
                                    QStringLiteral("centroid"));
    const QJsonObject customOrigin =
            json.value(QStringLiteral("customOriginNormalized")).toObject();
    config.customOriginNormalized = QPointF(
                customOrigin.value(QStringLiteral("x")).toDouble(0.5),
                customOrigin.value(QStringLiteral("y")).toDouble(0.5));
    config.referenceCreated = json.value(QStringLiteral("referenceCreated")).toBool(false);
    config.referencePose = json.value(QStringLiteral("referencePose")).toObject();
    config.modelCacheKey = json.value(QStringLiteral("modelCacheKey")).toString().trimmed();
    config.status = json.value(QStringLiteral("status")).toString().trimmed();
    config.message = json.value(QStringLiteral("message")).toString();
    config.score = json.value(QStringLiteral("score")).toDouble(0.0);
    const QJsonValue elapsedValue = json.value(QStringLiteral("elapsedMs"));
    config.elapsedMs = elapsedValue.isString()
            ? elapsedValue.toString().toLongLong()
            : static_cast<qint64>(elapsedValue.toDouble(0.0));
    return config;
}

// 序列化方案级基准图配置，包括归一化模板区域、自匹配模型状态和冻结姿态。
QJsonObject PositionCorrection::referenceToJson(
        const ReferencePositionCorrectionConfig &config)
{
    QJsonObject roi;
    roi.insert(QStringLiteral("x"), config.templateRoiNormalized.x());
    roi.insert(QStringLiteral("y"), config.templateRoiNormalized.y());
    roi.insert(QStringLiteral("width"), config.templateRoiNormalized.width());
    roi.insert(QStringLiteral("height"), config.templateRoiNormalized.height());

    const bool locatorFieldPresent = config.extra.contains(
                QStringLiteral("locator")) || !config.locator.isEmpty();
    bool outerVersionEncodingSupported = true;
    if (config.extra.contains(QStringLiteral("version"))) {
        outerVersionEncodingSupported = exactReferenceBankVersion(
                    config.extra.value(QStringLiteral("version")));
    }
    const TemplateLocationConfig::EnvelopeInspection locatorEnvelope =
            TemplateLocationConfig::inspectEnvelope(config.locator);
    const bool poseMapEncodingSupported =
            !config.extra.contains(QStringLiteral("referencePosesByTemplateId"))
            || config.extra.value(QStringLiteral("referencePosesByTemplateId"))
            .isObject();
    const bool bankEnvelope = (config.version == 4 || config.version == 5)
            && outerVersionEncodingSupported
            && !config.locator.isEmpty()
            && locatorEnvelope.supported
            && poseMapEncodingSupported;

    // A locator field outside the explicitly supported outer/nested version
    // pair is a forward or malformed contract.  It is read-only and must be
    // serialized from its raw envelope rather than falling through the legacy
    // writer, which would otherwise delete the locator or synthesize v3 fields.
    if (locatorFieldPresent && !bankEnvelope) {
        if (!config.extra.isEmpty())
            return config.extra;

        // Programmatically constructed unknown envelopes have no raw object to
        // return.  Preserve every representable bank field without pretending
        // that the contract is executable.
        QJsonObject unknown;
        unknown.insert(QStringLiteral("version"), config.version);
        unknown.insert(QStringLiteral("enabled"), config.enabled);
        if (!config.locator.isEmpty())
            unknown.insert(QStringLiteral("locator"), config.locator);
        if (!config.referencePosesByTemplateId.isEmpty()) {
            unknown.insert(QStringLiteral("referencePosesByTemplateId"),
                           config.referencePosesByTemplateId);
        }
        unknown.insert(QStringLiteral("referenceCreated"),
                       config.referenceCreated);
        unknown.insert(QStringLiteral("referencePose"), config.referencePose);
        if (!config.status.trimmed().isEmpty())
            unknown.insert(QStringLiteral("status"), config.status.trimmed());
        if (!config.message.trimmed().isEmpty())
            unknown.insert(QStringLiteral("message"), config.message);
        unknown.insert(QStringLiteral("score"), config.score);
        unknown.insert(QStringLiteral("elapsedMs"),
                       static_cast<double>(config.elapsedMs));
        return unknown;
    }

    QJsonObject json = config.extra;
    json.insert(QStringLiteral("version"), config.version);
    json.insert(QStringLiteral("enabled"), config.enabled);
    if (bankEnvelope) {
        // A known v4 envelope has exactly one template source: locator.  Do
        // not retain stale v3 mirrors that could later be interpreted instead.
        const QStringList legacyKeys{
            QStringLiteral("templateRegionType"),
            QStringLiteral("templateRoiNormalized"),
            QStringLiteral("templatePolygonNormalized"),
            QStringLiteral("templateMaskRegionType"),
            QStringLiteral("templateMaskRoiNormalized"),
            QStringLiteral("templateMaskPolygonNormalized"),
            QStringLiteral("templateMaskCircleCenterNormalized"),
            QStringLiteral("templateMaskCircleRadiusNormalized"),
            QStringLiteral("originMode"),
            QStringLiteral("customOriginNormalized"),
            QStringLiteral("referencePose"),
            QStringLiteral("modelCacheKey")
        };
        for (const QString &key : legacyKeys)
            json.remove(key);
        json.insert(QStringLiteral("version"), 4);
        json.insert(QStringLiteral("locator"), config.locator);
        json.insert(QStringLiteral("referencePosesByTemplateId"),
                    config.referencePosesByTemplateId);
        json.insert(QStringLiteral("referenceCreated"), config.referenceCreated);
        if (!config.status.trimmed().isEmpty())
            json.insert(QStringLiteral("status"), config.status.trimmed());
        else
            json.remove(QStringLiteral("status"));
        if (!config.message.trimmed().isEmpty())
            json.insert(QStringLiteral("message"), config.message);
        else
            json.remove(QStringLiteral("message"));
        json.insert(QStringLiteral("score"), config.score);
        json.insert(QStringLiteral("elapsedMs"),
                    static_cast<double>(config.elapsedMs));
        return json;
    }

    // Legacy v1..v3 remains byte-compatible and must not accidentally acquire
    // a half-populated bank contract.
    if (config.version <= 3) {
        json.remove(QStringLiteral("locator"));
        json.remove(QStringLiteral("referencePosesByTemplateId"));
    }
    json.insert(QStringLiteral("templateRegionType"),
                config.templateRegionType.trimmed().isEmpty()
                    ? QStringLiteral("rectangle")
                    : config.templateRegionType);
    json.insert(QStringLiteral("templateRoiNormalized"), roi);
    json.insert(QStringLiteral("templatePolygonNormalized"),
                config.templatePolygonNormalized);
    json.insert(QStringLiteral("templateMaskRegionType"),
                config.templateMaskRegionType.trimmed().isEmpty()
                    ? QStringLiteral("none")
                    : config.templateMaskRegionType.trimmed());
    json.insert(QStringLiteral("templateMaskRoiNormalized"),
                QJsonObject{
                    {QStringLiteral("x"), config.templateMaskRoiNormalized.x()},
                    {QStringLiteral("y"), config.templateMaskRoiNormalized.y()},
                    {QStringLiteral("width"),
                     config.templateMaskRoiNormalized.width()},
                    {QStringLiteral("height"),
                     config.templateMaskRoiNormalized.height()}});
    json.insert(QStringLiteral("templateMaskPolygonNormalized"),
                config.templateMaskPolygonNormalized);
    json.insert(QStringLiteral("templateMaskCircleCenterNormalized"),
                QJsonObject{
                    {QStringLiteral("x"),
                     config.templateMaskCircleCenterNormalized.x()},
                    {QStringLiteral("y"),
                     config.templateMaskCircleCenterNormalized.y()}});
    json.insert(QStringLiteral("templateMaskCircleRadiusNormalized"),
                config.templateMaskCircleRadiusNormalized);
    json.insert(QStringLiteral("originMode"),
                config.originMode.trimmed().isEmpty()
                    ? QStringLiteral("centroid")
                    : config.originMode.trimmed());
    json.insert(QStringLiteral("customOriginNormalized"),
                QJsonObject{{QStringLiteral("x"), config.customOriginNormalized.x()},
                            {QStringLiteral("y"), config.customOriginNormalized.y()}});
    json.insert(QStringLiteral("referenceCreated"), config.referenceCreated);
    json.insert(QStringLiteral("referencePose"), config.referencePose);
    if (!config.modelCacheKey.trimmed().isEmpty())
        json.insert(QStringLiteral("modelCacheKey"), config.modelCacheKey.trimmed());
    else
        json.remove(QStringLiteral("modelCacheKey"));
    if (!config.status.trimmed().isEmpty())
        json.insert(QStringLiteral("status"), config.status.trimmed());
    else
        json.remove(QStringLiteral("status"));
    if (!config.message.trimmed().isEmpty())
        json.insert(QStringLiteral("message"), config.message);
    else
        json.remove(QStringLiteral("message"));
    json.insert(QStringLiteral("score"), config.score);
    json.insert(QStringLiteral("elapsedMs"), static_cast<double>(config.elapsedMs));
    return json;
}
