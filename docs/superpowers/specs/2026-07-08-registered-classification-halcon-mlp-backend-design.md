# 注册分类 HALCON MLP 后端算法设计

## 背景

当前注册分类后端文档和代码曾按 `halcon_dl_classification` 方向组织：导入 HALCON DL 分类模型，运行时读取模型并输出分类结果。该路线更接近通用图像分类模型导入，不符合本轮对 SCMVS “注册分类”语义的复现目标。

本设计将注册分类后端硬替换为 HALCON 经典分类链路：训练窗口中用户注册图像、类别和 ROI；后端从每个 ROI 中提取稳定特征；使用 HALCON MLP 分类器训练和推理；运行时输出类别、置信度和 OK/NG。旧 DL 模型读取和推理不再作为兼容分支保留。

本设计只覆盖注册分类本身。模板匹配、形状模型定位、姿态归一化和真实位置修正属于独立的位置修正能力，不进入本注册分类后端算法。

## 目标

- 用 HALCON 经典 MLP 分类器替换现有 DL 分类后端主线。
- 训练阶段从注册图、类别和 ROI 生成可保存的注册分类模型包。
- 推理阶段在检测 ROI 内提取同版本特征，并调用 HALCON MLP 输出 TopK 类别和置信度。
- Adapter、Runner、模型包、payload 和错误码围绕 MLP 分类语义重新收敛。
- 异常输入和 HALCON 环境异常必须返回明确状态，不崩溃、不静默降级。

## 非目标

- 不保留 `halcon_dl_classification` 模型导入和 DL 推理兼容分支。
- 不解析海康 `.scbin`，不伪装兼容 SCMVS 专有模型格式。
- 不在注册分类后端实现模板匹配、`create_shape_model`、`find_shape_model` 或姿态对齐。
- 不实现位置修正算法。若未来有位置修正输出，可作为外部 ROI/坐标补偿输入接入。
- 不引入 OpenCV、自写分类器、ONNX Runtime、TensorRT 或其他非 HALCON 核心分类依赖。
- 不把训练窗口 UI 的交互完善和算法训练落地混在同一个实现任务里。

## 算法主线

注册分类后端由两条闭环组成：

```text
训练闭环:
注册图 + 类别表 + ROI 标注
        |
        v
HALCON 图像与 ROI 域
        |
        v
预处理 + 特征提取
        |
        v
create_class_mlp / add_sample_class_mlp / train_class_mlp
        |
        v
write_class_mlp + 模型元数据 JSON

推理闭环:
ToolRequest.image + ToolConfig 检测 ROI
        |
        v
HALCON 图像与检测域
        |
        v
同版本预处理 + 特征提取
        |
        v
read_class_mlp / classify_class_mlp
        |
        v
ToolResult: OK/NG、类别、得分、TopK、ROI、耗时、错误状态
```

这里的“注册”只表示用户把样本 ROI 注册到某个类别下，形成训练样本。它不表示在线阶段先做模板定位或姿态注册。

## HALCON 算子边界

核心分类算子：

- `create_class_mlp`：创建 MLP 分类器，输出函数使用 `softmax`。
- `add_sample_class_mlp`：把单个 ROI 特征向量和类别目标加入训练样本。
- `train_class_mlp`：训练 MLP。
- `classify_class_mlp`：对运行时特征向量输出 TopK 类别和置信度。
- `read_class_mlp` / `write_class_mlp`：读取和保存 MLP 分类器。
- `clear_class_mlp` 或对应 handle 清理接口：释放分类器资源。

训练数据辅助算子可作为实现备选：

- `create_class_train_data`
- `add_sample_class_train_data`
- `set_feature_lengths_class_train_data`

这些辅助算子可用于保存、检查或筛选特征训练数据，但第一版不要求做自动特征选择。

图像、ROI 和特征相关算子：

- `gen_image_interleaved`：把工程侧 `cv::Mat` 桥接为 HALCON image。
- `rgb1_to_gray` 或等价灰度化链路：统一输入通道。
- `gen_rectangle1` / 多边形区域生成算子：由训练 ROI 或检测 ROI 生成 HALCON region。
- `reduce_domain` / `crop_domain`：限制特征提取域。
- `mean_image`、`gauss_filter`、`scale_image_max`、`emphasize`：可配置预处理。
- `threshold`、`connection`、`select_shape`：需要形状特征时生成稳定前景区域。
- `area_center`、`moments_region_2nd`、`moments_region_2nd_invar`、`intensity`、`gray_histo`：提取形状和灰度统计特征。

实现前必须按项目 HALCON 红线确认本机 HALCON C API 符号存在。缺少核心分类符号时停止实现并返回阻塞，不允许改用 OpenCV 或自写分类器。

## 特征设计

第一版采用固定长度、低维、可解释的特征向量，避免一次性做复杂自动特征工程。特征版本写入模型元数据，训练和推理必须完全一致。

推荐第一版特征组：

- ROI 几何：宽高比、ROI 面积占检测图比例。
- 前景形状：前景面积、重心归一化位置、二阶矩或不变矩、连通域数量。
- 灰度统计：平均灰度、最小/最大灰度、标准差。
- 灰度直方图：固定 bin 数，例如 16 bin，并归一化为比例。

第一版建议 `featureVersion=halcon_mlp_roi_stats_v1`。一旦后续增删特征或调整 bin 数，必须升级 `featureVersion`，旧模型不得被新 runner 悄悄按新特征解释。

特征值需要做归一化。优先使用 HALCON MLP 的 `Preprocessing='normalization'`，同时在元数据中记录每个特征的名称、长度和取值含义，便于排查训练质量问题。

## 训练阶段设计

训练输入来自注册分类训练窗口的会话数据：

- 注册图列表。
- 类别列表。
- 每张注册图按类别标注的 ROI。
- 训练参数：隐藏层数量、最大迭代次数、置信度阈值、最少样本数等。

训练前校验：

- 至少 2 个有效类别。
- 每个参与训练的类别至少 1 个有效 ROI；推荐每类至少 3 个 ROI，样本不足时允许训练但给出质量警告。
- ROI 必须落在图像范围内，宽高不小于最小像素阈值。
- ROI 内能提取出有效特征，特征长度必须等于模型声明长度。
- 类别名不能为空，类别 ID 和类别名稳定写入类别表。

训练流程：

1. 将每张注册图转换为 HALCON image。
2. 根据 ROI 生成 HALCON region，裁剪或 reduce 到 ROI 域。
3. 按模型配置执行预处理。
4. 提取固定长度特征向量。
5. 创建 MLP：`create_class_mlp(numFeatures, numHidden, numClasses, 'softmax', 'normalization', 1, randSeed, mlpHandle)`。
6. 对每个样本调用 `add_sample_class_mlp`，类别目标使用稳定的类别索引。
7. 调用 `train_class_mlp`，记录 `Error` 和 `ErrorLog`。
8. 调用 `write_class_mlp` 保存分类器。
9. 写入模型元数据 JSON。

训练输出必须包含成功/失败状态。训练失败时，不覆盖上一次有效模型包；若写入到临时目录，只有 MLP 和 JSON 都写入成功后再原子替换目标模型目录。

## 推理阶段设计

推理输入来自 `ToolRequest.image` 和 `ToolConfig.params.registeredClassification`。

推理流程：

1. 校验图像非空、模型目录存在、元数据 JSON 存在、MLP 文件存在。
2. 加载元数据，确认 `modelType=halcon_mlp_registered_classification`。
3. 校验 `featureVersion` 为 runner 支持的版本。
4. 解析检测 ROI；全屏使用整图，矩形 ROI 使用归一化坐标转像素。
5. 将输入图转换为 HALCON image，并 reduce 到检测 ROI。
6. 按元数据声明的预处理和特征版本提取特征。
7. `read_class_mlp` 读取分类器。
8. 调用 `classify_class_mlp(MLPHandle, Features, TopK, Class, Confidence)`。
9. 将 HALCON 类别索引映射为项目类别名，并把置信度转为 0-100。
10. 根据 `judgeRule` 和最低得分输出 OK/NG。

第一版不在推理阶段做模板定位。位置修正若开启但外部补偿信息不可用，应返回 `position_correction_not_available` 或按 UI 策略提示未实现，不得在 runner 内自行做匹配替代。

## 模型包结构

注册分类模型保存为目录，而不是单个私有二进制文件。

```text
registered_classification_model/
  model.gmc
  metadata.json
  training_report.json
```

`model.gmc`：

- 由 `write_class_mlp` 生成。
- 只保存 HALCON MLP 分类器。

`metadata.json`：

```json
{
  "modelType": "halcon_mlp_registered_classification",
  "schemaVersion": 1,
  "featureVersion": "halcon_mlp_roi_stats_v1",
  "halconVersion": "24.11",
  "classLabels": [
    {"id": 0, "name": "OK"},
    {"id": 1, "name": "NG"}
  ],
  "featureNames": [
    "roiAspect",
    "roiAreaRatio",
    "foregroundAreaRatio",
    "foregroundCenterX",
    "foregroundCenterY",
    "momentRa",
    "momentRb",
    "momentPhi",
    "grayMean",
    "grayMin",
    "grayMax",
    "grayDeviation",
    "grayHist00",
    "grayHist01",
    "grayHist02",
    "grayHist03",
    "grayHist04",
    "grayHist05",
    "grayHist06",
    "grayHist07",
    "grayHist08",
    "grayHist09",
    "grayHist10",
    "grayHist11",
    "grayHist12",
    "grayHist13",
    "grayHist14",
    "grayHist15"
  ],
  "featureLength": 28,
  "preprocess": {
    "toGray": true,
    "normalizeGray": true,
    "smooth": "gauss"
  },
  "mlp": {
    "numHidden": 16,
    "maxIterations": 200,
    "randSeed": 42
  },
  "thresholds": {
    "minScore": 80,
    "rejectScore": 60,
    "top2Gap": 0
  }
}
```

`featureLength` 必须与 `featureNames` 数量和 runner 实际输出的特征向量长度一致。

`training_report.json`：

- 样本总数。
- 每类样本数。
- 每类有效/无效 ROI 统计。
- 训练耗时。
- HALCON `Error` 和 `ErrorLog`。
- 训练时的 warning 列表。

## 配置字段调整

`ToolConfig.params.registeredClassification` 中模型相关字段调整为：

```text
modelType = "halcon_mlp_registered_classification"
modelPath = "<model directory>"
modelName
detectRegionType = "full" | "rectangle"
roiNormalized
topK
enablePositionCorrection
positionCorrectionSource
```

旧值 `modelType=halcon_dl_classification` 不再运行。遇到旧配置时返回：

```text
unsupported_model_type
```

提示信息应说明当前版本只支持 `halcon_mlp_registered_classification`。不得自动尝试读取 DL 模型。

## 结果判断

分类得分统一为 0-100。

类别判断：

- `judgeRule.mode = "class_match"`。
- `predictedLabel == expectedLabel` 且 `score >= rejectScore` 时 OK。
- `expectedLabel` 为空返回 `missing_expected_label`。

最低得分：

- `judgeRule.mode = "min_score"`。
- `score >= minScore` 时 OK。
- `minScore` 范围为 0-100。

拒识策略：

- 若最高分低于 `rejectScore`，状态为 `classification_rejected`，`success=true`，`ok=false`。
- 若配置了 `top2Gap` 且 Top1 与 Top2 分差不足，状态为 `classification_ambiguous`，`success=true`，`ok=false`。
- 第一版可把 `top2Gap` 默认为 0，即不启用 Top2 分差拒识。

## payload 和 overlay

`ToolResult.payload` 至少包含：

- `algorithm = "halcon_mlp_registered_classification"`
- `modelPath`
- `modelName`
- `modelType`
- `schemaVersion`
- `featureVersion`
- `predictedLabel`
- `predictedClassId`
- `score`
- `topClasses`
- `detectRegionType`
- `roiNormalized`
- `roiPixelsRect`
- `featureLength`
- `elapsedMs`
- `judgeMode`
- `expectedLabel`
- `minScore`
- `rejectScore`
- `top2Gap`
- `trainingSampleCount`
- `classLabels`
- `positionCorrectionApplied`
- `errorCode`
- `errorMessage`

overlay 第一版只绘制检测 ROI 和文本结果：

- OK/NG 颜色沿用现有工具约定。
- ROI 使用检测区域，不显示训练 ROI。
- 文本包含预测类别、得分和耗时。

## 错误码

训练阶段：

- `training_no_image`：没有注册图。
- `training_no_class`：没有类别。
- `training_not_enough_classes`：有效类别少于 2。
- `training_no_roi`：没有有效 ROI。
- `training_class_has_no_sample`：某个类别没有有效样本。
- `training_invalid_roi`：ROI 坐标无效或太小。
- `feature_extract_failed`：特征提取失败。
- `feature_length_mismatch`：特征长度不一致。
- `halcon_symbol_missing`：必要 HALCON 符号缺失。
- `mlp_create_failed`：创建 MLP 失败。
- `mlp_train_failed`：训练失败。
- `model_write_failed`：模型包写入失败。

推理阶段：

- `image_empty`：输入图为空。
- `model_path_empty`：模型路径为空。
- `model_file_not_found`：模型目录、MLP 或元数据不存在。
- `unsupported_model_type`：模型类型不是 HALCON MLP 注册分类。
- `unsupported_feature_version`：特征版本不受当前 runner 支持。
- `invalid_roi`：检测 ROI 无效。
- `halcon_so_not_found`：HALCON runtime 找不到。
- `halcon_load_failed`：HALCON 库加载失败。
- `halcon_symbol_missing`：必要 HALCON 符号缺失。
- `mlp_read_failed`：读取 MLP 失败。
- `feature_extract_failed`：推理特征提取失败。
- `classification_failed`：`classify_class_mlp` 调用失败。
- `classification_rejected`：置信度低于拒识阈值。
- `classification_ambiguous`：Top1/Top2 分差不足。

## 接入影响

`RegisteredClassificationHalconRunner`：

- 改为 MLP 模型包读取、特征提取和分类推理。
- 移除 DL 模型符号依赖。
- 新增训练入口时，可拆出 `RegisteredClassificationTrainingRunner`，避免推理 runner 过大。

`RegisteredClassificationAdapter`：

- 解析 `modelType=halcon_mlp_registered_classification`。
- 对旧 DL 配置返回 `unsupported_model_type`。
- 保持 `ToolResult` 输出合同不变。

训练窗口：

- 现阶段 UI 已有注册图、类别和 ROI 会话数据。
- 后续算法接入时只把会话数据转换为训练请求，不在 UI 层提取特征或调用 HALCON。
- 训练成功后把模型目录写回主注册分类配置。

项目文档：

- `docs/FID/RegisteredClassification/registered_classification_function_implementation.md` 需要在实现计划中同步改写后端主线。
- `docs/FID/RegisteredClassification/注册分类提示词规范.md` 需要把“真实训练未接入”的阶段说明更新为 MLP 训练接入计划。

## 验证计划

HALCON 能力确认：

- 检查本机 HALCON C API 是否具备 MLP 创建、样本添加、训练、读写、分类和清理符号。
- 检查 ROI、预处理和特征提取所需符号。
- 缺核心符号时停止实现，并在文档中记录阻塞。

训练 smoke：

- 构造两类简易样本，每类多个 ROI。
- 训练成功生成 `model.gmc`、`metadata.json`、`training_report.json`。
- 校验样本不足、类别不足、ROI 无效、特征提取失败均返回明确错误。

推理 smoke：

- 加载训练得到的模型包。
- 对已知样本 ROI 推理，输出类别、得分和 TopK。
- 旧 `halcon_dl_classification` 配置返回 `unsupported_model_type`。
- 空图、缺模型、损坏元数据、特征版本不支持、HALCON 符号缺失均不崩溃。

工程验证：

```bash
mkdir -p build
cd build
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
make -j$(nproc)
```

涉及算法 runner 时，还需要新增并运行注册分类 MLP 训练/推理 smoke 子工程。

## 实施拆分建议

1. HALCON MLP 符号探测和最小训练 smoke。
2. 模型包读写结构和元数据校验。
3. 特征提取模块，固定 `featureVersion=halcon_mlp_roi_stats_v1`。
4. 训练 runner，从训练窗口会话数据生成模型包。
5. 推理 runner，替换现有 DL 推理链。
6. Adapter、UI 测试运行和结果展示联调。
7. 文档和提示词规范同步。

每一步都应保持旧无关工具链不受影响。任何阶段不得用非 HALCON 分类算法临时代替 MLP。
