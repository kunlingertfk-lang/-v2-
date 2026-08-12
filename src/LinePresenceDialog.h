#ifndef LINEPRESENCEDIALOG_H
#define LINEPRESENCEDIALOG_H

#include <QDialog>
#include <QRectF>

#include "frame/FrameViewHelper.h"
#include "tooladapters/LinePresenceAdapter.h"
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
class LinePresenceDialog;
}
QT_END_NAMESPACE

struct LinePresenceConfig
{
    QString detectRegionType = QStringLiteral("line_band");
    QPointF searchLineP1 = QPointF(0.15, 0.5);
    QPointF searchLineP2 = QPointF(0.85, 0.5);
    double searchBandWidth = 0.08;
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
    QString positionCorrectionSourceId;
    bool showPositionCorrectionMatchContour = true;
    int sensitivity = 60;
    int lineDegree = 25;
    QString edgePolarity = QStringLiteral("any");
    QString edgeType = QStringLiteral("strongest");
    QString judgeBasis = QStringLiteral("presence");
    bool existOk = true;
};

class LinePresenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LinePresenceDialog(QWidget *parent = nullptr);
    ~LinePresenceDialog() override;

    LinePresenceConfig configuration() const;
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
    void setupUiState();
    void connectControls();
    void applyAdaptiveWindowSize();
    void fitPreview();
    void updateBottomButtons();
    void rerunImportedTest();
    void showReferenceImage();
    void showFrameForRoiEditing();
    void startDetectRoiEditing();
    void showDetectRoiTodo(const QString &message);
    void handleLineBandChanged(const LineBandRoi &roi);
    void handleLineBandSelectionRejected();
    void handleRoiChanged(const QRectF &roi);
    void handleRoiSelectionRejected();
    void runLinePresenceOnFrame(const cv::Mat &frame,
                                const cv::Mat &referenceImage,
                                const QString &imageTitle,
                                const QString &emptyFrameMessage,
                                bool referenceTest = false);
    void displayLinePresenceResult(const ToolResult &result);
    void displayLinePresenceError(const QString &status, const QString &message);
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    QString detectRoiStatusText() const;
    QRectF effectiveRoiNormalized() const;
    LineBandRoi effectiveLineBandRoi() const;

    Ui::LinePresenceDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_basicResultPresenceGroup;
    QButtonGroup *m_resultPresenceGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_exitTestButton = nullptr;
    QPushButton *m_pcImportButton = nullptr;
    LinePresenceAdapter m_testLinePresenceAdapter;
    TemplateLocationAdapter m_testTemplateLocationAdapter;
    PositionCorrectionAdapter m_testPositionCorrectionAdapter;
    ToolEngine m_testToolEngine;
    PositionCorrectionDialogTestContext m_toolChainTestContext;
    QString m_toolId;
    bool m_enabled = true;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    LineBandRoi m_lineBandRoi;
    cv::Mat m_importedTestFrame;
    QString m_importedTestImageTitle;
    bool m_importedTestActive = false;
    bool m_linePresenceRunning = false;
    bool m_hasAcceptedToolConfig = false;
    ToolConfig m_acceptedToolConfig;
};

#endif // LINEPRESENCEDIALOG_H
