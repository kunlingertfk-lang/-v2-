# AI 目标检测算法链解析

> 功能：目标检测（`ToolType::AiDetection`）  
> 当前实现：RK3588 远程推理桥接，属于项目 HALCON 红线的 AI 例外  
> 核对日期：2026-07-23

## 1. 功能定位与链路

当前 AI 目标检测不是本机模型推理。它把本地图像或矩形 ROI 保存为临时 JPEG，经 SSH/SCP 上传到 RK 设备，调用远端 `run_rknn_demo.sh`，解析 stdout 中的检测框，再回传统一 `ToolResult`。

```text
ToolLibraryDialog(ObjectDetection)
  -> ObjectDetectionDialog
  -> ToolConfig
  -> AiDetectionAdapter
  -> AiDetectionRunner
       -> 本地 ROI 裁剪/JPEG
       -> ssh 创建远端目录
       -> scp 上传图像
       -> ssh 执行 RKNN shell
       -> 解析 stdout
       -> 本地过滤/判定
       -> 可选 scp 回传结果图
  -> ToolResult / overlays / payload
```

## 2. 文件地图

| 层级 | 文件 |
| --- | --- |
| UI | `src/ObjectDetectionDialog.{h,cpp}`、`ui/ObjectDetectionDialog.ui` |
| Adapter | `src/tooladapters/AiDetectionAdapter.{h,cpp}` |
| Runner | `src/algorithms/ai/AiDetectionRunner.{h,cpp}` |
| 入口/运行 | `src/ToolLibraryDialog.cpp`、`src/MainWindow.cpp` |
| 构建 | `qt_ui_test.pro` |
| smoke | `smoke/ai_detection_bridge_smoke.{cpp,pro}` |

注意：`ObjectDetectionDialog` 当前只负责配置、ROI 和静态预览，没有像六个有无 Dialog/OCR 那样内置测试 ToolEngine/Runner 调用。

## 3. UI 配置合同

| 分组 | `ToolConfig.params` 字段 |
| --- | --- |
| 模型 | `modelName` |
| 检测域 | `detectRegionType`、`ToolConfig.roiNormalized` |
| 推理阈值 | `maxDetections`、`confidenceThreshold`/`detectMinScore`、`maxOverlap`/`nmsIouThreshold` |
| 过滤 | `classFilterEnabled/Text`、`angleFilterEnabled/minAngle/maxAngle` |
| 过滤 | `widthFilterEnabled/minWidth/maxWidth`、`heightFilterEnabled/minHeight/maxHeight` |
| 过滤 | `boundaryFilterEnabled/boundaryOverlapRatio`、`sortMode` |
| 显示 | `showBoxes`、`showLabels`、`showScores` |
| 预留位置修正 | `positionCorrectionEnabled`、`positionCorrectionSource` |

`judgeRule`：

- `mode`
- `resultBasis`
- `minCount/maxCount`
- `minScore`
- `category`

## 4. Adapter 固定值与环境覆盖

Adapter 当前把推理后端固定为红黑线模型：

- RK 模型：`/home/cat/model/yolov8_red_black.rknn`
- 标签：`/home/cat/红黑线标签.txt`
- 脚本：`/home/cat/qt-AI/scripts/run_rknn_demo.sh`
- 类别数：2

非空但无法识别的 `modelName` 只产生 warning，不会切换真实模型。

环境变量：

| 变量 | 作用 |
| --- | --- |
| `V2_AI_REMOTE_HOST` | 覆盖默认 RK 地址 |
| `V2_AI_REMOTE_USER` | 覆盖默认用户 |
| `V2_AI_ALLOW_LEGACY_RK_SCRIPT` | 是否允许 8 参数失败后回退 5 参数脚本 |
| `V2_AI_SSH_PASSWORD` | Runner 可尝试通过 `sshpass` 使用；缺少 `sshpass` 时退回 BatchMode |

其余目录、脚本和模型路径目前没有从 `ToolConfig` 开放。

## 5. Runner 执行流程

1. 校验输入图像。
2. 将 `roiNormalized` 转像素；非全图矩形 ROI 会先用 OpenCV 裁剪，只上传裁剪图。
3. 非矩形 `detectRegionType` 只产生 warning，仍按矩形包围 ROI/全图执行。
4. 在 `/tmp/v2_ai_bridge` 创建本地临时目录并保存 JPEG。
5. 通过 SSH 在远端创建输入、输出和构建目录。
6. SCP 上传输入图。
7. 首先用 8 参数调用远程脚本；失败且允许 fallback 时改用 legacy 5 参数。
8. 解析 stdout：
   - `DETECTION_COUNT=...`
   - `DETECTION_LINE=<class> @ (left top right bottom) score`
   - `RESULT_IMAGE=...`
9. 将裁剪图坐标加 ROI offset，恢复到原图像素和归一化坐标。
10. 本地过滤：
    - 类别过滤：已实现；
    - 宽度/高度过滤：已实现；
    - `maxDetections`：截断已实现；
    - 角度、边界、排序：未实现，只写 warning。
11. 本地判定：
    - count：数量范围；
    - score：最佳分不低于阈值；
    - category：至少存在指定类别。
12. 按配置生成框、标签、得分 overlay。
13. 若 stdout 给出远端结果图，尝试 SCP 回传；回传失败只警告，不改变推理成功结果。

## 6. NMS、阈值和脚本兼容

- `detectMinScore/confidenceThreshold` 转换为 0..1 的 `boxThreshold`。
- `maxOverlap/nmsIouThreshold` 也归一化为 0..1。
- Adapter 明确 warning：旧 `run_rknn_demo.sh` 没有 NMS 参数，因此 UI NMS 阈值不保证在远端生效。
- legacy 5 参数模式不传标签路径、类别数和 box threshold，结果依赖旧脚本默认值。

## 7. 输出合同

`ToolResult`：

- `success`：远端推理流程是否成功；
- `ok`：过滤后检测结果是否满足 judgeRule；
- `score`：过滤后最佳得分；
- `count`：过滤后检测数；
- overlays：检测框、类别和得分文字；
- payload：远端配置、ROI、输入/结果图路径、raw/filtered detections、stdout/stderr、各进程信息、warnings、参数快照、判断快照、耗时和脚本参数模式。

每个检测包含 `className`、`score`、`bboxPixel`、`bboxNormalized`。

## 8. 错误状态

- `empty_image`
- `local_temp_failed`
- `local_save_failed`
- `remote_mkdir_failed`
- `scp_input_failed`
- `remote_inference_timeout`
- `remote_inference_failed`
- Adapter 异常：`ai_detection_exception`

SCP 结果图失败不属于主链失败。

## 9. 当前真实边界与风险

- 这是固定红黑线 RKNN 桥接，不是通用 `modelName -> 模型仓库` 路由。
- UI 位置修正字段没有进入 Adapter/Runner。
- 只支持矩形 ROI；自由区域按钮明确未接入。
- 角度过滤、边界过滤、排序没有应用。
- NMS 是否应用取决于远端脚本，当前旧脚本不支持传参。
- 依赖外部 RK 主机、SSH/SCP、远端脚本、模型、标签和可执行环境；本机工程构建成功不代表远端推理可用。
- 临时输入/结果文件当前不会在 Runner 结束时自动清理。
- 密码方式依赖 `sshpass`；默认更适合已配置 SSH key 的 BatchMode。

## 10. 验证

已有 `ai_detection_bridge_smoke`。完整验收应分层：

1. 纯解析：合法/非法 `DETECTION_LINE`、ROI offset、归一化框。
2. 纯本地：类别、宽高、maxDetections、count/score/category 判定。
3. 远端：目录创建、上传、8 参数、5 参数 fallback、超时、脚本失败。
4. 结果图：有/无 `RESULT_IMAGE`、回传成功/失败。
5. UI：保存/回显全部字段，并明确未生效能力。
6. 安全：日志不泄露密码，远端参数正确 shell quote。

