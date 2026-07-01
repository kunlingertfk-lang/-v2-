#include "algorithms/recognition/ColorComparisonHalconRunner.h"
#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QPointF>
#include <iostream>
#include <opencv2/core.hpp>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    ColorComparisonHalconConfig config;
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(), &config.halconSoPathCandidates);
    config.templateRoiNormalized = QRectF(0.0, 0.0, 0.5, 1.0);
    config.detectRoiNormalized = QRectF(0.5, 0.0, 0.5, 1.0);
    config.detectRegionType = QStringLiteral("rectangle");
    config.featureType = QStringLiteral("histogram");
    config.sensitivity = QStringLiteral("medium");
    config.brightnessEnabled = true;
    config.minScore = 90;

    cv::Mat image(24, 24, CV_8UC3, cv::Scalar(20, 80, 180));
    ColorComparisonHalconRunner runner;
    const ColorComparisonHalconResult result = runner.run(image, config);
    if (!result.success || !result.ok || result.score < 99.0) {
        std::cerr << "expected identical color comparison to pass, got "
                  << result.status.toStdString() << ": "
                  << result.message.toStdString() << " score="
                  << result.score << std::endl;
        return 1;
    }
    if (result.payload.value(QStringLiteral("comparisonMethod")).toString()
            != QStringLiteral("hsv_histogram_intersection")) {
        std::cerr << "comparison payload missing method" << std::endl;
        return 1;
    }

    ColorComparisonHalconConfig invalidConfig = config;
    invalidConfig.templateRoiNormalized = QRectF();
    const ColorComparisonHalconResult invalid = runner.run(image, invalidConfig);
    if (invalid.success || invalid.status != QStringLiteral("invalid_template_roi")) {
        std::cerr << "invalid template ROI must fail with invalid_template_roi, got "
                  << invalid.status.toStdString() << std::endl;
        return 1;
    }

    ColorComparisonHalconConfig maskedConfig = config;
    maskedConfig.detectMaskPolygonNormalized = {
        QPointF(0.0, 0.0),
        QPointF(1.0, 0.0),
        QPointF(1.0, 1.0),
        QPointF(0.0, 1.0)
    };
    const ColorComparisonHalconResult masked = runner.run(image, maskedConfig);
    if (masked.success || masked.status != QStringLiteral("detect_masked_empty")) {
        std::cerr << "fully masked detect ROI must fail with detect_masked_empty, got "
                  << masked.status.toStdString() << std::endl;
        return 1;
    }

    return 0;
}
