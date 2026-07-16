# 颜色比较模板/检测屏蔽区独立实施计划

> **执行要求：** 按任务顺序实施，每个任务先补失败测试，再完成最小实现并回归。不得夹带颜色识别或当前工作树中的其他改动。

**目标：** 让模板 Mask 只作用于模板、检测 Mask 只作用于检测；“与检测区域同步”仅复用检测 ROI 几何。同时让模板 ROI、模板 Mask、检测 ROI、检测 Mask 在配置视图中可同时核对，并完善清除、重画和已有 Mask 调整交互。

**架构：** Runner 继续使用统一 createEffectiveRegion()，但根据 templateRegion 严格选择 owner 对应的唯一 Mask。模板提取哈希加入固定 ownership 合同，使旧模型自动 stale。Dialog 将几何显示拆为“持久配置 overlay 层”和“单一活动编辑层”：前者负责同时显示四类已保存几何，后者继续由 FrameViewHelper 处理鼠标绘制和顶点调整。测试帧只组合检测组与运行结果 overlay。

**技术栈：** C++17、Qt 5.15.2 Widgets/Graphics View、HALCON 20.11 动态 C API、OpenCV 仅作输入容器、qmake、现有 smoke。

## 全局约束

- 固定合同：

  ~~~text
  模板有效区域 = 模板基础几何 - 模板 Mask
  检测有效区域 = 检测基础几何 - 检测 Mask
  ~~~

- sync 只同步矩形、圆形或全图几何，不同步 detectMaskPolygon。
- 不改变颜色比较 V2 顶层版本、32×32 H/S 特征、V 32 bins、评分、亮度补偿和阈值语义。
- 不修改颜色识别 GMM 文件。
- 不把 ROI、Mask、标签或结果 overlay 写入 QImage/cv::Mat；Runner 始终接收原始帧。
- FrameViewHelper 新增的显示提示必须默认不改变其他 Dialog。
- 每侧仍只支持一个 Mask 多边形；多个离散 Mask 不在本轮范围。
- 当前工作树包含大量未提交变更。每次提交只暂存任务明确列出的源码、测试和文档，禁止 reset、checkout 或批量清理。
- smoke/、docs/ 受忽略规则影响时，只对明确文件使用 git add -f，禁止强制添加整个目录。
- 所有 HALCON 数值测试先 source scripts/dependencies.env，并显式区分默认 preflight 与 RUN_HALCON_LICENSED_SMOKE=1。

---

## Task 1：用 HALCON 回归锁定 Mask owner 独立合同

**文件：**

- 修改：smoke/color_comparison_smoke.cpp
- 参考：smoke/color_comparison_smoke.pro
- 参考：src/algorithms/recognition/ColorComparisonHalconRunner.h

- [ ] **Step 1：增加可区分四个区域的固定测试图**

在 color_comparison_smoke.cpp 增加确定性的 BGR 测试图构造器。图像至少包含四个颜色明显不同的象限，使模板 Mask 或检测 Mask 扣除不同区域时，effectivePixelCount 和 H/S 分布都会发生可观测变化。

测试几何使用非退化归一化多边形，并保证 Mask 与基础 ROI 有实际交集。

- [ ] **Step 2：写入 sync 模式检测 Mask 不影响模板的失败测试**

使用相同参考图和 sync 基础矩形分别构建两个模板：

~~~text
build A: templateMask 固定，detectMask = polygon A
build B: templateMask 固定，detectMask = polygon B
~~~

断言：

- 两次 build 均成功；
- model.values 完全一致或在严格浮点容差内一致；
- model.valueHistogram 一致；
- effectivePixelCount 一致；
- extractParamsHash 一致；
- payload.extractParams 不包含 syncDetectionMaskPolygon；
- payload.extractParams.maskOwnershipContract 等于 independent_template_and_detection_v1。

当前实现会因模板分支扣除检测 Mask 且哈希包含检测 Mask而失败，确认 RED。

- [ ] **Step 3：写入模板 Mask 不影响检测的失败测试**

先用无模板 Mask 的 sync 配置建立可运行模型，再构造两次运行：

~~~text
run A: detectMask 固定，templateMask = polygon A
run B: detectMask 固定，templateMask = polygon B
~~~

确保模型的 expected extract hash与当前配置一致后，断言两次检测：

- measurementValid 均为 true；
- effectiveDetectionPixels 一致；
- detectFeature 和 detectValueHistogram 一致；

模板 Mask 的改变会改变各自模板模型，因此 rawIntersection 和 score 可以不同；本测试只锁定检测侧提取结果不变，不能错误断言最终比较分数一致。

- [ ] **Step 4：保留 owner 独立错误状态测试**

增加或确认以下断言：

- 模板 Mask 完全覆盖模板基础区域：template_masked_empty；
- 检测 Mask 完全覆盖检测基础区域：detect_masked_empty；
- 退化模板多边形：invalid_template_mask；
- 退化检测多边形：invalid_detect_mask。

- [ ] **Step 5：构建并确认 RED**

运行：

~~~bash
source scripts/dependencies.env
mkdir -p build/smoke/color_comparison
/home/tt/Qt/5.15.2/gcc_64/bin/qmake \
  smoke/color_comparison_smoke.pro \
  -o build/smoke/color_comparison/Makefile
make -C build/smoke/color_comparison -j"$(nproc)"
RUN_HALCON_LICENSED_SMOKE=1 \
  build/smoke/color_comparison/bin/color_comparison_smoke
~~~

预期：编译成功，新增的 sync Mask 独立断言失败；不得通过删除 detectMask 测试数据或放宽到无意义容差制造 GREEN。

- [ ] **Step 6：提交测试**

~~~bash
git add -f smoke/color_comparison_smoke.cpp
git commit -m "test: define independent color comparison masks"
~~~

---

## Task 2：修正 Runner Region 组合和模板提取哈希

**文件：**

- 修改：src/algorithms/recognition/ColorComparisonHalconRunner.cpp
- 验证：smoke/color_comparison_smoke.cpp
- 验证：smoke/color_comparison_feature_diagnostics_smoke.cpp

- [ ] **Step 1：固定模板提取参数合同**

修改 templateExtractParams()：

- 删除 syncDetectionMaskPolygon；
- 始终保留 templateMaskPolygon；
- 增加：

  ~~~text
  maskOwnershipContract = independent_template_and_detection_v1
  ~~~

- templateGeometry 在 custom 时仍取 templateRoiNormalized，在 sync 时仍取 detectGeometry。

不要把 detectMaskPolygon 或任何检测 Mask 派生值放入模板提取参数。

- [ ] **Step 2：简化 createEffectiveRegion() 的 Mask 选择**

保留现有基础几何选择，删除 sync 模板分支提前扣除 detectMaskPolygon 的逻辑。

统一为：

~~~cpp
const QVector<QPointF> &ownerMask = templateRegion
        ? config.templateMaskPolygonNormalized
        : config.detectMaskPolygonNormalized;
~~~

只对 ownerMask 执行一次 HALCON difference。删除不再需要的 additionalMaskRegion、additionalDifferenceRegion 参数和局部对象，避免残留“双重 Mask”入口。

- [ ] **Step 3：确认模型兼容行为**

保持颜色比较顶层 version=2。依赖新的 extractParamsHash 让旧模型产生 model_stale/model_rebuild_required，不增加静默迁移或沿用旧哈希的兼容分支。

确认 build payload 输出新 extractParams，运行阶段使用同一 templateExtractParams() 计算 expectedExtractParamsHash。

- [ ] **Step 4：运行 licensed GREEN**

重复 Task 1 命令，预期全部通过。随后运行诊断 smoke：

~~~bash
source scripts/dependencies.env
mkdir -p build/smoke/color_comparison_feature_diagnostics
/home/tt/Qt/5.15.2/gcc_64/bin/qmake \
  smoke/color_comparison_feature_diagnostics_smoke.pro \
  -o build/smoke/color_comparison_feature_diagnostics/Makefile
make -C build/smoke/color_comparison_feature_diagnostics -j"$(nproc)"
RUN_HALCON_LICENSED_SMOKE=1 \
  build/smoke/color_comparison_feature_diagnostics/bin/color_comparison_feature_diagnostics_smoke
~~~

预期：Mask owner 独立、现有亮度补偿、连续评分和直方图诊断全部通过。

- [ ] **Step 5：提交 Runner 修正**

~~~bash
git add src/algorithms/recognition/ColorComparisonHalconRunner.cpp
git commit -m "fix: isolate template and detection masks"
~~~

---

## Task 3：拆分检测几何与检测 Mask 的模型失效策略

**文件：**

- 修改：smoke/color_comparison_dialog_integration_smoke.cpp
- 修改：src/ColorComparisonDialog.h
- 修改：src/ColorComparisonDialog.cpp

- [ ] **Step 1：写入 Dialog 生命周期 RED 测试**

在 integration smoke 中准备 Ready 模型和 sync 模式，分别直接触发检测几何与检测 Mask 更新。

断言：

- sync 模式修改检测矩形或圆形后模型变为 Stale；
- sync 模式修改检测 Mask 后模型保持 Ready；
- custom 模式修改检测几何或检测 Mask 后模型保持 Ready，只增加 test generation；
- 修改模板 Mask 后无论 custom/sync 都进入 Stale；
- 清除检测 Mask 同样不使 sync 模板 stale；
- 清除模板 Mask 必须使模板 stale。

当前 handleDetectionConfigChanged() 在 sync 模式对所有 reason 统一 markModelStale，新增检测 Mask 断言应失败。

- [ ] **Step 2：拆分变更入口**

在 Dialog 中增加职责明确的方法：

~~~cpp
void handleDetectionGeometryChanged(const QString &reason);
void handleDetectionMaskChanged(const QString &reason);
~~~

规则：

- handleDetectionGeometryChanged()：sync 时 markModelStale() 并更新模板预览；custom 时只 invalidateAsyncWork()。
- handleDetectionMaskChanged()：始终只 invalidateAsyncWork()，不读取 templateRegionMode。
- 两者在 live test 中都触发 rerunLiveComparison()。
- 检测矩形、圆形、全图调用 geometry 入口。
- 检测 Mask 绘制和清除调用 mask 入口。

删除或收敛旧 handleDetectionConfigChanged()，避免后续调用方再次混淆 owner。

- [ ] **Step 3：运行 Dialog GREEN**

~~~bash
source scripts/dependencies.env
mkdir -p build/smoke/color_comparison_dialog_integration
/home/tt/Qt/5.15.2/gcc_64/bin/qmake \
  smoke/color_comparison_dialog_integration_smoke.pro \
  -o build/smoke/color_comparison_dialog_integration/Makefile
make -C build/smoke/color_comparison_dialog_integration -j"$(nproc)"
QT_QPA_PLATFORM=offscreen \
  build/smoke/color_comparison_dialog_integration/bin/color_comparison_dialog_integration_smoke
~~~

- [ ] **Step 4：提交生命周期修正**

~~~bash
git add src/ColorComparisonDialog.h src/ColorComparisonDialog.cpp
git add -f smoke/color_comparison_dialog_integration_smoke.cpp
git commit -m "fix: separate color comparison mask invalidation"
~~~

---

## Task 4：为持久几何 overlay 增加受控显示提示

**文件：**

- 修改：src/frame/FrameViewHelper.cpp
- 修改：smoke/frame_view_helper_navigation_smoke.cpp
- 不修改：src/toolcore/ToolOverlay.h

**接口合同：**

继续使用 ToolOverlay.extra，不扩展公共结构。FrameViewHelper 只识别以下可选显示提示，未提供时保持现有行为：

~~~text
displayRole = color_template_roi | color_template_mask |
              color_detect_roi | color_detect_mask
emphasis = normal | muted | active
clipGeometry = {
  type: rect | circle,
  rect: { x, y, width, height } 或
  center: { x, y },
  radius: number
}
~~~

ToolOverlay.points 和 clipGeometry 均使用原图像素坐标；归一化配置只在 Dialog 构建 overlay 时转换一次，视图缩放和平移不得改写这些值。

- [ ] **Step 1：写入 overlay 显示 RED 测试**

在 frame_view_helper_navigation_smoke.cpp 构造模板 ROI、检测圆、模板 Mask 和检测 Mask ToolOverlay，调用 setToolOverlays()。

通过 QGraphicsScene items 断言：

- 四种 displayRole 得到不同 owner 颜色；
- muted item 的 opacity 明显低于 normal；
- active item 的 pen 宽度更大；
- Mask 使用斜纹或半透明 brush；
- 带 rect clipGeometry 的 Mask 有效填充路径不超出矩形；
- 带 circle clipGeometry 的 Mask 有效填充路径不超出圆；
- Mask 超出 owner ROI 的部分由 FrameViewHelper 自动生成红色虚线且不填充；
- 不带新 extra 的既有 overlay 颜色、opacity 和类型保持原样。

- [ ] **Step 2：实现受控样式解析**

在 FrameViewHelper.cpp 匿名命名空间中增加纯辅助函数：

- overlayDisplayRole()
- overlayEmphasis()
- overlayPen()
- overlayBrush()
- overlayOpacity()
- overlayClipPath()

只允许固定枚举字符串，不解析任意外部颜色值，避免 payload 任意控制全局显示。

颜色固定为：

- template ROI：橙色；
- template Mask：橙红色；
- detect ROI：青色；
- detect Mask：蓝色；
- Mask ROI 外轮廓：红色虚线。

- [ ] **Step 3：用 QPainterPath 绘制带 clip 的 Mask**

Polygon overlay 没有 clipGeometry 时继续走现有 QGraphicsPolygonItem。

存在合法 clipGeometry 时：

1. 建立 polygonPath；
2. 建立 rectPath 或 circlePath；
3. effectivePath = polygonPath.intersected(ownerPath)；
4. 使用 QGraphicsPathItem 绘制有效填充；
5. outsidePath = polygonPath.subtract(ownerPath)，非空时由 FrameViewHelper 在同一次渲染中增加红色虚线、不填充的 QGraphicsPathItem。

clip 只改变显示路径，不修改 ToolOverlay.points 和保存配置。

- [ ] **Step 4：运行公共视图 GREEN**

~~~bash
source scripts/dependencies.env
mkdir -p build/smoke/frame_view_helper_navigation
/home/tt/Qt/5.15.2/gcc_64/bin/qmake \
  smoke/frame_view_helper_navigation_smoke.pro \
  -o build/smoke/frame_view_helper_navigation/Makefile
make -C build/smoke/frame_view_helper_navigation -j"$(nproc)"
QT_QPA_PLATFORM=offscreen \
  build/smoke/frame_view_helper_navigation/bin/frame_view_helper_navigation_smoke
~~~

- [ ] **Step 5：提交公共显示能力**

~~~bash
git add src/frame/FrameViewHelper.cpp
git add -f smoke/frame_view_helper_navigation_smoke.cpp
git commit -m "feat: support owned mask overlay styling"
~~~

---

## Task 5：在 Dialog 构建四类持久配置 overlay

**文件：**

- 修改：src/ColorComparisonDialog.h
- 修改：src/ColorComparisonDialog.cpp
- 修改：smoke/color_comparison_dialog_integration_smoke.cpp

- [ ] **Step 1：写入配置 overlay RED 测试**

通过 integration smoke 直接设置模板矩形、模板 Mask、检测矩形/圆和检测 Mask，调用新的纯构建方法，断言：

- 空闲配置态同时返回模板 ROI、模板 Mask、检测 ROI、检测 Mask；
- custom 模式模板 ROI 使用 m_templateRoi；
- sync 模式模板 ROI 使用检测矩形或圆形，而不是 m_templateRoi；
- template Mask 的 clipGeometry 永远指向模板基础几何；
- detect Mask 的 clipGeometry 永远指向检测基础几何；
- 编辑模板组时模板 overlay 为 active/normal、检测组为 muted；
- 编辑检测组时规则相反；
- 测试态构建结果不包含模板 ROI 和模板 Mask；
- Mask 超出 owner ROI 时保留原始 points 和 clipGeometry，由 FrameViewHelper 渲染 ROI 外红色虚线；
- overlay 构建不修改任何归一化几何。

- [ ] **Step 2：增加纯几何 overlay 构建方法**

在 Dialog 中增加：

~~~cpp
QVector<ToolOverlay> configurationGeometryOverlays() const;
QVector<ToolOverlay> detectionGeometryOverlays() const;
void refreshGeometryOverlays();
QVector<ToolOverlay> combinedDisplayOverlays() const;
~~~

同时保存最近一次运行结果 overlay：

~~~cpp
QVector<ToolOverlay> m_runtimeResultOverlays;
~~~

规则：

- 配置/基准图：configurationGeometryOverlays() 返回四类已配置几何。
- 测试/当前图：detectionGeometryOverlays() 只返回检测 ROI 和检测 Mask。
- combinedDisplayOverlays() 合并当前语境的几何 overlay 与 m_runtimeResultOverlays。
- setToolOverlays() 只在 combinedDisplayOverlays() 中调用，避免 showFrameImage()、测试完成和编辑刷新互相清空图层。

- [ ] **Step 3：保留活动编辑层而不清空持久层**

调整 setEditState() 和 refreshRoiOverlay()：

- 不再用 clearToolOverlays() 清除配置几何；
- ROI/circle/polygon 交互图元只表示当前活动对象；
- 非活动几何始终由持久 ToolOverlay 层显示；
- 活动编辑对象在持久层标为 active，同时交互图元位于更高 z 值；
- 编辑 Mask 时 owner ROI 必须继续显示；
- EditState::None 时清空交互图元，但立即刷新持久配置层。

- [ ] **Step 4：处理同步重合 ROI**

sync 模式仍分别生成 T 和 D overlay。两者几何相同但 displayRole、标签和线型不同；不得为了减少图元把它们合并。

标签固定为 T、T-Mask、D、D-Mask，避免依赖颜色作为唯一识别手段。

- [ ] **Step 5：运行 Dialog GREEN**

重复 Task 3 的 integration smoke 命令，确认四层显示、编辑降权、测试态过滤和原始缩略图像素断言全部通过。

- [ ] **Step 6：提交持久 overlay**

~~~bash
git add src/ColorComparisonDialog.h src/ColorComparisonDialog.cpp
git add -f smoke/color_comparison_dialog_integration_smoke.cpp
git commit -m "feat: retain color comparison geometry overlays"
~~~

---

## Task 6：统一 Mask 编辑、重画、清除和同步模式控件

**文件：**

- 修改：src/ColorComparisonDialog.h
- 修改：src/ColorComparisonDialog.cpp
- 修改：styles/app.qss（仅在现有公共角色不足时）
- 修改：smoke/color_comparison_dialog_integration_smoke.cpp

- [ ] **Step 1：写入控件与状态 RED 测试**

integration smoke 至少断言：

1. custom 模式模板矩形绘制入口可用；
2. sync 模式模板矩形入口保持可见但禁用；
3. sync 提示明确包含“ROI 跟随检测区域”和“屏蔽区独立”；
4. 模板/检测 Mask 图标在空闲态可见，第一次点击进入，第二次点击退出；
5. 已有 Mask 进入编辑后可以拖动顶点，但不会自动开始覆盖草稿；
6. 无 Mask 进入编辑后直接开始多边形绘制；
7. 点击“重画”开始新草稿，但在有效 polygonChanged 前保留旧 Mask；
8. 无效草稿或 Esc 不清空旧 Mask；
9. 清除模板 Mask 保留模板 ROI并使模型 stale；
10. 清除检测 Mask 保留检测 ROI、不使 sync 模板 stale；
11. 任一时刻最多一个绘制图标 checked。

- [ ] **Step 2：增加控件**

为模板和检测 Mask 分别增加：

- 始终可见的可切换多边形图标；
- “重画”按钮；
- “清除”按钮；
- 保留“完成”按钮作为显式退出入口。

移除“编辑按钮是唯一入口”的旧显隐逻辑。普通按钮继续使用现有 role/actionRole，不添加字体和颜色内联 QSS。

为模板区域增加同步说明 QLabel，并在 templateRegionMode 改变和配置回显后刷新。

- [ ] **Step 3：实现已有 Mask 编辑与安全重画**

增加状态：

~~~cpp
QVector<QPointF> m_maskBeforeRedraw;
bool m_maskRedrawInProgress = false;
~~~

由于任一时刻只有一个 EditState 活动，redraw owner 由当前 m_editState 唯一确定。规则如下：

- 进入已有 Mask 编辑：把保存多边形放入 FrameViewHelper，保持 polygon drawing disabled，使现有顶点和整体拖动逻辑生效。
- 进入空 Mask 编辑：直接启用 polygon drawing。
- 点击重画：备份旧 Mask，清空当前草稿显示但不清空保存字段，启用 polygon drawing。
- 收到有效 polygonChanged：替换 owner Mask，结束 redraw 标志，保持 EditState 但回到顶点调整。
- Esc、退出绘制或无效草稿：恢复旧 Mask。
- 不再在 handlePolygonChanged() 后自动 setPolygonDrawingEnabled(true)。

“保持活动状态允许连续重绘”解释为编辑状态持续存在；再次覆盖已有 Mask 必须显式点击重画，防止第一次点击误覆盖。

- [ ] **Step 4：实现清除操作**

增加 clearTemplateMask() 和 clearDetectionMask()：

- 模板：清空 m_templateMask，取消草稿/交互多边形，markModelStale("template_mask_cleared")，刷新模板预览和持久 overlay。
- 检测：清空 m_detectMask，取消草稿/交互多边形，handleDetectionMaskChanged("detect_mask_cleared")，刷新持久 overlay。
- 两者均不修改 ROI、圆、全图状态或另一方 Mask。
- 空 Mask 再次点击清除应为幂等操作。

- [ ] **Step 5：实现 sync 控件约束**

templateRegionMode 改变、配置加载和 UI 初始化时统一调用 refreshTemplateRegionControls()：

- sync：退出 TemplateRect 编辑、禁用模板矩形图标，显示同步说明；
- custom：恢复模板矩形图标可用；
- 两种模式都允许模板 Mask 编辑；
- 切换模式继续使模板模型 stale。

- [ ] **Step 6：运行 Dialog GREEN 和 QSS 检查**

重复 integration smoke。若修改 QSS，再运行：

~~~bash
QT_QPA_PLATFORM=offscreen timeout 4s \
  build/verify_ui/out/bin/qt_ui_test > /tmp/color-comparison-mask-ui.log 2>&1
test $? -eq 124
! rg -n "stylesheet|parse error|Unknown property" \
  /tmp/color-comparison-mask-ui.log
~~~

- [ ] **Step 7：提交交互修正**

~~~bash
git add src/ColorComparisonDialog.h src/ColorComparisonDialog.cpp
git add -f smoke/color_comparison_dialog_integration_smoke.cpp
git add styles/app.qss  # 仅实际修改时
git commit -m "feat: complete color comparison mask editing"
~~~

---

## Task 7：保证测试帧 overlay 语境和异步刷新正确

**文件：**

- 修改：src/ColorComparisonDialog.cpp
- 修改：smoke/color_comparison_dialog_integration_smoke.cpp

- [ ] **Step 1：写入测试语境 RED 回归**

覆盖：

- 配置态基准图显示四类配置几何；
- runReferenceTest/runTest/连续检测进入当前帧后不显示模板组；
- 有效结果合并检测几何、检测 Mask、Runner ROI、结果文字时不重复显示两份检测轮廓；
- 无效结果清除旧 runtime result，但保留当前检测配置几何；
- 旧 generation 结果不能恢复模板 overlay 或覆盖新帧；
- exitTestMode() 恢复基准图四类配置几何；
- showPreviewImage()/showFrameImage() 不清空随后应恢复的持久几何；
- 模板缩略图逐像素不包含任何 overlay 色。

- [ ] **Step 2：定义结果合并去重规则**

Runner runtime overlays 继续作为 ToolResult 合同输出，但 Dialog 显示时：

- 配置几何层负责检测 ROI/Mask；
- runtime 层只保留结果文字以及配置层没有表达的结果图元；
- 根据 label/extra.role 对 detect_roi、detect_mask 去重；
- 无效测量清空 m_runtimeResultOverlays，不清空检测配置层；
- 离开测试态清空 runtime 层并恢复 configurationGeometryOverlays()。

不修改 Runner payload 和 ToolResult.overlays，仅调整 Dialog 显示组合。

- [ ] **Step 3：运行 integration GREEN**

重复 Task 3 命令，确认异步 generation、无效结果清理和缩略图像素回归通过。

- [ ] **Step 4：提交测试语境修正**

~~~bash
git add src/ColorComparisonDialog.cpp
git add -f smoke/color_comparison_dialog_integration_smoke.cpp
git commit -m "fix: separate configuration and runtime overlays"
~~~

---

## Task 8：完整构建、回归和文档收口

**文件：**

- 修改：docs/FID/ColorComparison/颜色比较检测特征可视化设计.md
- 修改：docs/FID/ColorComparison/color_comparison_function_implementation.md
- 修改：docs/FID/ColorComparison/颜色比较交接说明.md
- 参考：docs/FID/Function_Docs.md
- 不修改：颜色识别文档

- [ ] **Step 1：运行全部颜色比较回归**

~~~bash
source scripts/dependencies.env

for name in \
  frame_view_helper_navigation \
  color_comparison_feature_view \
  color_comparison_dialog_integration \
  color_comparison_feature_diagnostics \
  color_comparison
do
  mkdir -p "build/smoke/$name"
  /home/tt/Qt/5.15.2/gcc_64/bin/qmake \
    "smoke/${name}_smoke.pro" \
    -o "build/smoke/$name/Makefile"
  make -C "build/smoke/$name" -j"$(nproc)"
done

QT_QPA_PLATFORM=offscreen \
  build/smoke/frame_view_helper_navigation/bin/frame_view_helper_navigation_smoke

QT_QPA_PLATFORM=offscreen \
  build/smoke/color_comparison_feature_view/bin/color_comparison_feature_view_smoke

QT_QPA_PLATFORM=offscreen \
  build/smoke/color_comparison_dialog_integration/bin/color_comparison_dialog_integration_smoke

build/smoke/color_comparison_feature_diagnostics/bin/color_comparison_feature_diagnostics_smoke

RUN_HALCON_LICENSED_SMOKE=1 \
  build/smoke/color_comparison_feature_diagnostics/bin/color_comparison_feature_diagnostics_smoke

RUN_HALCON_LICENSED_SMOKE=1 \
  build/smoke/color_comparison/bin/color_comparison_smoke
~~~

所有 binary 必须在本轮源码下重新 qmake/make，不能直接把旧 binary 通过作为证据。

- [ ] **Step 2：运行主工程影子构建**

~~~bash
source scripts/dependencies.env
mkdir -p build/verify_ui
/home/tt/Qt/5.15.2/gcc_64/bin/qmake \
  qt_ui_test.pro \
  BUILD_ROOT="$PWD/build/verify_ui/out" \
  -o build/verify_ui/Makefile
make -C build/verify_ui -j"$(nproc)"
~~~

预期 qmake、编译和链接 exit 0。

- [ ] **Step 3：运行 offscreen 启动检查**

~~~bash
QT_QPA_PLATFORM=offscreen timeout 4s \
  build/verify_ui/out/bin/qt_ui_test > /tmp/color-comparison-mask-ui.log 2>&1
test $? -eq 124
! rg -n "stylesheet|parse error|Unknown property|ASSERT|Segmentation" \
  /tmp/color-comparison-mask-ui.log
~~~

timeout 124 只表示应用按预期保持运行；日志不得包含 QSS 解析、断言或崩溃。

- [ ] **Step 4：执行 Ubuntu 手动验收**

逐项记录：

- custom/sync 模式模板 ROI 行为；
- 四类几何同时显示及重合时的 T/D 标签；
- 编辑 Mask 时所属 ROI 可见；
- 当前组高亮、另一组降权；
- 已有 Mask 顶点和整体调整；
- 重画、Esc、完成、再次点击退出；
- 清除模板/检测 Mask 后的模型状态；
- 放大、平移后的坐标稳定；
- 测试态只显示检测组和结果；
- 模板缩略图无 overlay 像素；
- 较低窗口高度下按钮不被遮挡。

未实际执行的人工项必须标记未执行，不得写成通过。

- [ ] **Step 5：更新颜色比较文档**

文档必须统一为：

~~~text
sync 只同步 ROI 几何
templateMask 只作用模板
detectMask 只作用检测
~~~

更新实施记录中的实际命令和结果；交接文档删除旧的“sync 模板扣除检测 Mask”描述，并登记新模型需重新取样。

公共 Function_Docs 已有原图/overlay 分离和 toggle 合同。只有新增了跨工具通用规则时才修改它；颜色比较专用颜色、标签和 Mask owner 语义留在颜色比较文档。

- [ ] **Step 6：检查提交范围**

~~~bash
git diff --check
git status --short
git diff --name-only
~~~

确认没有构建产物、日志、颜色识别源码、工程样例或用户项目数据进入本功能提交。

- [ ] **Step 7：提交文档和验证记录**

~~~bash
git add -f \
  docs/FID/ColorComparison/颜色比较检测特征可视化设计.md \
  docs/FID/ColorComparison/color_comparison_function_implementation.md \
  docs/FID/ColorComparison/颜色比较交接说明.md
git commit -m "docs: record independent color comparison masks"
~~~

## 完成标准

- sync 模式只同步 ROI 几何，检测 Mask 不进入模板 Region 或模板哈希。
- 模板/检测 Mask 各自只改变 owner 的有效像素和直方图。
- 旧提取合同模型明确 stale，并要求重新取样。
- 配置态可同时核对模板 ROI、模板 Mask、检测 ROI、检测 Mask。
- 编辑 Mask 时 owner ROI 始终可见，另一组降权但不消失。
- 已有 Mask 可调整、重画和清除；无效草稿不丢失旧值。
- 测试态只显示检测组及结果，退出后恢复配置几何。
- overlay 不污染缩略图或 HALCON 输入。
- licensed HALCON smoke、Qt offscreen smoke和主工程影子构建全部通过。
- 文档只记录颜色比较，不混入未完成的颜色识别。
