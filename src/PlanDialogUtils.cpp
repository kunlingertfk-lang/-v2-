#include "PlanDialogUtils.h"

#include <QDialog>
#include <QToolButton>
#include <QVariant>
#include <QWidget>

#include "MainWindow.h"

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

} // namespace

void PlanDialogUtils::applyConfiguredWindowState(QWidget *window)
{
    if (!window) {
        return;
    }

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
    dialog->setMinimumSize(1440, 860);
    dialog->resize(1760, 980);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowFlag(Qt::Window, true);

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

    if (target->windowState().testFlag(Qt::WindowMaximized)) {
        target->showMaximized();
    } else {
        target->show();
    }

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
