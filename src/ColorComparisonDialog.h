#ifndef COLORCOMPARISONDIALOG_H
#define COLORCOMPARISONDIALOG_H

#include <QDialog>
#include <QRectF>
#include <QVector>

#include <opencv2/core.hpp>

#include "frame/FrameViewHelper.h"
#include "tooladapters/ColorComparisonAdapter.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"

class QButtonGroup;
class QCheckBox;
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

protected:
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

    // 测试态下固定的检测图像来源：进入测试后检测区域绘制完成会按此来源立即重跑比较。
    enum class LiveTestSource {
        None,
        Reference,  // 基准图测试：始终在基准图上重测
        Camera      // 相机测试：连续态用最新帧，单次态用进入时缓存的快照帧
    };

    void buildUi();
    void connectControls();
    void setAllParamsMode(bool allMode);
    void setEditState(EditState state);
    void showPreviewImage();
    void showFrameImage(const cv::Mat &frame, const QString &title);
    void refreshRoiOverlay();
    void updateStatus(const QString &text);
    void updateTemplatePreview();
    void runTest();
    void runReferenceTest();
    void startContinuousRun();
    void stopContinuousRun();
    void runContinuousTick();
    void runSingleShotTest();
    void exitTestMode();
    // 依据当前 m_liveTestSource 取源帧并立即重跑比较（基准图 / 相机最新帧 / 单次快照）。
    void rerunLiveComparison();
    // 按 m_detectRegionType 进入检测区域编辑态（矩形/圆形），测试态下保持可绘制。
    void applyDetectRoiEditState();
    void updateBottomButtons();
    void runComparisonOnFrame(const cv::Mat &frame,
                              const QString &imageTitle,
                              bool referenceSource);
    void finishConfiguration();
    void handleRoiChanged(const QRectF &roi);
    void handleCircleChanged(const CircleRoi &circle);
    void handlePolygonChanged(const QVector<QPointF> &points);
    void displayResult(const ToolResult &result, bool referenceSource);
    void displayError(const QString &status, const QString &message);
    QRectF normalizedRoiOrDefault(const QRectF &roi) const;
    QImage templateRoiImage() const;
    void refreshEditControls();
    void refreshDetectRegionButtons();
    void refreshPositionCorrectionControls();
    void clearTemplateFeature();
    bool refreshTemplateFeatureFromFrame(const cv::Mat &frame, QString *status, QString *message);
    QJsonObject colorComparisonParams() const;

    QString m_toolId;
    bool m_enabled = true;
    QRectF m_templateRoi = QRectF(0.05, 0.05, 0.25, 0.25);
    QVector<QPointF> m_templateMask;
    QVector<double> m_templateFeature;
    QRectF m_detectRoi = QRectF(0.35, 0.05, 0.3, 0.3);
    QString m_detectRegionType = QStringLiteral("rectangle");
    CircleRoi m_detectCircle;
    QVector<QPointF> m_detectMask;
    bool m_positionCorrectionEnabled = false;
    QString m_positionCorrectionSource = QStringLiteral("1 基准图.位置修正信息");
    EditState m_editState = EditState::None;
    bool m_previewUsesReferenceImage = true;
    QImage m_previewImage;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    ColorComparisonAdapter m_testAdapter;
    TestUiMode m_testUiMode = TestUiMode::Edit;
    LiveTestSource m_liveTestSource = LiveTestSource::None;
    cv::Mat m_liveTestFrameSnapshot;       // 单次态锁定的相机帧快照
    QTimer *m_continuousTimer = nullptr;
    bool m_comparisonRunning = false;
    bool m_loadingConfig = false;

    QButtonGroup *m_segmentGroup = nullptr;
    QButtonGroup *m_detectRegionGroup = nullptr;
    FrameViewHelper *m_previewHelper = nullptr;
    QStackedWidget *m_paramsStack = nullptr;
    QFrame *m_featureCard = nullptr;
    QWidget *m_detectMaskRow = nullptr;
    QLabel *m_templatePreviewLabel = nullptr;
    QLabel *m_viewerTitleLabel = nullptr;
    QLabel *m_viewerStatusLabel = nullptr;
    QGraphicsView *m_previewGraphicsView = nullptr;
    QPushButton *m_basicButton = nullptr;
    QPushButton *m_allButton = nullptr;
    QPushButton *m_templateEditButton = nullptr;
    QToolButton *m_templateRectButton = nullptr;
    QPushButton *m_templateFinishButton = nullptr;
    QPushButton *m_templateMaskEditButton = nullptr;
    QToolButton *m_templateMaskPolygonButton = nullptr;
    QPushButton *m_templateMaskFinishButton = nullptr;
    QToolButton *m_detectGlobalButton = nullptr;
    QToolButton *m_detectRectButton = nullptr;
    QToolButton *m_detectCircleButton = nullptr;
    QCheckBox *m_positionCorrectionCheckBox = nullptr;
    QWidget *m_positionCorrectionSourceRow = nullptr;
    QComboBox *m_positionCorrectionComboBox = nullptr;
    QPushButton *m_detectMaskEditButton = nullptr;
    QToolButton *m_detectMaskPolygonButton = nullptr;
    QPushButton *m_detectMaskFinishButton = nullptr;
    QComboBox *m_sensitivityComboBox = nullptr;
    QComboBox *m_comparisonModeComboBox = nullptr;
    QComboBox *m_featureTypeComboBox = nullptr;
    QCheckBox *m_brightnessCheckBox = nullptr;
    QSpinBox *m_minScoreSpinBox = nullptr;
    QPushButton *m_referenceTestButton = nullptr;
    QPushButton *m_testRunButton = nullptr;
    QPushButton *m_finishButton = nullptr;
    QPushButton *m_exitTestButton = nullptr;
};

#endif // COLORCOMPARISONDIALOG_H
