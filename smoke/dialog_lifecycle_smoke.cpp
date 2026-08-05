#include "PlanDialogUtils.h"

#include <QApplication>
#include <QDialog>
#include <QFrame>
#include <QGraphicsView>
#include <QScreen>
#include <QTextStream>
#include <QTimer>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QDialog dialog;
    PlanDialogUtils::configureDialogWindow(&dialog, QStringLiteral("lifecycle smoke"));
    if (dialog.testAttribute(Qt::WA_DeleteOnClose)) {
        QTextStream(stderr) << "FAIL: modal stack dialog has WA_DeleteOnClose\n";
        return 1;
    }

    QTimer::singleShot(0, &dialog, &QDialog::accept);
    if (dialog.exec() != QDialog::Accepted) {
        QTextStream(stderr) << "FAIL: dialog did not complete through accept()\n";
        return 1;
    }

    // Accessing the dialog after exec() is the exact caller behavior that used
    // to crash when WA_DeleteOnClose deleted the stack object.
    dialog.setWindowTitle(QStringLiteral("still alive"));
    if (dialog.windowTitle() != QStringLiteral("still alive")) {
        QTextStream(stderr) << "FAIL: dialog is not usable after exec()\n";
        return 1;
    }

    QDialog toolDialog;
    QFrame parameterPanel(&toolDialog);
    parameterPanel.setObjectName(QStringLiteral("parameterPanel"));
    QFrame previewPanel(&toolDialog);
    previewPanel.setObjectName(QStringLiteral("previewPanel"));
    QGraphicsView previewGraphicsView(&previewPanel);
    QFrame titleBar(&toolDialog);
    titleBar.setObjectName(QStringLiteral("titleBar"));
    PlanDialogUtils::applyToolLevelStyle(&toolDialog);
    const int availableWidth = toolDialog.screen()
            ? toolDialog.screen()->availableGeometry().width()
            : toolDialog.width();
    const int expectedParameterWidth = qBound(
                520,
                qRound(static_cast<qreal>(availableWidth) * 0.32),
                610);
    if (!toolDialog.property("toolLevelStyle").toBool()
            || parameterPanel.minimumWidth() != expectedParameterWidth
            || parameterPanel.maximumWidth() != expectedParameterWidth
            || previewPanel.minimumWidth() != 0
            || previewPanel.maximumWidth() != QWIDGETSIZE_MAX
            || previewPanel.sizePolicy().horizontalPolicy() != QSizePolicy::Expanding
            || previewGraphicsView.minimumSize() != QSize(0, 0)
            || previewGraphicsView.sizePolicy().horizontalPolicy() != QSizePolicy::Expanding
            || titleBar.minimumHeight() != 54
            || titleBar.maximumHeight() != 54) {
        QTextStream(stderr) << "FAIL: tool-level layout standard was not applied\n";
        return 1;
    }

    QTextStream(stdout) << "PASS: dialog lifecycle and tool-style smoke\n";
    return 0;
}
