#ifndef FRAME_REFERENCEIMAGEPROVIDER_H
#define FRAME_REFERENCEIMAGEPROVIDER_H

#include <QImage>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

#include "frame/FrameInputMetadata.h"

struct ReferenceFrameSnapshot
{
    cv::Mat frame;
    FrameInputMetadata metadata;
    QString baseId;
    QString contentRevision;
};

/** One decoded frame in the scheme-level reference asset set. */
struct ReferenceFrameEntry
{
    QString baseId;
    QString name;
    cv::Mat frame;
    FrameInputMetadata metadata;
    QString contentRevision;
    int displayOrder = 0;
};

/** Atomically copied runtime view of the complete scheme-level Base set. */
struct ReferenceFrameSetSnapshot
{
    QList<ReferenceFrameEntry> entries;
    QMap<QString, cv::Mat> frames;
    QMap<QString, QString> contentRevisions;
    QString primaryBaseId;
    ReferenceFrameSnapshot primary;
};

class ReferenceImageProvider : public QObject
{
    Q_OBJECT

public:
    static ReferenceImageProvider &instance();

    /**
     * Compatibility setter for the primary reference frame.  Existing tools
     * continue to see this image through the no-argument accessors.
     */
    void setReferenceFrame(const cv::Mat &frame,
                           const FrameInputMetadata &metadata = FrameInputMetadata());
    /** Atomically replace the decoded 1..8 Base frame set. */
    bool setReferenceFrames(const QList<ReferenceFrameEntry> &entries,
                            const QString &primaryBaseId,
                            QString *errorMessage = nullptr);
    /** Update one decoded Base without changing any other Base. */
    bool setReferenceFrameForBase(
            const ReferenceFrameEntry &entry,
            bool makePrimary = false,
            QString *errorMessage = nullptr);
    bool setPrimaryBaseId(const QString &baseId,
                          QString *errorMessage = nullptr);

    cv::Mat referenceFrame() const;
    cv::Mat referenceFrame(const QString &baseId) const;
    ReferenceFrameSnapshot referenceFrameSnapshot() const;
    ReferenceFrameSnapshot referenceFrameSnapshot(const QString &baseId) const;
    ReferenceFrameSetSnapshot referenceFrameSetSnapshot() const;
    FrameInputMetadata referenceFrameMetadata() const;
    FrameInputMetadata referenceFrameMetadata(const QString &baseId) const;
    QImage referenceImage() const;
    QImage referenceImage(const QString &baseId) const;
    bool hasReferenceFrame() const;
    bool hasReferenceFrame(const QString &baseId) const;
    QString primaryBaseId() const;
    QStringList baseIds() const;
    QList<ReferenceFrameEntry> referenceFrames() const;
    QMap<QString, cv::Mat> referenceFrameMap() const;
    QMap<QString, QString> referenceRevisionMap() const;
    void clearReferenceFrame();
    void clearReferenceFrames();
    bool removeReferenceFrame(const QString &baseId,
                              QString *errorMessage = nullptr);
    static cv::Mat normalizeReferenceFrame(const cv::Mat &frame);

signals:
    void referenceFrameChanged(const QImage &image);
    void referenceFrameSetChanged();

private:
    explicit ReferenceImageProvider(QObject *parent = nullptr);

    static QImage matToImage(const cv::Mat &frame);
    static cv::Mat normalizeFrame(const cv::Mat &frame);
    static QString normalizedBaseId(const QString &baseId);

    mutable QMutex m_mutex;
    QMap<QString, ReferenceFrameEntry> m_referenceFrames;
    QString m_primaryBaseId;
};

#endif // FRAME_REFERENCEIMAGEPROVIDER_H
