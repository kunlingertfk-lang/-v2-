#include "algorithms/recognition/RegisteredClassificationHalconRunner.h"

#include <HalconC.h>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRect>
#include <QRegularExpression>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <exception>
#include <opencv2/imgproc.hpp>

namespace {

constexpr int kMinRoiPixelSize = 2;

// 模型文件名只允许英文大小写字母、数字、下划线（参考功能文档要求）。
const QRegularExpression &validModelNameRegex()
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_]+$"));
    return re;
}

bool halconStatusOk(const Herror status)
{
    return status == H_MSG_OK || status == H_MSG_TRUE || status == H_MSG_FALSE;
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

bool isRectangleRegionType(const QString &type)
{
    return type.trimmed().toLower() == QStringLiteral("rectangle");
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
    return QRect(x1, y1, x2 - x1, y2 - y1);
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

QJsonObject rectToJson(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

// 错误运行结果合成。位置修正第一版固定占位：applied=false, reason=not implemented。
RegisteredClassificationHalconResult runError(const QString &status,
                                              const QString &message,
                                              const RegisteredClassificationHalconConfig &config,
                                              const cv::Mat &image,
                                              const qint64 elapsedMs)
{
    RegisteredClassificationHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.payload.insert(QStringLiteral("error"), message);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_dl_classification"));
    result.payload.insert(QStringLiteral("modelPath"), config.modelPath);
    result.payload.insert(QStringLiteral("modelName"), config.modelName);
    result.payload.insert(QStringLiteral("modelType"), config.modelType);
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("topK"), config.topK);
    result.payload.insert(QStringLiteral("judgeMode"), config.judgeMode);
    result.payload.insert(QStringLiteral("expectedLabel"), config.expectedLabel);
    result.payload.insert(QStringLiteral("minScore"), config.minScore);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"),
                          config.halconSoPathCandidates.join(QStringLiteral("; ")));
    result.payload.insert(QStringLiteral("errorCode"), status);
    result.payload.insert(QStringLiteral("errorMessage"), message);
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    return result;
}

// 声明 HALCON C API 接口。仅含注册分类 DL 推理所需符号。
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
    using GetStringFn = char *(*)(const Htuple *, Hlong); // 可选，读取类别名
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using GenRectangle1Fn = Herror (*)(Hobject *, double, double, double, double);
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using ClearObjFn = Herror (*)(const Hobject);

    // DL 分类算子（T_ 前缀变体，按 Htuple 句柄操作）。
    using ReadDlModelFn = Herror (*)(const Htuple, Htuple *);
    using ApplyDlModelFn = Herror (*)(const Htuple, const Htuple, const Htuple, Htuple *);
    using ClearDlModelFn = Herror (*)(const Htuple);
    using SetDlModelParamFn = Herror (*)(const Htuple, const Htuple, const Htuple);
    using GetDlModelParamFn = Herror (*)(const Htuple, const Htuple, Htuple *);
    using CreateDictFn = Herror (*)(Htuple *);
    using SetDictObjectFn = Herror (*)(const Hobject, const Htuple, const Htuple);
    using SetDictTupleFn = Herror (*)(const Htuple, const Htuple, const Htuple);
    using GetDictTupleFn = Herror (*)(const Htuple, const Htuple, Htuple *);
    using GetDictObjectFn = Herror (*)(const Htuple, const Htuple, Hobject *);
    using ClearHandleFn = Herror (*)(const Htuple);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetIntFn setInt = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GetIntFn getInt = nullptr;
    GetStringFn getString = nullptr; // 可选
    GenImageInterleavedFn genImageInterleaved = nullptr;
    GenRectangle1Fn genRectangle1 = nullptr;
    ReduceDomainFn reduceDomain = nullptr;
    ClearObjFn clearObj = nullptr;
    ReadDlModelFn readDlModel = nullptr;
    ApplyDlModelFn applyDlModel = nullptr;
    ClearDlModelFn clearDlModel = nullptr;
    SetDlModelParamFn setDlModelParam = nullptr;
    GetDlModelParamFn getDlModelParam = nullptr;
    CreateDictFn createDict = nullptr;
    SetDictObjectFn setDictObject = nullptr;
    SetDictTupleFn setDictTuple = nullptr;
    GetDictTupleFn getDictTuple = nullptr;
    GetDictObjectFn getDictObject = nullptr;
    ClearHandleFn clearHandle = nullptr;
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
            !resolveRequired(m_handle, api.genRectangle1, "gen_rectangle1", errorMessage) ||
            !resolveRequired(m_handle, api.reduceDomain, "reduce_domain", errorMessage) ||
            !resolveRequired(m_handle, api.clearObj, "clear_obj", errorMessage) ||
            !resolveRequired(m_handle, api.readDlModel, "T_read_dl_model", errorMessage) ||
            !resolveRequired(m_handle, api.applyDlModel, "T_apply_dl_model", errorMessage) ||
            !resolveRequired(m_handle, api.clearDlModel, "T_clear_dl_model", errorMessage) ||
            !resolveRequired(m_handle, api.getDlModelParam, "T_get_dl_model_param", errorMessage) ||
            !resolveRequired(m_handle, api.createDict, "T_create_dict", errorMessage) ||
            !resolveRequired(m_handle, api.setDictObject, "T_set_dict_object", errorMessage) ||
            !resolveRequired(m_handle, api.setDictTuple, "T_set_dict_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getDictTuple, "T_get_dict_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.clearHandle, "T_clear_handle", errorMessage)) {
            symbolMissing = true;
            dlclose(m_handle);
            m_handle = nullptr;
            return false;
        }
        // 字符串元组读取与 dict object 读取为可选符号，缺失时优雅降级。
        resolveOptional(m_handle, api.getString, "F_get_s");
        resolveOptional(m_handle, api.getDictObject, "T_get_dict_object");
        resolveOptional(m_handle, api.setDlModelParam, "T_set_dl_model_param");

        if (api.setUtf8)
            api.setUtf8(1);
        return true;
    }

    HalconCApi api;

private:
    void *m_handle = nullptr;
};

// RAII Htuple 包装，复用 ColorRecognitionHalconRunner 的实现形态。
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

    Htuple *ptr() { return &m_tuple; }
    const Htuple &value() const { return m_tuple; }
    int size() const { return static_cast<int>(m_tuple.num); }

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

    QString stringAt(const int index) const
    {
        if (!m_api || !m_api->getString)
            return QString();
        const char *text = m_api->getString(&m_tuple, index);
        return text ? QString::fromUtf8(text) : QString();
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

// 构造一个字符串 Htuple（用于 dict key、模型文件名、算子参数名）。
HalconTuple stringTuple(HalconCApi *api, const QString &text)
{
    HalconTuple tuple(api);
    tuple.create(1);
    const QByteArray utf8 = text.toUtf8();
    tuple.setString(0, utf8.constData());
    return tuple;
}

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
    if (api && api->clearObj && object != 0 && object != NO_OBJECTS && object != EMPTY_REGION) {
        api->clearObj(object);
        object = NO_OBJECTS;
    }
}

// RAII dict 句柄，析构调 T_clear_handle。create_dict 输出参数要 Htuple*，
// 其余 dict 算子按值取 const Htuple。
class DictHandle
{
public:
    explicit DictHandle(HalconCApi *api)
        : m_api(api)
    {
    }
    ~DictHandle()
    {
        if (m_api && m_api->clearHandle && (m_handle.num > 0 || m_handle.capacity > 0))
            m_api->clearHandle(m_handle);
    }
    Htuple *ptr() { return &m_handle; }
    const Htuple &value() const { return m_handle; }
    bool valid() const { return m_handle.num > 0 || m_handle.capacity > 0; }
private:
    HalconCApi *m_api;
    Htuple m_handle = HTUPLE_INITIALIZER;
};

// RAII DL 模型句柄，析构调 T_clear_dl_model（按值）。
class DlModelHandle
{
public:
    explicit DlModelHandle(HalconCApi *api)
        : m_api(api)
    {
    }
    ~DlModelHandle()
    {
        if (m_api && m_api->clearDlModel && (m_handle.num > 0 || m_handle.capacity > 0))
            m_api->clearDlModel(m_handle);
    }
    Htuple *ptr() { return &m_handle; }
    const Htuple &value() const { return m_handle; }
private:
    HalconCApi *m_api;
    Htuple m_handle = HTUPLE_INITIALIZER;
};

// 从 DL 结果字典读取 classification_classes 与 classification_confidences，
// 结合模型 classes 参数解析出 topK 类别与得分。分数统一映射到 0-100。
QVector<RegisteredClassificationClassScore> extractTopClasses(
        HalconCApi *api,
        const DlModelHandle &model,
        const DictHandle &resultDict,
        const int topK)
{
    HalconTuple classesTuple(api);
    checkStatus(api,
                api->getDictTuple(resultDict.value(),
                                  stringTuple(api, QStringLiteral("classification_classes")).value(),
                                  classesTuple.ptr()),
                QStringLiteral("get_dict_tuple.classification_classes"));

    HalconTuple confidencesTuple(api);
    const Herror confStatus = api->getDictTuple(
                resultDict.value(),
                stringTuple(api, QStringLiteral("classification_confidences")).value(),
                confidencesTuple.ptr());
    const bool hasConfidences = halconStatusOk(confStatus);

    // 读取模型类别名列表（可选，缺失时用类别 id 兜底）。
    QVector<QString> classNames;
    HalconTuple classNamesTuple(api);
    const Herror namesStatus = api->getDlModelParam(
                model.value(),
                stringTuple(api, QStringLiteral("classes")).value(),
                classNamesTuple.ptr());
    if (halconStatusOk(namesStatus) && api->getString) {
        const int nameCount = classNamesTuple.size();
        classNames.reserve(nameCount);
        for (int i = 0; i < nameCount; ++i)
            classNames.append(classNamesTuple.stringAt(i));
    }

    const int count = classesTuple.size();
    QVector<RegisteredClassificationClassScore> scores;
    scores.reserve(count);
    for (int i = 0; i < count; ++i) {
        RegisteredClassificationClassScore item;
        item.classId = static_cast<int>(classesTuple.intAt(i));
        item.score = hasConfidences ? std::clamp(confidencesTuple.doubleAt(i) * 100.0, 0.0, 100.0)
                                    : 0.0;
        item.label = (item.classId >= 0 && item.classId < classNames.size())
                             ? classNames.at(item.classId)
                             : (api->getString ? classesTuple.stringAt(i)
                                               : QString::number(item.classId));
        scores.append(item);
    }

    std::sort(scores.begin(), scores.end(),
              [](const RegisteredClassificationClassScore &a,
                 const RegisteredClassificationClassScore &b) {
                  return a.score > b.score;
              });

    if (topK > 0 && scores.size() > topK)
        scores = scores.mid(0, topK);
    return scores;
}

} // namespace

RegisteredClassificationHalconResult RegisteredClassificationHalconRunner::run(
        const cv::Mat &image,
        const RegisteredClassificationHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    try {
        // 1. 输入校验：空图像。
        if (image.empty()) {
            return runError(QStringLiteral("image_empty"),
                            QStringLiteral("Input image is empty."),
                            config, image, timer.elapsed());
        }

        // 2. 输入校验：模型路径与模型名。
        const QString modelPath = config.modelPath.trimmed();
        if (modelPath.isEmpty()) {
            return runError(QStringLiteral("no_model"),
                            QStringLiteral("No classification model is configured."),
                            config, image, timer.elapsed());
        }

        const QString modelName = config.modelName.trimmed();
        if (!modelName.isEmpty() && !validModelNameRegex().match(modelName).hasMatch()) {
            return runError(QStringLiteral("invalid_model_name"),
                            QStringLiteral("Model name must only contain letters, digits and underscores: %1")
                                .arg(modelName),
                            config, image, timer.elapsed());
        }

        // 3. 输入校验：海康 .scbin 第一版明确不支持。
        const QString lowerSuffix = QFileInfo(modelPath).suffix().toLower();
        if (lowerSuffix == QStringLiteral("scbin")) {
            return runError(QStringLiteral("unsupported_model_format"),
                            QStringLiteral("Hikvision .scbin model format is not supported in the first version."),
                            config, image, timer.elapsed());
        }

        // 4. 输入校验：模型文件存在性。
        if (!QFileInfo::exists(modelPath)) {
            return runError(QStringLiteral("model_file_not_found"),
                            QStringLiteral("Classification model file not found: %1").arg(modelPath),
                            config, image, timer.elapsed());
        }

        // 5. 输入校验：矩形检测 ROI。
        const cv::Mat bgr = toBgr8(image);
        if (bgr.empty()) {
            return runError(QStringLiteral("image_empty"),
                            QStringLiteral("Input image is empty or unsupported."),
                            config, image, timer.elapsed());
        }
        QRect roiPixels;
        if (isRectangleRegionType(config.detectRegionType)) {
            roiPixels = normalizedRoiToPixels(config.roiNormalized, bgr.cols, bgr.rows);
            if (roiPixels.isEmpty()
                || roiPixels.width() < kMinRoiPixelSize
                || roiPixels.height() < kMinRoiPixelSize) {
                return runError(QStringLiteral("invalid_roi"),
                                QStringLiteral("Detection rectangle ROI is invalid."),
                                config, image, timer.elapsed());
            }
        }

        // 6. 输入校验：类别判断模式必须有 expectedLabel。
        const QString judgeMode = config.judgeMode.trimmed().toLower();
        if (judgeMode == QStringLiteral("class_match")
            && config.expectedLabel.trimmed().isEmpty()) {
            return runError(QStringLiteral("missing_expected_label"),
                            QStringLiteral("Expected label is empty for class_match judgement."),
                            config, image, timer.elapsed());
        }

        // 7. HALCON runtime 加载。
        if (config.halconSoPath.trimmed().isEmpty() || !QFileInfo::exists(config.halconSoPath)) {
            const QString triedPaths = config.halconSoPathCandidates.isEmpty()
                    ? config.halconSoPath
                    : config.halconSoPathCandidates.join(QStringLiteral("; "));
            return runError(QStringLiteral("halcon_so_not_found"),
                            QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                                .arg(config.halconSoPath, triedPaths),
                            config, image, timer.elapsed());
        }

        HalconLibrary library;
        QString loadMessage;
        bool symbolMissing = false;
        if (!library.load(config.halconSoPath, loadMessage, symbolMissing)) {
            return runError(symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                          : QStringLiteral("halcon_load_failed"),
                            loadMessage,
                            config, image, timer.elapsed());
        }

        HalconCApi *api = &library.api;

        // 8. 读取 HALCON DL 分类模型。
        DlModelHandle model(api);
        checkStatus(api,
                    api->readDlModel(stringTuple(api, modelPath).value(), model.ptr()),
                    QStringLiteral("read_dl_model"));

        // 9. cv::Mat -> HALCON image 桥接。
        Hobject halconImage = NO_OBJECTS;
        Hobject roiRegion = NO_OBJECTS;
        Hobject reducedImage = NO_OBJECTS;
        const Hobject *imageForInference = &halconImage;
        auto cleanupObjects = [&]() {
            clearObject(api, reducedImage);
            clearObject(api, roiRegion);
            clearObject(api, halconImage);
        };

        checkStatus(api,
                    api->genImageInterleaved(&halconImage,
                                              reinterpret_cast<Hlong>(bgr.data),
                                              "bgr",
                                              bgr.cols,
                                              bgr.rows,
                                              0,
                                              "byte",
                                              0, 0, 0, 0, 8, 0),
                    QStringLiteral("gen_image_interleaved"));

        if (isRectangleRegionType(config.detectRegionType)) {
            checkStatus(api,
                        api->genRectangle1(&roiRegion,
                                           roiPixels.y(),
                                           roiPixels.x(),
                                           roiPixels.y() + roiPixels.height() - 1,
                                           roiPixels.x() + roiPixels.width() - 1),
                        QStringLiteral("gen_rectangle1.detect_roi"));
            checkStatus(api,
                        api->reduceDomain(halconImage, roiRegion, &reducedImage),
                        QStringLiteral("reduce_domain"));
            imageForInference = &reducedImage;
        }

        // 10. 构造 DLSample 字典，写入图像。
        DictHandle sampleDict(api);
        checkStatus(api, api->createDict(sampleDict.ptr()), QStringLiteral("create_dict"));
        checkStatus(api,
                    api->setDictObject(*imageForInference,
                                       sampleDict.value(),
                                       stringTuple(api, QStringLiteral("image")).value()),
                    QStringLiteral("set_dict_object.image"));

        // 11. 执行 DL 分类推理。Outputs 传空元组以获取默认全部输出。
        HalconTuple emptyOutputs(api);
        emptyOutputs.create(0);
        DictHandle resultDict(api);
        checkStatus(api,
                    api->applyDlModel(model.value(),
                                      sampleDict.value(),
                                      emptyOutputs.value(),
                                      resultDict.ptr()),
                    QStringLiteral("apply_dl_model"));

        // 12. 解析 topK 结果。
        const QVector<RegisteredClassificationClassScore> topClasses =
                extractTopClasses(api, model, resultDict, qMax(1, config.topK));
        if (topClasses.isEmpty()) {
            cleanupObjects();
            return runError(QStringLiteral("inference_result_missing"),
                            QStringLiteral("DL classification result is empty or missing classification_classes."),
                            config, image, timer.elapsed());
        }

        const RegisteredClassificationClassScore top = topClasses.first();

        // 13. 判别。
        bool ok = false;
        QString resolvedJudgeMode = judgeMode;
        if (resolvedJudgeMode == QStringLiteral("class_match")) {
            ok = top.label == config.expectedLabel.trimmed();
        } else {
            resolvedJudgeMode = QStringLiteral("min_score");
            ok = top.score >= static_cast<double>(qBound(0, config.minScore, 100));
        }

        // 14. 组装成功结果与 payload。
        RegisteredClassificationHalconResult result;
        result.success = true;
        result.ok = ok;
        result.status = ok ? QStringLiteral("ok") : QStringLiteral("ng");
        result.predictedLabel = top.label;
        result.predictedClassId = top.classId;
        result.score = top.score;
        result.topClasses = topClasses;
        result.elapsedMs = timer.elapsed();

        QJsonArray topClassesJson;
        for (const RegisteredClassificationClassScore &item : topClasses) {
            QJsonObject itemJson;
            itemJson.insert(QStringLiteral("label"), item.label);
            itemJson.insert(QStringLiteral("classId"), item.classId);
            itemJson.insert(QStringLiteral("score"), item.score);
            topClassesJson.append(itemJson);
        }

        const QRect effectiveRoi = isRectangleRegionType(config.detectRegionType)
                ? roiPixels
                : QRect(0, 0, bgr.cols, bgr.rows);

        result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_dl_classification"));
        result.payload.insert(QStringLiteral("modelPath"), config.modelPath);
        result.payload.insert(QStringLiteral("modelName"), config.modelName);
        result.payload.insert(QStringLiteral("modelType"), config.modelType);
        result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
        result.payload.insert(QStringLiteral("roiPixelsRect"), rectToJson(QRectF(effectiveRoi)));
        result.payload.insert(QStringLiteral("predictedLabel"), result.predictedLabel);
        result.payload.insert(QStringLiteral("predictedClassId"), result.predictedClassId);
        result.payload.insert(QStringLiteral("score"), result.score);
        result.payload.insert(QStringLiteral("topClasses"), topClassesJson);
        result.payload.insert(QStringLiteral("topK"), config.topK);
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("judgeMode"), resolvedJudgeMode);
        result.payload.insert(QStringLiteral("expectedLabel"), config.expectedLabel);
        result.payload.insert(QStringLiteral("minScore"), config.minScore);
        result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
        result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
        result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
        result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));

        // overlay：检测 ROI + OK/NG + 预测类别与得分。
        ToolOverlay roiOverlay;
        roiOverlay.type = ToolOverlayType::Rect;
        roiOverlay.label = QStringLiteral("detect_roi");
        roiOverlay.rect = QRectF(effectiveRoi);
        result.overlays.append(roiOverlay);

        ToolOverlay resultOverlay;
        resultOverlay.type = ToolOverlayType::Text;
        resultOverlay.rect = QRectF(effectiveRoi);
        resultOverlay.label = QStringLiteral("result");
        resultOverlay.text = QStringLiteral("%1: %2 (%3%)")
                .arg(ok ? QStringLiteral("OK") : QStringLiteral("NG"),
                     result.predictedLabel,
                     QString::number(result.score, 'f', 1));
        resultOverlay.score = result.score;
        result.overlays.append(resultOverlay);

        cleanupObjects();
        return result;
    } catch (const std::pair<QString, QString> &error) {
        return runError(error.first, error.second, config, image, timer.elapsed());
    } catch (const std::exception &error) {
        return runError(QStringLiteral("exception"),
                        QString::fromLocal8Bit(error.what()),
                        config, image, timer.elapsed());
    } catch (...) {
        return runError(QStringLiteral("exception"),
                        QStringLiteral("Unknown exception while running registered classification."),
                        config, image, timer.elapsed());
    }
}
