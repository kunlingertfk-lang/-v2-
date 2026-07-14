#ifndef REFERENCEIMAGEDIALOG_H
#define REFERENCEIMAGEDIALOG_H

#include <QDialog>

#include "toolcore/PositionCorrection.h"

class FrameViewHelper;
class QPushButton;
class QFrame;
class QLabel;

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
    void editCurrentSchemeName();
    void saveCurrentScheme();
    void saveCurrentSchemeAs();
    void updatePositionCorrectionUi(bool enabled);

private:
    void setupUiState();
    void connectNavigation();
    void setupReferenceImageControls();
    void refreshSchemeHeader();
    void ensureCameraRunning();
    void updateReferenceImageControls();
    void refreshCurrentImage();
    void refreshReferenceImage();
    void setupPositionCorrectionControls();
    void loadPositionCorrectionConfig();

    Ui::ReferenceImageDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_captureImageButton = nullptr;
    QPushButton *m_exitCaptureButton = nullptr;
    QFrame *m_positionSettingsFrame = nullptr;
    QLabel *m_positionStatusLabel = nullptr;
    QPushButton *m_positionRectButton = nullptr;
    QPushButton *m_positionPolygonButton = nullptr;
    bool m_liveCaptureMode = false;
    ReferencePositionCorrectionConfig m_referencePositionCorrection;
};

#endif // REFERENCEIMAGEDIALOG_H
