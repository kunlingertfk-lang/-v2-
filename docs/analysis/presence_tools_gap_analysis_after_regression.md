# 有无工具回归与未闭环问题核查报告

日期：2026-05-25

范围：本报告只做静态代码核查和后续改造规划。本轮没有修改 `.cpp/.h/.ui/.pro`，没有运行 `qmake/make`，没有修改 OCR、Presence 算法或 `ToolEngine`。

## 1. 总结结论

| 项 | 当前结论 | 性质 | 需要修改 |
|---|---|---|---|
| 画面发蓝 | 确认为显示/格式链路的通道顺序回归风险，不是单个 Presence 算法导致 | 回归问题 | 先修统一 Mat->QImage 通道合约 |
| Pattern 多边形 ROI | 模板多边形只有 UI 入口；检测多边形当前没有真正入口和结构 | 未实现 / UI 壳 | 需要真实 Polygon ROI 编辑、保存、恢复、Runner 使用 |
| Pattern showContourPoints | Dialog/Adapter/Runner payload 有参数，但 Pattern Runner 没有使用它生成 contour/point overlay | 未做通 | 需要补 XLD/轮廓点 overlay |
| Circle ROI 风格 | 仍是“矩形”“全图”文字按钮，不是 Pattern 风格 | 风格不统一 | 改为图标式面型 ROI |
| Circle 圆 ROI | 当前只是“矩形检测区内找圆”，圆形检测 ROI 不可画、不可保存、不可参与算法 | 未实现 | 后续接入 Circle ROI 或隐藏未实现入口 |
| Blob ROI | UI 有自由/矩形/圆形/多边形/重置，但只有矩形和重置能用 | UI 有功能无 | 非矩形 ROI 不允许继续假接入 |
| Edge ROI | 仍是“矩形 ROI”文字按钮，ToolConfig/Runner 都是矩形 | 实现错误 | 必须改为 line-band / caliper ROI |
| Line ROI | 与 Edge 一样仍是普通矩形 ROI | 实现错误 | 必须改为 line-band / caliper ROI |
| Contour ROI | 矩形模板/检测 ROI 能用，模板多边形、检测自由/圆形只是 UI 壳 | 部分闭环 | 非矩形 ROI 需要真实结构和 Runner 支持 |
| Contour showContourPoints | 有条件做通：found 且开关开启时从模板 patch 提取 XLD 并转成 `contour_line` overlay | 部分实现 | 仍不是完整轮廓模型，需继续闭环 |

## 2. 画面发蓝根因

### 2.1 当前显示链路

相机实时图：

```text
CameraFrameProvider::workerLoop()
  -> decodeCapturedFrame()
  -> setCurrentFrame()
  -> normalizeFrame()
  -> matToImage()
  -> frameUpdated(QImage)
  -> FrameViewHelper::setImage(QImage)
```

主界面运行快照：

```text
CameraFrameProvider::currentFrame()
  -> MainWindow::imageFromFrame()
  -> m_lastRunImage
  -> FrameViewHelper::setImage(QImage)
```

工具页单次测试快照：

```text
CameraFrameProvider::currentFrame()
  -> <Dialog>::imageFromFrame()
  -> FrameViewHelper::setImage(QImage)
```

基准图：

```text
ReferenceImageProvider::referenceFrame()
  -> ReferenceImageProvider::matToImage()
  -> FrameViewHelper::setImage(QImage)
```

`FrameViewHelper::setImage()` 只执行 `QPixmap::fromImage(m_lastImage)`，不做 BGR/RGB 转换。因此颜色问题不在 overlay，也不在 `FrameViewHelper` 的绘制层。

### 2.2 当前转换合约

当前所有 Mat->QImage 入口都假设 `CV_8UC3` 是 BGR：

- `CameraFrameProvider::matToImage()` 使用 `cv::COLOR_BGR2RGB`。
- `ReferenceImageProvider::matToImage()` 使用 `cv::COLOR_BGR2RGB`。
- `MainWindow::imageFromFrame()` 使用 `cv::COLOR_BGR2RGB`。
- Pattern/Blob/Circle/Contour/Edge/Line/OCR dialog 的本地 `imageFromFrame()` 也使用 `cv::COLOR_BGR2RGB`。

如果传入的 `cv::Mat` 真的是 BGR，这条链路显示正确；如果当前相机 backend 实际给的是 RGB，或 NV12/NV21 解码后 U/V 顺序与代码假设不一致，再执行 BGR->RGB 就会红蓝互换，表现为画面发蓝。

### 2.3 精确根因判断

结论：发蓝是显示/格式链路回归，不是 Presence 算法问题，也不是 `FrameViewHelper` overlay 问题。

具体问题函数/路径：

- 第一嫌疑路径：`CameraFrameProvider::normalizeFrame()` 对 `CV_8UC3` 直接 `return frame.clone()`，没有记录或校验实际通道顺序。
- 可见发蓝发生点：`CameraFrameProvider::matToImage()` 以及各处重复的 `imageFromFrame()` 对 `CV_8UC3` 无条件执行 `COLOR_BGR2RGB`。
- 1920 路径风险点：`CameraFrameProvider::decodeCapturedFrame()` 在 `m_useNv12Path=true` 时强制按 `COLOR_YUV2BGR_NV12` 解码；如果实际 buffer UV 顺序不是 NV12，或 OpenCV/GStreamer 已经交付 BGR/RGB 三通道，再按 NV12/BGR 合约处理会造成色偏。

因此修复应先统一图像格式合约：工程内部 `cv::Mat` 明确为 BGR；所有从相机、PC 导入、基准图加载进入 provider 的路径必须转换到 BGR；所有显示只走一个公共 Mat->QImage 函数，禁止每个 dialog 复制 `imageFromFrame()`。

### 2.4 是否只有某些 dialog 发蓝

从代码看，不存在某个 Presence dialog 独有的颜色处理。主界面、相机参数页、基准图页、工具页最终都走 `FrameViewHelper::setImage()`，差异只在 QImage 来源：

- 实时预览走 `CameraFrameProvider::currentImage()/frameUpdated`。
- 单次运行/工具测试走 `currentFrame()` 后本地 `imageFromFrame()`。
- 基准图走 `ReferenceImageProvider::referenceImage()`。

如果实时、基准图、工具测试都发蓝，是全局通道合约问题；如果只在运行后快照发蓝，优先查 `MainWindow::imageFromFrame()` 和各 dialog 的本地 `imageFromFrame()` 重复转换路径。

## 3. PatternPresence / 图案有无

### 3.1 当前已实现

- 矩形模板 ROI：可拖拽矩形、保存到 `templateRoiNormalized`、写入 `params["templateRoiNormalized"]`、loadFromConfig 恢复。
- 矩形检测 ROI：可拖拽矩形、保存到 `ToolConfig.roiNormalized`、回显、Runner 使用矩形裁剪。
- UI 风格：模板 ROI 和检测 ROI 使用图标/符号式按钮，整体是面型 ROI 样板。

### 3.2 多边形 ROI 当前状态

模板多边形 ROI：

- UI 有 `basicTemplatePolygonButton` / `templatePolygonButton`。
- 点击后立即把矩形按钮重新置为 checked，并提示“多边形模板 ROI 暂未实现，请使用矩形模板区域”。
- 没有逐点落点、首尾闭合、完成保存、点集回显、ToolConfig 点集持久化。
- Runner 明确拒绝非矩形模板 ROI，payload 写 `polygon template ROI is not implemented`。

检测多边形 ROI：

- 当前 PatternPresence 检测区 UI 有自由绘制、矩形、圆形；未看到检测多边形按钮。
- 自由绘制按钮实际仍调用矩形拖拽流程，`FrameViewHelper` 只发出 `QRectF`。
- 圆形检测 ROI 点击后回退矩形，并提示“圆形 ROI 暂未实现”。
- Runner 只接收 `rectangle/rect/free/矩形`，其中 `free` 也是矩形 fallback；payload 仍写 `roiMode="rectangle"`。

结论：PatternPresence 的多边形 ROI 未实现。当前只有 UI 暗示/按钮，不具备 ROI 几何闭环。

### 3.3 showContourPoints 核查

当前链路：

- Dialog 从 `basicShowContourSwitch/showContourSwitch` 读取 `showContourPoints`。
- `toToolConfig()` 写入 `params["showContourPoints"]`。
- `loadFromConfig()` 能恢复开关状态。
- `PatternPresenceAdapter` 读取并传给 `PatternPresenceHalconConfig`。
- `PatternPresenceHalconRunner::fillPayload()` 写入 payload。

但 Pattern Runner 中没有基于 `showContourPoints` 追加 `contour_points`、`contour_line`、XLD 点集或 polygon overlay。当前 Pattern Runner 只输出检测 ROI、匹配矩形、中心十字、score 文本等 overlay。

结论：PatternPresence 的“运行显示轮廓点”未做通；目前只是参数保存和 payload 记录。

## 4. CirclePresence / 圆有无

### 4.1 当前已实现

- 矩形检测 ROI 可拖拽、保存、恢复、回显。
- Runner 在矩形 ROI 内检测圆，输出圆结果 overlay：`circle` 和 `circle_center`。

### 4.2 当前问题

- UI 仍是文字按钮：“矩形”“全图”，不是 PatternPresence 的图标式 ROI 风格。
- `configuration()` 固定 `detectRegionType="rect"`。
- `FrameViewHelper` 只支持矩形 ROI 编辑。
- 没有圆形检测 ROI 的绘制、保存、恢复、回显和 Runner 使用。
- Runner 只支持 `rect/rectangle/矩形`；非矩形时 payload 写 `mask/free/circle/polygon ROI is not implemented`。

结论：CirclePresence 需要改成 Pattern 风格图标式 ROI。当前“圆有无”算法能检测圆，但检测区域本身仍是矩形 ROI，不是圆形 ROI。

## 5. BlobPresence / 斑点有无

### 5.1 当前 ROI 图标

BlobPresence UI 已有：

- 自由绘制图标：`basicDetectionDrawButton` / `detectionDrawButton`
- 矩形：`basicDetectionRectButton` / `detectionRectButton`
- 圆形：`basicDetectionCircleButton` / `detectionCircleButton`
- 多边形：`basicDetectionPolygonButton` / `detectionPolygonButton`
- 重置：`basicDetectionResetButton` / `detectionResetButton`
- 屏蔽区：`detectionMaskEditButton`

### 5.2 当前真正可用

- 矩形 ROI：可画、可保存、可恢复、可回显、Runner 真正使用。
- 重置全图：可用。

### 5.3 当前只是 UI 壳

- 自由绘制 ROI：点击后提示“自由绘制 ROI 暂未实现，请使用矩形检测区域”。
- 圆形 ROI：点击后提示未实现。
- 多边形 ROI：点击后提示未实现。
- 屏蔽区：点击后提示未实现，payload 也显示 mask 未应用。

Runner：

- `BlobPresenceHalconConfig` 只有 `QRectF roiNormalized`。
- `isSupportedDetectRegionType()` 只支持 `rect/rectangle/矩形`。
- payload 固定 `roiMode="rectangle"`。

结论：BlobPresence 存在“UI 有了但功能没闭环”的问题。后续不能继续增加假图标，必须做到 ROI 数据结构、保存恢复、回显和 Runner 使用同步闭环。

## 6. EdgePresence / 边缘有无

### 6.1 当前状态

- UI 仍显示“矩形 ROI”文字按钮。
- tooltip 是“点击后在右侧拖拽矩形选择边缘检测区域”。
- `configuration()` 固定 `detectRegionType="rect"`。
- `ToolConfig.roiNormalized` 是唯一 ROI 几何。
- `handleRoiChanged()` 只接收 `QRectF`。
- `FrameViewHelper` 只画普通矩形 ROI。
- `EdgePresenceAdapter` 只解析 `config.roiNormalized` 和 `detectRegionType`。
- `EdgePresenceHalconConfig` 只有 `QRectF roiNormalized`。
- Runner 只支持矩形类型，payload 写 `roiMode="rectangle"`，并追加 `Rect` ROI overlay。

### 6.2 与海康标准差距

当前 EdgePresence 是普通矩形 ROI 工具，和用户确认的海康“边缘有无”不一致。缺少：

- 线型图标入口。
- 先画一条搜索线。
- 两端点可拖动。
- 拉宽度形成搜索带。
- line-band / caliper ROI 的 ToolConfig 字段。
- 中心线、搜索带、方向箭头 overlay。
- Runner 对 line-band 几何的使用或明确 fallback。

明确结论：EdgePresence 必须改为 line-band ROI，不允许继续保留“矩形 ROI”文字按钮作为最终形态。

## 7. LinePresence / 直线有无

### 7.1 当前状态

- UI 仍显示“矩形 ROI”文字按钮。
- tooltip 是“点击后在右侧拖拽矩形选择直线检测区域”。
- `configuration()` 固定 `detectRegionType="rect"`。
- `ToolConfig.roiNormalized` 是唯一 ROI 几何。
- `LinePresenceAdapter` 只解析矩形 ROI。
- `LinePresenceHalconConfig` 只有 `QRectF roiNormalized`。
- Runner 只支持矩形，payload 写 `roiMode="rectangle"`。
- 结果 overlay 有 `Line`，但 ROI 仍是普通矩形搜索区。

### 7.2 与海康标准差距

LinePresence 和 EdgePresence 应共用 line-band ROI 交互。当前缺少：

- 线型搜索 ROI 图标。
- 搜索中心线绘制。
- 端点编辑。
- 带宽编辑。
- ToolConfig 的 `line_band` 参数。
- Runner 对 line-band 的真实几何使用。

明确结论：LinePresence 必须改为 line-band ROI。当前 Runner 是矩形搜索 fallback，不是最终正确形态。

## 8. ContourPresence / 轮廓有无

### 8.1 当前已实现

- 矩形模板 ROI：可画、可保存、可恢复、可回显。
- 矩形检测 ROI：可画、可保存、可恢复、可回显。
- Runner 当前会调用 `PatternPresenceHalconRunner` 做 image patch shape model fallback。
- 结果 overlay 能显示 `detect_roi`、`template_roi`、`match_result`、score 文本。
- `showContourPoints` 在 Contour Runner 中有实际逻辑：found 且开关开启时，从模板 patch 提取 XLD 轮廓，再变换到匹配位置，追加 `contour_line` overlay，并写 `contourPointsOverlayApplied/contourPointsShown`。

### 8.2 当前只是 UI 壳

- 模板多边形 ROI：有按钮，但点击后回退矩形并提示“模板多边形 ROI 第一版暂未实现”。
- 检测自由绘制 ROI：有按钮，但提示“第一版仅支持矩形检测 ROI”。
- 检测圆形 ROI：有按钮，但提示“第一版仅支持矩形检测 ROI”。
- 配置里 `detectRegionType` 和 `templateShapeType` 固定为 `rect`。
- Runner 只接受 rect；非 rect payload 写未实现。

### 8.3 轮廓算法闭环程度

ContourPresence 不是纯 UI 壳，但还不是完整轮廓工具：

- 当前匹配主体是 Pattern shape model fallback。
- 轮廓点/线只作为 overlay 可视化，不是完整 XLD 轮廓模型匹配。
- 非矩形模板/检测 ROI 没有真实几何闭环。

结论：ContourPresence 已有部分结果 overlay 和 showContourPoints 闭环，但 ROI 形状能力仍存在 UI 壳问题。

## 9. FrameViewHelper 当前能力限制

`FrameViewHelper` 当前只支持：

- `setRoiDrawingEnabled(bool)` 启用矩形拖拽。
- 鼠标按下/移动/释放生成 `QRectF`。
- `roiChanged(const QRectF &roiNormalized)`。
- `setRoiRectNormalized()` 只显示矩形 ROI。

它能显示算法 overlay 的 `Rect/Line/Circle/Polygon/Text`，但这不等于能编辑这些 ROI。当前缺少：

- Polygon ROI 编辑和点集保存。
- Circle ROI 编辑。
- FreeDraw ROI 轨迹编辑。
- LineBand ROI 中心线、端点、带宽控制。
- 多 ROI shape 的统一数据模型。

## 10. 正确目标形态

### 10.1 面型 ROI 工具

适用：

- PatternPresence
- BlobPresence
- CirclePresence
- ContourPresence

统一要求：

- ROI 入口用图标按钮，不使用“矩形 ROI”大文字按钮。
- 风格统一到 PatternPresence。
- 只展示已实现或明确禁用的入口，不能有“图标可点但没有功能”的假接入。
- ROI 至少闭环：可绘制、可完成、可保存到 ToolConfig、可 loadFromConfig 恢复、可右侧回显、Runner 真正使用。

### 10.2 线型搜索 ROI 工具

适用：

- EdgePresence
- LinePresence

统一要求：

- 不是普通矩形 ROI。
- UI 用线型图标。
- 用户先画一条线，线有两个端点，再拉宽度形成搜索带矩形。
- ToolConfig 支持：
  - `detectRegionType = "line_band"`
  - `searchLineP1`
  - `searchLineP2`
  - `searchBandWidth`
  - 可选 `searchDirection`
- overlay 显示中心线、搜索带、结果边缘/结果直线。
- 不允许继续保留“矩形 ROI”文字按钮作为最终形态。

## 11. 分阶段修复建议

### P0：先修颜色发蓝

目标：先恢复所有画面颜色正确，避免后续 ROI/overlay 调试被颜色问题干扰。

建议：

- 建一个唯一公共转换函数，替代 MainWindow 和各 dialog 的重复 `imageFromFrame()`。
- 明确工程内部 `cv::Mat` 格式为 BGR。
- 在 `CameraFrameProvider::decodeCapturedFrame()` / `normalizeFrame()` 首帧日志中打印 backend、type、channels、size、是否 NV12、是否发生颜色转换。
- 如果确认当前 backend 返回 RGB，进入 provider 时转换为 BGR；如果确认 NV12 路径 UV 顺序相反，改对应 YUV conversion。

### P1：FrameViewHelper ROI 数据模型

目标：不要再做半吊子 UI。先让 ROI helper 支持真实 shape 编辑。

新增能力：

- `RoiShapeType::Rect`
- `RoiShapeType::Polygon`
- `RoiShapeType::Circle`
- `RoiShapeType::FreeDraw`
- `RoiShapeType::LineBand`

新增数据：

- 面型 ROI：rect / polygon points / circle center radius / free draw points。
- 线型 ROI：p1、p2、bandWidth、boundingRect。

### P2：EdgePresence 接入 LineBand

目标：先把 EdgePresence 从错误的矩形 ROI 改到正确 UI/ToolConfig。

- UI 改线型图标。
- ToolConfig 写 `detectRegionType="line_band"`、p1、p2、bandWidth。
- `roiNormalized` 保存 line-band bounding rect 兼容。
- Runner 第一版可 fallback 到 bounding rect，但 payload 必须写 `lineBandApplied=false` 和 `lineBandFallbackToBoundingRect=true`。

### P3：LinePresence 接入 LineBand

目标：和 EdgePresence 共用 LineBand 交互。

- UI、ToolConfig、overlay 与 EdgePresence 一致。
- 结果必须显示拟合直线 overlay，不用绿色矩形冒充结果。
- Runner 第一版可 fallback，但必须显式 payload 标记。

### P4：PatternPresence 补齐 Polygon 与 showContourPoints

目标：把 PatternPresence 真正做成面型 ROI 样板。

- 模板多边形：逐点、闭合、完成、保存、恢复、回显、Runner 使用。
- 检测区：根据 UI 决定是否增加多边形按钮；已展示的 free/circle 不能继续假接入。
- `showContourPoints`：Runner 真正输出 contour/xld/points overlay。

### P5：Blob / Circle / Contour 统一面型 ROI

目标：统一风格并消灭假入口。

- Blob：先保留矩形 + 重置；非矩形入口要么禁用/隐藏，要么真实实现。
- Circle：改为 Pattern 风格图标式 ROI，补真实 Circle ROI 或先只展示矩形图标。
- Contour：补模板多边形、检测自由/圆形，或禁用未实现入口。

### P6：Edge / Line Runner 真正使用 LineBand

目标：去掉 bounding rect fallback。

- Edge：沿搜索线法向找边缘。
- Line：在搜索带内提取 XLD/边缘并拟合线段。
- 方向、极性、端点顺序要和 UI 定义一致。

## 12. 本轮绝对不能继续半吊子的项

后续修改时，以下入口不能只改 UI 不闭环：

- Pattern 模板多边形 ROI。
- Pattern 检测自由/圆形/多边形 ROI。
- Blob 自由/圆形/多边形/屏蔽 ROI。
- Circle 图标式 ROI 与圆形检测 ROI。
- Edge line-band ROI。
- Line line-band ROI。
- Contour 模板多边形、检测自由/圆形 ROI。
- Pattern showContourPoints。

原则：按钮可见并可点击，就必须至少有明确实现闭环；暂不实现的入口应禁用、隐藏或明确标记为不可用，不再让用户误以为功能已完成。

## 13. 本轮修改说明

本轮只生成本分析报告：

- 没有修改 `.cpp`。
- 没有修改 `.h`。
- 没有修改 `.ui`。
- 没有修改 `.pro`。
- 没有运行 `qmake`。
- 没有运行 `make`。
- 没有修改 OCR 算法。
- 没有修改 PatternPresence / BlobPresence / CirclePresence / EdgePresence / LinePresence / ContourPresence 算法。
- 没有修改 `ToolEngine`。

