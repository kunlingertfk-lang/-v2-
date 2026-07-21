/********************************************************************************
** Form generated from reading UI file 'PositionCorrectionDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_POSITIONCORRECTIONDIALOG_H
#define UI_POSITIONCORRECTIONDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_PositionCorrectionDialog
{
public:
    QVBoxLayout *rootLayout;
    QFrame *titleBar;
    QHBoxLayout *titleLayout;
    QLabel *windowTitleLabel;
    QSpacerItem *titleSpacer;
    QToolButton *closeButton;
    QHBoxLayout *bodyLayout;
    QFrame *parameterPanel;
    QVBoxLayout *parameterLayout;
    QHBoxLayout *modeLayout;
    QPushButton *basicModeButton;
    QPushButton *allModeButton;
    QFrame *subscriptionCard;
    QGridLayout *subscriptionLayout;
    QLabel *subscriptionTitle;
    QLabel *runXLabel;
    QLineEdit *runPointXEdit;
    QPushButton *runPointXLinkButton;
    QLabel *runYLabel;
    QLineEdit *runPointYEdit;
    QPushButton *runPointYLinkButton;
    QLabel *runAngleLabel;
    QLineEdit *runAngleEdit;
    QPushButton *runAngleLinkButton;
    QLabel *createReferenceLabel;
    QPushButton *createReferenceButton;
    QFrame *templateCard;
    QVBoxLayout *templateLayout;
    QLabel *templateTitle;
    QHBoxLayout *templateToolsLayout;
    QLabel *templateField;
    QPushButton *rectTemplateButton;
    QPushButton *polygonTemplateButton;
    QSpacerItem *templateSpacer;
    QSpacerItem *parameterSpacer;
    QLabel *statusLabel;
    QHBoxLayout *actionLayout;
    QPushButton *testRunButton;
    QPushButton *finishButton;
    QFrame *previewPanel;
    QVBoxLayout *previewLayout;
    QFrame *viewerHeader;
    QHBoxLayout *viewerHeaderLayout;
    QLabel *viewerTitleLabel;
    QSpacerItem *viewerHeaderSpacer;
    QGraphicsView *previewGraphicsView;
    QFrame *viewerStatusBar;
    QHBoxLayout *viewerStatusLayout;
    QLabel *viewerStatusLabel;
    QSpacerItem *viewerStatusSpacer;

    void setupUi(QDialog *PositionCorrectionDialog)
    {
        if (PositionCorrectionDialog->objectName().isEmpty())
            PositionCorrectionDialog->setObjectName(QString::fromUtf8("PositionCorrectionDialog"));
        PositionCorrectionDialog->resize(1320, 820);
        rootLayout = new QVBoxLayout(PositionCorrectionDialog);
        rootLayout->setSpacing(0);
        rootLayout->setObjectName(QString::fromUtf8("rootLayout"));
        rootLayout->setContentsMargins(0, 0, 0, 0);
        titleBar = new QFrame(PositionCorrectionDialog);
        titleBar->setObjectName(QString::fromUtf8("titleBar"));
        titleBar->setMinimumSize(QSize(0, 54));
        titleLayout = new QHBoxLayout(titleBar);
        titleLayout->setObjectName(QString::fromUtf8("titleLayout"));
        windowTitleLabel = new QLabel(titleBar);
        windowTitleLabel->setObjectName(QString::fromUtf8("windowTitleLabel"));

        titleLayout->addWidget(windowTitleLabel);

        titleSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        titleLayout->addItem(titleSpacer);

        closeButton = new QToolButton(titleBar);
        closeButton->setObjectName(QString::fromUtf8("closeButton"));

        titleLayout->addWidget(closeButton);


        rootLayout->addWidget(titleBar);

        bodyLayout = new QHBoxLayout();
        bodyLayout->setSpacing(0);
        bodyLayout->setObjectName(QString::fromUtf8("bodyLayout"));
        parameterPanel = new QFrame(PositionCorrectionDialog);
        parameterPanel->setObjectName(QString::fromUtf8("parameterPanel"));
        parameterPanel->setMinimumSize(QSize(430, 0));
        parameterPanel->setMaximumSize(QSize(520, 16777215));
        parameterLayout = new QVBoxLayout(parameterPanel);
        parameterLayout->setObjectName(QString::fromUtf8("parameterLayout"));
        modeLayout = new QHBoxLayout();
        modeLayout->setObjectName(QString::fromUtf8("modeLayout"));
        basicModeButton = new QPushButton(parameterPanel);
        basicModeButton->setObjectName(QString::fromUtf8("basicModeButton"));
        basicModeButton->setCheckable(true);
        basicModeButton->setChecked(true);

        modeLayout->addWidget(basicModeButton);

        allModeButton = new QPushButton(parameterPanel);
        allModeButton->setObjectName(QString::fromUtf8("allModeButton"));
        allModeButton->setCheckable(true);

        modeLayout->addWidget(allModeButton);


        parameterLayout->addLayout(modeLayout);

        subscriptionCard = new QFrame(parameterPanel);
        subscriptionCard->setObjectName(QString::fromUtf8("subscriptionCard"));
        subscriptionLayout = new QGridLayout(subscriptionCard);
        subscriptionLayout->setObjectName(QString::fromUtf8("subscriptionLayout"));
        subscriptionLayout->setHorizontalSpacing(6);
        subscriptionLayout->setVerticalSpacing(22);
        subscriptionTitle = new QLabel(subscriptionCard);
        subscriptionTitle->setObjectName(QString::fromUtf8("subscriptionTitle"));

        subscriptionLayout->addWidget(subscriptionTitle, 0, 0, 1, 3);

        runXLabel = new QLabel(subscriptionCard);
        runXLabel->setObjectName(QString::fromUtf8("runXLabel"));

        subscriptionLayout->addWidget(runXLabel, 1, 0, 1, 1);

        runPointXEdit = new QLineEdit(subscriptionCard);
        runPointXEdit->setObjectName(QString::fromUtf8("runPointXEdit"));
        runPointXEdit->setReadOnly(true);

        subscriptionLayout->addWidget(runPointXEdit, 1, 1, 1, 1);

        runPointXLinkButton = new QPushButton(subscriptionCard);
        runPointXLinkButton->setObjectName(QString::fromUtf8("runPointXLinkButton"));

        subscriptionLayout->addWidget(runPointXLinkButton, 1, 2, 1, 1);

        runYLabel = new QLabel(subscriptionCard);
        runYLabel->setObjectName(QString::fromUtf8("runYLabel"));

        subscriptionLayout->addWidget(runYLabel, 2, 0, 1, 1);

        runPointYEdit = new QLineEdit(subscriptionCard);
        runPointYEdit->setObjectName(QString::fromUtf8("runPointYEdit"));
        runPointYEdit->setReadOnly(true);

        subscriptionLayout->addWidget(runPointYEdit, 2, 1, 1, 1);

        runPointYLinkButton = new QPushButton(subscriptionCard);
        runPointYLinkButton->setObjectName(QString::fromUtf8("runPointYLinkButton"));

        subscriptionLayout->addWidget(runPointYLinkButton, 2, 2, 1, 1);

        runAngleLabel = new QLabel(subscriptionCard);
        runAngleLabel->setObjectName(QString::fromUtf8("runAngleLabel"));

        subscriptionLayout->addWidget(runAngleLabel, 3, 0, 1, 1);

        runAngleEdit = new QLineEdit(subscriptionCard);
        runAngleEdit->setObjectName(QString::fromUtf8("runAngleEdit"));
        runAngleEdit->setReadOnly(true);

        subscriptionLayout->addWidget(runAngleEdit, 3, 1, 1, 1);

        runAngleLinkButton = new QPushButton(subscriptionCard);
        runAngleLinkButton->setObjectName(QString::fromUtf8("runAngleLinkButton"));

        subscriptionLayout->addWidget(runAngleLinkButton, 3, 2, 1, 1);

        createReferenceLabel = new QLabel(subscriptionCard);
        createReferenceLabel->setObjectName(QString::fromUtf8("createReferenceLabel"));

        subscriptionLayout->addWidget(createReferenceLabel, 4, 0, 1, 1);

        createReferenceButton = new QPushButton(subscriptionCard);
        createReferenceButton->setObjectName(QString::fromUtf8("createReferenceButton"));

        subscriptionLayout->addWidget(createReferenceButton, 4, 1, 1, 2);


        parameterLayout->addWidget(subscriptionCard);

        templateCard = new QFrame(parameterPanel);
        templateCard->setObjectName(QString::fromUtf8("templateCard"));
        templateLayout = new QVBoxLayout(templateCard);
        templateLayout->setObjectName(QString::fromUtf8("templateLayout"));
        templateTitle = new QLabel(templateCard);
        templateTitle->setObjectName(QString::fromUtf8("templateTitle"));

        templateLayout->addWidget(templateTitle);

        templateToolsLayout = new QHBoxLayout();
        templateToolsLayout->setObjectName(QString::fromUtf8("templateToolsLayout"));
        templateField = new QLabel(templateCard);
        templateField->setObjectName(QString::fromUtf8("templateField"));

        templateToolsLayout->addWidget(templateField);

        rectTemplateButton = new QPushButton(templateCard);
        rectTemplateButton->setObjectName(QString::fromUtf8("rectTemplateButton"));
        rectTemplateButton->setCheckable(true);

        templateToolsLayout->addWidget(rectTemplateButton);

        polygonTemplateButton = new QPushButton(templateCard);
        polygonTemplateButton->setObjectName(QString::fromUtf8("polygonTemplateButton"));
        polygonTemplateButton->setCheckable(true);

        templateToolsLayout->addWidget(polygonTemplateButton);


        templateLayout->addLayout(templateToolsLayout);


        parameterLayout->addWidget(templateCard);

        templateSpacer = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        parameterLayout->addItem(templateSpacer);

        parameterSpacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        parameterLayout->addItem(parameterSpacer);

        statusLabel = new QLabel(parameterPanel);
        statusLabel->setObjectName(QString::fromUtf8("statusLabel"));
        statusLabel->setWordWrap(true);
        statusLabel->setProperty("hint", QVariant(true));

        parameterLayout->addWidget(statusLabel);

        actionLayout = new QHBoxLayout();
        actionLayout->setObjectName(QString::fromUtf8("actionLayout"));
        testRunButton = new QPushButton(parameterPanel);
        testRunButton->setObjectName(QString::fromUtf8("testRunButton"));

        actionLayout->addWidget(testRunButton);

        finishButton = new QPushButton(parameterPanel);
        finishButton->setObjectName(QString::fromUtf8("finishButton"));

        actionLayout->addWidget(finishButton);


        parameterLayout->addLayout(actionLayout);


        bodyLayout->addWidget(parameterPanel);

        previewPanel = new QFrame(PositionCorrectionDialog);
        previewPanel->setObjectName(QString::fromUtf8("previewPanel"));
        previewLayout = new QVBoxLayout(previewPanel);
        previewLayout->setSpacing(0);
        previewLayout->setObjectName(QString::fromUtf8("previewLayout"));
        previewLayout->setContentsMargins(0, 0, 0, 0);
        viewerHeader = new QFrame(previewPanel);
        viewerHeader->setObjectName(QString::fromUtf8("viewerHeader"));
        viewerHeader->setMinimumSize(QSize(0, 44));
        viewerHeader->setMaximumSize(QSize(16777215, 44));
        viewerHeaderLayout = new QHBoxLayout(viewerHeader);
        viewerHeaderLayout->setObjectName(QString::fromUtf8("viewerHeaderLayout"));
        viewerHeaderLayout->setContentsMargins(16, -1, 16, -1);
        viewerTitleLabel = new QLabel(viewerHeader);
        viewerTitleLabel->setObjectName(QString::fromUtf8("viewerTitleLabel"));

        viewerHeaderLayout->addWidget(viewerTitleLabel);

        viewerHeaderSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        viewerHeaderLayout->addItem(viewerHeaderSpacer);


        previewLayout->addWidget(viewerHeader);

        previewGraphicsView = new QGraphicsView(previewPanel);
        previewGraphicsView->setObjectName(QString::fromUtf8("previewGraphicsView"));

        previewLayout->addWidget(previewGraphicsView);

        viewerStatusBar = new QFrame(previewPanel);
        viewerStatusBar->setObjectName(QString::fromUtf8("viewerStatusBar"));
        viewerStatusBar->setMinimumSize(QSize(0, 32));
        viewerStatusBar->setMaximumSize(QSize(16777215, 32));
        viewerStatusLayout = new QHBoxLayout(viewerStatusBar);
        viewerStatusLayout->setObjectName(QString::fromUtf8("viewerStatusLayout"));
        viewerStatusLayout->setContentsMargins(16, -1, 16, -1);
        viewerStatusLabel = new QLabel(viewerStatusBar);
        viewerStatusLabel->setObjectName(QString::fromUtf8("viewerStatusLabel"));

        viewerStatusLayout->addWidget(viewerStatusLabel);

        viewerStatusSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        viewerStatusLayout->addItem(viewerStatusSpacer);


        previewLayout->addWidget(viewerStatusBar);


        bodyLayout->addWidget(previewPanel);


        rootLayout->addLayout(bodyLayout);


        retranslateUi(PositionCorrectionDialog);

        QMetaObject::connectSlotsByName(PositionCorrectionDialog);
    } // setupUi

    void retranslateUi(QDialog *PositionCorrectionDialog)
    {
        PositionCorrectionDialog->setWindowTitle(QCoreApplication::translate("PositionCorrectionDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        windowTitleLabel->setText(QCoreApplication::translate("PositionCorrectionDialog", "\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
        windowTitleLabel->setProperty("role", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "windowTitle", nullptr)));
#if QT_CONFIG(tooltip)
        closeButton->setToolTip(QCoreApplication::translate("PositionCorrectionDialog", "\345\205\263\351\227\255\344\275\215\347\275\256\344\277\256\346\255\243", nullptr));
#endif // QT_CONFIG(tooltip)
        closeButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\303\227", nullptr));
        parameterPanel->setProperty("panelRole", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "parameterPanel", nullptr)));
        basicModeButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\345\237\272\347\241\200", nullptr));
        allModeButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\345\205\250\351\203\250", nullptr));
        subscriptionCard->setProperty("panelRole", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "configCard", nullptr)));
        subscriptionTitle->setText(QCoreApplication::translate("PositionCorrectionDialog", "\350\256\242\351\230\205\345\237\272\345\207\206\350\214\203\345\233\264", nullptr));
        subscriptionTitle->setProperty("role", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "cardTitle", nullptr)));
        runXLabel->setText(QCoreApplication::translate("PositionCorrectionDialog", "\350\277\220\350\241\214\347\202\271\345\235\220\346\240\207X", nullptr));
        runXLabel->setProperty("role", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "rowField", nullptr)));
#if QT_CONFIG(tooltip)
        runPointXLinkButton->setToolTip(QCoreApplication::translate("PositionCorrectionDialog", "\351\200\211\346\213\251\350\277\220\350\241\214\347\202\271\345\235\220\346\240\207X\346\235\245\346\272\220", nullptr));
#endif // QT_CONFIG(tooltip)
        runPointXLinkButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\351\223\276\346\216\245", nullptr));
        runYLabel->setText(QCoreApplication::translate("PositionCorrectionDialog", "\350\277\220\350\241\214\347\202\271\345\235\220\346\240\207Y", nullptr));
        runYLabel->setProperty("role", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "rowField", nullptr)));
#if QT_CONFIG(tooltip)
        runPointYLinkButton->setToolTip(QCoreApplication::translate("PositionCorrectionDialog", "\351\200\211\346\213\251\350\277\220\350\241\214\347\202\271\345\235\220\346\240\207Y\346\235\245\346\272\220", nullptr));
#endif // QT_CONFIG(tooltip)
        runPointYLinkButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\351\223\276\346\216\245", nullptr));
        runAngleLabel->setText(QCoreApplication::translate("PositionCorrectionDialog", "\350\277\220\350\241\214\350\247\222\345\272\246", nullptr));
        runAngleLabel->setProperty("role", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "rowField", nullptr)));
#if QT_CONFIG(tooltip)
        runAngleLinkButton->setToolTip(QCoreApplication::translate("PositionCorrectionDialog", "\351\200\211\346\213\251\350\277\220\350\241\214\350\247\222\345\272\246\346\235\245\346\272\220", nullptr));
#endif // QT_CONFIG(tooltip)
        runAngleLinkButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\351\223\276\346\216\245", nullptr));
        createReferenceLabel->setText(QCoreApplication::translate("PositionCorrectionDialog", "\345\210\233\345\273\272\345\237\272\345\207\206", nullptr));
        createReferenceLabel->setProperty("role", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "rowField", nullptr)));
        createReferenceButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\345\210\233\345\273\272\345\237\272\345\207\206", nullptr));
        templateCard->setProperty("panelRole", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "configCard", nullptr)));
        templateTitle->setText(QCoreApplication::translate("PositionCorrectionDialog", "\346\250\241\346\235\277\345\214\272\345\237\237\350\256\276\347\275\256", nullptr));
        templateTitle->setProperty("role", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "cardTitle", nullptr)));
        templateField->setText(QCoreApplication::translate("PositionCorrectionDialog", "\346\250\241\346\235\277\345\214\272\345\237\237", nullptr));
        rectTemplateButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\347\237\251\345\275\242", nullptr));
        polygonTemplateButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\345\244\232\350\276\271\345\275\242", nullptr));
        statusLabel->setText(QCoreApplication::translate("PositionCorrectionDialog", "\351\205\215\347\275\256\345\267\262\344\277\235\345\255\230\357\274\214\345\260\232\346\234\252\346\265\213\350\257\225", nullptr));
        testRunButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\346\265\213\350\257\225\350\277\220\350\241\214", nullptr));
        finishButton->setText(QCoreApplication::translate("PositionCorrectionDialog", "\345\256\214\346\210\220", nullptr));
        finishButton->setProperty("actionRole", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "primary", nullptr)));
        previewPanel->setProperty("panelRole", QVariant(QCoreApplication::translate("PositionCorrectionDialog", "previewPanel", nullptr)));
        viewerTitleLabel->setText(QCoreApplication::translate("PositionCorrectionDialog", "\345\237\272\345\207\206\345\233\276", nullptr));
        viewerStatusLabel->setText(QCoreApplication::translate("PositionCorrectionDialog", "\347\256\227\346\263\225\350\200\227\346\227\266: 0ms  \345\267\245\345\205\267\350\200\227\346\227\266: 0ms", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PositionCorrectionDialog: public Ui_PositionCorrectionDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_POSITIONCORRECTIONDIALOG_H
