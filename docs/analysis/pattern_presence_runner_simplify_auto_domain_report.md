# PatternPresenceHalconRunner auto domain 简化报告

## 1. 修改文件清单

- `src/algorithms/presence/PatternPresenceHalconRunner.cpp`
- `src/algorithms/presence/PatternPresenceHalconApi.h`
- `src/algorithms/presence/PatternPresenceHalconApi.cpp`
- `src/algorithms/presence/PatternPresenceAutoModelDomain.h`
- `src/algorithms/presence/PatternPresenceAutoModelDomain.cpp`
- `qt_ui_test.pro`：只追加上述新增 cpp/h 到工程列表。
- `docs/analysis/pattern_presence_runner_simplify_auto_domain_report.md`

`PatternPresenceHalconRunner.h` 的 public runner/config/result 接口未新增字段、未改变调用方式。

## 2. 旧 get_shape_model_contours 过滤逻辑处理

已从 `PatternPresenceHalconRunner.cpp` 主路径删除/移除：

- contour level 1/2/3 自动探测。
- `ModelContourInfo`、`MajorContourSelection`、`chooseMajorContourIndexes`。
- short/tiny/outer boundary fragment 筛选。
- raw/filtered contour bbox、contour quality status、contourLevelStats。
- selected/relaxed contour coverage 过滤。
- `model_contours_level1/2/3_raw.png`、`model_contours_selected_filtered.png`、`match_contours_selected_global.png`。

`get_shape_model_contours` 不再作为主显示来源。当前只在 `PatternPresenceHalconApi.cpp` 中作为可选符号解析，用于极简 debug availability payload，不参与 Overlay。

## 3. Runner 行数

- 重构前：`PatternPresenceHalconRunner.cpp` 3704 行。
- 重构后：`PatternPresenceHalconRunner.cpp` 2332 行。
- 新增拆分文件合计：`PatternPresenceHalconApi.*` 304 行，`PatternPresenceAutoModelDomain.*` 321 行。

Runner 行数已下降，HALCON `dlopen/dlsym` 和 auto domain 提取已拆出。

## 4. ROI/UI/OK-NG 结论

- 是否修改 ROI 逻辑：没有。
- 是否新增 UI 参数：没有。
- 是否修改 `.ui`：没有。
- 是否修改 `ToolEngine` / `MainWindow` / 其他 Presence Runner：没有。
- 是否改变 `templateRoiNormalized` / `templateShapeType` / `templatePolygonNormalized` 含义：没有。
- 是否改变 `roiNormalized` / `detectRegionType` / `detectPolygonNormalized` 含义：没有。
- 是否改变 `found` / `score` / `judgeBasis` / `existOk` / `scoreThreshold` OK/NG 判定：没有。

## 5. 新建模链路

主建模链路现在是：

1. 按原逻辑从 `referenceImage` 裁剪 `templateMat`。
2. polygon 模板 ROI 仍然先 `gen_region_polygon + reduce_domain`。
3. 在当前模板 ROI/domain 内尝试 autoModelDomain。
4. autoModelDomain 成功时，用 `autoDomain.reducedImageForModel` 创建 shape model。
5. autoModelDomain 失败或 auto reduced image 创建模型失败时，fallback 到旧 `templateModelSource` 创建 shape model。
6. detect polygon ROI 仍然保留原 `gen_region_polygon + reduce_domain` 搜索逻辑。
7. `find_shape_model`、minScore、angle、metric、OK/NG 判定保持原语义。

shape model cache key 已加入 autoModelDomain 版本与固定参数，避免旧模型污染。

## 6. autoModelDomain 算子链

`PatternPresenceAutoModelDomain.cpp` 实现：

`templateModelSource -> threshold -> connection -> select_shape(area) -> area_center/选最大候选 -> fill_up -> opening_circle -> closing_circle -> area_center -> dilation_circle -> reduce_domain(graySource, modelDomain) -> gen_contour_region_xld(targetRegion, "border")`

面积筛选没有候选时，会退回连接域候选池并继续用最终 `targetAreaRatio <= 0.85` 防止贴边大背景。这是为了兼容较紧的黑色目标 ROI，不引入轮廓过滤玄学。

## 7. 内部固定参数

- `thresholdHigh = 135`
- `minAreaRatio = 0.01`
- `maxAreaRatio = 0.75`
- `rejectMinAreaRatio = 0.005`
- `rejectMaxAreaRatio = 0.85`
- `borderMargin = 5.0`（保留为内部参数，当前未做复杂边界过滤）
- `openingRadius = 1.5`
- `closingRadius = 4.5`
- `dilationRadius = 5.5`

这些参数未暴露到 UI。

## 8. fallback 机制

- optional HALCON 算子缺失：`autoModelDomainApplied=false`，`modelDomainSource="roi_reduce_domain_fallback"`。
- threshold/connection/select/area/morph/reduce/contour 任一步失败：fallback 到旧 ROI/domain 建模源。
- auto domain 成功但 `create_shape_model(autoReducedImage)` 失败：清理 auto 结果，再用旧 `templateModelSource` 重试。
- 为空白/弱对比样例增加了 `set_check("~give_error")` 动态解析调用，使 HALCON 8510 等错误按返回码进入现有错误处理，不让进程退出。

## 9. showContourPoints 新显示来源

`showContourPoints=true && autoModelDomainApplied=true && found=true` 时：

- Overlay 来源：`target_region_outer_contour`。
- payload：`contourSource="template_auto_model_domain_outer_contour"`。
- 坐标变换：以 auto domain 的 `area_center(TargetRegion)` 为源点，经 `vector_angle_to_rigid + affine_trans_contour_xld` 变换到 match 位置，再加 detect ROI 偏移输出 `contour_line`。

`showContourPoints=true && autoModelDomainApplied=false` 时：

- 不再强行显示 shape model 碎线。
- 只保留 match rect/center/score。
- payload：`contourSource="auto_model_domain_unavailable"`，`contourOverlayReason=<fallbackReason>`。

## 10. debug 图收敛

`showContourPoints=true` 时最多输出：

- `template_crop.png`
- `template_gray.png`
- `auto_model_domain_region.png`
- `auto_model_domain_contour_local.png`
- `match_auto_model_domain_contour_global.png`
- `debug_info.json`

`debug_info.json` 只写核心 payload 字段，不写每个 contour object 的长数组。

## 11. 本地验收测试

测试 A：黑色箭头网纹图  
样例：`projects/scheme_75381ddd/reference.png`  
ROI：`x=0.1215235792, y=0.2055622733, w=0.5795042322, h=0.6191051995`

- `autoModelDomainApplied=true`
- `modelDomainSource="auto_model_domain"`
- `targetAreaRatio=0.718328`
- `found=true`
- `score=0.795746`
- `showContourPointsApplied=true`
- `displayContourPointCount=1513`
- `contourLineSegmentCount=1512`

测试 B：HALCON green-dot 示例图  
样例：`/home/hjl-ubuntu/桌面/smart_camera_v2_test_images/pattern_presence/pattern_070_green_dot.png`

- `autoModelDomainApplied=true`
- `modelDomainSource="auto_model_domain"`
- `targetAreaRatio=0.164323`
- `found=true`
- `score=0.999927`
- `showContourPointsApplied=true`
- `displayContourPointCount=1141`
- `contourLineSegmentCount=1140`

测试 C：失败 fallback  
样例：200x200 全白 `CV_8UC3` 内存图

- `autoModelDomainApplied=false`
- `modelDomainSource="roi_reduce_domain_fallback"`
- `autoModelDomainFallbackReason="candidate_area_zero"`
- fallback 后 `create_shape_model` 返回 `template model creation failed`
- 进程未崩溃，Runner 正常返回失败结果。

测试 D：`showContourPoints=false`

- 使用测试 A 同图同 ROI。
- `autoModelDomainApplied=true`
- `found=true`
- `score=0.795746`
- `showContourPointsRequested=false`
- `showContourPointsApplied=false`
- `contourSource="not_requested"`
- 不生成 contour overlay；只保留 detect/match 基础 overlay。

## 12. 编译结果

执行：

```bash
cd "/home/hjl-ubuntu/桌面/qt_znxj_v2_work_1920_515"
qmake
make -j$(nproc)
```

结果：

- `qmake` 成功。
- `make -j$(nproc)` 成功；最终复跑输出 `make: 对“first”无需做任何事。`
