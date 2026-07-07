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
|   +-- 模型训练卡片
|   |   +-- 导入模型
|   |   +-- 注册训练
|   |   +-- 模型管理
|   |   +-- 导出模型
|   |   +-- 删除模型
|   |
|   +-- 检测区域卡片
|   |   +-- 全屏
|   |   +-- 矩形 ROI
|   |   +-- 完成
|   |   +-- 独立位置修正使能 ⓘ
|   |   +-- 位置修正：1 基准图.位置修正信息
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

### 2026-07-03 联调阶段：测试图像传递与 UI 结果展示

#### 已实现功能

- `RegisteredClassificationDialog` 的“基准图测试”现在把 `ReferenceImageProvider::referenceFrame()` 同时写入 `ToolRequest.image` 和 `referenceImage`，避免后端误报 `image_empty`。
- “测试运行”现在从 `CameraFrameProvider::currentFrame()` 获取当前相机帧并写入 `ToolRequest.image`。
- 测试结果状态栏显示 OK/NG、预测类别、分数、TopK 和耗时；错误结果显示错误码、错误信息和耗时。
- 预览区使用 runner 返回的 overlay 绘制检测 ROI 和结果文本。
- 位置修正控件默认关闭并禁用，tooltip 明确说明当前版本尚未实现，避免误导用户。
- `RegisteredClassificationHalconRunner` 为结果文本 overlay 补充 `p1` 锚点和状态字段，确保 `FrameViewHelper` 可绘制文本。

#### 本次更改

- 修改 `src/RegisteredClassificationDialog.{h,cpp}`：测试帧来源、结果展示、overlay 展示、位置修正默认关闭/禁用。
- 修改 `src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp`：结果文本 overlay 增加锚点。
- 新增 `smoke/registered_classification_dialog_smoke.{cpp,pro}` 和测试专用 `registered_classification_dialog_plan_stub.cpp`，覆盖基准图/相机帧传入与位置修正默认关闭、禁用状态。

#### 验证结果

- `smoke/registered_classification_dialog_smoke` 覆盖基准图/测试运行传入 `request.image`，以及位置修正默认关闭和禁用状态。
- `smoke/registered_classification_adapter_smoke` 通过。
- 主工程 `qmake qt_ui_test.pro && make -j$(nproc)` 通过。

#### 剩余事项

- 仍需真实 HALCON DL 分类模型验证成功推理路径的 OK/NG、TopK 和 overlay 视觉效果。
- 连续运行、运行一次、退出测试等完整测试态尚未按颜色识别工具补齐。
- 位置修正真实补偿仍未实现；当前 UI 明确禁用。

### 2026-07-03 UI 调整：模型训练与检测区域换位

#### 本次更改

- `RegisteredClassificationDialog` 左侧参数区中，“模型训练”卡片调整到“检测区域”卡片上方。
- 更新 `registered_classification_dialog_smoke`，增加模型训练卡片位于检测区域卡片上方的断言。

#### 验证结果

- `smoke/registered_classification_dialog_smoke` 通过。

### 2026-07-03 公共位置修正占位 helper 接入

#### 已实现功能

- 新增 `src/toolcore/PositionCorrection.{h,cpp}`，沉淀 FID 公共位置修正占位能力。
- 公共 helper 支持从 params 解析 `enablePositionCorrection` / `positionCorrectionSource`，写回 params，并向 payload 写入 `positionCorrectionApplied=false` / `positionCorrectionReason="not implemented"`。
- 注册分类 Adapter 改为通过 `PositionCorrection::fromParams()` 解析位置修正配置。
- 注册分类 Runner 配置改为持有 `PositionCorrectionConfig`，错误和正常结果 payload 均通过 `PositionCorrection::writeNotAppliedPayload()` 写入统一字段。
- 注册分类 UI 保存 params 改为通过 `PositionCorrection::writeParams()` 写入位置修正字段，并清理重复 tooltip。

#### 本次更改

- 新增 `src/toolcore/PositionCorrection.h`、`src/toolcore/PositionCorrection.cpp`。
- 修改 `RegisteredClassificationDialog`、`RegisteredClassificationAdapter`、`RegisteredClassificationHalconRunner` 接入公共 helper。
- 更新 `qt_ui_test.pro`、`registered_classification_adapter_smoke.pro`、`registered_classification_dialog_smoke.pro` 链接新模块。
- 新增 `smoke/position_correction_smoke.{cpp,pro}` 覆盖公共 helper 的解析、写回和 payload 输出。

#### 验证结果

- `smoke/position_correction_smoke` 通过。
- `smoke/registered_classification_adapter_smoke` 通过。
- `smoke/registered_classification_dialog_smoke` 通过。
- 主工程 `qmake qt_ui_test.pro && make -j$(nproc)` 通过。

#### 剩余事项

- 有无类、颜色类等工具仍有各自的重复位置修正占位写法，后续可逐步迁移到 `PositionCorrection` 公共 helper。
- 真实位置补偿仍未实现；公共 helper 仅统一当前占位语义。

### 2026-07-03 UI 占位闭环：训练窗口、模型管理与全部参数调整

#### 已实现功能

- 新增 `RegisteredClassificationTrainingDialog` 占位窗口，`注册训练` 按钮可打开“注册分类”训练界面。
- 训练窗口按截图结构提供左侧注册图预览区、添加注册图、标注图像、类别列表、模型类型、任务类型、未注册状态和禁用的开始训练按钮。
- 新增 `RegisteredClassificationModelManagementDialog` 占位窗口，`模型管理` 按钮可打开“模型训练”管理界面。
- 模型管理窗口按截图结构提供数据集列表、创建/导入入口、模型列表、类型筛选、搜索框和提示文本。
- 主对话框“全部参数”卡片调整为“参数设置”，内容收敛为 `前K个类别` 和 `最小相似度`。
- `params.registeredClassification.minSimilarity` 默认保存为 `68`，`topK` 继续保存并回显。
- 结果判断新增 `判断类型` 下拉框，支持 `所有检测区域输出结果为 OK` 和 `任意检测区域输出结果为 OK`。
- `judgeRule.judgeType` 保存为 `all_ok` / `any_ok`，同时保存 `judgeTypeText` 供 UI 回显。

#### 本次更改

- 新增 `src/RegisteredClassificationTrainingDialog.{h,cpp}`。
- 新增 `src/RegisteredClassificationModelManagementDialog.{h,cpp}`。
- 修改 `src/RegisteredClassificationDialog.{h,cpp}`，接入两个占位窗口并调整参数/判断字段。
- 更新 `qt_ui_test.pro` 和 `smoke/registered_classification_dialog_smoke.pro` 链接新窗口。
- 更新 `smoke/registered_classification_dialog_smoke.cpp`，覆盖参数设置字段、判断类型默认值和两个窗口入口。

#### 验证结果

- `smoke/registered_classification_dialog_smoke` 通过，覆盖新字段默认保存、窗口可打开和既有基准图/测试运行路径。

#### 剩余事项

- 注册训练窗口当前只做 UI 占位，不接入本地标注、数据集落盘或 HALCON 训练流程。
- 模型管理窗口当前只做 UI 占位，不接入真实模型仓库、重新训练、导出或删除逻辑。
- `minSimilarity` 当前进入配置保存/回显链路，后续若要参与判定，需要同步扩展 Adapter/Runner 的多区域或相似度判定语义。

### 2026-07-03 UI 调整：训练/模型管理窗口尺寸与可读性

#### 本次更改

- `RegisteredClassificationTrainingDialog` 和 `RegisteredClassificationModelManagementDialog` 初始尺寸改为父窗口约 `76%`，无父窗口时保留默认兜底尺寸。
- 两个窗口统一提升标题、标签、按钮和表格字号，增加蓝色/橙色高对比边框和按钮状态，避免浅灰样式导致人眼难以识别。
- 模型管理窗口的数据集列表去掉“生产样本”和“验证样本”，仅保留“默认数据集”占位。
- 模型管理窗口的“创建数据集”和“导入”按钮移动到数据集列表标题栏右上角。

#### 验证结果

- `smoke/registered_classification_dialog_smoke` 通过，覆盖两个子窗口 70%-80% 初始尺寸、数据集裁剪和数据集操作按钮标题栏位置。

### 2026-07-03 UI 调整：数据集创建弹窗与高对比样式

#### 本次更改

- 模型管理窗口的“创建数据集”和“导入”改为带图标按钮，不再依赖文本或临时符号表达动作。
- 模型列表行的编辑、导出、删除动作改为标准图标按钮，去掉 `...` / `⇩` / `×` 文本符号样式。
- 新增“创建数据集”弹窗，占位复刻数据集名称、训练类型和确定/取消流程。
- “创建数据集”弹窗点击“确定”后关闭模型管理窗口，并进入注册训练窗口。
- 注册训练窗口和模型管理窗口继续提升字体大小，并将蓝/灰重底色改为白底、深色文字、橙色强调线，提升可读性。

#### 验证结果

- `smoke/registered_classification_dialog_smoke` 通过，覆盖创建/导入图标、创建数据集弹窗打开、确认后进入注册训练窗口。

### 2026-07-03 UI 调整：模型列表动作图标与提示

#### 本次更改

- 模型列表每行的三个操作按钮改为语义化自绘图标：
  - 重命名：文档 + 笔形图标。
  - 导出：向下箭头 + 托盘图标。
  - 删除：红色圆形叉号图标。
- 每个操作按钮增加 tooltip，鼠标悬停分别显示 `重命名`、`导出`、`删除`。
- 为按钮增加稳定 objectName，便于后续自动化测试和交互接线。

#### 验证结果

- `smoke/registered_classification_dialog_smoke` 通过，覆盖三个模型操作按钮 tooltip。

### 2026-07-03 注册训练窗口一阶段：抓图、导入和 ROI 绘制

#### 已实现功能

- `注册训练`窗口接入左侧预览区的 `FrameViewHelper`，用于显示注册图像和绘制 ROI。
- `相机抓图` 从 `CameraFrameProvider::currentFrame()` 获取当前帧并显示到左侧预览区。
- `外部导入` 打开图片文件选择框，仅允许选择图片文件，导入后显示到左侧预览区。
- `存图导入` 保持占位禁用，并用 tooltip 明确暂未接入。
- `全屏框选`、`矩形框选`、`多边形框选` 改为带语义图标的 `QToolButton`，并提供 tooltip。
- ROI 按钮手动互斥：点击高亮并进入对应 ROI 模式，再次点击同一高亮按钮退出绘制状态。
- 只有 `矩形框选` 高亮时启用矩形 ROI 绘制；只有 `多边形框选` 高亮时启用多边形 ROI 绘制；`全屏框选` 高亮时显示全屏 ROI。
- 矩形 ROI 和多边形 ROI 当前只做 UI 交互和预览显示，不写入真实训练样本或数据集。

#### 本次更改

- 修改 `src/RegisteredClassificationTrainingDialog.cpp`，接入图像显示、相机抓图、图片导入、ROI 按钮图标、互斥高亮和绘制状态。
- 更新 `smoke/registered_classification_dialog_smoke.cpp`，覆盖相机抓图显示、ROI 图标/tooltip、互斥切换和二次点击退出。

#### 验证结果

- `smoke/registered_classification_dialog_smoke` 通过。

#### 剩余事项

- 外部导入当前只读取本地图片并显示，不保存到数据集。
- 矩形/多边形 ROI 当前只显示和更新训练窗口状态，不保存到真实样本标注。
- 存图导入仍为占位禁用。
- 真实训练、数据集落盘、类别标注和模型生成仍未接入。

### 2026-07-07 注册训练窗口二阶段：缩略图、分类列表和 ROI 会话缓存

#### 已实现功能

- 注册训练窗口左侧预览区底部新增注册图缩略图列表，`相机抓图` 会加入缩略图并选中当前图。
- 训练窗口维护当前会话内的注册图列表、类别列表和 ROI 缓存；这些数据只用于 UI 交互，不落盘、不生成训练数据集。
- 右侧 `2/ 标注图像` 改为分类列表结构，显示 `分类列表(N)`、类别名称、`目标总数/图像总数` 和行内语义图标按钮。
- 分类行操作提供 `重命名`、`预览类别 ROI`、`删除当前 ROI` tooltip；不再使用 `...`、`◉`、`×` 作为最终操作表达。
- `+ 新建` 可在当前会话追加类别，并为新增类别生成对应预览/删除操作。
- 矩形 ROI 和多边形 ROI 完成后，会更新当前图片的已标注状态、类别统计和左侧缩略图文本。
- 点击类别预览按钮会在左侧大图显示当前类别的 ROI；删除当前 ROI 和清除全部标注会更新缩略图和统计。
- 无图片时 ROI 按钮不会产生有效标注，并在状态栏提示先添加注册图。
- `相机抓图` 在当前相机帧为空但基准图存在时，会自动取基准图加入注册图列表并显示，状态栏提示 `当前图像帧为空，已获取基准图`。
- 分类列表改为浅色高对比样式，避免深色底和字体颜色不符合项目控件规范。
- 分类行 `重命名` 按钮接入重命名弹窗，可修改当前会话内类别名称。
- 类别 ROI 预览时状态栏明确显示 `正在预览类别 ROI：<类别名>`。
- 注册图像工具栏新增 `上一张注册图` / `下一张注册图` 按钮，按循环列表语义切换当前缩略图和大图。

#### 本次更改

- 修改 `src/RegisteredClassificationTrainingDialog.cpp`，新增训练窗口内会话状态、缩略图列表、分类列表、类别操作按钮和 ROI 缓存刷新逻辑。
- 修改 `smoke/registered_classification_dialog_smoke.cpp`，覆盖缩略图生成、分类列表控件、ROI 标注状态、类别预览、删除当前 ROI、清除全部标注和新建类别。
- 根据截图反馈补充当前帧为空取基准图、分类列表浅色规范样式、类别重命名、类别预览状态文案和注册图循环切换。
- 新增 `docs/superpowers/plans/2026-07-07-registered-classification-training-window-ui.md` 记录本次实施计划。

#### 验证结果

- `smoke/registered_classification_dialog_smoke` 通过，覆盖训练窗口二阶段关键控件和交互、当前帧为空取基准图、类别重命名以及注册图循环切换路径。

#### 剩余事项

- 外部导入仍依赖手动文件选择，当前 smoke 未覆盖真实文件导入路径。
- 当前 ROI、类别和缩略图状态只保存在窗口会话内，不保存为真实数据集。
- 真实 HALCON 训练、模型生成、数据集落盘和模型管理真实能力仍未接入。

### 2026-07-07 注册训练窗口三阶段：ROI 预览页和多 ROI 会话标注

#### 已实现功能

- 注册训练窗口的会话标注模型由“每图每类单 ROI”升级为“每图每类多 ROI 列表”，矩形、全屏和多边形标注都会追加到当前类别。
- 左侧大图页显示当前图片、当前类别下的全部 ROI；ROI 左上角显示序号，最新 ROI 仍保留在 `FrameViewHelper` 的可编辑 ROI 状态中。
- 左侧预览区新增 ROI 预览页，点击类别行的预览按钮后显示该类别所有 ROI 的裁剪预览卡片，标题显示 `<类别名> | 已标注目标：N`。
- 预览页支持关闭按钮返回大图；再次点击同一个类别的预览按钮也会返回大图。
- 预览页 ROI 卡片支持点击选中；右键菜单提供 `删除当前 ROI`，只删除被选中的单个 ROI 并刷新卡片、统计和缩略图状态。
- 编辑当前多边形 ROI 时更新当前 ROI，不重复追加为新的 ROI。
- 预览页打开时进行类别重命名或新建类别，会返回大图页，避免左侧预览内容和右侧当前类别状态不一致。
- 分类行删除按钮语义调整为删除类别：多类别时删除该类别并移除对应 ROI 标注，剩余类别索引同步重排；只剩一个类别时保留该类别，但清空其 ROI 标注。

#### 本次更改

- 修改 `src/RegisteredClassificationTrainingDialog.cpp`，新增多 ROI 会话结构、左侧 `QStackedWidget` 预览页、ROI 裁剪卡片、右键删除单 ROI、类别删除和多 ROI 大图 overlay 显示。
- 修改 `smoke/registered_classification_dialog_smoke.cpp`，覆盖多 ROI 序号、预览页切换/关闭、ROI 卡片、右键删除单 ROI、删除类别和最后类别保留清标注语义。

#### 验证结果

- `smoke/registered_classification_dialog_smoke` 通过。
- 影子目录执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro && make -j8` 通过。
- `git diff --check` 通过。

#### 剩余事项

- 大图 overlay 的点击选中 ROI 暂未实现，仍作为后续优化；当前只实现预览页卡片选中和右键删除单个 ROI。
- ROI、类别和预览卡片仍为窗口会话内状态，不落盘、不生成真实数据集。
- 真实 HALCON 训练、模型生成、数据集落盘和 `.scbin` 支持仍未接入。
