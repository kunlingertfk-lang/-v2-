#include "algorithms/recognition/ColorRecognitionHalconRunner.h"

#include <HalconC.h>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRect>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <exception>
#include <opencv2/imgproc.hpp>

namespace {

constexpr int kMinRoiPixelSize = 2;

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

bool isValidNormalizedRoi(const QRectF &rect)
{
    return isFiniteRect(rect) && rect.width() > 0.0 && rect.height() > 0.0;
}

bool isCircleRegionType(const QString &type)
{
    return type.trimmed().toLower() == QStringLiteral("circle");
}

QRect normalizedRoiToPixels(const QRectF &sourceRoi, const int width, const int height)
{
    if (width <= 0 || height <= 0 || !isValidNormalizedRoi(sourceRoi))
        return QRect();

    const QRectF roi = sourceRoi.normalized();
    const double left = qBound(0.0, roi.left(), 1.0);
    const double top = qBound(0.0, roi.top(), 1.0);
    const double right = qBound(0.0, roi.right(), 1.0);
    const double bottom = qBound(0.0, roi.bottom(), 1.0);
    if (right <= left || bottom <= top)
        return QRect();

    const int x1 = qBound(0, static_cast<int>(std::floor(left * width)), width - 1);
    const int y1 = qBound(0, static_cast<int>(std::floor(top * height)), height - 1);
    const int x2 = qBound(0, static_cast<int>(std::ceil(right * width)), width);
    const int y2 = qBound(0, static_cast<int>(std::ceil(bottom * height)), height);
    const int roiWidth = x2 - x1;
    const int roiHeight = y2 - y1;
    if (roiWidth < kMinRoiPixelSize || roiHeight < kMinRoiPixelSize)
        return QRect();

    return QRect(x1, y1, roiWidth, roiHeight);
}

QJsonObject rectToJson(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

QJsonArray vectorToJson(const QVector<double> &values)
{
    QJsonArray array;
    for (const double value : values)
        array.append(value);
    return array;
}

QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points) {
        QJsonObject json;
        json.insert(QStringLiteral("x"), point.x());
        json.insert(QStringLiteral("y"), point.y());
        array.append(json);
    }
    return array;
}

QJsonObject pointToJsonObject(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
}

ToolOverlay rectOverlay(const QRectF &rect, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.rect = rect;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

ToolOverlay circleOverlay(const QPointF &center,
                          const double radius,
                          const QString &label,
                          const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Circle;
    overlay.center = center;
    overlay.radius = radius;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

QJsonObject rectToJsonObject(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

ToolOverlay textOverlay(const QPointF &position,
                        const QString &text,
                        const double score = 0.0,
                        const QString &label = QString())
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.p1 = position;
    overlay.text = text;
    overlay.label = label.isEmpty() ? text : label;
    overlay.score = score;
    return overlay;
}

int histogramBinsForSensitivity(const QString &sensitivity)
{
    const QString key = sensitivity.trimmed().toLower();
    if (key == QStringLiteral("low"))
        return 8;
    if (key == QStringLiteral("high"))
        return 32;
    return 16;
}

cv::Mat toBgr8(const cv::Mat &image)
{
    if (image.empty())
        return cv::Mat();

    cv::Mat source;
    if (image.depth() == CV_8U)
        source = image;
    else
        image.convertTo(source, CV_8U);

    cv::Mat bgr;
    if (source.channels() == 1) {
        cv::cvtColor(source, bgr, cv::COLOR_GRAY2BGR);
    } else if (source.channels() == 3) {
        bgr = source;
    } else if (source.channels() == 4) {
        cv::cvtColor(source, bgr, cv::COLOR_BGRA2BGR);
    }

    if (!bgr.empty() && !bgr.isContinuous())
        bgr = bgr.clone();
    return bgr;
}

ColorRecognitionHalconFeatureResult featureError(const QString &status,
                                                 const QString &message,
                                                 const ColorRecognitionHalconConfig &config,
                                                 const cv::Mat &image,
                                                 const qint64 elapsedMs)
{
    ColorRecognitionHalconFeatureResult result;
    result.success = false;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.payload.insert(QStringLiteral("error"), message);
    result.payload.insert(QStringLiteral("featureType"), config.featureType);
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                          pointToJsonObject(config.detectCircleCenterNormalized));
    result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                          config.detectCircleRadiusNormalized);
    result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                          rectToJson(config.detectCircleBoundingRectNormalized));
    result.payload.insert(QStringLiteral("detectMaskPolygon"),
                          pointsToJson(config.detectMaskPolygonNormalized));
    result.payload.insert(QStringLiteral("detectMaskConfigured"),
                          config.detectMaskPolygonNormalized.size() >= 3);
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"),
                          config.halconSoPathCandidates.join(QStringLiteral("; ")));
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    return result;
}

ColorRecognitionHalconResult runError(const QString &status,
                                      const QString &message,
                                      const ColorRecognitionHalconConfig &config,
                                      const cv::Mat &image,
                                      const qint64 elapsedMs)
{
    ColorRecognitionHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.payload.insert(QStringLiteral("error"), message);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_histogram_intersection_color_recognition"));
    result.payload.insert(QStringLiteral("featureType"), config.featureType);
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                          pointToJsonObject(config.detectCircleCenterNormalized));
    result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                          config.detectCircleRadiusNormalized);
    result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                          rectToJson(config.detectCircleBoundingRectNormalized));
    result.payload.insert(QStringLiteral("detectMaskPolygon"),
                          pointsToJson(config.detectMaskPolygonNormalized));
    result.payload.insert(QStringLiteral("detectMaskConfigured"),
                          config.detectMaskPolygonNormalized.size() >= 3);
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"),
                          config.halconSoPathCandidates.join(QStringLiteral("; ")));
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    return result;
}

ColorRecognitionHalconResult maskedRoiResult(const ColorRecognitionHalconConfig &config,
                                             const cv::Mat &image,
                                             const qint64 elapsedMs)
{
    ColorRecognitionHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = QStringLiteral("masked_roi_empty");
    result.message = QStringLiteral("检测 ROI 已完全被屏蔽区域覆盖，未执行颜色识别。");
    result.elapsedMs = elapsedMs;

    const QRect roiPixels = normalizedRoiToPixels(config.roiNormalized,
                                                  image.empty() ? 0 : image.cols,
                                                  image.empty() ? 0 : image.rows);
    if (!roiPixels.isEmpty()) {
        if (isCircleRegionType(config.detectRegionType) &&
            config.detectCircleRadiusNormalized > 0.0 &&
            !image.empty()) {
            const double radiusPixels = config.detectCircleRadiusNormalized *
                    static_cast<double>(qMax(image.cols, image.rows));
            result.overlays.append(circleOverlay(QPointF(config.detectCircleCenterNormalized.x() * image.cols,
                                                         config.detectCircleCenterNormalized.y() * image.rows),
                                                radiusPixels,
                                                QStringLiteral("ROI"),
                                                0.0));
        } else {
            result.overlays.append(rectOverlay(QRectF(roiPixels), QStringLiteral("ROI"), 0.0));
        }
        ToolOverlay statusText = textOverlay(QPointF(roiPixels.x(), roiPixels.y()),
                                             QStringLiteral("颜色识别 已屏蔽"),
                                             0.0,
                                             QStringLiteral("color_result_text"));
        statusText.extra.insert(QStringLiteral("status"), QStringLiteral("MASKED"));
        statusText.extra.insert(QStringLiteral("anchorRect"), rectToJsonObject(QRectF(roiPixels)));
        result.overlays.append(statusText);
    }

    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_histogram_intersection_color_recognition"));
    result.payload.insert(QStringLiteral("featureType"), config.featureType);
    result.payload.insert(QStringLiteral("detectMaskApplied"), true);
    result.payload.insert(QStringLiteral("detectMaskFullyCoversRoi"), true);
    result.payload.insert(QStringLiteral("detectMaskPolygon"), pointsToJson(config.detectMaskPolygonNormalized));
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("circleDetectRoiApplied"), isCircleRegionType(config.detectRegionType));
    result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                          pointToJsonObject(config.detectCircleCenterNormalized));
    result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                          config.detectCircleRadiusNormalized);
    result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                          rectToJson(config.detectCircleBoundingRectNormalized));
    result.payload.insert(QStringLiteral("roiPixelsRect"), rectToJson(QRectF(roiPixels)));
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    return result;
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
    using GetIntFn = Hlong (*)(const Htuple *, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Decompose3Fn = Herror (*)(const Hobject, Hobject *, Hobject *, Hobject *);
    using TransFromRgbFn = Herror (*)(const Hobject, const Hobject, const Hobject,
                                      Hobject *, Hobject *, Hobject *, const char *);
    using GenRectangle1Fn = Herror (*)(Hobject *, double, double, double, double);
    using GenCircleFn = Herror (*)(Hobject *, double, double, double);
    using GenRegionPolygonFilledFn = Herror (*)(Hobject *, const Htuple, const Htuple);
    using DifferenceFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using AreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using GrayHistoRangeFn = Herror (*)(const Hobject, const Hobject, const Htuple,
                                        const Htuple, const Htuple, Htuple *, Htuple *);
    using TupleMin2Fn = Herror (*)(const Htuple, const Htuple, Htuple *);
    using TupleSumFn = Herror (*)(const Htuple, Htuple *);
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
    Decompose3Fn decompose3 = nullptr;
    TransFromRgbFn transFromRgb = nullptr;
    GenRectangle1Fn genRectangle1 = nullptr;
    GenCircleFn genCircle = nullptr;
    GenRegionPolygonFilledFn genRegionPolygonFilled = nullptr;
    DifferenceFn difference = nullptr;
    AreaCenterFn areaCenter = nullptr;
    ReduceDomainFn reduceDomain = nullptr;
    GrayHistoRangeFn grayHistoRange = nullptr;
    TupleMin2Fn tupleMin2 = nullptr;
    TupleSumFn tupleSum = nullptr;
    ClearObjFn clearObj = nullptr;
};

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

        resolveOptional(m_handle, api.setUtf8, "SetHcInterfaceStringEncodingIsUtf8");
        if (!resolveRequired(m_handle, api.getErrorText, "get_error_text", errorMessage) ||
            !resolveRequired(m_handle, api.createTuple, "F_create_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.setDouble, "F_set_d", errorMessage) ||
            !resolveRequired(m_handle, api.setInt, "F_set_i", errorMessage) ||
            !resolveRequired(m_handle, api.setString, "F_set_s", errorMessage) ||
            !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
            !resolveRequired(m_handle, api.getInt, "F_get_i", errorMessage) ||
            !resolveRequired(m_handle, api.genImageInterleaved, "gen_image_interleaved", errorMessage) ||
            !resolveRequired(m_handle, api.decompose3, "decompose3", errorMessage) ||
            !resolveRequired(m_handle, api.transFromRgb, "trans_from_rgb", errorMessage) ||
            !resolveRequired(m_handle, api.genRectangle1, "gen_rectangle1", errorMessage) ||
            !resolveRequired(m_handle, api.genCircle, "gen_circle", errorMessage) ||
            !resolveRequired(m_handle, api.genRegionPolygonFilled, "T_gen_region_polygon_filled", errorMessage) ||
            !resolveRequired(m_handle, api.difference, "difference", errorMessage) ||
            !resolveRequired(m_handle, api.areaCenter, "T_area_center", errorMessage) ||
            !resolveRequired(m_handle, api.reduceDomain, "reduce_domain", errorMessage) ||
            !resolveRequired(m_handle, api.grayHistoRange, "T_gray_histo_range", errorMessage) ||
            !resolveRequired(m_handle, api.tupleMin2, "T_tuple_min2", errorMessage) ||
            !resolveRequired(m_handle, api.tupleSum, "T_tuple_sum", errorMessage) ||
            !resolveRequired(m_handle, api.clearObj, "clear_obj", errorMessage)) {
            symbolMissing = true;
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
    explicit HalconTuple(HalconCApi *api = nullptr)
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

    void setString(const int index, const char *value)
    {
        if (m_api && m_api->setString)
            m_api->setString(&m_tuple, value, index);
    }

    double doubleAt(const int index) const
    {
        return m_api && m_api->getDouble ? m_api->getDouble(&m_tuple, index) : 0.0;
    }

    Hlong intAt(const int index) const
    {
        return m_api && m_api->getInt ? m_api->getInt(&m_tuple, index) : 0;
    }

    void destroy()
    {
        if (m_api && m_api->destroyTuple && (m_tuple.num > 0 || m_tuple.capacity > 0))
            m_api->destroyTuple(&m_tuple);
        m_tuple = HTUPLE_INITIALIZER;
    }

private:
    HalconCApi *m_api = nullptr;
    Htuple m_tuple = HTUPLE_INITIALIZER;
};

QString halconErrorText(HalconCApi *api, const Herror status)
{
    char buffer[1024] = {0};
    if (api && api->getErrorText && halconStatusOk(api->getErrorText(static_cast<Hlong>(status), buffer))) {
        const QString message = QString::fromUtf8(buffer).trimmed();
        if (!message.isEmpty())
            return message;
    }
    return QStringLiteral("HALCON error code %1").arg(static_cast<qlonglong>(status));
}

void checkStatus(HalconCApi *api, const Herror status, const QString &stage)
{
    if (!halconStatusOk(status)) {
        throw std::pair<QString, QString>(
                QStringLiteral("halcon_error"),
                QStringLiteral("%1: %2").arg(stage, halconErrorText(api, status)));
    }
}

HalconTuple featureToTuple(HalconCApi *api, const QVector<double> &feature)
{
    HalconTuple tuple(api);
    tuple.create(feature.size());
    for (int i = 0; i < feature.size(); ++i)
        tuple.setDouble(i, feature.at(i));
    return tuple;
}

double tupleSumValue(HalconCApi *api, const HalconTuple &tuple, const QString &stage)
{
    HalconTuple sum(api);
    checkStatus(api, api->tupleSum(tuple.value(), sum.ptr()), stage + QStringLiteral(".tuple_sum"));
    return sum.size() > 0 ? sum.doubleAt(0) : 0.0;
}

void createDoubleArrayTuple(HalconTuple &tuple, const QVector<double> &values)
{
    tuple.create(values.size());
    for (int i = 0; i < values.size(); ++i)
        tuple.setDouble(i, values.at(i));
}

QVector<QPointF> maskPolygonToPixels(const QVector<QPointF> &normalizedPoints,
                                     const int width,
                                     const int height)
{
    QVector<QPointF> pixels;
    if (width <= 0 || height <= 0 || normalizedPoints.size() < 3)
        return pixels;

    pixels.reserve(normalizedPoints.size());
    for (const QPointF &point : normalizedPoints) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
            continue;
        pixels.append(QPointF(qBound(0.0, point.x(), 1.0) * static_cast<double>(width - 1),
                              qBound(0.0, point.y(), 1.0) * static_cast<double>(height - 1)));
    }
    return pixels.size() >= 3 ? pixels : QVector<QPointF>();
}

double regionArea(HalconCApi *api, const Hobject region, const QString &stage)
{
    HalconTuple area(api);
    HalconTuple row(api);
    HalconTuple column(api);
    checkStatus(api, api->areaCenter(region, area.ptr(), row.ptr(), column.ptr()),
                stage + QStringLiteral(".area_center"));
    return area.size() > 0 ? area.doubleAt(0) : 0.0;
}

double histogramIntersectionSimilarity(HalconCApi *api,
                                       const QVector<double> &queryFeature,
                                       const QVector<double> &sampleFeature)
{
    if (queryFeature.size() != sampleFeature.size() || queryFeature.isEmpty()) {
        throw std::pair<QString, QString>(
                QStringLiteral("invalid_feature_dimension"),
                QStringLiteral("Histogram feature dimensions do not match."));
    }

    HalconTuple queryTuple = featureToTuple(api, queryFeature);
    HalconTuple sampleTuple = featureToTuple(api, sampleFeature);
    HalconTuple minTuple(api);
    checkStatus(api, api->tupleMin2(queryTuple.value(), sampleTuple.value(), minTuple.ptr()),
                QStringLiteral("histogram_intersection.tuple_min2"));

    const double intersection = tupleSumValue(api, minTuple, QStringLiteral("histogram_intersection.minimum"));
    const double querySum = tupleSumValue(api, queryTuple, QStringLiteral("histogram_intersection.query"));
    const double sampleSum = tupleSumValue(api, sampleTuple, QStringLiteral("histogram_intersection.sample"));
    const double denominator = qMin(querySum, sampleSum);
    if (denominator <= 0.0)
        return 0.0;

    return qBound(0.0, intersection / denominator, 1.0);
}

void clearObject(HalconCApi *api, Hobject &object)
{
    if (api && api->clearObj && halconObjectAllocated(object))
        api->clearObj(object);
    object = NO_OBJECTS;
}

QVector<double> histogramForChannel(HalconCApi *api,
                                    const Hobject roiRegion,
                                    const Hobject channel,
                                    const int bins,
                                    const QString &stage)
{
    Hobject reducedChannel = NO_OBJECTS;
    HalconTuple minValue(api);
    HalconTuple maxValue(api);
    HalconTuple binCount(api);
    HalconTuple histo(api);
    HalconTuple binSize(api);

    minValue.create(1);
    minValue.setDouble(0, 0.0);
    maxValue.create(1);
    maxValue.setDouble(0, 255.0);
    binCount.create(1);
    binCount.setInt(0, bins);

    checkStatus(api, api->reduceDomain(channel, roiRegion, &reducedChannel),
                stage + QStringLiteral(".reduce_domain"));
    try {
        checkStatus(api, api->grayHistoRange(roiRegion,
                                             reducedChannel,
                                             minValue.value(),
                                             maxValue.value(),
                                             binCount.value(),
                                             histo.ptr(),
                                             binSize.ptr()),
                    stage + QStringLiteral(".gray_histo_range"));
    } catch (...) {
        clearObject(api, reducedChannel);
        throw;
    }
    clearObject(api, reducedChannel);

    double sum = 0.0;
    const int histoSize = histo.size();
    for (int i = 0; i < histoSize; ++i) {
        const Hlong value = histo.intAt(i);
        sum += static_cast<double>(value);
    }
    if (sum <= 0.0)
        sum = 1.0;

    QVector<double> normalized;
    normalized.reserve(histoSize);
    for (int i = 0; i < histoSize; ++i) {
        const Hlong value = histo.intAt(i);
        normalized.append(static_cast<double>(value) / sum);
    }
    return normalized;
}

struct HistogramExtractionResult
{
    QVector<double> feature;
    bool detectMaskApplied = false;
    bool detectMaskFullyCoversRoi = false;
    bool circleDetectRoiApplied = false;
    double effectiveRoiArea = 0.0;
};

HistogramExtractionResult extractHistogramFeature(const cv::Mat &bgr,
                                                  const QRect &roiPixels,
                                                  const ColorRecognitionHalconConfig &config,
                                                  HalconCApi *api)
{
    Hobject image = NO_OBJECTS;
    Hobject red = NO_OBJECTS;
    Hobject green = NO_OBJECTS;
    Hobject blue = NO_OBJECTS;
    Hobject hue = NO_OBJECTS;
    Hobject saturation = NO_OBJECTS;
    Hobject value = NO_OBJECTS;
    Hobject roiRegion = NO_OBJECTS;
    Hobject maskRegion = NO_OBJECTS;
    Hobject effectiveRegion = NO_OBJECTS;

    auto cleanup = [&]() {
        if (effectiveRegion != roiRegion)
            clearObject(api, effectiveRegion);
        clearObject(api, maskRegion);
        clearObject(api, roiRegion);
        clearObject(api, value);
        clearObject(api, saturation);
        clearObject(api, hue);
        clearObject(api, blue);
        clearObject(api, green);
        clearObject(api, red);
        clearObject(api, image);
    };

    try {
        checkStatus(api, api->genImageInterleaved(&image,
                                                  reinterpret_cast<Hlong>(bgr.data),
                                                  "bgr",
                                                  bgr.cols,
                                                  bgr.rows,
                                                  0,
                                                  "byte",
                                                  0,
                                                  0,
                                                  0,
                                                  0,
                                                  8,
                                                  0),
                    QStringLiteral("gen_image_interleaved"));
        checkStatus(api, api->decompose3(image, &red, &green, &blue),
                    QStringLiteral("decompose3"));
        checkStatus(api, api->transFromRgb(red, green, blue, &hue, &saturation, &value, "hsv"),
                    QStringLiteral("trans_from_rgb"));
        HistogramExtractionResult result;
        if (isCircleRegionType(config.detectRegionType)) {
            const double radiusPixels = config.detectCircleRadiusNormalized *
                    static_cast<double>(qMax(bgr.cols, bgr.rows));
            if (radiusPixels < static_cast<double>(kMinRoiPixelSize)) {
                throw std::pair<QString, QString>(
                        QStringLiteral("invalid_roi"),
                        QStringLiteral("Color recognition circle ROI is invalid."));
            }
            const double centerColumn =
                    qBound(0.0,
                           config.detectCircleCenterNormalized.x() * static_cast<double>(bgr.cols),
                           static_cast<double>(bgr.cols - 1));
            const double centerRow =
                    qBound(0.0,
                           config.detectCircleCenterNormalized.y() * static_cast<double>(bgr.rows),
                           static_cast<double>(bgr.rows - 1));
            checkStatus(api, api->genCircle(&roiRegion,
                                            centerRow,
                                            centerColumn,
                                            radiusPixels),
                        QStringLiteral("gen_circle.detect_roi"));
            result.circleDetectRoiApplied = true;
        } else {
            checkStatus(api, api->genRectangle1(&roiRegion,
                                                roiPixels.y(),
                                                roiPixels.x(),
                                                roiPixels.y() + roiPixels.height() - 1,
                                                roiPixels.x() + roiPixels.width() - 1),
                        QStringLiteral("gen_rectangle1"));
        }
        effectiveRegion = roiRegion;

        const QVector<QPointF> maskPixels =
                maskPolygonToPixels(config.detectMaskPolygonNormalized, bgr.cols, bgr.rows);
        if (maskPixels.size() >= 3) {
            QVector<double> rows;
            QVector<double> columns;
            rows.reserve(maskPixels.size());
            columns.reserve(maskPixels.size());
            for (const QPointF &point : maskPixels) {
                rows.append(qBound(0.0, point.y(), static_cast<double>(bgr.rows - 1)));
                columns.append(qBound(0.0, point.x(), static_cast<double>(bgr.cols - 1)));
            }

            HalconTuple rowTuple(api);
            HalconTuple columnTuple(api);
            createDoubleArrayTuple(rowTuple, rows);
            createDoubleArrayTuple(columnTuple, columns);
            checkStatus(api,
                        api->genRegionPolygonFilled(&maskRegion, rowTuple.value(), columnTuple.value()),
                        QStringLiteral("gen_region_polygon_filled.detect_mask"));
            checkStatus(api,
                        api->difference(roiRegion, maskRegion, &effectiveRegion),
                        QStringLiteral("difference.detect_mask"));
            result.detectMaskApplied = true;
        }

        result.effectiveRoiArea = regionArea(api, effectiveRegion, QStringLiteral("effective_roi"));
        if (result.effectiveRoiArea <= 0.0) {
            result.detectMaskFullyCoversRoi = result.detectMaskApplied;
            throw std::pair<QString, QString>(
                    QStringLiteral("masked_roi_empty"),
                    QStringLiteral("Detection ROI is fully covered by the mask ROI."));
        }

        const int bins = histogramBinsForSensitivity(config.sensitivity);
        result.feature.reserve(config.brightnessEnabled ? bins * 3 : bins * 2);
        result.feature += histogramForChannel(api, effectiveRegion, hue, bins, QStringLiteral("hue"));
        result.feature += histogramForChannel(api, effectiveRegion, saturation, bins, QStringLiteral("saturation"));
        if (config.brightnessEnabled)
            result.feature += histogramForChannel(api, effectiveRegion, value, bins, QStringLiteral("value"));

        cleanup();
        return result;
    } catch (...) {
        cleanup();
        throw;
    }
}

} // namespace

ColorRecognitionHalconFeatureResult ColorRecognitionHalconRunner::extractFeature(
        const cv::Mat &image,
        const ColorRecognitionHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    try {
        if (config.featureType.trimmed().toLower() == QStringLiteral("spectrum")) {
            return featureError(QStringLiteral("unsupported_feature"),
                                QStringLiteral("Spectrum feature is reserved but not implemented."),
                                config,
                                image,
                                timer.elapsed());
        }
        if (config.featureType.trimmed().toLower() != QStringLiteral("histogram")) {
            return featureError(QStringLiteral("unsupported_feature"),
                                QStringLiteral("Only histogram feature is supported in the first version."),
                                config,
                                image,
                                timer.elapsed());
        }

        const cv::Mat bgr = toBgr8(image);
        if (bgr.empty()) {
            return featureError(QStringLiteral("image_empty"),
                                QStringLiteral("Input image is empty or unsupported."),
                                config,
                                image,
                                timer.elapsed());
        }

        const QRect roiPixels = normalizedRoiToPixels(config.roiNormalized, bgr.cols, bgr.rows);
        if (roiPixels.isEmpty()) {
            return featureError(QStringLiteral("invalid_roi"),
                                QStringLiteral("Color recognition ROI is invalid."),
                                config,
                                image,
                                timer.elapsed());
        }

        if (config.halconSoPath.trimmed().isEmpty() || !QFileInfo::exists(config.halconSoPath)) {
            const QString triedPaths = config.halconSoPathCandidates.isEmpty()
                    ? config.halconSoPath
                    : config.halconSoPathCandidates.join(QStringLiteral("; "));
            return featureError(QStringLiteral("halcon_so_not_found"),
                                QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                                .arg(config.halconSoPath, triedPaths),
                                config,
                                image,
                                timer.elapsed());
        }

        HalconLibrary library;
        QString loadMessage;
        bool symbolMissing = false;
        if (!library.load(config.halconSoPath, loadMessage, symbolMissing)) {
            return featureError(symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                              : QStringLiteral("halcon_load_failed"),
                                loadMessage,
                                config,
                                image,
                                timer.elapsed());
        }

        const HistogramExtractionResult histogramResult =
                extractHistogramFeature(bgr, roiPixels, config, &library.api);
        ColorRecognitionHalconFeatureResult result;
        result.feature = histogramResult.feature;
        result.success = true;
        result.status = QStringLiteral("ok");
        result.message = QStringLiteral("feature extracted");
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_histogram_feature"));
        result.payload.insert(QStringLiteral("featureType"), QStringLiteral("histogram"));
        result.payload.insert(QStringLiteral("featureLength"), result.feature.size());
        result.payload.insert(QStringLiteral("histogramBins"), histogramBinsForSensitivity(config.sensitivity));
        result.payload.insert(QStringLiteral("brightnessEnabled"), config.brightnessEnabled);
        result.payload.insert(QStringLiteral("roiPixelsRect"), rectToJson(QRectF(roiPixels)));
        result.payload.insert(QStringLiteral("detectRegionType"),
                              isCircleRegionType(config.detectRegionType)
                              ? QStringLiteral("circle")
                              : QStringLiteral("rectangle"));
        result.payload.insert(QStringLiteral("circleDetectRoiApplied"),
                              histogramResult.circleDetectRoiApplied);
        result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                              pointToJsonObject(config.detectCircleCenterNormalized));
        result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                              config.detectCircleRadiusNormalized);
        result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                              rectToJson(config.detectCircleBoundingRectNormalized));
        result.payload.insert(QStringLiteral("detectMaskApplied"), histogramResult.detectMaskApplied);
        result.payload.insert(QStringLiteral("detectMaskFullyCoversRoi"), false);
        result.payload.insert(QStringLiteral("effectiveRoiArea"), histogramResult.effectiveRoiArea);
        result.payload.insert(QStringLiteral("detectMaskPolygon"), pointsToJson(config.detectMaskPolygonNormalized));
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const std::pair<QString, QString> &error) {
        return featureError(error.first, error.second, config, image, timer.elapsed());
    } catch (const std::exception &error) {
        return featureError(QStringLiteral("exception"),
                            QString::fromLocal8Bit(error.what()),
                            config,
                            image,
                            timer.elapsed());
    } catch (...) {
        return featureError(QStringLiteral("exception"),
                            QStringLiteral("Unknown exception while extracting color feature."),
                            config,
                            image,
                            timer.elapsed());
    }
}

ColorRecognitionHalconResult ColorRecognitionHalconRunner::run(
        const cv::Mat &image,
        const ColorRecognitionHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    if (config.featureType.trimmed().toLower() == QStringLiteral("spectrum")) {
        return runError(QStringLiteral("unsupported_feature"),
                        QStringLiteral("Spectrum feature is reserved but not implemented."),
                        config,
                        image,
                        timer.elapsed());
    }

    const ColorRecognitionHalconFeatureResult featureResult = extractFeature(image, config);
    if (!featureResult.success) {
        if (featureResult.status == QStringLiteral("masked_roi_empty"))
            return maskedRoiResult(config, image, timer.elapsed());

        ColorRecognitionHalconResult error = runError(featureResult.status,
                                                      featureResult.message,
                                                      config,
                                                      image,
                                                      timer.elapsed());
        error.payload.insert(QStringLiteral("featurePayload"), featureResult.payload);
        return error;
    }

    if (config.samples.isEmpty()) {
        return runError(QStringLiteral("no_model_samples"),
                        QStringLiteral("Color model has no trained samples."),
                        config,
                        image,
                        timer.elapsed());
    }

    QVector<ColorRecognitionHalconSample> usableSamples;
    usableSamples.reserve(config.samples.size());
    for (const ColorRecognitionHalconSample &sample : config.samples) {
        if (sample.feature.size() == featureResult.feature.size())
            usableSamples.append(sample);
    }
    if (usableSamples.isEmpty()) {
        return runError(QStringLiteral("invalid_model_samples"),
                        QStringLiteral("Color model samples do not match current feature dimension."),
                        config,
                        image,
                        timer.elapsed());
    }

    HalconLibrary library;
    QString loadMessage;
    bool symbolMissing = false;
    if (!library.load(config.halconSoPath, loadMessage, symbolMissing)) {
        return runError(symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                      : QStringLiteral("halcon_load_failed"),
                        loadMessage,
                        config,
                        image,
                        timer.elapsed());
    }

    HalconCApi *api = &library.api;

    try {
        double bestSimilarity = -1.0;
        int bestSampleIndex = -1;
        for (int sampleIndex = 0; sampleIndex < usableSamples.size(); ++sampleIndex) {
            const ColorRecognitionHalconSample &sample = usableSamples.at(sampleIndex);
            const double similarity = histogramIntersectionSimilarity(api,
                                                                      featureResult.feature,
                                                                      sample.feature);
            if (similarity > bestSimilarity) {
                bestSimilarity = similarity;
                bestSampleIndex = sampleIndex;
            }
        }

        if (bestSampleIndex < 0 || bestSimilarity < 0.0) {
            throw std::pair<QString, QString>(
                    QStringLiteral("empty_similarity"),
                    QStringLiteral("Histogram intersection returned no comparable sample."));
        }

        const ColorRecognitionHalconSample &bestSample = usableSamples.at(bestSampleIndex);
        const int predictedClassId = bestSample.classId;
        const double rating = qBound(0.0, bestSimilarity, 1.0);
        const double score = qBound(0.0, rating * 100.0, 100.0);
        QString predictedLabel = bestSample.label;
        for (const ColorRecognitionHalconLabel &label : config.labels) {
            if (label.classId == predictedClassId) {
                predictedLabel = label.name;
                break;
            }
        }

        bool ok = false;
        QString judgeMode = config.judgeMode.trimmed().toLower();
        if (judgeMode == QStringLiteral("category")) {
            if (config.expectedLabel.trimmed().isEmpty()) {
                throw std::pair<QString, QString>(
                        QStringLiteral("invalid_expected_label"),
                        QStringLiteral("Expected label is empty for category judgement."));
            }
            ok = predictedLabel == config.expectedLabel;
        } else {
            judgeMode = QStringLiteral("min_score");
            ok = score >= static_cast<double>(qBound(0, config.minScore, 100));
        }

        ColorRecognitionHalconResult result;
        result.success = true;
        result.ok = ok;
        result.status = ok ? QStringLiteral("ok") : QStringLiteral("ng");
        result.predictedLabel = predictedLabel;
        result.predictedClassId = predictedClassId;
        result.score = score;
        result.rating = rating;
        result.sampleCount = usableSamples.size();
        result.elapsedMs = timer.elapsed();
        result.message = QStringLiteral("label=%1, score=%2")
                .arg(predictedLabel.isEmpty()
                     ? QStringLiteral("#%1").arg(predictedClassId)
                     : predictedLabel,
                     QString::number(score, 'f', 2));

        const QRect roiPixels = normalizedRoiToPixels(config.roiNormalized,
                                                      image.empty() ? 0 : image.cols,
                                                      image.empty() ? 0 : image.rows);
        if (isCircleRegionType(config.detectRegionType) &&
            config.detectCircleRadiusNormalized > 0.0 &&
            !image.empty()) {
            const double radiusPixels = config.detectCircleRadiusNormalized *
                    static_cast<double>(qMax(image.cols, image.rows));
            result.overlays.append(circleOverlay(QPointF(config.detectCircleCenterNormalized.x() * image.cols,
                                                         config.detectCircleCenterNormalized.y() * image.rows),
                                                radiusPixels,
                                                QStringLiteral("ROI"),
                                                score));
        } else {
            result.overlays.append(rectOverlay(QRectF(roiPixels), QStringLiteral("ROI"), score));
        }
        ToolOverlay statusText = textOverlay(QPointF(roiPixels.x(), roiPixels.y()),
                                             QStringLiteral("%1 %2 %3")
                                             .arg(result.ok ? QStringLiteral("OK") : QStringLiteral("NG"),
                                                  predictedLabel,
                                                  QString::number(score, 'f', 1)),
                                             score,
                                             QStringLiteral("color_result_text"));
        statusText.extra.insert(QStringLiteral("status"), result.ok ? QStringLiteral("OK") : QStringLiteral("NG"));
        statusText.extra.insert(QStringLiteral("anchorRect"), rectToJsonObject(QRectF(roiPixels)));
        result.overlays.append(statusText);

        result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_histogram_intersection_color_recognition"));
        result.payload.insert(QStringLiteral("featureType"), QStringLiteral("histogram"));
        result.payload.insert(QStringLiteral("predictedLabel"), predictedLabel);
        result.payload.insert(QStringLiteral("predictedClassId"), predictedClassId);
        result.payload.insert(QStringLiteral("matchedSampleIndex"), bestSampleIndex);
        result.payload.insert(QStringLiteral("matchedSampleLabel"), bestSample.label);
        result.payload.insert(QStringLiteral("score"), score);
        result.payload.insert(QStringLiteral("rating"), rating);
        result.payload.insert(QStringLiteral("similarity"), rating);
        result.payload.insert(QStringLiteral("scoreDirection"), QStringLiteral("higher_is_better"));
        result.payload.insert(QStringLiteral("scoreFormula"), QStringLiteral("histogram_intersection_similarity_x100"));
        result.payload.insert(QStringLiteral("ratingMode"), QStringLiteral("histogram_intersection_similarity"));
        result.payload.insert(QStringLiteral("comparisonMethod"), QStringLiteral("histogram_intersection"));
        result.payload.insert(QStringLiteral("sampleCount"), result.sampleCount);
        result.payload.insert(QStringLiteral("featureLength"), featureResult.feature.size());
        result.payload.insert(QStringLiteral("queryFeature"), vectorToJson(featureResult.feature));
        result.payload.insert(QStringLiteral("roiPixelsRect"), rectToJson(QRectF(roiPixels)));
        result.payload.insert(QStringLiteral("detectRegionType"),
                              featureResult.payload.value(QStringLiteral("detectRegionType")).toString(QStringLiteral("rectangle")));
        result.payload.insert(QStringLiteral("circleDetectRoiApplied"),
                              featureResult.payload.value(QStringLiteral("circleDetectRoiApplied")).toBool(false));
        result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                              pointToJsonObject(config.detectCircleCenterNormalized));
        result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                              config.detectCircleRadiusNormalized);
        result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                              rectToJson(config.detectCircleBoundingRectNormalized));
        result.payload.insert(QStringLiteral("detectMaskApplied"),
                              featureResult.payload.value(QStringLiteral("detectMaskApplied")).toBool(false));
        result.payload.insert(QStringLiteral("detectMaskFullyCoversRoi"), false);
        result.payload.insert(QStringLiteral("effectiveRoiArea"),
                              featureResult.payload.value(QStringLiteral("effectiveRoiArea")).toDouble());
        result.payload.insert(QStringLiteral("detectMaskPolygon"),
                              pointsToJson(config.detectMaskPolygonNormalized));
        result.payload.insert(QStringLiteral("judgeMode"), judgeMode);
        result.payload.insert(QStringLiteral("minScore"), qBound(0, config.minScore, 100));
        result.payload.insert(QStringLiteral("expectedLabel"), config.expectedLabel);
        result.payload.insert(QStringLiteral("knnDistanceApplied"), QStringLiteral("not_used_histogram_intersection"));
        result.payload.insert(QStringLiteral("histogramBins"), histogramBinsForSensitivity(config.sensitivity));
        result.payload.insert(QStringLiteral("brightnessEnabled"), config.brightnessEnabled);
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));

        return result;
    } catch (const std::pair<QString, QString> &error) {
        return runError(error.first, error.second, config, image, timer.elapsed());
    } catch (const std::exception &error) {
        return runError(QStringLiteral("exception"),
                        QString::fromLocal8Bit(error.what()),
                        config,
                        image,
                        timer.elapsed());
    } catch (...) {
        return runError(QStringLiteral("exception"),
                        QStringLiteral("Unknown exception while classifying color feature."),
                        config,
                        image,
                        timer.elapsed());
    }
}
