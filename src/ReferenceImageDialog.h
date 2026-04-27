#ifndef REFERENCEIMAGEDIALOG_H
#define REFERENCEIMAGEDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class ReferenceImageDialog;
}
QT_END_NAMESPACE

class ReferenceImageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReferenceImageDialog(QWidget *parent = nullptr);
    ~ReferenceImageDialog() override;

private slots:
    void openCameraParamsDialog();
    void openToolsDialog();
    void openOutputDialog();

private:
    void setupUiState();
    void connectNavigation();

    Ui::ReferenceImageDialog *ui;
};

#endif // REFERENCEIMAGEDIALOG_H
