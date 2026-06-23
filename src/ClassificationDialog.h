#ifndef CLASSIFICATIONDIALOG_H
#define CLASSIFICATIONDIALOG_H

#include <QDialog>
#include <QRectF>

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"

class FrameViewHelper;
class QButtonGroup;
class QResizeEvent;

QT_BEGIN_NAMESPACE
namespace Ui {
class ClassificationDialog;
}
QT_END_NAMESPACE

struct ClassificationConfig
{
    bool independentPositionCorrection = true;
    QString positionCorrection;
    QString modelName;
    QString resultBasis = QStringLiteral("最低得分");
    int minScore = 50;
    QString category;
    QString judgeType = QStringLiteral("所有检测区域输出结果为 OK");
};

class ClassificationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ClassificationDialog(QWidget *parent = nullptr);
    ~ClassificationDialog() override;

    ClassificationConfig configuration() const;
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
    void setAllParamsMode(bool allMode);
    void showStageTodoMessage(const QString &actionName);
    void fitPreview();
    void showPreviewImage();
    void showFrameForRoiEditing();
    void startRectangleRoiEditing();
    void showFreeRoiUnsupported();
    void syncRegionButtons(bool rectangleRegion);
    void handleRoiChanged(const QRectF &roi);
    void handleRoiSelectionRejected();
    void refreshDisplayedRoiOverlay();
    void setViewerStatusText(const QString &displayText,
                             const QString &tooltipText = QString());
    QString roiStatusText() const;
    QRectF effectiveRoiNormalized() const;

    Ui::ClassificationDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_regionGroup;
    QButtonGroup *m_allRegionGroup;
    FrameViewHelper *m_previewHelper = nullptr;
    QString m_toolId;
    bool m_enabled = true;
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    ToolPreviewSnapshot m_referencePreviewSnapshot;
};

#endif // CLASSIFICATIONDIALOG_H
