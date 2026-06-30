#include "algorithms/recognition/ColorRecognitionHalconRunner.h"
#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QCoreApplication>
#include <QPointF>
#include <QString>
#include <iostream>
#include <opencv2/core.hpp>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    ColorRecognitionHalconConfig circleConfig;
    circleConfig.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(QString(),
                                                                         &circleConfig.halconSoPathCandidates);
    circleConfig.detectRegionType = QStringLiteral("circle");
    circleConfig.roiNormalized = QRectF(0.25, 0.25, 0.5, 0.5);
    circleConfig.detectCircleCenterNormalized = QPointF(0.5, 0.5);
    circleConfig.detectCircleRadiusNormalized = 0.25;
    circleConfig.detectCircleBoundingRectNormalized = circleConfig.roiNormalized;

    cv::Mat image(16, 16, CV_8UC3, cv::Scalar(20, 40, 60));
    ColorRecognitionHalconRunner runner;
    const ColorRecognitionHalconFeatureResult circleFeature = runner.extractFeature(image, circleConfig);
    if (!circleFeature.success) {
        std::cerr << "circle feature extraction failed: "
                  << circleFeature.status.toStdString() << ": "
                  << circleFeature.message.toStdString() << std::endl;
        return 1;
    }
    if (circleFeature.payload.value(QStringLiteral("detectRegionType")).toString() != QStringLiteral("circle") ||
        circleFeature.payload.value(QStringLiteral("circleDetectRoiApplied")).toBool() != true ||
        circleFeature.payload.value(QStringLiteral("effectiveRoiArea")).toDouble() <= 0.0) {
        std::cerr << "circle ROI payload missing or invalid" << std::endl;
        return 1;
    }

    ColorRecognitionHalconConfig config = circleConfig;
    config.detectMaskPolygonNormalized = {
        QPointF(0.0, 0.0),
        QPointF(1.0, 0.0),
        QPointF(1.0, 1.0),
        QPointF(0.0, 1.0)
    };

    const ColorRecognitionHalconResult result = runner.run(image, config);

    if (result.status != QStringLiteral("masked_roi_empty")) {
        std::cerr << "expected masked_roi_empty, got "
                  << result.status.toStdString() << ": "
                  << result.message.toStdString() << std::endl;
        return 1;
    }
    if (result.success || result.ok) {
        std::cerr << "fully masked ROI must not run detection as success/OK" << std::endl;
        return 1;
    }
    if (result.payload.value(QStringLiteral("detectMaskApplied")).toBool() != true) {
        std::cerr << "detectMaskApplied payload missing" << std::endl;
        return 1;
    }
    if (result.overlays.isEmpty() ||
        result.overlays.last().extra.value(QStringLiteral("status")).toString() != QStringLiteral("MASKED")) {
        std::cerr << "masked brown text overlay missing" << std::endl;
        return 1;
    }

    ColorRecognitionHalconConfig invalidCircleConfig = circleConfig;
    invalidCircleConfig.detectCircleRadiusNormalized = 0.0;
    const ColorRecognitionHalconFeatureResult invalidCircle =
            runner.extractFeature(image, invalidCircleConfig);
    if (invalidCircle.success || invalidCircle.status != QStringLiteral("invalid_roi")) {
        std::cerr << "invalid circle ROI must fail with invalid_roi, got "
                  << invalidCircle.status.toStdString() << std::endl;
        return 1;
    }

    return 0;
}
