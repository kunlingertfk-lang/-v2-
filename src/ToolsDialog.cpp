#include "ToolsDialog.h"

#include <QDebug>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPoint>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

#include <opencv2/imgproc.hpp>

#include "CharacterRecognitionDialog.h"
#include "BlobPresenceDialog.h"
#include "CameraParamsDialog.h"
#include "ColorComparisonDialog.h"
#include "ColorRecognitionDialog.h"
#include "ClassificationDialog.h"
#include "CirclePresenceDialog.h"
#include "ContourPresenceDialog.h"
#include "EdgePresenceDialog.h"
#include "LinePresenceDialog.h"
#include "MainWindow.h"
#include "ObjectDetectionDialog.h"
#include "OutputDialog.h"
#include "PatternPresenceDialog.h"
#include "PlanDialogUtils.h"
#include "ReferenceImageDialog.h"
#include "SchemeStore.h"
#include "ToolLibraryDialog.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "ui_ToolsDialog.h"

namespace {

constexpr int kToolCardHeight = 96;
constexpr int kToolListMargin = 8;
constexpr int kToolListSpacing = 10;

void refreshWidgetStyle(QWidget *widget)
{
    if (!widget)
        return;

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QImage imageFromFrame(const cv::Mat &frame)
{
    return MatImageConverter::matToDisplayImage(frame, QStringLiteral("ToolsDialog"));
}

//当前实时图
QImage currentReferenceImage()
{
    return imageFromFrame(ReferenceImageProvider::instance().referenceFrame());
}

QString toolIconForType(ToolType type)
{
    switch (type) {
    case ToolType::ColorRecognition:
    case ToolType::ColorComparison:
        return QStringLiteral(":/icons/compare.svg");
    case ToolType::Ocr:
        return QStringLiteral(":/icons/tool.svg");
    case ToolType::AiDetection:
    case ToolType::AiClassification:
        return QStringLiteral(":/icons/monitor.svg");
    case ToolType::PatternPresence:
    case ToolType::BlobPresence:
    case ToolType::CirclePresence:
    case ToolType::EdgePresence:
    case ToolType::LinePresence:
    case ToolType::ContourPresence:
        return QStringLiteral(":/icons/eye.svg");
    default:
        return QStringLiteral(":/icons/tool.svg");
    }
}

QVector<ToolOverlay> previewOverlaysForToolsPage(const ToolConfig &config,
                                                 const ToolPreviewSnapshot &snapshot)
{
    if (config.toolType != ToolType::ColorRecognition)
        return snapshot.overlays;

    QVector<ToolOverlay> filtered;
    filtered.reserve(snapshot.overlays.size());
    for (const ToolOverlay &overlay : snapshot.overlays) {
        if (overlay.type == ToolOverlayType::Rect &&
            overlay.label.compare(QStringLiteral("ROI"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        filtered.append(overlay);
    }
    return filtered;
}

template <typename Dialog>
bool runToolConfigDialog(QWidget *parent,
                         const ToolConfig *initialConfig,
                         ToolConfig *toolConfig,
                         ToolPreviewSnapshot *snapshot)
{
    Dialog configDialog(parent);
    configDialog.setWindowModality(Qt::WindowModal);
    configDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(&configDialog);

    if (initialConfig)
        configDialog.loadFromConfig(*initialConfig);

    QTimer::singleShot(0, &configDialog, [&configDialog]() {
        configDialog.raise();
        configDialog.activateWindow();
    });
    configDialog.raise();
    configDialog.activateWindow();

    if (configDialog.exec() != QDialog::Accepted)
        return false;

    if (toolConfig)
        *toolConfig = configDialog.toolConfig();
    if (snapshot)
        *snapshot = configDialog.referencePreviewSnapshot();
    return true;
}

} // namespace

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
    QString error;
    if (SchemeStore::instance().ensureLoaded(&error)) {
        const SchemeState &scheme = SchemeStore::instance().currentScheme();
        setInitialToolState(scheme.toolConfigs, scheme.referencePreviewSnapshots);
    } else {
        qWarning() << "[ToolsDialog] 方案加载失败:" << error;
    }
    refreshSchemeHeader();
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

const QMap<QString, ToolPreviewSnapshot> &ToolsDialog::referencePreviewSnapshots() const
{
    return m_toolPreviewSnapshots;
}

void ToolsDialog::setInitialToolState(const QVector<ToolConfig> &configs,
                                      const QMap<QString, ToolPreviewSnapshot> &snapshots)
{
    m_toolConfigs = configs;
    m_toolPreviewSnapshots.clear();

    for (const ToolConfig &config : m_toolConfigs) {
        const ToolPreviewSnapshot snapshot = snapshots.value(config.toolId);
        if (!snapshot.valid)
            continue;

        ToolPreviewSnapshot normalized = snapshot;
        normalized.toolId = config.toolId;
        normalized.toolType = config.toolType;
        m_toolPreviewSnapshots.insert(config.toolId, normalized);
    }

    m_selectedToolIndex = m_toolConfigs.isEmpty() ? -1 : 0;
    refreshSchemeHeader();
    refreshToolList();
    refreshReferencePreview();
}

bool ToolsDialog::openedOutputDialog() const
{
    return m_openedOutputDialog;
}

bool ToolsDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QFrame *card = qobject_cast<QFrame *>(watched);
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (card && mouseEvent->button() == Qt::LeftButton) {
            const QVariant indexValue = card->property("toolIndex");
            if (indexValue.isValid()) {
                selectTool(indexValue.toInt());
                return true;
            }
        }
    }

    return QDialog::eventFilter(watched, event);
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

    ui->verticalLayout_toolsList->setSpacing(kToolListSpacing);
    ui->verticalLayout_toolsList->setContentsMargins(kToolListMargin,
                                                     kToolListMargin,
                                                     kToolListMargin,
                                                     kToolListMargin);
    ui->scrollAreaWidgetContents->setSizePolicy(QSizePolicy::Preferred,
                                                QSizePolicy::MinimumExpanding);
    refreshSchemeHeader();
    refreshToolList();
}

void ToolsDialog::connectNavigation()
{
    connect(ui->addToolButton, &QToolButton::clicked, this, &ToolsDialog::openToolLibrary);
    connect(ui->cameraStepButton, &QToolButton::clicked, this, &ToolsDialog::openCameraParamsDialog);
    connect(ui->referenceStepButton, &QToolButton::clicked, this, &ToolsDialog::openReferenceImageDialog);
    connect(ui->outputStepButton, &QToolButton::clicked, this, &ToolsDialog::openOutputDialog);
    connect(ui->previousButton, &QPushButton::clicked, this, &ToolsDialog::openReferenceImageDialog);
    connect(ui->nextButton, &QPushButton::clicked, this, &ToolsDialog::openOutputDialog);
    connect(ui->setupExternalEditButton, &QToolButton::clicked, this, &ToolsDialog::editCurrentSchemeName);
    connect(ui->setupSaveButton, &QToolButton::clicked, this, &ToolsDialog::saveCurrentScheme);
    connect(ui->setupSaveAsButton, &QToolButton::clicked, this, &ToolsDialog::saveCurrentSchemeAs);
}

void ToolsDialog::refreshSchemeHeader()
{
    ui->setupPageCodeLabel->setText(SchemeStore::instance().currentSchemeName());
}

bool ToolsDialog::commitToolStateToScheme(bool saveToDisk)
{
    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        qWarning() << "[ToolsDialog] 方案状态初始化失败:" << error;
        return false;
    }

    store.setToolConfigs(m_toolConfigs, m_toolPreviewSnapshots);
    if (saveToDisk && !store.saveCurrentScheme(&error)) {
        qWarning() << "[ToolsDialog] 方案保存失败:" << error;
        QMessageBox::warning(this, tr("保存失败"), tr("方案保存失败：%1").arg(error));
        return false;
    }

    refreshSchemeHeader();
    return true;
}

void ToolsDialog::editCurrentSchemeName()
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
    commitToolStateToScheme(true);
}

void ToolsDialog::saveCurrentScheme()
{
    commitToolStateToScheme(true);
}

void ToolsDialog::saveCurrentSchemeAs()
{
    commitToolStateToScheme(false);

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
        qWarning() << "[ToolsDialog] 方案另存为失败:" << error;
        QMessageBox::warning(this, tr("另存为失败"), tr("方案另存为失败：%1").arg(error));
        return;
    }

    const SchemeState &scheme = SchemeStore::instance().currentScheme();
    setInitialToolState(scheme.toolConfigs, scheme.referencePreviewSnapshots);
    refreshSchemeHeader();
}

void ToolsDialog::refreshReferencePreview()
{
    if (!m_previewHelper)
        return;

    if (m_selectedToolIndex >= 0 && m_selectedToolIndex < m_toolConfigs.size()) {
        showSelectedToolPreview();
        return;
    }

    const QImage image = currentReferenceImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        ui->viewerStatusLabel->clear();
        return;
    }

    ui->viewerTitleLabel->setText(tr("基准图"));
    m_previewHelper->setImage(image);
    m_previewHelper->clearToolOverlays();
}

void ToolsDialog::openToolLibrary()
{
    ToolType selectedToolType = ToolType::Unknown;

    {
        ToolLibraryDialog library(this);
        library.setWindowModality(Qt::WindowModal);
        library.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        PlanDialogUtils::fitDialogToScreen(&library, this, 40);
        PlanDialogUtils::centerWindowOnScreen(&library, this, 40);

        QTimer::singleShot(0, &library, [&library]() {
            library.raise();
            library.activateWindow();
        });

        if (library.exec() != QDialog::Accepted) {
            return;
        }

        selectedToolType = library.selectedToolType();
    }

    openToolConfigDialogForAdd(selectedToolType);
}

void ToolsDialog::openCameraParamsDialog()
{
    if (!commitToolStateToScheme(true))
        return;
    PlanDialogUtils::replaceDialog(this, new CameraParamsDialog);
}

void ToolsDialog::openReferenceImageDialog()
{
    if (!commitToolStateToScheme(true))
        return;
    PlanDialogUtils::replaceDialog(this, new ReferenceImageDialog);
}

void ToolsDialog::openOutputDialog()
{
    m_openedOutputDialog = true;
    if (!commitToolStateToScheme(true))
        return;

    MainWindow *mainWindow = qobject_cast<MainWindow *>(parentWidget());
    OutputDialog *dialog = new OutputDialog(mainWindow);
    dialog->setSchemeTools(m_toolConfigs, m_toolPreviewSnapshots);
    PlanDialogUtils::showDialogFromWidget(this, dialog);
    close();
}

void ToolsDialog::addConfiguredTool(const ToolConfig &config, const ToolPreviewSnapshot &snapshot)
{
    if (!m_toolConfigs.isEmpty())
        m_selectedToolIndex = m_toolConfigs.size() - 1;

    storePreviewSnapshot(config, snapshot, false);
    refreshToolList();
    selectTool(m_selectedToolIndex);
}

bool ToolsDialog::openToolConfigDialogForAdd(ToolType type)
{
    ToolConfig config;
    ToolPreviewSnapshot snapshot;
    bool accepted = false;

    switch (type) {
    case ToolType::Ocr:
        accepted = runToolConfigDialog<CharacterRecognitionDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::ColorRecognition:
        accepted = runToolConfigDialog<ColorRecognitionDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::ColorComparison:
        accepted = runToolConfigDialog<ColorComparisonDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::PatternPresence:
        accepted = runToolConfigDialog<PatternPresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::BlobPresence:
        accepted = runToolConfigDialog<BlobPresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::CirclePresence:
        accepted = runToolConfigDialog<CirclePresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::EdgePresence:
        accepted = runToolConfigDialog<EdgePresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::LinePresence:
        accepted = runToolConfigDialog<LinePresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::ContourPresence:
        accepted = runToolConfigDialog<ContourPresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::AiDetection:
        accepted = runToolConfigDialog<ObjectDetectionDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::AiClassification:
        accepted = runToolConfigDialog<ClassificationDialog>(this, nullptr, &config, &snapshot);
        break;
/*============================tfk add=================================*/
    case ToolType::TemplateLocation:
        qDebug() << "[ToolsDialog] TemplateLocation dialog is not implemented yet.";
        break;

/*============================tfk end=================================*/
    default:
        qDebug() << "[ToolsDialog] Unsupported tool add type:" << toolTypeToString(type);
        break;
    }

    if (!accepted)
        return false;

    m_toolConfigs.append(config);
    addConfiguredTool(config, snapshot);
    commitToolStateToScheme(true);
    raise();
    activateWindow();
    return true;
}

bool ToolsDialog::openToolConfigDialogForEdit(int index)
{
    if (index < 0 || index >= m_toolConfigs.size())
        return false;

    const ToolConfig originalConfig = m_toolConfigs.at(index);
    ToolConfig editedConfig;
    ToolPreviewSnapshot snapshot;
    bool accepted = false;

    switch (originalConfig.toolType) {
    case ToolType::Ocr:
        accepted = runToolConfigDialog<CharacterRecognitionDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::ColorRecognition:
        accepted = runToolConfigDialog<ColorRecognitionDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::ColorComparison:
        accepted = runToolConfigDialog<ColorComparisonDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::PatternPresence:
        accepted = runToolConfigDialog<PatternPresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::BlobPresence:
        accepted = runToolConfigDialog<BlobPresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::CirclePresence:
        accepted = runToolConfigDialog<CirclePresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::EdgePresence:
        accepted = runToolConfigDialog<EdgePresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::LinePresence:
        accepted = runToolConfigDialog<LinePresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::ContourPresence:
        accepted = runToolConfigDialog<ContourPresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::AiDetection:
        accepted = runToolConfigDialog<ObjectDetectionDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::AiClassification:
        accepted = runToolConfigDialog<ClassificationDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    default:
        qDebug() << "[ToolsDialog] Unsupported tool edit type:" << toolTypeToString(originalConfig.toolType);
        break;
    }

    if (!accepted)
        return false;

    editedConfig.toolId = originalConfig.toolId;
    editedConfig.toolType = originalConfig.toolType;
    editedConfig.category = originalConfig.category;
    m_toolConfigs[index] = editedConfig;
    m_selectedToolIndex = index;
    storePreviewSnapshot(editedConfig, snapshot, true);
    refreshToolList();
    selectTool(index);
    commitToolStateToScheme(true);
    raise();
    activateWindow();
    return true;
}

void ToolsDialog::storePreviewSnapshot(const ToolConfig &config,
                                       const ToolPreviewSnapshot &snapshot,
                                       bool keepExistingWhenInvalid)
{
    if (config.toolId.trimmed().isEmpty())
        return;

    if (!snapshot.valid) {
        if (!keepExistingWhenInvalid)
            m_toolPreviewSnapshots.remove(config.toolId);
        return;
    }

    ToolPreviewSnapshot normalized = snapshot;
    normalized.toolId = config.toolId;
    normalized.toolType = config.toolType;
    m_toolPreviewSnapshots.insert(config.toolId, normalized);
}

void ToolsDialog::refreshToolList()
{
    clearToolList();

    if (m_toolConfigs.isEmpty()) {
        m_selectedToolIndex = -1;
    } else if (m_selectedToolIndex < 0 || m_selectedToolIndex >= m_toolConfigs.size()) {
        m_selectedToolIndex = m_toolConfigs.size() - 1;
    }

    m_toolCards.reserve(m_toolConfigs.size());
    for (int index = 0; index < m_toolConfigs.size(); ++index) {
        QFrame *card = createToolCard(m_toolConfigs.at(index), index);
        m_toolCards.append(card);
        ui->verticalLayout_toolsList->addWidget(card);
    }

    ui->verticalLayout_toolsList->addStretch(1);
    updateToolCardSelection();
    m_toolSerial = m_toolConfigs.size();
}

void ToolsDialog::clearToolList()
{
    while (QLayoutItem *item = ui->verticalLayout_toolsList->takeAt(0)) {
        if (QWidget *widget = item->widget())
            delete widget;
        delete item;
    }
    m_toolCards.clear();
}

QFrame *ToolsDialog::createToolCard(const ToolConfig &config, int index)
{
    QFrame *card = new QFrame(ui->scrollAreaWidgetContents);
    card->setObjectName(QStringLiteral("toolItemCard"));
    card->setProperty("panelRole", QStringLiteral("toolItem"));
    card->setProperty("selected", false);
    card->setProperty("toolIndex", index);
    card->setProperty("toolId", config.toolId);
    card->setFixedHeight(kToolCardHeight);
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    card->setCursor(Qt::PointingHandCursor);
    card->installEventFilter(this);

    QHBoxLayout *rowLayout = new QHBoxLayout(card);
    rowLayout->setContentsMargins(14, 10, 12, 10);
    rowLayout->setSpacing(10);

    QLabel *indexBadge = new QLabel(QStringLiteral("#%1").arg(index + 1), card);
    indexBadge->setObjectName(QStringLiteral("toolIndexBadge"));
    indexBadge->setProperty("role", QStringLiteral("toolIndexBadge"));
    indexBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    rowLayout->addWidget(indexBadge);

    QLabel *statusBadge = new QLabel(config.enabled ? tr("启用") : tr("停用"), card);
    statusBadge->setObjectName(QStringLiteral("toolStatusBadge"));
    statusBadge->setProperty("role", QStringLiteral("toolStatusBadge"));
    statusBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    rowLayout->addWidget(statusBadge);

    QLabel *typeIcon = new QLabel(card);
    typeIcon->setObjectName(QStringLiteral("toolTypeIconLabel"));
    typeIcon->setProperty("role", QStringLiteral("toolTypeIcon"));
    typeIcon->setAlignment(Qt::AlignCenter);
    typeIcon->setFixedSize(34, 34);
    typeIcon->setPixmap(QIcon(toolIconForType(config.toolType)).pixmap(QSize(24, 24)));
    typeIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    rowLayout->addWidget(typeIcon);

    const QString title = toolDisplayName(config);
    QLabel *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("toolTitleLabel"));
    titleLabel->setProperty("role", QStringLiteral("toolTitle"));
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    titleLabel->setToolTip(title);
    rowLayout->addWidget(titleLabel, 1);

    const QString previewState = toolPreviewStateText(config);
    QLabel *resultBadge = new QLabel(previewState, card);
    resultBadge->setObjectName(QStringLiteral("toolResultBadge"));
    resultBadge->setProperty("role", QStringLiteral("toolResultBadge"));
    resultBadge->setProperty("testState", previewState == QStringLiteral("OK")
                             ? QStringLiteral("ok")
                             : (previewState == QStringLiteral("NG") ? QStringLiteral("ng") : QStringLiteral("untested")));
    resultBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    rowLayout->addWidget(resultBadge);

    QToolButton *gearButton = new QToolButton(card);
    gearButton->setObjectName(QStringLiteral("toolGearButton"));
    gearButton->setProperty("role", QStringLiteral("toolGear"));
    gearButton->setIcon(QIcon(QStringLiteral(":/icons/settings.svg")));
    gearButton->setIconSize(QSize(22, 22));
    gearButton->setToolTip(tr("编辑工具参数"));
    rowLayout->addWidget(gearButton);

    connect(gearButton, &QToolButton::clicked, this, [this, index]() {
        openToolConfigDialogForEdit(index);
    });

    return card;
}

void ToolsDialog::selectTool(int index)
{
    if (index < 0 || index >= m_toolConfigs.size())
        return;

    m_selectedToolIndex = index;
    updateToolCardSelection();
    showSelectedToolPreview();

    const ToolConfig &config = m_toolConfigs.at(index);
    qDebug() << "[ToolsDialog] 选中工具"
             << "index=" << index
             << "toolId=" << config.toolId
             << "toolType=" << toolTypeToString(config.toolType);
}

void ToolsDialog::updateToolCardSelection()
{
    for (int index = 0; index < m_toolCards.size(); ++index) {
        QFrame *card = m_toolCards.at(index);
        if (!card)
            continue;
        card->setProperty("selected", index == m_selectedToolIndex);
        refreshWidgetStyle(card);
    }
}

void ToolsDialog::showSelectedToolPreview()
{
    if (!m_previewHelper || m_selectedToolIndex < 0 || m_selectedToolIndex >= m_toolConfigs.size())
        return;

    const ToolConfig &config = m_toolConfigs.at(m_selectedToolIndex);
    const QString title = toolDisplayName(config);
    ui->viewerTitleLabel->setText(title.isEmpty() ? tr("工具预览") : title);

    const QImage image = currentReferenceImage();
    const bool hasReferenceImage = !image.isNull();
    if (hasReferenceImage)
        m_previewHelper->setImage(image);
    else
        m_previewHelper->clear();
    m_previewHelper->clearRoi();

    const ToolPreviewSnapshot snapshot = m_toolPreviewSnapshots.value(config.toolId);
    if (!snapshot.valid) {
        if (hasReferenceImage) {
            m_previewHelper->clearToolOverlays();
            ui->viewerStatusLabel->setText(tr("当前工具暂无基准图测试快照"));
        } else {
            ui->viewerStatusLabel->setText(tr("当前无基准图，且该工具暂无基准图测试快照"));
        }
        return;
    }

    if (!hasReferenceImage) {
        ui->viewerStatusLabel->setText(tr("当前无基准图，无法显示该工具基准图测试快照"));
        return;
    }

    m_previewHelper->setToolOverlays(previewOverlaysForToolsPage(config, snapshot));
    ui->viewerStatusLabel->setText(toolPreviewStatusLine(snapshot));
}

QString ToolsDialog::toolDisplayName(const ToolConfig &config) const
{
    if (!config.displayName.trimmed().isEmpty())
        return config.displayName;
    if (!config.toolName.trimmed().isEmpty())
        return config.toolName;

    switch (config.toolType) {
    case ToolType::Ocr:
        return tr("字符识别");
    case ToolType::ColorRecognition:
        return tr("颜色识别");
    case ToolType::ColorComparison:
        return tr("颜色比较");
    case ToolType::PatternPresence:
        return tr("图案有无");
    case ToolType::BlobPresence:
        return tr("斑点有无");
    case ToolType::CirclePresence:
        return tr("圆有无");
    case ToolType::EdgePresence:
        return tr("边缘有无");
    case ToolType::LinePresence:
        return tr("直线有无");
    case ToolType::ContourPresence:
        return tr("轮廓有无");
    case ToolType::AiDetection:
        return tr("目标检测");
    case ToolType::AiClassification:
        return tr("分类");
    default:
        return toolTypeToString(config.toolType);
    }
}

QString ToolsDialog::toolPreviewStateText(const ToolConfig &config) const
{
    const ToolPreviewSnapshot snapshot = m_toolPreviewSnapshots.value(config.toolId);
    if (!snapshot.valid)
        return tr("未测试");

    return snapshot.ok ? QStringLiteral("OK") : QStringLiteral("NG");
}

QString ToolsDialog::toolPreviewStatusLine(const ToolPreviewSnapshot &snapshot) const
{
    const QString state = snapshot.ok ? QStringLiteral("OK") : QStringLiteral("NG");
    const QString status = snapshot.statusText.trimmed().isEmpty()
            ? snapshot.result.status
            : snapshot.statusText;
    return tr("%1 | %2 | score:%3 | count:%4")
            .arg(status,
                 state,
                 QString::number(snapshot.score, 'f', 3),
                 QString::number(snapshot.count));
}
