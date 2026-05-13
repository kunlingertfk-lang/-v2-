#include "WindowUtils.h"

#include <QApplication>
#include <QAbstractScrollArea>
#include <QComboBox>
#include <QList>
#include <QMainWindow>
#include <QLayout>
#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QToolButton>
#include <QWidget>
#include <QtGlobal>

namespace {

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
    QSize size = window->size();
    if (!size.isValid() || size.isEmpty())
        size = window->sizeHint();
    if (!size.isValid() || size.isEmpty())
        size = window->minimumSizeHint();
    return size;
}

QRect safeAvailableGeometry(QWidget *window, QWidget *parent, int margin)
{
    QScreen *screen = screenFor(window, parent);
    if (!screen) {
        QSize fallback = window ? currentWindowSize(window) : QSize();
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

bool isLargeSetupWindow(QWidget *window)
{
    return qobject_cast<QMainWindow *>(window) != nullptr
           || (window && (window->objectName() == QLatin1String("LoginWindow")
                          || window->objectName() == QLatin1String("CameraParamsDialog")
                          || window->objectName() == QLatin1String("ToolsDialog")
                          || window->objectName() == QLatin1String("ReferenceImageDialog")
                          || window->objectName() == QLatin1String("OutputDialog")));
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

void setWidgetMinimum(QWidget *root, const char *name, const QSize &minimum)
{
    QWidget *widget = root ? root->findChild<QWidget *>(QLatin1String(name)) : nullptr;
    if (!widget)
        return;

    widget->setMinimumSize(minimum);
}

void setLayoutMetrics(QWidget *root, const char *name, int left, int top, int right, int bottom, int spacing)
{
    QLayout *layout = root ? root->findChild<QLayout *>(QLatin1String(name)) : nullptr;
    if (!layout)
        return;

    layout->setContentsMargins(left, top, right, bottom);
    layout->setSpacing(spacing);
}

void setToolButtonMinimum(QWidget *root, const char *name, const QSize &minimum, const QSize &iconSize)
{
    QToolButton *button = root ? root->findChild<QToolButton *>(QLatin1String(name)) : nullptr;
    if (!button)
        return;

    button->setMinimumSize(minimum);
    button->setIconSize(iconSize);
}

void relaxMainWindowLayout(QWidget *window)
{
    if (!window || window->objectName() != QLatin1String("MainWindow"))
        return;

    setWidgetMinimum(window, "headerLogoLabel", QSize(48, 48));
    if (QWidget *logo = window->findChild<QWidget *>(QLatin1String("headerLogoLabel")))
        logo->setMaximumSize(48, 48);

    setLayoutMetrics(window, "horizontalLayout_header", 12, 10, 12, 10, 8);
    setLayoutMetrics(window, "verticalLayout_sidebar", 16, 12, 12, 12, 14);
    setLayoutMetrics(window, "horizontalLayout_actionBar", 14, 10, 14, 10, 10);
    setLayoutMetrics(window, "horizontalLayout_statusStrip", 14, 6, 14, 6, 12);

    setWidgetWidth(window, "sidebarFrame", 320, 380);
    if (QWidget *summary = window->findChild<QWidget *>(QLatin1String("summaryCard"))) {
        summary->setMinimumHeight(150);
        summary->setMaximumHeight(170);
    }
    if (QComboBox *combo = window->findChild<QComboBox *>(QLatin1String("headerDeviceComboBox"))) {
        combo->setMinimumWidth(200);
        combo->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    const QSize navMinimum(68, 54);
    const QSize navIcon(24, 24);
    setToolButtonMinimum(window, "mainNavCameraButton", navMinimum, navIcon);
    setToolButtonMinimum(window, "mainNavPlanButton", navMinimum, navIcon);
    setToolButtonMinimum(window, "mainNavIoButton", navMinimum, navIcon);
    setToolButtonMinimum(window, "mainNavCommButton", navMinimum, navIcon);
    setToolButtonMinimum(window, "mainNavAssistButton", navMinimum, navIcon);
    setToolButtonMinimum(window, "mainNavMonitorButton", navMinimum, navIcon);
}

void relaxSetupPageLayout(QWidget *window)
{
    if (!window || !(window->objectName() == QLatin1String("CameraParamsDialog")
                     || window->objectName() == QLatin1String("ToolsDialog")
                     || window->objectName() == QLatin1String("ReferenceImageDialog")
                     || window->objectName() == QLatin1String("OutputDialog")))
        return;

    setWidgetWidth(window, "setupStepRail", 92, 96);
    setWidgetWidth(window, "setupEditorPanel", 360, QWIDGETSIZE_MAX);
    setWidgetWidth(window, "setupViewerFrame", 0, QWIDGETSIZE_MAX);

    if (QWidget *viewer = window->findChild<QWidget *>(QLatin1String("setupViewerFrame")))
        viewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (QWidget *editor = window->findChild<QWidget *>(QLatin1String("setupEditorPanel")))
        editor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    setLayoutMetrics(window, "horizontalLayout_toolbar", 18, 8, 18, 8, 12);
    setLayoutMetrics(window, "verticalLayout_steps", 4, 12, 4, 12, 8);
    setLayoutMetrics(window, "verticalLayout_editor", 18, 16, 18, 14, 12);
    setLayoutMetrics(window, "horizontalLayout_viewerHeader", 14, 0, 12, 0, 6);
    setLayoutMetrics(window, "horizontalLayout_viewerStatus", 14, 0, 12, 0, 8);

    const QSize stepMinimum(84, 74);
    const QSize stepIcon(28, 28);
    setToolButtonMinimum(window, "cameraStepButton", stepMinimum, stepIcon);
    setToolButtonMinimum(window, "referenceStepButton", stepMinimum, stepIcon);
    setToolButtonMinimum(window, "toolsStepButton", stepMinimum, stepIcon);
    setToolButtonMinimum(window, "outputStepButton", stepMinimum, stepIcon);

    const QList<QAbstractScrollArea *> scrollAreas = window->findChildren<QAbstractScrollArea *>();
    for (QAbstractScrollArea *area : scrollAreas) {
        area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        area->setMinimumWidth(0);
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
}

} // namespace

void WindowUtils::applyLargeWindow(QWidget *window)
{
    if (!window)
        return;

    const QRect safe = safeAvailableGeometry(window, window->parentWidget(), 0);
    const QSize oldMinimum = window->minimumSize();
    window->setMinimumSize(qMin(oldMinimum.width(), safe.width()),
                           qMin(oldMinimum.height(), safe.height()));
    window->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    relaxMainWindowLayout(window);
    relaxSetupPageLayout(window);
    window->setGeometry(safe);
    window->setWindowState((window->windowState() & ~Qt::WindowMinimized) | Qt::WindowMaximized);
}

bool WindowUtils::isLargeWindow(QWidget *window)
{
    return isLargeSetupWindow(window);
}

void WindowUtils::fitDialogToScreen(QWidget *dialog, QWidget *parent, int margin)
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

void WindowUtils::fitWindowToScreen(QWidget *window, int margin)
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

void WindowUtils::centerWindowOnScreen(QWidget *window, QWidget *parent, int margin)
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

void WindowUtils::clampWindowToAvailableGeometry(QWidget *window, int margin)
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
