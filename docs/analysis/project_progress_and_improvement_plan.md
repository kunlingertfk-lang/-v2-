# 项目进程与完善计划

> 日期：2026-06-22  
> 对象：`qt_ui_test.pro` 主工程  
> 当前阶段：V2 工程已具备 Qt UI、方案存储、相机帧入口、工具链调度、传统视觉/OCR/AI 检测适配的主体框架；下一阶段重点是“真实硬件接入、运行链路稳定化、配置工程化、测试闭环”。

## 1. 当前做到的程度

### 1.1 工程运行

- 主工程是 Qt Widgets + qmake 工程，目标名为 `qt_ui_test`。
- 已在 Linux/WSL Qt 5.15.2 GCC 64bit 环境下完成构建验证。
- 当前可执行文件能启动进入 Qt 事件循环。
- HALCON 环境可通过以下变量跑通 license 识别：

```bash
HALCONROOT=/home/kunling/LIB/halcon24.11
HALCON_LICENSE_FILE=/home/kunling/LIB/halcon24.11/license/license_support_halcon24.11_steady_2026_06.dat
```

### 1.2 UI 与配置流程

- 已有登录页、主监控页、相机参数页、基准图页、工具库、工具链配置页、输出页。
- 登录逻辑仍是本地静态逻辑：
  - 设备列表硬编码为三台 `MV-SCA002C-03S-WBN-NR`。
  - 用户列表硬编码为 `管理员`、`工程师`、`操作员`。
  - 密码固定为 `123456`。
- 方案存储已经形成 `projects/scheme_xxx/scheme.json + reference.png` 的目录模型。
- UI 中文乱码问题已修复一轮：
  - 日志/终端输出改为 UTF-8。
  - qmake 增加 UTF-8 编译参数。
  - 启动时加载内置中文字体 `ALIMAMASHUHEITI-BOLD.OTF`。
  - 根目录旧 `ui_*.h` 已移入 `backup/stale_generated_ui_headers_20260622/`，避免覆盖 build 目录新生成 UI 头。

### 1.3 图像与相机链路

- 当前统一相机入口是 `CameraFrameProvider`。
- `CameraFrameProvider` 通过 OpenCV `cv::VideoCapture` 接入标准 Linux 视频设备。
- 当前尝试打开顺序：

```text
/dev/video00
/dev/video10
/dev/video20
/dev/video30
/dev/video0
```

- 支持路径：
  - V4L2：`cv::CAP_V4L2`
  - GStreamer：`1920x1080 MJPG -> NV12`
  - GStreamer：`640x480 MJPG -> BGR`
  - 普通 V4L2：`/dev/video0`
- 当前没有接入海康 MVS SDK：
  - 工程未包含 `MvCameraControl.h`。
  - 工程未链接 `libMvCameraControl`。
  - 当前环境未发现海康 MVS SDK 文件。
- 因此，当前只能打开被系统暴露为 `/dev/video*` 的相机；不能直接打开 GigE/USB3 海康工业相机 SDK 设备。

### 1.4 工具链与算法

- `ToolConfig`、`ToolRequest`、`ToolResult`、`ToolAdapter`、`ToolEngine` 已经形成工具抽象层。
- 主界面注册的实际 adapter：

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

- 传统视觉/OCR 通过 HALCON runner 执行。
- AI 检测通过远端 RKNN/YOLO 桥接执行，默认远端主机和路径仍写在配置默认值中。
- 多个有无检测工具已按海康式体验推进过 ROI、模板、轮廓点、区域显示和判定逻辑修复。

### 1.5 已建立的过程资产

- 架构总览：`docs/analysis/project_architecture_overview.md`
- Bug 台账：`docs/analysis/bug_fix_log.md`
- 乱码修复报告：`docs/analysis/encoding_garbled_text_fix_report.md`
- AI 迁移和远端桥接分析/报告。
- 有无检测系列阶段报告和回归分析。

## 2. 当前关键缺口

### 2.1 相机硬件缺口

当前项目没有真正枚举海康工业相机，也没有使用海康 SDK 打开设备。登录页显示的设备只是硬编码 UI 文本，不代表实际连接相机。

需要补齐：

- 海康 MVS SDK 安装路径确认。
- 头文件、库文件、运行时动态库接入。
- 设备枚举、打开、取流、关闭、异常重连。
- GigE/USB3 参数配置，例如曝光、增益、触发、帧率、包大小等。
- 与现有 `CameraFrameProvider` 的统一接口适配。

### 2.2 运行环境工程化不足

- HALCON 路径、license、AI 远端主机、模型路径等仍依赖本机绝对路径或默认值。
- OpenCV/HALCON/Qt 依赖没有统一写入配置说明或 `.pri`。
- 根目录长期混有历史构建产物，虽然 `.gitignore` 已覆盖，但实际目录仍有污染风险。

### 2.3 UI 与真实能力不完全一致

- 工具库展示的工具类型多于实际可运行 adapter。
- 登录设备列表不是真实枚举。
- 部分配置页面是海康式 UI 迁移结果，但参数并不一定全部进入算法执行。

### 2.4 测试闭环不足

- 目前主要靠人工运行和 smoke 工程。
- 缺少针对 `SchemeStore`、`ToolEngine`、核心 adapter、相机 Provider 的自动化验证。
- 缺少可复现的样例方案、样例图、期望结果基准。

## 3. 下一把建议怎么做

下一把建议优先做“相机接入澄清和最小可用硬件链路”，不要继续先扩 UI。

### 3.1 第一优先级：确认真实相机接入方式

目标：明确实际要支持的是哪类设备。

检查项：

```text
1. 海康相机型号：USB3 Vision / GigE Vision / UVC
2. Linux 下是否能看到 /dev/video*
3. 是否安装海康 MVS SDK
4. 是否有 MvCameraControl.h 和 libMvCameraControl.so
5. 海康官方 Demo 是否能枚举并取流
```

判断标准：

- 如果相机能作为 `/dev/video*` 出现，当前 `CameraFrameProvider` 可继续走 OpenCV/V4L2。
- 如果是标准海康工业相机，且只在 MVS SDK 中可见，则必须新增海康 SDK Provider。

### 3.2 第二优先级：抽象相机源接口

建议新增接口层，不要把海康 SDK 直接塞进 UI：

```text
ICameraSource
  open()
  close()
  start()
  stop()
  currentFrame()
  frameReady()
  error()

OpenCvV4L2CameraSource
HikMvsCameraSource
ImageFileCameraSource
```

然后让 `CameraFrameProvider` 变成统一协调层：

```text
CameraFrameProvider
  -> ICameraSource
    -> OpenCvV4L2CameraSource / HikMvsCameraSource
```

这样后续本地图片、视频文件、海康相机、UVC 相机都能走同一个工具链输入。

### 3.3 第三优先级：真实设备枚举接入 UI

登录页和主界面设备下拉框应来自真实枚举结果：

```text
DeviceInfo {
  id
  displayName
  vendor
  transport
  serialNumber
  backend
}
```

UI 显示设备名，内部保存 `id/backend`，避免现在硬编码三台假设备。

### 3.4 第四优先级：配置外置化

新增统一配置文件，例如：

```text
config/runtime.json
```

建议包含：

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
    "model": "/home/cat/model/yolov8_red_black.rknn",
    "labels": "/home/cat/红黑线标签.txt"
  }
}
```

### 3.5 第五优先级：做最小回归测试集

建立一个可重复跑的样例集：

```text
tests/fixtures/
  reference.png
  current_ok.png
  current_ng.png
  scheme.json
  expected_results.json
```

先覆盖：

- 方案读取/保存。
- 工具链空配置运行。
- 单个 PatternPresence/Blob/Circle/Line/Edge/Contour 配置运行。
- 无相机时 UI 不崩溃。
- 有样例图时工具结果可复现。

## 4. 项目完善路线图

### 阶段 A：稳定运行环境

- 固定 Linux Qt Kit 构建流程。
- 保留 `build/` 作为唯一构建输出。
- 清理根目录构建产物。
- 拆出 `qt_ui_test_linux.pri`。
- 写清 HALCON/OpenCV/Qt/MVS SDK 版本。

完成标准：

```text
Clean + qmake + build 一次通过
启动无乱码
日志明确
无根目录生成物参与编译
```

### 阶段 B：相机真实接入

- 先完成设备探测文档。
- 若走 `/dev/video*`，完善 OpenCV/V4L2 枚举和选择。
- 若走海康 MVS，新增 `HikMvsCameraSource`。
- UI 设备列表改为真实枚举。
- 相机参数页读写真实曝光、增益、触发模式等。

完成标准：

```text
能枚举真实设备
能选择设备
能打开设备
能连续取流
断开/占用/无权限时 UI 不崩溃且错误清楚
```

### 阶段 C：工具能力对齐

- 建立 `ToolType -> Adapter -> Dialog -> Runner` 支持矩阵。
- 隐藏或标记未实现工具。
- 对每个已实现工具补最小样例和期望结果。
- 修正 UI 参数和算法参数未接通的问题。

完成标准：

```text
用户能看到的工具都能运行
保存后重新打开参数不丢
测试运行和主流程运行结果一致
```

### 阶段 D：部署和交付

- 外置运行配置。
- 整理 HALCON、OpenCV、MVS SDK、AI 桥接依赖。
- 提供 Release 打包流程。
- 提供现场排障清单。

完成标准：

```text
新机器按文档可复现部署
缺依赖时错误可定位
现场替换相机/模型/方案不需要改源码
```

## 5. 下一次开工建议清单

建议下一次直接从以下事项开始：

1. 在目标 Linux 机器安装/确认海康 MVS SDK。
2. 运行海康官方 Demo，确认相机能被 SDK 枚举和取流。
3. 把 SDK 路径、头文件、库文件位置记录到文档。
4. 新增 `docs/analysis/hik_camera_integration_plan.md`。
5. 新增 `ICameraSource` 接口草案。
6. 将现有 OpenCV/V4L2 逻辑从 `CameraFrameProvider` 拆成 `OpenCvV4L2CameraSource`。
7. 再新增 `HikMvsCameraSource`，先只做枚举、打开、取一帧。

优先级最高的验收目标：

```text
点击相机参数页 -> 枚举真实海康设备 -> 打开选中设备 -> 画面出现在现有 FrameViewHelper 中
```

## 6. 2026-06-22 本机内置摄像头运行验证

### 6.1 验证环境

- 运行环境：WSL/Linux，Qt 5.15.2 GCC 64bit，OpenCV 4.8.0。
- 设备枚举结果：系统暴露了 `Integrated RGB Camera`。
- V4L2 节点：

```text
/dev/video0  彩色 Video Capture，支持 MJPG/YUYV
/dev/video1  metadata
/dev/video2  GREY 640x360
/dev/video3  metadata
```

### 6.2 运行观察

- 旧的根目录 `./qt_ui_test` 不能直接运行，原因是它链接的是缺失的 OpenCV 4.5 debug 动态库：`libopencv_core.so.4.5d` 等。
- 可运行版本是 `build/codex_run_release/qt_ui_test`，依赖 OpenCV 4.8 和 `/home/kunling/Qt/5.15.2/gcc_64/lib`。
- 项目相机入口 `CameraFrameProvider` 能打开 `/dev/video0` 并连续取帧。
- 修复前，项目 V4L2 fallback 实际进入 YUYV/默认路径，保存帧表现为大面积绿屏，不能认为准确成像。
- `ffmpeg` 直接抓取 `/dev/video0` 的 MJPG 可以得到正常画面，但启动时出现过 corrupted data 提示，说明 WSL 摄像头透传初期帧不稳定。
- OpenCV 探针验证：
  - 默认/YUYV 路径：持续绿屏或仅顶部少量真实画面。
  - MJPG 路径：预热数帧后画面正常。

### 6.3 原因

根因不是摄像头不存在，也不是权限问题。当前用户属于 `video` 组，`/dev/video0` 可打开。

实际问题是格式选择和启动帧稳定性：

- 这台内置摄像头的彩色可用格式是 MJPG/YUYV。
- 项目原 V4L2 fallback 强制设置 `UYVY`，设备最终回落到 YUYV。
- 在当前 WSL/OpenCV 组合下，YUYV 路径读出的三通道帧不能被项目当作标准 BGR 使用，导致绿屏。
- MJPG 路径可正确解码，但刚启动时前几帧可能损坏，需要预热丢弃。

### 6.4 已处理办法

已修改 `src/frame/CameraFrameProvider.cpp`：

- `initCameraWithV4L2()` 改为按 `MJPG -> YUYV -> UYVY` 顺序尝试。
- `testCameraRead()` 改为连续读取 6 帧，用最后一帧作为打开成功判定，避开启动瞬间坏帧。

重新编译：

```bash
cd build/codex_run_release
make -j2
```

验证结果：

- 修复后日志显示按 `format= MJPG` 打开 `/dev/video0`。
- `CameraFrameProvider` 500ms 后拿到第 15 帧。
- 保存图 `docs/analysis/webcam_provider_probe_after_fix.jpg` 成像正常，颜色、比例、内容均正确。

### 6.5 留存验证图

```text
docs/analysis/webcam_probe_video0_640.jpg          ffmpeg 直接抓取参考图
docs/analysis/opencv_default_late.jpg              OpenCV 默认/YUYV 绿屏示例
docs/analysis/opencv_mjpg_late.jpg                 OpenCV MJPG 稳定后正常示例
docs/analysis/webcam_provider_probe.jpg            修复前项目 Provider 绿屏示例
docs/analysis/webcam_provider_probe_after_fix.jpg  修复后项目 Provider 正常示例
```
