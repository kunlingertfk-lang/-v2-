# PatternPresence 模型轮廓点专项诊断报告

## 结论

本轮没有修改 PatternPresence 的 OK/NG 判断语义：`create_shape_model`、`find_shape_model`、`found`、`score`、`judgeBasis`、`existOk` 的判定路径保持不变。

当前代码层面可以确认：旧的 `showContourPoints` 显示差，主要是模型轮廓可视化实现问题，不是拿 ROI/匹配框/raw edge 冒充模型轮廓。旧实现确实调用了 `get_shape_model_contours`，但运行显示只允许前 8 个 XLD contour object，并且总线段限制为 512；当 HALCON 返回很多碎 contour 时，界面可能只显示前几个短碎片，造成“零散点/短线/不贴边”的效果。

是否某个具体模板的 HALCON shape model 轮廓本身也很差，必须以本轮新增的 `model_contours_local.png` 为准判断。如果该图里轮廓本身就很少或很碎，则是模型轮廓源质量问题；如果该图完整而界面差，则是显示/变换问题。修复后，后者已经处理。

## 诊断图

勾选 `showContourPoints=true` 后，Runner 会为每次运行创建目录：

`/home/hjl-ubuntu/桌面/pattern_contour_debug/<timestamp>_<seq>_<toolId>/`

输出文件：

- `template_crop.png`：模板 crop 原图，无 overlay。
- `template_gray.png`：建模前对应灰度图。
- `model_contours_local.png`：`get_shape_model_contours` 返回的模型 XLD，平移到模板 crop 中心后绘制，用于判断模型本身轮廓质量。
- `match_contours_global.png`：模型 XLD 经 `vector_angle_to_rigid(0,0,0,row,column,angle)` 和 `affine_trans_contour_xld` 后，加检测 ROI 偏移绘制到检测原图。
- `debug_info.json`：记录 ROI、参数、score、row/column/angle、contour 数量、点数、本地/全局 bbox、`showContourPointsApplied`、`contourSource` 等。

## 核查项

1. `get_shape_model_contours` 是否调用：是。来源仍是 HALCON shape model contours，不是 ROI、匹配框或 raw edge。

2. 是否遍历全部 XLD contours：已修复。旧实现统计时遍历全部，但显示只取前 8 个；新实现先遍历全部 contour object 做统计和 debug 图，再按总线段预算采样显示。

3. 是否之前只显示了部分 contour：是。`kMaxContourOverlayContours=8` 是旧显示差的直接风险点之一，已移除。

4. row/column 是否有问题：本轮保留并明确为 `x = column`、`y = row`。全局显示只加 `detectRoiPixels.x/y`，不加 `templateRoiPixels.x/y`。

5. transform origin 是否有问题：匹配显示使用 `vector_angle_to_rigid(0,0,0,bestRawRow,bestRawColumn,bestAngle)`，符合 HALCON shape model contour 的常规显示方式。没有把 template ROI 原点叠加到匹配结果。

6. 是否过度采样/过滤：旧实现存在过度截断和对象过滤。新实现不按 object 数截断，线段上限提高到 4096，并按总线段比例采样；点 marker 只保留少量 1px 辅助点，主显示是 polyline/Line 分段。

7. debug 图保存路径：见上方 `/home/hjl-ubuntu/桌面/pattern_contour_debug/...`。具体本次 UI 运行目录会写入 payload 的 `contourDebugDir` 和 `debug_info.json` 的 `debugDir`。

8. 修复后是否能像海康一样显示有效轮廓：如果 `model_contours_local.png` 里 HALCON 模型轮廓足够完整，运行 overlay 和 `match_contours_global.png` 应能显示贴合目标的有效模型轮廓。如果本地图本身碎/少，则不能假装修好。

9. 如果不能，原因：HALCON shape model 轮廓本身过少或过碎。可能原因包括模板有效边缘不足、ROI/domain 太小、多边形 reduced domain 有效区域太少、图像噪声/摩尔纹、polarity/metric 不合适，或 `create_shape_model` 的 Contrast/MinContrast 当前均为 `auto`。当前“灵敏度=2”已进入配置和 debug_info，但本轮未改变匹配语义，也未把它改映射到 HALCON Contrast/MinContrast。

10. 是否修改匹配算法语义：没有。只改诊断输出、轮廓 overlay 生成和稀疏轮廓提示。

## 稀疏模型处理

当 `get_shape_model_contours` 返回的点数或线段数过少时：

- `showContourPointsApplied=false`
- `contourSource="shape_model_contours_too_sparse"`
- `reason="shape model contours too sparse"`
- 状态栏提示：“当前模型轮廓点过少，建议调整模板区域或灵敏度。”

这时不显示乱点，避免把差模型轮廓伪装成可用轮廓。

## 编译

已执行：

- `qmake`：通过。
- `make -j$(nproc)`：通过。

仅保留既有 warning：`rotatedRectCorners` defined but not used。
