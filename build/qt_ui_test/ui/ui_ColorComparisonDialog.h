/********************************************************************************
** Form generated from reading UI file 'ColorComparisonDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_COLORCOMPARISONDIALOG_H
#define UI_COLORCOMPARISONDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QToolButton>

QT_BEGIN_NAMESPACE

class Ui_ColorComparisonDialog
{
public:
    QToolButton *toolButton;

    void setupUi(QDialog *ColorComparisonDialog)
    {
        if (ColorComparisonDialog->objectName().isEmpty())
            ColorComparisonDialog->setObjectName(QString::fromUtf8("ColorComparisonDialog"));
        ColorComparisonDialog->resize(1280, 800);
        toolButton = new QToolButton(ColorComparisonDialog);
        toolButton->setObjectName(QString::fromUtf8("toolButton"));
        toolButton->setGeometry(QRect(200, 420, 26, 24));

        retranslateUi(ColorComparisonDialog);

        QMetaObject::connectSlotsByName(ColorComparisonDialog);
    } // setupUi

    void retranslateUi(QDialog *ColorComparisonDialog)
    {
        ColorComparisonDialog->setWindowTitle(QCoreApplication::translate("ColorComparisonDialog", "\346\226\271\346\241\210\347\274\226\350\276\221 - \351\242\234\350\211\262\346\257\224\350\276\203", nullptr));
        toolButton->setText(QCoreApplication::translate("ColorComparisonDialog", "...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ColorComparisonDialog: public Ui_ColorComparisonDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_COLORCOMPARISONDIALOG_H
