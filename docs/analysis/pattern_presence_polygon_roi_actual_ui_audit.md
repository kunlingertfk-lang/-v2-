# PatternPresence 多边形 ROI 与相机纹理轮廓审计

日期：2026-06-01

范围：只审计当前“图案有无 PatternPresence”的实际 UI 参数和现有代码链路。本轮未改 MainWindow、ToolEngine、OCR、BlobPresence、CirclePresence，未新增 UI 参数，未改匹配算法核心逻辑。当前代码里已经存在临时 `[PatternPresence][ROI]` qDebug 观测点，本报告按现有代码和已有 debug 目录做阶段 A 结论。

## 1. 当前实际 UI 参数链路

`PatternPresenceDialog::configuration()` 是参数入口：

- 搜索模板：
  - 模板区域支持矩形/多边形按钮。
  - 多边形模式下，`templateRoiNormalized` 被设为多边形外接矩形，`templateShapeType="polygon"`，`templatePolygonNormalized` 保存点集。
  - 灵敏度来自 `basicSensitivitySpinBox` / `sensitivitySpinBox`，当前测试值 2。
- 检测区域：
  - 检测矩形保存在顶层 `ToolConfig::roiNormalized`。
  - UI 上有自由绘制/矩形/圆形按钮，但当前 PatternPresence 点击自由绘制或圆形会提示未实现并切回矩形；Runner 的 detect circle 也没有实现。
  - 独立位置修正使能和来源会保存到 `enablePositionCorrection` / `positionCorrectionSource`，但 Runner payload 当前写 `positionCorrectionApplied=false`，不参与本次匹配。
- 识别设置：
  - `showContourPoints` 保存“运行显示轮廓点”。
- 结果判断：
  - `judgeBasis` 保存 `presence` 或 `score`；当前问题图为结果有无。
  - `existOk` 保存存在OK/不存在OK。

主要代码点：

- `src/PatternPresenceDialog.cpp:239` `configuration()`
- `src/PatternPresenceDialog.cpp:302` `toToolConfig()`
- `src/tooladapters/PatternPresenceAdapter.cpp:73` `toHalconConfig()`
- `src/algorithms/presence/PatternPresenceHalconRunner.cpp:1630` `PatternPresenceHalconRunner::run()`

## 2. 问题 1：多边形 ROI 运行轮廓点失败

### 2.1 链路审计

Dialog 层没有发现点集丢失：

1. `FrameViewHelper::eventFilter()` 用 `mapToScene()` 把鼠标 view 坐标转成 image 坐标，见 `src/frame/FrameViewHelper.cpp:986`。
2. `FrameViewHelper::completeDraftPolygon()` 用 `imagePointToNormalized()` 转成归一化 image 坐标，并 emit `polygonChanged(m_polygonNormalized)`，见 `src/frame/FrameViewHelper.cpp:1059`。
3. `PatternPresenceDialog::handleTemplatePolygonChanged()` 保存 `m_templatePolygonNormalized`，并用 `boundingRectForPoints()` 更新模板外接矩形，见 `src/PatternPresenceDialog.cpp:1005`。
4. `PatternPresenceDialog::toToolConfig()` 保存 `templateShapeType`、`templatePolygonNormalized`、`templateRoiNormalized`，见 `src/PatternPresenceDialog.cpp:312`。
5. `PatternPresenceAdapter::toHalconConfig()` 读取这些字段并传给 Runner，见 `src/tooladapters/PatternPresenceAdapter.cpp:108`。

Runner 坐标链路基本正确：

1. `normalizedPolygonToPixels()` 将 normalized 点转成原图像素点，见 `src/algorithms/presence/PatternPresenceHalconRunner.cpp:631`。
2. `polygonToLocalClamped()` 用 `point - templateRoiPixels.x/y` 把原图点平移到模板 crop 局部坐标，见 `src/algorithms/presence/PatternPresenceHalconRunner.cpp:175`。
3. HALCON tuple 构造时 `columns.append(point.x())`、`rows.append(point.y())`，调用时 rows 在前、columns 在后，符合 HALCON row=y、col=x，见 `src/algorithms/presence/PatternPresenceHalconRunner.cpp:2405`。
4. Overlay 轮廓不是 ROI/bbox fallback。只有 `showContourPoints && found` 后，才用 `get_shape_model_contours -> vector_angle_to_rigid -> affine_trans_contour_xld`，再加 detect ROI 偏移回原图，见 `src/algorithms/presence/PatternPresenceHalconRunner.cpp:2926`。

### 2.2 根因判断

根因定位在 Runner/HALCON region 生成层：

- 当前 PatternPresence 解析并调用的是 `T_gen_region_polygon`，见 `src/algorithms/presence/PatternPresenceHalconApi.cpp:125` 和 `src/algorithms/presence/PatternPresenceHalconRunner.cpp:2416`。
- 本机 HALCON 24.11 英文文档写明 `gen_region_polygon` 创建的是 polygon route pixels，并且 “not automatically closed and not filled”。文档路径：`/home/hjl-ubuntu/MVTec/HALCON-24.11-Progress-Steady/doc_en_US/html/reference/operators/gen_region_polygon.html`。
- 同一文档说明 `gen_region_polygon_filled` 才返回 filled region，并指出没有孔洞的 polygon 可用 `gen_region_polygon + fill_up` 替代。文档路径：`/home/hjl-ubuntu/MVTec/HALCON-24.11-Progress-Steady/doc_en_US/html/reference/operators/gen_region_polygon_filled.html`。
- 当前代码没有追加首点闭合，也没有对模板 polygon region 调 `fill_up`；因此 `reduce_domain()` 实际作用域接近未闭合边线，而不是用户圈选的填充模板区域。

这与已有失败日志一致：

- 失败 debug：`/home/hjl-ubuntu/桌面/pattern_contour_debug/20260601_181440_016_25_pattern_presence_9286a6f5-7b58-415d-91e4-86cdc99d1d9e/debug_info.json`
- `templateShapeType="polygon"`
- `templateRoiPixels={x:186,y:178,width:193,height:192}`
- `templatePolygonLocalPoints` 有 9 点，且落在 193x192 crop 内，说明点集和 crop 平移不是空的。
- `polygonRegionArea` 的本地几何面积很大，约 28929 像素级；但 auto-domain 的各阈值 `thresholdRegionArea` 为 `0,0,0,0,0,1`，最终 `autoModelDomainFallbackReason="target_too_small"`。
- 这说明图像/ROI 几何上不是空，但 HALCON reduced domain 内可 threshold 的像素几乎没有。

矩形 ROI 能跑的原因：

- 矩形模板不走 `gen_region_polygon`，直接用 `templateMat` 整个矩形 crop 做建模输入。
- 非 polygon 时 Runner 将域面积设为 `templateMat.cols * templateMat.rows`，见 `src/algorithms/presence/PatternPresenceHalconRunner.cpp:1935`。
- 成功 debug：`/home/hjl-ubuntu/桌面/pattern_contour_debug/20260601_172826_866_7_pattern_presence_9286a6f5-7b58-415d-91e4-86cdc99d1d9e/debug_info.json`
  - `templateShapeType="rectangle"`
  - `autoModelDomainApplied=true`
  - `thresholdRegionArea=16383`
  - `displayContourObjectCount=3`
  - `displayContourPointCount=1166`
  - `bestScore=0.998565`

结论：多边形 ROI 失败不是图片优先问题，也不是灵敏度=2 优先问题；是 polygon ROI 在 HALCON 中未作为“已闭合填充区域”进入 `reduce_domain()`，导致自动目标域和后续模型点不足。

### 2.3 当前 qDebug 观测点

当前代码已有 `logPatternPresenceRoiDebug()`，会在 `debugPatternPolygonLog` 或 `showContourPoints` 打开时输出：

```text
[PatternPresence][ROI]
roiType=
imageWidth=
imageHeight=
templateRoiRect=
polygonPointCount=
polygonPointsImageCoord=
polygonBBox=
halconRegionArea=
reducedDomainArea=
thresholdRegionArea=
xldContourCount=
modelPointCount=
createModelSuccess=
errorCode=
```

函数位置：`src/algorithms/presence/PatternPresenceHalconRunner.cpp:1453`。

下一步复现时需要重点看：

- `polygonRegionArea` 是否远大于 `halconRegionArea`
- `halconRegionArea/reducedDomainArea` 是否接近 polygon 填充面积
- `thresholdRegionArea` 是否仍接近 0
- `modelPointStatus`、`halconStage`、`halconErrorCode`

如果改为 filled region 后，上述面积仍不合理，再继续查点序、自交 polygon、clip_region、subpixel rounding。

## 3. 问题 2：相机实拍图内部纹理也被识别

### 3.1 当前算法路线

PatternPresence 当前不是 raw XLD 边缘模板，也不是 `create_scaled_shape_model_xld`。主路径是 HALCON scaled shape model：

1. template crop / detect crop；
2. BGR/BGRA 转灰度；
3. 可选自动目标域：`threshold -> connection -> select_shape(area) -> fill_up -> opening_circle -> closing_circle -> dilation_circle -> reduce_domain`；
4. `create_scaled_shape_model()` 建模；
5. `find_scaled_shape_model()` 检测；
6. `showContourPoints=true` 且 found 后，`get_shape_model_contours()` 取模型轮廓并仿射到匹配位置。

主要证据：

- payload 写 `shapeModelOperator="create_scaled_shape_model/find_scaled_shape_model"`，见 `src/algorithms/presence/PatternPresenceHalconRunner.cpp:1821`。
- 实际调用 `createScaledShapeModel()`，见 `src/algorithms/presence/PatternPresenceHalconRunner.cpp:2486`。
- 实际调用 `findScaledShapeModel()`，见 `src/algorithms/presence/PatternPresenceHalconRunner.cpp:2743`。
- auto-domain 阈值候选为 `100,115,130,145,160,175`，见 `src/algorithms/presence/PatternPresenceAutoModelDomain.h:23`。

### 3.2 灵敏度=2 的实际映射

`PatternPresenceHalconRunner::shapeModelContrastSettings()`：

```text
s = clamp(templateSensitivity, 1, 10)
Contrast = 70 - s * 3
MinContrast = max(18, Contrast / 2)
```

函数位置：`src/algorithms/presence/PatternPresenceHalconRunner.cpp:77`。

所以当前灵敏度 2 实际传给 `create_scaled_shape_model()`：

```text
Contrast = 64
MinContrast = 32
```

方向没有反：灵敏度越低，Contrast 越高，理论上保留更少弱边缘；灵敏度越高，Contrast 越低，会保留更多弱边缘。

但灵敏度目前只映射到 shape model 的 `Contrast/MinContrast`：

- 不映射到 auto-domain threshold 候选。
- 不映射到 `edges_sub_pix` 的 Alpha/Low/High。
- 不映射到预处理强度。

### 3.3 为什么 PC 标准图干净、相机图内部纹理多

PC 导入标准图轮廓干净，主要因为图像更平滑、主轮廓梯度稳定，内部点阵/摩尔纹弱或不存在。已有截图 `aa28d892-6b79-4355-a108-b28e517c4eef.png` 显示 `score=1.000`，overlay 主要贴合外圈和箭头主轮廓。

相机实拍图中，图案内部存在点阵、印刷纹理、摩尔纹和采集噪声。这些纹理在局部梯度上可能并不低于 `Contrast=64/MinContrast=32`，所以会进入 HALCON shape model 的模型轮廓。已有截图 `7fae0353-d094-426b-a463-b67212267412.png` 显示 `score=0.867`，匹配成功，但青色轮廓包含大量内部短碎纹理。

已有 debug 也能说明当前显示的是 raw model contours：

- `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260601_182240_057_28_pattern_presence_9286a6f5-7b58-415d-91e4-86cdc99d1d9e/debug_info.json`
  - `templateShapeType="rectangle"`
  - `templateSensitivity=2`
  - `createShapeModelContrastUsed=64`
  - `createShapeModelMinContrastUsed=32`
  - `contourSource="get_shape_model_contours"`
  - `displayContourObjectCount=1054`
  - `displayContourPointCount=2038`
  - `bestScore=0.867463`
- `/home/hjl-ubuntu/桌面/pattern_contour_debug/20260601_172706_775_1_pattern_presence_9286a6f5-7b58-415d-91e4-86cdc99d1d9e/debug_info.json`
  - `displayContourObjectCount=1503`
  - `displayContourPointCount=2911`
  - `contourSource="get_shape_model_contours"`

### 3.4 当前没有的处理

当前 PatternPresence 没有看到这些建模前预处理：

- `mean_image`
- `gauss_filter`
- `median_image`
- `emphasize`
- `scale_image_range`

当前也没有对 showContour overlay 做这些过滤：

- 最小轮廓长度
- 最小面积
- 边缘幅值
- 闭合性
- 主轮廓优先
- 内部短碎边抑制

`appendContourLineOverlays()` 只是遍历模型 XLD 并按 `kMaxContourOverlayLineSegments=4096` 做显示采样/上限控制，不是语义过滤。位置：`src/algorithms/presence/PatternPresenceHalconRunner.cpp:1230`。

因此当前“运行显示轮廓点”显示的是 raw shape model contour，不是 filtered contour。视觉上“内部全识别”的直接原因是 overlay 把 `get_shape_model_contours` 返回的大量内部碎轮廓都画出来了。

## 4. 层级定位

| 层级 | 结论 |
| --- | --- |
| Dialog | 多边形绘制、完成、保存基本完整；点集为归一化 image 坐标。 |
| ToolConfig | `templateShapeType`、`templateRoiNormalized`、`templatePolygonNormalized` 均保存。 |
| Adapter | polygon 点、灵敏度、showContourPoints 均传给 Runner。 |
| Runner 坐标 | normalized -> 原图像素 -> template crop 局部坐标存在；row=y/col=x 正确。 |
| HALCON polygon region | 根因层。当前使用 `gen_region_polygon`，未闭合且未填充，不符合模板 ROI 填充区域语义。 |
| 自动目标域 | 在错误/过窄 polygon domain 上 threshold，导致 `threshold_no_region` 或 `target_too_small`。 |
| Overlay | 坐标映射不是多边形失败主因；相机纹理问题来自 raw model contours 无过滤。 |
| MainWindow/ToolEngine | 本轮未发现需要改动；不应把逻辑塞进这里。 |

## 5. 建议修改文件清单

阶段 B 建议只触碰：

- `src/algorithms/presence/PatternPresenceHalconApi.h`
- `src/algorithms/presence/PatternPresenceHalconApi.cpp`
- `src/algorithms/presence/PatternPresenceHalconRunner.cpp`

暂不建议改：

- `src/MainWindow.cpp`
- `src/toolcore/ToolEngine.*`
- OCR、BlobPresence、CirclePresence 算法文件
- UI 基础页参数

## 6. 建议修改函数清单

polygon ROI 修复：

- `PatternPresenceHalconLibrary::load()`：解析 `T_gen_region_polygon_filled`，或保留 `gen_region_polygon` 后增加 `fill_up`。
- `PatternPresenceHalconApi`：增加 filled polygon function pointer 或复用已有 fill operator。
- `PatternPresenceHalconRunner::run()`：模板 polygon 分支用 filled/closed region 后再 `reduce_domain()`；保留当前 row/col 和 crop 局部坐标。
- `logPatternPresenceRoiDebug()`：保留当前 qDebug，复测时确认 filled 后面积。

相机纹理优化：

- `PatternPresenceHalconRunner::shapeModelContrastSettings()`：复核低敏档 Contrast/MinContrast 是否需要更强抑制，但要保持当前 UI 复用。
- `PatternPresenceHalconRunner::showContourPoints` 分支：优先做只影响 overlay 的轮廓过滤，输出 raw/filtered object count 和 point count，不改变 found/score/OK/NG。
- 如果 overlay 过滤后仍有长纹理，再评估建模前轻度平滑/降噪。

## 7. 下一阶段修复计划

1. P0：把模板 polygon ROI 的 HALCON region 改为填充闭合 region，优先 `T_gen_region_polygon_filled`；若运行环境符号不可用，再用 `gen_region_polygon + fill_up`。
2. P1：用多边形问题图复测，确认 `[PatternPresence][ROI]` 中 `halconRegionArea/reducedDomainArea` 接近 `polygonRegionArea`，`thresholdRegionArea` 不再接近 0。
3. P2：确认矩形 ROI 原有成功路径不变，`score/found/OK/NG` 不回退。
4. P3：对相机实拍图先做 overlay 过滤，不改匹配结果语义；记录 raw/filtered contour 数量。
5. P4：回归 OCR、BlobPresence、CirclePresence，确认 polygon API 变更没有扩散影响。

## 8. 本轮验证状态

- 已执行代码搜索和只读审计。
- 已检查问题截图目录：`/mnt/hgfs/vmare-shared/61图案有无问题图`。
- 已打开关键截图核对现象。
- 已读取现有 debug 日志作为证据。
- 未运行 GUI 自动验收。
- 未运行构建；本轮只更新审计报告文档，未改 C++ 代码。
