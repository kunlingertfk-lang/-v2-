# PatternPresence 轮廓自动显示调优报告

## 修改文件清单

- `src/algorithms/presence/PatternPresenceHalconRunner.cpp`
- `docs/analysis/pattern_contour_auto_display_tuning_report.md`

## 结论

本轮没有新增 UI 参数，没有新增 contour level 控件。`get_shape_model_contours` 的 level 只作为 PatternPresence 内部自动显示策略使用，用户界面仍然只使用现有的“灵敏度”和“运行显示轮廓点”。

本轮没有修改 PatternPresence 的 OK/NG 判断语义：`found`、`score`、`judgeBasis`、`existOk`、`scoreThreshold` 的判定路径保持不变。改动范围只在 `showContourPoints=true` 后的模型轮廓显示、debug 输出和 `templateSensitivity -> create_shape_model Contrast/MinContrast` 内部映射。

## 主轮廓被过滤掉的原因

上一版显示过滤以单条 XLD contour 的长度和点数硬筛为主，`minLength=max(15px,minDim*0.03)`、`pointCount>=5`，再做累计长度裁剪。HALCON shape model 会把同一主轮廓拆成多段短 XLD，单段看起来像短碎线，但聚合后其实是主外轮廓的一部分，因此被误杀后界面只剩几段小线。

此外上一版固定 `level=1`，没有根据模型实际 level 做选择，也没有“过滤后过稀”的自动回退。某些模型只有 1 个 pyramid level，盲试 `level=2/3` 会触发 HALCON 参数错误；现在先读取模型 `NumLevels`，不存在的 level 只在 debug 中记录为 unavailable，并保存空 raw 对比图，不再影响显示。

## 当前内部策略

- 默认首选 `selectedContourLevel=2`。
- 如果模型没有 level 2，或 level 2 过滤后不可用，自动 fallback 到 level 1。
- level 3 只在存在且可用时作为最后 fallback，不会因为 level 3 太稀而强选。
- 过滤阈值放宽为 `minLength=max(5px,min(templateW,templateH)*0.01)`。
- `pointCount<3` 过滤；`bbox width<2 && height<2` 过滤。
- 先保留靠近 raw model contour 外包络边界的短段，再按累计长度保留主要 contour。
- 如果 filtered 总长度低于 raw 的 15%，或 raw 很多但 filtered 少于 3 条，自动放宽到 relaxed 规则，防止主轮廓被过滤到只剩几段。

## 灵敏度映射

当前内部映射：

```text
s = clamp(templateSensitivity, 1, 10)
contrast = 70 - s * 3
minContrast = max(18, contrast / 2)
```

高灵敏度不会再把 `minContrast` 降到 5 或 10 以下，避免把弱纹理和摩尔纹过度学进模型。

## Debug 输出

每次 `showContourPoints=true` 且发生匹配时，目录仍写入：

`/home/hjl-ubuntu/桌面/pattern_contour_debug/<timestamp>_<seq>_<toolId>/`

本轮保证包含：

- `template_crop.png`
- `model_contours_level1_raw.png`
- `model_contours_level2_raw.png`
- `model_contours_level3_raw.png`
- `model_contours_selected_filtered.png`
- `match_contours_selected_global.png`
- `debug_info.json`

`debug_info.json` 已记录 `selectedContourLevel`、`fallbackLevelUsed`、raw/filtered object/point/length、`filterRelaxedBecauseTooSparse`、`filterRulesUsed`、`templateSensitivity`、`createShapeModelContrastUsed`、`createShapeModelMinContrastUsed` 和每个 level 的 `contourLevelStats`。

## 自检数据

使用 `projects/scheme_75381ddd/reference.png` 直接调用 `PatternPresenceHalconRunner`，不经过 UI，也不经过 ToolEngine。三组均 `showContourPointsApplied=true`。

| 灵敏度 | selectedContourLevel | fallbackLevelUsed | raw objects / points / length | filtered objects / points / length | debug 目录 |
|---|---:|---|---:|---:|---|
| 3 | 1 | true | 4753 / 11456 / 7707.22 | 357 / 2246 / 2312.11 | `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_180451_203_1_pattern_contour_auto_tuning_selftest_s3` |
| 6 | 1 | true | 6907 / 18987 / 14246.65 | 512 / 3257 / 3399.61 | `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_180452_076_2_pattern_contour_auto_tuning_selftest_s6` |
| 10 | 2 | false | 92 / 326 / 257.12 | 18 / 143 / 138.61 | `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_180453_014_3_pattern_contour_auto_tuning_selftest_s10` |

s=3/6 的模型只有 level 1 可用，所以按内部策略 fallback 到 level 1；过滤后保留了数百条主要轮廓段，不再只剩几段。s=10 的 level 2 可用，选择 level 2 后 raw 已明显更干净，filtered 进一步压掉短碎纹理。

## 编译结果

- `qmake`：通过。
- `make -j$(nproc)`：通过，并成功链接 `qt_ui_test`。

## 验收项

- 是否新增 UI 参数：没有。
- 是否新增 level 控件：没有。
- level 是否只作为内部自动策略：是。
- showContourPoints=false：代码路径仍不生成模型轮廓 overlay。
- showContourPoints=true：主显示仍为 `contour_line`，不显示满屏 raw 点；只显示 filtered major contours。
- 是否使用 ROI 边界、匹配框、raw edge 冒充模型轮廓：没有，显示来源仍是 HALCON `get_shape_model_contours`。
- 是否修改 ToolEngine：没有。
- 是否修改其他工具算法：没有。
- 是否修改 OK/NG 算法语义：没有。
