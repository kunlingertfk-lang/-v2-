#include "algorithms/recognition/ColorComparisonModel.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#include <opencv2/core.hpp>

namespace {

int failureCount = 0;

void check(bool condition, const char *message)
{
    if (condition)
        return;

    std::cerr << "FAIL: " << message << std::endl;
    ++failureCount;
}

ColorComparisonModelV2 readyModel()
{
    ColorComparisonModelV2 model;
    model.state = ColorComparisonModelState::Ready;
    model.featureType = QStringLiteral("histogram_hs_2d");
    model.algorithm = QStringLiteral("histogram_intersection");
    model.colorSpace = QStringLiteral("hsv");
    model.hueBins = 32;
    model.saturationBins = 32;
    model.layout = QStringLiteral("hue_major");
    model.normalized = true;
    model.values = QVector<double>(1024, 0.0);
    model.values[5 * 32 + 20] = 1.0;
    model.valueHistogram = QVector<double>(32, 0.0);
    model.valueHistogram[18] = 1.0;
    model.effectivePixelCount = 4096;
    model.referenceImageHash = QStringLiteral("reference-hash");
    model.extractParamsHash = QStringLiteral("extract-hash");
    model.inputSignature.colorMode = QStringLiteral("color");
    model.inputSignature.pixelFormat = QStringLiteral("BGR8");
    model.inputSignature.bitDepth = 8;
    model.inputSignature.whiteBalance =
            QJsonObject{{QStringLiteral("mode"), QStringLiteral("manual")}};
    model.inputSignature.ccm = QJsonArray{1.0, 0.0, 0.0};
    model.inputSignature.exposure = 12.5;
    model.inputSignature.gain = 1.25;
    model.brightnessReference.mean = 118.5;
    model.brightnessReference.deviation = 7.25;
    return model;
}

void checkInvalid(const ColorComparisonModelV2 &model, const char *message)
{
    const ColorComparisonModelValidation validation =
            validateColorComparisonModel(model);
    check(!validation.success && validation.status == QStringLiteral("model_invalid"),
          message);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const ColorComparisonModelV2 model = readyModel();
    check(validateColorComparisonModel(model).success,
          "valid V2 model must pass strict validation");

    const QJsonObject root{
        {QStringLiteral("version"), 2},
        {QStringLiteral("model"), colorComparisonModelToJson(model)}
    };
    const ColorComparisonModelReadResult roundTrip =
            readColorComparisonModel(root, true);
    check(roundTrip.success
                  && roundTrip.model.state == ColorComparisonModelState::Ready
                  && roundTrip.model.values.size() == 1024,
          "V2 model must round-trip as ready");
    check(roundTrip.model.inputSignature.colorMode == QStringLiteral("color")
                  && roundTrip.model.inputSignature.pixelFormat == QStringLiteral("BGR8")
                  && roundTrip.model.inputSignature.bitDepth == 8
                  && roundTrip.model.inputSignature.whiteBalance
                          == model.inputSignature.whiteBalance
                  && roundTrip.model.inputSignature.ccm == model.inputSignature.ccm
                  && roundTrip.model.inputSignature.exposure
                          == model.inputSignature.exposure
                  && roundTrip.model.inputSignature.gain == model.inputSignature.gain,
          "V2 round-trip must preserve its full input signature");
    check(roundTrip.model.values == model.values
                  && roundTrip.model.valueHistogram == model.valueHistogram
                  && roundTrip.model.effectivePixelCount == model.effectivePixelCount
                  && roundTrip.model.referenceImageHash == model.referenceImageHash
                  && roundTrip.model.extractParamsHash == model.extractParamsHash
                  && roundTrip.model.brightnessReference.mean
                          == model.brightnessReference.mean
                  && roundTrip.model.brightnessReference.deviation
                          == model.brightnessReference.deviation,
          "V2 round-trip must preserve every runner-consumed model field");

    QJsonObject nonExactCountRoot = root;
    QJsonObject nonExactCountModel =
            nonExactCountRoot.value(QStringLiteral("model")).toObject();
    nonExactCountModel.insert(QStringLiteral("effectivePixelCount"),
                              9007199254740992.0);
    nonExactCountRoot.insert(QStringLiteral("model"), nonExactCountModel);
    const ColorComparisonModelReadResult nonExactCount =
            readColorComparisonModel(nonExactCountRoot, true);
    check(!nonExactCount.success
                  && nonExactCount.status == QStringLiteral("model_invalid")
                  && nonExactCount.model.effectivePixelCount == 0,
          "effective pixel count above 2^53-1 must be rejected before conversion");

    QJsonObject outOfRangeRoot = root;
    QJsonObject outOfRangeModel =
            outOfRangeRoot.value(QStringLiteral("model")).toObject();
    outOfRangeModel.insert(QStringLiteral("effectivePixelCount"), 1.0e20);
    outOfRangeRoot.insert(QStringLiteral("model"), outOfRangeModel);
    const ColorComparisonModelReadResult outOfRange =
            readColorComparisonModel(outOfRangeRoot, true);
    check(!outOfRange.success
                  && outOfRange.status == QStringLiteral("model_invalid")
                  && outOfRange.model.effectivePixelCount == 0,
          "out-of-range effective pixel count must be rejected without narrowing");

    QJsonObject outOfRangeBitDepthRoot = root;
    QJsonObject outOfRangeBitDepthModel =
            outOfRangeBitDepthRoot.value(QStringLiteral("model")).toObject();
    QJsonObject outOfRangeSignature =
            outOfRangeBitDepthModel.value(QStringLiteral("inputSignature")).toObject();
    outOfRangeSignature.insert(QStringLiteral("bitDepth"), 2147483648.0);
    outOfRangeBitDepthModel.insert(QStringLiteral("inputSignature"),
                                   outOfRangeSignature);
    outOfRangeBitDepthRoot.insert(QStringLiteral("model"), outOfRangeBitDepthModel);
    const ColorComparisonModelReadResult outOfRangeBitDepth =
            readColorComparisonModel(outOfRangeBitDepthRoot, true);
    check(!outOfRangeBitDepth.success
                  && outOfRangeBitDepth.status == QStringLiteral("model_invalid")
                  && outOfRangeBitDepth.model.inputSignature.bitDepth == -1,
          "out-of-int-range bit depth must be rejected before conversion");

    QJsonObject legacy;
    legacy.insert(QStringLiteral("version"), 1);
    legacy.insert(QStringLiteral("templateFeature"), QJsonArray{1.0, 0.0});
    const ColorComparisonModelReadResult legacyWithReference =
            readColorComparisonModel(legacy, true);
    check(!legacyWithReference.success
                  && legacyWithReference.requiresRebuild
                  && legacyWithReference.model.state == ColorComparisonModelState::Stale
                  && legacyWithReference.status == QStringLiteral("model_stale"),
          "V1 with reference must require re-sampling");
    const ColorComparisonModelReadResult legacyWithoutReference =
            readColorComparisonModel(legacy, false);
    check(!legacyWithoutReference.success
                  && legacyWithoutReference.requiresRebuild
                  && legacyWithoutReference.model.state == ColorComparisonModelState::Unsupported
                  && legacyWithoutReference.status == QStringLiteral("model_rebuild_required"),
          "V1 without reference must be unsupported");

    QJsonObject unknownVersion;
    unknownVersion.insert(QStringLiteral("version"), 99);
    const ColorComparisonModelReadResult unsupported =
            readColorComparisonModel(unknownVersion, true);
    check(!unsupported.success
                  && unsupported.model.state == ColorComparisonModelState::Unsupported
                  && unsupported.status == QStringLiteral("unsupported_model_version"),
          "unknown model versions must be rejected explicitly");

    ColorComparisonModelV2 invalid = model;
    invalid.inputSignature.bitDepth = 16;
    checkInvalid(invalid, "ready V2 model bit depth must equal 8");

    invalid = model;
    invalid.effectivePixelCount = 9007199254740992LL;
    checkInvalid(invalid,
                 "ready V2 model effective pixel count above 2^53-1 must be invalid");

    ColorComparisonModelV2 maximumExactCount = model;
    maximumExactCount.effectivePixelCount = 9007199254740991LL;
    check(validateColorComparisonModel(maximumExactCount).success,
          "2^53-1 must remain a valid effective pixel count boundary");
    const QJsonObject maximumExactCountRoot{
        {QStringLiteral("version"), 2},
        {QStringLiteral("model"), colorComparisonModelToJson(maximumExactCount)}
    };
    const ColorComparisonModelReadResult maximumExactCountRoundTrip =
            readColorComparisonModel(maximumExactCountRoot, true);
    check(maximumExactCountRoundTrip.success
                  && maximumExactCountRoundTrip.model.effectivePixelCount
                          == 9007199254740991LL,
          "2^53-1 effective pixel count must round-trip exactly");

    invalid = model;
    invalid.values.resize(1023);
    checkInvalid(invalid, "1023-dimensional HS model must be invalid");

    invalid = model;
    invalid.values.resize(1025);
    checkInvalid(invalid, "1025-dimensional HS model must be invalid");

    invalid = model;
    invalid.values[0] = std::numeric_limits<double>::quiet_NaN();
    checkInvalid(invalid, "NaN HS values must be invalid");

    invalid = model;
    invalid.values[0] = -0.01;
    checkInvalid(invalid, "negative HS values must be invalid");

    invalid = model;
    invalid.values[0] = 0.25;
    checkInvalid(invalid, "HS values whose sum is not one must be invalid");

    invalid = model;
    invalid.hueBins = 31;
    checkInvalid(invalid, "wrong hue bins must be invalid");

    invalid = model;
    invalid.saturationBins = 31;
    checkInvalid(invalid, "wrong saturation bins must be invalid");

    invalid = model;
    invalid.layout = QStringLiteral("saturation_major");
    checkInvalid(invalid, "wrong histogram layout must be invalid");

    invalid = model;
    invalid.referenceImageHash.clear();
    checkInvalid(invalid, "empty reference image hash must be invalid");

    invalid = model;
    invalid.extractParamsHash.clear();
    checkInvalid(invalid, "empty extract params hash must be invalid");

    invalid = model;
    invalid.effectivePixelCount = 0;
    checkInvalid(invalid, "zero effective pixel count must be invalid");

    invalid = model;
    invalid.effectivePixelCount = -1;
    checkInvalid(invalid, "negative effective pixel count must be invalid");

    invalid = model;
    invalid.valueHistogram.resize(31);
    checkInvalid(invalid, "31-dimensional V histogram must be invalid");

    const ColorComparisonModelValidation staleParams =
            validateColorComparisonModel(model, QStringLiteral("changed-extract-hash"));
    check(!staleParams.success && staleParams.status == QStringLiteral("model_stale"),
          "changed extraction parameters must mark a valid model stale");

    cv::Mat reference(3, 4, CV_8UC3, cv::Scalar(10, 20, 30));
    const QString referenceHash = colorComparisonReferenceHash(reference);
    check(!referenceHash.isEmpty()
                  && referenceHash == colorComparisonReferenceHash(reference.clone()),
          "identical image inputs must have a stable reference hash");

    cv::Mat paddedReference(3, 6, CV_8UC3, cv::Scalar(99, 99, 99));
    cv::Mat referenceRoi = paddedReference(cv::Rect(1, 0, 4, 3));
    referenceRoi.setTo(cv::Scalar(10, 20, 30));
    check(!referenceRoi.isContinuous()
                  && referenceHash == colorComparisonReferenceHash(referenceRoi)
                  && referenceHash == colorComparisonReferenceHash(referenceRoi.clone()),
          "reference hash must ignore non-contiguous Mat row padding");
    check(referenceHash
                  != colorComparisonReferenceHash(
                          cv::Mat(2, 6, CV_8UC3, cv::Scalar(10, 20, 30)))
                  && referenceHash
                          != colorComparisonReferenceHash(
                                  cv::Mat(3, 4, CV_8UC1, cv::Scalar(10))),
          "reference hash must include Mat shape and type");

    reference.at<cv::Vec3b>(1, 2)[0] = 11;
    check(referenceHash != colorComparisonReferenceHash(reference),
          "changed image bytes must change the reference hash");

    QJsonObject extractParams{
        {QStringLiteral("templateRegionMode"), QStringLiteral("custom")},
        {QStringLiteral("hueBins"), 32},
        {QStringLiteral("brightnessCompensation"), false}
    };
    const QString extractHash = colorComparisonExtractParamsHash(extractParams);
    check(!extractHash.isEmpty()
                  && extractHash == colorComparisonExtractParamsHash(extractParams),
          "identical extraction parameters must have a stable hash");
    extractParams.insert(QStringLiteral("hueBins"), 31);
    check(extractHash != colorComparisonExtractParamsHash(extractParams),
          "changed extraction parameters must change the hash");

    if (failureCount == 0) {
        std::cout << "color_comparison_model_smoke: V2 model contract checks passed"
                  << std::endl;
    }
    return failureCount == 0 ? 0 : 1;
}
