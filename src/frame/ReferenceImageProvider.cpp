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

void ReferenceImageProvider::setReferenceFrame(const cv::Mat &frame)
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

    {
        QMutexLocker locker(&m_mutex);
        m_referenceFrame = normalized.clone();
    }

    emit referenceFrameChanged(matToImage(normalized));
}

cv::Mat ReferenceImageProvider::referenceFrame() const
{
    QMutexLocker locker(&m_mutex);
    return m_referenceFrame.clone();
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

    if (frame.type() == CV_8UC3)
        return frame.clone();

    cv::Mat normalized;
    if (frame.type() == CV_8UC1) {
        cv::cvtColor(frame, normalized, cv::COLOR_GRAY2BGR);
        return normalized;
    }

    if (frame.type() == CV_8UC4) {
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
