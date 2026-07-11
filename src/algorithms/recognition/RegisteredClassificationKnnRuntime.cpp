#include "algorithms/recognition/RegisteredClassificationKnnRuntime.h"

#include "algorithms/halcon/HalconRuntimePaths.h"

#include <HalconC.h>

#include <QFileInfo>
#include <QJsonArray>
#include <QSet>

#include <cmath>
#include <dlfcn.h>

namespace {

struct HalconKnnApi
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
    using CreateClassKnnFn = Herror (*)(const Htuple, Htuple *);
    using AddSampleClassKnnFn = Herror (*)(const Htuple, const Htuple, const Htuple);
    using TrainClassKnnFn = Herror (*)(const Htuple, const Htuple, const Htuple);
    using SetParamsClassKnnFn = Herror (*)(const Htuple, const Htuple, const Htuple);
    using GetParamsClassKnnFn = Herror (*)(const Htuple, const Htuple, Htuple *);
    using GetSampleNumClassKnnFn = Herror (*)(const Htuple, Htuple *);
    using WriteClassKnnFn = Herror (*)(const Htuple, const Htuple);
    using ReadClassKnnFn = Herror (*)(const Htuple, Htuple *);
    using ClassifyClassKnnFn = Herror (*)(const Htuple, const Htuple, Htuple *, Htuple *);
    using ClearClassKnnFn = Herror (*)(const Htuple);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetIntFn setInt = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GetIntFn getInt = nullptr;
    CreateClassKnnFn createClassKnn = nullptr;
    AddSampleClassKnnFn addSampleClassKnn = nullptr;
    TrainClassKnnFn trainClassKnn = nullptr;
    SetParamsClassKnnFn setParamsClassKnn = nullptr;
    GetParamsClassKnnFn getParamsClassKnn = nullptr;
    GetSampleNumClassKnnFn getSampleNumClassKnn = nullptr;
    WriteClassKnnFn writeClassKnn = nullptr;
    ReadClassKnnFn readClassKnn = nullptr;
    ClassifyClassKnnFn classifyClassKnn = nullptr;
    ClearClassKnnFn clearClassKnn = nullptr;
};

bool statusOk(Herror status)
{
    return status == H_MSG_OK || status == H_MSG_TRUE || status == H_MSG_FALSE;
}

RegisteredClassificationKnnRuntimeResult errorResult(const QString &status, const QString &message)
{
    RegisteredClassificationKnnRuntimeResult result;
    result.status = status;
    result.message = message;
    result.payload.insert(QStringLiteral("errorCode"), status);
    result.payload.insert(QStringLiteral("errorMessage"), message);
    return result;
}

template <typename Function>
bool resolveRequired(void *handle, Function *target, const char *name, QString *error)
{
    dlerror();
    void *symbol = dlsym(handle, name);
    const char *symbolError = dlerror();
    if (!symbol || symbolError) {
        *error = QStringLiteral("Missing HALCON symbol %1: %2")
                .arg(QString::fromLatin1(name), symbolError ? QString::fromLocal8Bit(symbolError)
                                                            : QStringLiteral("not found"));
        return false;
    }
    *target = reinterpret_cast<Function>(symbol);
    return true;
}

class HalconLibrary
{
public:
    ~HalconLibrary()
    {
        if (m_handle)
            dlclose(m_handle);
    }

    bool load(const QString &path, QString *error, bool *missingSymbol)
    {
        *missingSymbol = false;
        const QByteArray encoded = path.toLocal8Bit();
        m_handle = dlopen(encoded.constData(), RTLD_NOW | RTLD_LOCAL);
        if (!m_handle) {
            *error = QString::fromLocal8Bit(dlerror());
            return false;
        }
        if (!resolveRequired(m_handle, &api.getErrorText, "get_error_text", error) ||
            !resolveRequired(m_handle, &api.createTuple, "F_create_tuple", error) ||
            !resolveRequired(m_handle, &api.setDouble, "F_set_d", error) ||
            !resolveRequired(m_handle, &api.setInt, "F_set_i", error) ||
            !resolveRequired(m_handle, &api.setString, "F_set_s", error) ||
            !resolveRequired(m_handle, &api.destroyTuple, "F_destroy_tuple", error) ||
            !resolveRequired(m_handle, &api.getDouble, "F_get_d", error) ||
            !resolveRequired(m_handle, &api.getInt, "F_get_i", error) ||
            !resolveRequired(m_handle, &api.createClassKnn, "T_create_class_knn", error) ||
            !resolveRequired(m_handle, &api.addSampleClassKnn, "T_add_sample_class_knn", error) ||
            !resolveRequired(m_handle, &api.trainClassKnn, "T_train_class_knn", error) ||
            !resolveRequired(m_handle, &api.setParamsClassKnn, "T_set_params_class_knn", error) ||
            !resolveRequired(m_handle, &api.getParamsClassKnn, "T_get_params_class_knn", error) ||
            !resolveRequired(m_handle, &api.getSampleNumClassKnn, "T_get_sample_num_class_knn", error) ||
            !resolveRequired(m_handle, &api.writeClassKnn, "T_write_class_knn", error) ||
            !resolveRequired(m_handle, &api.readClassKnn, "T_read_class_knn", error) ||
            !resolveRequired(m_handle, &api.classifyClassKnn, "T_classify_class_knn", error) ||
            !resolveRequired(m_handle, &api.clearClassKnn, "T_clear_class_knn", error)) {
            *missingSymbol = true;
            dlclose(m_handle);
            m_handle = nullptr;
            return false;
        }
        QString ignored;
        if (resolveRequired(m_handle, &api.setUtf8, "SetHcInterfaceStringEncodingIsUtf8", &ignored))
            api.setUtf8(1);
        return true;
    }

    HalconKnnApi api;

private:
    void *m_handle = nullptr;
};

class HalconTuple
{
public:
    explicit HalconTuple(HalconKnnApi *api = nullptr) : m_api(api) {}
    HalconTuple(const HalconTuple &) = delete;
    HalconTuple &operator=(const HalconTuple &) = delete;
    ~HalconTuple() { clear(); }

    void create(int size) { clear(); m_api->createTuple(&m_value, static_cast<Hlong>(size)); }
    void setDouble(int index, double value) { m_api->setDouble(&m_value, value, static_cast<Hlong>(index)); }
    void setInt(int index, Hlong value) { m_api->setInt(&m_value, value, static_cast<Hlong>(index)); }
    void setString(int index, const QString &value) {
        const QByteArray text = value.toUtf8();
        m_api->setString(&m_value, text.constData(), static_cast<Hlong>(index));
    }
    int size() const { return static_cast<int>(m_value.num); }
    double doubleAt(int index) const { return m_api->getDouble(&m_value, static_cast<Hlong>(index)); }
    Hlong intAt(int index) const { return m_api->getInt(&m_value, static_cast<Hlong>(index)); }
    const Htuple &value() const { return m_value; }
    Htuple *ptr() { return &m_value; }

private:
    void clear() { if (m_api) m_api->destroyTuple(&m_value); m_value = HTUPLE_INITIALIZER; }
    HalconKnnApi *m_api = nullptr;
    Htuple m_value = HTUPLE_INITIALIZER;
};

class HalconKnnHandleGuard
{
public:
    explicit HalconKnnHandleGuard(HalconKnnApi *api) : m_api(api), m_handle(api) {}
    ~HalconKnnHandleGuard() { if (m_valid) m_api->clearClassKnn(m_handle.value()); }
    Htuple *outPtr() { return m_handle.ptr(); }
    const Htuple &value() const { return m_handle.value(); }
    void markValid() { m_valid = true; }

private:
    HalconKnnApi *m_api = nullptr;
    HalconTuple m_handle;
    bool m_valid = false;
};

QString errorText(HalconKnnApi *api, Herror status)
{
    char text[1024] = {0};
    if (api->getErrorText && statusOk(api->getErrorText(static_cast<Hlong>(status), text))) {
        const QString message = QString::fromUtf8(text).trimmed();
        if (!message.isEmpty())
            return message;
    }
    return QStringLiteral("HALCON error code %1").arg(static_cast<qlonglong>(status));
}

QStringList candidatesFor(const RegisteredClassificationKnnRuntimeConfig &config)
{
    QStringList candidates;
    HalconRuntimePaths::initializeHalconEnvironment();
    HalconRuntimePaths::resolveHalconLibPath(config.halconSoPath, &candidates);
    for (const QString &candidate : config.halconSoPathCandidates) {
        if (!candidate.trimmed().isEmpty() && !candidates.contains(candidate))
            candidates.append(candidate);
    }
    return candidates;
}

QString existingLibrary(const RegisteredClassificationKnnRuntimeConfig &config, QStringList *candidates)
{
    *candidates = candidatesFor(config);
    for (const QString &candidate : *candidates) {
        if (QFileInfo(candidate).isFile())
            return candidate;
    }
    return QString();
}

QJsonArray distancesJson(const QVector<RegisteredClassificationKnnDistance> &distances)
{
    QJsonArray values;
    for (const RegisteredClassificationKnnDistance &distance : distances) {
        QJsonObject item;
        item.insert(QStringLiteral("classId"), distance.classId);
        item.insert(QStringLiteral("distance"), distance.distance);
        values.append(item);
    }
    return values;
}

bool finiteFeature(const QVector<double> &feature, int expectedLength)
{
    if (feature.size() != expectedLength)
        return false;
    for (double value : feature) {
        if (!std::isfinite(value))
            return false;
    }
    return true;
}

} // namespace

RegisteredClassificationKnnRuntimeResult RegisteredClassificationKnnRuntime::buildAndWrite(
        const RegisteredClassificationKnnRuntimeConfig &config,
        const RegisteredClassificationKnnBuildRequest &request) const
{
    if (request.outputPath.trimmed().isEmpty() || request.featureLength <= 0 || request.samples.isEmpty() ||
        request.numTrees <= 0 || request.k <= 0 || request.maxNumClasses <= 0) {
        return errorResult(QStringLiteral("invalid_knn_request"),
                           QStringLiteral("KNN output path, samples, feature length, and parameters must be valid."));
    }
    for (const RegisteredClassificationKnnSample &sample : request.samples) {
        if (sample.classId < 0 || !finiteFeature(sample.feature, request.featureLength)) {
            return errorResult(QStringLiteral("invalid_feature_value"),
                               QStringLiteral("Every KNN sample must have a non-negative class id and finite fixed-length feature."));
        }
    }
    QStringList candidates;
    const QString libraryPath = existingLibrary(config, &candidates);
    if (libraryPath.isEmpty()) {
        RegisteredClassificationKnnRuntimeResult result = errorResult(QStringLiteral("halcon_so_not_found"),
                QStringLiteral("HALCON runtime file not found. Tried: %1").arg(HalconRuntimePaths::formatTriedPaths(candidates)));
        result.payload.insert(QStringLiteral("halconSoPathCandidates"), QJsonArray::fromStringList(candidates));
        return result;
    }
    HalconLibrary library;
    QString loadError;
    bool missingSymbol = false;
    if (!library.load(libraryPath, &loadError, &missingSymbol))
        return errorResult(missingSymbol ? QStringLiteral("halcon_symbol_missing") : QStringLiteral("halcon_load_failed"), loadError);

    HalconKnnApi *api = &library.api;
    HalconTuple dimensions(api);
    dimensions.create(1);
    dimensions.setInt(0, request.featureLength);
    HalconKnnHandleGuard handle(api);
    Herror status = api->createClassKnn(dimensions.value(), handle.outPtr());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_create_failed"), errorText(api, status));
    handle.markValid();
    HalconTuple feature(api);
    HalconTuple classId(api);
    for (const RegisteredClassificationKnnSample &sample : request.samples) {
        feature.create(request.featureLength);
        for (int index = 0; index < request.featureLength; ++index)
            feature.setDouble(index, sample.feature.at(index));
        classId.create(1);
        classId.setInt(0, sample.classId);
        status = api->addSampleClassKnn(handle.value(), feature.value(), classId.value());
        if (!statusOk(status))
            return errorResult(QStringLiteral("knn_train_failed"), errorText(api, status));
    }
    HalconTuple trainNames(api);
    HalconTuple trainValues(api);
    trainNames.create(2);
    trainNames.setString(0, QStringLiteral("normalization"));
    trainNames.setString(1, QStringLiteral("num_trees"));
    trainValues.create(2);
    trainValues.setString(0, QStringLiteral("false"));
    trainValues.setInt(1, request.numTrees);
    status = api->trainClassKnn(handle.value(), trainNames.value(), trainValues.value());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_train_failed"), errorText(api, status));
    HalconTuple parameterNames(api);
    HalconTuple parameterValues(api);
    parameterNames.create(5);
    parameterNames.setString(0, QStringLiteral("method"));
    parameterNames.setString(1, QStringLiteral("k"));
    parameterNames.setString(2, QStringLiteral("max_num_classes"));
    parameterNames.setString(3, QStringLiteral("num_checks"));
    parameterNames.setString(4, QStringLiteral("epsilon"));
    parameterValues.create(5);
    parameterValues.setString(0, QStringLiteral("classes_distance"));
    parameterValues.setInt(1, request.k);
    parameterValues.setInt(2, request.maxNumClasses);
    parameterValues.setInt(3, 0);
    parameterValues.setDouble(4, 0.0);
    status = api->setParamsClassKnn(handle.value(), parameterNames.value(), parameterValues.value());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_parameter_failed"), errorText(api, status));
    HalconTuple outputPath(api);
    outputPath.create(1);
    outputPath.setString(0, request.outputPath);
    status = api->writeClassKnn(handle.value(), outputPath.value());
    if (!statusOk(status))
        return errorResult(QStringLiteral("model_write_failed"), errorText(api, status));
    RegisteredClassificationKnnRuntimeResult result;
    result.success = true;
    result.status = QStringLiteral("ok");
    result.message = QStringLiteral("HALCON KNN model written.");
    result.payload.insert(QStringLiteral("outputPath"), request.outputPath);
    result.payload.insert(QStringLiteral("halconSoPath"), libraryPath);
    return result;
}

RegisteredClassificationKnnRuntimeResult RegisteredClassificationKnnRuntime::classifyPair(
        const RegisteredClassificationKnnRuntimeConfig &config,
        const QString &sampleModelPath,
        const QString &centerModelPath,
        const QVector<double> &feature) const
{
    if (!QFileInfo(sampleModelPath).isFile() || !QFileInfo(centerModelPath).isFile())
        return errorResult(QStringLiteral("model_file_not_found"), QStringLiteral("Both sample and class-center KNN model files are required."));
    if (feature.isEmpty() || !finiteFeature(feature, feature.size()))
        return errorResult(QStringLiteral("invalid_feature_value"), QStringLiteral("KNN query feature must be finite and non-empty."));
    QStringList candidates;
    const QString libraryPath = existingLibrary(config, &candidates);
    if (libraryPath.isEmpty())
        return errorResult(QStringLiteral("halcon_so_not_found"), QStringLiteral("HALCON runtime file not found."));
    HalconLibrary library;
    QString loadError;
    bool missingSymbol = false;
    if (!library.load(libraryPath, &loadError, &missingSymbol))
        return errorResult(missingSymbol ? QStringLiteral("halcon_symbol_missing") : QStringLiteral("halcon_load_failed"), loadError);
    HalconKnnApi *api = &library.api;
    HalconTuple samplePath(api);
    HalconTuple centerPath(api);
    samplePath.create(1);
    samplePath.setString(0, sampleModelPath);
    centerPath.create(1);
    centerPath.setString(0, centerModelPath);
    HalconKnnHandleGuard sampleHandle(api);
    HalconKnnHandleGuard centerHandle(api);
    Herror status = api->readClassKnn(samplePath.value(), sampleHandle.outPtr());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_read_failed"), errorText(api, status));
    sampleHandle.markValid();
    status = api->readClassKnn(centerPath.value(), centerHandle.outPtr());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_read_failed"), errorText(api, status));
    centerHandle.markValid();
    HalconTuple sampleNum(api);
    HalconTuple centerNum(api);
    status = api->getSampleNumClassKnn(sampleHandle.value(), sampleNum.ptr());
    if (!statusOk(status) || sampleNum.size() != 1 || sampleNum.intAt(0) < 2)
        return errorResult(QStringLiteral("knn_model_mismatch"), QStringLiteral("Sample KNN model has an invalid sample count."));
    status = api->getSampleNumClassKnn(centerHandle.value(), centerNum.ptr());
    if (!statusOk(status) || centerNum.size() != 1 || centerNum.intAt(0) < 2)
        return errorResult(QStringLiteral("knn_model_mismatch"), QStringLiteral("Center KNN model has an invalid sample count."));
    HalconTuple parameterNames(api);
    HalconTuple sampleParameterValues(api);
    HalconTuple centerParameterValues(api);
    parameterNames.create(5);
    parameterNames.setString(0, QStringLiteral("method"));
    parameterNames.setString(1, QStringLiteral("k"));
    parameterNames.setString(2, QStringLiteral("max_num_classes"));
    parameterNames.setString(3, QStringLiteral("num_checks"));
    parameterNames.setString(4, QStringLiteral("epsilon"));
    const auto configureReadModel = [&parameterNames, api](HalconTuple *values, Hlong count) {
        values->create(5);
        values->setString(0, QStringLiteral("classes_distance"));
        values->setInt(1, count);
        values->setInt(2, count);
        values->setInt(3, 0);
        values->setDouble(4, 0.0);
        return api->setParamsClassKnn;
    };
    status = configureReadModel(&sampleParameterValues, sampleNum.intAt(0))(sampleHandle.value(), parameterNames.value(), sampleParameterValues.value());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_parameter_failed"), errorText(api, status));
    status = configureReadModel(&centerParameterValues, centerNum.intAt(0))(centerHandle.value(), parameterNames.value(), centerParameterValues.value());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_parameter_failed"), errorText(api, status));
    HalconTuple parameterName(api);
    HalconTuple sampleMaxClasses(api);
    HalconTuple centerMaxClasses(api);
    parameterName.create(1);
    parameterName.setString(0, QStringLiteral("max_num_classes"));
    status = api->getParamsClassKnn(sampleHandle.value(), parameterName.value(), sampleMaxClasses.ptr());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_read_failed"), errorText(api, status));
    status = api->getParamsClassKnn(centerHandle.value(), parameterName.value(), centerMaxClasses.ptr());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_read_failed"), errorText(api, status));
    HalconTuple query(api);
    query.create(feature.size());
    for (int index = 0; index < feature.size(); ++index)
        query.setDouble(index, feature.at(index));
    HalconTuple sampleClasses(api);
    HalconTuple sampleRatings(api);
    HalconTuple centerClasses(api);
    HalconTuple centerRatings(api);
    status = api->classifyClassKnn(sampleHandle.value(), query.value(), sampleClasses.ptr(), sampleRatings.ptr());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_classify_failed"), errorText(api, status));
    status = api->classifyClassKnn(centerHandle.value(), query.value(), centerClasses.ptr(), centerRatings.ptr());
    if (!statusOk(status))
        return errorResult(QStringLiteral("knn_classify_failed"), errorText(api, status));
    if (sampleClasses.size() < 2 || sampleClasses.size() != sampleRatings.size() ||
        centerClasses.size() != centerRatings.size() || centerClasses.size() != sampleClasses.size()) {
        return errorResult(QStringLiteral("knn_model_mismatch"),
                           QStringLiteral("KNN models returned incomplete or incompatible class distances "
                                          "(sample=%1/%2 max=%5, center=%3/%4 max=%6).").arg(sampleClasses.size())
                           .arg(sampleRatings.size()).arg(centerClasses.size()).arg(centerRatings.size())
                           .arg(sampleMaxClasses.size() ? sampleMaxClasses.intAt(0) : -1)
                           .arg(centerMaxClasses.size() ? centerMaxClasses.intAt(0) : -1));
    }
    QSet<int> sampleIds;
    QSet<int> centerIds;
    RegisteredClassificationKnnRuntimeResult result;
    for (int index = 0; index < sampleClasses.size(); ++index) {
        const int id = static_cast<int>(sampleClasses.intAt(index));
        const double distance = sampleRatings.doubleAt(index);
        if (id < 0 || !std::isfinite(distance) || sampleIds.contains(id))
            return errorResult(QStringLiteral("knn_model_mismatch"), QStringLiteral("Sample KNN output has duplicate or invalid classes."));
        sampleIds.insert(id);
        result.sampleDistances.append({id, distance});
    }
    for (int index = 0; index < centerClasses.size(); ++index) {
        const int id = static_cast<int>(centerClasses.intAt(index));
        const double distance = centerRatings.doubleAt(index);
        if (id < 0 || !std::isfinite(distance) || centerIds.contains(id))
            return errorResult(QStringLiteral("knn_model_mismatch"), QStringLiteral("Center KNN output has duplicate or invalid classes."));
        centerIds.insert(id);
        result.centerDistances.append({id, distance});
    }
    if (sampleIds != centerIds)
        return errorResult(QStringLiteral("knn_model_mismatch"), QStringLiteral("Sample and class-center KNN class-id sets differ."));
    result.success = true;
    result.status = QStringLiteral("ok");
    result.message = QStringLiteral("HALCON dual KNN classification succeeded.");
    result.payload.insert(QStringLiteral("sampleDistances"), distancesJson(result.sampleDistances));
    result.payload.insert(QStringLiteral("centerDistances"), distancesJson(result.centerDistances));
    result.payload.insert(QStringLiteral("halconSoPath"), libraryPath);
    return result;
}
