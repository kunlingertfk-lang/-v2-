#include "PlanDialogUtils.h"

#include <QByteArray>
#include <QDebug>
#include <QDialog>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QWidget>

#include "MainWindow.h"
#include "WindowUtils.h"

namespace {

constexpr auto kSessionDeviceNameProperty = "sessionDeviceName";
constexpr auto kSessionUserNameProperty = "sessionUserName";

void copySessionInfo(QWidget *source, QWidget *target)
{
    if (!source || !target) {
        return;
    }

    target->setProperty(kSessionDeviceNameProperty, source->property(kSessionDeviceNameProperty));
    target->setProperty(kSessionUserNameProperty, source->property(kSessionUserNameProperty));
}

QWidget *findFirstWidget(QWidget *root, const QList<QByteArray> &names)
{
    if (!root) {
        return nullptr;
    }

    for (const QByteArray &name : names) {
        if (QWidget *widget = root->findChild<QWidget *>(QLatin1String(name.constData()))) {
            return widget;
        }
    }

    return nullptr;
}

void logWidgetSize(const QString &prefix, QWidget *root, const QString &label, const QList<QByteArray> &names)
{
    QWidget *widget = findFirstWidget(root, names);
    if (!widget) {
        qDebug().noquote() << prefix << label << "<missing>";
        return;
    }

    qDebug().noquote() << prefix
                       << label
                       << "size=" << widget->size()
                       << "min=" << widget->minimumSize()
                       << "max=" << widget->maximumSize();
}

void logSetupDialogSize(QWidget *window)
{
    if (!window) {
        return;
    }

    const QString objectName = window->objectName();
    const bool isTools = objectName == QLatin1String("ToolsDialog");
    const bool isOutput = objectName == QLatin1String("OutputDialog");
    if (!isTools && !isOutput) {
        return;
    }

    static bool toolsLogged = false;
    static bool outputLogged = false;
    bool &logged = isTools ? toolsLogged : outputLogged;
    if (logged) {
        return;
    }
    logged = true;

    const QString prefix = isTools ? QStringLiteral("[ToolsDialogSize]")
                                   : QStringLiteral("[OutputDialogSize]");
    qDebug().noquote() << prefix
                       << "window"
                       << "size=" << window->size()
                       << "min=" << window->minimumSize()
                       << "max=" << window->maximumSize()
                       << "geometry=" << window->geometry()
                       << "windowState=" << static_cast<int>(window->windowState());
    logWidgetSize(prefix, window, QStringLiteral("setupBodyFrame"), {QByteArrayLiteral("setupBodyFrame")});
    logWidgetSize(prefix, window, QStringLiteral("setupEditorPanel"), {QByteArrayLiteral("setupEditorPanel")});
    logWidgetSize(prefix, window, QStringLiteral("setupViewerFrame"), {QByteArrayLiteral("setupViewerFrame")});
    logWidgetSize(prefix, window, QStringLiteral("previewGraphicsView"), {QByteArrayLiteral("previewGraphicsView"),
                                                                          QByteArrayLiteral("camera_1")});
    logWidgetSize(prefix, window, QStringLiteral("scrollArea"), {QByteArrayLiteral("toolsScrollArea"),
                                                                 QByteArrayLiteral("outputScrollArea"),
                                                                 QByteArrayLiteral("referenceParamsScrollArea"),
                                                                 QByteArrayLiteral("basicParamsScrollArea")});
    logWidgetSize(prefix, window, QStringLiteral("contentWidget"), {QByteArrayLiteral("scrollAreaWidgetContents"),
                                                                    QByteArrayLiteral("outputScrollAreaWidgetContents"),
                                                                    QByteArrayLiteral("referenceParamsContents"),
                                                                    QByteArrayLiteral("basicParamsContents")});
}

void scheduleSetupDialogSizeLog(QWidget *window)
{
    if (!window) {
        return;
    }

    const QString objectName = window->objectName();
    if (objectName != QLatin1String("ToolsDialog") &&
        objectName != QLatin1String("OutputDialog")) {
        return;
    }

    QPointer<QWidget> guarded(window);
    QTimer::singleShot(0, window, [guarded]() {
        if (guarded) {
            logSetupDialogSize(guarded.data());
        }
    });
}

} // namespace

void PlanDialogUtils::applyConfiguredWindowState(QWidget *window)
{
    if (!window) {
        return;
    }

    WindowUtils::fitWindowToScreen(window, 0);
    if (window->windowState().testFlag(Qt::WindowMaximized)) {
        window->setWindowState(window->windowState() | Qt::WindowMaximized);
    }
}

void PlanDialogUtils::configureDialogWindow(QDialog *dialog, const QString &title)
{
    if (!dialog) {
        return;
    }

    dialog->setWindowTitle(title);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowFlag(Qt::Window, true);
    WindowUtils::applyLargeWindow(dialog);
    scheduleSetupDialogSizeLog(dialog);

}

void PlanDialogUtils::connectWindowButtons(QWidget *window,
                                           QToolButton *closeButton,
                                           bool returnToMainWindow)
{
    if (!closeButton) {
        return;
    }

    QObject::connect(closeButton, &QToolButton::clicked, window, [window, returnToMainWindow]() {
        if (!window) {
            return;
        }

        if (returnToMainWindow) {
            PlanDialogUtils::returnToMainWindow(window);
            return;
        }

        window->close();
    });
}

void PlanDialogUtils::showWindowFromWidget(QWidget *source, QWidget *target)
{
    if (!target) {
        return;
    }

    copySessionInfo(source, target);
    WindowUtils::fitWindowToScreen(target, 0);

    if (WindowUtils::isLargeWindow(target)
            || target->windowState().testFlag(Qt::WindowMaximized)) {
        target->showMaximized();
    } else {
        WindowUtils::centerWindowOnScreen(target, source, 0);
        target->show();
    }

    WindowUtils::clampWindowToAvailableGeometry(target, 0);
    target->raise();
    target->activateWindow();
}

void PlanDialogUtils::showDialogFromWidget(QWidget *source, QDialog *dialog)
{
    showWindowFromWidget(source, dialog);
}

void PlanDialogUtils::replaceDialog(QWidget *current, QDialog *next)
{
    showDialogFromWidget(current, next);

    if (current) {
        current->close();
    }
}

void PlanDialogUtils::setSessionInfo(QWidget *window, const QString &deviceName, const QString &userName)
{
    if (!window) {
        return;
    }

    window->setProperty(kSessionDeviceNameProperty, deviceName);
    window->setProperty(kSessionUserNameProperty, userName);
}

void PlanDialogUtils::returnToMainWindow(QWidget *source)
{
    auto *mainWindow = new MainWindow;
    mainWindow->setAttribute(Qt::WA_DeleteOnClose);
    copySessionInfo(source, mainWindow);

    const QString deviceName = mainWindow->property(kSessionDeviceNameProperty).toString();
    const QString userName = mainWindow->property(kSessionUserNameProperty).toString();
    if (!deviceName.isEmpty() || !userName.isEmpty()) {
        mainWindow->setSessionInfo(deviceName, userName);
    }

    showWindowFromWidget(source, mainWindow);

    if (source) {
        source->close();
    }
}
