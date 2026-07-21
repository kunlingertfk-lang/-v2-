# qt_znxj_v2_work_1920_515 项目分析

> 分析日期：2026-07-16  
> 项目路径：`/home/tt/tfk/WorkerSpace/project/V2_615_1628/qt_znxj_v2_work_1920_515`  
> 分析范围：只读检查目录、工程入口、构建配置和第三方依赖；未修改任何源代码、工程文件或构建产物。

## 1. 项目概览

这是一个运行在 Ubuntu x86_64 上的 Qt Widgets 工业视觉应用。主界面围绕设备登录、方案管理、相机取流、视觉工具配置、工具链执行和结果展示展开。

核心技术栈：

- Qt 5.15.2：Widgets、Concurrent、Multimedia、MultimediaWidgets。
- C++17，GCC/G++ 11.4.0。
- OpenCV：图像容器、相机采集、显示/格式转换及 AI 桥接。
- HALCON 20.11.1：主要工业视觉算法，通过 `dlopen`/`dlsym` 在运行时加载 C API。
- qmake 3.1 + GNU Make 4.3：主工程构建链。

当前 Git 分支为 `main`，相对 `gitlab/main` 领先 156 个提交、落后 1 个提交。分析开始前工作区已有 137 项变化（127 个修改、4 个删除、6 个未跟踪项）。这些既有变化均未被本次分析修改或清理。

## 2. 项目目录结构

以下结构忽略 `.git/`、`.worktrees/`、`backup/` 和各类 `build*` 构建产物目录，突出当前开发相关内容：

```text
qt_znxj_v2_work_1920_515/
├── AGENTS.md                         # 项目级开发约束
├── README.md                         # 项目说明
├── INSTALLATION.md                   # 依赖安装与构建说明
├── qt_ui_test.pro                    # 主 qmake 工程入口
├── Makefile                          # 已生成的 qmake Makefile（构建产物）
├── local_paths.pri.example           # 本地依赖路径模板
├── qmake/
│   ├── opencv.pri                    # OpenCV 查找与链接规则
│   └── halcon_20_11.pri              # HALCON 20.11 头文件与版本校验
├── scripts/
│   ├── dependencies.env              # 当前机器依赖环境（本地生成）
│   ├── install_dependencies.sh       # 依赖发现/安装脚本
│   └── setup.sh                      # 检查、qmake、make、运行入口
├── src/
│   ├── main.cpp                      # 应用程序入口
│   ├── LoginWindow.*                 # 登录窗口
│   ├── MainWindow.*                  # 主窗口及工具链调度
│   ├── *Dialog.*                     # 工具和功能配置对话框
│   ├── algorithms/
│   │   ├── ai/                       # AI 推理桥接（约定的非 HALCON 例外）
│   │   ├── halcon/                   # HALCON 路径、License、运行库解析
│   │   ├── ocr/                      # OCR HALCON runner
│   │   ├── presence/                 # 图案/Blob/圆/边/线/轮廓有无检测
│   │   └── recognition/              # 颜色、注册分类等识别算法
│   ├── frame/                        # 相机帧、图像转换与参考图提供
│   ├── tooladapters/                 # 配置解析、runner 调用、结果统一
│   └── toolcore/                     # ToolConfig/Request/Result/Engine 等核心模型
├── ui/                               # 19 个 Qt Designer `.ui` 文件
├── resources/                        # QRC、图标、图片和字体
├── styles/                           # 全局 QSS
├── projects/                         # 方案 JSON 与参考图
├── smoke/                            # 独立 qmake 冒烟工程
├── tests/                            # 回归/冒烟源码与样例图
├── docs/                             # 功能文档、设计、分析和验证记录
└── rk_ai_materials/
    ├── model/                        # RKNN 模型
    ├── qt_ai/                        # Qt AI 示例/桥接材料
    └── yolo_demo/                    # 独立 CMake YOLO/RKNN 示例
```

代码规模（开发目录粗略统计）：89 个 `.cpp`、76 个 `.h`、19 个 `.ui`、18 个 `.pro`、2 个 `.pri`。根 `src/` 主要是窗口/对话框，算法、帧、adapter 和工具核心按职责分层。

## 3. Qt 工程入口分析

### 3.1 构建入口

主工程入口为根目录 `qt_ui_test.pro`：

- `TEMPLATE = app`
- `TARGET = qt_ui_test`
- `CONFIG += c++17`
- Qt 模块：Widgets、Concurrent、Multimedia、MultimediaWidgets
- 默认输出到 `build/qt_ui_test/` 下的 `bin/`、`obj/`、`moc/`、`rcc/`、`ui/`
- 引入 `qmake/opencv.pri` 和 `qmake/halcon_20_11.pri`
- 链接 `-ldl`，供 HALCON 动态加载使用

### 3.2 运行入口

`src/main.cpp::main()` 的启动顺序：

1. 调用 `HalconRuntimePaths::initializeHalconEnvironment()`，在创建 Qt 应用前解析/设置 HALCON License。
2. 创建 `QApplication`。
3. 将日志写到可执行文件目录下的 `qt_ui_test.log`，并安装 Qt 消息处理器。
4. 设置应用名 `HZZH` 和组织名“汇众智慧”。
5. 从 Qt Resource 加载 `:/styles/app.qss`。
6. 注册退出清理逻辑，停止取流并关闭相机。
7. 创建并显示 `LoginWindow`，进入 Qt 事件循环。

### 3.3 登录到主业务链路

`LoginWindow::handleLogin()` 当前使用硬编码密码 `123456`。验证成功后创建 `MainWindow`、传递设备/用户信息、显示主窗口并关闭登录窗口。

`MainWindow` 构造时注册 OCR、Pattern/Blob/Circle/Edge/Line/Contour Presence、颜色识别/比较、注册分类、AI 检测等 adapter；随后初始化方案状态、预览组件、工具链异步 watcher，并启动相机。

主业务调用关系可概括为：

```text
main.cpp
  -> LoginWindow
  -> MainWindow
  -> ToolEngine
  -> ToolAdapter
  -> HALCON Runner / AI Runner
  -> ToolResult
  -> MainWindow 结果与叠加显示
```

## 4. OpenCV 依赖分析

### 4.1 qmake 配置

`qmake/opencv.pri` 支持两种方式：

1. 设置 `OPENCV_ROOT`/`OPENCV_LIB_DIR`：使用自定义安装，加入 include、library path、RPATH，并链接 `core`、`imgproc`、`highgui`、`videoio`、`imgcodecs`。
2. 未设置 `OPENCV_ROOT`：通过 `pkg-config opencv4` 使用系统 OpenCV。

`scripts/dependencies.env` 当前声明自定义 OpenCV 4.8.0：

```text
OPENCV_ROOT=/home/tt/.local/opencv-4.8.0
```

该目录的头文件和五个主要动态库均存在，版本头显示 4.8.0。

### 4.2 实际使用

OpenCV 使用面较广，但主要职责符合项目约束：

- `CameraFrameProvider` 使用 `cv::VideoCapture`，后端为 GStreamer/V4L2。
- `MatImageConverter`、`FrameInputMetadata` 等负责 `cv::Mat`、`QImage` 和 HALCON 输入之间的桥接。
- 各 dialog、adapter、runner 以 `cv::Mat` 传递输入图像。
- `AiDetectionRunner` 使用 OpenCV 支撑 AI 推理桥接。
- 工业视觉核心算法由 HALCON runner 完成，而非以 OpenCV 替换。

### 4.3 当前不一致风险

工程存在两套 OpenCV 选择结果：

- 较早的根 `Makefile` 和 `build/qt_ui_test/Makefile` 指向自定义 OpenCV 4.8.0。
- 2026-07-16 生成的 `build/Makefile` 通过系统 `pkg-config` 链接 OpenCV；当前主二进制 `ldd` 实际解析到 Ubuntu 系统 OpenCV 4.5（库名后缀 `4.5d`）。

这说明不同终端/Qt Creator 构建时是否加载 `scripts/dependencies.env` 并不一致。后续开发应固定一套 OpenCV 来源，避免 ABI、行为和部署依赖漂移。

## 5. HALCON 依赖分析

### 5.1 构建期

`qmake/halcon_20_11.pri`：

- 从 `HALCON_ROOT` 或环境变量 `HALCONROOT` 获取根目录，默认 `/opt/halcon`。
- 检查 `include/HVersNum.h`。
- 强制主版本 20、次版本 11。
- 将 HALCON include 目录加入 `INCLUDEPATH`。
- 不直接把 `libhalconc` 链接进主程序。

当前 `/opt/halcon` 检测为 HALCON 20.11.1，头文件和以下运行库存在：

```text
/opt/halcon/lib/x64-linux/libhalconc.so
/opt/halcon/lib/x64-linux/libhalconc.so.20.11.1
```

### 5.2 运行期

工程通过 `HalconRuntimePaths` 解析 License、HALCON C 库和 OCR 模型路径。多个 runner 使用 `dlopen`/`dlsym` 动态装载 HALCON C API，因此 `ldd` 不会显示直接的 HALCON 链接依赖。

当前 HALCON License 文件可读，但 `scripts/dependencies.env` 指向的 License 文件位于 HALCON 24.11 软件目录，而运行库是 HALCON 20.11.1。仅凭文件可读无法确认 License 与 20.11 runtime 的实际授权兼容性；运行 HALCON 冒烟测试时应重点验证。

### 5.3 设计约束

项目 `AGENTS.md` 明确要求：工业视觉算子核心必须使用 HALCON；OpenCV 仅作为采集、容器、显示和格式桥接。AI 检测桥接是明确例外。

## 6. 海康 SDK 依赖分析

系统已安装海康 MVS SDK：

```text
/opt/MVS/include/MvCameraControl.h
/opt/MVS/lib/64/libMvCameraControl.so
```

已安装包名为 `mvs`，动态库版本为 4.8.0.3。

但是当前主工程未接入该 SDK：

- `qt_ui_test.pro` 没有 `/opt/MVS/include`、`-lMvCameraControl` 或相关库路径。
- `src/` 中没有 `MvCameraControl.h`、`MV_CC_*`、`HCNetSDK` 等 API 调用。
- 当前相机实现 `CameraFrameProvider` 使用 OpenCV `VideoCapture`，依次尝试虚拟机设备节点、GStreamer pipeline 和 V4L2。
- 唯一“海康”文字引用是对 `.scbin` 专有模型格式不解析的提示，不构成 SDK 集成。

结论：海康 MVS SDK 处于“机器已安装、项目未使用”状态。如后续要求直连海康工业相机，需要单独设计设备枚举、句柄生命周期、取流线程、像素格式转换、异常恢复，以及 qmake include/library/RPATH 配置。

## 7. 当前编译方式

### 7.1 主工程

主工程明确使用 **qmake + GNU Make**，不是 CMake：

```bash
cd /home/tt/tfk/WorkerSpace/project/V2_615_1628/qt_znxj_v2_work_1920_515
source scripts/dependencies.env
mkdir -p build
cd build
"$QT_ROOT/bin/qmake" ../qt_ui_test.pro
make -j"$(nproc)"
```

也可使用：

```bash
./scripts/setup.sh --build
./scripts/setup.sh --run
```

检测到的主工具链：

- qmake 3.1 / Qt 5.15.2
- GNU Make 4.3
- GCC/G++ 11.4.0

当前系统找不到 `cmake` 命令。根工程不依赖 CMake；只有 `rk_ai_materials/yolo_demo/CMakeLists.txt` 是独立的 YOLO/RKNN 示例工程，如果需要构建该示例，必须先安装 CMake。

### 7.2 测试工程

`smoke/` 和 `tests/` 下存在多个独立 `.pro`，仍以 qmake 构建。大量 `build*` 目录和 Makefile 表明当前主要采用影子构建，但项目根也残留一份历史 in-source `Makefile`。

## 8. 主要风险与建议开发任务

以下仅为分析结论和后续建议，本次未执行代码修改：

1. **固定 OpenCV 构建来源**：统一 Qt Creator、终端和脚本是否加载 `scripts/dependencies.env`，避免 4.5/4.8 混用。
2. **验证 HALCON License 兼容性**：运行 `smoke/halcon_runtime_smoke.pro`，确认 20.11.1 runtime 可使用当前 License，并验证必要符号。
3. **明确海康相机接入需求**：若目标硬件是海康工业相机，当前 OpenCV/V4L2 方案不等同于 MVS SDK 集成，应先形成独立接口与生命周期设计。
4. **保护脏工作区**：开始任何开发前先确认当前 137 项变化的归属；不要清理、覆盖或混入无关提交。
5. **同步远端分支**：当前分支领先 156、落后 1；合并/变基前应先评估大量本地变化与工作树状态。
6. **避免提交构建产物**：继续使用影子构建，并遵守 `AGENTS.md` 中对 Makefile、对象文件、日志和二进制的忽略规则。
7. **登录安全性**：`LoginWindow` 使用硬编码密码，适合原型但不适合正式安全边界；若进入生产，应单独规划身份与凭据管理。

## 9. 分析结论

- 主工程入口：`qt_ui_test.pro`，运行入口：`src/main.cpp`。
- 主构建方式：qmake + make；根工程不是 CMake。
- Qt：5.15.2，C++17。
- OpenCV：配置目标为 4.8.0，但最新实际构建产物使用系统 4.5，存在环境不一致。
- HALCON：20.11.1，运行时动态加载；License 可读但版本兼容性需要运行验证。
- 海康 SDK：MVS 4.8.0.3 已安装，但项目源码和 qmake 均未接入。
- 本次只新增本分析文档，不修改任何项目代码。
