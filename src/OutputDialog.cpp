#include "OutputDialog.h"

#include <QFrame>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "CameraParamsDialog.h"
#include "PlanDialogUtils.h"
#include "ReferenceImageDialog.h"
#include "ToolsDialog.h"
#include "ui_OutputDialog.h"
#include "MainWindow.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"

OutputDialog::OutputDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::OutputDialog)
{
    ui->setupUi(this);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    setupOutputScrollArea();
    connectNavigation();
    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) { refreshReferencePreview(); });
    refreshReferencePreview();
    connect(ui->checkBox, &QCheckBox::toggled, this, [=](bool checked){
        if(checked) {
            ui->checkBox->setText("开");
        } else {
            ui->checkBox->setText("关");
        }
    });

    // 初始化显示
    ui->checkBox->setText(ui->checkBox->isChecked() ? "开" : "关");
}

OutputDialog::~OutputDialog()
{
    delete ui;
}

void OutputDialog::setupUiState()
{
    PlanDialogUtils::configureDialogWindow(this, tr("方案编辑 - 输出"));
    PlanDialogUtils::connectWindowButtons(this, ui->headerCloseButton, true);

    ui->cameraStepButton->setChecked(false);
    ui->referenceStepButton->setChecked(false);
    ui->toolsStepButton->setChecked(false);
    ui->outputStepButton->setChecked(true);
}

void OutputDialog::setupOutputScrollArea()
{
    if (findChild<QScrollArea *>(QStringLiteral("outputScrollArea"))) {
        return;
    }

    QScrollArea *scrollArea = new QScrollArea(ui->setupEditorPanel);
    scrollArea->setObjectName(QStringLiteral("outputScrollArea"));
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumWidth(0);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget *contentWidget = new QWidget(scrollArea);
    contentWidget->setObjectName(QStringLiteral("outputScrollAreaWidgetContents"));
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setObjectName(QStringLiteral("verticalLayout_outputScrollContents"));
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);

    ui->verticalLayout_editor->removeWidget(ui->schemeResultCard);
    ui->verticalLayout_editor->removeWidget(ui->timedOutputCard);
    ui->verticalLayout_editor->removeWidget(ui->outputParamsCard);
    ui->verticalLayout_editor->removeItem(ui->verticalSpacer_editor);

    contentLayout->addWidget(ui->schemeResultCard);
    contentLayout->addWidget(ui->timedOutputCard);
    contentLayout->addWidget(ui->outputParamsCard);
    contentLayout->addItem(ui->verticalSpacer_editor);

    scrollArea->setWidget(contentWidget);
    ui->verticalLayout_editor->insertWidget(1, scrollArea);
}

void OutputDialog::refreshReferencePreview()
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

void OutputDialog::connectNavigation()
{
    connect(ui->cameraStepButton, &QToolButton::clicked, this, &OutputDialog::openCameraParamsDialog);
    connect(ui->referenceStepButton, &QToolButton::clicked, this, &OutputDialog::openReferenceImageDialog);
    connect(ui->toolsStepButton, &QToolButton::clicked, this, &OutputDialog::openToolsDialog);
    connect(ui->previousButton, &QPushButton::clicked, this, &OutputDialog::openToolsDialog);
    connect(ui->finishButton, &QPushButton::clicked, this, &OutputDialog::finishSetup);
}

void OutputDialog::openCameraParamsDialog()
{
    PlanDialogUtils::replaceDialog(this, new CameraParamsDialog);
}

void OutputDialog::openReferenceImageDialog()
{
    PlanDialogUtils::replaceDialog(this, new ReferenceImageDialog);
}

void OutputDialog::openToolsDialog()
{
    PlanDialogUtils::replaceDialog(this, new ToolsDialog);
}

void OutputDialog::finishSetup()
{
    QMessageBox::information(this, tr("方案编辑"), tr("输出页面为流程最后一步，当前演示版本将返回主页面。"));
    PlanDialogUtils::returnToMainWindow(this);
}
