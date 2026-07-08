#include "PlanDialogUtils.h"

#include <QWidget>

namespace PlanDialogUtils {

void applyLargeWindow(QWidget *window)
{
    if (window)
        window->resize(1280, 720);
}

void fitDialogToScreen(QWidget *dialog, QWidget *, int)
{
    applyLargeWindow(dialog);
}

void centerWindowOnScreen(QWidget *, QWidget *, int)
{
}

} // namespace PlanDialogUtils
