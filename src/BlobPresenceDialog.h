#ifndef BLOBPRESENCEDIALOG_H
#define BLOBPRESENCEDIALOG_H

#include <QDialog>
#include <QImage>
#include <QRectF>

#include "tooladapters/BlobPresenceAdapter.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolResult.h"

#include <opencv2/core.hpp>

class QButtonGroup;
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
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
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
    void resetDetectRoi();
    void handleRoiChanged(const QRectF &roi);
    void handleRoiSelectionRejected();
    void runBlobPresenceOnFrame(const cv::Mat &frame,
                                const cv::Mat &referenceImage,
                                const QString &imageTitle,
                                const QString &emptyFrameMessage);
    void displayBlobPresenceResult(const ToolResult &result);
    void displayBlobPresenceError(const QString &status, const QString &message);
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    QString detectRoiStatusText() const;
    QRectF effectiveRoiNormalized() const;

    Ui::BlobPresenceDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_basicDetectionRegionGroup;
    QButtonGroup *m_detectionRegionGroup;
    QButtonGroup *m_basicResultPresenceGroup;
    QButtonGroup *m_resultPresenceGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    BlobPresenceAdapter m_testBlobPresenceAdapter;
    ToolEngine m_testToolEngine;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    bool m_blobPresenceRunning = false;
};

#endif // BLOBPRESENCEDIALOG_H
