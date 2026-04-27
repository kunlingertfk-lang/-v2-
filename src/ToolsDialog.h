#ifndef TOOLSDIALOG_H
#define TOOLSDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class ToolsDialog;
}
QT_END_NAMESPACE

class ToolsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ToolsDialog(QWidget *parent = nullptr);
    ~ToolsDialog() override;

private slots:
    void openToolLibrary();
    void openCameraParamsDialog();
    void openReferenceImageDialog();
    void openOutputDialog();

private:
    void setupUiState();
    void connectNavigation();
    void addCharacterRecognitionTool(const QString &summaryText);

    Ui::ToolsDialog *ui;
    int m_toolSerial;
};

#endif // TOOLSDIALOG_H
