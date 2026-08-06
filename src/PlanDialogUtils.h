#ifndef PLANDIALOGUTILS_H
#define PLANDIALOGUTILS_H

#include <QString>

class QDialog;
class QGraphicsView;
class QToolButton;
class QWidget;

namespace PlanDialogUtils {

enum class Page {
    CameraParams,
    ReferenceImage,
    Tools,
    Output
};

void applyLargeWindow(QWidget *window);
bool isLargeWindow(QWidget *window);
void fitDialogToScreen(QWidget *dialog, QWidget *parent = nullptr, int margin = 40);
void fitWindowToScreen(QWidget *window, int margin = 40);
void centerWindowOnScreen(QWidget *window, QWidget *parent = nullptr, int margin = 40);
void clampWindowToAvailableGeometry(QWidget *window, int margin = 40);
void applyConfiguredWindowState(QWidget *window);
void configureDialogWindow(QDialog *dialog, const QString &title);
void connectWindowButtons(QWidget *window,
                          QToolButton *closeButton,
                          bool returnToMainWindow = false);
void populatePreviewScene(QGraphicsView *view, Page page);
void replaceDialog(QWidget *current, QDialog *next);
void setSessionInfo(QWidget *window, const QString &deviceName, const QString &userName);
void returnToMainWindow(QWidget *source);
void showDialogFromWidget(QWidget *source, QDialog *dialog);
void showWindowFromWidget(QWidget *source, QWidget *target);

} // namespace PlanDialogUtils

#endif // PLANDIALOGUTILS_H
