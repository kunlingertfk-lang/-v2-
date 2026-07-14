/********************************************************************************
** Form generated from reading UI file 'EdgePresenceDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_EDGEPRESENCEDIALOG_H
#define UI_EDGEPRESENCEDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_EdgePresenceDialog
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
    QFrame *setupEditorPanel;
    QVBoxLayout *verticalLayout_editor;
    QHBoxLayout *horizontalLayout_editorHeader;
    QLabel *editorTitleLabel;
    QToolButton *edgeExternalEditButton;
    QSpacerItem *horizontalSpacer_editorHeader;
    QFrame *segmentFrame;
    QHBoxLayout *horizontalLayout_segment;
    QPushButton *basicSegmentButton;
    QPushButton *allSegmentButton;
    QStackedWidget *edgeParamsStackedWidget;
    QWidget *basicParamsPage;
    QVBoxLayout *verticalLayout_basicParamsPage;
    QScrollArea *edgeBasicParamsScrollArea;
    QWidget *edgeBasicParamsContents;
    QVBoxLayout *verticalLayout_basicConfigCards;
    QFrame *basicDetectionAreaCard;
    QVBoxLayout *basicVerticalLayout_detectionArea;
    QHBoxLayout *basicHorizontalLayout_detectionHeader;
    QLabel *basicDetectionAreaTitleLabel;
    QSpacerItem *basicHorizontalLayout_detectionHeaderSpacer;
    QToolButton *basicDetectionCollapseButton;
    QHBoxLayout *basicHorizontalLayout_detectionAreaEdit;
    QLabel *basicDetectionAreaLabel;
    QToolButton *basicDetectionRectButton;
    QHBoxLayout *basicHorizontalLayout_positionEnable;
    QLabel *basicPositionEnableLabel;
    QSpacerItem *basicHorizontalSpacer_positionEnable;
    QCheckBox *basicPositionCorrectionSwitch;
    QHBoxLayout *basicHorizontalLayout_positionSource;
    QLabel *basicPositionSourceLabel;
    QComboBox *basicPositionCorrectionComboBox;
    QFrame *basicRecognitionSettingsCard;
    QVBoxLayout *basicVerticalLayout_recognitionSettings;
    QHBoxLayout *basicHorizontalLayout_recognitionHeader;
    QLabel *basicRecognitionSettingsTitleLabel;
    QSpacerItem *basicHorizontalLayout_recognitionHeaderSpacer;
    QToolButton *basicRecognitionCollapseButton;
    QHBoxLayout *basicHorizontalLayout_sensitivity;
    QLabel *basicSensitivityLabel;
    QSpinBox *edgeBasicSensitivitySpinBox;
    QFrame *basicResultJudgeCard;
    QVBoxLayout *basicVerticalLayout_resultJudge;
    QHBoxLayout *basicHorizontalLayout_resultHeader;
    QLabel *basicResultJudgeTitleLabel;
    QSpacerItem *basicHorizontalLayout_resultHeaderSpacer;
    QFrame *basicResultPresenceFrame;
    QHBoxLayout *basicHorizontalLayout_resultPresenceButtons;
    QPushButton *basicPresentOkButton;
    QPushButton *basicAbsentOkButton;
    QSpacerItem *basicVerticalSpacer_configCards;
    QWidget *allParamsPage;
    QVBoxLayout *verticalLayout_allParamsPage;
    QScrollArea *edgeAllParamsScrollArea;
    QWidget *edgeAllParamsContents;
    QVBoxLayout *verticalLayout_allConfigCards;
    QFrame *detectionAreaCard;
    QVBoxLayout *verticalLayout_detectionArea;
    QHBoxLayout *horizontalLayout_detectionHeader;
    QLabel *detectionAreaTitleLabel;
    QSpacerItem *horizontalLayout_detectionHeaderSpacer;
    QToolButton *detectionCollapseButton;
    QHBoxLayout *horizontalLayout_detectionAreaEdit;
    QLabel *detectionAreaLabel;
    QToolButton *detectionRectButton;
    QHBoxLayout *horizontalLayout_detectionMask;
    QLabel *detectionMaskLabel;
    QPushButton *detectionMaskEditButton;
    QHBoxLayout *horizontalLayout_positionEnable;
    QLabel *positionEnableLabel;
    QSpacerItem *horizontalSpacer_positionEnable;
    QCheckBox *positionCorrectionSwitch;
    QHBoxLayout *horizontalLayout_positionSource;
    QLabel *positionSourceLabel;
    QComboBox *positionCorrectionComboBox;
    QFrame *recognitionSettingsCard;
    QVBoxLayout *verticalLayout_recognitionSettings;
    QHBoxLayout *horizontalLayout_recognitionHeader;
    QLabel *recognitionSettingsTitleLabel;
    QSpacerItem *horizontalLayout_recognitionHeaderSpacer;
    QToolButton *recognitionCollapseButton;
    QHBoxLayout *horizontalLayout_sensitivity;
    QLabel *sensitivityLabel;
    QSpinBox *edgeSensitivitySpinBox;
    QHBoxLayout *horizontalLayout_edgePolarity;
    QLabel *edgePolarityLabel;
    QComboBox *edgePolarityComboBox;
    QFrame *resultJudgeCard;
    QVBoxLayout *verticalLayout_resultJudge;
    QHBoxLayout *horizontalLayout_resultHeader;
    QLabel *resultJudgeTitleLabel;
    QSpacerItem *horizontalLayout_resultHeaderSpacer;
    QFrame *resultPresenceFrame;
    QHBoxLayout *horizontalLayout_resultPresenceButtons;
    QPushButton *presentOkButton;
    QPushButton *absentOkButton;
    QSpacerItem *verticalSpacer_configCards;
    QHBoxLayout *horizontalLayout_actions;
    QSpacerItem *horizontalSpacer_actions;
    QPushButton *referenceTestButton;
    QPushButton *testRunButton;
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

    void setupUi(QDialog *EdgePresenceDialog)
    {
        if (EdgePresenceDialog->objectName().isEmpty())
            EdgePresenceDialog->setObjectName(QString::fromUtf8("EdgePresenceDialog"));
        EdgePresenceDialog->resize(1760, 980);
        verticalLayout_root = new QVBoxLayout(EdgePresenceDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        setupTopBar = new QFrame(EdgePresenceDialog);
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

        setupToolbar = new QFrame(EdgePresenceDialog);
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

        setupBodyFrame = new QFrame(EdgePresenceDialog);
        setupBodyFrame->setObjectName(QString::fromUtf8("setupBodyFrame"));
        setupBodyFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_body = new QHBoxLayout(setupBodyFrame);
        horizontalLayout_body->setSpacing(0);
        horizontalLayout_body->setObjectName(QString::fromUtf8("horizontalLayout_body"));
        horizontalLayout_body->setContentsMargins(0, 0, 0, 0);
        setupEditorPanel = new QFrame(setupBodyFrame);
        setupEditorPanel->setObjectName(QString::fromUtf8("setupEditorPanel"));
        setupEditorPanel->setMinimumSize(QSize(610, 0));
        setupEditorPanel->setMaximumSize(QSize(610, 16777215));
        setupEditorPanel->setFrameShape(QFrame::NoFrame);
        verticalLayout_editor = new QVBoxLayout(setupEditorPanel);
        verticalLayout_editor->setSpacing(14);
        verticalLayout_editor->setObjectName(QString::fromUtf8("verticalLayout_editor"));
        verticalLayout_editor->setContentsMargins(28, 22, 28, 18);
        horizontalLayout_editorHeader = new QHBoxLayout();
        horizontalLayout_editorHeader->setObjectName(QString::fromUtf8("horizontalLayout_editorHeader"));
        editorTitleLabel = new QLabel(setupEditorPanel);
        editorTitleLabel->setObjectName(QString::fromUtf8("editorTitleLabel"));

        horizontalLayout_editorHeader->addWidget(editorTitleLabel);

        edgeExternalEditButton = new QToolButton(setupEditorPanel);
        edgeExternalEditButton->setObjectName(QString::fromUtf8("edgeExternalEditButton"));
        edgeExternalEditButton->setIcon(icon);
        edgeExternalEditButton->setIconSize(QSize(24, 24));
        edgeExternalEditButton->setAutoRaise(true);

        horizontalLayout_editorHeader->addWidget(edgeExternalEditButton);

        horizontalSpacer_editorHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_editorHeader->addItem(horizontalSpacer_editorHeader);

        segmentFrame = new QFrame(setupEditorPanel);
        segmentFrame->setObjectName(QString::fromUtf8("segmentFrame"));
        segmentFrame->setMinimumSize(QSize(170, 42));
        segmentFrame->setMaximumSize(QSize(170, 42));
        segmentFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_segment = new QHBoxLayout(segmentFrame);
        horizontalLayout_segment->setSpacing(0);
        horizontalLayout_segment->setObjectName(QString::fromUtf8("horizontalLayout_segment"));
        horizontalLayout_segment->setContentsMargins(2, 2, 2, 2);
        basicSegmentButton = new QPushButton(segmentFrame);
        basicSegmentButton->setObjectName(QString::fromUtf8("basicSegmentButton"));
        basicSegmentButton->setCheckable(true);
        basicSegmentButton->setChecked(true);

        horizontalLayout_segment->addWidget(basicSegmentButton);

        allSegmentButton = new QPushButton(segmentFrame);
        allSegmentButton->setObjectName(QString::fromUtf8("allSegmentButton"));
        allSegmentButton->setCheckable(true);

        horizontalLayout_segment->addWidget(allSegmentButton);


        horizontalLayout_editorHeader->addWidget(segmentFrame);


        verticalLayout_editor->addLayout(horizontalLayout_editorHeader);

        edgeParamsStackedWidget = new QStackedWidget(setupEditorPanel);
        edgeParamsStackedWidget->setObjectName(QString::fromUtf8("edgeParamsStackedWidget"));
        basicParamsPage = new QWidget();
        basicParamsPage->setObjectName(QString::fromUtf8("basicParamsPage"));
        verticalLayout_basicParamsPage = new QVBoxLayout(basicParamsPage);
        verticalLayout_basicParamsPage->setSpacing(0);
        verticalLayout_basicParamsPage->setObjectName(QString::fromUtf8("verticalLayout_basicParamsPage"));
        verticalLayout_basicParamsPage->setContentsMargins(0, 0, 0, 0);
        edgeBasicParamsScrollArea = new QScrollArea(basicParamsPage);
        edgeBasicParamsScrollArea->setObjectName(QString::fromUtf8("edgeBasicParamsScrollArea"));
        edgeBasicParamsScrollArea->setFrameShape(QFrame::NoFrame);
        edgeBasicParamsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        edgeBasicParamsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        edgeBasicParamsScrollArea->setWidgetResizable(true);
        edgeBasicParamsContents = new QWidget();
        edgeBasicParamsContents->setObjectName(QString::fromUtf8("edgeBasicParamsContents"));
        edgeBasicParamsContents->setGeometry(QRect(0, 0, 554, 692));
        verticalLayout_basicConfigCards = new QVBoxLayout(edgeBasicParamsContents);
        verticalLayout_basicConfigCards->setSpacing(12);
        verticalLayout_basicConfigCards->setObjectName(QString::fromUtf8("verticalLayout_basicConfigCards"));
        verticalLayout_basicConfigCards->setContentsMargins(0, 0, 0, 0);
        basicDetectionAreaCard = new QFrame(edgeBasicParamsContents);
        basicDetectionAreaCard->setObjectName(QString::fromUtf8("basicDetectionAreaCard"));
        basicDetectionAreaCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_detectionArea = new QVBoxLayout(basicDetectionAreaCard);
        basicVerticalLayout_detectionArea->setSpacing(16);
        basicVerticalLayout_detectionArea->setObjectName(QString::fromUtf8("basicVerticalLayout_detectionArea"));
        basicVerticalLayout_detectionArea->setContentsMargins(20, 18, 20, 18);
        basicHorizontalLayout_detectionHeader = new QHBoxLayout();
        basicHorizontalLayout_detectionHeader->setObjectName(QString::fromUtf8("basicHorizontalLayout_detectionHeader"));
        basicHorizontalLayout_detectionHeader->setContentsMargins(0, 0, 0, 0);
        basicDetectionAreaTitleLabel = new QLabel(basicDetectionAreaCard);
        basicDetectionAreaTitleLabel->setObjectName(QString::fromUtf8("basicDetectionAreaTitleLabel"));

        basicHorizontalLayout_detectionHeader->addWidget(basicDetectionAreaTitleLabel);

        basicHorizontalLayout_detectionHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_detectionHeader->addItem(basicHorizontalLayout_detectionHeaderSpacer);

        basicDetectionCollapseButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionCollapseButton->setObjectName(QString::fromUtf8("basicDetectionCollapseButton"));

        basicHorizontalLayout_detectionHeader->addWidget(basicDetectionCollapseButton);


        basicVerticalLayout_detectionArea->addLayout(basicHorizontalLayout_detectionHeader);

        basicHorizontalLayout_detectionAreaEdit = new QHBoxLayout();
        basicHorizontalLayout_detectionAreaEdit->setObjectName(QString::fromUtf8("basicHorizontalLayout_detectionAreaEdit"));
        basicHorizontalLayout_detectionAreaEdit->setContentsMargins(0, 0, 0, 0);
        basicDetectionAreaLabel = new QLabel(basicDetectionAreaCard);
        basicDetectionAreaLabel->setObjectName(QString::fromUtf8("basicDetectionAreaLabel"));
        basicDetectionAreaLabel->setMinimumSize(QSize(150, 0));

        basicHorizontalLayout_detectionAreaEdit->addWidget(basicDetectionAreaLabel);

        basicDetectionRectButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionRectButton->setObjectName(QString::fromUtf8("basicDetectionRectButton"));
        basicDetectionRectButton->setMinimumSize(QSize(124, 38));
        basicDetectionRectButton->setMaximumSize(QSize(124, 38));
        basicDetectionRectButton->setCheckable(true);
        basicDetectionRectButton->setChecked(true);

        basicHorizontalLayout_detectionAreaEdit->addWidget(basicDetectionRectButton);


        basicVerticalLayout_detectionArea->addLayout(basicHorizontalLayout_detectionAreaEdit);

        basicHorizontalLayout_positionEnable = new QHBoxLayout();
        basicHorizontalLayout_positionEnable->setObjectName(QString::fromUtf8("basicHorizontalLayout_positionEnable"));
        basicHorizontalLayout_positionEnable->setContentsMargins(0, 0, 0, 0);
        basicPositionEnableLabel = new QLabel(basicDetectionAreaCard);
        basicPositionEnableLabel->setObjectName(QString::fromUtf8("basicPositionEnableLabel"));
        basicPositionEnableLabel->setMinimumSize(QSize(260, 0));

        basicHorizontalLayout_positionEnable->addWidget(basicPositionEnableLabel);

        basicHorizontalSpacer_positionEnable = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_positionEnable->addItem(basicHorizontalSpacer_positionEnable);

        basicPositionCorrectionSwitch = new QCheckBox(basicDetectionAreaCard);
        basicPositionCorrectionSwitch->setObjectName(QString::fromUtf8("basicPositionCorrectionSwitch"));
        basicPositionCorrectionSwitch->setChecked(true);

        basicHorizontalLayout_positionEnable->addWidget(basicPositionCorrectionSwitch);


        basicVerticalLayout_detectionArea->addLayout(basicHorizontalLayout_positionEnable);

        basicHorizontalLayout_positionSource = new QHBoxLayout();
        basicHorizontalLayout_positionSource->setObjectName(QString::fromUtf8("basicHorizontalLayout_positionSource"));
        basicHorizontalLayout_positionSource->setContentsMargins(0, 0, 0, 0);
        basicPositionSourceLabel = new QLabel(basicDetectionAreaCard);
        basicPositionSourceLabel->setObjectName(QString::fromUtf8("basicPositionSourceLabel"));
        basicPositionSourceLabel->setMinimumSize(QSize(150, 0));

        basicHorizontalLayout_positionSource->addWidget(basicPositionSourceLabel);

        basicPositionCorrectionComboBox = new QComboBox(basicDetectionAreaCard);
        basicPositionCorrectionComboBox->addItem(QString());
        basicPositionCorrectionComboBox->setObjectName(QString::fromUtf8("basicPositionCorrectionComboBox"));
        basicPositionCorrectionComboBox->setMinimumSize(QSize(260, 38));

        basicHorizontalLayout_positionSource->addWidget(basicPositionCorrectionComboBox);


        basicVerticalLayout_detectionArea->addLayout(basicHorizontalLayout_positionSource);


        verticalLayout_basicConfigCards->addWidget(basicDetectionAreaCard);

        basicRecognitionSettingsCard = new QFrame(edgeBasicParamsContents);
        basicRecognitionSettingsCard->setObjectName(QString::fromUtf8("basicRecognitionSettingsCard"));
        basicRecognitionSettingsCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_recognitionSettings = new QVBoxLayout(basicRecognitionSettingsCard);
        basicVerticalLayout_recognitionSettings->setSpacing(16);
        basicVerticalLayout_recognitionSettings->setObjectName(QString::fromUtf8("basicVerticalLayout_recognitionSettings"));
        basicVerticalLayout_recognitionSettings->setContentsMargins(20, 18, 20, 18);
        basicHorizontalLayout_recognitionHeader = new QHBoxLayout();
        basicHorizontalLayout_recognitionHeader->setObjectName(QString::fromUtf8("basicHorizontalLayout_recognitionHeader"));
        basicHorizontalLayout_recognitionHeader->setContentsMargins(0, 0, 0, 0);
        basicRecognitionSettingsTitleLabel = new QLabel(basicRecognitionSettingsCard);
        basicRecognitionSettingsTitleLabel->setObjectName(QString::fromUtf8("basicRecognitionSettingsTitleLabel"));

        basicHorizontalLayout_recognitionHeader->addWidget(basicRecognitionSettingsTitleLabel);

        basicHorizontalLayout_recognitionHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_recognitionHeader->addItem(basicHorizontalLayout_recognitionHeaderSpacer);

        basicRecognitionCollapseButton = new QToolButton(basicRecognitionSettingsCard);
        basicRecognitionCollapseButton->setObjectName(QString::fromUtf8("basicRecognitionCollapseButton"));

        basicHorizontalLayout_recognitionHeader->addWidget(basicRecognitionCollapseButton);


        basicVerticalLayout_recognitionSettings->addLayout(basicHorizontalLayout_recognitionHeader);

        basicHorizontalLayout_sensitivity = new QHBoxLayout();
        basicHorizontalLayout_sensitivity->setObjectName(QString::fromUtf8("basicHorizontalLayout_sensitivity"));
        basicHorizontalLayout_sensitivity->setContentsMargins(0, 0, 0, 0);
        basicSensitivityLabel = new QLabel(basicRecognitionSettingsCard);
        basicSensitivityLabel->setObjectName(QString::fromUtf8("basicSensitivityLabel"));
        basicSensitivityLabel->setMinimumSize(QSize(150, 0));

        basicHorizontalLayout_sensitivity->addWidget(basicSensitivityLabel);

        edgeBasicSensitivitySpinBox = new QSpinBox(basicRecognitionSettingsCard);
        edgeBasicSensitivitySpinBox->setObjectName(QString::fromUtf8("edgeBasicSensitivitySpinBox"));
        edgeBasicSensitivitySpinBox->setMinimumSize(QSize(260, 38));
        edgeBasicSensitivitySpinBox->setMinimum(0);
        edgeBasicSensitivitySpinBox->setMaximum(100);
        edgeBasicSensitivitySpinBox->setValue(60);

        basicHorizontalLayout_sensitivity->addWidget(edgeBasicSensitivitySpinBox);


        basicVerticalLayout_recognitionSettings->addLayout(basicHorizontalLayout_sensitivity);


        verticalLayout_basicConfigCards->addWidget(basicRecognitionSettingsCard);

        basicResultJudgeCard = new QFrame(edgeBasicParamsContents);
        basicResultJudgeCard->setObjectName(QString::fromUtf8("basicResultJudgeCard"));
        basicResultJudgeCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_resultJudge = new QVBoxLayout(basicResultJudgeCard);
        basicVerticalLayout_resultJudge->setSpacing(16);
        basicVerticalLayout_resultJudge->setObjectName(QString::fromUtf8("basicVerticalLayout_resultJudge"));
        basicVerticalLayout_resultJudge->setContentsMargins(20, 18, 20, 18);
        basicHorizontalLayout_resultHeader = new QHBoxLayout();
        basicHorizontalLayout_resultHeader->setObjectName(QString::fromUtf8("basicHorizontalLayout_resultHeader"));
        basicHorizontalLayout_resultHeader->setContentsMargins(0, 0, 0, 0);
        basicResultJudgeTitleLabel = new QLabel(basicResultJudgeCard);
        basicResultJudgeTitleLabel->setObjectName(QString::fromUtf8("basicResultJudgeTitleLabel"));

        basicHorizontalLayout_resultHeader->addWidget(basicResultJudgeTitleLabel);

        basicHorizontalLayout_resultHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_resultHeader->addItem(basicHorizontalLayout_resultHeaderSpacer);

        basicResultPresenceFrame = new QFrame(basicResultJudgeCard);
        basicResultPresenceFrame->setObjectName(QString::fromUtf8("basicResultPresenceFrame"));
        basicResultPresenceFrame->setMinimumSize(QSize(260, 38));
        basicResultPresenceFrame->setMaximumSize(QSize(260, 38));
        basicHorizontalLayout_resultPresenceButtons = new QHBoxLayout(basicResultPresenceFrame);
        basicHorizontalLayout_resultPresenceButtons->setSpacing(0);
        basicHorizontalLayout_resultPresenceButtons->setObjectName(QString::fromUtf8("basicHorizontalLayout_resultPresenceButtons"));
        basicHorizontalLayout_resultPresenceButtons->setContentsMargins(0, 0, 0, 0);
        basicPresentOkButton = new QPushButton(basicResultPresenceFrame);
        basicPresentOkButton->setObjectName(QString::fromUtf8("basicPresentOkButton"));
        basicPresentOkButton->setCheckable(true);
        basicPresentOkButton->setChecked(true);

        basicHorizontalLayout_resultPresenceButtons->addWidget(basicPresentOkButton);

        basicAbsentOkButton = new QPushButton(basicResultPresenceFrame);
        basicAbsentOkButton->setObjectName(QString::fromUtf8("basicAbsentOkButton"));
        basicAbsentOkButton->setCheckable(true);

        basicHorizontalLayout_resultPresenceButtons->addWidget(basicAbsentOkButton);


        basicHorizontalLayout_resultHeader->addWidget(basicResultPresenceFrame);


        basicVerticalLayout_resultJudge->addLayout(basicHorizontalLayout_resultHeader);


        verticalLayout_basicConfigCards->addWidget(basicResultJudgeCard);

        basicVerticalSpacer_configCards = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_basicConfigCards->addItem(basicVerticalSpacer_configCards);

        edgeBasicParamsScrollArea->setWidget(edgeBasicParamsContents);

        verticalLayout_basicParamsPage->addWidget(edgeBasicParamsScrollArea);

        edgeParamsStackedWidget->addWidget(basicParamsPage);
        allParamsPage = new QWidget();
        allParamsPage->setObjectName(QString::fromUtf8("allParamsPage"));
        verticalLayout_allParamsPage = new QVBoxLayout(allParamsPage);
        verticalLayout_allParamsPage->setSpacing(0);
        verticalLayout_allParamsPage->setObjectName(QString::fromUtf8("verticalLayout_allParamsPage"));
        verticalLayout_allParamsPage->setContentsMargins(0, 0, 0, 0);
        edgeAllParamsScrollArea = new QScrollArea(allParamsPage);
        edgeAllParamsScrollArea->setObjectName(QString::fromUtf8("edgeAllParamsScrollArea"));
        edgeAllParamsScrollArea->setFrameShape(QFrame::NoFrame);
        edgeAllParamsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        edgeAllParamsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        edgeAllParamsScrollArea->setWidgetResizable(true);
        edgeAllParamsContents = new QWidget();
        edgeAllParamsContents->setObjectName(QString::fromUtf8("edgeAllParamsContents"));
        edgeAllParamsContents->setGeometry(QRect(0, 0, 554, 692));
        verticalLayout_allConfigCards = new QVBoxLayout(edgeAllParamsContents);
        verticalLayout_allConfigCards->setSpacing(12);
        verticalLayout_allConfigCards->setObjectName(QString::fromUtf8("verticalLayout_allConfigCards"));
        verticalLayout_allConfigCards->setContentsMargins(0, 0, 0, 0);
        detectionAreaCard = new QFrame(edgeAllParamsContents);
        detectionAreaCard->setObjectName(QString::fromUtf8("detectionAreaCard"));
        detectionAreaCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_detectionArea = new QVBoxLayout(detectionAreaCard);
        verticalLayout_detectionArea->setSpacing(16);
        verticalLayout_detectionArea->setObjectName(QString::fromUtf8("verticalLayout_detectionArea"));
        verticalLayout_detectionArea->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_detectionHeader = new QHBoxLayout();
        horizontalLayout_detectionHeader->setObjectName(QString::fromUtf8("horizontalLayout_detectionHeader"));
        horizontalLayout_detectionHeader->setContentsMargins(0, 0, 0, 0);
        detectionAreaTitleLabel = new QLabel(detectionAreaCard);
        detectionAreaTitleLabel->setObjectName(QString::fromUtf8("detectionAreaTitleLabel"));

        horizontalLayout_detectionHeader->addWidget(detectionAreaTitleLabel);

        horizontalLayout_detectionHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_detectionHeader->addItem(horizontalLayout_detectionHeaderSpacer);

        detectionCollapseButton = new QToolButton(detectionAreaCard);
        detectionCollapseButton->setObjectName(QString::fromUtf8("detectionCollapseButton"));

        horizontalLayout_detectionHeader->addWidget(detectionCollapseButton);


        verticalLayout_detectionArea->addLayout(horizontalLayout_detectionHeader);

        horizontalLayout_detectionAreaEdit = new QHBoxLayout();
        horizontalLayout_detectionAreaEdit->setObjectName(QString::fromUtf8("horizontalLayout_detectionAreaEdit"));
        horizontalLayout_detectionAreaEdit->setContentsMargins(0, 0, 0, 0);
        detectionAreaLabel = new QLabel(detectionAreaCard);
        detectionAreaLabel->setObjectName(QString::fromUtf8("detectionAreaLabel"));
        detectionAreaLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_detectionAreaEdit->addWidget(detectionAreaLabel);

        detectionRectButton = new QToolButton(detectionAreaCard);
        detectionRectButton->setObjectName(QString::fromUtf8("detectionRectButton"));
        detectionRectButton->setMinimumSize(QSize(124, 38));
        detectionRectButton->setMaximumSize(QSize(124, 38));
        detectionRectButton->setCheckable(true);
        detectionRectButton->setChecked(true);

        horizontalLayout_detectionAreaEdit->addWidget(detectionRectButton);


        verticalLayout_detectionArea->addLayout(horizontalLayout_detectionAreaEdit);

        horizontalLayout_detectionMask = new QHBoxLayout();
        horizontalLayout_detectionMask->setObjectName(QString::fromUtf8("horizontalLayout_detectionMask"));
        horizontalLayout_detectionMask->setContentsMargins(0, 0, 0, 0);
        detectionMaskLabel = new QLabel(detectionAreaCard);
        detectionMaskLabel->setObjectName(QString::fromUtf8("detectionMaskLabel"));
        detectionMaskLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_detectionMask->addWidget(detectionMaskLabel);

        detectionMaskEditButton = new QPushButton(detectionAreaCard);
        detectionMaskEditButton->setObjectName(QString::fromUtf8("detectionMaskEditButton"));
        detectionMaskEditButton->setMinimumSize(QSize(260, 38));

        horizontalLayout_detectionMask->addWidget(detectionMaskEditButton);


        verticalLayout_detectionArea->addLayout(horizontalLayout_detectionMask);

        horizontalLayout_positionEnable = new QHBoxLayout();
        horizontalLayout_positionEnable->setObjectName(QString::fromUtf8("horizontalLayout_positionEnable"));
        horizontalLayout_positionEnable->setContentsMargins(0, 0, 0, 0);
        positionEnableLabel = new QLabel(detectionAreaCard);
        positionEnableLabel->setObjectName(QString::fromUtf8("positionEnableLabel"));
        positionEnableLabel->setMinimumSize(QSize(260, 0));

        horizontalLayout_positionEnable->addWidget(positionEnableLabel);

        horizontalSpacer_positionEnable = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_positionEnable->addItem(horizontalSpacer_positionEnable);

        positionCorrectionSwitch = new QCheckBox(detectionAreaCard);
        positionCorrectionSwitch->setObjectName(QString::fromUtf8("positionCorrectionSwitch"));
        positionCorrectionSwitch->setChecked(true);

        horizontalLayout_positionEnable->addWidget(positionCorrectionSwitch);


        verticalLayout_detectionArea->addLayout(horizontalLayout_positionEnable);

        horizontalLayout_positionSource = new QHBoxLayout();
        horizontalLayout_positionSource->setObjectName(QString::fromUtf8("horizontalLayout_positionSource"));
        horizontalLayout_positionSource->setContentsMargins(0, 0, 0, 0);
        positionSourceLabel = new QLabel(detectionAreaCard);
        positionSourceLabel->setObjectName(QString::fromUtf8("positionSourceLabel"));
        positionSourceLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_positionSource->addWidget(positionSourceLabel);

        positionCorrectionComboBox = new QComboBox(detectionAreaCard);
        positionCorrectionComboBox->addItem(QString());
        positionCorrectionComboBox->setObjectName(QString::fromUtf8("positionCorrectionComboBox"));
        positionCorrectionComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_positionSource->addWidget(positionCorrectionComboBox);


        verticalLayout_detectionArea->addLayout(horizontalLayout_positionSource);


        verticalLayout_allConfigCards->addWidget(detectionAreaCard);

        recognitionSettingsCard = new QFrame(edgeAllParamsContents);
        recognitionSettingsCard->setObjectName(QString::fromUtf8("recognitionSettingsCard"));
        recognitionSettingsCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_recognitionSettings = new QVBoxLayout(recognitionSettingsCard);
        verticalLayout_recognitionSettings->setSpacing(16);
        verticalLayout_recognitionSettings->setObjectName(QString::fromUtf8("verticalLayout_recognitionSettings"));
        verticalLayout_recognitionSettings->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_recognitionHeader = new QHBoxLayout();
        horizontalLayout_recognitionHeader->setObjectName(QString::fromUtf8("horizontalLayout_recognitionHeader"));
        horizontalLayout_recognitionHeader->setContentsMargins(0, 0, 0, 0);
        recognitionSettingsTitleLabel = new QLabel(recognitionSettingsCard);
        recognitionSettingsTitleLabel->setObjectName(QString::fromUtf8("recognitionSettingsTitleLabel"));

        horizontalLayout_recognitionHeader->addWidget(recognitionSettingsTitleLabel);

        horizontalLayout_recognitionHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_recognitionHeader->addItem(horizontalLayout_recognitionHeaderSpacer);

        recognitionCollapseButton = new QToolButton(recognitionSettingsCard);
        recognitionCollapseButton->setObjectName(QString::fromUtf8("recognitionCollapseButton"));

        horizontalLayout_recognitionHeader->addWidget(recognitionCollapseButton);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_recognitionHeader);

        horizontalLayout_sensitivity = new QHBoxLayout();
        horizontalLayout_sensitivity->setObjectName(QString::fromUtf8("horizontalLayout_sensitivity"));
        horizontalLayout_sensitivity->setContentsMargins(0, 0, 0, 0);
        sensitivityLabel = new QLabel(recognitionSettingsCard);
        sensitivityLabel->setObjectName(QString::fromUtf8("sensitivityLabel"));
        sensitivityLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_sensitivity->addWidget(sensitivityLabel);

        edgeSensitivitySpinBox = new QSpinBox(recognitionSettingsCard);
        edgeSensitivitySpinBox->setObjectName(QString::fromUtf8("edgeSensitivitySpinBox"));
        edgeSensitivitySpinBox->setMinimumSize(QSize(260, 38));
        edgeSensitivitySpinBox->setMinimum(0);
        edgeSensitivitySpinBox->setMaximum(100);
        edgeSensitivitySpinBox->setValue(60);

        horizontalLayout_sensitivity->addWidget(edgeSensitivitySpinBox);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_sensitivity);

        horizontalLayout_edgePolarity = new QHBoxLayout();
        horizontalLayout_edgePolarity->setObjectName(QString::fromUtf8("horizontalLayout_edgePolarity"));
        horizontalLayout_edgePolarity->setContentsMargins(0, 0, 0, 0);
        edgePolarityLabel = new QLabel(recognitionSettingsCard);
        edgePolarityLabel->setObjectName(QString::fromUtf8("edgePolarityLabel"));
        edgePolarityLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_edgePolarity->addWidget(edgePolarityLabel);

        edgePolarityComboBox = new QComboBox(recognitionSettingsCard);
        edgePolarityComboBox->addItem(QString());
        edgePolarityComboBox->addItem(QString());
        edgePolarityComboBox->addItem(QString());
        edgePolarityComboBox->setObjectName(QString::fromUtf8("edgePolarityComboBox"));
        edgePolarityComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_edgePolarity->addWidget(edgePolarityComboBox);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_edgePolarity);


        verticalLayout_allConfigCards->addWidget(recognitionSettingsCard);

        resultJudgeCard = new QFrame(edgeAllParamsContents);
        resultJudgeCard->setObjectName(QString::fromUtf8("resultJudgeCard"));
        resultJudgeCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_resultJudge = new QVBoxLayout(resultJudgeCard);
        verticalLayout_resultJudge->setSpacing(16);
        verticalLayout_resultJudge->setObjectName(QString::fromUtf8("verticalLayout_resultJudge"));
        verticalLayout_resultJudge->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_resultHeader = new QHBoxLayout();
        horizontalLayout_resultHeader->setObjectName(QString::fromUtf8("horizontalLayout_resultHeader"));
        horizontalLayout_resultHeader->setContentsMargins(0, 0, 0, 0);
        resultJudgeTitleLabel = new QLabel(resultJudgeCard);
        resultJudgeTitleLabel->setObjectName(QString::fromUtf8("resultJudgeTitleLabel"));

        horizontalLayout_resultHeader->addWidget(resultJudgeTitleLabel);

        horizontalLayout_resultHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_resultHeader->addItem(horizontalLayout_resultHeaderSpacer);

        resultPresenceFrame = new QFrame(resultJudgeCard);
        resultPresenceFrame->setObjectName(QString::fromUtf8("resultPresenceFrame"));
        resultPresenceFrame->setMinimumSize(QSize(260, 38));
        resultPresenceFrame->setMaximumSize(QSize(260, 38));
        horizontalLayout_resultPresenceButtons = new QHBoxLayout(resultPresenceFrame);
        horizontalLayout_resultPresenceButtons->setSpacing(0);
        horizontalLayout_resultPresenceButtons->setObjectName(QString::fromUtf8("horizontalLayout_resultPresenceButtons"));
        horizontalLayout_resultPresenceButtons->setContentsMargins(0, 0, 0, 0);
        presentOkButton = new QPushButton(resultPresenceFrame);
        presentOkButton->setObjectName(QString::fromUtf8("presentOkButton"));
        presentOkButton->setCheckable(true);
        presentOkButton->setChecked(true);

        horizontalLayout_resultPresenceButtons->addWidget(presentOkButton);

        absentOkButton = new QPushButton(resultPresenceFrame);
        absentOkButton->setObjectName(QString::fromUtf8("absentOkButton"));
        absentOkButton->setCheckable(true);

        horizontalLayout_resultPresenceButtons->addWidget(absentOkButton);


        horizontalLayout_resultHeader->addWidget(resultPresenceFrame);


        verticalLayout_resultJudge->addLayout(horizontalLayout_resultHeader);


        verticalLayout_allConfigCards->addWidget(resultJudgeCard);

        verticalSpacer_configCards = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_allConfigCards->addItem(verticalSpacer_configCards);

        edgeAllParamsScrollArea->setWidget(edgeAllParamsContents);

        verticalLayout_allParamsPage->addWidget(edgeAllParamsScrollArea);

        edgeParamsStackedWidget->addWidget(allParamsPage);

        verticalLayout_editor->addWidget(edgeParamsStackedWidget);

        horizontalLayout_actions = new QHBoxLayout();
        horizontalLayout_actions->setObjectName(QString::fromUtf8("horizontalLayout_actions"));
        horizontalSpacer_actions = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_actions->addItem(horizontalSpacer_actions);

        referenceTestButton = new QPushButton(setupEditorPanel);
        referenceTestButton->setObjectName(QString::fromUtf8("referenceTestButton"));
        referenceTestButton->setMinimumSize(QSize(120, 48));

        horizontalLayout_actions->addWidget(referenceTestButton);

        testRunButton = new QPushButton(setupEditorPanel);
        testRunButton->setObjectName(QString::fromUtf8("testRunButton"));
        testRunButton->setMinimumSize(QSize(120, 48));

        horizontalLayout_actions->addWidget(testRunButton);

        finishButton = new QPushButton(setupEditorPanel);
        finishButton->setObjectName(QString::fromUtf8("finishButton"));
        finishButton->setMinimumSize(QSize(120, 48));

        horizontalLayout_actions->addWidget(finishButton);


        verticalLayout_editor->addLayout(horizontalLayout_actions);


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
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/grid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerGridButton->setIcon(icon4);
        viewerGridButton->setIconSize(QSize(24, 24));
        viewerGridButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerGridButton);

        viewerZoomSearchButton = new QToolButton(viewerHeader);
        viewerZoomSearchButton->setObjectName(QString::fromUtf8("viewerZoomSearchButton"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/zoom.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerZoomSearchButton->setIcon(icon5);
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
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/icons/fit.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerZoomInButton->setIcon(icon6);
        viewerZoomInButton->setIconSize(QSize(24, 24));
        viewerZoomInButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerZoomInButton);

        viewerFullButton = new QToolButton(viewerHeader);
        viewerFullButton->setObjectName(QString::fromUtf8("viewerFullButton"));
        QIcon icon7;
        icon7.addFile(QString::fromUtf8(":/icons/fullscreen.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerFullButton->setIcon(icon7);
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


        retranslateUi(EdgePresenceDialog);

        edgeParamsStackedWidget->setCurrentIndex(1);
        edgePolarityComboBox->setCurrentIndex(2);


        QMetaObject::connectSlotsByName(EdgePresenceDialog);
    } // setupUi

    void retranslateUi(QDialog *EdgePresenceDialog)
    {
        EdgePresenceDialog->setWindowTitle(QCoreApplication::translate("EdgePresenceDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \350\276\271\347\274\230\346\234\211\346\227\240", nullptr));
        setupWindowTitleLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\303\227", nullptr));
        setupPageCodeLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "428", nullptr));
        setupSaveButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\344\277\235\345\255\230", nullptr));
        setupSaveAsButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\345\217\246\345\255\230\344\270\272", nullptr));
        setupExportButton->setText(QCoreApplication::translate("EdgePresenceDialog", "IO\350\276\223\345\207\272", nullptr));
        editorTitleLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\350\276\271\347\274\230\346\234\211\346\227\240", nullptr));
        basicSegmentButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\345\237\272\347\241\200", nullptr));
        allSegmentButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\345\205\250\351\203\250", nullptr));
        basicDetectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "configCard", nullptr)));
        basicDetectionAreaTitleLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        basicDetectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "cardTitle", nullptr)));
        basicDetectionCollapseButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\342\214\204", nullptr));
        basicDetectionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "collapseCard", nullptr)));
        basicDetectionAreaLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\346\243\200\346\265\213\345\214\272 \342\223\230", nullptr));
        basicDetectionAreaLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "rowField", nullptr)));
#if QT_CONFIG(tooltip)
        basicDetectionRectButton->setToolTip(QCoreApplication::translate("EdgePresenceDialog", "\347\202\271\345\207\273\345\220\216\345\234\250\345\217\263\344\276\247\347\273\230\345\210\266\347\272\277\345\236\213\346\220\234\347\264\242 ROI", nullptr));
#endif // QT_CONFIG(tooltip)
        basicDetectionRectButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\347\272\277\345\236\213 ROI", nullptr));
        basicDetectionRectButton->setProperty("actionRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "toolbarIcon", nullptr)));
        basicPositionEnableLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243\344\275\277\350\203\275 \342\223\230", nullptr));
        basicPositionEnableLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "rowField", nullptr)));
        basicPositionCorrectionSwitch->setText(QString());
        basicPositionSourceLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        basicPositionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "rowField", nullptr)));
        basicPositionCorrectionComboBox->setItemText(0, QCoreApplication::translate("EdgePresenceDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        basicRecognitionSettingsCard->setProperty("panelRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "configCard", nullptr)));
        basicRecognitionSettingsTitleLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\350\257\206\345\210\253\350\256\276\347\275\256", nullptr));
        basicRecognitionSettingsTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "cardTitle", nullptr)));
        basicRecognitionCollapseButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\342\214\204", nullptr));
        basicRecognitionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "collapseCard", nullptr)));
        basicSensitivityLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\347\201\265\346\225\217\345\272\246", nullptr));
        basicSensitivityLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "rowField", nullptr)));
        basicResultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "configCard", nullptr)));
        basicResultJudgeTitleLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        basicResultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "cardTitle", nullptr)));
        basicResultPresenceFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "optionGroup", nullptr)));
        basicPresentOkButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\345\255\230\345\234\250OK", nullptr));
        basicAbsentOkButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\344\270\215\345\255\230\345\234\250OK", nullptr));
        detectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "configCard", nullptr)));
        detectionAreaTitleLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        detectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "cardTitle", nullptr)));
        detectionCollapseButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\342\214\204", nullptr));
        detectionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "collapseCard", nullptr)));
        detectionAreaLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\346\243\200\346\265\213\345\214\272 \342\223\230", nullptr));
        detectionAreaLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "rowField", nullptr)));
#if QT_CONFIG(tooltip)
        detectionRectButton->setToolTip(QCoreApplication::translate("EdgePresenceDialog", "\347\202\271\345\207\273\345\220\216\345\234\250\345\217\263\344\276\247\347\273\230\345\210\266\347\272\277\345\236\213\346\220\234\347\264\242 ROI", nullptr));
#endif // QT_CONFIG(tooltip)
        detectionRectButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\347\272\277\345\236\213 ROI", nullptr));
        detectionRectButton->setProperty("actionRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "toolbarIcon", nullptr)));
        detectionMaskLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\345\261\217\350\224\275\345\214\272\345\237\237", nullptr));
        detectionMaskLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "rowField", nullptr)));
        detectionMaskEditButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\347\274\226\350\276\221", nullptr));
        detectionMaskEditButton->setProperty("actionRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "gray", nullptr)));
        positionEnableLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243\344\275\277\350\203\275 \342\223\230", nullptr));
        positionEnableLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "rowField", nullptr)));
        positionCorrectionSwitch->setText(QString());
        positionSourceLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        positionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "rowField", nullptr)));
        positionCorrectionComboBox->setItemText(0, QCoreApplication::translate("EdgePresenceDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        recognitionSettingsCard->setProperty("panelRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "configCard", nullptr)));
        recognitionSettingsTitleLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\350\257\206\345\210\253\350\256\276\347\275\256", nullptr));
        recognitionSettingsTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "cardTitle", nullptr)));
        recognitionCollapseButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\342\214\204", nullptr));
        recognitionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "collapseCard", nullptr)));
        sensitivityLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\347\201\265\346\225\217\345\272\246", nullptr));
        sensitivityLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "rowField", nullptr)));
        edgePolarityLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\350\276\271\347\274\230\346\236\201\346\200\247", nullptr));
        edgePolarityLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "rowField", nullptr)));
        edgePolarityComboBox->setItemText(0, QCoreApplication::translate("EdgePresenceDialog", "\351\273\221\345\210\260\347\231\275", nullptr));
        edgePolarityComboBox->setItemText(1, QCoreApplication::translate("EdgePresenceDialog", "\347\231\275\345\210\260\351\273\221", nullptr));
        edgePolarityComboBox->setItemText(2, QCoreApplication::translate("EdgePresenceDialog", "\344\273\273\346\204\217", nullptr));

        resultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "configCard", nullptr)));
        resultJudgeTitleLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        resultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("EdgePresenceDialog", "cardTitle", nullptr)));
        resultPresenceFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "optionGroup", nullptr)));
        presentOkButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\345\255\230\345\234\250OK", nullptr));
        absentOkButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\344\270\215\345\255\230\345\234\250OK", nullptr));
        referenceTestButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\345\237\272\345\207\206\345\233\276\346\265\213\350\257\225", nullptr));
        referenceTestButton->setProperty("actionRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "secondary", nullptr)));
        testRunButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\346\265\213\350\257\225\350\277\220\350\241\214", nullptr));
        testRunButton->setProperty("actionRole", QVariant(QCoreApplication::translate("EdgePresenceDialog", "secondary", nullptr)));
        finishButton->setText(QCoreApplication::translate("EdgePresenceDialog", "\345\256\214\346\210\220", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerZoomOutButton->setText(QCoreApplication::translate("EdgePresenceDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "39%", nullptr));
        viewerStatusLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: --ms   \345\267\245\345\205\267\350\200\227\346\227\266: --ms", nullptr));
        viewerCursorLabel->setText(QCoreApplication::translate("EdgePresenceDialog", "X: --  Y: ---   |   R: -- G: -- B: --", nullptr));
    } // retranslateUi

};

namespace Ui {
    class EdgePresenceDialog: public Ui_EdgePresenceDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_EDGEPRESENCEDIALOG_H
