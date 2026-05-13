#ifndef REFERENCEIMAGEDIALOG_H
#define REFERENCEIMAGEDIALOG_H

#include <QDialog>

class FrameViewHelper;
class QPushButton;

QT_BEGIN_NAMESPACE
namespace Ui {
class ReferenceImageDialog;
}
QT_END_NAMESPACE

class ReferenceImageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReferenceImageDialog(QWidget *parent = nullptr);
    ~ReferenceImageDialog() override;

private slots:
    void openCameraParamsDialog();
    void openToolsDialog();
    void openOutputDialog();
    void showCurrentImageMode();
    void captureReferenceImage();
    void showReferenceImageMode();
    void importReferenceImageFromPc();

private:
    void setupUiState();
    void connectNavigation();
    void setupReferenceImageControls();
    void ensureCameraRunning();
    void updateReferenceImageControls();
    void refreshCurrentImage();
    void refreshReferenceImage();

    Ui::ReferenceImageDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_captureImageButton = nullptr;
    QPushButton *m_exitCaptureButton = nullptr;
    bool m_liveCaptureMode = false;
};

#endif // REFERENCEIMAGEDIALOG_H
