# V2 图像发蓝二次诊断报告

日期：2026-05-26

## 1. 当前改动范围

按要求执行：

```bash
git diff --name-only
```

输出如下：

```text
qt_ui_test.pro
src/BlobPresenceDialog.cpp
src/BlobPresenceDialog.h
src/CameraParamsDialog.cpp
src/CameraParamsDialog.h
src/CharacterRecognitionDialog.cpp
src/CharacterRecognitionDialog.h
src/CirclePresenceDialog.cpp
src/CirclePresenceDialog.h
src/LoginWindow.cpp
src/MainWindow.cpp
src/MainWindow.h
src/OutputDialog.cpp
src/OutputDialog.h
src/PatternPresenceDialog.cpp
src/PatternPresenceDialog.h
src/PlanDialogUtils.cpp
src/PlanDialogUtils.h
src/ReferenceImageDialog.cpp
src/ReferenceImageDialog.h
src/ToolLibraryDialog.cpp
src/ToolLibraryDialog.h
src/ToolsDialog.cpp
src/ToolsDialog.h
src/algorithms/ocr/OcrHalconRunner.cpp
src/algorithms/ocr/OcrHalconRunner.h
src/algorithms/presence/BlobPresenceHalconRunner.cpp
src/algorithms/presence/BlobPresenceHalconRunner.h
src/algorithms/presence/CirclePresenceHalconRunner.cpp
src/algorithms/presence/CirclePresenceHalconRunner.h
src/algorithms/presence/PatternPresenceHalconRunner.cpp
src/algorithms/presence/PatternPresenceHalconRunner.h
src/frame/CameraFrameProvider.cpp
src/frame/CameraFrameProvider.h
src/frame/FrameViewHelper.cpp
src/frame/FrameViewHelper.h
src/frame/ReferenceImageProvider.cpp
src/main.cpp
src/tooladapters/BlobPresenceAdapter.cpp
src/tooladapters/CirclePresenceAdapter.cpp
src/tooladapters/OcrAdapter.cpp
src/tooladapters/PatternPresenceAdapter.cpp
styles/app.qss
ui/BlobPresenceDialog.ui
ui/CameraParamsDialog.ui
ui/CirclePresenceDialog.ui
ui/LoginWindow.ui
ui/MainWindow.ui
ui/OutputDialog.ui
ui/PatternPresenceDialog.ui
ui/ReferenceImageDialog.ui
ui/ToolLibraryDialog.ui
ui/ToolsDialog.ui
```

指定文件是否在当前 diff 中：

| 路径 | 是否在 `git diff --name-only` 中 | 本轮是否改动 |
|---|---:|---:|
| `src/frame/MatImageConverter.*` | 否；但 `git status --short` 显示 `MatImageConverter.cpp/.h` 是未跟踪文件 | 否 |
| `src/frame/CameraFrameProvider.*` | 是，cpp/h | 是，仅 `CameraFrameProvider.cpp` 增加一次性颜色诊断 |
| `src/frame/ReferenceImageProvider.*` | 是，cpp | 是，仅 `ReferenceImageProvider.cpp` 增加一次性颜色诊断 |
| `src/FrameViewHelper.*` | 否；实际路径是 `src/frame/FrameViewHelper.*` | 否 |
| `src/frame/FrameViewHelper.*` | 是，cpp/h | 否 |
| `src/PatternPresenceDialog.cpp` | 是 | 否 |
| `src/ToolsDialog.cpp` | 是 | 否 |
| `MainWindow.*` | 是，`src/MainWindow.cpp/.h` | 否 |
| 其他显示相关文件 | 是，包含 `ReferenceImageDialog.*`、多个 presence/OCR dialog、`OutputDialog.*`、多份 `.ui` | 否 |

本轮只改了图像链路诊断：`src/frame/CameraFrameProvider.cpp`、`src/frame/ReferenceImageProvider.cpp`，以及本报告文件。

## 2. 颜色转换点清单

按要求执行：

```bash
rg -n "cvtColor|COLOR_BGR2RGB|COLOR_RGB2BGR|COLOR_BGRA2RGBA|COLOR_RGBA2BGRA|rgbSwapped|Format_RGB888|Format_BGR888|Format_RGBA8888|QImage\\(|QPixmap::fromImage|matToDisplayImage|MatImageConverter" src
```

归纳如下：

| 文件 | 函数/位置 | 是否转换颜色 | 转换方向 | 是否可能重复转换 |
|---|---|---:|---|---|
| `src/frame/MatImageConverter.cpp` | `MatImageConverter::matToDisplayImage` | 是 | `CV_8UC3` 使用 `BGR2RGB`；`CV_8UC4` 使用 `BGRA2RGBA` | 若传入 Mat 已经是 RGB，则会重复/反向；这正是本轮要用诊断图确认的点 |
| `src/frame/MatImageConverter.cpp` | `QImage(... Format_RGB888/Format_RGBA8888)` | 否，包装转换后的 buffer | RGB/RGBA QImage | 不会再次换色 |
| `src/frame/MatImageConverter.cpp` | `logDisplayConversion` | 否 | 无 | 无；当前已按 source 前 5 次限流 |
| `src/frame/CameraFrameProvider.cpp` | `saveCameraColorDebugOnce` | 是，诊断专用 | `BGR2RGB` 后再 `imwrite`，生成对照图 | 否，非生产显示链路 |
| `src/frame/CameraFrameProvider.cpp` | `saveCameraColorDebugOnce` | 是，诊断专用 | 调 `MatImageConverter::matToDisplayImage` 保存 `display_qimage_roundtrip.png` | 否，非生产显示链路 |
| `src/frame/CameraFrameProvider.cpp` | `decodeCapturedFrame` | 是 | `NV12 -> BGR` | 否，provider 归一到 BGR |
| `src/frame/CameraFrameProvider.cpp` | `matToImage` | 是，委托 converter | `BGR -> RGB QImage` | 取决于 `m_currentFrame` 是否真为 BGR |
| `src/frame/CameraFrameProvider.cpp` | `normalizeFrame` | 是 | `GRAY2BGR`、`YUV2BGR_UYVY`、`BGRA2BGR`；`CV_8UC3` 直接 clone | 否，provider 层只归一为 BGR，不转显示 RGB |
| `src/frame/ReferenceImageProvider.cpp` | `saveReferenceColorDebugOnce` | 是，诊断专用 | 调 `MatImageConverter::matToDisplayImage` 保存 `reference_display_qimage.png` | 否，非生产显示链路 |
| `src/frame/ReferenceImageProvider.cpp` | `matToImage` | 是，委托 converter | `BGR -> RGB QImage` | 取决于 `m_referenceFrame` 是否真为 BGR |
| `src/frame/ReferenceImageProvider.cpp` | `normalizeFrame` | 是 | `GRAY2BGR`、`BGRA2BGR`；`CV_8UC3` 直接 clone | 否，provider 层只归一为 BGR |
| `src/ReferenceImageDialog.cpp` | `importReferenceImageFromPc` | 是 | `QImage::Format_RGB888` 包成 RGB Mat，再 `RGB2BGR` | 否，导入路径把外部 RGB 图片转为内部 BGR |
| `src/ReferenceImageDialog.cpp` | live/reference preview callbacks | 否 | 接收 provider 已生成的 QImage | 否 |
| `src/MainWindow.cpp` | namespace `imageFromFrame` | 是，委托 converter | `BGR -> RGB QImage` | 若传入 Mat 已经 RGB 才会重复；当前 `currentFrame()` 证据指向 BGR |
| `src/MainWindow.cpp` | `frameUpdated` lambda / `refreshLivePreview` / `selectSchemeTool` / `showToolSnapshot` | 否 | 直接展示 QImage | 否 |
| `src/PatternPresenceDialog.cpp` | namespace `imageFromFrame` | 是，委托 converter | `BGR -> RGB QImage` | 若传入 Mat 已经 RGB 才会重复；当前 `currentFrame/referenceFrame` 证据指向 BGR |
| `src/PatternPresenceDialog.cpp` | `showProviderImage` / `showLiveImage` | 否 | 展示 provider 发出的 QImage | 否 |
| `src/PatternPresenceDialog.cpp` | `showSingleShotImage` / `runContinuousTick` | 是，调用 `imageFromFrame` | `BGR -> RGB QImage` | 不会对 QImage 再转；和 provider QImage 是两条更新路径，不是二次换色 |
| `src/ToolsDialog.cpp` | namespace `imageFromFrame/currentReferenceImage` | 是，委托 converter | `BGR -> RGB QImage` | 当前 `referenceFrame()` 证据指向 BGR，暂不重复 |
| `src/BlobPresenceDialog.cpp` | namespace `imageFromFrame` | 是，委托 converter | `BGR -> RGB QImage` | 同上 |
| `src/CirclePresenceDialog.cpp` | namespace `imageFromFrame` | 是，委托 converter | `BGR -> RGB QImage` | 同上 |
| `src/CharacterRecognitionDialog.cpp` | namespace `imageFromFrame` | 是，委托 converter | `BGR -> RGB QImage` | 同上 |
| `src/ContourPresenceDialog.cpp` | namespace `imageFromFrame` | 是，委托 converter | `BGR -> RGB QImage` | 同上 |
| `src/EdgePresenceDialog.cpp` | namespace `imageFromFrame` | 是，委托 converter | `BGR -> RGB QImage` | 同上 |
| `src/LinePresenceDialog.cpp` | namespace `imageFromFrame` | 是，委托 converter | `BGR -> RGB QImage` | 同上 |
| `src/frame/FrameViewHelper.cpp` | `FrameViewHelper::setImage` | 否 | `QPixmap::fromImage(m_lastImage)` | 否，不做 `rgbSwapped` |
| `src/algorithms/presence/PatternPresenceHalconRunner.cpp` | 算法内部 | 是 | `BGR2GRAY`、`BGRA2GRAY`、`GRAY2BGR`、`BGRA2BGR` | 与 UI 显示无关；本轮未改 |

额外结论：

- 未发现 `rgbSwapped`。
- 未发现 `Format_BGR888`。
- 未发现 `COLOR_RGBA2BGRA`。
- 生产显示路径的颜色转换集中在 `MatImageConverter::matToDisplayImage()`。

## 3. 内部 Mat 约定确认

### 3.1 CameraFrameProvider 保存的 `currentFrame`

结论：按当前代码和现场日志对应的 GStreamer 路径，`currentFrame` 应为 OpenCV BGR Mat。

证据：

- 当前现场日志为 `640x480 MJPG->BGR`。
- `CameraFrameProvider::initCameraWithGStreamer()` 的 640 pipeline 明确写了 appsink caps：

```text
v4l2src device=/dev/video0 io-mode=mmap ! image/jpeg, width=640, height=480, framerate=30/1 ! jpegdec ! videoconvert ! video/x-raw, format=BGR ! appsink max-buffers=1 drop=true sync=false
```

- 该路径调用 `tryPipeline(pipeline640, false, "640x480 MJPG->BGR")`，因此 `m_useNv12Path=false`。
- `decodeCapturedFrame()` 在 `!m_useNv12Path` 时只调用 `normalizeFrame(frame)`。
- `normalizeFrame()` 对 `CV_8UC3` 直接 `clone()`，没有 BGR2RGB。
- `setCurrentFrame()` 保存 `m_currentFrame = normalized.clone()`。

因此，代码层面没有 provider 层提前 BGR2RGB。真实相机源是否已经偏蓝，要看本轮新增的 `/tmp/v2_color_debug/raw_mat_imwrite.png`。

### 3.2 ReferenceImageProvider 保存的 `referenceFrame`

结论：`referenceFrame` 应为 OpenCV BGR Mat。

证据：

- 从相机抓取基准图：`ReferenceImageDialog::captureReferenceImage()` 直接取 `CameraFrameProvider::currentFrame()`，而当前 `currentFrame` 约定为 BGR。
- 从 PC 导入基准图：`ReferenceImageDialog::importReferenceImageFromPc()` 将 QImage 转 `Format_RGB888`，随后 `cv::cvtColor(..., COLOR_RGB2BGR)`，再交给 `SchemeStore::setReferenceFrame()`。
- 从磁盘加载基准图：`SchemeStore::loadCurrentReferenceIntoProvider()` 使用 `cv::imread(path, cv::IMREAD_COLOR)`，OpenCV 读入为 BGR。
- 保存基准图：`SchemeStore::saveSchemeToFile()` 用 `cv::imwrite(referenceFrame)`，按 BGR 解释写图。
- `ReferenceImageProvider::normalizeFrame()` 对 `CV_8UC3` 直接 clone，不做 RGB 交换。

### 3.3 ToolsDialog / PatternPresenceDialog 传给 MatImageConverter 的 Mat

结论：传入的 Mat 应为 BGR。

证据：

- `ToolsDialog::currentReferenceImage()` 取 `ReferenceImageProvider::referenceFrame()`，再交给 `MatImageConverter`。
- `PatternPresenceDialog::showSingleShotImage()` 和 `runContinuousTick()` 取 `CameraFrameProvider::currentFrame()`，再交给 `MatImageConverter`。
- `PatternPresenceDialog::runReferenceTest()` 取 `ReferenceImageProvider::referenceFrame()`；显示时优先 `ReferenceImageProvider::referenceImage()`，为空时才把 BGR reference Mat 交给 `imageFromFrame()`。

### 3.4 provider 层是否调用了 BGR2RGB

生产路径：没有。

本轮新增的 `CameraFrameProvider::saveCameraColorDebugOnce()` 中有一次 `COLOR_BGR2RGB`，只用于生成 `/tmp/v2_color_debug/raw_after_bgr2rgb_then_imwrite.png` 对照图，不参与 UI 或 provider 保存。

### 3.5 是否存在 MatImageConverter 后又 `rgbSwapped`

没有。全局搜索未发现 `rgbSwapped`。`FrameViewHelper::setImage()` 只做 `QPixmap::fromImage(m_lastImage)`，不换色。

## 4. 本轮新增一次性诊断

### 4.1 CameraFrameProvider

首个有效 `currentFrame` 会保存：

```text
/tmp/v2_color_debug/raw_mat_imwrite.png
/tmp/v2_color_debug/raw_after_bgr2rgb_then_imwrite.png
/tmp/v2_color_debug/display_qimage_roundtrip.png
```

同时打印：

```text
[ColorDebug] source=CameraFrameProvider mat=640x480 type=16 meanB=... meanG=... meanR=...
[ColorDebug] saved=/tmp/v2_color_debug raw=ok swapped=ok display=ok
```

判读标准：

- `raw_mat_imwrite.png` 正常、UI 发蓝：显示转换或显示入口仍有问题。
- `raw_mat_imwrite.png` 也发蓝：相机源、白平衡或采集格式问题，不应继续改 `MatImageConverter`。
- `raw_mat_imwrite.png` 正常且 `raw_after_bgr2rgb_then_imwrite.png` 发蓝：raw 是 BGR，显示层应做 BGR2RGB。
- `raw_after_bgr2rgb_then_imwrite.png` 正常且 `raw_mat_imwrite.png` 发蓝：raw 实际是 RGB，不能再无条件 BGR2RGB。

### 4.2 ReferenceImageProvider

首个有效 `referenceFrame` 会保存：

```text
/tmp/v2_color_debug/reference_raw_imwrite.png
/tmp/v2_color_debug/reference_display_qimage.png
```

同时打印：

```text
[ColorDebug] source=ReferenceImageProvider mat=... type=... meanB=... meanG=... meanR=...
[ColorDebug] saved=/tmp/v2_color_debug referenceRaw=ok referenceDisplay=ok
```

## 5. GStreamer pipeline 检查

`CameraFrameProvider.cpp` 中当前实际候选 pipeline：

```text
v4l2src device=/dev/video0 io-mode=mmap ! image/jpeg, width=1920, height=1080, framerate=60/1 ! mppjpegdec ! video/x-raw, format=NV12 ! appsink max-buffers=1 drop=true sync=false
```

该路径打开成功时 tag 为 `1920x1080 MJPG->NV12`，随后代码用 `COLOR_YUV2BGR_NV12` 转 BGR。

```text
v4l2src device=/dev/video0 io-mode=mmap ! image/jpeg, width=640, height=480, framerate=30/1 ! jpegdec ! videoconvert ! video/x-raw, format=BGR ! appsink max-buffers=1 drop=true sync=false
```

现场日志对应第二条，appsink 明确 `video/x-raw, format=BGR`。因此按 GStreamer caps 和 OpenCV 常规约定，`cv::Mat` 应是 BGR。

## 6. 修复策略判断

当前静态证据不支持：

- 情况 A：未发现 provider 生产路径提前 BGR2RGB。
- 情况 B：现场命中的 640 pipeline appsink 不是 RGB，而是 `format=BGR`。
- 情况 D：主显示入口和 Dialog 显示入口未发现 `MatImageConverter` 后再 `rgbSwapped`；`FrameViewHelper` 不做换色。

当前更需要用新增诊断图确认：

- 如果 `raw_mat_imwrite.png` 发蓝，按情况 C 处理：相机源、白平衡、GStreamer decode 或 v4l2 控件问题，不继续改 `MatImageConverter`。
- 如果 `raw_mat_imwrite.png` 正常但 `display_qimage_roundtrip.png` 或 UI 发蓝，再回到显示链路查入口。

下一步相机侧排查命令建议：

```bash
v4l2-ctl -d /dev/video0 --list-formats-ext
v4l2-ctl -d /dev/video0 --list-ctrls
v4l2-ctl -d /dev/video0 --get-ctrl=white_balance_automatic
v4l2-ctl -d /dev/video0 --get-ctrl=white_balance_temperature
v4l2-ctl -d /dev/video0 --set-ctrl=white_balance_automatic=1
```

如需直接用 GStreamer 生成一张 BGR probe 图：

```bash
gst-launch-1.0 v4l2src device=/dev/video0 io-mode=mmap num-buffers=1 ! image/jpeg,width=640,height=480,framerate=30/1 ! jpegdec ! videoconvert ! video/x-raw,format=BGR ! pngenc ! filesink location=/tmp/v2_color_debug/gst_bgr_probe.png
```

## 7. 日志控制

`MatImageConverter::logDisplayConversion()` 当前逻辑：

- 按 `source` 计数；
- 每个 `source` 只打印前 5 次；
- 之后直接 return。

因此当前源码已满足“每个 source 前 5 次打印”的限流要求。本轮未改 `MatImageConverter`，避免在 raw 颜色未确认前继续改显示转换。

## 8. 编译结果

按要求执行：

```bash
cd "/home/hjl-ubuntu/桌面/qt_znxj_v2_work_1920_515"
qmake
make -j$(nproc)
```

结果：编译通过，生成/链接 `qt_ui_test` 成功。

## 9. 本轮结论

当前代码约定应为：

- provider 层保存 OpenCV BGR Mat；
- display 层由 `MatImageConverter` 做唯一 BGR2RGB；
- provider 层和 display 层没有生产路径双重 BGR2RGB；
- 没有发现 `MatImageConverter` 后又 `rgbSwapped`。

所以“GStreamer 输出真 BGR 但 UI 仍发蓝”的下一步证据必须来自 `/tmp/v2_color_debug` 三张相机诊断图。若 `raw_mat_imwrite.png` 也蓝，问题应转向相机源/白平衡/采集格式；若 raw 正常但 roundtrip/UI 蓝，再继续查显示入口或 Qt 显示格式。
