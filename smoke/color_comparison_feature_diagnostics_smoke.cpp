#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/recognition/ColorComparisonHalconRunner.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>

#include <cmath>
#include <iostream>
#include <opencv2/core.hpp>

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const ColorComparisonScoreBreakdown scenePlatform =
            ColorComparisonHalconRunner::scoreBreakdown(
                89.15929556406984, 27.49512987012987, 27.49512987012987,
                0.21695643066610806, 0.09929032258064516);
    check(std::abs(scenePlatform.finalScore - 40.0) > 0.5,
          "continuous saturation penalty must remove the fixed 40-point plateau");
    check(scenePlatform.saturationFactor > 0.39
          && scenePlatform.saturationFactor <= 0.40,
          "full gray/color mismatch must expose the saturation factor");
    const ColorComparisonScoreBreakdown lowerBase =
            ColorComparisonHalconRunner::scoreBreakdown(
                60.0, 27.5, 27.5, 0.217, 0.099);
    check(std::abs(scenePlatform.finalScore - lowerBase.finalScore) > 5.0,
          "different base scores must remain distinguishable after saturation penalty");
    const ColorComparisonScoreBreakdown belowBoundary =
            ColorComparisonHalconRunner::scoreBreakdown(
                90.0, 50.0, 50.0, 0.10, 0.1499);
    const ColorComparisonScoreBreakdown aboveBoundary =
            ColorComparisonHalconRunner::scoreBreakdown(
                90.0, 50.0, 50.0, 0.10, 0.1501);
    check(std::abs(belowBoundary.finalScore - aboveBoundary.finalScore) < 0.5,
          "saturation penalty must stay continuous around intermediate thresholds");

    ColorComparisonHalconConfig config;
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(), &config.halconSoPathCandidates);
    config.templateRegionMode = QStringLiteral("custom");
    config.templateRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    config.detectRegionType = QStringLiteral("rectangle");
    config.detectRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    config.inputSignature.colorMode = QStringLiteral("color");
    config.inputSignature.pixelFormat = QStringLiteral("BGR8");
    config.inputSignature.bitDepth = 8;

    cv::Mat image(48, 64, CV_8UC3, cv::Scalar(24, 96, 210));
    ColorComparisonHalconRunner runner;

    ColorComparisonHalconConfig emptyModelConfig = config;
    const ColorComparisonHalconResult emptyModel = runner.run(
                image, emptyModelConfig);
    const QJsonObject emptyDiagnostics = emptyModel.payload.value(
                QStringLiteral("histogramDiagnostics")).toObject();
    check(!emptyModel.success, "empty model must fail before measurement");
    check(!emptyDiagnostics.value(QStringLiteral("available")).toBool(true),
          "failure payload must mark histogram diagnostics unavailable");
    check(emptyDiagnostics.value(QStringLiteral("hueBins")).toInt() == 32
          && emptyDiagnostics.value(QStringLiteral("saturationBins")).toInt() == 32,
          "failure diagnostics must preserve histogram shape contract");

    if (!qEnvironmentVariableIsSet("RUN_HALCON_LICENSED_SMOKE")) {
        if (failures)
            return 1;
        std::cout << "color_comparison_feature_diagnostics_smoke: preflight passed; "
                     "licensed extraction skipped" << std::endl;
        return 0;
    }

    const ColorComparisonTemplateBuildResult built = runner.buildTemplateModel(
                image, config);
    check(built.success, "licensed template build must succeed");
    if (built.success) {
        config.model = built.model;
        const ColorComparisonHalconResult result = runner.run(image, config);
        check(result.success && result.measurementValid,
              "licensed comparison must return a valid measurement");
        check(result.detectFeature.size() == 1024,
              "runner must expose 1024-value detection HS histogram");
        check(result.detectValueHistogram.size() == 32,
              "runner must expose 32-value detection V histogram");
        const QJsonObject diagnostics = result.payload.value(
                    QStringLiteral("histogramDiagnostics")).toObject();
        check(diagnostics.value(QStringLiteral("available")).toBool(),
              "successful payload must mark diagnostics available");
        check(diagnostics.value(QStringLiteral("layout")).toString()
              == QStringLiteral("hue_major"),
              "successful diagnostics must preserve hue_major layout");
        check(diagnostics.value(QStringLiteral("detectHsHistogram"))
              .toArray().size() == 1024,
              "payload must contain 1024 detection HS values");
        check(diagnostics.value(QStringLiteral("detectValueHistogram"))
              .toArray().size() == 32,
              "payload must contain 32 detection V values");

        ColorComparisonHalconConfig fallbackConfig = config;
        fallbackConfig.brightnessCompensation = true;
        cv::Mat darkImage(48, 64, CV_8UC3, cv::Scalar(20, 40, 100));
        cv::Mat brightImage(48, 64, CV_8UC3, cv::Scalar(40, 80, 200));
        const ColorComparisonTemplateBuildResult fallbackTemplate =
                runner.buildTemplateModel(darkImage, fallbackConfig);
        check(fallbackTemplate.success,
              "brightness fallback template build must succeed");
        if (fallbackTemplate.success) {
            fallbackConfig.model = fallbackTemplate.model;
            const ColorComparisonHalconResult fallbackResult =
                    runner.run(brightImage, fallbackConfig);
            const QJsonObject brightness = fallbackResult.payload.value(
                        QStringLiteral("brightnessCompensation")).toObject();
            check(fallbackResult.success && fallbackResult.measurementValid,
                  "unsafe brightness scale must fall back to raw features");
            check(brightness.value(QStringLiteral("requested")).toBool()
                  && brightness.value(QStringLiteral("fallback")).toBool()
                  && !brightness.value(QStringLiteral("applied")).toBool(),
                  "brightness diagnostics must expose requested/fallback/applied states");
            check(brightness.value(QStringLiteral("fallbackReason")).toString()
                  == QStringLiteral("scale_out_of_range"),
                  "unsafe brightness scale must expose a stable fallback reason");
            check(fallbackResult.payload.value(QStringLiteral("warnings"))
                  .toArray().contains(QStringLiteral("brightness_compensation_skipped")),
                  "brightness fallback must be visible as a warning");
            check(fallbackResult.payload.value(QStringLiteral("histogramDiagnostics"))
                  .toObject().value(QStringLiteral("available")).toBool(),
                  "brightness fallback must retain detection histograms");
        }
    } else {
        std::cerr << "template build: " << built.status.toStdString()
                  << ": " << built.message.toStdString() << std::endl;
    }

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "color_comparison_feature_diagnostics_smoke: all checks passed"
              << std::endl;
    return 0;
}
