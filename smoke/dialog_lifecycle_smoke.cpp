#include "PlanDialogUtils.h"

#include <QApplication>
#include <QDialog>
#include <QFrame>
#include <QGraphicsView>
#include <QScreen>
#include <QTextStream>
#include <QTimer>

class FakeSetupHost : public QDialog
{
    Q_OBJECT

public:
    explicit FakeSetupHost(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setProperty("schemeSetupHost", true);
    }

    QString currentPage;
    int switchCount = 0;

public slots:
    bool showSetupPage(const QString &pageId)
    {
        currentPage = pageId;
        ++switchCount;
        return !pageId.isEmpty();
    }
};

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

    FakeSetupHost setupHost;
    setupHost.setObjectName(QStringLiteral("SchemeSetupWindow"));
    QWidget stackContainer(&setupHost);
    QDialog embeddedPage(&stackContainer);
    embeddedPage.setObjectName(QStringLiteral("CameraParamsDialog"));
    PlanDialogUtils::configureDialogWindow(&embeddedPage, QStringLiteral("embedded page"));
    const int topLevelsBefore = QApplication::topLevelWidgets().size();
    const bool switched = PlanDialogUtils::switchEmbeddedSetupPage(
                &embeddedPage, QStringLiteral("reference"));
    const int topLevelsAfter = QApplication::topLevelWidgets().size();
    if (!switched
            || embeddedPage.isWindow()
            || embeddedPage.windowFlags().testFlag(Qt::Window)
            || setupHost.currentPage != QStringLiteral("reference")
            || setupHost.switchCount != 1
            || topLevelsAfter != topLevelsBefore) {
        QTextStream(stderr) << "FAIL: embedded setup-page switching created a top-level window\n";
        return 1;
    }

    QDialog nestedToolDialog(&embeddedPage);
    nestedToolDialog.setObjectName(QStringLiteral("CirclePresenceDialog"));
    PlanDialogUtils::configureDialogWindow(
                &nestedToolDialog, QStringLiteral("tool dialog isolation"));
    if (!nestedToolDialog.isWindow()
            || !nestedToolDialog.windowFlags().testFlag(Qt::Window)) {
        QTextStream(stderr) << "FAIL: tool dialog was embedded into the setup-page stack\n";
        return 1;
    }

    QTextStream(stdout) << "PASS: dialog lifecycle, embedded setup and tool-style smoke\n";
    return 0;
}

#include "dialog_lifecycle_smoke.moc"
