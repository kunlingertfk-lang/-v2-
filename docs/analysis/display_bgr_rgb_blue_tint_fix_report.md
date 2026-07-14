# V2 Display BGR/RGB Blue Tint Fix Report

## 1. git diff --name-only

执行命令：

```bash
git diff --name-only
```

输出：

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

说明：

- `FrameViewHelper.*`：在当前工作树 diff 中已有修改；本轮未改其 ROI/绘制逻辑。
- `CameraFrameProvider.*`：在当前工作树 diff 中已有修改；本轮只在 `CameraFrameProvider.cpp` 的显示转换入口传入日志 source。
- `ReferenceImageProvider.*`：`ReferenceImageProvider.cpp` 在 diff 中；本轮只在显示转换入口传入日志 source。
- `PatternPresenceDialog.*`：在 diff 中；本轮只改 `PatternPresenceDialog.cpp` 的本地 `imageFromFrame()` 显示转换调用标签。
- `MainWindow.*`：在 diff 中已有修改；本轮未修改 MainWindow 业务流程。
- `ToolsDialog.*`：在 diff 中；本轮只改 `ToolsDialog.cpp` 的基准图预览显示转换调用标签。
- `MatImageConverter.*`：当前是未跟踪文件，`git diff --name-only` 不会列出；本轮在该显示 helper 中增加统一转换日志。
- `.ui`：当前 diff 中已有多个 `.ui` 改动；本轮未修改任何 `.ui`。

PatternPresence 算法文件本身不应导致整图发蓝，需要继续查现有显示转换。本轮没有修改 PatternPresence 算法逻辑。

## 2. Mat -> QImage 转换点

全局搜索命令：

```bash
rg -n "QImage|QPixmap|rgbSwapped|cvtColor|COLOR_BGR2RGB|COLOR_RGB2BGR|Format_RGB888|Format_BGR888|Format_Grayscale8|Format_ARGB32|Mat.*QImage|matTo|cvMat|setPixmap|setImage|FrameViewHelper|ReferenceImageProvider|CameraFrameProvider" src ui
```

找到的显示链路：

- `src/frame/MatImageConverter.cpp`：统一 `cv::Mat -> QImage` 转换函数。
- `src/frame/CameraFrameProvider.cpp`：`matToImage()` 调用 `MatImageConverter::matToDisplayImage()`；相机实时帧通过 `frameUpdated(QImage)` 进入 UI。
- `src/frame/ReferenceImageProvider.cpp`：`matToImage()` 调用 `MatImageConverter::matToDisplayImage()`；基准图通过 `referenceImage()` 或 `referenceFrameChanged(QImage)` 进入 UI。
- `src/PatternPresenceDialog.cpp`：本地 `imageFromFrame()` 调 `MatImageConverter`；基准图优先走 `ReferenceImageProvider::referenceImage()`，单次/连续测试图走 `imageFromFrame()`。
- `src/ToolsDialog.cpp`：`currentReferenceImage()` 从 `ReferenceImageProvider::referenceFrame()` 取 BGR Mat，再走 `MatImageConverter` 显示在工具参数页预览。
- `src/frame/FrameViewHelper.cpp`：`setImage(QImage)` 只执行 `QPixmap::fromImage()` 和 `QGraphicsPixmapItem::setPixmap()`，不做通道转换。
- `src/ReferenceImageDialog.cpp`：PC 导入图片时 `QImage::Format_RGB888 -> cv::COLOR_RGB2BGR`，保存到基准图 provider 的约定是 BGR。
- `src/SchemeStore.cpp`：`cv::imwrite(referenceFrame)` 保存基准图，OpenCV 写文件按 BGR 约定处理。
- `src/algorithms/presence/PatternPresenceHalconRunner.cpp`：debug 图使用 `cv::imwrite()`，不经过 Qt 显示转换。

未发现 `rgbSwapped`。未发现残留的 `Format_BGR888`。当前只有 `MatImageConverter` 里使用 `Format_RGB888`，并且前面有显式 `COLOR_BGR2RGB`。

## 3. 发蓝根因

发蓝现象符合 BGR/RGB 红蓝通道互换。`CameraFrameProvider` 的代码路径声明输出为 BGR：

- 640x480 GStreamer fallback pipeline 要求 `video/x-raw, format=BGR`。
- NV12 路径使用 `cv::COLOR_YUV2BGR_NV12`。
- V4L2/OpenCV 常规彩色帧按 BGR 约定返回。
- 日志 `type=16` 对应 `CV_8UC3`，因此显示前必须按 BGR 三通道转换到 Qt RGB。

如果 debug PNG 或 `cv::imwrite` 保存图颜色正常，但 UI 发蓝，问题就在 `cv::Mat -> QImage -> QPixmap` 显示链路；不是 PatternPresence 算法本身。

## 4. 修改文件

本轮实际修改：

- `src/frame/MatImageConverter.h`
- `src/frame/MatImageConverter.cpp`
- `src/frame/CameraFrameProvider.cpp`
- `src/frame/ReferenceImageProvider.cpp`
- `src/PatternPresenceDialog.cpp`
- `src/ToolsDialog.cpp`
- `docs/analysis/display_bgr_rgb_blue_tint_fix_report.md`

本轮未修改：

- `PatternPresenceHalconRunner.*`
- `PatternPresenceAutoModelDomain.*`
- `PatternPresenceHalconApi.*`
- `ToolEngine.*`
- `MainWindow.*` 业务流程
- ROI 交互逻辑
- `.ui`
- 其他工具算法

## 5. 统一转换规则

统一入口：`MatImageConverter::matToDisplayImage()`。

规则：

- `CV_8UC1`：直接 `QImage::Format_Grayscale8`，并 `copy()`。
- `CV_8UC3`：按 OpenCV BGR 约定，先 `cv::COLOR_BGR2RGB`，再 `QImage::Format_RGB888`，并 `copy()`。
- `CV_8UC4`：按 OpenCV BGRA 约定，先 `cv::COLOR_BGRA2RGBA`，再 `QImage::Format_RGBA8888`，并 `copy()`。
- 不对已经 RGB 的 `QImage` 再 `rgbSwapped()`。
- 不使用 `Format_RGB888` 直接包 BGR Mat。
- `QImage` 全部 `copy()`，避免临时 Mat 生命周期结束后显示错乱。

新增限频日志，单 source 前 5 次：

```text
[DisplayConvert] source=PatternPresenceDialog mat=640x480 type=16 channels=3 route=BGR2RGB qformat=RGB888
```

## 6. 是否修改 Pattern 算法

没有。

## 7. 是否修改 ROI

没有。

## 8. 验收截图/日志说明

使用合成红/蓝/绿标记图验证，文件输出在：

- `docs/analysis/display_convert_check_bgr_imwrite.png`
- `docs/analysis/display_convert_check_qimage_bgr.png`
- `docs/analysis/display_convert_check_qimage_gray.png`
- `docs/analysis/display_convert_check_qimage_bgra.png`

验收日志：

```text
[DisplayConvert] source=DisplayConvertCheck_BGR mat=120x80 type=16 channels=3 route=BGR2RGB qformat=RGB888
[DisplayConvert] source=DisplayConvertCheck_GRAY mat=32x32 type=0 channels=1 route=gray qformat=Grayscale8
[DisplayConvert] source=DisplayConvertCheck_BGRA mat=80x40 type=24 channels=4 route=BGRA2RGBA qformat=RGBA8888
[DisplayConvertCheck] result=PASS bgrFormat=13 grayFormat=24 bgraFormat=17 outDir=/home/hjl-ubuntu/桌面/qt_znxj_v2_work_1920_515/docs/analysis
```

验收结论：

- OpenCV `imwrite` BGR 图与 QImage 显示转换图颜色一致。
- `CV_8UC3`：左红、右蓝、绿色标记均正确，红蓝未互换。
- `CV_8UC1`：灰度显示正常，QImage format 为 `Grayscale8`。
- `CV_8UC4`：BGRA 显示正常，红蓝未互换。
- `FrameViewHelper::setImage()` 后续只做 `QPixmap::fromImage()`，不参与通道交换。

现场相机 UI 未在本轮自动化中打开；主界面、工具参数页、PatternPresenceDialog 的显示入口已统一到同一转换规则，并由 source 日志定位。

## 9. qmake/make 编译结果

执行：

```bash
qmake
make -j$(nproc)
```

结果：通过，生成 `qt_ui_test`。
