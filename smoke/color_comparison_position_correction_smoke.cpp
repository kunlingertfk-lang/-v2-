#include "algorithms/recognition/ColorComparisonModel.h"
#include "tooladapters/ColorComparisonAdapter.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolEngine.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

QJsonObject rect(double x, double y, double width, double height)
{
    return {{QStringLiteral("x"), x}, {QStringLiteral("y"), y},
            {QStringLiteral("width"), width}, {QStringLiteral("height"), height}};
}

QJsonObject inputMetadata()
{
    return {{QStringLiteral("colorMode"), QStringLiteral("color")},
            {QStringLiteral("pixelFormat"), QStringLiteral("BGR8")},
            {QStringLiteral("bitDepth"), 8}};
}

const ToolOverlay *overlayWithRole(const ToolResult &result, const QString &role)
{
    for (const ToolOverlay &overlay : result.overlays) {
        if (overlay.extra.value(QStringLiteral("role")).toString() == role)
            return &overlay;
    }
    return nullptr;
}

QRectF pointBounds(const QVector<QPointF> &points)
{
    if (points.isEmpty())
        return QRectF();
    double minX = std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();
    for (const QPointF &point : points) {
        minX = std::min(minX, point.x());
        minY = std::min(minY, point.y());
        maxX = std::max(maxX, point.x());
        maxY = std::max(maxY, point.y());
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

ToolConfig makeConfig(bool correctionEnabled)
{
    ToolConfig config;
    config.toolId = QStringLiteral("color-1");
    config.toolName = QStringLiteral("ColorComparison");
    config.displayName = QStringLiteral("Color Comparison");
    config.toolType = ToolType::ColorComparison;
    config.category = ToolCategory::Recognition;
    config.enabled = true;
    config.roiNormalized = QRectF(0.1, 0.1, 0.2, 0.2);
    config.judgeRule = {{QStringLiteral("mode"), QStringLiteral("min_score")},
                        {QStringLiteral("minScore"), 80}};
    config.params.insert(QStringLiteral("positionCorrectionSourceId"),
                         QStringLiteral("pc-1"));
    config.params.insert(QStringLiteral("colorComparison"), QJsonObject{
        {QStringLiteral("version"), 2},
        {QStringLiteral("templateRegionMode"), QStringLiteral("custom")},
        {QStringLiteral("templateRoiNormalized"), rect(0.1, 0.1, 0.2, 0.2)},
        {QStringLiteral("templateMaskPolygon"), QJsonArray()},
        {QStringLiteral("model"), colorComparisonModelToJson(ColorComparisonModelV2())},
        {QStringLiteral("detectRegionType"), QStringLiteral("rectangle")},
        {QStringLiteral("detectRoiNormalized"), rect(0.1, 0.1, 0.2, 0.2)},
        {QStringLiteral("detectMaskPolygon"), QJsonArray()},
        {QStringLiteral("comparison"), QJsonObject{
             {QStringLiteral("sensitivity"), QStringLiteral("medium")},
             {QStringLiteral("brightnessCompensation"), false}}},
        {QStringLiteral("positionCorrection"), QJsonObject{
             {QStringLiteral("enabled"), correctionEnabled},
             {QStringLiteral("showMatchContour"), true},
             {QStringLiteral("sourceId"), QStringLiteral("pc-1")},
             {QStringLiteral("interfaceVersion"), 1}}}
    });
    return config;
}

QJsonObject runtimeContext(const QJsonArray &matrix, bool includeContour = true)
{
    ToolResult correction;
    correction.toolId = QStringLiteral("pc-1");
    correction.toolType = ToolType::PositionCorrection;
    correction.success = true;
    correction.ok = true;
    correction.status = QStringLiteral("corrected");
    correction.payload = {
        {QStringLiteral("sourceId"), QStringLiteral("pc-1")},
        {QStringLiteral("positionCorrectionApplied"), true},
        {QStringLiteral("referenceToRunHomMat2D"), matrix}
    };
    if (includeContour) {
        ToolOverlay contour;
        contour.type = ToolOverlayType::Polygon;
        contour.label = QStringLiteral("match_result");
        contour.points = {QPointF(30.0, 10.0), QPointF(50.0, 10.0),
                          QPointF(50.0, 30.0), QPointF(30.0, 30.0)};
        correction.overlays.append(contour);
    }
    ToolOverlay origin;
    origin.type = ToolOverlayType::Line;
    origin.label = QStringLiteral("match_center");
    origin.p1 = QPointF(36.0, 20.0);
    origin.p2 = QPointF(44.0, 20.0);
    correction.overlays.append(origin);
    return {
        {QStringLiteral("input"), inputMetadata()},
        {QStringLiteral("referenceInput"), inputMetadata()},
        {QStringLiteral("positionCorrectionsById"), QJsonObject{
             {QStringLiteral("pc-1"), correction.toJson()}}}
    };
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    cv::Mat reference(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));
    reference(cv::Rect(10, 10, 20, 20)).setTo(cv::Scalar(0, 0, 255));
    cv::Mat runImage(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));
    runImage(cv::Rect(30, 10, 20, 20)).setTo(cv::Scalar(0, 0, 255));

    ColorComparisonAdapter adapter;
    ToolConfig config = makeConfig(true);
    ToolRequest buildRequest;
    buildRequest.config = config;
    buildRequest.referenceImage = reference;
    buildRequest.runtimeContext = {{QStringLiteral("referenceInput"), inputMetadata()}};
    const ColorComparisonTemplateBuildResult built =
            adapter.buildTemplateModel(buildRequest);
    if (!built.success) {
        std::cerr << "model build failed: " << built.status.toStdString()
                  << " " << built.message.toStdString() << std::endl;
        return 1;
    }
    QJsonObject params = config.params;
    QJsonObject color = params.value(QStringLiteral("colorComparison")).toObject();
    color.insert(QStringLiteral("model"), colorComparisonModelToJson(built.model));
    params.insert(QStringLiteral("colorComparison"), color);
    config.params = params;

    ToolRequest request;
    request.config = config;
    request.image = runImage;
    request.referenceImage = reference;
    request.runtimeContext = runtimeContext(QJsonArray{1.0, 0.0, 0.0,
                                                       0.0, 1.0, 20.0});
    const ToolResult corrected = adapter.run(request);
    if (!corrected.success || !corrected.ok || corrected.score < 95.0) {
        std::cerr << "corrected comparison failed: "
                  << corrected.status.toStdString() << " "
                  << corrected.message.toStdString() << " score="
                  << corrected.score << std::endl;
        return 1;
    }
    bool hasCorrectionContour = false;
    bool hasCorrectionOrigin = false;
    for (const ToolOverlay &overlay : corrected.overlays) {
        if (overlay.extra.value(QStringLiteral("role")).toString()
                == QStringLiteral("position_correction_match_contour")) {
            hasCorrectionContour = true;
        }
        if (overlay.extra.value(QStringLiteral("role")).toString()
                == QStringLiteral("position_correction_match_origin")) {
            hasCorrectionOrigin = true;
        }
    }
    if (!hasCorrectionContour || !hasCorrectionOrigin) {
        std::cerr << "enabled correction display did not expose contour and match origin"
                  << std::endl;
        return 1;
    }

    cv::Mat scaledRunImage(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));
    scaledRunImage(cv::Rect(4, 4, 32, 32)).setTo(cv::Scalar(0, 0, 255));
    ToolRequest scaledRequest = request;
    scaledRequest.image = scaledRunImage;
    scaledRequest.runtimeContext = runtimeContext(
                QJsonArray{1.5, 0.0, -10.0,
                           0.0, 1.5, -10.0});
    const ToolResult scaledRect = adapter.run(scaledRequest);
    const ToolOverlay *scaledRectOverlay =
            overlayWithRole(scaledRect, QStringLiteral("detect_roi"));
    const QRectF scaledRectBounds = scaledRectOverlay
            ? pointBounds(scaledRectOverlay->points) : QRectF();
    if (!scaledRect.success || !scaledRect.ok || scaledRect.score < 95.0
            || !scaledRectOverlay
            || scaledRectOverlay->type != ToolOverlayType::Polygon
            || std::fabs(scaledRectBounds.width() - 30.0) > 0.01
            || std::fabs(scaledRectBounds.height() - 30.0) > 0.01) {
        std::cerr << "scaled correction did not resize rectangle ROI and sampled region together"
                  << std::endl;
        return 1;
    }

    QJsonObject circleParams = config.params;
    QJsonObject circleColor = circleParams.value(
                QStringLiteral("colorComparison")).toObject();
    circleColor.insert(QStringLiteral("detectRegionType"), QStringLiteral("circle"));
    circleColor.insert(QStringLiteral("detectCircleNormalized"), QJsonObject{
                           {QStringLiteral("center"), QJsonObject{
                                {QStringLiteral("x"), 0.2},
                                {QStringLiteral("y"), 0.2}}},
                           {QStringLiteral("radius"), 0.1}});
    circleParams.insert(QStringLiteral("colorComparison"), circleColor);
    ToolRequest circleRequest = scaledRequest;
    circleRequest.config.params = circleParams;
    const ToolResult scaledCircle = adapter.run(circleRequest);
    const ToolOverlay *scaledCircleOverlay =
            overlayWithRole(scaledCircle, QStringLiteral("detect_roi"));
    if (!scaledCircle.success || !scaledCircle.ok || scaledCircle.score < 95.0
            || !scaledCircleOverlay
            || scaledCircleOverlay->type != ToolOverlayType::Circle
            || std::fabs(scaledCircleOverlay->radius - 15.0) > 0.01) {
        std::cerr << "scaled correction did not resize circle ROI radius, radius="
                  << (scaledCircleOverlay ? scaledCircleOverlay->radius : -1.0)
                  << std::endl;
        return 1;
    }

    QJsonObject hiddenParams = config.params;
    QJsonObject hiddenColor = hiddenParams.value(
                QStringLiteral("colorComparison")).toObject();
    QJsonObject hiddenCorrection = hiddenColor.value(
                QStringLiteral("positionCorrection")).toObject();
    hiddenCorrection.insert(QStringLiteral("showMatchContour"), false);
    hiddenColor.insert(QStringLiteral("positionCorrection"), hiddenCorrection);
    hiddenParams.insert(QStringLiteral("colorComparison"), hiddenColor);
    ToolRequest hiddenRequest = request;
    hiddenRequest.config.params = hiddenParams;
    const ToolResult hiddenContour = adapter.run(hiddenRequest);
    bool hiddenStillHasOrigin = false;
    for (const ToolOverlay &overlay : hiddenContour.overlays) {
        if (overlay.label == QStringLiteral("match_result")) {
            std::cerr << "disabled contour display still exposed match_result"
                      << std::endl;
            return 1;
        }
        if (overlay.extra.value(QStringLiteral("role")).toString()
                == QStringLiteral("position_correction_match_origin")) {
            hiddenStillHasOrigin = true;
        }
    }
    if (!hiddenStillHasOrigin) {
        std::cerr << "hiding the contour also hid the actual match origin"
                  << std::endl;
        return 1;
    }

    ToolRequest unavailableContourRequest = request;
    unavailableContourRequest.runtimeContext = runtimeContext(
                QJsonArray{1.0, 0.0, 0.0, 0.0, 1.0, 20.0}, false);
    const ToolResult unavailableContour = adapter.run(unavailableContourRequest);
    if (!unavailableContour.success
            || unavailableContour.payload.value(
                QStringLiteral("positionCorrectionContourStatus")).toString()
                != QStringLiteral("position_correction_contour_unavailable")
            || unavailableContour.message.trimmed().isEmpty()) {
        std::cerr << "missing display contour did not produce an explicit notice"
                  << std::endl;
        return 1;
    }
    const QJsonObject correctionPayload = corrected.payload
            .value(QStringLiteral("positionCorrection")).toObject();
    if (!correctionPayload.value(QStringLiteral("applied")).toBool(false)
            || correctionPayload.value(QStringLiteral("sourceId")).toString()
                    != QStringLiteral("pc-1")) {
        std::cerr << "successful run did not report the applied source" << std::endl;
        return 1;
    }

    request.runtimeContext = runtimeContext(QJsonArray{1.0, 0.0, 0.0,
                                                       0.0, 1.0, -20.0});
    const ToolResult reverseDirection = adapter.run(request);
    if (!reverseDirection.success || reverseDirection.ok
            || reverseDirection.score >= 50.0) {
        std::cerr << "reverse matrix must not find the translated target, score="
                  << reverseDirection.score << std::endl;
        return 1;
    }

    request.runtimeContext = runtimeContext(QJsonArray{1.0, 0.0, 0.0,
                                                       0.0, 1.0});
    const ToolResult malformed = adapter.run(request);
    if (malformed.success
            || malformed.status
                    != QStringLiteral("invalid_position_correction_matrix")) {
        std::cerr << "malformed matrix must fail explicitly, got "
                  << malformed.status.toStdString() << std::endl;
        return 1;
    }

    request.runtimeContext = {{QStringLiteral("input"), inputMetadata()},
                              {QStringLiteral("referenceInput"), inputMetadata()}};
    const ToolResult missing = adapter.run(request);
    if (missing.success
            || missing.status != QStringLiteral("position_correction_source_missing")) {
        std::cerr << "missing source must fail explicitly, got "
                  << missing.status.toStdString() << std::endl;
        return 1;
    }

    ToolResult failedCorrection;
    failedCorrection.toolId = QStringLiteral("pc-1");
    failedCorrection.toolType = ToolType::PositionCorrection;
    failedCorrection.success = true;
    failedCorrection.ok = false;
    failedCorrection.status = QStringLiteral("not_found");
    failedCorrection.message = QStringLiteral(
                "TemplateLocation: no match reached the minimum score");
    failedCorrection.payload = {
        {QStringLiteral("sourceId"), QStringLiteral("pc-1")},
        {QStringLiteral("positionCorrectionApplied"), false},
        {QStringLiteral("positionCorrectionReason"), QStringLiteral("not_found")}
    };
    request.runtimeContext = {
        {QStringLiteral("input"), inputMetadata()},
        {QStringLiteral("referenceInput"), inputMetadata()},
        {QStringLiteral("positionCorrectionsById"), QJsonObject{
             {QStringLiteral("pc-1"), failedCorrection.toJson()}}}
    };
    const ToolResult notFound = adapter.run(request);
    if (notFound.success
            || notFound.status
                    != QStringLiteral("position_correction_match_not_found")
            || !notFound.message.contains(QStringLiteral("minimum score"))) {
        std::cerr << "executed but unmatched correction must retain locator failure, got "
                  << notFound.status.toStdString() << " "
                  << notFound.message.toStdString() << std::endl;
        return 1;
    }

    QJsonObject engineParams = config.params;
    engineParams.insert(QStringLiteral("positionCorrectionSourceId"),
                        PositionCorrection::defaultSourceId());
    QJsonObject engineColor = engineParams.value(
                QStringLiteral("colorComparison")).toObject();
    QJsonObject engineCorrection = engineColor.value(
                QStringLiteral("positionCorrection")).toObject();
    engineCorrection.insert(QStringLiteral("sourceId"),
                            PositionCorrection::defaultSourceId());
    engineColor.insert(QStringLiteral("positionCorrection"), engineCorrection);
    engineParams.insert(QStringLiteral("colorComparison"), engineColor);
    ToolConfig engineConfig = config;
    engineConfig.params = engineParams;

    ReferencePositionCorrectionConfig referenceCorrection;
    referenceCorrection.enabled = true;
    referenceCorrection.templateRegionType = QStringLiteral("rectangle");
    referenceCorrection.templateRoiNormalized = QRectF(0.05, 0.05, 0.3, 0.3);
    referenceCorrection.originMode = QStringLiteral("custom");
    referenceCorrection.customOriginNormalized = QPointF(0.5, 0.5);
    referenceCorrection.referenceCreated = true;
    referenceCorrection.referencePose = QJsonObject{
        {QStringLiteral("x"), 50.0},
        {QStringLiteral("y"), 50.0},
        {QStringLiteral("angleDeg"), 0.0}
    };
    referenceCorrection.modelCacheKey =
            QStringLiteral("reference.positionCorrection.color_dialog_smoke");

    ToolEngine engine;
    engine.registerAdapter(&adapter);
    QJsonObject engineContext{
        {QStringLiteral("input"), inputMetadata()},
        {QStringLiteral("referenceInput"), inputMetadata()},
        {QStringLiteral("referencePositionCorrection"),
         PositionCorrection::referenceToJson(referenceCorrection)}
    };
    const QVector<ToolResult> engineResults = engine.runTools(
                QVector<ToolConfig>{engineConfig}, reference, reference,
                engineContext);
    if (engineResults.size() != 1 || !engineResults.first().success
            || engineResults.first().status
                    == QStringLiteral("position_correction_source_missing")) {
        std::cerr << "engine-backed dialog path did not provide reference correction: "
                  << (engineResults.isEmpty()
                      ? std::string("no result")
                      : engineResults.first().status.toStdString())
                  << std::endl;
        return 1;
    }
    const QJsonObject engineApplied = engineResults.first().payload
            .value(QStringLiteral("positionCorrection")).toObject();
    if (!engineApplied.value(QStringLiteral("applied")).toBool(false)
            || engineApplied.value(QStringLiteral("sourceId")).toString()
                    != PositionCorrection::defaultSourceId()) {
        std::cerr << "engine-backed dialog path did not apply reference correction"
                  << std::endl;
        return 1;
    }
    bool engineContourVisible = false;
    for (const ToolOverlay &overlay : engineResults.first().overlays) {
        if (overlay.extra.value(QStringLiteral("role")).toString()
                == QStringLiteral("position_correction_match_contour")) {
            engineContourVisible = true;
        }
    }
    if (!engineContourVisible) {
        std::cerr << "engine-backed reference correction did not expose its matched contour"
                  << std::endl;
        return 1;
    }

    std::cout << "color_comparison_position_correction_smoke: all checks passed"
              << std::endl;
    return 0;
}
