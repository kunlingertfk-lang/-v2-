#ifndef WINDOWUTILS_H
#define WINDOWUTILS_H

class QWidget;

namespace WindowUtils {

void applyLargeWindow(QWidget *window);
bool isLargeWindow(QWidget *window);
void fitDialogToScreen(QWidget *dialog, QWidget *parent = nullptr, int margin = 40);
void fitWindowToScreen(QWidget *window, int margin = 40);
void centerWindowOnScreen(QWidget *window, QWidget *parent = nullptr, int margin = 40);
void clampWindowToAvailableGeometry(QWidget *window, int margin = 40);

} // namespace WindowUtils

#endif // WINDOWUTILS_H
