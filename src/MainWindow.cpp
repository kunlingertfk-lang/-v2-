#include "MainWindow.h"

#include <QComboBox>
#include <QHeaderView>
#include <QDebug>
#include <QPainter>
#include <QPushButton>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QToolButton>

#include "CameraParamsDialog.h"
#include "PlanDialogUtils.h"
#include "ToolsDialog.h"
#include "WindowUtils.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "ui_MainWindow.h"

#include <opencv2/core.hpp>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_toolEngine.registerAdapter(&m_ocrAdapter);
    m_toolEngine.registerAdapter(&m_patternPresenceAdapter);
    m_toolEngine.registerAdapter(&m_blobPresenceAdapter);
    m_toolEngine.registerAdapter(&m_circlePresenceAdapter);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    ui->planSettingButton->setStyleSheet("background:#ff7a00");
    connect(ui->clearSummaryButton, &QPushButton::clicked, this, &MainWindow::clearSummary);
    connect(ui->planSettingButton, &QPushButton::clicked, this, &MainWindow::openCameraParamsDialog);
    connect(ui->toolsSettingButton, &QToolButton::clicked, this, &MainWindow::openToolsDialog);
    connect(ui->singleRunButton, &QPushButton::clicked, this, &MainWindow::runSingleToolFlow);
    connect(ui->headerMinimizeButton, &QToolButton::clicked, this, &MainWindow::showMinimized);
    connect(ui->headerMaximizeButton, &QToolButton::clicked, this, &MainWindow::toggleWindowState);
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &MainWindow::close);
    connect(&CameraFrameProvider::instance(),
            &CameraFrameProvider::frameUpdated,
            this,
            [this](const QImage &image) {
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

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    ensureCameraRunning();
    refreshLivePreview();
}

void MainWindow::clearSummary()
{
    ui->okRateValueLabel->setText(QStringLiteral("0%"));
    ui->totalCountValueLabel->setText(QStringLiteral("0"));
    ui->okCountValueLabel->setText(QStringLiteral("0"));
    ui->ngCountValueLabel->setText(QStringLiteral("0"));
    ui->runtimeValueLabel->setText(QStringLiteral("0s"));
}

void MainWindow::openCameraParamsDialog()
{
    CameraParamsDialog *dialog = new CameraParamsDialog();// 打开相机参数窗口
    PlanDialogUtils::setSessionInfo(dialog,
                                    ui->headerDeviceComboBox->currentText(),
                                    ui->headerUserButton->text());
    PlanDialogUtils::showDialogFromWidget(this, dialog);
    this->close();
}

void MainWindow::openToolsDialog()
{
    ToolsDialog dialog(this);
    dialog.setAttribute(Qt::WA_DeleteOnClose, false);
    PlanDialogUtils::setSessionInfo(&dialog,
                                    ui->headerDeviceComboBox->currentText(),
                                    ui->headerUserButton->text());

    dialog.exec();

    m_toolConfigs = dialog.toolConfigs();
    refreshToolConfigTable();

    qDebug() << "[MainWindow] 已同步工具配置数量:" << m_toolConfigs.size();
    for (const ToolConfig &config : m_toolConfigs) {
        qDebug() << "[MainWindow] ToolConfig"
                 << config.toolId
                 << toolTypeToString(config.toolType)
                 << config.summary;
    }

    raise();
    activateWindow();
}

void MainWindow::runSingleToolFlow()
{
    if (m_toolConfigs.isEmpty()) {
        qDebug() << "[MainWindow] 当前没有工具配置，单次运行跳过。";
        ui->toolsTableWidget->setRowCount(1);
        ui->toolsTableWidget->setItem(0, 0, new QTableWidgetItem(tr("无工具配置")));
        ui->toolsTableWidget->setItem(0, 1, new QTableWidgetItem(tr("-")));
        ui->toolsTableWidget->setItem(0, 2, new QTableWidgetItem(tr("idle")));
        ui->toolsTableWidget->setItem(0, 3, new QTableWidgetItem(tr("0ms")));
        ui->toolsTableWidget->setItem(0, 4, new QTableWidgetItem(tr("当前没有工具配置")));
        return;
    }

    cv::Mat image = CameraFrameProvider::instance().currentFrame();
    if (image.empty()) {
        qDebug() << "[MainWindow] Current frame is empty.";
    } else {
        qDebug() << QString("[MainWindow] Current frame size: %1 x %2")
                        .arg(image.cols)
                        .arg(image.rows);
    }
    qDebug() << "[MainWindow] 单次运行开始，工具数量:" << m_toolConfigs.size();

    const cv::Mat referenceImage = ReferenceImageProvider::instance().referenceFrame();
    const QVector<ToolResult> results = m_toolEngine.runTools(m_toolConfigs, image, referenceImage);
    for (int i = 0; i < results.size(); ++i) {
        const ToolResult &result = results.at(i);
        qDebug() << "[MainWindow] ToolResult"
                 << "toolId=" << result.toolId
                 << "type=" << toolTypeToString(result.toolType)
                 << "success=" << result.success
                 << "ok=" << result.ok
                 << "status=" << result.status
                 << "message=" << result.message
                 << "text=" << result.text
                 << "score=" << result.score
                 << "count=" << result.count
                 << "elapsedMs=" << result.elapsedMs;
        updateToolResultRow(i, result);
    }
}

void MainWindow::toggleWindowState()
{
    isMaximized() ? showNormal() : showMaximized();
}

void MainWindow::setupUiState()
{
    WindowUtils::applyLargeWindow(this);
    ui->headerDeviceComboBox->addItem(QIcon(QStringLiteral(":/icons/camera.svg")),QStringLiteral("MV-SCA002C-03S-WBN-NR (DA9880297)"));
    ui->headerDeviceComboBox->addItem(QIcon(QStringLiteral(":/icons/camera.svg")),QStringLiteral("MV-SCA002C-03S-WBN-NR (DA9880285)"));
    ui->headerDeviceComboBox->addItem(QIcon(QStringLiteral(":/icons/camera.svg")),QStringLiteral("MV-SCA002C-03S-WBN-NR (DA9880272)"));

    ui->toolResultButton->setChecked(true);
    ui->resultResultButton->setChecked(false);

    ui->toolsTableWidget->horizontalHeader()->setVisible(false);
    ui->toolsTableWidget->verticalHeader()->setVisible(false);
    ui->toolsTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->toolsTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->toolsTableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->toolsTableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->toolsTableWidget->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->toolsTableWidget->setColumnCount(5);
    ui->toolsTableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    ui->toolsTableWidget->setFocusPolicy(Qt::NoFocus);
    ui->toolsTableWidget->setShowGrid(false);
    ui->toolsTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
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
        m_previewHelper->clear();
        qDebug() << "[MainWindow] 当前无图像。";
        return;
    }

    m_previewHelper->setImage(image);
}

void MainWindow::refreshToolConfigTable()
{
    ui->toolsTableWidget->clearContents();
    ui->toolsTableWidget->setRowCount(m_toolConfigs.size());

    for (int row = 0; row < m_toolConfigs.size(); ++row) {
        const ToolConfig &config = m_toolConfigs.at(row);
        ui->toolsTableWidget->setItem(row, 0, new QTableWidgetItem(config.displayName));
        ui->toolsTableWidget->setItem(row, 1, new QTableWidgetItem(config.summary));
        ui->toolsTableWidget->setItem(row, 2, new QTableWidgetItem(config.enabled ? tr("enabled") : tr("disabled")));
        ui->toolsTableWidget->setItem(row, 3, new QTableWidgetItem(tr("-")));
        ui->toolsTableWidget->setItem(row, 4, new QTableWidgetItem(config.toolId));
    }
}

void MainWindow::updateToolResultRow(int row, const ToolResult &result)
{
    if (row < 0)
        return;

    if (ui->toolsTableWidget->rowCount() <= row)
        ui->toolsTableWidget->setRowCount(row + 1);

    const QString name = row < m_toolConfigs.size()
            ? m_toolConfigs.at(row).displayName
            : toolTypeToString(result.toolType);
    const QString resultText = result.success
            ? (result.ok ? tr("OK") : tr("NG"))
            : result.status;

    ui->toolsTableWidget->setItem(row, 0, new QTableWidgetItem(name));
    ui->toolsTableWidget->setItem(row, 1, new QTableWidgetItem(result.message));
    ui->toolsTableWidget->setItem(row, 2, new QTableWidgetItem(resultText));
    ui->toolsTableWidget->setItem(row, 3, new QTableWidgetItem(QStringLiteral("%1ms").arg(result.elapsedMs)));
    ui->toolsTableWidget->setItem(row, 4, new QTableWidgetItem(result.toolId));
}

void MainWindow::on_stopRunButton_clicked()
{
    m_isRunning = !m_isRunning; // 取反：最简单的状态切换

    if(m_isRunning) {
        ui->stopRunButton->setText("停止运行");
        ui->stopRunButton->setStyleSheet("background:#ff7a00");
        ui->stopRunButton->setIcon(QIcon(":/icons/stop.svg"));
        ui->planSettingButton->setStyleSheet("background:#8d94a1");

        ui->singleRunButton->setStyleSheet("background:#555b67");
        ui->singleRunButton->setEnabled(false); // 禁用点击
    } else {
        ui->stopRunButton->setText("连续运行");
        ui->stopRunButton->setStyleSheet("background:#8d94a1");
        ui->stopRunButton->setIcon(QIcon(":/icons/refresh.svg"));
        ui->planSettingButton->setStyleSheet("background:#ff7a00");

        ui->singleRunButton->setStyleSheet("background:#8d94a1");
        ui->singleRunButton->setEnabled(true); // 启用点击
    }
}
