#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/location/PositionCorrectionHalconTransform.h"
#include "algorithms/recognition/RegisteredClassificationFeatureSpace.h"
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <HalconC.h>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRect>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <exception>
#include <limits>
#include <opencv2/imgproc.hpp>

namespace {

constexpr int kMinRoiPixelSize = 2;
constexpr int kGrayHistogramBins = 256;
constexpr int kCompressedHistogramBins = 16;
constexpr int kGrayHistogramGroupSize = kGrayHistogramBins / kCompressedHistogramBins;
constexpr int kCanonicalSize = 128;
constexpr int kOccupancyGridSize = 4;

struct HalconFeatureApi
{
    using SetUtf8Fn = void (*)(int);
    using GetErrorTextFn = Herror (*)(Hlong, char *);
    using CreateTupleFn = void (*)(Htuple *, Hlong);
    using SetDoubleFn = void (*)(Htuple *, double, Hlong);
    using SetIntFn = void (*)(Htuple *, Hlong, Hlong);
    using SetStringFn = void (*)(Htuple *, const char *, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GetIntFn = Hlong (*)(const Htuple *, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Rgb1ToGrayFn = Herror (*)(const Hobject, Hobject *);
    using GenRectangle1Fn = Herror (*)(Hobject *, double, double, double, double);
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using UnaryObjectTupleFn = Herror (*)(const Hobject, Hobject *, const Htuple);
    using UnaryObjectFn = Herror (*)(const Hobject, Hobject *);
    using BinaryObjectFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using BinaryThresholdFn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple, Htuple *);
    using GenRegionPolygonFilledFn = Herror (*)(Hobject *, const Htuple, const Htuple);
    using SelectObjFn = Herror (*)(const Hobject, Hobject *, const Htuple);
    using CountObjFn = Herror (*)(const Hobject, Htuple *);
    using AreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using IntensityFn = Herror (*)(const Hobject, const Hobject, Htuple *, Htuple *);
    using GrayHistoFn = Herror (*)(const Hobject, const Hobject, Htuple *, Htuple *);
    using SmallestRectangle2Fn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *, Htuple *, Htuple *);
    using SmallestRectangle1Fn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *, Htuple *);
    using VectorAngleToRigidFn = Herror (*)(const Htuple, const Htuple, const Htuple,
                                            const Htuple, const Htuple, const Htuple, Htuple *);
    using AffineTransImageFn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple, const Htuple);
    using AffineTransRegionFn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple);
    using ClipRegionFn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple,
                                    const Htuple, const Htuple);
    using CropRectangle1Fn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple,
                                        const Htuple, const Htuple);
    using MoveRegionFn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple);
    using ZoomImageSizeFn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple, const Htuple);
    using ZoomRegionFn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple);
    using SingleMeasureFn = Herror (*)(const Hobject, Htuple *);
    using EccentricityFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using MomentsCentralInvarFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *, Htuple *);
    using Decompose3Fn = Herror (*)(const Hobject, Hobject *, Hobject *, Hobject *);
    using TransFromRgbFn = Herror (*)(const Hobject, const Hobject, const Hobject,
                                      Hobject *, Hobject *, Hobject *, const Htuple);
    using EntropyGrayFn = Herror (*)(const Hobject, const Hobject, Htuple *, Htuple *);
    using CoocFeatureImageFn = Herror (*)(const Hobject, const Hobject, const Htuple, const Htuple,
                                          Htuple *, Htuple *, Htuple *, Htuple *);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetIntFn setInt = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GetIntFn getInt = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Rgb1ToGrayFn rgb1ToGray = nullptr;
    GenRectangle1Fn genRectangle1 = nullptr;
    ReduceDomainFn reduceDomain = nullptr;
    UnaryObjectTupleFn gaussFilter = nullptr;
    BinaryThresholdFn binaryThreshold = nullptr;
    UnaryObjectTupleFn openingCircle = nullptr;
    UnaryObjectTupleFn closingCircle = nullptr;
    UnaryObjectTupleFn erosionCircle = nullptr;
    UnaryObjectFn fillUp = nullptr;
    UnaryObjectFn connection = nullptr;
    BinaryObjectFn intersection = nullptr;
    BinaryObjectFn difference = nullptr;
    GenRegionPolygonFilledFn genRegionPolygonFilled = nullptr;
    SelectObjFn selectObj = nullptr;
    CountObjFn countObj = nullptr;
    AreaCenterFn areaCenter = nullptr;
    IntensityFn intensity = nullptr;
    GrayHistoFn grayHisto = nullptr;
    SmallestRectangle2Fn smallestRectangle2 = nullptr;
    SmallestRectangle1Fn smallestRectangle1 = nullptr;
    VectorAngleToRigidFn vectorAngleToRigid = nullptr;
    AffineTransImageFn affineTransImage = nullptr;
    AffineTransRegionFn affineTransRegion = nullptr;
    ClipRegionFn clipRegion = nullptr;
    CropRectangle1Fn cropRectangle1 = nullptr;
    MoveRegionFn moveRegion = nullptr;
    ZoomImageSizeFn zoomImageSize = nullptr;
    ZoomRegionFn zoomRegion = nullptr;
    SingleMeasureFn circularity = nullptr;
    SingleMeasureFn compactness = nullptr;
    SingleMeasureFn convexity = nullptr;
    SingleMeasureFn rectangularity = nullptr;
    EccentricityFn eccentricity = nullptr;
    MomentsCentralInvarFn momentsRegionCentralInvar = nullptr;
    Decompose3Fn decompose3 = nullptr;
    TransFromRgbFn transFromRgb = nullptr;
    EntropyGrayFn entropyGray = nullptr;
    CoocFeatureImageFn coocFeatureImage = nullptr;
    ClearObjFn clearObj = nullptr;
};

bool halconStatusOk(const Herror status)
{
    return status == H_MSG_OK || status == H_MSG_TRUE || status == H_MSG_FALSE;
}

bool halconObjectAllocated(const Hobject object)
{
    return object != 0 && object != NO_OBJECTS && object != EMPTY_REGION;
}

bool isFiniteRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) &&
           std::isfinite(rect.y()) &&
           std::isfinite(rect.width()) &&
           std::isfinite(rect.height());
}

RegisteredClassificationFeatureResult errorResult(const QString &status, const QString &message)
{
    RegisteredClassificationFeatureResult result;
    result.success = false;
    result.status = status;
    result.message = message;
    result.payload.insert(QStringLiteral("errorCode"), status);
    result.payload.insert(QStringLiteral("errorMessage"), message);
    return result;
}

cv::Mat toBgr8ForHalcon(const cv::Mat &image)
{
    cv::Mat source;
    if (image.depth() == CV_8U)
        source = image;
    else
        image.convertTo(source, CV_8U);

    cv::Mat bgr;
    if (source.channels() == 1)
        cv::cvtColor(source, bgr, cv::COLOR_GRAY2BGR);
    else if (source.channels() == 3)
        bgr = source;
    else if (source.channels() == 4)
        cv::cvtColor(source, bgr, cv::COLOR_BGRA2BGR);

    if (!bgr.empty() && !bgr.isContinuous())
        bgr = bgr.clone();
    return bgr;
}

QJsonArray vectorToJson(const QVector<double> &values)
{
    QJsonArray array;
    for (double value : values)
        array.append(value);
    return array;
}

QJsonArray stringListToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values)
        array.append(value);
    return array;
}

QJsonObject rectToJson(const QRect &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

QRect normalizedRoiToPixels(const QRectF &sourceRoi, const int width, const int height)
{
    if (width <= 0 || height <= 0 || !isFiniteRect(sourceRoi))
        return QRect();

    const QRectF roi = sourceRoi.normalized();
    const double left = qBound(0.0, roi.left(), 1.0);
    const double top = qBound(0.0, roi.top(), 1.0);
    const double right = qBound(0.0, roi.right(), 1.0);
    const double bottom = qBound(0.0, roi.bottom(), 1.0);
    if (right <= left || bottom <= top)
        return QRect();

    int x1 = qBound(0, static_cast<int>(std::floor(left * width)), width - 1);
    int y1 = qBound(0, static_cast<int>(std::floor(top * height)), height - 1);
    int x2 = qBound(0, static_cast<int>(std::ceil(right * width)) - 1, width - 1);
    int y2 = qBound(0, static_cast<int>(std::ceil(bottom * height)) - 1, height - 1);
    if (x2 < x1)
        std::swap(x1, x2);
    if (y2 < y1)
        std::swap(y1, y2);
    return QRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}

void appendUnique(QStringList *values, const QString &value)
{
    if (!values)
        return;
    const QString cleaned = value.trimmed();
    if (!cleaned.isEmpty() && !values->contains(cleaned))
        values->append(cleaned);
}

QStringList halconLibCandidates(const RegisteredClassificationFeatureConfig &config)
{
    QStringList candidates;
    appendUnique(&candidates, config.halconSoPath);
    for (const QString &candidate : config.halconSoPathCandidates)
        appendUnique(&candidates, candidate);
    for (const QString &candidate : HalconRuntimePaths::halconLibCandidates(config.halconSoPath))
        appendUnique(&candidates, candidate);
    return candidates;
}

QString resolveExistingHalconLib(const QStringList &candidates)
{
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

QString halconErrorText(HalconFeatureApi *api, const Herror status)
{
    char buffer[1024] = {0};
    if (api && api->getErrorText && halconStatusOk(api->getErrorText(static_cast<Hlong>(status), buffer))) {
        const QString message = QString::fromUtf8(buffer).trimmed();
        if (!message.isEmpty())
            return message;
    }
    return QStringLiteral("HALCON error code %1").arg(static_cast<qlonglong>(status));
}

template <typename Function>
bool resolveRequired(void *handle, Function &target, const char *symbolName, QString &errorMessage)
{
    dlerror();
    void *symbol = dlsym(handle, symbolName);
    const char *symbolError = dlerror();
    if (symbolError != nullptr || symbol == nullptr) {
        errorMessage = QStringLiteral("Missing HALCON symbol %1: %2")
                .arg(QString::fromLatin1(symbolName),
                     symbolError ? QString::fromLocal8Bit(symbolError) : QStringLiteral("not found"));
        return false;
    }
    target = reinterpret_cast<Function>(symbol);
    return true;
}

class HalconLibrary
{
public:
    ~HalconLibrary()
    {
        if (m_handle)
            dlclose(m_handle);
    }

    bool load(const QString &path, QString &errorMessage, bool &symbolMissing)
    {
        symbolMissing = false;
        const QByteArray encodedPath = path.toLocal8Bit();
        m_handle = dlopen(encodedPath.constData(), RTLD_NOW | RTLD_LOCAL);
        if (!m_handle) {
            const char *loadError = dlerror();
            errorMessage = loadError ? QString::fromLocal8Bit(loadError)
                                     : QStringLiteral("dlopen returned a null handle.");
            return false;
        }

        if (!resolveRequired(m_handle, api.setUtf8, "SetHcInterfaceStringEncodingIsUtf8", errorMessage) ||
            !resolveRequired(m_handle, api.getErrorText, "get_error_text", errorMessage) ||
            !resolveRequired(m_handle, api.createTuple, "F_create_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.setDouble, "F_set_d", errorMessage) ||
            !resolveRequired(m_handle, api.setInt, "F_set_i", errorMessage) ||
            !resolveRequired(m_handle, api.setString, "F_set_s", errorMessage) ||
            !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
            !resolveRequired(m_handle, api.getInt, "F_get_i", errorMessage) ||
            !resolveRequired(m_handle, api.genImageInterleaved, "gen_image_interleaved", errorMessage) ||
            !resolveRequired(m_handle, api.rgb1ToGray, "rgb1_to_gray", errorMessage) ||
            !resolveRequired(m_handle, api.genRectangle1, "gen_rectangle1", errorMessage) ||
            !resolveRequired(m_handle, api.reduceDomain, "reduce_domain", errorMessage) ||
            !resolveRequired(m_handle, api.gaussFilter, "T_gauss_filter", errorMessage) ||
            !resolveRequired(m_handle, api.binaryThreshold, "T_binary_threshold", errorMessage) ||
            !resolveRequired(m_handle, api.openingCircle, "T_opening_circle", errorMessage) ||
            !resolveRequired(m_handle, api.closingCircle, "T_closing_circle", errorMessage) ||
            !resolveRequired(m_handle, api.erosionCircle, "T_erosion_circle", errorMessage) ||
            !resolveRequired(m_handle, api.fillUp, "T_fill_up", errorMessage) ||
            !resolveRequired(m_handle, api.connection, "T_connection", errorMessage) ||
            !resolveRequired(m_handle, api.intersection, "T_intersection", errorMessage) ||
            !resolveRequired(m_handle, api.difference, "T_difference", errorMessage) ||
            !resolveRequired(m_handle, api.genRegionPolygonFilled, "T_gen_region_polygon_filled", errorMessage) ||
            !resolveRequired(m_handle, api.selectObj, "T_select_obj", errorMessage) ||
            !resolveRequired(m_handle, api.countObj, "T_count_obj", errorMessage) ||
            !resolveRequired(m_handle, api.areaCenter, "T_area_center", errorMessage) ||
            !resolveRequired(m_handle, api.intensity, "T_intensity", errorMessage) ||
            !resolveRequired(m_handle, api.grayHisto, "T_gray_histo", errorMessage) ||
            !resolveRequired(m_handle, api.smallestRectangle2, "T_smallest_rectangle2", errorMessage) ||
            !resolveRequired(m_handle, api.smallestRectangle1, "T_smallest_rectangle1", errorMessage) ||
            !resolveRequired(m_handle, api.vectorAngleToRigid, "T_vector_angle_to_rigid", errorMessage) ||
            !resolveRequired(m_handle, api.affineTransImage, "T_affine_trans_image", errorMessage) ||
            !resolveRequired(m_handle, api.affineTransRegion, "T_affine_trans_region", errorMessage) ||
            !resolveRequired(m_handle, api.clipRegion, "T_clip_region", errorMessage) ||
            !resolveRequired(m_handle, api.cropRectangle1, "T_crop_rectangle1", errorMessage) ||
            !resolveRequired(m_handle, api.moveRegion, "T_move_region", errorMessage) ||
            !resolveRequired(m_handle, api.zoomImageSize, "T_zoom_image_size", errorMessage) ||
            !resolveRequired(m_handle, api.zoomRegion, "T_zoom_region", errorMessage) ||
            !resolveRequired(m_handle, api.circularity, "T_circularity", errorMessage) ||
            !resolveRequired(m_handle, api.compactness, "T_compactness", errorMessage) ||
            !resolveRequired(m_handle, api.convexity, "T_convexity", errorMessage) ||
            !resolveRequired(m_handle, api.rectangularity, "T_rectangularity", errorMessage) ||
            !resolveRequired(m_handle, api.eccentricity, "T_eccentricity", errorMessage) ||
            !resolveRequired(m_handle, api.momentsRegionCentralInvar, "T_moments_region_central_invar", errorMessage) ||
            !resolveRequired(m_handle, api.decompose3, "T_decompose3", errorMessage) ||
            !resolveRequired(m_handle, api.transFromRgb, "T_trans_from_rgb", errorMessage) ||
            !resolveRequired(m_handle, api.entropyGray, "T_entropy_gray", errorMessage) ||
            !resolveRequired(m_handle, api.coocFeatureImage, "T_cooc_feature_image", errorMessage) ||
            !resolveRequired(m_handle, api.clearObj, "clear_obj", errorMessage)) {
            symbolMissing = true;
            dlclose(m_handle);
            m_handle = nullptr;
            return false;
        }

        api.setUtf8(1);
        return true;
    }

    HalconFeatureApi api;

private:
    void *m_handle = nullptr;
};

class HalconTuple
{
public:
    explicit HalconTuple(HalconFeatureApi *api = nullptr)
        : m_api(api)
    {
    }

    HalconTuple(const HalconTuple &) = delete;
    HalconTuple &operator=(const HalconTuple &) = delete;

    HalconTuple(HalconTuple &&other) noexcept
        : m_api(other.m_api)
        , m_tuple(other.m_tuple)
    {
        other.m_api = nullptr;
        other.m_tuple = HTUPLE_INITIALIZER;
    }

    HalconTuple &operator=(HalconTuple &&other) noexcept
    {
        if (this == &other)
            return *this;
        destroy();
        m_api = other.m_api;
        m_tuple = other.m_tuple;
        other.m_api = nullptr;
        other.m_tuple = HTUPLE_INITIALIZER;
        return *this;
    }

    ~HalconTuple()
    {
        destroy();
    }

    void create(const int size)
    {
        destroy();
        if (m_api && m_api->createTuple)
            m_api->createTuple(&m_tuple, size);
    }

    void setDouble(const int index, const double value)
    {
        if (m_api && m_api->setDouble)
            m_api->setDouble(&m_tuple, value, index);
    }

    void setInt(const int index, const Hlong value)
    {
        if (m_api && m_api->setInt)
            m_api->setInt(&m_tuple, value, index);
    }

    void setString(const int index, const QByteArray &value)
    {
        if (m_api && m_api->setString)
            m_api->setString(&m_tuple, value.constData(), index);
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

    double doubleAt(const int index) const
    {
        return m_api && m_api->getDouble && index >= 0 && index < size()
                ? m_api->getDouble(&m_tuple, index)
                : 0.0;
    }

    Hlong intAt(const int index) const
    {
        return m_api && m_api->getInt && index >= 0 && index < size()
                ? m_api->getInt(&m_tuple, index)
                : 0;
    }

    void destroy()
    {
        if (m_api && m_api->destroyTuple && (m_tuple.num > 0 || m_tuple.capacity > 0))
            m_api->destroyTuple(&m_tuple);
        m_tuple = HTUPLE_INITIALIZER;
    }

private:
    HalconFeatureApi *m_api = nullptr;
    Htuple m_tuple = HTUPLE_INITIALIZER;
};

class HalconObjectGuard
{
public:
    explicit HalconObjectGuard(HalconFeatureApi *api = nullptr)
        : m_api(api)
    {
    }

    ~HalconObjectGuard()
    {
        reset();
    }

    Hobject *ptr()
    {
        return &m_object;
    }

    const Hobject &value() const
    {
        return m_object;
    }

    void reset()
    {
        if (m_api && m_api->clearObj && halconObjectAllocated(m_object))
            m_api->clearObj(m_object);
        m_object = NO_OBJECTS;
    }

private:
    HalconFeatureApi *m_api = nullptr;
    Hobject m_object = NO_OBJECTS;
};

void checkStatus(HalconFeatureApi *api, const Herror status, const QString &stage)
{
    if (!halconStatusOk(status)) {
        throw std::pair<QString, QString>(
                QStringLiteral("halcon_error"),
                QStringLiteral("%1: %2").arg(stage, halconErrorText(api, status)));
    }
}

QVector<double> compressHistogram(const HalconTuple &relativeHistogram)
{
    if (relativeHistogram.size() != kGrayHistogramBins) {
        throw std::pair<QString, QString>(
                QStringLiteral("invalid_feature_value"),
                QStringLiteral("HALCON gray histogram must contain exactly %1 bins.")
                .arg(kGrayHistogramBins));
    }
    QVector<double> compressed(kCompressedHistogramBins, 0.0);
    for (int index = 0; index < relativeHistogram.size(); ++index) {
        const double value = relativeHistogram.doubleAt(index);
        if (!std::isfinite(value) || value < 0.0) {
            throw std::pair<QString, QString>(
                    QStringLiteral("invalid_feature_value"),
                    QStringLiteral("HALCON gray histogram contains an invalid value at bin %1.")
                    .arg(index));
        }
        const int bucket = qBound(0, index / kGrayHistogramGroupSize, kCompressedHistogramBins - 1);
        compressed[bucket] += value;
    }
    return compressed;
}

HalconTuple tupleDouble(HalconFeatureApi *api, const double value)
{
    HalconTuple tuple(api);
    tuple.create(1);
    tuple.setDouble(0, value);
    return tuple;
}

HalconTuple tupleInt(HalconFeatureApi *api, const Hlong value)
{
    HalconTuple tuple(api);
    tuple.create(1);
    tuple.setInt(0, value);
    return tuple;
}

HalconTuple tupleString(HalconFeatureApi *api, const char *value)
{
    HalconTuple tuple(api);
    tuple.create(1);
    tuple.setString(0, QByteArray(value));
    return tuple;
}

double requiredFiniteDouble(const HalconTuple &tuple, const QString &stage)
{
    if (tuple.size() <= 0) {
        throw std::pair<QString, QString>(
                QStringLiteral("invalid_feature_value"),
                QStringLiteral("HALCON returned no numeric value for %1.").arg(stage));
    }
    const double value = tuple.doubleAt(0);
    if (!std::isfinite(value)) {
        throw std::pair<QString, QString>(
                QStringLiteral("invalid_feature_value"),
                QStringLiteral("HALCON returned a non-finite value for %1.").arg(stage));
    }
    return value;
}

double bounded01(const double value)
{
    return qBound(0.0, value, 1.0);
}

double boundedSigned(const double value)
{
    return 0.5 + 0.5 * value / (1.0 + std::abs(value));
}

PositionCorrectionHalconRegionApi positionCorrectionRegionApi(
        const HalconFeatureApi &api)
{
    PositionCorrectionHalconRegionApi transformApi;
    transformApi.createTuple = api.createTuple;
    transformApi.setDouble = api.setDouble;
    transformApi.setString = api.setString;
    transformApi.destroyTuple = api.destroyTuple;
    transformApi.getDouble = api.getDouble;
    transformApi.affineTransRegion = api.affineTransRegion;
    transformApi.clipRegion = api.clipRegion;
    transformApi.areaCenter = api.areaCenter;
    return transformApi;
}

QVector<double> occupancyVector(HalconFeatureApi *api, const Hobject region)
{
    QVector<double> occupancy;
    occupancy.reserve(kOccupancyGridSize * kOccupancyGridSize);
    const double cellSize = static_cast<double>(kCanonicalSize) / kOccupancyGridSize;
    for (int gridRow = 0; gridRow < kOccupancyGridSize; ++gridRow) {
        for (int gridColumn = 0; gridColumn < kOccupancyGridSize; ++gridColumn) {
            HalconObjectGuard cell(api);
            HalconObjectGuard overlap(api);
            HalconTuple area(api);
            HalconTuple row(api);
            HalconTuple column(api);
            const double row1 = gridRow * cellSize;
            const double column1 = gridColumn * cellSize;
            const double row2 = (gridRow + 1) * cellSize - 1.0;
            const double column2 = (gridColumn + 1) * cellSize - 1.0;
            checkStatus(api, api->genRectangle1(cell.ptr(), row1, column1, row2, column2),
                        QStringLiteral("gen_rectangle1(occupancy)"));
            checkStatus(api, api->intersection(region, cell.value(), overlap.ptr()),
                        QStringLiteral("intersection(occupancy)"));
            checkStatus(api, api->areaCenter(overlap.value(), area.ptr(), row.ptr(), column.ptr()),
                        QStringLiteral("area_center(occupancy)"));
            occupancy.append(bounded01(
                    requiredFiniteDouble(area, QStringLiteral("occupancy area"))
                    / (cellSize * cellSize)));
        }
    }
    return occupancy;
}

QVector<qint64> quantizedOccupancy(const QVector<double> &occupancy)
{
    QVector<qint64> quantized;
    quantized.reserve(occupancy.size());
    for (double value : occupancy)
        quantized.append(qRound64(value * 1000000.0));
    return quantized;
}

bool lexicographicallyLess(const QVector<qint64> &left, const QVector<qint64> &right)
{
    return std::lexicographical_compare(left.cbegin(), left.cend(), right.cbegin(), right.cend());
}

void buildCanonicalCandidate(HalconFeatureApi *api,
                             const Hobject image,
                             const Hobject region,
                             const double sourceRow,
                             const double sourceColumn,
                             const double sourceAngle,
                             const double destinationAngle,
                             const int imageWidth,
                             const int imageHeight,
                             const double paddingRatio,
                             HalconObjectGuard *canonicalImage,
                             HalconObjectGuard *canonicalRegion)
{
    HalconTuple sourceRowTuple = tupleDouble(api, sourceRow);
    HalconTuple sourceColumnTuple = tupleDouble(api, sourceColumn);
    HalconTuple sourceAngleTuple = tupleDouble(api, sourceAngle);
    HalconTuple destinationRowTuple = tupleDouble(api, sourceRow);
    HalconTuple destinationColumnTuple = tupleDouble(api, sourceColumn);
    HalconTuple destinationAngleTuple = tupleDouble(api, destinationAngle);
    HalconTuple homography(api);
    checkStatus(api, api->vectorAngleToRigid(sourceRowTuple.value(), sourceColumnTuple.value(),
                                              sourceAngleTuple.value(), destinationRowTuple.value(),
                                              destinationColumnTuple.value(), destinationAngleTuple.value(),
                                              homography.ptr()),
                QStringLiteral("vector_angle_to_rigid"));

    HalconObjectGuard transformedImage(api);
    HalconObjectGuard transformedRegion(api);
    HalconTuple interpolation = tupleString(api, "bilinear");
    HalconTuple adaptImageSize = tupleString(api, "false");
    HalconTuple regionInterpolation = tupleString(api, "nearest_neighbor");
    checkStatus(api, api->affineTransImage(image, transformedImage.ptr(), homography.value(),
                                           interpolation.value(), adaptImageSize.value()),
                QStringLiteral("affine_trans_image"));
    checkStatus(api, api->affineTransRegion(region, transformedRegion.ptr(), homography.value(),
                                            regionInterpolation.value()),
                QStringLiteral("affine_trans_region"));

    HalconTuple row1(api);
    HalconTuple column1(api);
    HalconTuple row2(api);
    HalconTuple column2(api);
    checkStatus(api, api->smallestRectangle1(transformedRegion.value(), row1.ptr(), column1.ptr(),
                                              row2.ptr(), column2.ptr()),
                QStringLiteral("smallest_rectangle1"));
    const double rawRow1 = requiredFiniteDouble(row1, QStringLiteral("canonical row1"));
    const double rawColumn1 = requiredFiniteDouble(column1, QStringLiteral("canonical column1"));
    const double rawRow2 = requiredFiniteDouble(row2, QStringLiteral("canonical row2"));
    const double rawColumn2 = requiredFiniteDouble(column2, QStringLiteral("canonical column2"));
    const double paddingRows = qMax(1.0, (rawRow2 - rawRow1 + 1.0) * paddingRatio);
    const double paddingColumns = qMax(1.0, (rawColumn2 - rawColumn1 + 1.0) * paddingRatio);
    const double cropRow1 = qBound(0.0, std::floor(rawRow1 - paddingRows),
                                   static_cast<double>(imageHeight - 1));
    const double cropColumn1 = qBound(0.0, std::floor(rawColumn1 - paddingColumns),
                                      static_cast<double>(imageWidth - 1));
    const double cropRow2 = qBound(cropRow1, std::ceil(rawRow2 + paddingRows),
                                   static_cast<double>(imageHeight - 1));
    const double cropColumn2 = qBound(cropColumn1, std::ceil(rawColumn2 + paddingColumns),
                                      static_cast<double>(imageWidth - 1));

    HalconTuple cropRow1Tuple = tupleDouble(api, cropRow1);
    HalconTuple cropColumn1Tuple = tupleDouble(api, cropColumn1);
    HalconTuple cropRow2Tuple = tupleDouble(api, cropRow2);
    HalconTuple cropColumn2Tuple = tupleDouble(api, cropColumn2);
    HalconObjectGuard croppedImage(api);
    HalconObjectGuard movedRegion(api);
    checkStatus(api, api->cropRectangle1(transformedImage.value(), croppedImage.ptr(),
                                          cropRow1Tuple.value(), cropColumn1Tuple.value(),
                                          cropRow2Tuple.value(), cropColumn2Tuple.value()),
                QStringLiteral("crop_rectangle1"));
    HalconTuple moveRows = tupleDouble(api, -cropRow1);
    HalconTuple moveColumns = tupleDouble(api, -cropColumn1);
    checkStatus(api, api->moveRegion(transformedRegion.value(), movedRegion.ptr(),
                                     moveRows.value(), moveColumns.value()),
                QStringLiteral("move_region"));

    HalconTuple canonicalWidth = tupleInt(api, kCanonicalSize);
    HalconTuple canonicalHeight = tupleInt(api, kCanonicalSize);
    checkStatus(api, api->zoomImageSize(croppedImage.value(), canonicalImage->ptr(),
                                        canonicalWidth.value(), canonicalHeight.value(),
                                        interpolation.value()),
                QStringLiteral("zoom_image_size"));
    const double cropWidth = cropColumn2 - cropColumn1 + 1.0;
    const double cropHeight = cropRow2 - cropRow1 + 1.0;
    HalconTuple scaleWidth = tupleDouble(api, kCanonicalSize / cropWidth);
    HalconTuple scaleHeight = tupleDouble(api, kCanonicalSize / cropHeight);
    checkStatus(api, api->zoomRegion(movedRegion.value(), canonicalRegion->ptr(),
                                     scaleWidth.value(), scaleHeight.value()),
                QStringLiteral("zoom_region"));
}

void appendWeightedGroups(QVector<double> *feature)
{
    const QVector<int> dimensions = registeredClassificationFeatureGroupDimensions();
    const QVector<double> weights = registeredClassificationFeatureGroupWeights();
    int offset = 0;
    for (int group = 0; group < dimensions.size(); ++group) {
        const double factor = std::sqrt(weights.at(group) / dimensions.at(group));
        for (int index = 0; index < dimensions.at(group); ++index)
            (*feature)[offset + index] *= factor;
        offset += dimensions.at(group);
    }
}

} // namespace

RegisteredClassificationFeatureResult RegisteredClassificationFeatureExtractor::extract(
        const cv::Mat &image,
        const RegisteredClassificationFeatureRegion &region,
        const RegisteredClassificationFeatureConfig &config) const
{
    if (image.empty())
        return errorResult(QStringLiteral("image_empty"), QStringLiteral("Feature extraction input image is empty."));

    const cv::Mat halconFrame = toBgr8ForHalcon(image);
    if (halconFrame.empty()) {
        return errorResult(QStringLiteral("unsupported_image_type"),
                           QStringLiteral("Only images convertible to continuous CV_8UC3 are supported."));
    }

    QRectF normalizedBounds;
    if (region.type == QStringLiteral("full")) {
        normalizedBounds = QRectF(0.0, 0.0, 1.0, 1.0);
    } else if (region.type == QStringLiteral("rectangle")) {
        normalizedBounds = region.rectNormalized;
    } else if (region.type == QStringLiteral("polygon") && region.polygonNormalized.size() >= 3) {
        double left = 1.0;
        double top = 1.0;
        double right = 0.0;
        double bottom = 0.0;
        for (const QPointF &point : region.polygonNormalized) {
            if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
                return errorResult(QStringLiteral("invalid_roi"), QStringLiteral("Polygon ROI contains a non-finite point."));
            left = qMin(left, point.x());
            top = qMin(top, point.y());
            right = qMax(right, point.x());
            bottom = qMax(bottom, point.y());
        }
        normalizedBounds = QRectF(QPointF(left, top), QPointF(right, bottom));
    } else {
        return errorResult(QStringLiteral("invalid_roi"),
                           QStringLiteral("Feature region must be full, rectangle, or a polygon with at least three points."));
    }

    QRect roiPixels = normalizedRoiToPixels(normalizedBounds, halconFrame.cols, halconFrame.rows);
    if (roiPixels.width() < kMinRoiPixelSize || roiPixels.height() < kMinRoiPixelSize) {
        return errorResult(QStringLiteral("invalid_roi"),
                           QStringLiteral("Normalized ROI must resolve to at least 2x2 pixels."));
    }

    HalconRuntimePaths::initializeHalconEnvironment();
    const QStringList halconCandidates = halconLibCandidates(config);
    const QString halconLibPath = resolveExistingHalconLib(halconCandidates);
    if (halconLibPath.isEmpty()) {
        RegisteredClassificationFeatureResult result = errorResult(
                    QStringLiteral("halcon_so_not_found"),
                    QStringLiteral("HALCON runtime file not found. Tried: %1")
                    .arg(HalconRuntimePaths::formatTriedPaths(halconCandidates)));
        result.roiPixels = roiPixels;
        return result;
    }

    HalconLibrary library;
    QString loadMessage;
    bool symbolMissing = false;
    if (!library.load(halconLibPath, loadMessage, symbolMissing)) {
        RegisteredClassificationFeatureResult result = errorResult(
                    symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                  : QStringLiteral("halcon_load_failed"),
                    loadMessage);
        result.roiPixels = roiPixels;
        result.payload.insert(QStringLiteral("halconSoPath"), halconLibPath);
        return result;
    }

    HalconFeatureApi *api = &library.api;
    RegisteredClassificationFeatureResult result;
    result.featureNames = registeredClassificationFeatureNamesV2();
    result.roiPixels = roiPixels;
    result.payload.insert(QStringLiteral("roiPixels"), rectToJson(roiPixels));
    result.payload.insert(QStringLiteral("referenceRoiPixels"),
                          rectToJson(roiPixels));
    result.payload.insert(QStringLiteral("halconSoPath"), halconLibPath);
    result.payload.insert(QStringLiteral("featureVersion"), registeredClassificationFeatureVersionV2());
    result.payload.insert(QStringLiteral("featureNames"), stringListToJson(result.featureNames));

    HalconObjectGuard halconImage(api);
    HalconObjectGuard grayImage(api);
    HalconObjectGuard roiRegion(api);
    HalconObjectGuard transformedRoiRegion(api);
    HalconObjectGuard clippedRoiRegion(api);
    HalconObjectGuard roiGray(api);
    HalconObjectGuard smoothedGray(api);
    HalconObjectGuard innerRoi(api);
    HalconObjectGuard borderBand(api);
    HalconObjectGuard lightConnected(api);
    HalconObjectGuard darkConnected(api);
    HalconObjectGuard foreground(api);

    try {
        checkStatus(api, api->genImageInterleaved(halconImage.ptr(),
                                                  reinterpret_cast<Hlong>(halconFrame.data),
                                                  "bgr", halconFrame.cols, halconFrame.rows, 0,
                                                  "byte", 0, 0, 0, 0, 8, 0),
                    QStringLiteral("gen_image_interleaved"));
        checkStatus(api, api->rgb1ToGray(halconImage.value(), grayImage.ptr()),
                    QStringLiteral("rgb1_to_gray"));

        if (region.type == QStringLiteral("polygon")) {
            HalconTuple rows(api);
            HalconTuple columns(api);
            rows.create(region.polygonNormalized.size());
            columns.create(region.polygonNormalized.size());
            for (int index = 0; index < region.polygonNormalized.size(); ++index) {
                rows.setDouble(index, qBound(0.0, region.polygonNormalized.at(index).y(), 1.0)
                               * (halconFrame.rows - 1));
                columns.setDouble(index, qBound(0.0, region.polygonNormalized.at(index).x(), 1.0)
                                  * (halconFrame.cols - 1));
            }
            checkStatus(api, api->genRegionPolygonFilled(roiRegion.ptr(), rows.value(), columns.value()),
                        QStringLiteral("gen_region_polygon_filled"));
        } else {
            checkStatus(api, api->genRectangle1(roiRegion.ptr(), roiPixels.top(), roiPixels.left(),
                                                 roiPixels.bottom(), roiPixels.right()),
                        QStringLiteral("gen_rectangle1(roi)"));
        }

        Hobject effectiveRoiRegion = roiRegion.value();
        if (!config.referenceToRunHomMat2D.isEmpty()) {
            result.payload.insert(
                        QStringLiteral("positionCorrectionOperation"),
                        QStringLiteral("affine_trans_region+clip_region"));
            const PositionCorrectionHalconTransformResult transformed =
                    PositionCorrectionHalconTransform::transformAndClipRegion(
                        positionCorrectionRegionApi(*api),
                        roiRegion.value(),
                        transformedRoiRegion.ptr(),
                        clippedRoiRegion.ptr(),
                        config.referenceToRunHomMat2D,
                        halconFrame.cols,
                        halconFrame.rows);
            if (!transformed.success) {
                if (transformed.halconStatus != H_MSG_OK) {
                    checkStatus(api,
                                transformed.halconStatus,
                                QStringLiteral("position_correction.%1")
                                .arg(transformed.operation));
                }
                throw std::pair<QString, QString>(
                        transformed.status,
                        QStringLiteral("Position-corrected classification ROI is invalid (%1).")
                        .arg(transformed.operation));
            }
            if (transformed.area <= 0.0) {
                throw std::pair<QString, QString>(
                        QStringLiteral("corrected_roi_out_of_image"),
                        QStringLiteral("Position-corrected classification ROI is outside the image."));
            }
            effectiveRoiRegion = clippedRoiRegion.value();

            HalconTuple correctedRow1(api);
            HalconTuple correctedColumn1(api);
            HalconTuple correctedRow2(api);
            HalconTuple correctedColumn2(api);
            checkStatus(api,
                        api->smallestRectangle1(
                            effectiveRoiRegion,
                            correctedRow1.ptr(),
                            correctedColumn1.ptr(),
                            correctedRow2.ptr(),
                            correctedColumn2.ptr()),
                        QStringLiteral("smallest_rectangle1(corrected_roi)"));
            const int left = qBound(
                        0,
                        static_cast<int>(std::floor(requiredFiniteDouble(
                                                       correctedColumn1,
                                                       QStringLiteral("corrected ROI column1")))),
                        halconFrame.cols - 1);
            const int top = qBound(
                        0,
                        static_cast<int>(std::floor(requiredFiniteDouble(
                                                       correctedRow1,
                                                       QStringLiteral("corrected ROI row1")))),
                        halconFrame.rows - 1);
            const int right = qBound(
                        left,
                        static_cast<int>(std::ceil(requiredFiniteDouble(
                                                      correctedColumn2,
                                                      QStringLiteral("corrected ROI column2")))),
                        halconFrame.cols - 1);
            const int bottom = qBound(
                        top,
                        static_cast<int>(std::ceil(requiredFiniteDouble(
                                                      correctedRow2,
                                                      QStringLiteral("corrected ROI row2")))),
                        halconFrame.rows - 1);
            roiPixels = QRect(left, top, right - left + 1, bottom - top + 1);
            result.roiPixels = roiPixels;
            result.payload.insert(QStringLiteral("roiPixels"), rectToJson(roiPixels));
            result.payload.insert(QStringLiteral("correctedRoiArea"), transformed.area);
        }

        HalconTuple roiAreaTuple(api);
        HalconTuple roiCenterRowTuple(api);
        HalconTuple roiCenterColumnTuple(api);
        checkStatus(api, api->areaCenter(effectiveRoiRegion, roiAreaTuple.ptr(),
                                         roiCenterRowTuple.ptr(), roiCenterColumnTuple.ptr()),
                    QStringLiteral("area_center(roi)"));
        const double roiArea = requiredFiniteDouble(roiAreaTuple, QStringLiteral("ROI area"));
        const double roiCenterRow = requiredFiniteDouble(
                    roiCenterRowTuple, QStringLiteral("ROI center row"));
        const double roiCenterColumn = requiredFiniteDouble(
                    roiCenterColumnTuple, QStringLiteral("ROI center column"));
        if (roiArea <= 0.0)
            return errorResult(QStringLiteral("invalid_roi"), QStringLiteral("Feature ROI has no pixels."));

        checkStatus(api, api->reduceDomain(grayImage.value(), effectiveRoiRegion, roiGray.ptr()),
                    QStringLiteral("reduce_domain"));
        HalconTuple gaussSize = tupleInt(api, 3);
        checkStatus(api, api->gaussFilter(roiGray.value(), smoothedGray.ptr(), gaussSize.value()),
                    QStringLiteral("gauss_filter"));

        const QJsonObject segmentation = registeredClassificationSegmentationContractV2();
        const double morphologyRadius = qMax(
                    segmentation.value(QStringLiteral("morphologyRadiusMinimum")).toDouble(),
                    segmentation.value(QStringLiteral("morphologyRadiusRoiScale")).toDouble()
                    * qMin(roiPixels.width(), roiPixels.height()));
        const double borderWidth = qMax(
                    1.0,
                    static_cast<double>(qRound(segmentation.value(QStringLiteral("borderBandRatio")).toDouble()
                                               * qMin(roiPixels.width(), roiPixels.height()))));
        HalconTuple borderRadius = tupleDouble(api, borderWidth);
        checkStatus(api, api->erosionCircle(effectiveRoiRegion, innerRoi.ptr(), borderRadius.value()),
                    QStringLiteral("erosion_circle(roi_border)"));
        checkStatus(api, api->difference(effectiveRoiRegion, innerRoi.value(), borderBand.ptr()),
                    QStringLiteral("difference(roi_border)"));

        const QString thresholdMethod = segmentation.value(QStringLiteral("thresholdMethod")).toString();
        const QJsonArray thresholdPolarities = segmentation.value(
                    QStringLiteral("thresholdPolarities")).toArray();
        if (thresholdMethod.isEmpty() || thresholdPolarities.size() != 2) {
            throw std::pair<QString, QString>(QStringLiteral("invalid_feature_value"),
                                              QStringLiteral("V2 segmentation contract is invalid."));
        }
        const auto segmentPolarity = [&](const QString &polarity, HalconObjectGuard *connected) {
            HalconObjectGuard thresholded(api);
            HalconObjectGuard opened(api);
            HalconObjectGuard closed(api);
            HalconObjectGuard filled(api);
            const QByteArray methodName = thresholdMethod.toLatin1();
            const QByteArray polarityName = polarity.toLatin1();
            HalconTuple method = tupleString(api, methodName.constData());
            HalconTuple lightDark = tupleString(api, polarityName.constData());
            HalconTuple usedThreshold(api);
            HalconTuple radius = tupleDouble(api, morphologyRadius);
            checkStatus(api, api->binaryThreshold(smoothedGray.value(), thresholded.ptr(),
                                                   method.value(), lightDark.value(), usedThreshold.ptr()),
                        QStringLiteral("binary_threshold(%1)").arg(polarity));
            checkStatus(api, api->openingCircle(thresholded.value(), opened.ptr(), radius.value()),
                        QStringLiteral("opening_circle"));
            checkStatus(api, api->closingCircle(opened.value(), closed.ptr(), radius.value()),
                        QStringLiteral("closing_circle"));
            checkStatus(api, api->fillUp(closed.value(), filled.ptr()), QStringLiteral("fill_up"));
            checkStatus(api, api->connection(filled.value(), connected->ptr()), QStringLiteral("connection"));
        };
        const QString lightPolarity = thresholdPolarities.at(0).toString();
        const QString darkPolarity = thresholdPolarities.at(1).toString();
        segmentPolarity(lightPolarity, &lightConnected);
        segmentPolarity(darkPolarity, &darkConnected);

        struct Candidate {
            bool valid = false;
            QString polarity;
            int index = 0;
            double areaRatio = 0.0;
            double objectScore = 0.0;
            double centerDistance = 0.0;
            double borderTouchRatio = 0.0;
        } best;

        const auto scoreCandidates = [&](const Hobject connected, const QString &polarity) {
            HalconTuple count(api);
            checkStatus(api, api->countObj(connected, count.ptr()), QStringLiteral("count_obj"));
            if (count.size() <= 0) {
                throw std::pair<QString, QString>(
                        QStringLiteral("invalid_feature_value"),
                        QStringLiteral("HALCON count_obj returned no object count."));
            }
            const Hlong objectCountValue = count.intAt(0);
            if (objectCountValue < 0
                    || objectCountValue > static_cast<Hlong>(std::numeric_limits<int>::max())) {
                throw std::pair<QString, QString>(
                        QStringLiteral("invalid_feature_value"),
                        QStringLiteral("HALCON count_obj returned an invalid object count."));
            }
            const int objectCount = static_cast<int>(objectCountValue);
            for (int index = 1; index <= objectCount; ++index) {
                HalconObjectGuard candidate(api);
                HalconObjectGuard borderOverlap(api);
                HalconTuple indexTuple = tupleInt(api, index);
                HalconTuple area(api);
                HalconTuple row(api);
                HalconTuple column(api);
                HalconTuple borderArea(api);
                HalconTuple borderRow(api);
                HalconTuple borderColumn(api);
                checkStatus(api, api->selectObj(connected, candidate.ptr(), indexTuple.value()),
                            QStringLiteral("select_obj"));
                checkStatus(api, api->areaCenter(candidate.value(), area.ptr(), row.ptr(), column.ptr()),
                            QStringLiteral("area_center(candidate)"));
                const double candidateArea = requiredFiniteDouble(
                            area, QStringLiteral("candidate area"));
                const double areaRatio = candidateArea / roiArea;
                const double minAreaRatio = segmentation.value(QStringLiteral("candidateAreaRatioMin")).toDouble();
                const double maxAreaRatio = segmentation.value(QStringLiteral("candidateAreaRatioMax")).toDouble();
                if (areaRatio < minAreaRatio || areaRatio > maxAreaRatio)
                    continue;

                checkStatus(api, api->intersection(candidate.value(), borderBand.value(), borderOverlap.ptr()),
                            QStringLiteral("intersection(border)"));
                checkStatus(api, api->areaCenter(borderOverlap.value(), borderArea.ptr(),
                                                  borderRow.ptr(), borderColumn.ptr()),
                            QStringLiteral("area_center(border)"));
                const double borderTouchRatio = candidateArea > 0.0
                        ? requiredFiniteDouble(borderArea, QStringLiteral("border overlap area"))
                          / candidateArea
                        : 1.0;
                const double dx = (requiredFiniteDouble(
                                       column, QStringLiteral("candidate center column"))
                                   - roiCenterColumn)
                        / qMax(1.0, roiPixels.width() * 0.5);
                const double dy = (requiredFiniteDouble(
                                       row, QStringLiteral("candidate center row"))
                                   - roiCenterRow)
                        / qMax(1.0, roiPixels.height() * 0.5);
                const double normalizedCenterDistance = bounded01(std::hypot(dx, dy) / std::sqrt(2.0));
                const double centerScore = 1.0 - normalizedCenterDistance;
                const double borderScore = 1.0 - qBound(
                            0.0,
                            borderTouchRatio / segmentation.value(QStringLiteral("borderTouchDivisor")).toDouble(),
                            1.0);
                const double areaScore = qMin(areaRatio / 0.20, 1.0);
                const double objectScore =
                        segmentation.value(QStringLiteral("objectScoreCenterWeight")).toDouble() * centerScore
                        + segmentation.value(QStringLiteral("objectScoreBorderWeight")).toDouble() * borderScore
                        + segmentation.value(QStringLiteral("objectScoreAreaWeight")).toDouble() * areaScore;
                if (!best.valid || objectScore > best.objectScore) {
                    best.valid = true;
                    best.polarity = polarity;
                    best.index = index;
                    best.areaRatio = areaRatio;
                    best.objectScore = objectScore;
                    best.centerDistance = normalizedCenterDistance;
                    best.borderTouchRatio = borderTouchRatio;
                }
            }
        };
        scoreCandidates(lightConnected.value(), lightPolarity);
        scoreCandidates(darkConnected.value(), darkPolarity);
        if (!best.valid) {
            result = errorResult(QStringLiteral("foreground_not_found"),
                                 QStringLiteral("No foreground candidate passed the 2%-98% area gate."));
            result.roiPixels = roiPixels;
            return result;
        }

        HalconTuple bestIndex = tupleInt(api, best.index);
        const Hobject bestConnected = best.polarity == lightPolarity
                ? lightConnected.value() : darkConnected.value();
        checkStatus(api, api->selectObj(bestConnected, foreground.ptr(), bestIndex.value()),
                    QStringLiteral("select_obj(best_foreground)"));

        HalconTuple sourceRow(api);
        HalconTuple sourceColumn(api);
        HalconTuple sourcePhi(api);
        HalconTuple sourceLength1(api);
        HalconTuple sourceLength2(api);
        checkStatus(api, api->smallestRectangle2(foreground.value(), sourceRow.ptr(), sourceColumn.ptr(),
                                                  sourcePhi.ptr(), sourceLength1.ptr(), sourceLength2.ptr()),
                    QStringLiteral("smallest_rectangle2"));
        const double sourceCenterRow = requiredFiniteDouble(
                    sourceRow, QStringLiteral("foreground center row"));
        const double sourceCenterColumn = requiredFiniteDouble(
                    sourceColumn, QStringLiteral("foreground center column"));
        double phi = requiredFiniteDouble(sourcePhi, QStringLiteral("foreground orientation"));
        double length1 = requiredFiniteDouble(sourceLength1, QStringLiteral("foreground long axis"));
        double length2 = requiredFiniteDouble(sourceLength2, QStringLiteral("foreground short axis"));
        if (length2 > length1) {
            std::swap(length1, length2);
            phi += M_PI_2;
        }
        if (length1 <= 0.0 || length2 <= 0.0)
            throw std::pair<QString, QString>(QStringLiteral("invalid_feature_value"),
                                              QStringLiteral("Foreground rectangle has a zero axis."));

        const QJsonObject canonicalization = registeredClassificationCanonicalizationContractV2();
        if (canonicalization.value(QStringLiteral("width")).toInt() != kCanonicalSize
                || canonicalization.value(QStringLiteral("height")).toInt() != kCanonicalSize
                || canonicalization.value(QStringLiteral("occupancyOrientationGridRows")).toInt()
                   != kOccupancyGridSize
                || canonicalization.value(QStringLiteral("occupancyOrientationGridColumns")).toInt()
                   != kOccupancyGridSize) {
            throw std::pair<QString, QString>(QStringLiteral("invalid_feature_value"),
                                              QStringLiteral("V2 canonicalization contract is invalid."));
        }
        const double paddingRatio = canonicalization.value(QStringLiteral("paddingRatio")).toDouble();
        const double nearEqualThreshold = canonicalization.value(QStringLiteral("nearEqualAxisThreshold")).toDouble();
        QVector<double> destinationAngles = {0.0, M_PI};
        if (std::abs(length1 - length2) / length1 < nearEqualThreshold)
            destinationAngles = {0.0, M_PI_2, M_PI, 3.0 * M_PI_2};

        double selectedAngle = destinationAngles.first();
        QVector<qint64> selectedOrientation;
        for (double destinationAngle : destinationAngles) {
            HalconObjectGuard candidateImage(api);
            HalconObjectGuard candidateRegion(api);
            buildCanonicalCandidate(api, halconImage.value(), foreground.value(),
                                    sourceCenterRow, sourceCenterColumn, phi,
                                    destinationAngle, halconFrame.cols, halconFrame.rows,
                                    paddingRatio, &candidateImage, &candidateRegion);
            const QVector<qint64> orientation = quantizedOccupancy(
                        occupancyVector(api, candidateRegion.value()));
            if (selectedOrientation.isEmpty() || lexicographicallyLess(orientation, selectedOrientation)) {
                selectedOrientation = orientation;
                selectedAngle = destinationAngle;
            }
        }

        HalconObjectGuard canonicalImage(api);
        HalconObjectGuard canonicalRegion(api);
        buildCanonicalCandidate(api, halconImage.value(), foreground.value(),
                                sourceCenterRow, sourceCenterColumn, phi,
                                selectedAngle, halconFrame.cols, halconFrame.rows,
                                paddingRatio, &canonicalImage, &canonicalRegion);
        const QVector<double> occupancy = occupancyVector(api, canonicalRegion.value());

        HalconTuple foregroundAreaTuple(api);
        HalconTuple foregroundRowTuple(api);
        HalconTuple foregroundColumnTuple(api);
        checkStatus(api, api->areaCenter(foreground.value(), foregroundAreaTuple.ptr(),
                                         foregroundRowTuple.ptr(), foregroundColumnTuple.ptr()),
                    QStringLiteral("area_center(foreground)"));
        const double foregroundArea = requiredFiniteDouble(
                    foregroundAreaTuple, QStringLiteral("foreground area"));
        HalconTuple circularity(api);
        HalconTuple compactness(api);
        HalconTuple convexity(api);
        HalconTuple rectangularity(api);
        HalconTuple anisometry(api);
        HalconTuple bulkiness(api);
        HalconTuple structureFactor(api);
        HalconTuple psi1(api);
        HalconTuple psi2(api);
        HalconTuple psi3(api);
        HalconTuple psi4(api);
        checkStatus(api, api->circularity(foreground.value(), circularity.ptr()), QStringLiteral("circularity"));
        checkStatus(api, api->compactness(foreground.value(), compactness.ptr()), QStringLiteral("compactness"));
        checkStatus(api, api->convexity(foreground.value(), convexity.ptr()), QStringLiteral("convexity"));
        checkStatus(api, api->rectangularity(foreground.value(), rectangularity.ptr()), QStringLiteral("rectangularity"));
        checkStatus(api, api->eccentricity(foreground.value(), anisometry.ptr(), bulkiness.ptr(),
                                           structureFactor.ptr()), QStringLiteral("eccentricity"));
        checkStatus(api, api->momentsRegionCentralInvar(foreground.value(), psi1.ptr(), psi2.ptr(),
                                                        psi3.ptr(), psi4.ptr()),
                    QStringLiteral("moments_region_central_invar"));

        QVector<double> feature;
        feature.reserve(59);
        feature.append(bounded01(length2 / length1));
        feature.append(bounded01(foregroundArea / (4.0 * length1 * length2)));
        feature.append(bounded01(requiredFiniteDouble(circularity, QStringLiteral("circularity"))));
        feature.append(bounded01(1.0 / qMax(
                                            1.0,
                                            requiredFiniteDouble(compactness, QStringLiteral("compactness")))));
        feature.append(bounded01(requiredFiniteDouble(convexity, QStringLiteral("convexity"))));
        feature.append(bounded01(requiredFiniteDouble(rectangularity, QStringLiteral("rectangularity"))));
        feature.append(bounded01(1.0 / qMax(
                                            1.0,
                                            requiredFiniteDouble(anisometry, QStringLiteral("anisometry")))));
        feature.append(bounded01(requiredFiniteDouble(bulkiness, QStringLiteral("bulkiness"))));
        feature.append(bounded01(requiredFiniteDouble(
                                     structureFactor, QStringLiteral("structure factor"))));
        feature.append(boundedSigned(requiredFiniteDouble(psi1, QStringLiteral("moment psi1"))));
        feature.append(boundedSigned(requiredFiniteDouble(psi2, QStringLiteral("moment psi2"))));
        feature.append(boundedSigned(requiredFiniteDouble(psi3, QStringLiteral("moment psi3"))));
        feature.append(boundedSigned(requiredFiniteDouble(psi4, QStringLiteral("moment psi4"))));
        for (double value : occupancy)
            feature.append(value);

        HalconObjectGuard canonicalGray(api);
        checkStatus(api, api->rgb1ToGray(canonicalImage.value(), canonicalGray.ptr()),
                    QStringLiteral("rgb1_to_gray(canonical)"));
        HalconTuple grayMean(api);
        HalconTuple grayDeviation(api);
        HalconTuple absoluteHistogram(api);
        HalconTuple relativeHistogram(api);
        checkStatus(api, api->intensity(canonicalRegion.value(), canonicalGray.value(),
                                        grayMean.ptr(), grayDeviation.ptr()), QStringLiteral("intensity(gray)"));
        checkStatus(api, api->grayHisto(canonicalRegion.value(), canonicalGray.value(),
                                        absoluteHistogram.ptr(), relativeHistogram.ptr()),
                    QStringLiteral("gray_histo"));
        feature.append(bounded01(requiredFiniteDouble(grayMean, QStringLiteral("gray mean")) / 255.0));
        feature.append(bounded01(
                           requiredFiniteDouble(grayDeviation, QStringLiteral("gray deviation")) / 255.0));
        const QVector<double> histogram = compressHistogram(relativeHistogram);
        for (double value : histogram)
            feature.append(bounded01(value));

        HalconObjectGuard red(api);
        HalconObjectGuard green(api);
        HalconObjectGuard blue(api);
        HalconObjectGuard labL(api);
        HalconObjectGuard labA(api);
        HalconObjectGuard labB(api);
        checkStatus(api, api->decompose3(canonicalImage.value(), red.ptr(), green.ptr(), blue.ptr()),
                    QStringLiteral("decompose3"));
        HalconTuple cielab = tupleString(api, "cielab");
        checkStatus(api, api->transFromRgb(red.value(), green.value(), blue.value(),
                                           labL.ptr(), labA.ptr(), labB.ptr(), cielab.value()),
                    QStringLiteral("trans_from_rgb"));
        const Hobject labChannels[] = {labL.value(), labA.value(), labB.value()};
        QVector<double> labMeans;
        QVector<double> labDeviations;
        for (const Hobject channel : labChannels) {
            HalconTuple mean(api);
            HalconTuple deviation(api);
            checkStatus(api, api->intensity(canonicalRegion.value(), channel, mean.ptr(), deviation.ptr()),
                        QStringLiteral("intensity(lab)"));
            labMeans.append(bounded01(
                                requiredFiniteDouble(mean, QStringLiteral("Lab mean")) / 255.0));
            labDeviations.append(bounded01(
                                     requiredFiniteDouble(deviation, QStringLiteral("Lab deviation")) / 255.0));
        }
        for (double value : labMeans)
            feature.append(value);
        for (double value : labDeviations)
            feature.append(value);

        HalconTuple entropy(api);
        HalconTuple textureAnisotropy(api);
        HalconTuple energy(api);
        HalconTuple correlation(api);
        HalconTuple homogeneity(api);
        HalconTuple contrast(api);
        checkStatus(api, api->entropyGray(canonicalRegion.value(), canonicalGray.value(),
                                          entropy.ptr(), textureAnisotropy.ptr()),
                    QStringLiteral("entropy_gray"));
        HalconTuple ldGray = tupleInt(api, 4);
        HalconTuple direction = tupleString(api, "mean");
        checkStatus(api, api->coocFeatureImage(canonicalRegion.value(), canonicalGray.value(),
                                               ldGray.value(), direction.value(), energy.ptr(),
                                               correlation.ptr(), homogeneity.ptr(), contrast.ptr()),
                    QStringLiteral("cooc_feature_image"));
        feature.append(bounded01(requiredFiniteDouble(entropy, QStringLiteral("texture entropy")) / 8.0));
        feature.append(boundedSigned(requiredFiniteDouble(
                                         textureAnisotropy, QStringLiteral("texture anisotropy"))));
        feature.append(bounded01(requiredFiniteDouble(energy, QStringLiteral("cooc energy"))));
        feature.append(bounded01(0.5 + 0.5 * requiredFiniteDouble(
                                     correlation, QStringLiteral("cooc correlation"))));
        feature.append(bounded01(requiredFiniteDouble(
                                     homogeneity, QStringLiteral("cooc homogeneity"))));
        const double contrastValue = requiredFiniteDouble(
                    contrast, QStringLiteral("cooc contrast"));
        feature.append(bounded01(contrastValue / (1.0 + std::abs(contrastValue))));

        appendWeightedGroups(&feature);
        if (feature.size() != result.featureNames.size()
                || !normalizeRegisteredClassificationFeature(&feature)) {
            throw std::pair<QString, QString>(
                    QStringLiteral("invalid_feature_value"),
                    QStringLiteral("V2 feature must contain exactly 59 finite non-zero values."));
        }

        result.success = true;
        result.status = QStringLiteral("ok");
        result.message = QStringLiteral("V2 feature extraction succeeded.");
        result.feature = feature;
        result.foregroundPolarity = best.polarity;
        result.foregroundAreaRatio = best.areaRatio;
        result.foregroundObjectScore = best.objectScore;
        result.payload.insert(QStringLiteral("feature"), vectorToJson(feature));
        result.payload.insert(QStringLiteral("featureLength"), feature.size());
        result.payload.insert(QStringLiteral("foregroundPolarity"), best.polarity);
        result.payload.insert(QStringLiteral("foregroundAreaRatio"), best.areaRatio);
        result.payload.insert(QStringLiteral("foregroundObjectScore"), best.objectScore);
        result.payload.insert(QStringLiteral("foregroundCenterDistance"), best.centerDistance);
        result.payload.insert(QStringLiteral("foregroundBorderTouchRatio"), best.borderTouchRatio);
        result.payload.insert(QStringLiteral("canonicalWidth"), kCanonicalSize);
        result.payload.insert(QStringLiteral("canonicalHeight"), kCanonicalSize);
        result.payload.insert(QStringLiteral("errorCode"), result.status);
        result.payload.insert(QStringLiteral("errorMessage"), result.message);
        return result;
    } catch (const std::pair<QString, QString> &failure) {
        result.success = false;
        result.status = failure.first;
        result.message = failure.second;
    } catch (const std::exception &e) {
        result.success = false;
        result.status = QStringLiteral("exception");
        result.message = QString::fromLocal8Bit(e.what());
    } catch (...) {
        result.success = false;
        result.status = QStringLiteral("exception");
        result.message = QStringLiteral("Unknown exception during V2 feature extraction.");
    }
    result.payload.insert(QStringLiteral("errorCode"), result.status);
    result.payload.insert(QStringLiteral("errorMessage"), result.message);
    return result;
}
