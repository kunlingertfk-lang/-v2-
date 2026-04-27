#include "OutputDialog.h"

#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>

#include "CameraParamsDialog.h"
#include "PlanDialogUtils.h"
#include "ReferenceImageDialog.h"
#include "ToolsDialog.h"
#include "ui_OutputDialog.h"
#include "MainWindow.h"

OutputDialog::OutputDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::OutputDialog)
{
    ui->setupUi(this);
    setupUiState();
    connectNavigation();
    showMaximized();
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
