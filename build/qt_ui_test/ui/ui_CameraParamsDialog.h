/********************************************************************************
** Form generated from reading UI file 'CameraParamsDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CAMERAPARAMSDIALOG_H
#define UI_CAMERAPARAMSDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGridLayout>
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

class Ui_CameraParamsDialog
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
    QVBoxLayout *verticalLayout_editor;
    QFrame *editorHeaderFrame;
    QHBoxLayout *horizontalLayout_editorHeader;
    QLabel *editorTitleLabel;
    QSpacerItem *horizontalSpacer_editorHeader;
    QFrame *segmentFrame;
    QHBoxLayout *horizontalLayout_segmentFrame;
    QPushButton *basicModeButton;
    QPushButton *allModeButton;
    QStackedWidget *cameraParamsStack;
    QWidget *basicParamsPage;
    QVBoxLayout *verticalLayout_basicParamsPage;
    QScrollArea *basicParamsScrollArea;
    QWidget *basicParamsScrollContent;
    QVBoxLayout *verticalLayout_basicParamsContent;
    QFrame *imagingAdjustCard;
    QVBoxLayout *verticalLayout_imagingAdjustCard;
    QFrame *imagingAdjustCardHeaderFrame;
    QHBoxLayout *horizontalLayout_imagingAdjustCardHeader;
    QLabel *imagingAdjustCardTitleLabel;
    QSpacerItem *horizontalSpacer_imagingAdjustCardHeader;
    QToolButton *imagingAdjustCardCollapseButton;
    QLabel *imagingAdjustHintLabel;
    QPushButton *oneKeyTuneButton;
    QFrame *triggerConfigCard;
    QVBoxLayout *verticalLayout_triggerConfigCard;
    QFrame *triggerConfigCardHeaderFrame;
    QHBoxLayout *horizontalLayout_triggerConfigCardHeader;
    QLabel *triggerConfigCardTitleLabel;
    QSpacerItem *horizontalSpacer_triggerConfigCardHeader;
    QToolButton *triggerConfigCardCollapseButton;
    QFrame *basicTriggerFormFrame;
    QGridLayout *gridLayout_basicTrigger;
    QLabel *triggerModeLabel;
    QFrame *triggerSegmentFrame;
    QHBoxLayout *horizontalLayout_triggerSegmentFrame;
    QPushButton *internalTriggerButton;
    QPushButton *externalTriggerButton;
    QFrame *brightnessCard;
    QVBoxLayout *verticalLayout_brightnessCard;
    QFrame *brightnessCardHeaderFrame;
    QHBoxLayout *horizontalLayout_brightnessCardHeader;
    QLabel *brightnessCardTitleLabel;
    QSpacerItem *horizontalSpacer_brightnessCardHeader;
    QToolButton *brightnessCardCollapseButton;
    QFrame *basicBrightnessFormFrame;
    QGridLayout *gridLayout_basicBrightness;
    QLabel *autoAdjustLabel;
    QPushButton *autoAdjustButton;
    QFrame *focusCard;
    QVBoxLayout *verticalLayout_focusCard;
    QFrame *focusCardHeaderFrame;
    QHBoxLayout *horizontalLayout_focusCardHeader;
    QLabel *focusCardTitleLabel;
    QSpacerItem *horizontalSpacer_focusCardHeader;
    QToolButton *focusCardCollapseButton;
    QFrame *basicFocusFormFrame;
    QGridLayout *gridLayout_basicFocus;
    QLabel *focusRegionLabel;
    QFrame *focusRegionButtonsFrame;
    QHBoxLayout *horizontalLayout_focusRegionButtons;
    QToolButton *focusRegionAutoButton;
    QToolButton *focusRegionRectButton;
    QLabel *autoFocusLabel;
    QFrame *autoFocusButtonsFrame;
    QHBoxLayout *horizontalLayout_autoFocusButtons;
    QPushButton *autoFocusButton;
    QToolButton *autoFocusResetButton;
    QSpacerItem *verticalSpacer_basicParams;
    QWidget *allParamsPage;
    QVBoxLayout *verticalLayout_allParamsPage;
    QScrollArea *allParamsScrollArea;
    QWidget *allParamsScrollContent;
    QVBoxLayout *verticalLayout_allParamsContent;
    QFrame *allImagingAdjustCard;
    QVBoxLayout *verticalLayout_allImagingAdjustCard;
    QFrame *allImagingAdjustCardHeaderFrame;
    QHBoxLayout *horizontalLayout_allImagingAdjustCardHeader;
    QLabel *allImagingAdjustCardTitleLabel;
    QSpacerItem *horizontalSpacer_allImagingAdjustCardHeader;
    QToolButton *allImagingAdjustCardCollapseButton;
    QLabel *allImagingAdjustHintLabel;
    QPushButton *allOneKeyTuneButton;
    QFrame *allTriggerConfigCard;
    QVBoxLayout *verticalLayout_allTriggerConfigCard;
    QFrame *allTriggerConfigCardHeaderFrame;
    QHBoxLayout *horizontalLayout_allTriggerConfigCardHeader;
    QLabel *allTriggerConfigCardTitleLabel;
    QSpacerItem *horizontalSpacer_allTriggerConfigCardHeader;
    QToolButton *allTriggerConfigCardCollapseButton;
    QFrame *allTriggerFormFrame;
    QGridLayout *gridLayout_allTrigger;
    QFrame *allTriggerSegmentFrame;
    QHBoxLayout *horizontalLayout_allTriggerSegmentFrame;
    QPushButton *allInternalTriggerButton;
    QPushButton *allExternalTriggerButton;
    QLabel *runIntervalLabel;
    QLabel *allTriggerModeLabel;
    QSpinBox *runIntervalspinBox;
    QFrame *allBrightnessCard;
    QVBoxLayout *verticalLayout_allBrightnessCard;
    QFrame *allBrightnessCardHeaderFrame;
    QHBoxLayout *horizontalLayout_allBrightnessCardHeader;
    QLabel *allBrightnessCardTitleLabel;
    QSpacerItem *horizontalSpacer_allBrightnessCardHeader;
    QToolButton *allBrightnessCardCollapseButton;
    QFrame *allBrightnessFormFrame;
    QGridLayout *gridLayout_allBrightness;
    QSpinBox *brightnessStandardspinBox;
    QLabel *exposureTimeLabel;
    QLineEdit *gainLineEdit;
    QSpinBox *exposureTimespinBox;
    QPushButton *allAutoAdjustButton;
    QLabel *brightnessStandardLabel;
    QLabel *gainLabel;
    QLabel *allAutoAdjustLabel;
    QSpacerItem *horizontalSpacer;
    QFrame *allFocusCard;
    QVBoxLayout *verticalLayout_allFocusCard;
    QFrame *allFocusCardHeaderFrame;
    QHBoxLayout *horizontalLayout_allFocusCardHeader;
    QLabel *allFocusCardTitleLabel;
    QSpacerItem *horizontalSpacer_allFocusCardHeader;
    QToolButton *allFocusCardCollapseButton;
    QFrame *allFocusFormFrame;
    QGridLayout *gridLayout_allFocus;
    QLabel *allAutoFocusLabel;
    QLabel *focusStepLabel;
    QFrame *allFocusRegionButtonsFrame;
    QHBoxLayout *horizontalLayout_allFocusRegionButtons;
    QToolButton *allFocusRegionAutoButton;
    QToolButton *allFocusRegionRectButton;
    QComboBox *focusModeComboBox;
    QLabel *focusPositionLabel;
    QFrame *allAutoFocusButtonsFrame;
    QHBoxLayout *horizontalLayout_allAutoFocusButtons;
    QPushButton *allAutoFocusButton;
    QToolButton *allAutoFocusResetButton;
    QLabel *allFocusRegionLabel;
    QLabel *focusModeLabel;
    QSpinBox *focusStepspinBox;
    QSpinBox *focusPositionspinBox;
    QFrame *lightControlCard;
    QVBoxLayout *verticalLayout_lightControlCard;
    QFrame *lightControlCardHeaderFrame;
    QHBoxLayout *horizontalLayout_lightControlCardHeader;
    QLabel *lightControlCardTitleLabel;
    QSpacerItem *horizontalSpacer_lightControlCardHeader;
    QToolButton *lightControlCardCollapseButton;
    QFrame *lightControlFormFrame;
    QGridLayout *gridLayout_lightControl;
    QLabel *lightControlLabel;
    QComboBox *lightControlComboBox;
    QFrame *lightDevicePreviewFrame;
    QVBoxLayout *verticalLayout_lightDevicePreview;
    QLabel *lightDeviceIconLabel;
    QLabel *lightPositionLabel;
    QFrame *lightPositionFrame;
    QHBoxLayout *horizontalLayout_lightPosition;
    QToolButton *lightPrevButton;
    QLabel *lightPositionValueLabel;
    QToolButton *lightNextButton;
    QFrame *otherParamsCard;
    QVBoxLayout *verticalLayout_otherParamsCard;
    QFrame *otherParamsCardHeaderFrame;
    QHBoxLayout *horizontalLayout_otherParamsCardHeader;
    QLabel *otherParamsCardTitleLabel;
    QSpacerItem *horizontalSpacer_otherParamsCardHeader;
    QToolButton *otherParamsCardCollapseButton;
    QFrame *otherParamsFormFrame;
    QGridLayout *gridLayout_otherParams;
    QLabel *sharpenLabel;
    QCheckBox *xMirrorCheckBox;
    QLabel *xMirrorLabel;
    QCheckBox *yMirrorCheckBox;
    QLabel *yMirrorLabel;
    QLabel *contrastLabel;
    QFrame *imageSizeSegmentFrame;
    QHBoxLayout *horizontalLayout_imageSizeSegmentFrame;
    QPushButton *imageOriginalButton;
    QPushButton *imageCustomButton;
    QLineEdit *gammaLineEdit;
    QSpinBox *sharpenspinBox;
    QSpinBox *contrastspinBox;
    QLabel *gammaLabel;
    QLabel *grayOutputLabel;
    QCheckBox *grayOutputCheckBox;
    QLabel *encodeQualityLabel;
    QLabel *imageSizeLabel;
    QSpacerItem *horizontalSpacer_2;
    QSpinBox *encodeQualityspinBox;
    QFrame *colorParamsCard;
    QVBoxLayout *verticalLayout_colorParamsCard;
    QFrame *colorParamsCardHeaderFrame;
    QHBoxLayout *horizontalLayout_colorParamsCardHeader;
    QLabel *colorParamsCardTitleLabel;
    QSpacerItem *horizontalSpacer_colorParamsCardHeader;
    QToolButton *colorParamsCardCollapseButton;
    QFrame *colorParamsFormFrame;
    QGridLayout *gridLayout_colorParams;
    QLabel *whiteBalanceLabel;
    QCheckBox *whiteBalanceCheckBox;
    QLabel *colorAutoAdjustLabel;
    QPushButton *colorAutoAdjustButton;
    QLabel *whiteBalanceParamsLabel;
    QPushButton *whiteBalanceEditButton;
    QFrame *colorBalanceTable;
    QGridLayout *gridLayout_colorBalanceTable;
    QLabel *colorBalanceTableCell00;
    QLabel *colorBalanceTableCell01;
    QLabel *colorBalanceTableCell02;
    QLabel *colorBalanceTableCell10;
    QLabel *colorBalanceTableCell11;
    QLabel *colorBalanceTableCell12;
    QLabel *ccmResetLabel;
    QPushButton *ccmResetButton;
    QLabel *ccmParamsLabel;
    QPushButton *ccmEditButton;
    QFrame *ccmTable;
    QGridLayout *gridLayout_ccmTable;
    QLabel *ccmTableCell00;
    QLabel *ccmTableCell01;
    QLabel *ccmTableCell02;
    QLabel *ccmTableCell03;
    QLabel *ccmTableCell10;
    QLabel *ccmTableCell11;
    QLabel *ccmTableCell12;
    QLabel *ccmTableCell13;
    QLabel *ccmTableCell20;
    QLabel *ccmTableCell21;
    QLabel *ccmTableCell22;
    QLabel *ccmTableCell23;
    QLabel *ccmTableCell30;
    QLabel *ccmTableCell31;
    QLabel *ccmTableCell32;
    QLabel *ccmTableCell33;
    QSpacerItem *verticalSpacer_allParams;
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
    QGraphicsView *camera_1;
    QFrame *viewerStatusBar;
    QHBoxLayout *horizontalLayout_viewerStatus;
    QLabel *viewerStatusLabel;
    QSpacerItem *horizontalSpacer_viewerStatus;
    QLabel *viewerCursorLabel;

    void setupUi(QDialog *CameraParamsDialog)
    {
        if (CameraParamsDialog->objectName().isEmpty())
            CameraParamsDialog->setObjectName(QString::fromUtf8("CameraParamsDialog"));
        CameraParamsDialog->resize(1530, 864);
        CameraParamsDialog->setStyleSheet(QString::fromUtf8("QDialog#CameraParamsDialog,\n"
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
        verticalLayout_root = new QVBoxLayout(CameraParamsDialog);
        verticalLayout_root->setSpacing(0);
        verticalLayout_root->setObjectName(QString::fromUtf8("verticalLayout_root"));
        verticalLayout_root->setContentsMargins(0, 0, 0, 0);
        setupTopBar = new QFrame(CameraParamsDialog);
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

        setupToolbar = new QFrame(CameraParamsDialog);
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

        setupBodyFrame = new QFrame(CameraParamsDialog);
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
        cameraStepButton->setChecked(true);
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
        verticalLayout_editor = new QVBoxLayout(setupEditorPanel);
        verticalLayout_editor->setSpacing(14);
        verticalLayout_editor->setObjectName(QString::fromUtf8("verticalLayout_editor"));
        verticalLayout_editor->setContentsMargins(24, 22, 24, 18);
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


        verticalLayout_editor->addWidget(editorHeaderFrame);

        cameraParamsStack = new QStackedWidget(setupEditorPanel);
        cameraParamsStack->setObjectName(QString::fromUtf8("cameraParamsStack"));
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
        basicParamsScrollContent = new QWidget();
        basicParamsScrollContent->setObjectName(QString::fromUtf8("basicParamsScrollContent"));
        basicParamsScrollContent->setGeometry(QRect(0, -16, 552, 613));
        verticalLayout_basicParamsContent = new QVBoxLayout(basicParamsScrollContent);
        verticalLayout_basicParamsContent->setSpacing(10);
        verticalLayout_basicParamsContent->setObjectName(QString::fromUtf8("verticalLayout_basicParamsContent"));
        verticalLayout_basicParamsContent->setContentsMargins(0, 0, 10, 0);
        imagingAdjustCard = new QFrame(basicParamsScrollContent);
        imagingAdjustCard->setObjectName(QString::fromUtf8("imagingAdjustCard"));
        imagingAdjustCard->setFrameShape(QFrame::NoFrame);
        imagingAdjustCard->setProperty("card", QVariant(true));
        verticalLayout_imagingAdjustCard = new QVBoxLayout(imagingAdjustCard);
        verticalLayout_imagingAdjustCard->setSpacing(14);
        verticalLayout_imagingAdjustCard->setObjectName(QString::fromUtf8("verticalLayout_imagingAdjustCard"));
        verticalLayout_imagingAdjustCard->setContentsMargins(18, 18, 18, 18);
        imagingAdjustCardHeaderFrame = new QFrame(imagingAdjustCard);
        imagingAdjustCardHeaderFrame->setObjectName(QString::fromUtf8("imagingAdjustCardHeaderFrame"));
        imagingAdjustCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_imagingAdjustCardHeader = new QHBoxLayout(imagingAdjustCardHeaderFrame);
        horizontalLayout_imagingAdjustCardHeader->setSpacing(0);
        horizontalLayout_imagingAdjustCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_imagingAdjustCardHeader"));
        horizontalLayout_imagingAdjustCardHeader->setContentsMargins(0, 0, 0, 0);
        imagingAdjustCardTitleLabel = new QLabel(imagingAdjustCardHeaderFrame);
        imagingAdjustCardTitleLabel->setObjectName(QString::fromUtf8("imagingAdjustCardTitleLabel"));
        imagingAdjustCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_imagingAdjustCardHeader->addWidget(imagingAdjustCardTitleLabel);

        horizontalSpacer_imagingAdjustCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_imagingAdjustCardHeader->addItem(horizontalSpacer_imagingAdjustCardHeader);

        imagingAdjustCardCollapseButton = new QToolButton(imagingAdjustCardHeaderFrame);
        imagingAdjustCardCollapseButton->setObjectName(QString::fromUtf8("imagingAdjustCardCollapseButton"));
        imagingAdjustCardCollapseButton->setMinimumSize(QSize(28, 28));
        imagingAdjustCardCollapseButton->setAutoRaise(true);

        horizontalLayout_imagingAdjustCardHeader->addWidget(imagingAdjustCardCollapseButton);


        verticalLayout_imagingAdjustCard->addWidget(imagingAdjustCardHeaderFrame);

        imagingAdjustHintLabel = new QLabel(imagingAdjustCard);
        imagingAdjustHintLabel->setObjectName(QString::fromUtf8("imagingAdjustHintLabel"));
        imagingAdjustHintLabel->setProperty("hint", QVariant(true));

        verticalLayout_imagingAdjustCard->addWidget(imagingAdjustHintLabel);

        oneKeyTuneButton = new QPushButton(imagingAdjustCard);
        oneKeyTuneButton->setObjectName(QString::fromUtf8("oneKeyTuneButton"));
        oneKeyTuneButton->setMinimumSize(QSize(0, 46));
        oneKeyTuneButton->setProperty("orangeButton", QVariant(true));

        verticalLayout_imagingAdjustCard->addWidget(oneKeyTuneButton);


        verticalLayout_basicParamsContent->addWidget(imagingAdjustCard);

        triggerConfigCard = new QFrame(basicParamsScrollContent);
        triggerConfigCard->setObjectName(QString::fromUtf8("triggerConfigCard"));
        triggerConfigCard->setFrameShape(QFrame::NoFrame);
        triggerConfigCard->setProperty("card", QVariant(true));
        verticalLayout_triggerConfigCard = new QVBoxLayout(triggerConfigCard);
        verticalLayout_triggerConfigCard->setSpacing(14);
        verticalLayout_triggerConfigCard->setObjectName(QString::fromUtf8("verticalLayout_triggerConfigCard"));
        verticalLayout_triggerConfigCard->setContentsMargins(18, 18, 18, 18);
        triggerConfigCardHeaderFrame = new QFrame(triggerConfigCard);
        triggerConfigCardHeaderFrame->setObjectName(QString::fromUtf8("triggerConfigCardHeaderFrame"));
        triggerConfigCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_triggerConfigCardHeader = new QHBoxLayout(triggerConfigCardHeaderFrame);
        horizontalLayout_triggerConfigCardHeader->setSpacing(0);
        horizontalLayout_triggerConfigCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_triggerConfigCardHeader"));
        horizontalLayout_triggerConfigCardHeader->setContentsMargins(0, 0, 0, 0);
        triggerConfigCardTitleLabel = new QLabel(triggerConfigCardHeaderFrame);
        triggerConfigCardTitleLabel->setObjectName(QString::fromUtf8("triggerConfigCardTitleLabel"));
        triggerConfigCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_triggerConfigCardHeader->addWidget(triggerConfigCardTitleLabel);

        horizontalSpacer_triggerConfigCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_triggerConfigCardHeader->addItem(horizontalSpacer_triggerConfigCardHeader);

        triggerConfigCardCollapseButton = new QToolButton(triggerConfigCardHeaderFrame);
        triggerConfigCardCollapseButton->setObjectName(QString::fromUtf8("triggerConfigCardCollapseButton"));
        triggerConfigCardCollapseButton->setMinimumSize(QSize(28, 28));
        triggerConfigCardCollapseButton->setAutoRaise(true);

        horizontalLayout_triggerConfigCardHeader->addWidget(triggerConfigCardCollapseButton);


        verticalLayout_triggerConfigCard->addWidget(triggerConfigCardHeaderFrame);

        basicTriggerFormFrame = new QFrame(triggerConfigCard);
        basicTriggerFormFrame->setObjectName(QString::fromUtf8("basicTriggerFormFrame"));
        basicTriggerFormFrame->setFrameShape(QFrame::NoFrame);
        gridLayout_basicTrigger = new QGridLayout(basicTriggerFormFrame);
        gridLayout_basicTrigger->setObjectName(QString::fromUtf8("gridLayout_basicTrigger"));
        gridLayout_basicTrigger->setHorizontalSpacing(18);
        gridLayout_basicTrigger->setVerticalSpacing(12);
        gridLayout_basicTrigger->setContentsMargins(0, 0, 0, 0);
        triggerModeLabel = new QLabel(basicTriggerFormFrame);
        triggerModeLabel->setObjectName(QString::fromUtf8("triggerModeLabel"));
        triggerModeLabel->setProperty("formLabel", QVariant(true));

        gridLayout_basicTrigger->addWidget(triggerModeLabel, 0, 0, 1, 1);

        triggerSegmentFrame = new QFrame(basicTriggerFormFrame);
        triggerSegmentFrame->setObjectName(QString::fromUtf8("triggerSegmentFrame"));
        triggerSegmentFrame->setMinimumSize(QSize(212, 34));
        triggerSegmentFrame->setMaximumSize(QSize(16777215, 34));
        triggerSegmentFrame->setFrameShape(QFrame::NoFrame);
        triggerSegmentFrame->setProperty("segmented", QVariant(true));
        horizontalLayout_triggerSegmentFrame = new QHBoxLayout(triggerSegmentFrame);
        horizontalLayout_triggerSegmentFrame->setSpacing(0);
        horizontalLayout_triggerSegmentFrame->setObjectName(QString::fromUtf8("horizontalLayout_triggerSegmentFrame"));
        horizontalLayout_triggerSegmentFrame->setContentsMargins(0, 0, 0, 0);
        internalTriggerButton = new QPushButton(triggerSegmentFrame);
        internalTriggerButton->setObjectName(QString::fromUtf8("internalTriggerButton"));
        internalTriggerButton->setMinimumSize(QSize(106, 38));
        internalTriggerButton->setCheckable(true);
        internalTriggerButton->setChecked(true);

        horizontalLayout_triggerSegmentFrame->addWidget(internalTriggerButton);

        externalTriggerButton = new QPushButton(triggerSegmentFrame);
        externalTriggerButton->setObjectName(QString::fromUtf8("externalTriggerButton"));
        externalTriggerButton->setMinimumSize(QSize(106, 38));
        externalTriggerButton->setCheckable(true);

        horizontalLayout_triggerSegmentFrame->addWidget(externalTriggerButton);


        gridLayout_basicTrigger->addWidget(triggerSegmentFrame, 0, 1, 1, 1);


        verticalLayout_triggerConfigCard->addWidget(basicTriggerFormFrame);


        verticalLayout_basicParamsContent->addWidget(triggerConfigCard);

        brightnessCard = new QFrame(basicParamsScrollContent);
        brightnessCard->setObjectName(QString::fromUtf8("brightnessCard"));
        brightnessCard->setFrameShape(QFrame::NoFrame);
        brightnessCard->setProperty("card", QVariant(true));
        verticalLayout_brightnessCard = new QVBoxLayout(brightnessCard);
        verticalLayout_brightnessCard->setSpacing(14);
        verticalLayout_brightnessCard->setObjectName(QString::fromUtf8("verticalLayout_brightnessCard"));
        verticalLayout_brightnessCard->setContentsMargins(18, 18, 18, 18);
        brightnessCardHeaderFrame = new QFrame(brightnessCard);
        brightnessCardHeaderFrame->setObjectName(QString::fromUtf8("brightnessCardHeaderFrame"));
        brightnessCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_brightnessCardHeader = new QHBoxLayout(brightnessCardHeaderFrame);
        horizontalLayout_brightnessCardHeader->setSpacing(0);
        horizontalLayout_brightnessCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_brightnessCardHeader"));
        horizontalLayout_brightnessCardHeader->setContentsMargins(0, 0, 0, 0);
        brightnessCardTitleLabel = new QLabel(brightnessCardHeaderFrame);
        brightnessCardTitleLabel->setObjectName(QString::fromUtf8("brightnessCardTitleLabel"));
        brightnessCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_brightnessCardHeader->addWidget(brightnessCardTitleLabel);

        horizontalSpacer_brightnessCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_brightnessCardHeader->addItem(horizontalSpacer_brightnessCardHeader);

        brightnessCardCollapseButton = new QToolButton(brightnessCardHeaderFrame);
        brightnessCardCollapseButton->setObjectName(QString::fromUtf8("brightnessCardCollapseButton"));
        brightnessCardCollapseButton->setMinimumSize(QSize(28, 28));
        brightnessCardCollapseButton->setAutoRaise(true);

        horizontalLayout_brightnessCardHeader->addWidget(brightnessCardCollapseButton);


        verticalLayout_brightnessCard->addWidget(brightnessCardHeaderFrame);

        basicBrightnessFormFrame = new QFrame(brightnessCard);
        basicBrightnessFormFrame->setObjectName(QString::fromUtf8("basicBrightnessFormFrame"));
        basicBrightnessFormFrame->setFrameShape(QFrame::NoFrame);
        gridLayout_basicBrightness = new QGridLayout(basicBrightnessFormFrame);
        gridLayout_basicBrightness->setObjectName(QString::fromUtf8("gridLayout_basicBrightness"));
        gridLayout_basicBrightness->setHorizontalSpacing(18);
        gridLayout_basicBrightness->setVerticalSpacing(12);
        gridLayout_basicBrightness->setContentsMargins(0, 0, 0, 0);
        autoAdjustLabel = new QLabel(basicBrightnessFormFrame);
        autoAdjustLabel->setObjectName(QString::fromUtf8("autoAdjustLabel"));
        autoAdjustLabel->setProperty("formLabel", QVariant(true));

        gridLayout_basicBrightness->addWidget(autoAdjustLabel, 0, 0, 1, 1);

        autoAdjustButton = new QPushButton(basicBrightnessFormFrame);
        autoAdjustButton->setObjectName(QString::fromUtf8("autoAdjustButton"));
        autoAdjustButton->setMinimumSize(QSize(260, 44));
        autoAdjustButton->setProperty("softButton", QVariant(true));

        gridLayout_basicBrightness->addWidget(autoAdjustButton, 0, 1, 1, 1);


        verticalLayout_brightnessCard->addWidget(basicBrightnessFormFrame);


        verticalLayout_basicParamsContent->addWidget(brightnessCard);

        focusCard = new QFrame(basicParamsScrollContent);
        focusCard->setObjectName(QString::fromUtf8("focusCard"));
        focusCard->setFrameShape(QFrame::NoFrame);
        focusCard->setProperty("card", QVariant(true));
        verticalLayout_focusCard = new QVBoxLayout(focusCard);
        verticalLayout_focusCard->setSpacing(14);
        verticalLayout_focusCard->setObjectName(QString::fromUtf8("verticalLayout_focusCard"));
        verticalLayout_focusCard->setContentsMargins(18, 18, 18, 18);
        focusCardHeaderFrame = new QFrame(focusCard);
        focusCardHeaderFrame->setObjectName(QString::fromUtf8("focusCardHeaderFrame"));
        focusCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_focusCardHeader = new QHBoxLayout(focusCardHeaderFrame);
        horizontalLayout_focusCardHeader->setSpacing(0);
        horizontalLayout_focusCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_focusCardHeader"));
        horizontalLayout_focusCardHeader->setContentsMargins(0, 0, 0, 0);
        focusCardTitleLabel = new QLabel(focusCardHeaderFrame);
        focusCardTitleLabel->setObjectName(QString::fromUtf8("focusCardTitleLabel"));
        focusCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_focusCardHeader->addWidget(focusCardTitleLabel);

        horizontalSpacer_focusCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_focusCardHeader->addItem(horizontalSpacer_focusCardHeader);

        focusCardCollapseButton = new QToolButton(focusCardHeaderFrame);
        focusCardCollapseButton->setObjectName(QString::fromUtf8("focusCardCollapseButton"));
        focusCardCollapseButton->setMinimumSize(QSize(28, 28));
        focusCardCollapseButton->setAutoRaise(true);

        horizontalLayout_focusCardHeader->addWidget(focusCardCollapseButton);


        verticalLayout_focusCard->addWidget(focusCardHeaderFrame);

        basicFocusFormFrame = new QFrame(focusCard);
        basicFocusFormFrame->setObjectName(QString::fromUtf8("basicFocusFormFrame"));
        basicFocusFormFrame->setFrameShape(QFrame::NoFrame);
        gridLayout_basicFocus = new QGridLayout(basicFocusFormFrame);
        gridLayout_basicFocus->setObjectName(QString::fromUtf8("gridLayout_basicFocus"));
        gridLayout_basicFocus->setHorizontalSpacing(18);
        gridLayout_basicFocus->setVerticalSpacing(12);
        gridLayout_basicFocus->setContentsMargins(0, 0, 0, 0);
        focusRegionLabel = new QLabel(basicFocusFormFrame);
        focusRegionLabel->setObjectName(QString::fromUtf8("focusRegionLabel"));
        focusRegionLabel->setProperty("formLabel", QVariant(true));

        gridLayout_basicFocus->addWidget(focusRegionLabel, 0, 0, 1, 1);

        focusRegionButtonsFrame = new QFrame(basicFocusFormFrame);
        focusRegionButtonsFrame->setObjectName(QString::fromUtf8("focusRegionButtonsFrame"));
        focusRegionButtonsFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_focusRegionButtons = new QHBoxLayout(focusRegionButtonsFrame);
        horizontalLayout_focusRegionButtons->setSpacing(10);
        horizontalLayout_focusRegionButtons->setObjectName(QString::fromUtf8("horizontalLayout_focusRegionButtons"));
        horizontalLayout_focusRegionButtons->setContentsMargins(0, 0, 0, 0);
        focusRegionAutoButton = new QToolButton(focusRegionButtonsFrame);
        focusRegionAutoButton->setObjectName(QString::fromUtf8("focusRegionAutoButton"));
        focusRegionAutoButton->setMinimumSize(QSize(56, 38));

        horizontalLayout_focusRegionButtons->addWidget(focusRegionAutoButton);

        focusRegionRectButton = new QToolButton(focusRegionButtonsFrame);
        focusRegionRectButton->setObjectName(QString::fromUtf8("focusRegionRectButton"));
        focusRegionRectButton->setMinimumSize(QSize(56, 38));

        horizontalLayout_focusRegionButtons->addWidget(focusRegionRectButton);


        gridLayout_basicFocus->addWidget(focusRegionButtonsFrame, 0, 1, 1, 1);

        autoFocusLabel = new QLabel(basicFocusFormFrame);
        autoFocusLabel->setObjectName(QString::fromUtf8("autoFocusLabel"));
        autoFocusLabel->setProperty("formLabel", QVariant(true));

        gridLayout_basicFocus->addWidget(autoFocusLabel, 1, 0, 1, 1);

        autoFocusButtonsFrame = new QFrame(basicFocusFormFrame);
        autoFocusButtonsFrame->setObjectName(QString::fromUtf8("autoFocusButtonsFrame"));
        autoFocusButtonsFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_autoFocusButtons = new QHBoxLayout(autoFocusButtonsFrame);
        horizontalLayout_autoFocusButtons->setSpacing(10);
        horizontalLayout_autoFocusButtons->setObjectName(QString::fromUtf8("horizontalLayout_autoFocusButtons"));
        horizontalLayout_autoFocusButtons->setContentsMargins(0, 0, 0, 0);
        autoFocusButton = new QPushButton(autoFocusButtonsFrame);
        autoFocusButton->setObjectName(QString::fromUtf8("autoFocusButton"));
        autoFocusButton->setMinimumSize(QSize(202, 44));
        autoFocusButton->setProperty("softButton", QVariant(true));

        horizontalLayout_autoFocusButtons->addWidget(autoFocusButton);

        autoFocusResetButton = new QToolButton(autoFocusButtonsFrame);
        autoFocusResetButton->setObjectName(QString::fromUtf8("autoFocusResetButton"));
        autoFocusResetButton->setMinimumSize(QSize(56, 40));

        horizontalLayout_autoFocusButtons->addWidget(autoFocusResetButton);


        gridLayout_basicFocus->addWidget(autoFocusButtonsFrame, 1, 1, 1, 1);


        verticalLayout_focusCard->addWidget(basicFocusFormFrame);


        verticalLayout_basicParamsContent->addWidget(focusCard);

        verticalSpacer_basicParams = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_basicParamsContent->addItem(verticalSpacer_basicParams);

        basicParamsScrollArea->setWidget(basicParamsScrollContent);

        verticalLayout_basicParamsPage->addWidget(basicParamsScrollArea);

        cameraParamsStack->addWidget(basicParamsPage);
        allParamsPage = new QWidget();
        allParamsPage->setObjectName(QString::fromUtf8("allParamsPage"));
        verticalLayout_allParamsPage = new QVBoxLayout(allParamsPage);
        verticalLayout_allParamsPage->setSpacing(0);
        verticalLayout_allParamsPage->setObjectName(QString::fromUtf8("verticalLayout_allParamsPage"));
        verticalLayout_allParamsPage->setContentsMargins(0, 0, 0, 0);
        allParamsScrollArea = new QScrollArea(allParamsPage);
        allParamsScrollArea->setObjectName(QString::fromUtf8("allParamsScrollArea"));
        allParamsScrollArea->setFrameShape(QFrame::NoFrame);
        allParamsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        allParamsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        allParamsScrollArea->setWidgetResizable(true);
        allParamsScrollContent = new QWidget();
        allParamsScrollContent->setObjectName(QString::fromUtf8("allParamsScrollContent"));
        allParamsScrollContent->setGeometry(QRect(0, -937, 552, 2413));
        verticalLayout_allParamsContent = new QVBoxLayout(allParamsScrollContent);
        verticalLayout_allParamsContent->setSpacing(10);
        verticalLayout_allParamsContent->setObjectName(QString::fromUtf8("verticalLayout_allParamsContent"));
        verticalLayout_allParamsContent->setContentsMargins(0, 0, 10, 0);
        allImagingAdjustCard = new QFrame(allParamsScrollContent);
        allImagingAdjustCard->setObjectName(QString::fromUtf8("allImagingAdjustCard"));
        allImagingAdjustCard->setFrameShape(QFrame::NoFrame);
        allImagingAdjustCard->setProperty("card", QVariant(true));
        verticalLayout_allImagingAdjustCard = new QVBoxLayout(allImagingAdjustCard);
        verticalLayout_allImagingAdjustCard->setSpacing(14);
        verticalLayout_allImagingAdjustCard->setObjectName(QString::fromUtf8("verticalLayout_allImagingAdjustCard"));
        verticalLayout_allImagingAdjustCard->setContentsMargins(18, 18, 18, 18);
        allImagingAdjustCardHeaderFrame = new QFrame(allImagingAdjustCard);
        allImagingAdjustCardHeaderFrame->setObjectName(QString::fromUtf8("allImagingAdjustCardHeaderFrame"));
        allImagingAdjustCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_allImagingAdjustCardHeader = new QHBoxLayout(allImagingAdjustCardHeaderFrame);
        horizontalLayout_allImagingAdjustCardHeader->setSpacing(0);
        horizontalLayout_allImagingAdjustCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_allImagingAdjustCardHeader"));
        horizontalLayout_allImagingAdjustCardHeader->setContentsMargins(0, 0, 0, 0);
        allImagingAdjustCardTitleLabel = new QLabel(allImagingAdjustCardHeaderFrame);
        allImagingAdjustCardTitleLabel->setObjectName(QString::fromUtf8("allImagingAdjustCardTitleLabel"));
        allImagingAdjustCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_allImagingAdjustCardHeader->addWidget(allImagingAdjustCardTitleLabel);

        horizontalSpacer_allImagingAdjustCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allImagingAdjustCardHeader->addItem(horizontalSpacer_allImagingAdjustCardHeader);

        allImagingAdjustCardCollapseButton = new QToolButton(allImagingAdjustCardHeaderFrame);
        allImagingAdjustCardCollapseButton->setObjectName(QString::fromUtf8("allImagingAdjustCardCollapseButton"));
        allImagingAdjustCardCollapseButton->setMinimumSize(QSize(28, 28));
        allImagingAdjustCardCollapseButton->setAutoRaise(true);

        horizontalLayout_allImagingAdjustCardHeader->addWidget(allImagingAdjustCardCollapseButton);


        verticalLayout_allImagingAdjustCard->addWidget(allImagingAdjustCardHeaderFrame);

        allImagingAdjustHintLabel = new QLabel(allImagingAdjustCard);
        allImagingAdjustHintLabel->setObjectName(QString::fromUtf8("allImagingAdjustHintLabel"));
        allImagingAdjustHintLabel->setProperty("hint", QVariant(true));

        verticalLayout_allImagingAdjustCard->addWidget(allImagingAdjustHintLabel);

        allOneKeyTuneButton = new QPushButton(allImagingAdjustCard);
        allOneKeyTuneButton->setObjectName(QString::fromUtf8("allOneKeyTuneButton"));
        allOneKeyTuneButton->setMinimumSize(QSize(0, 46));
        allOneKeyTuneButton->setProperty("orangeButton", QVariant(true));

        verticalLayout_allImagingAdjustCard->addWidget(allOneKeyTuneButton);


        verticalLayout_allParamsContent->addWidget(allImagingAdjustCard);

        allTriggerConfigCard = new QFrame(allParamsScrollContent);
        allTriggerConfigCard->setObjectName(QString::fromUtf8("allTriggerConfigCard"));
        allTriggerConfigCard->setFrameShape(QFrame::NoFrame);
        allTriggerConfigCard->setProperty("card", QVariant(true));
        verticalLayout_allTriggerConfigCard = new QVBoxLayout(allTriggerConfigCard);
        verticalLayout_allTriggerConfigCard->setSpacing(14);
        verticalLayout_allTriggerConfigCard->setObjectName(QString::fromUtf8("verticalLayout_allTriggerConfigCard"));
        verticalLayout_allTriggerConfigCard->setContentsMargins(18, 18, 18, 18);
        allTriggerConfigCardHeaderFrame = new QFrame(allTriggerConfigCard);
        allTriggerConfigCardHeaderFrame->setObjectName(QString::fromUtf8("allTriggerConfigCardHeaderFrame"));
        allTriggerConfigCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_allTriggerConfigCardHeader = new QHBoxLayout(allTriggerConfigCardHeaderFrame);
        horizontalLayout_allTriggerConfigCardHeader->setSpacing(0);
        horizontalLayout_allTriggerConfigCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_allTriggerConfigCardHeader"));
        horizontalLayout_allTriggerConfigCardHeader->setContentsMargins(0, 0, 0, 0);
        allTriggerConfigCardTitleLabel = new QLabel(allTriggerConfigCardHeaderFrame);
        allTriggerConfigCardTitleLabel->setObjectName(QString::fromUtf8("allTriggerConfigCardTitleLabel"));
        allTriggerConfigCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_allTriggerConfigCardHeader->addWidget(allTriggerConfigCardTitleLabel);

        horizontalSpacer_allTriggerConfigCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allTriggerConfigCardHeader->addItem(horizontalSpacer_allTriggerConfigCardHeader);

        allTriggerConfigCardCollapseButton = new QToolButton(allTriggerConfigCardHeaderFrame);
        allTriggerConfigCardCollapseButton->setObjectName(QString::fromUtf8("allTriggerConfigCardCollapseButton"));
        allTriggerConfigCardCollapseButton->setMinimumSize(QSize(28, 28));
        allTriggerConfigCardCollapseButton->setAutoRaise(true);

        horizontalLayout_allTriggerConfigCardHeader->addWidget(allTriggerConfigCardCollapseButton);


        verticalLayout_allTriggerConfigCard->addWidget(allTriggerConfigCardHeaderFrame);

        allTriggerFormFrame = new QFrame(allTriggerConfigCard);
        allTriggerFormFrame->setObjectName(QString::fromUtf8("allTriggerFormFrame"));
        allTriggerFormFrame->setFrameShape(QFrame::NoFrame);
        gridLayout_allTrigger = new QGridLayout(allTriggerFormFrame);
        gridLayout_allTrigger->setObjectName(QString::fromUtf8("gridLayout_allTrigger"));
        gridLayout_allTrigger->setHorizontalSpacing(18);
        gridLayout_allTrigger->setVerticalSpacing(12);
        gridLayout_allTrigger->setContentsMargins(0, 0, 0, 0);
        allTriggerSegmentFrame = new QFrame(allTriggerFormFrame);
        allTriggerSegmentFrame->setObjectName(QString::fromUtf8("allTriggerSegmentFrame"));
        allTriggerSegmentFrame->setMinimumSize(QSize(212, 34));
        allTriggerSegmentFrame->setMaximumSize(QSize(16777215, 34));
        allTriggerSegmentFrame->setFrameShape(QFrame::NoFrame);
        allTriggerSegmentFrame->setProperty("segmented", QVariant(true));
        horizontalLayout_allTriggerSegmentFrame = new QHBoxLayout(allTriggerSegmentFrame);
        horizontalLayout_allTriggerSegmentFrame->setSpacing(0);
        horizontalLayout_allTriggerSegmentFrame->setObjectName(QString::fromUtf8("horizontalLayout_allTriggerSegmentFrame"));
        horizontalLayout_allTriggerSegmentFrame->setContentsMargins(0, 0, 0, 0);
        allInternalTriggerButton = new QPushButton(allTriggerSegmentFrame);
        allInternalTriggerButton->setObjectName(QString::fromUtf8("allInternalTriggerButton"));
        allInternalTriggerButton->setMinimumSize(QSize(106, 38));
        allInternalTriggerButton->setCheckable(true);
        allInternalTriggerButton->setChecked(true);

        horizontalLayout_allTriggerSegmentFrame->addWidget(allInternalTriggerButton);

        allExternalTriggerButton = new QPushButton(allTriggerSegmentFrame);
        allExternalTriggerButton->setObjectName(QString::fromUtf8("allExternalTriggerButton"));
        allExternalTriggerButton->setMinimumSize(QSize(106, 38));
        allExternalTriggerButton->setCheckable(true);

        horizontalLayout_allTriggerSegmentFrame->addWidget(allExternalTriggerButton);


        gridLayout_allTrigger->addWidget(allTriggerSegmentFrame, 0, 1, 1, 1);

        runIntervalLabel = new QLabel(allTriggerFormFrame);
        runIntervalLabel->setObjectName(QString::fromUtf8("runIntervalLabel"));
        runIntervalLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allTrigger->addWidget(runIntervalLabel, 1, 0, 1, 1);

        allTriggerModeLabel = new QLabel(allTriggerFormFrame);
        allTriggerModeLabel->setObjectName(QString::fromUtf8("allTriggerModeLabel"));
        allTriggerModeLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allTrigger->addWidget(allTriggerModeLabel, 0, 0, 1, 1);

        runIntervalspinBox = new QSpinBox(allTriggerFormFrame);
        runIntervalspinBox->setObjectName(QString::fromUtf8("runIntervalspinBox"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(runIntervalspinBox->sizePolicy().hasHeightForWidth());
        runIntervalspinBox->setSizePolicy(sizePolicy);
        runIntervalspinBox->setMinimumSize(QSize(260, 40));
        runIntervalspinBox->setValue(50);

        gridLayout_allTrigger->addWidget(runIntervalspinBox, 1, 1, 1, 1);


        verticalLayout_allTriggerConfigCard->addWidget(allTriggerFormFrame);


        verticalLayout_allParamsContent->addWidget(allTriggerConfigCard);

        allBrightnessCard = new QFrame(allParamsScrollContent);
        allBrightnessCard->setObjectName(QString::fromUtf8("allBrightnessCard"));
        allBrightnessCard->setFrameShape(QFrame::NoFrame);
        allBrightnessCard->setProperty("card", QVariant(true));
        verticalLayout_allBrightnessCard = new QVBoxLayout(allBrightnessCard);
        verticalLayout_allBrightnessCard->setSpacing(14);
        verticalLayout_allBrightnessCard->setObjectName(QString::fromUtf8("verticalLayout_allBrightnessCard"));
        verticalLayout_allBrightnessCard->setContentsMargins(18, 18, 18, 18);
        allBrightnessCardHeaderFrame = new QFrame(allBrightnessCard);
        allBrightnessCardHeaderFrame->setObjectName(QString::fromUtf8("allBrightnessCardHeaderFrame"));
        allBrightnessCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_allBrightnessCardHeader = new QHBoxLayout(allBrightnessCardHeaderFrame);
        horizontalLayout_allBrightnessCardHeader->setSpacing(0);
        horizontalLayout_allBrightnessCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_allBrightnessCardHeader"));
        horizontalLayout_allBrightnessCardHeader->setContentsMargins(0, 0, 0, 0);
        allBrightnessCardTitleLabel = new QLabel(allBrightnessCardHeaderFrame);
        allBrightnessCardTitleLabel->setObjectName(QString::fromUtf8("allBrightnessCardTitleLabel"));
        allBrightnessCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_allBrightnessCardHeader->addWidget(allBrightnessCardTitleLabel);

        horizontalSpacer_allBrightnessCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allBrightnessCardHeader->addItem(horizontalSpacer_allBrightnessCardHeader);

        allBrightnessCardCollapseButton = new QToolButton(allBrightnessCardHeaderFrame);
        allBrightnessCardCollapseButton->setObjectName(QString::fromUtf8("allBrightnessCardCollapseButton"));
        allBrightnessCardCollapseButton->setMinimumSize(QSize(28, 28));
        allBrightnessCardCollapseButton->setAutoRaise(true);

        horizontalLayout_allBrightnessCardHeader->addWidget(allBrightnessCardCollapseButton);


        verticalLayout_allBrightnessCard->addWidget(allBrightnessCardHeaderFrame);

        allBrightnessFormFrame = new QFrame(allBrightnessCard);
        allBrightnessFormFrame->setObjectName(QString::fromUtf8("allBrightnessFormFrame"));
        allBrightnessFormFrame->setFrameShape(QFrame::NoFrame);
        gridLayout_allBrightness = new QGridLayout(allBrightnessFormFrame);
        gridLayout_allBrightness->setObjectName(QString::fromUtf8("gridLayout_allBrightness"));
        gridLayout_allBrightness->setHorizontalSpacing(18);
        gridLayout_allBrightness->setVerticalSpacing(12);
        gridLayout_allBrightness->setContentsMargins(0, 0, 0, 0);
        brightnessStandardspinBox = new QSpinBox(allBrightnessFormFrame);
        brightnessStandardspinBox->setObjectName(QString::fromUtf8("brightnessStandardspinBox"));
        sizePolicy.setHeightForWidth(brightnessStandardspinBox->sizePolicy().hasHeightForWidth());
        brightnessStandardspinBox->setSizePolicy(sizePolicy);
        brightnessStandardspinBox->setMinimumSize(QSize(260, 40));
        brightnessStandardspinBox->setValue(1);

        gridLayout_allBrightness->addWidget(brightnessStandardspinBox, 0, 2, 1, 1);

        exposureTimeLabel = new QLabel(allBrightnessFormFrame);
        exposureTimeLabel->setObjectName(QString::fromUtf8("exposureTimeLabel"));
        exposureTimeLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allBrightness->addWidget(exposureTimeLabel, 2, 0, 1, 1);

        gainLineEdit = new QLineEdit(allBrightnessFormFrame);
        gainLineEdit->setObjectName(QString::fromUtf8("gainLineEdit"));
        gainLineEdit->setMinimumSize(QSize(260, 40));

        gridLayout_allBrightness->addWidget(gainLineEdit, 3, 2, 1, 1);

        exposureTimespinBox = new QSpinBox(allBrightnessFormFrame);
        exposureTimespinBox->setObjectName(QString::fromUtf8("exposureTimespinBox"));
        sizePolicy.setHeightForWidth(exposureTimespinBox->sizePolicy().hasHeightForWidth());
        exposureTimespinBox->setSizePolicy(sizePolicy);
        exposureTimespinBox->setMinimumSize(QSize(260, 40));
        exposureTimespinBox->setMaximum(5000);
        exposureTimespinBox->setValue(1620);

        gridLayout_allBrightness->addWidget(exposureTimespinBox, 2, 2, 1, 1);

        allAutoAdjustButton = new QPushButton(allBrightnessFormFrame);
        allAutoAdjustButton->setObjectName(QString::fromUtf8("allAutoAdjustButton"));
        allAutoAdjustButton->setMinimumSize(QSize(260, 44));
        allAutoAdjustButton->setProperty("softButton", QVariant(true));

        gridLayout_allBrightness->addWidget(allAutoAdjustButton, 1, 2, 1, 1);

        brightnessStandardLabel = new QLabel(allBrightnessFormFrame);
        brightnessStandardLabel->setObjectName(QString::fromUtf8("brightnessStandardLabel"));
        brightnessStandardLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allBrightness->addWidget(brightnessStandardLabel, 0, 0, 1, 1);

        gainLabel = new QLabel(allBrightnessFormFrame);
        gainLabel->setObjectName(QString::fromUtf8("gainLabel"));
        gainLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allBrightness->addWidget(gainLabel, 3, 0, 1, 1);

        allAutoAdjustLabel = new QLabel(allBrightnessFormFrame);
        allAutoAdjustLabel->setObjectName(QString::fromUtf8("allAutoAdjustLabel"));
        allAutoAdjustLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allBrightness->addWidget(allAutoAdjustLabel, 1, 0, 1, 1);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_allBrightness->addItem(horizontalSpacer, 0, 1, 1, 1);


        verticalLayout_allBrightnessCard->addWidget(allBrightnessFormFrame);


        verticalLayout_allParamsContent->addWidget(allBrightnessCard);

        allFocusCard = new QFrame(allParamsScrollContent);
        allFocusCard->setObjectName(QString::fromUtf8("allFocusCard"));
        allFocusCard->setFrameShape(QFrame::NoFrame);
        allFocusCard->setProperty("card", QVariant(true));
        verticalLayout_allFocusCard = new QVBoxLayout(allFocusCard);
        verticalLayout_allFocusCard->setSpacing(14);
        verticalLayout_allFocusCard->setObjectName(QString::fromUtf8("verticalLayout_allFocusCard"));
        verticalLayout_allFocusCard->setContentsMargins(18, 18, 18, 18);
        allFocusCardHeaderFrame = new QFrame(allFocusCard);
        allFocusCardHeaderFrame->setObjectName(QString::fromUtf8("allFocusCardHeaderFrame"));
        allFocusCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_allFocusCardHeader = new QHBoxLayout(allFocusCardHeaderFrame);
        horizontalLayout_allFocusCardHeader->setSpacing(0);
        horizontalLayout_allFocusCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_allFocusCardHeader"));
        horizontalLayout_allFocusCardHeader->setContentsMargins(0, 0, 0, 0);
        allFocusCardTitleLabel = new QLabel(allFocusCardHeaderFrame);
        allFocusCardTitleLabel->setObjectName(QString::fromUtf8("allFocusCardTitleLabel"));
        allFocusCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_allFocusCardHeader->addWidget(allFocusCardTitleLabel);

        horizontalSpacer_allFocusCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_allFocusCardHeader->addItem(horizontalSpacer_allFocusCardHeader);

        allFocusCardCollapseButton = new QToolButton(allFocusCardHeaderFrame);
        allFocusCardCollapseButton->setObjectName(QString::fromUtf8("allFocusCardCollapseButton"));
        allFocusCardCollapseButton->setMinimumSize(QSize(28, 28));
        allFocusCardCollapseButton->setAutoRaise(true);

        horizontalLayout_allFocusCardHeader->addWidget(allFocusCardCollapseButton);


        verticalLayout_allFocusCard->addWidget(allFocusCardHeaderFrame);

        allFocusFormFrame = new QFrame(allFocusCard);
        allFocusFormFrame->setObjectName(QString::fromUtf8("allFocusFormFrame"));
        allFocusFormFrame->setFrameShape(QFrame::NoFrame);
        gridLayout_allFocus = new QGridLayout(allFocusFormFrame);
        gridLayout_allFocus->setObjectName(QString::fromUtf8("gridLayout_allFocus"));
        gridLayout_allFocus->setHorizontalSpacing(18);
        gridLayout_allFocus->setVerticalSpacing(12);
        gridLayout_allFocus->setContentsMargins(0, 0, 0, 0);
        allAutoFocusLabel = new QLabel(allFocusFormFrame);
        allAutoFocusLabel->setObjectName(QString::fromUtf8("allAutoFocusLabel"));
        allAutoFocusLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allFocus->addWidget(allAutoFocusLabel, 4, 0, 1, 1);

        focusStepLabel = new QLabel(allFocusFormFrame);
        focusStepLabel->setObjectName(QString::fromUtf8("focusStepLabel"));
        focusStepLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allFocus->addWidget(focusStepLabel, 1, 0, 1, 1);

        allFocusRegionButtonsFrame = new QFrame(allFocusFormFrame);
        allFocusRegionButtonsFrame->setObjectName(QString::fromUtf8("allFocusRegionButtonsFrame"));
        allFocusRegionButtonsFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_allFocusRegionButtons = new QHBoxLayout(allFocusRegionButtonsFrame);
        horizontalLayout_allFocusRegionButtons->setSpacing(10);
        horizontalLayout_allFocusRegionButtons->setObjectName(QString::fromUtf8("horizontalLayout_allFocusRegionButtons"));
        horizontalLayout_allFocusRegionButtons->setContentsMargins(0, 0, 0, 0);
        allFocusRegionAutoButton = new QToolButton(allFocusRegionButtonsFrame);
        allFocusRegionAutoButton->setObjectName(QString::fromUtf8("allFocusRegionAutoButton"));
        allFocusRegionAutoButton->setMinimumSize(QSize(56, 38));

        horizontalLayout_allFocusRegionButtons->addWidget(allFocusRegionAutoButton);

        allFocusRegionRectButton = new QToolButton(allFocusRegionButtonsFrame);
        allFocusRegionRectButton->setObjectName(QString::fromUtf8("allFocusRegionRectButton"));
        allFocusRegionRectButton->setMinimumSize(QSize(56, 38));

        horizontalLayout_allFocusRegionButtons->addWidget(allFocusRegionRectButton);


        gridLayout_allFocus->addWidget(allFocusRegionButtonsFrame, 3, 1, 1, 1);

        focusModeComboBox = new QComboBox(allFocusFormFrame);
        focusModeComboBox->addItem(QString());
        focusModeComboBox->setObjectName(QString::fromUtf8("focusModeComboBox"));
        focusModeComboBox->setMinimumSize(QSize(260, 40));

        gridLayout_allFocus->addWidget(focusModeComboBox, 0, 1, 1, 1);

        focusPositionLabel = new QLabel(allFocusFormFrame);
        focusPositionLabel->setObjectName(QString::fromUtf8("focusPositionLabel"));
        focusPositionLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allFocus->addWidget(focusPositionLabel, 2, 0, 1, 1);

        allAutoFocusButtonsFrame = new QFrame(allFocusFormFrame);
        allAutoFocusButtonsFrame->setObjectName(QString::fromUtf8("allAutoFocusButtonsFrame"));
        allAutoFocusButtonsFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_allAutoFocusButtons = new QHBoxLayout(allAutoFocusButtonsFrame);
        horizontalLayout_allAutoFocusButtons->setSpacing(10);
        horizontalLayout_allAutoFocusButtons->setObjectName(QString::fromUtf8("horizontalLayout_allAutoFocusButtons"));
        horizontalLayout_allAutoFocusButtons->setContentsMargins(0, 0, 0, 0);
        allAutoFocusButton = new QPushButton(allAutoFocusButtonsFrame);
        allAutoFocusButton->setObjectName(QString::fromUtf8("allAutoFocusButton"));
        allAutoFocusButton->setMinimumSize(QSize(202, 44));
        allAutoFocusButton->setProperty("softButton", QVariant(true));

        horizontalLayout_allAutoFocusButtons->addWidget(allAutoFocusButton);

        allAutoFocusResetButton = new QToolButton(allAutoFocusButtonsFrame);
        allAutoFocusResetButton->setObjectName(QString::fromUtf8("allAutoFocusResetButton"));
        allAutoFocusResetButton->setMinimumSize(QSize(56, 40));

        horizontalLayout_allAutoFocusButtons->addWidget(allAutoFocusResetButton);


        gridLayout_allFocus->addWidget(allAutoFocusButtonsFrame, 4, 1, 1, 1);

        allFocusRegionLabel = new QLabel(allFocusFormFrame);
        allFocusRegionLabel->setObjectName(QString::fromUtf8("allFocusRegionLabel"));
        allFocusRegionLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allFocus->addWidget(allFocusRegionLabel, 3, 0, 1, 1);

        focusModeLabel = new QLabel(allFocusFormFrame);
        focusModeLabel->setObjectName(QString::fromUtf8("focusModeLabel"));
        focusModeLabel->setProperty("formLabel", QVariant(true));

        gridLayout_allFocus->addWidget(focusModeLabel, 0, 0, 1, 1);

        focusStepspinBox = new QSpinBox(allFocusFormFrame);
        focusStepspinBox->setObjectName(QString::fromUtf8("focusStepspinBox"));
        sizePolicy.setHeightForWidth(focusStepspinBox->sizePolicy().hasHeightForWidth());
        focusStepspinBox->setSizePolicy(sizePolicy);
        focusStepspinBox->setMinimumSize(QSize(260, 40));
        focusStepspinBox->setMaximum(200);
        focusStepspinBox->setValue(100);

        gridLayout_allFocus->addWidget(focusStepspinBox, 1, 1, 1, 1);

        focusPositionspinBox = new QSpinBox(allFocusFormFrame);
        focusPositionspinBox->setObjectName(QString::fromUtf8("focusPositionspinBox"));
        sizePolicy.setHeightForWidth(focusPositionspinBox->sizePolicy().hasHeightForWidth());
        focusPositionspinBox->setSizePolicy(sizePolicy);
        focusPositionspinBox->setMinimumSize(QSize(260, 40));
        focusPositionspinBox->setMaximum(5000);
        focusPositionspinBox->setValue(2300);

        gridLayout_allFocus->addWidget(focusPositionspinBox, 2, 1, 1, 1);


        verticalLayout_allFocusCard->addWidget(allFocusFormFrame);


        verticalLayout_allParamsContent->addWidget(allFocusCard);

        lightControlCard = new QFrame(allParamsScrollContent);
        lightControlCard->setObjectName(QString::fromUtf8("lightControlCard"));
        lightControlCard->setFrameShape(QFrame::NoFrame);
        lightControlCard->setProperty("card", QVariant(true));
        verticalLayout_lightControlCard = new QVBoxLayout(lightControlCard);
        verticalLayout_lightControlCard->setSpacing(14);
        verticalLayout_lightControlCard->setObjectName(QString::fromUtf8("verticalLayout_lightControlCard"));
        verticalLayout_lightControlCard->setContentsMargins(18, 18, 18, 18);
        lightControlCardHeaderFrame = new QFrame(lightControlCard);
        lightControlCardHeaderFrame->setObjectName(QString::fromUtf8("lightControlCardHeaderFrame"));
        lightControlCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_lightControlCardHeader = new QHBoxLayout(lightControlCardHeaderFrame);
        horizontalLayout_lightControlCardHeader->setSpacing(0);
        horizontalLayout_lightControlCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_lightControlCardHeader"));
        horizontalLayout_lightControlCardHeader->setContentsMargins(0, 0, 0, 0);
        lightControlCardTitleLabel = new QLabel(lightControlCardHeaderFrame);
        lightControlCardTitleLabel->setObjectName(QString::fromUtf8("lightControlCardTitleLabel"));
        lightControlCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_lightControlCardHeader->addWidget(lightControlCardTitleLabel);

        horizontalSpacer_lightControlCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_lightControlCardHeader->addItem(horizontalSpacer_lightControlCardHeader);

        lightControlCardCollapseButton = new QToolButton(lightControlCardHeaderFrame);
        lightControlCardCollapseButton->setObjectName(QString::fromUtf8("lightControlCardCollapseButton"));
        lightControlCardCollapseButton->setMinimumSize(QSize(28, 28));
        lightControlCardCollapseButton->setAutoRaise(true);

        horizontalLayout_lightControlCardHeader->addWidget(lightControlCardCollapseButton);


        verticalLayout_lightControlCard->addWidget(lightControlCardHeaderFrame);

        lightControlFormFrame = new QFrame(lightControlCard);
        lightControlFormFrame->setObjectName(QString::fromUtf8("lightControlFormFrame"));
        lightControlFormFrame->setFrameShape(QFrame::NoFrame);
        gridLayout_lightControl = new QGridLayout(lightControlFormFrame);
        gridLayout_lightControl->setObjectName(QString::fromUtf8("gridLayout_lightControl"));
        gridLayout_lightControl->setHorizontalSpacing(18);
        gridLayout_lightControl->setVerticalSpacing(12);
        gridLayout_lightControl->setContentsMargins(0, 0, 0, 0);
        lightControlLabel = new QLabel(lightControlFormFrame);
        lightControlLabel->setObjectName(QString::fromUtf8("lightControlLabel"));
        lightControlLabel->setProperty("formLabel", QVariant(true));

        gridLayout_lightControl->addWidget(lightControlLabel, 0, 0, 1, 1);

        lightControlComboBox = new QComboBox(lightControlFormFrame);
        lightControlComboBox->addItem(QString());
        lightControlComboBox->setObjectName(QString::fromUtf8("lightControlComboBox"));
        lightControlComboBox->setMinimumSize(QSize(260, 40));

        gridLayout_lightControl->addWidget(lightControlComboBox, 0, 1, 1, 1);

        lightDevicePreviewFrame = new QFrame(lightControlFormFrame);
        lightDevicePreviewFrame->setObjectName(QString::fromUtf8("lightDevicePreviewFrame"));
        lightDevicePreviewFrame->setMinimumSize(QSize(210, 210));
        lightDevicePreviewFrame->setFrameShape(QFrame::NoFrame);
        verticalLayout_lightDevicePreview = new QVBoxLayout(lightDevicePreviewFrame);
        verticalLayout_lightDevicePreview->setSpacing(0);
        verticalLayout_lightDevicePreview->setObjectName(QString::fromUtf8("verticalLayout_lightDevicePreview"));
        verticalLayout_lightDevicePreview->setContentsMargins(0, 0, 0, 0);
        lightDeviceIconLabel = new QLabel(lightDevicePreviewFrame);
        lightDeviceIconLabel->setObjectName(QString::fromUtf8("lightDeviceIconLabel"));
        lightDeviceIconLabel->setProperty("devicePreview", QVariant(true));

        verticalLayout_lightDevicePreview->addWidget(lightDeviceIconLabel);


        gridLayout_lightControl->addWidget(lightDevicePreviewFrame, 1, 1, 1, 1);

        lightPositionLabel = new QLabel(lightControlFormFrame);
        lightPositionLabel->setObjectName(QString::fromUtf8("lightPositionLabel"));
        lightPositionLabel->setProperty("formLabel", QVariant(true));

        gridLayout_lightControl->addWidget(lightPositionLabel, 2, 0, 1, 1);

        lightPositionFrame = new QFrame(lightControlFormFrame);
        lightPositionFrame->setObjectName(QString::fromUtf8("lightPositionFrame"));
        lightPositionFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_lightPosition = new QHBoxLayout(lightPositionFrame);
        horizontalLayout_lightPosition->setSpacing(12);
        horizontalLayout_lightPosition->setObjectName(QString::fromUtf8("horizontalLayout_lightPosition"));
        horizontalLayout_lightPosition->setContentsMargins(0, 0, 0, 0);
        lightPrevButton = new QToolButton(lightPositionFrame);
        lightPrevButton->setObjectName(QString::fromUtf8("lightPrevButton"));
        lightPrevButton->setMinimumSize(QSize(56, 38));

        horizontalLayout_lightPosition->addWidget(lightPrevButton);

        lightPositionValueLabel = new QLabel(lightPositionFrame);
        lightPositionValueLabel->setObjectName(QString::fromUtf8("lightPositionValueLabel"));
        lightPositionValueLabel->setProperty("centerText", QVariant(true));

        horizontalLayout_lightPosition->addWidget(lightPositionValueLabel);

        lightNextButton = new QToolButton(lightPositionFrame);
        lightNextButton->setObjectName(QString::fromUtf8("lightNextButton"));
        lightNextButton->setMinimumSize(QSize(56, 38));

        horizontalLayout_lightPosition->addWidget(lightNextButton);


        gridLayout_lightControl->addWidget(lightPositionFrame, 2, 1, 1, 1);


        verticalLayout_lightControlCard->addWidget(lightControlFormFrame);


        verticalLayout_allParamsContent->addWidget(lightControlCard);

        otherParamsCard = new QFrame(allParamsScrollContent);
        otherParamsCard->setObjectName(QString::fromUtf8("otherParamsCard"));
        otherParamsCard->setFrameShape(QFrame::NoFrame);
        otherParamsCard->setProperty("card", QVariant(true));
        verticalLayout_otherParamsCard = new QVBoxLayout(otherParamsCard);
        verticalLayout_otherParamsCard->setSpacing(14);
        verticalLayout_otherParamsCard->setObjectName(QString::fromUtf8("verticalLayout_otherParamsCard"));
        verticalLayout_otherParamsCard->setContentsMargins(18, 18, 18, 18);
        otherParamsCardHeaderFrame = new QFrame(otherParamsCard);
        otherParamsCardHeaderFrame->setObjectName(QString::fromUtf8("otherParamsCardHeaderFrame"));
        otherParamsCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_otherParamsCardHeader = new QHBoxLayout(otherParamsCardHeaderFrame);
        horizontalLayout_otherParamsCardHeader->setSpacing(0);
        horizontalLayout_otherParamsCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_otherParamsCardHeader"));
        horizontalLayout_otherParamsCardHeader->setContentsMargins(0, 0, 0, 0);
        otherParamsCardTitleLabel = new QLabel(otherParamsCardHeaderFrame);
        otherParamsCardTitleLabel->setObjectName(QString::fromUtf8("otherParamsCardTitleLabel"));
        otherParamsCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_otherParamsCardHeader->addWidget(otherParamsCardTitleLabel);

        horizontalSpacer_otherParamsCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_otherParamsCardHeader->addItem(horizontalSpacer_otherParamsCardHeader);

        otherParamsCardCollapseButton = new QToolButton(otherParamsCardHeaderFrame);
        otherParamsCardCollapseButton->setObjectName(QString::fromUtf8("otherParamsCardCollapseButton"));
        otherParamsCardCollapseButton->setMinimumSize(QSize(28, 28));
        otherParamsCardCollapseButton->setAutoRaise(true);

        horizontalLayout_otherParamsCardHeader->addWidget(otherParamsCardCollapseButton);


        verticalLayout_otherParamsCard->addWidget(otherParamsCardHeaderFrame);

        otherParamsFormFrame = new QFrame(otherParamsCard);
        otherParamsFormFrame->setObjectName(QString::fromUtf8("otherParamsFormFrame"));
        otherParamsFormFrame->setFrameShape(QFrame::NoFrame);
        gridLayout_otherParams = new QGridLayout(otherParamsFormFrame);
        gridLayout_otherParams->setObjectName(QString::fromUtf8("gridLayout_otherParams"));
        gridLayout_otherParams->setHorizontalSpacing(18);
        gridLayout_otherParams->setVerticalSpacing(12);
        gridLayout_otherParams->setContentsMargins(0, 0, 0, 0);
        sharpenLabel = new QLabel(otherParamsFormFrame);
        sharpenLabel->setObjectName(QString::fromUtf8("sharpenLabel"));
        sharpenLabel->setProperty("formLabel", QVariant(true));

        gridLayout_otherParams->addWidget(sharpenLabel, 2, 0, 1, 1);

        xMirrorCheckBox = new QCheckBox(otherParamsFormFrame);
        xMirrorCheckBox->setObjectName(QString::fromUtf8("xMirrorCheckBox"));
        xMirrorCheckBox->setMinimumSize(QSize(58, 28));

        gridLayout_otherParams->addWidget(xMirrorCheckBox, 5, 2, 1, 1);

        xMirrorLabel = new QLabel(otherParamsFormFrame);
        xMirrorLabel->setObjectName(QString::fromUtf8("xMirrorLabel"));
        xMirrorLabel->setProperty("formLabel", QVariant(true));

        gridLayout_otherParams->addWidget(xMirrorLabel, 5, 0, 1, 1);

        yMirrorCheckBox = new QCheckBox(otherParamsFormFrame);
        yMirrorCheckBox->setObjectName(QString::fromUtf8("yMirrorCheckBox"));
        yMirrorCheckBox->setMinimumSize(QSize(58, 28));

        gridLayout_otherParams->addWidget(yMirrorCheckBox, 6, 2, 1, 1);

        yMirrorLabel = new QLabel(otherParamsFormFrame);
        yMirrorLabel->setObjectName(QString::fromUtf8("yMirrorLabel"));
        yMirrorLabel->setProperty("formLabel", QVariant(true));

        gridLayout_otherParams->addWidget(yMirrorLabel, 6, 0, 1, 1);

        contrastLabel = new QLabel(otherParamsFormFrame);
        contrastLabel->setObjectName(QString::fromUtf8("contrastLabel"));
        contrastLabel->setProperty("formLabel", QVariant(true));

        gridLayout_otherParams->addWidget(contrastLabel, 1, 0, 1, 1);

        imageSizeSegmentFrame = new QFrame(otherParamsFormFrame);
        imageSizeSegmentFrame->setObjectName(QString::fromUtf8("imageSizeSegmentFrame"));
        imageSizeSegmentFrame->setMinimumSize(QSize(212, 34));
        imageSizeSegmentFrame->setMaximumSize(QSize(16777215, 34));
        imageSizeSegmentFrame->setFrameShape(QFrame::NoFrame);
        imageSizeSegmentFrame->setProperty("segmented", QVariant(true));
        horizontalLayout_imageSizeSegmentFrame = new QHBoxLayout(imageSizeSegmentFrame);
        horizontalLayout_imageSizeSegmentFrame->setSpacing(0);
        horizontalLayout_imageSizeSegmentFrame->setObjectName(QString::fromUtf8("horizontalLayout_imageSizeSegmentFrame"));
        horizontalLayout_imageSizeSegmentFrame->setContentsMargins(0, 0, 0, 0);
        imageOriginalButton = new QPushButton(imageSizeSegmentFrame);
        imageOriginalButton->setObjectName(QString::fromUtf8("imageOriginalButton"));
        imageOriginalButton->setMinimumSize(QSize(106, 38));
        imageOriginalButton->setCheckable(true);
        imageOriginalButton->setChecked(true);

        horizontalLayout_imageSizeSegmentFrame->addWidget(imageOriginalButton);

        imageCustomButton = new QPushButton(imageSizeSegmentFrame);
        imageCustomButton->setObjectName(QString::fromUtf8("imageCustomButton"));
        imageCustomButton->setMinimumSize(QSize(106, 38));
        imageCustomButton->setCheckable(true);

        horizontalLayout_imageSizeSegmentFrame->addWidget(imageCustomButton);


        gridLayout_otherParams->addWidget(imageSizeSegmentFrame, 3, 2, 1, 1);

        gammaLineEdit = new QLineEdit(otherParamsFormFrame);
        gammaLineEdit->setObjectName(QString::fromUtf8("gammaLineEdit"));
        gammaLineEdit->setMinimumSize(QSize(260, 40));

        gridLayout_otherParams->addWidget(gammaLineEdit, 0, 2, 1, 1);

        sharpenspinBox = new QSpinBox(otherParamsFormFrame);
        sharpenspinBox->setObjectName(QString::fromUtf8("sharpenspinBox"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(sharpenspinBox->sizePolicy().hasHeightForWidth());
        sharpenspinBox->setSizePolicy(sizePolicy1);
        sharpenspinBox->setMinimumSize(QSize(260, 40));
        sharpenspinBox->setValue(50);

        gridLayout_otherParams->addWidget(sharpenspinBox, 2, 2, 1, 1);

        contrastspinBox = new QSpinBox(otherParamsFormFrame);
        contrastspinBox->setObjectName(QString::fromUtf8("contrastspinBox"));
        sizePolicy.setHeightForWidth(contrastspinBox->sizePolicy().hasHeightForWidth());
        contrastspinBox->setSizePolicy(sizePolicy);
        contrastspinBox->setMinimumSize(QSize(260, 40));
        contrastspinBox->setValue(50);

        gridLayout_otherParams->addWidget(contrastspinBox, 1, 2, 1, 1);

        gammaLabel = new QLabel(otherParamsFormFrame);
        gammaLabel->setObjectName(QString::fromUtf8("gammaLabel"));
        gammaLabel->setProperty("formLabel", QVariant(true));

        gridLayout_otherParams->addWidget(gammaLabel, 0, 0, 1, 1);

        grayOutputLabel = new QLabel(otherParamsFormFrame);
        grayOutputLabel->setObjectName(QString::fromUtf8("grayOutputLabel"));
        grayOutputLabel->setProperty("formLabel", QVariant(true));

        gridLayout_otherParams->addWidget(grayOutputLabel, 4, 0, 1, 1);

        grayOutputCheckBox = new QCheckBox(otherParamsFormFrame);
        grayOutputCheckBox->setObjectName(QString::fromUtf8("grayOutputCheckBox"));
        grayOutputCheckBox->setMinimumSize(QSize(58, 28));

        gridLayout_otherParams->addWidget(grayOutputCheckBox, 4, 2, 1, 1);

        encodeQualityLabel = new QLabel(otherParamsFormFrame);
        encodeQualityLabel->setObjectName(QString::fromUtf8("encodeQualityLabel"));
        encodeQualityLabel->setProperty("formLabel", QVariant(true));

        gridLayout_otherParams->addWidget(encodeQualityLabel, 7, 0, 1, 1);

        imageSizeLabel = new QLabel(otherParamsFormFrame);
        imageSizeLabel->setObjectName(QString::fromUtf8("imageSizeLabel"));
        imageSizeLabel->setProperty("formLabel", QVariant(true));

        gridLayout_otherParams->addWidget(imageSizeLabel, 3, 0, 1, 1);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        gridLayout_otherParams->addItem(horizontalSpacer_2, 0, 1, 1, 1);

        encodeQualityspinBox = new QSpinBox(otherParamsFormFrame);
        encodeQualityspinBox->setObjectName(QString::fromUtf8("encodeQualityspinBox"));
        sizePolicy.setHeightForWidth(encodeQualityspinBox->sizePolicy().hasHeightForWidth());
        encodeQualityspinBox->setSizePolicy(sizePolicy);
        encodeQualityspinBox->setMinimumSize(QSize(260, 40));
        encodeQualityspinBox->setValue(70);

        gridLayout_otherParams->addWidget(encodeQualityspinBox, 7, 2, 1, 1);


        verticalLayout_otherParamsCard->addWidget(otherParamsFormFrame);


        verticalLayout_allParamsContent->addWidget(otherParamsCard);

        colorParamsCard = new QFrame(allParamsScrollContent);
        colorParamsCard->setObjectName(QString::fromUtf8("colorParamsCard"));
        colorParamsCard->setFrameShape(QFrame::NoFrame);
        colorParamsCard->setProperty("card", QVariant(true));
        verticalLayout_colorParamsCard = new QVBoxLayout(colorParamsCard);
        verticalLayout_colorParamsCard->setSpacing(14);
        verticalLayout_colorParamsCard->setObjectName(QString::fromUtf8("verticalLayout_colorParamsCard"));
        verticalLayout_colorParamsCard->setContentsMargins(18, 18, 18, 18);
        colorParamsCardHeaderFrame = new QFrame(colorParamsCard);
        colorParamsCardHeaderFrame->setObjectName(QString::fromUtf8("colorParamsCardHeaderFrame"));
        colorParamsCardHeaderFrame->setFrameShape(QFrame::NoFrame);
        horizontalLayout_colorParamsCardHeader = new QHBoxLayout(colorParamsCardHeaderFrame);
        horizontalLayout_colorParamsCardHeader->setSpacing(0);
        horizontalLayout_colorParamsCardHeader->setObjectName(QString::fromUtf8("horizontalLayout_colorParamsCardHeader"));
        horizontalLayout_colorParamsCardHeader->setContentsMargins(0, 0, 0, 0);
        colorParamsCardTitleLabel = new QLabel(colorParamsCardHeaderFrame);
        colorParamsCardTitleLabel->setObjectName(QString::fromUtf8("colorParamsCardTitleLabel"));
        colorParamsCardTitleLabel->setProperty("cardTitle", QVariant(true));

        horizontalLayout_colorParamsCardHeader->addWidget(colorParamsCardTitleLabel);

        horizontalSpacer_colorParamsCardHeader = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_colorParamsCardHeader->addItem(horizontalSpacer_colorParamsCardHeader);

        colorParamsCardCollapseButton = new QToolButton(colorParamsCardHeaderFrame);
        colorParamsCardCollapseButton->setObjectName(QString::fromUtf8("colorParamsCardCollapseButton"));
        colorParamsCardCollapseButton->setMinimumSize(QSize(28, 28));
        colorParamsCardCollapseButton->setAutoRaise(true);

        horizontalLayout_colorParamsCardHeader->addWidget(colorParamsCardCollapseButton);


        verticalLayout_colorParamsCard->addWidget(colorParamsCardHeaderFrame);

        colorParamsFormFrame = new QFrame(colorParamsCard);
        colorParamsFormFrame->setObjectName(QString::fromUtf8("colorParamsFormFrame"));
        colorParamsFormFrame->setFrameShape(QFrame::NoFrame);
        gridLayout_colorParams = new QGridLayout(colorParamsFormFrame);
        gridLayout_colorParams->setObjectName(QString::fromUtf8("gridLayout_colorParams"));
        gridLayout_colorParams->setHorizontalSpacing(18);
        gridLayout_colorParams->setVerticalSpacing(12);
        gridLayout_colorParams->setContentsMargins(0, 0, 0, 0);
        whiteBalanceLabel = new QLabel(colorParamsFormFrame);
        whiteBalanceLabel->setObjectName(QString::fromUtf8("whiteBalanceLabel"));
        whiteBalanceLabel->setProperty("formLabel", QVariant(true));

        gridLayout_colorParams->addWidget(whiteBalanceLabel, 0, 0, 1, 1);

        whiteBalanceCheckBox = new QCheckBox(colorParamsFormFrame);
        whiteBalanceCheckBox->setObjectName(QString::fromUtf8("whiteBalanceCheckBox"));
        whiteBalanceCheckBox->setMinimumSize(QSize(58, 28));
        whiteBalanceCheckBox->setChecked(true);

        gridLayout_colorParams->addWidget(whiteBalanceCheckBox, 0, 1, 1, 1);

        colorAutoAdjustLabel = new QLabel(colorParamsFormFrame);
        colorAutoAdjustLabel->setObjectName(QString::fromUtf8("colorAutoAdjustLabel"));
        colorAutoAdjustLabel->setProperty("formLabel", QVariant(true));

        gridLayout_colorParams->addWidget(colorAutoAdjustLabel, 1, 0, 1, 1);

        colorAutoAdjustButton = new QPushButton(colorParamsFormFrame);
        colorAutoAdjustButton->setObjectName(QString::fromUtf8("colorAutoAdjustButton"));
        colorAutoAdjustButton->setMinimumSize(QSize(260, 44));
        colorAutoAdjustButton->setProperty("softButton", QVariant(true));

        gridLayout_colorParams->addWidget(colorAutoAdjustButton, 1, 1, 1, 1);

        whiteBalanceParamsLabel = new QLabel(colorParamsFormFrame);
        whiteBalanceParamsLabel->setObjectName(QString::fromUtf8("whiteBalanceParamsLabel"));
        whiteBalanceParamsLabel->setProperty("formLabel", QVariant(true));

        gridLayout_colorParams->addWidget(whiteBalanceParamsLabel, 2, 0, 1, 1);

        whiteBalanceEditButton = new QPushButton(colorParamsFormFrame);
        whiteBalanceEditButton->setObjectName(QString::fromUtf8("whiteBalanceEditButton"));
        whiteBalanceEditButton->setMinimumSize(QSize(260, 44));
        whiteBalanceEditButton->setProperty("softButton", QVariant(true));

        gridLayout_colorParams->addWidget(whiteBalanceEditButton, 2, 1, 1, 1);

        colorBalanceTable = new QFrame(colorParamsFormFrame);
        colorBalanceTable->setObjectName(QString::fromUtf8("colorBalanceTable"));
        colorBalanceTable->setFrameShape(QFrame::NoFrame);
        colorBalanceTable->setProperty("table", QVariant(true));
        gridLayout_colorBalanceTable = new QGridLayout(colorBalanceTable);
        gridLayout_colorBalanceTable->setSpacing(0);
        gridLayout_colorBalanceTable->setObjectName(QString::fromUtf8("gridLayout_colorBalanceTable"));
        gridLayout_colorBalanceTable->setContentsMargins(0, 0, 0, 0);
        colorBalanceTableCell00 = new QLabel(colorBalanceTable);
        colorBalanceTableCell00->setObjectName(QString::fromUtf8("colorBalanceTableCell00"));
        colorBalanceTableCell00->setProperty("tableCell", QVariant(true));

        gridLayout_colorBalanceTable->addWidget(colorBalanceTableCell00, 0, 0, 1, 1);

        colorBalanceTableCell01 = new QLabel(colorBalanceTable);
        colorBalanceTableCell01->setObjectName(QString::fromUtf8("colorBalanceTableCell01"));
        colorBalanceTableCell01->setProperty("tableCell", QVariant(true));

        gridLayout_colorBalanceTable->addWidget(colorBalanceTableCell01, 0, 1, 1, 1);

        colorBalanceTableCell02 = new QLabel(colorBalanceTable);
        colorBalanceTableCell02->setObjectName(QString::fromUtf8("colorBalanceTableCell02"));
        colorBalanceTableCell02->setProperty("tableCell", QVariant(true));

        gridLayout_colorBalanceTable->addWidget(colorBalanceTableCell02, 0, 2, 1, 1);

        colorBalanceTableCell10 = new QLabel(colorBalanceTable);
        colorBalanceTableCell10->setObjectName(QString::fromUtf8("colorBalanceTableCell10"));
        colorBalanceTableCell10->setProperty("tableCell", QVariant(true));

        gridLayout_colorBalanceTable->addWidget(colorBalanceTableCell10, 1, 0, 1, 1);

        colorBalanceTableCell11 = new QLabel(colorBalanceTable);
        colorBalanceTableCell11->setObjectName(QString::fromUtf8("colorBalanceTableCell11"));
        colorBalanceTableCell11->setProperty("tableCell", QVariant(true));

        gridLayout_colorBalanceTable->addWidget(colorBalanceTableCell11, 1, 1, 1, 1);

        colorBalanceTableCell12 = new QLabel(colorBalanceTable);
        colorBalanceTableCell12->setObjectName(QString::fromUtf8("colorBalanceTableCell12"));
        colorBalanceTableCell12->setProperty("tableCell", QVariant(true));

        gridLayout_colorBalanceTable->addWidget(colorBalanceTableCell12, 1, 2, 1, 1);


        gridLayout_colorParams->addWidget(colorBalanceTable, 3, 0, 1, 2);

        ccmResetLabel = new QLabel(colorParamsFormFrame);
        ccmResetLabel->setObjectName(QString::fromUtf8("ccmResetLabel"));
        ccmResetLabel->setProperty("formLabel", QVariant(true));

        gridLayout_colorParams->addWidget(ccmResetLabel, 4, 0, 1, 1);

        ccmResetButton = new QPushButton(colorParamsFormFrame);
        ccmResetButton->setObjectName(QString::fromUtf8("ccmResetButton"));
        ccmResetButton->setMinimumSize(QSize(260, 44));
        ccmResetButton->setProperty("softButton", QVariant(true));

        gridLayout_colorParams->addWidget(ccmResetButton, 4, 1, 1, 1);

        ccmParamsLabel = new QLabel(colorParamsFormFrame);
        ccmParamsLabel->setObjectName(QString::fromUtf8("ccmParamsLabel"));
        ccmParamsLabel->setProperty("formLabel", QVariant(true));

        gridLayout_colorParams->addWidget(ccmParamsLabel, 5, 0, 1, 1);

        ccmEditButton = new QPushButton(colorParamsFormFrame);
        ccmEditButton->setObjectName(QString::fromUtf8("ccmEditButton"));
        ccmEditButton->setMinimumSize(QSize(260, 44));
        ccmEditButton->setProperty("softButton", QVariant(true));

        gridLayout_colorParams->addWidget(ccmEditButton, 5, 1, 1, 1);

        ccmTable = new QFrame(colorParamsFormFrame);
        ccmTable->setObjectName(QString::fromUtf8("ccmTable"));
        ccmTable->setFrameShape(QFrame::NoFrame);
        ccmTable->setProperty("table", QVariant(true));
        gridLayout_ccmTable = new QGridLayout(ccmTable);
        gridLayout_ccmTable->setSpacing(0);
        gridLayout_ccmTable->setObjectName(QString::fromUtf8("gridLayout_ccmTable"));
        gridLayout_ccmTable->setContentsMargins(0, 0, 0, 0);
        ccmTableCell00 = new QLabel(ccmTable);
        ccmTableCell00->setObjectName(QString::fromUtf8("ccmTableCell00"));
        ccmTableCell00->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell00, 0, 0, 1, 1);

        ccmTableCell01 = new QLabel(ccmTable);
        ccmTableCell01->setObjectName(QString::fromUtf8("ccmTableCell01"));
        ccmTableCell01->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell01, 0, 1, 1, 1);

        ccmTableCell02 = new QLabel(ccmTable);
        ccmTableCell02->setObjectName(QString::fromUtf8("ccmTableCell02"));
        ccmTableCell02->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell02, 0, 2, 1, 1);

        ccmTableCell03 = new QLabel(ccmTable);
        ccmTableCell03->setObjectName(QString::fromUtf8("ccmTableCell03"));
        ccmTableCell03->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell03, 0, 3, 1, 1);

        ccmTableCell10 = new QLabel(ccmTable);
        ccmTableCell10->setObjectName(QString::fromUtf8("ccmTableCell10"));
        ccmTableCell10->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell10, 1, 0, 1, 1);

        ccmTableCell11 = new QLabel(ccmTable);
        ccmTableCell11->setObjectName(QString::fromUtf8("ccmTableCell11"));
        ccmTableCell11->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell11, 1, 1, 1, 1);

        ccmTableCell12 = new QLabel(ccmTable);
        ccmTableCell12->setObjectName(QString::fromUtf8("ccmTableCell12"));
        ccmTableCell12->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell12, 1, 2, 1, 1);

        ccmTableCell13 = new QLabel(ccmTable);
        ccmTableCell13->setObjectName(QString::fromUtf8("ccmTableCell13"));
        ccmTableCell13->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell13, 1, 3, 1, 1);

        ccmTableCell20 = new QLabel(ccmTable);
        ccmTableCell20->setObjectName(QString::fromUtf8("ccmTableCell20"));
        ccmTableCell20->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell20, 2, 0, 1, 1);

        ccmTableCell21 = new QLabel(ccmTable);
        ccmTableCell21->setObjectName(QString::fromUtf8("ccmTableCell21"));
        ccmTableCell21->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell21, 2, 1, 1, 1);

        ccmTableCell22 = new QLabel(ccmTable);
        ccmTableCell22->setObjectName(QString::fromUtf8("ccmTableCell22"));
        ccmTableCell22->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell22, 2, 2, 1, 1);

        ccmTableCell23 = new QLabel(ccmTable);
        ccmTableCell23->setObjectName(QString::fromUtf8("ccmTableCell23"));
        ccmTableCell23->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell23, 2, 3, 1, 1);

        ccmTableCell30 = new QLabel(ccmTable);
        ccmTableCell30->setObjectName(QString::fromUtf8("ccmTableCell30"));
        ccmTableCell30->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell30, 3, 0, 1, 1);

        ccmTableCell31 = new QLabel(ccmTable);
        ccmTableCell31->setObjectName(QString::fromUtf8("ccmTableCell31"));
        ccmTableCell31->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell31, 3, 1, 1, 1);

        ccmTableCell32 = new QLabel(ccmTable);
        ccmTableCell32->setObjectName(QString::fromUtf8("ccmTableCell32"));
        ccmTableCell32->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell32, 3, 2, 1, 1);

        ccmTableCell33 = new QLabel(ccmTable);
        ccmTableCell33->setObjectName(QString::fromUtf8("ccmTableCell33"));
        ccmTableCell33->setProperty("tableCell", QVariant(true));

        gridLayout_ccmTable->addWidget(ccmTableCell33, 3, 3, 1, 1);


        gridLayout_colorParams->addWidget(ccmTable, 6, 0, 1, 2);


        verticalLayout_colorParamsCard->addWidget(colorParamsFormFrame);


        verticalLayout_allParamsContent->addWidget(colorParamsCard);

        verticalSpacer_allParams = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_allParamsContent->addItem(verticalSpacer_allParams);

        allParamsScrollArea->setWidget(allParamsScrollContent);

        verticalLayout_allParamsPage->addWidget(allParamsScrollArea);

        cameraParamsStack->addWidget(allParamsPage);

        verticalLayout_editor->addWidget(cameraParamsStack);

        horizontalLayout_nav = new QHBoxLayout();
        horizontalLayout_nav->setSpacing(0);
        horizontalLayout_nav->setObjectName(QString::fromUtf8("horizontalLayout_nav"));
        horizontalLayout_nav->setContentsMargins(0, 14, 0, 0);
        previousButton = new QPushButton(setupEditorPanel);
        previousButton->setObjectName(QString::fromUtf8("previousButton"));
        previousButton->setEnabled(false);
        previousButton->setMinimumSize(QSize(112, 50));

        horizontalLayout_nav->addWidget(previousButton);

        horizontalSpacer_nav = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_nav->addItem(horizontalSpacer_nav);

        nextButton = new QPushButton(setupEditorPanel);
        nextButton->setObjectName(QString::fromUtf8("nextButton"));
        nextButton->setMinimumSize(QSize(110, 48));

        horizontalLayout_nav->addWidget(nextButton);


        verticalLayout_editor->addLayout(horizontalLayout_nav);


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
        QIcon icon9;
        icon9.addFile(QString::fromUtf8(":/icons/grid.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerGridButton->setIcon(icon9);
        viewerGridButton->setIconSize(QSize(24, 24));
        viewerGridButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerGridButton);

        viewerZoomSearchButton = new QToolButton(viewerHeader);
        viewerZoomSearchButton->setObjectName(QString::fromUtf8("viewerZoomSearchButton"));
        viewerZoomSearchButton->setMinimumSize(QSize(28, 28));
        QIcon icon10;
        icon10.addFile(QString::fromUtf8(":/icons/zoom.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerZoomSearchButton->setIcon(icon10);
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
        QIcon icon11;
        icon11.addFile(QString::fromUtf8(":/icons/fullscreen.svg"), QSize(), QIcon::Normal, QIcon::Off);
        viewerFullButton->setIcon(icon11);
        viewerFullButton->setIconSize(QSize(24, 24));
        viewerFullButton->setAutoRaise(true);

        horizontalLayout_viewerHeader->addWidget(viewerFullButton);


        verticalLayout_viewer->addWidget(viewerHeader);

        camera_1 = new QGraphicsView(setupViewerFrame);
        camera_1->setObjectName(QString::fromUtf8("camera_1"));
        camera_1->setProperty("viewerCanvas", QVariant(true));
        camera_1->setFrameShape(QFrame::NoFrame);
        camera_1->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        camera_1->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        camera_1->setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);

        verticalLayout_viewer->addWidget(camera_1);

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


        retranslateUi(CameraParamsDialog);

        cameraParamsStack->setCurrentIndex(1);


        QMetaObject::connectSlotsByName(CameraParamsDialog);
    } // setupUi

    void retranslateUi(QDialog *CameraParamsDialog)
    {
        CameraParamsDialog->setWindowTitle(QCoreApplication::translate("CameraParamsDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \347\233\270\346\234\272\345\217\202\346\225\260", nullptr));
        setupWindowTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\346\226\271\346\241\210\347\274\226\350\276\221", nullptr));
        headerMaximizeButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\226\241", nullptr));
        headerCloseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\303\227", nullptr));
        setupPageCodeLabel->setText(QCoreApplication::translate("CameraParamsDialog", "422", nullptr));
        setupExternalEditButton->setText(QString());
        setupSaveButton->setText(QCoreApplication::translate("CameraParamsDialog", "\344\277\235\345\255\230", nullptr));
        setupSaveAsButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\217\246\345\255\230\344\270\272", nullptr));
        setupExportButton->setText(QCoreApplication::translate("CameraParamsDialog", "IO\350\276\223\345\207\272", nullptr));
        setupQuickCalibrateButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\277\253\351\200\237\346\240\207\345\256\232", nullptr));
        cameraStepButton->setText(QCoreApplication::translate("CameraParamsDialog", "\347\233\270\346\234\272\345\217\202\346\225\260", nullptr));
        stepDividerLabel1->setText(QCoreApplication::translate("CameraParamsDialog", "\342\226\276", nullptr));
        referenceStepButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        stepDividerLabel2->setText(QCoreApplication::translate("CameraParamsDialog", "\342\226\276", nullptr));
        toolsStepButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\267\245\345\205\267", nullptr));
        stepDividerLabel3->setText(QCoreApplication::translate("CameraParamsDialog", "\342\226\276", nullptr));
        outputStepButton->setText(QCoreApplication::translate("CameraParamsDialog", "\350\276\223\345\207\272", nullptr));
        editorTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "1 / \347\233\270\346\234\272\345\217\202\346\225\260", nullptr));
        basicModeButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\237\272\347\241\200", nullptr));
        allModeButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\205\250\351\203\250", nullptr));
        imagingAdjustCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\346\210\220\345\203\217\350\260\203\350\212\202", nullptr));
        imagingAdjustCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        imagingAdjustHintLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\344\270\200\351\224\256\350\260\203\350\212\202\347\233\270\346\234\272\344\272\256\345\272\246\343\200\201\347\231\275\345\271\263\350\241\241\343\200\201\345\257\271\347\204\246 \342\223\230", nullptr));
        oneKeyTuneButton->setText(QCoreApplication::translate("CameraParamsDialog", "\344\270\200\351\224\256\350\260\203\350\260\220", nullptr));
        triggerConfigCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\350\247\246\345\217\221\351\205\215\347\275\256", nullptr));
        triggerConfigCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        triggerModeLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\350\247\246\345\217\221\346\250\241\345\274\217 \342\223\230", nullptr));
        internalTriggerButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\206\205\351\203\250\350\247\246\345\217\221", nullptr));
        externalTriggerButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\244\226\351\203\250\350\247\246\345\217\221", nullptr));
        brightnessCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\344\272\256\345\272\246\350\256\276\347\275\256", nullptr));
        brightnessCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        autoAdjustLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\344\270\200\351\224\256\350\256\276\347\275\256", nullptr));
        autoAdjustButton->setText(QCoreApplication::translate("CameraParamsDialog", "\350\207\252\345\212\250\350\260\203\350\212\202", nullptr));
        focusCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\346\234\272\346\242\260\350\260\203\347\204\246", nullptr));
        focusCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        focusRegionLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\347\204\246\347\202\271\345\214\272\345\237\237", nullptr));
        focusRegionAutoButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\226\243", nullptr));
        focusRegionRectButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\226\241", nullptr));
        autoFocusLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\350\207\252\345\212\250\345\257\271\347\204\246", nullptr));
        autoFocusButton->setText(QCoreApplication::translate("CameraParamsDialog", "\350\207\252\345\212\250\345\257\271\347\204\246", nullptr));
        autoFocusResetButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\206\273", nullptr));
        allImagingAdjustCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\346\210\220\345\203\217\350\260\203\350\212\202", nullptr));
        allImagingAdjustCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        allImagingAdjustHintLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\344\270\200\351\224\256\350\260\203\350\212\202\347\233\270\346\234\272\344\272\256\345\272\246\343\200\201\347\231\275\345\271\263\350\241\241\343\200\201\345\257\271\347\204\246 \342\223\230", nullptr));
        allOneKeyTuneButton->setText(QCoreApplication::translate("CameraParamsDialog", "\344\270\200\351\224\256\350\260\203\350\260\220", nullptr));
        allTriggerConfigCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\350\247\246\345\217\221\351\205\215\347\275\256", nullptr));
        allTriggerConfigCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        allInternalTriggerButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\206\205\351\203\250\350\247\246\345\217\221", nullptr));
        allExternalTriggerButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\244\226\351\203\250\350\247\246\345\217\221", nullptr));
        runIntervalLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\350\277\236\347\273\255\350\277\220\350\241\214\351\227\264\351\232\224(ms) \342\223\230", nullptr));
        allTriggerModeLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\350\247\246\345\217\221\346\250\241\345\274\217 \342\223\230", nullptr));
        allBrightnessCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\344\272\256\345\272\246\350\256\276\347\275\256", nullptr));
        allBrightnessCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        exposureTimeLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\346\233\235\345\205\211\346\227\266\351\227\264(us)", nullptr));
        gainLineEdit->setText(QCoreApplication::translate("CameraParamsDialog", "0.00", nullptr));
        allAutoAdjustButton->setText(QCoreApplication::translate("CameraParamsDialog", "\350\207\252\345\212\250\350\260\203\350\212\202", nullptr));
        brightnessStandardLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\344\272\256\345\272\246\346\240\207\345\207\206", nullptr));
        gainLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\345\242\236\347\233\212(dB)", nullptr));
        allAutoAdjustLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\344\270\200\351\224\256\350\256\276\347\275\256", nullptr));
        allFocusCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\346\234\272\346\242\260\350\260\203\347\204\246", nullptr));
        allFocusCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        allAutoFocusLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\350\207\252\345\212\250\345\257\271\347\204\246", nullptr));
        focusStepLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\347\204\246\350\267\235\346\255\245\350\277\233", nullptr));
        allFocusRegionAutoButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\226\243", nullptr));
        allFocusRegionRectButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\226\241", nullptr));
        focusModeComboBox->setItemText(0, QCoreApplication::translate("CameraParamsDialog", "\345\277\253\351\200\237\350\260\203\347\204\246\346\250\241\345\274\217", nullptr));

        focusPositionLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\347\204\246\347\202\271\344\275\215\347\275\256", nullptr));
        allAutoFocusButton->setText(QCoreApplication::translate("CameraParamsDialog", "\350\207\252\345\212\250\345\257\271\347\204\246", nullptr));
        allAutoFocusResetButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\206\273", nullptr));
        allFocusRegionLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\347\204\246\347\202\271\345\214\272\345\237\237", nullptr));
        focusModeLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\350\260\203\347\204\246\346\250\241\345\274\217 \342\223\230", nullptr));
        lightControlCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\345\205\211\346\272\220\350\260\203\350\212\202", nullptr));
        lightControlCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        lightControlLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\345\205\211\346\272\220\346\216\247\345\210\266", nullptr));
        lightControlComboBox->setItemText(0, QCoreApplication::translate("CameraParamsDialog", "\350\207\252\345\212\250\350\275\256\350\257\242", nullptr));

        lightDeviceIconLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\342\227\217  \342\227\217\n"
" \342\227\211\n"
"\342\227\213  \342\227\213", nullptr));
        lightPositionLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\345\205\211\346\272\220\344\275\215\347\275\256", nullptr));
        lightPrevButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\200\271", nullptr));
        lightPositionValueLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\350\207\252\345\212\250\350\275\256\350\257\242", nullptr));
        lightNextButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\200\272", nullptr));
        otherParamsCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\345\205\266\345\256\203\345\217\202\346\225\260", nullptr));
        otherParamsCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        sharpenLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\351\224\220\345\214\226", nullptr));
        xMirrorCheckBox->setText(QString());
        xMirrorLabel->setText(QCoreApplication::translate("CameraParamsDialog", "X\351\225\234\345\203\217", nullptr));
        yMirrorCheckBox->setText(QString());
        yMirrorLabel->setText(QCoreApplication::translate("CameraParamsDialog", "Y\351\225\234\345\203\217", nullptr));
        contrastLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\345\257\271\346\257\224\345\272\246", nullptr));
        imageOriginalButton->setText(QCoreApplication::translate("CameraParamsDialog", "\345\216\237\345\247\213", nullptr));
        imageCustomButton->setText(QCoreApplication::translate("CameraParamsDialog", "\350\207\252\345\256\232\344\271\211", nullptr));
        gammaLineEdit->setText(QCoreApplication::translate("CameraParamsDialog", "0.70", nullptr));
        gammaLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\344\274\275\351\251\254\345\200\274", nullptr));
        grayOutputLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\350\276\223\345\207\272\347\201\260\345\272\246\345\233\276", nullptr));
        grayOutputCheckBox->setText(QString());
        encodeQualityLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\347\274\226\347\240\201\350\264\250\351\207\217", nullptr));
        imageSizeLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\345\233\276\345\203\217\345\244\247\345\260\217", nullptr));
        colorParamsCardTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\345\275\251\350\211\262\345\233\276\345\203\217\345\217\202\346\225\260", nullptr));
        colorParamsCardCollapseButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\214\204", nullptr));
        whiteBalanceLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\347\231\275\345\271\263\350\241\241\344\275\277\350\203\275", nullptr));
        whiteBalanceCheckBox->setText(QString());
        colorAutoAdjustLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\344\270\200\351\224\256\350\256\276\347\275\256", nullptr));
        colorAutoAdjustButton->setText(QCoreApplication::translate("CameraParamsDialog", "\350\207\252\345\212\250\350\260\203\350\212\202", nullptr));
        whiteBalanceParamsLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\347\231\275\345\271\263\350\241\241\345\217\202\346\225\260 \342\223\230", nullptr));
        whiteBalanceEditButton->setText(QCoreApplication::translate("CameraParamsDialog", "\347\274\226\350\276\221", nullptr));
        colorBalanceTableCell00->setText(QCoreApplication::translate("CameraParamsDialog", "R", nullptr));
        colorBalanceTableCell01->setText(QCoreApplication::translate("CameraParamsDialog", "G", nullptr));
        colorBalanceTableCell02->setText(QCoreApplication::translate("CameraParamsDialog", "B", nullptr));
        colorBalanceTableCell10->setText(QCoreApplication::translate("CameraParamsDialog", "349", nullptr));
        colorBalanceTableCell11->setText(QCoreApplication::translate("CameraParamsDialog", "256", nullptr));
        colorBalanceTableCell12->setText(QCoreApplication::translate("CameraParamsDialog", "641", nullptr));
        ccmResetLabel->setText(QCoreApplication::translate("CameraParamsDialog", "CCM\351\207\215\347\275\256", nullptr));
        ccmResetButton->setText(QCoreApplication::translate("CameraParamsDialog", "\351\207\215\347\275\256", nullptr));
        ccmParamsLabel->setText(QCoreApplication::translate("CameraParamsDialog", "CCM\345\217\202\346\225\260 \342\223\230", nullptr));
        ccmEditButton->setText(QCoreApplication::translate("CameraParamsDialog", "\347\274\226\350\276\221", nullptr));
        ccmTableCell00->setText(QString());
        ccmTableCell01->setText(QCoreApplication::translate("CameraParamsDialog", "R", nullptr));
        ccmTableCell02->setText(QCoreApplication::translate("CameraParamsDialog", "G", nullptr));
        ccmTableCell03->setText(QCoreApplication::translate("CameraParamsDialog", "B", nullptr));
        ccmTableCell10->setText(QCoreApplication::translate("CameraParamsDialog", "R", nullptr));
        ccmTableCell11->setText(QCoreApplication::translate("CameraParamsDialog", "1.002", nullptr));
        ccmTableCell12->setText(QCoreApplication::translate("CameraParamsDialog", "0.113", nullptr));
        ccmTableCell13->setText(QCoreApplication::translate("CameraParamsDialog", "-0.115", nullptr));
        ccmTableCell20->setText(QCoreApplication::translate("CameraParamsDialog", "G", nullptr));
        ccmTableCell21->setText(QCoreApplication::translate("CameraParamsDialog", "-0.482", nullptr));
        ccmTableCell22->setText(QCoreApplication::translate("CameraParamsDialog", "1.712", nullptr));
        ccmTableCell23->setText(QCoreApplication::translate("CameraParamsDialog", "-0.229", nullptr));
        ccmTableCell30->setText(QCoreApplication::translate("CameraParamsDialog", "B", nullptr));
        ccmTableCell31->setText(QCoreApplication::translate("CameraParamsDialog", "0.010", nullptr));
        ccmTableCell32->setText(QCoreApplication::translate("CameraParamsDialog", "-0.701", nullptr));
        ccmTableCell33->setText(QCoreApplication::translate("CameraParamsDialog", "1.691", nullptr));
        previousButton->setText(QCoreApplication::translate("CameraParamsDialog", "\342\200\271 \344\270\212\344\270\200\346\255\245", nullptr));
        nextButton->setText(QCoreApplication::translate("CameraParamsDialog", "\344\270\213\344\270\200\346\255\245 \342\200\272", nullptr));
        viewerTitleLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\347\233\270\346\234\272\345\233\276\345\203\217", nullptr));
        viewerGridButton->setText(QString());
        viewerZoomSearchButton->setText(QString());
        viewerZoomOutButton->setText(QCoreApplication::translate("CameraParamsDialog", "-", nullptr));
        viewerZoomLabel->setText(QCoreApplication::translate("CameraParamsDialog", "39%", nullptr));
        viewerZoomInButton->setText(QString());
        viewerFullButton->setText(QString());
        viewerStatusLabel->setText(QCoreApplication::translate("CameraParamsDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: 0ms   \345\267\245\345\205\267\350\200\227\346\227\266: 0ms", nullptr));
        viewerCursorLabel->setText(QCoreApplication::translate("CameraParamsDialog", "X: --  Y: --   |   R: --  G: --  B: --", nullptr));
    } // retranslateUi

};

namespace Ui {
    class CameraParamsDialog: public Ui_CameraParamsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CAMERAPARAMSDIALOG_H
