# 项目通用约束

## 适用范围

- 本文件只记录项目级通用约束，用于指导所有功能算子和 UI 接入。
- 单个功能的实现计划、截图对照、字段清单、算法细节和专项测试项应放入对应功能文档（见「文档维护」），不写入根目录 `AGENTS.md`。
- 修改代码时保持范围收敛，只处理当前任务所需内容，不重构无关模块，不改变无关算子行为。

## 项目定位

- 本项目是基于 Qt 的工业视觉工具配置与运行工程。
- 核心链路是工具库入口、工具配置对话框、`ToolConfig` 配置保存、Adapter 参数解析、HALCON runner 算法执行、`ToolResult` 结果展示。
- 新增或完善功能时，应优先保持工具创建、保存、回显、测试运行和结果显示的完整闭环。

## 工程实现原则

- 优先沿用现有目录结构、命名方式、数据模型、对话框风格和 runner/adapter 链路。
- 新增功能应最小化接入现有工具库、配置保存、结果输出和 UI 展示流程。
- 变更公共结构、配置语义或跨模块接口前，先检查已有算子的兼容模式，并尽量保持向后兼容。
- 禁止为单个功能在项目级规则中沉淀过细实现提示；需要长期保留的功能细节应进入 `docs/` 下的功能文档。

## 目录导航

- `src/`：Qt 业务代码、工具对话框、工具配置、adapter 和算法 runner。
  - `src/algorithms/`：算法实现目录，runner 按域分子目录落子：
    - `presence/`：有无类算子（Pattern / Blob / Circle / Edge / Line / Contour Presence）。
    - `recognition/`：识别类算子（ColorRecognition / ColorComparison 等颜色类）。
    - `ocr/`：字符识别（OcrHalconRunner）。
    - `ai/`：AI 检测桥接算子（AiDetectionRunner，非 HALCON，属约定例外）。
    - `halcon/`：仅放 `HalconRuntimePaths` 运行时路径解析，不放 runner。
  - `src/tooladapters/`：一工具一 adapter，负责配置解析、调用 runner、输出统一 `ToolResult`。
  - `src/toolcore/`：跨工具公共结构，包括 `ToolConfig`、`ToolRequest`、`ToolResult`、`ToolTypes`、`ToolAdapter`、`ToolEngine`、`ToolOverlay`、`ToolPreviewSnapshot`。
  - `src/frame/`：相机帧与图像输入相关代码。
- `ui/`：Qt Designer `.ui` 文件。
- `styles/`：全局或共用 QSS 样式。
- `resources/`：图标、图片、字体等资源。
- `docs/`：分析记录、功能设计、实施计划和验证记录（结构见「文档维护」）。
- `smoke/`、`tests/`：算法/runner 冒烟与回归用例，每个用例配独立 `.pro`。
- `qt_ui_test.pro`：当前主要 Qt 构建入口。

## 新增工具最小骨架

新增一个视觉工具至少包含以下部分，缺项应在功能文档中标注为阻塞：

- 对话框：`src/<Tool>Dialog.{h,cpp}` + `ui/<Tool>Dialog.ui`，风格参考现有同类工具 dlg。
- Adapter：`src/tooladapters/<Tool>Adapter.{h,cpp}`。
- Runner：`src/algorithms/<域>/<Tool>HalconRunner.{h,cpp}`，命名与链路参考现有 HALCON runner。
- 配置字段：通过 `ToolConfig.params` / `judgeRule` 保存，字段命名清晰、可回显。
- 注册接入：在 `ToolLibraryDialog` / `ToolEngine` 中按既有方式注册，保证新建、保存、回显、运行闭环。
- 功能文档：`docs/FID/<Tool>/`（结构见「文档维护」）。

## HALCON 红线（核心约束）

以下各条贯穿全工程，任何与此冲突的实现都需先停下确认：

- 所有视觉算子的核心算法必须基于 HALCON 已有算子、类或过程实现；不得用 OpenCV、自写算法或第三方库替代 HALCON 核心算法能力。
- 任意视觉算子实现前，必须先确认本机 HALCON 环境具备所需能力，并在对应功能计划中列出 HALCON 接口。若 HALCON 没有对应能力或本机缺符号，必须停止实现并向用户确认。
- OpenCV 只允许作为图像容器、采集、显示或必要格式桥接使用；`cv::Mat` 可作为 `ToolRequest` 图像输入载体，但进入算子核心前必须转换为 HALCON 图像并由 HALCON 算子处理。
- 新增或重做算法 runner 必须采用 HALCON runner 形态，命名和链路参考现有 `OcrHalconRunner`、`PatternPresenceHalconRunner`、`BlobPresenceHalconRunner` 等。
- `qmake`、运行环境、license 和 runtime 路径解析必须走现有 `HalconRuntimePaths` 逻辑。
- 约定例外：`AiDetectionRunner` 等 AI 检测桥接算子走 AI 推理链路，不属 HALCON 约束范围，但不得借此类例外为其他算子引入非 HALCON 核心算法。

## UI 与交互约束

- 新增工具对话框应参考现有工具 dlg 的整体结构：顶部标题栏、左侧参数区、右侧图像预览区、底部动作按钮。
- 参数区优先沿用 `基础 / 全部` 分段按钮、`panelRole=configCard` 配置卡片、`role=cardTitle` 标题、`role=rowField` 行标题、`role=collapseCard` 折叠按钮等既有样式属性。
- ROI 工具、模式选择、结果判断等互斥控件应使用 `QButtonGroup` 管理 checked 状态。
- 图像预览区应保持现有工具栏、画布、ROI overlay、状态栏和 OK/NG 反馈节奏。
- 控件文案、颜色、间距和显隐关系优先与已有同类对话框一致；若用户提供专项截图，以对应功能文档中的截图要求为准。
- 按钮连接必须稳定，普通操作不得误触发 `accept`、`reject` 或程序退出。

## 配置与结果约束

- 工具配置应通过现有 `ToolConfig.params`、`judgeRule` 等结构保存，字段命名保持清晰、稳定、可回显。
- Adapter 负责解析配置、调用 runner、转换结果，并输出统一的 `ToolResult`。
- Runner 应返回明确的成功、失败和不支持状态；空图像、无效 ROI、无模型或参数缺失等场景不得崩溃。
- 对暂未实现但已预留的能力，应返回明确的 unsupported 信息，不允许静默降级到其他算法。
- 结果 payload 应包含便于 UI 展示和问题定位的关键字段，例如类别、得分、ROI、耗时、错误码或错误信息。

## 工程卫生

### 构建产物不入库

- Qt/qmake 构建产物（`*.o`、`moc_*.cpp`、`ui_*.h`、`qrc_*.cpp`、`Makefile`、`*.Makefile`、`.qmake.stash`、`moc_predefs.h`、可执行文件 `qt_ui_test`、`*_smoke` 等）已在 `.gitignore` 中忽略，**不得提交入库**。
- `smoke/`、`tests/` 子工程的产物同样不入库：源码 `.cpp`/`.pro` 入库，编译产生的可执行文件、`*.o`、`Makefile`/`*.Makefile`、`*.log` 一律不入库。
- 运行日志（`*.log`，如 `qt_ui_test.log`）不入库。
- `.bak`、`*.bak_*` 等手工备份文件不得入库；如需保留历史，依赖 git 而非本地备份后缀。

### 影子构建

- **优先在 `build/` 或影子目录构建**，避免 in-source 构建把产物散落到源码根目录与 `src/`、`smoke/`：
  ```bash
  mkdir build && cd build
  /home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
  make -j$(nproc)
  ```
- 若已在源码根目录执行过 in-source 构建，产物可安全清理（全部被 `.gitignore` 忽略，删除不影响源码，`make` 会重建）：
  ```bash
  rm -f *.o moc_*.cpp ui_*.h qrc_*.cpp Makefile .qmake.stash moc_predefs.h qt_ui_test *.log
  ```

### 误入库产物的处理

- 发现构建产物被误 `git add` 入库时，用 `git rm --cached <文件>` 移出索引（保留或删除本地文件视情况而定），并确认 `.gitignore` 规则已覆盖，避免再次入库。

### 依赖与链路

- 不新增非 HALCON 算法依赖；不以 OpenCV、自写分类器或第三方库替代 HALCON 核心算法能力。
- 保持已有 OCR、图案有无、Blob、圆、边、线、轮廓等 HALCON runner 链路不受无关影响。
- 若必须调整工程文件、链接方式或运行环境变量，应说明原因，并将改动限制在必要范围内。

## 文档维护

- 功能文档统一落在 `docs/FID/<工具名>/`，按工具分子目录（如 `ColorRecognition/`、`ColorComparison/`），含实现文档、流程、提示词规范等。
- 分析/修复/回归报告落在 `docs/analysis/`，文件名应能体现主题与阶段，避免一次性临时名。
- `docs/FID/Function_Docs.md` 作为功能文档总入口，新增工具时应登记。
- 根目录 `AGENTS.md` 不记录具体功能的完成状态，只保留项目级通用约束和文档维护规则。

## 通用完成标准

- 功能入口可见，能新建、保存、重新打开并回显配置。
- 测试运行能返回明确 `ToolResult`，UI 能展示 OK/NG、关键结果、ROI 和耗时。
- 空图像、无效 ROI、缺少模型、缺少参数、HALCON 运行环境异常等场景不崩溃，并返回可定位的错误信息。
- 新增或修改功能不影响已有工具的创建、编辑和运行链路。
- 文档中记录实现状态、剩余事项、阻塞项和验证结果，便于后续继续接手。

## 常见风险

- 不要把单功能实现计划、临时 TODO 或调试过程写入根目录 `AGENTS.md`。
- 不要为了新增功能重写公共 UI、公共配置结构或无关算法链路。
- 不要删除用户提供的截图、样例图片、分析文档或功能文档，除非用户明确要求。
- 不要在 HALCON 能力未确认时提前落地替代算法。
- 不要让按钮点击误触发关闭窗口、退出程序或保存无效配置。
- 不要把构建产物或 `.bak` 备份文件提交入库。

## 验证要求

- 涉及 Qt 工程或 UI 接入时，至少运行 `qmake qt_ui_test.pro` 和 `make` 验证构建。
- 涉及算法 runner 时，应补/跑对应 `smoke/` 用例，并验证 HALCON runtime、license、必要符号缺失时能返回明确错误。
- 涉及工具配置时，应验证新建、保存、重新打开、编辑、运行和异常输入场景。
- 涉及 UI 交互时，应手动检查关键按钮、下拉框、ROI 操作、取消/完成、无图像/无数据状态不崩溃。
- 颜色类（识别/比较）等带样本与 ROI 的工具，验证需额外覆盖空图、无效 ROI、缺样本/缺模板场景。
