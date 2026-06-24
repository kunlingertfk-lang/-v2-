# 图像发蓝诊断收尾清理报告

日期：2026-05-26

## 结论

图像发蓝根因已确认是虚拟机 USB/UVC/MJPEG 采集链路问题，不是 Qt 显示转换问题，也不是 PatternPresence 算法问题。

证据：

- 两个相机在板子上颜色正常。
- 同一类图像在虚拟机内通过 v4l2/ffmpeg 直抓也发蓝。
- 因此发蓝发生在虚拟机采集链路，早于 Qt 显示层和工具算法层。

是否继续修改显示转换：否。

是否影响板子部署：不影响。板子实机采集颜色正常，当前保留 OpenCV BGR Mat 到 Qt RGB QImage 的标准显示转换。

## 保留的显示转换规则

`MatImageConverter::matToDisplayImage()` 保留以下规则：

| Mat 类型 | 显示转换 |
|---|---|
| `CV_8UC1` | `QImage::Format_Grayscale8` |
| `CV_8UC3` | `cv::COLOR_BGR2RGB` -> `QImage::Format_RGB888` |
| `CV_8UC4` | `cv::COLOR_BGRA2RGBA` -> `QImage::Format_RGBA8888` |

所有返回的 `QImage` 继续使用 `copy()`，避免引用临时 Mat buffer。

## 本轮清理内容

已删除 `CameraFrameProvider.cpp` 中自动保存临时诊断图的逻辑：

- `/tmp/v2_color_debug/raw_mat_imwrite.png`
- `/tmp/v2_color_debug/raw_after_bgr2rgb_then_imwrite.png`
- `/tmp/v2_color_debug/display_qimage_roundtrip.png`
- `[ColorDebug] source=CameraFrameProvider ...` 均值日志

已删除 `ReferenceImageProvider.cpp` 中自动保存临时诊断图的逻辑：

- `/tmp/v2_color_debug/reference_raw_imwrite.png`
- `/tmp/v2_color_debug/reference_display_qimage.png`
- `[ColorDebug] source=ReferenceImageProvider ...` 均值日志

`[DisplayConvert]` 日志已改为默认关闭，仅在环境变量打开时打印：

```bash
V2_DISPLAY_CONVERT_DEBUG=1 ./qt_ui_test
```

打开后每个 source 最多打印前 3 次。

## 未修改范围

本轮未修改：

- PatternPresence 算法
- autoModelDomain
- ROI
- ToolEngine
- MainWindow 业务逻辑
- 其他工具算法
- `.ui` 文件

## 修改文件清单

本轮涉及文件：

```text
src/frame/CameraFrameProvider.cpp
src/frame/ReferenceImageProvider.cpp
src/frame/MatImageConverter.cpp
docs/analysis/display_blue_tint_vm_capture_conclusion_cleanup_report.md
```

说明：当前工作区中 `src/frame/MatImageConverter.cpp/.h` 是未跟踪文件，但本轮只调整了 `MatImageConverter.cpp` 的日志开关和打印次数，没有改变显示转换规则。

## 验证

执行：

```bash
cd "/home/hjl-ubuntu/桌面/qt_znxj_v2_work_1920_515"
qmake
make -j$(nproc)
```

结果：编译通过，`qt_ui_test` 链接成功。

残留检查：

```bash
rg -n "v2_color_debug|ColorDebug|raw_mat_imwrite|reference_raw|display_qimage_roundtrip|raw_after_bgr2rgb_then_imwrite|V2_DISPLAY_CONVERT_DEBUG|DisplayConvert" src/frame src
```

结果只剩 `MatImageConverter.cpp` 内默认关闭的 `V2_DISPLAY_CONVERT_DEBUG` 开关和受控 `[DisplayConvert]` 日志，没有自动保存 `/tmp/v2_color_debug` 的逻辑。
