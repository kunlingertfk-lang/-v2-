#include "algorithms/location/TemplateLocationConfig.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QUuid>

#include <cmath>

namespace {

QJsonObject rectToJson(const QRectF &rect)
{
    return QJsonObject{{QStringLiteral("x"), rect.x()},
                       {QStringLiteral("y"), rect.y()},
                       {QStringLiteral("width"), rect.width()},
                       {QStringLiteral("height"), rect.height()}};
}

QRectF rectFromJson(const QJsonObject &value, const QRectF &fallback = QRectF())
{
    if (value.isEmpty())
        return fallback;
    return QRectF(value.value(QStringLiteral("x")).toDouble(fallback.x()),
                  value.value(QStringLiteral("y")).toDouble(fallback.y()),
                  value.value(QStringLiteral("width")).toDouble(fallback.width()),
                  value.value(QStringLiteral("height")).toDouble(fallback.height()));
}

QJsonObject pointToJson(const QPointF &point)
{
    return QJsonObject{{QStringLiteral("x"), point.x()},
                       {QStringLiteral("y"), point.y()}};
}

QPointF pointFromJson(const QJsonObject &value, const QPointF &fallback = QPointF())
{
    if (value.isEmpty())
        return fallback;
    return QPointF(value.value(QStringLiteral("x")).toDouble(fallback.x()),
                   value.value(QStringLiteral("y")).toDouble(fallback.y()));
}

QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray values;
    for (const QPointF &point : points)
        values.append(pointToJson(point));
    return values;
}

QVector<QPointF> pointsFromJson(const QJsonArray &values)
{
    QVector<QPointF> points;
    points.reserve(values.size());
    for (const QJsonValue &value : values)
        points.append(pointFromJson(value.toObject()));
    return points;
}

QString stringValue(const QJsonObject &object,
                    const QString &key,
                    const QString &fallback)
{
    const QString value = object.value(key).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

QString normalizedMaskType(const QJsonObject &object)
{
    const QString configured = object.value(
                QStringLiteral("templateMaskRegionType")).toString().trimmed();
    if (!configured.isEmpty())
        return configured;
    return object.value(QStringLiteral("templateMaskPolygonNormalized"))
            .toArray().isEmpty() ? QStringLiteral("none")
                                 : QStringLiteral("polygon");
}

QString legacyCacheKey(const QString &toolId)
{
    const QString owner = toolId.trimmed().isEmpty()
            ? QStringLiteral("template_location") : toolId.trimmed();
    return QStringLiteral("%1_shape_model").arg(owner);
}

QString itemCacheKey(const QString &toolId, const QString &templateId)
{
    const QString owner = toolId.trimmed().isEmpty()
            ? QStringLiteral("template_location") : toolId.trimmed();
    return QStringLiteral("%1_%2_shape_model").arg(owner, templateId);
}

TemplateLocationTemplateConfig itemFromJson(const QJsonObject &object,
                                            int index,
                                            const QString &toolId,
                                            bool legacy)
{
    TemplateLocationTemplateConfig item;
    item.extra = object;
    item.templateId = object.value(QStringLiteral("templateId"))
            .toString().trimmed();
    if (legacy && item.templateId.isEmpty())
        item.templateId = QStringLiteral("legacy-template");
    item.name = stringValue(object, QStringLiteral("name"),
                            stringValue(object, QStringLiteral("templateName"),
                                        QStringLiteral("模板%1").arg(index + 1)));
    item.enabled = legacy ? true
                          : object.value(QStringLiteral("enabled")).toBool(true);
    item.priority = object.value(QStringLiteral("priority")).toInt(index);
    item.templateRegionType = stringValue(
                object, QStringLiteral("templateRegionType"),
                QStringLiteral("rectangle"));
    item.templateRoiNormalized = rectFromJson(
                object.value(QStringLiteral("templateRoiNormalized")).toObject());
    item.templatePolygonNormalized = pointsFromJson(
                object.value(QStringLiteral("templatePolygonNormalized")).toArray());
    item.templateMaskRegionType = normalizedMaskType(object);
    item.templateMaskRoiNormalized = rectFromJson(
                object.value(QStringLiteral("templateMaskRoiNormalized")).toObject());
    item.templateMaskPolygonNormalized = pointsFromJson(
                object.value(QStringLiteral("templateMaskPolygonNormalized")).toArray());
    item.templateMaskCircleCenterNormalized = pointFromJson(
                object.value(QStringLiteral("templateMaskCircleCenterNormalized"))
                    .toObject(),
                item.templateMaskRoiNormalized.center());
    item.templateMaskCircleRadiusNormalized = object.value(
                QStringLiteral("templateMaskCircleRadiusNormalized")).toDouble(
                qMin(item.templateMaskRoiNormalized.width(),
                     item.templateMaskRoiNormalized.height()) / 2.0);
    item.modelCacheKey = object.value(QStringLiteral("modelCacheKey"))
            .toString().trimmed();
    if (item.modelCacheKey.isEmpty()) {
        item.modelCacheKey = legacy
                ? legacyCacheKey(toolId)
                : (item.templateId.isEmpty()
                   ? QString() : itemCacheKey(toolId, item.templateId));
    }
    item.modelCreated = object.value(QStringLiteral("modelCreated"))
            .toBool(false);
    return item;
}

QJsonObject itemToJson(const TemplateLocationTemplateConfig &item,
                       bool includeBankMetadata)
{
    QJsonObject object = item.extra;
    object.insert(QStringLiteral("templateRegionType"), item.templateRegionType);
    object.insert(QStringLiteral("templateRoiNormalized"),
                  rectToJson(item.templateRoiNormalized));
    object.insert(QStringLiteral("templatePolygonNormalized"),
                  pointsToJson(item.templatePolygonNormalized));
    object.insert(QStringLiteral("templateMaskRegionType"),
                  item.templateMaskRegionType);
    object.insert(QStringLiteral("templateMaskRoiNormalized"),
                  rectToJson(item.templateMaskRoiNormalized));
    object.insert(QStringLiteral("templateMaskPolygonNormalized"),
                  pointsToJson(item.templateMaskPolygonNormalized));
    object.insert(QStringLiteral("templateMaskCircleCenterNormalized"),
                  pointToJson(item.templateMaskCircleCenterNormalized));
    object.insert(QStringLiteral("templateMaskCircleRadiusNormalized"),
                  item.templateMaskCircleRadiusNormalized);
    object.insert(QStringLiteral("modelCacheKey"), item.modelCacheKey);
    object.insert(QStringLiteral("modelCreated"), item.modelCreated);
    if (includeBankMetadata) {
        object.insert(QStringLiteral("templateId"), item.templateId);
        object.insert(QStringLiteral("name"), item.name);
        object.insert(QStringLiteral("enabled"), item.enabled);
        object.insert(QStringLiteral("priority"), item.priority);
        object.remove(QStringLiteral("templateName"));
    } else {
        object.remove(QStringLiteral("templateId"));
        object.remove(QStringLiteral("name"));
        object.remove(QStringLiteral("templateName"));
        object.remove(QStringLiteral("enabled"));
        object.remove(QStringLiteral("priority"));
    }
    return object;
}

void removeBankFieldsFromItem(QJsonObject *item)
{
    if (!item)
        return;
    const QStringList keys{
        QStringLiteral("version"),
        QStringLiteral("halconSoPath"),
        QStringLiteral("templateMode"),
        QStringLiteral("templates"),
        QStringLiteral("searchRegionType"),
        QStringLiteral("searchRoiNormalized"),
        QStringLiteral("searchPolygonNormalized"),
        QStringLiteral("searchCircleCenterNormalized"),
        QStringLiteral("searchCircleRadiusNormalized"),
        QStringLiteral("minScore"),
        QStringLiteral("angleStart"),
        QStringLiteral("angleExtent"),
        QStringLiteral("angleEnd"),
        QStringLiteral("scaleMin"),
        QStringLiteral("scaleMax"),
        QStringLiteral("polarity"),
        QStringLiteral("contrastMode"),
        QStringLiteral("contrast"),
        QStringLiteral("minContrast"),
        QStringLiteral("numLevels"),
        QStringLiteral("subPixel"),
        QStringLiteral("greediness"),
        QStringLiteral("timeoutMs"),
        QStringLiteral("maxMatches"),
        QStringLiteral("minMatchCount"),
        QStringLiteral("maxMatchCount"),
        QStringLiteral("maxOverlap"),
        QStringLiteral("originMode"),
        QStringLiteral("customOriginNormalized"),
        QStringLiteral("primaryMatchStrategy"),
        QStringLiteral("primaryTemplateId"),
        QStringLiteral("fusion")
    };
    for (const QString &key : keys)
        item->remove(key);
}

void removeFlatTemplateFields(QJsonObject *params)
{
    if (!params)
        return;
    const QStringList keys{
        QStringLiteral("templateId"),
        QStringLiteral("templateName"),
        QStringLiteral("templateRegionType"),
        QStringLiteral("templateRoiNormalized"),
        QStringLiteral("templatePolygonNormalized"),
        QStringLiteral("templateMaskRegionType"),
        QStringLiteral("templateMaskRoiNormalized"),
        QStringLiteral("templateMaskPolygonNormalized"),
        QStringLiteral("templateMaskCircleCenterNormalized"),
        QStringLiteral("templateMaskCircleRadiusNormalized"),
        QStringLiteral("modelCacheKey"),
        QStringLiteral("modelCreated")
    };
    for (const QString &key : keys)
        params->remove(key);
}

void writeSharedFields(const TemplateLocationModelBankConfig &config,
                       QJsonObject *params)
{
    if (!params)
        return;
    if (config.halconSoPath.trimmed().isEmpty())
        params->remove(QStringLiteral("halconSoPath"));
    else
        params->insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    params->insert(QStringLiteral("searchRegionType"), config.searchRegionType);
    params->insert(QStringLiteral("searchRoiNormalized"),
                   rectToJson(config.searchRoiNormalized));
    params->insert(QStringLiteral("searchPolygonNormalized"),
                   pointsToJson(config.searchPolygonNormalized));
    params->insert(QStringLiteral("searchCircleCenterNormalized"),
                   pointToJson(config.searchCircleCenterNormalized));
    params->insert(QStringLiteral("searchCircleRadiusNormalized"),
                   config.searchCircleRadiusNormalized);
    params->insert(QStringLiteral("minScore"), config.minScore);
    params->insert(QStringLiteral("angleStart"), config.angleStart);
    params->insert(QStringLiteral("angleExtent"), config.angleExtent);
    params->insert(QStringLiteral("angleEnd"),
                   config.angleStart + config.angleExtent);
    params->insert(QStringLiteral("scaleMin"), config.scaleMin);
    params->insert(QStringLiteral("scaleMax"), config.scaleMax);
    params->insert(QStringLiteral("polarity"), config.polarity);
    params->insert(QStringLiteral("contrastMode"), config.contrastMode);
    params->insert(QStringLiteral("contrast"), config.contrast);
    params->insert(QStringLiteral("minContrast"), config.minContrast);
    params->insert(QStringLiteral("numLevels"), config.numLevels);
    params->insert(QStringLiteral("subPixel"), config.subPixel);
    params->insert(QStringLiteral("greediness"), config.greediness);
    params->insert(QStringLiteral("timeoutMs"), config.timeoutMs);
    params->insert(QStringLiteral("maxMatches"), config.maxMatches);
    params->insert(QStringLiteral("minMatchCount"), config.minMatchCount);
    params->insert(QStringLiteral("maxMatchCount"), config.maxMatchCount);
    params->insert(QStringLiteral("maxOverlap"), config.maxOverlap * 100.0);
    params->insert(QStringLiteral("originMode"), config.originMode);
    params->insert(QStringLiteral("customOriginNormalized"),
                   pointToJson(config.customOriginNormalized));
}

bool finitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool validNormalizedPoint(const QPointF &point)
{
    return finitePoint(point)
            && point.x() >= 0.0 && point.x() <= 1.0
            && point.y() >= 0.0 && point.y() <= 1.0;
}

bool validNormalizedRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y())
            && std::isfinite(rect.width()) && std::isfinite(rect.height())
            && rect.width() > 0.0 && rect.height() > 0.0
            && rect.left() >= 0.0 && rect.top() >= 0.0
            && rect.right() <= 1.000001 && rect.bottom() <= 1.000001;
}

bool validNormalizedPolygon(const QVector<QPointF> &points)
{
    if (points.size() < 3)
        return false;
    for (const QPointF &point : points) {
        if (!validNormalizedPoint(point))
            return false;
    }
    return true;
}

TemplateLocationConfigValidationResult validationError(
        const QString &code,
        const QString &message,
        int templateIndex = -1,
        const QString &templateId = QString())
{
    TemplateLocationConfigValidationResult result;
    result.valid = false;
    result.code = code;
    result.message = message;
    result.templateIndex = templateIndex;
    result.templateId = templateId;
    return result;
}

QString itemDescription(const TemplateLocationTemplateConfig &item, int index)
{
    if (!item.name.trimmed().isEmpty())
        return item.name.trimmed();
    if (!item.templateId.trimmed().isEmpty())
        return item.templateId.trimmed();
    return QStringLiteral("template[%1]").arg(index);
}

void rejectDecode(TemplateLocationModelBankConfig *config,
                  const QString &status,
                  const QString &message)
{
    if (!config)
        return;
    config->decodeSupported = false;
    config->rawPassthrough = true;
    config->decodeStatus = status;
    config->decodeMessage = message;
}

} // namespace

namespace TemplateLocationConfig {

TemplateLocationModelBankConfig fromToolConfig(const ToolConfig &config)
{
    return fromToolParams(config.params, config.toolId);
}

TemplateLocationModelBankConfig fromToolParams(const QJsonObject &params,
                                               const QString &toolId)
{
    TemplateLocationModelBankConfig config;
    config.extraParams = params;
    config.toolId = toolId.trimmed();
    const EnvelopeInspection envelope = inspectEnvelope(params);
    config.version = envelope.version;
    if (!envelope.supported) {
        rejectDecode(&config, envelope.status, envelope.message);
        return config;
    }
    const bool bankContract = config.version == ModelBankParamsVersion;
    config.halconSoPath = params.value(QStringLiteral("halconSoPath"))
            .toString().trimmed();
    config.templateMode = stringValue(
                params, QStringLiteral("templateMode"),
                QStringLiteral("alternatives"));

    config.searchRegionType = stringValue(
                params, QStringLiteral("searchRegionType"),
                QStringLiteral("full"));
    config.searchRoiNormalized = rectFromJson(
                params.value(QStringLiteral("searchRoiNormalized")).toObject(),
                QRectF(0.0, 0.0, 1.0, 1.0));
    config.searchPolygonNormalized = pointsFromJson(
                params.value(QStringLiteral("searchPolygonNormalized")).toArray());
    config.searchCircleCenterNormalized = pointFromJson(
                params.value(QStringLiteral("searchCircleCenterNormalized"))
                    .toObject(),
                config.searchRoiNormalized.center());
    config.searchCircleRadiusNormalized = params.value(
                QStringLiteral("searchCircleRadiusNormalized")).toDouble(
                qMin(config.searchRoiNormalized.width(),
                     config.searchRoiNormalized.height()) / 2.0);
    config.minScore = params.value(QStringLiteral("minScore")).toInt(50);
    config.angleStart = params.value(QStringLiteral("angleStart")).toInt(-45);
    config.angleExtent = params.contains(QStringLiteral("angleExtent"))
            ? params.value(QStringLiteral("angleExtent")).toInt(90)
            : params.value(QStringLiteral("angleEnd")).toInt(45)
              - config.angleStart;
    config.scaleMin = params.value(QStringLiteral("scaleMin")).toInt(100);
    config.scaleMax = params.value(QStringLiteral("scaleMax")).toInt(100);
    config.polarity = stringValue(params, QStringLiteral("polarity"),
                                  QStringLiteral("use_polarity"));
    config.contrastMode = stringValue(params, QStringLiteral("contrastMode"),
                                      QStringLiteral("auto"));
    config.contrast = params.value(QStringLiteral("contrast")).toInt(40);
    config.minContrast = params.value(QStringLiteral("minContrast")).toInt(10);
    config.numLevels = params.value(QStringLiteral("numLevels")).toInt(0);
    config.subPixel = stringValue(params, QStringLiteral("subPixel"),
                                  QStringLiteral("least_squares"));
    config.greediness = params.value(QStringLiteral("greediness")).toDouble(0.5);
    config.timeoutMs = params.value(QStringLiteral("timeoutMs")).toInt(2000);
    config.maxMatches = params.value(QStringLiteral("maxMatches")).toInt(1);
    config.minMatchCount = params.value(QStringLiteral("minMatchCount")).toInt(1);
    config.maxMatchCount = params.value(QStringLiteral("maxMatchCount"))
            .toInt(config.maxMatches);
    config.maxOverlap = params.value(QStringLiteral("maxOverlap"))
            .toDouble(50.0) / 100.0;
    config.originMode = stringValue(params, QStringLiteral("originMode"),
                                    QStringLiteral("centroid"));
    config.customOriginNormalized = pointFromJson(
                params.value(QStringLiteral("customOriginNormalized")).toObject(),
                QPointF(0.5, 0.5));
    config.primaryMatchStrategy = stringValue(
                params, QStringLiteral("primaryMatchStrategy"),
                QStringLiteral("best_score"));
    config.primaryTemplateId = params.value(
                QStringLiteral("primaryTemplateId")).toString().trimmed();

    const QJsonObject fusion = params.value(QStringLiteral("fusion")).toObject();
    config.fusion.enabled = fusion.value(QStringLiteral("enabled")).toBool(true);
    config.fusion.positionTolerancePx = fusion.value(
                QStringLiteral("positionTolerancePx")).toDouble(5.0);
    config.fusion.angleToleranceDeg = fusion.value(
                QStringLiteral("angleToleranceDeg")).toDouble(2.0);
    config.fusion.scaleTolerance = fusion.value(
                QStringLiteral("scaleTolerance")).toDouble(0.05);

    if (bankContract) {
        const QJsonArray templates = params.value(QStringLiteral("templates")).toArray();
        config.templates.reserve(templates.size());
        for (int index = 0; index < templates.size(); ++index) {
            config.templates.append(itemFromJson(
                                        templates.at(index).toObject(),
                                        index, config.toolId, false));
        }
    } else {
        config.templates.append(itemFromJson(params, 0, config.toolId, true));
    }
    return config;
}

QJsonObject toToolParams(const TemplateLocationModelBankConfig &config)
{
    if (!config.decodeSupported || config.rawPassthrough
            || (config.version != LegacyParamsVersion
                && config.version != ModelBankParamsVersion)) {
        return config.extraParams;
    }

    QJsonObject params = config.extraParams;

    const bool bankContract = config.version >= ModelBankParamsVersion
            || config.templates.size() != 1;
    if (!bankContract) {
        const TemplateLocationTemplateConfig item = config.templates.isEmpty()
                ? TemplateLocationTemplateConfig() : config.templates.first();
        const QJsonObject flatItem = itemToJson(item, false);
        for (auto it = flatItem.constBegin(); it != flatItem.constEnd(); ++it)
            params.insert(it.key(), it.value());
        params.insert(QStringLiteral("version"), LegacyParamsVersion);
        params.remove(QStringLiteral("templates"));
        params.remove(QStringLiteral("templateMode"));
        params.remove(QStringLiteral("primaryMatchStrategy"));
        params.remove(QStringLiteral("primaryTemplateId"));
        params.remove(QStringLiteral("fusion"));
        // A legacy item's passthrough object originated at the top level and
        // may still contain stale shared values. Apply the edited bank values
        // last so they remain authoritative.
        writeSharedFields(config, &params);
        return params;
    }

    writeSharedFields(config, &params);
    params.insert(QStringLiteral("version"), ModelBankParamsVersion);
    params.insert(QStringLiteral("templateMode"), config.templateMode);
    removeFlatTemplateFields(&params);
    QJsonArray templates;
    for (const TemplateLocationTemplateConfig &item : config.templates) {
        QJsonObject encodedItem = itemToJson(item, true);
        // When a v4 item is promoted in memory, item.extra still contains the
        // former top-level object. Do not duplicate bank fields into v5 items.
        removeBankFieldsFromItem(&encodedItem);
        templates.append(encodedItem);
    }
    params.insert(QStringLiteral("templates"), templates);
    params.insert(QStringLiteral("primaryMatchStrategy"),
                  config.primaryMatchStrategy);
    params.insert(QStringLiteral("primaryTemplateId"),
                  config.primaryTemplateId);
    params.insert(QStringLiteral("fusion"), QJsonObject{
                      {QStringLiteral("enabled"), config.fusion.enabled},
                      {QStringLiteral("positionTolerancePx"),
                       config.fusion.positionTolerancePx},
                      {QStringLiteral("angleToleranceDeg"),
                       config.fusion.angleToleranceDeg},
                      {QStringLiteral("scaleTolerance"),
                       config.fusion.scaleTolerance}
                  });
    return params;
}

TemplateLocationTemplateConfig templateConfigFromScalar(
        const TemplateLocationHalconConfig &config,
        const QString &templateId,
        const QString &name,
        bool enabled,
        int priority)
{
    TemplateLocationTemplateConfig item;
    item.templateId = templateId.trimmed().isEmpty()
            ? QStringLiteral("legacy-template") : templateId.trimmed();
    item.name = name.trimmed().isEmpty() ? QStringLiteral("模板1") : name.trimmed();
    item.enabled = enabled;
    item.priority = priority;
    item.templateRegionType = config.templateRegionType;
    item.templateRoiNormalized = config.templateRoiNormalized;
    item.templatePolygonNormalized = config.templatePolygonNormalized;
    item.templateMaskRegionType = config.templateMaskRegionType;
    if ((item.templateMaskRegionType.trimmed().isEmpty() ||
         item.templateMaskRegionType == QStringLiteral("none")) &&
            !config.templateMaskPolygonNormalized.isEmpty()) {
        item.templateMaskRegionType = QStringLiteral("polygon");
    }
    item.templateMaskRoiNormalized = config.templateMaskRoiNormalized;
    item.templateMaskPolygonNormalized = config.templateMaskPolygonNormalized;
    item.templateMaskCircleCenterNormalized =
            config.templateMaskCircleCenterNormalized;
    item.templateMaskCircleRadiusNormalized =
            config.templateMaskCircleRadiusNormalized;
    item.modelCacheKey = config.modelCacheKey.trimmed().isEmpty()
            ? legacyCacheKey(config.toolId) : config.modelCacheKey.trimmed();
    // A scalar runner config has no readiness flag. It is an executable config,
    // so wrapping it as a bank marks the model logically available.
    item.modelCreated = true;
    return item;
}

TemplateLocationModelBankConfig modelBankFromScalar(
        const TemplateLocationHalconConfig &config,
        const QString &templateId,
        const QString &name)
{
    TemplateLocationModelBankConfig bank;
    bank.version = LegacyParamsVersion;
    bank.toolId = config.toolId;
    bank.halconSoPath = config.halconSoPath;
    bank.halconSoPathCandidates = config.halconSoPathCandidates;
    bank.templates.append(templateConfigFromScalar(config, templateId, name));
    bank.searchRegionType = config.searchRegionType;
    bank.searchRoiNormalized = config.searchRoiNormalized;
    bank.searchPolygonNormalized = config.searchPolygonNormalized;
    bank.searchCircleCenterNormalized = config.searchCircleCenterNormalized;
    bank.searchCircleRadiusNormalized = config.searchCircleRadiusNormalized;
    bank.minScore = config.minScore;
    bank.angleStart = config.angleStart;
    bank.angleExtent = config.angleExtent;
    bank.scaleMin = config.scaleMin;
    bank.scaleMax = config.scaleMax;
    bank.polarity = config.polarity;
    bank.contrastMode = config.contrastMode;
    bank.contrast = config.contrast;
    bank.minContrast = config.minContrast;
    bank.numLevels = config.numLevels;
    bank.subPixel = config.subPixel;
    bank.greediness = config.greediness;
    bank.timeoutMs = config.timeoutMs;
    bank.maxMatches = config.maxMatches;
    bank.minMatchCount = config.minMatchCount;
    bank.maxMatchCount = config.maxMatchCount;
    bank.maxOverlap = config.maxOverlap;
    bank.originMode = config.originMode;
    bank.customOriginNormalized = config.customOriginNormalized;
    return bank;
}

TemplateLocationHalconConfig scalarConfigForTemplate(
        const TemplateLocationModelBankConfig &bank,
        const TemplateLocationTemplateConfig &item)
{
    TemplateLocationHalconConfig config;
    config.toolId = bank.toolId;
    config.halconSoPath = bank.halconSoPath;
    config.halconSoPathCandidates = bank.halconSoPathCandidates;
    config.modelCacheKey = item.modelCacheKey;
    config.templateRegionType = item.templateRegionType;
    config.templateRoiNormalized = item.templateRoiNormalized;
    config.templatePolygonNormalized = item.templatePolygonNormalized;
    config.templateMaskRegionType = item.templateMaskRegionType;
    config.templateMaskRoiNormalized = item.templateMaskRoiNormalized;
    config.templateMaskPolygonNormalized = item.templateMaskPolygonNormalized;
    config.templateMaskCircleCenterNormalized =
            item.templateMaskCircleCenterNormalized;
    config.templateMaskCircleRadiusNormalized =
            item.templateMaskCircleRadiusNormalized;
    config.searchRegionType = bank.searchRegionType;
    config.searchRoiNormalized = bank.searchRoiNormalized;
    config.searchPolygonNormalized = bank.searchPolygonNormalized;
    config.searchCircleCenterNormalized = bank.searchCircleCenterNormalized;
    config.searchCircleRadiusNormalized = bank.searchCircleRadiusNormalized;
    config.minScore = bank.minScore;
    config.angleStart = bank.angleStart;
    config.angleExtent = bank.angleExtent;
    config.scaleMin = bank.scaleMin;
    config.scaleMax = bank.scaleMax;
    config.polarity = bank.polarity;
    config.contrastMode = bank.contrastMode;
    config.contrast = bank.contrast;
    config.minContrast = bank.minContrast;
    config.numLevels = bank.numLevels;
    config.subPixel = bank.subPixel;
    config.greediness = bank.greediness;
    config.timeoutMs = bank.timeoutMs;
    config.maxMatches = bank.maxMatches;
    config.minMatchCount = bank.minMatchCount;
    config.maxMatchCount = bank.maxMatchCount;
    config.maxOverlap = bank.maxOverlap;
    config.originMode = bank.originMode;
    config.customOriginNormalized = bank.customOriginNormalized;
    return config;
}

void ensureStableTemplateIds(TemplateLocationModelBankConfig *config)
{
    if (!config || !config->decodeSupported || config->rawPassthrough)
        return;
    QSet<QString> used;
    QSet<QString> usedCacheKeys;
    for (int index = 0; index < config->templates.size(); ++index) {
        TemplateLocationTemplateConfig &item = config->templates[index];
        QString id = item.templateId.trimmed();
        if (config->version < ModelBankParamsVersion && index == 0
                && (id.isEmpty() || used.contains(id))) {
            id = QStringLiteral("legacy-template");
        }
        while (id.isEmpty() || used.contains(id)) {
            id = QStringLiteral("tpl-%1").arg(
                        QUuid::createUuid().toString(QUuid::WithoutBraces));
        }
        item.templateId = id;
        used.insert(id);
        if (item.name.trimmed().isEmpty())
            item.name = QStringLiteral("模板%1").arg(index + 1);
        QString cacheKey = item.modelCacheKey.trimmed();
        if (cacheKey.isEmpty() || usedCacheKeys.contains(cacheKey)) {
            cacheKey = itemCacheKey(config->toolId, id);
            while (usedCacheKeys.contains(cacheKey)) {
                cacheKey = itemCacheKey(
                            config->toolId,
                            QStringLiteral("%1-%2").arg(
                                id,
                                QUuid::createUuid().toString(
                                    QUuid::WithoutBraces)));
            }
        }
        item.modelCacheKey = cacheKey;
        usedCacheKeys.insert(cacheKey);
    }
}

TemplateLocationModelBankConfig withStableTemplateIds(
        const TemplateLocationModelBankConfig &config)
{
    TemplateLocationModelBankConfig result = config;
    ensureStableTemplateIds(&result);
    return result;
}

int enabledTemplateCount(const TemplateLocationModelBankConfig &config)
{
    int count = 0;
    for (const TemplateLocationTemplateConfig &item : config.templates) {
        if (item.enabled)
            ++count;
    }
    return count;
}

bool allEnabledModelsReady(const TemplateLocationModelBankConfig &config,
                           QStringList *notReadyTemplateIds)
{
    if (notReadyTemplateIds)
        notReadyTemplateIds->clear();
    bool hasEnabled = false;
    bool ready = true;
    for (const TemplateLocationTemplateConfig &item : config.templates) {
        if (!item.enabled)
            continue;
        hasEnabled = true;
        if (item.modelCreated)
            continue;
        ready = false;
        if (notReadyTemplateIds) {
            notReadyTemplateIds->append(item.templateId.trimmed().isEmpty()
                                        ? item.name : item.templateId);
        }
    }
    return hasEnabled && ready;
}

TemplateLocationConfigValidationResult validateModelBank(
        const TemplateLocationModelBankConfig &config,
        bool requireReadyModels)
{
    if (!config.decodeSupported || config.rawPassthrough) {
        return validationError(
                    config.decodeStatus.trimmed().isEmpty()
                    ? QStringLiteral("invalid_config")
                    : config.decodeStatus,
                    config.decodeMessage.trimmed().isEmpty()
                    ? QStringLiteral("Template location configuration cannot be decoded safely.")
                    : config.decodeMessage);
    }
    if (config.version != LegacyParamsVersion
            && config.version != ModelBankParamsVersion) {
        return validationError(
                    QStringLiteral("unsupported_config_version"),
                    QStringLiteral("Template location supports configuration versions 4 and 5."));
    }
    if (config.templateMode != QStringLiteral("alternatives")) {
        return validationError(
                    QStringLiteral("unsupported_template_mode"),
                    QStringLiteral("Only the alternatives template mode is supported."));
    }
    if (config.templates.isEmpty()) {
        return validationError(QStringLiteral("no_templates"),
                               QStringLiteral("The template bank is empty."));
    }
    if (config.templates.size() > MaximumTemplateCount) {
        return validationError(
                    QStringLiteral("too_many_templates"),
                    QStringLiteral("A template bank supports at most %1 templates.")
                    .arg(MaximumTemplateCount));
    }
    if (config.version == LegacyParamsVersion && config.templates.size() != 1) {
        return validationError(
                    QStringLiteral("legacy_multiple_templates"),
                    QStringLiteral("The version 4 flat contract supports exactly one template."));
    }

    QSet<QString> templateIds;
    QSet<QString> modelCacheKeys;
    int enabledCount = 0;
    for (int index = 0; index < config.templates.size(); ++index) {
        const TemplateLocationTemplateConfig &item = config.templates.at(index);
        const QString id = item.templateId.trimmed();
        const QString description = itemDescription(item, index);
        if (id.isEmpty()) {
            return validationError(
                        QStringLiteral("missing_template_id"),
                        QStringLiteral("%1 has no stable template ID.").arg(description),
                        index, id);
        }
        if (templateIds.contains(id)) {
            return validationError(
                        QStringLiteral("duplicate_template_id"),
                        QStringLiteral("Template ID '%1' is duplicated.").arg(id),
                        index, id);
        }
        templateIds.insert(id);
        if (item.modelCacheKey.trimmed().isEmpty()) {
            return validationError(
                        QStringLiteral("missing_model_cache_key"),
                        QStringLiteral("%1 has no model cache identity.").arg(description),
                        index, id);
        }
        if (modelCacheKeys.contains(item.modelCacheKey.trimmed())) {
            return validationError(
                        QStringLiteral("duplicate_model_cache_key"),
                        QStringLiteral("%1 shares a model cache identity with another template.")
                        .arg(description), index, id);
        }
        modelCacheKeys.insert(item.modelCacheKey.trimmed());
        // Disabled alternatives remain persistable/editable but are not part
        // of the executable bank. Their unfinished geometry must not block
        // enabled templates from running.
        if (!item.enabled)
            continue;
        const QString regionType = item.templateRegionType.trimmed().toLower();
        if (regionType != QStringLiteral("rectangle")
                && regionType != QStringLiteral("polygon")) {
            return validationError(
                        QStringLiteral("invalid_template_region"),
                        QStringLiteral("%1 has an unsupported template region type.")
                        .arg(description), index, id);
        }
        if (!validNormalizedRect(item.templateRoiNormalized)
                || (regionType == QStringLiteral("polygon")
                    && !validNormalizedPolygon(item.templatePolygonNormalized))) {
            return validationError(
                        QStringLiteral("invalid_template_region"),
                        QStringLiteral("%1 has invalid template geometry.")
                        .arg(description), index, id);
        }
        const QString maskType = item.templateMaskRegionType.trimmed().toLower();
        if (maskType != QStringLiteral("none")
                && maskType != QStringLiteral("rectangle")
                && maskType != QStringLiteral("circle")
                && maskType != QStringLiteral("polygon")) {
            return validationError(
                        QStringLiteral("invalid_template_mask"),
                        QStringLiteral("%1 has an unsupported template mask type.")
                        .arg(description), index, id);
        }
        const bool validMask = maskType == QStringLiteral("none")
                || (maskType == QStringLiteral("rectangle")
                    && validNormalizedRect(item.templateMaskRoiNormalized))
                || (maskType == QStringLiteral("polygon")
                    && validNormalizedPolygon(item.templateMaskPolygonNormalized))
                || (maskType == QStringLiteral("circle")
                    && validNormalizedPoint(item.templateMaskCircleCenterNormalized)
                    && std::isfinite(item.templateMaskCircleRadiusNormalized)
                    && item.templateMaskCircleRadiusNormalized > 0.0
                    && item.templateMaskCircleRadiusNormalized <= 1.0);
        if (!validMask) {
            return validationError(
                        QStringLiteral("invalid_template_mask"),
                        QStringLiteral("%1 has invalid template mask geometry.")
                        .arg(description), index, id);
        }
        ++enabledCount;
        if (requireReadyModels && !item.modelCreated) {
            return validationError(
                        QStringLiteral("model_not_created"),
                        QStringLiteral("%1 must be built before execution.")
                        .arg(description), index, id);
        }
    }
    if (enabledCount == 0) {
        return validationError(QStringLiteral("no_enabled_templates"),
                               QStringLiteral("The template bank has no enabled templates."));
    }

    const QString searchType = config.searchRegionType.trimmed().toLower();
    if (searchType != QStringLiteral("full")
            && searchType != QStringLiteral("rectangle")
            && searchType != QStringLiteral("circle")
            && searchType != QStringLiteral("polygon")) {
        return validationError(QStringLiteral("invalid_search_region"),
                               QStringLiteral("The search region type is unsupported."));
    }
    if (!validNormalizedRect(config.searchRoiNormalized)
            || (searchType == QStringLiteral("polygon")
                && !validNormalizedPolygon(config.searchPolygonNormalized))
            || (searchType == QStringLiteral("circle")
                && (!validNormalizedPoint(config.searchCircleCenterNormalized)
                    || !std::isfinite(config.searchCircleRadiusNormalized)
                    || config.searchCircleRadiusNormalized <= 0.0
                    || config.searchCircleRadiusNormalized > 1.0))) {
        return validationError(QStringLiteral("invalid_search_region"),
                               QStringLiteral("The search region geometry is invalid."));
    }

    const QSet<QString> polarities{
        QStringLiteral("use_polarity"),
        QStringLiteral("ignore_local_polarity"),
        QStringLiteral("ignore_global_polarity")
    };
    const QSet<QString> subPixelModes{
        QStringLiteral("least_squares"), QStringLiteral("none")
    };
    if (config.minScore < 0 || config.minScore > 100
            || config.angleStart < -180 || config.angleStart > 180
            || config.angleExtent < 0 || config.angleExtent > 360
            || config.scaleMin < 10 || config.scaleMax > 200
            || config.scaleMin > config.scaleMax
            || !polarities.contains(config.polarity)
            || (config.contrastMode != QStringLiteral("auto")
                && config.contrastMode != QStringLiteral("manual"))
            || config.contrast < 2 || config.contrast > 255
            || config.minContrast < 1 || config.minContrast > 254
            || (config.contrastMode == QStringLiteral("manual")
                && config.minContrast >= config.contrast)
            || config.numLevels < 0 || config.numLevels > 10
            || !subPixelModes.contains(config.subPixel)
            || !std::isfinite(config.greediness)
            || config.greediness < 0.0 || config.greediness > 1.0
            || config.timeoutMs < 0
            || config.maxMatches < 1 || config.maxMatches > 100
            || config.minMatchCount < 1
            || config.maxMatchCount < config.minMatchCount
            || config.maxMatchCount > config.maxMatches
            || !std::isfinite(config.maxOverlap)
            || config.maxOverlap < 0.0 || config.maxOverlap > 1.0) {
        return validationError(QStringLiteral("invalid_parameter"),
                               QStringLiteral("Template location parameter range is invalid."));
    }
    if (config.originMode != QStringLiteral("centroid")
            && config.originMode != QStringLiteral("custom")) {
        return validationError(QStringLiteral("invalid_origin"),
                               QStringLiteral("The output origin mode is invalid."));
    }
    if (config.originMode == QStringLiteral("custom")
            && !validNormalizedPoint(config.customOriginNormalized)) {
        return validationError(QStringLiteral("invalid_origin"),
                               QStringLiteral("The custom output origin is invalid."));
    }

    const QSet<QString> strategies{
        QStringLiteral("best_score"),
        QStringLiteral("template_priority"),
        QStringLiteral("locked_template")
    };
    if (!strategies.contains(config.primaryMatchStrategy)) {
        return validationError(QStringLiteral("invalid_primary_match_strategy"),
                               QStringLiteral("The primary match strategy is invalid."));
    }
    if (config.primaryMatchStrategy == QStringLiteral("locked_template")) {
        const TemplateLocationTemplateConfig *locked = findTemplate(
                    config, config.primaryTemplateId);
        if (!locked || !locked->enabled) {
            return validationError(QStringLiteral("invalid_primary_template"),
                                   QStringLiteral("The locked template is missing or disabled."));
        }
    }
    if (!std::isfinite(config.fusion.positionTolerancePx)
            || config.fusion.positionTolerancePx < 0.0
            || !std::isfinite(config.fusion.angleToleranceDeg)
            || config.fusion.angleToleranceDeg < 0.0
            || config.fusion.angleToleranceDeg > 360.0
            || !std::isfinite(config.fusion.scaleTolerance)
            || config.fusion.scaleTolerance < 0.0) {
        return validationError(QStringLiteral("invalid_fusion_parameter"),
                               QStringLiteral("Template fusion tolerance is invalid."));
    }
    return TemplateLocationConfigValidationResult();
}

const TemplateLocationTemplateConfig *findTemplate(
        const TemplateLocationModelBankConfig &config,
        const QString &templateId)
{
    const QString id = templateId.trimmed();
    if (id.isEmpty())
        return nullptr;
    for (const TemplateLocationTemplateConfig &item : config.templates) {
        if (item.templateId == id)
            return &item;
    }
    return nullptr;
}

TemplateLocationTemplateConfig *findTemplate(
        TemplateLocationModelBankConfig *config,
        const QString &templateId)
{
    if (!config)
        return nullptr;
    const QString id = templateId.trimmed();
    if (id.isEmpty())
        return nullptr;
    for (TemplateLocationTemplateConfig &item : config->templates) {
        if (item.templateId == id)
            return &item;
    }
    return nullptr;
}

} // namespace TemplateLocationConfig
