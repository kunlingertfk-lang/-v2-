#include "ToolsDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QSize>
#include <QToolButton>
#include <QVBoxLayout>

#include "CharacterRecognitionDialog.h"
#include "CameraParamsDialog.h"
#include "OutputDialog.h"
#include "PlanDialogUtils.h"
#include "ReferenceImageDialog.h"
#include "ToolLibraryDialog.h"
#include "ui_ToolsDialog.h"

ToolsDialog::ToolsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ToolsDialog)
    , m_toolSerial(0)
{
    ui->setupUi(this);
    setupUiState();
    connectNavigation();
    showMaximized();
}

ToolsDialog::~ToolsDialog()
{
    delete ui;
}

void ToolsDialog::setupUiState()
{
    PlanDialogUtils::configureDialogWindow(this, tr("方案编辑 - 工具"));
    PlanDialogUtils::connectWindowButtons(this, ui->headerCloseButton, true);


    ui->cameraStepButton->setChecked(false);
    ui->referenceStepButton->setChecked(false);
    ui->toolsStepButton->setChecked(true);
    ui->outputStepButton->setChecked(false);
}

void ToolsDialog::connectNavigation()
{
    connect(ui->addToolButton, &QToolButton::clicked, this, &ToolsDialog::openToolLibrary);
    connect(ui->cameraStepButton, &QToolButton::clicked, this, &ToolsDialog::openCameraParamsDialog);
    connect(ui->referenceStepButton, &QToolButton::clicked, this, &ToolsDialog::openReferenceImageDialog);
    connect(ui->outputStepButton, &QToolButton::clicked, this, &ToolsDialog::openOutputDialog);
    connect(ui->previousButton, &QPushButton::clicked, this, &ToolsDialog::openReferenceImageDialog);
    connect(ui->nextButton, &QPushButton::clicked, this, &ToolsDialog::openOutputDialog);
}

void ToolsDialog::openToolLibrary()
{
    ToolLibraryDialog library(this);
    const QPoint center = frameGeometry().center() - QPoint(library.width() / 2, library.height() / 2);
    library.move(center);

    if (library.exec() != QDialog::Accepted) {
        return;
    }

    if (library.selectedTool() != ToolLibraryDialog::CharacterRecognition) {
        return;
    }

    CharacterRecognitionDialog configDialog(this);

    if (configDialog.exec() == QDialog::Accepted) {
        addCharacterRecognitionTool(configDialog.summaryText());
        raise();
        activateWindow();
    }
}

void ToolsDialog::openCameraParamsDialog()
{
    PlanDialogUtils::replaceDialog(this, new CameraParamsDialog);
}

void ToolsDialog::openReferenceImageDialog()
{
    PlanDialogUtils::replaceDialog(this, new ReferenceImageDialog);
}

void ToolsDialog::openOutputDialog()
{
    PlanDialogUtils::replaceDialog(this, new OutputDialog);
}

void ToolsDialog::addCharacterRecognitionTool(const QString &summaryText)
{
    QFrame *card = new QFrame(ui->scrollAreaWidgetContents);
    card->setObjectName(QStringLiteral("toolSelectedCard"));
    card->setProperty("panelRole", QStringLiteral("toolSelected"));
    card->setMinimumHeight(88);

    QHBoxLayout *rowLayout = new QHBoxLayout(card);
    rowLayout->setContentsMargins(16, 12, 16, 12);
    rowLayout->setSpacing(14);

    QLabel *statusBadge = new QLabel(tr("启用"), card);
    statusBadge->setObjectName(QStringLiteral("toolStatusBadge"));
    rowLayout->addWidget(statusBadge);

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setSpacing(6);

    QLabel *titleLabel = new QLabel(tr("字符识别_%1").arg(m_toolSerial++), card);
    titleLabel->setObjectName(QStringLiteral("toolTitleLabel"));
    titleLabel->setProperty("role", QStringLiteral("toolTitle"));

    QLabel *summaryLabel = new QLabel(summaryText, card);
    summaryLabel->setObjectName(QStringLiteral("toolSubTextLabel"));
    summaryLabel->setProperty("role", QStringLiteral("toolSubText"));
    summaryLabel->setWordWrap(true);

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(summaryLabel);
    rowLayout->addLayout(textLayout, 1);

    QToolButton *gearButton = new QToolButton(card);
    gearButton->setObjectName(QStringLiteral("toolGearButton"));
    gearButton->setProperty("role", QStringLiteral("toolGear"));
    gearButton->setIcon(QIcon(QStringLiteral(":/icons/settings.svg")));
    gearButton->setIconSize(QSize(22, 22));
    rowLayout->addWidget(gearButton);

    ui->verticalLayout_toolsList->addWidget(card);
}
