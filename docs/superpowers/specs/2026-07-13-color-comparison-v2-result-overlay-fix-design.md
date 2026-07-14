# 颜色比较 V2 检测结果文字恢复设计

## 1. 状态与范围

- 状态：已确认。
- 确认日期：2026-07-13。
- 目标：恢复颜色比较测试后检测 ROI 附近的 `OK/NG score:x.x` 结果文字。
- 本次只修复结果 overlay，不修改 HALCON H/S 二维直方图提取、直方图交集评分、灵敏度、阈值和模型合同。

## 2. 问题与根因

颜色比较 V2 的 Runner 正常计算并通过 Adapter 返回 `score`，Dialog 底部状态栏也能显示分数，但画布只显示检测 ROI。

根因是 V2 的 `detectionOverlays()` 只构造检测矩形或圆形以及检测 Mask，没有追加旧版已支持的 `color_result_text`。现有 smoke 又把 overlay 数量固定为“只有检测几何”，从而把该回归固化进测试。该行为与功能文档要求的“当前图显示检测 ROI、检测 Mask 和结果 overlay”不一致。

## 3. 方案比较与决策

### 3.1 Runner 生成结果文字（采用）

Runner 在得到 `score` 和 `passed` 后生成文字 overlay，并与检测几何一起返回。这样 `ToolResult` 在 Dialog、主运行链和预览快照中保持同一结果语义，也能直接复用 `FrameViewHelper` 已有的颜色与自动摆放逻辑。

### 3.2 Dialog 临时拼接文字（不采用）

只修复配置对话框，主运行链和其他 `ToolResult` 消费者仍缺少结果文字，而且会把算法结果展示规则重复到 UI。

### 3.3 FrameViewHelper 自动推导文字（不采用）

`FrameViewHelper::setToolOverlays()` 只接收 overlay，不接收完整 `ToolResult`，无法可靠获得分数、判定状态和锚点。扩展公共接口会扩大影响范围。

## 4. 结果 overlay 合同

仅在颜色比较运行成功且测量有效时追加一个结果文字 overlay：

```text
type  = Text
label = color_result_text
text  = "OK score:94.6" 或 "NG score:47.6"
score = Runner 实际分数
p1    = anchorRect 左上角
extra.status     = "OK" 或 "NG"
extra.anchorRect = 检测区域的图像像素矩形
```

- 矩形检测：`anchorRect` 为检测矩形的像素区域。
- 圆形检测：`anchorRect` 为检测圆的像素外接矩形。
- `FrameViewHelper` 继续负责绿色/红色文字、粗体字号和框内/上方/下方/右侧自动摆放。
- 当前图仍不得叠加模板 ROI、模板 Mask 或同步模式的模板几何。
- 失败或无效测量不伪造 `0.0` 结果文字，错误继续通过状态栏和 payload 展示。

## 5. 数据流

```text
HALCON H/S 特征与交集评分
        ↓
score + passed
        ↓
detectionOverlays(检测 ROI/Mask)
        + resultTextOverlay(score, passed, anchorRect)
        ↓
ColorComparisonHalconResult.overlays
        ↓
ColorComparisonAdapter 原样传递
        ↓
ColorComparisonDialog / 主运行链
        ↓
FrameViewHelper 渲染结果文字
```

## 6. 测试与验收

先修改 `color_comparison_smoke` 形成失败测试，再实现生产代码：

- 矩形检测：检测矩形后存在一个 `color_result_text`。
- 圆形检测：检测圆后存在一个 `color_result_text`，锚点使用圆形外接矩形。
- 检测 Mask：顺序为检测几何、检测 Mask、结果文字。
- 结果文字的 `score`、`text` 和 `extra.status` 与 Runner 结果一致。
- overlay 中不出现模板 ROI 或模板 Mask。
- 运行 licensed HALCON smoke，确认同图 100 分、H/S 轴、红色色相回绕和联合分布测试不受影响。
- 运行 Model、Dialog smoke 和完整 Qt 工程构建，确认保存、回显、异步测试及公共渲染链不回归。
- 人工 GUI 检查矩形、圆形、小 ROI 的文字位置以及 OK/NG 颜色。

## 7. 非目标

- 不通过降低阈值或扩大灵敏度掩盖纯色块与实物材质分布差异。
- 不在本修复中新增多样本模型、颜色类别识别或 `class_2dim_sup` 允许色域模式。
- 不修改颜色识别和其他视觉工具的 overlay 行为。
