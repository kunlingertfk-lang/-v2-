# 颜色比较模板/检测屏蔽区独立设计

## 1. 目标

修正颜色比较“模板区域与检测区域同步”模式下检测屏蔽区错误参与模板特征提取的问题，并统一模板 ROI、模板 Mask、检测 ROI、检测 Mask 的显示与编辑规则。

本设计只覆盖颜色比较，不修改颜色识别及其他工具。

## 2. 已确认的业务合同

模板与检测是两个独立的特征区域：

~~~text
模板有效区域 = 模板基准几何 - 模板屏蔽区
检测有效区域 = 检测几何 - 检测屏蔽区
~~~

两种 Mask 永不交叉生效：

- 模板 Mask 只影响模板建模。
- 检测 Mask 只影响检测特征提取。
- 修改模板 Mask 必须使模板模型 stale，并要求重新取样。
- 修改检测 Mask 不直接改变自定义模板模型，只使当前检测配置和异步结果失效。
- 显示 overlay 不得写入原图、缩略图或 HALCON Runner 输入像素。

## 3. “与检测区域同步”的准确定义

“模板区域 → 与检测区域同步”只同步基础 ROI 几何，不同步检测 Mask。

### 自定义模板模式

~~~text
模板基准几何 = templateRoiNormalized
模板有效区域 = templateRoiNormalized - templateMaskPolygon
检测有效区域 = detectGeometry - detectMaskPolygon
~~~

### 同步模板模式

~~~text
模板基准几何 = detectGeometry
模板有效区域 = detectGeometry - templateMaskPolygon
检测有效区域 = detectGeometry - detectMaskPolygon
~~~

其中 detectGeometry 可以是全图、检测矩形或检测圆形。

同步模式的目的只是让模板和检测使用相同形状及位置范围。即使基础几何相同，两套屏蔽区仍然独立。

## 4. 当前实现偏差

当前 ColorComparisonHalconRunner::createEffectiveRegion() 在构造同步模板区域时，会先从模板基础区域扣除 detectMaskPolygonNormalized，再扣除 templateMaskPolygonNormalized。

因此当前同步模式实际是：

~~~text
模板有效区域 = detectGeometry - detectMaskPolygon - templateMaskPolygon
~~~

该行为违反本设计合同。

模板提取参数哈希中还包含 syncDetectionMaskPolygon，使检测 Mask 的变化错误地成为模板模型提取合同的一部分。这一字段也必须移除或替换为新的独立合同版本。

## 5. Runner 设计

createEffectiveRegion() 保持一个统一入口，但按 owner 分开选择 Mask：

~~~text
templateRegion == true
    base geometry:
        custom -> templateRoiNormalized
        sync   -> detectGeometry
    subtract:
        templateMaskPolygonNormalized only

templateRegion == false
    base geometry:
        detectGeometry
    subtract:
        detectMaskPolygonNormalized only
~~~

Runner 不再在模板分支读取 detectMaskPolygonNormalized。

错误状态继续保持 owner 独立：

- 模板 Mask 扣除后有效像素少于最小值：template_masked_empty。
- 检测 Mask 扣除后有效像素少于最小值：detect_masked_empty。
- 模板 Mask 非法：invalid_template_mask。
- 检测 Mask 非法：invalid_detect_mask。

不允许在某一方 Mask 无效时静默改用另一方 Mask，也不允许忽略非法 Mask 继续计算。

## 6. 模型生命周期与兼容

这是模板提取合同变化，必须让按旧合同生成的同步模式模型失效。

templateExtractParams() 应满足：

- 保留 templateRegionMode 和 templateGeometry。
- 始终记录 templateMaskPolygon。
- 不再记录 syncDetectionMaskPolygon。
- 增加稳定字段 maskOwnershipContract=independent_template_and_detection_v1。

提取参数哈希因此变化，旧同步模型在重新打开或运行时应进入 model_stale/model_rebuild_required 流程，并提示重新取样。

自定义模板模式虽然原本没有同步检测 Mask，也应使用同一新合同哈希，避免同一模型版本出现两套不可解释的提取参数语义。

不需要改变 ToolConfig 顶层 version；当前仍为颜色比较 V2。通过提取参数哈希管理该行为变更。

## 7. 配置字段

现有字段继续保留：

~~~text
templateRegionMode
templateRoiNormalized
templateMaskPolygon
detectRegionType
detectGlobal
detectRoiNormalized
detectCircleNormalized
detectMaskPolygon
~~~

不合并两套 Mask，不新增共享 Mask 字段。

字段所属关系固定为：

| 字段 | 所属对象 | 影响 |
| --- | --- | --- |
| templateRoiNormalized | 模板 | custom 模式模板基础几何 |
| templateMaskPolygon | 模板 | 仅模板模型 |
| detectRoiNormalized / detectCircleNormalized / detectGlobal | 检测几何 | 检测基础几何；sync 模式也作为模板基础几何 |
| detectMaskPolygon | 检测 | 仅检测特征 |

## 8. UI 显示设计

### 8.1 配置空闲状态

基准图上同时显示所有已经配置的几何，便于完成后复核：

- 模板 ROI：橙色实线，标记 T。
- 模板 Mask：橙红色斜纹或半透明填充，标记 T-Mask。
- 检测 ROI：青色实线，标记 D。
- 检测 Mask：蓝色斜纹或半透明填充，标记 D-Mask。

Mask 的有效填充只显示其与所属 ROI 的交集。超出所属 ROI 的部分不参与算法，可使用红色虚线轮廓提示“区域外部分无效”，但不能把它裁剪后回写配置。

当模板模式为 sync 时，模板和检测基础几何可能重合。此时继续通过颜色、线型和 T/D 标签区分，不把两者合并为一个业务对象。

### 8.2 编辑状态

编辑模板 ROI 或模板 Mask 时：

- 模板组保持完整显示并提高对比度。
- 当前对象加粗并显示控制点。
- 检测组保持可见，但降低至约 30% 透明度。
- 编辑模板 Mask 时必须同时显示模板基础 ROI。

编辑检测 ROI 或检测 Mask 时：

- 检测组保持完整显示并提高对比度。
- 当前对象加粗并显示控制点。
- 模板组降低至约 30% 透明度。
- 编辑检测 Mask 时必须同时显示检测矩形、圆形或全图边界。

任一时刻只允许一个活动绘制状态，但非活动几何不再被全部清除。

### 8.3 测试状态

在当前检测帧上只显示：

- 检测 ROI；
- 检测 Mask；
- OK/NG、得分和结果文字。

模板 ROI 和模板 Mask 只在基准图/模板预览语境显示，不叠加到当前检测帧，以免用户误认为模板几何参与本帧扣除。

### 8.4 overlay 隔离

所有 ROI、Mask、顶点、标签和结果文字必须作为独立 overlay 图元绘制。

- 模板缩略图继续从原始参考帧裁剪。
- HALCON Runner 继续接收原始帧和归一化几何。
- 不允许把 overlay 合成到 QImage/cv::Mat 后再裁剪或提取特征。

## 9. 编辑交互

模板 ROI、模板 Mask、检测 ROI、检测 Mask 统一遵守公共 toggle 合同：

1. 点击绘制图标进入并高亮。
2. 保持活动状态时允许连续重绘。
3. 再次点击当前图标退出绘制，保留最后一个有效几何。
4. 点击另一图标直接切换，任一时刻最多一个活动绘制状态。
5. “完成”可作为显式退出入口，但不得承担唯一的状态切换能力。
6. checked 只表达当前活动绘制状态，不表达几何是否已经保存。

Mask 必须增加明确的“清除”操作：

- 清除模板 Mask：templateMaskPolygon 置空，模型 stale，要求重新取样。
- 清除检测 Mask：detectMaskPolygon 置空，使旧检测结果失效；sync 模式下不得使模板模型 stale。
- 清除 Mask 不得清除所属 ROI。

已有 Mask 应支持整体移动和顶点微调。进入编辑时先显示已有多边形；用户明确开始新绘制后才以新多边形替换旧值，未完成或无效草稿不得清空旧 Mask。

## 10. 同步模式 UI

当 templateRegionMode=sync 时：

- 禁用或隐藏模板矩形绘制入口。
- 显示说明“模板 ROI 跟随检测区域，模板屏蔽区保持独立”。
- 模板 Mask 仍可单独编辑。
- 修改检测基础几何会使同步模板模型 stale。
- 修改检测 Mask 只更新检测配置，不使模板模型 stale。

切换回 custom 后恢复模板矩形绘制入口，并使用保存的 templateRoiNormalized。

## 11. 自动化验证

### Runner

至少覆盖：

1. custom 模式：模板只扣除模板 Mask。
2. custom 模式：检测只扣除检测 Mask。
3. sync 模式：模板使用检测基础几何。
4. sync 模式：检测 Mask 不改变模板有效像素、模板直方图或模板哈希输入。
5. sync 模式：模板 Mask 不改变检测有效像素和检测直方图。
6. 两套不同 Mask 同时存在时，各自只影响 owner。
7. 模板完全屏蔽返回 template_masked_empty。
8. 检测完全屏蔽返回 detect_masked_empty。
9. 非法模板/检测 Mask 分别返回对应错误。
10. 新提取合同使旧同步模型 stale。

### Dialog

至少覆盖：

1. sync 模式模板矩形入口不可编辑，模板 Mask 可编辑。
2. sync 模式修改检测 Mask 不使模板模型 stale。
3. sync 模式修改检测 ROI 使模板模型 stale。
4. 编辑模板 Mask 时模板 ROI 仍显示。
5. 编辑检测 Mask 时检测 ROI/圆仍显示。
6. 空闲状态四类已配置几何同时显示。
7. 当前编辑组高亮，另一组降权显示。
8. 清除模板 Mask 保留模板 ROI 并使模型 stale。
9. 清除检测 Mask 保留检测 ROI，且不使模板模型 stale。
10. 测试帧不显示模板 ROI/Mask。
11. overlay 不进入模板缩略图像素。

### 回归

继续运行：

- color_comparison_smoke
- color_comparison_feature_diagnostics_smoke
- color_comparison_dialog_integration_smoke
- frame_view_helper_navigation_smoke
- 主工程 shadow qmake/make

涉及 HALCON 数值提取的独立性断言必须在有效 license 下使用 RUN_HALCON_LICENSED_SMOKE=1 执行。

## 12. 手动验证

在 Ubuntu 可交互图形环境检查：

- 四类几何同时存在时仍能区分 owner。
- 重合 ROI 在 sync 模式下可通过颜色、线型和标签识别。
- 编辑 Mask 时所属 ROI 始终可见。
- 模板/检测组切换时只有当前工具高亮，其他几何不消失。
- 清除 Mask 后图层、配置、模型状态和重新检测行为正确。
- 放大、缩小和平移后，四类几何与原图坐标一致。
- 测试运行只显示检测组和结果，不混入模板组。

## 13. 非目标

- 不把模板 Mask 和检测 Mask 合并成共享字段。
- 本轮不支持多个离散 Mask；模板和检测仍各保存一个多边形。
- 不实现位置修正。
- 不修改颜色比较特征维度、评分公式、阈值语义或亮度补偿。
- 不批量改造其他工具的 overlay 层。
