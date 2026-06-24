# V2 目标检测 SSH 桥接阶段 4 报告

## 1. 修改文件列表

- `qt_ui_test.pro`
- `src/MainWindow.h`
- `src/MainWindow.cpp`

## 2. 新增文件列表

- `src/algorithms/ai/AiDetectionRunner.h`
- `src/algorithms/ai/AiDetectionRunner.cpp`
- `src/tooladapters/AiDetectionAdapter.h`
- `src/tooladapters/AiDetectionAdapter.cpp`
- `smoke/ai_detection_bridge_smoke.cpp`
- `smoke/ai_detection_bridge_smoke.pro`
- `docs/analysis/ai_detection_remote_rknn_bridge_stage4_report.md`

## 3. 是否修改 UI

未修改 ObjectDetectionDialog.ui，未新增 UI 参数。

## 4. 是否保持 V2 原架构

仍走 ToolEngine -> ToolAdapter -> Runner -> ToolResult -> Overlay。

本轮只注册 `AiDetectionAdapter` 到主界面已有 `ToolEngine`，没有在 `ObjectDetectionDialog` 或 `ToolLibraryDialog` 中直接跑算法。

## 5. 命令行远程推理验证结果

安全检查：

- `git status --short`：进入任务前工作区已有大量未提交/未跟踪改动。
- `du -sh rk_ai_materials`：`20M`
- 关键文件/目录均存在：`ui/ObjectDetectionDialog.ui`、`src/ObjectDetectionDialog.h`、`src/ObjectDetectionDialog.cpp`、`rk_ai_materials/`、两份 RK 分析报告。

测试图：

- `rk_ai_materials/input_image/20260319_11-52-45-925-SN3_前端端子_.jpg`

传图：

- 普通 `scp` 在输入密码后被远端关闭。
- `scp -O` 成功上传到 `/tmp/v2_ai_input/v2_test.jpg`。

8 参数命令行验证：

- 按任务给定 8 参数调用 `/home/cat/qt-AI/scripts/run_rknn_demo.sh`。
- 当前 RK 板实际脚本不是 8 参数版，而是 5 参数旧版。
- 8 参数调用发生参数错位，末尾错误：`ERROR: result image not generated: 2/红黑线标签_out.png`
- 未得到 `RESULT_IMAGE=` / `DETECTION_COUNT=` / `DETECTION_LINE=`。

当前板端 5 参数兼容验证：

- 直接读取远端脚本确认 usage 为 `<source_root> <model_path> <input_image> <output_dir> <build_dir>`。
- 按 5 参数兼容调用成功：
  - `RESULT_IMAGE=/tmp/v2_ai_output/v2_test_out.png`
  - `DETECTION_COUNT=0`
  - 本张图无 `DETECTION_LINE`。

## 6. run_rknn_demo.sh 8 参数说明

本地材料和目标接口要求的 8 参数顺序：

1. `source_root`
2. `model_path`
3. `label_path`
4. `class_count`
5. `box_threshold`
6. `input_image`
7. `output_dir`
8. `build_dir`

当前 RK 板 `/home/cat/qt-AI/scripts/run_rknn_demo.sh` 实际仍是 5 参数旧版。V2 Runner 已实现 8 参数优先调用；若失败且允许 legacy fallback，则兼容当前 5 参数脚本，并在 payload warnings 中记录。

## 7. Runner 默认 RK 路径

- `remoteHost`: `192.168.31.88`
- `remoteUser`: `cat`
- `sourceRootOnRK`: `/home/cat/YOLOv8_RK3588_object_detect-main`
- `modelPathOnRK`: `/home/cat/model/yolov8_red_black.rknn`
- `labelPathOnRK`: `/home/cat/红黑线标签.txt`
- `scriptPathOnRK`: `/home/cat/qt-AI/scripts/run_rknn_demo.sh`
- `classCount`: `2`
- `remoteInputDir`: `/tmp/v2_ai_input`
- `remoteOutputDir`: `/tmp/v2_ai_output`
- `remoteBuildDir`: `/tmp/v2_ai_build`
- `localTempRoot`: `/tmp/v2_ai_bridge`
- `timeoutMs`: `120000`

## 8. scp / ssh / script 调用流程

Runner 流程：

1. 读取 `ToolConfig.params`、`ToolConfig.judgeRule`、`ToolConfig.roiNormalized`。
2. 如 ROI 非全图，按矩形 ROI 裁剪当前图。
3. 保存本地临时图：`/tmp/v2_ai_bridge/request_<timestamp>.jpg`。
4. `ssh` 创建远端临时目录。
5. `scp -O` 上传到 `/tmp/v2_ai_input/request_<timestamp>.jpg`。
6. `ssh` 优先按 8 参数调用 `run_rknn_demo.sh`。
7. 当前 RK 板 8 参数失败时，兼容 5 参数旧脚本。
8. 解析 stdout。
9. 可选 `scp -O` 回传 `RESULT_IMAGE`。

非交互认证说明：

- GUI 内部 `QProcess ssh/scp` 不能交互输入密码。
- Runner 使用 `BatchMode=yes`，适合 SSH key 或 SSH ControlMaster。
- 本轮远程 Runner 验证使用 ControlMaster：`ControlPath=/tmp/v2_ai_bridge_mux_%r_%h_%p`。
- 如果环境有 `sshpass` 且设置 `V2_AI_SSH_PASSWORD`，Runner 会使用 `sshpass -e`；本机当前未安装 `sshpass`。

## 9. stdout 解析规则

- `RESULT_IMAGE=<path>`：记录远端结果图路径。
- `DETECTION_COUNT=<n>`：记录脚本报告数量。
- `DETECTION_LINE=<raw>`：收集 raw detection line。
- 过滤前结果进入 `payload.rawDetections`。
- 过滤/截断后结果进入 `payload.detections`。

## 10. DETECTION_LINE 正则

```text
^(.+?) @ \(([-+0-9.]+)\s+([-+0-9.]+)\s+([-+0-9.]+)\s+([-+0-9.]+)\)\s+([-+0-9.]+)
```

字段含义：

- `className`
- `left`
- `top`
- `right`
- `bottom`
- `score`

## 11. DETECTION_COUNT 是否成功获取

成功获取。

- 命令行 5 参数兼容验证：`DETECTION_COUNT=0`
- V2 Runner 远程验证：`remote_detection_count_payload=0`

8 参数命令行验证未成功，原因是当前 RK 板脚本实际为 5 参数旧版。

## 12. bbox 是否成功解析

- smoke 已成功解析示例：`red @ (10 20 120 80) 0.91`
- 当前远程测试图返回 `DETECTION_COUNT=0`，没有真实 `DETECTION_LINE`，因此本次远程运行没有 bbox 可解析。
- Runner 的 parser、normalized bbox、ROI offset 回填逻辑已由 smoke 覆盖。

## 13. Overlay 是否已输出

已实现。

- 有 bbox 且 `showBoxes=true` 时输出 `ToolOverlayType::Rect`。
- `showLabels/showScores` 为 true 时输出 `ToolOverlayType::Text`。
- 本次远程实测 count 为 0，所以实际没有 overlay。

## 14. UI 参数遵循情况

| UI 参数 | ToolConfig 字段 | 桥接版是否已生效 | 说明 |
| --- | --- | --- | --- |
| `modelComboBox` | `params.modelName` | 已读取 | 默认/红黑线/yolov8 映射到 `/home/cat/model/yolov8_red_black.rknn`，无法识别时 fallback 并 warning。 |
| `roiNormalized` | `ToolConfig.roiNormalized` | 已生效 | 矩形 ROI 会裁剪上传，bbox 再映射回原图坐标；自由 ROI 暂未支持。 |
| `detectMinScoreSpinBox` | `params.detectMinScore` / `params.confidenceThreshold` | 8 参数版已接入；当前 5 参数 fallback 未生效 | 百分制会转 0-1，传给 8 参数脚本 `box_threshold`；当前 RK 旧脚本不接收该参数。 |
| `maxOverlapSpinBox` | `params.maxOverlap` / `params.nmsIouThreshold` | 已读取，暂未传入 RK | payload 保留；旧脚本无 NMS 参数。 |
| `maxFindCountSpinBox` | `params.maxDetections` / `params.maxFindCount` | 已生效 | V2 端解析后截断。 |
| `classFilterSwitch / classFilterLineEdit` | `params.classFilterEnabled` / `params.classFilterText` | 已生效 | V2 端按 className 过滤 red/black 等类别。 |
| `width filter` | `params.widthFilterEnabled/minWidth/maxWidth` | 已生效 | 按 bbox pixel width 过滤。 |
| `height filter` | `params.heightFilterEnabled/minHeight/maxHeight` | 已生效 | 按 bbox pixel height 过滤。 |
| `angle filter` | `params.angleFilterEnabled/minAngle/maxAngle` | 已读取，暂未生效 | RK 输出无 angle，payload warning。 |
| `boundary filter` | `params.boundaryFilterEnabled/boundaryOverlapRatio` | 已读取，暂未完整生效 | 当前未定义明确出界算法，payload warning。 |
| `resultBasisComboBox` | `judgeRule.mode` / `judgeRule.resultBasis` | 基础生效 | 支持 count / score / category 基础判断。 |
| `minCountSpinBox / maxCountSpinBox` | `judgeRule.minCount/maxCount` | 已生效 | count 模式使用数量范围。 |
| `minScoreSpinBox` | `judgeRule.minScore` | 已生效 | score 模式按 bestScore 判断，百分制转 0-1。 |
| `categoryLineEdit` | `judgeRule.category` | 已生效 | category 模式按过滤后 detections 的 className 判断。 |

本轮未修改 ObjectDetectionDialog.ui，未新增 UI 参数，桥接 Runner 只读取现有 UI 保存到 ToolConfig 的参数。

## 15. ToolResult / payload 字段说明

`ToolResult`：

- `toolType = ToolType::AiDetection`
- `success`：远程流程是否成功。
- `ok`：按 `judgeRule` 判定。
- `count`：过滤并截断后的 detection 数。
- `score`：过滤并截断后的 bestScore。
- `message`：成功时 `目标检测完成，检测到 N 个目标`，失败时包含远程失败原因。
- `overlays`：bbox rect/text overlay。

payload 至少包含：

- `remoteHost`
- `modelName`
- `modelPathOnRK`
- `labelPathOnRK`
- `scriptPathOnRK`
- `sourceRootOnRK`
- `classCount`
- `boxThreshold`
- `nmsIouThreshold` / `maxOverlap`
- `maxDetections`
- `roiNormalized`
- `roiApplied`
- `localInputImage`
- `remoteInputImage`
- `remoteResultImage`
- `localResultImage`
- `rawDetectionCount`
- `scriptDetectionCount`
- `detectionCount`
- `rawDetections`
- `detections`
- `rawDetectionLines`
- `stdout`
- `stderr`
- `elapsedMs`
- `warnings`

## 16. OK/NG 判定说明

判定在解析、类别过滤、宽高过滤、maxDetections 截断之后执行。

- `count`：`minCount <= detectionCount <= maxCount`
- `score`：至少一个 detection 且 `bestScore >= minScore`
- `category`：至少一个 detection 的 `className == category`
- 未识别 `resultBasis` 时 fallback 到 count 基础判断。

`ToolResult.ok` 不再简单等于 `detectionCount > 0`。

## 17. qmake / make 编译结果

- `qmake`：通过，exit code 0。
- `make -j$(nproc)`：通过，exit code 0。

未添加 RKNN / ONNX / OpenVINO 链接库。

## 18. smoke 验证结果

新增 smoke：`smoke/ai_detection_bridge_smoke.cpp`

执行结果：

```text
ai_detection_bridge_smoke: PASS
```

覆盖：

- 单条 `DETECTION_LINE` parser。
- 多条 `DETECTION_LINE` parser。
- 无检测结果。
- `classFilterEnabled=true` 过滤 red / black。
- `maxDetections` 截断。
- width / height filter。
- `minCount / maxCount` 基础 OK/NG。
- SSH 失败返回错误且不崩溃。
- `ToolType::AiDetection` 有 Adapter 支持。
- `ObjectDetectionDialog.ui` 文件仍存在。

## 19. 运行验证结果

主程序启动：

- `timeout 5s env QT_QPA_PLATFORM=offscreen ./qt_ui_test`
- 结果：5 秒后被 timeout 结束，未崩溃。

V2 Runner 远程调用：

```text
remote_success=1
remote_ok=1
remote_status=ok
remote_message=目标检测完成，检测到 0 个目标
remote_count=0
remote_raw_count=0
remote_detection_count_payload=0
remote_bbox_parsed=0
remote_script_argument_mode=5_legacy_fallback
remote_result_image=/tmp/v2_ai_output/request_1780655609737_out.png
```

说明：

- Runner 成功保存临时图。
- Runner 成功 `scp -O` 上传到 RK。
- Runner 成功 `ssh` 调脚本。
- Runner 优先 8 参数失败后，兼容当前 RK 5 参数旧脚本成功。
- Runner 成功解析 `DETECTION_COUNT=0`。
- 当前样图没有 `DETECTION_LINE`，所以本轮远程实测无 bbox。
- bbox parser 和 overlay 生成逻辑已由 smoke 覆盖。
- Headless 环境未做人工点击 UI；参数页未改 UI，主工程编译和 offscreen 启动通过。

## 20. 当前缺陷

- 依赖网络。
- 依赖 `ssh/scp`。
- 依赖旧脚本。
- 旧脚本可能每次 `cmake/make`。
- 不是最终架构。
- 后续仍建议 RK 常驻服务。
- NMS 阈值当前未传入 RK 脚本。
- 角度 / 出界过滤当前可能只是记录，未完整生效。
- 当前 RK 板脚本实际为 5 参数旧版，和本地材料/目标 8 参数接口不一致。
- GUI 内部非交互 SSH 需要 SSH key、ControlMaster，或安装 `sshpass` 并设置 `V2_AI_SSH_PASSWORD`。

## 21. 下一阶段建议

- 优化 RK 端脚本，避免每次 `cmake/make`。
- 或拆成 RK 常驻服务。
- 增加远程参数配置。
- 增加模型管理。
- 增加更稳定的错误 / 超时处理。
- 完善 ROI 裁剪与坐标还原。
- 完善 Overlay 与 OK/NG 判定。
- 统一 RK 板脚本为 8 参数版，或明确保留 5 参数 legacy 协议并在配置中显式标识。
