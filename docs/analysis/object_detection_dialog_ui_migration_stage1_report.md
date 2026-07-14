# 目标检测参数页面 UI 迁移第一阶段报告

## 1. 修改文件列表

- `qt_ui_test.pro`
- `src/ToolLibraryDialog.h`
- `src/ToolLibraryDialog.cpp`
- `src/ToolsDialog.cpp`
- `src/MainWindow.cpp`

## 2. 新增文件列表

- `ui/ObjectDetectionDialog.ui`
- `src/ObjectDetectionDialog.h`
- `src/ObjectDetectionDialog.cpp`
- `docs/analysis/object_detection_dialog_ui_migration_stage1_report.md`

## 3. ToolLibraryDialog 接入说明

- 复用现有 `ToolCategory::DeepLearning`，未新增工具分类。
- 复用现有 `ToolType::AiDetection`，未新增工具类型。
- 将现有 `dlDetectButton` 加入 `QButtonGroup`，并在初始化时设为 `checkable`。
- 在 `confirmSelection()` 中增加 `ObjectDetection -> ToolType::AiDetection` 分支。
- 工具库预览增加“目标检测 / 配置深度学习目标检测参数”说明。
- 未新增第二个目标检测按钮。

## 4. ToolsDialog / MainWindow 添加编辑接入说明

- `ToolsDialog::openToolConfigDialogForAdd()` 增加 `ToolType::AiDetection -> ObjectDetectionDialog`。
- `ToolsDialog::openToolConfigDialogForEdit()` 增加 `ToolType::AiDetection -> ObjectDetectionDialog`。
- `MainWindow::openToolConfigDialogForEdit()` 增加 `ToolType::AiDetection -> ObjectDetectionDialog`。
- `ToolsDialog::toolDisplayName()` 和 `MainWindow::toolDisplayName()` 增加 `AiDetection` 显示名“目标检测”。
- 取消 Dialog 时保持现有逻辑：不新增、不修改配置。

## 5. ObjectDetectionDialog 参数保存读取说明

- 对齐 V2 现有 Dialog 风格，提供 `configuration()`、`toToolConfig()`、`toolConfig()`、`loadFromConfig()`、`referencePreviewSnapshot()`。
- `ToolConfig` 主字段：
  - `toolType = ToolType::AiDetection`
  - `category = ToolCategory::DeepLearning`
  - `toolName/displayName = "目标检测"`
  - `enabled = true`
- ROI 主字段使用 `ToolConfig.roiNormalized`。本阶段未接 ROI 绘制，默认保存 `QRectF(0, 0, 1, 1)`，代码中已标注下一阶段 TODO。
- `ToolConfig.params` 写入：
  - `positionCorrectionEnabled`
  - `positionCorrectionSource`
  - `detectRegionType`
  - `modelName`
  - `maxDetections`
  - `confidenceThreshold`
  - `maxOverlap`
  - `nmsIouThreshold`
  - `sortMode`
  - `angleFilterEnabled`
  - `minAngle`
  - `maxAngle`
  - `widthFilterEnabled`
  - `minWidth`
  - `maxWidth`
  - `heightFilterEnabled`
  - `minHeight`
  - `maxHeight`
  - `boundaryFilterEnabled`
  - `boundaryOverlapRatio`
  - `classFilterEnabled`
  - `classFilterText`
  - `showBoxes`
  - `showLabels`
  - `showScores`
- `ToolConfig.judgeRule` 写入：
  - `mode`
  - `resultBasis`
  - `minCount`
  - `maxCount`
  - `minScore`
  - `category`
- `loadFromConfig()` 会回显工具 ID、启用状态、ROI、检测区域类型、位置修正、模型名、识别设置、过滤设置、判断依据和值。

## 6. 当前暂未接入内容

- 未接 ONNX / YOLO / V3 / 旧版算法。
- 未新增 `AiDetectionAdapter`。
- 未新增 Runner。
- 未接 `ToolEngine` 内的目标检测推理。
- 未新建相机线程。
- 预览区域本阶段保留为空白占位，未接 `FrameViewHelper`。
- ROI 绘制暂未接入，当前使用全图 normalized ROI。
- 测试运行按钮只提示“目标检测算法尚未接入”。
- 导入模型、外部编辑、保存、另存为、IO 输出、屏蔽区域编辑、预览缩放类按钮只提示本阶段暂未接入。

## 7. 编译结果

- `qmake`：通过。
- `make -j8`：通过，已成功链接生成 `qt_ui_test`。

## 8. 运行验证结果

- 使用 offscreen 模式启动主程序：`timeout 5s env QT_QPA_PLATFORM=offscreen ./qt_ui_test`。
  - 结果：进程保持运行直到 timeout 结束，未崩溃退出。
- 临时 smoke 验证程序 `/tmp/object_detection_smoke`：
  - `dlDetectButton` 可选中并确认返回 `ToolType::AiDetection`。
  - OCR、图案有无、斑点有无、圆有无、边缘有无、直线有无、轮廓有无入口仍可正常返回原有 ToolType。
  - `ObjectDetectionDialog::toolConfig()` 输出 `AiDetection / DeepLearning / 目标检测`。
  - `params`、`judgeRule` 关键字段可保存。
  - `loadFromConfig()` 可回显 `maxDetections`、`confidenceThreshold`、判断依据和最小得分。
  - 测试按钮提示路径可执行且不崩溃，不触发推理。

## 9. 下一阶段建议

- 接入 `FrameViewHelper` 预览，使用 V2 `ReferenceImageProvider` / `CameraFrameProvider`，不引入独立相机线程。
- 接入目标检测 ROI 绘制，并持续以 `ToolConfig.roiNormalized` 作为主 ROI 字段。
- 在算法阶段新增并接入 `AiDetectionAdapter` / Runner。
- 梳理 V3 / 旧版算法迁移边界，再迁移模型加载、推理、后处理和可视化 overlay。
