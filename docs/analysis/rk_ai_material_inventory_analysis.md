# RK AI / RKNN 目标检测材料盘点报告

## 1. 扫描方式

本轮从 V2 工程所在虚拟机通过 SSH 连接 `cat@192.168.31.88`，对 RK 板执行只读扫描。扫描输出写入本地 V2 工程：

- 原始日志：`docs/analysis/rk_ai_material_inventory_raw.log`
- 分析报告：`docs/analysis/rk_ai_material_inventory_analysis.md`

本轮没有在 RK 板上创建报告文件，没有复制模型，没有打包目录，没有运行推理程序，没有打开相机，没有启动 qtt5 / V1 / V2 应用，没有停止服务、编译、安装、删除、移动或修改 RK 板文件。

说明：初始 broad scan 被 HALCON 文档和第三方 OpenCV 头文件噪声拖慢，中途本地中断；后续使用聚焦只读扫描补充了 RKNN / YOLO / Qt 关键目录。最后阶段新 SSH 连接出现 `Connection refused`，因此实际可执行程序的完整 `file/ldd` 和源码关键词 grep 未完全采集。

## 2. RK 系统信息

| 项目 | 结果 |
| --- | --- |
| hostname | `lubancat` |
| 用户 | `cat` |
| 系统 | Debian GNU/Linux 11 bullseye |
| 内核 | `Linux lubancat 5.10.160 #13 SMP Thu Oct 16 13:39:40 CST 2025 aarch64` |
| 板型 compatible | `rockchip,rk3588s-lubancat-4`, `rockchip,rk3588` |
| 根分区 | `/dev/mmcblk0p3`, 29G, 已用 19G, 可用 9.1G, 使用率 67% |
| 内存 | 3.8Gi, 已用 792Mi, 可用 2.9Gi |
| swap | 0B |
| 扫描时间 | 2026-06-05 15:30-16:01 CST |

## 3. RKNN runtime / header 清单

已确认存在 RKNN runtime 和头文件。

| 类型 | 路径 | 大小 | 判断 |
| --- | --- | --- | --- |
| 系统 runtime | `/usr/lib/librknnrt.so` | 4.6M | RK 板系统级 RKNN runtime |
| YOLO demo runtime | `/home/cat/YOLOv8_RK3588_object_detect-main/rknn_lib/librknnrt.so` | 6.9M | YOLO demo 随项目携带 |
| YOLO demo header | `/home/cat/YOLOv8_RK3588_object_detect-main/include/rknn_api.h` | 35K | 存在 |
| RKNN rknpu2 header | `/home/cat/YOLOv8_RK3588_object_detect-main/3rdparty/rknpu2/include/rknn_api.h` | 35K | 存在 |
| RKNN matmul header | `/home/cat/YOLOv8_RK3588_object_detect-main/3rdparty/rknpu2/include/rknn_matmul_api.h` | 18K | 存在 |
| qt-AI runtime copy | `/home/cat/qt-AI/runtime/rknn_build/source/rknn_lib/librknnrt.so` | 6.9M | qt-AI 运行脚本复制/使用的源码树 |
| qt-AI cam0 runtime copy | `/home/cat/qt-AI/runtime/rknn_build_cam0/source/rknn_lib/librknnrt.so` | 6.9M | 第二套 runtime/source 目录 |

结论：include/lib 配套目录存在，且有 `rknpu1`、`rknpu2`、`Linux/aarch64` 等第三方 runtime 目录。V2 接入时优先参考系统 `/usr/lib/librknnrt.so` 和 YOLO demo 自带 `rknn_lib/librknnrt.so` 的加载方式。

## 4. 模型文件清单

| 路径 | 大小 | 初步判断 | 是否可能是目标检测模型 |
| --- | ---: | --- | --- |
| `/home/cat/model/yolov8_red_black.rknn` | 3.9M | RKNN 模型；qt-AI 配置中正在使用，`classCount=2` | 是，高优先级 |
| `/home/cat/model/yolov8.rknn` | 12M | 通用 YOLOv8 RKNN 模型 | 是 |
| `*/CMakeDetermineCompilerABI_*.bin` | 9.2K | CMake 编译器 ABI 测试产物，ELF aarch64 | 否 |

未在有效扫描结果中发现 `.onnx`、`.pt`、`.weights`、`.om`、`.hdlmc`。`.bin/.param` 未发现与检测模型直接相关的材料，主要是 CMake build 产物。

## 5. 标签文件清单

| 路径 | 大小 | 内容摘要 | 判断 |
| --- | ---: | --- | --- |
| `/home/cat/红黑线标签.txt` | 9B | `red`, `black` | 与 `yolov8_red_black.rknn` 匹配 |

`/home/cat/qt-AI/app_config.json` 中明确配置：

- `labelPath`: `/home/cat/红黑线标签.txt`
- `classCount`: `2`
- `modelPath`: `/home/cat/model/yolov8_red_black.rknn`

结论：红黑线二分类检测的标签文件存在且内容明确。

## 6. 推理脚本清单

| 路径 | 大小 | 调用关系 |
| --- | ---: | --- |
| `/home/cat/qt-AI/scripts/run_rknn_demo.sh` | 2.6K | qt-AI 配置指定的 RKNN 启动脚本 |

脚本行为从 RAW 预览确认如下：

- 参数：`<source_root> <model_path> <input_image> <output_dir> <build_dir>`
- 检查架构必须为 `aarch64`
- 将 `SOURCE_ROOT` 复制到 `$BUILD_DIR/source`
- 用 `sed -i` 修改 `src/main.cc` 中的 `modelPath`、`imageFolder`、`outputFolder`
- 将输入图复制到 `$BUILD_DIR/input_image`
- 在 `$PATCH_BUILD` 下执行 `cmake` 和 `make`
- 设置 `LD_LIBRARY_PATH`，运行 `$PATCH_BUILD/rknn_yolov8_demo`
- 输出 `RESULT_IMAGE=...`
- 从 log 中抽取包含 ` @ (` 的检测行，输出 `DETECTION_LINE=...`
- 输出 `DETECTION_COUNT=...`

本轮仅预览脚本，没有执行脚本。需要注意：该脚本如果运行，会编译并运行 RKNN demo，还会写入 runtime/build/input/results 目录；V2 接入时不建议照搬这种运行期 patch + 编译方式。

## 7. 推理可执行程序清单

已确认的可执行/候选：

| 路径 | 证据 | 判断 |
| --- | --- | --- |
| `/home/cat/qt-AI/qt-AI` | `ls -lh` 显示 216904 bytes，可执行 | Qt AI 应用，不应在本轮启动 |
| `/home/cat/qt-AI/scripts/run_rknn_demo.sh` | 可执行脚本 | 调用外部 RKNN demo |
| `/home/cat/YOLOv8_RK3588_object_detect-main/build/...` | 目录存在；README 说明 build 后运行 `./rknn_yolov8_demo` | 实际 demo 二进制未完整 `file/ldd` |
| `CMakeDetermineCompilerABI_*.bin` | `file` 显示 aarch64 ELF | CMake 测试产物，非推理程序 |

限制：实际 `rknn_yolov8_demo` 的 `file/ldd` 未完成采集，最后新 SSH 连接返回 `Connection refused`。因此是否直接依赖 `librknnrt.so`、OpenCV、Qt 等，不能仅凭本轮 `ldd` 下定论。基于目录结构和 CMake/README，可推断 YOLO demo 依赖 RKNN runtime 和 bundled OpenCV，但需下一轮复扫或拷回源码确认。

## 8. 推理源码清单

已确认关键源码存在：

| 路径 | 大小 | 作用判断 |
| --- | ---: | --- |
| `/home/cat/YOLOv8_RK3588_object_detect-main/src/main.cc` | 6.2K | YOLO demo 主程序，脚本会 patch 模型/输入/输出路径 |
| `/home/cat/YOLOv8_RK3588_object_detect-main/src/yolov8.cc` | 7.6K | README 说明为模型初始化、推理、反初始化 |
| `/home/cat/YOLOv8_RK3588_object_detect-main/src/postprocess.cc` | 18K | README 说明为推理后处理 |
| `/home/cat/YOLOv8_RK3588_object_detect-main/include/yolov8.h` | 727B | YOLO 接口声明 |
| `/home/cat/YOLOv8_RK3588_object_detect-main/include/postprocess.h` | 901B | 后处理声明 |
| `/home/cat/YOLOv8_RK3588_object_detect-main/utils/image_utils.c` | 28K | 图像处理工具，可能包含 resize/格式转换 |
| `/home/cat/YOLOv8_RK3588_object_detect-main/utils/image_drawing.c` | 44K | 绘制检测框/文本 |
| `/home/cat/YOLOv8_RK3588_object_detect-main/CMakeLists.txt` | 2.6K | demo 构建入口 |
| `/home/cat/qt-AI/yolorunner.cpp` | 6.1K | Qt 侧 YOLO 调用封装 |
| `/home/cat/qt-AI/appconfig.cpp/.h` | 7.6K / 1.1K | 读取模型、标签、脚本、阈值配置 |
| `/home/cat/qt-AI/mainwindow.cpp/.h` | 24K / 2.5K | Qt 主窗口、相机/推理入口 |
| `/home/cat/qt-AI/camerastream.cpp/.h` | 13K / 1.5K | 相机流封装 |

`rknn_init`、`rknn_run`、`rknn_outputs_get`、`rknn_query`、NMS、IOU、letterbox、bbox 坐标还原等函数级命中未完整采集；原因是源码深度 grep 阶段被第三方 OpenCV 头文件噪声拖慢，后续 SSH 连接受限。现有证据足以确认源码材料存在，但函数细节需下一步拷回源码或复扫。

## 9. Qt / QProcess 调用痕迹

RAW 未成功采集 `QProcess` 关键词命中，但已有间接证据表明 Qt 程序通过外部脚本/程序做推理：

- `app_config.json` 的 `yolo.scriptPath` 为 `scripts/run_rknn_demo.sh`
- `app_config.json` 的 `sourceRoot` 为 `/home/cat/YOLOv8_RK3588_object_detect-main`
- `README.md` 写明点击拍照后抓取左侧相机当前帧做 YOLO 推理
- `README.md` 写明脚本会复制 YOLO demo、patch `src/main.cc`、执行 `cmake/make`
- `yolorunner.cpp` 存在，符合 Qt 侧推理 runner 封装命名

结论：基本可以判断旧 qt-AI 是 Qt 调外部脚本/外部 demo 的模式，但是否使用 `QProcess` 需要下一轮读取 `yolorunner.cpp` 确认。

## 10. 预处理逻辑

已确认材料：

- YOLO demo 有 `utils/image_utils.c`
- README 提到 `python_script/jpg2png.py`，说明 jpg 读取曾有兼容问题
- demo 有 `src/main.cc`、`src/yolov8.cc`、`src/postprocess.cc`
- `app_config.json` 有输入抓拍和输出目录配置

未完整确认：

- `resize`
- `letterbox`
- `normalize`
- RGB/BGR 转换
- input size
- ROI crop

判断：预处理代码很可能在 `utils/image_utils.c`、`src/main.cc`、`src/yolov8.cc` 中，但本轮未取得函数级 grep 证据。V2 接入前必须把上述源码拷回本地细读。

## 11. 后处理逻辑

已确认材料：

- `/home/cat/YOLOv8_RK3588_object_detect-main/src/postprocess.cc` 存在，18K
- `/home/cat/YOLOv8_RK3588_object_detect-main/include/postprocess.h` 存在
- README 明确 `src/postprocess.cpp` 是模型推理后的后处理代码
- 标签为 `red`、`black`

未完整确认：

- YOLO output parser 的具体 tensor 解析
- confidence threshold 的来源和默认值
- NMS / IOU 实现
- bbox 坐标还原
- class score 计算

判断：后处理源码存在，具备迁移参考价值；但需要拷回 `postprocess.cc/.h` 后确认阈值、坐标系和输出格式。

## 12. 输出结果格式

已确认输出路径和格式线索：

- `app_config.json`：`outputDir` 为 `runtime/results`
- `run_rknn_demo.sh` 生成输出图：`${INPUT_BASENAME%.*}_out.png`
- 脚本输出 `RESULT_IMAGE=<output png path>`
- 脚本从 log 中抽取检测行，输出 `DETECTION_LINE=<line>`
- 脚本输出 `DETECTION_COUNT=<count>`
- `/home/cat/output_image` 下已有 `_out.png` 结果图：
  - `20260319_11-52-45-925-SN3_前端端子__out.png`
  - `20260319_13-14-37-148-SN4_前端端子__out.png`
  - `20260327_111319_419_cam1 - 副本 (2)_out.png`
  - `20260327_111419_075_cam1 - 副本 (4)_out.png`

未确认：

- 是否有 `.txt` / `.json` 检测结果文件
- `DETECTION_LINE` 中 bbox 的字段顺序
- stdout 原始 log 样例

判断：输出至少包括标注图和 stdout 文本行，脚本层已经把它们规整为 `RESULT_IMAGE`、`DETECTION_LINE`、`DETECTION_COUNT`，可作为 V2 `AiDetectionRunner` 解析参考。

## 13. 可迁移材料

可直接拷回 V2 虚拟机分析的材料：

- `/home/cat/model/yolov8_red_black.rknn`
- `/home/cat/model/yolov8.rknn`
- `/home/cat/红黑线标签.txt`
- `/home/cat/YOLOv8_RK3588_object_detect-main/src/main.cc`
- `/home/cat/YOLOv8_RK3588_object_detect-main/src/yolov8.cc`
- `/home/cat/YOLOv8_RK3588_object_detect-main/src/postprocess.cc`
- `/home/cat/YOLOv8_RK3588_object_detect-main/include/yolov8.h`
- `/home/cat/YOLOv8_RK3588_object_detect-main/include/postprocess.h`
- `/home/cat/YOLOv8_RK3588_object_detect-main/include/rknn_api.h`
- `/home/cat/YOLOv8_RK3588_object_detect-main/utils/image_utils.c`
- `/home/cat/YOLOv8_RK3588_object_detect-main/utils/image_utils.h`
- `/home/cat/YOLOv8_RK3588_object_detect-main/utils/image_drawing.c`
- `/home/cat/YOLOv8_RK3588_object_detect-main/CMakeLists.txt`
- `/home/cat/qt-AI/app_config.json`
- `/home/cat/qt-AI/scripts/run_rknn_demo.sh`
- `/home/cat/qt-AI/yolorunner.cpp`
- `/home/cat/qt-AI/yolorunner.h`
- 小测试图：`/home/cat/input_image/*.jpg`
- 输出样例图：`/home/cat/output_image/*_out.png`

可作为 V2 `AiDetectionRunner` 参考的材料：

- `run_rknn_demo.sh` 的 stdout 规整方式：`RESULT_IMAGE`、`DETECTION_LINE`、`DETECTION_COUNT`
- `app_config.json` 中模型/标签/脚本/输出目录配置字段
- YOLO demo 的 `main.cc` 路径参数替换思路
- `postprocess.cc/.h` 的 YOLO 后处理逻辑

只能参考不能直接迁移的材料：

- `/home/cat/qt-AI` 整个旧 Qt 应用，包含相机和界面逻辑，不应直接并入 V2
- 脚本中的运行期 `cmake/make`、`sed -i`、复制源码、删除/写入 runtime 目录等做法
- `3rdparty/opencv` 大目录，除非确认 V2/RK 端缺少对应 OpenCV
- CMake build 目录和 `.o`、`moc_*`、`CMakeDetermineCompilerABI_*.bin`

## 14. 缺失材料

已找到：

- 模型：已找到两个 `.rknn`
- 标签：已找到 `red/black`
- RKNN runtime：已找到系统和项目内 runtime
- `rknn_api.h`：已找到
- 推理源码：已找到关键源码路径
- 后处理源码：已找到 `postprocess.cc/.h`
- 测试图片：已找到 `/home/cat/input_image`
- 输出样例：已找到 `/home/cat/output_image`

仍缺或未完整采集：

- 实际 `rknn_yolov8_demo` 二进制的完整 `file/ldd`
- `yolov8.cc` 中 RKNN API 调用的函数级 grep 证据
- `postprocess.cc` 中 NMS/IOU/bbox/class score 的函数级细节
- stdout 原始检测样例 log
- 是否存在 `.txt` / `.json` 结构化检测结果文件
- `.onnx` / `.pt` 原始训练或转换前模型，当前未发现

## 15. 是否具备接入 V2 AiDetectionRunner 的条件

判断：基本具备，但缺部分材料确认。

理由：

- 目标 RK3588 板上存在 RKNN runtime 和头文件
- 存在实际使用中的 `yolov8_red_black.rknn`
- 存在 `red/black` 标签文件
- 存在 YOLO demo 源码、后处理源码、Qt 外部调用脚本和配置
- 存在测试输入图和输出样例图

阻塞点：

- 尚未确认实际 demo 可执行文件依赖
- 尚未细读 `yolov8.cc` / `postprocess.cc` 的输入尺寸、预处理、输出 tensor、NMS/IOU、bbox 坐标系
- 旧脚本会运行期编译和 patch 源码，不适合直接作为 V2 生产调用方式

V2 接入建议：不要直接照搬旧 `run_rknn_demo.sh`；先把必要源码和小样例拷回 V2 虚拟机，整理成稳定的 RKNN runner 接口，再决定是在 V2 中直接链接 RKNN runtime，还是短期使用外部进程桥接。

## 16. 下一步建议

下一步建议只拷贝必要材料，不整盘复制：

1. 拷贝模型和标签：
   - `/home/cat/model/yolov8_red_black.rknn`
   - `/home/cat/红黑线标签.txt`
   - 可选：`/home/cat/model/yolov8.rknn`

2. 拷贝 YOLO demo 核心源码：
   - `/home/cat/YOLOv8_RK3588_object_detect-main/src/main.cc`
   - `/home/cat/YOLOv8_RK3588_object_detect-main/src/yolov8.cc`
   - `/home/cat/YOLOv8_RK3588_object_detect-main/src/postprocess.cc`
   - `/home/cat/YOLOv8_RK3588_object_detect-main/include/yolov8.h`
   - `/home/cat/YOLOv8_RK3588_object_detect-main/include/postprocess.h`
   - `/home/cat/YOLOv8_RK3588_object_detect-main/include/rknn_api.h`
   - `/home/cat/YOLOv8_RK3588_object_detect-main/utils/image_utils.c`
   - `/home/cat/YOLOv8_RK3588_object_detect-main/utils/image_utils.h`
   - `/home/cat/YOLOv8_RK3588_object_detect-main/utils/image_drawing.c`
   - `/home/cat/YOLOv8_RK3588_object_detect-main/CMakeLists.txt`

3. 拷贝 Qt 调用参考：
   - `/home/cat/qt-AI/app_config.json`
   - `/home/cat/qt-AI/scripts/run_rknn_demo.sh`
   - `/home/cat/qt-AI/yolorunner.cpp`
   - `/home/cat/qt-AI/yolorunner.h`
   - `/home/cat/qt-AI/appconfig.cpp`
   - `/home/cat/qt-AI/appconfig.h`

4. 拷贝小测试输入和输出样例：
   - `/home/cat/input_image/*.jpg`
   - `/home/cat/output_image/*_out.png`

5. 下一轮只读复扫优先项：
   - `file` / `ldd` 实际 `rknn_yolov8_demo`
   - `grep -n` 只针对 `src/yolov8.cc`、`src/postprocess.cc`、`utils/image_utils.c`，避免递归第三方 OpenCV
   - 读取最近一次 `last_run.log`，如果存在且较小，确认 `DETECTION_LINE` bbox 格式
