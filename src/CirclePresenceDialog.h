#ifndef CIRCLEPRESENCEDIALOG_H
#define CIRCLEPRESENCEDIALOG_H

#include <QDialog>
#include <QImage>
#include <QRectF>

#include "tooladapters/CirclePresenceAdapter.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolResult.h"

#include <opencv2/core.hpp>

class QButtonGroup;
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
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
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
    void runCirclePresenceOnFrame(const cv::Mat &frame,
                                  const cv::Mat &referenceImage,
                                  const QString &imageTitle,
                                  const QString &emptyFrameMessage);
    void displayCirclePresenceResult(const ToolResult &result);
    void displayCirclePresenceError(const QString &status, const QString &message);
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    QString detectRoiStatusText() const;
    QRectF effectiveRoiNormalized() const;

    Ui::CirclePresenceDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_basicResultPresenceGroup;
    QButtonGroup *m_resultPresenceGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    CirclePresenceAdapter m_testCirclePresenceAdapter;
    ToolEngine m_testToolEngine;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    bool m_circlePresenceRunning = false;
};

#endif // CIRCLEPRESENCEDIALOG_H
