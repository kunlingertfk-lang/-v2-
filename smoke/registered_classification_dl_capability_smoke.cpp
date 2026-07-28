#include "algorithms/recognition/RegisteredClassificationDlCapability.h"
#include "algorithms/recognition/RegisteredClassificationEmbeddingModelProvider.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

namespace {

int g_failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++g_failures;
    }
}

QString sha256(const QByteArray &bytes)
{
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(
                QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
}

bool writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(bytes) == bytes.size();
}

QJsonObject validDescriptor(const QByteArray &modelBytes, const QByteArray &preprocessBytes)
{
    return {
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("modelId"), QStringLiteral("industrial_embedding")},
        {QStringLiteral("modelVersion"), QStringLiteral("v1")},
        {QStringLiteral("featureModelFile"), QStringLiteral("feature_model.hdl")},
        {QStringLiteral("featureModelSha256"), sha256(modelBytes)},
        {QStringLiteral("preprocessFile"), QStringLiteral("preprocess_params.hdict")},
        {QStringLiteral("preprocessSha256"), sha256(preprocessBytes)},
        {QStringLiteral("embeddingLayer"), QStringLiteral("embedding")},
        {QStringLiteral("embeddingLength"), 256},
        {QStringLiteral("inputWidth"), 224},
        {QStringLiteral("inputHeight"), 224},
        {QStringLiteral("inputChannels"), 3},
        {QStringLiteral("resizeMode"), QStringLiteral("keep_aspect_pad")},
        {QStringLiteral("paddingValue"), 0.0},
        {QStringLiteral("runtimePreference"), QStringLiteral("auto")}
    };
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QString root = QDir(QDir::tempPath()).filePath(
                QStringLiteral("registered_classification_dl_capability_smoke"));
    QDir(root).removeRecursively();
    check(QDir().mkpath(root), "temporary root must be created");

    RegisteredClassificationEmbeddingModelProvider provider(root);
    RegisteredClassificationEmbeddingModelResolveResult resolved =
            provider.resolve(QStringLiteral("industrial_embedding"), QStringLiteral("v1"));
    check(!resolved.success && resolved.status == QStringLiteral("dl_feature_model_missing"),
          "missing descriptor must return dl_feature_model_missing");

    resolved = provider.resolve(QStringLiteral("../escape"), QStringLiteral("v1"));
    check(!resolved.success && resolved.status == QStringLiteral("dl_feature_model_id_invalid"),
          "unsafe model id must be rejected before path access");

    const QString modelDir = QDir(root).filePath(QStringLiteral("industrial_embedding/v1"));
    check(QDir().mkpath(modelDir), "model directory must be created");
    check(writeFile(QDir(modelDir).filePath(QStringLiteral("descriptor.json")), QByteArray("{")),
          "invalid descriptor must be written");
    resolved = provider.resolve(QStringLiteral("industrial_embedding"), QStringLiteral("v1"));
    check(!resolved.success
          && resolved.status == QStringLiteral("dl_feature_model_descriptor_invalid"),
          "invalid JSON descriptor must be rejected");

    const QByteArray modelBytes("fake-halcon-hdl-for-provider-contract");
    const QByteArray preprocessBytes("fake-halcon-hdict-for-provider-contract");
    const QJsonObject descriptor = validDescriptor(modelBytes, preprocessBytes);
    check(writeFile(QDir(modelDir).filePath(QStringLiteral("descriptor.json")),
                    QJsonDocument(descriptor).toJson(QJsonDocument::Indented)),
          "valid descriptor must be written");
    resolved = provider.resolve(QStringLiteral("industrial_embedding"), QStringLiteral("v1"));
    check(!resolved.success && resolved.status == QStringLiteral("dl_feature_model_missing"),
          "missing .hdl must be reported");

    check(writeFile(QDir(modelDir).filePath(QStringLiteral("feature_model.hdl")), modelBytes),
          "fake .hdl must be written");
    resolved = provider.resolve(QStringLiteral("industrial_embedding"), QStringLiteral("v1"));
    check(!resolved.success && resolved.status == QStringLiteral("dl_preprocess_missing"),
          "missing preprocess file must be reported");

    check(writeFile(QDir(modelDir).filePath(QStringLiteral("preprocess_params.hdict")),
                    preprocessBytes),
          "fake preprocess file must be written");
    resolved = provider.resolve(QStringLiteral("industrial_embedding"), QStringLiteral("v1"));
    check(resolved.success && resolved.status == QStringLiteral("ok"),
          "valid descriptor and matching files must resolve");
    check(resolved.descriptor.embeddingLength == 256
          && resolved.descriptor.inputChannels == 3,
          "resolved descriptor must preserve embedding contract");

    check(writeFile(QDir(modelDir).filePath(QStringLiteral("feature_model.hdl")),
                    QByteArray("tampered")),
          "tampered .hdl must be written");
    resolved = provider.resolve(QStringLiteral("industrial_embedding"), QStringLiteral("v1"));
    check(!resolved.success
          && resolved.status == QStringLiteral("dl_feature_model_hash_mismatch"),
          "tampered .hdl must fail SHA-256 validation");

    const RegisteredClassificationDlCapabilityResult capability =
            RegisteredClassificationDlCapability().probe();
    check(!capability.status.isEmpty(), "DL capability probe must always return a status");
    check(capability.payload.value(QStringLiteral("halconVersion")).toString()
          == QStringLiteral("20.11.1"),
          "DL capability probe must report expected HALCON version");
    if (capability.success) {
        check(!capability.devices.isEmpty(), "successful probe must return at least one device");
        std::cout << "DL capability: available, devices="
                  << capability.devices.size() << std::endl;
        for (const RegisteredClassificationDlDeviceInfo &device : capability.devices) {
            std::cout << "  id=" << device.id
                      << ", type=" << device.type.toStdString()
                      << ", name=" << device.name.toStdString()
                      << ", inferenceOnly=" << (device.inferenceOnly ? "true" : "false")
                      << std::endl;
        }
    } else {
        std::cout << "DL capability: " << capability.status.toStdString()
                  << " | " << capability.message.toStdString() << std::endl;
    }

    QDir(root).removeRecursively();
    if (g_failures == 0)
        std::cout << "registered_classification_dl_capability_smoke: PASS" << std::endl;
    return g_failures == 0 ? 0 : 1;
}
