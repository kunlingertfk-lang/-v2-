#include "tooladapters/TemplateLocationAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>

namespace {

int intParam(const QJsonObject &object, const QString &key, int fallback)
{
    return object.value(key).toInt(fallback);
}

double doubleParam(const QJsonObject &object, const QString &key, double fallback)
{
    return object.value(key).toDouble(fallback);
}

QString stringParam(const QJsonObject &object, const QString &key, const QString &fallback)
{
    const QString value = object.value(key).toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

QRectF rectParam(const QJsonObject &object, const QString &key, const QRectF &fallback)
{
    const QJsonObject value = object.value(key).toObject();
    if (value.isEmpty())
        return fallback;
    return QRectF(value.value(QStringLiteral("x")).toDouble(fallback.x()),
                  value.value(QStringLiteral("y")).toDouble(fallback.y()),
                  value.value(QStringLiteral("width")).toDouble(fallback.width()),
                  value.value(QStringLiteral("height")).toDouble(fallback.height()));
}

QPointF pointParam(const QJsonObject &object, const QString &key, const QPointF &fallback)
{
    const QJsonObject value = object.value(key).toObject();
    if (value.isEmpty())
        return fallback;
    return QPointF(value.value(QStringLiteral("x")).toDouble(fallback.x()),
                   value.value(QStringLiteral("y")).toDouble(fallback.y()));
}

QVector<QPointF> pointsParam(const QJsonObject &object, const QString &key)
{
    QVector<QPointF> points;
    const QJsonArray values = object.value(key).toArray();
    points.reserve(values.size());
    for (const QJsonValue &value : values) {
        const QJsonObject point = value.toObject();
        points.append(QPointF(point.value(QStringLiteral("x")).toDouble(),
                              point.value(QStringLiteral("y")).toDouble()));
    }
    return points;
}

TemplateLocationHalconConfig runnerConfig(const ToolConfig &config)
{
    const QJsonObject params = config.params;
    TemplateLocationHalconConfig output;
    output.toolId = config.toolId;
    output.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                stringParam(params, QStringLiteral("halconSoPath"), QString()),
                &output.halconSoPathCandidates);
    output.modelCacheKey = stringParam(params,
                                       QStringLiteral("modelCacheKey"),
                                       QStringLiteral("%1_shape_model").arg(config.toolId));
    output.templateRegionType = stringParam(params, QStringLiteral("templateRegionType"), output.templateRegionType);
    output.templateRoiNormalized = rectParam(params, QStringLiteral("templateRoiNormalized"), output.templateRoiNormalized);
    output.templatePolygonNormalized = pointsParam(params, QStringLiteral("templatePolygonNormalized"));
    output.templateMaskRegionType = stringParam(
                params, QStringLiteral("templateMaskRegionType"),
                params.value(QStringLiteral("templateMaskPolygonNormalized")).toArray().isEmpty()
                    ? QStringLiteral("none") : QStringLiteral("polygon"));
    output.templateMaskRoiNormalized = rectParam(
                params, QStringLiteral("templateMaskRoiNormalized"),
                output.templateMaskRoiNormalized);
    output.templateMaskPolygonNormalized = pointsParam(
                params, QStringLiteral("templateMaskPolygonNormalized"));
    output.templateMaskCircleCenterNormalized = pointParam(
                params, QStringLiteral("templateMaskCircleCenterNormalized"),
                output.templateMaskRoiNormalized.center());
    output.templateMaskCircleRadiusNormalized = doubleParam(
                params, QStringLiteral("templateMaskCircleRadiusNormalized"),
                qMin(output.templateMaskRoiNormalized.width(),
                     output.templateMaskRoiNormalized.height()) / 2.0);
    output.searchRegionType = stringParam(params, QStringLiteral("searchRegionType"), output.searchRegionType);
    output.searchRoiNormalized = rectParam(params, QStringLiteral("searchRoiNormalized"), output.searchRoiNormalized);
    output.searchPolygonNormalized = pointsParam(params, QStringLiteral("searchPolygonNormalized"));
    output.searchCircleCenterNormalized = pointParam(
                params, QStringLiteral("searchCircleCenterNormalized"),
                output.searchRoiNormalized.center());
    output.searchCircleRadiusNormalized = doubleParam(
                params, QStringLiteral("searchCircleRadiusNormalized"),
                qMin(output.searchRoiNormalized.width(),
                     output.searchRoiNormalized.height()) / 2.0);
    output.minScore = qBound(0, intParam(params, QStringLiteral("minScore"), output.minScore), 100);
    output.angleStart = qBound(-180, intParam(params, QStringLiteral("angleStart"), output.angleStart), 180);
    output.angleExtent = qBound(0, intParam(params, QStringLiteral("angleExtent"), output.angleExtent), 360);
    output.scaleMin = intParam(params, QStringLiteral("scaleMin"), output.scaleMin);
    output.scaleMax = intParam(params, QStringLiteral("scaleMax"), output.scaleMax);
    output.polarity = stringParam(params, QStringLiteral("polarity"), output.polarity);
    output.contrastMode = stringParam(params, QStringLiteral("contrastMode"), output.contrastMode);
    output.contrast = qBound(2, intParam(params, QStringLiteral("contrast"), output.contrast), 255);
    output.minContrast = qBound(1, intParam(params, QStringLiteral("minContrast"), output.minContrast), 254);
    output.numLevels = qBound(0, intParam(params, QStringLiteral("numLevels"), output.numLevels), 10);
    output.subPixel = stringParam(params, QStringLiteral("subPixel"), output.subPixel);
    output.greediness = qBound(0.0, doubleParam(params, QStringLiteral("greediness"), output.greediness), 1.0);
    output.timeoutMs = qMax(0, intParam(params, QStringLiteral("timeoutMs"), output.timeoutMs));
    output.maxMatches = qBound(1, intParam(params, QStringLiteral("maxMatches"), output.maxMatches), 100);
    output.minMatchCount = intParam(params, QStringLiteral("minMatchCount"), 1);
    output.maxMatchCount = intParam(params, QStringLiteral("maxMatchCount"), output.maxMatches);
    output.maxOverlap = qBound(0.0,
                               doubleParam(params, QStringLiteral("maxOverlap"), 50.0) / 100.0,
                               1.0);
    output.originMode = stringParam(params, QStringLiteral("originMode"), QStringLiteral("centroid"));
    output.customOriginNormalized = pointParam(
                params, QStringLiteral("customOriginNormalized"), QPointF(0.5, 0.5));
    return output;
}

} // namespace

bool TemplateLocationAdapter::supports(ToolType type) const
{
    return type == ToolType::TemplateLocation;
}

ToolResult TemplateLocationAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::TemplateLocation) {
        return ToolResult::error(config.toolId, config.toolType,
                                 QStringLiteral("TemplateLocationAdapter only supports TemplateLocation."),
                                 QStringLiteral("invalid_tool_type"));
    }
    if (!config.params.value(QStringLiteral("modelCreated")).toBool(false)) {
        return ToolResult::error(config.toolId, config.toolType,
                                 QStringLiteral("Please create the template model first."),
                                 QStringLiteral("no_model"));
    }
    const QJsonObject params = config.params;
    const QString contrastMode = stringParam(params, QStringLiteral("contrastMode"), QStringLiteral("auto"));
    const int contrast = intParam(params, QStringLiteral("contrast"), 40);
    const int minContrast = intParam(params, QStringLiteral("minContrast"), 10);
    const int scaleMin = intParam(params, QStringLiteral("scaleMin"), 100);
    const int scaleMax = intParam(params, QStringLiteral("scaleMax"), 100);
    const int angleStart = intParam(params, QStringLiteral("angleStart"), -45);
    const int angleExtent = intParam(params, QStringLiteral("angleExtent"), 90);
    const int numLevels = intParam(params, QStringLiteral("numLevels"), 0);
    const int maxMatches = intParam(params, QStringLiteral("maxMatches"), 1);
    const int minMatchCount = intParam(params, QStringLiteral("minMatchCount"), 1);
    const int maxMatchCount = intParam(params, QStringLiteral("maxMatchCount"), maxMatches);
    const double maxOverlap = doubleParam(params, QStringLiteral("maxOverlap"), 50.0);
    const QString originMode = stringParam(params, QStringLiteral("originMode"), QStringLiteral("centroid"));
    const double greediness = doubleParam(params, QStringLiteral("greediness"), 0.5);
    if (contrastMode != QStringLiteral("auto") && contrastMode != QStringLiteral("manual")) {
        return ToolResult::error(config.toolId, config.toolType,
                                 QStringLiteral("contrastMode must be auto or manual."),
                                 QStringLiteral("invalid_parameter"));
    }
    if (contrastMode == QStringLiteral("manual") &&
        (contrast < 2 || contrast > 255 || minContrast < 1 || minContrast >= contrast)) {
        return ToolResult::error(config.toolId, config.toolType,
                                 QStringLiteral("Manual contrast requires 1 <= MinContrast < Contrast <= 255."),
                                 QStringLiteral("invalid_parameter"));
    }
    if (scaleMin < 10 || scaleMax > 200 || scaleMin > scaleMax ||
        angleStart < -180 || angleStart > 180 || angleExtent < 0 || angleExtent > 360 ||
        numLevels < 0 || numLevels > 10 || maxMatches < 1 || maxMatches > 100 ||
        minMatchCount < 1 || maxMatchCount < minMatchCount || maxMatchCount > maxMatches ||
        maxOverlap < 0.0 || maxOverlap > 100.0 ||
        (originMode != QStringLiteral("centroid") && originMode != QStringLiteral("custom")) ||
        greediness < 0.0 || greediness > 1.0) {
        return ToolResult::error(config.toolId, config.toolType,
                                 QStringLiteral("Template location parameter range is invalid."),
                                 QStringLiteral("invalid_parameter"));
    }

    const TemplateLocationHalconResult matched =
            m_runner.run(request.image, request.referenceImage, runnerConfig(config));
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::TemplateLocation;
    result.success = matched.success;
    result.ok = matched.ok;
    result.status = matched.status;
    result.message = matched.message;
    result.score = matched.score;
    result.value = matched.payload.value(QStringLiteral("x")).toDouble(-1.0);
    result.count = matched.count;
    result.elapsedMs = matched.elapsedMs;
    result.text = matched.status;
    result.overlays = matched.overlays;
    result.payload = matched.payload;
    return result;
}
