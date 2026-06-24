# Presence 有无工具海康式 ROI 分阶段整改报告

生成时间：2026-05-25

## 安全备份

- 执行了 `git status --short`，当前工作树包含上个会话遗留的大量已修改/新增文件。
- 已备份当前 tracked diff：`/home/hjl-ubuntu/桌面/znxj_v2_1920_presence_full_fix_before_20260525_125237.patch`
- 未执行 `git reset`、`git checkout` 覆盖、删除文件。

## P0 颜色发蓝修复

修改/确认文件：
- `src/frame/MatImageConverter.h`
- `src/frame/MatImageConverter.cpp`
- `src/frame/CameraFrameProvider.cpp`
- `src/frame/ReferenceImageProvider.cpp`
- `src/MainWindow.cpp`
- `src/ToolsDialog.cpp`
- `src/PatternPresenceDialog.cpp`
- `src/BlobPresenceDialog.cpp`
- `src/CirclePresenceDialog.cpp`
- `src/ContourPresenceDialog.cpp`
- `src/EdgePresenceDialog.cpp`
- `src/LinePresenceDialog.cpp`
- `src/CharacterRecognitionDialog.cpp`

根因：
- 显示链路中曾存在多个局部 `cv::Mat -> QImage` 转换入口，容易把 OpenCV 默认 BGR 数据直接按 RGB 显示，导致画面偏蓝。

修复方式：
- 统一入口为 `MatImageConverter::matToDisplayImage()`。
- `CV_8UC1` 转 `QImage::Format_Grayscale8`。
- `CV_8UC3` 统一 `BGR -> RGB888`。
- `CV_8UC4` 统一按 `BGRA -> RGBA8888`。
- `CameraFrameProvider::currentFrame()` 和 `ReferenceImageProvider::referenceFrame()` 保持 BGR `cv::Mat`，不把 RGB Mat 写回 Provider。
- UI 和 ToolPreviewSnapshot 回显走显示图转换，Runner 输入仍是 BGR/Gray Mat。

补充：
- 首次 P0 编译时发现上个会话停在 `LinePresenceDialog` line-band 信号连接后，缺少 `effectiveLineBandRoi()` / handler 实现；本阶段先补齐该编译阻断点，未进入后续阶段前已重新编译通过。

编译结果：
- `qmake` 通过。
- `make -j$(nproc)` 通过。

## P1 PatternPresence 图案有无

修改/确认文件：
- `src/PatternPresenceDialog.h`
- `src/PatternPresenceDialog.cpp`
- `src/tooladapters/PatternPresenceAdapter.cpp`
- `src/algorithms/presence/PatternPresenceHalconRunner.h`
- `src/algorithms/presence/PatternPresenceHalconRunner.cpp`
- `src/frame/FrameViewHelper.h`
- `src/frame/FrameViewHelper.cpp`

完成结果：
- 模板多边形 ROI 可通过左键点选绘制，完成时少于 3 点会提示“多边形至少需要 3 个点”。
- `ToolConfig.params` 保存：
  - `templateShapeType="polygon"`
  - `templatePolygonNormalized=[{x,y}, ...]`
  - `templateRoiNormalized=polygon bounding rect`
- `loadFromConfig()` 可恢复多边形字段和按钮状态。
- reference preview snapshot 使用 Runner overlay，可回显 `template_roi` polygon。
- `showContourPoints` 已由 Dialog 写入 params，Adapter 解析，Runner 按开关输出 `contour_line` / `contour_points`。
- found 时输出匹配结果；missing 时不会残留绿色匹配结果；关闭 `showContourPoints` 时不会输出轮廓点/线。

多边形算法参与情况：
- 当前优先真实应用 HALCON `gen_region_polygon + reduce_domain` 后再 `create_shape_model`。
- 如果运行时 HALCON 缺少符号或 polygon reduce-domain 失败，则 fallback 到外接矩形，并写入：
  - `templatePolygonApplied=false`
  - `polygonFallbackToBoundingRect=true`
  - `templatePolygonFallbackMessage`
- 成功应用时写入：
  - `templatePolygonApplied=true`
  - `polygonFallbackToBoundingRect=false`
  - `roiMode="polygon_reduce_domain"`

颜色分层：
- `template_roi`：蓝色。
- `detect_roi`：橙色。
- `match_rect` / `match_center`：绿色。
- `contour_line` / `contour_points`：青色。

编译结果：
- `qmake` 通过。
- `make -j$(nproc)` 通过。

## P2 面型 ROI 工具 UI 风格

修改/确认文件：
- `src/PatternPresenceDialog.cpp`
- `src/BlobPresenceDialog.cpp`
- `src/CirclePresenceDialog.cpp`
- `src/ContourPresenceDialog.cpp`
- `ui/CirclePresenceDialog.ui`

完成结果：
- Pattern / Blob / Circle / Contour 的 ROI 入口统一为 52x38 图标式按钮，并设置 tooltip。
- Circle `.ui` 中“矩形 / 全图”文字已改为图标 `□ / ⟳`。
- Blob 未实现的自由绘制、圆形、多边形、屏蔽区均会提示，不会静默 fallback。
- Contour 未实现的模板多边形、自由绘制检测 ROI、圆形检测 ROI、屏蔽区均会提示。

真实实现与 TODO：
- Circle：矩形检测 ROI、全图恢复、保存/恢复已实现；屏蔽区未实现；无圆形 ROI UI。
- Blob：矩形检测 ROI、全图恢复、保存/恢复已实现；自由绘制/圆形/多边形/屏蔽区未实现并提示。
- Contour：矩形模板 ROI、矩形检测 ROI、保存/恢复、showContourPoints overlay 已实现；模板多边形、自由绘制检测 ROI、圆形检测 ROI、模板/检测屏蔽区仍 TODO。

编译结果：
- `qmake` 通过。
- `make -j$(nproc)` 通过。

## P3 Edge / Line 线型搜索 ROI

修改/确认文件：
- `src/frame/FrameViewHelper.h`
- `src/frame/FrameViewHelper.cpp`
- `src/EdgePresenceDialog.h`
- `src/EdgePresenceDialog.cpp`
- `src/LinePresenceDialog.h`
- `src/LinePresenceDialog.cpp`
- `src/tooladapters/EdgePresenceAdapter.cpp`
- `src/tooladapters/LinePresenceAdapter.cpp`
- `src/algorithms/presence/EdgePresenceHalconRunner.h`
- `src/algorithms/presence/EdgePresenceHalconRunner.cpp`
- `src/algorithms/presence/LinePresenceHalconRunner.h`
- `src/algorithms/presence/LinePresenceHalconRunner.cpp`
- `ui/EdgePresenceDialog.ui`
- `ui/LinePresenceDialog.ui`

完成结果：
- `FrameViewHelper` 增加 `RoiShapeType::LineBand`、`LineBandRoi`、绘制/调宽/回显能力。
- EdgePresence 和 LinePresence UI 不再启用普通矩形 ROI 绘制，入口为线型 ROI。
- 交互为拖拽中心线，移动鼠标拉出带宽，再次左键确认。
- 显示中心线、端点和搜索带边界。
- `ToolConfig.params` 保存：
  - `detectRegionType="line_band"`
  - `searchLineP1`
  - `searchLineP2`
  - `searchBandWidth`
  - `lineBandWidthUnit="normalized_max_dimension"`
- `ToolConfig.roiNormalized` 保存 line-band 外接矩形。
- Edge/Line `.ui` 中残留“矩形 ROI / 拖拽矩形”文案已改为线型 ROI 文案。

算法 fallback：
- EdgePresence Runner 当前仍在 line-band 外接矩形内找边缘。
- LinePresence Runner 当前仍在 line-band 外接矩形内拟合直线。
- 两者 payload 均明确：
  - `lineBandApplied=false`
  - `lineBandFallbackToBoundingRect=true`
  - `roiMode="line_band_bounding_rect"`

编译结果：
- `qmake` 通过。
- `make -j$(nproc)` 通过。

## P4 回归结果

静态检查：
- Edge/Line Dialog 源码中未再使用矩形绘制路径。
- Edge/Line `.ui` 未再残留“矩形 ROI / 拖拽矩形”文案。
- `MatImageConverter` 为当前显示转换统一入口。
- Pattern/Contour 中仍存在“拖拽矩形”文案，含义为其已实现的矩形 ROI 编辑，不是 Edge/Line 残留。

最终编译：
- `qmake` 通过。
- `make -j$(nproc)` 通过，最终输出为“无需做任何事”。

手工测试建议：
- 主界面相机图颜色是否正常。
- 工具页基准图颜色是否正常。
- Pattern 矩形模板 ROI found。
- Pattern 多边形模板 ROI 绘制、完成、保存、重新打开恢复。
- Pattern 勾选/取消“运行显示轮廓点”的 overlay 差异。
- Blob 未实现 ROI 按钮是否明确提示。
- Circle 图标式矩形 ROI 和全图恢复。
- Contour 矩形模板/检测 ROI、showContourPoints found/missing 行为。
- Edge 线型搜索带绘制、保存、恢复、运行 overlay。
- Line 线型搜索带绘制、保存、恢复、运行 overlay。
- 方案保存/恢复后工具链仍完整。
- 主界面单次运行和连续运行无明显卡顿。

## 边界说明

- 未修改 OCR 算法语义。
- 未修改 Pattern / Blob / Circle / Edge / Line / Contour 的核心检测判定语义；本次改动集中在 ROI 输入、显示转换、overlay、UI 风格、参数传递和快照回显。
- 未把算法写入 Dialog 或 MainWindow。
- 未修改 ToolEngine 调度原则。
- 未从 QLabel / QPixmap / QGraphicsView 截图给算法。
- 未通过降低分辨率掩盖颜色或 ROI 问题。
