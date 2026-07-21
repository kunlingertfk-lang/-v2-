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

bool vectorsNear(const QVector<double> &left,
                 const QVector<double> &right,
                 double tolerance = 1e-12)
{
    if (left.size() != right.size())
        return false;
    for (int index = 0; index < left.size(); ++index) {
        if (std::abs(left.at(index) - right.at(index)) > tolerance)
            return false;
    }
    return true;
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

        cv::Mat ownershipImage(64, 64, CV_8UC3);
        ownershipImage(cv::Rect(0, 0, 32, 32)).setTo(cv::Scalar(20, 20, 220));
        ownershipImage(cv::Rect(32, 0, 32, 32)).setTo(cv::Scalar(20, 220, 20));
        ownershipImage(cv::Rect(0, 32, 32, 32)).setTo(cv::Scalar(220, 20, 20));
        ownershipImage(cv::Rect(32, 32, 32, 32)).setTo(cv::Scalar(80, 180, 220));

        const QVector<QPointF> topLeftMask = {
            QPointF(0.0, 0.0), QPointF(0.5, 0.0),
            QPointF(0.5, 0.5), QPointF(0.0, 0.5)
        };
        const QVector<QPointF> topRightMask = {
            QPointF(0.5, 0.0), QPointF(1.0, 0.0),
            QPointF(1.0, 0.5), QPointF(0.5, 0.5)
        };

        ColorComparisonHalconConfig syncMaskA = config;
        syncMaskA.templateRegionMode = QStringLiteral("sync");
        syncMaskA.detectRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
        syncMaskA.detectMaskPolygonNormalized = topLeftMask;
        syncMaskA.templateMaskPolygonNormalized.clear();
        ColorComparisonHalconConfig syncMaskB = syncMaskA;
        syncMaskB.detectMaskPolygonNormalized = topRightMask;
        const ColorComparisonTemplateBuildResult syncBuildA =
                runner.buildTemplateModel(ownershipImage, syncMaskA);
        const ColorComparisonTemplateBuildResult syncBuildB =
                runner.buildTemplateModel(ownershipImage, syncMaskB);
        check(syncBuildA.success && syncBuildB.success,
              "sync ownership template builds must succeed");
        if (syncBuildA.success && syncBuildB.success) {
            check(vectorsNear(syncBuildA.model.values, syncBuildB.model.values)
                  && vectorsNear(syncBuildA.model.valueHistogram,
                                 syncBuildB.model.valueHistogram)
                  && syncBuildA.model.effectivePixelCount
                     == syncBuildB.model.effectivePixelCount,
                  "detection mask must not alter a synchronized template feature");
            check(syncBuildA.model.extractParamsHash
                  == syncBuildB.model.extractParamsHash,
                  "detection mask must not enter synchronized template hash");
            const QJsonObject extractParams = syncBuildA.payload.value(
                        QStringLiteral("extractParams")).toObject();
            check(!extractParams.contains(
                      QStringLiteral("syncDetectionMaskPolygon"))
                  && extractParams.value(
                      QStringLiteral("maskOwnershipContract")).toString()
                     == QStringLiteral(
                         "independent_template_and_detection_v1"),
                  "template extract params must expose independent mask ownership");
        }

        ColorComparisonHalconConfig templateMaskA = config;
        templateMaskA.templateRegionMode = QStringLiteral("custom");
        templateMaskA.templateRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
        templateMaskA.detectRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
        templateMaskA.detectMaskPolygonNormalized = topLeftMask;
        templateMaskA.templateMaskPolygonNormalized = topLeftMask;
        ColorComparisonHalconConfig templateMaskB = templateMaskA;
        templateMaskB.templateMaskPolygonNormalized = topRightMask;
        const ColorComparisonTemplateBuildResult templateBuildA =
                runner.buildTemplateModel(ownershipImage, templateMaskA);
        const ColorComparisonTemplateBuildResult templateBuildB =
                runner.buildTemplateModel(ownershipImage, templateMaskB);
        check(templateBuildA.success && templateBuildB.success,
              "independent template mask builds must succeed");
        if (templateBuildA.success && templateBuildB.success) {
            templateMaskA.model = templateBuildA.model;
            templateMaskB.model = templateBuildB.model;
            const ColorComparisonHalconResult detectWithTemplateMaskA =
                    runner.run(ownershipImage, templateMaskA);
            const ColorComparisonHalconResult detectWithTemplateMaskB =
                    runner.run(ownershipImage, templateMaskB);
            check(detectWithTemplateMaskA.success
                  && detectWithTemplateMaskB.success
                  && detectWithTemplateMaskA.measurementValid
                  && detectWithTemplateMaskB.measurementValid,
                  "independent template masks must preserve valid detection");
            check(detectWithTemplateMaskA.payload.value(
                      QStringLiteral("effectiveDetectionPixels")).toDouble()
                  == detectWithTemplateMaskB.payload.value(
                      QStringLiteral("effectiveDetectionPixels")).toDouble()
                  && vectorsNear(detectWithTemplateMaskA.detectFeature,
                                 detectWithTemplateMaskB.detectFeature)
                  && vectorsNear(detectWithTemplateMaskA.detectValueHistogram,
                                 detectWithTemplateMaskB.detectValueHistogram),
                  "template mask must not alter detection feature extraction");
        }

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

        ColorComparisonHalconConfig clipFallbackConfig = config;
        clipFallbackConfig.brightnessCompensation = true;
        cv::Mat clipTemplateImage(48, 64, CV_8UC3,
                                  cv::Scalar(120, 120, 120));
        cv::Mat clipDetectionImage(48, 64, CV_8UC3,
                                   cv::Scalar(92, 92, 92));
        const int brightPixels = 200;
        for (int index = 0; index < brightPixels; ++index) {
            const int row = index / clipDetectionImage.cols;
            const int column = index % clipDetectionImage.cols;
            clipDetectionImage.at<cv::Vec3b>(row, column) =
                    cv::Vec3b(250, 250, 250);
        }
        const ColorComparisonTemplateBuildResult clipTemplate =
                runner.buildTemplateModel(clipTemplateImage,
                                          clipFallbackConfig);
        check(clipTemplate.success,
              "clip-ratio fallback template build must succeed");
        if (clipTemplate.success) {
            clipFallbackConfig.model = clipTemplate.model;
            const ColorComparisonHalconResult clipFallbackResult =
                    runner.run(clipDetectionImage, clipFallbackConfig);
            const QJsonObject brightness = clipFallbackResult.payload.value(
                        QStringLiteral("brightnessCompensation")).toObject();
            check(clipFallbackResult.success
                  && clipFallbackResult.measurementValid,
                  "excessive clipping must fall back to raw features");
            check(brightness.value(QStringLiteral("fallback")).toBool()
                  && brightness.value(QStringLiteral("fallbackReason")).toString()
                     == QStringLiteral("clip_ratio_exceeded"),
                  "clip fallback must expose a stable reason");
            check(brightness.value(QStringLiteral("clippedRatio")).toDouble()
                  > 0.02,
                  "clip fallback diagnostics must preserve the measured ratio");
            check(clipFallbackResult.payload.value(
                      QStringLiteral("histogramDiagnostics")).toObject()
                  .value(QStringLiteral("available")).toBool(),
                  "clip fallback must retain detection histograms");
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
