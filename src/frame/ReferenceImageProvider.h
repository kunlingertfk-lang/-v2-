#ifndef FRAME_REFERENCEIMAGEPROVIDER_H
#define FRAME_REFERENCEIMAGEPROVIDER_H

#include <QImage>
#include <QMutex>
#include <QObject>

#include <opencv2/core.hpp>

#include "frame/FrameInputMetadata.h"

struct ReferenceFrameSnapshot
{
    cv::Mat frame;
    FrameInputMetadata metadata;
};

class ReferenceImageProvider : public QObject
{
    Q_OBJECT

public:
    static ReferenceImageProvider &instance();

    void setReferenceFrame(const cv::Mat &frame,
                           const FrameInputMetadata &metadata = FrameInputMetadata());
    cv::Mat referenceFrame() const;
    ReferenceFrameSnapshot referenceFrameSnapshot() const;
    FrameInputMetadata referenceFrameMetadata() const;
    QImage referenceImage() const;
    bool hasReferenceFrame() const;
    void clearReferenceFrame();
    static cv::Mat normalizeReferenceFrame(const cv::Mat &frame);

signals:
    void referenceFrameChanged(const QImage &image);

private:
    explicit ReferenceImageProvider(QObject *parent = nullptr);

    static QImage matToImage(const cv::Mat &frame);
    static cv::Mat normalizeFrame(const cv::Mat &frame);

    mutable QMutex m_mutex;
    cv::Mat m_referenceFrame;
    FrameInputMetadata m_referenceFrameMetadata;
};

#endif // FRAME_REFERENCEIMAGEPROVIDER_H
