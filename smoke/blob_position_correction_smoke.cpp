#include "tooladapters/BlobPresenceAdapter.h"
#include "toolcore/ToolAdapter.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolResult.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QPolygonF>

#include <cmath>
#include <iostream>

namespace {

ToolConfig blobConfig(bool correctionEnabled)
{
    ToolConfig config;
    config.toolId = QStringLiteral("blob-1");
    config.toolType = ToolType::BlobPresence;
    config.category = ToolCategory::Presence;
    config.toolName = QStringLiteral("BlobPresence");
    config.enabled = true;
    config.roiNormalized = QRectF(0.1, 0.1, 0.2, 0.2);
    config.params = {
        {QStringLiteral("detectRegionType"), QStringLiteral("rect")},
        {QStringLiteral("enablePositionCorrection"), correctionEnabled},
        {QStringLiteral("positionCorrectionSource"), QStringLiteral("pc-1")},
        {QStringLiteral("positionCorrectionSourceId"), QStringLiteral("pc-1")},
        {QStringLiteral("grayMin"), 200},
        {QStringLiteral("grayMax"), 255},
        {QStringLiteral("areaMin"), 20},
        {QStringLiteral("areaMax"), 1000},
        {QStringLiteral("existOk"), true}
    };
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
    image(cv::Rect(42, 12, 8, 8)).setTo(cv::Scalar(255));

    BlobPresenceAdapter adapter;
    ToolRequest correctedRequest;
    correctedRequest.config = blobConfig(true);
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
        std::cerr << "Blob did not consume the common corrected ROI: "
                  << corrected.status.toStdString() << " "
                  << corrected.message.toStdString()
                  << " count=" << corrected.count << std::endl;
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
                QVector<ToolConfig>{correctionTool, blobConfig(true)},
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
        std::cerr << "Blob did not consume a preceding correction through ToolEngine"
                  << std::endl;
        return 1;
    }

    ToolRequest uncorrectedRequest = correctedRequest;
    uncorrectedRequest.config = blobConfig(false);
    uncorrectedRequest.runtimeContext = QJsonObject();
    const ToolResult uncorrected = adapter.run(uncorrectedRequest);
    if (!uncorrected.success || uncorrected.count != 0) {
        std::cerr << "Blob baseline ROI unexpectedly found the shifted target"
                  << std::endl;
        return 1;
    }

    cv::Mat scaledImage(100, 100, CV_8UC1, cv::Scalar(0));
    scaledImage(cv::Rect(45, 15, 8, 8)).setTo(cv::Scalar(255));
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
        std::cerr << "Blob corrected Region and overlay did not scale together"
                  << " status=" << scaled.status.toStdString()
                  << " message=" << scaled.message.toStdString()
                  << " count=" << scaled.count
                  << " bounds=" << scaledBounds.x() << ","
                  << scaledBounds.y() << " "
                  << scaledBounds.width() << "x" << scaledBounds.height()
                  << std::endl;
        return 1;
    }

    ToolRequest missingSourceRequest = correctedRequest;
    missingSourceRequest.runtimeContext = QJsonObject();
    const ToolResult missingSource = adapter.run(missingSourceRequest);
    if (missingSource.success
            || missingSource.status
            != QStringLiteral("position_correction_source_missing")) {
        std::cerr << "Blob did not expose the common missing-source error"
                  << std::endl;
        return 1;
    }

    std::cout << "blob_position_correction_smoke passed" << std::endl;
    return 0;
}
