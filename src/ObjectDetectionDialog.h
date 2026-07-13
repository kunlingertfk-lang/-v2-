#ifndef OBJECTDETECTIONDIALOG_H
#define OBJECTDETECTIONDIALOG_H

#include <QDialog>
#include <QRectF>

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"

class QButtonGroup;
class QResizeEvent;
class QSpinBox;
class FrameViewHelper;

QT_BEGIN_NAMESPACE
namespace Ui {
class ObjectDetectionDialog;
}
QT_END_NAMESPACE

struct ObjectDetectionConfig
{
    bool independentPositionCorrection = true;
    QString positionCorrection;
    QString modelName;
    int maxFindCount = 1;
    int minScore = 50;
    int maxOverlap = 50;
    QString sortType;
    bool angleEnabled = false;
    int minAngle = -180;
    int maxAngle = 180;
    bool widthEnabled = false;
    int minWidth = 1;
    int maxWidth = 2048;
    bool heightEnabled = false;
    int minHeight = 1;
    int maxHeight = 1536;
    bool boundaryFilterEnabled = false;
    int overlapRatio = 50;
    bool classFilterEnabled = false;
    QString classFilter;
    QString resultBasis;
    int minCount = 0;
    int maxCount = 1;
    int resultMinScore = 50;
    QString category;
};

class ObjectDetectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ObjectDetectionDialog(QWidget *parent = nullptr);
    ~ObjectDetectionDialog() override;

    ObjectDetectionConfig configuration() const;
    ToolConfig toToolConfig() const;
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    void loadFromConfig(const ToolConfig &config);
    QString summaryText() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void finishConfiguration();

private:
    void setupUiState();
    void connectControls();
    void fitPreview();
    void bindRange(QSpinBox *minimumSpinBox, QSpinBox *maximumSpinBox);
    void setAllParamsMode(bool allMode);
    void showStageTodoMessage(const QString &actionName);
    void showPreviewImage();
    void showFrameForRoiEditing();
    void startRectangleRoiEditing();
    void showFreeRoiUnsupported();
    void syncRegionButtons(bool rectangleRegion);
    void handleRoiChanged(const QRectF &roi);
    void handleRoiSelectionRejected();
    void refreshDisplayedRoiOverlay();
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString());
    QString roiStatusText() const;
    QRectF effectiveRoiNormalized() const;

    Ui::ObjectDetectionDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_regionGroup;
    QButtonGroup *m_allRegionGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    QString m_toolId;
    bool m_enabled = true;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    bool m_showBoxes = true;
    bool m_showLabels = true;
    bool m_showScores = true;
};

#endif // OBJECTDETECTIONDIALOG_H
