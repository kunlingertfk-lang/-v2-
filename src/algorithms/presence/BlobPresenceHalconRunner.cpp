#include "algorithms/presence/BlobPresenceHalconRunner.h"

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
#include <limits>
#include <opencv2/core.hpp>

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

QJsonObject rectToJson(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

QJsonObject pointToJson(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
}

QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points)
        array.append(pointToJson(point));
    return array;
}

QJsonArray rectsToJson(const QVector<QRectF> &rects)
{
    QJsonArray array;
    for (const QRectF &rect : rects)
        array.append(rectToJson(rect));
    return array;
}

QJsonArray numbersToJson(const QVector<double> &numbers)
{
    QJsonArray array;
    for (const double number : numbers)
        array.append(number);
    return array;
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

bool isFinitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
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
    const int x2Exclusive = qBound(0, static_cast<int>(std::ceil(right * width)), width);
    const int y2Exclusive = qBound(0, static_cast<int>(std::ceil(bottom * height)), height);

    const int roiWidth = x2Exclusive - x1;
    const int roiHeight = y2Exclusive - y1;
    if (roiWidth < kMinRoiPixelSize || roiHeight < kMinRoiPixelSize)
        return QRect();

    return QRect(x1, y1, roiWidth, roiHeight);
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

ToolOverlay polygonOverlay(const QVector<QPointF> &points, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Polygon;
    overlay.points = points;
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

ToolOverlay lineOverlay(const QPointF &p1,
                        const QPointF &p2,
                        const QString &label,
                        const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Line;
    overlay.p1 = p1;
    overlay.p2 = p2;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

ToolOverlay textOverlay(const QPointF &position,
                        const QString &text,
                        const QString &label,
                        const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.p1 = position;
    overlay.text = text;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

bool isSupportedDetectRegionType(const QString &regionType)
{
    const QString key = regionType.trimmed().toLower();
    return key.isEmpty() ||
           key == QStringLiteral("rect") ||
           key == QStringLiteral("rectangle") ||
           key == QStringLiteral("polygon") ||
           key == QStringLiteral("poly") ||
           key == QStringLiteral("circle") ||
           key.contains(QStringLiteral("矩形")) ||
           key.contains(QStringLiteral("多边形")) ||
           key.contains(QStringLiteral("圆"));
}

bool isPolygonRegionType(const QString &regionType)
{
    const QString key = regionType.trimmed().toLower();
    return key == QStringLiteral("polygon") ||
           key == QStringLiteral("poly") ||
           key.contains(QStringLiteral("多边形"));
}

bool isCircleRegionType(const QString &regionType)
{
    const QString key = regionType.trimmed().toLower();
    return key == QStringLiteral("circle") ||
           key.contains(QStringLiteral("圆"));
}

QVector<QPointF> normalizedPolygonToPixels(const QVector<QPointF> &points,
                                           const int width,
                                           const int height)
{
    QVector<QPointF> pixelPoints;
    if (width <= 0 || height <= 0)
        return pixelPoints;

    pixelPoints.reserve(points.size());
    for (const QPointF &point : points) {
        if (!isFinitePoint(point))
            continue;
        pixelPoints.append(QPointF(qBound(0.0, point.x(), 1.0) * width,
                                   qBound(0.0, point.y(), 1.0) * height));
    }
    return pixelPoints;
}

double normalizedCircleRadiusToPixels(const BlobPresenceHalconConfig &config,
                                      const int width,
                                      const int height)
{
    const double maxDimension = static_cast<double>(qMax(width, height));
    return qBound(0.0, config.detectCircleRadiusNormalized, 1.0) * maxDimension;
}

QPointF normalizedCircleCenterToPixels(const BlobPresenceHalconConfig &config,
                                       const int width,
                                       const int height)
{
    return QPointF(qBound(0.0, config.detectCircleCenterNormalized.x(), 1.0) * width,
                   qBound(0.0, config.detectCircleCenterNormalized.y(), 1.0) * height);
}

void fillPayload(BlobPresenceHalconResult &result,
                 const cv::Mat &image,
                 const BlobPresenceHalconConfig &config)
{
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"),
                          config.halconSoPathCandidates.join(QStringLiteral("; ")));
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("detectPolygonNormalized"),
                          pointsToJson(config.detectPolygonNormalized));
    result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                          pointToJson(config.detectCircleCenterNormalized));
    result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                          config.detectCircleRadiusNormalized);
    result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                          rectToJson(config.detectCircleBoundingRectNormalized));
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("grayMin"), config.grayMin);
    result.payload.insert(QStringLiteral("grayMax"), config.grayMax);
    result.payload.insert(QStringLiteral("invertRange"), config.invertRange);
    result.payload.insert(QStringLiteral("areaMin"), config.areaMin);
    result.payload.insert(QStringLiteral("areaMax"), config.areaMax);
    result.payload.insert(QStringLiteral("maskOutputEnabled"), config.maskOutputEnabled);
    result.payload.insert(QStringLiteral("judgeBasis"), config.judgeBasis);
    result.payload.insert(QStringLiteral("existOk"), config.existOk);
    result.payload.insert(QStringLiteral("timeoutMs"), config.timeoutMs);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("blob_region"));
    result.payload.insert(QStringLiteral("grayMinUsed"), config.grayMin);
    result.payload.insert(QStringLiteral("grayMaxUsed"), config.grayMax);
    result.payload.insert(QStringLiteral("invertRangeUsed"), config.invertRange);
    result.payload.insert(QStringLiteral("areaMinUsed"), config.areaMin);
    result.payload.insert(QStringLiteral("areaMaxUsed"), config.areaMax);
    result.payload.insert(QStringLiteral("countMinUsed"), 1);
    result.payload.insert(QStringLiteral("countMaxUsed"), std::numeric_limits<int>::max());
    result.payload.insert(QStringLiteral("countRangeSource"), QStringLiteral("internal_default"));
    result.payload.insert(QStringLiteral("morphologyApplied"), false);
    result.payload.insert(QStringLiteral("maskOutputRequested"), config.maskOutputEnabled);
    result.payload.insert(QStringLiteral("maskOutputApplied"), false);
    result.payload.insert(QStringLiteral("maskOutputReason"), QStringLiteral("not implemented in runner"));
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));
    result.payload.insert(QStringLiteral("detectMaskApplied"), false);
    result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), false);
    result.payload.insert(QStringLiteral("circleDetectRoiApplied"), false);
    result.payload.insert(QStringLiteral("maskOutputWritten"), false);
}

BlobPresenceHalconResult makeParameterError(const QString &status,
                                            const QString &error,
                                            const cv::Mat &image,
                                            const BlobPresenceHalconConfig &config)
{
    BlobPresenceHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = error;
    result.text = QStringLiteral("error");
    result.payload.insert(QStringLiteral("error"), error);
    fillPayload(result, image, config);
    result.payload.insert(QStringLiteral("okNgReason"), error);
    return result;
}

struct HalconCApi
{
    using SetUtf8Fn = void (*)(int);
    using GetErrorTextFn = Herror (*)(Hlong, char *);
    using CreateTupleFn = void (*)(Htuple *, Hlong);
    using SetDoubleFn = void (*)(Htuple *, double, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GenImage1Fn = Herror (*)(Hobject *, const char *, Hlong, Hlong, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Rgb1ToGrayFn = Herror (*)(const Hobject, Hobject *);
    using ThresholdFn = Herror (*)(const Hobject, Hobject *, double, double);
    using GenRegionPolygonFn = Herror (*)(Hobject *, const Htuple, const Htuple);
    using GenCircleFn = Herror (*)(Hobject *, double, double, double);
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using GenEmptyRegionFn = Herror (*)(Hobject *);
    using Union2Fn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using ConnectionFn = Herror (*)(const Hobject, Hobject *);
    using SelectShapeFn = Herror (*)(const Hobject, Hobject *, const char *, const char *, double, double);
    using CountObjFn = Herror (*)(const Hobject, Hlong *);
    using TSmallestRectangle1Fn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *, Htuple *);
    using TAreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GenImage1Fn genImage1 = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Rgb1ToGrayFn rgb1ToGray = nullptr;
    ThresholdFn threshold = nullptr;
    GenRegionPolygonFn genRegionPolygon = nullptr;
    GenCircleFn genCircle = nullptr;
    ReduceDomainFn reduceDomain = nullptr;
    GenEmptyRegionFn genEmptyRegion = nullptr;
    Union2Fn union2 = nullptr;
    ConnectionFn connection = nullptr;
    SelectShapeFn selectShape = nullptr;
    CountObjFn countObj = nullptr;
    TSmallestRectangle1Fn smallestRectangle1 = nullptr;
    TAreaCenterFn areaCenter = nullptr;
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
        resolveOptional(m_handle, api.createTuple, "F_create_tuple");
        resolveOptional(m_handle, api.setDouble, "F_set_d");
        resolveOptional(m_handle, api.genRegionPolygon, "T_gen_region_polygon");
        if (!api.genRegionPolygon)
            resolveOptional(m_handle, api.genRegionPolygon, "gen_region_polygon");
        resolveOptional(m_handle, api.genCircle, "gen_circle");
        resolveOptional(m_handle, api.reduceDomain, "reduce_domain");

        if (!resolveRequired(m_handle, api.getErrorText, "get_error_text", errorMessage) ||
            !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
            !resolveRequired(m_handle, api.genImage1, "gen_image1", errorMessage) ||
            !resolveRequired(m_handle, api.genImageInterleaved, "gen_image_interleaved", errorMessage) ||
            !resolveRequired(m_handle, api.rgb1ToGray, "rgb1_to_gray", errorMessage) ||
            !resolveRequired(m_handle, api.threshold, "threshold", errorMessage) ||
            !resolveRequired(m_handle, api.genEmptyRegion, "gen_empty_region", errorMessage) ||
            !resolveRequired(m_handle, api.union2, "union2", errorMessage) ||
            !resolveRequired(m_handle, api.connection, "connection", errorMessage) ||
            !resolveRequired(m_handle, api.selectShape, "select_shape", errorMessage) ||
            !resolveRequired(m_handle, api.countObj, "count_obj", errorMessage) ||
            !resolveRequired(m_handle, api.smallestRectangle1, "T_smallest_rectangle1", errorMessage) ||
            !resolveRequired(m_handle, api.areaCenter, "T_area_center", errorMessage) ||
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

struct GrayHalconImage
{
    Hobject inputImage = NO_OBJECTS;
    Hobject grayImage = NO_OBJECTS;
    Hobject graySource = NO_OBJECTS;
};

} // namespace

BlobPresenceHalconResult BlobPresenceHalconRunner::run(
        const cv::Mat &image,
        const BlobPresenceHalconConfig &config)
{
    QElapsedTimer timer;
    timer.start();

    if (image.empty()) {
        BlobPresenceHalconResult result = makeParameterError(QStringLiteral("image_empty"),
                                                             QStringLiteral("input image is empty"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isValidNormalizedRoi(config.roiNormalized)) {
        BlobPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                             QStringLiteral("detect ROI is invalid"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isSupportedDetectRegionType(config.detectRegionType)) {
        BlobPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_detect_roi"),
                                                             QStringLiteral("detect ROI shape is not supported"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("todo"), QStringLiteral("free/circle/polygon detect ROI is not implemented"));
        return result;
    }

    if (isPolygonRegionType(config.detectRegionType) &&
        config.detectPolygonNormalized.size() < 3) {
        BlobPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_polygon"),
                                                             QStringLiteral("polygon detect ROI requires at least 3 points"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (isCircleRegionType(config.detectRegionType) &&
        (!isFinitePoint(config.detectCircleCenterNormalized) ||
         config.detectCircleRadiusNormalized <= 0.0)) {
        BlobPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_circle"),
                                                             QStringLiteral("circle detect ROI is invalid"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (config.grayMin > config.grayMax) {
        BlobPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_gray_range"),
                                                             QStringLiteral("gray range is invalid"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (config.areaMin > config.areaMax) {
        BlobPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_area_range"),
                                                             QStringLiteral("area range is invalid"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (image.depth() != CV_8U ||
        (image.channels() != 1 && image.channels() != 3 && image.channels() != 4)) {
        BlobPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_image_type"),
                                                             QStringLiteral("BlobPresence supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (config.halconSoPath.trimmed().isEmpty() || !QFileInfo::exists(config.halconSoPath)) {
        const QString triedPaths = config.halconSoPathCandidates.isEmpty()
                ? config.halconSoPath
                : config.halconSoPathCandidates.join(QStringLiteral("; "));
        BlobPresenceHalconResult result = makeParameterError(QStringLiteral("halcon_so_not_found"),
                                                             QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                                                             .arg(config.halconSoPath, triedPaths),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    const QRect detectRoiPixels = normalizedRoiToPixels(config.roiNormalized,
                                                        image.cols,
                                                        image.rows);
    if (detectRoiPixels.isEmpty()) {
        BlobPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                             QStringLiteral("detect ROI is invalid"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    BlobPresenceHalconResult result;
    fillPayload(result, image, config);
    result.payload.insert(QStringLiteral("detectRoi"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoiPixels"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoiPixelX"), detectRoiPixels.x());
    result.payload.insert(QStringLiteral("detectRoiPixelY"), detectRoiPixels.y());
    result.payload.insert(QStringLiteral("detectRoiPixelW"), detectRoiPixels.width());
    result.payload.insert(QStringLiteral("detectRoiPixelH"), detectRoiPixels.height());
    const bool polygonDetectRoi = isPolygonRegionType(config.detectRegionType);
    const bool circleDetectRoi = isCircleRegionType(config.detectRegionType);
    const QVector<QPointF> detectPolygonPixels = polygonDetectRoi
            ? normalizedPolygonToPixels(config.detectPolygonNormalized, image.cols, image.rows)
            : QVector<QPointF>();
    const QPointF detectCircleCenterPixels = circleDetectRoi
            ? normalizedCircleCenterToPixels(config, image.cols, image.rows)
            : QPointF();
    const double detectCircleRadiusPixels = circleDetectRoi
            ? normalizedCircleRadiusToPixels(config, image.cols, image.rows)
            : 0.0;
    result.payload.insert(QStringLiteral("roiMode"),
                          polygonDetectRoi ? QStringLiteral("polygon")
                                           : (circleDetectRoi ? QStringLiteral("circle")
                                                              : QStringLiteral("rectangle")));
    result.payload.insert(QStringLiteral("detectPolygonPixels"), pointsToJson(detectPolygonPixels));
    result.payload.insert(QStringLiteral("detectCircleCenterPixels"), pointToJson(detectCircleCenterPixels));
    result.payload.insert(QStringLiteral("detectCircleRadiusPixels"), detectCircleRadiusPixels);
    if (polygonDetectRoi && detectPolygonPixels.size() >= 3)
        result.overlays.append(polygonOverlay(detectPolygonPixels, QStringLiteral("detect_roi")));
    else if (circleDetectRoi && detectCircleRadiusPixels > 0.0)
        result.overlays.append(circleOverlay(detectCircleCenterPixels,
                                             detectCircleRadiusPixels,
                                             QStringLiteral("detect_roi")));
    else
        result.overlays.append(rectOverlay(QRectF(detectRoiPixels), QStringLiteral("detect_roi")));

    cv::Mat detectMat = image(cv::Rect(detectRoiPixels.x(),
                                       detectRoiPixels.y(),
                                       detectRoiPixels.width(),
                                       detectRoiPixels.height())).clone();
    if (detectMat.empty()) {
        BlobPresenceHalconResult errorResult = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                                  QStringLiteral("detect ROI is invalid"),
                                                                  image,
                                                                  config);
        errorResult.elapsedMs = timer.elapsed();
        errorResult.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(errorResult.elapsedMs));
        return errorResult;
    }

    if (!detectMat.isContinuous())
        detectMat = detectMat.clone();

    HalconLibrary library;
    QString loadMessage;
    bool symbolMissing = false;
    if (!library.load(config.halconSoPath, loadMessage, symbolMissing)) {
        result.success = false;
        result.ok = false;
        result.status = symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                      : QStringLiteral("halcon_load_failed");
        result.message = loadMessage;
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), loadMessage);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    HalconCApi *api = &library.api;
    GrayHalconImage detectImage;
    Hobject detectRoiRegion = NO_OBJECTS;
    Hobject detectReducedImage = NO_OBJECTS;
    Hobject thresholdRegion = NO_OBJECTS;
    Hobject lowerRegion = NO_OBJECTS;
    Hobject upperRegion = NO_OBJECTS;
    Hobject connectedRegions = NO_OBJECTS;
    Hobject selectedRegions = NO_OBJECTS;
    Htuple row1Tuple = HTUPLE_INITIALIZER;
    Htuple col1Tuple = HTUPLE_INITIALIZER;
    Htuple row2Tuple = HTUPLE_INITIALIZER;
    Htuple col2Tuple = HTUPLE_INITIALIZER;
    Htuple areaTuple = HTUPLE_INITIALIZER;
    Htuple centerRowTuple = HTUPLE_INITIALIZER;
    Htuple centerColTuple = HTUPLE_INITIALIZER;
    Htuple roiAreaTuple = HTUPLE_INITIALIZER;
    Htuple roiCenterRowTuple = HTUPLE_INITIALIZER;
    Htuple roiCenterColTuple = HTUPLE_INITIALIZER;
    Htuple polygonRowsTuple = HTUPLE_INITIALIZER;
    Htuple polygonColumnsTuple = HTUPLE_INITIALIZER;

    auto halconErrorText = [&](const Herror status) {
        char buffer[1024] = {0};
        if (api->getErrorText && halconStatusOk(api->getErrorText(static_cast<Hlong>(status), buffer))) {
            const QString message = QString::fromUtf8(buffer).trimmed();
            if (!message.isEmpty())
                return message;
        }
        return QStringLiteral("HALCON error code %1").arg(static_cast<qlonglong>(status));
    };

    auto clearObject = [&](Hobject &object) {
        if (halconObjectAllocated(object) && api->clearObj)
            api->clearObj(object);
        object = NO_OBJECTS;
    };

    auto destroyTuple = [&](Htuple &tuple) {
        if ((tuple.num > 0 || tuple.capacity > 0) && api->destroyTuple)
            api->destroyTuple(&tuple);
        tuple = HTUPLE_INITIALIZER;
    };

    auto cleanup = [&]() {
        destroyTuple(polygonColumnsTuple);
        destroyTuple(polygonRowsTuple);
        destroyTuple(centerColTuple);
        destroyTuple(centerRowTuple);
        destroyTuple(areaTuple);
        destroyTuple(roiCenterColTuple);
        destroyTuple(roiCenterRowTuple);
        destroyTuple(roiAreaTuple);
        destroyTuple(col2Tuple);
        destroyTuple(row2Tuple);
        destroyTuple(col1Tuple);
        destroyTuple(row1Tuple);
        clearObject(selectedRegions);
        clearObject(connectedRegions);
        clearObject(upperRegion);
        clearObject(lowerRegion);
        clearObject(thresholdRegion);
        clearObject(detectReducedImage);
        clearObject(detectRoiRegion);
        clearObject(detectImage.grayImage);
        clearObject(detectImage.inputImage);
    };

    auto checkStatus = [&](const Herror status, const QString &stage) {
        if (!halconStatusOk(status)) {
            throw std::pair<QString, QString>(
                    QStringLiteral("BlobPresence HALCON error"),
                    QStringLiteral("%1: %2").arg(stage, halconErrorText(status)));
        }
    };

    auto generateGrayImage = [&](const cv::Mat &mat, GrayHalconImage &halconImage, const QString &stage) {
        const int channels = mat.channels();
        if (channels == 1) {
            checkStatus(api->genImage1(&halconImage.inputImage,
                                       "byte",
                                       mat.cols,
                                       mat.rows,
                                       reinterpret_cast<Hlong>(mat.data)),
                        stage + QStringLiteral(".gen_image1"));
            halconImage.graySource = halconImage.inputImage;
            return;
        }

        const char *colorFormat = channels == 4 ? "bgrx" : "bgr";
        checkStatus(api->genImageInterleaved(&halconImage.inputImage,
                                             reinterpret_cast<Hlong>(mat.data),
                                             colorFormat,
                                             mat.cols,
                                             mat.rows,
                                             0,
                                             "byte",
                                             0,
                                             0,
                                             0,
                                             0,
                                             8,
                                             0),
                    stage + QStringLiteral(".gen_image_interleaved"));
        checkStatus(api->rgb1ToGray(halconImage.inputImage, &halconImage.grayImage),
                    stage + QStringLiteral(".rgb1_to_gray"));
        halconImage.graySource = halconImage.grayImage;
    };

    auto createDoubleArrayTuple = [&](Htuple &tuple, const QVector<double> &values) {
        if (!api->createTuple || !api->setDouble) {
            throw std::pair<QString, QString>(
                    QStringLiteral("BlobPresence HALCON error"),
                    QStringLiteral("HALCON tuple API is unavailable for polygon ROI"));
        }

        api->createTuple(&tuple, static_cast<Hlong>(values.size()));
        for (int index = 0; index < values.size(); ++index)
            api->setDouble(&tuple, values.at(index), static_cast<Hlong>(index));
    };

    try {
        generateGrayImage(detectMat, detectImage, QStringLiteral("detect"));
        Hobject thresholdSource = detectImage.graySource;

        if (polygonDetectRoi) {
            if (detectPolygonPixels.size() < 3)
                throw std::pair<QString, QString>(
                        QStringLiteral("BlobPresence HALCON error"),
                        QStringLiteral("polygon detect ROI requires at least 3 points"));
            if (!api->genRegionPolygon || !api->reduceDomain)
                throw std::pair<QString, QString>(
                        QStringLiteral("BlobPresence HALCON error"),
                        QStringLiteral("HALCON polygon ROI symbols are unavailable"));

            QVector<double> rows;
            QVector<double> columns;
            rows.reserve(detectPolygonPixels.size());
            columns.reserve(detectPolygonPixels.size());
            for (const QPointF &point : detectPolygonPixels) {
                rows.append(qBound(0.0,
                                   point.y() - static_cast<double>(detectRoiPixels.y()),
                                   static_cast<double>(detectMat.rows - 1)));
                columns.append(qBound(0.0,
                                      point.x() - static_cast<double>(detectRoiPixels.x()),
                                      static_cast<double>(detectMat.cols - 1)));
            }

            createDoubleArrayTuple(polygonRowsTuple, rows);
            createDoubleArrayTuple(polygonColumnsTuple, columns);
            checkStatus(api->genRegionPolygon(&detectRoiRegion,
                                              polygonRowsTuple,
                                              polygonColumnsTuple),
                        QStringLiteral("gen_region_polygon.detect_roi"));
            checkStatus(api->reduceDomain(detectImage.graySource,
                                          detectRoiRegion,
                                          &detectReducedImage),
                        QStringLiteral("reduce_domain.detect_polygon_roi"));
            thresholdSource = detectReducedImage;
            result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), true);
            result.payload.insert(QStringLiteral("detectMaskApplied"), true);
        } else if (circleDetectRoi) {
            if (detectCircleRadiusPixels <= 0.0)
                throw std::pair<QString, QString>(
                        QStringLiteral("BlobPresence HALCON error"),
                        QStringLiteral("circle detect ROI is invalid"));
            if (!api->genCircle || !api->reduceDomain)
                throw std::pair<QString, QString>(
                        QStringLiteral("BlobPresence HALCON error"),
                        QStringLiteral("HALCON circle ROI symbols are unavailable"));

            const double localRow = detectCircleCenterPixels.y() - static_cast<double>(detectRoiPixels.y());
            const double localColumn = detectCircleCenterPixels.x() - static_cast<double>(detectRoiPixels.x());
            checkStatus(api->genCircle(&detectRoiRegion,
                                       localRow,
                                       localColumn,
                                       detectCircleRadiusPixels),
                        QStringLiteral("gen_circle.detect_roi"));
            checkStatus(api->reduceDomain(detectImage.graySource,
                                          detectRoiRegion,
                                          &detectReducedImage),
                        QStringLiteral("reduce_domain.detect_circle_roi"));
            thresholdSource = detectReducedImage;
            result.payload.insert(QStringLiteral("circleDetectRoiApplied"), true);
            result.payload.insert(QStringLiteral("detectMaskApplied"), true);
        }

        double roiArea = static_cast<double>(detectRoiPixels.width()) *
                static_cast<double>(detectRoiPixels.height());
        QString areaRateMode = QStringLiteral("bbox_fallback");
        if (halconObjectAllocated(detectRoiRegion)) {
            checkStatus(api->areaCenter(detectRoiRegion,
                                        &roiAreaTuple,
                                        &roiCenterRowTuple,
                                        &roiCenterColTuple),
                        QStringLiteral("area_center.detect_roi"));
            if (roiAreaTuple.num > 0) {
                const double measuredRoiArea = api->getDouble(&roiAreaTuple, 0);
                if (std::isfinite(measuredRoiArea) && measuredRoiArea > 0.0) {
                    roiArea = measuredRoiArea;
                    areaRateMode = QStringLiteral("roi_region_area");
                }
            }
        }

        if (!config.invertRange) {
            checkStatus(api->threshold(thresholdSource,
                                       &thresholdRegion,
                                       static_cast<double>(config.grayMin),
                                       static_cast<double>(config.grayMax)),
                        QStringLiteral("threshold"));
        } else {
            if (config.grayMin > 0) {
                checkStatus(api->threshold(thresholdSource,
                                           &lowerRegion,
                                           0.0,
                                           static_cast<double>(config.grayMin - 1)),
                            QStringLiteral("threshold.lower"));
            } else {
                checkStatus(api->genEmptyRegion(&lowerRegion), QStringLiteral("gen_empty_region.lower"));
            }

            if (config.grayMax < 255) {
                checkStatus(api->threshold(thresholdSource,
                                           &upperRegion,
                                           static_cast<double>(config.grayMax + 1),
                                           255.0),
                            QStringLiteral("threshold.upper"));
            } else {
                checkStatus(api->genEmptyRegion(&upperRegion), QStringLiteral("gen_empty_region.upper"));
            }

            checkStatus(api->union2(lowerRegion, upperRegion, &thresholdRegion),
                        QStringLiteral("union2.invert_range"));
        }

        checkStatus(api->connection(thresholdRegion, &connectedRegions),
                    QStringLiteral("connection"));
        checkStatus(api->selectShape(connectedRegions,
                                     &selectedRegions,
                                     "area",
                                     "and",
                                     static_cast<double>(config.areaMin),
                                     static_cast<double>(config.areaMax)),
                    QStringLiteral("select_shape.area"));

        Hlong objectCount = 0;
        checkStatus(api->countObj(selectedRegions, &objectCount),
                    QStringLiteral("count_obj"));
        const int blobCount = qMax(0, static_cast<int>(objectCount));
        const int countMinUsed = 1;
        const int countMaxUsed = std::numeric_limits<int>::max();
        const bool found = blobCount >= countMinUsed && blobCount <= countMaxUsed;
        const bool ok = config.existOk ? found : !found;

        QVector<QRectF> blobRects;
        QVector<double> blobAreas;
        QVector<QPointF> blobCenters;
        blobRects.reserve(blobCount);
        blobAreas.reserve(blobCount);
        blobCenters.reserve(blobCount);
        double totalArea = 0.0;
        double largestArea = 0.0;
        int largestBlobIndex = -1;

        if (blobCount > 0) {
            checkStatus(api->smallestRectangle1(selectedRegions,
                                                &row1Tuple,
                                                &col1Tuple,
                                                &row2Tuple,
                                                &col2Tuple),
                        QStringLiteral("smallest_rectangle1"));
            checkStatus(api->areaCenter(selectedRegions,
                                        &areaTuple,
                                        &centerRowTuple,
                                        &centerColTuple),
                        QStringLiteral("area_center"));

            const int tupleCount = qMin<int>(qMin<int>(row1Tuple.num, col1Tuple.num),
                                             qMin<int>(row2Tuple.num, col2Tuple.num));
            const int areaCount = qMin<int>(areaTuple.num, tupleCount);
            const int centerCount = qMin<int>(centerRowTuple.num, centerColTuple.num);
            for (int index = 0; index < tupleCount; ++index) {
                const double row1 = api->getDouble(&row1Tuple, index);
                const double col1 = api->getDouble(&col1Tuple, index);
                const double row2 = api->getDouble(&row2Tuple, index);
                const double col2 = api->getDouble(&col2Tuple, index);
                const QRectF blobRect(col1 + detectRoiPixels.x(),
                                      row1 + detectRoiPixels.y(),
                                      qMax(1.0, col2 - col1 + 1.0),
                                      qMax(1.0, row2 - row1 + 1.0));
                blobRects.append(blobRect);
                result.overlays.append(rectOverlay(blobRect, QStringLiteral("blob_bbox"), 1.0));

                double area = 0.0;
                if (index < areaCount)
                    area = api->getDouble(&areaTuple, index);
                blobAreas.append(area);
                totalArea += area;
                if (largestBlobIndex < 0 || area > largestArea) {
                    largestArea = area;
                    largestBlobIndex = index;
                }

                QPointF center(blobRect.center());
                if (index < centerCount) {
                    center = QPointF(api->getDouble(&centerColTuple, index) + detectRoiPixels.x(),
                                     api->getDouble(&centerRowTuple, index) + detectRoiPixels.y());
                }
                blobCenters.append(center);

                const double marker = 4.0;
                result.overlays.append(lineOverlay(QPointF(center.x() - marker, center.y()),
                                                   QPointF(center.x() + marker, center.y()),
                                                   QStringLiteral("blob_center"),
                                                   1.0));
                result.overlays.append(lineOverlay(QPointF(center.x(), center.y() - marker),
                                                   QPointF(center.x(), center.y() + marker),
                                                   QStringLiteral("blob_center"),
                                                   1.0));
                result.overlays.append(textOverlay(QPointF(blobRect.x(), qMax(0.0, blobRect.y() - 14.0)),
                                                   QStringLiteral("area=%1").arg(area, 0, 'f', 0),
                                                   QStringLiteral("blob_area_text"),
                                                   1.0));
            }
        }

        const double averageArea = blobCount > 0
                ? totalArea / static_cast<double>(blobCount)
                : 0.0;
        const double areaRate = roiArea > 0.0 ? totalArea / roiArea : 0.0;
        QString okNgReason;
        if (found) {
            okNgReason = config.existOk
                    ? QStringLiteral("found target, existOk=true")
                    : QStringLiteral("found target, existOk=false");
        } else if (blobCount <= 0) {
            okNgReason = config.existOk
                    ? QStringLiteral("not found target, existOk=true")
                    : QStringLiteral("not found target, existOk=false");
        } else {
            okNgReason = QStringLiteral("blob count out of internal range");
        }

        result.success = true;
        result.ok = ok;
        result.score = found ? 1.0 : 0.0;
        result.count = blobCount;
        result.text = found ? QStringLiteral("found") : QStringLiteral("missing");
        result.status = QStringLiteral("BlobPresence: %1 count=%2")
                .arg(result.text, QString::number(blobCount));
        result.message = result.status;

        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("blobCount"), blobCount);
        result.payload.insert(QStringLiteral("areas"), numbersToJson(blobAreas));
        result.payload.insert(QStringLiteral("centers"), pointsToJson(blobCenters));
        result.payload.insert(QStringLiteral("boundingRects"), rectsToJson(blobRects));
        result.payload.insert(QStringLiteral("blobRects"), rectsToJson(blobRects));
        result.payload.insert(QStringLiteral("blobAreas"), numbersToJson(blobAreas));
        result.payload.insert(QStringLiteral("totalArea"), totalArea);
        result.payload.insert(QStringLiteral("largestArea"), largestArea);
        result.payload.insert(QStringLiteral("averageArea"), averageArea);
        result.payload.insert(QStringLiteral("areaRate"), areaRate);
        result.payload.insert(QStringLiteral("areaRateMode"), areaRateMode);
        result.payload.insert(QStringLiteral("roiEffectiveArea"), roiArea);
        result.payload.insert(QStringLiteral("largestBlobIndex"), largestBlobIndex);
        result.payload.insert(QStringLiteral("countMinUsed"), countMinUsed);
        result.payload.insert(QStringLiteral("countMaxUsed"), countMaxUsed);
        result.payload.insert(QStringLiteral("countRangeSource"), QStringLiteral("internal_default"));
        result.payload.insert(QStringLiteral("okNgReason"), okNgReason);
        result.payload.insert(QStringLiteral("text"), result.text);

        const QPointF countPosition(detectRoiPixels.x() + 4.0, detectRoiPixels.y() + 4.0);
        result.overlays.append(textOverlay(countPosition,
                                           QStringLiteral("count=%1").arg(blobCount),
                                           QStringLiteral("blob_count_text"),
                                           result.score));

        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("timeoutExceeded"),
                              config.timeoutMs > 0 && result.elapsedMs > config.timeoutMs);

        cleanup();
        return result;
    } catch (const std::pair<QString, QString> &errorInfo) {
        cleanup();
        result.success = false;
        result.ok = false;
        result.status = errorInfo.first;
        result.message = errorInfo.second;
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), errorInfo.second);
        result.payload.insert(QStringLiteral("okNgReason"), errorInfo.second);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const std::exception &error) {
        cleanup();
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("BlobPresence HALCON error");
        result.message = QString::fromLocal8Bit(error.what());
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.payload.insert(QStringLiteral("okNgReason"), result.message);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
}
