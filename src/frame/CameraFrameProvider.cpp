#include "frame/CameraFrameProvider.h"

#include "frame/MatImageConverter.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMutexLocker>
#include <QStringList>

#include <cstring>

#include <opencv2/imgproc.hpp>

CameraFrameProvider::CameraFrameProvider(QObject *parent)
    : QObject(parent)
    , m_grabRunning(0)
{
}

CameraFrameProvider::~CameraFrameProvider()
{
    m_grabRunning.storeRelease(0);

    if (m_workerThread) {
        m_workerThread->requestInterruption();

        if (!m_workerThread->wait(5000)) {
            qCritical()
                    << "[CameraFrameProvider] worker did not exit in 5 seconds;"
                    << "waiting indefinitely to avoid use-after-free";

            // 当前结构下只能继续等待。
            // 不能 terminate、delete、release m_capture。
            m_workerThread->wait();
        }

        // 只有确认线程结束后才能删除。
        Q_ASSERT(m_workerThread->isFinished());
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    // worker 已结束，不可能再访问 m_capture。
    QMutexLocker locker(&m_cameraMutex);
    if (m_capture.isOpened())
        m_capture.release();
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

    if(!stopGrab()){
        qCritical() << "[CameraFrameProvider] open aborted because worker is still running";   
        return false;
    };

    const QString path = devicePath.isEmpty()
            ? QStringLiteral("/dev/video0")
            : devicePath;
    {
        QMutexLocker locker(&m_cameraMutex);
        m_devicePath = path;
        m_captureMode.clear();
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

void CameraFrameProvider::closeCamera(const QString &reason)
{
    if(!stopGrab()){
        qCritical() << "[CameraFrameProvider] close aborted beacuse worker is still running";
        return;
    }

    {
        QMutexLocker locker(&m_cameraMutex);
        if (m_capture.isOpened()) {
            m_capture.release();
            qDebug() << "[CAMERA-LIFECYCLE] closeCamera reason="
                     << (reason.isEmpty() ? QStringLiteral("unspecified") : reason);
            qDebug() << "[CameraFrameProvider] camera closed";
        }
        m_useNv12Path = false;
        m_captureMode.clear();
    }

    clearFrame();
}

bool CameraFrameProvider::startGrab()
{
    if (m_workerThread){
        if(m_workerThread->isRunning()){
            qWarning() << "[CameraFrameProvider] previous grab thread is still running";
            emit cameraError(tr("上一次采集线程尚未退出"));
            return false;
        }
        delete m_workerThread;
        m_workerThread = nullptr;
    }

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

/*===============没有返回值。使用terminate()存在资源未完全释放的风险===================*/
// void CameraFrameProvider::stopGrab(int timeoutMs)
// {
//     if (!m_grabRunning.loadAcquire() && !m_workerThread) {
//         return;
//     }
//     m_grabRunning.storeRelease(0);
//     if (m_workerThread) {
//         if (!m_workerThread->wait(3000)) {
//             qWarning() << "[CameraFrameProvider] grab thread did not stop in time, terminating";
//             m_workerThread->terminate();
//             m_workerThread->wait();
//         }
//         delete m_workerThread;
//         m_workerThread = nullptr;
//     }
//     qDebug() << "[CameraFrameProvider] grab thread stopped";
// }

//===============有返回值。使用requestInterruption()等待线程退出，避免资源未释放的风险===================*/
bool CameraFrameProvider::stopGrab(int timeoutMs)
{
    m_grabRunning.storeRelease(0);

    QThread *thread = m_workerThread;
    if(!thread) 
        return true;

    thread->requestInterruption();

    if(!thread->wait(timeoutMs)){
        const QString message = tr("相机采集线程停止超时，暂不释放相机资源");
        qCritical() << "[CameraFrameProvider]" << message;
        emit cameraError(message);

        //不能terminate线程，可能导致资源未释放，后续无法重新打开相机
        //不能delete，因为线程还在运行，delete会导致崩溃
        //也不能继续release m_capture，因为线程还在运行，release会导致崩溃
        return false;
    }

    delete thread;
    m_workerThread = nullptr;

    qDebug() << "[CameraFrameProvider] grab thread stopped";
    return true;
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

void CameraFrameProvider::setCurrentFrame(const cv::Mat &frame,
                                          const FrameInputMetadata &metadata)
{
    if (frame.empty()) {
        clearFrame();
        return;
    }

    FrameInputMetadata resolvedMetadata = metadata;
    if (resolvedMetadata.colorMode == QStringLiteral("unknown")
            && resolvedMetadata.pixelFormat.isEmpty()
            && resolvedMetadata.originalChannels == 0
            && resolvedMetadata.originalDepth < 0
            && resolvedMetadata.source.isEmpty()) {
        resolvedMetadata = FrameInputMetadata::fromMat(frame, QStringLiteral("camera"));
    }

    const cv::Mat normalized = normalizeFrame(frame);
    if (normalized.empty()) {
        clearFrame();
        return;
    }

    qint64 frameIndex = 0;
    {
        QMutexLocker locker(&m_frameMutex);
        m_currentFrame = normalized.clone();
        m_currentFrameMetadata = resolvedMetadata;
        frameIndex = ++m_frameIndex;
    }

    emit frameUpdated(matToImage(normalized));
    emit frameUpdatedMat();
    emit frameIndexChanged(frameIndex);
}

cv::Mat CameraFrameProvider::currentFrame() const
{
    QMutexLocker locker(&m_frameMutex);
    return m_currentFrame.clone();
}

cv::Mat CameraFrameProvider::currentFrame(qint64 *frameIndex) const
{
    QMutexLocker locker(&m_frameMutex);
    if (frameIndex)
        *frameIndex = m_frameIndex;
    return m_currentFrame.clone();
}

CameraFrameSnapshot CameraFrameProvider::currentFrameSnapshot() const
{
    QMutexLocker locker(&m_frameMutex);
    CameraFrameSnapshot snapshot;
    snapshot.frame = m_currentFrame.clone();
    snapshot.frameIndex = m_frameIndex;
    snapshot.metadata = m_currentFrameMetadata;
    return snapshot;
}

FrameInputMetadata CameraFrameProvider::currentFrameMetadata() const
{
    QMutexLocker locker(&m_frameMutex);
    return m_currentFrameMetadata;
}

qint64 CameraFrameProvider::currentFrameIndex() const
{
    QMutexLocker locker(&m_frameMutex);
    return m_frameIndex;
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
    qint64 frameIndex = 0;
    {
        QMutexLocker locker(&m_frameMutex);
        m_currentFrame.release();
        m_currentFrameMetadata = FrameInputMetadata();
        frameIndex = ++m_frameIndex;
    }

    emit frameUpdated(QImage());
    emit frameUpdatedMat();
    emit frameIndexChanged(frameIndex);
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
        m_captureMode = QStringLiteral("v4l2");
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
    const QString pipeline1920 = QStringLiteral(
        "v4l2src device=%1 io-mode=mmap ! "
        "image/jpeg, width=1920, height=1080, framerate=60/1 ! "
        "mppjpegdec ! "
        "video/x-raw, format=NV12 ! "
        "appsink max-buffers=1 drop=true sync=false"
    ).arg(devicePath);

    if (tryOpenGStreamerPipeline(pipeline1920,
                                 true,
                                 QStringLiteral("1920x1080 MJPG->NV12"),
                                 QStringLiteral("gstreamer-1920-nv12"))) {
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

    return tryOpenGStreamerPipeline(pipeline640,
                                    false,
                                    QStringLiteral("640x480 MJPG->BGR"),
                                    QStringLiteral("gstreamer-640-bgr"));
}

bool CameraFrameProvider::tryOpenGStreamerPipeline(const QString &pipeline,
                                                   bool useNv12,
                                                   const QString &tag,
                                                   const QString &captureMode)
{
    QMutexLocker locker(&m_cameraMutex);
    if (m_capture.isOpened()) {
        m_capture.release();
    }

    qDebug() << "[CameraFrameProvider] try GStreamer pipeline:" << tag;
    if (!m_capture.open(pipeline.toStdString(), cv::CAP_GSTREAMER)) {
        qDebug() << "[CameraFrameProvider] GStreamer open failed:" << tag;
        return false;
    }

    const bool timeoutSupported = m_capture.set(cv::CAP_PROP_READ_TIMEOUT_MSEC, 500);
    qDebug() << "[CameraFrameProvider] CAP_PROP_READ_TIMEOUT_MSEC supported:" << timeoutSupported;

    if (!testCameraRead()) {
        qDebug() << "[CameraFrameProvider] GStreamer opened but no frame:" << tag;
        m_capture.release();
        return false;
    }

    m_captureMode = captureMode;
    m_useNv12Path = useNv12;
    qDebug() << "[CameraFrameProvider] opened GStreamer pipeline:" << tag
             << "useNv12=" << m_useNv12Path;
    return true;
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
        m_captureMode = QStringLiteral("v4l2");
        m_useNv12Path = false;
        return true;
    }

    m_capture.release();
    return false;
}

bool CameraFrameProvider::tryReconnectCamera()
{
    QString devicePath;
    QString captureMode;
    {
        QMutexLocker locker(&m_cameraMutex);
        devicePath = m_devicePath;
        captureMode = m_captureMode;
        if (m_capture.isOpened()) {
            m_capture.release();
        }
        m_useNv12Path = false;
    }

    if (devicePath.isEmpty() || !QFileInfo::exists(devicePath)) {
        return false;
    }

    if (captureMode == QStringLiteral("gstreamer-1920-nv12")) {
        const QString pipeline = QStringLiteral(
            "v4l2src device=%1 io-mode=mmap ! "
            "image/jpeg, width=1920, height=1080, framerate=60/1 ! "
            "mppjpegdec ! "
            "video/x-raw, format=NV12 ! "
            "appsink max-buffers=1 drop=true sync=false"
        ).arg(devicePath);
        return tryOpenGStreamerPipeline(pipeline,
                                        true,
                                        QStringLiteral("1920x1080 MJPG->NV12 reconnect"),
                                        captureMode);
    }

    if (captureMode == QStringLiteral("gstreamer-640-bgr")) {
        const QString pipeline = QStringLiteral(
            "v4l2src device=%1 io-mode=mmap ! "
            "image/jpeg, width=640, height=480, framerate=30/1 ! "
            "jpegdec ! "
            "videoconvert ! "
            "video/x-raw, format=BGR ! "
            "appsink max-buffers=1 drop=true sync=false"
        ).arg(devicePath);
        return tryOpenGStreamerPipeline(pipeline,
                                        false,
                                        QStringLiteral("640x480 MJPG->BGR reconnect"),
                                        captureMode);
    }

    return initCameraWithV4L2(devicePath);
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

    QThread *currentThread = QThread::currentThread();

    qDebug() << "[CameraFrameProvider] worker loop started";

    QElapsedTimer frameTimer;
    int failureCount = 0;
    int frameCount = 0;
    bool reconnecting = false;

    const int reconnectFailureThreshold = 30;
    const unsigned long reconnectBackoffMs = 1000;

    while (m_grabRunning.loadAcquire() 
            && !currentThread->isInterruptionRequested()) {
        frameTimer.restart();

        cv::Mat capturedFrame;
        bool grabSuccess = false;
        
        {
            QMutexLocker locker(&m_cameraMutex);

            if(!m_grabRunning.loadAcquire() 
                    || currentThread->isInterruptionRequested()) {
                break;
            }

            if (m_capture.isOpened()) {
                try {
                    const bool grabbed = m_capture.grab();
                    grabSuccess = grabbed
                            && m_capture.retrieve(capturedFrame)
                            && !capturedFrame.empty();
                } catch (const cv::Exception &exception) {
                    qWarning() << "[CameraFrameProvider] OpenCV exception in worker:"
                                << exception.what();
                }
            }
        }

        if(!m_grabRunning.loadAcquire()
                ||currentThread->isInterruptionRequested()){
            break;
        }

        if (!grabSuccess) {
            ++failureCount;
            if (failureCount == 1 || failureCount % 120 == 0) {
                qWarning() << "[CameraFrameProvider] failed to grab frame, count:" << failureCount;
            }

            if (failureCount >= reconnectFailureThreshold) {
                if (!reconnecting) {
                    reconnecting = true;
                    qWarning() << "[CameraFrameProvider] camera stream lost; starting reconnect";
                    emit cameraError(tr("相机连接中断，正在自动重连"));
                    clearFrame();
                }

                if (tryReconnectCamera()) {
                    qDebug() << "[CameraFrameProvider] camera reconnected successfully";
                    failureCount = 0;
                    reconnecting = false;
                    continue;
                }

                for (unsigned long waited = 0;
                     waited < reconnectBackoffMs
                     && m_grabRunning.loadAcquire()
                     && !currentThread->isInterruptionRequested();
                     waited += 50) {
                    QThread::msleep(50);
                }
                continue;
            }

            QThread::msleep(5);
            continue;
        }

        failureCount = 0;
        reconnecting = false;
        FrameInputMetadata capturedMetadata =
                FrameInputMetadata::fromMat(capturedFrame, QStringLiteral("camera"));
        if (m_useNv12Path) {
            capturedMetadata.colorMode = QStringLiteral("color");
            capturedMetadata.pixelFormat = QStringLiteral("NV12");
            capturedMetadata.originalChannels = capturedFrame.channels();
            capturedMetadata.originalDepth = 8;
        } else if (capturedFrame.type() == CV_8UC2) {
            capturedMetadata.colorMode = QStringLiteral("color");
            capturedMetadata.pixelFormat = QStringLiteral("UYVY8");
        }

        const cv::Mat bgrFrame = decodeCapturedFrame(capturedFrame);
        if (bgrFrame.empty()) {
            QThread::msleep(5);
            continue;
        }

        setCurrentFrame(bgrFrame, capturedMetadata);
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
    return MatImageConverter::matToDisplayImage(frame, QStringLiteral("CameraFrameProvider"));
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
