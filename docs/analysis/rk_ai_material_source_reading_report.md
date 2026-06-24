# RK AI / YOLOv8 RKNN 源码细读报告

## 1. 拷贝材料清单

本轮新增材料目录为 `rk_ai_materials`，未修改 V2 源码、未修改 `ObjectDetectionDialog.ui`、未编译、未运行推理、未打开相机、未修改 RK 板文件。

实际执行时，RK 板 SSH/SCP 在读取模型和后续小文件时多次卡住；为避免继续占用 RK 板连接，本地材料最终使用已有 staging 副本补齐：

- `rk_ai_materials/model/yolov8_red_black.rknn`
- `rk_ai_materials/model/yolov8.rknn`
- `rk_ai_materials/labels/红黑线标签.txt`
- `rk_ai_materials/yolo_demo/src/main.cc`
- `rk_ai_materials/yolo_demo/src/yolov8.cc`
- `rk_ai_materials/yolo_demo/src/postprocess.cc`
- `rk_ai_materials/yolo_demo/include/yolov8.h`
- `rk_ai_materials/yolo_demo/include/postprocess.h`
- `rk_ai_materials/yolo_demo/include/rknn_api.h`
- `rk_ai_materials/yolo_demo/utils/image_utils.c`
- `rk_ai_materials/yolo_demo/utils/image_utils.h`
- `rk_ai_materials/yolo_demo/utils/image_drawing.c`
- `rk_ai_materials/yolo_demo/CMakeLists.txt`
- `rk_ai_materials/qt_ai/app_config.json`
- `rk_ai_materials/qt_ai/run_rknn_demo.sh`
- `rk_ai_materials/qt_ai/yolorunner.cpp`
- `rk_ai_materials/qt_ai/yolorunner.h`
- `rk_ai_materials/qt_ai/appconfig.cpp`
- `rk_ai_materials/qt_ai/appconfig.h`
- `rk_ai_materials/input_image/` 下 4 张本地可用测试图
- `rk_ai_materials/output_image/` 下 4 张本地可用输出图

`rk_ai_materials/qt_ai/apps/yolo_demo/src/postprocess.cc` 未找到，远端尝试和本地 `qt-AI` 副本中均无该文件。

## 2. 材料占用和 git 风险

`rk_ai_materials` 总占用 `20M`，远小于 1GB。子目录占用：

- `model`: `16M`
- `output_image`: `3.5M`
- `input_image`: `408K`
- `yolo_demo`: `176K`
- `qt_ai`: `36K`
- `labels`: `8.0K`

`git status --short` 显示 `rk_ai_materials/` 为未跟踪目录。本轮未执行 `git add`，也未修改 `.gitignore`。

风险提醒：`rk_ai_materials/model/*.rknn` 仅作本地分析材料，不建议进入 git。若后续要提交报告，应只提交报告或显式排除模型文件。

## 3. 模型与标签确认

模型文件：

- `rk_ai_materials/model/yolov8_red_black.rknn`: `4,064,465` bytes，sha256 `f3989fcf013e5de55e741d782936b61e88cea582d95f46cbc9680da41fbe3e01`
- `rk_ai_materials/model/yolov8.rknn`: `12,549,329` bytes，sha256 `8b7d18afb9953251891898e62bb06e04d26ed35593b95001a2bf04ffbf059456`

远端曾成功 `stat` 到 `/home/cat/model/yolov8_red_black.rknn` 大小为 `4,064,465` bytes，和本地材料一致。`yolov8.rknn` 是否仍在 RK 当前路径上未最终确认，因为 SSH 后续卡住。

标签文件 `rk_ai_materials/labels/红黑线标签.txt` 为 9 bytes，内容为：

```text
red
black
```

类别顺序为 `0=red`，`1=black`。结合模型名 `yolov8_red_black.rknn`、标签和 `classCount=2`，当前 red/black 模型适合作为红黑线目标检测模型的最小接入对象。

## 4. app_config.json 配置分析

`rk_ai_materials/qt_ai/app_config.json` 的 `yolo` 配置包含：

- `enabled: true`
- `sourceRoot: /home/cat/YOLOv8_RK3588_object_detect-main`
- `modelPath: /home/cat/model/yolov8_red_black.rknn`
- `labelPath: /home/cat/红黑线标签.txt`
- `outputDir: runtime/results`
- `buildDir: runtime/rknn_build`
- `scriptPath: scripts/run_rknn_demo.sh`
- `classCount: 2`
- `boxThreshold: 0.02`
- `timeoutMs: 120000`

`appconfig.cpp` 默认值也写死同一组 RK 路径和 `classCount=2`。这说明旧 Qt AI 默认就是 red/black 二分类模型，而不是通用 COCO 模型。

## 5. run_rknn_demo.sh 调用链分析

脚本参数顺序为：

```text
<source_root> <model_path> <label_path> <class_count> <box_threshold> <input_image> <output_dir> <build_dir>
```

脚本会校验 source/model/label/input、类别数、阈值，并要求运行主机为 `aarch64`。随后它会：

- `mkdir -p` 创建 build/runtime 目录。
- `cp -a "$SOURCE_ROOT/." "$PATCH_ROOT/"`，复制完整 demo 源码到 build 目录。
- 使用 `sed -i` patch 临时源码。
- 清空 `RUN_INPUT_DIR` 内文件并复制单张输入图。
- 删除旧输出图。
- 每次执行 `cmake` 和 `make -j$(nproc)`。
- 设置 `LD_LIBRARY_PATH` 后运行 `rknn_yolov8_demo`。
- 打印完整 log。
- 输出 `RESULT_IMAGE=<path>`。
- 将 log 中包含 ` @ (` 的行转为 `DETECTION_LINE=<raw line>`。
- 输出 `DETECTION_COUNT=<n>`。

结论：这个脚本可作为 V2 SSH 桥接的临时兼容层，但它每次复制源码、patch、cmake/make，不能作为最终生产结构。

## 6. YoloRunner / QProcess 调用分析

`yolorunner.cpp` 使用 `QProcess`：

- 构造时连接 `finished` 信号。
- `run()` 解析脚本、sourceRoot/modelPath/labelPath/outputDir/buildDir。
- 参数顺序和脚本一致。
- `setProcessChannelMode(QProcess::SeparateChannels)`。
- `start()` 后只 `waitForStarted(3000)`。
- 完成后一次性读取 stdout/stderr。

它没有连接 `readyReadStandardOutput`，也没有实际使用 `timeoutMs` 杀进程。输出解析规则：

- `RESULT_IMAGE=` 覆盖结果图路径。
- `DETECTION_LINE=` 收集 raw detection line。
- `DETECTION_COUNT=(\d+)` 优先作为目标数。

旧链路返回的结构包括 result image、检测行、检测数量，但不是结构化 JSON。

## 7. RKNN 生命周期分析

RKNN 初始化位于 `rk_ai_materials/yolo_demo/src/yolov8.cc`：

- `init_yolov8_model()` 读取模型文件，调用 `rknn_init(&ctx, model, model_len, 0, NULL)`。
- 随后 `rknn_query` 查询 `RKNN_QUERY_SDK_VERSION`、`RKNN_QUERY_IN_OUT_NUM`、每个 `RKNN_QUERY_INPUT_ATTR`、每个 `RKNN_QUERY_OUTPUT_ATTR`。
- 输入/输出 attr 被复制到 `app_ctx`，并解析模型输入高宽通道。
- `release_yolov8_model()` 释放 input/output attrs，并调用 `rknn_destroy`。

推理在 `inference_yolov8_model()` 中：

- `rknn_inputs_set(app_ctx->rknn_ctx, app_ctx->io_num.n_input, inputs)`
- `rknn_run(app_ctx->rknn_ctx, nullptr)`
- `rknn_outputs_get(app_ctx->rknn_ctx, app_ctx->io_num.n_output, outputs, NULL)`
- `post_process(...)`
- `rknn_outputs_release(...)`

`main.cc` 在进程启动时 init 一次模型，然后处理输入目录内图片，最后 release。旧脚本每次只放一张输入图并启动一次二进制，因此对旧 Qt AI 来说基本是“每次检测重新初始化 RKNN”。V2 Runner 或未来 RK 常驻服务应缓存模型上下文，避免重复 `rknn_init/rknn_destroy`。

## 8. 输入尺寸与预处理分析

输入图片由 `main.cc` 用 OpenCV `imread(..., IMREAD_COLOR)` 读取，OpenCV 默认 BGR。随后：

- BGRA 输入转 RGB。
- BGR 输入转 RGB。
- `image_buffer_t` 标记为 `IMAGE_FORMAT_RGB888`。

`inference_yolov8_model()` 根据模型输入尺寸创建 `dst_img`，格式固定 `IMAGE_FORMAT_RGB888`，背景色 `114`，调用 `convert_image_with_letterbox()`。

`convert_image_with_letterbox()`：

- 计算 `scale = min(dst_w/src_w, dst_h/src_h)`。
- resize 后按 RGA 约束做宽 4 对齐、高 2 对齐的轻微调整。
- 计算水平或垂直 padding。
- 居中放置图像。
- 写出 `letterbox->scale`、`x_pad`、`y_pad`。
- 调用 `convert_image()`，优先 RGA，失败回退 CPU。

未看到显式 mean/std normalize；输入类型为 `RKNN_TENSOR_UINT8`。输入设置为 `RKNN_TENSOR_NHWC`，即使初始化阶段识别到模型原始 fmt 为 NCHW，当前代码仍按 NHWC 喂入，依赖 RKNN runtime 做必要转换。

## 9. YOLO 输出与后处理分析

后处理假设 YOLOv8 三个输出分支：

- `output_per_branch = app_ctx->io_num.n_output / 3`
- 每个分支 box tensor index 为 `i * output_per_branch`
- score tensor index 为 `i * output_per_branch + 1`
- 若 `output_per_branch == 3`，第三个 tensor 为 score_sum 快速过滤

`dfl_len = output_attrs[0].dims[1] / 4`，说明 box 输出是 YOLOv8 DFL 格式。grid 尺寸取 `output_attrs[box_idx].dims[2]` 和 `dims[3]`，stride 为 `model_in_h / grid_h`。

支持 int8/u8 量化和 fp32：

- int8/u8 分支使用 attr 的 `zp/scale` 做阈值量化和反量化。
- fp32 分支直接比较 threshold。

class score 通过遍历 `0..g_obj_class_num-1` 取最大 score。`g_obj_class_num` 来自 `init_post_process(label_path, class_num, box_threshold)`，因此支持多类别，不写死 red/black。

## 10. NMS / IOU / bbox 坐标还原分析

IOU 在 `CalculateOverlap()` 中实现。NMS 在 `nms()` 中按 class 分组执行，阈值来自 `NMS_THRESH=0.45`，运行时由 `inference_yolov8_model()` 固定传入。

中间 box 存储为 `x, y, w, h`：

- DFL 解码得到 `x1/y1/x2/y2`
- 存入 `boxes` 时写入 `x1, y1, w, h`
- NMS 内部临时还原为 xyxy 做 IOU

最终结果结构为 xyxy：

- `left = (clamp(x1 - x_pad) / scale)`
- `top = (clamp(y1 - y_pad) / scale)`
- `right = (clamp(x2 - x_pad) / scale)`
- `bottom = (clamp(y2 - y_pad) / scale)`

因此 bbox 映射回原图依赖 `letterbox.scale`、`letterbox.x_pad`、`letterbox.y_pad`、模型输入宽高。若 V2 后续只传 ROI 裁剪图，还需要把 ROI 原点偏移加回全图坐标。

## 11. 输出结果格式分析

内部 `object_detect_result` 包含：

- `image_rect_t box`
- `float prop`
- `int cls_id`

className 不在结果结构里，但 `main.cc` 输出时调用 `coco_cls_to_name(cls_id)` 从标签表取名称。stdout 行格式为：

```text
<className> @ (<left> <top> <right> <bottom>) <score>
```

旧脚本再包装成：

```text
RESULT_IMAGE=<path>
DETECTION_LINE=<className> @ (<left> <top> <right> <bottom>) <score>
DETECTION_COUNT=<n>
```

DETECTION_LINE 格式可由源码确认，但本轮未运行推理验证实际 stdout。

## 12. 旧方案中必须丢弃的内容

不建议带入 V2 最终生产结构的内容：

- 每次 `cp -a "$SOURCE_ROOT/."` 复制整个 demo。
- 每次 `sed -i` patch 源码。
- 每次 `cmake && make`。
- 依赖脚本扫描 log 文本，而不是结构化 JSON。
- 依赖 `LD_LIBRARY_PATH` 拼接运行时库。
- 旧脚本会在运行目录删除临时输入/输出文件。
- 旧 CMake 仍引用 `3rdparty`、`utils`、OpenCV、rknn lib 等完整构建环境，不适合直接塞进 V2。

这些内容只能作为过渡桥接，不应成为 V2 的 AI 模块边界。

## 13. V2 最小化 SSH 桥接方案

建议下一阶段先做 SSH 桥接版，暂时不分离 RK 常驻服务，保留 V2 原架构：

```text
ToolEngine
-> AiDetectionAdapter
-> AiDetectionRunner
-> QProcess scp / ssh
-> RK run_rknn_demo.sh
-> stdout parser
-> ToolResult
-> ToolOverlay
```

落地建议：

- `AiDetectionAdapter` 实现 `ToolAdapter`，支持 `ToolType::ObjectDetection`。
- `AiDetectionRunner` 负责保存当前 `cv::Mat` 到临时图片，`scp` 到 RK 临时输入目录。
- 默认 RK 路径先写为 `/home/cat/YOLOv8_RK3588_object_detect-main`、`/home/cat/model/yolov8_red_black.rknn`、`/home/cat/红黑线标签.txt`、`/home/cat/qt-AI/scripts/run_rknn_demo.sh`。
- 当前 UI 的 `modelName` 暂时映射到固定模型 `yolov8_red_black.rknn`。
- `confidenceThreshold` 可映射到脚本 `box_threshold`；若 UI 是 0-100 整数，先转成 `value / 100.0`，也可临时固定 `0.02` 与旧配置一致。
- `nmsIouThreshold/maxOverlap` 旧脚本暂不能传入，因为 demo 中 NMS 固定 `0.45`。先在报告/代码注释中标注限制。
- stdout parser 解析 `RESULT_IMAGE`、`DETECTION_LINE`、`DETECTION_COUNT`。
- 若 scp 结果图回传不稳定，可先只把 count/raw lines 写入 `ToolResult.payload`。
- bbox 格式已从源码确认，可用正则解析 `class @ (left top right bottom) score`，生成 `ToolOverlayType::Rect`。

缺点：

- 依赖网络。
- 依赖 ssh/scp。
- 依赖旧脚本。
- 旧脚本可能每次 cmake/make。
- 不是最终架构。
- 错误恢复和超时控制需要 V2 Runner 自己补齐。

## 14. 后续分离式架构建议

后续仍建议改成 RK 常驻服务：

- 服务启动时加载模型并缓存 RKNN context。
- 请求只发送图片或共享路径，返回结构化 JSON。
- 避免每次 `rknn_init`。
- 避免每次编译和 patch。
- 减少 ssh/scp 连接成本。
- 可以统一管理模型、标签、阈值、NMS、超时和日志。
- 可以直接返回 `detections: [{className, classId, score, bbox}]`，V2 只负责适配 `ToolResult/ToolOverlay`。

最小服务接口建议：

```json
{
  "model": "yolov8_red_black.rknn",
  "imagePath": "/tmp/v2_ai/input.jpg",
  "boxThreshold": 0.02,
  "nmsThreshold": 0.45
}
```

返回：

```json
{
  "success": true,
  "count": 1,
  "detections": [
    {"className": "red", "classId": 0, "score": 0.91, "bbox": [10, 20, 120, 80]}
  ],
  "resultImagePath": "/tmp/v2_ai/output.jpg"
}
```

## 15. 当前不修改 UI 参数的原因

当前 `ObjectDetectionDialog` 已把 `modelName`、`confidenceThreshold/detectMinScore`、`maxOverlap/nmsIouThreshold`、`maxDetections`、ROI、类别过滤等写入 `ToolConfig.params`。最小 SSH 桥接阶段可以在 Adapter 内解释这些字段，不需要改 `ObjectDetectionDialog.ui`。

暂不改 UI 的收益：

- 避免扩大本轮变更面。
- 保持 V2 原有工具配置流程。
- 先验证 RK 远端调用、stdout parser、ToolResult/ToolOverlay 是否跑通。
- `modelName` 可先映射固定 red/black 模型，后续再做模型管理。

## 16. 下一阶段建议

下一阶段建议按以下顺序推进：

1. 新增 `AiDetectionRunner`，只做本地临时图保存、scp、ssh 调脚本、stdout parser。
2. 新增 `AiDetectionAdapter`，接入 `ToolEngine`，返回 `ToolResult.count/text/payload`。
3. 先不画框，只显示 `detectionCount` 和 `rawDetectionLines`。
4. 再启用 bbox regex 解析，生成 `ToolOverlayType::Rect` 和可选 `Text` overlay。
5. 做超时、错误分类和 SSH 连接失败提示。
6. 如果 SSH 桥接可用，再抽离 RK 常驻服务。
7. 若要本地编译 demo，需要补齐本轮未列入材料清单的小头文件和库依赖，例如 `common.h`、`file_utils.h`、`image_drawing.h`、`3rdparty`/rknn runtime；但这些不应进入 V2 源码主线。
