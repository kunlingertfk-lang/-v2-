# PatternPresence 模型轮廓清理显示报告

## 修改文件清单

- `src/algorithms/presence/PatternPresenceHalconRunner.cpp`
- `src/PatternPresenceDialog.cpp`
- `docs/analysis/pattern_model_contour_clean_display_report.md`

未修改 OCR / Blob / Circle / Edge / Line / Contour 算法，未修改 ToolEngine。

## 根因

`showContourPoints` 使用的是 HALCON `get_shape_model_contours` 返回的 shape model XLD 轮廓。当前模板里存在内部纹理、噪声和摩尔纹边缘；旧显示路径把这些 XLD object 基本按 raw 结果画出来，所以会出现大量青色短碎线段。灵敏度继续升高时，`create_shape_model` 的 Contrast / MinContrast 偏低会把更多弱纹理边缘学进模型，噪声轮廓进一步增多。

本次不再以增加点数为目标，而是把显示目标改为 `major_contours_only`：只显示过滤后的主要有效模型轮廓，raw 轮廓仅保留在诊断图里。

## 轮廓统计

新增每个 XLD contour 的质量记录：

- `pointCount`
- `polylineLength`
- `bboxWidth` / `bboxHeight` / `bboxArea`
- `shortContour`
- `tooFewPoints`
- `tinyBBox`
- `shortFragmentContour`
- `displayed`

`debug_info.json` / payload 新增汇总字段：

- `rawContourObjectCount`
- `rawContourPointCount`
- `rawContourTotalLength`
- `filteredContourObjectCount`
- `filteredContourPointCount`
- `filteredContourTotalLength`
- `minContourLengthUsed`
- `minContourPointCountUsed`
- `shortContourFilteredCount`
- `tinyBBoxFilteredCount`
- `tooFewPointContourFilteredCount`
- `majorContourTrimmedCount`
- `contourDisplayMode`
- `contourNoiseSuppressionApplied`

本轮用旧 debug 的 `template_crop.png` 做了一次自匹配验证：

- debug 目录：`/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_172705_386_1_pattern_filter_selftest`
- `templateSensitivity=4`
- `createShapeModelContrastUsed=54`
- `createShapeModelMinContrastUsed=27`
- raw：`3069` objects，`7252` points，total length `4788.22`
- filtered：`5` objects，`109` points，total length `118.78`
- `shortContourFilteredCount=3063`
- `tinyBBoxFilteredCount=2740`
- `tooFewPointContourFilteredCount=2763`
- `majorContourTrimmedCount=1`
- `contourNoiseSuppressionApplied=true`
- `showContourPointsApplied=true`

这说明显示层已经把大量短碎纹理对象压下去，只保留少数主要 contour 用于界面显示。

## 过滤规则

只影响 `showContourPoints=true` 时的显示 overlay 和 filtered debug 图，不影响 found / score / OK / NG。

- 最小长度：`max(15px, min(templateW, templateH) * 0.03)`
- 最小点数：`pointCount >= 5`
- 微小 bbox：`bbox width < 3 && bbox height < 3` 时过滤
- 主要轮廓选择：按 polyline length 降序，保留累计长度前 `80%`，最多 `96` 条
- UI overlay 只画 filtered major contours 的 polyline，不再画大量点 marker
- filtered 后有效轮廓不足时，不画乱点，状态提示：`模型有效轮廓过少，请调整灵敏度或模板区域。`

## 灵敏度映射

`templateSensitivity` 现在映射到 `create_shape_model` 的 `Contrast / MinContrast`，并进入模型 cache key，避免旧模型复用错误参数。

版本：`pattern_shape_model_contrast_v2_noise_guard`

映射：

```text
s = clamp(templateSensitivity, 1, 10)
contrast = max(30, 70 - s * 4)
minContrast = max(10, contrast / 2)
```

示例：

- `s=1` -> `contrast=66`, `minContrast=33`
- `s=5` -> `contrast=50`, `minContrast=25`
- `s=10` -> `contrast=30`, `minContrast=15`

高灵敏度仍能学习更多边缘，但不会把 Contrast / MinContrast 降到无下限，减少内部纹理和背景噪声爆炸。模板预平滑本轮未启用：`modelPreSmoothApplied=false`。

## Debug 图

`showContourPoints=true` 后输出：

- `template_crop.png`
- `template_gray.png`
- `model_contours_local_raw.png`
- `model_contours_local_filtered.png`
- `match_contours_global_filtered.png`
- `debug_info.json`

本轮验证目录：

`/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_172705_386_1_pattern_filter_selftest`

旧 unfiltered 诊断目录示例：

`/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_164055_448_31_pattern_presence_9286a6f5-7b58-415d-91e4-86cdc99d1d9e`

旧目录中 raw contour 曾达到 `2692` objects / `6111` points / total length `3897.62`，符合“内部纹理轮廓很多”的现象。

## 显示效果

实现层面已经改为接近海康的主要轮廓显示方式：UI 只显示 filtered major contours，raw contours 只进诊断图。自匹配验证中 raw `3069` 条被压到 filtered `5` 条，显示对象数量明显收敛。最终是否与海康视觉效果完全一致，需要用户用实拍模板在界面目视确认。

## 不足

- 当前过滤优先解决短碎纹理和微小 bbox；如果某些内部纹理本身是较长连续边，仍可能被保留为主要轮廓。
- 本轮未启用 Gaussian / median 平滑，避免改变匹配输入语义。若后续实拍仍有长内部纹理，可只对建模输入增加轻度平滑并记录到 payload。

## 编译结果

执行：

```text
qmake
make -j$(nproc)
```

结果：均成功。最终 `make -j$(nproc)` 无 warning / error。

## 算法语义声明

- 是否修改 PatternPresence OK/NG 判断语义：没有。
- 是否修改 PatternPresence found / score / count 语义：没有。
- 是否修改 ToolEngine 调度原则：没有。
- 是否修改其他工具算法：没有。
- 是否用 ROI 边界、匹配框、raw edge 冒充模型轮廓：没有。
