#include "algorithms/location/TemplateLocationConfig.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QUuid>

#include <algorithm>
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

TemplateLocationRegionConfig regionFromJson(const QJsonObject &object)
{
    TemplateLocationRegionConfig region;
    region.extra = object;
    region.regionId = object.value(QStringLiteral("regionId"))
            .toString().trimmed();
    region.regionType = stringValue(
                object, QStringLiteral("regionType"),
                QStringLiteral("rectangle")).toLower();
    region.roiNormalized = rectFromJson(
                object.value(QStringLiteral("roiNormalized")).toObject());
    region.polygonNormalized = pointsFromJson(
                object.value(QStringLiteral("polygonNormalized")).toArray());
    region.circleCenterNormalized = pointFromJson(
                object.value(QStringLiteral("circleCenterNormalized")).toObject(),
                region.roiNormalized.center());
    region.circleRadiusNormalized = object.value(
                QStringLiteral("circleRadiusNormalized")).toDouble(
                qMin(region.roiNormalized.width(),
                     region.roiNormalized.height()) / 2.0);
    return region;
}

QJsonObject regionToJson(const TemplateLocationRegionConfig &region)
{
    QJsonObject object = region.extra;
    object.insert(QStringLiteral("regionId"), region.regionId);
    object.insert(QStringLiteral("regionType"), region.regionType);
    object.insert(QStringLiteral("roiNormalized"),
                  rectToJson(region.roiNormalized));
    object.insert(QStringLiteral("polygonNormalized"),
                  pointsToJson(region.polygonNormalized));
    object.insert(QStringLiteral("circleCenterNormalized"),
                  pointToJson(region.circleCenterNormalized));
    object.insert(QStringLiteral("circleRadiusNormalized"),
                  region.circleRadiusNormalized);
    return object;
}

QJsonArray regionsToJson(const QVector<TemplateLocationRegionConfig> &regions)
{
    QJsonArray array;
    for (const TemplateLocationRegionConfig &region : regions)
        array.append(regionToJson(region));
    return array;
}

QVector<TemplateLocationRegionConfig> regionsFromJson(const QJsonArray &array)
{
    QVector<TemplateLocationRegionConfig> regions;
    regions.reserve(array.size());
    for (const QJsonValue &value : array)
        regions.append(regionFromJson(value.toObject()));
    return regions;
}

const QStringList &matchParameterKeys()
{
    static const QStringList keys{
        QStringLiteral("minScore"),
        QStringLiteral("angleStart"),
        QStringLiteral("angleExtent"),
        QStringLiteral("scaleMin"),
        QStringLiteral("scaleMax"),
        QStringLiteral("polarity"),
        QStringLiteral("contrastMode"),
        QStringLiteral("contrast"),
        QStringLiteral("minContrast"),
        QStringLiteral("numLevels"),
        QStringLiteral("subPixel"),
        QStringLiteral("greediness"),
        QStringLiteral("maxOverlap")
    };
    return keys;
}

TemplateLocationMatchParameters matchParametersFromJson(
        const QJsonObject &object,
        const TemplateLocationMatchParameters &fallback,
        bool *complete = nullptr)
{
    TemplateLocationMatchParameters parameters = fallback;
    parameters.extra = object;
    if (complete) {
        *complete = true;
        for (const QString &key : matchParameterKeys()) {
            if (!object.contains(key)) {
                *complete = false;
                break;
            }
        }
    }
    parameters.minScore = object.value(QStringLiteral("minScore"))
            .toInt(fallback.minScore);
    parameters.angleStart = object.value(QStringLiteral("angleStart"))
            .toInt(fallback.angleStart);
    parameters.angleExtent = object.value(QStringLiteral("angleExtent"))
            .toInt(fallback.angleExtent);
    parameters.scaleMin = object.value(QStringLiteral("scaleMin"))
            .toInt(fallback.scaleMin);
    parameters.scaleMax = object.value(QStringLiteral("scaleMax"))
            .toInt(fallback.scaleMax);
    parameters.polarity = stringValue(
                object, QStringLiteral("polarity"), fallback.polarity);
    parameters.contrastMode = stringValue(
                object, QStringLiteral("contrastMode"), fallback.contrastMode);
    parameters.contrast = object.value(QStringLiteral("contrast"))
            .toInt(fallback.contrast);
    parameters.minContrast = object.value(QStringLiteral("minContrast"))
            .toInt(fallback.minContrast);
    parameters.numLevels = object.value(QStringLiteral("numLevels"))
            .toInt(fallback.numLevels);
    parameters.subPixel = stringValue(
                object, QStringLiteral("subPixel"), fallback.subPixel);
    parameters.greediness = object.value(QStringLiteral("greediness"))
            .toDouble(fallback.greediness);
    parameters.maxOverlap = object.value(QStringLiteral("maxOverlap"))
            .toDouble(fallback.maxOverlap * 100.0) / 100.0;
    return parameters;
}

QJsonObject matchParametersToJson(
        const TemplateLocationMatchParameters &parameters)
{
    QJsonObject object = parameters.extra;
    object.insert(QStringLiteral("minScore"), parameters.minScore);
    object.insert(QStringLiteral("angleStart"), parameters.angleStart);
    object.insert(QStringLiteral("angleExtent"), parameters.angleExtent);
    object.insert(QStringLiteral("scaleMin"), parameters.scaleMin);
    object.insert(QStringLiteral("scaleMax"), parameters.scaleMax);
    object.insert(QStringLiteral("polarity"), parameters.polarity);
    object.insert(QStringLiteral("contrastMode"), parameters.contrastMode);
    object.insert(QStringLiteral("contrast"), parameters.contrast);
    object.insert(QStringLiteral("minContrast"), parameters.minContrast);
    object.insert(QStringLiteral("numLevels"), parameters.numLevels);
    object.insert(QStringLiteral("subPixel"), parameters.subPixel);
    object.insert(QStringLiteral("greediness"), parameters.greediness);
    object.insert(QStringLiteral("maxOverlap"), parameters.maxOverlap * 100.0);
    return object;
}

void deriveLegacyRegionMirrors(TemplateLocationTemplateConfig *item)
{
    if (!item || item->includeRegions.isEmpty())
        return;
    const TemplateLocationRegionConfig &include = item->includeRegions.first();
    item->templateRegionType = include.regionType;
    item->templateRoiNormalized = include.roiNormalized;
    item->templatePolygonNormalized = include.polygonNormalized;
    if (include.regionType == QStringLiteral("circle")) {
        const double radius = include.circleRadiusNormalized;
        item->templateRegionType = QStringLiteral("rectangle");
        item->templateRoiNormalized = QRectF(
                    include.circleCenterNormalized.x() - radius,
                    include.circleCenterNormalized.y() - radius,
                    radius * 2.0, radius * 2.0);
        item->templatePolygonNormalized.clear();
    }

    item->templateMaskRegionType = QStringLiteral("none");
    item->templateMaskRoiNormalized = QRectF();
    item->templateMaskPolygonNormalized.clear();
    item->templateMaskCircleCenterNormalized = QPointF();
    item->templateMaskCircleRadiusNormalized = 0.0;
    if (item->excludeRegions.isEmpty())
        return;
    const TemplateLocationRegionConfig &exclude = item->excludeRegions.first();
    item->templateMaskRegionType = exclude.regionType;
    item->templateMaskRoiNormalized = exclude.roiNormalized;
    item->templateMaskPolygonNormalized = exclude.polygonNormalized;
    item->templateMaskCircleCenterNormalized = exclude.circleCenterNormalized;
    item->templateMaskCircleRadiusNormalized = exclude.circleRadiusNormalized;
}

TemplateLocationBaseBindingConfig baseBindingFromJson(
        const QJsonObject &object,
        int index)
{
    TemplateLocationBaseBindingConfig binding;
    binding.extra = object;
    binding.baseId = object.value(QStringLiteral("baseId"))
            .toString().trimmed();
    binding.order = object.value(QStringLiteral("order")).toInt(index);
    binding.searchRegionType = stringValue(
                object, QStringLiteral("searchRegionType"),
                QStringLiteral("full")).toLower();
    binding.searchRoiNormalized = rectFromJson(
                object.value(QStringLiteral("searchRoiNormalized")).toObject(),
                QRectF(0.0, 0.0, 1.0, 1.0));
    binding.searchPolygonNormalized = pointsFromJson(
                object.value(QStringLiteral("searchPolygonNormalized")).toArray());
    binding.searchCircleCenterNormalized = pointFromJson(
                object.value(QStringLiteral("searchCircleCenterNormalized"))
                    .toObject(),
                binding.searchRoiNormalized.center());
    binding.searchCircleRadiusNormalized = object.value(
                QStringLiteral("searchCircleRadiusNormalized")).toDouble(
                qMin(binding.searchRoiNormalized.width(),
                     binding.searchRoiNormalized.height()) / 2.0);
    binding.originMode = stringValue(
                object, QStringLiteral("originMode"),
                QStringLiteral("centroid")).toLower();
    binding.customOriginNormalized = pointFromJson(
                object.value(QStringLiteral("customOriginNormalized")).toObject(),
                QPointF(0.5, 0.5));
    return binding;
}

QJsonObject baseBindingToJson(const TemplateLocationBaseBindingConfig &binding)
{
    QJsonObject object = binding.extra;
    object.insert(QStringLiteral("baseId"), binding.baseId);
    object.insert(QStringLiteral("order"), binding.order);
    object.insert(QStringLiteral("searchRegionType"), binding.searchRegionType);
    object.insert(QStringLiteral("searchRoiNormalized"),
                  rectToJson(binding.searchRoiNormalized));
    object.insert(QStringLiteral("searchPolygonNormalized"),
                  pointsToJson(binding.searchPolygonNormalized));
    object.insert(QStringLiteral("searchCircleCenterNormalized"),
                  pointToJson(binding.searchCircleCenterNormalized));
    object.insert(QStringLiteral("searchCircleRadiusNormalized"),
                  binding.searchCircleRadiusNormalized);
    object.insert(QStringLiteral("originMode"), binding.originMode);
    object.insert(QStringLiteral("customOriginNormalized"),
                  pointToJson(binding.customOriginNormalized));
    return object;
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

TemplateLocationTemplateConfig v6ItemFromJson(
        const QJsonObject &object,
        int index,
        const QString &toolId,
        const TemplateLocationMatchParameters &sharedParameters)
{
    TemplateLocationTemplateConfig item;
    item.extra = object;
    item.templateId = object.value(QStringLiteral("templateId"))
            .toString().trimmed();
    item.name = stringValue(object, QStringLiteral("name"),
                            QStringLiteral("模板%1").arg(index + 1));
    item.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    item.order = object.value(QStringLiteral("order")).toInt(index);
    // Keep the legacy ordering mirror usable until every consumer switches to
    // orderedTemplateRefs().
    item.priority = item.order;
    item.sourceBaseId = object.value(QStringLiteral("sourceBaseId"))
            .toString().trimmed();
    item.includeRegions = regionsFromJson(
                object.value(QStringLiteral("includeRegions")).toArray());
    item.excludeRegions = regionsFromJson(
                object.value(QStringLiteral("excludeRegions")).toArray());
    item.useIndependentParameters = object.value(
                QStringLiteral("useIndependentParameters")).toBool(false);
    const QJsonObject overrides = object.value(
                QStringLiteral("parameterOverrides")).toObject();
    item.independentParameters = matchParametersFromJson(
                overrides, sharedParameters,
                &item.independentParametersComplete);
    if (!item.useIndependentParameters)
        item.independentParametersComplete = true;
    item.modelCacheKey = object.value(QStringLiteral("modelCacheKey"))
            .toString().trimmed();
    if (item.modelCacheKey.isEmpty() && !item.templateId.isEmpty())
        item.modelCacheKey = itemCacheKey(toolId, item.templateId);
    item.modelCreated = object.value(QStringLiteral("modelCreated"))
            .toBool(false);
    deriveLegacyRegionMirrors(&item);
    return item;
}

QJsonObject v6ItemToJson(const TemplateLocationTemplateConfig &item)
{
    QJsonObject object = item.extra;
    // v6 composite regions are the sole geometry source. Do not perpetuate
    // stale v4/v5 mirrors if an intermediate editor happened to write them.
    const QStringList staleGeometryKeys{
        QStringLiteral("templateRegionType"),
        QStringLiteral("templateRoiNormalized"),
        QStringLiteral("templatePolygonNormalized"),
        QStringLiteral("templateMaskRegionType"),
        QStringLiteral("templateMaskRoiNormalized"),
        QStringLiteral("templateMaskPolygonNormalized"),
        QStringLiteral("templateMaskCircleCenterNormalized"),
        QStringLiteral("templateMaskCircleRadiusNormalized"),
        QStringLiteral("priority"),
        QStringLiteral("templateName")
    };
    for (const QString &key : staleGeometryKeys)
        object.remove(key);
    object.insert(QStringLiteral("templateId"), item.templateId);
    object.insert(QStringLiteral("name"), item.name);
    object.insert(QStringLiteral("enabled"), item.enabled);
    object.insert(QStringLiteral("order"), item.order);
    object.insert(QStringLiteral("sourceBaseId"), item.sourceBaseId);
    object.insert(QStringLiteral("includeRegions"),
                  regionsToJson(item.includeRegions));
    object.insert(QStringLiteral("excludeRegions"),
                  regionsToJson(item.excludeRegions));
    object.insert(QStringLiteral("useIndependentParameters"),
                  item.useIndependentParameters);
    if (item.useIndependentParameters) {
        object.insert(QStringLiteral("parameterOverrides"),
                      matchParametersToJson(item.independentParameters));
    } else if (item.independentParameters.extra.isEmpty()) {
        object.remove(QStringLiteral("parameterOverrides"));
    } else {
        // Preserve extension fields even while inheritance is selected.
        object.insert(QStringLiteral("parameterOverrides"),
                      item.independentParameters.extra);
    }
    object.insert(QStringLiteral("modelCacheKey"), item.modelCacheKey);
    object.insert(QStringLiteral("modelCreated"), item.modelCreated);
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

void writeV6SharedFields(const TemplateLocationModelBankConfig &config,
                         QJsonObject *params)
{
    if (!params)
        return;
    writeSharedFields(config, params);
    // In v6 search geometry and public output origins are owned exclusively by
    // baseBindings. These legacy mirrors must not become a second authority.
    const QStringList bindingOwnedKeys{
        QStringLiteral("searchRegionType"),
        QStringLiteral("searchRoiNormalized"),
        QStringLiteral("searchPolygonNormalized"),
        QStringLiteral("searchCircleCenterNormalized"),
        QStringLiteral("searchCircleRadiusNormalized"),
        QStringLiteral("originMode"),
        QStringLiteral("customOriginNormalized"),
        QStringLiteral("angleEnd")
    };
    for (const QString &key : bindingOwnedKeys)
        params->remove(key);
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

bool validMatchParameters(const TemplateLocationMatchParameters &parameters)
{
    const QSet<QString> polarities{
        QStringLiteral("use_polarity"),
        QStringLiteral("ignore_local_polarity"),
        QStringLiteral("ignore_global_polarity")
    };
    const QSet<QString> subPixelModes{
        QStringLiteral("least_squares"), QStringLiteral("none")
    };
    return parameters.minScore >= 0 && parameters.minScore <= 100
            && parameters.angleStart >= -180
            && parameters.angleStart <= 180
            && parameters.angleExtent >= 0
            && parameters.angleExtent <= 360
            && parameters.scaleMin >= 10 && parameters.scaleMax <= 200
            && parameters.scaleMin <= parameters.scaleMax
            && polarities.contains(parameters.polarity)
            && (parameters.contrastMode == QStringLiteral("auto")
                || parameters.contrastMode == QStringLiteral("manual"))
            && parameters.contrast >= 2 && parameters.contrast <= 255
            && parameters.minContrast >= 1
            && parameters.minContrast <= 254
            && (parameters.contrastMode != QStringLiteral("manual")
                || parameters.minContrast < parameters.contrast)
            && parameters.numLevels >= 0 && parameters.numLevels <= 10
            && subPixelModes.contains(parameters.subPixel)
            && std::isfinite(parameters.greediness)
            && parameters.greediness >= 0.0
            && parameters.greediness <= 1.0
            && std::isfinite(parameters.maxOverlap)
            && parameters.maxOverlap >= 0.0
            && parameters.maxOverlap <= 1.0;
}

bool validRegion(const TemplateLocationRegionConfig &region)
{
    const QString type = region.regionType.trimmed().toLower();
    if (type == QStringLiteral("rectangle"))
        return validNormalizedRect(region.roiNormalized);
    if (type == QStringLiteral("polygon"))
        return validNormalizedPolygon(region.polygonNormalized);
    if (type == QStringLiteral("circle")) {
        return validNormalizedPoint(region.circleCenterNormalized)
                && std::isfinite(region.circleRadiusNormalized)
                && region.circleRadiusNormalized > 0.0
                && region.circleRadiusNormalized <= 1.0;
    }
    return false;
}

bool validSearchGeometry(const TemplateLocationBaseBindingConfig &binding)
{
    const QString type = binding.searchRegionType.trimmed().toLower();
    if (type != QStringLiteral("full")
            && type != QStringLiteral("rectangle")
            && type != QStringLiteral("circle")
            && type != QStringLiteral("polygon")) {
        return false;
    }
    return validNormalizedRect(binding.searchRoiNormalized)
            && (type != QStringLiteral("polygon")
                || validNormalizedPolygon(binding.searchPolygonNormalized))
            && (type != QStringLiteral("circle")
                || (validNormalizedPoint(binding.searchCircleCenterNormalized)
                    && std::isfinite(binding.searchCircleRadiusNormalized)
                    && binding.searchCircleRadiusNormalized > 0.0
                    && binding.searchCircleRadiusNormalized <= 1.0));
}

TemplateLocationConfigValidationResult validateV6ModelBank(
        const TemplateLocationModelBankConfig &config,
        bool requireReadyModels)
{
    if (config.baseBindings.isEmpty()) {
        return validationError(QStringLiteral("no_base_bindings"),
                               QStringLiteral("The v6 template bank has no base binding."));
    }
    if (config.baseBindings.size() >
            TemplateLocationConfig::MaximumBaseBindingCount) {
        return validationError(
                    QStringLiteral("too_many_base_bindings"),
                    QStringLiteral("A template bank supports at most %1 base bindings.")
                    .arg(TemplateLocationConfig::MaximumBaseBindingCount));
    }

    QSet<QString> baseIds;
    QSet<int> baseOrders;
    int enabledTemplateCount = 0;
    QSet<QString> enabledBaseIds;
    for (const TemplateLocationTemplateConfig &item : config.templates) {
        if (!item.enabled)
            continue;
        ++enabledTemplateCount;
        enabledBaseIds.insert(item.sourceBaseId.trimmed());
    }
    const bool requiresCommonOrigin = enabledTemplateCount > 1
            || enabledBaseIds.size() > 1;
    for (const TemplateLocationBaseBindingConfig &binding :
         config.baseBindings) {
        const QString baseId = binding.baseId.trimmed();
        if (baseId.isEmpty()) {
            return validationError(QStringLiteral("missing_base_id"),
                                   QStringLiteral("A v6 base binding has no stable base ID."));
        }
        if (baseIds.contains(baseId)) {
            return validationError(
                        QStringLiteral("duplicate_base_id"),
                        QStringLiteral("Base ID '%1' is duplicated.").arg(baseId));
        }
        if (binding.order < 0 || baseOrders.contains(binding.order)) {
            return validationError(
                        QStringLiteral("invalid_base_order"),
                        QStringLiteral("Base binding order must be non-negative and unique."));
        }
        baseIds.insert(baseId);
        baseOrders.insert(binding.order);
        if (!validSearchGeometry(binding)) {
            return validationError(
                        QStringLiteral("invalid_search_region"),
                        QStringLiteral("Base '%1' has invalid search geometry.")
                        .arg(baseId));
        }
        if (binding.originMode != QStringLiteral("centroid")
                && binding.originMode != QStringLiteral("custom")) {
            return validationError(
                        QStringLiteral("invalid_origin"),
                        QStringLiteral("Base '%1' has an invalid output origin mode.")
                        .arg(baseId));
        }
        const bool bindingNeedsCommonOrigin = requiresCommonOrigin
                && enabledBaseIds.contains(baseId);
        if ((binding.originMode == QStringLiteral("custom")
             && !validNormalizedPoint(binding.customOriginNormalized))
                || (bindingNeedsCommonOrigin
                    && binding.originMode != QStringLiteral("custom"))) {
            return validationError(
                        QStringLiteral("invalid_origin"),
                        bindingNeedsCommonOrigin
                        ? QStringLiteral("Every base in a multi-template or multi-base bank requires a custom common origin.")
                        : QStringLiteral("The custom output origin is invalid."));
        }
    }

    const TemplateLocationMatchParameters shared =
            TemplateLocationConfig::sharedMatchParameters(config);
    if (!validMatchParameters(shared)) {
        return validationError(QStringLiteral("invalid_parameter"),
                               QStringLiteral("Shared template matching parameters are invalid."));
    }
    if (config.timeoutMs < 0
            || config.maxMatches < 1 || config.maxMatches > 100
            || config.minMatchCount < 1
            || config.maxMatchCount < config.minMatchCount
            || config.maxMatchCount > config.maxMatches) {
        return validationError(QStringLiteral("invalid_parameter"),
                               QStringLiteral("Library-wide timeout or result-count rules are invalid."));
    }
    if (config.primaryMatchStrategy != QStringLiteral("first_valid")) {
        return validationError(
                    QStringLiteral("invalid_primary_match_strategy"),
                    QStringLiteral("Version 6 requires the first_valid template strategy."));
    }

    QSet<QString> templateIds;
    QSet<QString> cacheKeys;
    QSet<QString> regionIds;
    QSet<QString> templateOrderKeys;
    int enabledCount = 0;
    for (int templateIndex = 0; templateIndex < config.templates.size();
         ++templateIndex) {
        const TemplateLocationTemplateConfig &item =
                config.templates.at(templateIndex);
        const QString id = item.templateId.trimmed();
        const QString description = itemDescription(item, templateIndex);
        if (id.isEmpty()) {
            return validationError(
                        QStringLiteral("missing_template_id"),
                        QStringLiteral("%1 has no stable template ID.").arg(description),
                        templateIndex, id);
        }
        if (templateIds.contains(id)) {
            return validationError(
                        QStringLiteral("duplicate_template_id"),
                        QStringLiteral("Template ID '%1' is duplicated.").arg(id),
                        templateIndex, id);
        }
        templateIds.insert(id);
        if (!baseIds.contains(item.sourceBaseId)) {
            return validationError(
                        QStringLiteral("missing_source_base"),
                        QStringLiteral("%1 references an unknown source base.")
                        .arg(description), templateIndex, id);
        }
        const QString orderKey = QStringLiteral("%1\n%2")
                .arg(item.sourceBaseId).arg(item.order);
        if (item.order < 0 || templateOrderKeys.contains(orderKey)) {
            return validationError(
                        QStringLiteral("invalid_template_order"),
                        QStringLiteral("Template order must be non-negative and unique within each base."),
                        templateIndex, id);
        }
        templateOrderKeys.insert(orderKey);
        if (item.modelCacheKey.trimmed().isEmpty()
                || cacheKeys.contains(item.modelCacheKey.trimmed())) {
            return validationError(
                        item.modelCacheKey.trimmed().isEmpty()
                        ? QStringLiteral("missing_model_cache_key")
                        : QStringLiteral("duplicate_model_cache_key"),
                        QStringLiteral("%1 has an invalid model cache identity.")
                        .arg(description), templateIndex, id);
        }
        cacheKeys.insert(item.modelCacheKey.trimmed());

        // A disabled v6 item is a persistent slot, not an executable model.
        // Keep its stable identity/Base/order contract strict so UI T-numbering
        // remains deterministic, but allow an unfinished draft to be disabled
        // without its geometry, override parameters, or model readiness
        // blocking the other enabled templates.
        if (!item.enabled)
            continue;

        ++enabledCount;
        if (item.includeRegions.isEmpty()
                || item.includeRegions.size() >
                   TemplateLocationConfig::MaximumRegionsPerTemplate) {
            return validationError(
                        QStringLiteral("invalid_include_region_count"),
                        QStringLiteral("%1 requires 1 to %2 include regions.")
                        .arg(description)
                        .arg(TemplateLocationConfig::MaximumRegionsPerTemplate),
                        templateIndex, id);
        }
        if (item.excludeRegions.size() >
                TemplateLocationConfig::MaximumRegionsPerTemplate) {
            return validationError(
                        QStringLiteral("invalid_exclude_region_count"),
                        QStringLiteral("%1 supports at most %2 exclude regions.")
                        .arg(description)
                        .arg(TemplateLocationConfig::MaximumRegionsPerTemplate),
                        templateIndex, id);
        }
        const auto validateRegions = [&](
                const QVector<TemplateLocationRegionConfig> &regions) {
            for (const TemplateLocationRegionConfig &region : regions) {
                const QString regionId = region.regionId.trimmed();
                if (regionId.isEmpty() || regionIds.contains(regionId)
                        || !validRegion(region)) {
                    return false;
                }
                regionIds.insert(regionId);
            }
            return true;
        };
        if (!validateRegions(item.includeRegions)
                || !validateRegions(item.excludeRegions)) {
            return validationError(
                        QStringLiteral("invalid_composite_region"),
                        QStringLiteral("%1 has a missing/duplicate region ID or invalid geometry.")
                        .arg(description), templateIndex, id);
        }
        if (item.useIndependentParameters
                && (!item.independentParametersComplete
                    || !validMatchParameters(item.independentParameters))) {
            return validationError(
                        QStringLiteral("invalid_parameter_override"),
                        QStringLiteral("%1 requires a complete valid parameter override.")
                        .arg(description), templateIndex, id);
        }
        if (requireReadyModels && !item.modelCreated) {
            return validationError(
                        QStringLiteral("model_not_created"),
                        QStringLiteral("%1 must be built before execution.")
                        .arg(description), templateIndex, id);
        }
    }
    if (enabledCount == 0) {
        return validationError(QStringLiteral("no_enabled_templates"),
                               QStringLiteral("The template bank has no enabled templates."));
    }
    return TemplateLocationConfigValidationResult();
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
    const bool compositeContract =
            config.version == CompositeBankParamsVersion;
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
                compositeContract ? QStringLiteral("first_valid")
                                  : QStringLiteral("best_score"));
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

    if (compositeContract) {
        const QJsonArray bindings = params.value(
                    QStringLiteral("baseBindings")).toArray();
        config.baseBindings.reserve(bindings.size());
        for (int index = 0; index < bindings.size(); ++index) {
            config.baseBindings.append(baseBindingFromJson(
                                           bindings.at(index).toObject(),
                                           index));
        }
        if (!config.baseBindings.isEmpty()) {
            // Compatibility mirrors are derived, never serialized as a second
            // v6 source of truth.
            const TemplateLocationBaseBindingConfig &binding =
                    config.baseBindings.first();
            config.searchRegionType = binding.searchRegionType;
            config.searchRoiNormalized = binding.searchRoiNormalized;
            config.searchPolygonNormalized = binding.searchPolygonNormalized;
            config.searchCircleCenterNormalized =
                    binding.searchCircleCenterNormalized;
            config.searchCircleRadiusNormalized =
                    binding.searchCircleRadiusNormalized;
            config.originMode = binding.originMode;
            config.customOriginNormalized = binding.customOriginNormalized;
        }
        const TemplateLocationMatchParameters shared =
                sharedMatchParameters(config);
        const QJsonArray templates = params.value(QStringLiteral("templates"))
                .toArray();
        config.templates.reserve(templates.size());
        for (int index = 0; index < templates.size(); ++index) {
            config.templates.append(v6ItemFromJson(
                                        templates.at(index).toObject(),
                                        index, config.toolId, shared));
        }
    } else if (bankContract) {
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
                && config.version != ModelBankParamsVersion
                && config.version != CompositeBankParamsVersion)) {
        return config.extraParams;
    }

    QJsonObject params = config.extraParams;

    if (config.version == CompositeBankParamsVersion) {
        writeV6SharedFields(config, &params);
        params.insert(QStringLiteral("version"),
                      CompositeBankParamsVersion);
        params.insert(QStringLiteral("templateMode"),
                      QStringLiteral("alternatives"));
        params.insert(QStringLiteral("primaryMatchStrategy"),
                      QStringLiteral("first_valid"));
        params.remove(QStringLiteral("primaryTemplateId"));
        params.remove(QStringLiteral("fusion"));
        removeFlatTemplateFields(&params);

        QJsonArray bindings;
        for (const TemplateLocationBaseBindingConfig &binding :
             config.baseBindings) {
            bindings.append(baseBindingToJson(binding));
        }
        params.insert(QStringLiteral("baseBindings"), bindings);

        QJsonArray templates;
        for (const TemplateLocationTemplateConfig &item : config.templates)
            templates.append(v6ItemToJson(item));
        params.insert(QStringLiteral("templates"), templates);
        return params;
    }

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

TemplateLocationMatchParameters sharedMatchParameters(
        const TemplateLocationModelBankConfig &config)
{
    TemplateLocationMatchParameters parameters;
    parameters.minScore = config.minScore;
    parameters.angleStart = config.angleStart;
    parameters.angleExtent = config.angleExtent;
    parameters.scaleMin = config.scaleMin;
    parameters.scaleMax = config.scaleMax;
    parameters.polarity = config.polarity;
    parameters.contrastMode = config.contrastMode;
    parameters.contrast = config.contrast;
    parameters.minContrast = config.minContrast;
    parameters.numLevels = config.numLevels;
    parameters.subPixel = config.subPixel;
    parameters.greediness = config.greediness;
    parameters.maxOverlap = config.maxOverlap;
    return parameters;
}

TemplateLocationMatchParameters effectiveMatchParameters(
        const TemplateLocationModelBankConfig &config,
        const TemplateLocationTemplateConfig &item)
{
    if (config.version == CompositeBankParamsVersion
            && item.useIndependentParameters) {
        return item.independentParameters;
    }
    return sharedMatchParameters(config);
}

const TemplateLocationBaseBindingConfig *findBaseBinding(
        const TemplateLocationModelBankConfig &config,
        const QString &baseId)
{
    const QString id = baseId.trimmed();
    if (id.isEmpty())
        return nullptr;
    for (const TemplateLocationBaseBindingConfig &binding :
         config.baseBindings) {
        if (binding.baseId == id)
            return &binding;
    }
    return nullptr;
}

TemplateLocationBaseBindingConfig *findBaseBinding(
        TemplateLocationModelBankConfig *config,
        const QString &baseId)
{
    if (!config)
        return nullptr;
    const QString id = baseId.trimmed();
    if (id.isEmpty())
        return nullptr;
    for (TemplateLocationBaseBindingConfig &binding : config->baseBindings) {
        if (binding.baseId == id)
            return &binding;
    }
    return nullptr;
}

QVector<TemplateLocationOrderedTemplateRef> orderedTemplateRefs(
        const TemplateLocationModelBankConfig &config,
        bool enabledOnly)
{
    QVector<TemplateLocationOrderedTemplateRef> refs;
    refs.reserve(config.templates.size());
    for (int templateIndex = 0; templateIndex < config.templates.size();
         ++templateIndex) {
        const TemplateLocationTemplateConfig &item =
                config.templates.at(templateIndex);
        if (enabledOnly && !item.enabled)
            continue;
        TemplateLocationOrderedTemplateRef ref;
        ref.templateIndex = templateIndex;
        ref.templateId = item.templateId;
        ref.baseId = item.sourceBaseId;
        ref.templateOrder = config.version == CompositeBankParamsVersion
                ? item.order : item.priority;
        if (config.version == CompositeBankParamsVersion) {
            for (int bindingIndex = 0;
                 bindingIndex < config.baseBindings.size(); ++bindingIndex) {
                const TemplateLocationBaseBindingConfig &binding =
                        config.baseBindings.at(bindingIndex);
                if (binding.baseId != item.sourceBaseId)
                    continue;
                ref.baseBindingIndex = bindingIndex;
                ref.baseOrder = binding.order;
                break;
            }
            if (ref.baseBindingIndex < 0)
                ref.baseOrder = MaximumBaseBindingCount + 1;
        }
        refs.append(ref);
    }
    std::stable_sort(refs.begin(), refs.end(),
                     [](const TemplateLocationOrderedTemplateRef &left,
                        const TemplateLocationOrderedTemplateRef &right) {
        if (left.baseOrder != right.baseOrder)
            return left.baseOrder < right.baseOrder;
        if (left.baseId != right.baseId)
            return left.baseId < right.baseId;
        if (left.templateOrder != right.templateOrder)
            return left.templateOrder < right.templateOrder;
        if (left.templateId != right.templateId)
            return left.templateId < right.templateId;
        return left.templateIndex < right.templateIndex;
    });
    for (int index = 0; index < refs.size(); ++index)
        refs[index].globalIndex = index;
    return refs;
}

QVector<TemplateLocationOrderedTemplateRef> firstValidTemplateOrder(
        const TemplateLocationModelBankConfig &config)
{
    // Assign T0..T7 from the complete persisted bank first, then filter the
    // execution list. Disabled slots therefore keep their UI-visible global
    // indices while FirstValid still searches enabled templates only.
    const QVector<TemplateLocationOrderedTemplateRef> allRefs =
            orderedTemplateRefs(config, false);
    QVector<TemplateLocationOrderedTemplateRef> enabledRefs;
    enabledRefs.reserve(allRefs.size());
    for (const TemplateLocationOrderedTemplateRef &ref : allRefs) {
        if (ref.templateIndex >= 0 &&
                ref.templateIndex < config.templates.size() &&
                config.templates.at(ref.templateIndex).enabled) {
            enabledRefs.append(ref);
        }
    }
    return enabledRefs;
}

int globalTemplateIndex(const TemplateLocationModelBankConfig &config,
                        const QString &templateId,
                        bool enabledOnly)
{
    const QString id = templateId.trimmed();
    const QVector<TemplateLocationOrderedTemplateRef> refs =
            orderedTemplateRefs(config, enabledOnly);
    for (const TemplateLocationOrderedTemplateRef &ref : refs) {
        if (ref.templateId == id)
            return ref.globalIndex;
    }
    return -1;
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
    const TemplateLocationMatchParameters effective =
            effectiveMatchParameters(bank, item);
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
    const TemplateLocationBaseBindingConfig *binding =
            bank.version == CompositeBankParamsVersion
            ? findBaseBinding(bank, item.sourceBaseId) : nullptr;
    config.searchRegionType = binding ? binding->searchRegionType
                                      : bank.searchRegionType;
    config.searchRoiNormalized = binding ? binding->searchRoiNormalized
                                         : bank.searchRoiNormalized;
    config.searchPolygonNormalized = binding ? binding->searchPolygonNormalized
                                             : bank.searchPolygonNormalized;
    config.searchCircleCenterNormalized = binding
            ? binding->searchCircleCenterNormalized
            : bank.searchCircleCenterNormalized;
    config.searchCircleRadiusNormalized = binding
            ? binding->searchCircleRadiusNormalized
            : bank.searchCircleRadiusNormalized;
    config.minScore = effective.minScore;
    config.angleStart = effective.angleStart;
    config.angleExtent = effective.angleExtent;
    config.scaleMin = effective.scaleMin;
    config.scaleMax = effective.scaleMax;
    config.polarity = effective.polarity;
    config.contrastMode = effective.contrastMode;
    config.contrast = effective.contrast;
    config.minContrast = effective.minContrast;
    config.numLevels = effective.numLevels;
    config.subPixel = effective.subPixel;
    config.greediness = effective.greediness;
    config.timeoutMs = bank.timeoutMs;
    config.maxMatches = bank.maxMatches;
    config.minMatchCount = bank.minMatchCount;
    config.maxMatchCount = bank.maxMatchCount;
    config.maxOverlap = effective.maxOverlap;
    config.originMode = binding ? binding->originMode : bank.originMode;
    config.customOriginNormalized = binding
            ? binding->customOriginNormalized : bank.customOriginNormalized;
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
    if (result.version == CompositeBankParamsVersion)
        ensureStableV6Ids(&result);
    else
        ensureStableTemplateIds(&result);
    return result;
}

void ensureStableV6Ids(TemplateLocationModelBankConfig *config)
{
    if (!config || config->version != CompositeBankParamsVersion
            || !config->decodeSupported || config->rawPassthrough) {
        return;
    }

    // Base IDs are foreign keys into the scheme-level ReferenceAssetSet.
    // Never invent or rewrite them here: malformed/missing bindings must stay
    // visible so validation and the UI can require an explicit rebind.
    ensureStableTemplateIds(config);
    QSet<QString> regionIds;
    for (TemplateLocationTemplateConfig &item : config->templates) {
        const auto normalizeRegions = [&regionIds](
                QVector<TemplateLocationRegionConfig> *regions) {
            if (!regions)
                return;
            for (TemplateLocationRegionConfig &region : *regions) {
                QString id = region.regionId.trimmed();
                while (id.isEmpty() || regionIds.contains(id)) {
                    id = QStringLiteral("region-%1").arg(
                                QUuid::createUuid().toString(
                                    QUuid::WithoutBraces));
                }
                region.regionId = id;
                regionIds.insert(id);
            }
        };
        normalizeRegions(&item.includeRegions);
        normalizeRegions(&item.excludeRegions);
        deriveLegacyRegionMirrors(&item);
    }
}

TemplateLocationModelBankConfig upgradeToV6(
        const TemplateLocationModelBankConfig &config,
        const QString &defaultBaseId)
{
    if (!config.decodeSupported || config.rawPassthrough)
        return config;
    if (config.version == CompositeBankParamsVersion) {
        TemplateLocationModelBankConfig result = config;
        ensureStableV6Ids(&result);
        return result;
    }
    if (config.version != LegacyParamsVersion
            && config.version != ModelBankParamsVersion) {
        return config;
    }

    TemplateLocationModelBankConfig result = config;
    result.version = CompositeBankParamsVersion;
    result.templateMode = QStringLiteral("alternatives");
    result.primaryMatchStrategy = QStringLiteral("first_valid");
    result.primaryTemplateId.clear();
    result.baseBindings.clear();

    TemplateLocationBaseBindingConfig binding;
    binding.baseId = defaultBaseId.trimmed().isEmpty()
            ? QStringLiteral("base-0") : defaultBaseId.trimmed();
    binding.order = 0;
    binding.searchRegionType = config.searchRegionType;
    binding.searchRoiNormalized = config.searchRoiNormalized;
    binding.searchPolygonNormalized = config.searchPolygonNormalized;
    binding.searchCircleCenterNormalized = config.searchCircleCenterNormalized;
    binding.searchCircleRadiusNormalized = config.searchCircleRadiusNormalized;
    binding.originMode = config.originMode;
    binding.customOriginNormalized = config.customOriginNormalized;
    result.baseBindings.append(binding);

    const TemplateLocationMatchParameters shared =
            sharedMatchParameters(config);
    for (int index = 0; index < result.templates.size(); ++index) {
        TemplateLocationTemplateConfig &item = result.templates[index];
        item.sourceBaseId = binding.baseId;
        item.order = index;
        item.priority = index;
        item.includeRegions.clear();
        item.excludeRegions.clear();

        TemplateLocationRegionConfig include;
        include.regionType = item.templateRegionType;
        include.roiNormalized = item.templateRoiNormalized;
        include.polygonNormalized = item.templatePolygonNormalized;
        item.includeRegions.append(include);

        QString maskType = item.templateMaskRegionType.trimmed().toLower();
        if ((maskType.isEmpty() || maskType == QStringLiteral("none"))
                && !item.templateMaskPolygonNormalized.isEmpty()) {
            maskType = QStringLiteral("polygon");
        }
        if (!maskType.isEmpty() && maskType != QStringLiteral("none")) {
            TemplateLocationRegionConfig exclude;
            exclude.regionType = maskType;
            exclude.roiNormalized = item.templateMaskRoiNormalized;
            exclude.polygonNormalized = item.templateMaskPolygonNormalized;
            exclude.circleCenterNormalized =
                    item.templateMaskCircleCenterNormalized;
            exclude.circleRadiusNormalized =
                    item.templateMaskCircleRadiusNormalized;
            item.excludeRegions.append(exclude);
        }
        item.useIndependentParameters = false;
        item.independentParameters = shared;
        item.independentParametersComplete = true;
    }
    ensureStableV6Ids(&result);
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
            && config.version != ModelBankParamsVersion
            && config.version != CompositeBankParamsVersion) {
        return validationError(
                    QStringLiteral("unsupported_config_version"),
                    QStringLiteral("Template location supports configuration versions 4, 5, and 6."));
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
    if (config.version == CompositeBankParamsVersion)
        return validateV6ModelBank(config, requireReadyModels);

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
