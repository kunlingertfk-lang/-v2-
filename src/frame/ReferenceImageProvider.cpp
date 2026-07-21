#include "frame/ReferenceImageProvider.h"

#include "frame/MatImageConverter.h"

#include <QMutexLocker>

#include <opencv2/imgproc.hpp>

ReferenceImageProvider::ReferenceImageProvider(QObject *parent)
    : QObject(parent)
{
}

ReferenceImageProvider &ReferenceImageProvider::instance()
{
    static ReferenceImageProvider provider;
    return provider;
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

    {
        QMutexLocker locker(&m_mutex);
        m_referenceFrame = normalized.clone();
        m_referenceFrameMetadata = resolvedMetadata;
    }

    emit referenceFrameChanged(matToImage(normalized));
}

cv::Mat ReferenceImageProvider::referenceFrame() const
{
    QMutexLocker locker(&m_mutex);
    return m_referenceFrame.clone();
}

ReferenceFrameSnapshot ReferenceImageProvider::referenceFrameSnapshot() const
{
    QMutexLocker locker(&m_mutex);
    ReferenceFrameSnapshot snapshot;
    snapshot.frame = m_referenceFrame.clone();
    snapshot.metadata = m_referenceFrameMetadata;
    return snapshot;
}

FrameInputMetadata ReferenceImageProvider::referenceFrameMetadata() const
{
    QMutexLocker locker(&m_mutex);
    return m_referenceFrameMetadata;
}

QImage ReferenceImageProvider::referenceImage() const
{
    cv::Mat frame;
    {
        QMutexLocker locker(&m_mutex);
        frame = m_referenceFrame.clone();
    }

    return matToImage(frame);
}

bool ReferenceImageProvider::hasReferenceFrame() const
{
    QMutexLocker locker(&m_mutex);
    return !m_referenceFrame.empty();
}

void ReferenceImageProvider::clearReferenceFrame()
{
    {
        QMutexLocker locker(&m_mutex);
        m_referenceFrame.release();
        m_referenceFrameMetadata = FrameInputMetadata();
    }

    emit referenceFrameChanged(QImage());
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
