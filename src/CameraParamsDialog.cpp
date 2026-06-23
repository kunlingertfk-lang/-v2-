#include "CameraParamsDialog.h"
#include "ui_CameraParamsDialog.h"

#include <QDebug>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>

#include "OutputDialog.h"
#include "PlanDialogUtils.h"
#include "ReferenceImageDialog.h"
#include "SchemeStore.h"
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
    QString schemeError;
    if (!SchemeStore::instance().ensureLoaded(&schemeError))
        qWarning() << "[CameraParamsDialog] 方案加载失败:" << schemeError;
    refreshSchemeHeader();

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
    refreshSchemeHeader();

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
    connect(ui->setupExternalEditButton, &QToolButton::clicked, this, &CameraParamsDialog::editCurrentSchemeName);
    connect(ui->setupSaveButton, &QToolButton::clicked, this, &CameraParamsDialog::saveCurrentScheme);
    connect(ui->setupSaveAsButton, &QToolButton::clicked, this, &CameraParamsDialog::saveCurrentSchemeAs);
}

void CameraParamsDialog::refreshSchemeHeader()
{
    ui->setupPageCodeLabel->setText(SchemeStore::instance().currentSchemeName());
}

void CameraParamsDialog::editCurrentSchemeName()
{
    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        QMessageBox::warning(this, tr("方案名称"), tr("方案加载失败：%1").arg(error));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("编辑方案名"),
                                               tr("方案名"),
                                               QLineEdit::Normal,
                                               store.currentSchemeName(),
                                               &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    store.setSchemeName(name);
    saveCurrentScheme();
}

void CameraParamsDialog::saveCurrentScheme()
{
    QString error;
    if (!SchemeStore::instance().saveCurrentScheme(&error)) {
        qWarning() << "[CameraParamsDialog] 方案保存失败:" << error;
        QMessageBox::warning(this, tr("保存失败"), tr("方案保存失败：%1").arg(error));
        return;
    }
    refreshSchemeHeader();
}

void CameraParamsDialog::saveCurrentSchemeAs()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("另存为"),
                                               tr("新方案名"),
                                               QLineEdit::Normal,
                                               SchemeStore::instance().currentSchemeName() + tr("_副本"),
                                               &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    QString error;
    if (!SchemeStore::instance().saveCurrentSchemeAs(name, &error)) {
        qWarning() << "[CameraParamsDialog] 方案另存为失败:" << error;
        QMessageBox::warning(this, tr("另存为失败"), tr("方案另存为失败：%1").arg(error));
        return;
    }
    refreshSchemeHeader();
}

void CameraParamsDialog::openReferenceImageDialog()
{
    saveCurrentScheme();
    PlanDialogUtils::replaceDialog(this, new ReferenceImageDialog);
}

void CameraParamsDialog::openToolsDialog()
{
    saveCurrentScheme();
    PlanDialogUtils::replaceDialog(this, new ToolsDialog);
}

void CameraParamsDialog::openOutputDialog()
{
    saveCurrentScheme();
    PlanDialogUtils::replaceDialog(this, new OutputDialog);
}
