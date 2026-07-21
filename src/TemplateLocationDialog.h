#ifndef TEMPLATELOCATIONDIALOG_H
#define TEMPLATELOCATIONDIALOG_H

#include <QDialog>
#include <QJsonArray>
#include <QMetaObject>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "tooladapters/TemplateLocationAdapter.h"
#include "frame/FrameViewHelper.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolPreviewSnapshot.h"

class QButtonGroup;
class QFrame;
class QPushButton;
class QTableWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class TemplateLocationDialog; }
QT_END_NAMESPACE

class TemplateLocationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TemplateLocationDialog(QWidget *parent = nullptr);
    ~TemplateLocationDialog() override;

    void loadFromConfig(const ToolConfig &config);
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;

private:
    enum class EditTarget {
        None,
        TemplateRect,
        TemplatePolygon,
        SearchRect,
        SearchCircle,
        SearchPolygon,
        SearchGlobal
    };

    void setupUiState();
    void connectControls();
    void setAdvancedVisible(bool visible);
    void startEditing(EditTarget target);
    void stopEditing();
    void showReferenceImage();
    void markModelDirty();
    void deleteTemplate();
    bool validateParameters(QString *message = nullptr) const;
    bool validateTemplate(QString *message = nullptr) const;
    void createTemplate();
    void runReferenceTest();
    void toggleContinuousTest();
    void runOnFrame(const cv::Mat &frame, const QString &title);
    void displayResult(const ToolResult &result);
    QString polarityValue() const;
    void setPolarityValue(const QString &value);
    QString contrastModeValue() const;
    void updateContrastControls();
    void updateOriginControls();
    void updateMatchResultTable(const QJsonArray &matches);
    void setMatchResultsExpanded(bool expanded);

    Ui::TemplateLocationDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    QButtonGroup *m_modeGroup = nullptr;
    QButtonGroup *m_templateGroup = nullptr;
    QButtonGroup *m_searchGroup = nullptr;
    TemplateLocationAdapter m_testAdapter;
    ToolEngine m_testEngine;
    QMetaObject::Connection m_frameConnection;
    ToolConfig m_config;
    ToolPreviewSnapshot m_snapshot;
    QRectF m_templateRoi;
    QVector<QPointF> m_templatePolygon;
    QRectF m_searchRoi = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> m_searchPolygon;
    CircleRoi m_searchCircle;
    QString m_templateRegionType = QStringLiteral("rectangle");
    QString m_searchRegionType = QStringLiteral("full");
    QString m_modelCacheKey;
    EditTarget m_editTarget = EditTarget::None;
    bool m_modelCreated = false;
    bool m_creatingTemplate = false;
    QVector<ToolOverlay> m_templateDisplayOverlays;
    bool m_running = false;
    bool m_processing = false;
    QString m_originMode = QStringLiteral("centroid");
    QPointF m_customOriginNormalized = QPointF(0.5, 0.5);
    QFrame *m_matchResultDrawer = nullptr;
    QPushButton *m_matchResultToggle = nullptr;
    QTableWidget *m_matchResultTable = nullptr;
    bool m_matchResultsExpanded = false;
    ToolResult m_lastDisplayResult;
};

#endif // TEMPLATELOCATIONDIALOG_H
