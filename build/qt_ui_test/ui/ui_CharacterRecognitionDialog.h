/********************************************************************************
** Form generated from reading UI file 'CharacterRecognitionDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CHARACTERRECOGNITIONDIALOG_H
#define UI_CHARACTERRECOGNITIONDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_CharacterRecognitionDialog
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
    QToolButton *ocrExternalEditButton;
    QSpacerItem *horizontalSpacer_editorHeader;
    QFrame *segmentFrame;
    QHBoxLayout *horizontalLayout_segment;
    QPushButton *basicSegmentButton;
    QPushButton *allSegmentButton;
    QScrollArea *ocrConfigScrollArea;
    QWidget *ocrConfigContents;
    QVBoxLayout *verticalLayout_configCards;
    QStackedWidget *configStackedWidget;
    QWidget *basicConfigPage;
    QVBoxLayout *verticalLayout_basicConfigPage;
    QFrame *detectionAreaCard;
    QVBoxLayout *verticalLayout_detectionArea;
    QHBoxLayout *horizontalLayout_detectionHeader;
    QLabel *detectionAreaTitleLabel;
    QSpacerItem *horizontalSpacer_detectionHeader;
    QToolButton *detectionCollapseButton;
    QHBoxLayout *horizontalLayout_region;
    QLabel *regionLabel;
    QToolButton *regionDrawButton;
    QToolButton *regionRectButton;
    QHBoxLayout *horizontalLayout_positionEnable;
    QLabel *positionEnableLabel;
    QSpacerItem *horizontalSpacer_positionEnable;
    QCheckBox *positionCorrectionSwitch;
    QHBoxLayout *horizontalLayout_positionSource;
    QLabel *positionSourceLabel;
    QComboBox *positionCorrectionComboBox;
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
    QWidget *countBasisPage;
    QHBoxLayout *horizontalLayout_countRange;
    QLabel *countRangeLabel;
    QSpinBox *minCountSpinBox;
    QLabel *rangeDashLabel;
    QSpinBox *maxCountSpinBox;
    QWidget *scoreBasisPage;
    QHBoxLayout *horizontalLayout_minScore;
    QLabel *minScoreLabel;
    QSpinBox *minScoreSpinBox;
    QSpacerItem *horizontalSpacer_minScore;
    QWidget *baselineBasisPage;
    QHBoxLayout *horizontalLayout_baselineText;
    QLabel *baselineTextLabel;
    QLineEdit *baselineTextLineEdit;
    QFrame *modelListCard;
    QVBoxLayout *verticalLayout_modelList;
    QLabel *modelListTitleLabel;
    QHBoxLayout *horizontalLayout_model;
    QSpacerItem *horizontalSpacer_modelLabel;
    QComboBox *modelComboBox;
    QHBoxLayout *horizontalLayout_importModel;
    QSpacerItem *horizontalSpacer_importModelLeft;
    QLabel *modelHintLabel;
    QPushButton *importModelButton;
    QSpacerItem *verticalSpacer_configCards;
    QWidget *allConfigPage;
    QVBoxLayout *verticalLayout_allConfigPage;
    QFrame *advancedOcrCard;
    QVBoxLayout *verticalLayout_advancedOcr;
    QLabel *advancedOcrTitleLabel;
    QFormLayout *formLayout_advancedOcr;
    QLabel *binaryThresholdLabel;
    QSpinBox *binaryThresholdSpinBox;
    QLabel *polarityLabel;
    QComboBox *polarityComboBox;
    QLabel *minCharAreaLabel;
    QDoubleSpinBox *minCharAreaSpinBox;
    QLabel *maxCharAreaLabel;
    QDoubleSpinBox *maxCharAreaSpinBox;
    QLabel *minConfidenceLabel;
    QSpinBox *minConfidenceSpinBox;
    QLabel *minCharWidthLabel;
    QDoubleSpinBox *minCharWidthSpinBox;
    QLabel *minCharHeightLabel;
    QDoubleSpinBox *minCharHeightSpinBox;
    QLabel *maxCharWidthLabel;
    QDoubleSpinBox *maxCharWidthSpinBox;
    QLabel *maxCharHeightLabel;
    QDoubleSpinBox *maxCharHeightSpinBox;
    QLabel *minAspectRatioLabel;
    QDoubleSpinBox *minAspectRatioSpinBox;
    QLabel *maxAspectRatioLabel;
    QDoubleSpinBox *maxAspectRatioSpinBox;
    QLabel *modelPathLabel;
    QHBoxLayout *horizontalLayout_modelPath;
    QLineEdit *modelPathLineEdit;
    QPushButton *selectModelButton;
    QSpacerItem *verticalSpacer_allConfigPage;
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

    void setupUi(QDialog *CharacterRecognitionDialog)
    {
        if (CharacterRecognitionDialog->objectName().isEmpty())
            CharacterRecognitionDialog->setObjectName(QString::fromUtf8("CharacterRecognitionDialog"));
        CharacterRecognitionDialog->resize(1760, 980);
        verticalLayout_root = new QVBoxLayout(CharacterRecognitionDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        setupTopBar = new QFrame(CharacterRecognitionDialog);
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

        setupToolbar = new QFrame(CharacterRecognitionDialog);
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

        setupBodyFrame = new QFrame(CharacterRecognitionDialog);
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

        ocrExternalEditButton = new QToolButton(setupEditorPanel);
        ocrExternalEditButton->setObjectName(QString::fromUtf8("ocrExternalEditButton"));
        ocrExternalEditButton->setIcon(icon);
        ocrExternalEditButton->setIconSize(QSize(24, 24));
        ocrExternalEditButton->setAutoRaise(true);

        horizontalLayout_editorHeader->addWidget(ocrExternalEditButton);

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

        ocrConfigScrollArea = new QScrollArea(setupEditorPanel);
        ocrConfigScrollArea->setObjectName(QString::fromUtf8("ocrConfigScrollArea"));
        ocrConfigScrollArea->setFrameShape(QFrame::NoFrame);
        ocrConfigScrollArea->setWidgetResizable(true);
        ocrConfigContents = new QWidget();
        ocrConfigContents->setObjectName(QString::fromUtf8("ocrConfigContents"));
        ocrConfigContents->setGeometry(QRect(0, 0, 554, 692));
        verticalLayout_configCards = new QVBoxLayout(ocrConfigContents);
        verticalLayout_configCards->setSpacing(12);
        verticalLayout_configCards->setObjectName(QString::fromUtf8("verticalLayout_configCards"));
        verticalLayout_configCards->setContentsMargins(0, 0, 0, 0);
        configStackedWidget = new QStackedWidget(ocrConfigContents);
        configStackedWidget->setObjectName(QString::fromUtf8("configStackedWidget"));
        basicConfigPage = new QWidget();
        basicConfigPage->setObjectName(QString::fromUtf8("basicConfigPage"));
        verticalLayout_basicConfigPage = new QVBoxLayout(basicConfigPage);
        verticalLayout_basicConfigPage->setSpacing(12);
        verticalLayout_basicConfigPage->setObjectName(QString::fromUtf8("verticalLayout_basicConfigPage"));
        verticalLayout_basicConfigPage->setContentsMargins(0, 0, 0, 0);
        detectionAreaCard = new QFrame(basicConfigPage);
        detectionAreaCard->setObjectName(QString::fromUtf8("detectionAreaCard"));
        detectionAreaCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_detectionArea = new QVBoxLayout(detectionAreaCard);
        verticalLayout_detectionArea->setSpacing(18);
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

        horizontalLayout_region = new QHBoxLayout();
        horizontalLayout_region->setObjectName(QString::fromUtf8("horizontalLayout_region"));
        regionLabel = new QLabel(detectionAreaCard);
        regionLabel->setObjectName(QString::fromUtf8("regionLabel"));
        regionLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_region->addWidget(regionLabel);

        regionDrawButton = new QToolButton(detectionAreaCard);
        regionDrawButton->setObjectName(QString::fromUtf8("regionDrawButton"));
        regionDrawButton->setMinimumSize(QSize(118, 38));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/fit.svg"), QSize(), QIcon::Normal, QIcon::Off);
        regionDrawButton->setIcon(icon4);
        regionDrawButton->setIconSize(QSize(22, 22));
        regionDrawButton->setCheckable(true);
        regionDrawButton->setChecked(true);

        horizontalLayout_region->addWidget(regionDrawButton);

        regionRectButton = new QToolButton(detectionAreaCard);
        regionRectButton->setObjectName(QString::fromUtf8("regionRectButton"));
        regionRectButton->setMinimumSize(QSize(118, 38));
        regionRectButton->setCheckable(true);

        horizontalLayout_region->addWidget(regionRectButton);


        verticalLayout_detectionArea->addLayout(horizontalLayout_region);

        horizontalLayout_positionEnable = new QHBoxLayout();
        horizontalLayout_positionEnable->setObjectName(QString::fromUtf8("horizontalLayout_positionEnable"));
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


        verticalLayout_basicConfigPage->addWidget(detectionAreaCard);

        resultJudgeCard = new QFrame(basicConfigPage);
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
        resultBasisLabel = new QLabel(resultJudgeCard);
        resultBasisLabel->setObjectName(QString::fromUtf8("resultBasisLabel"));
        resultBasisLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_resultBasis->addWidget(resultBasisLabel);

        resultBasisComboBox = new QComboBox(resultJudgeCard);
        resultBasisComboBox->addItem(QString());
        resultBasisComboBox->addItem(QString());
        resultBasisComboBox->addItem(QString());
        resultBasisComboBox->setObjectName(QString::fromUtf8("resultBasisComboBox"));
        resultBasisComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_resultBasis->addWidget(resultBasisComboBox);


        verticalLayout_resultJudge->addLayout(horizontalLayout_resultBasis);

        resultBasisStackedWidget = new QStackedWidget(resultJudgeCard);
        resultBasisStackedWidget->setObjectName(QString::fromUtf8("resultBasisStackedWidget"));
        countBasisPage = new QWidget();
        countBasisPage->setObjectName(QString::fromUtf8("countBasisPage"));
        horizontalLayout_countRange = new QHBoxLayout(countBasisPage);
        horizontalLayout_countRange->setObjectName(QString::fromUtf8("horizontalLayout_countRange"));
        horizontalLayout_countRange->setContentsMargins(0, 0, 0, 0);
        countRangeLabel = new QLabel(countBasisPage);
        countRangeLabel->setObjectName(QString::fromUtf8("countRangeLabel"));
        countRangeLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_countRange->addWidget(countRangeLabel);

        minCountSpinBox = new QSpinBox(countBasisPage);
        minCountSpinBox->setObjectName(QString::fromUtf8("minCountSpinBox"));
        minCountSpinBox->setMinimumSize(QSize(110, 38));
        minCountSpinBox->setMinimum(1);
        minCountSpinBox->setMaximum(999);
        minCountSpinBox->setValue(1);

        horizontalLayout_countRange->addWidget(minCountSpinBox);

        rangeDashLabel = new QLabel(countBasisPage);
        rangeDashLabel->setObjectName(QString::fromUtf8("rangeDashLabel"));
        rangeDashLabel->setAlignment(Qt::AlignCenter);

        horizontalLayout_countRange->addWidget(rangeDashLabel);

        maxCountSpinBox = new QSpinBox(countBasisPage);
        maxCountSpinBox->setObjectName(QString::fromUtf8("maxCountSpinBox"));
        maxCountSpinBox->setMinimumSize(QSize(110, 38));
        maxCountSpinBox->setMinimum(1);
        maxCountSpinBox->setMaximum(999);
        maxCountSpinBox->setValue(10);

        horizontalLayout_countRange->addWidget(maxCountSpinBox);

        resultBasisStackedWidget->addWidget(countBasisPage);
        scoreBasisPage = new QWidget();
        scoreBasisPage->setObjectName(QString::fromUtf8("scoreBasisPage"));
        horizontalLayout_minScore = new QHBoxLayout(scoreBasisPage);
        horizontalLayout_minScore->setObjectName(QString::fromUtf8("horizontalLayout_minScore"));
        horizontalLayout_minScore->setContentsMargins(0, 0, 0, 0);
        minScoreLabel = new QLabel(scoreBasisPage);
        minScoreLabel->setObjectName(QString::fromUtf8("minScoreLabel"));
        minScoreLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_minScore->addWidget(minScoreLabel);

        minScoreSpinBox = new QSpinBox(scoreBasisPage);
        minScoreSpinBox->setObjectName(QString::fromUtf8("minScoreSpinBox"));
        minScoreSpinBox->setMinimumSize(QSize(180, 38));
        minScoreSpinBox->setMinimum(0);
        minScoreSpinBox->setMaximum(100);
        minScoreSpinBox->setValue(70);

        horizontalLayout_minScore->addWidget(minScoreSpinBox);

        horizontalSpacer_minScore = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_minScore->addItem(horizontalSpacer_minScore);

        resultBasisStackedWidget->addWidget(scoreBasisPage);
        baselineBasisPage = new QWidget();
        baselineBasisPage->setObjectName(QString::fromUtf8("baselineBasisPage"));
        horizontalLayout_baselineText = new QHBoxLayout(baselineBasisPage);
        horizontalLayout_baselineText->setObjectName(QString::fromUtf8("horizontalLayout_baselineText"));
        horizontalLayout_baselineText->setContentsMargins(0, 0, 0, 0);
        baselineTextLabel = new QLabel(baselineBasisPage);
        baselineTextLabel->setObjectName(QString::fromUtf8("baselineTextLabel"));
        baselineTextLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_baselineText->addWidget(baselineTextLabel);

        baselineTextLineEdit = new QLineEdit(baselineBasisPage);
        baselineTextLineEdit->setObjectName(QString::fromUtf8("baselineTextLineEdit"));
        baselineTextLineEdit->setMinimumSize(QSize(260, 38));

        horizontalLayout_baselineText->addWidget(baselineTextLineEdit);

        resultBasisStackedWidget->addWidget(baselineBasisPage);

        verticalLayout_resultJudge->addWidget(resultBasisStackedWidget);


        verticalLayout_basicConfigPage->addWidget(resultJudgeCard);

        modelListCard = new QFrame(basicConfigPage);
        modelListCard->setObjectName(QString::fromUtf8("modelListCard"));
        modelListCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_modelList = new QVBoxLayout(modelListCard);
        verticalLayout_modelList->setSpacing(14);
        verticalLayout_modelList->setObjectName(QString::fromUtf8("verticalLayout_modelList"));
        verticalLayout_modelList->setContentsMargins(20, 18, 20, 18);
        modelListTitleLabel = new QLabel(modelListCard);
        modelListTitleLabel->setObjectName(QString::fromUtf8("modelListTitleLabel"));

        verticalLayout_modelList->addWidget(modelListTitleLabel);

        horizontalLayout_model = new QHBoxLayout();
        horizontalLayout_model->setObjectName(QString::fromUtf8("horizontalLayout_model"));
        horizontalSpacer_modelLabel = new QSpacerItem(150, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_model->addItem(horizontalSpacer_modelLabel);

        modelComboBox = new QComboBox(modelListCard);
        modelComboBox->addItem(QString());
        modelComboBox->setObjectName(QString::fromUtf8("modelComboBox"));
        modelComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_model->addWidget(modelComboBox);


        verticalLayout_modelList->addLayout(horizontalLayout_model);

        horizontalLayout_importModel = new QHBoxLayout();
        horizontalLayout_importModel->setObjectName(QString::fromUtf8("horizontalLayout_importModel"));
        horizontalSpacer_importModelLeft = new QSpacerItem(150, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_importModel->addItem(horizontalSpacer_importModelLeft);

        modelHintLabel = new QLabel(modelListCard);
        modelHintLabel->setObjectName(QString::fromUtf8("modelHintLabel"));

        horizontalLayout_importModel->addWidget(modelHintLabel);

        importModelButton = new QPushButton(modelListCard);
        importModelButton->setObjectName(QString::fromUtf8("importModelButton"));
        importModelButton->setMinimumSize(QSize(78, 38));

        horizontalLayout_importModel->addWidget(importModelButton);


        verticalLayout_modelList->addLayout(horizontalLayout_importModel);


        verticalLayout_basicConfigPage->addWidget(modelListCard);

        verticalSpacer_configCards = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_basicConfigPage->addItem(verticalSpacer_configCards);

        configStackedWidget->addWidget(basicConfigPage);
        allConfigPage = new QWidget();
        allConfigPage->setObjectName(QString::fromUtf8("allConfigPage"));
        verticalLayout_allConfigPage = new QVBoxLayout(allConfigPage);
        verticalLayout_allConfigPage->setSpacing(12);
        verticalLayout_allConfigPage->setObjectName(QString::fromUtf8("verticalLayout_allConfigPage"));
        verticalLayout_allConfigPage->setContentsMargins(0, 0, 0, 0);
        advancedOcrCard = new QFrame(allConfigPage);
        advancedOcrCard->setObjectName(QString::fromUtf8("advancedOcrCard"));
        advancedOcrCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_advancedOcr = new QVBoxLayout(advancedOcrCard);
        verticalLayout_advancedOcr->setSpacing(14);
        verticalLayout_advancedOcr->setObjectName(QString::fromUtf8("verticalLayout_advancedOcr"));
        verticalLayout_advancedOcr->setContentsMargins(20, 18, 20, 18);
        advancedOcrTitleLabel = new QLabel(advancedOcrCard);
        advancedOcrTitleLabel->setObjectName(QString::fromUtf8("advancedOcrTitleLabel"));

        verticalLayout_advancedOcr->addWidget(advancedOcrTitleLabel);

        formLayout_advancedOcr = new QFormLayout();
        formLayout_advancedOcr->setObjectName(QString::fromUtf8("formLayout_advancedOcr"));
        formLayout_advancedOcr->setHorizontalSpacing(16);
        formLayout_advancedOcr->setVerticalSpacing(12);
        formLayout_advancedOcr->setLabelAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        binaryThresholdLabel = new QLabel(advancedOcrCard);
        binaryThresholdLabel->setObjectName(QString::fromUtf8("binaryThresholdLabel"));

        formLayout_advancedOcr->setWidget(0, QFormLayout::LabelRole, binaryThresholdLabel);

        binaryThresholdSpinBox = new QSpinBox(advancedOcrCard);
        binaryThresholdSpinBox->setObjectName(QString::fromUtf8("binaryThresholdSpinBox"));
        binaryThresholdSpinBox->setMinimumSize(QSize(220, 38));
        binaryThresholdSpinBox->setMaximum(255);
        binaryThresholdSpinBox->setValue(128);

        formLayout_advancedOcr->setWidget(0, QFormLayout::FieldRole, binaryThresholdSpinBox);

        polarityLabel = new QLabel(advancedOcrCard);
        polarityLabel->setObjectName(QString::fromUtf8("polarityLabel"));

        formLayout_advancedOcr->setWidget(1, QFormLayout::LabelRole, polarityLabel);

        polarityComboBox = new QComboBox(advancedOcrCard);
        polarityComboBox->addItem(QString());
        polarityComboBox->addItem(QString());
        polarityComboBox->addItem(QString());
        polarityComboBox->setObjectName(QString::fromUtf8("polarityComboBox"));
        polarityComboBox->setMinimumSize(QSize(220, 38));

        formLayout_advancedOcr->setWidget(1, QFormLayout::FieldRole, polarityComboBox);

        minCharAreaLabel = new QLabel(advancedOcrCard);
        minCharAreaLabel->setObjectName(QString::fromUtf8("minCharAreaLabel"));

        formLayout_advancedOcr->setWidget(2, QFormLayout::LabelRole, minCharAreaLabel);

        minCharAreaSpinBox = new QDoubleSpinBox(advancedOcrCard);
        minCharAreaSpinBox->setObjectName(QString::fromUtf8("minCharAreaSpinBox"));
        minCharAreaSpinBox->setMinimumSize(QSize(220, 38));
        minCharAreaSpinBox->setMaximum(1000000.000000000000000);
        minCharAreaSpinBox->setValue(20.000000000000000);

        formLayout_advancedOcr->setWidget(2, QFormLayout::FieldRole, minCharAreaSpinBox);

        maxCharAreaLabel = new QLabel(advancedOcrCard);
        maxCharAreaLabel->setObjectName(QString::fromUtf8("maxCharAreaLabel"));

        formLayout_advancedOcr->setWidget(3, QFormLayout::LabelRole, maxCharAreaLabel);

        maxCharAreaSpinBox = new QDoubleSpinBox(advancedOcrCard);
        maxCharAreaSpinBox->setObjectName(QString::fromUtf8("maxCharAreaSpinBox"));
        maxCharAreaSpinBox->setMinimumSize(QSize(220, 38));
        maxCharAreaSpinBox->setMaximum(10000000.000000000000000);
        maxCharAreaSpinBox->setValue(100000.000000000000000);

        formLayout_advancedOcr->setWidget(3, QFormLayout::FieldRole, maxCharAreaSpinBox);

        minConfidenceLabel = new QLabel(advancedOcrCard);
        minConfidenceLabel->setObjectName(QString::fromUtf8("minConfidenceLabel"));

        formLayout_advancedOcr->setWidget(4, QFormLayout::LabelRole, minConfidenceLabel);

        minConfidenceSpinBox = new QSpinBox(advancedOcrCard);
        minConfidenceSpinBox->setObjectName(QString::fromUtf8("minConfidenceSpinBox"));
        minConfidenceSpinBox->setMinimumSize(QSize(220, 38));
        minConfidenceSpinBox->setMaximum(100);
        minConfidenceSpinBox->setValue(70);

        formLayout_advancedOcr->setWidget(4, QFormLayout::FieldRole, minConfidenceSpinBox);

        minCharWidthLabel = new QLabel(advancedOcrCard);
        minCharWidthLabel->setObjectName(QString::fromUtf8("minCharWidthLabel"));

        formLayout_advancedOcr->setWidget(5, QFormLayout::LabelRole, minCharWidthLabel);

        minCharWidthSpinBox = new QDoubleSpinBox(advancedOcrCard);
        minCharWidthSpinBox->setObjectName(QString::fromUtf8("minCharWidthSpinBox"));
        minCharWidthSpinBox->setMinimumSize(QSize(220, 38));
        minCharWidthSpinBox->setMaximum(99999.000000000000000);

        formLayout_advancedOcr->setWidget(5, QFormLayout::FieldRole, minCharWidthSpinBox);

        minCharHeightLabel = new QLabel(advancedOcrCard);
        minCharHeightLabel->setObjectName(QString::fromUtf8("minCharHeightLabel"));

        formLayout_advancedOcr->setWidget(6, QFormLayout::LabelRole, minCharHeightLabel);

        minCharHeightSpinBox = new QDoubleSpinBox(advancedOcrCard);
        minCharHeightSpinBox->setObjectName(QString::fromUtf8("minCharHeightSpinBox"));
        minCharHeightSpinBox->setMinimumSize(QSize(220, 38));
        minCharHeightSpinBox->setMaximum(99999.000000000000000);

        formLayout_advancedOcr->setWidget(6, QFormLayout::FieldRole, minCharHeightSpinBox);

        maxCharWidthLabel = new QLabel(advancedOcrCard);
        maxCharWidthLabel->setObjectName(QString::fromUtf8("maxCharWidthLabel"));

        formLayout_advancedOcr->setWidget(7, QFormLayout::LabelRole, maxCharWidthLabel);

        maxCharWidthSpinBox = new QDoubleSpinBox(advancedOcrCard);
        maxCharWidthSpinBox->setObjectName(QString::fromUtf8("maxCharWidthSpinBox"));
        maxCharWidthSpinBox->setMinimumSize(QSize(220, 38));
        maxCharWidthSpinBox->setMaximum(99999.000000000000000);
        maxCharWidthSpinBox->setValue(99999.000000000000000);

        formLayout_advancedOcr->setWidget(7, QFormLayout::FieldRole, maxCharWidthSpinBox);

        maxCharHeightLabel = new QLabel(advancedOcrCard);
        maxCharHeightLabel->setObjectName(QString::fromUtf8("maxCharHeightLabel"));

        formLayout_advancedOcr->setWidget(8, QFormLayout::LabelRole, maxCharHeightLabel);

        maxCharHeightSpinBox = new QDoubleSpinBox(advancedOcrCard);
        maxCharHeightSpinBox->setObjectName(QString::fromUtf8("maxCharHeightSpinBox"));
        maxCharHeightSpinBox->setMinimumSize(QSize(220, 38));
        maxCharHeightSpinBox->setMaximum(99999.000000000000000);
        maxCharHeightSpinBox->setValue(99999.000000000000000);

        formLayout_advancedOcr->setWidget(8, QFormLayout::FieldRole, maxCharHeightSpinBox);

        minAspectRatioLabel = new QLabel(advancedOcrCard);
        minAspectRatioLabel->setObjectName(QString::fromUtf8("minAspectRatioLabel"));

        formLayout_advancedOcr->setWidget(9, QFormLayout::LabelRole, minAspectRatioLabel);

        minAspectRatioSpinBox = new QDoubleSpinBox(advancedOcrCard);
        minAspectRatioSpinBox->setObjectName(QString::fromUtf8("minAspectRatioSpinBox"));
        minAspectRatioSpinBox->setMinimumSize(QSize(220, 38));
        minAspectRatioSpinBox->setDecimals(2);
        minAspectRatioSpinBox->setMinimum(0.010000000000000);
        minAspectRatioSpinBox->setMaximum(100.000000000000000);
        minAspectRatioSpinBox->setSingleStep(0.100000000000000);
        minAspectRatioSpinBox->setValue(0.100000000000000);

        formLayout_advancedOcr->setWidget(9, QFormLayout::FieldRole, minAspectRatioSpinBox);

        maxAspectRatioLabel = new QLabel(advancedOcrCard);
        maxAspectRatioLabel->setObjectName(QString::fromUtf8("maxAspectRatioLabel"));

        formLayout_advancedOcr->setWidget(10, QFormLayout::LabelRole, maxAspectRatioLabel);

        maxAspectRatioSpinBox = new QDoubleSpinBox(advancedOcrCard);
        maxAspectRatioSpinBox->setObjectName(QString::fromUtf8("maxAspectRatioSpinBox"));
        maxAspectRatioSpinBox->setMinimumSize(QSize(220, 38));
        maxAspectRatioSpinBox->setDecimals(2);
        maxAspectRatioSpinBox->setMinimum(0.010000000000000);
        maxAspectRatioSpinBox->setMaximum(100.000000000000000);
        maxAspectRatioSpinBox->setSingleStep(0.100000000000000);
        maxAspectRatioSpinBox->setValue(10.000000000000000);

        formLayout_advancedOcr->setWidget(10, QFormLayout::FieldRole, maxAspectRatioSpinBox);

        modelPathLabel = new QLabel(advancedOcrCard);
        modelPathLabel->setObjectName(QString::fromUtf8("modelPathLabel"));

        formLayout_advancedOcr->setWidget(11, QFormLayout::LabelRole, modelPathLabel);

        horizontalLayout_modelPath = new QHBoxLayout();
        horizontalLayout_modelPath->setObjectName(QString::fromUtf8("horizontalLayout_modelPath"));
        modelPathLineEdit = new QLineEdit(advancedOcrCard);
        modelPathLineEdit->setObjectName(QString::fromUtf8("modelPathLineEdit"));
        modelPathLineEdit->setMinimumSize(QSize(0, 38));

        horizontalLayout_modelPath->addWidget(modelPathLineEdit);

        selectModelButton = new QPushButton(advancedOcrCard);
        selectModelButton->setObjectName(QString::fromUtf8("selectModelButton"));
        selectModelButton->setMinimumSize(QSize(78, 38));

        horizontalLayout_modelPath->addWidget(selectModelButton);


        formLayout_advancedOcr->setLayout(11, QFormLayout::FieldRole, horizontalLayout_modelPath);


        verticalLayout_advancedOcr->addLayout(formLayout_advancedOcr);


        verticalLayout_allConfigPage->addWidget(advancedOcrCard);

        verticalSpacer_allConfigPage = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_allConfigPage->addItem(verticalSpacer_allConfigPage);

        configStackedWidget->addWidget(allConfigPage);

        verticalLayout_configCards->addWidget(configStackedWidget);

        ocrConfigScrollArea->setWidget(ocrConfigContents);

        verticalLayout_editor->addWidget(ocrConfigScrollArea);

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


        retranslateUi(CharacterRecognitionDialog);

        configStackedWidget->setCurrentIndex(0);
        resultBasisStackedWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(CharacterRecognitionDialog);
    } // setupUi

    void retranslateUi(QDialog *CharacterRecognitionDialog)
    {
        CharacterRecognitionDialog->setWindowTitle(QCoreApplication::translate("CharacterRecognitionDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \345\255\227\347\254\246\350\257\206\345\210\253", nullptr));
        setupWindowTitleLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\303\227", nullptr));
        setupPageCodeLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "422", nullptr));
        setupSaveButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\344\277\235\345\255\230", nullptr));
        setupSaveAsButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\217\246\345\255\230\344\270\272", nullptr));
        setupExportButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "IO\350\276\223\345\207\272", nullptr));
        editorTitleLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\255\227\347\254\246\350\257\206\345\210\253", nullptr));
        basicSegmentButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\237\272\347\241\200", nullptr));
        allSegmentButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\205\250\351\203\250", nullptr));
        detectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "configCard", nullptr)));
        detectionAreaTitleLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        detectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "cardTitle", nullptr)));
        detectionCollapseButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\342\214\204", nullptr));
        detectionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "collapseCard", nullptr)));
        regionLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        regionLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "rowField", nullptr)));
        regionRectButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\342\226\241", nullptr));
        positionEnableLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243\344\275\277\350\203\275 \342\223\230", nullptr));
        positionEnableLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "rowField", nullptr)));
        positionCorrectionSwitch->setText(QString());
        positionSourceLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        positionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "rowField", nullptr)));
        positionCorrectionComboBox->setItemText(0, QCoreApplication::translate("CharacterRecognitionDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        resultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "configCard", nullptr)));
        resultJudgeTitleLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        resultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "cardTitle", nullptr)));
        resultCollapseButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\342\214\204", nullptr));
        resultCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "collapseCard", nullptr)));
        resultBasisLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\210\244\346\226\255\344\276\235\346\215\256", nullptr));
        resultBasisLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "rowField", nullptr)));
        resultBasisComboBox->setItemText(0, QCoreApplication::translate("CharacterRecognitionDialog", "\345\255\227\347\254\246\344\270\252\346\225\260", nullptr));
        resultBasisComboBox->setItemText(1, QCoreApplication::translate("CharacterRecognitionDialog", "\345\255\227\347\254\246\345\276\227\345\210\206", nullptr));
        resultBasisComboBox->setItemText(2, QCoreApplication::translate("CharacterRecognitionDialog", "\345\237\272\345\207\206\345\255\227\347\254\246", nullptr));

        countRangeLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\225\260\351\207\217\350\214\203\345\233\264", nullptr));
        countRangeLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "rowField", nullptr)));
        rangeDashLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "-", nullptr));
        minScoreLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));
        minScoreLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "rowField", nullptr)));
        baselineTextLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\255\227\347\254\246\345\206\205\345\256\271", nullptr));
        baselineTextLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "rowField", nullptr)));
        baselineTextLineEdit->setText(QCoreApplication::translate("CharacterRecognitionDialog", "2026-4-22", nullptr));
        modelListCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "configCard", nullptr)));
        modelListTitleLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\350\257\206\345\210\253\346\250\241\345\236\213\345\210\227\350\241\250", nullptr));
        modelListTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "cardTitle", nullptr)));
        modelComboBox->setItemText(0, QCoreApplication::translate("CharacterRecognitionDialog", "Common.bin \347\263\273\347\273\237\346\250\241\345\236\213", nullptr));

        modelHintLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\262\241\346\234\211\345\220\210\351\200\202\347\232\204\346\250\241\345\236\213\357\274\237", nullptr));
        modelHintLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "cardHint", nullptr)));
        importModelButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\257\274\345\205\245", nullptr));
        advancedOcrCard->setProperty("panelRole", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "configCard", nullptr)));
        advancedOcrTitleLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "OCR \351\253\230\347\272\247\345\217\202\346\225\260", nullptr));
        advancedOcrTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "cardTitle", nullptr)));
        binaryThresholdLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\344\272\214\345\200\274\351\230\210\345\200\274", nullptr));
        polarityLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\255\227\347\254\246\346\236\201\346\200\247", nullptr));
        polarityComboBox->setItemText(0, QCoreApplication::translate("CharacterRecognitionDialog", "\346\232\227\347\233\256\346\240\207", nullptr));
        polarityComboBox->setItemText(1, QCoreApplication::translate("CharacterRecognitionDialog", "\344\272\256\347\233\256\346\240\207", nullptr));
        polarityComboBox->setItemText(2, QCoreApplication::translate("CharacterRecognitionDialog", "\350\207\252\345\212\250", nullptr));

        minCharAreaLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\234\200\345\260\217\345\255\227\347\254\246\351\235\242\347\247\257", nullptr));
        maxCharAreaLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\234\200\345\244\247\345\255\227\347\254\246\351\235\242\347\247\257", nullptr));
        minConfidenceLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));
        minCharWidthLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\234\200\345\260\217\345\255\227\347\254\246\345\256\275\345\272\246", nullptr));
        minCharHeightLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\234\200\345\260\217\345\255\227\347\254\246\351\253\230\345\272\246", nullptr));
        maxCharWidthLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\234\200\345\244\247\345\255\227\347\254\246\345\256\275\345\272\246", nullptr));
        maxCharHeightLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\234\200\345\244\247\345\255\227\347\254\246\351\253\230\345\272\246", nullptr));
        minAspectRatioLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\234\200\345\260\217\351\225\277\345\256\275\346\257\224", nullptr));
        maxAspectRatioLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\346\234\200\345\244\247\351\225\277\345\256\275\346\257\224", nullptr));
        modelPathLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "OCR \346\250\241\345\236\213\350\267\257\345\276\204", nullptr));
        modelPathLineEdit->setText(QCoreApplication::translate("CharacterRecognitionDialog", "/home/hjl-ubuntu/MVTec/HALCON-24.11-Progress-Steady/ocr/OCRB_0-9A-Z_NoRej.omc", nullptr));
        selectModelButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\351\200\211\346\213\251", nullptr));
        referenceTestButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\237\272\345\207\206\345\233\276\346\265\213\350\257\225", nullptr));
        referenceTestButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "secondary", nullptr)));
        testRunButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\217\226\346\266\210", nullptr));
        testRunButton->setProperty("actionRole", QVariant(QCoreApplication::translate("CharacterRecognitionDialog", "secondary", nullptr)));
        finishButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\256\214\346\210\220", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerZoomOutButton->setText(QCoreApplication::translate("CharacterRecognitionDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "39%", nullptr));
        viewerStatusLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: --ms   \345\267\245\345\205\267\350\200\227\346\227\266: --ms", nullptr));
        viewerCursorLabel->setText(QCoreApplication::translate("CharacterRecognitionDialog", "X: --  Y: ---   |   R: -- G: -- B: --", nullptr));
    } // retranslateUi

};

namespace Ui {
    class CharacterRecognitionDialog: public Ui_CharacterRecognitionDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CHARACTERRECOGNITIONDIALOG_H
