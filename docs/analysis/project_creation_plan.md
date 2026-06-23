# 智能相机视觉检测项目建设规划

> 日期：2026-06-23  
> 规划对象：`qt_ui_test.pro` 主工程  
> 项目定位：基于 Qt Widgets 的工业视觉检测桌面应用，围绕“相机取图、方案管理、基准图、工具链配置、传统视觉/OCR/AI 检测、结果输出”形成可部署、可维护、可扩展的软件系统。

## 1. 建设目标

本项目目标是建设一套面向现场检测设备的智能相机视觉软件，支持操作人员选择设备和检测方案，采集实时图像，配置多种检测工具，并在运行态输出 OK/NG、检测耗时、统计数据和图像叠加结果。

核心目标：

- 建立稳定的 Qt 桌面端操作界面。
- 打通相机实时采集、基准图保存、方案持久化和工具链运行流程。
- 支持 OCR、图案有无、Blob 有无、圆有无、边有无、线有无、轮廓有无、AI 目标检测等工具。
- 支持 HALCON 传统视觉算法和远端 RKNN/YOLO AI 推理桥接。
- 逐步补齐真实工业相机接入、配置外置化、测试回归和部署交付能力。

## 2. 当前基础

### 2.1 已有工程能力

- 主工程为 Qt Widgets + qmake 工程，目标名为 `qt_ui_test`。
- 工程启用 C++17，使用 Qt `widgets`、`concurrent`、`multimedia`、`multimediawidgets` 模块。
- 图像承接以 OpenCV `cv::Mat` 为核心，显示层通过 `QImage` 和 `QGraphicsView` 完成。
- 传统视觉/OCR 算法通过 HALCON runner 执行。
- AI 检测通过远端 RKNN/YOLO 桥接执行。
- 当前已有方案目录模型：`projects/scheme_xxx/scheme.json + reference.png`。
- 当前已有 smoke 工程用于 HALCON runtime 和 AI bridge 冒烟验证。

### 2.2 已有主要模块

```text
src/main.cpp                  应用启动、日志、HALCON 环境初始化
src/LoginWindow.*             登录和会话入口
src/MainWindow.*              主监控、方案、相机预览、工具链运行
src/SchemeStore.*             方案读取、保存、列表管理
src/frame/                    相机帧、基准图、图像转换、图像显示辅助
src/toolcore/                 工具抽象、配置、请求、结果、调度引擎
src/tooladapters/             ToolConfig 到算法 runner 的适配层
src/algorithms/halcon/        HALCON 运行时路径和环境处理
src/algorithms/presence/      有无检测类 HALCON runner
src/algorithms/ocr/           OCR runner
src/algorithms/ai/            AI 检测 runner
ui/                           Qt Designer 界面文件
resources/                    qrc、图标、图片、字体、QSS
docs/analysis/                架构分析、问题记录、阶段报告
rk_ai_materials/              RKNN/YOLO 模型、样例图和联调材料
```

### 2.3 当前已接通的运行链路

```text
登录
  -> 主界面
  -> 加载当前方案
  -> 打开 CameraFrameProvider
  -> 获取实时图像
  -> 选择或配置工具链
  -> ToolEngine 顺序执行已启用工具
  -> Adapter 调用 HALCON/RKNN runner
  -> 汇总 ToolResult
  -> 主界面显示 OK/NG、耗时、统计和 overlay
```

## 3. 建设范围

### 3.1 本期范围

本期重点不是继续堆 UI，而是把已有原型整理成可现场联调的工程版本。

本期应覆盖：

- 工程结构固化和构建流程稳定。
- 相机接入方式确认和真实设备取流。
- 方案管理、基准图和工具链配置稳定化。
- 已接入工具能力与 UI 展示能力对齐。
- HALCON、OpenCV、AI 桥接等运行配置外置化。
- 最小自动化/半自动化回归样例建立。
- 部署和现场排障文档整理。

### 3.2 暂不纳入本期的内容

- 完整权限系统和后台账户管理。
- 多设备集中管理平台。
- Web 管理端。
- 大规模数据库存储。
- 所有海康式工具类型一次性全部实现。
- 深度重构为 CMake 或插件化架构，除非后续交付环境明确要求。

## 4. 总体架构规划

建议保留当前核心抽象，并在相机、运行配置和测试层补齐缺口。

```text
UI 层
  LoginWindow / MainWindow / 各工具配置 Dialog

应用服务层
  SchemeStore
  RuntimeConfig
  CameraManager
  ToolChainRunner

图像输入层
  ICameraSource
    OpenCvV4L2CameraSource
    HikMvsCameraSource
    ImageFileCameraSource

工具调度层
  ToolConfig
  ToolRequest
  ToolResult
  ToolAdapter
  ToolEngine

算法执行层
  HALCON OCR
  HALCON presence tools
  RKNN/YOLO AI bridge

资源与部署层
  resources.qrc
  runtime.json
  projects/
  smoke/
  docs/
```

当前 `ToolEngine` 和 adapter 边界已经比较清楚，应继续沿用。后续重点是从 `MainWindow` 中逐步拆出运行控制、相机源和外部配置，降低主窗口复杂度。

## 5. 模块建设计划

### 5.1 工程和构建模块

目标：让项目在目标 Linux/Qt 环境中可以稳定 clean build，不依赖根目录历史构建产物。

建设任务：

- 固定目标 Qt、OpenCV、HALCON、编译器版本。
- 清理根目录 `.o`、`moc_*.cpp`、`ui_*.h`、`Makefile`、旧可执行文件等构建产物。
- 保留独立 `build/` 或 Qt Creator shadow build 作为唯一构建输出。
- 将平台依赖拆入 `.pri` 文件，例如 `qt_ui_test_linux.pri`。
- 将 HALCON/OpenCV/MVS SDK 路径从主 `.pro` 中逐步收敛为环境变量或配置入口。

验收标准：

```text
新拉取工程后，按文档执行 qmake + make 能完成构建
构建产物不回写源码根目录
启动日志中能明确打印 Qt/OpenCV/HALCON 关键环境
缺失依赖时错误信息可定位
```

### 5.2 相机输入模块

目标：建立统一相机源接口，支持当前 OpenCV/V4L2 路径，并预留海康 MVS SDK 工业相机接入。

建议接口：

```text
ICameraSource
  enumerateDevices()
  open(DeviceInfo)
  close()
  start()
  stop()
  currentFrame()
  frameReady()
  error()
```

设备信息建议结构：

```text
DeviceInfo
  id
  displayName
  vendor
  model
  serialNumber
  transport
  backend
```

建设任务：

- 保留当前 `CameraFrameProvider` 对外信号和取帧能力。
- 将 OpenCV/V4L2/GStreamer 打开逻辑拆成 `OpenCvV4L2CameraSource`。
- 新增 `ImageFileCameraSource`，用于无相机开发和回归测试。
- 确认现场相机是否为 UVC、USB3 Vision 或 GigE Vision。
- 如果是海康工业相机，新增 `HikMvsCameraSource`，接入 `MvCameraControl.h` 和 `libMvCameraControl.so`。
- 登录页和主界面设备列表改为真实枚举结果，不再硬编码三台假设备。

验收标准：

```text
无相机时 UI 不崩溃，错误提示明确
UVC/V4L2 相机能枚举、打开、取流、关闭
海康 MVS 相机能枚举、打开、连续取流
断线、占用、无权限时不会卡死主线程
相机输出统一为 BGR cv::Mat
```

### 5.3 方案和基准图模块

目标：使方案成为现场可维护资产，支持稳定保存、迁移和恢复。

建设任务：

- 保持 `projects/scheme_xxx/scheme.json + reference.png` 目录模型。
- 明确 `schemaVersion` 升级策略。
- 对 `ToolConfig`、`ToolPreviewSnapshot`、`outputConfig` 增加兼容性校验。
- 在加载损坏方案时给出错误提示，并保留备份。
- 支持方案导入/导出压缩包，便于现场备份。

验收标准：

```text
新建方案、切换方案、保存方案、重启后恢复均正常
基准图不存在或损坏时提示清楚
工具配置保存后重新打开不丢参数
旧 schemaVersion 方案可识别并尽量兼容
```

### 5.4 工具链运行模块

目标：让工具运行链路稳定、可观测、可回归。

建设任务：

- 保留 `ToolConfig -> ToolRequest -> ToolAdapter -> ToolResult` 抽象。
- 从 `MainWindow` 拆出 `ToolChainRunner`，专门处理单次运行、连续运行、帧去重、性能统计和结果汇总。
- 建立 `ToolType -> Dialog -> Adapter -> Runner -> 支持状态` 矩阵。
- UI 只展示已实现、可运行或明确标记“未实现”的工具。
- 每个 runner 输出统一的 `success`、`ok`、`status`、`message`、`score`、`count`、`elapsedMs` 和 overlay。

支持矩阵初稿：

| 工具 | Dialog | Adapter | Runner | 当前状态 |
| --- | --- | --- | --- | --- |
| OCR | `CharacterRecognitionDialog` | `OcrAdapter` | `OcrHalconRunner` | 已接入 |
| 图案有无 | `PatternPresenceDialog` | `PatternPresenceAdapter` | `PatternPresenceHalconRunner` | 已接入 |
| Blob 有无 | `BlobPresenceDialog` | `BlobPresenceAdapter` | `BlobPresenceHalconRunner` | 已接入 |
| 圆有无 | `CirclePresenceDialog` | `CirclePresenceAdapter` | `CirclePresenceHalconRunner` | 已接入 |
| 边有无 | `EdgePresenceDialog` | `EdgePresenceAdapter` | `EdgePresenceHalconRunner` | 已接入 |
| 线有无 | `LinePresenceDialog` | `LinePresenceAdapter` | `LinePresenceHalconRunner` | 已接入 |
| 轮廓有无 | `ContourPresenceDialog` | `ContourPresenceAdapter` | `ContourPresenceHalconRunner` | 已接入 |
| AI 目标检测 | `ObjectDetectionDialog` | `AiDetectionAdapter` | `AiDetectionRunner` | 已接入，依赖远端环境 |
| AI 分类 | `ClassificationDialog` | 暂缺 | 暂缺 | UI 存在，运行能力需补齐或隐藏 |

验收标准：

```text
所有可见可选工具都能运行
运行失败时有明确失败原因
连续运行不会并发踩踏同一帧
OK/NG 统计和单次结果一致
overlay 与工具结果对应
```

### 5.5 算法和依赖模块

目标：把 HALCON、OpenCV、RKNN/YOLO 依赖从个人环境迁移为可交付配置。

建设任务：

- 将 HALCON root、license、动态库路径整理为环境变量优先、配置文件其次、默认路径最后。
- 将 AI 远端主机、脚本、模型、标签、临时目录从源码默认值迁移到 `runtime.json` 或方案配置。
- 为 AI bridge 增加连通性检查、输入文件检查、输出解析失败提示。
- 为 HALCON runner 增加缺 license、缺算子、图像格式不支持等错误信息。

建议配置文件：

```json
{
  "camera": {
    "backend": "opencv_v4l2",
    "device": "/dev/video0"
  },
  "halcon": {
    "root": "/home/kunling/LIB/halcon24.11",
    "license": "/home/kunling/LIB/halcon24.11/license/license_support_halcon24.11_steady_2026_06.dat"
  },
  "ai": {
    "host": "cat@192.168.31.88",
    "script": "/home/cat/qt-AI/scripts/run_rknn_demo.sh",
    "model": "/home/cat/model/yolov8_red_black.rknn",
    "labels": "/home/cat/红黑线标签.txt",
    "localTempDir": "/tmp/v2_ai_bridge"
  }
}
```

验收标准：

```text
更换 HALCON 路径不需要改源码
更换 AI 主机、模型、标签不需要改源码
缺依赖时 smoke 工程能提前暴露问题
错误日志能定位到具体缺失项
```

### 5.6 UI 和交互模块

目标：让界面展示与真实能力一致，降低现场误操作。

建设任务：

- 登录设备列表改为真实设备枚举。
- 工具库只展示当前版本支持的工具，或显式标记未实现。
- 相机参数页只展示当前 backend 可读写的参数。
- 主界面运行状态、相机状态、方案状态、工具状态保持一致。
- 保持中文字体和 UTF-8 构建参数，避免乱码回归。

验收标准：

```text
启动、登录、切换方案、配置工具、单次运行、连续运行流程完整
按钮状态和实际运行状态一致
界面无明显中文乱码
禁用能力不会被误认为可执行
```

### 5.7 测试和验证模块

目标：用最小成本建立可重复验证闭环。

建设任务：

- 建立 `tests/fixtures/` 样例集。
- 覆盖方案读写、工具链空配置、单工具运行、无相机启动、样例图检测。
- 将已有 `smoke/halcon_runtime_smoke.pro` 和 `smoke/ai_detection_bridge_smoke.pro` 纳入验证流程。
- 为相机源提供图片回放 backend，降低没有硬件时的回归成本。

建议样例目录：

```text
tests/fixtures/
  scheme_basic/
    scheme.json
    reference.png
    current_ok.png
    current_ng.png
    expected_results.json
```

验收标准：

```text
无真实相机时也能跑基础回归
有 HALCON 环境时能跑 presence/OCR 冒烟验证
有 AI 远端环境时能跑 AI bridge 冒烟验证
每次关键修改后能确认核心链路未断
```

### 5.8 部署和交付模块

目标：让软件能在新机器或现场机器上按文档复现部署。

建设任务：

- 编写 Linux 部署文档。
- 记录 Qt、OpenCV、HALCON、MVS SDK、GStreamer、RKNN/YOLO 依赖。
- 提供启动脚本，统一设置环境变量。
- 提供现场排障清单：相机不可见、license 不可用、AI 不通、图像绿屏、方案损坏等。
- 确认 release 包内容：可执行文件、资源、配置、依赖说明、默认方案、样例图。

验收标准：

```text
新机器按文档能部署启动
现场替换相机、模型、方案不需要改源码
依赖缺失时能通过日志和排障文档定位
```

## 6. 阶段排期

### 阶段 A：工程基线固化

目标：形成稳定开发基线。

任务：

1. 固定构建环境和依赖版本。
2. 清理构建产物和历史生成文件。
3. 收敛 `.pro` 中的本机绝对路径。
4. 输出构建和启动说明。

交付物：

```text
docs/analysis/build_and_runtime_baseline.md
qt_ui_test_linux.pri
干净 build 流程
```

完成标准：

```text
clean build 通过
程序可启动
日志无乱码
根目录不再产生编译中间文件
```

### 阶段 B：真实相机接入

目标：让项目能连接目标硬件。

任务：

1. 确认相机型号、接口类型、驱动和 SDK。
2. 完成设备枚举。
3. 完成打开、取流、关闭。
4. 接入 UI 设备列表和相机参数页。
5. 验证断线、占用、无权限异常。

交付物：

```text
docs/analysis/camera_integration_report.md
ICameraSource 接口
OpenCvV4L2CameraSource
HikMvsCameraSource 或明确不需要 MVS SDK 的结论
```

完成标准：

```text
真实相机画面进入主界面预览
工具链可使用真实相机帧运行
相机异常不会导致 UI 卡死或崩溃
```

### 阶段 C：工具能力对齐

目标：让用户可见能力与实际算法能力一致。

任务：

1. 维护工具支持矩阵。
2. 隐藏或禁用未接入工具。
3. 补齐 AI 分类等缺口，或明确移出当前版本。
4. 为每个已接入工具建立最小样例。
5. 修正 UI 参数未进入算法的问题。

交付物：

```text
docs/analysis/tool_support_matrix.md
tests/fixtures/ 基础样例
工具配置回归记录
```

完成标准：

```text
可配置工具全部可运行
保存后重开参数不丢失
主界面运行结果与配置页测试结果一致
```

### 阶段 D：配置外置和部署

目标：降低现场部署对源码和个人路径的依赖。

任务：

1. 新增 `config/runtime.json`。
2. 接入 RuntimeConfig 读取和校验。
3. 迁移 HALCON 和 AI 默认路径。
4. 编写启动脚本和部署文档。
5. 补充 smoke 验证流程。

交付物：

```text
config/runtime.json
scripts/run_qt_ui_test.sh
docs/analysis/deployment_guide.md
docs/analysis/site_troubleshooting_checklist.md
```

完成标准：

```text
现场配置可通过 json 或环境变量调整
替换模型、标签、相机设备不需要改源码
缺依赖时可以在启动前或 smoke 阶段暴露
```

### 阶段 E：现场试运行和稳定化

目标：完成试运行闭环，为正式交付做准备。

任务：

1. 在目标设备连续运行。
2. 记录检测 FPS、算法耗时、UI 耗时、丢帧数。
3. 收集现场误检、漏检、相机异常、配置误操作问题。
4. 更新 bug 台账和修复报告。
5. 固化 release 包和版本说明。

交付物：

```text
docs/analysis/site_trial_report.md
docs/analysis/release_checklist.md
release 包
```

完成标准：

```text
连续运行稳定
关键异常有日志和恢复策略
现场使用流程可由操作人员独立完成
```

## 7. 优先级

### P0 必须先做

- 确认真实相机接入方式。
- 建立相机源接口和真实设备枚举。
- 固定构建和运行环境。
- 外置 HALCON、AI、相机配置。
- 工具库与实际 adapter 能力对齐。

### P1 本期应做

- `ToolChainRunner` 从 `MainWindow` 中拆出。
- 最小回归样例和 smoke 流程。
- 方案导入/导出。
- 现场部署文档和排障清单。

### P2 后续优化

- 完整权限系统。
- 多相机管理。
- 工具插件化。
- CMake 化。
- 更完整的自动化测试框架。

## 8. 风险和应对

| 风险 | 影响 | 应对 |
| --- | --- | --- |
| 真实相机不是 UVC，必须走 MVS SDK | 当前 OpenCV/V4L2 路径无法直接使用 | 优先确认型号和 SDK，新增 `HikMvsCameraSource` |
| HALCON license 或运行库依赖个人路径 | 新机器无法启动算法 | 配置外置化，smoke 提前检查 |
| AI 远端主机、模型、脚本路径不固定 | AI 检测不可复现 | RuntimeConfig 管理，增加连通性检查 |
| UI 展示工具多于实际支持工具 | 操作人员配置后无法运行 | 建立支持矩阵，隐藏或禁用未实现工具 |
| `MainWindow` 职责过重 | 后续修改易引入回归 | 分阶段拆出相机管理和工具链运行服务 |
| 没有样例回归 | 修复一个工具可能破坏另一个工具 | 建立 fixtures 和 smoke 流程 |
| 根目录构建产物污染 | 版本管理混乱、误用旧文件 | 统一 shadow build，清理历史产物 |

## 9. 近期执行清单

建议下一轮按以下顺序开工：

1. 记录目标机器 Qt/OpenCV/HALCON/GStreamer/MVS SDK 版本。
2. 确认真实相机型号、接口和系统枚举方式。
3. 运行海康官方 Demo 或 `v4l2-ctl` 验证取流路径。
4. 新增 `ICameraSource` 草案。
5. 将现有 `CameraFrameProvider` 中 OpenCV/V4L2 打开逻辑拆出。
6. 登录页和主界面设备下拉框改为真实枚举。
7. 新增 `config/runtime.json` 和 RuntimeConfig 读取。
8. 建立 `docs/analysis/tool_support_matrix.md`。
9. 建立 `tests/fixtures/` 最小样例。
10. 将 smoke 验证流程写入部署文档。

## 10. 本期核心验收目标

本期最重要的验收目标是：

```text
在目标 Linux 机器上启动软件
  -> 枚举真实相机
  -> 选择设备并打开取流
  -> 采集或导入基准图
  -> 配置至少一个 HALCON 有无检测工具
  -> 单次运行输出 OK/NG 和 overlay
  -> 连续运行稳定显示统计
  -> 关闭和重启后方案配置不丢失
```

如果使用 AI 检测，还需要额外验收：

```text
AI 远端环境连通
模型、标签、脚本路径来自配置文件
推理结果能回传并显示检测框
远端失败时主程序不崩溃，错误可定位
```

## 11. 关联文档

```text
docs/analysis/project_architecture_overview.md
docs/analysis/project_progress_and_improvement_plan.md
docs/analysis/bug_fix_log.md
docs/analysis/encoding_garbled_text_fix_report.md
docs/analysis/v3_presence_tools_real_usage_audit.md
docs/analysis/rk_ai_material_inventory_analysis.md
docs/analysis/ai_detection_remote_rknn_bridge_stage4_report.md
```
