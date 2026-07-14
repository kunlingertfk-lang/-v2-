/********************************************************************************
** Form generated from reading UI file 'ContourPresenceDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONTOURPRESENCEDIALOG_H
#define UI_CONTOURPRESENCEDIALOG_H

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

class Ui_ContourPresenceDialog
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
    QToolButton *patternExternalEditButton;
    QSpacerItem *horizontalSpacer_editorHeader;
    QFrame *segmentFrame;
    QHBoxLayout *horizontalLayout_segment;
    QPushButton *basicSegmentButton;
    QPushButton *allSegmentButton;
    QStackedWidget *contourParamsStackedWidget;
    QWidget *basicParamsPage;
    QVBoxLayout *verticalLayout_basicParamsPage;
    QScrollArea *basicParamsScrollArea;
    QWidget *basicParamsContents;
    QVBoxLayout *verticalLayout_basicConfigCards;
    QFrame *basicSearchTemplateCard;
    QVBoxLayout *basicVerticalLayout_searchTemplate;
    QHBoxLayout *basicHorizontalLayout_templateHeader;
    QLabel *basicSearchTemplateTitleLabel;
    QSpacerItem *basicHorizontalSpacer_templateHeader;
    QToolButton *basicTemplateCollapseButton;
    QHBoxLayout *basicHorizontalLayout_templateArea;
    QLabel *basicTemplateAreaLabel;
    QSpacerItem *horizontalSpacer_3;
    QToolButton *basicTemplateRectButton;
    QToolButton *basicTemplatePolygonButton;
    QPushButton *basicTemplateFinishButton;
    QFrame *basicDetectionAreaCard;
    QVBoxLayout *basicVerticalLayout_detectionArea;
    QHBoxLayout *basicHorizontalLayout_detectionHeader;
    QLabel *basicDetectionAreaTitleLabel;
    QSpacerItem *basicHorizontalSpacer_detectionHeader;
    QToolButton *basicDetectionCollapseButton;
    QHBoxLayout *basicHorizontalLayout_detectionAreaTools;
    QLabel *basicDetectionAreaLabel;
    QToolButton *basicDetectionDrawButton;
    QToolButton *basicDetectionRectButton;
    QToolButton *basicDetectionCircleButton;
    QToolButton *basicDetectionPolygonButton;
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
    QSpacerItem *basicHorizontalSpacer_recognitionHeader;
    QToolButton *basicRecognitionCollapseButton;
    QHBoxLayout *basicHorizontalLayout_showContour;
    QLabel *basicShowContourLabel;
    QSpacerItem *basicHorizontalSpacer_showContour;
    QCheckBox *basicShowContourSwitch;
    QFrame *basicResultJudgeCard;
    QVBoxLayout *basicVerticalLayout_resultJudge;
    QHBoxLayout *basicHorizontalLayout_resultHeader;
    QLabel *basicResultJudgeTitleLabel;
    QSpacerItem *basicHorizontalSpacer_resultHeader;
    QToolButton *basicResultCollapseButton;
    QHBoxLayout *basicHorizontalLayout_resultBasis;
    QLabel *basicResultBasisLabel;
    QComboBox *basicResultBasisComboBox;
    QStackedWidget *basicResultBasisStackedWidget;
    QWidget *basicResultPresencePage;
    QHBoxLayout *basicHorizontalLayout_resultPresence;
    QLabel *basicResultPresenceLabel;
    QFrame *basicResultPresenceFrame;
    QHBoxLayout *basicHorizontalLayout_resultPresenceButtons;
    QPushButton *basicPresentOkButton;
    QPushButton *basicAbsentOkButton;
    QWidget *basicResultScorePage;
    QHBoxLayout *basicHorizontalLayout_resultScore;
    QLabel *basicResultScoreLabel;
    QSpinBox *basicResultMinScoreSpinBox;
    QSpacerItem *basicVerticalSpacer_configCards;
    QWidget *allParamsPage;
    QVBoxLayout *verticalLayout_allParamsPage;
    QScrollArea *contourConfigScrollArea;
    QWidget *contourConfigContents;
    QVBoxLayout *verticalLayout_configCards;
    QFrame *searchTemplateCard;
    QVBoxLayout *verticalLayout_searchTemplate;
    QHBoxLayout *horizontalLayout_templateHeader;
    QLabel *searchTemplateTitleLabel;
    QSpacerItem *horizontalSpacer_templateHeader;
    QToolButton *templateCollapseButton;
    QHBoxLayout *horizontalLayout_templateArea;
    QLabel *templateAreaLabel;
    QSpacerItem *horizontalSpacer_templateArea;
    QToolButton *templateRectButton;
    QToolButton *templatePolygonButton;
    QPushButton *templateFinishButton;
    QHBoxLayout *horizontalLayout_templateMask;
    QLabel *templateMaskLabel;
    QPushButton *templateMaskEditButton;
    QHBoxLayout *horizontalLayout_scaleMode;
    QLabel *scaleModeLabel;
    QFrame *scaleModeFrame;
    QHBoxLayout *horizontalLayout_scaleModeButtons;
    QPushButton *scaleManualButton;
    QPushButton *scaleAutoButton;
    QWidget *scaleSpeedRow;
    QHBoxLayout *horizontalLayout_scaleSpeed;
    QLabel *speedScaleLabel;
    QSpinBox *speedScaleSpinBox;
    QWidget *scaleFeatureRow;
    QHBoxLayout *horizontalLayout_scaleFeature;
    QLabel *featureScaleLabel;
    QSpinBox *featureScaleSpinBox;
    QHBoxLayout *horizontalLayout_thresholdMode;
    QLabel *thresholdModeLabel;
    QFrame *thresholdModeFrame;
    QHBoxLayout *horizontalLayout_thresholdModeButtons;
    QPushButton *thresholdManualButton;
    QPushButton *thresholdAutoButton;
    QWidget *thresholdGrayRow;
    QHBoxLayout *horizontalLayout_grayThreshold;
    QLabel *grayThresholdLabel;
    QSpinBox *grayThresholdSpinBox;
    QHBoxLayout *horizontalLayout_chainMode;
    QLabel *chainModeLabel;
    QFrame *chainModeFrame;
    QHBoxLayout *horizontalLayout_chainModeButtons;
    QPushButton *chainManualButton;
    QPushButton *chainAutoButton;
    QWidget *chainMinRow;
    QHBoxLayout *horizontalLayout_chainMin;
    QLabel *minChainLengthLabel;
    QSpinBox *minChainLengthSpinBox;
    QFrame *detectionAreaCard;
    QVBoxLayout *verticalLayout_detectionArea;
    QHBoxLayout *horizontalLayout_detectionHeader;
    QLabel *detectionAreaTitleLabel;
    QSpacerItem *horizontalSpacer_detectionHeader;
    QToolButton *detectionCollapseButton;
    QHBoxLayout *horizontalLayout_detectionAreaTools;
    QLabel *detectionAreaLabel;
    QToolButton *detectionDrawButton;
    QToolButton *detectionRectButton;
    QToolButton *detectionCircleButton;
    QToolButton *detectionPolygonButton;
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
    QSpacerItem *horizontalSpacer_recognitionHeader;
    QToolButton *recognitionCollapseButton;
    QHBoxLayout *horizontalLayout_minScore;
    QLabel *minScoreLabel;
    QSpinBox *minScoreSpinBox;
    QHBoxLayout *horizontalLayout_matchPolarity;
    QLabel *matchPolarityLabel;
    QComboBox *matchPolarityComboBox;
    QHBoxLayout *horizontalLayout_thresholdType;
    QLabel *thresholdTypeLabel;
    QComboBox *thresholdTypeComboBox;
    QHBoxLayout *horizontalLayout_scaleRange;
    QLabel *scaleRangeLabel;
    QSpacerItem *horizontalSpacer;
    QSpinBox *minScaleSpinBox;
    QLabel *scaleDashLabel;
    QSpinBox *maxScaleSpinBox;
    QHBoxLayout *horizontalLayout_angleRange;
    QLabel *angleRangeLabel;
    QSpacerItem *horizontalSpacer_2;
    QSpinBox *minAngleSpinBox;
    QLabel *angleDashLabel;
    QSpinBox *maxAngleSpinBox;
    QHBoxLayout *horizontalLayout_timeout;
    QLabel *timeoutLabel;
    QSpinBox *timeoutSpinBox;
    QHBoxLayout *horizontalLayout_showContour;
    QLabel *showContourLabel;
    QSpacerItem *horizontalSpacer_showContour;
    QCheckBox *showContourSwitch;
    QHBoxLayout *horizontalLayout_sortMode;
    QLabel *sortModeLabel;
    QComboBox *sortModeComboBox;
    QFrame *resultJudgeCard;
    QVBoxLayout *verticalLayout_resultJudge;
    QHBoxLayout *horizontalLayout_resultHeader;
    QLabel *resultJudgeTitleLabel;
    QSpacerItem *horizontalSpacer_resultHeader;
    QToolButton *resultCollapseButton;
    QHBoxLayout *horizontalLayout_resultBasis;
    QLabel *resultBasisLabel;
    QComboBox *resultBasisComboBox;
    QStackedWidget *resultBasisStackedWidget;
    QWidget *resultPresencePage;
    QHBoxLayout *horizontalLayout_resultPresence;
    QLabel *resultPresenceLabel;
    QFrame *resultPresenceFrame;
    QHBoxLayout *horizontalLayout_resultPresenceButtons;
    QPushButton *presentOkButton;
    QPushButton *absentOkButton;
    QWidget *resultScorePage;
    QHBoxLayout *horizontalLayout_resultScore;
    QLabel *resultScoreLabel;
    QSpinBox *resultMinScoreSpinBox;
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

    void setupUi(QDialog *ContourPresenceDialog)
    {
        if (ContourPresenceDialog->objectName().isEmpty())
            ContourPresenceDialog->setObjectName(QString::fromUtf8("ContourPresenceDialog"));
        ContourPresenceDialog->resize(1760, 980);
        verticalLayout_root = new QVBoxLayout(ContourPresenceDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        setupTopBar = new QFrame(ContourPresenceDialog);
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

        setupToolbar = new QFrame(ContourPresenceDialog);
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

        setupBodyFrame = new QFrame(ContourPresenceDialog);
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

        patternExternalEditButton = new QToolButton(setupEditorPanel);
        patternExternalEditButton->setObjectName(QString::fromUtf8("patternExternalEditButton"));
        patternExternalEditButton->setIcon(icon);
        patternExternalEditButton->setIconSize(QSize(24, 24));
        patternExternalEditButton->setAutoRaise(true);

        horizontalLayout_editorHeader->addWidget(patternExternalEditButton);

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

        contourParamsStackedWidget = new QStackedWidget(setupEditorPanel);
        contourParamsStackedWidget->setObjectName(QString::fromUtf8("contourParamsStackedWidget"));
        basicParamsPage = new QWidget();
        basicParamsPage->setObjectName(QString::fromUtf8("basicParamsPage"));
        verticalLayout_basicParamsPage = new QVBoxLayout(basicParamsPage);
        verticalLayout_basicParamsPage->setSpacing(0);
        verticalLayout_basicParamsPage->setObjectName(QString::fromUtf8("verticalLayout_basicParamsPage"));
        verticalLayout_basicParamsPage->setContentsMargins(0, 0, 0, 0);
        basicParamsScrollArea = new QScrollArea(basicParamsPage);
        basicParamsScrollArea->setObjectName(QString::fromUtf8("basicParamsScrollArea"));
        basicParamsScrollArea->setFrameShape(QFrame::NoFrame);
        basicParamsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        basicParamsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        basicParamsScrollArea->setWidgetResizable(true);
        basicParamsContents = new QWidget();
        basicParamsContents->setObjectName(QString::fromUtf8("basicParamsContents"));
        basicParamsContents->setGeometry(QRect(0, 0, 554, 692));
        verticalLayout_basicConfigCards = new QVBoxLayout(basicParamsContents);
        verticalLayout_basicConfigCards->setSpacing(12);
        verticalLayout_basicConfigCards->setObjectName(QString::fromUtf8("verticalLayout_basicConfigCards"));
        verticalLayout_basicConfigCards->setContentsMargins(0, 0, 0, 0);
        basicSearchTemplateCard = new QFrame(basicParamsContents);
        basicSearchTemplateCard->setObjectName(QString::fromUtf8("basicSearchTemplateCard"));
        basicSearchTemplateCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_searchTemplate = new QVBoxLayout(basicSearchTemplateCard);
        basicVerticalLayout_searchTemplate->setSpacing(16);
        basicVerticalLayout_searchTemplate->setObjectName(QString::fromUtf8("basicVerticalLayout_searchTemplate"));
        basicVerticalLayout_searchTemplate->setContentsMargins(20, 18, 20, 18);
        basicHorizontalLayout_templateHeader = new QHBoxLayout();
        basicHorizontalLayout_templateHeader->setObjectName(QString::fromUtf8("basicHorizontalLayout_templateHeader"));
        basicSearchTemplateTitleLabel = new QLabel(basicSearchTemplateCard);
        basicSearchTemplateTitleLabel->setObjectName(QString::fromUtf8("basicSearchTemplateTitleLabel"));

        basicHorizontalLayout_templateHeader->addWidget(basicSearchTemplateTitleLabel);

        basicHorizontalSpacer_templateHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_templateHeader->addItem(basicHorizontalSpacer_templateHeader);

        basicTemplateCollapseButton = new QToolButton(basicSearchTemplateCard);
        basicTemplateCollapseButton->setObjectName(QString::fromUtf8("basicTemplateCollapseButton"));

        basicHorizontalLayout_templateHeader->addWidget(basicTemplateCollapseButton);


        basicVerticalLayout_searchTemplate->addLayout(basicHorizontalLayout_templateHeader);

        basicHorizontalLayout_templateArea = new QHBoxLayout();
        basicHorizontalLayout_templateArea->setSpacing(0);
        basicHorizontalLayout_templateArea->setObjectName(QString::fromUtf8("basicHorizontalLayout_templateArea"));
        basicHorizontalLayout_templateArea->setContentsMargins(0, 0, 0, 0);
        basicTemplateAreaLabel = new QLabel(basicSearchTemplateCard);
        basicTemplateAreaLabel->setObjectName(QString::fromUtf8("basicTemplateAreaLabel"));
        basicTemplateAreaLabel->setMinimumSize(QSize(150, 0));

        basicHorizontalLayout_templateArea->addWidget(basicTemplateAreaLabel);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_templateArea->addItem(horizontalSpacer_3);

        basicTemplateRectButton = new QToolButton(basicSearchTemplateCard);
        basicTemplateRectButton->setObjectName(QString::fromUtf8("basicTemplateRectButton"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(basicTemplateRectButton->sizePolicy().hasHeightForWidth());
        basicTemplateRectButton->setSizePolicy(sizePolicy);
        basicTemplateRectButton->setMinimumSize(QSize(86, 38));
        basicTemplateRectButton->setCheckable(true);
        basicTemplateRectButton->setChecked(true);

        basicHorizontalLayout_templateArea->addWidget(basicTemplateRectButton);

        basicTemplatePolygonButton = new QToolButton(basicSearchTemplateCard);
        basicTemplatePolygonButton->setObjectName(QString::fromUtf8("basicTemplatePolygonButton"));
        sizePolicy.setHeightForWidth(basicTemplatePolygonButton->sizePolicy().hasHeightForWidth());
        basicTemplatePolygonButton->setSizePolicy(sizePolicy);
        basicTemplatePolygonButton->setMinimumSize(QSize(86, 38));
        basicTemplatePolygonButton->setCheckable(true);

        basicHorizontalLayout_templateArea->addWidget(basicTemplatePolygonButton);

        basicTemplateFinishButton = new QPushButton(basicSearchTemplateCard);
        basicTemplateFinishButton->setObjectName(QString::fromUtf8("basicTemplateFinishButton"));
        sizePolicy.setHeightForWidth(basicTemplateFinishButton->sizePolicy().hasHeightForWidth());
        basicTemplateFinishButton->setSizePolicy(sizePolicy);
        basicTemplateFinishButton->setMinimumSize(QSize(86, 38));

        basicHorizontalLayout_templateArea->addWidget(basicTemplateFinishButton);


        basicVerticalLayout_searchTemplate->addLayout(basicHorizontalLayout_templateArea);


        verticalLayout_basicConfigCards->addWidget(basicSearchTemplateCard);

        basicDetectionAreaCard = new QFrame(basicParamsContents);
        basicDetectionAreaCard->setObjectName(QString::fromUtf8("basicDetectionAreaCard"));
        basicDetectionAreaCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_detectionArea = new QVBoxLayout(basicDetectionAreaCard);
        basicVerticalLayout_detectionArea->setSpacing(16);
        basicVerticalLayout_detectionArea->setObjectName(QString::fromUtf8("basicVerticalLayout_detectionArea"));
        basicVerticalLayout_detectionArea->setContentsMargins(20, 18, 20, 18);
        basicHorizontalLayout_detectionHeader = new QHBoxLayout();
        basicHorizontalLayout_detectionHeader->setObjectName(QString::fromUtf8("basicHorizontalLayout_detectionHeader"));
        basicDetectionAreaTitleLabel = new QLabel(basicDetectionAreaCard);
        basicDetectionAreaTitleLabel->setObjectName(QString::fromUtf8("basicDetectionAreaTitleLabel"));

        basicHorizontalLayout_detectionHeader->addWidget(basicDetectionAreaTitleLabel);

        basicHorizontalSpacer_detectionHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_detectionHeader->addItem(basicHorizontalSpacer_detectionHeader);

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
        basicDetectionDrawButton->setMinimumSize(QSize(86, 38));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/fit.svg"), QSize(), QIcon::Normal, QIcon::Off);
        basicDetectionDrawButton->setIcon(icon4);
        basicDetectionDrawButton->setIconSize(QSize(22, 22));
        basicDetectionDrawButton->setCheckable(true);
        basicDetectionDrawButton->setChecked(true);

        basicHorizontalLayout_detectionAreaTools->addWidget(basicDetectionDrawButton);

        basicDetectionRectButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionRectButton->setObjectName(QString::fromUtf8("basicDetectionRectButton"));
        basicDetectionRectButton->setMinimumSize(QSize(86, 38));
        basicDetectionRectButton->setCheckable(true);

        basicHorizontalLayout_detectionAreaTools->addWidget(basicDetectionRectButton);

        basicDetectionCircleButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionCircleButton->setObjectName(QString::fromUtf8("basicDetectionCircleButton"));
        basicDetectionCircleButton->setMinimumSize(QSize(86, 38));
        basicDetectionCircleButton->setCheckable(true);

        basicHorizontalLayout_detectionAreaTools->addWidget(basicDetectionCircleButton);

        basicDetectionPolygonButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionPolygonButton->setObjectName(QString::fromUtf8("basicDetectionPolygonButton"));
        basicDetectionPolygonButton->setMinimumSize(QSize(86, 38));
        basicDetectionPolygonButton->setCheckable(true);

        basicHorizontalLayout_detectionAreaTools->addWidget(basicDetectionPolygonButton);


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

        basicRecognitionSettingsCard = new QFrame(basicParamsContents);
        basicRecognitionSettingsCard->setObjectName(QString::fromUtf8("basicRecognitionSettingsCard"));
        basicRecognitionSettingsCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_recognitionSettings = new QVBoxLayout(basicRecognitionSettingsCard);
        basicVerticalLayout_recognitionSettings->setSpacing(16);
        basicVerticalLayout_recognitionSettings->setObjectName(QString::fromUtf8("basicVerticalLayout_recognitionSettings"));
        basicVerticalLayout_recognitionSettings->setContentsMargins(20, 18, 20, 18);
        basicHorizontalLayout_recognitionHeader = new QHBoxLayout();
        basicHorizontalLayout_recognitionHeader->setObjectName(QString::fromUtf8("basicHorizontalLayout_recognitionHeader"));
        basicRecognitionSettingsTitleLabel = new QLabel(basicRecognitionSettingsCard);
        basicRecognitionSettingsTitleLabel->setObjectName(QString::fromUtf8("basicRecognitionSettingsTitleLabel"));

        basicHorizontalLayout_recognitionHeader->addWidget(basicRecognitionSettingsTitleLabel);

        basicHorizontalSpacer_recognitionHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_recognitionHeader->addItem(basicHorizontalSpacer_recognitionHeader);

        basicRecognitionCollapseButton = new QToolButton(basicRecognitionSettingsCard);
        basicRecognitionCollapseButton->setObjectName(QString::fromUtf8("basicRecognitionCollapseButton"));

        basicHorizontalLayout_recognitionHeader->addWidget(basicRecognitionCollapseButton);


        basicVerticalLayout_recognitionSettings->addLayout(basicHorizontalLayout_recognitionHeader);

        basicHorizontalLayout_showContour = new QHBoxLayout();
        basicHorizontalLayout_showContour->setObjectName(QString::fromUtf8("basicHorizontalLayout_showContour"));
        basicHorizontalLayout_showContour->setContentsMargins(0, 0, 0, 0);
        basicShowContourLabel = new QLabel(basicRecognitionSettingsCard);
        basicShowContourLabel->setObjectName(QString::fromUtf8("basicShowContourLabel"));
        basicShowContourLabel->setMinimumSize(QSize(260, 0));

        basicHorizontalLayout_showContour->addWidget(basicShowContourLabel);

        basicHorizontalSpacer_showContour = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_showContour->addItem(basicHorizontalSpacer_showContour);

        basicShowContourSwitch = new QCheckBox(basicRecognitionSettingsCard);
        basicShowContourSwitch->setObjectName(QString::fromUtf8("basicShowContourSwitch"));

        basicHorizontalLayout_showContour->addWidget(basicShowContourSwitch);


        basicVerticalLayout_recognitionSettings->addLayout(basicHorizontalLayout_showContour);


        verticalLayout_basicConfigCards->addWidget(basicRecognitionSettingsCard);

        basicResultJudgeCard = new QFrame(basicParamsContents);
        basicResultJudgeCard->setObjectName(QString::fromUtf8("basicResultJudgeCard"));
        basicResultJudgeCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_resultJudge = new QVBoxLayout(basicResultJudgeCard);
        basicVerticalLayout_resultJudge->setSpacing(16);
        basicVerticalLayout_resultJudge->setObjectName(QString::fromUtf8("basicVerticalLayout_resultJudge"));
        basicVerticalLayout_resultJudge->setContentsMargins(20, 18, 20, 18);
        basicHorizontalLayout_resultHeader = new QHBoxLayout();
        basicHorizontalLayout_resultHeader->setObjectName(QString::fromUtf8("basicHorizontalLayout_resultHeader"));
        basicResultJudgeTitleLabel = new QLabel(basicResultJudgeCard);
        basicResultJudgeTitleLabel->setObjectName(QString::fromUtf8("basicResultJudgeTitleLabel"));

        basicHorizontalLayout_resultHeader->addWidget(basicResultJudgeTitleLabel);

        basicHorizontalSpacer_resultHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_resultHeader->addItem(basicHorizontalSpacer_resultHeader);

        basicResultCollapseButton = new QToolButton(basicResultJudgeCard);
        basicResultCollapseButton->setObjectName(QString::fromUtf8("basicResultCollapseButton"));

        basicHorizontalLayout_resultHeader->addWidget(basicResultCollapseButton);


        basicVerticalLayout_resultJudge->addLayout(basicHorizontalLayout_resultHeader);

        basicHorizontalLayout_resultBasis = new QHBoxLayout();
        basicHorizontalLayout_resultBasis->setObjectName(QString::fromUtf8("basicHorizontalLayout_resultBasis"));
        basicHorizontalLayout_resultBasis->setContentsMargins(0, 0, 0, 0);
        basicResultBasisLabel = new QLabel(basicResultJudgeCard);
        basicResultBasisLabel->setObjectName(QString::fromUtf8("basicResultBasisLabel"));
        basicResultBasisLabel->setMinimumSize(QSize(150, 0));

        basicHorizontalLayout_resultBasis->addWidget(basicResultBasisLabel);

        basicResultBasisComboBox = new QComboBox(basicResultJudgeCard);
        basicResultBasisComboBox->addItem(QString());
        basicResultBasisComboBox->addItem(QString());
        basicResultBasisComboBox->setObjectName(QString::fromUtf8("basicResultBasisComboBox"));
        basicResultBasisComboBox->setMinimumSize(QSize(260, 38));

        basicHorizontalLayout_resultBasis->addWidget(basicResultBasisComboBox);


        basicVerticalLayout_resultJudge->addLayout(basicHorizontalLayout_resultBasis);

        basicResultBasisStackedWidget = new QStackedWidget(basicResultJudgeCard);
        basicResultBasisStackedWidget->setObjectName(QString::fromUtf8("basicResultBasisStackedWidget"));
        basicResultPresencePage = new QWidget();
        basicResultPresencePage->setObjectName(QString::fromUtf8("basicResultPresencePage"));
        basicHorizontalLayout_resultPresence = new QHBoxLayout(basicResultPresencePage);
        basicHorizontalLayout_resultPresence->setObjectName(QString::fromUtf8("basicHorizontalLayout_resultPresence"));
        basicHorizontalLayout_resultPresence->setContentsMargins(0, 0, 0, 0);
        basicResultPresenceLabel = new QLabel(basicResultPresencePage);
        basicResultPresenceLabel->setObjectName(QString::fromUtf8("basicResultPresenceLabel"));
        basicResultPresenceLabel->setMinimumSize(QSize(150, 0));

        basicHorizontalLayout_resultPresence->addWidget(basicResultPresenceLabel);

        basicResultPresenceFrame = new QFrame(basicResultPresencePage);
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


        basicHorizontalLayout_resultPresence->addWidget(basicResultPresenceFrame);

        basicResultBasisStackedWidget->addWidget(basicResultPresencePage);
        basicResultScorePage = new QWidget();
        basicResultScorePage->setObjectName(QString::fromUtf8("basicResultScorePage"));
        basicHorizontalLayout_resultScore = new QHBoxLayout(basicResultScorePage);
        basicHorizontalLayout_resultScore->setObjectName(QString::fromUtf8("basicHorizontalLayout_resultScore"));
        basicHorizontalLayout_resultScore->setContentsMargins(0, 0, 0, 0);
        basicResultScoreLabel = new QLabel(basicResultScorePage);
        basicResultScoreLabel->setObjectName(QString::fromUtf8("basicResultScoreLabel"));
        basicResultScoreLabel->setMinimumSize(QSize(150, 0));

        basicHorizontalLayout_resultScore->addWidget(basicResultScoreLabel);

        basicResultMinScoreSpinBox = new QSpinBox(basicResultScorePage);
        basicResultMinScoreSpinBox->setObjectName(QString::fromUtf8("basicResultMinScoreSpinBox"));
        basicResultMinScoreSpinBox->setMinimumSize(QSize(260, 38));
        basicResultMinScoreSpinBox->setMinimum(0);
        basicResultMinScoreSpinBox->setMaximum(100);
        basicResultMinScoreSpinBox->setValue(50);

        basicHorizontalLayout_resultScore->addWidget(basicResultMinScoreSpinBox);

        basicResultBasisStackedWidget->addWidget(basicResultScorePage);

        basicVerticalLayout_resultJudge->addWidget(basicResultBasisStackedWidget);


        verticalLayout_basicConfigCards->addWidget(basicResultJudgeCard);

        basicVerticalSpacer_configCards = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_basicConfigCards->addItem(basicVerticalSpacer_configCards);

        basicParamsScrollArea->setWidget(basicParamsContents);

        verticalLayout_basicParamsPage->addWidget(basicParamsScrollArea);

        contourParamsStackedWidget->addWidget(basicParamsPage);
        allParamsPage = new QWidget();
        allParamsPage->setObjectName(QString::fromUtf8("allParamsPage"));
        verticalLayout_allParamsPage = new QVBoxLayout(allParamsPage);
        verticalLayout_allParamsPage->setSpacing(0);
        verticalLayout_allParamsPage->setObjectName(QString::fromUtf8("verticalLayout_allParamsPage"));
        verticalLayout_allParamsPage->setContentsMargins(0, 0, 0, 0);
        contourConfigScrollArea = new QScrollArea(allParamsPage);
        contourConfigScrollArea->setObjectName(QString::fromUtf8("contourConfigScrollArea"));
        contourConfigScrollArea->setFrameShape(QFrame::NoFrame);
        contourConfigScrollArea->setWidgetResizable(true);
        contourConfigContents = new QWidget();
        contourConfigContents->setObjectName(QString::fromUtf8("contourConfigContents"));
        contourConfigContents->setGeometry(QRect(0, -854, 540, 1546));
        verticalLayout_configCards = new QVBoxLayout(contourConfigContents);
        verticalLayout_configCards->setSpacing(12);
        verticalLayout_configCards->setObjectName(QString::fromUtf8("verticalLayout_configCards"));
        verticalLayout_configCards->setContentsMargins(0, 0, 0, 0);
        searchTemplateCard = new QFrame(contourConfigContents);
        searchTemplateCard->setObjectName(QString::fromUtf8("searchTemplateCard"));
        searchTemplateCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_searchTemplate = new QVBoxLayout(searchTemplateCard);
        verticalLayout_searchTemplate->setSpacing(16);
        verticalLayout_searchTemplate->setObjectName(QString::fromUtf8("verticalLayout_searchTemplate"));
        verticalLayout_searchTemplate->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_templateHeader = new QHBoxLayout();
        horizontalLayout_templateHeader->setObjectName(QString::fromUtf8("horizontalLayout_templateHeader"));
        searchTemplateTitleLabel = new QLabel(searchTemplateCard);
        searchTemplateTitleLabel->setObjectName(QString::fromUtf8("searchTemplateTitleLabel"));

        horizontalLayout_templateHeader->addWidget(searchTemplateTitleLabel);

        horizontalSpacer_templateHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_templateHeader->addItem(horizontalSpacer_templateHeader);

        templateCollapseButton = new QToolButton(searchTemplateCard);
        templateCollapseButton->setObjectName(QString::fromUtf8("templateCollapseButton"));

        horizontalLayout_templateHeader->addWidget(templateCollapseButton);


        verticalLayout_searchTemplate->addLayout(horizontalLayout_templateHeader);

        horizontalLayout_templateArea = new QHBoxLayout();
        horizontalLayout_templateArea->setSpacing(0);
        horizontalLayout_templateArea->setObjectName(QString::fromUtf8("horizontalLayout_templateArea"));
        horizontalLayout_templateArea->setContentsMargins(0, 0, 0, 0);
        templateAreaLabel = new QLabel(searchTemplateCard);
        templateAreaLabel->setObjectName(QString::fromUtf8("templateAreaLabel"));
        templateAreaLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_templateArea->addWidget(templateAreaLabel);

        horizontalSpacer_templateArea = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_templateArea->addItem(horizontalSpacer_templateArea);

        templateRectButton = new QToolButton(searchTemplateCard);
        templateRectButton->setObjectName(QString::fromUtf8("templateRectButton"));
        sizePolicy.setHeightForWidth(templateRectButton->sizePolicy().hasHeightForWidth());
        templateRectButton->setSizePolicy(sizePolicy);
        templateRectButton->setMinimumSize(QSize(86, 38));
        templateRectButton->setCheckable(true);
        templateRectButton->setChecked(true);

        horizontalLayout_templateArea->addWidget(templateRectButton);

        templatePolygonButton = new QToolButton(searchTemplateCard);
        templatePolygonButton->setObjectName(QString::fromUtf8("templatePolygonButton"));
        sizePolicy.setHeightForWidth(templatePolygonButton->sizePolicy().hasHeightForWidth());
        templatePolygonButton->setSizePolicy(sizePolicy);
        templatePolygonButton->setMinimumSize(QSize(86, 38));
        templatePolygonButton->setCheckable(true);

        horizontalLayout_templateArea->addWidget(templatePolygonButton);

        templateFinishButton = new QPushButton(searchTemplateCard);
        templateFinishButton->setObjectName(QString::fromUtf8("templateFinishButton"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(templateFinishButton->sizePolicy().hasHeightForWidth());
        templateFinishButton->setSizePolicy(sizePolicy1);
        templateFinishButton->setMinimumSize(QSize(86, 38));

        horizontalLayout_templateArea->addWidget(templateFinishButton);


        verticalLayout_searchTemplate->addLayout(horizontalLayout_templateArea);

        horizontalLayout_templateMask = new QHBoxLayout();
        horizontalLayout_templateMask->setObjectName(QString::fromUtf8("horizontalLayout_templateMask"));
        horizontalLayout_templateMask->setContentsMargins(0, 0, 0, 0);
        templateMaskLabel = new QLabel(searchTemplateCard);
        templateMaskLabel->setObjectName(QString::fromUtf8("templateMaskLabel"));
        templateMaskLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_templateMask->addWidget(templateMaskLabel);

        templateMaskEditButton = new QPushButton(searchTemplateCard);
        templateMaskEditButton->setObjectName(QString::fromUtf8("templateMaskEditButton"));
        templateMaskEditButton->setMinimumSize(QSize(260, 38));

        horizontalLayout_templateMask->addWidget(templateMaskEditButton);


        verticalLayout_searchTemplate->addLayout(horizontalLayout_templateMask);

        horizontalLayout_scaleMode = new QHBoxLayout();
        horizontalLayout_scaleMode->setObjectName(QString::fromUtf8("horizontalLayout_scaleMode"));
        horizontalLayout_scaleMode->setContentsMargins(0, 0, 0, 0);
        scaleModeLabel = new QLabel(searchTemplateCard);
        scaleModeLabel->setObjectName(QString::fromUtf8("scaleModeLabel"));
        scaleModeLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_scaleMode->addWidget(scaleModeLabel);

        scaleModeFrame = new QFrame(searchTemplateCard);
        scaleModeFrame->setObjectName(QString::fromUtf8("scaleModeFrame"));
        scaleModeFrame->setMinimumSize(QSize(260, 38));
        scaleModeFrame->setMaximumSize(QSize(260, 38));
        horizontalLayout_scaleModeButtons = new QHBoxLayout(scaleModeFrame);
        horizontalLayout_scaleModeButtons->setSpacing(0);
        horizontalLayout_scaleModeButtons->setObjectName(QString::fromUtf8("horizontalLayout_scaleModeButtons"));
        horizontalLayout_scaleModeButtons->setContentsMargins(0, 0, 0, 0);
        scaleManualButton = new QPushButton(scaleModeFrame);
        scaleManualButton->setObjectName(QString::fromUtf8("scaleManualButton"));
        scaleManualButton->setCheckable(true);

        horizontalLayout_scaleModeButtons->addWidget(scaleManualButton);

        scaleAutoButton = new QPushButton(scaleModeFrame);
        scaleAutoButton->setObjectName(QString::fromUtf8("scaleAutoButton"));
        scaleAutoButton->setCheckable(true);
        scaleAutoButton->setChecked(true);

        horizontalLayout_scaleModeButtons->addWidget(scaleAutoButton);


        horizontalLayout_scaleMode->addWidget(scaleModeFrame);


        verticalLayout_searchTemplate->addLayout(horizontalLayout_scaleMode);

        scaleSpeedRow = new QWidget(searchTemplateCard);
        scaleSpeedRow->setObjectName(QString::fromUtf8("scaleSpeedRow"));
        horizontalLayout_scaleSpeed = new QHBoxLayout(scaleSpeedRow);
        horizontalLayout_scaleSpeed->setSpacing(0);
        horizontalLayout_scaleSpeed->setObjectName(QString::fromUtf8("horizontalLayout_scaleSpeed"));
        horizontalLayout_scaleSpeed->setContentsMargins(0, 0, 0, 0);
        speedScaleLabel = new QLabel(scaleSpeedRow);
        speedScaleLabel->setObjectName(QString::fromUtf8("speedScaleLabel"));
        speedScaleLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_scaleSpeed->addWidget(speedScaleLabel);

        speedScaleSpinBox = new QSpinBox(scaleSpeedRow);
        speedScaleSpinBox->setObjectName(QString::fromUtf8("speedScaleSpinBox"));
        speedScaleSpinBox->setMinimumSize(QSize(260, 38));
        speedScaleSpinBox->setMinimum(1);
        speedScaleSpinBox->setMaximum(999);
        speedScaleSpinBox->setValue(5);

        horizontalLayout_scaleSpeed->addWidget(speedScaleSpinBox);


        verticalLayout_searchTemplate->addWidget(scaleSpeedRow);

        scaleFeatureRow = new QWidget(searchTemplateCard);
        scaleFeatureRow->setObjectName(QString::fromUtf8("scaleFeatureRow"));
        horizontalLayout_scaleFeature = new QHBoxLayout(scaleFeatureRow);
        horizontalLayout_scaleFeature->setSpacing(0);
        horizontalLayout_scaleFeature->setObjectName(QString::fromUtf8("horizontalLayout_scaleFeature"));
        horizontalLayout_scaleFeature->setContentsMargins(0, 0, 0, 0);
        featureScaleLabel = new QLabel(scaleFeatureRow);
        featureScaleLabel->setObjectName(QString::fromUtf8("featureScaleLabel"));
        featureScaleLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_scaleFeature->addWidget(featureScaleLabel);

        featureScaleSpinBox = new QSpinBox(scaleFeatureRow);
        featureScaleSpinBox->setObjectName(QString::fromUtf8("featureScaleSpinBox"));
        featureScaleSpinBox->setMinimumSize(QSize(260, 38));
        featureScaleSpinBox->setMinimum(1);
        featureScaleSpinBox->setMaximum(999);
        featureScaleSpinBox->setValue(1);

        horizontalLayout_scaleFeature->addWidget(featureScaleSpinBox);


        verticalLayout_searchTemplate->addWidget(scaleFeatureRow);

        horizontalLayout_thresholdMode = new QHBoxLayout();
        horizontalLayout_thresholdMode->setObjectName(QString::fromUtf8("horizontalLayout_thresholdMode"));
        horizontalLayout_thresholdMode->setContentsMargins(0, 0, 0, 0);
        thresholdModeLabel = new QLabel(searchTemplateCard);
        thresholdModeLabel->setObjectName(QString::fromUtf8("thresholdModeLabel"));
        thresholdModeLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_thresholdMode->addWidget(thresholdModeLabel);

        thresholdModeFrame = new QFrame(searchTemplateCard);
        thresholdModeFrame->setObjectName(QString::fromUtf8("thresholdModeFrame"));
        thresholdModeFrame->setMinimumSize(QSize(260, 38));
        thresholdModeFrame->setMaximumSize(QSize(260, 38));
        horizontalLayout_thresholdModeButtons = new QHBoxLayout(thresholdModeFrame);
        horizontalLayout_thresholdModeButtons->setSpacing(0);
        horizontalLayout_thresholdModeButtons->setObjectName(QString::fromUtf8("horizontalLayout_thresholdModeButtons"));
        horizontalLayout_thresholdModeButtons->setContentsMargins(0, 0, 0, 0);
        thresholdManualButton = new QPushButton(thresholdModeFrame);
        thresholdManualButton->setObjectName(QString::fromUtf8("thresholdManualButton"));
        thresholdManualButton->setCheckable(true);

        horizontalLayout_thresholdModeButtons->addWidget(thresholdManualButton);

        thresholdAutoButton = new QPushButton(thresholdModeFrame);
        thresholdAutoButton->setObjectName(QString::fromUtf8("thresholdAutoButton"));
        thresholdAutoButton->setCheckable(true);
        thresholdAutoButton->setChecked(true);

        horizontalLayout_thresholdModeButtons->addWidget(thresholdAutoButton);


        horizontalLayout_thresholdMode->addWidget(thresholdModeFrame);


        verticalLayout_searchTemplate->addLayout(horizontalLayout_thresholdMode);

        thresholdGrayRow = new QWidget(searchTemplateCard);
        thresholdGrayRow->setObjectName(QString::fromUtf8("thresholdGrayRow"));
        horizontalLayout_grayThreshold = new QHBoxLayout(thresholdGrayRow);
        horizontalLayout_grayThreshold->setSpacing(0);
        horizontalLayout_grayThreshold->setObjectName(QString::fromUtf8("horizontalLayout_grayThreshold"));
        horizontalLayout_grayThreshold->setContentsMargins(0, 0, 0, 0);
        grayThresholdLabel = new QLabel(thresholdGrayRow);
        grayThresholdLabel->setObjectName(QString::fromUtf8("grayThresholdLabel"));
        grayThresholdLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_grayThreshold->addWidget(grayThresholdLabel);

        grayThresholdSpinBox = new QSpinBox(thresholdGrayRow);
        grayThresholdSpinBox->setObjectName(QString::fromUtf8("grayThresholdSpinBox"));
        grayThresholdSpinBox->setMinimumSize(QSize(260, 38));
        grayThresholdSpinBox->setMinimum(0);
        grayThresholdSpinBox->setMaximum(255);
        grayThresholdSpinBox->setValue(15);

        horizontalLayout_grayThreshold->addWidget(grayThresholdSpinBox);


        verticalLayout_searchTemplate->addWidget(thresholdGrayRow);

        horizontalLayout_chainMode = new QHBoxLayout();
        horizontalLayout_chainMode->setObjectName(QString::fromUtf8("horizontalLayout_chainMode"));
        horizontalLayout_chainMode->setContentsMargins(0, 0, 0, 0);
        chainModeLabel = new QLabel(searchTemplateCard);
        chainModeLabel->setObjectName(QString::fromUtf8("chainModeLabel"));
        chainModeLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_chainMode->addWidget(chainModeLabel);

        chainModeFrame = new QFrame(searchTemplateCard);
        chainModeFrame->setObjectName(QString::fromUtf8("chainModeFrame"));
        chainModeFrame->setMinimumSize(QSize(260, 38));
        chainModeFrame->setMaximumSize(QSize(260, 38));
        horizontalLayout_chainModeButtons = new QHBoxLayout(chainModeFrame);
        horizontalLayout_chainModeButtons->setSpacing(0);
        horizontalLayout_chainModeButtons->setObjectName(QString::fromUtf8("horizontalLayout_chainModeButtons"));
        horizontalLayout_chainModeButtons->setContentsMargins(0, 0, 0, 0);
        chainManualButton = new QPushButton(chainModeFrame);
        chainManualButton->setObjectName(QString::fromUtf8("chainManualButton"));
        chainManualButton->setCheckable(true);

        horizontalLayout_chainModeButtons->addWidget(chainManualButton);

        chainAutoButton = new QPushButton(chainModeFrame);
        chainAutoButton->setObjectName(QString::fromUtf8("chainAutoButton"));
        chainAutoButton->setCheckable(true);
        chainAutoButton->setChecked(true);

        horizontalLayout_chainModeButtons->addWidget(chainAutoButton);


        horizontalLayout_chainMode->addWidget(chainModeFrame);


        verticalLayout_searchTemplate->addLayout(horizontalLayout_chainMode);

        chainMinRow = new QWidget(searchTemplateCard);
        chainMinRow->setObjectName(QString::fromUtf8("chainMinRow"));
        horizontalLayout_chainMin = new QHBoxLayout(chainMinRow);
        horizontalLayout_chainMin->setSpacing(0);
        horizontalLayout_chainMin->setObjectName(QString::fromUtf8("horizontalLayout_chainMin"));
        horizontalLayout_chainMin->setContentsMargins(0, 0, 0, 0);
        minChainLengthLabel = new QLabel(chainMinRow);
        minChainLengthLabel->setObjectName(QString::fromUtf8("minChainLengthLabel"));
        minChainLengthLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_chainMin->addWidget(minChainLengthLabel);

        minChainLengthSpinBox = new QSpinBox(chainMinRow);
        minChainLengthSpinBox->setObjectName(QString::fromUtf8("minChainLengthSpinBox"));
        minChainLengthSpinBox->setMinimumSize(QSize(260, 38));
        minChainLengthSpinBox->setMinimum(1);
        minChainLengthSpinBox->setMaximum(9999);
        minChainLengthSpinBox->setValue(4);

        horizontalLayout_chainMin->addWidget(minChainLengthSpinBox);


        verticalLayout_searchTemplate->addWidget(chainMinRow);


        verticalLayout_configCards->addWidget(searchTemplateCard);

        detectionAreaCard = new QFrame(contourConfigContents);
        detectionAreaCard->setObjectName(QString::fromUtf8("detectionAreaCard"));
        detectionAreaCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_detectionArea = new QVBoxLayout(detectionAreaCard);
        verticalLayout_detectionArea->setSpacing(16);
        verticalLayout_detectionArea->setObjectName(QString::fromUtf8("verticalLayout_detectionArea"));
        verticalLayout_detectionArea->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_detectionHeader = new QHBoxLayout();
        horizontalLayout_detectionHeader->setObjectName(QString::fromUtf8("horizontalLayout_detectionHeader"));
        detectionAreaTitleLabel = new QLabel(detectionAreaCard);
        detectionAreaTitleLabel->setObjectName(QString::fromUtf8("detectionAreaTitleLabel"));

        horizontalLayout_detectionHeader->addWidget(detectionAreaTitleLabel);

        horizontalSpacer_detectionHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_detectionHeader->addItem(horizontalSpacer_detectionHeader);

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
        detectionDrawButton->setMinimumSize(QSize(86, 38));
        detectionDrawButton->setIcon(icon4);
        detectionDrawButton->setIconSize(QSize(22, 22));
        detectionDrawButton->setCheckable(true);
        detectionDrawButton->setChecked(true);

        horizontalLayout_detectionAreaTools->addWidget(detectionDrawButton);

        detectionRectButton = new QToolButton(detectionAreaCard);
        detectionRectButton->setObjectName(QString::fromUtf8("detectionRectButton"));
        detectionRectButton->setMinimumSize(QSize(86, 38));
        detectionRectButton->setCheckable(true);

        horizontalLayout_detectionAreaTools->addWidget(detectionRectButton);

        detectionCircleButton = new QToolButton(detectionAreaCard);
        detectionCircleButton->setObjectName(QString::fromUtf8("detectionCircleButton"));
        detectionCircleButton->setMinimumSize(QSize(86, 38));
        detectionCircleButton->setCheckable(true);

        horizontalLayout_detectionAreaTools->addWidget(detectionCircleButton);

        detectionPolygonButton = new QToolButton(detectionAreaCard);
        detectionPolygonButton->setObjectName(QString::fromUtf8("detectionPolygonButton"));
        detectionPolygonButton->setMinimumSize(QSize(86, 38));
        detectionPolygonButton->setCheckable(true);

        horizontalLayout_detectionAreaTools->addWidget(detectionPolygonButton);


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


        verticalLayout_configCards->addWidget(detectionAreaCard);

        recognitionSettingsCard = new QFrame(contourConfigContents);
        recognitionSettingsCard->setObjectName(QString::fromUtf8("recognitionSettingsCard"));
        recognitionSettingsCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_recognitionSettings = new QVBoxLayout(recognitionSettingsCard);
        verticalLayout_recognitionSettings->setSpacing(16);
        verticalLayout_recognitionSettings->setObjectName(QString::fromUtf8("verticalLayout_recognitionSettings"));
        verticalLayout_recognitionSettings->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_recognitionHeader = new QHBoxLayout();
        horizontalLayout_recognitionHeader->setObjectName(QString::fromUtf8("horizontalLayout_recognitionHeader"));
        recognitionSettingsTitleLabel = new QLabel(recognitionSettingsCard);
        recognitionSettingsTitleLabel->setObjectName(QString::fromUtf8("recognitionSettingsTitleLabel"));

        horizontalLayout_recognitionHeader->addWidget(recognitionSettingsTitleLabel);

        horizontalSpacer_recognitionHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_recognitionHeader->addItem(horizontalSpacer_recognitionHeader);

        recognitionCollapseButton = new QToolButton(recognitionSettingsCard);
        recognitionCollapseButton->setObjectName(QString::fromUtf8("recognitionCollapseButton"));

        horizontalLayout_recognitionHeader->addWidget(recognitionCollapseButton);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_recognitionHeader);

        horizontalLayout_minScore = new QHBoxLayout();
        horizontalLayout_minScore->setObjectName(QString::fromUtf8("horizontalLayout_minScore"));
        horizontalLayout_minScore->setContentsMargins(0, 0, 0, 0);
        minScoreLabel = new QLabel(recognitionSettingsCard);
        minScoreLabel->setObjectName(QString::fromUtf8("minScoreLabel"));
        minScoreLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_minScore->addWidget(minScoreLabel);

        minScoreSpinBox = new QSpinBox(recognitionSettingsCard);
        minScoreSpinBox->setObjectName(QString::fromUtf8("minScoreSpinBox"));
        minScoreSpinBox->setMinimumSize(QSize(260, 38));
        minScoreSpinBox->setMinimum(0);
        minScoreSpinBox->setMaximum(100);
        minScoreSpinBox->setValue(50);

        horizontalLayout_minScore->addWidget(minScoreSpinBox);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_minScore);

        horizontalLayout_matchPolarity = new QHBoxLayout();
        horizontalLayout_matchPolarity->setObjectName(QString::fromUtf8("horizontalLayout_matchPolarity"));
        horizontalLayout_matchPolarity->setContentsMargins(0, 0, 0, 0);
        matchPolarityLabel = new QLabel(recognitionSettingsCard);
        matchPolarityLabel->setObjectName(QString::fromUtf8("matchPolarityLabel"));
        matchPolarityLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_matchPolarity->addWidget(matchPolarityLabel);

        matchPolarityComboBox = new QComboBox(recognitionSettingsCard);
        matchPolarityComboBox->addItem(QString());
        matchPolarityComboBox->addItem(QString());
        matchPolarityComboBox->setObjectName(QString::fromUtf8("matchPolarityComboBox"));
        matchPolarityComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_matchPolarity->addWidget(matchPolarityComboBox);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_matchPolarity);

        horizontalLayout_thresholdType = new QHBoxLayout();
        horizontalLayout_thresholdType->setObjectName(QString::fromUtf8("horizontalLayout_thresholdType"));
        horizontalLayout_thresholdType->setContentsMargins(0, 0, 0, 0);
        thresholdTypeLabel = new QLabel(recognitionSettingsCard);
        thresholdTypeLabel->setObjectName(QString::fromUtf8("thresholdTypeLabel"));
        thresholdTypeLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_thresholdType->addWidget(thresholdTypeLabel);

        thresholdTypeComboBox = new QComboBox(recognitionSettingsCard);
        thresholdTypeComboBox->addItem(QString());
        thresholdTypeComboBox->addItem(QString());
        thresholdTypeComboBox->addItem(QString());
        thresholdTypeComboBox->setObjectName(QString::fromUtf8("thresholdTypeComboBox"));
        thresholdTypeComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_thresholdType->addWidget(thresholdTypeComboBox);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_thresholdType);

        horizontalLayout_scaleRange = new QHBoxLayout();
        horizontalLayout_scaleRange->setObjectName(QString::fromUtf8("horizontalLayout_scaleRange"));
        horizontalLayout_scaleRange->setContentsMargins(0, 0, 0, 0);
        scaleRangeLabel = new QLabel(recognitionSettingsCard);
        scaleRangeLabel->setObjectName(QString::fromUtf8("scaleRangeLabel"));
        scaleRangeLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_scaleRange->addWidget(scaleRangeLabel);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_scaleRange->addItem(horizontalSpacer);

        minScaleSpinBox = new QSpinBox(recognitionSettingsCard);
        minScaleSpinBox->setObjectName(QString::fromUtf8("minScaleSpinBox"));
        QSizePolicy sizePolicy2(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(minScaleSpinBox->sizePolicy().hasHeightForWidth());
        minScaleSpinBox->setSizePolicy(sizePolicy2);
        minScaleSpinBox->setMinimumSize(QSize(121, 38));
        minScaleSpinBox->setMaximumSize(QSize(121, 38));
        minScaleSpinBox->setMaximum(999);
        minScaleSpinBox->setValue(100);

        horizontalLayout_scaleRange->addWidget(minScaleSpinBox);

        scaleDashLabel = new QLabel(recognitionSettingsCard);
        scaleDashLabel->setObjectName(QString::fromUtf8("scaleDashLabel"));
        scaleDashLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout_scaleRange->addWidget(scaleDashLabel);

        maxScaleSpinBox = new QSpinBox(recognitionSettingsCard);
        maxScaleSpinBox->setObjectName(QString::fromUtf8("maxScaleSpinBox"));
        sizePolicy2.setHeightForWidth(maxScaleSpinBox->sizePolicy().hasHeightForWidth());
        maxScaleSpinBox->setSizePolicy(sizePolicy2);
        maxScaleSpinBox->setMinimumSize(QSize(121, 38));
        maxScaleSpinBox->setMaximumSize(QSize(121, 38));
        maxScaleSpinBox->setMaximum(999);
        maxScaleSpinBox->setValue(100);

        horizontalLayout_scaleRange->addWidget(maxScaleSpinBox);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_scaleRange);

        horizontalLayout_angleRange = new QHBoxLayout();
        horizontalLayout_angleRange->setObjectName(QString::fromUtf8("horizontalLayout_angleRange"));
        horizontalLayout_angleRange->setContentsMargins(0, 0, 0, 0);
        angleRangeLabel = new QLabel(recognitionSettingsCard);
        angleRangeLabel->setObjectName(QString::fromUtf8("angleRangeLabel"));
        angleRangeLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_angleRange->addWidget(angleRangeLabel);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_angleRange->addItem(horizontalSpacer_2);

        minAngleSpinBox = new QSpinBox(recognitionSettingsCard);
        minAngleSpinBox->setObjectName(QString::fromUtf8("minAngleSpinBox"));
        sizePolicy2.setHeightForWidth(minAngleSpinBox->sizePolicy().hasHeightForWidth());
        minAngleSpinBox->setSizePolicy(sizePolicy2);
        minAngleSpinBox->setMinimumSize(QSize(121, 38));
        minAngleSpinBox->setMaximumSize(QSize(121, 38));
        minAngleSpinBox->setMinimum(-180);
        minAngleSpinBox->setMaximum(180);
        minAngleSpinBox->setValue(-45);

        horizontalLayout_angleRange->addWidget(minAngleSpinBox);

        angleDashLabel = new QLabel(recognitionSettingsCard);
        angleDashLabel->setObjectName(QString::fromUtf8("angleDashLabel"));
        angleDashLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout_angleRange->addWidget(angleDashLabel);

        maxAngleSpinBox = new QSpinBox(recognitionSettingsCard);
        maxAngleSpinBox->setObjectName(QString::fromUtf8("maxAngleSpinBox"));
        sizePolicy2.setHeightForWidth(maxAngleSpinBox->sizePolicy().hasHeightForWidth());
        maxAngleSpinBox->setSizePolicy(sizePolicy2);
        maxAngleSpinBox->setMinimumSize(QSize(121, 38));
        maxAngleSpinBox->setMaximumSize(QSize(121, 38));
        maxAngleSpinBox->setMinimum(-180);
        maxAngleSpinBox->setMaximum(180);
        maxAngleSpinBox->setValue(45);

        horizontalLayout_angleRange->addWidget(maxAngleSpinBox);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_angleRange);

        horizontalLayout_timeout = new QHBoxLayout();
        horizontalLayout_timeout->setObjectName(QString::fromUtf8("horizontalLayout_timeout"));
        horizontalLayout_timeout->setContentsMargins(0, 0, 0, 0);
        timeoutLabel = new QLabel(recognitionSettingsCard);
        timeoutLabel->setObjectName(QString::fromUtf8("timeoutLabel"));
        timeoutLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_timeout->addWidget(timeoutLabel);

        timeoutSpinBox = new QSpinBox(recognitionSettingsCard);
        timeoutSpinBox->setObjectName(QString::fromUtf8("timeoutSpinBox"));
        timeoutSpinBox->setMinimumSize(QSize(260, 38));
        timeoutSpinBox->setMaximum(99999);
        timeoutSpinBox->setValue(2000);

        horizontalLayout_timeout->addWidget(timeoutSpinBox);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_timeout);

        horizontalLayout_showContour = new QHBoxLayout();
        horizontalLayout_showContour->setObjectName(QString::fromUtf8("horizontalLayout_showContour"));
        horizontalLayout_showContour->setContentsMargins(0, 0, 0, 0);
        showContourLabel = new QLabel(recognitionSettingsCard);
        showContourLabel->setObjectName(QString::fromUtf8("showContourLabel"));
        showContourLabel->setMinimumSize(QSize(260, 0));

        horizontalLayout_showContour->addWidget(showContourLabel);

        horizontalSpacer_showContour = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_showContour->addItem(horizontalSpacer_showContour);

        showContourSwitch = new QCheckBox(recognitionSettingsCard);
        showContourSwitch->setObjectName(QString::fromUtf8("showContourSwitch"));

        horizontalLayout_showContour->addWidget(showContourSwitch);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_showContour);

        horizontalLayout_sortMode = new QHBoxLayout();
        horizontalLayout_sortMode->setObjectName(QString::fromUtf8("horizontalLayout_sortMode"));
        horizontalLayout_sortMode->setContentsMargins(0, 0, 0, 0);
        sortModeLabel = new QLabel(recognitionSettingsCard);
        sortModeLabel->setObjectName(QString::fromUtf8("sortModeLabel"));
        sortModeLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_sortMode->addWidget(sortModeLabel);

        sortModeComboBox = new QComboBox(recognitionSettingsCard);
        sortModeComboBox->addItem(QString());
        sortModeComboBox->addItem(QString());
        sortModeComboBox->addItem(QString());
        sortModeComboBox->addItem(QString());
        sortModeComboBox->addItem(QString());
        sortModeComboBox->addItem(QString());
        sortModeComboBox->addItem(QString());
        sortModeComboBox->addItem(QString());
        sortModeComboBox->addItem(QString());
        sortModeComboBox->setObjectName(QString::fromUtf8("sortModeComboBox"));
        sortModeComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_sortMode->addWidget(sortModeComboBox);


        verticalLayout_recognitionSettings->addLayout(horizontalLayout_sortMode);


        verticalLayout_configCards->addWidget(recognitionSettingsCard);

        resultJudgeCard = new QFrame(contourConfigContents);
        resultJudgeCard->setObjectName(QString::fromUtf8("resultJudgeCard"));
        resultJudgeCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_resultJudge = new QVBoxLayout(resultJudgeCard);
        verticalLayout_resultJudge->setSpacing(16);
        verticalLayout_resultJudge->setObjectName(QString::fromUtf8("verticalLayout_resultJudge"));
        verticalLayout_resultJudge->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_resultHeader = new QHBoxLayout();
        horizontalLayout_resultHeader->setObjectName(QString::fromUtf8("horizontalLayout_resultHeader"));
        resultJudgeTitleLabel = new QLabel(resultJudgeCard);
        resultJudgeTitleLabel->setObjectName(QString::fromUtf8("resultJudgeTitleLabel"));

        horizontalLayout_resultHeader->addWidget(resultJudgeTitleLabel);

        horizontalSpacer_resultHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_resultHeader->addItem(horizontalSpacer_resultHeader);

        resultCollapseButton = new QToolButton(resultJudgeCard);
        resultCollapseButton->setObjectName(QString::fromUtf8("resultCollapseButton"));

        horizontalLayout_resultHeader->addWidget(resultCollapseButton);


        verticalLayout_resultJudge->addLayout(horizontalLayout_resultHeader);

        horizontalLayout_resultBasis = new QHBoxLayout();
        horizontalLayout_resultBasis->setObjectName(QString::fromUtf8("horizontalLayout_resultBasis"));
        horizontalLayout_resultBasis->setContentsMargins(0, 0, 0, 0);
        resultBasisLabel = new QLabel(resultJudgeCard);
        resultBasisLabel->setObjectName(QString::fromUtf8("resultBasisLabel"));
        resultBasisLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_resultBasis->addWidget(resultBasisLabel);

        resultBasisComboBox = new QComboBox(resultJudgeCard);
        resultBasisComboBox->addItem(QString());
        resultBasisComboBox->addItem(QString());
        resultBasisComboBox->setObjectName(QString::fromUtf8("resultBasisComboBox"));
        resultBasisComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_resultBasis->addWidget(resultBasisComboBox);


        verticalLayout_resultJudge->addLayout(horizontalLayout_resultBasis);

        resultBasisStackedWidget = new QStackedWidget(resultJudgeCard);
        resultBasisStackedWidget->setObjectName(QString::fromUtf8("resultBasisStackedWidget"));
        resultPresencePage = new QWidget();
        resultPresencePage->setObjectName(QString::fromUtf8("resultPresencePage"));
        horizontalLayout_resultPresence = new QHBoxLayout(resultPresencePage);
        horizontalLayout_resultPresence->setObjectName(QString::fromUtf8("horizontalLayout_resultPresence"));
        horizontalLayout_resultPresence->setContentsMargins(0, 0, 0, 0);
        resultPresenceLabel = new QLabel(resultPresencePage);
        resultPresenceLabel->setObjectName(QString::fromUtf8("resultPresenceLabel"));
        resultPresenceLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_resultPresence->addWidget(resultPresenceLabel);

        resultPresenceFrame = new QFrame(resultPresencePage);
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


        horizontalLayout_resultPresence->addWidget(resultPresenceFrame);

        resultBasisStackedWidget->addWidget(resultPresencePage);
        resultScorePage = new QWidget();
        resultScorePage->setObjectName(QString::fromUtf8("resultScorePage"));
        horizontalLayout_resultScore = new QHBoxLayout(resultScorePage);
        horizontalLayout_resultScore->setObjectName(QString::fromUtf8("horizontalLayout_resultScore"));
        horizontalLayout_resultScore->setContentsMargins(0, 0, 0, 0);
        resultScoreLabel = new QLabel(resultScorePage);
        resultScoreLabel->setObjectName(QString::fromUtf8("resultScoreLabel"));
        resultScoreLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_resultScore->addWidget(resultScoreLabel);

        resultMinScoreSpinBox = new QSpinBox(resultScorePage);
        resultMinScoreSpinBox->setObjectName(QString::fromUtf8("resultMinScoreSpinBox"));
        resultMinScoreSpinBox->setMinimumSize(QSize(260, 38));
        resultMinScoreSpinBox->setMinimum(0);
        resultMinScoreSpinBox->setMaximum(100);
        resultMinScoreSpinBox->setValue(50);

        horizontalLayout_resultScore->addWidget(resultMinScoreSpinBox);

        resultBasisStackedWidget->addWidget(resultScorePage);

        verticalLayout_resultJudge->addWidget(resultBasisStackedWidget);


        verticalLayout_configCards->addWidget(resultJudgeCard);

        verticalSpacer_configCards = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_configCards->addItem(verticalSpacer_configCards);

        contourConfigScrollArea->setWidget(contourConfigContents);

        verticalLayout_allParamsPage->addWidget(contourConfigScrollArea);

        contourParamsStackedWidget->addWidget(allParamsPage);

        verticalLayout_editor->addWidget(contourParamsStackedWidget);

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


        retranslateUi(ContourPresenceDialog);

        contourParamsStackedWidget->setCurrentIndex(1);
        basicResultBasisStackedWidget->setCurrentIndex(0);
        matchPolarityComboBox->setCurrentIndex(1);
        sortModeComboBox->setCurrentIndex(4);
        resultBasisStackedWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(ContourPresenceDialog);
    } // setupUi

    void retranslateUi(QDialog *ContourPresenceDialog)
    {
        ContourPresenceDialog->setWindowTitle(QCoreApplication::translate("ContourPresenceDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \350\275\256\345\273\223\346\234\211\346\227\240", nullptr));
        setupWindowTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\303\227", nullptr));
        setupPageCodeLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "422", nullptr));
        setupSaveButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\344\277\235\345\255\230", nullptr));
        setupSaveAsButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\217\246\345\255\230\344\270\272", nullptr));
        setupExportButton->setText(QCoreApplication::translate("ContourPresenceDialog", "IO\350\276\223\345\207\272", nullptr));
        editorTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\350\275\256\345\273\223\346\234\211\346\227\240", nullptr));
        basicSegmentButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\237\272\347\241\200", nullptr));
        allSegmentButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\205\250\351\203\250", nullptr));
        basicSearchTemplateCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "configCard", nullptr)));
        basicSearchTemplateTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\220\234\347\264\242\346\250\241\346\235\277", nullptr));
        basicSearchTemplateTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "cardTitle", nullptr)));
        basicTemplateCollapseButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\214\204", nullptr));
        basicTemplateCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "collapseCard", nullptr)));
        basicTemplateAreaLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\250\241\346\235\277\345\214\272\345\237\237", nullptr));
        basicTemplateAreaLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        basicTemplateRectButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\226\241", nullptr));
        basicTemplateRectButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "toolbarIcon", nullptr)));
        basicTemplatePolygonButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\254\241", nullptr));
        basicTemplatePolygonButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "toolbarIcon", nullptr)));
        basicTemplateFinishButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\256\214\346\210\220", nullptr));
        basicTemplateFinishButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "highlight", nullptr)));
        basicDetectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "configCard", nullptr)));
        basicDetectionAreaTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        basicDetectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "cardTitle", nullptr)));
        basicDetectionCollapseButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\214\204", nullptr));
        basicDetectionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "collapseCard", nullptr)));
        basicDetectionAreaLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\243\200\346\265\213\345\214\272", nullptr));
        basicDetectionAreaLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        basicDetectionRectButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\226\241", nullptr));
        basicDetectionCircleButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\227\213", nullptr));
        basicDetectionPolygonButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\254\241", nullptr));
        basicPositionEnableLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243\344\275\277\350\203\275 \342\223\230", nullptr));
        basicPositionEnableLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        basicPositionCorrectionSwitch->setText(QString());
        basicPositionSourceLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        basicPositionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        basicPositionCorrectionComboBox->setItemText(0, QCoreApplication::translate("ContourPresenceDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        basicRecognitionSettingsCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "configCard", nullptr)));
        basicRecognitionSettingsTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\350\257\206\345\210\253\350\256\276\347\275\256", nullptr));
        basicRecognitionSettingsTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "cardTitle", nullptr)));
        basicRecognitionCollapseButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\214\204", nullptr));
        basicRecognitionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "collapseCard", nullptr)));
        basicShowContourLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\350\277\220\350\241\214\346\230\276\347\244\272\350\275\256\345\273\223\347\202\271", nullptr));
        basicShowContourLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        basicShowContourSwitch->setText(QString());
        basicResultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "configCard", nullptr)));
        basicResultJudgeTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        basicResultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "cardTitle", nullptr)));
        basicResultCollapseButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\214\204", nullptr));
        basicResultCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "collapseCard", nullptr)));
        basicResultBasisLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\210\244\346\226\255\344\276\235\346\215\256", nullptr));
        basicResultBasisLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        basicResultBasisComboBox->setItemText(0, QCoreApplication::translate("ContourPresenceDialog", "\347\273\223\346\236\234\346\234\211\346\227\240", nullptr));
        basicResultBasisComboBox->setItemText(1, QCoreApplication::translate("ContourPresenceDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));

        basicResultPresenceLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\273\223\346\236\234\346\234\211\346\227\240", nullptr));
        basicResultPresenceLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        basicResultPresenceFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "optionGroup", nullptr)));
        basicPresentOkButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\255\230\345\234\250OK", nullptr));
        basicAbsentOkButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\344\270\215\345\255\230\345\234\250OK", nullptr));
        basicResultScoreLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));
        basicResultScoreLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        searchTemplateCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "configCard", nullptr)));
        searchTemplateTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\220\234\347\264\242\346\250\241\346\235\277", nullptr));
        searchTemplateTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "cardTitle", nullptr)));
        templateCollapseButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\214\204", nullptr));
        templateCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "collapseCard", nullptr)));
        templateAreaLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\250\241\346\235\277\345\214\272\345\237\237", nullptr));
        templateAreaLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        templateRectButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\226\241", nullptr));
        templateRectButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "toolbarIcon", nullptr)));
        templatePolygonButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\254\241", nullptr));
        templatePolygonButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "toolbarIcon", nullptr)));
        templateFinishButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\256\214\346\210\220", nullptr));
        templateFinishButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "highlight", nullptr)));
        templateMaskLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\250\241\346\235\277\345\261\217\350\224\275\345\214\272\345\237\237", nullptr));
        templateMaskLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        templateMaskEditButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\274\226\350\276\221", nullptr));
        templateMaskEditButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "gray", nullptr)));
        scaleModeLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\260\272\345\272\246\346\250\241\345\274\217", nullptr));
        scaleModeLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        scaleModeFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "optionGroup", nullptr)));
        scaleManualButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\211\213\345\212\250\346\250\241\345\274\217", nullptr));
        scaleAutoButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\350\207\252\345\212\250", nullptr));
        speedScaleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\351\200\237\345\272\246\345\260\272\345\272\246 \342\223\230", nullptr));
        speedScaleLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        featureScaleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\211\271\345\276\201\345\260\272\345\272\246 \342\223\230", nullptr));
        featureScaleLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        thresholdModeLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\351\230\210\345\200\274\346\250\241\345\274\217", nullptr));
        thresholdModeLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        thresholdModeFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "optionGroup", nullptr)));
        thresholdManualButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\211\213\345\212\250\346\250\241\345\274\217", nullptr));
        thresholdAutoButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\350\207\252\345\212\250", nullptr));
        grayThresholdLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\201\260\345\272\246\351\230\210\345\200\274 \342\223\230", nullptr));
        grayThresholdLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        chainModeLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\351\223\276\351\225\277\346\250\241\345\274\217", nullptr));
        chainModeLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        chainModeFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "optionGroup", nullptr)));
        chainManualButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\211\213\345\212\250\346\250\241\345\274\217", nullptr));
        chainAutoButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\350\207\252\345\212\250", nullptr));
        minChainLengthLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\234\200\345\260\217\351\223\276\351\225\277 \342\223\230", nullptr));
        minChainLengthLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        detectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "configCard", nullptr)));
        detectionAreaTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        detectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "cardTitle", nullptr)));
        detectionCollapseButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\214\204", nullptr));
        detectionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "collapseCard", nullptr)));
        detectionAreaLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\243\200\346\265\213\345\214\272", nullptr));
        detectionAreaLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        detectionRectButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\226\241", nullptr));
        detectionCircleButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\227\213", nullptr));
        detectionPolygonButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\254\241", nullptr));
        detectionMaskLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\261\217\350\224\275\345\214\272\345\237\237", nullptr));
        detectionMaskLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        detectionMaskEditButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\274\226\350\276\221", nullptr));
        detectionMaskEditButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "gray", nullptr)));
        positionEnableLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243\344\275\277\350\203\275 \342\223\230", nullptr));
        positionEnableLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        positionCorrectionSwitch->setText(QString());
        positionSourceLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        positionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        positionCorrectionComboBox->setItemText(0, QCoreApplication::translate("ContourPresenceDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        recognitionSettingsCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "configCard", nullptr)));
        recognitionSettingsTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\350\257\206\345\210\253\350\256\276\347\275\256", nullptr));
        recognitionSettingsTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "cardTitle", nullptr)));
        recognitionCollapseButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\214\204", nullptr));
        recognitionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "collapseCard", nullptr)));
        minScoreLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\234\200\345\260\217\345\276\227\345\210\206", nullptr));
        minScoreLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        matchPolarityLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\214\271\351\205\215\346\236\201\346\200\247 \342\223\230", nullptr));
        matchPolarityLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        matchPolarityComboBox->setItemText(0, QCoreApplication::translate("ContourPresenceDialog", "\344\270\215\350\200\203\350\231\221\346\236\201\346\200\247", nullptr));
        matchPolarityComboBox->setItemText(1, QCoreApplication::translate("ContourPresenceDialog", "\350\200\203\350\231\221\346\236\201\346\200\247", nullptr));

        thresholdTypeLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\351\230\210\345\200\274\347\261\273\345\236\213", nullptr));
        thresholdTypeLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        thresholdTypeComboBox->setItemText(0, QCoreApplication::translate("ContourPresenceDialog", "\350\207\252\345\212\250\351\230\210\345\200\274", nullptr));
        thresholdTypeComboBox->setItemText(1, QCoreApplication::translate("ContourPresenceDialog", "\346\250\241\346\235\277\351\230\210\345\200\274", nullptr));
        thresholdTypeComboBox->setItemText(2, QCoreApplication::translate("ContourPresenceDialog", "\346\211\213\345\212\250\351\230\210\345\200\274", nullptr));

        scaleRangeLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\260\272\345\272\246\350\214\203\345\233\264 \342\223\230", nullptr));
        scaleRangeLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        scaleDashLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "-", nullptr));
        angleRangeLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\350\247\222\345\272\246\350\214\203\345\233\264 \342\223\230", nullptr));
        angleRangeLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        angleDashLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "-", nullptr));
        timeoutLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\256\227\346\263\225\350\266\205\346\227\266\346\227\266\351\227\264(ms)", nullptr));
        timeoutLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        showContourLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\350\277\220\350\241\214\346\230\276\347\244\272\350\275\256\345\273\223\347\202\271", nullptr));
        showContourLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        showContourSwitch->setText(QString());
        sortModeLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\216\222\345\272\217\346\226\271\345\274\217", nullptr));
        sortModeLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        sortModeComboBox->setItemText(0, QCoreApplication::translate("ContourPresenceDialog", "\346\214\211X\345\235\220\346\240\207\344\273\216\345\260\217\345\210\260\345\244\247\346\216\222\345\272\217", nullptr));
        sortModeComboBox->setItemText(1, QCoreApplication::translate("ContourPresenceDialog", "\346\214\211X\345\235\220\346\240\207\344\273\216\345\244\247\345\210\260\345\260\217\346\216\222\345\272\217", nullptr));
        sortModeComboBox->setItemText(2, QCoreApplication::translate("ContourPresenceDialog", "\346\214\211Y\345\235\220\346\240\207\344\273\216\345\260\217\345\210\260\345\244\247\346\216\222\345\272\217", nullptr));
        sortModeComboBox->setItemText(3, QCoreApplication::translate("ContourPresenceDialog", "\346\214\211Y\345\235\220\346\240\207\344\273\216\345\244\247\345\210\260\345\260\217\346\216\222\345\272\217", nullptr));
        sortModeComboBox->setItemText(4, QCoreApplication::translate("ContourPresenceDialog", "\346\214\211\345\276\227\345\210\206\344\273\216\345\260\217\345\210\260\345\244\247\346\216\222\345\272\217", nullptr));
        sortModeComboBox->setItemText(5, QCoreApplication::translate("ContourPresenceDialog", "\346\214\211\345\276\227\345\210\206\344\273\216\345\244\247\345\210\260\345\260\217\346\216\222\345\272\217", nullptr));
        sortModeComboBox->setItemText(6, QCoreApplication::translate("ContourPresenceDialog", "\346\214\211\350\247\222\345\272\246\344\273\216\345\260\217\345\210\260\345\244\247\346\216\222\345\272\217", nullptr));
        sortModeComboBox->setItemText(7, QCoreApplication::translate("ContourPresenceDialog", "\346\214\211\350\247\222\345\272\246\344\273\216\345\244\247\345\210\260\345\260\217\346\216\222\345\272\217", nullptr));
        sortModeComboBox->setItemText(8, QCoreApplication::translate("ContourPresenceDialog", "\346\214\211ROI\345\272\217\345\217\267\346\216\222\345\272\217", nullptr));

        resultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "configCard", nullptr)));
        resultJudgeTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        resultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "cardTitle", nullptr)));
        resultCollapseButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\342\214\204", nullptr));
        resultCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "collapseCard", nullptr)));
        resultBasisLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\210\244\346\226\255\344\276\235\346\215\256", nullptr));
        resultBasisLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        resultBasisComboBox->setItemText(0, QCoreApplication::translate("ContourPresenceDialog", "\347\273\223\346\236\234\346\234\211\346\227\240", nullptr));
        resultBasisComboBox->setItemText(1, QCoreApplication::translate("ContourPresenceDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));

        resultPresenceLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\273\223\346\236\234\346\234\211\346\227\240", nullptr));
        resultPresenceLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        resultPresenceFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "optionGroup", nullptr)));
        presentOkButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\255\230\345\234\250OK", nullptr));
        absentOkButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\344\270\215\345\255\230\345\234\250OK", nullptr));
        resultScoreLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));
        resultScoreLabel->setProperty("role", QVariant(QCoreApplication::translate("ContourPresenceDialog", "rowField", nullptr)));
        referenceTestButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\237\272\345\207\206\345\233\276\346\265\213\350\257\225", nullptr));
        referenceTestButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "secondary", nullptr)));
        testRunButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\346\265\213\350\257\225\350\277\220\350\241\214", nullptr));
        testRunButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ContourPresenceDialog", "secondary", nullptr)));
        finishButton->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\256\214\346\210\220", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerZoomOutButton->setText(QCoreApplication::translate("ContourPresenceDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "39%", nullptr));
        viewerStatusLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: --ms   \345\267\245\345\205\267\350\200\227\346\227\266: --ms", nullptr));
        viewerCursorLabel->setText(QCoreApplication::translate("ContourPresenceDialog", "X: --  Y: ---   |   R: -- G: -- B: --", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ContourPresenceDialog: public Ui_ContourPresenceDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONTOURPRESENCEDIALOG_H
