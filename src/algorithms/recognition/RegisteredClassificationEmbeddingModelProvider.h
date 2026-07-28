#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONEMBEDDINGMODELPROVIDER_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONEMBEDDINGMODELPROVIDER_H

#include <QJsonObject>
#include <QString>

struct RegisteredClassificationEmbeddingModelDescriptor
{
    int schemaVersion = 1;
    QString modelId;
    QString modelVersion;
    QString modelPath;
    QString modelSha256;
    QString preprocessPath;
    QString preprocessSha256;
    QString embeddingLayer;
    int embeddingLength = 0;
    int inputWidth = 0;
    int inputHeight = 0;
    int inputChannels = 0;
    QString resizeMode = QStringLiteral("keep_aspect_pad");
    double paddingValue = 0.0;
    QString runtimePreference = QStringLiteral("auto");
};

struct RegisteredClassificationEmbeddingModelResolveResult
{
    bool success = false;
    QString status;
    QString message;
    RegisteredClassificationEmbeddingModelDescriptor descriptor;
    QJsonObject payload;
};

class RegisteredClassificationEmbeddingModelProvider
{
public:
    explicit RegisteredClassificationEmbeddingModelProvider(
            const QString &featureModelRoot = QString());
    virtual ~RegisteredClassificationEmbeddingModelProvider() = default;

    virtual RegisteredClassificationEmbeddingModelResolveResult resolve(
            const QString &modelId,
            const QString &modelVersion = QString()) const;

    QString featureModelRoot() const;
    static QString defaultFeatureModelRoot();

private:
    QString m_featureModelRoot;
};

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONEMBEDDINGMODELPROVIDER_H
