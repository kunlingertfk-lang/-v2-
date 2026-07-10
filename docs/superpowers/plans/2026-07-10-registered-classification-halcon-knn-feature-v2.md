# 注册分类 HALCON KNN 少样本特征 V2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将注册分类从 28 维 HALCON MLP 硬替换为 59 维目标特征、HALCON 双 KNN 和三层拒识，并保持训练会话、模型管理、日期目录与重新训练闭环。

**Architecture:** `RegisteredClassificationFeatureSpace` 固化 V2 特征名称、分组权重、L2 归一化和距离换算；`RegisteredClassificationFeatureExtractor` 只负责 HALCON 前景、姿态/尺度规范化和 59 维特征；`RegisteredClassificationKnnRuntime` 统一封装 HALCON KNN 动态符号与句柄生命周期。TrainingRunner 生成样本 KNN、类别中心 KNN 和类内半径，HalconRunner 融合两组距离并执行拒识，Dialog/Adapter 只传递稳定配置。

**Tech Stack:** C++17、Qt 5.15.2 Core/Widgets/JSON、HALCON 24.11.1 C API 动态加载、OpenCV 4.8 `cv::Mat` 图像桥接、qmake smoke 工程。

## Global Constraints

- 设计依据为 `docs/superpowers/specs/2026-07-10-registered-classification-halcon-knn-feature-v2-design.md`。
- 核心视觉与分类必须使用 HALCON；OpenCV 只用于 `cv::Mat`、测试图生成和格式桥接。
- 最终运行链不得包含 `create_class_mlp`、`train_class_mlp`、`classify_class_mlp`、`read_class_mlp` 或 `write_class_mlp`。
- 最终模型固定为 `modelType="halcon_knn_registered_classification"`、`schemaVersion=2`、`featureVersion="halcon_registered_feature_v2"`、`featureLength=59`。
- KNN 固定使用 `method='classes_distance'`、`normalization='false'`、`num_trees=4`、`num_checks=0`、`epsilon=0.0`。
- 融合权重固定为样本 0.70、类别中心 0.30；默认 `minSimilarity=80`、`minMargin=8`。
- 不实现形状模型匹配、目标定位或位置修正；位置修正 payload 保持 `positionCorrectionApplied=false`。
- 新注册写入新的 `ModelFiles/RegisteredClass/yyyyMMdd/model_HHmmss_zzz/`；重新训练原子更新选中目录。
- schema 1 MLP 只允许被识别为 legacy 并重新训练，不允许进入推理。
- 不修改 `docs/FID/RegisteredClassification/tempFunc.md`、`projects/scheme_0d5611a7/reference.png`、`projects/scheme_0d5611a7/scheme.json` 的现有用户改动。
- 所有手工代码修改使用 `apply_patch`；构建使用 `build/` 影子目录；构建产物不入库。

---

## File Map

**Create:**

- `src/algorithms/recognition/RegisteredClassificationFeatureSpace.h`：V2 固定契约和向量空间 API。
- `src/algorithms/recognition/RegisteredClassificationFeatureSpace.cpp`：59 维名称、权重、L2、中心、距离、半径实现。
- `src/algorithms/recognition/RegisteredClassificationKnnRuntime.h`：HALCON KNN build/classify 请求与结果接口。
- `src/algorithms/recognition/RegisteredClassificationKnnRuntime.cpp`：动态符号、tuple、双句柄 RAII、`.gnc` 读写与分类。
- `smoke/registered_classification_feature_v2_smoke.cpp`：特征契约、前景和尺度/旋转回归。
- `smoke/registered_classification_feature_v2_smoke.pro`：独立特征 smoke 构建。

**Rename:**

- `smoke/registered_classification_mlp_backend_smoke.cpp` -> `smoke/registered_classification_knn_backend_smoke.cpp`。
- `smoke/registered_classification_mlp_backend_smoke.pro` -> `smoke/registered_classification_knn_backend_smoke.pro`。

**Modify:**

- `src/algorithms/recognition/RegisteredClassificationModelPackage.{h,cpp}`：schema 2 元数据、类别统计、完整包检查和 legacy inspection。
- `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.{h,cpp}`：HALCON V2 前景、规范化、59 维提取。
- `src/algorithms/recognition/RegisteredClassificationTrainingRunner.{h,cpp}`：双 KNN 训练、类别中心/半径、原子模型包。
- `src/algorithms/recognition/RegisteredClassificationHalconRunner.{h,cpp}`：双 KNN 推理、融合评分、UNKNOWN 和三层拒识。
- `src/tooladapters/RegisteredClassificationAdapter.cpp`：schema 2 配置解析。
- `src/RegisteredClassificationDialog.{h,cpp}`：KNN 模型类型、80/8 阈值、完整包导入。
- `src/RegisteredClassificationTrainingDialog.cpp`：矩形/多边形区域直传、KNN 请求和训练文案。
- `src/RegisteredClassificationModelManagementDialog.cpp`：新旧模型并列、legacy 禁用使用、重新训练升级。
- `smoke/registered_classification_adapter_smoke.{cpp,pro}`：Adapter V2 配置和错误路径。
- `smoke/registered_classification_dialog_smoke.{cpp,pro}`：保存回显、模型管理、训练恢复和升级。
- `qt_ui_test.pro`：新增 FeatureSpace 和 KnnRuntime 源/头文件。
- `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`：最终流程和字段。
- `docs/FID/RegisteredClassification/注册分类算法当前实现说明.md`：实际算法、模型包、错误码和验证结果。
- `docs/FID/RegisteredClassification/注册分类提示词规范.md`：后续开发固定 KNN V2 约束。

---

### Task 1: 固化 V2 特征空间与 schema 2 模型包合同

**Files:**

- Create: `src/algorithms/recognition/RegisteredClassificationFeatureSpace.h`
- Create: `src/algorithms/recognition/RegisteredClassificationFeatureSpace.cpp`
- Create: `smoke/registered_classification_feature_v2_smoke.cpp`
- Create: `smoke/registered_classification_feature_v2_smoke.pro`
- Modify: `src/algorithms/recognition/RegisteredClassificationModelPackage.h:1-68`
- Modify: `src/algorithms/recognition/RegisteredClassificationModelPackage.cpp:1-246`

**Interfaces:**

- Produces: `registeredClassificationKnnModelType()`, `registeredClassificationLegacyMlpModelType()`, `registeredClassificationFeatureVersionV2()`, `registeredClassificationFeatureNamesV2()`。
- Produces: `normalizeRegisteredClassificationFeature(QVector<double>*)`, `registeredClassificationFeatureDistance(...)`, `registeredClassificationSimilarityFromDistance(...)`, `registeredClassificationClassCenter(...)`, `registeredClassificationRadiusStats(...)`。
- Produces: `RegisteredClassificationKnnModelMetadata`、`RegisteredClassificationClassStats`、`RegisteredClassificationClassStatsDocument`、`RegisteredClassificationModelInspection`。
- Produces: `writeRegisteredClassificationKnnMetadata(...)`、`readRegisteredClassificationKnnMetadata(...)`、`writeRegisteredClassificationClassStats(...)`、`readRegisteredClassificationClassStats(...)` 和非严格的 `inspectRegisteredClassificationModelPackage(...)`。
- Produces: `registeredClassificationSampleKnnPath(...)`、`registeredClassificationCenterKnnPath(...)`、`registeredClassificationClassStatsPath(...)`。
- Produces: `validateRegisteredClassificationKnnPackage(const QString &modelDir)`，要求 metadata、class_stats 和两个 `.gnc` 完整一致。

- [ ] **Step 1: 写 FeatureSpace 和模型包失败测试**

在 `smoke/registered_classification_feature_v2_smoke.cpp` 先加入以下核心断言；测试必须只依赖 Qt Core、ModelPackage 和 FeatureSpace：

```cpp
check(registeredClassificationKnnModelType()
      == QStringLiteral("halcon_knn_registered_classification"),
      "V2 model type must be HALCON KNN");
check(registeredClassificationFeatureVersionV2()
      == QStringLiteral("halcon_registered_feature_v2"),
      "V2 feature version must be stable");
check(registeredClassificationFeatureNamesV2().size() == 59,
      "V2 feature contract must contain 59 names");

QVector<double> unit(59, 1.0);
check(normalizeRegisteredClassificationFeature(&unit),
      "non-zero V2 vector must normalize");
check(qAbs(registeredClassificationFeatureDistance(unit, unit)) < 1e-9,
      "identical vectors must have zero distance");
check(qAbs(registeredClassificationSimilarityFromDistance(0.0) - 1.0) < 1e-9,
      "zero distance must map to similarity one");

RegisteredClassificationKnnModelMetadata metadata;
metadata.modelType = registeredClassificationKnnModelType();
metadata.schemaVersion = 2;
metadata.featureVersion = registeredClassificationFeatureVersionV2();
metadata.featureNames = registeredClassificationFeatureNamesV2();
metadata.featureLength = metadata.featureNames.size();
metadata.classLabels = {{0, QStringLiteral("A")}, {1, QStringLiteral("B")}};
metadata.trainingSampleCount = 6;
check(writeRegisteredClassificationKnnMetadata(modelDir, metadata).success,
      "schema 2 metadata must write");
```

同时覆盖：特征名唯一、五组维数为 13/16/18/6/6、权重和为 1、零向量拒绝、半径少于 3 样本时关闭、schema/featureLength 错误、class stats round-trip、只含 schema 1 metadata 时 inspection 返回 `legacy_model_requires_retraining`。

- [ ] **Step 2: 运行测试确认缺少 V2 API**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile
make -C build -f registered_classification_feature_v2.Makefile -j8
```

Expected: 编译失败，错误包含 `registeredClassificationKnnModelType was not declared` 或 `RegisteredClassificationKnnModelMetadata does not name a type`。

- [ ] **Step 3: 实现 FeatureSpace 固定合同**

`RegisteredClassificationFeatureSpace.h` 使用以下公开合同：

```cpp
struct RegisteredClassificationRadiusStats
{
    bool enabled = false;
    double radius = 0.0;
    double meanDistance = 0.0;
    double stdDevDistance = 0.0;
    double maxDistance = 0.0;
};

QString registeredClassificationFeatureVersionV2();
QStringList registeredClassificationFeatureNamesV2();
QVector<int> registeredClassificationFeatureGroupDimensions();
QVector<double> registeredClassificationFeatureGroupWeights();
bool normalizeRegisteredClassificationFeature(QVector<double> *feature);
double registeredClassificationFeatureDistance(const QVector<double> &left,
                                               const QVector<double> &right);
double registeredClassificationSimilarityFromDistance(double distance);
QVector<double> registeredClassificationClassCenter(
        const QVector<QVector<double>> &features);
RegisteredClassificationRadiusStats registeredClassificationRadiusStats(
        const QVector<QVector<double>> &features,
        const QVector<double> &center);
```

实现必须：验证 59 维和有限值；对完整向量做 L2；距离长度不一致时返回 `NaN`；相似度使用 `clamp(1-d*d/2, 0, 1)`；半径仅在样本数 >= 3 时启用，并使用设计中的 `max(maxD*1.10, mean+2.5*populationStdDev)` 与 `[0.10,2.00]` 限制。

- [ ] **Step 4: 实现 schema 2 元数据、类别统计和 inspection**

在 ModelPackage 中新增并最终保留：

```cpp
struct RegisteredClassificationKnnParams {
    int numTrees = 4;
    int numChecks = 0;
    double epsilon = 0.0;
    double sampleWeight = 0.70;
    double centerWeight = 0.30;
};

struct RegisteredClassificationKnnThresholds {
    int minSimilarity = 80;
    int minMargin = 8;
};

struct RegisteredClassificationClassStats {
    int classId = -1;
    int sampleCount = 0;
    bool radiusEnabled = false;
    double radius = 0.0;
    double meanDistance = 0.0;
    double stdDevDistance = 0.0;
    double maxDistance = 0.0;
};

struct RegisteredClassificationKnnModelMetadata {
    QString modelType;
    int schemaVersion = 2;
    QString featureVersion;
    QString halconVersion;
    QVector<RegisteredClassificationClassLabel> classLabels;
    QStringList featureNames;
    int featureLength = 0;
    QJsonObject segmentation;
    QJsonObject canonicalization;
    QJsonObject featureGroups;
    RegisteredClassificationKnnParams knn;
    RegisteredClassificationKnnThresholds thresholds;
    int trainingSampleCount = 0;
};

struct RegisteredClassificationClassStatsDocument {
    int schemaVersion = 2;
    QString featureVersion;
    QVector<RegisteredClassificationClassStats> classes;
};

struct RegisteredClassificationModelInspection {
    bool success = false;
    bool runnable = false;
    bool legacy = false;
    bool hasTrainingSession = false;
    QString status;
    QString message;
    QString modelType;
    QString featureVersion;
    int classCount = 0;
    int trainingSampleCount = 0;
    RegisteredClassificationKnnModelMetadata metadata;
};
```

路径函数固定返回 `model.gnc`、`class_centers.gnc`、`metadata.json`、`class_stats.json`、`training_report.json`。strict reader 遇到 schema 1 返回 `legacy_model_requires_retraining`；inspection 仍解析旧包的类别数、样本数、featureVersion 和训练会话存在性，但设置 `runnable=false`、`legacy=true`。

- [ ] **Step 5: 构建并运行合同 smoke**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile
make -C build -f registered_classification_feature_v2.Makefile -j8
./build/smoke/registered_classification_feature_v2/bin/registered_classification_feature_v2_smoke
```

Expected: `registered_classification_feature_v2_smoke: feature contract and schema 2 package checks passed`，退出码 0。

- [ ] **Step 6: 提交合同层**

```bash
git add src/algorithms/recognition/RegisteredClassificationFeatureSpace.h \
        src/algorithms/recognition/RegisteredClassificationFeatureSpace.cpp \
        src/algorithms/recognition/RegisteredClassificationModelPackage.h \
        src/algorithms/recognition/RegisteredClassificationModelPackage.cpp \
        smoke/registered_classification_feature_v2_smoke.cpp \
        smoke/registered_classification_feature_v2_smoke.pro
git commit -m "feat: add registered classification v2 feature contract"
```

---

### Task 2: 用 HALCON 实现目标前景、姿态尺度归一化和 59 维特征

**Files:**

- Modify: `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h:1-42`
- Modify: `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp:1-643`
- Modify: `smoke/registered_classification_feature_v2_smoke.cpp`
- Modify: `smoke/registered_classification_feature_v2_smoke.pro`

**Interfaces:**

- Consumes: Task 1 FeatureSpace API。
- Produces: `RegisteredClassificationFeatureRegion` and `RegisteredClassificationFeatureExtractor::extractV2(...)`。
- Produces payload: `foregroundPolarity`、`foregroundAreaRatio`、`foregroundObjectScore`、`canonicalWidth=128`、`canonicalHeight=128`。

- [ ] **Step 1: 写前景、旋转、尺度和多边形失败测试**

扩展 feature smoke，测试图只用 OpenCV 绘制，待测算法仍只调用 HALCON：

```cpp
RegisteredClassificationFeatureRegion region;
region.type = QStringLiteral("rectangle");
region.rectNormalized = QRectF(0.05, 0.05, 0.90, 0.90);

const auto base = extractor.extractV2(makePartImage(0.0, 1.0), region, config);
const auto rotated = extractor.extractV2(makePartImage(45.0, 1.0), region, config);
const auto scaled = extractor.extractV2(makePartImage(90.0, 0.70), region, config);
check(base.success && rotated.success && scaled.success,
      "rotation and scale variants must extract");
check(base.feature.size() == 59, "V2 extractor must return 59 values");
check(allFinite(base.feature), "V2 features must be finite");
check(qAbs(vectorNorm(base.feature) - 1.0) < 1e-6,
      "V2 output must be L2 normalized");
```

增加以下断言：亮/暗极性都能提取；矩形与圆的特征距离大于同一矩形旋转后的距离；同色不同形和同形不同色均产生非零距离；polygon ROI 使用真实 polygon 而非 bounding rect；均匀图返回 `foreground_not_found`；非 HALCON so 返回 `halcon_symbol_missing`。

- [ ] **Step 2: 运行测试确认 `extractV2` 不存在**

```bash
make -C build -f registered_classification_feature_v2.Makefile -j8
```

Expected: 编译失败，错误包含 `RegisteredClassificationFeatureRegion was not declared` 或 `has no member named extractV2`。

- [ ] **Step 3: 扩展 FeatureExtractor 公共结果和区域输入**

头文件加入：

```cpp
struct RegisteredClassificationFeatureRegion
{
    QString type = QStringLiteral("rectangle"); // full | rectangle | polygon
    QRectF rectNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> polygonNormalized;
};

struct RegisteredClassificationFeatureResult
{
    bool success = false;
    QString status;
    QString message;
    QVector<double> feature;
    QStringList featureNames;
    QRect roiPixels;
    QString foregroundPolarity;
    double foregroundAreaRatio = 0.0;
    double foregroundObjectScore = 0.0;
    QJsonObject payload;
};

RegisteredClassificationFeatureResult extractV2(
        const cv::Mat &image,
        const RegisteredClassificationFeatureRegion &region,
        const RegisteredClassificationFeatureConfig &config) const;
```

本任务暂时保留旧 `extract(image,QRectF,config)` 仅保证旧调用点可编译；Task 3 切换完所有调用后删除旧实现，不形成最终兼容分支。

- [ ] **Step 4: 解析并绑定全部 HALCON V2 符号**

在 `HalconFeatureApi` 中加入设计已确认的 `T_gauss_filter`、`T_binary_threshold`、`T_opening_circle`、`T_closing_circle`、`T_fill_up`、`T_connection`、`T_intersection`、`T_difference`、`T_smallest_rectangle2`、`T_vector_angle_to_rigid`、`T_affine_trans_image`、`T_affine_trans_region`、`T_zoom_image_size`、`T_zoom_region`、`T_circularity`、`T_compactness`、`T_convexity`、`T_rectangularity`、`T_eccentricity`、`T_moments_region_central_invar`、`T_decompose3`、`T_trans_from_rgb`、`T_entropy_gray`、`T_cooc_feature_image` 和 polygon region 符号。任一必要符号缺失统一返回 `halcon_symbol_missing`，错误文本必须包含符号名。

- [ ] **Step 5: 实现双极性前景和候选评分**

按设计实现并把所有中间 `Hobject` 包入现有/新增 RAII guard。评分代码固定为：

```cpp
const double centerScore = 1.0 - normalizedCenterDistance;
const double borderScore = 1.0 - qBound(0.0, borderTouchRatio / 0.05, 1.0);
const double areaScore = qMin(areaRatio / 0.20, 1.0);
const double objectScore = 0.55 * centerScore
                         + 0.30 * borderScore
                         + 0.15 * areaScore;
```

无 2%～98% 面积候选时返回 `foreground_not_found`，不得把整 ROI 当作目标。

- [ ] **Step 6: 实现规范图和 59 维特征**

规范尺寸固定 128x128、外扩 8%；长边水平；比较 180 度候选，近等轴目标比较四个四分之一转角并用量化到 `1e-6` 的 occupancy 向量字典序决定方向。按 13/16/18/6/6 顺序填充特征，调用 Task 1 的组权重和 L2 API；`cooc_feature_image` 固定 `LdGray=4`、`Direction='mean'`。长度不是 59、出现非有限值或 L2 为零时返回 `invalid_feature_value`。

- [ ] **Step 7: 运行 V2 feature smoke**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile
make -C build -f registered_classification_feature_v2.Makefile -j8
./build/smoke/registered_classification_feature_v2/bin/registered_classification_feature_v2_smoke
```

Expected: 所有合同、前景、旋转、尺度、颜色、polygon 和异常断言通过，退出码 0。

- [ ] **Step 8: 提交 V2 特征提取**

```bash
git add src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h \
        src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp \
        smoke/registered_classification_feature_v2_smoke.cpp \
        smoke/registered_classification_feature_v2_smoke.pro
git commit -m "feat: extract registered classification v2 features"
```

---

### Task 3: 硬替换为 HALCON 双 KNN 训练、推理和三层拒识

**Files:**

- Create: `src/algorithms/recognition/RegisteredClassificationKnnRuntime.h`
- Create: `src/algorithms/recognition/RegisteredClassificationKnnRuntime.cpp`
- Modify: `src/algorithms/recognition/RegisteredClassificationTrainingRunner.h:1-48`
- Modify: `src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp:1-669`
- Modify: `src/algorithms/recognition/RegisteredClassificationHalconRunner.h:1-68`
- Modify: `src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp:1-678`
- Modify: `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.{h,cpp}`
- Rename: `smoke/registered_classification_mlp_backend_smoke.{cpp,pro}` to `smoke/registered_classification_knn_backend_smoke.{cpp,pro}`
- Modify: `qt_ui_test.pro:70-155`

**Interfaces:**

- Consumes: Task 1 schema 2/FeatureSpace and Task 2 `extractV2`。
- Produces: `RegisteredClassificationKnnRuntime::buildAndWrite(...)` and `classifyPair(...)`。
- Produces: final `RegisteredClassificationTrainingRequest` without MLP fields。
- Produces: final `RegisteredClassificationHalconConfig` with `minSimilarity` and `minMargin`。

- [ ] **Step 1: 重命名 backend smoke 并写双 KNN 失败测试**

使用 `git mv` 重命名两个 smoke 文件，把 target、输出目录和成功文本中的 `mlp` 改为 `knn`。测试必须覆盖：

```cpp
check(QFileInfo(registeredClassificationSampleKnnPath(modelDir)).exists(),
      "training must create model.gnc");
check(QFileInfo(registeredClassificationCenterKnnPath(modelDir)).exists(),
      "training must create class_centers.gnc");
check(QFileInfo(registeredClassificationClassStatsPath(modelDir)).exists(),
      "training must create class_stats.json");
check(!QFileInfo(QDir(modelDir).filePath(QStringLiteral("model.gmc"))).exists(),
      "V2 package must not contain model.gmc");
```

另外覆盖：每类一个样本可训练且半径关闭；每类三个样本半径启用；同图同类多个不同尺寸/方向 ROI 的样本计数准确；变换后的同类查询仍归同类；三个拒识状态；UNKNOWN 的 `success=true/ok=false`；完整 payload；legacy MLP、缺中心模型、类别不一致、重复推理和 HALCON 符号缺失。

- [ ] **Step 2: 运行 renamed smoke 确认仍引用 MLP**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_knn_backend_smoke.pro -o build/registered_classification_knn_backend.Makefile
make -C build -f registered_classification_knn_backend.Makefile -j8
```

Expected: 编译或断言失败，指出缺少 `.gnc` path API、KNN runtime 或仍生成 `model.gmc`。

- [ ] **Step 3: 实现 KnnRuntime 的 HALCON API 和 RAII**

头文件固定使用以下接口：

```cpp
struct RegisteredClassificationKnnSample {
    QVector<double> feature;
    int classId = -1;
};

struct RegisteredClassificationKnnDistance {
    int classId = -1;
    double distance = 0.0;
};

struct RegisteredClassificationKnnRuntimeConfig {
    QString halconSoPath;
    QStringList halconSoPathCandidates;
};

struct RegisteredClassificationKnnBuildRequest {
    QVector<RegisteredClassificationKnnSample> samples;
    int featureLength = 59;
    int numTrees = 4;
    int k = 1;
    int maxNumClasses = 1;
    QString outputPath;
};

struct RegisteredClassificationKnnRuntimeResult {
    bool success = false;
    QString status;
    QString message;
    QVector<RegisteredClassificationKnnDistance> sampleDistances;
    QVector<RegisteredClassificationKnnDistance> centerDistances;
    QJsonObject payload;
};

class RegisteredClassificationKnnRuntime {
public:
    RegisteredClassificationKnnRuntimeResult buildAndWrite(
            const RegisteredClassificationKnnRuntimeConfig &config,
            const RegisteredClassificationKnnBuildRequest &request) const;
    RegisteredClassificationKnnRuntimeResult classifyPair(
            const RegisteredClassificationKnnRuntimeConfig &config,
            const QString &sampleModelPath,
            const QString &centerModelPath,
            const QVector<double> &feature) const;
};
```

动态绑定 `T_create_class_knn`、`T_add_sample_class_knn`、`T_train_class_knn`、`T_set_params_class_knn`、`T_write_class_knn`、`T_read_class_knn`、`T_classify_class_knn`、`T_clear_class_knn`。两个加载句柄各自用 guard 管理；只在 read/create 成功后 mark；所有 return 路径由析构清理一次。

build 固定先调用 `train_class_knn` 的 `['normalization','num_trees']=['false',request.numTrees]`，再调用 `set_params_class_knn` 的 `['method','k','max_num_classes','num_checks','epsilon']=['classes_distance',request.k,request.maxNumClasses,0,0.0]`，最后 `write_class_knn`。classifyPair 必须校验样本模型和中心模型返回的 classId 集合完全相同、没有重复且至少包含两个类别，否则返回 `knn_model_mismatch`；HalconRunner 随后再校验该集合与 metadata 类别表完全一致。

- [ ] **Step 4: 重写 TrainingRunner 为双 KNN 和 schema 2 原子包**

TrainingSample 改为：

```cpp
struct RegisteredClassificationTrainingSample
{
    cv::Mat image;
    RegisteredClassificationFeatureRegion region;
    int classId = -1;
};

struct RegisteredClassificationTrainingRequest
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString outputModelDir;
    QVector<RegisteredClassificationClassLabel> classLabels;
    QVector<RegisteredClassificationTrainingSample> samples;
    QJsonObject trainingSessionManifest;
    QVector<RegisteredClassificationTrainingSessionAsset> trainingSessionAssets;
    RegisteredClassificationKnnParams knn;
    RegisteredClassificationKnnThresholds thresholds;
};
```

对每类 V2 特征调用 `registeredClassificationClassCenter` 和 `registeredClassificationRadiusStats`；样本模型的 `k=有效样本总数`，中心模型的 `k=类别数`。临时目录必须同时写完两个 `.gnc`、两个 JSON、报告和可选训练会话后才调用现有 `swapModelDirectory`；失败时保留旧目录。报告删除 MLP `Error/ErrorLog`，改为 `classStats`、有效/无效样本、warnings、两个 KNN build 耗时和总耗时。

- [ ] **Step 5: 重写 HalconRunner 的融合评分和拒识**

最终 config/result 字段：

```cpp
int minSimilarity = 80;
int minMargin = 8;

double sampleSimilarity = 0.0;
double centerSimilarity = 0.0;
double centerDistance = 0.0;
double secondScore = 0.0;
double scoreMargin = 0.0;
double classRadius = 0.0;
bool radiusEnabled = false;
bool rejected = false;
QString rejectionReason;
```

按 classId 合并两组距离，计算 `0.70*sampleSimilarity + 0.30*centerSimilarity`，同分按 classId 升序。拒识顺序和状态严格为 `classification_rejected_low_similarity`、`classification_rejected_ambiguous`、`classification_rejected_out_of_radius`；UNKNOWN 保留最佳候选 TopK。非 UNKNOWN 再执行 `class_match` 或 `min_score`；UNKNOWN 始终 NG。

- [ ] **Step 6: 删除 MLP 核心和旧 28 维执行入口**

删除 TrainingRunner/HalconRunner 中所有 MLP function pointer、handle guard、参数和错误码；删除 FeatureExtractor 的旧 `extract(image,QRectF,config)` 与 V1 28 维实现，把 `extractV2` 收敛为唯一公开接口：

```cpp
RegisteredClassificationFeatureResult extract(
        const cv::Mat &image,
        const RegisteredClassificationFeatureRegion &region,
        const RegisteredClassificationFeatureConfig &config) const;
```

同步更新 feature/backend smoke 和两个 Runner 的调用点。ModelPackage 可暂时保留 legacy 字符串读取用于 Task 5，但不得保留任何 MLP HALCON 调用。

- [ ] **Step 7: 更新 qmake 源文件列表**

在 backend smoke 和 `qt_ui_test.pro` 中加入：

```qmake
../src/algorithms/recognition/RegisteredClassificationFeatureSpace.cpp \
../src/algorithms/recognition/RegisteredClassificationKnnRuntime.cpp
```

对应 HEADERS 同步加入两个头文件；移除旧 smoke 文件名。

- [ ] **Step 8: 构建并运行 feature/backend smoke**

```bash
make -C build -f registered_classification_feature_v2.Makefile -j8
./build/smoke/registered_classification_feature_v2/bin/registered_classification_feature_v2_smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_knn_backend_smoke.pro -o build/registered_classification_knn_backend.Makefile
make -C build -f registered_classification_knn_backend.Makefile -j8
./build/smoke/registered_classification_knn_backend/bin/registered_classification_knn_backend_smoke
```

Expected: 两个 smoke 均输出 passed 文本并退出 0；重复运行 backend smoke 仍通过。

- [ ] **Step 9: 确认核心源码没有 MLP HALCON 符号**

```bash
rg -n "T_(create|add_sample|train|write|read|classify|clear)_class_mlp|classify_class_mlp|model\.gmc" \
  src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp \
  src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp \
  src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp
```

Expected: 无输出，退出码 1。

- [ ] **Step 10: 提交 KNN 核心替换**

```bash
git add src/algorithms/recognition/RegisteredClassificationKnnRuntime.h \
        src/algorithms/recognition/RegisteredClassificationKnnRuntime.cpp \
        src/algorithms/recognition/RegisteredClassificationTrainingRunner.h \
        src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp \
        src/algorithms/recognition/RegisteredClassificationHalconRunner.h \
        src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp \
        src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h \
        src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp \
        smoke/registered_classification_knn_backend_smoke.cpp \
        smoke/registered_classification_knn_backend_smoke.pro \
        smoke/registered_classification_mlp_backend_smoke.cpp \
        smoke/registered_classification_mlp_backend_smoke.pro \
        qt_ui_test.pro
git commit -m "feat: replace registered classification MLP with HALCON KNN"
```

---

### Task 4: 接通 Adapter、主 Dialog 和训练 Dialog 的 V2 配置

**Files:**

- Modify: `src/tooladapters/RegisteredClassificationAdapter.cpp:1-128`
- Modify: `src/RegisteredClassificationDialog.h:1-109`
- Modify: `src/RegisteredClassificationDialog.cpp:224-1045`
- Modify: `src/RegisteredClassificationTrainingDialog.cpp:1699-2026`
- Modify: `src/RegisteredClassificationModelManagementDialog.cpp:180-285,480-535`
- Modify: `smoke/registered_classification_adapter_smoke.cpp:1-260`
- Modify: `smoke/registered_classification_adapter_smoke.pro`
- Modify: `smoke/registered_classification_dialog_smoke.cpp:264-1101`
- Modify: `smoke/registered_classification_dialog_smoke.pro`

**Interfaces:**

- Consumes: final Task 3 request/config/result types。
- Produces: `params.registeredClassification.version=2`、`minSimilarity`、`minMargin`、KNN modelType。
- Produces: training polygon region passed losslessly to FeatureExtractor。

- [ ] **Step 1: 更新 Adapter smoke 为 V2 失败测试**

`makeConfig` 增加 `minSimilarity` 和 `minMargin`，并断言 runner payload 收到 80/8。训练 fixture 使用 `RegisteredClassificationFeatureRegion`，检查两个 `.gnc`。增加：schema 1 modelType 返回 `legacy_model_requires_retraining`；`minMargin=100` 产生 ambiguous UNKNOWN；UNKNOWN 在 `min_score` 下仍 NG。

- [ ] **Step 2: 运行 Adapter smoke 确认默认值仍为 MLP**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_adapter_smoke.pro -o build/registered_classification_adapter.Makefile
make -C build -f registered_classification_adapter.Makefile -j8
./build/smoke/registered_classification_adapter/bin/registered_classification_adapter_smoke
```

Expected: 编译或断言失败，指出 modelType/version/minMargin 尚未切换。

- [ ] **Step 3: 修改 Adapter 配置映射**

`toRunnerConfig` 固定：

```cpp
runnerConfig.modelType = stringParam(
        params, QStringLiteral("modelType"), registeredClassificationKnnModelType());
runnerConfig.minSimilarity = qBound(
        0, intParam(params, QStringLiteral("minSimilarity"), 80), 100);
runnerConfig.minMargin = qBound(
        0, intParam(params, QStringLiteral("minMargin"), 8), 100);
```

删除 `rejectScore/top2Gap` 解析；judgeRule 的 `minScore` 继续独立传递。

- [ ] **Step 4: 写主 Dialog 保存/回显失败断言**

在 dialog smoke 断言：默认 version=2、KNN modelType、`minSimilarity=80`、`minMargin=8`；loadFromConfig 后两个值回显；全部页存在 objectName `registeredClassificationMinMarginSpinBox`；完整 KNN 包可导入，缺 `class_centers.gnc` 的包被拒绝。

- [ ] **Step 5: 修改主 Dialog 控件和配置**

头文件新增 `QSpinBox *m_minMarginSpinBox = nullptr;`。参数卡使用：

```cpp
m_modelTypeComboBox->addItem(tr("HALCON KNN 注册分类"),
                             registeredClassificationKnnModelType());
m_minSimilaritySpinBox->setValue(80);
m_minMarginSpinBox = new QSpinBox(m_advancedCard);
m_minMarginSpinBox->setObjectName(
        QStringLiteral("registeredClassificationMinMarginSpinBox"));
m_minMarginSpinBox->setRange(0, 100);
m_minMarginSpinBox->setValue(8);
advancedLayout->addLayout(row(tr("最小类别差值"), m_minMarginSpinBox));
```

保存 version=2 和两个阈值；训练完成/模型选择后设置 KNN modelType；导入调用 strict schema 2 reader并要求两个 `.gnc`、metadata、class_stats 全部存在。

- [ ] **Step 6: 修改训练 Dialog 的请求构造**

删除 MLP 参数和 polygon bounding warning。每个 mark 直接转换：

```cpp
sample.region.type = mark.type;
sample.region.rectNormalized = mark.rect.normalized();
sample.region.polygonNormalized = mark.polygon;
sample.classId = classIndex;
```

全屏 mark 使用 `(0,0,1,1)`；多边形点数小于 3 时不生成样本。preview 的 modelType 改为 KNN，训练状态使用“特征模型生成完成”，保留 `trainingCompleted(modelDir, modelName)` 信号和现有 session manifest/assets。

- [ ] **Step 7: 让模型管理先识别并使用完整 V2 包**

本任务只切换正常 V2 路径：`scanModelRecords()` 要求 metadata、两个 `.gnc` 和 class_stats 完整，使用 strict schema 2 reader；行详情显示 V2 featureVersion；“使用模型”和“重新训练”维持现有交互。schema 1 并列显示、禁用使用和升级行为由 Task 5 加入。

- [ ] **Step 8: 更新两个 smoke `.pro` 并运行**

两个 `.pro` 加入 FeatureSpace 和 KnnRuntime 源/头。执行：

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_adapter_smoke.pro -o build/registered_classification_adapter.Makefile
make -C build -f registered_classification_adapter.Makefile -j8
./build/smoke/registered_classification_adapter/bin/registered_classification_adapter_smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_dialog_smoke.pro -o build/registered_classification_dialog.Makefile
make -C build -f registered_classification_dialog.Makefile -j8
QT_QPA_PLATFORM=offscreen ./build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: Adapter 和 Dialog smoke 全部输出 passed 文本并退出 0；模型管理中的完整 V2 包可使用、可重新训练并回填主 Dialog。

- [ ] **Step 9: 提交 V2 配置链**

```bash
git add src/tooladapters/RegisteredClassificationAdapter.cpp \
        src/RegisteredClassificationDialog.h \
        src/RegisteredClassificationDialog.cpp \
        src/RegisteredClassificationTrainingDialog.cpp \
        src/RegisteredClassificationModelManagementDialog.cpp \
        smoke/registered_classification_adapter_smoke.cpp \
        smoke/registered_classification_adapter_smoke.pro \
        smoke/registered_classification_dialog_smoke.cpp \
        smoke/registered_classification_dialog_smoke.pro
git commit -m "feat: connect registered classification KNN configuration"
```

---

### Task 5: 模型管理记录 V2，并让 legacy 基于训练会话原地升级

**Files:**

- Modify: `src/RegisteredClassificationModelManagementDialog.cpp:1-563`
- Modify: `src/RegisteredClassificationDialog.cpp:420-540,1011-1043`
- Modify: `src/algorithms/recognition/RegisteredClassificationModelPackage.{h,cpp}`
- Modify: `smoke/registered_classification_dialog_smoke.cpp:760-1010`

**Interfaces:**

- Consumes: Task 1 inspection and Task 3 atomic TrainingRunner。
- Produces: `ModelRecord.runnable`、`ModelRecord.legacy`、`ModelRecord.status/message`。
- Produces: legacy row can retrain but cannot emit `modelSelected` before successful upgrade。

- [ ] **Step 1: 写模型扫描和迁移失败测试**

在 dialog smoke 建立三个日期目录 fixture：完整 V2、带 training_session 的 schema 1 MLP、无 session 的 schema 1 MLP。断言：三行都可见；只有 V2 的“使用模型”可用；两个 legacy 的“重新训练”可用；legacy 行显示“旧版，需重新训练”；有 session 的 retrain 恢复图名/类别/ROI；训练成功后同目录出现两个 `.gnc` 且 `model.gmc` 消失；无 session 打开空白窗口并显示既有明确提示。

- [ ] **Step 2: 运行 Dialog smoke 确认 legacy 当前被过滤**

```bash
make -C build -f registered_classification_dialog.Makefile -j8
QT_QPA_PLATFORM=offscreen ./build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: 失败信息指出 legacy row 不存在或 use 按钮未禁用。

- [ ] **Step 3: 让 scanModelRecords 使用 package inspection**

ModelRecord 改为：

```cpp
struct ModelRecord
{
    QString modelDir;
    QString modelName;
    QString dateText;
    RegisteredClassificationModelInspection inspection;
    QDateTime lastModified;
    bool hasTrainingSession = false;
};
```

扫描条件只要求目录含 `metadata.json`；调用 inspection 决定 runnable/legacy。V2 行显示 `halcon_registered_feature_v2`；legacy 行显示其旧 featureVersion 和“旧版，需重新训练”；损坏且不可恢复的目录不显示为可用模型，但保留明确日志。

- [ ] **Step 4: 实现行按钮状态和重新训练行为**

```cpp
useButton->setEnabled(record.inspection.runnable);
useButton->setToolTip(record.inspection.runnable
                      ? QObject::tr("使用模型")
                      : QObject::tr("旧模型不能运行，请先重新训练"));
retrainButton->setEnabled(true);
```

点击 legacy use 不得发射 `modelSelected`。重新训练继续调用 `setUpdateTargetModelDir(record.modelDir)`；成功信号后刷新同一目录并仅在 inspection 已变为 runnable 时发射 `modelSelected`。

- [ ] **Step 5: 收紧主 Dialog 导入和选中行为**

完整 V2 包才可导入。strict reader 返回 `legacy_model_requires_retraining` 时，提示“旧版模型不能直接运行，请在模型管理中重新训练”，不改变当前模型路径。模型管理只会把 runnable V2 回填主 Dialog。

- [ ] **Step 6: 删除过渡 V1 API，保留只读 legacy inspection**

删除 `RegisteredClassificationMlpParams`、V1 feature names、V1 metadata writer 和所有旧模型可运行判断。允许保留的 legacy 内容仅限：字符串 `halcon_mlp_registered_classification`、`model.gmc` 是否存在、schema 1 基本 JSON 读取和 `legacy_model_requires_retraining` 状态。

- [ ] **Step 7: 运行 Dialog、Adapter 和 backend 回归**

```bash
QT_QPA_PLATFORM=offscreen ./build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
./build/smoke/registered_classification_adapter/bin/registered_classification_adapter_smoke
./build/smoke/registered_classification_knn_backend/bin/registered_classification_knn_backend_smoke
```

Expected: 三个 smoke 全部退出 0；legacy 不可使用但可恢复升级；V2 选中后主 Dialog 路径与 modelType 正确。

- [ ] **Step 8: 提交模型管理迁移**

```bash
git add src/RegisteredClassificationModelManagementDialog.cpp \
        src/RegisteredClassificationDialog.cpp \
        src/algorithms/recognition/RegisteredClassificationModelPackage.h \
        src/algorithms/recognition/RegisteredClassificationModelPackage.cpp \
        smoke/registered_classification_dialog_smoke.cpp
git commit -m "feat: migrate managed registered classification models to KNN"
```

---

### Task 6: 更新规范并执行完整验证

**Files:**

- Modify: `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`
- Modify: `docs/FID/RegisteredClassification/注册分类算法当前实现说明.md`
- Modify: `docs/FID/RegisteredClassification/注册分类提示词规范.md`
- Verify: `qt_ui_test.pro`

**Interfaces:**

- Consumes: Tasks 1-5 的最终文件、字段、错误码和验证输出。
- Produces: 后续开发唯一有效的 KNN V2 文档状态。

- [ ] **Step 1: 更新功能实现文档**

把算法主线、字段表和模型包改为：59 维 Feature V2、`model.gnc`、`class_centers.gnc`、`class_stats.json`、80/8 拒识。明确 polygon 使用真实区域；UNKNOWN 是成功推理但 NG；位置修正仍未应用；删除 MLP 当前主线描述。

- [ ] **Step 2: 更新当前实现说明和提示词规范**

当前实现说明记录实际 HALCON 算子、双 KNN 参数、融合公式、三个拒识状态、资源清理和 legacy 升级。提示词规范将默认 modelType/schema/featureVersion 固定为 V2，并禁止后续代码重新引入 MLP/DL 兼容。历史完成记录保留，但标明 MLP 已被本轮硬替换。

- [ ] **Step 3: 重建并运行全部注册分类 smoke**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile
make -C build -f registered_classification_feature_v2.Makefile -j8
./build/smoke/registered_classification_feature_v2/bin/registered_classification_feature_v2_smoke

/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_knn_backend_smoke.pro -o build/registered_classification_knn_backend.Makefile
make -C build -f registered_classification_knn_backend.Makefile -j8
./build/smoke/registered_classification_knn_backend/bin/registered_classification_knn_backend_smoke

/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_adapter_smoke.pro -o build/registered_classification_adapter.Makefile
make -C build -f registered_classification_adapter.Makefile -j8
./build/smoke/registered_classification_adapter/bin/registered_classification_adapter_smoke

/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_dialog_smoke.pro -o build/registered_classification_dialog.Makefile
make -C build -f registered_classification_dialog.Makefile -j8
QT_QPA_PLATFORM=offscreen ./build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: 四个 smoke 都输出 passed 文本并退出 0。

- [ ] **Step 4: 执行主工程 shadow build**

```bash
mkdir -p build/main
cd build/main
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
make -j8
cd ../..
```

Expected: qmake 和 make 退出 0，生成 `build/main/qt_ui_test` 或工程配置的等价目标。

- [ ] **Step 5: 执行静态边界检查**

```bash
rg -n "T_(create|add_sample|train|write|read|classify|clear)_class_mlp|classify_class_mlp" \
  src/algorithms/recognition src/tooladapters/RegisteredClassificationAdapter.cpp \
  src/RegisteredClassificationDialog.cpp src/RegisteredClassificationTrainingDialog.cpp
rg -n "halcon_knn_registered_classification|halcon_registered_feature_v2" \
  src docs/FID/RegisteredClassification smoke
git diff --check -- src smoke qt_ui_test.pro \
  docs/FID/RegisteredClassification/registered_classification_function_implementation.md \
  docs/FID/RegisteredClassification/注册分类算法当前实现说明.md \
  docs/FID/RegisteredClassification/注册分类提示词规范.md
git status --short
```

Expected: 第一条无输出；第二条覆盖 ModelPackage、Runner、Adapter、Dialog、文档和 smoke；限定范围的 `git diff --check` 无输出；status 中不得出现构建产物，且实施提交不得包含任务开始前已修改的 `tempFunc.md`。

- [ ] **Step 6: 手工检查关键 UI 流程**

启动 `build/main/qt_ui_test`，验证：新注册生成日期目录；训练后主 Dialog 自动选中 KNN；模型管理能使用 V2；legacy 使用按钮禁用且重新训练恢复图/名称/类别/ROI；重新训练后路径不变；矩形和 polygon 样本均可训练；基准图连续测试中绘制 ROI 后自动测试；UNKNOWN 显示类别、分数和拒识原因；切换模型与反复测试不崩溃。

- [ ] **Step 7: 提交文档和最终验证记录**

```bash
git add docs/FID/RegisteredClassification/registered_classification_function_implementation.md \
        docs/FID/RegisteredClassification/注册分类算法当前实现说明.md \
        docs/FID/RegisteredClassification/注册分类提示词规范.md
git commit -m "docs: document registered classification KNN backend"
```

- [ ] **Step 8: 最终提交范围审计**

```bash
git log --oneline --decorate -8
git status --short
git diff 7121775..HEAD --stat
```

Expected: `7121775` 之后只包含本计划文档以及本计划列出的源码、smoke、工程和三份 FID 文档；`tempFunc.md` 与两个 `projects/scheme_0d5611a7` 文件仍保持用户原有未提交状态。
