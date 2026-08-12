#include "PlanDialogUtils.h"

#include <QApplication>
#include <QByteArray>
#include <QDebug>
#include <QDialog>
#include <QGraphicsView>
#include <QLayout>
#include <QList>
#include <QMainWindow>
#include <QMetaObject>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QWidget>
#include <QtGlobal>

#include "MainWindow.h"

namespace {

constexpr auto kSessionDeviceNameProperty = "sessionDeviceName";
constexpr auto kSessionUserNameProperty = "sessionUserName";

QScreen *screenFor(QWidget *window, QWidget *parent = nullptr)
{
    if (window && window->screen())
        return window->screen();
    if (parent && parent->screen())
        return parent->screen();
    return QApplication::primaryScreen();
}

QSize currentWindowSize(QWidget *window)
{
    QSize size = window ? window->size() : QSize();
    if (!size.isValid() || size.isEmpty())
        size = window ? window->sizeHint() : QSize();
    if (!size.isValid() || size.isEmpty())
        size = window ? window->minimumSizeHint() : QSize();
    return size;
}

QRect safeAvailableGeometry(QWidget *window, QWidget *parent, int margin)
{
    QScreen *screen = screenFor(window, parent);
    if (!screen) {
        QSize fallback = currentWindowSize(window);
        if (!fallback.isValid() || fallback.isEmpty())
            fallback = QSize(1, 1);
        return QRect(QPoint(0, 0), fallback);
    }

    const QRect available = screen->availableGeometry();
    const int safeMargin = qMax(0, margin);
    QRect safe = available.adjusted(safeMargin, safeMargin, -safeMargin, -safeMargin);
    if (safe.width() <= 0 || safe.height() <= 0)
        safe = available;
    return safe;
}

bool isSetupWindow(QWidget *window)
{
    if (!window)
        return false;

    if (window->property("schemeSetupHost").toBool())
        return true;

    const QString objectName = window->objectName();
    return objectName == QLatin1String("SchemeSetupWindow")
            || objectName == QLatin1String("CameraParamsDialog")
            || objectName == QLatin1String("ReferenceImageDialog")
            || objectName == QLatin1String("ToolsDialog")
            || objectName == QLatin1String("OutputDialog");
}

QWidget *findSchemeSetupHost(QWidget *source)
{
    QWidget *widget = source;
    while (widget) {
        if (widget->property("schemeSetupHost").toBool())
            return widget;
        widget = widget->parentWidget();
    }
    return nullptr;
}

bool isEmbeddedSetupPage(QWidget *window)
{
    if (!window || !isSetupWindow(window)
            || window->property("schemeSetupHost").toBool()) {
        return false;
    }
    return findSchemeSetupHost(window->parentWidget());
}

bool isLargeSetupWindow(QWidget *window)
{
    return qobject_cast<QMainWindow *>(window) != nullptr
            || (window && (window->objectName() == QLatin1String("LoginWindow")
                           || isSetupWindow(window)));
}

void setWidgetWidth(QWidget *root, const char *name, int minimum, int maximum)
{
    QWidget *widget = root ? root->findChild<QWidget *>(QLatin1String(name)) : nullptr;
    if (!widget)
        return;

    widget->setMinimumWidth(minimum);
    widget->setMaximumWidth(maximum);
    widget->setSizePolicy(QSizePolicy::Preferred, widget->sizePolicy().verticalPolicy());
}

void setToolButtonMinimum(QWidget *root, const char *name, const QSize &minimum, const QSize &iconSize)
{
    QToolButton *button = root ? root->findChild<QToolButton *>(QLatin1String(name)) : nullptr;
    if (!button)
        return;

    button->setMinimumSize(minimum);
    button->setIconSize(iconSize);
}

void applyStandardSetupPageLayout(QWidget *window)
{
    if (!isSetupWindow(window))
        return;

    setWidgetWidth(window, "setupStepRail", 106, 106);
    setWidgetWidth(window, "setupEditorPanel", 610, 610);
    setWidgetWidth(window, "setupViewerFrame", 0, QWIDGETSIZE_MAX);

    if (QWidget *viewer = window->findChild<QWidget *>(QLatin1String("setupViewerFrame")))
        viewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (QWidget *editor = window->findChild<QWidget *>(QLatin1String("setupEditorPanel")))
        editor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    const QSize stepMinimum(108, 106);
    const QSize stepIcon(34, 34);
    setToolButtonMinimum(window, "cameraStepButton", stepMinimum, stepIcon);
    setToolButtonMinimum(window, "referenceStepButton", stepMinimum, stepIcon);
    setToolButtonMinimum(window, "toolsStepButton", stepMinimum, stepIcon);
    setToolButtonMinimum(window, "outputStepButton", stepMinimum, stepIcon);
}

void applyStandardToolPageLayout(QWidget *window)
{
    if (!window || !window->property("toolLevelStyle").toBool())
        return;

    const QRect available = safeAvailableGeometry(window, window->parentWidget(), 0);
    const int parameterPanelWidth = qBound(
                520,
                qRound(static_cast<qreal>(available.width()) * 0.32),
                610);

    static const char *const parameterPanels[] = {
        "setupEditorPanel",
        "leftPanel",
        "parameterPanel",
        "colorComparisonLeftPanel",
        "calibrationTransformConfigScroll",
        "configScrollArea",
        "colorTemplateLeftScrollArea"
    };
    for (const char *name : parameterPanels)
        setWidgetWidth(window, name, parameterPanelWidth, parameterPanelWidth);

    static const char *const viewerPanels[] = {
        "setupViewerFrame",
        "previewPanel",
        "colorComparisonRightPanel",
        "resultFrame"
    };
    for (const char *name : viewerPanels) {
        QWidget *viewer = window->findChild<QWidget *>(QLatin1String(name));
        if (!viewer)
            continue;
        viewer->setMinimumWidth(0);
        viewer->setMaximumWidth(QWIDGETSIZE_MAX);
        viewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    const QList<QGraphicsView *> graphicsViews = window->findChildren<QGraphicsView *>();
    for (QGraphicsView *view : graphicsViews) {
        view->setMinimumSize(0, 0);
        view->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    static const char *const titleBars[] = {
        "setupTopBar",
        "titleBar",
        "headerFrame",
        "colorComparisonHeader",
        "calibrationTransformHeader"
    };
    for (const char *name : titleBars) {
        QWidget *titleBar = window->findChild<QWidget *>(QLatin1String(name));
        if (!titleBar)
            continue;
        titleBar->setMinimumHeight(54);
        titleBar->setMaximumHeight(54);
    }
}

void refreshWidgetStyle(QWidget *widget)
{
    if (!widget)
        return;
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

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

MainWindow *findParentMainWindow(QWidget *source)
{
    QWidget *widget = source;
    while (widget) {
        if (MainWindow *mainWindow = qobject_cast<MainWindow *>(widget))
            return mainWindow;
        widget = widget->parentWidget();
    }

    return nullptr;
}

} // namespace

void PlanDialogUtils::applyLargeWindow(QWidget *window)
{
    if (!window)
        return;

    applyStandardSetupPageLayout(window);

    const QRect safe = safeAvailableGeometry(window, window->parentWidget(), 0);
    const QSize oldMinimum = window->minimumSize();
    window->setMinimumSize(qMin(oldMinimum.width(), safe.width()),
                           qMin(oldMinimum.height(), safe.height()));
    window->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    window->setGeometry(safe);
    window->setWindowState((window->windowState() & ~Qt::WindowMinimized) | Qt::WindowMaximized);
}

void PlanDialogUtils::applyToolLevelStyle(QWidget *window)
{
    if (!window)
        return;

    window->setProperty("toolLevelStyle", true);
    applyStandardToolPageLayout(window);
    refreshWidgetStyle(window);
    const QList<QWidget *> children = window->findChildren<QWidget *>();
    for (QWidget *child : children)
        refreshWidgetStyle(child);
}

bool PlanDialogUtils::isLargeWindow(QWidget *window)
{
    return isLargeSetupWindow(window);
}

void PlanDialogUtils::fitDialogToScreen(QWidget *dialog, QWidget *parent, int margin)
{
    if (!dialog)
        return;

    if (isLargeSetupWindow(dialog)) {
        applyLargeWindow(dialog);
        return;
    }

    const QRect safe = safeAvailableGeometry(dialog, parent, margin);
    const QSize maxSize(safe.width(), safe.height());

    const QSize oldMinimum = dialog->minimumSize();
    dialog->setMinimumSize(qMin(oldMinimum.width(), maxSize.width()),
                           qMin(oldMinimum.height(), maxSize.height()));
    dialog->setMaximumSize(maxSize);

    QSize target = currentWindowSize(dialog);
    if (!target.isValid() || target.isEmpty())
        target = maxSize;

    target.setWidth(qMin(target.width(), maxSize.width()));
    target.setHeight(qMin(target.height(), maxSize.height()));
    target.setWidth(qMax(target.width(), dialog->minimumWidth()));
    target.setHeight(qMax(target.height(), dialog->minimumHeight()));

    dialog->resize(target);
    clampWindowToAvailableGeometry(dialog, margin);
}

void PlanDialogUtils::fitWindowToScreen(QWidget *window, int margin)
{
    if (!window)
        return;

    if (isLargeSetupWindow(window)) {
        applyLargeWindow(window);
        return;
    }

    const QRect safe = safeAvailableGeometry(window, window->parentWidget(), margin);
    const QSize maxSize(safe.width(), safe.height());
    const QSize oldMinimum = window->minimumSize();
    window->setMinimumSize(qMin(oldMinimum.width(), maxSize.width()),
                           qMin(oldMinimum.height(), maxSize.height()));
    window->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    QSize target = currentWindowSize(window);
    if (!target.isValid() || target.isEmpty())
        target = maxSize;

    target.setWidth(qMin(target.width(), maxSize.width()));
    target.setHeight(qMin(target.height(), maxSize.height()));
    target.setWidth(qMax(target.width(), window->minimumWidth()));
    target.setHeight(qMax(target.height(), window->minimumHeight()));

    window->resize(target);
    clampWindowToAvailableGeometry(window, margin);
}

void PlanDialogUtils::centerWindowOnScreen(QWidget *window, QWidget *parent, int margin)
{
    if (!window)
        return;

    if (isLargeSetupWindow(window)) {
        applyLargeWindow(window);
        return;
    }

    fitDialogToScreen(window, parent, margin);

    const QRect safe = safeAvailableGeometry(window, parent, margin);
    QRect base = safe;
    if (parent) {
        QWidget *parentWindow = parent->window();
        const QRect parentGeometry = parentWindow ? parentWindow->frameGeometry() : parent->frameGeometry();
        if (parentGeometry.isValid() && safe.intersects(parentGeometry))
            base = parentGeometry;
    }

    const QSize size = window->size();
    const QPoint topLeft(base.center().x() - size.width() / 2,
                         base.center().y() - size.height() / 2);
    window->move(topLeft);
    clampWindowToAvailableGeometry(window, margin);
}

void PlanDialogUtils::clampWindowToAvailableGeometry(QWidget *window, int margin)
{
    if (!window)
        return;

    if (window->windowState().testFlag(Qt::WindowMaximized))
        return;

    const QRect safe = safeAvailableGeometry(window, window->parentWidget(), margin);
    QSize size = window->size();
    bool resized = false;

    if (size.width() > safe.width()) {
        size.setWidth(safe.width());
        resized = true;
    }
    if (size.height() > safe.height()) {
        size.setHeight(safe.height());
        resized = true;
    }
    if (resized)
        window->resize(size);

    QRect geometry(window->pos(), window->size());
    int x = geometry.x();
    int y = geometry.y();

    if (geometry.right() > safe.right())
        x = safe.right() - geometry.width() + 1;
    if (geometry.bottom() > safe.bottom())
        y = safe.bottom() - geometry.height() + 1;
    if (x < safe.left())
        x = safe.left();
    if (y < safe.top())
        y = safe.top();

    window->move(x, y);
}

void PlanDialogUtils::applyConfiguredWindowState(QWidget *window)
{
    if (!window) {
        return;
    }

    PlanDialogUtils::fitWindowToScreen(window, 0);
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
    // Ownership is decided by the caller. Modal configuration dialogs are
    // commonly stack allocated and opened with exec(); enabling
    // WA_DeleteOnClose here would delete a stack object when accept()/reject()
    // closes it. Heap-allocated setup pages opt in from showDialogFromWidget().
    dialog->setAttribute(Qt::WA_DeleteOnClose, false);

    if (isEmbeddedSetupPage(dialog)) {
        dialog->setWindowFlags(Qt::Widget);
        dialog->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        applyStandardSetupPageLayout(dialog);
        return;
    }

    dialog->setWindowFlag(Qt::Window, true);
    PlanDialogUtils::applyLargeWindow(dialog);
    scheduleSetupDialogSizeLog(dialog);

}

bool PlanDialogUtils::switchEmbeddedSetupPage(QWidget *current, const QString &pageId)
{
    QWidget *host = findSchemeSetupHost(current);
    if (!host)
        return false;

    bool switched = false;
    const bool invoked = QMetaObject::invokeMethod(
                host,
                "showSetupPage",
                Qt::DirectConnection,
                Q_RETURN_ARG(bool, switched),
                Q_ARG(QString, pageId));
    return invoked && switched;
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
    PlanDialogUtils::fitWindowToScreen(target, 0);

    if (isSetupWindow(target)) {
        if (MainWindow *mainWindow = findParentMainWindow(target))
            mainWindow->registerActiveSetupWindow(target);
    }

    if (PlanDialogUtils::isLargeWindow(target)
            || target->windowState().testFlag(Qt::WindowMaximized)) {
        target->showMaximized();
    } else {
        PlanDialogUtils::centerWindowOnScreen(target, source, 0);
        target->show();
    }

    PlanDialogUtils::clampWindowToAvailableGeometry(target, 0);
    target->raise();
    target->activateWindow();
}

void PlanDialogUtils::showDialogFromWidget(QWidget *source, QDialog *dialog)
{
    if (dialog) {
        const bool persistentSetupHost = dialog->property("schemeSetupHost").toBool();
        dialog->setAttribute(Qt::WA_DeleteOnClose, !persistentSetupHost);
    }
    showWindowFromWidget(source, dialog);
}

void PlanDialogUtils::replaceDialog(QWidget *current, QDialog *next)
{
    if (current && next && !next->parentWidget()) {
        if (MainWindow *mainWindow = findParentMainWindow(current))
            next->setParent(mainWindow, next->windowFlags());
    }

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
    MainWindow *mainWindow = findParentMainWindow(source);
    if (!mainWindow) {
        qWarning() << "[SCHEME-RETURN] returnToMainWindow failed: original MainWindow not found"
                   << "source=" << (source ? source->objectName() : QStringLiteral("<null>"));
        return;
    }

    copySessionInfo(source, mainWindow);

    const QString deviceName = mainWindow->property(kSessionDeviceNameProperty).toString();
    const QString userName = mainWindow->property(kSessionUserNameProperty).toString();
    if (!deviceName.isEmpty() || !userName.isEmpty()) {
        mainWindow->setSessionInfo(deviceName, userName);
    }

    qDebug() << "[SCHEME-RETURN] returnToMainWindow"
             << "source=" << (source ? source->objectName() : QStringLiteral("<null>"))
             << "mainWindow=" << mainWindow
             << "reuseExisting=" << true;

    QWidget *windowToClose = findSchemeSetupHost(source);
    if (!windowToClose)
        windowToClose = source;

    showWindowFromWidget(source, mainWindow);

    if (windowToClose) {
        windowToClose->close();
    }

    qDebug() << "[PlanDialogUtils] returnToMainWindow reused existing MainWindow";
}
