#include "algorithms/recognition/ColorComparisonHalconRunner.h"
#include "algorithms/halcon/HalconRuntimePaths.h"

#include <HalconC.h>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QPolygonF>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <exception>
#include <limits>

namespace {

constexpr int kHistogramBins = 32;
constexpr int kHistogramLength = kHistogramBins * kHistogramBins;
constexpr int kPaddedHistogramWidth = 64;
constexpr int kTiledHistogramHeight = 96;
constexpr int kSaturationOffset = 16;
constexpr double kMinBrightnessMean = 8.0;
constexpr double kMaxBrightnessMean = 247.0;
constexpr double kMinBrightnessScale = 0.75;
constexpr double kMaxBrightnessScale = 1.3333333333;
constexpr double kMaxClippedRatio = 0.02;
constexpr double kGeometryEpsilon = 1e-12;

struct RunnerFailure
{
    QString status;
    QString message;
    QJsonObject payloadPatch = QJsonObject();
};

struct SmoothingProfile
{
    QString sensitivity;
    double hueSigma = 0.0;
    double saturationSigma = 0.0;
    bool valid = false;
};

SmoothingProfile smoothingProfile(const QString &sensitivity)
{
    const QString normalized = sensitivity.trimmed().toLower();
    if (normalized == QStringLiteral("high"))
        return {normalized, 1.5, 4.0, true};
    if (normalized == QStringLiteral("medium"))
        return {normalized, 3.0, 8.0, true};
    if (normalized == QStringLiteral("low"))
        return {normalized, 4.0, 10.0, true};
    return {};
}

bool halconStatusOk(Herror status)
{
    return status == H_MSG_OK || status == H_MSG_TRUE || status == H_MSG_FALSE;
}

bool halconObjectAllocated(Hobject object)
{
    return object != 0 && object != NO_OBJECTS && object != EMPTY_REGION;
}

bool isLicenseError(Herror status)
{
    const Hlong code = static_cast<Hlong>(status);
    return code >= H_ERR_LIC_NO_LICENSE && code <= H_ERR_LIC_NEWVER;
}

bool finiteValue(double value)
{
    return std::isfinite(value);
}

struct CirclePixelGeometry
{
    QPointF center;
    double radius = 0.0;
};

CirclePixelGeometry circlePixelGeometry(const QPointF &normalizedCenter,
                                        double normalizedRadius,
                                        int imageWidth,
                                        int imageHeight)
{
    return {
        QPointF(normalizedCenter.x() * static_cast<double>(imageWidth),
                normalizedCenter.y() * static_cast<double>(imageHeight)),
        normalizedRadius
                * static_cast<double>(std::max(imageWidth, imageHeight))
    };
}

bool validNormalizedRect(const QRectF &rect)
{
    return finiteValue(rect.x()) && finiteValue(rect.y())
            && finiteValue(rect.width()) && finiteValue(rect.height())
            && rect.x() >= 0.0 && rect.y() >= 0.0
            && rect.width() > 0.0 && rect.height() > 0.0
            && rect.x() + rect.width() <= 1.0
            && rect.y() + rect.height() <= 1.0;
}

bool validNormalizedCircle(const QPointF &center,
                           double radius,
                           int imageWidth,
                           int imageHeight)
{
    if (imageWidth <= 0 || imageHeight <= 0
            || !finiteValue(center.x()) || !finiteValue(center.y())
            || !finiteValue(radius) || radius <= 0.0) {
        return false;
    }

    const CirclePixelGeometry geometry = circlePixelGeometry(
                center, radius, imageWidth, imageHeight);
    return finiteValue(geometry.center.x())
            && finiteValue(geometry.center.y())
            && finiteValue(geometry.radius)
            && geometry.center.x() - geometry.radius >= -kGeometryEpsilon
            && geometry.center.x() + geometry.radius
                <= static_cast<double>(imageWidth) + kGeometryEpsilon
            && geometry.center.y() - geometry.radius >= -kGeometryEpsilon
            && geometry.center.y() + geometry.radius
                <= static_cast<double>(imageHeight) + kGeometryEpsilon;
}

bool validNormalizedPolygon(const QVector<QPointF> &points)
{
    if (points.isEmpty())
        return true;
    if (points.size() < 3)
        return false;

    double twiceArea = 0.0;
    for (int index = 0; index < points.size(); ++index) {
        const QPointF &point = points.at(index);
        const QPointF &next = points.at((index + 1) % points.size());
        if (!finiteValue(point.x()) || !finiteValue(point.y())
                || point.x() < 0.0 || point.x() > 1.0
                || point.y() < 0.0 || point.y() > 1.0) {
            return false;
        }
        twiceArea += point.x() * next.y() - next.x() * point.y();
    }
    return std::abs(twiceArea) > kGeometryEpsilon;
}

QString normalizedTemplateRegionMode(const QString &mode)
{
    return mode.trimmed().toLower();
}

QString normalizedDetectRegionType(const QString &type)
{
    return type.trimmed().toLower();
}

QString normalizedColorMode(const QString &mode)
{
    return mode.trimmed().toLower();
}

bool supportedImageType(const cv::Mat &image)
{
    return image.type() == CV_8UC3 || image.type() == CV_8UC4;
}

bool supportedOriginalBitDepth(const ColorComparisonInputSignature &signature)
{
    return signature.bitDepth == -1 || signature.bitDepth == 8;
}

bool supportedOriginalPixelFormat(const ColorComparisonInputSignature &signature)
{
    const QString pixelFormat = signature.pixelFormat.trimmed();
    if (pixelFormat.isEmpty())
        return true;

    static const QStringList supportedFormats = {
        QStringLiteral("BGR8"),
        QStringLiteral("BGRA8"),
        QStringLiteral("UYVY8"),
        QStringLiteral("NV12"),
        QStringLiteral("RGB8"),
        QStringLiteral("RGBX8"),
        QStringLiteral("ARGB8"),
        QStringLiteral("ARGB8_Premultiplied"),
        QStringLiteral("RGBA8"),
        QStringLiteral("RGBA8_Premultiplied")
    };
    return supportedFormats.contains(pixelFormat);
}

QJsonObject pointToJson(const QPointF &point)
{
    return {
        {QStringLiteral("x"), point.x()},
        {QStringLiteral("y"), point.y()}
    };
}

QJsonObject rectToJson(const QRectF &rect)
{
    return {
        {QStringLiteral("x"), rect.x()},
        {QStringLiteral("y"), rect.y()},
        {QStringLiteral("width"), rect.width()},
        {QStringLiteral("height"), rect.height()}
    };
}

QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points)
        array.append(pointToJson(point));
    return array;
}

QJsonArray featureToJson(const QVector<double> &feature)
{
    QJsonArray array;
    for (double value : feature)
        array.append(value);
    return array;
}

QJsonArray stringsToJson(const QStringList &strings)
{
    QJsonArray array;
    for (const QString &string : strings)
        array.append(string);
    return array;
}

QJsonArray doublesToJson(const QVector<double> &values)
{
    QJsonArray array;
    for (double value : values)
        array.append(value);
    return array;
}

QJsonObject brightnessConstantsJson()
{
    return {
        {QStringLiteral("minMean"), kMinBrightnessMean},
        {QStringLiteral("maxMean"), kMaxBrightnessMean},
        {QStringLiteral("minScale"), kMinBrightnessScale},
        {QStringLiteral("maxScale"), kMaxBrightnessScale},
        {QStringLiteral("maxClippedRatio"), kMaxClippedRatio}
    };
}

QJsonObject templateExtractParams(const ColorComparisonHalconConfig &config)
{
    QJsonObject geometry;
    const QString mode = normalizedTemplateRegionMode(config.templateRegionMode);
    if (mode == QStringLiteral("custom")) {
        geometry.insert(QStringLiteral("type"), QStringLiteral("rectangle"));
        geometry.insert(QStringLiteral("rect"), rectToJson(config.templateRoiNormalized));
    } else if (normalizedDetectRegionType(config.detectRegionType)
               == QStringLiteral("circle")) {
        geometry.insert(QStringLiteral("type"), QStringLiteral("circle"));
        geometry.insert(QStringLiteral("center"),
                        pointToJson(config.detectCircleCenterNormalized));
        geometry.insert(QStringLiteral("radius"), config.detectCircleRadiusNormalized);
    } else {
        geometry.insert(QStringLiteral("type"), QStringLiteral("rectangle"));
        geometry.insert(QStringLiteral("rect"), rectToJson(config.detectRoiNormalized));
    }

    QJsonObject params = {
        {QStringLiteral("featureType"), QStringLiteral("histogram_hs_2d")},
        {QStringLiteral("algorithm"), QStringLiteral("histogram_intersection")},
        {QStringLiteral("colorSpace"), QStringLiteral("hsv")},
        {QStringLiteral("hueBins"), kHistogramBins},
        {QStringLiteral("saturationBins"), kHistogramBins},
        {QStringLiteral("layout"), QStringLiteral("hue_major")},
        {QStringLiteral("histogramCoordinateContract"),
         QStringLiteral("image_col_h_image_row_s_row_s_column_h")},
        {QStringLiteral("minimumEffectivePixels"),
         static_cast<double>(kColorComparisonMinimumEffectivePixels)},
        {QStringLiteral("templateRegionMode"), mode},
        {QStringLiteral("templateGeometry"), geometry},
        {QStringLiteral("templateMaskPolygon"),
         pointsToJson(config.templateMaskPolygonNormalized)},
        {QStringLiteral("maskOwnershipContract"),
         QStringLiteral("independent_template_and_detection_v1")},
        {QStringLiteral("brightnessCompensation"), config.brightnessCompensation},
        {QStringLiteral("brightnessConstants"), brightnessConstantsJson()}
    };
    return params;
}

QJsonObject detectionRoiJson(const ColorComparisonHalconConfig &config,
                             int imageWidth,
                             int imageHeight)
{
    QJsonObject json;
    const QString type = normalizedDetectRegionType(config.detectRegionType);
    json.insert(QStringLiteral("type"), type);
    json.insert(QStringLiteral("angle"), 0.0);
    if (type == QStringLiteral("circle")) {
        const CirclePixelGeometry geometry = circlePixelGeometry(
                    config.detectCircleCenterNormalized,
                    config.detectCircleRadiusNormalized,
                    imageWidth,
                    imageHeight);
        const double normalizedWidth = imageWidth > 0
                ? geometry.radius * 2.0 / static_cast<double>(imageWidth)
                : config.detectCircleRadiusNormalized * 2.0;
        const double normalizedHeight = imageHeight > 0
                ? geometry.radius * 2.0 / static_cast<double>(imageHeight)
                : config.detectCircleRadiusNormalized * 2.0;
        json.insert(QStringLiteral("centerX"), config.detectCircleCenterNormalized.x());
        json.insert(QStringLiteral("centerY"), config.detectCircleCenterNormalized.y());
        json.insert(QStringLiteral("width"), normalizedWidth);
        json.insert(QStringLiteral("height"), normalizedHeight);
        json.insert(QStringLiteral("radius"), config.detectCircleRadiusNormalized);
        json.insert(QStringLiteral("radiusNormalized"),
                    config.detectCircleRadiusNormalized);
        json.insert(QStringLiteral("radiusPixels"), geometry.radius);
    } else {
        json.insert(QStringLiteral("centerX"),
                    config.detectRoiNormalized.x()
                    + config.detectRoiNormalized.width() * 0.5);
        json.insert(QStringLiteral("centerY"),
                    config.detectRoiNormalized.y()
                    + config.detectRoiNormalized.height() * 0.5);
        json.insert(QStringLiteral("width"), config.detectRoiNormalized.width());
        json.insert(QStringLiteral("height"), config.detectRoiNormalized.height());
    }
    return json;
}

QJsonObject positionCorrectionJson(const ColorComparisonHalconConfig &config)
{
    QJsonObject json = {
        {QStringLiteral("requested"), config.positionCorrectionRequested},
        {QStringLiteral("applied"), config.positionCorrectionApplied},
        {QStringLiteral("showMatchContour"),
         config.showPositionCorrectionMatchContour},
        {QStringLiteral("matchContourAvailable"),
         !config.positionCorrectionMatchContours.isEmpty()},
        {QStringLiteral("sourceId"), config.positionCorrectionSourceId}
    };
    if (config.positionCorrectionApplied)
        json.insert(QStringLiteral("referenceToRunHomMat2D"),
                    doublesToJson(config.referenceToRunHomMat2D));
    return json;
}

QJsonObject emptyBrightnessDiagnostics(const ColorComparisonHalconConfig &config)
{
    return {
        {QStringLiteral("enabled"), config.brightnessCompensation},
        {QStringLiteral("requested"), config.brightnessCompensation},
        {QStringLiteral("applied"), false},
        {QStringLiteral("fallback"), false},
        {QStringLiteral("fallbackReason"), QString()},
        {QStringLiteral("templateMean"), config.model.brightnessReference.mean},
        {QStringLiteral("detectMeanBefore"), 0.0},
        {QStringLiteral("detectMeanAfter"), 0.0},
        {QStringLiteral("scale"), 1.0},
        {QStringLiteral("clippedRatio"), 0.0}
    };
}

QJsonObject baseRunPayload(const ColorComparisonHalconConfig &config,
                           const QStringList &warnings,
                           int imageWidth,
                           int imageHeight)
{
    return {
        {QStringLiteral("measurementValid"), false},
        {QStringLiteral("passed"), false},
        {QStringLiteral("algorithm"), QStringLiteral("histogram_intersection")},
        {QStringLiteral("featureType"), config.model.featureType},
        {QStringLiteral("modelVersion"), 2},
        {QStringLiteral("score"), 0.0},
        {QStringLiteral("similarity"), 0.0},
        {QStringLiteral("threshold"), static_cast<double>(config.minScore)},
        {QStringLiteral("effectiveTemplatePixels"),
         static_cast<double>(config.model.effectivePixelCount)},
        {QStringLiteral("effectiveDetectionPixels"), 0.0},
        {QStringLiteral("brightnessCompensation"),
         emptyBrightnessDiagnostics(config)},
        {QStringLiteral("positionCorrection"), positionCorrectionJson(config)},
        {QStringLiteral("detectionRoi"),
         detectionRoiJson(config, imageWidth, imageHeight)},
        {QStringLiteral("histogramDiagnostics"), QJsonObject{
             {QStringLiteral("available"), false},
             {QStringLiteral("hueBins"), kHistogramBins},
             {QStringLiteral("saturationBins"), kHistogramBins},
             {QStringLiteral("layout"), QStringLiteral("hue_major")}
         }},
        {QStringLiteral("warnings"), stringsToJson(warnings)}
    };
}

ColorComparisonHalconResult runFailure(const QString &status,
                                       const QString &message,
                                       const ColorComparisonHalconConfig &config,
                                       const QStringList &warnings,
                                       qint64 elapsedMs,
                                       int imageWidth,
                                       int imageHeight)
{
    ColorComparisonHalconResult result;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.payload = baseRunPayload(config, warnings, imageWidth, imageHeight);
    result.payload.insert(QStringLiteral("status"), status);
    result.payload.insert(QStringLiteral("message"), message);
    result.payload.insert(QStringLiteral("elapsedMs"),
                          static_cast<double>(elapsedMs));
    return result;
}

ColorComparisonHalconResult unsupportedColorResult(
        const ColorComparisonHalconConfig &config,
        qint64 elapsedMs,
        int imageWidth,
        int imageHeight)
{
    ColorComparisonHalconResult result = runFailure(
                QStringLiteral("unsupported_color_input"),
                QStringLiteral("Original input is monochrome; color comparison is invalid."),
                config,
                QStringList(),
                elapsedMs,
                imageWidth,
                imageHeight);
    result.success = true;
    result.ok = false;
    result.measurementValid = false;
    return result;
}

ColorComparisonTemplateBuildResult buildFailure(const QString &status,
                                                 const QString &message,
                                                 qint64 elapsedMs)
{
    ColorComparisonTemplateBuildResult result;
    result.status = status;
    result.message = message;
    result.payload.insert(QStringLiteral("status"), status);
    result.payload.insert(QStringLiteral("message"), message);
    result.payload.insert(QStringLiteral("algorithm"),
                          QStringLiteral("histogram_intersection"));
    result.payload.insert(QStringLiteral("featureType"),
                          QStringLiteral("histogram_hs_2d"));
    result.payload.insert(QStringLiteral("modelVersion"), 2);
    result.payload.insert(QStringLiteral("elapsedMs"),
                          static_cast<double>(elapsedMs));
    return result;
}

void mergePayloadPatch(QJsonObject *payload, const QJsonObject &patch)
{
    for (auto iterator = patch.constBegin(); iterator != patch.constEnd(); ++iterator)
        payload->insert(iterator.key(), iterator.value());
}

RunnerFailure validateGeometry(const ColorComparisonHalconConfig &config,
                               int imageWidth,
                               int imageHeight)
{
    if (config.positionCorrectionApplied) {
        if (config.referenceToRunHomMat2D.size() != 6) {
            return {QStringLiteral("invalid_position_correction_matrix"),
                    QStringLiteral("referenceToRunHomMat2D must contain six values.")};
        }
        for (double value : config.referenceToRunHomMat2D) {
            if (!finiteValue(value)) {
                return {QStringLiteral("invalid_position_correction_matrix"),
                        QStringLiteral("referenceToRunHomMat2D contains a non-finite value.")};
            }
        }
    }

    const QString templateMode = normalizedTemplateRegionMode(config.templateRegionMode);
    if (templateMode != QStringLiteral("custom")
            && templateMode != QStringLiteral("sync")) {
        return {QStringLiteral("invalid_template_roi"),
                QStringLiteral("Template region mode must be custom or sync.")};
    }
    if (templateMode == QStringLiteral("custom")
            && !validNormalizedRect(config.templateRoiNormalized)) {
        return {QStringLiteral("invalid_template_roi"),
                QStringLiteral("Custom template ROI must be finite and inside [0,1].")};
    }
    if (!validNormalizedPolygon(config.templateMaskPolygonNormalized)) {
        return {QStringLiteral("invalid_template_mask"),
                QStringLiteral("Template mask polygon is invalid or degenerate.")};
    }

    const QString detectType = normalizedDetectRegionType(config.detectRegionType);
    if (detectType == QStringLiteral("rectangle")) {
        if (!validNormalizedRect(config.detectRoiNormalized)) {
            return {QStringLiteral("invalid_detect_roi"),
                    QStringLiteral("Detection rectangle must be finite and inside [0,1].")};
        }
    } else if (detectType == QStringLiteral("circle")) {
        if (!validNormalizedCircle(config.detectCircleCenterNormalized,
                                   config.detectCircleRadiusNormalized,
                                   imageWidth,
                                   imageHeight)) {
            return {QStringLiteral("invalid_detect_roi"),
                    QStringLiteral("Detection circle must be finite, positive, and inside the image bounds.")};
        }
    } else {
        return {QStringLiteral("invalid_detect_roi"),
                QStringLiteral("Detection region type must be rectangle or circle.")};
    }
    if (!validNormalizedPolygon(config.detectMaskPolygonNormalized)) {
        return {QStringLiteral("invalid_detect_mask"),
                QStringLiteral("Detection mask polygon is invalid or degenerate.")};
    }
    return {};
}

QString modelFailureStatus(const ColorComparisonModelValidation &validation)
{
    if (validation.status == QStringLiteral("model_unsupported"))
        return QStringLiteral("model_rebuild_required");
    return validation.status;
}

struct HalconCApi
{
    using SetUtf8Fn = void (*)(int);
    using GetErrorTextFn = Herror (*)(Hlong, char *);
    using CreateTupleFn = void (*)(Htuple *, Hlong);
    using SetDoubleFn = void (*)(Htuple *, double, Hlong);
    using SetIntFn = void (*)(Htuple *, Hlong, Hlong);
    using SetStringFn = void (*)(Htuple *, const char *, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong,
                                             Hlong, Hlong, const char *, Hlong, Hlong,
                                             Hlong, Hlong, Hlong, Hlong);
    using Decompose3Fn = Herror (*)(const Hobject, Hobject *, Hobject *, Hobject *);
    using Compose3Fn = Herror (*)(const Hobject, const Hobject, const Hobject, Hobject *);
    using TransFromRgbFn = Herror (*)(const Hobject, const Hobject, const Hobject,
                                      Hobject *, Hobject *, Hobject *, const char *);
    using GenRectangle1Fn = Herror (*)(Hobject *, double, double, double, double);
    using GenCircleFn = Herror (*)(Hobject *, double, double, double);
    using GenRegionPolygonFilledFn = Herror (*)(Hobject *, const Htuple, const Htuple);
    using AffineTransRegionFn = Herror (*)(const Hobject, Hobject *, const Htuple,
                                           const Htuple);
    using ClipRegionFn = Herror (*)(const Hobject, Hobject *, const Htuple,
                                    const Htuple, const Htuple, const Htuple);
    using DifferenceFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using AreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using Histo2DimFn = Herror (*)(const Hobject, const Hobject, const Hobject, Hobject *);
    using GetGrayvalFn = Herror (*)(const Hobject, const Htuple, const Htuple, Htuple *);
    using IntensityFn = Herror (*)(const Hobject, const Hobject, Htuple *, Htuple *);
    using ScaleImageFn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple);
    using TupleSelectFn = Herror (*)(const Htuple, const Htuple, Htuple *);
    using TupleMin2Fn = Herror (*)(const Htuple, const Htuple, Htuple *);
    using TupleSumFn = Herror (*)(const Htuple, Htuple *);
    using TupleMaxFn = Herror (*)(const Htuple, Htuple *);
    using TupleDivFn = Herror (*)(const Htuple, const Htuple, Htuple *);
    using GenImage1Fn = Herror (*)(Hobject *, const Htuple, const Htuple,
                                   const Htuple, const Htuple);
    using GenGaussFilterFn = Herror (*)(Hobject *, const Htuple, const Htuple,
                                        const Htuple, const Htuple, const Htuple,
                                        const Htuple, const Htuple);
    using RftGenericFn = Herror (*)(const Hobject, Hobject *, const Htuple,
                                    const Htuple, const Htuple, const Htuple);
    using ConvolFftFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetIntFn setInt = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Decompose3Fn decompose3 = nullptr;
    Compose3Fn compose3 = nullptr;
    TransFromRgbFn transFromRgb = nullptr;
    GenRectangle1Fn genRectangle1 = nullptr;
    GenCircleFn genCircle = nullptr;
    GenRegionPolygonFilledFn genRegionPolygonFilled = nullptr;
    AffineTransRegionFn affineTransRegion = nullptr;
    ClipRegionFn clipRegion = nullptr;
    DifferenceFn difference = nullptr;
    AreaCenterFn areaCenter = nullptr;
    Histo2DimFn histo2Dim = nullptr;
    GetGrayvalFn getGrayval = nullptr;
    IntensityFn intensity = nullptr;
    ScaleImageFn scaleImage = nullptr;
    TupleSelectFn tupleSelect = nullptr;
    TupleMin2Fn tupleMin2 = nullptr;
    TupleSumFn tupleSum = nullptr;
    TupleMaxFn tupleMax = nullptr;
    TupleDivFn tupleDiv = nullptr;
    GenImage1Fn genImage1 = nullptr;
    GenGaussFilterFn genGaussFilter = nullptr;
    RftGenericFn rftGeneric = nullptr;
    ConvolFftFn convolFft = nullptr;
    ClearObjFn clearObj = nullptr;
};

template <typename Function>
bool resolveRequired(void *handle,
                     Function &target,
                     const char *symbolName,
                     QString *errorMessage)
{
    dlerror();
    void *symbol = dlsym(handle, symbolName);
    const char *symbolError = dlerror();
    if (symbolError != nullptr || symbol == nullptr) {
        *errorMessage = QStringLiteral("Missing HALCON symbol %1: %2")
                .arg(QString::fromLatin1(symbolName),
                     symbolError ? QString::fromLocal8Bit(symbolError)
                                 : QStringLiteral("not found"));
        return false;
    }
    target = reinterpret_cast<Function>(symbol);
    return true;
}

template <typename Function>
void resolveOptional(void *handle, Function &target, const char *symbolName)
{
    dlerror();
    void *symbol = dlsym(handle, symbolName);
    const char *symbolError = dlerror();
    if (symbolError == nullptr && symbol != nullptr)
        target = reinterpret_cast<Function>(symbol);
}

class HalconLibrary
{
public:
    ~HalconLibrary()
    {
        if (m_handle)
            dlclose(m_handle);
    }

    bool load(const QString &path, QString *message, bool *symbolMissing)
    {
        *symbolMissing = false;
        const QByteArray encodedPath = QFileInfo(path).absoluteFilePath().toLocal8Bit();
        m_handle = dlopen(encodedPath.constData(), RTLD_NOW | RTLD_LOCAL);
        if (!m_handle) {
            const char *loadError = dlerror();
            *message = loadError ? QString::fromLocal8Bit(loadError)
                                 : QStringLiteral("dlopen returned a null handle");
            return false;
        }

        resolveOptional(m_handle, api.setUtf8, "SetHcInterfaceStringEncodingIsUtf8");
        if (!resolveRequired(m_handle, api.getErrorText, "get_error_text", message)
                || !resolveRequired(m_handle, api.createTuple, "F_create_tuple", message)
                || !resolveRequired(m_handle, api.setDouble, "F_set_d", message)
                || !resolveRequired(m_handle, api.setInt, "F_set_i", message)
                || !resolveRequired(m_handle, api.setString, "F_set_s", message)
                || !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", message)
                || !resolveRequired(m_handle, api.getDouble, "F_get_d", message)
                || !resolveRequired(m_handle, api.genImageInterleaved,
                                    "gen_image_interleaved", message)
                || !resolveRequired(m_handle, api.decompose3, "decompose3", message)
                || !resolveRequired(m_handle, api.compose3, "compose3", message)
                || !resolveRequired(m_handle, api.transFromRgb, "trans_from_rgb", message)
                || !resolveRequired(m_handle, api.genRectangle1, "gen_rectangle1", message)
                || !resolveRequired(m_handle, api.genCircle, "gen_circle", message)
                || !resolveRequired(m_handle, api.genRegionPolygonFilled,
                                    "T_gen_region_polygon_filled", message)
                || !resolveRequired(m_handle, api.affineTransRegion,
                                    "T_affine_trans_region", message)
                || !resolveRequired(m_handle, api.clipRegion,
                                    "T_clip_region", message)
                || !resolveRequired(m_handle, api.difference, "difference", message)
                || !resolveRequired(m_handle, api.areaCenter, "T_area_center", message)
                || !resolveRequired(m_handle, api.histo2Dim, "T_histo_2dim", message)
                || !resolveRequired(m_handle, api.getGrayval, "T_get_grayval", message)
                || !resolveRequired(m_handle, api.intensity, "T_intensity", message)
                || !resolveRequired(m_handle, api.scaleImage, "T_scale_image", message)
                || !resolveRequired(m_handle, api.tupleSelect, "T_tuple_select", message)
                || !resolveRequired(m_handle, api.tupleMin2, "T_tuple_min2", message)
                || !resolveRequired(m_handle, api.tupleSum, "T_tuple_sum", message)
                || !resolveRequired(m_handle, api.tupleMax, "T_tuple_max", message)
                || !resolveRequired(m_handle, api.tupleDiv, "T_tuple_div", message)
                || !resolveRequired(m_handle, api.genImage1, "T_gen_image1", message)
                || !resolveRequired(m_handle, api.genGaussFilter,
                                    "T_gen_gauss_filter", message)
                || !resolveRequired(m_handle, api.rftGeneric,
                                    "T_rft_generic", message)
                || !resolveRequired(m_handle, api.convolFft,
                                    "T_convol_fft", message)
                || !resolveRequired(m_handle, api.clearObj, "clear_obj", message)) {
            *symbolMissing = true;
            dlclose(m_handle);
            m_handle = nullptr;
            return false;
        }
        if (api.setUtf8)
            api.setUtf8(1);
        return true;
    }

    HalconCApi api;

private:
    void *m_handle = nullptr;
};

class HalconTuple
{
public:
    explicit HalconTuple(HalconCApi *api)
        : m_api(api)
    {
    }

    HalconTuple(const HalconTuple &) = delete;
    HalconTuple &operator=(const HalconTuple &) = delete;

    HalconTuple(HalconTuple &&other) noexcept
        : m_api(other.m_api)
        , m_tuple(other.m_tuple)
    {
        other.m_tuple = HTUPLE_INITIALIZER;
    }

    HalconTuple &operator=(HalconTuple &&other) noexcept
    {
        if (this == &other)
            return *this;
        destroy();
        m_api = other.m_api;
        m_tuple = other.m_tuple;
        other.m_tuple = HTUPLE_INITIALIZER;
        return *this;
    }

    ~HalconTuple()
    {
        destroy();
    }

    Htuple *ptr()
    {
        return &m_tuple;
    }

    const Htuple &value() const
    {
        return m_tuple;
    }

    int size() const
    {
        return static_cast<int>(m_tuple.num);
    }

    void create(int size)
    {
        destroy();
        m_api->createTuple(&m_tuple, size);
    }

    void setDouble(int index, double value)
    {
        m_api->setDouble(&m_tuple, value, index);
    }

    void setInt(int index, Hlong value)
    {
        m_api->setInt(&m_tuple, value, index);
    }

    void setString(int index, const char *value)
    {
        m_api->setString(&m_tuple, value, index);
    }

    double doubleAt(int index) const
    {
        return m_api->getDouble(&m_tuple, index);
    }

private:
    void destroy()
    {
        if (m_tuple.num > 0 || m_tuple.capacity > 0)
            m_api->destroyTuple(&m_tuple);
        m_tuple = HTUPLE_INITIALIZER;
    }

    HalconCApi *m_api = nullptr;
    Htuple m_tuple = HTUPLE_INITIALIZER;
};

class HalconObject
{
public:
    explicit HalconObject(HalconCApi *api)
        : m_api(api)
    {
    }

    HalconObject(const HalconObject &) = delete;
    HalconObject &operator=(const HalconObject &) = delete;

    ~HalconObject()
    {
        if (halconObjectAllocated(m_object))
            m_api->clearObj(m_object);
    }

    Hobject *ptr()
    {
        return &m_object;
    }

    Hobject value() const
    {
        return m_object;
    }

private:
    HalconCApi *m_api = nullptr;
    Hobject m_object = NO_OBJECTS;
};

QString halconErrorText(HalconCApi *api, Herror status)
{
    char buffer[1024] = {0};
    if (api->getErrorText
            && halconStatusOk(api->getErrorText(static_cast<Hlong>(status), buffer))) {
        const QString message = QString::fromUtf8(buffer).trimmed();
        if (!message.isEmpty())
            return message;
    }
    return QStringLiteral("Unknown HALCON error");
}

void checkHalcon(HalconCApi *api, Herror status, const QString &stage)
{
    if (halconStatusOk(status))
        return;

    const QString code = QString::number(static_cast<qlonglong>(status));
    throw RunnerFailure{
        isLicenseError(status) ? QStringLiteral("halcon_license_error")
                               : QStringLiteral("halcon_error"),
        QStringLiteral("%1: HALCON #%2: %3")
            .arg(stage, code, halconErrorText(api, status))
    };
}

HalconTuple scalarTuple(HalconCApi *api, double value)
{
    HalconTuple tuple(api);
    tuple.create(1);
    tuple.setDouble(0, value);
    return tuple;
}

HalconTuple stringTuple(HalconCApi *api, const char *value)
{
    HalconTuple tuple(api);
    tuple.create(1);
    tuple.setString(0, value);
    return tuple;
}

HalconTuple vectorTuple(HalconCApi *api, const QVector<double> &values)
{
    HalconTuple tuple(api);
    tuple.create(values.size());
    for (int index = 0; index < values.size(); ++index)
        tuple.setDouble(index, values.at(index));
    return tuple;
}

HalconTuple indexTuple(HalconCApi *api, const QVector<int> &values)
{
    HalconTuple tuple(api);
    tuple.create(values.size());
    for (int index = 0; index < values.size(); ++index)
        tuple.setInt(index, values.at(index));
    return tuple;
}

QVector<double> tupleToVector(const HalconTuple &tuple)
{
    QVector<double> values;
    values.reserve(tuple.size());
    for (int index = 0; index < tuple.size(); ++index)
        values.append(tuple.doubleAt(index));
    return values;
}

double tupleScalar(const HalconTuple &tuple)
{
    return tuple.size() > 0 ? tuple.doubleAt(0) : 0.0;
}

QVector<double> normalizedTupleValues(HalconCApi *api,
                                      const HalconTuple &raw,
                                      const QString &stage)
{
    HalconTuple sum(api);
    checkHalcon(api, api->tupleSum(raw.value(), sum.ptr()),
                stage + QStringLiteral(".tuple_sum"));
    if (tupleScalar(sum) <= 0.0) {
        throw RunnerFailure{QStringLiteral("halcon_error"),
                            stage + QStringLiteral(": histogram sum is zero")};
    }

    HalconTuple doubleDenominator = scalarTuple(api, tupleScalar(sum));
    HalconTuple normalized(api);
    checkHalcon(api,
                api->tupleDiv(raw.value(),
                              doubleDenominator.value(),
                              normalized.ptr()),
                stage + QStringLiteral(".tuple_div"));
    return tupleToVector(normalized);
}

double regionArea(HalconCApi *api, Hobject region, const QString &stage)
{
    HalconTuple area(api);
    HalconTuple row(api);
    HalconTuple column(api);
    checkHalcon(api, api->areaCenter(region, area.ptr(), row.ptr(), column.ptr()),
                stage + QStringLiteral(".area_center"));
    return tupleScalar(area);
}

QPair<double, double> intensityStatistics(HalconCApi *api,
                                          Hobject region,
                                          Hobject image,
                                          const QString &stage)
{
    HalconTuple mean(api);
    HalconTuple deviation(api);
    checkHalcon(api, api->intensity(region, image, mean.ptr(), deviation.ptr()),
                stage + QStringLiteral(".intensity"));
    return qMakePair(tupleScalar(mean), tupleScalar(deviation));
}

void createPolygonRegion(HalconCApi *api,
                         const QVector<QPointF> &normalizedPoints,
                         int width,
                         int height,
                         HalconObject *region,
                         const QString &stage)
{
    QVector<double> rows;
    QVector<double> columns;
    rows.reserve(normalizedPoints.size());
    columns.reserve(normalizedPoints.size());
    for (const QPointF &point : normalizedPoints) {
        rows.append(point.y() * static_cast<double>(height - 1));
        columns.append(point.x() * static_cast<double>(width - 1));
    }
    HalconTuple rowTuple = vectorTuple(api, rows);
    HalconTuple columnTuple = vectorTuple(api, columns);
    checkHalcon(api,
                api->genRegionPolygonFilled(region->ptr(),
                                            rowTuple.value(),
                                            columnTuple.value()),
                stage + QStringLiteral(".gen_region_polygon_filled"));
}

void createRectangleRegion(HalconCApi *api,
                           const QRectF &normalizedRect,
                           int width,
                           int height,
                           HalconObject *region,
                           const QString &stage)
{
    const double row1 = normalizedRect.y() * static_cast<double>(height);
    const double column1 = normalizedRect.x() * static_cast<double>(width);
    const double row2 = (normalizedRect.y() + normalizedRect.height())
            * static_cast<double>(height) - 1.0;
    const double column2 = (normalizedRect.x() + normalizedRect.width())
            * static_cast<double>(width) - 1.0;
    checkHalcon(api,
                api->genRectangle1(region->ptr(), row1, column1, row2, column2),
                stage + QStringLiteral(".gen_rectangle1"));
}

void createCircleRegion(HalconCApi *api,
                        const QPointF &normalizedCenter,
                        double normalizedRadius,
                        int width,
                        int height,
                        HalconObject *region,
                        const QString &stage)
{
    const CirclePixelGeometry geometry = circlePixelGeometry(
                normalizedCenter, normalizedRadius, width, height);
    checkHalcon(api,
                api->genCircle(region->ptr(),
                               geometry.center.y(),
                               geometry.center.x(),
                               geometry.radius),
                stage + QStringLiteral(".gen_circle"));
}

void createEffectiveRegion(HalconCApi *api,
                           const ColorComparisonHalconConfig &config,
                           const cv::Mat &image,
                           bool templateRegion,
                           HalconObject *baseRegion,
                           HalconObject *maskRegion,
                           HalconObject *differenceRegion,
                           Hobject *effectiveRegion,
                           bool *maskApplied)
{
    const QString stage = templateRegion ? QStringLiteral("template_region")
                                         : QStringLiteral("detect_region");
    const QString detectType = normalizedDetectRegionType(config.detectRegionType);
    const QString templateMode = normalizedTemplateRegionMode(config.templateRegionMode);
    const bool useDetectionGeometry = !templateRegion
            || templateMode == QStringLiteral("sync");

    if (useDetectionGeometry && detectType == QStringLiteral("circle")) {
        createCircleRegion(api,
                           config.detectCircleCenterNormalized,
                           config.detectCircleRadiusNormalized,
                           image.cols,
                           image.rows,
                           baseRegion,
                           stage);
    } else {
        const QRectF rect = useDetectionGeometry ? config.detectRoiNormalized
                                                 : config.templateRoiNormalized;
        createRectangleRegion(api, rect, image.cols, image.rows, baseRegion, stage);
    }

    *effectiveRegion = baseRegion->value();
    *maskApplied = false;
    const QVector<QPointF> &ownerMask = templateRegion
            ? config.templateMaskPolygonNormalized
            : config.detectMaskPolygonNormalized;
    if (!ownerMask.isEmpty()) {
        createPolygonRegion(api,
                            ownerMask,
                            image.cols,
                            image.rows,
                            maskRegion,
                            stage);
        checkHalcon(api,
                    api->difference(*effectiveRegion,
                                    maskRegion->value(),
                                    differenceRegion->ptr()),
                    stage + QStringLiteral(".difference"));
        *effectiveRegion = differenceRegion->value();
        *maskApplied = true;
    }

    const double area = regionArea(api, *effectiveRegion, stage);
    if (area < static_cast<double>(kColorComparisonMinimumEffectivePixels)) {
        if (*maskApplied) {
            throw RunnerFailure{
                templateRegion ? QStringLiteral("template_masked_empty")
                               : QStringLiteral("detect_masked_empty"),
                templateRegion
                    ? QStringLiteral("Template mask leaves too few effective pixels.")
                    : QStringLiteral("Detection mask leaves too few effective pixels.")
            };
        }
        throw RunnerFailure{
            templateRegion ? QStringLiteral("invalid_template_roi")
                           : QStringLiteral("invalid_detect_roi"),
            templateRegion
                ? QStringLiteral("Template ROI contains too few effective pixels.")
                : QStringLiteral("Detection ROI contains too few effective pixels.")
        };
    }
}

struct ExtractedFeature
{
    QVector<double> hsHistogram;
    QVector<double> valueHistogram;
    qint64 effectivePixelCount = 0;
    double meanBefore = 0.0;
    double deviationBefore = 0.0;
    double meanAfter = 0.0;
    double scale = 1.0;
    double clippedRatio = 0.0;
    bool compensationApplied = false;
    bool compensationFallback = false;
    QString compensationFallbackReason;
};

RunnerFailure illuminationFailure(const QString &message,
                                   const ColorComparisonHalconConfig &config,
                                   const ExtractedFeature &extracted,
                                   bool templateRegion,
                                   double templateMean)
{
    RunnerFailure failure{
        QStringLiteral("invalid_illumination"),
        message,
        QJsonObject()
    };
    failure.payloadPatch.insert(
                QStringLiteral("brightnessCompensation"),
                QJsonObject{
                    {QStringLiteral("enabled"), config.brightnessCompensation},
                    {QStringLiteral("requested"), config.brightnessCompensation},
                    {QStringLiteral("applied"), extracted.compensationApplied},
                    {QStringLiteral("fallback"), extracted.compensationFallback},
                    {QStringLiteral("fallbackReason"),
                     extracted.compensationFallbackReason},
                    {QStringLiteral("templateMean"),
                     templateRegion ? extracted.meanBefore : templateMean},
                    {QStringLiteral("detectMeanBefore"),
                     templateRegion ? 0.0 : extracted.meanBefore},
                    {QStringLiteral("detectMeanAfter"),
                     templateRegion ? 0.0 : extracted.meanAfter},
                    {QStringLiteral("scale"), extracted.scale},
                    {QStringLiteral("clippedRatio"), extracted.clippedRatio}
                });
    return failure;
}

double compensationClippedRatio(HalconCApi *api,
                                Hobject region,
                                Hobject valueImage,
                                double scale,
                                double effectivePixelCount)
{
    if (scale <= 1.0 || effectivePixelCount <= 0.0)
        return 0.0;

    HalconObject histogram(api);
    checkHalcon(api,
                api->histo2Dim(region, valueImage, valueImage, histogram.ptr()),
                QStringLiteral("brightness.clipped.histo_2dim"));

    QVector<int> diagonal;
    diagonal.reserve(256);
    for (int grayValue = 0; grayValue < 256; ++grayValue)
        diagonal.append(grayValue);
    HalconTuple rowTuple = indexTuple(api, diagonal);
    HalconTuple columnTuple = indexTuple(api, diagonal);
    HalconTuple counts(api);
    checkHalcon(api,
                api->getGrayval(histogram.value(),
                                rowTuple.value(),
                                columnTuple.value(),
                                counts.ptr()),
                QStringLiteral("brightness.clipped.get_grayval"));

    QVector<int> clippedIndices;
    for (int grayValue = 0; grayValue < 256; ++grayValue) {
        if (static_cast<double>(grayValue) * scale > 255.0)
            clippedIndices.append(grayValue);
    }
    if (clippedIndices.isEmpty())
        return 0.0;

    HalconTuple indices = indexTuple(api, clippedIndices);
    HalconTuple clippedCounts(api);
    checkHalcon(api,
                api->tupleSelect(counts.value(), indices.value(), clippedCounts.ptr()),
                QStringLiteral("brightness.clipped.tuple_select"));
    HalconTuple clippedSum(api);
    checkHalcon(api,
                api->tupleSum(clippedCounts.value(), clippedSum.ptr()),
                QStringLiteral("brightness.clipped.tuple_sum"));
    HalconTuple denominator = scalarTuple(api, effectivePixelCount);
    HalconTuple ratio(api);
    checkHalcon(api,
                api->tupleDiv(clippedSum.value(), denominator.value(), ratio.ptr()),
                QStringLiteral("brightness.clipped.tuple_div"));
    return qBound(0.0, tupleScalar(ratio), 1.0);
}

QVector<double> hsHistogram(HalconCApi *api,
                            Hobject region,
                            Hobject hue,
                            Hobject saturation)
{
    HalconTuple multiplier = scalarTuple(api, 31.0 / 255.0);
    HalconTuple add = scalarTuple(api, 0.0);
    HalconObject quantizedHue(api);
    HalconObject quantizedSaturation(api);
    checkHalcon(api,
                api->scaleImage(hue, quantizedHue.ptr(), multiplier.value(), add.value()),
                QStringLiteral("hs_histogram.quantize_h.scale_image"));
    checkHalcon(api,
                api->scaleImage(saturation,
                                quantizedSaturation.ptr(),
                                multiplier.value(),
                                add.value()),
                QStringLiteral("hs_histogram.quantize_s.scale_image"));

    HalconObject histogram(api);
    checkHalcon(api,
                api->histo2Dim(region,
                               quantizedHue.value(),
                               quantizedSaturation.value(),
                               histogram.ptr()),
                QStringLiteral("hs_histogram.histo_2dim"));

    QVector<int> rows;
    QVector<int> columns;
    rows.reserve(kHistogramLength);
    columns.reserve(kHistogramLength);
    for (int hueBin = 0; hueBin < kHistogramBins; ++hueBin) {
        for (int saturationBin = 0; saturationBin < kHistogramBins;
             ++saturationBin) {
            // HALCON's ImageCol/ImageRow parameter names, related official
            // guides, and the 24.11.1 runtime canary agree that the first
            // image maps to columns and the second to rows.  The tuple stays
            // externally H-major even though the operator-page prose differs.
            rows.append(saturationBin);
            columns.append(hueBin);
        }
    }
    HalconTuple rowTuple = indexTuple(api, rows);
    HalconTuple columnTuple = indexTuple(api, columns);
    HalconTuple values(api);
    checkHalcon(api,
                api->getGrayval(histogram.value(),
                                rowTuple.value(),
                                columnTuple.value(),
                                values.ptr()),
                QStringLiteral("hs_histogram.get_grayval"));
    return normalizedTupleValues(api, values, QStringLiteral("hs_histogram.normalize"));
}

QVector<double> valueHistogram(HalconCApi *api,
                               Hobject region,
                               Hobject valueImage)
{
    HalconTuple multiplier = scalarTuple(api, 31.0 / 255.0);
    HalconTuple add = scalarTuple(api, 0.0);
    HalconObject quantizedValue(api);
    checkHalcon(api,
                api->scaleImage(valueImage,
                                quantizedValue.ptr(),
                                multiplier.value(),
                                add.value()),
                QStringLiteral("value_histogram.quantize_v.scale_image"));

    HalconObject histogram(api);
    checkHalcon(api,
                api->histo2Dim(region,
                               quantizedValue.value(),
                               quantizedValue.value(),
                               histogram.ptr()),
                QStringLiteral("value_histogram.histo_2dim"));

    QVector<int> diagonal;
    diagonal.reserve(kHistogramBins);
    for (int bin = 0; bin < kHistogramBins; ++bin)
        diagonal.append(bin);
    HalconTuple rowTuple = indexTuple(api, diagonal);
    HalconTuple columnTuple = indexTuple(api, diagonal);
    HalconTuple values(api);
    checkHalcon(api,
                api->getGrayval(histogram.value(),
                                rowTuple.value(),
                                columnTuple.value(),
                                values.ptr()),
                QStringLiteral("value_histogram.get_grayval"));
    return normalizedTupleValues(api, values,
                                 QStringLiteral("value_histogram.normalize"));
}

ExtractedFeature extractFeature(HalconCApi *api,
                                const cv::Mat &input,
                                const ColorComparisonHalconConfig &config,
                                bool templateRegion,
                                bool applyBrightnessCompensation,
                                double templateMean)
{
    const cv::Mat continuous = input.isContinuous() ? input : input.clone();
    HalconObject image(api);
    const char *colorFormat = continuous.type() == CV_8UC4 ? "bgrx" : "bgr";
    checkHalcon(api,
                api->genImageInterleaved(
                    image.ptr(),
                    reinterpret_cast<Hlong>(continuous.data),
                    colorFormat,
                    continuous.cols,
                    continuous.rows,
                    0,
                    "byte",
                    0,
                    0,
                    0,
                    0,
                    8,
                    0),
                QStringLiteral("gen_image_interleaved"));

    HalconObject red(api);
    HalconObject green(api);
    HalconObject blue(api);
    checkHalcon(api, api->decompose3(image.value(), red.ptr(), green.ptr(), blue.ptr()),
                QStringLiteral("decompose3"));

    HalconObject hue(api);
    HalconObject saturation(api);
    HalconObject value(api);
    checkHalcon(api,
                api->transFromRgb(red.value(),
                                  green.value(),
                                  blue.value(),
                                  hue.ptr(),
                                  saturation.ptr(),
                                  value.ptr(),
                                  "hsv"),
                QStringLiteral("trans_from_rgb"));

    HalconObject baseRegion(api);
    HalconObject maskRegion(api);
    HalconObject differenceRegion(api);
    HalconObject transformedRegion(api);
    HalconObject clippedRegion(api);
    Hobject effectiveRegion = NO_OBJECTS;
    bool maskApplied = false;
    createEffectiveRegion(api,
                          config,
                          continuous,
                          templateRegion,
                          &baseRegion,
                          &maskRegion,
                          &differenceRegion,
                          &effectiveRegion,
                          &maskApplied);
    Q_UNUSED(maskApplied)

    if (!templateRegion && config.positionCorrectionApplied) {
        HalconTuple homMat = vectorTuple(api, config.referenceToRunHomMat2D);
        HalconTuple interpolation = stringTuple(api, "nearest_neighbor");
        checkHalcon(api,
                    api->affineTransRegion(effectiveRegion,
                                           transformedRegion.ptr(),
                                           homMat.value(),
                                           interpolation.value()),
                    QStringLiteral("detect_region.affine_trans_region"));
        HalconTuple row1 = scalarTuple(api, 0.0);
        HalconTuple column1 = scalarTuple(api, 0.0);
        HalconTuple row2 = scalarTuple(api, continuous.rows - 1.0);
        HalconTuple column2 = scalarTuple(api, continuous.cols - 1.0);
        checkHalcon(api,
                    api->clipRegion(transformedRegion.value(),
                                    clippedRegion.ptr(),
                                    row1.value(),
                                    column1.value(),
                                    row2.value(),
                                    column2.value()),
                    QStringLiteral("detect_region.clip_region"));
        effectiveRegion = clippedRegion.value();
        const double correctedArea = regionArea(
                    api, effectiveRegion,
                    QStringLiteral("detect_region.corrected"));
        if (correctedArea < static_cast<double>(
                    kColorComparisonMinimumEffectivePixels)) {
            throw RunnerFailure{
                QStringLiteral("corrected_detect_roi_empty"),
                QStringLiteral("Corrected detection ROI is outside the current image.")
            };
        }
    }

    ExtractedFeature extracted;
    extracted.effectivePixelCount = qRound64(regionArea(
        api,
        effectiveRegion,
        templateRegion ? QStringLiteral("template_region")
                       : QStringLiteral("detect_region")));
    const QPair<double, double> before = intensityStatistics(
                api, effectiveRegion, value.value(), QStringLiteral("brightness.before"));
    extracted.meanBefore = before.first;
    extracted.deviationBefore = before.second;
    extracted.meanAfter = before.first;

    Hobject featureHue = hue.value();
    Hobject featureSaturation = saturation.value();
    Hobject featureValue = value.value();
    HalconObject scaledRed(api);
    HalconObject scaledGreen(api);
    HalconObject scaledBlue(api);
    HalconObject composed(api);
    HalconObject compensatedHue(api);
    HalconObject compensatedSaturation(api);
    HalconObject compensatedValue(api);

    if (applyBrightnessCompensation) {
        if (!finiteValue(templateMean)
                || templateMean < kMinBrightnessMean
                || templateMean > kMaxBrightnessMean
                || !finiteValue(extracted.meanBefore)
                || extracted.meanBefore < kMinBrightnessMean
                || extracted.meanBefore > kMaxBrightnessMean) {
            throw illuminationFailure(
                        QStringLiteral("Template or detection brightness mean is outside [8,247]."),
                        config,
                        extracted,
                        templateRegion,
                        templateMean);
        }

        extracted.scale = templateMean / extracted.meanBefore;
        if (!finiteValue(extracted.scale)
                || extracted.scale < kMinBrightnessScale
                || extracted.scale > kMaxBrightnessScale) {
            extracted.compensationFallback = true;
            extracted.compensationFallbackReason =
                    QStringLiteral("scale_out_of_range");
        } else {
            extracted.clippedRatio = compensationClippedRatio(
                        api,
                        effectiveRegion,
                        value.value(),
                        extracted.scale,
                        static_cast<double>(extracted.effectivePixelCount));
            if (extracted.clippedRatio > kMaxClippedRatio) {
                extracted.compensationFallback = true;
                extracted.compensationFallbackReason =
                        QStringLiteral("clip_ratio_exceeded");
            }
        }

        if (!extracted.compensationFallback) {
            HalconTuple multiplier = scalarTuple(api, extracted.scale);
            HalconTuple add = scalarTuple(api, 0.0);
            checkHalcon(api,
                        api->scaleImage(red.value(),
                                        scaledRed.ptr(),
                                        multiplier.value(),
                                        add.value()),
                        QStringLiteral("brightness.scale_red.scale_image"));
            checkHalcon(api,
                        api->scaleImage(green.value(),
                                        scaledGreen.ptr(),
                                        multiplier.value(),
                                        add.value()),
                        QStringLiteral("brightness.scale_green.scale_image"));
            checkHalcon(api,
                        api->scaleImage(blue.value(),
                                        scaledBlue.ptr(),
                                        multiplier.value(),
                                        add.value()),
                        QStringLiteral("brightness.scale_blue.scale_image"));
            checkHalcon(api,
                        api->compose3(scaledRed.value(),
                                      scaledGreen.value(),
                                      scaledBlue.value(),
                                      composed.ptr()),
                        QStringLiteral("brightness.compose3"));
            checkHalcon(api,
                        api->transFromRgb(scaledRed.value(),
                                          scaledGreen.value(),
                                          scaledBlue.value(),
                                          compensatedHue.ptr(),
                                          compensatedSaturation.ptr(),
                                          compensatedValue.ptr(),
                                          "hsv"),
                        QStringLiteral("brightness.trans_from_rgb"));
            featureHue = compensatedHue.value();
            featureSaturation = compensatedSaturation.value();
            featureValue = compensatedValue.value();
            extracted.compensationApplied = true;
            extracted.meanAfter = intensityStatistics(
                        api,
                        effectiveRegion,
                        featureValue,
                        QStringLiteral("brightness.after")).first;
        }
    }

    extracted.hsHistogram = hsHistogram(api,
                                        effectiveRegion,
                                        featureHue,
                                        featureSaturation);
    extracted.valueHistogram = valueHistogram(api, effectiveRegion, featureValue);
    if (templateRegion && config.brightnessCompensation
            && (extracted.meanBefore < kMinBrightnessMean
                || extracted.meanBefore > kMaxBrightnessMean)) {
        throw illuminationFailure(
                    QStringLiteral("Template brightness is outside the safe compensation range."),
                    config,
                    extracted,
                    templateRegion,
                    extracted.meanBefore);
    }
    return extracted;
}

double histogramIntersection(HalconCApi *api,
                             const QVector<double> &left,
                             const QVector<double> &right)
{
    if (left.size() != kHistogramLength || right.size() != kHistogramLength) {
        throw RunnerFailure{QStringLiteral("invalid_smoothed_histogram"),
                            QStringLiteral("H/S histograms must contain 1024 values.")};
    }
    HalconTuple leftTuple = vectorTuple(api, left);
    HalconTuple rightTuple = vectorTuple(api, right);
    HalconTuple minimum(api);
    checkHalcon(api,
                api->tupleMin2(leftTuple.value(), rightTuple.value(), minimum.ptr()),
                QStringLiteral("score.tuple_min2"));
    HalconTuple sum(api);
    checkHalcon(api, api->tupleSum(minimum.value(), sum.ptr()),
                QStringLiteral("score.tuple_sum"));
    const double result = tupleScalar(sum);
    if (!finiteValue(result)) {
        throw RunnerFailure{QStringLiteral("invalid_smoothed_histogram"),
                            QStringLiteral("Histogram intersection is not finite.")};
    }
    return qBound(0.0, result, 1.0);
}

QVector<double> smoothHsHistogram(HalconCApi *api,
                                  const QVector<double> &histogram,
                                  const SmoothingProfile &profile)
{
    if (histogram.size() != kHistogramLength) {
        throw RunnerFailure{QStringLiteral("invalid_smoothed_histogram"),
                            QStringLiteral("H/S histogram must contain 1024 values.")};
    }

    QVector<float> tiled(kPaddedHistogramWidth * kTiledHistogramHeight, 0.0f);
    for (int hueBin = 0; hueBin < kHistogramBins; ++hueBin) {
        for (int saturationBin = 0; saturationBin < kHistogramBins;
             ++saturationBin) {
            const double value = histogram.at(hueBin * kHistogramBins
                                              + saturationBin);
            if (!finiteValue(value) || value < 0.0) {
                throw RunnerFailure{QStringLiteral("invalid_smoothed_histogram"),
                                    QStringLiteral("H/S histogram contains an invalid value.")};
            }
            for (int repeat = 0; repeat < 3; ++repeat) {
                tiled[(repeat * kHistogramBins + hueBin)
                      * kPaddedHistogramWidth
                      + kSaturationOffset + saturationBin]
                        = static_cast<float>(value);
            }
        }
    }

    HalconTuple realType = stringTuple(api, "real");
    HalconTuple width = scalarTuple(api, kPaddedHistogramWidth);
    HalconTuple height = scalarTuple(api, kTiledHistogramHeight);
    HalconTuple pointer(api);
    pointer.create(1);
    pointer.setInt(0, reinterpret_cast<Hlong>(tiled.data()));

    HalconObject image(api);
    checkHalcon(api,
                api->genImage1(image.ptr(), realType.value(), width.value(),
                               height.value(), pointer.value()),
                QStringLiteral("score.gen_image1"));

    HalconTuple saturationSigma = scalarTuple(api, profile.saturationSigma);
    HalconTuple hueSigma = scalarTuple(api, profile.hueSigma);
    HalconTuple phi = scalarTuple(api, 0.0);
    HalconTuple filterNorm = stringTuple(api, "n");
    HalconTuple filterMode = stringTuple(api, "rft");
    HalconObject filter(api);
    checkHalcon(api,
                api->genGaussFilter(filter.ptr(), saturationSigma.value(),
                                    hueSigma.value(), phi.value(),
                                    filterNorm.value(), filterMode.value(),
                                    width.value(), height.value()),
                QStringLiteral("score.gen_gauss_filter"));

    HalconTuple toFrequency = stringTuple(api, "to_freq");
    HalconTuple none = stringTuple(api, "none");
    HalconTuple complex = stringTuple(api, "complex");
    HalconObject frequency(api);
    checkHalcon(api,
                api->rftGeneric(image.value(), frequency.ptr(),
                                toFrequency.value(), none.value(),
                                complex.value(), width.value()),
                QStringLiteral("score.rft_to_freq"));
    HalconObject convolved(api);
    checkHalcon(api,
                api->convolFft(frequency.value(), filter.value(), convolved.ptr()),
                QStringLiteral("score.convol_fft"));

    HalconTuple fromFrequency = stringTuple(api, "from_freq");
    HalconObject smoothed(api);
    checkHalcon(api,
                api->rftGeneric(convolved.value(), smoothed.ptr(),
                                fromFrequency.value(), none.value(),
                                realType.value(), width.value()),
                QStringLiteral("score.rft_from_freq"));

    QVector<int> rows;
    QVector<int> columns;
    rows.reserve(kHistogramLength);
    columns.reserve(kHistogramLength);
    for (int hueBin = 0; hueBin < kHistogramBins; ++hueBin) {
        for (int saturationBin = 0; saturationBin < kHistogramBins;
             ++saturationBin) {
            rows.append(kHistogramBins + hueBin);
            columns.append(kSaturationOffset + saturationBin);
        }
    }
    HalconTuple rowTuple = indexTuple(api, rows);
    HalconTuple columnTuple = indexTuple(api, columns);
    HalconTuple values(api);
    checkHalcon(api,
                api->getGrayval(smoothed.value(), rowTuple.value(),
                                columnTuple.value(), values.ptr()),
                QStringLiteral("score.get_grayval"));
    QVector<double> cleanedValues = tupleToVector(values);
    for (double &value : cleanedValues) {
        if (!finiteValue(value)) {
            throw RunnerFailure{QStringLiteral("invalid_smoothed_histogram"),
                                QStringLiteral("Smoothed histogram contains an invalid value.")};
        }
        // The inverse FFT may leave very small negative round-off values.
        // They are not histogram mass and must not invalidate a valid model.
        if (value < 0.0)
            value = 0.0;
    }
    HalconTuple cleanedTuple = vectorTuple(api, cleanedValues);
    return normalizedTupleValues(api, cleanedTuple,
                                 QStringLiteral("score.normalize_smoothed"));
}

double meanSaturation(const QVector<double> &histogram)
{
    double result = 0.0;
    for (int hueBin = 0; hueBin < kHistogramBins; ++hueBin) {
        for (int saturationBin = 0; saturationBin < kHistogramBins;
             ++saturationBin) {
            result += histogram.at(hueBin * kHistogramBins + saturationBin)
                    * static_cast<double>(saturationBin)
                    / static_cast<double>(kHistogramBins - 1);
        }
    }
    return qBound(0.0, result, 1.0);
}

QJsonObject brightnessDiagnostics(const ColorComparisonHalconConfig &config,
                                  const ExtractedFeature &extracted)
{
    return {
        {QStringLiteral("enabled"), config.brightnessCompensation},
        {QStringLiteral("requested"), config.brightnessCompensation},
        {QStringLiteral("applied"), extracted.compensationApplied},
        {QStringLiteral("fallback"), extracted.compensationFallback},
        {QStringLiteral("fallbackReason"),
         extracted.compensationFallbackReason},
        {QStringLiteral("templateMean"), config.model.brightnessReference.mean},
        {QStringLiteral("detectMeanBefore"), extracted.meanBefore},
        {QStringLiteral("detectMeanAfter"), extracted.meanAfter},
        {QStringLiteral("scale"), extracted.scale},
        {QStringLiteral("clippedRatio"), extracted.clippedRatio}
    };
}

} // namespace

ColorComparisonScoreBreakdown ColorComparisonHalconRunner::scoreBreakdown(
        double hsScore,
        double templateBrightnessMean,
        double detectBrightnessMean,
        double templateMeanSaturation,
        double detectMeanSaturation)
{
    ColorComparisonScoreBreakdown breakdown;
    breakdown.hsScore = qBound(0.0, hsScore, 100.0);
    breakdown.brightnessDifference = qBound(
                0.0,
                std::abs(templateBrightnessMean - detectBrightnessMean) / 255.0,
                1.0);
    if (breakdown.brightnessDifference > 0.10
            && breakdown.brightnessDifference < 0.40) {
        breakdown.brightnessFactor = 1.10 - breakdown.brightnessDifference;
    } else if (breakdown.brightnessDifference >= 0.40) {
        breakdown.brightnessFactor = 0.70;
    }

    const double colorScore = breakdown.hsScore * breakdown.brightnessFactor;
    breakdown.grayScore = 100.0 * qMax(
                0.0, 1.0 - breakdown.brightnessDifference / 0.50);
    const double minimumSaturation = qMin(templateMeanSaturation,
                                          detectMeanSaturation);
    const double maximumSaturation = qMax(templateMeanSaturation,
                                          detectMeanSaturation);
    breakdown.grayWeight = qBound(
                0.0, (0.20 - maximumSaturation) / 0.10, 1.0);
    breakdown.baseScoreBeforeSaturationPenalty =
            breakdown.grayWeight * breakdown.grayScore
            + (1.0 - breakdown.grayWeight) * colorScore;

    if (minimumSaturation <= 0.10) {
        const double linearProgress = qBound(
                    0.0, (maximumSaturation - 0.10) / 0.10, 1.0);
        breakdown.saturationMismatchProgress = linearProgress * linearProgress
                * (3.0 - 2.0 * linearProgress);
        breakdown.saturationFactor =
                1.0 - 0.60 * breakdown.saturationMismatchProgress;
    }
    breakdown.finalScore = qBound(
                0.0,
                breakdown.baseScoreBeforeSaturationPenalty
                * breakdown.saturationFactor,
                100.0);
    return breakdown;
}

namespace {

QRectF normalizedRectToPixels(const QRectF &rect, const cv::Mat &image)
{
    return QRectF(rect.x() * image.cols,
                  rect.y() * image.rows,
                  rect.width() * image.cols,
                  rect.height() * image.rows);
}

QVector<QPointF> normalizedPointsToPixels(const QVector<QPointF> &points,
                                          const cv::Mat &image)
{
    QVector<QPointF> pixels;
    pixels.reserve(points.size());
    for (const QPointF &point : points) {
        pixels.append(QPointF(point.x() * static_cast<double>(image.cols - 1),
                              point.y() * static_cast<double>(image.rows - 1)));
    }
    return pixels;
}

QPointF transformPixelPoint(const ColorComparisonHalconConfig &config,
                            const QPointF &point)
{
    if (!config.positionCorrectionApplied
            || config.referenceToRunHomMat2D.size() != 6) {
        return point;
    }
    const QVector<double> &matrix = config.referenceToRunHomMat2D;
    const double row = point.y();
    const double column = point.x();
    return QPointF(matrix.at(3) * row + matrix.at(4) * column + matrix.at(5),
                   matrix.at(0) * row + matrix.at(1) * column + matrix.at(2));
}

QVector<QPointF> transformPixelPoints(const ColorComparisonHalconConfig &config,
                                     const QVector<QPointF> &points)
{
    QVector<QPointF> transformed;
    transformed.reserve(points.size());
    for (const QPointF &point : points)
        transformed.append(transformPixelPoint(config, point));
    return transformed;
}

ToolOverlay resultTextOverlay(const QRectF &anchorRect,
                              double score,
                              bool passed)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.label = QStringLiteral("color_result_text");
    overlay.text = QStringLiteral("%1 score:%2")
            .arg(passed ? QStringLiteral("OK") : QStringLiteral("NG"),
                 QString::number(score, 'f', 1));
    overlay.score = score;
    overlay.p1 = anchorRect.topLeft();
    overlay.extra.insert(QStringLiteral("status"),
                         passed ? QStringLiteral("OK") : QStringLiteral("NG"));
    overlay.extra.insert(QStringLiteral("anchorRect"), rectToJson(anchorRect));
    return overlay;
}

QVector<ToolOverlay> detectionOverlays(const ColorComparisonHalconConfig &config,
                                       const cv::Mat &image,
                                       double score,
                                       bool passed)
{
    QVector<ToolOverlay> overlays;
    ToolOverlay roi;
    QRectF anchorRect;
    if (normalizedDetectRegionType(config.detectRegionType)
            == QStringLiteral("circle")) {
        const CirclePixelGeometry geometry = circlePixelGeometry(
                    config.detectCircleCenterNormalized,
                    config.detectCircleRadiusNormalized,
                    image.cols,
                    image.rows);
        roi.type = ToolOverlayType::Circle;
        roi.center = transformPixelPoint(config, geometry.center);
        const QPointF transformedRadiusPoint = transformPixelPoint(
                    config,
                    QPointF(geometry.center.x() + geometry.radius,
                            geometry.center.y()));
        roi.radius = std::hypot(transformedRadiusPoint.x() - roi.center.x(),
                                transformedRadiusPoint.y() - roi.center.y());
        anchorRect = QRectF(roi.center.x() - roi.radius,
                            roi.center.y() - roi.radius,
                            roi.radius * 2.0,
                            roi.radius * 2.0);
    } else {
        const QRectF referenceRect = normalizedRectToPixels(
                    config.detectRoiNormalized, image);
        if (config.positionCorrectionApplied) {
            roi.type = ToolOverlayType::Polygon;
            roi.points = transformPixelPoints(config, QVector<QPointF>{
                referenceRect.topLeft(), referenceRect.topRight(),
                referenceRect.bottomRight(), referenceRect.bottomLeft()
            });
            anchorRect = QPolygonF(roi.points).boundingRect();
        } else {
            roi.type = ToolOverlayType::Rect;
            roi.rect = referenceRect;
            anchorRect = roi.rect;
        }
    }
    roi.label = QStringLiteral("Detection ROI");
    roi.extra.insert(QStringLiteral("role"), QStringLiteral("detect_roi"));
    roi.extra.insert(QStringLiteral("positionCorrectionApplied"),
                     config.positionCorrectionApplied);
    if (config.positionCorrectionApplied)
        roi.extra.insert(QStringLiteral("positionCorrectionSourceId"),
                         config.positionCorrectionSourceId);
    overlays.append(roi);

    if (!config.detectMaskPolygonNormalized.isEmpty()) {
        ToolOverlay mask;
        mask.type = ToolOverlayType::Polygon;
        mask.points = transformPixelPoints(
                    config,
                    normalizedPointsToPixels(config.detectMaskPolygonNormalized, image));
        mask.label = QStringLiteral("Detection Mask");
        mask.extra.insert(QStringLiteral("role"), QStringLiteral("detect_mask"));
        overlays.append(mask);
    }
    if (config.positionCorrectionApplied
            && config.showPositionCorrectionMatchContour) {
        for (ToolOverlay contour : config.positionCorrectionMatchContours) {
            contour.extra.insert(QStringLiteral("role"),
                                 QStringLiteral("position_correction_match_contour"));
            contour.extra.insert(QStringLiteral("positionCorrectionSourceId"),
                                 config.positionCorrectionSourceId);
            overlays.append(contour);
        }
    }
    overlays.append(resultTextOverlay(anchorRect, score, passed));
    return overlays;
}

bool runtimeExists(const ColorComparisonHalconConfig &config)
{
    return !config.halconSoPath.trimmed().isEmpty()
            && QFileInfo::exists(config.halconSoPath);
}

QString runtimeNotFoundMessage(const ColorComparisonHalconConfig &config)
{
    const QString tried = config.halconSoPathCandidates.isEmpty()
            ? config.halconSoPath
            : config.halconSoPathCandidates.join(QStringLiteral("; "));
    return QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
            .arg(config.halconSoPath, tried);
}

} // namespace

ColorComparisonTemplateBuildResult
ColorComparisonHalconRunner::buildTemplateModel(
        const cv::Mat &referenceImage,
        const ColorComparisonHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    if (referenceImage.empty()) {
        return buildFailure(QStringLiteral("image_empty"),
                            QStringLiteral("Reference image is empty."),
                            timer.elapsed());
    }
    const QString colorMode = normalizedColorMode(config.inputSignature.colorMode);
    if (colorMode == QStringLiteral("mono")) {
        return buildFailure(QStringLiteral("unsupported_color_input"),
                            QStringLiteral("A monochrome source cannot build a color model."),
                            timer.elapsed());
    }
    if (colorMode != QStringLiteral("color")
            && colorMode != QStringLiteral("unknown")) {
        return buildFailure(QStringLiteral("unsupported_color_input"),
                            QStringLiteral("Original color mode is invalid."),
                            timer.elapsed());
    }
    if (!supportedOriginalBitDepth(config.inputSignature)
            || !supportedOriginalPixelFormat(config.inputSignature)
            || !supportedImageType(referenceImage)) {
        return buildFailure(QStringLiteral("unsupported_pixel_format"),
                            QStringLiteral("Only original 8-bit CV_8UC3 and CV_8UC4 inputs are supported."),
                            timer.elapsed());
    }
    const RunnerFailure geometryFailure = validateGeometry(
                config, referenceImage.cols, referenceImage.rows);
    if (!geometryFailure.status.isEmpty()) {
        return buildFailure(geometryFailure.status,
                            geometryFailure.message,
                            timer.elapsed());
    }
    if (config.model.featureType != QStringLiteral("histogram_hs_2d")) {
        return buildFailure(QStringLiteral("unsupported_feature"),
                            QStringLiteral("Only histogram_hs_2d is implemented."),
                            timer.elapsed());
    }
    if (!runtimeExists(config)) {
        return buildFailure(QStringLiteral("halcon_so_not_found"),
                            runtimeNotFoundMessage(config),
                            timer.elapsed());
    }

    HalconLibrary library;
    QString loadMessage;
    bool symbolMissing = false;
    if (!library.load(config.halconSoPath, &loadMessage, &symbolMissing)) {
        return buildFailure(symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                          : QStringLiteral("halcon_load_failed"),
                            loadMessage,
                            timer.elapsed());
    }

    try {
        const ExtractedFeature extracted = extractFeature(&library.api,
                                                          referenceImage,
                                                          config,
                                                          true,
                                                          false,
                                                          0.0);
        ColorComparisonTemplateBuildResult result;
        result.success = true;
        result.status = QStringLiteral("ok");
        result.model.state = ColorComparisonModelState::Ready;
        result.model.featureType = QStringLiteral("histogram_hs_2d");
        result.model.algorithm = QStringLiteral("histogram_intersection");
        result.model.colorSpace = QStringLiteral("hsv");
        result.model.hueBins = kHistogramBins;
        result.model.saturationBins = kHistogramBins;
        result.model.layout = QStringLiteral("hue_major");
        result.model.normalized = true;
        result.model.values = extracted.hsHistogram;
        result.model.valueHistogram = extracted.valueHistogram;
        result.model.effectivePixelCount = extracted.effectivePixelCount;
        result.model.referenceImageHash = colorComparisonReferenceHash(referenceImage);
        const QJsonObject extractParams = templateExtractParams(config);
        result.model.extractParamsHash = colorComparisonExtractParamsHash(extractParams);
        result.model.inputSignature = config.inputSignature;
        if (result.model.inputSignature.bitDepth == -1)
            result.model.inputSignature.bitDepth = 8;
        if (result.model.inputSignature.pixelFormat.trimmed().isEmpty()) {
            result.model.inputSignature.pixelFormat = referenceImage.type() == CV_8UC4
                    ? QStringLiteral("BGRA8") : QStringLiteral("BGR8");
        }
        result.model.brightnessReference.mean = extracted.meanBefore;
        result.model.brightnessReference.deviation = extracted.deviationBefore;
        result.payload = {
            {QStringLiteral("status"), result.status},
            {QStringLiteral("algorithm"), result.model.algorithm},
            {QStringLiteral("featureType"), result.model.featureType},
            {QStringLiteral("modelVersion"), 2},
            {QStringLiteral("effectiveTemplatePixels"),
             static_cast<double>(result.model.effectivePixelCount)},
            {QStringLiteral("referenceImageHash"), result.model.referenceImageHash},
            {QStringLiteral("extractParamsHash"), result.model.extractParamsHash},
            {QStringLiteral("extractParams"), extractParams},
            {QStringLiteral("valueHistogram"),
             featureToJson(result.model.valueHistogram)},
            {QStringLiteral("brightnessReference"), QJsonObject{
                 {QStringLiteral("mean"), result.model.brightnessReference.mean},
                 {QStringLiteral("deviation"),
                  result.model.brightnessReference.deviation}
             }},
            {QStringLiteral("warnings"),
             stringsToJson(colorMode == QStringLiteral("unknown")
                           ? QStringList{QStringLiteral("input_color_mode_unknown")}
                           : QStringList())},
            {QStringLiteral("elapsedMs"), static_cast<double>(timer.elapsed())}
        };
        return result;
    } catch (const RunnerFailure &failure) {
        ColorComparisonTemplateBuildResult result =
                buildFailure(failure.status, failure.message, timer.elapsed());
        mergePayloadPatch(&result.payload, failure.payloadPatch);
        return result;
    } catch (const std::exception &error) {
        return buildFailure(QStringLiteral("exception"),
                            QString::fromLocal8Bit(error.what()),
                            timer.elapsed());
    } catch (...) {
        return buildFailure(QStringLiteral("exception"),
                            QStringLiteral("Unknown exception while building color model."),
                            timer.elapsed());
    }
}

ColorComparisonHalconResult ColorComparisonHalconRunner::run(
        const cv::Mat &image,
        const ColorComparisonHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();
    QStringList warnings;

    if (config.positionCorrectionApplied
            && config.showPositionCorrectionMatchContour
            && config.positionCorrectionMatchContours.isEmpty()) {
        warnings.append(QStringLiteral("position_correction_contour_unavailable"));
    }

    if (image.empty()) {
        return runFailure(QStringLiteral("image_empty"),
                          QStringLiteral("Input image is empty."),
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    }

    const QString colorMode = normalizedColorMode(config.inputSignature.colorMode);
    if (colorMode == QStringLiteral("mono"))
        return unsupportedColorResult(config, timer.elapsed(), image.cols, image.rows);
    if (colorMode == QStringLiteral("unknown")) {
        warnings.append(QStringLiteral("input_color_mode_unknown"));
    } else if (colorMode != QStringLiteral("color")) {
        return runFailure(QStringLiteral("unsupported_color_input"),
                          QStringLiteral("Original color mode is invalid."),
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    }

    if (!supportedOriginalBitDepth(config.inputSignature)
            || !supportedOriginalPixelFormat(config.inputSignature)
            || !supportedImageType(image)) {
        return runFailure(QStringLiteral("unsupported_pixel_format"),
                          QStringLiteral("Only original 8-bit CV_8UC3 and CV_8UC4 inputs are supported."),
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    }

    const RunnerFailure geometryFailure = validateGeometry(
                config, image.cols, image.rows);
    if (!geometryFailure.status.isEmpty()) {
        return runFailure(geometryFailure.status,
                          geometryFailure.message,
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    }

    if (config.model.featureType != QStringLiteral("histogram_hs_2d")) {
        return runFailure(QStringLiteral("unsupported_feature"),
                          QStringLiteral("Only histogram_hs_2d is implemented."),
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    }

    const QString expectedExtractParamsHash = colorComparisonExtractParamsHash(
                templateExtractParams(config));
    const ColorComparisonModelValidation modelValidation =
            validateColorComparisonModel(config.model, expectedExtractParamsHash);
    if (!modelValidation.success) {
        return runFailure(modelFailureStatus(modelValidation),
                          modelValidation.message,
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    }

    const SmoothingProfile profile = smoothingProfile(config.sensitivity);
    if (!profile.valid) {
        return runFailure(QStringLiteral("invalid_sensitivity"),
                          QStringLiteral("Sensitivity must be high, medium, or low."),
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    }
    if (!runtimeExists(config)) {
        return runFailure(QStringLiteral("halcon_so_not_found"),
                          runtimeNotFoundMessage(config),
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    }

    HalconLibrary library;
    QString loadMessage;
    bool symbolMissing = false;
    if (!library.load(config.halconSoPath, &loadMessage, &symbolMissing)) {
        return runFailure(symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                        : QStringLiteral("halcon_load_failed"),
                          loadMessage,
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    }

    try {
        const ExtractedFeature extracted = extractFeature(
                    &library.api,
                    image,
                    config,
                    false,
                    config.brightnessCompensation,
                    config.model.brightnessReference.mean);
        if (extracted.compensationFallback)
            warnings.append(QStringLiteral("brightness_compensation_skipped"));
        const double rawIntersection = histogramIntersection(
                    &library.api, config.model.values, extracted.hsHistogram);
        const QVector<double> smoothedTemplate = smoothHsHistogram(
                    &library.api, config.model.values, profile);
        const QVector<double> smoothedDetect = smoothHsHistogram(
                    &library.api, extracted.hsHistogram, profile);
        const double smoothedIntersection = histogramIntersection(
                    &library.api, smoothedTemplate, smoothedDetect);
        const double hsScore = smoothedIntersection * 100.0;
        const double templateMeanSaturation = meanSaturation(config.model.values);
        const double detectMeanSaturation = meanSaturation(extracted.hsHistogram);
        const ColorComparisonScoreBreakdown scoreParts = scoreBreakdown(
                    hsScore,
                    config.model.brightnessReference.mean,
                    extracted.meanAfter,
                    templateMeanSaturation,
                    detectMeanSaturation);
        const double score = scoreParts.finalScore;
        const double similarity = score / 100.0;
        const bool passed = score >= static_cast<double>(config.minScore);

        ColorComparisonHalconResult result;
        result.success = true;
        result.ok = passed;
        result.measurementValid = true;
        result.status = QStringLiteral("ok");
        result.score = score;
        result.similarity = similarity;
        result.elapsedMs = timer.elapsed();
        result.detectFeature = extracted.hsHistogram;
        result.detectValueHistogram = extracted.valueHistogram;
        result.overlays = detectionOverlays(config, image, score, passed);
        result.payload = baseRunPayload(config, warnings, image.cols, image.rows);
        result.payload.insert(QStringLiteral("status"), result.status);
        result.payload.insert(QStringLiteral("message"), result.message);
        result.payload.insert(QStringLiteral("measurementValid"), true);
        result.payload.insert(QStringLiteral("passed"), passed);
        result.payload.insert(QStringLiteral("score"), score);
        result.payload.insert(QStringLiteral("similarity"), similarity);
        result.payload.insert(QStringLiteral("rawIntersection"), rawIntersection);
        result.payload.insert(QStringLiteral("histogramDiagnostics"), QJsonObject{
            {QStringLiteral("available"), true},
            {QStringLiteral("hueBins"), kHistogramBins},
            {QStringLiteral("saturationBins"), kHistogramBins},
            {QStringLiteral("layout"), QStringLiteral("hue_major")},
            {QStringLiteral("detectHsHistogram"),
             featureToJson(extracted.hsHistogram)},
            {QStringLiteral("detectValueHistogram"),
             featureToJson(extracted.valueHistogram)},
            {QStringLiteral("rawIntersection"), rawIntersection}
        });
        result.payload.insert(QStringLiteral("smoothedIntersection"),
                              smoothedIntersection);
        result.payload.insert(QStringLiteral("hsScore"), hsScore);
        result.payload.insert(QStringLiteral("templateMeanSaturation"),
                              templateMeanSaturation);
        result.payload.insert(QStringLiteral("detectMeanSaturation"),
                              detectMeanSaturation);
        result.payload.insert(QStringLiteral("brightnessDifference"),
                              scoreParts.brightnessDifference);
        result.payload.insert(QStringLiteral("brightnessFactor"),
                              scoreParts.brightnessFactor);
        result.payload.insert(QStringLiteral("grayScore"),
                              scoreParts.grayScore);
        result.payload.insert(QStringLiteral("grayWeight"),
                              scoreParts.grayWeight);
        result.payload.insert(QStringLiteral("baseScoreBeforeSaturationPenalty"),
                              scoreParts.baseScoreBeforeSaturationPenalty);
        result.payload.insert(QStringLiteral("saturationMismatchProgress"),
                              scoreParts.saturationMismatchProgress);
        result.payload.insert(QStringLiteral("saturationFactor"),
                              scoreParts.saturationFactor);
        result.payload.insert(QStringLiteral("finalScore"), scoreParts.finalScore);
        result.payload.insert(QStringLiteral("smoothingProfile"), QJsonObject{
            {QStringLiteral("sensitivity"), profile.sensitivity},
            {QStringLiteral("hueSigma"), profile.hueSigma},
            {QStringLiteral("saturationSigma"), profile.saturationSigma},
            {QStringLiteral("hueCircular"), true},
            {QStringLiteral("saturationBoundary"), QStringLiteral("zero_pad")}
        });
        result.payload.insert(QStringLiteral("halconRuntimePath"),
                              QFileInfo(config.halconSoPath).absoluteFilePath());
        result.payload.insert(QStringLiteral("halconRuntimeVersion"),
                              HalconRuntimePaths::expectedHalconVersion());
        result.payload.insert(QStringLiteral("effectiveDetectionPixels"),
                              static_cast<double>(extracted.effectivePixelCount));
        result.payload.insert(QStringLiteral("brightnessCompensation"),
                              brightnessDiagnostics(config, extracted));
        result.payload.insert(QStringLiteral("warnings"), stringsToJson(warnings));
        result.payload.insert(QStringLiteral("elapsedMs"),
                              static_cast<double>(result.elapsedMs));
        return result;
    } catch (const RunnerFailure &failure) {
        ColorComparisonHalconResult result = runFailure(failure.status,
                                                        failure.message,
                                                        config,
                                                        warnings,
                                                        timer.elapsed(),
                                                        image.cols,
                                                        image.rows);
        mergePayloadPatch(&result.payload, failure.payloadPatch);
        return result;
    } catch (const std::exception &error) {
        return runFailure(QStringLiteral("exception"),
                          QString::fromLocal8Bit(error.what()),
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    } catch (...) {
        return runFailure(QStringLiteral("exception"),
                          QStringLiteral("Unknown exception while comparing colors."),
                          config,
                          warnings,
                          timer.elapsed(),
                          image.cols,
                          image.rows);
    }
}
