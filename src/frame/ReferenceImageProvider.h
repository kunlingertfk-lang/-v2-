#ifndef FRAME_REFERENCEIMAGEPROVIDER_H
#define FRAME_REFERENCEIMAGEPROVIDER_H

#include <QImage>
#include <QMutex>
#include <QObject>

#include <opencv2/core.hpp>

class ReferenceImageProvider : public QObject
{
    Q_OBJECT

public:
    static ReferenceImageProvider &instance();

    void setReferenceFrame(const cv::Mat &frame);
    cv::Mat referenceFrame() const;
    QImage referenceImage() const;
    bool hasReferenceFrame() const;
    void clearReferenceFrame();

signals:
    void referenceFrameChanged(const QImage &image);

private:
    explicit ReferenceImageProvider(QObject *parent = nullptr);

    static QImage matToImage(const cv::Mat &frame);
    static cv::Mat normalizeFrame(const cv::Mat &frame);

    mutable QMutex m_mutex;
    cv::Mat m_referenceFrame;
};

#endif // FRAME_REFERENCEIMAGEPROVIDER_H
