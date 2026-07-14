/********************************************************************************
** Form generated from reading UI file 'OutputDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OUTPUTDIALOG_H
#define UI_OUTPUTDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_OutputDialog
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
    QLabel *editorTitleLabel;
    QFrame *schemeResultCard;
    QVBoxLayout *verticalLayout_schemeResult;
    QHBoxLayout *horizontalLayout_schemeResultHeader;
    QLabel *schemeResultTitle;
    QSpacerItem *horizontalSpacer_schemeHeader;
    QToolButton *schemeCollapseButton;
    QLabel *schemeHintLabel;
    QFrame *segmentFrame;
    QHBoxLayout *horizontalLayout_segment;
    QPushButton *allModulesOkButton;
    QPushButton *anyModuleOkButton;
    QPushButton *customResultButton;
    QFrame *timedOutputCard;
    QVBoxLayout *verticalLayout_timedOutput;
    QHBoxLayout *horizontalLayout_timedHeader;
    QLabel *timedOutputTitle;
    QSpacerItem *horizontalSpacer_timedHeader;
    QToolButton *timedCollapseButton;
    QHBoxLayout *horizontalLayout_timedSwitch;
    QLabel *timedSwitchLabel;
    QSpacerItem *horizontalSpacer_timedSwitch;
    QCheckBox *checkBox;
    QHBoxLayout *horizontalLayout_timedSpin;
    QLabel *timedMsLabel;
    QSpacerItem *horizontalSpacer_timedSpin;
    QSpinBox *timedSpinBox;
    QFrame *outputParamsCard;
    QVBoxLayout *verticalLayout_outputParams;
    QHBoxLayout *horizontalLayout_outputHeader;
    QLabel *outputParamsTitle;
    QSpacerItem *horizontalSpacer_outputHeader;
    QToolButton *outputCollapseButton;
    QHBoxLayout *horizontalLayout_resultOutput;
    QLabel *resultOutputLabel;
    QSpacerItem *horizontalSpacer_resultOutput;
    QCheckBox *checkBox_2;
    QHBoxLayout *horizontalLayout_ioEdit;
    QLabel *ioOutputLabel;
    QSpacerItem *horizontalSpacer_ioEdit;
    QPushButton *editIoButton;
    QHBoxLayout *horizontalLayout_commJump;
    QLabel *commSetupLabel;
    QSpacerItem *horizontalSpacer_commJump;
    QPushButton *jumpCommButton;
    QSpacerItem *verticalSpacer_editor;
    QHBoxLayout *horizontalLayout_nav;
    QPushButton *previousButton;
    QSpacerItem *horizontalSpacer_nav;
    QPushButton *finishButton;
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

    void setupUi(QDialog *OutputDialog)
    {
        if (OutputDialog->objectName().isEmpty())
            OutputDialog->setObjectName(QString::fromUtf8("OutputDialog"));
        OutputDialog->resize(1760, 980);
        OutputDialog->setStyleSheet(QString::fromUtf8("QDialog#OutputDialog {\n"
"    background: #eef2f6;\n"
"    color: #314968;\n"
"    font-family: \"Noto Sans CJK SC\", \"Microsoft YaHei\", sans-serif;\n"
"    font-size: 14px;\n"
"}\n"
"\n"
"QFrame#setupTopBar {\n"
"    background: #4b4f5b;\n"
"}\n"
"\n"
"QLabel#setupWindowTitleLabel {\n"
"    background: transparent;\n"
"    color: #ffffff;\n"
"    font-size: 18px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QToolButton#headerMaximizeButton,\n"
"QToolButton#headerCloseButton,\n"
"QToolButton#closeButton {\n"
"    min-width: 34px;\n"
"    min-height: 34px;\n"
"    border: none;\n"
"    border-radius: 4px;\n"
"    background: transparent;\n"
"    color: #f5f6f8;\n"
"    font-size: 20px;\n"
"}\n"
"\n"
"QToolButton#headerMaximizeButton:hover,\n"
"QToolButton#headerCloseButton:hover,\n"
"QToolButton#closeButton:hover {\n"
"    background: rgba(255, 255, 255, 0.10);\n"
"}\n"
"\n"
"QToolButton#headerCloseButton:hover,\n"
"QToolButton#closeButton:hover {\n"
"    background: #ff4b4b;\n"
"}\n"
"\n"
"QFrame#setupToolbar {"
                        "\n"
"    background: #ffffff;\n"
"    border-bottom: 1px solid #dfe3ea;\n"
"}\n"
"\n"
"QLabel#setupPageCodeLabel,\n"
"QLabel[pageCode=\"true\"] {\n"
"    background: transparent;\n"
"    color: #4c5970;\n"
"    font-size: 20px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QFrame#toolbarDivider {\n"
"    background: #dfe3ea;\n"
"}\n"
"\n"
"QFrame#setupBodyFrame,\n"
"QFrame#toolLibraryBody {\n"
"    background: #eef2f6;\n"
"}\n"
"\n"
"QFrame#setupStepRail {\n"
"    background: #d6dae1;\n"
"    border-right: 1px solid #c6ccd6;\n"
"}\n"
"\n"
"QToolButton#cameraStepButton,\n"
"QToolButton#referenceStepButton,\n"
"QToolButton#toolsStepButton,\n"
"QToolButton#outputStepButton {\n"
"    min-width: 92px;\n"
"    min-height: 86px;\n"
"    border: none;\n"
"    border-radius: 0;\n"
"    background: transparent;\n"
"    color: #6f7787;\n"
"    font-size: 15px;\n"
"    font-weight: 700;\n"
"    padding: 10px 8px;\n"
"}\n"
"\n"
"QToolButton#cameraStepButton:hover,\n"
"QToolButton#referenceStepButton:hover,\n"
"QToolButton#tools"
                        "StepButton:hover,\n"
"QToolButton#outputStepButton:hover {\n"
"    background: rgba(255, 255, 255, 0.45);\n"
"    color: #50586a;\n"
"}\n"
"\n"
"QToolButton#cameraStepButton:checked,\n"
"QToolButton#referenceStepButton:checked,\n"
"QToolButton#toolsStepButton:checked,\n"
"QToolButton#outputStepButton:checked {\n"
"    background: #f8fafc;\n"
"    color: #ff830f;\n"
"    border-left: 3px solid #ff830f;\n"
"}\n"
"\n"
"QLabel[stepArrow=\"true\"],\n"
"QLabel[role=\"stepDivider\"] {\n"
"    background: transparent;\n"
"    color: #bbc2ce;\n"
"    font-size: 18px;\n"
"    qproperty-alignment: AlignCenter;\n"
"}\n"
"\n"
"QFrame#setupEditorPanel,\n"
"QFrame[editorPanel=\"true\"] {\n"
"    background: #eef2f6;\n"
"}\n"
"\n"
"QStackedWidget#cameraParamsStack,\n"
"QWidget#basicParamsPage,\n"
"QWidget#allParamsPage,\n"
"QWidget#basicParamsScrollContent,\n"
"QWidget#allParamsScrollContent,\n"
"QWidget#referenceParamsScrollContent {\n"
"    background: transparent;\n"
"}\n"
"\n"
"QLabel#editorTitleLabel,\n"
"QLabel[editorTi"
                        "tle=\"true\"] {\n"
"    background: transparent;\n"
"    color: #324969;\n"
"    font-size: 24px;\n"
"    font-weight: 800;\n"
"}\n"
"\n"
"QFrame[card=\"true\"] QFrame {\n"
"    background: transparent;\n"
"    border: none;\n"
"}\n"
"\n"
"QFrame#segmentFrame,\n"
"QFrame[segmented=\"true\"] {\n"
"    background: #dde2ea;\n"
"    border: 1px solid #cfd5de;\n"
"    border-radius: 22px;\n"
"}\n"
"\n"
"QFrame#segmentFrame QPushButton,\n"
"QFrame[segmented=\"true\"] QPushButton {\n"
"    min-height: 38px;\n"
"    border: none;\n"
"    border-radius: 18px;\n"
"    background: transparent;\n"
"    color: #6f7887;\n"
"    padding: 0 22px;\n"
"    font-size: 17px;\n"
"    font-weight: 600;\n"
"}\n"
"\n"
"QFrame#segmentFrame QPushButton:checked,\n"
"QFrame[segmented=\"true\"] QPushButton:checked {\n"
"    background: #ffffff;\n"
"    color: #ff830f;\n"
"}\n"
"\n"
"QFrame#configCard,\n"
"QFrame[panelRole=\"configCard\"],\n"
"QFrame[card=\"true\"] {\n"
"    background: #ffffff;\n"
"    border: 1px solid #e1e5eb;\n"
"    b"
                        "order-radius: 8px;\n"
"}\n"
"\n"
"QLabel#cardTitleLabel,\n"
"QLabel[role=\"cardTitle\"],\n"
"QLabel[cardTitle=\"true\"] {\n"
"    background: transparent;\n"
"    color: #314968;\n"
"    font-size: 18px;\n"
"    font-weight: 800;\n"
"}\n"
"\n"
"QLabel#cardHintLabel,\n"
"QLabel[role=\"cardHint\"],\n"
"QLabel[hint=\"true\"] {\n"
"    background: transparent;\n"
"    color: #8992a1;\n"
"    font-size: 14px;\n"
"}\n"
"\n"
"QLabel#rowFieldLabel,\n"
"QLabel[role=\"rowField\"],\n"
"QLabel[formLabel=\"true\"] {\n"
"    background: transparent;\n"
"    color: #314968;\n"
"    font-size: 16px;\n"
"    font-weight: 600;\n"
"}\n"
"\n"
"QPushButton[softButton=\"true\"],\n"
"QPushButton#historyImageButton,\n"
"QPushButton#pcImportButton,\n"
"QPushButton#grayActionButton,\n"
"QPushButton[actionRole=\"gray\"],\n"
"QPushButton[actionRole=\"secondary\"] {\n"
"    min-height: 42px;\n"
"    border-radius: 4px;\n"
"    border: 1px solid #c9d1db;\n"
"    background: #dfe6ef;\n"
"    color: #536274;\n"
"    padding: 0 18px;\n"
"    "
                        "font-size: 16px;\n"
"    font-weight: 600;\n"
"}\n"
"\n"
"QPushButton[softButton=\"true\"]:hover,\n"
"QPushButton#historyImageButton:hover,\n"
"QPushButton#pcImportButton:hover,\n"
"QPushButton[actionRole=\"gray\"]:hover,\n"
"QPushButton[actionRole=\"secondary\"]:hover {\n"
"    background: #edf2f7;\n"
"}\n"
"\n"
"QPushButton[orangeButton=\"true\"],\n"
"QPushButton#confirmButton,\n"
"QPushButton#currentImageButton,\n"
"QPushButton#highlightActionButton,\n"
"QPushButton[actionRole=\"highlight\"] {\n"
"    min-height: 46px;\n"
"    border: none;\n"
"    border-radius: 4px;\n"
"    background: #ff830f;\n"
"    color: #ffffff;\n"
"    padding: 0 18px;\n"
"    font-size: 18px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QPushButton[orangeButton=\"true\"]:hover,\n"
"QPushButton#confirmButton:hover,\n"
"QPushButton#currentImageButton:hover,\n"
"QPushButton#highlightActionButton:hover,\n"
"QPushButton[actionRole=\"highlight\"]:hover {\n"
"    background: #ff972f;\n"
"}\n"
"\n"
"QPushButton#previousButton,\n"
"QPushButton"
                        "#nextButton,\n"
"QPushButton#finishButton {\n"
"    min-width: 110px;\n"
"    min-height: 48px;\n"
"    border-radius: 4px;\n"
"    font-size: 16px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QPushButton#previousButton {\n"
"    background: #ffffff;\n"
"    color: #4f5a6d;\n"
"    border: 1px solid #d1d8e1;\n"
"}\n"
"\n"
"QPushButton#previousButton:disabled {\n"
"    color: #bfc6d0;\n"
"    background: #ffffff;\n"
"}\n"
"\n"
"QPushButton#nextButton,\n"
"QPushButton#finishButton {\n"
"    background: #ff830f;\n"
"    color: #ffffff;\n"
"    border: none;\n"
"}\n"
"\n"
"QPushButton#nextButton:hover,\n"
"QPushButton#finishButton:hover {\n"
"    background: #ff972f;\n"
"}\n"
"\n"
"QComboBox,\n"
"QLineEdit {\n"
"    min-height: 38px;\n"
"    border: 1px solid #d1d8e1;\n"
"    border-radius: 4px;\n"
"    background: #ffffff;\n"
"    color: #536274;\n"
"    padding: 0 12px;\n"
"    font-size: 15px;\n"
"}\n"
"\n"
"QComboBox::drop-down {\n"
"    border: none;\n"
"    width: 28px;\n"
"}\n"
"\n"
"QCheckBox {\n"
"    spacin"
                        "g: 0;\n"
"    color: #314968;\n"
"    background: transparent;\n"
"}\n"
"\n"
"QCheckBox::indicator {\n"
"    width: 48px;\n"
"    height: 24px;\n"
"    border-radius: 12px;\n"
"    border: 1px solid #c8ced8;\n"
"    background: #d5dae2;\n"
"}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"    background: #ff830f;\n"
"    border-color: #ff830f;\n"
"}\n"
"\n"
"QScrollArea {\n"
"    border: none;\n"
"    background: transparent;\n"
"}\n"
"\n"
"QScrollArea > QWidget > QWidget {\n"
"    background: transparent;\n"
"}\n"
"\n"
"QScrollBar:vertical {\n"
"    width: 10px;\n"
"    background: transparent;\n"
"    margin: 4px 0 4px 0;\n"
"}\n"
"\n"
"QScrollBar::handle:vertical {\n"
"    background: #6c7280;\n"
"    min-height: 28px;\n"
"    border-radius: 5px;\n"
"}\n"
"\n"
"QScrollBar:horizontal {\n"
"    height: 0px;\n"
"    background: transparent;\n"
"    margin: 0;\n"
"}\n"
"\n"
"QScrollBar::handle:horizontal {\n"
"    background: transparent;\n"
"}\n"
"\n"
"QFrame[table=\"true\"] QLabel[tableCell=\"true\"] {\n"
"    m"
                        "in-height: 34px;\n"
"    padding-left: 10px;\n"
"    background: #ffffff;\n"
"    color: #314968;\n"
"    border: 1px solid #d1d8e1;\n"
"}\n"
"\n"
"QLabel[devicePreview=\"true\"] {\n"
"    color: #49d45b;\n"
"    background: #24262b;\n"
"    border: 1px solid #cfd5de;\n"
"    font-size: 36px;\n"
"    qproperty-alignment: AlignCenter;\n"
"}\n"
"\n"
"QLabel[centerText=\"true\"] {\n"
"    qproperty-alignment: AlignCenter;\n"
"}\n"
"\n"
"QLabel[greenText=\"true\"] {\n"
"    background: transparent;\n"
"    color: #20c933;\n"
"    font-size: 18px;\n"
"    font-weight: 600;\n"
"}\n"
"\n"
"QLabel[redText=\"true\"] {\n"
"    background: transparent;\n"
"    color: #ff2020;\n"
"    font-size: 18px;\n"
"    font-weight: 600;\n"
"}\n"
"\n"
"QFrame[examplePanel=\"true\"] {\n"
"    background: #eef2f6;\n"
"    border: 0;\n"
"}\n"
"\n"
"QLabel[correctionPreview=\"true\"] {\n"
"    background: transparent;\n"
"    color: #28d83d;\n"
"    font-size: 44px;\n"
"    qproperty-alignment: AlignCenter;\n"
"}\n"
"\n"
"QFrame#setupVi"
                        "ewerFrame,\n"
"QFrame[viewerPanel=\"true\"] {\n"
"    background: #101215;\n"
"}\n"
"\n"
"QFrame#viewerHeader,\n"
"QFrame#viewerStatusBar {\n"
"    background: #24262b;\n"
"}\n"
"\n"
"QLabel#viewerTitleLabel,\n"
"QLabel#viewerZoomLabel,\n"
"QLabel#viewerStatusLabel,\n"
"QLabel#viewerCursorLabel,\n"
"QLabel[viewerTitle=\"true\"],\n"
"QLabel[viewerZoom=\"true\"],\n"
"QLabel[viewerStatus=\"true\"] {\n"
"    background: transparent;\n"
"    color: #ffffff;\n"
"}\n"
"\n"
"QLabel#viewerTitleLabel,\n"
"QLabel[viewerTitle=\"true\"] {\n"
"    font-size: 16px;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QToolButton#viewerGridButton,\n"
"QToolButton#viewerZoomSearchButton,\n"
"QToolButton#viewerZoomOutButton,\n"
"QToolButton#viewerZoomInButton,\n"
"QToolButton#viewerFullButton,\n"
"QToolButton#viewerIconButton,\n"
"QToolButton#viewerTextButton {\n"
"    min-width: 28px;\n"
"    min-height: 28px;\n"
"    border: none;\n"
"    border-radius: 4px;\n"
"    background: transparent;\n"
"    color: #f0f2f5;\n"
"}\n"
"\n"
"QToolBut"
                        "ton#viewerGridButton:hover,\n"
"QToolButton#viewerZoomSearchButton:hover,\n"
"QToolButton#viewerZoomOutButton:hover,\n"
"QToolButton#viewerZoomInButton:hover,\n"
"QToolButton#viewerFullButton:hover,\n"
"QToolButton#viewerIconButton:hover,\n"
"QToolButton#viewerTextButton:hover {\n"
"    background: rgba(255, 255, 255, 0.10);\n"
"}\n"
"\n"
"QGraphicsView#previewGraphicsView,\n"
"QGraphicsView[viewerCanvas=\"true\"],\n"
"QVideoWidget[viewerCanvas=\"true\"],\n"
"QWidget[viewerCanvas=\"true\"] {\n"
"    background: #000000;\n"
"    border: none;\n"
"}\n"
""));
        verticalLayout_root = new QVBoxLayout(OutputDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        setupTopBar = new QFrame(OutputDialog);
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

        setupToolbar = new QFrame(OutputDialog);
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

        setupBodyFrame = new QFrame(OutputDialog);
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
        icon6.addFile(QString::fromUtf8(":/icons/tool-step.svg"), QSize(), QIcon::Normal, QIcon::Off);
        toolsStepButton->setIcon(icon6);
        toolsStepButton->setIconSize(QSize(34, 34));
        toolsStepButton->setCheckable(true);
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
        icon7.addFile(QString::fromUtf8(":/icons/output-active.svg"), QSize(), QIcon::Normal, QIcon::Off);
        outputStepButton->setIcon(icon7);
        outputStepButton->setIconSize(QSize(34, 34));
        outputStepButton->setCheckable(true);
        outputStepButton->setChecked(true);
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
        verticalLayout_editor->setSpacing(14);
        verticalLayout_editor->setObjectName(QString::fromUtf8("verticalLayout_editor"));
        verticalLayout_editor->setContentsMargins(24, 22, 24, 18);
        editorTitleLabel = new QLabel(setupEditorPanel);
        editorTitleLabel->setObjectName(QString::fromUtf8("editorTitleLabel"));

        verticalLayout_editor->addWidget(editorTitleLabel);

        schemeResultCard = new QFrame(setupEditorPanel);
        schemeResultCard->setObjectName(QString::fromUtf8("schemeResultCard"));
        schemeResultCard->setFrameShape(QFrame::NoFrame);
        schemeResultCard->setProperty("panelRole", QVariant(QString::fromUtf8("configCard")));
        verticalLayout_schemeResult = new QVBoxLayout(schemeResultCard);
        verticalLayout_schemeResult->setSpacing(12);
        verticalLayout_schemeResult->setObjectName(QString::fromUtf8("verticalLayout_schemeResult"));
        verticalLayout_schemeResult->setContentsMargins(20, 18, 18, 16);
        horizontalLayout_schemeResultHeader = new QHBoxLayout();
        horizontalLayout_schemeResultHeader->setObjectName(QString::fromUtf8("horizontalLayout_schemeResultHeader"));
        schemeResultTitle = new QLabel(schemeResultCard);
        schemeResultTitle->setObjectName(QString::fromUtf8("schemeResultTitle"));
        schemeResultTitle->setProperty("role", QVariant(QString::fromUtf8("cardTitle")));

        horizontalLayout_schemeResultHeader->addWidget(schemeResultTitle);

        horizontalSpacer_schemeHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_schemeResultHeader->addItem(horizontalSpacer_schemeHeader);

        schemeCollapseButton = new QToolButton(schemeResultCard);
        schemeCollapseButton->setObjectName(QString::fromUtf8("schemeCollapseButton"));
        QIcon icon8;
        icon8.addFile(QString::fromUtf8(":/icons/arrow-down.svg"), QSize(), QIcon::Normal, QIcon::Off);
        schemeCollapseButton->setIcon(icon8);
        schemeCollapseButton->setIconSize(QSize(18, 18));
        schemeCollapseButton->setAutoRaise(true);
        schemeCollapseButton->setProperty("role", QVariant(QString::fromUtf8("collapseCard")));

        horizontalLayout_schemeResultHeader->addWidget(schemeCollapseButton);


        verticalLayout_schemeResult->addLayout(horizontalLayout_schemeResultHeader);

        schemeHintLabel = new QLabel(schemeResultCard);
        schemeHintLabel->setObjectName(QString::fromUtf8("schemeHintLabel"));
        schemeHintLabel->setProperty("role", QVariant(QString::fromUtf8("cardHint")));

        verticalLayout_schemeResult->addWidget(schemeHintLabel);

        segmentFrame = new QFrame(schemeResultCard);
        segmentFrame->setObjectName(QString::fromUtf8("segmentFrame"));
        segmentFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_segment = new QHBoxLayout(segmentFrame);
        horizontalLayout_segment->setSpacing(6);
        horizontalLayout_segment->setObjectName(QString::fromUtf8("horizontalLayout_segment"));
        horizontalLayout_segment->setContentsMargins(6, 6, 6, 6);
        allModulesOkButton = new QPushButton(segmentFrame);
        allModulesOkButton->setObjectName(QString::fromUtf8("allModulesOkButton"));
        allModulesOkButton->setCheckable(true);
        allModulesOkButton->setChecked(true);

        horizontalLayout_segment->addWidget(allModulesOkButton);

        anyModuleOkButton = new QPushButton(segmentFrame);
        anyModuleOkButton->setObjectName(QString::fromUtf8("anyModuleOkButton"));
        anyModuleOkButton->setCheckable(true);

        horizontalLayout_segment->addWidget(anyModuleOkButton);

        customResultButton = new QPushButton(segmentFrame);
        customResultButton->setObjectName(QString::fromUtf8("customResultButton"));
        customResultButton->setCheckable(true);

        horizontalLayout_segment->addWidget(customResultButton);


        verticalLayout_schemeResult->addWidget(segmentFrame);


        verticalLayout_editor->addWidget(schemeResultCard);

        timedOutputCard = new QFrame(setupEditorPanel);
        timedOutputCard->setObjectName(QString::fromUtf8("timedOutputCard"));
        timedOutputCard->setFrameShape(QFrame::NoFrame);
        timedOutputCard->setProperty("panelRole", QVariant(QString::fromUtf8("configCard")));
        verticalLayout_timedOutput = new QVBoxLayout(timedOutputCard);
        verticalLayout_timedOutput->setSpacing(12);
        verticalLayout_timedOutput->setObjectName(QString::fromUtf8("verticalLayout_timedOutput"));
        verticalLayout_timedOutput->setContentsMargins(20, 18, 18, 16);
        horizontalLayout_timedHeader = new QHBoxLayout();
        horizontalLayout_timedHeader->setObjectName(QString::fromUtf8("horizontalLayout_timedHeader"));
        timedOutputTitle = new QLabel(timedOutputCard);
        timedOutputTitle->setObjectName(QString::fromUtf8("timedOutputTitle"));
        timedOutputTitle->setProperty("role", QVariant(QString::fromUtf8("cardTitle")));

        horizontalLayout_timedHeader->addWidget(timedOutputTitle);

        horizontalSpacer_timedHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_timedHeader->addItem(horizontalSpacer_timedHeader);

        timedCollapseButton = new QToolButton(timedOutputCard);
        timedCollapseButton->setObjectName(QString::fromUtf8("timedCollapseButton"));
        timedCollapseButton->setIcon(icon8);
        timedCollapseButton->setIconSize(QSize(18, 18));
        timedCollapseButton->setAutoRaise(true);
        timedCollapseButton->setProperty("role", QVariant(QString::fromUtf8("collapseCard")));

        horizontalLayout_timedHeader->addWidget(timedCollapseButton);


        verticalLayout_timedOutput->addLayout(horizontalLayout_timedHeader);

        horizontalLayout_timedSwitch = new QHBoxLayout();
        horizontalLayout_timedSwitch->setObjectName(QString::fromUtf8("horizontalLayout_timedSwitch"));
        timedSwitchLabel = new QLabel(timedOutputCard);
        timedSwitchLabel->setObjectName(QString::fromUtf8("timedSwitchLabel"));
        timedSwitchLabel->setProperty("role", QVariant(QString::fromUtf8("rowField")));

        horizontalLayout_timedSwitch->addWidget(timedSwitchLabel);

        horizontalSpacer_timedSwitch = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_timedSwitch->addItem(horizontalSpacer_timedSwitch);

        checkBox = new QCheckBox(timedOutputCard);
        checkBox->setObjectName(QString::fromUtf8("checkBox"));
        checkBox->setStyleSheet(QString::fromUtf8("QCheckBox {\n"
"    color: white;\n"
"    spacing: 8px;\n"
"}\n"
"\n"
"/* \345\274\200\345\205\263\350\203\214\346\231\257 - \345\205\263\351\227\255\347\212\266\346\200\201 */\n"
"QCheckBox::indicator {\n"
"    width: 50px;\n"
"    height: 22px;\n"
"    border-radius: 11px;\n"
"    background-color: #666666;\n"
"}\n"
"\n"
"/* \345\274\200\345\205\263\350\203\214\346\231\257 - \345\274\200\345\220\257\347\212\266\346\200\201\357\274\210\346\251\231\350\211\262\357\274\211 */\n"
"QCheckBox::indicator:checked {\n"
"    background-color: #FF7F24;\n"
"}\n"
"\n"
"/* \346\273\221\345\212\250\345\260\217\345\234\206\347\202\271 */\n"
"QCheckBox::indicator::handle {\n"
"    width: 18px;\n"
"    height: 18px;\n"
"    border-radius: 9px;\n"
"    background-color: white;\n"
"    margin: 2px;\n"
"}\n"
"QCheckBox::indicator:checked::handle {\n"
"    margin-left: 30px;\n"
"}"));

        horizontalLayout_timedSwitch->addWidget(checkBox);


        verticalLayout_timedOutput->addLayout(horizontalLayout_timedSwitch);

        horizontalLayout_timedSpin = new QHBoxLayout();
        horizontalLayout_timedSpin->setObjectName(QString::fromUtf8("horizontalLayout_timedSpin"));
        timedMsLabel = new QLabel(timedOutputCard);
        timedMsLabel->setObjectName(QString::fromUtf8("timedMsLabel"));
        timedMsLabel->setProperty("role", QVariant(QString::fromUtf8("rowField")));

        horizontalLayout_timedSpin->addWidget(timedMsLabel);

        horizontalSpacer_timedSpin = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_timedSpin->addItem(horizontalSpacer_timedSpin);

        timedSpinBox = new QSpinBox(timedOutputCard);
        timedSpinBox->setObjectName(QString::fromUtf8("timedSpinBox"));
        timedSpinBox->setMinimum(0);
        timedSpinBox->setMaximum(99999);
        timedSpinBox->setValue(100);

        horizontalLayout_timedSpin->addWidget(timedSpinBox);


        verticalLayout_timedOutput->addLayout(horizontalLayout_timedSpin);


        verticalLayout_editor->addWidget(timedOutputCard);

        outputParamsCard = new QFrame(setupEditorPanel);
        outputParamsCard->setObjectName(QString::fromUtf8("outputParamsCard"));
        outputParamsCard->setFrameShape(QFrame::NoFrame);
        outputParamsCard->setProperty("panelRole", QVariant(QString::fromUtf8("configCard")));
        verticalLayout_outputParams = new QVBoxLayout(outputParamsCard);
        verticalLayout_outputParams->setSpacing(12);
        verticalLayout_outputParams->setObjectName(QString::fromUtf8("verticalLayout_outputParams"));
        verticalLayout_outputParams->setContentsMargins(20, 18, 18, 16);
        horizontalLayout_outputHeader = new QHBoxLayout();
        horizontalLayout_outputHeader->setObjectName(QString::fromUtf8("horizontalLayout_outputHeader"));
        outputParamsTitle = new QLabel(outputParamsCard);
        outputParamsTitle->setObjectName(QString::fromUtf8("outputParamsTitle"));
        outputParamsTitle->setProperty("role", QVariant(QString::fromUtf8("cardTitle")));

        horizontalLayout_outputHeader->addWidget(outputParamsTitle);

        horizontalSpacer_outputHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_outputHeader->addItem(horizontalSpacer_outputHeader);

        outputCollapseButton = new QToolButton(outputParamsCard);
        outputCollapseButton->setObjectName(QString::fromUtf8("outputCollapseButton"));
        outputCollapseButton->setIcon(icon8);
        outputCollapseButton->setIconSize(QSize(18, 18));
        outputCollapseButton->setAutoRaise(true);
        outputCollapseButton->setProperty("role", QVariant(QString::fromUtf8("collapseCard")));

        horizontalLayout_outputHeader->addWidget(outputCollapseButton);


        verticalLayout_outputParams->addLayout(horizontalLayout_outputHeader);

        horizontalLayout_resultOutput = new QHBoxLayout();
        horizontalLayout_resultOutput->setObjectName(QString::fromUtf8("horizontalLayout_resultOutput"));
        resultOutputLabel = new QLabel(outputParamsCard);
        resultOutputLabel->setObjectName(QString::fromUtf8("resultOutputLabel"));
        resultOutputLabel->setProperty("role", QVariant(QString::fromUtf8("rowField")));

        horizontalLayout_resultOutput->addWidget(resultOutputLabel);

        horizontalSpacer_resultOutput = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_resultOutput->addItem(horizontalSpacer_resultOutput);

        checkBox_2 = new QCheckBox(outputParamsCard);
        checkBox_2->setObjectName(QString::fromUtf8("checkBox_2"));
        checkBox_2->setStyleSheet(QString::fromUtf8("QCheckBox {\n"
"    color: white;\n"
"    spacing: 8px;\n"
"}\n"
"\n"
"/* \345\274\200\345\205\263\350\203\214\346\231\257 - \345\205\263\351\227\255\347\212\266\346\200\201 */\n"
"QCheckBox::indicator {\n"
"    width: 50px;\n"
"    height: 22px;\n"
"    border-radius: 11px;\n"
"    background-color: #666666;\n"
"}\n"
"\n"
"/* \345\274\200\345\205\263\350\203\214\346\231\257 - \345\274\200\345\220\257\347\212\266\346\200\201\357\274\210\346\251\231\350\211\262\357\274\211 */\n"
"QCheckBox::indicator:checked {\n"
"    background-color: #FF7F24;\n"
"}\n"
"\n"
"/* \346\273\221\345\212\250\345\260\217\345\234\206\347\202\271 */\n"
"QCheckBox::indicator::handle {\n"
"    width: 18px;\n"
"    height: 18px;\n"
"    border-radius: 9px;\n"
"    background-color: white;\n"
"    margin: 2px;\n"
"}\n"
"QCheckBox::indicator:checked::handle {\n"
"    margin-left: 30px;\n"
"}"));

        horizontalLayout_resultOutput->addWidget(checkBox_2);


        verticalLayout_outputParams->addLayout(horizontalLayout_resultOutput);

        horizontalLayout_ioEdit = new QHBoxLayout();
        horizontalLayout_ioEdit->setObjectName(QString::fromUtf8("horizontalLayout_ioEdit"));
        ioOutputLabel = new QLabel(outputParamsCard);
        ioOutputLabel->setObjectName(QString::fromUtf8("ioOutputLabel"));
        ioOutputLabel->setProperty("role", QVariant(QString::fromUtf8("rowField")));

        horizontalLayout_ioEdit->addWidget(ioOutputLabel);

        horizontalSpacer_ioEdit = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_ioEdit->addItem(horizontalSpacer_ioEdit);

        editIoButton = new QPushButton(outputParamsCard);
        editIoButton->setObjectName(QString::fromUtf8("editIoButton"));
        editIoButton->setProperty("actionRole", QVariant(QString::fromUtf8("gray")));

        horizontalLayout_ioEdit->addWidget(editIoButton);


        verticalLayout_outputParams->addLayout(horizontalLayout_ioEdit);

        horizontalLayout_commJump = new QHBoxLayout();
        horizontalLayout_commJump->setObjectName(QString::fromUtf8("horizontalLayout_commJump"));
        commSetupLabel = new QLabel(outputParamsCard);
        commSetupLabel->setObjectName(QString::fromUtf8("commSetupLabel"));
        commSetupLabel->setProperty("role", QVariant(QString::fromUtf8("rowField")));

        horizontalLayout_commJump->addWidget(commSetupLabel);

        horizontalSpacer_commJump = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_commJump->addItem(horizontalSpacer_commJump);

        jumpCommButton = new QPushButton(outputParamsCard);
        jumpCommButton->setObjectName(QString::fromUtf8("jumpCommButton"));
        jumpCommButton->setProperty("actionRole", QVariant(QString::fromUtf8("gray")));

        horizontalLayout_commJump->addWidget(jumpCommButton);


        verticalLayout_outputParams->addLayout(horizontalLayout_commJump);


        verticalLayout_editor->addWidget(outputParamsCard);

        verticalSpacer_editor = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_editor->addItem(verticalSpacer_editor);

        horizontalLayout_nav = new QHBoxLayout();
        horizontalLayout_nav->setObjectName(QString::fromUtf8("horizontalLayout_nav"));
        previousButton = new QPushButton(setupEditorPanel);
        previousButton->setObjectName(QString::fromUtf8("previousButton"));

        horizontalLayout_nav->addWidget(previousButton);

        horizontalSpacer_nav = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_nav->addItem(horizontalSpacer_nav);

        finishButton = new QPushButton(setupEditorPanel);
        finishButton->setObjectName(QString::fromUtf8("finishButton"));

        horizontalLayout_nav->addWidget(finishButton);


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
        QIcon icon9;
        icon9.addFile(QString::fromUtf8(":/icons/grid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerGridButton->setIcon(icon9);
        viewerGridButton->setIconSize(QSize(24, 24));
        viewerGridButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerGridButton);

        viewerZoomSearchButton = new QToolButton(viewerHeader);
        viewerZoomSearchButton->setObjectName(QString::fromUtf8("viewerZoomSearchButton"));
        QIcon icon10;
        icon10.addFile(QString::fromUtf8(":/icons/zoom.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerZoomSearchButton->setIcon(icon10);
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
        QIcon icon11;
        icon11.addFile(QString::fromUtf8(":/icons/fit.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerZoomInButton->setIcon(icon11);
        viewerZoomInButton->setIconSize(QSize(24, 24));
        viewerZoomInButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerZoomInButton);

        viewerFullButton = new QToolButton(viewerHeader);
        viewerFullButton->setObjectName(QString::fromUtf8("viewerFullButton"));
        QIcon icon12;
        icon12.addFile(QString::fromUtf8(":/icons/fullscreen.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerFullButton->setIcon(icon12);
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


        retranslateUi(OutputDialog);

        QMetaObject::connectSlotsByName(OutputDialog);
    } // setupUi

    void retranslateUi(QDialog *OutputDialog)
    {
        OutputDialog->setWindowTitle(QCoreApplication::translate("OutputDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \350\276\223\345\207\272", nullptr));
        setupWindowTitleLabel->setText(QCoreApplication::translate("OutputDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("OutputDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("OutputDialog", "\303\227", nullptr));
        setupPageCodeLabel->setText(QCoreApplication::translate("OutputDialog", "324", nullptr));
        setupSaveButton->setText(QCoreApplication::translate("OutputDialog", "\344\277\235\345\255\230", nullptr));
        setupSaveAsButton->setText(QCoreApplication::translate("OutputDialog", "\345\217\246\345\255\230\344\270\272", nullptr));
        setupExportButton->setText(QCoreApplication::translate("OutputDialog", "IO\350\276\223\345\207\272", nullptr));
        cameraStepButton->setText(QCoreApplication::translate("OutputDialog", "\347\233\270\346\234\272\345\217\202\346\225\260", nullptr));
        stepDividerLabel1->setText(QCoreApplication::translate("OutputDialog", "\342\226\276", nullptr));
        referenceStepButton->setText(QCoreApplication::translate("OutputDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        stepDividerLabel2->setText(QCoreApplication::translate("OutputDialog", "\342\226\276", nullptr));
        toolsStepButton->setText(QCoreApplication::translate("OutputDialog", "\345\267\245\345\205\267", nullptr));
        stepDividerLabel3->setText(QCoreApplication::translate("OutputDialog", "\342\226\276", nullptr));
        outputStepButton->setText(QCoreApplication::translate("OutputDialog", "\350\276\223\345\207\272", nullptr));
        editorTitleLabel->setText(QCoreApplication::translate("OutputDialog", "4/ \350\276\223\345\207\272", nullptr));
        schemeResultTitle->setText(QCoreApplication::translate("OutputDialog", "\346\226\271\346\241\210\347\273\223\346\236\234", nullptr));
        schemeHintLabel->setText(QCoreApplication::translate("OutputDialog", "\350\256\276\347\275\256\346\226\271\346\241\210\346\225\264\344\275\223\350\276\223\345\207\272\347\273\223\346\236\234\344\270\272OK\347\232\204\346\235\241\344\273\266", nullptr));
        allModulesOkButton->setText(QCoreApplication::translate("OutputDialog", "\346\211\200\346\234\211\346\250\241\345\235\227OK", nullptr));
        anyModuleOkButton->setText(QCoreApplication::translate("OutputDialog", "\344\273\273\346\204\217\346\250\241\345\235\227OK", nullptr));
        customResultButton->setText(QCoreApplication::translate("OutputDialog", "\350\207\252\345\256\232\344\271\211", nullptr));
        timedOutputTitle->setText(QCoreApplication::translate("OutputDialog", "\345\256\232\346\227\266\350\276\223\345\207\272", nullptr));
        timedSwitchLabel->setText(QCoreApplication::translate("OutputDialog", "\345\256\232\346\227\266\350\276\223\345\207\272\344\275\277\350\203\275 \342\223\230", nullptr));
        checkBox->setText(QString());
        timedMsLabel->setText(QCoreApplication::translate("OutputDialog", "\345\256\232\346\227\266\346\227\266\351\227\264\357\274\210ms\357\274\211", nullptr));
        outputParamsTitle->setText(QCoreApplication::translate("OutputDialog", "\350\276\223\345\207\272\345\217\202\346\225\260", nullptr));
        resultOutputLabel->setText(QCoreApplication::translate("OutputDialog", "\347\273\223\346\236\234\350\276\223\345\207\272", nullptr));
        checkBox_2->setText(QString());
        ioOutputLabel->setText(QCoreApplication::translate("OutputDialog", "IO\350\276\223\345\207\272", nullptr));
        editIoButton->setText(QCoreApplication::translate("OutputDialog", "\347\274\226\350\276\221", nullptr));
        commSetupLabel->setText(QCoreApplication::translate("OutputDialog", "\351\200\232\344\277\241\350\256\276\347\275\256", nullptr));
        jumpCommButton->setText(QCoreApplication::translate("OutputDialog", "\350\267\263\350\275\254", nullptr));
        previousButton->setText(QCoreApplication::translate("OutputDialog", "\343\200\210 \344\270\212\344\270\200\346\255\245", nullptr));
        finishButton->setText(QCoreApplication::translate("OutputDialog", "\345\256\214\346\210\220", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("OutputDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerZoomOutButton->setText(QCoreApplication::translate("OutputDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("OutputDialog", "126%", nullptr));
        viewerStatusLabel->setText(QCoreApplication::translate("OutputDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: --ms   \345\267\245\345\205\267\350\200\227\346\227\266: --ms", nullptr));
        viewerCursorLabel->setText(QCoreApplication::translate("OutputDialog", "X: --  Y: --   |   R: --  G: --  B: --", nullptr));
    } // retranslateUi

};

namespace Ui {
    class OutputDialog: public Ui_OutputDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OUTPUTDIALOG_H
