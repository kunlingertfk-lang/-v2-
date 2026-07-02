# 注册分类功能实现记录

## 文档用途

本文档单独记录注册分类算子的需求、HALCON 实现方案、阶段边界、接口字段、验证要求和后续实现记录。后续继续开发注册分类时，以根目录 `AGENTS.md` 的项目约束和 HALCON 约束为最高规则，以 `docs/FID/Function_Docs.md` 作为 FID 公共规范，以本文档作为注册分类功能状态追踪依据。

## 需求来源

注册分类参考海康官方功能描述：

- 根据已注册图像类别对图像分类。
- 支持设置检测区域，默认全屏，可绘制矩形检测区域。
- 支持独立位置修正使能，默认开启，可订阅基准图或位置修正工具信息。
- 支持模型导入、注册训练、模型管理、导出和删除。
- 支持类别判断和最低得分两种结果判断。
- 测试运行时实时采集图像并输出分类结果。

当前项目不能直接照搬海康实现。所有视觉核心算法必须基于 HALCON 已有算子、类或过程实现；海康 `.scbin` 作为专有格式，第一版不解析、不伪装兼容。

## 第一版目标

注册分类第一版实现“推理闭环”：

- 工具库入口可见，能创建注册分类工具。
- 配置对话框能保存、重新打开并回显模型、检测 ROI、位置修正占位和结果判断。
- 模型导入仅支持本机 HALCON 确认可读的分类模型。
- `.scbin` 导入返回 `unsupported_model_format`。
- 测试运行时使用 HALCON 执行分类推理。
- UI 显示 OK/NG、预测类别、得分、TopK 类别、ROI、耗时和错误状态。
- 空图像、无模型、模型路径不存在、模型格式不支持、无效 ROI、HALCON runtime/license/符号缺失时不崩溃，并返回明确错误。

第一版暂不实现：

- 注册图像窗口。
- 本地数据集管理。
- 图像标注。
- 本地训练。
- 模型管理窗口的真实训练能力。
- 海康 `.scbin` 读取。
- 海康 `OLClassify*.bin` 底层模型接入。
- 位置修正真实补偿。

## 总体链路

```text
ToolLibraryDialog / ToolsDialog
        |
        v
RegisteredClassificationDialog
        |
        v
ToolConfig.params / judgeRule
        |
        v
RegisteredClassificationAdapter
        |
        v
RegisteredClassificationHalconRunner
        |
        v
ToolResult / overlays / payload
```

第一版按独立注册分类工具链实现，不复用 AI 检测桥接链路，不把颜色识别、颜色比较或模板匹配作为 fallback。

## UI 和后端分层边界

注册分类第一版应按 UI、后端、联调三段推进，避免一个任务同时修改界面、配置、Adapter、Runner 和 HALCON 推理细节。

UI 阶段：

- 负责工具入口、配置对话框、基础/全部页、模型路径管理、检测 ROI、位置修正控件、结果判断控件、保存和回显。
- 可以提供测试运行入口，但后端未完成时只能返回或展示 `backend_not_implemented`。
- 不得伪造分类结果，不得调用颜色识别、颜色比较、AI 检测或其他替代算法。
- `.scbin` 导入必须在 UI 阶段就明确提示不支持。

后端阶段：

- 负责 `RegisteredClassificationAdapter`、`RegisteredClassificationHalconRunner`、HALCON 模型读取、分类推理、错误码、payload、overlay 和 smoke 测试。
- 只通过 `ToolConfig.params.registeredClassification`、`judgeRule`、`ToolRequest` 和 `ToolResult` 与 UI 对接。
- 不重做 UI 布局，不新增训练/模型管理真实能力。
- 不改变 `.scbin` 不支持策略。

联调阶段：

- 负责把 UI 测试运行、基准图测试、连续运行、运行一次、退出测试接到真实 Adapter/Runner。
- 负责工具页预览、状态栏、OK/NG、类别、得分、ROI 和耗时显示。
- 负责验证异常输入和 HALCON 异常能从后端传到 UI。

## UI 方案

`RegisteredClassificationDialog` 参考现有颜色识别、颜色比较对话框：

```text
RegisteredClassificationDialog
|
+-- 顶部标题栏
|
+-- 左侧参数区
|   +-- 基础 / 全部 分段按钮
|   |
|   +-- 检测区域卡片
|   |   +-- 全屏
|   |   +-- 矩形 ROI
|   |   +-- 完成
|   |
|   +-- 位置修正卡片
|   |   +-- 独立位置修正使能 ⓘ
|   |   +-- 位置修正：1 基准图.位置修正信息
|   |
|   +-- 模型训练卡片
|   |   +-- 导入模型
|   |   +-- 注册训练
|   |   +-- 模型管理
|   |   +-- 导出模型
|   |   +-- 删除模型
|   |
|   +-- 全部页扩展参数
|   |   +-- 模型类型
|   |   +-- TopK
|   |   +-- HALCON runtime 信息
|   |
|   +-- 结果判断卡片
|       +-- 类别判断
|       +-- 最低得分
|
+-- 右侧图像预览区
|   +-- 基准图 / 当前图像
|   +-- 检测 ROI / 结果 overlay
|   +-- 底部状态栏
|
+-- 底部按钮
    +-- 基准图测试 / 测试运行 / 完成
    +-- 测试态：停止运行 / 运行一次 / 退出测试
```

交互约束：

- `注册训练`、`模型管理` 第一版若显示入口，点击必须提示未实现。
- 普通按钮不得误触发 `accept`、`reject` 或程序退出。
- ROI 编辑使用单一状态，第一版只实现 `Full` 和 `DetectRect`。
- 未实现的自由绘制、圆形 ROI、屏蔽区域不得写入假参数。

## 配置字段

通过 `ToolConfig.params.registeredClassification` 和 `judgeRule` 保存配置：

```text
ToolConfig
|
+-- toolType = RegisteredClassification
+-- category = Recognition
+-- roiNormalized
|   +-- 检测矩形 ROI；全屏为 (0, 0, 1, 1)
|
+-- params
|   +-- registeredClassification
|       +-- version = 1
|       +-- modelPath
|       +-- modelName
|       +-- modelType = "halcon_dl_classification"
|       +-- detectRegionType = "full" | "rectangle"
|       +-- roiNormalized
|       +-- enablePositionCorrection = true
|       +-- positionCorrectionSource = "1 基准图.位置修正信息"
|       +-- topK = 1
|       +-- halconSoPath
|
+-- judgeRule
    +-- mode = "class_match" | "min_score"
    +-- expectedLabel
    +-- minScore
```

默认值：

- `version=1`
- `modelType=halcon_dl_classification`
- `detectRegionType=full`
- `roiNormalized=(0, 0, 1, 1)`
- `enablePositionCorrection=true`
- `positionCorrectionSource=1 基准图.位置修正信息`
- `topK=1`
- `judgeRule.mode=class_match`
- `judgeRule.minScore=80`

旧配置缺少字段时必须按以上默认值回退。保存和回显必须一致。

## HALCON 算法方案

第一优先方案是 HALCON DL 分类推理链：

- `T_read_dl_model`：读取 HALCON DL 分类模型。
- `T_apply_dl_model`：执行分类推理。
- `T_set_dl_model_param`：设置运行设备、batch 或推理参数。
- `T_get_dl_model_param`：读取模型输入尺寸、类别名、输出层等参数。
- `T_create_dict`：创建 DLSample / 参数字典。
- `T_set_dict_object`：写入图像对象。
- `T_set_dict_tuple`：写入 tuple 参数。
- `T_get_dict_tuple`：读取分类结果。
- `T_clear_dl_model` / `T_clear_handle`：释放模型和字典句柄。
- `gen_image_interleaved`：将工程侧 `cv::Mat` 桥接为 HALCON image。
- `gen_rectangle1` / `reduce_domain`：限制检测 ROI。
- `clear_obj`：释放 HALCON object。

备选经典分类链仅用于 HALCON 经典分类模型：

- `read_class_knn` / `read_class_svm` / `read_class_mlp`
- `T_classify_class_knn` / `T_classify_class_svm` / `T_classify_class_mlp`

备选链不能作为 `.scbin` 的替代解析方案，也不能绕过 HALCON 能力确认。

## 模型格式策略

第一版模型格式策略：

- `.scbin`：返回 `unsupported_model_format`。
- 不符合英文大小写字母、数字、下划线命名规则的模型文件：返回 `invalid_model_name`。
- 文件不存在或不可读：返回 `model_file_not_found`。
- HALCON runtime 不存在：返回 `halcon_so_not_found`。
- HALCON 库加载失败：返回 `halcon_load_failed`。
- HALCON 必要符号缺失：返回 `halcon_symbol_missing`。
- HALCON 读取模型失败：返回 `model_load_failed`。

可支持的模型后缀必须以本机 HALCON 实测可读为准。实现前应在功能计划中列出已确认后缀和 HALCON 读取接口。

## 结果判断

类别判断：

- `judgeRule.mode = "class_match"`。
- `predictedLabel == expectedLabel` 时 OK，否则 NG。
- `expectedLabel` 为空返回 `missing_expected_label`。

最低得分：

- `judgeRule.mode = "min_score"`。
- `score >= minScore` 时 OK，否则 NG。
- `minScore` 范围为 0-100。

输出分数统一为 0-100，分数越高表示分类置信度越高。

## payload 和 overlay

`ToolResult.payload` 至少输出：

- `predictedLabel`
- `predictedClassId`
- `score`
- `topClasses`
- `modelPath`
- `modelName`
- `modelType`
- `detectRegionType`
- `roiPixelsRect`
- `elapsedMs`
- `judgeMode`
- `expectedLabel`
- `minScore`
- `enablePositionCorrection`
- `positionCorrectionSource`
- `positionCorrectionApplied`
- `positionCorrectionReason`
- `errorCode`
- `errorMessage`

Overlay 至少输出：

- 检测 ROI 矩形。
- OK/NG 文本。
- 预测类别和得分文本。

位置修正第一版只做占位：

```text
positionCorrectionApplied = false
positionCorrectionReason = "not implemented"
```

不允许在 UI 开启位置修正后静默忽略，也不允许假报已应用。

## 验证要求

涉及 Qt 工程或 UI 接入时执行：

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro
make -j8
git diff --check
```

涉及 runner 或 adapter 时新增或运行注册分类 smoke 测试，至少覆盖：

- 正常模型推理成功路径。
- 空图像。
- 无模型或模型路径不存在。
- 不支持 `.scbin`。
- 无效 ROI。
- 类别判断 OK/NG。
- 最低得分 OK/NG。
- HALCON runtime、license 或符号缺失。

手动验证：

- 工具库入口可见。
- 新建、保存、重新打开回显。
- 导入模型名称校验只允许英文大小写、数字和下划线。
- ROI 绘制和取消/完成按钮不误触发窗口关闭。
- 测试运行显示 OK/NG、类别、得分、ROI、耗时。

## 当前状态

截至本文档创建时：

- 已完成注册分类第一版方案和提示词规范。
- 已实现 UI 第一阶段：
  - `RegisteredClassificationDialog` 代码构建界面。
  - 工具库“注册分类”入口。
  - `ToolsDialog` / `MainWindow` 新建和编辑入口。
  - `ToolType::RegisteredClassification` 字符串映射。
  - `RegisteredClassificationAdapter` 占位端口，固定返回 `backend_not_implemented`。
  - 注册分类占位 adapter smoke 测试。
- 尚未实现真实 HALCON Runner 和模型推理。
- `.scbin` 明确列为第一版不支持格式。
- 注册训练、模型管理、底层模型选择和真实位置修正列为后续阶段。

后续每次修改注册分类功能后，应在本文档继续追加：

- 已实现功能。
- 本次更改。
- 出现的问题与处理。
- 验证结果。
- 剩余事项。

## 实现记录

### 2026-07-02 后端阶段：HALCON DL 分类推理闭环

#### 已实现功能

- 注册分类后端从占位 `backend_not_implemented` 升级为真实 HALCON DL 分类推理链。
- `RegisteredClassificationHalconRunner` 实现：cv::Mat→HALCON image 桥接 → `T_read_dl_model` 读模型 → `gen_rectangle1`/`reduce_domain` 限制检测 ROI → `T_create_dict`/`T_set_dict_object` 构造 DLSample → `T_apply_dl_model` 推理 → `T_get_dict_tuple` 解析 `classification_classes`/`classification_confidences` → topK 排序 → 判别（class_match / min_score）。
- 输入校验返回明确错误码：`image_empty` / `no_model` / `invalid_model_name` / `unsupported_model_format`（.scbin）/ `model_file_not_found` / `invalid_roi` / `missing_expected_label` / `halcon_so_not_found` / `halcon_load_failed` / `halcon_symbol_missing` / `model_load_failed`（read_dl_model 失败）/ `inference_result_missing` / `halcon_error` / `exception`。
- payload 输出按文档字段表：`predictedLabel`/`predictedClassId`/`score`(0-100)/`topClasses`/`modelPath`/`modelName`/`modelType`/`detectRegionType`/`roiPixelsRect`/`elapsedMs`/`judgeMode`/`expectedLabel`/`minScore`/`enablePositionCorrection`/`positionCorrectionSource`/`positionCorrectionApplied=false`/`positionCorrectionReason="not implemented"`/`errorCode`/`errorMessage`。
- overlay 输出检测 ROI 矩形 + OK/NG 文本（含预测类别与得分）。
- 位置修正保持占位，不假报已应用。
- HALCON runtime 走 `HalconRuntimePaths::resolveHalconLibPath`，符号走 `dlsym` 动态加载，镜像 `ColorRecognitionHalconRunner` 模式。

#### 本次更改

- 新增 `src/algorithms/recognition/RegisteredClassificationHalconRunner.h` —— `RegisteredClassificationHalconConfig` + `RegisteredClassificationHalconResult` + `RegisteredClassificationClassScore` + `run()` 声明。
- 新增 `src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp` —— 自包含 HALCON C API 动态加载设施（`HalconCApi`/`HalconLibrary`/`HalconTuple`/`DictHandle`/`DlModelHandle`）+ DL 推理实现。
- 修改 `src/tooladapters/RegisteredClassificationAdapter.{h,cpp}` —— `run()` 从占位改为解析 `params.registeredClassification`/`judgeRule` → `RegisteredClassificationHalconConfig` → 调 Runner → 转 `ToolResult`；新增 `intParam`/`rectFromJson` 工具函数。
- 修改 `qt_ui_test.pro` —— SOURCES/HEADERS 加新 Runner 文件。
- 修改 `smoke/registered_classification_adapter_smoke.{pro,cpp}` —— .pro 加 Runner/HalconRuntimePaths/HALCON include/`-ldl`/opencv imgproc；.cpp 从"断言 backend_not_implemented"改为覆盖 9 个异常路径断言。

#### 出现的问题与处理

- 问题：`DictHandle`/`DlModelHandle` 析构调 `T_clear_handle`/`T_clear_dl_model` 时传 `&m_handle`（Htuple*），但这两个算子签名是按值 `const Htuple`，编译报 `could not convert Htuple* to Htuple`。
  处理：析构改为传 `m_handle`（按值），`ptr()` 仍返回 `Htuple*` 供 `create_dict`/`read_dl_model` 等输出参数使用。
- 问题：smoke .pro 缺 HALCON include 路径与 `-ldl`/opencv imgproc，导致 `HalconC.h` 找不到、`dlsym`/`cv::cvtColor` 链接失败。
  处理：参照 `color_comparison_smoke.pro` 补 `HALCON_ROOT` include、`-ldl`、`-lopencv_imgproc`。

#### 验证结果

- 主工程影子构建：`/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j$(nproc)` 通过（Runner + Adapter 编译链接，无 error/undefined）。
- smoke 异常路径：`cd smoke && qmake registered_classification_adapter_smoke.pro && make && ./registered_classification_adapter_smoke` 全部断言通过，覆盖 image_empty / no_model / unsupported_model_format(.scbin) / invalid_model_name / model_file_not_found / invalid_roi / missing_expected_label / 非模型文件(model_load_failed|halcon_error|exception) / 位置修正占位 payload 字段完整性。
  - 注意：`HalconRuntimePaths::resolveHalconLibPath` 对错误 so 路径会容错回退到默认 HALCON 库，故无法在本地构造 `halcon_so_not_found`；非模型文件会走到 `T_read_dl_model` 失败，返回 model_load_failed / halcon_error / exception，已纳入断言。
- 成功推理路径：暂无 HALCON DL 模型文件可验证，留待后续。

#### 剩余事项

- 待有 HALCON DL 分类模型（.hdl 等 HALCON 可读格式）后，补成功推理路径 smoke 与手动 UI 验证（OK/NG、类别、得分、TopK、ROI、耗时显示）。
- 注册训练窗口、模型管理真实训练、本地数据集/标注仍为后续阶段。
- 海康 `.scbin`/`OLClassify*.bin` 第一版不支持，保持现状。
- 位置修正真实补偿仍为占位。
- 经典分类备选链（`read_class_mlp/svm/knn`）未实现，文档已列为备选。
