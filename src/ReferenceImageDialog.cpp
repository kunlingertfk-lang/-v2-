#include "ReferenceImageDialog.h"

#include <QDebug>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QLabel>
#include <QPushButton>
#include <QSize>
#include <QToolButton>

#include <opencv2/imgproc.hpp>

#include "CameraParamsDialog.h"
#include "OutputDialog.h"
#include "PlanDialogUtils.h"
#include "SchemeStore.h"
#include "ToolsDialog.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameInputMetadata.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "ui_ReferenceImageDialog.h"

namespace {

FrameInputMetadata importedImageMetadata(const QImage &image)
{
    FrameInputMetadata metadata;
    metadata.source = QStringLiteral("file");

    const bool isGrayscale8 = image.format() == QImage::Format_Grayscale8;
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    const bool isGrayscale16 = image.format() == QImage::Format_Grayscale16;
#else
    const bool isGrayscale16 = false;
#endif

    if (isGrayscale8 || isGrayscale16) {
        metadata.colorMode = QStringLiteral("mono");
        metadata.pixelFormat = isGrayscale16
                ? QStringLiteral("Mono16")
                : QStringLiteral("Mono8");
        metadata.originalChannels = 1;
        metadata.originalDepth = isGrayscale16 ? 16 : 8;
        return metadata;
    }

    metadata.colorMode = QStringLiteral("color");
    metadata.pixelFormat = QStringLiteral("BGR8");
    metadata.originalChannels = 3;
    metadata.originalDepth = 8;
    return metadata;
}

} // namespace

ReferenceImageDialog::ReferenceImageDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ReferenceImageDialog)
{
    ui->setupUi(this);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    setupReferenceImageControls();
    setupPositionCorrectionControls();
    connectNavigation();
    connect(&CameraFrameProvider::instance(),
            &CameraFrameProvider::frameUpdated,
            this,
            [this](const QImage &image) {
                if (m_liveCaptureMode && m_previewHelper) {
                    ui->viewerTitleLabel->setText(image.isNull() ? tr("当前无图像") : tr("当前图像"));
                    m_previewHelper->setImage(image);
                }
            });
    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &image) {
                if (!m_liveCaptureMode && m_previewHelper) {
                    m_previewHelper->setImage(image);
                    ui->viewerTitleLabel->setText(image.isNull() ? tr("请先设置基准图") : tr("基准图"));
                }
            });

    QString error;
    if (!SchemeStore::instance().ensureLoaded(&error)) {
        qWarning() << "[ReferenceImageDialog] 方案加载失败:" << error;
    } else if (!SchemeStore::instance().loadCurrentReferenceIntoProvider(&error)
               && !SchemeStore::instance().currentScheme().referenceImagePath.isEmpty()) {
        qWarning() << "[ReferenceImageDialog]" << error;
    }
    refreshSchemeHeader();
    loadPositionCorrectionConfig();
    showReferenceImageMode();
}

ReferenceImageDialog::~ReferenceImageDialog()
{
    delete ui;
}

void ReferenceImageDialog::setupUiState()
{
    PlanDialogUtils::configureDialogWindow(this, tr("方案编辑 - 基准图"));
    PlanDialogUtils::connectWindowButtons(this, ui->headerCloseButton, true);

    ui->cameraStepButton->setChecked(false);
    ui->referenceStepButton->setChecked(true);
    ui->toolsStepButton->setChecked(false);
    ui->outputStepButton->setChecked(false);
    refreshSchemeHeader();
}

void ReferenceImageDialog::connectNavigation()
{
    connect(ui->cameraStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openCameraParamsDialog);
    connect(ui->toolsStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openToolsDialog);
    connect(ui->outputStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openOutputDialog);
    connect(ui->previousButton, &QPushButton::clicked, this, &ReferenceImageDialog::openCameraParamsDialog);
    connect(ui->nextButton, &QPushButton::clicked, this, &ReferenceImageDialog::openToolsDialog);
    connect(ui->currentImageButton, &QPushButton::clicked, this, &ReferenceImageDialog::showCurrentImageMode);
    connect(m_captureImageButton, &QPushButton::clicked, this, &ReferenceImageDialog::captureReferenceImage);
    connect(m_exitCaptureButton, &QPushButton::clicked, this, &ReferenceImageDialog::showReferenceImageMode);
    connect(ui->setupExternalEditButton, &QToolButton::clicked, this, &ReferenceImageDialog::editCurrentSchemeName);
    connect(ui->setupSaveButton, &QToolButton::clicked, this, &ReferenceImageDialog::saveCurrentScheme);
    connect(ui->setupSaveAsButton, &QToolButton::clicked, this, &ReferenceImageDialog::saveCurrentSchemeAs);
    connect(ui->historyImageButton, &QPushButton::clicked, this, []() {
        qDebug() << "[ReferenceImageDialog] 历史图像暂未接入。";
    });
    connect(ui->pcImportButton, &QPushButton::clicked, this, &ReferenceImageDialog::importReferenceImageFromPc);
}

void ReferenceImageDialog::refreshSchemeHeader()
{
    ui->setupPageCodeLabel->setText(SchemeStore::instance().currentSchemeName());
}

void ReferenceImageDialog::editCurrentSchemeName()
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

void ReferenceImageDialog::saveCurrentScheme()
{
    SchemeStore::instance().setReferencePositionCorrection(m_referencePositionCorrection);
    QString error;
    if (!SchemeStore::instance().saveCurrentScheme(&error)) {
        qWarning() << "[ReferenceImageDialog] 方案保存失败:" << error;
        QMessageBox::warning(this, tr("保存失败"), tr("方案保存失败：%1").arg(error));
        return;
    }
    refreshSchemeHeader();
}

void ReferenceImageDialog::setupPositionCorrectionControls()
{
    m_positionSettingsFrame = new QFrame(ui->positionCorrectionCard);
    m_positionSettingsFrame->setObjectName(QStringLiteral("positionCorrectionSettingsFrame"));
    m_positionSettingsFrame->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *settingsLayout = new QVBoxLayout(m_positionSettingsFrame);
    settingsLayout->setContentsMargins(0, 8, 0, 0);
    settingsLayout->setSpacing(10);

    QHBoxLayout *header = new QHBoxLayout;
    QLabel *title = new QLabel(tr("模板区域设置"), m_positionSettingsFrame);
    title->setProperty("role", QStringLiteral("cardTitle"));
    QPushButton *testButton = new QPushButton(tr("测试运行"), m_positionSettingsFrame);
    testButton->setObjectName(QStringLiteral("referencePositionTestButton"));
    testButton->setToolTip(tr("测试基准图位置修正；当前后端尚未实现"));
    header->addWidget(title);
    header->addStretch(1);
    header->addWidget(testButton);
    settingsLayout->addLayout(header);

    QHBoxLayout *tools = new QHBoxLayout;
    QLabel *field = new QLabel(tr("模板区域"), m_positionSettingsFrame);
    field->setProperty("role", QStringLiteral("rowField"));
    m_positionRectButton = new QPushButton(tr("矩形"), m_positionSettingsFrame);
    m_positionRectButton->setObjectName(QStringLiteral("referencePositionRectButton"));
    m_positionRectButton->setCheckable(true);
    m_positionPolygonButton = new QPushButton(tr("多边形"), m_positionSettingsFrame);
    m_positionPolygonButton->setObjectName(QStringLiteral("referencePositionPolygonButton"));
    m_positionPolygonButton->setCheckable(true);
    QPushButton *finishButton = new QPushButton(tr("完成"), m_positionSettingsFrame);
    finishButton->setObjectName(QStringLiteral("referencePositionRoiFinishButton"));
    finishButton->setProperty("actionRole", QStringLiteral("primary"));
    tools->addWidget(field);
    tools->addStretch(1);
    tools->addWidget(m_positionRectButton);
    tools->addWidget(m_positionPolygonButton);
    tools->addWidget(finishButton);
    settingsLayout->addLayout(tools);

    m_positionStatusLabel = new QLabel(tr("配置已保存，尚未测试"), m_positionSettingsFrame);
    m_positionStatusLabel->setObjectName(QStringLiteral("referencePositionStatusLabel"));
    m_positionStatusLabel->setProperty("hint", true);
    settingsLayout->addWidget(m_positionStatusLabel);
    ui->positionCorrectionCard->layout()->addWidget(m_positionSettingsFrame);

    connect(ui->positionCorrectionCheckBox, &QCheckBox::toggled,
            this, &ReferenceImageDialog::updatePositionCorrectionUi);
    connect(m_positionRectButton, &QPushButton::clicked, this, [this]() {
        m_referencePositionCorrection.templateRegionType = QStringLiteral("rectangle");
        m_positionRectButton->setChecked(true);
        m_positionPolygonButton->setChecked(false);
        m_positionStatusLabel->setText(ReferenceImageProvider::instance().referenceImage().isNull()
                                       ? tr("请先设置基准图")
                                       : tr("矩形模板区域编辑将在算法 UI 联调阶段接入"));
    });
    connect(m_positionPolygonButton, &QPushButton::clicked, this, [this]() {
        m_referencePositionCorrection.templateRegionType = QStringLiteral("polygon");
        m_positionRectButton->setChecked(false);
        m_positionPolygonButton->setChecked(true);
        m_positionStatusLabel->setText(ReferenceImageProvider::instance().referenceImage().isNull()
                                       ? tr("请先设置基准图")
                                       : tr("多边形模板区域编辑将在算法 UI 联调阶段接入"));
    });
    connect(finishButton, &QPushButton::clicked, this, [this]() {
        m_positionStatusLabel->setText(tr("配置已保存，尚未测试"));
    });
    connect(testButton, &QPushButton::clicked, this, [this]() {
        if (ReferenceImageProvider::instance().referenceImage().isNull()) {
            m_positionStatusLabel->setText(tr("请先设置基准图"));
            return;
        }
        m_positionStatusLabel->setText(tr("位置修正后端尚未实现"));
    });
}

void ReferenceImageDialog::loadPositionCorrectionConfig()
{
    m_referencePositionCorrection =
            SchemeStore::instance().currentScheme().referencePositionCorrection;
    ui->positionCorrectionCheckBox->setChecked(m_referencePositionCorrection.enabled);
    updatePositionCorrectionUi(m_referencePositionCorrection.enabled);
}

void ReferenceImageDialog::updatePositionCorrectionUi(bool enabled)
{
    m_referencePositionCorrection.enabled = enabled;
    ui->correctionExampleFrame->setVisible(!enabled);
    if (m_positionSettingsFrame)
        m_positionSettingsFrame->setVisible(enabled);
    if (m_positionRectButton)
        m_positionRectButton->setChecked(
                    m_referencePositionCorrection.templateRegionType == QStringLiteral("rectangle"));
    if (m_positionPolygonButton)
        m_positionPolygonButton->setChecked(
                    m_referencePositionCorrection.templateRegionType == QStringLiteral("polygon"));
}

void ReferenceImageDialog::saveCurrentSchemeAs()
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
        qWarning() << "[ReferenceImageDialog] 方案另存为失败:" << error;
        QMessageBox::warning(this, tr("另存为失败"), tr("方案另存为失败：%1").arg(error));
        return;
    }
    refreshSchemeHeader();
}

void ReferenceImageDialog::openCameraParamsDialog()
{
    saveCurrentScheme();
    PlanDialogUtils::replaceDialog(this, new CameraParamsDialog);
}

void ReferenceImageDialog::openToolsDialog()
{
    saveCurrentScheme();
    PlanDialogUtils::replaceDialog(this, new ToolsDialog);
}

void ReferenceImageDialog::openOutputDialog()
{
    saveCurrentScheme();
    PlanDialogUtils::replaceDialog(this, new OutputDialog);
}

void ReferenceImageDialog::showCurrentImageMode()
{
    m_liveCaptureMode = true;
    updateReferenceImageControls();
    ui->viewerTitleLabel->setText(tr("当前图像"));
    ensureCameraRunning();
    refreshCurrentImage();
}

void ReferenceImageDialog::captureReferenceImage()
{
    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    const FrameInputMetadata metadata =
            CameraFrameProvider::instance().currentFrameMetadata();
    if (frame.empty()) {
        qWarning() << "[ReferenceImageDialog] 当前无图像，无法设置基准图。";
        ui->viewerTitleLabel->setText(tr("当前无图像"));
        if (m_previewHelper) {
            m_previewHelper->clear();
        }
        return;
    }

    QString error;
    if (!SchemeStore::instance().setReferenceFrame(frame, &error, metadata)) {
        qWarning() << "[ReferenceImageDialog] 基准图保存失败:" << error;
        QMessageBox::warning(this, tr("基准图保存失败"), tr("基准图保存失败：%1").arg(error));
        return;
    }
    showReferenceImageMode();
    qDebug() << QString("[ReferenceImageDialog] 已抓取静态基准图: %1x%2 type=%3")
                    .arg(frame.cols)
                    .arg(frame.rows)
                    .arg(frame.type());
}

void ReferenceImageDialog::showReferenceImageMode()
{
    m_liveCaptureMode = false;
    updateReferenceImageControls();
    refreshReferenceImage();
}

void ReferenceImageDialog::importReferenceImageFromPc()
{
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          tr("PC导入基准图"),
                                                          QString(),
                                                          tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (fileName.isEmpty())
        return;

    QImage image(fileName);
    if (image.isNull()) {
        qWarning() << "[ReferenceImageDialog] 图片导入失败，无法读取:" << fileName;
        ui->viewerTitleLabel->setText(tr("图片导入失败"));
        QMessageBox::warning(this, tr("图片导入失败"), tr("无法读取所选图片。"));
        return;
    }

    const FrameInputMetadata metadata = importedImageMetadata(image);

    const QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgbFrame(rgbImage.height(),
                     rgbImage.width(),
                     CV_8UC3,
                     const_cast<uchar *>(rgbImage.constBits()),
                     static_cast<size_t>(rgbImage.bytesPerLine()));
    cv::Mat bgrFrame;
    cv::cvtColor(rgbFrame, bgrFrame, cv::COLOR_RGB2BGR);

    if (bgrFrame.empty()) {
        qWarning() << "[ReferenceImageDialog] 图片导入失败，转换为空图像:" << fileName;
        ui->viewerTitleLabel->setText(tr("图片导入失败"));
        QMessageBox::warning(this, tr("图片导入失败"), tr("图片转换失败。"));
        return;
    }

    QString error;
    if (!SchemeStore::instance().setReferenceFrame(bgrFrame, &error, metadata)) {
        qWarning() << "[ReferenceImageDialog] PC 基准图保存失败:" << error;
        QMessageBox::warning(this, tr("基准图保存失败"), tr("基准图保存失败：%1").arg(error));
        return;
    }
    showReferenceImageMode();
    qDebug() << QString("[ReferenceImageDialog] 已导入 PC 基准图: %1 size=%2x%3 type=%4")
                    .arg(fileName)
                    .arg(bgrFrame.cols)
                    .arg(bgrFrame.rows)
                    .arg(bgrFrame.type());
}

void ReferenceImageDialog::setupReferenceImageControls()
{
    m_captureImageButton = new QPushButton(tr("抓取图像"), this);
    m_captureImageButton->setObjectName(QStringLiteral("captureImageButton"));
    m_captureImageButton->setMinimumSize(150, 46);
    m_captureImageButton->setIcon(QIcon(QStringLiteral(":/icons/camera.svg")));
    m_captureImageButton->setIconSize(QSize(24, 24));
    ui->horizontalLayout_referenceButtons->addWidget(m_captureImageButton);

    m_exitCaptureButton = new QPushButton(tr("退出相机抓取"), this);
    m_exitCaptureButton->setObjectName(QStringLiteral("exitCaptureButton"));
    m_exitCaptureButton->setMinimumSize(150, 46);
    m_exitCaptureButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    m_exitCaptureButton->setIconSize(QSize(24, 24));
    ui->horizontalLayout_referenceButtons->addWidget(m_exitCaptureButton);

    updateReferenceImageControls();
}

void ReferenceImageDialog::ensureCameraRunning()
{
    CameraFrameProvider &provider = CameraFrameProvider::instance();

    if (!provider.isOpened() && !provider.openCamera(QStringLiteral("/dev/video0"))) {
        qWarning() << "[ReferenceImageDialog] 当前图像模式启动失败：相机打开失败。";
        return;
    }

    if (!provider.isGrabbing() && !provider.startGrab()) {
        qWarning() << "[ReferenceImageDialog] 当前图像模式启动失败：采集启动失败。";
    }
}

void ReferenceImageDialog::updateReferenceImageControls()
{
    ui->currentImageButton->setVisible(!m_liveCaptureMode);
    ui->historyImageButton->setVisible(!m_liveCaptureMode);
    ui->pcImportButton->setVisible(!m_liveCaptureMode);

    if (m_captureImageButton) {
        m_captureImageButton->setVisible(m_liveCaptureMode);
    }
    if (m_exitCaptureButton) {
        m_exitCaptureButton->setVisible(m_liveCaptureMode);
    }
}

void ReferenceImageDialog::refreshCurrentImage()
{
    if (!m_previewHelper) {
        return;
    }

    const QImage image = CameraFrameProvider::instance().currentImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("当前无图像"));
        return;
    }

    m_previewHelper->setImage(image);
}

void ReferenceImageDialog::refreshReferenceImage()
{
    if (!m_previewHelper) {
        return;
    }

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        return;
    }

    ui->viewerTitleLabel->setText(tr("基准图"));
    m_previewHelper->setImage(image);
}
