#include "tooladapters/CirclePresenceAdapter.h"
#include "toolcore/ToolAdapter.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolResult.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPolygonF>

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <iostream>

namespace {

ToolConfig circleConfig(bool correctionEnabled)
{
    ToolConfig config;
    config.toolId = QStringLiteral("circle-1");
    config.toolType = ToolType::CirclePresence;
    config.category = ToolCategory::Presence;
    config.toolName = QStringLiteral("CirclePresence");
    config.enabled = true;
    config.roiNormalized = QRectF(0.1, 0.1, 0.2, 0.2);
    config.params = {
        {QStringLiteral("detectRegionType"), QStringLiteral("rect")},
        {QStringLiteral("enablePositionCorrection"), correctionEnabled},
        {QStringLiteral("positionCorrectionSource"), QStringLiteral("pc-1")},
        {QStringLiteral("positionCorrectionSourceId"), QStringLiteral("pc-1")},
        {QStringLiteral("sensitivity"), 60},
        {QStringLiteral("roundness"), 60},
        {QStringLiteral("edgePolarity"), QStringLiteral("white_to_black")},
        {QStringLiteral("edgeType"), QStringLiteral("strongest")},
        {QStringLiteral("showPositionCorrectionMatchContour"), true},
        {QStringLiteral("existOk"), true}
    };
    return config;
}

ToolConfig polygonRoiConfig(bool correctionEnabled)
{
    ToolConfig config = circleConfig(correctionEnabled);
    config.params.insert(QStringLiteral("detectRegionType"),
                         QStringLiteral("polygon"));
    config.params.insert(
            QStringLiteral("detectPolygonNormalized"),
            QJsonArray{
                QJsonObject{{QStringLiteral("x"), 0.1},
                            {QStringLiteral("y"), 0.1}},
                QJsonObject{{QStringLiteral("x"), 0.3},
                            {QStringLiteral("y"), 0.1}},
                QJsonObject{{QStringLiteral("x"), 0.3},
                            {QStringLiteral("y"), 0.3}},
                QJsonObject{{QStringLiteral("x"), 0.1},
                            {QStringLiteral("y"), 0.3}}
            });
    return config;
}

ToolConfig circleRoiConfig(bool correctionEnabled)
{
    ToolConfig config = circleConfig(correctionEnabled);
    config.params.insert(QStringLiteral("detectRegionType"),
                         QStringLiteral("circle"));
    config.params.insert(
            QStringLiteral("detectCircleNormalized"),
            QJsonObject{
                {QStringLiteral("center"),
                 QJsonObject{{QStringLiteral("x"), 0.2},
                             {QStringLiteral("y"), 0.2}}},
                {QStringLiteral("radius"), 0.1},
                {QStringLiteral("boundingRect"),
                 QJsonObject{{QStringLiteral("x"), 0.1},
                             {QStringLiteral("y"), 0.1},
                             {QStringLiteral("width"), 0.2},
                             {QStringLiteral("height"), 0.2}}}
            });
    return config;
}

QJsonObject correctionContext(const QJsonArray &matrix =
                              QJsonArray{1.0, 0.0, 0.0,
                                         0.0, 1.0, 30.0})
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
        {QStringLiteral("referenceToRunHomMat2D"), matrix},
        {QStringLiteral("referenceScale"), 1.0},
        {QStringLiteral("runScale"), 1.0},
        {QStringLiteral("scaleRatio"), 1.0}
    };
    ToolOverlay origin;
    origin.type = ToolOverlayType::Line;
    origin.label = QStringLiteral("match_center");
    origin.p1 = QPointF(38.0, 20.0);
    origin.p2 = QPointF(46.0, 20.0);
    correction.overlays.append(origin);
    return {
        {QStringLiteral("positionCorrectionsById"),
         QJsonObject{{QStringLiteral("pc-1"), correction.toJson()}}}
    };
}

const ToolOverlay *detectOverlay(const ToolResult &result)
{
    for (const ToolOverlay &overlay : result.overlays) {
        if (overlay.extra.value(QStringLiteral("role")).toString()
                == QStringLiteral("detect_roi")
                || overlay.label == QStringLiteral("detect_roi")) {
            return &overlay;
        }
    }
    return nullptr;
}

class CorrectionSourceAdapter final : public ToolAdapter
{
public:
    bool supports(ToolType type) const override
    {
        return type == ToolType::PositionCorrection;
    }

    ToolResult run(const ToolRequest &request) override
    {
        ToolResult result;
        result.toolId = request.config.toolId;
        result.toolType = ToolType::PositionCorrection;
        result.success = true;
        result.ok = true;
        result.status = QStringLiteral("corrected");
        result.payload = {
            {QStringLiteral("sourceId"), request.config.toolId},
            {QStringLiteral("positionCorrectionApplied"), true},
            {QStringLiteral("referenceToRunHomMat2D"),
             QJsonArray{1.0, 0.0, 0.0, 0.0, 1.0, 30.0}},
            {QStringLiteral("referenceScale"), 1.0},
            {QStringLiteral("runScale"), 1.0},
            {QStringLiteral("scaleRatio"), 1.0}
        };
        ToolOverlay contour;
        contour.type = ToolOverlayType::Polygon;
        contour.label = QStringLiteral("match_result");
        contour.points = {QPointF(40.0, 10.0), QPointF(50.0, 10.0),
                          QPointF(50.0, 20.0)};
        result.overlays.append(contour);
        ToolOverlay origin;
        origin.type = ToolOverlayType::Line;
        origin.label = QStringLiteral("match_center");
        origin.p1 = QPointF(40.0, 15.0);
        origin.p2 = QPointF(48.0, 15.0);
        result.overlays.append(origin);
        return result;
    }
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    cv::Mat image(100, 100, CV_8UC1, cv::Scalar(0));
    cv::circle(image,
               cv::Point(50, 20),
               5,
               cv::Scalar(255),
               cv::FILLED);

    CirclePresenceAdapter adapter;
    ToolRequest correctedRequest;
    correctedRequest.config = circleConfig(true);
    correctedRequest.image = image;
    correctedRequest.runtimeContext = correctionContext();
    const ToolResult corrected = adapter.run(correctedRequest);
    const ToolOverlay *roi = detectOverlay(corrected);
    bool hasMatchOrigin = false;
    for (const ToolOverlay &overlay : corrected.overlays) {
        if (overlay.extra.value(QStringLiteral("role")).toString()
                == QStringLiteral("position_correction_match_origin")) {
            hasMatchOrigin = true;
        }
    }
    if (!corrected.success || !corrected.ok || corrected.count != 1
            || !corrected.payload.value(
                QStringLiteral("positionCorrectionApplied")).toBool(false)
            || !roi || roi->type != ToolOverlayType::Polygon
            || !hasMatchOrigin) {
        std::cerr << "Circle did not consume the common corrected ROI: "
                  << corrected.status.toStdString() << " "
                  << corrected.message.toStdString()
                  << " count=" << corrected.count << std::endl;
        return 1;
    }

    ToolRequest polygonRequest = correctedRequest;
    polygonRequest.config = polygonRoiConfig(true);
    const ToolResult polygonCorrected = adapter.run(polygonRequest);
    const ToolOverlay *polygonRoi = detectOverlay(polygonCorrected);
    if (!polygonCorrected.success
            || !polygonCorrected.ok
            || polygonCorrected.count != 1
            || !polygonRoi
            || polygonRoi->type != ToolOverlayType::Polygon) {
        std::cerr << "Circle polygon ROI was not corrected with its HALCON Region"
                  << " status=" << polygonCorrected.status.toStdString()
                  << " count=" << polygonCorrected.count
                  << " payload="
                  << QJsonDocument(polygonCorrected.payload)
                     .toJson(QJsonDocument::Compact).toStdString()
                  << std::endl;
        return 1;
    }

    ToolRequest circleRoiRequest = correctedRequest;
    circleRoiRequest.config = circleRoiConfig(true);
    const ToolResult circleRoiCorrected = adapter.run(circleRoiRequest);
    const ToolOverlay *circleRoi = detectOverlay(circleRoiCorrected);
    if (!circleRoiCorrected.success
            || !circleRoiCorrected.ok
            || circleRoiCorrected.count != 1
            || !circleRoi
            || circleRoi->type != ToolOverlayType::Circle
            || std::fabs(circleRoi->center.x() - 50.0) > 0.01
            || std::fabs(circleRoi->center.y() - 20.0) > 0.01) {
        std::cerr << "Circle-shaped ROI was not corrected with its HALCON Region"
                  << " status=" << circleRoiCorrected.status.toStdString()
                  << " count=" << circleRoiCorrected.count << std::endl;
        return 1;
    }

    cv::Mat rotatedImage(100, 100, CV_8UC1, cv::Scalar(0));
    cv::circle(rotatedImage,
               cv::Point(40, 20),
               5,
               cv::Scalar(255),
               cv::FILLED);
    ToolRequest rotatedRequest = correctedRequest;
    rotatedRequest.image = rotatedImage;
    rotatedRequest.runtimeContext = correctionContext(
            QJsonArray{0.0, -1.0, 40.0,
                       1.0, 0.0, 20.0});
    const ToolResult rotated = adapter.run(rotatedRequest);
    if (!rotated.success || !rotated.ok || rotated.count != 1) {
        std::cerr << "Circle corrected Region did not follow rotation"
                  << " status=" << rotated.status.toStdString()
                  << " message=" << rotated.message.toStdString()
                  << " count=" << rotated.count << std::endl;
        return 1;
    }

    CorrectionSourceAdapter correctionAdapter;
    ToolEngine chainEngine;
    chainEngine.registerAdapter(&correctionAdapter);
    chainEngine.registerAdapter(&adapter);
    ToolConfig correctionTool;
    correctionTool.toolId = QStringLiteral("pc-1");
    correctionTool.toolType = ToolType::PositionCorrection;
    correctionTool.category = ToolCategory::Location;
    correctionTool.enabled = true;
    const QVector<ToolResult> chainResults = chainEngine.runTools(
                QVector<ToolConfig>{correctionTool, circleConfig(true)},
                image);
    bool chainHasOrigin = false;
    if (chainResults.size() == 2) {
        for (const ToolOverlay &overlay : chainResults.last().overlays) {
            if (overlay.extra.value(QStringLiteral("role")).toString()
                    == QStringLiteral("position_correction_match_origin")) {
                chainHasOrigin = true;
            }
        }
    }
    if (chainResults.size() != 2
            || !chainResults.last().success
            || !chainResults.last().ok
            || !chainHasOrigin) {
        std::cerr << "Circle did not consume a preceding correction through ToolEngine"
                  << std::endl;
        return 1;
    }

    ToolRequest uncorrectedRequest = correctedRequest;
    uncorrectedRequest.config = circleConfig(false);
    uncorrectedRequest.runtimeContext = QJsonObject();
    const ToolResult uncorrected = adapter.run(uncorrectedRequest);
    if (!uncorrected.success || uncorrected.count != 0) {
        std::cerr << "Circle baseline ROI unexpectedly found the shifted target"
                  << std::endl;
        return 1;
    }

    cv::Mat scaledImage(100, 100, CV_8UC1, cv::Scalar(0));
    cv::circle(scaledImage,
               cv::Point(55, 25),
               8,
               cv::Scalar(255),
               cv::FILLED);
    ToolRequest scaledRequest = correctedRequest;
    scaledRequest.image = scaledImage;
    scaledRequest.runtimeContext = correctionContext(
                QJsonArray{1.5, 0.0, -5.0,
                           0.0, 1.5, 25.0});
    const ToolResult scaled = adapter.run(scaledRequest);
    const ToolOverlay *scaledRoi = detectOverlay(scaled);
    const QRectF scaledBounds = scaledRoi
            && scaledRoi->type == ToolOverlayType::Polygon
            ? QPolygonF(scaledRoi->points).boundingRect() : QRectF();
    if (!scaled.success || !scaled.ok || scaled.count != 1
            || !scaledRoi
            || std::fabs(scaledBounds.width() - 31.5) > 0.01
            || std::fabs(scaledBounds.height() - 31.5) > 0.01) {
        std::cerr << "Circle corrected Region and overlay did not scale together"
                  << " status=" << scaled.status.toStdString()
                  << " message=" << scaled.message.toStdString()
                  << " count=" << scaled.count
                  << " bounds=" << scaledBounds.x() << ","
                  << scaledBounds.y() << " "
                  << scaledBounds.width() << "x" << scaledBounds.height()
                  << std::endl;
        return 1;
    }

    ToolRequest scaledCircleRoiRequest = scaledRequest;
    scaledCircleRoiRequest.config = circleRoiConfig(true);
    const ToolResult scaledCircleRoi =
            adapter.run(scaledCircleRoiRequest);
    const ToolOverlay *scaledCircleOverlay =
            detectOverlay(scaledCircleRoi);
    if (!scaledCircleRoi.success
            || !scaledCircleRoi.ok
            || scaledCircleRoi.count != 1
            || !scaledCircleOverlay
            || scaledCircleOverlay->type != ToolOverlayType::Circle
            || std::fabs(scaledCircleOverlay->radius - 15.0) > 0.01) {
        std::cerr << "Circle ROI radius and HALCON Region did not scale together"
                  << " status=" << scaledCircleRoi.status.toStdString()
                  << " count=" << scaledCircleRoi.count
                  << " radius="
                  << (scaledCircleOverlay
                      ? scaledCircleOverlay->radius : -1.0)
                  << std::endl;
        return 1;
    }

    ToolRequest missingSourceRequest = correctedRequest;
    missingSourceRequest.runtimeContext = QJsonObject();
    const ToolResult missingSource = adapter.run(missingSourceRequest);
    if (missingSource.success
            || missingSource.status
            != QStringLiteral("position_correction_source_missing")) {
        std::cerr << "Circle did not expose the common missing-source error"
                  << std::endl;
        return 1;
    }

    std::cout << "circle_position_correction_smoke passed" << std::endl;
    return 0;
}
