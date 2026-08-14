#include "SchemeStore.h"

#include <algorithm>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStringList>
#include <QUuid>

#include <opencv2/imgcodecs.hpp>

#include <utility>

#include "frame/ReferenceImageProvider.h"
#include "algorithms/location/TemplateLocationConfig.h"

namespace {

constexpr auto kProjectsDirName = "projects";
constexpr auto kSchemeJsonName = "scheme.json";
constexpr auto kProjectRootOverrideEnvironment =
        "ZNXJ_SCHEME_STORE_PROJECT_ROOT";

bool exactReferenceBankVersion(const QJsonValue &value, int *version = nullptr)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (number != 4.0 && number != 5.0)
        return false;
    if (version)
        *version = static_cast<int>(number);
    return true;
}

void invalidateReferenceLocatorForImageChange(
        ReferencePositionCorrectionConfig *config)
{
    if (!config)
        return;

    config->referenceCreated = false;
    config->referencePose = QJsonObject();
    config->referencePosesByTemplateId = QJsonObject();
    config->score = 0.0;
    config->elapsedMs = 0;
    config->status = QStringLiteral("reference_image_changed");
    config->message = QStringLiteral(
                "Reference image changed; rebuild all enabled reference templates.");

    QJsonObject locator = config->locator;
    const bool locatorObjectWasPresent = config->extra.contains(
                QStringLiteral("locator"));
    bool outerVersionEncodingSupported = true;
    if (config->extra.contains(QStringLiteral("version"))) {
        outerVersionEncodingSupported = exactReferenceBankVersion(
                    config->extra.value(QStringLiteral("version")));
    }
    const TemplateLocationConfig::EnvelopeInspection locatorEnvelope =
            TemplateLocationConfig::inspectEnvelope(locator);
    const int locatorVersion = locatorEnvelope.version;
    const bool poseMapEncodingSupported =
            !config->extra.contains(QStringLiteral("referencePosesByTemplateId"))
            || config->extra.value(QStringLiteral("referencePosesByTemplateId"))
            .isObject();
    const bool knownOuterEnvelope = config->version == 4
            || config->version == 5;
    const bool knownLocator = knownOuterEnvelope && !locator.isEmpty()
            && outerVersionEncodingSupported
            && locatorEnvelope.supported
            && poseMapEncodingSupported;
    if (knownLocator) {
        if (locatorVersion == TemplateLocationConfig::ModelBankParamsVersion
                || locatorVersion ==
                   TemplateLocationConfig::CompositeBankParamsVersion) {
            QJsonArray templates = locator.value(
                        QStringLiteral("templates")).toArray();
            for (int index = 0; index < templates.size(); ++index) {
                if (!templates.at(index).isObject())
                    continue;
                QJsonObject item = templates.at(index).toObject();
                item.insert(QStringLiteral("modelCreated"), false);
                templates.replace(index, item);
            }
            locator.insert(QStringLiteral("templates"), templates);
        } else {
            // A nested v4 locator is a single flat template.
            locator.insert(QStringLiteral("modelCreated"), false);
        }
        config->locator = locator;
        // The v5 reference envelope is required when its nested locator binds
        // templates to stable Base IDs. Legacy v4/v5 locator banks keep the
        // established v4 outer envelope.
        config->version = locatorVersion ==
                TemplateLocationConfig::CompositeBankParamsVersion ? 5 : 4;
    }

    // Keep config.extra coherent as well.  This is important for malformed or
    // future read-only envelopes whose raw fields are otherwise round-tripped.
    config->extra.insert(QStringLiteral("referenceCreated"), false);
    config->extra.insert(QStringLiteral("referencePose"), QJsonObject());
    config->extra.insert(QStringLiteral("referencePosesByTemplateId"),
                         QJsonObject());
    config->extra.insert(QStringLiteral("status"), config->status);
    config->extra.insert(QStringLiteral("message"), config->message);
    config->extra.insert(QStringLiteral("score"), 0.0);
    config->extra.insert(QStringLiteral("elapsedMs"), 0.0);
    if (knownLocator)
        config->extra.insert(QStringLiteral("locator"), locator);
    else if (locatorObjectWasPresent)
        config->locator = config->extra.value(QStringLiteral("locator"))
                .toObject();
}

void markReferenceLocatorChanged(ReferencePositionCorrectionConfig *config,
                                 const QString &message)
{
    if (!config)
        return;
    config->referenceCreated = false;
    config->referencePose = QJsonObject();
    config->score = 0.0;
    config->elapsedMs = 0;
    config->status = QStringLiteral("reference_image_changed");
    config->message = message;
    config->extra.insert(QStringLiteral("referenceCreated"), false);
    config->extra.insert(QStringLiteral("referencePose"), QJsonObject());
    config->extra.insert(QStringLiteral("referencePosesByTemplateId"),
                         config->referencePosesByTemplateId);
    config->extra.insert(QStringLiteral("status"), config->status);
    config->extra.insert(QStringLiteral("message"), config->message);
    config->extra.insert(QStringLiteral("score"), 0.0);
    config->extra.insert(QStringLiteral("elapsedMs"), 0.0);
}

bool invalidateReferenceLocatorDependencies(
        ReferencePositionCorrectionConfig *config,
        const QString &baseId,
        bool legacyPrimaryChanged,
        bool v6PixelsChanged)
{
    if (!config)
        return false;
    const TemplateLocationConfig::EnvelopeInspection envelope =
            TemplateLocationConfig::inspectEnvelope(config->locator);
    if (envelope.supported
            && envelope.version ==
               TemplateLocationConfig::CompositeBankParamsVersion) {
        // Changing the legacy primary pointer does not change the immutable
        // Base selected by a v6 locator. Only pixel replacement of a Base that
        // the bank actually references invalidates its cached model/pose.
        if (!v6PixelsChanged)
            return false;
        TemplateLocationModelBankConfig bank =
                TemplateLocationConfig::fromToolParams(
                    config->locator, PositionCorrection::defaultSourceId());
        if (!bank.decodeSupported || bank.rawPassthrough)
            return false;
        bool affected = false;
        for (TemplateLocationTemplateConfig &item : bank.templates) {
            if (item.sourceBaseId != baseId)
                continue;
            item.modelCreated = false;
            config->referencePosesByTemplateId.remove(item.templateId);
            affected = true;
        }
        if (!affected)
            return false;
        config->locator = TemplateLocationConfig::toToolParams(bank);
        config->version = 5;
        config->extra.insert(QStringLiteral("version"), 5);
        config->extra.insert(QStringLiteral("locator"), config->locator);
        markReferenceLocatorChanged(
                    config,
                    QStringLiteral("Reference Base '%1' changed; rebuild its bound templates.")
                    .arg(baseId));
        return true;
    }
    if (legacyPrimaryChanged) {
        invalidateReferenceLocatorForImageChange(config);
        return true;
    }
    return false;
}

bool invalidateToolLocatorDependencies(ToolConfig *tool,
                                       const QString &baseId,
                                       bool legacyPrimaryChanged,
                                       bool v6PixelsChanged)
{
    if (!tool || tool->toolType != ToolType::TemplateLocation)
        return false;
    TemplateLocationModelBankConfig bank =
            TemplateLocationConfig::fromToolConfig(*tool);
    if (!bank.decodeSupported || bank.rawPassthrough)
        return false;
    bool affected = false;
    if (bank.version == TemplateLocationConfig::CompositeBankParamsVersion) {
        if (!v6PixelsChanged)
            return false;
        for (TemplateLocationTemplateConfig &item : bank.templates) {
            if (item.sourceBaseId != baseId)
                continue;
            item.modelCreated = false;
            affected = true;
        }
    } else if (legacyPrimaryChanged) {
        for (TemplateLocationTemplateConfig &item : bank.templates)
            item.modelCreated = false;
        affected = !bank.templates.isEmpty();
    }
    if (affected)
        tool->params = TemplateLocationConfig::toToolParams(bank);
    return affected;
}

void invalidateSchemeLocatorDependencies(SchemeState *state,
                                         const QString &baseId,
                                         bool legacyPrimaryChanged,
                                         bool v6PixelsChanged)
{
    if (!state)
        return;
    invalidateReferenceLocatorDependencies(
                &state->referencePositionCorrection, baseId,
                legacyPrimaryChanged, v6PixelsChanged);
    for (ToolConfig &tool : state->toolConfigs) {
        if (!invalidateToolLocatorDependencies(
                    &tool, baseId, legacyPrimaryChanged,
                    v6PixelsChanged)) {
            continue;
        }
        state->referencePreviewSnapshots.remove(tool.toolId);
    }
}

bool jsonReferencesBaseId(const QJsonValue &value, const QString &baseId)
{
    if (value.isArray()) {
        for (const QJsonValue &item : value.toArray()) {
            if (jsonReferencesBaseId(item, baseId))
                return true;
        }
        return false;
    }
    if (!value.isObject())
        return false;
    const QJsonObject object = value.toObject();
    if (object.value(QStringLiteral("sourceBaseId")).toString().trimmed()
            == baseId) {
        return true;
    }
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (jsonReferencesBaseId(it.value(), baseId))
            return true;
    }
    return false;
}

bool schemeReferencesBaseId(const SchemeState &state, const QString &baseId)
{
    if (jsonReferencesBaseId(
                PositionCorrection::referenceToJson(
                    state.referencePositionCorrection), baseId)) {
        return true;
    }
    for (const ToolConfig &tool : state.toolConfigs) {
        if (jsonReferencesBaseId(tool.params, baseId))
            return true;
    }
    return false;
}

QJsonArray toolConfigsToJson(const QVector<ToolConfig> &configs)
{
    QJsonArray array;
    for (const ToolConfig &config : configs)
        array.append(config.toJson());
    return array;
}

QVector<ToolConfig> toolConfigsFromJson(const QJsonArray &array)
{
    QVector<ToolConfig> configs;
    configs.reserve(array.size());
    for (const QJsonValue &value : array) {
        ToolConfig config = ToolConfig::fromJson(value.toObject());
        if (config.isValid())
            configs.append(config);
    }
    return configs;
}

// 方案保存事务的文件回滚器：提交前失败时删除本轮新建的资产和目录。
class SaveFileRollback
{
public:
    ~SaveFileRollback()
    {
        if (m_committed)
            return;
        for (auto it = m_files.crbegin(); it != m_files.crend(); ++it)
            QFile::remove(*it);
        for (auto it = m_directories.crbegin(); it != m_directories.crend(); ++it)
            QDir().rmdir(*it);
    }

    void trackFile(const QString &path) { m_files.append(path); }
    void trackDirectory(const QString &path) { m_directories.append(path); }
    void commit() { m_committed = true; }

private:
    QStringList m_files;
    QStringList m_directories;
    bool m_committed = false;
};

// 将标定转换引用的外部文件按内容摘要归档到方案目录，并重写配置/快照路径。
bool materializeCalibrationAssetsForScheme(SchemeState *scheme,
                                           SaveFileRollback *rollback,
                                           QString *errorMessage)
{
    if (!scheme || !rollback)
        return false;

    const QString assetDirectoryPath = QDir(scheme->schemeDir)
            .filePath(QStringLiteral("calibrations"));
    QDir targetAssetDir(assetDirectoryPath);
    const QString targetAssetRoot = QDir::cleanPath(
                QFileInfo(assetDirectoryPath).absoluteFilePath()) + QDir::separator();
    QMap<QString, QString> materializedPaths;

    auto materialize = [&](const QString &sourceValue, QString *targetPath) -> bool {
        const QString trimmed = sourceValue.trimmed();
        if (trimmed.isEmpty()) {
            if (targetPath)
                targetPath->clear();
            return true;
        }
        const QString sourcePath = QDir::cleanPath(
                    QFileInfo(trimmed).absoluteFilePath());
        const QString cached = materializedPaths.value(sourcePath);
        if (!cached.isEmpty()) {
            if (targetPath)
                *targetPath = cached;
            return true;
        }

        const QFileInfo sourceInfo(sourcePath);
        if (!sourceInfo.exists() || !sourceInfo.isFile() || !sourceInfo.isReadable()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("标定资产不存在或不可读: %1").arg(sourcePath);
            return false;
        }
        if (sourcePath.startsWith(targetAssetRoot)) {
            materializedPaths.insert(sourcePath, sourcePath);
            if (targetPath)
                *targetPath = sourcePath;
            return true;
        }

        const bool assetDirectoryExisted = targetAssetDir.exists();
        if (!assetDirectoryExisted && !QDir().mkpath(assetDirectoryPath)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("无法创建方案标定资产目录");
            return false;
        }
        if (!assetDirectoryExisted) {
            rollback->trackDirectory(QDir::cleanPath(
                                         QFileInfo(assetDirectoryPath).absoluteFilePath()));
            targetAssetDir = QDir(assetDirectoryPath);
        }

        QFile sourceFile(sourcePath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("无法读取标定资产: %1").arg(sourcePath);
            return false;
        }
        const QByteArray sourceBytes = sourceFile.readAll();
        sourceFile.close();
        const QByteArray digest = QCryptographicHash::hash(
                    sourceBytes, QCryptographicHash::Sha256).toHex();
        const QString suffix = sourceInfo.suffix().trimmed();
        if (suffix.isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("标定资产缺少文件扩展名: %1").arg(sourcePath);
            return false;
        }
        const QString baseName = sourceInfo.completeBaseName().trimmed().isEmpty()
                ? QStringLiteral("calibration") : sourceInfo.completeBaseName();
        const QString destination = targetAssetDir.filePath(
                    QStringLiteral("%1_%2.%3")
                    .arg(baseName, QString::fromLatin1(digest.left(12)), suffix));
        if (QFileInfo::exists(destination)) {
            QFile existing(destination);
            if (!existing.open(QIODevice::ReadOnly)
                    || QCryptographicHash::hash(existing.readAll(),
                                                QCryptographicHash::Sha256).toHex()
                    != digest) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("方案内同名标定资产内容冲突: %1")
                            .arg(destination);
                return false;
            }
        } else {
            if (!QFile::copy(sourcePath, destination)) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("无法复制标定资产: %1").arg(sourcePath);
                return false;
            }
            rollback->trackFile(destination);
        }
        materializedPaths.insert(sourcePath, destination);
        if (targetPath)
            *targetPath = destination;
        return true;
    };

    for (ToolConfig &tool : scheme->toolConfigs) {
        if (tool.toolType != ToolType::CalibrationTransform)
            continue;
        QJsonObject transform = tool.params.value(QStringLiteral("calibrationTransform")).toObject();
        QJsonArray rewrittenFiles;
        const QJsonArray files = transform.value(QStringLiteral("calibrationFiles")).toArray();
        for (const QJsonValue &value : files) {
            QString targetPath;
            if (!materialize(value.toString(), &targetPath))
                return false;
            if (targetPath.isEmpty())
                continue;
            if (!rewrittenFiles.contains(targetPath))
                rewrittenFiles.append(targetPath);
        }
        const QString activeValue = transform.value(
                    QStringLiteral("activeCalibrationFile")).toString().trimmed();
        QString activeTarget;
        if (!materialize(activeValue, &activeTarget))
            return false;
        if (!activeTarget.isEmpty() && !rewrittenFiles.contains(activeTarget))
            rewrittenFiles.append(activeTarget);
        transform.insert(QStringLiteral("calibrationFiles"), rewrittenFiles);
        transform.insert(QStringLiteral("activeCalibrationFile"), activeTarget);
        tool.params.insert(QStringLiteral("calibrationTransform"), transform);

        auto snapshotIt = scheme->referencePreviewSnapshots.find(tool.toolId);
        if (snapshotIt != scheme->referencePreviewSnapshots.end() && snapshotIt->valid) {
            QString previewFile = snapshotIt->result.payload
                    .value(QStringLiteral("calibrationFile")).toString().trimmed();
            if (!previewFile.isEmpty()) {
                const QString previewSource = QDir::cleanPath(
                            QFileInfo(previewFile).absoluteFilePath());
                const QString previewTarget = materializedPaths.value(previewSource);
                if (!previewTarget.isEmpty()) {
                    snapshotIt->result.payload.insert(QStringLiteral("calibrationFile"),
                                                      previewTarget);
                    snapshotIt->payload.insert(QStringLiteral("calibrationFile"),
                                               previewTarget);
                }
            }
        }
    }
    return true;
}

QJsonObject previewSnapshotsToJson(const QMap<QString, ToolPreviewSnapshot> &snapshots)
{
    QJsonObject object;
    for (auto it = snapshots.cbegin(); it != snapshots.cend(); ++it) {
        if (!it.value().valid)
            continue;
        object.insert(it.key(), toolPreviewSnapshotToJson(it.value()));
    }
    return object;
}

QMap<QString, ToolPreviewSnapshot> previewSnapshotsFromJson(const QJsonObject &object)
{
    QMap<QString, ToolPreviewSnapshot> snapshots;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        ToolPreviewSnapshot snapshot = toolPreviewSnapshotFromJson(it.value().toObject());
        if (!snapshot.valid)
            continue;
        const QString storageKey = it.key().trimmed().isEmpty()
                ? snapshot.toolId.trimmed() : it.key().trimmed();
        if (storageKey.isEmpty())
            continue;
        snapshots.insert(storageKey, snapshot);
    }
    return snapshots;
}

void normalizeSnapshotsForTools(SchemeState *state)
{
    if (!state)
        return;
    QMap<QString, ToolPreviewSnapshot> normalizedSnapshots;
    for (const ToolConfig &config : std::as_const(state->toolConfigs)) {
        ToolPreviewSnapshot snapshot = state->referencePreviewSnapshots.value(config.toolId);
        if (!snapshot.valid)
            continue;
        snapshot.toolId = config.toolId;
        snapshot.toolType = config.toolType;
        snapshot.result.toolId = config.toolId;
        snapshot.result.toolType = config.toolType;
        normalizedSnapshots.insert(config.toolId, snapshot);
    }
    state->referencePreviewSnapshots = normalizedSnapshots;
}

QString sanitizedSchemeId(QString id)
{
    id = id.trimmed();
    for (QChar &ch : id) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('_') && ch != QLatin1Char('-'))
            ch = QLatin1Char('_');
    }
    return id;
}

QString cleanAbsolutePath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString bytesRevision(const QByteArray &bytes)
{
    if (bytes.isEmpty())
        return QString();
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(
                QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
}

QString fileRevision(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    return bytesRevision(file.readAll());
}

bool encodeReferencePng(const cv::Mat &frame,
                        QByteArray *bytes,
                        QString *errorMessage)
{
    if (!bytes)
        return false;
    try {
        std::vector<uchar> encoded;
        if (!cv::imencode(".png", frame, encoded) || encoded.empty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("无法编码参考图 PNG");
            return false;
        }
        *bytes = QByteArray(reinterpret_cast<const char *>(encoded.data()),
                            static_cast<int>(encoded.size()));
        return true;
    } catch (const cv::Exception &exception) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("编码参考图异常: %1")
                    .arg(QString::fromLocal8Bit(exception.what()));
        }
        return false;
    }
}

QString immutableReferenceFileName(const QString &baseId,
                                   const QString &contentRevision)
{
    QString digest = contentRevision;
    if (digest.startsWith(QStringLiteral("sha256:")))
        digest.remove(0, 7);
    return QStringLiteral("reference_%1_%2.png").arg(baseId, digest);
}

bool isSafeReferenceFileName(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty())
        return true;
    if (QFileInfo(trimmed).isAbsolute())
        return false;

    const QString cleaned = QDir::cleanPath(trimmed);
    return cleaned != QStringLiteral(".")
            && cleaned != QStringLiteral("..")
            && !cleaned.startsWith(QStringLiteral("../"))
            && QFileInfo(cleaned).fileName() == cleaned;
}

bool validateSchemeDirectory(const QString &rootPath,
                             const QString &schemeId,
                             const QString &schemeDir,
                             bool mustExist,
                             QString *error)
{
    const QString expected =
            cleanAbsolutePath(QDir(rootPath).filePath(schemeId));
    const QString actual = cleanAbsolutePath(schemeDir);
    if (actual != expected) {
        if (error) {
            *error = QStringLiteral("方案目录越界: %1（应为 %2）")
                    .arg(actual, expected);
        }
        return false;
    }

    const QFileInfo dirInfo(actual);
    if (mustExist && (!dirInfo.exists() || !dirInfo.isDir())) {
        if (error)
            *error = QStringLiteral("方案目录不存在: %1").arg(actual);
        return false;
    }
    if (dirInfo.exists() && dirInfo.isSymLink()) {
        if (error)
            *error = QStringLiteral("方案目录不允许使用符号链接: %1").arg(actual);
        return false;
    }

    const QFileInfo rootInfo(rootPath);
    if (rootInfo.exists() && dirInfo.exists()
            && cleanAbsolutePath(dirInfo.dir().absolutePath())
                    != cleanAbsolutePath(rootInfo.absoluteFilePath())) {
        if (error)
            *error = QStringLiteral("方案目录不在项目根目录内: %1").arg(actual);
        return false;
    }
    return true;
}

bool validateReferencePath(const QString &schemeDir,
                           const QString &referencePath,
                           bool mustExist,
                           QString *error)
{
    if (!isSafeReferenceFileName(referencePath)) {
        if (error)
            *error = QStringLiteral("基准图路径越界或格式非法: %1").arg(referencePath);
        return false;
    }
    if (referencePath.trimmed().isEmpty())
        return true;

    const QString absolutePath =
            cleanAbsolutePath(QDir(schemeDir).filePath(referencePath));
    const QString expectedParent = cleanAbsolutePath(schemeDir);
    const QFileInfo referenceInfo(absolutePath);
    if (cleanAbsolutePath(referenceInfo.dir().absolutePath()) != expectedParent) {
        if (error)
            *error = QStringLiteral("基准图不在方案目录内: %1").arg(absolutePath);
        return false;
    }
    if (mustExist && (!referenceInfo.exists() || !referenceInfo.isFile())) {
        if (error)
            *error = QStringLiteral("方案基准图不存在: %1").arg(absolutePath);
        return false;
    }
    if (referenceInfo.exists() && referenceInfo.isSymLink()) {
        if (error)
            *error = QStringLiteral("基准图不允许使用符号链接: %1").arg(absolutePath);
        return false;
    }
    return true;
}

} // namespace

SchemeStore &SchemeStore::instance()
{
    static SchemeStore store;
    return store;
}

bool SchemeStore::ensureLoaded(QString *errorMessage)
{
    if (m_loaded)
        return true;

    if (!refreshAvailableSchemes(errorMessage))
        return false;

    if (m_availableSchemes.isEmpty()) {
        if (!createDefaultScheme(errorMessage))
            return false;
    }

    if (m_availableSchemes.isEmpty()) {
        setError(errorMessage, QStringLiteral("没有可用方案"));
        return false;
    }

    m_currentScheme = m_availableSchemes.first();
    m_loaded = true;
    normalizeSnapshotsForCurrentTools();
    QString loadReferenceError;
    if (!loadCurrentReferenceIntoProvider(&loadReferenceError)) {
        ReferenceImageProvider::instance().clearReferenceFrames();
        qWarning() << "[SchemeStore]" << loadReferenceError;
    }
    return true;
}

bool SchemeStore::setCurrentScheme(const QString &schemeId, QString *errorMessage)
{
    if (!ensureLoaded(errorMessage))
        return false;

    if (m_currentScheme.schemeId == schemeId)
        return true;

    QString saveError;
    if (!saveCurrentScheme(&saveError)) {
        setError(errorMessage, QStringLiteral("切换方案前保存当前方案失败: %1").arg(saveError));
        return false;
    }

    SchemeState target;
    bool found = false;
    for (const SchemeState &state : std::as_const(m_availableSchemes)) {
        if (state.schemeId == schemeId) {
            target = state;
            found = true;
            break;
        }
    }

    if (!found) {
        setError(errorMessage, QStringLiteral("未找到方案: %1").arg(schemeId));
        return false;
    }

    SchemeState loadedTarget;
    const QString path = schemeJsonPath(target);
    if (!loadSchemeFromFile(path, &loadedTarget, errorMessage))
        return false;

    QList<ReferenceFrameEntry> targetEntries;
    QString prepareError;
    if (!prepareReferenceFrameEntries(loadedTarget,
                                      &targetEntries,
                                      &prepareError)) {
        setError(errorMessage,
                 QStringLiteral("目标方案的参考资产不可用，已保留当前方案: %1")
                 .arg(prepareError));
        return false;
    }

    const SchemeState previousState = m_currentScheme;
    const QList<ReferenceFrameEntry> previousEntries =
            ReferenceImageProvider::instance().referenceFrames();
    const QString previousPrimary =
            ReferenceImageProvider::instance().primaryBaseId();
    m_currentScheme = loadedTarget;
    normalizeSnapshotsForCurrentTools();
    QString publishError;
    if (!ReferenceImageProvider::instance().setReferenceFrames(
            targetEntries,
            loadedTarget.referenceAssets.primaryBaseId(),
            &publishError)) {
        m_currentScheme = previousState;
        normalizeSnapshotsForCurrentTools();
        QString restoreError;
        if (!ReferenceImageProvider::instance().setReferenceFrames(
                previousEntries, previousPrimary, &restoreError)) {
            ReferenceImageProvider::instance().clearReferenceFrames();
            publishError += QStringLiteral("；恢复旧运行态失败: %1")
                    .arg(restoreError);
        }
        setError(errorMessage,
                 QStringLiteral("目标方案运行态发布失败，已回滚方案切换: %1")
                 .arg(publishError));
        return false;
    }
    return true;
}

SchemeState SchemeStore::loadScheme(const QString &schemeId, QString *errorMessage) const
{
    const QString targetId = sanitizedSchemeId(schemeId);
    if (targetId.isEmpty()) {
        setError(errorMessage, QStringLiteral("方案 id 为空"));
        return SchemeState();
    }

    QDir root(projectsRootPath());
    if (!root.exists()) {
        setError(errorMessage, QStringLiteral("方案根目录不存在: %1").arg(root.absolutePath()));
        return SchemeState();
    }

    const QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString jsonPath = QDir(entry.absoluteFilePath()).filePath(QString::fromLatin1(kSchemeJsonName));
        if (!QFileInfo::exists(jsonPath))
            continue;

        SchemeState state;
        QString loadError;
        if (!loadSchemeFromFile(jsonPath, &state, &loadError)) {
            qWarning() << "[SchemeStore]" << loadError;
            continue;
        }

        if (state.schemeId == targetId)
            return state;
    }

    setError(errorMessage, QStringLiteral("未找到方案: %1").arg(targetId));
    return SchemeState();
}

bool SchemeStore::saveCurrentScheme(QString *errorMessage)
{
    if (!ensureLoaded(errorMessage))
        return false;

    SchemeState persistedState;
    QString rollbackLoadError;
    const bool hasPersistedState =
            loadSchemeFromFile(schemeJsonPath(m_currentScheme),
                               &persistedState,
                               &rollbackLoadError);

    SchemeState savedState;
    if (!saveSchemeToFile(m_currentScheme, errorMessage, nullptr, &savedState)) {
        if (hasPersistedState)
            m_currentScheme = persistedState;
        return false;
    }

    m_currentScheme = savedState;
    refreshAvailableSchemes(nullptr);
    return true;
}

bool SchemeStore::saveScheme(const SchemeState &state, QString *errorMessage)
{
    if (state.schemeId.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("方案 id 为空"));
        return false;
    }

    if (!saveSchemeToFile(state, errorMessage))
        return false;

    refreshAvailableSchemes(nullptr);
    return true;
}

bool SchemeStore::saveCurrentSchemeAs(const QString &schemeName, QString *errorMessage)
{
    if (!ensureLoaded(errorMessage))
        return false;

    const QString newName = schemeName.trimmed().isEmpty()
            ? QStringLiteral("方案副本")
            : schemeName.trimmed();

    SchemeState copy = m_currentScheme;
    if (copy.referenceAssets.isReadOnly()) {
        setError(errorMessage,
                 QStringLiteral("参考资产配置无法安全复制: %1")
                 .arg(copy.referenceAssets.readOnlyReason()));
        return false;
    }
    copy.schemeId = makeUniqueSchemeId();
    copy.schemeName = newName;
    copy.schemeDir = QDir(projectsRootPath()).filePath(copy.schemeId);
    copy.updatedAt = QDateTime::currentDateTime();

    SaveFileRollback rollback;
    QDir targetDir(copy.schemeDir);
    if (!targetDir.mkpath(QStringLiteral("."))) {
        setError(errorMessage,
                 QStringLiteral("无法创建方案副本目录: %1").arg(copy.schemeDir));
        return false;
    }
    rollback.trackDirectory(cleanAbsolutePath(copy.schemeDir));
    for (const ReferenceAsset &asset : copy.referenceAssets.assets()) {
        QString sourceError;
        const QString sourcePath = referenceAssetAbsolutePath(
                    m_currentScheme, asset.baseId, true, &sourceError);
        if (sourcePath.isEmpty()) {
            setError(errorMessage,
                     QStringLiteral("无法读取待复制的参考资产 %1: %2")
                     .arg(asset.baseId, sourceError));
            return false;
        }
        QFile source(sourcePath);
        if (!source.open(QIODevice::ReadOnly)) {
            setError(errorMessage,
                     QStringLiteral("无法读取待复制的参考资产: %1").arg(sourcePath));
            return false;
        }
        const QByteArray bytes = source.readAll();
        if (bytes.isEmpty()) {
            setError(errorMessage,
                     QStringLiteral("待复制的参考资产为空: %1").arg(sourcePath));
            return false;
        }
        const QString targetPath = targetDir.filePath(asset.path);
        QSaveFile target(targetPath);
        if (!target.open(QIODevice::WriteOnly)
                || target.write(bytes) != bytes.size()
                || !target.commit()) {
            target.cancelWriting();
            setError(errorMessage,
                     QStringLiteral("无法复制参考资产: %1").arg(asset.path));
            return false;
        }
        rollback.trackFile(targetPath);
    }

    copy = normalizedStateForSave(copy);
    QList<ReferenceFrameEntry> copiedEntries;
    QString prepareError;
    if (!prepareReferenceFrameEntries(copy, &copiedEntries, &prepareError)) {
        setError(errorMessage,
                 QStringLiteral("方案副本的参考资产不可用: %1")
                 .arg(prepareError));
        return false;
    }

    SchemeState savedCopy;
    if (!saveSchemeToFile(copy, errorMessage, nullptr, &savedCopy))
        return false;

    rollback.trackFile(schemeJsonPath(savedCopy));
    const SchemeState previousState = m_currentScheme;
    const QList<ReferenceFrameEntry> previousEntries =
            ReferenceImageProvider::instance().referenceFrames();
    const QString previousPrimary =
            ReferenceImageProvider::instance().primaryBaseId();
    m_currentScheme = savedCopy;
    refreshAvailableSchemes(nullptr);
    QString publishError;
    if (!ReferenceImageProvider::instance().setReferenceFrames(
            copiedEntries,
            savedCopy.referenceAssets.primaryBaseId(),
            &publishError)) {
        m_currentScheme = previousState;
        refreshAvailableSchemes(nullptr);
        QString restoreError;
        if (!ReferenceImageProvider::instance().setReferenceFrames(
                previousEntries, previousPrimary, &restoreError)) {
            ReferenceImageProvider::instance().clearReferenceFrames();
            publishError += QStringLiteral("；恢复原方案运行态失败: %1")
                    .arg(restoreError);
        }
        setError(errorMessage,
                 QStringLiteral("方案副本运行态发布失败，已保留原方案: %1")
                 .arg(publishError));
        return false;
    }
    rollback.commit();
    return true;
}

SchemeState SchemeStore::createEmptyScheme(const QString &schemeName, QString *errorMessage)
{
    const QString trimmedName = schemeName.trimmed();
    const QString defaultName = QStringLiteral("方案_%1")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));

    SchemeState state;
    state.schemeId = makeUniqueSchemeId();
    state.schemeName = trimmedName.isEmpty() ? defaultName : trimmedName;
    state.schemeDir = QDir(projectsRootPath()).filePath(state.schemeId);
    state.referenceImagePath.clear();
    state.referenceAssets = ReferenceAssetSet();
    state.referencePositionCorrection = ReferencePositionCorrectionConfig();
    state.toolConfigs.clear();
    state.referencePreviewSnapshots.clear();
    state.quickCalibrationConfig = QJsonObject();
    state.outputConfig = QJsonObject();
    state.updatedAt = QDateTime::currentDateTime();

    SchemeState savedState;
    if (!saveSchemeToFile(state, errorMessage, nullptr, &savedState))
        return SchemeState();

    refreshAvailableSchemes(nullptr);
    return savedState;
}

bool SchemeStore::schemeExists(const QString &schemeId) const
{
    QString error;
    return !loadScheme(schemeId, &error).schemeId.isEmpty();
}

const SchemeState &SchemeStore::currentScheme() const
{
    return m_currentScheme;
}

QVector<SchemeState> SchemeStore::availableSchemes() const
{
    return m_availableSchemes;
}

QList<SchemeSummary> SchemeStore::listSchemes() const
{
    QList<SchemeSummary> summaries;

    QDir root(projectsRootPath());
    if (!root.exists() && !root.mkpath(QStringLiteral("."))) {
        qWarning() << "[SchemeStore] 无法创建方案根目录:" << root.absolutePath();
        return summaries;
    }

    const QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString jsonPath = QDir(entry.absoluteFilePath()).filePath(QString::fromLatin1(kSchemeJsonName));
        if (!QFileInfo::exists(jsonPath))
            continue;

        SchemeState state;
        QString loadError;
        if (!loadSchemeFromFile(jsonPath, &state, &loadError)) {
            qWarning() << "[SchemeStore]" << loadError;
            continue;
        }

        SchemeSummary summary;
        summary.schemeId = state.schemeId;
        summary.schemeName = state.schemeName.trimmed().isEmpty() ? state.schemeId : state.schemeName;
        summary.schemeDir = state.schemeDir;
        summary.referenceImagePath = state.referenceImagePath.trimmed().isEmpty()
                ? QString()
                : QDir(state.schemeDir).filePath(state.referenceImagePath);
        summary.toolCount = state.toolConfigs.size();
        summary.updatedAt = state.updatedAt.isValid() ? state.updatedAt : QFileInfo(jsonPath).lastModified();
        summaries.append(summary);
    }

    std::sort(summaries.begin(), summaries.end(), [](const SchemeSummary &left, const SchemeSummary &right) {
        const int nameCompare = left.schemeName.localeAwareCompare(right.schemeName);
        if (nameCompare != 0)
            return nameCompare < 0;
        return left.schemeId < right.schemeId;
    });

    return summaries;
}

QString SchemeStore::projectsRootPath() const
{
    return QDir(resolveProjectRootPath()).filePath(QString::fromLatin1(kProjectsDirName));
}

QString SchemeStore::currentSchemeName() const
{
    return m_currentScheme.schemeName.trimmed().isEmpty()
            ? m_currentScheme.schemeId
            : m_currentScheme.schemeName;
}

QString SchemeStore::currentReferenceImageAbsolutePath() const
{
    if (m_currentScheme.referenceImagePath.trimmed().isEmpty())
        return QString();
    QString validationError;
    if (!validateSchemeDirectory(projectsRootPath(),
                                 m_currentScheme.schemeId,
                                 m_currentScheme.schemeDir,
                                 true,
                                 &validationError)
            || !validateReferencePath(m_currentScheme.schemeDir,
                                      m_currentScheme.referenceImagePath,
                                      true,
                                      &validationError)) {
        qWarning() << "[SchemeStore]" << validationError;
        return QString();
    }

    QDir dir(m_currentScheme.schemeDir);
    return dir.filePath(m_currentScheme.referenceImagePath);
}

QVector<ReferenceAsset> SchemeStore::referenceAssets() const
{
    return m_currentScheme.referenceAssets.assets();
}

bool SchemeStore::referenceAsset(const QString &baseId,
                                 ReferenceAsset *asset) const
{
    const ReferenceAsset *found = m_currentScheme.referenceAssets.assetById(baseId);
    if (!found)
        return false;
    if (asset)
        *asset = *found;
    return true;
}

bool SchemeStore::primaryReferenceAsset(ReferenceAsset *asset) const
{
    const ReferenceAsset *primary = m_currentScheme.referenceAssets.primaryAsset();
    if (!primary)
        return false;
    if (asset)
        *asset = *primary;
    return true;
}

QString SchemeStore::referenceAssetAbsolutePath(const QString &baseId) const
{
    return referenceAssetAbsolutePath(m_currentScheme, baseId, true, nullptr);
}

void SchemeStore::setSchemeName(const QString &schemeName)
{
    if (!ensureLoaded(nullptr))
        return;

    const QString trimmed = schemeName.trimmed();
    if (trimmed.isEmpty())
        return;

    m_currentScheme.schemeName = trimmed;
    m_currentScheme.updatedAt = QDateTime::currentDateTime();
}

void SchemeStore::setToolConfigs(const QVector<ToolConfig> &configs,
                                 const QMap<QString, ToolPreviewSnapshot> &referenceSnapshots)
{
    if (!ensureLoaded(nullptr))
        return;

    m_currentScheme.toolConfigs = configs;
    m_currentScheme.referencePreviewSnapshots.clear();
    for (const ToolConfig &config : m_currentScheme.toolConfigs) {
        const ToolPreviewSnapshot snapshot = referenceSnapshots.value(config.toolId);
        if (!snapshot.valid)
            continue;

        ToolPreviewSnapshot normalized = snapshot;
        normalized.toolId = config.toolId;
        normalized.toolType = config.toolType;
        m_currentScheme.referencePreviewSnapshots.insert(config.toolId, normalized);
    }
    m_currentScheme.updatedAt = QDateTime::currentDateTime();
    normalizeSnapshotsForCurrentTools();
}

void SchemeStore::setOutputConfig(const QJsonObject &outputConfig)
{
    if (!ensureLoaded(nullptr))
        return;

    m_currentScheme.outputConfig = outputConfig;
    m_currentScheme.updatedAt = QDateTime::currentDateTime();
}

void SchemeStore::setQuickCalibrationConfig(
        const QJsonObject &quickCalibrationConfig)
{
    if (!ensureLoaded(nullptr))
        return;

    m_currentScheme.quickCalibrationConfig = quickCalibrationConfig;
    m_currentScheme.updatedAt = QDateTime::currentDateTime();
}

void SchemeStore::setReferencePositionCorrection(
        const ReferencePositionCorrectionConfig &config)
{
    if (!ensureLoaded(nullptr))
        return;
    m_currentScheme.referencePositionCorrection = config;
    m_currentScheme.updatedAt = QDateTime::currentDateTime();
}

bool SchemeStore::setReferenceFrame(const cv::Mat &frame,
                                    QString *errorMessage,
                                    const FrameInputMetadata &metadata)
{
    if (!ensureLoaded(errorMessage))
        return false;

    if (frame.empty()) {
        setError(errorMessage, QStringLiteral("基准图为空"));
        return false;
    }

    const cv::Mat normalizedFrame =
            ReferenceImageProvider::normalizeReferenceFrame(frame);
    if (normalizedFrame.empty()) {
        setError(errorMessage, QStringLiteral("基准图格式不受支持"));
        return false;
    }

    ReferenceAsset primary;
    if (!primaryReferenceAsset(&primary)) {
        QString createdBaseId;
        return addReferenceAsset(QStringLiteral("基准图 1"),
                                 normalizedFrame,
                                 &createdBaseId,
                                 errorMessage,
                                 metadata);
    }
    return replaceReferenceAsset(primary.baseId,
                                 normalizedFrame,
                                 errorMessage,
                                 metadata);
}

bool SchemeStore::addReferenceAsset(const QString &name,
                                    const cv::Mat &frame,
                                    QString *createdBaseId,
                                    QString *errorMessage,
                                    const FrameInputMetadata &metadata)
{
    if (!ensureLoaded(errorMessage))
        return false;
    if (m_currentScheme.referenceAssets.isReadOnly()) {
        setError(errorMessage, m_currentScheme.referenceAssets.readOnlyReason());
        return false;
    }

    const cv::Mat normalizedFrame =
            ReferenceImageProvider::normalizeReferenceFrame(frame);
    if (normalizedFrame.empty()) {
        setError(errorMessage, frame.empty()
                 ? QStringLiteral("参考图为空")
                 : QStringLiteral("参考图格式不受支持"));
        return false;
    }

    QString baseId;
    if (m_currentScheme.referenceAssets.isEmpty()) {
        baseId = QStringLiteral("base0");
    } else {
        do {
            baseId = QStringLiteral("base_%1")
                    .arg(QUuid::createUuid().toString(
                             QUuid::WithoutBraces).left(8));
        } while (m_currentScheme.referenceAssets.assetById(baseId));
    }

    QByteArray pngBytes;
    if (!encodeReferencePng(normalizedFrame, &pngBytes, errorMessage))
        return false;
    const QString revision = bytesRevision(pngBytes);
    const QString source = metadata.source.trimmed().isEmpty()
            ? QStringLiteral("reference") : metadata.source;

    ReferenceAsset asset;
    asset.baseId = baseId;
    asset.name = name.trimmed().isEmpty()
            ? QStringLiteral("基准图 %1")
              .arg(m_currentScheme.referenceAssets.size() + 1)
            : name.trimmed();
    asset.path = immutableReferenceFileName(baseId, revision);
    asset.inputMetadata = FrameInputMetadata::fromMat(normalizedFrame, source);
    asset.contentRevision = revision;

    SchemeState candidate = m_currentScheme;
    const bool firstAsset = candidate.referenceAssets.isEmpty();
    if (!candidate.referenceAssets.addAsset(asset,
                                            firstAsset,
                                            errorMessage)) {
        return false;
    }
    if (!persistReferenceAssetFrameMutation(candidate,
                                            baseId,
                                            normalizedFrame,
                                            firstAsset,
                                            errorMessage)) {
        return false;
    }
    if (createdBaseId)
        *createdBaseId = baseId;
    return true;
}

bool SchemeStore::replaceReferenceAsset(const QString &baseId,
                                        const cv::Mat &frame,
                                        QString *errorMessage,
                                        const FrameInputMetadata &metadata)
{
    if (!ensureLoaded(errorMessage))
        return false;
    if (m_currentScheme.referenceAssets.isReadOnly()) {
        setError(errorMessage, m_currentScheme.referenceAssets.readOnlyReason());
        return false;
    }

    const ReferenceAsset *existing =
            m_currentScheme.referenceAssets.assetById(baseId);
    if (!existing) {
        setError(errorMessage,
                 QStringLiteral("未找到参考资产: %1").arg(baseId));
        return false;
    }
    const cv::Mat normalizedFrame =
            ReferenceImageProvider::normalizeReferenceFrame(frame);
    if (normalizedFrame.empty()) {
        setError(errorMessage, frame.empty()
                 ? QStringLiteral("参考图为空")
                 : QStringLiteral("参考图格式不受支持"));
        return false;
    }

    QByteArray pngBytes;
    if (!encodeReferencePng(normalizedFrame, &pngBytes, errorMessage))
        return false;
    const QString revision = bytesRevision(pngBytes);
    const QString source = metadata.source.trimmed().isEmpty()
            ? QStringLiteral("reference") : metadata.source;

    ReferenceAsset replacement = *existing;
    replacement.path = immutableReferenceFileName(replacement.baseId, revision);
    replacement.inputMetadata = FrameInputMetadata::fromMat(normalizedFrame, source);
    replacement.contentRevision = revision;

    SchemeState candidate = m_currentScheme;
    if (!candidate.referenceAssets.replaceAsset(replacement, errorMessage))
        return false;
    const bool replacingPrimary = baseId.trimmed()
            == candidate.referenceAssets.primaryBaseId();
    return persistReferenceAssetFrameMutation(candidate,
                                              replacement.baseId,
                                              normalizedFrame,
                                              replacingPrimary,
                                              errorMessage);
}

bool SchemeStore::removeReferenceAsset(const QString &baseId,
                                       QString *errorMessage)
{
    if (!ensureLoaded(errorMessage))
        return false;
    const QString normalizedBaseId = baseId.trimmed();
    if (schemeReferencesBaseId(m_currentScheme, normalizedBaseId)) {
        setError(errorMessage,
                 QStringLiteral("参考资产 %1 正被模板定位配置使用，请先移除对应模板")
                 .arg(normalizedBaseId));
        return false;
    }
    SchemeState candidate = m_currentScheme;
    if (!candidate.referenceAssets.removeAsset(normalizedBaseId, errorMessage))
        return false;
    syncLegacyReferenceMirror(&candidate);

    return commitReferenceAssetState(candidate, errorMessage);
}

bool SchemeStore::setPrimaryReferenceAsset(const QString &baseId,
                                           QString *errorMessage)
{
    if (!ensureLoaded(errorMessage))
        return false;
    SchemeState candidate = m_currentScheme;
    if (candidate.referenceAssets.primaryBaseId() == baseId.trimmed())
        return true;
    if (!candidate.referenceAssets.setPrimaryBaseId(baseId, errorMessage))
        return false;

    // The primary pointer is only a compatibility source for legacy tools.
    // Stable v6 Base bindings remain valid because no Base pixels changed.
    candidate.referencePreviewSnapshots.clear();
    invalidateSchemeLocatorDependencies(&candidate,
                                        baseId.trimmed(),
                                        true,
                                        false);
    syncLegacyReferenceMirror(&candidate);

    return commitReferenceAssetState(candidate, errorMessage);
}

bool SchemeStore::setMultiReferenceEnabled(bool enabled,
                                           QString *errorMessage)
{
    if (!m_loaded && !ensureLoaded(errorMessage))
        return false;
    return m_currentScheme.referenceAssets.setMultiBaseEnabled(
                enabled, errorMessage);
}

bool SchemeStore::persistReferenceAssetFrameMutation(
        SchemeState candidate,
        const QString &baseId,
        const cv::Mat &frame,
        bool replacingPrimary,
        QString *errorMessage,
        QString *savedRelativePath)
{
    const ReferenceAsset *asset = candidate.referenceAssets.assetById(baseId);
    if (!asset) {
        setError(errorMessage,
                 QStringLiteral("参考资产候选状态缺少: %1").arg(baseId));
        return false;
    }

    QByteArray pngBytes;
    if (!encodeReferencePng(frame, &pngBytes, errorMessage))
        return false;
    if (bytesRevision(pngBytes) != asset->contentRevision) {
        setError(errorMessage, QStringLiteral("参考资产内容修订签名不一致"));
        return false;
    }

    QString directoryError;
    if (!validateSchemeDirectory(projectsRootPath(),
                                 candidate.schemeId,
                                 candidate.schemeDir,
                                 true,
                                 &directoryError)
            || !validateReferencePath(candidate.schemeDir,
                                      asset->path,
                                      false,
                                      &directoryError)) {
        setError(errorMessage, directoryError);
        return false;
    }

    const QString absolutePath = QDir(candidate.schemeDir).filePath(asset->path);
    bool createdFile = false;
    if (QFileInfo::exists(absolutePath)) {
        if (QFileInfo(absolutePath).isSymLink()
                || fileRevision(absolutePath) != asset->contentRevision) {
            setError(errorMessage,
                     QStringLiteral("同名不可变参考资产内容冲突: %1")
                     .arg(absolutePath));
            return false;
        }
    } else {
        QSaveFile imageFile(absolutePath);
        if (!imageFile.open(QIODevice::WriteOnly)
                || imageFile.write(pngBytes) != pngBytes.size()
                || !imageFile.commit()) {
            imageFile.cancelWriting();
            setError(errorMessage,
                     QStringLiteral("无法提交参考资产: %1").arg(absolutePath));
            return false;
        }
        createdFile = true;
    }

    invalidateSchemeLocatorDependencies(&candidate,
                                        baseId,
                                        replacingPrimary,
                                        true);
    if (replacingPrimary) {
        candidate.referencePreviewSnapshots.clear();
    }
    syncLegacyReferenceMirror(&candidate);
    candidate.updatedAt = QDateTime::currentDateTime();

    const QString assetPath = asset->path;
    if (!commitReferenceAssetState(candidate, errorMessage)) {
        if (createdFile)
            QFile::remove(absolutePath);
        return false;
    }
    if (savedRelativePath)
        *savedRelativePath = assetPath;
    return true;
}

bool SchemeStore::prepareReferenceFrameEntries(
        const SchemeState &state,
        QList<ReferenceFrameEntry> *entries,
        QString *errorMessage) const
{
    if (!entries) {
        setError(errorMessage, QStringLiteral("参考资产运行态输出指针为空"));
        return false;
    }
    entries->clear();
    if (state.referenceAssets.isReadOnly()) {
        setError(errorMessage,
                 QStringLiteral("参考资产配置无法安全加载: %1")
                 .arg(state.referenceAssets.readOnlyReason()));
        return false;
    }
    if (state.referenceAssets.isEmpty())
        return true;

    QString validationError;
    if (!state.referenceAssets.validate(&validationError)) {
        setError(errorMessage,
                 QStringLiteral("参考资产集合无效: %1").arg(validationError));
        return false;
    }

    entries->reserve(state.referenceAssets.size());
    for (const ReferenceAsset &asset : state.referenceAssets.assets()) {
        QString pathError;
        const QString path = referenceAssetAbsolutePath(
                    state, asset.baseId, true, &pathError);
        if (path.isEmpty()) {
            entries->clear();
            setError(errorMessage,
                     QStringLiteral("无法解析参考资产 %1: %2")
                     .arg(asset.baseId, pathError));
            return false;
        }
        const cv::Mat decoded = cv::imread(path.toStdString(),
                                           cv::IMREAD_UNCHANGED);
        if (decoded.empty()) {
            entries->clear();
            setError(errorMessage,
                     QStringLiteral("无法解码参考资产 %1: %2")
                     .arg(asset.baseId, path));
            return false;
        }
        const cv::Mat normalized =
                ReferenceImageProvider::normalizeReferenceFrame(decoded);
        if (normalized.empty()) {
            entries->clear();
            setError(errorMessage,
                     QStringLiteral("参考资产格式不受支持 %1: %2")
                     .arg(asset.baseId, path));
            return false;
        }

        ReferenceFrameEntry entry;
        entry.baseId = asset.baseId;
        entry.name = asset.name;
        entry.frame = normalized;
        const QString source = asset.inputMetadata.source.trimmed().isEmpty()
                ? QStringLiteral("reference:%1").arg(asset.baseId)
                : asset.inputMetadata.source;
        entry.metadata = FrameInputMetadata::fromMat(normalized, source);
        entry.contentRevision = asset.contentRevision;
        entry.displayOrder = asset.order;
        entries->append(entry);
    }
    return true;
}

bool SchemeStore::commitReferenceAssetState(SchemeState candidate,
                                            QString *errorMessage)
{
    candidate = normalizedStateForSave(candidate);
    QList<ReferenceFrameEntry> preparedEntries;
    QString prepareError;
    if (!prepareReferenceFrameEntries(candidate,
                                      &preparedEntries,
                                      &prepareError)) {
        setError(errorMessage,
                 QStringLiteral("参考资产未保存，候选运行态不可用: %1")
                 .arg(prepareError));
        return false;
    }

    const SchemeState previousState = m_currentScheme;
    const QByteArray previousJson = [&previousState]() {
        QFile file(QDir(previousState.schemeDir).filePath(
                       QString::fromLatin1(kSchemeJsonName)));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }();
    const QList<ReferenceFrameEntry> previousEntries =
            ReferenceImageProvider::instance().referenceFrames();
    const QString previousPrimary =
            ReferenceImageProvider::instance().primaryBaseId();

    SchemeState savedState;
    if (!saveSchemeToFile(candidate, errorMessage, nullptr, &savedState))
        return false;

    // Publish signals are synchronous. Make the matching scheme state visible
    // first, then compensate both durable and runtime state on an unexpected
    // provider rejection (all normal validation happened in the prepare step).
    m_currentScheme = savedState;
    refreshAvailableSchemes(nullptr);
    QString publishError;
    if (!ReferenceImageProvider::instance().setReferenceFrames(
            preparedEntries,
            savedState.referenceAssets.primaryBaseId(),
            &publishError)) {
        QString rollbackError;
        if (!previousJson.isEmpty()) {
            QSaveFile rollbackFile(schemeJsonPath(previousState));
            if (!rollbackFile.open(QIODevice::WriteOnly)
                    || rollbackFile.write(previousJson) != previousJson.size()
                    || !rollbackFile.commit()) {
                rollbackFile.cancelWriting();
                rollbackError = QStringLiteral("方案文件回滚失败");
            }
        } else {
            rollbackError = QStringLiteral("缺少旧方案文件快照");
        }
        m_currentScheme = previousState;
        refreshAvailableSchemes(nullptr);
        QString restoreError;
        if (!ReferenceImageProvider::instance().setReferenceFrames(
                previousEntries, previousPrimary, &restoreError)) {
            ReferenceImageProvider::instance().clearReferenceFrames();
            rollbackError += QStringLiteral("；运行态恢复失败: %1")
                    .arg(restoreError);
        }
        setError(errorMessage,
                 QStringLiteral("参考资产运行态发布失败，事务已回滚: %1%2")
                 .arg(publishError,
                      rollbackError.isEmpty()
                      ? QString()
                      : QStringLiteral("；%1").arg(rollbackError)));
        return false;
    }

    emit referenceAssetStateCommitted(m_currentScheme.schemeId);
    return true;
}

bool SchemeStore::loadCurrentReferenceIntoProvider(QString *errorMessage)
{
    if (!ensureLoaded(errorMessage))
        return false;

    QList<ReferenceFrameEntry> entries;
    if (!prepareReferenceFrameEntries(m_currentScheme, &entries, errorMessage))
        return false;

    QString providerError;
    if (!ReferenceImageProvider::instance().setReferenceFrames(
            entries,
            m_currentScheme.referenceAssets.primaryBaseId(),
            &providerError)) {
        setError(errorMessage,
                 QStringLiteral("无法发布参考资产运行态: %1").arg(providerError));
        return false;
    }
    m_currentScheme.referenceInputMetadata =
            ReferenceImageProvider::instance().referenceFrameMetadata();
    return true;
}

bool SchemeStore::loadSchemeFromFile(const QString &schemeJsonPath,
                                     SchemeState *state,
                                     QString *errorMessage) const
{
    if (!state) {
        setError(errorMessage, QStringLiteral("方案状态指针为空"));
        return false;
    }

    const QFileInfo jsonInfo(schemeJsonPath);
    if (jsonInfo.fileName() != QString::fromLatin1(kSchemeJsonName)) {
        setError(errorMessage, QStringLiteral("方案文件名非法: %1").arg(schemeJsonPath));
        return false;
    }
    const QString directoryId = jsonInfo.dir().dirName();
    QString pathError;
    if (!validateSchemeDirectory(projectsRootPath(),
                                 directoryId,
                                 jsonInfo.absolutePath(),
                                 true,
                                 &pathError)) {
        setError(errorMessage, pathError);
        return false;
    }
    if (jsonInfo.isSymLink()) {
        setError(errorMessage, QStringLiteral("方案文件不允许使用符号链接: %1")
                 .arg(schemeJsonPath));
        return false;
    }

    QFile file(schemeJsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("无法打开方案文件: %1").arg(schemeJsonPath));
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        setError(errorMessage, QStringLiteral("方案文件不是有效 JSON: %1").arg(schemeJsonPath));
        return false;
    }

    const QJsonObject json = document.object();
    SchemeState loaded;
    loaded.schemaVersion = json.value(QStringLiteral("schemaVersion")).toInt(1);
    loaded.schemeDir = QFileInfo(schemeJsonPath).absolutePath();
    loaded.schemeId = sanitizedSchemeId(json.value(QStringLiteral("schemeId")).toString());
    if (loaded.schemeId.isEmpty())
        loaded.schemeId = QFileInfo(loaded.schemeDir).fileName();
    if (loaded.schemeId != directoryId) {
        setError(errorMessage,
                 QStringLiteral("方案 id 与目录名不一致: %1 / %2")
                 .arg(loaded.schemeId, directoryId));
        return false;
    }
    loaded.schemeName = json.value(QStringLiteral("schemeName")).toString(loaded.schemeId);
    loaded.referenceImagePath = json.value(QStringLiteral("referenceImage")).toString();
    if (!validateReferencePath(loaded.schemeDir,
                               loaded.referenceImagePath,
                               !loaded.referenceImagePath.trimmed().isEmpty(),
                               &pathError)) {
        setError(errorMessage, pathError);
        return false;
    }
    loaded.referenceInputMetadata = FrameInputMetadata::fromJson(
                json.value(QStringLiteral("referenceInputMetadata")).toObject());
    if (json.contains(QStringLiteral("referenceAssets"))) {
        QString assetsDecodeMessage;
        loaded.referenceAssets = ReferenceAssetSet::fromJson(
                    json.value(QStringLiteral("referenceAssets")),
                    &assetsDecodeMessage);
        if (loaded.referenceAssets.isReadOnly())
            qWarning() << "[SchemeStore]" << assetsDecodeMessage;
    } else {
        const QString legacyAbsolutePath = loaded.referenceImagePath.trimmed().isEmpty()
                ? QString()
                : QDir(loaded.schemeDir).filePath(loaded.referenceImagePath);
        loaded.referenceAssets = ReferenceAssetSet::fromLegacyReference(
                    loaded.referenceImagePath,
                    loaded.referenceInputMetadata,
                    legacyAbsolutePath.isEmpty()
                    ? QString() : fileRevision(legacyAbsolutePath));
    }
    if (!loaded.referenceAssets.isReadOnly()) {
        for (const ReferenceAsset &asset : loaded.referenceAssets.assets()) {
            if (!validateReferencePath(loaded.schemeDir,
                                       asset.path,
                                       true,
                                       &pathError)) {
                setError(errorMessage,
                         QStringLiteral("参考资产 %1 无效: %2")
                         .arg(asset.baseId, pathError));
                return false;
            }
            if (asset.contentRevision.startsWith(QStringLiteral("sha256:"))
                    && fileRevision(QDir(loaded.schemeDir).filePath(asset.path))
                       != asset.contentRevision) {
                setError(errorMessage,
                         QStringLiteral("参考资产内容修订不匹配: %1")
                         .arg(asset.baseId));
                return false;
            }
        }
        syncLegacyReferenceMirror(&loaded);
    }
    loaded.referencePositionCorrection = PositionCorrection::referenceFromJson(
                json.value(QStringLiteral("referencePositionCorrection")).toObject());
    loaded.toolConfigs = toolConfigsFromJson(json.value(QStringLiteral("tools")).toArray());
    loaded.referencePreviewSnapshots = previewSnapshotsFromJson(json.value(QStringLiteral("previews")).toObject());
    loaded.quickCalibrationConfig = json.value(
                QStringLiteral("quickCalibration")).toObject();
    loaded.outputConfig = json.value(QStringLiteral("output")).toObject();
    loaded.updatedAt = QDateTime::fromString(json.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);
    if (!loaded.updatedAt.isValid())
        loaded.updatedAt = QFileInfo(schemeJsonPath).lastModified();

    normalizeSnapshotsForTools(&loaded);

    *state = loaded;
    return true;
}

bool SchemeStore::saveSchemeToFile(const SchemeState &state,
                                   QString *errorMessage,
                                   const cv::Mat *referenceFrame,
                                   SchemeState *savedState) const
{
    SchemeState normalized = normalizedStateForSave(state);
    normalizeSnapshotsForTools(&normalized);
    SaveFileRollback rollback;

    QDir root(projectsRootPath());
    if (!root.exists() && !root.mkpath(QStringLiteral("."))) {
        setError(errorMessage,
                 QStringLiteral("无法创建方案根目录: %1").arg(root.absolutePath()));
        return false;
    }
    QString pathError;
    if (!validateSchemeDirectory(root.absolutePath(),
                                 normalized.schemeId,
                                 normalized.schemeDir,
                                 false,
                                 &pathError)) {
        setError(errorMessage, pathError);
        return false;
    }
    if (!validateReferencePath(normalized.schemeDir,
                               normalized.referenceImagePath,
                               false,
                               &pathError)) {
        setError(errorMessage, pathError);
        return false;
    }

    QDir schemeDir(normalized.schemeDir);
    const bool schemeDirectoryExisted = schemeDir.exists();
    if (!schemeDirectoryExisted && !schemeDir.mkpath(QStringLiteral("."))) {
        setError(errorMessage, QStringLiteral("无法创建方案目录: %1").arg(normalized.schemeDir));
        return false;
    }
    if (!schemeDirectoryExisted)
        rollback.trackDirectory(normalized.schemeDir);
    if (!validateSchemeDirectory(root.absolutePath(),
                                 normalized.schemeId,
                                 normalized.schemeDir,
                                 true,
                                 &pathError)) {
        setError(errorMessage, pathError);
        return false;
    }
    if (!materializeCalibrationAssetsForScheme(&normalized, &rollback, errorMessage))
        return false;

    if (!normalized.referenceAssets.isReadOnly()) {
        QString assetsError;
        if (!normalized.referenceAssets.validate(&assetsError)) {
            setError(errorMessage,
                     QStringLiteral("参考资产集合无效: %1").arg(assetsError));
            return false;
        }
        for (const ReferenceAsset &asset : normalized.referenceAssets.assets()) {
            if (!validateReferencePath(normalized.schemeDir,
                                       asset.path,
                                       true,
                                       &pathError)) {
                setError(errorMessage,
                         QStringLiteral("参考资产 %1 无效: %2")
                         .arg(asset.baseId, pathError));
                return false;
            }
            if (asset.contentRevision.startsWith(QStringLiteral("sha256:"))
                    && fileRevision(schemeDir.filePath(asset.path))
                       != asset.contentRevision) {
                setError(errorMessage,
                         QStringLiteral("参考资产内容修订不匹配: %1")
                         .arg(asset.baseId));
                return false;
            }
        }
    }

    QString newlyWrittenReferencePath;
    if (referenceFrame) {
        if (referenceFrame->empty() || normalized.referenceImagePath.trimmed().isEmpty()) {
            setError(errorMessage, QStringLiteral("基准图保存参数无效"));
            return false;
        }

        newlyWrittenReferencePath =
                schemeDir.filePath(normalized.referenceImagePath);
        try {
            if (!cv::imwrite(newlyWrittenReferencePath.toStdString(), *referenceFrame)) {
                QFile::remove(newlyWrittenReferencePath);
                setError(errorMessage,
                         QStringLiteral("无法写入基准图: %1")
                         .arg(newlyWrittenReferencePath));
                return false;
            }
        } catch (const cv::Exception &exception) {
            QFile::remove(newlyWrittenReferencePath);
            setError(errorMessage,
                     QStringLiteral("写入基准图异常: %1")
                     .arg(QString::fromLocal8Bit(exception.what())));
            return false;
        }
    } else if (!normalized.referenceImagePath.trimmed().isEmpty()) {
        const QString existingReferencePath =
                schemeDir.filePath(normalized.referenceImagePath);
        if (!validateReferencePath(normalized.schemeDir,
                                   normalized.referenceImagePath,
                                   true,
                                   &pathError)) {
            setError(errorMessage, pathError);
            return false;
        }
    }

    QJsonObject json;
    json.insert(QStringLiteral("schemaVersion"), normalized.schemaVersion);
    json.insert(QStringLiteral("schemeId"), normalized.schemeId);
    json.insert(QStringLiteral("schemeName"), normalized.schemeName);
    json.insert(QStringLiteral("referenceImage"), normalized.referenceImagePath);
    json.insert(QStringLiteral("referenceInputMetadata"),
                normalized.referenceInputMetadata.toJson());
    json.insert(QStringLiteral("referenceAssets"),
                normalized.referenceAssets.toJson());
    json.insert(QStringLiteral("referencePositionCorrection"),
                PositionCorrection::referenceToJson(normalized.referencePositionCorrection));
    json.insert(QStringLiteral("tools"), toolConfigsToJson(normalized.toolConfigs));
    json.insert(QStringLiteral("previews"), previewSnapshotsToJson(normalized.referencePreviewSnapshots));
    json.insert(QStringLiteral("quickCalibration"),
                normalized.quickCalibrationConfig);
    json.insert(QStringLiteral("output"), normalized.outputConfig);
    json.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));

    QSaveFile file(schemeDir.filePath(QString::fromLatin1(kSchemeJsonName)));
    if (!file.open(QIODevice::WriteOnly)) {
        if (!newlyWrittenReferencePath.isEmpty())
            QFile::remove(newlyWrittenReferencePath);
        setError(errorMessage, QStringLiteral("无法写入方案文件: %1").arg(file.fileName()));
        return false;
    }

    const QByteArray jsonBytes =
            QJsonDocument(json).toJson(QJsonDocument::Indented);
    if (file.write(jsonBytes) != jsonBytes.size()) {
        file.cancelWriting();
        if (!newlyWrittenReferencePath.isEmpty())
            QFile::remove(newlyWrittenReferencePath);
        setError(errorMessage,
                 QStringLiteral("方案文件写入不完整: %1").arg(file.fileName()));
        return false;
    }
    if (!file.commit()) {
        if (!newlyWrittenReferencePath.isEmpty())
            QFile::remove(newlyWrittenReferencePath);
        setError(errorMessage, QStringLiteral("方案文件提交失败: %1").arg(file.fileName()));
        return false;
    }

    rollback.commit();
    if (savedState)
        *savedState = normalized;
    return true;
}

bool SchemeStore::refreshAvailableSchemes(QString *errorMessage)
{
    const QString rootPath = projectsRootPath();
    QDir root(rootPath);
    if (!root.exists() && !root.mkpath(QStringLiteral("."))) {
        setError(errorMessage, QStringLiteral("无法创建方案根目录: %1").arg(rootPath));
        return false;
    }

    QVector<SchemeState> schemes;
    const QFileInfoList entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString jsonPath = QDir(entry.absoluteFilePath()).filePath(QString::fromLatin1(kSchemeJsonName));
        if (!QFileInfo::exists(jsonPath))
            continue;

        SchemeState state;
        QString loadError;
        if (!loadSchemeFromFile(jsonPath, &state, &loadError)) {
            qWarning() << "[SchemeStore]" << loadError;
            continue;
        }
        schemes.append(state);
    }

    std::sort(schemes.begin(), schemes.end(), [](const SchemeState &left, const SchemeState &right) {
        return left.schemeName.localeAwareCompare(right.schemeName) < 0;
    });

    m_availableSchemes = schemes;
    return true;
}

bool SchemeStore::createDefaultScheme(QString *errorMessage)
{
    SchemeState state;
    state.schemeId = QStringLiteral("scheme_1");
    state.schemeName = QStringLiteral("方案 1");
    state.schemeDir = QDir(projectsRootPath()).filePath(state.schemeId);
    state.referenceImagePath.clear();
    state.updatedAt = QDateTime::currentDateTime();

    m_currentScheme = state;
    m_loaded = true;
    if (!saveCurrentScheme(errorMessage)) {
        m_loaded = false;
        return false;
    }

    return refreshAvailableSchemes(errorMessage);
}

QString SchemeStore::resolveProjectRootPath() const
{
    // Integration/UI smokes exercise the real persistence path.  Give those
    // processes an explicit root instead of relying on the source-tree search,
    // which could otherwise select a developer's checkout and overwrite a
    // real scheme.  The marker requirement keeps accidental values harmless.
    const QString overriddenRoot = qEnvironmentVariable(
                kProjectRootOverrideEnvironment).trimmed();
    if (!overriddenRoot.isEmpty()) {
        const QDir overrideDir(overriddenRoot);
        if (overrideDir.isAbsolute()
                && overrideDir.exists(QStringLiteral("qt_ui_test.pro"))) {
            return overrideDir.absolutePath();
        }
        qWarning() << "[SchemeStore] Ignoring invalid project-root override:"
                   << overriddenRoot;
    }

    const QStringList candidates = {
        QDir::currentPath(),
        QCoreApplication::applicationDirPath()
    };

    for (const QString &candidate : candidates) {
        QDir dir(candidate);
        for (int depth = 0; depth < 8; ++depth) {
            if (dir.exists(QStringLiteral("qt_ui_test.pro")))
                return dir.absolutePath();
            if (!dir.cdUp())
                break;
        }
    }

    return QDir::currentPath();
}

QString SchemeStore::schemeJsonPath(const SchemeState &state) const
{
    return QDir(state.schemeDir).filePath(QString::fromLatin1(kSchemeJsonName));
}

QString SchemeStore::makeUniqueSchemeId() const
{
    QString id;
    do {
        id = QStringLiteral("scheme_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    } while (QDir(projectsRootPath()).exists(id));
    return id;
}

SchemeState SchemeStore::normalizedStateForSave(SchemeState state) const
{
    state.schemeId = sanitizedSchemeId(state.schemeId);
    if (state.schemeId.isEmpty())
        state.schemeId = QStringLiteral("scheme_1");
    if (state.schemeName.trimmed().isEmpty())
        state.schemeName = state.schemeId;
    if (state.schemeDir.trimmed().isEmpty())
        state.schemeDir = QDir(projectsRootPath()).filePath(state.schemeId);
    state.schemeDir = cleanAbsolutePath(state.schemeDir);
    if (!state.referenceAssets.isReadOnly()) {
        state.referenceAssets.normalizeOrder();
        syncLegacyReferenceMirror(&state);
    }
    state.updatedAt = QDateTime::currentDateTime();
    return state;
}

QString SchemeStore::referenceAssetAbsolutePath(const SchemeState &state,
                                                const QString &baseId,
                                                bool mustExist,
                                                QString *errorMessage) const
{
    const ReferenceAsset *asset = state.referenceAssets.assetById(baseId);
    if (!asset) {
        setError(errorMessage,
                 QStringLiteral("未找到参考资产: %1").arg(baseId));
        return QString();
    }
    QString pathError;
    if (!validateSchemeDirectory(projectsRootPath(),
                                 state.schemeId,
                                 state.schemeDir,
                                 mustExist,
                                 &pathError)
            || !validateReferencePath(state.schemeDir,
                                      asset->path,
                                      mustExist,
                                      &pathError)) {
        setError(errorMessage, pathError);
        return QString();
    }
    return QDir(state.schemeDir).filePath(asset->path);
}

void SchemeStore::syncLegacyReferenceMirror(SchemeState *state) const
{
    if (!state || state->referenceAssets.isReadOnly())
        return;
    const ReferenceAsset *primary = state->referenceAssets.primaryAsset();
    if (!primary) {
        state->referenceImagePath.clear();
        state->referenceInputMetadata = FrameInputMetadata();
        return;
    }
    state->referenceImagePath = primary->path;
    state->referenceInputMetadata = primary->inputMetadata;
}

void SchemeStore::normalizeSnapshotsForCurrentTools()
{
    normalizeSnapshotsForTools(&m_currentScheme);
}

void SchemeStore::setError(QString *errorMessage, const QString &message) const
{
    if (errorMessage)
        *errorMessage = message;
}
