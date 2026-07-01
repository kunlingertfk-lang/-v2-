#include "algorithms/recognition/ColorRecognitionHalconRunner.h"
#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
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

    if (circleFeature.payload.value(QStringLiteral("lightingNormalizationMode")).toString()
            != QStringLiteral("include_value_channel")) {
        std::cerr << "brightness enabled must include value channel" << std::endl;
        return 1;
    }

    ColorRecognitionHalconConfig hsConfig = circleConfig;
    hsConfig.brightnessEnabled = false;
    const ColorRecognitionHalconFeatureResult hsFeature = runner.extractFeature(image, hsConfig);
    if (!hsFeature.success || hsFeature.feature.size() != 32 ||
        hsFeature.payload.value(QStringLiteral("lightingNormalizationMode")).toString()
            != QStringLiteral("hue_saturation_priority")) {
        std::cerr << "brightness disabled must use H/S priority feature" << std::endl;
        return 1;
    }

    ColorRecognitionHalconConfig dominantConfig = circleConfig;
    dominantConfig.brightnessEnabled = false;
    dominantConfig.colorDecisionMode = QStringLiteral("dominant_ratio");
    dominantConfig.labels = {
        {QStringLiteral("warm"), 1},
        {QStringLiteral("cool"), 2}
    };
    dominantConfig.samples = {
        {QStringLiteral("warm"), 1, hsFeature.feature, dominantConfig.roiNormalized},
        {QStringLiteral("cool"), 2, QVector<double>(hsFeature.feature.size(), 0.0), dominantConfig.roiNormalized}
    };
    const ColorRecognitionHalconResult dominantResult = runner.run(image, dominantConfig);
    if (!dominantResult.success ||
        dominantResult.payload.value(QStringLiteral("colorDecisionMode")).toString()
            != QStringLiteral("dominant_ratio") ||
        dominantResult.payload.value(QStringLiteral("comparisonMethod")).toString()
            != QStringLiteral("dominant_color_ratio") ||
        dominantResult.payload.value(QStringLiteral("dominantColorRatio")).toDouble() <= 0.99 ||
        dominantResult.predictedLabel != QStringLiteral("warm") ||
        dominantResult.payload.value(QStringLiteral("labelAreaRatios")).toArray().isEmpty()) {
        std::cerr << "dominant ratio mode did not select dominant color" << std::endl;
        return 1;
    }

    ColorRecognitionHalconConfig legacyConfig = dominantConfig;
    legacyConfig.colorDecisionMode = QStringLiteral("histogram_intersection");
    const ColorRecognitionHalconResult legacyResult = runner.run(image, legacyConfig);
    if (!legacyResult.success ||
        legacyResult.payload.value(QStringLiteral("colorDecisionMode")).toString()
            != QStringLiteral("histogram_intersection") ||
        legacyResult.payload.value(QStringLiteral("comparisonMethod")).toString()
            != QStringLiteral("histogram_intersection")) {
        std::cerr << "legacy histogram intersection mode must remain available" << std::endl;
        return 1;
    }

    ColorRecognitionHalconConfig alignedConfig = dominantConfig;
    alignedConfig.samples = {
        {QStringLiteral("warm"), 1, circleFeature.feature, dominantConfig.roiNormalized}
    };
    const ColorRecognitionHalconResult alignedResult = runner.run(image, alignedConfig);
    if (!alignedResult.success ||
        alignedResult.predictedLabel != QStringLiteral("warm") ||
        alignedResult.payload.value(QStringLiteral("featureAlignmentApplied")).toBool() != true) {
        std::cerr << "brightness-disabled detection must align older H/S/V samples to H/S" << std::endl;
        return 1;
    }

    ColorRecognitionHalconConfig brightnessOnAlignedConfig = circleConfig;
    brightnessOnAlignedConfig.colorDecisionMode = QStringLiteral("dominant_ratio");
    brightnessOnAlignedConfig.labels = {
        {QStringLiteral("warm"), 1}
    };
    brightnessOnAlignedConfig.samples = {
        {QStringLiteral("warm"), 1, hsFeature.feature, brightnessOnAlignedConfig.roiNormalized}
    };
    const ColorRecognitionHalconResult brightnessOnAlignedResult =
            runner.run(image, brightnessOnAlignedConfig);
    if (!brightnessOnAlignedResult.success ||
        brightnessOnAlignedResult.predictedLabel != QStringLiteral("warm") ||
        brightnessOnAlignedResult.payload.value(QStringLiteral("featureAlignmentApplied")).toBool() != true) {
        std::cerr << "brightness-enabled detection must align older H/S samples to H/S" << std::endl;
        return 1;
    }

    ColorRecognitionHalconConfig positionConfig = dominantConfig;
    positionConfig.enablePositionCorrection = true;
    positionConfig.positionCorrectionSource = QStringLiteral("1 基准图.位置修正信息");
    const ColorRecognitionHalconResult positionResult = runner.run(image, positionConfig);
    if (!positionResult.success ||
        positionResult.payload.value(QStringLiteral("enablePositionCorrection")).toBool() != true ||
        positionResult.payload.value(QStringLiteral("positionCorrectionSource")).toString()
            != QStringLiteral("1 基准图.位置修正信息") ||
        positionResult.payload.value(QStringLiteral("positionCorrectionApplied")).toBool() != false ||
        positionResult.payload.value(QStringLiteral("positionCorrectionReason")).toString()
            != QStringLiteral("not implemented")) {
        std::cerr << "position correction payload must expose requested-but-not-applied state" << std::endl;
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
