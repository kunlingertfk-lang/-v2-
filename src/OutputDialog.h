#ifndef OUTPUTDIALOG_H
#define OUTPUTDIALOG_H

#include <QDialog>

class FrameViewHelper;

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
    void setupOutputScrollArea();
    void connectNavigation();
    void refreshReferencePreview();

    Ui::OutputDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
};

#endif // OUTPUTDIALOG_H
