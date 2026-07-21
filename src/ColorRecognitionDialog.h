#ifndef COLORRECOGNITIONDIALOG_H
#define COLORRECOGNITIONDIALOG_H

#include <QDialog>
#include <QFutureWatcher>
#include <QRectF>
#include <QString>
#include <QVector>

#include <opencv2/core.hpp>

#include "ColorTemplateDialog.h"
#include "frame/FrameInputMetadata.h"
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
    // Dialog 层配置快照：模板列表和判定规则会被写入 ToolConfig 保存。
    QVector<ColorRecognitionTemplateData> templates;
    QString activeTemplateId;
    QString judgeMode = QStringLiteral("min_score");
    int minScore = 80;
    int minCategoryConfidence = 80;
    int minClassifiedCoverage = 90;
    int expectedClassId = -1;
    QString expectedLabel;
};

class ColorRecognitionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColorRecognitionDialog(QWidget *parent = nullptr);
    ~ColorRecognitionDialog() override;

    // 返回 Dialog 当前内存态配置，供 UI 内部和保存前汇总使用。
    ColorRecognitionDialogConfig configuration() const;
    // 将当前 UI 状态序列化为 ToolConfig，形成项目保存和 Adapter 运行的统一配置。
    ToolConfig toToolConfig() const;
    // 对外暴露当前工具配置，保持与其他工具对话框的调用习惯一致。
    ToolConfig toolConfig() const;
    // 返回基准预览图快照，供重新打开或测试运行时恢复图像来源。
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    // 从已保存 ToolConfig 回显界面、模板、ROI、屏蔽区和判定规则。
    void loadFromConfig(const ToolConfig &config);
    // 生成工具列表中展示的简要说明。
    QString summaryText() const;

protected:
    // 窗口尺寸变化后同步预览区域缩放。
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // 完成配置：校验当前状态、生成 ToolConfig 并关闭 Dialog。
    void finishConfiguration();
    // 进入相机/连续测试流程。
    void runTest();
    // 退出测试态并恢复编辑态按钮和预览状态。
    void exitTestMode();

private:
    enum class EditState {
        None,
        DetectRect,
        DetectCircle,
        DetectMaskPolygon
    };

    enum class TestUiMode {
        Edit,
        Continuous,
        TestPaused
    };

    // 测试态下固定的检测图像来源：进入测试后 ROI 绘制完成会按此来源立即重跑检测。
    enum class LiveTestSource {
        None,       // 编辑态，不触发自动重测
        Reference,  // 基准图测试：始终在基准图上重测
        Camera      // 相机测试：连续态用最新帧，单次态用进入时缓存的快照帧
    };

    void setupUiState();                         // 初始化控件状态、按钮组、预览 helper 和测试态辅助控件。
    void connectControls();                      // 建立 UI 控件、ROI helper、模板列表和测试按钮的信号连接。
    void setAllParamsMode(bool allMode);         // 切换“基础/全部”参数页的显示范围。
    void addTemplate();                          // 新增颜色模板并打开模板编辑对话框采集样本。
    void editCurrentTemplate();                  // 编辑当前选中的颜色模板。
    void importTemplate();                       // 从 JSON 文件导入颜色模板。
    void exportCurrentTemplate();                // 将当前模板导出为 JSON 文件。
    void renameCurrentTemplate();                // 修改当前模板名称并刷新列表。
    void deleteCurrentTemplate();                // 删除当前模板并维护选中模板 id。
    void updateTemplateList();                   // 根据模板数据重建左侧模板列表和缩略图。
    void updateActiveTemplateSummary();          // 刷新当前模板的标签、样本和参数摘要。
    void updateExpectedLabelCombo();             // 根据当前模板标签刷新“期望类别”下拉框。
    void updateJudgementControls();              // 根据判定模式切换分数阈值和类别控件可用状态。
    void updateGmmRejectionHint(double value);   // 显示 K-sigma 语义和过高阈值警告。
    void handleTemplateListItemClicked(QListWidgetItem *item); // 响应模板列表点击并切换当前模板。
    int currentTemplateIndex() const;            // 返回当前活动模板在数组中的索引。
    ColorRecognitionTemplateData *activeTemplate(); // 返回可编辑的当前模板指针。
    const ColorRecognitionTemplateData *activeTemplate() const; // 返回只读当前模板指针。
    QString activeTemplateId() const;            // 返回当前模板 id，缺省时回退到首个模板。
    void performTestRun();                       // 进入连续相机测试并启动定时重跑。
    void runReferenceTest();                     // 使用基准图执行一次颜色识别测试。
    void runOnceInTestMode();                    // 在测试态锁定当前相机帧执行一次检测。
    void stopLiveTestRun();                      // 停止连续测试定时器和在途检测标记。
    // 依据当前 m_liveTestSource 取源帧（基准图 / 相机最新帧 / 单次快照）并立即重跑检测。
    void rerunLiveTest();
    // 统一的检测发射入口：处理 busy 排队、回显标题、generation 标记与异步执行。
    void launchDetection(const cv::Mat &frame,
                         const FrameInputMetadata &metadata,
                         bool referenceSource);
    void updateBottomButtons();                  // 按编辑/连续/暂停测试态刷新底部动作按钮。
    void fitPreview();                           // 将预览图适配到当前画布尺寸。
    void showPreviewImage();                     // 显示基准图或默认预览图，并同步 ROI overlay。
    void showFrameForRoiEditing();               // ROI 编辑前确保画布显示可操作的图像帧。
    void startGlobalDetection();                 // 切换为整图检测模式并清理局部 ROI 编辑态。
    void startRectangleRoiEditing();             // 启动矩形检测 ROI 绘制。
    void startCircleRoiEditing();                // 启动圆形检测 ROI 绘制。
    void showUnsupportedRegionMessage();         // 提示当前暂不支持的检测区域模式。
    void setEditState(EditState state);           // 统一切换活动绘制态，不修改已保存 ROI 数据。
    void toggleEditState(EditState state);        // 再次点击当前绘制工具时退出绘制态。
    void refreshRegionButtons();                  // 仅按活动绘制态和全图配置刷新按钮。
    void updateViewerZoomLabel(qreal scale);      // 同步右侧视图倍率文本。
    void handleRoiChanged(const QRectF &roi);    // 接收矩形 ROI 编辑结果并触发测试态重跑。
    void handleRoiSelectionRejected();           // 处理矩形 ROI 绘制取消。
    void handleCircleRoiChanged(const CircleRoi &roi); // 接收圆形 ROI 编辑结果并触发测试态重跑。
    void handleCircleRoiSelectionRejected();     // 处理圆形 ROI 绘制取消。
    void startMaskEditing();                     // 进入屏蔽区编辑态。
    void startMaskPolygonDrawing();              // 启动屏蔽多边形绘制。
    void finishMaskEditing();                    // 完成屏蔽区编辑并恢复普通预览状态。
    void handleMaskPolygonChanged(const QVector<QPointF> &points); // 接收屏蔽多边形并触发重跑。
    void handleMaskPolygonSelectionRejected(int pointCount); // 处理屏蔽区点数不足或取消。
    void syncMaskControls();                     // 按屏蔽区状态刷新相关按钮可用性。
    void refreshPositionCorrectionControls();    // 刷新位置修正相关控件状态。
    void refreshDisplayedRoiOverlay();           // 根据当前 ROI/屏蔽区重绘预览 overlay。
    void displayResult(const ToolResult &result, bool referenceSource); // 展示 Adapter 返回的 OK/NG、类别、分数和 overlay。
    void displayError(const QString &status, const QString &message); // 将本地校验/运行错误统一显示为 ToolResult。
    void setViewerStatusText(const QString &displayText, const QString &tooltipText = QString()); // 更新预览状态栏文本和 tooltip。
    QString roiStatusText() const;               // 生成当前检测区域的人类可读状态文本。
    QRectF effectiveRoiNormalized() const;       // 返回当前检测有效 ROI，整图模式回退到全图。

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
    EditState m_editState = EditState::None;
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
    LiveTestSource m_liveTestSource = LiveTestSource::None;
    cv::Mat m_liveTestFrameSnapshot;       // 单次态锁定的相机帧快照
    FrameInputMetadata m_liveTestFrameMetadata; // 与单次快照成对锁定的原始输入格式元数据
    bool m_liveTestRunning = false;
    QFutureWatcher<ToolResult> *m_testRunWatcher = nullptr;
    bool m_testRunBusy = false;
    bool m_pendingRerun = false;           // 检测在途又收到新请求时置位，结束后补跑一次
    int m_testRunGeneration = 0;
};

#endif // COLORRECOGNITIONDIALOG_H
