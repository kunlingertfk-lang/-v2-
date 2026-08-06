#ifndef CIRCLEPRESENCEDIALOG_H
#define CIRCLEPRESENCEDIALOG_H

#include <QDialog>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QVector>


#include "frame/FrameViewHelper.h"
#include "tooladapters/CirclePresenceAdapter.h"
#include "tooladapters/PositionCorrectionAdapter.h"
#include "tooladapters/TemplateLocationAdapter.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolPreviewSnapshot.h"
#include "toolcore/ToolResult.h"

#include <opencv2/core.hpp>


class QButtonGroup;
class QPushButton;
class QResizeEvent;
class FrameViewHelper;

QT_BEGIN_NAMESPACE
namespace Ui {
class CirclePresenceDialog;
}
QT_END_NAMESPACE

struct CirclePresenceConfig
{
    QString detectRegionType = QStringLiteral("rect");
    QVector<QPointF> detectPolygonNormalized;
    CircleRoi detectCircleNormalized;
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
    QString positionCorrectionSourceId;
    bool showPositionCorrectionMatchContour = true;
    int sensitivity = 60;
    int roundness = 25;
    QString edgePolarity = QStringLiteral("any");
    QString edgeType = QStringLiteral("strongest");
    QString judgeBasis = QStringLiteral("presence");
    bool existOk = true;
    int timeoutMs = 1000;
};

class CirclePresenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CirclePresenceDialog(QWidget *parent = nullptr);
    ~CirclePresenceDialog() override;

    CirclePresenceConfig configuration() const;
    ToolConfig toToolConfig() const;
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    void loadFromConfig(const ToolConfig &config);
    void setToolChainTestContext(
        const QVector<ToolConfig> &toolConfigs,
        int currentToolIndex,
        ToolEngine *sharedToolEngine,
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
    void setupUiState();
    void connectControls();
    void applyAdaptiveWindowSize();
    void fitPreview();
    void showReferenceImage();
    void showFrameForRoiEditing();
    void startDetectRoiEditing();
    void startDetectPolygonEditing();
    void startDetectCircleEditing();
    void showDetectRoiTodo(const QString &message);
    void resetDetectRoi();
    void handleRoiChanged(const QRectF &roi);
    void handlePolygonChanged(const QVector<QPointF> &points);
    void handleCircleChanged(const CircleRoi &roi);
    void handlePolygonSelectionRejected(int pointCount);
    void handleCircleSelectionRejected();
    void handleRoiSelectionRejected();
    void refreshDisplayedRoiOverlay();
    void updateBottomButtons();
    void rerunImportedTest();
    void runCirclePresenceOnFrame(const cv::Mat &frame,
                                  const cv::Mat &referenceImage,
                                  const QString &imageTitle,
                                  const QString &emptyFrameMessage,
                                  bool referenceTest = false);
    void displayCirclePresenceResult(const ToolResult &result);
    void displayCirclePresenceError(const QString &status, const QString &message);
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    QString detectRoiStatusText() const;
    QRectF effectiveRoiNormalized() const;
    bool isDetectPolygonMode() const;
    bool isDetectCircleMode() const;

    Ui::CirclePresenceDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_basicDetectionRegionGroup;
    QButtonGroup *m_detectionRegionGroup;
    QButtonGroup *m_basicResultPresenceGroup;
    QButtonGroup *m_resultPresenceGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_exitTestButton = nullptr;
    TemplateLocationAdapter m_testTemplateLocationAdapter;
    PositionCorrectionAdapter m_testPositionCorrectionAdapter;
    CirclePresenceAdapter m_testCirclePresenceAdapter;
    ToolEngine m_testToolEngine;
    ToolEngine *m_sharedToolEngine = nullptr;
    QVector<ToolConfig> m_toolChainTestConfigs;
    int m_toolChainTestIndex = -1;
    ReferencePositionCorrectionConfig m_referencePositionCorrection;
    QString m_toolId;
    bool m_enabled = true;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> m_detectPolygonNormalized;
    CircleRoi m_detectCircleNormalized;
    cv::Mat m_importedTestFrame;
    QString m_importedTestImageTitle;
    bool m_importedTestActive = false;
    QString m_loadedPositionCorrectionSourceId;
    bool m_showPositionCorrectionMatchContour = true;
    bool m_circlePresenceRunning = false;
};

#endif // CIRCLEPRESENCEDIALOG_H
