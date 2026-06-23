# PatternPresence 模型轮廓点质量修复报告

## 修改文件清单

- `src/algorithms/presence/PatternPresenceHalconRunner.cpp`
- `src/PatternPresenceDialog.cpp`

未修改 `ToolEngine`，未修改其他工具算法。

## 问题根因

修复前 debug 目录：

`/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_161746_324_1_pattern_presence_9286a6f5-7b58-415d-91e4-86cdc99d1d9e`

修复前关键数据：

- `createShapeModelContrast`: `"auto"`
- `createShapeModelMinContrast`: `"auto"`
- `templateSensitivity`: `2`
- `contourObjectCount`: `3`
- `contourPointCount`: `13`
- `showContourPointsApplied`: `true`
- `contourSource`: `halcon_shape_model_contours`
- `findMs`: `1029`

之前只有 13 个模型轮廓点的直接原因是 `create_shape_model` 的 `Contrast` 和 `MinContrast` 固定传 `"auto"`。UI 和 Adapter 已经传递了 `templateSensitivity`，但 Runner 没有把它映射到 HALCON 建模阈值，也没有把灵敏度放进模型缓存 key，因此灵敏度变化不会真正影响模型轮廓提取。

`NumLevels`、`AngleStep`、`Optimization` 仍为 HALCON `"auto"`；`Metric` 来自极性设置；`scaleMin/scaleMax` 用于查找阶段，不是本次模型轮廓点过少的主要原因。

## 修复内容

新增 `templateSensitivity -> Contrast/MinContrast` 映射：

- `s = clamp(templateSensitivity, 1, 10)`
- `Contrast = max(20, 80 - s * 6)`
- `MinContrast = max(3, Contrast / 2)`

对应示例：

- `s=1`: `Contrast=74`, `MinContrast=37`
- `s=5`: `Contrast=50`, `MinContrast=25`
- `s=10`: `Contrast=20`, `MinContrast=10`

Runner 现在用整数 tuple 传给 `create_shape_model`，不再固定使用 `"auto"`。模型缓存 key 已加入 `templateSensitivity`、`Contrast`、`MinContrast`，避免复用旧参数生成的稀疏模型。

payload/debug_info 新增或修正：

- `templateSensitivity`
- `templateSensitivityClamped`
- `createShapeModelContrastUsed`
- `createShapeModelMinContrastUsed`
- `modelContourObjectCount`
- `modelContourPointCount`
- `contourQualityStatus`
- `contourQualityReason`
- `contourTotalLength`

## 稀疏轮廓兜底

`showContourPoints=true` 时，如果模型轮廓过少，则不再显示零散乱点：

- `contourPointCount < 50`
- 或 `contourObjectCount < 2` 且总轮廓长度 `< 24px`

触发后 payload：

- `showContourPointsApplied=false`
- `contourSource="shape_model_contours_too_sparse"`
- `reason="shape model contours too sparse"`
- `contourPointCount=实际点数`

状态栏提示改为：

`当前模型轮廓点过少，请提高模板灵敏度或调整模板区域。`

该兜底只影响轮廓显示，不影响 found、score、OK/NG 判断。

## 修复前后对比

修复前：

- debug: `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_161746_324_1_pattern_presence_9286a6f5-7b58-415d-91e4-86cdc99d1d9e`
- `model_contours_local.png`: `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_161746_324_1_pattern_presence_9286a6f5-7b58-415d-91e4-86cdc99d1d9e/model_contours_local.png`
- `match_contours_global.png`: `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_161746_324_1_pattern_presence_9286a6f5-7b58-415d-91e4-86cdc99d1d9e/match_contours_global.png`
- `contourPointCount=13`
- `showContourPointsApplied=true`

修复后 smoke，多边形模板，低灵敏度 `s=2`：

- debug: `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_163237_336_1_pattern_presence_smoke_s2`
- `Contrast=68`, `MinContrast=34`
- `modelContourObjectCount=11`
- `modelContourPointCount=12`
- `contourQualityStatus="too_sparse"`
- `showContourPointsApplied=false`
- 结果：不会再显示零散模型点。

修复后 smoke，多边形模板，高灵敏度 `s=10`：

- debug: `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_163237_336_1_pattern_presence_smoke_s10`
- `model_contours_local.png`: `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_163237_336_1_pattern_presence_smoke_s10/model_contours_local.png`
- `match_contours_global.png`: `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_163237_336_1_pattern_presence_smoke_s10/match_contours_global.png`
- `Contrast=20`, `MinContrast=10`
- `modelContourObjectCount=84`
- `modelContourPointCount=119`
- `contourQualityStatus="usable"`
- `showContourPointsApplied=true`

修复后 smoke，矩形模板，高灵敏度 `s=10`：

- debug: `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260525_163312_170_1_pattern_presence_smoke_rectangle_s10`
- `modelContourObjectCount=2303`
- `modelContourPointCount=7862`
- `contourQualityStatus="usable"`
- `showContourPointsApplied=true`

`showContourPoints=false` smoke：

- `found=true`
- `showContourPointsRequested=false`
- `showContourPointsApplied=false`
- `contourSource="not_requested"`
- 未生成轮廓 debug 目录，OK/NG 语义不变。

## 性能记录

修复前 debug：`findMs=1029ms`。

修复后 smoke：

- 多边形 `s=2`: `findMs=163ms`
- 多边形 `s=10`: `findMs=24ms`
- 矩形 `s=10`: `findMs=20ms`

这些 smoke 数据使用保存的 reference 图和 debug ROI 直接调用 Runner，不能等同于完整 UI 场景性能；正式参数页测试仍会在 payload/debug_info 中记录 `findMs`。如果实际模板因更多模型点导致查找变慢，应按质量和速度权衡继续调参，而不是降低 OK/NG 语义。

## 编译结果

- `qmake`: 通过。
- `make -j$(nproc)`: 通过并成功链接 `qt_ui_test`。
- 编译器仍报告一个已有 warning：`rotatedRectCorners` 未使用；本次未为轮廓点质量修复改动该无关函数。

## 结论

- `templateSensitivity` 已真正映射到 HALCON `create_shape_model` 的 `Contrast/MinContrast`。
- 正式显示仍只使用 `get_shape_model_contours`，没有用 ROI 边界、raw edge、匹配框冒充模型轮廓。
- 稀疏模型轮廓会被明确拦截并提示，不再假装成可用轮廓效果。
- 未修改 PatternPresence OK/NG 判断语义。
- 未修改其他工具算法。
