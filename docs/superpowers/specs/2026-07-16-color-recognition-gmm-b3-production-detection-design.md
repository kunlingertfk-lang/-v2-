# 颜色识别 GMM B3 生产检测设计

日期：2026-07-16
状态：已确认，待实施
上位设计：`docs/FID/ColorRecognition/颜色识别算法V2整理设计.md`
前置阶段：

- `docs/superpowers/specs/2026-07-15-color-recognition-gmm-b1-design.md`
- `docs/superpowers/specs/2026-07-16-color-recognition-gmm-b2-model-lifecycle-design.md`

## 1. 目标与边界

B3 将 B2 已建立并持久化的 CIELAB GMM 模型接入生产检测，完成以下闭环：

```text
活动模板选择 cielab_gmm
  → Adapter 严格解析模型与检测参数
  → 校验模型状态、签名和序列化产物
  → HALCON CIELAB 像素分类
  → 有效 ROI 内类别面积和 rejection 统计
  → 完整业务判定
  → ToolResult、payload、overlay 和 UI 测试结果
```

B3 实现单后端生产运行，不实现同帧 HSV/GMM 对比测试，不实现 GMM handle 跨帧长期缓存，不修改颜色比较和注册分类链。新建颜色模板继续默认 `hsv_histogram`，避免改变现有用户默认行为；GMM 作为用户显式选择的工业推荐后端。

任一 GMM 错误不得 fallback 到 HSV。HSV 继续沿用现有严格特征签名和类别直方图交集评分。

## 2. 架构

采用扩展现有 `ColorRecognitionGmmHalconBackend` 的方案，新增独立检测入口，共用已经验证的 HALCON Gmm runtime profile、8/16 位图像规范化、CIELAB 转换和模型反序列化逻辑：

```text
ColorRecognitionAdapter
  ├─ hsv_histogram → ColorRecognitionHalconRunner::run()
  └─ cielab_gmm
       → ColorRecognitionHalconRunner::runGmmModel()
       → ColorRecognitionGmmHalconBackend::runModel()
       → ColorRecognitionGmmRunResult
       → ToolResult
```

`ColorRecognitionHalconRunner::run()` 不加入 GMM 分支，保持现有 HSV 入口语义稳定。GMM 的强类型配置、结果和 public facade 放在 `ColorRecognitionHalconRunner.h`，具体 HALCON 实现留在 GMM backend，避免 Adapter 直接持有 HALCON handle。

B3 暂时每次运行严格校验并反序列化模型。连续运行性能和 handle 缓存根据后续 P95 数据单独优化；不得在没有缓存键、生命周期和并发失效设计时提前缓存裸 handle。

## 3. 强类型合同

### 3.1 检测配置

新增 `ColorRecognitionGmmRunConfig`，至少包含：

```text
halconSoPath / halconSoPathCandidates
roiNormalized
detectRegionType
detectCircleCenterNormalized / radius / boundingRect
detectMaskPolygonNormalized
enablePositionCorrection / sourceId / source
pixelFormat / validBits / bitShift
colorChannels: ab | lab
modelState
algorithmVersion / featureSchemaVersion
trainingDataHash / buildParamsHash
serializedGmmBase64 / size / sha256
classIdOrder
labels
gmmRejectionThreshold: 0.0..1.0
judgeMode: min_score | expected_class
minScore: 0..100
minCategoryConfidence: 0..100
minClassifiedCoverage: 0..100
expectedClassId
expectedLabel
```

Adapter 不信任 JSON 中的派生显示字段。`classIdOrder` 必须与模型诊断和当前标签映射一致；类别顺序不得重新按标签文本、UI 顺序或当前面积排序。

### 3.2 类别测量

每类输出稳定结构：

```text
classId
label
classIndex
area
ratio
```

`classIndex` 是 HALCON 输出 region 的 0 基下标，仅用于诊断；业务判断始终使用稳定 `classId`。

### 3.3 检测结果

新增 `ColorRecognitionGmmRunResult`，至少包含：

```text
success / ok / status / message
measurementValid
predictedClassId / predictedLabel
score / rating
predictedClassRatio
categoryConfidence
classifiedCoverage
unclassifiedArea / unclassifiedRatio
effectiveArea / classifiedArea
classMeasurements[]
elapsedMs
overlays
payload
```

HALCON 运行成功但业务阈值不满足时固定为 `success=true`、`measurementValid=true`、`ok=false`、`status=ng`。

## 4. HALCON 算子链

本机 HALCON 20.11.1.1 已确认存在：

```text
classify_image_class_gmm(Image, ClassRegions, GMMHandle, RejectionThreshold)
```

C 接口为：

```text
Herror classify_image_class_gmm(
    const Hobject Image,
    Hobject *ClassRegions,
    Hlong GMMHandle,
    double RejectionThreshold)
```

Gmm runtime profile 在 B1 符号基础上增加：

```text
classify_image_class_gmm
gen_circle
gen_region_polygon_filled
select_obj
intersection
difference
area_center
reduce_domain
get_domain
```

检测链固定为：

```text
cv::Mat 输入容器
  → HALCON interleaved RGB/BGR/RGBA/BGRA image
  → 16 位 bit_rshift + min_max_gray 校验
  → convert_image_type(real) + scale_image 到 [0,1]
  → trans_from_rgb(..., 'cielab')
  → compose2(a,b) 或 compose3(L,a,b)
  → 检测矩形/圆形 ROI
  → difference(ROI, mask) 得到 effectiveRegion
  → reduce_domain(featureImage, effectiveRegion)
  → Base64/size/SHA-256 校验
  → create_serialized_item_ptr + deserialize_class_gmm
  → classify_image_class_gmm
  → select_obj(ClassRegions, classIndex + 1)
  → intersection(classRegion, effectiveRegion)
  → area_center
  → clear_class_gmm / clear_serialized_item / clear_obj
```

OpenCV 只作为输入图像容器，不参与颜色转换、分类、形态学或面积统计。

## 5. 模型与输入校验

进入 HALCON 分类前按顺序校验：

1. 输入图非空且为 8/16 位三或四通道彩色图；Mono 明确不支持。
2. pixelFormat 与通道、位深一致；16 位必须提供有效 `validBits/bitShift`。
3. 位置修正开启时返回 `unsupported_position_correction`。
4. ROI、圆形 ROI 和 mask 合法，effectiveRegion 面积大于 0。
5. modelState 只能为 `ready` 或 `ready_with_warning`。
6. `colorChannels`、算法版本和特征版本与 B1 固定合同一致。
7. `classIdOrder` 非空、无重复，并能唯一映射当前标签和每类建模诊断。
8. 使用当前 trainingDataHash、classIdOrder、类别中心范围、通道和固定训练参数按 B1 完全相同的 canonical JSON 合同重算 `buildParamsHash`，必须与模型保存值一致。
9. serialized Base64、size、SHA-256 和 HALCON 反序列化全部通过。
10. `gmmRejectionThreshold` 在 `[0,1]`；三个判定阈值在 `[0,100]`。

`buildParamsHash` 复算逻辑必须由 B1/B3 共用同一个颜色识别 GMM 内部 helper，不能在 Adapter 或 UI 中复制另一套近似公式。这样 `classIdOrder`、中心范围或通道被篡改时会在分类前失败，避免 HALCON region 被映射到错误业务类别。

`ready_with_warning` 允许运行，但 payload 和 UI 必须持续报告模型样本覆盖不足。检测参数变化不使模型 stale。

## 6. 面积、评分与 rejection

HALCON `classify_image_class_gmm` 返回与 `classIdOrder` 顺序一致的 `NumClasses` 个 region。低于 rejection threshold 的像素不属于任何 region，不额外返回“未分类”region。

统一定义：

```text
effectiveArea       = effectiveRegion 面积
classArea[i]        = intersection 后类别 i 面积
classifiedArea      = sum(classArea[i])
unclassifiedArea    = clamp(effectiveArea - classifiedArea, 0, effectiveArea)
classRatio[i]       = classArea[i] / effectiveArea
classifiedCoverage = clamp(classifiedArea / effectiveArea, 0, 1)
predictedClass      = argmax(classArea[i])
predictedClassRatio = max(classRatio[i])
categoryConfidence  = predictedClassArea / max(classifiedArea, 1)
bestScore           = predictedClassRatio * 100
```

所有面积使用 `area_center` 返回值，不根据图像宽高自行估算。每个 class region 与 effectiveRegion 再取一次交集，防止 HALCON domain 或后续实现变化导致 ROI 外像素进入统计。

`bestScore` 的分母必须是 `effectiveArea`，不能是 `classifiedArea`。当 `classifiedArea=0` 时没有有效预测类别：`predictedClassId=-1`、标签为空、score/categoryConfidence/classifiedCoverage 均为 0，业务结果为 NG，不将某个类别硬选为预测结果。

并列最大面积时按 `classIdOrder` 中更靠前的类别稳定决胜，payload 写入 `predictionTie=true` 和所有并列 classId，便于现场诊断。

对外语义固定为：

```text
scoreMeaning = accepted_predicted_class_pixel_ratio
similarity = bestScore
similarityAliasOf = bestScore
similarityMeaning = accepted_predicted_class_pixel_ratio
```

它不是 GMM 后验概率，也不得与 HSV score 直接比较。

## 7. 完整业务判定

首版默认值：

```text
gmmRejectionThreshold = 0.50
minScore = 80
minCategoryConfidence = 80
minClassifiedCoverage = 90
```

最低分数模式：

```text
ok = predictedClassId >= 0
     && score >= minScore
     && categoryConfidence * 100 >= minCategoryConfidence
     && classifiedCoverage * 100 >= minClassifiedCoverage
```

目标类别模式：

```text
ok = predictedClassId == expectedClassId
     && score >= minScore
     && categoryConfidence * 100 >= minCategoryConfidence
     && classifiedCoverage * 100 >= minClassifiedCoverage
```

失败原因按以下稳定顺序写入 `judgeFailureReasons`，允许同时出现多项：

```text
no_accepted_class
expected_class_mismatch
score_below_minimum
category_confidence_below_minimum
classified_coverage_below_minimum
```

`expectedClassId` 是正式判定键。旧配置只有 `expectedLabel` 时，仅在标签中唯一匹配时迁移；不能唯一匹配则返回 `invalid_expected_class`，不得按当前下拉框第一项猜测。

## 8. 配置与 UI

### 8.1 配置

GMM 检测配置保存在活动模板或工具稳定节点：

```json
{
  "backendConfigs": {
    "cielabGmm": {
      "colorChannels": "ab",
      "maxSamplesPerClass": 10000,
      "gmmRejectionThreshold": 0.5
    }
  },
  "judgeRule": {
    "mode": "min_score",
    "minScore": 80,
    "minCategoryConfidence": 80,
    "minClassifiedCoverage": 90,
    "expectedClassId": 1,
    "expectedLabel": "red"
  }
}
```

`gmmRejectionThreshold` 和三个判定阈值不进入模型 buildParamsHash，修改后可直接重新检测。

### 8.2 UI

颜色识别主 Dialog 在活动模板为 GMM 时显示：

- rejection threshold：`0.00..1.00`，步长 `0.05`，默认 `0.50`。
- 最低分数：`0..100`。
- 最低类别置信度：`0..100`，默认 `80`。
- 最低有效分类覆盖率：`0..100`，默认 `90`。
- 目标类别，保存稳定 `expectedClassId`。

HSV 模式隐藏或禁用 GMM 专属阈值，不读取其值参与 HSV 判定。切换后端保留各自参数。

模型为 empty/stale/invalid/failed 时禁止或拒绝测试运行并显示具体原因；`ready_with_warning` 允许测试，但状态区持续显示样本不足警告。

测试结果至少显示：预测颜色、bestScore、类别置信度、有效分类覆盖率、未分类占比、OK/NG 和耗时。每类面积诊断放在 payload/结果详情，不创建逐像素 overlay。

## 9. payload 与 overlay

GMM 成功测量 payload 至少包含：

```json
{
  "recognitionBackend": "cielab_gmm",
  "algorithm": "halcon_cielab_gmm_classification",
  "algorithmVersion": "halcon_cielab_gmm_classification_v1",
  "featureType": "cielab_ab_gmm",
  "featureSchemaVersion": "cielab_gmm_pixel_classification_v1",
  "measurementValid": true,
  "modelState": "ready",
  "predictedClassId": 1,
  "predictedLabel": "red",
  "bestScore": 91.0,
  "scoreMeaning": "accepted_predicted_class_pixel_ratio",
  "similarity": 91.0,
  "similarityAliasOf": "bestScore",
  "predictedClassRatio": 0.91,
  "categoryConfidence": 0.94,
  "classifiedCoverage": 0.97,
  "unclassifiedRatio": 0.03,
  "effectiveRoiArea": 12000,
  "classifiedArea": 11640,
  "classAreaRatios": [],
  "judgeMode": "min_score",
  "judgeFailureReasons": []
}
```

`classAreaRatios` 按 `classIdOrder` 输出，每项固定包含 `classId/label/classIndex/area/ratio`。不适用于 HSV 的 GMM 指标不得伪造到 HSV payload。

默认 overlay 只包含：

- 检测矩形或圆形 ROI。
- 屏蔽多边形（若配置）。
- 一条结果文本：预测类别、score、类别置信度、分类覆盖率和 OK/NG。

测量失败时不得保留上一帧类别数据或 overlay。

## 10. 错误合同

模型/配置错误：

```text
gmm_model_empty
gmm_model_stale
gmm_model_invalid
gmm_model_failed
model_artifact_invalid
model_artifact_too_large
model_signature_mismatch
invalid_gmm_class_order
invalid_expected_class
invalid_gmm_detection_threshold
```

输入/区域错误：

```text
empty_image
unsupported_mono_for_color_recognition
unsupported_image_type
missing_pixel_format_metadata
invalid_pixel_format_metadata
pixel_value_out_of_range
invalid_roi
masked_roi_empty
unsupported_position_correction
```

HALCON/runtime 错误：

```text
halcon_runtime_not_found
halcon_symbol_profile_missing
halcon_license_error
gmm_deserialize_failed
gmm_classification_failed
```

错误路径必须清理 GMM handle、serialized item 和所有 iconic object。错误结果固定 `success=false`、`measurementValid=false`、`ok=false`，payload 保留 stage、HALCON error code、backend 和模型状态。

## 11. 验证

新增 GMM B3 production smoke，至少覆盖：

1. 红/绿/蓝纯色模型正确预测类别，score、置信度和覆盖率接近 100%。
2. 25/75、50/50、75/25 两色拼接图的类别面积占比误差不超过 2 个百分点。
3. rejection threshold 提高后未分类占比增加，分母仍为 effectiveArea。
4. 全部像素被 rejection 时不产生硬分类，返回 NG 和 `no_accepted_class`。
5. 矩形 ROI、圆形 ROI、mask 后的 effectiveArea 和类别面积正确。
6. minScore、minCategoryConfidence、minClassifiedCoverage 和 expectedClassId 可分别触发对应 NG reason。
7. 多项条件失败时 failure reasons 顺序稳定。
8. 并列类别面积按 classIdOrder 稳定决胜并输出 tie 诊断。
9. 8 位与正确编码 16 位同场景得到一致类别和接近的面积比例。
10. RGB/BGR/RGBA/BGRA 元数据正确时结果一致，alpha 不参与分类。
11. a/b 模型对合理明暗变化保持类别稳定；lab 模型使用 L 通道。
12. 模型序列化前后对同一输入的 classIdOrder、面积和判定一致。
13. empty/stale/invalid/failed、篡改 Base64/hash、错误 classIdOrder 均明确失败。
14. Mono、缺 16 位元数据、越界像素、无效 ROI、空 mask 区域明确失败。
15. 缺 Gmm 检测符号只阻断 GMM，HSV Phase A smoke 继续通过。
16. Adapter 在 GMM ready 时实际运行 GMM，不再返回 B2 unsupported；GMM 失败不调用 HSV。
17. UI 保存、重新打开、切换后端和连续测试正确回显阈值与结果。
18. Qt 主工程、GMM B1、B2 lifecycle 和 HSV Phase A smoke 全部通过。

## 12. 完成标准

- GMM ready/ready_with_warning 模型可从 Adapter 和颜色识别 UI 完成生产检测。
- 面积、score、类别置信度、有效覆盖率和未分类占比符合统一公式。
- 完整判定四项条件均生效，并输出稳定失败原因。
- 8/16 位、ROI/mask、模型异常和 HALCON 异常不崩溃且可定位。
- HSV/GMM 配置、模型、评分和错误路径保持隔离，禁止 fallback。
- 不修改颜色比较和注册分类功能。
- 主工程和相关 smoke 在影子目录构建、运行通过。
