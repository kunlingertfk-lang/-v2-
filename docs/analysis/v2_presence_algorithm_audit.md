# V2 六类有无工具算法层审计报告

审计范围：`qt_znxj_v2_work_1920_515` 当前代码中的六类 Presence 工具，仅做只读审计和文档记录，未修改算法、UI、主流程或注册链路。

当前注册链路保持为：
`ToolCategory -> ToolType -> 参数 Dialog -> ToolConfig -> ToolEngine -> ToolAdapter -> Runner -> ToolResult -> Overlay`。

共用结构：

| 层 | 文件 | 当前承载方式 |
|---|---|---|
| ToolConfig | `src/toolcore/ToolConfig.h` | 固定字段 `toolId/toolName/toolType/category/enabled/roiNormalized/params/judgeRule/displayName/summary`，各工具参数主要写入 `params` 和 `judgeRule` |
| ToolEngine | `src/toolcore/ToolEngine.cpp` | 通过 `registerAdapter()` 注册 Adapter，`runTool()` 查找支持该 `ToolType` 的 Adapter |
| ToolResult | `src/toolcore/ToolResult.h` | `success` 表示执行成功，`ok` 表示检测判定 OK；输出 `score/value/count/text/status/message/elapsedMs/overlays/payload` |
| Overlay | `src/toolcore/ToolOverlay.h`、`src/frame/FrameViewHelper.cpp` | 支持 `Rect/Line/Circle/Polygon/Text`，按 label 着色和分层显示 |
| 主流程注册 | `src/MainWindow.cpp` | OCR、PatternPresence、BlobPresence、CirclePresence、ContourPresence、EdgePresence、LinePresence 均已注册到 `m_toolEngine` |

## 1. PatternPresence 图案有无

### 当前 UI 参数表

Dialog 文件：

- `ui/PatternPresenceDialog.ui`
- `src/PatternPresenceDialog.h`
- `src/PatternPresenceDialog.cpp`

| UI 参数/控件 | 语义 | 当前保存情况 |
|---|---|---|
| 基础/全部页签 | 切换简化参数和完整参数 | 影响 `configuration()` 读取哪些控件 |
| 模板区域：矩形 ROI、多边形 ROI、完成 | 建模模板区域 | 保存到 `params.templateRoiNormalized`、`params.templateShapeType`、`params.templatePolygonNormalized` |
| 模板屏蔽区域：编辑 | 模板 mask UI 入口 | 未进入 ToolConfig，Runner 也未应用 |
| 模板灵敏度：自动/手动、灵敏度 | 建模 contrast/minContrast 映射输入 | 保存到 `templateSensitivityMode/templateSensitivity`，Runner 用于内部 contrast 映射 |
| 检测区：自由绘制、矩形、圆形 | 搜索 ROI | 自由/圆形点击后提示未实现并回退矩形；矩形保存到 `roiNormalized`、`detectRegionType` |
| 屏蔽区域：编辑 | 检测 mask UI 入口 | 未进入 ToolConfig，Runner 未应用 |
| 独立位置修正使能、来源 | 位置修正 | 保存到 `enablePositionCorrection/positionCorrectionSource`，Runner 标记未应用 |
| 最小得分 | `find_shape_model` 的搜索最低分 | 保存到 `minScore`，单位为百分比 0-100，Adapter/Runner 转为 0-1 |
| 匹配极性 | HALCON metric 选择 | 保存到 `polarity`，Runner 映射到 `use_polarity/ignore_global_polarity` |
| 尺度范围 | 尺度搜索范围 | 保存到 `scaleMin/scaleMax`，百分比；Runner 当前只接受 100-100，其他值报 `unsupported_scale_range` |
| 角度范围 | 搜索角度范围 | 保存到 `angleStart/angleExtent/angleEnd`，单位度，Runner 转弧度 |
| 算法超时时间(ms) | 超时阈值 | 保存到 `timeoutMs`，Runner 仅写 `timeoutExceeded`，不主动中止 |
| 运行显示轮廓点 | 显示匹配轮廓 | 保存到 `showContourPoints`，Runner 仅在自动模型域 contour 可用时显示 region 外轮廓 |
| 排序方式 | 匹配排序 | 保存到 `sortMode`，Runner payload 记录但未实际排序 |
| 判断依据：结果有无/最低得分 | OK/NG 规则 | 保存到 `judgeBasis`、`judgeBasisText` |
| 存在OK/不存在OK | presence 判定极性 | 保存到 `existOk` |
| 最低得分 | 结果判定阈值 | 保存到 `scoreThreshold`，单位百分比 0-100 |
| 基准图测试、测试运行、完成 | 运行/保存入口 | 基准图和测试按钮会经 ToolEngine 跑当前 ToolConfig；完成返回 ToolConfig |

是否有 ROI：有，模板 ROI 与检测 ROI 分离；模板支持矩形/多边形，检测当前实际只闭环矩形。

是否有基准图/模板图：有，模板来源固定为 `referenceImage`。

是否有测试按钮：有，`referenceTestButton`、`testRunButton`。

是否有保存参数逻辑：有，`PatternPresenceDialog::toToolConfig()`。

### 当前 Config 字段表

| 字段 | 类型/默认值/单位 | 来源 |
|---|---|---|
| `roiNormalized` | `QRectF`，默认全图，归一化 | 检测 ROI |
| `params.templateRoiNormalized` | `QRectF`，默认全图，归一化 | 模板 ROI |
| `params.templateSource` | string，`referenceImage` | 内部固定 |
| `params.templateImagePath` | string，空 | 预留，UI 未暴露 |
| `params.modelPath` | string，空 | 预留，UI 未暴露 |
| `params.modelAutoCreate` | bool，true | 内部固定 |
| `params.modelCacheKey` | string，默认 `toolId_shape_model` | 内部生成 |
| `params.templateShapeType` | string，`rectangle/polygon` | 模板 ROI 按钮 |
| `params.templatePolygonNormalized` | point array，归一化 | 模板多边形 |
| `params.templateSensitivityMode` | string，`auto/manual` | 模板灵敏度 |
| `params.templateSensitivity` | int，默认 2，1-10 | 灵敏度 |
| `params.detectRegionType` | string，默认 `rectangle` | 检测 ROI 按钮 |
| `params.enablePositionCorrection` | bool，默认 true | 位置修正开关 |
| `params.positionCorrectionSource` | string | 来源下拉 |
| `params.minScore` | int，默认 50，百分比 | 最小得分 |
| `params.polarity` | string | 匹配极性下拉 |
| `params.scaleMin/scaleMax` | int，默认 100，百分比 | 尺度范围 |
| `params.angleStart/angleExtent/angleEnd` | int，默认 -45/90/45，度 | 角度范围 |
| `params.timeoutMs` | int，默认 2000，ms | 超时时间 |
| `params.showContourPoints` | bool，默认 false | 轮廓显示 |
| `params.debugPatternPolygonLog` | bool，false | 内部固定 |
| `params.sortMode` | string | 排序方式 |
| `params.judgeBasis` | string，`presence/score` | 判断依据 |
| `params.existOk` | bool，默认 true | 存在OK/不存在OK |
| `params.scoreThreshold` | int，默认 50，百分比 | 判定最低分 |
| `judgeRule.mode/existOk/scoreThreshold` | JSON | 同上 |

UI 显示但未真正进入 ToolConfig：

- 模板屏蔽区域、检测屏蔽区域。
- 检测自由绘制和圆形 ROI 是 UI 入口但点击后回退矩形，不形成有效非矩形 Config。

### 当前 Adapter 映射表

Adapter 文件：`src/tooladapters/PatternPresenceAdapter.cpp`

| ToolConfig 字段 | Runner 字段 | 映射/问题 |
|---|---|---|
| `roiNormalized` | `roiNormalized` | 原样传递 |
| `templateRoiNormalized` | `templateRoiNormalized` | 原样传递 |
| `templateShapeType/templatePolygonNormalized` | 同名 | 已传 |
| `detectRegionType` | `detectRegionType` | 已传；Adapter 还读取 `detectPolygonNormalized`，但 Dialog 当前不写该字段 |
| `templateSensitivity` | `templateSensitivity` | `qBound(1, value, 10)` |
| `minScore` | `minScore` | `qBound(0, value, 100)` |
| `polarity` | `polarity` | 字符串透传，Runner 再映射 HALCON metric |
| `scaleMin/scaleMax` | `scaleMin/scaleMax` | 百分比透传；Runner 不支持非 100 |
| `angleStart/angleExtent` | 同名 | 度，Runner 转弧度 |
| `timeoutMs` | `timeoutMs` | 仅用于 payload 超时标记 |
| `showContourPoints` | `showContourPoints` | 已传 |
| `sortMode` | `sortMode` | 已传但 Runner 未执行排序 |
| `judgeBasis/existOk/scoreThreshold` | 同名 | 已传；scoreThreshold 百分比 |
| `maxCount/expectedCount` | Runner 字段 | UI 未暴露，内部默认 1；可从 params/judgeRule 读 |

参数问题：

- UI 有尺度范围，但 Runner 会拒绝非 100-100。
- UI 有排序方式，但 Runner 未实际排序。
- 检测非矩形 ROI 的 Adapter/Runner 有部分字段通道，但 Dialog 未形成完整 Config。

### 当前 Runner 算法链

Runner 文件：`src/algorithms/presence/PatternPresenceHalconRunner.cpp`

当前算法：

1. 校验空图、空基准图、模板来源、模板 ROI、检测 ROI、图像类型、HALCON so。
2. 从 `referenceImage` 裁剪 `templateMat`，从当前图裁剪 `detectMat`。
3. 生成 HALCON 灰度图：`gen_image1/gen_image_interleaved -> rgb1_to_gray`。
4. 模板矩形直接建模；模板多边形走 `gen_region_polygon -> reduce_domain`。
5. 内部尝试自动模型域，使用阈值/连通域选目标区域，成功时用该 reduced domain 建模。
6. 建模使用 `create_shape_model`，不是 `create_scaled_shape_model`。
7. 搜索使用 `find_shape_model`，不是 `find_scaled_shape_model`。
8. 检测多边形如果有完整点集，可 `gen_region_polygon -> reduce_domain`，但当前 Dialog 不产生该点集。
9. 坐标从 ROI 局部坐标加 `detectRoiPixels.x/y` 映射回原图。
10. 勾选轮廓显示时，当前使用自动模型域 region 的 display contour，经 `vector_angle_to_rigid -> affine_trans_contour_xld` 变换到匹配位置；未使用 `get_shape_model_contours` 作为真实 shape model contour。

### 当前 OK/NG 判定

| 模式 | 当前逻辑 |
|---|---|
| 结果有无 | `found = matchCount > 0`，`ok = existOk ? found : !found` |
| 最低得分 | `ok = bestScore >= scoreThreshold/100.0` |
| 执行状态 | 参数/运行异常为 `success=false`；无匹配但算法正常为 `success=true, ok=false` 或按不存在OK为 `ok=true` |

风险：

- 若最低得分阈值设为 0，未匹配时 `bestScore=0` 也可能判 OK。
- `minScore` 是搜索过滤，`scoreThreshold` 是判定阈值，两个阈值同时存在，UI 上容易理解为一个阈值。

### 当前 Overlay 显示

| Overlay | 当前内容 | 真实性 |
|---|---|---|
| `detect_roi` | 检测 ROI 矩形/多边形 | 真实 ROI |
| `match_rect` | 以模板 ROI 宽高围绕最佳匹配中心画框 | 近似匹配框，不是模型轮廓 |
| `match_center` | 中心十字 | 真实匹配中心 |
| `match_score` | 最佳得分文本 | 真实分数 |
| `contour_line` | 自动模型域 region contour 变换后的折线 | 是自动域目标区域轮廓，不是 `get_shape_model_contours` 的真实 shape model contour |

### 问题清单

- 未使用 `CreateScaledShapeModel/FindScaledShapeModel`，尺度 UI 变更后直接报不支持。
- 未使用 `GetShapeModelContours`，轮廓显示来源不是 shape model contour。
- `showContourPoints` 依赖自动模型域成功；自动域不可用时不显示模型轮廓。
- 检测 ROI 的自由/圆形 UI 是提示回退，不是完整闭环。
- 模板/检测屏蔽区域 UI 没有 Config 和 Runner 闭环。
- `sortMode` 保存但 Runner 未用。
- 缺少显式 `okNgReason` 字段；只能从 `status/message/payload` 推断。

### 算法优化建议

- P1 优先改 Runner：使用 `create_scaled_shape_model/find_scaled_shape_model`，内部默认 `scaleMin=0.9`、`scaleMax=1.1`；若继续读取 UI 百分比则转为 0.01 倍。
- 建模和显示分离：模板 ROI 用于建模，搜索 ROI 用于搜索。
- Overlay 用 `get_shape_model_contours -> vector_angle_to_rigid -> affine_trans_contour_xld` 显示真实模型轮廓。
- ToolResult payload 增加 `score/angle/scale/matchCount/okNgReason`。
- UI 未暴露的 `numLevels/minContrast/greediness` 使用内部默认并写入 payload。

是否需要改 UI：否。当前 UI 参数足以先完成真实模型匹配和轮廓显示；未暴露参数使用内部默认。

## 2. BlobPresence 斑点有无

### 当前 UI 参数表

Dialog 文件：

- `ui/BlobPresenceDialog.ui`
- `src/BlobPresenceDialog.h`
- `src/BlobPresenceDialog.cpp`

| UI 参数/控件 | 语义 | 当前保存情况 |
|---|---|---|
| 基础/全部页签 | 简化/完整参数 | 影响读取控件 |
| 检测区：自由绘制、矩形、圆形、多边形、重置 | 检测 ROI | 矩形、圆形、多边形保存；自由绘制提示未实现；重置为全图 |
| 独立位置修正使能、来源 | 位置修正 | 保存但 Runner 标记未实现 |
| 灰度阈值最小/最大 | 灰度范围 | 保存到 `grayMin/grayMax` |
| 反选范围 | 检测灰度范围之外 | 全部页保存到 `invertRange` |
| 面积最小/最大 | 连通域面积过滤 | 全部页保存到 `areaMin/areaMax`；基础页内部默认 10-999999 |
| 掩膜输出 | 输出 mask | 保存到 `maskOutputEnabled`，Runner 标记未写出 |
| 存在OK/不存在OK | presence 判定极性 | 保存到 `existOk` |
| 基准图测试、测试运行、完成 | 运行/保存入口 | 已有 |

是否有 ROI：有，矩形/圆形/多边形闭环；自由绘制未实现。

是否有基准图/模板图：有基准图测试入口，但算法不使用模板图。

是否有测试按钮：有。

是否有保存参数逻辑：有，`BlobPresenceDialog::toToolConfig()`。

### 当前 Config 字段表

| 字段 | 类型/默认值/单位 |
|---|---|
| `roiNormalized` | `QRectF`，默认全图，归一化 |
| `params.detectRegionType` | string，`rect/polygon/circle` |
| `params.detectPolygonNormalized` | point array，归一化 |
| `params.detectCircleNormalized` | `{center,radius,boundingRect,valid}`，归一化 |
| `params.enablePositionCorrection` | bool，默认 false |
| `params.positionCorrectionSource` | string |
| `params.grayMin/grayMax` | int，默认 0/255，灰度级 |
| `params.invertRange` | bool，默认 false |
| `params.areaMin/areaMax` | int，默认 10/999999，像素面积 |
| `params.maskOutputEnabled` | bool，默认 false |
| `params.judgeBasis` | string，固定 `presence` |
| `params.existOk` | bool，默认 true |
| `params.timeoutMs` | int，默认 1000，ms |
| `judgeRule.mode/existOk` | JSON，presence 判定 |

UI 显示但未真正进入 Runner 效果：

- 位置修正保存但未应用。
- 掩膜输出保存但未写 mask。
- 自由绘制 ROI 没有算法闭环。

### 当前 Adapter 映射表

Adapter 文件：`src/tooladapters/BlobPresenceAdapter.cpp`

| ToolConfig 字段 | Runner 字段 | 映射/问题 |
|---|---|---|
| `roiNormalized` | `roiNormalized` | 有效时传递 |
| `detectRegionType` | `detectRegionType` | 已传 |
| `detectPolygonNormalized` | 同名 | 已传 |
| `detectCircleNormalized` | center/radius/boundingRect | 已拆分传入 |
| `grayMin/grayMax` | 同名 | clamp 到 0-255 |
| `invertRange` | `invertRange` | 已传 |
| `areaMin/areaMax` | 同名 | 非负 clamp |
| `maskOutputEnabled` | 同名 | 已传但 Runner 不写出 |
| `judgeBasis/existOk` | 同名 | 已传 |
| `timeoutMs` | `timeoutMs` | 已传，仅超时标记 |

### 当前 Runner 算法链

Runner 文件：`src/algorithms/presence/BlobPresenceHalconRunner.cpp`

当前算法：

1. 校验空图、ROI、灰度范围、面积范围、图像类型、HALCON so。
2. 裁剪 `detectMat`，生成 HALCON 灰度图。
3. 圆/多边形 ROI 使用 `gen_circle/gen_region_polygon -> reduce_domain`。
4. 阈值：
   - 正常范围：`threshold(grayMin, grayMax)`。
   - 反选范围：分别 threshold 低段和高段，再 `union2`。
5. 连通域：`connection`。
6. 面积过滤：`select_shape(..., "area", areaMin, areaMax)`。
7. 统计：`count_obj`，`smallest_rectangle1`，`area_center`。
8. 坐标以 ROI 局部结果加 detect ROI 偏移映射回原图。

### 当前 OK/NG 判定

| 规则 | 当前逻辑 |
|---|---|
| 检测有无 | `found = blobCount > 0` |
| OK/NG | `ok = existOk ? found : !found` |
| 执行状态 | 空图/参数错误/HALCON 错误为 `success=false`，无斑点为 `success=true, ok=false` 或按不存在OK为 OK |

### 当前 Overlay 显示

| Overlay | 当前内容 | 真实性 |
|---|---|---|
| `detect_roi` | 矩形/圆/多边形 ROI | 真实 ROI |
| `blob` | 每个 selected region 的外接矩形 | 真实候选的 bbox，不是真实 region |
| `blob_count` | count 文本 | 真实数量 |

ToolResult 主要字段：

- `score=1/0`
- `value=count`
- `count=blobCount`
- `payload.found/blobCount/blobRects/blobAreas`

缺失：

- centers 未写入 payload。
- totalArea/largestArea/areaRate 未写入。
- Overlay 不显示真实 region，只显示 bbox。
- 缺少 `okNgReason`。

### 问题清单

- 没有 Opening/Closing 等形态学去噪。
- 只按 `count > 0` 判定，没有 count 范围、总面积、最大面积、面积占比判定。
- `area_center` 取了中心 tuple，但没有输出中心点 Overlay 和 payload。
- `maskOutputEnabled` 保存但无输出。
- 位置修正保存但未应用。

### 算法优化建议

- 在 Runner 内补完整连通域输出：`areas/centers/totalArea/largestArea/areaRate/okNgReason`。
- Overlay 显示真实 region 需要现有 `ToolOverlay` 扩展或折线/多边形近似；不改 UI 的前提下先输出中心点、bbox、面积文本，并在 payload 保留 region 统计。
- 可内部默认形态学参数：默认不开启或轻量 opening/closing，UI 未暴露，内部默认。

是否需要改 UI：否。已有灰度和面积参数足够先优化 Runner 输出和判定解释。

## 3. CirclePresence 圆有无

### 当前 UI 参数表

Dialog 文件：

- `ui/CirclePresenceDialog.ui`
- `src/CirclePresenceDialog.h`
- `src/CirclePresenceDialog.cpp`

| UI 参数/控件 | 语义 | 当前保存情况 |
|---|---|---|
| 基础/全部页签 | 简化/完整参数 | 影响读取控件 |
| 检测区：矩形、圆形、多边形、重置 | 检测 ROI | 保存到 `detectRegionType`、`detectCircleNormalized`、`detectPolygonNormalized`、`roiNormalized` |
| 屏蔽区域 | mask | UI 有入口，未进入 ToolConfig |
| 独立位置修正、来源 | 位置修正 | 保存但 Runner 未应用 |
| 灵敏度 | 阈值 margin/边缘阈值内部映射 | 保存到 `sensitivity`，0-100 |
| 圆度 | circularity 阈值输入 | 全部页保存到 `roundness`，0-100；基础页默认 25 |
| 边缘极性 | 黑到白/白到黑/任意 | 全部页保存到 `edgePolarity`；基础页默认 `any` |
| 边缘类型 | 最强/最大/最小/手动选择 | 全部页保存到 `edgeType`；手动选择 Runner fallback |
| 存在OK/不存在OK | presence 判定极性 | 保存到 `existOk` |
| 基准图测试、测试运行、完成 | 运行/保存入口 | 已有 |

是否有 ROI：有，矩形/圆形/多边形闭环。

是否有基准图/模板图：有基准图测试入口，但圆算法不使用模板图。

是否有测试按钮：有。

是否有保存参数逻辑：有，`CirclePresenceDialog::toToolConfig()`。

### 当前 Config 字段表

| 字段 | 类型/默认值/单位 |
|---|---|
| `roiNormalized` | `QRectF`，默认全图，归一化 |
| `params.detectRegionType` | string，`rect/polygon/circle` |
| `params.detectPolygonNormalized` | point array，归一化 |
| `params.detectCircleNormalized` | `{center,radius,boundingRect,valid}`，归一化 |
| `params.enablePositionCorrection` | bool，默认 false |
| `params.positionCorrectionSource` | string |
| `params.sensitivity` | int，默认 60，0-100 |
| `params.roundness` | int，默认 25，0-100 |
| `params.edgePolarity` | string，默认 `any` |
| `params.edgeType` | string，默认 `strongest` |
| `params.judgeBasis` | string，固定 `presence` |
| `params.existOk` | bool，默认 true |
| `params.timeoutMs` | int，默认 1000，ms |
| `judgeRule.mode/existOk` | JSON |

UI 显示但未真正进入 Runner 效果：

- 屏蔽区域未保存。
- 位置修正保存但 Runner 未应用。
- 边缘类型“手动选择”保存后 Runner fallback 到 strongest。

### 当前 Adapter 映射表

Adapter 文件：`src/tooladapters/CirclePresenceAdapter.cpp`

| ToolConfig 字段 | Runner 字段 | 映射/问题 |
|---|---|---|
| `roiNormalized` | `roiNormalized` | 有效时传递 |
| `detectRegionType` | `detectRegionType` | 已传 |
| `detectPolygonNormalized` | 同名 | 已传 |
| `detectCircleNormalized` | center/radius/boundingRect | 已拆分传入 |
| `sensitivity` | `sensitivity` | clamp 到 0-100 |
| `roundness` | `roundness` | clamp 到 0-100 |
| `edgePolarity` | `edgePolarity` | 透传，Runner 归一化 |
| `edgeType` | `edgeType` | 透传，Runner 归一化 |
| `existOk` | `existOk` | 已传 |
| `timeoutMs` | `timeoutMs` | 已传，仅超时标记 |
| `enablePositionCorrection/source` | 同名 | 已传，Runner 不应用 |

### 当前 Runner 算法链

Runner 文件：`src/algorithms/presence/CirclePresenceHalconRunner.cpp`

当前算法：

1. 校验空图、ROI、图像类型、HALCON so。
2. 裁剪检测 ROI 并生成灰度图。
3. 圆/多边形 ROI 使用 `gen_circle/gen_region_polygon -> reduce_domain`。
4. 根据 `edgePolarity` 做一到两次 threshold pass：
   - `binary_threshold(..., max_separability, light/dark)`。
   - 根据灵敏度得到 threshold margin，再 `threshold`。
5. `connection`。
6. `select_shape(area)`，面积范围由内部半径默认推导。
7. `select_shape(circularity)`，阈值由 `roundness` 和 `sensitivity` 推导。
8. `smallest_circle`、`area_center`、`circularity`。
9. 按 `edgeType` 选择最大/最小/最强；手动选择 fallback。
10. 坐标从 ROI 局部映射回原图。

### 当前 OK/NG 判定

| 规则 | 当前逻辑 |
|---|---|
| 检测有无 | `found = finalCircleCount > 0` |
| OK/NG | `ok = existOk ? found : !found` |
| 其他 | 没有半径范围、count 范围、fitError/hitRatio 判定 |

### 当前 Overlay 显示

| Overlay | 当前内容 | 真实性 |
|---|---|---|
| `detect_roi` | 矩形/圆/多边形 ROI | 真实 ROI |
| `circle` | `smallest_circle` 结果圆 | 真实区域拟合圆 |
| `circle_center` | 中心十字 | 真实圆心 |
| `circle_count` | 数量文本 | 真实数量 |

ToolResult 主要字段：

- `score` 为 strength，综合 circularity/area/contrast。
- `value=count`
- `count=finalCircleCount`
- `payload.rawCircles/circles/thresholdPasses/thresholdLowUsed/thresholdHighUsed/circularityMinUsed/radiusMinUsed/radiusMaxUsed`

缺失：

- 没有 `edgePointCount/hitRatio/fitError`，当前不是卡尺圆。
- 没有 radius UI 参数，半径阈值是内部默认。
- 没有显式 `okNgReason`。

### 问题清单

- 当前是区域圆，不是 MeasurePos/卡尺圆。
- 半径阈值、面积阈值均内部默认，UI 未暴露。
- `edgeType=manual` 无 UI 点选数据，只能 fallback。
- 屏蔽区域和位置修正未实现。

### 算法优化建议

- 保持 UI 不变，先强化区域圆：输出 `center/radius/circularity/count/okNgReason`，判定增加内部默认 count>=1、radius 合理、circularity 达标。
- 可预留卡尺模式字段但默认 region；UI 未暴露 mode，内部默认 region。
- 后续若启用卡尺，内部默认 `sampleCount/sigma/threshold/hitRatio/maxFitError`，并输出采样点和拟合圆。

是否需要改 UI：否。当前 UI 足够支持区域圆优化；卡尺参数先内部默认。

## 4. EdgePresence 边缘有无

### 当前 UI 参数表

Dialog 文件：

- `ui/EdgePresenceDialog.ui`
- `src/EdgePresenceDialog.h`
- `src/EdgePresenceDialog.cpp`

| UI 参数/控件 | 语义 | 当前保存情况 |
|---|---|---|
| 基础/全部页签 | 简化/完整参数 | 影响读取控件 |
| 检测区：线型 ROI | 线型搜索带 | 保存到 `detectRegionType=line_band`、`searchLineP1/searchLineP2/searchBandWidth`，`roiNormalized` 保存线带外接矩形 |
| 屏蔽区域 | mask | UI 有入口但 Edge Dialog 当前未写 mask 参数 |
| 独立位置修正、来源 | 位置修正 | 保存但 Runner 不应用 |
| 灵敏度 | Canny 高/低阈值内部映射 | 保存到 `sensitivity`，0-100 |
| 边缘极性 | 黑到白/白到黑/任意 | 全部页保存到 `edgePolarity` |
| 存在OK/不存在OK | presence 判定极性 | 保存到 `existOk` |
| 基准图测试、测试运行、完成 | 运行/保存入口 | 已有 |

是否有 ROI：有，UI 是线型 ROI。

是否有基准图/模板图：有基准图测试入口，但算法不使用模板图。

是否有测试按钮：有。

是否有保存参数逻辑：有，`EdgePresenceDialog::toToolConfig()`。

### 当前 Config 字段表

| 字段 | 类型/默认值/单位 |
|---|---|
| `roiNormalized` | `QRectF`，默认线带外接矩形，归一化 |
| `params.detectRegionType` | string，固定 `line_band` |
| `params.searchLineP1/searchLineP2` | point，默认 `(0.15,0.5)/(0.85,0.5)`，归一化 |
| `params.searchBandWidth` | double，默认 0.08，归一化最大尺寸 |
| `params.lineBandWidthUnit` | string，`normalized_max_dimension` |
| `params.enablePositionCorrection` | bool，默认 false |
| `params.positionCorrectionSource` | string |
| `params.sensitivity` | int，默认 60，0-100 |
| `params.edgePolarity` | string，默认 `any` |
| `params.judgeBasis` | string，固定 `presence` |
| `params.existOk` | bool，默认 true |
| `judgeRule.mode/existOk` | JSON |

UI 显示但未真正进入 Runner 效果：

- 线型 ROI 保存了，但 Runner 实际 fallback 到外接矩形。
- 边缘极性保存了，但当前 Canny/XLD 算法未按灰度跳变方向过滤。
- 位置修正和 mask 未应用。

### 当前 Adapter 映射表

Adapter 文件：`src/tooladapters/EdgePresenceAdapter.cpp`

| ToolConfig 字段 | Runner 字段 | 映射/问题 |
|---|---|---|
| `roiNormalized` | `roiNormalized` | 有效时传递 |
| `detectRegionType` | `detectRegionType` | 已传 |
| `searchLineP1/P2/searchBandWidth` | 同名 | 已传，宽度 clamp 到 0.001-1.0 |
| `sensitivity` | `sensitivity` | clamp 到 0-100 |
| `edgePolarity` | `edgePolarity` | 归一化后传入，但 Runner 未用于方向判定 |
| `existOk` | `existOk` | 已传 |
| `timeoutMs` | `timeoutMsInternalDefault` | UI 未暴露，内部固定 1000ms |
| `halconSoPath` | `halconSoPath` | Adapter 未读取 params 中的 `halconSoPath`，直接走默认解析 |

### 当前 Runner 算法链

Runner 文件：`src/algorithms/presence/EdgePresenceHalconRunner.cpp`

当前算法：

1. 校验空图、ROI、detectRegionType、图像类型、HALCON so。
2. `detectRegionType=line_band` 被接受，但实际 `roiMode=line_band_bounding_rect`，`lineBandApplied=false`。
3. 裁剪线带外接矩形。
4. 生成灰度图。
5. `edges_sub_pix(..., "canny", sigma=1.0, low, high)`，阈值由灵敏度映射。
6. `count_obj`、`length_xld`、`smallest_rectangle1_xld`、`get_contour_xld`。
7. 统计 edgeCount、edgeLength、contour sample。
8. 坐标从 ROI 局部映射回原图。

### 当前 OK/NG 判定

| 规则 | 当前逻辑 |
|---|---|
| 检测有无 | `found = edgeCount > 0 && edgeLength >= minEdgeLengthUsed` |
| OK/NG | `ok = existOk ? found : !found` |
| 阈值 | `minEdgeLengthUsed` 为内部默认，UI 未暴露 |

### 当前 Overlay 显示

| Overlay | 当前内容 | 真实性 |
|---|---|---|
| `detect_roi` | 线带外接矩形 | 真实外接矩形，但不是实际测量带 mask |
| `line_band_center`/`line_band_boundary` | UI 线带几何 | 真实用户线带，但算法未按该带 reduce |
| `edge_contour` | XLD contour 折线采样 | 真实 Canny XLD |
| `edge_count` | `edges/length` 文本 | 真实统计 |

ToolResult 主要字段：

- `score=edgeLength/minEdgeLengthUsed` clamp 到 1。
- `value=edgeLength`
- `count=edgeCount`
- `payload.edgeCount/edgeLength/edgeLengths/edgeLineSamples/contourPointCount/rawEdgeContours`

缺失：

- 没有 MeasurePos edge points、amplitudes、distances。
- 没有 polarity/select/countMin/countMax。
- 缺少显式 `okNgReason`。

### 问题清单

- 不是工业边缘有无推荐的 MeasurePos 灰度跳变检测。
- 线型 ROI 只画出来并保存，Runner 对算法区域 fallback 到外接矩形。
- 边缘极性保存但没有用于方向过滤。
- 结果是 Canny/XLD 长度语义，和“某条测量线/测量带上是否存在边缘点”的工业语义不一致。

### 算法优化建议

- P3 改 Runner 为 `GenMeasureRectangle2 -> MeasurePos`。
- 使用现有线型 ROI 生成测量矩形；`sensitivity` 映射 `threshold/amplitude`。
- UI 未暴露参数内部默认：`sigma=1.0`、`polarity=all`、`select=all/strongest`、`countMin=1`、`countMax=INT_MAX`。
- ToolResult 增加 `edgePoints/amplitudes/distances/count/okNgReason`，Overlay 显示测量矩形、边缘点短标记、数量和幅值文本。

是否需要改 UI：否。UI 已有线型 ROI、灵敏度、极性，MeasurePos 其余参数可内部默认。

## 5. LinePresence 直线有无

### 当前 UI 参数表

Dialog 文件：

- `ui/LinePresenceDialog.ui`
- `src/LinePresenceDialog.h`
- `src/LinePresenceDialog.cpp`

| UI 参数/控件 | 语义 | 当前保存情况 |
|---|---|---|
| 基础/全部页签 | 简化/完整参数 | 影响读取控件 |
| 检测区：线型 ROI | 线型搜索带 | 保存到 `detectRegionType=line_band`、`searchLineP1/searchLineP2/searchBandWidth`，`roiNormalized` 为外接矩形 |
| 屏蔽区域 | mask | UI 有入口但当前未写 mask 参数 |
| 独立位置修正、来源 | 位置修正 | 保存但 Runner 不应用 |
| 灵敏度 | Canny 高/低阈值内部映射 | 保存到 `sensitivity`，0-100 |
| 直线度 | straightness/fitError 内部阈值 | 全部页保存到 `lineDegree`，0-100；基础页默认 25 |
| 边缘极性 | 黑到白/白到黑/任意 | 保存到 `edgePolarity`，Runner 未用于方向判定 |
| 边缘类型 | 最强/第一条/最后一条/手动选择 | 保存到 `edgeType`，用于候选线选择；手动 fallback |
| 存在OK/不存在OK | presence 判定极性 | 保存到 `existOk` |
| 基准图测试、测试运行、完成 | 运行/保存入口 | 已有 |

是否有 ROI：有，线型 ROI。

是否有基准图/模板图：有基准图测试入口，但算法不使用模板图。

是否有测试按钮：有。

是否有保存参数逻辑：有，`LinePresenceDialog::toToolConfig()`。

### 当前 Config 字段表

| 字段 | 类型/默认值/单位 |
|---|---|
| `roiNormalized` | `QRectF`，线带外接矩形，归一化 |
| `params.detectRegionType` | string，固定 `line_band` |
| `params.searchLineP1/searchLineP2` | point，默认 `(0.15,0.5)/(0.85,0.5)`，归一化 |
| `params.searchBandWidth` | double，默认 0.08，归一化最大尺寸 |
| `params.lineBandWidthUnit` | string，`normalized_max_dimension` |
| `params.enablePositionCorrection` | bool，默认 false |
| `params.positionCorrectionSource` | string |
| `params.sensitivity` | int，默认 60，0-100 |
| `params.lineDegree` | int，默认 25，0-100 |
| `params.edgePolarity` | string，默认 `any` |
| `params.edgeType` | string，默认 `strongest` |
| `params.judgeBasis` | string，固定 `presence` |
| `params.existOk` | bool，默认 true |
| `judgeRule.mode/existOk` | JSON |

UI 显示但未真正进入 Runner 效果：

- 线型 ROI 保存了，但 Runner 实际使用外接矩形。
- 边缘极性保存但未参与灰度跳变方向判定。
- mask、位置修正未应用。

### 当前 Adapter 映射表

Adapter 文件：`src/tooladapters/LinePresenceAdapter.cpp`

| ToolConfig 字段 | Runner 字段 | 映射/问题 |
|---|---|---|
| `roiNormalized` | `roiNormalized` | 有效时传递 |
| `detectRegionType` | `detectRegionType` | 已传 |
| `searchLineP1/P2/searchBandWidth` | 同名 | 已传，宽度 clamp 到 0.001-1.0 |
| `sensitivity` | `sensitivity` | clamp 到 0-100 |
| `lineDegree` | `lineDegree` | clamp 到 0-100 |
| `edgePolarity` | `edgePolarity` | 已归一化但 Runner 未用于边缘方向 |
| `edgeType` | `edgeType` | 归一化为 strongest/first/last/manual |
| `existOk` | `existOk` | 已传 |
| `timeoutMs` | `timeoutMsInternalDefault` | UI 未暴露，内部固定 1000ms |
| `halconSoPath` | `halconSoPath` | Adapter 未读取 params 中的 `halconSoPath`，直接走默认解析 |

### 当前 Runner 算法链

Runner 文件：`src/algorithms/presence/LinePresenceHalconRunner.cpp`

当前算法：

1. 校验空图、ROI、detectRegionType、图像类型、HALCON so。
2. `detectRegionType=line_band` 仅标记，实际 `roiMode=line_band_bounding_rect`、`lineBandApplied=false`。
3. 裁剪线带外接矩形，生成灰度图。
4. `edges_sub_pix(..., "canny")`。
5. `segment_contours_xld(..., "lines")`。
6. 对每段：
   - `length_xld`
   - `get_contour_xld`
   - `fit_line_contour_xld`
   - 计算 length、straightness、fitError、score。
7. 按内部 `minLineLength/straightness/maxFitError` 过滤。
8. 按 edgeType 选择 strongest/first/last。
9. 坐标映射回原图。

### 当前 OK/NG 判定

| 规则 | 当前逻辑 |
|---|---|
| 检测有无 | `found = selectedLines.size() > 0` |
| OK/NG | `ok = existOk ? found : !found` |
| 其他 | 长度、直线度、fitError 是内部过滤条件，不是 UI 独立判定项 |

### 当前 Overlay 显示

| Overlay | 当前内容 | 真实性 |
|---|---|---|
| `detect_roi` | 线带外接矩形 | 真实外接矩形，但算法未按线带 mask |
| `line_band_center`/`line_band_boundary` | 用户线带 | 真实用户线带，但不参与 reduce |
| `line` | 拟合线段 | 真实 XLD 分割后拟合线 |
| `line_count` | 数量/长度文本 | 真实统计 |

ToolResult 主要字段：

- `score=bestLineScore/roiDiagonal`
- `value=totalLineLength`
- `count=lineCount`
- `payload.lines/rawLineCandidates/rejectedLines/totalLineLength/bestLineLength/bestLineScoreNormalized`

缺失：

- 不是 HALCON Metrology Line。
- 没有采样矩形、采样点、hitCount、metrology score。
- 没有 angle range/length range UI 映射。
- 缺少显式 `okNgReason`。

### 问题清单

- 当前是 Canny/XLD + segment + fit line，不是推荐的 Metrology Line。
- 线型 ROI 未真正约束算法区域。
- `edgePolarity` 没有参与边缘方向。
- `lineDegree` 影响内部过滤，但 OK/NG reason 不明确。

### 算法优化建议

- P2 改 Runner 为 HALCON Metrology Line：
  `CreateMetrologyModel -> AddMetrologyObjectLineMeasure -> SetMetrologyObjectParam -> ApplyMetrologyModel -> GetMetrologyObjectResult -> GetMetrologyObjectMeasures`。
- 现有线型 ROI 映射为 line metrology 对象和 measure length/width。
- UI 未暴露参数内部默认：`sampleCount`、`minScore`、`maxFitError`、`minHitCount`、angle/length range。
- ToolResult 增加 `startPoint/endPoint/midpoint/angle/length/score/fitError/hitCount/samplePoints/okNgReason`。

是否需要改 UI：否。当前线型 ROI、灵敏度、直线度、极性、选择方式足够先映射到 Metrology Line。

## 6. ContourPresence 轮廓有无

### 当前 UI 参数表

Dialog 文件：

- `ui/ContourPresenceDialog.ui`
- `src/ContourPresenceDialog.h`
- `src/ContourPresenceDialog.cpp`

| UI 参数/控件 | 语义 | 当前保存情况 |
|---|---|---|
| 基础/全部页签 | 简化/完整参数 | 影响读取控件 |
| 模板区域：矩形、多边形、完成 | 轮廓模板区域 | 保存到 `templateRoiNormalized/templateShapeType/templatePolygonNormalized` |
| 模板屏蔽区域 | mask | UI 入口，未进入有效 Runner 逻辑 |
| 尺度模式、速度尺度、特征尺度 | 轮廓/模板尺度相关 | 保存到 `scaleMode/speedScale/featureScale`，Runner payload 标记未应用 |
| 阈值模式、灰度阈值、阈值类型 | 轮廓提取阈值 | 保存到 `thresholdMode/grayThreshold/thresholdType`，Runner payload 标记未应用 |
| 链长模式、最小链长 | 轮廓链过滤 | 保存到 `chainMode/minChainLength`，Runner payload 标记未应用 |
| 检测区：自由绘制、矩形、圆形、多边形 | 搜索 ROI | 矩形/多边形保存；自由/圆形提示未实现或不支持 |
| 屏蔽区域 | 检测 mask | UI 入口，未进入有效 Runner 逻辑 |
| 独立位置修正、来源 | 位置修正 | 保存但未应用 |
| 最小得分 | 模板匹配最小分 | 保存到 `minScore`，UI 百分比转 0-1 |
| 匹配极性 | HALCON metric | 保存到 `polarity` |
| 尺度范围 | 尺度搜索 | 保存到 `scaleMin/scaleMax`，Runner 强制使用 100/100 |
| 角度范围 | 搜索角度 | 保存到 `angleStart/angleExtent` |
| 算法超时时间(ms) | 超时阈值 | 保存到 `timeoutMs`，只标记 |
| 运行显示轮廓点 | 显示轮廓点 | 保存到 `showContourPoints`，Runner 提取模板 patch Canny 轮廓并仿射到匹配位置 |
| 排序方式 | 匹配排序 | 保存但未应用 |
| 判断依据/存在OK/最低得分 | OK/NG 规则 | 保存到 `judgeBasis/existOk/scoreThreshold` |
| 基准图测试、测试运行、完成 | 运行/保存入口 | 已有 |

是否有 ROI：有，模板矩形/多边形，检测矩形/多边形；自由和圆形未完整闭环。

是否有基准图/模板图：有，当前算法强依赖 referenceImage 模板。

是否有测试按钮：有。

是否有保存参数逻辑：有，`ContourPresenceDialog::toToolConfig()`。

### 当前 Config 字段表

| 字段 | 类型/默认值/单位 |
|---|---|
| `roiNormalized` | `QRectF`，检测 ROI，归一化 |
| `params.templateRoiNormalized` | `QRectF`，模板 ROI，归一化 |
| `params.templatePolygonNormalized` | point array，归一化 |
| `params.templateSource` | string，`referenceImage` |
| `params.detectRegionType` | string，`rect/polygon` |
| `params.detectPolygonNormalized` | point array，归一化 |
| `params.templateShapeType` | string，`rect/polygon` |
| `params.enablePositionCorrection` | bool，默认 true |
| `params.positionCorrectionSource` | string |
| `params.minScore` | double，默认 0.5，0-1 |
| `params.polarity` | string |
| `params.thresholdType` | string |
| `params.scaleMode` | string，`auto/manual` |
| `params.speedScale/featureScale` | int，默认 5/1 |
| `params.thresholdMode` | string，`auto/manual` |
| `params.grayThreshold` | int，默认 15，灰度 |
| `params.chainMode` | string，`auto/manual` |
| `params.minChainLength` | int，默认 4，像素/点数语义 |
| `params.scaleMin/scaleMax` | double，默认 100/100，百分比 |
| `params.angleStart/angleExtent` | double，默认 -45/90，度 |
| `params.timeoutMs` | int，默认 2000，ms |
| `params.showContourPoints` | bool，默认 false |
| `params.sortMode` | string |
| `params.judgeBasis` | string，`presence/score` |
| `params.existOk` | bool |
| `params.scoreThreshold` | double，默认 0.5，0-1 |
| `judgeRule.mode/existOk/scoreThreshold` | JSON |

UI 显示但未真正进入 Runner 效果：

- 阈值类型/模式、灰度阈值、链长、速度尺度、特征尺度、排序方式保存但未参与当前检测。
- 尺度范围保存但 Runner 固定 100/100。
- mask、位置修正未应用。

### 当前 Adapter 映射表

Adapter 文件：`src/tooladapters/ContourPresenceAdapter.cpp`

| ToolConfig 字段 | Runner 字段 | 映射/问题 |
|---|---|---|
| `roiNormalized/templateRoiNormalized` | 同名 | 已传 |
| `templatePolygonNormalized/detectPolygonNormalized` | 同名 | 已传 |
| `templateShapeType/detectRegionType` | 同名 | 已传 |
| `minScore/scoreThreshold` | double 0-1 | Adapter 支持 0-1 或百分比输入 |
| `polarity/angleStart/angleExtent` | 同名 | 已传 |
| `scaleMin/scaleMax` | 同名 | 已传，但 Runner 委托 Pattern 时强制 100/100 |
| `thresholdType/scaleMode/speedScale/featureScale/thresholdMode/grayThreshold/chainMode/minChainLength` | 同名 | 已传，但 Runner 未用于检测 |
| `showContourPoints` | `showContourPoints` | 已传 |
| `runtimeContext.referenceTest` | `referenceTest` | 已传 |
| `halconSoPath` | `halconSoPath` | Adapter 未读取 params 中的 `halconSoPath`，直接默认解析 |

### 当前 Runner 算法链

Runner 文件：`src/algorithms/presence/ContourPresenceHalconRunner.cpp`

当前算法：

1. 校验空图、空基准图、模板 ROI、检测 ROI、模板/检测 ROI 类型。
2. 将 Contour config 转为 Pattern config。
3. 强制 `scaleMin=100/scaleMax=100`。
4. 调用 `PatternPresenceHalconRunner::run()` 做 image patch shape model 匹配。
5. 把 Pattern 结果映射为 ContourPresence 结果。
6. Overlay 清空后重建：检测 ROI、基准图测试时模板 ROI、匹配矩形/中心/score。
7. 勾选轮廓点时，从模板 patch 用 `edges_sub_pix("canny") -> get_contour_xld` 提取 XLD，再按匹配中心/角度旋转平移显示。

注意：文件内有 `extractTemplateContoursWithHalcon()`，但它只用于显示模板 patch 的 Canny 轮廓，不是当前图纯轮廓 presence。

### 当前 OK/NG 判定

| 规则 | 当前逻辑 |
|---|---|
| 检测有无 | 继承 PatternPresence 的 `found = matchCount > 0` |
| 最低得分 | 继承 PatternPresence 的 score 判定 |
| OK/NG | 完全来自委托的 Pattern result |

### 当前 Overlay 显示

| Overlay | 当前内容 | 真实性 |
|---|---|---|
| `detect_roi` | 检测 ROI | 真实 ROI |
| `template_roi` | referenceTest 时显示模板 ROI | 真实模板 ROI |
| `match_result` | 模板匹配框和中心 | 模板匹配结果，不是纯当前图轮廓 |
| `score_text` | 匹配得分 | 真实匹配分 |
| `contour_line` | 模板 patch Canny 轮廓变换到匹配位置 | 是模板轮廓的变换，不是当前图真实检测轮廓 |

ToolResult 主要字段：

- `score/count/text/status/message` 继承 Pattern 结果。
- `payload.imagePatchShapeModelPayload` 保存委托 Pattern payload。
- `payload.contourPointsOverlayApplied/contourOverlayLineSegmentCount` 表示模板轮廓显示。

缺失：

- 没有当前图真实 XLD 轮廓 count/length/longestLength。
- 没有闭合度、总长度、轮廓筛选结果。
- 缺少显式 `okNgReason`。

### 问题清单

- 当前不是“轮廓有无”，而是“图像 patch 模板匹配 + 可选模板轮廓显示”。
- 大量轮廓相关 UI 参数保存但未参与算法。
- Overlay 显示的轮廓不是当前图真实检测轮廓。
- 与推荐的纯轮廓 presence 语义不一致。

### 算法优化建议

- P6 改 Runner 为纯当前图轮廓检测：
  `ReduceDomain -> EdgesSubPix -> SegmentContoursXld -> SelectContoursXld/LengthXld`。
- 保留现有 UI 字段映射：`grayThreshold/minChainLength/thresholdMode/chainMode` 可先用于内部阈值或过滤；未能对应的尺度/模板字段先保留但标记未使用。
- ToolResult 增加 `contourCount/totalLength/longestLength/selectedContours/okNgReason`。
- Overlay 显示当前图真实 XLD 轮廓和数量/长度文本。
- 若未来要保留当前模板匹配能力，建议另命名为 `ContourMatchPresence`；第一阶段不改 UI。

是否需要改 UI：否。短期可在 Runner 内切换为纯当前图 XLD presence，模板相关 UI 参数保留但不参与，报告中说明语义差异；如长期拆分 ContourMatchPresence 才需要 UI/流程变更。

## 跨工具问题汇总

| 问题 | 影响工具 | 说明 |
|---|---|---|
| 缺少显式 `okNgReason` | 六类 | 目前只能从 `status/message/payload` 推断 OK/NG 原因 |
| 位置修正保存但未应用 | 六类多数 | payload 多处写 `positionCorrectionApplied=false` |
| mask/屏蔽区域 UI 未闭环 | Pattern/Blob/Circle/Line/Contour/Edge | 多数只是 UI 入口或保存开关，没有 Runner 应用 |
| 超时只标记不打断 | Pattern/Blob/Circle/Contour/Edge/Line | `timeoutExceeded` 是事后 payload 标记 |
| 参数保存但算法未使用 | Pattern sort/scale，Contour 多数轮廓参数，Edge/Line polarity | 需要在第二阶段优先补 Runner 或 Adapter |
| Overlay 类型不足以表达 HALCON region/XLD | Blob/Contour 等 | 当前只能用 Rect/Line/Circle/Polygon/Text 近似 |

## 第二阶段建议优先级

1. PatternPresence：真实 scaled shape model 与 `get_shape_model_contours` 轮廓显示，补 `scale/angle/matchCount/okNgReason`。
2. LinePresence：改 HALCON Metrology Line，真实使用 line-band ROI。
3. EdgePresence：改 `MeasurePos`，真实使用 line-band ROI。
4. BlobPresence：补连通域统计和中心/面积/面积率输出。
5. CirclePresence：保留 region 模式，补 radius/circularity/count 解释；预留卡尺默认参数。
6. ContourPresence：从模板匹配改为当前图 XLD 轮廓 presence，或明确拆分为 ContourMatchPresence。

第一阶段结论：当前架构链路完整，OCR 和六类 Presence Adapter 均已注册。主要问题集中在 Runner 算法语义、字段使用不完整、Overlay 真实性和 OK/NG 解释不足；按用户约束，第二阶段可优先改 Runner/Adapter/ToolResult payload，不需要重做 UI 或主流程。
