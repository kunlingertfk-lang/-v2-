#ifndef REFERENCEIMAGEDIALOG_H
#define REFERENCEIMAGEDIALOG_H

#include <QDialog>

#include "toolcore/PositionCorrection.h"

class FrameViewHelper;
class QPushButton;
class QFrame;
class QLabel;

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

private slots:
    void openCameraParamsDialog();
    void openToolsDialog();
    void openOutputDialog();
    void showCurrentImageMode();
    void captureReferenceImage();
    void showReferenceImageMode();
    void importReferenceImageFromPc();
    void editCurrentSchemeName();
    void saveCurrentScheme();
    void saveCurrentSchemeAs();
    void updatePositionCorrectionUi(bool enabled);

private:
    enum class PositionCorrectionRoiEditMode {
        None,
        Rectangle,
        Polygon
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
    /** 校验当前模板区域并退出 ROI 编辑状态。 */
    bool finishReferencePositionRoiEditing();
    /** 接收公共预览控件生成的归一化矩形 ROI。 */
    void handleReferencePositionRectChanged(const QRectF &roiNormalized);
    /** 接收公共预览控件生成的归一化多边形 ROI。 */
    void handleReferencePositionPolygonChanged(const QVector<QPointF> &pointsNormalized);
    /** 根据当前配置在基准图预览上恢复矩形或多边形 ROI。 */
    void restoreReferencePositionRoi();
    /** 停止所有位置修正 ROI 绘制手势，并可选择恢复已确认区域。 */
    void stopReferencePositionRoiEditing(bool restoreConfirmedRoi);
    /** 判断当前配置是否包含与模板类型一致的有效 ROI。 */
    bool hasReferencePositionTemplateRoi() const;
    /** 生成当前模板 ROI 的可读状态文本。 */
    QString referencePositionRoiStatusText() const;

    Ui::ReferenceImageDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_captureImageButton = nullptr;
    QPushButton *m_exitCaptureButton = nullptr;
    QFrame *m_positionSettingsFrame = nullptr;
    QLabel *m_positionStatusLabel = nullptr;
    QPushButton *m_positionRectButton = nullptr;
    QPushButton *m_positionPolygonButton = nullptr;
    QPushButton *m_positionFinishButton = nullptr;
    QPushButton *m_positionTestButton = nullptr;
    bool m_liveCaptureMode = false;
    PositionCorrectionRoiEditMode m_positionRoiEditMode =
            PositionCorrectionRoiEditMode::None;
    ReferencePositionCorrectionConfig m_referencePositionCorrection;
};

#endif // REFERENCEIMAGEDIALOG_H
