/********************************************************************************
** Form generated from reading UI file 'LoginWindow.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGINWINDOW_H
#define UI_LOGINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LoginWindow
{
public:
    QVBoxLayout *verticalLayout_root;
    QFrame *headerBar;
    QHBoxLayout *horizontalLayout_header;
    QWidget *headerBrandWidget;
    QHBoxLayout *horizontalLayout_brand;
    QLabel *headerLogoLabel;
    QLabel *headerTitleLabel;
    QToolButton *navCameraButton;
    QToolButton *navPlanButton;
    QToolButton *navIoButton;
    QToolButton *navCommButton;
    QToolButton *navAssistButton;
    QToolButton *navMonitorButton;
    QSpacerItem *horizontalSpacer_header;
    QPushButton *btn_system;
    QPushButton *btn_help;
    QToolButton *headerMinimizeButton;
    QToolButton *headerMaximizeButton;
    QToolButton *headerCloseButton;
    QWidget *contentArea;
    QHBoxLayout *horizontalLayout_contentArea;
    QSpacerItem *horizontalSpacer_left;
    QFrame *contentCard;
    QHBoxLayout *horizontalLayout_contentCard;
    QFrame *leftPanel;
    QVBoxLayout *verticalLayout_leftPanel;
    QHBoxLayout *horizontalLayout_deviceHeader;
    QLabel *sectionTitleLabel;
    QSpacerItem *horizontalSpacer_deviceHeader;
    QToolButton *refreshDeviceButton;
    QToolButton *addDeviceButton;
    QListWidget *deviceListWidget;
    QFrame *deviceInfoPanel;
    QVBoxLayout *verticalLayout_deviceInfo;
    QFormLayout *formLayout_deviceInfo;
    QLabel *adapterLabel;
    QLabel *deviceNameLabel;
    QLabel *macLabel;
    QLabel *ipLabel;
    QLabel *maskLabel;
    QLabel *gatewayLabel;
    QLineEdit *lineEdit_adapterValue;
    QLineEdit *lineEdit_deviceNameValue;
    QLineEdit *lineEdit_macValue;
    QLineEdit *lineEdit_ipValue;
    QLineEdit *lineEdit_maskValue;
    QLineEdit *lineEdit_gatewayValue;
    QFrame *rightPanel;
    QVBoxLayout *verticalLayout_rightPanel;
    QHBoxLayout *horizontalLayout_cardClose;
    QSpacerItem *horizontalSpacer_cardClose;
    QToolButton *closeCardButton;
    QSpacerItem *verticalSpacer_topRight;
    QHBoxLayout *horizontalLayout_logo;
    QSpacerItem *horizontalSpacer_logoLeft;
    QLabel *logoMarkLargeLabel;
    QLabel *logoTitleLabel;
    QSpacerItem *horizontalSpacer_logoRight;
    QSpacerItem *verticalSpacer_logoGap;
    QFrame *deviceFieldFrame;
    QHBoxLayout *horizontalLayout_deviceField;
    QLabel *deviceFieldIconLabel;
    QComboBox *deviceComboBox;
    QFrame *userFieldFrame;
    QHBoxLayout *horizontalLayout_userField;
    QLabel *userFieldIconLabel;
    QComboBox *userComboBox;
    QFrame *passwordFieldFrame;
    QHBoxLayout *horizontalLayout_passwordField;
    QLabel *passwordFieldIconLabel;
    QLineEdit *passwordLineEdit;
    QToolButton *passwordToggleButton;
    QSpacerItem *verticalSpacer_rightStretch;
    QHBoxLayout *horizontalLayout_loginButton;
    QSpacerItem *horizontalSpacer_loginLeft;
    QPushButton *loginButton;
    QSpacerItem *horizontalSpacer_loginRight;
    QSpacerItem *verticalSpacer;
    QSpacerItem *horizontalSpacer_right;

    void setupUi(QDialog *LoginWindow)
    {
        if (LoginWindow->objectName().isEmpty())
            LoginWindow->setObjectName(QString::fromUtf8("LoginWindow"));
        LoginWindow->resize(1352, 923);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(LoginWindow->sizePolicy().hasHeightForWidth());
        LoginWindow->setSizePolicy(sizePolicy);
        LoginWindow->setWindowState(Qt::WindowMaximized);
        verticalLayout_root = new QVBoxLayout(LoginWindow);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        headerBar = new QFrame(LoginWindow);
        headerBar->setObjectName(QString::fromUtf8("headerBar"));
        headerBar->setMinimumSize(QSize(0, 82));
        headerBar->setMaximumSize(QSize(16777215, 82));
        headerBar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_header = new QHBoxLayout(headerBar);
        horizontalLayout_header->setSpacing(16);
        horizontalLayout_header->setObjectName(QString::fromUtf8("horizontalLayout_header"));
        horizontalLayout_header->setContentsMargins(22, 10, 22, 10);
        headerBrandWidget = new QWidget(headerBar);
        headerBrandWidget->setObjectName(QString::fromUtf8("headerBrandWidget"));
        horizontalLayout_brand = new QHBoxLayout(headerBrandWidget);
        horizontalLayout_brand->setSpacing(12);
        horizontalLayout_brand->setObjectName(QString::fromUtf8("horizontalLayout_brand"));
        horizontalLayout_brand->setContentsMargins(0, 0, 0, 0);
        headerLogoLabel = new QLabel(headerBrandWidget);
        headerLogoLabel->setObjectName(QString::fromUtf8("headerLogoLabel"));
        headerLogoLabel->setMinimumSize(QSize(70, 48));
        headerLogoLabel->setMaximumSize(QSize(48, 48));
        headerLogoLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/LOGO.png")));
        headerLogoLabel->setScaledContents(true);

        horizontalLayout_brand->addWidget(headerLogoLabel);

        headerTitleLabel = new QLabel(headerBrandWidget);
        headerTitleLabel->setObjectName(QString::fromUtf8("headerTitleLabel"));

        horizontalLayout_brand->addWidget(headerTitleLabel);


        horizontalLayout_header->addWidget(headerBrandWidget);

        navCameraButton = new QToolButton(headerBar);
        navCameraButton->setObjectName(QString::fromUtf8("navCameraButton"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/camera.svg"), QSize(), QIcon::Normal, QIcon::Off);
        navCameraButton->setIcon(icon);
        navCameraButton->setIconSize(QSize(28, 28));
        navCameraButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        navCameraButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(navCameraButton);

        navPlanButton = new QToolButton(headerBar);
        navPlanButton->setObjectName(QString::fromUtf8("navPlanButton"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/plan.svg"), QSize(), QIcon::Normal, QIcon::Off);
        navPlanButton->setIcon(icon1);
        navPlanButton->setIconSize(QSize(28, 28));
        navPlanButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        navPlanButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(navPlanButton);

        navIoButton = new QToolButton(headerBar);
        navIoButton->setObjectName(QString::fromUtf8("navIoButton"));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/io.svg"), QSize(), QIcon::Normal, QIcon::Off);
        navIoButton->setIcon(icon2);
        navIoButton->setIconSize(QSize(28, 28));
        navIoButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        navIoButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(navIoButton);

        navCommButton = new QToolButton(headerBar);
        navCommButton->setObjectName(QString::fromUtf8("navCommButton"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icons/comm.svg"), QSize(), QIcon::Normal, QIcon::Off);
        navCommButton->setIcon(icon3);
        navCommButton->setIconSize(QSize(28, 28));
        navCommButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        navCommButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(navCommButton);

        navAssistButton = new QToolButton(headerBar);
        navAssistButton->setObjectName(QString::fromUtf8("navAssistButton"));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/tool.svg"), QSize(), QIcon::Normal, QIcon::Off);
        navAssistButton->setIcon(icon4);
        navAssistButton->setIconSize(QSize(28, 28));
        navAssistButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        navAssistButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(navAssistButton);

        navMonitorButton = new QToolButton(headerBar);
        navMonitorButton->setObjectName(QString::fromUtf8("navMonitorButton"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/monitor.svg"), QSize(), QIcon::Normal, QIcon::Off);
        navMonitorButton->setIcon(icon5);
        navMonitorButton->setIconSize(QSize(28, 28));
        navMonitorButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        navMonitorButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(navMonitorButton);

        horizontalSpacer_header = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_header->addItem(horizontalSpacer_header);

        btn_system = new QPushButton(headerBar);
        btn_system->setObjectName(QString::fromUtf8("btn_system"));

        horizontalLayout_header->addWidget(btn_system);

        btn_help = new QPushButton(headerBar);
        btn_help->setObjectName(QString::fromUtf8("btn_help"));

        horizontalLayout_header->addWidget(btn_help);

        headerMinimizeButton = new QToolButton(headerBar);
        headerMinimizeButton->setObjectName(QString::fromUtf8("headerMinimizeButton"));
        headerMinimizeButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(headerMinimizeButton);

        headerMaximizeButton = new QToolButton(headerBar);
        headerMaximizeButton->setObjectName(QString::fromUtf8("headerMaximizeButton"));
        headerMaximizeButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(headerMaximizeButton);

        headerCloseButton = new QToolButton(headerBar);
        headerCloseButton->setObjectName(QString::fromUtf8("headerCloseButton"));
        headerCloseButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(headerCloseButton);


        verticalLayout_root->addWidget(headerBar);

        contentArea = new QWidget(LoginWindow);
        contentArea->setObjectName(QString::fromUtf8("contentArea"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(contentArea->sizePolicy().hasHeightForWidth());
        contentArea->setSizePolicy(sizePolicy1);
        contentArea->setMinimumSize(QSize(0, 180));
        horizontalLayout_contentArea = new QHBoxLayout(contentArea);
        horizontalLayout_contentArea->setObjectName(QString::fromUtf8("horizontalLayout_contentArea"));
        horizontalLayout_contentArea->setContentsMargins(110, 48, 110, 20);
        horizontalSpacer_left = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_contentArea->addItem(horizontalSpacer_left);

        contentCard = new QFrame(contentArea);
        contentCard->setObjectName(QString::fromUtf8("contentCard"));
        contentCard->setMinimumSize(QSize(1120, 660));
        contentCard->setFrameShape(QFrame::NoFrame);
        horizontalLayout_contentCard = new QHBoxLayout(contentCard);
        horizontalLayout_contentCard->setSpacing(0);
        horizontalLayout_contentCard->setObjectName(QString::fromUtf8("horizontalLayout_contentCard"));
        horizontalLayout_contentCard->setContentsMargins(0, 0, 0, 0);
        leftPanel = new QFrame(contentCard);
        leftPanel->setObjectName(QString::fromUtf8("leftPanel"));
        leftPanel->setMinimumSize(QSize(470, 0));
        leftPanel->setMaximumSize(QSize(470, 16777215));
        leftPanel->setFrameShape(QFrame::NoFrame);
        verticalLayout_leftPanel = new QVBoxLayout(leftPanel);
        verticalLayout_leftPanel->setSpacing(24);
        verticalLayout_leftPanel->setObjectName(QString::fromUtf8("verticalLayout_leftPanel"));
        verticalLayout_leftPanel->setContentsMargins(46, 10, 36, 10);
        horizontalLayout_deviceHeader = new QHBoxLayout();
        horizontalLayout_deviceHeader->setObjectName(QString::fromUtf8("horizontalLayout_deviceHeader"));
        sectionTitleLabel = new QLabel(leftPanel);
        sectionTitleLabel->setObjectName(QString::fromUtf8("sectionTitleLabel"));

        horizontalLayout_deviceHeader->addWidget(sectionTitleLabel);

        horizontalSpacer_deviceHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_deviceHeader->addItem(horizontalSpacer_deviceHeader);

        refreshDeviceButton = new QToolButton(leftPanel);
        refreshDeviceButton->setObjectName(QString::fromUtf8("refreshDeviceButton"));
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/icons/refresh.svg"), QSize(), QIcon::Normal, QIcon::Off);
        refreshDeviceButton->setIcon(icon6);
        refreshDeviceButton->setIconSize(QSize(22, 22));
        refreshDeviceButton->setAutoRaise(true);

        horizontalLayout_deviceHeader->addWidget(refreshDeviceButton);

        addDeviceButton = new QToolButton(leftPanel);
        addDeviceButton->setObjectName(QString::fromUtf8("addDeviceButton"));
        QIcon icon7;
        icon7.addFile(QString::fromUtf8(":/icons/add.svg"), QSize(), QIcon::Normal, QIcon::Off);
        addDeviceButton->setIcon(icon7);
        addDeviceButton->setIconSize(QSize(22, 22));
        addDeviceButton->setAutoRaise(true);

        horizontalLayout_deviceHeader->addWidget(addDeviceButton);


        verticalLayout_leftPanel->addLayout(horizontalLayout_deviceHeader);

        deviceListWidget = new QListWidget(leftPanel);
        deviceListWidget->setObjectName(QString::fromUtf8("deviceListWidget"));
        deviceListWidget->setMinimumSize(QSize(0, 330));
        deviceListWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        deviceListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        deviceListWidget->setSelectionMode(QAbstractItemView::SingleSelection);

        verticalLayout_leftPanel->addWidget(deviceListWidget);

        deviceInfoPanel = new QFrame(leftPanel);
        deviceInfoPanel->setObjectName(QString::fromUtf8("deviceInfoPanel"));
        deviceInfoPanel->setMinimumSize(QSize(0, 180));
        deviceInfoPanel->setFrameShape(QFrame::NoFrame);
        verticalLayout_deviceInfo = new QVBoxLayout(deviceInfoPanel);
        verticalLayout_deviceInfo->setSpacing(18);
        verticalLayout_deviceInfo->setObjectName(QString::fromUtf8("verticalLayout_deviceInfo"));
        verticalLayout_deviceInfo->setContentsMargins(18, 20, 14, 20);
        formLayout_deviceInfo = new QFormLayout();
        formLayout_deviceInfo->setObjectName(QString::fromUtf8("formLayout_deviceInfo"));
        formLayout_deviceInfo->setHorizontalSpacing(24);
        formLayout_deviceInfo->setVerticalSpacing(16);
        adapterLabel = new QLabel(deviceInfoPanel);
        adapterLabel->setObjectName(QString::fromUtf8("adapterLabel"));

        formLayout_deviceInfo->setWidget(0, QFormLayout::LabelRole, adapterLabel);

        deviceNameLabel = new QLabel(deviceInfoPanel);
        deviceNameLabel->setObjectName(QString::fromUtf8("deviceNameLabel"));

        formLayout_deviceInfo->setWidget(1, QFormLayout::LabelRole, deviceNameLabel);

        macLabel = new QLabel(deviceInfoPanel);
        macLabel->setObjectName(QString::fromUtf8("macLabel"));

        formLayout_deviceInfo->setWidget(2, QFormLayout::LabelRole, macLabel);

        ipLabel = new QLabel(deviceInfoPanel);
        ipLabel->setObjectName(QString::fromUtf8("ipLabel"));

        formLayout_deviceInfo->setWidget(3, QFormLayout::LabelRole, ipLabel);

        maskLabel = new QLabel(deviceInfoPanel);
        maskLabel->setObjectName(QString::fromUtf8("maskLabel"));

        formLayout_deviceInfo->setWidget(4, QFormLayout::LabelRole, maskLabel);

        gatewayLabel = new QLabel(deviceInfoPanel);
        gatewayLabel->setObjectName(QString::fromUtf8("gatewayLabel"));

        formLayout_deviceInfo->setWidget(5, QFormLayout::LabelRole, gatewayLabel);

        lineEdit_adapterValue = new QLineEdit(deviceInfoPanel);
        lineEdit_adapterValue->setObjectName(QString::fromUtf8("lineEdit_adapterValue"));

        formLayout_deviceInfo->setWidget(0, QFormLayout::FieldRole, lineEdit_adapterValue);

        lineEdit_deviceNameValue = new QLineEdit(deviceInfoPanel);
        lineEdit_deviceNameValue->setObjectName(QString::fromUtf8("lineEdit_deviceNameValue"));

        formLayout_deviceInfo->setWidget(1, QFormLayout::FieldRole, lineEdit_deviceNameValue);

        lineEdit_macValue = new QLineEdit(deviceInfoPanel);
        lineEdit_macValue->setObjectName(QString::fromUtf8("lineEdit_macValue"));

        formLayout_deviceInfo->setWidget(2, QFormLayout::FieldRole, lineEdit_macValue);

        lineEdit_ipValue = new QLineEdit(deviceInfoPanel);
        lineEdit_ipValue->setObjectName(QString::fromUtf8("lineEdit_ipValue"));

        formLayout_deviceInfo->setWidget(3, QFormLayout::FieldRole, lineEdit_ipValue);

        lineEdit_maskValue = new QLineEdit(deviceInfoPanel);
        lineEdit_maskValue->setObjectName(QString::fromUtf8("lineEdit_maskValue"));

        formLayout_deviceInfo->setWidget(4, QFormLayout::FieldRole, lineEdit_maskValue);

        lineEdit_gatewayValue = new QLineEdit(deviceInfoPanel);
        lineEdit_gatewayValue->setObjectName(QString::fromUtf8("lineEdit_gatewayValue"));

        formLayout_deviceInfo->setWidget(5, QFormLayout::FieldRole, lineEdit_gatewayValue);


        verticalLayout_deviceInfo->addLayout(formLayout_deviceInfo);


        verticalLayout_leftPanel->addWidget(deviceInfoPanel);


        horizontalLayout_contentCard->addWidget(leftPanel);

        rightPanel = new QFrame(contentCard);
        rightPanel->setObjectName(QString::fromUtf8("rightPanel"));
        rightPanel->setMinimumSize(QSize(70, 0));
        rightPanel->setFrameShape(QFrame::NoFrame);
        verticalLayout_rightPanel = new QVBoxLayout(rightPanel);
        verticalLayout_rightPanel->setSpacing(20);
        verticalLayout_rightPanel->setObjectName(QString::fromUtf8("verticalLayout_rightPanel"));
        verticalLayout_rightPanel->setContentsMargins(76, 20, 76, 5);
        horizontalLayout_cardClose = new QHBoxLayout();
        horizontalLayout_cardClose->setObjectName(QString::fromUtf8("horizontalLayout_cardClose"));
        horizontalSpacer_cardClose = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_cardClose->addItem(horizontalSpacer_cardClose);

        closeCardButton = new QToolButton(rightPanel);
        closeCardButton->setObjectName(QString::fromUtf8("closeCardButton"));
        closeCardButton->setAutoRaise(true);

        horizontalLayout_cardClose->addWidget(closeCardButton);


        verticalLayout_rightPanel->addLayout(horizontalLayout_cardClose);

        verticalSpacer_topRight = new QSpacerItem(20, 6, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_rightPanel->addItem(verticalSpacer_topRight);

        horizontalLayout_logo = new QHBoxLayout();
        horizontalLayout_logo->setObjectName(QString::fromUtf8("horizontalLayout_logo"));
        horizontalSpacer_logoLeft = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_logo->addItem(horizontalSpacer_logoLeft);

        logoMarkLargeLabel = new QLabel(rightPanel);
        logoMarkLargeLabel->setObjectName(QString::fromUtf8("logoMarkLargeLabel"));
        logoMarkLargeLabel->setMinimumSize(QSize(100, 72));
        logoMarkLargeLabel->setMaximumSize(QSize(72, 72));
        logoMarkLargeLabel->setPixmap(QPixmap(QString::fromUtf8(":/images/LOGO.png")));
        logoMarkLargeLabel->setScaledContents(true);

        horizontalLayout_logo->addWidget(logoMarkLargeLabel);

        logoTitleLabel = new QLabel(rightPanel);
        logoTitleLabel->setObjectName(QString::fromUtf8("logoTitleLabel"));
        QFont font;
        font.setFamily(QString::fromUtf8("Tlwg Typo"));
        logoTitleLabel->setFont(font);

        horizontalLayout_logo->addWidget(logoTitleLabel);

        horizontalSpacer_logoRight = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_logo->addItem(horizontalSpacer_logoRight);


        verticalLayout_rightPanel->addLayout(horizontalLayout_logo);

        verticalSpacer_logoGap = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_rightPanel->addItem(verticalSpacer_logoGap);

        deviceFieldFrame = new QFrame(rightPanel);
        deviceFieldFrame->setObjectName(QString::fromUtf8("deviceFieldFrame"));
        deviceFieldFrame->setMinimumSize(QSize(0, 58));
        deviceFieldFrame->setMaximumSize(QSize(16777215, 58));
        deviceFieldFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_deviceField = new QHBoxLayout(deviceFieldFrame);
        horizontalLayout_deviceField->setSpacing(10);
        horizontalLayout_deviceField->setObjectName(QString::fromUtf8("horizontalLayout_deviceField"));
        horizontalLayout_deviceField->setContentsMargins(18, 0, 14, 0);
        deviceFieldIconLabel = new QLabel(deviceFieldFrame);
        deviceFieldIconLabel->setObjectName(QString::fromUtf8("deviceFieldIconLabel"));
        deviceFieldIconLabel->setMinimumSize(QSize(24, 24));
        deviceFieldIconLabel->setMaximumSize(QSize(24, 24));
        deviceFieldIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/camera.svg")));
        deviceFieldIconLabel->setScaledContents(true);

        horizontalLayout_deviceField->addWidget(deviceFieldIconLabel);

        deviceComboBox = new QComboBox(deviceFieldFrame);
        deviceComboBox->setObjectName(QString::fromUtf8("deviceComboBox"));

        horizontalLayout_deviceField->addWidget(deviceComboBox);


        verticalLayout_rightPanel->addWidget(deviceFieldFrame);

        userFieldFrame = new QFrame(rightPanel);
        userFieldFrame->setObjectName(QString::fromUtf8("userFieldFrame"));
        userFieldFrame->setMinimumSize(QSize(0, 58));
        userFieldFrame->setMaximumSize(QSize(16777215, 58));
        userFieldFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_userField = new QHBoxLayout(userFieldFrame);
        horizontalLayout_userField->setSpacing(10);
        horizontalLayout_userField->setObjectName(QString::fromUtf8("horizontalLayout_userField"));
        horizontalLayout_userField->setContentsMargins(18, 0, 14, 0);
        userFieldIconLabel = new QLabel(userFieldFrame);
        userFieldIconLabel->setObjectName(QString::fromUtf8("userFieldIconLabel"));
        userFieldIconLabel->setMinimumSize(QSize(24, 24));
        userFieldIconLabel->setMaximumSize(QSize(24, 24));
        userFieldIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/user.svg")));
        userFieldIconLabel->setScaledContents(true);

        horizontalLayout_userField->addWidget(userFieldIconLabel);

        userComboBox = new QComboBox(userFieldFrame);
        userComboBox->setObjectName(QString::fromUtf8("userComboBox"));

        horizontalLayout_userField->addWidget(userComboBox);


        verticalLayout_rightPanel->addWidget(userFieldFrame);

        passwordFieldFrame = new QFrame(rightPanel);
        passwordFieldFrame->setObjectName(QString::fromUtf8("passwordFieldFrame"));
        passwordFieldFrame->setMinimumSize(QSize(0, 58));
        passwordFieldFrame->setMaximumSize(QSize(16777215, 58));
        passwordFieldFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_passwordField = new QHBoxLayout(passwordFieldFrame);
        horizontalLayout_passwordField->setSpacing(10);
        horizontalLayout_passwordField->setObjectName(QString::fromUtf8("horizontalLayout_passwordField"));
        horizontalLayout_passwordField->setContentsMargins(18, 0, 14, 0);
        passwordFieldIconLabel = new QLabel(passwordFieldFrame);
        passwordFieldIconLabel->setObjectName(QString::fromUtf8("passwordFieldIconLabel"));
        passwordFieldIconLabel->setMinimumSize(QSize(24, 24));
        passwordFieldIconLabel->setMaximumSize(QSize(24, 24));
        passwordFieldIconLabel->setPixmap(QPixmap(QString::fromUtf8(":/icons/lock.svg")));
        passwordFieldIconLabel->setScaledContents(true);

        horizontalLayout_passwordField->addWidget(passwordFieldIconLabel);

        passwordLineEdit = new QLineEdit(passwordFieldFrame);
        passwordLineEdit->setObjectName(QString::fromUtf8("passwordLineEdit"));

        horizontalLayout_passwordField->addWidget(passwordLineEdit);

        passwordToggleButton = new QToolButton(passwordFieldFrame);
        passwordToggleButton->setObjectName(QString::fromUtf8("passwordToggleButton"));
        QIcon icon8;
        icon8.addFile(QString::fromUtf8(":/icons/eye.svg"), QSize(), QIcon::Normal, QIcon::Off);
        passwordToggleButton->setIcon(icon8);
        passwordToggleButton->setIconSize(QSize(18, 18));
        passwordToggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        passwordToggleButton->setAutoRaise(true);

        horizontalLayout_passwordField->addWidget(passwordToggleButton);


        verticalLayout_rightPanel->addWidget(passwordFieldFrame);

        verticalSpacer_rightStretch = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_rightPanel->addItem(verticalSpacer_rightStretch);

        horizontalLayout_loginButton = new QHBoxLayout();
        horizontalLayout_loginButton->setObjectName(QString::fromUtf8("horizontalLayout_loginButton"));
        horizontalSpacer_loginLeft = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_loginButton->addItem(horizontalSpacer_loginLeft);

        loginButton = new QPushButton(rightPanel);
        loginButton->setObjectName(QString::fromUtf8("loginButton"));

        horizontalLayout_loginButton->addWidget(loginButton);

        horizontalSpacer_loginRight = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_loginButton->addItem(horizontalSpacer_loginRight);


        verticalLayout_rightPanel->addLayout(horizontalLayout_loginButton);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_rightPanel->addItem(verticalSpacer);


        horizontalLayout_contentCard->addWidget(rightPanel);


        horizontalLayout_contentArea->addWidget(contentCard);

        horizontalSpacer_right = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_contentArea->addItem(horizontalSpacer_right);


        verticalLayout_root->addWidget(contentArea);


        retranslateUi(LoginWindow);

        QMetaObject::connectSlotsByName(LoginWindow);
    } // setupUi

    void retranslateUi(QDialog *LoginWindow)
    {
        LoginWindow->setWindowTitle(QCoreApplication::translate("LoginWindow", "\347\231\273\345\275\225\347\225\214\351\235\242", nullptr));
        headerTitleLabel->setText(QCoreApplication::translate("LoginWindow", "\346\261\207\344\274\227\346\231\272\346\205\247", nullptr));
        navCameraButton->setText(QCoreApplication::translate("LoginWindow", "\347\233\270\346\234\272\350\256\276\347\275\256", nullptr));
        navPlanButton->setText(QCoreApplication::translate("LoginWindow", "\346\226\271\346\241\210\347\256\241\347\220\206", nullptr));
        navIoButton->setText(QCoreApplication::translate("LoginWindow", "IO\350\256\276\347\275\256", nullptr));
        navCommButton->setText(QCoreApplication::translate("LoginWindow", "\351\200\232\344\277\241\350\256\276\347\275\256", nullptr));
        navAssistButton->setText(QCoreApplication::translate("LoginWindow", "\350\276\205\345\212\251\345\267\245\345\205\267", nullptr));
        navMonitorButton->setText(QCoreApplication::translate("LoginWindow", "\350\277\220\350\241\214\347\233\221\346\216\247", nullptr));
        btn_system->setText(QCoreApplication::translate("LoginWindow", "\347\263\273\347\273\237", nullptr));
        btn_help->setText(QCoreApplication::translate("LoginWindow", "\345\270\256\345\212\251", nullptr));
        headerMinimizeButton->setText(QCoreApplication::translate("LoginWindow", "_", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("LoginWindow", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("LoginWindow", "\303\227", nullptr));
        sectionTitleLabel->setText(QCoreApplication::translate("LoginWindow", "\350\256\276\345\244\207\345\210\227\350\241\250", nullptr));
        adapterLabel->setText(QCoreApplication::translate("LoginWindow", "\347\275\221\345\215\241", nullptr));
        deviceNameLabel->setText(QCoreApplication::translate("LoginWindow", "\350\256\276\345\244\207\345\220\215\347\247\260", nullptr));
        macLabel->setText(QCoreApplication::translate("LoginWindow", "\347\211\251\347\220\206\345\234\260\345\235\200", nullptr));
        ipLabel->setText(QCoreApplication::translate("LoginWindow", "IP\345\234\260\345\235\200", nullptr));
        maskLabel->setText(QCoreApplication::translate("LoginWindow", "\345\255\220\347\275\221\346\216\251\347\240\201", nullptr));
        gatewayLabel->setText(QCoreApplication::translate("LoginWindow", "\347\275\221\345\205\263", nullptr));
        lineEdit_adapterValue->setText(QCoreApplication::translate("LoginWindow", "Realtek PCIe GbE Family Controller", nullptr));
        lineEdit_deviceNameValue->setText(QCoreApplication::translate("LoginWindow", "--", nullptr));
        lineEdit_macValue->setText(QCoreApplication::translate("LoginWindow", "34:BD:20:8A:21:AD", nullptr));
        lineEdit_ipValue->setText(QCoreApplication::translate("LoginWindow", "192.168.137.14", nullptr));
        lineEdit_maskValue->setText(QCoreApplication::translate("LoginWindow", "255.255.255.0", nullptr));
        lineEdit_gatewayValue->setText(QCoreApplication::translate("LoginWindow", "192.168.137.1", nullptr));
        closeCardButton->setText(QCoreApplication::translate("LoginWindow", "\303\227", nullptr));
        logoTitleLabel->setText(QCoreApplication::translate("LoginWindow", "\346\261\207\344\274\227\346\231\272\346\205\247", nullptr));
        passwordLineEdit->setPlaceholderText(QCoreApplication::translate("LoginWindow", "\350\257\267\350\276\223\345\205\245\345\257\206\347\240\201", nullptr));
        passwordToggleButton->setText(QCoreApplication::translate("LoginWindow", "\346\230\276\347\244\272", nullptr));
        loginButton->setText(QCoreApplication::translate("LoginWindow", "\347\231\273\345\275\225", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LoginWindow: public Ui_LoginWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGINWINDOW_H
