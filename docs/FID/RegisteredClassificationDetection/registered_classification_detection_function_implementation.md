# 注册目标检测功能实现记录

## 文档用途

本文档记录“注册目标检测”工具的阶段边界、实现状态、配置字段和验证要求。内部代码和配置命名暂沿用 `RegisteredClassificationDetection` / `registeredClassificationDetection`，用于保持既有配置兼容；用户可见文案统一为 `注册目标检测`。后续继续开发时，以根目录 `AGENTS.md` 为最高约束，以 `docs/FID/Function_Docs.md` 为 FID 公共规范，以 `docs/FID/RegisteredClassificationDetection/注册分类检测提示词规范.md` 为提示词入口。

## 第一阶段目标

注册目标检测第一阶段实现独立 UI 复刻闭环：

- 工具库入口显示 `注册目标检测`。
- 新增 `ToolType::RegisteredClassificationDetection` 和字符串映射。
- 新增 `RegisteredClassificationDetectionDialog`。
- 主对话框复刻现有注册分类配置 UI。
- 配置保存到 `params.registeredClassificationDetection`。
- 支持基础/全部模式、模型路径字段、全屏/矩形 ROI、位置修正占位、TopK、最小相似度和结果判断字段。
- 后端未接入时测试运行返回 `backend_not_implemented`。
- `注册训练` 打开独立 `RegisteredClassificationDetectionTrainingDialog`，窗口标题为 `注册目标检测`。
- `模型管理` 暂时复用现有注册分类模型管理窗口作为占位入口。
- 训练窗口支持会话内添加注册图、矩形/多边形目标标注、ROI 编号标注、目标计数、目标 ROI 预览和检测专属训练参数开关。
- 训练窗口右侧参数区保持紧凑，`添加注册图` 与检测专属参数卡片压缩高度，`目标列表` 下方不保留深色大块空白，参数卡片下方可保留空白。
- 训练窗口缩略图区支持 `全部 / 标注 / 未标注` 筛选；缩略图支持单图删除，缩略图区支持删除全部注册图并二次确认。

## 当前不实现

- 不接入真实 HALCON 推理。
- 不新增 Adapter 或 Runner。
- 不实现真实训练、真实数据集落盘或模型生成。
- 不支持 `.scbin`。
- 不修改现有注册分类工具行为。
- 不把注册分类训练窗口改造成多模式窗口。

## 配置链路

```text
ToolLibraryDialog / ToolsDialog / MainWindow
        |
        v
RegisteredClassificationDetectionDialog
        |
        +--> RegisteredClassificationDetectionTrainingDialog
        |
        v
ToolConfig.params.registeredClassificationDetection / judgeRule
        |
        v
backend_not_implemented 占位结果
```

## 验证

第一阶段验证项：

- 工具库能选择 `注册目标检测`。
- 对话框能保存 `ToolType::RegisteredClassificationDetection`。
- 默认配置保存 `detectRegionType=full`、`minSimilarity=68`、`enablePositionCorrection=false`、`judgeType=all_ok`。
- 矩形 ROI 模式保存和回显为 `detectRegionType=rectangle`。
- 空相机帧显示 `image_empty`。
- 有图像但无后端时显示 `backend_not_implemented`。
- 注册训练按钮能打开独立 `注册目标检测` 窗口，不触发主对话框关闭。
- 训练窗口不显示 `全屏框选`，只显示 `矩形框选`、`多边形框选`。
- 训练窗口默认目标为 `Target`，不显示 `+ 新建` 和删除目标按钮。
- 训练窗口支持相机抓图添加注册图、绘制矩形 ROI 后目标计数更新为 `1 / 1`、目标 ROI 预览。
- 训练窗口绘制 ROI 后在图像预览区显示与注册分类训练窗口一致的 ROI 编号标签。
- 训练窗口开始训练占位校验：无图提示 `请先添加注册图`，有图无标注提示 `请先标注目标`，有图有标注提示 `训练算法未接入，当前仅完成 UI 标注闭环。`
- 模型管理按钮能打开现有占位窗口，不触发主对话框关闭。

验证命令：

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_detection_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_detection_dialog/bin/registered_classification_detection_dialog_smoke
```
