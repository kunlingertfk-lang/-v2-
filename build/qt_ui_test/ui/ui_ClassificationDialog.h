/********************************************************************************
** Form generated from reading UI file 'ClassificationDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CLASSIFICATIONDIALOG_H
#define UI_CLASSIFICATIONDIALOG_H

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

class Ui_ClassificationDialog
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
    QToolButton *classificationExternalEditButton;
    QSpacerItem *horizontalSpacer_editorHeader;
    QFrame *segmentFrame;
    QHBoxLayout *horizontalLayout_segment;
    QPushButton *basicSegmentButton;
    QPushButton *allSegmentButton;
    QStackedWidget *classificationParamsStackedWidget;
    QWidget *basicParamsPage;
    QVBoxLayout *verticalLayout_basicParamsPage;
    QScrollArea *classificationBasicScrollArea;
    QWidget *classificationBasicContents;
    QVBoxLayout *verticalLayout_classificationBasicConfigCards;
    QFrame *DetectionAreaCard;
    QVBoxLayout *verticalLayout_DetectionArea;
    QHBoxLayout *horizontalLayout_DetectionAreaTitleLabel_header;
    QLabel *DetectionAreaTitleLabel;
    QSpacerItem *horizontalSpacer_DetectionAreaTitleLabel_header;
    QToolButton *DetectionCollapseButton;
    QHBoxLayout *horizontalLayout_region;
    QLabel *regionLabel;
    QFrame *regionButtonsFrame;
    QHBoxLayout *horizontalLayout_regionButtons;
    QToolButton *regionDrawButton;
    QToolButton *regionRectButton;
    QHBoxLayout *horizontalLayout_positionEnable;
    QLabel *positionEnableLabel;
    QSpacerItem *horizontalSpacer_horizontalLayout_positionEnable;
    QCheckBox *positionCorrectionSwitch;
    QHBoxLayout *horizontalLayout_positionSource;
    QLabel *positionSourceLabel;
    QSpacerItem *horizontalSpacer_horizontalLayout_positionSource;
    QComboBox *positionCorrectionComboBox;
    QFrame *ModelListCard;
    QVBoxLayout *verticalLayout_ModelList;
    QHBoxLayout *horizontalLayout_Model;
    QLabel *ModelListTitleLabel;
    QSpacerItem *horizontalSpacer_Model;
    QComboBox *modelComboBox;
    QHBoxLayout *horizontalLayout_ImportModel;
    QSpacerItem *horizontalSpacer_ImportModelLeft;
    QLabel *modelHintLabel;
    QPushButton *importModelButton;
    QFrame *ResultJudgeCard;
    QVBoxLayout *verticalLayout_ResultJudge;
    QHBoxLayout *horizontalLayout_ResultJudgeTitleLabel_header;
    QLabel *ResultJudgeTitleLabel;
    QSpacerItem *horizontalSpacer_ResultJudgeTitleLabel_header;
    QToolButton *ResultCollapseButton;
    QHBoxLayout *horizontalLayout_ResultBasis;
    QLabel *ResultBasisLabel;
    QSpacerItem *horizontalSpacer_horizontalLayout_ResultBasis;
    QComboBox *resultBasisComboBox;
    QStackedWidget *resultBasisStackedWidget;
    QWidget *ScoreBasisPage;
    QHBoxLayout *horizontalLayout_ScoreBasis;
    QLabel *ScoreBasisLabel;
    QSpacerItem *horizontalSpacer_ScoreBasis;
    QSpinBox *minScoreSpinBox;
    QWidget *CategoryBasisPage;
    QHBoxLayout *horizontalLayout_CategoryBasis;
    QLabel *CategoryBasisLabel;
    QSpacerItem *horizontalSpacer_CategoryBasis;
    QLineEdit *categoryLineEdit;
    QSpacerItem *verticalSpacer_classificationBasicContents;
    QWidget *allParamsPage;
    QVBoxLayout *verticalLayout_allParamsPage;
    QScrollArea *classificationAllScrollArea;
    QWidget *classificationAllContents;
    QVBoxLayout *verticalLayout_classificationAllConfigCards;
    QFrame *allDetectionAreaCard;
    QVBoxLayout *verticalLayout_allDetectionArea;
    QHBoxLayout *horizontalLayout_allDetectionAreaTitleLabel_header;
    QLabel *allDetectionAreaTitleLabel;
    QSpacerItem *horizontalSpacer_allDetectionAreaTitleLabel_header;
    QToolButton *allDetectionCollapseButton;
    QHBoxLayout *horizontalLayout_allRegion;
    QLabel *allRegionLabel;
    QFrame *allRegionButtonsFrame;
    QHBoxLayout *horizontalLayout_allRegionButtons;
    QToolButton *allRegionDrawButton;
    QToolButton *allRegionRectButton;
    QHBoxLayout *horizontalLayout_screenRegion;
    QLabel *screenRegionLabel;
    QSpacerItem *horizontalSpacer_horizontalLayout_screenRegion;
    QPushButton *screenRegionEditButton;
    QHBoxLayout *horizontalLayout_allPositionEnable;
    QLabel *allPositionEnableLabel;
    QSpacerItem *horizontalSpacer_horizontalLayout_allPositionEnable;
    QCheckBox *allPositionCorrectionSwitch;
    QHBoxLayout *horizontalLayout_allPositionSource;
    QLabel *allPositionSourceLabel;
    QSpacerItem *horizontalSpacer_horizontalLayout_allPositionSource;
    QComboBox *allPositionCorrectionComboBox;
    QFrame *allModelListCard;
    QVBoxLayout *verticalLayout_allModelList;
    QHBoxLayout *horizontalLayout_allModel;
    QLabel *allModelListTitleLabel;
    QSpacerItem *horizontalSpacer_allModel;
    QComboBox *allModelComboBox;
    QHBoxLayout *horizontalLayout_allImportModel;
    QSpacerItem *horizontalSpacer_allImportModelLeft;
    QLabel *allModelHintLabel;
    QPushButton *allImportModelButton;
    QFrame *allResultJudgeCard;
    QVBoxLayout *verticalLayout_allResultJudge;
    QHBoxLayout *horizontalLayout_allResultJudgeTitleLabel_header;
    QLabel *allResultJudgeTitleLabel;
    QSpacerItem *horizontalSpacer_allResultJudgeTitleLabel_header;
    QToolButton *allResultCollapseButton;
    QHBoxLayout *horizontalLayout_allResultBasis;
    QLabel *allResultBasisLabel;
    QSpacerItem *horizontalSpacer_horizontalLayout_allResultBasis;
    QComboBox *allResultBasisComboBox;
    QStackedWidget *allResultBasisStackedWidget;
    QWidget *allScoreBasisPage;
    QHBoxLayout *horizontalLayout_allScoreBasis;
    QLabel *allScoreBasisLabel;
    QSpacerItem *horizontalSpacer_allScoreBasis;
    QSpinBox *allMinScoreSpinBox;
    QWidget *allCategoryBasisPage;
    QHBoxLayout *horizontalLayout_allCategoryBasis;
    QLabel *allCategoryBasisLabel;
    QSpacerItem *horizontalSpacer_allCategoryBasis;
    QLineEdit *allCategoryLineEdit;
    QHBoxLayout *horizontalLayout_allJudgeType;
    QLabel *allJudgeTypeLabel;
    QSpacerItem *horizontalSpacer_allJudgeType;
    QComboBox *allJudgeTypeComboBox;
    QSpacerItem *verticalSpacer_classificationAllContents;
    QHBoxLayout *horizontalLayout_actions;
    QSpacerItem *horizontalSpacer_actions;
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

    void setupUi(QDialog *ClassificationDialog)
    {
        if (ClassificationDialog->objectName().isEmpty())
            ClassificationDialog->setObjectName(QString::fromUtf8("ClassificationDialog"));
        ClassificationDialog->resize(1760, 980);
        verticalLayout_root = new QVBoxLayout(ClassificationDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        setupTopBar = new QFrame(ClassificationDialog);
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

        setupToolbar = new QFrame(ClassificationDialog);
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

        setupBodyFrame = new QFrame(ClassificationDialog);
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

        classificationExternalEditButton = new QToolButton(setupEditorPanel);
        classificationExternalEditButton->setObjectName(QString::fromUtf8("classificationExternalEditButton"));
        classificationExternalEditButton->setIcon(icon);
        classificationExternalEditButton->setIconSize(QSize(24, 24));
        classificationExternalEditButton->setAutoRaise(true);

        horizontalLayout_editorHeader->addWidget(classificationExternalEditButton);

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

        classificationParamsStackedWidget = new QStackedWidget(setupEditorPanel);
        classificationParamsStackedWidget->setObjectName(QString::fromUtf8("classificationParamsStackedWidget"));
        basicParamsPage = new QWidget();
        basicParamsPage->setObjectName(QString::fromUtf8("basicParamsPage"));
        verticalLayout_basicParamsPage = new QVBoxLayout(basicParamsPage);
        verticalLayout_basicParamsPage->setObjectName(QString::fromUtf8("verticalLayout_basicParamsPage"));
        verticalLayout_basicParamsPage->setContentsMargins(0, 0, 0, 0);
        classificationBasicScrollArea = new QScrollArea(basicParamsPage);
        classificationBasicScrollArea->setObjectName(QString::fromUtf8("classificationBasicScrollArea"));
        classificationBasicScrollArea->setFrameShape(QFrame::NoFrame);
        classificationBasicScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        classificationBasicScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        classificationBasicScrollArea->setWidgetResizable(true);
        classificationBasicContents = new QWidget();
        classificationBasicContents->setObjectName(QString::fromUtf8("classificationBasicContents"));
        classificationBasicContents->setGeometry(QRect(0, 0, 554, 692));
        verticalLayout_classificationBasicConfigCards = new QVBoxLayout(classificationBasicContents);
        verticalLayout_classificationBasicConfigCards->setSpacing(12);
        verticalLayout_classificationBasicConfigCards->setObjectName(QString::fromUtf8("verticalLayout_classificationBasicConfigCards"));
        verticalLayout_classificationBasicConfigCards->setContentsMargins(0, 0, 0, 0);
        DetectionAreaCard = new QFrame(classificationBasicContents);
        DetectionAreaCard->setObjectName(QString::fromUtf8("DetectionAreaCard"));
        DetectionAreaCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_DetectionArea = new QVBoxLayout(DetectionAreaCard);
        verticalLayout_DetectionArea->setSpacing(16);
        verticalLayout_DetectionArea->setObjectName(QString::fromUtf8("verticalLayout_DetectionArea"));
        verticalLayout_DetectionArea->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_DetectionAreaTitleLabel_header = new QHBoxLayout();
        horizontalLayout_DetectionAreaTitleLabel_header->setObjectName(QString::fromUtf8("horizontalLayout_DetectionAreaTitleLabel_header"));
        DetectionAreaTitleLabel = new QLabel(DetectionAreaCard);
        DetectionAreaTitleLabel->setObjectName(QString::fromUtf8("DetectionAreaTitleLabel"));

        horizontalLayout_DetectionAreaTitleLabel_header->addWidget(DetectionAreaTitleLabel);

        horizontalSpacer_DetectionAreaTitleLabel_header = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_DetectionAreaTitleLabel_header->addItem(horizontalSpacer_DetectionAreaTitleLabel_header);

        DetectionCollapseButton = new QToolButton(DetectionAreaCard);
        DetectionCollapseButton->setObjectName(QString::fromUtf8("DetectionCollapseButton"));

        horizontalLayout_DetectionAreaTitleLabel_header->addWidget(DetectionCollapseButton);


        verticalLayout_DetectionArea->addLayout(horizontalLayout_DetectionAreaTitleLabel_header);

        horizontalLayout_region = new QHBoxLayout();
        horizontalLayout_region->setSpacing(8);
        horizontalLayout_region->setObjectName(QString::fromUtf8("horizontalLayout_region"));
        horizontalLayout_region->setContentsMargins(0, 0, 0, 0);
        regionLabel = new QLabel(DetectionAreaCard);
        regionLabel->setObjectName(QString::fromUtf8("regionLabel"));
        regionLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_region->addWidget(regionLabel);

        regionButtonsFrame = new QFrame(DetectionAreaCard);
        regionButtonsFrame->setObjectName(QString::fromUtf8("regionButtonsFrame"));
        regionButtonsFrame->setMinimumSize(QSize(260, 38));
        regionButtonsFrame->setMaximumSize(QSize(260, 38));
        horizontalLayout_regionButtons = new QHBoxLayout(regionButtonsFrame);
        horizontalLayout_regionButtons->setSpacing(6);
        horizontalLayout_regionButtons->setObjectName(QString::fromUtf8("horizontalLayout_regionButtons"));
        horizontalLayout_regionButtons->setContentsMargins(0, 0, 0, 0);
        regionDrawButton = new QToolButton(regionButtonsFrame);
        regionDrawButton->setObjectName(QString::fromUtf8("regionDrawButton"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(regionDrawButton->sizePolicy().hasHeightForWidth());
        regionDrawButton->setSizePolicy(sizePolicy);
        regionDrawButton->setMinimumSize(QSize(0, 38));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/fit.svg"), QSize(), QIcon::Normal, QIcon::Off);
        regionDrawButton->setIcon(icon4);
        regionDrawButton->setIconSize(QSize(22, 22));
        regionDrawButton->setCheckable(true);

        horizontalLayout_regionButtons->addWidget(regionDrawButton);

        regionRectButton = new QToolButton(regionButtonsFrame);
        regionRectButton->setObjectName(QString::fromUtf8("regionRectButton"));
        sizePolicy.setHeightForWidth(regionRectButton->sizePolicy().hasHeightForWidth());
        regionRectButton->setSizePolicy(sizePolicy);
        regionRectButton->setMinimumSize(QSize(0, 38));
        regionRectButton->setCheckable(true);

        horizontalLayout_regionButtons->addWidget(regionRectButton);


        horizontalLayout_region->addWidget(regionButtonsFrame);


        verticalLayout_DetectionArea->addLayout(horizontalLayout_region);

        horizontalLayout_positionEnable = new QHBoxLayout();
        horizontalLayout_positionEnable->setSpacing(8);
        horizontalLayout_positionEnable->setObjectName(QString::fromUtf8("horizontalLayout_positionEnable"));
        horizontalLayout_positionEnable->setContentsMargins(0, 0, 0, 0);
        positionEnableLabel = new QLabel(DetectionAreaCard);
        positionEnableLabel->setObjectName(QString::fromUtf8("positionEnableLabel"));
        positionEnableLabel->setMinimumSize(QSize(260, 0));

        horizontalLayout_positionEnable->addWidget(positionEnableLabel);

        horizontalSpacer_horizontalLayout_positionEnable = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_positionEnable->addItem(horizontalSpacer_horizontalLayout_positionEnable);

        positionCorrectionSwitch = new QCheckBox(DetectionAreaCard);
        positionCorrectionSwitch->setObjectName(QString::fromUtf8("positionCorrectionSwitch"));
        positionCorrectionSwitch->setChecked(true);

        horizontalLayout_positionEnable->addWidget(positionCorrectionSwitch);


        verticalLayout_DetectionArea->addLayout(horizontalLayout_positionEnable);

        horizontalLayout_positionSource = new QHBoxLayout();
        horizontalLayout_positionSource->setSpacing(8);
        horizontalLayout_positionSource->setObjectName(QString::fromUtf8("horizontalLayout_positionSource"));
        horizontalLayout_positionSource->setContentsMargins(0, 0, 0, 0);
        positionSourceLabel = new QLabel(DetectionAreaCard);
        positionSourceLabel->setObjectName(QString::fromUtf8("positionSourceLabel"));
        positionSourceLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_positionSource->addWidget(positionSourceLabel);

        horizontalSpacer_horizontalLayout_positionSource = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_positionSource->addItem(horizontalSpacer_horizontalLayout_positionSource);

        positionCorrectionComboBox = new QComboBox(DetectionAreaCard);
        positionCorrectionComboBox->addItem(QString());
        positionCorrectionComboBox->setObjectName(QString::fromUtf8("positionCorrectionComboBox"));
        positionCorrectionComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_positionSource->addWidget(positionCorrectionComboBox);


        verticalLayout_DetectionArea->addLayout(horizontalLayout_positionSource);


        verticalLayout_classificationBasicConfigCards->addWidget(DetectionAreaCard);

        ModelListCard = new QFrame(classificationBasicContents);
        ModelListCard->setObjectName(QString::fromUtf8("ModelListCard"));
        ModelListCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_ModelList = new QVBoxLayout(ModelListCard);
        verticalLayout_ModelList->setSpacing(14);
        verticalLayout_ModelList->setObjectName(QString::fromUtf8("verticalLayout_ModelList"));
        verticalLayout_ModelList->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_Model = new QHBoxLayout();
        horizontalLayout_Model->setSpacing(8);
        horizontalLayout_Model->setObjectName(QString::fromUtf8("horizontalLayout_Model"));
        horizontalLayout_Model->setContentsMargins(0, 0, 0, 0);
        ModelListTitleLabel = new QLabel(ModelListCard);
        ModelListTitleLabel->setObjectName(QString::fromUtf8("ModelListTitleLabel"));
        ModelListTitleLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_Model->addWidget(ModelListTitleLabel);

        horizontalSpacer_Model = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_Model->addItem(horizontalSpacer_Model);

        modelComboBox = new QComboBox(ModelListCard);
        modelComboBox->setObjectName(QString::fromUtf8("modelComboBox"));
        modelComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_Model->addWidget(modelComboBox);


        verticalLayout_ModelList->addLayout(horizontalLayout_Model);

        horizontalLayout_ImportModel = new QHBoxLayout();
        horizontalLayout_ImportModel->setSpacing(8);
        horizontalLayout_ImportModel->setObjectName(QString::fromUtf8("horizontalLayout_ImportModel"));
        horizontalLayout_ImportModel->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_ImportModelLeft = new QSpacerItem(150, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_ImportModel->addItem(horizontalSpacer_ImportModelLeft);

        modelHintLabel = new QLabel(ModelListCard);
        modelHintLabel->setObjectName(QString::fromUtf8("modelHintLabel"));

        horizontalLayout_ImportModel->addWidget(modelHintLabel);

        importModelButton = new QPushButton(ModelListCard);
        importModelButton->setObjectName(QString::fromUtf8("importModelButton"));
        importModelButton->setMinimumSize(QSize(78, 38));

        horizontalLayout_ImportModel->addWidget(importModelButton);


        verticalLayout_ModelList->addLayout(horizontalLayout_ImportModel);


        verticalLayout_classificationBasicConfigCards->addWidget(ModelListCard);

        ResultJudgeCard = new QFrame(classificationBasicContents);
        ResultJudgeCard->setObjectName(QString::fromUtf8("ResultJudgeCard"));
        ResultJudgeCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_ResultJudge = new QVBoxLayout(ResultJudgeCard);
        verticalLayout_ResultJudge->setSpacing(16);
        verticalLayout_ResultJudge->setObjectName(QString::fromUtf8("verticalLayout_ResultJudge"));
        verticalLayout_ResultJudge->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_ResultJudgeTitleLabel_header = new QHBoxLayout();
        horizontalLayout_ResultJudgeTitleLabel_header->setObjectName(QString::fromUtf8("horizontalLayout_ResultJudgeTitleLabel_header"));
        ResultJudgeTitleLabel = new QLabel(ResultJudgeCard);
        ResultJudgeTitleLabel->setObjectName(QString::fromUtf8("ResultJudgeTitleLabel"));

        horizontalLayout_ResultJudgeTitleLabel_header->addWidget(ResultJudgeTitleLabel);

        horizontalSpacer_ResultJudgeTitleLabel_header = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_ResultJudgeTitleLabel_header->addItem(horizontalSpacer_ResultJudgeTitleLabel_header);

        ResultCollapseButton = new QToolButton(ResultJudgeCard);
        ResultCollapseButton->setObjectName(QString::fromUtf8("ResultCollapseButton"));

        horizontalLayout_ResultJudgeTitleLabel_header->addWidget(ResultCollapseButton);


        verticalLayout_ResultJudge->addLayout(horizontalLayout_ResultJudgeTitleLabel_header);

        horizontalLayout_ResultBasis = new QHBoxLayout();
        horizontalLayout_ResultBasis->setSpacing(8);
        horizontalLayout_ResultBasis->setObjectName(QString::fromUtf8("horizontalLayout_ResultBasis"));
        horizontalLayout_ResultBasis->setContentsMargins(0, 0, 0, 0);
        ResultBasisLabel = new QLabel(ResultJudgeCard);
        ResultBasisLabel->setObjectName(QString::fromUtf8("ResultBasisLabel"));
        ResultBasisLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_ResultBasis->addWidget(ResultBasisLabel);

        horizontalSpacer_horizontalLayout_ResultBasis = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_ResultBasis->addItem(horizontalSpacer_horizontalLayout_ResultBasis);

        resultBasisComboBox = new QComboBox(ResultJudgeCard);
        resultBasisComboBox->addItem(QString());
        resultBasisComboBox->addItem(QString());
        resultBasisComboBox->setObjectName(QString::fromUtf8("resultBasisComboBox"));
        resultBasisComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_ResultBasis->addWidget(resultBasisComboBox);


        verticalLayout_ResultJudge->addLayout(horizontalLayout_ResultBasis);

        resultBasisStackedWidget = new QStackedWidget(ResultJudgeCard);
        resultBasisStackedWidget->setObjectName(QString::fromUtf8("resultBasisStackedWidget"));
        ScoreBasisPage = new QWidget();
        ScoreBasisPage->setObjectName(QString::fromUtf8("ScoreBasisPage"));
        horizontalLayout_ScoreBasis = new QHBoxLayout(ScoreBasisPage);
        horizontalLayout_ScoreBasis->setSpacing(8);
        horizontalLayout_ScoreBasis->setObjectName(QString::fromUtf8("horizontalLayout_ScoreBasis"));
        horizontalLayout_ScoreBasis->setContentsMargins(0, 0, 0, 0);
        ScoreBasisLabel = new QLabel(ScoreBasisPage);
        ScoreBasisLabel->setObjectName(QString::fromUtf8("ScoreBasisLabel"));
        ScoreBasisLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_ScoreBasis->addWidget(ScoreBasisLabel);

        horizontalSpacer_ScoreBasis = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_ScoreBasis->addItem(horizontalSpacer_ScoreBasis);

        minScoreSpinBox = new QSpinBox(ScoreBasisPage);
        minScoreSpinBox->setObjectName(QString::fromUtf8("minScoreSpinBox"));
        minScoreSpinBox->setMinimumSize(QSize(260, 38));
        minScoreSpinBox->setMinimum(1);
        minScoreSpinBox->setMaximum(100);
        minScoreSpinBox->setValue(50);

        horizontalLayout_ScoreBasis->addWidget(minScoreSpinBox);

        resultBasisStackedWidget->addWidget(ScoreBasisPage);
        CategoryBasisPage = new QWidget();
        CategoryBasisPage->setObjectName(QString::fromUtf8("CategoryBasisPage"));
        horizontalLayout_CategoryBasis = new QHBoxLayout(CategoryBasisPage);
        horizontalLayout_CategoryBasis->setSpacing(8);
        horizontalLayout_CategoryBasis->setObjectName(QString::fromUtf8("horizontalLayout_CategoryBasis"));
        horizontalLayout_CategoryBasis->setContentsMargins(0, 0, 0, 0);
        CategoryBasisLabel = new QLabel(CategoryBasisPage);
        CategoryBasisLabel->setObjectName(QString::fromUtf8("CategoryBasisLabel"));
        CategoryBasisLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_CategoryBasis->addWidget(CategoryBasisLabel);

        horizontalSpacer_CategoryBasis = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_CategoryBasis->addItem(horizontalSpacer_CategoryBasis);

        categoryLineEdit = new QLineEdit(CategoryBasisPage);
        categoryLineEdit->setObjectName(QString::fromUtf8("categoryLineEdit"));
        categoryLineEdit->setMinimumSize(QSize(260, 38));

        horizontalLayout_CategoryBasis->addWidget(categoryLineEdit);

        resultBasisStackedWidget->addWidget(CategoryBasisPage);

        verticalLayout_ResultJudge->addWidget(resultBasisStackedWidget);


        verticalLayout_classificationBasicConfigCards->addWidget(ResultJudgeCard);

        verticalSpacer_classificationBasicContents = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_classificationBasicConfigCards->addItem(verticalSpacer_classificationBasicContents);

        classificationBasicScrollArea->setWidget(classificationBasicContents);

        verticalLayout_basicParamsPage->addWidget(classificationBasicScrollArea);

        classificationParamsStackedWidget->addWidget(basicParamsPage);
        allParamsPage = new QWidget();
        allParamsPage->setObjectName(QString::fromUtf8("allParamsPage"));
        verticalLayout_allParamsPage = new QVBoxLayout(allParamsPage);
        verticalLayout_allParamsPage->setObjectName(QString::fromUtf8("verticalLayout_allParamsPage"));
        verticalLayout_allParamsPage->setContentsMargins(0, 0, 0, 0);
        classificationAllScrollArea = new QScrollArea(allParamsPage);
        classificationAllScrollArea->setObjectName(QString::fromUtf8("classificationAllScrollArea"));
        classificationAllScrollArea->setFrameShape(QFrame::NoFrame);
        classificationAllScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        classificationAllScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        classificationAllScrollArea->setWidgetResizable(true);
        classificationAllContents = new QWidget();
        classificationAllContents->setObjectName(QString::fromUtf8("classificationAllContents"));
        classificationAllContents->setGeometry(QRect(0, -687, 540, 1379));
        verticalLayout_classificationAllConfigCards = new QVBoxLayout(classificationAllContents);
        verticalLayout_classificationAllConfigCards->setSpacing(12);
        verticalLayout_classificationAllConfigCards->setObjectName(QString::fromUtf8("verticalLayout_classificationAllConfigCards"));
        verticalLayout_classificationAllConfigCards->setContentsMargins(0, 0, 0, 0);
        allDetectionAreaCard = new QFrame(classificationAllContents);
        allDetectionAreaCard->setObjectName(QString::fromUtf8("allDetectionAreaCard"));
        allDetectionAreaCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_allDetectionArea = new QVBoxLayout(allDetectionAreaCard);
        verticalLayout_allDetectionArea->setSpacing(16);
        verticalLayout_allDetectionArea->setObjectName(QString::fromUtf8("verticalLayout_allDetectionArea"));
        verticalLayout_allDetectionArea->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_allDetectionAreaTitleLabel_header = new QHBoxLayout();
        horizontalLayout_allDetectionAreaTitleLabel_header->setObjectName(QString::fromUtf8("horizontalLayout_allDetectionAreaTitleLabel_header"));
        allDetectionAreaTitleLabel = new QLabel(allDetectionAreaCard);
        allDetectionAreaTitleLabel->setObjectName(QString::fromUtf8("allDetectionAreaTitleLabel"));

        horizontalLayout_allDetectionAreaTitleLabel_header->addWidget(allDetectionAreaTitleLabel);

        horizontalSpacer_allDetectionAreaTitleLabel_header = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allDetectionAreaTitleLabel_header->addItem(horizontalSpacer_allDetectionAreaTitleLabel_header);

        allDetectionCollapseButton = new QToolButton(allDetectionAreaCard);
        allDetectionCollapseButton->setObjectName(QString::fromUtf8("allDetectionCollapseButton"));

        horizontalLayout_allDetectionAreaTitleLabel_header->addWidget(allDetectionCollapseButton);


        verticalLayout_allDetectionArea->addLayout(horizontalLayout_allDetectionAreaTitleLabel_header);

        horizontalLayout_allRegion = new QHBoxLayout();
        horizontalLayout_allRegion->setSpacing(8);
        horizontalLayout_allRegion->setObjectName(QString::fromUtf8("horizontalLayout_allRegion"));
        horizontalLayout_allRegion->setContentsMargins(0, 0, 0, 0);
        allRegionLabel = new QLabel(allDetectionAreaCard);
        allRegionLabel->setObjectName(QString::fromUtf8("allRegionLabel"));
        allRegionLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_allRegion->addWidget(allRegionLabel);

        allRegionButtonsFrame = new QFrame(allDetectionAreaCard);
        allRegionButtonsFrame->setObjectName(QString::fromUtf8("allRegionButtonsFrame"));
        allRegionButtonsFrame->setMinimumSize(QSize(260, 38));
        allRegionButtonsFrame->setMaximumSize(QSize(260, 38));
        horizontalLayout_allRegionButtons = new QHBoxLayout(allRegionButtonsFrame);
        horizontalLayout_allRegionButtons->setSpacing(6);
        horizontalLayout_allRegionButtons->setObjectName(QString::fromUtf8("horizontalLayout_allRegionButtons"));
        horizontalLayout_allRegionButtons->setContentsMargins(0, 0, 0, 0);
        allRegionDrawButton = new QToolButton(allRegionButtonsFrame);
        allRegionDrawButton->setObjectName(QString::fromUtf8("allRegionDrawButton"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(allRegionDrawButton->sizePolicy().hasHeightForWidth());
        allRegionDrawButton->setSizePolicy(sizePolicy1);
        allRegionDrawButton->setMinimumSize(QSize(0, 38));
        allRegionDrawButton->setIcon(icon4);
        allRegionDrawButton->setIconSize(QSize(22, 22));
        allRegionDrawButton->setCheckable(true);

        horizontalLayout_allRegionButtons->addWidget(allRegionDrawButton);

        allRegionRectButton = new QToolButton(allRegionButtonsFrame);
        allRegionRectButton->setObjectName(QString::fromUtf8("allRegionRectButton"));
        sizePolicy1.setHeightForWidth(allRegionRectButton->sizePolicy().hasHeightForWidth());
        allRegionRectButton->setSizePolicy(sizePolicy1);
        allRegionRectButton->setMinimumSize(QSize(0, 38));
        allRegionRectButton->setCheckable(true);

        horizontalLayout_allRegionButtons->addWidget(allRegionRectButton);


        horizontalLayout_allRegion->addWidget(allRegionButtonsFrame);


        verticalLayout_allDetectionArea->addLayout(horizontalLayout_allRegion);

        horizontalLayout_screenRegion = new QHBoxLayout();
        horizontalLayout_screenRegion->setSpacing(8);
        horizontalLayout_screenRegion->setObjectName(QString::fromUtf8("horizontalLayout_screenRegion"));
        horizontalLayout_screenRegion->setContentsMargins(0, 0, 0, 0);
        screenRegionLabel = new QLabel(allDetectionAreaCard);
        screenRegionLabel->setObjectName(QString::fromUtf8("screenRegionLabel"));
        screenRegionLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_screenRegion->addWidget(screenRegionLabel);

        horizontalSpacer_horizontalLayout_screenRegion = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_screenRegion->addItem(horizontalSpacer_horizontalLayout_screenRegion);

        screenRegionEditButton = new QPushButton(allDetectionAreaCard);
        screenRegionEditButton->setObjectName(QString::fromUtf8("screenRegionEditButton"));
        screenRegionEditButton->setMinimumSize(QSize(260, 38));

        horizontalLayout_screenRegion->addWidget(screenRegionEditButton);


        verticalLayout_allDetectionArea->addLayout(horizontalLayout_screenRegion);

        horizontalLayout_allPositionEnable = new QHBoxLayout();
        horizontalLayout_allPositionEnable->setSpacing(8);
        horizontalLayout_allPositionEnable->setObjectName(QString::fromUtf8("horizontalLayout_allPositionEnable"));
        horizontalLayout_allPositionEnable->setContentsMargins(0, 0, 0, 0);
        allPositionEnableLabel = new QLabel(allDetectionAreaCard);
        allPositionEnableLabel->setObjectName(QString::fromUtf8("allPositionEnableLabel"));
        allPositionEnableLabel->setMinimumSize(QSize(260, 0));

        horizontalLayout_allPositionEnable->addWidget(allPositionEnableLabel);

        horizontalSpacer_horizontalLayout_allPositionEnable = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allPositionEnable->addItem(horizontalSpacer_horizontalLayout_allPositionEnable);

        allPositionCorrectionSwitch = new QCheckBox(allDetectionAreaCard);
        allPositionCorrectionSwitch->setObjectName(QString::fromUtf8("allPositionCorrectionSwitch"));
        allPositionCorrectionSwitch->setChecked(true);

        horizontalLayout_allPositionEnable->addWidget(allPositionCorrectionSwitch);


        verticalLayout_allDetectionArea->addLayout(horizontalLayout_allPositionEnable);

        horizontalLayout_allPositionSource = new QHBoxLayout();
        horizontalLayout_allPositionSource->setSpacing(8);
        horizontalLayout_allPositionSource->setObjectName(QString::fromUtf8("horizontalLayout_allPositionSource"));
        horizontalLayout_allPositionSource->setContentsMargins(0, 0, 0, 0);
        allPositionSourceLabel = new QLabel(allDetectionAreaCard);
        allPositionSourceLabel->setObjectName(QString::fromUtf8("allPositionSourceLabel"));
        allPositionSourceLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_allPositionSource->addWidget(allPositionSourceLabel);

        horizontalSpacer_horizontalLayout_allPositionSource = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allPositionSource->addItem(horizontalSpacer_horizontalLayout_allPositionSource);

        allPositionCorrectionComboBox = new QComboBox(allDetectionAreaCard);
        allPositionCorrectionComboBox->addItem(QString());
        allPositionCorrectionComboBox->setObjectName(QString::fromUtf8("allPositionCorrectionComboBox"));
        allPositionCorrectionComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_allPositionSource->addWidget(allPositionCorrectionComboBox);


        verticalLayout_allDetectionArea->addLayout(horizontalLayout_allPositionSource);


        verticalLayout_classificationAllConfigCards->addWidget(allDetectionAreaCard);

        allModelListCard = new QFrame(classificationAllContents);
        allModelListCard->setObjectName(QString::fromUtf8("allModelListCard"));
        allModelListCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_allModelList = new QVBoxLayout(allModelListCard);
        verticalLayout_allModelList->setSpacing(14);
        verticalLayout_allModelList->setObjectName(QString::fromUtf8("verticalLayout_allModelList"));
        verticalLayout_allModelList->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_allModel = new QHBoxLayout();
        horizontalLayout_allModel->setSpacing(8);
        horizontalLayout_allModel->setObjectName(QString::fromUtf8("horizontalLayout_allModel"));
        horizontalLayout_allModel->setContentsMargins(0, 0, 0, 0);
        allModelListTitleLabel = new QLabel(allModelListCard);
        allModelListTitleLabel->setObjectName(QString::fromUtf8("allModelListTitleLabel"));
        allModelListTitleLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_allModel->addWidget(allModelListTitleLabel);

        horizontalSpacer_allModel = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allModel->addItem(horizontalSpacer_allModel);

        allModelComboBox = new QComboBox(allModelListCard);
        allModelComboBox->setObjectName(QString::fromUtf8("allModelComboBox"));
        allModelComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_allModel->addWidget(allModelComboBox);


        verticalLayout_allModelList->addLayout(horizontalLayout_allModel);

        horizontalLayout_allImportModel = new QHBoxLayout();
        horizontalLayout_allImportModel->setSpacing(8);
        horizontalLayout_allImportModel->setObjectName(QString::fromUtf8("horizontalLayout_allImportModel"));
        horizontalLayout_allImportModel->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_allImportModelLeft = new QSpacerItem(150, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allImportModel->addItem(horizontalSpacer_allImportModelLeft);

        allModelHintLabel = new QLabel(allModelListCard);
        allModelHintLabel->setObjectName(QString::fromUtf8("allModelHintLabel"));

        horizontalLayout_allImportModel->addWidget(allModelHintLabel);

        allImportModelButton = new QPushButton(allModelListCard);
        allImportModelButton->setObjectName(QString::fromUtf8("allImportModelButton"));
        allImportModelButton->setMinimumSize(QSize(78, 38));

        horizontalLayout_allImportModel->addWidget(allImportModelButton);


        verticalLayout_allModelList->addLayout(horizontalLayout_allImportModel);


        verticalLayout_classificationAllConfigCards->addWidget(allModelListCard);

        allResultJudgeCard = new QFrame(classificationAllContents);
        allResultJudgeCard->setObjectName(QString::fromUtf8("allResultJudgeCard"));
        allResultJudgeCard->setFrameShape(QFrame::NoFrame);
        verticalLayout_allResultJudge = new QVBoxLayout(allResultJudgeCard);
        verticalLayout_allResultJudge->setSpacing(16);
        verticalLayout_allResultJudge->setObjectName(QString::fromUtf8("verticalLayout_allResultJudge"));
        verticalLayout_allResultJudge->setContentsMargins(20, 18, 20, 18);
        horizontalLayout_allResultJudgeTitleLabel_header = new QHBoxLayout();
        horizontalLayout_allResultJudgeTitleLabel_header->setObjectName(QString::fromUtf8("horizontalLayout_allResultJudgeTitleLabel_header"));
        allResultJudgeTitleLabel = new QLabel(allResultJudgeCard);
        allResultJudgeTitleLabel->setObjectName(QString::fromUtf8("allResultJudgeTitleLabel"));

        horizontalLayout_allResultJudgeTitleLabel_header->addWidget(allResultJudgeTitleLabel);

        horizontalSpacer_allResultJudgeTitleLabel_header = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allResultJudgeTitleLabel_header->addItem(horizontalSpacer_allResultJudgeTitleLabel_header);

        allResultCollapseButton = new QToolButton(allResultJudgeCard);
        allResultCollapseButton->setObjectName(QString::fromUtf8("allResultCollapseButton"));

        horizontalLayout_allResultJudgeTitleLabel_header->addWidget(allResultCollapseButton);


        verticalLayout_allResultJudge->addLayout(horizontalLayout_allResultJudgeTitleLabel_header);

        horizontalLayout_allResultBasis = new QHBoxLayout();
        horizontalLayout_allResultBasis->setSpacing(8);
        horizontalLayout_allResultBasis->setObjectName(QString::fromUtf8("horizontalLayout_allResultBasis"));
        horizontalLayout_allResultBasis->setContentsMargins(0, 0, 0, 0);
        allResultBasisLabel = new QLabel(allResultJudgeCard);
        allResultBasisLabel->setObjectName(QString::fromUtf8("allResultBasisLabel"));
        allResultBasisLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_allResultBasis->addWidget(allResultBasisLabel);

        horizontalSpacer_horizontalLayout_allResultBasis = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allResultBasis->addItem(horizontalSpacer_horizontalLayout_allResultBasis);

        allResultBasisComboBox = new QComboBox(allResultJudgeCard);
        allResultBasisComboBox->addItem(QString());
        allResultBasisComboBox->addItem(QString());
        allResultBasisComboBox->setObjectName(QString::fromUtf8("allResultBasisComboBox"));
        allResultBasisComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_allResultBasis->addWidget(allResultBasisComboBox);


        verticalLayout_allResultJudge->addLayout(horizontalLayout_allResultBasis);

        allResultBasisStackedWidget = new QStackedWidget(allResultJudgeCard);
        allResultBasisStackedWidget->setObjectName(QString::fromUtf8("allResultBasisStackedWidget"));
        allScoreBasisPage = new QWidget();
        allScoreBasisPage->setObjectName(QString::fromUtf8("allScoreBasisPage"));
        horizontalLayout_allScoreBasis = new QHBoxLayout(allScoreBasisPage);
        horizontalLayout_allScoreBasis->setSpacing(8);
        horizontalLayout_allScoreBasis->setObjectName(QString::fromUtf8("horizontalLayout_allScoreBasis"));
        horizontalLayout_allScoreBasis->setContentsMargins(0, 0, 0, 0);
        allScoreBasisLabel = new QLabel(allScoreBasisPage);
        allScoreBasisLabel->setObjectName(QString::fromUtf8("allScoreBasisLabel"));
        allScoreBasisLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_allScoreBasis->addWidget(allScoreBasisLabel);

        horizontalSpacer_allScoreBasis = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allScoreBasis->addItem(horizontalSpacer_allScoreBasis);

        allMinScoreSpinBox = new QSpinBox(allScoreBasisPage);
        allMinScoreSpinBox->setObjectName(QString::fromUtf8("allMinScoreSpinBox"));
        allMinScoreSpinBox->setMinimumSize(QSize(260, 38));
        allMinScoreSpinBox->setMinimum(1);
        allMinScoreSpinBox->setMaximum(100);
        allMinScoreSpinBox->setValue(50);

        horizontalLayout_allScoreBasis->addWidget(allMinScoreSpinBox);

        allResultBasisStackedWidget->addWidget(allScoreBasisPage);
        allCategoryBasisPage = new QWidget();
        allCategoryBasisPage->setObjectName(QString::fromUtf8("allCategoryBasisPage"));
        horizontalLayout_allCategoryBasis = new QHBoxLayout(allCategoryBasisPage);
        horizontalLayout_allCategoryBasis->setSpacing(8);
        horizontalLayout_allCategoryBasis->setObjectName(QString::fromUtf8("horizontalLayout_allCategoryBasis"));
        horizontalLayout_allCategoryBasis->setContentsMargins(0, 0, 0, 0);
        allCategoryBasisLabel = new QLabel(allCategoryBasisPage);
        allCategoryBasisLabel->setObjectName(QString::fromUtf8("allCategoryBasisLabel"));
        allCategoryBasisLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_allCategoryBasis->addWidget(allCategoryBasisLabel);

        horizontalSpacer_allCategoryBasis = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allCategoryBasis->addItem(horizontalSpacer_allCategoryBasis);

        allCategoryLineEdit = new QLineEdit(allCategoryBasisPage);
        allCategoryLineEdit->setObjectName(QString::fromUtf8("allCategoryLineEdit"));
        allCategoryLineEdit->setMinimumSize(QSize(260, 38));

        horizontalLayout_allCategoryBasis->addWidget(allCategoryLineEdit);

        allResultBasisStackedWidget->addWidget(allCategoryBasisPage);

        verticalLayout_allResultJudge->addWidget(allResultBasisStackedWidget);

        horizontalLayout_allJudgeType = new QHBoxLayout();
        horizontalLayout_allJudgeType->setSpacing(8);
        horizontalLayout_allJudgeType->setObjectName(QString::fromUtf8("horizontalLayout_allJudgeType"));
        horizontalLayout_allJudgeType->setContentsMargins(0, 0, 0, 0);
        allJudgeTypeLabel = new QLabel(allResultJudgeCard);
        allJudgeTypeLabel->setObjectName(QString::fromUtf8("allJudgeTypeLabel"));
        allJudgeTypeLabel->setMinimumSize(QSize(150, 0));

        horizontalLayout_allJudgeType->addWidget(allJudgeTypeLabel);

        horizontalSpacer_allJudgeType = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allJudgeType->addItem(horizontalSpacer_allJudgeType);

        allJudgeTypeComboBox = new QComboBox(allResultJudgeCard);
        allJudgeTypeComboBox->addItem(QString());
        allJudgeTypeComboBox->addItem(QString());
        allJudgeTypeComboBox->setObjectName(QString::fromUtf8("allJudgeTypeComboBox"));
        allJudgeTypeComboBox->setMinimumSize(QSize(260, 38));

        horizontalLayout_allJudgeType->addWidget(allJudgeTypeComboBox);


        verticalLayout_allResultJudge->addLayout(horizontalLayout_allJudgeType);


        verticalLayout_classificationAllConfigCards->addWidget(allResultJudgeCard);

        verticalSpacer_classificationAllContents = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_classificationAllConfigCards->addItem(verticalSpacer_classificationAllContents);

        classificationAllScrollArea->setWidget(classificationAllContents);

        verticalLayout_allParamsPage->addWidget(classificationAllScrollArea);

        classificationParamsStackedWidget->addWidget(allParamsPage);

        verticalLayout_editor->addWidget(classificationParamsStackedWidget);

        horizontalLayout_actions = new QHBoxLayout();
        horizontalLayout_actions->setObjectName(QString::fromUtf8("horizontalLayout_actions"));
        horizontalSpacer_actions = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_actions->addItem(horizontalSpacer_actions);

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


        retranslateUi(ClassificationDialog);

        classificationParamsStackedWidget->setCurrentIndex(0);
        resultBasisStackedWidget->setCurrentIndex(0);
        allResultBasisStackedWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(ClassificationDialog);
    } // setupUi

    void retranslateUi(QDialog *ClassificationDialog)
    {
        ClassificationDialog->setWindowTitle(QCoreApplication::translate("ClassificationDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \345\210\206\347\261\273", nullptr));
        setupWindowTitleLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("ClassificationDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("ClassificationDialog", "\303\227", nullptr));
        setupPageCodeLabel->setText(QCoreApplication::translate("ClassificationDialog", "428", nullptr));
        setupSaveButton->setText(QCoreApplication::translate("ClassificationDialog", "\344\277\235\345\255\230", nullptr));
        setupSaveAsButton->setText(QCoreApplication::translate("ClassificationDialog", "\345\217\246\345\255\230\344\270\272", nullptr));
        setupExportButton->setText(QCoreApplication::translate("ClassificationDialog", "IO\350\276\223\345\207\272", nullptr));
        editorTitleLabel->setText(QCoreApplication::translate("ClassificationDialog", "\345\210\206\347\261\273", nullptr));
        basicSegmentButton->setText(QCoreApplication::translate("ClassificationDialog", "\345\237\272\347\241\200", nullptr));
        allSegmentButton->setText(QCoreApplication::translate("ClassificationDialog", "\345\205\250\351\203\250", nullptr));
        DetectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ClassificationDialog", "configCard", nullptr)));
        DetectionAreaTitleLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        DetectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "cardTitle", nullptr)));
        DetectionCollapseButton->setText(QCoreApplication::translate("ClassificationDialog", "\342\214\204", nullptr));
        DetectionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "collapseCard", nullptr)));
        regionLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\243\200\346\265\213\345\214\272", nullptr));
        regionLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        regionButtonsFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("ClassificationDialog", "optionGroup", nullptr)));
        regionDrawButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ClassificationDialog", "toolbarIcon", nullptr)));
        regionRectButton->setText(QCoreApplication::translate("ClassificationDialog", "\342\226\241", nullptr));
        regionRectButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ClassificationDialog", "toolbarIcon", nullptr)));
        positionEnableLabel->setText(QCoreApplication::translate("ClassificationDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243\344\275\277\350\203\275 \342\223\230", nullptr));
        positionEnableLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        positionCorrectionSwitch->setText(QString());
        positionSourceLabel->setText(QCoreApplication::translate("ClassificationDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        positionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        positionCorrectionComboBox->setItemText(0, QCoreApplication::translate("ClassificationDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        ModelListCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ClassificationDialog", "configCard", nullptr)));
        ModelListTitleLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\250\241\345\236\213\345\210\227\350\241\250", nullptr));
        ModelListTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "cardTitle", nullptr)));
        modelHintLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\262\241\346\234\211\345\220\210\351\200\202\347\232\204\346\250\241\345\236\213\357\274\237", nullptr));
        modelHintLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "cardHint", nullptr)));
        importModelButton->setText(QCoreApplication::translate("ClassificationDialog", "\345\257\274\345\205\245", nullptr));
        ResultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ClassificationDialog", "configCard", nullptr)));
        ResultJudgeTitleLabel->setText(QCoreApplication::translate("ClassificationDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        ResultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "cardTitle", nullptr)));
        ResultCollapseButton->setText(QCoreApplication::translate("ClassificationDialog", "\342\214\204", nullptr));
        ResultCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "collapseCard", nullptr)));
        ResultBasisLabel->setText(QCoreApplication::translate("ClassificationDialog", "\345\210\244\346\226\255\344\276\235\346\215\256", nullptr));
        ResultBasisLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        resultBasisComboBox->setItemText(0, QCoreApplication::translate("ClassificationDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));
        resultBasisComboBox->setItemText(1, QCoreApplication::translate("ClassificationDialog", "\347\261\273\345\210\253\345\210\244\346\226\255", nullptr));

        ScoreBasisLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));
        ScoreBasisLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        CategoryBasisLabel->setText(QCoreApplication::translate("ClassificationDialog", "\347\261\273\345\210\253", nullptr));
        CategoryBasisLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        categoryLineEdit->setText(QCoreApplication::translate("ClassificationDialog", "1", nullptr));
        allDetectionAreaCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ClassificationDialog", "configCard", nullptr)));
        allDetectionAreaTitleLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        allDetectionAreaTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "cardTitle", nullptr)));
        allDetectionCollapseButton->setText(QCoreApplication::translate("ClassificationDialog", "\342\214\204", nullptr));
        allDetectionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "collapseCard", nullptr)));
        allRegionLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\243\200\346\265\213\345\214\272", nullptr));
        allRegionLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        allRegionButtonsFrame->setProperty("panelRole", QVariant(QCoreApplication::translate("ClassificationDialog", "optionGroup", nullptr)));
        allRegionDrawButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ClassificationDialog", "toolbarIcon", nullptr)));
        allRegionRectButton->setText(QCoreApplication::translate("ClassificationDialog", "\342\226\241", nullptr));
        allRegionRectButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ClassificationDialog", "toolbarIcon", nullptr)));
        screenRegionLabel->setText(QCoreApplication::translate("ClassificationDialog", "\345\261\217\350\224\275\345\214\272\345\237\237", nullptr));
        screenRegionLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        screenRegionEditButton->setText(QCoreApplication::translate("ClassificationDialog", "\347\274\226\350\276\221", nullptr));
        screenRegionEditButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ClassificationDialog", "gray", nullptr)));
        allPositionEnableLabel->setText(QCoreApplication::translate("ClassificationDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243\344\275\277\350\203\275 \342\223\230", nullptr));
        allPositionEnableLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        allPositionCorrectionSwitch->setText(QString());
        allPositionSourceLabel->setText(QCoreApplication::translate("ClassificationDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        allPositionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        allPositionCorrectionComboBox->setItemText(0, QCoreApplication::translate("ClassificationDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        allModelListCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ClassificationDialog", "configCard", nullptr)));
        allModelListTitleLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\250\241\345\236\213\345\210\227\350\241\250", nullptr));
        allModelListTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "cardTitle", nullptr)));
        allModelHintLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\262\241\346\234\211\345\220\210\351\200\202\347\232\204\346\250\241\345\236\213\357\274\237", nullptr));
        allModelHintLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "cardHint", nullptr)));
        allImportModelButton->setText(QCoreApplication::translate("ClassificationDialog", "\345\257\274\345\205\245", nullptr));
        allResultJudgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ClassificationDialog", "configCard", nullptr)));
        allResultJudgeTitleLabel->setText(QCoreApplication::translate("ClassificationDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        allResultJudgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "cardTitle", nullptr)));
        allResultCollapseButton->setText(QCoreApplication::translate("ClassificationDialog", "\342\214\204", nullptr));
        allResultCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "collapseCard", nullptr)));
        allResultBasisLabel->setText(QCoreApplication::translate("ClassificationDialog", "\345\210\244\346\226\255\344\276\235\346\215\256", nullptr));
        allResultBasisLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        allResultBasisComboBox->setItemText(0, QCoreApplication::translate("ClassificationDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));
        allResultBasisComboBox->setItemText(1, QCoreApplication::translate("ClassificationDialog", "\347\261\273\345\210\253\345\210\244\346\226\255", nullptr));

        allScoreBasisLabel->setText(QCoreApplication::translate("ClassificationDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));
        allScoreBasisLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        allCategoryBasisLabel->setText(QCoreApplication::translate("ClassificationDialog", "\347\261\273\345\210\253", nullptr));
        allCategoryBasisLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        allCategoryLineEdit->setText(QCoreApplication::translate("ClassificationDialog", "1", nullptr));
        allJudgeTypeLabel->setText(QCoreApplication::translate("ClassificationDialog", "\345\210\244\346\226\255\347\261\273\345\236\213", nullptr));
        allJudgeTypeLabel->setProperty("role", QVariant(QCoreApplication::translate("ClassificationDialog", "rowField", nullptr)));
        allJudgeTypeComboBox->setItemText(0, QCoreApplication::translate("ClassificationDialog", "\346\211\200\346\234\211\346\243\200\346\265\213\345\214\272\345\237\237\350\276\223\345\207\272\347\273\223\346\236\234\344\270\272 OK", nullptr));
        allJudgeTypeComboBox->setItemText(1, QCoreApplication::translate("ClassificationDialog", "\344\273\273\346\204\217\346\243\200\346\265\213\345\214\272\345\237\237\350\276\223\345\207\272\347\273\223\346\236\234\344\270\272 OK", nullptr));

        testRunButton->setText(QCoreApplication::translate("ClassificationDialog", "\346\265\213\350\257\225\350\277\220\350\241\214", nullptr));
        testRunButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ClassificationDialog", "secondary", nullptr)));
        finishButton->setText(QCoreApplication::translate("ClassificationDialog", "\345\256\214\346\210\220", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("ClassificationDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerZoomOutButton->setText(QCoreApplication::translate("ClassificationDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("ClassificationDialog", "39%", nullptr));
        viewerStatusLabel->setText(QCoreApplication::translate("ClassificationDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: --ms   \345\267\245\345\205\267\350\200\227\346\227\266: --ms", nullptr));
        viewerCursorLabel->setText(QCoreApplication::translate("ClassificationDialog", "X: --  Y: ---   |   R: -- G: -- B: --", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ClassificationDialog: public Ui_ClassificationDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CLASSIFICATIONDIALOG_H
