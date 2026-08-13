#ifndef REFERENCEIMAGEDIALOG_H
#define REFERENCEIMAGEDIALOG_H

#include <QDialog>

#include "algorithms/location/TemplateLocationHalconRunner.h"
#include "frame/RoiGeometry.h"
#include "toolcore/PositionCorrection.h"

class FrameViewHelper;
class QPushButton;
class QFrame;
class QLabel;
class QComboBox;
class QButtonGroup;
class QCheckBox;
class QDoubleSpinBox;
class QListWidget;
class QListWidgetItem;
class QSpinBox;
class QWidget;
class QVBoxLayout;

QT_BEGIN_NAMESPACE
namespace Ui {
class ReferenceImageDialog;
}
QT_END_NAMESPACE

namespace ReferenceTemplateBankValidation {
/** Pure multi-template anchor-consistency check shared by the editor and smoke. */
bool consistentFrozenPoses(const TemplateLocationModelBankConfig &bank,
                           const QJsonObject &posesByTemplateId,
                           QString *templateName = nullptr,
                           QString *message = nullptr);
}

class ReferenceImageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ReferenceImageDialog(QWidget *parent = nullptr);
    ~ReferenceImageDialog() override;

public slots:
    void prepareForDisplay();

private slots:
    void openCameraParamsDialog();
    void openToolsDialog();
    void openOutputDialog();
    void showCurrentImageMode();
    void captureReferenceImage();
    void showReferenceImageMode();
    void importReferenceImageFromPc();
    void editCurrentSchemeName();
    bool saveCurrentScheme();
    void saveCurrentSchemeAs();
    void updatePositionCorrectionUi(bool enabled);

private:
    enum class PositionCorrectionRoiEditMode {
        None,
        Rectangle,
        Polygon,
        MaskRectangle,
        MaskCircle,
        MaskPolygon,
        SearchRectangle,
        SearchCircle,
        SearchPolygon
    };

    void setupUiState();
    void setupParameterModeControls();
    void setAdvancedVisible(bool visible);
    void connectNavigation();
    void setupReferenceImageControls();
    void refreshSchemeHeader();
    void ensureCameraRunning();
    void updateReferenceImageControls();
    void refreshCurrentImage();
    void refreshReferenceImage();
    void setupPositionCorrectionControls();
    void setupReferenceTemplateBankControls();
    void setupReferenceMatchingParameterControls(QVBoxLayout *settingsLayout);
    void loadPositionCorrectionConfig();
    void loadLocatorControls(const TemplateLocationModelBankConfig &config);
    void writeLocatorControls();
    void updateMatchingParameterControlState();
    void markLocatorDirty(bool modelParametersChanged = true);
    void markActiveTemplateDirty();
    bool validateLocatorControls(QString *message = nullptr) const;
    int activeTemplateIndex() const;
    void storeLegacyGeometryToActiveTemplate();
    void loadActiveTemplateGeometry();
    void refreshReferenceTemplateBank();
    void switchActiveTemplate(const QString &templateId);
    void addReferenceTemplate();
    void renameReferenceTemplate();
    void deleteReferenceTemplate();
    void handleReferenceTemplateItemChanged(QListWidgetItem *item);
    void reloadReferenceStateAfterImageChange();
    /** 开始在基准图上拖拽绘制矩形位置修正模板区域。 */
    void startReferencePositionRectEditing();
    /** 开始在基准图上逐点绘制多边形位置修正模板区域。 */
    void startReferencePositionPolygonEditing();
    /** 开始绘制矩形、圆形或多边形模板屏蔽区。 */
    void startReferencePositionMaskEditing(PositionCorrectionRoiEditMode mode);
    void startReferenceSearchRegionEditing(PositionCorrectionRoiEditMode mode);
    bool finishReferenceSearchRegionEditing();
    /** 校验当前模板区域并退出 ROI 编辑状态。 */
    bool finishReferencePositionRoiEditing();
    /** 校验屏蔽区并退出屏蔽 ROI 编辑状态。 */
    bool finishReferencePositionMaskEditing();
    /** 接收公共预览控件生成的归一化矩形 ROI。 */
    void handleReferencePositionRectChanged(const QRectF &roiNormalized);
    /** 接收公共预览控件生成的归一化多边形 ROI。 */
    void handleReferencePositionPolygonChanged(const QVector<QPointF> &pointsNormalized);
    /** 接收公共预览控件生成的圆形模板屏蔽区。 */
    void handleReferencePositionCircleChanged(const CircleRoi &circle);
    /** 根据当前配置在基准图预览上恢复矩形或多边形 ROI。 */
    void restoreReferencePositionRoi();
    /** 停止所有位置修正 ROI 绘制手势，并可选择恢复已确认区域。 */
    void stopReferencePositionRoiEditing(bool restoreConfirmedRoi);
    /** 判断当前配置是否包含与模板类型一致的有效 ROI。 */
    bool hasReferencePositionTemplateRoi() const;
    /** 生成当前模板 ROI 的可读状态文本。 */
    QString referencePositionRoiStatusText() const;
    /** ROI 完成后立即用基准图创建私有 HALCON 模型并自匹配验证，成功才标记可用。 */
    bool buildAndValidateReferencePositionModel();
    /** 显示基准图私有模板自匹配返回的轮廓和质心十字。 */
    void showReferencePositionMatchOverlays(const QVector<ToolOverlay> &overlays);
    /** 清除已经失效或不属于当前画面的模板匹配标记。 */
    void clearReferencePositionMatchOverlays();
    /** 根据定位点模式更新选择按钮和坐标文本。 */
    void updateReferencePositionOriginControls();
    /** 结束基准图定位点选择，恢复普通画布交互。 */
    void stopReferencePositionOriginSelection();
    /** 同时绘制自定义基准点与当前自匹配轮廓/定位点。 */
    void renderReferencePositionOverlays();
    /** 返回前向 locator 配置在旧版页面中的只读提示。 */
    QString referencePositionReadOnlyMessage() const;

    Ui::ReferenceImageDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_captureImageButton = nullptr;
    QPushButton *m_exitCaptureButton = nullptr;
    QFrame *m_positionSettingsFrame = nullptr;
    QButtonGroup *m_parameterModeGroup = nullptr;
    QWidget *m_templateMaskToolsWidget = nullptr;
    QFrame *m_basicMatchingCard = nullptr;
    QFrame *m_advancedMatchingCard = nullptr;
    QWidget *m_searchRegionToolsWidget = nullptr;
    QLabel *m_positionStatusLabel = nullptr;
    QPushButton *m_positionRectButton = nullptr;
    QPushButton *m_positionPolygonButton = nullptr;
    QPushButton *m_positionFinishButton = nullptr;
    QPushButton *m_positionMaskRectButton = nullptr;
    QPushButton *m_positionMaskCircleButton = nullptr;
    QPushButton *m_positionMaskPolygonButton = nullptr;
    QPushButton *m_positionMaskClearButton = nullptr;
    QPushButton *m_positionMaskFinishButton = nullptr;
    QPushButton *m_positionTestButton = nullptr;
    QComboBox *m_positionOriginModeComboBox = nullptr;
    QPushButton *m_positionSelectOriginButton = nullptr;
    QLabel *m_positionOriginValueLabel = nullptr;
    QListWidget *m_referenceTemplateBankList = nullptr;
    QLabel *m_referenceTemplateBankSummaryLabel = nullptr;
    QPushButton *m_addReferenceTemplateButton = nullptr;
    QPushButton *m_renameReferenceTemplateButton = nullptr;
    QPushButton *m_deleteReferenceTemplateButton = nullptr;
    QSpinBox *m_minScoreSpinBox = nullptr;
    QSpinBox *m_angleMinSpinBox = nullptr;
    QSpinBox *m_angleMaxSpinBox = nullptr;
    QSpinBox *m_scaleMinSpinBox = nullptr;
    QSpinBox *m_scaleMaxSpinBox = nullptr;
    QComboBox *m_polarityComboBox = nullptr;
    QComboBox *m_contrastModeComboBox = nullptr;
    QSpinBox *m_contrastSpinBox = nullptr;
    QSpinBox *m_minContrastSpinBox = nullptr;
    QSpinBox *m_numLevelsSpinBox = nullptr;
    QComboBox *m_subPixelComboBox = nullptr;
    QDoubleSpinBox *m_greedinessSpinBox = nullptr;
    QSpinBox *m_timeoutSpinBox = nullptr;
    QSpinBox *m_maxOverlapSpinBox = nullptr;
    QSpinBox *m_templatePrioritySpinBox = nullptr;
    QComboBox *m_primaryStrategyComboBox = nullptr;
    QComboBox *m_lockedTemplateComboBox = nullptr;
    QCheckBox *m_fusionEnabledCheckBox = nullptr;
    QDoubleSpinBox *m_fusionPositionToleranceSpinBox = nullptr;
    QDoubleSpinBox *m_fusionAngleToleranceSpinBox = nullptr;
    QDoubleSpinBox *m_fusionScaleToleranceSpinBox = nullptr;
    QPushButton *m_searchFullButton = nullptr;
    QPushButton *m_searchRectButton = nullptr;
    QPushButton *m_searchCircleButton = nullptr;
    QPushButton *m_searchPolygonButton = nullptr;
    QPushButton *m_searchFinishButton = nullptr;
    bool m_liveCaptureMode = false;
    PositionCorrectionRoiEditMode m_positionRoiEditMode =
            PositionCorrectionRoiEditMode::None;
    ReferencePositionCorrectionConfig m_referencePositionCorrection;
    TemplateLocationModelBankConfig m_locatorConfig;
    QString m_activeTemplateId;
    bool m_loadingLocatorControls = false;
    bool m_updatingTemplateBank = false;
    bool m_referenceLocatorBankEnvelope = false;
    bool m_referencePositionEditorReadOnly = false;
    QString m_referencePositionUnsupportedStatus;
    QString m_referencePositionUnsupportedMessage;
    TemplateLocationHalconRunner m_referencePositionRunner;
    QVector<ToolOverlay> m_referencePositionMatchOverlays;
};

#endif // REFERENCEIMAGEDIALOG_H
