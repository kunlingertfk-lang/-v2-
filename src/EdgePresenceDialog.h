#ifndef EDGEPRESENCEDIALOG_H
#define EDGEPRESENCEDIALOG_H

#include <QDialog>
#include <QRectF>

#include "frame/FrameViewHelper.h"
#include "tooladapters/EdgePresenceAdapter.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolPreviewSnapshot.h"
#include "toolcore/ToolResult.h"

#include <opencv2/core.hpp>

class QButtonGroup;
class QResizeEvent;
class FrameViewHelper;

QT_BEGIN_NAMESPACE
namespace Ui {
class EdgePresenceDialog;
}
QT_END_NAMESPACE

struct EdgePresenceConfig
{
    QString detectRegionType = QStringLiteral("line_band");
    QPointF searchLineP1 = QPointF(0.15, 0.5);
    QPointF searchLineP2 = QPointF(0.85, 0.5);
    double searchBandWidth = 0.08;
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
    int sensitivity = 60;
    QString edgePolarity = QStringLiteral("any");
    QString judgeBasis = QStringLiteral("presence");
    bool existOk = true;
};

class EdgePresenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EdgePresenceDialog(QWidget *parent = nullptr);
    ~EdgePresenceDialog() override;

    EdgePresenceConfig configuration() const;
    ToolConfig toToolConfig() const;
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    void loadFromConfig(const ToolConfig &config);
    QString summaryText() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void finishConfiguration();
    void runReferenceTest();
    void runCameraTest();

private:
    void setupUiState();
    void connectControls();
    void applyAdaptiveWindowSize();
    void fitPreview();
    void showReferenceImage();
    void showFrameForRoiEditing();
    void startDetectRoiEditing();
    void showDetectRoiTodo(const QString &message);
    void handleLineBandChanged(const LineBandRoi &roi);
    void handleLineBandSelectionRejected();
    void handleRoiChanged(const QRectF &roi);
    void handleRoiSelectionRejected();
    void runEdgePresenceOnFrame(const cv::Mat &frame,
                                const cv::Mat &referenceImage,
                                const QString &imageTitle,
                                const QString &emptyFrameMessage,
                                bool referenceTest = false);
    void displayEdgePresenceResult(const ToolResult &result);
    void displayEdgePresenceError(const QString &status, const QString &message);
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    QString detectRoiStatusText() const;
    QRectF effectiveRoiNormalized() const;
    LineBandRoi effectiveLineBandRoi() const;

    Ui::EdgePresenceDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_basicResultPresenceGroup;
    QButtonGroup *m_resultPresenceGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    EdgePresenceAdapter m_testEdgePresenceAdapter;
    ToolEngine m_testToolEngine;
    QString m_toolId;
    bool m_enabled = true;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    LineBandRoi m_lineBandRoi;
    bool m_edgePresenceRunning = false;
    bool m_hasAcceptedToolConfig = false;
    ToolConfig m_acceptedToolConfig;
};

#endif // EDGEPRESENCEDIALOG_H
