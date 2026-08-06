#include "toolcore/PositionCorrectionConsumer.h"

#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolResult.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>

namespace {

PositionCorrectionResolveResult failure(
        const PositionCorrectionContext &context,
        const QString &status,
        const QString &message)
{
    PositionCorrectionResolveResult result;
    result.context = context;
    result.status = status;
    result.message = message;
    return result;
}

bool finitePositiveNumber(const QJsonValue &value, double *number)
{
    if (!value.isDouble())
        return false;
    const double parsed = value.toDouble();
    if (!std::isfinite(parsed) || parsed <= 0.0)
        return false;
    *number = parsed;
    return true;
}

bool readOptionalScale(const QJsonObject &payload,
                       const QString &key,
                       double *scale)
{
    const QJsonValue value = payload.value(key);
    if (value.isUndefined() || value.isNull()) {
        *scale = 1.0;
        return true;
    }
    return finitePositiveNumber(value, scale);
}

bool readMatrix(const QJsonValue &value, QVector<double> *matrix)
{
    if (!value.isArray())
        return false;
    const QJsonArray array = value.toArray();
    if (array.size() != 6)
        return false;
    QVector<double> parsed;
    parsed.reserve(6);
    for (const QJsonValue &entry : array) {
        if (!entry.isDouble())
            return false;
        const double number = entry.toDouble();
        if (!std::isfinite(number))
            return false;
        parsed.append(number);
    }
    *matrix = parsed;
    return true;
}

bool matchContour(const ToolOverlay &overlay)
{
    const QString role = overlay.extra.value(QStringLiteral("role")).toString();
    return overlay.label == QStringLiteral("match_result")
            || role == QStringLiteral("match_result")
            || role == QStringLiteral("position_correction_match_contour");
}

bool matchOrigin(const ToolOverlay &overlay)
{
    const QString role = overlay.extra.value(QStringLiteral("role")).toString();
    return overlay.label == QStringLiteral("match_center")
            || role == QStringLiteral("match_origin")
            || role == QStringLiteral("position_correction_match_origin");
}

} // namespace

PositionCorrectionResolveResult PositionCorrectionConsumer::resolve(
        const ToolRequest &request,
        const PositionCorrectionConsumerOptions &options)
{
    PositionCorrectionContext context;
    context.requested = options.requested;
    context.showMatchContour = options.showMatchContour;
    context.sourceId = PositionCorrection::normalizedSourceId(options.sourceId);

    if (!context.requested) {
        PositionCorrectionResolveResult result;
        result.success = true;
        result.context = context;
        return result;
    }

    const QJsonValue correctionsValue = request.runtimeContext.value(
                QStringLiteral("positionCorrectionsById"));
    if (!correctionsValue.isObject()) {
        return failure(context,
                       QStringLiteral("position_correction_source_missing"),
                       QStringLiteral("Position correction results are unavailable for this frame."));
    }

    const QJsonValue sourceValue = correctionsValue.toObject().value(context.sourceId);
    if (!sourceValue.isObject()) {
        return failure(context,
                       QStringLiteral("position_correction_source_missing"),
                       QStringLiteral("Selected position correction source is unavailable for this frame."));
    }

    const QJsonObject sourceResult = sourceValue.toObject();
    const QJsonObject payload = sourceResult.value(QStringLiteral("payload")).toObject();
    if (sourceResult.value(QStringLiteral("toolId")).toString().trimmed()
            != context.sourceId
            || toolTypeFromString(sourceResult.value(QStringLiteral("toolType"))
                                  .toString()) != ToolType::PositionCorrection) {
        return failure(context,
                       QStringLiteral("position_correction_source_invalid"),
                       QStringLiteral("Position correction result identity or type is invalid."));
    }

    const QString currentFrameId = request.frameId.trimmed().isEmpty()
            ? request.runtimeContext.value(QStringLiteral("frameId"))
              .toString().trimmed()
            : request.frameId.trimmed();
    const QString sourceFrameId = payload.value(QStringLiteral("frameId"))
            .toString().trimmed();
    if (!currentFrameId.isEmpty()
            && (sourceFrameId.isEmpty() || sourceFrameId != currentFrameId)) {
        return failure(context,
                       QStringLiteral("position_correction_frame_mismatch"),
                       QStringLiteral("Position correction result does not belong to the current frame."));
    }

    context.sourceStatus = sourceResult.value(QStringLiteral("status"))
            .toString(payload.value(QStringLiteral("positionCorrectionReason"))
                      .toString()).trimmed();

    if (!sourceResult.value(QStringLiteral("success")).toBool(false)
            || !sourceResult.value(QStringLiteral("ok")).toBool(false)
            || !payload.value(QStringLiteral("positionCorrectionApplied")).toBool(false)) {
        const QString sourceMessage = sourceResult.value(QStringLiteral("message"))
                .toString().trimmed();
        if (context.sourceStatus == QStringLiteral("not_found")) {
            return failure(
                        context,
                        QStringLiteral("position_correction_match_not_found"),
                        sourceMessage.isEmpty()
                        ? QStringLiteral("位置修正未找到匹配轮廓。")
                        : QStringLiteral("位置修正未找到匹配轮廓：%1").arg(sourceMessage));
        }
        return failure(
                    context,
                    QStringLiteral("position_correction_source_failed"),
                    sourceMessage.isEmpty()
                    ? QStringLiteral("Position correction source failed for this frame (%1).")
                      .arg(context.sourceStatus.isEmpty()
                           ? QStringLiteral("unknown") : context.sourceStatus)
                    : QStringLiteral("%1 (%2)")
                      .arg(sourceMessage,
                           context.sourceStatus.isEmpty()
                           ? QStringLiteral("unknown") : context.sourceStatus));
    }

    const QString payloadSourceId = payload.value(QStringLiteral("sourceId"))
            .toString(context.sourceId).trimmed();
    if (payloadSourceId != context.sourceId) {
        return failure(context,
                       QStringLiteral("position_correction_source_invalid"),
                       QStringLiteral("Position correction source ID does not match the selected source."));
    }

    if (!readMatrix(payload.value(QStringLiteral("referenceToRunHomMat2D")),
                    &context.referenceToRunHomMat2D)) {
        return failure(context,
                       QStringLiteral("invalid_position_correction_matrix"),
                       QStringLiteral("referenceToRunHomMat2D must contain six finite values."));
    }

    const QJsonValue inverseValue =
            payload.value(QStringLiteral("runToReferenceHomMat2D"));
    if (!inverseValue.isUndefined() && !inverseValue.isNull()
            && !readMatrix(inverseValue, &context.runToReferenceHomMat2D)) {
        return failure(context,
                       QStringLiteral("invalid_position_correction_matrix"),
                       QStringLiteral("runToReferenceHomMat2D must contain six finite values."));
    }

    if (!readOptionalScale(payload, QStringLiteral("referenceScale"),
                           &context.referenceScale)
            || !readOptionalScale(payload, QStringLiteral("runScale"),
                                  &context.runScale)
            || !readOptionalScale(payload, QStringLiteral("scaleRatio"),
                                  &context.scaleRatio)) {
        return failure(context,
                       QStringLiteral("invalid_pose_scale"),
                       QStringLiteral("Position correction scales must be finite positive numbers."));
    }

    const ToolResult correctionResult = ToolResult::fromJson(sourceResult);
    for (const ToolOverlay &overlay : correctionResult.overlays) {
        if (matchContour(overlay))
            context.matchContours.append(overlay);
        if (matchOrigin(overlay))
            context.matchOrigins.append(overlay);
    }

    context.applied = true;
    PositionCorrectionResolveResult result;
    result.success = true;
    result.context = context;
    return result;
}
