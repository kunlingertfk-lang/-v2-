#include "LoginWindow.h"

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>

#include <QFontDatabase>
#include <QFont>

#include "MainWindow.h"
#include "PlanDialogUtils.h"
#include "WindowUtils.h"
#include "ui_LoginWindow.h"

LoginWindow::LoginWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginWindow)
    , m_passwordVisible(false)
{
    ui->setupUi(this);
    setupUiState();
    populateDevices();
    populateUsers();

    connect(ui->deviceListWidget, &QListWidget::currentRowChanged,this, &LoginWindow::syncSelectionFromList);
    connect(ui->deviceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),this, &LoginWindow::syncSelectionFromCombo);
    connect(ui->loginButton, &QPushButton::clicked, this, &LoginWindow::handleLogin);
    connect(ui->closeCardButton, &QToolButton::clicked, this, &LoginWindow::closeRequested);
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &LoginWindow::closeRequested);
    connect(ui->headerMinimizeButton, &QToolButton::clicked, this, &LoginWindow::showMinimized);
    connect(ui->headerMaximizeButton, &QToolButton::clicked, this, &LoginWindow::toggleWindowState);
    connect(ui->passwordToggleButton, &QToolButton::clicked,this, &LoginWindow::togglePasswordVisibility);
    connect(ui->refreshDeviceButton, &QToolButton::clicked, this, &LoginWindow::populateDevices);
}

LoginWindow::~LoginWindow()
{
    delete ui;
}


void LoginWindow::setupUiState()
{
    setWindowFlag(Qt::Dialog, false);
    setWindowFlag(Qt::Window, true);
    setWindowState(Qt::WindowNoState);
    WindowUtils::applyLargeWindow(this);

    ui->passwordLineEdit->setEchoMode(QLineEdit::Password);
    ui->passwordLineEdit->setText(QStringLiteral("123456"));
    ui->loginButton->setDefault(true);
}

void LoginWindow::populateDevices()
{
    const int currentRow = ui->deviceListWidget->currentRow();

    // 只保留设备名称
    QStringList deviceNames = {
        QStringLiteral("MV-SCA002C-03S-WBN-NR (DA9880297)"),
        QStringLiteral("MV-SCA002C-03S-WBN-NR (DA9880285)"),
        QStringLiteral("MV-SCA002C-03S-WBN-NR (DA9880272)")
    };

    {
        const QSignalBlocker blockerList(ui->deviceListWidget);
        const QSignalBlocker blockerCombo(ui->deviceComboBox);

        ui->deviceListWidget->clear();
        ui->deviceComboBox->clear();

        // 只加载名字
        for (const QString &name : deviceNames) {
            auto *item = new QListWidgetItem(QIcon(QStringLiteral(":/icons/device.svg")), name);
            item->setSizeHint(QSize(0, 56));
            ui->deviceListWidget->addItem(item);
            ui->deviceComboBox->addItem(QIcon(QStringLiteral(":/icons/camera.svg")), name);
        }
    }

    const int targetRow = (currentRow >= 0 && currentRow < deviceNames.size()) ? currentRow : 0;
    if (!deviceNames.isEmpty()) {
        ui->deviceListWidget->setCurrentRow(targetRow);
        ui->deviceComboBox->setCurrentIndex(targetRow);
    }
}

void LoginWindow::populateUsers()
{
    ui->userComboBox->clear();
    ui->userComboBox->addItems({
        QStringLiteral("管理员"),
        QStringLiteral("工程师"),
        QStringLiteral("操作员")
    });
}

void LoginWindow::handleLogin()
{
    if (ui->passwordLineEdit->text() != QStringLiteral("123456")) {
        QMessageBox::warning(this, tr("登录失败"), tr("密码错误"));
        ui->passwordLineEdit->setFocus();
        ui->passwordLineEdit->selectAll();
        return;
    }

    auto *mainWindow = new MainWindow;
    mainWindow->setAttribute(Qt::WA_DeleteOnClose);
    mainWindow->setSessionInfo(ui->deviceComboBox->currentText(), ui->userComboBox->currentText());
    PlanDialogUtils::showWindowFromWidget(this, mainWindow);

    close();
}

void LoginWindow::syncSelectionFromList(int row)
{
    const QSignalBlocker blocker(ui->deviceComboBox);
    ui->deviceComboBox->setCurrentIndex(row);
}

void LoginWindow::syncSelectionFromCombo(int index)
{
    const QSignalBlocker blocker(ui->deviceListWidget);
    ui->deviceListWidget->setCurrentRow(index);
}

void LoginWindow::togglePasswordVisibility()
{
    m_passwordVisible = !m_passwordVisible;
    ui->passwordLineEdit->setEchoMode(m_passwordVisible ? QLineEdit::Normal : QLineEdit::Password);
    ui->passwordToggleButton->setText(m_passwordVisible ? tr("隐藏") : tr("显示"));
}

void LoginWindow::toggleWindowState()
{
    isMaximized() ? showNormal() : showMaximized();
}

void LoginWindow::closeRequested()
{
    close();
}
