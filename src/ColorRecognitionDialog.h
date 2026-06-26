#ifndef COLORRECOGNITIONDIALOG_H
#define COLORRECOGNITIONDIALOG_H

#include <QDialog>
#include <QRectF>
#include <QString>
#include <QVector>

#include "algorithms/recognition/ColorRecognitionHalconRunner.h"
#include "tooladapters/ColorRecognitionAdapter.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"

class FrameViewHelper;
class QButtonGroup;
class QResizeEvent;

QT_BEGIN_NAMESPACE
namespace Ui {
class ColorRecognitionDialog;
}
QT_END_NAMESPACE

struct ColorRecognitionDialogLabel
{
    QString name;
    int classId = 0;
};

struct ColorRecognitionDialogSample
{
    QString label;
    int classId = 0;
    QVector<double> feature;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
};

struct ColorRecognitionDialogConfig
{
    QString modelName = QStringLiteral("颜色模型");
    QString featureType = QStringLiteral("histogram");
    QString sensitivity = QStringLiteral("medium");
    bool brightnessEnabled = true;
    int knnK = 3;
    QString knnDistance = QStringLiteral("halcon_l2");
    QVector<ColorRecognitionDialogLabel> labels;
    QVector<ColorRecognitionDialogSample> samples;
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

private:
    void setupUiState();
    void connectControls();
    void setAllParamsMode(bool allMode);
    void addLabel();
    void renameCurrentLabel();
    void deleteCurrentLabel();
    void addSampleFromCurrentRoi();
    void updateJudgementControls();
    void updateLabelCombos();
    void updateSampleCount();
    void ensureDefaultLabel();
    int nextClassId() const;
    int currentClassId() const;
    QString currentLabelName() const;
    ColorRecognitionHalconConfig featureExtractionConfig() const;
    void fitPreview();
    void showPreviewImage();
    void showFrameForRoiEditing();
    void startRectangleRoiEditing();
    void showUnsupportedRegionMessage();
    void syncRegionButtons(bool rectangleRegion);
    void handleRoiChanged(const QRectF &roi);
    void handleRoiSelectionRejected();
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
    QVector<ColorRecognitionDialogLabel> m_labels;
    QVector<ColorRecognitionDialogSample> m_samples;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    ColorRecognitionAdapter m_testAdapter;
    ColorRecognitionHalconRunner m_featureRunner;
};

#endif // COLORRECOGNITIONDIALOG_H
