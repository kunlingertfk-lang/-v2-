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

QT_BEGIN_NAMESPACE
namespace Ui {
class ReferenceImageDialog;
}
QT_END_NAMESPACE

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
        MaskPolygon
    };

    void setupUiState();
    void connectNavigation();
    void setupReferenceImageControls();
    void refreshSchemeHeader();
    void ensureCameraRunning();
    void updateReferenceImageControls();
    void refreshCurrentImage();
    void refreshReferenceImage();
    void setupPositionCorrectionControls();
    void loadPositionCorrectionConfig();
    /** 开始在基准图上拖拽绘制矩形位置修正模板区域。 */
    void startReferencePositionRectEditing();
    /** 开始在基准图上逐点绘制多边形位置修正模板区域。 */
    void startReferencePositionPolygonEditing();
    /** 开始绘制矩形、圆形或多边形模板屏蔽区。 */
    void startReferencePositionMaskEditing(PositionCorrectionRoiEditMode mode);
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

    Ui::ReferenceImageDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_captureImageButton = nullptr;
    QPushButton *m_exitCaptureButton = nullptr;
    QFrame *m_positionSettingsFrame = nullptr;
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
    bool m_liveCaptureMode = false;
    PositionCorrectionRoiEditMode m_positionRoiEditMode =
            PositionCorrectionRoiEditMode::None;
    ReferencePositionCorrectionConfig m_referencePositionCorrection;
    TemplateLocationHalconRunner m_referencePositionRunner;
    QVector<ToolOverlay> m_referencePositionMatchOverlays;
};

#endif // REFERENCEIMAGEDIALOG_H
