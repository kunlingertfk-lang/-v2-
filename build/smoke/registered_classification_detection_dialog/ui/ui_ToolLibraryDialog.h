/********************************************************************************
** Form generated from reading UI file 'ToolLibraryDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TOOLLIBRARYDIALOG_H
#define UI_TOOLLIBRARYDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ToolLibraryDialog
{
public:
    QVBoxLayout *verticalLayout_root;
    QFrame *toolLibraryTitleBar;
    QHBoxLayout *horizontalLayout_titleBar;
    QLabel *toolLibraryTitleLabel;
    QSpacerItem *horizontalSpacer_titleBar;
    QToolButton *closeButton;
    QFrame *toolLibraryBody;
    QHBoxLayout *horizontalLayout_body;
    QFrame *toolLibrarySidebar;
    QVBoxLayout *verticalLayout_sidebar;
    QLineEdit *toolSearchEdit;
    QLabel *generalToolsLabel;
    QPushButton *allToolsButton;
    QPushButton *measureToolsButton;
    QPushButton *countToolsButton;
    QPushButton *recognitionToolsButton;
    QPushButton *presenceToolsButton;
    QPushButton *logicToolsButton;
    QPushButton *locationToolsButton;
    QPushButton *deepLearningToolsButton;
    QPushButton *defectToolsButton;
    QSpacerItem *verticalSpacer_sidebar;
    QFrame *toolGridPanel;
    QVBoxLayout *verticalLayout_toolGridPanel;
    QScrollArea *toolCategoryScrollArea;
    QWidget *toolCategoryContent;
    QVBoxLayout *verticalLayout_toolCategoryContent;
    QFrame *measurementCategoryFrame;
    QGridLayout *gridLayout_measurementCategoryFrame;
    QLabel *measurementCategoryFrameTitleLabel;
    QToolButton *pointMeasureButton;
    QToolButton *lineMeasureButton;
    QToolButton *contrastMeasureButton;
    QToolButton *grayAreaToolButton;
    QToolButton *gapMeasureButton;
    QToolButton *widthMeasureButton;
    QToolButton *brightnessAverageButton;
    QToolButton *lineAngleButton;
    QToolButton *colorMeasureButton;
    QToolButton *colorAreaToolButton;
    QToolButton *diameterMeasureButton;
    QToolButton *straightAngleButton;
    QFrame *countCategoryFrame;
    QGridLayout *gridLayout_countCategoryFrame;
    QLabel *countCategoryFrameTitleLabel;
    QToolButton *counterToolButton;
    QToolButton *areaCounterButton;
    QToolButton *edgeCounterButton;
    QFrame *recognitionCategoryFrame;
    QGridLayout *gridLayout_recognitionCategoryFrame;
    QLabel *recognitionCategoryFrameTitleLabel;
    QToolButton *ocrToolButton;
    QToolButton *codeToolButton;
    QToolButton *categoryToolButton;
    QToolButton *colorRecognitionToolButton;
    QToolButton *colorComparisonToolButton;
    QToolButton *registrationClassToolButton;
    QToolButton *registrationClassDetectionToolButton;
    QFrame *presenceCategoryFrame;
    QGridLayout *gridLayout_presenceCategoryFrame;
    QToolButton *presenceToolButton;
    QToolButton *circlePresenceButton;
    QToolButton *blobPresenceButton;
    QToolButton *edgePresenceButton;
    QToolButton *linePresenceButton;
    QToolButton *contourPresenceButton;
    QLabel *presenceCategoryFrameTitleLabel;
    QFrame *logicCategoryFrame;
    QGridLayout *gridLayout_logicCategoryFrame;
    QLabel *logicCategoryFrameTitleLabel;
    QToolButton *judgeToolButton;
    QToolButton *conditionToolButton;
    QToolButton *variableToolButton;
    QToolButton *outputLogicButton;
    QFrame *locationCategoryFrame;
    QGridLayout *gridLayout_locationCategoryFrame;
    QLabel *locationCategoryFrameTitleLabel;
    QToolButton *templateLocationButton;
    QToolButton *edgeLocationButton;
    QToolButton *circleLocationButton;
    QFrame *deepLearningCategoryFrame;
    QGridLayout *gridLayout_deepLearningCategoryFrame;
    QLabel *deepLearningCategoryFrameTitleLabel;
    QToolButton *dlDetectButton;
    QToolButton *FaultDetectButton;
    QToolButton *ClassifyButton;
    QFrame *defectCategoryFrame;
    QGridLayout *gridLayout_defectCategoryFrame;
    QLabel *defectCategoryFrameTitleLabel;
    QToolButton *scratchDefectButton;
    QToolButton *stainDefectButton;
    QToolButton *missingDefectButton;
    QSpacerItem *verticalSpacer_toolCategories;
    QFrame *toolPreviewPanel;
    QVBoxLayout *verticalLayout_preview;
    QDialogButtonBox *buttonBox;
    QFrame *toolPreviewGraphic;
    QVBoxLayout *verticalLayout_previewGraphic;
    QLabel *previewIconLabel;
    QLabel *previewTitleLabel;
    QLabel *previewDescriptionLabel;
    QSpacerItem *verticalSpacer_preview;
    QFrame *toolLibraryBottomBar;
    QHBoxLayout *horizontalLayout_bottom;
    QLabel *toolLibraryTipLabel;
    QSpacerItem *horizontalSpacer_bottom;
    QPushButton *confirmButton;
    QPushButton *cancelButton;

    void setupUi(QDialog *ToolLibraryDialog)
    {
        if (ToolLibraryDialog->objectName().isEmpty())
            ToolLibraryDialog->setObjectName(QString::fromUtf8("ToolLibraryDialog"));
        ToolLibraryDialog->resize(1180, 790);
        ToolLibraryDialog->setStyleSheet(QString::fromUtf8("QDialog {\n"
"      background: #eef1f4;\n"
"      color: #3f4652;\n"
"      font-family: \"Microsoft YaHei\", \"Noto Sans CJK SC\", sans-serif;\n"
"      font-size: 16px;\n"
"  }\n"
"  QFrame#toolLibraryTitleBar {\n"
"      background: #3f444e;\n"
"      border-top: 2px solid #ff7a00;\n"
"  }\n"
"  QLabel#toolLibraryTitleLabel {\n"
"      color: #ffffff;\n"
"      font-size: 20px;\n"
"      font-weight: 500;\n"
"  }\n"
"  QToolButton#closeButton, QToolButton#headerMaximizeButton {\n"
"      color: #ffffff;\n"
"      font-size: 28px;\n"
"      border: 0;\n"
"      background: transparent;\n"
"  }\n"
"  QFrame#toolLibrarySidebar {\n"
"      background: #ffffff;\n"
"      border-right: 1px solid #e2e5e9;\n"
"  }\n"
"  QLineEdit#toolSearchEdit {\n"
"      min-height: 38px;\n"
"      border: 1px solid #c9cdd3;\n"
"      color: #6b747f;\n"
"      padding-left: 12px;\n"
"      font-size: 16px;\n"
"  }\n"
"  QPushButton[navButton=\"true\"] {\n"
"      border: 0;\n"
"      border-radius: 0;\n"
"      background: #ffff"
                        "ff;\n"
"      color: #3f4652;\n"
"      text-align: left;\n"
"      padding-left: 42px;\n"
"      min-height: 48px;\n"
"      font-size: 17px;\n"
"  }\n"
"  QPushButton[navButton=\"true\"]:checked {\n"
"      color: #ff7a00;\n"
"      background: #eef1f4;\n"
"      border-right: 2px solid #ff7a00;\n"
"  }\n"
"  QLabel[sidebarGroup=\"true\"] {\n"
"      color: #3f4652;\n"
"      font-size: 17px;\n"
"      font-weight: 500;\n"
"      padding-left: 18px;\n"
"  }\n"
"  QFrame#toolGridPanel {\n"
"      background: #ffffff;\n"
"      border: 0;\n"
"  }\n"
"  QFrame#toolPreviewPanel {\n"
"      background: #ffffff;\n"
"      border-left: 1px solid #e0e4e8;\n"
"  }\n"
"  QFrame[toolCategory=\"true\"] {\n"
"      background: #ffffff;\n"
"      border: 0;\n"
"  }\n"
"  QLabel[categoryTitle=\"true\"] {\n"
"      color: #3f4652;\n"
"      font-size: 17px;\n"
"      font-weight: 600;\n"
"      padding: 8px 0 0 0;\n"
"  }\n"
"  QToolButton[toolCard=\"true\"] {\n"
"      background: #ffffff;\n"
"      border: 1px solid trans"
                        "parent;\n"
"      border-radius: 2px;\n"
"      color: #4a5360;\n"
"      font-size: 16px;\n"
"      padding: 8px;\n"
"  }\n"
"  QToolButton[toolCard=\"true\"]:hover, QToolButton[toolCard=\"true\"]:checked {\n"
"      border: 1px solid #ff7a00;\n"
"      color: #ff7a00;\n"
"      background: #fff7f0;\n"
"  }\n"
"  QFrame#toolPreviewGraphic {\n"
"      background: #ffffff;\n"
"      border: 0;\n"
"  }\n"
"  QLabel#previewIconLabel {\n"
"      color: #d6dde6;\n"
"      font-size: 86px;\n"
"      qproperty-alignment: AlignCenter;\n"
"  }\n"
"  QLabel#previewTitleLabel {\n"
"      color: #3f4652;\n"
"      font-size: 18px;\n"
"      font-weight: 600;\n"
"      qproperty-alignment: AlignCenter;\n"
"  }\n"
"  QLabel#previewDescriptionLabel {\n"
"      color: #9aa3ae;\n"
"      font-size: 15px;\n"
"      qproperty-alignment: AlignCenter;\n"
"  }\n"
"  QFrame#toolLibraryBottomBar {\n"
"      background: #e3e7eb;\n"
"      border-top: 1px solid #d0d5db;\n"
"  }\n"
"  QPushButton#confirmButton {\n"
"      background: #f"
                        "f7a00;\n"
"      color: #ffffff;\n"
"      border: 1px solid #ef7000;\n"
"      min-width: 104px;\n"
"      min-height: 42px;\n"
"      font-size: 16px;\n"
"  }\n"
"  QPushButton#cancelButton {\n"
"      background: #ffffff;\n"
"      color: #3f4652;\n"
"      border: 1px solid #c9cdd3;\n"
"      min-width: 104px;\n"
"      min-height: 42px;\n"
"      font-size: 16px;\n"
"  }\n"
"  QLabel#toolLibraryTipLabel {\n"
"      color: #ff3b30;\n"
"      font-size: 15px;\n"
"  }\n"
"  QScrollArea { background: transparent; border: 0; }\n"
"  QScrollArea > QWidget > QWidget { background: #ffffff; }\n"
"  "));
        verticalLayout_root = new QVBoxLayout(ToolLibraryDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        toolLibraryTitleBar = new QFrame(ToolLibraryDialog);
        toolLibraryTitleBar->setObjectName(QString::fromUtf8("toolLibraryTitleBar"));
        toolLibraryTitleBar->setMinimumSize(QSize(0, 50));
        toolLibraryTitleBar->setMaximumSize(QSize(16777215, 50));
        toolLibraryTitleBar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_titleBar = new QHBoxLayout(toolLibraryTitleBar);
        horizontalLayout_titleBar->setSpacing(8);
        horizontalLayout_titleBar->setObjectName(QString::fromUtf8("horizontalLayout_titleBar"));
        horizontalLayout_titleBar->setContentsMargins(20, 0, 12, 0);
        toolLibraryTitleLabel = new QLabel(toolLibraryTitleBar);
        toolLibraryTitleLabel->setObjectName(QString::fromUtf8("toolLibraryTitleLabel"));

        horizontalLayout_titleBar->addWidget(toolLibraryTitleLabel);

        horizontalSpacer_titleBar = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_titleBar->addItem(horizontalSpacer_titleBar);

        closeButton = new QToolButton(toolLibraryTitleBar);
        closeButton->setObjectName(QString::fromUtf8("closeButton"));
        closeButton->setMinimumSize(QSize(38, 38));
        closeButton->setAutoRaise(true);

        horizontalLayout_titleBar->addWidget(closeButton);


        verticalLayout_root->addWidget(toolLibraryTitleBar);

        toolLibraryBody = new QFrame(ToolLibraryDialog);
        toolLibraryBody->setObjectName(QString::fromUtf8("toolLibraryBody"));
        toolLibraryBody->setFrameShape(QFrame::NoFrame);
        horizontalLayout_body = new QHBoxLayout(toolLibraryBody);
        horizontalLayout_body->setSpacing(0);
        horizontalLayout_body->setObjectName(QString::fromUtf8("horizontalLayout_body"));
        horizontalLayout_body->setContentsMargins(0, 0, 0, 0);
        toolLibrarySidebar = new QFrame(toolLibraryBody);
        toolLibrarySidebar->setObjectName(QString::fromUtf8("toolLibrarySidebar"));
        toolLibrarySidebar->setMinimumSize(QSize(200, 0));
        toolLibrarySidebar->setMaximumSize(QSize(200, 16777215));
        toolLibrarySidebar->setFrameShape(QFrame::NoFrame);
        verticalLayout_sidebar = new QVBoxLayout(toolLibrarySidebar);
        verticalLayout_sidebar->setSpacing(8);
        verticalLayout_sidebar->setObjectName(QString::fromUtf8("verticalLayout_sidebar"));
        verticalLayout_sidebar->setContentsMargins(14, 16, 14, 0);
        toolSearchEdit = new QLineEdit(toolLibrarySidebar);
        toolSearchEdit->setObjectName(QString::fromUtf8("toolSearchEdit"));
        toolSearchEdit->setMinimumSize(QSize(170, 40));

        verticalLayout_sidebar->addWidget(toolSearchEdit);

        generalToolsLabel = new QLabel(toolLibrarySidebar);
        generalToolsLabel->setObjectName(QString::fromUtf8("generalToolsLabel"));
        generalToolsLabel->setProperty("sidebarGroup", QVariant(true));

        verticalLayout_sidebar->addWidget(generalToolsLabel);

        allToolsButton = new QPushButton(toolLibrarySidebar);
        allToolsButton->setObjectName(QString::fromUtf8("allToolsButton"));
        allToolsButton->setMinimumSize(QSize(0, 48));
        allToolsButton->setCheckable(true);
        allToolsButton->setProperty("navButton", QVariant(true));

        verticalLayout_sidebar->addWidget(allToolsButton);

        measureToolsButton = new QPushButton(toolLibrarySidebar);
        measureToolsButton->setObjectName(QString::fromUtf8("measureToolsButton"));
        measureToolsButton->setMinimumSize(QSize(0, 48));
        measureToolsButton->setCheckable(true);
        measureToolsButton->setProperty("navButton", QVariant(true));

        verticalLayout_sidebar->addWidget(measureToolsButton);

        countToolsButton = new QPushButton(toolLibrarySidebar);
        countToolsButton->setObjectName(QString::fromUtf8("countToolsButton"));
        countToolsButton->setMinimumSize(QSize(0, 48));
        countToolsButton->setCheckable(true);
        countToolsButton->setProperty("navButton", QVariant(true));

        verticalLayout_sidebar->addWidget(countToolsButton);

        recognitionToolsButton = new QPushButton(toolLibrarySidebar);
        recognitionToolsButton->setObjectName(QString::fromUtf8("recognitionToolsButton"));
        recognitionToolsButton->setMinimumSize(QSize(0, 48));
        recognitionToolsButton->setCheckable(true);
        recognitionToolsButton->setProperty("navButton", QVariant(true));

        verticalLayout_sidebar->addWidget(recognitionToolsButton);

        presenceToolsButton = new QPushButton(toolLibrarySidebar);
        presenceToolsButton->setObjectName(QString::fromUtf8("presenceToolsButton"));
        presenceToolsButton->setMinimumSize(QSize(0, 48));
        presenceToolsButton->setCheckable(true);
        presenceToolsButton->setProperty("navButton", QVariant(true));

        verticalLayout_sidebar->addWidget(presenceToolsButton);

        logicToolsButton = new QPushButton(toolLibrarySidebar);
        logicToolsButton->setObjectName(QString::fromUtf8("logicToolsButton"));
        logicToolsButton->setMinimumSize(QSize(0, 48));
        logicToolsButton->setCheckable(true);
        logicToolsButton->setProperty("navButton", QVariant(true));

        verticalLayout_sidebar->addWidget(logicToolsButton);

        locationToolsButton = new QPushButton(toolLibrarySidebar);
        locationToolsButton->setObjectName(QString::fromUtf8("locationToolsButton"));
        locationToolsButton->setMinimumSize(QSize(0, 48));
        locationToolsButton->setCheckable(true);
        locationToolsButton->setProperty("navButton", QVariant(true));

        verticalLayout_sidebar->addWidget(locationToolsButton);

        deepLearningToolsButton = new QPushButton(toolLibrarySidebar);
        deepLearningToolsButton->setObjectName(QString::fromUtf8("deepLearningToolsButton"));
        deepLearningToolsButton->setMinimumSize(QSize(0, 48));
        deepLearningToolsButton->setCheckable(true);
        deepLearningToolsButton->setProperty("navButton", QVariant(true));

        verticalLayout_sidebar->addWidget(deepLearningToolsButton);

        defectToolsButton = new QPushButton(toolLibrarySidebar);
        defectToolsButton->setObjectName(QString::fromUtf8("defectToolsButton"));
        defectToolsButton->setMinimumSize(QSize(0, 48));
        defectToolsButton->setCheckable(true);
        defectToolsButton->setProperty("navButton", QVariant(true));

        verticalLayout_sidebar->addWidget(defectToolsButton);

        verticalSpacer_sidebar = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_sidebar->addItem(verticalSpacer_sidebar);


        horizontalLayout_body->addWidget(toolLibrarySidebar);

        toolGridPanel = new QFrame(toolLibraryBody);
        toolGridPanel->setObjectName(QString::fromUtf8("toolGridPanel"));
        toolGridPanel->setFrameShape(QFrame::NoFrame);
        verticalLayout_toolGridPanel = new QVBoxLayout(toolGridPanel);
        verticalLayout_toolGridPanel->setSpacing(0);
        verticalLayout_toolGridPanel->setObjectName(QString::fromUtf8("verticalLayout_toolGridPanel"));
        verticalLayout_toolGridPanel->setContentsMargins(0, 0, 0, 0);
        toolCategoryScrollArea = new QScrollArea(toolGridPanel);
        toolCategoryScrollArea->setObjectName(QString::fromUtf8("toolCategoryScrollArea"));
        toolCategoryScrollArea->setFrameShape(QFrame::NoFrame);
        toolCategoryScrollArea->setWidgetResizable(true);
        toolCategoryContent = new QWidget();
        toolCategoryContent->setObjectName(QString::fromUtf8("toolCategoryContent"));
        toolCategoryContent->setGeometry(QRect(0, -449, 642, 1974));
        verticalLayout_toolCategoryContent = new QVBoxLayout(toolCategoryContent);
        verticalLayout_toolCategoryContent->setSpacing(0);
        verticalLayout_toolCategoryContent->setObjectName(QString::fromUtf8("verticalLayout_toolCategoryContent"));
        verticalLayout_toolCategoryContent->setContentsMargins(0, 0, 0, 0);
        measurementCategoryFrame = new QFrame(toolCategoryContent);
        measurementCategoryFrame->setObjectName(QString::fromUtf8("measurementCategoryFrame"));
        measurementCategoryFrame->setFrameShape(QFrame::NoFrame);
        measurementCategoryFrame->setProperty("toolCategory", QVariant(true));
        gridLayout_measurementCategoryFrame = new QGridLayout(measurementCategoryFrame);
        gridLayout_measurementCategoryFrame->setObjectName(QString::fromUtf8("gridLayout_measurementCategoryFrame"));
        gridLayout_measurementCategoryFrame->setHorizontalSpacing(26);
        gridLayout_measurementCategoryFrame->setVerticalSpacing(20);
        gridLayout_measurementCategoryFrame->setContentsMargins(18, 12, 18, 12);
        measurementCategoryFrameTitleLabel = new QLabel(measurementCategoryFrame);
        measurementCategoryFrameTitleLabel->setObjectName(QString::fromUtf8("measurementCategoryFrameTitleLabel"));
        measurementCategoryFrameTitleLabel->setProperty("categoryTitle", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(measurementCategoryFrameTitleLabel, 0, 0, 1, 4);

        pointMeasureButton = new QToolButton(measurementCategoryFrame);
        pointMeasureButton->setObjectName(QString::fromUtf8("pointMeasureButton"));
        pointMeasureButton->setMinimumSize(QSize(132, 118));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/measure.svg"), QSize(), QIcon::Normal, QIcon::Off);
        pointMeasureButton->setIcon(icon);
        pointMeasureButton->setIconSize(QSize(46, 46));
        pointMeasureButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        pointMeasureButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(pointMeasureButton, 1, 0, 1, 1);

        lineMeasureButton = new QToolButton(measurementCategoryFrame);
        lineMeasureButton->setObjectName(QString::fromUtf8("lineMeasureButton"));
        lineMeasureButton->setMinimumSize(QSize(132, 118));
        lineMeasureButton->setIcon(icon);
        lineMeasureButton->setIconSize(QSize(46, 46));
        lineMeasureButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        lineMeasureButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(lineMeasureButton, 1, 1, 1, 1);

        contrastMeasureButton = new QToolButton(measurementCategoryFrame);
        contrastMeasureButton->setObjectName(QString::fromUtf8("contrastMeasureButton"));
        contrastMeasureButton->setMinimumSize(QSize(132, 118));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/icons/compare.svg"), QSize(), QIcon::Normal, QIcon::Off);
        contrastMeasureButton->setIcon(icon1);
        contrastMeasureButton->setIconSize(QSize(46, 46));
        contrastMeasureButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        contrastMeasureButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(contrastMeasureButton, 1, 2, 1, 1);

        grayAreaToolButton = new QToolButton(measurementCategoryFrame);
        grayAreaToolButton->setObjectName(QString::fromUtf8("grayAreaToolButton"));
        grayAreaToolButton->setMinimumSize(QSize(132, 118));
        grayAreaToolButton->setIcon(icon);
        grayAreaToolButton->setIconSize(QSize(46, 46));
        grayAreaToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        grayAreaToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(grayAreaToolButton, 1, 3, 1, 1);

        gapMeasureButton = new QToolButton(measurementCategoryFrame);
        gapMeasureButton->setObjectName(QString::fromUtf8("gapMeasureButton"));
        gapMeasureButton->setMinimumSize(QSize(132, 118));
        gapMeasureButton->setIcon(icon);
        gapMeasureButton->setIconSize(QSize(46, 46));
        gapMeasureButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        gapMeasureButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(gapMeasureButton, 2, 0, 1, 1);

        widthMeasureButton = new QToolButton(measurementCategoryFrame);
        widthMeasureButton->setObjectName(QString::fromUtf8("widthMeasureButton"));
        widthMeasureButton->setMinimumSize(QSize(132, 118));
        widthMeasureButton->setIcon(icon);
        widthMeasureButton->setIconSize(QSize(46, 46));
        widthMeasureButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        widthMeasureButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(widthMeasureButton, 2, 1, 1, 1);

        brightnessAverageButton = new QToolButton(measurementCategoryFrame);
        brightnessAverageButton->setObjectName(QString::fromUtf8("brightnessAverageButton"));
        brightnessAverageButton->setMinimumSize(QSize(132, 118));
        brightnessAverageButton->setIcon(icon);
        brightnessAverageButton->setIconSize(QSize(46, 46));
        brightnessAverageButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        brightnessAverageButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(brightnessAverageButton, 2, 2, 1, 1);

        lineAngleButton = new QToolButton(measurementCategoryFrame);
        lineAngleButton->setObjectName(QString::fromUtf8("lineAngleButton"));
        lineAngleButton->setMinimumSize(QSize(132, 118));
        lineAngleButton->setIcon(icon);
        lineAngleButton->setIconSize(QSize(46, 46));
        lineAngleButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        lineAngleButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(lineAngleButton, 2, 3, 1, 1);

        colorMeasureButton = new QToolButton(measurementCategoryFrame);
        colorMeasureButton->setObjectName(QString::fromUtf8("colorMeasureButton"));
        colorMeasureButton->setMinimumSize(QSize(132, 118));
        colorMeasureButton->setIcon(icon);
        colorMeasureButton->setIconSize(QSize(46, 46));
        colorMeasureButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        colorMeasureButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(colorMeasureButton, 3, 0, 1, 1);

        colorAreaToolButton = new QToolButton(measurementCategoryFrame);
        colorAreaToolButton->setObjectName(QString::fromUtf8("colorAreaToolButton"));
        colorAreaToolButton->setMinimumSize(QSize(132, 118));
        colorAreaToolButton->setIcon(icon);
        colorAreaToolButton->setIconSize(QSize(46, 46));
        colorAreaToolButton->setCheckable(true);
        colorAreaToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        colorAreaToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(colorAreaToolButton, 3, 1, 1, 1);

        diameterMeasureButton = new QToolButton(measurementCategoryFrame);
        diameterMeasureButton->setObjectName(QString::fromUtf8("diameterMeasureButton"));
        diameterMeasureButton->setMinimumSize(QSize(132, 118));
        diameterMeasureButton->setIcon(icon);
        diameterMeasureButton->setIconSize(QSize(46, 46));
        diameterMeasureButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        diameterMeasureButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(diameterMeasureButton, 3, 2, 1, 1);

        straightAngleButton = new QToolButton(measurementCategoryFrame);
        straightAngleButton->setObjectName(QString::fromUtf8("straightAngleButton"));
        straightAngleButton->setMinimumSize(QSize(132, 118));
        straightAngleButton->setIcon(icon);
        straightAngleButton->setIconSize(QSize(46, 46));
        straightAngleButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        straightAngleButton->setProperty("toolCard", QVariant(true));

        gridLayout_measurementCategoryFrame->addWidget(straightAngleButton, 3, 3, 1, 1);


        verticalLayout_toolCategoryContent->addWidget(measurementCategoryFrame);

        countCategoryFrame = new QFrame(toolCategoryContent);
        countCategoryFrame->setObjectName(QString::fromUtf8("countCategoryFrame"));
        countCategoryFrame->setFrameShape(QFrame::NoFrame);
        countCategoryFrame->setProperty("toolCategory", QVariant(true));
        gridLayout_countCategoryFrame = new QGridLayout(countCategoryFrame);
        gridLayout_countCategoryFrame->setObjectName(QString::fromUtf8("gridLayout_countCategoryFrame"));
        gridLayout_countCategoryFrame->setHorizontalSpacing(26);
        gridLayout_countCategoryFrame->setVerticalSpacing(20);
        gridLayout_countCategoryFrame->setContentsMargins(18, 12, 18, 12);
        countCategoryFrameTitleLabel = new QLabel(countCategoryFrame);
        countCategoryFrameTitleLabel->setObjectName(QString::fromUtf8("countCategoryFrameTitleLabel"));
        countCategoryFrameTitleLabel->setProperty("categoryTitle", QVariant(true));

        gridLayout_countCategoryFrame->addWidget(countCategoryFrameTitleLabel, 0, 0, 1, 4);

        counterToolButton = new QToolButton(countCategoryFrame);
        counterToolButton->setObjectName(QString::fromUtf8("counterToolButton"));
        counterToolButton->setMinimumSize(QSize(132, 118));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/tool.svg"), QSize(), QIcon::Normal, QIcon::Off);
        counterToolButton->setIcon(icon2);
        counterToolButton->setIconSize(QSize(46, 46));
        counterToolButton->setCheckable(true);
        counterToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        counterToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_countCategoryFrame->addWidget(counterToolButton, 1, 0, 1, 1);

        areaCounterButton = new QToolButton(countCategoryFrame);
        areaCounterButton->setObjectName(QString::fromUtf8("areaCounterButton"));
        areaCounterButton->setMinimumSize(QSize(132, 118));
        areaCounterButton->setIcon(icon2);
        areaCounterButton->setIconSize(QSize(46, 46));
        areaCounterButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        areaCounterButton->setProperty("toolCard", QVariant(true));

        gridLayout_countCategoryFrame->addWidget(areaCounterButton, 1, 1, 1, 1);

        edgeCounterButton = new QToolButton(countCategoryFrame);
        edgeCounterButton->setObjectName(QString::fromUtf8("edgeCounterButton"));
        edgeCounterButton->setMinimumSize(QSize(132, 118));
        edgeCounterButton->setIcon(icon2);
        edgeCounterButton->setIconSize(QSize(46, 46));
        edgeCounterButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        edgeCounterButton->setProperty("toolCard", QVariant(true));

        gridLayout_countCategoryFrame->addWidget(edgeCounterButton, 1, 2, 1, 1);


        verticalLayout_toolCategoryContent->addWidget(countCategoryFrame);

        recognitionCategoryFrame = new QFrame(toolCategoryContent);
        recognitionCategoryFrame->setObjectName(QString::fromUtf8("recognitionCategoryFrame"));
        recognitionCategoryFrame->setFrameShape(QFrame::NoFrame);
        recognitionCategoryFrame->setProperty("toolCategory", QVariant(true));
        gridLayout_recognitionCategoryFrame = new QGridLayout(recognitionCategoryFrame);
        gridLayout_recognitionCategoryFrame->setObjectName(QString::fromUtf8("gridLayout_recognitionCategoryFrame"));
        gridLayout_recognitionCategoryFrame->setHorizontalSpacing(26);
        gridLayout_recognitionCategoryFrame->setVerticalSpacing(20);
        gridLayout_recognitionCategoryFrame->setContentsMargins(18, 12, 18, 12);
        recognitionCategoryFrameTitleLabel = new QLabel(recognitionCategoryFrame);
        recognitionCategoryFrameTitleLabel->setObjectName(QString::fromUtf8("recognitionCategoryFrameTitleLabel"));
        recognitionCategoryFrameTitleLabel->setProperty("categoryTitle", QVariant(true));

        gridLayout_recognitionCategoryFrame->addWidget(recognitionCategoryFrameTitleLabel, 0, 0, 1, 4);

        ocrToolButton = new QToolButton(recognitionCategoryFrame);
        ocrToolButton->setObjectName(QString::fromUtf8("ocrToolButton"));
        ocrToolButton->setMinimumSize(QSize(132, 118));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icons/tool-step.svg"), QSize(), QIcon::Normal, QIcon::Off);
        ocrToolButton->setIcon(icon3);
        ocrToolButton->setIconSize(QSize(46, 46));
        ocrToolButton->setCheckable(true);
        ocrToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        ocrToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_recognitionCategoryFrame->addWidget(ocrToolButton, 1, 0, 1, 1);

        codeToolButton = new QToolButton(recognitionCategoryFrame);
        codeToolButton->setObjectName(QString::fromUtf8("codeToolButton"));
        codeToolButton->setMinimumSize(QSize(132, 118));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/zoom.svg"), QSize(), QIcon::Normal, QIcon::Off);
        codeToolButton->setIcon(icon4);
        codeToolButton->setIconSize(QSize(46, 46));
        codeToolButton->setCheckable(true);
        codeToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        codeToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_recognitionCategoryFrame->addWidget(codeToolButton, 1, 1, 1, 1);

        categoryToolButton = new QToolButton(recognitionCategoryFrame);
        categoryToolButton->setObjectName(QString::fromUtf8("categoryToolButton"));
        categoryToolButton->setMinimumSize(QSize(132, 118));
        categoryToolButton->setIcon(icon1);
        categoryToolButton->setIconSize(QSize(46, 46));
        categoryToolButton->setCheckable(true);
        categoryToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        categoryToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_recognitionCategoryFrame->addWidget(categoryToolButton, 1, 2, 1, 1);

        colorRecognitionToolButton = new QToolButton(recognitionCategoryFrame);
        colorRecognitionToolButton->setObjectName(QString::fromUtf8("colorRecognitionToolButton"));
        colorRecognitionToolButton->setMinimumSize(QSize(132, 118));
        colorRecognitionToolButton->setIcon(icon1);
        colorRecognitionToolButton->setIconSize(QSize(46, 46));
        colorRecognitionToolButton->setCheckable(true);
        colorRecognitionToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        colorRecognitionToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_recognitionCategoryFrame->addWidget(colorRecognitionToolButton, 1, 3, 1, 1);

        colorComparisonToolButton = new QToolButton(recognitionCategoryFrame);
        colorComparisonToolButton->setObjectName(QString::fromUtf8("colorComparisonToolButton"));
        colorComparisonToolButton->setMinimumSize(QSize(132, 118));
        colorComparisonToolButton->setIcon(icon1);
        colorComparisonToolButton->setIconSize(QSize(46, 46));
        colorComparisonToolButton->setCheckable(true);
        colorComparisonToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        colorComparisonToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_recognitionCategoryFrame->addWidget(colorComparisonToolButton, 2, 0, 1, 1);

        registrationClassToolButton = new QToolButton(recognitionCategoryFrame);
        registrationClassToolButton->setObjectName(QString::fromUtf8("registrationClassToolButton"));
        registrationClassToolButton->setMinimumSize(QSize(132, 118));
        registrationClassToolButton->setIcon(icon1);
        registrationClassToolButton->setIconSize(QSize(46, 46));
        registrationClassToolButton->setCheckable(true);
        registrationClassToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        registrationClassToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_recognitionCategoryFrame->addWidget(registrationClassToolButton, 2, 1, 1, 1);

        registrationClassDetectionToolButton = new QToolButton(recognitionCategoryFrame);
        registrationClassDetectionToolButton->setObjectName(QString::fromUtf8("registrationClassDetectionToolButton"));
        registrationClassDetectionToolButton->setMinimumSize(QSize(132, 118));
        registrationClassDetectionToolButton->setIcon(icon1);
        registrationClassDetectionToolButton->setIconSize(QSize(46, 46));
        registrationClassDetectionToolButton->setCheckable(true);
        registrationClassDetectionToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        registrationClassDetectionToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_recognitionCategoryFrame->addWidget(registrationClassDetectionToolButton, 2, 2, 1, 1);


        verticalLayout_toolCategoryContent->addWidget(recognitionCategoryFrame);

        presenceCategoryFrame = new QFrame(toolCategoryContent);
        presenceCategoryFrame->setObjectName(QString::fromUtf8("presenceCategoryFrame"));
        presenceCategoryFrame->setFrameShape(QFrame::NoFrame);
        presenceCategoryFrame->setProperty("toolCategory", QVariant(true));
        gridLayout_presenceCategoryFrame = new QGridLayout(presenceCategoryFrame);
        gridLayout_presenceCategoryFrame->setObjectName(QString::fromUtf8("gridLayout_presenceCategoryFrame"));
        gridLayout_presenceCategoryFrame->setHorizontalSpacing(26);
        gridLayout_presenceCategoryFrame->setVerticalSpacing(20);
        gridLayout_presenceCategoryFrame->setContentsMargins(18, 12, 18, 12);
        presenceToolButton = new QToolButton(presenceCategoryFrame);
        presenceToolButton->setObjectName(QString::fromUtf8("presenceToolButton"));
        presenceToolButton->setMinimumSize(QSize(132, 118));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/eye.svg"), QSize(), QIcon::Normal, QIcon::Off);
        presenceToolButton->setIcon(icon5);
        presenceToolButton->setIconSize(QSize(46, 46));
        presenceToolButton->setCheckable(true);
        presenceToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        presenceToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_presenceCategoryFrame->addWidget(presenceToolButton, 1, 0, 1, 1);

        circlePresenceButton = new QToolButton(presenceCategoryFrame);
        circlePresenceButton->setObjectName(QString::fromUtf8("circlePresenceButton"));
        circlePresenceButton->setMinimumSize(QSize(132, 118));
        circlePresenceButton->setIcon(icon5);
        circlePresenceButton->setIconSize(QSize(46, 46));
        circlePresenceButton->setCheckable(true);
        circlePresenceButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        circlePresenceButton->setProperty("toolCard", QVariant(true));

        gridLayout_presenceCategoryFrame->addWidget(circlePresenceButton, 1, 1, 1, 1);

        blobPresenceButton = new QToolButton(presenceCategoryFrame);
        blobPresenceButton->setObjectName(QString::fromUtf8("blobPresenceButton"));
        blobPresenceButton->setMinimumSize(QSize(132, 118));
        blobPresenceButton->setIcon(icon5);
        blobPresenceButton->setIconSize(QSize(46, 46));
        blobPresenceButton->setCheckable(true);
        blobPresenceButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        blobPresenceButton->setProperty("toolCard", QVariant(true));

        gridLayout_presenceCategoryFrame->addWidget(blobPresenceButton, 1, 2, 1, 1);

        edgePresenceButton = new QToolButton(presenceCategoryFrame);
        edgePresenceButton->setObjectName(QString::fromUtf8("edgePresenceButton"));
        edgePresenceButton->setMinimumSize(QSize(132, 118));
        edgePresenceButton->setIcon(icon5);
        edgePresenceButton->setIconSize(QSize(46, 46));
        edgePresenceButton->setCheckable(true);
        edgePresenceButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        edgePresenceButton->setProperty("toolCard", QVariant(true));

        gridLayout_presenceCategoryFrame->addWidget(edgePresenceButton, 1, 3, 1, 1);

        linePresenceButton = new QToolButton(presenceCategoryFrame);
        linePresenceButton->setObjectName(QString::fromUtf8("linePresenceButton"));
        linePresenceButton->setMinimumSize(QSize(132, 118));
        linePresenceButton->setIcon(icon5);
        linePresenceButton->setIconSize(QSize(46, 46));
        linePresenceButton->setCheckable(true);
        linePresenceButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        linePresenceButton->setProperty("toolCard", QVariant(true));

        gridLayout_presenceCategoryFrame->addWidget(linePresenceButton, 2, 0, 1, 1);

        contourPresenceButton = new QToolButton(presenceCategoryFrame);
        contourPresenceButton->setObjectName(QString::fromUtf8("contourPresenceButton"));
        contourPresenceButton->setMinimumSize(QSize(132, 118));
        contourPresenceButton->setIcon(icon5);
        contourPresenceButton->setIconSize(QSize(46, 46));
        contourPresenceButton->setCheckable(true);
        contourPresenceButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        contourPresenceButton->setProperty("toolCard", QVariant(true));

        gridLayout_presenceCategoryFrame->addWidget(contourPresenceButton, 2, 1, 1, 1);

        presenceCategoryFrameTitleLabel = new QLabel(presenceCategoryFrame);
        presenceCategoryFrameTitleLabel->setObjectName(QString::fromUtf8("presenceCategoryFrameTitleLabel"));
        presenceCategoryFrameTitleLabel->setProperty("categoryTitle", QVariant(true));

        gridLayout_presenceCategoryFrame->addWidget(presenceCategoryFrameTitleLabel, 0, 0, 1, 4);


        verticalLayout_toolCategoryContent->addWidget(presenceCategoryFrame);

        logicCategoryFrame = new QFrame(toolCategoryContent);
        logicCategoryFrame->setObjectName(QString::fromUtf8("logicCategoryFrame"));
        logicCategoryFrame->setFrameShape(QFrame::NoFrame);
        logicCategoryFrame->setProperty("toolCategory", QVariant(true));
        gridLayout_logicCategoryFrame = new QGridLayout(logicCategoryFrame);
        gridLayout_logicCategoryFrame->setObjectName(QString::fromUtf8("gridLayout_logicCategoryFrame"));
        gridLayout_logicCategoryFrame->setHorizontalSpacing(26);
        gridLayout_logicCategoryFrame->setVerticalSpacing(20);
        gridLayout_logicCategoryFrame->setContentsMargins(18, 12, 18, 12);
        logicCategoryFrameTitleLabel = new QLabel(logicCategoryFrame);
        logicCategoryFrameTitleLabel->setObjectName(QString::fromUtf8("logicCategoryFrameTitleLabel"));
        logicCategoryFrameTitleLabel->setProperty("categoryTitle", QVariant(true));

        gridLayout_logicCategoryFrame->addWidget(logicCategoryFrameTitleLabel, 0, 0, 1, 4);

        judgeToolButton = new QToolButton(logicCategoryFrame);
        judgeToolButton->setObjectName(QString::fromUtf8("judgeToolButton"));
        judgeToolButton->setMinimumSize(QSize(132, 118));
        QIcon icon6;
        icon6.addFile(QString::fromUtf8(":/icons/comm.svg"), QSize(), QIcon::Normal, QIcon::Off);
        judgeToolButton->setIcon(icon6);
        judgeToolButton->setIconSize(QSize(46, 46));
        judgeToolButton->setCheckable(true);
        judgeToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        judgeToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_logicCategoryFrame->addWidget(judgeToolButton, 1, 0, 1, 1);

        conditionToolButton = new QToolButton(logicCategoryFrame);
        conditionToolButton->setObjectName(QString::fromUtf8("conditionToolButton"));
        conditionToolButton->setMinimumSize(QSize(132, 118));
        conditionToolButton->setIcon(icon6);
        conditionToolButton->setIconSize(QSize(46, 46));
        conditionToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        conditionToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_logicCategoryFrame->addWidget(conditionToolButton, 1, 1, 1, 1);

        variableToolButton = new QToolButton(logicCategoryFrame);
        variableToolButton->setObjectName(QString::fromUtf8("variableToolButton"));
        variableToolButton->setMinimumSize(QSize(132, 118));
        variableToolButton->setIcon(icon6);
        variableToolButton->setIconSize(QSize(46, 46));
        variableToolButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        variableToolButton->setProperty("toolCard", QVariant(true));

        gridLayout_logicCategoryFrame->addWidget(variableToolButton, 1, 2, 1, 1);

        outputLogicButton = new QToolButton(logicCategoryFrame);
        outputLogicButton->setObjectName(QString::fromUtf8("outputLogicButton"));
        outputLogicButton->setMinimumSize(QSize(132, 118));
        QIcon icon7;
        icon7.addFile(QString::fromUtf8(":/icons/output.svg"), QSize(), QIcon::Normal, QIcon::Off);
        outputLogicButton->setIcon(icon7);
        outputLogicButton->setIconSize(QSize(46, 46));
        outputLogicButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        outputLogicButton->setProperty("toolCard", QVariant(true));

        gridLayout_logicCategoryFrame->addWidget(outputLogicButton, 1, 3, 1, 1);


        verticalLayout_toolCategoryContent->addWidget(logicCategoryFrame);

        locationCategoryFrame = new QFrame(toolCategoryContent);
        locationCategoryFrame->setObjectName(QString::fromUtf8("locationCategoryFrame"));
        locationCategoryFrame->setFrameShape(QFrame::NoFrame);
        locationCategoryFrame->setProperty("toolCategory", QVariant(true));
        gridLayout_locationCategoryFrame = new QGridLayout(locationCategoryFrame);
        gridLayout_locationCategoryFrame->setObjectName(QString::fromUtf8("gridLayout_locationCategoryFrame"));
        gridLayout_locationCategoryFrame->setHorizontalSpacing(26);
        gridLayout_locationCategoryFrame->setVerticalSpacing(20);
        gridLayout_locationCategoryFrame->setContentsMargins(18, 12, 18, 12);
        locationCategoryFrameTitleLabel = new QLabel(locationCategoryFrame);
        locationCategoryFrameTitleLabel->setObjectName(QString::fromUtf8("locationCategoryFrameTitleLabel"));
        locationCategoryFrameTitleLabel->setProperty("categoryTitle", QVariant(true));

        gridLayout_locationCategoryFrame->addWidget(locationCategoryFrameTitleLabel, 0, 0, 1, 4);

        templateLocationButton = new QToolButton(locationCategoryFrame);
        templateLocationButton->setObjectName(QString::fromUtf8("templateLocationButton"));
        templateLocationButton->setMinimumSize(QSize(132, 118));
        QIcon icon8;
        icon8.addFile(QString::fromUtf8(":/icons/fit.svg"), QSize(), QIcon::Normal, QIcon::Off);
        templateLocationButton->setIcon(icon8);
        templateLocationButton->setIconSize(QSize(46, 46));
        templateLocationButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        templateLocationButton->setProperty("toolCard", QVariant(true));

        gridLayout_locationCategoryFrame->addWidget(templateLocationButton, 1, 0, 1, 1);

        edgeLocationButton = new QToolButton(locationCategoryFrame);
        edgeLocationButton->setObjectName(QString::fromUtf8("edgeLocationButton"));
        edgeLocationButton->setMinimumSize(QSize(132, 118));
        edgeLocationButton->setIcon(icon8);
        edgeLocationButton->setIconSize(QSize(46, 46));
        edgeLocationButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        edgeLocationButton->setProperty("toolCard", QVariant(true));

        gridLayout_locationCategoryFrame->addWidget(edgeLocationButton, 1, 1, 1, 1);

        circleLocationButton = new QToolButton(locationCategoryFrame);
        circleLocationButton->setObjectName(QString::fromUtf8("circleLocationButton"));
        circleLocationButton->setMinimumSize(QSize(132, 118));
        circleLocationButton->setIcon(icon8);
        circleLocationButton->setIconSize(QSize(46, 46));
        circleLocationButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        circleLocationButton->setProperty("toolCard", QVariant(true));

        gridLayout_locationCategoryFrame->addWidget(circleLocationButton, 1, 2, 1, 1);


        verticalLayout_toolCategoryContent->addWidget(locationCategoryFrame);

        deepLearningCategoryFrame = new QFrame(toolCategoryContent);
        deepLearningCategoryFrame->setObjectName(QString::fromUtf8("deepLearningCategoryFrame"));
        deepLearningCategoryFrame->setFrameShape(QFrame::NoFrame);
        deepLearningCategoryFrame->setProperty("toolCategory", QVariant(true));
        gridLayout_deepLearningCategoryFrame = new QGridLayout(deepLearningCategoryFrame);
        gridLayout_deepLearningCategoryFrame->setObjectName(QString::fromUtf8("gridLayout_deepLearningCategoryFrame"));
        gridLayout_deepLearningCategoryFrame->setHorizontalSpacing(26);
        gridLayout_deepLearningCategoryFrame->setVerticalSpacing(20);
        gridLayout_deepLearningCategoryFrame->setContentsMargins(18, 12, 18, 12);
        deepLearningCategoryFrameTitleLabel = new QLabel(deepLearningCategoryFrame);
        deepLearningCategoryFrameTitleLabel->setObjectName(QString::fromUtf8("deepLearningCategoryFrameTitleLabel"));
        deepLearningCategoryFrameTitleLabel->setProperty("categoryTitle", QVariant(true));

        gridLayout_deepLearningCategoryFrame->addWidget(deepLearningCategoryFrameTitleLabel, 0, 0, 1, 5);

        dlDetectButton = new QToolButton(deepLearningCategoryFrame);
        dlDetectButton->setObjectName(QString::fromUtf8("dlDetectButton"));
        dlDetectButton->setMinimumSize(QSize(132, 118));
        QIcon icon9;
        icon9.addFile(QString::fromUtf8(":/icons/monitor.svg"), QSize(), QIcon::Normal, QIcon::Off);
        dlDetectButton->setIcon(icon9);
        dlDetectButton->setIconSize(QSize(46, 46));
        dlDetectButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        dlDetectButton->setProperty("toolCard", QVariant(true));

        gridLayout_deepLearningCategoryFrame->addWidget(dlDetectButton, 1, 0, 1, 1);

        FaultDetectButton = new QToolButton(deepLearningCategoryFrame);
        FaultDetectButton->setObjectName(QString::fromUtf8("FaultDetectButton"));
        FaultDetectButton->setMinimumSize(QSize(132, 118));
        FaultDetectButton->setIcon(icon9);
        FaultDetectButton->setIconSize(QSize(46, 46));
        FaultDetectButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        FaultDetectButton->setProperty("toolCard", QVariant(true));

        gridLayout_deepLearningCategoryFrame->addWidget(FaultDetectButton, 1, 2, 1, 1);

        ClassifyButton = new QToolButton(deepLearningCategoryFrame);
        ClassifyButton->setObjectName(QString::fromUtf8("ClassifyButton"));
        ClassifyButton->setMinimumSize(QSize(132, 118));
        ClassifyButton->setIcon(icon9);
        ClassifyButton->setIconSize(QSize(46, 46));
        ClassifyButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        ClassifyButton->setProperty("toolCard", QVariant(true));

        gridLayout_deepLearningCategoryFrame->addWidget(ClassifyButton, 1, 1, 1, 1);


        verticalLayout_toolCategoryContent->addWidget(deepLearningCategoryFrame);

        defectCategoryFrame = new QFrame(toolCategoryContent);
        defectCategoryFrame->setObjectName(QString::fromUtf8("defectCategoryFrame"));
        defectCategoryFrame->setFrameShape(QFrame::NoFrame);
        defectCategoryFrame->setProperty("toolCategory", QVariant(true));
        gridLayout_defectCategoryFrame = new QGridLayout(defectCategoryFrame);
        gridLayout_defectCategoryFrame->setObjectName(QString::fromUtf8("gridLayout_defectCategoryFrame"));
        gridLayout_defectCategoryFrame->setHorizontalSpacing(26);
        gridLayout_defectCategoryFrame->setVerticalSpacing(20);
        gridLayout_defectCategoryFrame->setContentsMargins(18, 12, 18, 12);
        defectCategoryFrameTitleLabel = new QLabel(defectCategoryFrame);
        defectCategoryFrameTitleLabel->setObjectName(QString::fromUtf8("defectCategoryFrameTitleLabel"));
        defectCategoryFrameTitleLabel->setProperty("categoryTitle", QVariant(true));

        gridLayout_defectCategoryFrame->addWidget(defectCategoryFrameTitleLabel, 0, 0, 1, 4);

        scratchDefectButton = new QToolButton(defectCategoryFrame);
        scratchDefectButton->setObjectName(QString::fromUtf8("scratchDefectButton"));
        scratchDefectButton->setMinimumSize(QSize(132, 118));
        scratchDefectButton->setIcon(icon5);
        scratchDefectButton->setIconSize(QSize(46, 46));
        scratchDefectButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        scratchDefectButton->setProperty("toolCard", QVariant(true));

        gridLayout_defectCategoryFrame->addWidget(scratchDefectButton, 1, 0, 1, 1);

        stainDefectButton = new QToolButton(defectCategoryFrame);
        stainDefectButton->setObjectName(QString::fromUtf8("stainDefectButton"));
        stainDefectButton->setMinimumSize(QSize(132, 118));
        stainDefectButton->setIcon(icon5);
        stainDefectButton->setIconSize(QSize(46, 46));
        stainDefectButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        stainDefectButton->setProperty("toolCard", QVariant(true));

        gridLayout_defectCategoryFrame->addWidget(stainDefectButton, 1, 1, 1, 1);

        missingDefectButton = new QToolButton(defectCategoryFrame);
        missingDefectButton->setObjectName(QString::fromUtf8("missingDefectButton"));
        missingDefectButton->setMinimumSize(QSize(132, 118));
        missingDefectButton->setIcon(icon5);
        missingDefectButton->setIconSize(QSize(46, 46));
        missingDefectButton->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        missingDefectButton->setProperty("toolCard", QVariant(true));

        gridLayout_defectCategoryFrame->addWidget(missingDefectButton, 1, 2, 1, 1);


        verticalLayout_toolCategoryContent->addWidget(defectCategoryFrame);

        verticalSpacer_toolCategories = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_toolCategoryContent->addItem(verticalSpacer_toolCategories);

        toolCategoryScrollArea->setWidget(toolCategoryContent);

        verticalLayout_toolGridPanel->addWidget(toolCategoryScrollArea);


        horizontalLayout_body->addWidget(toolGridPanel);

        toolPreviewPanel = new QFrame(toolLibraryBody);
        toolPreviewPanel->setObjectName(QString::fromUtf8("toolPreviewPanel"));
        toolPreviewPanel->setMinimumSize(QSize(340, 0));
        toolPreviewPanel->setMaximumSize(QSize(340, 16777215));
        toolPreviewPanel->setFrameShape(QFrame::NoFrame);
        verticalLayout_preview = new QVBoxLayout(toolPreviewPanel);
        verticalLayout_preview->setSpacing(0);
        verticalLayout_preview->setObjectName(QString::fromUtf8("verticalLayout_preview"));
        verticalLayout_preview->setContentsMargins(0, 0, 0, 0);
        buttonBox = new QDialogButtonBox(toolPreviewPanel);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout_preview->addWidget(buttonBox);

        toolPreviewGraphic = new QFrame(toolPreviewPanel);
        toolPreviewGraphic->setObjectName(QString::fromUtf8("toolPreviewGraphic"));
        toolPreviewGraphic->setFrameShape(QFrame::NoFrame);
        verticalLayout_previewGraphic = new QVBoxLayout(toolPreviewGraphic);
        verticalLayout_previewGraphic->setSpacing(20);
        verticalLayout_previewGraphic->setObjectName(QString::fromUtf8("verticalLayout_previewGraphic"));
        verticalLayout_previewGraphic->setContentsMargins(24, 120, 24, 0);
        previewIconLabel = new QLabel(toolPreviewGraphic);
        previewIconLabel->setObjectName(QString::fromUtf8("previewIconLabel"));

        verticalLayout_previewGraphic->addWidget(previewIconLabel);

        previewTitleLabel = new QLabel(toolPreviewGraphic);
        previewTitleLabel->setObjectName(QString::fromUtf8("previewTitleLabel"));

        verticalLayout_previewGraphic->addWidget(previewTitleLabel);

        previewDescriptionLabel = new QLabel(toolPreviewGraphic);
        previewDescriptionLabel->setObjectName(QString::fromUtf8("previewDescriptionLabel"));

        verticalLayout_previewGraphic->addWidget(previewDescriptionLabel);


        verticalLayout_preview->addWidget(toolPreviewGraphic);

        verticalSpacer_preview = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_preview->addItem(verticalSpacer_preview);


        horizontalLayout_body->addWidget(toolPreviewPanel);


        verticalLayout_root->addWidget(toolLibraryBody);

        toolLibraryBottomBar = new QFrame(ToolLibraryDialog);
        toolLibraryBottomBar->setObjectName(QString::fromUtf8("toolLibraryBottomBar"));
        toolLibraryBottomBar->setMinimumSize(QSize(0, 62));
        toolLibraryBottomBar->setMaximumSize(QSize(16777215, 62));
        toolLibraryBottomBar->setFrameShape(QFrame::NoFrame);
        horizontalLayout_bottom = new QHBoxLayout(toolLibraryBottomBar);
        horizontalLayout_bottom->setSpacing(16);
        horizontalLayout_bottom->setObjectName(QString::fromUtf8("horizontalLayout_bottom"));
        horizontalLayout_bottom->setContentsMargins(18, 10, 14, 10);
        toolLibraryTipLabel = new QLabel(toolLibraryBottomBar);
        toolLibraryTipLabel->setObjectName(QString::fromUtf8("toolLibraryTipLabel"));

        horizontalLayout_bottom->addWidget(toolLibraryTipLabel);

        horizontalSpacer_bottom = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_bottom->addItem(horizontalSpacer_bottom);

        confirmButton = new QPushButton(toolLibraryBottomBar);
        confirmButton->setObjectName(QString::fromUtf8("confirmButton"));
        confirmButton->setMinimumSize(QSize(106, 44));

        horizontalLayout_bottom->addWidget(confirmButton);

        cancelButton = new QPushButton(toolLibraryBottomBar);
        cancelButton->setObjectName(QString::fromUtf8("cancelButton"));
        cancelButton->setMinimumSize(QSize(106, 44));

        horizontalLayout_bottom->addWidget(cancelButton);


        verticalLayout_root->addWidget(toolLibraryBottomBar);


        retranslateUi(ToolLibraryDialog);

        QMetaObject::connectSlotsByName(ToolLibraryDialog);
    } // setupUi

    void retranslateUi(QDialog *ToolLibraryDialog)
    {
        ToolLibraryDialog->setWindowTitle(QCoreApplication::translate("ToolLibraryDialog", "\346\267\273\345\212\240\345\267\245\345\205\267", nullptr));
        toolLibraryTitleLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\267\273\345\212\240\345\267\245\345\205\267", nullptr));
        closeButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\303\227", nullptr));
        toolSearchEdit->setPlaceholderText(QCoreApplication::translate("ToolLibraryDialog", "\346\220\234\347\264\242\345\267\245\345\205\267\345\220\215", nullptr));
        generalToolsLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\351\200\232\347\224\250\345\267\245\345\205\267", nullptr));
        allToolsButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\205\250\351\203\250\345\267\245\345\205\267", nullptr));
        measureToolsButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\265\213\351\207\217\345\267\245\345\205\267", nullptr));
        countToolsButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\256\241\346\225\260\345\267\245\345\205\267", nullptr));
        recognitionToolsButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\257\206\345\210\253\345\267\245\345\205\267", nullptr));
        presenceToolsButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\234\211\346\227\240\345\267\245\345\205\267", nullptr));
        logicToolsButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\351\200\273\350\276\221\345\267\245\345\205\267", nullptr));
        locationToolsButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\256\232\344\275\215\345\267\245\345\205\267", nullptr));
        deepLearningToolsButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\267\261\345\272\246\345\255\246\344\271\240", nullptr));
        defectToolsButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\274\272\351\231\267\345\267\245\345\205\267", nullptr));
        measurementCategoryFrameTitleLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\265\213\351\207\217\345\267\245\345\205\267", nullptr));
        pointMeasureButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\202\271\347\202\271\346\265\213\351\207\217", nullptr));
        lineMeasureButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\202\271\347\272\277\346\265\213\351\207\217", nullptr));
        contrastMeasureButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\257\271\346\257\224\345\272\246\346\265\213\351\207\217", nullptr));
        grayAreaToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\201\260\345\272\246\351\235\242\347\247\257", nullptr));
        gapMeasureButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\212\202\350\267\235\346\265\213\351\207\217", nullptr));
        widthMeasureButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\256\275\345\272\246\346\265\213\351\207\217", nullptr));
        brightnessAverageButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\344\272\256\345\272\246\345\235\207\345\200\274", nullptr));
        lineAngleButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\272\277\347\272\277\350\247\222\345\272\246", nullptr));
        colorMeasureButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\351\242\234\350\211\262\346\265\213\351\207\217", nullptr));
        colorAreaToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\351\242\234\350\211\262\351\235\242\347\247\257", nullptr));
        diameterMeasureButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\233\264\345\276\204\346\265\213\351\207\217", nullptr));
        straightAngleButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\233\264\347\272\277\350\247\222\345\272\246", nullptr));
        countCategoryFrameTitleLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\256\241\346\225\260\345\267\245\345\205\267", nullptr));
        counterToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\255\246\344\271\240\350\256\241\346\225\260", nullptr));
        areaCounterButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\214\272\345\237\237\350\256\241\346\225\260", nullptr));
        edgeCounterButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\276\271\347\274\230\350\256\241\346\225\260", nullptr));
        recognitionCategoryFrameTitleLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\257\206\345\210\253\345\267\245\345\205\267", nullptr));
        ocrToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\255\227\347\254\246\350\257\206\345\210\253", nullptr));
        codeToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\240\201\350\257\206\345\210\253", nullptr));
        categoryToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\247\215\347\261\273\350\257\206\345\210\253", nullptr));
        colorRecognitionToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\351\242\234\350\211\262\350\257\206\345\210\253", nullptr));
        colorComparisonToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\351\242\234\350\211\262\346\257\224\350\276\203", nullptr));
        registrationClassToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\263\250\345\206\214\345\210\206\347\261\273", nullptr));
        registrationClassDetectionToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\263\250\345\206\214\347\233\256\346\240\207\346\243\200\346\265\213", nullptr));
        presenceToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\233\276\346\241\210\346\234\211\346\227\240", nullptr));
        circlePresenceButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\234\206\346\234\211\346\227\240", nullptr));
        blobPresenceButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\226\221\347\202\271\346\234\211\346\227\240", nullptr));
        edgePresenceButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\276\271\347\274\230\346\234\211\346\227\240", nullptr));
        linePresenceButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\233\264\347\272\277\346\234\211\346\227\240", nullptr));
        contourPresenceButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\275\256\345\273\223\346\234\211\346\227\240", nullptr));
        presenceCategoryFrameTitleLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\234\211\346\227\240\345\267\245\345\205\267", nullptr));
        logicCategoryFrameTitleLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\351\200\273\350\276\221\345\267\245\345\205\267", nullptr));
        judgeToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\274\202\345\270\270\345\210\244\346\226\255", nullptr));
        conditionToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\235\241\344\273\266\345\210\206\346\224\257", nullptr));
        variableToolButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\217\230\351\207\217\350\256\241\347\256\227", nullptr));
        outputLogicButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\273\223\346\236\234\350\276\223\345\207\272", nullptr));
        locationCategoryFrameTitleLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\256\232\344\275\215\345\267\245\345\205\267", nullptr));
        templateLocationButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\250\241\346\235\277\345\256\232\344\275\215", nullptr));
        edgeLocationButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\276\271\347\274\230\345\256\232\344\275\215", nullptr));
        circleLocationButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\234\206\345\256\232\344\275\215", nullptr));
        deepLearningCategoryFrameTitleLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\267\261\345\272\246\345\255\246\344\271\240", nullptr));
        dlDetectButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\233\256\346\240\207\346\243\200\346\265\213", nullptr));
        FaultDetectButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\274\202\345\270\270\346\243\200\346\265\213", nullptr));
        ClassifyButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\210\206\347\261\273", nullptr));
        defectCategoryFrameTitleLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\274\272\351\231\267\345\267\245\345\205\267", nullptr));
        scratchDefectButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\210\222\344\274\244\346\243\200\346\265\213", nullptr));
        stainDefectButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\346\261\241\346\270\215\346\243\200\346\265\213", nullptr));
        missingDefectButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\274\272\345\244\261\346\243\200\346\265\213", nullptr));
        previewIconLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\342\214\225", nullptr));
        previewTitleLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\257\267\351\200\211\346\213\251\345\267\245\345\205\267", nullptr));
        previewDescriptionLabel->setText(QCoreApplication::translate("ToolLibraryDialog", "\350\257\267\346\212\212\351\274\240\346\240\207\346\202\254\346\265\256\345\234\250\345\267\246\344\276\247\345\267\245\345\205\267\344\270\212\346\237\245\347\234\213\350\257\264\346\230\216", nullptr));
        toolLibraryTipLabel->setText(QString());
        confirmButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\347\241\256\345\256\232", nullptr));
        cancelButton->setText(QCoreApplication::translate("ToolLibraryDialog", "\345\217\226\346\266\210", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ToolLibraryDialog: public Ui_ToolLibraryDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TOOLLIBRARYDIALOG_H
