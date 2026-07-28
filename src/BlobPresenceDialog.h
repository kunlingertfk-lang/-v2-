#ifndef BLOBPRESENCEDIALOG_H
#define BLOBPRESENCEDIALOG_H

#include <QDialog>
#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "frame/FrameViewHelper.h"
#include "tooladapters/BlobPresenceAdapter.h"
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
class BlobPresenceDialog;
}
QT_END_NAMESPACE

struct BlobPresenceConfig
{
    QString detectRegionType = QStringLiteral("rect");
    QVector<QPointF> detectPolygonNormalized;
    CircleRoi detectCircleNormalized;
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
    QString positionCorrectionSourceId;
    bool showPositionCorrectionMatchContour = true;
    int grayMin = 0;
    int grayMax = 255;
    bool invertRange = false;
    int areaMin = 10;
    int areaMax = 999999;
    bool maskOutputEnabled = false;
    QString judgeBasis = QStringLiteral("presence");
    bool existOk = true;
    int timeoutMs = 1000;
};

class BlobPresenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BlobPresenceDialog(QWidget *parent = nullptr);
    ~BlobPresenceDialog() override;

    BlobPresenceConfig configuration() const;
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
    void runBlobPresenceOnFrame(const cv::Mat &frame,
                                const cv::Mat &referenceImage,
                                const QString &imageTitle,
                                const QString &emptyFrameMessage,
                                bool referenceTest = false);
    void displayBlobPresenceResult(const ToolResult &result);
    void displayBlobPresenceError(const QString &status, const QString &message);
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    QString detectRoiStatusText() const;
    QRectF effectiveRoiNormalized() const;
    bool isDetectPolygonMode() const;
    bool isDetectCircleMode() const;

    Ui::BlobPresenceDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_basicDetectionRegionGroup;
    QButtonGroup *m_detectionRegionGroup;
    QButtonGroup *m_basicResultPresenceGroup;
    QButtonGroup *m_resultPresenceGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_exitTestButton = nullptr;
    TemplateLocationAdapter m_testTemplateLocationAdapter;
    PositionCorrectionAdapter m_testPositionCorrectionAdapter;
    BlobPresenceAdapter m_testBlobPresenceAdapter;
    ToolEngine m_testToolEngine;
    ToolEngine *m_sharedToolEngine = nullptr;
    QVector<ToolConfig> m_toolChainTestConfigs;
    int m_toolChainTestIndex = -1;
    ReferencePositionCorrectionConfig m_referencePositionCorrection;
    QString m_loadedPositionCorrectionSourceId;
    bool m_showPositionCorrectionMatchContour = true;
    QString m_toolId;
    bool m_enabled = true;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> m_detectPolygonNormalized;
    CircleRoi m_detectCircleNormalized;
    cv::Mat m_importedTestFrame;
    QString m_importedTestImageTitle;
    bool m_importedTestActive = false;
    bool m_blobPresenceRunning = false;
};

#endif // BLOBPRESENCEDIALOG_H
