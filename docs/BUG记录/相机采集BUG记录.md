# 相机采集 BUG 记录

## 维护约定

本文件持续记录相机采集、断连检测、自动恢复和实时预览链路中的 Bug。新问题按发现时间追加，不再为同一功能新建独立文档。

## 问题索引

| 编号 | 发现时间 | 状态 | 问题 |
|---|---|---|---|
| CAM-20260803-01 | 2026-08-03（时刻未记录） | 已修复，待真实摄像头拔插复测 | 摄像头断连后采集画面永久卡住 |

## CAM-20260803-01 2026-08-03（时刻未记录） — 摄像头断连后采集画面永久卡住

### 基本信息

- 发现日期：2026-08-03
- 状态：已修复，待真实摄像头拔插复测
- 影响模块：相机采集、主界面实时预览、相机参数页
- 相关设备：Alcor Micro `058f:1412 PC Camera`
- 运行环境：VMware 虚拟机、Linux UVC/V4L2、OpenCV `VideoCapture`、GStreamer

### 问题现象

摄像头开始采集后可以正常显示数秒，随后画面停留在最后一帧且不再更新。程序本身通常仍可响应，但即使摄像头重新枚举，采集画面也不会自行恢复，用户只能停止或重启程序。

应用日志在故障后持续输出：

```text
[CameraFrameProvider] failed to grab frame, count: 1
[CameraFrameProvider] failed to grab frame, count: 120
...
```

### 复现与诊断证据

第一次现场复现：

- `16:25:32.906`：应用首次报告 `failed to grab frame, count: 1`；
- `16:25:32`：Linux 内核同一秒报告 `USB disconnect`。

第二次现场复现：

- `16:27:06.404`：应用再次首次报告取帧失败；
- `16:27:06`：Linux 内核再次报告同一摄像头 `USB disconnect`。

设备重新枚举期间，内核还出现以下错误：

```text
usb_set_interface failed
cannot submit urb 0, error -2: endpoint not enabled
device descriptor read/64, error -110
```

运行态检查显示 Qt 主线程仍处于正常事件循环，采集线程也没有发生互斥锁死；停止的是底层 USB/UVC 视频流。因此本次故障不是算法执行或 UI 线程卡死。

### 根因

本问题包含两层原因：

1. **设备侧首因**：摄像头所在 USB/UVC 链路发生真实断连。当前摄像头经过 VMware USB/XHCI 转发，断连可能来自摄像头、线缆、USB 端口、供电或 VMware USB 直通链路。
2. **软件侧放大问题**：`CameraFrameProvider::workerLoop()` 在 `grab()` / `retrieve()` 失败后只休眠 5ms 并无限重试，未释放已经失效的 `VideoCapture` 句柄，也没有等待设备重新出现并重新打开。即使 USB 设备重新枚举，采集线程仍绑定旧句柄，画面会永久停在最后一帧。

此外，当前程序固定打开 `/dev/video0`。界面显示的 `MV-SCA002C` 名称没有真正参与设备选择；现场实际连接的是通用 UVC `PC Camera`。

### 修复方案

1. 为采集线程增加连续失败计数；连续 30 次取帧失败后判定视频流已经中断，避免把偶发单帧失败误判为掉线。
2. 首次进入重连状态时清空旧画面，并发送“相机连接中断，正在自动重连”错误状态。
3. 释放失效的 `VideoCapture` 句柄，不再对旧句柄无限重试。
4. 每隔 1 秒检查原设备节点是否恢复；恢复后按照上次成功的 GStreamer/V4L2 模式重新创建采集链路。
5. 重连成功后复用原采集线程继续推送新帧，无需重启应用。
6. 重连退避过程每 50ms 检查停止和线程中断状态，保证程序退出或主动停止采集时可以及时结束。
7. 记录明确的掉线与恢复日志：

```text
[CameraFrameProvider] camera stream lost; starting reconnect
[CameraFrameProvider] camera reconnected successfully
```

### 修复后预期行为

- 短暂丢失一两帧不会触发相机重开。
- 确认视频流中断后，界面不再永久显示失效的最后一帧。
- 摄像头重新出现在原设备节点后，程序自动重建采集链路并恢复画面。
- 摄像头未恢复时按 1 秒间隔重试，不再每 5ms 高速刷失败日志。
- 自动重连期间仍可以正常停止采集或退出程序。

### 验证记录

- `qmake qt_ui_test.pro`：通过。
- 主工程完整 `make -j`：通过。
- `git diff --check`：通过。
- 无摄像头设备情况下启动程序：通过，无崩溃。
- 真实摄像头拔插恢复：待现场执行；记录编写时摄像头已经不在虚拟机 `/dev/video*` 和 USB 设备列表中，无法完成真实热插拔验证。

### 现场复测步骤

1. 将摄像头接入虚拟机，确认 `/dev/video0` 存在。
2. 启动最新构建并确认实时画面正常更新。
3. 采集过程中断开摄像头，确认日志出现 `camera stream lost; starting reconnect`，界面进入无图像状态且程序仍可操作。
4. 数秒后将摄像头重新接回同一 USB 口。
5. 确认日志出现 `camera reconnected successfully`，实时画面自动恢复，无需重启程序。
6. 重复拔插至少 3 次，并分别检查主界面和相机参数页。
7. 在重连等待期间关闭程序，确认采集线程可以正常退出。

### 已知边界与后续建议

- 当前自动重连以原设备路径 `/dev/video0` 为目标。如果重新枚举后设备号变为 `/dev/video2` 等其他路径，本次实现不会自动迁移，后续应使用 `/dev/v4l/by-path/`、USB VID/PID 或序列号进行稳定设备识别。
- 若独立 `v4l2-ctl` 连续采集仍出现内核 `USB disconnect`，应继续处理摄像头、线缆、端口、供电或 VMware USB 直通问题；软件自动重连只能恢复服务，不能消除底层频繁掉线。
- 后续建议增加相机连接状态信号和最后成功帧时间戳，使主界面能够明确区分“正在采集”“正在重连”和“设备不可用”。

### 相关文件

- `src/frame/CameraFrameProvider.h`
- `src/frame/CameraFrameProvider.cpp`
- `src/MainWindow.cpp`
- `src/CameraParamsDialog.cpp`
