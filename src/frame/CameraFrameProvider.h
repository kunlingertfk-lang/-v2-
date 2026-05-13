#ifndef FRAME_CAMERAFRAMEPROVIDER_H
#define FRAME_CAMERAFRAMEPROVIDER_H

#include <QImage>
#include <QAtomicInt>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QThread>

#include <opencv2/opencv.hpp>

class CameraFrameProvider : public QObject
{
    Q_OBJECT

public:
    static CameraFrameProvider &instance();
    ~CameraFrameProvider() override;

    bool openCamera(const QString &devicePath = QStringLiteral("/dev/video0"));
    void closeCamera();
    bool startGrab();
    void stopGrab();
    bool isOpened() const;
    bool isGrabbing() const;

    void setCurrentFrame(const cv::Mat &frame);
    cv::Mat currentFrame() const;

    QImage currentImage() const;
    bool hasFrame() const;
    void clearFrame();

signals:
    void frameUpdated(const QImage &image);
    void frameUpdatedMat();
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
    cv::VideoCapture m_capture;
    QString m_devicePath;
    bool m_useNv12Path = false;
    QThread *m_workerThread = nullptr;
    QAtomicInt m_grabRunning;
    int m_workerFps = 30;
};

#endif // FRAME_CAMERAFRAMEPROVIDER_H
