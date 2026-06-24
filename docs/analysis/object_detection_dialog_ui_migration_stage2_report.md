# 目标检测参数页面 UI 迁移第二阶段报告

## 1. 本轮修改文件列表

- `src/ObjectDetectionDialog.h`
- `src/ObjectDetectionDialog.cpp`
- `docs/analysis/object_detection_dialog_ui_migration_stage2_report.md`

临时 smoke 夹具位于 `/tmp/object_detection_stage2_smoke`，未写入工程。

## 2. 参考实现阅读结论

- `PatternPresenceDialog`、`BlobPresenceDialog`、`CirclePresenceDialog`、`EdgePresenceDialog`、`LinePresenceDialog`、`ContourPresenceDialog`、`CharacterRecognitionDialog` 均通过 `FrameViewHelper` 承载参数页预览。
- 参数页图像来源以 `ReferenceImageProvider::referenceImage()` 为主；ROI 编辑或测试需要当前图时，再读取 `CameraFrameProvider::currentImage()/currentFrame()`。这些 Dialog 不在参数页内 `openCamera()`、`startGrab()` 或创建独立采集线程。
- 矩形 ROI 由 `FrameViewHelper::roiChanged(QRectF)` 返回 normalized QRectF，保存到 `ToolConfig.roiNormalized`。编辑回显时调用 `setRoiRectNormalized()`。
- 取消 Dialog 时依赖外层只在 `Accepted` 后取 `ToolConfig`，不会写回配置。
- 已接算法的测试按钮会走各自 Adapter/Runner；本轮目标检测仍不接算法，因此只保留提示。

## 3. 预览区域处理方式

- 已移除 `ObjectDetectionDialog` 自己持有的 `QGraphicsScene` 预览占位。
- 构造时创建 `FrameViewHelper(ui->previewGraphicsView, this)`。
- 打开页面时优先显示 `ReferenceImageProvider` 的基准图；没有基准图时读取 `CameraFrameProvider` 当前图像作为只读预览。
- 没有任何图像时调用 `FrameViewHelper::clear()`，标题显示“当前无图像”，页面不崩溃。
- 没有调用 `openCamera()`、`startGrab()`、`closeCamera()`，没有新增相机线程，也没有订阅持续相机流。

## 4. 是否已接 FrameViewHelper

已接入。

当前接入范围：

- 图像显示：`FrameViewHelper::setImage()` / `clear()` / `fitToView()`。
- 矩形 ROI 回显：`setRoiRectNormalized()`。
- 矩形 ROI 绘制：`setRoiDrawingEnabled(true)` 后接收 `roiChanged()`。
- ROI 无效拖拽：接收 `roiSelectionRejected()` 并在状态栏提示。

## 5. ROI 绘制接入情况

- 已打通 `regionRectButton` / `allRegionRectButton`。
- `regionDrawButton` / `allRegionDrawButton` 暂不接自由区域，点击后恢复矩形模式并提示“暂未接入自由区域，当前支持矩形区域”。
- ROI 默认值为 `QRectF(0, 0, 1, 1)`，表示全图。
- ROI 主保存字段为 `ToolConfig.roiNormalized`。
- 未保存像素坐标作为主数据。
- 基础参数页和全部参数页的 ROI 按钮状态双向同步，当前最终都落到矩形模式。

## 6. roiNormalized 保存 / 回显说明

- `toolConfig()` / `toToolConfig()` 输出 `effectiveRoiNormalized()`，会将非法或空 ROI 兜底为全图 normalized ROI。
- `loadFromConfig()` 读取 `ToolConfig.roiNormalized`，非法或空值同样兜底为全图。
- 回显时调用 `FrameViewHelper::setRoiRectNormalized()`，有图像时显示矩形框；无图像时仅保留 normalized 数据和状态文本。

## 7. 基础参数页 / 全部参数页同步说明

本轮已补齐双向同步：

- `modelComboBox` <-> `allModelComboBox`
- `positionCorrectionSwitch` <-> `allPositionCorrectionSwitch`
- `positionCorrectionComboBox` <-> `allPositionCorrectionComboBox`
- `minCountSpinBox` <-> `allMinCountSpinBox`
- `maxCountSpinBox` <-> `allMaxCountSpinBox`
- `minScoreSpinBox` <-> `allMinScoreSpinBox`
- `categoryLineEdit` <-> `allCategoryLineEdit`
- `regionRectButton` <-> `allRegionRectButton`
- `regionDrawButton` <-> `allRegionDrawButton`，但自由区域暂不启用，点击后恢复矩形模式

`resultBasisComboBox` / `allResultBasisComboBox` 第一阶段已有同步，本轮保留。`toolConfig()` 仍只输出一份最终 `params` / `judgeRule`，不会保存 basic/all 两套互相矛盾的数据。

## 8. params / judgeRule 最终字段表

### ToolConfig 主字段

| 字段 | 说明 |
|---|---|
| `toolId` | 目标检测工具 ID |
| `toolName` / `displayName` | `目标检测` |
| `toolType` | `ToolType::AiDetection` |
| `category` | `ToolCategory::DeepLearning` |
| `enabled` | 工具启用状态 |
| `roiNormalized` | 主 ROI 字段，normalized QRectF |
| `summary` | 页面摘要 |

### params

| 字段 | 来源 / 说明 |
|---|---|
| `paramMode` | 当前页签：`basic` / `all` |
| `positionCorrectionEnabled` | `positionCorrectionSwitch` / `allPositionCorrectionSwitch` |
| `positionCorrectionSource` | `positionCorrectionComboBox` / `allPositionCorrectionComboBox` |
| `detectRegionType` | 当前固定输出 `rectangle` |
| `modelName` | `modelComboBox` / `allModelComboBox`，当前作为模型名称占位 |
| `maxDetections` | `maxFindCountSpinBox` |
| `confidenceThreshold` | `detectMinScoreSpinBox` |
| `detectMinScore` | `detectMinScoreSpinBox` 兼容别名 |
| `maxOverlap` | `maxOverlapSpinBox` |
| `nmsIouThreshold` | `maxOverlapSpinBox` 兼容 NMS IOU 阈值语义 |
| `sortMode` | `sortTypeComboBox` |
| `angleFilterEnabled` | `angleEnableSwitch` |
| `minAngle` / `maxAngle` | `minAngleSpinBox` / `maxAngleSpinBox` |
| `widthFilterEnabled` | `widthEnableSwitch` |
| `minWidth` / `maxWidth` | `minWidthSpinBox` / `maxWidthSpinBox` |
| `heightFilterEnabled` | `heightEnableSwitch` |
| `minHeight` / `maxHeight` | `minHeightSpinBox` / `maxHeightSpinBox` |
| `boundaryFilterEnabled` | `boundaryEnableSwitch` |
| `boundaryOverlapRatio` | `overlapRatioSpinBox` |
| `classFilterEnabled` | `classFilterSwitch` |
| `classFilterText` | `classFilterLineEdit` |
| `showBoxes` / `showLabels` / `showScores` | 预留显示开关，当前 UI 未独立暴露 |

### judgeRule

| 字段 | 来源 / 说明 |
|---|---|
| `mode` | `count` / `score` / `category` |
| `resultBasis` | `resultBasisComboBox` / `allResultBasisComboBox` |
| `minCount` | `minCountSpinBox` / `allMinCountSpinBox` |
| `maxCount` | `maxCountSpinBox` / `allMaxCountSpinBox` |
| `minScore` | `minScoreSpinBox` / `allMinScoreSpinBox` |
| `category` | `categoryLineEdit` / `allCategoryLineEdit` |

## 9. 暂未接入按钮列表

- `testRunButton`：可点击，只提示“目标检测算法尚未接入”。
- `importModelButton` / `allImportModelButton`：提示“导入模型本阶段暂未接入”。
- `setupExternalEditButton` / `objectDetectionExternalEditButton`：提示暂未接入。
- `setupSaveButton` / `setupSaveAsButton`：不单独实现保存流程，只提示暂未接入。
- `setupExportButton`：不接硬件 IO，只提示暂未接入。
- `screenRegionEditButton`：不创建屏蔽区域流程，只提示暂未接入。
- `viewerGridButton`、`viewerZoomSearchButton`、`viewerZoomOutButton`、`viewerZoomInButton`、`viewerFullButton`：只提示暂未接入。
- `regionDrawButton` / `allRegionDrawButton`：自由区域暂不接入，恢复矩形模式并在状态栏提示。

## 10. 模型路径 / 标签路径 / 输入尺寸分析结论

- 当前 V2 未发现面向 `AiDetection` 的模型管理器、模型路径字段、标签路径字段或输入尺寸字段。
- V2 中已有模型路径主要集中在 OCR：`modelPath` / `ocrModelPath`；图案有无存在 shape model 的内部 `modelPath` / cache key 概念，但不是深度学习模型管理。
- `ObjectDetectionDialog.ui` 当前只有 `modelComboBox` / `allModelComboBox`，没有路径选择、标签文件或输入尺寸控件。
- 本轮保留 `params.modelName` 作为占位，语义为“模型名称/模型列表项”，不强行解释为文件路径。
- 下一阶段如接算法，建议先确定 V2 模型管理结构，再新增稳定字段，例如 `modelPath`、`labelPath`、`inputWidth`、`inputHeight`；不建议在本轮直接大改布局。

## 11. 编译结果

执行：

```bash
cd ~/桌面/qt_znxj_v2_work_1920_515
qmake
make -j$(nproc)
```

结果：

- `qmake`：通过。
- `make -j$(nproc)`：通过，成功链接生成 `qt_ui_test`。

## 12. 运行 / smoke 验证结果

### 主程序启动

执行：

```bash
timeout 5s env QT_QPA_PLATFORM=offscreen ./qt_ui_test
```

结果：进程保持运行直到 timeout 结束，退出码 `124`，未提前崩溃。

### 目标检测阶段二 smoke

临时程序：`/tmp/object_detection_stage2_smoke/object_detection_stage2_smoke`

执行结果：

```text
object_detection_stage2_smoke: PASS
```

覆盖项：

- `ObjectDetectionDialog` 可实例化，打开不崩溃。
- 合成当前帧写入 `CameraFrameProvider` 后，打开目标检测页面和点击矩形 ROI 按钮不会清空当前帧。
- 默认 `roiNormalized == QRectF(0, 0, 1, 1)`。
- 默认 `detectRegionType == rectangle`。
- `toolConfig()` 输出 `roiNormalized`。
- `loadFromConfig()` 能回显 `roiNormalized`。
- 基础参数页和全部参数页的模型名、计数范围、最低得分、类别双向同步。
- `confidenceThreshold` 和 `detectMinScore` 输出一致。
- 测试按钮只弹出“目标检测算法尚未接入”，不触发推理。
- 工具库入口验证：
  - 深度学习 -> 目标检测 -> `ToolType::AiDetection`
  - OCR -> `ToolType::Ocr`
  - 图案有无 -> `ToolType::PatternPresence`
  - 斑点有无 -> `ToolType::BlobPresence`
  - 圆有无 -> `ToolType::CirclePresence`
  - 边缘有无 -> `ToolType::EdgePresence`
  - 直线有无 -> `ToolType::LinePresence`
  - 轮廓有无 -> `ToolType::ContourPresence`

## 13. 下一阶段建议

- 接入目标检测算法前，先定义 V2 原生模型管理：模型名称、模型路径、标签路径、输入尺寸、类别过滤之间的稳定字段关系。
- 算法阶段再新增 `AiDetectionAdapter` / Runner，不在参数页内直接调用旧版/V3/ONNX/YOLO 逻辑。
- 推理可视化结果应走 `ToolResult.overlays` 和 `FrameViewHelper::setToolOverlays()`，不要在 Dialog 内另建绘制系统。
- 如需要自由 ROI，优先复用 `FrameViewHelper` 现有 polygon/free draw 能力，并继续以 normalized 数据为主保存字段。
- 测试运行如进入连续预览，应继续复用 `CameraFrameProvider` 当前帧/信号，不新增独立相机线程。
