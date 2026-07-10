# 注册分类基准图持续测试与 ROI 自动复测设计

## 背景

当前注册分类主对话框的“基准图测试”是一次性动作。用户点击后执行一次测试；检测区域 ROI 修改完成后不会自动重新测试，导致用户需要反复点击测试按钮。检测区域矩形 ROI 已由 `FrameViewHelper` 支持绘制和编辑，本需求只调整测试模式和现有 UI 状态协作。

## 目标

- 点击“基准图测试”后进入可复用的持续测试模式。
- 测试按钮在持续测试模式下保持高亮，再次点击退出模式。
- 点击检测区域矩形 ROI 图标后进入 ROI 编辑，ROI 图标保持高亮。
- ROI 绘制完成后，在持续测试模式开启时自动执行基准图测试。
- 用户可以反复编辑 ROI，每次完成都会自动复测，不需要重复点击测试按钮。
- 点击“完成”只退出 ROI 编辑，不清除 ROI、不退出持续测试模式、不清除最近测试结果。

## 非目标

- 不新增独立测试控制器或新的算法 runner。
- 不改变 HALCON 分类算法、模型格式、ToolConfig 字段和结果判断语义。
- 不自动进入持续测试模式，也不在没有基准图时伪造测试结果。
- 不改变“测试运行”按钮的相机帧测试语义。
- 不实现圆形、多边形或屏蔽区域检测 ROI；当前主 Dialog 仍只支持全屏和矩形检测 ROI。

## 状态模型

在 `RegisteredClassificationDialog` 内增加：

- `m_referenceTestMode`：是否处于基准图持续测试模式。
- `m_roiEditing`：是否处于矩形 ROI 编辑状态。

状态规则：

| 操作 | 持续测试模式 | ROI 编辑 | 行为 |
|---|---:|---:|---|
| 点击“基准图测试”，有基准图 | 开启 | 保持原状态 | 立即执行一次基准图测试 |
| 点击“基准图测试”，无基准图 | 不变 | 不变 | 显示 `no_reference_image`，不执行测试 |
| 点击矩形 ROI 图标 | 不变 | 开启 | 使用现有 ROI 作为编辑起点 |
| 完成有效矩形 ROI | 不变 | 保持编辑态 | 保存 ROI；持续测试开启时立即复测 |
| 点击“完成” | 不变 | 关闭 | 保留 ROI、overlay 和最近结果 |
| 再次点击“基准图测试” | 关闭 | 不变 | 退出持续测试模式，保留 ROI 和最近结果 |
| 切换全屏区域 | 不变 | 关闭 | 保存全屏检测区域；持续测试开启时立即复测 |

ROI 图标的 checked 状态由 `m_roiEditing` 和现有检测区域状态共同驱动。测试按钮的 checked 状态由 `m_referenceTestMode` 驱动，不能在一次测试完成后自动取消。

## 数据流

1. 用户点击“基准图测试”。
2. Dialog 校验 `ReferenceImageProvider` 是否有基准图。
3. 设置持续测试状态并立即调用现有 `runReferenceTest()` 执行链路。
4. 用户点击矩形 ROI 图标，Dialog 调用现有 `FrameViewHelper::setRoiDrawingEnabled(true)`。
5. `FrameViewHelper::roiChanged` 调用现有 `handleRoiChanged()`，更新 `m_roiNormalized` 和检测区域类型。
6. ROI 有效时，如果 `m_referenceTestMode` 为真，调用基准图测试；请求使用最新 `toToolConfig()`，因此包含最新 ROI。
7. 结果显示沿用现有 `displayResult()`，ROI overlay 和结果 overlay 同时保留。

为避免在鼠标拖动过程中频繁测试，自动复测只响应 `FrameViewHelper::roiChanged` 的完成事件；不直接监听 draft ROI 或鼠标移动事件。

## 错误处理

- 无基准图：不打开持续测试模式，显示 `注册分类: no_reference_image`。
- ROI 无效：保持原 ROI 和测试结果，显示现有无效 ROI 提示，不触发复测。
- 无模型、模型不可读或 HALCON 错误：持续测试模式保持开启，显示现有错误结果；用户仍可继续修改 ROI。
- 测试结果为 NG：不自动退出任何状态，不清除 ROI。

## UI 要求

- 测试按钮和矩形 ROI 图标使用现有 `:checked` 高亮样式。
- 测试模式开启时，状态栏或 viewer status 应能说明当前处于基准图持续测试状态。
- ROI 编辑完成后的状态文本应同时保留 ROI 信息和测试状态，不使用一次性“测试完成后复位”的文案。
- 普通点击不能触发 `accept`、`reject` 或关闭窗口。

## 测试

在 `smoke/registered_classification_dialog_smoke.cpp` 增加覆盖：

- 点击基准图测试后按钮保持 checked，并立即进入测试状态。
- 无基准图点击测试时按钮不保持 checked，并显示 `no_reference_image`。
- 点击矩形 ROI 图标后按钮保持 checked，`FrameViewHelper::isRoiDrawingEnabled()` 为真。
- 发出第一次有效 ROI 后，持续测试模式仍保持，测试调用使用最新 ROI。
- 再发出第二次有效 ROI 后，测试可再次执行，无需再次点击基准图测试。
- 点击“完成”后 ROI 编辑关闭但测试按钮和 ROI 数据保留。
- 再次点击基准图测试退出持续测试模式，同时保留 ROI 和结果 overlay。
- 现有测试运行、模型为空和主 Qt shadow build 回归继续通过。

## 状态

- `[已确认]` 采用方案 A：Dialog 内部状态驱动持续测试和 ROI 自动复测。
- `[待实现]` 基准图持续测试状态。
- `[待实现]` ROI 完成自动复测。
- `[待实现]` smoke 回归和规范更新。
