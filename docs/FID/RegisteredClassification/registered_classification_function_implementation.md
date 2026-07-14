# 注册分类功能实现记录

## 文档用途

本文档是当前注册分类功能的实现基线。当前基线以 `7728ac0` 及其之前的 Task 1-5 实现为准，后续 UI、Adapter、训练、模型管理和 Runner 变更必须同时更新本文档与同目录的当前实现说明、提示词规范。

## 当前结论

注册分类当前唯一可运行主线是 HALCON Feature V2 + 双 KNN：

- 模型类型固定为 `halcon_knn_registered_classification`。
- 模型包 schema 固定为 `2`，特征版本固定为 `halcon_registered_feature_v2`，特征长度固定为 `59`。
- 训练和推理均使用 HALCON 核心算子；OpenCV 仅作为 `cv::Mat` 图像输入和格式桥接。
- 当前运行链不包含 MLP 或 DL 推理，也不读取 `model.gmc` 作为可运行模型。

## 端到端链路

```text
注册图 + 类别 + ROI
  -> HALCON ROI / 前景分割
  -> 姿态与尺度规范化
  -> 59 维 Feature V2
  -> 样本 KNN + 类别中心 KNN
  -> class_stats.json 类内半径
  -> schema 2 模型包

输入图 + 检测 ROI
  -> 同一 Feature V2 提取链
  -> 两个 HALCON KNN 分类
  -> 相似度融合
  -> 80 最低相似度、8 最小类别差值、类内半径拒识
  -> 已知类别或 UNKNOWN
  -> judgeRule 输出 OK/NG
```

## UI 和配置闭环

主注册分类 Dialog 保存、回显并传递以下字段：

| 路径 | 当前值或语义 |
| --- | --- |
| `params.registeredClassification.version` | `2` |
| `params.registeredClassification.modelType` | `halcon_knn_registered_classification` |
| `params.registeredClassification.modelPath` | 完整模型包目录，不是单个 `.gnc` 文件 |
| `params.registeredClassification.modelName` | 模型目录名或用户显示名称 |
| `params.registeredClassification.detectRegionType` | `full` 或 `rectangle` |
| `params.registeredClassification.roiNormalized` | 检测矩形 ROI；全屏为 `(0, 0, 1, 1)` |
| `params.registeredClassification.topK` | 仅控制候选 payload/UI 展示数量，默认 `1` |
| `params.registeredClassification.minSimilarity` | 模型拒识阈值，固定默认 `80` |
| `params.registeredClassification.minMargin` | Top1/Top2 类别差值阈值，固定默认 `8` |
| `params.registeredClassification.enablePositionCorrection` | 可保存和回显，但当前不应用 |
| `params.registeredClassification.positionCorrectionSource` | 位置修正来源文本，默认 `1 基准图.位置修正信息` |

`judgeRule.mode` 仍使用 `class_match` 或 `min_score`。`judgeRule.minScore` 是结果判断阈值，不替代模型拒识的 `minSimilarity` 和 `minMargin`。UNKNOWN 在两种判断模式下都为 NG。

主 Dialog 的检测 ROI 当前只支持全屏和矩形。注册训练窗口的样本 ROI 支持全屏、矩形和多边形；训练请求保留多边形点列，不降级为外接矩形。

## Feature V2

Feature V2 使用 HALCON 完成 ROI、前景、规范化和统计。特征名称、顺序、映射、权重和 59 维长度是持久化契约，训练与推理必须完全一致。五组维度和组权重如下：

| 组 | 维度 | 权重 | 内容 |
| --- | ---: | ---: | --- |
| shape | 13 | 0.35 | 形状比例、填充率、圆度、紧致度倒数、凸度、矩形度、各向异性倒数、体积性、结构因子、4 个中心不变矩 |
| occupancy | 16 | 0.25 | 规范化前景的 4x4 网格占用 |
| gray | 18 | 0.15 | 灰度均值、偏差和 16 档直方图 |
| color | 6 | 0.15 | Lab 均值和偏差 |
| texture | 6 | 0.10 | 熵、各向异性及共生矩阵能量/相关性/同质性/对比度 |

组内按 `sqrt(groupWeight / groupDimension)` 加权，最后对完整向量做 L2 归一化。HALCON KNN 的自动 normalization 固定关闭。无效或非有限特征必须返回 `invalid_feature_value`，不得静默改成零向量。

### ROI 和前景

训练样本的全屏、矩形和多边形 ROI 均生成真实 HALCON 区域：矩形使用 `gen_rectangle1`，多边形使用 `gen_region_polygon_filled`。多边形的前景分割、边界带、质心和候选评分都基于真实区域，不能用 bounding rectangle 替代。多边形至少需要三个有效点。

前景流程使用 `reduce_domain`、`gauss_filter`、亮/暗两次 `binary_threshold(..., 'max_separability', ...)`、形态学处理、连通域、面积门限和边界/中心/面积评分。目标区域再经 HALCON 姿态、尺度规范化，输出 `128 x 128` 规范图与区域。

在线检测 ROI 只接受全屏或矩形，因为主 Dialog 当前没有在线多边形检测区域配置。

## 双 KNN 和模型包

训练器生成的 schema 2 目录如下；其中 `training_report.json` 是训练记录，
`training_session/` 仅在存在可恢复训练会话时生成：

```text
ModelFiles/RegisteredClass/yyyyMMdd/model_HHmmss_zzz/
├── model.gnc
├── class_centers.gnc
├── metadata.json
├── class_stats.json
├── training_report.json                 # 训练器生成，非运行最小包校验项
└── training_session/                    # 可选
    ├── session.json
    └── images/*.png
```

运行最小包必须包含 `metadata.json`、`model.gnc`、`class_centers.gnc` 和
`class_stats.json`，不能只导入一个 KNN 文件。`model.gnc` 保存每个有效注册样本的
HALCON KNN；`class_centers.gnc` 保存每个类别的归一化中心。两者都使用 59 维输入、
`method=classes_distance`、`normalization=false`、`num_trees=4`、`num_checks=0`、
`epsilon=0.0`。样本 KNN 的 `k` 为有效样本总数，中心 KNN 的 `k` 为类别数；两者的
`max_num_classes` 为类别数。metadata 保存样本权重 `0.70`、中心权重 `0.30` 和默认
拒识阈值 `80/8`。

每个类别的中心是该类别样本向量的均值再归一化。类内半径为：

```text
radius = max(maxDistance * 1.10,
             meanDistance + 2.5 * populationStdDev)
radius = clamp(radius, 0.10, 2.00)
```

少于 3 个样本时 `radiusEnabled=false` 且 `radius=0`；达到 3 个样本时启用半径。模型包写入临时目录，校验所有文件后原子替换目标目录。

新注册训练写入新的日期/时间目录；模型管理中的重新训练使用选中模型的同一目录原子升级，不另建目录。

## 融合和拒识

对两个 KNN 返回的 L2 距离分别使用：

```text
similarity = clamp(1 - distance^2 / 2, 0, 1)
sampleSimilarity = 100 * sampleSimilarity01
centerSimilarity = 100 * centerSimilarity01
score = 0.70 * sampleSimilarity + 0.30 * centerSimilarity
```

类别按 score 降序排序，分数相同时按 classId 升序。`minSimilarity` 和 `minMargin`
默认分别为 `80` 和 `8`，工具配置可在 `[0,100]` 内覆盖运行时阈值；拒识顺序固定为：

1. `score < minSimilarity`：`classification_rejected_low_similarity`。
2. `score - secondScore < minMargin`：`classification_rejected_ambiguous`。
3. 启用类内半径且 `centerDistance > classRadius`：`classification_rejected_out_of_radius`。

三种状态均表示算法成功完成但拒绝给出已知类别：`success=true`、`ok=false`、`predictedLabel=UNKNOWN`、`predictedClassId=-1`。payload 必须保留最佳已知候选的 `bestCandidateClassId`/`bestCandidateLabel`、分数、TopK、`rejectionReason`、半径和距离诊断。模型缺失、图像为空、ROI 无效、HALCON 错误或特征错误才令 `success=false`。

拒识完成后才执行 `judgeRule`；UNKNOWN 在 `class_match` 和 `min_score` 下均为 NG。

## 资源生命周期和位置修正

HALCON 动态库、tuple、图像/区域对象和两个 KNN 句柄均使用作用域清理。每个已成功创建或读取的 KNN 句柄只清理一次；正常返回、拒识、异常和中途错误都不能泄漏句柄。当前不跨运行缓存 KNN 句柄。

位置修正字段仍可保存、回显并写入 payload，但分类 Runner 不改变 ROI 坐标，不执行补偿；payload 固定表达 `positionCorrectionApplied=false` 和未实现原因。不得把位置修正字段存在误写成已应用。

## 旧模型与模型管理

模型管理扫描真实模型目录并保留可识别的完整 V2、旧 schema 1 和不完整包记录。完整 schema 2 KNN 包可以使用；旧 schema 1/`model.gmc` 记录只能显示为 legacy、导出/删除或重新训练，不能直接运行、不能发出可运行模型选择信号。

带 `training_session/session.json` 的 legacy 模型重新训练时恢复注册图、名称、类别和 ROI；没有训练会话时打开空白训练状态并提示重新添加注册图和 ROI。重新训练成功后在原目录生成 `model.gnc`、`class_centers.gnc`、schema 2 metadata 和统计文件，并移除旧 `model.gmc`，确认包完整后才允许选择。

MLP/DL 仅作为历史模型的识别标签和升级入口存在。不得新增 MLP/DL 推理、模型读取兼容、特征兼容、权重转换或“自动兼容”分支；不得把 legacy 直接降级为当前 KNN 运行。

## 关键错误状态

当前链路至少应明确区分：`invalid_roi`、`foreground_not_found`、`invalid_feature_value`、`model_package_incomplete`、`legacy_model_requires_retraining`、`knn_read_failed`、`knn_model_mismatch`、`knn_classify_failed`、`halcon_symbol_missing`、`classification_rejected_low_similarity`、`classification_rejected_ambiguous` 和 `classification_rejected_out_of_radius`。错误 payload 应包含状态、消息、模型/ROI/耗时等定位信息。

## 历史记录（仅供追溯，不是当前实现）

- 早期 UI/后端阶段曾使用 `halcon_dl_classification`、`model.gmc` 以及固定 28 维 ROI 统计特征；这些内容已经退出当前运行路径。
- 2026-07-09 的中间实现曾完成 HALCON MLP 训练和推理闭环；Task 1-5 本轮已将其硬替换为 schema 2 双 KNN。
- 2026-07-03 至 2026-07-10 的 UI、训练会话、ROI 预览、基准图持续测试和位置修正占位记录仍保留其历史事实，但其模型格式和当前算法语义以本文档前文为准。
