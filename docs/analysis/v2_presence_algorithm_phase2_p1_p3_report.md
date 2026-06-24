# V2 Presence Algorithm Phase 2 Stage 1 Report: P1-P3

## 1. 本阶段目标

本阶段只执行阶段 0 和阶段 1，优化：

- P1 PatternPresence
- P2 LinePresence
- P3 EdgePresence

P4 BlobPresence、P5 CirclePresence、P6 ContourPresence 只阅读审计和保留后续计划，本阶段未改其代码。

本阶段没有改 UI，没有改 Dialog 布局，没有改 MainWindow/totalwindow 主流程，没有改 OCR 链路，没有清理、回退或整理用户已有工作区修改。

## 2. 阶段 0 记录

已读取：

- `docs/analysis/v2_presence_algorithm_audit.md`
- `docs/analysis/v3_presence_tools_real_usage_audit.md`

执行 `git status --short` 时，工作区已有大量修改和未跟踪文件，包含 Dialog、MainWindow、OCR、Blob/Circle Presence、docs、projects 等。本阶段未回退、未删除、未清理这些既有修改。

构建方式确认：

- 项目使用 qmake：`qt_ui_test.pro`
- 未发现 CMakeLists.txt
- 未发现现有 build 目录

## 3. 修改文件清单

本阶段实际修改：

- `src/algorithms/presence/PatternPresenceHalconApi.h`
- `src/algorithms/presence/PatternPresenceHalconApi.cpp`
- `src/algorithms/presence/PatternPresenceHalconRunner.cpp`
- `src/tooladapters/PatternPresenceAdapter.cpp`
- `src/algorithms/presence/LinePresenceHalconRunner.cpp`
- `src/algorithms/presence/EdgePresenceHalconRunner.cpp`
- `src/frame/FrameViewHelper.cpp`

未修改：

- UI `.ui`
- Dialog 布局
- MainWindow/主流程
- OCR Runner/Adapter
- P4-P6 Runner/Adapter

## 4. 文件修改摘要

- `PatternPresenceHalconApi.*`：增加 scaled shape model、shape model timeout、局部缩放矩阵等 HALCON 动态符号。
- `PatternPresenceHalconRunner.cpp`：移除非 100 scale 拒绝逻辑，改为 `create_scaled_shape_model -> find_scaled_shape_model`；新增 scale fallback、bestScale、okNgReason、真实 `get_shape_model_contours` overlay。
- `PatternPresenceAdapter.cpp`：scaleMin/scaleMax 不再提前 clamp，保留原始 UI 值给 Runner 做非法输入 fallback。
- `LinePresenceHalconRunner.cpp`：新增 Metrology Line 主路径，使用 line-band ROI、采样矩形近似 overlay、采样点、拟合线和 hitCount 判定。
- `EdgePresenceHalconRunner.cpp`：新增 `GenMeasureRectangle2 -> MeasurePos` 主路径，使用 line-band ROI、测量矩形和 edgePoints payload。
- `FrameViewHelper.cpp`：补充 P2/P3 新 overlay label 的颜色和层级。

## 5. P1 PatternPresence

优化前算法链：

`create_shape_model -> find_shape_model`，非 100/100 scale 直接返回 `unsupported_scale_range`；轮廓显示来自自动模型域 region contour，不是 `get_shape_model_contours`。

优化后算法链：

`create_scaled_shape_model -> set_shape_model_param(timeout) -> get_shape_model_contours -> find_scaled_shape_model -> vector_angle_to_rigid -> hom_mat2d_scale_local -> affine_trans_contour_xld`

UI 参数映射：

| UI 参数 | 当前映射 |
|---|---|
| scaleMin/scaleMax | 百分比转 0-1，非法值 fallback 到 0.9-1.1 |
| angleStart/angleExtent | 度转弧度 |
| minScore | 搜索最低分，百分比转 0-1 |
| scoreThreshold | 判定阈值，百分比转 0-1 |
| polarity | `use_polarity` / `ignore_global_polarity` / `ignore_local_polarity` |
| showContourPoints | 控制真实模型轮廓 overlay |
| sortMode | 记录但未应用 |

内部默认：

| 参数 | 值 |
|---|---|
| numLevels | `auto` |
| findNumLevels | `0` |
| greediness | `0.5` |
| maxOverlap | `0.5` |
| optimization | `auto` |
| minContrast | 由 templateSensitivity 映射 |

新增 payload 重点字段：

`algorithm`, `shapeModelType`, `found`, `matchCount`, `bestScore`, `bestAngleDeg`, `bestScale`, `scaleMinUsed`, `scaleMaxUsed`, `angleStartDegUsed`, `angleExtentDegUsed`, `searchMinScoreUsed`, `judgeScoreThresholdUsed`, `metricUsed`, `modelContourOverlayApplied`, `matchBboxOverlayApplied`, `sortModeApplied`, `maskApplied`, `positionCorrectionApplied`, `okNgReason`。

Overlay 变化：

- `detect_roi`
- `match_center`
- `match_score`
- `match_rect`
- `contour_line`，来源为 `get_shape_model_contours` 变换后的模型轮廓

OK/NG 变化：

- 无匹配但算法正常：`success=true`
- presence 判定：`ok = existOk ? found : !found`
- score 判定：无匹配时强制 `ok=false`，避免 `scoreThreshold=0` 误判 OK

## 6. P2 LinePresence

优化前算法链：

`edges_sub_pix -> segment_contours_xld -> fit_line_contour_xld`，line-band ROI 只作为外接矩形参与，edgePolarity 未真实进入灰度跳变方向。

优化后算法链：

`create_metrology_model -> add_metrology_object_line_measure -> set_metrology_object_param -> apply_metrology_model -> get_metrology_object_result -> get_metrology_object_measures`

UI 参数映射：

| UI 参数 | 当前映射 |
|---|---|
| searchLineP1/P2 | 原图像素线段起终点 |
| searchBandWidth | 搜索带半宽 |
| sensitivity | `thresholdUsed=clamp(120-sensitivity,5,120)` |
| edgePolarity | `positive/negative/all` |
| edgeType | first/last -> first/last，strongest -> all，manual -> all 并标记 unsupported |
| lineDegree | 映射 `maxFitErrorUsed` |

内部默认：

| 参数 | 值 |
|---|---|
| sampleCount | 20 |
| sigma | 1.0 |
| minScore | 0.3 |
| minHitCount | `max(3, sampleCount*0.3)` |
| angleRangeApplied | false |
| lengthRangeApplied | internal |

新增 payload 重点字段：

`algorithm="metrology_line"`, `found`, `lineCount`, `startPoint`, `endPoint`, `midpoint`, `angleDeg`, `length`, `score`, `fitError`, `hitCount`, `sampleCount`, `samplePoints`, `measureRegionsCount`, `thresholdUsed`, `sigmaUsed`, `transitionUsed`, `selectUsed`, `minScoreUsed`, `minHitCountUsed`, `maxFitErrorUsed`, `sensitivityMapping`, `lineDegreeMapping`, `lineBandApplied`, `positionCorrectionApplied`, `maskApplied`, `okNgReason`。

Overlay 变化：

- `line_band_center`
- `line_band_boundary`
- `measure_regions`
- `sample_points`
- `fitted_line`
- `line_result_text`

OK/NG 变化：

- Metrology 正常但没找到线：`success=true, found=false`
- 找到 line 且 hitCount/score/fitError 达标：`found=true`
- `ok = existOk ? found : !found`

## 7. P3 EdgePresence

优化前算法链：

`edges_sub_pix -> XLD contour count/length`，line-band ROI 只作为外接矩形参与，result.count 是 XLD contour 数量。

优化后算法链：

`gen_measure_rectangle2 -> measure_pos -> edgePoints/amplitudes/distances -> count 判定`

UI 参数映射：

| UI 参数 | 当前映射 |
|---|---|
| searchLineP1/P2 | 测量矩形中心线 |
| searchBandWidth | 测量矩形半宽 |
| sensitivity | `thresholdUsed=clamp(120-sensitivity,5,120)` |
| edgePolarity | `positive/negative/all` |

内部默认：

| 参数 | 值 |
|---|---|
| sigma | 1.0 |
| select | all |
| countMin | 1 |
| countMax | INT_MAX |
| countRangeSource | internal_default |

新增 payload 重点字段：

`algorithm="measure_pos"`, `found`, `edgeCount`, `edgePoints`, `amplitudes`, `distances`, `thresholdUsed`, `sigmaUsed`, `transitionUsed`, `selectUsed`, `countMinUsed`, `countMaxUsed`, `countRangeSource`, `lineBandApplied`, `lineBandFallback`, `measureRectangle`, `positionCorrectionApplied`, `maskApplied`, `okNgReason`。

Overlay 变化：

- `line_band_center`
- `line_band_boundary`
- `measure_rectangle`
- `edge_points`
- `edge_count_text`

OK/NG 变化：

- `found = edgeCount in [countMin,countMax]`
- `ok = existOk ? found : !found`
- MeasurePos 正常但无边缘点：`success=true, found=false`

## 8. UI 未生效参数

| 工具 | 参数 | 当前处理 |
|---|---|---|
| Pattern | mask | `maskApplied=false`，Runner 暂不支持 |
| Pattern | sortMode | `sortModeApplied=false` |
| Pattern | positionCorrection | `positionCorrectionApplied=false` |
| Line | mask | `maskApplied=false` |
| Line | positionCorrection | `positionCorrectionApplied=false` |
| Line | edgeType=manual | `manualUnsupported=true` |
| Edge | mask | `maskApplied=false` |
| Edge | positionCorrection | `positionCorrectionApplied=false` |
| Edge | countMin/countMax UI | UI 未暴露，使用内部默认 |

## 9. 编译

命令：

```bash
qmake qt_ui_test.pro
make -j8
```

结果：编译通过，生成 `qt_ui_test`。

## 10. 静态图测试

本阶段未开启相机。

执行了非 GUI smoke 测试程序 `/tmp/presence_phase1_smoke`，使用 OpenCV 合成静态灰度图直接调用 P1-P3 Runner。

测试结果：

| 工具 | 结果 |
|---|---|
| Pattern | `success=true`, `ok=true`, `algorithm=scaled_shape_model`, `scaleMinUsed=0.9`, `scaleMaxUsed=1.1` |
| Line | `success=true`, `ok=true`, `algorithm=metrology_line`, `hitCount=40` |
| Edge | `success=true`, `ok=false`, `algorithm=measure_pos`, `edgeCount=0` |

说明：Edge MeasurePos 路径已正常执行并保持 `success/ok` 分离；当前合成边界样例未检出 edge point，需要在 GUI/真实静态图中继续验证测量矩形方向、line-band 配置与图像灰度跳变方向。

未做 GUI 测试，原因是当前会话没有打开 Qt GUI 测试流程。建议手动测试：

1. 打开项目，加载静态 reference 图。
2. 分别创建 PatternPresence、LinePresence、EdgePresence。
3. Pattern 设置 scaleMin=90、scaleMax=110，执行基准图测试和测试运行。
4. Line 使用线型 ROI 覆盖真实直线边缘，检查采样矩形、采样点、拟合线。
5. Edge 使用线型 ROI 穿过真实灰度跳变，检查测量矩形、edge points、edgeCount。
6. 保存配置后重新运行，确认 payload 字段仍存在。

## 11. 未解决问题

- Pattern `sortMode` 仍未实际排序。
- Pattern/Line/Edge 的 mask 和 positionCorrection 仍未实现，只在 payload 中明确标记。
- Edge 的合成 smoke 有无目标样例未检出 edge point，需 GUI/真实图确认 MeasurePos 方向和 ROI 使用体验。
- Overlay 仍基于现有 Rect/Line/Circle/Polygon/Text 类型，HALCON region/XLD 只能用线段近似显示。

## 12. 风险和后续建议

- P2/P3 已改为工业卡尺/测量语义，但真实项目图片中的 polarity、threshold、ROI 方向仍需要现场参数确认。
- P4-P6 不应混入本阶段改动。下一阶段建议按审计顺序单独处理 Blob/Circle，然后单独处理 ContourPresence 语义拆分风险。
- 长期建议为 Edge/Line 增加专门的非 GUI 回归测试夹具，覆盖有目标、无目标、空图、非法 ROI 和极端参数。

## 13. 明确声明

- 本阶段没有改 UI。
- 本阶段没有改主流程。
- 本阶段没有破坏 OCR。
- 本阶段没有清理、回退或删除用户已有工作区修改。
