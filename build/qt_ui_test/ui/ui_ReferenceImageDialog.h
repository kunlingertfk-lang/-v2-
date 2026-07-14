/********************************************************************************
** Form generated from reading UI file 'ReferenceImageDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_REFERENCEIMAGEDIALOG_H
#define UI_REFERENCEIMAGEDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ReferenceImageDialog
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
    QFrame *toolbarDivider;
    QToolButton *setupExportButton;
    QToolButton *setupQuickCalibrateButton;
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
    QVBoxLayout *verticalLayout_3;
    QFrame *editorHeaderFrame;
    QHBoxLayout *horizontalLayout_editorHeader;
    QLabel *editorTitleLabel;
    QSpacerItem *horizontalSpacer_editorHeader;
    QFrame *segmentFrame;
    QHBoxLayout *horizontalLayout_segmentFrame;
    QPushButton *basicModeButton;
    QPushButton *allModeButton;
    QScrollArea *referenceParamsScrollArea;
    QWidget *referenceParamsScrollContent;
    QVBoxLayout *verticalLayout_2;
    QFrame *multiReferenceCard;
    QVBoxLayout *verticalLayout_multiReferenceCard;
    QFrame *multiReferenceCardHeaderFrame;
    QHBoxLayout *horizontalLayout_multiReferenceCardHeader;
    QLabel *multiReferenceCardTitleLabel;
    QSpacerItem *horizontalSpacer_multiReferenceCardHeader;
    QCheckBox *multiReferenceCheckBox;
    QToolButton *multiReferenceCardCollapseButton;
    QFrame *referenceCard;
    QVBoxLayout *verticalLayout_referenceCard;
    QFrame *referenceCardHeaderFrame;
    QHBoxLayout *horizontalLayout_referenceCardHeader;
    QLabel *referenceCardTitleLabel;
    QSpacerItem *horizontalSpacer_referenceCardHeader;
    QToolButton *referenceCardCollapseButton;
    QLabel *referenceHintLabel;
    QFrame *referenceButtonFrame;
    QHBoxLayout *horizontalLayout_referenceButtons;
    QPushButton *currentImageButton;
    QPushButton *historyImageButton;
    QPushButton *pcImportButton;
    QFrame *positionCorrectionCard;
    QVBoxLayout *verticalLayout;
    QFrame *positionCorrectionCardHeaderFrame;
    QHBoxLayout *horizontalLayout_positionCorrectionCardHeader;
    QLabel *positionCorrectionCardTitleLabel;
    QSpacerItem *horizontalSpacer_positionCorrectionCardHeader;
    QCheckBox *positionCorrectionCheckBox;
    QToolButton *positionCorrectionCardCollapseButton;
    QLabel *positionCorrectionHintLabel;
    QFrame *correctionExampleFrame;
    QHBoxLayout *horizontalLayout;
    QFrame *enabledCorrectionPreview;
    QGridLayout *gridLayout;
    QLabel *enabledCorrectionTitleLabel;
    QLabel *enabledCorrectionImageLabel;
    QFrame *disabledCorrectionPreview;
    QGridLayout *gridLayout_2;
    QLabel *disabledCorrectionTitleLabel;
    QLabel *disabledCorrectionImageLabel;
    QSpacerItem *verticalSpacer_referenceParams;
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

    void setupUi(QDialog *ReferenceImageDialog)
    {
        if (ReferenceImageDialog->objectName().isEmpty())
            ReferenceImageDialog->setObjectName(QString::fromUtf8("ReferenceImageDialog"));
        ReferenceImageDialog->resize(1760, 980);
        ReferenceImageDialog->setStyleSheet(QString::fromUtf8("QDialog#CameraParamsDialog,\n"
"QDialog#ReferenceImageDialog {\n"
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
"    background: #ff4b"
                        "4b;\n"
"}\n"
"\n"
"QFrame#setupToolbar {\n"
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
"QToolButton#referenc"
                        "eStepButton:hover,\n"
"QToolButton#toolsStepButton:hover,\n"
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
""
                        "QLabel#editorTitleLabel,\n"
"QLabel[editorTitle=\"true\"] {\n"
"    background: transparent;\n"
"    color: #324969;\n"
"    font-size: 24px;\n"
"    font-weight: 800;\n"
"}\n"
"\n"
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
"    background: #fff"
                        "fff;\n"
"    border: 1px solid #e1e5eb;\n"
"    border-radius: 8px;\n"
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
"    "
                        "color: #536274;\n"
"    padding: 0 18px;\n"
"    font-size: 16px;\n"
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
""
                        "\n"
"QPushButton#previousButton,\n"
"QPushButton#nextButton,\n"
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
"    width:"
                        " 28px;\n"
"}\n"
"\n"
"QCheckBox {\n"
"    spacing: 0;\n"
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
"QFrame[table="
                        "\"true\"] QLabel[tableCell=\"true\"] {\n"
"    min-height: 34px;\n"
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
"    qproperty-align"
                        "ment: AlignCenter;\n"
"}\n"
"\n"
"QFrame#setupViewerFrame,\n"
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
"    background: transparent;"
                        "\n"
"    color: #f0f2f5;\n"
"}\n"
"\n"
"QToolButton#viewerGridButton:hover,\n"
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
        verticalLayout_root = new QVBoxLayout(ReferenceImageDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        setupTopBar = new QFrame(ReferenceImageDialog);
        setupTopBar->setObjectName(QString::fromUtf8("setupTopBar"));
        setupTopBar->setMinimumSize(QSize(0, 54));
        setupTopBar->setMaximumSize(QSize(16777215, 54));
        setupTopBar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_topBar = new QHBoxLayout(setupTopBar);
        horizontalLayout_topBar->setSpacing(8);
        horizontalLayout_topBar->setObjectName(QString::fromUtf8("horizontalLayout_topBar"));
        horizontalLayout_topBar->setContentsMargins(18, 0, 10, 0);
        setupWindowTitleLabel = new QLabel(setupTopBar);
        setupWindowTitleLabel->setObjectName(QString::fromUtf8("setupWindowTitleLabel"));

        horizontalLayout_topBar->addWidget(setupWindowTitleLabel);

        horizontalSpacer_topBar = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_topBar->addItem(horizontalSpacer_topBar);

        headerMaximizeButton = new QToolButton(setupTopBar);
        headerMaximizeButton->setObjectName(QString::fromUtf8("headerMaximizeButton"));
        headerMaximizeButton->setMinimumSize(QSize(34, 34));
        headerMaximizeButton->setAutoRaise(true);

        horizontalLayout_topBar->addWidget(headerMaximizeButton);

        headerCloseButton = new QToolButton(setupTopBar);
        headerCloseButton->setObjectName(QString::fromUtf8("headerCloseButton"));
        headerCloseButton->setMinimumSize(QSize(34, 34));
        headerCloseButton->setAutoRaise(true);

        horizontalLayout_topBar->addWidget(headerCloseButton);


        verticalLayout_root->addWidget(setupTopBar);

        setupToolbar = new QFrame(ReferenceImageDialog);
        setupToolbar->setObjectName(QString::fromUtf8("setupToolbar"));
        setupToolbar->setMinimumSize(QSize(0, 72));
        setupToolbar->setMaximumSize(QSize(16777215, 72));
        setupToolbar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_toolbar = new QHBoxLayout(setupToolbar);
        horizontalLayout_toolbar->setSpacing(18);
        horizontalLayout_toolbar->setObjectName(QString::fromUtf8("horizontalLayout_toolbar"));
        horizontalLayout_toolbar->setContentsMargins(24, 8, 24, 8);
        setupPageCodeLabel = new QLabel(setupToolbar);
        setupPageCodeLabel->setObjectName(QString::fromUtf8("setupPageCodeLabel"));
        setupPageCodeLabel->setProperty("pageCode", QVariant(true));

        horizontalLayout_toolbar->addWidget(setupPageCodeLabel);

        setupExternalEditButton = new QToolButton(setupToolbar);
        setupExternalEditButton->setObjectName(QString::fromUtf8("setupExternalEditButton"));
        setupExternalEditButton->setMinimumSize(QSize(42, 42));
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
        setupSaveButton->setMinimumSize(QSize(66, 56));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/save.svg"), QSize(), QIcon::Normal, QIcon::Off);
        setupSaveButton->setIcon(icon1);
        setupSaveButton->setIconSize(QSize(24, 24));
        setupSaveButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        setupSaveButton->setAutoRaise(true);

        horizontalLayout_toolbar->addWidget(setupSaveButton);

        setupSaveAsButton = new QToolButton(setupToolbar);
        setupSaveAsButton->setObjectName(QString::fromUtf8("setupSaveAsButton"));
        setupSaveAsButton->setMinimumSize(QSize(72, 56));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/save-as.svg"), QSize(), QIcon::Normal, QIcon::Off);
        setupSaveAsButton->setIcon(icon2);
        setupSaveAsButton->setIconSize(QSize(24, 24));
        setupSaveAsButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        setupSaveAsButton->setAutoRaise(true);

        horizontalLayout_toolbar->addWidget(setupSaveAsButton);

        toolbarDivider = new QFrame(setupToolbar);
        toolbarDivider->setObjectName(QString::fromUtf8("toolbarDivider"));
        toolbarDivider->setMinimumSize(QSize(1, 36));
        toolbarDivider->setMaximumSize(QSize(1, 36));
        toolbarDivider->setFrameShape(QFrame::NoFrame);

        horizontalLayout_toolbar->addWidget(toolbarDivider);

        setupExportButton = new QToolButton(setupToolbar);
        setupExportButton->setObjectName(QString::fromUtf8("setupExportButton"));
        setupExportButton->setMinimumSize(QSize(72, 56));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icons/io.svg"), QSize(), QIcon::Normal, QIcon::Off);
        setupExportButton->setIcon(icon3);
        setupExportButton->setIconSize(QSize(24, 24));
        setupExportButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        setupExportButton->setAutoRaise(true);

        horizontalLayout_toolbar->addWidget(setupExportButton);

        setupQuickCalibrateButton = new QToolButton(setupToolbar);
        setupQuickCalibrateButton->setObjectName(QString::fromUtf8("setupQuickCalibrateButton"));
        setupQuickCalibrateButton->setMinimumSize(QSize(82, 56));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/fit.svg"), QSize(), QIcon::Normal, QIcon::Off);
        setupQuickCalibrateButton->setIcon(icon4);
        setupQuickCalibrateButton->setIconSize(QSize(24, 24));
        setupQuickCalibrateButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        setupQuickCalibrateButton->setAutoRaise(true);

        horizontalLayout_toolbar->addWidget(setupQuickCalibrateButton);


        verticalLayout_root->addWidget(setupToolbar);

        setupBodyFrame = new QFrame(ReferenceImageDialog);
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
        cameraStepButton->setMinimumSize(QSize(108, 106));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/sliders.svg"), QSize(), QIcon::Normal, QIcon::Off);
        cameraStepButton->setIcon(icon5);
        cameraStepButton->setIconSize(QSize(34, 34));
        cameraStepButton->setCheckable(true);
        cameraStepButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        verticalLayout_steps->addWidget(cameraStepButton);

        stepDividerLabel1 = new QLabel(setupStepRail);
        stepDividerLabel1->setObjectName(QString::fromUtf8("stepDividerLabel1"));
        stepDividerLabel1->setAlignment(Qt::AlignCenter);
        stepDividerLabel1->setProperty("stepArrow", QVariant(true));

        verticalLayout_steps->addWidget(stepDividerLabel1);

        referenceStepButton = new QToolButton(setupStepRail);
        referenceStepButton->setObjectName(QString::fromUtf8("referenceStepButton"));
        referenceStepButton->setMinimumSize(QSize(108, 106));
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/icons/reference.svg"), QSize(), QIcon::Normal, QIcon::Off);
        referenceStepButton->setIcon(icon6);
        referenceStepButton->setIconSize(QSize(34, 34));
        referenceStepButton->setCheckable(true);
        referenceStepButton->setChecked(true);
        referenceStepButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        verticalLayout_steps->addWidget(referenceStepButton);

        stepDividerLabel2 = new QLabel(setupStepRail);
        stepDividerLabel2->setObjectName(QString::fromUtf8("stepDividerLabel2"));
        stepDividerLabel2->setAlignment(Qt::AlignCenter);
        stepDividerLabel2->setProperty("stepArrow", QVariant(true));

        verticalLayout_steps->addWidget(stepDividerLabel2);

        toolsStepButton = new QToolButton(setupStepRail);
        toolsStepButton->setObjectName(QString::fromUtf8("toolsStepButton"));
        toolsStepButton->setMinimumSize(QSize(108, 106));
        QIcon icon7;
        icon7.addFile(QString::fromUtf8(":/icons/tool.svg"), QSize(), QIcon::Normal, QIcon::Off);
        toolsStepButton->setIcon(icon7);
        toolsStepButton->setIconSize(QSize(34, 34));
        toolsStepButton->setCheckable(true);
        toolsStepButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

        verticalLayout_steps->addWidget(toolsStepButton);

        stepDividerLabel3 = new QLabel(setupStepRail);
        stepDividerLabel3->setObjectName(QString::fromUtf8("stepDividerLabel3"));
        stepDividerLabel3->setAlignment(Qt::AlignCenter);
        stepDividerLabel3->setProperty("stepArrow", QVariant(true));

        verticalLayout_steps->addWidget(stepDividerLabel3);

        outputStepButton = new QToolButton(setupStepRail);
        outputStepButton->setObjectName(QString::fromUtf8("outputStepButton"));
        outputStepButton->setMinimumSize(QSize(108, 106));
        QIcon icon8;
        icon8.addFile(QString::fromUtf8(":/icons/output.svg"), QSize(), QIcon::Normal, QIcon::Off);
        outputStepButton->setIcon(icon8);
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
        setupEditorPanel->setProperty("editorPanel", QVariant(true));
        verticalLayout_3 = new QVBoxLayout(setupEditorPanel);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        editorHeaderFrame = new QFrame(setupEditorPanel);
        editorHeaderFrame->setObjectName(QString::fromUtf8("editorHeaderFrame"));
        editorHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_editorHeader = new QHBoxLayout(editorHeaderFrame);
        horizontalLayout_editorHeader->setSpacing(0);
        horizontalLayout_editorHeader->setObjectName(QString::fromUtf8("horizontalLayout_editorHeader"));
        horizontalLayout_editorHeader->setContentsMargins(0, 0, 0, 14);
        editorTitleLabel = new QLabel(editorHeaderFrame);
        editorTitleLabel->setObjectName(QString::fromUtf8("editorTitleLabel"));
        editorTitleLabel->setProperty("editorTitle", QVariant(true));

        horizontalLayout_editorHeader->addWidget(editorTitleLabel);

        horizontalSpacer_editorHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_editorHeader->addItem(horizontalSpacer_editorHeader);

        segmentFrame = new QFrame(editorHeaderFrame);
        segmentFrame->setObjectName(QString::fromUtf8("segmentFrame"));
        segmentFrame->setMinimumSize(QSize(212, 34));
        segmentFrame->setMaximumSize(QSize(16777215, 34));
        segmentFrame->setFrameShape(QFrame::NoFrame);
        segmentFrame->setProperty("segmented", QVariant(true));
        horizontalLayout_segmentFrame = new QHBoxLayout(segmentFrame);
        horizontalLayout_segmentFrame->setSpacing(0);
        horizontalLayout_segmentFrame->setObjectName(QString::fromUtf8("horizontalLayout_segmentFrame"));
        horizontalLayout_segmentFrame->setContentsMargins(0, 0, 0, 0);
        basicModeButton = new QPushButton(segmentFrame);
        basicModeButton->setObjectName(QString::fromUtf8("basicModeButton"));
        basicModeButton->setMinimumSize(QSize(106, 38));
        basicModeButton->setCheckable(true);
        basicModeButton->setChecked(true);

        horizontalLayout_segmentFrame->addWidget(basicModeButton);

        allModeButton = new QPushButton(segmentFrame);
        allModeButton->setObjectName(QString::fromUtf8("allModeButton"));
        allModeButton->setMinimumSize(QSize(106, 38));
        allModeButton->setCheckable(true);

        horizontalLayout_segmentFrame->addWidget(allModeButton);


        horizontalLayout_editorHeader->addWidget(segmentFrame);


        verticalLayout_3->addWidget(editorHeaderFrame);

        referenceParamsScrollArea = new QScrollArea(setupEditorPanel);
        referenceParamsScrollArea->setObjectName(QString::fromUtf8("referenceParamsScrollArea"));
        referenceParamsScrollArea->setFrameShape(QFrame::NoFrame);
        referenceParamsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        referenceParamsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        referenceParamsScrollArea->setWidgetResizable(true);
        referenceParamsScrollContent = new QWidget();
        referenceParamsScrollContent->setObjectName(QString::fromUtf8("referenceParamsScrollContent"));
        referenceParamsScrollContent->setGeometry(QRect(0, 0, 592, 702));
        verticalLayout_2 = new QVBoxLayout(referenceParamsScrollContent);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        multiReferenceCard = new QFrame(referenceParamsScrollContent);
        multiReferenceCard->setObjectName(QString::fromUtf8("multiReferenceCard"));
        multiReferenceCard->setFrameShape(QFrame::NoFrame);
        multiReferenceCard->setProperty("card", QVariant(true));
        verticalLayout_multiReferenceCard = new QVBoxLayout(multiReferenceCard);
        verticalLayout_multiReferenceCard->setSpacing(14);
        verticalLayout_multiReferenceCard->setObjectName(QString::fromUtf8("verticalLayout_multiReferenceCard"));
        verticalLayout_multiReferenceCard->setContentsMargins(18, 18, 18, 18);
        multiReferenceCardHeaderFrame = new QFrame(multiReferenceCard);
        multiReferenceCardHeaderFrame->setObjectName(QString::fromUtf8("multiReferenceCardHeaderFrame"));
        multiReferenceCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_multiReferenceCardHeader = new QHBoxLayout(multiReferenceCardHeaderFrame);
        horizontalLayout_multiReferenceCardHeader->setSpacing(0);
        horizontalLayout_multiReferenceCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_multiReferenceCardHeader"));
        horizontalLayout_multiReferenceCardHeader->setContentsMargins(0, 0, 0, 0);
        multiReferenceCardTitleLabel = new QLabel(multiReferenceCardHeaderFrame);
        multiReferenceCardTitleLabel->setObjectName(QString::fromUtf8("multiReferenceCardTitleLabel"));
        multiReferenceCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_multiReferenceCardHeader->addWidget(multiReferenceCardTitleLabel);

        horizontalSpacer_multiReferenceCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_multiReferenceCardHeader->addItem(horizontalSpacer_multiReferenceCardHeader);

        multiReferenceCheckBox = new QCheckBox(multiReferenceCardHeaderFrame);
        multiReferenceCheckBox->setObjectName(QString::fromUtf8("multiReferenceCheckBox"));
        multiReferenceCheckBox->setMinimumSize(QSize(58, 28));

        horizontalLayout_multiReferenceCardHeader->addWidget(multiReferenceCheckBox);

        multiReferenceCardCollapseButton = new QToolButton(multiReferenceCardHeaderFrame);
        multiReferenceCardCollapseButton->setObjectName(QString::fromUtf8("multiReferenceCardCollapseButton"));
        multiReferenceCardCollapseButton->setMinimumSize(QSize(28, 28));
        multiReferenceCardCollapseButton->setVisible(false);
        multiReferenceCardCollapseButton->setAutoRaise(true);

        horizontalLayout_multiReferenceCardHeader->addWidget(multiReferenceCardCollapseButton);


        verticalLayout_multiReferenceCard->addWidget(multiReferenceCardHeaderFrame);


        verticalLayout_2->addWidget(multiReferenceCard);

        referenceCard = new QFrame(referenceParamsScrollContent);
        referenceCard->setObjectName(QString::fromUtf8("referenceCard"));
        referenceCard->setFrameShape(QFrame::NoFrame);
        referenceCard->setProperty("card", QVariant(true));
        verticalLayout_referenceCard = new QVBoxLayout(referenceCard);
        verticalLayout_referenceCard->setSpacing(14);
        verticalLayout_referenceCard->setObjectName(QString::fromUtf8("verticalLayout_referenceCard"));
        verticalLayout_referenceCard->setContentsMargins(18, 18, 18, 18);
        referenceCardHeaderFrame = new QFrame(referenceCard);
        referenceCardHeaderFrame->setObjectName(QString::fromUtf8("referenceCardHeaderFrame"));
        referenceCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_referenceCardHeader = new QHBoxLayout(referenceCardHeaderFrame);
        horizontalLayout_referenceCardHeader->setSpacing(0);
        horizontalLayout_referenceCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_referenceCardHeader"));
        horizontalLayout_referenceCardHeader->setContentsMargins(0, 0, 0, 0);
        referenceCardTitleLabel = new QLabel(referenceCardHeaderFrame);
        referenceCardTitleLabel->setObjectName(QString::fromUtf8("referenceCardTitleLabel"));
        referenceCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_referenceCardHeader->addWidget(referenceCardTitleLabel);

        horizontalSpacer_referenceCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_referenceCardHeader->addItem(horizontalSpacer_referenceCardHeader);

        referenceCardCollapseButton = new QToolButton(referenceCardHeaderFrame);
        referenceCardCollapseButton->setObjectName(QString::fromUtf8("referenceCardCollapseButton"));
        referenceCardCollapseButton->setMinimumSize(QSize(28, 28));
        referenceCardCollapseButton->setAutoRaise(true);

        horizontalLayout_referenceCardHeader->addWidget(referenceCardCollapseButton);


        verticalLayout_referenceCard->addWidget(referenceCardHeaderFrame);

        referenceHintLabel = new QLabel(referenceCard);
        referenceHintLabel->setObjectName(QString::fromUtf8("referenceHintLabel"));
        referenceHintLabel->setWordWrap(true);
        referenceHintLabel->setProperty("hint", QVariant(true));

        verticalLayout_referenceCard->addWidget(referenceHintLabel);

        referenceButtonFrame = new QFrame(referenceCard);
        referenceButtonFrame->setObjectName(QString::fromUtf8("referenceButtonFrame"));
        referenceButtonFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_referenceButtons = new QHBoxLayout(referenceButtonFrame);
        horizontalLayout_referenceButtons->setSpacing(10);
        horizontalLayout_referenceButtons->setObjectName(QString::fromUtf8("horizontalLayout_referenceButtons"));
        horizontalLayout_referenceButtons->setContentsMargins(0, 0, 0, 0);
        currentImageButton = new QPushButton(referenceButtonFrame);
        currentImageButton->setObjectName(QString::fromUtf8("currentImageButton"));
        currentImageButton->setMinimumSize(QSize(150, 46));
        QIcon icon9;
        icon9.addFile(QString::fromUtf8(":/icons/camera.svg"), QSize(), QIcon::Normal, QIcon::Off);
        currentImageButton->setIcon(icon9);
        currentImageButton->setIconSize(QSize(24, 24));

        horizontalLayout_referenceButtons->addWidget(currentImageButton);

        historyImageButton = new QPushButton(referenceButtonFrame);
        historyImageButton->setObjectName(QString::fromUtf8("historyImageButton"));
        historyImageButton->setMinimumSize(QSize(150, 44));
        QIcon icon10;
        icon10.addFile(QString::fromUtf8(":/icons/add.svg"), QSize(), QIcon::Normal, QIcon::Off);
        historyImageButton->setIcon(icon10);
        historyImageButton->setIconSize(QSize(24, 24));

        horizontalLayout_referenceButtons->addWidget(historyImageButton);

        pcImportButton = new QPushButton(referenceButtonFrame);
        pcImportButton->setObjectName(QString::fromUtf8("pcImportButton"));
        pcImportButton->setMinimumSize(QSize(150, 44));
        pcImportButton->setIcon(icon10);
        pcImportButton->setIconSize(QSize(24, 24));

        horizontalLayout_referenceButtons->addWidget(pcImportButton);


        verticalLayout_referenceCard->addWidget(referenceButtonFrame);


        verticalLayout_2->addWidget(referenceCard);

        positionCorrectionCard = new QFrame(referenceParamsScrollContent);
        positionCorrectionCard->setObjectName(QString::fromUtf8("positionCorrectionCard"));
        positionCorrectionCard->setFrameShape(QFrame::NoFrame);
        positionCorrectionCard->setProperty("card", QVariant(true));
        verticalLayout = new QVBoxLayout(positionCorrectionCard);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        positionCorrectionCardHeaderFrame = new QFrame(positionCorrectionCard);
        positionCorrectionCardHeaderFrame->setObjectName(QString::fromUtf8("positionCorrectionCardHeaderFrame"));
        positionCorrectionCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_positionCorrectionCardHeader = new QHBoxLayout(positionCorrectionCardHeaderFrame);
        horizontalLayout_positionCorrectionCardHeader->setSpacing(0);
        horizontalLayout_positionCorrectionCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_positionCorrectionCardHeader"));
        horizontalLayout_positionCorrectionCardHeader->setContentsMargins(0, 0, 0, 0);
        positionCorrectionCardTitleLabel = new QLabel(positionCorrectionCardHeaderFrame);
        positionCorrectionCardTitleLabel->setObjectName(QString::fromUtf8("positionCorrectionCardTitleLabel"));
        positionCorrectionCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_positionCorrectionCardHeader->addWidget(positionCorrectionCardTitleLabel);

        horizontalSpacer_positionCorrectionCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_positionCorrectionCardHeader->addItem(horizontalSpacer_positionCorrectionCardHeader);

        positionCorrectionCheckBox = new QCheckBox(positionCorrectionCardHeaderFrame);
        positionCorrectionCheckBox->setObjectName(QString::fromUtf8("positionCorrectionCheckBox"));
        positionCorrectionCheckBox->setMinimumSize(QSize(58, 28));

        horizontalLayout_positionCorrectionCardHeader->addWidget(positionCorrectionCheckBox);

        positionCorrectionCardCollapseButton = new QToolButton(positionCorrectionCardHeaderFrame);
        positionCorrectionCardCollapseButton->setObjectName(QString::fromUtf8("positionCorrectionCardCollapseButton"));
        positionCorrectionCardCollapseButton->setMinimumSize(QSize(28, 28));
        positionCorrectionCardCollapseButton->setVisible(false);
        positionCorrectionCardCollapseButton->setAutoRaise(true);

        horizontalLayout_positionCorrectionCardHeader->addWidget(positionCorrectionCardCollapseButton);


        verticalLayout->addWidget(positionCorrectionCardHeaderFrame);

        positionCorrectionHintLabel = new QLabel(positionCorrectionCard);
        positionCorrectionHintLabel->setObjectName(QString::fromUtf8("positionCorrectionHintLabel"));
        positionCorrectionHintLabel->setWordWrap(true);
        positionCorrectionHintLabel->setProperty("hint", QVariant(true));

        verticalLayout->addWidget(positionCorrectionHintLabel);

        correctionExampleFrame = new QFrame(positionCorrectionCard);
        correctionExampleFrame->setObjectName(QString::fromUtf8("correctionExampleFrame"));
        correctionExampleFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout = new QHBoxLayout(correctionExampleFrame);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        enabledCorrectionPreview = new QFrame(correctionExampleFrame);
        enabledCorrectionPreview->setObjectName(QString::fromUtf8("enabledCorrectionPreview"));
        enabledCorrectionPreview->setFrameShape(QFrame::NoFrame);
        enabledCorrectionPreview->setProperty("examplePanel", QVariant(true));
        gridLayout = new QGridLayout(enabledCorrectionPreview);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        enabledCorrectionTitleLabel = new QLabel(enabledCorrectionPreview);
        enabledCorrectionTitleLabel->setObjectName(QString::fromUtf8("enabledCorrectionTitleLabel"));
        enabledCorrectionTitleLabel->setProperty("greenText", QVariant(true));

        gridLayout->addWidget(enabledCorrectionTitleLabel, 0, 0, 1, 1);

        enabledCorrectionImageLabel = new QLabel(enabledCorrectionPreview);
        enabledCorrectionImageLabel->setObjectName(QString::fromUtf8("enabledCorrectionImageLabel"));
        enabledCorrectionImageLabel->setProperty("correctionPreview", QVariant(true));

        gridLayout->addWidget(enabledCorrectionImageLabel, 1, 0, 1, 1);


        horizontalLayout->addWidget(enabledCorrectionPreview);

        disabledCorrectionPreview = new QFrame(correctionExampleFrame);
        disabledCorrectionPreview->setObjectName(QString::fromUtf8("disabledCorrectionPreview"));
        disabledCorrectionPreview->setFrameShape(QFrame::NoFrame);
        disabledCorrectionPreview->setProperty("examplePanel", QVariant(true));
        gridLayout_2 = new QGridLayout(disabledCorrectionPreview);
        gridLayout_2->setObjectName(QString::fromUtf8("gridLayout_2"));
        disabledCorrectionTitleLabel = new QLabel(disabledCorrectionPreview);
        disabledCorrectionTitleLabel->setObjectName(QString::fromUtf8("disabledCorrectionTitleLabel"));
        disabledCorrectionTitleLabel->setProperty("redText", QVariant(true));

        gridLayout_2->addWidget(disabledCorrectionTitleLabel, 0, 0, 1, 1);

        disabledCorrectionImageLabel = new QLabel(disabledCorrectionPreview);
        disabledCorrectionImageLabel->setObjectName(QString::fromUtf8("disabledCorrectionImageLabel"));
        disabledCorrectionImageLabel->setProperty("correctionPreview", QVariant(true));

        gridLayout_2->addWidget(disabledCorrectionImageLabel, 1, 0, 1, 1);


        horizontalLayout->addWidget(disabledCorrectionPreview);


        verticalLayout->addWidget(correctionExampleFrame);


        verticalLayout_2->addWidget(positionCorrectionCard);

        verticalSpacer_referenceParams = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer_referenceParams);

        referenceParamsScrollArea->setWidget(referenceParamsScrollContent);

        verticalLayout_3->addWidget(referenceParamsScrollArea);

        horizontalLayout_nav = new QHBoxLayout();
        horizontalLayout_nav->setSpacing(0);
        horizontalLayout_nav->setObjectName(QString::fromUtf8("horizontalLayout_nav"));
        horizontalLayout_nav->setContentsMargins(6, 14, 6, 9);
        previousButton = new QPushButton(setupEditorPanel);
        previousButton->setObjectName(QString::fromUtf8("previousButton"));
        previousButton->setMinimumSize(QSize(112, 50));

        horizontalLayout_nav->addWidget(previousButton);

        horizontalSpacer_nav = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_nav->addItem(horizontalSpacer_nav);

        nextButton = new QPushButton(setupEditorPanel);
        nextButton->setObjectName(QString::fromUtf8("nextButton"));
        nextButton->setMinimumSize(QSize(110, 48));

        horizontalLayout_nav->addWidget(nextButton);


        verticalLayout_3->addLayout(horizontalLayout_nav);


        horizontalLayout_body->addWidget(setupEditorPanel);

        setupViewerFrame = new QFrame(setupBodyFrame);
        setupViewerFrame->setObjectName(QString::fromUtf8("setupViewerFrame"));
        setupViewerFrame->setFrameShape(QFrame::NoFrame);
        setupViewerFrame->setProperty("viewerPanel", QVariant(true));
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
        horizontalLayout_viewerHeader->setSpacing(8);
        horizontalLayout_viewerHeader->setObjectName(QString::fromUtf8("horizontalLayout_viewerHeader"));
        horizontalLayout_viewerHeader->setContentsMargins(18, 0, 14, 0);
        viewerTitleLabel = new QLabel(viewerHeader);
        viewerTitleLabel->setObjectName(QString::fromUtf8("viewerTitleLabel"));
        viewerTitleLabel->setProperty("viewerTitle", QVariant(true));

        horizontalLayout_viewerHeader->addWidget(viewerTitleLabel);

        horizontalSpacer_viewerHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_viewerHeader->addItem(horizontalSpacer_viewerHeader);

        viewerGridButton = new QToolButton(viewerHeader);
        viewerGridButton->setObjectName(QString::fromUtf8("viewerGridButton"));
        viewerGridButton->setMinimumSize(QSize(28, 28));
        QIcon icon11;
        icon11.addFile(QString::fromUtf8(":/icons/grid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerGridButton->setIcon(icon11);
        viewerGridButton->setIconSize(QSize(24, 24));
        viewerGridButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerGridButton);

        viewerZoomSearchButton = new QToolButton(viewerHeader);
        viewerZoomSearchButton->setObjectName(QString::fromUtf8("viewerZoomSearchButton"));
        viewerZoomSearchButton->setMinimumSize(QSize(28, 28));
        QIcon icon12;
        icon12.addFile(QString::fromUtf8(":/icons/zoom.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerZoomSearchButton->setIcon(icon12);
        viewerZoomSearchButton->setIconSize(QSize(24, 24));
        viewerZoomSearchButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerZoomSearchButton);

        viewerZoomOutButton = new QToolButton(viewerHeader);
        viewerZoomOutButton->setObjectName(QString::fromUtf8("viewerZoomOutButton"));
        viewerZoomOutButton->setMinimumSize(QSize(28, 28));
        viewerZoomOutButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerZoomOutButton);

        viewerZoomLabel = new QLabel(viewerHeader);
        viewerZoomLabel->setObjectName(QString::fromUtf8("viewerZoomLabel"));
        viewerZoomLabel->setProperty("viewerZoom", QVariant(true));

        horizontalLayout_viewerHeader->addWidget(viewerZoomLabel);

        viewerZoomInButton = new QToolButton(viewerHeader);
        viewerZoomInButton->setObjectName(QString::fromUtf8("viewerZoomInButton"));
        viewerZoomInButton->setMinimumSize(QSize(28, 28));
        viewerZoomInButton->setIcon(icon4);
        viewerZoomInButton->setIconSize(QSize(24, 24));
        viewerZoomInButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerZoomInButton);

        viewerFullButton = new QToolButton(viewerHeader);
        viewerFullButton->setObjectName(QString::fromUtf8("viewerFullButton"));
        viewerFullButton->setMinimumSize(QSize(28, 28));
        QIcon icon13;
        icon13.addFile(QString::fromUtf8(":/icons/fullscreen.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerFullButton->setIcon(icon13);
        viewerFullButton->setIconSize(QSize(24, 24));
        viewerFullButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerFullButton);


        verticalLayout_viewer->addWidget(viewerHeader);

        previewGraphicsView = new QGraphicsView(setupViewerFrame);
        previewGraphicsView->setObjectName(QString::fromUtf8("previewGraphicsView"));
        previewGraphicsView->setProperty("viewerCanvas", QVariant(true));

        verticalLayout_viewer->addWidget(previewGraphicsView);

        viewerStatusBar = new QFrame(setupViewerFrame);
        viewerStatusBar->setObjectName(QString::fromUtf8("viewerStatusBar"));
        viewerStatusBar->setMinimumSize(QSize(0, 42));
        viewerStatusBar->setMaximumSize(QSize(16777215, 42));
        viewerStatusBar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_viewerStatus = new QHBoxLayout(viewerStatusBar);
        horizontalLayout_viewerStatus->setSpacing(10);
        horizontalLayout_viewerStatus->setObjectName(QString::fromUtf8("horizontalLayout_viewerStatus"));
        horizontalLayout_viewerStatus->setContentsMargins(18, 0, 16, 0);
        viewerStatusLabel = new QLabel(viewerStatusBar);
        viewerStatusLabel->setObjectName(QString::fromUtf8("viewerStatusLabel"));
        viewerStatusLabel->setProperty("viewerStatus", QVariant(true));

        horizontalLayout_viewerStatus->addWidget(viewerStatusLabel);

        horizontalSpacer_viewerStatus = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_viewerStatus->addItem(horizontalSpacer_viewerStatus);

        viewerCursorLabel = new QLabel(viewerStatusBar);
        viewerCursorLabel->setObjectName(QString::fromUtf8("viewerCursorLabel"));
        viewerCursorLabel->setProperty("viewerStatus", QVariant(true));

        horizontalLayout_viewerStatus->addWidget(viewerCursorLabel);


        verticalLayout_viewer->addWidget(viewerStatusBar);


        horizontalLayout_body->addWidget(setupViewerFrame);


        verticalLayout_root->addWidget(setupBodyFrame);


        retranslateUi(ReferenceImageDialog);

        QMetaObject::connectSlotsByName(ReferenceImageDialog);
    } // setupUi

    void retranslateUi(QDialog *ReferenceImageDialog)
    {
        ReferenceImageDialog->setWindowTitle(QCoreApplication::translate("ReferenceImageDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \350\256\276\347\275\256\345\237\272\345\207\206\345\233\276", nullptr));
        setupWindowTitleLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\303\227", nullptr));
        setupPageCodeLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "422", nullptr));
        setupExternalEditButton->setText(QString());
        setupSaveButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\344\277\235\345\255\230", nullptr));
        setupSaveAsButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\217\246\345\255\230\344\270\272", nullptr));
        setupExportButton->setText(QCoreApplication::translate("ReferenceImageDialog", "IO\350\276\223\345\207\272", nullptr));
        setupQuickCalibrateButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\277\253\351\200\237\346\240\207\345\256\232", nullptr));
        cameraStepButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\347\233\270\346\234\272\345\217\202\346\225\260", nullptr));
        stepDividerLabel1->setText(QCoreApplication::translate("ReferenceImageDialog", "\342\226\276", nullptr));
        referenceStepButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        stepDividerLabel2->setText(QCoreApplication::translate("ReferenceImageDialog", "\342\226\276", nullptr));
        toolsStepButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\267\245\345\205\267", nullptr));
        stepDividerLabel3->setText(QCoreApplication::translate("ReferenceImageDialog", "\342\226\276", nullptr));
        outputStepButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\350\276\223\345\207\272", nullptr));
        editorTitleLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "2 / \350\256\276\347\275\256\345\237\272\345\207\206\345\233\276", nullptr));
        basicModeButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\237\272\347\241\200", nullptr));
        allModeButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\205\250\351\203\250", nullptr));
        multiReferenceCardTitleLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\244\232\345\237\272\345\207\206", nullptr));
        multiReferenceCheckBox->setText(QString());
        multiReferenceCardCollapseButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\342\214\204", nullptr));
        referenceCardTitleLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        referenceCardCollapseButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\342\214\204", nullptr));
        referenceHintLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\346\212\212\351\234\200\350\246\201\344\275\234\344\270\272\345\210\244\346\226\255\344\276\235\346\215\256\347\232\204\345\233\276\345\203\217\350\256\276\347\275\256\344\270\272\345\237\272\345\207\206\345\233\276", nullptr));
        currentImageButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\275\223\345\211\215\345\233\276\345\203\217", nullptr));
        historyImageButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\216\206\345\217\262\345\233\276\345\203\217", nullptr));
        pcImportButton->setText(QCoreApplication::translate("ReferenceImageDialog", "PC\345\257\274\345\205\245", nullptr));
        positionCorrectionCardTitleLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        positionCorrectionCheckBox->setText(QString());
        positionCorrectionCardCollapseButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\342\214\204", nullptr));
        positionCorrectionHintLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\273\272\347\253\213\346\243\200\346\265\213\345\237\272\345\207\206\357\274\214\344\277\256\346\255\243\350\247\206\350\247\211\345\267\245\345\205\267\346\243\200\346\265\213\345\214\272\345\237\237\347\233\270\345\257\271\345\201\217\347\247\273\357\274\214\350\267\237\351\232\217\345\237\272\345\207\206\347\247\273\345\212\250\343\200\202", nullptr));
        enabledCorrectionTitleLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\220\257\347\224\250\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        enabledCorrectionImageLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\342\226\241  \342\227\207\n"
"\342\226\243  \342\227\207", nullptr));
        disabledCorrectionTitleLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\346\234\252\345\220\257\347\224\250\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        disabledCorrectionImageLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\342\226\241  \342\227\207\n"
"\342\226\241  \342\227\207", nullptr));
        previousButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\342\200\271 \344\270\212\344\270\200\346\255\245", nullptr));
        nextButton->setText(QCoreApplication::translate("ReferenceImageDialog", "\344\270\213\344\270\200\346\255\245 \342\200\272", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerGridButton->setText(QString());
        viewerZoomSearchButton->setText(QString());
        viewerZoomOutButton->setText(QCoreApplication::translate("ReferenceImageDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "39%", nullptr));
        viewerZoomInButton->setText(QString());
        viewerFullButton->setText(QString());
        viewerStatusLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: 0ms   \345\267\245\345\205\267\350\200\227\346\227\266: 0ms", nullptr));
        viewerCursorLabel->setText(QCoreApplication::translate("ReferenceImageDialog", "X: --  Y: --   |   R: --  G: --  B: --", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ReferenceImageDialog: public Ui_ReferenceImageDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_REFERENCEIMAGEDIALOG_H
