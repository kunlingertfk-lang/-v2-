/********************************************************************************
** Form generated from reading UI file 'BlobPresenceDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_BLOBPRESENCEDIALOG_H
#define UI_BLOBPRESENCEDIALOG_H

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

class Ui_BlobPresenceDialog
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
    QToolButton *spotExternalEditButton;
    QSpacerItem *horizontalSpacer_editorHeader;
    QPushButton *blobPcImportButton;
    QFrame *segmentFrame;
    QHBoxLayout *horizontalLayout_segment;
    QPushButton *basicSegmentButton;
    QPushButton *allSegmentButton;
    QStackedWidget *spotParamsStackedWidget;
    QWidget *basicParamsPage;
    QVBoxLayout *verticalLayout_basicParamsPage;
    QScrollArea *spotBasicParamsScrollArea;
    QWidget *spotBasicParamsContents;
    QVBoxLayout *verticalLayout_basicConfigCards;
    QFrame *basicDetectionAreaCard;
    QVBoxLayout *basicVerticalLayout_detectionArea;
    QHBoxLayout *basicHorizontalLayout_detectionHeader;
    QLabel *basicDetectionAreaTitleLabel;
    QSpacerItem *basicHorizontalLayout_detectionHeaderSpacer;
    QToolButton *basicDetectionCollapseButton;
    QHBoxLayout *basicHorizontalLayout_detectionAreaTools;
    QLabel *basicDetectionAreaLabel;
    QToolButton *basicDetectionDrawButton;
    QToolButton *basicDetectionRectButton;
    QToolButton *basicDetectionCircleButton;
    QToolButton *basicDetectionPolygonButton;
    QToolButton *basicDetectionResetButton;
    QHBoxLayout *basicHorizontalLayout_positionEnable;
    QLabel *basicPositionEnableLabel;
    QSpacerItem *basicHorizontalSpacer_positionEnable;
    QCheckBox *basicPositionCorrectionSwitch;
    QHBoxLayout *basicHorizontalLayout_positionSource;
    QLabel *basicPositionSourceLabel;
    QComboBox *basicPositionCorrectionComboBox;
    QFrame *basicGrayRecognitionSettingsCard;
    QVBoxLayout *basicVerticalLayout_grayRecognitionSettings;
    QHBoxLayout *basicHorizontalLayout_grayHeader;
    QLabel *basicGrayRecognitionSettingsTitleLabel;
    QSpacerItem *basicHorizontalLayout_grayHeaderSpacer;
    QToolButton *basicGrayCollapseButton;
    QHBoxLayout *basicHorizontalLayout_grayThreshold;
    QLabel *basicGrayThresholdLabel;
    QSpacerItem *horizontalSpacer_3;
    QSpinBox *basicMinGraySpinBox;
    QLabel *basicHorizontalLayout_grayThresholdDashLabel;
    QSpinBox *basicMaxGraySpinBox;
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
    QScrollArea *spotAllParamsScrollArea;
    QWidget *spotAllParamsContents;
    QVBoxLayout *verticalLayout_allConfigCards;
    QFrame *detectionAreaCard;
    QVBoxLayout *verticalLayout_detectionArea;
    QHBoxLayout *horizontalLayout_detectionHeader;
    QLabel *detectionAreaTitleLabel;
    QSpacerItem *horizontalLayout_detectionHeaderSpacer;
    QToolButton *detectionCollapseButton;
    QHBoxLayout *horizontalLayout_detectionAreaTools;
    QLabel *detectionAreaLabel;
    QToolButton *detectionDrawButton;
    QToolButton *detectionRectButton;
    QToolButton *detectionCircleButton;
    QToolButton *detectionPolygonButton;
    QToolButton *detectionResetButton;
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
    QFrame *grayRecognitionSettingsCard;
    QVBoxLayout *verticalLayout_grayRecognitionSettings;
    QHBoxLayout *horizontalLayout_grayHeader;
    QLabel *grayRecognitionSettingsTitleLabel;
    QSpacerItem *horizontalLayout_grayHeaderSpacer;
    QToolButton *grayCollapseButton;
    QHBoxLayout *horizontalLayout_grayThreshold;
    QLabel *grayThresholdLabel;
    QSpacerItem *horizontalSpacer;
    QSpinBox *minGraySpinBox;
    QLabel *horizontalLayout_grayThresholdDashLabel;
    QSpinBox *maxGraySpinBox;
    QHBoxLayout *horizontalLayout_invertRange;
    QLabel *invertRangeLabel;
    QSpacerItem *horizontalSpacer_invertRange;
    QCheckBox *invertRangeSwitch;
    QHBoxLayout *horizontalLayout_areaRange;
    QLabel *areaRangeLabel;
    QSpacerItem *horizontalSpacer_2;
    QSpinBox *minAreaSpinBox;
    QLabel *horizontalLayout_areaRangeDashLabel;
    QSpinBox *maxAreaSpinBox;
    QHBoxLayout *horizontalLayout_maskOutput;
    QLabel *maskOutputLabel;
    QSpacerItem *horizontalSpacer_maskOutput;
    QCheckBox *maskOutputSwitch;
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

    void setupUi(QDialog *BlobPresenceDialog)
    {
        if (BlobPresenceDialog->objectName().isEmpty())
            BlobPresenceDialog->setObjectName(QString::fromUtf8("BlobPresenceDialog"));
        BlobPresenceDialog->resize(1760, 980);
        verticalLayout_root = new QVBoxLayout(BlobPresenceDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        setupTopBar = new QFrame(BlobPresenceDialog);
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

        setupToolbar = new QFrame(BlobPresenceDialog);
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

        setupBodyFrame = new QFrame(BlobPresenceDialog);
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

        spotExternalEditButton = new QToolButton(setupEditorPanel);
        spotExternalEditButton->setObjectName(QString::fromUtf8("spotExternalEditButton"));
        spotExternalEditButton->setIcon(icon);
        spotExternalEditButton->setIconSize(QSize(24, 24));
        spotExternalEditButton->setAutoRaise(true);

        horizontalLayout_editorHeader->addWidget(spotExternalEditButton);

        horizontalSpacer_editorHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_editorHeader->addItem(horizontalSpacer_editorHeader);

        blobPcImportButton = new QPushButton(setupEditorPanel);
        blobPcImportButton->setObjectName(QString::fromUtf8("blobPcImportButton"));
        blobPcImportButton->setMinimumSize(QSize(118, 42));
        blobPcImportButton->setProperty("optionalEntry", QVariant(true));

        horizontalLayout_editorHeader->addWidget(blobPcImportButton);

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

        spotParamsStackedWidget = new QStackedWidget(setupEditorPanel);
        spotParamsStackedWidget->setObjectName(QString::fromUtf8("spotParamsStackedWidget"));
        basicParamsPage = new QWidget();
        basicParamsPage->setObjectName(QString::fromUtf8("basicParamsPage"));
        verticalLayout_basicParamsPage = new QVBoxLayout(basicParamsPage);
        verticalLayout_basicParamsPage->setSpacing(0);
        verticalLayout_basicParamsPage->setObjectName(QString::fromUtf8("verticalLayout_basicParamsPage"));
        verticalLayout_basicParamsPage->setContentsMargins(0, 0, 0, 0);
        spotBasicParamsScrollArea = new QScrollArea(basicParamsPage);
        spotBasicParamsScrollArea->setObjectName(QString::fromUtf8("spotBasicParamsScrollArea"));
        spotBasicParamsScrollArea->setFrameShape(QFrame::NoFrame);
        spotBasicParamsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        spotBasicParamsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        spotBasicParamsScrollArea->setWidgetResizable(true);
        spotBasicParamsContents = new QWidget();
        spotBasicParamsContents->setObjectName(QString::fromUtf8("spotBasicParamsContents"));
        spotBasicParamsContents->setGeometry(QRect(0, 0, 554, 692));
        verticalLayout_basicConfigCards = new QVBoxLayout(spotBasicParamsContents);
        verticalLayout_basicConfigCards->setSpacing(12);
        verticalLayout_basicConfigCards->setObjectName(QString::fromUtf8("verticalLayout_basicConfigCards"));
        verticalLayout_basicConfigCards->setContentsMargins(0, 0, 0, 0);
        basicDetectionAreaCard = new QFrame(spotBasicParamsContents);
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

        basicHorizontalLayout_detectionAreaTools = new QHBoxLayout();
        basicHorizontalLayout_detectionAreaTools->setSpacing(0);
        basicHorizontalLayout_detectionAreaTools->setObjectName(QString::fromUtf8("basicHorizontalLayout_detectionAreaTools"));
        basicHorizontalLayout_detectionAreaTools->setContentsMargins(0, 0, 0, 0);
        basicDetectionAreaLabel = new QLabel(basicDetectionAreaCard);
        basicDetectionAreaLabel->setObjectName(QString::fromUtf8("basicDetectionAreaLabel"));
        basicDetectionAreaLabel->setMinimumSize(QSize(150, 0));

        basicHorizontalLayout_detectionAreaTools->addWidget(basicDetectionAreaLabel);

        basicDetectionDrawButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionDrawButton->setObjectName(QString::fromUtf8("basicDetectionDrawButton"));
        basicDetectionDrawButton->setMinimumSize(QSize(52, 38));
        basicDetectionDrawButton->setMaximumSize(QSize(52, 38));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/fit.svg"), QSize(), QIcon::Normal, QIcon::Off);
        basicDetectionDrawButton->setIcon(icon4);
        basicDetectionDrawButton->setIconSize(QSize(22, 22));
        basicDetectionDrawButton->setCheckable(true);
        basicDetectionDrawButton->setChecked(true);

        basicHorizontalLayout_detectionAreaTools->addWidget(basicDetectionDrawButton);

        basicDetectionRectButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionRectButton->setObjectName(QString::fromUtf8("basicDetectionRectButton"));
        basicDetectionRectButton->setMinimumSize(QSize(52, 38));
        basicDetectionRectButton->setMaximumSize(QSize(52, 38));
        basicDetectionRectButton->setCheckable(true);

        basicHorizontalLayout_detectionAreaTools->addWidget(basicDetectionRectButton);

        basicDetectionCircleButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionCircleButton->setObjectName(QString::fromUtf8("basicDetectionCircleButton"));
        basicDetectionCircleButton->setMinimumSize(QSize(52, 38));
        basicDetectionCircleButton->setMaximumSize(QSize(52, 38));
        basicDetectionCircleButton->setCheckable(true);

        basicHorizontalLayout_detectionAreaTools->addWidget(basicDetectionCircleButton);

        basicDetectionPolygonButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionPolygonButton->setObjectName(QString::fromUtf8("basicDetectionPolygonButton"));
        basicDetectionPolygonButton->setMinimumSize(QSize(52, 38));
        basicDetectionPolygonButton->setMaximumSize(QSize(52, 38));
        basicDetectionPolygonButton->setCheckable(true);

        basicHorizontalLayout_detectionAreaTools->addWidget(basicDetectionPolygonButton);

        basicDetectionResetButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionResetButton->setObjectName(QString::fromUtf8("basicDetectionResetButton"));
        basicDetectionResetButton->setMinimumSize(QSize(52, 38));
        basicDetectionResetButton->setMaximumSize(QSize(52, 38));
        basicDetectionResetButton->setCheckable(true);

        basicHorizontalLayout_detectionAreaTools->addWidget(basicDetectionResetButton);


        basicVerticalLayout_detectionArea->addLayout(basicHorizontalLayout_detectionAreaTools);

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

        basicGrayRecognitionSettingsCard = new QFrame(spotBasicParamsContents);
        basicGrayRecognitionSettingsCard->setObjectName(QString::fromUtf8("basicGrayRecognitionSettingsCard"));
        basicGrayRecognitionSettingsCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_grayRecognitionSettings = new QVBoxLayout(basicGrayRecognitionSettingsCard);
        basicVerticalLayout_grayRecognitionSettings->setSpacing(16);
        basicVerticalLayout_grayRecognitionSettings->setObjectName(QString::fromUtf8("basicVerticalLayout_grayRecognitionSettings"));
        basicVerticalLayout_grayRecognitionSettings->setContentsMargins(20, 18, 20, 18);
        basicHorizontalLayout_grayHeader = new QHBoxLayout();
        basicHorizontalLayout_grayHeader->setObjectName(QString::fromUtf8("basicHorizontalLayout_grayHeader"));
        basicHorizontalLayout_grayHeader->setContentsMargins(0, 0, 0, 0);
        basicGrayRecognitionSettingsTitleLabel = new QLabel(basicGrayRecognitionSettingsCard);
        basicGrayRecognitionSettingsTitleLabel->setObjectName(QString::fromUtf8("basicGrayRecognitionSettingsTitleLabel"));

        basicHorizontalLayout_grayHeader->addWidget(basicGrayRecognitionSettingsTitleLabel);

        basicHorizontalLayout_grayHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_grayHeader->addItem(basicHorizontalLayout_grayHeaderSpacer);

        basicGrayCollapseButton = new QToolButton(basicGrayRecognitionSettingsCard);
        basicGrayCollapseButton->setObjectName(QString::fromUtf8("basicGrayCollapseButton"));

        basicHorizontalLayout_grayHeader->addWidget(basicGrayCollapseButton);


        basicVerticalLayout_grayRecognitionSettings->addLayout(basicHorizontalLayout_grayHeader);

        basicHorizontalLayout_grayThreshold = new QHBoxLayout();
        basicHorizontalLayout_grayThreshold->setObjectName(QString::fromUtf8("basicHorizontalLayout_grayThreshold"));
        basicHorizontalLayout_grayThreshold->setContentsMargins(0, 0, 0, 0);
        basicGrayThresholdLabel = new QLabel(basicGrayRecognitionSettingsCard);
        basicGrayThresholdLabel->setObjectName(QString::fromUtf8("basicGrayThresholdLabel"));
        basicGrayThresholdLabel->setMinimumSize(QSize(150, 0));

        basicHorizontalLayout_grayThreshold->addWidget(basicGrayThresholdLabel);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_grayThreshold->addItem(horizontalSpacer_3);

        basicMinGraySpinBox = new QSpinBox(basicGrayRecognitionSettingsCard);
        basicMinGraySpinBox->setObjectName(QString::fromUtf8("basicMinGraySpinBox"));
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(basicMinGraySpinBox->sizePolicy().hasHeightForWidth());
        basicMinGraySpinBox->setSizePolicy(sizePolicy);
        basicMinGraySpinBox->setMinimumSize(QSize(121, 38));
        basicMinGraySpinBox->setMaximumSize(QSize(121, 38));
        basicMinGraySpinBox->setMinimum(1);
        basicMinGraySpinBox->setMaximum(255);
        basicMinGraySpinBox->setValue(80);

        basicHorizontalLayout_grayThreshold->addWidget(basicMinGraySpinBox);

        basicHorizontalLayout_grayThresholdDashLabel = new QLabel(basicGrayRecognitionSettingsCard);
        basicHorizontalLayout_grayThresholdDashLabel->setObjectName(QString::fromUtf8("basicHorizontalLayout_grayThresholdDashLabel"));
        basicHorizontalLayout_grayThresholdDashLabel->setAlignment(Qt::AlignCenter);

        basicHorizontalLayout_grayThreshold->addWidget(basicHorizontalLayout_grayThresholdDashLabel);

        basicMaxGraySpinBox = new QSpinBox(basicGrayRecognitionSettingsCard);
        basicMaxGraySpinBox->setObjectName(QString::fromUtf8("basicMaxGraySpinBox"));
        sizePolicy.setHeightForWidth(basicMaxGraySpinBox->sizePolicy().hasHeightForWidth());
        basicMaxGraySpinBox->setSizePolicy(sizePolicy);
        basicMaxGraySpinBox->setMinimumSize(QSize(121, 38));
        basicMaxGraySpinBox->setMaximumSize(QSize(121, 38));
        basicMaxGraySpinBox->setMinimum(0);
        basicMaxGraySpinBox->setMaximum(255);
        basicMaxGraySpinBox->setValue(120);

        basicHorizontalLayout_grayThreshold->addWidget(basicMaxGraySpinBox);


        basicVerticalLayout_grayRecognitionSettings->addLayout(basicHorizontalLayout_grayThreshold);


        verticalLayout_basicConfigCards->addWidget(basicGrayRecognitionSettingsCard);

        basicResultJudgeCard = new QFrame(spotBasicParamsContents);
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

        spotBasicParamsScrollArea->setWidget(spotBasicParamsContents);

        verticalLayout_basicParamsPage->addWidget(spotBasicParamsScrollArea);

        spotParamsStackedWidget->addWidget(basicParamsPage);
        allParamsPage = new QWidget();
        allParamsPage->setObjectName(QString::fromUtf8("allParamsPage"));
        verticalLayout_allParamsPage = new QVBoxLayout(allParamsPage);
        verticalLayout_allParamsPage->setSpacing(0);
        verticalLayout_allParamsPage->setObjectName(QString::fromUtf8("verticalLayout_allParamsPage"));
        verticalLayout_allParamsPage->setContentsMargins(0, 0, 0, 0);
        spotAllParamsScrollArea = new QScrollArea(allParamsPage);
        spotAllParamsScrollArea->setObjectName(QString::fromUtf8("spotAllParamsScrollArea"));
        spotAllParamsScrollArea->setFrameShape(QFrame::NoFrame);
        spotAllParamsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        spotAllParamsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        spotAllParamsScrollArea->setWidgetResizable(true);
        spotAllParamsContents = new QWidget();
        spotAllParamsContents->setObjectName(QString::fromUtf8("spotAllParamsContents"));
        spotAllParamsContents->setGeometry(QRect(0, 0, 554, 692));
        verticalLayout_allConfigCards = new QVBoxLayout(spotAllParamsContents);
        verticalLayout_allConfigCards->setSpacing(12);
        verticalLayout_allConfigCards->setObjectName(QString::fromUtf8("verticalLayout_allConfigCards"));
        verticalLayout_allConfigCards->setContentsMargins(0, 0, 0, 0);
        detectionAreaCard = new QFrame(spotAllParamsContents);
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

        horizontalLayout_detectionAreaTools = new QHBoxLayout();
        horizontalLayout_detectionAreaTools->setSpacing(0);
        horizontalLayout_detectionAreaTools->setObjectName(QString::fromUtf8("horizontalLayout_detectionAreaTools"));
        horizontalLayout_detectionAreaTools->setContentsMargins(0, 0, 0, 0);
        detectionAreaLabel = new QLabel(detectionAreaCard);
        detectionAreaLabel->setObjectName(QString::fromUtf8("detectionAreaLabel"));
        detectionAreaLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_detectionAreaTools->addWidget(detectionAreaLabel);

        detectionDrawButton = new QToolButton(detectionAreaCard);
        detectionDrawButton->setObjectName(QString::fromUtf8("detectionDrawButton"));
        detectionDrawButton->setMinimumSize(QSize(52, 38));
        detectionDrawButton->setMaximumSize(QSize(52, 38));
        detectionDrawButton->setIcon(icon4);
        detectionDrawButton->setIconSize(QSize(22, 22));
        detectionDrawButton->setCheckable(true);
        detectionDrawButton->setChecked(true);

        horizontalLayout_detectionAreaTools->addWidget(detectionDrawButton);

        detectionRectButton = new QToolButton(detectionAreaCard);
        detectionRectButton->setObjectName(QString::fromUtf8("detectionRectButton"));
        detectionRectButton->setMinimumSize(QSize(52, 38));
        detectionRectButton->setMaximumSize(QSize(52, 38));
        detectionRectButton->setCheckable(true);

        horizontalLayout_detectionAreaTools->addWidget(detectionRectButton);

        detectionCircleButton = new QToolButton(detectionAreaCard);
        detectionCircleButton->setObjectName(QString::fromUtf8("detectionCircleButton"));
        detectionCircleButton->setMinimumSize(QSize(52, 38));
        detectionCircleButton->setMaximumSize(QSize(52, 38));
        detectionCircleButton->setCheckable(true);

        horizontalLayout_detectionAreaTools->addWidget(detectionCircleButton);

        detectionPolygonButton = new QToolButton(detectionAreaCard);
        detectionPolygonButton->setObjectName(QString::fromUtf8("detectionPolygonButton"));
        detectionPolygonButton->setMinimumSize(QSize(52, 38));
        detectionPolygonButton->setMaximumSize(QSize(52, 38));
        detectionPolygonButton->setCheckable(true);

        horizontalLayout_detectionAreaTools->addWidget(detectionPolygonButton);

        detectionResetButton = new QToolButton(detectionAreaCard);
        detectionResetButton->setObjectName(QString::fromUtf8("detectionResetButton"));
        detectionResetButton->setMinimumSize(QSize(52, 38));
        detectionResetButton->setMaximumSize(QSize(52, 38));
        detectionResetButton->setCheckable(true);

        horizontalLayout_detectionAreaTools->addWidget(detectionResetButton);


        verticalLayout_detectionArea->addLayout(horizontalLayout_detectionAreaTools);

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

        grayRecognitionSettingsCard = new QFrame(spotAllParamsContents);
        grayRecognitionSettingsCard->setObjectName(QString::fromUtf8("grayRecognitionSettingsCard"));
        grayRecognitionSettingsCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_grayRecognitionSettings = new QVBoxLayout(grayRecognitionSettingsCard);
        verticalLayout_grayRecognitionSettings->setSpacing(16);
        verticalLayout_grayRecognitionSettings->setObjectName(QString::fromUtf8("verticalLayout_grayRecognitionSettings"));
        verticalLayout_grayRecognitionSettings->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_grayHeader = new QHBoxLayout();
        horizontalLayout_grayHeader->setObjectName(QString::fromUtf8("horizontalLayout_grayHeader"));
        horizontalLayout_grayHeader->setContentsMargins(0, 0, 0, 0);
        grayRecognitionSettingsTitleLabel = new QLabel(grayRecognitionSettingsCard);
        grayRecognitionSettingsTitleLabel->setObjectName(QString::fromUtf8("grayRecognitionSettingsTitleLabel"));

        horizontalLayout_grayHeader->addWidget(grayRecognitionSettingsTitleLabel);

        horizontalLayout_grayHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_grayHeader->addItem(horizontalLayout_grayHeaderSpacer);

        grayCollapseButton = new QToolButton(grayRecognitionSettingsCard);
        grayCollapseButton->setObjectName(QString::fromUtf8("grayCollapseButton"));

        horizontalLayout_grayHeader->addWidget(grayCollapseButton);


        verticalLayout_grayRecognitionSettings->addLayout(horizontalLayout_grayHeader);

        horizontalLayout_grayThreshold = new QHBoxLayout();
        horizontalLayout_grayThreshold->setObjectName(QString::fromUtf8("horizontalLayout_grayThreshold"));
        horizontalLayout_grayThreshold->setContentsMargins(0, 0, 0, 0);
        grayThresholdLabel = new QLabel(grayRecognitionSettingsCard);
        grayThresholdLabel->setObjectName(QString::fromUtf8("grayThresholdLabel"));
        grayThresholdLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_grayThreshold->addWidget(grayThresholdLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_grayThreshold->addItem(horizontalSpacer);

        minGraySpinBox = new QSpinBox(grayRecognitionSettingsCard);
        minGraySpinBox->setObjectName(QString::fromUtf8("minGraySpinBox"));
        sizePolicy.setHeightForWidth(minGraySpinBox->sizePolicy().hasHeightForWidth());
        minGraySpinBox->setSizePolicy(sizePolicy);
        minGraySpinBox->setMinimumSize(QSize(121, 38));
        minGraySpinBox->setMaximumSize(QSize(121, 38));
        minGraySpinBox->setMinimum(1);
        minGraySpinBox->setMaximum(255);
        minGraySpinBox->setValue(80);

        horizontalLayout_grayThreshold->addWidget(minGraySpinBox);

        horizontalLayout_grayThresholdDashLabel = new QLabel(grayRecognitionSettingsCard);
        horizontalLayout_grayThresholdDashLabel->setObjectName(QString::fromUtf8("horizontalLayout_grayThresholdDashLabel"));
        horizontalLayout_grayThresholdDashLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout_grayThreshold->addWidget(horizontalLayout_grayThresholdDashLabel);

        maxGraySpinBox = new QSpinBox(grayRecognitionSettingsCard);
        maxGraySpinBox->setObjectName(QString::fromUtf8("maxGraySpinBox"));
        sizePolicy.setHeightForWidth(maxGraySpinBox->sizePolicy().hasHeightForWidth());
        maxGraySpinBox->setSizePolicy(sizePolicy);
        maxGraySpinBox->setMinimumSize(QSize(121, 38));
        maxGraySpinBox->setMaximumSize(QSize(121, 38));
        maxGraySpinBox->setMinimum(0);
        maxGraySpinBox->setMaximum(255);
        maxGraySpinBox->setValue(120);

        horizontalLayout_grayThreshold->addWidget(maxGraySpinBox);


        verticalLayout_grayRecognitionSettings->addLayout(horizontalLayout_grayThreshold);

        horizontalLayout_invertRange = new QHBoxLayout();
        horizontalLayout_invertRange->setObjectName(QString::fromUtf8("horizontalLayout_invertRange"));
        horizontalLayout_invertRange->setContentsMargins(0, 0, 0, 0);
        invertRangeLabel = new QLabel(grayRecognitionSettingsCard);
        invertRangeLabel->setObjectName(QString::fromUtf8("invertRangeLabel"));
        invertRangeLabel->setMinimumSize(QSize(260, 0));

        horizontalLayout_invertRange->addWidget(invertRangeLabel);

        horizontalSpacer_invertRange = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_invertRange->addItem(horizontalSpacer_invertRange);

        invertRangeSwitch = new QCheckBox(grayRecognitionSettingsCard);
        invertRangeSwitch->setObjectName(QString::fromUtf8("invertRangeSwitch"));

        horizontalLayout_invertRange->addWidget(invertRangeSwitch);


        verticalLayout_grayRecognitionSettings->addLayout(horizontalLayout_invertRange);

        horizontalLayout_areaRange = new QHBoxLayout();
        horizontalLayout_areaRange->setObjectName(QString::fromUtf8("horizontalLayout_areaRange"));
        horizontalLayout_areaRange->setContentsMargins(0, 0, 0, 0);
        areaRangeLabel = new QLabel(grayRecognitionSettingsCard);
        areaRangeLabel->setObjectName(QString::fromUtf8("areaRangeLabel"));
        areaRangeLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_areaRange->addWidget(areaRangeLabel);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_areaRange->addItem(horizontalSpacer_2);

        minAreaSpinBox = new QSpinBox(grayRecognitionSettingsCard);
        minAreaSpinBox->setObjectName(QString::fromUtf8("minAreaSpinBox"));
        sizePolicy.setHeightForWidth(minAreaSpinBox->sizePolicy().hasHeightForWidth());
        minAreaSpinBox->setSizePolicy(sizePolicy);
        minAreaSpinBox->setMinimumSize(QSize(121, 38));
        minAreaSpinBox->setMaximumSize(QSize(121, 38));
        minAreaSpinBox->setMinimum(1);
        minAreaSpinBox->setMaximum(99999999);
        minAreaSpinBox->setValue(1);

        horizontalLayout_areaRange->addWidget(minAreaSpinBox);

        horizontalLayout_areaRangeDashLabel = new QLabel(grayRecognitionSettingsCard);
        horizontalLayout_areaRangeDashLabel->setObjectName(QString::fromUtf8("horizontalLayout_areaRangeDashLabel"));
        horizontalLayout_areaRangeDashLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout_areaRange->addWidget(horizontalLayout_areaRangeDashLabel);

        maxAreaSpinBox = new QSpinBox(grayRecognitionSettingsCard);
        maxAreaSpinBox->setObjectName(QString::fromUtf8("maxAreaSpinBox"));
        sizePolicy.setHeightForWidth(maxAreaSpinBox->sizePolicy().hasHeightForWidth());
        maxAreaSpinBox->setSizePolicy(sizePolicy);
        maxAreaSpinBox->setMinimumSize(QSize(121, 38));
        maxAreaSpinBox->setMaximumSize(QSize(121, 38));
        maxAreaSpinBox->setMinimum(0);
        maxAreaSpinBox->setMaximum(99999999);
        maxAreaSpinBox->setValue(9000000);

        horizontalLayout_areaRange->addWidget(maxAreaSpinBox);


        verticalLayout_grayRecognitionSettings->addLayout(horizontalLayout_areaRange);

        horizontalLayout_maskOutput = new QHBoxLayout();
        horizontalLayout_maskOutput->setObjectName(QString::fromUtf8("horizontalLayout_maskOutput"));
        horizontalLayout_maskOutput->setContentsMargins(0, 0, 0, 0);
        maskOutputLabel = new QLabel(grayRecognitionSettingsCard);
        maskOutputLabel->setObjectName(QString::fromUtf8("maskOutputLabel"));
        maskOutputLabel->setMinimumSize(QSize(260, 0));

        horizontalLayout_maskOutput->addWidget(maskOutputLabel);

        horizontalSpacer_maskOutput = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_maskOutput->addItem(horizontalSpacer_maskOutput);

        maskOutputSwitch = new QCheckBox(grayRecognitionSettingsCard);
        maskOutputSwitch->setObjectName(QString::fromUtf8("maskOutputSwitch"));
        maskOutputSwitch->setChecked(true);

        horizontalLayout_maskOutput->addWidget(maskOutputSwitch);


        verticalLayout_grayRecognitionSettings->addLayout(horizontalLayout_maskOutput);


        verticalLayout_allConfigCards->addWidget(grayRecognitionSettingsCard);

        resultJudgeCard = new QFrame(spotAllParamsContents);
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

        spotAllParamsScrollArea->setWidget(spotAllParamsContents);

        verticalLayout_allParamsPage->addWidget(spotAllParamsScrollArea);

        spotParamsStackedWidget->addWidget(allParamsPage);

        verticalLayout_editor->addWidget(spotParamsStackedWidget);

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
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/grid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerGridButton->setIcon(icon5);
        viewerGridButton->setIconSize(QSize(24, 24));
        viewerGridButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerGridButton);

        viewerZoomSearchButton = new QToolButton(viewerHeader);
        viewerZoomSearchButton->setObjectName(QString::fromUtf8("viewerZoomSearchButton"));
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/icons/zoom.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerZoomSearchButton->setIcon(icon6);
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
        viewerZoomInButton->setIcon(icon4);
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


        retranslateUi(BlobPresenceDialog);

        spotParamsStackedWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(BlobPresenceDialog);
    } // setupUi

    void retranslateUi(QDialog *BlobPresenceDialog)
    {
        BlobPresenceDialog->setWindowTitle(QCoreApplication::translate("BlobPresenceDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \346\226\221\347\202\271\346\234\211\346\227\240", nullptr));
        setupWindowTitleLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\303\227", nullptr));
        setupPageCodeLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "428", nullptr));
        setupSaveButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\344\277\235\345\255\230", nullptr));
        setupSaveAsButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\345\217\246\345\255\230\344\270\272", nullptr));
        setupExportButton->setText(QCoreApplication::translate("BlobPresenceDialog", "IO\350\276\223\345\207\272", nullptr));
        editorTitleLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\346\226\221\347\202\271\346\234\211\346\227\240", nullptr));
        blobPcImportButton->setText(QCoreApplication::translate("BlobPresenceDialog", "PC\345\257\274\345\205\245\345\233\276\347\211\207", nullptr));
        blobPcImportButton->setProperty("actionRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "secondary", nullptr)));
        basicSegmentButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\345\237\272\347\241\200", nullptr));
        allSegmentButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\345\205\250\351\203\250", nullptr));
        basicDetectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "configCard", nullptr)));
        basicDetectionAreaTitleLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        basicDetectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "cardTitle", nullptr)));
        basicDetectionCollapseButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\214\204", nullptr));
        basicDetectionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "collapseCard", nullptr)));
        basicDetectionAreaLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\346\243\200\346\265\213\345\214\272", nullptr));
        basicDetectionAreaLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        basicDetectionRectButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\226\241", nullptr));
        basicDetectionCircleButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\227\213", nullptr));
        basicDetectionPolygonButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\254\241", nullptr));
        basicDetectionResetButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\237\263", nullptr));
        basicPositionEnableLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243\344\275\277\350\203\275 \342\223\230", nullptr));
        basicPositionEnableLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        basicPositionCorrectionSwitch->setText(QString());
        basicPositionSourceLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        basicPositionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        basicPositionCorrectionComboBox->setItemText(0, QCoreApplication::translate("BlobPresenceDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        basicGrayRecognitionSettingsCard->setProperty("panelRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "configCard", nullptr)));
        basicGrayRecognitionSettingsTitleLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\347\201\260\345\272\246\350\257\206\345\210\253\350\256\276\347\275\256", nullptr));
        basicGrayRecognitionSettingsTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "cardTitle", nullptr)));
        basicGrayCollapseButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\214\204", nullptr));
        basicGrayCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "collapseCard", nullptr)));
        basicGrayThresholdLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\347\201\260\345\272\246\351\230\210\345\200\274 \342\223\230", nullptr));
        basicGrayThresholdLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        basicHorizontalLayout_grayThresholdDashLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "-", nullptr));
        basicResultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "configCard", nullptr)));
        basicResultJudgeTitleLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        basicResultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "cardTitle", nullptr)));
        basicResultPresenceFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "optionGroup", nullptr)));
        basicPresentOkButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\345\255\230\345\234\250OK", nullptr));
        basicAbsentOkButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\344\270\215\345\255\230\345\234\250OK", nullptr));
        detectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "configCard", nullptr)));
        detectionAreaTitleLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        detectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "cardTitle", nullptr)));
        detectionCollapseButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\214\204", nullptr));
        detectionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "collapseCard", nullptr)));
        detectionAreaLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\346\243\200\346\265\213\345\214\272", nullptr));
        detectionAreaLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        detectionRectButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\226\241", nullptr));
        detectionCircleButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\227\213", nullptr));
        detectionPolygonButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\254\241", nullptr));
        detectionResetButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\237\263", nullptr));
        detectionMaskLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\345\261\217\350\224\275\345\214\272\345\237\237", nullptr));
        detectionMaskLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        detectionMaskEditButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\347\274\226\350\276\221", nullptr));
        detectionMaskEditButton->setProperty("actionRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "gray", nullptr)));
        positionEnableLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243\344\275\277\350\203\275 \342\223\230", nullptr));
        positionEnableLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        positionCorrectionSwitch->setText(QString());
        positionSourceLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        positionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        positionCorrectionComboBox->setItemText(0, QCoreApplication::translate("BlobPresenceDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        grayRecognitionSettingsCard->setProperty("panelRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "configCard", nullptr)));
        grayRecognitionSettingsTitleLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\347\201\260\345\272\246\350\257\206\345\210\253\350\256\276\347\275\256", nullptr));
        grayRecognitionSettingsTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "cardTitle", nullptr)));
        grayCollapseButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\342\214\204", nullptr));
        grayCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "collapseCard", nullptr)));
        grayThresholdLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\347\201\260\345\272\246\351\230\210\345\200\274 \342\223\230", nullptr));
        grayThresholdLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        horizontalLayout_grayThresholdDashLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "-", nullptr));
        invertRangeLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\345\217\226\345\217\215\345\220\221\350\214\203\345\233\264\345\200\274", nullptr));
        invertRangeLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        invertRangeSwitch->setText(QString());
        areaRangeLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\351\235\242\347\247\257\350\214\203\345\233\264", nullptr));
        areaRangeLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        horizontalLayout_areaRangeDashLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "-", nullptr));
        maskOutputLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\346\216\251\350\206\234\345\233\276\350\276\223\345\207\272\344\275\277\350\203\275", nullptr));
        maskOutputLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "rowField", nullptr)));
        maskOutputSwitch->setText(QString());
        resultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "configCard", nullptr)));
        resultJudgeTitleLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        resultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("BlobPresenceDialog", "cardTitle", nullptr)));
        resultPresenceFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "optionGroup", nullptr)));
        presentOkButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\345\255\230\345\234\250OK", nullptr));
        absentOkButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\344\270\215\345\255\230\345\234\250OK", nullptr));
        referenceTestButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\345\237\272\345\207\206\345\233\276\346\265\213\350\257\225", nullptr));
        referenceTestButton->setProperty("actionRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "secondary", nullptr)));
        testRunButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\346\265\213\350\257\225\350\277\220\350\241\214", nullptr));
        testRunButton->setProperty("actionRole", QVariant(QCoreApplication::translate("BlobPresenceDialog", "secondary", nullptr)));
        finishButton->setText(QCoreApplication::translate("BlobPresenceDialog", "\345\256\214\346\210\220", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerZoomOutButton->setText(QCoreApplication::translate("BlobPresenceDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "39%", nullptr));
        viewerStatusLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: --ms   \345\267\245\345\205\267\350\200\227\346\227\266: --ms", nullptr));
        viewerCursorLabel->setText(QCoreApplication::translate("BlobPresenceDialog", "X: --  Y: ---   |   R: -- G: -- B: --", nullptr));
    } // retranslateUi

};

namespace Ui {
    class BlobPresenceDialog: public Ui_BlobPresenceDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_BLOBPRESENCEDIALOG_H
