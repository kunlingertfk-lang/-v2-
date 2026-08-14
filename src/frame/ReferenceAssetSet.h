#ifndef FRAME_REFERENCEASSETSET_H
#define FRAME_REFERENCEASSETSET_H

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>

#include "frame/FrameInputMetadata.h"

struct ReferenceAsset
{
    QString baseId;
    QString name;
    int order = 0;
    QString path;
    FrameInputMetadata inputMetadata;
    QString contentRevision;

    // Unknown fields are retained so a supported v1 envelope can be edited
    // without discarding extensions written by another component.
    QJsonObject extra;
    QJsonObject inputMetadataExtra;
};

class ReferenceAssetSet
{
public:
    static constexpr int kCurrentVersion = 1;
    static constexpr int kMaximumAssets = 8;

    static ReferenceAssetSet fromJson(const QJsonValue &value,
                                      QString *errorMessage = nullptr);
    static ReferenceAssetSet fromLegacyReference(
            const QString &path,
            const FrameInputMetadata &metadata,
            const QString &contentRevision = QString());

    QJsonValue toJson() const;

    int version() const { return m_version; }
    const QVector<ReferenceAsset> &assets() const { return m_assets; }
    QString primaryBaseId() const { return m_primaryBaseId; }
    bool multiBaseEnabled() const { return m_multiBaseEnabled; }
    const ReferenceAsset *assetById(const QString &baseId) const;
    const ReferenceAsset *primaryAsset() const;
    int size() const { return m_assets.size(); }
    bool isEmpty() const { return m_assets.isEmpty(); }
    bool isReadOnly() const { return m_readOnly; }
    QString readOnlyReason() const { return m_readOnlyReason; }

    bool addAsset(const ReferenceAsset &asset,
                  bool makePrimary,
                  QString *errorMessage = nullptr);
    bool replaceAsset(const ReferenceAsset &asset,
                      QString *errorMessage = nullptr);
    bool removeAsset(const QString &baseId,
                     QString *errorMessage = nullptr);
    bool setPrimaryBaseId(const QString &baseId,
                          QString *errorMessage = nullptr);
    bool setMultiBaseEnabled(bool enabled,
                             QString *errorMessage = nullptr);
    bool validate(QString *errorMessage = nullptr) const;
    void normalizeOrder();

private:
    static ReferenceAssetSet readOnlyRaw(const QJsonValue &raw,
                                         const QString &reason);
    bool ensureWritable(QString *errorMessage) const;

    int m_version = kCurrentVersion;
    QVector<ReferenceAsset> m_assets;
    QString m_primaryBaseId;
    bool m_multiBaseEnabled = false;
    QJsonObject m_extra;
    bool m_readOnly = false;
    QString m_readOnlyReason;
    QJsonValue m_raw;
};

#endif // FRAME_REFERENCEASSETSET_H
