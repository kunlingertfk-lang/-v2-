#include "frame/ReferenceAssetSet.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QSet>
#include <QStringList>

namespace {

const QStringList kAssetKeys = {
    QStringLiteral("baseId"),
    QStringLiteral("name"),
    QStringLiteral("order"),
    QStringLiteral("path"),
    QStringLiteral("inputMetadata"),
    QStringLiteral("contentRevision")
};

const QStringList kMetadataKeys = {
    QStringLiteral("colorMode"),
    QStringLiteral("pixelFormat"),
    QStringLiteral("originalChannels"),
    QStringLiteral("originalDepth"),
    QStringLiteral("validBits"),
    QStringLiteral("bitShift"),
    QStringLiteral("source")
};

const QStringList kSetKeys = {
    QStringLiteral("version"),
    QStringLiteral("primaryBaseId"),
    QStringLiteral("multiBaseEnabled"),
    QStringLiteral("assets")
};

bool exactInteger(const QJsonValue &value, int *result)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number)
        return false;
    if (number < static_cast<double>(std::numeric_limits<int>::min())
            || number > static_cast<double>(std::numeric_limits<int>::max()))
        return false;
    if (result)
        *result = static_cast<int>(number);
    return true;
}

bool safeRelativeFileName(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty() || QFileInfo(trimmed).isAbsolute())
        return false;
    const QString cleaned = QDir::cleanPath(trimmed);
    return cleaned != QStringLiteral(".")
            && cleaned != QStringLiteral("..")
            && !cleaned.startsWith(QStringLiteral("../"))
            && QFileInfo(cleaned).fileName() == cleaned;
}

bool stableId(const QString &value)
{
    const QString id = value.trimmed();
    if (id.isEmpty() || id.size() > 96)
        return false;
    for (const QChar ch : id) {
        if (!ch.isLetterOrNumber()
                && ch != QLatin1Char('_')
                && ch != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

QJsonObject unknownFields(const QJsonObject &source,
                          const QStringList &knownKeys)
{
    QJsonObject extra = source;
    for (const QString &key : knownKeys)
        extra.remove(key);
    return extra;
}

QJsonObject mergedMetadata(const ReferenceAsset &asset)
{
    QJsonObject json = asset.inputMetadataExtra;
    const QJsonObject known = asset.inputMetadata.toJson();
    for (auto it = known.constBegin(); it != known.constEnd(); ++it)
        json.insert(it.key(), it.value());
    return json;
}

QJsonObject assetToJson(const ReferenceAsset &asset)
{
    QJsonObject json = asset.extra;
    json.insert(QStringLiteral("baseId"), asset.baseId);
    json.insert(QStringLiteral("name"), asset.name);
    json.insert(QStringLiteral("order"), asset.order);
    json.insert(QStringLiteral("path"), asset.path);
    json.insert(QStringLiteral("inputMetadata"), mergedMetadata(asset));
    json.insert(QStringLiteral("contentRevision"), asset.contentRevision);
    return json;
}

bool assetFromJson(const QJsonValue &value,
                   ReferenceAsset *asset,
                   QString *errorMessage)
{
    if (!value.isObject()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参考资产条目不是对象");
        return false;
    }
    const QJsonObject json = value.toObject();
    if (!json.value(QStringLiteral("baseId")).isString()
            || !json.value(QStringLiteral("name")).isString()
            || !json.value(QStringLiteral("path")).isString()
            || !json.value(QStringLiteral("inputMetadata")).isObject()
            || !json.value(QStringLiteral("contentRevision")).isString()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参考资产字段类型非法");
        return false;
    }

    int order = -1;
    if (!exactInteger(json.value(QStringLiteral("order")), &order)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参考资产顺序不是整数");
        return false;
    }

    ReferenceAsset decoded;
    decoded.baseId = json.value(QStringLiteral("baseId")).toString().trimmed();
    decoded.name = json.value(QStringLiteral("name")).toString().trimmed();
    decoded.order = order;
    decoded.path = json.value(QStringLiteral("path")).toString().trimmed();
    const QJsonObject metadataJson = json.value(
                QStringLiteral("inputMetadata")).toObject();
    decoded.inputMetadata = FrameInputMetadata::fromJson(metadataJson);
    decoded.inputMetadataExtra = unknownFields(metadataJson, kMetadataKeys);
    decoded.contentRevision = json.value(
                QStringLiteral("contentRevision")).toString().trimmed();
    decoded.extra = unknownFields(json, kAssetKeys);

    if (!stableId(decoded.baseId)
            || decoded.name.isEmpty()
            || decoded.order < 0
            || !safeRelativeFileName(decoded.path)
            || decoded.contentRevision.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参考资产包含空值、越界路径或非法标识");
        return false;
    }
    if (asset)
        *asset = decoded;
    return true;
}

} // namespace

ReferenceAssetSet ReferenceAssetSet::readOnlyRaw(const QJsonValue &raw,
                                                 const QString &reason)
{
    ReferenceAssetSet result;
    result.m_readOnly = true;
    result.m_readOnlyReason = reason;
    result.m_raw = raw;
    return result;
}

ReferenceAssetSet ReferenceAssetSet::fromJson(const QJsonValue &value,
                                              QString *errorMessage)
{
    if (value.isUndefined() || value.isNull())
        return ReferenceAssetSet();
    if (!value.isObject()) {
        const QString reason = QStringLiteral("referenceAssets 不是对象，只读保留原始值");
        if (errorMessage)
            *errorMessage = reason;
        return readOnlyRaw(value, reason);
    }

    const QJsonObject json = value.toObject();
    int version = 0;
    if (!exactInteger(json.value(QStringLiteral("version")), &version)) {
        const QString reason = QStringLiteral("referenceAssets.version 不是整数，只读保留原始值");
        if (errorMessage)
            *errorMessage = reason;
        return readOnlyRaw(value, reason);
    }
    if (version != kCurrentVersion) {
        const QString reason = QStringLiteral("不支持 referenceAssets 版本 %1，只读保留原始值")
                .arg(version);
        if (errorMessage)
            *errorMessage = reason;
        return readOnlyRaw(value, reason);
    }
    if (!json.value(QStringLiteral("primaryBaseId")).isString()
            || !json.value(QStringLiteral("assets")).isArray()) {
        const QString reason = QStringLiteral("referenceAssets 必填字段类型非法，只读保留原始值");
        if (errorMessage)
            *errorMessage = reason;
        return readOnlyRaw(value, reason);
    }
    if (json.contains(QStringLiteral("multiBaseEnabled"))
            && !json.value(QStringLiteral("multiBaseEnabled")).isBool()) {
        const QString reason = QStringLiteral(
                    "referenceAssets.multiBaseEnabled 不是布尔值，只读保留原始值");
        if (errorMessage)
            *errorMessage = reason;
        return readOnlyRaw(value, reason);
    }

    ReferenceAssetSet decoded;
    decoded.m_version = version;
    decoded.m_primaryBaseId = json.value(
                QStringLiteral("primaryBaseId")).toString().trimmed();
    decoded.m_extra = unknownFields(json, kSetKeys);
    const QJsonArray array = json.value(QStringLiteral("assets")).toArray();
    if (array.size() > kMaximumAssets) {
        const QString reason = QStringLiteral("参考资产超过上限 %1，只读保留原始值")
                .arg(kMaximumAssets);
        if (errorMessage)
            *errorMessage = reason;
        return readOnlyRaw(value, reason);
    }
    decoded.m_assets.reserve(array.size());
    for (const QJsonValue &item : array) {
        ReferenceAsset asset;
        QString itemError;
        if (!assetFromJson(item, &asset, &itemError)) {
            const QString reason = QStringLiteral("%1，只读保留原始值").arg(itemError);
            if (errorMessage)
                *errorMessage = reason;
            return readOnlyRaw(value, reason);
        }
        decoded.m_assets.append(asset);
    }
    // v1 originally had no explicit UI mode. Existing multi-Base schemes
    // migrate to the expanded thumbnail view, while legacy/single-Base
    // schemes keep the compact single-image view.
    decoded.m_multiBaseEnabled = json.contains(
                QStringLiteral("multiBaseEnabled"))
            ? json.value(QStringLiteral("multiBaseEnabled")).toBool()
            : decoded.m_assets.size() > 1;
    QString validationError;
    if (!decoded.validate(&validationError)) {
        const QString reason = QStringLiteral("%1，只读保留原始值")
                .arg(validationError);
        if (errorMessage)
            *errorMessage = reason;
        return readOnlyRaw(value, reason);
    }
    std::stable_sort(decoded.m_assets.begin(), decoded.m_assets.end(),
                     [](const ReferenceAsset &left, const ReferenceAsset &right) {
        if (left.order != right.order)
            return left.order < right.order;
        return left.baseId < right.baseId;
    });
    return decoded;
}

ReferenceAssetSet ReferenceAssetSet::fromLegacyReference(
        const QString &path,
        const FrameInputMetadata &metadata,
        const QString &contentRevision)
{
    ReferenceAssetSet result;
    if (path.trimmed().isEmpty())
        return result;

    ReferenceAsset asset;
    asset.baseId = QStringLiteral("base0");
    asset.name = QStringLiteral("基准图 1");
    asset.order = 0;
    asset.path = path.trimmed();
    asset.inputMetadata = metadata;
    asset.contentRevision = contentRevision.trimmed().isEmpty()
            ? QStringLiteral("legacy:unknown") : contentRevision.trimmed();
    result.m_assets.append(asset);
    result.m_primaryBaseId = asset.baseId;
    return result;
}

QJsonValue ReferenceAssetSet::toJson() const
{
    if (m_readOnly)
        return m_raw;

    QJsonObject json = m_extra;
    json.insert(QStringLiteral("version"), m_version);
    json.insert(QStringLiteral("primaryBaseId"), m_primaryBaseId);
    json.insert(QStringLiteral("multiBaseEnabled"), m_multiBaseEnabled);
    QJsonArray array;
    for (const ReferenceAsset &asset : m_assets)
        array.append(assetToJson(asset));
    json.insert(QStringLiteral("assets"), array);
    return json;
}

const ReferenceAsset *ReferenceAssetSet::assetById(const QString &baseId) const
{
    const QString id = baseId.trimmed();
    for (const ReferenceAsset &asset : m_assets) {
        if (asset.baseId == id)
            return &asset;
    }
    return nullptr;
}

const ReferenceAsset *ReferenceAssetSet::primaryAsset() const
{
    return assetById(m_primaryBaseId);
}

bool ReferenceAssetSet::ensureWritable(QString *errorMessage) const
{
    if (!m_readOnly)
        return true;
    if (errorMessage) {
        *errorMessage = m_readOnlyReason.isEmpty()
                ? QStringLiteral("参考资产配置为只读")
                : m_readOnlyReason;
    }
    return false;
}

bool ReferenceAssetSet::addAsset(const ReferenceAsset &asset,
                                 bool makePrimary,
                                 QString *errorMessage)
{
    if (!ensureWritable(errorMessage))
        return false;
    if (m_assets.size() >= kMaximumAssets) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参考资产最多允许 %1 张").arg(kMaximumAssets);
        return false;
    }
    if (assetById(asset.baseId)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参考资产 baseId 已存在: %1").arg(asset.baseId);
        return false;
    }

    ReferenceAsset appended = asset;
    appended.baseId = appended.baseId.trimmed();
    appended.name = appended.name.trimmed();
    appended.path = appended.path.trimmed();
    appended.contentRevision = appended.contentRevision.trimmed();
    // Imported v1 sets are allowed to use sparse (but unique) order values.
    // Appending at size() would collide for a valid set such as {0, 2}.
    int maximumOrder = -1;
    for (const ReferenceAsset &existing : m_assets)
        maximumOrder = std::max(maximumOrder, existing.order);
    appended.order = maximumOrder + 1;
    const QString previousPrimaryBaseId = m_primaryBaseId;
    const bool previousMultiBaseEnabled = m_multiBaseEnabled;
    m_assets.append(appended);
    if (m_assets.size() > 1)
        m_multiBaseEnabled = true;
    if (makePrimary || m_primaryBaseId.isEmpty())
        m_primaryBaseId = appended.baseId;
    QString validationError;
    if (!validate(&validationError)) {
        m_assets.removeLast();
        m_multiBaseEnabled = previousMultiBaseEnabled;
        if (m_primaryBaseId == appended.baseId)
            m_primaryBaseId = previousPrimaryBaseId;
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }
    return true;
}

bool ReferenceAssetSet::replaceAsset(const ReferenceAsset &asset,
                                     QString *errorMessage)
{
    if (!ensureWritable(errorMessage))
        return false;
    for (int index = 0; index < m_assets.size(); ++index) {
        if (m_assets.at(index).baseId != asset.baseId.trimmed())
            continue;
        ReferenceAsset replacement = asset;
        replacement.baseId = replacement.baseId.trimmed();
        replacement.name = replacement.name.trimmed();
        replacement.path = replacement.path.trimmed();
        replacement.contentRevision = replacement.contentRevision.trimmed();
        replacement.order = m_assets.at(index).order;
        const ReferenceAsset previous = m_assets.at(index);
        m_assets[index] = replacement;
        QString validationError;
        if (!validate(&validationError)) {
            m_assets[index] = previous;
            if (errorMessage)
                *errorMessage = validationError;
            return false;
        }
        return true;
    }
    if (errorMessage)
        *errorMessage = QStringLiteral("未找到参考资产: %1").arg(asset.baseId);
    return false;
}

bool ReferenceAssetSet::removeAsset(const QString &baseId,
                                    QString *errorMessage)
{
    if (!ensureWritable(errorMessage))
        return false;
    const QString id = baseId.trimmed();
    if (id == m_primaryBaseId) {
        if (errorMessage) {
            *errorMessage = m_assets.size() <= 1
                    ? QStringLiteral("不能删除最后一张主基准图")
                    : QStringLiteral("不能直接删除主基准图，请先设置另一张主基准图");
        }
        return false;
    }
    for (int index = 0; index < m_assets.size(); ++index) {
        if (m_assets.at(index).baseId != id)
            continue;
        m_assets.removeAt(index);
        normalizeOrder();
        return true;
    }
    if (errorMessage)
        *errorMessage = QStringLiteral("未找到参考资产: %1").arg(id);
    return false;
}

bool ReferenceAssetSet::setPrimaryBaseId(const QString &baseId,
                                         QString *errorMessage)
{
    if (!ensureWritable(errorMessage))
        return false;
    const QString id = baseId.trimmed();
    if (!assetById(id)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("未找到参考资产: %1").arg(id);
        return false;
    }
    m_primaryBaseId = id;
    return true;
}

bool ReferenceAssetSet::setMultiBaseEnabled(bool enabled,
                                             QString *errorMessage)
{
    if (!ensureWritable(errorMessage))
        return false;
    m_multiBaseEnabled = enabled;
    return true;
}

bool ReferenceAssetSet::validate(QString *errorMessage) const
{
    if (m_readOnly)
        return true;
    if (m_assets.size() > kMaximumAssets) {
        if (errorMessage)
            *errorMessage = QStringLiteral("参考资产超过上限 %1").arg(kMaximumAssets);
        return false;
    }
    if (m_assets.isEmpty()) {
        if (!m_primaryBaseId.isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("空参考资产集合不能设置 primaryBaseId");
            return false;
        }
        return true;
    }
    if (!stableId(m_primaryBaseId)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("primaryBaseId 非法或为空");
        return false;
    }

    QSet<QString> ids;
    QSet<int> orders;
    bool primaryFound = false;
    for (const ReferenceAsset &asset : m_assets) {
        if (!stableId(asset.baseId)
                || asset.name.trimmed().isEmpty()
                || asset.order < 0
                || !safeRelativeFileName(asset.path)
                || asset.contentRevision.trimmed().isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("参考资产包含空值、越界路径或非法标识");
            return false;
        }
        if (ids.contains(asset.baseId) || orders.contains(asset.order)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("参考资产 baseId 或 order 重复");
            return false;
        }
        ids.insert(asset.baseId);
        orders.insert(asset.order);
        primaryFound = primaryFound || asset.baseId == m_primaryBaseId;
    }
    if (!primaryFound) {
        if (errorMessage)
            *errorMessage = QStringLiteral("primaryBaseId 未指向现有参考资产");
        return false;
    }
    return true;
}

void ReferenceAssetSet::normalizeOrder()
{
    std::stable_sort(m_assets.begin(), m_assets.end(),
                     [](const ReferenceAsset &left, const ReferenceAsset &right) {
        if (left.order != right.order)
            return left.order < right.order;
        return left.baseId < right.baseId;
    });
    for (int index = 0; index < m_assets.size(); ++index)
        m_assets[index].order = index;
}
