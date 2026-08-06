#include "algorithms/presence/LinePresenceHalconRunner.h"
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
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <exception>
#include <opencv2/core.hpp>

namespace {

constexpr int kMinRoiPixelSize = 2;
constexpr int kMaxRawLineCandidatesInPayload = 128;
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

QVector<QPointF> lineBandPolygonPixels(const LinePresenceHalconConfig &config,
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

QString normalizedEdgeType(const QString &edgeType)
{
    const QString key = edgeType.trimmed().toLower();
    if (key == QStringLiteral("first") || key.contains(QStringLiteral("第一")))
        return QStringLiteral("first");
    if (key == QStringLiteral("last") || key.contains(QStringLiteral("最后")))
        return QStringLiteral("last");
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
    return qBound(10, qRound(82.0 - static_cast<double>(sensitivity) * 0.70), 82);
}

int lowThresholdForHighThreshold(const int highThreshold)
{
    return qBound(1, qRound(static_cast<double>(highThreshold) * 0.45), qMax(1, highThreshold - 1));
}

double minLineLengthFor(const QRect &roi)
{
    const double shortSide = static_cast<double>(qMin(roi.width(), roi.height()));
    return qMax(20.0, shortSide * 0.35);
}

double straightnessMinForLineDegree(const int lineDegree)
{
    const double ratio = qBound(0.0, static_cast<double>(lineDegree) / 100.0, 1.0);
    return qBound(0.55, 0.55 + ratio * 0.43, 0.98);
}

double maxFitErrorForLineDegree(const int lineDegree)
{
    const double ratio = qBound(0.0, static_cast<double>(lineDegree) / 100.0, 1.0);
    return qBound(0.8, 5.5 - ratio * 4.2, 5.5);
}

QPointF countOverlayPosition(const QRect &roi, const int imageHeight)
{
    if (roi.y() >= 20)
        return QPointF(roi.x() + 4.0, roi.y() - 18.0);
    if (roi.bottom() + 24 < imageHeight)
        return QPointF(roi.x() + 4.0, roi.bottom() + 8.0);
    return QPointF(roi.x() + 4.0, roi.y() + 4.0);
}

struct LineCandidate
{
    QPointF p1;
    QPointF p2;
    double length = 0.0;
    double contourLength = 0.0;
    double straightness = 0.0;
    double straightnessFactor = 0.0;
    double fitError = 0.0;
    double score = 0.0;
    double scanX = 0.0;
    double scanY = 0.0;
    int sourceIndex = 0;
};

QJsonObject candidateToJson(const LineCandidate &candidate)
{
    QJsonObject json = lineToJson(candidate.p1, candidate.p2);
    json.insert(QStringLiteral("sourceIndex"), candidate.sourceIndex);
    json.insert(QStringLiteral("length"), candidate.length);
    json.insert(QStringLiteral("contourLength"), candidate.contourLength);
    json.insert(QStringLiteral("straightness"), candidate.straightness);
    json.insert(QStringLiteral("straightnessFactor"), candidate.straightnessFactor);
    json.insert(QStringLiteral("fitError"), candidate.fitError);
    json.insert(QStringLiteral("score"), candidate.score);
    json.insert(QStringLiteral("scanX"), candidate.scanX);
    json.insert(QStringLiteral("scanY"), candidate.scanY);
    return json;
}

void fillPayload(LinePresenceHalconResult &result,
                 const cv::Mat &image,
                 const LinePresenceHalconConfig &config)
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
    result.payload.insert(QStringLiteral("lineDegree"), qBound(0, config.lineDegree, 100));
    result.payload.insert(QStringLiteral("edgePolarity"), normalizedEdgePolarity(config.edgePolarity));
    result.payload.insert(QStringLiteral("edgePolarityApplied"), false);
    result.payload.insert(QStringLiteral("edgePolarityReason"), QStringLiteral("not reliably supported by first-version line extraction"));
    result.payload.insert(QStringLiteral("edgeTypeRequested"), normalizedEdgeType(config.edgeType));
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
    result.payload.insert(QStringLiteral("algorithm"),
                          QStringLiteral("halcon_edges_sub_pix_segment_contours_xld_fit_line_contour_xld"));
    result.payload.insert(QStringLiteral("timeoutMsInternalDefault"), config.timeoutMsInternalDefault);
}

LinePresenceHalconResult makeParameterError(const QString &status,
                                            const QString &error,
                                            const cv::Mat &image,
                                            const LinePresenceHalconConfig &config)
{
    LinePresenceHalconResult result;
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
    using CreateTupleFn = void (*)(Htuple *, Hlong);
    using CreateTupleIntFn = void (*)(Htuple *, Hlong);
    using CreateTupleDoubleFn = void (*)(Htuple *, double);
    using CreateTupleStringFn = void (*)(Htuple *, const char *);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using CreateMetrologyModelFn = Herror (*)(Htuple *);
    using AddMetrologyObjectLineMeasureFn = Herror (*)(const Htuple, const Htuple, const Htuple,
                                                       const Htuple, const Htuple, const Htuple,
                                                       const Htuple, const Htuple, const Htuple,
                                                       const Htuple, const Htuple, Htuple *);
    using SetMetrologyObjectParamFn = Herror (*)(const Htuple, const Htuple, const Htuple, const Htuple);
    using ApplyMetrologyModelFn = Herror (*)(const Hobject, const Htuple);
    using GetMetrologyObjectResultFn = Herror (*)(const Htuple, const Htuple, const Htuple,
                                                  const Htuple, const Htuple, Htuple *);
    using GetMetrologyObjectMeasuresFn = Herror (*)(Hobject *, const Htuple, const Htuple,
                                                    const Htuple, Htuple *, Htuple *);
    using ClearMetrologyModelFn = Herror (*)(const Htuple);
    using GenImage1Fn = Herror (*)(Hobject *, const char *, Hlong, Hlong, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Rgb1ToGrayFn = Herror (*)(const Hobject, Hobject *);
    using EdgesSubPixFn = Herror (*)(const Hobject, Hobject *, const char *, double, Hlong, Hlong);
    using SegmentContoursXldFn = Herror (*)(const Hobject, Hobject *, const char *, Hlong, double, double);
    using CountObjFn = Herror (*)(const Hobject, Hlong *);
    using SelectObjFn = Herror (*)(const Hobject, Hobject *, const Hlong);
    using TGetContourXldFn = Herror (*)(const Hobject, Htuple *, Htuple *);
    using TLengthXldFn = Herror (*)(const Hobject, Htuple *);
    using FitLineContourXldFn = Herror (*)(const Hobject, const char *, Hlong, Hlong, Hlong, double,
                                           double *, double *, double *, double *, double *, double *, double *);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    CreateTupleIntFn createTupleInt = nullptr;
    CreateTupleDoubleFn createTupleDouble = nullptr;
    CreateTupleStringFn createTupleString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    CreateMetrologyModelFn createMetrologyModel = nullptr;
    AddMetrologyObjectLineMeasureFn addMetrologyObjectLineMeasure = nullptr;
    SetMetrologyObjectParamFn setMetrologyObjectParam = nullptr;
    ApplyMetrologyModelFn applyMetrologyModel = nullptr;
    GetMetrologyObjectResultFn getMetrologyObjectResult = nullptr;
    GetMetrologyObjectMeasuresFn getMetrologyObjectMeasures = nullptr;
    ClearMetrologyModelFn clearMetrologyModel = nullptr;
    GenImage1Fn genImage1 = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Rgb1ToGrayFn rgb1ToGray = nullptr;
    EdgesSubPixFn edgesSubPix = nullptr;
    SegmentContoursXldFn segmentContoursXld = nullptr;
    CountObjFn countObj = nullptr;
    SelectObjFn selectObj = nullptr;
    TGetContourXldFn getContourXld = nullptr;
    TLengthXldFn lengthXld = nullptr;
    FitLineContourXldFn fitLineContourXld = nullptr;
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
            !resolveRequired(m_handle, api.createTupleInt, "F_create_tuple_i", errorMessage) ||
            !resolveRequired(m_handle, api.createTupleDouble, "F_create_tuple_d", errorMessage) ||
            !resolveRequired(m_handle, api.createTupleString, "F_create_tuple_s", errorMessage) ||
            !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
            !resolveRequired(m_handle, api.createMetrologyModel, "T_create_metrology_model", errorMessage) ||
            !resolveRequired(m_handle, api.addMetrologyObjectLineMeasure, "T_add_metrology_object_line_measure", errorMessage) ||
            !resolveRequired(m_handle, api.setMetrologyObjectParam, "T_set_metrology_object_param", errorMessage) ||
            !resolveRequired(m_handle, api.applyMetrologyModel, "T_apply_metrology_model", errorMessage) ||
            !resolveRequired(m_handle, api.getMetrologyObjectResult, "T_get_metrology_object_result", errorMessage) ||
            !resolveRequired(m_handle, api.getMetrologyObjectMeasures, "T_get_metrology_object_measures", errorMessage) ||
            !resolveRequired(m_handle, api.clearMetrologyModel, "T_clear_metrology_model", errorMessage) ||
            !resolveRequired(m_handle, api.genImage1, "gen_image1", errorMessage) ||
            !resolveRequired(m_handle, api.genImageInterleaved, "gen_image_interleaved", errorMessage) ||
            !resolveRequired(m_handle, api.rgb1ToGray, "rgb1_to_gray", errorMessage) ||
            !resolveRequired(m_handle, api.edgesSubPix, "edges_sub_pix", errorMessage) ||
            !resolveRequired(m_handle, api.segmentContoursXld, "segment_contours_xld", errorMessage) ||
            !resolveRequired(m_handle, api.countObj, "count_obj", errorMessage) ||
            !resolveRequired(m_handle, api.selectObj, "select_obj", errorMessage) ||
            !resolveRequired(m_handle, api.getContourXld, "T_get_contour_xld", errorMessage) ||
            !resolveRequired(m_handle, api.lengthXld, "T_length_xld", errorMessage) ||
            !resolveRequired(m_handle, api.fitLineContourXld, "fit_line_contour_xld", errorMessage) ||
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

struct MetrologyLineGeometry
{
    QPointF p1;
    QPointF p2;
    double length = 0.0;
    double halfBandWidth = 0.0;
    bool valid = false;
    bool fallback = false;
    QString fallbackReason;
};

QJsonArray pointsArrayToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points)
        array.append(pointToJson(point));
    return array;
}

int metrologyThresholdForSensitivity(const int sensitivity)
{
    return qBound(5, 120 - qBound(0, sensitivity, 100), 120);
}

QString metrologyTransitionForPolarity(const QString &polarity)
{
    const QString normalized = normalizedEdgePolarity(polarity);
    if (normalized == QStringLiteral("black_to_white"))
        return QStringLiteral("positive");
    if (normalized == QStringLiteral("white_to_black"))
        return QStringLiteral("negative");
    return QStringLiteral("all");
}

QString metrologySelectForEdgeType(const QString &edgeType)
{
    const QString normalized = normalizedEdgeType(edgeType);
    if (normalized == QStringLiteral("first"))
        return QStringLiteral("first");
    if (normalized == QStringLiteral("last"))
        return QStringLiteral("last");
    return QStringLiteral("all");
}

MetrologyLineGeometry lineGeometryFromConfig(const LinePresenceHalconConfig &config,
                                             const cv::Mat &image,
                                             QString *fallbackReason)
{
    MetrologyLineGeometry geometry;
    const QPointF p1 = normalizedToPixelPoint(config.searchLineP1, image.cols, image.rows);
    const QPointF p2 = normalizedToPixelPoint(config.searchLineP2, image.cols, image.rows);
    const double lineLength = std::hypot(p2.x() - p1.x(), p2.y() - p1.y());
    const double halfBand = config.searchBandWidth * static_cast<double>(qMax(image.cols, image.rows)) / 2.0;
    if (lineLength > 2.0 && std::isfinite(halfBand) && halfBand > 0.5) {
        geometry.p1 = p1;
        geometry.p2 = p2;
        geometry.length = lineLength;
        geometry.halfBandWidth = qMax(1.0, halfBand);
        geometry.valid = true;
        return geometry;
    }

    bool roiTooSmall = false;
    const QRect roi = normalizedRoiToPixels(config.roiNormalized, image.cols, image.rows, &roiTooSmall);
    if (!roi.isEmpty()) {
        geometry.p1 = QPointF(roi.left(), roi.center().y());
        geometry.p2 = QPointF(roi.right(), roi.center().y());
        geometry.length = std::hypot(geometry.p2.x() - geometry.p1.x(),
                                     geometry.p2.y() - geometry.p1.y());
        geometry.halfBandWidth = qMax(1.0, static_cast<double>(roi.height()) / 2.0);
        geometry.valid = geometry.length > 2.0;
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

void appendMetrologyMeasureRectangleOverlays(QVector<ToolOverlay> *overlays,
                                             const MetrologyLineGeometry &geometry,
                                             const int sampleCount,
                                             const double measureLength2)
{
    if (!overlays || !geometry.valid || sampleCount <= 0 || geometry.length <= 0.0)
        return;

    const double ux = (geometry.p2.x() - geometry.p1.x()) / geometry.length;
    const double uy = (geometry.p2.y() - geometry.p1.y()) / geometry.length;
    const QPointF along(ux * measureLength2, uy * measureLength2);
    const QPointF normal(-uy * geometry.halfBandWidth, ux * geometry.halfBandWidth);

    const int overlayCount = qMin(sampleCount, 40);
    const int step = qMax(1, sampleCount / qMax(1, overlayCount));
    for (int index = 0; index < sampleCount; index += step) {
        const double t = sampleCount == 1
                ? 0.5
                : static_cast<double>(index) / static_cast<double>(sampleCount - 1);
        const QPointF center = geometry.p1 + (geometry.p2 - geometry.p1) * t;
        const QPointF a = center - along - normal;
        const QPointF b = center + along - normal;
        const QPointF c = center + along + normal;
        const QPointF d = center - along + normal;
        overlays->append(lineOverlay(a, b, QStringLiteral("measure_regions")));
        overlays->append(lineOverlay(b, c, QStringLiteral("measure_regions")));
        overlays->append(lineOverlay(c, d, QStringLiteral("measure_regions")));
        overlays->append(lineOverlay(d, a, QStringLiteral("measure_regions")));
    }
}

LinePresenceHalconResult makeMetrologyLineError(const QString &status,
                                                const QString &error,
                                                const cv::Mat &image,
                                                const LinePresenceHalconConfig &config)
{
    LinePresenceHalconResult result = makeParameterError(status, error, image, config);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("metrology_line"));
    result.payload.insert(QStringLiteral("okNgReason"), error);
    return result;
}

LinePresenceHalconResult runMetrologyLinePresence(const cv::Mat &image,
                                                  const LinePresenceHalconConfig &config)
{
    QElapsedTimer timer;
    timer.start();

    if (image.empty()) {
        LinePresenceHalconResult result = makeMetrologyLineError(QStringLiteral("image_empty"),
                                                                 QStringLiteral("input image is empty"),
                                                                 image,
                                                                 config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (!isSupportedDetectRegionType(config.detectRegionType)) {
        LinePresenceHalconResult result = makeMetrologyLineError(QStringLiteral("unsupported_detect_roi"),
                                                                 QStringLiteral("detect ROI shape is not supported"),
                                                                 image,
                                                                 config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (image.depth() != CV_8U ||
        (image.channels() != 1 && image.channels() != 3 && image.channels() != 4)) {
        LinePresenceHalconResult result = makeMetrologyLineError(QStringLiteral("unsupported_image_type"),
                                                                 QStringLiteral("LinePresence supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
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
        LinePresenceHalconResult result = makeMetrologyLineError(QStringLiteral("halcon_so_not_found"),
                                                                 QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                                                                 .arg(config.halconSoPath, triedPaths),
                                                                 image,
                                                                 config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    QString fallbackReason;
    MetrologyLineGeometry geometry = lineGeometryFromConfig(config, image, &fallbackReason);
    if (!geometry.valid) {
        LinePresenceHalconResult result = makeMetrologyLineError(QStringLiteral("invalid_line_band"),
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
        geometry.length = std::hypot(geometry.p2.x() - geometry.p1.x(),
                                     geometry.p2.y() - geometry.p1.y());
        geometry.halfBandWidth *= config.positionCorrection.scaleRatio;
        if (!std::isfinite(geometry.length) || geometry.length <= 2.0) {
            LinePresenceHalconResult errorResult =
                    makeMetrologyLineError(QStringLiteral("corrected_roi_out_of_image"),
                                           QStringLiteral("Position-corrected line ROI is invalid"),
                                           image,
                                           config);
            errorResult.elapsedMs = timer.elapsed();
            return errorResult;
        }
        const QRectF correctedBounds(
                    QPointF(qMin(geometry.p1.x(), geometry.p2.x()) - geometry.halfBandWidth,
                            qMin(geometry.p1.y(), geometry.p2.y()) - geometry.halfBandWidth),
                    QPointF(qMax(geometry.p1.x(), geometry.p2.x()) + geometry.halfBandWidth,
                            qMax(geometry.p1.y(), geometry.p2.y()) + geometry.halfBandWidth));
        const QRectF imageBounds(0.0, 0.0,
                                 static_cast<double>(image.cols),
                                 static_cast<double>(image.rows));
        if (!std::isfinite(geometry.halfBandWidth)
                || geometry.halfBandWidth <= 0.0
                || !correctedBounds.intersects(imageBounds)) {
            LinePresenceHalconResult errorResult =
                    makeMetrologyLineError(QStringLiteral("corrected_roi_out_of_image"),
                                           QStringLiteral("Position-corrected line ROI is outside the image"),
                                           image,
                                           config);
            errorResult.elapsedMs = timer.elapsed();
            return errorResult;
        }
    }

    LinePresenceHalconResult result;
    fillPayload(result, image, config);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("metrology_line"));
    result.payload.insert(QStringLiteral("roiMode"), geometry.fallback
                          ? QStringLiteral("roi_fallback_metrology")
                          : QStringLiteral("line_band_metrology"));
    result.payload.insert(QStringLiteral("lineBandApplied"), !geometry.fallback);
    result.payload.insert(QStringLiteral("lineBandFallback"), geometry.fallback);
    result.payload.insert(QStringLiteral("lineBandFallbackReason"), geometry.fallbackReason);

    const int sensitivityUsed = qBound(0, config.sensitivity, 100);
    const int lineDegreeUsed = qBound(0, config.lineDegree, 100);
    const int sampleCount = 20;
    const double sigmaUsed = 1.0;
    const int thresholdUsed = metrologyThresholdForSensitivity(sensitivityUsed);
    const QString transitionUsed = metrologyTransitionForPolarity(config.edgePolarity);
    const QString edgeTypeRequested = normalizedEdgeType(config.edgeType);
    const bool manualUnsupported = edgeTypeRequested == QStringLiteral("manual");
    const QString selectUsed = manualUnsupported
            ? QStringLiteral("all")
            : metrologySelectForEdgeType(edgeTypeRequested);
    const double minScoreUsed = 0.3;
    const int minHitCountUsed = qMax(3, qRound(static_cast<double>(sampleCount) * 0.3));
    const double maxFitErrorUsed = qBound(1.0, 8.0 - static_cast<double>(lineDegreeUsed) * 0.06, 8.0);
    const double measureLength1 = geometry.halfBandWidth;
    const double measureLength2 = qMax(1.0, geometry.length / (static_cast<double>(sampleCount) * 2.0));

    result.payload.insert(QStringLiteral("startPointRequested"), pointToJson(geometry.p1));
    result.payload.insert(QStringLiteral("endPointRequested"), pointToJson(geometry.p2));
    result.payload.insert(QStringLiteral("lineLengthRequested"), geometry.length);
    result.payload.insert(QStringLiteral("searchBandHalfWidthPx"), geometry.halfBandWidth);
    result.payload.insert(QStringLiteral("thresholdUsed"), thresholdUsed);
    result.payload.insert(QStringLiteral("sigmaUsed"), sigmaUsed);
    result.payload.insert(QStringLiteral("transitionUsed"), transitionUsed);
    result.payload.insert(QStringLiteral("selectUsed"), selectUsed);
    result.payload.insert(QStringLiteral("minScoreUsed"), minScoreUsed);
    result.payload.insert(QStringLiteral("minHitCountUsed"), minHitCountUsed);
    result.payload.insert(QStringLiteral("maxFitErrorUsed"), maxFitErrorUsed);
    result.payload.insert(QStringLiteral("sensitivityMapping"),
                          QStringLiteral("thresholdUsed=clamp(120-sensitivity,5,120)"));
    result.payload.insert(QStringLiteral("lineDegreeMapping"),
                          QStringLiteral("maxFitErrorUsed=clamp(8-lineDegree*0.06,1,8)"));
    result.payload.insert(QStringLiteral("sampleCount"), sampleCount);
    result.payload.insert(QStringLiteral("sampleCountUsed"), sampleCount);
    result.payload.insert(QStringLiteral("measureLength1Used"), measureLength1);
    result.payload.insert(QStringLiteral("measureLength2Used"), measureLength2);
    result.payload.insert(QStringLiteral("angleRangeApplied"), false);
    result.payload.insert(QStringLiteral("lengthRangeApplied"), QStringLiteral("internal"));
    result.payload.insert(QStringLiteral("edgeTypeRequested"), edgeTypeRequested);
    result.payload.insert(QStringLiteral("edgeTypeUsed"), manualUnsupported ? QStringLiteral("strongest") : edgeTypeRequested);
    result.payload.insert(QStringLiteral("manualUnsupported"), manualUnsupported);
    result.payload.insert(QStringLiteral("edgePolarityApplied"), true);
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
    const QVector<QPointF> band = lineBandPolygonPixels(config, image.cols, image.rows);
    if (!geometry.fallback && band.size() == 4) {
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
    appendMetrologyMeasureRectangleOverlays(&result.overlays, geometry, sampleCount, measureLength2);

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
    Hobject measureContours = NO_OBJECTS;
    QVector<Htuple *> tuples;
    Htuple metrologyHandleTuple = HTUPLE_INITIALIZER;
    Htuple rowBeginTuple = HTUPLE_INITIALIZER;
    Htuple colBeginTuple = HTUPLE_INITIALIZER;
    Htuple rowEndTuple = HTUPLE_INITIALIZER;
    Htuple colEndTuple = HTUPLE_INITIALIZER;
    Htuple measureLength1Tuple = HTUPLE_INITIALIZER;
    Htuple measureLength2Tuple = HTUPLE_INITIALIZER;
    Htuple sigmaTuple = HTUPLE_INITIALIZER;
    Htuple thresholdTuple = HTUPLE_INITIALIZER;
    Htuple emptyParamNameTuple = HTUPLE_INITIALIZER;
    Htuple emptyParamValueTuple = HTUPLE_INITIALIZER;
    Htuple indexTuple = HTUPLE_INITIALIZER;
    Htuple paramNameTuple = HTUPLE_INITIALIZER;
    Htuple paramValueTuple = HTUPLE_INITIALIZER;
    Htuple instanceTuple = HTUPLE_INITIALIZER;
    Htuple resultTypeNameTuple = HTUPLE_INITIALIZER;
    Htuple allParamTuple = HTUPLE_INITIALIZER;
    Htuple lineParamTuple = HTUPLE_INITIALIZER;
    Htuple transitionTuple = HTUPLE_INITIALIZER;
    Htuple measureRowsTuple = HTUPLE_INITIALIZER;
    Htuple measureColsTuple = HTUPLE_INITIALIZER;

    auto track = [&](Htuple &tuple) {
        if (!tuples.contains(&tuple))
            tuples.append(&tuple);
    };
    auto createEmptyTuple = [&](Htuple &tuple) {
        api->createTuple(&tuple, 0);
        track(tuple);
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
        clearObject(measureContours);
        clearObject(halconImage.grayImage);
        clearObject(halconImage.inputImage);
        if ((metrologyHandleTuple.num > 0 || metrologyHandleTuple.capacity > 0) &&
            api->clearMetrologyModel) {
            api->clearMetrologyModel(metrologyHandleTuple);
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
                    QStringLiteral("LinePresence HALCON error"),
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
        checkStatus(api->createMetrologyModel(&metrologyHandleTuple), QStringLiteral("create_metrology_model"));
        track(metrologyHandleTuple);
        createDoubleTuple(rowBeginTuple, geometry.p1.y());
        createDoubleTuple(colBeginTuple, geometry.p1.x());
        createDoubleTuple(rowEndTuple, geometry.p2.y());
        createDoubleTuple(colEndTuple, geometry.p2.x());
        createDoubleTuple(measureLength1Tuple, measureLength1);
        createDoubleTuple(measureLength2Tuple, measureLength2);
        createDoubleTuple(sigmaTuple, sigmaUsed);
        createIntTuple(thresholdTuple, thresholdUsed);
        createEmptyTuple(emptyParamNameTuple);
        createEmptyTuple(emptyParamValueTuple);
        checkStatus(api->addMetrologyObjectLineMeasure(metrologyHandleTuple,
                                                       rowBeginTuple,
                                                       colBeginTuple,
                                                       rowEndTuple,
                                                       colEndTuple,
                                                       measureLength1Tuple,
                                                       measureLength2Tuple,
                                                       sigmaTuple,
                                                       thresholdTuple,
                                                       emptyParamNameTuple,
                                                       emptyParamValueTuple,
                                                       &indexTuple),
                    QStringLiteral("add_metrology_object_line_measure"));
        track(indexTuple);

        auto setParam = [&](const QByteArray &name, const QVariant &value) {
            destroyTuple(paramNameTuple);
            destroyTuple(paramValueTuple);
            createStringTuple(paramNameTuple, name);
            if (value.type() == QVariant::String)
                createStringTuple(paramValueTuple, value.toString().toLatin1());
            else if (value.type() == QVariant::Double)
                createDoubleTuple(paramValueTuple, value.toDouble());
            else
                createIntTuple(paramValueTuple, static_cast<Hlong>(value.toInt()));
            checkStatus(api->setMetrologyObjectParam(metrologyHandleTuple,
                                                     indexTuple,
                                                     paramNameTuple,
                                                     paramValueTuple),
                        QStringLiteral("set_metrology_object_param.") + QString::fromLatin1(name));
        };

        setParam(QByteArray("num_instances"), QVariant(1));
        setParam(QByteArray("num_measures"), QVariant(sampleCount));
        setParam(QByteArray("measure_transition"), QVariant(transitionUsed));
        setParam(QByteArray("measure_select"), QVariant(selectUsed));
        setParam(QByteArray("min_score"), QVariant(minScoreUsed));

        checkStatus(api->applyMetrologyModel(halconImage.graySource, metrologyHandleTuple),
                    QStringLiteral("apply_metrology_model"));

        createStringTuple(instanceTuple, QByteArray("all"));
        createStringTuple(resultTypeNameTuple, QByteArray("result_type"));
        createStringTuple(allParamTuple, QByteArray("all_param"));
        checkStatus(api->getMetrologyObjectResult(metrologyHandleTuple,
                                                  indexTuple,
                                                  instanceTuple,
                                                  resultTypeNameTuple,
                                                  allParamTuple,
                                                  &lineParamTuple),
                    QStringLiteral("get_metrology_object_result"));
        track(lineParamTuple);
        createStringTuple(transitionTuple, transitionUsed.toLatin1());
        checkStatus(api->getMetrologyObjectMeasures(&measureContours,
                                                    metrologyHandleTuple,
                                                    indexTuple,
                                                    transitionTuple,
                                                    &measureRowsTuple,
                                                    &measureColsTuple),
                    QStringLiteral("get_metrology_object_measures"));
        track(measureRowsTuple);
        track(measureColsTuple);

        QVector<QPointF> samplePoints;
        const int hitCount = qMin<int>(measureRowsTuple.num, measureColsTuple.num);
        samplePoints.reserve(hitCount);
        for (int index = 0; index < hitCount; ++index) {
            const double row = api->getDouble(&measureRowsTuple, index);
            const double col = api->getDouble(&measureColsTuple, index);
            if (std::isfinite(row) && std::isfinite(col)) {
                const QPointF point(col, row);
                samplePoints.append(point);
                const double marker = 3.0;
                result.overlays.append(lineOverlay(QPointF(point.x() - marker, point.y()),
                                                   QPointF(point.x() + marker, point.y()),
                                                   QStringLiteral("sample_points")));
                result.overlays.append(lineOverlay(QPointF(point.x(), point.y() - marker),
                                                   QPointF(point.x(), point.y() + marker),
                                                   QStringLiteral("sample_points")));
            }
        }

        Hlong measureObjectCount = 0;
        if (halconObjectAllocated(measureContours) && api->countObj)
            api->countObj(measureContours, &measureObjectCount);

        QPointF lineStart;
        QPointF lineEnd;
        bool hasLine = lineParamTuple.num >= 4;
        if (hasLine) {
            const double row1 = api->getDouble(&lineParamTuple, 0);
            const double col1 = api->getDouble(&lineParamTuple, 1);
            const double row2 = api->getDouble(&lineParamTuple, 2);
            const double col2 = api->getDouble(&lineParamTuple, 3);
            if (std::isfinite(row1) && std::isfinite(col1) &&
                std::isfinite(row2) && std::isfinite(col2)) {
                lineStart = QPointF(col1, row1);
                lineEnd = QPointF(col2, row2);
            } else {
                hasLine = false;
            }
        }

        double lineLength = 0.0;
        double angleDeg = 0.0;
        double fitError = 999999.0;
        double score = 0.0;
        QPointF midpoint;
        if (hasLine) {
            lineLength = std::hypot(lineEnd.x() - lineStart.x(), lineEnd.y() - lineStart.y());
            angleDeg = std::atan2(lineEnd.y() - lineStart.y(), lineEnd.x() - lineStart.x()) * 180.0 / kPi;
            midpoint = QPointF((lineStart.x() + lineEnd.x()) / 2.0,
                               (lineStart.y() + lineEnd.y()) / 2.0);
            score = qBound(0.0, static_cast<double>(hitCount) / static_cast<double>(sampleCount), 1.0);
            if (lineLength > 0.001 && !samplePoints.isEmpty()) {
                double squaredDistanceSum = 0.0;
                for (const QPointF &point : samplePoints) {
                    const double distance = std::abs((point.x() - lineStart.x()) * (lineEnd.y() - lineStart.y()) -
                                                     (point.y() - lineStart.y()) * (lineEnd.x() - lineStart.x())) /
                                            lineLength;
                    squaredDistanceSum += distance * distance;
                }
                fitError = std::sqrt(squaredDistanceSum / static_cast<double>(samplePoints.size()));
            }
        }

        const bool found = hasLine &&
                           hitCount >= minHitCountUsed &&
                           score >= minScoreUsed &&
                           fitError <= maxFitErrorUsed;
        const bool ok = config.existOk ? found : !found;
        QString okNgReason;
        if (found) {
            okNgReason = config.existOk
                    ? QStringLiteral("found target, existOk=true")
                    : QStringLiteral("found target, existOk=false");
        } else if (!hasLine) {
            okNgReason = config.existOk
                    ? QStringLiteral("not found target, existOk=true")
                    : QStringLiteral("not found target, existOk=false");
        } else if (hitCount < minHitCountUsed) {
            okNgReason = QStringLiteral("hit count below minimum");
        } else if (score < minScoreUsed) {
            okNgReason = QStringLiteral("metrology score below threshold");
        } else {
            okNgReason = QStringLiteral("fit error above threshold");
        }

        if (hasLine)
            result.overlays.append(lineOverlay(lineStart, lineEnd, QStringLiteral("fitted_line"), score));

        result.success = true;
        result.ok = ok;
        result.score = score;
        result.value = lineLength;
        result.count = found ? 1 : 0;
        result.text = found ? QStringLiteral("found") : QStringLiteral("missing");
        result.status = QStringLiteral("LinePresence: %1 hit=%2 score=%3")
                .arg(result.text,
                     QString::number(hitCount),
                     QString::number(score, 'f', 3));
        result.message = result.status;
        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("lineCount"), found ? 1 : 0);
        result.payload.insert(QStringLiteral("startPoint"), pointToJson(lineStart));
        result.payload.insert(QStringLiteral("endPoint"), pointToJson(lineEnd));
        result.payload.insert(QStringLiteral("midpoint"), pointToJson(midpoint));
        result.payload.insert(QStringLiteral("angleDeg"), angleDeg);
        result.payload.insert(QStringLiteral("length"), lineLength);
        result.payload.insert(QStringLiteral("score"), score);
        result.payload.insert(QStringLiteral("fitError"), fitError);
        result.payload.insert(QStringLiteral("hitCount"), hitCount);
        result.payload.insert(QStringLiteral("samplePoints"), pointsArrayToJson(samplePoints));
        result.payload.insert(QStringLiteral("measureRegionsCount"), static_cast<int>(measureObjectCount));
        result.payload.insert(QStringLiteral("okNgReason"), okNgReason);
        result.payload.insert(QStringLiteral("text"), result.text);

        const QPointF textPosition = hasLine ? midpoint : geometry.p1;
        result.overlays.append(textOverlay(textPosition,
                                           QStringLiteral("line hit=%1 score=%2")
                                           .arg(hitCount)
                                           .arg(score, 0, 'f', 2),
                                           QStringLiteral("line_result_text"),
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
        result.status = QStringLiteral("LinePresence HALCON error");
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

LinePresenceHalconResult LinePresenceHalconRunner::run(
        const cv::Mat &image,
        const LinePresenceHalconConfig &config)
{
    return runMetrologyLinePresence(image, config);

    QElapsedTimer timer;
    timer.start();

    if (image.empty()) {
        LinePresenceHalconResult result = makeParameterError(QStringLiteral("image_empty"),
                                                             QStringLiteral("input image is empty"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isValidNormalizedRoi(config.roiNormalized)) {
        LinePresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                             QStringLiteral("detect ROI is invalid"),
                                                             image,
                                                             config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isSupportedDetectRegionType(config.detectRegionType)) {
        LinePresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_detect_roi"),
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
        LinePresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_image_type"),
                                                             QStringLiteral("LinePresence supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
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
        LinePresenceHalconResult result = makeParameterError(QStringLiteral("halcon_so_not_found"),
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
        LinePresenceHalconResult result = makeParameterError(roiTooSmall
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

    LinePresenceHalconResult result;
    fillPayload(result, image, config);

    const int sensitivityUsed = qBound(0, config.sensitivity, 100);
    const int lineDegreeUsed = qBound(0, config.lineDegree, 100);
    const int lineThresholdInternalUsed = highThresholdForSensitivity(sensitivityUsed);
    const int lineThresholdLowInternalUsed = lowThresholdForHighThreshold(lineThresholdInternalUsed);
    const double minLineLengthUsed = minLineLengthFor(detectRoiPixels);
    const double straightnessMinUsed = straightnessMinForLineDegree(lineDegreeUsed);
    const double maxFitErrorUsed = maxFitErrorForLineDegree(lineDegreeUsed);
    const double maxGapUsed = maxFitErrorUsed;
    const QString edgeTypeRequested = normalizedEdgeType(config.edgeType);
    const bool manualUnsupported = edgeTypeRequested == QStringLiteral("manual");
    const QString edgeTypeUsed = manualUnsupported ? QStringLiteral("strongest") : edgeTypeRequested;

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
    result.payload.insert(QStringLiteral("lineThresholdInternalUsed"), lineThresholdInternalUsed);
    result.payload.insert(QStringLiteral("lineThresholdLowInternalUsed"), lineThresholdLowInternalUsed);
    result.payload.insert(QStringLiteral("thresholdSource"), QStringLiteral("derived_from_sensitivity"));
    result.payload.insert(QStringLiteral("lineDegreeUsed"), lineDegreeUsed);
    result.payload.insert(QStringLiteral("lineDegreeApplied"), true);
    result.payload.insert(QStringLiteral("straightnessMinUsed"), straightnessMinUsed);
    result.payload.insert(QStringLiteral("straightnessSource"), QStringLiteral("derived_from_line_degree"));
    result.payload.insert(QStringLiteral("maxFitErrorUsed"), maxFitErrorUsed);
    result.payload.insert(QStringLiteral("maxFitErrorSource"), QStringLiteral("derived_from_line_degree"));
    result.payload.insert(QStringLiteral("fitErrorMetric"), QStringLiteral("rms_distance_to_fitted_line_px"));
    result.payload.insert(QStringLiteral("minLineLengthUsed"), minLineLengthUsed);
    result.payload.insert(QStringLiteral("minLineLengthSource"), QStringLiteral("internal_default"));
    result.payload.insert(QStringLiteral("minLineLengthRule"),
                          QStringLiteral("max(20, min(roiWidth, roiHeight) * 0.35)"));
    result.payload.insert(QStringLiteral("maxGapUsed"), maxGapUsed);
    result.payload.insert(QStringLiteral("maxGapSource"), QStringLiteral("internal_default"));
    result.payload.insert(QStringLiteral("segmentMaxLineDistUsed"), maxGapUsed);
    result.payload.insert(QStringLiteral("angleRangeSource"), QStringLiteral("not_configured"));
    result.payload.insert(QStringLiteral("countRuleSource"), QStringLiteral("presence_only"));
    result.payload.insert(QStringLiteral("edgeTypeRequested"), edgeTypeRequested);
    result.payload.insert(QStringLiteral("edgeTypeUsed"), edgeTypeUsed);
    result.payload.insert(QStringLiteral("edgeTypeApplied"), !manualUnsupported);
    result.payload.insert(QStringLiteral("manualUnsupported"), manualUnsupported);
    result.payload.insert(QStringLiteral("selectionScoreFormula"),
                          QStringLiteral("lineLength * straightnessFactor"));
    result.payload.insert(QStringLiteral("straightnessFactorFormula"),
                          QStringLiteral("straightness"));
    if (manualUnsupported)
        result.payload.insert(QStringLiteral("edgeTypeReason"), QStringLiteral("manual selection has no UI data; fallback to strongest"));
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
        LinePresenceHalconResult errorResult = makeParameterError(QStringLiteral("invalid_detect_roi"),
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
    Hobject lineSegments = NO_OBJECTS;

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
        clearObject(lineSegments);
        clearObject(edges);
        clearObject(detectImage.grayImage);
        clearObject(detectImage.inputImage);
    };

    auto checkStatus = [&](const Herror status, const QString &stage) {
        if (!halconStatusOk(status)) {
            throw std::pair<QString, QString>(
                    QStringLiteral("LinePresence HALCON error"),
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
                                     static_cast<Hlong>(lineThresholdLowInternalUsed),
                                     static_cast<Hlong>(lineThresholdInternalUsed)),
                    QStringLiteral("edges_sub_pix"));

        checkStatus(api->segmentContoursXld(edges,
                                            &lineSegments,
                                            "lines",
                                            5,
                                            maxGapUsed,
                                            maxGapUsed),
                    QStringLiteral("segment_contours_xld"));

        Hlong rawSegmentObjectCount = 0;
        checkStatus(api->countObj(lineSegments, &rawSegmentObjectCount),
                    QStringLiteral("count_obj.line_segments"));
        const int rawSegmentCount = qMax(0, static_cast<int>(rawSegmentObjectCount));

        QVector<LineCandidate> validCandidates;
        QJsonArray rawCandidatesJson;
        QJsonArray rejectedLinesJson;
        int rejectedLineCandidateCount = 0;

        for (int index = 0; index < rawSegmentCount; ++index) {
            Hobject selectedSegment = NO_OBJECTS;
            Htuple lengthTuple = HTUPLE_INITIALIZER;
            Htuple rowTuple = HTUPLE_INITIALIZER;
            Htuple colTuple = HTUPLE_INITIALIZER;

            try {
                checkStatus(api->selectObj(lineSegments,
                                           &selectedSegment,
                                           static_cast<Hlong>(index + 1)),
                            QStringLiteral("select_obj.line_segment"));

                checkStatus(api->lengthXld(selectedSegment, &lengthTuple),
                            QStringLiteral("length_xld.line_segment"));

                checkStatus(api->getContourXld(selectedSegment, &rowTuple, &colTuple),
                            QStringLiteral("get_contour_xld.line_segment"));

                double contourLength = 0.0;
                if (lengthTuple.num > 0) {
                    const double rawLength = api->getDouble(&lengthTuple, 0);
                    if (std::isfinite(rawLength))
                        contourLength = qMax(0.0, rawLength);
                }

                double rowBegin = 0.0;
                double colBegin = 0.0;
                double rowEnd = 0.0;
                double colEnd = 0.0;
                double nr = 0.0;
                double nc = 0.0;
                double hesseDistance = 0.0;
                Q_UNUSED(nr);
                Q_UNUSED(nc);
                Q_UNUSED(hesseDistance);
                checkStatus(api->fitLineContourXld(selectedSegment,
                                                   "tukey",
                                                   -1,
                                                   0,
                                                   5,
                                                   2.0,
                                                   &rowBegin,
                                                   &colBegin,
                                                   &rowEnd,
                                                   &colEnd,
                                                   &nr,
                                                   &nc,
                                                   &hesseDistance),
                            QStringLiteral("fit_line_contour_xld"));

                QPointF localP1(colBegin, rowBegin);
                QPointF localP2(colEnd, rowEnd);
                const double lineLength = std::hypot(localP2.x() - localP1.x(),
                                                     localP2.y() - localP1.y());

                if (contourLength <= 0.0)
                    contourLength = lineLength;

                const double straightness = qBound(0.0,
                                                   lineLength / qMax(1.0, contourLength),
                                                   1.0);
                double fitError = 999999.0;
                const Hlong pointCount = qMin(rowTuple.num, colTuple.num);
                if (lineLength > 0.001 && pointCount > 0) {
                    double squaredDistanceSum = 0.0;
                    for (Hlong pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
                        const double row = api->getDouble(&rowTuple, pointIndex);
                        const double col = api->getDouble(&colTuple, pointIndex);
                        const double distance = std::abs((col - localP1.x()) * (localP2.y() - localP1.y()) -
                                                         (row - localP1.y()) * (localP2.x() - localP1.x())) /
                                                lineLength;
                        squaredDistanceSum += distance * distance;
                    }
                    fitError = std::sqrt(squaredDistanceSum / static_cast<double>(pointCount));
                }
                const double normalizedFitError = std::isfinite(fitError) ? std::abs(fitError) : 999999.0;
                const double straightnessFactor = qBound(0.0, straightness, 1.0);

                LineCandidate candidate;
                candidate.p1 = QPointF(detectRoiPixels.x() + localP1.x(),
                                       detectRoiPixels.y() + localP1.y());
                candidate.p2 = QPointF(detectRoiPixels.x() + localP2.x(),
                                       detectRoiPixels.y() + localP2.y());
                candidate.length = lineLength;
                candidate.contourLength = contourLength;
                candidate.straightness = straightness;
                candidate.straightnessFactor = straightnessFactor;
                candidate.fitError = normalizedFitError;
                candidate.score = lineLength * straightnessFactor;
                candidate.scanX = qMin(candidate.p1.x(), candidate.p2.x());
                candidate.scanY = qMin(candidate.p1.y(), candidate.p2.y());
                candidate.sourceIndex = index + 1;

                if (rawCandidatesJson.size() < kMaxRawLineCandidatesInPayload)
                    rawCandidatesJson.append(candidateToJson(candidate));

                QJsonArray rejectReasons;
                if (candidate.length < minLineLengthUsed)
                    rejectReasons.append(QStringLiteral("length_below_internal_min"));
                if (candidate.straightness < straightnessMinUsed)
                    rejectReasons.append(QStringLiteral("straightness_below_line_degree_min"));
                if (candidate.fitError > maxFitErrorUsed)
                    rejectReasons.append(QStringLiteral("fit_error_above_line_degree_max"));

                if (rejectReasons.isEmpty()) {
                    validCandidates.append(candidate);
                } else {
                    ++rejectedLineCandidateCount;
                    if (rejectedLinesJson.size() < kMaxRawLineCandidatesInPayload) {
                        QJsonObject rejectedJson = candidateToJson(candidate);
                        rejectedJson.insert(QStringLiteral("rejectReasons"), rejectReasons);
                        rejectedLinesJson.append(rejectedJson);
                    }
                }

                destroyTuple(colTuple);
                destroyTuple(rowTuple);
                destroyTuple(lengthTuple);
                clearObject(selectedSegment);
            } catch (...) {
                destroyTuple(colTuple);
                destroyTuple(rowTuple);
                destroyTuple(lengthTuple);
                clearObject(selectedSegment);
                throw;
            }
        }

        QVector<LineCandidate> selectedLines;
        if (!validCandidates.isEmpty()) {
            if (edgeTypeUsed == QStringLiteral("first") || edgeTypeUsed == QStringLiteral("last")) {
                std::stable_sort(validCandidates.begin(), validCandidates.end(), [](const LineCandidate &a,
                                                                                    const LineCandidate &b) {
                    if (!qFuzzyCompare(a.scanY + 1.0, b.scanY + 1.0))
                        return a.scanY < b.scanY;
                    if (!qFuzzyCompare(a.scanX + 1.0, b.scanX + 1.0))
                        return a.scanX < b.scanX;
                    return a.sourceIndex < b.sourceIndex;
                });

                selectedLines.append(edgeTypeUsed == QStringLiteral("last")
                                     ? validCandidates.last()
                                     : validCandidates.first());
            } else {
                std::stable_sort(validCandidates.begin(), validCandidates.end(), [](const LineCandidate &a,
                                                                                    const LineCandidate &b) {
                    if (!qFuzzyCompare(a.score + 1.0, b.score + 1.0))
                        return a.score > b.score;
                    if (!qFuzzyCompare(a.length + 1.0, b.length + 1.0))
                        return a.length > b.length;
                    return a.sourceIndex < b.sourceIndex;
                });
                selectedLines.append(validCandidates.first());
            }
        }

        QJsonArray selectedLinesJson;
        double totalLineLength = 0.0;
        double bestLineLength = 0.0;
        double bestScore = 0.0;

        for (const LineCandidate &line : selectedLines) {
            selectedLinesJson.append(candidateToJson(line));
            totalLineLength += line.length;
            bestLineLength = qMax(bestLineLength, line.length);
            bestScore = qMax(bestScore, line.score);
            result.overlays.append(lineOverlay(line.p1, line.p2, QStringLiteral("line"), line.score));
        }

        const int lineCount = selectedLines.size();
        const bool found = lineCount > 0;
        const bool ok = config.existOk ? found : !found;
        const double roiDiagonal = std::hypot(static_cast<double>(detectRoiPixels.width()),
                                              static_cast<double>(detectRoiPixels.height()));
        const double score = found ? qBound(0.0, bestScore / qMax(1.0, roiDiagonal), 1.0) : 0.0;

        result.success = true;
        result.ok = ok;
        result.score = score;
        result.value = totalLineLength;
        result.count = lineCount;
        result.text = found ? QStringLiteral("found") : QStringLiteral("missing");
        result.status = QStringLiteral("LinePresence: %1 count=%2 length=%3")
                .arg(result.text,
                     QString::number(lineCount),
                     QString::number(totalLineLength, 'f', 1));
        result.message = result.status;

        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("lineCount"), lineCount);
        result.payload.insert(QStringLiteral("rawLineSegmentCount"), rawSegmentCount);
        result.payload.insert(QStringLiteral("validLineCandidateCount"), validCandidates.size());
        result.payload.insert(QStringLiteral("rejectedLineCandidateCount"), rejectedLineCandidateCount);
        result.payload.insert(QStringLiteral("totalLineLength"), totalLineLength);
        result.payload.insert(QStringLiteral("bestLineLength"), bestLineLength);
        result.payload.insert(QStringLiteral("bestLineScore"), bestScore);
        result.payload.insert(QStringLiteral("bestLineScoreNormalized"), score);
        result.payload.insert(QStringLiteral("lines"), selectedLinesJson);
        result.payload.insert(QStringLiteral("rawLineCandidates"), rawCandidatesJson);
        result.payload.insert(QStringLiteral("rejectedLines"), rejectedLinesJson);
        result.payload.insert(QStringLiteral("score"), score);
        result.payload.insert(QStringLiteral("text"), result.text);

        const QPointF countPosition = countOverlayPosition(detectRoiPixels, image.rows);
        result.overlays.append(textOverlay(countPosition,
                                           QStringLiteral("lines=%1 length=%2")
                                           .arg(lineCount)
                                           .arg(totalLineLength, 0, 'f', 1),
                                           QStringLiteral("line_count"),
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
        result.status = QStringLiteral("LinePresence HALCON error");
        result.message = QString::fromLocal8Bit(error.what());
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
}
