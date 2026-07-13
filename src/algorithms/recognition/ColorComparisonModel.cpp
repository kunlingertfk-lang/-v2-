#include "ColorComparisonModel.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>

#include <cmath>
#include <limits>

namespace {

constexpr qint64 kMaxExactJsonInteger = 9007199254740991LL;

ColorComparisonModelValidation validationFailure(const QString &status,
                                                  const QString &message)
{
    return {false, status, message};
}

QJsonArray vectorToJson(const QVector<double> &values)
{
    QJsonArray array;
    for (double value : values)
        array.append(value);
    return array;
}

QVector<double> vectorFromJson(const QJsonArray &array)
{
    QVector<double> values;
    values.reserve(array.size());
    for (const QJsonValue &value : array) {
        values.append(value.isDouble()
                              ? value.toDouble()
                              : std::numeric_limits<double>::quiet_NaN());
    }
    return values;
}

QJsonObject inputSignatureToJson(const ColorComparisonInputSignature &signature)
{
    return {
        {QStringLiteral("colorMode"), signature.colorMode},
        {QStringLiteral("pixelFormat"), signature.pixelFormat},
        {QStringLiteral("bitDepth"), signature.bitDepth},
        {QStringLiteral("whiteBalance"), signature.whiteBalance},
        {QStringLiteral("ccm"), signature.ccm},
        {QStringLiteral("exposure"), signature.exposure},
        {QStringLiteral("gain"), signature.gain}
    };
}

QJsonObject brightnessReferenceToJson(
        const ColorComparisonBrightnessReference &brightnessReference)
{
    return {
        {QStringLiteral("mean"), brightnessReference.mean},
        {QStringLiteral("deviation"), brightnessReference.deviation}
    };
}

bool isIntegerJsonValue(const QJsonValue &value)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number;
}

bool isIntJsonValue(const QJsonValue &value)
{
    if (!isIntegerJsonValue(value))
        return false;

    const double number = value.toDouble();
    return number >= static_cast<double>(std::numeric_limits<int>::min())
            && number <= static_cast<double>(std::numeric_limits<int>::max());
}

bool isEffectivePixelCountJsonValue(const QJsonValue &value)
{
    if (!isIntegerJsonValue(value))
        return false;

    const double number = value.toDouble();
    return number >= static_cast<double>(std::numeric_limits<qint64>::min())
            && number <= static_cast<double>(kMaxExactJsonInteger);
}

bool hasStrictModelShape(const QJsonObject &object)
{
    const QJsonObject inputSignature =
            object.value(QStringLiteral("inputSignature")).toObject();
    const QJsonObject brightnessReference =
            object.value(QStringLiteral("brightnessReference")).toObject();

    return object.value(QStringLiteral("state")).isString()
            && object.value(QStringLiteral("featureType")).isString()
            && object.value(QStringLiteral("algorithm")).isString()
            && object.value(QStringLiteral("colorSpace")).isString()
            && isIntJsonValue(object.value(QStringLiteral("hueBins")))
            && isIntJsonValue(object.value(QStringLiteral("saturationBins")))
            && object.value(QStringLiteral("layout")).isString()
            && object.value(QStringLiteral("normalized")).isBool()
            && object.value(QStringLiteral("values")).isArray()
            && object.value(QStringLiteral("valueHistogram")).isArray()
            && isEffectivePixelCountJsonValue(
                    object.value(QStringLiteral("effectivePixelCount")))
            && object.value(QStringLiteral("referenceImageHash")).isString()
            && object.value(QStringLiteral("extractParamsHash")).isString()
            && object.value(QStringLiteral("inputSignature")).isObject()
            && inputSignature.value(QStringLiteral("colorMode")).isString()
            && inputSignature.value(QStringLiteral("pixelFormat")).isString()
            && isIntJsonValue(inputSignature.value(QStringLiteral("bitDepth")))
            && inputSignature.contains(QStringLiteral("whiteBalance"))
            && inputSignature.contains(QStringLiteral("ccm"))
            && inputSignature.contains(QStringLiteral("exposure"))
            && inputSignature.contains(QStringLiteral("gain"))
            && object.value(QStringLiteral("brightnessReference")).isObject()
            && brightnessReference.value(QStringLiteral("mean")).isDouble()
            && brightnessReference.value(QStringLiteral("deviation")).isDouble();
}

ColorComparisonModelState stateFromName(const QString &name, bool *known)
{
    if (known)
        *known = true;
    if (name == QStringLiteral("empty"))
        return ColorComparisonModelState::Empty;
    if (name == QStringLiteral("stale"))
        return ColorComparisonModelState::Stale;
    if (name == QStringLiteral("ready"))
        return ColorComparisonModelState::Ready;
    if (name == QStringLiteral("invalid"))
        return ColorComparisonModelState::Invalid;
    if (name == QStringLiteral("unsupported"))
        return ColorComparisonModelState::Unsupported;

    if (known)
        *known = false;
    return ColorComparisonModelState::Invalid;
}

ColorComparisonModelV2 modelFromJson(const QJsonObject &object, bool *stateKnown)
{
    ColorComparisonModelV2 model;
    model.state = stateFromName(object.value(QStringLiteral("state")).toString(),
                                stateKnown);
    model.featureType = object.value(QStringLiteral("featureType")).toString();
    model.algorithm = object.value(QStringLiteral("algorithm")).toString();
    model.colorSpace = object.value(QStringLiteral("colorSpace")).toString();
    model.hueBins = object.value(QStringLiteral("hueBins")).toInt();
    model.saturationBins = object.value(QStringLiteral("saturationBins")).toInt();
    model.layout = object.value(QStringLiteral("layout")).toString();
    model.normalized = object.value(QStringLiteral("normalized")).toBool();
    model.values = vectorFromJson(object.value(QStringLiteral("values")).toArray());
    model.valueHistogram =
            vectorFromJson(object.value(QStringLiteral("valueHistogram")).toArray());
    model.effectivePixelCount = static_cast<qint64>(
            object.value(QStringLiteral("effectivePixelCount")).toDouble());
    model.referenceImageHash =
            object.value(QStringLiteral("referenceImageHash")).toString();
    model.extractParamsHash =
            object.value(QStringLiteral("extractParamsHash")).toString();

    const QJsonObject inputSignature =
            object.value(QStringLiteral("inputSignature")).toObject();
    model.inputSignature.colorMode =
            inputSignature.value(QStringLiteral("colorMode")).toString();
    model.inputSignature.pixelFormat =
            inputSignature.value(QStringLiteral("pixelFormat")).toString();
    model.inputSignature.bitDepth =
            inputSignature.value(QStringLiteral("bitDepth")).toInt();
    model.inputSignature.whiteBalance =
            inputSignature.value(QStringLiteral("whiteBalance"));
    model.inputSignature.ccm = inputSignature.value(QStringLiteral("ccm"));
    model.inputSignature.exposure = inputSignature.value(QStringLiteral("exposure"));
    model.inputSignature.gain = inputSignature.value(QStringLiteral("gain"));

    const QJsonObject brightnessReference =
            object.value(QStringLiteral("brightnessReference")).toObject();
    model.brightnessReference.mean =
            brightnessReference.value(QStringLiteral("mean")).toDouble();
    model.brightnessReference.deviation =
            brightnessReference.value(QStringLiteral("deviation")).toDouble();
    return model;
}

ColorComparisonModelReadResult invalidReadResult(const QString &message)
{
    ColorComparisonModelReadResult result;
    result.model.state = ColorComparisonModelState::Invalid;
    result.status = QStringLiteral("model_invalid");
    result.message = message;
    return result;
}

} // namespace

QString colorComparisonModelStateName(ColorComparisonModelState state)
{
    switch (state) {
    case ColorComparisonModelState::Empty:
        return QStringLiteral("empty");
    case ColorComparisonModelState::Stale:
        return QStringLiteral("stale");
    case ColorComparisonModelState::Ready:
        return QStringLiteral("ready");
    case ColorComparisonModelState::Invalid:
        return QStringLiteral("invalid");
    case ColorComparisonModelState::Unsupported:
        return QStringLiteral("unsupported");
    }
    return QStringLiteral("invalid");
}

QJsonObject colorComparisonModelToJson(const ColorComparisonModelV2 &model)
{
    return {
        {QStringLiteral("state"), colorComparisonModelStateName(model.state)},
        {QStringLiteral("featureType"), model.featureType},
        {QStringLiteral("algorithm"), model.algorithm},
        {QStringLiteral("colorSpace"), model.colorSpace},
        {QStringLiteral("hueBins"), model.hueBins},
        {QStringLiteral("saturationBins"), model.saturationBins},
        {QStringLiteral("layout"), model.layout},
        {QStringLiteral("normalized"), model.normalized},
        {QStringLiteral("values"), vectorToJson(model.values)},
        {QStringLiteral("valueHistogram"), vectorToJson(model.valueHistogram)},
        {QStringLiteral("effectivePixelCount"),
         static_cast<double>(model.effectivePixelCount)},
        {QStringLiteral("referenceImageHash"), model.referenceImageHash},
        {QStringLiteral("extractParamsHash"), model.extractParamsHash},
        {QStringLiteral("inputSignature"), inputSignatureToJson(model.inputSignature)},
        {QStringLiteral("brightnessReference"),
         brightnessReferenceToJson(model.brightnessReference)}
    };
}

ColorComparisonModelReadResult readColorComparisonModel(
        const QJsonObject &colorComparison, bool referenceAvailable)
{
    const QJsonValue versionValue = colorComparison.value(QStringLiteral("version"));
    if (!isIntegerJsonValue(versionValue)) {
        ColorComparisonModelReadResult result;
        result.requiresRebuild = true;
        result.status = QStringLiteral("unsupported_model_version");
        result.message = QStringLiteral("不支持的颜色比较模型版本");
        result.model.state = ColorComparisonModelState::Unsupported;
        return result;
    }

    const int version = versionValue.toInt();
    if (version == 1) {
        ColorComparisonModelReadResult result;
        result.requiresRebuild = true;
        if (referenceAvailable) {
            result.status = QStringLiteral("model_stale");
            result.message = QStringLiteral("旧模型需要重新取样");
            result.model.state = ColorComparisonModelState::Stale;
        } else {
            result.status = QStringLiteral("model_rebuild_required");
            result.message = QStringLiteral("旧模型缺少参考图，必须重建");
            result.model.state = ColorComparisonModelState::Unsupported;
        }
        return result;
    }

    if (version != 2) {
        ColorComparisonModelReadResult result;
        result.requiresRebuild = true;
        result.status = QStringLiteral("unsupported_model_version");
        result.message = QStringLiteral("不支持的颜色比较模型版本");
        result.model.state = ColorComparisonModelState::Unsupported;
        return result;
    }

    const QJsonValue modelValue = colorComparison.value(QStringLiteral("model"));
    if (!modelValue.isObject() || !hasStrictModelShape(modelValue.toObject()))
        return invalidReadResult(QStringLiteral("颜色比较模型 JSON 结构错误"));

    bool stateKnown = false;
    ColorComparisonModelReadResult result;
    result.model = modelFromJson(modelValue.toObject(), &stateKnown);
    if (!stateKnown) {
        result.model.state = ColorComparisonModelState::Invalid;
        result.status = QStringLiteral("model_invalid");
        result.message = QStringLiteral("颜色比较模型状态无效");
        return result;
    }

    const ColorComparisonModelValidation validation =
            validateColorComparisonModel(result.model);
    result.success = validation.success;
    result.status = validation.status;
    result.message = validation.message;
    if (!validation.success && validation.status == QStringLiteral("model_invalid"))
        result.model.state = ColorComparisonModelState::Invalid;
    return result;
}

ColorComparisonModelValidation validateColorComparisonModel(
        const ColorComparisonModelV2 &model,
        const QString &expectedExtractParamsHash)
{
    if (model.state != ColorComparisonModelState::Ready) {
        return {false,
                QStringLiteral("model_%1").arg(colorComparisonModelStateName(model.state)),
                QStringLiteral("颜色比较模型未处于 ready 状态")};
    }
    if (model.featureType != QStringLiteral("histogram_hs_2d")
            || model.algorithm != QStringLiteral("histogram_intersection")
            || model.colorSpace != QStringLiteral("hsv")
            || model.hueBins != 32 || model.saturationBins != 32
            || model.layout != QStringLiteral("hue_major") || !model.normalized
            || model.inputSignature.bitDepth != 8) {
        return validationFailure(QStringLiteral("model_invalid"),
                                 QStringLiteral("颜色比较模型合同不匹配"));
    }
    if (model.values.size() != 1024 || model.valueHistogram.size() != 32) {
        return validationFailure(QStringLiteral("model_invalid"),
                                 QStringLiteral("颜色比较模型维度错误"));
    }

    double sum = 0.0;
    for (double value : model.values) {
        if (!std::isfinite(value) || value < 0.0) {
            return validationFailure(QStringLiteral("model_invalid"),
                                     QStringLiteral("颜色比较模型包含非法值"));
        }
        sum += value;
    }
    if (std::abs(sum - 1.0) > 1e-6) {
        return validationFailure(QStringLiteral("model_invalid"),
                                 QStringLiteral("颜色比较模型未归一化"));
    }

    double valueSum = 0.0;
    for (double value : model.valueHistogram) {
        if (!std::isfinite(value) || value < 0.0) {
            return validationFailure(QStringLiteral("model_invalid"),
                                     QStringLiteral("颜色比较模型包含非法值"));
        }
        valueSum += value;
    }
    if (std::abs(valueSum - 1.0) > 1e-6) {
        return validationFailure(QStringLiteral("model_invalid"),
                                 QStringLiteral("颜色比较模型未归一化"));
    }
    if (model.referenceImageHash.isEmpty() || model.extractParamsHash.isEmpty()
            || model.effectivePixelCount
                < kColorComparisonMinimumEffectivePixels
            || model.effectivePixelCount > kMaxExactJsonInteger
            || !std::isfinite(model.brightnessReference.mean)
            || !std::isfinite(model.brightnessReference.deviation)
            || model.brightnessReference.mean < 0.0
            || model.brightnessReference.mean
                > kColorComparisonMaximumByteValue
            || model.brightnessReference.deviation < 0.0
            || model.brightnessReference.deviation
                > kColorComparisonMaximumByteValue) {
        return validationFailure(QStringLiteral("model_invalid"),
                                 QStringLiteral("颜色比较模型元数据无效"));
    }
    if (!expectedExtractParamsHash.isEmpty()
            && model.extractParamsHash != expectedExtractParamsHash) {
        return validationFailure(QStringLiteral("model_stale"),
                                 QStringLiteral("颜色比较模型提取参数已变化"));
    }
    return {true, QStringLiteral("ok"), QString()};
}

QString colorComparisonReferenceHash(const cv::Mat &image)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData("rows=", 5);
    hash.addData(QByteArray::number(image.rows));
    hash.addData(";cols=", 6);
    hash.addData(QByteArray::number(image.cols));
    hash.addData(";type=", 6);
    hash.addData(QByteArray::number(image.type()));
    hash.addData(";", 1);

    const int bytesPerRow = static_cast<int>(image.cols * image.elemSize());
    for (int row = 0; row < image.rows; ++row) {
        hash.addData(reinterpret_cast<const char *>(image.ptr(row)), bytesPerRow);
    }
    return QString::fromLatin1(hash.result().toHex());
}

QString colorComparisonExtractParamsHash(const QJsonObject &extractParams)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QJsonDocument(extractParams).toJson(QJsonDocument::Compact));
    return QString::fromLatin1(hash.result().toHex());
}
