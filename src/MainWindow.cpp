#include "MainWindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QEvent>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QDebug>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QSet>
#include <QSignalBlocker>
#include <QShowEvent>
#include <QSize>
#include <QSizePolicy>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>

#include "BlobPresenceDialog.h"
#include "CameraParamsDialog.h"
#include "CharacterRecognitionDialog.h"
#include "ClassificationDialog.h"
#include "CirclePresenceDialog.h"
#include "ColorComparisonDialog.h"
#include "ColorRecognitionDialog.h"
#include "ContourPresenceDialog.h"
#include "EdgePresenceDialog.h"
#include "LinePresenceDialog.h"
#include "ObjectDetectionDialog.h"
#include "PatternPresenceDialog.h"
#include "PlanDialogUtils.h"
#include "RegisteredClassificationDialog.h"
#include "RegisteredClassificationDetectionDialog.h"
#include "SchemeStore.h"
#include "ToolsDialog.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "ui_MainWindow.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

struct MainWindow::ToolChainRunOutput
{
    bool valid = false;
    bool overallOk = false;
    QString message;
    QImage frameImage;
    QVector<ToolConfig> enabledConfigs;
    QVector<ToolResult> results;
    qint64 elapsedMs = 0;
    qint64 startedWallMs = 0;
    qint64 frameCopyMs = 0;
    qint64 referenceCopyMs = 0;
    qint64 displayImageMs = 0;
    qint64 engineMs = 0;
    qint64 uiImageMs = 0;
    qint64 uiOverlayMs = 0;
    qint64 uiListMs = 0;
    qint64 uiStatsMs = 0;
    qint64 uiTotalMs = 0;
    qint64 finishIntervalMs = 0;
    qint64 triggerFrameIndex = -1;
    qint64 cameraFrameIndex = -1;
    qint64 processedFrameIndex = -1;
    QSize frameSize;
    QSize referenceSize;
    int sequence = 0;
    int continuousTick = 0;
    int submittedFrames = 0;
    int droppedFrames = 0;
    int completedFrames = 0;
    int overlayCount = 0;
    double actualDetectFps = 0.0;
    bool continuousRun = false;
};

namespace {

class ClickableSchemeRow : public QFrame
{
public:
    explicit ClickableSchemeRow(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
    }

    std::function<void()> clicked;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
            if (clicked)
                clicked();
            event->accept();
            return;
        }

        QFrame::mouseReleaseEvent(event);
    }
};

QPixmap makeSchemePlaceholderThumbnail(const QSize &size)
{
    QPixmap pixmap(size);
    pixmap.fill(QColor(74, 80, 92));

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(185, 191, 202));
    painter.setFont(QFont(QStringLiteral("Sans Serif"), 10, QFont::DemiBold));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("无基准图"));
    return pixmap;
}

QPixmap makeSchemeThumbnail(const QString &path, const QSize &size)
{
    if (!path.trimmed().isEmpty() && QFileInfo::exists(path)) {
        QPixmap pixmap(path);
        if (!pixmap.isNull())
            return pixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return makeSchemePlaceholderThumbnail(size);
}

class SchemeSelectorPopup : public QFrame
{
public:
    SchemeSelectorPopup(const QList<SchemeSummary> &schemes,
                        const QString &currentSchemeId,
                        QWidget *parent = nullptr)
        : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
    {
        setObjectName(QStringLiteral("schemeSelectorPopup"));
        setAttribute(Qt::WA_DeleteOnClose);
        setFocusPolicy(Qt::StrongFocus);

        setStyleSheet(QStringLiteral(
            "QFrame#schemeSelectorPopup {"
            "  background: #252932;"
            "  border: 1px solid #505766;"
            "  border-radius: 6px;"
            "}"
            "QLabel#schemeSelectorTitle {"
            "  color: #f4f7fb;"
            "  font-size: 16px;"
            "  font-weight: 700;"
            "}"
            "QScrollArea#schemeSelectorScroll {"
            "  border: none;"
            "  background: transparent;"
            "}"
            "QWidget#schemeSelectorList {"
            "  background: transparent;"
            "}"
            "QFrame#schemeSelectorRow {"
            "  background: #303540;"
            "  border: 1px solid #444b58;"
            "  border-radius: 5px;"
            "}"
            "QFrame#schemeSelectorRow:hover {"
            "  background: #3a414e;"
            "  border-color: #6b7485;"
            "}"
            "QFrame#schemeSelectorRow[current=\"true\"] {"
            "  background: #414855;"
            "  border-color: #ff9f2f;"
            "}"
            "QLabel#schemeThumbnail {"
            "  background: #4a505c;"
            "  border-radius: 4px;"
            "}"
            "QLabel#schemeNameLabel {"
            "  color: #ffffff;"
            "  font-size: 15px;"
            "  font-weight: 700;"
            "}"
            "QLabel#schemeMetaLabel {"
            "  color: #c8ced8;"
            "  font-size: 12px;"
            "}"
            "QLabel#schemeCurrentBadge {"
            "  color: #ffb454;"
            "  font-size: 13px;"
            "  font-weight: 700;"
            "}"
            "QPushButton#newSchemeButton {"
            "  min-height: 42px;"
            "  border-radius: 5px;"
            "  border: 1px solid #ff9f2f;"
            "  background: #ff7a00;"
            "  color: #ffffff;"
            "  font-size: 15px;"
            "  font-weight: 700;"
            "}"
            "QPushButton#newSchemeButton:hover {"
            "  background: #ff8f24;"
            "}"
        ));

        QVBoxLayout *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(12, 10, 12, 12);
        rootLayout->setSpacing(10);

        QLabel *titleLabel = new QLabel(QStringLiteral("选择方案"), this);
        titleLabel->setObjectName(QStringLiteral("schemeSelectorTitle"));
        rootLayout->addWidget(titleLabel);

        QScrollArea *scrollArea = new QScrollArea(this);
        scrollArea->setObjectName(QStringLiteral("schemeSelectorScroll"));
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setFrameShape(QFrame::NoFrame);

        QWidget *listWidget = new QWidget(scrollArea);
        listWidget->setObjectName(QStringLiteral("schemeSelectorList"));
        QVBoxLayout *listLayout = new QVBoxLayout(listWidget);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(8);

        if (schemes.isEmpty()) {
            QLabel *emptyLabel = new QLabel(QStringLiteral("暂无方案"), listWidget);
            emptyLabel->setObjectName(QStringLiteral("schemeMetaLabel"));
            emptyLabel->setAlignment(Qt::AlignCenter);
            emptyLabel->setMinimumHeight(80);
            listLayout->addWidget(emptyLabel);
        } else {
            for (const SchemeSummary &scheme : schemes)
                listLayout->addWidget(createSchemeRow(scheme, scheme.schemeId == currentSchemeId, listWidget));
        }

        listLayout->addStretch();
        scrollArea->setWidget(listWidget);
        rootLayout->addWidget(scrollArea, 1);

        QPushButton *newSchemeButton = new QPushButton(QStringLiteral("+ 新建方案"), this);
        newSchemeButton->setObjectName(QStringLiteral("newSchemeButton"));
        connect(newSchemeButton, &QPushButton::clicked, this, [this]() {
            if (createRequested)
                createRequested();
        });
        rootLayout->addWidget(newSchemeButton);
    }

    std::function<void(const QString &schemeId)> schemeSelected;
    std::function<void()> createRequested;

private:
    QWidget *createSchemeRow(const SchemeSummary &scheme, bool current, QWidget *parent)
    {
        ClickableSchemeRow *row = new ClickableSchemeRow(parent);
        row->setObjectName(QStringLiteral("schemeSelectorRow"));
        row->setProperty("current", current ? QStringLiteral("true") : QStringLiteral("false"));
        row->setMinimumHeight(84);
        row->setMaximumHeight(84);
        row->clicked = [this, scheme]() {
            if (schemeSelected)
                schemeSelected(scheme.schemeId);
        };

        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(10, 8, 10, 8);
        rowLayout->setSpacing(12);

        QLabel *thumbnailLabel = new QLabel(row);
        thumbnailLabel->setObjectName(QStringLiteral("schemeThumbnail"));
        thumbnailLabel->setFixedSize(80, 60);
        thumbnailLabel->setAlignment(Qt::AlignCenter);
        thumbnailLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        thumbnailLabel->setPixmap(makeSchemeThumbnail(scheme.referenceImagePath, thumbnailLabel->size()));
        rowLayout->addWidget(thumbnailLabel);

        QVBoxLayout *textLayout = new QVBoxLayout();
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(6);

        QLabel *nameLabel = new QLabel(scheme.schemeName.trimmed().isEmpty() ? scheme.schemeId : scheme.schemeName, row);
        nameLabel->setObjectName(QStringLiteral("schemeNameLabel"));
        nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        nameLabel->setTextInteractionFlags(Qt::NoTextInteraction);
        nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        textLayout->addWidget(nameLabel);

        const QString updatedText = scheme.updatedAt.isValid()
                ? scheme.updatedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                : QStringLiteral("--");
        QLabel *metaLabel = new QLabel(QStringLiteral("工具 %1 个  |  %2")
                                       .arg(scheme.toolCount)
                                       .arg(updatedText),
                                       row);
        metaLabel->setObjectName(QStringLiteral("schemeMetaLabel"));
        metaLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        textLayout->addWidget(metaLabel);
        rowLayout->addLayout(textLayout, 1);

        QLabel *currentBadge = new QLabel(current ? QStringLiteral("当前") : QString(), row);
        currentBadge->setObjectName(QStringLiteral("schemeCurrentBadge"));
        currentBadge->setMinimumWidth(40);
        currentBadge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        currentBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
        rowLayout->addWidget(currentBadge);

        return row;
    }
};

QImage imageFromFrame(const cv::Mat &frame)
{
    return MatImageConverter::matToDisplayImage(frame);
}

ToolPreviewSnapshot makeCameraToolPreviewSnapshot(const ToolConfig &config,
                                                  const ToolResult &result)
{
    ToolPreviewSnapshot snapshot;
    snapshot.valid = true;
    snapshot.toolId = config.toolId;
    snapshot.toolType = config.toolType;
    snapshot.sourceType = QStringLiteral("cameraFrame");
    snapshot.result = result;
    snapshot.overlays = result.overlays;
    snapshot.statusText = toolPreviewStatusText(result);
    snapshot.ok = result.ok;
    snapshot.score = result.score;
    snapshot.count = result.count;
    snapshot.payload = result.payload.toVariantMap();
    snapshot.roiNormalized = config.roiNormalized;
    snapshot.timestamp = QDateTime::currentDateTime();
    return snapshot;
}

template <typename Dialog>
bool runToolConfigDialog(QWidget *parent,
                         const ToolConfig &initialConfig,
                         ToolConfig *toolConfig,
                         ToolPreviewSnapshot *snapshot)
{
    Dialog configDialog(parent);
    configDialog.setWindowModality(Qt::WindowModal);
    configDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(&configDialog);
    configDialog.loadFromConfig(initialConfig);

    QTimer::singleShot(0, &configDialog, [&configDialog]() {
        configDialog.raise();
        configDialog.activateWindow();
    });

    if (configDialog.exec() != QDialog::Accepted)
        return false;

    if (toolConfig)
        *toolConfig = configDialog.toolConfig();
    if (snapshot)
        *snapshot = configDialog.referencePreviewSnapshot();
    return true;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_toolEngine.registerAdapter(&m_ocrAdapter);
    m_toolEngine.registerAdapter(&m_patternPresenceAdapter);
    m_toolEngine.registerAdapter(&m_blobPresenceAdapter);
    m_toolEngine.registerAdapter(&m_circlePresenceAdapter);
    m_toolEngine.registerAdapter(&m_colorComparisonAdapter);
    m_toolEngine.registerAdapter(&m_colorRecognitionAdapter);
    m_toolEngine.registerAdapter(&m_contourPresenceAdapter);
    m_toolEngine.registerAdapter(&m_edgePresenceAdapter);
    m_toolEngine.registerAdapter(&m_linePresenceAdapter);
    m_toolEngine.registerAdapter(&m_aiDetectionAdapter);
    m_toolEngine.registerAdapter(&m_registeredClassificationAdapter);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    m_toolChainWatcher = new QFutureWatcher<ToolChainRunOutput>(this);
    setupUiState();
    setupSchemeSelector();
    applyCurrentSchemeState();
    ui->planSettingButton->setStyleSheet("background:#ff7a00");
    connect(ui->clearSummaryButton, &QPushButton::clicked, this, &MainWindow::clearSummary);
    connect(ui->planSettingButton, &QPushButton::clicked, this, &MainWindow::openCameraParamsDialog);
    connect(ui->toolsSettingButton, &QToolButton::clicked, this, &MainWindow::openToolsDialog);
    connect(ui->singleRunButton, &QPushButton::clicked, this, &MainWindow::runSingleToolFlow);
    connect(ui->comboBox, QOverload<int>::of(&QComboBox::activated),
            this, &MainWindow::switchSchemeFromCombo);
    connect(ui->toolsTableWidget, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (column == 4)
            return;
        selectSchemeTool(row);
    });
    connect(m_toolChainWatcher, &QFutureWatcher<ToolChainRunOutput>::finished,
            this, &MainWindow::handleToolChainRunFinished);
    connect(ui->headerMinimizeButton, &QToolButton::clicked, this, &MainWindow::showMinimized);
    connect(ui->headerMaximizeButton, &QToolButton::clicked, this, &MainWindow::toggleWindowState);
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &MainWindow::close);
    connect(&CameraFrameProvider::instance(),
            &CameraFrameProvider::frameUpdated,
            this,
            [this](const QImage &image) {
                if (m_selectedToolIndex >= 0 || m_isToolChainRunning || m_isContinuousRunning) {
                    return;
                }
                if (m_previewHelper) {
                    m_previewHelper->setImage(image);
                }
            });
    connect(&CameraFrameProvider::instance(),
            &CameraFrameProvider::cameraError,
            this,
            [](const QString &message) {
                qWarning() << "[MainWindow]" << message;
            });
    refreshLivePreview();
    QTimer::singleShot(0, this, &MainWindow::ensureCameraRunning);
}

MainWindow::~MainWindow()
{
    stopContinuousRun();
    if (m_toolChainWatcher && m_toolChainWatcher->isRunning())
        m_toolChainWatcher->waitForFinished();
    delete ui;
}

void MainWindow::setSessionInfo(const QString &deviceName, const QString &userName)
{
    PlanDialogUtils::setSessionInfo(this, deviceName, userName);

    if (ui->headerDeviceComboBox->findText(deviceName) < 0) {
        ui->headerDeviceComboBox->insertItem(0, QIcon(QStringLiteral(":/icons/camera.svg")), deviceName);
    }
    ui->headerDeviceComboBox->setCurrentText(deviceName);// 设置顶部设备名称
    ui->headerUserButton->setText(userName);// 设置右上角用户名
}

void MainWindow::setSchemeTools(const QVector<ToolConfig> &configs,
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

    syncSnapshotMapsWithConfigs();
    refreshToolConfigTable();

    if (m_selectedToolIndex >= 0 && m_selectedToolIndex < m_schemeToolConfigs.size())
        selectSchemeTool(m_selectedToolIndex);
    else
        refreshLivePreview();

    persistCurrentSchemeState(QStringLiteral("setSchemeTools"));
    qDebug() << "[MainWindow] 当前方案工具链数量:" << m_schemeToolConfigs.size();
}

void MainWindow::updateSchemeToolsFromToolsDialog(const QVector<ToolConfig> &configs,
                                                  const QMap<QString, ToolPreviewSnapshot> &referenceSnapshots)
{
    setSchemeTools(configs, referenceSnapshots);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    ensureCameraRunning();
    refreshLivePreview();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->comboBox) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                showSchemeSelectorPopup();
                return true;
            }
        }

        if (event->type() == QEvent::MouseButtonRelease)
            return true;

        if (event->type() == QEvent::Wheel)
            return true;

        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            const int key = keyEvent->key();
            if (key == Qt::Key_Return || key == Qt::Key_Enter
                    || key == Qt::Key_Space || key == Qt::Key_Down) {
                showSchemeSelectorPopup();
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::clearSummary()
{
    m_totalInspectionCount = 0;
    m_okInspectionCount = 0;
    m_ngInspectionCount = 0;
    m_runtimeStarted = false;
    ui->okRateValueLabel->setText(QStringLiteral("0%"));
    ui->totalCountValueLabel->setText(QStringLiteral("0"));
    ui->okCountValueLabel->setText(QStringLiteral("0"));
    ui->ngCountValueLabel->setText(QStringLiteral("0"));
    ui->runtimeValueLabel->setText(QStringLiteral("0s"));
}

void MainWindow::openCameraParamsDialog()
{
    if (m_isToolChainRunning) {
        showStatusText(tr("工具链运行中，暂不能进入方案设置"));
        return;
    }

    if (m_isContinuousRunning)
        stopContinuousRun();

    CameraFrameProvider &provider = CameraFrameProvider::instance();
    qDebug() << "[SCHEME-OPEN] openCameraParamsDialog enter"
             << "mainWindow=" << this
             << "visible=" << isVisible()
             << "cameraOpened=" << provider.isOpened()
             << "grabbing=" << provider.isGrabbing()
             << "hasFrame=" << provider.hasFrame()
             << "frameIndex=" << provider.currentFrameIndex();

    persistCurrentSchemeState(QStringLiteral("openCameraParamsDialog"));
    CameraParamsDialog *dialog = new CameraParamsDialog(this);// 打开相机参数窗口
    PlanDialogUtils::setSessionInfo(dialog,
                                    ui->headerDeviceComboBox->currentText(),
                                    ui->headerUserButton->text());
    PlanDialogUtils::showDialogFromWidget(this, dialog);

    qDebug() << "[SCHEME-OPEN] CameraParamsDialog shown; MainWindow kept alive"
             << "mainWindow=" << this
             << "cameraOpened=" << provider.isOpened()
             << "grabbing=" << provider.isGrabbing()
             << "hasFrame=" << provider.hasFrame()
             << "frameIndex=" << provider.currentFrameIndex();
}

void MainWindow::openToolsDialog()
{
    if (m_isToolChainRunning) {
        showStatusText(tr("工具链运行中，暂不能进入工具页面"));
        return;
    }

    if (m_isContinuousRunning)
        stopContinuousRun();

    applyCurrentSchemeState();
    ToolsDialog dialog(this);
    dialog.setAttribute(Qt::WA_DeleteOnClose, false);
    dialog.setInitialToolState(m_schemeToolConfigs, m_referencePreviewSnapshots);
    PlanDialogUtils::setSessionInfo(&dialog,
                                    ui->headerDeviceComboBox->currentText(),
                                    ui->headerUserButton->text());

    dialog.exec();

    updateSchemeToolsFromToolsDialog(dialog.toolConfigs(), dialog.referencePreviewSnapshots());

    qDebug() << "[MainWindow] 已同步工具配置数量:" << m_schemeToolConfigs.size();
    for (const ToolConfig &config : m_schemeToolConfigs) {
        qDebug() << "[MainWindow] ToolConfig"
                 << config.toolId
                 << toolTypeToString(config.toolType)
                 << config.summary;
    }

    if (!dialog.openedOutputDialog()) {
        raise();
        activateWindow();
    }
}

void MainWindow::runSingleToolFlow()
{
    if (m_isContinuousRunning) {
        showStatusText(tr("请先停止连续运行，再执行单次运行"));
        return;
    }

    runCurrentToolChainOnce();
}

void MainWindow::toggleWindowState()
{
    isMaximized() ? showNormal() : showMaximized();
}

void MainWindow::setupUiState()
{
    PlanDialogUtils::applyLargeWindow(this);
    ui->headerDeviceComboBox->addItem(QIcon(QStringLiteral(":/icons/camera.svg")),QStringLiteral("MV-SCA002C-03S-WBN-NR (DA9880297)"));
    ui->headerDeviceComboBox->addItem(QIcon(QStringLiteral(":/icons/camera.svg")),QStringLiteral("MV-SCA002C-03S-WBN-NR (DA9880285)"));
    ui->headerDeviceComboBox->addItem(QIcon(QStringLiteral(":/icons/camera.svg")),QStringLiteral("MV-SCA002C-03S-WBN-NR (DA9880272)"));

    ui->toolResultButton->setChecked(true);
    ui->resultResultButton->setChecked(false);

    ui->toolsTableWidget->horizontalHeader()->setVisible(false);
    ui->toolsTableWidget->verticalHeader()->setVisible(false);
    ui->toolsTableWidget->setColumnCount(5);
    ui->toolsTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->toolsTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->toolsTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->toolsTableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->toolsTableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->toolsTableWidget->verticalHeader()->setDefaultSectionSize(46);
    ui->toolsTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->toolsTableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->toolsTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->toolsTableWidget->setFocusPolicy(Qt::NoFocus);
    ui->toolsTableWidget->setShowGrid(false);
    ui->toolsTableWidget->setWordWrap(false);
    ui->toolsTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    refreshToolConfigTable();
    updateContinuousRunUi();
}

void MainWindow::setupSchemeSelector()
{
    ui->comboBox->installEventFilter(this);
    ui->comboBox->setEditable(false);
    ui->comboBox->setInsertPolicy(QComboBox::NoInsert);
    ui->comboBox->setMinimumWidth(170);
    ui->comboBox->setToolTip(tr("点击选择或新建方案"));

    QString error;
    if (!SchemeStore::instance().ensureLoaded(&error)) {
        qWarning() << "[MainWindow] 方案加载失败:" << error;
        showStatusText(tr("方案加载失败: %1").arg(error));
        return;
    }

    refreshSchemeSelector();
}

void MainWindow::refreshSchemeSelector()
{
    QSignalBlocker blocker(ui->comboBox);
    ui->comboBox->clear();

    const QVector<SchemeState> schemes = SchemeStore::instance().availableSchemes();
    const QString currentId = SchemeStore::instance().currentScheme().schemeId;
    int currentIndex = -1;
    for (int index = 0; index < schemes.size(); ++index) {
        const SchemeState &scheme = schemes.at(index);
        const QString name = scheme.schemeName.trimmed().isEmpty() ? scheme.schemeId : scheme.schemeName;
        ui->comboBox->addItem(name, scheme.schemeId);
        if (scheme.schemeId == currentId)
            currentIndex = index;
    }

    if (currentIndex >= 0)
        ui->comboBox->setCurrentIndex(currentIndex);
}

void MainWindow::showSchemeSelectorPopup()
{
    QString error;
    SchemeStore &store = SchemeStore::instance();
    if (!store.ensureLoaded(&error)) {
        qWarning() << "[MainWindow] 方案列表加载失败:" << error;
        showStatusText(tr("方案列表加载失败: %1").arg(error));
        return;
    }

    const QList<SchemeSummary> schemes = store.listSchemes();
    const QString currentId = store.currentScheme().schemeId;

    SchemeSelectorPopup *popup = new SchemeSelectorPopup(schemes, currentId, this);
    popup->schemeSelected = [this, popup](const QString &schemeId) {
        popup->close();
        switchSchemeById(schemeId);
    };
    popup->createRequested = [this, popup]() {
        popup->close();
        createAndSwitchToNewScheme();
    };

    const int popupWidth = qMax(ui->comboBox->width(), 460);
    const int visibleRows = qBound(1, schemes.size(), 5);
    const int popupHeight = qBound(220, 70 + visibleRows * 92 + 64, 560);
    popup->resize(popupWidth, popupHeight);

    QPoint pos = ui->comboBox->mapToGlobal(QPoint(0, ui->comboBox->height() + 6));
    QScreen *screen = QApplication::screenAt(pos);
    const QRect availableGeometry = screen ? screen->availableGeometry() : QApplication::primaryScreen()->availableGeometry();

    if (pos.x() + popupWidth > availableGeometry.right())
        pos.setX(qMax(availableGeometry.left(), availableGeometry.right() - popupWidth));
    if (pos.y() + popupHeight > availableGeometry.bottom()) {
        const int aboveY = ui->comboBox->mapToGlobal(QPoint(0, -popupHeight - 6)).y();
        pos.setY(qMax(availableGeometry.top(), aboveY));
    }

    popup->move(pos);
    popup->show();
    popup->raise();
    popup->activateWindow();
}

void MainWindow::createAndSwitchToNewScheme()
{
    if (m_isContinuousRunning)
        stopContinuousRun();

    if (m_isToolChainRunning) {
        showStatusText(tr("工具链运行中，暂不能新建方案"));
        return;
    }

    const QString defaultName = QStringLiteral("新方案_%1")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    bool accepted = false;
    const QString schemeName = QInputDialog::getText(this,
                                                     tr("新建方案"),
                                                     tr("请输入方案名称"),
                                                     QLineEdit::Normal,
                                                     defaultName,
                                                     &accepted);
    if (!accepted)
        return;

    const QString trimmedName = schemeName.trimmed();
    if (trimmedName.isEmpty()) {
        showStatusText(tr("方案名称不能为空"));
        return;
    }

    QString error;
    SchemeState created = SchemeStore::instance().createEmptyScheme(trimmedName, &error);
    if (created.schemeId.isEmpty()) {
        qWarning() << "[MainWindow] 新建方案失败:" << error;
        showStatusText(tr("新建方案失败: %1").arg(error));
        refreshSchemeSelector();
        return;
    }

    switchSchemeById(created.schemeId);
}

bool MainWindow::switchSchemeById(const QString &schemeId)
{
    const QString trimmedId = schemeId.trimmed();
    if (trimmedId.isEmpty())
        return false;

    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        qWarning() << "[MainWindow] 方案切换前初始化失败:" << error;
        showStatusText(tr("方案加载失败: %1").arg(error));
        refreshSchemeSelector();
        return false;
    }

    if (store.currentScheme().schemeId == trimmedId) {
        refreshSchemeSelector();
        return true;
    }

    if (m_isContinuousRunning)
        stopContinuousRun();

    if (m_isToolChainRunning) {
        showStatusText(tr("工具链运行中，暂不能切换方案"));
        refreshSchemeSelector();
        return false;
    }

    if (!persistCurrentSchemeState(QStringLiteral("switchSchemeById"))) {
        refreshSchemeSelector();
        return false;
    }

    if (!store.setCurrentScheme(trimmedId, &error)) {
        qWarning() << "[MainWindow] 切换方案失败:" << error;
        showStatusText(tr("切换方案失败: %1").arg(error));
        refreshSchemeSelector();
        return false;
    }

    applyCurrentSchemeState();
    clearSummary();
    showStatusText(tr("已切换到方案：%1").arg(store.currentSchemeName()));
    return true;
}

void MainWindow::applyCurrentSchemeState()
{
    QString error;
    if (!SchemeStore::instance().ensureLoaded(&error)) {
        qWarning() << "[MainWindow] 方案状态加载失败:" << error;
        showStatusText(tr("方案加载失败: %1").arg(error));
        return;
    }

    const SchemeState &scheme = SchemeStore::instance().currentScheme();
    m_schemeToolConfigs = scheme.toolConfigs;
    m_referencePreviewSnapshots = scheme.referencePreviewSnapshots;
    m_lastRunSnapshots.clear();
    m_lastRunImage = QImage();
    m_selectedToolIndex = -1;
    syncSnapshotMapsWithConfigs();
    refreshToolConfigTable();
    refreshSchemeSelector();

    QString referenceError;
    if (!SchemeStore::instance().loadCurrentReferenceIntoProvider(&referenceError)
            && !scheme.referenceImagePath.isEmpty()) {
        qWarning() << "[MainWindow]" << referenceError;
        showStatusText(tr("当前方案基准图加载失败"));
    }
}

bool MainWindow::persistCurrentSchemeState(const QString &context)
{
    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        qWarning() << "[MainWindow]" << context << "方案状态初始化失败:" << error;
        showStatusText(tr("方案状态初始化失败"));
        return false;
    }

    store.setToolConfigs(m_schemeToolConfigs, m_referencePreviewSnapshots);
    if (!store.saveCurrentScheme(&error)) {
        qWarning() << "[MainWindow]" << context << "方案保存失败:" << error;
        showStatusText(tr("方案保存失败: %1").arg(error));
        return false;
    }

    refreshSchemeSelector();
    return true;
}

void MainWindow::ensureCameraRunning()
{
    CameraFrameProvider &provider = CameraFrameProvider::instance();

    if (!provider.isOpened() && !provider.openCamera(QStringLiteral("/dev/video0"))) {
        qDebug() << "[MainWindow] 当前无图像：相机打开失败。";
        refreshLivePreview();
        return;
    }

    if (!provider.isGrabbing() && !provider.startGrab()) {
        qDebug() << "[MainWindow] 当前无图像：相机采集启动失败。";
        refreshLivePreview();
        return;
    }
}

void MainWindow::refreshLivePreview()
{
    if (!m_previewHelper) {
        return;
    }

    const QImage image = CameraFrameProvider::instance().currentImage();
    if (image.isNull()) {
        if (m_previewHelper->hasImage()) {
            showStatusText(tr("当前图像暂不可用，保留上一帧"));
            return;
        }
        m_previewHelper->clear();
        qDebug() << "[MainWindow] 当前无图像。";
        return;
    }

    m_previewHelper->setImage(image);
}

void MainWindow::refreshToolConfigTable()
{
    ui->toolsTableWidget->setUpdatesEnabled(false);
    ui->toolsTableWidget->clearContents();
    ui->toolsTableWidget->setRowCount(0);

    if (m_schemeToolConfigs.isEmpty()) {
        m_selectedToolIndex = -1;
        ui->toolsTableWidget->setRowCount(1);
        QTableWidgetItem *emptyItem = new QTableWidgetItem(tr("暂无工具"));
        emptyItem->setTextAlignment(Qt::AlignCenter);
        ui->toolsTableWidget->setItem(0, 0, emptyItem);
        ui->toolsTableWidget->setSpan(0, 0, 1, 5);
        ui->toolsTableWidget->setUpdatesEnabled(true);
        return;
    }

    ui->toolsTableWidget->clearSpans();
    ui->toolsTableWidget->setRowCount(m_schemeToolConfigs.size());
    for (int row = 0; row < m_schemeToolConfigs.size(); ++row)
        updateToolResultRow(row);

    ui->toolsTableWidget->setUpdatesEnabled(true);

    if (m_selectedToolIndex >= 0 && m_selectedToolIndex < m_schemeToolConfigs.size())
        ui->toolsTableWidget->selectRow(m_selectedToolIndex);
}

void MainWindow::updateToolResultRow(int row)
{
    if (row < 0 || row >= m_schemeToolConfigs.size())
        return;

    const ToolConfig &config = m_schemeToolConfigs.at(row);

    QTableWidgetItem *indexItem = new QTableWidgetItem(QStringLiteral("#%1").arg(row + 1));
    indexItem->setData(Qt::UserRole, config.toolId);
    indexItem->setTextAlignment(Qt::AlignCenter);

    QTableWidgetItem *enabledItem = new QTableWidgetItem(config.enabled ? tr("启用") : tr("停用"));
    enabledItem->setTextAlignment(Qt::AlignCenter);

    QTableWidgetItem *nameItem = new QTableWidgetItem(toolDisplayName(config));
    nameItem->setData(Qt::UserRole, config.toolId);
    nameItem->setToolTip(config.summary.trimmed().isEmpty() ? toolDisplayName(config) : config.summary);

    QTableWidgetItem *stateItem = new QTableWidgetItem(toolRunStateText(config));
    stateItem->setTextAlignment(Qt::AlignCenter);

    ui->toolsTableWidget->setItem(row, 0, indexItem);
    ui->toolsTableWidget->setItem(row, 1, enabledItem);
    ui->toolsTableWidget->setItem(row, 2, nameItem);
    ui->toolsTableWidget->setItem(row, 3, stateItem);

    QToolButton *gearButton = new QToolButton(ui->toolsTableWidget);
    gearButton->setObjectName(QStringLiteral("toolGearButton"));
    gearButton->setProperty("role", QStringLiteral("toolGear"));
    gearButton->setIcon(QIcon(QStringLiteral(":/icons/settings.svg")));
    gearButton->setIconSize(QSize(18, 18));
    gearButton->setToolTip(tr("编辑工具参数"));
    gearButton->setAutoRaise(true);
    connect(gearButton, &QToolButton::clicked, this, [this, row]() {
        editSchemeTool(row);
    });
    ui->toolsTableWidget->setCellWidget(row, 4, gearButton);
    ui->toolsTableWidget->setRowHeight(row, 46);
}

void MainWindow::refreshToolRunStateRows()
{
    if (m_schemeToolConfigs.isEmpty() ||
        ui->toolsTableWidget->rowCount() != m_schemeToolConfigs.size()) {
        refreshToolConfigTable();
        return;
    }

    QSignalBlocker blocker(ui->toolsTableWidget);
    ui->toolsTableWidget->setUpdatesEnabled(false);

    for (int row = 0; row < m_schemeToolConfigs.size(); ++row) {
        const ToolConfig &config = m_schemeToolConfigs.at(row);

        QTableWidgetItem *enabledItem = ui->toolsTableWidget->item(row, 1);
        if (enabledItem)
            enabledItem->setText(config.enabled ? tr("启用") : tr("停用"));

        QTableWidgetItem *stateItem = ui->toolsTableWidget->item(row, 3);
        if (!stateItem) {
            stateItem = new QTableWidgetItem();
            stateItem->setTextAlignment(Qt::AlignCenter);
            ui->toolsTableWidget->setItem(row, 3, stateItem);
        }
        stateItem->setText(toolRunStateText(config));
    }

    ui->toolsTableWidget->setUpdatesEnabled(true);

    if (m_selectedToolIndex >= 0 && m_selectedToolIndex < m_schemeToolConfigs.size())
        ui->toolsTableWidget->selectRow(m_selectedToolIndex);
}

void MainWindow::on_stopRunButton_clicked()
{
    if (m_isContinuousRunning)
        stopContinuousRun();
    else
        startContinuousRun();
}

bool MainWindow::runCurrentToolChainOnce()
{
    if (m_isContinuousRunning) {
        showStatusText(tr("请先停止连续运行，再执行单次运行"));
        return false;
    }

    return submitToolChainRun(false, CameraFrameProvider::instance().currentFrameIndex());
}

void MainWindow::onCameraFrameUpdated(qint64 frameIndex)
{
    if (!m_isContinuousRunning)
        return;

    tryRunToolChainOnLatestFrame(frameIndex);
}

bool MainWindow::tryRunToolChainOnLatestFrame(qint64 frameIndex)
{
    if (!m_isContinuousRunning)
        return false;

    if (frameIndex > m_lastSeenFrameIndex)
        m_lastSeenFrameIndex = frameIndex;

    if (m_isToolChainRunning) {
        if (frameIndex > m_lastProcessedFrameIndex)
            ++m_continuousDroppedFrames;
        return false;
    }

    return submitToolChainRun(true, frameIndex);
}

bool MainWindow::submitToolChainRun(bool continuousRun, qint64 triggerFrameIndex)
{
    if (m_isToolChainRunning) {
        if (continuousRun)
            ++m_continuousDroppedFrames;
        else
            qDebug() << "[MainWindow] 工具链正在运行，本轮触发跳过。";
        return false;
    }

    if (m_schemeToolConfigs.isEmpty()) {
        showStatusText(tr("当前没有工具配置"));
        refreshToolConfigTable();
        return false;
    }

    QVector<ToolConfig> enabledConfigs;
    enabledConfigs.reserve(m_schemeToolConfigs.size());
    for (const ToolConfig &config : m_schemeToolConfigs) {
        if (config.enabled)
            enabledConfigs.append(config);
    }

    if (enabledConfigs.isEmpty()) {
        showStatusText(tr("无启用工具"));
        refreshToolConfigTable();
        return false;
    }

    if (!ReferenceImageProvider::instance().hasReferenceFrame()) {
        QString referenceLoadError;
        if (!SchemeStore::instance().loadCurrentReferenceIntoProvider(&referenceLoadError)
                && !SchemeStore::instance().currentScheme().referenceImagePath.isEmpty()) {
            qWarning() << "[MainWindow] 运行前加载当前方案基准图失败:" << referenceLoadError;
        }
    }

    const qint64 startedWallMs = QDateTime::currentMSecsSinceEpoch();

    QElapsedTimer frameCopyTimer;
    frameCopyTimer.start();
    qint64 actualFrameIndex = -1;
    const cv::Mat image = CameraFrameProvider::instance().currentFrame(&actualFrameIndex);
    const qint64 frameCopyMs = frameCopyTimer.elapsed();

    if (continuousRun) {
        if (actualFrameIndex > m_lastSeenFrameIndex)
            m_lastSeenFrameIndex = actualFrameIndex;
        if (actualFrameIndex <= m_lastProcessedFrameIndex)
            return false;
    }

    if (image.empty()) {
        if (continuousRun && actualFrameIndex > m_lastProcessedFrameIndex)
            m_lastProcessedFrameIndex = actualFrameIndex;
        showStatusText(tr("当前图像为空或无新帧，无法运行"));
        refreshLivePreview();
        return false;
    }

    QElapsedTimer referenceCopyTimer;
    referenceCopyTimer.start();
    const cv::Mat referenceImage = ReferenceImageProvider::instance().referenceFrame();
    const qint64 referenceCopyMs = referenceCopyTimer.elapsed();

    QJsonObject runtimeContext;
    runtimeContext.insert(QStringLiteral("input"),
                          CameraFrameProvider::instance().currentFrameMetadata().toJson());
    runtimeContext.insert(QStringLiteral("referenceInput"),
                          ReferenceImageProvider::instance().referenceFrameMetadata().toJson());

    QElapsedTimer displayImageTimer;
    displayImageTimer.start();
    const QImage displayImage = imageFromFrame(image);
    const qint64 displayImageMs = displayImageTimer.elapsed();
    const QSize frameSize(image.cols, image.rows);
    const QSize referenceSize(referenceImage.empty()
                              ? QSize()
                              : QSize(referenceImage.cols, referenceImage.rows));
    const int runSequence = ++m_toolChainRunSequence;
    ToolEngine *engine = &m_toolEngine;

    if (continuousRun) {
        m_lastProcessedFrameIndex = actualFrameIndex;
        ++m_continuousSubmittedFrames;
    }

    m_isToolChainRunning = true;
    updateContinuousRunUi();

    auto future = QtConcurrent::run([engine,
                                     enabledConfigs,
                                     image,
                                     referenceImage,
                                     runtimeContext,
                                     displayImage,
                                     startedWallMs,
                                     frameCopyMs,
                                     referenceCopyMs,
                                     displayImageMs,
                                     frameSize,
                                     referenceSize,
                                     runSequence,
                                     continuousRun,
                                     triggerFrameIndex,
                                     actualFrameIndex]() {
        ToolChainRunOutput output;
        output.valid = true;
        output.frameImage = displayImage;
        output.enabledConfigs = enabledConfigs;
        output.startedWallMs = startedWallMs;
        output.frameCopyMs = frameCopyMs;
        output.referenceCopyMs = referenceCopyMs;
        output.displayImageMs = displayImageMs;
        output.frameSize = frameSize;
        output.referenceSize = referenceSize;
        output.sequence = runSequence;
        output.continuousRun = continuousRun;
        output.triggerFrameIndex = triggerFrameIndex;
        output.cameraFrameIndex = actualFrameIndex;
        output.processedFrameIndex = actualFrameIndex;

        QElapsedTimer timer;
        timer.start();
        output.results = engine->runTools(enabledConfigs,
                                          image,
                                          referenceImage,
                                          runtimeContext);
        output.engineMs = timer.elapsed();

        output.overallOk = !output.results.isEmpty();
        for (const ToolResult &result : output.results) {
            output.overlayCount += result.overlays.size();
            if (!result.success || !result.ok)
                output.overallOk = false;
        }

        output.message = output.overallOk ? QStringLiteral("OK") : QStringLiteral("NG");
        return output;
    });

    m_toolChainWatcher->setFuture(future);
    return true;
}

void MainWindow::handleToolChainRunFinished()
{
    if (!m_toolChainWatcher)
        return;

    ToolChainRunOutput output = m_toolChainWatcher->result();
    const qint64 finishedWallMs = QDateTime::currentMSecsSinceEpoch();
    if (output.startedWallMs > 0)
        output.elapsedMs = finishedWallMs - output.startedWallMs;
    if (output.continuousRun) {
        output.finishIntervalMs = m_lastContinuousRunFinishWallMs > 0
                ? finishedWallMs - m_lastContinuousRunFinishWallMs
                : 0;
        m_lastContinuousRunFinishWallMs = finishedWallMs;
        output.completedFrames = ++m_continuousCompletedRuns;
        output.continuousTick = output.completedFrames;
        output.submittedFrames = m_continuousSubmittedFrames;
        output.droppedFrames = m_continuousDroppedFrames;
    }
    m_isToolChainRunning = false;
    applyToolChainRunResult(output);

    if (m_isContinuousRunning) {
        const qint64 latestFrameIndex = CameraFrameProvider::instance().currentFrameIndex();
        if (latestFrameIndex > m_lastProcessedFrameIndex)
            tryRunToolChainOnLatestFrame(latestFrameIndex);
    }

    updateContinuousRunUi();
}

void MainWindow::applyToolChainRunResult(ToolChainRunOutput output)
{
    if (!output.valid) {
        showStatusText(tr("工具链运行失败"));
        return;
    }

    QElapsedTimer uiTimer;
    uiTimer.start();

    storeLastRunSnapshots(output.enabledConfigs, output.results);
    m_lastRunImage = output.frameImage;

    if (!m_lastRunImage.isNull()) {
        QElapsedTimer imageTimer;
        imageTimer.start();
        m_previewHelper->setImage(m_lastRunImage);
        output.uiImageMs = imageTimer.elapsed();

        QElapsedTimer overlayTimer;
        overlayTimer.start();
        showAllLastRunOverlays();
        output.uiOverlayMs = overlayTimer.elapsed();
    }

    bool overallOk = !output.results.isEmpty();
    for (const ToolResult &result : output.results) {
        if (!output.continuousRun) {
            qDebug() << "[MainWindow] ToolResult"
                     << "toolId=" << result.toolId
                     << "type=" << toolTypeToString(result.toolType)
                     << "success=" << result.success
                     << "ok=" << result.ok
                     << "status=" << result.status
                     << "message=" << result.message
                     << "score=" << result.score
                     << "count=" << result.count
                     << "elapsedMs=" << result.elapsedMs;
        }
        if (!result.success || !result.ok)
            overallOk = false;
    }

    QElapsedTimer statsTimer;
    statsTimer.start();
    ++m_totalInspectionCount;
    if (overallOk)
        ++m_okInspectionCount;
    else
        ++m_ngInspectionCount;

    if (!m_runtimeStarted) {
        m_runtimeStarted = true;
        m_runtimeTimer.start();
    }

    const int okRate = m_totalInspectionCount > 0
            ? static_cast<int>((100.0 * m_okInspectionCount / m_totalInspectionCount) + 0.5)
            : 0;
    ui->okRateValueLabel->setText(QStringLiteral("%1%").arg(okRate));
    ui->totalCountValueLabel->setText(QString::number(m_totalInspectionCount));
    ui->okCountValueLabel->setText(QString::number(m_okInspectionCount));
    ui->ngCountValueLabel->setText(QString::number(m_ngInspectionCount));
    ui->runtimeValueLabel->setText(QStringLiteral("%1s").arg(m_runtimeTimer.elapsed() / 1000));
    showRunResultStatus(output, overallOk);
    output.uiStatsMs = statsTimer.elapsed();

    QElapsedTimer listTimer;
    listTimer.start();
    refreshToolRunStateRows();
    output.uiListMs = listTimer.elapsed();
    output.uiTotalMs = uiTimer.elapsed();

    if (!output.continuousRun) {
        logToolChainRunPerformance(output, true);
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (m_continuousPerfWindowStartWallMs <= 0)
        m_continuousPerfWindowStartWallMs = nowMs;

    ++m_continuousPerfWindowCompleted;
    m_continuousPerfEngineMs += output.engineMs;
    m_continuousPerfTotalMs += output.elapsedMs;
    m_continuousPerfUiMs += output.uiTotalMs;
    m_continuousPerfFrameCopyMs += output.frameCopyMs;
    m_continuousPerfRefCopyMs += output.referenceCopyMs;

    const qint64 windowMs = qMax<qint64>(1, nowMs - m_continuousPerfWindowStartWallMs);
    output.actualDetectFps = (1000.0 * m_continuousPerfWindowCompleted) / windowMs;
    output.submittedFrames = m_continuousSubmittedFrames;
    output.droppedFrames = m_continuousDroppedFrames;
    output.completedFrames = m_continuousCompletedRuns;

    const bool enoughFrames = m_continuousPerfWindowCompleted >= 30;
    const bool enoughTime = (nowMs - m_lastContinuousPerfLogWallMs) >= 2000;
    if (enoughFrames || enoughTime) {
        logToolChainRunPerformance(output, false);
        m_lastContinuousPerfLogWallMs = nowMs;
        m_continuousPerfWindowStartWallMs = nowMs;
        m_continuousPerfWindowCompleted = 0;
        m_continuousPerfEngineMs = 0;
        m_continuousPerfTotalMs = 0;
        m_continuousPerfUiMs = 0;
        m_continuousPerfFrameCopyMs = 0;
        m_continuousPerfRefCopyMs = 0;
    }
}

void MainWindow::startContinuousRun()
{
    if (m_isContinuousRunning)
        return;

    if (m_isToolChainRunning) {
        showStatusText(tr("工具链运行中，暂不能启动连续运行"));
        return;
    }

    if (m_schemeToolConfigs.isEmpty()) {
        showStatusText(tr("当前没有工具配置"));
        refreshToolConfigTable();
        return;
    }

    bool hasEnabledTool = false;
    for (const ToolConfig &config : m_schemeToolConfigs) {
        if (config.enabled) {
            hasEnabledTool = true;
            break;
        }
    }
    if (!hasEnabledTool) {
        showStatusText(tr("无启用工具"));
        refreshToolConfigTable();
        return;
    }

    resetContinuousRunStats();
    m_isContinuousRunning = true;

    if (!m_continuousFrameConnection) {
        m_continuousFrameConnection = connect(&CameraFrameProvider::instance(),
                                              &CameraFrameProvider::frameIndexChanged,
                                              this,
                                              &MainWindow::onCameraFrameUpdated,
                                              Qt::QueuedConnection);
    }

    updateContinuousRunUi();
    tryRunToolChainOnLatestFrame(CameraFrameProvider::instance().currentFrameIndex());
}

void MainWindow::stopContinuousRun()
{
    m_isContinuousRunning = false;
    if (m_continuousFrameConnection) {
        QObject::disconnect(m_continuousFrameConnection);
        m_continuousFrameConnection = QMetaObject::Connection();
    }
    m_lastContinuousRunFinishWallMs = 0;
    updateContinuousRunUi();
}

void MainWindow::resetContinuousRunStats()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    m_continuousCompletedRuns = 0;
    m_continuousSubmittedFrames = 0;
    m_continuousDroppedFrames = 0;
    m_lastProcessedFrameIndex = -1;
    m_lastSeenFrameIndex = CameraFrameProvider::instance().currentFrameIndex();
    m_continuousRunStartWallMs = nowMs;
    m_lastContinuousRunFinishWallMs = 0;
    m_lastContinuousPerfLogWallMs = nowMs;
    m_continuousPerfWindowStartWallMs = nowMs;
    m_continuousPerfWindowCompleted = 0;
    m_continuousPerfEngineMs = 0;
    m_continuousPerfTotalMs = 0;
    m_continuousPerfUiMs = 0;
    m_continuousPerfFrameCopyMs = 0;
    m_continuousPerfRefCopyMs = 0;
}

void MainWindow::updateContinuousRunUi()
{
    if (m_isContinuousRunning) {
        ui->stopRunButton->setText(tr("停止运行"));
        ui->stopRunButton->setStyleSheet(QStringLiteral("background:#ff7a00"));
        ui->stopRunButton->setIcon(QIcon(QStringLiteral(":/icons/stop.svg")));
        ui->planSettingButton->setStyleSheet(QStringLiteral("background:#8d94a1"));
        ui->singleRunButton->setStyleSheet(QStringLiteral("background:#555b67"));
        ui->singleRunButton->setEnabled(false);
        return;
    }

    ui->stopRunButton->setText(tr("连续运行"));
    ui->stopRunButton->setStyleSheet(QStringLiteral("background:#8d94a1"));
    ui->stopRunButton->setIcon(QIcon(QStringLiteral(":/icons/refresh.svg")));
    ui->planSettingButton->setStyleSheet(QStringLiteral("background:#ff7a00"));
    ui->singleRunButton->setStyleSheet(QStringLiteral("background:#8d94a1"));
    ui->singleRunButton->setEnabled(!m_isToolChainRunning);
}

void MainWindow::showStatusText(const QString &text)
{
    ui->totalDurationLabel->setText(text);
}

void MainWindow::showRunResultStatus(const ToolChainRunOutput &output, bool overallOk)
{
    const QString resultText = overallOk ? QStringLiteral("OK") : QStringLiteral("NG");
    if (output.continuousRun) {
        ui->totalDurationLabel->setText(tr("本次运行: %1 | 总耗时:%2ms | 算法耗时:%3ms | dropped:%4")
                                        .arg(resultText)
                                        .arg(output.elapsedMs)
                                        .arg(output.engineMs)
                                        .arg(output.droppedFrames));
    } else {
        ui->totalDurationLabel->setText(tr("本次运行: %1 | 总耗时:%2ms")
                                        .arg(resultText)
                                        .arg(output.elapsedMs));
    }
    ui->captureDurationLabel->setText(tr("出图耗时: --ms"));
    ui->algorithmDurationLabel->setText(tr("算法耗时: %1ms").arg(output.engineMs));
    ui->toolDurationLabel->setText(tr("工具耗时: %1ms").arg(output.engineMs));
    ui->templateDurationLabel->setText(tr("基准图耗时:%1ms").arg(output.referenceCopyMs));
}

void MainWindow::logToolChainRunPerformance(const ToolChainRunOutput &output, bool forceLog)
{
    const QString frameSizeText = output.frameSize.isEmpty()
            ? QStringLiteral("--")
            : QStringLiteral("%1x%2").arg(output.frameSize.width()).arg(output.frameSize.height());
    const QString referenceSizeText = output.referenceSize.isEmpty()
            ? QStringLiteral("--")
            : QStringLiteral("%1x%2").arg(output.referenceSize.width()).arg(output.referenceSize.height());

    if (output.continuousRun) {
        const int sampleCount = qMax(1, m_continuousPerfWindowCompleted);
        const qint64 engineAvg = m_continuousPerfEngineMs / sampleCount;
        const qint64 totalAvg = m_continuousPerfTotalMs / sampleCount;
        const qint64 uiAvg = m_continuousPerfUiMs / sampleCount;
        const qint64 frameCopyAvg = m_continuousPerfFrameCopyMs / sampleCount;
        const qint64 refCopyAvg = m_continuousPerfRefCopyMs / sampleCount;

        qInfo().noquote()
                << QStringLiteral("[RUN-PERF] mode=continuous-frame-driven completed=%1 submitted=%2 dropped=%3 actualDetectFps=%4 cameraFrameIndex=%5 processedFrameIndex=%6 engineAvg=%7ms totalAvg=%8ms uiAvg=%9ms frameCopyAvg=%10ms refCopyAvg=%11ms overlayCount=%12 tools=%13 frame=%14")
                   .arg(output.completedFrames)
                   .arg(output.submittedFrames)
                   .arg(output.droppedFrames)
                   .arg(QString::number(output.actualDetectFps, 'f', 1))
                   .arg(output.cameraFrameIndex)
                   .arg(output.processedFrameIndex)
                   .arg(engineAvg)
                   .arg(totalAvg)
                   .arg(uiAvg)
                   .arg(frameCopyAvg)
                   .arg(refCopyAvg)
                   .arg(output.overlayCount)
                   .arg(output.enabledConfigs.size())
                   .arg(frameSizeText);
        return;
    }

    if (!forceLog)
        return;

    qInfo().noquote()
            << QStringLiteral("[RUN-PERF] mode=single sequence=%1 cameraFrameIndex=%2 processedFrameIndex=%3 frameCopy=%4ms frame=%5 refCopy=%6ms ref=%7 displayImage=%8ms engine=%9ms ui=%10ms imageUi=%11ms overlayUi=%12ms list=%13ms stats=%14ms overlays=%15 total=%16ms tools=%17")
               .arg(output.sequence)
               .arg(output.cameraFrameIndex)
               .arg(output.processedFrameIndex)
               .arg(output.frameCopyMs)
               .arg(frameSizeText)
               .arg(output.referenceCopyMs)
               .arg(referenceSizeText)
               .arg(output.displayImageMs)
               .arg(output.engineMs)
               .arg(output.uiTotalMs)
               .arg(output.uiImageMs)
               .arg(output.uiOverlayMs)
               .arg(output.uiListMs)
               .arg(output.uiStatsMs)
               .arg(output.overlayCount)
               .arg(output.elapsedMs)
               .arg(output.enabledConfigs.size());
}

void MainWindow::selectSchemeTool(int row)
{
    if (row < 0 || row >= m_schemeToolConfigs.size())
        return;

    m_selectedToolIndex = row;
    ui->toolsTableWidget->selectRow(row);

    const ToolConfig &config = m_schemeToolConfigs.at(row);
    const ToolPreviewSnapshot lastRunSnapshot = m_lastRunSnapshots.value(config.toolId);
    if (lastRunSnapshot.valid) {
        showToolSnapshot(row, lastRunSnapshot, m_lastRunImage, tr("最近一次主界面运行结果"));
        return;
    }

    const ToolPreviewSnapshot referenceSnapshot = m_referencePreviewSnapshots.value(config.toolId);
    if (referenceSnapshot.valid) {
        showToolSnapshot(row,
                         referenceSnapshot,
                         ReferenceImageProvider::instance().referenceImage(),
                         tr("基准图测试快照"));
        return;
    }

    const QImage currentImage = CameraFrameProvider::instance().currentImage();
    if (!currentImage.isNull()) {
        m_previewHelper->setImage(currentImage);
        m_previewHelper->clearToolOverlays();
    } else {
        const QImage referenceImage = ReferenceImageProvider::instance().referenceImage();
        if (!referenceImage.isNull()) {
            m_previewHelper->setImage(referenceImage);
            m_previewHelper->clearToolOverlays();
        } else {
            m_previewHelper->clear();
        }
    }
    showStatusText(tr("暂无运行结果"));
}

void MainWindow::showToolSnapshot(int row,
                                  const ToolPreviewSnapshot &snapshot,
                                  const QImage &baseImage,
                                  const QString &sourceLabel)
{
    Q_UNUSED(row)

    if (!baseImage.isNull()) {
        m_previewHelper->setImage(baseImage);
        m_previewHelper->setToolOverlays(snapshot.overlays);
    } else {
        m_previewHelper->clear();
    }

    showStatusText(toolSnapshotStatusLine(snapshot, sourceLabel));
}

void MainWindow::showAllLastRunOverlays()
{
    QVector<ToolOverlay> overlays;
    for (const ToolConfig &config : m_schemeToolConfigs) {
        if (!config.enabled)
            continue;

        const ToolPreviewSnapshot snapshot = m_lastRunSnapshots.value(config.toolId);
        if (!snapshot.valid)
            continue;

        overlays += snapshot.overlays;
    }
    m_previewHelper->setToolOverlays(overlays);
}

void MainWindow::storeLastRunSnapshots(const QVector<ToolConfig> &enabledConfigs,
                                       const QVector<ToolResult> &results)
{
    const int count = qMin(enabledConfigs.size(), results.size());
    for (int index = 0; index < count; ++index) {
        const ToolConfig &config = enabledConfigs.at(index);
        const ToolResult &result = results.at(index);
        m_lastRunSnapshots.insert(config.toolId, makeCameraToolPreviewSnapshot(config, result));
    }
}

void MainWindow::syncSnapshotMapsWithConfigs()
{
    QSet<QString> activeIds;
    for (const ToolConfig &config : m_schemeToolConfigs)
        activeIds.insert(config.toolId);

    for (auto it = m_referencePreviewSnapshots.begin(); it != m_referencePreviewSnapshots.end();) {
        if (!activeIds.contains(it.key()))
            it = m_referencePreviewSnapshots.erase(it);
        else
            ++it;
    }

    for (auto it = m_lastRunSnapshots.begin(); it != m_lastRunSnapshots.end();) {
        if (!activeIds.contains(it.key()))
            it = m_lastRunSnapshots.erase(it);
        else
            ++it;
    }

    if (m_selectedToolIndex >= m_schemeToolConfigs.size())
        m_selectedToolIndex = m_schemeToolConfigs.isEmpty() ? -1 : m_schemeToolConfigs.size() - 1;
}

QString MainWindow::toolDisplayName(const ToolConfig &config) const
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
    case ToolType::RegisteredClassification:
        return tr("注册分类");
    case ToolType::RegisteredClassificationDetection:
        return tr("注册目标检测");
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

QString MainWindow::toolRunStateText(const ToolConfig &config) const
{
    if (!config.enabled)
        return tr("未运行");

    const ToolPreviewSnapshot snapshot = m_lastRunSnapshots.value(config.toolId);
    if (!snapshot.valid)
        return tr("未运行");

    if (!snapshot.result.success)
        return QStringLiteral("error");

    return snapshot.ok ? QStringLiteral("OK") : QStringLiteral("NG");
}

QString MainWindow::toolSnapshotStatusLine(const ToolPreviewSnapshot &snapshot,
                                           const QString &sourceLabel) const
{
    const QString state = snapshot.result.success
            ? (snapshot.ok ? QStringLiteral("OK") : QStringLiteral("NG"))
            : QStringLiteral("error");
    const QString status = snapshot.statusText.trimmed().isEmpty()
            ? snapshot.result.status
            : snapshot.statusText;
    return tr("%1 | %2 | %3 | score:%4 | count:%5")
            .arg(sourceLabel,
                 state,
                 status,
                 QString::number(snapshot.score, 'f', 3),
                 QString::number(snapshot.count));
}

void MainWindow::editSchemeTool(int row)
{
    if (row < 0 || row >= m_schemeToolConfigs.size())
        return;

    if (m_isContinuousRunning)
        stopContinuousRun();

    if (m_isToolChainRunning) {
        showStatusText(tr("工具链运行中，暂不能编辑工具"));
        return;
    }

    openToolConfigDialogForEdit(row);
}

void MainWindow::switchSchemeFromCombo(int index)
{
    const QString schemeId = ui->comboBox->itemData(index).toString();
    switchSchemeById(schemeId);
}

bool MainWindow::openToolConfigDialogForEdit(int row)
{
    if (row < 0 || row >= m_schemeToolConfigs.size())
        return false;

    const ToolConfig originalConfig = m_schemeToolConfigs.at(row);
    ToolConfig editedConfig;
    ToolPreviewSnapshot snapshot;
    bool accepted = false;

    switch (originalConfig.toolType) {
    case ToolType::Ocr:
        accepted = runToolConfigDialog<CharacterRecognitionDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::ColorRecognition:
        accepted = runToolConfigDialog<ColorRecognitionDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::ColorComparison:
        accepted = runToolConfigDialog<ColorComparisonDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::RegisteredClassification:
        accepted = runToolConfigDialog<RegisteredClassificationDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::RegisteredClassificationDetection:
        accepted = runToolConfigDialog<RegisteredClassificationDetectionDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::PatternPresence:
        accepted = runToolConfigDialog<PatternPresenceDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::BlobPresence:
        accepted = runToolConfigDialog<BlobPresenceDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::CirclePresence:
        accepted = runToolConfigDialog<CirclePresenceDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::EdgePresence:
        accepted = runToolConfigDialog<EdgePresenceDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::LinePresence:
        accepted = runToolConfigDialog<LinePresenceDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::ContourPresence:
        accepted = runToolConfigDialog<ContourPresenceDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::AiDetection:
        accepted = runToolConfigDialog<ObjectDetectionDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::AiClassification:
        accepted = runToolConfigDialog<ClassificationDialog>(this, originalConfig, &editedConfig, &snapshot);
        break;
    default:
        qDebug() << "[MainWindow] Unsupported tool edit type:" << toolTypeToString(originalConfig.toolType);
        break;
    }

    if (!accepted)
        return false;

    editedConfig.toolId = originalConfig.toolId;
    editedConfig.toolType = originalConfig.toolType;
    editedConfig.category = originalConfig.category;
    m_schemeToolConfigs[row] = editedConfig;

    if (snapshot.valid) {
        ToolPreviewSnapshot normalized = snapshot;
        normalized.toolId = editedConfig.toolId;
        normalized.toolType = editedConfig.toolType;
        m_referencePreviewSnapshots.insert(editedConfig.toolId, normalized);
    }

    m_lastRunSnapshots.remove(editedConfig.toolId);
    m_selectedToolIndex = row;
    refreshToolConfigTable();
    selectSchemeTool(row);
    showStatusText(tr("工具参数已更新，主界面运行结果已标记为未运行"));
    persistCurrentSchemeState(QStringLiteral("editSchemeTool"));
    return true;
}
