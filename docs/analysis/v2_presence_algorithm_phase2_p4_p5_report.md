# V2 Presence Phase 2 P4-P5 Report

## 1. 本轮目标

阶段 2 仅优化：

- P4 BlobPresence：补连通域统计、OK/NG 解释和 overlay 表达。
- P5 CirclePresence：保留 region 圆检测路线，补 radius/circularity/count 解释、edgeType 选择说明和 overlay 文本。

本轮未改 UI，未改主流程，未改 OCR，未改 ContourPresence。

## 2. git status 摘要

本轮开始前工作区已有大量修改和未跟踪文件。本轮没有清理、回退或 reset。与 P4/P5 相关的实际修改集中在：

- `src/algorithms/presence/BlobPresenceHalconRunner.cpp`
- `src/algorithms/presence/CirclePresenceHalconRunner.cpp`
- `src/frame/FrameViewHelper.cpp`
- `/tmp/presence_phase2_smoke/main.cpp`
- `/tmp/presence_phase2_smoke/presence_phase2_smoke.pro`

未修改：

- `ui/BlobPresenceDialog.ui`
- `ui/CirclePresenceDialog.ui`
- `src/BlobPresenceDialog.cpp`
- `src/CirclePresenceDialog.cpp`
- MainWindow / totalwindow / 主流程
- OCR
- ContourPresence

## 3. Blob 修改摘要

算法链保持：

`ReduceDomain -> Threshold/invert threshold union -> Connection -> SelectShape(area) -> AreaCenter -> SmallestRectangle1`

保留参数映射：

- `grayMin/grayMax`
- `invertRange`
- `areaMin/areaMax`
- `existOk`
- `timeoutMs`
- 矩形、圆形、多边形 ROI

内部默认参数：

- `countMinUsed=1`
- `countMaxUsed=INT_MAX`
- `countRangeSource="internal_default"`
- `morphologyApplied=false`
- `positionCorrectionApplied=false`
- `maskOutputApplied=false`

新增 payload：

- `algorithm="blob_region"`
- `found`
- `blobCount`
- `areas`
- `centers`
- `boundingRects`
- `totalArea`
- `largestArea`
- `averageArea`
- `areaRate`
- `areaRateMode`
- `roiEffectiveArea`
- `largestBlobIndex`
- `grayMinUsed/grayMaxUsed`
- `invertRangeUsed`
- `areaMinUsed/areaMaxUsed`
- `countMinUsed/countMaxUsed/countRangeSource`
- `morphologyApplied`
- `maskOutputRequested/maskOutputApplied/maskOutputReason`
- `positionCorrectionApplied`
- `okNgReason`

`areaRate` 计算：

- 圆形/多边形 ROI 优先用 HALCON `area_center(roiRegion)`，`areaRateMode="roi_region_area"`。
- 矩形 ROI 使用检测 bbox 面积，`areaRateMode="bbox_fallback"`。

Overlay 变化：

- `detect_roi`
- `blob_bbox`
- `blob_center`
- `blob_area_text`
- `blob_count_text`

OK/NG：

- `found = blobCount >= 1 && blobCount <= INT_MAX`
- `ok = existOk ? found : !found`
- 无 blob 但算法正常：`success=true`
- 参数错误、空图、HALCON 异常：`success=false`

## 4. Circle 修改摘要

算法链保持 region 模式：

`BinaryThreshold/Threshold pass -> Connection -> SelectShape(area) -> SelectShape(circularity) -> SmallestCircle -> AreaCenter -> Circularity`

未启用卡尺圆：

- `mode="region"`
- `caliperModeAvailable=false`
- `caliperModeApplied=false`

保留参数映射：

- `sensitivity` -> threshold margin / edge threshold payload。
- `roundness` -> `circularityMinUsed`。
- `edgePolarity` -> light/dark/any threshold pass。
- `edgeType` -> strongest / maximum / minimum / manual fallback。
- `existOk`
- `timeoutMs`
- 矩形、圆形、多边形 ROI

内部默认参数：

- `countMinUsed=1`
- `countMaxUsed=INT_MAX`
- `radiusMinUsed=3.0`
- `radiusMaxUsed=min(roiW,roiH)/2`
- `areaMinUsed=pi*radiusMin^2`
- `areaMaxUsed=pi*radiusMax^2`
- `positionCorrectionApplied=false`
- `maskApplied=false`

新增 payload：

- `algorithm="circle_region"`
- `mode="region"`
- `found`
- `circleCount`
- `circles`
- `bestCircle`
- `center`
- `radius`
- `circularity`
- `area`
- `strength`
- `thresholdPasses`
- `thresholdLowUsed/thresholdHighUsed`
- `circularityMinUsed`
- `radiusMinUsed/radiusMaxUsed`
- `areaMinUsed/areaMaxUsed`
- `countMinUsed/countMaxUsed`
- `edgePolarityUsed`
- `edgeTypeUsed`
- `edgeTypeSelectionMetric`
- `manualUnsupported`
- `rejectedCandidates`
- `finalSelectedIndex`
- `caliperModeAvailable=false`
- `caliperModeApplied=false`
- `positionCorrectionApplied`
- `maskApplied`
- `okNgReason`

edgeType 选择：

- `strongest`：按 `strength` 排序并选择最佳圆，`edgeTypeSelectionMetric="strength"`。
- `maximum`：按半径最大选择，`edgeTypeSelectionMetric="radius_max"`。
- `minimum`：按半径最小选择，`edgeTypeSelectionMetric="radius_min"`。
- `manual`：无手动点选数据时 fallback 到 strongest，`manualUnsupported=true`。

Overlay 变化：

- `detect_roi`
- `circle`
- `circle_center`
- `circle_radius_text`
- `circularity_text`
- `circle_count_text`

OK/NG：

- `found = circleCount >= 1 && circleCount <= INT_MAX`
- `ok = existOk ? found : !found`
- 无圆但算法正常：`success=true`
- 参数错误、空图、HALCON 异常：`success=false`

## 5. smoke 测试设计和结果

目录：

- `/tmp/presence_phase2_smoke`

执行：

```bash
cd /tmp/presence_phase2_smoke
qmake presence_phase2_smoke.pro
make -j8
./presence_phase2_smoke
```

Blob 结果：

| Case | success | ok | algorithm | count | 关键字段 |
|---|---:|---:|---|---:|---|
| Blob.single | true | true | blob_region | 1 | `totalArea=1009`, `largestArea=1009`, `areaRate=0.0254798` |
| Blob.multi | true | true | blob_region | 2 | `areas=[441,660]`, `boundingRects` 2 个 |
| Blob.noise_filtered | true | false | blob_region | 0 | 小噪点被 `areaMin=50` 过滤 |
| Blob.invert | true | true | blob_region | 1 | `invertRange=true` 检出暗斑 |
| Blob.no_blob.not_exist_ok | true | true | blob_region | 0 | `existOk=false` 反转 OK |
| Blob.empty_image | false | false | blob_region | 0 | `okNgReason=input image is empty` |
| Blob.empty_roi | false | false | blob_region | 0 | `okNgReason=detect ROI is invalid` |

Circle 结果：

| Case | success | ok | algorithm | count | 关键字段 |
|---|---:|---:|---|---:|---|
| Circle.white | true | true | circle_region | 1 | `radius=35.5`, `circularity=1` |
| Circle.black | true | true | circle_region | 1 | dark threshold pass 可检出 |
| Circle.non_circle_high_roundness | true | false | circle_region | 0 | 高 roundness 下矩形被过滤 |
| Circle.no_circle.not_exist_ok | true | true | circle_region | 0 | `existOk=false` 反转 OK |
| Circle.edgeType.strongest | true | true | circle_region | 1 | `edgeTypeSelectionMetric=strength` |
| Circle.edgeType.maximum | true | true | circle_region | 1 | `edgeTypeSelectionMetric=radius_max` |
| Circle.edgeType.minimum | true | true | circle_region | 1 | `edgeTypeSelectionMetric=radius_min` |
| Circle.edgeType.manual | true | true | circle_region | 1 | `manualUnsupported=true` |

Overlay 未做 GUI 自动截图验证；手动 GUI 验证步骤：加载静态图，分别创建 BlobPresence/CirclePresence，运行测试，检查检测 ROI、bbox/center/area/count 文本、圆/圆心/radius/circularity/count 文本。

## 6. 编译结果

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

## 7. 仍未解决问题

- Blob 不输出真实 HALCON region overlay，只用 bbox、center、area 文本表达。
- Blob 未实现 mask 文件输出，payload 明确 `maskOutputApplied=false`。
- Blob 未启用 morphology，payload 明确 `morphologyApplied=false`。
- Circle 未启用卡尺圆，payload 明确 `caliperModeApplied=false`。
- Circle 未实现手动选择，payload 明确 `manualUnsupported=true`。
- positionCorrection 仍未实现。

## 8. 明确声明

- 未改 UI。
- 未改主流程。
- 未改 OCR。
- 未改 P6 ContourPresence。
- 未新增复杂 UI 控件。
- 未把算法逻辑写进 Dialog。
- 未改变 `ToolCategory -> ToolType -> Dialog -> ToolConfig -> ToolEngine -> ToolAdapter -> Runner -> ToolResult -> Overlay` 主链路。
