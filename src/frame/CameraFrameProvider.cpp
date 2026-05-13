#include "frame/CameraFrameProvider.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QStringList>

#include <cstring>

CameraFrameProvider::CameraFrameProvider(QObject *parent)
    : QObject(parent)
    , m_grabRunning(0)
{
}

CameraFrameProvider::~CameraFrameProvider()
{
    closeCamera();
}

CameraFrameProvider &CameraFrameProvider::instance()
{
    static CameraFrameProvider provider;
    return provider;
}

bool CameraFrameProvider::openCamera(const QString &devicePath)
{
    if (isOpened()) {
        return true;
    }

    stopGrab();

    const QString path = devicePath.isEmpty()
            ? QStringLiteral("/dev/video0")
            : devicePath;

    {
        QMutexLocker locker(&m_cameraMutex);
        m_devicePath = path;
        m_useNv12Path = false;
        if (m_capture.isOpened()) {
            m_capture.release();
        }
    }

    qDebug() << "[CameraFrameProvider] open camera:" << path;

    const QStringList vmCameraDevices = {
        QStringLiteral("/dev/video00"),
        QStringLiteral("/dev/video10"),
        QStringLiteral("/dev/video20"),
        QStringLiteral("/dev/video30")
    };
    for (const QString &device : vmCameraDevices) {
        if (tryOpenV4L2Device(device)) {
            qDebug() << "[CameraFrameProvider] opened VM camera device:" << device;
            return true;
        }
    }

    if (initCameraWithGStreamer(path)) {
        return true;
    }

    if (initCameraWithV4L2(path)) {
        return true;
    }

    const QString message = tr("无法初始化摄像头: %1").arg(path);
    qWarning() << "[CameraFrameProvider]" << message;
    emit cameraError(message);
    return false;
}

void CameraFrameProvider::closeCamera()
{
    stopGrab();

    {
        QMutexLocker locker(&m_cameraMutex);
        if (m_capture.isOpened()) {
            m_capture.release();
            qDebug() << "[CameraFrameProvider] camera closed";
        }
        m_useNv12Path = false;
    }

    clearFrame();
}

bool CameraFrameProvider::startGrab()
{
    if (isGrabbing()) {
        return true;
    }

    if (!isOpened()) {
        qWarning() << "[CameraFrameProvider] startGrab failed: camera is not opened";
        emit cameraError(tr("相机未打开，无法开始采集"));
        return false;
    }

    m_grabRunning.storeRelease(1);
    m_workerThread = QThread::create([this]() {
        workerLoop();
    });
    m_workerThread->start();

    qDebug() << "[CameraFrameProvider] grab thread started";
    return true;
}

void CameraFrameProvider::stopGrab()
{
    if (!m_grabRunning.loadAcquire() && !m_workerThread) {
        return;
    }

    m_grabRunning.storeRelease(0);

    if (m_workerThread) {
        if (!m_workerThread->wait(3000)) {
            qWarning() << "[CameraFrameProvider] grab thread did not stop in time, terminating";
            m_workerThread->terminate();
            m_workerThread->wait();
        }
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    qDebug() << "[CameraFrameProvider] grab thread stopped";
}

bool CameraFrameProvider::isOpened() const
{
    QMutexLocker locker(&m_cameraMutex);
    return m_capture.isOpened();
}

bool CameraFrameProvider::isGrabbing() const
{
    return m_grabRunning.loadAcquire() != 0;
}

void CameraFrameProvider::setCurrentFrame(const cv::Mat &frame)
{
    if (frame.empty()) {
        clearFrame();
        return;
    }

    const cv::Mat normalized = normalizeFrame(frame);
    if (normalized.empty()) {
        clearFrame();
        return;
    }

    {
        QMutexLocker locker(&m_frameMutex);
        m_currentFrame = normalized.clone();
    }

    emit frameUpdated(matToImage(normalized));
    emit frameUpdatedMat();
}

cv::Mat CameraFrameProvider::currentFrame() const
{
    QMutexLocker locker(&m_frameMutex);
    return m_currentFrame.clone();
}

QImage CameraFrameProvider::currentImage() const
{
    cv::Mat frame;
    {
        QMutexLocker locker(&m_frameMutex);
        frame = m_currentFrame.clone();
    }

    return matToImage(frame);
}

bool CameraFrameProvider::hasFrame() const
{
    QMutexLocker locker(&m_frameMutex);
    return !m_currentFrame.empty();
}

void CameraFrameProvider::clearFrame()
{
    {
        QMutexLocker locker(&m_frameMutex);
        m_currentFrame.release();
    }

    emit frameUpdated(QImage());
    emit frameUpdatedMat();
}

bool CameraFrameProvider::tryOpenV4L2Device(const QString &devicePath)
{
    QMutexLocker locker(&m_cameraMutex);
    if (m_capture.isOpened()) {
        m_capture.release();
    }

    qDebug() << "[CameraFrameProvider] try V4L2 device:" << devicePath;
    if (!m_capture.open(devicePath.toStdString(), cv::CAP_V4L2)) {
        return false;
    }

    m_capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    m_capture.set(cv::CAP_PROP_FPS, 30);

    if (testCameraRead()) {
        m_devicePath = devicePath;
        m_useNv12Path = false;
        qDebug() << QString("[CameraFrameProvider] V4L2 test frame ok: %1, %2x%3")
                        .arg(devicePath)
                        .arg(static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_WIDTH)))
                        .arg(static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_HEIGHT)));
        return true;
    }

    m_capture.release();
    return false;
}

bool CameraFrameProvider::initCameraWithGStreamer(const QString &devicePath)
{
    auto tryPipeline = [this](const QString &pipeline, bool useNv12, const QString &tag) -> bool {
        QMutexLocker locker(&m_cameraMutex);
        if (m_capture.isOpened()) {
            m_capture.release();
        }

        qDebug() << "[CameraFrameProvider] try GStreamer pipeline:" << tag;
        if (!m_capture.open(pipeline.toStdString(), cv::CAP_GSTREAMER)) {
            qDebug() << "[CameraFrameProvider] GStreamer open failed:" << tag;
            return false;
        }

        if (!testCameraRead()) {
            qDebug() << "[CameraFrameProvider] GStreamer opened but no frame:" << tag;
            m_capture.release();
            return false;
        }

        m_useNv12Path = useNv12;
        qDebug() << "[CameraFrameProvider] opened GStreamer pipeline:" << tag
                 << "useNv12=" << m_useNv12Path;
        return true;
    };

    const QString pipeline1920 = QStringLiteral(
        "v4l2src device=%1 io-mode=mmap ! "
        "image/jpeg, width=1920, height=1080, framerate=60/1 ! "
        "mppjpegdec ! "
        "video/x-raw, format=NV12 ! "
        "appsink max-buffers=1 drop=true sync=false"
    ).arg(devicePath);

    if (tryPipeline(pipeline1920, true, QStringLiteral("1920x1080 MJPG->NV12"))) {
        return true;
    }

    const QString pipeline640 = QStringLiteral(
        "v4l2src device=%1 io-mode=mmap ! "
        "image/jpeg, width=640, height=480, framerate=30/1 ! "
        "jpegdec ! "
        "videoconvert ! "
        "video/x-raw, format=BGR ! "
        "appsink max-buffers=1 drop=true sync=false"
    ).arg(devicePath);

    return tryPipeline(pipeline640, false, QStringLiteral("640x480 MJPG->BGR"));
}

bool CameraFrameProvider::initCameraWithV4L2(const QString &devicePath)
{
    QMutexLocker locker(&m_cameraMutex);
    if (m_capture.isOpened()) {
        m_capture.release();
    }

    qDebug() << "[CameraFrameProvider] try requested V4L2 device:" << devicePath;
    if (!m_capture.open(devicePath.toStdString(), cv::CAP_V4L2)) {
        return false;
    }

    m_capture.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('U', 'Y', 'V', 'Y'));
    m_capture.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    m_capture.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    m_capture.set(cv::CAP_PROP_FPS, 30);

    if (testCameraRead()) {
        m_devicePath = devicePath;
        m_useNv12Path = false;
        return true;
    }

    m_capture.release();
    return false;
}

bool CameraFrameProvider::testCameraRead()
{
    if (!m_capture.isOpened()) {
        return false;
    }

    cv::Mat testFrame;
    try {
        if (m_capture.read(testFrame) && !testFrame.empty()) {
            qDebug() << QString("[CameraFrameProvider] test frame: %1x%2 channels=%3 type=%4")
                            .arg(testFrame.cols)
                            .arg(testFrame.rows)
                            .arg(testFrame.channels())
                            .arg(testFrame.type());
            return true;
        }
    } catch (const cv::Exception &e) {
        qWarning() << "[CameraFrameProvider] OpenCV exception while reading test frame:" << e.what();
    }

    return false;
}

cv::Mat CameraFrameProvider::decodeCapturedFrame(const cv::Mat &frame) const
{
    if (frame.empty()) {
        return cv::Mat();
    }

    if (!m_useNv12Path) {
        return normalizeFrame(frame);
    }

    const size_t requiredBytes = 1920U * 1620U;
    const size_t actualBytes = frame.total() * frame.elemSize();
    if (actualBytes < requiredBytes) {
        qWarning() << "[CameraFrameProvider] NV12 frame is smaller than expected:"
                   << actualBytes << "required" << requiredBytes;
        return normalizeFrame(frame);
    }

    cv::Mat nv12(1620, 1920, CV_8UC1);
    std::memcpy(nv12.data, frame.data, 1920 * 1080);
    std::memcpy(nv12.data + 1920 * 1080, frame.data + 1920 * 1088, 1920 * 532);

    cv::Mat bgr;
    cv::cvtColor(nv12, bgr, cv::COLOR_YUV2BGR_NV12);
    if (bgr.cols >= 1920 && bgr.rows >= 1080) {
        cv::rectangle(bgr, cv::Rect(0, 1065, 1920, 15), cv::Scalar(0, 0, 0), -1);
        return bgr(cv::Rect(480, 270, 960, 540)).clone();
    }

    return bgr;
}

void CameraFrameProvider::workerLoop()
{
    qDebug() << "[CameraFrameProvider] worker loop started";

    QElapsedTimer frameTimer;
    int failureCount = 0;
    int frameCount = 0;

    while (m_grabRunning.loadAcquire()) {
        frameTimer.restart();

        cv::Mat capturedFrame;
        bool grabSuccess = false;
        {
            QMutexLocker locker(&m_cameraMutex);
            if (m_capture.isOpened()) {
                try {
                    const bool grabbed = m_capture.grab();
                    grabSuccess = grabbed && m_capture.retrieve(capturedFrame) && !capturedFrame.empty();
                } catch (const cv::Exception &e) {
                    qWarning() << "[CameraFrameProvider] OpenCV exception in worker:" << e.what();
                    grabSuccess = false;
                }
            }
        }

        if (!grabSuccess) {
            ++failureCount;
            if (failureCount == 1 || failureCount % 120 == 0) {
                qWarning() << "[CameraFrameProvider] failed to grab frame, count:" << failureCount;
            }
            QThread::msleep(5);
            continue;
        }

        failureCount = 0;
        const cv::Mat bgrFrame = decodeCapturedFrame(capturedFrame);
        if (bgrFrame.empty()) {
            QThread::msleep(5);
            continue;
        }

        setCurrentFrame(bgrFrame);
        ++frameCount;
        if (frameCount == 1 || frameCount % 120 == 0) {
            qDebug() << QString("[CameraFrameProvider] frame #%1: %2x%3 type=%4")
                            .arg(frameCount)
                            .arg(bgrFrame.cols)
                            .arg(bgrFrame.rows)
                            .arg(bgrFrame.type());
        }

        const int fps = qBound(1, m_workerFps, 100);
        const int targetInterval = 1000 / fps;
        const qint64 elapsed = frameTimer.elapsed();
        if (elapsed < targetInterval) {
            QThread::msleep(static_cast<unsigned long>(targetInterval - elapsed));
        }
    }

    qDebug() << "[CameraFrameProvider] worker loop finished";
}

QImage CameraFrameProvider::matToImage(const cv::Mat &frame)
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

cv::Mat CameraFrameProvider::normalizeFrame(const cv::Mat &frame)
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

    if (frame.type() == CV_8UC2) {
        cv::cvtColor(frame, normalized, cv::COLOR_YUV2BGR_UYVY);
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
