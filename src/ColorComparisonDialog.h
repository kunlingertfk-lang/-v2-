#ifndef COLORCOMPARISONDIALOG_H
#define COLORCOMPARISONDIALOG_H

#include <QDialog>
#include <QRectF>
#include <QVector>

#include <opencv2/core.hpp>

#include "algorithms/recognition/ColorComparisonModel.h"
#include "frame/FrameInputMetadata.h"
#include "frame/FrameViewHelper.h"
#include "tooladapters/ColorComparisonAdapter.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"
#include "toolcore/ToolRequest.h"

class QButtonGroup;
class QCheckBox;
class QCloseEvent;
class QComboBox;
class QFrame;
class QGraphicsView;
class QLabel;
class QPushButton;
class QResizeEvent;
class QSpinBox;
class QStackedWidget;
class QToolButton;
class QTimer;
class QWidget;
class ColorComparisonFeatureView;
struct ReferenceFrameSetSnapshot;
namespace Ui { class ColorComparisonDialog; }
template <typename T> class QFutureWatcher;

class ColorComparisonDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColorComparisonDialog(QWidget *parent = nullptr);
    ~ColorComparisonDialog() override;

    ToolConfig toToolConfig() const;
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    void loadFromConfig(const ToolConfig &config);
    QString summaryText() const;

public slots:
    void accept() override;
    void reject() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class EditState {
        None,
        TemplateRect,
        TemplateMaskPolygon,
        DetectRect,
        DetectCircle,
        DetectMaskPolygon
    };

    enum class TestUiMode {
        Edit,
        Continuous,
        TestPaused
    };

    enum class LiveTestSource {
        None,
        Reference,
        Camera,
        Imported
    };

    void buildUi();
    void connectControls();
    void connectAsyncWorkers();
    void setAllParamsMode(bool allMode);
    void setEditState(EditState state);
    void toggleEditState(EditState requestedState);
    void showPreviewImage();
    void showFrameImage(const cv::Mat &frame, const QString &title);
    void refreshRoiOverlay();
    QVector<ToolOverlay> configurationGeometryOverlays() const;
    QVector<ToolOverlay> detectionGeometryOverlays() const;
    QVector<ToolOverlay> combinedDisplayOverlays() const;
    void refreshGeometryOverlays();
    void updateStatus(const QString &text);
    void updateTemplatePreview();
    void runTest();
    void runReferenceTest();
    void startContinuousRun();
    void stopContinuousRun();
    void runContinuousTick();
    void runSingleShotTest();
    void importTestImageFromPc();
    void exitTestMode();
    void rerunLiveComparison();
    void updateBottomButtons();
    void runComparisonOnFrame(const cv::Mat &frame,
                              const FrameInputMetadata &metadata,
                              const QString &imageTitle,
                              bool referenceSource,
                              const ReferenceFrameSetSnapshot *referenceSet = nullptr);
    ToolRequest makeTestRequest(const cv::Mat &frame,
                                const FrameInputMetadata &metadata,
                                const ReferenceFrameSetSnapshot &referenceSet) const;
    void queueTestRequest(const ToolRequest &request,
                          const QString &imageTitle,
                          bool referenceSource);
    void launchTestRequest(const ToolRequest &request,
                           const QString &imageTitle,
                           bool referenceSource,
                           quint64 generation);
    void handleTestFinished();
    void finishConfiguration();
    void handleRoiChanged(const QRectF &roi);
    void handleCircleChanged(const CircleRoi &circle);
    void handlePolygonChanged(const QVector<QPointF> &points);
    void displayResult(const ToolResult &result, bool referenceSource);
    void displayError(const QString &status, const QString &message);
    void displayStoredModelInstruction();
    bool blockInvalidConfigAction();
    void enterInvalidConfigReadOnly(const ToolConfig &config,
                                    const QString &status,
                                    const QString &message);
    void leaveInvalidConfigReadOnly();
    void updateInvalidConfigReadOnlyUi();
    QRectF normalizedRoiOrDefault(const QRectF &roi) const;
    QImage templateRawRoiImage() const;
    void refreshEditControls();
    void refreshTemplateRegionControls();
    void refreshDetectRegionButtons();
    void refreshPositionCorrectionControls();
    void beginMaskRedraw(EditState state);
    void clearTemplateMask();
    void clearDetectionMask();
    void handleDetectionGeometryChanged(const QString &reason);
    void handleDetectionMaskChanged(const QString &reason);
    void invalidateAsyncWork();
    bool invalidateModelBuild();
    void markModelStale(const QString &reason);
    bool rebuildTemplateModelFromFrame(const cv::Mat &frame,
                                       const FrameInputMetadata &metadata,
                                       QString *status,
                                       QString *message);
    void startExplicitModelRebuild();
    void handleModelBuildFinished();
    void updateModelStateUi();
    void updateFeaturePreview();
    bool updateDetectionFeaturePreview(const ToolResult &result);
    QJsonObject colorComparisonParams() const;

    QString m_toolId;
    bool m_enabled = true;
    QString m_templateRegionMode = QStringLiteral("custom");
    QRectF m_templateRoi = QRectF(0.05, 0.05, 0.25, 0.25);
    QVector<QPointF> m_templateMask;
    ColorComparisonModelV2 m_model;
    int m_modelOriginVersion = 2;
    QString m_modelStatus = QStringLiteral("model_empty");
    QString m_modelReason = QStringLiteral("尚未取样，请点击重新取样");
    QRectF m_detectRoi = QRectF(0.35, 0.05, 0.3, 0.3);
    QString m_detectRegionType = QStringLiteral("rectangle");
    bool m_globalDetection = false;
    CircleRoi m_detectCircle;
    QVector<QPointF> m_detectMask;
    bool m_positionCorrectionEnabled = false;
    bool m_showPositionCorrectionMatchContour = true;
    QString m_positionCorrectionSource = QStringLiteral("reference.positionCorrection");
    EditState m_editState = EditState::None;
    bool m_previewUsesReferenceImage = true;
    QImage m_previewImage;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    TestUiMode m_testUiMode = TestUiMode::Edit;
    LiveTestSource m_liveTestSource = LiveTestSource::None;
    cv::Mat m_liveTestFrameSnapshot;
    FrameInputMetadata m_liveTestFrameMetadata;
    QString m_liveTestImageTitle;
    QTimer *m_continuousTimer = nullptr;
    bool m_loadingConfig = false;
    bool m_invalidConfigReadOnly = false;
    ToolConfig m_originalInvalidConfig;
    QString m_invalidConfigStatus;
    QString m_invalidConfigMessage;

    QFutureWatcher<ToolResult> *m_testWatcher = nullptr;
    QFutureWatcher<ColorComparisonTemplateBuildResult> *m_modelBuildWatcher = nullptr;
    quint64 m_testGeneration = 0;
    quint64 m_activeTestGeneration = 0;
    quint64 m_modelBuildGeneration = 0;
    quint64 m_activeModelBuildGeneration = 0;
    bool m_modelBuildUiActive = false;
    bool m_pendingContinuousRun = false;
    ToolRequest m_pendingTestRequest;
    QString m_pendingImageTitle;
    bool m_pendingReferenceSource = false;
    quint64 m_pendingTestGeneration = 0;
    QString m_activeImageTitle;
    bool m_activeReferenceSource = false;
    QVector<ToolOverlay> m_runtimeResultOverlays;
    QVector<QPointF> m_maskBeforeRedraw;
    bool m_maskRedrawInProgress = false;

    QButtonGroup *m_segmentGroup = nullptr;
    QButtonGroup *m_detectRegionGroup = nullptr;
    FrameViewHelper *m_previewHelper = nullptr;
    QStackedWidget *m_paramsStack = nullptr;
    QFrame *m_featureCard = nullptr;
    QWidget *m_detectMaskRow = nullptr;
    QWidget *m_positionCorrectionPanel = nullptr;
    QLabel *m_templatePreviewLabel = nullptr;
    QLabel *m_modelStateLabel = nullptr;
    ColorComparisonFeatureView *m_featureView = nullptr;
    QLabel *m_viewerTitleLabel = nullptr;
    QLabel *m_viewerStatusLabel = nullptr;
    QGraphicsView *m_previewGraphicsView = nullptr;
    QPushButton *m_pcImportButton = nullptr;
    QPushButton *m_basicButton = nullptr;
    QPushButton *m_allButton = nullptr;
    QComboBox *m_templateRegionModeComboBox = nullptr;
    QLabel *m_templateSyncHintLabel = nullptr;
    QPushButton *m_templateEditButton = nullptr;
    QToolButton *m_templateRectButton = nullptr;
    QPushButton *m_templateFinishButton = nullptr;
    QPushButton *m_templateMaskEditButton = nullptr;
    QToolButton *m_templateMaskPolygonButton = nullptr;
    QPushButton *m_templateMaskRedrawButton = nullptr;
    QPushButton *m_templateMaskClearButton = nullptr;
    QPushButton *m_templateMaskFinishButton = nullptr;
    QPushButton *m_rebuildModelButton = nullptr;
    QToolButton *m_detectGlobalButton = nullptr;
    QToolButton *m_detectRectButton = nullptr;
    QToolButton *m_detectCircleButton = nullptr;
    QCheckBox *m_positionCorrectionCheckBox = nullptr;
    QWidget *m_positionCorrectionSourceRow = nullptr;
    QComboBox *m_positionCorrectionComboBox = nullptr;
    QWidget *m_positionCorrectionContourRow = nullptr;
    QCheckBox *m_positionCorrectionContourCheckBox = nullptr;
    QPushButton *m_detectMaskEditButton = nullptr;
    QToolButton *m_detectMaskPolygonButton = nullptr;
    QPushButton *m_detectMaskRedrawButton = nullptr;
    QPushButton *m_detectMaskClearButton = nullptr;
    QPushButton *m_detectMaskFinishButton = nullptr;
    QComboBox *m_sensitivityComboBox = nullptr;
    QComboBox *m_featureTypeComboBox = nullptr;
    QCheckBox *m_brightnessCheckBox = nullptr;
    QSpinBox *m_minScoreSpinBox = nullptr;
    QPushButton *m_referenceTestButton = nullptr;
    QPushButton *m_testRunButton = nullptr;
    QPushButton *m_finishButton = nullptr;
    QPushButton *m_exitTestButton = nullptr;
    Ui::ColorComparisonDialog *ui = nullptr;
};

#endif // COLORCOMPARISONDIALOG_H
