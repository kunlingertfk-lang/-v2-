#include "algorithms/presence/PatternPresenceHalconRunner.h"

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
#include <opencv2/core.hpp>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kMinRoiPixelSize = 2;

bool halconStatusOk(const Herror status)
{
    return status == H_MSG_OK || status == H_MSG_TRUE || status == H_MSG_FALSE;
}

bool halconObjectAllocated(const Hobject object)
{
    return object != 0 && object != NO_OBJECTS && object != EMPTY_REGION;
}

double normalizedScore(const int percentScore)
{
    return qBound(0.0, static_cast<double>(percentScore) / 100.0, 1.0);
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

QJsonArray scoresToJson(const QVector<double> &scores)
{
    QJsonArray array;
    for (const double score : scores)
        array.append(score);
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

QString halconMetricForPolarity(const QString &polarity)
{
    const QString key = polarity.trimmed().toLower();
    if (key == QStringLiteral("ignore_polarity") ||
        key == QStringLiteral("ignore_global_polarity") ||
        key.contains(QStringLiteral("ignore")) ||
        key.contains(QStringLiteral("不考虑")) ||
        key.contains(QStringLiteral("忽略"))) {
        return QStringLiteral("ignore_global_polarity");
    }

    return QStringLiteral("use_polarity");
}

bool isPresenceJudgeBasis(const QString &judgeBasis)
{
    const QString key = judgeBasis.trimmed().toLower();
    return key == QStringLiteral("presence") ||
           key == QStringLiteral("resultpresence") ||
           key == QStringLiteral("result_presence") ||
           key == QStringLiteral("hasresult") ||
           key == QStringLiteral("existence") ||
           key.contains(QStringLiteral("有无"));
}

bool isScoreJudgeBasis(const QString &judgeBasis)
{
    const QString key = judgeBasis.trimmed().toLower();
    return key == QStringLiteral("score") ||
           key == QStringLiteral("minscore") ||
           key == QStringLiteral("min_score") ||
           key == QStringLiteral("lowestscore") ||
           key.contains(QStringLiteral("得分"));
}

bool isRectangleTemplateType(const QString &shapeType)
{
    const QString key = shapeType.trimmed().toLower();
    return key.isEmpty() ||
           key == QStringLiteral("rectangle") ||
           key == QStringLiteral("rect") ||
           key.contains(QStringLiteral("矩形"));
}

bool isSupportedDetectRegionType(const QString &regionType)
{
    const QString key = regionType.trimmed().toLower();
    // TODO: Free-form/polygon detection regions currently arrive as a rectangular
    // QRectF through ToolConfig. Add real polygon domain support when the UI stores it.
    return key.isEmpty() ||
           key == QStringLiteral("rectangle") ||
           key == QStringLiteral("rect") ||
           key == QStringLiteral("free") ||
           key.contains(QStringLiteral("矩形"));
}

void fillPayload(PatternPresenceHalconResult &result,
                 const cv::Mat &image,
                 const cv::Mat &referenceImage,
                 const PatternPresenceHalconConfig &config)
{
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("hasReferenceImage"), !referenceImage.empty());
    result.payload.insert(QStringLiteral("referenceImageWidth"), referenceImage.empty() ? 0 : referenceImage.cols);
    result.payload.insert(QStringLiteral("referenceImageHeight"), referenceImage.empty() ? 0 : referenceImage.rows);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("templateRoiNormalized"), rectToJson(config.templateRoiNormalized));
    result.payload.insert(QStringLiteral("templateSource"), config.templateSource);
    result.payload.insert(QStringLiteral("templateImagePath"), config.templateImagePath);
    result.payload.insert(QStringLiteral("modelPath"), config.modelPath);
    result.payload.insert(QStringLiteral("modelAutoCreate"), config.modelAutoCreate);
    result.payload.insert(QStringLiteral("modelCacheKey"), config.modelCacheKey);
    result.payload.insert(QStringLiteral("templateShapeType"), config.templateShapeType);
    result.payload.insert(QStringLiteral("templateSensitivityMode"), config.templateSensitivityMode);
    result.payload.insert(QStringLiteral("templateSensitivity"), config.templateSensitivity);
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("minScore"), config.minScore);
    result.payload.insert(QStringLiteral("polarity"), config.polarity);
    result.payload.insert(QStringLiteral("scaleMin"), config.scaleMin);
    result.payload.insert(QStringLiteral("scaleMax"), config.scaleMax);
    result.payload.insert(QStringLiteral("angleStart"), config.angleStart);
    result.payload.insert(QStringLiteral("angleExtent"), config.angleExtent);
    result.payload.insert(QStringLiteral("timeoutMs"), config.timeoutMs);
    result.payload.insert(QStringLiteral("showContourPoints"), config.showContourPoints);
    result.payload.insert(QStringLiteral("sortMode"), config.sortMode);
    result.payload.insert(QStringLiteral("judgeBasis"), config.judgeBasis);
    result.payload.insert(QStringLiteral("existOk"), config.existOk);
    result.payload.insert(QStringLiteral("scoreThreshold"), config.scoreThreshold);
    result.payload.insert(QStringLiteral("maxCount"), config.maxCount);
    result.payload.insert(QStringLiteral("expectedCount"), config.expectedCount);
    result.payload.insert(QStringLiteral("scaleApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("templateMaskApplied"), false);
    result.payload.insert(QStringLiteral("detectMaskApplied"), false);
}

PatternPresenceHalconResult makeParameterError(const QString &status,
                                               const QString &error,
                                               const cv::Mat &image,
                                               const cv::Mat &referenceImage,
                                               const PatternPresenceHalconConfig &config)
{
    PatternPresenceHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = error;
    result.text = QStringLiteral("error");
    result.payload.insert(QStringLiteral("error"), error);
    fillPayload(result, image, referenceImage, config);
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
    using SetDoubleFn = void (*)(Htuple *, double, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetHandleFn = Hphandle (*)(const Htuple *, Hlong);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GenImage1Fn = Herror (*)(Hobject *, const char *, Hlong, Hlong, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Rgb1ToGrayFn = Herror (*)(const Hobject, Hobject *);
    using CreateShapeModelFn = Herror (*)(const Hobject, const Htuple, const Htuple, const Htuple,
                                          const Htuple, const Htuple, const Htuple, const Htuple,
                                          const Htuple, Htuple *);
    using FindShapeModelFn = Herror (*)(const Hobject, const Htuple, const Htuple, const Htuple,
                                        const Htuple, const Htuple, const Htuple, const Htuple,
                                        const Htuple, const Htuple, Htuple *, Htuple *, Htuple *, Htuple *);
    using ClearShapeModelFn = Herror (*)(const Htuple);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    CreateTupleIntFn createTupleInt = nullptr;
    CreateTupleDoubleFn createTupleDouble = nullptr;
    CreateTupleStringFn createTupleString = nullptr;
    SetDoubleFn setDouble = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetHandleFn getHandle = nullptr;
    GetDoubleFn getDouble = nullptr;
    GenImage1Fn genImage1 = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Rgb1ToGrayFn rgb1ToGray = nullptr;
    CreateShapeModelFn createShapeModel = nullptr;
    FindShapeModelFn findShapeModel = nullptr;
    ClearShapeModelFn clearShapeModel = nullptr;
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
            !resolveRequired(m_handle, api.setDouble, "F_set_d", errorMessage) ||
            !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getHandle, "F_get_h", errorMessage) ||
            !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
            !resolveRequired(m_handle, api.genImage1, "gen_image1", errorMessage) ||
            !resolveRequired(m_handle, api.genImageInterleaved, "gen_image_interleaved", errorMessage) ||
            !resolveRequired(m_handle, api.rgb1ToGray, "rgb1_to_gray", errorMessage) ||
            !resolveRequired(m_handle, api.createShapeModel, "T_create_shape_model", errorMessage) ||
            !resolveRequired(m_handle, api.findShapeModel, "T_find_shape_model", errorMessage) ||
            !resolveRequired(m_handle, api.clearShapeModel, "T_clear_shape_model", errorMessage) ||
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

PatternPresenceHalconResult PatternPresenceHalconRunner::run(
        const cv::Mat &image,
        const cv::Mat &referenceImage,
        const PatternPresenceHalconConfig &config)
{
    QElapsedTimer timer;
    timer.start();

    if (image.empty()) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("image_empty"),
                                                                QStringLiteral("input image is empty"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (referenceImage.empty()) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("reference_image_empty"),
                                                                QStringLiteral("reference image is empty"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    const QString templateSource = config.templateSource.trimmed();
    if (!templateSource.isEmpty() &&
        templateSource.compare(QStringLiteral("referenceImage"), Qt::CaseInsensitive) != 0) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_template_source"),
                                                                QStringLiteral("template source is not supported"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("todo"), QStringLiteral("template files and model paths are not implemented"));
        return result;
    }

    if (!isValidNormalizedRoi(config.templateRoiNormalized)) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_template_roi"),
                                                                QStringLiteral("template ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isValidNormalizedRoi(config.roiNormalized)) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                                QStringLiteral("detect ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isRectangleTemplateType(config.templateShapeType)) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_template_roi"),
                                                                QStringLiteral("template ROI shape is not supported"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("todo"), QStringLiteral("polygon template ROI is not implemented"));
        return result;
    }

    if (!isSupportedDetectRegionType(config.detectRegionType)) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_detect_roi"),
                                                                QStringLiteral("detect ROI shape is not supported"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("todo"), QStringLiteral("circle/polygon detect ROI is not implemented"));
        return result;
    }

    if (config.scaleMin != 100 || config.scaleMax != 100) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_scale_range"),
                                                                QStringLiteral("scale range is not supported in the first version"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("todo"), QStringLiteral("scaled shape model support is not implemented"));
        return result;
    }

    if (image.depth() != CV_8U || referenceImage.depth() != CV_8U ||
        (image.channels() != 1 && image.channels() != 3 && image.channels() != 4) ||
        (referenceImage.channels() != 1 && referenceImage.channels() != 3 && referenceImage.channels() != 4)) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_image_type"),
                                                                QStringLiteral("PatternPresence supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (config.halconSoPath.trimmed().isEmpty() || !QFileInfo::exists(config.halconSoPath)) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("halcon_so_not_found"),
                                                                QStringLiteral("HALCON runtime file not found: %1").arg(config.halconSoPath),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    const QRect templateRoiPixels = normalizedRoiToPixels(config.templateRoiNormalized,
                                                          referenceImage.cols,
                                                          referenceImage.rows);
    if (templateRoiPixels.isEmpty()) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_template_roi"),
                                                                QStringLiteral("template ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    const QRect detectRoiPixels = normalizedRoiToPixels(config.roiNormalized,
                                                        image.cols,
                                                        image.rows);
    if (detectRoiPixels.isEmpty()) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                                QStringLiteral("detect ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    PatternPresenceHalconResult result;
    fillPayload(result, image, referenceImage, config);
    result.payload.insert(QStringLiteral("templateRoi"), rectToJson(QRectF(templateRoiPixels)));
    result.payload.insert(QStringLiteral("templateRoiPixels"), rectToJson(QRectF(templateRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoi"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoiPixels"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("shapeModelOperator"), QStringLiteral("create_shape_model/find_shape_model"));
    result.payload.insert(QStringLiteral("shapeModelOrigin"), QStringLiteral("halcon_default"));
    result.payload.insert(QStringLiteral("roiMode"), QStringLiteral("rectangle"));
    result.payload.insert(QStringLiteral("detectRoiCoordinates"), QStringLiteral("original_image_pixels"));
    result.payload.insert(QStringLiteral("templateRoiCoordinates"), QStringLiteral("reference_image_pixels"));
    result.payload.insert(QStringLiteral("templateRoiPixelX"), templateRoiPixels.x());
    result.payload.insert(QStringLiteral("templateRoiPixelY"), templateRoiPixels.y());
    result.payload.insert(QStringLiteral("templateRoiPixelW"), templateRoiPixels.width());
    result.payload.insert(QStringLiteral("templateRoiPixelH"), templateRoiPixels.height());
    result.payload.insert(QStringLiteral("detectRoiPixelX"), detectRoiPixels.x());
    result.payload.insert(QStringLiteral("detectRoiPixelY"), detectRoiPixels.y());
    result.payload.insert(QStringLiteral("detectRoiPixelW"), detectRoiPixels.width());
    result.payload.insert(QStringLiteral("detectRoiPixelH"), detectRoiPixels.height());
    result.overlays.append(rectOverlay(QRectF(detectRoiPixels), QStringLiteral("ROI")));

    cv::Mat templateMat = referenceImage(cv::Rect(templateRoiPixels.x(),
                                                  templateRoiPixels.y(),
                                                  templateRoiPixels.width(),
                                                  templateRoiPixels.height())).clone();
    cv::Mat detectMat = image(cv::Rect(detectRoiPixels.x(),
                                       detectRoiPixels.y(),
                                       detectRoiPixels.width(),
                                       detectRoiPixels.height())).clone();
    if (templateMat.empty()) {
        PatternPresenceHalconResult errorResult = makeParameterError(QStringLiteral("invalid_template_roi"),
                                                                     QStringLiteral("template ROI is invalid"),
                                                                     image,
                                                                     referenceImage,
                                                                     config);
        errorResult.elapsedMs = timer.elapsed();
        errorResult.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(errorResult.elapsedMs));
        return errorResult;
    }
    if (detectMat.empty()) {
        PatternPresenceHalconResult errorResult = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                                     QStringLiteral("detect ROI is invalid"),
                                                                     image,
                                                                     referenceImage,
                                                                     config);
        errorResult.elapsedMs = timer.elapsed();
        errorResult.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(errorResult.elapsedMs));
        return errorResult;
    }

    if (!templateMat.isContinuous())
        templateMat = templateMat.clone();
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

    GrayHalconImage templateImage;
    GrayHalconImage detectImage;
    Htuple createNumLevelsTuple = HTUPLE_INITIALIZER;
    Htuple angleStartTuple = HTUPLE_INITIALIZER;
    Htuple angleExtentTuple = HTUPLE_INITIALIZER;
    Htuple angleStepTuple = HTUPLE_INITIALIZER;
    Htuple optimizationTuple = HTUPLE_INITIALIZER;
    Htuple metricTuple = HTUPLE_INITIALIZER;
    Htuple contrastTuple = HTUPLE_INITIALIZER;
    Htuple minContrastTuple = HTUPLE_INITIALIZER;
    Htuple modelIdTuple = HTUPLE_INITIALIZER;
    Htuple minScoreTuple = HTUPLE_INITIALIZER;
    Htuple numMatchesTuple = HTUPLE_INITIALIZER;
    Htuple maxOverlapTuple = HTUPLE_INITIALIZER;
    Htuple subPixelTuple = HTUPLE_INITIALIZER;
    Htuple findNumLevelsTuple = HTUPLE_INITIALIZER;
    Htuple greedinessTuple = HTUPLE_INITIALIZER;
    Htuple rowTuple = HTUPLE_INITIALIZER;
    Htuple columnTuple = HTUPLE_INITIALIZER;
    Htuple angleTuple = HTUPLE_INITIALIZER;
    Htuple scoreTuple = HTUPLE_INITIALIZER;

    QVector<Htuple *> createdTuples;
    bool modelCreated = false;
    bool outputTuplesCreated = false;

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

    auto trackTuple = [&](Htuple &tuple) {
        if (!createdTuples.contains(&tuple))
            createdTuples.append(&tuple);
    };

    auto cleanup = [&]() {
        if (modelCreated && api->clearShapeModel) {
            api->clearShapeModel(modelIdTuple);
            modelCreated = false;
        }

        if (outputTuplesCreated) {
            destroyTuple(scoreTuple);
            destroyTuple(angleTuple);
            destroyTuple(columnTuple);
            destroyTuple(rowTuple);
            outputTuplesCreated = false;
        }

        for (Htuple *tuple : createdTuples) {
            if (tuple)
                destroyTuple(*tuple);
        }
        createdTuples.clear();

        clearObject(detectImage.grayImage);
        clearObject(detectImage.inputImage);
        clearObject(templateImage.grayImage);
        clearObject(templateImage.inputImage);
    };

    auto checkStatus = [&](const Herror status, const QString &stage) {
        if (!halconStatusOk(status)) {
            throw std::pair<QString, QString>(
                    QStringLiteral("PatternPresence HALCON error"),
                    QStringLiteral("%1: %2").arg(stage, halconErrorText(status)));
        }
    };

    auto createIntTuple = [&](Htuple &tuple, const Hlong value) {
        api->createTupleInt(&tuple, value);
        trackTuple(tuple);
    };

    auto createDoubleTuple = [&](Htuple &tuple, const double value) {
        api->createTupleDouble(&tuple, value);
        trackTuple(tuple);
    };

    auto createStringTuple = [&](Htuple &tuple, const QByteArray &value) {
        api->createTupleString(&tuple, value.constData());
        trackTuple(tuple);
    };

    auto validateModelHandle = [&]() {
        if (modelIdTuple.num < 1) {
            throw std::pair<QString, QString>(
                    QStringLiteral("PatternPresence HALCON error"),
                    QStringLiteral("create_shape_model: ModelID was not returned"));
        }
        if (api->getHandle && api->getHandle(&modelIdTuple, 0) == HALCONC_HNULL) {
            throw std::pair<QString, QString>(
                    QStringLiteral("PatternPresence HALCON error"),
                    QStringLiteral("create_shape_model: ModelID is invalid"));
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
        generateGrayImage(templateMat, templateImage, QStringLiteral("template"));
        generateGrayImage(detectMat, detectImage, QStringLiteral("detect"));

        const double angleStartRad = static_cast<double>(config.angleStart) * kPi / 180.0;
        const double angleExtentRad = qBound(0.0,
                                             static_cast<double>(config.angleExtent) * kPi / 180.0,
                                             2.0 * kPi);
        const double minScore = normalizedScore(config.minScore);
        const int numMatches = qMax(1, config.maxCount);
        const QString metric = halconMetricForPolarity(config.polarity);

        createStringTuple(createNumLevelsTuple, QByteArray("auto"));
        createDoubleTuple(angleStartTuple, angleStartRad);
        createDoubleTuple(angleExtentTuple, angleExtentRad);
        createStringTuple(angleStepTuple, QByteArray("auto"));
        createStringTuple(optimizationTuple, QByteArray("auto"));
        createStringTuple(metricTuple, metric.toLatin1());
        createStringTuple(contrastTuple, QByteArray("auto"));
        createStringTuple(minContrastTuple, QByteArray("auto"));
        createDoubleTuple(minScoreTuple, minScore);
        createIntTuple(numMatchesTuple, static_cast<Hlong>(numMatches));
        createDoubleTuple(maxOverlapTuple, 0.5);
        createStringTuple(subPixelTuple, QByteArray("least_squares"));
        createIntTuple(findNumLevelsTuple, 0);
        createDoubleTuple(greedinessTuple, 0.8);

        trackTuple(modelIdTuple);
        checkStatus(api->createShapeModel(templateImage.graySource,
                                          createNumLevelsTuple,
                                          angleStartTuple,
                                          angleExtentTuple,
                                          angleStepTuple,
                                          optimizationTuple,
                                          metricTuple,
                                          contrastTuple,
                                          minContrastTuple,
                                          &modelIdTuple),
                    QStringLiteral("create_shape_model"));
        modelCreated = true;
        validateModelHandle();

        outputTuplesCreated = true;
        checkStatus(api->findShapeModel(detectImage.graySource,
                                        modelIdTuple,
                                        angleStartTuple,
                                        angleExtentTuple,
                                        minScoreTuple,
                                        numMatchesTuple,
                                        maxOverlapTuple,
                                        subPixelTuple,
                                        findNumLevelsTuple,
                                        greedinessTuple,
                                        &rowTuple,
                                        &columnTuple,
                                        &angleTuple,
                                        &scoreTuple),
                    QStringLiteral("find_shape_model"));

        const int matchCount = qMin<int>(qMin<int>(rowTuple.num, columnTuple.num),
                                         qMin<int>(angleTuple.num, scoreTuple.num));
        QVector<double> matchScores;
        QJsonArray matchesArray;
        matchScores.reserve(qMax(0, matchCount));

        double bestScore = 0.0;
        double bestRow = -1.0;
        double bestColumn = -1.0;
        double bestRawRow = -1.0;
        double bestRawColumn = -1.0;
        double bestAngle = 0.0;
        for (int index = 0; index < matchCount; ++index) {
            const double localRow = api->getDouble(&rowTuple, index);
            const double localColumn = api->getDouble(&columnTuple, index);
            const double angle = api->getDouble(&angleTuple, index);
            const double score = api->getDouble(&scoreTuple, index);
            const double imageRow = localRow + static_cast<double>(detectRoiPixels.y());
            const double imageColumn = localColumn + static_cast<double>(detectRoiPixels.x());

            matchScores.append(score);
            QJsonObject matchJson;
            matchJson.insert(QStringLiteral("row"), imageRow);
            matchJson.insert(QStringLiteral("column"), imageColumn);
            matchJson.insert(QStringLiteral("localRow"), localRow);
            matchJson.insert(QStringLiteral("localColumn"), localColumn);
            matchJson.insert(QStringLiteral("angle"), angle);
            matchJson.insert(QStringLiteral("score"), score);
            matchesArray.append(matchJson);

            if (index == 0 || score > bestScore) {
                bestScore = score;
                bestRow = imageRow;
                bestColumn = imageColumn;
                bestRawRow = localRow;
                bestRawColumn = localColumn;
                bestAngle = angle;
            }
        }

        const bool found = matchCount > 0;
        bool ok = false;
        if (isScoreJudgeBasis(config.judgeBasis)) {
            ok = bestScore >= normalizedScore(config.scoreThreshold);
        } else if (isPresenceJudgeBasis(config.judgeBasis)) {
            ok = config.existOk ? found : !found;
        } else {
            ok = config.existOk ? found : !found;
        }

        result.success = true;
        result.ok = ok;
        result.score = bestScore;
        result.count = matchCount;
        result.text = found ? QStringLiteral("found") : QStringLiteral("missing");
        result.status = QStringLiteral("%1 score=%2")
                .arg(result.text, QString::number(bestScore, 'f', 3));
        result.message = QStringLiteral("PatternPresence: %1 score=%2 count=%3")
                .arg(result.text,
                     QString::number(bestScore, 'f', 3),
                     QString::number(matchCount));

        QRectF matchRect;
        if (found) {
            matchRect = QRectF(bestColumn - static_cast<double>(templateRoiPixels.width()) / 2.0,
                               bestRow - static_cast<double>(templateRoiPixels.height()) / 2.0,
                               templateRoiPixels.width(),
                               templateRoiPixels.height());
        }

        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("foundCount"), matchCount);
        result.payload.insert(QStringLiteral("bestScore"), bestScore);
        result.payload.insert(QStringLiteral("bestRow"), bestRow);
        result.payload.insert(QStringLiteral("bestColumn"), bestColumn);
        result.payload.insert(QStringLiteral("bestAngle"), bestAngle);
        result.payload.insert(QStringLiteral("bestAngleDeg"), bestAngle * 180.0 / kPi);
        result.payload.insert(QStringLiteral("rawMatchRow"), bestRawRow);
        result.payload.insert(QStringLiteral("rawMatchCol"), bestRawColumn);
        result.payload.insert(QStringLiteral("globalMatchRow"), bestRow);
        result.payload.insert(QStringLiteral("globalMatchCol"), bestColumn);
        result.payload.insert(QStringLiteral("matchRectX"), matchRect.x());
        result.payload.insert(QStringLiteral("matchRectY"), matchRect.y());
        result.payload.insert(QStringLiteral("matchRectW"), matchRect.width());
        result.payload.insert(QStringLiteral("matchRectH"), matchRect.height());
        result.payload.insert(QStringLiteral("matchScores"), scoresToJson(matchScores));
        result.payload.insert(QStringLiteral("matches"), matchesArray);
        result.payload.insert(QStringLiteral("usedMinScore"), minScore);
        result.payload.insert(QStringLiteral("usedScoreThreshold"), normalizedScore(config.scoreThreshold));
        result.payload.insert(QStringLiteral("usedMetric"), metric);
        result.payload.insert(QStringLiteral("usedAngleStartRad"), angleStartRad);
        result.payload.insert(QStringLiteral("usedAngleExtentRad"), angleExtentRad);
        result.payload.insert(QStringLiteral("usedNumMatches"), numMatches);
        result.payload.insert(QStringLiteral("text"), result.text);

        if (found) {
            const double crossRadius = qBound(6.0,
                                              static_cast<double>(qMin(templateMat.cols, templateMat.rows)) / 4.0,
                                              24.0);
            const QPointF center(bestColumn, bestRow);
            result.overlays.append(rectOverlay(matchRect,
                                               QStringLiteral("match_rect"),
                                               bestScore));
            result.overlays.append(lineOverlay(QPointF(center.x() - crossRadius, center.y()),
                                               QPointF(center.x() + crossRadius, center.y()),
                                               QStringLiteral("match_center"),
                                               bestScore));
            result.overlays.append(lineOverlay(QPointF(center.x(), center.y() - crossRadius),
                                               QPointF(center.x(), center.y() + crossRadius),
                                               QStringLiteral("match_center"),
                                               bestScore));
            result.overlays.append(textOverlay(QPointF(center.x() + crossRadius + 2.0,
                                                       center.y() + crossRadius + 2.0),
                                               QString::number(bestScore, 'f', 3),
                                               QStringLiteral("match_score"),
                                               bestScore));
        }

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
        result.status = QStringLiteral("PatternPresence HALCON error");
        result.message = QString::fromLocal8Bit(error.what());
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
}
