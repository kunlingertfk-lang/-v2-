#ifndef FRAME_CAMERAFRAMEPROVIDER_H
#define FRAME_CAMERAFRAMEPROVIDER_H

#include <QImage>
#include <QAtomicInt>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QThread>

#include <opencv2/opencv.hpp>

#include "frame/FrameInputMetadata.h"

class CameraFrameProvider : public QObject
{
    Q_OBJECT

public:
    static CameraFrameProvider &instance();
    ~CameraFrameProvider() override;

    bool openCamera(const QString &devicePath = QStringLiteral("/dev/video0"));
    void closeCamera(const QString &reason = QString());
    bool startGrab();
    void stopGrab();
    bool isOpened() const;
    bool isGrabbing() const;

    void setCurrentFrame(const cv::Mat &frame,
                         const FrameInputMetadata &metadata = FrameInputMetadata());
    cv::Mat currentFrame() const;
    cv::Mat currentFrame(qint64 *frameIndex) const;
    FrameInputMetadata currentFrameMetadata() const;
    qint64 currentFrameIndex() const;

    QImage currentImage() const;
    bool hasFrame() const;
    void clearFrame();

signals:
    void frameUpdated(const QImage &image);
    void frameUpdatedMat();
    void frameIndexChanged(qint64 frameIndex);
    void cameraError(const QString &message);

private:
    explicit CameraFrameProvider(QObject *parent = nullptr);

    bool initCameraWithGStreamer(const QString &devicePath);
    bool initCameraWithV4L2(const QString &devicePath);
    bool testCameraRead();
    bool tryOpenV4L2Device(const QString &devicePath);
    cv::Mat decodeCapturedFrame(const cv::Mat &frame) const;
    void workerLoop();

    static QImage matToImage(const cv::Mat &frame);
    static cv::Mat normalizeFrame(const cv::Mat &frame);

    mutable QMutex m_frameMutex;
    mutable QMutex m_cameraMutex;
    cv::Mat m_currentFrame;
    FrameInputMetadata m_currentFrameMetadata;
    qint64 m_frameIndex = 0;
    cv::VideoCapture m_capture;
    QString m_devicePath;
    bool m_useNv12Path = false;
    QThread *m_workerThread = nullptr;
    QAtomicInt m_grabRunning;
    int m_workerFps = 30;
};

#endif // FRAME_CAMERAFRAMEPROVIDER_H
