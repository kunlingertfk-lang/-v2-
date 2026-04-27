#include "MainWindow.h"

#include <QComboBox>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>

#include "CameraParamsDialog.h"
#include "PlanDialogUtils.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupUiState();
    ui->planSettingButton->setStyleSheet("background:#ff7a00");
    connect(ui->clearSummaryButton, &QPushButton::clicked, this, &MainWindow::clearSummary);
    connect(ui->planSettingButton, &QPushButton::clicked, this, &MainWindow::openCameraParamsDialog);
    connect(ui->headerMinimizeButton, &QToolButton::clicked, this, &MainWindow::showMinimized);
    connect(ui->headerMaximizeButton, &QToolButton::clicked, this, &MainWindow::toggleWindowState);
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &MainWindow::close);
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

void MainWindow::toggleWindowState()
{
    isMaximized() ? showNormal() : showMaximized();
}

void MainWindow::setupUiState()
{
    setMinimumSize(1440, 900);
    PlanDialogUtils::applyConfiguredWindowState(this);
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
