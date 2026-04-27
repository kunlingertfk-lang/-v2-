#ifndef TOOLLIBRARYDIALOG_H
#define TOOLLIBRARYDIALOG_H

#include <QDialog>

class QButtonGroup;

QT_BEGIN_NAMESPACE
namespace Ui {
class ToolLibraryDialog;
}
QT_END_NAMESPACE

class ToolLibraryDialog : public QDialog
{
    Q_OBJECT

public:
    enum ToolId {
        NoTool = -1,
        Presence = 0,
        Counter,
        Judge,
        Category,
        ColorArea,
        CharacterRecognition,
        Code
    };

    explicit ToolLibraryDialog(QWidget *parent = nullptr);
    ~ToolLibraryDialog() override;

    ToolId selectedTool() const;

private slots:
    void confirmSelection();
    void updatePreview(int id);

private:
    void setupUiState();
    void setupButtonGroup();

    Ui::ToolLibraryDialog *ui;
    QButtonGroup *m_buttonGroup;
};

#endif // TOOLLIBRARYDIALOG_H
