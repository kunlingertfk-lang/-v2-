#include "algorithms/recognition/RegisteredClassificationTrainingRunner.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"

#include <HalconC.h>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>

#include <dlfcn.h>

namespace {

struct PreparedSample
{
    int classIndex = -1;
    int classId = -1;
    QVector<double> feature;
};

struct HalconTrainingApi
{
    using SetUtf8Fn = void (*)(int);
    using GetErrorTextFn = Herror (*)(Hlong, char *);
    using CreateTupleFn = void (*)(Htuple *, Hlong);
    using SetDoubleFn = void (*)(Htuple *, double, Hlong);
    using SetIntFn = void (*)(Htuple *, Hlong, Hlong);
    using SetStringFn = void (*)(Htuple *, const char *, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using CreateClassMlpFn = Herror (*)(const Htuple, const Htuple, const Htuple, const Htuple,
                                        const Htuple, const Htuple, const Htuple, Htuple *);
    using AddSampleClassMlpFn = Herror (*)(const Htuple, const Htuple, const Htuple);
    using TrainClassMlpFn = Herror (*)(const Htuple, const Htuple, const Htuple, const Htuple,
                                       Htuple *, Htuple *);
    using WriteClassMlpFn = Herror (*)(const Htuple, const Htuple);
    using ClearClassMlpFn = Herror (*)(const Htuple);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetIntFn setInt = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    CreateClassMlpFn createClassMlp = nullptr;
    AddSampleClassMlpFn addSampleClassMlp = nullptr;
    TrainClassMlpFn trainClassMlp = nullptr;
    WriteClassMlpFn writeClassMlp = nullptr;
    ClearClassMlpFn clearClassMlp = nullptr;
};

bool halconStatusOk(const Herror status)
{
    return status == H_MSG_OK || status == H_MSG_TRUE || status == H_MSG_FALSE;
}

RegisteredClassificationTrainingResult resultWithStatus(const QString &status,
                                                        const QString &message)
{
    RegisteredClassificationTrainingResult result;
    result.success = (status == QStringLiteral("ok"));
    result.status = status;
    result.message = message;
    result.payload.insert(QStringLiteral("errorCode"), status);
    result.payload.insert(QStringLiteral("errorMessage"), message);
    return result;
}

void appendUnique(QStringList *values, const QString &value)
{
    if (!values)
        return;
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty() && !values->contains(trimmed))
        values->append(trimmed);
}

QJsonArray stringListToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values)
        array.append(value);
    return array;
}

QJsonArray doubleVectorToJson(const QVector<double> &values)
{
    QJsonArray array;
    for (double value : values)
        array.append(value);
    return array;
}

QJsonObject mapToJsonObject(const QMap<QString, int> &counts)
{
    QJsonObject json;
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it)
        json.insert(it.key(), it.value());
    return json;
}

QString halconErrorText(HalconTrainingApi *api, const Herror status)
{
    char buffer[1024] = {0};
    if (api && api->getErrorText && halconStatusOk(api->getErrorText(static_cast<Hlong>(status), buffer))) {
        const QString message = QString::fromUtf8(buffer).trimmed();
        if (!message.isEmpty())
            return message;
    }
    return QStringLiteral("HALCON error code %1").arg(static_cast<qlonglong>(status));
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
            !resolveRequired(m_handle, api.createClassMlp, "T_create_class_mlp", errorMessage) ||
            !resolveRequired(m_handle, api.addSampleClassMlp, "T_add_sample_class_mlp", errorMessage) ||
            !resolveRequired(m_handle, api.trainClassMlp, "T_train_class_mlp", errorMessage) ||
            !resolveRequired(m_handle, api.writeClassMlp, "T_write_class_mlp", errorMessage) ||
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

    HalconTrainingApi api;

private:
    void *m_handle = nullptr;
};

class HalconTuple
{
public:
    explicit HalconTuple(HalconTrainingApi *api = nullptr)
        : m_api(api)
    {
    }

    HalconTuple(const HalconTuple &) = delete;
    HalconTuple &operator=(const HalconTuple &) = delete;

    ~HalconTuple()
    {
        destroy();
    }

    void create(const int size)
    {
        destroy();
        if (m_api)
            m_api->createTuple(&m_tuple, static_cast<Hlong>(size));
    }

    void setInt(const int index, const Hlong value)
    {
        if (m_api)
            m_api->setInt(&m_tuple, value, static_cast<Hlong>(index));
    }

    void setDouble(const int index, const double value)
    {
        if (m_api)
            m_api->setDouble(&m_tuple, value, static_cast<Hlong>(index));
    }

    void setString(const int index, const QString &value)
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

    double getDouble(const int index) const
    {
        return m_api ? m_api->getDouble(&m_tuple, static_cast<Hlong>(index)) : 0.0;
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

    HalconTrainingApi *m_api = nullptr;
    Htuple m_tuple = HTUPLE_INITIALIZER;
};

class HalconMlpHandleGuard
{
public:
    explicit HalconMlpHandleGuard(HalconTrainingApi *api)
        : m_api(api)
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
    HalconTrainingApi *m_api = nullptr;
    HalconTuple m_handle {m_api};
    bool m_hasHandle = false;
};

bool ensureDirectoryEmpty(const QString &path)
{
    QDir dir(path);
    if (dir.exists() && !dir.removeRecursively())
        return false;
    return QDir().mkpath(path);
}

bool swapModelDirectory(const QString &tmpDirPath,
                        const QString &targetDirPath,
                        QString *errorMessage)
{
    const QString backupDirPath = targetDirPath + QStringLiteral(".bak");
    QDir backupDir(backupDirPath);
    if (backupDir.exists() && !backupDir.removeRecursively()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to clear backup directory %1.").arg(backupDirPath);
        return false;
    }

    bool targetMoved = false;
    QDir targetDir(targetDirPath);
    if (targetDir.exists()) {
        if (!QDir().rename(targetDirPath, backupDirPath)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Failed to move existing model directory %1.").arg(targetDirPath);
            return false;
        }
        targetMoved = true;
    }

    if (!QDir().rename(tmpDirPath, targetDirPath)) {
        if (targetMoved)
            QDir().rename(backupDirPath, targetDirPath);
        if (errorMessage)
            *errorMessage = QStringLiteral("Failed to move trained model directory into place.");
        return false;
    }

    if (targetMoved)
        QDir(backupDirPath).removeRecursively();
    return true;
}

} // namespace

RegisteredClassificationTrainingResult RegisteredClassificationTrainingRunner::train(
        const RegisteredClassificationTrainingRequest &request) const
{
    if (request.outputModelDir.trimmed().isEmpty()) {
        return resultWithStatus(QStringLiteral("invalid_argument"),
                                QStringLiteral("Training outputModelDir must not be empty."));
    }
    if (request.classLabels.size() < 2) {
        return resultWithStatus(QStringLiteral("training_not_enough_classes"),
                                QStringLiteral("At least two class labels are required for MLP training."));
    }

    QMap<int, int> classIndexById;
    QMap<QString, int> validSamplesByClass;
    QMap<QString, int> invalidSamplesByClass;
    QStringList warnings;
    for (int classIndex = 0; classIndex < request.classLabels.size(); ++classIndex) {
        const RegisteredClassificationClassLabel &label = request.classLabels.at(classIndex);
        if (label.id < 0 || label.name.trimmed().isEmpty()) {
            return resultWithStatus(QStringLiteral("invalid_class_label"),
                                    QStringLiteral("Class labels must have non-negative ids and non-empty names."));
        }
        if (classIndexById.contains(label.id)) {
            return resultWithStatus(QStringLiteral("invalid_class_label"),
                                    QStringLiteral("Class label ids must be unique."));
        }
        classIndexById.insert(label.id, classIndex);
        validSamplesByClass.insert(label.name, 0);
        invalidSamplesByClass.insert(label.name, 0);
    }

    RegisteredClassificationFeatureConfig featureConfig;
    featureConfig.halconSoPath = request.halconSoPath;
    featureConfig.halconSoPathCandidates = request.halconSoPathCandidates;

    RegisteredClassificationFeatureExtractor extractor;
    QVector<PreparedSample> preparedSamples;
    preparedSamples.reserve(request.samples.size());

    for (int sampleIndex = 0; sampleIndex < request.samples.size(); ++sampleIndex) {
        const RegisteredClassificationTrainingSample &sample = request.samples.at(sampleIndex);
        if (!classIndexById.contains(sample.classId))
            continue;

        const RegisteredClassificationClassLabel &label =
                request.classLabels.at(classIndexById.value(sample.classId));
        const RegisteredClassificationFeatureResult feature =
                extractor.extract(sample.image, sample.roiNormalized, featureConfig);
        if (!feature.success) {
            invalidSamplesByClass[label.name] += 1;
            warnings.append(QStringLiteral("sample[%1] class=%2 skipped: %3")
                            .arg(sampleIndex)
                            .arg(label.name, feature.status));
            continue;
        }
        if (feature.feature.size() != registeredClassificationFeatureNamesV1().size()) {
            invalidSamplesByClass[label.name] += 1;
            warnings.append(QStringLiteral("sample[%1] class=%2 skipped: feature_length_mismatch")
                            .arg(sampleIndex)
                            .arg(label.name));
            continue;
        }

        PreparedSample prepared;
        prepared.classIndex = classIndexById.value(sample.classId);
        prepared.classId = sample.classId;
        prepared.feature = feature.feature;
        preparedSamples.append(prepared);
        validSamplesByClass[label.name] += 1;
    }

    QStringList missingClassNames;
    QStringList lowSampleClassNames;
    for (const RegisteredClassificationClassLabel &label : request.classLabels) {
        const int validCount = validSamplesByClass.value(label.name);
        if (validCount <= 0)
            missingClassNames.append(label.name);
        else if (validCount < 3)
            lowSampleClassNames.append(label.name);
    }

    if (!missingClassNames.isEmpty()) {
        RegisteredClassificationTrainingResult result =
                resultWithStatus(QStringLiteral("training_not_enough_classes"),
                                 QStringLiteral("Each class must contribute at least one valid sample."));
        result.payload.insert(QStringLiteral("missingValidSampleClasses"),
                              stringListToJson(missingClassNames));
        result.payload.insert(QStringLiteral("validSamplesByClass"), mapToJsonObject(validSamplesByClass));
        result.payload.insert(QStringLiteral("invalidSamplesByClass"), mapToJsonObject(invalidSamplesByClass));
        return result;
    }
    if (preparedSamples.isEmpty()) {
        return resultWithStatus(QStringLiteral("training_not_enough_classes"),
                                QStringLiteral("No valid training samples were produced."));
    }
    if (!lowSampleClassNames.isEmpty())
        warnings.append(QStringLiteral("classes with fewer than 3 valid samples: %1")
                        .arg(lowSampleClassNames.join(QStringLiteral(", "))));

    HalconRuntimePaths::initializeHalconEnvironment();
    QStringList halconCandidates;
    QString halconLibPath = HalconRuntimePaths::resolveHalconLibPath(request.halconSoPath, &halconCandidates);
    for (const QString &candidate : request.halconSoPathCandidates)
        appendUnique(&halconCandidates, candidate);
    if (halconLibPath.isEmpty()) {
        for (const QString &candidate : halconCandidates) {
            if (QFileInfo::exists(candidate)) {
                halconLibPath = candidate;
                break;
            }
        }
    }
    if (halconLibPath.isEmpty()) {
        RegisteredClassificationTrainingResult result =
                resultWithStatus(QStringLiteral("halcon_so_not_found"),
                                 QStringLiteral("HALCON runtime file not found."));
        result.payload.insert(QStringLiteral("halconSoPathCandidates"), stringListToJson(halconCandidates));
        return result;
    }

    HalconLibrary library;
    QString loadMessage;
    bool symbolMissing = false;
    if (!library.load(halconLibPath, loadMessage, symbolMissing)) {
        RegisteredClassificationTrainingResult result =
                resultWithStatus(symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                              : QStringLiteral("halcon_load_failed"),
                                 loadMessage);
        result.payload.insert(QStringLiteral("halconSoPath"), halconLibPath);
        result.payload.insert(QStringLiteral("halconSoPathCandidates"), stringListToJson(halconCandidates));
        return result;
    }

    HalconTrainingApi *api = &library.api;
    QElapsedTimer timer;
    timer.start();

    const QString tmpDirPath = request.outputModelDir + QStringLiteral(".tmp");
    if (!ensureDirectoryEmpty(tmpDirPath)) {
        return resultWithStatus(QStringLiteral("model_write_failed"),
                                QStringLiteral("Failed to create temporary model directory."));
    }

    HalconTuple numInput(api);
    HalconTuple numHidden(api);
    HalconTuple numOutput(api);
    HalconTuple outputFunction(api);
    HalconTuple preprocessing(api);
    HalconTuple numComponents(api);
    HalconTuple randSeed(api);
    HalconMlpHandleGuard mlpHandle(api);
    HalconTuple features(api);
    HalconTuple target(api);
    HalconTuple maxIterations(api);
    HalconTuple weightTolerance(api);
    HalconTuple errorTolerance(api);
    HalconTuple trainError(api);
    HalconTuple errorLog(api);
    HalconTuple mlpPath(api);

    auto cleanupTmp = [&tmpDirPath]() {
        QDir(tmpDirPath).removeRecursively();
    };

    numInput.create(1);
    numInput.setInt(0, static_cast<Hlong>(registeredClassificationFeatureNamesV1().size()));
    numHidden.create(1);
    numHidden.setInt(0, static_cast<Hlong>(qMax(1, request.mlp.numHidden)));
    numOutput.create(1);
    numOutput.setInt(0, static_cast<Hlong>(request.classLabels.size()));
    outputFunction.create(1);
    outputFunction.setString(0, QStringLiteral("softmax"));
    preprocessing.create(1);
    preprocessing.setString(0, QStringLiteral("normalization"));
    numComponents.create(1);
    numComponents.setInt(0, 1);
    randSeed.create(1);
    randSeed.setInt(0, static_cast<Hlong>(request.mlp.randSeed));

    const Herror createStatus = api->createClassMlp(numInput.value(),
                                                    numHidden.value(),
                                                    numOutput.value(),
                                                    outputFunction.value(),
                                                    preprocessing.value(),
                                                    numComponents.value(),
                                                    randSeed.value(),
                                                    mlpHandle.outPtr());
    if (!halconStatusOk(createStatus)) {
        cleanupTmp();
        return resultWithStatus(QStringLiteral("mlp_create_failed"),
                                halconErrorText(api, createStatus));
    }
    mlpHandle.markCreated();

    for (const PreparedSample &sample : preparedSamples) {
        features.create(sample.feature.size());
        for (int featureIndex = 0; featureIndex < sample.feature.size(); ++featureIndex)
            features.setDouble(featureIndex, sample.feature.at(featureIndex));

        target.create(1);
        target.setInt(0, static_cast<Hlong>(sample.classIndex));

        const Herror addStatus = api->addSampleClassMlp(mlpHandle.value(), features.value(), target.value());
        if (!halconStatusOk(addStatus)) {
            cleanupTmp();
            return resultWithStatus(QStringLiteral("mlp_train_failed"),
                                    halconErrorText(api, addStatus));
        }
    }

    maxIterations.create(1);
    maxIterations.setInt(0, static_cast<Hlong>(qMax(1, request.mlp.maxIterations)));
    weightTolerance.create(1);
    weightTolerance.setDouble(0, 0.01);
    errorTolerance.create(1);
    errorTolerance.setDouble(0, 0.01);

    const Herror trainStatus = api->trainClassMlp(mlpHandle.value(),
                                                  maxIterations.value(),
                                                  weightTolerance.value(),
                                                  errorTolerance.value(),
                                                  trainError.ptr(),
                                                  errorLog.ptr());
    if (!halconStatusOk(trainStatus)) {
        cleanupTmp();
        return resultWithStatus(QStringLiteral("mlp_train_failed"),
                                halconErrorText(api, trainStatus));
    }

    const QString mlpOutputPath = registeredClassificationMlpPath(tmpDirPath);
    mlpPath.create(1);
    mlpPath.setString(0, mlpOutputPath);
    const Herror writeStatus = api->writeClassMlp(mlpHandle.value(), mlpPath.value());
    if (!halconStatusOk(writeStatus)) {
        cleanupTmp();
        return resultWithStatus(QStringLiteral("model_write_failed"),
                                halconErrorText(api, writeStatus));
    }

    RegisteredClassificationModelMetadata metadata;
    metadata.modelType = registeredClassificationMlpModelType();
    metadata.schemaVersion = 1;
    metadata.featureVersion = registeredClassificationFeatureVersionV1();
    metadata.halconVersion = QStringLiteral("24.11");
    metadata.classLabels = request.classLabels;
    metadata.featureNames = registeredClassificationFeatureNamesV1();
    metadata.featureLength = metadata.featureNames.size();
    metadata.preprocess.insert(QStringLiteral("halconPreprocessing"), QStringLiteral("normalization"));
    metadata.preprocess.insert(QStringLiteral("normalizeGray"), true);
    metadata.preprocess.insert(QStringLiteral("smooth"), true);
    metadata.mlp = request.mlp;
    metadata.thresholds = request.thresholds;
    metadata.trainingSampleCount = preparedSamples.size();

    const RegisteredClassificationModelPackageResult metadataWrite =
            writeRegisteredClassificationMetadata(tmpDirPath, metadata);
    if (!metadataWrite.success) {
        cleanupTmp();
        return resultWithStatus(QStringLiteral("model_write_failed"), metadataWrite.message);
    }

    QJsonObject report;
    report.insert(QStringLiteral("sampleCount"), preparedSamples.size());
    report.insert(QStringLiteral("validSamplesByClass"), mapToJsonObject(validSamplesByClass));
    report.insert(QStringLiteral("invalidSamplesByClass"), mapToJsonObject(invalidSamplesByClass));
    report.insert(QStringLiteral("elapsedMs"), static_cast<double>(timer.elapsed()));
    report.insert(QStringLiteral("halconSoPath"), halconLibPath);
    report.insert(QStringLiteral("Error"), trainError.size() > 0 ? trainError.getDouble(0) : 0.0);
    report.insert(QStringLiteral("ErrorLog"), doubleVectorToJson([&errorLog]() {
        QVector<double> values;
        values.reserve(errorLog.size());
        for (int i = 0; i < errorLog.size(); ++i)
            values.append(errorLog.getDouble(i));
        return values;
    }()));
    report.insert(QStringLiteral("warnings"), stringListToJson(warnings));

    QFile reportFile(registeredClassificationTrainingReportPath(tmpDirPath));
    if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cleanupTmp();
        return resultWithStatus(QStringLiteral("model_write_failed"), reportFile.errorString());
    }
    reportFile.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    reportFile.close();

    if (!request.trainingSessionManifest.isEmpty() || !request.trainingSessionAssets.isEmpty()) {
        const RegisteredClassificationTrainingSessionResult sessionWrite =
                writeRegisteredClassificationTrainingSession(
                        QDir(tmpDirPath).filePath(QStringLiteral("training_session")),
                        request.trainingSessionManifest,
                        request.trainingSessionAssets);
        if (!sessionWrite.success) {
            cleanupTmp();
            return resultWithStatus(QStringLiteral("training_session_write_failed"),
                                    sessionWrite.message);
        }
    }

    if (!QFileInfo::exists(mlpOutputPath) ||
        !QFileInfo::exists(registeredClassificationMetadataPath(tmpDirPath)) ||
        !QFileInfo::exists(registeredClassificationTrainingReportPath(tmpDirPath))) {
        cleanupTmp();
        return resultWithStatus(QStringLiteral("model_write_failed"),
                                QStringLiteral("Temporary model package is incomplete."));
    }

    QString swapError;
    if (!swapModelDirectory(tmpDirPath, request.outputModelDir, &swapError)) {
        cleanupTmp();
        return resultWithStatus(QStringLiteral("model_write_failed"), swapError);
    }

    RegisteredClassificationTrainingResult result =
            resultWithStatus(QStringLiteral("ok"), QStringLiteral("Registered classification MLP training completed."));
    result.modelDir = request.outputModelDir;
    result.sampleCount = preparedSamples.size();
    result.payload.insert(QStringLiteral("halconSoPath"), halconLibPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"), stringListToJson(halconCandidates));
    result.payload.insert(QStringLiteral("validSamplesByClass"), mapToJsonObject(validSamplesByClass));
    result.payload.insert(QStringLiteral("invalidSamplesByClass"), mapToJsonObject(invalidSamplesByClass));
    result.payload.insert(QStringLiteral("warnings"), stringListToJson(warnings));
    result.payload.insert(QStringLiteral("errorLog"), report.value(QStringLiteral("ErrorLog")).toArray());
    result.payload.insert(QStringLiteral("trainingError"), report.value(QStringLiteral("Error")));
    result.payload.insert(QStringLiteral("elapsedMs"), report.value(QStringLiteral("elapsedMs")));
    return result;
}
