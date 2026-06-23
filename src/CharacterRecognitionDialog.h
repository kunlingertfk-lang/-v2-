#ifndef CHARACTERRECOGNITIONDIALOG_H
#define CHARACTERRECOGNITIONDIALOG_H

#include <QDialog>
#include <QImage>
#include <QMetaObject>
#include <QRectF>
#include <QVector>

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolPreviewSnapshot.h"
#include "toolcore/ToolResult.h"
#include "tooladapters/OcrAdapter.h"

#include <opencv2/core.hpp>

class QButtonGroup;
class QPushButton;
class QResizeEvent;
class QTimer;
class FrameViewHelper;

QT_BEGIN_NAMESPACE
namespace Ui {
class CharacterRecognitionDialog;
}
QT_END_NAMESPACE

struct CharacterRecognitionConfig
{
    bool independentPositionCorrection = true;
    QString positionCorrection;
    QString resultBasis;
    int minCount = 1;
    int maxCount = 10;
    int minScore = 70;
    QString baselineText;
    QString modelName;
};

class CharacterRecognitionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CharacterRecognitionDialog(QWidget *parent = nullptr);
    ~CharacterRecognitionDialog() override;

    CharacterRecognitionConfig configuration() const;
    ToolConfig toToolConfig() const;
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    void loadFromConfig(const ToolConfig &config);
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
    enum class OcrUiMode {
        Edit,
        TestReady,
        Continuous,
        SingleShot
    };

    void setupUiState();
    void connectControls();
    void applyAdaptiveWindowSize();
    void setUiMode(OcrUiMode mode);
    void showReferenceImage();
    void showLiveImage(const QImage &image);
    void showSingleShotImage();
    void updateBottomButtons();
    void fitPreview();
    void startContinuousRun();
    void stopContinuousRun();
    void runContinuousTick();
    void runReferenceTest();
    void runOcrOnFrame(const cv::Mat &frame, const QString &imageTitle, bool referenceTest = false);
    void displayOcrResult(const ToolResult &result);
    void displayOcrError(const QString &status, const QString &message);
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    QVector<ToolOverlay> displayOverlaysWithoutRoi(const QVector<ToolOverlay> &overlays) const;

    Ui::CharacterRecognitionDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_regionGroup;
    FrameViewHelper *m_previewHelper;
    QPushButton *m_exitTestButton = nullptr;
    QTimer *m_continuousTimer = nullptr;
    OcrAdapter m_testOcrAdapter;
    ToolEngine m_testToolEngine;
    QMetaObject::Connection m_frameUpdatedConnection;
    QString m_toolId;
    bool m_enabled = true;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    OcrUiMode m_uiMode = OcrUiMode::Edit;
    bool m_ocrRunning = false;
};

#endif // CHARACTERRECOGNITIONDIALOG_H
