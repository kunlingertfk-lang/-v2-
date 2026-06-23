# 智能相机 Qt 项目架构分析

> 分析对象：`qt_ui_test.pro` 主工程  
> 分析日期：2026-06-22  
> 当前结论：这是一个基于 Qt Widgets 的机器视觉桌面应用，核心链路是“登录/方案选择 -> 相机取流/基准图 -> 工具配置 -> 工具链运行 -> 结果显示/输出”。算法层主要由 OpenCV 数据结构承接图像，HALCON runner 执行传统视觉/OCR，AI 检测通过远端 RKNN 推理桥接执行。

## 1. 工程定位

项目主入口是 qmake 工程：

```text
qt_ui_test.pro
```

主程序目标名：

```text
qt_ui_test
```

工程启用的 Qt 模块：

```text
widgets
concurrent
multimedia
multimediawidgets
```

工程语言标准：

```text
C++17
```

从代码和构建配置看，当前主工程更偏 Linux/RK 平台运行：

- OpenCV include 使用 `/usr/local/include/opencv4`。
- 链接参数包含 `-ldl`。
- 相机入口默认 `/dev/video0`。
- 相机采集使用 V4L2/GStreamer/OpenCV `VideoCapture`。
- HALCON 路径默认指向 Linux HALCON 24.11 目录。
- AI 检测默认通过远端 RK 主机、RKNN 模型和 shell 脚本执行。

如果要在 Windows + Qt Creator 跑通，需要单独调整 OpenCV/HALCON/相机采集/`-ldl` 等平台相关配置。

## 2. 顶层目录

```text
.
├─ qt_ui_test.pro                 主 qmake 工程
├─ src/                           C++ 源码
│  ├─ main.cpp                    应用启动、日志、HALCON 环境初始化
│  ├─ *Dialog.* / *Window.*       Qt Widgets 窗口和配置界面
│  ├─ SchemeStore.*               方案持久化
│  ├─ frame/                      图像帧、基准图、显示辅助
│  ├─ toolcore/                   工具抽象数据结构和调度引擎
│  ├─ tooladapters/               ToolConfig 到具体算法 runner 的适配层
│  └─ algorithms/                 HALCON/OCR/AI/有无检测算法实现
├─ ui/                            Qt Designer `.ui` 文件
├─ resources/                     qrc、图标、图片、字体
├─ styles/                        全局 QSS
├─ projects/                      方案数据：scheme.json + reference.png
├─ smoke/                         算法/运行时冒烟测试工程
├─ rk_ai_materials/               RKNN/YOLO 相关模型、demo、样例图
├─ docs/analysis/                 分析报告、bug 台账和历史排查记录
└─ backup/                        UI/源码备份
```

根目录里还存在大量 `.o`、`moc_*.cpp`、`ui_*.h`、`Makefile`、`qt_ui_test` 等构建产物。长期维护建议把构建产物移到独立 build 目录，并通过 `.gitignore` 排除。

Bug 记录入口：

```text
docs/analysis/bug_fix_log.md
```

今后每个 bug 都应先登记到该文档，记录现象、复现步骤、定位流程、解决方案、验证方式和涉及知识点，再根据需要补充独立修复报告。

项目进程与完善计划入口：

```text
docs/analysis/project_progress_and_improvement_plan.md
```

该文档记录当前做到的程度、关键缺口、下一把建议做什么，以及项目完善路线图。

## 3. 启动流程

入口在 `src/main.cpp`：

1. 调用 `HalconRuntimePaths::initializeHalconEnvironment()` 尝试解析 HALCON license。
2. 创建 `QApplication`。
3. 安装 Qt message handler，把日志输出到 stderr 和程序目录下的 `qt_ui_test.log`。
4. 设置应用名 `HZZH`、组织名 `汇众智慧`。
5. 从 `:/styles/app.qss` 加载全局样式。
6. 绑定 `aboutToQuit`，退出前停止相机采集并关闭相机。
7. 显示 `LoginWindow`。

登录窗口 `LoginWindow` 当前是本地静态逻辑：

- 设备列表是硬编码的三台 `MV-SCA002C-03S-WBN-NR`。
- 用户列表是 `管理员`、`工程师`、`操作员`。
- 密码固定为 `123456`。
- 登录成功后创建 `MainWindow`，并把设备名、用户名写入会话信息。

## 4. 主界面职责

`MainWindow` 是运行态核心，承担以下职责：

- 注册所有工具 adapter 到 `ToolEngine`。
- 管理当前方案的工具列表、基准图预览快照、最近一次运行快照。
- 初始化方案选择器，加载 `SchemeStore` 当前方案。
- 启动相机采集并刷新实时预览。
- 打开参数、工具、输出等配置界面。
- 执行单次工具链或连续运行。
- 用 `QtConcurrent::run` 把工具链运行放到后台线程。
- 汇总每个工具的 `ToolResult`，更新 UI 表格、统计 OK/NG 和叠加图形。

主界面内注册的工具 adapter：

```text
OcrAdapter
PatternPresenceAdapter
BlobPresenceAdapter
CirclePresenceAdapter
ContourPresenceAdapter
EdgePresenceAdapter
LinePresenceAdapter
AiDetectionAdapter
```

这说明当前运行链路真正接入执行的工具类型集中在 OCR、有无检测、轮廓/边/线/圆/blob 检测和 AI 检测。工具库里展示的部分类型可能还只是 UI 或配置占位。

## 5. 界面模块

主要窗口和对话框：

```text
LoginWindow                  登录页
MainWindow                   主监控/运行页
CameraParamsDialog           相机/方案参数页
ReferenceImageDialog         基准图采集/导入页
ToolLibraryDialog            工具选择页
ToolsDialog                  工具链配置页
OutputDialog                 输出配置页
CharacterRecognitionDialog   OCR 配置页
ObjectDetectionDialog        目标检测配置页
PatternPresenceDialog        图案有无配置页
BlobPresenceDialog           Blob 有无配置页
CirclePresenceDialog         圆有无配置页
EdgePresenceDialog           边有无配置页
LinePresenceDialog           线有无配置页
ContourPresenceDialog        轮廓有无配置页
ClassificationDialog         分类配置页
```

配置类对话框的共同模式：

1. 从 `ToolConfig` 加载已有配置。
2. 允许用户在图像上设置 ROI 或模板区域。
3. 生成新的 `ToolConfig`。
4. 生成 `ToolPreviewSnapshot`，用于配置阶段预览和主界面回显。
5. 测试模式下会临时创建 adapter + `ToolEngine` 对当前帧或基准图运行一次。

`PlanDialogUtils` 负责窗口尺寸、窗口切换和会话信息等 UI 公共逻辑。

## 6. 图像帧和显示层

图像相关模块集中在 `src/frame/`：

```text
CameraFrameProvider      相机单例、取流线程、当前帧缓存
ReferenceImageProvider   基准图单例、基准帧缓存
MatImageConverter        cv::Mat 到 QImage 的显示转换
FrameViewHelper          QGraphicsView 图像显示、ROI 绘制、结果 overlay 显示
```

`CameraFrameProvider` 的关键行为：

- 单例提供相机当前帧。
- 默认打开 `/dev/video0`。
- 优先尝试若干 VM V4L2 设备：`/dev/video00`、`/dev/video10`、`/dev/video20`、`/dev/video30`。
- 再尝试 GStreamer 管线，包括 1920x1080 MJPG -> NV12 和 640x480 MJPG -> BGR。
- 最后尝试普通 V4L2。
- 采集线程使用 `QThread::create` 运行 `workerLoop()`。
- 每帧归一化为 BGR `cv::Mat`，同时发出 `frameUpdated(QImage)`、`frameUpdatedMat()`、`frameIndexChanged(qint64)`。

`FrameViewHelper` 是 UI 图像显示的核心基础设施：

- 设置/清空图像，自动适配视图。
- 支持矩形 ROI、多边形 ROI、圆形 ROI、线带 ROI。
- 把视图坐标、图像坐标、归一化坐标相互转换。
- 根据 `ToolOverlay` 绘制矩形、线、文本、点等检测结果。

## 7. 方案存储

方案由 `SchemeStore` 单例管理，默认根目录是：

```text
projects/
```

每个方案一个目录：

```text
projects/
└─ scheme_xxx/
   ├─ scheme.json
   └─ reference.png
```

`SchemeState` 包含：

- `schemeId`
- `schemeName`
- `schemeDir`
- `referenceImagePath`
- `toolConfigs`
- `referencePreviewSnapshots`
- `outputConfig`
- `updatedAt`

`scheme.json` 的主要结构：

```json
{
  "schemaVersion": 1,
  "schemeId": "scheme_1",
  "schemeName": "5221",
  "referenceImage": "reference.png",
  "tools": [],
  "previews": {},
  "output": {},
  "updatedAt": "2026-06-18T17:06:12"
}
```

保存时使用 `QSaveFile`，这是合理的，能降低写入中断造成 JSON 损坏的概率。

## 8. 工具抽象层

工具核心抽象在 `src/toolcore/`：

```text
ToolTypes.h              ToolType / ToolCategory 枚举及字符串转换
ToolConfig.h             工具配置，可序列化为 JSON
ToolRequest.h            工具运行请求，包含 ToolConfig、当前图、基准图和运行上下文
ToolResult.h             工具运行结果，包含 success/ok/status/message/score/count/overlays/payload
ToolOverlay.h            结果叠加图形
ToolPreviewSnapshot.h    配置或运行时的预览快照
ToolAdapter.h            工具适配器接口
ToolEngine.h/.cpp        工具注册和顺序执行引擎
```

核心调用关系：

```text
MainWindow
  -> ToolEngine::runTools(configs, image, referenceImage)
    -> ToolEngine::runTool(request)
      -> findAdapter(config.toolType)
        -> ToolAdapter::run(request)
          -> Algorithm Runner
            -> ToolResult
```

`ToolEngine` 目前是顺序执行每个启用工具，不做工具间依赖解析，也不做并行化。单次工具链整体被 `MainWindow` 放到 `QtConcurrent` 后台线程中运行。

## 9. Adapter 和算法层

Adapter 层位于 `src/tooladapters/`，职责是把通用 `ToolConfig` 转换为具体 runner 的配置，并把 runner 结果转换回 `ToolResult`。

```text
OcrAdapter                 -> OcrHalconRunner
PatternPresenceAdapter     -> PatternPresenceHalconRunner
BlobPresenceAdapter        -> BlobPresenceHalconRunner
CirclePresenceAdapter      -> CirclePresenceHalconRunner
ContourPresenceAdapter     -> ContourPresenceHalconRunner
EdgePresenceAdapter        -> EdgePresenceHalconRunner
LinePresenceAdapter        -> LinePresenceHalconRunner
AiDetectionAdapter         -> AiDetectionRunner
```

算法层位于 `src/algorithms/`：

```text
algorithms/halcon/         HALCON 路径、license、动态库候选路径解析
algorithms/ocr/            HALCON OCR runner
algorithms/presence/       图案/blob/圆/边/线/轮廓有无检测 runner
algorithms/ai/             AI 检测 runner，桥接远端 RKNN YOLO 推理
```

传统视觉/OCR：

- 使用 HALCON 动态库路径候选。
- 图像输入以 OpenCV `cv::Mat` 为主。
- 输出统一转换为 `ToolOverlay` 和 `payload`。
- `PatternPresence` 支持模板 ROI、检测 ROI、自动建模、分数阈值、角度/尺度范围等。

AI 检测：

- `AiDetectionRunner` 默认远端主机是 `cat@192.168.31.88`。
- 默认模型路径 `/home/cat/model/yolov8_red_black.rknn`。
- 默认标签路径 `/home/cat/红黑线标签.txt`。
- 默认脚本 `/home/cat/qt-AI/scripts/run_rknn_demo.sh`。
- 本地临时目录默认 `/tmp/v2_ai_bridge`。
- 结果经过解析、过滤、判定后生成检测框 overlay 和 `ToolResult`。

## 10. 主运行链路

一次工具链运行的关键流程：

```text
用户点击单次运行/连续运行
  -> MainWindow 检查启用工具
  -> 确保当前方案基准图已加载到 ReferenceImageProvider
  -> 从 CameraFrameProvider 拷贝当前 cv::Mat
  -> 从 ReferenceImageProvider 拷贝 reference cv::Mat
  -> QtConcurrent 后台线程调用 ToolEngine::runTools
  -> 每个 adapter 调具体 runner
  -> 汇总 ToolResult
  -> 回到 UI 线程更新表格、状态、OK/NG 统计和 overlay
```

连续运行模式通过 `CameraFrameProvider::frameIndexChanged` 触发，使用帧序号避免重复处理旧帧，并统计提交帧、完成帧、丢帧、耗时等性能信息。

## 11. 资源系统

资源入口是：

```text
resources/resources.qrc
```

主要包含：

- `styles/app.qss`
- SVG 图标
- PNG 图片
- 字体 `ALIMAMASHUHEITI-BOLD.OTF`

运行时通过 qrc 路径访问，例如：

```text
:/styles/app.qss
:/icons/device.svg
:/icons/camera.svg
```

## 12. 辅助工程和材料

`smoke/` 下有两个 qmake 冒烟测试：

```text
halcon_runtime_smoke.pro
ai_detection_bridge_smoke.pro
```

用途：

- `halcon_runtime_smoke`：验证 HALCON runtime/OCR/presence runner 的基础可用性。
- `ai_detection_bridge_smoke`：验证 AI 检测桥接逻辑。

`rk_ai_materials/` 包含 RKNN/YOLO 相关材料：

- `model/*.rknn`
- `labels/红黑线标签.txt`
- `yolo_demo/`
- `qt_ai/app_config.json`
- 输入/输出样例图片

这部分更像 AI 算法部署和联调用材料，不是主 Qt 工程的直接编译源码。

## 13. 当前架构优点

1. 工具抽象边界比较清楚：`ToolConfig`、`ToolRequest`、`ToolResult`、`ToolAdapter`、`ToolEngine` 已经形成稳定接口。
2. UI 配置和算法执行通过 adapter 解耦，新增算法时不必直接改 `ToolEngine`。
3. 图像显示和 ROI 交互集中在 `FrameViewHelper`，避免每个对话框重复写坐标转换。
4. 方案以 JSON + 图片目录持久化，便于人工检查和迁移。
5. 单次工具链在后台线程执行，避免长时间算法阻塞 UI。
6. 有 smoke 工程，说明已经开始把复杂运行时依赖拆出来验证。

## 14. 主要风险和维护点

1. 平台耦合较重  
   当前构建配置、相机采集、HALCON 路径、AI 临时路径都明显偏 Linux/RK 环境。Windows 跑通需要独立平台配置，不只是 Qt Creator 打开工程。

2. 根目录混有构建产物  
   `.o`、`moc_*.cpp`、`ui_*.h`、`Makefile`、可执行文件都在项目根目录，容易污染版本管理和人工分析。

3. 登录和设备列表是硬编码  
   当前不是真实权限系统，也没有真实枚举工业相机设备。

4. 工具库和实际 adapter 覆盖不完全一致  
   `ToolTypes` 和 `ToolLibraryDialog` 包含很多工具类型，但实际注册的 adapter 只覆盖一部分。用户可见工具应和实际可执行能力保持一致。

5. `MainWindow` 职责偏重  
   它同时管理方案、相机、工具链、性能统计、UI 表格、预览、连续运行。后续功能继续扩展时，建议拆出运行控制器或工具链服务。

6. AI 检测对远端环境依赖强  
   默认 IP、用户名、路径、脚本、模型都写在配置结构中。部署时需要把这些改为方案级或全局配置，并提供连通性检查。

7. HALCON 路径候选包含历史绝对路径  
   `HalconRuntimePaths` 中有多处本机/历史用户路径。建议收敛为环境变量优先、配置文件其次、打包相对路径最后。

## 15. 建议的后续整理顺序

1. 优先确认真实相机接入方式：`/dev/video*` 还是海康 MVS SDK。
2. 把相机源抽象为接口：V4L2/GStreamer、工业相机 SDK、本地图片、视频文件都走同一帧源接口。
3. 接入真实设备枚举，替换登录页和主界面的硬编码设备列表。
4. 清理构建产物，建立固定的 `build/` 输出目录和 `.gitignore` 规则。
5. 把平台相关配置拆成 `qt_ui_test_linux.pri`、`qt_ui_test_windows.pri` 或 CMake preset。
6. 为 `ToolType` 到 adapter 的支持关系生成一张可维护表，避免 UI 暴露不可运行工具。
7. 从 `MainWindow` 拆出 `ToolChainRunner`，专门负责单次/连续运行、帧去重、性能统计和结果汇总。
8. 把 HALCON/RKNN/远端 SSH 配置从源码默认值迁移到全局配置或方案配置。
9. 给 `SchemeStore`、`ToolEngine`、核心 adapters 增加小型单元测试或 smoke 测试用例。

## 16. 快速阅读入口

新开发者建议按这个顺序读代码：

```text
1. qt_ui_test.pro
2. src/main.cpp
3. src/LoginWindow.cpp
4. src/MainWindow.h / src/MainWindow.cpp
5. src/SchemeStore.h / src/SchemeStore.cpp
6. src/frame/CameraFrameProvider.*
7. src/frame/FrameViewHelper.*
8. src/toolcore/*
9. src/tooladapters/*
10. src/algorithms/*
11. ui/*.ui
12. projects/*/scheme.json
```

掌握以上文件后，基本能理解这个项目的启动、配置、取图、运行、判定、显示和持久化链路。
