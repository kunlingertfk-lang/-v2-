#ifndef OUTPUTDIALOG_H
#define OUTPUTDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class OutputDialog;
}
QT_END_NAMESPACE

class OutputDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OutputDialog(QWidget *parent = nullptr);
    ~OutputDialog() override;

private slots:
    void openCameraParamsDialog();
    void openReferenceImageDialog();
    void openToolsDialog();
    void finishSetup();

private:
    void setupUiState();
    void connectNavigation();

    Ui::OutputDialog *ui;
};

#endif // OUTPUTDIALOG_H
