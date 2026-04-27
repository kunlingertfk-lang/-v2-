#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void setSessionInfo(const QString &deviceName, const QString &userName);

private slots:
    void clearSummary();
    void openCameraParamsDialog();
    void toggleWindowState();
    void on_stopRunButton_clicked();

private:
    void setupUiState();
    bool m_isRunning = false;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
