#include "frame/ReferenceImageProvider.h"

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
    if (frame.empty())
        return QImage();

    if (frame.type() == CV_8UC1) {
        QImage image(frame.data,
                     frame.cols,
                     frame.rows,
                     static_cast<int>(frame.step),
                     QImage::Format_Grayscale8);
        return image.copy();
    }

    if (frame.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data,
                     rgb.cols,
                     rgb.rows,
                     static_cast<int>(rgb.step),
                     QImage::Format_RGB888);
        return image.copy();
    }

    if (frame.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(frame, rgba, cv::COLOR_BGRA2RGBA);
        QImage image(rgba.data,
                     rgba.cols,
                     rgba.rows,
                     static_cast<int>(rgba.step),
                     QImage::Format_RGBA8888);
        return image.copy();
    }

    return QImage();
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
