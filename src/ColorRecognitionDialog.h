#ifndef COLORRECOGNITIONDIALOG_H
#define COLORRECOGNITIONDIALOG_H

#include <QDialog>
#include <QFutureWatcher>
#include <QRectF>
#include <QString>
#include <QVector>

#include "ColorTemplateDialog.h"
#include "frame/FrameViewHelper.h"
#include "tooladapters/ColorRecognitionAdapter.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFrame;
class QListWidgetItem;
class QPushButton;
class QResizeEvent;
class QTimer;
class QToolButton;

QT_BEGIN_NAMESPACE
namespace Ui {
class ColorRecognitionDialog;
}
QT_END_NAMESPACE

struct ColorRecognitionDialogConfig
{
    QVector<ColorRecognitionTemplateData> templates;
    QString activeTemplateId;
    QString judgeMode = QStringLiteral("min_score");
    int minScore = 80;
    QString expectedLabel;
};

class ColorRecognitionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColorRecognitionDialog(QWidget *parent = nullptr);
    ~ColorRecognitionDialog() override;

    ColorRecognitionDialogConfig configuration() const;
    ToolConfig toToolConfig() const;
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    void loadFromConfig(const ToolConfig &config);
    QString summaryText() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void finishConfiguration();
    void runTest();
    void exitTestMode();

private:
    enum class TestUiMode {
        Edit,
        Continuous,
        TestPaused
    };

    void setupUiState();
    void connectControls();
    void setAllParamsMode(bool allMode);
    void addTemplate();
    void editCurrentTemplate();
    void importTemplate();
    void exportCurrentTemplate();
    void renameCurrentTemplate();
    void deleteCurrentTemplate();
    void updateTemplateList();
    void updateActiveTemplateSummary();
    void updateExpectedLabelCombo();
    void updateJudgementControls();
    void handleTemplateListItemClicked(QListWidgetItem *item);
    int currentTemplateIndex() const;
    ColorRecognitionTemplateData *activeTemplate();
    const ColorRecognitionTemplateData *activeTemplate() const;
    QString activeTemplateId() const;
    void performTestRun();
    void runReferenceTest();
    void runOnceInTestMode();
    void stopLiveTestRun();
    void updateBottomButtons();
    void fitPreview();
    void showPreviewImage();
    void showFrameForRoiEditing();
    void startGlobalDetection();
    void startRectangleRoiEditing();
    void startCircleRoiEditing();
    void showUnsupportedRegionMessage();
    void syncRegionButtons(bool rectangleRegion);
    void handleRoiChanged(const QRectF &roi);
    void handleRoiSelectionRejected();
    void handleCircleRoiChanged(const CircleRoi &roi);
    void handleCircleRoiSelectionRejected();
    void startMaskEditing();
    void startMaskPolygonDrawing();
    void finishMaskEditing();
    void handleMaskPolygonChanged(const QVector<QPointF> &points);
    void handleMaskPolygonSelectionRejected(int pointCount);
    void syncMaskControls();
    void refreshPositionCorrectionControls();
    void refreshDisplayedRoiOverlay();
    void displayResult(const ToolResult &result, bool referenceSource);
    void displayError(const QString &status, const QString &message);
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    QString roiStatusText() const;
    QRectF effectiveRoiNormalized() const;

    Ui::ColorRecognitionDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_regionGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    QString m_toolId;
    bool m_enabled = true;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString m_detectRegionType = QStringLiteral("rectangle");
    CircleRoi m_circleRoiNormalized;
    bool m_globalDetection = false;
    QVector<ColorRecognitionTemplateData> m_templates;
    QString m_activeTemplateId;
    int m_displayedSampleIndex = -1;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    ColorRecognitionAdapter m_testAdapter;
    QFrame *m_maskCard = nullptr;
    QPushButton *m_maskEditButton = nullptr;
    QToolButton *m_maskPolygonButton = nullptr;
    QPushButton *m_maskFinishButton = nullptr;
    QVector<QPointF> m_maskPolygonNormalized;
    bool m_maskEditing = false;
    bool m_previewUsesReferenceImage = true;
    QTimer *m_testRunTimer = nullptr;
    QPushButton *m_referenceTestButton = nullptr;
    QPushButton *m_exitTestButton = nullptr;
    TestUiMode m_testUiMode = TestUiMode::Edit;
    bool m_liveTestRunning = false;
    QFutureWatcher<ToolResult> *m_testRunWatcher = nullptr;
    bool m_testRunBusy = false;
    int m_testRunGeneration = 0;
};

#endif // COLORRECOGNITIONDIALOG_H
