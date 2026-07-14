/********************************************************************************
** Form generated from reading UI file 'ToolsDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TOOLSDIALOG_H
#define UI_TOOLSDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ToolsDialog
{
public:
    QVBoxLayout *verticalLayout_root;
    QFrame *setupTopBar;
    QHBoxLayout *horizontalLayout_topBar;
    QLabel *setupWindowTitleLabel;
    QSpacerItem *horizontalSpacer_topBar;
    QToolButton *headerMaximizeButton;
    QToolButton *headerCloseButton;
    QFrame *setupToolbar;
    QHBoxLayout *horizontalLayout_toolbar;
    QLabel *setupPageCodeLabel;
    QToolButton *setupExternalEditButton;
    QSpacerItem *horizontalSpacer_toolbar;
    QToolButton *setupSaveButton;
    QToolButton *setupSaveAsButton;
    QToolButton *setupExportButton;
    QFrame *setupBodyFrame;
    QHBoxLayout *horizontalLayout_body;
    QFrame *setupStepRail;
    QVBoxLayout *verticalLayout_steps;
    QToolButton *cameraStepButton;
    QLabel *stepDividerLabel1;
    QToolButton *referenceStepButton;
    QLabel *stepDividerLabel2;
    QToolButton *toolsStepButton;
    QLabel *stepDividerLabel3;
    QToolButton *outputStepButton;
    QSpacerItem *verticalSpacer_steps;
    QFrame *setupEditorPanel;
    QVBoxLayout *verticalLayout_editor;
    QHBoxLayout *horizontalLayout_editorHeader;
    QLabel *editorTitleLabel;
    QSpacerItem *horizontalSpacer_editorHeader;
    QToolButton *addToolButton;
    QToolButton *copyToolButton;
    QToolButton *deleteToolButton;
    QToolButton *settingsToolButton;
    QScrollArea *toolsScrollArea;
    QWidget *scrollAreaWidgetContents;
    QVBoxLayout *verticalLayout_toolsList;
    QHBoxLayout *horizontalLayout_nav;
    QPushButton *previousButton;
    QSpacerItem *horizontalSpacer_nav;
    QPushButton *nextButton;
    QFrame *setupViewerFrame;
    QVBoxLayout *verticalLayout_viewer;
    QFrame *viewerHeader;
    QHBoxLayout *horizontalLayout_viewerHeader;
    QLabel *viewerTitleLabel;
    QSpacerItem *horizontalSpacer_viewerHeader;
    QToolButton *viewerGridButton;
    QToolButton *viewerZoomSearchButton;
    QToolButton *viewerZoomOutButton;
    QLabel *viewerZoomLabel;
    QToolButton *viewerZoomInButton;
    QToolButton *viewerFullButton;
    QGraphicsView *previewGraphicsView;
    QFrame *viewerStatusBar;
    QHBoxLayout *horizontalLayout_viewerStatus;
    QLabel *viewerStatusLabel;
    QSpacerItem *horizontalSpacer_viewerStatus;
    QLabel *viewerCursorLabel;

    void setupUi(QDialog *ToolsDialog)
    {
        if (ToolsDialog->objectName().isEmpty())
            ToolsDialog->setObjectName(QString::fromUtf8("ToolsDialog"));
        ToolsDialog->resize(1760, 980);
        verticalLayout_root = new QVBoxLayout(ToolsDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        setupTopBar = new QFrame(ToolsDialog);
        setupTopBar->setObjectName(QString::fromUtf8("setupTopBar"));
        setupTopBar->setMinimumSize(QSize(0, 54));
        setupTopBar->setMaximumSize(QSize(16777215, 54));
        setupTopBar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_topBar = new QHBoxLayout(setupTopBar);
        horizontalLayout_topBar->setObjectName(QString::fromUtf8("horizontalLayout_topBar"));
        horizontalLayout_topBar->setContentsMargins(18, -1, 10, -1);
        setupWindowTitleLabel = new QLabel(setupTopBar);
        setupWindowTitleLabel->setObjectName(QString::fromUtf8("setupWindowTitleLabel"));

        horizontalLayout_topBar->addWidget(setupWindowTitleLabel);

        horizontalSpacer_topBar = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_topBar->addItem(horizontalSpacer_topBar);

        headerMaximizeButton = new QToolButton(setupTopBar);
        headerMaximizeButton->setObjectName(QString::fromUtf8("headerMaximizeButton"));
        headerMaximizeButton->setAutoRaise(true);

        horizontalLayout_topBar->addWidget(headerMaximizeButton);

        headerCloseButton = new QToolButton(setupTopBar);
        headerCloseButton->setObjectName(QString::fromUtf8("headerCloseButton"));
        headerCloseButton->setAutoRaise(true);

        horizontalLayout_topBar->addWidget(headerCloseButton);


        verticalLayout_root->addWidget(setupTopBar);

        setupToolbar = new QFrame(ToolsDialog);
        setupToolbar->setObjectName(QString::fromUtf8("setupToolbar"));
        setupToolbar->setMinimumSize(QSize(0, 72));
        setupToolbar->setMaximumSize(QSize(16777215, 72));
        setupToolbar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_toolbar = new QHBoxLayout(setupToolbar);
        horizontalLayout_toolbar->setObjectName(QString::fromUtf8("horizontalLayout_toolbar"));
        horizontalLayout_toolbar->setContentsMargins(24, 8, 24, 8);
        setupPageCodeLabel = new QLabel(setupToolbar);
        setupPageCodeLabel->setObjectName(QString::fromUtf8("setupPageCodeLabel"));

        horizontalLayout_toolbar->addWidget(setupPageCodeLabel);

        setupExternalEditButton = new QToolButton(setupToolbar);
        setupExternalEditButton->setObjectName(QString::fromUtf8("setupExternalEditButton"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/external-link.svg"), QSize(), QIcon::Normal, QIcon::Off);
        setupExternalEditButton->setIcon(icon);
        setupExternalEditButton->setIconSize(QSize(24, 24));
        setupExternalEditButton->setAutoRaise(true);

        horizontalLayout_toolbar->addWidget(setupExternalEditButton);

        horizontalSpacer_toolbar = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_toolbar->addItem(horizontalSpacer_toolbar);

        setupSaveButton = new QToolButton(setupToolbar);
        setupSaveButton->setObjectName(QString::fromUtf8("setupSaveButton"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/save.svg"), QSize(), QIcon::Normal, QIcon::Off);
        setupSaveButton->setIcon(icon1);
        setupSaveButton->setIconSize(QSize(28, 28));
        setupSaveButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        horizontalLayout_toolbar->addWidget(setupSaveButton);

        setupSaveAsButton = new QToolButton(setupToolbar);
        setupSaveAsButton->setObjectName(QString::fromUtf8("setupSaveAsButton"));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/save-as.svg"), QSize(), QIcon::Normal, QIcon::Off);
        setupSaveAsButton->setIcon(icon2);
        setupSaveAsButton->setIconSize(QSize(28, 28));
        setupSaveAsButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        horizontalLayout_toolbar->addWidget(setupSaveAsButton);

        setupExportButton = new QToolButton(setupToolbar);
        setupExportButton->setObjectName(QString::fromUtf8("setupExportButton"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icons/export-output.svg"), QSize(), QIcon::Normal, QIcon::Off);
        setupExportButton->setIcon(icon3);
        setupExportButton->setIconSize(QSize(28, 28));
        setupExportButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        horizontalLayout_toolbar->addWidget(setupExportButton);


        verticalLayout_root->addWidget(setupToolbar);

        setupBodyFrame = new QFrame(ToolsDialog);
        setupBodyFrame->setObjectName(QString::fromUtf8("setupBodyFrame"));
        setupBodyFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_body = new QHBoxLayout(setupBodyFrame);
        horizontalLayout_body->setSpacing(0);
        horizontalLayout_body->setObjectName(QString::fromUtf8("horizontalLayout_body"));
        horizontalLayout_body->setContentsMargins(0, 0, 0, 0);
        setupStepRail = new QFrame(setupBodyFrame);
        setupStepRail->setObjectName(QString::fromUtf8("setupStepRail"));
        setupStepRail->setMinimumSize(QSize(106, 0));
        setupStepRail->setMaximumSize(QSize(106, 16777215));
        setupStepRail->setFrameShape(QFrame::NoFrame);
        verticalLayout_steps = new QVBoxLayout(setupStepRail);
        verticalLayout_steps->setSpacing(12);
        verticalLayout_steps->setObjectName(QString::fromUtf8("verticalLayout_steps"));
        verticalLayout_steps->setContentsMargins(8, 18, 8, 18);
        cameraStepButton = new QToolButton(setupStepRail);
        cameraStepButton->setObjectName(QString::fromUtf8("cameraStepButton"));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/sliders.svg"), QSize(), QIcon::Normal, QIcon::Off);
        cameraStepButton->setIcon(icon4);
        cameraStepButton->setIconSize(QSize(34, 34));
        cameraStepButton->setCheckable(true);
        cameraStepButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        verticalLayout_steps->addWidget(cameraStepButton);

        stepDividerLabel1 = new QLabel(setupStepRail);
        stepDividerLabel1->setObjectName(QString::fromUtf8("stepDividerLabel1"));
        stepDividerLabel1->setAlignment(Qt::AlignCenter);
        stepDividerLabel1->setProperty("role", QVariant(QString::fromUtf8("stepDivider")));

        verticalLayout_steps->addWidget(stepDividerLabel1);

        referenceStepButton = new QToolButton(setupStepRail);
        referenceStepButton->setObjectName(QString::fromUtf8("referenceStepButton"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/reference.svg"), QSize(), QIcon::Normal, QIcon::Off);
        referenceStepButton->setIcon(icon5);
        referenceStepButton->setIconSize(QSize(34, 34));
        referenceStepButton->setCheckable(true);
        referenceStepButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        verticalLayout_steps->addWidget(referenceStepButton);

        stepDividerLabel2 = new QLabel(setupStepRail);
        stepDividerLabel2->setObjectName(QString::fromUtf8("stepDividerLabel2"));
        stepDividerLabel2->setAlignment(Qt::AlignCenter);
        stepDividerLabel2->setProperty("role", QVariant(QString::fromUtf8("stepDivider")));

        verticalLayout_steps->addWidget(stepDividerLabel2);

        toolsStepButton = new QToolButton(setupStepRail);
        toolsStepButton->setObjectName(QString::fromUtf8("toolsStepButton"));
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/icons/tool.svg"), QSize(), QIcon::Normal, QIcon::Off);
        toolsStepButton->setIcon(icon6);
        toolsStepButton->setIconSize(QSize(34, 34));
        toolsStepButton->setCheckable(true);
        toolsStepButton->setChecked(true);
        toolsStepButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        verticalLayout_steps->addWidget(toolsStepButton);

        stepDividerLabel3 = new QLabel(setupStepRail);
        stepDividerLabel3->setObjectName(QString::fromUtf8("stepDividerLabel3"));
        stepDividerLabel3->setAlignment(Qt::AlignCenter);
        stepDividerLabel3->setProperty("role", QVariant(QString::fromUtf8("stepDivider")));

        verticalLayout_steps->addWidget(stepDividerLabel3);

        outputStepButton = new QToolButton(setupStepRail);
        outputStepButton->setObjectName(QString::fromUtf8("outputStepButton"));
        QIcon icon7;
        icon7.addFile(QString::fromUtf8(":/icons/output.svg"), QSize(), QIcon::Normal, QIcon::Off);
        outputStepButton->setIcon(icon7);
        outputStepButton->setIconSize(QSize(34, 34));
        outputStepButton->setCheckable(true);
        outputStepButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        verticalLayout_steps->addWidget(outputStepButton);

        verticalSpacer_steps = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_steps->addItem(verticalSpacer_steps);


        horizontalLayout_body->addWidget(setupStepRail);

        setupEditorPanel = new QFrame(setupBodyFrame);
        setupEditorPanel->setObjectName(QString::fromUtf8("setupEditorPanel"));
        setupEditorPanel->setMinimumSize(QSize(610, 0));
        setupEditorPanel->setMaximumSize(QSize(610, 16777215));
        setupEditorPanel->setFrameShape(QFrame::NoFrame);
        verticalLayout_editor = new QVBoxLayout(setupEditorPanel);
        verticalLayout_editor->setSpacing(16);
        verticalLayout_editor->setObjectName(QString::fromUtf8("verticalLayout_editor"));
        verticalLayout_editor->setContentsMargins(24, 22, 24, 18);
        horizontalLayout_editorHeader = new QHBoxLayout();
        horizontalLayout_editorHeader->setObjectName(QString::fromUtf8("horizontalLayout_editorHeader"));
        editorTitleLabel = new QLabel(setupEditorPanel);
        editorTitleLabel->setObjectName(QString::fromUtf8("editorTitleLabel"));

        horizontalLayout_editorHeader->addWidget(editorTitleLabel);

        horizontalSpacer_editorHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_editorHeader->addItem(horizontalSpacer_editorHeader);

        addToolButton = new QToolButton(setupEditorPanel);
        addToolButton->setObjectName(QString::fromUtf8("addToolButton"));
        addToolButton->setStyleSheet(QString::fromUtf8("QToolButton { color:#ffffff; font-size:28px; font-weight:700; }"));
        addToolButton->setProperty("actionRole", QVariant(QString::fromUtf8("toolbarPrimary")));

        horizontalLayout_editorHeader->addWidget(addToolButton);

        copyToolButton = new QToolButton(setupEditorPanel);
        copyToolButton->setObjectName(QString::fromUtf8("copyToolButton"));
        QIcon icon8;
        icon8.addFile(QString::fromUtf8(":/icons/copy.svg"), QSize(), QIcon::Normal, QIcon::Off);
        copyToolButton->setIcon(icon8);
        copyToolButton->setIconSize(QSize(22, 22));
        copyToolButton->setProperty("actionRole", QVariant(QString::fromUtf8("toolbarIcon")));

        horizontalLayout_editorHeader->addWidget(copyToolButton);

        deleteToolButton = new QToolButton(setupEditorPanel);
        deleteToolButton->setObjectName(QString::fromUtf8("deleteToolButton"));
        QIcon icon9;
        icon9.addFile(QString::fromUtf8(":/icons/delete-outline.svg"), QSize(), QIcon::Normal, QIcon::Off);
        deleteToolButton->setIcon(icon9);
        deleteToolButton->setIconSize(QSize(22, 22));
        deleteToolButton->setProperty("actionRole", QVariant(QString::fromUtf8("toolbarIcon")));

        horizontalLayout_editorHeader->addWidget(deleteToolButton);

        settingsToolButton = new QToolButton(setupEditorPanel);
        settingsToolButton->setObjectName(QString::fromUtf8("settingsToolButton"));
        QIcon icon10;
        icon10.addFile(QString::fromUtf8(":/icons/tool-step.svg"), QSize(), QIcon::Normal, QIcon::Off);
        settingsToolButton->setIcon(icon10);
        settingsToolButton->setIconSize(QSize(22, 22));
        settingsToolButton->setProperty("actionRole", QVariant(QString::fromUtf8("toolbarIcon")));

        horizontalLayout_editorHeader->addWidget(settingsToolButton);


        verticalLayout_editor->addLayout(horizontalLayout_editorHeader);

        toolsScrollArea = new QScrollArea(setupEditorPanel);
        toolsScrollArea->setObjectName(QString::fromUtf8("toolsScrollArea"));
        toolsScrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 560, 707));
        verticalLayout_toolsList = new QVBoxLayout(scrollAreaWidgetContents);
        verticalLayout_toolsList->setSpacing(12);
        verticalLayout_toolsList->setObjectName(QString::fromUtf8("verticalLayout_toolsList"));
        verticalLayout_toolsList->setContentsMargins(0, 0, 0, 0);
        toolsScrollArea->setWidget(scrollAreaWidgetContents);

        verticalLayout_editor->addWidget(toolsScrollArea);

        horizontalLayout_nav = new QHBoxLayout();
        horizontalLayout_nav->setObjectName(QString::fromUtf8("horizontalLayout_nav"));
        previousButton = new QPushButton(setupEditorPanel);
        previousButton->setObjectName(QString::fromUtf8("previousButton"));

        horizontalLayout_nav->addWidget(previousButton);

        horizontalSpacer_nav = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_nav->addItem(horizontalSpacer_nav);

        nextButton = new QPushButton(setupEditorPanel);
        nextButton->setObjectName(QString::fromUtf8("nextButton"));

        horizontalLayout_nav->addWidget(nextButton);


        verticalLayout_editor->addLayout(horizontalLayout_nav);


        horizontalLayout_body->addWidget(setupEditorPanel);

        setupViewerFrame = new QFrame(setupBodyFrame);
        setupViewerFrame->setObjectName(QString::fromUtf8("setupViewerFrame"));
        setupViewerFrame->setFrameShape(QFrame::NoFrame);
        verticalLayout_viewer = new QVBoxLayout(setupViewerFrame);
        verticalLayout_viewer->setSpacing(0);
        verticalLayout_viewer->setObjectName(QString::fromUtf8("verticalLayout_viewer"));
        verticalLayout_viewer->setContentsMargins(0, 0, 0, 0);
        viewerHeader = new QFrame(setupViewerFrame);
        viewerHeader->setObjectName(QString::fromUtf8("viewerHeader"));
        viewerHeader->setMinimumSize(QSize(0, 54));
        viewerHeader->setMaximumSize(QSize(16777215, 54));
        viewerHeader->setFrameShape(QFrame::NoFrame);
        horizontalLayout_viewerHeader = new QHBoxLayout(viewerHeader);
        horizontalLayout_viewerHeader->setObjectName(QString::fromUtf8("horizontalLayout_viewerHeader"));
        horizontalLayout_viewerHeader->setContentsMargins(18, -1, 14, -1);
        viewerTitleLabel = new QLabel(viewerHeader);
        viewerTitleLabel->setObjectName(QString::fromUtf8("viewerTitleLabel"));

        horizontalLayout_viewerHeader->addWidget(viewerTitleLabel);

        horizontalSpacer_viewerHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_viewerHeader->addItem(horizontalSpacer_viewerHeader);

        viewerGridButton = new QToolButton(viewerHeader);
        viewerGridButton->setObjectName(QString::fromUtf8("viewerGridButton"));
        QIcon icon11;
        icon11.addFile(QString::fromUtf8(":/icons/grid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerGridButton->setIcon(icon11);
        viewerGridButton->setIconSize(QSize(24, 24));
        viewerGridButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerGridButton);

        viewerZoomSearchButton = new QToolButton(viewerHeader);
        viewerZoomSearchButton->setObjectName(QString::fromUtf8("viewerZoomSearchButton"));
        QIcon icon12;
        icon12.addFile(QString::fromUtf8(":/icons/zoom.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerZoomSearchButton->setIcon(icon12);
        viewerZoomSearchButton->setIconSize(QSize(24, 24));
        viewerZoomSearchButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerZoomSearchButton);

        viewerZoomOutButton = new QToolButton(viewerHeader);
        viewerZoomOutButton->setObjectName(QString::fromUtf8("viewerZoomOutButton"));
        viewerZoomOutButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerZoomOutButton);

        viewerZoomLabel = new QLabel(viewerHeader);
        viewerZoomLabel->setObjectName(QString::fromUtf8("viewerZoomLabel"));

        horizontalLayout_viewerHeader->addWidget(viewerZoomLabel);

        viewerZoomInButton = new QToolButton(viewerHeader);
        viewerZoomInButton->setObjectName(QString::fromUtf8("viewerZoomInButton"));
        QIcon icon13;
        icon13.addFile(QString::fromUtf8(":/icons/fit.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerZoomInButton->setIcon(icon13);
        viewerZoomInButton->setIconSize(QSize(24, 24));
        viewerZoomInButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerZoomInButton);

        viewerFullButton = new QToolButton(viewerHeader);
        viewerFullButton->setObjectName(QString::fromUtf8("viewerFullButton"));
        QIcon icon14;
        icon14.addFile(QString::fromUtf8(":/icons/fullscreen.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerFullButton->setIcon(icon14);
        viewerFullButton->setIconSize(QSize(24, 24));
        viewerFullButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerFullButton);


        verticalLayout_viewer->addWidget(viewerHeader);

        previewGraphicsView = new QGraphicsView(setupViewerFrame);
        previewGraphicsView->setObjectName(QString::fromUtf8("previewGraphicsView"));
        previewGraphicsView->setFrameShape(QFrame::NoFrame);
        previewGraphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        previewGraphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        previewGraphicsView->setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);

        verticalLayout_viewer->addWidget(previewGraphicsView);

        viewerStatusBar = new QFrame(setupViewerFrame);
        viewerStatusBar->setObjectName(QString::fromUtf8("viewerStatusBar"));
        viewerStatusBar->setMinimumSize(QSize(0, 42));
        viewerStatusBar->setMaximumSize(QSize(16777215, 42));
        viewerStatusBar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_viewerStatus = new QHBoxLayout(viewerStatusBar);
        horizontalLayout_viewerStatus->setObjectName(QString::fromUtf8("horizontalLayout_viewerStatus"));
        horizontalLayout_viewerStatus->setContentsMargins(18, -1, 16, -1);
        viewerStatusLabel = new QLabel(viewerStatusBar);
        viewerStatusLabel->setObjectName(QString::fromUtf8("viewerStatusLabel"));

        horizontalLayout_viewerStatus->addWidget(viewerStatusLabel);

        horizontalSpacer_viewerStatus = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_viewerStatus->addItem(horizontalSpacer_viewerStatus);

        viewerCursorLabel = new QLabel(viewerStatusBar);
        viewerCursorLabel->setObjectName(QString::fromUtf8("viewerCursorLabel"));

        horizontalLayout_viewerStatus->addWidget(viewerCursorLabel);


        verticalLayout_viewer->addWidget(viewerStatusBar);


        horizontalLayout_body->addWidget(setupViewerFrame);


        verticalLayout_root->addWidget(setupBodyFrame);


        retranslateUi(ToolsDialog);

        QMetaObject::connectSlotsByName(ToolsDialog);
    } // setupUi

    void retranslateUi(QDialog *ToolsDialog)
    {
        ToolsDialog->setWindowTitle(QCoreApplication::translate("ToolsDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \345\267\245\345\205\267", nullptr));
        setupWindowTitleLabel->setText(QCoreApplication::translate("ToolsDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("ToolsDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("ToolsDialog", "\303\227", nullptr));
        setupPageCodeLabel->setText(QCoreApplication::translate("ToolsDialog", "324", nullptr));
        setupSaveButton->setText(QCoreApplication::translate("ToolsDialog", "\344\277\235\345\255\230", nullptr));
        setupSaveAsButton->setText(QCoreApplication::translate("ToolsDialog", "\345\217\246\345\255\230\344\270\272", nullptr));
        setupExportButton->setText(QCoreApplication::translate("ToolsDialog", "IO\350\276\223\345\207\272", nullptr));
        cameraStepButton->setText(QCoreApplication::translate("ToolsDialog", "\347\233\270\346\234\272\345\217\202\346\225\260", nullptr));
        stepDividerLabel1->setText(QCoreApplication::translate("ToolsDialog", "\342\226\276", nullptr));
        referenceStepButton->setText(QCoreApplication::translate("ToolsDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        stepDividerLabel2->setText(QCoreApplication::translate("ToolsDialog", "\342\226\276", nullptr));
        toolsStepButton->setText(QCoreApplication::translate("ToolsDialog", "\345\267\245\345\205\267", nullptr));
        stepDividerLabel3->setText(QCoreApplication::translate("ToolsDialog", "\342\226\276", nullptr));
        outputStepButton->setText(QCoreApplication::translate("ToolsDialog", "\350\276\223\345\207\272", nullptr));
        editorTitleLabel->setText(QCoreApplication::translate("ToolsDialog", "3/ \345\267\245\345\205\267", nullptr));
        addToolButton->setText(QCoreApplication::translate("ToolsDialog", "+", nullptr));
        previousButton->setText(QCoreApplication::translate("ToolsDialog", "\343\200\210 \344\270\212\344\270\200\346\255\245", nullptr));
        nextButton->setText(QCoreApplication::translate("ToolsDialog", "\344\270\213\344\270\200\346\255\245 \343\200\211", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("ToolsDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerZoomOutButton->setText(QCoreApplication::translate("ToolsDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("ToolsDialog", "126%", nullptr));
        viewerStatusLabel->setText(QCoreApplication::translate("ToolsDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: --ms   \345\267\245\345\205\267\350\200\227\346\227\266: --ms", nullptr));
        viewerCursorLabel->setText(QCoreApplication::translate("ToolsDialog", "X: --  Y: --   |   R: --  G: --  B: --", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ToolsDialog: public Ui_ToolsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TOOLSDIALOG_H
