# PatternPresence polygon autoModelDomain debug 修复报告

## 1. 修改文件清单

- `src/algorithms/presence/PatternPresenceAutoModelDomain.h`
- `src/algorithms/presence/PatternPresenceAutoModelDomain.cpp`
- `src/algorithms/presence/PatternPresenceHalconRunner.cpp`
- `docs/analysis/pattern_presence_polygon_auto_domain_debug_fix_report.md`

未修改 `PatternPresenceHalconApi.h/.cpp`、ROI 交互、ToolEngine、MainWindow、其他 Presence 工具和 `.ui` 文件。

## 2. candidate_area_zero 根因

旧 autoModelDomain 只使用固定 `thresholdHigh=135`，并且缺少 threshold/connection/select_shape/area_center/ratio 各阶段的中间统计。某些 polygon ROI 下，连接域或面积读取已出现异常/不合理候选时，最终只落到粗粒度 `candidate_area_zero`，无法判断是 threshold 空、connection 空、select_shape 过滤空、候选面积为 0，还是 target ratio 被拒绝。

本次修复后，`candidate_area_zero` 只保留给所有阈值都没有正面积候选的情况；如果 connection 对象存在但 `area_center` 读不到有效面积，则输出 `candidate_area_unreadable`，避免把面积读取问题伪装成零候选。select_shape 为空会先回退到 connection 原始候选池继续选择，target ratio 过小/过大分别输出 `target_too_small` / `target_too_large_or_background`。

## 3. 调试字段

新增/输出的 autoModelDomain 字段：

- `thresholdTriedValues` / `autoModelDomainThresholdTriedValues`
- `selectedThresholdHigh` / `autoModelDomainSelectedThresholdHigh`
- `thresholdRegionArea`
- `connectedRegionCount`
- `rawCandidateCount`
- `areaCandidateCount`
- `selectedCandidateArea`
- `selectedCandidateIndex`
- `selectedCandidateAreaRatio`
- `failureStage`
- `failureReason`
- `perThresholdStats`

每个 threshold stat 记录：

- `thresholdHigh`
- `thresholdRegionArea`
- `connectedCount`
- `rawAreas`
- `areaCandidateCount`
- `selectedArea`
- `selectedAreaRatio`
- `rejectedReason`

`[PatternPolygon]` 日志已增加：

- `autoModelDomainCandidateCount`
- `autoModelDomainTargetArea`
- `autoModelDomainTargetAreaRatio`
- `selectedThresholdHigh`
- `failureStage`

## 4. 多阈值候选策略

当前内部尝试：

`[100, 115, 130, 145, 160, 175]`

选择规则：

- 先保留 targetAreaRatio 在合理范围内的候选。
- 多个候选合理时，选择 target area 最大且未超过上限的候选。
- `select_shape(area)` 没候选时，不再直接失败，回退到 connection 原始候选池。
- 所有 threshold 都没有正面积候选时，才输出 `candidate_area_zero`。

## 5. polygon ROI ratio 放宽

- polygon：`minRatio=0.005`，`rejectMaxAreaRatio=0.97`
- rectangle：`minRatio=0.005`，`rejectMaxAreaRatio=0.85`

polygon targetAreaRatio 在 `(0.85, 0.97)` 时允许 `autoModelDomainApplied=true`，并写入：

`autoModelDomainWarning="target area is close to polygon ROI area"`

## 6. 当前 polygon ROI 复测数据

复测使用用户提供的 polygon 点，构造模板外接矩形 `x=178,y=178,w=207,h=196`，本地可用图像使用 `projects/scheme_1/reference.png`。仓库和现有日志中未找到现场原图，因此该项只验证点位、ROI 面积、payload 和失败阶段语义。

- `polygonRegionArea`: `29085.165565431504`
- `thresholdCandidates`: `[100, 115, 130, 145, 160, 175]`
- `selectedThresholdHigh`: `175`
- `connectedCount`: `7`
- `areaCandidateCount`: `1`
- `targetArea`: `0`
- `targetAreaRatio`: `0`
- `autoModelDomainApplied`: `false`
- `fallbackReason`: `target_too_small`
- `failureStage`: `target_area`

结论：本地复测不再直接落到 `candidate_area_zero`，payload/debug 已能指出真实失败阶段。现场原图若如 HDevelop 验证能提取出合理 target 区域，应进入 `autoModelDomainApplied=true`，并输出 selected threshold、target area 和 target contour。

## 7. debug 图

`showContourPoints=true` 或 `debugPolygonLog=true` 时保存：

- `template_crop.png`
- `template_gray.png`
- `auto_domain_threshold_mask.png`
- `auto_domain_connected_candidates.png`
- `auto_domain_selected_candidate.png`
- `auto_model_domain_region.png`
- `auto_model_domain_contour_local.png`
- `debug_info.json`

失败时也会保存。首次 auto-domain 运行时，threshold/connected/selected 图由 HALCON region 轮廓生成，避免 OpenCV 复刻 threshold 与 HALCON reduced-domain 结果不一致。

## 8. Contrast 风险

本轮未调整 Contrast 映射，payload 写入：

- `createShapeModelContrastUsed=64`
- `createShapeModelMinContrastUsed=32`
- `autoDomainContrastOverrideApplied=false`

autoModelDomain 成功后，`create_shape_model` 仍使用当前 `templateSensitivity=2 -> 64/32`。HDevelop 验证更接近 `40/10`。如果现场 autoModelDomain 已成功但 `create_shape_model` 仍报 HALCON 8510，建议下一任务调整 `templateSensitivity` 映射，或仅在 auto-domain route 下使用更宽松的 `40/10` 或 `45/15`。

## 9. 不变项

- 是否修改 ROI：没有。
- 是否新增 UI：没有。
- 是否修改 `.ui`：没有。
- 是否恢复旧 contour filtering / level probing：没有。
- 是否修改 ToolEngine/MainWindow：没有。
- 是否改变 found / score / judgeBasis / existOk / scoreThreshold OK/NG 语义：没有。

## 10. 编译结果

执行：

```bash
cd "/home/hjl-ubuntu/桌面/qt_znxj_v2_work_1920_515"
qmake
make -j$(nproc)
```

结果：通过，`qt_ui_test` 链接成功。
