#include "tooladapters/PositionCorrectionAdapter.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    }
}

bool near(double lhs, double rhs, double eps = 1e-6)
{
    return std::fabs(lhs - rhs) <= eps;
}

QPointF transformPoint(const QJsonArray &matrix, const QPointF &point)
{
    if (matrix.size() != 6)
        return QPointF(qQNaN(), qQNaN());
    const double row = point.y();
    const double column = point.x();
    return QPointF(matrix.at(3).toDouble() * row
                   + matrix.at(4).toDouble() * column
                   + matrix.at(5).toDouble(),
                   matrix.at(0).toDouble() * row
                   + matrix.at(1).toDouble() * column
                   + matrix.at(2).toDouble());
}

ToolConfig positionCorrectionConfig(bool referenceCreated = true,
                                    double referenceScale = 1.0,
                                    bool includeScale = true)
{
    ToolConfig config;
    config.toolId = QStringLiteral("pc-1");
    config.toolType = ToolType::PositionCorrection;
    config.category = ToolCategory::Location;
    config.enabled = true;

    QJsonObject correction;
    correction.insert(QStringLiteral("version"), 2);
    correction.insert(QStringLiteral("runPoseSource"), QJsonObject{
                          {QStringLiteral("producerId"), QStringLiteral("tpl-1")},
                          {QStringLiteral("xKey"), QStringLiteral("x")},
                          {QStringLiteral("yKey"), QStringLiteral("y")},
                          {QStringLiteral("angleKey"), QStringLiteral("angle")},
                          {QStringLiteral("scaleKey"), QStringLiteral("scale")},
                          {QStringLiteral("displayText"), QStringLiteral("1 模板定位")}});
    correction.insert(QStringLiteral("referenceCreated"), referenceCreated);
    QJsonObject referencePose{
        {QStringLiteral("x"), 10.0},
        {QStringLiteral("y"), 20.0},
        {QStringLiteral("angleDeg"), 0.0}
    };
    if (includeScale)
        referencePose.insert(QStringLiteral("scale"), referenceScale);
    correction.insert(QStringLiteral("referencePose"), referencePose);
    config.params.insert(QStringLiteral("positionCorrection"), correction);
    return config;
}

QJsonObject runtimeWithPose(double x,
                            double y,
                            double angleDeg,
                            double scale,
                            bool includeScale = true,
                            bool ok = true)
{
    ToolResult producer;
    producer.toolId = QStringLiteral("tpl-1");
    producer.toolType = ToolType::TemplateLocation;
    producer.success = ok;
    producer.ok = ok;
    producer.status = ok ? QStringLiteral("found") : QStringLiteral("not_found");
    producer.payload.insert(QStringLiteral("x"), x);
    producer.payload.insert(QStringLiteral("y"), y);
    producer.payload.insert(QStringLiteral("angle"), angleDeg);
    producer.payload.insert(QStringLiteral("angleDeg"), angleDeg);
    producer.payload.insert(QStringLiteral("frameId"), QStringLiteral("frame-1"));
    if (includeScale)
        producer.payload.insert(QStringLiteral("scale"), scale);
    if (ok) {
        ToolOverlay contour;
        contour.type = ToolOverlayType::Polygon;
        contour.label = QStringLiteral("match_result");
        contour.points = {QPointF(10.0, 10.0), QPointF(20.0, 10.0),
                          QPointF(20.0, 20.0)};
        producer.overlays.append(contour);
        ToolOverlay center;
        center.type = ToolOverlayType::Line;
        center.label = QStringLiteral("match_center");
        center.p1 = QPointF(x - 4.0, y);
        center.p2 = QPointF(x + 4.0, y);
        producer.overlays.append(center);
    }

    QJsonObject toolResults;
    toolResults.insert(producer.toolId, producer.toJson());
    QJsonObject context;
    context.insert(QStringLiteral("frameId"), QStringLiteral("frame-1"));
    context.insert(QStringLiteral("toolResultsById"), toolResults);
    context.insert(QStringLiteral("positionCorrectionsById"), QJsonObject());
    return context;
}

QJsonObject runtimeWithTemplateResult(bool ok = true)
{
    return runtimeWithPose(15.0, 20.0, 0.0, 1.0, true, ok);
}

ToolResult run(PositionCorrectionAdapter *adapter,
               const ToolConfig &config,
               const QJsonObject &runtimeContext)
{
    ToolRequest request;
    request.config = config;
    request.runtimeContext = runtimeContext;
    return adapter->run(request);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    PositionCorrectionAdapter adapter;

    const ToolResult success =
            run(&adapter, positionCorrectionConfig(), runtimeWithTemplateResult());
    check(success.success && success.ok && success.status == QStringLiteral("corrected"),
          "valid referencePose and runPose must produce a correction");
    check(success.payload.value(QStringLiteral("positionCorrectionApplied")).toBool(false),
          "successful correction must mark applied=true");
    check(success.payload.value(QStringLiteral("sourceId")).toString() == QStringLiteral("pc-1"),
          "successful correction must expose its stable source id");
    check(success.overlays.size() == 2
          && success.overlays.at(0).extra.value(QStringLiteral("role")).toString()
             == QStringLiteral("position_correction_match_contour")
          && success.overlays.at(1).extra.value(QStringLiteral("role")).toString()
             == QStringLiteral("position_correction_match_origin"),
          "position correction must retain producer contour and actual match origin");
    check(near(success.payload.value(QStringLiteral("deltaX")).toDouble(), 5.0)
          && near(success.payload.value(QStringLiteral("deltaY")).toDouble(), 0.0),
          "delta must describe runPose minus referencePose");
    const QJsonArray referenceToRun =
            success.payload.value(QStringLiteral("referenceToRunHomMat2D")).toArray();
    const QJsonArray runToReference =
            success.payload.value(QStringLiteral("runToReferenceHomMat2D")).toArray();
    check(referenceToRun.size() == 6 && runToReference.size() == 6,
          "HALCON transform payload must contain six homography values");
    if (referenceToRun.size() == 6 && runToReference.size() == 6) {
        check(near(referenceToRun.at(2).toDouble(), 0.0)
              && near(referenceToRun.at(5).toDouble(), 5.0)
              && near(runToReference.at(2).toDouble(), 0.0)
              && near(runToReference.at(5).toDouble(), -5.0),
              "referenceToRun must move ROI in the run direction and reverse must invert it");
    }
    check(near(success.payload.value(QStringLiteral("scaleRatio")).toDouble(), 1.0),
          "scaleRatio must default to one for an unscaled pose");

    ToolConfig referenceSourceConfig = positionCorrectionConfig();
    QJsonObject referenceSourceCorrection = referenceSourceConfig.params.value(
                QStringLiteral("positionCorrection")).toObject();
    QJsonObject referenceRunPoseSource = referenceSourceCorrection.value(
                QStringLiteral("runPoseSource")).toObject();
    referenceRunPoseSource.insert(
                QStringLiteral("producerId"),
                QStringLiteral("reference.positionCorrection"));
    referenceSourceCorrection.insert(QStringLiteral("runPoseSource"),
                                     referenceRunPoseSource);
    referenceSourceConfig.params.insert(QStringLiteral("positionCorrection"),
                                        referenceSourceCorrection);
    QJsonObject referenceSourceContext = runtimeWithTemplateResult();
    QJsonObject referenceSourceResults = referenceSourceContext.value(
                QStringLiteral("toolResultsById")).toObject();
    QJsonObject referenceProducer = referenceSourceResults.take(
                QStringLiteral("tpl-1")).toObject();
    referenceProducer.insert(QStringLiteral("toolId"),
                             QStringLiteral("reference.positionCorrection"));
    referenceProducer.insert(QStringLiteral("toolType"),
                             toolTypeToString(ToolType::PositionCorrection));
    referenceSourceResults.insert(QStringLiteral("reference.positionCorrection"),
                                  referenceProducer);
    referenceSourceContext.insert(QStringLiteral("toolResultsById"),
                                  referenceSourceResults);
    const ToolResult referenceSourceResult = run(
                &adapter, referenceSourceConfig, referenceSourceContext);
    check(referenceSourceResult.success && referenceSourceResult.ok,
          "tool-level correction must accept the valid reference correction pose source");

    const ToolResult scaled = run(
                &adapter,
                positionCorrectionConfig(true, 0.8),
                runtimeWithPose(30.0, 40.0, 90.0, 0.96));
    check(scaled.success && scaled.ok,
          "translation, rotation, and scale must produce a correction");
    check(near(scaled.payload.value(QStringLiteral("referenceScale")).toDouble(), 0.8)
          && near(scaled.payload.value(QStringLiteral("runScale")).toDouble(), 0.96)
          && near(scaled.payload.value(QStringLiteral("scaleRatio")).toDouble(), 1.2),
          "scale payload must expose reference, run, and ratio values");
    const QJsonArray scaledForward =
            scaled.payload.value(QStringLiteral("referenceToRunHomMat2D")).toArray();
    const QJsonArray scaledInverse =
            scaled.payload.value(QStringLiteral("runToReferenceHomMat2D")).toArray();
    const QPointF mappedAnchor = transformPoint(scaledForward, QPointF(10.0, 20.0));
    const QPointF mappedRadius = transformPoint(scaledForward, QPointF(20.0, 20.0));
    check(near(mappedAnchor.x(), 30.0) && near(mappedAnchor.y(), 40.0),
          "similarity transform must keep the pose anchor correspondence");
    check(near(std::hypot(mappedRadius.x() - mappedAnchor.x(),
                          mappedRadius.y() - mappedAnchor.y()), 12.0),
          "similarity transform must scale distance from the anchor");
    const QPointF restoredRadius = transformPoint(scaledInverse, mappedRadius);
    check(near(restoredRadius.x(), 20.0) && near(restoredRadius.y(), 20.0),
          "runToReference must invert the scaled forward transform");

    const ToolResult legacyScale = run(
                &adapter,
                positionCorrectionConfig(true, 1.0, false),
                runtimeWithPose(15.0, 20.0, 0.0, 1.0, false));
    check(legacyScale.success
          && near(legacyScale.payload.value(QStringLiteral("scaleRatio")).toDouble(), 1.0),
          "legacy poses without scale must remain compatible with scale=1");

    const ToolResult invalidRunScale = run(
                &adapter,
                positionCorrectionConfig(),
                runtimeWithPose(15.0, 20.0, 0.0, 0.0));
    check(!invalidRunScale.success
          && invalidRunScale.status == QStringLiteral("invalid_pose_scale"),
          "non-positive run scale must fail explicitly");

    const ToolResult invalidReferenceScale = run(
                &adapter,
                positionCorrectionConfig(true, -1.0),
                runtimeWithTemplateResult());
    check(!invalidReferenceScale.success
          && invalidReferenceScale.status == QStringLiteral("invalid_pose_scale"),
          "non-positive reference scale must fail explicitly");

    ToolConfig invalidReferencePoseConfig = positionCorrectionConfig();
    QJsonObject invalidReferenceCorrection =
            invalidReferencePoseConfig.params.value(
                QStringLiteral("positionCorrection")).toObject();
    QJsonObject invalidReferencePose = invalidReferenceCorrection.value(
                QStringLiteral("referencePose")).toObject();
    invalidReferencePose.insert(QStringLiteral("x"), QStringLiteral("not-a-number"));
    invalidReferenceCorrection.insert(QStringLiteral("referencePose"),
                                      invalidReferencePose);
    invalidReferencePoseConfig.params.insert(
                QStringLiteral("positionCorrection"),
                invalidReferenceCorrection);
    const ToolResult invalidReferencePoseResult = run(
                &adapter,
                invalidReferencePoseConfig,
                runtimeWithTemplateResult());
    check(!invalidReferencePoseResult.success
          && invalidReferencePoseResult.status
          == QStringLiteral("invalid_reference_pose"),
          "non-numeric reference x/y/angle must fail explicitly");

    QJsonObject invalidRunContext = runtimeWithTemplateResult();
    QJsonObject invalidRunResults = invalidRunContext.value(
                QStringLiteral("toolResultsById")).toObject();
    QJsonObject invalidRunProducer = invalidRunResults.value(
                QStringLiteral("tpl-1")).toObject();
    QJsonObject invalidRunPayload = invalidRunProducer.value(
                QStringLiteral("payload")).toObject();
    invalidRunPayload.insert(QStringLiteral("x"), QStringLiteral("not-a-number"));
    invalidRunProducer.insert(QStringLiteral("payload"), invalidRunPayload);
    invalidRunResults.insert(QStringLiteral("tpl-1"), invalidRunProducer);
    invalidRunContext.insert(QStringLiteral("toolResultsById"), invalidRunResults);
    const ToolResult invalidRunPoseResult = run(
                &adapter,
                positionCorrectionConfig(),
                invalidRunContext);
    check(!invalidRunPoseResult.success
          && invalidRunPoseResult.status == QStringLiteral("invalid_run_pose"),
          "non-numeric run x/y/angle must fail explicitly");

    QJsonObject staleRunContext = runtimeWithTemplateResult();
    QJsonObject staleRunResults = staleRunContext.value(
                QStringLiteral("toolResultsById")).toObject();
    QJsonObject staleRunProducer = staleRunResults.value(
                QStringLiteral("tpl-1")).toObject();
    QJsonObject staleRunPayload = staleRunProducer.value(
                QStringLiteral("payload")).toObject();
    staleRunPayload.insert(QStringLiteral("frameId"), QStringLiteral("frame-0"));
    staleRunProducer.insert(QStringLiteral("payload"), staleRunPayload);
    staleRunResults.insert(QStringLiteral("tpl-1"), staleRunProducer);
    staleRunContext.insert(QStringLiteral("toolResultsById"), staleRunResults);
    const ToolResult staleRunPoseResult = run(
                &adapter,
                positionCorrectionConfig(),
                staleRunContext);
    check(!staleRunPoseResult.success
          && staleRunPoseResult.status == QStringLiteral("source_frame_mismatch"),
          "pose source from another frame must be rejected");

    const ToolResult missingSource =
            run(&adapter, positionCorrectionConfig(), QJsonObject());
    check(!missingSource.success
          && missingSource.status == QStringLiteral("source_unavailable"),
          "missing producer result must return source_unavailable");

    const ToolResult sourceNg =
            run(&adapter, positionCorrectionConfig(), runtimeWithTemplateResult(false));
    check(!sourceNg.success && sourceNg.status == QStringLiteral("not_found")
          && !sourceNg.message.trimmed().isEmpty(),
          "unmatched producer result must retain not_found and an explicit message");

    const ToolResult missingReference =
            run(&adapter, positionCorrectionConfig(false), runtimeWithTemplateResult());
    check(!missingReference.success
          && missingReference.status == QStringLiteral("reference_pose_missing"),
          "missing referencePose must return reference_pose_missing");

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "position_correction_backend_smoke: all checks passed" << std::endl;
    return 0;
}
