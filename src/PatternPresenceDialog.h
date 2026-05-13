#ifndef PATTERNPRESENCEDIALOG_H
#define PATTERNPRESENCEDIALOG_H

#include <QDialog>
#include <QImage>
#include <QMetaObject>
#include <QRectF>

#include "tooladapters/PatternPresenceAdapter.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolResult.h"

#include <opencv2/core.hpp>

class QButtonGroup;
class QPushButton;
class QResizeEvent;
class QTimer;
class FrameViewHelper;

QT_BEGIN_NAMESPACE
namespace Ui {
class PatternPresenceDialog;
}
QT_END_NAMESPACE

struct PatternPresenceConfig
{
    QRectF templateRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString templateSource = QStringLiteral("referenceImage");
    QString templateImagePath;
    QString modelPath;
    bool modelAutoCreate = true;
    QString modelCacheKey;
    QString templateShapeType = QStringLiteral("rectangle");
    QString templateSensitivityMode = QStringLiteral("auto");
    int templateSensitivity = 2;
    QString detectRegionType = QStringLiteral("free");
    bool enablePositionCorrection = true;
    QString positionCorrectionSource;
    int minScore = 50;
    QString polarity;
    int scaleMin = 100;
    int scaleMax = 100;
    int angleStart = -45;
    int angleExtent = 90;
    int timeoutMs = 2000;
    bool showContourPoints = false;
    QString sortMode;
    QString judgeBasis;
    bool existOk = true;
    int scoreThreshold = 50;
};

class PatternPresenceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PatternPresenceDialog(QWidget *parent = nullptr);
    ~PatternPresenceDialog() override;

    PatternPresenceConfig configuration() const;
    ToolConfig toToolConfig() const;
    ToolConfig toolConfig() const;
    QString summaryText() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void finishConfiguration();
    void showProviderImage(const QImage &image);
    void handleTestRunButton();
    void handleFinishButton();
    void enterTestMode();
    void exitTestMode();
    void runOnceInTestMode();

private:
    enum class PresenceUiMode {
        Edit,
        TestReady,
        Continuous,
        SingleShot
    };

    enum class RoiEditTarget {
        None,
        TemplateRoi,
        DetectRoi
    };

    void setupUiState();
    void connectControls();
    void applyAdaptiveWindowSize();
    void setUiMode(PresenceUiMode mode);
    void showReferenceImage();
    void showLiveImage(const QImage &image);
    void showSingleShotImage();
    void updateBottomButtons();
    void fitPreview();
    void startContinuousRun();
    void stopContinuousRun();
    void runContinuousTick();
    void runReferenceTest();
    void runPatternPresenceOnFrame(const cv::Mat &frame, const QString &imageTitle);
    void displayPatternPresenceResult(const ToolResult &result);
    void displayPatternPresenceError(const QString &status, const QString &message);
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    void startTemplateRoiEditing();
    void finishTemplateRoiEditing();
    void startDetectRoiEditing(const QString &title);
    void showTemplateRoiTodo(const QString &message);
    void handleRoiChanged(const QRectF &roi);
    void handleRoiSelectionRejected();
    void refreshDisplayedRoiOverlay();
    QString templateRoiStatusText() const;
    QString detectRoiStatusText() const;

    Ui::PatternPresenceDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_basicTemplateRegionGroup;
    QButtonGroup *m_templateRegionGroup;
    QButtonGroup *m_basicDetectionRegionGroup;
    QButtonGroup *m_detectionRegionGroup;
    QButtonGroup *m_templateSensitivityGroup;
    QButtonGroup *m_basicResultPresenceGroup;
    QButtonGroup *m_resultPresenceGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_exitTestButton = nullptr;
    QTimer *m_continuousTimer = nullptr;
    PatternPresenceAdapter m_testPatternPresenceAdapter;
    ToolEngine m_testToolEngine;
    QMetaObject::Connection m_frameUpdatedConnection;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QRectF m_templateRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString m_modelCacheKey;
    PresenceUiMode m_uiMode = PresenceUiMode::Edit;
    RoiEditTarget m_roiEditTarget = RoiEditTarget::None;
    bool m_editingTemplateRoi = false;
    bool m_presenceRunning = false;
};

#endif // PATTERNPRESENCEDIALOG_H
