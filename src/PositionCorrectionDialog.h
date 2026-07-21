#ifndef POSITIONCORRECTIONDIALOG_H
#define POSITIONCORRECTIONDIALOG_H

#include <QDialog>
#include <QJsonObject>

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"

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
    void setAvailableProducers(const QVector<ToolConfig> &tools, int consumerIndex);

private:
    /** 将配置中的 X、Y、角度绑定回显到输入框，并初始化来源菜单。 */
    void setupBindings();
    /** 按基准图和当前工具之前的有效节点重新生成二级输出字段菜单。 */
    void rebuildBindingMenus();
    /** 保存指定字段的稳定来源绑定，并同步更新对应只读输入框。 */
    void setBinding(const QString &key, const QJsonObject &binding);
    /** 读取指定字段当前保存的来源绑定对象。 */
    QJsonObject binding(const QString &key) const;
    /** 根据配置中的模板区域类型同步矩形和多边形按钮选中态。 */
    void updateTemplateButtons();
    /** 显示无基准图或位置修正后端尚未实现的明确提示。 */
    void showNotImplemented();
    /** 校验完成操作所需的 X、Y、角度绑定是否完整。 */
    bool validateForFinish();

    Ui::PositionCorrectionDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    ToolConfig m_config;
    QJsonObject m_correction;
    QVector<ToolConfig> m_producers;
    int m_consumerIndex = 0;
};

#endif // POSITIONCORRECTIONDIALOG_H
