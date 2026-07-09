#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
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
#include <opencv2/imgproc.hpp>

namespace {

constexpr int kMinRoiPixelSize = 2;
constexpr int kGrayHistogramBins = 256;
constexpr int kCompressedHistogramBins = 16;
constexpr int kGrayHistogramGroupSize = kGrayHistogramBins / kCompressedHistogramBins;

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
    using ThresholdFn = Herror (*)(const Hobject, Hobject *, double, double);
    using AreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using MomentsRegion2ndFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *, Htuple *, Htuple *);
    using IntensityFn = Herror (*)(const Hobject, const Hobject, Htuple *, Htuple *);
    using GrayHistoFn = Herror (*)(const Hobject, const Hobject, Htuple *, Htuple *);
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
    ThresholdFn threshold = nullptr;
    AreaCenterFn areaCenter = nullptr;
    MomentsRegion2ndFn momentsRegion2nd = nullptr;
    IntensityFn intensity = nullptr;
    GrayHistoFn grayHisto = nullptr;
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
            !resolveRequired(m_handle, api.threshold, "threshold", errorMessage) ||
            !resolveRequired(m_handle, api.areaCenter, "T_area_center", errorMessage) ||
            !resolveRequired(m_handle, api.momentsRegion2nd, "T_moments_region_2nd", errorMessage) ||
            !resolveRequired(m_handle, api.intensity, "T_intensity", errorMessage) ||
            !resolveRequired(m_handle, api.grayHisto, "T_gray_histo", errorMessage) ||
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
    QVector<double> compressed(kCompressedHistogramBins, 0.0);
    for (int index = 0; index < relativeHistogram.size(); ++index) {
        const int bucket = qBound(0, index / kGrayHistogramGroupSize, kCompressedHistogramBins - 1);
        compressed[bucket] += relativeHistogram.doubleAt(index);
    }
    return compressed;
}

double normalizedCenter(const double center, const int start, const int extent)
{
    if (extent <= 0)
        return 0.0;
    const double normalized = (center - static_cast<double>(start)) / static_cast<double>(extent);
    return qBound(0.0, normalized, 1.0);
}

int histogramMinValue(const HalconTuple &absoluteHistogram)
{
    for (int index = 0; index < absoluteHistogram.size(); ++index) {
        if (absoluteHistogram.intAt(index) > 0)
            return index;
    }
    return 0;
}

int histogramMaxValue(const HalconTuple &absoluteHistogram)
{
    for (int index = absoluteHistogram.size() - 1; index >= 0; --index) {
        if (absoluteHistogram.intAt(index) > 0)
            return index;
    }
    return 0;
}

} // namespace

RegisteredClassificationFeatureResult RegisteredClassificationFeatureExtractor::extract(
        const cv::Mat &image,
        const QRectF &roiNormalized,
        const RegisteredClassificationFeatureConfig &config) const
{
    if (image.empty())
        return errorResult(QStringLiteral("image_empty"), QStringLiteral("Feature extraction input image is empty."));

    const cv::Mat halconFrame = toBgr8ForHalcon(image);
    if (halconFrame.empty()) {
        return errorResult(QStringLiteral("unsupported_image_type"),
                           QStringLiteral("Only images convertible to continuous CV_8UC3 are supported."));
    }

    const QRect roiPixels = normalizedRoiToPixels(roiNormalized, halconFrame.cols, halconFrame.rows);
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
        result.payload.insert(QStringLiteral("roiPixels"), rectToJson(roiPixels));
        result.payload.insert(QStringLiteral("halconSoPathCandidates"), stringListToJson(halconCandidates));
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
        result.payload.insert(QStringLiteral("roiPixels"), rectToJson(roiPixels));
        result.payload.insert(QStringLiteral("halconSoPath"), halconLibPath);
        result.payload.insert(QStringLiteral("halconSoPathCandidates"), stringListToJson(halconCandidates));
        return result;
    }

    HalconFeatureApi *api = &library.api;
    HalconObjectGuard halconImage(api);
    HalconObjectGuard grayImage(api);
    HalconObjectGuard roiRegion(api);
    HalconObjectGuard roiGray(api);
    HalconObjectGuard foreground(api);
    HalconTuple area(api);
    HalconTuple row(api);
    HalconTuple column(api);
    HalconTuple momentM11(api);
    HalconTuple momentM20(api);
    HalconTuple momentM02(api);
    HalconTuple momentIa(api);
    HalconTuple momentIb(api);
    HalconTuple grayMean(api);
    HalconTuple grayDeviation(api);
    HalconTuple absoluteHistogram(api);
    HalconTuple relativeHistogram(api);

    RegisteredClassificationFeatureResult result;
    result.featureNames = registeredClassificationFeatureNamesV1();
    result.roiPixels = roiPixels;
    result.payload.insert(QStringLiteral("roiPixels"), rectToJson(roiPixels));
    result.payload.insert(QStringLiteral("roiNormalizedX"), roiNormalized.x());
    result.payload.insert(QStringLiteral("roiNormalizedY"), roiNormalized.y());
    result.payload.insert(QStringLiteral("roiNormalizedWidth"), roiNormalized.width());
    result.payload.insert(QStringLiteral("roiNormalizedHeight"), roiNormalized.height());
    result.payload.insert(QStringLiteral("halconSoPath"), halconLibPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"), stringListToJson(halconCandidates));
    result.payload.insert(QStringLiteral("featureNames"), stringListToJson(result.featureNames));

    try {
        checkStatus(api, api->genImageInterleaved(halconImage.ptr(),
                                                  reinterpret_cast<Hlong>(halconFrame.data),
                                                  "bgr",
                                                  halconFrame.cols,
                                                  halconFrame.rows,
                                                  0,
                                                  "byte",
                                                  0,
                                                  0,
                                                  0,
                                                  0,
                                                  8,
                                                  0),
                    QStringLiteral("gen_image_interleaved"));
        checkStatus(api, api->rgb1ToGray(halconImage.value(), grayImage.ptr()),
                    QStringLiteral("rgb1_to_gray"));

        const double row1 = static_cast<double>(roiPixels.top());
        const double col1 = static_cast<double>(roiPixels.left());
        const double row2 = static_cast<double>(roiPixels.bottom());
        const double col2 = static_cast<double>(roiPixels.right());
        checkStatus(api, api->genRectangle1(roiRegion.ptr(), row1, col1, row2, col2),
                    QStringLiteral("gen_rectangle1"));
        checkStatus(api, api->reduceDomain(grayImage.value(), roiRegion.value(), roiGray.ptr()),
                    QStringLiteral("reduce_domain"));

        checkStatus(api, api->intensity(roiRegion.value(), grayImage.value(), grayMean.ptr(), grayDeviation.ptr()),
                    QStringLiteral("intensity"));
        const double mean = grayMean.size() > 0 ? grayMean.doubleAt(0) : 0.0;
        checkStatus(api, api->threshold(roiGray.value(), foreground.ptr(), mean, 255.0),
                    QStringLiteral("threshold"));
        checkStatus(api, api->areaCenter(foreground.value(), area.ptr(), row.ptr(), column.ptr()),
                    QStringLiteral("area_center"));
        checkStatus(api, api->momentsRegion2nd(foreground.value(),
                                               momentM11.ptr(),
                                               momentM20.ptr(),
                                               momentM02.ptr(),
                                               momentIa.ptr(),
                                               momentIb.ptr()),
                    QStringLiteral("moments_region_2nd"));
        checkStatus(api, api->grayHisto(roiRegion.value(), grayImage.value(),
                                        absoluteHistogram.ptr(), relativeHistogram.ptr()),
                    QStringLiteral("gray_histo"));

        const double roiArea = static_cast<double>(roiPixels.width()) * static_cast<double>(roiPixels.height());
        const double imageArea = static_cast<double>(halconFrame.cols) * static_cast<double>(halconFrame.rows);
        const double foregroundArea = area.size() > 0 ? area.doubleAt(0) : 0.0;
        const double centerRow = row.size() > 0 ? row.doubleAt(0) : static_cast<double>(roiPixels.top());
        const double centerColumn = column.size() > 0 ? column.doubleAt(0) : static_cast<double>(roiPixels.left());
        const double m11 = momentM11.size() > 0 ? momentM11.doubleAt(0) : 0.0;
        const double m20 = momentM20.size() > 0 ? momentM20.doubleAt(0) : 0.0;
        const double m02 = momentM02.size() > 0 ? momentM02.doubleAt(0) : 0.0;
        const double phi = 0.5 * std::atan2(2.0 * m11, m20 - m02);
        const QVector<double> grayHistogram = compressHistogram(relativeHistogram);

        result.feature.reserve(result.featureNames.size());
        result.feature.append(static_cast<double>(roiPixels.width()) / static_cast<double>(roiPixels.height()));
        result.feature.append(imageArea > 0.0 ? roiArea / imageArea : 0.0);
        result.feature.append(roiArea > 0.0 ? foregroundArea / roiArea : 0.0);
        result.feature.append(normalizedCenter(centerColumn, roiPixels.left(), roiPixels.width()));
        result.feature.append(normalizedCenter(centerRow, roiPixels.top(), roiPixels.height()));
        result.feature.append(momentIa.size() > 0 ? momentIa.doubleAt(0) : 0.0);
        result.feature.append(momentIb.size() > 0 ? momentIb.doubleAt(0) : 0.0);
        result.feature.append(phi);
        result.feature.append(mean);
        result.feature.append(static_cast<double>(histogramMinValue(absoluteHistogram)));
        result.feature.append(static_cast<double>(histogramMaxValue(absoluteHistogram)));
        result.feature.append(grayDeviation.size() > 0 ? grayDeviation.doubleAt(0) : 0.0);
        for (double value : grayHistogram)
            result.feature.append(value);

        result.success = (result.feature.size() == result.featureNames.size());
        result.status = result.success ? QStringLiteral("ok") : QStringLiteral("feature_length_mismatch");
        result.message = result.success
                ? QStringLiteral("Feature extraction succeeded.")
                : QStringLiteral("Feature vector length does not match metadata.");
        result.payload.insert(QStringLiteral("feature"), vectorToJson(result.feature));
        result.payload.insert(QStringLiteral("roiAspect"), result.feature.value(0));
        result.payload.insert(QStringLiteral("foregroundArea"), foregroundArea);
        result.payload.insert(QStringLiteral("grayMean"), mean);
        result.payload.insert(QStringLiteral("grayDeviation"), result.feature.value(11));
        result.payload.insert(QStringLiteral("grayMin"), result.feature.value(9));
        result.payload.insert(QStringLiteral("grayMax"), result.feature.value(10));
        result.payload.insert(QStringLiteral("foregroundCenterRow"), centerRow);
        result.payload.insert(QStringLiteral("foregroundCenterColumn"), centerColumn);
        result.payload.insert(QStringLiteral("momentM11"), m11);
        result.payload.insert(QStringLiteral("momentM20"), m20);
        result.payload.insert(QStringLiteral("momentM02"), m02);
        result.payload.insert(QStringLiteral("errorCode"), result.status);
        result.payload.insert(QStringLiteral("errorMessage"), result.message);
        return result;
    } catch (const std::pair<QString, QString> &failure) {
        result.success = false;
        result.status = failure.first;
        result.message = failure.second;
        result.payload.insert(QStringLiteral("errorCode"), result.status);
        result.payload.insert(QStringLiteral("errorMessage"), result.message);
        return result;
    } catch (const std::exception &e) {
        result.success = false;
        result.status = QStringLiteral("exception");
        result.message = QString::fromLocal8Bit(e.what());
        result.payload.insert(QStringLiteral("errorCode"), result.status);
        result.payload.insert(QStringLiteral("errorMessage"), result.message);
        return result;
    } catch (...) {
        result.success = false;
        result.status = QStringLiteral("exception");
        result.message = QStringLiteral("Unknown exception during feature extraction.");
        result.payload.insert(QStringLiteral("errorCode"), result.status);
        result.payload.insert(QStringLiteral("errorMessage"), result.message);
        return result;
    }
}
