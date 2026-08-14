#include "frame/ReferenceImageProvider.h"

#include "frame/MatImageConverter.h"

#include <QMutexLocker>
#include <QStringList>

#include <opencv2/imgproc.hpp>

#include <algorithm>

ReferenceImageProvider::ReferenceImageProvider(QObject *parent)
    : QObject(parent)
{
}

ReferenceImageProvider &ReferenceImageProvider::instance()
{
    static ReferenceImageProvider provider;
    return provider;
}

QString ReferenceImageProvider::normalizedBaseId(const QString &baseId)
{
    return baseId.trimmed();
}

void ReferenceImageProvider::setReferenceFrame(const cv::Mat &frame,
                                               const FrameInputMetadata &metadata)
{
    if (frame.empty()) {
        clearReferenceFrame();
        return;
    }

    const cv::Mat normalized = normalizeFrame(frame);
    if (normalized.empty()) {
        clearReferenceFrame();
        return;
    }

    // ReferenceImageProvider stores the normalized Mat. Runtime metadata must describe
    // that Mat rather than the camera/file representation that existed before conversion.
    const QString source = metadata.source.trimmed().isEmpty()
            ? QStringLiteral("reference") : metadata.source;
    const FrameInputMetadata resolvedMetadata =
            FrameInputMetadata::fromMat(normalized, source);

    ReferenceFrameEntry entry;
    {
        QMutexLocker locker(&m_mutex);
        entry.baseId = m_primaryBaseId.trimmed().isEmpty()
                ? QStringLiteral("base-0") : m_primaryBaseId;
        const auto existing = m_referenceFrames.constFind(entry.baseId);
        if (existing != m_referenceFrames.cend()) {
            entry.name = existing->name;
            entry.displayOrder = existing->displayOrder;
        }
    }
    if (entry.name.trimmed().isEmpty())
        entry.name = QStringLiteral("Base 1");
    entry.frame = normalized;
    entry.metadata = resolvedMetadata;
    setReferenceFrameForBase(entry, true, nullptr);
}

bool ReferenceImageProvider::setReferenceFrames(
        const QList<ReferenceFrameEntry> &entries,
        const QString &primaryBaseId,
        QString *errorMessage)
{
    if (entries.size() > 8) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Reference frame set supports at most 8 Base images.");
        return false;
    }

    QMap<QString, ReferenceFrameEntry> normalizedEntries;
    for (const ReferenceFrameEntry &source : entries) {
        const QString baseId = normalizedBaseId(source.baseId);
        if (baseId.isEmpty() || normalizedEntries.contains(baseId)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Reference Base ID is empty or duplicated: %1")
                        .arg(baseId);
            return false;
        }
        const cv::Mat normalized = normalizeFrame(source.frame);
        if (normalized.empty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Reference Base '%1' has an unsupported or empty frame.")
                        .arg(baseId);
            return false;
        }
        ReferenceFrameEntry entry = source;
        entry.baseId = baseId;
        entry.frame = normalized.clone();
        entry.metadata = FrameInputMetadata::fromMat(
                    normalized,
                    source.metadata.source.trimmed().isEmpty()
                    ? QStringLiteral("reference:%1").arg(baseId)
                    : source.metadata.source);
        normalizedEntries.insert(baseId, entry);
    }

    QString resolvedPrimary = normalizedBaseId(primaryBaseId);
    if (!normalizedEntries.isEmpty() && !normalizedEntries.contains(resolvedPrimary)) {
        if (resolvedPrimary.isEmpty()) {
            auto first = normalizedEntries.cbegin();
            for (auto it = normalizedEntries.cbegin();
                 it != normalizedEntries.cend(); ++it) {
                if (it->displayOrder < first->displayOrder)
                    first = it;
            }
            resolvedPrimary = first.key();
        } else {
            if (errorMessage)
                *errorMessage = QStringLiteral("Primary reference Base '%1' does not exist.")
                        .arg(resolvedPrimary);
            return false;
        }
    }

    QImage primaryImage;
    {
        QMutexLocker locker(&m_mutex);
        m_referenceFrames = normalizedEntries;
        m_primaryBaseId = resolvedPrimary;
        if (m_referenceFrames.contains(m_primaryBaseId))
            primaryImage = matToImage(m_referenceFrames.value(m_primaryBaseId).frame);
    }
    emit referenceFrameSetChanged();
    emit referenceFrameChanged(primaryImage);
    return true;
}

bool ReferenceImageProvider::setReferenceFrameForBase(
        const ReferenceFrameEntry &source,
        bool makePrimary,
        QString *errorMessage)
{
    const QString baseId = normalizedBaseId(source.baseId);
    const cv::Mat normalized = normalizeFrame(source.frame);
    if (baseId.isEmpty() || normalized.empty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Reference Base ID or frame is invalid.");
        return false;
    }

    ReferenceFrameEntry entry = source;
    entry.baseId = baseId;
    entry.frame = normalized.clone();
    entry.metadata = FrameInputMetadata::fromMat(
                normalized,
                source.metadata.source.trimmed().isEmpty()
                ? QStringLiteral("reference:%1").arg(baseId)
                : source.metadata.source);

    QImage primaryImage;
    bool primaryChanged = false;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_referenceFrames.contains(baseId) && m_referenceFrames.size() >= 8) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Reference frame set already contains 8 Base images.");
            return false;
        }
        m_referenceFrames.insert(baseId, entry);
        if (makePrimary || m_primaryBaseId.isEmpty()) {
            primaryChanged = m_primaryBaseId != baseId;
            m_primaryBaseId = baseId;
        }
        if (m_primaryBaseId == baseId)
            primaryImage = matToImage(entry.frame);
    }
    emit referenceFrameSetChanged();
    if (primaryChanged || !primaryImage.isNull())
        emit referenceFrameChanged(primaryImage);
    return true;
}

bool ReferenceImageProvider::setPrimaryBaseId(const QString &baseId,
                                               QString *errorMessage)
{
    const QString normalized = normalizedBaseId(baseId);
    QImage image;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_referenceFrames.contains(normalized)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Reference Base '%1' does not exist.")
                        .arg(normalized);
            return false;
        }
        if (m_primaryBaseId == normalized)
            return true;
        m_primaryBaseId = normalized;
        image = matToImage(m_referenceFrames.value(normalized).frame);
    }
    emit referenceFrameSetChanged();
    emit referenceFrameChanged(image);
    return true;
}

cv::Mat ReferenceImageProvider::referenceFrame() const
{
    QMutexLocker locker(&m_mutex);
    return m_referenceFrames.value(m_primaryBaseId).frame.clone();
}

cv::Mat ReferenceImageProvider::referenceFrame(const QString &baseId) const
{
    QMutexLocker locker(&m_mutex);
    return m_referenceFrames.value(normalizedBaseId(baseId)).frame.clone();
}

ReferenceFrameSnapshot ReferenceImageProvider::referenceFrameSnapshot() const
{
    QMutexLocker locker(&m_mutex);
    ReferenceFrameSnapshot snapshot;
    const auto it = m_referenceFrames.constFind(m_primaryBaseId);
    if (it == m_referenceFrames.cend())
        return snapshot;
    snapshot.frame = it->frame.clone();
    snapshot.metadata = it->metadata;
    snapshot.baseId = it->baseId;
    snapshot.contentRevision = it->contentRevision;
    return snapshot;
}

ReferenceFrameSnapshot ReferenceImageProvider::referenceFrameSnapshot(
        const QString &baseId) const
{
    QMutexLocker locker(&m_mutex);
    ReferenceFrameSnapshot snapshot;
    const auto it = m_referenceFrames.constFind(normalizedBaseId(baseId));
    if (it == m_referenceFrames.cend())
        return snapshot;
    snapshot.frame = it->frame.clone();
    snapshot.metadata = it->metadata;
    snapshot.baseId = it->baseId;
    snapshot.contentRevision = it->contentRevision;
    return snapshot;
}

ReferenceFrameSetSnapshot ReferenceImageProvider::referenceFrameSetSnapshot() const
{
    QMutexLocker locker(&m_mutex);
    ReferenceFrameSetSnapshot snapshot;
    snapshot.primaryBaseId = m_primaryBaseId;
    for (auto it = m_referenceFrames.cbegin();
         it != m_referenceFrames.cend(); ++it) {
        const cv::Mat copiedFrame = it->frame.clone();
        snapshot.frames.insert(it.key(), copiedFrame);
        snapshot.contentRevisions.insert(it.key(), it->contentRevision);
        ReferenceFrameEntry copiedEntry = *it;
        copiedEntry.frame = copiedFrame;
        snapshot.entries.append(copiedEntry);
        if (it.key() != m_primaryBaseId)
            continue;
        snapshot.primary.frame = copiedFrame;
        snapshot.primary.metadata = it->metadata;
        snapshot.primary.baseId = it->baseId;
        snapshot.primary.contentRevision = it->contentRevision;
    }
    std::sort(snapshot.entries.begin(), snapshot.entries.end(),
              [](const ReferenceFrameEntry &left,
                 const ReferenceFrameEntry &right) {
        if (left.displayOrder != right.displayOrder)
            return left.displayOrder < right.displayOrder;
        return left.baseId < right.baseId;
    });
    return snapshot;
}

FrameInputMetadata ReferenceImageProvider::referenceFrameMetadata() const
{
    QMutexLocker locker(&m_mutex);
    return m_referenceFrames.value(m_primaryBaseId).metadata;
}

FrameInputMetadata ReferenceImageProvider::referenceFrameMetadata(
        const QString &baseId) const
{
    QMutexLocker locker(&m_mutex);
    return m_referenceFrames.value(normalizedBaseId(baseId)).metadata;
}

QImage ReferenceImageProvider::referenceImage() const
{
    QMutexLocker locker(&m_mutex);
    return matToImage(m_referenceFrames.value(m_primaryBaseId).frame);
}

QImage ReferenceImageProvider::referenceImage(const QString &baseId) const
{
    return matToImage(referenceFrame(baseId));
}

bool ReferenceImageProvider::hasReferenceFrame() const
{
    QMutexLocker locker(&m_mutex);
    const auto it = m_referenceFrames.constFind(m_primaryBaseId);
    return it != m_referenceFrames.cend() && !it->frame.empty();
}

bool ReferenceImageProvider::hasReferenceFrame(const QString &baseId) const
{
    QMutexLocker locker(&m_mutex);
    const auto it = m_referenceFrames.constFind(normalizedBaseId(baseId));
    return it != m_referenceFrames.cend() && !it->frame.empty();
}

QString ReferenceImageProvider::primaryBaseId() const
{
    QMutexLocker locker(&m_mutex);
    return m_primaryBaseId;
}

QStringList ReferenceImageProvider::baseIds() const
{
    QMutexLocker locker(&m_mutex);
    QList<ReferenceFrameEntry> entries = m_referenceFrames.values();
    std::sort(entries.begin(), entries.end(), [](const ReferenceFrameEntry &left,
                                                  const ReferenceFrameEntry &right) {
        if (left.displayOrder != right.displayOrder)
            return left.displayOrder < right.displayOrder;
        return left.baseId < right.baseId;
    });
    QStringList ids;
    for (const ReferenceFrameEntry &entry : entries)
        ids.append(entry.baseId);
    return ids;
}

QList<ReferenceFrameEntry> ReferenceImageProvider::referenceFrames() const
{
    QMutexLocker locker(&m_mutex);
    QList<ReferenceFrameEntry> entries;
    for (const ReferenceFrameEntry &stored : m_referenceFrames) {
        ReferenceFrameEntry copy = stored;
        copy.frame = stored.frame.clone();
        entries.append(copy);
    }
    std::sort(entries.begin(), entries.end(), [](const ReferenceFrameEntry &left,
                                                  const ReferenceFrameEntry &right) {
        if (left.displayOrder != right.displayOrder)
            return left.displayOrder < right.displayOrder;
        return left.baseId < right.baseId;
    });
    return entries;
}

QMap<QString, cv::Mat> ReferenceImageProvider::referenceFrameMap() const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, cv::Mat> frames;
    for (auto it = m_referenceFrames.cbegin();
         it != m_referenceFrames.cend(); ++it) {
        frames.insert(it.key(), it->frame.clone());
    }
    return frames;
}

QMap<QString, QString> ReferenceImageProvider::referenceRevisionMap() const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, QString> revisions;
    for (auto it = m_referenceFrames.cbegin();
         it != m_referenceFrames.cend(); ++it) {
        revisions.insert(it.key(), it->contentRevision);
    }
    return revisions;
}

void ReferenceImageProvider::clearReferenceFrame()
{
    clearReferenceFrames();
}

void ReferenceImageProvider::clearReferenceFrames()
{
    {
        QMutexLocker locker(&m_mutex);
        m_referenceFrames.clear();
        m_primaryBaseId.clear();
    }
    emit referenceFrameSetChanged();
    emit referenceFrameChanged(QImage());
}

bool ReferenceImageProvider::removeReferenceFrame(const QString &baseId,
                                                   QString *errorMessage)
{
    const QString normalized = normalizedBaseId(baseId);
    QImage primaryImage;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_referenceFrames.contains(normalized)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("Reference Base '%1' does not exist.")
                        .arg(normalized);
            return false;
        }
        m_referenceFrames.remove(normalized);
        if (m_primaryBaseId == normalized) {
            m_primaryBaseId.clear();
            if (!m_referenceFrames.isEmpty()) {
                auto first = m_referenceFrames.cbegin();
                for (auto it = m_referenceFrames.cbegin();
                     it != m_referenceFrames.cend(); ++it) {
                    if (it->displayOrder < first->displayOrder)
                        first = it;
                }
                m_primaryBaseId = first.key();
                primaryImage = matToImage(first->frame);
            }
        }
    }
    emit referenceFrameSetChanged();
    emit referenceFrameChanged(primaryImage.isNull()
                               ? referenceImage() : primaryImage);
    return true;
}

cv::Mat ReferenceImageProvider::normalizeReferenceFrame(const cv::Mat &frame)
{
    return normalizeFrame(frame);
}

QImage ReferenceImageProvider::matToImage(const cv::Mat &frame)
{
    return MatImageConverter::matToDisplayImage(frame, QStringLiteral("ReferenceImageProvider"));
}

cv::Mat ReferenceImageProvider::normalizeFrame(const cv::Mat &frame)
{
    if (frame.empty())
        return cv::Mat();

    cv::Mat normalized;
    if ((frame.depth() == CV_8U || frame.depth() == CV_16U) && frame.channels() == 3)
        return frame.clone();

    if ((frame.depth() == CV_8U || frame.depth() == CV_16U) && frame.channels() == 1) {
        cv::cvtColor(frame, normalized, cv::COLOR_GRAY2BGR);
        return normalized;
    }

    if ((frame.depth() == CV_8U || frame.depth() == CV_16U) && frame.channels() == 4) {
        cv::cvtColor(frame, normalized, cv::COLOR_BGRA2BGR);
        return normalized;
    }

    if (frame.depth() != CV_8U) {
        cv::Mat converted;
        frame.convertTo(converted, CV_8U);
        return normalizeFrame(converted);
    }

    return cv::Mat();
}
