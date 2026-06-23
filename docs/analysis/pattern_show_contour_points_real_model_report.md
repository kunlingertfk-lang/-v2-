# PatternPresence showContourPoints 真实模型轮廓核查报告

## 结论

本次只修正 PatternPresence 的运行 overlay 分层和模型轮廓显示 payload，不限制模板 ROI / 检测 ROI 的大小，不改变 PatternPresence 匹配判断语义，不改 ToolEngine，不改其他工具算法。

`showContourPoints=true` 的有效路线已经改为明确的 HALCON shape model XLD 路线：`get_shape_model_contours` 取模型轮廓，`vector_angle_to_rigid` 生成从模型参考点到 `find_shape_model` 匹配位姿的刚性变换，`affine_trans_contour_xld` 变换轮廓，`get_contour_xld` 取点，最后只把检测 ROI 偏移加回原图坐标。

## 当前“搞笑轮廓”的来源

1. `showContourPoints=true` 时 Runner 输出：
   - 固定输出：`detect_roi`。
   - found 时输出：`match_rect`、两条 `match_center` 线、`match_score` 文本。
   - found 且 `showContourPoints=true` 且 HALCON 符号可用时输出：`contour_line`、`contour_points`。

2. 当前青色/蓝色点的来源：
   - `contour_points` 是 `get_contour_xld` 从 `affine_trans_contour_xld` 后的模型 XLD 轮廓中取出的点，显示为小圆。
   - 不是 `edges_sub_pix`、`threshold`、raw edge，也不是 QLabel/QPixmap/QGraphicsView 截图。

3. 当前蓝色大多边形的来源：
   - 原 Runner 会把 `template_roi` 也塞进运行结果 overlay。
   - `FrameViewHelper` 把 `template_roi` 画成蓝色，所以在检测图运行显示时会像一个大蓝色多边形/边界。
   - 这不是模型轮廓，已从 PatternPresence 运行 overlay 中移除；payload 记录 `runtimeTemplateRoiOverlayEmitted=false`。

4. 是否调用 `get_shape_model_contours`：
   - 是。调用点在 `PatternPresenceHalconRunner.cpp` 的 showContourPoints 分支。

5. 是否使用 `vector_angle_to_rigid` / `affine_trans_contour_xld`：
   - 是。变换从 `(0, 0, 0)` 到 `find_shape_model` 返回的本地 `Row/Column/Angle`。
   - 因为 `get_shape_model_contours` 返回的是参考点归一到 `(0,0)` 的模型轮廓，所以这里不叠加 template ROI 坐标。

6. 是否把 raw edge 当 contour points 画：
   - PatternPresence Runner 当前没有 `edges_sub_pix` / `threshold` raw edge 轮廓输出。
   - 本次也没有添加任何 raw edge 轮廓 fallback。

7. 是否把 ROI 边界当 contour points 画：
   - `detect_roi` 仍作为检测区域 overlay 显示。
   - `template_roi` 不再作为运行结果 overlay 显示。
   - `contour_line` / `contour_points` 只来自 HALCON shape model contours。

8. 是否只是改变 `match_rect` 颜色：
   - 否。`match_rect` 仍是匹配框；模型轮廓单独输出为 `contour_line` / `contour_points`。

## 本次修改

- 移除 PatternPresence Runner 运行结果中的 `template_roi` overlay，避免模板 ROI 在检测图上显示成蓝色大多边形。
- 保留 `detect_roi`，found 时保留匹配中心、匹配框、score。
- `showContourPoints=true` 且 found 时，只输出 HALCON 模型 XLD 变换后的轮廓线/点。
- 如果 HALCON 轮廓变换符号不可用：
  - `showContourPointsRequested=true`
  - `showContourPointsApplied=false`
  - `contourSource="not_available"`
  - `reason="shape model contour transform not implemented"`
  - 不显示假轮廓点。
- payload 增加/规范：
  - `contourSource`
  - `contourPointCount`
  - `contourDisplayPointCount`
  - `contourDisplaySampled`
  - `contourObjectCount`
  - `contourDisplayedObjectCount`
  - `contourLineSegmentCount`
  - `contourMarkerCount`

## ROI 大的处理

没有增加任何“ROI 太大”的错误判断。

模板 ROI 和检测 ROI 仍允许画大；Runner 只对无效 ROI 或小于最小像素尺寸的空 ROI 报错。大 ROI 不作为主要问题解释，也不强制用户框小模板。

## 坐标约定

- HALCON row 对应显示 y。
- HALCON column 对应显示 x。
- `find_shape_model` 在 `detectMat` 裁剪图内运行，所以模型轮廓变换后只加 `detectRoiPixels.x/y` 回到原图坐标。
- 不叠加 `templateRoiPixels.x/y`。
- 不把模板 ROI 边界或检测 ROI 边界当模型轮廓。

## 编译

执行结果：

- `qmake`：通过。
- `make -j$(nproc)`：通过。

仅有一个既有 warning：

- `rotatedRectCorners` defined but not used。
