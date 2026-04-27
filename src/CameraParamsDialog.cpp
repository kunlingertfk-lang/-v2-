#include "CameraParamsDialog.h"
#include "ui_CameraParamsDialog.h"

#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QMetaType>
#include <QPushButton>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <cstring>
#include <exception>

#include "OutputDialog.h"
#include "PlanDialogUtils.h"
#include "ReferenceImageDialog.h"
#include "ToolsDialog.h"


CameraParamsDialog::CameraParamsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CameraParamsDialog)
    , m_videoCapture(nullptr)
    , m_devicePath("")
    , m_workerThread(nullptr)
    , m_workerRunning(0)
    , m_workerPaused(0)
    , m_displaySize(640, 480)
    , m_workerFps(60)
{
    qRegisterMetaType<QPixmap>("QPixmap");
    qRegisterMetaType<QList<QPair<int,int>>>("QList<QPair<int,int>>");

    ui->setupUi(this);
    setupUiState();
    connectNavigation();

    connect(this, &CameraParamsDialog::cameraInitialized,this, &CameraParamsDialog::onCameraInitialized);
    connect(this, &CameraParamsDialog::frameReadyForDisplay,this, &CameraParamsDialog::onFrameReadyForDisplay,Qt::QueuedConnection);
    initCamera("/dev/video0");
    showMaximized();
}

CameraParamsDialog::~CameraParamsDialog()
{
    stopWorkerThread();
    releaseCamera();
    delete ui;
}

// ==================== 相机接口实现：移植自旧项目 VisionAlgorithm ====================
bool CameraParamsDialog::initCamera(const QString& devicePath)
{
    qDebug() << "初始化相机:" << devicePath;

    m_devicePath = devicePath;
    qDebug() << "保存设备路径:" << m_devicePath;

    if (m_videoCapture) {
        releaseCamera();
    }

    m_videoCapture = new cv::VideoCapture();

    qDebug() << "方法0: 尝试虚拟机常见摄像头设备...";
    QStringList vmCameraDevices = {"/dev/video00", "/dev/video10", "/dev/video20", "/dev/video30"};
    for (const QString& device : vmCameraDevices) {
        qDebug() << "尝试打开:" << device;
        bool opened = m_videoCapture->open(device.toStdString(), cv::CAP_V4L2);
        if (opened) {
            m_videoCapture->set(cv::CAP_PROP_FRAME_WIDTH, 640);
            m_videoCapture->set(cv::CAP_PROP_FRAME_HEIGHT, 480);
            m_videoCapture->set(cv::CAP_PROP_FPS, 30);

            if (testCameraRead()) {
                qDebug() << "虚拟机摄像头成功打开:" << device;
                qDebug() << QString("分辨率: %1x%2")
                            .arg((int)m_videoCapture->get(cv::CAP_PROP_FRAME_WIDTH))
                            .arg((int)m_videoCapture->get(cv::CAP_PROP_FRAME_HEIGHT));
                emit cameraInitialized(true);
                return true;
            } else {
                qDebug() << device << "打开成功但无法读取帧";
                m_videoCapture->release();
            }
        } else {
            qDebug() << device << "打开失败";
        }
    }
    qDebug() << "虚拟机常见设备都无法打开，尝试其他方法...";

    if (initCameraWithGStreamer(devicePath)) {
        emit cameraInitialized(true);
        return true;
    }

    if (initCameraWithV4L2(devicePath)) {
        emit cameraInitialized(true);
        return true;
    }

    qDebug() << "所有方法都失败";
    emit cameraInitialized(false);
    emit errorOccurred("无法初始化摄像头");
    return false;
}

bool CameraParamsDialog::initCameraWithGStreamer(const QString& devicePath)
{
    auto tryPipeline = [&](const QString& pipeline, bool useNv12, const QString& tag) -> bool {
        qDebug() << "尝试GStreamer管道:" << pipeline;

        bool opened = m_videoCapture->open(pipeline.toStdString(), cv::CAP_GSTREAMER);
        if (opened && testCameraRead()) {
            m_useNv12Path = useNv12;
            double actualFps = m_videoCapture->get(cv::CAP_PROP_FPS);
            Q_UNUSED(actualFps);
            qDebug() << "当前相机路径 =" << tag << ", m_useNv12Path =" << m_useNv12Path;
            return true;
        }

        if (opened) {
            m_videoCapture->release();
            qDebug() << "GStreamer管道打开成功但无法读取帧:" << tag;
        } else {
            qDebug() << "GStreamer管道打开失败:" << tag;
        }
        return false;
    };

    // 先试 1920x1080：MJPG -> mppjpegdec -> NV12
    QString pipeline1920 = QString(
        "v4l2src device=%1 io-mode=mmap ! "
        "image/jpeg, width=1920, height=1080, framerate=60/1 ! "
        "mppjpegdec ! "
        "video/x-raw, format=NV12 ! "
        "appsink max-buffers=1 drop=true sync=false"
    ).arg(devicePath);

    if (tryPipeline(pipeline1920, true, "1920x1080 MJPG->NV12")) {
        return true;
    }

    // 1920 不行，再退 640x480：MJPG -> jpegdec -> BGR
    QString pipeline640 = QString(
        "v4l2src device=%1 io-mode=mmap ! "
        "image/jpeg, width=640, height=480, framerate=30/1 ! "
        "jpegdec ! "
        "videoconvert ! "
        "video/x-raw, format=BGR ! "
        "appsink max-buffers=1 drop=true sync=false"
    ).arg(devicePath);

    if (tryPipeline(pipeline640, false, "640x480 MJPG->BGR")) {
        return true;
    }

    return false;
}

bool CameraParamsDialog::initCameraWithV4L2(const QString& devicePath)
{
    bool opened = m_videoCapture->open(devicePath.toStdString(), cv::CAP_V4L2);
    if (opened) {
        m_videoCapture->set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('U','Y','V','Y'));
        m_videoCapture->set(cv::CAP_PROP_FRAME_WIDTH, 640);
        m_videoCapture->set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        m_videoCapture->set(cv::CAP_PROP_FPS, 30);

        if (testCameraRead()) {
            return true;
        }
    }
    return false;
}

bool CameraParamsDialog::testCameraRead()
{
    if (!m_videoCapture || !m_videoCapture->isOpened()) {
        return false;
    }

    cv::Mat testFrame;
    try {
        bool success = m_videoCapture->read(testFrame);
        if (success && !testFrame.empty()) {
            qDebug() << QString("帧读取成功: %1x%2, 通道数: %3")
                        .arg(testFrame.cols).arg(testFrame.rows).arg(testFrame.channels());
            return true;
        }
    } catch (const cv::Exception& e) {
        qDebug() << "OpenCV异常:" << e.what();
    }
    return false;
}

void CameraParamsDialog::releaseCamera()
{
    if (m_videoCapture) {
        if (m_videoCapture->isOpened()) {
            m_videoCapture->release();
        }
        delete m_videoCapture;
        m_videoCapture = nullptr;
    }
}

bool CameraParamsDialog::captureFrame(cv::Mat& frame)
{
    QMutexLocker locker(&m_cameraMutex);

    if (!m_videoCapture || !m_videoCapture->isOpened()) {
        return false;
    }

    bool success = m_videoCapture->read(frame);

    if (!success || frame.empty()) {
        return false;
    }

    // 🔥 关键诊断：检查实际像素值（每100帧检查一次）
    static int diagCount = 0;
    diagCount++;
    if (diagCount % 100 == 1) {
        // 检查中心10个像素
        int cx = frame.cols / 2;
        int cy = frame.rows / 2;

        bool isFakeColor = true;
        for (int i = 0; i < 10; i++) {
            cv::Vec3b pixel = frame.at<cv::Vec3b>(cy, cx + i);
            if (pixel[0] != pixel[1] || pixel[1] != pixel[2]) {
                isFakeColor = false;
                break;
            }
        }

        cv::Vec3b centerPixel = frame.at<cv::Vec3b>(cy, cx);
        qDebug() << QString("🔬 像素检查[%1]: B=%2 G=%3 R=%4 | %5").arg(diagCount).arg(centerPixel[0]).arg(centerPixel[1]).arg(centerPixel[2]).arg(isFakeColor ? "❌伪灰度" : "✅真彩色");
    }
    emit frameUpdated(frame);
    return true;
}

bool CameraParamsDialog::isOpened() const
{
    return m_videoCapture && m_videoCapture->isOpened();
}

QPixmap CameraParamsDialog::matToQPixmap(const cv::Mat &mat)
{
    // 🔥 添加空值检查
    if (mat.empty()) {
        qDebug() << "❌ matToQPixmap: 输入Mat为空";
        return QPixmap();
    }

    try {
        switch (mat.type()) {
            case CV_8UC4: {
                QImage qimg(mat.data, mat.cols, mat.rows,static_cast<int>(mat.step), QImage::Format_ARGB32);
                return QPixmap::fromImage(qimg.copy());  // 🔥 必须copy
            }
            case CV_8UC3: {
                cv::Mat rgb;
                cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
                QImage qimg(rgb.data, rgb.cols, rgb.rows,static_cast<int>(rgb.step), QImage::Format_RGB888);
                // 🔥 关键：必须copy，因为rgb是局部变量
                QPixmap result = QPixmap::fromImage(qimg.copy());
                return result;
            }
            case CV_8UC1: {
                QImage qimg(mat.data, mat.cols, mat.rows,static_cast<int>(mat.step), QImage::Format_Grayscale8);
                return QPixmap::fromImage(qimg.copy());  // 🔥 必须copy
            }
            default:
                qDebug() << "❌ 不支持的Mat类型:" << mat.type();
                return QPixmap();
        }
    } catch (const std::exception& e) {
        qDebug() << "❌ matToQPixmap异常:" << e.what();
        return QPixmap();
    }
}

void CameraParamsDialog::startWorkerThread()
{
    if (m_workerRunning.loadAcquire()) {
        qDebug() << "⚠️ 工作线程已在运行";
        return;
    }

    m_workerRunning.storeRelease(1);
    m_workerPaused.storeRelease(0);

    m_workerThread = QThread::create([this]() {
        workerLoop();
    });

    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);

    m_workerThread->start();

    qDebug() << "▶️ 工作线程启动，目标FPS:" << m_workerFps;
}

void CameraParamsDialog::stopWorkerThread()
{
    if (!m_workerRunning.loadAcquire()) {
        return;
    }

    qDebug() << "⏹️ 工作线程停止中...";

    m_workerRunning.storeRelease(0);

    m_workerMutex.lock();
    m_workerCondition.wakeAll();
    m_workerMutex.unlock();

    if (m_workerThread) {
        if (!m_workerThread->wait(3000)) {
            qWarning() << "⚠️ 线程未正常退出，强制终止";
            m_workerThread->terminate();
            m_workerThread->wait();
        }
        m_workerThread = nullptr;
    }

    qDebug() << "✅ 工作线程已停止";
}

void CameraParamsDialog::pauseWorkerThread()
{
    m_workerPaused.storeRelease(1);
    qDebug() << "⏸️ 工作线程暂停";
}

void CameraParamsDialog::resumeWorkerThread()
{
    m_workerPaused.storeRelease(0);
    m_workerMutex.lock();
    m_workerCondition.wakeAll();
    m_workerMutex.unlock();
    qDebug() << "▶️ 工作线程恢复";
}

void CameraParamsDialog::setDisplaySize(const QSize& size)
{
    QMutexLocker locker(&m_workerMutex);
    m_displaySize = size;
}

void CameraParamsDialog::setWorkerFps(int fps)
{
    m_workerFps = qBound(1, fps, 100);
}

void CameraParamsDialog::workerLoop()
{
    qDebug() << "🔄 工作线程启动...";

    QElapsedTimer frameTimer;
    cv::Mat currentFrame;

    while (m_workerRunning.loadAcquire()) {
        frameTimer.start();

        // 1. 采集图像
        qint64 birthTime = 0;
        bool grabSuccess = false;
        {
            QMutexLocker locker(&m_cameraMutex);
            if (m_videoCapture && m_videoCapture->isOpened()) {
                qint64 grabStart = QDateTime::currentMSecsSinceEpoch();
                bool grabbed = m_videoCapture->grab();
                qint64 grabDone = QDateTime::currentMSecsSinceEpoch();

                if (grabbed && m_videoCapture->retrieve(currentFrame) && !currentFrame.empty()) {
                    if (m_useNv12Path) {
                        // 1920 路径：NV12转BGR，再裁 960x540
                        cv::Mat nv12(1620, 1920, CV_8UC1);
                        memcpy(nv12.data, currentFrame.data, 1920 * 1080);
                        memcpy(nv12.data + 1920 * 1080, currentFrame.data + 1920 * 1088, 1920 * 532);
                        cv::cvtColor(nv12, currentFrame, cv::COLOR_YUV2BGR_NV12);
                        cv::rectangle(currentFrame, cv::Rect(0, 1065, 1920, 15), cv::Scalar(0, 0, 0), -1);
                        currentFrame = currentFrame(cv::Rect(480, 270, 960, 540)).clone();
                    } else {
                        /*
                            // 640 路径：已经是 BGR，直接用原图
                        */
                    }

                    grabSuccess = true;
                    birthTime = QDateTime::currentMSecsSinceEpoch();
                    qint64 decodeCost = birthTime - grabDone;
                    qint64 waitCost = grabDone - grabStart;
                    Q_UNUSED(decodeCost);
                    Q_UNUSED(waitCost);
                }
            }
        }

        if (m_lastBirthTime > 0 && (birthTime - m_lastBirthTime) > 0) {
        }
        m_lastBirthTime = birthTime;

        if (!grabSuccess) {
            QThread::msleep(5);
            continue;
        }

        // 3. 预览模式
        QPixmap pixmap = matToQPixmap(currentFrame);
        emit frameReadyForDisplay(pixmap, false, "", 0, 0, 0.0,QList<QPair<int,int>>(), birthTime);

        // 4. 帧率控制
        qint64 elapsed = frameTimer.elapsed();
        int fps = (m_workerFps > 0) ? m_workerFps : 60;
        int targetInterval = 1000 / fps;
        if (elapsed < targetInterval) {
            QThread::msleep(targetInterval - elapsed);
        }
    }
}

void CameraParamsDialog::onCameraInitialized(bool success)
{
    if (success) {
        qDebug() << "✅ 相机初始化成功";
        setupCameraUI();

        // 🔥 启动定时器前先测试一帧
        QTimer::singleShot(100, this, [this]() {
            qDebug() << "\n========== 测试相机显示 ==========";
            cv::Mat testFrame;
            if (captureFrame(testFrame) && !testFrame.empty()) {
                qDebug() << QString("✅ 测试帧: %1x%2, 通道:%3").arg(testFrame.cols).arg(testFrame.rows).arg(testFrame.channels());
                QPixmap testPixmap = matToQPixmap(testFrame);
                if (!testPixmap.isNull()) {
                    qDebug() << QString("✅ Pixmap转换成功: %1x%2").arg(testPixmap.width()).arg(testPixmap.height());
                    // 手动显示测试帧
                    QLabel *cameraLabel = ui->camera_1->findChild<QLabel*>("cameraDisplay");
                    if (cameraLabel) {
                        QPixmap scaled = testPixmap.scaled(640, 360,Qt::KeepAspectRatio,Qt::SmoothTransformation);
                        cameraLabel->setPixmap(scaled);
                        qDebug() << "✅ 测试帧已显示到界面";
                    } else {
                        qDebug() << "❌ 未找到cameraDisplay控件";
                    }
                } else {
                    qDebug() << "❌ Pixmap转换失败";
                }
            } else {
                qDebug() << "❌ 无法捕获测试帧";
            }
            qDebug() << "==================================\n";
        });

        // 🔥 设置显示尺寸并启动工作线程
        QTimer::singleShot(200, this, [this]() {
            QLabel *cameraLabel = ui->camera_1->findChild<QLabel*>("cameraDisplay");
            if (cameraLabel) {
                setDisplaySize(cameraLabel->size());
            }
            setWorkerFps(30);
            startWorkerThread();
            qDebug() << "✅ 工作线程已启动";
        });
    } else {
        qDebug() << "❌ 相机初始化失败";
        setupCameraErrorUI();
    }
}

void CameraParamsDialog::onFrameReadyForDisplay(const QPixmap& pixmap, bool hasDetection,
                                                const QString& resultText, int total, int ok, double rate,
                                                const QList<QPair<int, int>>& wirePositions,qint64 frameTime)
{
    Q_UNUSED(hasDetection);
    Q_UNUSED(resultText);
    Q_UNUSED(total);
    Q_UNUSED(ok);
    Q_UNUSED(rate);
    Q_UNUSED(wirePositions);
    Q_UNUSED(frameTime);

    if (!this->isVisible()) {
        return;
    }

    static int frameCount = 0;
    frameCount++;
    if (frameCount % 30 == 1) {
        qDebug() << QString("📷 [CameraParamsDialog] 收到帧 #%1").arg(frameCount);
    }

    QLabel *cameraLabel = ui->camera_1->findChild<QLabel*>("cameraDisplay");
    if (!cameraLabel || pixmap.isNull()) {
        return;
    }

    QSize labelSize = cameraLabel->size();
    if (labelSize.width() < 100 || labelSize.height() < 100) {
        labelSize = QSize(640, 480);
    }

    QPixmap scaledPix = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    cameraLabel->setPixmap(scaledPix);
}

void CameraParamsDialog::setupCameraUI()
{
    QLayout* oldLayout = ui->camera_1->layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }
    QLabel *cameraLabel = new QLabel(ui->camera_1);
    cameraLabel->setObjectName("cameraDisplay");
    cameraLabel->setStyleSheet("border: 1px solid gray;");
    cameraLabel->setAlignment(Qt::AlignCenter);
    // ✅ 自适应父控件尺寸
    cameraLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QVBoxLayout *layout = new QVBoxLayout(ui->camera_1);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(cameraLabel);
    ui->camera_1->setLayout(layout);
    qDebug() << QString("✅ 相机显示区域: %1x%2") .arg(ui->camera_1->width()) .arg(ui->camera_1->height());
}

void CameraParamsDialog::setupCameraErrorUI()
{
    QLayout* oldLayout = ui->camera_1->layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }
    QLabel *errorLabel = new QLabel("无法打开摄像头\n可能被其他程序占用", ui->camera_1);
    errorLabel->setAlignment(Qt::AlignCenter);
    errorLabel->setStyleSheet("color: red; font-size: 12px;");
    QPushButton *retryButton = new QPushButton("重试", ui->camera_1);
    connect(retryButton, &QPushButton::clicked, this, [this]() {
        initCamera("/dev/video0");
    });
    QVBoxLayout *layout = new QVBoxLayout(ui->camera_1);
    layout->addWidget(errorLabel);
    layout->addWidget(retryButton);
    ui->camera_1->setLayout(layout);
}

void CameraParamsDialog::setupUiState()
{
    PlanDialogUtils::configureDialogWindow(this, tr("方案编辑 - 相机参数"));
    PlanDialogUtils::connectWindowButtons(this, ui->headerCloseButton, true);

    ui->cameraStepButton->setChecked(true);
    ui->referenceStepButton->setChecked(false);
    ui->toolsStepButton->setChecked(false);
    ui->outputStepButton->setChecked(false);

    const auto applyParamMode = [this](bool allMode) {
        ui->basicModeButton->setChecked(!allMode);
        ui->allModeButton->setChecked(allMode);
        ui->cameraParamsStack->setCurrentWidget(allMode ? ui->allParamsPage : ui->basicParamsPage);
    };

    applyParamMode(false);
    connect(ui->basicModeButton, &QPushButton::clicked, this, [applyParamMode]() {
        applyParamMode(false);
    });
    connect(ui->allModeButton, &QPushButton::clicked, this, [applyParamMode]() {
        applyParamMode(true);
    });
}

void CameraParamsDialog::connectNavigation()
{
    connect(ui->referenceStepButton, &QToolButton::clicked, this, &CameraParamsDialog::openReferenceImageDialog);
    connect(ui->toolsStepButton, &QToolButton::clicked, this, &CameraParamsDialog::openToolsDialog);
    connect(ui->outputStepButton, &QToolButton::clicked, this, &CameraParamsDialog::openOutputDialog);
    connect(ui->nextButton, &QPushButton::clicked, this, &CameraParamsDialog::openReferenceImageDialog);
}

void CameraParamsDialog::openReferenceImageDialog()
{
    PlanDialogUtils::replaceDialog(this, new ReferenceImageDialog);
}

void CameraParamsDialog::openToolsDialog()
{
    PlanDialogUtils::replaceDialog(this, new ToolsDialog);
}

void CameraParamsDialog::openOutputDialog()
{
    PlanDialogUtils::replaceDialog(this, new OutputDialog);
}
