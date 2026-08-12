#include "SchemeStore.h"

#include <algorithm>

#include <QCoreApplication>
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

namespace {

constexpr auto kProjectsDirName = "projects";
constexpr auto kSchemeJsonName = "scheme.json";
constexpr auto kReferenceImageSlotA = "reference_a.png";
constexpr auto kReferenceImageSlotB = "reference_b.png";

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

bool copyCalibrationAssetsForScheme(const QString &sourceSchemeDir,
                                    SchemeState *targetScheme,
                                    QString *errorMessage)
{
    if (!targetScheme)
        return false;
    const QString sourceRoot = QDir(sourceSchemeDir).absolutePath() + QDir::separator();
    QDir targetAssetDir(QDir(targetScheme->schemeDir).filePath(QStringLiteral("calibrations")));
    QMap<QString, QString> copiedPaths;
    for (ToolConfig &tool : targetScheme->toolConfigs) {
        if (tool.toolType != ToolType::CalibrationTransform)
            continue;
        QJsonObject transform = tool.params.value(QStringLiteral("calibrationTransform")).toObject();
        QJsonArray rewrittenFiles;
        const QJsonArray files = transform.value(QStringLiteral("calibrationFiles")).toArray();
        for (const QJsonValue &value : files) {
            const QString sourceValue = value.toString().trimmed();
            if (sourceValue.isEmpty())
                continue;
            const QString sourcePath = QFileInfo(sourceValue).absoluteFilePath();
            QString targetPath = sourcePath;
            if (sourcePath.startsWith(sourceRoot)) {
                if (!targetAssetDir.exists() && !targetAssetDir.mkpath(QStringLiteral("."))) {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("无法创建方案副本的标定资产目录");
                    return false;
                }
                targetPath = copiedPaths.value(sourcePath);
                if (targetPath.isEmpty()) {
                    targetPath = targetAssetDir.filePath(QFileInfo(sourcePath).fileName());
                    if (!QFile::copy(sourcePath, targetPath)) {
                        if (errorMessage)
                            *errorMessage = QStringLiteral("无法复制标定资产: %1").arg(sourcePath);
                        return false;
                    }
                    copiedPaths.insert(sourcePath, targetPath);
                }
            }
            rewrittenFiles.append(targetPath);
        }
        const QString activeValue = transform.value(
                    QStringLiteral("activeCalibrationFile")).toString().trimmed();
        const QString activeSource = activeValue.isEmpty()
                ? QString() : QFileInfo(activeValue).absoluteFilePath();
        transform.insert(QStringLiteral("calibrationFiles"), rewrittenFiles);
        transform.insert(QStringLiteral("activeCalibrationFile"),
                         activeSource.isEmpty() ? QString()
                                                : copiedPaths.value(activeSource, activeSource));
        tool.params.insert(QStringLiteral("calibrationTransform"), transform);
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
        if (snapshot.toolId.trimmed().isEmpty())
            snapshot.toolId = it.key();
        snapshots.insert(snapshot.toolId, snapshot);
    }
    return snapshots;
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

QString nextReferenceImageSlot(const QString &currentPath)
{
    return QFileInfo(currentPath).fileName() == QString::fromLatin1(kReferenceImageSlotA)
            ? QString::fromLatin1(kReferenceImageSlotB)
            : QString::fromLatin1(kReferenceImageSlotA);
}

QString cleanAbsolutePath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
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
    QString loadReferenceError;
    if (!loadCurrentReferenceIntoProvider(&loadReferenceError) && !m_currentScheme.referenceImagePath.isEmpty())
        qWarning() << "[SchemeStore]" << loadReferenceError;
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

    const QString path = schemeJsonPath(target);
    if (!loadSchemeFromFile(path, &m_currentScheme, errorMessage))
        return false;

    normalizeSnapshotsForCurrentTools();
    QString loadReferenceError;
    if (!loadCurrentReferenceIntoProvider(&loadReferenceError) && !m_currentScheme.referenceImagePath.isEmpty())
        qWarning() << "[SchemeStore]" << loadReferenceError;
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

    if (!saveSchemeToFile(m_currentScheme, errorMessage)) {
        if (hasPersistedState)
            m_currentScheme = persistedState;
        return false;
    }

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
    copy.schemeId = makeUniqueSchemeId();
    copy.schemeName = newName;
    copy.schemeDir = QDir(projectsRootPath()).filePath(copy.schemeId);
    copy.updatedAt = QDateTime::currentDateTime();
    cv::Mat referenceFrame;
    if (!copy.referenceImagePath.trimmed().isEmpty()) {
        const QString sourceReferencePath = currentReferenceImageAbsolutePath();
        referenceFrame = cv::imread(sourceReferencePath.toStdString(), cv::IMREAD_UNCHANGED);
        if (referenceFrame.empty()) {
            setError(errorMessage,
                     QStringLiteral("无法读取待复制的基准图: %1").arg(sourceReferencePath));
            return false;
        }
        copy.referenceImagePath = QString::fromLatin1(kReferenceImageSlotA);
    }
    if (!copyCalibrationAssetsForScheme(m_currentScheme.schemeDir, &copy, errorMessage))
        return false;

    SchemeState savedCopy;
    const cv::Mat *referenceFramePtr = referenceFrame.empty() ? nullptr : &referenceFrame;
    if (!saveSchemeToFile(copy, errorMessage, referenceFramePtr, &savedCopy))
        return false;

    m_currentScheme = savedCopy;
    if (referenceFrame.empty()) {
        ReferenceImageProvider::instance().clearReferenceFrame();
    } else {
        ReferenceImageProvider::instance().setReferenceFrame(
                    referenceFrame, savedCopy.referenceInputMetadata);
    }
    refreshAvailableSchemes(nullptr);
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
    state.referencePositionCorrection = ReferencePositionCorrectionConfig();
    state.toolConfigs.clear();
    state.referencePreviewSnapshots.clear();
    state.outputConfig = QJsonObject();
    state.updatedAt = QDateTime::currentDateTime();

    if (!saveSchemeToFile(state, errorMessage))
        return SchemeState();

    refreshAvailableSchemes(nullptr);
    return state;
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

    SchemeState candidate = m_currentScheme;
    candidate.referenceImagePath =
            nextReferenceImageSlot(m_currentScheme.referenceImagePath);
    const QString source = metadata.source.trimmed().isEmpty()
            ? QStringLiteral("reference")
            : metadata.source;
    candidate.referenceInputMetadata =
            FrameInputMetadata::fromMat(normalizedFrame, source);
    candidate.updatedAt = QDateTime::currentDateTime();

    SchemeState savedState;
    if (!saveSchemeToFile(candidate,
                          errorMessage,
                          &normalizedFrame,
                          &savedState)) {
        return false;
    }

    m_currentScheme = savedState;
    ReferenceImageProvider::instance().setReferenceFrame(
                normalizedFrame, savedState.referenceInputMetadata);
    refreshAvailableSchemes(nullptr);
    return true;
}

bool SchemeStore::loadCurrentReferenceIntoProvider(QString *errorMessage)
{
    if (!ensureLoaded(errorMessage))
        return false;

    const QString path = currentReferenceImageAbsolutePath();
    if (path.trimmed().isEmpty()) {
        if (!m_currentScheme.referenceImagePath.trimmed().isEmpty()) {
            ReferenceImageProvider::instance().clearReferenceFrame();
            setError(errorMessage, QStringLiteral("方案基准图路径校验失败"));
            return false;
        }
        ReferenceImageProvider::instance().clearReferenceFrame();
        return true;
    }

    const cv::Mat frame = cv::imread(path.toStdString(), cv::IMREAD_UNCHANGED);
    if (frame.empty()) {
        ReferenceImageProvider::instance().clearReferenceFrame();
        setError(errorMessage, QStringLiteral("无法加载方案基准图: %1").arg(path));
        return false;
    }

    // The persisted PNG may be 8-bit or 16-bit. Do not reuse acquisition metadata
    // (for example RGBX8/BGRA16) as the runtime contract after decoding/normalization.
    const QString source = m_currentScheme.referenceInputMetadata.source.trimmed().isEmpty()
            ? QStringLiteral("reference")
            : m_currentScheme.referenceInputMetadata.source;
    const FrameInputMetadata metadata = FrameInputMetadata::fromMat(frame, source);
    ReferenceImageProvider::instance().setReferenceFrame(frame, metadata);
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
    loaded.referencePositionCorrection = PositionCorrection::referenceFromJson(
                json.value(QStringLiteral("referencePositionCorrection")).toObject());
    loaded.toolConfigs = toolConfigsFromJson(json.value(QStringLiteral("tools")).toArray());
    loaded.referencePreviewSnapshots = previewSnapshotsFromJson(json.value(QStringLiteral("previews")).toObject());
    loaded.outputConfig = json.value(QStringLiteral("output")).toObject();
    loaded.updatedAt = QDateTime::fromString(json.value(QStringLiteral("updatedAt")).toString(), Qt::ISODate);
    if (!loaded.updatedAt.isValid())
        loaded.updatedAt = QFileInfo(schemeJsonPath).lastModified();

    *state = loaded;
    return true;
}

bool SchemeStore::saveSchemeToFile(const SchemeState &state,
                                   QString *errorMessage,
                                   const cv::Mat *referenceFrame,
                                   SchemeState *savedState) const
{
    SchemeState normalized = normalizedStateForSave(state);

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
    if (!schemeDir.exists() && !schemeDir.mkpath(QStringLiteral("."))) {
        setError(errorMessage, QStringLiteral("无法创建方案目录: %1").arg(normalized.schemeDir));
        return false;
    }
    if (!validateSchemeDirectory(root.absolutePath(),
                                 normalized.schemeId,
                                 normalized.schemeDir,
                                 true,
                                 &pathError)) {
        setError(errorMessage, pathError);
        return false;
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
    json.insert(QStringLiteral("referencePositionCorrection"),
                PositionCorrection::referenceToJson(normalized.referencePositionCorrection));
    json.insert(QStringLiteral("tools"), toolConfigsToJson(normalized.toolConfigs));
    json.insert(QStringLiteral("previews"), previewSnapshotsToJson(normalized.referencePreviewSnapshots));
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

    if (savedState)
        *savedState = normalized;
    if (normalized.schemeId == m_currentScheme.schemeId)
        const_cast<SchemeStore *>(this)->m_currentScheme = normalized;

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
    state.updatedAt = QDateTime::currentDateTime();
    return state;
}

void SchemeStore::normalizeSnapshotsForCurrentTools()
{
    QMap<QString, ToolPreviewSnapshot> normalizedSnapshots;
    for (const ToolConfig &config : std::as_const(m_currentScheme.toolConfigs)) {
        ToolPreviewSnapshot snapshot = m_currentScheme.referencePreviewSnapshots.value(config.toolId);
        if (!snapshot.valid)
            continue;
        snapshot.toolId = config.toolId;
        snapshot.toolType = config.toolType;
        normalizedSnapshots.insert(config.toolId, snapshot);
    }
    m_currentScheme.referencePreviewSnapshots = normalizedSnapshots;
}

void SchemeStore::setError(QString *errorMessage, const QString &message) const
{
    if (errorMessage)
        *errorMessage = message;
}
