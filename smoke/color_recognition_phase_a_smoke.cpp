#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/recognition/ColorRecognitionHalconRunner.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QtGlobal>

#include <cmath>
#include <iostream>
#include <opencv2/core.hpp>

namespace {

bool near(double lhs, double rhs, double tolerance = 1e-6)
{
    return std::abs(lhs - rhs) <= tolerance;
}

double sum(const QVector<double> &values)
{
    double total = 0.0;
    for (double value : values)
        total += value;
    return total;
}

double hueMass(const QVector<double> &feature, int bins, int hueBin)
{
    double total = 0.0;
    for (int saturationBin = 0; saturationBin < bins; ++saturationBin)
        total += feature.at(hueBin * bins + saturationBin);
    return total;
}

double saturationMass(const QVector<double> &feature, int bins, int saturationBin)
{
    double total = 0.0;
    for (int hueBin = 0; hueBin < bins; ++hueBin)
        total += feature.at(hueBin * bins + saturationBin);
    return total;
}

bool sameFeature(const QVector<double> &lhs,
                 const QVector<double> &rhs,
                 double tolerance)
{
    if (lhs.size() != rhs.size())
        return false;
    for (int i = 0; i < lhs.size(); ++i) {
        if (!near(lhs.at(i), rhs.at(i), tolerance))
            return false;
    }
    return true;
}

ColorRecognitionHalconConfig baseConfig()
{
    ColorRecognitionHalconConfig config;
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(), &config.halconSoPathCandidates);
    config.featureType = QStringLiteral("histogram_2dim_hs");
    config.sensitivity = QStringLiteral("medium");
    config.brightnessEnabled = false;
    return config;
}

int fail(const char *message)
{
    std::cerr << message << std::endl;
    return 1;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    ColorRecognitionHalconRunner runner;
    const ColorRecognitionHalconConfig config = baseConfig();

    const cv::Mat mono(24, 24, CV_8UC1, cv::Scalar(128));
    const ColorRecognitionHalconFeatureResult monoResult = runner.extractFeature(mono, config);
    if (monoResult.success ||
        monoResult.status != QStringLiteral("unsupported_mono_for_color_recognition")) {
        return fail("Mono input must be rejected explicitly");
    }

    const cv::Mat color16Missing(24, 24, CV_16UC3, cv::Scalar(1000, 2000, 3000));
    const ColorRecognitionHalconFeatureResult missingMetadata =
            runner.extractFeature(color16Missing, config);
    if (missingMetadata.success ||
        missingMetadata.status != QStringLiteral("missing_pixel_format_metadata")) {
        return fail("16-bit input without validBits/bitShift must be rejected");
    }

    const cv::Mat red8(24, 24, CV_8UC3, cv::Scalar(0, 0, 255));

    const ColorRecognitionHalconFeatureResult redFeature = runner.extractFeature(red8, config);
    if (!redFeature.success || redFeature.feature.size() != 16 * 16) {
        std::cerr << redFeature.status.toStdString() << ": "
                  << redFeature.message.toStdString() << ", size="
                  << redFeature.feature.size() << std::endl;
        return fail("2D H/S feature extraction failed");
    }
    if (!near(sum(redFeature.feature), 1.0) ||
        !near(hueMass(redFeature.feature, 16, 0), 0.25) ||
        !near(hueMass(redFeature.feature, 16, 14), 0.25) ||
        !near(hueMass(redFeature.feature, 16, 15), 0.50)) {
        std::cerr << "hue masses: first=" << hueMass(redFeature.feature, 16, 0)
                  << ", penultimate=" << hueMass(redFeature.feature, 16, 14)
                  << ", last=" << hueMass(redFeature.feature, 16, 15)
                  << ", sum=" << sum(redFeature.feature) << std::endl;
        return fail("Hue smoothing must wrap across first/last bins");
    }
    if (redFeature.payload.value(QStringLiteral("featureType")).toString()
            != QStringLiteral("histogram_2dim_hs") ||
        redFeature.payload.value(QStringLiteral("featureSchemaVersion")).toString()
            != QStringLiteral("hs_joint_histogram_v2") ||
        !redFeature.payload.value(QStringLiteral("featureSignature")).toString()
            .startsWith(QStringLiteral("sha256:"))) {
        return fail("Actual feature contract is missing from payload");
    }

    const cv::Mat gray8(24, 24, CV_8UC3, cv::Scalar(128, 128, 128));
    const ColorRecognitionHalconFeatureResult grayFeature = runner.extractFeature(gray8, config);
    if (!grayFeature.success ||
        saturationMass(grayFeature.feature, 16, 15) > 1e-9 ||
        saturationMass(grayFeature.feature, 16, 0) < 0.80 ||
        saturationMass(grayFeature.feature, 16, 1) < 0.15) {
        return fail("Saturation smoothing must not wrap at the boundary");
    }

    ColorRecognitionHalconConfig runConfig = config;
    runConfig.labels = {{QStringLiteral("red"), 1}};
    ColorRecognitionHalconSample sample;
    sample.label = QStringLiteral("red");
    sample.classId = 1;
    sample.feature = redFeature.feature;
    sample.featureSignature = redFeature.payload
            .value(QStringLiteral("featureSignature")).toString();
    runConfig.samples = {sample};
    const ColorRecognitionHalconResult matched = runner.run(red8, runConfig);
    if (!matched.success || matched.predictedLabel != QStringLiteral("red") ||
        matched.payload.value(QStringLiteral("featureAlignmentApplied")).toBool(true) ||
        matched.payload.value(QStringLiteral("colorDecisionMode")).toString()
            != QStringLiteral("category_similarity") ||
        matched.payload.contains(QStringLiteral("dominantColorRatio")) ||
        matched.payload.contains(QStringLiteral("labelAreaRatios"))) {
        return fail("Strictly signed sample should classify without feature alignment");
    }

    ColorRecognitionHalconConfig positionConfig = runConfig;
    positionConfig.roiNormalized = QRectF(0.0, 0.0, 0.5, 1.0);
    positionConfig.enablePositionCorrection = true;
    positionConfig.positionCorrectionSourceId =
            QStringLiteral("reference.positionCorrection");
    positionConfig.positionCorrectionSource =
            QStringLiteral("0 基准图.位置修正信息");
    positionConfig.positionCorrection.requested = true;
    positionConfig.positionCorrection.applied = true;
    positionConfig.positionCorrection.sourceId =
            QStringLiteral("reference.positionCorrection");
    positionConfig.positionCorrection.referenceToRunHomMat2D =
            {1.0, 0.0, 0.0, 0.0, 1.0, 12.0};
    const ColorRecognitionHalconResult positionResult =
            runner.run(red8, positionConfig);
    if (!positionResult.success ||
        !positionResult.payload.value(
            QStringLiteral("positionCorrectionApplied")).toBool(false) ||
        positionResult.overlays.isEmpty() ||
        positionResult.overlays.first().extra.value(
            QStringLiteral("role")).toString() != QStringLiteral("detect_roi")) {
        return fail("Position correction must transform the HSV HALCON ROI");
    }

    ColorRecognitionHalconConfig lowConfidenceConfig = runConfig;
    lowConfidenceConfig.judgeMode = QStringLiteral("category");
    lowConfidenceConfig.expectedLabel = QStringLiteral("red");
    lowConfidenceConfig.minScore = 50;
    lowConfidenceConfig.samples[0].feature.fill(0.0);
    const ColorRecognitionHalconResult lowConfidence =
            runner.run(red8, lowConfidenceConfig);
    if (!lowConfidence.success || lowConfidence.ok || lowConfidence.score != 0.0) {
        return fail("Category judgement must also enforce the minimum confidence");
    }

    ColorRecognitionHalconConfig staleConfig = runConfig;
    staleConfig.sensitivity = QStringLiteral("high");
    const ColorRecognitionHalconResult stale = runner.run(red8, staleConfig);
    if (stale.success || stale.status != QStringLiteral("model_stale_needs_resample"))
        return fail("Parameter changes must mark the model stale");

    cv::Mat red16(24, 24, CV_16UC3, cv::Scalar(0, 0, 65535));
    ColorRecognitionHalconConfig config16 = config;
    config16.pixelFormat = QStringLiteral("BGR16");
    config16.validBits = 16;
    config16.bitShift = 0;
    const ColorRecognitionHalconFeatureResult red16Feature = runner.extractFeature(red16, config16);
    if (!red16Feature.success ||
        !sameFeature(redFeature.feature, red16Feature.feature, 1e-6)) {
        return fail("8-bit and normalized 16-bit features must be equivalent");
    }

    cv::Mat red12Left(24, 24, CV_16UC3, cv::Scalar(0, 0, 4095U << 4));
    ColorRecognitionHalconConfig config12 = config16;
    config12.pixelFormat = QStringLiteral("BGR12Left");
    config12.validBits = 12;
    config12.bitShift = 4;
    const ColorRecognitionHalconFeatureResult red12Feature =
            runner.extractFeature(red12Left, config12);
    if (!red12Feature.success ||
        !sameFeature(redFeature.feature, red12Feature.feature, 1e-6)) {
        return fail("Left-aligned 12-bit and 8-bit features must be equivalent");
    }

    return 0;
}
