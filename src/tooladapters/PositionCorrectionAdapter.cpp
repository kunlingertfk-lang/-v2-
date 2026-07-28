#include "tooladapters/PositionCorrectionAdapter.h"

#include "toolcore/PositionCorrection.h"

#include <QJsonArray>
#include <QJsonObject>

#include <cmath>

namespace {

QJsonObject positionCorrectionConfig(const ToolConfig &config)
{
    return config.params.value(QStringLiteral("positionCorrection")).toObject();
}

bool finiteJsonNumber(const QJsonValue &value, double *number)
{
    if (!number)
        return false;
    bool ok = true;
    const double parsed = value.isString()
            ? value.toString().toDouble(&ok)
            : value.toDouble(qQNaN());
    if (!ok || !std::isfinite(parsed))
        return false;
    *number = parsed;
    return true;
}

bool readScale(const QJsonObject &object,
               const QString &key,
               double *scale,
               bool *invalidScale)
{
    if (invalidScale)
        *invalidScale = false;
    if (!scale)
        return false;
    if (!object.contains(key)) {
        *scale = 1.0;
        return true;
    }
    if (!finiteJsonNumber(object.value(key), scale) || *scale <= 0.0) {
        if (invalidScale)
            *invalidScale = true;
        return false;
    }
    return true;
}

bool poseFromJson(const QJsonObject &object,
                  PositionPose *pose,
                  bool *invalidScale = nullptr,
                  bool *invalidPose = nullptr)
{
    if (invalidPose)
        *invalidPose = false;
    if (!pose || object.isEmpty())
        return false;
    if (!object.contains(QStringLiteral("x")) ||
            !object.contains(QStringLiteral("y")) ||
            !(object.contains(QStringLiteral("angleDeg")) ||
              object.contains(QStringLiteral("angle")))) {
        return false;
    }
    const QJsonValue angleValue = object.contains(QStringLiteral("angleDeg"))
            ? object.value(QStringLiteral("angleDeg"))
            : object.value(QStringLiteral("angle"));
    if (!finiteJsonNumber(object.value(QStringLiteral("x")), &pose->x)
            || !finiteJsonNumber(object.value(QStringLiteral("y")), &pose->y)
            || !finiteJsonNumber(angleValue, &pose->angleDeg)) {
        if (invalidPose)
            *invalidPose = true;
        return false;
    }
    return readScale(object, QStringLiteral("scale"), &pose->scale, invalidScale);
}

bool poseFromPayload(const QJsonObject &payload,
                     const PositionRunPoseSource &source,
                     PositionPose *pose,
                     bool *invalidScale = nullptr,
                     bool *invalidPose = nullptr)
{
    if (invalidPose)
        *invalidPose = false;
    if (!pose)
        return false;
    if (!payload.contains(source.xKey) ||
            !payload.contains(source.yKey) ||
            !payload.contains(source.angleKey)) {
        return false;
    }
    if (!finiteJsonNumber(payload.value(source.xKey), &pose->x)
            || !finiteJsonNumber(payload.value(source.yKey), &pose->y)
            || !finiteJsonNumber(payload.value(source.angleKey), &pose->angleDeg)) {
        if (invalidPose)
            *invalidPose = true;
        return false;
    }
    return readScale(payload, source.scaleKey, &pose->scale, invalidScale);
}

ToolResult errorResult(const ToolConfig &config,
                       const QString &status,
                       const QString &message,
                       const PositionRunPoseSource &source = PositionRunPoseSource())
{
    ToolResult result = ToolResult::error(config.toolId, config.toolType, message, status);
    result.payload.insert(QStringLiteral("sourceKind"), QStringLiteral("tool"));
    result.payload.insert(QStringLiteral("scope"), QStringLiteral("local"));
    result.payload.insert(QStringLiteral("sourceId"), config.toolId);
    result.payload.insert(QStringLiteral("poseProducerId"), source.producerId);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), status);
    return result;
}

} // namespace

bool PositionCorrectionAdapter::supports(ToolType type) const
{
    return type == ToolType::PositionCorrection;
}

ToolResult PositionCorrectionAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::PositionCorrection) {
        return ToolResult::error(config.toolId, config.toolType,
                                 QStringLiteral("PositionCorrectionAdapter only supports PositionCorrection."),
                                 QStringLiteral("invalid_tool_type"));
    }

    const QJsonObject correction = positionCorrectionConfig(config);
    const PositionRunPoseSource source =
            PositionCorrection::runPoseSourceFromConfig(correction);
    if (source.inconsistent) {
        return errorResult(config, QStringLiteral("inconsistent_pose_source"),
                           QStringLiteral("Position correction X/Y/angle must come from one producer."),
                           source);
    }
    if (!source.valid) {
        return errorResult(config, QStringLiteral("incomplete_input_binding"),
                           QStringLiteral("Position correction runPoseSource is incomplete."),
                           source);
    }

    if (!correction.value(QStringLiteral("referenceCreated")).toBool(false)) {
        return errorResult(config, QStringLiteral("reference_pose_missing"),
                           QStringLiteral("Position correction reference pose has not been created."),
                           source);
    }
    PositionPose referencePose;
    bool invalidReferenceScale = false;
    bool invalidReferencePose = false;
    if (!poseFromJson(correction.value(QStringLiteral("referencePose")).toObject(),
                      &referencePose,
                      &invalidReferenceScale,
                      &invalidReferencePose)) {
        if (invalidReferenceScale) {
            return errorResult(config, QStringLiteral("invalid_pose_scale"),
                               QStringLiteral("Position correction reference scale must be finite and greater than zero."),
                               source);
        }
        if (invalidReferencePose) {
            return errorResult(config, QStringLiteral("invalid_reference_pose"),
                               QStringLiteral("Position correction reference x/y/angle must be finite numbers."),
                               source);
        }
        return errorResult(config, QStringLiteral("reference_pose_missing"),
                           QStringLiteral("Position correction referencePose is missing."),
                           source);
    }

    const QJsonObject toolResults = request.runtimeContext
            .value(QStringLiteral("toolResultsById")).toObject();
    const QJsonObject producerResult = toolResults.value(source.producerId).toObject();
    if (producerResult.isEmpty()) {
        return errorResult(config, QStringLiteral("source_unavailable"),
                           QStringLiteral("Position correction pose source is unavailable."),
                           source);
    }
    const ToolType producerType = toolTypeFromString(
                producerResult.value(QStringLiteral("toolType")).toString());
    const bool validProducerType = producerType == ToolType::TemplateLocation
            || (source.producerId == PositionCorrection::defaultSourceId()
                && producerType == ToolType::PositionCorrection);
    if (producerResult.value(QStringLiteral("toolId")).toString().trimmed()
            != source.producerId
            || !validProducerType) {
        return errorResult(config, QStringLiteral("source_invalid"),
                           QStringLiteral("Position correction pose source identity or type is invalid."),
                           source);
    }
    const QJsonObject producerPayload =
            producerResult.value(QStringLiteral("payload")).toObject();
    const QString currentFrameId = request.frameId.trimmed().isEmpty()
            ? request.runtimeContext.value(QStringLiteral("frameId"))
              .toString().trimmed()
            : request.frameId.trimmed();
    const QString producerFrameId = producerPayload.value(QStringLiteral("frameId"))
            .toString().trimmed();
    if (!currentFrameId.isEmpty()
            && (producerFrameId.isEmpty() || producerFrameId != currentFrameId)) {
        return errorResult(config, QStringLiteral("source_frame_mismatch"),
                           QStringLiteral("Position correction pose source does not belong to the current frame."),
                           source);
    }
    if (!producerResult.value(QStringLiteral("success")).toBool(false) ||
            !producerResult.value(QStringLiteral("ok")).toBool(false)) {
        const QString producerStatus = producerResult.value(QStringLiteral("status"))
                .toString().trimmed();
        const QString producerMessage = producerResult.value(QStringLiteral("message"))
                .toString().trimmed();
        if (producerStatus == QStringLiteral("not_found")) {
            return errorResult(
                        config,
                        QStringLiteral("not_found"),
                        producerMessage.isEmpty()
                        ? QStringLiteral("Position correction template was not found in this frame.")
                        : producerMessage,
                        source);
        }
        return errorResult(
                    config,
                    QStringLiteral("source_invalid"),
                    producerMessage.isEmpty()
                    ? QStringLiteral("Position correction pose source did not produce an OK result.")
                    : QStringLiteral("%1 (%2)").arg(
                          producerMessage,
                          producerStatus.isEmpty() ? QStringLiteral("unknown")
                                                   : producerStatus),
                    source);
    }
    PositionPose runPose;
    bool invalidRunScale = false;
    bool invalidRunPose = false;
    if (!poseFromPayload(producerPayload,
                         source,
                         &runPose,
                         &invalidRunScale,
                         &invalidRunPose)) {
        if (invalidRunScale) {
            return errorResult(config, QStringLiteral("invalid_pose_scale"),
                               QStringLiteral("Position correction run scale must be finite and greater than zero."),
                               source);
        }
        if (invalidRunPose) {
            return errorResult(config, QStringLiteral("invalid_run_pose"),
                               QStringLiteral("Position correction run x/y/angle must be finite numbers."),
                               source);
        }
        return errorResult(config, QStringLiteral("input_binding_not_found"),
                           QStringLiteral("Position correction pose source payload lacks x/y/angle."),
                           source);
    }

    const PositionCorrectionHalconResult corrected =
            m_runner.run(referencePose, runPose);
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::PositionCorrection;
    result.success = corrected.success;
    result.ok = corrected.ok;
    result.status = corrected.status;
    result.message = corrected.message;
    result.elapsedMs = corrected.elapsedMs;
    result.text = corrected.status;
    result.payload = corrected.payload;
    result.payload.insert(QStringLiteral("sourceKind"), QStringLiteral("tool"));
    result.payload.insert(QStringLiteral("scope"), QStringLiteral("local"));
    result.payload.insert(QStringLiteral("sourceId"), config.toolId);
    result.payload.insert(QStringLiteral("poseProducerId"), source.producerId);
    result.payload.insert(QStringLiteral("frameId"),
                          request.runtimeContext.value(QStringLiteral("frameId")).toString());
    result.payload.insert(QStringLiteral("frameMeta"),
                          request.runtimeContext.value(QStringLiteral("frameMeta")).toObject());
    const ToolResult producerToolResult = ToolResult::fromJson(producerResult);
    for (ToolOverlay overlay : producerToolResult.overlays) {
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
                             config.toolId);
        result.overlays.append(overlay);
    }
    return result;
}
