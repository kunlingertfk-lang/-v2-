#include "CameraParamsDialog.h"
#include "ui_CameraParamsDialog.h"

#include <QDebug>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

#include "OutputDialog.h"
#include "PlanDialogUtils.h"
#include "ReferenceImageDialog.h"
#include "ToolsDialog.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"

CameraParamsDialog::CameraParamsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CameraParamsDialog)
{
    ui->setupUi(this);
    setupUiState();
    connectNavigation();
    setupCameraUI();

    connect(&CameraFrameProvider::instance(),
            &CameraFrameProvider::frameUpdated,
            this,
            &CameraParamsDialog::showLiveImage);
    connect(&CameraFrameProvider::instance(),
            &CameraFrameProvider::cameraError,
            this,
            [this](const QString &message) {
                qWarning() << "[CameraParamsDialog]" << message;
                setupCameraErrorUI();
            });

    refreshLiveImage();
    QTimer::singleShot(0, this, &CameraParamsDialog::ensureCameraRunning);
}

CameraParamsDialog::~CameraParamsDialog()
{
    delete ui;
}

void CameraParamsDialog::setupCameraUI()
{
    if (!m_previewHelper) {
        m_previewHelper = new FrameViewHelper(ui->camera_1, this);
    }

    ui->viewerTitleLabel->setText(tr("相机图像"));
    qDebug() << QString("[CameraParamsDialog] preview area: %1x%2")
                    .arg(ui->camera_1->width())
                    .arg(ui->camera_1->height());
}

void CameraParamsDialog::setupCameraErrorUI()
{
    if (!m_previewHelper) {
        m_previewHelper = new FrameViewHelper(ui->camera_1, this);
    }

    m_previewHelper->clear();
    ui->viewerTitleLabel->setText(tr("当前无图像"));
}

void CameraParamsDialog::ensureCameraRunning()
{
    CameraFrameProvider &provider = CameraFrameProvider::instance();

    if (!provider.isOpened() && !provider.openCamera(QStringLiteral("/dev/video0"))) {
        setupCameraErrorUI();
        return;
    }

    if (!provider.isGrabbing() && !provider.startGrab()) {
        setupCameraErrorUI();
        return;
    }

    refreshLiveImage();
}

void CameraParamsDialog::refreshLiveImage()
{
    showLiveImage(CameraFrameProvider::instance().currentImage());
}

void CameraParamsDialog::showLiveImage(const QImage &image)
{
    if (!m_previewHelper || !isVisible()) {
        return;
    }

    if (image.isNull()) {
        setupCameraErrorUI();
        return;
    }

    ui->viewerTitleLabel->setText(tr("相机图像"));
    m_previewHelper->setImage(image);
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
