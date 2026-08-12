#ifndef POSITIONCORRECTIONDIALOG_H
#define POSITIONCORRECTIONDIALOG_H

#include <QDialog>
#include <QJsonObject>

#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolOverlay.h"
#include "toolcore/ToolPreviewSnapshot.h"
#include "tooladapters/PositionCorrectionAdapter.h"
#include "tooladapters/TemplateLocationAdapter.h"

class FrameViewHelper;

QT_BEGIN_NAMESPACE
namespace Ui { class PositionCorrectionDialog; }
QT_END_NAMESPACE

class PositionCorrectionDialog : public QDialog
{
    Q_OBJECT

public:
    /** 初始化位置修正配置界面、默认绑定、预览图以及各控件交互。 */
    explicit PositionCorrectionDialog(QWidget *parent = nullptr);
    /** 释放由 Qt Designer 创建的界面对象。 */
    ~PositionCorrectionDialog() override;

    /** 从已有工具配置恢复稳定工具 ID、字段绑定和模板区域类型。 */
    void loadFromConfig(const ToolConfig &config);
    /** 汇总当前界面状态，生成可保存到方案中的位置修正工具配置。 */
    ToolConfig toolConfig() const;
    /** 返回工具列表预览快照；UI 阶段暂不生成独立快照。 */
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    /** 设置当前工具可引用的前置节点，并据此重建字段绑定菜单。 */
    void setAvailableProducers(
            const QVector<ToolConfig> &tools,
            int consumerIndex,
            const QVector<PositionReferencePoseProducer> &referenceProducers = {});

private:
    /** 将配置中的运行姿态来源回显到 X/Y/角度输入框，并初始化来源菜单。 */
    void setupBindings();
    /** 按当前工具之前真实声明位姿输出的节点重新生成来源菜单。 */
    void rebuildBindingMenus();
    /** 保存统一运行姿态来源，并同步更新 runPoseSource 与旧三字段回显。 */
    void setRunPoseSource(const PositionRunPoseSource &source);
    /** 将当前 runPoseSource 显示到三个只读输入框。 */
    void updateRunPoseDisplay();
    /** 查找当前绑定的普通工具或基准图位姿来源。 */
    bool findToolProducer(const QString &producerId, ToolConfig *config) const;
    bool findReferenceProducer(const QString &producerId,
                               PositionReferencePoseProducer *producer) const;
    /** 在图像右上角显示订阅来源、基准位姿和运行位姿。 */
    QVector<ToolOverlay> poseInfoOverlays(
            const QSize &imageSize,
            const PositionRunPoseSource &source,
            const QJsonObject &referencePose,
            const QJsonObject &runPose = QJsonObject()) const;
    void showFrameWithPoseInfo(const cv::Mat &frame,
                               const QString &title,
                               const PositionRunPoseSource &source,
                               const QJsonObject &referencePose,
                               const QJsonObject &runPose,
                               const QVector<ToolOverlay> &sourceOverlays = {});
    /** 在基准图上执行 runPoseSource 指向的同一上游模板定位实例，并冻结 referencePose。 */
    void createReferencePose();
    /** 使用当前运行帧执行同一上游定位与位置修正后端。 */
    void runPositionCorrectionTest();
    /** 校验完成操作所需的运行姿态来源是否完整且来自合法前置生产者。 */
    bool validateForFinish();

    Ui::PositionCorrectionDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    ToolConfig m_config;
    QJsonObject m_correction;
    QVector<ToolConfig> m_producers;
    QVector<PositionPoseProducer> m_poseProducers;
    QVector<PositionReferencePoseProducer> m_referencePoseProducers;
    int m_consumerIndex = 0;
    TemplateLocationAdapter m_templateLocationAdapter;
    PositionCorrectionAdapter m_positionCorrectionAdapter;
};

#endif // POSITIONCORRECTIONDIALOG_H
