#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/location/TemplateLocationHalconRunner.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <cmath>
#include <iostream>

#include <opencv2/imgproc.hpp>

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    }
}

bool finitePayloadPose(const QJsonObject &payload)
{
    const double x = payload.value(QStringLiteral("x")).toDouble(qQNaN());
    const double y = payload.value(QStringLiteral("y")).toDouble(qQNaN());
    const double angle = payload.value(QStringLiteral("angleDeg")).toDouble(qQNaN());
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(angle);
}

int overlayCount(const QVector<ToolOverlay> &overlays, const QString &label)
{
    int count = 0;
    for (const ToolOverlay &overlay : overlays) {
        if (overlay.label == label)
            ++count;
    }
    return count;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    cv::Mat image(240, 320, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::rectangle(image, cv::Rect(90, 70, 80, 50), cv::Scalar(255, 255, 255), cv::FILLED);
    cv::line(image, cv::Point(90, 70), cv::Point(170, 120), cv::Scalar(0, 0, 0), 3);

    TemplateLocationHalconConfig config;
    config.toolId = QStringLiteral("reference.positionCorrection");
    config.modelCacheKey = QStringLiteral("reference.positionCorrection.private_template.smoke");
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(), &config.halconSoPathCandidates);
    config.templateRegionType = QStringLiteral("rectangle");
    config.templateRoiNormalized = QRectF(80.0 / 320.0, 60.0 / 240.0,
                                          110.0 / 320.0, 80.0 / 240.0);
    config.searchRegionType = QStringLiteral("full");
    config.searchRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    config.minScore = 40;
    config.angleStart = -20;
    config.angleExtent = 40;
    config.scaleMin = 100;
    config.scaleMax = 100;
    config.maxMatches = 1;
    config.minMatchCount = 1;
    config.maxMatchCount = 1;
    config.timeoutMs = 2000;

    TemplateLocationHalconRunner runner;
    const TemplateLocationHalconResult result = runner.run(image, image, config);
    check(result.success && result.ok && result.status == QStringLiteral("found"),
          "reference private template must self-match on the reference image");
    check(result.payload.value(QStringLiteral("found")).toBool(false),
          "self-match payload must mark found=true");
    check(finitePayloadPose(result.payload),
          "self-match payload must expose finite x/y/angleDeg pose");
    check(result.payload.value(QStringLiteral("modelCacheKey")).toString() == config.modelCacheKey,
          "self-match payload must expose the private model cache key");
    check(overlayCount(result.overlays, QStringLiteral("match_result")) > 0,
          "self-match must expose the matched model contour for the reference preview");
    check(overlayCount(result.overlays, QStringLiteral("match_center")) == 2,
          "self-match must expose a two-line centroid cross for the reference preview");

    TemplateLocationHalconConfig rectangleMaskConfig = config;
    rectangleMaskConfig.modelCacheKey =
            QStringLiteral("reference.positionCorrection.private_template.mask.smoke");
    rectangleMaskConfig.templateMaskRegionType = QStringLiteral("rectangle");
    rectangleMaskConfig.templateMaskRoiNormalized =
            QRectF(100.0 / 320.0, 78.0 / 240.0,
                   20.0 / 320.0, 16.0 / 240.0);
    const TemplateLocationHalconResult rectangleMaskResult =
            runner.run(image, image, rectangleMaskConfig);
    check(rectangleMaskResult.success && rectangleMaskResult.ok &&
          rectangleMaskResult.payload.value(
              QStringLiteral("templateMaskRegionType")).toString() ==
              QStringLiteral("rectangle"),
          "reference private locator must self-match with a rectangle template mask");

    const double poseX = result.payload.value(QStringLiteral("x")).toDouble();
    const double poseY = result.payload.value(QStringLiteral("y")).toDouble();
    bool hasHorizontalCenterLine = false;
    bool hasVerticalCenterLine = false;
    for (const ToolOverlay &overlay : result.overlays) {
        if (overlay.label != QStringLiteral("match_center"))
            continue;
        hasHorizontalCenterLine = hasHorizontalCenterLine
                || (std::abs(overlay.p1.y() - poseY) < 1e-6
                    && std::abs(overlay.p2.y() - poseY) < 1e-6);
        hasVerticalCenterLine = hasVerticalCenterLine
                || (std::abs(overlay.p1.x() - poseX) < 1e-6
                    && std::abs(overlay.p2.x() - poseX) < 1e-6);
    }
    check(hasHorizontalCenterLine && hasVerticalCenterLine,
          "centroid cross must be anchored at the self-match x/y pose");

    TemplateLocationHalconConfig customConfig = config;
    customConfig.originMode = QStringLiteral("custom");
    customConfig.customOriginNormalized = QPointF(0.82, 0.16);
    const TemplateLocationHalconResult customResult =
            runner.run(image, image, customConfig);
    check(customResult.success && customResult.ok,
          "reference private template must self-match with a custom origin");
    check(std::abs(customResult.payload.value(QStringLiteral("x")).toDouble()
                   - 0.82 * image.cols) <= 2.0
          && std::abs(customResult.payload.value(QStringLiteral("y")).toDouble()
                      - 0.16 * image.rows) <= 2.0,
          "custom self-match pose must use the selected full-image normalized point");
    check(customResult.payload.value(QStringLiteral("originMode")).toString()
                  == QStringLiteral("custom"),
          "custom self-match payload must retain the custom origin mode");

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "reference_position_correction_private_model_smoke: all checks passed"
              << std::endl;
    return 0;
}
