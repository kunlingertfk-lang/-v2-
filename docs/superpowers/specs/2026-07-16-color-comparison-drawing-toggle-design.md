# 颜色比较绘制工具切换交互设计

## 目标

修复颜色比较中检测区域绘制图标长期高亮的问题，并形成可供后续其他功能接入的公共绘制交互约束。

本次只修改颜色比较功能及公共规范，不批量改造其他现有工具。

## 问题原因

当前颜色比较使用 `m_detectRegionType` 和 `m_globalDetection` 同步矩形、圆形按钮的 checked 状态。这两个字段表达的是已经保存的检测区域类型，不是当前是否处于绘制模式。

因此矩形或圆形 ROI 绘制完成后，只要保存的区域类型仍是矩形或圆形，对应图标就会持续高亮，无法准确表达当前绘制状态。

## 交互合同

所有绘制入口统一采用以下切换语义：

1. 第一次点击绘制图标，进入对应绘制模式，图标高亮。
2. 未再次点击该图标时，保持绘制模式，允许连续重绘。
3. 再次点击当前高亮图标，退出绘制模式并取消高亮。
4. 退出绘制模式时保留最后一次有效 ROI 或 Mask，不清空已保存几何数据。
5. 点击另一个绘制图标时，直接切换到新绘制模式；旧图标取消高亮，新图标高亮。
6. 任一时刻最多只有一个绘制模式处于活动状态。
7. 退出绘制模式后，图像视图恢复非绘制态交互：普通滚轮缩放、普通左键拖动平移。
8. 进入绘制模式后，继续遵守公共视图约束：`Ctrl + 滚轮` 缩放、`Ctrl + 左键拖动` 平移，普通左键用于绘制。

该合同适用于矩形、圆形、多边形、模板 ROI、检测 ROI、模板 Mask 和检测 Mask 等绘制工具。

## 状态设计

### 唯一绘制状态源

颜色比较继续使用 `ColorComparisonDialog::EditState` 作为唯一绘制状态源：

```text
None
TemplateRect
TemplateMaskPolygon
DetectRect
DetectCircle
DetectMaskPolygon
```

按钮 checked 状态必须只由 `m_editState` 推导，不得由已保存的 ROI 类型、ROI 是否有效、Mask 是否存在或配置回显结果推导。

### 数据状态与交互状态分离

以下字段只表达已保存配置，不表达当前绘制状态：

```text
m_globalDetection
m_detectRegionType
m_detectRoi
m_detectCircle
m_templateRoi
m_templateMask
m_detectMask
```

退出绘制状态只把 `m_editState` 设为 `None`，不得清空上述字段。

## 状态转换

绘制图标点击统一使用以下转换：

```text
requestedState == m_editState
    -> setEditState(None)

requestedState != m_editState
    -> setEditState(requestedState)
```

建议在 `ColorComparisonDialog` 内增加一个范围收敛的辅助方法，例如：

```cpp
void toggleEditState(EditState requestedState);
```

该方法只负责活动状态切换；具体 ROI 类型、全图状态和配置失效处理仍由现有业务槽函数负责。

## 按钮显示规则

- 模板矩形按钮仅在 `m_editState == TemplateRect` 时 checked。
- 模板 Mask 按钮仅在 `m_editState == TemplateMaskPolygon` 时 checked。
- 检测矩形按钮仅在 `m_editState == DetectRect` 时 checked。
- 检测圆形按钮仅在 `m_editState == DetectCircle` 时 checked。
- 检测 Mask 按钮仅在 `m_editState == DetectMaskPolygon` 时 checked。
- 全图按钮可以表示当前保存区域为全图，但它不是绘制工具；点击全图必须退出当前绘制状态。

`refreshDetectRegionButtons()` 不再用 `m_detectRegionType` 控制矩形和圆形按钮 checked 状态。

## 与现有流程的兼容

- 配置加载和回显后默认处于 `EditState::None`，不因已保存的矩形或圆形 ROI 自动进入绘制状态。
- ROI 绘制完成信号只更新几何数据，不自动退出绘制模式，以便用户连续重绘。
- 切换基础/全部参数页不得改变当前绘制状态；同一逻辑按钮的高亮应随状态正确回显。
- 进入相机测试态后继续遵守现有模板区域编辑限制。
- 点击全图、完成配置、关闭窗口或退出相关编辑流程时必须退出绘制模式。

## 公共规范更新

在 `docs/FID/Function_Docs.md` 的公共图像视图与 ROI 交互章节补充：

- 绘制按钮使用再次点击退出的 toggle 语义。
- checked 只表示当前活动绘制模式，不表示已保存的 ROI 类型或有效性。
- 已保存几何数据与活动绘制状态必须分离。
- 单一活动绘制状态和视图 Ctrl 交互要求。

后续其他功能新增或改造绘制工具时按该规范接入；本次不批量修改其他 Dialog。

## 自动化验证

颜色比较 Dialog smoke 至少覆盖：

1. 初始存在矩形检测 ROI 时，矩形按钮不因保存类型而高亮。
2. 第一次点击矩形按钮后进入 `DetectRect`，矩形按钮高亮。
3. ROI 更新后仍保持 `DetectRect`，允许连续重绘。
4. 再次点击矩形按钮后进入 `None`，按钮取消高亮，已保存矩形 ROI 不变。
5. 从矩形状态点击圆形按钮后切换为 `DetectCircle`，只高亮圆形按钮。
6. 再次点击圆形按钮后退出绘制，已保存圆形 ROI 不变。
7. 点击全图后退出绘制模式，并正确保存全图检测语义。
8. 模板 ROI、模板 Mask 和检测 Mask 采用相同的再次点击退出规则。
9. 任一状态下最多只有一个绘制图标 checked。

## 手动验证

在 Ubuntu 图形环境中检查：

- 图标第一次点击、连续绘制、再次点击退出时的高亮变化。
- 矩形、圆形和 Mask 之间切换时无多图标同时高亮。
- 退出绘制后普通滚轮缩放、普通左键平移恢复。
- 重新进入绘制模式后最后一次 ROI 仍正确显示且可重绘。

## 非目标

- 不改变颜色比较 HALCON 特征提取和评分逻辑。
- 不改变 ROI/Mask 配置字段和持久化格式。
- 不批量改造其他工具 Dialog。
- 不新增跨项目绘制状态 controller；待多个功能按公共规范迁移后再评估抽象需求。
