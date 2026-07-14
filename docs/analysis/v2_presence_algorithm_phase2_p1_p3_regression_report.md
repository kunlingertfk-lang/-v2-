# V2 Presence Phase 2 P1-P3 Regression Report

## 1. 本轮目标

本轮执行阶段 1.5：复核 PatternPresence、LinePresence、EdgePresence，并专项验证 EdgePresence 的 `GenMeasureRectangle2 -> MeasurePos` 测量方向、`phi`、`length1/length2` 和 edge count 统计。

本轮未改 UI，未改主流程，未改 OCR，未改 P6 ContourPresence，未清理、未回退、未 reset 工作区已有修改。

## 2. git status 摘要

按要求先执行过 `git status --short`。当前工作区在本轮开始前已有大量修改和未跟踪文件，包含 Dialog、MainWindow、OCR、P1-P5 runner/adapter、UI、docs、projects 等。本轮仅在允许范围内继续修改算法/overlay 相关文件，未整理或回退既有修改。

本轮相关修改文件：

- `src/algorithms/presence/EdgePresenceHalconRunner.cpp`
- `src/frame/FrameViewHelper.cpp`
- `/tmp/presence_phase2_smoke/main.cpp`
- `/tmp/presence_phase2_smoke/presence_phase2_smoke.pro`
- `/tmp/presence_phase2_smoke/edge_measure_experiment.cpp`

## 3. 回归检查项

已读取：

- `docs/analysis/v2_presence_algorithm_audit.md`
- `docs/analysis/v2_presence_algorithm_phase2_p1_p3_report.md`
- `docs/analysis/v3_presence_tools_real_usage_audit.md`

检查结果：

- Pattern 主链路仍为 `create_scaled_shape_model -> find_scaled_shape_model`。
- Pattern `scaleMin/scaleMax` 仍按 UI 百分比转 HALCON scale，例如 90/110 -> 0.9/1.1。
- Pattern overlay 仍使用 `get_shape_model_contours` 产生的真实模型轮廓。
- Pattern 无匹配但算法正常时保持 `success=true`，OK/NG 由 presence/score 规则决定。
- Line 主链路仍为 HALCON Metrology Line。
- Line line-band ROI 真实参与 `AddMetrologyObjectLineMeasure`，不是只使用外接矩形。
- Line payload 仍包含 `samplePoints`、`measureRegionsCount`、`hitCount`、`fitted_line` 相关结果。
- Edge 主链路为 `GenMeasureRectangle2 -> MeasurePos`。

## 4. Edge 专项测试设计

新增非 GUI smoke 夹具：`/tmp/presence_phase2_smoke`。

Edge 专项覆盖：

- 水平测量线 + 垂直黑白边界。
- 垂直测量线 + 水平黑白边界。
- `black_to_white`。
- `white_to_black`。
- `any`。
- 无边界图。
- `existOk=false` 下无边界 OK 反转。

另加直接 HALCON 对照程序：

- `/tmp/presence_phase2_smoke/edge_measure_experiment.cpp`

对照结论：

- `phi=line` 能检出沿测量线方向的边缘。
- `phi=line+90` 在横/竖合成图中不能检出。
- `length1=line half length`、`length2=band half width` 正确。
- HALCON 的角度坐标使用 y-up 约定，因此 Runner 中 `phi` 修正为 `atan2(-dy, dx)`，让 `black_to_white/white_to_black` 与用户从 `searchLineP1` 到 `searchLineP2` 的方向一致。

## 5. Edge 修正内容

修正点：

- `edgeCount` 不再取 `row/col/amplitude/distance` 四个 tuple 的最小长度。
- `MeasurePos` 对单个边缘时 `distance` tuple 可以为空；原逻辑因此把真实单边缘误算为 0。
- 现在 `edgeCount` 使用 `row/col/amplitude` 的最小长度，`distances` 只在 HALCON 返回时记录。
- `measureDirectionMode="along_line"`。
- `phiConvention="halcon_cartesian_y_up_from_search_line_p1_to_p2"`。
- payload 明确写入 `measureRectangle`、`thresholdUsed`、`transitionUsed`、`selectUsed`、`lineBandApplied`、`edgeCount`、`okNgReason`。

未采用：

- 未使用 `phi + pi/2`。
- 未交换 `length1/length2`。

## 6. smoke 测试结果

执行：

```bash
cd /tmp/presence_phase2_smoke
qmake presence_phase2_smoke.pro
make -j8
./presence_phase2_smoke
```

关键结果：

| Case | success | ok | algorithm | count | 关键 payload |
|---|---:|---:|---|---:|---|
| Pattern.regression | true | true | scaled_shape_model | 1 | `scaleMinUsed=0.9`, `scaleMaxUsed=1.1`, `modelContourOverlayApplied=true` |
| Line.regression | true | true | metrology_line | 1 | `hitCount=40`, `samplePoints=40`, `lineBandApplied=true` |
| Edge horizontal/vertical any | true | true | measure_pos | 1 | `edgeCount=1`, `measureDirectionMode=along_line` |
| Edge horizontal/vertical black_to_white | true | true | measure_pos | 1 | `transitionUsed=positive` |
| Edge horizontal/vertical white_to_black | true | false | measure_pos | 0 | 方向不匹配，正常无目标 |
| Edge vertical/horizontal any | true | true | measure_pos | 1 | `phiDeg=-90` |
| Edge white_to_black | true | true | measure_pos | 1 | `transitionUsed=negative` |
| Edge no boundary existOk=true | true | false | measure_pos | 0 | `okNgReason=not found target, existOk=true` |
| Edge no boundary existOk=false | true | true | measure_pos | 0 | `okNgReason=not found target, existOk=false` |

## 7. 编译结果

执行：

```bash
qmake qt_ui_test.pro
make -j8
git diff --check
```

结果：

- `qmake qt_ui_test.pro` 通过。
- `make -j8` 通过，生成 `qt_ui_test`。
- `git diff --check` 通过，无空白错误。

## 8. 仍存在的问题

- Pattern 的 mask、sortMode、positionCorrection 仍未实现或未应用，仅在 payload 中标记。
- Line 的 mask、positionCorrection、manual edgeType 仍为内部 fallback。
- Edge 的 mask、positionCorrection 仍未实现。
- 本轮 smoke 未做真实 GUI 视觉验收；Overlay 可按下列步骤手动验收：加载静态图，创建 Pattern/Line/Edge，运行测试，检查模型轮廓、采样区域、拟合线、测量矩形和 edge point 是否显示。

## 9. 明确声明

- 未改 UI。
- 未改主流程。
- 未改 OCR。
- 未改 P6 ContourPresence。
- 未改变 `ToolCategory -> ToolType -> Dialog -> ToolConfig -> ToolEngine -> ToolAdapter -> Runner -> ToolResult -> Overlay` 主链路。
