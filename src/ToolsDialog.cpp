#include "ToolsDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QSize>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "CharacterRecognitionDialog.h"
#include "BlobPresenceDialog.h"
#include "CameraParamsDialog.h"
#include "CirclePresenceDialog.h"
#include "MainWindow.h"
#include "OutputDialog.h"
#include "PatternPresenceDialog.h"
#include "PlanDialogUtils.h"
#include "ReferenceImageDialog.h"
#include "ToolLibraryDialog.h"
#include "WindowUtils.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "ui_ToolsDialog.h"

ToolsDialog::ToolsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ToolsDialog)
    , m_toolSerial(0)
{
    ui->setupUi(this);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    connectNavigation();
    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) { refreshReferencePreview(); });
    refreshReferencePreview();
}

ToolsDialog::~ToolsDialog()
{
    delete ui;
}

const QVector<ToolConfig> &ToolsDialog::toolConfigs() const
{
    return m_toolConfigs;
}

void ToolsDialog::setupUiState()
{
    PlanDialogUtils::configureDialogWindow(this, tr("方案编辑 - 工具"));
    if (qobject_cast<MainWindow *>(parentWidget())) {
        connect(ui->headerCloseButton, &QToolButton::clicked, this, &ToolsDialog::accept);
    } else {
        PlanDialogUtils::connectWindowButtons(this, ui->headerCloseButton, true);
    }


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

void ToolsDialog::refreshReferencePreview()
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

void ToolsDialog::openToolLibrary()
{
    ToolType selectedToolType = ToolType::Unknown;

    {
        ToolLibraryDialog library(this);
        library.setWindowModality(Qt::WindowModal);
        library.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        WindowUtils::fitDialogToScreen(&library, this, 40);
        WindowUtils::centerWindowOnScreen(&library, this, 40);

        QTimer::singleShot(0, &library, [&library]() {
            library.raise();
            library.activateWindow();
        });

        if (library.exec() != QDialog::Accepted) {
            return;
        }

        selectedToolType = library.selectedToolType();
    }

    if (selectedToolType == ToolType::Ocr) {
        CharacterRecognitionDialog configDialog(this);
        configDialog.setWindowModality(Qt::WindowModal);
        configDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        WindowUtils::applyLargeWindow(&configDialog);

        QTimer::singleShot(0, &configDialog, [&configDialog]() {
            configDialog.raise();
            configDialog.activateWindow();
        });
        configDialog.raise();
        configDialog.activateWindow();

        if (configDialog.exec() == QDialog::Accepted) {
            const ToolConfig config = configDialog.toToolConfig();
            m_toolConfigs.append(config);
            addConfiguredTool(config);
            raise();
            activateWindow();
        }
        return;
    }

    if (selectedToolType == ToolType::PatternPresence) {
        PatternPresenceDialog configDialog(this);
        configDialog.setWindowModality(Qt::WindowModal);
        configDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        WindowUtils::applyLargeWindow(&configDialog);

        QTimer::singleShot(0, &configDialog, [&configDialog]() {
            configDialog.raise();
            configDialog.activateWindow();
        });
        configDialog.raise();
        configDialog.activateWindow();

        if (configDialog.exec() == QDialog::Accepted) {
            const ToolConfig config = configDialog.toToolConfig();
            m_toolConfigs.append(config);
            addConfiguredTool(config);
            raise();
            activateWindow();
        }
        return;
    }

    if (selectedToolType == ToolType::BlobPresence) {
        BlobPresenceDialog configDialog(this);
        configDialog.setWindowModality(Qt::WindowModal);
        configDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        WindowUtils::applyLargeWindow(&configDialog);

        QTimer::singleShot(0, &configDialog, [&configDialog]() {
            configDialog.raise();
            configDialog.activateWindow();
        });
        configDialog.raise();
        configDialog.activateWindow();

        if (configDialog.exec() == QDialog::Accepted) {
            const ToolConfig config = configDialog.toToolConfig();
            m_toolConfigs.append(config);
            addConfiguredTool(config);
            raise();
            activateWindow();
        }
        return;
    }

    if (selectedToolType == ToolType::CirclePresence) {
        CirclePresenceDialog configDialog(this);
        configDialog.setWindowModality(Qt::WindowModal);
        configDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        WindowUtils::applyLargeWindow(&configDialog);

        QTimer::singleShot(0, &configDialog, [&configDialog]() {
            configDialog.raise();
            configDialog.activateWindow();
        });
        configDialog.raise();
        configDialog.activateWindow();

        if (configDialog.exec() == QDialog::Accepted) {
            const ToolConfig config = configDialog.toToolConfig();
            m_toolConfigs.append(config);
            addConfiguredTool(config);
            raise();
            activateWindow();
        }
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

void ToolsDialog::addConfiguredTool(const ToolConfig &config)
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

    QLabel *titleLabel = new QLabel(config.displayName.isEmpty() ? config.toolName : config.displayName, card);
    titleLabel->setObjectName(QStringLiteral("toolTitleLabel"));
    titleLabel->setProperty("role", QStringLiteral("toolTitle"));

    QLabel *summaryLabel = new QLabel(config.summary, card);
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
