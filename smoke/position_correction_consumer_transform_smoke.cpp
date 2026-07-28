#include "toolcore/PositionCorrection.h"
#include "toolcore/PositionCorrectionConsumer.h"
#include "toolcore/PositionCorrectionTransform.h"
#include "toolcore/ToolResult.h"

#include <QCoreApplication>
#include <QJsonArray>

#include <cmath>
#include <iostream>

namespace {

bool near(double actual, double expected)
{
    return std::fabs(actual - expected) < 1e-6;
}

ToolRequest requestWithResult(const ToolResult &correction)
{
    ToolRequest request;
    request.frameId = QStringLiteral("frame-1");
    request.runtimeContext.insert(QStringLiteral("frameId"),
                                  QStringLiteral("frame-1"));
    request.runtimeContext.insert(
                QStringLiteral("positionCorrectionsById"),
                QJsonObject{{QStringLiteral("pc-1"), correction.toJson()}});
    return request;
}

ToolResult successfulCorrection()
{
    ToolResult correction;
    correction.toolId = QStringLiteral("pc-1");
    correction.toolType = ToolType::PositionCorrection;
    correction.success = true;
    correction.ok = true;
    correction.status = QStringLiteral("corrected");
    correction.payload = {
        {QStringLiteral("sourceId"), QStringLiteral("pc-1")},
        {QStringLiteral("frameId"), QStringLiteral("frame-1")},
        {QStringLiteral("positionCorrectionApplied"), true},
        {QStringLiteral("referenceToRunHomMat2D"),
         QJsonArray{0.0, 2.0, 10.0, -2.0, 0.0, 20.0}},
        {QStringLiteral("runToReferenceHomMat2D"),
         QJsonArray{0.0, -0.5, 10.0, 0.5, 0.0, -5.0}},
        {QStringLiteral("referenceScale"), 1.0},
        {QStringLiteral("runScale"), 2.0},
        {QStringLiteral("scaleRatio"), 2.0}
    };
    ToolOverlay contour;
    contour.type = ToolOverlayType::Polygon;
    contour.label = QStringLiteral("match_result");
    contour.points = {QPointF(1.0, 1.0), QPointF(2.0, 1.0),
                      QPointF(2.0, 2.0)};
    correction.overlays.append(contour);
    ToolOverlay origin;
    origin.type = ToolOverlayType::Line;
    origin.label = QStringLiteral("match_center");
    origin.p1 = QPointF(8.0, 10.0);
    origin.p2 = QPointF(12.0, 10.0);
    correction.overlays.append(origin);
    return correction;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (PositionCorrection::normalizedSourceId(
                QStringLiteral("1 基准图.位置修正信息"))
            != PositionCorrection::defaultSourceId()) {
        std::cerr << "legacy reference display number was not migrated"
                  << std::endl;
        return 1;
    }
    const PositionCorrectionConfig migrated = PositionCorrection::fromParams(
                QJsonObject{
                    {QStringLiteral("positionCorrectionSource"),
                     QStringLiteral("1 基准图.位置修正信息")}
                });
    QJsonObject migratedParams;
    PositionCorrection::writeParams(migrated, &migratedParams);
    if (migrated.sourceId != PositionCorrection::defaultSourceId()
            || migrated.source != PositionCorrection::defaultSource()
            || migratedParams.value(QStringLiteral("positionCorrectionSource"))
               .toString() != PositionCorrection::defaultSource()) {
        std::cerr << "legacy reference display was not rewritten with number zero"
                  << std::endl;
        return 1;
    }

    PositionCorrectionConsumerOptions disabled;
    const PositionCorrectionResolveResult disabledResult =
            PositionCorrectionConsumer::resolve(ToolRequest(), disabled);
    if (!disabledResult.success || disabledResult.context.requested
            || disabledResult.context.applied) {
        std::cerr << "disabled correction did not pass through" << std::endl;
        return 1;
    }

    PositionCorrectionConsumerOptions options;
    options.requested = true;
    options.sourceId = QStringLiteral("pc-1");
    const PositionCorrectionResolveResult resolved =
            PositionCorrectionConsumer::resolve(
                requestWithResult(successfulCorrection()), options);
    if (!resolved.success || !resolved.context.applied
            || !near(resolved.context.scaleRatio, 2.0)
            || resolved.context.matchContours.size() != 1
            || resolved.context.matchOrigins.size() != 1) {
        std::cerr << "valid correction context was not resolved" << std::endl;
        return 1;
    }

    const QPointF point = PositionCorrectionTransform::transformPoint(
                QPointF(3.0, 4.0),
                resolved.context.referenceToRunHomMat2D);
    if (!near(point.x(), 12.0) || !near(point.y(), 16.0)) {
        std::cerr << "row/column point transform is incorrect" << std::endl;
        return 1;
    }

    ToolOverlay rect;
    rect.type = ToolOverlayType::Rect;
    rect.rect = QRectF(0.0, 0.0, 10.0, 5.0);
    const ToolOverlay transformedRect =
            PositionCorrectionTransform::transformOverlay(
                rect, resolved.context.referenceToRunHomMat2D);
    if (transformedRect.type != ToolOverlayType::Polygon
            || transformedRect.points.size() != 4) {
        std::cerr << "rectangle was not preserved as four transformed corners"
                  << std::endl;
        return 1;
    }

    ToolOverlay circle;
    circle.type = ToolOverlayType::Circle;
    circle.center = QPointF(5.0, 5.0);
    circle.radius = 3.0;
    const ToolOverlay transformedCircle =
            PositionCorrectionTransform::transformOverlay(
                circle, resolved.context.referenceToRunHomMat2D);
    if (!near(transformedCircle.radius, 6.0)) {
        std::cerr << "circle radius did not follow scale" << std::endl;
        return 1;
    }

    const QVector<ToolOverlay> contours =
            PositionCorrectionTransform::matchContourOverlays(
                resolved.context.matchContours, resolved.context.sourceId);
    if (contours.size() != 1
            || contours.first().extra.value(QStringLiteral("role")).toString()
            != QStringLiteral("position_correction_match_contour")) {
        std::cerr << "match contour role was not normalized" << std::endl;
        return 1;
    }
    const QVector<ToolOverlay> origins =
            PositionCorrectionTransform::matchOriginOverlays(
                resolved.context.matchOrigins, resolved.context.sourceId);
    if (origins.size() != 1
            || origins.first().extra.value(QStringLiteral("role")).toString()
            != QStringLiteral("position_correction_match_origin")) {
        std::cerr << "match origin role was not normalized" << std::endl;
        return 1;
    }

    ToolRequest missingRequest;
    const PositionCorrectionResolveResult missing =
            PositionCorrectionConsumer::resolve(missingRequest, options);
    if (missing.success
            || missing.status != QStringLiteral("position_correction_source_missing")) {
        std::cerr << "missing source did not fail explicitly" << std::endl;
        return 1;
    }

    ToolResult notFound = successfulCorrection();
    notFound.success = false;
    notFound.ok = false;
    notFound.status = QStringLiteral("not_found");
    notFound.message = QStringLiteral("no match");
    const PositionCorrectionResolveResult notFoundResult =
            PositionCorrectionConsumer::resolve(
                requestWithResult(notFound), options);
    if (notFoundResult.success
            || notFoundResult.status
            != QStringLiteral("position_correction_match_not_found")) {
        std::cerr << "not-found source did not map to the common status"
                  << std::endl;
        return 1;
    }

    ToolResult invalidMatrix = successfulCorrection();
    invalidMatrix.payload.insert(
                QStringLiteral("referenceToRunHomMat2D"),
                QJsonArray{1.0, 0.0});
    const PositionCorrectionResolveResult invalidMatrixResult =
            PositionCorrectionConsumer::resolve(
                requestWithResult(invalidMatrix), options);
    if (invalidMatrixResult.success
            || invalidMatrixResult.status
            != QStringLiteral("invalid_position_correction_matrix")) {
        std::cerr << "invalid matrix was accepted" << std::endl;
        return 1;
    }

    ToolResult invalidScale = successfulCorrection();
    invalidScale.payload.insert(QStringLiteral("scaleRatio"), 0.0);
    const PositionCorrectionResolveResult invalidScaleResult =
            PositionCorrectionConsumer::resolve(
                requestWithResult(invalidScale), options);
    if (invalidScaleResult.success
            || invalidScaleResult.status != QStringLiteral("invalid_pose_scale")) {
        std::cerr << "invalid scale was accepted" << std::endl;
        return 1;
    }

    ToolResult staleFrame = successfulCorrection();
    staleFrame.payload.insert(QStringLiteral("frameId"),
                              QStringLiteral("frame-0"));
    const PositionCorrectionResolveResult staleFrameResult =
            PositionCorrectionConsumer::resolve(
                requestWithResult(staleFrame), options);
    if (staleFrameResult.success
            || staleFrameResult.status
            != QStringLiteral("position_correction_frame_mismatch")) {
        std::cerr << "stale correction frame was accepted" << std::endl;
        return 1;
    }

    ToolResult wrongIdentity = successfulCorrection();
    wrongIdentity.toolId = QStringLiteral("pc-other");
    const PositionCorrectionResolveResult wrongIdentityResult =
            PositionCorrectionConsumer::resolve(
                requestWithResult(wrongIdentity), options);
    if (wrongIdentityResult.success
            || wrongIdentityResult.status
            != QStringLiteral("position_correction_source_invalid")) {
        std::cerr << "mismatched correction identity was accepted" << std::endl;
        return 1;
    }

    std::cout << "position_correction_consumer_transform_smoke passed"
              << std::endl;
    return 0;
}
