# V2 AiDetection 与旧版 qt-AI 目标检测迁移只读分析

命令：
HALCONROOT=/home/kunling/LIB/halcon24.11
HALCON_LICENSE_FILE=/home/kunling/LIB/halcon24.11/license/license_support_halcon24.11_steady_2026_06.dat
LD_LIBRARY_PATH=/home/kunling/Qt/5.15.2/gcc_64/lib:/usr/local/lib:/home/kunling/src/opencv-4.8.0/build/lib:/home/kunling/LIB/halcon24.11/lib/x64-linux


分析日期：2026-06-05

范围约束：

- 本轮仅做只读分析；未修改 V2 源码，未修改旧版 qt-AI 源码。
- 本轮未编译、未运行 V2、未运行旧版 qt-AI、未打开相机、未新建相机线程、未接入任何真实推理。
- 本报告是本轮唯一新增文件。

## 1. V2 当前 AiDetection 状态

### 1.1 git status 与文件存在性

在 `/home/hjl-ubuntu/桌面/qt_znxj_v2_work_1920_515` 执行 `git status --short` 后，工作树已有大量修改与未跟踪文件；其中 `src/ObjectDetectionDialog.cpp`、`src/ObjectDetectionDialog.h`、`ui/ObjectDetectionDialog.ui` 为未跟踪文件，符合“阶段 2.5 后仍可能未跟踪”的背景。

重点文件存在性：

| 文件 | 是否存在 | 当前判断 |
| --- | --- | --- |
| `ui/ObjectDetectionDialog.ui` | 存在 | 未跟踪；目标检测 UI 已迁入 |
| `src/ObjectDetectionDialog.h` | 存在 | 未跟踪；定义 `ObjectDetectionDialog` 和配置结构 |
| `src/ObjectDetectionDialog.cpp` | 存在 | 未跟踪；已实现保存、回显、ROI 预览 |
| `docs/analysis/object_detection_dialog_ui_migration_stage1_report.md` | 存在 | 阶段 1 报告存在 |
| `docs/analysis/object_detection_dialog_ui_migration_stage2_report.md` | 存在 | 阶段 2 报告存在 |
| `docs/analysis/object_detection_ui_stage2_5_git_safety_report.md` | 存在 | 阶段 2.5 报告存在 |
| `docs/patches/object_detection_ui_stage1_stage2_before_algorithm.patch` | 存在 | 算法接入前 patch 备份存在 |

### 1.2 V2 当前 AiDetection 链路表

| 链路环节 | 是否存在 | 类/枚举/函数 | 文件路径 | 当前状态 | 是否需要新增 | 是否能复用现有通用结构 |
| --- | --- | --- | --- | --- | --- | --- |
| ToolCategory | 存在 | `ToolCategory::DeepLearning` | `src/toolcore/ToolTypes.h` | 已有枚举和 string/fromString 映射 | 否 | 是 |
| ToolType | 存在 | `ToolType::AiDetection` | `src/toolcore/ToolTypes.h` | 已有枚举和 string/fromString 映射 | 否 | 是 |
| ToolLibrary | 存在 | `ToolLibraryDialog::ObjectDetection` -> `ToolType::AiDetection` | `src/ToolLibraryDialog.cpp` | 工具库“深度学习/目标检测”可选，确认后返回 AiDetection | 否 | 是 |
| Dialog | 存在 | `ObjectDetectionDialog` | `src/ObjectDetectionDialog.h/.cpp`, `ui/ObjectDetectionDialog.ui` | 已迁入；保存、回显、基础页/全部页同步、ROI 接入 FrameViewHelper；测试按钮仅提示未接入 | 否 | 是，继续仅做配置 UI |
| ToolConfig | 存在 | `ToolConfig` | `src/toolcore/ToolConfig.h` | 支持 `toolType/category/params/judgeRule/roiNormalized` JSON 保存回显 | 否 | 是 |
| AiDetection params | 存在于 Dialog 输出 | `ObjectDetectionDialog::toToolConfig()` | `src/ObjectDetectionDialog.cpp` | 已写入 `modelName/maxDetections/confidenceThreshold/nmsIouThreshold/sortMode/width/height/angle/classFilter/showBoxes/showLabels/showScores` 等 | 后续可扩展模型路径、标签路径、输入尺寸 | 是 |
| AiDetection judgeRule | 存在于 Dialog 输出 | `ObjectDetectionDialog::toToolConfig()` | `src/ObjectDetectionDialog.cpp` | 已写入 `mode/resultBasis/minCount/maxCount/minScore/category` | 后续 Adapter/Runner 读取 | 是 |
| ToolEngine | 存在 | `ToolEngine::runTool/runTools/registerAdapter` | `src/toolcore/ToolEngine.h/.cpp` | 根据 `ToolType` 查找已注册 Adapter；无 Adapter 时返回 unsupported | 否 | 是，必须复用 |
| AiDetectionAdapter | 不存在 | 无 | 预期 `src/tooladapters/AiDetectionAdapter.*` | 当前 `MainWindow` 未 include、未成员持有、未 register | 是 | 应按 OCR/Presence Adapter 模式新增 |
| AiDetectionRunner | 不存在 | 无 | 预期 `src/algorithms/ai/AiDetectionRunner.*` | 当前没有真实目标检测 Runner | 是 | 应按 `src/algorithms/ocr`、`src/algorithms/presence` 风格新增 |
| Preprocess/Postprocess | 不存在 | 无 | 预期 `src/algorithms/ai/AiDetectionPreprocess.*`、`AiDetectionPostprocess.*` | 当前没有目标检测专用预处理/后处理 | 是 | 可复用 `cv::Mat`、ROI 像素转换、`ToolOverlay` |
| ToolRequest | 存在 | `ToolRequest` | `src/toolcore/ToolRequest.h` | 已承载 `cv::Mat image/referenceImage`、运行上下文 | 否 | 是，AiDetection 应接收 V2 当前帧 |
| ToolResult | 存在 | `ToolResult` | `src/toolcore/ToolResult.h` | 支持 success/ok/status/message/score/count/text/overlays/payload | 否 | 是 |
| Overlay | 存在 | `ToolOverlay`、`FrameViewHelper::setToolOverlays` | `src/toolcore/ToolOverlay.h`, `src/frame/FrameViewHelper.cpp` | 支持 Rect/Line/Circle/Polygon/Text；坐标为图像像素坐标 | 否 | 是，检测框和标签应输出为 ToolOverlay |
| 主运行链路 | 存在 | `MainWindow::submitToolChainRun` -> `ToolEngine::runTools` | `src/MainWindow.cpp` | 已在 `QtConcurrent::run` 中批量执行当前方案工具并显示 overlays | 否 | 是，禁止绕过 |
| 主窗口 Adapter 注册 | 部分存在 | `m_toolEngine.registerAdapter(...)` | `src/MainWindow.cpp`, `src/MainWindow.h` | 已注册 OCR、图案/斑点/圆/轮廓/边缘/直线有无；没有 AiDetection | 是 | 复用注册方式 |
| 测试按钮 | 存在但未接算法 | `ObjectDetectionDialog::connectControls()` | `src/ObjectDetectionDialog.cpp` | `testRunButton` 仅弹出“目标检测算法尚未接入” | 后续阶段 10 修改为走 ToolEngine | 是 |

### 1.3 V2 现有 Adapter/Runner 模式

现有结构遵循：

- `src/tooladapters/*Adapter.h/.cpp`：实现 `ToolAdapter::supports()` 和 `run()`，负责从 `ToolConfig.params/judgeRule/roiNormalized` 转换到领域 Runner 配置，再将 Runner 结果映射为 `ToolResult`。
- `src/algorithms/ocr/OcrHalconRunner.*`：OCR 领域 Runner。
- `src/algorithms/presence/*PresenceHalconRunner.*`：Presence 系列 Runner。
- `src/toolcore/ToolResult.h` 和 `src/toolcore/ToolOverlay.h` 是统一输出层。

AiDetection 最适合沿用这一模式：

`ToolConfig` -> `AiDetectionAdapter::run()` -> `AiDetectionRunner::run()` -> `AiDetectionResult` -> `ToolResult`。

目标检测不应写入 `ObjectDetectionDialog`，也不应从 Dialog 直接启动推理。

## 2. 旧版 qt-AI 相关文件清单

旧版路径：`/media/hjl-ubuntu/KIOXIA/qt-AI`

### 2.1 当前可见业务文件

| 文件 | 作用判断 |
| --- | --- |
| `README.md` | 描述独立 Qt Widgets 工程、双相机预览、YOLOv8 RKNN 推理、脚本和结果目录 |
| `mainwindow.h/.cpp` | 主界面、相机选择、模型/标签/阈值 UI、抓拍、调用 `YoloRunner`、显示 `ResultDialog` |
| `yolorunner.h/.cpp` | Qt 层外部推理封装；通过 `QProcess` 调用 RKNN 启动脚本并解析 stdout |
| `appconfig.h` | 配置结构：相机参数、YOLO 模型路径、标签路径、输出目录、脚本路径等 |
| `camerastream.cpp` | 自有相机采集线程实现，使用 OpenCV `VideoCapture`、GStreamer/V4L2、中心 640 裁剪 |
| `resultdialog.h/.cpp` | 显示原图、外部 demo 输出的标注图和原始检测文本 |
| `runtime/rknn_build/source/CMakeLists.txt` | RKNN YOLOv8 demo 的 CMake 配置残留 |
| `runtime/rknn_build/source/README.md` | YOLOv8 RK3588 demo 说明，提到 `src/main.cpp/postprocess.cpp/yolov8.cpp` |
| `runtime/rknn_build/source/build_qt_ai/rknn_yolov8_demo` | 已构建的 aarch64 RKNN demo 二进制 |
| `runtime/captures/*` | 历史抓拍输入图 |
| `runtime/results/*_out.png` | 历史标注结果图 |

### 2.2 当前缺失但 Makefile/README 引用的文件

这些文件在当前目录没有找到，但被 `README.md` 或 `Makefile` 引用：

- `qt-AI.pro`
- `main.cpp`
- `appconfig.cpp`
- `camerastream.h`
- `moc_camerastream.cpp`
- `app_config.json`
- `scripts/run_rknn_demo.sh`
- `runtime/rknn_build/source/src/main.cc`
- `runtime/rknn_build/source/src/postprocess.cc`
- `runtime/rknn_build/source/src/yolov8.cc`
- `runtime/rknn_build/source/include/*.h`
- `runtime/rknn_build/source/model/*.rknn`
- `runtime/rknn_build/source/model/*.txt`
- `runtime/rknn_build/source/rknn_lib` 或 `3rdparty/rknpu2/Linux/aarch64/librknnrt.so`

结论：当前旧版 qt-AI 副本不是完整源码快照，不能直接作为完整算法实现迁入 V2。

### 2.3 模型/标签/脚本扫描结果

- 当前旧版目录中未发现 `.rknn`、`.onnx`、`.pt`、`.weights` 模型文件。
- 当前旧版目录中未发现标签 `.txt` 文件，除了 CMake 文本。
- 当前旧版目录中未发现 `scripts/run_rknn_demo.sh`。
- `runtime/rknn_build/source/build_qt_ai/rknn_yolov8_demo` 是 aarch64 ELF，动态依赖 `librknnrt.so`、OpenCV 4.5、pthread 等。

## 3. 旧版 qt-AI 模型加载分析

旧版 Qt 层没有直接加载模型，模型路径由 UI 和配置提供：

- `mainwindow.cpp` 提供“选择模型”和“选择标签”按钮。
- `appconfig.h::YoloConfig` 包含 `modelPath`、`labelPath`、`classCount`、`boxThreshold`、`sourceRoot`、`outputDir`、`buildDir`、`scriptPath`。
- `YoloRunner::run()` 解析这些路径并调用外部脚本，参数顺序为：
  `sourceRoot modelPath labelPath classCount boxThreshold inputImagePath outputDir buildDir`。

重要限制：

- 当前缺少 `scripts/run_rknn_demo.sh`，无法确认脚本如何改写 demo、如何传参、是否每次重新构建。
- 当前缺少 `appconfig.cpp/app_config.json`，无法确认默认路径。
- 二进制字符串中存在硬编码路径 `/home/cat/model/yolov8_red_black.rknn`、`/home/cat/qt-AI/runtime/rknn_build/input_image`、`/home/cat/qt-AI/runtime/results`，说明 demo 源码曾被硬编码或脚本改写过。
- Qt 层每次检测启动外部进程，等价于每次检测都重新进入 demo 程序；Qt 层没有模型缓存。
- 二进制符号包含 `init_yolov8_model`、`release_yolov8_model`，说明模型生命周期在外部 demo 内部，不在 Qt 工程内。

迁入判断：

- `modelPath/labelPath/classCount/boxThreshold` 这些配置概念可迁入 V2。
- 旧版 Qt 层的 QProcess 模型加载方式不建议直接迁入 V2 主链路。
- 如果未来选择 RKNN 后端，应在 `AiDetectionRunner` 内建立明确的模型生命周期和缓存，而不是每次检测启动脚本。

## 4. 旧版 qt-AI 图像输入与预处理分析

### 4.1 图像输入

旧版输入链路：

`CameraStreamThread` -> `cv::Mat snapshot()` -> `MainWindow::saveSnapshot()` 写 PNG -> `YoloRunner` 把图片路径传给外部脚本/demo。

细节：

- 相机使用 OpenCV `cv::VideoCapture`。
- 优先 GStreamer + `mppjpegdec`，失败后回退 V4L2 MJPG，再回退 640 模式。
- `CameraStreamThread::snapshot()` 对 1920x1080 近似尺寸执行中心 640x640 裁剪。
- `cv::imwrite()` 保存抓拍图，再由外部 RKNN demo 读取文件。

迁入判断：

- V2 已有 `CameraFrameProvider` 和主运行链路 `ToolRequest.image`，旧版相机链路必须丢弃。
- V2 不应保存临时图再推理，应让 `AiDetectionRunner::run(const cv::Mat &, const AiDetectionConfig &)` 直接接收当前帧。

### 4.2 预处理

旧版 Qt 层能确认的预处理：

- 只有相机侧中心 640 裁剪和预览缩放。
- 没有 ROI 裁剪参数。
- 没有在 Qt 层做 letterbox/normalize/HWC/CHW。

外部 RKNN demo 残留符号显示：

- `convert_image_with_letterbox`
- `model input height=%d, width=%d, channel=%d`
- `model is NCHW input fmt`
- `model is NHWC input fmt`
- `scale=%f ... padding_w=%d padding_h=%d`
- `src_box=(%d %d %d %d)` / `dst_box=(%d %d %d %d)`

由于 `src/main.cc/postprocess.cc/yolov8.cc` 不存在，不能确认：

- BGR/RGB 细节。
- 是否归一化或量化输入。
- 具体输入尺寸。
- letterbox 边界映射公式。
- 是否使用 RGA 做 resize/颜色转换。

迁入判断：

- V2 应新增 `AiDetectionPreprocess`，明确处理 ROI 裁剪、letterbox、scale/pad 元数据、BGR/RGB、NCHW/NHWC。
- 如果找回旧版 RKNN demo 源码，可参考 `convert_image_with_letterbox` 和坐标还原逻辑；当前副本不能直接迁移实现。

## 5. 旧版 qt-AI 推理分析

旧版 Qt 层推理方式：

- `YoloRunner` 使用 `QProcess` 异步启动脚本。
- `YoloRunner` 通过 `finished` 信号读取 stdout/stderr。
- `YoloConfig::timeoutMs` 存在，但当前 `YoloRunner.cpp` 没有看到超时 kill 逻辑。

外部 demo 推理后端：

- 二进制动态依赖 `librknnrt.so`。
- 字符串/符号包含 `rknn_init`、`rknn_inputs_set`、`rknn_run`、`rknn_outputs_get`、`rknn_outputs_release`、`rknn_destroy`。
- `file` 显示 `qt-AI` 和 `rknn_yolov8_demo` 都是 ARM aarch64 ELF。
- CMake 输出显示构建系统为 Linux aarch64，OpenCV_DIR 为 `/usr/lib/aarch64-linux-gnu/cmake/opencv4`。
- 链接信息引用 aarch64 OpenCV、RGA、jpeg_turbo、`3rdparty/rknpu2/Linux/aarch64/librknnrt.so` 和 `rknn_lib`。

风险：

- 当前目录没有 `librknnrt.so`，也没有 `rknpu2` 目录。
- 当前 V2 工作机是否具备 RKNN/NPU 运行环境不能从 V2 代码确认。
- 旧版不是 ONNX、OpenCV DNN 或 HALCON DL；其真实后端是 RKNN。

迁入判断：

- 不能把 `YoloRunner` 作为 V2 `AiDetectionRunner` 原样使用。
- 后续如果迁 RKNN，应做成 `AiDetectionRunner` 内的可选 RKNN 后端，且必须在部署目标上验证。
- 当前建议先进入 Adapter/Runner 空壳阶段，不接真实后端。

## 6. 旧版 qt-AI 后处理分析

旧版 Qt 层只解析文本，不做后处理：

- `YoloRunner` 解析 stdout 中的 `RESULT_IMAGE=`、`DETECTION_LINE=`、`DETECTION_COUNT=`。
- `YoloResult::detections` 是 `QStringList`，不是结构化 bbox。

外部 demo 残留符号显示：

- `post_process`
- `post_process_1pass`
- `post_process_prepass`
- `post_process_2pass`
- 静态 `nms`
- `object_detect_result_list`
- `coco_cls_to_name`
- `%s %.1f%%`
- `draw_rectangle`

不能确认的内容：

- confidence threshold 如何进入后处理；Qt 只传 `boxThreshold` 给脚本。
- NMS IOU 阈值是多少、是否可配置。
- max detections 是否支持。
- class filter 是否支持。
- bbox 如何从 letterbox 输入映射回原图。
- ROI 内坐标到全图坐标如何处理；旧版没有 V2 ROI。
- 小框/大框过滤是否存在。

迁入判断：

- 当前可参考“外部 demo 内有 NMS/后处理”的事实，但没有可直接迁移的 C++ 源码。
- V2 应新增 `AiDetectionPostprocess`，按 V2 UI 参数实现 confidence、NMS、maxDetections、classFilter、width/height/angle/boundary 过滤和坐标还原。

## 7. 旧版 qt-AI 结果结构分析

旧版 Qt 层结果结构：

```cpp
struct YoloResult {
    bool success = false;
    QString inputImagePath;
    QString outputImagePath;
    QString summaryText;
    QStringList detections;
    QString stdoutText;
    QString stderrText;
    QString errorText;
    QString commandLine;
};
```

现状：

- 没有 `classId` 字段。
- 没有 `className` 结构化字段。
- 没有 `score` 数值字段。
- 没有 `bbox` 结构化字段。
- 没有 `angle/width/height` 结构化字段。
- 没有 accepted/filtered/filterReason。
- 检测数量来自 `DETECTION_COUNT` 或 `detections.size()`。
- 结果图路径来自 `RESULT_IMAGE` 或预期输出路径。

迁入 V2 时建议的结果结构：

- `AiDetectionBox`：`classId`、`className`、`score`、`QRectF bboxPixels`、`QRectF bboxNormalized`、`angle`、`width`、`height`、`accepted`、`filterReason`。
- `AiDetectionResult`：`success`、`ok`、`status`、`message`、`count`、`bestScore`、`elapsedMs`、`QVector<AiDetectionBox>`、`QVector<ToolOverlay>`、`QJsonObject payload`。
- `ToolResult.payload["detections"]` 使用 JSON 数组保存结构化检测框。

## 8. 旧版 qt-AI OK/NG 判定分析

旧版没有完整 OK/NG 判定链路：

- `YoloResult::success` 表示外部进程是否正常退出且输出图存在。
- `summaryText` 显示“检测完成，目标数: N”或错误信息。
- 未看到数量上下限判定。
- 未看到类别判定。
- 未看到最低分判定。
- 未看到“存在即 OK / 缺失即 NG”可配置规则。
- UI 没有 V2 目标检测 Dialog 里已有的 `resultBasis/minCount/maxCount/minScore/category` 对应逻辑。

迁入判断：

- OK/NG 判定应由 V2 `AiDetectionAdapter` 或 `AiDetectionRunner` 读取 `ToolConfig.judgeRule` 后实现。
- 旧版只可参考“检测数量用于摘要”，不能直接迁移判定规则。

## 9. 旧版 qt-AI Overlay 分析

旧版 Overlay 方式：

- 外部 RKNN demo 生成标注后的输出图片。
- `ResultDialog` 用 `QPixmap(result.outputImagePath)` 显示结果图。
- Qt 层没有 `QPainter` 绘制检测框。
- Qt 层没有 `ToolOverlay`、`QGraphicsScene` 检测框对象。
- stdout 中的 `DETECTION_LINE` 只作为文本展示。

迁入 V2 判断：

- 不应把“结果图文件”作为 V2 Overlay 主实现。
- `AiDetectionRunner` 应输出检测框像素坐标。
- `AiDetectionAdapter` 应生成 `ToolOverlayType::Rect` 和 `ToolOverlayType::Text`，交给 `FrameViewHelper::setToolOverlays()`。
- 若需要保存标注图，应作为调试/导出功能，不能替代 `ToolOverlay`。

## 10. UI 耦合风险

旧版 UI 与算法流程耦合较高：

- `MainWindow` 同时负责相机枚举、相机线程生命周期、模型路径 UI、参数保存、抓拍、启动推理、结果展示。
- `YoloRunner` 虽然独立，但仍是 Qt `QObject/QProcess` 和文件路径协议，不是纯算法 Runner。
- `ResultDialog` 绑定旧版的结果图/原图文件展示方式。
- `applyYoloSettings()` 直接保存到 `app_config.json`，不适合 V2 `SchemeStore/ToolConfig`。

迁入建议：

- 丢弃旧版 `MainWindow` UI 逻辑。
- 丢弃旧版 `ResultDialog`。
- 不在 `ObjectDetectionDialog` 中跑推理。
- 仅参考模型路径、标签路径、类别数、阈值这些参数概念，并映射到 V2 `ToolConfig.params`。

## 11. 相机耦合风险

旧版相机逻辑与 V2 冲突：

- 直接枚举 `/dev/video*`。
- 直接创建 `CameraStreamThread`。
- 直接打开 `cv::VideoCapture`。
- 使用 GStreamer/V4L2/MJPG/YUYV 回退。
- 抓拍时从相机线程取 `snapshot()`。
- 对 1920 图像做中心 640 裁剪。

V2 已有 `CameraFrameProvider`、`ReferenceImageProvider`、主运行链路当前帧输入。迁入时必须：

- 不复用旧版相机线程。
- 不在目标检测算法中打开相机。
- 不保存临时抓拍图作为算法输入主路径。
- 使用 `ToolRequest.image` 中的 V2 当前帧。
- 使用 `ToolConfig.roiNormalized` 作为检测 ROI。

## 12. 依赖库风险

依赖风险：

- 旧版 `qt-AI` 和 `rknn_yolov8_demo` 都是 aarch64 ELF，不能在 x86_64 主机直接运行。
- 外部 demo 需要 `librknnrt.so`，当前目录没有找到该 so。
- 链接信息指向 `3rdparty/rknpu2/Linux/aarch64/librknnrt.so` 和 `runtime/rknn_build/source/rknn_lib`，当前目录均缺失。
- 外部 demo 依赖 aarch64 OpenCV 4.5、RGA、jpeg_turbo、pthread。
- 当前 demo 源码缺失，无法审查 ABI、输入输出 tensor、内存释放和线程安全。
- 旧版脚本缺失，无法确认是否每次复制/改写/编译外部工程。
- 二进制硬编码 `/home/cat/...` 路径，不适合 V2。

迁入前置条件：

- 找回完整 `YOLOv8_RK3588_object_detect-main` 源码或等价后端源码。
- 明确目标部署平台是 RK3588/aarch64 还是 x86。
- 明确 V2 是否允许引入 RKNN Runtime。
- 建立可关闭/可降级的后端策略，避免破坏现有 OCR/Presence 工具。

## 13. 可直接复用内容

当前旧版可直接复用的内容很少：

- 参数概念：模型路径、标签路径、类别数、置信度阈值。
- 结果摘要概念：检测数量、错误信息、stdout/stderr 调试信息。
- `YoloResult` 的字段命名可作为 V2 payload 设计参考，但不建议直接复制类型。

不建议直接复用 `YoloRunner`，因为它是 QProcess + 脚本 + 文件协议，不符合 V2 `ToolEngine -> Adapter -> Runner` 的统一链路。

## 14. 需改造后复用内容

可改造后复用：

- `YoloRunner` 中的路径校验思路：模型路径、标签路径、阈值范围、类别数范围。
- stdout 协议解析思路：若未来保留外部进程后端，可把 `RESULT_IMAGE/DETECTION_LINE/DETECTION_COUNT` 解析器拆成纯函数，但这只能作为临时兼容后端。
- 外部 demo 的 RKNN 推理/letterbox/NMS/postprocess：只有在找回源码后才能迁入，且应拆到 `AiDetectionRunner`、`AiDetectionPreprocess`、`AiDetectionPostprocess`。
- `CameraStreamThread::toImage()` 的 BGR->RGB 转换思路不需要迁入算法，但可作为图像显示参考；V2 已有 `MatImageConverter`。

## 15. 只能参考或必须丢弃内容

只能参考：

- 旧版 UI 参数排布。
- 旧版 README 中的部署说明。
- 已构建二进制暴露出的 RKNN/letterbox/NMS 符号。
- 历史结果图，可用于人工观察旧版输出效果。

必须丢弃：

- 旧版 `MainWindow` 直接控制推理的方式。
- 旧版自有相机线程和 `/dev/video*` 枚举。
- 每次抓拍保存 PNG 再调用外部脚本的主链路。
- `ResultDialog` 显示标注图替代 overlay 的方式。
- qmake/Makefile/对象文件/已构建 aarch64 二进制等构建产物。
- 硬编码 `/home/cat/...` 路径。

## 16. 推荐 V2 新增类和路径

V2 当前目录风格是：

- Adapter：`src/tooladapters/*Adapter.*`
- 算法 Runner：`src/algorithms/<domain>/*Runner.*`

因此建议使用下列路径，而不是把 Adapter 放到 `src/algorithms/ai`：

| 文件 | 作用 |
| --- | --- |
| `src/tooladapters/AiDetectionAdapter.h` | 声明 `AiDetectionAdapter : public ToolAdapter` |
| `src/tooladapters/AiDetectionAdapter.cpp` | 读取 `ToolConfig`，调用 Runner，映射 `ToolResult` |
| `src/algorithms/ai/AiDetectionTypes.h` | 定义 `AiDetectionConfig`、`AiDetectionBox`、`AiDetectionResult`、枚举和 JSON 辅助结构 |
| `src/algorithms/ai/AiDetectionRunner.h` | 声明目标检测 Runner 统一接口 |
| `src/algorithms/ai/AiDetectionRunner.cpp` | 阶段 4 先返回 unsupported/empty，后续接模型加载和推理 |
| `src/algorithms/ai/AiDetectionPreprocess.h` | 声明 ROI、letterbox、颜色/布局转换的数据结构和函数 |
| `src/algorithms/ai/AiDetectionPreprocess.cpp` | 实现 ROI 像素裁剪、输入尺寸适配、scale/pad 元数据 |
| `src/algorithms/ai/AiDetectionPostprocess.h` | 声明检测输出解析、NMS、过滤、坐标还原 |
| `src/algorithms/ai/AiDetectionPostprocess.cpp` | 实现置信度、NMS、类别、尺寸、边界过滤和 ToolOverlay 前置结果 |

后续可选：

- `src/algorithms/ai/AiDetectionLabelMap.h/.cpp`：标签文件读取和 classId/className 映射。
- `src/algorithms/ai/AiDetectionModelCache.h/.cpp`：模型缓存和线程安全。
- `src/algorithms/ai/backends/AiDetectionRknnBackend.h/.cpp`：如果确认 RKNN 是目标后端，再单独隔离。

## 17. 推荐迁移阶段规划

### 阶段 4：新增 AiDetectionAdapter / AiDetectionRunner 空壳

- 修改目标：建立 V2 统一链路，`ToolType::AiDetection` 有 Adapter，但不做真实推理。
- 修改文件：`src/tooladapters/AiDetectionAdapter.*`、`src/algorithms/ai/AiDetectionTypes.h`、`src/algorithms/ai/AiDetectionRunner.*`、`src/MainWindow.h/.cpp` 注册 Adapter、`qt_ui_test.pro` 增加文件。
- 验证方法：编译；运行单次工具链时目标检测返回 `unsupported` 或明确的 `algorithm_not_connected`，OCR/Presence 不受影响。
- 风险点：返回 unsupported 会导致整条工具链 overall NG；状态文案需清晰。
- 回退方案：移除 Adapter 注册和新增文件，回到 ToolEngine 无 Adapter 的 unsupported 状态。

### 阶段 5：接模型路径、标签路径、输入尺寸参数，但不推理

- 修改目标：扩展 `ObjectDetectionDialog` 和 `ToolConfig.params`，保存 `modelPath`、`labelPath`、`inputWidth`、`inputHeight`、`backend` 等。
- 修改文件：`ObjectDetectionDialog.*/.ui`、`AiDetectionAdapter.cpp`、`AiDetectionTypes.h`。
- 验证方法：新增/编辑目标检测工具，确认保存、回显、方案 JSON、Runner payload 均包含参数；不执行真实推理。
- 风险点：现有 `modelName` 与 `modelPath` 命名冲突；需兼容旧配置。
- 回退方案：保留 `modelName`，忽略新字段；不影响 ToolEngine。

### 阶段 6：迁移模型加载和预处理

- 修改目标：实现标签读取、模型路径检查、ROI 裁剪和 letterbox 元数据，但仍不调用推理。
- 修改文件：`AiDetectionPreprocess.*`、`AiDetectionLabelMap.*`、`AiDetectionRunner.cpp`。
- 验证方法：用固定 `cv::Mat` 或当前帧走 Runner，返回 payload 中的 ROI 像素、input size、scale、pad、labelCount。
- 风险点：坐标归一化和像素裁剪边界；BGR/RGB 和 NCHW/NHWC 尚未与后端确认。
- 回退方案：关闭 preprocess 应用，只返回参数检查结果。

### 阶段 7：迁移推理和后处理

- 修改目标：接入真实后端推理和后处理，输出结构化检测结果。
- 修改文件：`AiDetectionRunner.*`、`AiDetectionPostprocess.*`、可选 `backends/AiDetectionRknnBackend.*`。
- 验证方法：在目标部署环境用固定图片验证 detection 数量、class、score、bbox；对照旧版结果图人工比对。
- 风险点：RKNN Runtime、NPU、RGA、OpenCV ABI、线程安全、输出 tensor 格式不确定。
- 回退方案：编译开关或运行参数禁用真实后端，回到 unsupported/empty 结果。

### 阶段 8：输出 ToolResult 和 ToolOverlay

- 修改目标：将 `AiDetectionResult` 映射为 `ToolResult.payload` 和 `ToolOverlay`。
- 修改文件：`AiDetectionAdapter.cpp`、`AiDetectionTypes.h`、必要时 `FrameViewHelper` 仅修 bug 不改协议。
- 验证方法：主窗口单次运行显示 Rect/Text overlay；payload 中 `detections` 数组完整。
- 风险点：ToolOverlay 使用图像像素坐标，不能误用归一化坐标。
- 回退方案：保留 payload，临时关闭 overlay 输出。

### 阶段 9：接 OK/NG 判定

- 修改目标：按 `judgeRule.mode` 实现数量判断、最低得分、类别判断。
- 修改文件：`AiDetectionAdapter.cpp` 或 `AiDetectionPostprocess.cpp`。
- 验证方法：分别设置 min/max count、minScore、category，验证 ToolResult ok/status/message。
- 风险点：className 与 label 文件不一致；过滤前后数量口径需明确。
- 回退方案：只使用 count 判定，其他模式返回配置错误。

### 阶段 10：测试按钮改为走 ToolEngine

- 修改目标：`ObjectDetectionDialog` 的测试按钮使用 V2 当前帧/基准图和测试 ToolEngine，不直接跑算法。
- 修改文件：`ObjectDetectionDialog.h/.cpp`，可能新增测试用 `ToolEngine` 和 `AiDetectionAdapter` 成员。
- 验证方法：点击测试按钮后显示 ToolResult 状态，并在 Dialog 预览区显示 overlays。
- 风险点：Dialog 内持有 Adapter/Runner 生命周期与主窗口 Runner 缓存重复。
- 回退方案：恢复提示“目标检测算法尚未接入”。

### 阶段 11：性能、线程、模型缓存、异常处理

- 修改目标：模型按路径缓存，避免每帧重复加载；明确并发策略和异常隔离。
- 修改文件：`AiDetectionModelCache.*`、`AiDetectionRunner.*`、后端实现。
- 验证方法：连续运行统计加载次数、单帧耗时、异常返回；确认 OCR/Presence 同时运行不退化。
- 风险点：RKNN context 是否线程安全；缓存释放时机；连续运行阻塞。
- 回退方案：禁用缓存或限制 AiDetection 单线程执行。

### 阶段 12：对标海康目标检测参数体验

- 修改目标：完善模型管理、类别过滤、屏蔽区域、排序、结果表、阈值体验和错误提示。
- 修改文件：`ObjectDetectionDialog.*/.ui`、`AiDetectionTypes.h`、`AiDetectionAdapter.cpp`。
- 验证方法：按海康参数项逐项对照保存、回显、运行结果、Overlay 和 OK/NG。
- 风险点：UI 参数过多导致配置字段不稳定；与现有阶段 1/2/2.5 UI 改动冲突。
- 回退方案：保留当前基础页/全部页字段，只隐藏高级项。

## 18. 是否建议进入 Adapter/Runner 空壳实现阶段

建议进入阶段 4，但只做空壳。

理由：

- V2 的 `ToolCategory::DeepLearning`、`ToolType::AiDetection`、Dialog、ToolConfig、ToolEngine、ToolResult、ToolOverlay 链路已具备。
- 当前缺口明确集中在 `AiDetectionAdapter`、`AiDetectionRunner` 和算法域结构。
- 旧版 qt-AI 当前副本源码不完整，真实 RKNN demo 源码、脚本、模型、标签、rknn runtime 均缺失或不可直接确认。
- 空壳 Adapter/Runner 可以先让 V2 链路闭环，同时不接 ONNX/YOLO/RKNN/OpenCV DNN/HALCON DL 真实推理，不破坏现有 OCR/Presence 工具。

不建议现在直接接真实算法。

真实推理前必须先找回或重新确认：

- 完整 RKNN demo 源码。
- 模型和标签文件格式。
- 输入尺寸、颜色顺序、layout、量化参数。
- 后处理输出 tensor 格式、NMS IOU、坐标还原逻辑。
- 目标部署平台和 `librknnrt.so`/RGA/OpenCV ABI。
