# 原图坐标与像素值显示实施文档

## 1. 目标与范围

在所有由 `FrameViewHelper` 承载的主图像画布上显示鼠标所在位置的原图整数像素坐标和像素值。缩放、平移、ROI 编辑及结果 overlay 不得改变坐标或取值语义。

本次覆盖主界面、工具配置对话框、相机参数页、输出页及注册分类训练/检测等程序化主预览。训练集缩略图、类别缩略图、样本裁剪图等静态 `QLabel` 图片不纳入。

## 2. 现状审计

- 工程中约有 23 处 `new FrameViewHelper(...)` 主预览实例，统一持有实际显示的 `QImage`，并通过 `QGraphicsView` 完成缩放、平移、ROI 和 overlay 显示。
- `FrameViewHelper::viewToImage()` 已能执行 view 到 scene/原图坐标转换，但现有接口会把图像外位置钳制到边缘，不能直接作为悬停像素探查的有效性判断。
- 主界面、Tools、ReferenceImage、CameraParams、Output 及多数 Presence/OCR/AI/Classification `.ui` 已有 `X/Y/R/G/B` 占位标签，但没有动态更新代码。
- ColorRecognition、ColorComparison、ColorTemplate、TemplateLocation、PositionCorrection 及部分注册分类程序化窗口缺少独立像素状态标签。

## 3. 坐标与像素合同

- 坐标系左上角为 `(0,0)`，X 向右、Y 向下。
- 鼠标位置按 `viewport position -> mapToScene -> image pixel` 转换；像素索引对 scene 坐标向下取整。
- 有效范围严格为 `0 <= x < width`、`0 <= y < height`。图像外黑边返回无效状态，不钳制或吸附到边缘像素。
- 像素值来自 `FrameViewHelper::setImage()` 当前持有的 `QImage`。上游已经转换或降位深的数据不能在视图层恢复。
- RGB 图显示 `R/G/B`；灰度图显示 `Gray`。`Grayscale16` 保留 0-65535 数值，其余 Qt 彩色格式按 `QColor` 的实际显示通道读取。
- 无图、鼠标离开 viewport 或位于图像外时显示占位文本。

## 4. 公共设计

### 4.1 FramePixelProbe

在 `src/frame/` 新增纯采样结构与工具：

- `FramePixelSample`：`valid`、`imagePosition`、`channelModel`、`bitDepth`、RGB/Gray 通道值。
- `FramePixelProbe::sample(QImage, QPoint)`：完成边界、格式和通道采样。
- `FramePixelProbe::displayText(sample)`：集中生成状态栏文本，Dialog 不重复拼接。

### 4.2 FrameViewHelper

- 新增 `cursorPixelChanged(const FramePixelSample &)` 信号。
- 新增 `bindPixelStatusLabel(QLabel *)` 公共绑定入口，复用统一格式。
- viewport 的 MouseMove 在现有导航和 ROI 分支之前计算悬停像素，保证交互状态不影响探查。
- Leave、clear 和无效位置发送无效样本；只在样本变化时更新，避免无意义的标签刷新。
- 不修改现有 ROI、导航和 overlay 公共接口。

## 5. 接入方式

- 已有 `viewerCursorLabel` 或主界面 `cursorLabel`：构造 `FrameViewHelper` 后直接绑定。
- 缺少标签的 Designer 页面：在现有 viewer status bar 中增加右侧 `viewerCursorLabel`，保留左侧业务状态。
- 程序化页面：在预览底部状态布局中增加专用 QLabel；不得用业务状态标签显示像素值。
- 每个主画布只绑定一个像素标签；缩略图不安装探查。

## 6. 异常和兼容行为

- 空 `QImage`、越界位置、未识别格式均返回安全状态，不崩溃。
- Mono/Indexed 图按其实际颜色表或灰度语义显示；不猜测 BGR/RGB 来源。
- 鼠标事件探查不接受事件，不改变现有 eventFilter 返回值。
- 不修改 `ToolConfig`、`ToolResult`、Adapter、HALCON runner 或配置 JSON。

## 7. 验证设计

- 新增 `frame_view_helper_pixel_probe_smoke`：覆盖 RGB888、BGR888、Grayscale8、Grayscale16、空图、边界、图像外、Leave、适应窗口和放大后的坐标一致性。
- 回归 `frame_view_helper_navigation_smoke` 与 `frame_view_helper_roi_edit_smoke`。
- 抽查 Designer 窗口和程序化窗口均绑定独立像素标签。
- 在影子目录执行主工程 qmake/make。

## 8. 实施记录

- 2026-08-03：完成只读审计并建立本文档，尚未修改功能代码。
- 2026-08-03：新增 `FramePixelSample` 和 `FramePixelProbe`，实现 RGB/Gray、8/16 位采样、统一文本和样本比较。
- 2026-08-03：扩展 `FrameViewHelper`，新增 `cursorPixelChanged`、`bindPixelStatusLabel`、严格图像边界判断以及 MouseMove/Leave/clear 状态发布。
- 2026-08-03：24 处 `FrameViewHelper` 构造代码均完成标签绑定；其中 CameraParams 的两处构造属于互斥初始化分支。已有占位标签直接复用，缺少标签的 Designer 和程序化窗口新增独立像素标签。
- 2026-08-03：主工程及所有直接包含 `FrameViewHelper.cpp` 的现有 smoke `.pro` 已补入 `FramePixelProbe` 源文件；新增专用 pixel probe smoke。
- 2026-08-03：更新 `docs/FID/Function_Docs.md` 公共图像视图像素探查合同。
- 2026-08-03：根据实际截图修复 ColorComparison 专用 QLabel 规则覆盖通用白字规则的问题；游标标签加入右侧深色区域专用选择器。

## 9. 验证结果

### 9.1 已通过

- 主工程影子构建：`qmake ../qt_ui_test.pro && make -j$(nproc)`，通过。
- `frame_view_helper_pixel_probe_smoke`：通过。覆盖 RGB888、BGR888、Grayscale8、Grayscale16、空图、越界、图像外、Leave、clear 和缩放前后坐标/像素一致性。
- `frame_view_helper_navigation_smoke`：通过。
- `frame_view_helper_roi_edit_smoke`：通过。
- `position_correction_ui_smoke`：通过。
- `template_location_ui_smoke`：通过。
- `registered_classification_dialog_smoke`：通过，包含程序化主预览和训练预览构造。
- `color_comparison_dialog_integration_smoke`：通过。
- `git diff --check`：通过，无空白错误。
- 静态覆盖检查：24 处 `new FrameViewHelper` 均有对应 `bindPixelStatusLabel`；静态缩略图未安装探查。

### 9.2 已知既有测试工程问题

- `registered_classification_detection_dialog_smoke` 编译完成但链接失败。缺失符号来自该 `.pro` 原有源文件清单未包含训练 runner、KNN/model package、`FrameInputMetadata.cpp` 等当前 Dialog 已依赖实现；主工程包含这些实现并已成功链接。
- 本次只为该 `.pro` 补入新的 `FramePixelProbe` 依赖，没有扩展范围修复其既有训练子系统链接清单。注册分类检测相关源码已由主工程构建覆盖。

### 9.3 手动验收建议

- 在有真实相机或基准图的运行环境中打开主界面及常用工具 Dialog，确认状态栏字体、宽度和实时刷新视觉效果。
- 对相机原始 12/16 位值有诊断需求时，应另行设计 raw frame/metadata 探查链；当前功能严格显示视图持有 `QImage` 的值。
