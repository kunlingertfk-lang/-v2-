# PatternPresence 多边形 ROI 崩溃和轮廓偏移修复报告

## 修改文件清单

- `src/algorithms/presence/PatternPresenceHalconRunner.h`
- `src/algorithms/presence/PatternPresenceHalconRunner.cpp`
- `src/tooladapters/PatternPresenceAdapter.cpp`
- `src/PatternPresenceDialog.h`
- `src/PatternPresenceDialog.cpp`

## HALCON #8510 根因

`create_shape_model` 返回 HALCON #8510 的直接含义是 shape model 的有效模型点太少。多边形模板 ROI 下最可疑链路是：polygon 点来自原图坐标，而模板图 `templateMat` 是 polygon 外接矩形裁剪图；如果 `gen_region_polygon` 使用原图坐标生成 region，`reduce_domain(templateMat, region)` 会错位或接近空域，最终建模点不足。

本次修复后，PatternPresence 在建模前明确执行：

- `templatePolygonNormalized` 转原图像素点，写入 `payload["templatePolygonGlobalPoints"]`。
- `templateRoiNormalized` 使用 polygon 外接矩形，裁剪 `templateMat`。
- `gen_region_polygon` 使用 `templateMat` 局部坐标：
  - `localCol = globalCol - boundingRect.x`
  - `localRow = globalRow - boundingRect.y`
- local 点 clamp 到 `templateMat` 范围内，写入 `payload["templatePolygonLocalPoints"]`。
- 写入 `templateBoundingRectPixel`、`templateMatWidth/Height`、`polygonRegionArea`、`polygonApplied`、`templatePolygonApplied`、`polygonFallbackToBoundingRect`。

## QMutex destroying locked mutex 为什么出现

这是 HALCON 错误路径没有被完整兜底时的二次症状：建模或匹配异常向外传播后，UI/测试调用栈和相机相关对象开始析构，仍可能存在模型缓存锁、模型实例锁或运行状态未正常退出，Qt 在析构锁对象时打印 `QMutex: destroying locked mutex`。

本次修复做了两层处理：

- Runner 内部 catch `HalconFailure`、`std::exception` 和未知异常，所有 HALCON 错误转成 `PatternPresenceHalconResult`。
- Adapter 和 Dialog 再兜底 catch，避免 Runner 之外的异常继续穿透到 ToolEngine/UI/相机线程。
- `create_shape_model` 不再在全局 shape model cache mutex 持锁期间执行，只在查缓存和插缓存时短暂持锁。

## polygon 坐标体系修正

已修正。多边形模板 ROI 的 HALCON region 现在基于 `templateMat` 局部坐标，不再把原图坐标直接交给 `reduce_domain(templateMat, region)`。

已写入 payload：

- `templatePolygonGlobalPoints`
- `templatePolygonLocalPoints`
- `templateBoundingRectPixel`
- `templateMatWidth`
- `templateMatHeight`
- `polygonRegionArea`
- `polygonApplied`
- `templatePolygonApplied`
- `polygonFallbackToBoundingRect`

## polygon 合法性检查

建模前新增检查：

- polygon 点数小于 3：不运行，提示 `多边形至少需要 3 个点`。
- polygon 面积过小：不运行，提示 `多边形区域过小`。
- bounding rect 或模板区域过小：不运行，提示 `模板区域过小`。
- polygon 有效区域太小：不调用 `create_shape_model`，提示 `模板区域有效特征过少，无法创建模型`。
- `create_shape_model` 返回 #8510：catch 后返回错误结果，不退出程序。

#8510 的返回内容：

- `success=false`
- `ok=false`
- `status="template model creation failed"`
- `message="模板区域有效模型点过少，无法创建图案模型"`
- `payload["halconErrorCode"]=8510`
- `payload["halconErrorMessage"]="Number of shape model points too small"`

## HALCON 异常兜底

已对 PatternPresence runner 内以下调用做统一返回码检查和异常兜底：

- `gen_region_polygon`
- `reduce_domain`
- `create_shape_model`
- `find_shape_model`
- `get_shape_model_contours`
- `vector_angle_to_rigid`
- `affine_trans_contour_xld`

Dialog 的状态栏现在会显示错误原因；Adapter/Dialog 都有额外 catch，防止异常导致程序退出、相机 provider 析构或 `closeCamera`。

## showContourPoints 坐标修正

`showContourPoints=true` 时仍只使用 HALCON shape model contours：

- `get_shape_model_contours`
- `vector_angle_to_rigid(0, 0, 0, rawMatchRow, rawMatchCol, matchAngle)`
- `affine_trans_contour_xld`

坐标换算规则：

- `find_shape_model` 在 `detectMat` 裁剪图内执行，返回 `rawMatchRow/rawMatchCol`，即 detectMat 局部坐标。
- 变换后的 contour 也是 detectMat 局部坐标。
- 显示到原图时只加 `detectRoi.x/y`。
- 不加 `templateRoi.x/y`。

新增 payload：

- `rawMatchRow`
- `rawMatchCol`
- `globalMatchRow`
- `globalMatchCol`
- `contourBBoxLocal`
- `contourBBoxGlobal`
- `detectRoiPixelX/Y/W/H`
- `templateRoiPixelX/Y/W/H`
- `contourTransformApplied`
- `contourPointCount`
- `contourDisplayPointCount`

missing 或 contour transform 失败时不会保留旧轮廓；UI 每次用当前 `ToolResult.overlays` 重置 overlay。

## polygon fallback 状态

正常路径不再静默退回 polygon 外接矩形。polygon 模板要求 HALCON `gen_region_polygon/reduce_domain` 成功应用，否则返回错误。

如果进程内命中了旧缓存模型且该模型标记为未应用 polygon，payload 会显式写：

- `templatePolygonApplied=false`
- `polygonApplied=false`
- `polygonFallbackToBoundingRect=true`
- `templatePolygonFallbackMessage="多边形模板未应用，已退回外接矩形"`

## 多边形绘制状态

PatternPresence 参数页测试前新增检查：

- 正在绘制 polygon 时，基准图测试/测试运行不会继续，提示 `模板多边形 ROI 未完成：请先点击完成闭合多边形`。
- polygon 点数不足时提示 `多边形至少需要 3 个点`。
- 完成按钮仍可调用 `finishPolygonDrawing()` 闭合。
- 已有 `FrameViewHelper` 使用 `m_polygonHoverPointValid`，无效 hover 点不会画到 `(0,0)`；靠近首点点击会自动闭合。

## PatternPolygon 调试日志

仅参数页测试打开 `debugPatternPolygonLog`，连续运行关闭，日志格式为 `[PatternPolygon] ...`，包含：

- `templateShapeType`
- `pointsNorm`
- `globalPoints`
- `boundingRect`
- `localPoints`
- `templateMat`
- `polygonRegionArea`
- `reduceDomainApplied`
- `createModelMs`
- `findMs`
- `halconErrorCode`
- `modelPointStatus`
- `showContourPoints`
- `contourBBoxLocal`
- `contourBBoxGlobal`

## 编译结果

- `qmake`：通过。
- `make -j$(nproc)`：通过。
- 编译仍有一个既有 warning：`rotatedRectCorners` 未使用；本次没有扩大处理。

## 手工测试建议

1. 矩形模板 ROI 基准图测试，确认 found 正常且无回归。
2. 多边形模板 ROI 画 4 点，靠近首点闭合后基准图测试，确认不崩溃。
3. 故意画很小或空白 polygon，确认提示有效特征过少或模型点过少，不退出程序，相机不关闭。
4. 打开 `showContourPoints`，确认显示的是 HALCON shape model contours，轮廓贴近匹配目标，不显示旧轮廓残留。
5. 多次重新画 polygon、多次基准图测试，确认无崩溃、无 `QMutex destroying locked mutex`。

## 边界说明

- 没有修改 PatternPresence 匹配语义；只修 polygon ROI 数据链路、HALCON 错误返回、contour 显示坐标和 UI 测试保护。
- 没有修改 OCR / Blob / Circle / Edge / Line / Contour 算法。
- 没有修改 ToolEngine 调度原则。
- 没有从 QLabel/QPixmap/QGraphicsView 截图。
- 没有把算法写进 Dialog。
