/********************************************************************************
** Form generated from reading UI file 'CirclePresenceDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CIRCLEPRESENCEDIALOG_H
#define UI_CIRCLEPRESENCEDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGridLayout>
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

class Ui_CirclePresenceDialog
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
    QToolButton *circleExternalEditButton;
    QSpacerItem *horizontalSpacer_editorHeader;
    QFrame *segmentFrame;
    QHBoxLayout *horizontalLayout_segment;
    QPushButton *basicSegmentButton;
    QPushButton *allSegmentButton;
    QStackedWidget *circleParamsStackedWidget;
    QWidget *basicParamsPage;
    QVBoxLayout *verticalLayout_basicParamsPage;
    QScrollArea *circleBasicParamsScrollArea;
    QWidget *circleBasicParamsContents;
    QVBoxLayout *verticalLayout_basicConfigCards;
    QFrame *basicDetectionAreaCard;
    QVBoxLayout *basicVerticalLayout_detectionArea;
    QHBoxLayout *basicHorizontalLayout_detectionHeader;
    QLabel *basicDetectionAreaTitleLabel;
    QSpacerItem *basicHorizontalSpacer_detectionHeader;
    QToolButton *basicDetectionCollapseButton;
    QHBoxLayout *basicHorizontalLayout_detectionButtons;
    QToolButton *basicDetectionRectButton;
    QToolButton *basicDetectionCircleButton;
    QToolButton *basicDetectionPolygonButton;
    QToolButton *basicDetectionResetButton;
    QSpacerItem *basicHorizontalSpacer_detectionButtons;
    QPushButton *basicDetectionMaskEditButton;
    QFrame *basicPositionCard;
    QVBoxLayout *basicVerticalLayout_position;
    QLabel *basicPositionTitleLabel;
    QGridLayout *basicGridLayout_position;
    QLabel *basicPositionCorrectionLabel;
    QPushButton *basicPositionCorrectionSwitch;
    QLabel *basicPositionCorrectionSourceLabel;
    QComboBox *basicPositionCorrectionComboBox;
    QFrame *basicParamCard;
    QVBoxLayout *basicVerticalLayout_param;
    QLabel *basicParamTitleLabel;
    QGridLayout *basicGridLayout_param;
    QLabel *circleBasicSensitivityLabel;
    QSpinBox *circleBasicSensitivitySpinBox;
    QFrame *basicResultJudgeCard;
    QVBoxLayout *basicVerticalLayout_resultJudge;
    QLabel *basicResultJudgeTitleLabel;
    QHBoxLayout *basicHorizontalLayout_resultPresenceButtons;
    QPushButton *basicPresentOkButton;
    QPushButton *basicAbsentOkButton;
    QSpacerItem *verticalSpacer_basicConfigCards;
    QWidget *allParamsPage;
    QVBoxLayout *verticalLayout_allParamsPage;
    QScrollArea *circleAllParamsScrollArea;
    QWidget *circleAllParamsContents;
    QVBoxLayout *verticalLayout_allConfigCards;
    QFrame *detectionAreaCard;
    QVBoxLayout *verticalLayout_detectionArea;
    QLabel *detectionAreaTitleLabel;
    QHBoxLayout *horizontalLayout_detectionButtons;
    QToolButton *detectionRectButton;
    QToolButton *detectionCircleButton;
    QToolButton *detectionPolygonButton;
    QToolButton *detectionResetButton;
    QSpacerItem *horizontalSpacer_detectionButtons;
    QPushButton *detectionMaskEditButton;
    QFrame *positionCard;
    QVBoxLayout *verticalLayout_position;
    QLabel *positionTitleLabel;
    QGridLayout *gridLayout_position;
    QLabel *positionCorrectionLabel;
    QPushButton *positionCorrectionSwitch;
    QLabel *positionCorrectionSourceLabel;
    QComboBox *positionCorrectionComboBox;
    QFrame *paramCard;
    QVBoxLayout *verticalLayout_param;
    QLabel *paramTitleLabel;
    QGridLayout *gridLayout_param;
    QLabel *circleSensitivityLabel;
    QSpinBox *circleSensitivitySpinBox;
    QLabel *roundnessLabel;
    QSpinBox *roundnessSpinBox;
    QLabel *edgePolarityLabel;
    QComboBox *edgePolarityComboBox;
    QLabel *edgeTypeLabel;
    QComboBox *edgeTypeComboBox;
    QFrame *resultJudgeCard;
    QVBoxLayout *verticalLayout_resultJudge;
    QLabel *resultJudgeTitleLabel;
    QHBoxLayout *horizontalLayout_resultPresenceButtons;
    QPushButton *presentOkButton;
    QPushButton *absentOkButton;
    QSpacerItem *verticalSpacer_allConfigCards;
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

    void setupUi(QDialog *CirclePresenceDialog)
    {
        if (CirclePresenceDialog->objectName().isEmpty())
            CirclePresenceDialog->setObjectName(QString::fromUtf8("CirclePresenceDialog"));
        CirclePresenceDialog->resize(1760, 980);
        verticalLayout_root = new QVBoxLayout(CirclePresenceDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        setupTopBar = new QFrame(CirclePresenceDialog);
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

        setupToolbar = new QFrame(CirclePresenceDialog);
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

        setupBodyFrame = new QFrame(CirclePresenceDialog);
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

        circleExternalEditButton = new QToolButton(setupEditorPanel);
        circleExternalEditButton->setObjectName(QString::fromUtf8("circleExternalEditButton"));
        circleExternalEditButton->setIcon(icon);
        circleExternalEditButton->setIconSize(QSize(24, 24));
        circleExternalEditButton->setAutoRaise(true);

        horizontalLayout_editorHeader->addWidget(circleExternalEditButton);

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

        circleParamsStackedWidget = new QStackedWidget(setupEditorPanel);
        circleParamsStackedWidget->setObjectName(QString::fromUtf8("circleParamsStackedWidget"));
        basicParamsPage = new QWidget();
        basicParamsPage->setObjectName(QString::fromUtf8("basicParamsPage"));
        verticalLayout_basicParamsPage = new QVBoxLayout(basicParamsPage);
        verticalLayout_basicParamsPage->setSpacing(0);
        verticalLayout_basicParamsPage->setObjectName(QString::fromUtf8("verticalLayout_basicParamsPage"));
        verticalLayout_basicParamsPage->setContentsMargins(0, 0, 0, 0);
        circleBasicParamsScrollArea = new QScrollArea(basicParamsPage);
        circleBasicParamsScrollArea->setObjectName(QString::fromUtf8("circleBasicParamsScrollArea"));
        circleBasicParamsScrollArea->setFrameShape(QFrame::NoFrame);
        circleBasicParamsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        circleBasicParamsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        circleBasicParamsScrollArea->setWidgetResizable(true);
        circleBasicParamsContents = new QWidget();
        circleBasicParamsContents->setObjectName(QString::fromUtf8("circleBasicParamsContents"));
        circleBasicParamsContents->setGeometry(QRect(0, 0, 554, 692));
        verticalLayout_basicConfigCards = new QVBoxLayout(circleBasicParamsContents);
        verticalLayout_basicConfigCards->setSpacing(12);
        verticalLayout_basicConfigCards->setObjectName(QString::fromUtf8("verticalLayout_basicConfigCards"));
        verticalLayout_basicConfigCards->setContentsMargins(0, 0, 0, 0);
        basicDetectionAreaCard = new QFrame(circleBasicParamsContents);
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

        basicHorizontalLayout_detectionButtons = new QHBoxLayout();
        basicHorizontalLayout_detectionButtons->setSpacing(10);
        basicHorizontalLayout_detectionButtons->setObjectName(QString::fromUtf8("basicHorizontalLayout_detectionButtons"));
        basicDetectionRectButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionRectButton->setObjectName(QString::fromUtf8("basicDetectionRectButton"));
        basicDetectionRectButton->setCheckable(true);
        basicDetectionRectButton->setChecked(true);

        basicHorizontalLayout_detectionButtons->addWidget(basicDetectionRectButton);

        basicDetectionCircleButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionCircleButton->setObjectName(QString::fromUtf8("basicDetectionCircleButton"));
        basicDetectionCircleButton->setCheckable(true);

        basicHorizontalLayout_detectionButtons->addWidget(basicDetectionCircleButton);

        basicDetectionPolygonButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionPolygonButton->setObjectName(QString::fromUtf8("basicDetectionPolygonButton"));
        basicDetectionPolygonButton->setCheckable(true);

        basicHorizontalLayout_detectionButtons->addWidget(basicDetectionPolygonButton);

        basicDetectionResetButton = new QToolButton(basicDetectionAreaCard);
        basicDetectionResetButton->setObjectName(QString::fromUtf8("basicDetectionResetButton"));

        basicHorizontalLayout_detectionButtons->addWidget(basicDetectionResetButton);

        basicHorizontalSpacer_detectionButtons = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        basicHorizontalLayout_detectionButtons->addItem(basicHorizontalSpacer_detectionButtons);


        basicVerticalLayout_detectionArea->addLayout(basicHorizontalLayout_detectionButtons);

        basicDetectionMaskEditButton = new QPushButton(basicDetectionAreaCard);
        basicDetectionMaskEditButton->setObjectName(QString::fromUtf8("basicDetectionMaskEditButton"));

        basicVerticalLayout_detectionArea->addWidget(basicDetectionMaskEditButton);


        verticalLayout_basicConfigCards->addWidget(basicDetectionAreaCard);

        basicPositionCard = new QFrame(circleBasicParamsContents);
        basicPositionCard->setObjectName(QString::fromUtf8("basicPositionCard"));
        basicPositionCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_position = new QVBoxLayout(basicPositionCard);
        basicVerticalLayout_position->setSpacing(16);
        basicVerticalLayout_position->setObjectName(QString::fromUtf8("basicVerticalLayout_position"));
        basicVerticalLayout_position->setContentsMargins(20, 18, 20, 18);
        basicPositionTitleLabel = new QLabel(basicPositionCard);
        basicPositionTitleLabel->setObjectName(QString::fromUtf8("basicPositionTitleLabel"));

        basicVerticalLayout_position->addWidget(basicPositionTitleLabel);

        basicGridLayout_position = new QGridLayout();
        basicGridLayout_position->setObjectName(QString::fromUtf8("basicGridLayout_position"));
        basicGridLayout_position->setHorizontalSpacing(14);
        basicGridLayout_position->setVerticalSpacing(12);
        basicPositionCorrectionLabel = new QLabel(basicPositionCard);
        basicPositionCorrectionLabel->setObjectName(QString::fromUtf8("basicPositionCorrectionLabel"));

        basicGridLayout_position->addWidget(basicPositionCorrectionLabel, 0, 0, 1, 1);

        basicPositionCorrectionSwitch = new QPushButton(basicPositionCard);
        basicPositionCorrectionSwitch->setObjectName(QString::fromUtf8("basicPositionCorrectionSwitch"));
        basicPositionCorrectionSwitch->setCheckable(true);

        basicGridLayout_position->addWidget(basicPositionCorrectionSwitch, 0, 1, 1, 1);

        basicPositionCorrectionSourceLabel = new QLabel(basicPositionCard);
        basicPositionCorrectionSourceLabel->setObjectName(QString::fromUtf8("basicPositionCorrectionSourceLabel"));

        basicGridLayout_position->addWidget(basicPositionCorrectionSourceLabel, 1, 0, 1, 1);

        basicPositionCorrectionComboBox = new QComboBox(basicPositionCard);
        basicPositionCorrectionComboBox->addItem(QString());
        basicPositionCorrectionComboBox->setObjectName(QString::fromUtf8("basicPositionCorrectionComboBox"));

        basicGridLayout_position->addWidget(basicPositionCorrectionComboBox, 1, 1, 1, 1);


        basicVerticalLayout_position->addLayout(basicGridLayout_position);


        verticalLayout_basicConfigCards->addWidget(basicPositionCard);

        basicParamCard = new QFrame(circleBasicParamsContents);
        basicParamCard->setObjectName(QString::fromUtf8("basicParamCard"));
        basicParamCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_param = new QVBoxLayout(basicParamCard);
        basicVerticalLayout_param->setSpacing(16);
        basicVerticalLayout_param->setObjectName(QString::fromUtf8("basicVerticalLayout_param"));
        basicVerticalLayout_param->setContentsMargins(20, 18, 20, 18);
        basicParamTitleLabel = new QLabel(basicParamCard);
        basicParamTitleLabel->setObjectName(QString::fromUtf8("basicParamTitleLabel"));

        basicVerticalLayout_param->addWidget(basicParamTitleLabel);

        basicGridLayout_param = new QGridLayout();
        basicGridLayout_param->setObjectName(QString::fromUtf8("basicGridLayout_param"));
        basicGridLayout_param->setHorizontalSpacing(14);
        basicGridLayout_param->setVerticalSpacing(12);
        circleBasicSensitivityLabel = new QLabel(basicParamCard);
        circleBasicSensitivityLabel->setObjectName(QString::fromUtf8("circleBasicSensitivityLabel"));

        basicGridLayout_param->addWidget(circleBasicSensitivityLabel, 0, 0, 1, 1);

        circleBasicSensitivitySpinBox = new QSpinBox(basicParamCard);
        circleBasicSensitivitySpinBox->setObjectName(QString::fromUtf8("circleBasicSensitivitySpinBox"));
        circleBasicSensitivitySpinBox->setMinimum(0);
        circleBasicSensitivitySpinBox->setMaximum(100);
        circleBasicSensitivitySpinBox->setValue(60);

        basicGridLayout_param->addWidget(circleBasicSensitivitySpinBox, 0, 1, 1, 1);


        basicVerticalLayout_param->addLayout(basicGridLayout_param);


        verticalLayout_basicConfigCards->addWidget(basicParamCard);

        basicResultJudgeCard = new QFrame(circleBasicParamsContents);
        basicResultJudgeCard->setObjectName(QString::fromUtf8("basicResultJudgeCard"));
        basicResultJudgeCard->setFrameShape(QFrame::NoFrame);
        basicVerticalLayout_resultJudge = new QVBoxLayout(basicResultJudgeCard);
        basicVerticalLayout_resultJudge->setSpacing(16);
        basicVerticalLayout_resultJudge->setObjectName(QString::fromUtf8("basicVerticalLayout_resultJudge"));
        basicVerticalLayout_resultJudge->setContentsMargins(20, 18, 20, 18);
        basicResultJudgeTitleLabel = new QLabel(basicResultJudgeCard);
        basicResultJudgeTitleLabel->setObjectName(QString::fromUtf8("basicResultJudgeTitleLabel"));

        basicVerticalLayout_resultJudge->addWidget(basicResultJudgeTitleLabel);

        basicHorizontalLayout_resultPresenceButtons = new QHBoxLayout();
        basicHorizontalLayout_resultPresenceButtons->setObjectName(QString::fromUtf8("basicHorizontalLayout_resultPresenceButtons"));
        basicPresentOkButton = new QPushButton(basicResultJudgeCard);
        basicPresentOkButton->setObjectName(QString::fromUtf8("basicPresentOkButton"));
        basicPresentOkButton->setCheckable(true);
        basicPresentOkButton->setChecked(true);

        basicHorizontalLayout_resultPresenceButtons->addWidget(basicPresentOkButton);

        basicAbsentOkButton = new QPushButton(basicResultJudgeCard);
        basicAbsentOkButton->setObjectName(QString::fromUtf8("basicAbsentOkButton"));
        basicAbsentOkButton->setCheckable(true);

        basicHorizontalLayout_resultPresenceButtons->addWidget(basicAbsentOkButton);


        basicVerticalLayout_resultJudge->addLayout(basicHorizontalLayout_resultPresenceButtons);


        verticalLayout_basicConfigCards->addWidget(basicResultJudgeCard);

        verticalSpacer_basicConfigCards = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_basicConfigCards->addItem(verticalSpacer_basicConfigCards);

        circleBasicParamsScrollArea->setWidget(circleBasicParamsContents);

        verticalLayout_basicParamsPage->addWidget(circleBasicParamsScrollArea);

        circleParamsStackedWidget->addWidget(basicParamsPage);
        allParamsPage = new QWidget();
        allParamsPage->setObjectName(QString::fromUtf8("allParamsPage"));
        verticalLayout_allParamsPage = new QVBoxLayout(allParamsPage);
        verticalLayout_allParamsPage->setSpacing(0);
        verticalLayout_allParamsPage->setObjectName(QString::fromUtf8("verticalLayout_allParamsPage"));
        verticalLayout_allParamsPage->setContentsMargins(0, 0, 0, 0);
        circleAllParamsScrollArea = new QScrollArea(allParamsPage);
        circleAllParamsScrollArea->setObjectName(QString::fromUtf8("circleAllParamsScrollArea"));
        circleAllParamsScrollArea->setFrameShape(QFrame::NoFrame);
        circleAllParamsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        circleAllParamsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        circleAllParamsScrollArea->setWidgetResizable(true);
        circleAllParamsContents = new QWidget();
        circleAllParamsContents->setObjectName(QString::fromUtf8("circleAllParamsContents"));
        circleAllParamsContents->setGeometry(QRect(0, 0, 554, 786));
        verticalLayout_allConfigCards = new QVBoxLayout(circleAllParamsContents);
        verticalLayout_allConfigCards->setSpacing(12);
        verticalLayout_allConfigCards->setObjectName(QString::fromUtf8("verticalLayout_allConfigCards"));
        verticalLayout_allConfigCards->setContentsMargins(0, 0, 0, 0);
        detectionAreaCard = new QFrame(circleAllParamsContents);
        detectionAreaCard->setObjectName(QString::fromUtf8("detectionAreaCard"));
        detectionAreaCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_detectionArea = new QVBoxLayout(detectionAreaCard);
        verticalLayout_detectionArea->setSpacing(16);
        verticalLayout_detectionArea->setObjectName(QString::fromUtf8("verticalLayout_detectionArea"));
        verticalLayout_detectionArea->setContentsMargins(20, 18, 20, 18);
        detectionAreaTitleLabel = new QLabel(detectionAreaCard);
        detectionAreaTitleLabel->setObjectName(QString::fromUtf8("detectionAreaTitleLabel"));

        verticalLayout_detectionArea->addWidget(detectionAreaTitleLabel);

        horizontalLayout_detectionButtons = new QHBoxLayout();
        horizontalLayout_detectionButtons->setSpacing(10);
        horizontalLayout_detectionButtons->setObjectName(QString::fromUtf8("horizontalLayout_detectionButtons"));
        detectionRectButton = new QToolButton(detectionAreaCard);
        detectionRectButton->setObjectName(QString::fromUtf8("detectionRectButton"));
        detectionRectButton->setCheckable(true);
        detectionRectButton->setChecked(true);

        horizontalLayout_detectionButtons->addWidget(detectionRectButton);

        detectionCircleButton = new QToolButton(detectionAreaCard);
        detectionCircleButton->setObjectName(QString::fromUtf8("detectionCircleButton"));
        detectionCircleButton->setCheckable(true);

        horizontalLayout_detectionButtons->addWidget(detectionCircleButton);

        detectionPolygonButton = new QToolButton(detectionAreaCard);
        detectionPolygonButton->setObjectName(QString::fromUtf8("detectionPolygonButton"));
        detectionPolygonButton->setCheckable(true);

        horizontalLayout_detectionButtons->addWidget(detectionPolygonButton);

        detectionResetButton = new QToolButton(detectionAreaCard);
        detectionResetButton->setObjectName(QString::fromUtf8("detectionResetButton"));

        horizontalLayout_detectionButtons->addWidget(detectionResetButton);

        horizontalSpacer_detectionButtons = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_detectionButtons->addItem(horizontalSpacer_detectionButtons);


        verticalLayout_detectionArea->addLayout(horizontalLayout_detectionButtons);

        detectionMaskEditButton = new QPushButton(detectionAreaCard);
        detectionMaskEditButton->setObjectName(QString::fromUtf8("detectionMaskEditButton"));

        verticalLayout_detectionArea->addWidget(detectionMaskEditButton);


        verticalLayout_allConfigCards->addWidget(detectionAreaCard);

        positionCard = new QFrame(circleAllParamsContents);
        positionCard->setObjectName(QString::fromUtf8("positionCard"));
        positionCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_position = new QVBoxLayout(positionCard);
        verticalLayout_position->setSpacing(16);
        verticalLayout_position->setObjectName(QString::fromUtf8("verticalLayout_position"));
        verticalLayout_position->setContentsMargins(20, 18, 20, 18);
        positionTitleLabel = new QLabel(positionCard);
        positionTitleLabel->setObjectName(QString::fromUtf8("positionTitleLabel"));

        verticalLayout_position->addWidget(positionTitleLabel);

        gridLayout_position = new QGridLayout();
        gridLayout_position->setObjectName(QString::fromUtf8("gridLayout_position"));
        gridLayout_position->setHorizontalSpacing(14);
        gridLayout_position->setVerticalSpacing(12);
        positionCorrectionLabel = new QLabel(positionCard);
        positionCorrectionLabel->setObjectName(QString::fromUtf8("positionCorrectionLabel"));

        gridLayout_position->addWidget(positionCorrectionLabel, 0, 0, 1, 1);

        positionCorrectionSwitch = new QPushButton(positionCard);
        positionCorrectionSwitch->setObjectName(QString::fromUtf8("positionCorrectionSwitch"));
        positionCorrectionSwitch->setCheckable(true);

        gridLayout_position->addWidget(positionCorrectionSwitch, 0, 1, 1, 1);

        positionCorrectionSourceLabel = new QLabel(positionCard);
        positionCorrectionSourceLabel->setObjectName(QString::fromUtf8("positionCorrectionSourceLabel"));

        gridLayout_position->addWidget(positionCorrectionSourceLabel, 1, 0, 1, 1);

        positionCorrectionComboBox = new QComboBox(positionCard);
        positionCorrectionComboBox->addItem(QString());
        positionCorrectionComboBox->setObjectName(QString::fromUtf8("positionCorrectionComboBox"));

        gridLayout_position->addWidget(positionCorrectionComboBox, 1, 1, 1, 1);


        verticalLayout_position->addLayout(gridLayout_position);


        verticalLayout_allConfigCards->addWidget(positionCard);

        paramCard = new QFrame(circleAllParamsContents);
        paramCard->setObjectName(QString::fromUtf8("paramCard"));
        paramCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_param = new QVBoxLayout(paramCard);
        verticalLayout_param->setSpacing(16);
        verticalLayout_param->setObjectName(QString::fromUtf8("verticalLayout_param"));
        verticalLayout_param->setContentsMargins(20, 18, 20, 18);
        paramTitleLabel = new QLabel(paramCard);
        paramTitleLabel->setObjectName(QString::fromUtf8("paramTitleLabel"));

        verticalLayout_param->addWidget(paramTitleLabel);

        gridLayout_param = new QGridLayout();
        gridLayout_param->setObjectName(QString::fromUtf8("gridLayout_param"));
        gridLayout_param->setHorizontalSpacing(14);
        gridLayout_param->setVerticalSpacing(12);
        circleSensitivityLabel = new QLabel(paramCard);
        circleSensitivityLabel->setObjectName(QString::fromUtf8("circleSensitivityLabel"));

        gridLayout_param->addWidget(circleSensitivityLabel, 0, 0, 1, 1);

        circleSensitivitySpinBox = new QSpinBox(paramCard);
        circleSensitivitySpinBox->setObjectName(QString::fromUtf8("circleSensitivitySpinBox"));
        circleSensitivitySpinBox->setMinimum(0);
        circleSensitivitySpinBox->setMaximum(100);
        circleSensitivitySpinBox->setValue(60);

        gridLayout_param->addWidget(circleSensitivitySpinBox, 0, 1, 1, 1);

        roundnessLabel = new QLabel(paramCard);
        roundnessLabel->setObjectName(QString::fromUtf8("roundnessLabel"));

        gridLayout_param->addWidget(roundnessLabel, 1, 0, 1, 1);

        roundnessSpinBox = new QSpinBox(paramCard);
        roundnessSpinBox->setObjectName(QString::fromUtf8("roundnessSpinBox"));
        roundnessSpinBox->setMinimum(0);
        roundnessSpinBox->setMaximum(100);
        roundnessSpinBox->setValue(25);

        gridLayout_param->addWidget(roundnessSpinBox, 1, 1, 1, 1);

        edgePolarityLabel = new QLabel(paramCard);
        edgePolarityLabel->setObjectName(QString::fromUtf8("edgePolarityLabel"));

        gridLayout_param->addWidget(edgePolarityLabel, 2, 0, 1, 1);

        edgePolarityComboBox = new QComboBox(paramCard);
        edgePolarityComboBox->addItem(QString());
        edgePolarityComboBox->addItem(QString());
        edgePolarityComboBox->addItem(QString());
        edgePolarityComboBox->setObjectName(QString::fromUtf8("edgePolarityComboBox"));

        gridLayout_param->addWidget(edgePolarityComboBox, 2, 1, 1, 1);

        edgeTypeLabel = new QLabel(paramCard);
        edgeTypeLabel->setObjectName(QString::fromUtf8("edgeTypeLabel"));

        gridLayout_param->addWidget(edgeTypeLabel, 3, 0, 1, 1);

        edgeTypeComboBox = new QComboBox(paramCard);
        edgeTypeComboBox->addItem(QString());
        edgeTypeComboBox->addItem(QString());
        edgeTypeComboBox->addItem(QString());
        edgeTypeComboBox->addItem(QString());
        edgeTypeComboBox->setObjectName(QString::fromUtf8("edgeTypeComboBox"));

        gridLayout_param->addWidget(edgeTypeComboBox, 3, 1, 1, 1);


        verticalLayout_param->addLayout(gridLayout_param);


        verticalLayout_allConfigCards->addWidget(paramCard);

        resultJudgeCard = new QFrame(circleAllParamsContents);
        resultJudgeCard->setObjectName(QString::fromUtf8("resultJudgeCard"));
        resultJudgeCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_resultJudge = new QVBoxLayout(resultJudgeCard);
        verticalLayout_resultJudge->setSpacing(16);
        verticalLayout_resultJudge->setObjectName(QString::fromUtf8("verticalLayout_resultJudge"));
        verticalLayout_resultJudge->setContentsMargins(20, 18, 20, 18);
        resultJudgeTitleLabel = new QLabel(resultJudgeCard);
        resultJudgeTitleLabel->setObjectName(QString::fromUtf8("resultJudgeTitleLabel"));

        verticalLayout_resultJudge->addWidget(resultJudgeTitleLabel);

        horizontalLayout_resultPresenceButtons = new QHBoxLayout();
        horizontalLayout_resultPresenceButtons->setObjectName(QString::fromUtf8("horizontalLayout_resultPresenceButtons"));
        presentOkButton = new QPushButton(resultJudgeCard);
        presentOkButton->setObjectName(QString::fromUtf8("presentOkButton"));
        presentOkButton->setCheckable(true);
        presentOkButton->setChecked(true);

        horizontalLayout_resultPresenceButtons->addWidget(presentOkButton);

        absentOkButton = new QPushButton(resultJudgeCard);
        absentOkButton->setObjectName(QString::fromUtf8("absentOkButton"));
        absentOkButton->setCheckable(true);

        horizontalLayout_resultPresenceButtons->addWidget(absentOkButton);


        verticalLayout_resultJudge->addLayout(horizontalLayout_resultPresenceButtons);


        verticalLayout_allConfigCards->addWidget(resultJudgeCard);

        verticalSpacer_allConfigCards = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_allConfigCards->addItem(verticalSpacer_allConfigCards);

        circleAllParamsScrollArea->setWidget(circleAllParamsContents);

        verticalLayout_allParamsPage->addWidget(circleAllParamsScrollArea);

        circleParamsStackedWidget->addWidget(allParamsPage);

        verticalLayout_editor->addWidget(circleParamsStackedWidget);

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


        retranslateUi(CirclePresenceDialog);

        circleParamsStackedWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(CirclePresenceDialog);
    } // setupUi

    void retranslateUi(QDialog *CirclePresenceDialog)
    {
        CirclePresenceDialog->setWindowTitle(QCoreApplication::translate("CirclePresenceDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \345\234\206\346\234\211\346\227\240", nullptr));
        setupWindowTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\303\227", nullptr));
        setupPageCodeLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "428", nullptr));
        setupSaveButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\344\277\235\345\255\230", nullptr));
        setupSaveAsButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\217\246\345\255\230\344\270\272", nullptr));
        setupExportButton->setText(QCoreApplication::translate("CirclePresenceDialog", "IO\350\276\223\345\207\272", nullptr));
        editorTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\234\206\346\234\211\346\227\240", nullptr));
        basicSegmentButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\237\272\347\241\200", nullptr));
        allSegmentButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\205\250\351\203\250", nullptr));
        basicDetectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "configCard", nullptr)));
        basicDetectionAreaTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        basicDetectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "cardTitle", nullptr)));
        basicDetectionCollapseButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\342\214\204", nullptr));
        basicDetectionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "collapseCard", nullptr)));
        basicDetectionRectButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\342\226\241", nullptr));
        basicDetectionRectButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "toolbarIcon", nullptr)));
        basicDetectionCircleButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\342\227\213", nullptr));
        basicDetectionCircleButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "toolbarIcon", nullptr)));
        basicDetectionPolygonButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\342\254\241", nullptr));
        basicDetectionPolygonButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "toolbarIcon", nullptr)));
        basicDetectionResetButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\342\237\263", nullptr));
        basicDetectionResetButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "toolbarIcon", nullptr)));
        basicDetectionMaskEditButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\261\217\350\224\275\345\214\272\345\237\237", nullptr));
        basicDetectionMaskEditButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "secondary", nullptr)));
        basicPositionCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "configCard", nullptr)));
        basicPositionTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        basicPositionTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "cardTitle", nullptr)));
        basicPositionCorrectionLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        basicPositionCorrectionLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "rowField", nullptr)));
        basicPositionCorrectionSwitch->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\220\257\347\224\250", nullptr));
        basicPositionCorrectionSwitch->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "secondary", nullptr)));
        basicPositionCorrectionSourceLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\346\235\245\346\272\220", nullptr));
        basicPositionCorrectionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "rowField", nullptr)));
        basicPositionCorrectionComboBox->setItemText(0, QCoreApplication::translate("CirclePresenceDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        basicParamCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "configCard", nullptr)));
        basicParamTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\217\202\346\225\260", nullptr));
        basicParamTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "cardTitle", nullptr)));
        circleBasicSensitivityLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\347\201\265\346\225\217\345\272\246", nullptr));
        circleBasicSensitivityLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "rowField", nullptr)));
        basicResultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "configCard", nullptr)));
        basicResultJudgeTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        basicResultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "cardTitle", nullptr)));
        basicPresentOkButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\255\230\345\234\250OK", nullptr));
        basicAbsentOkButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\344\270\215\345\255\230\345\234\250OK", nullptr));
        detectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "configCard", nullptr)));
        detectionAreaTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        detectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "cardTitle", nullptr)));
        detectionRectButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\342\226\241", nullptr));
        detectionRectButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "toolbarIcon", nullptr)));
        detectionCircleButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\342\227\213", nullptr));
        detectionCircleButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "toolbarIcon", nullptr)));
        detectionPolygonButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\342\254\241", nullptr));
        detectionPolygonButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "toolbarIcon", nullptr)));
        detectionResetButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\342\237\263", nullptr));
        detectionResetButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "toolbarIcon", nullptr)));
        detectionMaskEditButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\261\217\350\224\275\345\214\272\345\237\237", nullptr));
        detectionMaskEditButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "secondary", nullptr)));
        positionCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "configCard", nullptr)));
        positionTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        positionTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "cardTitle", nullptr)));
        positionCorrectionLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        positionCorrectionLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "rowField", nullptr)));
        positionCorrectionSwitch->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\220\257\347\224\250", nullptr));
        positionCorrectionSwitch->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "secondary", nullptr)));
        positionCorrectionSourceLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\346\235\245\346\272\220", nullptr));
        positionCorrectionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "rowField", nullptr)));
        positionCorrectionComboBox->setItemText(0, QCoreApplication::translate("CirclePresenceDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        paramCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "configCard", nullptr)));
        paramTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\217\202\346\225\260", nullptr));
        paramTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "cardTitle", nullptr)));
        circleSensitivityLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\347\201\265\346\225\217\345\272\246", nullptr));
        circleSensitivityLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "rowField", nullptr)));
        roundnessLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\234\206\345\272\246", nullptr));
        roundnessLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "rowField", nullptr)));
        edgePolarityLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\350\276\271\347\274\230\346\236\201\346\200\247", nullptr));
        edgePolarityLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "rowField", nullptr)));
        edgePolarityComboBox->setItemText(0, QCoreApplication::translate("CirclePresenceDialog", "\351\273\221\345\210\260\347\231\275", nullptr));
        edgePolarityComboBox->setItemText(1, QCoreApplication::translate("CirclePresenceDialog", "\347\231\275\345\210\260\351\273\221", nullptr));
        edgePolarityComboBox->setItemText(2, QCoreApplication::translate("CirclePresenceDialog", "\344\273\273\346\204\217", nullptr));

        edgeTypeLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\350\276\271\347\274\230\347\261\273\345\236\213", nullptr));
        edgeTypeLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "rowField", nullptr)));
        edgeTypeComboBox->setItemText(0, QCoreApplication::translate("CirclePresenceDialog", "\346\234\200\345\274\272", nullptr));
        edgeTypeComboBox->setItemText(1, QCoreApplication::translate("CirclePresenceDialog", "\346\234\200\345\244\247", nullptr));
        edgeTypeComboBox->setItemText(2, QCoreApplication::translate("CirclePresenceDialog", "\346\234\200\345\260\217", nullptr));
        edgeTypeComboBox->setItemText(3, QCoreApplication::translate("CirclePresenceDialog", "\346\211\213\345\212\250\351\200\211\346\213\251", nullptr));

        resultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "configCard", nullptr)));
        resultJudgeTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        resultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CirclePresenceDialog", "cardTitle", nullptr)));
        presentOkButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\255\230\345\234\250OK", nullptr));
        absentOkButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\344\270\215\345\255\230\345\234\250OK", nullptr));
        referenceTestButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\237\272\345\207\206\345\233\276\346\265\213\350\257\225", nullptr));
        referenceTestButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "secondary", nullptr)));
        testRunButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\346\265\213\350\257\225\350\277\220\350\241\214", nullptr));
        testRunButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CirclePresenceDialog", "secondary", nullptr)));
        finishButton->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\256\214\346\210\220", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerZoomOutButton->setText(QCoreApplication::translate("CirclePresenceDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "39%", nullptr));
        viewerStatusLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "\346\243\200\346\265\213 ROI x=0.000 y=0.000 w=1.000 h=1.000", nullptr));
        viewerCursorLabel->setText(QCoreApplication::translate("CirclePresenceDialog", "X: --  Y: --   |   R: -- G: -- B: --", nullptr));
    } // retranslateUi

};

namespace Ui {
    class CirclePresenceDialog: public Ui_CirclePresenceDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CIRCLEPRESENCEDIALOG_H
