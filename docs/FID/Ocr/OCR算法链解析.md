# OCR 算法链解析

> 功能：字符识别（`ToolType::Ocr`）  
> 核对日期：2026-07-23

## 1. 功能定位与链路

OCR 使用 HALCON MLP OCR 模型。先在 ROI 中按阈值分割字符，按面积、宽高和长宽比过滤并排序，再调用 `do_ocr_multi_class_mlp` 分类；Adapter 根据计数、最低分或基准文本执行最终判定。

```text
ToolLibraryDialog(CharacterRecognition)
  -> CharacterRecognitionDialog
  -> ToolConfig
  -> OcrAdapter
  -> HalconRuntimePaths
  -> OcrHalconRunner
  -> OCR 原始结果/置信度过滤结果
  -> OcrAdapter::judgeOcrResult
  -> ToolResult / overlays / payload
```

## 2. 文件地图

| 层级 | 文件 |
| --- | --- |
| UI | `src/CharacterRecognitionDialog.{h,cpp}`、`ui/CharacterRecognitionDialog.ui` |
| Adapter | `src/tooladapters/OcrAdapter.{h,cpp}` |
| Runner | `src/algorithms/ocr/OcrHalconRunner.{h,cpp}` |
| Runtime 路径 | `src/algorithms/halcon/HalconRuntimePaths.{h,cpp}` |
| 入口/注册 | `src/ToolLibraryDialog.cpp`、`src/MainWindow.cpp` |
| 工程 | `qt_ui_test.pro` |

Dialog 支持参考图单测、相机单次/连续测试和结果 snapshot。

## 3. 配置合同

`CharacterRecognitionDialog::toToolConfig()` 写入：

| 分组 | 字段 |
| --- | --- |
| 模型 | `modelName`、`modelPath`、`ocrModelPath`、`ocr_model_path` |
| ROI | `ToolConfig.roiNormalized` |
| 二值化 | `polarity`、`binaryThreshold` |
| 字符几何 | `min/maxCharArea`、`min/maxCharWidth`、`min/maxCharHeight`、`min/maxAspectRatio` |
| 置信度 | `minScore`、`minConfidence` |
| 判定镜像 | `resultBasis`、`minCount/maxCount`、`baselineText` |
| 预留 | `independentPositionCorrection`、`positionCorrection` |

`judgeRule` 根据 UI 结果依据写为：

- count：`mode=count`、`minCount`、`maxCount`
- score：`mode=score`、`minScore`
- text：`mode=text`、`expectedText`、`matchRule=exact`

Adapter 兼容 camelCase/snake_case 旧字段。分数大于 1 时按百分数除以 100，再约束为 0..1。

## 4. Runtime 和模型路径

Adapter 在调用 Runner 前：

1. `resolveHalconLibPath()` 解析 HALCON runtime；
2. `resolveOcrModelPath()` 解析 OCR `.omc` 模型；
3. 路径不存在时直接返回错误，并在 payload 写入尝试过的候选路径。

UI 文件中存在历史绝对默认模型路径，但运行时仍应以 `HalconRuntimePaths` 的解析结果和当前机器文件为准。

## 5. Runner 算法流程

1. 初始化 payload，校验图像、HALCON runtime、OCR 模型、图像类型和 ROI。
2. 支持 `CV_8UC1` 和 BGR `CV_8UC3`；彩色图通过 `rgb1_to_gray` 转灰度。
3. 用 `gen_region_polygon_filled` 构造 ROI，`reduce_domain` 限定识别区域。
4. 字符分割：
   - dark：`threshold(0, binaryThreshold)`；
   - bright：`threshold(binaryThreshold, 255)`；
   - auto：先 dark，无候选时再 bright。
5. `connection` 得到候选字符。
6. 连续调用 `select_shape`，按 area、width、height、ratio 过滤。
7. `sort_region("character", "true", "row")` 按字符阅读顺序排序。
8. 无候选时返回 `success=true,status=no_characters`。
9. `read_ocr_class_mlp` 加载模型，`do_ocr_multi_class_mlp` 得到字符类别和置信度。
10. 用 `smallest_rectangle1` 取得每个字符框。
11. 保留两套结果：
    - raw：模型返回的全部字符；
    - filtered：置信度不低于 `minConfidence` 的字符。
12. filtered 作为 Runner 的 `text/count/score/chars` 别名，并生成字符框和 OCR 文本 overlay。

主要 HALCON 接口：`gen_image1`、`gen_image_interleaved`、`rgb1_to_gray`、`gen_region_polygon_filled`、`reduce_domain`、`threshold`、`connection`、`select_shape`、`sort_region`、`read_ocr_class_mlp`、`do_ocr_multi_class_mlp`、`smallest_rectangle1`、`clear_ocr_class_mlp`。

## 6. Adapter 最终判定

Runner 成功后，Adapter 对 filtered 结果判定：

| mode | 规则 | 失败状态 |
| --- | --- | --- |
| `count` | `minCount <= charCount <= maxCount` | `count_out_of_range` |
| `score` | 有字符且平均置信度 >= `minScore` | `score_too_low` |
| `text` | filteredText 与 expectedText 按规则比较 | `text_mismatch`；基准空为 `baseline_text_empty` |
| `none`/其他 | 默认不形成业务 OK | `ng` |

若 Runner 成功但没有通过置信度过滤的字符，Adapter 保持明确的 `no_characters` 语义。

## 7. 输出合同

- `text`：过滤后的识别文本；
- `count`：过滤后的字符数；
- `score`：过滤后平均置信度；
- overlays：ROI、过滤后字符框、识别文本；
- payload 同时包含 `rawText/rawChars/rawCharCount/rawAverageConfidence` 与 `filtered*`，并包含阈值、极性、ROI、字符几何、模型路径、判断规则和候选路径。

## 8. 错误与真实边界

错误状态：

- `image_empty`
- `halcon_so_not_found`、`halcon_load_failed`、`halcon_symbol_missing`
- `model_not_found`
- `unsupported_image_type`
- `invalid_roi`
- `ocr_failed`
- `no_characters`（算法成功但无字符）

真实边界：

- 只支持 HALCON MLP OCR `.omc` 链路，不是深度学习 OCR，也没有行检测/方向校正。
- 字符分割依赖单阈值和几何筛选，复杂背景、粘连字符、多行文本需要额外策略。
- auto 极性只在 dark 完全无候选时尝试 bright，不会比较两套识别质量。
- OCR 已消费公共位置修正 Context，详见本文末尾 2026-07-24 记录。
- `matchRule` 当前 UI 固定 exact；Runner 保存 expectedText/matchRule，但最终业务判断在 Adapter。

## 9. 验证建议

当前未发现 OCR 独立 smoke。建议覆盖：

1. 模型路径自动解析、自定义路径、缺模型和缺 runtime。
2. dark/bright/auto、阈值边界、几何筛选边界。
3. 原始字符与置信度过滤后字符差异。
4. count/score/text 三类判定及空基准文本。
5. 单行字符顺序、空 ROI、空图、灰度/BGR 和不支持的 BGRA 输入。
6. 连续测试期间资源清理和错误恢复。
## 2026-07-24 位置修正接入

- OCR 新旧配置兼容：继续读取 `independentPositionCorrection/positionCorrection`，同时统一写入 `enablePositionCorrection/positionCorrectionSource/positionCorrectionSourceId`。
- Dialog 测试注册模板定位、位置修正和 OCR Adapter，并通过方案前缀取得本帧修正 Context。
- Adapter 使用公共 Consumer；来源缺失、前置匹配失败、矩阵或尺度非法时 OCR 直接返回明确错误。
- Runner 在整图创建 OCR 基准 Region，经 HALCON `affine_trans_region`、`clip_region` 后 `reduce_domain`，字符分割和识别只在修正 Region 内运行。
- 运行态显示修正后的 `detect_roi`、匹配轮廓和匹配原点，不再保留重叠的基准 ROI。
- Dialog 已提供 PC 图片离线测试，矩形 ROI 修改后立即重测，静态导入图不会进入连续定时器。
- 验证：主工程影子 qmake/make、`presence_ocr_position_correction_contract_smoke` 通过。
