#include "algorithms/recognition/ColorRecognitionGmmHalconBackend.h"

#include <HalconC.h>

#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <dlfcn.h>
#include <limits>
#include <memory>
#include <utility>

namespace {

constexpr qint64 kMaxSerializedBytes = 8 * 1024 * 1024;
constexpr int kMinimumPixelsPerClass = 256;
const char kAlgorithmVersion[] = "halcon_cielab_gmm_classification_v1";
const char kFeatureSchemaVersion[] = "cielab_gmm_pixel_classification_v1";
const char kSamplingAlgorithmVersion[] = "halcon_region_grid_points_v3";

bool statusOk(const Herror status)
{
    return status == H_MSG_OK || status == H_MSG_TRUE || status == H_MSG_FALSE;
}

bool objectAllocated(const Hobject object)
{
    return object != 0 && object != NO_OBJECTS && object != EMPTY_REGION;
}

struct Failure
{
    QString status;
    QString message;
    QString stage;
    Herror halconCode = H_MSG_OK;
};

struct GmmApi
{
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
    using BitRshiftFn = Herror (*)(const Hobject, Hobject *, Hlong);
    using GenRectangle1Fn = Herror (*)(Hobject *, double, double, double, double);
    using GenCircleFn = Herror (*)(Hobject *, double, double, double);
    using GenRegionPolygonFilledFn = Herror (*)(Hobject *, const Htuple, const Htuple);
    using SelectObjFn = Herror (*)(const Hobject, Hobject *, Hlong);
    using IntersectionFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using DifferenceFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using GetDomainFn = Herror (*)(const Hobject, Hobject *);
    using AreaCenterFn = Herror (*)(const Hobject, Hlong *, double *, double *);
    using MinMaxGrayFn = Herror (*)(const Hobject, const Hobject, double, double *, double *, double *);
    using ConvertImageTypeFn = Herror (*)(const Hobject, Hobject *, const char *);
    using ScaleImageFn = Herror (*)(const Hobject, Hobject *, double, double);
    using TransFromRgbFn = Herror (*)(const Hobject, const Hobject, const Hobject,
                                      Hobject *, Hobject *, Hobject *, const char *);
    using Compose2Fn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using Compose3Fn = Herror (*)(const Hobject, const Hobject, const Hobject, Hobject *);
    using GenGridRegionFn = Herror (*)(Hobject *, const Htuple, const Htuple,
                                       const Htuple, const Htuple, const Htuple);
    using MoveRegionFn = Herror (*)(const Hobject, Hobject *, Hlong, Hlong);
    using GenEmptyRegionFn = Herror (*)(Hobject *);
    using ConcatObjFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using ClearObjFn = Herror (*)(const Hobject);
    using CreateClassGmmFn = Herror (*)(const Htuple, const Htuple, const Htuple, const Htuple,
                                        const Htuple, const Htuple, const Htuple, Htuple *);
    using AddSamplesImageClassGmmFn = Herror (*)(const Hobject, const Hobject,
                                                 const Htuple, const Htuple);
    using TrainClassGmmFn = Herror (*)(const Htuple, const Htuple, const Htuple, const Htuple,
                                       const Htuple, Htuple *, Htuple *);
    using SerializeClassGmmFn = Herror (*)(const Htuple, Htuple *);
    using GetSerializedItemPtrFn = Herror (*)(const Htuple, Htuple *, Htuple *);
    using CreateSerializedItemPtrFn = Herror (*)(const Htuple, const Htuple,
                                                 const Htuple, Htuple *);
    using DeserializeClassGmmFn = Herror (*)(const Htuple, Htuple *);
    using ClassifyImageClassGmmFn = Herror (*)(const Hobject, Hobject *, const Htuple, const Htuple);
    using ClearSerializedItemFn = Herror (*)(const Htuple);
    using ClearClassGmmFn = Herror (*)(const Htuple);

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
    BitRshiftFn bitRshift = nullptr;
    GenRectangle1Fn genRectangle1 = nullptr;
    GenCircleFn genCircle = nullptr;
    GenRegionPolygonFilledFn genRegionPolygonFilled = nullptr;
    SelectObjFn selectObj = nullptr;
    IntersectionFn intersection = nullptr;
    DifferenceFn difference = nullptr;
    ReduceDomainFn reduceDomain = nullptr;
    GetDomainFn getDomain = nullptr;
    AreaCenterFn areaCenter = nullptr;
    MinMaxGrayFn minMaxGray = nullptr;
    ConvertImageTypeFn convertImageType = nullptr;
    ScaleImageFn scaleImage = nullptr;
    TransFromRgbFn transFromRgb = nullptr;
    Compose2Fn compose2 = nullptr;
    Compose3Fn compose3 = nullptr;
    GenGridRegionFn genGridRegion = nullptr;
    MoveRegionFn moveRegion = nullptr;
    GenEmptyRegionFn genEmptyRegion = nullptr;
    ConcatObjFn concatObj = nullptr;
    ClearObjFn clearObj = nullptr;
    CreateClassGmmFn createClassGmm = nullptr;
    AddSamplesImageClassGmmFn addSamplesImageClassGmm = nullptr;
    TrainClassGmmFn trainClassGmm = nullptr;
    SerializeClassGmmFn serializeClassGmm = nullptr;
    GetSerializedItemPtrFn getSerializedItemPtr = nullptr;
    CreateSerializedItemPtrFn createSerializedItemPtr = nullptr;
    DeserializeClassGmmFn deserializeClassGmm = nullptr;
    ClassifyImageClassGmmFn classifyImageClassGmm = nullptr;
    ClearSerializedItemFn clearSerializedItem = nullptr;
    ClearClassGmmFn clearClassGmm = nullptr;
};

template <typename Function>
bool resolve(void *handle, Function &target, const char *name, QString *error)
{
    dlerror();
    void *symbol = dlsym(handle, name);
    const char *symbolError = dlerror();
    if (!symbol || symbolError) {
        *error = QStringLiteral("Missing HALCON symbol %1: %2")
                .arg(QString::fromLatin1(name),
                     symbolError ? QString::fromLocal8Bit(symbolError) : QStringLiteral("not found"));
        return false;
    }
    target = reinterpret_cast<Function>(symbol);
    return true;
}

class GmmLibrary
{
public:
    ~GmmLibrary()
    {
        if (m_handle)
            dlclose(m_handle);
    }

    bool load(const QString &path, QString *error)
    {
        m_handle = dlopen(path.toLocal8Bit().constData(), RTLD_NOW | RTLD_LOCAL);
        if (!m_handle) {
            const char *loadError = dlerror();
            *error = loadError ? QString::fromLocal8Bit(loadError)
                               : QStringLiteral("dlopen returned null");
            return false;
        }

        return resolve(m_handle, api.getErrorText, "get_error_text", error) &&
                resolve(m_handle, api.createTuple, "F_create_tuple", error) &&
                resolve(m_handle, api.setDouble, "F_set_d", error) &&
                resolve(m_handle, api.setInt, "F_set_i", error) &&
                resolve(m_handle, api.setString, "F_set_s", error) &&
                resolve(m_handle, api.destroyTuple, "F_destroy_tuple", error) &&
                resolve(m_handle, api.getDouble, "F_get_d", error) &&
                resolve(m_handle, api.getInt, "F_get_i", error) &&
                resolve(m_handle, api.genImageInterleaved, "gen_image_interleaved", error) &&
                resolve(m_handle, api.decompose3, "decompose3", error) &&
                resolve(m_handle, api.bitRshift, "bit_rshift", error) &&
                resolve(m_handle, api.genRectangle1, "gen_rectangle1", error) &&
                resolve(m_handle, api.genCircle, "gen_circle", error) &&
                resolve(m_handle, api.genRegionPolygonFilled, "T_gen_region_polygon_filled", error) &&
                resolve(m_handle, api.selectObj, "select_obj", error) &&
                resolve(m_handle, api.intersection, "intersection", error) &&
                resolve(m_handle, api.difference, "difference", error) &&
                resolve(m_handle, api.reduceDomain, "reduce_domain", error) &&
                resolve(m_handle, api.getDomain, "get_domain", error) &&
                resolve(m_handle, api.areaCenter, "area_center", error) &&
                resolve(m_handle, api.minMaxGray, "min_max_gray", error) &&
                resolve(m_handle, api.convertImageType, "convert_image_type", error) &&
                resolve(m_handle, api.scaleImage, "scale_image", error) &&
                resolve(m_handle, api.transFromRgb, "trans_from_rgb", error) &&
                resolve(m_handle, api.compose2, "compose2", error) &&
                resolve(m_handle, api.compose3, "compose3", error) &&
                resolve(m_handle, api.genGridRegion, "T_gen_grid_region", error) &&
                resolve(m_handle, api.moveRegion, "move_region", error) &&
                resolve(m_handle, api.genEmptyRegion, "gen_empty_region", error) &&
                resolve(m_handle, api.concatObj, "concat_obj", error) &&
                resolve(m_handle, api.clearObj, "clear_obj", error) &&
                resolve(m_handle, api.createClassGmm, "T_create_class_gmm", error) &&
                resolve(m_handle, api.addSamplesImageClassGmm, "T_add_samples_image_class_gmm", error) &&
                resolve(m_handle, api.trainClassGmm, "T_train_class_gmm", error) &&
                resolve(m_handle, api.serializeClassGmm, "T_serialize_class_gmm", error) &&
                resolve(m_handle, api.getSerializedItemPtr, "T_get_serialized_item_ptr", error) &&
                resolve(m_handle, api.createSerializedItemPtr, "T_create_serialized_item_ptr", error) &&
                resolve(m_handle, api.deserializeClassGmm, "T_deserialize_class_gmm", error) &&
                resolve(m_handle, api.classifyImageClassGmm, "T_classify_image_class_gmm", error) &&
                resolve(m_handle, api.clearSerializedItem, "T_clear_serialized_item", error) &&
                resolve(m_handle, api.clearClassGmm, "T_clear_class_gmm", error);
    }

    GmmApi api;

private:
    void *m_handle = nullptr;
};

std::shared_ptr<GmmLibrary> acquireLibrary(const QString &path, QString *error)
{
    static QMutex mutex;
    static QHash<QString, std::shared_ptr<GmmLibrary>> cache;

    QFileInfo info(path);
    QString normalized = info.canonicalFilePath();
    if (normalized.isEmpty())
        normalized = info.absoluteFilePath();
    const QString key = normalized + QStringLiteral("|gmm");

    QMutexLocker locker(&mutex);
    const auto found = cache.constFind(key);
    if (found != cache.constEnd())
        return found.value();

    std::shared_ptr<GmmLibrary> library = std::make_shared<GmmLibrary>();
    if (!library->load(normalized, error))
        return {};
    cache.insert(key, library);
    return library;
}

class Tuple
{
public:
    explicit Tuple(GmmApi *api = nullptr) : m_api(api) {}
    Tuple(const Tuple &) = delete;
    Tuple &operator=(const Tuple &) = delete;
    ~Tuple() { reset(); }

    void create(int size)
    {
        reset();
        m_api->createTuple(&m_value, size);
    }
    void integer(int index, Hlong value) { m_api->setInt(&m_value, value, index); }
    void real(int index, double value) { m_api->setDouble(&m_value, value, index); }
    void string(int index, const QByteArray &value) { m_api->setString(&m_value, value.constData(), index); }
    Hlong integerAt(int index) const { return m_api->getInt(&m_value, index); }
    double realAt(int index) const { return m_api->getDouble(&m_value, index); }
    int size() const { return static_cast<int>(m_value.num); }
    Htuple *ptr() { return &m_value; }
    const Htuple &value() const { return m_value; }
    void reset()
    {
        if (m_api && (m_value.num > 0 || m_value.capacity > 0))
            m_api->destroyTuple(&m_value);
        m_value = HTUPLE_INITIALIZER;
    }

private:
    GmmApi *m_api = nullptr;
    Htuple m_value = HTUPLE_INITIALIZER;
};

QString errorText(GmmApi *api, Herror status)
{
    char buffer[1024] = {0};
    if (api->getErrorText && statusOk(api->getErrorText(status, buffer))) {
        const QString text = QString::fromUtf8(buffer).trimmed();
        if (!text.isEmpty())
            return text;
    }
    return QStringLiteral("HALCON error %1").arg(static_cast<qlonglong>(status));
}

void check(GmmApi *api, Herror status, const QString &stage)
{
    if (statusOk(status))
        return;
    const QString message = errorText(api, status);
    throw Failure{message.contains(QStringLiteral("license"), Qt::CaseInsensitive)
                  ? QStringLiteral("halcon_license_error")
                  : QStringLiteral("gmm_training_failed"),
                  QStringLiteral("%1: %2").arg(stage, message), stage, status};
}

void checkRun(GmmApi *api, Herror status, const QString &stage)
{
    if (statusOk(status))
        return;
    const QString message = errorText(api, status);
    QString failureStatus = QStringLiteral("gmm_classification_failed");
    if (message.contains(QStringLiteral("license"), Qt::CaseInsensitive))
        failureStatus = QStringLiteral("halcon_license_error");
    else if (stage.contains(QStringLiteral("deserialize")))
        failureStatus = QStringLiteral("gmm_deserialize_failed");
    throw Failure{failureStatus, QStringLiteral("%1: %2").arg(stage, message), stage, status};
}

void clearObject(GmmApi *api, Hobject &object)
{
    if (objectAllocated(object))
        api->clearObj(object);
    object = NO_OBJECTS;
}

QString sha256(const QByteArray &bytes)
{
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(
                QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
}

QString canonicalImageHash(const cv::Mat &image)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QByteArray header = QByteArray::number(image.rows) + 'x' +
            QByteArray::number(image.cols) + ':' + QByteArray::number(image.type()) + ':';
    hash.addData(header);
    const int rowBytes = image.cols * static_cast<int>(image.elemSize());
    for (int row = 0; row < image.rows; ++row)
        hash.addData(reinterpret_cast<const char *>(image.ptr(row)), rowBytes);
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(hash.result().toHex()));
}

QRect normalizedRoi(const QRectF &source, int width, int height)
{
    if (!std::isfinite(source.x()) || !std::isfinite(source.y()) ||
        !std::isfinite(source.width()) || !std::isfinite(source.height()) ||
        source.width() <= 0.0 || source.height() <= 0.0 || width <= 0 || height <= 0) {
        return {};
    }
    const QRectF roi = source.normalized();
    const double left = qBound(0.0, roi.left(), 1.0);
    const double top = qBound(0.0, roi.top(), 1.0);
    const double right = qBound(0.0, roi.right(), 1.0);
    const double bottom = qBound(0.0, roi.bottom(), 1.0);
    if (right <= left || bottom <= top)
        return {};
    const int x1 = qBound(0, static_cast<int>(std::floor(left * width)), width - 1);
    const int y1 = qBound(0, static_cast<int>(std::floor(top * height)), height - 1);
    const int x2 = qBound(0, static_cast<int>(std::ceil(right * width)), width);
    const int y2 = qBound(0, static_cast<int>(std::ceil(bottom * height)), height);
    return x2 > x1 && y2 > y1 ? QRect(x1, y1, x2 - x1, y2 - y1) : QRect();
}

struct PreparedSample
{
    const ColorRecognitionGmmBuildSample *source = nullptr;
    QRect roi;
    int quota = 0;
};

QVector<int> balancedQuotas(const QVector<PreparedSample> &samples, int target)
{
    QVector<int> quotas(samples.size(), 0);
    QVector<int> remainingCapacity;
    remainingCapacity.reserve(samples.size());
    for (const PreparedSample &sample : samples)
        remainingCapacity.append(sample.roi.width() * sample.roi.height());

    int remaining = target;
    while (remaining > 0) {
        int active = 0;
        for (int capacity : remainingCapacity)
            active += capacity > 0 ? 1 : 0;
        if (active == 0)
            break;
        const int share = qMax(1, (remaining + active - 1) / active);
        bool progressed = false;
        for (int i = 0; i < remainingCapacity.size() && remaining > 0; ++i) {
            if (remainingCapacity.at(i) <= 0)
                continue;
            const int assigned = qMin(qMin(share, remainingCapacity.at(i)), remaining);
            quotas[i] += assigned;
            remainingCapacity[i] -= assigned;
            remaining -= assigned;
            progressed = progressed || assigned > 0;
        }
        if (!progressed)
            break;
    }
    return quotas;
}

QJsonArray intArray(const QVector<int> &values)
{
    QJsonArray result;
    for (int value : values)
        result.append(value);
    return result;
}

QByteArray canonicalJson(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QJsonObject buildContract(const QString &channels,
                          const QVector<int> &classIdOrder,
                          const QVector<ColorRecognitionGmmClassDiagnostics> &classes,
                          int maxSamplesPerClass,
                          const QString &trainingDataHash)
{
    QJsonArray classJson;
    for (const ColorRecognitionGmmClassDiagnostics &item : classes) {
        QJsonObject json;
        json.insert(QStringLiteral("classId"), item.classId);
        json.insert(QStringLiteral("label"), item.label);
        json.insert(QStringLiteral("roiCount"), item.roiCount);
        json.insert(QStringLiteral("availablePixels"), static_cast<double>(item.availablePixels));
        json.insert(QStringLiteral("requestedTrainingPixels"), item.requestedTrainingPixels);
        json.insert(QStringLiteral("trainingPixels"), item.trainingPixels);
        json.insert(QStringLiteral("minCenters"), item.minCenters);
        json.insert(QStringLiteral("maxCenters"), item.maxCenters);
        classJson.append(json);
    }
    QJsonObject result;
    result.insert(QStringLiteral("modelSchemaVersion"), 3);
    result.insert(QStringLiteral("algorithmVersion"), QString::fromLatin1(kAlgorithmVersion));
    result.insert(QStringLiteral("featureSchemaVersion"), QString::fromLatin1(kFeatureSchemaVersion));
    result.insert(QStringLiteral("colorSpace"), QStringLiteral("cielab"));
    result.insert(QStringLiteral("colorChannels"), channels);
    result.insert(QStringLiteral("numDim"), channels == QStringLiteral("lab") ? 3 : 2);
    result.insert(QStringLiteral("classIdOrder"), intArray(classIdOrder));
    result.insert(QStringLiteral("classes"), classJson);
    result.insert(QStringLiteral("covarianceType"), QStringLiteral("full"));
    result.insert(QStringLiteral("preprocessing"), QStringLiteral("none"));
    result.insert(QStringLiteral("randSeed"), 42);
    result.insert(QStringLiteral("maxIter"), 100);
    result.insert(QStringLiteral("threshold"), 1e-4);
    result.insert(QStringLiteral("classPriors"), QStringLiteral("uniform"));
    result.insert(QStringLiteral("regularize"), 1e-4);
    result.insert(QStringLiteral("samplingAlgorithmVersion"), QString::fromLatin1(kSamplingAlgorithmVersion));
    result.insert(QStringLiteral("maxSamplesPerClass"), maxSamplesPerClass);
    result.insert(QStringLiteral("trainingDataHash"), trainingDataHash);
    result.insert(QStringLiteral("halconVersion"), QStringLiteral("20.11"));
    return result;
}

QString normalizedPixelFormat(const ColorRecognitionGmmBuildSample &sample)
{
    return sample.pixelFormat.trimmed().toUpper();
}

struct HalconLabImage
{
    Hobject interleaved = NO_OBJECTS;
    Hobject red = NO_OBJECTS;
    Hobject green = NO_OBJECTS;
    Hobject blue = NO_OBJECTS;
    Hobject shiftedRed = NO_OBJECTS;
    Hobject shiftedGreen = NO_OBJECTS;
    Hobject shiftedBlue = NO_OBJECTS;
    Hobject realRed = NO_OBJECTS;
    Hobject realGreen = NO_OBJECTS;
    Hobject realBlue = NO_OBJECTS;
    Hobject normalizedRed = NO_OBJECTS;
    Hobject normalizedGreen = NO_OBJECTS;
    Hobject normalizedBlue = NO_OBJECTS;
    Hobject lightness = NO_OBJECTS;
    Hobject a = NO_OBJECTS;
    Hobject b = NO_OBJECTS;
    Hobject feature = NO_OBJECTS;
    Hobject fullRegion = NO_OBJECTS;
    Hobject roiRegion = NO_OBJECTS;
};

void clearLabImage(GmmApi *api, HalconLabImage *image)
{
    clearObject(api, image->roiRegion);
    clearObject(api, image->fullRegion);
    clearObject(api, image->feature);
    clearObject(api, image->b);
    clearObject(api, image->a);
    clearObject(api, image->lightness);
    clearObject(api, image->normalizedBlue);
    clearObject(api, image->normalizedGreen);
    clearObject(api, image->normalizedRed);
    clearObject(api, image->realBlue);
    clearObject(api, image->realGreen);
    clearObject(api, image->realRed);
    clearObject(api, image->shiftedBlue);
    clearObject(api, image->shiftedGreen);
    clearObject(api, image->shiftedRed);
    clearObject(api, image->blue);
    clearObject(api, image->green);
    clearObject(api, image->red);
    clearObject(api, image->interleaved);
}

HalconLabImage makeLabImage(GmmApi *api,
                            const ColorRecognitionGmmBuildSample &sample,
                            const QRect &roi,
                            const QString &channels)
{
    HalconLabImage result;
    try {
        cv::Mat image = sample.image.isContinuous() ? sample.image : sample.image.clone();
        const QString format = normalizedPixelFormat(sample);
        const bool input16 = image.depth() == CV_16U;
        QByteArray halconFormat;
        if (format == QStringLiteral("BGR8")) halconFormat = "bgr";
        else if (format == QStringLiteral("RGB8")) halconFormat = "rgb";
        else if (format == QStringLiteral("BGRA8")) halconFormat = "bgrx";
        else if (format == QStringLiteral("RGBA8")) halconFormat = "rgbx";
        else if (format == QStringLiteral("BGR16")) halconFormat = "bgr48";
        else if (format == QStringLiteral("RGB16")) halconFormat = "rgb48";
        else if (format == QStringLiteral("BGRA16")) halconFormat = "bgrx64";
        else if (format == QStringLiteral("RGBA16")) halconFormat = "rgbx64";
        else throw Failure{QStringLiteral("invalid_pixel_format_metadata"),
                           QStringLiteral("Unsupported pixelFormat: %1").arg(format), {}, H_MSG_OK};

        check(api, api->genImageInterleaved(&result.interleaved,
                                             reinterpret_cast<Hlong>(image.data),
                                             halconFormat.constData(), image.cols, image.rows, 0,
                                             input16 ? "uint2" : "byte",
                                             0, 0, 0, 0, input16 ? 16 : 8, 0),
              QStringLiteral("gmm.gen_image_interleaved"));
        check(api, api->decompose3(result.interleaved, &result.red, &result.green, &result.blue),
              QStringLiteral("gmm.decompose3"));
        check(api, api->genRectangle1(&result.fullRegion, 0, 0, image.rows - 1, image.cols - 1),
              QStringLiteral("gmm.gen_full_region"));

        Hobject sourceRed = result.red;
        Hobject sourceGreen = result.green;
        Hobject sourceBlue = result.blue;
        int validBits = 8;
        int bitShift = 0;
        if (input16) {
            validBits = sample.validBits;
            bitShift = sample.bitShift;
            check(api, api->bitRshift(result.red, &result.shiftedRed, bitShift),
                  QStringLiteral("gmm.bit_rshift.red"));
            check(api, api->bitRshift(result.green, &result.shiftedGreen, bitShift),
                  QStringLiteral("gmm.bit_rshift.green"));
            check(api, api->bitRshift(result.blue, &result.shiftedBlue, bitShift),
                  QStringLiteral("gmm.bit_rshift.blue"));
            sourceRed = result.shiftedRed;
            sourceGreen = result.shiftedGreen;
            sourceBlue = result.shiftedBlue;
        }

        const double maxCode = static_cast<double>((quint64(1) << validBits) - 1U);
        for (const auto &entry : {qMakePair(sourceRed, QStringLiteral("red")),
                                  qMakePair(sourceGreen, QStringLiteral("green")),
                                  qMakePair(sourceBlue, QStringLiteral("blue"))}) {
            double minimum = 0.0;
            double maximum = 0.0;
            double range = 0.0;
            check(api, api->minMaxGray(result.fullRegion, entry.first, 0.0,
                                        &minimum, &maximum, &range),
                  QStringLiteral("gmm.min_max_gray.%1").arg(entry.second));
            if (minimum < 0.0 || maximum > maxCode) {
                throw Failure{QStringLiteral("pixel_value_out_of_range"),
                              QStringLiteral("%1 channel range [%2,%3] exceeds [0,%4]")
                              .arg(entry.second).arg(minimum).arg(maximum).arg(maxCode), {}, H_MSG_OK};
            }
        }

        check(api, api->convertImageType(sourceRed, &result.realRed, "real"),
              QStringLiteral("gmm.convert_real.red"));
        check(api, api->convertImageType(sourceGreen, &result.realGreen, "real"),
              QStringLiteral("gmm.convert_real.green"));
        check(api, api->convertImageType(sourceBlue, &result.realBlue, "real"),
              QStringLiteral("gmm.convert_real.blue"));
        const double multiplier = 1.0 / maxCode;
        check(api, api->scaleImage(result.realRed, &result.normalizedRed, multiplier, 0.0),
              QStringLiteral("gmm.normalize.red"));
        check(api, api->scaleImage(result.realGreen, &result.normalizedGreen, multiplier, 0.0),
              QStringLiteral("gmm.normalize.green"));
        check(api, api->scaleImage(result.realBlue, &result.normalizedBlue, multiplier, 0.0),
              QStringLiteral("gmm.normalize.blue"));
        check(api, api->transFromRgb(result.normalizedRed, result.normalizedGreen, result.normalizedBlue,
                                     &result.lightness, &result.a, &result.b, "cielab"),
              QStringLiteral("gmm.trans_from_rgb.cielab"));
        if (channels == QStringLiteral("lab")) {
            check(api, api->compose3(result.lightness, result.a, result.b, &result.feature),
                  QStringLiteral("gmm.compose3.lab"));
        } else {
            check(api, api->compose2(result.a, result.b, &result.feature),
                  QStringLiteral("gmm.compose2.ab"));
        }
        check(api, api->genRectangle1(&result.roiRegion,
                                       roi.y(), roi.x(), roi.y() + roi.height() - 1,
                                       roi.x() + roi.width() - 1),
              QStringLiteral("gmm.gen_sample_roi"));
        return result;
    } catch (...) {
        clearLabImage(api, &result);
        throw;
    }
}

struct SampledRegionResult
{
    Hobject region = NO_OBJECTS;
    int pixelCount = 0;
    double rowStep = 0.0;
    double columnStep = 0.0;
};

// C++ 只计算目标网格密度；实际像素位置完全由 HALCON 网格、移动和区域相交算子决定。
SampledRegionResult sampledRegion(GmmApi *api,
                                  const Hobject roiRegion,
                                  const QRect &roi,
                                  int requestedCount)
{
    const int available = roi.width() * roi.height();
    if (!roi.isValid() || requestedCount <= 0 || requestedCount > available) {
        throw Failure{QStringLiteral("gmm_insufficient_pixels"),
                      QStringLiteral("Sampling quota exceeds ROI pixels."), {}, H_MSG_OK};
    }

    SampledRegionResult best;
    if (requestedCount == available) {
        check(api, api->moveRegion(roiRegion, &best.region, 0, 0),
              QStringLiteral("gmm.copy_full_sample_region"));
        best.pixelCount = available;
        best.rowStep = 1.0;
        best.columnStep = 1.0;
        return best;
    }

    struct AxisGrid
    {
        int count = 1;
        int step = 2;
    };
    const auto axisGrids = [](int extent) {
        QVector<AxisGrid> grids;
        int previousCount = -1;
        for (int step = 2; step <= qMax(2, extent); ++step) {
            const int count = (extent + step - 1) / step;
            if (count != previousCount) {
                grids.append({count, step});
                previousCount = count;
            }
        }
        return grids;
    };

    // gen_grid_region 在 HALCON 20.11 中按离散步长布点。先寻找不超过配额、
    // 像素数最多且纵横覆盖比例最接近 ROI 的网格，再只调用一次 HALCON 生成区域。
    const QVector<AxisGrid> rowGrids = axisGrids(roi.height());
    const QVector<AxisGrid> columnGrids = axisGrids(roi.width());
    int selectedCount = 0;
    int selectedRowStep = 2;
    int selectedColumnStep = 2;
    double selectedAspectError = std::numeric_limits<double>::max();
    const double roiAspect = static_cast<double>(roi.width()) / roi.height();
    for (const AxisGrid &rows : rowGrids) {
        for (const AxisGrid &columns : columnGrids) {
            const int count = rows.count * columns.count;
            if (count > requestedCount)
                continue;
            const double gridAspect = static_cast<double>(columns.count) / rows.count;
            const double aspectError = std::abs(std::log(gridAspect / roiAspect));
            if (count > selectedCount ||
                (count == selectedCount && aspectError < selectedAspectError)) {
                selectedCount = count;
                selectedRowStep = rows.step;
                selectedColumnStep = columns.step;
                selectedAspectError = aspectError;
            }
        }
    }

    Tuple rowStep(api), columnStep(api), type(api), width(api), height(api);
    rowStep.create(1); rowStep.integer(0, selectedRowStep);
    columnStep.create(1); columnStep.integer(0, selectedColumnStep);
    type.create(1); type.string(0, "points");
    width.create(1); width.integer(0, roi.width());
    height.create(1); height.integer(0, roi.height());

    Hobject localGrid = NO_OBJECTS;
    Hobject movedGrid = NO_OBJECTS;
    try {
        check(api, api->genGridRegion(&localGrid, rowStep.value(), columnStep.value(),
                                       type.value(), width.value(), height.value()),
              QStringLiteral("gmm.gen_grid_region"));
        check(api, api->moveRegion(localGrid, &movedGrid, roi.y(), roi.x()),
              QStringLiteral("gmm.move_grid_region"));
        check(api, api->intersection(movedGrid, roiRegion, &best.region),
              QStringLiteral("gmm.intersection_grid_roi"));
        Hlong area = 0;
        double row = 0.0;
        double column = 0.0;
        check(api, api->areaCenter(best.region, &area, &row, &column),
              QStringLiteral("gmm.area_sampled_region"));
        best.pixelCount = static_cast<int>(area);
        best.rowStep = selectedRowStep;
        best.columnStep = selectedColumnStep;
        clearObject(api, movedGrid);
        clearObject(api, localGrid);
    } catch (...) {
        clearObject(api, movedGrid);
        clearObject(api, localGrid);
        clearObject(api, best.region);
        throw;
    }

    if (!objectAllocated(best.region) || best.pixelCount <= 0) {
        clearObject(api, best.region);
        throw Failure{QStringLiteral("gmm_insufficient_pixels"),
                      QStringLiteral("HALCON grid sampling produced an empty ROI."),
                      QStringLiteral("gmm.sample_grid"), H_MSG_OK};
    }
    return best;
}

Hobject classRegionTuple(GmmApi *api, int classCount, int classIndex, Hobject selected)
{
    Hobject tuple = NO_OBJECTS;
    for (int i = 0; i < classCount; ++i) {
        Hobject part = NO_OBJECTS;
        if (i == classIndex) {
            part = selected;
            selected = NO_OBJECTS;
        } else {
            check(api, api->genEmptyRegion(&part), QStringLiteral("gmm.gen_empty_region"));
        }
        if (i == 0) {
            tuple = part;
            part = NO_OBJECTS;
        } else {
            Hobject combined = NO_OBJECTS;
            try {
                check(api, api->concatObj(tuple, part, &combined), QStringLiteral("gmm.concat_class_regions"));
            } catch (...) {
                clearObject(api, part);
                clearObject(api, tuple);
                clearObject(api, selected);
                throw;
            }
            clearObject(api, part);
            clearObject(api, tuple);
            tuple = combined;
        }
    }
    clearObject(api, selected);
    return tuple;
}

} // namespace

QString colorRecognitionGmmSamplingAlgorithmVersion()
{
    return QString::fromLatin1(kSamplingAlgorithmVersion);
}

ColorRecognitionGmmBuildResult ColorRecognitionGmmHalconBackend::buildModel(
        const QVector<ColorRecognitionGmmBuildSample> &samples,
        const ColorRecognitionGmmBuildConfig &config) const
{
    QElapsedTimer timer;
    timer.start();
    ColorRecognitionGmmBuildResult result;
    result.algorithmVersion = QString::fromLatin1(kAlgorithmVersion);
    result.featureSchemaVersion = QString::fromLatin1(kFeatureSchemaVersion);
    result.colorChannels = config.colorChannels.trimmed().toLower();

    auto fail = [&](const Failure &failure) {
        result.success = false;
        result.status = failure.status;
        result.message = failure.message;
        result.elapsedMs = timer.elapsed();
        result.artifact = {};
        result.payload.insert(QStringLiteral("stage"), failure.stage);
        result.payload.insert(QStringLiteral("halconErrorCode"), static_cast<double>(failure.halconCode));
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    };

    try {
        if (samples.isEmpty())
            throw Failure{QStringLiteral("gmm_samples_missing"), QStringLiteral("GMM samples are empty."), {}, H_MSG_OK};
        if (config.labels.size() < 2)
            throw Failure{QStringLiteral("gmm_class_sample_missing"), QStringLiteral("GMM requires at least two declared classes."), {}, H_MSG_OK};
        if (result.colorChannels != QStringLiteral("ab") && result.colorChannels != QStringLiteral("lab"))
            throw Failure{QStringLiteral("invalid_gmm_color_channels"), QStringLiteral("colorChannels must be ab or lab."), {}, H_MSG_OK};
        if (config.maxSamplesPerClass < kMinimumPixelsPerClass)
            throw Failure{QStringLiteral("invalid_gmm_sampling_limit"), QStringLiteral("maxSamplesPerClass must be at least 256."), {}, H_MSG_OK};

        QMap<int, QString> labels;
        for (const ColorRecognitionGmmLabel &label : config.labels) {
            const QString name = label.name.trimmed();
            if (name.isEmpty() || labels.contains(label.classId))
                throw Failure{QStringLiteral("invalid_gmm_labels"), QStringLiteral("GMM labels must have unique classId and non-empty names."), {}, H_MSG_OK};
            labels.insert(label.classId, name);
        }
        for (auto it = labels.cbegin(); it != labels.cend(); ++it)
            result.classIdOrder.append(it.key());

        QMap<int, QVector<PreparedSample>> preparedByClass;
        QSet<QString> sampleIds;
        QJsonArray trainingSamplesJson;
        QVector<ColorRecognitionGmmBuildSample> sortedSamples = samples;
        std::sort(sortedSamples.begin(), sortedSamples.end(), [](const auto &lhs, const auto &rhs) {
            if (lhs.classId != rhs.classId)
                return lhs.classId < rhs.classId;
            return lhs.sampleId < rhs.sampleId;
        });
        for (const ColorRecognitionGmmBuildSample &sample : sortedSamples) {
            const QString sampleId = sample.sampleId.trimmed();
            if (sampleId.isEmpty() || sampleIds.contains(sampleId))
                throw Failure{QStringLiteral("gmm_sample_image_invalid"), QStringLiteral("sampleId must be non-empty and unique."), {}, H_MSG_OK};
            sampleIds.insert(sampleId);
            if (!labels.contains(sample.classId) || labels.value(sample.classId) != sample.label.trimmed())
                throw Failure{QStringLiteral("invalid_gmm_labels"), QStringLiteral("Sample classId/label does not match declared labels."), {}, H_MSG_OK};
            if (sample.image.empty())
                throw Failure{QStringLiteral("gmm_sample_image_invalid"), QStringLiteral("Sample image is empty."), {}, H_MSG_OK};
            if (sample.image.channels() == 1)
                throw Failure{QStringLiteral("unsupported_mono_for_color_recognition"), QStringLiteral("Mono sample has no color information."), {}, H_MSG_OK};
            if (sample.image.channels() != 3 && sample.image.channels() != 4)
                throw Failure{QStringLiteral("unsupported_image_type"), QStringLiteral("GMM samples require three or four channels."), {}, H_MSG_OK};
            if (sample.image.depth() != CV_8U && sample.image.depth() != CV_16U)
                throw Failure{QStringLiteral("unsupported_image_type"), QStringLiteral("GMM samples require unsigned 8-bit or 16-bit input."), {}, H_MSG_OK};
            const QString format = normalizedPixelFormat(sample);
            const QStringList validFormats = sample.image.depth() == CV_8U
                    ? (sample.image.channels() == 3
                       ? QStringList{QStringLiteral("BGR8"), QStringLiteral("RGB8")}
                       : QStringList{QStringLiteral("BGRA8"), QStringLiteral("RGBA8")})
                    : (sample.image.channels() == 3
                       ? QStringList{QStringLiteral("BGR16"), QStringLiteral("RGB16")}
                       : QStringList{QStringLiteral("BGRA16"), QStringLiteral("RGBA16")});
            if (!validFormats.contains(format))
                throw Failure{QStringLiteral("invalid_pixel_format_metadata"),
                              QStringLiteral("pixelFormat does not match image depth/channel count."),
                              {}, H_MSG_OK};
            if (sample.image.depth() == CV_16U && (sample.validBits < 0 || sample.bitShift < 0))
                throw Failure{QStringLiteral("missing_pixel_format_metadata"), QStringLiteral("16-bit sample requires validBits and bitShift."), {}, H_MSG_OK};
            if (sample.image.depth() == CV_16U &&
                (sample.validBits < 1 || sample.validBits > 16 || sample.bitShift < 0 ||
                 sample.bitShift > 15 || sample.validBits + sample.bitShift > 16)) {
                throw Failure{QStringLiteral("invalid_pixel_format_metadata"), QStringLiteral("Invalid validBits/bitShift contract."), {}, H_MSG_OK};
            }
            const QString actualHash = canonicalImageHash(sample.image);
            if (sample.imageSha256.trimmed() != actualHash)
                throw Failure{QStringLiteral("gmm_sample_hash_mismatch"), QStringLiteral("Sample image hash mismatch: %1").arg(sampleId), {}, H_MSG_OK};
            const QRect roi = normalizedRoi(sample.roiNormalized, sample.image.cols, sample.image.rows);
            if (!roi.isValid())
                throw Failure{QStringLiteral("invalid_roi"), QStringLiteral("Sample ROI is invalid: %1").arg(sampleId), {}, H_MSG_OK};
            preparedByClass[sample.classId].append(PreparedSample{&sample, roi, 0});

            QJsonObject sampleJson;
            sampleJson.insert(QStringLiteral("sampleId"), sampleId);
            sampleJson.insert(QStringLiteral("classId"), sample.classId);
            sampleJson.insert(QStringLiteral("label"), sample.label.trimmed());
            sampleJson.insert(QStringLiteral("imageSha256"), actualHash);
            sampleJson.insert(QStringLiteral("pixelFormat"), format);
            sampleJson.insert(QStringLiteral("validBits"), sample.image.depth() == CV_8U ? 8 : sample.validBits);
            sampleJson.insert(QStringLiteral("bitShift"), sample.image.depth() == CV_8U ? 0 : sample.bitShift);
            sampleJson.insert(QStringLiteral("roiX"), sample.roiNormalized.x());
            sampleJson.insert(QStringLiteral("roiY"), sample.roiNormalized.y());
            sampleJson.insert(QStringLiteral("roiWidth"), sample.roiNormalized.width());
            sampleJson.insert(QStringLiteral("roiHeight"), sample.roiNormalized.height());
            trainingSamplesJson.append(sampleJson);
        }

        qint64 targetPixels = config.maxSamplesPerClass;
        for (int classId : result.classIdOrder) {
            const QVector<PreparedSample> classSamples = preparedByClass.value(classId);
            if (classSamples.isEmpty())
                throw Failure{QStringLiteral("gmm_class_sample_missing"), QStringLiteral("Declared class %1 has no sample.").arg(classId), {}, H_MSG_OK};
            qint64 available = 0;
            for (const PreparedSample &sample : classSamples)
                available += static_cast<qint64>(sample.roi.width()) * sample.roi.height();
            if (available < kMinimumPixelsPerClass)
                throw Failure{QStringLiteral("gmm_insufficient_pixels"), QStringLiteral("Class %1 has fewer than 256 pixels.").arg(classId), {}, H_MSG_OK};
            targetPixels = qMin(targetPixels, available);
        }

        QVector<int> centers;
        for (int classId : result.classIdOrder) {
            QVector<PreparedSample> &classSamples = preparedByClass[classId];
            const QVector<int> quotas = balancedQuotas(classSamples, static_cast<int>(targetPixels));
            qint64 available = 0;
            for (int i = 0; i < classSamples.size(); ++i) {
                available += static_cast<qint64>(classSamples[i].roi.width()) * classSamples[i].roi.height();
                classSamples[i].quota = quotas.at(i);
            }
            const int roiCount = classSamples.size();
            const int maxCenters = roiCount >= 10 ? 3 : (roiCount >= 5 ? 2 : 1);
            centers.append(1);
            centers.append(maxCenters);
            ColorRecognitionGmmClassDiagnostics diagnostics;
            diagnostics.classId = classId;
            diagnostics.label = labels.value(classId);
            diagnostics.roiCount = roiCount;
            diagnostics.availablePixels = available;
            diagnostics.requestedTrainingPixels = static_cast<int>(targetPixels);
            diagnostics.trainingPixels = 0;
            diagnostics.maxCenters = maxCenters;
            result.classes.append(diagnostics);

        }

        QJsonObject trainingContract;
        trainingContract.insert(QStringLiteral("samples"), trainingSamplesJson);
        result.trainingDataHash = sha256(canonicalJson(trainingContract));
        QJsonObject buildContractJson;
        QJsonArray roiSamplingDiagnostics;

        QString loadError;
        const std::shared_ptr<GmmLibrary> library = acquireLibrary(config.halconSoPath, &loadError);
        if (!library) {
            const bool missingSymbol = loadError.contains(QStringLiteral("Missing HALCON symbol"));
            throw Failure{missingSymbol ? QStringLiteral("halcon_symbol_profile_missing")
                                        : QStringLiteral("halcon_runtime_not_found"),
                          loadError, QStringLiteral("gmm.acquire_runtime"), H_MSG_OK};
        }
        GmmApi *api = &library->api;

        Tuple numDim(api), numClasses(api), numCenters(api), covar(api), preprocessing(api);
        Tuple numComponents(api), randSeed(api), gmmHandle(api);
        numDim.create(1); numDim.integer(0, result.colorChannels == QStringLiteral("lab") ? 3 : 2);
        numClasses.create(1); numClasses.integer(0, result.classIdOrder.size());
        numCenters.create(centers.size());
        for (int i = 0; i < centers.size(); ++i) numCenters.integer(i, centers.at(i));
        covar.create(1); covar.string(0, "full");
        preprocessing.create(1); preprocessing.string(0, "none");
        numComponents.create(1); numComponents.integer(0, 0);
        randSeed.create(1); randSeed.integer(0, 42);
        check(api, api->createClassGmm(numDim.value(), numClasses.value(), numCenters.value(),
                                       covar.value(), preprocessing.value(), numComponents.value(),
                                       randSeed.value(), gmmHandle.ptr()),
              QStringLiteral("gmm.create_class_gmm"));

        auto clearGmm = [&]() {
            if (gmmHandle.size() > 0)
                api->clearClassGmm(gmmHandle.value());
        };
        try {
            Tuple randomize(api);
            randomize.create(1);
            randomize.real(0, 0.0);
            QVector<int> actualTrainingPixels(result.classIdOrder.size(), 0);
            for (int classIndex = 0; classIndex < result.classIdOrder.size(); ++classIndex) {
                const int classId = result.classIdOrder.at(classIndex);
                for (const PreparedSample &prepared : preparedByClass.value(classId)) {
                    HalconLabImage lab = makeLabImage(api, *prepared.source, prepared.roi, result.colorChannels);
                    try {
                        const SampledRegionResult sampled = sampledRegion(
                                    api, lab.roiRegion, prepared.roi, prepared.quota);
                        actualTrainingPixels[classIndex] += sampled.pixelCount;
                        QJsonObject samplingItem;
                        samplingItem.insert(QStringLiteral("sampleId"),
                                            prepared.source->sampleId.trimmed());
                        samplingItem.insert(QStringLiteral("classId"), classId);
                        samplingItem.insert(QStringLiteral("requestedPixels"), prepared.quota);
                        samplingItem.insert(QStringLiteral("actualPixels"), sampled.pixelCount);
                        samplingItem.insert(QStringLiteral("rowStep"), sampled.rowStep);
                        samplingItem.insert(QStringLiteral("columnStep"), sampled.columnStep);
                        roiSamplingDiagnostics.append(samplingItem);
                        Hobject regions = classRegionTuple(api, result.classIdOrder.size(),
                                                           classIndex, sampled.region);
                        try {
                            check(api, api->addSamplesImageClassGmm(lab.feature, regions,
                                                                    gmmHandle.value(), randomize.value()),
                                  QStringLiteral("gmm.add_samples_image_class_gmm"));
                        } catch (...) {
                            clearObject(api, regions);
                            throw;
                        }
                        clearObject(api, regions);
                    } catch (...) {
                        clearLabImage(api, &lab);
                        throw;
                    }
                    clearLabImage(api, &lab);
                }
            }

            for (int classIndex = 0; classIndex < result.classes.size(); ++classIndex) {
                result.classes[classIndex].trainingPixels = actualTrainingPixels.at(classIndex);
                const int minimumAccepted = qMin(
                    kMinimumPixelsPerClass,
                    static_cast<int>(std::floor(
                        result.classes.at(classIndex).requestedTrainingPixels * 0.80)));
                if (result.classes.at(classIndex).trainingPixels < minimumAccepted) {
                    throw Failure{QStringLiteral("gmm_insufficient_pixels"),
                                  QStringLiteral("HALCON grid sampling produced too few pixels for class %1 (%2/%3).")
                                  .arg(result.classes.at(classIndex).classId)
                                  .arg(result.classes.at(classIndex).trainingPixels)
                                  .arg(result.classes.at(classIndex).requestedTrainingPixels),
                                  QStringLiteral("gmm.sample_grid"), H_MSG_OK};
                }
            }
            buildContractJson = buildContract(result.colorChannels,
                                               result.classIdOrder,
                                               result.classes,
                                               config.maxSamplesPerClass,
                                               result.trainingDataHash);
            result.buildParamsHash = sha256(canonicalJson(buildContractJson));

            Tuple maxIter(api), threshold(api), priors(api), regularize(api), trainedCenters(api), iterations(api);
            maxIter.create(1); maxIter.integer(0, 100);
            threshold.create(1); threshold.real(0, 1e-4);
            priors.create(1); priors.string(0, "uniform");
            regularize.create(1); regularize.real(0, 1e-4);
            check(api, api->trainClassGmm(gmmHandle.value(), maxIter.value(), threshold.value(),
                                           priors.value(), regularize.value(),
                                           trainedCenters.ptr(), iterations.ptr()),
                  QStringLiteral("gmm.train_class_gmm"));

            Tuple serializedHandle(api);
            check(api, api->serializeClassGmm(gmmHandle.value(), serializedHandle.ptr()),
                  QStringLiteral("gmm.serialize_class_gmm"));
            try {
                Tuple pointer(api), size(api);
                check(api, api->getSerializedItemPtr(serializedHandle.value(), pointer.ptr(), size.ptr()),
                      QStringLiteral("gmm.get_serialized_item_ptr"));
                const qint64 byteCount = size.size() > 0 ? static_cast<qint64>(size.integerAt(0)) : 0;
                if (byteCount <= 0)
                    throw Failure{QStringLiteral("model_artifact_invalid"), QStringLiteral("Serialized GMM is empty."), {}, H_MSG_OK};
                if (byteCount > kMaxSerializedBytes)
                    throw Failure{QStringLiteral("model_artifact_too_large"), QStringLiteral("Serialized GMM exceeds 8 MiB."), {}, H_MSG_OK};
                const Hlong address = pointer.size() > 0 ? pointer.integerAt(0) : 0;
                if (address == 0)
                    throw Failure{QStringLiteral("model_artifact_invalid"), QStringLiteral("Serialized GMM pointer is null."), {}, H_MSG_OK};
                result.artifact.serializedBytes = QByteArray(
                            reinterpret_cast<const char *>(static_cast<quintptr>(address)),
                            static_cast<int>(byteCount));
                result.artifact.serializedSize = byteCount;
                result.artifact.serializedSha256 = sha256(result.artifact.serializedBytes);
                result.artifact.serializedGmmBase64 = QString::fromLatin1(result.artifact.serializedBytes.toBase64());
                if (QByteArray::fromBase64(result.artifact.serializedGmmBase64.toLatin1()) !=
                    result.artifact.serializedBytes) {
                    throw Failure{QStringLiteral("model_artifact_invalid"), QStringLiteral("Serialized GMM Base64 round trip failed."), {}, H_MSG_OK};
                }

                Tuple sourcePointer(api), sourceSize(api), copy(api), validationSerialized(api), validationGmm(api);
                sourcePointer.create(1);
                sourcePointer.integer(0, reinterpret_cast<Hlong>(result.artifact.serializedBytes.constData()));
                sourceSize.create(1);
                sourceSize.integer(0, byteCount);
                copy.create(1);
                copy.string(0, "true");
                check(api, api->createSerializedItemPtr(sourcePointer.value(), sourceSize.value(), copy.value(),
                                                        validationSerialized.ptr()),
                      QStringLiteral("gmm.create_serialized_item_ptr"));
                try {
                    check(api, api->deserializeClassGmm(validationSerialized.value(), validationGmm.ptr()),
                          QStringLiteral("gmm.deserialize_class_gmm"));
                    if (validationGmm.size() > 0)
                        api->clearClassGmm(validationGmm.value());
                } catch (...) {
                    if (validationGmm.size() > 0)
                        api->clearClassGmm(validationGmm.value());
                    api->clearSerializedItem(validationSerialized.value());
                    throw;
                }
                api->clearSerializedItem(validationSerialized.value());
            } catch (...) {
                api->clearSerializedItem(serializedHandle.value());
                throw;
            }
            api->clearSerializedItem(serializedHandle.value());
        } catch (...) {
            clearGmm();
            throw;
        }
        clearGmm();

        result.success = true;
        result.status = QStringLiteral("ok");
        result.state = std::any_of(result.classes.cbegin(), result.classes.cend(), [](const auto &item) {
            return item.roiCount < 3;
        }) ? QStringLiteral("ReadyWithWarning") : QStringLiteral("Ready");
        result.message = result.state == QStringLiteral("Ready")
                ? QStringLiteral("GMM model built")
                : QStringLiteral("GMM model built with limited independent ROI samples");
        result.elapsedMs = timer.elapsed();
        result.payload = buildContractJson;
        result.payload.insert(QStringLiteral("state"), result.state);
        result.payload.insert(QStringLiteral("buildParamsHash"), result.buildParamsHash);
        result.payload.insert(QStringLiteral("serializedSize"), static_cast<double>(result.artifact.serializedSize));
        result.payload.insert(QStringLiteral("serializedSha256"), result.artifact.serializedSha256);
        result.payload.insert(QStringLiteral("roiSamplingDiagnostics"), roiSamplingDiagnostics);
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const Failure &failure) {
        return fail(failure);
    } catch (const std::exception &exception) {
        return fail(Failure{QStringLiteral("gmm_training_failed"),
                            QString::fromLocal8Bit(exception.what()), QStringLiteral("gmm.exception"), H_MSG_OK});
    } catch (...) {
        return fail(Failure{QStringLiteral("gmm_training_failed"),
                            QStringLiteral("Unknown GMM build failure."), QStringLiteral("gmm.exception"), H_MSG_OK});
    }
}

ColorRecognitionGmmArtifactValidationResult ColorRecognitionGmmHalconBackend::validateArtifact(
        const ColorRecognitionGmmArtifact &artifact,
        const ColorRecognitionGmmBuildConfig &config) const
{
    QElapsedTimer timer;
    timer.start();
    ColorRecognitionGmmArtifactValidationResult result;

    auto fail = [&](const Failure &failure) {
        result.success = false;
        result.status = failure.status;
        result.message = failure.message;
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("stage"), failure.stage);
        result.payload.insert(QStringLiteral("halconErrorCode"), static_cast<double>(failure.halconCode));
        return result;
    };

    try {
        if (artifact.serializedGmmBase64.trimmed().isEmpty())
            throw Failure{QStringLiteral("model_artifact_invalid"),
                          QStringLiteral("Serialized GMM Base64 is empty."),
                          QStringLiteral("gmm.validate.base64"), H_MSG_OK};
        const QByteArray decoded = QByteArray::fromBase64(
                    artifact.serializedGmmBase64.toLatin1(),
                    QByteArray::AbortOnBase64DecodingErrors);
        if (decoded.isEmpty() || decoded.size() > kMaxSerializedBytes)
            throw Failure{decoded.size() > kMaxSerializedBytes
                          ? QStringLiteral("model_artifact_too_large")
                          : QStringLiteral("model_artifact_invalid"),
                          QStringLiteral("Serialized GMM Base64 cannot be decoded or exceeds 8 MiB."),
                          QStringLiteral("gmm.validate.base64"), H_MSG_OK};
        if (artifact.serializedSize != decoded.size())
            throw Failure{QStringLiteral("model_artifact_invalid"),
                          QStringLiteral("Serialized GMM size mismatch."),
                          QStringLiteral("gmm.validate.size"), H_MSG_OK};
        const QString actualHash = sha256(decoded);
        if (artifact.serializedSha256.trimmed() != actualHash)
            throw Failure{QStringLiteral("model_artifact_invalid"),
                          QStringLiteral("Serialized GMM SHA-256 mismatch."),
                          QStringLiteral("gmm.validate.sha256"), H_MSG_OK};
        if (!artifact.serializedBytes.isEmpty() && artifact.serializedBytes != decoded)
            throw Failure{QStringLiteral("model_artifact_invalid"),
                          QStringLiteral("Serialized GMM byte cache differs from Base64."),
                          QStringLiteral("gmm.validate.byte_cache"), H_MSG_OK};

        QString loadError;
        const std::shared_ptr<GmmLibrary> library = acquireLibrary(config.halconSoPath, &loadError);
        if (!library) {
            const bool missingSymbol = loadError.contains(QStringLiteral("Missing HALCON symbol"));
            throw Failure{missingSymbol ? QStringLiteral("halcon_symbol_profile_missing")
                                        : QStringLiteral("halcon_runtime_not_found"),
                          loadError, QStringLiteral("gmm.validate.acquire_runtime"), H_MSG_OK};
        }
        GmmApi *api = &library->api;
        Tuple pointer(api), size(api), copy(api), serializedHandle(api), gmmHandle(api);
        pointer.create(1);
        pointer.integer(0, reinterpret_cast<Hlong>(decoded.constData()));
        size.create(1);
        size.integer(0, decoded.size());
        copy.create(1);
        copy.string(0, "true");
        check(api, api->createSerializedItemPtr(pointer.value(), size.value(), copy.value(),
                                                serializedHandle.ptr()),
              QStringLiteral("gmm.validate.create_serialized_item_ptr"));
        try {
            check(api, api->deserializeClassGmm(serializedHandle.value(), gmmHandle.ptr()),
                  QStringLiteral("gmm.validate.deserialize_class_gmm"));
            if (gmmHandle.size() > 0)
                api->clearClassGmm(gmmHandle.value());
        } catch (...) {
            if (gmmHandle.size() > 0)
                api->clearClassGmm(gmmHandle.value());
            api->clearSerializedItem(serializedHandle.value());
            throw;
        }
        api->clearSerializedItem(serializedHandle.value());

        result.success = true;
        result.status = QStringLiteral("ok");
        result.message = QStringLiteral("GMM artifact validated");
        result.serializedSize = decoded.size();
        result.serializedSha256 = actualHash;
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("serializedSize"), static_cast<double>(decoded.size()));
        result.payload.insert(QStringLiteral("serializedSha256"), actualHash);
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const Failure &failure) {
        if (failure.status == QStringLiteral("gmm_training_failed")) {
            Failure artifactFailure = failure;
            artifactFailure.status = QStringLiteral("model_artifact_invalid");
            return fail(artifactFailure);
        }
        return fail(failure);
    } catch (const std::exception &exception) {
        return fail(Failure{QStringLiteral("model_artifact_invalid"),
                            QString::fromLocal8Bit(exception.what()),
                            QStringLiteral("gmm.validate.exception"), H_MSG_OK});
    } catch (...) {
        return fail(Failure{QStringLiteral("model_artifact_invalid"),
                            QStringLiteral("Unknown GMM artifact validation failure."),
                            QStringLiteral("gmm.validate.exception"), H_MSG_OK});
    }
}

ColorRecognitionGmmRunResult ColorRecognitionGmmHalconBackend::runModel(
        const cv::Mat &image,
        const ColorRecognitionGmmRunConfig &config) const
{
    QElapsedTimer timer;
    timer.start();
    ColorRecognitionGmmRunResult result;

    auto fail = [&](const Failure &failure) {
        result.success = false;
        result.ok = false;
        result.measurementValid = false;
        result.status = failure.status;
        result.message = failure.message;
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("backend"), QStringLiteral("cielab_gmm"));
        result.payload.insert(QStringLiteral("modelState"), config.modelState);
        result.payload.insert(QStringLiteral("stage"), failure.stage);
        result.payload.insert(QStringLiteral("halconErrorCode"), static_cast<double>(failure.halconCode));
        result.payload.insert(QStringLiteral("declaredPixelFormat"), config.pixelFormat);
        result.payload.insert(QStringLiteral("actualMatDepth"), image.empty() ? -1 : image.depth());
        result.payload.insert(QStringLiteral("actualChannelCount"), image.empty() ? 0 : image.channels());
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    };

    try {
        if (image.empty())
            throw Failure{QStringLiteral("empty_image"), QStringLiteral("Input image is empty."), QStringLiteral("gmm.run.input"), H_MSG_OK};
        if (image.channels() == 1)
            throw Failure{QStringLiteral("unsupported_mono_for_color_recognition"), QStringLiteral("GMM color recognition requires RGB input."), QStringLiteral("gmm.run.input"), H_MSG_OK};
        if ((image.channels() != 3 && image.channels() != 4) ||
            (image.depth() != CV_8U && image.depth() != CV_16U)) {
            throw Failure{QStringLiteral("unsupported_image_type"), QStringLiteral("GMM input must be 8/16-bit 3/4-channel color."), QStringLiteral("gmm.run.input"), H_MSG_OK};
        }
        ColorRecognitionGmmBuildSample input;
        input.image = image;
        input.pixelFormat = config.pixelFormat;
        input.validBits = config.validBits;
        input.bitShift = config.bitShift;
        const QString format = normalizedPixelFormat(input);
        const QStringList validFormats = image.depth() == CV_8U
                ? (image.channels() == 3 ? QStringList{QStringLiteral("BGR8"), QStringLiteral("RGB8")}
                                         : QStringList{QStringLiteral("BGRA8"), QStringLiteral("RGBA8")})
                : (image.channels() == 3 ? QStringList{QStringLiteral("BGR16"), QStringLiteral("RGB16")}
                                         : QStringList{QStringLiteral("BGRA16"), QStringLiteral("RGBA16")});
        if (!validFormats.contains(format))
            throw Failure{QStringLiteral("invalid_pixel_format_metadata"), QStringLiteral("pixelFormat does not match input depth/channel count."), QStringLiteral("gmm.run.input"), H_MSG_OK};
        if (image.depth() == CV_16U && (config.validBits < 1 || config.validBits > 16 ||
                                        config.bitShift < 0 || config.bitShift > 15 ||
                                        config.validBits + config.bitShift > 16)) {
            throw Failure{config.validBits < 0 || config.bitShift < 0
                          ? QStringLiteral("missing_pixel_format_metadata")
                          : QStringLiteral("invalid_pixel_format_metadata"),
                          QStringLiteral("16-bit input requires a valid validBits/bitShift contract."),
                          QStringLiteral("gmm.run.input"), H_MSG_OK};
        }
        if (config.enablePositionCorrection)
            throw Failure{QStringLiteral("unsupported_position_correction"), QStringLiteral("GMM position correction is not implemented."), QStringLiteral("gmm.run.position_correction"), H_MSG_OK};

        const QRect roi = normalizedRoi(config.roiNormalized, image.cols, image.rows);
        if (roi.isEmpty())
            throw Failure{QStringLiteral("invalid_roi"), QStringLiteral("Detection ROI is invalid."), QStringLiteral("gmm.run.roi"), H_MSG_OK};
        const QString state = config.modelState.trimmed().toLower();
        if (state != QStringLiteral("ready") && state != QStringLiteral("ready_with_warning") &&
            state != QStringLiteral("readywithwarning")) {
            QString status = QStringLiteral("gmm_model_empty");
            if (state == QStringLiteral("stale")) status = QStringLiteral("gmm_model_stale");
            else if (state == QStringLiteral("invalid")) status = QStringLiteral("gmm_model_invalid");
            else if (state == QStringLiteral("failed")) status = QStringLiteral("gmm_model_failed");
            throw Failure{status,
                          QStringLiteral("GMM model is not ready: %1").arg(config.modelState),
                          QStringLiteral("gmm.run.model_state"), H_MSG_OK};
        }
        const QString channels = config.colorChannels.trimmed().toLower();
        if (channels != QStringLiteral("ab") && channels != QStringLiteral("lab"))
            throw Failure{QStringLiteral("invalid_model_signature"), QStringLiteral("Unsupported GMM color channels."), QStringLiteral("gmm.run.signature"), H_MSG_OK};
        if (config.algorithmVersion != QString::fromLatin1(kAlgorithmVersion) ||
            config.featureSchemaVersion != QString::fromLatin1(kFeatureSchemaVersion)) {
            throw Failure{QStringLiteral("invalid_model_signature"), QStringLiteral("GMM algorithm or feature schema version mismatch."), QStringLiteral("gmm.run.signature"), H_MSG_OK};
        }
        if (config.classIdOrder.isEmpty() || config.classIdOrder.size() != config.classes.size())
            throw Failure{QStringLiteral("invalid_gmm_class_order"), QStringLiteral("GMM classIdOrder is empty or differs from diagnostics."), QStringLiteral("gmm.run.class_order"), H_MSG_OK};
        QHash<int, QString> labelById;
        for (const ColorRecognitionGmmLabel &label : config.labels) {
            if (labelById.contains(label.classId) || label.name.trimmed().isEmpty())
                throw Failure{QStringLiteral("invalid_gmm_labels"), QStringLiteral("GMM labels must have unique classId and non-empty names."), QStringLiteral("gmm.run.labels"), H_MSG_OK};
            labelById.insert(label.classId, label.name.trimmed());
        }
        QSet<int> classIds;
        for (int index = 0; index < config.classIdOrder.size(); ++index) {
            const int classId = config.classIdOrder.at(index);
            const ColorRecognitionGmmClassDiagnostics &diagnostics = config.classes.at(index);
            if (classIds.contains(classId) || diagnostics.classId != classId ||
                !labelById.contains(classId) || labelById.value(classId) != diagnostics.label.trimmed()) {
                throw Failure{QStringLiteral("invalid_gmm_class_order"), QStringLiteral("classIdOrder, labels and diagnostics do not match."), QStringLiteral("gmm.run.class_order"), H_MSG_OK};
            }
            classIds.insert(classId);
        }
        const QJsonObject contract = buildContract(channels, config.classIdOrder, config.classes,
                                                   config.maxSamplesPerClass, config.trainingDataHash);
        if (config.buildParamsHash.trimmed() != sha256(canonicalJson(contract)))
            throw Failure{QStringLiteral("model_signature_mismatch"), QStringLiteral("GMM build parameter signature mismatch."), QStringLiteral("gmm.run.build_params_hash"), H_MSG_OK};
        if (!std::isfinite(config.gmmRejectionThreshold) || config.gmmRejectionThreshold < 0.0 || config.gmmRejectionThreshold > 1.0 ||
            config.minScore < 0 || config.minScore > 100 || config.minCategoryConfidence < 0 || config.minCategoryConfidence > 100 ||
            config.minClassifiedCoverage < 0 || config.minClassifiedCoverage > 100) {
            throw Failure{QStringLiteral("invalid_gmm_detection_threshold"), QStringLiteral("GMM rejection or judgment threshold is out of range."), QStringLiteral("gmm.run.threshold"), H_MSG_OK};
        }
        const QString judgeMode = config.judgeMode.trimmed().toLower();
        if (judgeMode != QStringLiteral("min_score") && judgeMode != QStringLiteral("expected_class"))
            throw Failure{QStringLiteral("invalid_judge_mode"), QStringLiteral("Unsupported GMM judge mode."), QStringLiteral("gmm.run.judge_mode"), H_MSG_OK};
        int expectedClassId = config.expectedClassId;
        if (judgeMode == QStringLiteral("expected_class") && !classIds.contains(expectedClassId)) {
            QVector<int> matches;
            if (expectedClassId < 0 && !config.expectedLabel.trimmed().isEmpty()) {
                for (auto it = labelById.cbegin(); it != labelById.cend(); ++it)
                    if (it.value() == config.expectedLabel.trimmed()) matches.append(it.key());
            }
            if (matches.size() == 1) expectedClassId = matches.first();
            else throw Failure{QStringLiteral("invalid_expected_class"), QStringLiteral("Expected GMM class cannot be resolved uniquely."), QStringLiteral("gmm.run.expected_class"), H_MSG_OK};
        }

        const QByteArray decoded = QByteArray::fromBase64(config.artifact.serializedGmmBase64.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        if (decoded.size() > kMaxSerializedBytes)
            throw Failure{QStringLiteral("model_artifact_too_large"), QStringLiteral("Serialized GMM exceeds 8 MiB."), QStringLiteral("gmm.run.artifact"), H_MSG_OK};
        if (decoded.isEmpty() || decoded.size() != config.artifact.serializedSize ||
            sha256(decoded) != config.artifact.serializedSha256 ||
            (!config.artifact.serializedBytes.isEmpty() && config.artifact.serializedBytes != decoded)) {
            throw Failure{QStringLiteral("model_artifact_invalid"), QStringLiteral("Serialized GMM Base64, size or SHA-256 is invalid."), QStringLiteral("gmm.run.artifact"), H_MSG_OK};
        }

        QString loadError;
        const std::shared_ptr<GmmLibrary> library = acquireLibrary(config.halconSoPath, &loadError);
        if (!library) {
            throw Failure{loadError.contains(QStringLiteral("Missing HALCON symbol"))
                          ? QStringLiteral("halcon_symbol_profile_missing")
                          : QStringLiteral("halcon_runtime_not_found"),
                          loadError, QStringLiteral("gmm.run.acquire_runtime"), H_MSG_OK};
        }
        GmmApi *api = &library->api;
        HalconLabImage lab = makeLabImage(api, input, QRect(0, 0, image.cols, image.rows), channels);
        Hobject roiRegion = NO_OBJECTS, maskRegion = NO_OBJECTS, effectiveRegion = NO_OBJECTS;
        Hobject reduced = NO_OBJECTS, classRegions = NO_OBJECTS;
        Tuple serializedPointer(api), serializedSize(api), copy(api), serializedHandle(api), gmmHandle(api);
        try {
            const QString regionType = config.detectRegionType.trimmed().toLower();
            if (regionType == QStringLiteral("circle")) {
                const double radius = config.detectCircleRadiusNormalized * qMax(image.cols, image.rows);
                if (!std::isfinite(radius) || radius <= 0.0)
                    throw Failure{QStringLiteral("invalid_roi"), QStringLiteral("Circle ROI is invalid."), QStringLiteral("gmm.run.circle_roi"), H_MSG_OK};
                checkRun(api, api->genCircle(&roiRegion,
                                              qBound(0.0, config.detectCircleCenterNormalized.y() * image.rows, double(image.rows - 1)),
                                              qBound(0.0, config.detectCircleCenterNormalized.x() * image.cols, double(image.cols - 1)), radius),
                         QStringLiteral("gmm.run.gen_circle"));
            } else {
                checkRun(api, api->genRectangle1(&roiRegion, roi.y(), roi.x(),
                                                  roi.y() + roi.height() - 1, roi.x() + roi.width() - 1),
                         QStringLiteral("gmm.run.gen_rectangle1"));
            }
            effectiveRegion = roiRegion;
            if (config.detectMaskPolygonNormalized.size() >= 3) {
                Tuple rows(api), columns(api);
                rows.create(config.detectMaskPolygonNormalized.size());
                columns.create(config.detectMaskPolygonNormalized.size());
                for (int i = 0; i < config.detectMaskPolygonNormalized.size(); ++i) {
                    const QPointF point = config.detectMaskPolygonNormalized.at(i);
                    rows.real(i, qBound(0.0, point.y() * image.rows, double(image.rows - 1)));
                    columns.real(i, qBound(0.0, point.x() * image.cols, double(image.cols - 1)));
                }
                checkRun(api, api->genRegionPolygonFilled(&maskRegion, rows.value(), columns.value()),
                         QStringLiteral("gmm.run.gen_mask"));
                checkRun(api, api->difference(roiRegion, maskRegion, &effectiveRegion),
                         QStringLiteral("gmm.run.difference_mask"));
            }
            Hlong effectiveArea = 0; double centerRow = 0.0, centerColumn = 0.0;
            checkRun(api, api->areaCenter(effectiveRegion, &effectiveArea, &centerRow, &centerColumn),
                     QStringLiteral("gmm.run.area_effective"));
            if (effectiveArea <= 0)
                throw Failure{QStringLiteral("masked_roi_empty"), QStringLiteral("Detection ROI is fully masked."), QStringLiteral("gmm.run.effective_region"), H_MSG_OK};
            checkRun(api, api->reduceDomain(lab.feature, effectiveRegion, &reduced), QStringLiteral("gmm.run.reduce_domain"));

            serializedPointer.create(1); serializedPointer.integer(0, reinterpret_cast<Hlong>(decoded.constData()));
            serializedSize.create(1); serializedSize.integer(0, decoded.size());
            copy.create(1); copy.string(0, "true");
            checkRun(api, api->createSerializedItemPtr(serializedPointer.value(), serializedSize.value(), copy.value(), serializedHandle.ptr()),
                     QStringLiteral("gmm.run.create_serialized_item"));
            checkRun(api, api->deserializeClassGmm(serializedHandle.value(), gmmHandle.ptr()),
                     QStringLiteral("gmm.run.deserialize_class_gmm"));
            Tuple rejectionThreshold(api);
            rejectionThreshold.create(1);
            rejectionThreshold.real(0, config.gmmRejectionThreshold);
            checkRun(api, api->classifyImageClassGmm(reduced, &classRegions, gmmHandle.value(), rejectionThreshold.value()),
                     QStringLiteral("gmm.run.classify_image_class_gmm"));

            qint64 classifiedArea = 0;
            qint64 maximumArea = -1;
            QVector<int> tiedIds;
            for (int index = 0; index < config.classIdOrder.size(); ++index) {
                Hobject selected = NO_OBJECTS, inside = NO_OBJECTS;
                try {
                    checkRun(api, api->selectObj(classRegions, &selected, index + 1), QStringLiteral("gmm.run.select_class"));
                    checkRun(api, api->intersection(selected, effectiveRegion, &inside), QStringLiteral("gmm.run.intersection_class"));
                    Hlong area = 0; double row = 0.0, column = 0.0;
                    checkRun(api, api->areaCenter(inside, &area, &row, &column), QStringLiteral("gmm.run.area_class"));
                    ColorRecognitionGmmClassMeasurement measurement;
                    measurement.classId = config.classIdOrder.at(index);
                    measurement.label = labelById.value(measurement.classId);
                    measurement.classIndex = index;
                    measurement.area = static_cast<double>(area);
                    measurement.ratio = qBound(0.0, static_cast<double>(area) / effectiveArea, 1.0);
                    result.classes.append(measurement);
                    classifiedArea += area;
                    if (area > maximumArea) { maximumArea = area; tiedIds = {measurement.classId}; }
                    else if (area == maximumArea) tiedIds.append(measurement.classId);
                } catch (...) { clearObject(api, inside); clearObject(api, selected); throw; }
                clearObject(api, inside); clearObject(api, selected);
            }
            classifiedArea = qBound<qint64>(0, classifiedArea, effectiveArea);
            result.effectiveArea = effectiveArea;
            result.classifiedArea = classifiedArea;
            result.classifiedCoverage = qBound(0.0, static_cast<double>(classifiedArea) / effectiveArea, 1.0);
            if (classifiedArea > 0 && maximumArea > 0) {
                int bestIndex = 0;
                for (int i = 1; i < result.classes.size(); ++i)
                    if (result.classes.at(i).area > result.classes.at(bestIndex).area) bestIndex = i;
                const ColorRecognitionGmmClassMeasurement &best = result.classes.at(bestIndex);
                result.predictedClassId = best.classId;
                result.predictedClassIndex = best.classIndex;
                result.predictedLabel = best.label;
                result.score = best.ratio * 100.0;
                result.rating = result.score;
                result.predictedClassRatio = best.ratio;
                result.categoryConfidence = qBound(0.0, best.area / classifiedArea, 1.0);
            }
            result.unclassifiedArea = qMax(0.0, result.effectiveArea - result.classifiedArea);
            result.unclassifiedRatio = qBound(0.0, 1.0 - result.classifiedCoverage, 1.0);
            if (result.predictedClassId < 0) result.failureReasons.append(QStringLiteral("no_accepted_class"));
            if (judgeMode == QStringLiteral("expected_class") && result.predictedClassId != expectedClassId)
                result.failureReasons.append(QStringLiteral("expected_class_mismatch"));
            if (result.score < config.minScore) result.failureReasons.append(QStringLiteral("score_below_minimum"));
            if (result.categoryConfidence * 100.0 < config.minCategoryConfidence)
                result.failureReasons.append(QStringLiteral("category_confidence_below_minimum"));
            if (result.classifiedCoverage * 100.0 < config.minClassifiedCoverage)
                result.failureReasons.append(QStringLiteral("classified_coverage_below_minimum"));

            result.success = true;
            result.measurementValid = true;
            result.ok = result.failureReasons.isEmpty();
            result.status = result.ok ? QStringLiteral("ok") : QStringLiteral("ng");
            result.message = QStringLiteral("label=%1, score=%2, confidence=%3, coverage=%4")
                    .arg(result.predictedLabel.isEmpty() ? QStringLiteral("none") : result.predictedLabel)
                    .arg(result.score, 0, 'f', 2).arg(result.categoryConfidence * 100.0, 0, 'f', 2)
                    .arg(result.classifiedCoverage * 100.0, 0, 'f', 2);

            ToolOverlay roiOverlay;
            if (regionType == QStringLiteral("circle")) {
                roiOverlay.type = ToolOverlayType::Circle;
                roiOverlay.center = QPointF(config.detectCircleCenterNormalized.x() * image.cols,
                                            config.detectCircleCenterNormalized.y() * image.rows);
                roiOverlay.radius = config.detectCircleRadiusNormalized * qMax(image.cols, image.rows);
            } else { roiOverlay.type = ToolOverlayType::Rect; roiOverlay.rect = QRectF(roi); }
            roiOverlay.label = QStringLiteral("ROI"); roiOverlay.score = result.score;
            result.overlays.append(roiOverlay);
            ToolOverlay textOverlay; textOverlay.type = ToolOverlayType::Text;
            textOverlay.p1 = QPointF(roi.x(), roi.y()); textOverlay.score = result.score;
            textOverlay.text = QStringLiteral("%1 %2 %3").arg(result.ok ? QStringLiteral("OK") : QStringLiteral("NG"),
                                                              result.predictedLabel.isEmpty() ? QStringLiteral("未分类") : result.predictedLabel,
                                                              QString::number(result.score, 'f', 1));
            textOverlay.label = QStringLiteral("color_result_text");
            textOverlay.extra.insert(QStringLiteral("status"), result.ok ? QStringLiteral("OK") : QStringLiteral("NG"));
            textOverlay.extra.insert(QStringLiteral("anchorRect"), QJsonObject{
                                         {QStringLiteral("x"), roi.x()},
                                         {QStringLiteral("y"), roi.y()},
                                         {QStringLiteral("width"), roi.width()},
                                         {QStringLiteral("height"), roi.height()}});
            result.overlays.append(textOverlay);

            QJsonArray measurements;
            for (const ColorRecognitionGmmClassMeasurement &measurement : result.classes) {
                QJsonObject item; item.insert(QStringLiteral("classId"), measurement.classId);
                item.insert(QStringLiteral("label"), measurement.label); item.insert(QStringLiteral("classIndex"), measurement.classIndex);
                item.insert(QStringLiteral("area"), measurement.area); item.insert(QStringLiteral("ratio"), measurement.ratio);
                measurements.append(item);
            }
            QJsonArray failures; for (const QString &reason : result.failureReasons) failures.append(reason);
            QJsonArray ties; for (int id : tiedIds) ties.append(id);
            result.payload = contract;
            result.payload.insert(QStringLiteral("backend"), QStringLiteral("cielab_gmm"));
            result.payload.insert(QStringLiteral("modelState"), config.modelState);
            result.payload.insert(QStringLiteral("buildParamsHash"), config.buildParamsHash);
            result.payload.insert(QStringLiteral("gmmRejectionThreshold"), config.gmmRejectionThreshold);
            result.payload.insert(QStringLiteral("predictedClassId"), result.predictedClassId);
            result.payload.insert(QStringLiteral("predictedLabel"), result.predictedLabel);
            result.payload.insert(QStringLiteral("predictedClassIndex"), result.predictedClassIndex);
            result.payload.insert(QStringLiteral("bestScore"), result.score);
            result.payload.insert(QStringLiteral("scoreMeaning"), QStringLiteral("accepted_predicted_class_pixel_ratio"));
            result.payload.insert(QStringLiteral("similarity"), result.score);
            result.payload.insert(QStringLiteral("similarityAliasOf"), QStringLiteral("bestScore"));
            result.payload.insert(QStringLiteral("similarityMeaning"), QStringLiteral("accepted_predicted_class_pixel_ratio"));
            result.payload.insert(QStringLiteral("categoryConfidence"), result.categoryConfidence * 100.0);
            result.payload.insert(QStringLiteral("classifiedCoverage"), result.classifiedCoverage * 100.0);
            result.payload.insert(QStringLiteral("effectiveArea"), result.effectiveArea);
            result.payload.insert(QStringLiteral("classifiedArea"), result.classifiedArea);
            result.payload.insert(QStringLiteral("predictedClassRatio"), result.predictedClassRatio);
            result.payload.insert(QStringLiteral("unclassifiedArea"), result.unclassifiedArea);
            result.payload.insert(QStringLiteral("unclassifiedRatio"), result.unclassifiedRatio);
            result.payload.insert(QStringLiteral("classAreaRatios"), measurements);
            result.payload.insert(QStringLiteral("judgeFailureReasons"), failures);
            result.payload.insert(QStringLiteral("predictionTie"), tiedIds.size() > 1 && maximumArea > 0);
            result.payload.insert(QStringLiteral("predictionTieClassIds"), ties);
            result.payload.insert(QStringLiteral("judgeMode"), judgeMode);
            result.payload.insert(QStringLiteral("expectedClassId"), expectedClassId);
            result.payload.insert(QStringLiteral("minScore"), config.minScore);
            result.payload.insert(QStringLiteral("minCategoryConfidence"), config.minCategoryConfidence);
            result.payload.insert(QStringLiteral("minClassifiedCoverage"), config.minClassifiedCoverage);
        } catch (...) {
            if (gmmHandle.size() > 0) api->clearClassGmm(gmmHandle.value());
            if (serializedHandle.size() > 0) api->clearSerializedItem(serializedHandle.value());
            clearObject(api, classRegions); clearObject(api, reduced);
            if (effectiveRegion != roiRegion) clearObject(api, effectiveRegion);
            clearObject(api, maskRegion); clearObject(api, roiRegion); clearLabImage(api, &lab);
            throw;
        }
        if (gmmHandle.size() > 0) api->clearClassGmm(gmmHandle.value());
        if (serializedHandle.size() > 0) api->clearSerializedItem(serializedHandle.value());
        clearObject(api, classRegions); clearObject(api, reduced);
        if (effectiveRegion != roiRegion) clearObject(api, effectiveRegion);
        clearObject(api, maskRegion); clearObject(api, roiRegion); clearLabImage(api, &lab);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const Failure &failure) {
        return fail(failure);
    } catch (const std::exception &exception) {
        return fail(Failure{QStringLiteral("gmm_classification_failed"), QString::fromLocal8Bit(exception.what()), QStringLiteral("gmm.run.exception"), H_MSG_OK});
    } catch (...) {
        return fail(Failure{QStringLiteral("gmm_classification_failed"), QStringLiteral("Unknown GMM classification failure."), QStringLiteral("gmm.run.exception"), H_MSG_OK});
    }
}

ColorRecognitionGmmBuildResult ColorRecognitionHalconRunner::buildGmmTemplateModel(
        const QVector<ColorRecognitionGmmBuildSample> &samples,
        const ColorRecognitionGmmBuildConfig &config) const
{
    return ColorRecognitionGmmHalconBackend().buildModel(samples, config);
}

ColorRecognitionGmmArtifactValidationResult ColorRecognitionHalconRunner::validateGmmModelArtifact(
        const ColorRecognitionGmmArtifact &artifact,
        const ColorRecognitionGmmBuildConfig &config) const
{
    return ColorRecognitionGmmHalconBackend().validateArtifact(artifact, config);
}

ColorRecognitionGmmRunResult ColorRecognitionHalconRunner::runGmmModel(
        const cv::Mat &image,
        const ColorRecognitionGmmRunConfig &config) const
{
    return ColorRecognitionGmmHalconBackend().runModel(image, config);
}
