#include "ReferenceImageDialog.h"

#include <QPushButton>
#include <QToolButton>

#include "CameraParamsDialog.h"
#include "OutputDialog.h"
#include "PlanDialogUtils.h"
#include "ToolsDialog.h"
#include "ui_ReferenceImageDialog.h"

ReferenceImageDialog::ReferenceImageDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ReferenceImageDialog)
{
    ui->setupUi(this);
    setupUiState();
    connectNavigation();
    showMaximized();
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
}

void ReferenceImageDialog::connectNavigation()
{
    connect(ui->cameraStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openCameraParamsDialog);
    connect(ui->toolsStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openToolsDialog);
    connect(ui->outputStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openOutputDialog);
    connect(ui->previousButton, &QPushButton::clicked, this, &ReferenceImageDialog::openCameraParamsDialog);
    connect(ui->nextButton, &QPushButton::clicked, this, &ReferenceImageDialog::openToolsDialog);
}

void ReferenceImageDialog::openCameraParamsDialog()
{
    PlanDialogUtils::replaceDialog(this, new CameraParamsDialog);
}

void ReferenceImageDialog::openToolsDialog()
{
    PlanDialogUtils::replaceDialog(this, new ToolsDialog);
}

void ReferenceImageDialog::openOutputDialog()
{
    PlanDialogUtils::replaceDialog(this, new OutputDialog);
}
