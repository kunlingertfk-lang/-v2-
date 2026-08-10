#include "QuickCalibrationWizard.h"

#include "PlanDialogUtils.h"
#include "SchemeStore.h"
#include "UiStyleRoles.h"
#include "calibration/CalibrationFileLoader.h"
#include "calibration/CalibrationSourceFingerprint.h"
#include "calibration/NPointCalibrationConfigWidget.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsView>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSaveFile>
#include <QSet>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <opencv2/imgproc.hpp>

#include <cmath>
#include <utility>

namespace {

QLabel *pageTitle(const QString &text, QWidget *parent)
{
    QLabel *label = new QLabel(text, parent);
    label->setProperty("role", QStringLiteral("pageTitle"));
    return label;
}

QString matrixLine(const std::array<double, 6> &matrix)
{
    return QStringLiteral("[%1  %2  %3]\n[%4  %5  %6]")
            .arg(matrix[0], 0, 'g', 12).arg(matrix[1], 0, 'g', 12)
            .arg(matrix[2], 0, 'g', 12).arg(matrix[3], 0, 'g', 12)
            .arg(matrix[4], 0, 'g', 12).arg(matrix[5], 0, 'g', 12);
}

cv::Mat qImageToBgrMat(const QImage &image)
{
    if (image.isNull())
        return cv::Mat();

    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgbMat(rgb.height(), rgb.width(), CV_8UC3,
                   const_cast<uchar *>(rgb.constBits()),
                   static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(rgbMat, bgr, cv::COLOR_RGB2BGR);
    return bgr.clone();
}

bool finitePayloadNumber(const QJsonObject &payload, const QString &key)
{
    return payload.value(key).isDouble()
            && std::isfinite(payload.value(key).toDouble());
}

QJsonArray overlayPointArray(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points) {
        array.append(QJsonObject{{QStringLiteral("x"), point.x()},
                                 {QStringLiteral("y"), point.y()}});
    }
    return array;
}

QString fileSha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return QString();
    return QString::fromLatin1(hash.result().toHex());
}

QString imageSha256(const QImage &source)
{
    if (source.isNull())
        return QString();
    const QImage image = source.convertToFormat(QImage::Format_RGBA8888);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QByteArray metadata = QByteArray::number(image.width()) + 'x'
            + QByteArray::number(image.height()) + ':'
            + QByteArray::number(image.bytesPerLine()) + ':';
    hash.addData(metadata);
    hash.addData(reinterpret_cast<const char *>(image.constBits()),
                 image.bytesPerLine() * image.height());
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace

QuickCalibrationWizard::QuickCalibrationWizard(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("QuickCalibrationWizard"));
    m_captureToolEngine.registerAdapter(&m_captureTemplateLocationAdapter);
    m_communicationSession = new CalibrationCommunicationSession(this);
    m_communicationSession->setMessageHandler(
                [this](CalibrationCommunicationMessage *message, QString *errorMessage) {
        return handleCommunicationMessage(message, errorMessage);
    });
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowModality(Qt::WindowModal);
    setMinimumSize(1280, 780);
    PlanDialogUtils::configureDialogWindow(this, tr("快速标定"));

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    QFrame *titleBar = new QFrame(this);
    titleBar->setObjectName(QStringLiteral("calibrationHeader"));
    titleBar->setFixedHeight(58);
    QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(28, 0, 18, 0);
    QLabel *title = new QLabel(tr("标定"), titleBar);
    title->setProperty("role", QStringLiteral("dialogTitle"));
    titleLayout->addWidget(title);
    titleLayout->addStretch();
    m_closeButton = new QPushButton(QStringLiteral("×"), titleBar);
    m_closeButton->setProperty("actionRole", QStringLiteral("windowClose"));
    titleLayout->addWidget(m_closeButton);
    root->addWidget(titleBar);
    root->addWidget(createStepHeader());

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(createMethodPage());
    m_pages->addWidget(createCommunicationPage());
    m_pages->addWidget(createConfigurationPage());
    m_pages->addWidget(createResultPage());
    root->addWidget(m_pages, 1);

    QFrame *footer = new QFrame(this);
    footer->setObjectName(QStringLiteral("calibrationFooter"));
    footer->setFixedHeight(70);
    QHBoxLayout *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(28, 10, 28, 10);
    footerLayout->addStretch();
    m_previousButton = new QPushButton(tr("上一步"), footer);
    m_nextButton = new QPushButton(tr("下一步"), footer);
    m_previousButton->setProperty("actionRole", QStringLiteral("secondary"));
    m_nextButton->setProperty("actionRole", QStringLiteral("highlight"));
    footerLayout->addWidget(m_previousButton);
    footerLayout->addWidget(m_nextButton);
    root->addWidget(footer);

    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(this, &QDialog::finished, this, [this](int) {
        if (m_communicationSession && m_communicationSession->isRunning())
            m_communicationSession->stop();
        QString error;
        if (!saveDraft(&error) && !error.isEmpty())
            qWarning() << "[QuickCalibrationWizard] 关闭时保存草稿失败:" << error;
    });
    connect(m_communicationSession, &CalibrationCommunicationSession::messageReceived,
            this, [this](const CalibrationCommunicationMessage &message) {
        m_lastCommunicationMessage = message;
        m_sessionLog.append(message.logEntry);
        if (m_methodConfigWidget)
            m_methodConfigWidget->setLatestPhysicalSample(
                        message.valid && message.event == CalibrationCommunicationEvent::Capture,
                        message.x, message.y, message.angleDeg);
        m_communicationStatus->setText(message.valid
                ? tr("已接收：X=%1 Y=%2 Angle=%3")
                  .arg(message.x).arg(message.y).arg(message.angleDeg)
                : tr("报文失败：%1").arg(message.error));
    });
    connect(m_communicationSession, &CalibrationCommunicationSession::stateChanged,
            this, [this](const QString &state) {
        if (m_communicationStatus)
            m_communicationStatus->setText(tr("通信状态：%1").arg(state));
        if (m_communicationStartButton)
            m_communicationStartButton->setText(
                        m_communicationSession->isRunning() ? tr("停止通信") : tr("启动通信"));
    });
    connect(m_previousButton, &QPushButton::clicked, this, [this]() { setStep(m_step - 1); });
    connect(m_nextButton, &QPushButton::clicked, this, [this]() {
        if (m_step == 0) {
            const ICalibrationMethod *method = CalibrationMethodRegistry::instance().method(m_methodId);
            if (!method || method->availability() != CalibrationMethodAvailability::Available) {
                QMessageBox::information(this, tr("标定方式"), tr("该标定方式将在后续版本接入"));
                return;
            }
        }
        if (m_step == 1 && !validateCommunicationPage())
            return;
        if (m_step == 2) {
            solveCalibration();
            if (!m_solveResult.success)
                return;
        }
        if (m_step == 3) {
            accept();
            return;
        }
        setStep(m_step + 1);
    });
    setStep(0);
}

QWidget *QuickCalibrationWizard::createStepHeader()
{
    QFrame *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("calibrationStepHeader"));
    frame->setFixedHeight(120);
    QHBoxLayout *layout = new QHBoxLayout(frame);
    layout->setContentsMargins(250, 0, 250, 0);
    const QStringList names{tr("1  选择标定类型"), tr("2  通信配置"),
                            tr("3  标定配置"), tr("4  结果查看")};
    for (int i = 0; i < names.size(); ++i) {
        QPushButton *button = new QPushButton(names.at(i), frame);
        button->setFlat(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setProperty("stepIndex", i);
        button->setProperty("navRole", QStringLiteral("calibrationStep"));
        m_stepButtons.append(button);
        layout->addWidget(button, 1);
        connect(button, &QPushButton::clicked, this, [this, i]() {
            if (i <= m_maxVisitedStep)
                setStep(i);
        });
        if (i + 1 < names.size()) {
            QLabel *arrow = new QLabel(QStringLiteral("›››"), frame);
            arrow->setProperty("role", QStringLiteral("stepArrow"));
            layout->addWidget(arrow);
        }
    }
    return frame;
}

QWidget *QuickCalibrationWizard::createMethodPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName(QStringLiteral("calibrationMethodPage"));
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(150, 70, 150, 70);
    layout->addWidget(pageTitle(tr("选择标定类型"), page));
    layout->addSpacing(36);
    QHBoxLayout *cards = new QHBoxLayout;
    cards->setSpacing(48);
    m_methodButtons = new QButtonGroup(page);
    m_methodButtons->setExclusive(true);
    const QVector<const ICalibrationMethod *> methods =
            CalibrationMethodRegistry::instance().methods();
    for (const ICalibrationMethod *method : methods) {
        QPushButton *button = new QPushButton(page);
        button->setCheckable(true);
        button->setMinimumSize(300, 230);
        button->setText(QStringLiteral("%1\n\n%2\n\n%3")
                        .arg(method->displayName(), method->description(),
                             calibrationAvailabilityText(method->availability())));
        button->setProperty("methodId", method->methodId());
        button->setProperty("buttonRole", QStringLiteral("calibrationMethod"));
        button->setProperty("availability", method->availability()
                            == CalibrationMethodAvailability::Available
                            ? QStringLiteral("available") : QStringLiteral("planned"));
        if (method->methodId() == QStringLiteral("n_point"))
            button->setChecked(true);
        m_methodButtons->addButton(button);
        cards->addWidget(button);
    }
    connect(m_methodButtons, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            this, [this](QAbstractButton *button) {
        m_methodId = button->property("methodId").toString();
    });
    layout->addLayout(cards);
    layout->addStretch();
    return page;
}

QWidget *QuickCalibrationWizard::createCommunicationPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName(QStringLiteral("calibrationCommunicationPage"));
    QVBoxLayout *pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(0);
    QScrollArea *scroll = new QScrollArea(page);
    scroll->setObjectName(QStringLiteral("calibrationCommunicationScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("calibrationCommunicationContent"));
    QVBoxLayout *root = new QVBoxLayout(content);
    root->setContentsMargins(180, 45, 180, 45);
    root->setSpacing(18);
    root->addWidget(pageTitle(tr("通信配置"), page));
    QGroupBox *device = new QGroupBox(tr("通信设备"), page);
    device->setProperty("panelRole", QStringLiteral("configCard"));
    QFormLayout *deviceForm = new QFormLayout(device);
    deviceForm->setVerticalSpacing(10);
    deviceForm->setHorizontalSpacing(18);
    m_communicationType = new QComboBox(device);
    m_communicationType->setObjectName(QStringLiteral("calibrationCommunicationTypeCombo"));
    m_communicationType->addItem(tr("无设备（手动输入）"), QStringLiteral("none"));
    m_communicationType->addItem(tr("TCP 客户端"), QStringLiteral("tcp_client"));
    m_communicationType->addItem(tr("TCP 服务端"), QStringLiteral("tcp_server"));
    m_communicationType->addItem(tr("UDP"), QStringLiteral("udp"));
    UiStyleRoles::applyLightComboBox(m_communicationType);
    deviceForm->addRow(tr("通信方式"), m_communicationType);
    m_communicationHost = new QLineEdit(QStringLiteral("127.0.0.1"), device);
    m_communicationHost->setObjectName(QStringLiteral("calibrationCommunicationHostEdit"));
    m_communicationHost->setProperty("transportIndex", 0);
    m_communicationHost->setProperty("tcpClientAddress", QStringLiteral("127.0.0.1"));
    m_communicationHost->setProperty("tcpServerAddress", QStringLiteral("0.0.0.0"));
    m_communicationHost->setProperty("udpBindAddress", QStringLiteral("0.0.0.0"));
    m_communicationPort = new QSpinBox(device);
    m_communicationPort->setObjectName(QStringLiteral("calibrationCommunicationPortSpin"));
    m_communicationPort->setRange(1, 65535);
    m_communicationPort->setValue(2000);
    QLabel *hostLabel = new QLabel(tr("IP 地址"), device);
    hostLabel->setObjectName(QStringLiteral("calibrationCommunicationHostLabel"));
    hostLabel->setBuddy(m_communicationHost);
    deviceForm->addRow(hostLabel, m_communicationHost);
    deviceForm->addRow(tr("端口"), m_communicationPort);
    root->addWidget(device);
    QGroupBox *signalGroup = new QGroupBox(tr("通信字符配置"), page);
    signalGroup->setProperty("panelRole", QStringLiteral("configCard"));
    QGridLayout *grid = new QGridLayout(signalGroup);
    grid->setContentsMargins(18, 24, 18, 18);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 1);
    m_startSignal = new QLineEdit(QStringLiteral("SC"), signalGroup);
    m_calibrationSignal = new QLineEdit(QStringLiteral("21212"), signalGroup);
    m_endSignal = new QLineEdit(QStringLiteral("EC"), signalGroup);
    m_delimiter = new QLineEdit(QStringLiteral(","), signalGroup);
    m_terminator = new QLineEdit(QStringLiteral("\\r"), signalGroup);
    grid->addWidget(new QLabel(tr("类型"), signalGroup), 0, 0);
    grid->addWidget(new QLabel(tr("输入"), signalGroup), 0, 1);
    grid->addWidget(new QLabel(tr("OK 输出"), signalGroup), 0, 2);
    grid->addWidget(new QLabel(tr("NG 输出"), signalGroup), 0, 3);
    m_startOk = new QLineEdit(QStringLiteral("SC,1"), signalGroup);
    m_startNg = new QLineEdit(QStringLiteral("SC,0"), signalGroup);
    m_captureOk = new QLineEdit(QStringLiteral("21212,1"), signalGroup);
    m_captureNg = new QLineEdit(QStringLiteral("21212,0"), signalGroup);
    m_endOk = new QLineEdit(QStringLiteral("EC,1"), signalGroup);
    m_endNg = new QLineEdit(QStringLiteral("EC,0"), signalGroup);
    struct SignalRow { QString title; QLineEdit *input; QLineEdit *ok; QLineEdit *ng; };
    const QList<SignalRow> rows{{tr("开始信号"), m_startSignal, m_startOk, m_startNg},
                                {tr("标定信号"), m_calibrationSignal, m_captureOk, m_captureNg},
                                {tr("结束信号"), m_endSignal, m_endOk, m_endNg}};
    for (int i = 0; i < rows.size(); ++i) {
        grid->addWidget(new QLabel(rows.at(i).title, signalGroup), i + 1, 0);
        grid->addWidget(rows.at(i).input, i + 1, 1);
        grid->addWidget(rows.at(i).ok, i + 1, 2);
        grid->addWidget(rows.at(i).ng, i + 1, 3);
    }
    grid->addWidget(new QLabel(tr("分割符"), signalGroup), 4, 0);
    grid->addWidget(m_delimiter, 4, 1);
    grid->addWidget(new QLabel(tr("结束符"), signalGroup), 4, 2);
    grid->addWidget(m_terminator, 4, 3);
    m_xField = new QSpinBox(signalGroup);
    m_yField = new QSpinBox(signalGroup);
    m_angleField = new QSpinBox(signalGroup);
    for (QSpinBox *field : {m_xField, m_yField, m_angleField})
        field->setRange(1, 32);
    m_xField->setValue(1);
    m_yField->setValue(2);
    m_angleField->setValue(3);
    QLabel *fieldMappingTitle = new QLabel(tr("坐标字段映射（命令列=0）"), signalGroup);
    fieldMappingTitle->setProperty("role", QStringLiteral("subsectionTitle"));
    grid->addWidget(fieldMappingTitle, 5, 0, 1, 4);
    QHBoxLayout *fieldLayout = new QHBoxLayout;
    fieldLayout->setContentsMargins(0, 0, 0, 0);
    fieldLayout->setSpacing(12);
    fieldLayout->addWidget(new QLabel(tr("X"), signalGroup));
    fieldLayout->addWidget(m_xField, 1);
    fieldLayout->addWidget(new QLabel(tr("Y"), signalGroup));
    fieldLayout->addWidget(m_yField, 1);
    fieldLayout->addWidget(new QLabel(tr("Angle"), signalGroup));
    fieldLayout->addWidget(m_angleField, 1);
    grid->addLayout(fieldLayout, 6, 0, 1, 4);
    root->addWidget(signalGroup);
    QGroupBox *testGroup = new QGroupBox(tr("报文验证与会话日志"), page);
    testGroup->setProperty("panelRole", QStringLiteral("configCard"));
    QHBoxLayout *testLayout = new QHBoxLayout(testGroup);
    m_testMessage = new QLineEdit(QStringLiteral("21212,10.0,20.0,0.0\\r"), testGroup);
    QPushButton *testButton = new QPushButton(tr("解析测试"), testGroup);
    m_communicationStartButton = new QPushButton(tr("启动通信"), testGroup);
    testButton->setProperty("actionRole", QStringLiteral("secondary"));
    m_communicationStartButton->setProperty("actionRole", QStringLiteral("highlight"));
    m_communicationStatus = new QLabel(tr("尚未验证"), testGroup);
    testLayout->addWidget(m_testMessage, 1);
    testLayout->addWidget(testButton);
    testLayout->addWidget(m_communicationStartButton);
    testLayout->addWidget(m_communicationStatus);
    root->addWidget(testGroup);
    connect(testButton, &QPushButton::clicked,
            this, &QuickCalibrationWizard::testCommunicationMessage);
    connect(m_communicationStartButton, &QPushButton::clicked,
            this, &QuickCalibrationWizard::toggleCommunicationSession);
    const auto addressPropertyName = [](int index) -> const char * {
        switch (index) {
        case 1: return "tcpClientAddress";
        case 2: return "tcpServerAddress";
        case 3: return "udpBindAddress";
        default: return nullptr;
        }
    };
    const auto syncTransportFields = [this, hostLabel, addressPropertyName](int index) {
        const int previousIndex = m_communicationHost->property("transportIndex").toInt();
        const char *previousAddressProperty = addressPropertyName(previousIndex);
        if (previousAddressProperty && previousIndex != index) {
            m_communicationHost->setProperty(previousAddressProperty,
                                             m_communicationHost->text().trimmed());
        }

        const char *currentAddressProperty = addressPropertyName(index);
        if (currentAddressProperty && previousIndex != index) {
            m_communicationHost->setText(
                        m_communicationHost->property(currentAddressProperty).toString());
        }
        m_communicationHost->setProperty("transportIndex", index);

        const bool hasDevice = index != 0;
        m_communicationHost->setEnabled(hasDevice);
        m_communicationPort->setEnabled(hasDevice);
        m_communicationStartButton->setText(hasDevice ? tr("启动通信") : tr("启用手动会话"));

        QString labelText = tr("IP 地址");
        QString hint = tr("选择通信方式后填写对应的 IP 地址");
        if (index == 1) {
            labelText = tr("服务端 IP 地址");
            hint = tr("填写机械臂、PLC 等对端 TCP 服务端的 IP；127.0.0.1 仅用于同机联调");
            m_communicationHost->setPlaceholderText(tr("例如：192.168.10.20（对端设备）"));
        } else if (index == 2) {
            labelText = tr("本机绑定 IP 地址");
            hint = tr("填写本机网卡 IP；0.0.0.0 表示监听全部本机网卡");
            m_communicationHost->setPlaceholderText(tr("0.0.0.0（监听全部网卡）"));
        } else if (index == 3) {
            labelText = tr("本机绑定 IP 地址");
            hint = tr("UDP 当前为接收绑定模式；0.0.0.0 表示监听全部本机网卡");
            m_communicationHost->setPlaceholderText(tr("0.0.0.0（监听全部网卡）"));
        } else {
            m_communicationHost->setPlaceholderText(tr("当前模式无需 IP 地址"));
        }
        hostLabel->setText(labelText);
        hostLabel->setToolTip(hint);
        m_communicationHost->setToolTip(hint);
    };
    connect(m_communicationType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, syncTransportFields);
    syncTransportFields(m_communicationType->currentIndex());
    root->addStretch();
    scroll->setWidget(content);
    pageLayout->addWidget(scroll);
    return page;
}

QWidget *QuickCalibrationWizard::createConfigurationPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName(QStringLiteral("calibrationConfigurationPage"));
    QHBoxLayout *body = new QHBoxLayout(page);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    QScrollArea *configScroll = new QScrollArea(page);
    configScroll->setObjectName(QStringLiteral("calibrationConfigScroll"));
    configScroll->setWidgetResizable(true);
    configScroll->setMinimumWidth(520);
    configScroll->setMaximumWidth(680);
    QFrame *left = new QFrame(configScroll);
    left->setObjectName(QStringLiteral("calibrationConfigPanel"));
    m_methodConfigLayout = new QVBoxLayout(left);
    m_methodConfigLayout->setContentsMargins(24, 20, 24, 20);
    m_methodConfigLayout->setSpacing(12);
    m_methodConfigLayout->addWidget(pageTitle(tr("标定配置"), left));
    QPushButton *execute = new QPushButton(tr("执行"), left);
    execute->setProperty("actionRole", QStringLiteral("highlight"));
    m_methodConfigLayout->addWidget(execute, 0, Qt::AlignLeft);
    configScroll->setWidget(left);
    body->addWidget(configScroll, 2);
    QFrame *preview = new QFrame(page);
    preview->setObjectName(QStringLiteral("calibrationPreviewPanel"));
    QVBoxLayout *previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);
    QFrame *toolbar = new QFrame(preview);
    toolbar->setObjectName(QStringLiteral("calibrationPreviewToolbar"));
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(16, 8, 16, 8);
    m_cameraModeButton = new QPushButton(tr("相机模式"), toolbar);
    m_imageModeButton = new QPushButton(tr("图像模式"), toolbar);
    m_cameraModeButton->setCheckable(true);
    m_imageModeButton->setCheckable(true);
    m_cameraModeButton->setProperty("modeRole", QStringLiteral("previewMode"));
    m_imageModeButton->setProperty("modeRole", QStringLiteral("previewMode"));
    m_imageModeButton->setChecked(true);
    toolbarLayout->addWidget(m_cameraModeButton);
    toolbarLayout->addWidget(m_imageModeButton);
    toolbarLayout->addStretch();
    previewLayout->addWidget(toolbar);
    m_previewView = new QGraphicsView(preview);
    m_previewView->setObjectName(QStringLiteral("calibrationPreviewView"));
    m_previewHelper = new FrameViewHelper(m_previewView, this);
    m_previewHelper->setNavigationEnabled(true);
    previewLayout->addWidget(m_previewView, 1);
    m_previewEmptyLabel = new QLabel(tr("暂无图像\n请先设置基准图或切换相机输入"), m_previewView);
    m_previewEmptyLabel->setObjectName(QStringLiteral("calibrationPreviewEmpty"));
    m_previewEmptyLabel->setAlignment(Qt::AlignCenter);
    m_previewEmptyLabel->setWordWrap(true);
    m_previewEmptyLabel->setGeometry(0, 0, 420, 100);
    m_previewEmptyLabel->move(60, 120);
    QFrame *statusBar = new QFrame(preview);
    statusBar->setObjectName(QStringLiteral("calibrationPreviewStatusBar"));
    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 8, 16, 8);
    m_sampleStatus = new QLabel(tr("等待采样"), statusBar);
    m_sampleStatus->setProperty("status", QStringLiteral("idle"));
    statusLayout->addWidget(m_sampleStatus);
    statusLayout->addStretch();
    m_imageCounterLabel = new QLabel(tr("当前：0/0"), statusBar);
    m_imageCounterLabel->setObjectName(QStringLiteral("calibrationImageCounter"));
    statusLayout->addWidget(m_imageCounterLabel);
    previewLayout->addWidget(statusBar);

    m_imageCollectionPanel = new QFrame(preview);
    m_imageCollectionPanel->setObjectName(QStringLiteral("calibrationImageCollection"));
    QVBoxLayout *collectionLayout = new QVBoxLayout(m_imageCollectionPanel);
    collectionLayout->setContentsMargins(12, 8, 12, 10);
    collectionLayout->setSpacing(8);
    QHBoxLayout *imageActions = new QHBoxLayout;
    imageActions->setContentsMargins(0, 0, 0, 0);
    m_previousImageButton = new QToolButton(m_imageCollectionPanel);
    m_previousImageButton->setText(QStringLiteral("‹"));
    m_previousImageButton->setToolTip(tr("上一张"));
    m_nextImageButton = new QToolButton(m_imageCollectionPanel);
    m_nextImageButton->setText(QStringLiteral("›"));
    m_nextImageButton->setToolTip(tr("下一张"));
    m_importImageButton = new QToolButton(m_imageCollectionPanel);
    m_importImageButton->setText(tr("导入图片"));
    m_importImageButton->setIcon(QIcon(QStringLiteral(":/icons/add.svg")));
    m_importImageButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_importImageButton->setToolTip(tr("从外部导入一张或多张标定图片"));
    m_removeImageButton = new QToolButton(m_imageCollectionPanel);
    m_removeImageButton->setIcon(QIcon(QStringLiteral(":/icons/delete-outline.svg")));
    m_removeImageButton->setToolTip(tr("删除当前图片"));
    m_clearImagesButton = new QToolButton(m_imageCollectionPanel);
    m_clearImagesButton->setIcon(QIcon(QStringLiteral(":/icons/trash.svg")));
    m_clearImagesButton->setToolTip(tr("清空图片列表"));
    const QList<QToolButton *> imageButtons = {
        m_previousImageButton, m_nextImageButton, m_importImageButton,
        m_removeImageButton, m_clearImagesButton
    };
    for (QToolButton *button : imageButtons)
        button->setProperty("actionRole", QStringLiteral("imageToolbar"));
    m_importImageButton->setProperty("actionState", QStringLiteral("primary"));
    imageActions->addWidget(m_previousImageButton);
    imageActions->addWidget(m_nextImageButton);
    imageActions->addStretch();
    imageActions->addWidget(m_importImageButton);
    imageActions->addWidget(m_removeImageButton);
    imageActions->addWidget(m_clearImagesButton);
    collectionLayout->addLayout(imageActions);
    m_imageThumbnailList = new QListWidget(m_imageCollectionPanel);
    m_imageThumbnailList->setObjectName(QStringLiteral("calibrationImageThumbnailList"));
    m_imageThumbnailList->setViewMode(QListView::IconMode);
    m_imageThumbnailList->setFlow(QListView::LeftToRight);
    m_imageThumbnailList->setMovement(QListView::Static);
    m_imageThumbnailList->setResizeMode(QListView::Adjust);
    m_imageThumbnailList->setWrapping(false);
    m_imageThumbnailList->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_imageThumbnailList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_imageThumbnailList->setIconSize(QSize(132, 82));
    m_imageThumbnailList->setSpacing(8);
    m_imageThumbnailList->setFixedHeight(126);
    collectionLayout->addWidget(m_imageThumbnailList);
    previewLayout->addWidget(m_imageCollectionPanel);
    body->addWidget(preview, 3);
    connect(execute, &QPushButton::clicked, this, &QuickCalibrationWizard::solveCalibration);
    connect(m_cameraModeButton, &QPushButton::clicked, this, [this]() {
        m_cameraModeButton->setChecked(true);
        m_imageModeButton->setChecked(false);
        m_imageCollectionPanel->hide();
        m_imageCounterLabel->hide();
        updatePreviewImage(CameraFrameProvider::instance().currentImage());
        m_sampleStatus->setText(CameraFrameProvider::instance().hasFrame()
                                ? tr("相机模式：实时帧已接入")
                                : tr("相机模式：当前没有相机帧"));
        saveDraft(nullptr);
    });
    connect(m_imageModeButton, &QPushButton::clicked, this, [this]() {
        m_cameraModeButton->setChecked(false);
        m_imageModeButton->setChecked(true);
        m_imageCollectionPanel->show();
        m_imageCounterLabel->show();
        updateImageModeUi();
        saveDraft(nullptr);
    });
    connect(&ReferenceImageProvider::instance(), &ReferenceImageProvider::referenceFrameChanged,
            this, [this](const QImage &image) {
        m_referencePreviewImage = image;
        if (m_imageModeButton && m_imageModeButton->isChecked())
            updateImageModeUi();
    });
    connect(&CameraFrameProvider::instance(), &CameraFrameProvider::frameUpdated,
            this, [this](const QImage &image) {
        if (m_cameraModeButton && m_cameraModeButton->isChecked())
            updatePreviewImage(image);
    });
    connect(m_importImageButton, &QToolButton::clicked,
            this, &QuickCalibrationWizard::importExternalImages);
    connect(m_removeImageButton, &QToolButton::clicked,
            this, &QuickCalibrationWizard::removeCurrentExternalImage);
    connect(m_clearImagesButton, &QToolButton::clicked,
            this, &QuickCalibrationWizard::clearExternalImages);
    connect(m_previousImageButton, &QToolButton::clicked, this, [this]() {
        m_externalImageSequenceComplete = false;
        showExternalImage(m_imageThumbnailList->currentRow() - 1);
    });
    connect(m_nextImageButton, &QToolButton::clicked, this, [this]() {
        m_externalImageSequenceComplete = false;
        showExternalImage(m_imageThumbnailList->currentRow() + 1);
    });
    connect(m_imageThumbnailList, &QListWidget::currentRowChanged,
            this, [this](int index) {
        m_externalImageSequenceComplete = false;
        showExternalImage(index);
    });
    m_referencePreviewImage = ReferenceImageProvider::instance().referenceImage();
    updateImageModeUi();
    return page;
}

QWidget *QuickCalibrationWizard::createResultPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName(QStringLiteral("calibrationResultPage"));
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(120, 36, 120, 36);
    layout->addWidget(pageTitle(tr("结果查看"), page));
    m_resultTable = new QTableWidget(0, 3, page);
    m_resultTable->setObjectName(QStringLiteral("calibrationResultTable"));
    m_resultTable->setHorizontalHeaderLabels({tr("序号"), tr("信号内容"), tr("结果状态")});
    QHeaderView *resultHeader = m_resultTable->horizontalHeader();
    resultHeader->setSectionsMovable(false);
    resultHeader->setMinimumSectionSize(60);
    resultHeader->setSectionResizeMode(0, QHeaderView::Fixed);
    resultHeader->setSectionResizeMode(1, QHeaderView::Interactive);
    resultHeader->setSectionResizeMode(2, QHeaderView::Stretch);
    m_resultTable->verticalHeader()->setVisible(false);
    layout->addWidget(m_resultTable, 1);
    m_matrixLabel = new QLabel(page);
    m_matrixLabel->setProperty("panelRole", QStringLiteral("matrixSummary"));
    layout->addWidget(m_matrixLabel);
    m_qualityLabel = new QLabel(page);
    layout->addWidget(m_qualityLabel);

    QGroupBox *targetGroup = new QGroupBox(tr("生成后应用到标定转换（可多选）"), page);
    targetGroup->setObjectName(QStringLiteral("calibrationTargetTransformGroup"));
    targetGroup->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *targetLayout = new QVBoxLayout(targetGroup);
    m_targetTransformHint = new QLabel(
                tr("未勾选时仅生成标定文件，不改变任何标定转换的当前文件。"),
                targetGroup);
    m_targetTransformHint->setWordWrap(true);
    m_targetTransformHint->setProperty("role", QStringLiteral("cardHint"));
    targetLayout->addWidget(m_targetTransformHint);
    m_targetTransformList = new QListWidget(targetGroup);
    m_targetTransformList->setObjectName(
                QStringLiteral("calibrationTargetTransformList"));
    m_targetTransformList->setMaximumHeight(132);
    targetLayout->addWidget(m_targetTransformList);
    layout->addWidget(targetGroup);

    QHBoxLayout *fileRow = new QHBoxLayout;
    m_updateAfterGenerate = new QCheckBox(tr("允许覆盖同名文件"), page);
    m_updateAfterGenerate->setObjectName(
                QStringLiteral("calibrationAllowOverwriteCheck"));
    m_filePath = new QLineEdit(page);
    QPushButton *browse = new QPushButton(tr("选择路径"), page);
    QPushButton *generate = new QPushButton(tr("生成标定文件"), page);
    browse->setProperty("actionRole", QStringLiteral("secondary"));
    generate->setProperty("actionRole", QStringLiteral("highlight"));
    fileRow->addWidget(m_updateAfterGenerate);
    fileRow->addWidget(new QLabel(tr("标定文件路径"), page));
    fileRow->addWidget(m_filePath, 1);
    fileRow->addWidget(browse);
    fileRow->addWidget(generate);
    layout->addLayout(fileRow);
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString selected = QFileDialog::getSaveFileName(
                    this, tr("标定文件路径"), m_filePath->text(),
                    tr("项目标定 XML (*.xml)"));
        if (!selected.isEmpty())
            m_filePath->setText(selected);
    });
    connect(generate, &QPushButton::clicked,
            this, &QuickCalibrationWizard::generateCalibrationFile);
    return page;
}

void QuickCalibrationWizard::setStep(int step)
{
    const int previousStep = m_step;
    m_step = qBound(0, step, 3);
    if (previousStep == 3 && m_step < 3 && m_generatedFileCompleted) {
        m_generatedFileCompleted = false;
        m_generatedFilePath.clear();
        if (m_sampleStatus) {
            m_sampleStatus->setText(
                        tr("已返回标定配置；原 XML 保留为文件，但不会自动应用，需重新求解并生成"));
            m_sampleStatus->setProperty("status", QStringLiteral("idle"));
        }
    }
    if (m_step == 2 && !ensureMethodConfigWidget()) {
        m_step = 0;
        QMessageBox::warning(this, tr("标定配置"), tr("所选标定方式未提供配置页面"));
    }
    if (m_step == 2 && m_draftPersistenceEnabled && !m_draftRestoreHandled) {
        if (m_communicationSession && m_communicationSession->isRunning())
            m_communicationSession->stop();
        m_draftRestorePromptActive = true;
        promptRestoreDraft();
        m_draftRestorePromptActive = false;
    }
    m_maxVisitedStep = qMax(m_maxVisitedStep, m_step);
    m_pages->setCurrentIndex(m_step);
    if (m_step == 3 && m_resultTable && !m_resultTableWidthsInitialized) {
        QTimer::singleShot(0, m_resultTable, [this]() {
            if (!m_resultTable || m_resultTableWidthsInitialized)
                return;
            const int availableWidth = m_resultTable->viewport()->width();
            if (availableWidth <= 0)
                return;
            const int indexWidth = 72;
            const int statusWidth = qBound(220, availableWidth / 5, 320);
            QHeaderView *header = m_resultTable->horizontalHeader();
            header->resizeSection(0, indexWidth);
            header->resizeSection(
                        1, qMax(header->minimumSectionSize(),
                                availableWidth - indexWidth - statusWidth));
            m_resultTableWidthsInitialized = true;
        });
    }
    m_previousButton->setVisible(m_step > 0);
    m_nextButton->setText(m_step == 3 ? tr("关闭") : tr("下一步"));
    if (m_step == 3 && m_filePath->text().isEmpty()) {
        const QString dir = QDir(SchemeStore::instance().currentScheme().schemeDir)
                .filePath(QStringLiteral("calibrations"));
        m_filePath->setText(QDir(dir).filePath(
                                QStringLiteral("calibration_%1.xml")
                                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")))));
        showSolveResult();
    }
    updateStepHeader();
}

void QuickCalibrationWizard::updateStepHeader()
{
    for (int i = 0; i < m_stepButtons.size(); ++i) {
        QPushButton *button = m_stepButtons.at(i);
        button->setProperty("stepState", i == m_step
                            ? QStringLiteral("current")
                            : (i < m_step ? QStringLiteral("completed")
                                          : QStringLiteral("pending")));
        button->setEnabled(i <= m_maxVisitedStep);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

void QuickCalibrationWizard::setPreviewImage(const QImage &image)
{
    m_referencePreviewImage = image;
    if (m_imageModeButton && m_imageModeButton->isChecked() && m_externalImages.isEmpty())
        updateImageModeUi();
}

void QuickCalibrationWizard::importExternalImages()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
                this, tr("导入标定图片"), QString(),
                tr("图像文件 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;所有文件 (*.*)"));
    if (!paths.isEmpty())
        addExternalImageFiles(paths);
}

void QuickCalibrationWizard::addExternalImageFiles(const QStringList &filePaths)
{
    const int firstNewIndex = m_externalImages.size();
    QStringList failedFiles;
    for (const QString &path : filePaths) {
        const QString absolutePath = QFileInfo(path).absoluteFilePath();
        if (m_externalImagePaths.contains(absolutePath))
            continue;
        const QImage image(absolutePath);
        if (image.isNull()) {
            failedFiles.append(QFileInfo(path).fileName());
            continue;
        }
        m_externalImages.append(image);
        m_externalImagePaths.append(absolutePath);
    }
    m_externalImageSequenceComplete = false;
    rebuildExternalImageList(firstNewIndex < m_externalImages.size()
                             ? firstNewIndex : m_imageThumbnailList->currentRow());
    if (!failedFiles.isEmpty()) {
        QMessageBox::warning(this, tr("部分图片未导入"),
                             tr("以下文件不是有效图片：\n%1").arg(failedFiles.join(QStringLiteral("\n"))));
    }
}

void QuickCalibrationWizard::showExternalImage(int index)
{
    if (!m_imageThumbnailList || m_externalImages.isEmpty()) {
        updateImageModeUi();
        return;
    }
    index = qBound(0, index, m_externalImages.size() - 1);
    if (m_imageThumbnailList->currentRow() != index) {
        QSignalBlocker blocker(m_imageThumbnailList);
        m_imageThumbnailList->setCurrentRow(index);
    }
    updatePreviewImage(m_externalImages.at(index));
    m_imageThumbnailList->scrollToItem(m_imageThumbnailList->item(index));
    m_imageCounterLabel->setText(tr("当前：%1/%2").arg(index + 1).arg(m_externalImages.size()));
    m_sampleStatus->setText(tr("图像模式：%1").arg(QFileInfo(m_externalImagePaths.at(index)).fileName()));
    m_previousImageButton->setEnabled(index > 0);
    m_nextImageButton->setEnabled(index + 1 < m_externalImages.size());
    m_removeImageButton->setEnabled(true);
    m_clearImagesButton->setEnabled(true);
    saveDraft(nullptr);
}

void QuickCalibrationWizard::rebuildExternalImageList(int currentIndex)
{
    QSignalBlocker blocker(m_imageThumbnailList);
    m_imageThumbnailList->clear();
    for (int i = 0; i < m_externalImages.size(); ++i) {
        QListWidgetItem *item = new QListWidgetItem(
                    QIcon(QPixmap::fromImage(m_externalImages.at(i))),
                    QFileInfo(m_externalImagePaths.at(i)).fileName(), m_imageThumbnailList);
        item->setToolTip(m_externalImagePaths.at(i));
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        item->setSizeHint(QSize(150, 108));
    }
    if (!m_externalImages.isEmpty())
        m_imageThumbnailList->setCurrentRow(qBound(0, currentIndex, m_externalImages.size() - 1));
    blocker.unblock();
    updateImageModeUi();
}

void QuickCalibrationWizard::removeCurrentExternalImage()
{
    const int index = m_imageThumbnailList ? m_imageThumbnailList->currentRow() : -1;
    if (index < 0 || index >= m_externalImages.size())
        return;
    m_externalImages.removeAt(index);
    m_externalImagePaths.removeAt(index);
    m_externalImageSequenceComplete = false;
    rebuildExternalImageList(qMin(index, m_externalImages.size() - 1));
    saveDraft(nullptr);
}

void QuickCalibrationWizard::clearExternalImages()
{
    if (m_externalImages.isEmpty())
        return;
    m_externalImages.clear();
    m_externalImagePaths.clear();
    m_externalImageSequenceComplete = false;
    rebuildExternalImageList(-1);
    saveDraft(nullptr);
}

void QuickCalibrationWizard::updateImageModeUi()
{
    if (!m_imageModeButton || !m_imageModeButton->isChecked())
        return;
    if (!m_externalImages.isEmpty()) {
        showExternalImage(qMax(0, m_imageThumbnailList->currentRow()));
        return;
    }
    updatePreviewImage(m_referencePreviewImage);
    m_imageCounterLabel->setText(tr("当前：0/0"));
    m_sampleStatus->setText(m_referencePreviewImage.isNull()
                            ? tr("图像模式：请导入外部标定图片")
                            : tr("图像模式：当前基准图（可导入外部图片）"));
    m_previousImageButton->setEnabled(false);
    m_nextImageButton->setEnabled(false);
    m_removeImageButton->setEnabled(false);
    m_clearImagesButton->setEnabled(false);
}

void QuickCalibrationWizard::updatePreviewImage(const QImage &image)
{
    if (!m_previewHelper)
        return;
    m_currentLocationOverlays.clear();
    if (image.isNull()) {
        m_previewHelper->clear();
        if (m_previewEmptyLabel)
            m_previewEmptyLabel->show();
        return;
    }
    m_previewHelper->setImage(image);
    m_previewHelper->fitToView();
    if (m_previewEmptyLabel)
        m_previewEmptyLabel->hide();
    updateCalibrationOverlays();
}

bool QuickCalibrationWizard::calibrationResultPassed() const
{
    if (!m_methodConfigWidget || !m_solveResult.success)
        return false;
    if (m_methodConfigWidget->translationSampleCount() <= 0
            || m_methodConfigWidget->completedTranslationSampleCount()
               != m_methodConfigWidget->translationSampleCount()) {
        return false;
    }
    const ICalibrationMethod *method =
            CalibrationMethodRegistry::instance().method(m_methodId);
    QString validationError;
    return method && method->validateResult(m_solveResult, &validationError);
}

void QuickCalibrationWizard::updateCalibrationOverlays()
{
    if (!m_previewHelper)
        return;
    QVector<ToolOverlay> overlays = m_currentLocationOverlays;
    if (m_methodConfigWidget) {
        const bool solved = calibrationResultPassed();
        NPointCalibrationConfigWidget *nPoint =
                dynamic_cast<NPointCalibrationConfigWidget *>(m_methodConfigWidget);
        if (m_solveResult.success) {
            if (nPoint) {
                const bool regionReady = m_solveResult.model.validRegion.size() >= 3
                        && m_solveResult.model.safeRegion.size() >= 3;
                nPoint->setRegionSummary(
                            m_solveResult.model.validRegion.size(),
                            m_solveResult.model.safeRegion.size(),
                            regionReady
                            ? tr("已生成：安全内缩 %1 px")
                              .arg(m_solveResult.model.safeMarginPx, 0, 'f', 2)
                            : tr("有效区域结构无效"),
                            regionReady
                            ? (solved ? QStringLiteral("ok")
                                      : QStringLiteral("warning"))
                            : QStringLiteral("error"));
            }

            const QVector<QPointF> &validRegion = m_solveResult.model.validRegion;
            const QVector<QPointF> &safeRegion = m_solveResult.model.safeRegion;
            if (validRegion.size() >= 3 && safeRegion.size() >= 3) {
                ToolOverlay boundaryBand;
                boundaryBand.type = ToolOverlayType::Polygon;
                boundaryBand.points = validRegion;
                boundaryBand.label = tr("Boundary 警戒带");
                boundaryBand.extra.insert(QStringLiteral("displayRole"),
                                          QStringLiteral("calibration_boundary_band"));
                boundaryBand.extra.insert(QStringLiteral("clipGeometry"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("polygon")},
                    {QStringLiteral("points"), overlayPointArray(safeRegion)}
                });
                overlays.append(boundaryBand);

                ToolOverlay safeOverlay;
                safeOverlay.type = ToolOverlayType::Polygon;
                safeOverlay.points = safeRegion;
                safeOverlay.label = tr("SafeROI（生产安全区）");
                safeOverlay.extra.insert(QStringLiteral("displayRole"),
                                         QStringLiteral("calibration_safe_roi"));
                overlays.append(safeOverlay);

                ToolOverlay validOutline;
                validOutline.type = ToolOverlayType::Polygon;
                validOutline.points = validRegion;
                validOutline.label = tr("ValidROI（平移点凸包）");
                validOutline.extra.insert(QStringLiteral("displayRole"),
                                          QStringLiteral("calibration_valid_roi"));
                overlays.append(validOutline);
            }
        } else if (nPoint && (!m_solveResult.message.trimmed().isEmpty()
                              || !m_solveResult.status.trimmed().isEmpty())) {
            nPoint->setRegionSummary(
                        m_solveResult.model.validRegion.size(),
                        m_solveResult.model.safeRegion.size(),
                        m_solveResult.message.trimmed().isEmpty()
                        ? m_solveResult.status : m_solveResult.message,
                        QStringLiteral("error"));
        }
        const QVector<QLineF> segments =
                m_methodConfigWidget->completedTranslationSegments();
        overlays.reserve(overlays.size() + segments.size());
        for (const QLineF &segment : segments) {
            ToolOverlay overlay;
            overlay.type = ToolOverlayType::Line;
            overlay.p1 = segment.p1();
            overlay.p2 = segment.p2();
            overlay.label = solved ? tr("平移标定路径（已完成）")
                                   : tr("平移标定路径（采集中）");
            overlay.extra.insert(
                        QStringLiteral("displayRole"),
                        solved
                        ? QStringLiteral("calibration_translation_path_solved")
                        : QStringLiteral("calibration_translation_path_pending"));
            overlay.extra.insert(QStringLiteral("emphasis"),
                                 QStringLiteral("active"));
            overlays.append(overlay);
        }
    }
    m_previewHelper->setToolOverlays(overlays);
}

bool QuickCalibrationWizard::validateCoordinateSourceFingerprint(
        const QJsonObject &fingerprint,
        QString *errorMessage) const
{
    if (errorMessage)
        errorMessage->clear();
    QString invalidField;
    if (!CalibrationSourceFingerprint::isComplete(fingerprint, &invalidField)) {
        if (errorMessage)
            *errorMessage = tr("坐标来源指纹不完整：%1").arg(invalidField);
        return false;
    }
    const QString producerId = fingerprint.value(
                QStringLiteral("producerId")).toString().trimmed();
    const auto configIt = m_calibrationProducerConfigs.constFind(producerId);
    if (configIt == m_calibrationProducerConfigs.cend()
            || configIt->toolType != ToolType::TemplateLocation
            || !configIt->enabled) {
        if (errorMessage)
            *errorMessage = tr("标定时使用的模板定位来源已不存在或被禁用");
        return false;
    }

    const ToolConfig &currentConfig = configIt.value();
    cv::Mat currentReferenceFrame =
            ReferenceImageProvider::instance().referenceFrame();
    if (currentReferenceFrame.empty())
        currentReferenceFrame = qImageToBgrMat(m_referencePreviewImage);
    QJsonObject currentPayload{
        {QStringLiteral("originMode"),
         currentConfig.params.value(QStringLiteral("originMode"))
         .toString(QStringLiteral("centroid"))},
        {QStringLiteral("customOriginNormalized"),
         currentConfig.params.value(
             QStringLiteral("customOriginNormalized")).toObject()},
        // The model signature is deterministic for an unchanged template
        // configuration and reference image.  The surrounding comparison also
        // checks both identities before this saved value can be reused.
        {QStringLiteral("modelSignature"),
         fingerprint.value(QStringLiteral("modelSignature")).toString()},
        {QStringLiteral("coordinateSourceConfigSignature"),
         CalibrationSourceFingerprint::coordinateSourceConfigSignature(
             currentConfig)},
        {QStringLiteral("coordinateSourceReferenceSignature"),
         CalibrationSourceFingerprint::imageSignature(
             currentReferenceFrame)}
    };
    const QJsonObject currentFingerprint =
            CalibrationSourceFingerprint::makeFingerprint(
                producerId, currentConfig.toolType, currentPayload);
    QString mismatchField;
    if (!CalibrationSourceFingerprint::matches(
                fingerprint, currentFingerprint, &mismatchField)) {
        if (errorMessage) {
            *errorMessage = tr("当前模板、原点、基准图或来源配置已变化（%1）")
                    .arg(mismatchField);
        }
        return false;
    }
    return true;
}

void QuickCalibrationWizard::solveCalibration()
{
    // Every solve attempt owns a fresh result.  Early validation failures must
    // never leave the previous successful model available to the Next action.
    m_solveResult = CalibrationSolveResult();
    updateCalibrationOverlays();
    if (!m_persistentConfigurationError.isEmpty()) {
        QMessageBox::warning(this, tr("求解已禁止"),
                             m_persistentConfigurationError);
        return;
    }
    const ICalibrationMethod *method = CalibrationMethodRegistry::instance().method(m_methodId);
    if (!method) {
        QMessageBox::warning(this, tr("求解失败"), tr("标定方式不存在"));
        return;
    }
    QString error;
    if (!ensureMethodConfigWidget())
        return;
    CalibrationDraft draft = m_methodConfigWidget->draft(&error);
    if (!method->validateDraft(draft, &error)) {
        QMessageBox::warning(this, tr("求解失败"), error);
        return;
    }

    QImage calibrationImage;
    if (m_cameraModeButton && m_cameraModeButton->isChecked()) {
        calibrationImage = CameraFrameProvider::instance().currentImage();
    } else if (!m_externalImages.isEmpty()) {
        const QSize expectedSize = m_externalImages.first().size();
        for (const QImage &image : std::as_const(m_externalImages)) {
            if (image.size() != expectedSize) {
                QMessageBox::warning(this, tr("求解失败"),
                                     tr("外部标定图片尺寸不一致，无法建立稳定图像空间绑定"));
                return;
            }
        }
        const int currentIndex = m_imageThumbnailList
                ? qBound(0, m_imageThumbnailList->currentRow(), m_externalImages.size() - 1)
                : 0;
        calibrationImage = m_externalImages.at(currentIndex);
    } else {
        calibrationImage = m_referencePreviewImage;
    }
    if (method->capabilities().requiresImage && calibrationImage.isNull()) {
        QMessageBox::warning(this, tr("求解失败"),
                             tr("当前标定方式需要有效标定图像，无法建立图像空间绑定"));
        return;
    }
    if (!calibrationImage.isNull()) {
        QJsonObject imageBinding = draft.parameters
                .value(QStringLiteral("imageBinding")).toObject();
        imageBinding.insert(QStringLiteral("inputFingerprint"), QJsonObject{
                                {QStringLiteral("version"), 1},
                                {QStringLiteral("width"), calibrationImage.width()},
                                {QStringLiteral("height"), calibrationImage.height()}});
        const QString captureMode = draft.parameters
                .value(QStringLiteral("captureBindings")).toObject()
                .value(QStringLiteral("captureMode"))
                .toString(QStringLiteral("trigger"));
        if (captureMode == QStringLiteral("manual")) {
            imageBinding.insert(QStringLiteral("coordinateSourceFingerprint"),
                                QJsonObject{{QStringLiteral("mode"),
                                             QStringLiteral("manual")}});
        } else {
            QString sourceError;
            if (!validateCoordinateSourceFingerprint(
                        m_lockedCoordinateSourceFingerprint, &sourceError)) {
                QMessageBox::warning(
                            this, tr("求解失败"),
                            tr("标定点来源无法继续使用：%1，请清空后重新采集")
                            .arg(sourceError));
                return;
            }
            imageBinding.insert(QStringLiteral("coordinateSourceFingerprint"),
                                m_lockedCoordinateSourceFingerprint);
        }
        draft.parameters.insert(QStringLiteral("imageBinding"), imageBinding);
    }
    m_solveResult = method->solve(draft);
    updateCalibrationOverlays();
    if (!m_solveResult.success) {
        QMessageBox::warning(this, tr("求解失败"), m_solveResult.message);
        return;
    }
    QMessageBox::information(this, tr("标定结果"), m_solveResult.message);
}

void QuickCalibrationWizard::showSolveResult()
{
    if (!m_solveResult.success)
        return;
    m_resultTable->setRowCount(m_sessionLog.size() + m_solveResult.model.samples.size());
    int outputRow = 0;
    for (const QJsonObject &entry : m_sessionLog) {
        m_resultTable->setItem(outputRow, 0, new QTableWidgetItem(
                                   QString::number(outputRow + 1)));
        const QString timestamp = entry.value(QStringLiteral("timestamp")).toString();
        const QString exchange = entry.value(QStringLiteral("response")).toString().isEmpty()
                ? entry.value(QStringLiteral("raw")).toString()
                : tr("输入: %1  输出: %2")
                  .arg(entry.value(QStringLiteral("raw")).toString(),
                       entry.value(QStringLiteral("response")).toString());
        m_resultTable->setItem(outputRow, 1, new QTableWidgetItem(
                                   tr("[%1] %2").arg(timestamp, exchange)));
        m_resultTable->setItem(outputRow, 2, new QTableWidgetItem(
                                   entry.value(QStringLiteral("valid")).toBool()
                                   ? tr("通信解析通过")
                                   : entry.value(QStringLiteral("error")).toString()));
        ++outputRow;
    }
    for (int row = 0; row < m_solveResult.model.samples.size(); ++row) {
        const CalibrationSample &sample = m_solveResult.model.samples.at(row);
        m_resultTable->setItem(outputRow, 0, new QTableWidgetItem(QString::number(outputRow + 1)));
        m_resultTable->setItem(outputRow, 1, new QTableWidgetItem(
                                   tr("点%1：C=%2,R=%3 → X=%4,Y=%5")
                                   .arg(sample.index)
                                   .arg(sample.column).arg(sample.row)
                                   .arg(sample.machineX).arg(sample.machineY)));
        m_resultTable->setItem(outputRow, 2, new QTableWidgetItem(
                                   tr("残差 %1 mm").arg(sample.residual, 0, 'f', 6)));
        ++outputRow;
    }
    m_matrixLabel->setText(tr("标定矩阵\n%1\n\n逆矩阵\n%2")
                           .arg(matrixLine(m_solveResult.model.forward),
                                matrixLine(m_solveResult.model.inverse)));
    const CalibrationQuality &quality = m_solveResult.model.quality;
    const ICalibrationMethod *method = CalibrationMethodRegistry::instance().method(m_methodId);
    const QJsonObject methodSummary = method
            ? method->createResultSummary(m_solveResult) : QJsonObject();
    m_qualityLabel->setText(tr("方式=%1  模型=%2  Mean=%3 mm  RMSE=%4 mm  Max=%5 mm  |  %6")
                            .arg(methodSummary.value(QStringLiteral("methodId"))
                                 .toString(m_solveResult.model.methodId))
                            .arg(methodSummary.value(QStringLiteral("modelType"))
                                 .toString(m_solveResult.model.modelType))
                            .arg(quality.meanError, 0, 'f', 6)
                            .arg(quality.rmse, 0, 'f', 6)
                            .arg(quality.maxError, 0, 'f', 6)
                            .arg(quality.passed ? tr("通过") : tr("超限")));
}

QString QuickCalibrationWizard::configurationTargetKey() const
{
    QStringList ids = targetCalibrationTransformIds();
    ids.sort();
    return ids.isEmpty() ? QStringLiteral("unassigned") : ids.first();
}

QString QuickCalibrationWizard::draftTargetKey() const
{
    QStringList ids = targetCalibrationTransformIds();
    ids.sort();
    return ids.isEmpty()
            ? QStringLiteral("unassigned")
            : QStringLiteral("targets:%1").arg(ids.join(QLatin1Char('|')));
}

QJsonObject QuickCalibrationWizard::communicationSettings() const
{
    QJsonObject addresses{
        {QStringLiteral("tcpClient"),
         m_communicationHost->property("tcpClientAddress").toString()},
        {QStringLiteral("tcpServer"),
         m_communicationHost->property("tcpServerAddress").toString()},
        {QStringLiteral("udpBind"),
         m_communicationHost->property("udpBindAddress").toString()}
    };
    const QString transport = m_communicationType->currentData().toString();
    if (transport == QStringLiteral("tcp_client"))
        addresses.insert(QStringLiteral("tcpClient"), m_communicationHost->text().trimmed());
    else if (transport == QStringLiteral("tcp_server"))
        addresses.insert(QStringLiteral("tcpServer"), m_communicationHost->text().trimmed());
    else if (transport == QStringLiteral("udp"))
        addresses.insert(QStringLiteral("udpBind"), m_communicationHost->text().trimmed());

    return QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("transport"), transport},
        {QStringLiteral("host"), m_communicationHost->text().trimmed()},
        {QStringLiteral("addresses"), addresses},
        {QStringLiteral("port"), m_communicationPort->value()},
        {QStringLiteral("startSignal"), m_startSignal->text()},
        {QStringLiteral("captureSignal"), m_calibrationSignal->text()},
        {QStringLiteral("endSignal"), m_endSignal->text()},
        {QStringLiteral("delimiter"), m_delimiter->text()},
        {QStringLiteral("terminator"), m_terminator->text()},
        {QStringLiteral("startOk"), m_startOk->text()},
        {QStringLiteral("startNg"), m_startNg->text()},
        {QStringLiteral("captureOk"), m_captureOk->text()},
        {QStringLiteral("captureNg"), m_captureNg->text()},
        {QStringLiteral("endOk"), m_endOk->text()},
        {QStringLiteral("endNg"), m_endNg->text()},
        {QStringLiteral("fieldMapping"), QJsonObject{
             {QStringLiteral("x"), m_xField->value()},
             {QStringLiteral("y"), m_yField->value()},
             {QStringLiteral("angle"), m_angleField->value()}
         }}
    };
}

void QuickCalibrationWizard::restoreCommunicationSettings(
        const QJsonObject &settings)
{
    if (settings.isEmpty())
        return;
    const QJsonObject addresses = settings.value(QStringLiteral("addresses")).toObject();
    if (addresses.value(QStringLiteral("tcpClient")).isString())
        m_communicationHost->setProperty(
                    "tcpClientAddress",
                    addresses.value(QStringLiteral("tcpClient")).toString());
    if (addresses.value(QStringLiteral("tcpServer")).isString())
        m_communicationHost->setProperty(
                    "tcpServerAddress",
                    addresses.value(QStringLiteral("tcpServer")).toString());
    if (addresses.value(QStringLiteral("udpBind")).isString())
        m_communicationHost->setProperty(
                    "udpBindAddress",
                    addresses.value(QStringLiteral("udpBind")).toString());

    const QString transport = settings.value(QStringLiteral("transport"))
            .toString(QStringLiteral("none"));
    const int transportIndex = m_communicationType->findData(transport);
    if (transportIndex >= 0)
        m_communicationType->setCurrentIndex(transportIndex);
    if (settings.value(QStringLiteral("host")).isString())
        m_communicationHost->setText(settings.value(QStringLiteral("host")).toString());
    if (settings.value(QStringLiteral("port")).isDouble())
        m_communicationPort->setValue(settings.value(QStringLiteral("port")).toInt());

    const auto restoreText = [&settings](const QString &key, QLineEdit *edit) {
        if (settings.value(key).isString())
            edit->setText(settings.value(key).toString());
    };
    restoreText(QStringLiteral("startSignal"), m_startSignal);
    restoreText(QStringLiteral("captureSignal"), m_calibrationSignal);
    restoreText(QStringLiteral("endSignal"), m_endSignal);
    restoreText(QStringLiteral("delimiter"), m_delimiter);
    restoreText(QStringLiteral("terminator"), m_terminator);
    restoreText(QStringLiteral("startOk"), m_startOk);
    restoreText(QStringLiteral("startNg"), m_startNg);
    restoreText(QStringLiteral("captureOk"), m_captureOk);
    restoreText(QStringLiteral("captureNg"), m_captureNg);
    restoreText(QStringLiteral("endOk"), m_endOk);
    restoreText(QStringLiteral("endNg"), m_endNg);
    const QJsonObject mapping = settings.value(QStringLiteral("fieldMapping")).toObject();
    if (mapping.value(QStringLiteral("x")).isDouble())
        m_xField->setValue(mapping.value(QStringLiteral("x")).toInt());
    if (mapping.value(QStringLiteral("y")).isDouble())
        m_yField->setValue(mapping.value(QStringLiteral("y")).toInt());
    if (mapping.value(QStringLiteral("angle")).isDouble())
        m_angleField->setValue(mapping.value(QStringLiteral("angle")).toInt());
}

void QuickCalibrationWizard::setPersistentConfiguration(
        const QJsonObject &configuration)
{
    m_draftPersistenceEnabled = true;
    m_loadedPersistentConfiguration = configuration;
    m_persistentConfigurationWritable = true;
    m_persistentConfigurationError.clear();
    const int version = configuration.value(QStringLiteral("version")).toInt(1);
    if (!configuration.isEmpty() && version > 1) {
        m_persistentConfigurationWritable = false;
        m_persistentConfigurationError = tr(
                    "当前方案的快速标定配置版本为 %1，高于本程序支持版本；已禁止求解和生成，请使用兼容版本处理。")
                .arg(version);
        if (m_communicationStatus)
            m_communicationStatus->setText(m_persistentConfigurationError);
        if (m_sampleStatus)
            m_sampleStatus->setText(m_persistentConfigurationError);
        qWarning() << "[QuickCalibrationWizard] 快速标定配置版本高于当前程序，保持只读:"
                   << version;
        return;
    }
    restoreCommunicationSettings(
                configuration.value(QStringLiteral("communication")).toObject());
    if (configuration.contains(QStringLiteral("lastTargetToolIds"))
            && configuration.value(QStringLiteral("lastTargetToolIds")).isArray()
            && m_targetTransformList) {
        QSet<QString> savedIds;
        const QJsonArray savedTargets = configuration.value(
                    QStringLiteral("lastTargetToolIds")).toArray();
        for (const QJsonValue &value : savedTargets) {
            if (value.isString() && !value.toString().trimmed().isEmpty())
                savedIds.insert(value.toString().trimmed());
        }
        for (int row = 0; row < m_targetTransformList->count(); ++row) {
            QListWidgetItem *item = m_targetTransformList->item(row);
            const QString id = item->data(Qt::UserRole).toString();
            if (!id.isEmpty())
                item->setCheckState(savedIds.contains(id)
                                    ? Qt::Checked : Qt::Unchecked);
        }
    }
    const QJsonObject targets = configuration.value(QStringLiteral("targets")).toObject();
    const QJsonObject target = targets.value(configurationTargetKey()).toObject();
    m_pendingMethodSettings = target.value(QStringLiteral("nPoint")).toObject();
    if (m_pendingMethodSettings.value(QStringLiteral("version")).toInt(1) > 1) {
        m_pendingMethodSettings = QJsonObject();
        m_persistentConfigurationWritable = false;
        m_persistentConfigurationError = tr(
                    "当前方案的 N 点配置版本高于本程序支持版本；已禁止求解和生成，请使用兼容版本处理。");
        if (m_communicationStatus)
            m_communicationStatus->setText(m_persistentConfigurationError);
        if (m_sampleStatus)
            m_sampleStatus->setText(m_persistentConfigurationError);
        qWarning() << "[QuickCalibrationWizard] N点稳定配置版本高于当前程序，保持只读";
        return;
    }
    if (NPointCalibrationConfigWidget *nPoint =
            dynamic_cast<NPointCalibrationConfigWidget *>(m_methodConfigWidget)) {
        QString restoreError;
        if (!nPoint->restorePersistentSettings(
                    m_pendingMethodSettings, &restoreError)) {
            m_persistentConfigurationWritable = false;
            m_persistentConfigurationError = tr(
                        "当前方案的 N 点稳定配置无效：%1。已禁止求解和生成，请修复方案配置。")
                    .arg(restoreError);
            if (m_communicationStatus)
                m_communicationStatus->setText(m_persistentConfigurationError);
            if (m_sampleStatus)
                m_sampleStatus->setText(m_persistentConfigurationError);
            qWarning() << "[QuickCalibrationWizard] N点稳定配置无效，保持原配置不覆盖:"
                       << restoreError;
        }
    }
}

QJsonObject QuickCalibrationWizard::persistentConfiguration() const
{
    if (!m_persistentConfigurationWritable)
        return m_loadedPersistentConfiguration;
    QJsonObject configuration = m_loadedPersistentConfiguration;
    configuration.insert(QStringLiteral("version"), 1);
    configuration.insert(QStringLiteral("communication"), communicationSettings());
    configuration.insert(QStringLiteral("lastMethodId"), m_methodId);
    configuration.insert(QStringLiteral("lastTargetKey"), configurationTargetKey());
    QJsonArray selectedTargetIds;
    QStringList targetIds = targetCalibrationTransformIds();
    targetIds.sort();
    for (const QString &id : std::as_const(targetIds))
        selectedTargetIds.append(id);
    configuration.insert(QStringLiteral("lastTargetToolIds"), selectedTargetIds);

    QJsonObject methodSettings = m_pendingMethodSettings;
    if (const NPointCalibrationConfigWidget *nPoint =
            dynamic_cast<const NPointCalibrationConfigWidget *>(m_methodConfigWidget)) {
        methodSettings = nPoint->persistentSettings();
    }
    if (!methodSettings.isEmpty()) {
        QJsonObject targets = configuration.value(QStringLiteral("targets")).toObject();
        QStringList settingsTargets = targetIds;
        if (settingsTargets.isEmpty())
            settingsTargets.append(QStringLiteral("unassigned"));
        for (const QString &targetId : std::as_const(settingsTargets)) {
            QJsonObject target = targets.value(targetId).toObject();
            target.insert(QStringLiteral("methodId"), QStringLiteral("n_point"));
            target.insert(QStringLiteral("nPoint"), methodSettings);
            targets.insert(targetId, target);
        }
        configuration.insert(QStringLiteral("targets"), targets);
    }
    return configuration;
}

CalibrationCommunicationConfig QuickCalibrationWizard::communicationConfig() const
{
    CalibrationCommunicationConfig config;
    config.transport = m_communicationType->currentData().toString();
    if (config.transport.isEmpty())
        config.transport = QStringLiteral("none");
    config.host = m_communicationHost->text().trimmed();
    config.port = static_cast<quint16>(m_communicationPort->value());
    config.startSignal = m_startSignal->text();
    config.captureSignal = m_calibrationSignal->text();
    config.endSignal = m_endSignal->text();
    config.delimiter = m_delimiter->text();
    config.terminator = m_terminator->text();
    config.xField = m_xField->value();
    config.yField = m_yField->value();
    config.angleField = m_angleField->value();
    config.startOk = m_startOk->text();
    config.startNg = m_startNg->text();
    config.captureOk = m_captureOk->text();
    config.captureNg = m_captureNg->text();
    config.endOk = m_endOk->text();
    config.endNg = m_endNg->text();
    return config;
}

bool QuickCalibrationWizard::validateCommunicationPage()
{
    if (!m_persistentConfigurationError.isEmpty()) {
        QMessageBox::warning(this, tr("快速标定配置不兼容"),
                             m_persistentConfigurationError);
        return false;
    }
    const ICalibrationMethod *method = CalibrationMethodRegistry::instance().method(m_methodId);
    if (method && method->capabilities().requiresCommunication
            && m_communicationType->currentIndex() == 0) {
        QMessageBox::warning(this, tr("通信配置"),
                             tr("所选标定方式要求配置机械通信，不能使用无设备模式"));
        return false;
    }
    QString error;
    if (!CalibrationCommunicationProtocol::validate(communicationConfig(), &error)) {
        QMessageBox::warning(this, tr("通信配置"), error);
        return false;
    }
    return true;
}

void QuickCalibrationWizard::testCommunicationMessage()
{
    const CalibrationCommunicationMessage message =
            CalibrationCommunicationProtocol::parse(m_testMessage->text(),
                                                    communicationConfig());
    m_sessionLog.append(message.logEntry);
    m_lastCommunicationMessage = message;
    if (m_methodConfigWidget)
        m_methodConfigWidget->setLatestPhysicalSample(
                    message.valid && message.event == CalibrationCommunicationEvent::Capture,
                    message.x, message.y, message.angleDeg);
    m_communicationStatus->setText(message.valid
            ? tr("通过：X=%1 Y=%2 Angle=%3")
              .arg(message.x).arg(message.y).arg(message.angleDeg)
            : tr("失败：%1").arg(message.error));
}

void QuickCalibrationWizard::toggleCommunicationSession()
{
    if (m_communicationSession->isRunning()) {
        m_communicationSession->stop();
        return;
    }
    if (!m_persistentConfigurationError.isEmpty()) {
        QMessageBox::warning(this, tr("启动通信已禁止"),
                             m_persistentConfigurationError);
        return;
    }
    QString error;
    if (!m_communicationSession->start(communicationConfig(), &error))
        QMessageBox::warning(this, tr("启动通信失败"), error);
}

bool QuickCalibrationWizard::handleCommunicationMessage(
        CalibrationCommunicationMessage *message, QString *errorMessage)
{
    if (!message || message->event != CalibrationCommunicationEvent::Capture)
        return true;
    if (errorMessage)
        errorMessage->clear();
    const auto showCaptureFailure = [this, errorMessage](const QString &fallback) {
        if (!m_sampleStatus)
            return;
        const QString detail = errorMessage && !errorMessage->isEmpty()
                ? *errorMessage : fallback;
        m_sampleStatus->setText(detail);
        m_sampleStatus->setProperty("status", QStringLiteral("error"));
        m_sampleStatus->style()->unpolish(m_sampleStatus);
        m_sampleStatus->style()->polish(m_sampleStatus);
    };
    if (m_draftRestorePromptActive || m_restoringDraft) {
        if (errorMessage)
            *errorMessage = tr("正在确认或恢复未完成标定，暂不接受触发采样");
        showCaptureFailure(tr("正在恢复标定草稿"));
        return false;
    }
    if (!m_persistentConfigurationError.isEmpty()) {
        if (errorMessage)
            *errorMessage = m_persistentConfigurationError;
        showCaptureFailure(tr("快速标定配置不兼容"));
        return false;
    }
    if (m_step != 2) {
        if (errorMessage)
            *errorMessage = tr("当前不在标定配置页，不能执行触发采样");
        return false;
    }
    if (!m_methodConfigWidget) {
        if (errorMessage)
            *errorMessage = tr("尚未进入标定配置页，不能执行触发采样");
        return false;
    }
    if (m_imageModeButton && m_imageModeButton->isChecked()
            && !m_externalImages.isEmpty() && m_externalImageSequenceComplete) {
        if (errorMessage)
            *errorMessage = tr("外部图片序列已采集完成，请重新选择起始图片或导入新图片");
        showCaptureFailure(tr("外部图片序列已采集完成"));
        return false;
    }

    m_methodConfigWidget->setLatestPhysicalSample(true, message->x,
                                                   message->y, message->angleDeg);
    if (!runCurrentImageLocation(errorMessage)) {
        showCaptureFailure(tr("当前图片模板定位失败"));
        return false;
    }
    if (!m_methodConfigWidget->captureCurrentSample(errorMessage)) {
        showCaptureFailure(tr("当前标定点采样失败"));
        return false;
    }

    if (m_pendingCaptureCameraFrameIndex >= 0)
        m_lastCapturedCameraFrameIndex = m_pendingCaptureCameraFrameIndex;
    m_pendingCaptureCameraFrameIndex = -1;
    m_solveResult = CalibrationSolveResult();
    advanceExternalImageAfterCapture();
    QString draftError;
    if (!saveDraft(&draftError) && !draftError.isEmpty())
        qWarning() << "[QuickCalibrationWizard] 采点后保存草稿失败:" << draftError;
    return true;
}

bool QuickCalibrationWizard::runCurrentImageLocation(QString *errorMessage)
{
    m_pendingCaptureCameraFrameIndex = -1;
    m_currentLocationOverlays.clear();
    updateCalibrationOverlays();
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };
    if (!m_methodConfigWidget)
        return fail(tr("标定配置尚未初始化"));

    const QString producerId = m_methodConfigWidget->captureImageProducerId();
    if (producerId.isEmpty()) {
        return fail(tr("图像点 X、Y、角度必须绑定到同一个模板定位工具"));
    }
    const auto configIt = m_calibrationProducerConfigs.constFind(producerId);
    if (configIt == m_calibrationProducerConfigs.constEnd())
        return fail(tr("绑定的模板定位工具不存在或已禁用"));

    cv::Mat currentFrame;
    QString frameId;
    int externalImageIndex = -1;
    QString imagePath;
    if (m_cameraModeButton && m_cameraModeButton->isChecked()) {
        const CameraFrameSnapshot snapshot =
                CameraFrameProvider::instance().currentFrameSnapshot();
        currentFrame = snapshot.frame;
        if (!currentFrame.empty()
                && snapshot.frameIndex <= m_lastCapturedCameraFrameIndex) {
            return fail(tr("当前相机帧已经采样，请等待新图像后再触发"));
        }
        m_pendingCaptureCameraFrameIndex = snapshot.frameIndex;
        frameId = QStringLiteral("calibration-camera-%1")
                .arg(snapshot.frameIndex);
    } else if (!m_externalImages.isEmpty()) {
        externalImageIndex = m_imageThumbnailList
                ? m_imageThumbnailList->currentRow() : 0;
        if (externalImageIndex < 0 || externalImageIndex >= m_externalImages.size())
            externalImageIndex = 0;
        currentFrame = qImageToBgrMat(m_externalImages.at(externalImageIndex));
        imagePath = m_externalImagePaths.value(externalImageIndex);
        frameId = QStringLiteral("calibration-image-%1-%2")
                .arg(externalImageIndex + 1)
                .arg(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    } else {
        currentFrame = qImageToBgrMat(m_referencePreviewImage);
        frameId = QStringLiteral("calibration-reference-%1")
                .arg(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
    }
    if (currentFrame.empty())
        return fail(tr("当前没有可用于模板定位的图像"));

    cv::Mat referenceFrame = ReferenceImageProvider::instance().referenceFrame();
    if (referenceFrame.empty())
        referenceFrame = qImageToBgrMat(m_referencePreviewImage);
    if (referenceFrame.empty())
        return fail(tr("当前方案没有有效基准图，无法运行模板定位"));

    ToolRequest request;
    request.requestId = frameId;
    request.frameId = frameId;
    request.config = configIt.value();
    request.image = currentFrame;
    request.referenceImage = referenceFrame;
    request.imagePath = imagePath;
    const QString source = externalImageIndex >= 0
            ? QStringLiteral("calibration_external_image")
            : (m_cameraModeButton && m_cameraModeButton->isChecked()
               ? QStringLiteral("calibration_camera")
               : QStringLiteral("calibration_reference_image"));
    request.runtimeContext = QJsonObject{
        {QStringLiteral("frameId"), frameId},
        {QStringLiteral("source"), source},
        {QStringLiteral("imageIndex"), externalImageIndex}
    };
    const ToolResult result = m_captureToolEngine.runTool(request);
    const bool validPose = result.success && result.ok
            && finitePayloadNumber(result.payload, QStringLiteral("x"))
            && finitePayloadNumber(result.payload, QStringLiteral("y"))
            && finitePayloadNumber(result.payload, QStringLiteral("angle"));

    if (validPose) {
        const QJsonObject currentFingerprint =
                CalibrationSourceFingerprint::makeFingerprint(
                    producerId, ToolType::TemplateLocation, result.payload);
        QString fingerprintError;
        if (!CalibrationSourceFingerprint::isComplete(
                    currentFingerprint, &fingerprintError)) {
            return fail(tr("模板定位未返回完整坐标来源身份（%1），禁止采样")
                        .arg(fingerprintError));
        }
        if (!m_lockedCoordinateSourceFingerprint.isEmpty()) {
            QString mismatchField;
            if (!CalibrationSourceFingerprint::matches(
                        m_lockedCoordinateSourceFingerprint,
                        currentFingerprint,
                        &mismatchField)) {
                return fail(tr("模板、原点、基准图或来源配置已变化（%1），"
                               "请清空点表后重新采样")
                            .arg(mismatchField));
            }
        } else {
            m_lockedCoordinateSourceFingerprint = currentFingerprint;
        }
    }

    bool producerUpdated = false;
    for (CalibrationProducerSnapshot &snapshot : m_calibrationProducerSnapshots) {
        if (snapshot.producerId != producerId)
            continue;
        snapshot.valid = validPose;
        snapshot.payload = result.payload;
        producerUpdated = true;
        break;
    }
    if (!producerUpdated) {
        CalibrationProducerSnapshot snapshot;
        snapshot.producerId = producerId;
        snapshot.displayName = configIt.value().displayName.trimmed().isEmpty()
                ? tr("模板定位") : configIt.value().displayName;
        snapshot.valid = validPose;
        snapshot.payload = result.payload;
        m_calibrationProducerSnapshots.append(snapshot);
    }
    m_methodConfigWidget->setProducerSnapshots(m_calibrationProducerSnapshots);
    m_currentLocationOverlays = result.overlays;
    updateCalibrationOverlays();

    if (!result.success || !result.ok) {
        const QString detail = result.message.trimmed().isEmpty()
                ? result.status : result.message;
        return fail(tr("当前图片模板定位失败：%1").arg(detail));
    }
    if (!validPose)
        return fail(tr("当前图片模板定位未返回有效的 X、Y、角度"));
    return true;
}

void QuickCalibrationWizard::advanceExternalImageAfterCapture()
{
    if (!m_imageModeButton || !m_imageModeButton->isChecked()
            || !m_imageThumbnailList || m_externalImages.isEmpty()) {
        return;
    }

    const int currentIndex = m_imageThumbnailList->currentRow();
    if (currentIndex < 0)
        return;
    if (currentIndex + 1 < m_externalImages.size()) {
        m_externalImageSequenceComplete = false;
        showExternalImage(currentIndex + 1);
        if (m_sampleStatus) {
            m_sampleStatus->setText(
                        tr("第 %1 张采样成功，已自动切换至第 %2/%3 张")
                        .arg(currentIndex + 1)
                        .arg(currentIndex + 2)
                        .arg(m_externalImages.size()));
            m_sampleStatus->setProperty("status", QStringLiteral("ok"));
            m_sampleStatus->style()->unpolish(m_sampleStatus);
            m_sampleStatus->style()->polish(m_sampleStatus);
        }
    } else {
        m_externalImageSequenceComplete = true;
        if (!m_sampleStatus)
            return;
        m_sampleStatus->setText(tr("第 %1 张采样成功，外部图片序列已完成")
                                .arg(currentIndex + 1));
        m_sampleStatus->setProperty("status", QStringLiteral("ok"));
        m_sampleStatus->style()->unpolish(m_sampleStatus);
        m_sampleStatus->style()->polish(m_sampleStatus);
    }
}

QString QuickCalibrationWizard::draftFilePath() const
{
    const QString schemeDir = SchemeStore::instance().currentScheme().schemeDir;
    if (schemeDir.trimmed().isEmpty())
        return QString();
    const QByteArray keyHash = QCryptographicHash::hash(
                draftTargetKey().toUtf8(), QCryptographicHash::Sha256)
            .toHex().left(16);
    const QString draftDir = QDir(schemeDir).filePath(
                QStringLiteral("calibrations/drafts"));
    return QDir(draftDir).filePath(
                QStringLiteral("n_point_%1.json")
                .arg(QString::fromLatin1(keyHash)));
}

bool QuickCalibrationWizard::saveDraft(QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    if (!m_draftPersistenceEnabled || m_restoringDraft
            || m_generatedFileCompleted)
        return true;
    NPointCalibrationConfigWidget *nPoint =
            dynamic_cast<NPointCalibrationConfigWidget *>(m_methodConfigWidget);
    if (!nPoint)
        return true;

    QString pointError;
    nPoint->draft(&pointError);
    if (!pointError.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("标定点表包含无效数据，草稿未覆盖：%1").arg(pointError);
        return false;
    }

    const QString path = draftFilePath();
    if (path.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("当前方案目录为空，无法保存标定草稿");
        return false;
    }
    if (nPoint->completedSampleCount() == 0 && m_externalImagePaths.isEmpty()) {
        QFile::remove(path);
        if (!m_lastSavedDraftPath.isEmpty() && m_lastSavedDraftPath != path)
            QFile::remove(m_lastSavedDraftPath);
        m_lastSavedDraftPath.clear();
        return true;
    }

    QJsonArray images;
    for (int index = 0; index < m_externalImagePaths.size(); ++index) {
        const QString imagePath = m_externalImagePaths.at(index);
        const QImage image = index < m_externalImages.size()
                ? m_externalImages.at(index) : QImage(imagePath);
        images.append(QJsonObject{
            {QStringLiteral("path"), imagePath},
            {QStringLiteral("width"), image.width()},
            {QStringLiteral("height"), image.height()},
            {QStringLiteral("sha256"), fileSha256(imagePath)}
        });
    }
    QJsonArray targetIds;
    for (const QString &id : targetCalibrationTransformIds())
        targetIds.append(id);
    const QJsonObject document{
        {QStringLiteral("version"), 1},
        {QStringLiteral("methodId"), QStringLiteral("n_point")},
        {QStringLiteral("targetKey"), draftTargetKey()},
        {QStringLiteral("targetToolIds"), targetIds},
        {QStringLiteral("persistentSettings"), nPoint->persistentSettings()},
        {QStringLiteral("nPoint"), nPoint->draftState()},
        {QStringLiteral("coordinateSourceFingerprint"),
         m_lockedCoordinateSourceFingerprint},
        {QStringLiteral("inputMode"),
         m_cameraModeButton && m_cameraModeButton->isChecked()
            ? QStringLiteral("camera") : QStringLiteral("image")},
        {QStringLiteral("referenceImage"), QJsonObject{
             {QStringLiteral("present"), !m_referencePreviewImage.isNull()},
             {QStringLiteral("width"), m_referencePreviewImage.width()},
             {QStringLiteral("height"), m_referencePreviewImage.height()},
             {QStringLiteral("sha256"), imageSha256(m_referencePreviewImage)}
         }},
        {QStringLiteral("images"), images},
        {QStringLiteral("currentImageIndex"),
         m_imageThumbnailList ? m_imageThumbnailList->currentRow() : -1},
        {QStringLiteral("updatedAt"),
         QDateTime::currentDateTime().toString(Qt::ISODateWithMs)}
    };

    QDir parent = QFileInfo(path).dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        if (errorMessage)
            *errorMessage = tr("无法创建标定草稿目录：%1").arg(parent.absolutePath());
        return false;
    }
    QSaveFile file(path);
    const QByteArray bytes = QJsonDocument(document).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly)
            || file.write(bytes) != bytes.size()
            || !file.commit()) {
        if (errorMessage)
            *errorMessage = tr("无法原子保存标定草稿：%1").arg(path);
        return false;
    }
    if (!m_lastSavedDraftPath.isEmpty() && m_lastSavedDraftPath != path)
        QFile::remove(m_lastSavedDraftPath);
    m_lastSavedDraftPath = path;
    return true;
}

bool QuickCalibrationWizard::restoreDraft(QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    const QString path = draftFilePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = tr("无法读取标定草稿：%1").arg(path);
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsed.isObject()) {
        if (errorMessage)
            *errorMessage = tr("标定草稿格式无效：%1").arg(parseError.errorString());
        return false;
    }
    const QJsonObject document = parsed.object();
    if (document.value(QStringLiteral("version")).toInt() != 1
            || document.value(QStringLiteral("methodId")).toString()
               != QStringLiteral("n_point")) {
        if (errorMessage)
            *errorMessage = tr("标定草稿版本或标定方式不兼容");
        return false;
    }
    if (document.value(QStringLiteral("targetKey")).toString()
            != draftTargetKey()) {
        if (errorMessage)
            *errorMessage = tr("标定草稿目标与当前选择不一致");
        return false;
    }
    const QJsonArray savedTargetIds = document.value(
                QStringLiteral("targetToolIds")).toArray();
    if (document.contains(QStringLiteral("targetToolIds"))
            && m_targetTransformList) {
        QSet<QString> availableIds;
        for (int row = 0; row < m_targetTransformList->count(); ++row) {
            const QString id = m_targetTransformList->item(row)
                    ->data(Qt::UserRole).toString();
            if (!id.isEmpty())
                availableIds.insert(id);
        }
        for (const QJsonValue &value : savedTargetIds) {
            if (!value.isString() || !availableIds.contains(value.toString())) {
                if (errorMessage)
                    *errorMessage = tr("标定草稿引用的目标工具已不存在");
                return false;
            }
        }
    }
    if (!ensureMethodConfigWidget()) {
        if (errorMessage)
            *errorMessage = tr("无法创建 N 点标定配置页面");
        return false;
    }
    NPointCalibrationConfigWidget *nPoint =
            dynamic_cast<NPointCalibrationConfigWidget *>(m_methodConfigWidget);
    if (!nPoint) {
        if (errorMessage)
            *errorMessage = tr("当前标定方式不是 N 点标定");
        return false;
    }

    const QJsonObject restoredSourceFingerprint = document.value(
                QStringLiteral("coordinateSourceFingerprint")).toObject();
    QString sourceFingerprintError;
    if (!restoredSourceFingerprint.isEmpty()
            && !CalibrationSourceFingerprint::isComplete(
                restoredSourceFingerprint, &sourceFingerprintError)) {
        if (errorMessage) {
            *errorMessage = tr("标定草稿的坐标来源指纹无效：%1")
                    .arg(sourceFingerprintError);
        }
        return false;
    }
    const QJsonObject savedPointState = document.value(
                QStringLiteral("nPoint")).toObject();
    int savedCompletedCount = 0;
    const QJsonArray savedSamples = savedPointState.value(
                QStringLiteral("samples")).toArray();
    for (const QJsonValue &value : savedSamples) {
        if (value.toObject().value(QStringLiteral("completed")).toBool(false))
            ++savedCompletedCount;
    }
    const QString savedCaptureMode = document.value(
                QStringLiteral("persistentSettings")).toObject()
            .value(QStringLiteral("captureBindings")).toObject()
            .value(QStringLiteral("captureMode"))
            .toString(QStringLiteral("trigger"));
    if (savedCompletedCount > 0
            && savedCaptureMode != QStringLiteral("manual")
            && restoredSourceFingerprint.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("旧草稿未记录模板来源身份，无法安全续采；请放弃草稿后重新标定");
        }
        return false;
    }
    if (savedCompletedCount > 0
            && savedCaptureMode != QStringLiteral("manual")) {
        QString currentSourceError;
        if (!validateCoordinateSourceFingerprint(
                    restoredSourceFingerprint, &currentSourceError)) {
            if (errorMessage) {
                *errorMessage = tr("草稿对应的模板来源已变化：%1")
                        .arg(currentSourceError);
            }
            return false;
        }
    }

    QVector<QImage> restoredImages;
    QStringList restoredPaths;
    const QJsonObject referenceInfo = document.value(
                QStringLiteral("referenceImage")).toObject();
    const bool savedReferencePresent = referenceInfo.value(
                QStringLiteral("present")).toBool();
    if (savedReferencePresent != !m_referencePreviewImage.isNull()
            || (savedReferencePresent
                && (referenceInfo.value(QStringLiteral("width")).toInt()
                    != m_referencePreviewImage.width()
                    || referenceInfo.value(QStringLiteral("height")).toInt()
                    != m_referencePreviewImage.height()
                    || referenceInfo.value(QStringLiteral("sha256")).toString()
                    != imageSha256(m_referencePreviewImage)))) {
        if (errorMessage)
            *errorMessage = tr("当前方案基准图与草稿采样时不一致");
        return false;
    }
    const QJsonArray images = document.value(QStringLiteral("images")).toArray();
    for (const QJsonValue &value : images) {
        const QJsonObject imageInfo = value.toObject();
        const QString imagePath = imageInfo.value(QStringLiteral("path"))
                .toString().trimmed();
        const QImage image(imagePath);
        if (imagePath.isEmpty() || image.isNull()) {
            if (errorMessage)
                *errorMessage = tr("草稿图片缺失或不可读：%1").arg(imagePath);
            return false;
        }
        if (image.width() != imageInfo.value(QStringLiteral("width")).toInt()
                || image.height() != imageInfo.value(QStringLiteral("height")).toInt()) {
            if (errorMessage)
                *errorMessage = tr("草稿图片尺寸已变化：%1").arg(imagePath);
            return false;
        }
        const QString expectedHash = imageInfo.value(QStringLiteral("sha256")).toString();
        if (!expectedHash.isEmpty() && fileSha256(imagePath) != expectedHash) {
            if (errorMessage)
                *errorMessage = tr("草稿图片内容已变化：%1").arg(imagePath);
            return false;
        }
        restoredImages.append(image);
        restoredPaths.append(QFileInfo(imagePath).absoluteFilePath());
    }

    const QString inputMode = document.value(QStringLiteral("inputMode"))
            .toString(QStringLiteral("image"));
    if (inputMode != QStringLiteral("camera")
            && inputMode != QStringLiteral("image")) {
        if (errorMessage)
            *errorMessage = tr("标定草稿图像输入模式无效");
        return false;
    }
    const int requestedIndex = document.value(
                QStringLiteral("currentImageIndex")).toInt(-1);
    if (requestedIndex < -1 || requestedIndex >= restoredImages.size()) {
        if (errorMessage)
            *errorMessage = tr("标定草稿当前图片序号无效");
        return false;
    }

    m_restoringDraft = true;
    const QJsonObject previousSettings = nPoint->persistentSettings();
    const QJsonObject previousState = nPoint->draftState();
    const QJsonObject previousSourceFingerprint =
            m_lockedCoordinateSourceFingerprint;
    QString restoreError;
    const bool settingsOk = nPoint->restorePersistentSettings(
                document.value(QStringLiteral("persistentSettings")).toObject(),
                &restoreError);
    const bool stateOk = settingsOk && nPoint->restoreDraftState(
                document.value(QStringLiteral("nPoint")).toObject(),
                &restoreError);
    if (!stateOk) {
        QString rollbackError;
        nPoint->restorePersistentSettings(previousSettings, &rollbackError);
        nPoint->restoreDraftState(previousState, &rollbackError);
        m_lockedCoordinateSourceFingerprint = previousSourceFingerprint;
        m_restoringDraft = false;
        if (errorMessage)
            *errorMessage = restoreError;
        return false;
    }

    if (document.contains(QStringLiteral("targetToolIds"))
            && m_targetTransformList) {
        for (int row = 0; row < m_targetTransformList->count(); ++row) {
            QListWidgetItem *item = m_targetTransformList->item(row);
            const QString id = item->data(Qt::UserRole).toString();
            if (!id.isEmpty()) {
                item->setCheckState(savedTargetIds.contains(id)
                                    ? Qt::Checked : Qt::Unchecked);
            }
        }
    }

    m_externalImages = restoredImages;
    m_externalImagePaths = restoredPaths;
    rebuildExternalImageList(requestedIndex);
    if (inputMode == QStringLiteral("camera")) {
        m_cameraModeButton->setChecked(true);
        m_imageModeButton->setChecked(false);
        m_imageCollectionPanel->hide();
        m_imageCounterLabel->hide();
        m_lastCapturedCameraFrameIndex =
                CameraFrameProvider::instance().currentFrameIndex();
        updatePreviewImage(CameraFrameProvider::instance().currentImage());
    } else {
        m_cameraModeButton->setChecked(false);
        m_imageModeButton->setChecked(true);
        m_imageCollectionPanel->show();
        m_imageCounterLabel->show();
        updateImageModeUi();
    }
    m_externalImageSequenceComplete = false;
    m_lastCommunicationMessage = CalibrationCommunicationMessage();
    m_sessionLog.clear();
    m_solveResult = CalibrationSolveResult();
    m_lockedCoordinateSourceFingerprint = restoredSourceFingerprint;
    if (m_communicationSession->isRunning())
        m_communicationSession->stop();
    m_communicationStatus->setText(tr("已恢复草稿；通信保持停止，请确认后重新启动"));
    m_sampleStatus->setText(tr("已恢复未完成标定：%1/%2，下一点为第 %3 点")
                            .arg(nPoint->completedSampleCount())
                            .arg(nPoint->sampleCount())
                            .arg(qMin(nPoint->completedSampleCount() + 1,
                                      nPoint->sampleCount())));
    m_sampleStatus->setProperty("status", QStringLiteral("idle"));
    m_lastSavedDraftPath = path;
    m_restoringDraft = false;
    return true;
}

void QuickCalibrationWizard::promptRestoreDraft()
{
    m_draftRestoreHandled = true;
    const QString path = draftFilePath();
    if (path.isEmpty() || !QFileInfo::exists(path))
        return;

    QFile file(path);
    int completed = 0;
    int total = 0;
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        const QJsonObject state = object.value(QStringLiteral("nPoint")).toObject();
        completed = state.value(QStringLiteral("completedSampleCount")).toInt();
        total = state.value(QStringLiteral("sampleCount")).toInt();
    }
    const QMessageBox::StandardButton answer = QMessageBox::question(
                this, tr("恢复未完成标定"),
                tr("发现当前方案未完成的 N 点标定（%1/%2）。\n"
                   "选择“是”从下一未完成点继续；选择“否”放弃该草稿。\n"
                   "通信不会自动连接。").arg(completed).arg(total),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);
    if (answer == QMessageBox::No) {
        discardDraft();
        return;
    }
    QString error;
    if (!restoreDraft(&error))
        QMessageBox::warning(this, tr("草稿恢复失败"), error);
}

void QuickCalibrationWizard::discardDraft()
{
    const QString path = draftFilePath();
    if (!path.isEmpty())
        QFile::remove(path);
    if (!m_lastSavedDraftPath.isEmpty() && m_lastSavedDraftPath != path)
        QFile::remove(m_lastSavedDraftPath);
    m_lastSavedDraftPath.clear();
}

void QuickCalibrationWizard::setProducerTools(
        const QVector<ToolConfig> &tools,
        const QMap<QString, ToolPreviewSnapshot> &snapshots,
        int selectedToolIndex)
{
    rebuildTargetTransformList(tools, selectedToolIndex);
    m_calibrationProducerConfigs.clear();
    m_calibrationProducerSnapshots.clear();
    for (const ToolConfig &tool : tools) {
        if (!tool.enabled || tool.toolId.trimmed().isEmpty()
                || tool.toolType != ToolType::TemplateLocation)
            continue;
        m_calibrationProducerConfigs.insert(tool.toolId, tool);
        const QString name = tool.displayName.trimmed().isEmpty()
                ? tr("模板定位") : tool.displayName;
        const ToolPreviewSnapshot preview = snapshots.value(tool.toolId);
        CalibrationProducerSnapshot producer;
        producer.producerId = tool.toolId;
        producer.displayName = name;
        producer.valid = preview.valid
                && preview.result.success
                && preview.result.ok;
        producer.payload = preview.result.payload;
        m_calibrationProducerSnapshots.append(producer);
    }
    if (m_methodConfigWidget)
        m_methodConfigWidget->setProducerSnapshots(m_calibrationProducerSnapshots);
}

void QuickCalibrationWizard::rebuildTargetTransformList(
        const QVector<ToolConfig> &tools, int selectedToolIndex)
{
    if (!m_targetTransformList)
        return;

    m_targetTransformList->clear();
    m_targetTransformConfigs.clear();
    QVector<int> transformIndexes;
    for (int index = 0; index < tools.size(); ++index) {
        if (tools.at(index).toolType == ToolType::CalibrationTransform
                && !tools.at(index).toolId.trimmed().isEmpty()) {
            transformIndexes.append(index);
        }
    }

    int defaultIndex = -1;
    if (selectedToolIndex >= 0 && selectedToolIndex < tools.size()
            && tools.at(selectedToolIndex).toolType
               == ToolType::CalibrationTransform) {
        defaultIndex = selectedToolIndex;
    } else if (transformIndexes.size() == 1) {
        defaultIndex = transformIndexes.first();
    }
    m_configurationTargetToolId = defaultIndex >= 0
            ? tools.at(defaultIndex).toolId : QString();

    if (transformIndexes.isEmpty()) {
        QListWidgetItem *empty = new QListWidgetItem(
                    tr("当前方案没有标定转换；XML 将仅保存为方案标定文件"),
                    m_targetTransformList);
        empty->setFlags(Qt::NoItemFlags);
        if (m_targetTransformHint)
            m_targetTransformHint->setText(
                        tr("可先完成标定文件生成，之后新增标定转换并选择该文件。"));
        return;
    }

    for (int index : std::as_const(transformIndexes)) {
        const ToolConfig &tool = tools.at(index);
        m_targetTransformConfigs.insert(tool.toolId, tool);
        const QJsonObject transform = tool.params
                .value(QStringLiteral("calibrationTransform")).toObject();
        const QString activeFile = transform
                .value(QStringLiteral("activeCalibrationFile")).toString();
        const QJsonObject inputX = transform.value(QStringLiteral("inputX")).toObject();
        const QJsonObject inputY = transform.value(QStringLiteral("inputY")).toObject();
        const QJsonObject inputAngle = transform
                .value(QStringLiteral("inputAngle")).toObject();
        QString producerId = inputX.value(QStringLiteral("producerId")).toString();
        if (producerId.isEmpty()
                || inputY.value(QStringLiteral("producerId")).toString() != producerId
                || inputAngle.value(QStringLiteral("producerId")).toString() != producerId) {
            producerId.clear();
        }
        const QString displayName = tool.displayName.trimmed().isEmpty()
                ? tr("标定转换") : tool.displayName.trimmed();
        const QString shortId = tool.toolId.left(8);
        const QString sourceText = producerId.isEmpty()
                ? tr("输入来源未唯一绑定")
                : tr("输入来源 %1").arg(producerId.left(8));
        const QString activeText = activeFile.trimmed().isEmpty()
                ? tr("当前文件：未设置")
                : tr("当前文件：%1").arg(QFileInfo(activeFile).fileName());
        QListWidgetItem *item = new QListWidgetItem(
                    tr("#%1 %2 [%3]  ·  %4  ·  %5")
                    .arg(index + 1).arg(displayName, shortId, sourceText, activeText),
                    m_targetTransformList);
        item->setData(Qt::UserRole, tool.toolId);
        item->setData(Qt::UserRole + 1, producerId);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(index == defaultIndex ? Qt::Checked : Qt::Unchecked);
    }
    if (m_targetTransformHint) {
        m_targetTransformHint->setText(transformIndexes.size() == 1
                ? tr("已默认选择唯一标定转换；取消勾选可仅保存文件。")
                : tr("方案中存在多个标定转换，请确认目标；程序不会默认全部替换。"));
    }
}

QStringList QuickCalibrationWizard::targetCalibrationTransformIds() const
{
    QStringList ids;
    if (!m_targetTransformList)
        return ids;
    for (int row = 0; row < m_targetTransformList->count(); ++row) {
        const QListWidgetItem *item = m_targetTransformList->item(row);
        if (item->checkState() != Qt::Checked)
            continue;
        const QString id = item->data(Qt::UserRole).toString().trimmed();
        if (!id.isEmpty() && !ids.contains(id))
            ids.append(id);
    }
    return ids;
}

bool QuickCalibrationWizard::ensureMethodConfigWidget()
{
    if (m_methodConfigWidget && m_configWidgetMethodId == m_methodId)
        return true;
    const ICalibrationMethod *method = CalibrationMethodRegistry::instance().method(m_methodId);
    if (!method || method->availability() != CalibrationMethodAvailability::Available)
        return false;
    if (m_methodConfigWidget) {
        m_methodConfigLayout->removeWidget(m_methodConfigWidget);
        m_methodConfigWidget->deleteLater();
        m_methodConfigWidget = nullptr;
    }
    m_methodConfigWidget = method->createConfigWidget(m_pages->widget(2));
    if (!m_methodConfigWidget)
        return false;
    m_configWidgetMethodId = m_methodId;
    m_methodConfigWidget->setProducerSnapshots(m_calibrationProducerSnapshots);
    if (NPointCalibrationConfigWidget *nPoint =
            dynamic_cast<NPointCalibrationConfigWidget *>(m_methodConfigWidget)) {
        QString restoreError;
        if (!m_pendingMethodSettings.isEmpty()
                && !nPoint->restorePersistentSettings(
                    m_pendingMethodSettings, &restoreError)) {
            m_persistentConfigurationWritable = false;
            m_persistentConfigurationError = tr(
                        "当前方案的 N 点稳定配置无效：%1。已禁止求解和生成，请修复方案配置。")
                    .arg(restoreError);
            if (m_communicationStatus)
                m_communicationStatus->setText(m_persistentConfigurationError);
            if (m_sampleStatus)
                m_sampleStatus->setText(m_persistentConfigurationError);
            qWarning() << "[QuickCalibrationWizard] 恢复N点稳定配置失败:"
                       << restoreError;
        }
    }
    m_methodConfigWidget->setLatestPhysicalSample(
                m_lastCommunicationMessage.valid
                && m_lastCommunicationMessage.event == CalibrationCommunicationEvent::Capture,
                m_lastCommunicationMessage.x,
                m_lastCommunicationMessage.y,
                m_lastCommunicationMessage.angleDeg);
    connect(m_methodConfigWidget, &CalibrationMethodConfigWidget::sampleStateChanged,
            this, [this](const QString &text, bool ok) {
        if (!m_sampleStatus)
            return;
        m_sampleStatus->setText(text);
        m_sampleStatus->setProperty("status", ok ? QStringLiteral("ok")
                                                  : QStringLiteral("error"));
        m_sampleStatus->style()->unpolish(m_sampleStatus);
        m_sampleStatus->style()->polish(m_sampleStatus);
        if (ok && !m_restoringDraft) {
            QString draftError;
            if (!saveDraft(&draftError) && !draftError.isEmpty())
                qWarning() << "[QuickCalibrationWizard] 保存标定草稿失败:"
                           << draftError;
        }
    });
    connect(m_methodConfigWidget, &CalibrationMethodConfigWidget::sampleDataChanged,
            this, [this]() {
        m_solveResult = CalibrationSolveResult();
        if (NPointCalibrationConfigWidget *nPoint =
                dynamic_cast<NPointCalibrationConfigWidget *>(m_methodConfigWidget)) {
            nPoint->setRegionSummary(0, 0,
                                     tr("参数或点表已改变，请重新执行"),
                                     QStringLiteral("warning"));
        }
        if (!m_restoringDraft && m_methodConfigWidget
                && m_methodConfigWidget->completedTranslationSampleCount() == 0) {
            m_lockedCoordinateSourceFingerprint = QJsonObject();
        }
        updateCalibrationOverlays();
        if (!m_restoringDraft) {
            QString draftError;
            if (!saveDraft(&draftError) && !draftError.isEmpty())
                qWarning() << "[QuickCalibrationWizard] 保存点表草稿失败:"
                           << draftError;
        }
    });
    m_methodConfigLayout->insertWidget(1, m_methodConfigWidget, 1);
    return true;
}

void QuickCalibrationWizard::generateCalibrationFile()
{
    if (!m_persistentConfigurationError.isEmpty()) {
        QMessageBox::warning(this, tr("生成已禁止"),
                             m_persistentConfigurationError);
        return;
    }
    const ICalibrationMethod *method = CalibrationMethodRegistry::instance().method(m_methodId);
    QString validationError;
    if (!method || !method->validateResult(m_solveResult, &validationError)) {
        QMessageBox::warning(this, tr("生成失败"), validationError.isEmpty()
                             ? tr("只有通过方式专用质量门禁的结果才能生成文件")
                             : validationError);
        return;
    }
    const CalibrationModel model = method->buildCalibrationModel(m_solveResult);
    QString path = m_filePath->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("生成失败"), tr("请选择标定文件路径"));
        return;
    }
    if (!path.endsWith(QStringLiteral(".xml"), Qt::CaseInsensitive))
        path += QStringLiteral(".xml");
    path = QFileInfo(path).absoluteFilePath();
    QDir parent = QFileInfo(path).dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        QMessageBox::warning(this, tr("生成失败"), tr("无法创建标定文件目录"));
        return;
    }
    if (QFileInfo::exists(path) && !m_updateAfterGenerate->isChecked()) {
        QMessageBox::warning(this, tr("生成失败"), tr("目标文件已存在；请更换文件名或开启生成后更新"));
        return;
    }
    if (QFileInfo::exists(path)) {
        QStringList referencingTools;
        for (auto it = m_targetTransformConfigs.constBegin();
             it != m_targetTransformConfigs.constEnd(); ++it) {
            const QJsonObject transform = it.value().params
                    .value(QStringLiteral("calibrationTransform")).toObject();
            bool referenced = QFileInfo(transform.value(
                        QStringLiteral("activeCalibrationFile")).toString())
                    .absoluteFilePath() == path;
            const QJsonArray files = transform.value(
                        QStringLiteral("calibrationFiles")).toArray();
            for (const QJsonValue &value : files) {
                if (QFileInfo(value.toString()).absoluteFilePath() == path) {
                    referenced = true;
                    break;
                }
            }
            if (referenced) {
                referencingTools.append(it.value().displayName.trimmed().isEmpty()
                                        ? it.key() : it.value().displayName.trimmed());
            }
        }
        if (!referencingTools.isEmpty()) {
            QMessageBox::warning(
                        this, tr("生成失败"),
                        tr("该文件正被标定转换引用，禁止原地覆盖：%1\n"
                           "请使用新的时间戳文件名，避免未选择的工具被间接改变。")
                        .arg(referencingTools.join(QStringLiteral("、"))));
            return;
        }
    }
    QString error;
    if (!ProjectXmlCalibrationLoader().save(path, model, &error)) {
        QMessageBox::warning(this, tr("生成失败"), error);
        return;
    }

    QString generatedAssetPath = path;
    const QString schemeDir = SchemeStore::instance().currentScheme().schemeDir;
    if (schemeDir.trimmed().isEmpty()) {
        QMessageBox::warning(this, tr("生成未归档"),
                             tr("XML 已写出，但当前方案目录为空，未应用到运行配置：\n%1")
                             .arg(path));
        return;
    }
    const QString assetDirectory = QDir(schemeDir).filePath(
                QStringLiteral("calibrations"));
    const QString assetRoot = QDir::cleanPath(
                QFileInfo(assetDirectory).absoluteFilePath()) + QDir::separator();
    if (!QDir::cleanPath(path).startsWith(assetRoot)) {
        const QString digest = fileSha256(path);
        QDir assetDir(assetDirectory);
        if (digest.isEmpty()
                || (!assetDir.exists() && !QDir().mkpath(assetDirectory))) {
            QMessageBox::warning(this, tr("生成未归档"),
                                 tr("XML 已写出，但无法归档到当前方案：\n%1")
                                 .arg(path));
            return;
        }
        const QFileInfo sourceInfo(path);
        generatedAssetPath = assetDir.filePath(
                    QStringLiteral("%1_%2.%3")
                    .arg(sourceInfo.completeBaseName(), digest.left(12),
                         sourceInfo.suffix()));
        if (QFileInfo::exists(generatedAssetPath)) {
            if (fileSha256(generatedAssetPath) != digest) {
                QMessageBox::warning(this, tr("生成未归档"),
                                     tr("方案标定目录存在内容冲突：\n%1")
                                     .arg(generatedAssetPath));
                return;
            }
        } else if (!QFile::copy(path, generatedAssetPath)) {
            QMessageBox::warning(this, tr("生成未归档"),
                                 tr("XML 已写出，但复制到方案标定目录失败：\n%1")
                                 .arg(generatedAssetPath));
            return;
        }
    }

    m_generatedFilePath = generatedAssetPath;
    m_filePath->setText(generatedAssetPath);
    m_generatedFileCompleted = true;
    discardDraft();
    const int targetCount = targetCalibrationTransformIds().size();
    QMessageBox::information(this, tr("生成成功"),
                             targetCount > 0
                             ? tr("标定文件已生成：\n%1\n关闭向导后将应用到 %2 个已勾选的标定转换。")
                               .arg(generatedAssetPath).arg(targetCount)
                             : tr("标定文件已生成：\n%1\n当前未选择目标，不会改变标定转换的运行文件。")
                               .arg(generatedAssetPath));
}

QString QuickCalibrationWizard::generatedFilePath() const
{
    return m_generatedFilePath;
}
