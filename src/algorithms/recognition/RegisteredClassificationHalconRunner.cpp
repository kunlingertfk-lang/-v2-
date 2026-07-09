#include "algorithms/recognition/RegisteredClassificationHalconRunner.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <HalconC.h>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRect>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <exception>

namespace {

constexpr int kMinRoiPixelSize = 2;

struct HalconInferenceApi
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
    using ReadClassMlpFn = Herror (*)(const Htuple, Htuple *);
    using ClassifyClassMlpFn = Herror (*)(const Htuple, const Htuple, const Htuple, Htuple *, Htuple *);
    using ClearClassMlpFn = Herror (*)(const Htuple);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetIntFn setInt = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GetIntFn getInt = nullptr;
    ReadClassMlpFn readClassMlp = nullptr;
    ClassifyClassMlpFn classifyClassMlp = nullptr;
    ClearClassMlpFn clearClassMlp = nullptr;
};

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

bool isRectangleRegionType(const QString &type)
{
    return type.trimmed().toLower() == QStringLiteral("rectangle");
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

QJsonArray classLabelsToJson(const QVector<RegisteredClassificationClassLabel> &labels)
{
    QJsonArray array;
    for (const RegisteredClassificationClassLabel &label : labels) {
        QJsonObject item;
        item.insert(QStringLiteral("id"), label.id);
        item.insert(QStringLiteral("name"), label.name);
        array.append(item);
    }
    return array;
}

static RegisteredClassificationHalconResult makeError(
        const QString &status,
        const QString &message,
        const RegisteredClassificationHalconConfig &config,
        const cv::Mat &image,
        qint64 elapsedMs)
{
    RegisteredClassificationHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.rejectScore = qBound(0, config.rejectScore, 100);
    result.top2Gap = qMax(0, config.top2Gap);
    result.payload.insert(QStringLiteral("algorithm"), registeredClassificationMlpModelType());
    result.payload.insert(QStringLiteral("modelPath"), config.modelPath);
    result.payload.insert(QStringLiteral("modelName"), config.modelName);
    result.payload.insert(QStringLiteral("modelType"), config.modelType);
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("judgeMode"), config.judgeMode);
    result.payload.insert(QStringLiteral("expectedLabel"), config.expectedLabel);
    result.payload.insert(QStringLiteral("minScore"), config.minScore);
    result.payload.insert(QStringLiteral("rejectScore"), result.rejectScore);
    result.payload.insert(QStringLiteral("top2Gap"), result.top2Gap);
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    PositionCorrection::writeNotAppliedPayload(config.positionCorrection, &result.payload);
    result.payload.insert(QStringLiteral("errorCode"), status);
    result.payload.insert(QStringLiteral("errorMessage"), message);
    return result;
}

static QJsonArray topClassesToJson(
        const QVector<RegisteredClassificationClassScore> &topClasses)
{
    QJsonArray array;
    for (const RegisteredClassificationClassScore &item : topClasses) {
        QJsonObject json;
        json.insert(QStringLiteral("label"), item.label);
        json.insert(QStringLiteral("classId"), item.classId);
        json.insert(QStringLiteral("score"), item.score);
        array.append(json);
    }
    return array;
}

static QRect normalizedRoiToPixels(
        const QRectF &sourceRoi,
        int width,
        int height)
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

QString halconErrorText(HalconInferenceApi *api, const Herror status)
{
    char buffer[1024] = {0};
    if (api && api->getErrorText &&
        halconStatusOk(api->getErrorText(static_cast<Hlong>(status), buffer))) {
        const QString message = QString::fromUtf8(buffer).trimmed();
        if (!message.isEmpty())
            return message;
    }
    return QStringLiteral("HALCON error code %1").arg(static_cast<qlonglong>(status));
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

        if (!resolveRequired(m_handle, api.getErrorText, "get_error_text", errorMessage) ||
            !resolveRequired(m_handle, api.createTuple, "F_create_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.setDouble, "F_set_d", errorMessage) ||
            !resolveRequired(m_handle, api.setInt, "F_set_i", errorMessage) ||
            !resolveRequired(m_handle, api.setString, "F_set_s", errorMessage) ||
            !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
            !resolveRequired(m_handle, api.getInt, "F_get_i", errorMessage) ||
            !resolveRequired(m_handle, api.readClassMlp, "T_read_class_mlp", errorMessage) ||
            !resolveRequired(m_handle, api.classifyClassMlp, "T_classify_class_mlp", errorMessage) ||
            !resolveRequired(m_handle, api.clearClassMlp, "T_clear_class_mlp", errorMessage)) {
            symbolMissing = true;
            dlclose(m_handle);
            m_handle = nullptr;
            return false;
        }

        resolveRequired(m_handle, api.setUtf8, "SetHcInterfaceStringEncodingIsUtf8", errorMessage);
        if (api.setUtf8)
            api.setUtf8(1);
        return true;
    }

    HalconInferenceApi api;

private:
    void *m_handle = nullptr;
};

class HalconTuple
{
public:
    explicit HalconTuple(HalconInferenceApi *api = nullptr)
        : m_api(api)
    {
    }

    HalconTuple(const HalconTuple &) = delete;
    HalconTuple &operator=(const HalconTuple &) = delete;

    ~HalconTuple()
    {
        destroy();
    }

    void create(int size)
    {
        destroy();
        if (m_api)
            m_api->createTuple(&m_tuple, static_cast<Hlong>(size));
    }

    void setDouble(int index, double value)
    {
        if (m_api)
            m_api->setDouble(&m_tuple, value, static_cast<Hlong>(index));
    }

    void setInt(int index, Hlong value)
    {
        if (m_api)
            m_api->setInt(&m_tuple, value, static_cast<Hlong>(index));
    }

    void setString(int index, const QString &value)
    {
        if (!m_api)
            return;
        const QByteArray utf8 = value.toUtf8();
        m_api->setString(&m_tuple, utf8.constData(), static_cast<Hlong>(index));
    }

    int size() const
    {
        return static_cast<int>(m_tuple.num);
    }

    double doubleAt(int index) const
    {
        return m_api ? m_api->getDouble(&m_tuple, static_cast<Hlong>(index)) : 0.0;
    }

    Hlong intAt(int index) const
    {
        return m_api ? m_api->getInt(&m_tuple, static_cast<Hlong>(index)) : 0;
    }

    const Htuple &value() const
    {
        return m_tuple;
    }

    Htuple *ptr()
    {
        return &m_tuple;
    }

private:
    void destroy()
    {
        if (m_api)
            m_api->destroyTuple(&m_tuple);
        m_tuple = HTUPLE_INITIALIZER;
    }

    HalconInferenceApi *m_api = nullptr;
    Htuple m_tuple = HTUPLE_INITIALIZER;
};

class HalconMlpHandleGuard
{
public:
    explicit HalconMlpHandleGuard(HalconInferenceApi *api)
        : m_api(api)
        , m_handle(api)
    {
    }

    ~HalconMlpHandleGuard()
    {
        if (m_hasHandle && m_api)
            m_api->clearClassMlp(m_handle.value());
    }

    Htuple *outPtr()
    {
        return m_handle.ptr();
    }

    void markCreated()
    {
        m_hasHandle = true;
    }

    const Htuple &value() const
    {
        return m_handle.value();
    }

private:
    HalconInferenceApi *m_api = nullptr;
    HalconTuple m_handle;
    bool m_hasHandle = false;
};

ToolOverlay makeRoiOverlay(const QRect &roi)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.label = QStringLiteral("detect_roi");
    overlay.rect = QRectF(roi);
    return overlay;
}

ToolOverlay makeTextOverlay(const QRect &roi,
                            const RegisteredClassificationHalconResult &result)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.rect = QRectF(roi);
    overlay.p1 = QPointF(roi.left(), qMax(0, roi.top() - 24));
    overlay.label = QStringLiteral("result");
    overlay.text = QStringLiteral("%1: %2 (%3%, %4 ms)")
            .arg(result.ok ? QStringLiteral("OK") : QStringLiteral("NG"),
                 result.predictedLabel,
                 QString::number(result.score, 'f', 1),
                 QString::number(result.elapsedMs));
    overlay.score = result.score;
    overlay.extra.insert(QStringLiteral("status"), result.status);
    return overlay;
}

} // namespace

RegisteredClassificationHalconResult RegisteredClassificationHalconRunner::run(
        const cv::Mat &image,
        const RegisteredClassificationHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    if (image.empty()) {
        return makeError(QStringLiteral("image_empty"),
                         QStringLiteral("Input image is empty."),
                         config,
                         image,
                         timer.elapsed());
    }

    const QString modelDir = config.modelPath.trimmed();
    if (modelDir.isEmpty()) {
        return makeError(QStringLiteral("model_path_empty"),
                         QStringLiteral("Classification model path is empty."),
                         config,
                         image,
                         timer.elapsed());
    }

    if (config.modelType != registeredClassificationMlpModelType()) {
        return makeError(QStringLiteral("unsupported_model_type"),
                         QStringLiteral("Only halcon_mlp_registered_classification is supported."),
                         config,
                         image,
                         timer.elapsed());
    }

    RegisteredClassificationModelMetadata metadata;
    const RegisteredClassificationModelPackageResult metadataResult =
            readRegisteredClassificationMetadata(modelDir, &metadata);
    if (!metadataResult.success) {
        return makeError(metadataResult.status,
                         metadataResult.message,
                         config,
                         image,
                         timer.elapsed());
    }

    const QString mlpPath = registeredClassificationMlpPath(modelDir);
    if (!QFileInfo::exists(mlpPath)) {
        return makeError(QStringLiteral("model_file_not_found"),
                         QStringLiteral("model.gmc does not exist."),
                         config,
                         image,
                         timer.elapsed());
    }

    const int effectiveRejectScore = qBound(0, metadata.thresholds.rejectScore, 100);
    const int effectiveTop2Gap = qMax(0, metadata.thresholds.top2Gap);

    const QRect roiPixels = isRectangleRegionType(config.detectRegionType)
            ? normalizedRoiToPixels(config.roiNormalized, image.cols, image.rows)
            : QRect(0, 0, image.cols, image.rows);
    if (roiPixels.isEmpty() ||
        roiPixels.width() < kMinRoiPixelSize ||
        roiPixels.height() < kMinRoiPixelSize) {
        return makeError(QStringLiteral("invalid_roi"),
                         QStringLiteral("Detection rectangle ROI is invalid."),
                         config,
                         image,
                         timer.elapsed());
    }

    HalconRuntimePaths::initializeHalconEnvironment();
    QStringList halconCandidates;
    QString halconLibPath = HalconRuntimePaths::resolveHalconLibPath(config.halconSoPath, &halconCandidates);
    for (const QString &candidate : config.halconSoPathCandidates) {
        if (!halconCandidates.contains(candidate))
            halconCandidates.append(candidate);
        if (halconLibPath.isEmpty() && QFileInfo::exists(candidate))
            halconLibPath = candidate;
    }
    if (halconLibPath.isEmpty()) {
        RegisteredClassificationHalconResult result =
                makeError(QStringLiteral("halcon_so_not_found"),
                          QStringLiteral("HALCON runtime file not found."),
                          config,
                          image,
                          timer.elapsed());
        result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
        result.payload.insert(QStringLiteral("halconSoPathCandidates"), halconCandidates.join(QStringLiteral("; ")));
        return result;
    }

    RegisteredClassificationFeatureConfig featureConfig;
    featureConfig.halconSoPath = halconLibPath;
    featureConfig.halconSoPathCandidates = halconCandidates;
    RegisteredClassificationFeatureExtractor extractor;
    const RegisteredClassificationFeatureResult featureResult =
            extractor.extract(image, isRectangleRegionType(config.detectRegionType)
                                      ? config.roiNormalized
                                      : QRectF(0.0, 0.0, 1.0, 1.0),
                              featureConfig);
    if (!featureResult.success) {
        return makeError(QStringLiteral("feature_extract_failed"),
                         QStringLiteral("%1: %2").arg(featureResult.status, featureResult.message),
                         config,
                         image,
                         timer.elapsed());
    }
    if (featureResult.feature.size() != metadata.featureLength) {
        return makeError(QStringLiteral("unsupported_feature_version"),
                         QStringLiteral("Feature length does not match metadata."),
                         config,
                         image,
                         timer.elapsed());
    }
    if (metadata.featureVersion != registeredClassificationFeatureVersionV1()) {
        return makeError(QStringLiteral("unsupported_feature_version"),
                         QStringLiteral("Only halcon_mlp_roi_stats_v1 is supported."),
                         config,
                         image,
                         timer.elapsed());
    }

    HalconLibrary library;
    QString loadMessage;
    bool symbolMissing = false;
    if (!library.load(halconLibPath, loadMessage, symbolMissing)) {
        RegisteredClassificationHalconResult result =
                makeError(symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                        : QStringLiteral("halcon_load_failed"),
                          loadMessage,
                          config,
                          image,
                          timer.elapsed());
        result.payload.insert(QStringLiteral("halconSoPath"), halconLibPath);
        result.payload.insert(QStringLiteral("halconSoPathCandidates"), halconCandidates.join(QStringLiteral("; ")));
        return result;
    }

    HalconInferenceApi *api = &library.api;
    HalconMlpHandleGuard mlp(api);
    HalconTuple mlpPathTuple(api);
    HalconTuple featuresTuple(api);
    HalconTuple topKTuple(api);
    HalconTuple classesTuple(api);
    HalconTuple confidencesTuple(api);

    mlpPathTuple.create(1);
    mlpPathTuple.setString(0, mlpPath);
    const Herror readStatus = api->readClassMlp(mlpPathTuple.value(), mlp.outPtr());
    if (!halconStatusOk(readStatus)) {
        return makeError(QStringLiteral("mlp_read_failed"),
                         halconErrorText(api, readStatus),
                         config,
                         image,
                         timer.elapsed());
    }
    mlp.markCreated();

    featuresTuple.create(featureResult.feature.size());
    for (int i = 0; i < featureResult.feature.size(); ++i)
        featuresTuple.setDouble(i, featureResult.feature.at(i));

    topKTuple.create(1);
    topKTuple.setInt(0, static_cast<Hlong>(qBound(1, config.topK, metadata.classLabels.size())));
    const Herror classifyStatus = api->classifyClassMlp(mlp.value(),
                                                        featuresTuple.value(),
                                                        topKTuple.value(),
                                                        classesTuple.ptr(),
                                                        confidencesTuple.ptr());
    if (!halconStatusOk(classifyStatus)) {
        return makeError(QStringLiteral("classification_failed"),
                         halconErrorText(api, classifyStatus),
                         config,
                         image,
                         timer.elapsed());
    }

    QVector<RegisteredClassificationClassScore> topClasses;
    const int classCount = qMin(classesTuple.size(), confidencesTuple.size());
    topClasses.reserve(classCount);
    for (int i = 0; i < classCount; ++i) {
        const int rawClassValue = static_cast<int>(classesTuple.intAt(i));
        int classIndex = -1;
        if (rawClassValue >= 1 && rawClassValue <= metadata.classLabels.size())
            classIndex = rawClassValue - 1;
        else if (rawClassValue >= 0 && rawClassValue < metadata.classLabels.size())
            classIndex = rawClassValue;
        if (classIndex < 0 || classIndex >= metadata.classLabels.size())
            continue;

        RegisteredClassificationClassScore score;
        score.classId = metadata.classLabels.at(classIndex).id;
        score.label = metadata.classLabels.at(classIndex).name;
        score.score = qBound(0.0, confidencesTuple.doubleAt(i) * 100.0, 100.0);
        topClasses.append(score);
    }
    std::sort(topClasses.begin(), topClasses.end(),
              [](const RegisteredClassificationClassScore &left,
                 const RegisteredClassificationClassScore &right) {
                  return left.score > right.score;
              });
    if (topClasses.isEmpty()) {
        return makeError(QStringLiteral("classification_failed"),
                         QStringLiteral("classify_class_mlp returned no classes."),
                         config,
                         image,
                         timer.elapsed());
    }

    const RegisteredClassificationClassScore top = topClasses.first();

    RegisteredClassificationHalconResult result;
    result.success = true;
    result.ok = false;
    result.status = QStringLiteral("ng");
    result.message = QStringLiteral("Classification completed.");
    result.predictedLabel = top.label;
    result.predictedClassId = top.classId;
    result.score = top.score;
    result.rejectScore = effectiveRejectScore;
    result.top2Gap = effectiveTop2Gap;
    result.topClasses = topClasses;
    result.elapsedMs = timer.elapsed();

    if (top.score < static_cast<double>(effectiveRejectScore)) {
        result.status = QStringLiteral("classification_rejected");
        result.message = QStringLiteral("Top-1 score is below rejectScore.");
    } else if (effectiveTop2Gap > 0 && topClasses.size() >= 2) {
        const double gap = topClasses.at(0).score - topClasses.at(1).score;
        if (gap < static_cast<double>(effectiveTop2Gap)) {
            result.status = QStringLiteral("classification_ambiguous");
            result.message = QStringLiteral("Top-1 and Top-2 scores are too close.");
        }
    }

    if (result.status == QStringLiteral("ng")) {
        const QString judgeMode = config.judgeMode.trimmed().toLower();
        if (judgeMode == QStringLiteral("class_match")) {
            if (config.expectedLabel.trimmed().isEmpty()) {
                return makeError(QStringLiteral("missing_expected_label"),
                                 QStringLiteral("Expected label is empty for class_match judgement."),
                                 config,
                                 image,
                                 timer.elapsed());
            }
            result.ok = (result.predictedLabel == config.expectedLabel.trimmed());
        } else {
            result.ok = result.score >= static_cast<double>(qBound(0, config.minScore, 100));
        }
        result.status = result.ok ? QStringLiteral("ok") : QStringLiteral("ng");
        result.message = result.ok
                ? QStringLiteral("Classification completed.")
                : QStringLiteral("Classification result does not satisfy the judge rule.");
    }

    result.payload.insert(QStringLiteral("algorithm"), registeredClassificationMlpModelType());
    result.payload.insert(QStringLiteral("modelPath"), modelDir);
    result.payload.insert(QStringLiteral("modelName"), config.modelName);
    result.payload.insert(QStringLiteral("modelType"), config.modelType);
    result.payload.insert(QStringLiteral("schemaVersion"), metadata.schemaVersion);
    result.payload.insert(QStringLiteral("featureVersion"), metadata.featureVersion);
    result.payload.insert(QStringLiteral("predictedLabel"), result.predictedLabel);
    result.payload.insert(QStringLiteral("predictedClassId"), result.predictedClassId);
    result.payload.insert(QStringLiteral("score"), result.score);
    result.payload.insert(QStringLiteral("topClasses"), topClassesToJson(result.topClasses));
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("roiPixelsRect"), rectToJson(QRectF(roiPixels)));
    result.payload.insert(QStringLiteral("featureLength"), metadata.featureLength);
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
    result.payload.insert(QStringLiteral("judgeMode"), config.judgeMode);
    result.payload.insert(QStringLiteral("expectedLabel"), config.expectedLabel);
    result.payload.insert(QStringLiteral("minScore"), config.minScore);
    result.payload.insert(QStringLiteral("rejectScore"), result.rejectScore);
    result.payload.insert(QStringLiteral("top2Gap"), result.top2Gap);
    result.payload.insert(QStringLiteral("trainingSampleCount"), metadata.trainingSampleCount);
    result.payload.insert(QStringLiteral("classLabels"), classLabelsToJson(metadata.classLabels));
    result.payload.insert(QStringLiteral("halconSoPath"), halconLibPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"), halconCandidates.join(QStringLiteral("; ")));
    PositionCorrection::writeNotAppliedPayload(config.positionCorrection, &result.payload);
    result.payload.insert(QStringLiteral("errorCode"), result.status);
    result.payload.insert(QStringLiteral("errorMessage"), result.message);

    result.overlays.append(makeRoiOverlay(roiPixels));
    result.overlays.append(makeTextOverlay(roiPixels, result));
    return result;
}
