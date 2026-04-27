#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QDialog>
#include <QVector>

QT_BEGIN_NAMESPACE
namespace Ui {
class LoginWindow;
}
QT_END_NAMESPACE

class LoginWindow : public QDialog
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    ~LoginWindow() override;

private slots:
    void handleLogin();
    void syncSelectionFromList(int row);
    void syncSelectionFromCombo(int index);
    void togglePasswordVisibility();
    void toggleWindowState();
    void closeRequested();

private:
    void setupUiState();
    void populateDevices();
    void populateUsers();

    Ui::LoginWindow *ui;
    bool m_passwordVisible;
};

#endif // LOGINWINDOW_H
