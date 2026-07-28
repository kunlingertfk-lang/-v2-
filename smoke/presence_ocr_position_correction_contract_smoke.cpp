#include "tooladapters/ContourPresenceAdapter.h"
#include "tooladapters/EdgePresenceAdapter.h"
#include "tooladapters/LinePresenceAdapter.h"
#include "tooladapters/OcrAdapter.h"
#include "tooladapters/PatternPresenceAdapter.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <iostream>

namespace {

ToolConfig correctedConfig(ToolType type, const QString &toolId)
{
    ToolConfig config;
    config.toolId = toolId;
    config.toolType = type;
    config.enabled = true;
    config.roiNormalized = QRectF(0.1, 0.1, 0.7, 0.7);
    config.params.insert(QStringLiteral("enablePositionCorrection"), true);
    config.params.insert(QStringLiteral("positionCorrectionSource"),
                         QStringLiteral("1 位置修正.位置修正信息"));
    config.params.insert(QStringLiteral("positionCorrectionSourceId"),
                         QStringLiteral("pc-1"));
    config.params.insert(QStringLiteral("showPositionCorrectionMatchContour"), true);
    if (type == ToolType::Ocr) {
        config.params.insert(
                    QStringLiteral("ocrModelPath"),
                    QStringLiteral("/opt/halcon/ocr/Industrial_0-9+_NoRej.omc"));
    }
    return config;
}

bool expectsMissingSource(ToolAdapter *adapter,
                          ToolType type,
                          const QString &toolId,
                          const cv::Mat &image)
{
    ToolRequest request;
    request.config = correctedConfig(type, toolId);
    request.image = image;
    request.referenceImage = image;
    const ToolResult result = adapter->run(request);
    if (result.status == QStringLiteral("position_correction_source_missing"))
        return true;

    std::cerr << toolId.toStdString()
              << " did not use PositionCorrectionConsumer: "
              << result.status.toStdString() << " "
              << result.message.toStdString() << std::endl;
    return false;
}

QJsonObject correctionContext(const QJsonArray &matrix)
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
    return {
        {QStringLiteral("positionCorrectionsById"),
         QJsonObject{{QStringLiteral("pc-1"), correction.toJson()}}}
    };
}

bool expectsCorrectedRoiOutOfImage(ToolAdapter *adapter,
                                   ToolType type,
                                   const QString &toolId,
                                   const cv::Mat &image)
{
    ToolRequest request;
    request.config = correctedConfig(type, toolId);
    request.image = image;
    request.referenceImage = image;
    request.runtimeContext = correctionContext(
                QJsonArray{1.0, 0.0, 1000.0,
                           0.0, 1.0, 1000.0});
    const ToolResult result = adapter->run(request);
    if (!result.success
            && result.status == QStringLiteral("corrected_roi_out_of_image")
            && result.payload.value(
                QStringLiteral("positionCorrectionApplied")).toBool(false)) {
        return true;
    }

    std::cerr << toolId.toStdString()
              << " did not reject an out-of-image corrected ROI: "
              << result.status.toStdString() << " "
              << result.message.toStdString() << std::endl;
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    cv::Mat image(96, 128, CV_8UC1, cv::Scalar(0));
    image(cv::Rect(32, 24, 20, 14)).setTo(cv::Scalar(255));
    image(cv::Rect(68, 48, 10, 24)).setTo(cv::Scalar(180));

    PatternPresenceAdapter pattern;
    EdgePresenceAdapter edge;
    LinePresenceAdapter line;
    ContourPresenceAdapter contour;
    OcrAdapter ocr;

    ToolRequest unsupportedContourRegion;
    unsupportedContourRegion.config.toolId = QStringLiteral("contour-unsupported-roi");
    unsupportedContourRegion.config.toolType = ToolType::ContourPresence;
    unsupportedContourRegion.config.roiNormalized = QRectF(0.1, 0.1, 0.7, 0.7);
    unsupportedContourRegion.config.params.insert(
                QStringLiteral("detectRegionType"), QStringLiteral("circle"));
    unsupportedContourRegion.image = image;
    unsupportedContourRegion.referenceImage = image;
    const ToolResult unsupportedContourResult =
            contour.run(unsupportedContourRegion);
    if (unsupportedContourResult.status
            != QStringLiteral("unsupported_detect_roi")) {
        std::cerr << "contour unsupported ROI was silently downgraded: "
                  << unsupportedContourResult.status.toStdString() << std::endl;
        return 1;
    }

    if (!expectsMissingSource(&pattern, ToolType::PatternPresence,
                              QStringLiteral("pattern"), image)
            || !expectsMissingSource(&edge, ToolType::EdgePresence,
                                     QStringLiteral("edge"), image)
            || !expectsMissingSource(&line, ToolType::LinePresence,
                                     QStringLiteral("line"), image)
            || !expectsMissingSource(&contour, ToolType::ContourPresence,
                                     QStringLiteral("contour"), image)
            || !expectsMissingSource(&ocr, ToolType::Ocr,
                                     QStringLiteral("ocr"), image)) {
        return 1;
    }

    if (!expectsCorrectedRoiOutOfImage(&pattern, ToolType::PatternPresence,
                                      QStringLiteral("pattern"), image)
            || !expectsCorrectedRoiOutOfImage(&edge, ToolType::EdgePresence,
                                      QStringLiteral("edge"), image)
            || !expectsCorrectedRoiOutOfImage(&line, ToolType::LinePresence,
                                              QStringLiteral("line"), image)
            || !expectsCorrectedRoiOutOfImage(&contour, ToolType::ContourPresence,
                                              QStringLiteral("contour"), image)
            || !expectsCorrectedRoiOutOfImage(&ocr, ToolType::Ocr,
                                              QStringLiteral("ocr"), image)) {
        return 1;
    }

    std::cout << "presence/OCR position-correction consumer contract smoke passed"
              << std::endl;
    return 0;
}
