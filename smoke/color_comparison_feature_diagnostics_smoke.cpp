#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/recognition/ColorComparisonHalconRunner.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>

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
