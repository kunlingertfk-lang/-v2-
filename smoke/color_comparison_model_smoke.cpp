#include "algorithms/recognition/ColorComparisonModel.h"
#include "tooladapters/ColorComparisonAdapter.h"

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

QJsonObject pointJson(double x, double y)
{
    return {
        {QStringLiteral("x"), x},
        {QStringLiteral("y"), y}
    };
}

QJsonObject rectJson(double x, double y, double width, double height)
{
    return {
        {QStringLiteral("x"), x},
        {QStringLiteral("y"), y},
        {QStringLiteral("width"), width},
        {QStringLiteral("height"), height}
    };
}

QString defaultAdapterExtractHash()
{
    const QJsonObject extractParams{
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
             {QStringLiteral("rect"), rectJson(0.0, 0.0, 1.0, 1.0)}
         }},
        {QStringLiteral("templateMaskPolygon"), QJsonArray()},
        {QStringLiteral("brightnessCompensation"), false},
        {QStringLiteral("brightnessConstants"), QJsonObject{
             {QStringLiteral("minMean"), 8.0},
             {QStringLiteral("maxMean"), 247.0},
             {QStringLiteral("minScale"), 0.75},
             {QStringLiteral("maxScale"), 1.3333333333},
             {QStringLiteral("maxClippedRatio"), 0.02}
         }}
    };
    return colorComparisonExtractParamsHash(extractParams);
}

ColorComparisonModelV2 readyAdapterModel()
{
    ColorComparisonModelV2 model = readyModel();
    model.extractParamsHash = defaultAdapterExtractHash();
    return model;
}

QJsonObject inputMetadata(const QString &colorMode,
                          const QString &pixelFormat,
                          int originalChannels,
                          int originalDepth)
{
    return {
        {QStringLiteral("colorMode"), colorMode},
        {QStringLiteral("pixelFormat"), pixelFormat},
        {QStringLiteral("originalChannels"), originalChannels},
        {QStringLiteral("originalDepth"), originalDepth},
        {QStringLiteral("source"), QStringLiteral("test")}
    };
}

QJsonObject validAdapterParams(const ColorComparisonModelV2 &model)
{
    return {
        {QStringLiteral("version"), 2},
        {QStringLiteral("templateRegionMode"), QStringLiteral("custom")},
        {QStringLiteral("templateRoiNormalized"), rectJson(0.0, 0.0, 1.0, 1.0)},
        {QStringLiteral("templateMaskPolygon"), QJsonArray()},
        {QStringLiteral("detectRegionType"), QStringLiteral("rectangle")},
        {QStringLiteral("detectRoiNormalized"), rectJson(0.0, 0.0, 1.0, 1.0)},
        {QStringLiteral("detectCircleNormalized"), QJsonObject{
             {QStringLiteral("center"), pointJson(0.5, 0.5)},
             {QStringLiteral("radius"), 0.25},
             {QStringLiteral("boundingRect"), rectJson(0.25, 0.25, 0.5, 0.5)}
         }},
        {QStringLiteral("detectMaskPolygon"), QJsonArray()},
        {QStringLiteral("model"), colorComparisonModelToJson(model)},
        {QStringLiteral("comparison"), QJsonObject{
             {QStringLiteral("sensitivity"), QStringLiteral("medium")},
             {QStringLiteral("brightnessCompensation"), false}
         }},
        {QStringLiteral("positionCorrection"), QJsonObject{
             {QStringLiteral("enabled"), false},
             {QStringLiteral("sourceId"), QString()},
             {QStringLiteral("interfaceVersion"), 1}
         }}
    };
}

ToolRequest adapterRequest(const QJsonObject &colorComparison)
{
    ToolRequest request;
    request.config.toolId = QStringLiteral("color-comparison-v2");
    request.config.toolType = ToolType::ColorComparison;
    request.config.category = ToolCategory::Recognition;
    request.config.enabled = true;
    request.image = cv::Mat(16, 16, CV_8UC3, cv::Scalar(20, 120, 200)).clone();
    request.config.params.insert(QStringLiteral("colorComparison"), colorComparison);
    request.config.judgeRule.insert(QStringLiteral("mode"),
                                    QStringLiteral("min_score"));
    request.config.judgeRule.insert(QStringLiteral("minScore"), 80);
    request.runtimeContext.insert(QStringLiteral("input"),
                                  inputMetadata(QStringLiteral("color"),
                                                QStringLiteral("BGR8"), 3, 8));
    request.runtimeContext.insert(QStringLiteral("referenceInput"),
                                  inputMetadata(QStringLiteral("color"),
                                                QStringLiteral("BGR8"), 3, 8));
    return request;
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

void checkAdapterContract()
{
    ColorComparisonAdapter adapter;
    check(adapter.supports(ToolType::ColorComparison)
                  && !adapter.supports(ToolType::ColorRecognition),
          "Adapter must support only ColorComparison");

    ColorComparisonModelV2 model = readyAdapterModel();
    QJsonObject minimalMonoParams;
    minimalMonoParams.insert(QStringLiteral("version"), 2);
    minimalMonoParams.insert(QStringLiteral("model"),
                             colorComparisonModelToJson(model));
    minimalMonoParams.insert(
            QStringLiteral("comparison"),
            QJsonObject{{QStringLiteral("sensitivity"), QStringLiteral("medium")},
                        {QStringLiteral("brightnessCompensation"), false}});
    ToolRequest monoRequest = adapterRequest(minimalMonoParams);
    monoRequest.runtimeContext.insert(
            QStringLiteral("input"),
            inputMetadata(QStringLiteral("mono"), QStringLiteral("Mono8"), 1, 8));
    const ToolResult mono = adapter.run(monoRequest);
    check(mono.success && !mono.ok
                  && mono.status == QStringLiteral("unsupported_color_input"),
          "Adapter must preserve grayscale NG semantics");
    check(!mono.payload.value(QStringLiteral("measurementValid")).toBool(true)
                  && !mono.payload.value(QStringLiteral("passed")).toBool(true)
                  && mono.count == 0,
          "grayscale measurement must be invalid and count as no measurement");
    check(mono.score == 0.0 && mono.value == 0.0
                  && mono.text == QStringLiteral("0.00")
                  && mono.elapsedMs >= 0,
          "Adapter must map runner score, similarity, text, and elapsed time");

    QJsonObject unknownVersion{{QStringLiteral("version"), 99}};
    const ToolResult unsupportedVersion = adapter.run(adapterRequest(unknownVersion));
    check(!unsupportedVersion.success
                  && unsupportedVersion.status
                          == QStringLiteral("unsupported_model_version"),
          "unknown versions must not fall back to V1 or V2 defaults");

    QJsonObject unknownSpectrumVersion{
        {QStringLiteral("version"), 99},
        {QStringLiteral("model"), QJsonObject{
             {QStringLiteral("featureType"), QStringLiteral("spectrum")}
         }}
    };
    const ToolResult versionBeforeFeature =
            adapter.run(adapterRequest(unknownSpectrumVersion));
    check(!versionBeforeFeature.success
                  && versionBeforeFeature.status
                          == QStringLiteral("unsupported_model_version"),
          "unknown model version status must take precedence over feature parsing");

    ToolRequest emptyImage = adapterRequest(unknownVersion);
    emptyImage.image.release();
    const ToolResult emptyBeforeConfig = adapter.run(emptyImage);
    check(!emptyBeforeConfig.success
                  && emptyBeforeConfig.status == QStringLiteral("image_empty"),
          "empty input must take precedence over model parsing");

    QJsonObject legacy{
        {QStringLiteral("version"), 1},
        {QStringLiteral("templateFeature"), QJsonArray{1.0, 0.0}}
    };
    ToolRequest legacyWithoutReference = adapterRequest(legacy);
    const ToolResult rebuildRequired = adapter.run(legacyWithoutReference);
    check(!rebuildRequired.success
                  && rebuildRequired.status
                          == QStringLiteral("model_rebuild_required"),
          "V1 without a reference must require a model rebuild");

    ToolRequest legacyWithReference = adapterRequest(legacy);
    legacyWithReference.referenceImage =
            cv::Mat(16, 16, CV_8UC3, cv::Scalar(20, 120, 200)).clone();
    const ToolResult staleLegacy = adapter.run(legacyWithReference);
    check(!staleLegacy.success
                  && staleLegacy.status == QStringLiteral("model_stale"),
          "V1 with a reference must remain stale until explicit sampling");

    QJsonObject malformedV2{
        {QStringLiteral("version"), 2},
        {QStringLiteral("model"), QJsonObject{
             {QStringLiteral("state"), QStringLiteral("ready")}
         }}
    };
    const ToolResult badModel = adapter.run(adapterRequest(malformedV2));
    check(!badModel.success && badModel.status == QStringLiteral("model_invalid"),
          "malformed V2 models must be rejected without fallback");

    ColorComparisonModelV2 unsupportedStateModel = model;
    unsupportedStateModel.state = ColorComparisonModelState::Unsupported;
    const ToolResult unsupportedState = adapter.run(
                adapterRequest(validAdapterParams(unsupportedStateModel)));
    check(!unsupportedState.success
                  && unsupportedState.status
                          == QStringLiteral("model_rebuild_required"),
          "unsupported V2 model state must preserve the runner rebuild status");

    QJsonObject spectrumModel = colorComparisonModelToJson(model);
    spectrumModel.insert(QStringLiteral("featureType"),
                         QStringLiteral("spectrum"));
    QJsonObject spectrumParams = validAdapterParams(model);
    spectrumParams.insert(QStringLiteral("model"), spectrumModel);
    const ToolResult spectrum = adapter.run(adapterRequest(spectrumParams));
    check(!spectrum.success
                  && spectrum.status == QStringLiteral("unsupported_feature"),
          "reserved spectrum requests must preserve unsupported_feature");

    QJsonObject invalidTemplateMode = validAdapterParams(model);
    invalidTemplateMode.insert(QStringLiteral("templateRegionMode"),
                               QStringLiteral("automatic"));
    const ToolResult badTemplateMode =
            adapter.run(adapterRequest(invalidTemplateMode));
    check(!badTemplateMode.success
                  && badTemplateMode.status == QStringLiteral("invalid_template_roi"),
          "unknown template region enums must be rejected");

    QJsonObject invalidDetectRoi = validAdapterParams(model);
    invalidDetectRoi.insert(QStringLiteral("detectRoiNormalized"),
                            rectJson(-0.1, 0.0, 0.5, 0.5));
    const ToolResult badDetectRoi = adapter.run(adapterRequest(invalidDetectRoi));
    check(!badDetectRoi.success
                  && badDetectRoi.status == QStringLiteral("invalid_detect_roi"),
          "out-of-range detection geometry must not be clamped");

    QJsonObject invalidCircle = validAdapterParams(model);
    invalidCircle.insert(QStringLiteral("detectRegionType"),
                         QStringLiteral("circle"));
    invalidCircle.insert(QStringLiteral("detectCircleNormalized"),
                         QJsonObject{{QStringLiteral("center"), pointJson(0.5, 0.5)},
                                     {QStringLiteral("radius"), 0.0}});
    const ToolResult badCircle = adapter.run(adapterRequest(invalidCircle));
    check(!badCircle.success
                  && badCircle.status == QStringLiteral("invalid_detect_roi"),
          "zero-radius detection circles must be rejected");

    QJsonObject invalidMask = validAdapterParams(model);
    invalidMask.insert(
            QStringLiteral("detectMaskPolygon"),
            QJsonArray{pointJson(0.1, 0.1),
                       pointJson(0.2, 0.2),
                       pointJson(0.3, 0.3)});
    const ToolResult badMask = adapter.run(adapterRequest(invalidMask));
    check(!badMask.success
                  && badMask.status == QStringLiteral("invalid_detect_mask"),
          "degenerate detection masks must be rejected");

    QJsonObject outOfRangeMask = validAdapterParams(model);
    outOfRangeMask.insert(
            QStringLiteral("templateMaskPolygon"),
            QJsonArray{pointJson(-0.1, 0.1),
                       pointJson(0.3, 0.1),
                       pointJson(0.3, 0.3)});
    const ToolResult badTemplateMask =
            adapter.run(adapterRequest(outOfRangeMask));
    check(!badTemplateMask.success
                  && badTemplateMask.status
                          == QStringLiteral("invalid_template_mask"),
          "out-of-range template mask points must not be clamped");

    QJsonObject invalidSensitivity = validAdapterParams(model);
    invalidSensitivity.insert(
            QStringLiteral("comparison"),
            QJsonObject{{QStringLiteral("sensitivity"), QStringLiteral("extreme")},
                        {QStringLiteral("brightnessCompensation"), false}});
    const ToolResult badSensitivity =
            adapter.run(adapterRequest(invalidSensitivity));
    check(!badSensitivity.success
                  && badSensitivity.status == QStringLiteral("invalid_sensitivity"),
          "unknown sensitivity enums must be rejected without defaulting");

    ToolRequest invalidThreshold = adapterRequest(validAdapterParams(model));
    invalidThreshold.config.judgeRule.insert(QStringLiteral("minScore"), 101);
    const ToolResult badThreshold = adapter.run(invalidThreshold);
    check(!badThreshold.success,
          "out-of-range score thresholds must be rejected rather than clamped");

    QJsonObject badPosition = validAdapterParams(model);
    badPosition.insert(
            QStringLiteral("positionCorrection"),
            QJsonObject{{QStringLiteral("enabled"), true},
                        {QStringLiteral("sourceId"), QStringLiteral("pose-1")},
                        {QStringLiteral("interfaceVersion"), 2}});
    const ToolResult badPositionInterface = adapter.run(adapterRequest(badPosition));
    check(!badPositionInterface.success,
          "unknown position-correction interface versions must be rejected");

    QJsonObject positionParams = validAdapterParams(model);
    positionParams.insert(
            QStringLiteral("positionCorrection"),
            QJsonObject{{QStringLiteral("enabled"), true},
                        {QStringLiteral("sourceId"), QStringLiteral("pose-1")},
                        {QStringLiteral("interfaceVersion"), 1}});
    positionParams.insert(QStringLiteral("halconSoPath"),
                          QCoreApplication::applicationFilePath());
    const ToolResult position = adapter.run(adapterRequest(positionParams));
    check(warningsContain(position.payload,
                          QStringLiteral("position_correction_not_implemented"))
                  && position.payload.value(QStringLiteral("positionCorrection"))
                             .toObject().value(QStringLiteral("requested")).toBool()
                  && !position.payload.value(QStringLiteral("positionCorrection"))
                              .toObject().value(QStringLiteral("applied")).toBool(true),
          "reserved position correction must run uncorrected and emit its warning");

    ToolRequest missingReference = adapterRequest(validAdapterParams(model));
    const ColorComparisonTemplateBuildResult missing =
            adapter.buildTemplateModel(missingReference);
    check(!missing.success && missing.status == QStringLiteral("image_empty"),
          "explicit template build without a reference image must return image_empty");

    ToolRequest monoReference = adapterRequest(validAdapterParams(model));
    monoReference.referenceImage =
            cv::Mat(16, 16, CV_8UC3, cv::Scalar(20, 120, 200)).clone();
    monoReference.runtimeContext.insert(
            QStringLiteral("referenceInput"),
            inputMetadata(QStringLiteral("mono"), QStringLiteral("Mono8"), 1, 8));
    const ColorComparisonTemplateBuildResult monoBuild =
            adapter.buildTemplateModel(monoReference);
    check(!monoBuild.success
                  && monoBuild.status == QStringLiteral("unsupported_color_input"),
          "template build must map runtimeContext.referenceInput, not detection input");
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

    checkAdapterContract();

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
