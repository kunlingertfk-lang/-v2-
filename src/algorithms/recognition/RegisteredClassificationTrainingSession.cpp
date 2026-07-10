#include "algorithms/recognition/RegisteredClassificationTrainingSession.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>

#include <opencv2/imgproc.hpp>

namespace {

RegisteredClassificationTrainingSessionResult failure(const QString &status,
                                                       const QString &message)
{
    RegisteredClassificationTrainingSessionResult result;
    result.status = status;
    result.message = message;
    return result;
}

RegisteredClassificationTrainingSessionResult success()
{
    RegisteredClassificationTrainingSessionResult result;
    result.success = true;
    result.status = QStringLiteral("ok");
    return result;
}

bool validRelativePath(const QString &path)
{
    if (path.trimmed().isEmpty() || QDir::isAbsolutePath(path))
        return false;
    const QString clean = QDir::cleanPath(path);
    return clean != QStringLiteral("..") && !clean.startsWith(QStringLiteral("../"))
            && !clean.startsWith(QStringLiteral("..%1").arg(QDir::separator()));
}

QString absoluteSessionPath(const QString &sessionRoot, const QString &relativePath)
{
    return QDir(sessionRoot).absoluteFilePath(QDir::cleanPath(relativePath));
}

bool isInsideRoot(const QString &rootPath, const QString &filePath)
{
    const QString root = QDir::cleanPath(QFileInfo(rootPath).canonicalFilePath());
    const QString file = QDir::cleanPath(QFileInfo(filePath).canonicalFilePath());
    if (root.isEmpty() || file.isEmpty())
        return false;
    return file == root || file.startsWith(root + QDir::separator());
}

QImage imageToRgb(const cv::Mat &image)
{
    if (image.empty())
        return QImage();
    if (image.type() == CV_8UC1) {
        QImage gray(image.data, image.cols, image.rows,
                    static_cast<int>(image.step), QImage::Format_Grayscale8);
        return gray.copy();
    }
    if (image.type() != CV_8UC3)
        return QImage();
    cv::Mat rgb;
    cv::cvtColor(image, rgb, cv::COLOR_BGR2RGB);
    QImage result(rgb.data, rgb.cols, rgb.rows,
                  static_cast<int>(rgb.step), QImage::Format_RGB888);
    return result.copy();
}

cv::Mat imageToBgr(const QImage &image)
{
    const QImage converted = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgb(converted.height(), converted.width(), CV_8UC3,
                const_cast<uchar *>(converted.constBits()),
                static_cast<size_t>(converted.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    return bgr.clone();
}

bool imageManifestPaths(const QJsonObject &manifest, QStringList *paths, QString *error)
{
    const QJsonArray images = manifest.value(QStringLiteral("images")).toArray();
    if (images.isEmpty()) {
        if (error)
            *error = QStringLiteral("Training session images must not be empty.");
        return false;
    }
    QSet<QString> uniquePaths;
    for (const QJsonValue &value : images) {
        const QString path = value.toObject().value(QStringLiteral("relativePath")).toString();
        if (!validRelativePath(path) || !path.startsWith(QStringLiteral("images/"))) {
            if (error)
                *error = QStringLiteral("Training session image path is invalid.");
            return false;
        }
        if (uniquePaths.contains(path)) {
            if (error)
                *error = QStringLiteral("Training session image paths must be unique.");
            return false;
        }
        uniquePaths.insert(path);
        if (paths)
            paths->append(path);
    }
    return true;
}

} // namespace

RegisteredClassificationTrainingSessionResult writeRegisteredClassificationTrainingSession(
        const QString &sessionRoot,
        const QJsonObject &manifest,
        const QVector<RegisteredClassificationTrainingSessionAsset> &assets)
{
    if (manifest.value(QStringLiteral("schemaVersion")).toInt() != 1)
        return failure(QStringLiteral("unsupported_session_version"),
                       QStringLiteral("Training session schema version is unsupported."));

    QStringList manifestPaths;
    QString validationError;
    if (!imageManifestPaths(manifest, &manifestPaths, &validationError))
        return failure(QStringLiteral("invalid_session_manifest"), validationError);
    if (manifestPaths.size() != assets.size())
        return failure(QStringLiteral("session_asset_count_mismatch"),
                       QStringLiteral("Training session manifest and assets differ."));

    QSet<QString> assetPaths;
    const QDir root(sessionRoot);
    if (!QDir().mkpath(root.filePath(QStringLiteral("images"))))
        return failure(QStringLiteral("session_directory_create_failed"),
                       QStringLiteral("Could not create training session directory."));
    for (const RegisteredClassificationTrainingSessionAsset &asset : assets) {
        if (!validRelativePath(asset.relativePath) || !asset.relativePath.startsWith(QStringLiteral("images/")))
            return failure(QStringLiteral("invalid_session_path"),
                           QStringLiteral("Training session asset path escapes the session directory."));
        if (assetPaths.contains(asset.relativePath))
            return failure(QStringLiteral("duplicate_session_path"),
                           QStringLiteral("Training session asset paths must be unique."));
        assetPaths.insert(asset.relativePath);
        const QImage image = imageToRgb(asset.image);
        if (image.isNull())
            return failure(QStringLiteral("invalid_session_image"),
                           QStringLiteral("Training session contains an invalid image."));
        const QString outputPath = absoluteSessionPath(sessionRoot, asset.relativePath);
        if (!image.save(outputPath, "PNG"))
            return failure(QStringLiteral("session_image_write_failed"),
                           QStringLiteral("Could not write training session image."));
    }

    for (const QString &path : manifestPaths) {
        if (!assetPaths.contains(path))
            return failure(QStringLiteral("session_asset_missing"),
                           QStringLiteral("Training session manifest references a missing asset."));
    }

    QSaveFile file(root.filePath(QStringLiteral("session.json")));
    if (!file.open(QIODevice::WriteOnly))
        return failure(QStringLiteral("session_manifest_write_failed"),
                       QStringLiteral("Could not write training session manifest."));
    file.write(QJsonDocument(manifest).toJson(QJsonDocument::Indented));
    if (!file.commit())
        return failure(QStringLiteral("session_manifest_write_failed"),
                       QStringLiteral("Could not commit training session manifest."));
    return success();
}

RegisteredClassificationTrainingSessionResult readRegisteredClassificationTrainingSession(
        const QString &sessionRoot,
        RegisteredClassificationTrainingSessionPayload *payload)
{
    if (!payload)
        return failure(QStringLiteral("invalid_argument"), QStringLiteral("Session payload is null."));
    *payload = RegisteredClassificationTrainingSessionPayload();
    const QDir root(sessionRoot);
    QFile file(root.filePath(QStringLiteral("session.json")));
    if (!file.exists())
        return failure(QStringLiteral("missing_training_session"),
                       QStringLiteral("Training model has no historical training data."));
    if (!file.open(QIODevice::ReadOnly))
        return failure(QStringLiteral("session_manifest_read_failed"),
                       QStringLiteral("Could not read training session manifest."));
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return failure(QStringLiteral("invalid_training_session"),
                       QStringLiteral("Training session manifest is invalid JSON."));
    const QJsonObject manifest = document.object();
    if (manifest.value(QStringLiteral("schemaVersion")).toInt() != 1)
        return failure(QStringLiteral("unsupported_session_version"),
                       QStringLiteral("Training session schema version is unsupported."));

    QStringList manifestPaths;
    QString validationError;
    if (!imageManifestPaths(manifest, &manifestPaths, &validationError))
        return failure(QStringLiteral("invalid_training_session"), validationError);
    QVector<RegisteredClassificationTrainingSessionAsset> assets;
    for (const QString &relativePath : manifestPaths) {
        const QString imagePath = absoluteSessionPath(sessionRoot, relativePath);
        if (!validRelativePath(relativePath) || !isInsideRoot(sessionRoot, imagePath))
            return failure(QStringLiteral("invalid_session_path"),
                           QStringLiteral("Training session asset path escapes the session directory."));
        QImage image(imagePath);
        if (image.isNull())
            return failure(QStringLiteral("missing_session_image"),
                           QStringLiteral("Training session image is missing or unreadable."));
        RegisteredClassificationTrainingSessionAsset asset;
        asset.relativePath = relativePath;
        asset.image = imageToBgr(image);
        assets.append(asset);
    }
    payload->schemaVersion = 1;
    payload->manifest = manifest;
    payload->assets = assets;
    return success();
}
