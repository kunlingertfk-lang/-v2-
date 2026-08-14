#ifndef TEMPLATELOCATIONDIALOG_H
#define TEMPLATELOCATIONDIALOG_H

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaObject>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QStringList>
#include <QVector>

#include "algorithms/location/TemplateLocationConfig.h"
#include "tooladapters/TemplateLocationAdapter.h"
#include "frame/FrameViewHelper.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolPreviewSnapshot.h"

class QButtonGroup;
class QCheckBox;
class QFrame;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTableWidget;
class QToolButton;
class QWidget;
class ReferenceBaseSelector;
struct ReferenceFrameSetSnapshot;

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
    struct TemplateItemState {
        QString templateId;
        QString name;
        bool enabled = true;
        int priority = 0;
        int order = 0;
        QString sourceBaseId;
        QString regionType = QStringLiteral("rectangle");
        QRectF roi;
        QVector<QPointF> polygon;
        QString maskRegionType = QStringLiteral("none");
        QRectF maskRoi;
        QVector<QPointF> maskPolygon;
        CircleRoi maskCircle;
        QString modelCacheKey;
        bool modelCreated = false;
        QVector<TemplateLocationRegionConfig> includeRegions;
        QVector<TemplateLocationRegionConfig> excludeRegions;
        bool useIndependentParameters = false;
        TemplateLocationMatchParameters independentParameters;
        bool independentParametersComplete = true;
        QJsonObject extra;
        QString modelError;
        QVector<ToolOverlay> displayOverlays;
    };

    enum class EditTarget {
        None,
        TemplateRect,
        TemplatePolygon,
        TemplateMaskRect,
        TemplateMaskCircle,
        TemplateMaskPolygon,
        SearchRect,
        SearchCircle,
        SearchPolygon,
        SearchGlobal
    };

    void setupUiState();
    void setupTemplateBankUi();
    void connectControls();
    void ensureV6Editing();
    void freezeCentroidOriginsForReferencedBases();
    void seedBaseBindingsFromProvider();
    void syncCurrentBaseBinding();
    void loadSelectedBaseBinding();
    QVector<TemplateLocationBaseBindingConfig> baseBindingsForOutput() const;
    TemplateLocationMatchParameters matchParametersFromUi(
            const TemplateLocationMatchParameters *preserved = nullptr) const;
    void applyMatchParametersToUi(
            const TemplateLocationMatchParameters &parameters);
    void flushParameterEditor();
    void loadParameterEditor();
    void handleParameterEditorChanged(bool rebuildModels);
    TemplateLocationTemplateConfig templateConfigFromState(
            const TemplateItemState &item) const;
    TemplateItemState templateStateFromConfig(
            const TemplateLocationTemplateConfig &item,
            int fallbackIndex) const;
    TemplateLocationModelBankConfig modelBankForOutput() const;
    QString selectedBaseId() const;
    QString activeTemplateBaseId() const;
    bool activeTemplateSourceAvailable() const;
    bool isTemplateGeometryTarget(EditTarget target) const;
    QString baseDisplayName(const QString &baseId) const;
    void validateAllReferenceBases();
    void setAdvancedVisible(bool visible);
    void startEditing(EditTarget target);
    void stopEditing();
    void showReferenceImage();
    void markModelDirty();
    void markAllModelsDirty();
    void invalidateRunPreview();
    void deleteTemplate();
    void addTemplateItem();
    void renameActiveTemplateItem();
    void deleteActiveTemplateItem();
    void handleTemplateItemChanged(QListWidgetItem *item);
    void switchActiveTemplate(const QString &templateId);
    void flushActiveTemplateEditor();
    void loadActiveTemplateEditor();
    void refreshTemplateList();
    void refreshActiveTemplateUi();
    int activeTemplateIndex() const;
    TemplateItemState activeTemplateForOutput() const;
    bool validateTemplateItem(const TemplateItemState &item,
                              QString *message = nullptr) const;
    bool validateTemplateBank(bool requireModels,
                              QString *message = nullptr) const;
    ToolConfig activeTemplateToolConfig() const;
    void commitPendingCacheDeletes();
    bool validateParameters(QString *message = nullptr) const;
    bool validateTemplate(QString *message = nullptr) const;
    void createTemplate();
    void runReferenceTest();
    void toggleContinuousTest();
    void runOnFrame(const cv::Mat &frame, const QString &title,
                    const QString &displayBaseId = QString(),
                    const ReferenceFrameSetSnapshot *referenceSnapshot = nullptr);
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
    QList<TemplateItemState> m_templates;
    QString m_activeTemplateId;
    bool m_usesTemplateBankSchema = false;
    int m_contractVersion = TemplateLocationConfig::CompositeBankParamsVersion;
    QVector<TemplateLocationBaseBindingConfig> m_baseBindings;
    TemplateLocationMatchParameters m_sharedParameters;
    QString m_selectedBaseId;
    QString m_runDisplayBaseId;
    QSize m_runDisplayImageSize;
    bool m_loadingConfig = false;
    bool m_updatingParameterEditor = false;
    bool m_templateIncludeEdited = false;
    bool m_templateExcludeEdited = false;
    bool m_configReadOnly = false;
    QString m_configReadOnlyStatus;
    QString m_configReadOnlyMessage;
    bool m_updatingTemplateList = false;
    QStringList m_pendingCacheDeletes;
    QRectF m_templateRoi;
    QVector<QPointF> m_templatePolygon;
    QString m_templateMaskRegionType = QStringLiteral("none");
    QRectF m_templateMaskRoi;
    QVector<QPointF> m_templateMaskPolygon;
    CircleRoi m_templateMaskCircle;
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
    QWidget *m_templateMaskRow = nullptr;
    QToolButton *m_templateMaskRectButton = nullptr;
    QToolButton *m_templateMaskCircleButton = nullptr;
    QToolButton *m_templateMaskPolygonButton = nullptr;
    QPushButton *m_templateMaskClearButton = nullptr;
    QFrame *m_templateBankCard = nullptr;
    QListWidget *m_templateBankList = nullptr;
    QLabel *m_templateBankSummaryLabel = nullptr;
    QPushButton *m_addTemplateItemButton = nullptr;
    QPushButton *m_renameTemplateItemButton = nullptr;
    QPushButton *m_deleteTemplateItemButton = nullptr;
    ReferenceBaseSelector *m_baseSelector = nullptr;
    QCheckBox *m_independentParametersCheckBox = nullptr;
    QLabel *m_parameterScopeLabel = nullptr;
    bool m_matchResultsExpanded = false;
    ToolResult m_lastDisplayResult;
};

#endif // TEMPLATELOCATIONDIALOG_H
