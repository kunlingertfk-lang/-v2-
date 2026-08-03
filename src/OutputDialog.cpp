#include "OutputDialog.h"

#include <QDebug>
#include <QFrame>
#include <QInputDialog>
#include <QJsonObject>
#include <QLineEdit>
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
#include "SchemeStore.h"
#include "ToolsDialog.h"
#include "ui_OutputDialog.h"
#include "MainWindow.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"

OutputDialog::OutputDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::OutputDialog)
    , m_sourceMainWindow(qobject_cast<MainWindow *>(parent))
{
    ui->setupUi(this);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);
    setupUiState();
    setupOutputScrollArea();
    connectNavigation();
    loadCurrentSchemeState();
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
    refreshSchemeHeader();
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

void OutputDialog::setSchemeTools(const QVector<ToolConfig> &configs,
                                  const QMap<QString, ToolPreviewSnapshot> &referenceSnapshots)
{
    m_schemeToolConfigs = configs;
    m_referencePreviewSnapshots.clear();

    for (const ToolConfig &config : m_schemeToolConfigs) {
        const ToolPreviewSnapshot snapshot = referenceSnapshots.value(config.toolId);
        if (!snapshot.valid)
            continue;

        ToolPreviewSnapshot normalized = snapshot;
        normalized.toolId = config.toolId;
        normalized.toolType = config.toolType;
        m_referencePreviewSnapshots.insert(config.toolId, normalized);
    }
    commitOutputStateToScheme(false);
}

void OutputDialog::connectNavigation()
{
    connect(ui->cameraStepButton, &QToolButton::clicked, this, &OutputDialog::openCameraParamsDialog);
    connect(ui->referenceStepButton, &QToolButton::clicked, this, &OutputDialog::openReferenceImageDialog);
    connect(ui->toolsStepButton, &QToolButton::clicked, this, &OutputDialog::openToolsDialog);
    connect(ui->previousButton, &QPushButton::clicked, this, &OutputDialog::openToolsDialog);
    connect(ui->finishButton, &QPushButton::clicked, this, &OutputDialog::finishSetup);
    connect(ui->setupExternalEditButton, &QToolButton::clicked, this, &OutputDialog::editCurrentSchemeName);
    connect(ui->setupSaveButton, &QToolButton::clicked, this, &OutputDialog::saveCurrentScheme);
    connect(ui->setupSaveAsButton, &QToolButton::clicked, this, &OutputDialog::saveCurrentSchemeAs);
}

void OutputDialog::refreshSchemeHeader()
{
    ui->setupPageCodeLabel->setText(SchemeStore::instance().currentSchemeName());
}

void OutputDialog::loadCurrentSchemeState()
{
    QString error;
    if (!SchemeStore::instance().ensureLoaded(&error)) {
        qWarning() << "[OutputDialog] 方案加载失败:" << error;
        return;
    }

    const SchemeState &scheme = SchemeStore::instance().currentScheme();
    if (m_schemeToolConfigs.isEmpty() && m_referencePreviewSnapshots.isEmpty()) {
        m_schemeToolConfigs = scheme.toolConfigs;
        m_referencePreviewSnapshots = scheme.referencePreviewSnapshots;
    }

    if (!scheme.outputConfig.isEmpty()) {
        ui->checkBox->setChecked(scheme.outputConfig.value(QStringLiteral("timedOutputEnabled")).toBool(ui->checkBox->isChecked()));
        ui->timedSpinBox->setValue(scheme.outputConfig.value(QStringLiteral("timedOutputMs")).toInt(ui->timedSpinBox->value()));
        ui->checkBox_2->setChecked(scheme.outputConfig.value(QStringLiteral("resultOutputEnabled")).toBool(ui->checkBox_2->isChecked()));
    }
    refreshSchemeHeader();
}

bool OutputDialog::commitOutputStateToScheme(bool saveToDisk)
{
    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        qWarning() << "[OutputDialog] 方案状态初始化失败:" << error;
        return false;
    }

    QJsonObject outputConfig = store.currentScheme().outputConfig;
    outputConfig.insert(QStringLiteral("timedOutputEnabled"), ui->checkBox->isChecked());
    outputConfig.insert(QStringLiteral("timedOutputMs"), ui->timedSpinBox->value());
    outputConfig.insert(QStringLiteral("resultOutputEnabled"), ui->checkBox_2->isChecked());

    store.setToolConfigs(m_schemeToolConfigs, m_referencePreviewSnapshots);
    store.setOutputConfig(outputConfig);
    if (saveToDisk && !store.saveCurrentScheme(&error)) {
        qWarning() << "[OutputDialog] 方案保存失败:" << error;
        QMessageBox::warning(this, tr("保存失败"), tr("方案保存失败：%1").arg(error));
        return false;
    }
    if (saveToDisk) {
        if (MainWindow *mainWindow = sourceMainWindow()) {
            mainWindow->applySavedSchemeTools(m_schemeToolConfigs,
                                              m_referencePreviewSnapshots);
        }
    }

    refreshSchemeHeader();
    return true;
}

void OutputDialog::editCurrentSchemeName()
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
    commitOutputStateToScheme(true);
}

void OutputDialog::saveCurrentScheme()
{
    commitOutputStateToScheme(true);
}

void OutputDialog::saveCurrentSchemeAs()
{
    if (!commitOutputStateToScheme(false))
        return;

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
        qWarning() << "[OutputDialog] 方案另存为失败:" << error;
        QMessageBox::warning(this, tr("另存为失败"), tr("方案另存为失败：%1").arg(error));
        return;
    }

    loadCurrentSchemeState();
    refreshSchemeHeader();
}

void OutputDialog::openCameraParamsDialog()
{
    if (!commitOutputStateToScheme(true))
        return;
    PlanDialogUtils::replaceDialog(this, new CameraParamsDialog);
}

void OutputDialog::openReferenceImageDialog()
{
    if (!commitOutputStateToScheme(true))
        return;
    PlanDialogUtils::replaceDialog(this, new ReferenceImageDialog);
}

void OutputDialog::openToolsDialog()
{
    if (!commitOutputStateToScheme(true))
        return;
    MainWindow *mainWindow = sourceMainWindow();
    ToolsDialog *dialog = new ToolsDialog(mainWindow);
    dialog->setInitialToolState(m_schemeToolConfigs, m_referencePreviewSnapshots);
    PlanDialogUtils::showDialogFromWidget(this, dialog);
    close();
}

void OutputDialog::finishSetup()
{
    if (!commitOutputStateToScheme(true))
        return;

    QMessageBox::information(this, tr("方案编辑"), tr("输出页面为流程最后一步，当前演示版本将返回主页面。"));
    returnToSourceMainWindow();
}

MainWindow *OutputDialog::sourceMainWindow() const
{
    if (m_sourceMainWindow)
        return m_sourceMainWindow.data();

    QWidget *widget = parentWidget();
    while (widget) {
        if (MainWindow *mainWindow = qobject_cast<MainWindow *>(widget))
            return mainWindow;
        widget = widget->parentWidget();
    }

    return nullptr;
}

void OutputDialog::returnToSourceMainWindow()
{
    MainWindow *mainWindow = sourceMainWindow();
    if (!mainWindow) {
        qWarning() << "[SCHEME-RETURN] OutputDialog return failed: original MainWindow not found";
        QMessageBox::warning(this, tr("方案编辑"), tr("无法返回主界面：未找到原主窗口。"));
        return;
    }

    qDebug() << "[SCHEME-RETURN] OutputDialog returnToSourceMainWindow"
             << "mainWindow=" << mainWindow
             << "sourceMainWindowValid=" << !m_sourceMainWindow.isNull();

    mainWindow->applySavedSchemeTools(m_schemeToolConfigs,
                                      m_referencePreviewSnapshots);
    PlanDialogUtils::showWindowFromWidget(this, mainWindow);
    close();
}
