#include "ToolEngine.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/location/PositionCorrectionHalconRunner.h"
#include "algorithms/location/TemplateLocationHalconRunner.h"
#include "toolcore/PositionCorrection.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QRectF>

#include <cmath>

namespace {

QJsonObject ensureObject(const QJsonObject &parent, const QString &key)
{
    return parent.value(key).toObject();
}

void registerResultInContext(const ToolResult &result, QJsonObject *context)
{
    if (!context || result.toolId.trimmed().isEmpty())
        return;

    QJsonObject toolResults = ensureObject(*context, QStringLiteral("toolResultsById"));
    toolResults.insert(result.toolId, result.toJson());
    context->insert(QStringLiteral("toolResultsById"), toolResults);

    // Keep both successful and failed position-correction results in the frame
    // context. Consumers must be able to distinguish "source did not run" from
    // "source ran, but template location failed".
    if (result.toolType == ToolType::PositionCorrection) {
        const QString sourceId = result.payload.value(QStringLiteral("sourceId"))
                .toString(result.toolId).trimmed();
        if (!sourceId.isEmpty()) {
            QJsonObject corrections = ensureObject(*context,
                                                   QStringLiteral("positionCorrectionsById"));
            corrections.insert(sourceId, result.toJson());
            context->insert(QStringLiteral("positionCorrectionsById"), corrections);
        }
    }
}

double finiteDouble(const QJsonObject &object, const QString &key, double fallback = 0.0)
{
    const QJsonValue value = object.value(key);
    const double number = value.isString()
            ? value.toString().toDouble()
            : value.toDouble(fallback);
    return std::isfinite(number) ? number : fallback;
}

bool validNormalizedPoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y())
            && point.x() >= 0.0 && point.x() <= 1.0
            && point.y() >= 0.0 && point.y() <= 1.0;
}

QVector<QPointF> polygonPoints(const QJsonArray &values)
{
    QVector<QPointF> points;
    points.reserve(values.size());
    for (const QJsonValue &value : values) {
        const QJsonObject point = value.toObject();
        points.append(QPointF(point.value(QStringLiteral("x")).toDouble(),
                              point.value(QStringLiteral("y")).toDouble()));
    }
    return points;
}

ToolResult referencePositionCorrectionError(const QString &status,
                                            const QString &message)
{
    ToolResult result;
    result.toolId = PositionCorrection::defaultSourceId();
    result.toolType = ToolType::PositionCorrection;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.payload.insert(QStringLiteral("sourceKind"), QStringLiteral("reference"));
    result.payload.insert(QStringLiteral("sourceId"), PositionCorrection::defaultSourceId());
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), status);
    return result;
}

ToolResult runReferencePositionCorrection(const cv::Mat &image,
                                          const cv::Mat &referenceImage,
                                          const QJsonObject &runtimeContext)
{
    const ReferencePositionCorrectionConfig referenceConfig =
            PositionCorrection::referenceFromJson(
                runtimeContext.value(QStringLiteral("referencePositionCorrection")).toObject());
    if (!referenceConfig.enabled)
        return ToolResult();
    if (!referenceConfig.referenceCreated)
        return referencePositionCorrectionError(
                    QStringLiteral("reference_pose_missing"),
                    QStringLiteral("Reference position correction has not been created."));
    if (image.empty())
        return referencePositionCorrectionError(QStringLiteral("image_empty"),
                                                QStringLiteral("input image is empty"));
    if (referenceImage.empty())
        return referencePositionCorrectionError(QStringLiteral("no_reference_image"),
                                                QStringLiteral("reference image is empty"));
    if (referenceConfig.referencePose.isEmpty())
        return referencePositionCorrectionError(QStringLiteral("reference_pose_missing"),
                                                QStringLiteral("reference position pose is missing"));
    if (referenceConfig.originMode != QStringLiteral("centroid")
            && referenceConfig.originMode != QStringLiteral("custom")) {
        return referencePositionCorrectionError(QStringLiteral("invalid_custom_origin"),
                                                QStringLiteral("reference origin mode is invalid"));
    }
    if (referenceConfig.originMode == QStringLiteral("custom")
            && !validNormalizedPoint(referenceConfig.customOriginNormalized)) {
        return referencePositionCorrectionError(QStringLiteral("invalid_custom_origin"),
                                                QStringLiteral("reference custom origin is invalid"));
    }

    TemplateLocationHalconConfig locatorConfig;
    locatorConfig.toolId = PositionCorrection::defaultSourceId();
    locatorConfig.modelCacheKey = referenceConfig.modelCacheKey.trimmed().isEmpty()
            ? QStringLiteral("reference.positionCorrection.private_template")
            : referenceConfig.modelCacheKey.trimmed();
    locatorConfig.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(), &locatorConfig.halconSoPathCandidates);
    locatorConfig.templateRegionType = referenceConfig.templateRegionType;
    locatorConfig.templateRoiNormalized = referenceConfig.templateRoiNormalized;
    locatorConfig.templatePolygonNormalized =
            polygonPoints(referenceConfig.templatePolygonNormalized);
    locatorConfig.searchRegionType = QStringLiteral("full");
    locatorConfig.searchRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    locatorConfig.minScore = 50;
    locatorConfig.angleStart = -45;
    locatorConfig.angleExtent = 90;
    locatorConfig.scaleMin = 100;
    locatorConfig.scaleMax = 100;
    locatorConfig.maxMatches = 1;
    locatorConfig.minMatchCount = 1;
    locatorConfig.maxMatchCount = 1;
    locatorConfig.maxOverlap = 0.5;
    locatorConfig.originMode = referenceConfig.originMode;
    locatorConfig.customOriginNormalized = referenceConfig.customOriginNormalized;
    locatorConfig.timeoutMs = 2000;

    TemplateLocationHalconRunner locator;
    const TemplateLocationHalconResult located =
            locator.run(image, referenceImage, locatorConfig);
    if (!located.success || !located.ok) {
        ToolResult result = referencePositionCorrectionError(
                    located.status,
                    located.message.trimmed().isEmpty()
                    ? QStringLiteral("reference position locator failed")
                    : located.message);
        result.elapsedMs = located.elapsedMs;
        result.payload.insert(QStringLiteral("locatorPayload"), located.payload);
        return result;
    }

    PositionCorrectionHalconRunner correctionRunner;
    const PositionPose referencePose{
        finiteDouble(referenceConfig.referencePose, QStringLiteral("x")),
        finiteDouble(referenceConfig.referencePose, QStringLiteral("y")),
        finiteDouble(referenceConfig.referencePose, QStringLiteral("angleDeg")),
        finiteDouble(referenceConfig.referencePose, QStringLiteral("scale"), 1.0)
    };
    const PositionPose runPose{
        finiteDouble(located.payload, QStringLiteral("x")),
        finiteDouble(located.payload, QStringLiteral("y")),
        finiteDouble(located.payload, QStringLiteral("angleDeg")),
        finiteDouble(located.payload, QStringLiteral("scale"), 1.0)
    };
    const PositionCorrectionHalconResult corrected =
            correctionRunner.run(referencePose, runPose);
    ToolResult result;
    result.toolId = PositionCorrection::defaultSourceId();
    result.toolType = ToolType::PositionCorrection;
    result.success = corrected.success;
    result.ok = corrected.ok;
    result.status = corrected.status;
    result.message = corrected.message;
    result.score = located.score;
    result.count = located.count;
    result.elapsedMs = located.elapsedMs + corrected.elapsedMs;
    for (ToolOverlay overlay : located.overlays) {
        if (overlay.label == QStringLiteral("match_result")) {
            overlay.extra.insert(
                        QStringLiteral("role"),
                        QStringLiteral("position_correction_match_contour"));
        } else if (overlay.label == QStringLiteral("match_center")
                   || overlay.extra.value(QStringLiteral("role")).toString()
                   == QStringLiteral("match_origin")) {
            overlay.extra.insert(
                        QStringLiteral("role"),
                        QStringLiteral("position_correction_match_origin"));
        } else {
            continue;
        }
        overlay.extra.insert(QStringLiteral("positionCorrectionSourceId"),
                             PositionCorrection::defaultSourceId());
        result.overlays.append(overlay);
    }
    result.payload = corrected.payload;
    result.payload.insert(QStringLiteral("sourceKind"), QStringLiteral("reference"));
    result.payload.insert(QStringLiteral("scope"), QStringLiteral("global"));
    result.payload.insert(QStringLiteral("sourceId"), PositionCorrection::defaultSourceId());
    result.payload.insert(QStringLiteral("locatorStatus"), located.status);
    result.payload.insert(QStringLiteral("locatorScore"), located.score);
    result.payload.insert(QStringLiteral("locatorPayload"), located.payload);
    result.payload.insert(QStringLiteral("x"), runPose.x);
    result.payload.insert(QStringLiteral("y"), runPose.y);
    result.payload.insert(QStringLiteral("angle"), runPose.angleDeg);
    result.payload.insert(QStringLiteral("angleDeg"), runPose.angleDeg);
    result.payload.insert(QStringLiteral("scale"), runPose.scale);
    result.payload.insert(QStringLiteral("frameId"),
                          runtimeContext.value(QStringLiteral("frameId")).toString());
    return result;
}

} // namespace

void ToolEngine::registerAdapter(ToolAdapter *adapter)
{
    if (!adapter || m_adapters.contains(adapter))
        return;

    m_adapters.append(adapter);
}

ToolResult ToolEngine::runTool(const ToolRequest &request) const
{
    const ToolConfig &config = request.config;
    if (!config.enabled) {
        return ToolResult::error(config.toolId,
                                 config.toolType,
                                 QStringLiteral("Tool is disabled."),
                                 QStringLiteral("disabled"));
    }

    if (!config.isValid()) {
        return ToolResult::error(config.toolId,
                                 config.toolType,
                                 QStringLiteral("Invalid ToolConfig."),
                                 QStringLiteral("invalid_config"));
    }

    ToolAdapter *adapter = findAdapter(config.toolType);
    if (!adapter)
        return ToolResult::unsupported(config.toolId, config.toolType);

    QElapsedTimer timer;
    timer.start();
    ToolResult result = adapter->run(request);
    if (result.toolId.isEmpty())
        result.toolId = config.toolId;
    if (result.toolType == ToolType::Unknown)
        result.toolType = config.toolType;
    if (result.elapsedMs <= 0)
        result.elapsedMs = timer.elapsed();
    return result;
}

QVector<ToolResult> ToolEngine::runTools(const QVector<ToolConfig> &configs,
                                         const cv::Mat &image,
                                         const cv::Mat &referenceImage,
                                         const QJsonObject &runtimeContext,
                                         ToolResult *referenceCorrectionResult) const
{
    QVector<ToolResult> results;
    results.reserve(configs.size());
    QJsonObject frameContext = runtimeContext;
    if (!frameContext.contains(QStringLiteral("toolResultsById")))
        frameContext.insert(QStringLiteral("toolResultsById"), QJsonObject());
    if (!frameContext.contains(QStringLiteral("positionCorrectionsById")))
        frameContext.insert(QStringLiteral("positionCorrectionsById"), QJsonObject());

    const ToolResult referenceCorrection =
            runReferencePositionCorrection(image, referenceImage, frameContext);
    if (referenceCorrectionResult)
        *referenceCorrectionResult = referenceCorrection;
    if (!referenceCorrection.toolId.trimmed().isEmpty())
        registerResultInContext(referenceCorrection, &frameContext);

    for (const ToolConfig &config : configs) {
        if (!config.enabled)
            continue;

        ToolRequest request;
        request.config = config;
        request.image = image;
        request.referenceImage = referenceImage;
        request.runtimeContext = frameContext;
        ToolResult result = runTool(request);
        registerResultInContext(result, &frameContext);
        results.append(result);
    }

    return results;
}

ToolAdapter *ToolEngine::findAdapter(ToolType type) const
{
    for (ToolAdapter *adapter : m_adapters) {
        if (adapter && adapter->supports(type))
            return adapter;
    }

    return nullptr;
}
