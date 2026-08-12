#ifndef CONTOURPRESENCEDIALOG_H
#define CONTOURPRESENCEDIALOG_H

#include <QDialog>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "tooladapters/ContourPresenceAdapter.h"
#include "tooladapters/PositionCorrectionAdapter.h"
#include "tooladapters/TemplateLocationAdapter.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolPreviewSnapshot.h"
#include "PositionCorrectionDialogTestHelper.h"
#include "toolcore/ToolResult.h"

#include <opencv2/core.hpp>

class QButtonGroup;
class QResizeEvent;
class FrameViewHelper;

QT_BEGIN_NAMESPACE
namespace Ui {
class ContourPresenceDialog;
}
QT_END_NAMESPACE

struct ContourPresenceConfig
{
    QRectF templateRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> templatePolygonNormalized;
    QString templateSource = QStringLiteral("referenceImage");
    QString detectRegionType = QStringLiteral("rect");
    QVector<QPointF> detectPolygonNormalized;
    QString templateShapeType = QStringLiteral("rect");
    bool enablePositionCorrection = true;
    QString positionCorrectionSource;
    QString positionCorrectionSourceId;
    bool showPositionCorrectionMatchContour = true;
    double minScore = 0.5;
    QString polarity;
    QString thresholdType;
    QString scaleMode;
    int speedScale = 5;
    int featureScale = 1;
    QString thresholdMode;
    int grayThreshold = 15;
    QString chainMode;
    int minChainLength = 4;
    double scaleMin = 100.0;
    double scaleMax = 100.0;
    double angleStart = -45.0;
    double angleExtent = 90.0;
    int timeoutMs = 2000;
    bool showContourPoints = false;
    QString sortMode;
    QString judgeBasis = QStringLiteral("presence");
    bool existOk = true;
    double scoreThreshold = 0.5;
};

class ContourPresenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ContourPresenceDialog(QWidget *parent = nullptr);
    ~ContourPresenceDialog() override;

    ContourPresenceConfig configuration() const;
    ToolConfig toToolConfig() const;
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    void loadFromConfig(const ToolConfig &config);
    void setToolChainTestContext(
            const QVector<ToolConfig> &toolConfigs,
            int currentToolIndex,
            const ReferencePositionCorrectionConfig &referencePositionCorrection);
    QString summaryText() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void finishConfiguration();
    void runReferenceTest();
    void runCameraTest();
    void importTestImageFromPc();
    void exitTestMode();

private:
    enum class RoiEditTarget {
        None,
        TemplateRoi,
        DetectRoi
    };

    void setupUiState();
    void connectControls();
    void applyAdaptiveWindowSize();
    void fitPreview();
    void updateBottomButtons();
    void rerunImportedTest();
    void showReferenceImage();
    void showFrameForRoiEditing();
    void startTemplateRoiEditing();
    void startTemplatePolygonEditing();
    void finishTemplateRoiEditing();
    void startDetectRoiEditing();
    void startDetectPolygonEditing();
    void showTemplateRoiTodo(const QString &message);
    void showDetectRoiTodo(const QString &message);
    void handleRoiChanged(const QRectF &roi);
    void handlePolygonChanged(const QVector<QPointF> &points);
    void handlePolygonSelectionRejected(int pointCount);
    void handleRoiSelectionRejected();
    void refreshDisplayedRoiOverlay();
    void runContourPresenceOnFrame(const cv::Mat &frame,
                                   const cv::Mat &referenceImage,
                                   const QString &imageTitle,
                                   const QString &emptyFrameMessage,
                                   bool referenceTest);
    void displayContourPresenceResult(const ToolResult &result);
    void displayContourPresenceError(const QString &status, const QString &message);
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    QString templateRoiStatusText() const;
    QString detectRoiStatusText() const;
    QRectF effectiveTemplateRoiNormalized() const;
    QRectF effectiveRoiNormalized() const;
    bool isTemplatePolygonMode() const;
    bool isDetectPolygonMode() const;

    Ui::ContourPresenceDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_basicTemplateRegionGroup;
    QButtonGroup *m_templateRegionGroup;
    QButtonGroup *m_basicDetectionRegionGroup;
    QButtonGroup *m_detectionRegionGroup;
    QButtonGroup *m_scaleModeGroup;
    QButtonGroup *m_thresholdModeGroup;
    QButtonGroup *m_chainModeGroup;
    QButtonGroup *m_basicResultPresenceGroup;
    QButtonGroup *m_resultPresenceGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_exitTestButton = nullptr;
    QPushButton *m_pcImportButton = nullptr;
    ContourPresenceAdapter m_testContourPresenceAdapter;
    TemplateLocationAdapter m_testTemplateLocationAdapter;
    PositionCorrectionAdapter m_testPositionCorrectionAdapter;
    ToolEngine m_testToolEngine;
    PositionCorrectionDialogTestContext m_toolChainTestContext;
    QString m_toolId;
    bool m_enabled = true;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QRectF m_templateRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> m_templatePolygonNormalized;
    QVector<QPointF> m_detectPolygonNormalized;
    RoiEditTarget m_roiEditTarget = RoiEditTarget::None;
    cv::Mat m_importedTestFrame;
    QString m_importedTestImageTitle;
    bool m_importedTestActive = false;
    bool m_contourPresenceRunning = false;
    bool m_hasAcceptedToolConfig = false;
    ToolConfig m_acceptedToolConfig;
};

#endif // CONTOURPRESENCEDIALOG_H
