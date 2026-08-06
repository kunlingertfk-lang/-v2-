#include "algorithms/recognition/RegisteredClassificationEmbeddingModelProvider.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

#include <cmath>

namespace {

RegisteredClassificationEmbeddingModelResolveResult errorResult(
        const QString &status,
        const QString &message,
        const QString &root,
        const QString &descriptorPath = QString())
{
    RegisteredClassificationEmbeddingModelResolveResult result;
    result.status = status;
    result.message = message;
    result.payload.insert(QStringLiteral("featureModelRoot"), root);
    if (!descriptorPath.isEmpty())
        result.payload.insert(QStringLiteral("descriptorPath"), descriptorPath);
    return result;
}

bool isSafePathComponent(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_.-]+$"));
    return !value.isEmpty()
            && value != QStringLiteral(".")
            && value != QStringLiteral("..")
            && pattern.match(value).hasMatch();
}

bool isSafeLeafFileName(const QString &value)
{
    if (value.trimmed() != value || !isSafePathComponent(value))
        return false;
    return QFileInfo(value).fileName() == value;
}

bool isExactInteger(const QJsonValue &value)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number;
}

QString fileSha256(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        if (error)
            *error = QStringLiteral("Failed to read the complete file.");
        return QString();
    }
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(hash.result().toHex()));
}

bool isSha256(const QString &value)
{
    static const QRegularExpression pattern(QStringLiteral("^sha256:[0-9a-f]{64}$"));
    return pattern.match(value).hasMatch();
}

QString requiredString(const QJsonObject &json, const QString &key)
{
    const QJsonValue value = json.value(key);
    return value.isString() ? value.toString().trimmed() : QString();
}

} // namespace

RegisteredClassificationEmbeddingModelProvider::
RegisteredClassificationEmbeddingModelProvider(const QString &featureModelRoot)
    : m_featureModelRoot(QDir::cleanPath(featureModelRoot.trimmed().isEmpty()
                                        ? defaultFeatureModelRoot()
                                        : featureModelRoot.trimmed()))
{
}

RegisteredClassificationEmbeddingModelResolveResult
RegisteredClassificationEmbeddingModelProvider::resolve(
        const QString &requestedModelId,
        const QString &requestedModelVersion) const
{
    const QString modelId = requestedModelId.trimmed();
    const QString modelVersion = requestedModelVersion.trimmed();
    if (!isSafePathComponent(modelId)
            || (!modelVersion.isEmpty() && !isSafePathComponent(modelVersion))) {
        return errorResult(
                    QStringLiteral("dl_feature_model_id_invalid"),
                    QStringLiteral("Feature model id and version must contain only letters, digits, '.', '_' or '-'."),
                    m_featureModelRoot);
    }

    QString modelDirectory = QDir(m_featureModelRoot).filePath(modelId);
    if (!modelVersion.isEmpty())
        modelDirectory = QDir(modelDirectory).filePath(modelVersion);
    modelDirectory = QDir::cleanPath(modelDirectory);
    const QString descriptorPath = QDir(modelDirectory).filePath(QStringLiteral("descriptor.json"));

    QFile descriptorFile(descriptorPath);
    if (!descriptorFile.open(QIODevice::ReadOnly)) {
        return errorResult(
                    QStringLiteral("dl_feature_model_missing"),
                    QStringLiteral("Feature model descriptor is not readable: %1").arg(descriptorFile.errorString()),
                    m_featureModelRoot,
                    descriptorPath);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(descriptorFile.readAll(), &parseError);
    descriptorFile.close();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return errorResult(
                    QStringLiteral("dl_feature_model_descriptor_invalid"),
                    QStringLiteral("Feature model descriptor is not a valid JSON object: %1")
                    .arg(parseError.errorString()),
                    m_featureModelRoot,
                    descriptorPath);
    }

    const QJsonObject json = document.object();
    const QJsonValue schemaVersionValue = json.value(QStringLiteral("schemaVersion"));
    if (!isExactInteger(schemaVersionValue) || schemaVersionValue.toInt() != 1) {
        return errorResult(
                    QStringLiteral("dl_feature_model_descriptor_invalid"),
                    QStringLiteral("Only feature model descriptor schemaVersion=1 is supported."),
                    m_featureModelRoot,
                    descriptorPath);
    }

    RegisteredClassificationEmbeddingModelDescriptor descriptor;
    descriptor.schemaVersion = 1;
    descriptor.modelId = requiredString(json, QStringLiteral("modelId"));
    descriptor.modelVersion = requiredString(json, QStringLiteral("modelVersion"));
    const QString featureModelFile = requiredString(json, QStringLiteral("featureModelFile"));
    descriptor.modelSha256 = requiredString(json, QStringLiteral("featureModelSha256")).toLower();
    const QString preprocessFile = requiredString(json, QStringLiteral("preprocessFile"));
    descriptor.preprocessSha256 = requiredString(json, QStringLiteral("preprocessSha256")).toLower();
    descriptor.embeddingLayer = requiredString(json, QStringLiteral("embeddingLayer"));
    descriptor.resizeMode = requiredString(json, QStringLiteral("resizeMode"));
    descriptor.runtimePreference = requiredString(json, QStringLiteral("runtimePreference")).toLower();

    const QJsonValue embeddingLength = json.value(QStringLiteral("embeddingLength"));
    const QJsonValue inputWidth = json.value(QStringLiteral("inputWidth"));
    const QJsonValue inputHeight = json.value(QStringLiteral("inputHeight"));
    const QJsonValue inputChannels = json.value(QStringLiteral("inputChannels"));
    const QJsonValue paddingValue = json.value(QStringLiteral("paddingValue"));
    if (isExactInteger(embeddingLength))
        descriptor.embeddingLength = embeddingLength.toInt();
    if (isExactInteger(inputWidth))
        descriptor.inputWidth = inputWidth.toInt();
    if (isExactInteger(inputHeight))
        descriptor.inputHeight = inputHeight.toInt();
    if (isExactInteger(inputChannels))
        descriptor.inputChannels = inputChannels.toInt();
    if (paddingValue.isDouble())
        descriptor.paddingValue = paddingValue.toDouble();

    const bool descriptorFieldsValid =
            isSafePathComponent(descriptor.modelId)
            && isSafePathComponent(descriptor.modelVersion)
            && isSafeLeafFileName(featureModelFile)
            && featureModelFile.endsWith(QStringLiteral(".hdl"), Qt::CaseInsensitive)
            && isSha256(descriptor.modelSha256)
            && isSafeLeafFileName(preprocessFile)
            && preprocessFile.endsWith(QStringLiteral(".hdict"), Qt::CaseInsensitive)
            && isSha256(descriptor.preprocessSha256)
            && !descriptor.embeddingLayer.isEmpty()
            && descriptor.embeddingLength > 0
            && descriptor.inputWidth > 0
            && descriptor.inputHeight > 0
            && (descriptor.inputChannels == 1 || descriptor.inputChannels == 3)
            && descriptor.resizeMode == QStringLiteral("keep_aspect_pad")
            && std::isfinite(descriptor.paddingValue)
            && descriptor.paddingValue >= 0.0
            && descriptor.paddingValue <= 255.0
            && (descriptor.runtimePreference == QStringLiteral("auto")
                || descriptor.runtimePreference == QStringLiteral("cpu")
                || descriptor.runtimePreference == QStringLiteral("gpu"));
    if (!descriptorFieldsValid) {
        return errorResult(
                    QStringLiteral("dl_feature_model_descriptor_invalid"),
                    QStringLiteral("Feature model descriptor fields do not satisfy the schema 1 contract."),
                    m_featureModelRoot,
                    descriptorPath);
    }
    if (descriptor.modelId != modelId
            || (!modelVersion.isEmpty() && descriptor.modelVersion != modelVersion)) {
        return errorResult(
                    QStringLiteral("dl_feature_model_descriptor_mismatch"),
                    QStringLiteral("Feature model descriptor id or version does not match the requested model."),
                    m_featureModelRoot,
                    descriptorPath);
    }

    descriptor.modelPath = QDir(modelDirectory).filePath(featureModelFile);
    descriptor.preprocessPath = QDir(modelDirectory).filePath(preprocessFile);
    if (!QFileInfo(descriptor.modelPath).isReadable()) {
        return errorResult(
                    QStringLiteral("dl_feature_model_missing"),
                    QStringLiteral("Feature model .hdl file is not readable."),
                    m_featureModelRoot,
                    descriptorPath);
    }
    if (!QFileInfo(descriptor.preprocessPath).isReadable()) {
        return errorResult(
                    QStringLiteral("dl_preprocess_missing"),
                    QStringLiteral("Feature model preprocess .hdict file is not readable."),
                    m_featureModelRoot,
                    descriptorPath);
    }

    QString hashError;
    const QString actualModelHash = fileSha256(descriptor.modelPath, &hashError);
    if (actualModelHash.isEmpty()) {
        return errorResult(
                    QStringLiteral("dl_feature_model_read_failed"),
                    QStringLiteral("Failed to hash feature model: %1").arg(hashError),
                    m_featureModelRoot,
                    descriptorPath);
    }
    if (actualModelHash != descriptor.modelSha256) {
        return errorResult(
                    QStringLiteral("dl_feature_model_hash_mismatch"),
                    QStringLiteral("Feature model SHA-256 does not match descriptor."),
                    m_featureModelRoot,
                    descriptorPath);
    }
    const QString actualPreprocessHash = fileSha256(descriptor.preprocessPath, &hashError);
    if (actualPreprocessHash.isEmpty()) {
        return errorResult(
                    QStringLiteral("dl_preprocess_read_failed"),
                    QStringLiteral("Failed to hash preprocess file: %1").arg(hashError),
                    m_featureModelRoot,
                    descriptorPath);
    }
    if (actualPreprocessHash != descriptor.preprocessSha256) {
        return errorResult(
                    QStringLiteral("dl_preprocess_hash_mismatch"),
                    QStringLiteral("Preprocess SHA-256 does not match descriptor."),
                    m_featureModelRoot,
                    descriptorPath);
    }

    RegisteredClassificationEmbeddingModelResolveResult result;
    result.success = true;
    result.status = QStringLiteral("ok");
    result.message = QStringLiteral("HALCON embedding feature model descriptor resolved.");
    result.descriptor = descriptor;
    result.payload.insert(QStringLiteral("featureModelRoot"), m_featureModelRoot);
    result.payload.insert(QStringLiteral("descriptorPath"), descriptorPath);
    result.payload.insert(QStringLiteral("modelId"), descriptor.modelId);
    result.payload.insert(QStringLiteral("modelVersion"), descriptor.modelVersion);
    result.payload.insert(QStringLiteral("modelPath"), descriptor.modelPath);
    result.payload.insert(QStringLiteral("modelSha256"), descriptor.modelSha256);
    result.payload.insert(QStringLiteral("preprocessPath"), descriptor.preprocessPath);
    result.payload.insert(QStringLiteral("preprocessSha256"), descriptor.preprocessSha256);
    result.payload.insert(QStringLiteral("embeddingLayer"), descriptor.embeddingLayer);
    result.payload.insert(QStringLiteral("embeddingLength"), descriptor.embeddingLength);
    result.payload.insert(QStringLiteral("inputWidth"), descriptor.inputWidth);
    result.payload.insert(QStringLiteral("inputHeight"), descriptor.inputHeight);
    result.payload.insert(QStringLiteral("inputChannels"), descriptor.inputChannels);
    result.payload.insert(QStringLiteral("resizeMode"), descriptor.resizeMode);
    result.payload.insert(QStringLiteral("paddingValue"), descriptor.paddingValue);
    result.payload.insert(QStringLiteral("runtimePreference"), descriptor.runtimePreference);
    return result;
}

QString RegisteredClassificationEmbeddingModelProvider::featureModelRoot() const
{
    return m_featureModelRoot;
}

QString RegisteredClassificationEmbeddingModelProvider::defaultFeatureModelRoot()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(
                QStringLiteral("ModelFiles/RegisteredClass/_feature_models"));
}
