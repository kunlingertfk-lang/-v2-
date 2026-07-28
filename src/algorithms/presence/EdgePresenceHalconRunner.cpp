#include "algorithms/presence/EdgePresenceHalconRunner.h"
#include "toolcore/PositionCorrectionTransform.h"

#include <HalconC.h>

#include <QByteArray>
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
constexpr int kMaxOverlayContours = 24;
constexpr int kMaxOverlayLineSegments = 512;
constexpr int kTargetOverlaySegmentsPerContour = 96;
constexpr int kMaxLineSamplesInPayload = 128;
constexpr int kMaxContourPointSamplesInPayload = 64;
constexpr int kMaxRawContourDebugEntries = 24;
constexpr int kMaxPointsPerRawContourDebug = 16;
constexpr double kPi = 3.14159265358979323846;

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

QJsonObject lineToJson(const QPointF &p1, const QPointF &p2)
{
    QJsonObject json;
    json.insert(QStringLiteral("x1"), p1.x());
    json.insert(QStringLiteral("y1"), p1.y());
    json.insert(QStringLiteral("x2"), p2.x());
    json.insert(QStringLiteral("y2"), p2.y());
    return json;
}

QJsonObject pointToJson(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
}

QJsonArray sampledPointsToJson(const QVector<QPointF> &points, const int maxPoints)
{
    QJsonArray array;
    if (points.isEmpty() || maxPoints <= 0)
        return array;

    const int step = qMax(1, (points.size() + maxPoints - 1) / maxPoints);
    for (int index = 0; index < points.size() && array.size() < maxPoints; index += step)
        array.append(pointToJson(points.at(index)));

    return array;
}

bool hasVisibleLength(const QPointF &p1, const QPointF &p2)
{
    return std::hypot(p2.x() - p1.x(), p2.y() - p1.y()) > 0.001;
}

int contourOverlayStep(const int pointCount)
{
    if (pointCount <= 2)
        return 1;

    return qBound(1,
                  (pointCount + kTargetOverlaySegmentsPerContour - 1) / kTargetOverlaySegmentsPerContour,
                  8);
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

bool isSupportedDetectRegionType(const QString &regionType)
{
    const QString key = regionType.trimmed().toLower();
    return key.isEmpty() ||
           key == QStringLiteral("rect") ||
           key == QStringLiteral("rectangle") ||
           key == QStringLiteral("line_band") ||
           key == QStringLiteral("lineband") ||
           key.contains(QStringLiteral("线型")) ||
           key.contains(QStringLiteral("矩形"));
}

bool isLineBandRegionType(const QString &regionType)
{
    const QString key = regionType.trimmed().toLower();
    return key == QStringLiteral("line_band") ||
           key == QStringLiteral("lineband") ||
           key.contains(QStringLiteral("线型"));
}

QPointF normalizedToPixelPoint(const QPointF &point, const int width, const int height)
{
    return QPointF(qBound(0.0, point.x(), 1.0) * width,
                   qBound(0.0, point.y(), 1.0) * height);
}

QVector<QPointF> lineBandPolygonPixels(const EdgePresenceHalconConfig &config,
                                       const int width,
                                       const int height)
{
    QVector<QPointF> points;
    const QPointF p1 = normalizedToPixelPoint(config.searchLineP1, width, height);
    const QPointF p2 = normalizedToPixelPoint(config.searchLineP2, width, height);
    const double length = std::hypot(p2.x() - p1.x(), p2.y() - p1.y());
    if (length <= 0.001)
        return points;

    const double halfWidth = qMax(1.0,
                                  config.searchBandWidth *
                                  static_cast<double>(qMax(width, height)) / 2.0);
    const double nx = -(p2.y() - p1.y()) / length;
    const double ny = (p2.x() - p1.x()) / length;
    const QPointF offset(nx * halfWidth, ny * halfWidth);
    points << p1 + offset << p2 + offset << p2 - offset << p1 - offset;
    return points;
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

int highThresholdForSensitivity(const int sensitivity)
{
    return qBound(12, qRound(80.0 - static_cast<double>(sensitivity) * 0.65), 80);
}

int lowThresholdForHighThreshold(const int highThreshold)
{
    return qBound(1, qRound(static_cast<double>(highThreshold) * 0.45), qMax(1, highThreshold - 1));
}

double minEdgeLengthFor(const QRect &roi, const int sensitivity)
{
    const double shortSide = static_cast<double>(qMin(roi.width(), roi.height()));
    const double sensitivityRatio = qBound(0.0, static_cast<double>(sensitivity) / 100.0, 1.0);
    const double scale = 0.35 - sensitivityRatio * 0.25;
    return qMax(4.0, shortSide * scale);
}

QPointF countOverlayPosition(const QRect &roi, const int imageHeight)
{
    if (roi.y() >= 20)
        return QPointF(roi.x() + 4.0, roi.y() - 18.0);
    if (roi.bottom() + 24 < imageHeight)
        return QPointF(roi.x() + 4.0, roi.bottom() + 8.0);
    return QPointF(roi.x() + 4.0, roi.y() + 4.0);
}

void fillPayload(EdgePresenceHalconResult &result,
                 const cv::Mat &image,
                 const EdgePresenceHalconConfig &config)
{
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"),
                          config.halconSoPathCandidates.join(QStringLiteral("; ")));
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("searchLineP1"), pointToJson(config.searchLineP1));
    result.payload.insert(QStringLiteral("searchLineP2"), pointToJson(config.searchLineP2));
    result.payload.insert(QStringLiteral("searchBandWidth"), config.searchBandWidth);
    result.payload.insert(QStringLiteral("lineBandWidthUnit"), QStringLiteral("normalized_max_dimension"));
    result.payload.insert(QStringLiteral("lineBandApplied"), false);
    result.payload.insert(QStringLiteral("lineBandFallbackToBoundingRect"), isLineBandRegionType(config.detectRegionType));
    result.payload.insert(QStringLiteral("sensitivity"), qBound(0, config.sensitivity, 100));
    result.payload.insert(QStringLiteral("edgePolarity"), normalizedEdgePolarity(config.edgePolarity));
    result.payload.insert(QStringLiteral("edgePolarityApplied"), false);
    result.payload.insert(QStringLiteral("edgePolarityReason"), QStringLiteral("not implemented in first version"));
    result.payload.insert(QStringLiteral("existOk"), config.existOk);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("positionCorrectionRequested"),
                          config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"),
                          config.positionCorrection.applied);
    result.payload.insert(QStringLiteral("positionCorrectionSourceId"),
                          config.positionCorrection.sourceId);
    result.payload.insert(QStringLiteral("positionCorrectionReason"),
                          config.positionCorrection.applied
                          ? QStringLiteral("applied") : QStringLiteral("not_requested"));
    result.payload.insert(QStringLiteral("referenceScale"),
                          config.positionCorrection.referenceScale);
    result.payload.insert(QStringLiteral("runScale"),
                          config.positionCorrection.runScale);
    result.payload.insert(QStringLiteral("scaleRatio"),
                          config.positionCorrection.scaleRatio);
    result.payload.insert(QStringLiteral("maskApplied"), false);
    result.payload.insert(QStringLiteral("detectMaskApplied"), false);
    result.payload.insert(QStringLiteral("maskReason"), QStringLiteral("not implemented"));
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_edges_sub_pix"));
    result.payload.insert(QStringLiteral("timeoutMsInternalDefault"), config.timeoutMsInternalDefault);
}

EdgePresenceHalconResult makeParameterError(const QString &status,
                                            const QString &error,
                                            const cv::Mat &image,
                                            const EdgePresenceHalconConfig &config)
{
    EdgePresenceHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = error;
    result.text = QStringLiteral("error");
    result.payload.insert(QStringLiteral("error"), error);
    fillPayload(result, image, config);
    return result;
}

struct HalconCApi
{
    using SetUtf8Fn = void (*)(int);
    using GetErrorTextFn = Herror (*)(Hlong, char *);
    using CreateTupleIntFn = void (*)(Htuple *, Hlong);
    using CreateTupleDoubleFn = void (*)(Htuple *, double);
    using CreateTupleStringFn = void (*)(Htuple *, const char *);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GenMeasureRectangle2Fn = Herror (*)(const Htuple, const Htuple, const Htuple, const Htuple,
                                              const Htuple, const Htuple, const Htuple, const Htuple,
                                              Htuple *);
    using MeasurePosFn = Herror (*)(const Hobject, const Htuple, const Htuple, const Htuple,
                                    const Htuple, const Htuple, Htuple *, Htuple *, Htuple *, Htuple *);
    using CloseMeasureFn = Herror (*)(const Htuple);
    using GenImage1Fn = Herror (*)(Hobject *, const char *, Hlong, Hlong, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Rgb1ToGrayFn = Herror (*)(const Hobject, Hobject *);
    using EdgesSubPixFn = Herror (*)(const Hobject, Hobject *, const char *, double, Hlong, Hlong);
    using CountObjFn = Herror (*)(const Hobject, Hlong *);
    using SelectObjFn = Herror (*)(const Hobject, Hobject *, const Hlong);
    using TGetContourXldFn = Herror (*)(const Hobject, Htuple *, Htuple *);
    using TLengthXldFn = Herror (*)(const Hobject, Htuple *);
    using TSmallestRectangle1XldFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *, Htuple *);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleIntFn createTupleInt = nullptr;
    CreateTupleDoubleFn createTupleDouble = nullptr;
    CreateTupleStringFn createTupleString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GenMeasureRectangle2Fn genMeasureRectangle2 = nullptr;
    MeasurePosFn measurePos = nullptr;
    CloseMeasureFn closeMeasure = nullptr;
    GenImage1Fn genImage1 = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Rgb1ToGrayFn rgb1ToGray = nullptr;
    EdgesSubPixFn edgesSubPix = nullptr;
    CountObjFn countObj = nullptr;
    SelectObjFn selectObj = nullptr;
    TGetContourXldFn getContourXld = nullptr;
    TLengthXldFn lengthXld = nullptr;
    TSmallestRectangle1XldFn smallestRectangle1Xld = nullptr;
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
            !resolveRequired(m_handle, api.createTupleInt, "F_create_tuple_i", errorMessage) ||
            !resolveRequired(m_handle, api.createTupleDouble, "F_create_tuple_d", errorMessage) ||
            !resolveRequired(m_handle, api.createTupleString, "F_create_tuple_s", errorMessage) ||
            !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
            !resolveRequired(m_handle, api.genMeasureRectangle2, "T_gen_measure_rectangle2", errorMessage) ||
            !resolveRequired(m_handle, api.measurePos, "T_measure_pos", errorMessage) ||
            !resolveRequired(m_handle, api.closeMeasure, "T_close_measure", errorMessage) ||
            !resolveRequired(m_handle, api.genImage1, "gen_image1", errorMessage) ||
            !resolveRequired(m_handle, api.genImageInterleaved, "gen_image_interleaved", errorMessage) ||
            !resolveRequired(m_handle, api.rgb1ToGray, "rgb1_to_gray", errorMessage) ||
            !resolveRequired(m_handle, api.edgesSubPix, "edges_sub_pix", errorMessage) ||
            !resolveRequired(m_handle, api.countObj, "count_obj", errorMessage) ||
            !resolveRequired(m_handle, api.selectObj, "select_obj", errorMessage) ||
            !resolveRequired(m_handle, api.getContourXld, "T_get_contour_xld", errorMessage) ||
            !resolveRequired(m_handle, api.lengthXld, "T_length_xld", errorMessage) ||
            !resolveRequired(m_handle, api.smallestRectangle1Xld, "T_smallest_rectangle1_xld", errorMessage) ||
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

struct MeasureRectangleGeometry
{
    QPointF p1;
    QPointF p2;
    QPointF center;
    double phi = 0.0;
    double length1 = 0.0;
    double length2 = 0.0;
    bool valid = false;
    bool fallback = false;
    QString fallbackReason;
};

int measureThresholdForSensitivity(const int sensitivity)
{
    return qBound(5, 120 - qBound(0, sensitivity, 100), 120);
}

QString measureTransitionForPolarity(const QString &polarity)
{
    const QString normalized = normalizedEdgePolarity(polarity);
    if (normalized == QStringLiteral("black_to_white"))
        return QStringLiteral("positive");
    if (normalized == QStringLiteral("white_to_black"))
        return QStringLiteral("negative");
    return QStringLiteral("all");
}

QJsonArray pointsToJsonArray(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points)
        array.append(pointToJson(point));
    return array;
}

QJsonArray doublesToJsonArray(const QVector<double> &values)
{
    QJsonArray array;
    for (const double value : values)
        array.append(value);
    return array;
}

QJsonObject measureRectangleToJson(const MeasureRectangleGeometry &geometry)
{
    QJsonObject json;
    json.insert(QStringLiteral("row"), geometry.center.y());
    json.insert(QStringLiteral("col"), geometry.center.x());
    json.insert(QStringLiteral("phi"), geometry.phi);
    json.insert(QStringLiteral("phiDeg"), geometry.phi * 180.0 / kPi);
    json.insert(QStringLiteral("length1"), geometry.length1);
    json.insert(QStringLiteral("length2"), geometry.length2);
    return json;
}

MeasureRectangleGeometry measureGeometryFromConfig(const EdgePresenceHalconConfig &config,
                                                   const cv::Mat &image,
                                                   QString *fallbackReason)
{
    MeasureRectangleGeometry geometry;
    const QPointF p1 = normalizedToPixelPoint(config.searchLineP1, image.cols, image.rows);
    const QPointF p2 = normalizedToPixelPoint(config.searchLineP2, image.cols, image.rows);
    const double lineLength = std::hypot(p2.x() - p1.x(), p2.y() - p1.y());
    const double bandWidth = config.searchBandWidth * static_cast<double>(qMax(image.cols, image.rows));
    if (lineLength > 2.0 && std::isfinite(bandWidth) && bandWidth > 1.0) {
        geometry.p1 = p1;
        geometry.p2 = p2;
        geometry.center = QPointF((p1.x() + p2.x()) / 2.0, (p1.y() + p2.y()) / 2.0);
        geometry.phi = std::atan2(-(p2.y() - p1.y()), p2.x() - p1.x());
        geometry.length1 = lineLength / 2.0;
        geometry.length2 = qMax(1.0, bandWidth / 2.0);
        geometry.valid = true;
        return geometry;
    }

    bool roiTooSmall = false;
    const QRect roi = normalizedRoiToPixels(config.roiNormalized, image.cols, image.rows, &roiTooSmall);
    if (!roi.isEmpty()) {
        geometry.p1 = QPointF(roi.left(), roi.center().y());
        geometry.p2 = QPointF(roi.right(), roi.center().y());
        geometry.center = QPointF(roi.center().x(), roi.center().y());
        geometry.phi = 0.0;
        geometry.length1 = qMax(1.0, static_cast<double>(roi.width()) / 2.0);
        geometry.length2 = qMax(1.0, static_cast<double>(roi.height()) / 2.0);
        geometry.valid = true;
        geometry.fallback = true;
        geometry.fallbackReason = QStringLiteral("invalid line-band geometry, fallback to roiNormalized");
        if (fallbackReason)
            *fallbackReason = geometry.fallbackReason;
        return geometry;
    }

    if (fallbackReason) {
        *fallbackReason = roiTooSmall
                ? QStringLiteral("line-band invalid and fallback ROI too small")
                : QStringLiteral("line-band invalid and fallback ROI invalid");
    }
    return geometry;
}

void appendMeasureRectangleOverlay(QVector<ToolOverlay> *overlays,
                                   const MeasureRectangleGeometry &geometry)
{
    if (!overlays || !geometry.valid)
        return;

    const double ux = std::cos(geometry.phi);
    const double uy = std::sin(geometry.phi);
    const QPointF along(ux * geometry.length1, uy * geometry.length1);
    const QPointF normal(-uy * geometry.length2, ux * geometry.length2);
    const QPointF a = geometry.center - along - normal;
    const QPointF b = geometry.center + along - normal;
    const QPointF c = geometry.center + along + normal;
    const QPointF d = geometry.center - along + normal;
    overlays->append(lineOverlay(a, b, QStringLiteral("measure_rectangle")));
    overlays->append(lineOverlay(b, c, QStringLiteral("measure_rectangle")));
    overlays->append(lineOverlay(c, d, QStringLiteral("measure_rectangle")));
    overlays->append(lineOverlay(d, a, QStringLiteral("measure_rectangle")));
}

EdgePresenceHalconResult makeMeasurePosError(const QString &status,
                                             const QString &error,
                                             const cv::Mat &image,
                                             const EdgePresenceHalconConfig &config)
{
    EdgePresenceHalconResult result = makeParameterError(status, error, image, config);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("measure_pos"));
    result.payload.insert(QStringLiteral("okNgReason"), error);
    return result;
}

EdgePresenceHalconResult runMeasurePosPresence(const cv::Mat &image,
                                               const EdgePresenceHalconConfig &config)
{
    QElapsedTimer timer;
    timer.start();

    if (image.empty()) {
        EdgePresenceHalconResult result = makeMeasurePosError(QStringLiteral("image_empty"),
                                                              QStringLiteral("input image is empty"),
                                                              image,
                                                              config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (!isSupportedDetectRegionType(config.detectRegionType)) {
        EdgePresenceHalconResult result = makeMeasurePosError(QStringLiteral("unsupported_detect_roi"),
                                                              QStringLiteral("detect ROI shape is not supported"),
                                                              image,
                                                              config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (image.depth() != CV_8U ||
        (image.channels() != 1 && image.channels() != 3 && image.channels() != 4)) {
        EdgePresenceHalconResult result = makeMeasurePosError(QStringLiteral("unsupported_image_type"),
                                                              QStringLiteral("EdgePresence supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
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
        EdgePresenceHalconResult result = makeMeasurePosError(QStringLiteral("halcon_so_not_found"),
                                                              QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                                                              .arg(config.halconSoPath, triedPaths),
                                                              image,
                                                              config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    QString fallbackReason;
    MeasureRectangleGeometry geometry = measureGeometryFromConfig(config, image, &fallbackReason);
    if (!geometry.valid) {
        EdgePresenceHalconResult result = makeMeasurePosError(QStringLiteral("invalid_line_band"),
                                                              fallbackReason.isEmpty()
                                                              ? QStringLiteral("line-band ROI is invalid")
                                                              : fallbackReason,
                                                              image,
                                                              config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    const bool correctionApplied = config.positionCorrection.applied;
    if (correctionApplied) {
        geometry.p1 = PositionCorrectionTransform::transformPoint(
                    geometry.p1, config.positionCorrection.referenceToRunHomMat2D);
        geometry.p2 = PositionCorrectionTransform::transformPoint(
                    geometry.p2, config.positionCorrection.referenceToRunHomMat2D);
        geometry.center = QPointF((geometry.p1.x() + geometry.p2.x()) / 2.0,
                                  (geometry.p1.y() + geometry.p2.y()) / 2.0);
        const double lineLength = std::hypot(geometry.p2.x() - geometry.p1.x(),
                                             geometry.p2.y() - geometry.p1.y());
        if (!std::isfinite(lineLength) || lineLength <= 2.0) {
            EdgePresenceHalconResult errorResult =
                    makeMeasurePosError(QStringLiteral("corrected_roi_out_of_image"),
                                        QStringLiteral("Position-corrected edge ROI is invalid"),
                                        image,
                                        config);
            errorResult.elapsedMs = timer.elapsed();
            return errorResult;
        }
        geometry.phi = std::atan2(-(geometry.p2.y() - geometry.p1.y()),
                                  geometry.p2.x() - geometry.p1.x());
        geometry.length1 = lineLength / 2.0;
        geometry.length2 *= config.positionCorrection.scaleRatio;
        const QRectF correctedBounds(
                    QPointF(qMin(geometry.p1.x(), geometry.p2.x()) - geometry.length2,
                            qMin(geometry.p1.y(), geometry.p2.y()) - geometry.length2),
                    QPointF(qMax(geometry.p1.x(), geometry.p2.x()) + geometry.length2,
                            qMax(geometry.p1.y(), geometry.p2.y()) + geometry.length2));
        const QRectF imageBounds(0.0, 0.0,
                                 static_cast<double>(image.cols),
                                 static_cast<double>(image.rows));
        if (!std::isfinite(geometry.length2)
                || geometry.length2 <= 0.0
                || !correctedBounds.intersects(imageBounds)) {
            EdgePresenceHalconResult errorResult =
                    makeMeasurePosError(QStringLiteral("corrected_roi_out_of_image"),
                                        QStringLiteral("Position-corrected edge ROI is outside the image"),
                                        image,
                                        config);
            errorResult.elapsedMs = timer.elapsed();
            return errorResult;
        }
    }

    EdgePresenceHalconResult result;
    fillPayload(result, image, config);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("measure_pos"));
    result.payload.insert(QStringLiteral("roiMode"), geometry.fallback
                          ? QStringLiteral("roi_fallback_measure_pos")
                          : QStringLiteral("line_band_measure_pos"));
    result.payload.insert(QStringLiteral("lineBandApplied"), !geometry.fallback);
    result.payload.insert(QStringLiteral("lineBandFallbackToBoundingRect"), geometry.fallback);
    result.payload.insert(QStringLiteral("lineBandFallback"), geometry.fallback);
    result.payload.insert(QStringLiteral("lineBandFallbackReason"), geometry.fallbackReason);

    const int sensitivityUsed = qBound(0, config.sensitivity, 100);
    const int thresholdUsed = measureThresholdForSensitivity(sensitivityUsed);
    const double sigmaUsed = 1.0;
    const QString transitionUsed = measureTransitionForPolarity(config.edgePolarity);
    const QString selectUsed = QStringLiteral("all");
    const int countMinUsed = 1;
    const int countMaxUsed = std::numeric_limits<int>::max();

    result.payload.insert(QStringLiteral("sensitivityUsed"), sensitivityUsed);
    result.payload.insert(QStringLiteral("thresholdUsed"), thresholdUsed);
    result.payload.insert(QStringLiteral("sigmaUsed"), sigmaUsed);
    result.payload.insert(QStringLiteral("transitionUsed"), transitionUsed);
    result.payload.insert(QStringLiteral("selectUsed"), selectUsed);
    result.payload.insert(QStringLiteral("measureDirectionMode"), QStringLiteral("along_line"));
    result.payload.insert(QStringLiteral("phiConvention"), QStringLiteral("halcon_cartesian_y_up_from_search_line_p1_to_p2"));
    result.payload.insert(QStringLiteral("countMinUsed"), countMinUsed);
    result.payload.insert(QStringLiteral("countMaxUsed"), countMaxUsed);
    result.payload.insert(QStringLiteral("countRangeSource"), QStringLiteral("internal_default"));
    result.payload.insert(QStringLiteral("measureRectangle"), measureRectangleToJson(geometry));
    result.payload.insert(QStringLiteral("edgePolarityApplied"), true);
    result.payload.insert(QStringLiteral("edgePolarityReason"), QStringLiteral("mapped to MeasurePos transition"));
    result.payload.insert(QStringLiteral("maskApplied"), false);
    result.payload.insert(QStringLiteral("okNgReason"), QString());
    ToolOverlay detectOverlay = rectOverlay(
                QRectF(normalizedRoiToPixels(config.roiNormalized,
                                             image.cols,
                                             image.rows,
                                             nullptr)),
                QStringLiteral("detect_roi"));
    if (correctionApplied) {
        detectOverlay = PositionCorrectionTransform::transformOverlay(
                    detectOverlay,
                    config.positionCorrection.referenceToRunHomMat2D);
        detectOverlay.extra.insert(QStringLiteral("positionCorrectionSourceId"),
                                   config.positionCorrection.sourceId);
    }
    detectOverlay.extra.insert(QStringLiteral("role"), QStringLiteral("detect_roi"));
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
    result.overlays.append(lineOverlay(geometry.p1, geometry.p2, QStringLiteral("line_band_center")));
    appendMeasureRectangleOverlay(&result.overlays, geometry);
    if (!geometry.fallback) {
        const QVector<QPointF> band = lineBandPolygonPixels(config, image.cols, image.rows);
        if (band.size() == 4) {
            for (int index = 0; index < band.size(); ++index) {
                ToolOverlay boundary = lineOverlay(
                            band.at(index),
                            band.at((index + 1) % band.size()),
                            QStringLiteral("line_band_boundary"));
                if (correctionApplied) {
                    boundary = PositionCorrectionTransform::transformOverlay(
                                boundary,
                                config.positionCorrection.referenceToRunHomMat2D);
                }
                result.overlays.append(boundary);
            }
        }
    }

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
        result.payload.insert(QStringLiteral("okNgReason"), loadMessage);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    HalconCApi *api = &library.api;
    GrayHalconImage halconImage;
    QVector<Htuple *> tuples;
    Htuple measureRowTuple = HTUPLE_INITIALIZER;
    Htuple measureColTuple = HTUPLE_INITIALIZER;
    Htuple measurePhiTuple = HTUPLE_INITIALIZER;
    Htuple measureLength1Tuple = HTUPLE_INITIALIZER;
    Htuple measureLength2Tuple = HTUPLE_INITIALIZER;
    Htuple measureWidthTuple = HTUPLE_INITIALIZER;
    Htuple measureHeightTuple = HTUPLE_INITIALIZER;
    Htuple interpolationTuple = HTUPLE_INITIALIZER;
    Htuple measureHandleTuple = HTUPLE_INITIALIZER;
    Htuple sigmaTuple = HTUPLE_INITIALIZER;
    Htuple thresholdTuple = HTUPLE_INITIALIZER;
    Htuple transitionTuple = HTUPLE_INITIALIZER;
    Htuple selectTuple = HTUPLE_INITIALIZER;
    Htuple rowEdgesTuple = HTUPLE_INITIALIZER;
    Htuple colEdgesTuple = HTUPLE_INITIALIZER;
    Htuple amplitudeTuple = HTUPLE_INITIALIZER;
    Htuple distanceTuple = HTUPLE_INITIALIZER;

    auto track = [&](Htuple &tuple) {
        if (!tuples.contains(&tuple))
            tuples.append(&tuple);
    };
    auto createIntTuple = [&](Htuple &tuple, const Hlong value) {
        api->createTupleInt(&tuple, value);
        track(tuple);
    };
    auto createDoubleTuple = [&](Htuple &tuple, const double value) {
        api->createTupleDouble(&tuple, value);
        track(tuple);
    };
    auto createStringTuple = [&](Htuple &tuple, const QByteArray &value) {
        api->createTupleString(&tuple, value.constData());
        track(tuple);
    };
    auto destroyTuple = [&](Htuple &tuple) {
        if ((tuple.num > 0 || tuple.capacity > 0) && api->destroyTuple)
            api->destroyTuple(&tuple);
        tuple = HTUPLE_INITIALIZER;
    };
    auto clearObject = [&](Hobject &object) {
        if (halconObjectAllocated(object) && api->clearObj)
            api->clearObj(object);
        object = NO_OBJECTS;
    };
    auto cleanup = [&]() {
        clearObject(halconImage.grayImage);
        clearObject(halconImage.inputImage);
        if ((measureHandleTuple.num > 0 || measureHandleTuple.capacity > 0) &&
            api->closeMeasure) {
            api->closeMeasure(measureHandleTuple);
        }
        for (Htuple *tuple : tuples) {
            if (tuple)
                destroyTuple(*tuple);
        }
        tuples.clear();
    };
    auto halconErrorText = [&](const Herror status) {
        char buffer[1024] = {0};
        if (api->getErrorText && halconStatusOk(api->getErrorText(static_cast<Hlong>(status), buffer))) {
            const QString message = QString::fromUtf8(buffer).trimmed();
            if (!message.isEmpty())
                return message;
        }
        return QStringLiteral("HALCON error code %1").arg(static_cast<qlonglong>(status));
    };
    auto checkStatus = [&](const Herror status, const QString &stage) {
        if (!halconStatusOk(status)) {
            throw std::pair<QString, QString>(
                    QStringLiteral("EdgePresence HALCON error"),
                    QStringLiteral("%1: %2").arg(stage, halconErrorText(status)));
        }
    };
    auto generateGrayImage = [&](const cv::Mat &mat) {
        const int channels = mat.channels();
        if (channels == 1) {
            checkStatus(api->genImage1(&halconImage.inputImage,
                                       "byte",
                                       mat.cols,
                                       mat.rows,
                                       reinterpret_cast<Hlong>(mat.data)),
                        QStringLiteral("gen_image1"));
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
                    QStringLiteral("gen_image_interleaved"));
        checkStatus(api->rgb1ToGray(halconImage.inputImage, &halconImage.grayImage),
                    QStringLiteral("rgb1_to_gray"));
        halconImage.graySource = halconImage.grayImage;
    };

    try {
        generateGrayImage(image);
        createDoubleTuple(measureRowTuple, geometry.center.y());
        createDoubleTuple(measureColTuple, geometry.center.x());
        createDoubleTuple(measurePhiTuple, geometry.phi);
        createDoubleTuple(measureLength1Tuple, geometry.length1);
        createDoubleTuple(measureLength2Tuple, geometry.length2);
        createIntTuple(measureWidthTuple, image.cols);
        createIntTuple(measureHeightTuple, image.rows);
        createStringTuple(interpolationTuple, QByteArray("nearest_neighbor"));
        checkStatus(api->genMeasureRectangle2(measureRowTuple,
                                              measureColTuple,
                                              measurePhiTuple,
                                              measureLength1Tuple,
                                              measureLength2Tuple,
                                              measureWidthTuple,
                                              measureHeightTuple,
                                              interpolationTuple,
                                              &measureHandleTuple),
                    QStringLiteral("gen_measure_rectangle2"));
        track(measureHandleTuple);
        createDoubleTuple(sigmaTuple, sigmaUsed);
        createIntTuple(thresholdTuple, thresholdUsed);
        createStringTuple(transitionTuple, transitionUsed.toLatin1());
        createStringTuple(selectTuple, selectUsed.toLatin1());
        checkStatus(api->measurePos(halconImage.graySource,
                                    measureHandleTuple,
                                    sigmaTuple,
                                    thresholdTuple,
                                    transitionTuple,
                                    selectTuple,
                                    &rowEdgesTuple,
                                    &colEdgesTuple,
                                    &amplitudeTuple,
                                    &distanceTuple),
                    QStringLiteral("measure_pos"));
        track(rowEdgesTuple);
        track(colEdgesTuple);
        track(amplitudeTuple);
        track(distanceTuple);
        result.payload.insert(QStringLiteral("rowEdgeTupleCount"), static_cast<int>(rowEdgesTuple.num));
        result.payload.insert(QStringLiteral("colEdgeTupleCount"), static_cast<int>(colEdgesTuple.num));
        result.payload.insert(QStringLiteral("amplitudeTupleCount"), static_cast<int>(amplitudeTuple.num));
        result.payload.insert(QStringLiteral("distanceTupleCount"), static_cast<int>(distanceTuple.num));

        QVector<QPointF> edgePoints;
        QVector<double> amplitudes;
        QVector<double> distances;
        const int edgeCount = qMin<int>(qMin<int>(rowEdgesTuple.num, colEdgesTuple.num),
                                        amplitudeTuple.num);
        edgePoints.reserve(edgeCount);
        amplitudes.reserve(edgeCount);
        distances.reserve(edgeCount);
        double maxAbsAmplitude = 0.0;
        for (int index = 0; index < edgeCount; ++index) {
            const double row = api->getDouble(&rowEdgesTuple, index);
            const double col = api->getDouble(&colEdgesTuple, index);
            const double amplitude = api->getDouble(&amplitudeTuple, index);
            const double distance = index < distanceTuple.num
                    ? api->getDouble(&distanceTuple, index)
                    : 0.0;
            if (!std::isfinite(row) || !std::isfinite(col))
                continue;
            const QPointF point(col, row);
            edgePoints.append(point);
            amplitudes.append(amplitude);
            if (index < distanceTuple.num)
                distances.append(distance);
            maxAbsAmplitude = qMax(maxAbsAmplitude, std::abs(amplitude));
            const double marker = 4.0;
            result.overlays.append(lineOverlay(QPointF(point.x() - marker, point.y()),
                                               QPointF(point.x() + marker, point.y()),
                                               QStringLiteral("edge_points")));
            result.overlays.append(lineOverlay(QPointF(point.x(), point.y() - marker),
                                               QPointF(point.x(), point.y() + marker),
                                               QStringLiteral("edge_points")));
        }

        const bool found = edgePoints.size() >= countMinUsed && edgePoints.size() <= countMaxUsed;
        const bool ok = config.existOk ? found : !found;
        QString okNgReason;
        if (found) {
            okNgReason = config.existOk
                    ? QStringLiteral("found target, existOk=true")
                    : QStringLiteral("found target, existOk=false");
        } else if (edgePoints.isEmpty()) {
            okNgReason = config.existOk
                    ? QStringLiteral("not found target, existOk=true")
                    : QStringLiteral("not found target, existOk=false");
        } else {
            okNgReason = QStringLiteral("edge count out of range");
        }

        result.success = true;
        result.ok = ok;
        result.score = edgePoints.isEmpty() ? 0.0 : qBound(0.0, maxAbsAmplitude / 255.0, 1.0);
        result.value = edgePoints.size();
        result.count = edgePoints.size();
        result.text = found ? QStringLiteral("found") : QStringLiteral("missing");
        result.status = QStringLiteral("EdgePresence: %1 edges=%2")
                .arg(result.text,
                     QString::number(edgePoints.size()));
        result.message = result.status;
        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("edgeCount"), edgePoints.size());
        result.payload.insert(QStringLiteral("edgePoints"), pointsToJsonArray(edgePoints));
        result.payload.insert(QStringLiteral("amplitudes"), doublesToJsonArray(amplitudes));
        result.payload.insert(QStringLiteral("distances"), doublesToJsonArray(distances));
        result.payload.insert(QStringLiteral("maxAbsAmplitude"), maxAbsAmplitude);
        result.payload.insert(QStringLiteral("valueMeaning"), QStringLiteral("edgeCount"));
        result.payload.insert(QStringLiteral("okNgReason"), okNgReason);
        result.payload.insert(QStringLiteral("text"), result.text);
        result.overlays.append(textOverlay(geometry.center,
                                           QStringLiteral("edges=%1")
                                           .arg(edgePoints.size()),
                                           QStringLiteral("edge_count_text"),
                                           result.score));
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("timeoutExceeded"),
                              config.timeoutMsInternalDefault > 0 &&
                              result.elapsedMs > config.timeoutMsInternalDefault);
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
        result.status = QStringLiteral("EdgePresence HALCON error");
        result.message = QString::fromLocal8Bit(error.what());
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.payload.insert(QStringLiteral("okNgReason"), result.message);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
}

} // namespace

EdgePresenceHalconResult EdgePresenceHalconRunner::run(
        const cv::Mat &image,
        const EdgePresenceHalconConfig &config)
{
    return runMeasurePosPresence(image, config);

    QElapsedTimer timer;
    timer.start();

    if (image.empty()) {
        EdgePresenceHalconResult result = makeParameterError(QStringLiteral("image_empty"),
                                                             QStringLiteral("input image is empty"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isValidNormalizedRoi(config.roiNormalized)) {
        EdgePresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                             QStringLiteral("detect ROI is invalid"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isSupportedDetectRegionType(config.detectRegionType)) {
        EdgePresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_detect_roi"),
                                                             QStringLiteral("detect ROI shape is not supported"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("todo"), QStringLiteral("only rectangle detect ROI is implemented"));
        return result;
    }

    if (image.depth() != CV_8U ||
        (image.channels() != 1 && image.channels() != 3 && image.channels() != 4)) {
        EdgePresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_image_type"),
                                                             QStringLiteral("EdgePresence supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
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
        EdgePresenceHalconResult result = makeParameterError(QStringLiteral("halcon_so_not_found"),
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
        EdgePresenceHalconResult result = makeParameterError(roiTooSmall
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

    EdgePresenceHalconResult result;
    fillPayload(result, image, config);

    const int sensitivityUsed = qBound(0, config.sensitivity, 100);
    const int edgeThresholdInternalUsed = highThresholdForSensitivity(sensitivityUsed);
    const int edgeThresholdLowInternalUsed = lowThresholdForHighThreshold(edgeThresholdInternalUsed);
    const double minEdgeLengthUsed = minEdgeLengthFor(detectRoiPixels, sensitivityUsed);

    result.payload.insert(QStringLiteral("detectRoi"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoiPixels"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoiPixelX"), detectRoiPixels.x());
    result.payload.insert(QStringLiteral("detectRoiPixelY"), detectRoiPixels.y());
    result.payload.insert(QStringLiteral("detectRoiPixelW"), detectRoiPixels.width());
    result.payload.insert(QStringLiteral("detectRoiPixelH"), detectRoiPixels.height());
    const bool lineBandMode = isLineBandRegionType(config.detectRegionType);
    result.payload.insert(QStringLiteral("roiMode"),
                          lineBandMode ? QStringLiteral("line_band_bounding_rect")
                                       : QStringLiteral("rectangle"));
    result.payload.insert(QStringLiteral("lineBandApplied"), false);
    result.payload.insert(QStringLiteral("lineBandFallbackToBoundingRect"), lineBandMode);
    result.payload.insert(QStringLiteral("sensitivityUsed"), sensitivityUsed);
    result.payload.insert(QStringLiteral("edgeThresholdInternalUsed"), edgeThresholdInternalUsed);
    result.payload.insert(QStringLiteral("edgeThresholdLowInternalUsed"), edgeThresholdLowInternalUsed);
    result.payload.insert(QStringLiteral("thresholdSource"), QStringLiteral("derived_from_sensitivity"));
    result.payload.insert(QStringLiteral("minEdgeLengthUsed"), minEdgeLengthUsed);
    result.payload.insert(QStringLiteral("minEdgeLengthSource"), QStringLiteral("internal_default"));
    result.overlays.append(rectOverlay(QRectF(detectRoiPixels), QStringLiteral("detect_roi")));
    if (lineBandMode) {
        const QPointF p1 = normalizedToPixelPoint(config.searchLineP1, image.cols, image.rows);
        const QPointF p2 = normalizedToPixelPoint(config.searchLineP2, image.cols, image.rows);
        result.overlays.append(lineOverlay(p1, p2, QStringLiteral("line_band_center")));
        const QVector<QPointF> band = lineBandPolygonPixels(config, image.cols, image.rows);
        if (band.size() == 4) {
            for (int index = 0; index < band.size(); ++index)
                result.overlays.append(lineOverlay(band.at(index),
                                                   band.at((index + 1) % band.size()),
                                                   QStringLiteral("line_band_boundary")));
        }
    }

    cv::Mat detectMat = image(cv::Rect(detectRoiPixels.x(),
                                       detectRoiPixels.y(),
                                       detectRoiPixels.width(),
                                       detectRoiPixels.height())).clone();
    if (detectMat.empty()) {
        EdgePresenceHalconResult errorResult = makeParameterError(QStringLiteral("invalid_detect_roi"),
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
    Hobject edges = NO_OBJECTS;
    Htuple lengthTuple = HTUPLE_INITIALIZER;
    Htuple row1Tuple = HTUPLE_INITIALIZER;
    Htuple col1Tuple = HTUPLE_INITIALIZER;
    Htuple row2Tuple = HTUPLE_INITIALIZER;
    Htuple col2Tuple = HTUPLE_INITIALIZER;

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
        destroyTuple(col2Tuple);
        destroyTuple(row2Tuple);
        destroyTuple(col1Tuple);
        destroyTuple(row1Tuple);
        destroyTuple(lengthTuple);
        clearObject(edges);
        clearObject(detectImage.grayImage);
        clearObject(detectImage.inputImage);
    };

    auto checkStatus = [&](const Herror status, const QString &stage) {
        if (!halconStatusOk(status)) {
            throw std::pair<QString, QString>(
                    QStringLiteral("EdgePresence HALCON error"),
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

    try {
        generateGrayImage(detectMat, detectImage, QStringLiteral("detect"));

        checkStatus(api->edgesSubPix(detectImage.graySource,
                                     &edges,
                                     "canny",
                                     1.0,
                                     static_cast<Hlong>(edgeThresholdLowInternalUsed),
                                     static_cast<Hlong>(edgeThresholdInternalUsed)),
                    QStringLiteral("edges_sub_pix"));

        Hlong objectCount = 0;
        checkStatus(api->countObj(edges, &objectCount), QStringLiteral("count_obj.edges"));
        const int edgeCount = qMax(0, static_cast<int>(objectCount));
        double edgeLength = 0.0;
        QJsonArray edgeLengthsJson;
        QJsonArray edgeRectsJson;
        QJsonArray edgeLinesJson;
        QJsonArray contourPointsSampleJson;
        QJsonArray rawEdgeContoursJson;
        int contourPointCount = 0;
        int overlayContourCount = 0;
        int overlayLineSegmentCount = 0;

        auto appendEdgeLineOverlay = [&](const QPointF &p1, const QPointF &p2) {
            if (!hasVisibleLength(p1, p2))
                return;

            if (edgeLinesJson.size() < kMaxLineSamplesInPayload)
                edgeLinesJson.append(lineToJson(p1, p2));

            if (overlayLineSegmentCount >= kMaxOverlayLineSegments)
                return;

            result.overlays.append(lineOverlay(p1, p2, QStringLiteral("edge_contour"), 1.0));
            ++overlayLineSegmentCount;
        };

        if (edgeCount > 0) {
            checkStatus(api->lengthXld(edges, &lengthTuple), QStringLiteral("length_xld"));
            const int lengthCount = qMax(0, static_cast<int>(lengthTuple.num));
            for (int index = 0; index < lengthCount; ++index) {
                const double length = api->getDouble(&lengthTuple, index);
                if (std::isfinite(length) && length > 0.0) {
                    edgeLength += length;
                    edgeLengthsJson.append(length);
                }
            }

            checkStatus(api->smallestRectangle1Xld(edges,
                                                   &row1Tuple,
                                                   &col1Tuple,
                                                   &row2Tuple,
                                                   &col2Tuple),
                        QStringLiteral("smallest_rectangle1_xld"));

            const int rectCount = qMin<int>(qMin<int>(row1Tuple.num, col1Tuple.num),
                                            qMin<int>(row2Tuple.num, col2Tuple.num));
            for (int index = 0; index < rectCount; ++index) {
                const double row1 = api->getDouble(&row1Tuple, index);
                const double col1 = api->getDouble(&col1Tuple, index);
                const double row2 = api->getDouble(&row2Tuple, index);
                const double col2 = api->getDouble(&col2Tuple, index);
                const QRectF rect(col1 + detectRoiPixels.x(),
                                  row1 + detectRoiPixels.y(),
                                  qMax(1.0, col2 - col1 + 1.0),
                                  qMax(1.0, row2 - row1 + 1.0));
                edgeRectsJson.append(rectToJson(rect));

                if (index < kMaxRawContourDebugEntries) {
                    QJsonObject debugRect;
                    debugRect.insert(QStringLiteral("index"), index + 1);
                    debugRect.insert(QStringLiteral("boundingRect"), rectToJson(rect));
                    rawEdgeContoursJson.append(debugRect);
                }
            }

            for (int index = 0; index < edgeCount; ++index) {
                Hobject selectedEdge = NO_OBJECTS;
                Htuple rowTuple = HTUPLE_INITIALIZER;
                Htuple colTuple = HTUPLE_INITIALIZER;

                try {
                    checkStatus(api->selectObj(edges,
                                               &selectedEdge,
                                               static_cast<Hlong>(index + 1)),
                                QStringLiteral("select_obj.edge"));
                    checkStatus(api->getContourXld(selectedEdge, &rowTuple, &colTuple),
                                QStringLiteral("get_contour_xld"));

                    const int pointTupleCount = qMin<int>(rowTuple.num, colTuple.num);
                    QVector<QPointF> contourPoints;
                    contourPoints.reserve(pointTupleCount);
                    for (int pointIndex = 0; pointIndex < pointTupleCount; ++pointIndex) {
                        const double row = api->getDouble(&rowTuple, pointIndex);
                        const double column = api->getDouble(&colTuple, pointIndex);
                        if (!std::isfinite(row) || !std::isfinite(column))
                            continue;

                        contourPoints.append(QPointF(detectRoiPixels.x() + column,
                                                      detectRoiPixels.y() + row));
                    }

                    contourPointCount += contourPoints.size();

                    if (contourPointsSampleJson.size() < kMaxContourPointSamplesInPayload) {
                        const QJsonArray sampledPoints =
                                sampledPointsToJson(contourPoints, kMaxPointsPerRawContourDebug);
                        for (const QJsonValue &pointValue : sampledPoints) {
                            if (contourPointsSampleJson.size() >= kMaxContourPointSamplesInPayload)
                                break;
                            contourPointsSampleJson.append(pointValue);
                        }
                    }

                    if (index < rawEdgeContoursJson.size()) {
                        QJsonObject contourDebug = rawEdgeContoursJson.at(index).toObject();
                        contourDebug.insert(QStringLiteral("pointCount"), contourPoints.size());
                        contourDebug.insert(QStringLiteral("pointsSample"),
                                            sampledPointsToJson(contourPoints,
                                                                kMaxPointsPerRawContourDebug));
                        rawEdgeContoursJson.replace(index, contourDebug);
                    } else if (index < kMaxRawContourDebugEntries) {
                        QJsonObject contourDebug;
                        contourDebug.insert(QStringLiteral("index"), index + 1);
                        contourDebug.insert(QStringLiteral("pointCount"), contourPoints.size());
                        contourDebug.insert(QStringLiteral("pointsSample"),
                                            sampledPointsToJson(contourPoints,
                                                                kMaxPointsPerRawContourDebug));
                        rawEdgeContoursJson.append(contourDebug);
                    }

                    if (index < kMaxOverlayContours &&
                        contourPoints.size() >= 2 &&
                        overlayLineSegmentCount < kMaxOverlayLineSegments) {
                        ++overlayContourCount;
                        const int step = contourOverlayStep(contourPoints.size());
                        QPointF previousPoint = contourPoints.first();
                        for (int pointIndex = step;
                             pointIndex < contourPoints.size() &&
                             overlayLineSegmentCount < kMaxOverlayLineSegments;
                             pointIndex += step) {
                            const QPointF currentPoint = contourPoints.at(pointIndex);
                            appendEdgeLineOverlay(previousPoint, currentPoint);
                            previousPoint = currentPoint;
                        }

                        if (overlayLineSegmentCount < kMaxOverlayLineSegments)
                            appendEdgeLineOverlay(previousPoint, contourPoints.last());
                    }

                    destroyTuple(colTuple);
                    destroyTuple(rowTuple);
                    clearObject(selectedEdge);
                } catch (...) {
                    destroyTuple(colTuple);
                    destroyTuple(rowTuple);
                    clearObject(selectedEdge);
                    throw;
                }
            }
        }

        const bool found = edgeCount > 0 && edgeLength >= minEdgeLengthUsed;
        const bool ok = config.existOk ? found : !found;
        const double score = qBound(0.0, edgeLength / qMax(1.0, minEdgeLengthUsed), 1.0);

        result.success = true;
        result.ok = ok;
        result.score = score;
        result.value = edgeLength;
        result.count = edgeCount;
        result.text = found ? QStringLiteral("found") : QStringLiteral("missing");
        result.status = QStringLiteral("EdgePresence: %1 edges=%2 length=%3")
                .arg(result.text,
                     QString::number(edgeCount),
                     QString::number(edgeLength, 'f', 1));
        result.message = result.status;

        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("edgeCount"), edgeCount);
        result.payload.insert(QStringLiteral("edgeSegmentCount"), edgeCount);
        result.payload.insert(QStringLiteral("edgeLength"), edgeLength);
        result.payload.insert(QStringLiteral("rawEdgeCount"), edgeCount);
        result.payload.insert(QStringLiteral("rawEdgeLength"), edgeLength);
        result.payload.insert(QStringLiteral("edgeLengths"), edgeLengthsJson);
        result.payload.insert(QStringLiteral("edgeBoundingRects"), edgeRectsJson);
        result.payload.insert(QStringLiteral("edgeBoundingRectsDisplayed"), false);
        result.payload.insert(QStringLiteral("edgeLineSamples"), edgeLinesJson);
        result.payload.insert(QStringLiteral("contourPointCount"), contourPointCount);
        result.payload.insert(QStringLiteral("contourPointsSample"), contourPointsSampleJson);
        result.payload.insert(QStringLiteral("rawEdgeContours"), rawEdgeContoursJson);
        result.payload.insert(QStringLiteral("edgeOverlayMode"),
                              QStringLiteral("line_segments_from_xld_contours"));
        result.payload.insert(QStringLiteral("edgeOverlayContourCount"), overlayContourCount);
        result.payload.insert(QStringLiteral("edgeOverlayLineSegmentCount"), overlayLineSegmentCount);
        result.payload.insert(QStringLiteral("score"), score);
        result.payload.insert(QStringLiteral("text"), result.text);

        const QPointF countPosition = countOverlayPosition(detectRoiPixels, image.rows);
        result.overlays.append(textOverlay(countPosition,
                                           QStringLiteral("edges=%1 length=%2")
                                           .arg(edgeCount)
                                           .arg(edgeLength, 0, 'f', 1),
                                           QStringLiteral("edge_count"),
                                           result.score));

        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("timeoutExceeded"),
                              config.timeoutMsInternalDefault > 0 &&
                              result.elapsedMs > config.timeoutMsInternalDefault);

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
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const std::exception &error) {
        cleanup();
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("EdgePresence HALCON error");
        result.message = QString::fromLocal8Bit(error.what());
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
}
