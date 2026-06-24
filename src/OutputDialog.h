#ifndef OUTPUTDIALOG_H
#define OUTPUTDIALOG_H

#include <QDialog>
#include <QMap>
#include <QPointer>
#include <QVector>

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"

class FrameViewHelper;
class MainWindow;

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

    void setSchemeTools(const QVector<ToolConfig> &configs,
                        const QMap<QString, ToolPreviewSnapshot> &referenceSnapshots);

private slots:
    void openCameraParamsDialog();
    void openReferenceImageDialog();
    void openToolsDialog();
    void finishSetup();
    void editCurrentSchemeName();
    void saveCurrentScheme();
    void saveCurrentSchemeAs();

private:
    void setupUiState();
    void setupOutputScrollArea();
    void connectNavigation();
    void refreshSchemeHeader();
    void loadCurrentSchemeState();
    bool commitOutputStateToScheme(bool saveToDisk);
    void refreshReferencePreview();
    MainWindow *sourceMainWindow() const;
    void returnToSourceMainWindow();

    Ui::OutputDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    QVector<ToolConfig> m_schemeToolConfigs;
    QMap<QString, ToolPreviewSnapshot> m_referencePreviewSnapshots;
    QPointer<MainWindow> m_sourceMainWindow;
};

#endif // OUTPUTDIALOG_H
