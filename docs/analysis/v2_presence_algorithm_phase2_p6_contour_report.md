# V2 Presence Phase 2 P6 ContourPresence Report

## 1. 本轮目标

本轮只处理 P6 ContourPresence。目标是把当前“Contour config -> PatternPresenceRunner -> image patch shape model”的实现，改为独立的 XLD 轮廓模板匹配：

`template ROI -> contour extraction -> create_scaled_shape_model_xld -> find_scaled_shape_model -> get_shape_model_contours overlay`

本轮未改 UI，未改 Dialog 布局，未改 MainWindow / totalwindow / 主流程，未改 OCR，未改 Pattern / Blob / Circle / Edge / Line，未清理、未回退、未 reset 用户已有工作区修改。

## 2. 语义确认

本轮按海康截图中的“轮廓有无”修正为“轮廓模板匹配”，不是当前图 raw contour count/length。原因：

- V3 审计中对应的是 `CreateScaledShapeModelXld -> FindScaledShapeModel` 的轮廓匹配工具。
- 用户操作包含模板区域、检测区域、最小得分、尺度范围、角度范围、匹配极性、显示轮廓点，天然是模板匹配参数组。
- raw contour count/length 只能回答“ROI 里有没有边缘”，不能输出模板匹配的 X/Y、角度、尺度、score，也不能显示匹配后的模型轮廓。

纯当前图轮廓统计应以后另做 RawContourPresence。

## 3. 当前真实 UI 参数表

| UI 参数 | 本轮处理 |
|---|---|
| 模板矩形 ROI | 支持，裁剪模板区域建模 |
| 模板多边形 ROI | 支持，模板裁剪后 `gen_region_polygon -> reduce_domain` |
| 模板 mask | 未实现，payload `templateMaskApplied=false` |
| scaleMode | 参与默认尺度策略；auto 且 100/100 时扩展为 0.9-1.1，显式 scaleMin/scaleMax 优先 |
| speedScale | 已映射到 `find_scaled_shape_model` greediness |
| featureScale | auto 提取时映射到 `edges_sub_pix` sigma |
| thresholdMode | auto/manual 均参与模板轮廓提取 |
| grayThreshold | manual 模式参与 `threshold` |
| thresholdType | light/dark/auto 归一化并参与 threshold 路线 |
| chainMode | auto/manual 均参与最小链长计算 |
| minChainLength | manual 模式作为 `select_contours_xld` 的 contour_length 下限 |
| 检测矩形 ROI | 支持 |
| 检测多边形 ROI | Config 有点集时支持 `gen_region_polygon -> reduce_domain` |
| 自由/圆形检测 ROI | 未完整闭环，payload 标记 fallback/unsupported |
| 检测 mask | 未实现，payload `detectMaskApplied=false` |
| 位置修正 | 未实现，payload `positionCorrectionApplied=false` |
| minScore | 用作 `find_scaled_shape_model` MinScore |
| polarity | 映射 HALCON metric |
| scaleMin/scaleMax | 百分比转倍率，90/110 -> 0.9/1.1 |
| angleStart/angleExtent | 度转弧度传 HALCON |
| timeoutMs | 尝试 `set_shape_model_param("timeout", timeoutMs)` |
| showContourPoints | true 时显示 `get_shape_model_contours -> affine_trans_contour_xld` 真实模型轮廓 |
| sortMode | 未实际排序，payload `sortModeApplied=false` |
| judgeBasis/existOk/scoreThreshold | 已用于 OK/NG 判定 |

## 4. 修改文件清单

- `src/algorithms/presence/ContourPresenceHalconRunner.cpp`
- `src/algorithms/presence/PatternPresenceHalconApi.h`
- `src/algorithms/presence/PatternPresenceHalconApi.cpp`
- `src/tooladapters/ContourPresenceAdapter.cpp`
- `src/frame/FrameViewHelper.cpp`
- `docs/analysis/v2_presence_algorithm_phase2_p6_contour_report.md`
- `/tmp/presence_phase3_contour_smoke/*`

## 5. 修改前算法链

旧实现：

`Contour config -> PatternPresenceHalconRunner -> create_scaled_shape_model/find_scaled_shape_model on image patch`

问题：

- ContourPresence 委托 PatternPresence。
- 强制 scale 100/100。
- thresholdMode、grayThreshold、thresholdType、chainMode、minChainLength、speedScale、featureScale 没有参与检测。
- showContourPoints 来自模板 patch 临时 Canny 轮廓，不是 HALCON 模型轮廓。

## 6. 修改后算法链

新实现：

1. 基准图模板 ROI 裁剪。
2. 模板多边形 ROI 可 `reduce_domain`。
3. 根据 UI 参数提取模板 XLD 轮廓。
4. `create_scaled_shape_model_xld` 创建 XLD shape model。
5. 当前图检测 ROI 裁剪。
6. 检测多边形 ROI 可 `reduce_domain`。
7. `find_scaled_shape_model` 搜索 XLD 轮廓模型。
8. 输出 bestRow/bestCol/bestAngleDeg/bestScale/bestScore/matchCount。
9. `get_shape_model_contours -> vector_angle_to_rigid -> hom_mat2d_scale_local -> affine_trans_contour_xld` 生成 overlay。

ContourPresence 已不再委托 PatternPresence 做普通图案匹配。

## 7. 模板轮廓提取策略

- `thresholdMode=manual`：按 `thresholdType` 执行 light/dark/auto threshold，之后 `connection -> gen_contour_region_xld -> select_contours_xld(contour_length)`。
- `thresholdMode=auto`：执行 `edges_sub_pix(canny) -> segment_contours_xld -> select_contours_xld(contour_length)`。
- 模板多边形先在模板裁剪图中生成 local polygon region，再 `reduce_domain`。
- 如果 XLD 轮廓为空、点数过少或总长度过短，返回 `success=false`，不使用空 XLD 建模。

未使用 ROI 外框冒充轮廓，未使用当前图 raw edge 冒充匹配结果。

## 8. grayThreshold / minChainLength 生效

- `grayThreshold` 在 manual threshold 路线中作为 `threshold` 分界值。
- `thresholdType=light` 使用 `[grayThreshold, 255]`。
- `thresholdType=dark` 使用 `[0, grayThreshold]`。
- `thresholdType=auto` 同时尝试 light/dark，选择点数/长度更好的候选。
- `minChainLength` 在 `chainMode=manual` 时作为 `select_contours_xld` 的 contour_length 下限。
- `chainMode=auto` 时根据模板 ROI 尺寸和 speedScale 推导内部 minChainLength。

## 9. scale / angle / polarity

- `scaleMin/scaleMax` 百分比转倍率后传给建模和查找。
- `scaleMode=auto` 且 UI 仍为默认 100/100 时，内部扩展为 0.9-1.1；显式 scaleMin/scaleMax 优先。
- `angleStart/angleExtent` 度转弧度传给 HALCON。
- `polarity` 映射为 `use_polarity`、`ignore_global_polarity` 或 `ignore_local_polarity`。

## 10. payload 新增字段

重点字段：

`algorithm="xld_contour_template_matching"`, `modelType="scaled_shape_model_xld"`, `found`, `matchCount`, `bestScore`, `bestAngleDeg`, `bestScale`, `bestRow`, `bestCol`, `templateContourCount`, `templateContourPointCount`, `templateContourTotalLength`, `templateContourExtractionMode`, `thresholdModeUsed`, `grayThresholdUsed`, `thresholdTypeUsed`, `chainModeUsed`, `minChainLengthUsed`, `scaleModeApplied`, `speedScaleApplied`, `featureScaleApplied`, `sigmaUsed`, `scaleMinUsed`, `scaleMaxUsed`, `scaleRangeApplied`, `angleStartDegUsed`, `angleExtentDegUsed`, `searchMinScoreUsed`, `judgeScoreThresholdUsed`, `metricUsed`, `numLevelsUsed`, `greedinessUsed`, `maxOverlapUsed`, `timeoutApplied`, `coordinateMode`, `modelContourOverlayApplied`, `templateMaskApplied`, `detectMaskApplied`, `positionCorrectionApplied`, `sortModeRequested`, `sortModeApplied`, `okNgReason`。

## 11. Overlay 变化

Overlay 包含：

- `detect_roi`
- `template_roi`，基准图测试且尺寸一致时显示
- `match_bbox`，明确是 bbox，不冒充轮廓
- `match_center`
- `match_score_text`
- `contour_model_line`，来源为真实 HALCON shape model contours 的仿射变换结果

当前 ToolOverlay 没有 XLD 原生类型，因此用多段 Line 近似显示模型轮廓，并写入 `contourOverlayLineSegmentCount`。

## 12. OK/NG 与 success/ok 语义

- 参数错误、空图、无 reference、无模板 ROI、HALCON 异常：`success=false, ok=false`。
- 算法正常但没匹配：`success=true, found=false`，OK 按 existOk / judgeBasis。
- score 判定且无匹配：existOk=true -> NG；existOk=false -> OK，并写 `noMatchAndExistOkFalse=true`。
- 匹配到且 score 不足：`success=true, found=true, ok=false`。
- 匹配到且满足规则：`success=true, ok=true`。

`okNgReason` 明确写入原因。

## 13. smoke 测试

目录：`/tmp/presence_phase3_contour_smoke`

执行：

```bash
cd /tmp/presence_phase3_contour_smoke
qmake presence_phase3_contour_smoke.pro
make -j8
./presence_phase3_contour_smoke
```

结果摘要：

| Case | success | ok | matchCount | bestScore | angle | scale | 说明 |
|---|---:|---:|---:|---:|---:|---:|---|
| basic | true | true | 1 | 0.9983 | -0.01 | 1.0011 | 基础匹配 |
| translation | true | true | 1 | 0.9983 | -0.01 | 1.0011 | bestRow/bestCol 随目标平移 |
| rotation | true | true | 1 | 0.9971 | -24.09 | 0.9957 | 角度输出有效 |
| scale | true | true | 1 | 0.9979 | 0.11 | 1.2366 | 证明不再强制 100/100 |
| no_target.existOk_true | true | false | 0 | 0 | 0 | 1.0 | 无目标 NG |
| no_target.existOk_false | true | true | 0 | 0 | 0 | 1.0 | 不存在 OK |
| gray_threshold.low | true | true | 1 | 0.9957 | 0 | 1.0018 | grayThreshold=80，轮廓点 484 |
| gray_threshold.high | true | true | 1 | 0.9993 | -0.02 | 1.0125 | grayThreshold=150，轮廓点 333 |
| chain.short | true | true | 1 | 0.9957 | 0 | 1.0018 | minChainLength=3，轮廓数 4 |
| chain.long | true | true | 1 | 0.9996 | 0.01 | 1.0143 | minChainLength=90，轮廓数 1 |
| show_contour.true | true | true | 1 | 0.9983 | -0.01 | 1.0011 | overlay 有 `contour_model_line` |
| show_contour.false | true | true | 1 | 0.9983 | -0.01 | 1.0011 | overlay 无模型轮廓线 |

最终 `failed=0`。

GUI Overlay 未做自动截图验证。手动验证步骤：

1. 加载静态 reference 图。
2. 创建 ContourPresence，画模板 ROI 和检测 ROI。
3. 勾选运行显示轮廓点。
4. 运行基准图测试/测试运行。
5. 检查画面是否显示 `detect_roi`、`match_center`、score 文本和贴合目标的青色模型轮廓线。

## 14. 编译结果

执行：

```bash
qmake qt_ui_test.pro
make -j8
git diff --check
```

结果：

- `qmake qt_ui_test.pro` 通过。
- `make -j8` 通过，生成 `qt_ui_test`。
- `git diff --check` 通过。

## 15. 仍未实现的内容

- 模板 mask 未进入 Runner，payload `templateMaskApplied=false`。
- 检测 mask 未进入 Runner，payload `detectMaskApplied=false`。
- 独立位置修正未实现，payload `positionCorrectionApplied=false`。
- sortMode 未实际排序，payload `sortModeApplied=false`。
- 自由/圆形检测 ROI 当前 Config 未完整闭环，本轮不假装支持，payload 写 fallback/unsupported。
- GUI overlay 自动截图未做，本轮提供手动 GUI 验证步骤。

## 16. 风险和后续建议

- XLD 轮廓提取质量依赖模板 ROI 和阈值/链长参数；现场图像建议先通过 payload 的 `templateContourCount/templateContourPointCount/templateContourTotalLength` 判断模型质量。
- `scaleMode` 目前只参与默认尺度策略，后续可结合 UI 语义做更完整的自动尺度估计。
- `speedScale` 当前映射到 greediness，后续可进一步影响 numLevels/optimization。
- 若需要纯 raw contour count/length，应新增 RawContourPresence，不应复用当前 ContourPresence 语义。

## 17. 明确声明

- 未改 UI。
- 未改主流程。
- 未改 OCR。
- 未改 Pattern / Blob / Circle / Edge / Line。
- 未新增复杂 UI 控件。
- 未把算法逻辑写进 Dialog 或 MainWindow。
- 未清理/回退用户已有工作区修改。
- ContourPresence 已经不再委托 PatternPresence 做普通图案匹配。
- 当前 ContourPresence 是独立 XLD 轮廓模板匹配。
