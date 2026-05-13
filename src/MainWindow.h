#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolResult.h"
#include "tooladapters/OcrAdapter.h"
#include "tooladapters/PatternPresenceAdapter.h"
#include "tooladapters/BlobPresenceAdapter.h"
#include "tooladapters/CirclePresenceAdapter.h"

class FrameViewHelper;
class QShowEvent;

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

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void clearSummary();
    void openCameraParamsDialog();
    void openToolsDialog();
    void runSingleToolFlow();
    void toggleWindowState();
    void on_stopRunButton_clicked();

private:
    void setupUiState();
    void ensureCameraRunning();
    void refreshLivePreview();
    void refreshToolConfigTable();
    void updateToolResultRow(int row, const ToolResult &result);
    bool m_isRunning = false;
    QVector<ToolConfig> m_toolConfigs;
    OcrAdapter m_ocrAdapter;
    PatternPresenceAdapter m_patternPresenceAdapter;
    BlobPresenceAdapter m_blobPresenceAdapter;
    CirclePresenceAdapter m_circlePresenceAdapter;
    ToolEngine m_toolEngine;
    FrameViewHelper *m_previewHelper = nullptr;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
