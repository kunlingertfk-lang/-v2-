# 注册分类 HALCON KNN 少样本特征 V2 设计

## 背景与决策

当前注册分类使用 `halcon_mlp_registered_classification` 和固定 28 维 ROI 统计特征。该实现把 ROI 宽高、ROI 面积、前景重心和原始方向直接送入 MLP，容易学习框选范围和样本分布；当每类只有 1～10 个样本时，MLP 也容易过拟合。

本轮采用已确认的方案 1：硬替换为 HALCON 原生 KNN 少样本分类。每个注册 ROI 先提取面向目标的 V2 特征，再由 HALCON `class_knn` 保存注册样本和类别中心。在线阶段组合“最近注册样本”和“类别中心”两种距离，并执行最低相似度、Top1/Top2 分差、类内半径三层拒识。

这里的“特征模板”只表示 KNN 中保存的特征向量，不是 HALCON 形状模板，不调用 `create_shape_model` 或 `find_shape_model`。位置定位与位置修正继续作为独立功能实现。

## 目标

- 每类 1～10 个有效样本即可生成模型，不再训练 MLP 网络。
- 同一图像、同一类别允许注册多个不同大小、不同方向和不同框选范围的 ROI，每个 ROI 都作为独立样本。
- 去除 ROI 框本身的位置、面积和方向依赖，增强尺度、旋转和 ROI 留白变化下的稳定性。
- 同时利用形状、空间占用、灰度、颜色和纹理信息，避免模型只偏向圆形等单一形状。
- 使用 HALCON 原生 KNN 完成核心分类、模型保存、模型加载和句柄释放，不引入自写分类器或非 HALCON 算法依赖。
- 对未知物体、类别边界模糊和类内异常样本给出明确的 `UNKNOWN` 与拒识原因。
- 保留现有训练会话、模型管理、日期目录、重新训练和原子更新闭环。

## 非目标

- 不保留 MLP 推理兼容分支，不继续读取 `model.gmc`。
- 不保留或恢复 DL 分类兼容，不接入预训练网络、ONNX、TensorRT 或其他深度特征。
- 不实现形状模板匹配、目标定位或位置修正。
- 不在第一版增加 SVM、自动特征选择、聚类中心或在线增量学习。
- 不承诺在只有一个样本时自动覆盖全部光照、批次和外观变化；单样本允许生成模型，但必须给出质量警告。

## HALCON 能力确认

本机 HALCON 版本为 24.11.1.0，已在 `libhalconc.so` 和 `HProto.h` 中确认以下核心接口存在：

- KNN：`create_class_knn`、`add_sample_class_knn`、`train_class_knn`、`set_params_class_knn`、`classify_class_knn`、`read_class_knn`、`write_class_knn`、`clear_class_knn`。
- ROI 与前景：`gen_rectangle1`、`gen_region_polygon_filled`、`reduce_domain`、`gauss_filter`、`binary_threshold`、`opening_circle`、`closing_circle`、`fill_up`、`connection`、`intersection`、`difference`、`area_center`。
- 姿态与尺度：`smallest_rectangle2`、`orientation_region`、`vector_angle_to_rigid`、`affine_trans_image`、`affine_trans_region`、`crop_domain`、`zoom_image_size`、`zoom_region`。
- 形状与统计：`circularity`、`compactness`、`convexity`、`rectangularity`、`eccentricity`、`moments_region_central_invar`、`intensity`、`gray_histo`。
- 颜色与纹理：`decompose3`、`trans_from_rgb`、`entropy_gray`、`cooc_feature_image`。

`classify_class_knn` 的 `classes_distance` 模式使用 L2 距离，并可返回各类别最近注册样本的类别 ID 和距离。第一版把 `num_checks` 设置为 `0` 使用精确搜索，避免少样本模型出现近似搜索误差。

## 总体架构

```text
训练:
注册图 + 类别 + ROI
  -> HALCON 外层 ROI
  -> 双极性前景分割
  -> 姿态与尺度归一化
  -> 59 维 V2 特征
  -> HALCON 样本 KNN + HALCON 类别中心 KNN
  -> 类内半径统计
  -> V2 模型包

推理:
输入图 + 检测 ROI
  -> 与训练完全相同的前景、归一化和 59 维特征
  -> 两个 classify_class_knn
  -> 样本/中心融合评分
  -> 最低相似度 + 类别分差 + 类内半径
  -> 已知类别或 UNKNOWN
  -> judgeRule 输出 OK/NG
```

核心组件边界：

- `RegisteredClassificationFeatureExtractor`：只负责 HALCON ROI、前景、归一化和 V2 特征，不保存模型、不决定类别。
- `RegisteredClassificationTrainingRunner`：校验样本，创建两个 HALCON KNN，计算类别中心和半径，原子写入模型包。
- `RegisteredClassificationModelPackage`：定义 schema 2、路径、元数据、类别统计和完整性校验。
- `RegisteredClassificationHalconRunner`：加载两个 KNN，提取查询特征，调用 HALCON 分类并执行拒识与结果判断。
- Dialog、Adapter 和训练会话类：保留现有职责，只扩展 V2 模型类型和拒识配置。

## ROI 与前景提取

训练外层 ROI 支持全屏、矩形和已有多边形标注。矩形使用 `gen_rectangle1`，多边形使用 `gen_region_polygon_filled`；在线检测 ROI 仍只支持全屏或矩形。外层 ROI 只限定搜索范围，不再作为分类特征。

每个样本按以下固定流程生成目标前景：

1. 将输入通过 `gen_image_interleaved` 转为 HALCON 图像，并在外层 ROI 内转灰度，再执行尺寸为 3 的 `gauss_filter`。
2. 使用 `binary_threshold(..., 'max_separability', 'light')` 和 `binary_threshold(..., 'max_separability', 'dark')` 同时生成亮目标与暗目标候选。
3. 两组候选分别执行 `opening_circle`、`closing_circle`、`fill_up` 和 `connection`。形态学半径固定为 `max(1.0, 0.005 * min(roiWidth, roiHeight))`。
4. 保留面积占外层 ROI `2%～98%` 的连通域。
5. 外层 ROI 边界带宽固定为 `max(1, round(0.01 * min(roiWidth, roiHeight)))`。`borderTouchRatio` 为候选与边界带交集面积除以候选面积；`normalizedCenterDistance` 为 `hypot((column-centerColumn)/(roiWidth/2), (row-centerRow)/(roiHeight/2)) / sqrt(2)` 并限制到 `[0,1]`。候选评分固定为：

```text
centerScore = 1 - normalizedCenterDistance
borderScore = 1 - clamp(borderTouchRatio / 0.05, 0, 1)
areaScore   = min(areaRatio / 0.20, 1)
objectScore = 0.55 * centerScore + 0.30 * borderScore + 0.15 * areaScore
```

6. 选择 `objectScore` 最高的连通域作为主前景，并记录所选极性、面积比例、中心距离和边界接触比例。

无有效连通域时返回 `foreground_not_found`，不得回退为整块 ROI。第一版假设一个训练 ROI 内存在一个占主导的连通目标；多个互相分离且面积接近的零件应分别框选，或在后续特征版本中扩展多部件聚合。

## 姿态与尺度归一化

前景归一化的固定输出为 `128 x 128`：

1. 使用 `smallest_rectangle2` 得到目标中心、方向和两条半轴，长边统一为水平方向。
2. 通过 `vector_angle_to_rigid`、`affine_trans_image` 和 `affine_trans_region` 把目标旋转到标准方向并移到规范中心。
3. 对 180 度方向歧义，比较两个候选规范图的 4x4 前景占用向量，选择字典序较小者。长短边差异小于 5% 时，同时比较 0、90、180、270 度四个候选，以稳定正方形和近等轴目标的方向。
4. 按变换后前景的最小包围框向外增加 8% 留白，裁剪后使用 `zoom_image_size` 和 `zoom_region` 缩放到 `128 x 128`。

原始 ROI 宽高、ROI 面积、前景在 ROI 中的绝对重心和原始方向不进入特征。目标外形比例由独立形状特征保留，因此规范图缩放不会丢失长宽比信息。

## V2 特征契约

`featureVersion` 固定为 `halcon_registered_feature_v2`，总长度固定为 59。训练和推理的名称、顺序、变换和权重必须完全一致。

### 形状组，13 维，组权重 0.35

- `shapeAspectShortLong`
- `shapeFillRatio`
- `shapeCircularity`
- `shapeCompactnessReciprocal`
- `shapeConvexity`
- `shapeRectangularity`
- `shapeAnisometryReciprocal`
- `shapeBulkiness`
- `shapeStructureFactor`
- `shapeMomentPsi1` 至 `shapeMomentPsi4`

比例、圆度、凸度和矩形度统一限制到 `[0,1]`；长宽比使用短边/长边；紧致度和各向异性使用倒数。四个中心不变矩使用 `0.5 + 0.5 * x / (1 + abs(x))` 映射到 `[0,1]`。

### 空间占用组，16 维，组权重 0.25

- `occupancyR0C0` 至 `occupancyR3C3`

把规范前景划分为 4x4 网格，每维为前景与网格交集面积除以网格面积。该组用于补充不变矩无法表达的局部轮廓差异。

### 灰度组，18 维，组权重 0.15

- `grayMean`
- `grayDeviation`
- `grayHist00` 至 `grayHist15`

均值和标准差除以 255；16 档直方图按前景像素总数归一化，所有 bin 之和为 1。原始最小值和最大值不保留，降低孤立高光和暗点影响。

### 颜色组，6 维，组权重 0.15

- `labMeanL`、`labMeanA`、`labMeanB`
- `labDeviationL`、`labDeviationA`、`labDeviationB`

使用 `decompose3` 和 `trans_from_rgb(..., 'cielab')`，只统计规范前景域；输出按 byte 图像范围除以 255。灰度输入通过三通道桥接后得到中性 Lab 色度，不单独切换特征版本。

### 纹理组，6 维，组权重 0.10

- `textureEntropy`
- `textureAnisotropy`
- `coocEnergy`
- `coocCorrelation`
- `coocHomogeneity`
- `coocContrast`

`cooc_feature_image` 固定使用 `LdGray=4`，即区分 16 个灰度级，并使用 `Direction='mean'` 让 HALCON 对 0、45、90、135 度四个方向取均值，以减小旋转影响。各值按定义范围归一化或通过 `x / (1 + abs(x))` 做有界映射。

### 向量加权

每个特征先按以上规则变为有限有界值。组内每维乘以 `sqrt(groupWeight / groupDimension)`，最后对完整 59 维向量做 L2 归一化。HALCON KNN 的自动 normalization 固定关闭，确保样本 KNN、中心 KNN 和类内半径处于同一个距离空间。

任何特征无法得到有限值时返回 `invalid_feature_value`；不得用静默的零向量继续训练或推理。修改维度、顺序、映射、规范尺寸或组权重时必须升级 `featureVersion`。

## HALCON 双 KNN 训练

训练前必须满足：

- 至少两个非空类别。
- 每个类别至少一个有效样本。
- 每个样本 ROI 有效、前景可提取、V2 特征长度为 59 且 L2 范数非零。

样本 KNN：

1. `create_class_knn(59)`。
2. 每个注册样本调用 `add_sample_class_knn`，ClassID 使用稳定类别 ID。
3. `train_class_knn` 设置 `normalization='false'`、`num_trees=4`。
4. `set_params_class_knn` 设置 `method='classes_distance'`、`k=有效样本总数`、`max_num_classes=类别数`、`num_checks=0`、`epsilon=0.0`。
5. `write_class_knn` 保存为 `model.gnc`。

类别中心 KNN：

1. 对每类已归一化样本向量求平均，再做一次 L2 归一化，得到一个类别中心。
2. `create_class_knn(59)`，每类中心作为一个样本加入相同 ClassID。
3. 训练参数与样本 KNN 相同；`k` 和 `max_num_classes` 都设为类别数。
4. 保存为 `class_centers.gnc`。

类别半径使用样本到本类中心的 L2 距离：

```text
radius = max(maxTrainingDistance * 1.10,
             meanTrainingDistance + 2.5 * populationStdDev)
radius = clamp(radius, 0.10, 2.00)
```

每类少于 3 个样本时 `radiusEnabled=false`，只执行相似度和类别分差拒识，并在训练报告中警告“类内半径样本不足”。训练成功前两个 KNN 句柄均由作用域清理器负责；成功写盘后也立即调用 `clear_class_knn`。

## 推理、评分与拒识

推理时分别对查询特征调用两个 `classify_class_knn`。样本 KNN 返回每类最近注册样本距离 `dSample`，中心 KNN 返回每类中心距离 `dCenter`。

由于所有向量均为单位向量，距离转相似度使用固定公式：

```text
sampleSimilarity = clamp(1 - dSample^2 / 2, 0, 1)
centerSimilarity = clamp(1 - dCenter^2 / 2, 0, 1)
classSimilarity  = 0.70 * sampleSimilarity + 0.30 * centerSimilarity
score            = classSimilarity * 100
```

按 `classSimilarity` 降序得到 Top1 和 Top2；相似度完全相同时按类别 ID 升序，保证结果稳定。拒识顺序固定为：

1. `Top1 score < minSimilarity`：输出 `UNKNOWN`，状态 `classification_rejected_low_similarity`。
2. `Top1 score - Top2 score < minMargin`：输出 `UNKNOWN`，状态 `classification_rejected_ambiguous`。
3. 最佳类别已启用半径且 `dCenter > classRadius`：输出 `UNKNOWN`，状态 `classification_rejected_out_of_radius`。
4. 三项均通过：输出最佳类别。

默认 `minSimilarity=80`、`minMargin=8`，范围均为 0～100。`topK` 只控制 UI/payload 展示数量，不改变内部至少计算 Top2 和全部类别拒识所需距离的行为。

`UNKNOWN` 属于一次成功完成的算法推理：`success=true`、`ok=false`、`predictedLabel='UNKNOWN'`、`predictedClassId=-1`，同时保留最佳已知候选分数、TopK 和拒识诊断。只有模型、图像、ROI、HALCON 或特征链路错误才令 `success=false`。

结果判断在拒识之后执行：

- `class_match`：只有已知类别等于期望类别时 OK。
- `min_score`：只有结果不是 `UNKNOWN` 且分数达到 `judgeRule.minScore` 时 OK。
- `UNKNOWN` 在所有判断模式下均为 NG。

## 配置语义

`ToolConfig.params.registeredClassification` 使用：

```text
version = 2
modelType = "halcon_knn_registered_classification"
modelPath
modelName
detectRegionType = "full" | "rectangle"
roiNormalized
topK = 1
minSimilarity = 80
minMargin = 8
enablePositionCorrection
positionCorrectionSource
```

现有“全部”页的 `最小相似度` 接入 `minSimilarity`，默认值从 68 调整为 80；新增 `最小类别差值` 对应 `minMargin`。`judgeRule.minScore` 仍只属于结果判断，不与模型拒识阈值混用。

## 模型包 schema 2

```text
ModelFiles/RegisteredClass/yyyyMMdd/model_HHmmss_zzz/
├── model.gnc
├── class_centers.gnc
├── metadata.json
├── class_stats.json
├── training_report.json
└── training_session/
    ├── session.json
    └── images/*.png
```

`metadata.json` 至少包含：

- `modelType="halcon_knn_registered_classification"`
- `schemaVersion=2`
- `featureVersion="halcon_registered_feature_v2"`
- `featureLength=59`
- HALCON 版本、类别 ID/名称、有效样本总数和每类样本数。
- 规范尺寸、分割参数、59 个特征名、组权重。
- KNN 参数和默认拒识参数。

`class_stats.json` 按类别保存中心半径、`radiusEnabled`、样本数、训练距离均值/标准差/最大值。`training_report.json` 保存有效/无效 ROI、每类样本数、各阶段耗时、警告和失败详情。训练会话目录结构和恢复协议保持不变。

模型写入继续使用临时目录加原子替换：新注册生成新的日期/时间目录；从模型管理执行“重新训练”时更新选中模型的原目录。任一步骤失败都保留原模型包。

V2 导入和运行以完整模型包目录为单位，必须同时存在两个 `.gnc`、`metadata.json` 和 `class_stats.json`。单独导入 `model.gnc` 或 `class_centers.gnc` 无法恢复另一组距离、类别映射和半径，必须返回 `model_package_incomplete`，不得退化为单 KNN 推理。

## 旧模型与重新训练

本轮是硬替换，不保留 MLP 运行分支：

- `modelType=halcon_mlp_registered_classification` 或只含 `model.gmc` 的包标记为“旧版，需重新训练”。运行返回 `legacy_model_requires_retraining`。
- 旧模型若带有效 `training_session`，点击“重新训练”恢复原图、图像命名、类别和 ROI；训练成功后在同一目录原子升级为 schema 2，并移除旧 `model.gmc`。
- 旧模型没有训练会话时，打开空白训练窗口并明确提示需要重新添加注册图和 ROI。
- 不自动把旧 28 维特征解释为 V2，也不从 MLP 权重伪造注册样本。

模型管理仍可列出旧模型，允许重新训练、导出和删除，但旧模型不能被设为当前可运行模型。

## 资源生命周期

运行时每次加载 `model.gnc` 和 `class_centers.gnc` 后，两个句柄都由独立的作用域清理器管理。正常返回、拒识、异常和中途错误都必须各调用一次 `clear_class_knn`。HALCON 图像、区域和 tuple 同样按作用域释放。

第一版不跨运行缓存 KNN 句柄，因此切换模型、注册新模型或关闭窗口时不存在遗留分类器句柄。后续若增加缓存，缓存键必须包含模型绝对路径和两个 `.gnc` 文件修改时间，替换缓存前先清理旧的两个句柄。

## 错误与 payload

新增或收敛的错误状态：

- `unsupported_model_type`
- `legacy_model_requires_retraining`
- `model_package_incomplete`
- `knn_model_load_failed`
- `knn_model_mismatch`
- `foreground_not_found`
- `invalid_feature_value`
- `knn_classify_failed`
- `classification_rejected_low_similarity`
- `classification_rejected_ambiguous`
- `classification_rejected_out_of_radius`

payload 在现有字段基础上增加：

- `featureVersion`、`featureLength`
- `foregroundPolarity`、`foregroundAreaRatio`
- `sampleSimilarity`、`centerSimilarity`、`centerDistance`
- `secondLabel`、`secondScore`、`scoreMargin`
- `minSimilarity`、`minMargin`
- `classRadius`、`radiusEnabled`
- `rejected`、`rejectionReason`

不得把 HALCON 距离直接标为置信度；UI 展示的 0～100 分数只能使用本文定义的融合相似度。

## 验证与验收

### 算法 smoke

- 本机 HALCON 符号、runtime 和 license 可用时能创建、训练、写入、读取、分类并清理两个 KNN。
- 同一合成目标在 0/45/90/180 度、0.7/1.0/1.3 倍尺寸和不同 ROI 留白下仍预测为同类。
- 同一张图中同类多个不同大小、不同方向 ROI 均作为独立训练样本写入 `model.gnc`。
- 圆、矩形、多边形以及同形不同色样本可由形状和 Lab 特征区分。
- 低相似度、Top1/Top2 接近和超出类内半径分别命中三个拒识状态。
- 每类 1 个样本可生成模型但半径关闭；每类 3 个及以上样本生成有效半径。
- 前景为空、ROI 越界、特征非有限值、模型缺文件、两个 KNN 类别不一致和 HALCON 符号缺失均返回明确错误且不泄漏句柄。

### 模型与 UI 回归

- 新注册仍写入新的日期目录；重新训练仍原子更新选中目录。
- 重新训练恢复图像名称、类别、ROI 和训练图。
- schema 1 MLP 在模型管理中显示旧版状态，不能运行；有训练会话时可原地升级。
- `minSimilarity`、`minMargin`、`topK` 保存、回显并进入 Runner。
- `UNKNOWN` 的 UI 显示、overlay、payload 和 OK/NG 语义一致。
- 模型切换、重新训练、注册新模型和反复测试后无残留 KNN 句柄。
- 运行注册分类 smoke、主 Qt shadow build、`git diff --check`。

## 实施边界

后续实现计划应按以下顺序拆分并逐步验证：

1. V2 前景、规范化和 59 维特征及算法 smoke。
2. schema 2 模型包、HALCON 双 KNN 训练和模型包 smoke。
3. 双 KNN 推理、融合评分、三层拒识和 Adapter 配置。
4. 训练 Dialog、模型管理、旧模型状态、重新训练升级和 UI smoke。
5. 更新注册分类实现文档、算法当前实现说明和提示词规范，再执行完整构建回归。

## 状态

- `[已确认]` 采用 HALCON 原生 KNN 少样本分类，硬替换 MLP，不保留 DL/MLP 推理兼容。
- `[已确认]` 位置模板匹配和位置修正不进入本注册分类算法。
- `[已确认]` 旧模型失效；有训练会话时允许基于原始注册数据重新训练升级。
- `[待实现]` V2 特征、双 KNN 模型包、三层拒识、UI 配置和回归测试。
