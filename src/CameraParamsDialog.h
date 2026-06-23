#ifndef CAMERAPARAMSDIALOG_H
#define CAMERAPARAMSDIALOG_H

#include <QDialog>
#include <QImage>

QT_BEGIN_NAMESPACE
namespace Ui {
class CameraParamsDialog;
}
QT_END_NAMESPACE

class FrameViewHelper;

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
    void showLiveImage(const QImage &image);
    void editCurrentSchemeName();
    void saveCurrentScheme();
    void saveCurrentSchemeAs();

private:
    void setupUiState();
    void connectNavigation();
    void refreshSchemeHeader();
    void setupCameraUI();
    void setupCameraErrorUI();
    void ensureCameraRunning();
    void refreshLiveImage();

    Ui::CameraParamsDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
};

#endif // CAMERAPARAMSDIALOG_H
