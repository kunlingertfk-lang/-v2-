# 注册分类训练窗口 ROI 预览与删除同步设计

## 背景

注册目标检测训练窗口已具备较完整的 ROI 预览页和删除交互：深色预览页、高对比标题、预览卡删除入口、预览卡右键删除，以及大图中右键命中 ROI 后删除当前 ROI。注册分类训练窗口已有多 ROI、类别 ROI 预览页和预览卡右键删除基础能力，但视觉样式和大图删除能力与注册目标检测不一致。

本次只做局部同步，让两个注册训练窗口的 ROI 预览与删除体验一致，不扩大到真实训练、数据集落盘或主分类推理链路。

## 目标

- 注册分类训练窗口 ROI 预览页视觉同步注册目标检测训练窗口。
- ROI 预览页使用深色背景，标题和空态文案使用白色或浅色高对比样式。
- ROI 预览卡删除图标补齐稳定提示：`toolTip`、`statusTip`、`whatsThis` 均为 `删除当前 ROI`。
- ROI 预览卡支持右键菜单，动作名为 `删除当前 ROI`，删除该卡对应的单个 ROI。
- 大图编辑页支持右键命中当前图、当前类别的 ROI，弹出 `删除当前 ROI` 菜单。
- 大图右键删除后刷新大图 overlay、ROI 编号、类别计数、缩略图已标注状态和状态栏。
- 更新注册分类提示词规范，把本次完成能力写入文档，避免仍被描述为后续优化。

## 范围

In:

- 修改 `src/RegisteredClassificationTrainingDialog.cpp`。
- 更新 `smoke/registered_classification_dialog_smoke.cpp` 中与注册分类训练窗口相关的断言。
- 更新 `docs/FID/RegisteredClassification/注册分类提示词规范.md` 的完成状态、交互要求和验证说明。
- 必要时更新 `docs/FID/RegisteredClassification/registered_classification_function_implementation.md` 的阶段记录。

Out:

- 不实现真实 HALCON 训练。
- 不实现真实数据集落盘。
- 不生成或导出训练模型。
- 不修改 `RegisteredClassificationDialog` 主推理配置窗口。
- 不修改 `RegisteredClassificationAdapter` 或 `RegisteredClassificationHalconRunner`。
- 不抽取公共 ROI 预览组件；两个窗口数据结构不同，本次保持局部同步，降低回归风险。

## 交互设计

### ROI 预览页视觉

注册分类训练窗口的 `registeredTrainingPreviewPage` 使用与注册目标检测一致的深色页背景。预览页标题使用 `role="previewPageTitle"` 的 QSS 控制，不使用 inline style。无 ROI 空态使用浅色高对比文案，避免在深色背景下不可读。

ROI 预览卡仍保留现有内容：

- ROI 裁剪图。
- 来源注册图名称。
- ROI 编号。
- 删除当前 ROI 图标。

删除图标必须是可读的无文字控件，设置 `toolTip/statusTip/whatsThis` 为 `删除当前 ROI`。

### 预览卡删除

每个 ROI 预览卡支持两种删除入口：

- 点击卡片右上角删除图标。
- 右键卡片、裁剪图或标题区域，弹出菜单并点击 `删除当前 ROI`。

删除后只移除该卡对应的单个 ROI，并刷新：

- ROI 预览卡列表。
- 类别目标总数和图像总数。
- 缩略图已标注状态。
- 状态栏。

删除最后一个 ROI 后，预览页保持打开并显示空态。

### 大图右键删除

注册分类大图编辑页只处理当前注册图和当前类别的 ROI。右键事件进入命中检测：

- 若无注册图，状态栏提示 `请先添加注册图`。
- 若右键位置未命中当前类别 ROI，状态栏提示 `请右键目标 ROI`。
- 若命中 ROI，记录当前图、当前类别和 ROI 索引，弹出菜单 `删除当前 ROI`。
- 菜单触发后删除命中的单个 ROI。

命中检测规则：

- 矩形和全屏 ROI 使用归一化矩形包含判断。
- 多边形 ROI 使用归一化多边形包含判断。
- 多个 ROI 重叠时，从后向前命中，优先删除视觉上更晚添加的 ROI。

删除后刷新：

- 大图 overlay。
- ROI 编号。
- 当前类别行统计。
- 缩略图已标注状态。
- 状态栏文案。

## 数据与状态

沿用现有会话内数据结构：

- `TrainingImageState::marksByClass`
- `TrainingRoiMark`
- `TrainingSessionState::currentImage`
- `TrainingSessionState::currentClass`

可在 `TrainingSessionState` 内增加用于大图右键删除的临时选中索引，例如：

- `selectedRoiImage`
- `selectedRoiClass`
- `selectedRoiIndex`

这些字段只表示当前右键命中的 ROI，不写入工具配置，也不参与真实训练数据。

## 错误处理

- 无图时不弹空菜单，只显示清晰状态提示。
- 未命中 ROI 时不删除任何内容。
- ROI 索引过期、图片索引过期或类别索引过期时直接忽略删除动作，并保持窗口不崩溃。
- 删除后若当前处于预览页，刷新预览页；若处于大图页，刷新当前图。

## 验证

自动化 smoke 覆盖：

- ROI 预览页对象存在并使用深色页背景 QSS。
- 预览页标题依赖 role QSS，不使用 inline style。
- 删除 ROI 图标存在，且 `toolTip/statusTip` 为 `删除当前 ROI`。
- 预览卡右键菜单包含 `删除当前 ROI`，触发后只删除对应 ROI。
- 大图右键命中当前类别 ROI 后弹出 `删除当前 ROI` 菜单。
- 大图右键删除后类别计数更新为预期值。

工程验证：

- `git diff --check`
- `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro`
- `make -j$(nproc)`
- 相关 smoke：`smoke/registered_classification_dialog_smoke`

## 文档更新

实现完成后更新 `docs/FID/RegisteredClassification/注册分类提示词规范.md`：

- 将“大图 overlay 点击/右键选中 ROI 删除”从后续优化改为已完成能力。
- 记录 ROI 预览页深色视觉同步已完成。
- 记录预览卡删除图标 tooltip 和右键菜单已完成。
- 在验证项中加入预览页样式、预览卡右键删除和大图右键删除。

若实现过程中同步更新阶段记录，可补充 `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`，但不得把临时调试过程写入根目录 `AGENTS.md`。
