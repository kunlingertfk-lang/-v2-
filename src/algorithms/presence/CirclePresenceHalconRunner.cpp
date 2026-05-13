#include "algorithms/presence/CirclePresenceHalconRunner.h"

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

QJsonObject circleToJson(const QPointF &center,
                         const double radius,
                         const double area,
                         const double circularity,
                         const QString &polarity)
{
    QJsonObject json;
    json.insert(QStringLiteral("centerX"), center.x());
    json.insert(QStringLiteral("centerY"), center.y());
    json.insert(QStringLiteral("radius"), radius);
    json.insert(QStringLiteral("area"), area);
    json.insert(QStringLiteral("circularity"), circularity);
    json.insert(QStringLiteral("polarity"), polarity);
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
           key.contains(QStringLiteral("矩形"));
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
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("sensitivity"), qBound(0, config.sensitivity, 100));
    result.payload.insert(QStringLiteral("roundness"), qBound(0, config.roundness, 100));
    result.payload.insert(QStringLiteral("edgePolarity"), normalizedEdgePolarity(config.edgePolarity));
    result.payload.insert(QStringLiteral("edgeType"), config.edgeType);
    result.payload.insert(QStringLiteral("judgeBasis"), QStringLiteral("presence"));
    result.payload.insert(QStringLiteral("existOk"), config.existOk);
    result.payload.insert(QStringLiteral("timeoutMs"), config.timeoutMs);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_region_circularity"));
    result.payload.insert(QStringLiteral("edgePolarityApplied"), true);
    result.payload.insert(QStringLiteral("edgeTypeApplied"), false);
    result.payload.insert(QStringLiteral("roundnessApplied"), true);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("maskApplied"), false);
    result.payload.insert(QStringLiteral("detectMaskApplied"), false);
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
    return result;
}

struct HalconCApi
{
    using SetUtf8Fn = void (*)(int);
    using GetErrorTextFn = Herror (*)(Hlong, char *);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GenImage1Fn = Herror (*)(Hobject *, const char *, Hlong, Hlong, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Rgb1ToGrayFn = Herror (*)(const Hobject, Hobject *);
    using BinaryThresholdFn = Herror (*)(const Hobject, Hobject *, const char *, const char *, Hlong *);
    using ThresholdFn = Herror (*)(const Hobject, Hobject *, double, double);
    using ConnectionFn = Herror (*)(const Hobject, Hobject *);
    using SelectShapeFn = Herror (*)(const Hobject, Hobject *, const char *, const char *, double, double);
    using CountObjFn = Herror (*)(const Hobject, Hlong *);
    using TSmallestCircleFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using TAreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using TCircularityFn = Herror (*)(const Hobject, Htuple *);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GenImage1Fn genImage1 = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Rgb1ToGrayFn rgb1ToGray = nullptr;
    BinaryThresholdFn binaryThreshold = nullptr;
    ThresholdFn threshold = nullptr;
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
    QString polarity;
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
        CirclePresenceHalconResult result = makeParameterError(QStringLiteral("halcon_so_not_found"),
                                                               QStringLiteral("HALCON runtime file not found: %1").arg(config.halconSoPath),
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
    const int thresholdMarginUsed = thresholdMarginFor(sensitivityUsed);
    const double circularityMinUsed = circularityMinFor(roundnessUsed, sensitivityUsed);
    const double edgeThresholdUsed = edgeThresholdForPayload(sensitivityUsed);
    const double radiusMinUsed = 3.0;
    const double radiusMaxUsed = qMax(radiusMinUsed,
                                      static_cast<double>(qMin(detectRoiPixels.width(),
                                                               detectRoiPixels.height())) / 2.0);
    const double areaMinUsed = kPi * radiusMinUsed * radiusMinUsed;
    const double areaMaxUsed = kPi * radiusMaxUsed * radiusMaxUsed;

    result.payload.insert(QStringLiteral("detectRoi"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoiPixels"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoiPixelX"), detectRoiPixels.x());
    result.payload.insert(QStringLiteral("detectRoiPixelY"), detectRoiPixels.y());
    result.payload.insert(QStringLiteral("detectRoiPixelW"), detectRoiPixels.width());
    result.payload.insert(QStringLiteral("detectRoiPixelH"), detectRoiPixels.height());
    result.payload.insert(QStringLiteral("roiMode"), QStringLiteral("rectangle"));
    result.payload.insert(QStringLiteral("sensitivityUsed"), sensitivityUsed);
    result.payload.insert(QStringLiteral("thresholdMarginUsed"), thresholdMarginUsed);
    result.payload.insert(QStringLiteral("edgeThresholdUsed"), edgeThresholdUsed);
    result.payload.insert(QStringLiteral("radiusMinUsed"), radiusMinUsed);
    result.payload.insert(QStringLiteral("radiusMaxUsed"), radiusMaxUsed);
    result.payload.insert(QStringLiteral("areaMinUsed"), areaMinUsed);
    result.payload.insert(QStringLiteral("areaMaxUsed"), areaMaxUsed);
    result.payload.insert(QStringLiteral("circularityMinUsed"), circularityMinUsed);
    result.payload.insert(QStringLiteral("minCount"), 1);
    result.payload.insert(QStringLiteral("maxCount"), -1);
    result.payload.insert(QStringLiteral("scoreThresholdApplied"), false);
    result.overlays.append(rectOverlay(QRectF(detectRoiPixels), QStringLiteral("ROI")));

    cv::Mat detectMat = image(cv::Rect(detectRoiPixels.x(),
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
            checkStatus(api->binaryThreshold(detectImage.graySource,
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
                    : qBound(0.0, static_cast<double>(thresholdCenter - thresholdMarginUsed), 255.0);

            thresholdLowUsed = qMin(thresholdLowUsed, low);
            thresholdHighUsed = qMax(thresholdHighUsed, high);

            QJsonObject passJson;
            passJson.insert(QStringLiteral("polarity"), pass.polarity);
            passJson.insert(QStringLiteral("lightDark"), QString::fromLatin1(pass.lightDark));
            passJson.insert(QStringLiteral("binaryThresholdUsed"), static_cast<int>(binaryThresholdUsed));
            passJson.insert(QStringLiteral("thresholdLowUsed"), low);
            passJson.insert(QStringLiteral("thresholdHighUsed"), high);

            checkStatus(api->threshold(detectImage.graySource,
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

                    const QPointF center(localCol + detectRoiPixels.x(),
                                         localRow + detectRoiPixels.y());
                    if (isDuplicateCircle(detectedCircles, center, radius))
                        continue;

                    DetectedCircle circle;
                    circle.center = center;
                    circle.radius = radius;
                    circle.area = index < areaTuple.num ? api->getDouble(&areaTuple, index) : 0.0;
                    circle.circularity = index < circularityTuple.num
                            ? api->getDouble(&circularityTuple, index)
                            : 0.0;
                    circle.polarity = pass.polarity;
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

        for (const ThresholdPass &pass : passes)
            runThresholdPass(pass);

        if (thresholdLowUsed > thresholdHighUsed) {
            thresholdLowUsed = 0.0;
            thresholdHighUsed = 0.0;
        }

        QJsonArray circlesArray;
        for (const DetectedCircle &circle : detectedCircles) {
            circlesArray.append(circleToJson(circle.center,
                                             circle.radius,
                                             circle.area,
                                             circle.circularity,
                                             circle.polarity));

            result.overlays.append(circleOverlay(circle.center,
                                                 circle.radius,
                                                 QStringLiteral("circle"),
                                                 1.0));

            const double crossRadius = qBound(4.0, circle.radius * 0.25, 16.0);
            result.overlays.append(lineOverlay(QPointF(circle.center.x() - crossRadius, circle.center.y()),
                                               QPointF(circle.center.x() + crossRadius, circle.center.y()),
                                               QStringLiteral("circle_center"),
                                               1.0));
            result.overlays.append(lineOverlay(QPointF(circle.center.x(), circle.center.y() - crossRadius),
                                               QPointF(circle.center.x(), circle.center.y() + crossRadius),
                                               QStringLiteral("circle_center"),
                                               1.0));
        }

        const int circleCount = detectedCircles.size();
        const bool found = circleCount > 0;
        const bool ok = config.existOk ? found : !found;

        result.success = true;
        result.ok = ok;
        result.score = found ? 1.0 : 0.0;
        result.count = circleCount;
        result.text = found ? QStringLiteral("found") : QStringLiteral("missing");
        result.status = QStringLiteral("CirclePresence: %1 count=%2")
                .arg(result.text, QString::number(circleCount));
        result.message = result.status;

        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("circleCount"), circleCount);
        result.payload.insert(QStringLiteral("circles"), circlesArray);
        result.payload.insert(QStringLiteral("thresholdPasses"), thresholdPassesJson);
        result.payload.insert(QStringLiteral("thresholdLowUsed"), thresholdLowUsed);
        result.payload.insert(QStringLiteral("thresholdHighUsed"), thresholdHighUsed);
        result.payload.insert(QStringLiteral("edgePolarityApplied"), true);
        result.payload.insert(QStringLiteral("edgeTypeApplied"), false);
        result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
        result.payload.insert(QStringLiteral("maskApplied"), false);
        result.payload.insert(QStringLiteral("text"), result.text);

        const QPointF countPosition = countOverlayPosition(detectRoiPixels,
                                                           QSize(image.cols, image.rows));
        result.overlays.append(textOverlay(countPosition,
                                           QStringLiteral("count=%1").arg(circleCount),
                                           QStringLiteral("circle_count"),
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
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
}
