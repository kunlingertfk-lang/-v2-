#include "PlanDialogUtils.h"

#include <QWidget>

namespace PlanDialogUtils {

void applyLargeWindow(QWidget *window)
{
    if (window)
        window->resize(1280, 720);
}

} // namespace PlanDialogUtils
