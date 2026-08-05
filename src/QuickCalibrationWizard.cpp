#include "QuickCalibrationWizard.h"

#include "PlanDialogUtils.h"
#include "SchemeStore.h"
#include "calibration/CalibrationFileLoader.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
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
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

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

} // namespace

QuickCalibrationWizard::QuickCalibrationWizard(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("QuickCalibrationWizard"));
    m_communicationSession = new CalibrationCommunicationSession(this);
    m_communicationSession->setMessageHandler(
                [this](CalibrationCommunicationMessage *message, QString *errorMessage) {
        if (!message || message->event != CalibrationCommunicationEvent::Capture)
            return true;
        if (!m_methodConfigWidget) {
            if (errorMessage)
                *errorMessage = tr("尚未进入标定配置页，不能执行触发采样");
            return false;
        }
        m_methodConfigWidget->setLatestPhysicalSample(true, message->x,
                                                       message->y, message->angleDeg);
        return m_methodConfigWidget->captureCurrentSample(errorMessage);
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
    m_communicationType->addItems({tr("无设备（手动输入）"), tr("TCP 客户端"),
                                   tr("TCP 服务端"), tr("UDP")});
    deviceForm->addRow(tr("通信方式"), m_communicationType);
    m_communicationHost = new QLineEdit(QStringLiteral("127.0.0.1"), device);
    m_communicationPort = new QSpinBox(device);
    m_communicationPort->setRange(1, 65535);
    m_communicationPort->setValue(2000);
    deviceForm->addRow(tr("主机/绑定地址"), m_communicationHost);
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
    const auto syncTransportFields = [this](int index) {
        const bool hasDevice = index != 0;
        m_communicationHost->setEnabled(hasDevice);
        m_communicationPort->setEnabled(hasDevice);
        m_communicationStartButton->setText(hasDevice ? tr("启动通信") : tr("启用手动会话"));
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
    });
    connect(m_imageModeButton, &QPushButton::clicked, this, [this]() {
        m_cameraModeButton->setChecked(false);
        m_imageModeButton->setChecked(true);
        m_imageCollectionPanel->show();
        m_imageCounterLabel->show();
        updateImageModeUi();
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
        showExternalImage(m_imageThumbnailList->currentRow() - 1);
    });
    connect(m_nextImageButton, &QToolButton::clicked, this, [this]() {
        showExternalImage(m_imageThumbnailList->currentRow() + 1);
    });
    connect(m_imageThumbnailList, &QListWidget::currentRowChanged,
            this, &QuickCalibrationWizard::showExternalImage);
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
    m_resultTable->setHorizontalHeaderLabels({tr("序号"), tr("信号内容"), tr("结果状态")});
    m_resultTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_resultTable->verticalHeader()->setVisible(false);
    layout->addWidget(m_resultTable, 1);
    m_matrixLabel = new QLabel(page);
    m_matrixLabel->setProperty("panelRole", QStringLiteral("matrixSummary"));
    layout->addWidget(m_matrixLabel);
    m_qualityLabel = new QLabel(page);
    layout->addWidget(m_qualityLabel);
    QHBoxLayout *fileRow = new QHBoxLayout;
    m_updateAfterGenerate = new QCheckBox(tr("文件生成后更新"), page);
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
    m_step = qBound(0, step, 3);
    if (m_step == 2 && !ensureMethodConfigWidget()) {
        m_step = 0;
        QMessageBox::warning(this, tr("标定配置"), tr("所选标定方式未提供配置页面"));
    }
    m_maxVisitedStep = qMax(m_maxVisitedStep, m_step);
    m_pages->setCurrentIndex(m_step);
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
    rebuildExternalImageList(qMin(index, m_externalImages.size() - 1));
}

void QuickCalibrationWizard::clearExternalImages()
{
    if (m_externalImages.isEmpty())
        return;
    m_externalImages.clear();
    m_externalImagePaths.clear();
    rebuildExternalImageList(-1);
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
}

void QuickCalibrationWizard::solveCalibration()
{
    const ICalibrationMethod *method = CalibrationMethodRegistry::instance().method(m_methodId);
    if (!method) {
        QMessageBox::warning(this, tr("求解失败"), tr("标定方式不存在"));
        return;
    }
    QString error;
    if (!ensureMethodConfigWidget())
        return;
    const CalibrationDraft draft = m_methodConfigWidget->draft(&error);
    if (!method->validateDraft(draft, &error)) {
        QMessageBox::warning(this, tr("求解失败"), error);
        return;
    }
    m_solveResult = method->solve(draft);
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

CalibrationCommunicationConfig QuickCalibrationWizard::communicationConfig() const
{
    CalibrationCommunicationConfig config;
    const QStringList transports{QStringLiteral("none"), QStringLiteral("tcp_client"),
                                 QStringLiteral("tcp_server"), QStringLiteral("udp")};
    config.transport = transports.value(m_communicationType->currentIndex(),
                                        QStringLiteral("none"));
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
    QString error;
    if (!m_communicationSession->start(communicationConfig(), &error))
        QMessageBox::warning(this, tr("启动通信失败"), error);
}

void QuickCalibrationWizard::setProducerTools(
        const QVector<ToolConfig> &tools,
        const QMap<QString, ToolPreviewSnapshot> &snapshots)
{
    m_calibrationProducerSnapshots.clear();
    for (const ToolConfig &tool : tools) {
        if (!tool.enabled || tool.toolId.trimmed().isEmpty()
                || tool.toolType != ToolType::TemplateLocation)
            continue;
        const QString name = tool.displayName.trimmed().isEmpty()
                ? tr("模板定位") : tool.displayName;
        const ToolPreviewSnapshot preview = snapshots.value(tool.toolId);
        CalibrationProducerSnapshot producer;
        producer.producerId = tool.toolId;
        producer.displayName = name;
        producer.valid = preview.valid && preview.result.success;
        producer.payload = preview.result.payload;
        m_calibrationProducerSnapshots.append(producer);
    }
    if (m_methodConfigWidget)
        m_methodConfigWidget->setProducerSnapshots(m_calibrationProducerSnapshots);
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
    });
    m_methodConfigLayout->insertWidget(1, m_methodConfigWidget, 1);
    return true;
}

void QuickCalibrationWizard::generateCalibrationFile()
{
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
    QDir parent = QFileInfo(path).dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        QMessageBox::warning(this, tr("生成失败"), tr("无法创建标定文件目录"));
        return;
    }
    if (QFileInfo::exists(path) && !m_updateAfterGenerate->isChecked()) {
        QMessageBox::warning(this, tr("生成失败"), tr("目标文件已存在；请更换文件名或开启生成后更新"));
        return;
    }
    QString error;
    if (!ProjectXmlCalibrationLoader().save(path, model, &error)) {
        QMessageBox::warning(this, tr("生成失败"), error);
        return;
    }
    m_generatedFilePath = path;
    m_filePath->setText(path);
    QMessageBox::information(this, tr("生成成功"),
                             tr("标定文件已生成：\n%1\n可在“标定转换”工具中直接导入。").arg(path));
}

QString QuickCalibrationWizard::generatedFilePath() const
{
    return m_generatedFilePath;
}
