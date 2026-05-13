#ifndef TOOLSDIALOG_H
#define TOOLSDIALOG_H

#include <QDialog>
#include <QVector>

#include "toolcore/ToolConfig.h"

class FrameViewHelper;

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

    const QVector<ToolConfig> &toolConfigs() const;

private slots:
    void openToolLibrary();
    void openCameraParamsDialog();
    void openReferenceImageDialog();
    void openOutputDialog();

private:
    void setupUiState();
    void connectNavigation();
    void addConfiguredTool(const ToolConfig &config);
    void refreshReferencePreview();

    Ui::ToolsDialog *ui;
    int m_toolSerial;
    QVector<ToolConfig> m_toolConfigs;
    FrameViewHelper *m_previewHelper = nullptr;
};

#endif // TOOLSDIALOG_H
