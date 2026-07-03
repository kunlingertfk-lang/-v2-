# FID 功能公有约束和规范

本文档记录 FID 目录下各功能算子开发的公有约束。后续所有 FID 子功能提示词规范、实现计划、实现记录和代码开发，都必须同时遵守根目录 `AGENTS.md` 和本文档。

## 功能文档索引

- 颜色识别：`docs/FID/ColorRecognition/颜色识别提示词规范.md`、`docs/FID/ColorRecognition/color_recognition_function_implementation.md`
- 颜色比较：`docs/FID/ColorComparison/颜色比较提示词规范.md`、`docs/FID/ColorComparison/color_comparison_function_implementation.md`
- 注册分类：`docs/FID/RegisteredClassification/注册分类提示词规范.md`、`docs/FID/RegisteredClassification/registered_classification_function_implementation.md`

## 约束优先级

1. 根目录 `AGENTS.md` 是项目最高约束。
2. `docs/FID/Function_Docs.md` 是 FID 功能公有约束。
3. 各子目录功能文档只记录单功能需求、截图、字段、实现状态和验证记录。

若子功能文档与 `AGENTS.md` 或本文档冲突，以 `AGENTS.md` 和本文档为准。

## 通用开发链路

FID 功能算子应保持完整闭环：

```text
ToolLibraryDialog / ToolsDialog
        |
        v
功能配置 Dialog
        |
        v
ToolConfig.params / judgeRule
        |
        v
功能 Adapter
        |
        v
HALCON Runner
        |
        v
ToolResult / overlays / payload
```

新增功能或修复功能时，应优先保持工具可见、新建、保存、重新打开回显、测试运行、结果显示和异常输入不崩溃。

## 算法约束

- 视觉核心算法必须使用 HALCON 已有算子、类或过程实现。
- OpenCV 只允许作为图像输入容器、显示、采集或格式桥接，不允许作为新算子核心算法。
- HALCON runtime、license 或符号缺失时，必须返回明确错误，不允许静默降级。
- Runner 必须使用现有 HALCON runner 形态，参考 OCR、颜色识别、有无工具等已有实现。

## UI 和 ROI 约束

- 对话框结构优先沿用现有工具：顶部标题栏、左侧参数区、右侧图像预览区、底部按钮。
- 左侧参数区优先使用 `基础 / 全部` 分段、配置卡片、行标题和现有控件样式。
- ROI、屏蔽区、模板区等互斥操作必须使用单一编辑状态或 `QButtonGroup` 管理，避免同时编辑多个目标。
- 点击普通操作按钮不得误触发 `accept`、`reject` 或程序退出。
- ROI 数据必须保存、回显，并在测试运行时通过 overlays 或 payload 给出可定位信息。

## 测试运行统一规范

测试运行是 FID 功能算子的公共交互能力。支持测试运行的功能 Dialog 应统一区分编辑态和测试态，避免按钮语义混乱。

### 编辑态按钮

编辑态底部按钮应按以下语义提供：

```text
基准图测试 / 测试运行 / 完成
```

- `基准图测试`：使用已设置的基准图执行一次测试。
- `测试运行`：进入测试态，并默认启动连续运行。
- `完成`：保存当前配置并关闭配置 Dialog。

### 测试态按钮

测试态底部按钮应按以下语义提供：

```text
停止运行 / 运行一次 / 退出测试
```

连续运行停止后，左侧按钮切换为：

```text
连续运行 / 运行一次 / 退出测试
```

- 连续运行中，左侧按钮显示 `停止运行`，并使用高亮样式表示正在运行。
- 连续运行停止后，左侧按钮显示 `连续运行`。
- `运行一次` 始终表示从实时相机取最新一帧并执行一次测试。
- `退出测试` 停止测试态并恢复编辑态按钮。

### 基准图测试

- 只使用设置基准图页面保存的基准图。
- 图像来源必须是 `ReferenceImageProvider::referenceFrame()`。
- 不允许在基准图为空时回退到实时相机帧。
- 基准图为空时必须返回明确错误，例如 `no_reference_image`。
- 执行一次测试后，应在右侧视图显示基准图和测试结果。
- 基准图测试结果可生成 `referencePreviewSnapshot`，用于工具页显示基准图测试快照。

### 连续运行

- 点击编辑态 `测试运行` 后进入测试态，并默认启动连续运行。
- 连续运行使用实时视频流最新帧。
- 图像来源必须是 `CameraFrameProvider::currentFrame()`。
- 每次连续测试应显示当前帧、运行算法并更新 `ToolResult`、overlays、payload 和状态栏。
- 点击 `停止运行` 后应停止连续测试，按钮切换为 `连续运行`，视图保留最后一帧。
- 运行中应防止重复进入算法，至少使用 `m_running`、`m_xxxRunning` 或等价状态防重入。

### 运行一次

- 获取一张最新实时帧。
- 图像来源必须是 `CameraFrameProvider::currentFrame()`。
- 显示该帧并执行一次测试。
- 若当前正在连续运行，应先停止连续运行，再执行单次测试。
- `运行一次` 不使用基准图。

### 退出测试

- 停止连续运行。
- 退出测试态。
- 恢复编辑态按钮：

```text
基准图测试 / 测试运行 / 完成
```

- 右侧视图回到该功能的编辑预览状态，优先显示基准图或配置预览。

### 实现要求

- 各功能 Dialog 应使用清晰的 UI mode 管理状态，例如 `Edit / Continuous / TestPaused`。
- 连续运行可参考现有有无工具和字符识别工具实现方式。
- 测试结果应继续走统一 `ToolResult`，并通过 overlays、payload 和状态栏展示。
- 无图像、空基准图、算法异常等场景不得崩溃，必须返回明确状态和错误信息。

## 位置修正统一规范

位置修正是多个 FID 算子的统一能力入口。涉及位置修正 UI 时，应使用统一字段和占位语义：

### UI 字段

- 开关文案：`独立位置修正使能 ⓘ`
- 来源文案：`位置修正`
- 默认来源：`1 基准图.位置修正信息`
- 开关关闭时隐藏来源行，开启时显示来源行。
- 开关样式优先使用现有 `positionCorrectionSwitch` 样式。

### 配置字段

字段应保存到当前功能的 params 命名空间内，或按该功能已有扁平 params 结构保存：

```text
enablePositionCorrection: bool
positionCorrectionSource: string
```

旧配置缺少字段时：

- `enablePositionCorrection` 默认 `false`，除非已有功能历史默认值不同。
- `positionCorrectionSource` 默认 `1 基准图.位置修正信息` 或空字符串，按已有功能兼容策略确定。

### Adapter 和 Runner

Adapter 应解析并透传：

```text
enablePositionCorrection
positionCorrectionSource
```

Runner payload 至少输出：

```text
enablePositionCorrection
positionCorrectionSource
positionCorrectionApplied
positionCorrectionReason
```

若当前 runner 尚未实现实际位置补偿，必须明确：

```text
positionCorrectionApplied = false
positionCorrectionReason = "not implemented"
```

不允许在 UI 开关开启后静默忽略，也不允许假报已应用。

## 配置和 payload 约束

- 新字段必须有默认值和旧配置回退策略。
- 不删除已有 params / judgeRule 字段。
- `ToolConfig.roiNormalized` 继续保存通用检测 ROI 或圆形 ROI 外接矩形。
- payload 字段名称应稳定，方便 UI、日志和后续调试读取。
- 暂未实现但已经预留的能力必须输出明确状态或 reason，不允许静默成功。

## 文档维护要求

每次修改 FID 功能后，应更新对应功能实现记录文档，记录：

- 已实现功能。
- 本次更改。
- 出现的问题与处理。
- 验证结果。
- 剩余事项。

各功能提示词规范必须明确要求开发者先阅读：

- `AGENTS.md`
- `docs/FID/Function_Docs.md`
- 当前功能实现记录文档

## 验证要求

涉及 Qt 工程或 UI 接入时，至少执行：

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro
make -j8
git diff --check
```

涉及 runner 或 adapter 时，应补充或运行对应 smoke 测试。无法自动验证的图形交互项，应在功能文档中明确记录为手动验证项。
