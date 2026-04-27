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
