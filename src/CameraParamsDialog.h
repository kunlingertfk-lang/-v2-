#ifndef CAMERAPARAMSDIALOG_H
#define CAMERAPARAMSDIALOG_H

#include <QDialog>
#include <QAtomicInt>
#include <QList>
#include <QMutex>
#include <QPair>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QThread>
#include <QVideoWidget>
#include <QWaitCondition>

#include <opencv2/opencv.hpp>

QT_BEGIN_NAMESPACE
namespace Ui {
class CameraParamsDialog;
}
QT_END_NAMESPACE

class CameraParamsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraParamsDialog(QWidget *parent = nullptr);
    ~CameraParamsDialog() override;

private slots:
    void openReferenceImageDialog();
    void openToolsDialog();
    void openOutputDialog();
    void onCameraInitialized(bool success);
    void onFrameReadyForDisplay(const QPixmap& pixmap, bool hasDetection,
                                const QString& resultText, int total, int ok, double rate,
                                const QList<QPair<int, int>>& wirePositions,
                                qint64 frameTime);

signals:
    void cameraInitialized(bool success);
    void frameUpdated(const cv::Mat& frame);
    void errorOccurred(const QString& error);
    void frameReadyForDisplay(const QPixmap& pixmap, bool hasDetection,
                              const QString& resultText, int total, int ok, double rate,
                              const QList<QPair<int, int>>& wirePositions,
                              qint64 frameTime);

private:
    void setupUiState();
    void connectNavigation();
    void setupCameraUI();
    void setupCameraErrorUI();

    bool initCamera(const QString& devicePath = "/dev/video22");
    bool initCameraWithGStreamer(const QString& devicePath);
    bool initCameraWithV4L2(const QString& devicePath);
    bool testCameraRead();
    void releaseCamera();
    bool captureFrame(cv::Mat& frame);
    bool isOpened() const;
    QPixmap matToQPixmap(const cv::Mat& mat);

    void startWorkerThread();
    void stopWorkerThread();
    void pauseWorkerThread();
    void resumeWorkerThread();
    bool isWorkerRunning() const { return m_workerRunning.loadAcquire(); }
    bool isWorkerPaused() const { return m_workerPaused.loadAcquire(); }
    void setDisplaySize(const QSize& size);
    void setWorkerFps(int fps);
    void workerLoop();

    Ui::CameraParamsDialog *ui;
    cv::VideoCapture* m_videoCapture;
    QString m_devicePath;
    bool m_useNv12Path = true;

    QThread* m_workerThread;
    QAtomicInt m_workerRunning;
    QAtomicInt m_workerPaused;
    QMutex m_workerMutex;
    QWaitCondition m_workerCondition;
    QSize m_displaySize;
    int m_workerFps;
    qint64 m_lastBirthTime = 0;

    mutable QMutex m_cameraMutex;
};

#endif // CAMERAPARAMSDIALOG_H
