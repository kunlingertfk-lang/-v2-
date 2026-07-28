# 注册分类通用 HALCON 深度 Embedding 升级实施规划

## 1. 文档目的

本文规划注册分类从当前 `HALCON Feature V2 + 双 class_knn` 升级为通用的
`HALCON 深度 Embedding + 注册检索`。目标是更换产品、形状、颜色或纹理后，仅需
重新注册样本，不修改算法代码，不增加针对圆环、齿轮、螺母等具体对象的专用规则。

本文是升级规划，不代表深度后端已经实现。当前生产可运行后端仍以
`注册分类算法当前实现说明.md` 为准。

### 1.1 2026-07-25实施状态

已完成：

- 基础模型descriptor合同及本地`RegisteredClassificationEmbeddingModelProvider`。
- `.hdl`和预处理`.hdict`的路径、输入规格、模型身份和SHA-256严格校验。
- HALCON `query_available_dl_devices`能力探针。
- 专项smoke覆盖缺失模型、非法ID、非法descriptor、缺失预处理、摘要不符和成功解析。
- 本机实测HALCON返回1个CPU DL设备。
- 主工程构建，以及现有Feature V2双KNN、Adapter和Dialog smoke回归。

尚未完成：

- 真实`.hdl`读取、网络summary和Embedding层验证。
- 深度Embedding提取、schema 3训练包及DL Runner。
- DL模式下的现场注册、模型代际热切换和UI状态。

当前阻塞仍是缺少许可可部署的真实`.hdl`及配套预处理文件。现有Feature V2现场注册、
训练成功后自动选模和当前工件测试检测保持可用。

## 2. 理解复述

### 2.1 目标

- 输入：单目标图像或经全屏、矩形 ROI、位置修正后得到的单目标 ROI。
- 注册：用户为每个类别添加少量图像并标注类别，点击训练后建立可立即使用的注册模型。
- 推理：对待测 ROI 提取通用深度特征，与注册样本及类别中心比较。
- 输出：Top-K、预测类别、相似度、最相似注册图索引、UNKNOWN 拒识原因和耗时。
- 通用性：默认算法不得包含面向某种具体零件的齿数、孔径比、圆度或六边形规则。
- 兼容性：现有 schema 2 Feature V2 模型继续运行；新后端使用独立模型类型和 schema，
  不静默迁移、不静默回退。

### 2.2 范围边界

本规划覆盖“单目标 ROI 分类”，不负责在整图中搜索多个目标。多目标定位和分类属于
注册目标检测工具边界。

“通用”限定为模型训练分布覆盖范围内的工业视觉注册分类，不承诺对任意自然图像、
任意成像模态或任意未知目标都有效。可靠 UNKNOWN 仍需要独立已知验证图和未知类验证图。

### 2.3 明确不做

- 不使用 OpenCV DNN、第三方推理库或自写卷积网络替代 HALCON 深度学习链路。
- 不把当前零件的几何特征硬编码进默认分类器。
- 不直接使用深度模型原有 Softmax 类别作为用户注册类别。
- 不复用历史 `halcon_dl_classification` 名称恢复旧兼容分支。
- 不在缺少 `.hdl` 模型、DL 许可证或必要 HALCON 符号时伪造深度结果。
- 不删除或覆盖现有 schema 2 模型。

## 3. 已确认事实、信息缺口与默认假设

### 3.1 已确认事实

- 本机 HALCON 为 `20.11.1.1`，安装路径为 `/opt/halcon`。
- 本机头文件具备 `read_dl_model`、`query_available_dl_devices`、
  `set_dl_model_param`、`apply_dl_model` 等接口。
- HALCON 20.11 官方示例通过
  `set_dl_model_param(DLModelHandle, 'extract_feature_maps', LayerName)`
  截断分类模型，并通过 `apply_dl_model` 和 `get_dict_object` 取得指定层特征图。
- `extract_feature_maps` 会修改并截断当前模型句柄，不能在同一句柄上恢复原分类网络。
- 当前安装未发现可直接使用的 `.hdl` 预训练分类模型。
- 2026-07-25实测 `query_available_dl_devices` 返回1个HALCON CPU设备：
  `AMD Ryzen 7 H 260 w/ Radeon 780M Graphics`，`inferenceOnly=false`。
- 当前环境未发现 `nvidia-smi`，不能假设存在可用 NVIDIA GPU。
- 当前注册分类模型为 schema 2、59 维 Feature V2、样本/中心双 KNN。
- 当前相似度采用 `1 - distance² / 2`，实际样例存在 Top-K 分数挤在高分段的问题。

### 3.2 阻塞项

以下任一项未通过时，不进入生产后端实施：

1. 取得许可可用、允许随产品部署的 HALCON `.hdl` 分类基础模型。
2. 使用 `query_available_dl_devices` 确认至少一个 DL 推理设备。
3. 使用真实模型完成一次 `read_dl_model → extract_feature_maps → apply_dl_model`。
4. 确认当前 HALCON license 允许所需 DL 推理算子。
5. 确认所选模型层、输出维度、通道和预处理合同稳定。
6. CPU-only 环境下的 P50/P95 耗时满足产品节拍；节拍上限需由项目验收方给出。

### 3.3 可安全采用的默认假设

- 第一阶段以 CPU 推理验证，不将 GPU 作为功能正确性的前提。
- 用户注册时每类建议 10～20 张；允许 1 张注册，但 UI 和报告明确标记
  “只能验证样本回放，无法可靠估计类内半径”。
- 注册和推理使用同一套 ROI、通道、缩放、填充和像素归一化合同。
- 深度后端不与 Feature V2 拼接；手工特征只保留为独立旧后端，避免新后端随样例规则漂移。

## 4. 目标架构

```text
相机帧 / PC图像
        |
        v
位置修正（可选，沿用公共 PositionCorrectionConsumer）
        |
        v
全屏或矩形 ROI -> 裁剪 -> 保持宽高比缩放 -> 固定填充
        |
        v
HALCON DLSample + 与模型一致的预处理
        |
        v
HALCON 分类模型的指定中间层特征图
        |
        v
HALCON 全局池化 -> 向量校验 -> L2 归一化
        |
        +----------------------+
        |                      |
        v                      v
样本 KNN/最近注册图       类别中心 KNN/类别统计
        |                      |
        +----------+-----------+
                   v
         原始距离 + 校准分数 + Top-K
                   |
                   v
 最小相似度 / 类别差值 / 类半径 -> 类别或 UNKNOWN
```

### 4.1 深度特征提取合同

新增 `RegisteredClassificationDlEmbeddingExtractor`，只负责：

1. 将 `cv::Mat` 桥接为 HALCON Image。
2. 按模型参数确定输入宽、高、通道和数值范围。
3. 保持 ROI 宽高比缩放并填充，不把所有目标强行拉伸成同一形状。
4. 生成 DLSample 并执行与模型绑定的预处理。
5. 从配置的 `embeddingLayer` 提取特征图。
6. 使用 HALCON 算子做全局平均池化，形成固定长度向量。
7. 检查长度、有限值和范数，再执行 L2 归一化。

提取器不得根据目标外观选择不同的手工分支。注册与推理必须共享同一入口，禁止复制两套
预处理代码。

### 4.2 模型句柄

- 原始分类模型与截断后的 Embedding 模型使用不同句柄。
- Runner 缓存截断后的只读模型句柄，避免每帧 `read_dl_model`。
- HALCON 20.11 DL 设备和模型句柄具有线程约束，缓存按执行线程或 ToolEngine
  的实际线程模型设计，不跨线程直接复用。
- 模型路径、文件摘要、Embedding 层和预处理摘要发生变化时，缓存必须失效。
- 所有句柄采用 RAII；加载下一模型、异常和程序退出时各清理一次。

### 4.3 基础模型提供器接口

工程先预留稳定的基础模型提供器，不把具体 `.hdl` 文件名和网络层写死在
`RegisteredClassificationTrainingRunner` 或 Dialog 中。

建议新增：

```cpp
struct RegisteredClassificationEmbeddingModelDescriptor
{
    QString modelId;
    QString modelVersion;
    QString modelPath;
    QString modelSha256;
    QString preprocessPath;
    QString preprocessSha256;
    QString embeddingLayer;
    int embeddingLength = 0;
    int inputWidth = 0;
    int inputHeight = 0;
    int inputChannels = 0;
};

class RegisteredClassificationEmbeddingModelProvider
{
public:
    virtual ~RegisteredClassificationEmbeddingModelProvider() = default;
    virtual EmbeddingModelResolveResult resolve(
            const QString &modelId,
            const QString &modelVersion) const = 0;
};
```

首个实现使用本地受管理目录，例如：

```text
ModelFiles/RegisteredClass/_feature_models/
└── industrial_embedding_v1/
    ├── feature_model.hdl
    ├── preprocess_params.hdict
    └── descriptor.json
```

descriptor示例见`industrial_embedding_descriptor.example.json`。示例中的摘要和层名是占位，
部署前必须由真实模型生成和Phase 0验证，不能原样复制到运行目录。

Runner、训练器和模型管理只依赖 descriptor，不依赖具体模型名称。未来替换
`industrial_embedding_v2.hdl` 时，通过新的 `modelId/modelVersion/SHA-256` 并存，
旧现场模型继续绑定旧基础模型，禁止自动换底座后继续使用旧 Embedding。

基础模型暂未提供时：

- schema 2 Feature V2 注册、训练和检测保持可用。
- DL模式入口可显示“基础特征模型未安装”，但不能生成伪DL模型包。
- 调用DL训练或推理返回 `dl_feature_model_missing`。
- UI、配置、模型管理和 Adapter 可以先按 descriptor 合同完成接口预留。

### 4.4 注册与分类

继续使用“样本 + 类别中心”双路径，但特征来源改为深度 Embedding：

- 样本路径保留类别的多种正常外观，并返回最相似注册图索引。
- 中心路径抑制异常注册图影响。
- 注册新类别只提取 Embedding 和重建 KNN，不训练深度网络。
- 相似度排序至少使用全部类别，`topK` 只影响输出数量。

L2 归一化后的向量可继续使用 HALCON `class_knn` 的欧氏距离。原始余弦相似度为：

```text
rawCosine = clamp(1 - distance² / 2, -1, 1)
```

`rawCosine` 只作为诊断值，不直接等同 UI 的百分制相似度。

### 4.5 现场在线注册与热切换

“现场实时注册”定义为：生产人员采集当前工件ROI并指定类别后，系统立即提取Embedding，
在不训练深度网络的情况下生成新的轻量注册模型；提交成功后，后续检测帧自动使用新模型。
它不是在每一检测帧上持续训练基础 `.hdl` 网络。

在线注册流程：

```text
开始注册草稿
    |
    +--> 采集当前帧/导入图片
    +--> 选择或新建类别
    +--> 框选ROI/使用位置修正
    +--> 调用基础.hdl提取Embedding
    +--> 显示样本质量和最近类别
    |
提交注册
    |
    +--> 在临时目录重建样本KNN、中心KNN和统计
    +--> 完整校验schema、文件摘要、类别和向量维度
    +--> 原子发布新的modelGeneration
    +--> 检测Runner在帧边界切换不可变快照
```

并发约束：

- 活跃检测模型只读，注册过程不得原地修改正在使用的 KNN 或统计文件。
- 每次提交生成新的 `modelGeneration` 和临时模型目录。
- 只有新快照全部写入且严格校验通过后，才原子替换“当前模型”引用。
- 正在执行的帧继续使用旧快照；下一帧开始使用新快照，不出现半新半旧状态。
- 注册取消、特征提取失败、落盘失败或校验失败时，旧检测模型不受影响。
- 基础 `.hdl` 句柄可复用，但训练/检测线程必须遵守 HALCON 的线程和设备句柄约束。

现场注册反馈至少包括：

- 当前注册图质量是否可用。
- 与已有类别的最近相似度，防止误注册到错误类别。
- 当前类别样本数及是否足以启用类半径。
- 新模型生效的 `modelGeneration`。
- 一次当前图回放结果，证明新注册类别能够被检测。

样本策略：

- 允许1张样本立即生成可检测模型，满足现场快速建类。
- 1～2张时关闭类半径并显示“泛化能力未验证”。
- 3张起可计算初步类半径。
- 建议累计5～20张覆盖旋转、光照、位置和正常制造差异。
- 新增样本后重新做留一法统计，但不重新训练基础 `.hdl`。

### 4.6 分数标定与 UNKNOWN

新后端必须同时输出：

- `rawDistance`
- `rawCosine`
- `calibratedSimilarity`，范围 `[0, 100]`
- `secondSimilarity`
- `margin`
- `classRadius` 和 `radiusEnabled`

训练时使用留一法计算类内分布，禁止用样本与自身的 100% 匹配参与阈值标定。
有独立验证集时，优先用已知验证图和未知验证图拟合单调距离映射；没有未知验证图时，
只能标定已知类召回边界，并在训练报告中写明“未知类误接收率未验证”。

拒识顺序保持可诊断：

1. `calibratedSimilarity < minSimilarity`
2. `Top1 - Top2 < minMargin`
3. `distanceToCenter > classRadius`

`minMargin` 在新 schema 中使用浮点百分值，不再受整数 1% 粒度限制。UI 显示至少
一位小数。模型阈值、工具运行覆盖值和结果 payload 均保持同一单位。

## 5. 模型选择闸门

不能因为 HALCON 能输出某层特征图，就默认该层适合注册分类。必须先完成模型与层选择实验。

### 5.1 候选要求

- HALCON 20.11 能读取并在目标设备上推理。
- 模型类型为 classification，支持 `extract_feature_maps`。
- 模型文件许可允许项目部署、导入导出和模型包分发。
- 输入规格、预处理参数和候选 Embedding 层可固定记录。
- 模型不是只针对当前圆环/齿轮数据训练的专用四分类模型。

### 5.2 层选择

对 `get_dl_model_param(..., 'summary')` 返回的网络层做候选筛选，优先考察分类头之前、
空间分辨率较低且语义较深的层。每个候选层必须记录：

- 输出形状和池化后维度
- 同图重复推理误差
- 类内距离分布
- 类间最近距离
- 已知准确率
- UNKNOWN 误接收率
- CPU/GPU P50、P95耗时和内存

不凭层名称直接确定生产层。

### 5.3 通用验证集

模型选择不能只使用当前一张零件图。最小验证集至少覆盖：

1. 轮廓主导：圆环、螺母、齿轮及其他外形件。
2. 纹理主导：表面纹理或材料外观不同、轮廓接近的类别。
3. 颜色主导：形状接近、颜色不同的类别。
4. 局部结构主导：孔位、端子、装配方向或局部图案不同的类别。

每个域建议至少 4 个已知类别，每类 10 张注册图、5 张独立测试图，并准备不少于
20 张未注册类别图。必须按来源批次拆分注册与测试，不能将同一原图的简单复制同时放入两边。

## 6. 模型包与兼容方案

### 6.1 新模型身份

建议新建而不是复用当前身份：

```text
modelType     = halcon_dl_embedding_registered_classification
schemaVersion = 3
featureVersion = halcon_dl_embedding_<backboneFingerprint>_<layer>
```

现有 schema 2：

```text
halcon_knn_registered_classification
```

继续按原路径推理。Adapter 根据已校验的 metadata 显式分发两个 Runner，不允许
深度模型失败后回退 Feature V2，也不允许反向回退。

### 6.2 schema 3最小包

```text
model_xxx/
├── metadata.json
├── model.gnc
├── class_centers.gnc
├── class_stats.json
├── sample_index.json
├── calibration.json
├── training_report.json
├── preprocess_params.hdict
└── feature_model.hdl 或受校验的共享模型引用
```

`metadata.json` 至少新增：

- `embeddingLayer`
- `embeddingLength`
- `inputWidth`、`inputHeight`、`inputChannels`
- `resizeMode=keep_aspect_pad`
- `paddingValue`
- `preprocessSha256`
- `featureModelSha256`
- `dlRuntimePreference`
- `scoreCalibrationVersion`

`sample_index.json` 将 KNN 样本索引映射到训练会话图片、ROI、类别和用户可见注册图索引。

基础模型是否随每个模型包复制，需在“许可、包体积、离线导入”三项确认后决定：

- 若可分发且强调跨机导入完整性，模型包携带 `feature_model.hdl`。
- 若不可复制或体积不可接受，使用共享只读模型仓库；模型包保存稳定 ID 和 SHA-256，
  导入时缺失必须返回 `dl_feature_model_missing`，不得选择另一模型代替。

## 7. 模块与文件抓手

### 7.1 新增算法模块

- `src/algorithms/recognition/RegisteredClassificationEmbeddingModelProvider.{h,cpp}`
  - 解析基础模型ID、版本、路径、预处理合同和SHA-256；基础模型缺失时明确失败。
- `src/algorithms/recognition/RegisteredClassificationDlEmbeddingExtractor.{h,cpp}`
  - DL设备查询、模型读取、截断、预处理、池化和向量输出。
- `src/algorithms/recognition/RegisteredClassificationDlHalconRunner.{h,cpp}`
  - schema 3加载、双KNN、标定、Top-K和拒识。
- `src/algorithms/recognition/RegisteredClassificationScoreCalibration.{h,cpp}`
  - 留一法统计、距离映射和阈值建议。
- `src/algorithms/recognition/RegisteredClassificationOnlineRegistry.{h,cpp}`
  - 管理注册草稿、临时模型构建、严格校验、原子发布和模型代际。
- `src/algorithms/recognition/RegisteredClassificationModelSnapshot.{h,cpp}`
  - 封装只读模型快照及帧边界热切换，隔离注册写入与在线检测读取。

### 7.2 修改现有模块

- `RegisteredClassificationTrainingRunner`
  - 按新训练请求显式选择 Feature V2 或 DL Embedding；
  - 深度模式生成 schema 3，不改变 schema 2输出。
- `RegisteredClassificationModelPackage`
  - 增加 schema 3严格校验和共享模型指纹校验；
  - 保留 schema 2和legacy inspection。
- `RegisteredClassificationAdapter`
  - 按模型包类型显式分发，统一 ToolResult。
- `RegisteredClassificationTrainingDialog`
  - 显示特征后端、基础模型可用性、样本覆盖提示和标定结果；
  - 不要求普通用户选择网络层，生产层由已验证配置提供。
- `RegisteredClassificationDialog`
  - 将深度模型的最小类别差值调整为可显示小数；
  - 显示原始相似度、校准相似度、最相似注册图和拒识原因。
- `qt_ui_test.pro`
  - 仅加入必要新源文件，不引入非HALCON算法依赖。

### 7.3 错误合同

新增错误至少包括：

- `dl_feature_model_missing`
- `dl_feature_model_hash_mismatch`
- `dl_license_unavailable`
- `dl_device_unavailable`
- `dl_model_read_failed`
- `dl_embedding_layer_invalid`
- `dl_preprocess_mismatch`
- `dl_apply_failed`
- `dl_embedding_invalid`
- `dl_embedding_length_mismatch`
- `dl_calibration_missing`

所有错误保留 HALCON 操作名、模型路径/指纹、设备类型和底层错误文本；不得返回 UNKNOWN
掩盖后端错误。

## 8. 分阶段实施

### Phase 0：可行性探针

1. 安装或取得候选 HALCON `.hdl`模型和对应预处理文件。
2. 新增独立 smoke，调用 `query_available_dl_devices` 并记录设备。
3. 读取模型 summary，验证至少一个候选层可提取特征图。
4. 对同图、同类变化、异类和未知类计算距离矩阵。
5. 记录 CPU/GPU P50、P95和内存。
6. 输出模型许可、可分发方式和模型文件摘要。

退出条件：DL链可执行、同图确定性稳定、至少一个候选层在跨域验证集中表现出
“类内距离明显小于最近类间距离”，且节拍可接受。否则停止生产接入，保留 schema 2。

### Phase 1：Embedding提取器

1. 实现统一 ROI 与位置修正输入。
2. 实现保持宽高比缩放、填充和 HALCON DL预处理。
3. 实现模型截断、池化、L2归一化和句柄生命周期。
4. 覆盖灰度/RGB、空图、无效ROI、通道不匹配、模型/层缺失。

退出条件：注册与推理对相同输入产生一致向量；不同线程、重复加载和错误路径无泄漏、无崩溃。

### Phase 2：schema 3训练和模型生命周期

1. 构建样本/中心双KNN。
2. 保存注册图索引、类别统计和校准文件。
3. 实现严格模型包校验、导入、导出、删除和重新训练。
4. 保留 schema 2可运行和模型管理能力。

退出条件：新建、保存、重开、导出、跨目录导入和重训闭环通过；破损包被明确拒绝。

### Phase 3：推理、拒识和UI

1. Adapter按模型类型分发。
2. 输出 Top-K、最相似注册图、原始/校准分数和拒识诊断。
3. 接入位置修正后的真实 HALCON Region。
4. UI支持小数margin和UNKNOWN原因展示。
5. 接入现场注册草稿、提交、取消和当前图回放。
6. 实现不可变模型快照及帧边界原子热切换。

退出条件：同一注册图回放不会因分数压缩被错误拒识；已知低相似、类别模糊和类外样本
可区分诊断；注册失败不影响旧模型持续检测，注册成功后下一帧使用新模型代际。

### Phase 4：通用性回归和默认切换评审

1. 跑轮廓、纹理、颜色、局部结构四类数据集。
2. 对比 Feature V2、DL Embedding和海康参考结果。
3. 评估已知准确率、未知误接收率、拒识率、P95耗时和内存。
4. 依据证据决定新训练默认后端；未达标时不切换默认值。

## 9. 验证与验收

### 9.1 必须新增的smoke

- `registered_classification_dl_capability_smoke`
- `registered_classification_dl_embedding_smoke`
- `registered_classification_dl_package_smoke`
- `registered_classification_dl_adapter_smoke`
- `registered_classification_dl_position_correction_smoke`
- `registered_classification_dl_unknown_rejection_smoke`

### 9.2 正常路径

- 同一注册图重复推理，Embedding一致且预测正确。
- 独立同类图预测正确，Top1高于Top2。
- 新增类别只重建注册库，不训练基础网络。
- Top-K类别、分数及最相似注册图索引稳定。
- 保存、重开、导出导入后结果一致。
- 开启位置修正后，变换ROI与显示overlay使用同一矩阵。
- 检测运行期间新增类别，旧帧使用旧代际，新帧完整切换到新代际。
- 单样本注册可立即回放，且明确提示类半径和泛化验证尚未启用。
- 在线注册取消或提交失败后，当前检测模型和输出保持不变。

### 9.3 异常路径

- 空图、无效ROI、ROI越界。
- 灰度/RGB通道与模型不兼容。
- HALCON runtime、license、DL设备或必要符号缺失。
- `.hdl`、预处理文件、KNN或校准文件缺失/损坏/摘要不符。
- Embedding层不存在、向量为空、非有限或长度变化。
- 只有1～2张样本时半径关闭并给出明确诊断。
- 未知样本被拒识，而不是强制归入最相似类别。
- 在线注册过程中断电、磁盘写入失败或包校验失败时，旧代际仍可加载和检测。
- 注册模型绑定的基础模型ID或SHA-256不一致时拒绝加载，不自动换用其他基础模型。

### 9.4 指标

在没有用户节拍要求前，不虚设固定毫秒门槛；Phase 0必须记录目标设备的P50/P95，
由产品验收方确认上限。算法质量至少报告：

- 已知类Top-1准确率
- 已知类误拒率
- 未知类误接收率
- 类别混淆矩阵
- 各类最小Top1-Top2 margin
- 类内/类间距离分布
- 同图重复推理最大漂移

建议进入默认后端评审的目标为：跨域验证集已知类Top-1不低于95%，未知误接收率不高于5%；
该数值是评审目标，不是尚未验证的完成声明。

### 9.5 工程验证

- 影子目录执行 `qmake ../qt_ui_test.pro` 和 `make -j$(nproc)`。
- 分别运行新增DL smoke和现有schema 2注册分类smoke。
- 手动检查训练、模型管理、检测、位置修正、取消/完成和异常提示。
- 构建产物、模型测试缓存和运行日志不入库。

## 10. 决策点

生产切换前仍需提供许可可部署的工业 HALCON `.hdl` 基础模型。验证阶段采用公开
ONNX Model Zoo MobileNetV2，经HALCON 20.11导入并转存为`.hdl`，只用于打通
Phase 0和评估特征可分性，不作为默认生产模型。Phase 0结果决定：

- 使用哪个Embedding层；
- 是否需要对工业数据进行基础模型微调；
- CPU是否满足节拍，是否要求GPU；
- 模型随包复制还是使用共享仓库；
- 是否具备替换Feature V2默认后端的证据。

## 11. 实施进度（2026-07-26）

已完成Phase 0中不依赖真实`.hdl`内容的接口部分：

- `RegisteredClassificationEmbeddingModelProvider`及descriptor schema 1。
- 本地基础模型目录、版本和SHA-256严格校验。
- `RegisteredClassificationDlCapability`设备能力探针。
- 专项`registered_classification_dl_capability_smoke`。
- 主工程编译接入及现有Feature V2回归。

本机实测HALCON可发现1个CPU DL设备。验证阶段进一步完成：

- 下载公开ONNX Model Zoo MobileNetV2 opset 7；
- HALCON `read_dl_model`成功导入，模型类型由`generic`明确设置为`classification`；
- 使用HALCON标准过程生成`constant_values`预处理字典并转存`.hdl`；
- 选用`mobilenetv20_features_pool0_fwd`，得到`1×1×1280`特征；
- `RegisteredClassificationEmbeddingModelProvider`对真实模型包和SHA-256校验通过；
- `registered_classification_embedding_inference_smoke`在CPU执行真实图像推理通过，
  单次首次观测约127 ms，向量长度1280且值有限、L2范数非零。

同时记录了一个兼容性反例：ONNX Model Zoo ResNet-18 opset 7在本机HALCON 20.11
读取时返回错误7801，因此外部ONNX不能仅按opset判断可用，必须逐模型实测。

临时模型位于影子构建目录，不入库。可重复生成工具为：

- `registered_classification_public_backbone_probe`：导入并输出网络summary；
- `registered_classification_public_backbone_provision`：生成`.hdl`、`.hdict`和descriptor；
- `registered_classification_embedding_inference_smoke`：验证真实特征层推理。

当前完成的是Phase 0链路验证，尚未完成多类别类内/类间距离、旋转尺度稳定性、P50/P95
统计，也没有启用schema 3或替换生产Feature V2后端。
