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

ToolOverlay rectOverlay(const QRectF &rect, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.rect = rect;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

ToolOverlay textOverlay(const QPointF &position, const QString &text, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.p1 = position;
    overlay.text = text;
    overlay.label = text;
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
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_histogram_knn_color_recognition"));
    result.payload.insert(QStringLiteral("featureType"), config.featureType);
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"),
                          config.halconSoPathCandidates.join(QStringLiteral("; ")));
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
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using GrayHistoRangeFn = Herror (*)(const Hobject, const Hobject, double, double, Hlong,
                                        Hlong *, double *);
    using CreateClassKnnFn = Herror (*)(const Htuple, Htuple *);
    using AddSampleClassKnnFn = Herror (*)(const Htuple, const Htuple, const Htuple);
    using TrainClassKnnFn = Herror (*)(const Htuple, const Htuple, const Htuple);
    using SetParamsClassKnnFn = Herror (*)(const Htuple, const Htuple, const Htuple);
    using ClassifyClassKnnFn = Herror (*)(const Htuple, const Htuple, Htuple *, Htuple *);
    using ClearClassKnnFn = Herror (*)(const Htuple);
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
    ReduceDomainFn reduceDomain = nullptr;
    GrayHistoRangeFn grayHistoRange = nullptr;
    CreateClassKnnFn createClassKnn = nullptr;
    AddSampleClassKnnFn addSampleClassKnn = nullptr;
    TrainClassKnnFn trainClassKnn = nullptr;
    SetParamsClassKnnFn setParamsClassKnn = nullptr;
    ClassifyClassKnnFn classifyClassKnn = nullptr;
    ClearClassKnnFn clearClassKnn = nullptr;
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
            !resolveRequired(m_handle, api.reduceDomain, "reduce_domain", errorMessage) ||
            !resolveRequired(m_handle, api.grayHistoRange, "gray_histo_range", errorMessage) ||
            !resolveRequired(m_handle, api.createClassKnn, "T_create_class_knn", errorMessage) ||
            !resolveRequired(m_handle, api.addSampleClassKnn, "T_add_sample_class_knn", errorMessage) ||
            !resolveRequired(m_handle, api.trainClassKnn, "T_train_class_knn", errorMessage) ||
            !resolveRequired(m_handle, api.setParamsClassKnn, "T_set_params_class_knn", errorMessage) ||
            !resolveRequired(m_handle, api.classifyClassKnn, "T_classify_class_knn", errorMessage) ||
            !resolveRequired(m_handle, api.clearClassKnn, "T_clear_class_knn", errorMessage) ||
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
    QVector<Hlong> histo(bins);
    double binSize = 0.0;
    checkStatus(api, api->reduceDomain(channel, roiRegion, &reducedChannel),
                stage + QStringLiteral(".reduce_domain"));
    try {
        checkStatus(api, api->grayHistoRange(roiRegion,
                                             reducedChannel,
                                             0.0,
                                             255.0,
                                             bins,
                                             histo.data(),
                                             &binSize),
                    stage + QStringLiteral(".gray_histo_range"));
    } catch (...) {
        clearObject(api, reducedChannel);
        throw;
    }
    clearObject(api, reducedChannel);

    double sum = 0.0;
    for (const Hlong value : histo)
        sum += static_cast<double>(value);
    if (sum <= 0.0)
        sum = 1.0;

    QVector<double> normalized;
    normalized.reserve(bins);
    for (const Hlong value : histo)
        normalized.append(static_cast<double>(value) / sum);
    return normalized;
}

QVector<double> extractHistogramFeature(const cv::Mat &bgr,
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

    auto cleanup = [&]() {
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
        checkStatus(api, api->genRectangle1(&roiRegion,
                                            roiPixels.y(),
                                            roiPixels.x(),
                                            roiPixels.y() + roiPixels.height() - 1,
                                            roiPixels.x() + roiPixels.width() - 1),
                    QStringLiteral("gen_rectangle1"));

        const int bins = histogramBinsForSensitivity(config.sensitivity);
        QVector<double> feature;
        feature.reserve(config.brightnessEnabled ? bins * 3 : bins * 2);
        feature += histogramForChannel(api, roiRegion, hue, bins, QStringLiteral("hue"));
        feature += histogramForChannel(api, roiRegion, saturation, bins, QStringLiteral("saturation"));
        if (config.brightnessEnabled)
            feature += histogramForChannel(api, roiRegion, value, bins, QStringLiteral("value"));

        cleanup();
        return feature;
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

    try {
        ColorRecognitionHalconFeatureResult result;
        result.feature = extractHistogramFeature(bgr, roiPixels, config, &library.api);
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

    if (config.samples.isEmpty()) {
        return runError(QStringLiteral("no_model_samples"),
                        QStringLiteral("Color model has no trained samples."),
                        config,
                        image,
                        timer.elapsed());
    }

    const ColorRecognitionHalconFeatureResult featureResult = extractFeature(image, config);
    if (!featureResult.success) {
        ColorRecognitionHalconResult error = runError(featureResult.status,
                                                      featureResult.message,
                                                      config,
                                                      image,
                                                      timer.elapsed());
        error.payload.insert(QStringLiteral("featurePayload"), featureResult.payload);
        return error;
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
    HalconTuple numDim(api);
    HalconTuple knnHandle(api);
    HalconTuple trainNames(api);
    HalconTuple trainValues(api);
    HalconTuple paramNames(api);
    HalconTuple paramValues(api);
    HalconTuple queryFeatures(api);
    HalconTuple resultTuple(api);
    HalconTuple ratingTuple(api);

    auto makeFeatureTuple = [&](const QVector<double> &feature) {
        HalconTuple tuple(api);
        tuple.create(feature.size());
        for (int i = 0; i < feature.size(); ++i)
            tuple.setDouble(i, feature.at(i));
        return tuple;
    };

    try {
        numDim.create(1);
        numDim.setInt(0, featureResult.feature.size());
        checkStatus(api, api->createClassKnn(numDim.value(), knnHandle.ptr()),
                    QStringLiteral("create_class_knn"));

        for (const ColorRecognitionHalconSample &sample : usableSamples) {
            HalconTuple featureTuple = makeFeatureTuple(sample.feature);
            HalconTuple classTuple(api);
            classTuple.create(1);
            classTuple.setInt(0, sample.classId);
            checkStatus(api, api->addSampleClassKnn(knnHandle.value(),
                                                    featureTuple.value(),
                                                    classTuple.value()),
                        QStringLiteral("add_sample_class_knn"));
        }

        trainNames.create(1);
        trainValues.create(1);
        trainNames.setString(0, "normalization");
        trainValues.setString(0, "true");
        checkStatus(api, api->trainClassKnn(knnHandle.value(), trainNames.value(), trainValues.value()),
                    QStringLiteral("train_class_knn"));

        paramNames.create(4);
        paramValues.create(4);
        paramNames.setString(0, "method");
        paramValues.setString(0, "classes_frequency");
        paramNames.setString(1, "k");
        paramValues.setInt(1, qBound(1, config.knnK, qMax(1, usableSamples.size())));
        paramNames.setString(2, "max_num_classes");
        paramValues.setInt(2, 1);
        paramNames.setString(3, "num_checks");
        paramValues.setInt(3, 0);
        checkStatus(api, api->setParamsClassKnn(knnHandle.value(), paramNames.value(), paramValues.value()),
                    QStringLiteral("set_params_class_knn"));

        queryFeatures = makeFeatureTuple(featureResult.feature);
        checkStatus(api, api->classifyClassKnn(knnHandle.value(),
                                               queryFeatures.value(),
                                               resultTuple.ptr(),
                                               ratingTuple.ptr()),
                    QStringLiteral("classify_class_knn"));

        if (resultTuple.size() <= 0 || ratingTuple.size() <= 0) {
            throw std::pair<QString, QString>(
                    QStringLiteral("empty_classification"),
                    QStringLiteral("HALCON KNN returned no classification result."));
        }

        const int predictedClassId = static_cast<int>(resultTuple.intAt(0));
        const double rating = ratingTuple.doubleAt(0);
        const double score = qBound(0.0, rating * 100.0, 100.0);
        QString predictedLabel;
        for (const ColorRecognitionHalconLabel &label : config.labels) {
            if (label.classId == predictedClassId) {
                predictedLabel = label.name;
                break;
            }
        }
        if (predictedLabel.isEmpty()) {
            for (const ColorRecognitionHalconSample &sample : usableSamples) {
                if (sample.classId == predictedClassId) {
                    predictedLabel = sample.label;
                    break;
                }
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
        result.overlays.append(rectOverlay(QRectF(roiPixels), QStringLiteral("ROI"), score));
        result.overlays.append(textOverlay(QPointF(roiPixels.x() + 4.0, qMax(18, roiPixels.y() - 8)),
                                           QStringLiteral("%1 %2 %3")
                                           .arg(result.ok ? QStringLiteral("OK") : QStringLiteral("NG"),
                                                predictedLabel,
                                                QString::number(score, 'f', 1)),
                                           score));

        result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_histogram_knn_color_recognition"));
        result.payload.insert(QStringLiteral("featureType"), QStringLiteral("histogram"));
        result.payload.insert(QStringLiteral("predictedLabel"), predictedLabel);
        result.payload.insert(QStringLiteral("predictedClassId"), predictedClassId);
        result.payload.insert(QStringLiteral("score"), score);
        result.payload.insert(QStringLiteral("rating"), rating);
        result.payload.insert(QStringLiteral("scoreDirection"), QStringLiteral("higher_is_better"));
        result.payload.insert(QStringLiteral("scoreFormula"), QStringLiteral("classes_frequency_rating_x100"));
        result.payload.insert(QStringLiteral("ratingMode"), QStringLiteral("classes_frequency_relative_frequency"));
        result.payload.insert(QStringLiteral("sampleCount"), result.sampleCount);
        result.payload.insert(QStringLiteral("featureLength"), featureResult.feature.size());
        result.payload.insert(QStringLiteral("queryFeature"), vectorToJson(featureResult.feature));
        result.payload.insert(QStringLiteral("roiPixelsRect"), rectToJson(QRectF(roiPixels)));
        result.payload.insert(QStringLiteral("judgeMode"), judgeMode);
        result.payload.insert(QStringLiteral("minScore"), qBound(0, config.minScore, 100));
        result.payload.insert(QStringLiteral("expectedLabel"), config.expectedLabel);
        result.payload.insert(QStringLiteral("knnK"), qBound(1, config.knnK, qMax(1, usableSamples.size())));
        result.payload.insert(QStringLiteral("knnDistanceApplied"), QStringLiteral("halcon_l2_norm"));
        result.payload.insert(QStringLiteral("knnMethod"), QStringLiteral("classes_frequency"));
        result.payload.insert(QStringLiteral("knnNumChecks"), 0);
        result.payload.insert(QStringLiteral("histogramBins"), histogramBinsForSensitivity(config.sensitivity));
        result.payload.insert(QStringLiteral("brightnessEnabled"), config.brightnessEnabled);
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));

        if (api->clearClassKnn)
            api->clearClassKnn(knnHandle.value());
        return result;
    } catch (const std::pair<QString, QString> &error) {
        if (api->clearClassKnn && knnHandle.size() > 0)
            api->clearClassKnn(knnHandle.value());
        return runError(error.first, error.second, config, image, timer.elapsed());
    } catch (const std::exception &error) {
        if (api->clearClassKnn && knnHandle.size() > 0)
            api->clearClassKnn(knnHandle.value());
        return runError(QStringLiteral("exception"),
                        QString::fromLocal8Bit(error.what()),
                        config,
                        image,
                        timer.elapsed());
    }
}
