#include "algorithms/presence/CirclePresenceHalconRunner.h"

#include "algorithms/location/PositionCorrectionHalconTransform.h"
#include "toolcore/PositionCorrectionTransform.h"

#include <HalconC.h>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRect>
#include <QSize>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <exception>
#include <limits>
#include <opencv2/core.hpp>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kMinRoiPixelSize = 8;

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

QJsonArray doublesToJson(const QVector<double> &values)
{
    QJsonArray array;
    for (const double value : values)
        array.append(value);
    return array;
}

QJsonObject circleToJson(const QPointF &center,
                         const double radius,
                         const double area,
                         const double circularity,
                         const QString &polarity,
                         const double strength,
                         const double contrastScore)
{
    QJsonObject json;
    json.insert(QStringLiteral("centerX"), center.x());
    json.insert(QStringLiteral("centerY"), center.y());
    json.insert(QStringLiteral("radius"), radius);
    json.insert(QStringLiteral("area"), area);
    json.insert(QStringLiteral("circularity"), circularity);
    json.insert(QStringLiteral("polarity"), polarity);
    json.insert(QStringLiteral("strength"), strength);
    json.insert(QStringLiteral("contrastScore"), contrastScore);
    return json;
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

double normalizedCircleRadiusToPixels(const CirclePresenceHalconConfig &config,
                                      const int width,
                                      const int height)
{
    const double maxDimension = static_cast<double>(qMax(width, height));
    return qBound(0.0, config.detectCircleRadiusNormalized, 1.0) * maxDimension;
}

QPointF normalizedCircleCenterToPixels(const CirclePresenceHalconConfig &config,
                                       const int width,
                                       const int height)
{
    return QPointF(qBound(0.0, config.detectCircleCenterNormalized.x(), 1.0) * width,
                   qBound(0.0, config.detectCircleCenterNormalized.y(), 1.0) * height);
}

QString normalizedEdgePolarity(const QString &polarity)
{
    const QString key = polarity.trimmed().toLower();
    if (key == QStringLiteral("black_to_white") || key.contains(QStringLiteral("黑到白")))
        return QStringLiteral("black_to_white");
    if (key == QStringLiteral("white_to_black") || key.contains(QStringLiteral("白到黑")))
        return QStringLiteral("white_to_black");
    return QStringLiteral("any");
}

QString normalizedEdgeType(const QString &edgeType)
{
    const QString key = edgeType.trimmed().toLower();
    if (key == QStringLiteral("maximum") || key.contains(QStringLiteral("最大")))
        return QStringLiteral("maximum");
    if (key == QStringLiteral("minimum") || key.contains(QStringLiteral("最小")))
        return QStringLiteral("minimum");
    if (key == QStringLiteral("manual") || key.contains(QStringLiteral("手动")))
        return QStringLiteral("manual");
    return QStringLiteral("strongest");
}

QRect normalizedRoiToPixels(const QRectF &sourceRoi,
                            const int width,
                            const int height,
                            bool *tooSmall)
{
    if (tooSmall)
        *tooSmall = false;

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
    if (roiWidth < kMinRoiPixelSize || roiHeight < kMinRoiPixelSize) {
        if (tooSmall)
            *tooSmall = true;
        return QRect();
    }

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

double circularityMinFor(const int roundness, const int sensitivity)
{
    const double requested = qBound(0.0, static_cast<double>(roundness) / 100.0, 1.0);
    const double relaxed = requested - qBound(0.0, static_cast<double>(sensitivity) / 100.0, 1.0) * 0.20;
    return qBound(0.20, relaxed, 0.98);
}

double circularityRelaxFromSensitivityFor(const int sensitivity)
{
    return qBound(0.0, static_cast<double>(sensitivity) / 100.0, 1.0) * 0.20;
}

int thresholdMarginFor(const int sensitivity)
{
    return qBound(2, 28 - qRound(static_cast<double>(sensitivity) * 0.22), 28);
}

double edgeThresholdForPayload(const int sensitivity)
{
    return qBound(8.0, 80.0 - static_cast<double>(sensitivity) * 0.55, 80.0);
}

QPointF countOverlayPosition(const QRect &roi, const QSize &imageSize)
{
    if (roi.y() >= 20)
        return QPointF(roi.x() + 4.0, roi.y() - 18.0);
    if (roi.bottom() + 24 < imageSize.height())
        return QPointF(roi.x() + 4.0, roi.bottom() + 8.0);
    return QPointF(roi.x() + 4.0, roi.y() + 4.0);
}

void fillPayload(CirclePresenceHalconResult &result,
                 const cv::Mat &image,
                 const CirclePresenceHalconConfig &config)
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
    result.payload.insert(QStringLiteral("sensitivity"), qBound(0, config.sensitivity, 100));
    result.payload.insert(QStringLiteral("roundness"), qBound(0, config.roundness, 100));
    result.payload.insert(QStringLiteral("edgePolarity"), normalizedEdgePolarity(config.edgePolarity));
    result.payload.insert(QStringLiteral("edgeType"), normalizedEdgeType(config.edgeType));
    result.payload.insert(QStringLiteral("judgeBasis"), QStringLiteral("presence"));
    result.payload.insert(QStringLiteral("existOk"), config.existOk);
    result.payload.insert(QStringLiteral("timeoutMs"), config.timeoutMs);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("circle_region"));
    result.payload.insert(QStringLiteral("mode"), QStringLiteral("region"));
    result.payload.insert(QStringLiteral("edgePolarityApplied"), true);
    result.payload.insert(QStringLiteral("edgeTypeApplied"), false);
    result.payload.insert(QStringLiteral("roundnessApplied"), true);
    result.payload.insert(QStringLiteral("caliperModeAvailable"), false);
    result.payload.insert(QStringLiteral("caliperModeApplied"), false);
    result.payload.insert(QStringLiteral("countMinUsed"), 1);
    result.payload.insert(QStringLiteral("countMaxUsed"), std::numeric_limits<int>::max());
    result.payload.insert(QStringLiteral("countRangeSource"), QStringLiteral("internal_default"));
    result.payload.insert(QStringLiteral("positionCorrectionRequested"),
                      config.positionCorrection.requested);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"),
                    config.positionCorrection.applied);
    result.payload.insert(QStringLiteral("positionCorrectionSourceId"),
                    config.positionCorrection.sourceId);
    result.payload.insert(QStringLiteral("positionCorrectionReason"),
                    config.positionCorrection.applied
                    ? QStringLiteral("applied")
                    : QStringLiteral("not_requested"));
    result.payload.insert(QStringLiteral("referenceToRunHomMat2D"),
                    doublesToJson(
                        config.positionCorrection.referenceToRunHomMat2D));
    result.payload.insert(QStringLiteral("referenceScale"),
                    config.positionCorrection.referenceScale);
    result.payload.insert(QStringLiteral("runScale"),
                    config.positionCorrection.runScale);
    result.payload.insert(QStringLiteral("scaleRatio"),
                    config.positionCorrection.scaleRatio);
    result.payload.insert(QStringLiteral("showPositionCorrectionMatchContour"),
                    config.positionCorrection.showMatchContour);
    result.payload.insert(QStringLiteral("maskApplied"), false);
    result.payload.insert(QStringLiteral("detectMaskApplied"), false);
    result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), false);
    result.payload.insert(QStringLiteral("circleDetectRoiApplied"), false);
    result.payload.insert(QStringLiteral("maskReason"), QStringLiteral("not implemented"));
}

CirclePresenceHalconResult makeParameterError(const QString &status,
                                              const QString &error,
                                              const cv::Mat &image,
                                              const CirclePresenceHalconConfig &config)
{
    CirclePresenceHalconResult result;
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
    using SetStringFn = void (*)(Htuple *, const char *, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GenImage1Fn = Herror (*)(Hobject *, const char *, Hlong, Hlong, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Rgb1ToGrayFn = Herror (*)(const Hobject, Hobject *);
    using BinaryThresholdFn = Herror (*)(const Hobject, Hobject *, const char *, const char *, Hlong *);
    using ThresholdFn = Herror (*)(const Hobject, Hobject *, double, double);
    using GenRegionPolygonFilledFn = Herror (*)(Hobject *, const Htuple, const Htuple);
    using GenCircleFn = Herror (*)(Hobject *, double, double, double);
    using GenRectangle1Fn = Herror (*)(Hobject *, double, double, double, double);
    using AffineTransRegionFn = Herror (*)(const Hobject, Hobject *,const Htuple, const Htuple);
    using ClipRegionFn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple, const Htuple, const Htuple);
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using ConnectionFn = Herror (*)(const Hobject, Hobject *);
    using SelectShapeFn = Herror (*)(const Hobject, Hobject *, const char *, const char *, double, double);
    using CountObjFn = Herror (*)(const Hobject, Hlong *);
    using TSmallestCircleFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using TAreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using TCircularityFn = Herror (*)(const Hobject, Htuple *);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GenImage1Fn genImage1 = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Rgb1ToGrayFn rgb1ToGray = nullptr;
    BinaryThresholdFn binaryThreshold = nullptr;
    ThresholdFn threshold = nullptr;
    GenRegionPolygonFilledFn genRegionPolygonFilled = nullptr;
    GenCircleFn genCircle = nullptr;
    GenRectangle1Fn genRectangle1 = nullptr;
    AffineTransRegionFn affineTransRegion = nullptr;
    ClipRegionFn clipRegion = nullptr;
    ReduceDomainFn reduceDomain = nullptr;
    ConnectionFn connection = nullptr;
    SelectShapeFn selectShape = nullptr;
    CountObjFn countObj = nullptr;
    TSmallestCircleFn smallestCircle = nullptr;
    TAreaCenterFn areaCenter = nullptr;
    TCircularityFn circularity = nullptr;
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
        resolveOptional(m_handle, api.setString, "F_set_s");
        resolveOptional(m_handle,
                        api.genRegionPolygonFilled,
                        "T_gen_region_polygon_filled");
        resolveOptional(m_handle, api.genCircle, "gen_circle");
        resolveOptional(m_handle, api.genRectangle1, "gen_rectangle1");
        resolveOptional(m_handle, api.affineTransRegion, "T_affine_trans_region");
        resolveOptional(m_handle, api.clipRegion, "T_clip_region");
        resolveOptional(m_handle, api.reduceDomain, "reduce_domain");

        if (!resolveRequired(m_handle, api.getErrorText, "get_error_text", errorMessage) ||
            !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
            !resolveRequired(m_handle, api.genImage1, "gen_image1", errorMessage) ||
            !resolveRequired(m_handle, api.genImageInterleaved, "gen_image_interleaved", errorMessage) ||
            !resolveRequired(m_handle, api.rgb1ToGray, "rgb1_to_gray", errorMessage) ||
            !resolveRequired(m_handle, api.binaryThreshold, "binary_threshold", errorMessage) ||
            !resolveRequired(m_handle, api.threshold, "threshold", errorMessage) ||
            !resolveRequired(m_handle, api.connection, "connection", errorMessage) ||
            !resolveRequired(m_handle, api.selectShape, "select_shape", errorMessage) ||
            !resolveRequired(m_handle, api.countObj, "count_obj", errorMessage) ||
            !resolveRequired(m_handle, api.smallestCircle, "T_smallest_circle", errorMessage) ||
            !resolveRequired(m_handle, api.areaCenter, "T_area_center", errorMessage) ||
            !resolveRequired(m_handle, api.circularity, "T_circularity", errorMessage) ||
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

PositionCorrectionHalconRegionApi positionCorrectionRegionApi(
        const HalconCApi &api)
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

struct GrayHalconImage
{
    Hobject inputImage = NO_OBJECTS;
    Hobject grayImage = NO_OBJECTS;
    Hobject graySource = NO_OBJECTS;
};

struct ThresholdPass
{
    QString polarity;
    const char *lightDark = "light";
    bool bright = true;
};

struct DetectedCircle
{
    QPointF center;
    double radius = 0.0;
    double area = 0.0;
    double circularity = 0.0;
    double contrastScore = 0.0;
    double strength = 0.0;
    QString polarity;
    int rawIndex = -1;
};

bool isDuplicateCircle(const QVector<DetectedCircle> &circles,
                       const QPointF &center,
                       const double radius)
{
    for (const DetectedCircle &circle : circles) {
        const double dx = circle.center.x() - center.x();
        const double dy = circle.center.y() - center.y();
        const double centerTolerance = qMax(3.0, qMin(circle.radius, radius) * 0.35);
        const double radiusTolerance = qMax(3.0, qMin(circle.radius, radius) * 0.35);
        if (std::sqrt(dx * dx + dy * dy) <= centerTolerance &&
            std::abs(circle.radius - radius) <= radiusTolerance) {
            return true;
        }
    }

    return false;
}

} // namespace

CirclePresenceHalconResult CirclePresenceHalconRunner::run(
        const cv::Mat &image,
        const CirclePresenceHalconConfig &config)
{
    QElapsedTimer timer;
    timer.start();

    if (image.empty()) {
        CirclePresenceHalconResult result = makeParameterError(QStringLiteral("image_empty"),
                                                               QStringLiteral("input image is empty"),
                                                               image,
                                                               config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isValidNormalizedRoi(config.roiNormalized)) {
        CirclePresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                               QStringLiteral("detect ROI is invalid"),
                                                               image,
                                                               config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isSupportedDetectRegionType(config.detectRegionType)) {
        CirclePresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_detect_roi"),
                                                               QStringLiteral("detect ROI shape is not supported"),
                                                               image,
                                                               config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("todo"), QStringLiteral("mask/free/circle/polygon ROI is not implemented"));
        return result;
    }

    if (isPolygonRegionType(config.detectRegionType) &&
        config.detectPolygonNormalized.size() < 3) {
        CirclePresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_polygon"),
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
        CirclePresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_circle"),
                                                               QStringLiteral("circle detect ROI is invalid"),
                                                               image,
                                                               config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (image.depth() != CV_8U ||
        (image.channels() != 1 && image.channels() != 3 && image.channels() != 4)) {
        CirclePresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_image_type"),
                                                               QStringLiteral("CirclePresence supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
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
        CirclePresenceHalconResult result = makeParameterError(QStringLiteral("halcon_so_not_found"),
                                                               QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                                                               .arg(config.halconSoPath, triedPaths),
                                                               image,
                                                               config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    bool roiTooSmall = false;
    const QRect detectRoiPixels = normalizedRoiToPixels(config.roiNormalized,
                                                        image.cols,
                                                        image.rows,
                                                        &roiTooSmall);
    if (detectRoiPixels.isEmpty()) {
        CirclePresenceHalconResult result = makeParameterError(roiTooSmall
                                                               ? QStringLiteral("detect_roi_too_small")
                                                               : QStringLiteral("invalid_detect_roi"),
                                                               roiTooSmall
                                                               ? QStringLiteral("detect ROI is too small")
                                                               : QStringLiteral("detect ROI is invalid"),
                                                               image,
                                                               config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    CirclePresenceHalconResult result;
    fillPayload(result, image, config);

    const int sensitivityUsed = qBound(0, config.sensitivity, 100);
    const int roundnessUsed = qBound(0, config.roundness, 100);
    const double circularityRequested = qBound(0.0, static_cast<double>(roundnessUsed) / 100.0, 1.0);
    const double circularityRelaxFromSensitivity = circularityRelaxFromSensitivityFor(sensitivityUsed);
    const int thresholdMarginUsed = thresholdMarginFor(sensitivityUsed);
    const double circularityMinUsed = circularityMinFor(roundnessUsed, sensitivityUsed);
    const double edgeThresholdUsed = edgeThresholdForPayload(sensitivityUsed);
    const QString edgeTypeRequested = normalizedEdgeType(config.edgeType);
    const bool manualEdgeTypeUnsupported = edgeTypeRequested == QStringLiteral("manual");
    const QString edgeTypeUsed = manualEdgeTypeUnsupported ? QStringLiteral("strongest") : edgeTypeRequested;
    const bool correctionApplied = config.positionCorrection.applied;
    const double correctionScale = correctionApplied ? config.positionCorrection.scaleRatio : 1.0;
    const double radiusMinUsed = 3.0;
    const double radiusMaxUsed = qMax(radiusMinUsed,
            static_cast<double>(
                qMin(detectRoiPixels.width(),detectRoiPixels.height()))
                / 2.0 * correctionScale);
    const double areaMinUsed = kPi * radiusMinUsed * radiusMinUsed;
    const double areaMaxUsed = kPi * radiusMaxUsed * radiusMaxUsed;
    const int countMinUsed = 1;
    const int countMaxUsed = std::numeric_limits<int>::max();

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
    result.payload.insert(QStringLiteral("sensitivityUsed"), sensitivityUsed);
    result.payload.insert(QStringLiteral("thresholdMarginUsed"), thresholdMarginUsed);
    result.payload.insert(QStringLiteral("edgeThresholdUsed"), edgeThresholdUsed);
    result.payload.insert(QStringLiteral("roundnessCircularityRequested"), circularityRequested);
    result.payload.insert(QStringLiteral("circularityRelaxFromSensitivity"), circularityRelaxFromSensitivity);
    result.payload.insert(QStringLiteral("radiusMinUsed"), radiusMinUsed);
    result.payload.insert(QStringLiteral("radiusMaxUsed"), radiusMaxUsed);
    result.payload.insert(QStringLiteral("radiusSource"), QStringLiteral("internal_default"));
    result.payload.insert(QStringLiteral("areaMinUsed"), areaMinUsed);
    result.payload.insert(QStringLiteral("areaMaxUsed"), areaMaxUsed);
    result.payload.insert(QStringLiteral("circularityMinUsed"), circularityMinUsed);
    result.payload.insert(QStringLiteral("countMinUsed"), countMinUsed);
    result.payload.insert(QStringLiteral("countMaxUsed"), countMaxUsed);
    result.payload.insert(QStringLiteral("countRangeSource"), QStringLiteral("internal_default"));
    result.payload.insert(QStringLiteral("mode"), QStringLiteral("region"));
    result.payload.insert(QStringLiteral("caliperModeAvailable"), false);
    result.payload.insert(QStringLiteral("caliperModeApplied"), false);
    result.payload.insert(QStringLiteral("countRule"), QStringLiteral("presence_only"));
    result.payload.insert(QStringLiteral("scoreThresholdApplied"), false);
    result.payload.insert(QStringLiteral("edgeTypeRequested"), edgeTypeRequested);
    result.payload.insert(QStringLiteral("edgeTypeUsed"), edgeTypeUsed);
    result.payload.insert(QStringLiteral("edgeTypeApplied"), true);
    result.payload.insert(QStringLiteral("manualUnsupported"), manualEdgeTypeUnsupported);
    ToolOverlay detectOverlay;
    if (polygonDetectRoi && detectPolygonPixels.size() >= 3) {
        detectOverlay = polygonOverlay(detectPolygonPixels,
                                    QStringLiteral("detect_roi"));
    } else if (circleDetectRoi && detectCircleRadiusPixels > 0.0) {
        detectOverlay = circleOverlay(detectCircleCenterPixels,
                                    detectCircleRadiusPixels,
                                    QStringLiteral("detect_roi"));
    } else {
        detectOverlay = rectOverlay(QRectF(detectRoiPixels),
                                    QStringLiteral("detect_roi"));
    }

    if (correctionApplied) {
        detectOverlay = PositionCorrectionTransform::transformOverlay(
                    detectOverlay,
                    config.positionCorrection.referenceToRunHomMat2D);
        detectOverlay.extra.insert(
                    QStringLiteral("positionCorrectionSourceId"),
                    config.positionCorrection.sourceId);
    }

    detectOverlay.extra.insert(QStringLiteral("role"),
                            QStringLiteral("detect_roi"));
    result.overlays.append(detectOverlay);

    if (correctionApplied && config.positionCorrection.showMatchContour) {
        result.overlays += PositionCorrectionTransform::matchContourOverlays(
                    config.positionCorrection.matchContours,
                    config.positionCorrection.sourceId);
    }

    if (correctionApplied) {
        result.overlays += PositionCorrectionTransform::matchOriginOverlays(
                    config.positionCorrection.matchOrigins,
                    config.positionCorrection.sourceId);
    }
    cv::Mat detectMat = correctionApplied
                        ? image.clone()
                        : image(cv::Rect(detectRoiPixels.x(),
                                        detectRoiPixels.y(),
                                        detectRoiPixels.width(),
                                        detectRoiPixels.height())).clone();
    if (detectMat.empty()) {
        CirclePresenceHalconResult errorResult = makeParameterError(QStringLiteral("invalid_detect_roi"),
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
    Hobject transformedDetectRoiRegion = NO_OBJECTS;
    Hobject clippedDetectRoiRegion = NO_OBJECTS;
    Hobject detectReducedImage = NO_OBJECTS;
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
        clearObject(detectReducedImage);
        clearObject(clippedDetectRoiRegion);
        clearObject(transformedDetectRoiRegion);
        clearObject(detectRoiRegion);
        clearObject(detectImage.grayImage);
        clearObject(detectImage.inputImage);
    };

    auto checkStatus = [&](const Herror status, const QString &stage) {
        if (!halconStatusOk(status)) {
            throw std::pair<QString, QString>(
                    QStringLiteral("CirclePresence HALCON error"),
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
                    QStringLiteral("CirclePresence HALCON error"),
                    QStringLiteral("HALCON tuple API is unavailable for polygon ROI"));
        }

        api->createTuple(&tuple, static_cast<Hlong>(values.size()));
        for (int index = 0; index < values.size(); ++index)
            api->setDouble(&tuple, values.at(index), static_cast<Hlong>(index));
    };

    const QString edgePolarityUsed = normalizedEdgePolarity(config.edgePolarity);
    QVector<ThresholdPass> passes;
    if (edgePolarityUsed == QStringLiteral("white_to_black")) {
        passes.append(ThresholdPass{QStringLiteral("white_to_black"), "light", true});
    } else if (edgePolarityUsed == QStringLiteral("black_to_white")) {
        passes.append(ThresholdPass{QStringLiteral("black_to_white"), "dark", false});
    } else {
        passes.append(ThresholdPass{QStringLiteral("white_to_black"), "light", true});
        passes.append(ThresholdPass{QStringLiteral("black_to_white"), "dark", false});
    }

    QVector<DetectedCircle> detectedCircles;
    QJsonArray thresholdPassesJson;
    double thresholdLowUsed = 255.0;
    double thresholdHighUsed = 0.0;
    Hobject thresholdSource = NO_OBJECTS;

    auto runThresholdPass = [&](const ThresholdPass &pass) {
        Hobject binaryRegion = NO_OBJECTS;
        Hobject thresholdRegion = NO_OBJECTS;
        Hobject connectedRegions = NO_OBJECTS;
        Hobject areaRegions = NO_OBJECTS;
        Hobject selectedRegions = NO_OBJECTS;
        Htuple circleRowTuple = HTUPLE_INITIALIZER;
        Htuple circleColTuple = HTUPLE_INITIALIZER;
        Htuple radiusTuple = HTUPLE_INITIALIZER;
        Htuple areaTuple = HTUPLE_INITIALIZER;
        Htuple areaRowTuple = HTUPLE_INITIALIZER;
        Htuple areaColTuple = HTUPLE_INITIALIZER;
        Htuple circularityTuple = HTUPLE_INITIALIZER;

        auto cleanupPass = [&]() {
            destroyTuple(circularityTuple);
            destroyTuple(areaColTuple);
            destroyTuple(areaRowTuple);
            destroyTuple(areaTuple);
            destroyTuple(radiusTuple);
            destroyTuple(circleColTuple);
            destroyTuple(circleRowTuple);
            clearObject(selectedRegions);
            clearObject(areaRegions);
            clearObject(connectedRegions);
            clearObject(thresholdRegion);
            clearObject(binaryRegion);
        };

        try {
            Hlong binaryThresholdUsed = 0;
            checkStatus(api->binaryThreshold(thresholdSource,
                                             &binaryRegion,
                                             "max_separability",
                                             pass.lightDark,
                                             &binaryThresholdUsed),
                        QStringLiteral("binary_threshold.%1").arg(pass.polarity));

            const int thresholdCenter = qBound(0, static_cast<int>(binaryThresholdUsed), 255);
            const double low = pass.bright
                    ? qBound(0.0, static_cast<double>(thresholdCenter + thresholdMarginUsed), 255.0)
                    : 0.0;
            const double high = pass.bright
                    ? 255.0
                    : qBound(0.0, static_cast<double>(thresholdCenter + thresholdMarginUsed), 255.0);

            thresholdLowUsed = qMin(thresholdLowUsed, low);
            thresholdHighUsed = qMax(thresholdHighUsed, high);

            QJsonObject passJson;
            passJson.insert(QStringLiteral("polarity"), pass.polarity);
            passJson.insert(QStringLiteral("lightDark"), QString::fromLatin1(pass.lightDark));
            passJson.insert(QStringLiteral("binaryThresholdUsed"), static_cast<int>(binaryThresholdUsed));
            passJson.insert(QStringLiteral("thresholdLowUsed"), low);
            passJson.insert(QStringLiteral("thresholdHighUsed"), high);

            checkStatus(api->threshold(thresholdSource,
                                       &thresholdRegion,
                                       low,
                                       high),
                        QStringLiteral("threshold.%1").arg(pass.polarity));
            checkStatus(api->connection(thresholdRegion, &connectedRegions),
                        QStringLiteral("connection.%1").arg(pass.polarity));
            checkStatus(api->selectShape(connectedRegions,
                                         &areaRegions,
                                         "area",
                                         "and",
                                         areaMinUsed,
                                         areaMaxUsed),
                        QStringLiteral("select_shape.area.%1").arg(pass.polarity));
            Hlong areaCandidateCount = 0;
            checkStatus(api->countObj(areaRegions, &areaCandidateCount),
                        QStringLiteral("count_obj.area.%1").arg(pass.polarity));
            passJson.insert(QStringLiteral("areaCandidateCount"), static_cast<int>(areaCandidateCount));
            checkStatus(api->selectShape(areaRegions,
                                         &selectedRegions,
                                         "circularity",
                                         "and",
                                         circularityMinUsed,
                                         1.0),
                        QStringLiteral("select_shape.circularity.%1").arg(pass.polarity));

            Hlong objectCount = 0;
            checkStatus(api->countObj(selectedRegions, &objectCount),
                        QStringLiteral("count_obj.%1").arg(pass.polarity));
            passJson.insert(QStringLiteral("selectedCount"), static_cast<int>(objectCount));
            passJson.insert(QStringLiteral("circularityRejectedCount"),
                            qMax(0, static_cast<int>(areaCandidateCount - objectCount)));

            if (objectCount > 0) {
                checkStatus(api->smallestCircle(selectedRegions,
                                                &circleRowTuple,
                                                &circleColTuple,
                                                &radiusTuple),
                            QStringLiteral("smallest_circle.%1").arg(pass.polarity));
                checkStatus(api->areaCenter(selectedRegions,
                                            &areaTuple,
                                            &areaRowTuple,
                                            &areaColTuple),
                            QStringLiteral("area_center.%1").arg(pass.polarity));
                checkStatus(api->circularity(selectedRegions,
                                             &circularityTuple),
                            QStringLiteral("circularity.%1").arg(pass.polarity));

                const int tupleCount = qMin<int>(qMin<int>(circleRowTuple.num, circleColTuple.num),
                                                 radiusTuple.num);
                for (int index = 0; index < tupleCount; ++index) {
                    const double localRow = api->getDouble(&circleRowTuple, index);
                    const double localCol = api->getDouble(&circleColTuple, index);
                    const double radius = api->getDouble(&radiusTuple, index);
                    if (radius < radiusMinUsed || radius > radiusMaxUsed)
                        continue;

                    const double coordinateOffsetX = correctionApplied ? 0.0 : detectRoiPixels.x();
                    const double coordinateOffsetY = correctionApplied ? 0.0 : detectRoiPixels.y();
                    const QPointF center(localCol + coordinateOffsetX,
                                            localRow + coordinateOffsetY);
                    if (isDuplicateCircle(detectedCircles, center, radius))
                        continue;

                    DetectedCircle circle;
                    circle.center = center;
                    circle.radius = radius;
                    circle.area = index < areaTuple.num ? api->getDouble(&areaTuple, index) : 0.0;
                    circle.circularity = index < circularityTuple.num
                            ? api->getDouble(&circularityTuple, index)
                            : 0.0;
                    const double areaScore = qBound(0.0,
                                                    circle.area / qMax(1.0, areaMaxUsed),
                                                    1.0);
                    circle.contrastScore = qBound(0.0,
                                                   std::abs(static_cast<double>(binaryThresholdUsed) - 127.5) / 127.5,
                                                   1.0);
                    circle.strength = qBound(0.0,
                                             circle.circularity * 0.60 +
                                             areaScore * 0.25 +
                                             circle.contrastScore * 0.15,
                                             1.0);
                    circle.polarity = pass.polarity;
                    circle.rawIndex = detectedCircles.size();
                    detectedCircles.append(circle);
                }
            }

            passJson.insert(QStringLiteral("acceptedTotalAfterPass"), detectedCircles.size());
            thresholdPassesJson.append(passJson);
            cleanupPass();
        } catch (...) {
            cleanupPass();
            throw;
        }
    };

    try {
        generateGrayImage(detectMat, detectImage, QStringLiteral("detect"));
        thresholdSource = detectImage.graySource;

        if (correctionApplied) {
            if (!api->reduceDomain
                    || !api->affineTransRegion
                    || !api->clipRegion
                    || !api->setString) {
                throw std::pair<QString, QString>(
                        QStringLiteral("CirclePresence HALCON error"),
                        QStringLiteral(
                            "HALCON position-correction ROI symbols are unavailable"));
            }
            if (polygonDetectRoi) {
                if (detectPolygonPixels.size() < 3
                        || !api->genRegionPolygonFilled) {
                    throw std::pair<QString, QString>(
                            QStringLiteral("CirclePresence HALCON error"),
                            QStringLiteral(
                                "polygon detect ROI requires HALCON polygon support"));
                }

                QVector<double> rows;
                QVector<double> columns;
                rows.reserve(detectPolygonPixels.size());
                columns.reserve(detectPolygonPixels.size());

                for (const QPointF &point : detectPolygonPixels) {
                    rows.append(point.y());
                    columns.append(point.x());
                }
                if (!rows.isEmpty()
                        && (rows.first() != rows.last()
                            || columns.first() != columns.last())) {
                    rows.append(rows.first());
                    columns.append(columns.first());
                }

                createDoubleArrayTuple(polygonRowsTuple, rows);
                createDoubleArrayTuple(polygonColumnsTuple, columns);
                checkStatus(api->genRegionPolygonFilled(
                                &detectRoiRegion,
                                polygonRowsTuple,
                                polygonColumnsTuple),
                            QStringLiteral(
                                "gen_region_polygon_filled.reference_roi"));
            } else if (circleDetectRoi) {
                if (detectCircleRadiusPixels <= 0.0 || !api->genCircle) {
                    throw std::pair<QString, QString>(
                            QStringLiteral("CirclePresence HALCON error"),
                            QStringLiteral(
                                "circle detect ROI requires HALCON circle support"));
                }

                checkStatus(api->genCircle(
                                &detectRoiRegion,
                                detectCircleCenterPixels.y(),
                                detectCircleCenterPixels.x(),
                                detectCircleRadiusPixels),
                            QStringLiteral("gen_circle.reference_roi"));
            } else {
                if (!api->genRectangle1) {
                    throw std::pair<QString, QString>(
                            QStringLiteral("CirclePresence HALCON error"),
                            QStringLiteral(
                                "HALCON rectangle ROI symbol is unavailable"));
                }

                checkStatus(api->genRectangle1(
                                &detectRoiRegion,
                                detectRoiPixels.top(),
                                detectRoiPixels.left(),
                                detectRoiPixels.bottom(),
                                detectRoiPixels.right()),
                            QStringLiteral("gen_rectangle1.reference_roi"));
            }
            const PositionCorrectionHalconTransformResult transformed =
                PositionCorrectionHalconTransform::transformAndClipRegion(
                positionCorrectionRegionApi(*api),
                detectRoiRegion,
                &transformedDetectRoiRegion,
                &clippedDetectRoiRegion,
                config.positionCorrection.referenceToRunHomMat2D,
                image.cols,
                image.rows);

            if (!transformed.success) {
                if (transformed.halconStatus != H_MSG_OK) {
                    checkStatus(
                                transformed.halconStatus,
                                QStringLiteral("position_correction.%1")
                                .arg(transformed.operation));
                }

                throw std::pair<QString, QString>(
                        transformed.status,
                        QStringLiteral(
                            "Position-corrected Circle ROI is invalid (%1)")
                        .arg(transformed.operation));
            }

            if (transformed.area <= 0.0) {
                throw std::pair<QString, QString>(
                        QStringLiteral("corrected_roi_out_of_image"),
                        QStringLiteral(
                            "Position-corrected Circle ROI is outside the image"));
            }

            checkStatus(api->reduceDomain(
                            detectImage.graySource,
                            clippedDetectRoiRegion,
                            &detectReducedImage),
                        QStringLiteral("reduce_domain.corrected_roi"));

            thresholdSource = detectReducedImage;

            result.payload.insert(QStringLiteral("correctedRoiArea"),
                                transformed.area);
            result.payload.insert(QStringLiteral("detectMaskApplied"), true);
            result.payload.insert(QStringLiteral("detectRoiRegionApplied"), true);

            if (polygonDetectRoi)
                result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), true);
            else if (circleDetectRoi)
                result.payload.insert(QStringLiteral("circleDetectRoiApplied"), true);
        } else if (polygonDetectRoi) {
            if (detectPolygonPixels.size() < 3)
                throw std::pair<QString, QString>(
                        QStringLiteral("CirclePresence HALCON error"),
                        QStringLiteral("polygon detect ROI requires at least 3 points"));
            if (!api->genRegionPolygonFilled || !api->reduceDomain)
                throw std::pair<QString, QString>(
                        QStringLiteral("CirclePresence HALCON error"),
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
            if (!rows.isEmpty()
                    && (rows.first() != rows.last()
                        || columns.first() != columns.last())) {
                rows.append(rows.first());
                columns.append(columns.first());
            }

            createDoubleArrayTuple(polygonRowsTuple, rows);
            createDoubleArrayTuple(polygonColumnsTuple, columns);
            checkStatus(api->genRegionPolygonFilled(
                            &detectRoiRegion,
                            polygonRowsTuple,
                            polygonColumnsTuple),
                        QStringLiteral(
                            "gen_region_polygon_filled.detect_roi"));
            checkStatus(api->reduceDomain(detectImage.graySource,
                                          detectRoiRegion,
                                          &detectReducedImage),
                        QStringLiteral("reduce_domain.detect_polygon_roi"));
            thresholdSource = detectReducedImage;
            result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), true);
            result.payload.insert(QStringLiteral("detectMaskApplied"), true);
            result.payload.insert(QStringLiteral("detectRoiRegionApplied"), true);
            result.payload.insert(QStringLiteral("maskApplied"), false);
            result.payload.insert(QStringLiteral("maskReason"), QStringLiteral("not implemented"));
        } else if (circleDetectRoi) {
            if (detectCircleRadiusPixels <= 0.0)
                throw std::pair<QString, QString>(
                        QStringLiteral("CirclePresence HALCON error"),
                        QStringLiteral("circle detect ROI is invalid"));
            if (!api->genCircle || !api->reduceDomain)
                throw std::pair<QString, QString>(
                        QStringLiteral("CirclePresence HALCON error"),
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
            result.payload.insert(QStringLiteral("detectRoiRegionApplied"), true);
            result.payload.insert(QStringLiteral("maskApplied"), false);
            result.payload.insert(QStringLiteral("maskReason"), QStringLiteral("not implemented"));
        }

        for (const ThresholdPass &pass : passes)
            runThresholdPass(pass);

        if (thresholdLowUsed > thresholdHighUsed) {
            thresholdLowUsed = 0.0;
            thresholdHighUsed = 0.0;
        }

        QJsonArray rawCirclesArray;
        for (const DetectedCircle &circle : detectedCircles) {
            rawCirclesArray.append(circleToJson(circle.center,
                                                circle.radius,
                                                circle.area,
                                                circle.circularity,
                                                circle.polarity,
                                                circle.strength,
                                                circle.contrastScore));
        }

        QVector<DetectedCircle> finalCircles = detectedCircles;
        QString edgeTypeSelectionMetric = QStringLiteral("strength");
        if (edgeTypeUsed == QStringLiteral("maximum")) {
            edgeTypeSelectionMetric = QStringLiteral("radius_max");
            std::sort(finalCircles.begin(), finalCircles.end(), [](const DetectedCircle &a, const DetectedCircle &b) {
                return a.radius > b.radius;
            });
            if (finalCircles.size() > 1)
                finalCircles.resize(1);
        } else if (edgeTypeUsed == QStringLiteral("minimum")) {
            edgeTypeSelectionMetric = QStringLiteral("radius_min");
            std::sort(finalCircles.begin(), finalCircles.end(), [](const DetectedCircle &a, const DetectedCircle &b) {
                return a.radius < b.radius;
            });
            if (finalCircles.size() > 1)
                finalCircles.resize(1);
        } else {
            std::sort(finalCircles.begin(), finalCircles.end(), [](const DetectedCircle &a, const DetectedCircle &b) {
                if (!qFuzzyCompare(a.strength, b.strength))
                    return a.strength > b.strength;
                if (!qFuzzyCompare(a.circularity, b.circularity))
                    return a.circularity > b.circularity;
                return a.radius > b.radius;
            });
            if (finalCircles.size() > 1)
                finalCircles.resize(1);
        }

        QJsonArray circlesArray;
        for (const DetectedCircle &circle : finalCircles) {
            circlesArray.append(circleToJson(circle.center,
                                             circle.radius,
                                             circle.area,
                                             circle.circularity,
                                             circle.polarity,
                                             circle.strength,
                                             circle.contrastScore));

            result.overlays.append(circleOverlay(circle.center,
                                                 circle.radius,
                                                 QStringLiteral("circle"),
                                                 circle.strength));

            const double crossRadius = qBound(4.0, circle.radius * 0.25, 16.0);
            result.overlays.append(lineOverlay(QPointF(circle.center.x() - crossRadius, circle.center.y()),
                                               QPointF(circle.center.x() + crossRadius, circle.center.y()),
                                               QStringLiteral("circle_center"),
                                               circle.strength));
            result.overlays.append(lineOverlay(QPointF(circle.center.x(), circle.center.y() - crossRadius),
                                               QPointF(circle.center.x(), circle.center.y() + crossRadius),
                                               QStringLiteral("circle_center"),
                                               circle.strength));
            result.overlays.append(textOverlay(QPointF(circle.center.x() + crossRadius + 2.0,
                                                       circle.center.y() - crossRadius),
                                               QStringLiteral("r=%1").arg(circle.radius, 0, 'f', 1),
                                               QStringLiteral("circle_radius_text"),
                                               circle.strength));
            result.overlays.append(textOverlay(QPointF(circle.center.x() + crossRadius + 2.0,
                                                       circle.center.y() + 2.0),
                                               QStringLiteral("circ=%1").arg(circle.circularity, 0, 'f', 2),
                                               QStringLiteral("circularity_text"),
                                               circle.strength));
        }

        const int rawCircleCount = detectedCircles.size();
        const int circleCount = finalCircles.size();
        const bool found = circleCount >= countMinUsed && circleCount <= countMaxUsed;
        const bool ok = config.existOk ? found : !found;
        const DetectedCircle *bestCircle = finalCircles.isEmpty() ? nullptr : &finalCircles.first();
        const int finalSelectedIndex = bestCircle ? bestCircle->rawIndex : -1;
        const QJsonObject bestCircleJson = bestCircle
                ? circleToJson(bestCircle->center,
                               bestCircle->radius,
                               bestCircle->area,
                               bestCircle->circularity,
                               bestCircle->polarity,
                               bestCircle->strength,
                               bestCircle->contrastScore)
                : QJsonObject();
        QJsonArray rejectedCandidates;
        for (const QJsonValue &value : thresholdPassesJson) {
            const QJsonObject passJson = value.toObject();
            QJsonObject rejected;
            rejected.insert(QStringLiteral("polarity"), passJson.value(QStringLiteral("polarity")));
            rejected.insert(QStringLiteral("reason"), QStringLiteral("area_or_circularity_filter"));
            rejected.insert(QStringLiteral("areaCandidateCount"), passJson.value(QStringLiteral("areaCandidateCount")));
            rejected.insert(QStringLiteral("selectedCount"), passJson.value(QStringLiteral("selectedCount")));
            rejected.insert(QStringLiteral("circularityRejectedCount"),
                            passJson.value(QStringLiteral("circularityRejectedCount")));
            rejectedCandidates.append(rejected);
        }
        QString okNgReason;
        if (found) {
            okNgReason = config.existOk
                    ? QStringLiteral("found target, existOk=true")
                    : QStringLiteral("found target, existOk=false");
        } else if (circleCount <= 0) {
            okNgReason = config.existOk
                    ? QStringLiteral("not found target, existOk=true")
                    : QStringLiteral("not found target, existOk=false");
        } else {
            okNgReason = QStringLiteral("circle count out of internal range");
        }

        result.success = true;
        result.ok = ok;
        result.score = bestCircle ? qBound(0.0, bestCircle->strength, 1.0) : 0.0;
        result.count = circleCount;
        result.text = found ? QStringLiteral("found") : QStringLiteral("missing");
        result.status = QStringLiteral("CirclePresence: %1 count=%2 raw=%3 edgeType=%4 sensitivity=%5 thresholdMargin=%6 circularityMin=%7")
                .arg(result.text,
                     QString::number(circleCount),
                     QString::number(rawCircleCount),
                     edgeTypeUsed,
                     QString::number(sensitivityUsed),
                     QString::number(thresholdMarginUsed),
                     QString::number(circularityMinUsed, 'f', 3));
        result.message = result.status;

        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("rawCircleCount"), rawCircleCount);
        result.payload.insert(QStringLiteral("circleCount"), circleCount);
        result.payload.insert(QStringLiteral("finalCircleCount"), circleCount);
        result.payload.insert(QStringLiteral("rawCircles"), rawCirclesArray);
        result.payload.insert(QStringLiteral("circles"), circlesArray);
        result.payload.insert(QStringLiteral("rejectedCandidates"), rejectedCandidates);
        result.payload.insert(QStringLiteral("bestCircle"), bestCircleJson);
        result.payload.insert(QStringLiteral("center"), bestCircle ? pointToJson(bestCircle->center) : QJsonObject());
        result.payload.insert(QStringLiteral("radius"), bestCircle ? bestCircle->radius : 0.0);
        result.payload.insert(QStringLiteral("circularity"), bestCircle ? bestCircle->circularity : 0.0);
        result.payload.insert(QStringLiteral("area"), bestCircle ? bestCircle->area : 0.0);
        result.payload.insert(QStringLiteral("strength"), bestCircle ? bestCircle->strength : 0.0);
        result.payload.insert(QStringLiteral("finalSelectedIndex"), finalSelectedIndex);
        result.payload.insert(QStringLiteral("thresholdPasses"), thresholdPassesJson);
        result.payload.insert(QStringLiteral("thresholdLowUsed"), thresholdLowUsed);
        result.payload.insert(QStringLiteral("thresholdHighUsed"), thresholdHighUsed);
        result.payload.insert(QStringLiteral("edgePolarityUsed"), edgePolarityUsed);
        result.payload.insert(QStringLiteral("edgePolarityApplied"), true);
        result.payload.insert(QStringLiteral("edgeTypeApplied"), true);
        result.payload.insert(QStringLiteral("edgeTypeRequested"), edgeTypeRequested);
        result.payload.insert(QStringLiteral("edgeTypeUsed"), edgeTypeUsed);
        result.payload.insert(QStringLiteral("edgeTypeSelectionMetric"), edgeTypeSelectionMetric);
        result.payload.insert(QStringLiteral("manualUnsupported"), manualEdgeTypeUnsupported);
        result.payload.insert(QStringLiteral("mode"), QStringLiteral("region"));
        result.payload.insert(QStringLiteral("caliperModeAvailable"), false);
        result.payload.insert(QStringLiteral("caliperModeApplied"), false);
        result.payload.insert(QStringLiteral("maskApplied"), false);
        result.payload.insert(QStringLiteral("okNgReason"), okNgReason);
        result.payload.insert(QStringLiteral("text"), result.text);

        QPointF countPosition = countOverlayPosition(
                        detectRoiPixels,
                        QSize(image.cols, image.rows));

        if (correctionApplied) {
            countPosition = PositionCorrectionTransform::transformPoint(
                        countPosition,
                        config.positionCorrection.referenceToRunHomMat2D);
        }
        result.overlays.append(textOverlay(countPosition,
                                           QStringLiteral("count=%1").arg(circleCount),
                                           QStringLiteral("circle_count_text"),
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
        result.status = QStringLiteral("CirclePresence HALCON error");
        result.message = QString::fromLocal8Bit(error.what());
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.payload.insert(QStringLiteral("okNgReason"), result.message);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
}
