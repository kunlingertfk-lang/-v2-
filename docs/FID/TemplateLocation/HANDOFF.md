# 模板定位功能交接文档

更新时间：2026-07-20

## 1. 项目与任务背景

这是一个 Qt 5.15.2 + HALCON 20.11.1 的工业视觉工具配置与运行工程。当前主任务是把原先占位的“模板定位”实现成一个完整、独立的视觉工具，打通以下链路：

```text
工具库入口
  -> TemplateLocationDialog
  -> ToolConfig.params
  -> TemplateLocationAdapter
  -> TemplateLocationHalconRunner
  -> ToolResult / payload / overlays
```

用户的明确要求：

- 模板定位必须独立实现，不能复用“图案有无”等其他功能的算法 Runner。
- 核心算法必须使用 HALCON，OpenCV 只能作为图像输入容器或格式桥接。
- 支持多匹配、最大查找数、数量判定、最大重叠率、逐结果得分。
- 支持模板矩形/多边形 ROI，搜索全局/矩形/圆形/多边形 ROI。
- 支持质心和自定义定位点。
- 支持自动/手动对比度和持久模型缓存。
- UI 要能完整显示，搜索图标选中态要明显，模板轮廓与查找轮廓颜色要区分。
- 后续需要与“位置修正”串联，但不能在尚未实现时假装已经可用。

项目级约束见根目录 `AGENTS.md`，接手后必须先读。功能文档入口是 `docs/FID/Function_Docs.md`。

## 2. 已完成内容

### 2.1 独立 HALCON 算法链路

已新增并接入：

- `src/algorithms/location/TemplateLocationHalconRunner.{h,cpp}`
- `src/tooladapters/TemplateLocationAdapter.{h,cpp}`
- `src/TemplateLocationDialog.{h,cpp}`
- `ui/TemplateLocationDialog.ui`
- `smoke/template_location_smoke.{cpp,pro}`
- `smoke/template_location_ui_smoke.{cpp,pro}`

核心 Runner 没有调用 `PatternPresenceHalconRunner`。HALCON 算子链包括：

- 图像桥接：`gen_image1`
- ROI：`crop_rectangle1`、`gen_region_polygon`、`gen_circle`、`reduce_domain`
- 建模：`create_scaled_shape_model`
- 参数回读：`get_shape_model_params`
- 查找：`find_scaled_shape_model`
- 轮廓及位姿：`get_shape_model_contours`、`vector_angle_to_rigid`、`hom_mat2d_scale_local`、`affine_trans_contour_xld`
- 自定义点变换：`affine_trans_point_2d`
- 持久模型：`write_shape_model`、`read_shape_model`
- 清理：`clear_shape_model`、`clear_obj`

已接入 `ToolType::TemplateLocation`、工具库、`ToolsDialog`、`MainWindow`、`ToolEngine` 注册和 `qt_ui_test.pro`。

### 2.2 参数、匹配与判定

已实现：

- 最低得分、角度范围、缩放范围、极性。
- 自动/手动对比度；手动模式校验 `MinContrast < Contrast`。
- 金字塔层级、亚像素、贪婪度、超时。
- 最大查找数 `maxMatches`，范围 1–100。
- 数量判定 `minMatchCount..maxMatchCount`。
- 最大重叠率 `maxOverlap`，UI 使用 0–100%，Adapter 转为 HALCON 的 0.0–1.0。
- 多结果 `matches`，每项包含 X、Y、角度、缩放和得分。
- 顶层 `x/y/angle/angleDeg/scale/score/pose` 固定使用 HALCON 返回的第一个最佳结果，`primaryMatchIndex=0`。

结果语义：

- 找到且数量合格：`success=true, ok=true, status=found`。
- 找到但数量不合格：`success=true, ok=false, status=count_out_of_range`。
- 未找到：正常 NG，`success=true, ok=false, status=not_found`。
- 空图、无基准、无模型、无效 ROI、参数或 HALCON 异常：执行失败并返回稳定错误码。

坐标合同不能改错：`x = HALCON Column`，`y = HALCON Row`；对外角度单位为度。

### 2.3 ROI、定位点和模型生命周期

已实现：

- 模板 ROI：矩形、多边形。
- 搜索 ROI：全局、矩形、圆形、多边形。
- 搜索矩形/圆形/多边形完成一次绘制后保持当前绘制模式，可直接再次绘制并覆盖旧 ROI。
- 定位点：默认模板质心，也可在基准图上选择自定义点。
- 创建/重新创建模板、删除模板。
- 创建模板时显示橙色模板模型轮廓；查找结果使用绿色轮廓、中心十字、序号和得分。
- 修改建模相关参数后模型标记为需要重新创建。

### 2.4 持久模型缓存

缓存位置由 `QStandardPaths::AppLocalDataLocation` 决定，子目录为 `template_location_models/`；若路径不可用则退回系统临时目录。

- `modelCacheKey` 经过 SHA-256 后作为缓存文件名。
- `.shm` 保存 HALCON shape model，`.sha256` 保存模型签名。
- 签名包含基准灰度图、模板 ROI 和建模参数。
- 签名一致时 `read_shape_model` 命中缓存；不一致时重新建模并覆盖缓存。
- 删除模板同时删除两类缓存文件。

### 2.5 UI 已完成的专项调整

- 模板、搜索图标统一使用以下资源：
  - `resources/icons/global-search.svg`
  - `resources/icons/roi-rectangle.svg`
  - `resources/icons/roi-circle.svg`
  - `resources/icons/roi-polygon.svg`
- 搜索顺序固定为：全局、矩形、圆形、多边形；图标组整体靠右。
- 搜索图标 checked 状态使用浅橙底和 3px 深橙边框，并使用对象名 QSS 保证状态明显。
- 删除模板按钮与模板 ROI 图标区域右侧对齐。
- 自动对比度时 Contrast/MinContrast 标签和控件使用明显深灰禁用态。
- 多结果表包含：序号、X、Y、角度、缩放、得分；点击行可突出对应目标。
- 多结果表已改成底部可折叠抽屉：
  - 默认 36px，只显示“匹配结果（N）”。
  - 展开总高度 152px，表格 116px，约显示 3 行，其余滚动查看。
  - 图像区是唯一纵向伸缩区域；结果栏不再把窗口撑出屏幕。
- 最下方运行结果栏保持固定 48px，但内部上下边距已从 Qt 默认值压到 2px，结果文字获得横向 stretch，解决两行结果被底边裁切的问题。

### 2.6 文档

现有文档：

- `docs/FID/TemplateLocation/模板定位UI与交互设计.md`
- `docs/FID/TemplateLocation/template_location_function_implementation.md`
- `docs/FID/TemplateLocation/模板定位与位置修正衔接.md`

功能文档索引已登记在 `docs/FID/Function_Docs.md`。

注意：`模板定位UI与交互设计.md` 开头仍有“定位单个最佳目标”的旧表述，而当前功能已经支持多匹配；后续应改为“支持单个或多个目标，顶层主位姿取最佳结果”，避免文档歧义。

## 3. 已做验证

最近一次验证结果：

- 主工程 qmake/make 成功，已链接 `build/qt_ui_test/bin/qt_ui_test`。
- `template_location_ui_smoke`：全部通过。
- 此前算法 smoke 已覆盖自动/手动对比度、MinContrast 回读、平移/旋转/缩放、未找到、空图、无模型、无效 ROI、参数约束、超时参数传入。
- 此前已回归图案有无的默认对比度模式。

推荐验证命令（优先使用临时影子目录）：

```bash
build_dir=$(mktemp -d /tmp/qt-template-location-XXXXXX)
cd "$build_dir"
/home/tt/Qt/5.15.2/gcc_64/bin/qmake /home/tt/tfk/WorkerSpace/project/V2_615_1628/qt_znxj_v2_work_1920_515/qt_ui_test.pro
make -j$(nproc)
```

UI smoke：

```bash
smoke_dir=$(mktemp -d /tmp/template-location-ui-XXXXXX)
cd "$smoke_dir"
/home/tt/Qt/5.15.2/gcc_64/bin/qmake \
  /home/tt/tfk/WorkerSpace/project/V2_615_1628/qt_znxj_v2_work_1920_515/smoke/template_location_ui_smoke.pro \
  BUILD_ROOT="$smoke_dir/out"
make -j$(nproc)
QT_QPA_PLATFORM=offscreen "$smoke_dir/out/bin/template_location_ui_smoke"
```

算法 smoke 同理使用 `smoke/template_location_smoke.pro`。

## 4. 当前卡点与未完成事项

### 4.1 模板定位本身

没有已知算法阻塞。最新两处 UI 调整已经编译并通过 offscreen UI smoke，但还需要在真实桌面人工确认：

1. 搜索“全局/矩形/圆形/多边形”的浅橙底 + 深橙粗边框是否足够明显。
2. 折叠/展开匹配结果后，48px 运行结果栏的两行文字在真实字体和 DPI 下是否完全可见。
3. 展开抽屉、选择结果行、继续绘制 ROI、基准图测试和连续测试之间是否有视觉或 checked 状态回退。

如果仍有裁切，优先继续减小 `resultLayout` 的上下 margin 或字体行距；不要直接把整个结果栏加到很高，用户明确希望它只占约两行文字高度。

### 4.2 与位置修正的串联（真正的下一阶段）

目前 `PositionCorrectionDialog` 只有来源绑定和配置保存；后端明确显示 `not implemented`。`ToolEngine::runTools` 也没有把前序 `ToolResult` 作为运行上下文传给后序工具。因此现在只能在 UI 中选择模板定位来源，不能真正应用位置修正。

推荐实现顺序：

1. 定义工具链运行上下文，按稳定 `toolId` 保存每个前序 `ToolResult`。
2. 工具链强制顺序为：`模板定位 -> 位置修正 -> 被修正的检测工具`，禁止后向引用。
3. 位置修正保存模板定位在基准图上的 `referencePose={x0,y0,angle0}`。
4. 每帧从模板定位顶层主位姿读取 `runPose={x,y,angle}`。
5. 使用 HALCON `vector_angle_to_rigid` 计算运行位姿到基准位姿的刚性变换。
6. 位置修正输出 `homMat2D`、`deltaX`、`deltaY`、`deltaAngleDeg`、`sourceToolId`、`applied=true`。
7. 下游检测工具读取所选位置修正结果，用 HALCON `affine_trans_region`、`affine_trans_contour_xld` 或适合该 ROI 的 HALCON 变换接口修正检测区域。
8. 增加成功、来源缺失、基准位姿缺失、模板定位 NG、链顺序错误和下游未应用等 smoke。

失败传播必须明确：模板定位失败、未找到或数量判定 NG 时，位置修正返回 `source_invalid`，绝对不能沿用上一帧位姿。

## 5. 绝对不要重复踩的坑

1. **不要复用图案有无算法。** 用户已经明确质疑过 `PatternPresenceHalconRunner().run(...)` 的复用。模板定位必须保持独立 Runner；不要把它重新改成图案有无的包装。
2. **不要用 OpenCV 或自写算法替代 HALCON 核心。** OpenCV 只能做输入容器/桥接；所有匹配、ROI 域、变换必须走 HALCON。
3. **不要声称位置修正已可用。** 当前只是 UI/config 占位，没有运行时上下文和后端。
4. **不要混淆坐标。** 对外 `x=Column`、`y=Row`，角度是度；HALCON 内部角度是弧度。
5. **不要把多结果随机交给位置修正。** 顶层主结果固定为第一最佳结果。未来若要最近邻或编号跟踪，必须显式增加 `primaryMatchStrategy`。
6. **不要沿用上一帧位姿。** 当前帧定位 NG/失败时必须阻断修正，避免设备使用陈旧位置。
7. **不要把结果表直接作为高最小高度控件插入主布局。** 这曾导致图像、结果栏和底部按钮被挤出屏幕。必须保留当前可折叠、有界高度、可滚动的抽屉设计。
8. **不要用 Qt 默认布局边距塞两行文字进 48px。** 默认上下各约 9px，只剩约 30px，第二行会被裁掉；当前已设为上下 2px。
9. **不要只依赖动态 `actionRole` 的 checked QSS。** 动态属性设置后样式刷新不总是可靠；模板定位 ROI 高亮已增加对象名选择器，不要删掉。
10. **不要把自动 Contrast 伪装成可回读数值。** HALCON 20.11 可以回读实际 `MinContrast` 和层级，但不能可靠回读数值型自动 `Contrast`；保持显示 `auto/自动`。
11. **不要忘记建模参数变化会使模型失效。** 角度、缩放、极性、对比度、层级、模板 ROI 变化后必须要求重新创建；搜索 ROI、最低得分等仅查找参数不应无故重建。
12. **不要破坏持久缓存键和签名。** 重新打开必须能命中相同模型；基准图、模板 ROI 或建模参数变化必须使签名失效。
13. **不要让普通按钮触发 Dialog 关闭。** 创建、删除、ROI、测试按钮不能误连 `accept/reject`。
14. **不要在源码目录做新的 in-source 构建。** 优先 `/tmp` 或 `build/` 影子构建，构建产物不应入库。
15. **不要清理、reset 或全量提交当前工作区。** 当前仓库有大量其他功能的用户改动、未跟踪源码和已跟踪构建产物；必须按当前任务逐文件确认范围。

## 6. 工作区特别说明

当前 `git status` 非常脏，包含：

- 模板定位新增文件目前多为未跟踪文件。
- 颜色识别、颜色比较、位置修正、主窗口、参考图等大量其他改动。
- `build/`、根目录 `Makefile`、对象文件、可执行文件和日志等构建产物变化。
- 用户自己的项目方案和图片变化。

这些变化不能假定属于当前任务，也不能擅自删除或回退。接手时：

- 先运行 `git status --short`。
- 只编辑模板定位或下一阶段位置修正明确需要的文件。
- 不要运行 `git reset --hard`、`git checkout --` 或全量清理。
- 不要执行 `git add .` 或把全部工作区一次提交。
- 构建时使用临时影子目录，避免继续污染源码树。

## 7. 新会话建议的第一步

1. 阅读 `AGENTS.md` 和本文件。
2. 阅读 `docs/FID/TemplateLocation/` 下三份文档。
3. 查看 `git status --short`，确认不要覆盖其他功能修改。
4. 启动应用进行最新 UI 人工检查，重点看搜索按钮高亮、匹配结果抽屉和底部两行结果。
5. UI 确认后，不再扩展模板定位；按“模板定位与位置修正衔接”文档先设计并实现 ToolEngine 的前序结果运行上下文。
