#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QImage>
#include <QMap>
#include <QMetaObject>
#include <QVector>

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolPreviewSnapshot.h"
#include "toolcore/ToolResult.h"
#include "tooladapters/OcrAdapter.h"
#include "tooladapters/PatternPresenceAdapter.h"
#include "tooladapters/BlobPresenceAdapter.h"
#include "tooladapters/CirclePresenceAdapter.h"
#include "tooladapters/ColorComparisonAdapter.h"
#include "tooladapters/ColorRecognitionAdapter.h"
#include "tooladapters/ContourPresenceAdapter.h"
#include "tooladapters/EdgePresenceAdapter.h"
#include "tooladapters/LinePresenceAdapter.h"
#include "tooladapters/AiDetectionAdapter.h"
#include "tooladapters/RegisteredClassificationAdapter.h"
#include "tooladapters/TemplateLocationAdapter.h"
#include "tooladapters/PositionCorrectionAdapter.h"

class FrameViewHelper;
class QEvent;
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
    void setSchemeTools(const QVector<ToolConfig> &configs,
                        const QMap<QString, ToolPreviewSnapshot> &referenceSnapshots);
    void applySavedSchemeTools(const QVector<ToolConfig> &configs,
                               const QMap<QString, ToolPreviewSnapshot> &referenceSnapshots);
    void updateSchemeToolsFromToolsDialog(const QVector<ToolConfig> &configs,
                                          const QMap<QString, ToolPreviewSnapshot> &referenceSnapshots);

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void clearSummary();
    void openCameraParamsDialog();
    void openToolsDialog();
    void runSingleToolFlow();
    void toggleWindowState();
    void on_stopRunButton_clicked();
    void handleToolChainRunFinished();
    void selectSchemeTool(int row);
    void editSchemeTool(int row);
    void switchSchemeFromCombo(int index);

private:
    struct ToolChainRunOutput;

    void setupUiState();            
    void setupSchemeSelector();
    void refreshSchemeSelector();
    void showSchemeSelectorPopup();
    void createAndSwitchToNewScheme();
    bool switchSchemeById(const QString &schemeId);
    void applyCurrentSchemeState();
    bool persistCurrentSchemeState(const QString &context);
    void ensureCameraRunning();
    void refreshLivePreview();
    void refreshToolConfigTable();
    void updateToolResultRow(int row);
    void refreshToolRunStateRows();
    bool runCurrentToolChainOnce();
    bool tryRunToolChainOnLatestFrame(qint64 frameIndex);
    bool submitToolChainRun(bool continuousRun, qint64 triggerFrameIndex);
    void onCameraFrameUpdated(qint64 frameIndex);
    void applyToolChainRunResult(ToolChainRunOutput output);
    void startContinuousRun();
    void stopContinuousRun();
    void resetContinuousRunStats();
    void updateContinuousRunUi();
    void showStatusText(const QString &text);
    void showRunResultStatus(const ToolChainRunOutput &output, bool overallOk);
    void logToolChainRunPerformance(const ToolChainRunOutput &output, bool forceLog);
    void showToolSnapshot(int row,
                          const ToolPreviewSnapshot &snapshot,
                          const QImage &baseImage,
                          const QString &sourceLabel);
    void showAllLastRunOverlays();
    void storeLastRunSnapshots(const QVector<ToolConfig> &enabledConfigs,
                               const QVector<ToolResult> &results);
    void syncSnapshotMapsWithConfigs();
    QString toolDisplayName(const ToolConfig &config) const;
    QString toolRunStateText(const ToolConfig &config) const;
    QString toolSnapshotStatusLine(const ToolPreviewSnapshot &snapshot,
                                   const QString &sourceLabel) const;
    bool openToolConfigDialogForEdit(int row);

    bool m_isContinuousRunning = false;
    bool m_isToolChainRunning = false;
    QVector<ToolConfig> m_schemeToolConfigs;
    QMap<QString, ToolPreviewSnapshot> m_referencePreviewSnapshots;
    QMap<QString, ToolPreviewSnapshot> m_lastRunSnapshots;
    QVector<ToolOverlay> m_lastReferenceCorrectionOverlays;
    QImage m_lastRunImage;
    int m_selectedToolIndex = -1;
    QFutureWatcher<ToolChainRunOutput> *m_toolChainWatcher = nullptr;
    QMetaObject::Connection m_continuousFrameConnection;
    int m_toolChainRunSequence = 0;
    int m_continuousCompletedRuns = 0;
    int m_continuousSubmittedFrames = 0;
    int m_continuousDroppedFrames = 0;
    qint64 m_lastProcessedFrameIndex = -1;
    qint64 m_lastSeenFrameIndex = -1;
    qint64 m_continuousRunStartWallMs = 0;
    qint64 m_lastContinuousRunFinishWallMs = 0;
    qint64 m_lastContinuousPerfLogWallMs = 0;
    qint64 m_continuousPerfWindowStartWallMs = 0;
    int m_continuousPerfWindowCompleted = 0;
    qint64 m_continuousPerfEngineMs = 0;
    qint64 m_continuousPerfTotalMs = 0;
    qint64 m_continuousPerfUiMs = 0;
    qint64 m_continuousPerfFrameCopyMs = 0;
    qint64 m_continuousPerfRefCopyMs = 0;
    int m_totalInspectionCount = 0;
    int m_okInspectionCount = 0;
    int m_ngInspectionCount = 0;
    bool m_runtimeStarted = false;
    QElapsedTimer m_runtimeTimer;
    OcrAdapter m_ocrAdapter;
    PatternPresenceAdapter m_patternPresenceAdapter;
    BlobPresenceAdapter m_blobPresenceAdapter;
    CirclePresenceAdapter m_circlePresenceAdapter;
    ColorComparisonAdapter m_colorComparisonAdapter;
    ColorRecognitionAdapter m_colorRecognitionAdapter;
    ContourPresenceAdapter m_contourPresenceAdapter;
    EdgePresenceAdapter m_edgePresenceAdapter;
    LinePresenceAdapter m_linePresenceAdapter;
    AiDetectionAdapter m_aiDetectionAdapter;
    RegisteredClassificationAdapter m_registeredClassificationAdapter;
    TemplateLocationAdapter m_templateLocationAdapter;
    PositionCorrectionAdapter m_positionCorrectionAdapter;
    ToolEngine m_toolEngine;
    FrameViewHelper *m_previewHelper = nullptr;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
