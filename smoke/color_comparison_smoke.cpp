#include "algorithms/recognition/ColorComparisonHalconRunner.h"
#include "algorithms/recognition/ColorRecognitionHalconRunner.h"
#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QPointF>
#include <QVector>
#include <cmath>
#include <iostream>
#include <opencv2/core.hpp>

QVector<double> hsvFeature(int bins, int hueBin, int saturationBin, int valueBin = -1)
{
    QVector<double> feature(bins * (valueBin >= 0 ? 3 : 2), 0.0);
    feature[hueBin] = 1.0;
    feature[bins + saturationBin] = 1.0;
    if (valueBin >= 0)
        feature[bins * 2 + valueBin] = 1.0;
    return feature;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QVector<double> templateGreen = hsvFeature(16, 5, 12);
    const QVector<double> shiftedGreen = hsvFeature(16, 6, 12);
    const ColorComparisonHsvSimilarity shiftedGreenSimilarity =
            compareColorComparisonHsvHistograms(templateGreen, shiftedGreen, 16, false);
    if (shiftedGreenSimilarity.combined < 0.70) {
        std::cerr << "medium sensitivity must tolerate nearby green hue bins, got "
                  << shiftedGreenSimilarity.combined * 100.0 << std::endl;
        return 1;
    }

    const QVector<double> cyanHue = hsvFeature(16, 8, 12);
    const ColorComparisonHsvSimilarity cyanSimilarity =
            compareColorComparisonHsvHistograms(templateGreen, cyanHue, 16, false);
    if (cyanSimilarity.combined > 0.52) {
        std::cerr << "cyan must not score as green, got "
                  << cyanSimilarity.combined * 100.0 << std::endl;
        return 1;
    }

    const QVector<double> blueHue = hsvFeature(16, 11, 12);
    const ColorComparisonHsvSimilarity blueSimilarity =
            compareColorComparisonHsvHistograms(templateGreen, blueHue, 16, false);
    if (blueSimilarity.combined > 0.45) {
        std::cerr << "blue must stay below color-match threshold, got "
                  << blueSimilarity.combined * 100.0 << std::endl;
        return 1;
    }

    ColorComparisonHalconConfig config;
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(), &config.halconSoPathCandidates);
    config.templateRoiNormalized = QRectF(0.0, 0.0, 0.5, 1.0);
    config.detectRoiNormalized = QRectF(0.5, 0.0, 0.5, 1.0);
    config.detectRegionType = QStringLiteral("rectangle");
    config.featureType = QStringLiteral("histogram");
    config.sensitivity = QStringLiteral("medium");
    config.brightnessEnabled = false;
    config.minScore = 90;

    cv::Mat image(24, 24, CV_8UC3, cv::Scalar(20, 80, 180));
    ColorComparisonHalconRunner runner;
    ColorComparisonHalconConfig missingTemplateConfig = config;
    missingTemplateConfig.halconSoPath.clear();
    const ColorComparisonHalconResult missingTemplate =
            runner.run(image, missingTemplateConfig);
    if (missingTemplate.success || missingTemplate.status != QStringLiteral("no_template_feature")) {
        std::cerr << "missing saved template feature must fail before HALCON extraction, got "
                  << missingTemplate.status.toStdString() << ": "
                  << missingTemplate.message.toStdString() << std::endl;
        return 1;
    }

    if (!qEnvironmentVariableIsSet("RUN_HALCON_LICENSED_SMOKE")) {
        std::cerr << "HALCON licensed smoke is skipped; pure HSV assertions passed. "
                  << "Set RUN_HALCON_LICENSED_SMOKE=1 on a machine with a valid HALCON license "
                  << "to run Bhattacharyya tuple and image runner checks." << std::endl;
        return 0;
    }

    ColorRecognitionHalconConfig histogramCompareConfig;
    histogramCompareConfig.halconSoPath = config.halconSoPath;
    histogramCompareConfig.halconSoPathCandidates = config.halconSoPathCandidates;
    ColorRecognitionHalconRunner featureRunner;
    const ColorRecognitionHalconHistogramCompareResult bhattacharyyaResult =
            featureRunner.compareHistogramBhattacharyya(templateGreen,
                                                        templateGreen,
                                                        histogramCompareConfig);
    if (!bhattacharyyaResult.success &&
            bhattacharyyaResult.message.contains(QStringLiteral("license"), Qt::CaseInsensitive)) {
        std::cerr << "HALCON license is unavailable; pure HSV assertions passed, "
                  << "Bhattacharyya HALCON tuple execution and image runner are skipped: "
                  << bhattacharyyaResult.message.toStdString() << std::endl;
        return 0;
    }
    if (!bhattacharyyaResult.success || bhattacharyyaResult.distance > 0.001) {
        std::cerr << "Bhattacharyya histogram mode must use available HALCON histogram/tuple "
                  << "operators without requiring compare_histogram, got "
                  << bhattacharyyaResult.status.toStdString() << ": "
                  << bhattacharyyaResult.message.toStdString() << " distance="
                  << bhattacharyyaResult.distance << std::endl;
        return 1;
    }

    const QVector<double> referenceDistribution = {1.0, 0.0};
    const QVector<double> testDistribution = {0.0625, 0.9375};
    const ColorRecognitionHalconHistogramCompareResult bhattacharyyaLogResult =
            featureRunner.compareHistogramBhattacharyya(referenceDistribution,
                                                        testDistribution,
                                                        histogramCompareConfig);
    const double expectedBhattacharyyaDistance = -std::log(0.25);
    if (!bhattacharyyaLogResult.success ||
            std::abs(bhattacharyyaLogResult.distance - expectedBhattacharyyaDistance) > 0.001) {
        std::cerr << "Bhattacharyya distance must use -ln(BC), expected "
                  << expectedBhattacharyyaDistance << " got "
                  << bhattacharyyaLogResult.status.toStdString() << ": "
                  << bhattacharyyaLogResult.message.toStdString() << " distance="
                  << bhattacharyyaLogResult.distance << std::endl;
        return 1;
    }

    config.templateFeature = templateGreen;
    const ColorComparisonHalconResult result = runner.run(image, config);
    if (!result.success || !result.ok || result.score < 99.0) {
        std::cerr << "expected identical color comparison to pass, got "
                  << result.status.toStdString() << ": "
                  << result.message.toStdString() << " score="
                  << result.score << std::endl;
        return 1;
    }
    if (result.payload.value(QStringLiteral("comparisonMethod")).toString()
            != QStringLiteral("hsv_histogram_soft_kernel_weighted")) {
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
