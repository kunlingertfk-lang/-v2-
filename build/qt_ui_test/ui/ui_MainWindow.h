/********************************************************************************
** Form generated from reading UI file 'MainWindow.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout_root;
    QFrame *headerBar;
    QHBoxLayout *horizontalLayout_header;
    QWidget *headerBrandWidget;
    QHBoxLayout *horizontalLayout_brand;
    QLabel *headerLogoLabel;
    QLabel *headerTitleLabel;
    QToolButton *mainNavCameraButton;
    QToolButton *mainNavPlanButton;
    QToolButton *mainNavIoButton;
    QToolButton *mainNavCommButton;
    QToolButton *mainNavAssistButton;
    QToolButton *mainNavMonitorButton;
    QSpacerItem *horizontalSpacer_header;
    QPushButton *btn_system_2;
    QPushButton *btn_help_2;
    QToolButton *headerUserButton;
    QComboBox *headerDeviceComboBox;
    QToolButton *headerMinimizeButton;
    QToolButton *headerMaximizeButton;
    QToolButton *headerCloseButton;
    QFrame *mainBodyFrame;
    QHBoxLayout *horizontalLayout_body;
    QFrame *workspaceFrame;
    QVBoxLayout *verticalLayout_workspace;
    QFrame *actionBar;
    QHBoxLayout *horizontalLayout_actionBar;
    QComboBox *comboBox;
    QSpacerItem *horizontalSpacer_action;
    QPushButton *planSettingButton;
    QPushButton *singleRunButton;
    QPushButton *stopRunButton;
    QFrame *viewerToolbar;
    QHBoxLayout *horizontalLayout_viewerToolbar;
    QSpacerItem *horizontalSpacer_viewerLeft;
    QToolButton *fitViewButton;
    QToolButton *captureButton;
    QToolButton *gridButton;
    QToolButton *zoomOutButton;
    QLabel *zoomValueLabel;
    QToolButton *zoomInButton;
    QToolButton *fullScreenButton;
    QGraphicsView *previewGraphicsView;
    QFrame *statusStrip;
    QHBoxLayout *horizontalLayout_statusStrip;
    QLabel *totalDurationLabel;
    QLabel *captureDurationLabel;
    QLabel *algorithmDurationLabel;
    QLabel *toolDurationLabel;
    QLabel *templateDurationLabel;
    QSpacerItem *horizontalSpacer_status;
    QLabel *cursorLabel;
    QFrame *resultBar;
    QHBoxLayout *horizontalLayout_resultBar;
    QPushButton *toolResultButton;
    QPushButton *resultResultButton;
    QSpacerItem *horizontalSpacer_resultBar;
    QToolButton *expandResultButton;
    QToolButton *collapseResultButton;
    QFrame *sidebarFrame;
    QVBoxLayout *verticalLayout_sidebar;
    QFrame *summaryCard;
    QVBoxLayout *verticalLayout_summaryCard;
    QHBoxLayout *horizontalLayout_summaryTop;
    QLabel *ngBadgeLabel;
    QSpacerItem *horizontalSpacer_summaryTop;
    QPushButton *clearSummaryButton;
    QHBoxLayout *horizontalLayout_summaryMain;
    QVBoxLayout *verticalLayout_okRate;
    QLabel *okRateValueLabel;
    QLabel *okRateTextLabel;
    QGridLayout *gridLayout_summaryStats;
    QLabel *summaryTotalTitleLabel;
    QLabel *totalCountValueLabel;
    QLabel *okCountTextLabel;
    QLabel *okCountValueLabel;
    QLabel *ngCountTextLabel;
    QLabel *ngCountValueLabel;
    QVBoxLayout *verticalLayout_runtime;
    QLabel *runtimeValueLabel;
    QLabel *runtimeTextLabel;
    QHBoxLayout *horizontalLayout_toolsTitle;
    QLabel *toolsTitleLabel;
    QSpacerItem *horizontalSpacer_toolsTitle;
    QToolButton *toolsSettingButton;
    QTableWidget *toolsTableWidget;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(1321, 926);
        MainWindow->setMinimumSize(QSize(70, 0));
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        verticalLayout_root = new QVBoxLayout(centralwidget);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        headerBar = new QFrame(centralwidget);
        headerBar->setObjectName(QString::fromUtf8("headerBar"));
        headerBar->setMinimumSize(QSize(0, 82));
        headerBar->setMaximumSize(QSize(16777215, 82));
        headerBar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_header = new QHBoxLayout(headerBar);
        horizontalLayout_header->setSpacing(6);
        horizontalLayout_header->setObjectName(QString::fromUtf8("horizontalLayout_header"));
        horizontalLayout_header->setContentsMargins(12, 10, 12, 10);
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

        mainNavCameraButton = new QToolButton(headerBar);
        mainNavCameraButton->setObjectName(QString::fromUtf8("mainNavCameraButton"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/camera.svg"), QSize(), QIcon::Normal, QIcon::Off);
        mainNavCameraButton->setIcon(icon);
        mainNavCameraButton->setIconSize(QSize(28, 28));
        mainNavCameraButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        mainNavCameraButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(mainNavCameraButton);

        mainNavPlanButton = new QToolButton(headerBar);
        mainNavPlanButton->setObjectName(QString::fromUtf8("mainNavPlanButton"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/plan.svg"), QSize(), QIcon::Normal, QIcon::Off);
        mainNavPlanButton->setIcon(icon1);
        mainNavPlanButton->setIconSize(QSize(28, 28));
        mainNavPlanButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        mainNavPlanButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(mainNavPlanButton);

        mainNavIoButton = new QToolButton(headerBar);
        mainNavIoButton->setObjectName(QString::fromUtf8("mainNavIoButton"));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/io.svg"), QSize(), QIcon::Normal, QIcon::Off);
        mainNavIoButton->setIcon(icon2);
        mainNavIoButton->setIconSize(QSize(28, 28));
        mainNavIoButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        mainNavIoButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(mainNavIoButton);

        mainNavCommButton = new QToolButton(headerBar);
        mainNavCommButton->setObjectName(QString::fromUtf8("mainNavCommButton"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icons/comm.svg"), QSize(), QIcon::Normal, QIcon::Off);
        mainNavCommButton->setIcon(icon3);
        mainNavCommButton->setIconSize(QSize(28, 28));
        mainNavCommButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        mainNavCommButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(mainNavCommButton);

        mainNavAssistButton = new QToolButton(headerBar);
        mainNavAssistButton->setObjectName(QString::fromUtf8("mainNavAssistButton"));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/tool.svg"), QSize(), QIcon::Normal, QIcon::Off);
        mainNavAssistButton->setIcon(icon4);
        mainNavAssistButton->setIconSize(QSize(28, 28));
        mainNavAssistButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        mainNavAssistButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(mainNavAssistButton);

        mainNavMonitorButton = new QToolButton(headerBar);
        mainNavMonitorButton->setObjectName(QString::fromUtf8("mainNavMonitorButton"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/monitor.svg"), QSize(), QIcon::Normal, QIcon::Off);
        mainNavMonitorButton->setIcon(icon5);
        mainNavMonitorButton->setIconSize(QSize(28, 28));
        mainNavMonitorButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        mainNavMonitorButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(mainNavMonitorButton);

        horizontalSpacer_header = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_header->addItem(horizontalSpacer_header);

        btn_system_2 = new QPushButton(headerBar);
        btn_system_2->setObjectName(QString::fromUtf8("btn_system_2"));

        horizontalLayout_header->addWidget(btn_system_2);

        btn_help_2 = new QPushButton(headerBar);
        btn_help_2->setObjectName(QString::fromUtf8("btn_help_2"));

        horizontalLayout_header->addWidget(btn_help_2);

        headerUserButton = new QToolButton(headerBar);
        headerUserButton->setObjectName(QString::fromUtf8("headerUserButton"));
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/icons/user.svg"), QSize(), QIcon::Normal, QIcon::Off);
        headerUserButton->setIcon(icon6);
        headerUserButton->setIconSize(QSize(20, 20));
        headerUserButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        headerUserButton->setAutoRaise(true);

        horizontalLayout_header->addWidget(headerUserButton);

        headerDeviceComboBox = new QComboBox(headerBar);
        headerDeviceComboBox->setObjectName(QString::fromUtf8("headerDeviceComboBox"));
        headerDeviceComboBox->setMinimumSize(QSize(200, 38));

        horizontalLayout_header->addWidget(headerDeviceComboBox);

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

        mainBodyFrame = new QFrame(centralwidget);
        mainBodyFrame->setObjectName(QString::fromUtf8("mainBodyFrame"));
        mainBodyFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_body = new QHBoxLayout(mainBodyFrame);
        horizontalLayout_body->setSpacing(0);
        horizontalLayout_body->setObjectName(QString::fromUtf8("horizontalLayout_body"));
        horizontalLayout_body->setContentsMargins(0, 0, 0, 0);
        workspaceFrame = new QFrame(mainBodyFrame);
        workspaceFrame->setObjectName(QString::fromUtf8("workspaceFrame"));
        workspaceFrame->setFrameShape(QFrame::NoFrame);
        verticalLayout_workspace = new QVBoxLayout(workspaceFrame);
        verticalLayout_workspace->setSpacing(0);
        verticalLayout_workspace->setObjectName(QString::fromUtf8("verticalLayout_workspace"));
        verticalLayout_workspace->setContentsMargins(0, 0, 0, 0);
        actionBar = new QFrame(workspaceFrame);
        actionBar->setObjectName(QString::fromUtf8("actionBar"));
        actionBar->setMinimumSize(QSize(0, 74));
        actionBar->setMaximumSize(QSize(16777215, 74));
        actionBar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_actionBar = new QHBoxLayout(actionBar);
        horizontalLayout_actionBar->setSpacing(14);
        horizontalLayout_actionBar->setObjectName(QString::fromUtf8("horizontalLayout_actionBar"));
        horizontalLayout_actionBar->setContentsMargins(22, 12, 22, 12);
        comboBox = new QComboBox(actionBar);
        comboBox->addItem(QString());
        comboBox->addItem(QString());
        comboBox->setObjectName(QString::fromUtf8("comboBox"));

        horizontalLayout_actionBar->addWidget(comboBox);

        horizontalSpacer_action = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_actionBar->addItem(horizontalSpacer_action);

        planSettingButton = new QPushButton(actionBar);
        planSettingButton->setObjectName(QString::fromUtf8("planSettingButton"));
        QIcon icon7;
        icon7.addFile(QString::fromUtf8(":/icons/settings.svg"), QSize(), QIcon::Normal, QIcon::Off);
        planSettingButton->setIcon(icon7);
        planSettingButton->setIconSize(QSize(24, 24));

        horizontalLayout_actionBar->addWidget(planSettingButton);

        singleRunButton = new QPushButton(actionBar);
        singleRunButton->setObjectName(QString::fromUtf8("singleRunButton"));
        QIcon icon8;
        icon8.addFile(QString::fromUtf8(":/icons/play.svg"), QSize(), QIcon::Normal, QIcon::Off);
        singleRunButton->setIcon(icon8);
        singleRunButton->setIconSize(QSize(24, 24));

        horizontalLayout_actionBar->addWidget(singleRunButton);

        stopRunButton = new QPushButton(actionBar);
        stopRunButton->setObjectName(QString::fromUtf8("stopRunButton"));
        QIcon icon9;
        icon9.addFile(QString::fromUtf8(":/icons/refresh.svg"), QSize(), QIcon::Normal, QIcon::Off);
        stopRunButton->setIcon(icon9);
        stopRunButton->setIconSize(QSize(24, 24));

        horizontalLayout_actionBar->addWidget(stopRunButton);


        verticalLayout_workspace->addWidget(actionBar);

        viewerToolbar = new QFrame(workspaceFrame);
        viewerToolbar->setObjectName(QString::fromUtf8("viewerToolbar"));
        viewerToolbar->setMinimumSize(QSize(0, 44));
        viewerToolbar->setMaximumSize(QSize(16777215, 44));
        viewerToolbar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_viewerToolbar = new QHBoxLayout(viewerToolbar);
        horizontalLayout_viewerToolbar->setSpacing(10);
        horizontalLayout_viewerToolbar->setObjectName(QString::fromUtf8("horizontalLayout_viewerToolbar"));
        horizontalLayout_viewerToolbar->setContentsMargins(18, 4, 18, 4);
        horizontalSpacer_viewerLeft = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_viewerToolbar->addItem(horizontalSpacer_viewerLeft);

        fitViewButton = new QToolButton(viewerToolbar);
        fitViewButton->setObjectName(QString::fromUtf8("fitViewButton"));
        QIcon icon10;
        icon10.addFile(QString::fromUtf8(":/icons/fit.svg"), QSize(), QIcon::Normal, QIcon::Off);
        fitViewButton->setIcon(icon10);
        fitViewButton->setIconSize(QSize(22, 22));
        fitViewButton->setAutoRaise(true);

        horizontalLayout_viewerToolbar->addWidget(fitViewButton);

        captureButton = new QToolButton(viewerToolbar);
        captureButton->setObjectName(QString::fromUtf8("captureButton"));
        captureButton->setIcon(icon);
        captureButton->setIconSize(QSize(22, 22));
        captureButton->setAutoRaise(true);

        horizontalLayout_viewerToolbar->addWidget(captureButton);

        gridButton = new QToolButton(viewerToolbar);
        gridButton->setObjectName(QString::fromUtf8("gridButton"));
        QIcon icon11;
        icon11.addFile(QString::fromUtf8(":/icons/grid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        gridButton->setIcon(icon11);
        gridButton->setIconSize(QSize(22, 22));
        gridButton->setAutoRaise(true);

        horizontalLayout_viewerToolbar->addWidget(gridButton);

        zoomOutButton = new QToolButton(viewerToolbar);
        zoomOutButton->setObjectName(QString::fromUtf8("zoomOutButton"));
        zoomOutButton->setAutoRaise(true);

        horizontalLayout_viewerToolbar->addWidget(zoomOutButton);

        zoomValueLabel = new QLabel(viewerToolbar);
        zoomValueLabel->setObjectName(QString::fromUtf8("zoomValueLabel"));

        horizontalLayout_viewerToolbar->addWidget(zoomValueLabel);

        zoomInButton = new QToolButton(viewerToolbar);
        zoomInButton->setObjectName(QString::fromUtf8("zoomInButton"));
        QIcon icon12;
        icon12.addFile(QString::fromUtf8(":/icons/zoom.svg"), QSize(), QIcon::Normal, QIcon::Off);
        zoomInButton->setIcon(icon12);
        zoomInButton->setIconSize(QSize(22, 22));
        zoomInButton->setAutoRaise(true);

        horizontalLayout_viewerToolbar->addWidget(zoomInButton);

        fullScreenButton = new QToolButton(viewerToolbar);
        fullScreenButton->setObjectName(QString::fromUtf8("fullScreenButton"));
        QIcon icon13;
        icon13.addFile(QString::fromUtf8(":/icons/fullscreen.svg"), QSize(), QIcon::Normal, QIcon::Off);
        fullScreenButton->setIcon(icon13);
        fullScreenButton->setIconSize(QSize(22, 22));
        fullScreenButton->setAutoRaise(true);

        horizontalLayout_viewerToolbar->addWidget(fullScreenButton);


        verticalLayout_workspace->addWidget(viewerToolbar);

        previewGraphicsView = new QGraphicsView(workspaceFrame);
        previewGraphicsView->setObjectName(QString::fromUtf8("previewGraphicsView"));
        previewGraphicsView->setFrameShape(QFrame::NoFrame);
        previewGraphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        previewGraphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        previewGraphicsView->setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);

        verticalLayout_workspace->addWidget(previewGraphicsView);

        statusStrip = new QFrame(workspaceFrame);
        statusStrip->setObjectName(QString::fromUtf8("statusStrip"));
        statusStrip->setMinimumSize(QSize(0, 36));
        statusStrip->setMaximumSize(QSize(16777215, 36));
        statusStrip->setFrameShape(QFrame::NoFrame);
        horizontalLayout_statusStrip = new QHBoxLayout(statusStrip);
        horizontalLayout_statusStrip->setSpacing(22);
        horizontalLayout_statusStrip->setObjectName(QString::fromUtf8("horizontalLayout_statusStrip"));
        horizontalLayout_statusStrip->setContentsMargins(22, 6, 22, 6);
        totalDurationLabel = new QLabel(statusStrip);
        totalDurationLabel->setObjectName(QString::fromUtf8("totalDurationLabel"));

        horizontalLayout_statusStrip->addWidget(totalDurationLabel);

        captureDurationLabel = new QLabel(statusStrip);
        captureDurationLabel->setObjectName(QString::fromUtf8("captureDurationLabel"));

        horizontalLayout_statusStrip->addWidget(captureDurationLabel);

        algorithmDurationLabel = new QLabel(statusStrip);
        algorithmDurationLabel->setObjectName(QString::fromUtf8("algorithmDurationLabel"));

        horizontalLayout_statusStrip->addWidget(algorithmDurationLabel);

        toolDurationLabel = new QLabel(statusStrip);
        toolDurationLabel->setObjectName(QString::fromUtf8("toolDurationLabel"));

        horizontalLayout_statusStrip->addWidget(toolDurationLabel);

        templateDurationLabel = new QLabel(statusStrip);
        templateDurationLabel->setObjectName(QString::fromUtf8("templateDurationLabel"));

        horizontalLayout_statusStrip->addWidget(templateDurationLabel);

        horizontalSpacer_status = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_statusStrip->addItem(horizontalSpacer_status);

        cursorLabel = new QLabel(statusStrip);
        cursorLabel->setObjectName(QString::fromUtf8("cursorLabel"));

        horizontalLayout_statusStrip->addWidget(cursorLabel);


        verticalLayout_workspace->addWidget(statusStrip);

        resultBar = new QFrame(workspaceFrame);
        resultBar->setObjectName(QString::fromUtf8("resultBar"));
        resultBar->setMinimumSize(QSize(0, 48));
        resultBar->setMaximumSize(QSize(16777215, 48));
        resultBar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_resultBar = new QHBoxLayout(resultBar);
        horizontalLayout_resultBar->setSpacing(0);
        horizontalLayout_resultBar->setObjectName(QString::fromUtf8("horizontalLayout_resultBar"));
        horizontalLayout_resultBar->setContentsMargins(0, 0, 12, 0);
        toolResultButton = new QPushButton(resultBar);
        toolResultButton->setObjectName(QString::fromUtf8("toolResultButton"));
        toolResultButton->setCheckable(true);
        toolResultButton->setChecked(true);

        horizontalLayout_resultBar->addWidget(toolResultButton);

        resultResultButton = new QPushButton(resultBar);
        resultResultButton->setObjectName(QString::fromUtf8("resultResultButton"));
        resultResultButton->setCheckable(true);

        horizontalLayout_resultBar->addWidget(resultResultButton);

        horizontalSpacer_resultBar = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_resultBar->addItem(horizontalSpacer_resultBar);

        expandResultButton = new QToolButton(resultBar);
        expandResultButton->setObjectName(QString::fromUtf8("expandResultButton"));
        expandResultButton->setAutoRaise(true);

        horizontalLayout_resultBar->addWidget(expandResultButton);

        collapseResultButton = new QToolButton(resultBar);
        collapseResultButton->setObjectName(QString::fromUtf8("collapseResultButton"));
        collapseResultButton->setAutoRaise(true);

        horizontalLayout_resultBar->addWidget(collapseResultButton);


        verticalLayout_workspace->addWidget(resultBar);


        horizontalLayout_body->addWidget(workspaceFrame);

        sidebarFrame = new QFrame(mainBodyFrame);
        sidebarFrame->setObjectName(QString::fromUtf8("sidebarFrame"));
        sidebarFrame->setMinimumSize(QSize(420, 0));
        sidebarFrame->setMaximumSize(QSize(420, 16777215));
        sidebarFrame->setFrameShape(QFrame::NoFrame);
        verticalLayout_sidebar = new QVBoxLayout(sidebarFrame);
        verticalLayout_sidebar->setSpacing(22);
        verticalLayout_sidebar->setObjectName(QString::fromUtf8("verticalLayout_sidebar"));
        verticalLayout_sidebar->setContentsMargins(26, 16, 16, 16);
        summaryCard = new QFrame(sidebarFrame);
        summaryCard->setObjectName(QString::fromUtf8("summaryCard"));
        summaryCard->setMinimumSize(QSize(0, 186));
        summaryCard->setMaximumSize(QSize(16777215, 186));
        summaryCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_summaryCard = new QVBoxLayout(summaryCard);
        verticalLayout_summaryCard->setSpacing(6);
        verticalLayout_summaryCard->setObjectName(QString::fromUtf8("verticalLayout_summaryCard"));
        verticalLayout_summaryCard->setContentsMargins(0, 0, 0, 0);
        horizontalLayout_summaryTop = new QHBoxLayout();
        horizontalLayout_summaryTop->setObjectName(QString::fromUtf8("horizontalLayout_summaryTop"));
        horizontalLayout_summaryTop->setContentsMargins(0, 0, 8, 0);
        ngBadgeLabel = new QLabel(summaryCard);
        ngBadgeLabel->setObjectName(QString::fromUtf8("ngBadgeLabel"));

        horizontalLayout_summaryTop->addWidget(ngBadgeLabel);

        horizontalSpacer_summaryTop = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_summaryTop->addItem(horizontalSpacer_summaryTop);

        clearSummaryButton = new QPushButton(summaryCard);
        clearSummaryButton->setObjectName(QString::fromUtf8("clearSummaryButton"));
        QIcon icon14;
        icon14.addFile(QString::fromUtf8(":/icons/trash.svg"), QSize(), QIcon::Normal, QIcon::Off);
        clearSummaryButton->setIcon(icon14);
        clearSummaryButton->setIconSize(QSize(18, 18));

        horizontalLayout_summaryTop->addWidget(clearSummaryButton);


        verticalLayout_summaryCard->addLayout(horizontalLayout_summaryTop);

        horizontalLayout_summaryMain = new QHBoxLayout();
        horizontalLayout_summaryMain->setSpacing(16);
        horizontalLayout_summaryMain->setObjectName(QString::fromUtf8("horizontalLayout_summaryMain"));
        horizontalLayout_summaryMain->setContentsMargins(18, 0, 18, 14);
        verticalLayout_okRate = new QVBoxLayout();
        verticalLayout_okRate->setSpacing(0);
        verticalLayout_okRate->setObjectName(QString::fromUtf8("verticalLayout_okRate"));
        okRateValueLabel = new QLabel(summaryCard);
        okRateValueLabel->setObjectName(QString::fromUtf8("okRateValueLabel"));

        verticalLayout_okRate->addWidget(okRateValueLabel);

        okRateTextLabel = new QLabel(summaryCard);
        okRateTextLabel->setObjectName(QString::fromUtf8("okRateTextLabel"));

        verticalLayout_okRate->addWidget(okRateTextLabel);


        horizontalLayout_summaryMain->addLayout(verticalLayout_okRate);

        gridLayout_summaryStats = new QGridLayout();
        gridLayout_summaryStats->setObjectName(QString::fromUtf8("gridLayout_summaryStats"));
        gridLayout_summaryStats->setHorizontalSpacing(18);
        gridLayout_summaryStats->setVerticalSpacing(10);
        summaryTotalTitleLabel = new QLabel(summaryCard);
        summaryTotalTitleLabel->setObjectName(QString::fromUtf8("summaryTotalTitleLabel"));

        gridLayout_summaryStats->addWidget(summaryTotalTitleLabel, 0, 0, 1, 2);

        totalCountValueLabel = new QLabel(summaryCard);
        totalCountValueLabel->setObjectName(QString::fromUtf8("totalCountValueLabel"));

        gridLayout_summaryStats->addWidget(totalCountValueLabel, 0, 2, 1, 1);

        okCountTextLabel = new QLabel(summaryCard);
        okCountTextLabel->setObjectName(QString::fromUtf8("okCountTextLabel"));

        gridLayout_summaryStats->addWidget(okCountTextLabel, 1, 0, 1, 1);

        okCountValueLabel = new QLabel(summaryCard);
        okCountValueLabel->setObjectName(QString::fromUtf8("okCountValueLabel"));

        gridLayout_summaryStats->addWidget(okCountValueLabel, 1, 1, 1, 1);

        ngCountTextLabel = new QLabel(summaryCard);
        ngCountTextLabel->setObjectName(QString::fromUtf8("ngCountTextLabel"));

        gridLayout_summaryStats->addWidget(ngCountTextLabel, 1, 2, 1, 1);

        ngCountValueLabel = new QLabel(summaryCard);
        ngCountValueLabel->setObjectName(QString::fromUtf8("ngCountValueLabel"));

        gridLayout_summaryStats->addWidget(ngCountValueLabel, 1, 3, 1, 1);

        verticalLayout_runtime = new QVBoxLayout();
        verticalLayout_runtime->setSpacing(4);
        verticalLayout_runtime->setObjectName(QString::fromUtf8("verticalLayout_runtime"));
        runtimeValueLabel = new QLabel(summaryCard);
        runtimeValueLabel->setObjectName(QString::fromUtf8("runtimeValueLabel"));

        verticalLayout_runtime->addWidget(runtimeValueLabel);

        runtimeTextLabel = new QLabel(summaryCard);
        runtimeTextLabel->setObjectName(QString::fromUtf8("runtimeTextLabel"));

        verticalLayout_runtime->addWidget(runtimeTextLabel);


        gridLayout_summaryStats->addLayout(verticalLayout_runtime, 0, 4, 2, 1);


        horizontalLayout_summaryMain->addLayout(gridLayout_summaryStats);


        verticalLayout_summaryCard->addLayout(horizontalLayout_summaryMain);


        verticalLayout_sidebar->addWidget(summaryCard);

        horizontalLayout_toolsTitle = new QHBoxLayout();
        horizontalLayout_toolsTitle->setObjectName(QString::fromUtf8("horizontalLayout_toolsTitle"));
        horizontalLayout_toolsTitle->setContentsMargins(0, 0, 0, 0);
        toolsTitleLabel = new QLabel(sidebarFrame);
        toolsTitleLabel->setObjectName(QString::fromUtf8("toolsTitleLabel"));

        horizontalLayout_toolsTitle->addWidget(toolsTitleLabel);

        horizontalSpacer_toolsTitle = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_toolsTitle->addItem(horizontalSpacer_toolsTitle);

        toolsSettingButton = new QToolButton(sidebarFrame);
        toolsSettingButton->setObjectName(QString::fromUtf8("toolsSettingButton"));
        toolsSettingButton->setIcon(icon7);
        toolsSettingButton->setIconSize(QSize(18, 18));
        toolsSettingButton->setAutoRaise(true);

        horizontalLayout_toolsTitle->addWidget(toolsSettingButton);


        verticalLayout_sidebar->addLayout(horizontalLayout_toolsTitle);

        toolsTableWidget = new QTableWidget(sidebarFrame);
        if (toolsTableWidget->columnCount() < 5)
            toolsTableWidget->setColumnCount(5);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        toolsTableWidget->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        toolsTableWidget->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        toolsTableWidget->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        toolsTableWidget->setHorizontalHeaderItem(3, __qtablewidgetitem3);
        QTableWidgetItem *__qtablewidgetitem4 = new QTableWidgetItem();
        toolsTableWidget->setHorizontalHeaderItem(4, __qtablewidgetitem4);
        toolsTableWidget->setObjectName(QString::fromUtf8("toolsTableWidget"));
        toolsTableWidget->setAlternatingRowColors(false);
        toolsTableWidget->setShowGrid(false);
        toolsTableWidget->setCornerButtonEnabled(false);
        toolsTableWidget->horizontalHeader()->setVisible(false);
        toolsTableWidget->verticalHeader()->setVisible(false);

        verticalLayout_sidebar->addWidget(toolsTableWidget);


        horizontalLayout_body->addWidget(sidebarFrame);

        horizontalLayout_body->setStretch(0, 3);
        horizontalLayout_body->setStretch(1, 1);

        verticalLayout_root->addWidget(mainBodyFrame);

        MainWindow->setCentralWidget(centralwidget);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\344\270\273\351\241\265\351\235\242", nullptr));
        headerTitleLabel->setText(QCoreApplication::translate("MainWindow", "\346\261\207\344\274\227\346\231\272\346\205\247", nullptr));
        mainNavCameraButton->setText(QCoreApplication::translate("MainWindow", "\347\233\270\346\234\272\350\256\276\347\275\256", nullptr));
        mainNavPlanButton->setText(QCoreApplication::translate("MainWindow", "\346\226\271\346\241\210\347\256\241\347\220\206", nullptr));
        mainNavIoButton->setText(QCoreApplication::translate("MainWindow", "IO\350\256\276\347\275\256", nullptr));
        mainNavCommButton->setText(QCoreApplication::translate("MainWindow", "\351\200\232\344\277\241\350\256\276\347\275\256", nullptr));
        mainNavAssistButton->setText(QCoreApplication::translate("MainWindow", "\350\276\205\345\212\251\345\267\245\345\205\267", nullptr));
        mainNavMonitorButton->setText(QCoreApplication::translate("MainWindow", "\350\277\220\350\241\214\347\233\221\346\216\247", nullptr));
        btn_system_2->setText(QCoreApplication::translate("MainWindow", "\347\263\273\347\273\237", nullptr));
        btn_help_2->setText(QCoreApplication::translate("MainWindow", "\345\270\256\345\212\251", nullptr));
        headerUserButton->setText(QCoreApplication::translate("MainWindow", "\347\256\241\347\220\206\345\221\230", nullptr));
        headerMinimizeButton->setText(QCoreApplication::translate("MainWindow", "_", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("MainWindow", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("MainWindow", "\303\227", nullptr));
        comboBox->setItemText(0, QCoreApplication::translate("MainWindow", "1", nullptr));
        comboBox->setItemText(1, QCoreApplication::translate("MainWindow", "2", nullptr));

        planSettingButton->setText(QCoreApplication::translate("MainWindow", "\346\226\271\346\241\210\350\256\276\347\275\256", nullptr));
        singleRunButton->setText(QCoreApplication::translate("MainWindow", "\345\215\225\346\254\241\350\277\220\350\241\214", nullptr));
        stopRunButton->setText(QCoreApplication::translate("MainWindow", "\350\277\236\347\273\255\350\277\220\350\241\214", nullptr));
        zoomOutButton->setText(QCoreApplication::translate("MainWindow", "-", nullptr));
        zoomValueLabel->setText(QCoreApplication::translate("MainWindow", "135%", nullptr));
        totalDurationLabel->setText(QCoreApplication::translate("MainWindow", "\346\200\273\344\275\223\350\200\227\346\227\266: --ms", nullptr));
        captureDurationLabel->setText(QCoreApplication::translate("MainWindow", "\345\207\272\345\233\276\350\200\227\346\227\266: --ms", nullptr));
        algorithmDurationLabel->setText(QCoreApplication::translate("MainWindow", "\347\256\227\346\263\225\350\200\227\346\227\266: --ms", nullptr));
        toolDurationLabel->setText(QCoreApplication::translate("MainWindow", "\345\267\245\345\205\267\350\200\227\346\227\266: --ms", nullptr));
        templateDurationLabel->setText(QCoreApplication::translate("MainWindow", "\345\237\272\345\207\206\345\233\276\350\200\227\346\227\266: 0ms", nullptr));
        cursorLabel->setText(QCoreApplication::translate("MainWindow", "X: --  Y: --  |  R: --  G: --  B: --", nullptr));
        toolResultButton->setText(QCoreApplication::translate("MainWindow", "\345\267\245\345\205\267\347\273\223\346\236\234", nullptr));
        resultResultButton->setText(QCoreApplication::translate("MainWindow", "\346\226\271\346\241\210\347\273\223\346\236\234", nullptr));
        expandResultButton->setText(QCoreApplication::translate("MainWindow", "\342\206\227", nullptr));
        collapseResultButton->setText(QCoreApplication::translate("MainWindow", "\342\214\203", nullptr));
        ngBadgeLabel->setText(QCoreApplication::translate("MainWindow", "NG", nullptr));
        clearSummaryButton->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272", nullptr));
        okRateValueLabel->setText(QCoreApplication::translate("MainWindow", "0%", nullptr));
        okRateTextLabel->setText(QCoreApplication::translate("MainWindow", "OK\347\216\207", nullptr));
        summaryTotalTitleLabel->setText(QCoreApplication::translate("MainWindow", "\346\243\200\346\265\213\346\200\273\346\225\260", nullptr));
        totalCountValueLabel->setText(QCoreApplication::translate("MainWindow", "0", nullptr));
        okCountTextLabel->setText(QCoreApplication::translate("MainWindow", "OK:", nullptr));
        okCountValueLabel->setText(QCoreApplication::translate("MainWindow", "0", nullptr));
        ngCountTextLabel->setText(QCoreApplication::translate("MainWindow", "NG:", nullptr));
        ngCountValueLabel->setText(QCoreApplication::translate("MainWindow", "0", nullptr));
        runtimeValueLabel->setText(QCoreApplication::translate("MainWindow", "0s", nullptr));
        runtimeTextLabel->setText(QCoreApplication::translate("MainWindow", "\345\267\262\350\277\220\350\241\214", nullptr));
        toolsTitleLabel->setText(QCoreApplication::translate("MainWindow", "\345\267\245\345\205\267\345\210\227\350\241\250", nullptr));
        QTableWidgetItem *___qtablewidgetitem = toolsTableWidget->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("MainWindow", "Name", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = toolsTableWidget->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("MainWindow", "Value", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = toolsTableWidget->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("MainWindow", "Status", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = toolsTableWidget->horizontalHeaderItem(3);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("MainWindow", "Setting", nullptr));
        QTableWidgetItem *___qtablewidgetitem4 = toolsTableWidget->horizontalHeaderItem(4);
        ___qtablewidgetitem4->setText(QCoreApplication::translate("MainWindow", "Visible", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
