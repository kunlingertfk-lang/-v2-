# 模板定位功能设计与实现

## 1. 目标链路

```text
ToolLibraryDialog / ToolsDialog
        -> TemplateLocationDialog
        -> ToolConfig.params
        -> TemplateLocationAdapter
        -> TemplateLocationHalconRunner
        -> ToolResult / overlays / payload
```

模板定位使用 HALCON 形状模板匹配，OpenCV 仅作为 `cv::Mat` 输入容器和 Qt/HALCON 格式桥接。

## 2. 配置合同

`ToolType::TemplateLocation`、`ToolCategory::Location` 的参数保存到 `ToolConfig.params`：

```json
{
  "version": 4,
  "templateRegionType": "rectangle",
  "templateRoiNormalized": {"x": 0, "y": 0, "width": 1, "height": 1},
  "templatePolygonNormalized": [],
  "templateMaskRegionType": "none",
  "templateMaskRoiNormalized": {"x": 0, "y": 0, "width": 0, "height": 0},
  "templateMaskPolygonNormalized": [],
  "templateMaskCircleCenterNormalized": {"x": 0.5, "y": 0.5},
  "templateMaskCircleRadiusNormalized": 0,
  "searchRegionType": "full",
  "searchRoiNormalized": {"x": 0, "y": 0, "width": 1, "height": 1},
  "searchPolygonNormalized": [],
  "searchCircleCenterNormalized": {"x": 0.5, "y": 0.5},
  "searchCircleRadiusNormalized": 0.2,
  "minScore": 50,
  "angleStart": -45,
  "angleExtent": 90,
  "scaleMin": 100,
  "scaleMax": 100,
  "polarity": "use_polarity",
  "contrastMode": "auto",
  "contrast": 40,
  "minContrast": 10,
  "numLevels": 0,
  "subPixel": "least_squares",
  "greediness": 0.5,
  "timeoutMs": 2000,
  "maxMatches": 1,
  "minMatchCount": 1,
  "maxMatchCount": 1,
  "maxOverlap": 50,
  "originMode": "centroid",
  "customOriginNormalized": {"x": 0.5, "y": 0.5},
  "modelCacheKey": "<stable key>",
  "modelCreated": false
}
```

`numLevels=0` 表示 HALCON 自动层级。旧方案缺失字段时使用上表默认值。`templateMaskRegionType` 支持 `none`、`rectangle`、`circle`、`polygon`，对应使用归一化矩形、圆心/半径或多边形字段；旧版仅含 `templateMaskPolygonNormalized` 时自动迁移为多边形屏蔽。`maxOverlap` 使用 UI 百分比，Adapter 转为 HALCON 的 0.0–1.0。`modelCacheKey` 是稳定身份，缓存签名同时包含基准图灰度内容、模板 ROI、模板屏蔽区和全部建模参数；签名一致时用 `read_shape_model` 恢复，首次或签名变化时用 `write_shape_model` 覆盖持久缓存。删除模板同时删除对应缓存文件。

## 3. HALCON 算子链

1. OpenCV 仅负责输入通道归一化与内存桥接，随后使用 `gen_image1` 创建 HALCON 灰度图像。
2. `crop_rectangle1` 建立模板和搜索包围域；模板或搜索多边形使用 `gen_region_polygon`，圆形搜索使用 `gen_circle`，随后通过 `reduce_domain` 限制域；全局搜索直接使用整图域。存在模板屏蔽区时，矩形使用 `gen_rectangle1`、圆形使用 `gen_circle`、多边形使用 `gen_region_polygon_filled` 生成实心屏蔽 Region，再使用 `intersection` 将其裁到模板基础 Region，并用 `difference` 得到有效建模 Region；有效面积不足模板基础面积 5% 时返回 `template_masked_empty`。
3. `create_scaled_shape_model` 创建模型。自动模式传入字符串 `auto`；手动模式传入数值 `Contrast` 和 `MinContrast`；`write_shape_model` / `read_shape_model` 管理磁盘持久缓存。
4. `get_shape_model_params` 回读实际 `NumLevels` 和 `MinContrast`。HALCON 20.11 不提供数值型 `Contrast` 回读，自动模式将 `contrastUsed` 明确保留为 `"auto"`。
5. `find_scaled_shape_model` 搜索匹配目标，`NumMatches` 使用 UI 的“最大查找数”，`MaxOverlap` 使用配置值；返回数量再按 `minMatchCount..maxMatchCount` 判定 OK/NG。
6. `get_shape_model_contours`、`vector_angle_to_rigid`、`hom_mat2d_scale_local`、`affine_trans_contour_xld` 和 `get_contour_xld` 生成结果轮廓；自定义点再通过 `affine_trans_point_2d` 变换到匹配位姿。
7. `clear_shape_model`、`clear_obj` 释放 HALCON 资源。

本机 HALCON 20.11.1 已确认上述关键符号存在，现有 shape model smoke 已能成功建模和匹配。

## 4. 结果合同

`x` 对应 HALCON `Column`，`y` 对应 `Row`，角度对外统一为度。`ToolResult.payload` 至少包含：

```text
found, countAccepted, foundCount, matches, x, y, angle, angleDeg, scale, score, pose, elapsedMs
contrastMode, contrastUsed, minContrastUsed, numLevelsUsed
templateRoiNormalized, templateMaskApplied, templateMaskRegionType
templateMaskRoiNormalized, templateMaskPolygonNormalized
templateMaskCircleCenterNormalized, templateMaskCircleRadiusNormalized
templateBaseArea, templateEffectiveArea, searchRoiNormalized
modelCacheKey, modelCacheHit, modelCachePersisted, modelSignature, errorCode, errorMessage
```

- 找到且数量合格：`success=true`、`ok=true`、`status=found`；找到但数量不合格：`success=true`、`ok=false`、`status=count_out_of_range`。`matches` 返回每个目标的定位点位姿和得分，顶层 `x/y/angle` 固定取第一个最佳匹配，供位置修正绑定。
- 未找到：`success=true`、`ok=false`、`status=not_found`、`count=0`。
- 配置、输入或 HALCON 错误：`success=false`、`ok=false`，返回稳定错误码。
- overlays 包含搜索 ROI、变换后的模型轮廓、定位中心十字，以及每个匹配对应的序号和得分文字。建模预览将首个自匹配轮廓标记为橙色 `template_model`；查找结果使用绿色 `match_result`，两者不混色。
- 模板屏蔽区只影响建模 Domain。质心模式仍输出未屏蔽模板基础 Region 的质心，自定义定位点也不因屏蔽区变化而偏移。

## 5. 验证标准

- 工具库入口可选择；能新建、保存、重新打开、编辑、测试运行并展示结果。
- 自动/手动对比度均能建模；手动模式拒绝 `MinContrast >= Contrast`；参数修改后要求重建。
- 已知平移、旋转和缩放的合成图返回容差内的 X/Y/角度/缩放。
- 覆盖空图、无模板、无效 ROI、未找到、超时、runtime/license/符号异常且不崩溃。
- 执行 qmake/make、模板定位算法/UI smoke，并回归现有图案有无 smoke。

## 6. 实现与验证记录（2026-07-20）

- 已完成独立的 `TemplateLocationDialog`、`TemplateLocationAdapter`、`TemplateLocationHalconRunner`，并接入工具库、工具编辑页、主窗口引擎和 qmake 工程；模板定位不调用其他功能的 Runner。
- 已完成矩形/多边形模板 ROI，全局/矩形/圆形/多边形搜索 ROI；搜索模式采用高对比度持续高亮并支持连续覆盖绘制。已完成数量判定、最大重叠率、质心/自定义定位点、持久模型缓存、可折叠六列多结果表、创建/重建/删除模板、逐目标得分、基准图测试和连续测试。
- 算法 smoke 已覆盖自动/手动对比度、实际 `MinContrast` 回读、平移、旋转、缩放、未匹配、空图、无模型、无效 ROI、参数约束、超时参数传入及原图案有无默认对比度模式回归。
- UI smoke 已覆盖配置回显、模型状态、自动/手动控件显隐、参数变更使模型失效、工具库选择和预览画布。
- Qt 5.15.2 全工程已在 `/tmp` 影子目录执行 qmake/make 并成功链接；未向源码目录写入本次构建产物。
- 当前无已知实现阻塞；HALCON runtime/license/符号异常沿既有 runner 错误链返回，不做算法降级。

## 7. 模板屏蔽区域实现与验证记录（2026-07-25、2026-07-26）

- “全部”页提供单个矩形、圆形或多边形模板屏蔽区及清除入口；模板 ROI、模板屏蔽区、搜索 ROI 和定位点继续使用互斥编辑状态。
- 屏蔽区按归一化坐标保存和回显，修改或清除屏蔽区会使模型失效；配置合同升级为版本 4，旧配置缺少字段时按空屏蔽区兼容，版本 3 的多边形字段可自动迁移。
- HALCON 建模使用“模板基础 Region - 屏蔽 Region”的有效 Domain；屏蔽区不参与搜索，不会显示在实时检测图的固定参考坐标上。
- 模型缓存签名包含屏蔽区；结果 payload 返回是否应用屏蔽、基础面积和有效面积。
- 错误合同增加 `invalid_template_mask` 和 `template_masked_empty`。
- 算法 smoke 覆盖矩形、圆形、多边形的实际 Domain 扣除，以及缓存失效/命中、不对称屏蔽原点稳定、无交集屏蔽和完全屏蔽；UI smoke 覆盖三种入口、配置回显、旧配置迁移、基础/全部显隐及清除后模型失效。
- 已通过 `template_location_smoke`、`template_location_ui_smoke`（offscreen）以及 `qt_ui_test.pro` 的 qmake/全工程构建；构建产物位于既有 `build/` 或 `/tmp` 影子目录。
