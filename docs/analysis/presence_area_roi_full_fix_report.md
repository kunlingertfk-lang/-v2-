# Presence 面型 ROI 闭环修复报告

## 修改文件清单

- `src/frame/FrameViewHelper.h/.cpp`
- `src/toolcore/ToolOverlay.h`
- `src/PatternPresenceDialog.h/.cpp`
- `src/tooladapters/PatternPresenceAdapter.cpp`
- `src/algorithms/presence/PatternPresenceHalconRunner.h/.cpp`
- `src/BlobPresenceDialog.h/.cpp`
- `src/tooladapters/BlobPresenceAdapter.cpp`
- `src/algorithms/presence/BlobPresenceHalconRunner.h/.cpp`
- `src/CirclePresenceDialog.h/.cpp`
- `src/tooladapters/CirclePresenceAdapter.cpp`
- `src/algorithms/presence/CirclePresenceHalconRunner.h/.cpp`
- `src/ContourPresenceDialog.h/.cpp`
- `src/tooladapters/ContourPresenceAdapter.cpp`
- `src/algorithms/presence/ContourPresenceHalconRunner.h/.cpp`
- `ui/PatternPresenceDialog.ui`
- `ui/BlobPresenceDialog.ui`
- `ui/CirclePresenceDialog.ui`
- `ui/ContourPresenceDialog.ui`
- `styles/app.qss`

## FrameViewHelper

根因：多边形草稿预览把 hover 点作为最后一段预览线来源，如果 hover 点来自旧状态或默认点，点击后会短暂画到图像左上角；鼠标移动后 hover 被刷新，所以现象又恢复正常。

修复结果：
- 多边形绘制状态按 `Idle / DrawingPolygon / CompletedPolygon` 收口。
- 点击多边形工具会清空临时点并进入绘制态。
- 左键只添加顶点并清空 hover 预览点；hover 只由鼠标移动更新。
- 预览线仅在 `m_polygonHoverPointValid=true` 时绘制，不会再连到默认 `(0,0)`。
- 点数不少于 3 且点击首点附近 12 px 内自动闭合。
- 右键撤销最后一点，无点时退出绘制。
- Esc 取消草稿，不修改已保存 ROI。
- 双击或 Dialog 完成按钮均可完成已有 3 点以上草稿。
- 完成后才写入正式 polygon ROI；草稿 overlay 不污染正式 overlay。

## PatternPresence

模板多边形 ROI：
- `templateShapeType=polygon`、`templatePolygonNormalized`、`templateRoiNormalized` 都写入 `ToolConfig.params`。
- `loadFromConfig` 可恢复多边形点集，并用外接矩形兼容旧字段。
- UI 回显使用正式 polygon overlay，不只显示外接矩形。
- 基准图测试会把多边形模板传到 Runner。

算法使用情况：
- Runner 优先使用 HALCON `gen_region_polygon + reduce_domain + create_shape_model`。
- 如果 HALCON 符号缺失或 reduce 失败，会 fallback 到外接矩形，并写入：
  - `templatePolygonApplied=false`
  - `polygonFallbackToBoundingRect=true`
  - `templatePolygonFallbackMessage`

运行显示轮廓点：
- `showContourPoints` 已从 Dialog 写入 ToolConfig，经 Adapter 传到 Runner。
- Runner found 且开启时调用 `get_shape_model_contours / vector_angle_to_rigid / affine_trans_contour_xld / get_contour_xld`。
- 输出 `contour_line` 和 `contour_points` overlay，不再用“框变色”冒充轮廓。
- missing 时不输出匹配框或轮廓 overlay。
- 如果 HALCON 轮廓变换 API 不可用，payload 写 `showContourPointsApplied=false` 和原因。

Pattern 检测区自由绘制、圆形检测 ROI 未实现，入口改为图标按钮并明确提示，不再写入假 `free` 检测类型。

## BlobPresence

- 检测矩形 ROI 保持可用。
- 检测多边形 ROI 可绘制、自动闭合、完成按钮闭合、保存、恢复、回显。
- 检测圆形 ROI 可拖拽绘制、保存、恢复、回显。
- Adapter 解析 `detectRegionType / detectPolygonNormalized / detectCircleNormalized`。
- Runner 对多边形使用 `gen_region_polygon + reduce_domain`，payload 写 `polygonDetectRoiApplied=true`。
- Runner 对圆形使用 `gen_circle + reduce_domain`，payload 写 `circleDetectRoiApplied=true`。
- 自由绘制 ROI 未实现，按钮保留为图标入口并明确提示。
- 屏蔽区域未实现，点击提示“检测屏蔽区域暂未实现，第一版不参与算法”。

## CirclePresence

- ROI 按钮已统一为图标式，矩形、多边形、圆形、重置均不再使用大文字按钮。
- 检测矩形 ROI 保持可用。
- 检测多边形 ROI 可绘制、自动闭合、完成按钮闭合、保存、恢复、回显。
- 检测圆形 ROI 可绘制、保存、恢复、回显。
- Adapter 解析 `detectRegionType / detectPolygonNormalized / detectCircleNormalized`。
- Runner 在 polygon/circle reduce-domain 后执行原有圆检测，payload 写：
  - `detectRegionType`
  - `polygonDetectRoiApplied`
  - `circleDetectRoiApplied`
- 未改变圆有无判断语义。

## ContourPresence

- 模板矩形 ROI 保持可用。
- 模板多边形 ROI 可绘制、自动闭合、完成按钮闭合、保存、恢复、回显。
- 检测矩形 ROI 保持可用。
- 检测多边形 ROI 可绘制、自动闭合、保存、恢复、回显。
- Adapter 解析模板/检测多边形字段并传给 Runner。
- Runner 委托 Pattern shape-model 链路执行匹配，因此模板多边形和检测多边形沿用同一套 HALCON polygon reduce-domain 能力。
- 如果模板多边形 fallback，payload 透传 `templatePolygonApplied=false`、`polygonFallbackToBoundingRect=true`。
- 检测多边形 fallback 或失败时 payload 透传 `polygonDetectRoiApplied=false` 和 delegated payload。
- `showContourPoints` 会在 found 时生成轮廓线 overlay，missing 时不残留匹配结果。
- 检测圆形、自由绘制和屏蔽区未实现，入口明确提示，不做假接入。

## 按钮和未实现项

- Pattern / Blob / Circle / Contour 的 ROI 工具按钮已统一为图标式。
- 未实现但仍显示的入口均改为提示，不再进入无效算法参数：
  - Pattern 检测自由绘制、检测圆形。
  - Blob 自由绘制、屏蔽区。
  - Circle 屏蔽区。
  - Contour 检测自由绘制、检测圆形、模板/检测屏蔽区。
- EdgePresence / LinePresence 本轮未处理；线型工具仍需要单独做 line-band ROI，避免把面型 ROI 和线型 ROI 混在一起。

## 编译结果

- `qmake`：通过。
- `make -j$(nproc)`：通过。

## 语义边界

- 本轮没有修改 OCR 算法。
- 本轮没有修改 ToolEngine 调度原则。
- 本轮没有把算法写进 Dialog 或 MainWindow。
- 本轮没有从 QLabel/QPixmap/QGraphicsView 截图给算法。
- 本轮没有修改 EdgePresence / LinePresence 逻辑。
- 本轮没有新增算法参数，只补齐 ROI 数据、配置保存恢复、overlay 和 Runner ROI 限制链路。
