# FID 功能公有约束和规范

本文档记录 FID 目录下各功能算子开发的公有约束。后续所有 FID 子功能提示词规范、实现计划、实现记录和代码开发，都必须同时遵守根目录 `AGENTS.md` 和本文档。

## 功能文档索引

- 颜色识别：`docs/FID/ColorRecognition/颜色识别提示词规范.md`、`docs/FID/ColorRecognition/color_recognition_function_implementation.md`
- 颜色比较：`docs/FID/ColorComparison/颜色比较V2设计说明.md`、`docs/FID/ColorComparison/颜色比较V2提示词规范.md`、`docs/FID/ColorComparison/color_comparison_function_implementation.md`（V1 历史提示词仅供追溯）
- 注册分类：`docs/FID/RegisteredClassification/注册分类提示词规范.md`、`docs/FID/RegisteredClassification/registered_classification_function_implementation.md`、`docs/FID/RegisteredClassification/注册分类算法当前实现说明.md`
- 注册目标检测：`docs/FID/RegisteredClassificationDetection/注册分类检测提示词规范.md`、`docs/FID/RegisteredClassificationDetection/registered_classification_detection_function_implementation.md`
- 位置修正：`docs/FID/PositionCorrection/位置修正UI设计规范.md`、`docs/FID/PositionCorrection/位置修正提示词规范.md`、`docs/FID/PositionCorrection/position_correction_function_implementation.md`

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
- 工具页/主览窗口显示测试结果时，必须显示当前检测 ROI 框；不得因为清理配置阶段交互 ROI 而过滤 runner 输出的 `ROI` 结果 overlay。
- 只允许清理配置 Dialog 关闭后残留的交互式编辑 ROI、样本 ROI 或屏蔽区编辑态图形；检测运行产生的 ROI overlay 属于结果展示内容，应与 OK/NG、类别、得分和耗时一起保留。

### 公共图像视图缩放、平移与 ROI 坐标规范

工具配置页的图片浏览、ROI 绘制和结果 overlay 应优先复用 `src/frame/FrameViewHelper`，不得在单个 Dialog 中重复实现一套视图坐标转换。公共缩放和平移能力采用显式启用方式：新增或改造功能按需开启；未开启的旧功能保持现有交互，避免公共类扩展造成行为回归。

启用公共浏览能力后，交互统一为：

| 当前状态 | 图片缩放 | 图片平移 | 普通左键 |
| --- | --- | --- | --- |
| 未启用 ROI/屏蔽区绘制 | 鼠标滚轮 | 左键按压拖动 | 平移图片 |
| ROI/屏蔽区绘制按钮高亮 | `Ctrl + 鼠标滚轮` | `Ctrl + 左键按压拖动` | 绘制当前 ROI/屏蔽区 |

实现必须满足以下约束：

- 鼠标按下时锁定本次拖动的操作模式；拖动过程中按下或松开 `Ctrl` 不得在平移与 ROI 绘制之间切换。
- 缩放以鼠标指针下的图像位置为锚点，默认范围为“适应窗口”到 `8×`；达到上下限后继续滚轮不得产生额外变换。
- 双击非 ROI 操作区域或调用公共“适应窗口”动作时恢复初始比例。
- 图片平移和缩放只修改 `QGraphicsView` 的 transform/视图中心，不修改图片、ROI 或结果 overlay 的 scene 坐标。
- 鼠标坐标必须通过 `view -> scene -> 原图像素 -> 0..1 归一化坐标` 转换；不得用视口像素直接计算 ROI。
- 矩形、圆形、多边形、线带、模板区和屏蔽区必须使用同一坐标转换合同。缩放或平移前后，同一个 ROI 的归一化坐标和对应原图像素尺寸必须保持不变。
- ROI 和结果轮廓边框优先使用 cosmetic pen，使线宽不随视图倍率变粗；顶点命中阈值等交互尺寸应以视口像素为语义，不应因放大倍率而失真。
- 图片为空时忽略缩放和平移。鼠标位于图像外时不得开始 ROI，但已放大的图片允许继续平移。
- 图片内容或分辨率改变时恢复适应窗口；同尺寸连续帧刷新时保持用户当前倍率和观察中心。
- 视口尺寸变化时，处于适应窗口状态则重新适应；用户已手动放大时保持倍率和观察中心，不得因普通 resize 强制复位。
- 临时平移不得发出 `roiChanged`、`polygonChanged`、`circleChanged`、`lineBandChanged` 等 ROI 变更信号。

### 原始图像、ROI 裁剪与显示层分离规范

- 算法输入必须来自相机帧、参考帧或其未经标注的原始图像副本；禁止把 QWidget 截图、`QGraphicsScene::render` 结果、带 overlay 的 `QImage/QPixmap` 或预览窗口缓存传入 Runner。
- ROI 缩略图必须先从原始图像按原图像素或 `0..1` 归一化坐标完成裁剪，再进行仅影响显示尺寸的缩放；橙色 ROI 框、Mask 填充、顶点、文字、OK/NG 和得分等显示层不得写入裁剪图像像素。
- ROI、Mask 和结果标注应使用独立的 scene item、overlay widget 或控件外框绘制。需要在缩略图中表达排除区域时，也必须作为可移除的独立显示层，并明确其不属于算法像素。
- 模板建模、检测运行和缩略图展示必须共用同一套 ROI 坐标合同，但不得共用已经合成显示标注的位图。缩放、平移、抗锯齿和高 DPI 适配只能改变显示，不得改变 Runner 使用的裁剪范围。
- 公共辅助函数应在命名上区分 `raw/source image` 与 `annotated/preview image`，禁止用语义含糊的 preview 图像作为算法输入。
- 自动化验证至少覆盖：原始裁剪像素与源图对应区域一致；开启 ROI/Mask/结果 overlay 前后 Runner 输入哈希或特征不变；缩略图像素中不存在框线、填充或文字的显示颜色。

自动化验证至少覆盖：缩放上下限、光标锚点、平移模式锁定、绘制态 `Ctrl` 切换、适应窗口恢复，以及缩放/平移前后 ROI 归一化坐标不变。单个功能启用该能力后，还应手工检查该功能实际提供的所有 ROI 和屏蔽区形状。

## FID UI 可读性和占位窗口样式规范

当前 FID 二级窗口和占位流程以注册分类训练窗口、模型管理窗口为基准。后续新增或调整 FID 工具时，若没有专项设计稿，应优先沿用本节规范；若子功能旧文档与本节冲突，以本节为准。

### 颜色和底色

- 常规参数区、卡片区、表格区和弹窗内容区优先使用白底或近白底，文字使用深色，不允许出现深底深字、蓝底深字、灰褐底深字等低对比组合。
- 深色背景只允许用于图像画布、相机预览或明确需要突出图像内容的区域；该区域上的状态文字必须使用高对比浅色或放入独立浅色状态条。
- 主操作使用橙色强调，推荐 `#ff7a00`；二级操作可使用蓝色，推荐 `#0ea5e9` 或 `#4094ff`。
- 普通边框使用高可见蓝色系，推荐 `#60a5fa`、`#93c5fd`、`#4094ff`；禁用态使用浅底深灰字，不得使用过淡文字。
- 删除、危险或不可恢复操作使用红色语义，推荐 `#ef4444`；不要大面积使用红色底色，避免干扰正常操作。
- 提示条优先使用浅蓝或浅橙底配深色文字，例如浅蓝提示条配深蓝文字、浅橙状态条配深棕文字。

### 字体和尺寸

- 一级窗口标题不小于 `26px`，使用加粗字重。
- 卡片标题不小于 `22px`，使用加粗字重。
- 普通标签、输入框、下拉框、按钮、表格内容不小于 `18px`。
- 弹窗字段标签不小于 `20px`，确认/取消按钮不小于 `20px`。
- 状态文字不小于 `18px`；关键状态如 `未注册` 不小于 `22px`，使用高对比颜色和加粗字重。
- 图标按钮建议最小尺寸 `38x38`；模型列表等关键操作按钮建议不小于 `44x44`。
- 输入框、下拉框、按钮应提供足够 padding，避免文字贴边或因字号增大被裁切。

### 控件样式

- 参数卡片和二级窗口卡片使用明确边框，不使用难以分辨的浅灰边框；推荐白底、蓝色边框、6-8px 圆角。
- 输入框、下拉框和普通按钮使用白底、深色文字、蓝色边框；hover 可使用浅蓝底。
- 位于图像预览、缩略图状态栏等深色工作区内的筛选下拉可以使用深底浅字，但必须保证高对比、右侧有明确下拉提示区，展开列表也必须显式设置高对比底色和文字色；不得依赖系统 palette。
- 主按钮使用橙底白字；二级按钮可使用蓝底白字；取消按钮使用白底深字。
- 表格表头使用浅蓝底、深蓝字、加粗；表格内容使用白底深字。
- 禁用控件必须仍可读，推荐浅灰底、深灰字和可见边框。
- 所有没有可见文字说明的控件必须设置鼠标悬浮提示 `tooltip`；纯图标按钮、缩略图角标、ROI 删除按钮、预览/查看按钮、导入/导出按钮和开关类控件都必须说明动作对象和结果，例如 `删除当前 ROI`、`删除全部注册图`、`预览目标 ROI`、`导出模型`。
- 占位窗口中，所有未实现能力必须用状态文字或提示条说明，例如 `占位界面，训练算法未接入`、`未注册`。
- 弹窗或二级窗口的内容区不得使用未命名、未设样式的普通 `QWidget` 承接布局；必须使用设置了 `objectName` 或 `panelRole` 的 `QFrame` / `QWidget`，并在 QSS 中显式声明背景色和文字色。
- 严禁依赖系统 palette 或父窗口默认背景来决定内容区底色；深色系统主题下也必须保持白底深字或等价高对比组合。
- 对关键弹窗应增加自动化检查，至少断言内容容器存在明确样式属性；能渲染采样时，应验证内容区实际像素不是深色背景。

### 注册训练和检测控件样式

注册训练、注册目标检测训练等带图像标注和模型训练入口的二级窗口，应以 `RegisteredClassificationTrainingDialog` 的控件风格为基准。后续新增检测类训练窗口若没有单独设计稿，应直接复用以下视觉规范，避免同一类窗口出现字号、边框和按钮状态不一致。

- 窗口整体使用白底深字，推荐根样式为 `QDialog{background:#ffffff;color:#0f172a;font-size:18px;}`。
- 左侧图像预览面板和右侧训练卡片统一使用白底、蓝色高可见边框，推荐 `border:3px solid #60a5fa;border-radius:8px;`。
- 右侧内容区使用白底，不使用灰底或深底；推荐 `QFrame[panelRole="rightPanel"]{background:#ffffff;border:0;}`。
- 图像画布允许使用近黑底，推荐 `QGraphicsView[panelRole="trainingCanvas"]{background:#05070a;border:0;}`，但画布外状态栏必须使用浅色底和深色文字。
- 预览工具栏保持白底，并使用橙色上/下分隔强调，推荐 `border-bottom:4px solid #ff7a00;`。
- 底部图像状态栏使用浅橙底、橙色边线和深棕文字，推荐 `background:#fff7ed;border-top:3px solid #ffb366;color:#7c2d12;`。
- 普通标签默认不小于 `18px`，颜色 `#0f172a`，字重建议 `600`；不得在训练/检测窗口中使用小于 `16px` 的主要操作文字。
- 窗口标题使用 `role="windowTitle"`，推荐 `font-size:28px;font-weight:800;color:#08386f;`。
- 步骤卡片标题使用 `role="cardTitle"`，推荐 `font-size:23px;font-weight:800;color:#08386f;`。
- ROI 预览页标题使用 `role="previewPageTitle"`，推荐 `font-size:26px;font-weight:800;color:#1f2937;`。
- 空状态提示使用 `role="previewEmpty"`，推荐 `font-size:22px;font-weight:700;color:#94a3b8;`。
- 关键状态文字使用 `role="stateLabel"`，推荐 `font-size:24px;font-weight:800;color:#c2410c;`；例如 `未注册`、训练未接入、无图或无标注提示。
- 普通按钮、ROI 工具按钮、图标按钮和下拉框统一使用白底深字、蓝色边框，推荐 `border:2px solid #4094ff;border-radius:6px;padding:10px;font-size:18px;font-weight:700;`。
- 缩略图状态栏中的过滤下拉属于深色工作区控件，可使用深灰底白字，并需要独立右侧下拉提示区；下拉展开列表必须同步设置深底白字或白底深字的高对比样式。
- 普通按钮 hover 使用浅蓝底，推荐 `background:#e0f2fe;border-color:#0284c7;`。
- 主按钮必须使用橙底白字，推荐 `QPushButton[actionRole="primary"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;font-size:20px;font-weight:800;}`。
- 禁用按钮必须保持可读，推荐 `background:#f8fafc;color:#64748b;border-color:#94a3b8;`。
- ROI 工具按钮应具有明确选中态；选中态可使用橙色边框或浅橙底，且必须与 hover 态可区分。
- 缩略图列表使用深色背景时，文字必须为白色，推荐 `QListWidget{background:#27303c;color:#ffffff;border-top:3px solid #4094ff;font-size:15px;font-weight:700;}`。
- 缩略图列表项推荐 `background:#344155;color:#ffffff;border:2px solid transparent;padding:6px;margin:5px;`，选中态推荐 `background:#0ea5e9;color:#ffffff;border-color:#ff7a00;`。
- 分类列表、目标列表的表头使用浅蓝或浅灰蓝底；列表行默认白/浅灰底，选中行使用浅绿底和绿色边框，推荐 `background:#dcfce7;border-color:#22c55e;`。
- 分类行、目标行内操作按钮使用纯图标或图标+tooltip；图标按钮最小尺寸不小于 `38x38`，目标/类别行内按钮建议 `40x40` 到 `44x44`。
- 训练/检测窗口中的无文字控件不得只依赖图标表达语义；必须设置稳定、可读的 tooltip，且文案使用具体动作名，不使用 `按钮`、`操作` 这类泛称。
- 训练/检测窗口中所有卡片、列表、预览页、状态栏、目标行和分类行必须设置稳定 `panelRole` 或 `objectName`，并在 QSS 中显式声明背景、文字和边框。
- 注册目标检测训练窗口若参考注册分类训练窗口，只能按功能差异删减控件，不得降低字体、边框、对比度或主按钮语义。检测窗口的 `矩形框选`、`多边形框选`、`目标列表`、`角度使能`、`最优模型分辨率设置使能` 等控件应与注册分类训练窗口的按钮和标签字号保持一致。

### 图标和提示

- 纯图标按钮必须设置 `toolTip`，鼠标悬停时显示动作名称。
- 图标必须表达真实语义，不允许用 `...`、`⇩`、`×` 等临时文本符号代替正式动作图标。
- 常见动作建议：
  - 重命名：文档 + 笔、编辑笔或等价图标，tooltip 为 `重命名`。
  - 导出：向下箭头、保存/导出托盘或等价图标，tooltip 为 `导出`。
  - 删除：红色叉号、删除桶或等价图标，tooltip 为 `删除`。
  - 新增/创建：加号、文件夹加号或等价图标，tooltip 或按钮文案包含 `创建` / `新建`。
  - 导入：打开文件夹、上传/导入箭头或等价图标，tooltip 或按钮文案包含 `导入`。
- 需要自动化测试或后续接真实动作的按钮，应设置稳定 `objectName`。

### 二级窗口和占位流程

- 训练、模型管理、数据集管理等二级窗口初始尺寸应为父窗口的 70%-80%，当前推荐约 `76%`；无父窗口时可使用合理兜底尺寸。
- 二级窗口可以是占位闭环，但不得伪造真实训练、真实标注、真实模型仓库或真实导出成功。
- 占位闭环应让用户看到下一步界面，同时明确当前能力未接入。例如创建数据集确认后可进入注册训练窗口，但训练按钮应禁用或提示训练算法未接入。
- 创建类弹窗应包含明确标题、必填字段标记、训练/工具类型选择、确认和取消按钮；确认按钮为橙色主按钮。
- 底部提示条必须可读，提示文字与背景保持高对比，不得使用深底深字。

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

位置修正是多个 FID 算子的统一能力入口。正式名称统一为“位置修正”，代码和配置英文名使用 `PositionCorrection`。涉及位置修正 UI 时，应使用统一字段、稳定引用和占位语义。代码侧公共入口为 `src/toolcore/PositionCorrection.{h,cpp}`，当前只提供字段解析、写回和未应用 payload 占位，不代表真实位置补偿已经实现。

专项 UI、配置和截图规范见 `docs/FID/PositionCorrection/位置修正UI设计规范.md`。

### 节点模型

位置修正包含两类互相独立的节点：

- 基准图位置修正：方案级固定节点，只允许一份，固定来源 ID 为 `reference.positionCorrection`，不占用工具序号。
- 工具位置修正：属于 `Location` 分类，允许创建多个实例，每个实例使用独立且稳定的 `toolId`。

基准图配置和工具实例配置不得相互覆盖。界面中的 `1 基准图`、`12 位置修正` 等序号是动态显示文本，不得作为唯一引用键。

### UI 字段

- 开关文案：`独立位置修正使能 ⓘ`
- 来源文案：`位置修正`
- 默认来源：`1 基准图.位置修正信息`
- 开关关闭时隐藏来源行，开启时显示来源行。
- 开关样式优先使用现有 `positionCorrectionSwitch` 样式。
- 开启时，来源菜单只显示固定基准图节点以及当前消费工具之前、已启用且有效的位置修正工具实例。
- 不得显示当前工具自身、后置工具、已删除工具或会形成循环依赖的节点。

### 配置字段

字段应保存到当前功能的 params 命名空间内，或按该功能已有扁平 params 结构保存：

```text
enablePositionCorrection: bool
positionCorrectionSourceId: string
positionCorrectionSource: string
```

目标合同中：

- `positionCorrectionSourceId` 是运行和引用校验使用的稳定键。
- `positionCorrectionSource` 是显示文本和旧配置兼容字段。
- 当前公共 helper 尚未读写 `positionCorrectionSourceId`；新增稳定 ID 必须在位置修正 UI/config 专项实施中统一补齐，不得由单个消费工具私自定义不同字段。

旧配置缺少字段时：

- `enablePositionCorrection` 默认 `false`，除非已有功能历史默认值不同。
- `positionCorrectionSource` 默认 `1 基准图.位置修正信息` 或空字符串，按已有功能兼容策略确定。
- 只有旧文本能唯一映射到合法节点时才补齐 `positionCorrectionSourceId`；无法匹配或匹配不唯一时必须显示来源失效，不得自动回退到基准图。

### 来源生命周期

- 创建位置修正工具时生成新的 `toolId`；不得因为已有同类工具而复用实例。
- 复制位置修正工具时生成新的 `toolId`，已有消费关系继续指向原实例。
- 工具插入或重排后动态更新显示序号，但稳定 ID 不变。
- 来源被删除、禁用或移动到消费工具之后时，消费配置保留原来源 ID，并显示 `来源不可用` 或等价明确错误。
- 当 `enablePositionCorrection=true` 且来源无效时，禁止完成保存和测试运行。
- 当 `enablePositionCorrection=false` 时可以保存失效 ID，但运行时不得应用位置修正。
- 任何来源失效场景都不允许静默使用默认来源、第一项来源或上一次运行结果。

### Adapter 和 Runner

Adapter 应解析并透传：

```text
enablePositionCorrection
positionCorrectionSourceId
positionCorrectionSource
```

在稳定 ID 尚未完成公共 helper 接入前，已有 Adapter/Runner 可以继续只透传旧字段，但必须保持 `positionCorrectionApplied=false` 和明确的未实现原因；不得假报稳定引用已经生效。

新增或改造工具时优先使用公共结构和方法：

```text
PositionCorrectionConfig
PositionCorrection::fromParams(...)
PositionCorrection::writeParams(...)
PositionCorrection::writeNotAppliedPayload(...)
```

Runner payload 至少输出：

```text
enablePositionCorrection
positionCorrectionSourceId
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
