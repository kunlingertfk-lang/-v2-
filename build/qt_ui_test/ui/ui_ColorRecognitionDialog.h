/********************************************************************************
** Form generated from reading UI file 'ColorRecognitionDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_COLORRECOGNITIONDIALOG_H
#define UI_COLORRECOGNITIONDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ColorRecognitionDialog
{
public:
    QVBoxLayout *rootLayout;
    QFrame *headerFrame;
    QHBoxLayout *headerLayout;
    QLabel *headerTitleLabel;
    QSpacerItem *headerSpacer;
    QToolButton *headerMaximizeButton;
    QToolButton *headerCloseButton;
    QHBoxLayout *contentLayout;
    QFrame *leftPanel;
    QVBoxLayout *leftLayout;
    QHBoxLayout *titleLayout;
    QLabel *dialogTitleLabel;
    QSpacerItem *titleSpacer;
    QPushButton *basicSegmentButton;
    QPushButton *allSegmentButton;
    QStackedWidget *colorParamsStackedWidget;
    QWidget *basicParamsPage;
    QVBoxLayout *basicParamsLayout;
    QFrame *templateCard;
    QVBoxLayout *templateLayout;
    QHBoxLayout *templateHeaderLayout;
    QLabel *templateTitleLabel;
    QSpacerItem *templateHeaderSpacer;
    QToolButton *templateCollapseButton;
    QListWidget *templateListWidget;
    QLabel *activeTemplateSummaryLabel;
    QHBoxLayout *templateButtonLayout;
    QPushButton *addTemplateButton;
    QPushButton *editTemplateButton;
    QHBoxLayout *templateManageButtonLayout;
    QPushButton *renameTemplateButton;
    QPushButton *deleteTemplateButton;
    QPushButton *importTemplateButton;
    QPushButton *exportTemplateButton;
    QFrame *regionCard;
    QVBoxLayout *regionLayout;
    QHBoxLayout *regionHeaderLayout;
    QLabel *regionTitleLabel;
    QSpacerItem *regionHeaderSpacer;
    QToolButton *regionCollapseButton;
    QHBoxLayout *regionButtonLayout;
    QLabel *regionLabel;
    QSpacerItem *regionButtonSpacer;
    QToolButton *regionDrawButton;
    QToolButton *regionRectButton;
    QToolButton *regionCircleButton;
    QHBoxLayout *positionCorrectionEnableLayout;
    QLabel *positionCorrectionEnableLabel;
    QSpacerItem *positionCorrectionEnableSpacer;
    QCheckBox *positionCorrectionSwitch;
    QWidget *positionCorrectionSourceRow;
    QHBoxLayout *positionCorrectionSourceLayout;
    QLabel *positionCorrectionSourceLabel;
    QComboBox *positionCorrectionSourceComboBox;
    QFrame *judgeCard;
    QVBoxLayout *judgeLayout;
    QHBoxLayout *judgeHeaderLayout;
    QLabel *judgeTitleLabel;
    QSpacerItem *judgeHeaderSpacer;
    QToolButton *judgeCollapseButton;
    QHBoxLayout *resultBasisLayout;
    QLabel *resultBasisLabel;
    QComboBox *resultBasisComboBox;
    QHBoxLayout *colorDecisionModeLayout;
    QLabel *colorDecisionModeLabel;
    QComboBox *colorDecisionModeComboBox;
    QHBoxLayout *minScoreLayout;
    QLabel *minScoreLabel;
    QSpinBox *minScoreSpinBox;
    QHBoxLayout *expectedLabelLayout;
    QLabel *expectedLabelTitleLabel;
    QComboBox *expectedLabelComboBox;
    QSpacerItem *basicParamsSpacer;
    QWidget *allParamsPage;
    QVBoxLayout *allParamsLayout;
    QFrame *allTemplateCard;
    QVBoxLayout *allTemplateLayout;
    QLabel *allTemplateTitleLabel;
    QLabel *allTemplateSummaryLabel;
    QSpacerItem *allParamsSpacer;
    QHBoxLayout *bottomButtonLayout;
    QSpacerItem *bottomButtonSpacer;
    QPushButton *testRunButton;
    QPushButton *finishButton;
    QFrame *previewPanel;
    QVBoxLayout *previewLayout;
    QHBoxLayout *viewerHeaderLayout;
    QLabel *viewerTitleLabel;
    QSpacerItem *viewerHeaderSpacer;
    QToolButton *viewerGridButton;
    QToolButton *viewerZoomSearchButton;
    QToolButton *viewerZoomOutButton;
    QLabel *viewerZoomLabel;
    QToolButton *viewerZoomInButton;
    QToolButton *viewerFullButton;
    QGraphicsView *previewGraphicsView;
    QLabel *viewerStatusLabel;

    void setupUi(QDialog *ColorRecognitionDialog)
    {
        if (ColorRecognitionDialog->objectName().isEmpty())
            ColorRecognitionDialog->setObjectName(QString::fromUtf8("ColorRecognitionDialog"));
        ColorRecognitionDialog->resize(1280, 760);
        ColorRecognitionDialog->setStyleSheet(QString::fromUtf8("QDialog { background:#ffffff; color:#111827; }\n"
"QWidget { background:#ffffff; color:#111827; }\n"
"QFrame#headerFrame, QFrame#leftPanel, QFrame#previewPanel { background:#ffffff; color:#111827; }\n"
"QFrame[panelRole=\"configCard\"] { background:#ffffff; color:#111827; border:1px solid #cfd6df; border-radius:6px; }\n"
"QLabel { background:#ffffff; color:#111827; font-size:14px; }\n"
"QLabel[role=\"cardTitle\"] { background:#ffffff; color:#111827; font-size:17px; font-weight:700; }\n"
"QLabel[role=\"rowField\"] { background:#ffffff; color:#111827; font-size:14px; }\n"
"QLabel#viewerTitleLabel, QLabel#viewerStatusLabel, QLabel#viewerZoomLabel { background:#ffffff; color:#111827; }\n"
"QPushButton { background:#ffffff; color:#111827; border:1px solid #cfd6df; border-radius:4px; padding:8px 18px; font-size:15px; }\n"
"QPushButton#finishButton, QPushButton#testRunButton, QPushButton[actionRole=\"secondary\"], QPushButton[actionRole=\"plain\"] { background:#ffffff; color:#111827; border:1px solid #cfd6df; border-"
                        "radius:4px; }\n"
"QPushButton[actionRole=\"plain\"] { padding:6px 12px; }\n"
"QPushButton#basicSegmentButton, QPushButton#allSegmentButton { background:#ffffff; color:#111827; border:1px solid #cfd6df; padding:6px 18px; }\n"
"QPushButton#basicSegmentButton:checked, QPushButton#allSegmentButton:checked { background:#ffffff; color:#111827; border-color:#ff7a00; }\n"
"QToolButton[role=\"collapseCard\"] { background:#ffffff; border:1px solid #cfd6df; color:#111827; font-size:18px; min-width:50px; min-height:40px; }\n"
"QToolButton[actionRole=\"toolbarIcon\"], QToolButton#viewerGridButton, QToolButton#viewerZoomSearchButton, QToolButton#viewerZoomOutButton, QToolButton#viewerZoomInButton, QToolButton#viewerFullButton { background:#ffffff; border:1px solid #cfd6df; border-radius:4px; color:#111827; min-width:34px; min-height:32px; }\n"
"QToolButton[actionRole=\"toolbarIcon\"]:checked { background:#ffffff; border-color:#ff7a00; color:#111827; }\n"
"QListWidget, QComboBox, QSpinBox, QLineEdit { background:#ffffff; bor"
                        "der:1px solid #cfd6df; border-radius:4px; min-height:32px; color:#111827; }\n"
"QListWidget#templateListWidget { font-size:14px; }\n"
"QComboBox QAbstractItemView, QListWidget::item { background:#ffffff; color:#111827; selection-background-color:#ffffff; selection-color:#111827; outline:0; }\n"
"QCheckBox { background:#ffffff; color:#111827; border:1px solid #cfd6df; border-radius:4px; padding:6px 10px; min-height:20px; }\n"
"QCheckBox::indicator { width:16px; height:16px; border:1px solid #9aa6b2; border-radius:3px; background:#ffffff; }\n"
"QCheckBox::indicator:checked { background:#ff7a00; border-color:#ff7a00; }\n"
"QPushButton:disabled, QToolButton:disabled, QComboBox:disabled, QSpinBox:disabled, QLineEdit:disabled { background:#ffffff; color:#111827; border-color:#d7dde6; }\n"
"QGraphicsView { border:1px solid #ff7a00; background:#ffffff; color:#111827; }"));
        rootLayout = new QVBoxLayout(ColorRecognitionDialog);
        rootLayout->setSpacing(0);
        rootLayout->setObjectName(QString::fromUtf8("rootLayout"));
        rootLayout->setContentsMargins(0, 0, 0, 0);
        headerFrame = new QFrame(ColorRecognitionDialog);
        headerFrame->setObjectName(QString::fromUtf8("headerFrame"));
        headerFrame->setMinimumSize(QSize(0, 42));
        headerLayout = new QHBoxLayout(headerFrame);
        headerLayout->setObjectName(QString::fromUtf8("headerLayout"));
        headerLayout->setContentsMargins(18, -1, 12, -1);
        headerTitleLabel = new QLabel(headerFrame);
        headerTitleLabel->setObjectName(QString::fromUtf8("headerTitleLabel"));
        headerTitleLabel->setStyleSheet(QString::fromUtf8("background:#ffffff; color:#111827; font-size:15px; font-weight:600;"));

        headerLayout->addWidget(headerTitleLabel);

        headerSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        headerLayout->addItem(headerSpacer);

        headerMaximizeButton = new QToolButton(headerFrame);
        headerMaximizeButton->setObjectName(QString::fromUtf8("headerMaximizeButton"));
        headerMaximizeButton->setStyleSheet(QString::fromUtf8("background:#ffffff; color:#111827; border:0; font-size:20px;"));

        headerLayout->addWidget(headerMaximizeButton);

        headerCloseButton = new QToolButton(headerFrame);
        headerCloseButton->setObjectName(QString::fromUtf8("headerCloseButton"));
        headerCloseButton->setStyleSheet(QString::fromUtf8("background:#ffffff; color:#111827; border:0; font-size:20px;"));

        headerLayout->addWidget(headerCloseButton);


        rootLayout->addWidget(headerFrame);

        contentLayout = new QHBoxLayout();
        contentLayout->setSpacing(0);
        contentLayout->setObjectName(QString::fromUtf8("contentLayout"));
        leftPanel = new QFrame(ColorRecognitionDialog);
        leftPanel->setObjectName(QString::fromUtf8("leftPanel"));
        leftPanel->setMinimumSize(QSize(390, 0));
        leftPanel->setMaximumSize(QSize(450, 16777215));
        leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setSpacing(14);
        leftLayout->setObjectName(QString::fromUtf8("leftLayout"));
        leftLayout->setContentsMargins(24, 18, 24, 18);
        titleLayout = new QHBoxLayout();
        titleLayout->setObjectName(QString::fromUtf8("titleLayout"));
        dialogTitleLabel = new QLabel(leftPanel);
        dialogTitleLabel->setObjectName(QString::fromUtf8("dialogTitleLabel"));

        titleLayout->addWidget(dialogTitleLabel);

        titleSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        titleLayout->addItem(titleSpacer);

        basicSegmentButton = new QPushButton(leftPanel);
        basicSegmentButton->setObjectName(QString::fromUtf8("basicSegmentButton"));
        basicSegmentButton->setCheckable(true);

        titleLayout->addWidget(basicSegmentButton);

        allSegmentButton = new QPushButton(leftPanel);
        allSegmentButton->setObjectName(QString::fromUtf8("allSegmentButton"));
        allSegmentButton->setCheckable(true);

        titleLayout->addWidget(allSegmentButton);


        leftLayout->addLayout(titleLayout);

        colorParamsStackedWidget = new QStackedWidget(leftPanel);
        colorParamsStackedWidget->setObjectName(QString::fromUtf8("colorParamsStackedWidget"));
        basicParamsPage = new QWidget();
        basicParamsPage->setObjectName(QString::fromUtf8("basicParamsPage"));
        basicParamsLayout = new QVBoxLayout(basicParamsPage);
        basicParamsLayout->setSpacing(14);
        basicParamsLayout->setObjectName(QString::fromUtf8("basicParamsLayout"));
        basicParamsLayout->setContentsMargins(0, 8, 0, 0);
        templateCard = new QFrame(basicParamsPage);
        templateCard->setObjectName(QString::fromUtf8("templateCard"));
        templateLayout = new QVBoxLayout(templateCard);
        templateLayout->setSpacing(14);
        templateLayout->setObjectName(QString::fromUtf8("templateLayout"));
        templateLayout->setContentsMargins(20, 18, 20, 18);
        templateHeaderLayout = new QHBoxLayout();
        templateHeaderLayout->setObjectName(QString::fromUtf8("templateHeaderLayout"));
        templateTitleLabel = new QLabel(templateCard);
        templateTitleLabel->setObjectName(QString::fromUtf8("templateTitleLabel"));

        templateHeaderLayout->addWidget(templateTitleLabel);

        templateHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        templateHeaderLayout->addItem(templateHeaderSpacer);

        templateCollapseButton = new QToolButton(templateCard);
        templateCollapseButton->setObjectName(QString::fromUtf8("templateCollapseButton"));

        templateHeaderLayout->addWidget(templateCollapseButton);


        templateLayout->addLayout(templateHeaderLayout);

        templateListWidget = new QListWidget(templateCard);
        templateListWidget->setObjectName(QString::fromUtf8("templateListWidget"));
        templateListWidget->setMinimumHeight(232);

        templateLayout->addWidget(templateListWidget);

        activeTemplateSummaryLabel = new QLabel(templateCard);
        activeTemplateSummaryLabel->setObjectName(QString::fromUtf8("activeTemplateSummaryLabel"));
        activeTemplateSummaryLabel->setWordWrap(true);
        activeTemplateSummaryLabel->setMinimumHeight(28);
        activeTemplateSummaryLabel->setStyleSheet(QString::fromUtf8("background:#ffffff; color:#111827; border:1px solid #cfd6df; padding:4px;"));

        templateLayout->addWidget(activeTemplateSummaryLabel);

        templateButtonLayout = new QHBoxLayout();
        templateButtonLayout->setObjectName(QString::fromUtf8("templateButtonLayout"));
        addTemplateButton = new QPushButton(templateCard);
        addTemplateButton->setObjectName(QString::fromUtf8("addTemplateButton"));

        templateButtonLayout->addWidget(addTemplateButton);

        editTemplateButton = new QPushButton(templateCard);
        editTemplateButton->setObjectName(QString::fromUtf8("editTemplateButton"));

        templateButtonLayout->addWidget(editTemplateButton);


        templateLayout->addLayout(templateButtonLayout);

        templateManageButtonLayout = new QHBoxLayout();
        templateManageButtonLayout->setObjectName(QString::fromUtf8("templateManageButtonLayout"));
        renameTemplateButton = new QPushButton(templateCard);
        renameTemplateButton->setObjectName(QString::fromUtf8("renameTemplateButton"));

        templateManageButtonLayout->addWidget(renameTemplateButton);

        deleteTemplateButton = new QPushButton(templateCard);
        deleteTemplateButton->setObjectName(QString::fromUtf8("deleteTemplateButton"));

        templateManageButtonLayout->addWidget(deleteTemplateButton);

        importTemplateButton = new QPushButton(templateCard);
        importTemplateButton->setObjectName(QString::fromUtf8("importTemplateButton"));

        templateManageButtonLayout->addWidget(importTemplateButton);

        exportTemplateButton = new QPushButton(templateCard);
        exportTemplateButton->setObjectName(QString::fromUtf8("exportTemplateButton"));

        templateManageButtonLayout->addWidget(exportTemplateButton);


        templateLayout->addLayout(templateManageButtonLayout);


        basicParamsLayout->addWidget(templateCard);

        regionCard = new QFrame(basicParamsPage);
        regionCard->setObjectName(QString::fromUtf8("regionCard"));
        regionLayout = new QVBoxLayout(regionCard);
        regionLayout->setSpacing(14);
        regionLayout->setObjectName(QString::fromUtf8("regionLayout"));
        regionLayout->setContentsMargins(20, 18, 20, 18);
        regionHeaderLayout = new QHBoxLayout();
        regionHeaderLayout->setObjectName(QString::fromUtf8("regionHeaderLayout"));
        regionTitleLabel = new QLabel(regionCard);
        regionTitleLabel->setObjectName(QString::fromUtf8("regionTitleLabel"));

        regionHeaderLayout->addWidget(regionTitleLabel);

        regionHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        regionHeaderLayout->addItem(regionHeaderSpacer);

        regionCollapseButton = new QToolButton(regionCard);
        regionCollapseButton->setObjectName(QString::fromUtf8("regionCollapseButton"));

        regionHeaderLayout->addWidget(regionCollapseButton);


        regionLayout->addLayout(regionHeaderLayout);

        regionButtonLayout = new QHBoxLayout();
        regionButtonLayout->setObjectName(QString::fromUtf8("regionButtonLayout"));
        regionLabel = new QLabel(regionCard);
        regionLabel->setObjectName(QString::fromUtf8("regionLabel"));
        regionLabel->setMinimumWidth(118);

        regionButtonLayout->addWidget(regionLabel);

        regionButtonSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        regionButtonLayout->addItem(regionButtonSpacer);

        regionDrawButton = new QToolButton(regionCard);
        regionDrawButton->setObjectName(QString::fromUtf8("regionDrawButton"));
        regionDrawButton->setMinimumSize(QSize(74, 36));
        regionDrawButton->setCheckable(true);

        regionButtonLayout->addWidget(regionDrawButton);

        regionRectButton = new QToolButton(regionCard);
        regionRectButton->setObjectName(QString::fromUtf8("regionRectButton"));
        regionRectButton->setMinimumSize(QSize(74, 36));
        regionRectButton->setCheckable(true);
        regionRectButton->setChecked(true);

        regionButtonLayout->addWidget(regionRectButton);

        regionCircleButton = new QToolButton(regionCard);
        regionCircleButton->setObjectName(QString::fromUtf8("regionCircleButton"));
        regionCircleButton->setMinimumSize(QSize(74, 36));
        regionCircleButton->setCheckable(true);

        regionButtonLayout->addWidget(regionCircleButton);


        regionLayout->addLayout(regionButtonLayout);

        positionCorrectionEnableLayout = new QHBoxLayout();
        positionCorrectionEnableLayout->setObjectName(QString::fromUtf8("positionCorrectionEnableLayout"));
        positionCorrectionEnableLabel = new QLabel(regionCard);
        positionCorrectionEnableLabel->setObjectName(QString::fromUtf8("positionCorrectionEnableLabel"));

        positionCorrectionEnableLayout->addWidget(positionCorrectionEnableLabel);

        positionCorrectionEnableSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        positionCorrectionEnableLayout->addItem(positionCorrectionEnableSpacer);

        positionCorrectionSwitch = new QCheckBox(regionCard);
        positionCorrectionSwitch->setObjectName(QString::fromUtf8("positionCorrectionSwitch"));

        positionCorrectionEnableLayout->addWidget(positionCorrectionSwitch);


        regionLayout->addLayout(positionCorrectionEnableLayout);

        positionCorrectionSourceRow = new QWidget(regionCard);
        positionCorrectionSourceRow->setObjectName(QString::fromUtf8("positionCorrectionSourceRow"));
        positionCorrectionSourceLayout = new QHBoxLayout(positionCorrectionSourceRow);
        positionCorrectionSourceLayout->setObjectName(QString::fromUtf8("positionCorrectionSourceLayout"));
        positionCorrectionSourceLayout->setContentsMargins(0, 0, 0, 0);
        positionCorrectionSourceLabel = new QLabel(positionCorrectionSourceRow);
        positionCorrectionSourceLabel->setObjectName(QString::fromUtf8("positionCorrectionSourceLabel"));
        positionCorrectionSourceLabel->setMinimumWidth(118);

        positionCorrectionSourceLayout->addWidget(positionCorrectionSourceLabel);

        positionCorrectionSourceComboBox = new QComboBox(positionCorrectionSourceRow);
        positionCorrectionSourceComboBox->addItem(QString());
        positionCorrectionSourceComboBox->setObjectName(QString::fromUtf8("positionCorrectionSourceComboBox"));

        positionCorrectionSourceLayout->addWidget(positionCorrectionSourceComboBox);


        regionLayout->addWidget(positionCorrectionSourceRow);


        basicParamsLayout->addWidget(regionCard);

        judgeCard = new QFrame(basicParamsPage);
        judgeCard->setObjectName(QString::fromUtf8("judgeCard"));
        judgeLayout = new QVBoxLayout(judgeCard);
        judgeLayout->setSpacing(14);
        judgeLayout->setObjectName(QString::fromUtf8("judgeLayout"));
        judgeLayout->setContentsMargins(20, 18, 20, 18);
        judgeHeaderLayout = new QHBoxLayout();
        judgeHeaderLayout->setObjectName(QString::fromUtf8("judgeHeaderLayout"));
        judgeTitleLabel = new QLabel(judgeCard);
        judgeTitleLabel->setObjectName(QString::fromUtf8("judgeTitleLabel"));

        judgeHeaderLayout->addWidget(judgeTitleLabel);

        judgeHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        judgeHeaderLayout->addItem(judgeHeaderSpacer);

        judgeCollapseButton = new QToolButton(judgeCard);
        judgeCollapseButton->setObjectName(QString::fromUtf8("judgeCollapseButton"));

        judgeHeaderLayout->addWidget(judgeCollapseButton);


        judgeLayout->addLayout(judgeHeaderLayout);

        resultBasisLayout = new QHBoxLayout();
        resultBasisLayout->setObjectName(QString::fromUtf8("resultBasisLayout"));
        resultBasisLabel = new QLabel(judgeCard);
        resultBasisLabel->setObjectName(QString::fromUtf8("resultBasisLabel"));
        resultBasisLabel->setMinimumWidth(118);

        resultBasisLayout->addWidget(resultBasisLabel);

        resultBasisComboBox = new QComboBox(judgeCard);
        resultBasisComboBox->addItem(QString());
        resultBasisComboBox->addItem(QString());
        resultBasisComboBox->setObjectName(QString::fromUtf8("resultBasisComboBox"));

        resultBasisLayout->addWidget(resultBasisComboBox);


        judgeLayout->addLayout(resultBasisLayout);

        colorDecisionModeLayout = new QHBoxLayout();
        colorDecisionModeLayout->setObjectName(QString::fromUtf8("colorDecisionModeLayout"));
        colorDecisionModeLabel = new QLabel(judgeCard);
        colorDecisionModeLabel->setObjectName(QString::fromUtf8("colorDecisionModeLabel"));
        colorDecisionModeLabel->setMinimumWidth(118);

        colorDecisionModeLayout->addWidget(colorDecisionModeLabel);

        colorDecisionModeComboBox = new QComboBox(judgeCard);
        colorDecisionModeComboBox->addItem(QString());
        colorDecisionModeComboBox->addItem(QString());
        colorDecisionModeComboBox->setObjectName(QString::fromUtf8("colorDecisionModeComboBox"));

        colorDecisionModeLayout->addWidget(colorDecisionModeComboBox);


        judgeLayout->addLayout(colorDecisionModeLayout);

        minScoreLayout = new QHBoxLayout();
        minScoreLayout->setObjectName(QString::fromUtf8("minScoreLayout"));
        minScoreLabel = new QLabel(judgeCard);
        minScoreLabel->setObjectName(QString::fromUtf8("minScoreLabel"));
        minScoreLabel->setMinimumWidth(118);

        minScoreLayout->addWidget(minScoreLabel);

        minScoreSpinBox = new QSpinBox(judgeCard);
        minScoreSpinBox->setObjectName(QString::fromUtf8("minScoreSpinBox"));
        minScoreSpinBox->setMaximum(100);
        minScoreSpinBox->setValue(80);

        minScoreLayout->addWidget(minScoreSpinBox);


        judgeLayout->addLayout(minScoreLayout);

        expectedLabelLayout = new QHBoxLayout();
        expectedLabelLayout->setObjectName(QString::fromUtf8("expectedLabelLayout"));
        expectedLabelTitleLabel = new QLabel(judgeCard);
        expectedLabelTitleLabel->setObjectName(QString::fromUtf8("expectedLabelTitleLabel"));
        expectedLabelTitleLabel->setMinimumWidth(118);

        expectedLabelLayout->addWidget(expectedLabelTitleLabel);

        expectedLabelComboBox = new QComboBox(judgeCard);
        expectedLabelComboBox->setObjectName(QString::fromUtf8("expectedLabelComboBox"));

        expectedLabelLayout->addWidget(expectedLabelComboBox);


        judgeLayout->addLayout(expectedLabelLayout);


        basicParamsLayout->addWidget(judgeCard);

        basicParamsSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        basicParamsLayout->addItem(basicParamsSpacer);

        colorParamsStackedWidget->addWidget(basicParamsPage);
        allParamsPage = new QWidget();
        allParamsPage->setObjectName(QString::fromUtf8("allParamsPage"));
        allParamsLayout = new QVBoxLayout(allParamsPage);
        allParamsLayout->setSpacing(14);
        allParamsLayout->setObjectName(QString::fromUtf8("allParamsLayout"));
        allParamsLayout->setContentsMargins(0, 8, 0, 0);
        allTemplateCard = new QFrame(allParamsPage);
        allTemplateCard->setObjectName(QString::fromUtf8("allTemplateCard"));
        allTemplateLayout = new QVBoxLayout(allTemplateCard);
        allTemplateLayout->setSpacing(14);
        allTemplateLayout->setObjectName(QString::fromUtf8("allTemplateLayout"));
        allTemplateLayout->setContentsMargins(20, 18, 20, 18);
        allTemplateTitleLabel = new QLabel(allTemplateCard);
        allTemplateTitleLabel->setObjectName(QString::fromUtf8("allTemplateTitleLabel"));

        allTemplateLayout->addWidget(allTemplateTitleLabel);

        allTemplateSummaryLabel = new QLabel(allTemplateCard);
        allTemplateSummaryLabel->setObjectName(QString::fromUtf8("allTemplateSummaryLabel"));
        allTemplateSummaryLabel->setWordWrap(true);

        allTemplateLayout->addWidget(allTemplateSummaryLabel);


        allParamsLayout->addWidget(allTemplateCard);

        allParamsSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        allParamsLayout->addItem(allParamsSpacer);

        colorParamsStackedWidget->addWidget(allParamsPage);

        leftLayout->addWidget(colorParamsStackedWidget);

        bottomButtonLayout = new QHBoxLayout();
        bottomButtonLayout->setObjectName(QString::fromUtf8("bottomButtonLayout"));
        bottomButtonSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        bottomButtonLayout->addItem(bottomButtonSpacer);

        testRunButton = new QPushButton(leftPanel);
        testRunButton->setObjectName(QString::fromUtf8("testRunButton"));

        bottomButtonLayout->addWidget(testRunButton);

        finishButton = new QPushButton(leftPanel);
        finishButton->setObjectName(QString::fromUtf8("finishButton"));

        bottomButtonLayout->addWidget(finishButton);


        leftLayout->addLayout(bottomButtonLayout);


        contentLayout->addWidget(leftPanel);

        previewPanel = new QFrame(ColorRecognitionDialog);
        previewPanel->setObjectName(QString::fromUtf8("previewPanel"));
        previewLayout = new QVBoxLayout(previewPanel);
        previewLayout->setSpacing(8);
        previewLayout->setObjectName(QString::fromUtf8("previewLayout"));
        previewLayout->setContentsMargins(14, 12, 14, 8);
        viewerHeaderLayout = new QHBoxLayout();
        viewerHeaderLayout->setObjectName(QString::fromUtf8("viewerHeaderLayout"));
        viewerTitleLabel = new QLabel(previewPanel);
        viewerTitleLabel->setObjectName(QString::fromUtf8("viewerTitleLabel"));

        viewerHeaderLayout->addWidget(viewerTitleLabel);

        viewerHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        viewerHeaderLayout->addItem(viewerHeaderSpacer);

        viewerGridButton = new QToolButton(previewPanel);
        viewerGridButton->setObjectName(QString::fromUtf8("viewerGridButton"));

        viewerHeaderLayout->addWidget(viewerGridButton);

        viewerZoomSearchButton = new QToolButton(previewPanel);
        viewerZoomSearchButton->setObjectName(QString::fromUtf8("viewerZoomSearchButton"));

        viewerHeaderLayout->addWidget(viewerZoomSearchButton);

        viewerZoomOutButton = new QToolButton(previewPanel);
        viewerZoomOutButton->setObjectName(QString::fromUtf8("viewerZoomOutButton"));

        viewerHeaderLayout->addWidget(viewerZoomOutButton);

        viewerZoomLabel = new QLabel(previewPanel);
        viewerZoomLabel->setObjectName(QString::fromUtf8("viewerZoomLabel"));

        viewerHeaderLayout->addWidget(viewerZoomLabel);

        viewerZoomInButton = new QToolButton(previewPanel);
        viewerZoomInButton->setObjectName(QString::fromUtf8("viewerZoomInButton"));

        viewerHeaderLayout->addWidget(viewerZoomInButton);

        viewerFullButton = new QToolButton(previewPanel);
        viewerFullButton->setObjectName(QString::fromUtf8("viewerFullButton"));

        viewerHeaderLayout->addWidget(viewerFullButton);


        previewLayout->addLayout(viewerHeaderLayout);

        previewGraphicsView = new QGraphicsView(previewPanel);
        previewGraphicsView->setObjectName(QString::fromUtf8("previewGraphicsView"));

        previewLayout->addWidget(previewGraphicsView);

        viewerStatusLabel = new QLabel(previewPanel);
        viewerStatusLabel->setObjectName(QString::fromUtf8("viewerStatusLabel"));
        viewerStatusLabel->setMinimumSize(QSize(0, 28));

        previewLayout->addWidget(viewerStatusLabel);


        contentLayout->addWidget(previewPanel);


        rootLayout->addLayout(contentLayout);


        retranslateUi(ColorRecognitionDialog);

        QMetaObject::connectSlotsByName(ColorRecognitionDialog);
    } // setupUi

    void retranslateUi(QDialog *ColorRecognitionDialog)
    {
        ColorRecognitionDialog->setWindowTitle(QCoreApplication::translate("ColorRecognitionDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \351\242\234\350\211\262\350\257\206\345\210\253", nullptr));
        headerTitleLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\303\227", nullptr));
        dialogTitleLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\351\242\234\350\211\262\350\257\206\345\210\253", nullptr));
        dialogTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "cardTitle", nullptr)));
        basicSegmentButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\345\237\272\347\241\200", nullptr));
        allSegmentButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\345\205\250\351\203\250", nullptr));
        templateCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "configCard", nullptr)));
        templateTitleLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\346\250\241\346\235\277\350\256\255\347\273\203", nullptr));
        templateTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "cardTitle", nullptr)));
        templateCollapseButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\342\214\204", nullptr));
        templateCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "collapseCard", nullptr)));
        activeTemplateSummaryLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\346\234\252\346\267\273\345\212\240\346\250\241\346\235\277", nullptr));
        addTemplateButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "+ \346\267\273\345\212\240\346\250\241\346\235\277", nullptr));
        addTemplateButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "secondary", nullptr)));
        editTemplateButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\347\274\226\350\276\221", nullptr));
        editTemplateButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "plain", nullptr)));
        renameTemplateButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\351\207\215\345\221\275\345\220\215", nullptr));
        renameTemplateButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "plain", nullptr)));
        deleteTemplateButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\345\210\240\351\231\244", nullptr));
        deleteTemplateButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "plain", nullptr)));
        importTemplateButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\345\257\274\345\205\245\346\250\241\346\235\277", nullptr));
        importTemplateButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "plain", nullptr)));
        exportTemplateButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\345\257\274\345\207\272\346\250\241\346\235\277", nullptr));
        exportTemplateButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "plain", nullptr)));
        regionCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "configCard", nullptr)));
        regionTitleLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\346\243\200\346\265\213\345\214\272\345\237\237", nullptr));
        regionTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "cardTitle", nullptr)));
        regionCollapseButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\342\214\204", nullptr));
        regionCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "collapseCard", nullptr)));
        regionLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\346\243\200\346\265\213\345\214\272", nullptr));
        regionLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "rowField", nullptr)));
        regionDrawButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\342\234\216", nullptr));
        regionDrawButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "toolbarIcon", nullptr)));
        regionRectButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\342\226\241", nullptr));
        regionRectButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "toolbarIcon", nullptr)));
        regionCircleButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\342\227\213", nullptr));
        regionCircleButton->setProperty("actionRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "toolbarIcon", nullptr)));
        positionCorrectionEnableLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\347\213\254\347\253\213\344\275\215\347\275\256\344\277\256\346\255\243\344\275\277\350\203\275 \342\223\230", nullptr));
        positionCorrectionEnableLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "rowField", nullptr)));
        positionCorrectionSwitch->setText(QString());
        positionCorrectionSourceLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        positionCorrectionSourceLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "rowField", nullptr)));
        positionCorrectionSourceComboBox->setItemText(0, QCoreApplication::translate("ColorRecognitionDialog", "1 \345\237\272\345\207\206\345\233\276.\344\275\215\347\275\256\344\277\256\346\255\243\344\277\241\346\201\257", nullptr));

        judgeCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "configCard", nullptr)));
        judgeTitleLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\347\273\223\346\236\234\345\210\244\346\226\255", nullptr));
        judgeTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "cardTitle", nullptr)));
        judgeCollapseButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\342\214\204", nullptr));
        judgeCollapseButton->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "collapseCard", nullptr)));
        resultBasisLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\345\210\244\346\226\255\344\276\235\346\215\256", nullptr));
        resultBasisLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "rowField", nullptr)));
        resultBasisComboBox->setItemText(0, QCoreApplication::translate("ColorRecognitionDialog", "\346\234\200\344\275\216\345\210\206\346\225\260", nullptr));
        resultBasisComboBox->setItemText(1, QCoreApplication::translate("ColorRecognitionDialog", "\347\261\273\345\210\253\345\210\244\346\226\255", nullptr));

        colorDecisionModeLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\345\210\244\345\210\253\346\226\271\345\274\217", nullptr));
        colorDecisionModeLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "rowField", nullptr)));
        colorDecisionModeComboBox->setItemText(0, QCoreApplication::translate("ColorRecognitionDialog", "\344\270\273\351\242\234\350\211\262\345\215\240\346\257\224", nullptr));
        colorDecisionModeComboBox->setItemText(1, QCoreApplication::translate("ColorRecognitionDialog", "\346\225\264\344\275\223\347\233\270\344\274\274\345\272\246", nullptr));

        minScoreLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\346\234\200\344\275\216\345\276\227\345\210\206", nullptr));
        minScoreLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "rowField", nullptr)));
        expectedLabelTitleLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\347\233\256\346\240\207\347\261\273\345\210\253", nullptr));
        expectedLabelTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "rowField", nullptr)));
        allTemplateCard->setProperty("panelRole", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "configCard", nullptr)));
        allTemplateTitleLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\345\275\223\345\211\215\346\250\241\346\235\277", nullptr));
        allTemplateTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("ColorRecognitionDialog", "cardTitle", nullptr)));
        allTemplateSummaryLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\346\234\252\346\267\273\345\212\240\346\250\241\346\235\277", nullptr));
        testRunButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\346\265\213\350\257\225\350\277\220\350\241\214", nullptr));
        finishButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\345\256\214\346\210\220", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerGridButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\342\226\246", nullptr));
        viewerZoomSearchButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\342\214\225", nullptr));
        viewerZoomOutButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "100%", nullptr));
        viewerZoomInButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "+", nullptr));
        viewerFullButton->setText(QCoreApplication::translate("ColorRecognitionDialog", "\342\226\241", nullptr));
        viewerStatusLabel->setText(QCoreApplication::translate("ColorRecognitionDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: 0ms  \345\267\245\345\205\267\350\200\227\346\227\266: 0ms", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ColorRecognitionDialog: public Ui_ColorRecognitionDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_COLORRECOGNITIONDIALOG_H
