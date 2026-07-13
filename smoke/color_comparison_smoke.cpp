#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/recognition/ColorComparisonHalconRunner.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QtGlobal>

#include <cmath>
#include <iostream>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << "FAIL: " << message << std::endl;
    ++failures;
}

bool near(double actual, double expected, double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

void checkResultTextOverlay(const QVector<ToolOverlay> &overlays,
                            int index,
                            double expectedScore,
                            bool expectedOk,
                            const QRectF &expectedAnchor,
                            const char *message)
{
    if (index < 0 || index >= overlays.size()) {
        check(false, message);
        return;
    }

    const ToolOverlay &overlay = overlays.at(index);
    const QJsonObject anchor = overlay.extra
            .value(QStringLiteral("anchorRect")).toObject();
    const QString expectedStatus = expectedOk
            ? QStringLiteral("OK") : QStringLiteral("NG");
    const QString expectedText = QStringLiteral("%1 score:%2")
            .arg(expectedStatus, QString::number(expectedScore, 'f', 1));
    check(overlay.type == ToolOverlayType::Text
          && overlay.label == QStringLiteral("color_result_text")
          && overlay.text == expectedText
          && near(overlay.score, expectedScore, 1e-9)
          && overlay.extra.value(QStringLiteral("status")).toString()
                 == expectedStatus
          && near(overlay.p1.x(), expectedAnchor.x(), 1e-9)
          && near(overlay.p1.y(), expectedAnchor.y(), 1e-9)
          && near(anchor.value(QStringLiteral("x")).toDouble(),
                  expectedAnchor.x(), 1e-9)
          && near(anchor.value(QStringLiteral("y")).toDouble(),
                  expectedAnchor.y(), 1e-9)
          && near(anchor.value(QStringLiteral("width")).toDouble(),
                  expectedAnchor.width(), 1e-9)
          && near(anchor.value(QStringLiteral("height")).toDouble(),
                  expectedAnchor.height(), 1e-9),
          message);
}

cv::Vec3b hsvFullToBgr(unsigned char hue,
                       unsigned char saturation,
                       unsigned char value)
{
    cv::Mat hsv(1, 1, CV_8UC3, cv::Scalar(hue, saturation, value));
    cv::Mat bgr;
    cv::cvtColor(hsv, bgr, cv::COLOR_HSV2BGR_FULL);
    return bgr.at<cv::Vec3b>(0, 0);
}

cv::Mat solidColor(int rows, int columns, const cv::Vec3b &color)
{
    return cv::Mat(rows, columns, CV_8UC3, cv::Scalar(color[0], color[1], color[2])).clone();
}

cv::Mat verticalMix(int rows,
                    int columns,
                    const cv::Vec3b &left,
                    const cv::Vec3b &right,
                    int leftColumns)
{
    cv::Mat image = solidColor(rows, columns, right);
    image.colRange(0, qBound(0, leftColumns, columns)).setTo(
                cv::Scalar(left[0], left[1], left[2]));
    return image;
}

cv::Mat pairedHsImage(bool swapped)
{
    const cv::Vec3b h1s1 = hsvFullToBgr(40, 96, 180);
    const cv::Vec3b h1s2 = hsvFullToBgr(40, 224, 180);
    const cv::Vec3b h2s1 = hsvFullToBgr(120, 96, 180);
    const cv::Vec3b h2s2 = hsvFullToBgr(120, 224, 180);
    return swapped
            ? verticalMix(64, 64, h1s2, h2s1, 32)
            : verticalMix(64, 64, h1s1, h2s2, 32);
}

ColorComparisonHalconConfig baseConfig()
{
    ColorComparisonHalconConfig config;
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(), &config.halconSoPathCandidates);
    config.templateRegionMode = QStringLiteral("custom");
    config.templateRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    config.detectRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    config.detectRegionType = QStringLiteral("rectangle");
    config.inputSignature.colorMode = QStringLiteral("color");
    config.inputSignature.pixelFormat = QStringLiteral("BGR8");
    config.inputSignature.bitDepth = 8;
    config.sensitivity = QStringLiteral("high");
    config.minScore = 80;
    return config;
}

QJsonArray legacyPointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points) {
        array.append(QJsonObject{
            {QStringLiteral("x"), point.x()},
            {QStringLiteral("y"), point.y()}
        });
    }
    return array;
}

QJsonObject legacyCustomExtractParams(const QVector<QPointF> &templateMask)
{
    return {
        {QStringLiteral("featureType"), QStringLiteral("histogram_hs_2d")},
        {QStringLiteral("algorithm"), QStringLiteral("histogram_intersection")},
        {QStringLiteral("colorSpace"), QStringLiteral("hsv")},
        {QStringLiteral("hueBins"), 32},
        {QStringLiteral("saturationBins"), 32},
        {QStringLiteral("layout"), QStringLiteral("hue_major")},
        {QStringLiteral("minimumEffectivePixels"), 4.0},
        {QStringLiteral("templateRegionMode"), QStringLiteral("custom")},
        {QStringLiteral("templateGeometry"), QJsonObject{
             {QStringLiteral("type"), QStringLiteral("rectangle")},
             {QStringLiteral("rect"), QJsonObject{
                  {QStringLiteral("x"), 0.0},
                  {QStringLiteral("y"), 0.0},
                  {QStringLiteral("width"), 1.0},
                  {QStringLiteral("height"), 1.0}
              }}
         }},
        {QStringLiteral("templateMaskPolygon"), legacyPointsToJson(templateMask)},
        {QStringLiteral("brightnessCompensation"), false},
        {QStringLiteral("brightnessConstants"), QJsonObject{
             {QStringLiteral("minMean"), 8.0},
             {QStringLiteral("maxMean"), 247.0},
             {QStringLiteral("minScale"), 0.75},
             {QStringLiteral("maxScale"), 1.3333333333},
             {QStringLiteral("maxClippedRatio"), 0.02}
         }}
    };
}

int maximumFeatureIndex(const QVector<double> &feature)
{
    int bestIndex = -1;
    double bestValue = -1.0;
    for (int index = 0; index < feature.size(); ++index) {
        if (feature.at(index) > bestValue) {
            bestValue = feature.at(index);
            bestIndex = index;
        }
    }
    return bestIndex;
}

bool warningsContain(const QJsonObject &payload, const QString &warning)
{
    const QJsonArray warnings = payload.value(QStringLiteral("warnings")).toArray();
    for (const QJsonValue &value : warnings) {
        if (value.toString() == warning)
            return true;
    }
    return false;
}

void checkRunnerSourceContract()
{
    QFile source(QString::fromLocal8Bit(COLOR_COMPARISON_RUNNER_SOURCE_PATH));
    check(source.open(QIODevice::ReadOnly),
          "runner source must be readable for HALCON-only scoring contract checks");
    if (!source.isOpen())
        return;

    const QByteArray code = source.readAll();
    check(!code.contains("constexpr qint64 kMinEffectivePixels"),
          "runner must not duplicate the shared minimum-effective-pixels contract");
    check(code.contains("kColorComparisonMinimumEffectivePixels"),
          "runner must use the shared minimum-effective-pixels contract");
    check(code.contains("\"T_tuple_max\""),
          "runner must resolve T_tuple_max as a required HALCON symbol");
    const int scoringStart = code.indexOf("double shiftedHistogramIntersection");
    const int scoringEnd = code.indexOf("QJsonObject brightnessDiagnostics",
                                        scoringStart);
    check(scoringStart >= 0 && scoringEnd > scoringStart,
          "runner scoring implementation must be locatable for static contract checks");
    if (scoringStart < 0 || scoringEnd <= scoringStart)
        return;

    const QByteArray scoring = code.mid(scoringStart, scoringEnd - scoringStart);
    check(scoring.contains("api->tupleMax("),
          "runner scoring must call the resolved HALCON tuple_max operator");
    check(!scoring.contains("qMax("),
          "runner must not use C++ qMax for scoring candidate aggregation");
}

void checkDefaultContract(ColorComparisonHalconRunner *runner)
{
    ColorComparisonHalconConfig emptyModelConfig;
    emptyModelConfig.model.state = ColorComparisonModelState::Empty;
    const ColorComparisonHalconResult emptyModel =
            runner->run(cv::Mat(16, 16, CV_8UC3), emptyModelConfig);
    check(!emptyModel.success &&
          emptyModel.status == QStringLiteral("model_empty"),
          "empty model must fail before HALCON loading");

    ColorComparisonHalconConfig monoConfig;
    monoConfig.inputSignature.colorMode = QStringLiteral("mono");
    const ColorComparisonHalconResult mono =
            runner->run(cv::Mat(16, 16, CV_8UC3), monoConfig);
    check(mono.success && !mono.ok && !mono.measurementValid &&
          mono.status == QStringLiteral("unsupported_color_input"),
          "original mono source must produce invalid NG");

    ColorComparisonHalconConfig emptyImageConfig;
    emptyImageConfig.inputSignature.colorMode = QStringLiteral("mono");
    const ColorComparisonHalconResult emptyImage =
            runner->run(cv::Mat(), emptyImageConfig);
    check(!emptyImage.success && emptyImage.status == QStringLiteral("image_empty"),
          "empty image must take precedence over original color mode");

    ColorComparisonHalconConfig unsupportedFormatConfig;
    const ColorComparisonHalconResult unsupportedFormat =
            runner->run(cv::Mat(16, 16, CV_16UC3), unsupportedFormatConfig);
    check(!unsupportedFormat.success &&
          unsupportedFormat.status == QStringLiteral("unsupported_pixel_format"),
          "non-8-bit input must fail before model and HALCON loading");

    ColorComparisonHalconConfig originalDepthConfig;
    originalDepthConfig.inputSignature.colorMode = QStringLiteral("color");
    originalDepthConfig.inputSignature.bitDepth = 16;
    const ColorComparisonHalconResult originalDepth =
            runner->run(cv::Mat(16, 16, CV_8UC3), originalDepthConfig);
    check(!originalDepth.success &&
          originalDepth.status == QStringLiteral("unsupported_pixel_format"),
          "original non-8-bit metadata must not be hidden by normalized CV_8UC3 storage");

    ColorComparisonHalconConfig incompatiblePixelFormatConfig;
    incompatiblePixelFormatConfig.inputSignature.colorMode = QStringLiteral("color");
    incompatiblePixelFormatConfig.inputSignature.pixelFormat = QStringLiteral("Mono8");
    incompatiblePixelFormatConfig.inputSignature.bitDepth = 8;
    const ColorComparisonHalconResult incompatiblePixelFormat =
            runner->run(cv::Mat(16, 16, CV_8UC3),
                        incompatiblePixelFormatConfig);
    check(!incompatiblePixelFormat.success &&
          incompatiblePixelFormat.status == QStringLiteral("unsupported_pixel_format"),
          "explicit incompatible original pixel format must fail before model and HALCON loading");

    ColorComparisonHalconConfig invalidTemplateRoiConfig;
    invalidTemplateRoiConfig.templateRoiNormalized = QRectF(0.0, 0.0, 0.0, 1.0);
    const ColorComparisonHalconResult invalidTemplateRoi =
            runner->run(cv::Mat(16, 16, CV_8UC3), invalidTemplateRoiConfig);
    check(!invalidTemplateRoi.success &&
          invalidTemplateRoi.status == QStringLiteral("invalid_template_roi"),
          "custom template ROI must be validated before the model");

    ColorComparisonHalconConfig invalidDetectRoiConfig;
    invalidDetectRoiConfig.detectRoiNormalized = QRectF(-0.1, 0.0, 0.5, 0.5);
    const ColorComparisonHalconResult invalidDetectRoi =
            runner->run(cv::Mat(16, 16, CV_8UC3), invalidDetectRoiConfig);
    check(!invalidDetectRoi.success &&
          invalidDetectRoi.status == QStringLiteral("invalid_detect_roi"),
          "out-of-range detection ROI must fail before the model");

    ColorComparisonHalconConfig invalidCircleConfig;
    invalidCircleConfig.detectRegionType = QStringLiteral("circle");
    invalidCircleConfig.detectCircleCenterNormalized = QPointF(0.5, 0.5);
    invalidCircleConfig.detectCircleRadiusNormalized = 0.0;
    const ColorComparisonHalconResult invalidCircle =
            runner->run(cv::Mat(16, 16, CV_8UC3), invalidCircleConfig);
    check(!invalidCircle.success &&
          invalidCircle.status == QStringLiteral("invalid_detect_roi"),
          "zero-radius detection circle must fail before the model");

    const cv::Mat landscapeImage(103, 384, CV_8UC3, cv::Scalar(0, 0, 0));
    ColorComparisonHalconConfig shortEdgeCircleConfig = baseConfig();
    shortEdgeCircleConfig.halconSoPath = QCoreApplication::applicationFilePath();
    shortEdgeCircleConfig.detectRegionType = QStringLiteral("circle");
    shortEdgeCircleConfig.detectCircleCenterNormalized = QPointF(0.5, 0.2);
    shortEdgeCircleConfig.detectCircleRadiusNormalized = 30.0 / 384.0;
    const ColorComparisonHalconResult shortEdgeRun =
            runner->run(landscapeImage, shortEdgeCircleConfig);
    check(!shortEdgeRun.success &&
          shortEdgeRun.status == QStringLiteral("invalid_detect_roi"),
          "landscape circle crossing the short image edge must fail run preflight");
    const ColorComparisonTemplateBuildResult shortEdgeBuild =
            runner->buildTemplateModel(landscapeImage, shortEdgeCircleConfig);
    check(!shortEdgeBuild.success &&
          shortEdgeBuild.status == QStringLiteral("invalid_detect_roi"),
          "landscape circle crossing the short image edge must fail build preflight");

    ColorComparisonHalconConfig invalidMaskConfig;
    invalidMaskConfig.detectMaskPolygonNormalized = {
        QPointF(0.1, 0.1), QPointF(0.2, 0.2), QPointF(0.3, 0.3)
    };
    const ColorComparisonHalconResult invalidMask =
            runner->run(cv::Mat(16, 16, CV_8UC3), invalidMaskConfig);
    check(!invalidMask.success &&
          invalidMask.status == QStringLiteral("invalid_detect_mask"),
          "degenerate detection mask must fail before the model");

    ColorComparisonHalconConfig unsupportedFeatureConfig;
    unsupportedFeatureConfig.model.state = ColorComparisonModelState::Ready;
    unsupportedFeatureConfig.model.featureType = QStringLiteral("spectrum");
    const ColorComparisonHalconResult unsupportedFeature =
            runner->run(cv::Mat(16, 16, CV_8UC3), unsupportedFeatureConfig);
    check(!unsupportedFeature.success &&
          unsupportedFeature.status == QStringLiteral("unsupported_feature"),
          "reserved spectrum feature must fail explicitly before model validation");

    ColorComparisonHalconConfig spectrumBuildConfig;
    spectrumBuildConfig.model.featureType = QStringLiteral("spectrum");
    const ColorComparisonTemplateBuildResult spectrumBuild =
            runner->buildTemplateModel(cv::Mat(16, 16, CV_8UC3),
                                       spectrumBuildConfig);
    check(!spectrumBuild.success &&
          spectrumBuild.status == QStringLiteral("unsupported_feature"),
          "template building must reject reserved spectrum before HALCON loading");

    ColorComparisonHalconConfig staleConfig;
    staleConfig.model.state = ColorComparisonModelState::Stale;
    const ColorComparisonHalconResult stale =
            runner->run(cv::Mat(16, 16, CV_8UC3), staleConfig);
    check(!stale.success && stale.status == QStringLiteral("model_stale"),
          "stale model must fail before HALCON loading");
}

bool buildOrReport(ColorComparisonHalconRunner *runner,
                   const cv::Mat &referenceImage,
                   const ColorComparisonHalconConfig &config,
                   ColorComparisonTemplateBuildResult *built,
                   const char *context)
{
    *built = runner->buildTemplateModel(referenceImage, config);
    if (built->success)
        return true;

    std::cerr << "licensed template build failed (" << context << "): "
              << built->status.toStdString() << ": "
              << built->message.toStdString() << std::endl;
    ++failures;
    return false;
}

void checkLicensedContract(ColorComparisonHalconRunner *runner)
{
    const cv::Vec3b green = hsvFullToBgr(85, 255, 180);
    const cv::Mat referenceImage = solidColor(64, 64, green);
    ColorComparisonHalconConfig buildConfig = baseConfig();
    ColorComparisonTemplateBuildResult built;
    if (!buildOrReport(runner, referenceImage, buildConfig, &built, "base"))
        return;

    check(built.model.state == ColorComparisonModelState::Ready &&
          built.model.values.size() == 32 * 32 &&
          built.model.valueHistogram.size() == 32 &&
          built.model.layout == QStringLiteral("hue_major") &&
          built.model.referenceImageHash == colorComparisonReferenceHash(referenceImage) &&
          !built.model.extractParamsHash.isEmpty() &&
          built.model.effectivePixelCount > 0,
          "template build must populate the complete V2 model contract");

    const int greenMaximum = maximumFeatureIndex(built.model.values);
    if (!(greenMaximum >= 0 && greenMaximum / 32 >= 9 && greenMaximum / 32 <= 11 &&
          greenMaximum % 32 >= 30)) {
        std::cerr << "axis diagnostic: maxIndex=" << greenMaximum
                  << " hueBin=" << greenMaximum / 32
                  << " saturationBin=" << greenMaximum % 32 << std::endl;
    }
    check(greenMaximum >= 0 && greenMaximum / 32 >= 9 && greenMaximum / 32 <= 11 &&
          greenMaximum % 32 >= 30,
          "histo_2dim ImageCol=H/ImageRow=S must read row=S/column=H and expose hue-major output");

    ColorComparisonHalconConfig runConfig = buildConfig;
    runConfig.model = built.model;
    const ColorComparisonHalconResult same = runner->run(referenceImage, runConfig);
    check(same.success && same.ok && same.measurementValid &&
          std::abs(same.score - 100.0) < 1e-6 &&
          same.payload.value(QStringLiteral("algorithm")).toString()
              == QStringLiteral("histogram_intersection") &&
          same.payload.value(QStringLiteral("featureType")).toString()
              == QStringLiteral("histogram_hs_2d"),
          "identical image must score 100 with V2 payload semantics");

    const ColorComparisonHalconResult resizedSame =
            runner->run(solidColor(37, 91, green), runConfig);
    check(resizedSame.success && near(resizedSame.score, 100.0, 1e-6),
          "same color distribution at a different resolution must score 100");

    cv::Mat bgraReference;
    cv::cvtColor(referenceImage, bgraReference, cv::COLOR_BGR2BGRA);
    ColorComparisonHalconConfig bgraConfig = buildConfig;
    bgraConfig.inputSignature.pixelFormat = QStringLiteral("BGRA8");
    ColorComparisonTemplateBuildResult bgraBuilt;
    if (buildOrReport(runner, bgraReference, bgraConfig,
                      &bgraBuilt, "BGRA input")) {
        bgraConfig.model = bgraBuilt.model;
        const ColorComparisonHalconResult bgraSame =
                runner->run(bgraReference, bgraConfig);
        check(bgraSame.success && near(bgraSame.score, 100.0, 1e-6),
              "CV_8UC4 input must use the same HALCON V2 color path");
    }

    ColorComparisonHalconConfig changedTemplateConfig = runConfig;
    changedTemplateConfig.templateRoiNormalized = QRectF(0.0, 0.0, 0.75, 1.0);
    const ColorComparisonHalconResult staleTemplate =
            runner->run(referenceImage, changedTemplateConfig);
    check(!staleTemplate.success &&
          staleTemplate.status == QStringLiteral("model_stale"),
          "template extraction geometry changes must invalidate the model hash");

    ColorComparisonHalconConfig changedDetectionConfig = runConfig;
    changedDetectionConfig.detectRoiNormalized = QRectF(0.0, 0.0, 0.5, 1.0);
    const ColorComparisonHalconResult changedDetection =
            runner->run(referenceImage, changedDetectionConfig);
    check(changedDetection.success && near(changedDetection.score, 100.0, 1e-6),
          "detection ROI changes must not invalidate a custom template model");

    ColorComparisonHalconConfig warningConfig = runConfig;
    warningConfig.inputSignature.colorMode = QStringLiteral("unknown");
    warningConfig.positionCorrectionRequested = true;
    warningConfig.positionCorrectionSourceId = QStringLiteral("reserved-source");
    const ColorComparisonHalconResult warningResult =
            runner->run(referenceImage, warningConfig);
    check(warningResult.success && warningResult.measurementValid &&
          warningsContain(warningResult.payload,
                          QStringLiteral("input_color_mode_unknown")) &&
          warningsContain(warningResult.payload,
                          QStringLiteral("position_correction_not_implemented")) &&
          !warningResult.payload.value(QStringLiteral("positionCorrection"))
               .toObject().value(QStringLiteral("applied")).toBool(),
          "unknown color mode and reserved position correction must only add warnings");

    ColorComparisonHalconConfig redConfig = baseConfig();
    redConfig.sensitivity = QStringLiteral("medium");
    const cv::Mat redLowHue = solidColor(64, 64, hsvFullToBgr(1, 220, 180));
    const cv::Mat redHighHue = solidColor(64, 64, hsvFullToBgr(254, 220, 180));
    ColorComparisonTemplateBuildResult redBuilt;
    if (buildOrReport(runner, redLowHue, redConfig, &redBuilt, "red wrap")) {
        redConfig.model = redBuilt.model;
        const ColorComparisonHalconResult redWrap = runner->run(redHighHue, redConfig);
        if (!(redWrap.success && redWrap.score > 99.0)) {
            std::cerr << "red-wrap diagnostic: status="
                      << redWrap.status.toStdString()
                      << " score=" << redWrap.score
                      << " templatePeak=" << maximumFeatureIndex(redBuilt.model.values)
                      << " detectPeak=" << maximumFeatureIndex(redWrap.detectFeature)
                      << std::endl;
        }
        check(redWrap.success && redWrap.score > 99.0,
              "medium tolerance must wrap hue across red bin 31/0");
    }

    ColorComparisonHalconConfig pairedConfig = baseConfig();
    pairedConfig.sensitivity = QStringLiteral("high");
    ColorComparisonTemplateBuildResult pairedBuilt;
    if (buildOrReport(runner, pairedHsImage(false), pairedConfig,
                      &pairedBuilt, "joint HS pairing")) {
        pairedConfig.model = pairedBuilt.model;
        const ColorComparisonHalconResult pairedDifferent =
                runner->run(pairedHsImage(true), pairedConfig);
        if (!(pairedDifferent.success && pairedDifferent.score < 10.0)) {
            std::cerr << "joint-pair diagnostic: status="
                      << pairedDifferent.status.toStdString()
                      << " message=" << pairedDifferent.message.toStdString()
                      << " score=" << pairedDifferent.score << std::endl;
        }
        check(pairedDifferent.success && pairedDifferent.measurementValid &&
              !pairedDifferent.ok && pairedDifferent.score < 10.0,
              "equal H/S marginals with different joint pairings must not match");
    }

    const cv::Vec3b colorA = hsvFullToBgr(35, 210, 180);
    const cv::Vec3b colorB = hsvFullToBgr(135, 210, 180);
    ColorComparisonHalconConfig ratioConfig = baseConfig();
    const cv::Mat ratioReference = verticalMix(64, 64, colorA, colorB, 48);
    const cv::Mat ratioDetection = verticalMix(64, 64, colorA, colorB, 16);
    ColorComparisonTemplateBuildResult ratioBuilt;
    if (buildOrReport(runner, ratioReference, ratioConfig, &ratioBuilt,
                      "multi-color proportions")) {
        ratioConfig.model = ratioBuilt.model;
        const ColorComparisonHalconResult ratio =
                runner->run(ratioDetection, ratioConfig);
        if (!(ratio.success && ratio.score > 45.0 && ratio.score < 55.0)) {
            std::cerr << "ratio diagnostic: status=" << ratio.status.toStdString()
                      << " message=" << ratio.message.toStdString()
                      << " score=" << ratio.score << std::endl;
        }
        check(ratio.success && ratio.measurementValid && !ratio.ok &&
              ratio.score > 45.0 && ratio.score < 55.0,
              "multi-color area-ratio changes must reduce histogram intersection");
        checkResultTextOverlay(ratio.overlays, 1, ratio.score, false,
                               QRectF(0.0, 0.0, 64.0, 64.0),
                               "NG comparison must expose score text at the detection ROI");
    }

    ColorComparisonHalconConfig rectangleConfig = baseConfig();
    rectangleConfig.templateRoiNormalized = QRectF(0.0, 0.0, 0.5, 1.0);
    rectangleConfig.detectRoiNormalized = QRectF(0.5, 0.0, 0.5, 1.0);
    const cv::Mat rectangleReference = verticalMix(64, 64, green, colorB, 32);
    const cv::Mat rectangleDetection = verticalMix(64, 64, colorB, green, 32);
    ColorComparisonTemplateBuildResult rectangleBuilt;
    if (buildOrReport(runner, rectangleReference, rectangleConfig,
                      &rectangleBuilt, "custom rectangle")) {
        rectangleConfig.model = rectangleBuilt.model;
        const ColorComparisonHalconResult rectangle =
                runner->run(rectangleDetection, rectangleConfig);
        check(rectangle.success && near(rectangle.score, 100.0, 1e-6) &&
              rectangle.overlays.size() == 2 &&
              rectangle.overlays.at(0).type == ToolOverlayType::Rect,
              "custom template rectangle and independent detection rectangle must match");
        checkResultTextOverlay(rectangle.overlays, 1, rectangle.score, true,
                               QRectF(32.0, 0.0, 32.0, 64.0),
                               "rectangle comparison must anchor score text to the detection rectangle");
    }

    ColorComparisonHalconConfig circleConfig = baseConfig();
    circleConfig.templateRegionMode = QStringLiteral("sync");
    circleConfig.detectRegionType = QStringLiteral("circle");
    circleConfig.detectCircleCenterNormalized = QPointF(0.5, 0.5);
    circleConfig.detectCircleRadiusNormalized = 0.24;
    cv::Mat circleImage = solidColor(64, 64, colorB);
    cv::circle(circleImage, cv::Point(32, 32), 18,
               cv::Scalar(green[0], green[1], green[2]), cv::FILLED);
    ColorComparisonTemplateBuildResult circleBuilt;
    if (buildOrReport(runner, circleImage, circleConfig, &circleBuilt,
                      "sync circle")) {
        circleConfig.model = circleBuilt.model;
        const ColorComparisonHalconResult circle = runner->run(circleImage, circleConfig);
        const double circleRadiusPixels = 0.24 * 64.0;
        check(circle.success && near(circle.score, 100.0, 1e-6) &&
              circle.overlays.size() == 2 &&
              circle.overlays.at(0).type == ToolOverlayType::Circle,
              "sync mode must build and run with the actual circular region");
        checkResultTextOverlay(circle.overlays, 1, circle.score, true,
                               QRectF(32.0 - circleRadiusPixels,
                                      32.0 - circleRadiusPixels,
                                      circleRadiusPixels * 2.0,
                                      circleRadiusPixels * 2.0),
                               "circle comparison must anchor score text to the circle bounds");
    }

    {
        constexpr int imageWidth = 384;
        constexpr int imageHeight = 103;
        constexpr double radiusPixels = 30.0;
        constexpr double pi = 3.14159265358979323846;
        const double radiusNormalized = radiusPixels / imageWidth;
        const double expectedArea = pi * radiusPixels * radiusPixels;
        const cv::Mat landscapeImage = solidColor(imageHeight, imageWidth, green);

        ColorComparisonHalconConfig landscapeCircleConfig = baseConfig();
        landscapeCircleConfig.templateRegionMode = QStringLiteral("sync");
        landscapeCircleConfig.detectRegionType = QStringLiteral("circle");
        landscapeCircleConfig.detectCircleCenterNormalized = QPointF(0.5, 0.5);
        landscapeCircleConfig.detectCircleRadiusNormalized = radiusNormalized;

        ColorComparisonTemplateBuildResult landscapeBuilt;
        if (buildOrReport(runner,
                          landscapeImage,
                          landscapeCircleConfig,
                          &landscapeBuilt,
                          "landscape sync circle")) {
            check(std::abs(static_cast<double>(landscapeBuilt.model.effectivePixelCount)
                           - expectedArea) <= expectedArea * 0.03,
                  "landscape circle template area must use radius normalized by the maximum image side");

            landscapeCircleConfig.model = landscapeBuilt.model;
            const ColorComparisonHalconResult landscape =
                    runner->run(landscapeImage, landscapeCircleConfig);
            check(landscape.success &&
                  std::abs(landscape.payload
                               .value(QStringLiteral("effectiveDetectionPixels"))
                               .toDouble() - expectedArea) <= expectedArea * 0.03,
                  "landscape circle detection area must preserve the user-drawn pixel radius");
            check(landscape.success && landscape.overlays.size() == 2 &&
                  landscape.overlays.at(0).type == ToolOverlayType::Circle &&
                  near(landscape.overlays.at(0).center.x(), imageWidth * 0.5, 1e-9) &&
                  near(landscape.overlays.at(0).center.y(), imageHeight * 0.5, 1e-9) &&
                  near(landscape.overlays.at(0).radius, radiusPixels, 1e-9),
                  "landscape circle overlay must use the same pixel center and maximum-side radius");
            checkResultTextOverlay(landscape.overlays, 1, landscape.score, true,
                                   QRectF(imageWidth * 0.5 - radiusPixels,
                                          imageHeight * 0.5 - radiusPixels,
                                          radiusPixels * 2.0,
                                          radiusPixels * 2.0),
                                   "landscape circle score text must use the same pixel geometry");

            const QJsonObject detectionRoi = landscape.payload
                    .value(QStringLiteral("detectionRoi")).toObject();
            check(detectionRoi.value(QStringLiteral("type")).toString()
                      == QStringLiteral("circle") &&
                  near(detectionRoi.value(QStringLiteral("radius")).toDouble(),
                       radiusNormalized,
                       1e-12) &&
                  near(detectionRoi.value(QStringLiteral("radiusNormalized")).toDouble(),
                       radiusNormalized,
                       1e-12) &&
                  near(detectionRoi.value(QStringLiteral("radiusPixels")).toDouble(),
                       radiusPixels,
                       1e-9) &&
                  near(detectionRoi.value(QStringLiteral("width")).toDouble(),
                       2.0 * radiusPixels / imageWidth,
                       1e-12) &&
                  near(detectionRoi.value(QStringLiteral("height")).toDouble(),
                       2.0 * radiusPixels / imageHeight,
                       1e-12),
                  "landscape circle payload must expose maximum-side radius and aspect-correct bounds");
        }
    }

    {
        ColorComparisonHalconConfig sync = baseConfig();
        sync.templateRegionMode = QStringLiteral("sync");
        sync.detectMaskPolygonNormalized = {
            QPointF(0.0, 0.0), QPointF(0.25, 0.0),
            QPointF(0.25, 1.0), QPointF(0.0, 1.0)
        };
        sync.templateMaskPolygonNormalized = {
            QPointF(0.75, 0.0), QPointF(1.0, 0.0),
            QPointF(1.0, 1.0), QPointF(0.75, 1.0)
        };
        const ColorComparisonTemplateBuildResult built =
                runner->buildTemplateModel(referenceImage, sync);
        ColorComparisonHalconConfig templateOnly = sync;
        templateOnly.detectMaskPolygonNormalized.clear();
        const ColorComparisonTemplateBuildResult templateOnlyBuilt =
                runner->buildTemplateModel(referenceImage, templateOnly);
        check(built.success && templateOnlyBuilt.success &&
              built.model.effectivePixelCount
                  < templateOnlyBuilt.model.effectivePixelCount,
              "sync template build must apply detection and template masks");

        ColorComparisonHalconConfig changedSync = sync;
        changedSync.model = built.model;
        changedSync.detectMaskPolygonNormalized[1].setX(0.30);
        const ColorComparisonHalconResult stale = runner->run(referenceImage, changedSync);
        check(!stale.success && stale.status == QStringLiteral("model_stale"),
              "sync detection mask changes must stale the model");

        ColorComparisonHalconConfig custom = sync;
        custom.templateRegionMode = QStringLiteral("custom");
        custom.detectMaskPolygonNormalized.clear();
        const ColorComparisonTemplateBuildResult customBuilt =
                runner->buildTemplateModel(referenceImage, custom);
        check(customBuilt.success,
              "custom compatibility fixture must build before model validation");
        const QJsonObject customExtractParams = customBuilt.payload
                .value(QStringLiteral("extractParams")).toObject();
        const QString legacyCustomHash = colorComparisonExtractParamsHash(
                    legacyCustomExtractParams(custom.templateMaskPolygonNormalized));
        check(customBuilt.success &&
              customBuilt.model.extractParamsHash != legacyCustomHash,
              "histogram coordinate contract revision must invalidate legacy V2 extract hashes");
        check(customBuilt.success &&
              customBuilt.payload.value(QStringLiteral("extractParams")).isObject() &&
              customExtractParams
                  .value(QStringLiteral("histogramCoordinateContract"))
                  .toString()
                  == QStringLiteral("image_col_h_image_row_s_row_s_column_h") &&
              !customExtractParams.contains(
                  QStringLiteral("syncDetectionMaskPolygon")),
              "custom extract payload must identify the histogram coordinate contract and omit the sync mask key");

        if (customBuilt.success) {
            ColorComparisonHalconConfig legacyModel = custom;
            legacyModel.model = customBuilt.model;
            legacyModel.model.extractParamsHash = legacyCustomHash;
            const ColorComparisonHalconResult rejectedLegacy =
                    runner->run(referenceImage, legacyModel);
            check(!rejectedLegacy.success &&
                  rejectedLegacy.status == QStringLiteral("model_stale"),
                  "models hashed without the histogram coordinate contract revision must be stale");
        }

        if (customBuilt.success) {
            custom.model = customBuilt.model;
            custom.detectMaskPolygonNormalized = sync.detectMaskPolygonNormalized;
            custom.halconSoPath = QCoreApplication::applicationFilePath();
            const ColorComparisonHalconResult customResult =
                    runner->run(referenceImage, custom);
            check(!customResult.success &&
                  customResult.status == QStringLiteral("halcon_load_failed"),
                  "custom detection mask changes must pass validation before forced load failure");
        }
    }

    const QVector<QPointF> rightHalfMask = {
        QPointF(0.5, 0.0), QPointF(1.0, 0.0),
        QPointF(1.0, 1.0), QPointF(0.5, 1.0)
    };
    ColorComparisonHalconConfig templateMaskConfig = baseConfig();
    templateMaskConfig.templateMaskPolygonNormalized = rightHalfMask;
    const cv::Mat maskedReference = verticalMix(64, 64, green, colorB, 32);
    ColorComparisonTemplateBuildResult templateMaskBuilt;
    if (buildOrReport(runner, maskedReference, templateMaskConfig,
                      &templateMaskBuilt, "template mask")) {
        templateMaskConfig.model = templateMaskBuilt.model;
        const ColorComparisonHalconResult templateMasked =
                runner->run(referenceImage, templateMaskConfig);
        check(templateMasked.success && templateMasked.score > 99.0 &&
              templateMaskBuilt.model.effectivePixelCount < 64 * 64,
              "template mask must be subtracted from the final template region");
    }

    ColorComparisonHalconConfig detectMaskConfig = runConfig;
    detectMaskConfig.detectMaskPolygonNormalized = rightHalfMask;
    const cv::Mat detectMaskedImage = verticalMix(64, 64, green, colorB, 32);
    const ColorComparisonHalconResult detectMasked =
            runner->run(detectMaskedImage, detectMaskConfig);
    check(detectMasked.success && detectMasked.score > 99.0 &&
          detectMasked.overlays.size() == 3 &&
          detectMasked.overlays.at(0).type == ToolOverlayType::Rect &&
          detectMasked.overlays.at(1).type == ToolOverlayType::Polygon,
          "detection mask overlays must retain geometry before the result text");
    checkResultTextOverlay(detectMasked.overlays, 2, detectMasked.score, true,
                           QRectF(0.0, 0.0, 64.0, 64.0),
                           "masked comparison must keep score text after detection geometry");

    ColorComparisonHalconConfig fullyMaskedConfig = runConfig;
    fullyMaskedConfig.detectMaskPolygonNormalized = {
        QPointF(0.0, 0.0), QPointF(1.0, 0.0),
        QPointF(1.0, 1.0), QPointF(0.0, 1.0)
    };
    const ColorComparisonHalconResult fullyMasked =
            runner->run(referenceImage, fullyMaskedConfig);
    check(!fullyMasked.success &&
          fullyMasked.status == QStringLiteral("detect_masked_empty"),
          "fully masked detection region must fail explicitly");

    ColorComparisonHalconConfig compensationConfig = baseConfig();
    compensationConfig.brightnessCompensation = true;
    const cv::Mat brightnessReference =
            solidColor(64, 64, hsvFullToBgr(70, 160, 120));
    const cv::Mat darkerDetection =
            solidColor(64, 64, hsvFullToBgr(70, 160, 100));
    ColorComparisonTemplateBuildResult compensationBuilt;
    if (buildOrReport(runner, brightnessReference, compensationConfig,
                      &compensationBuilt, "brightness compensation")) {
        compensationConfig.model = compensationBuilt.model;
        const ColorComparisonHalconResult compensated =
                runner->run(darkerDetection, compensationConfig);
        const QJsonObject diagnostics = compensated.payload
                .value(QStringLiteral("brightnessCompensation")).toObject();
        check(compensated.success && compensated.measurementValid &&
              diagnostics.value(QStringLiteral("enabled")).toBool() &&
              diagnostics.value(QStringLiteral("applied")).toBool() &&
              diagnostics.value(QStringLiteral("templateMean")).toDouble() > 115.0 &&
              diagnostics.value(QStringLiteral("templateMean")).toDouble() < 125.0 &&
              diagnostics.value(QStringLiteral("detectMeanBefore")).toDouble() > 95.0 &&
              diagnostics.value(QStringLiteral("detectMeanBefore")).toDouble() < 105.0 &&
              diagnostics.value(QStringLiteral("detectMeanAfter")).toDouble() > 115.0 &&
              diagnostics.value(QStringLiteral("detectMeanAfter")).toDouble() < 125.0 &&
              diagnostics.value(QStringLiteral("scale")).toDouble() > 1.15 &&
              diagnostics.value(QStringLiteral("scale")).toDouble() < 1.25 &&
              diagnostics.value(QStringLiteral("clippedRatio")).toDouble() <= 0.02,
              "brightness compensation must report HALCON normalization diagnostics");

        const ColorComparisonHalconResult tooDark = runner->run(
                    solidColor(64, 64, hsvFullToBgr(70, 160, 4)),
                    compensationConfig);
        check(!tooDark.success && tooDark.status == QStringLiteral("invalid_illumination"),
              "out-of-range detection brightness must fail as invalid illumination");

        const cv::Mat clippingDetection = verticalMix(
                    64,
                    64,
                    hsvFullToBgr(70, 160, 240),
                    hsvFullToBgr(70, 160, 100),
                    4);
        const ColorComparisonHalconResult clipping =
                runner->run(clippingDetection, compensationConfig);
        check(!clipping.success &&
              clipping.status == QStringLiteral("invalid_illumination") &&
              clipping.payload.value(QStringLiteral("brightnessCompensation"))
                  .toObject().value(QStringLiteral("clippedRatio")).toDouble() > 0.02,
              "compensation that clips over two percent of effective pixels must fail with diagnostics");
    }

    ColorComparisonHalconConfig downscaleConfig = baseConfig();
    downscaleConfig.brightnessCompensation = true;
    const cv::Mat downscaleReference =
            solidColor(64, 64, hsvFullToBgr(70, 160, 200));
    ColorComparisonTemplateBuildResult downscaleBuilt;
    if (buildOrReport(runner, downscaleReference, downscaleConfig,
                      &downscaleBuilt, "brightness downscale clipping")) {
        downscaleConfig.model = downscaleBuilt.model;
        const cv::Mat naturallyBrightDetection = verticalMix(
                    64,
                    64,
                    hsvFullToBgr(70, 160, 255),
                    hsvFullToBgr(70, 160, 198),
                    4);
        const ColorComparisonHalconResult downscaled =
                runner->run(naturallyBrightDetection, downscaleConfig);
        const QJsonObject diagnostics = downscaled.payload
                .value(QStringLiteral("brightnessCompensation")).toObject();
        check(downscaled.success &&
              diagnostics.value(QStringLiteral("scale")).toDouble() < 1.0 &&
              near(diagnostics.value(QStringLiteral("clippedRatio")).toDouble(),
                   0.0,
                   1e-12),
              "brightness downscaling must not report natural highlights as compensation clipping");
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    HalconRuntimePaths::initializeHalconEnvironment();

    ColorComparisonHalconRunner runner;
    checkRunnerSourceContract();
    checkDefaultContract(&runner);

    if (!qEnvironmentVariableIsSet("RUN_HALCON_LICENSED_SMOKE")) {
        if (failures != 0)
            return 1;
        std::cerr << "Color comparison V2 preflight passed; licensed HALCON checks were skipped. "
                  << "Set RUN_HALCON_LICENSED_SMOKE=1 to require licensed checks."
                  << std::endl;
        return 0;
    }

    checkLicensedContract(&runner);
    if (failures != 0) {
        std::cerr << "Color comparison V2 licensed smoke failed with "
                  << failures << " failure(s)." << std::endl;
        return 1;
    }

    std::cerr << "Color comparison V2 licensed smoke passed." << std::endl;
    return 0;
}
