# 颜色识别 GMM B2 模型生命周期设计

日期：2026-07-16  
状态：已确认，待实施  
上位设计：`docs/FID/ColorRecognition/颜色识别算法V2整理设计.md`  
前置阶段：`docs/superpowers/specs/2026-07-15-color-recognition-gmm-b1-design.md`

## 1. 目标与边界

B2 将已完成的 HALCON CIELAB GMM 建模能力接入颜色模板编辑、配置保存、重新打开和模型 stale 管理，形成以下闭环：

```text
选择 GMM 后端
  → 采集无损 ROI 样本
  → 建立并验证 GMM
  → 保存模型产物和严格签名
  → 重新打开并校验
  → 公共样本或 GMM 参数变化后标记 stale
  → 重新建模恢复 ready
```

B2 不实现检测图像分类、GMM rejection、类别面积统计或最终判定。`ColorRecognitionAdapter` 收到 `recognitionBackend=cielab_gmm` 时必须返回 `unsupported_gmm_detection_phase_b2`，不得静默调用 HSV。现有 HSV 生产运行行为保持不变。

颜色比较和注册分类训练链不参与本阶段实现；不复用 `RegisteredClassificationModelPackage`，不抽取公共模型底座。

## 2. 架构选择

采用模板内嵌双后端模型方案。每个颜色模板保存公共标签和原始 ROI 样本，同时分别保存 HSV 与 GMM 的配置和派生模型：

```text
ColorRecognitionTemplateData
  ├─ labels / samples                 # 公共训练事实
  ├─ recognitionBackend              # hsv_histogram | cielab_gmm
  ├─ backendConfigs
  │    ├─ hsvHistogram
  │    └─ cielabGmm
  └─ backendModels
       ├─ hsvHistogram
       └─ cielabGmm
```

不建立外部 GMM 文件路径。HALCON serialized item 以 Base64 保存在模板 JSON 中，随工程和模板导入导出移动，避免路径失效。模型大小继续受 B1 的 8 MiB 上限约束。

## 3. 数据合同

### 3.1 模板公共字段

新增或规范化以下字段：

```text
modelSchemaVersion: 3
recognitionBackend: hsv_histogram | cielab_gmm
labels[]
samples[]
backendConfigs
backendModels
```

旧模板缺少 `recognitionBackend` 时迁移为 `hsv_histogram`。旧的顶层 `featureType`、`sensitivity`、`brightnessEnabled` 读入 `backendConfigs.hsvHistogram`，保存时写入新结构；为兼容当前 Adapter，在 B2 内部可同步回写旧 HSV 字段，但它们不表达 GMM 语义。

### 3.2 公共样本字段

每个新采集样本至少保存：

```text
sampleId: UUID，非空且模板内唯一
classId / label
roiNormalized                         # 原图中的展示/追溯 ROI
roiImagePngBase64                     # 现有 UI 缩略图，仅用于显示
gmmRoiImagePngBase64                  # 从 cv::Mat ROI 无损编码的训练图
gmmImageSha256                        # 对解码后有效像素做规范哈希
gmmImageWidth / gmmImageHeight
pixelFormat
validBits / bitShift
feature / featureSignature            # HSV 派生数据
```

`gmmRoiImagePngBase64` 必须由原始 `cv::Mat` ROI 无损编码：8 位和 16 位通道深度不得经过显示用 `QImage` 降位。OpenCV 只允许承担 ROI 图像容器和 PNG 编解码桥接；颜色转换、训练采样和 GMM 仍全部由 HALCON 完成。

传给 B1 建模接口时，解码后的图像已经是裁剪 ROI，因此 `ColorRecognitionGmmBuildSample.roiNormalized` 固定为整幅 `[0,0,1,1]`。原始 `roiNormalized` 用于 UI 回显；训练图及其 SHA-256 保证 ROI 内容变化进入训练签名。

旧样本只有显示缩略图或 HSV 特征时仍可继续运行 HSV，但不得将显示缩略图冒充 GMM 原始训练数据。建立 GMM 时返回 `gmm_raw_sample_missing` 并提示重新采样。

### 3.3 GMM 配置

```text
backendConfigs.cielabGmm = {
  colorChannels: "ab" | "lab",
  maxSamplesPerClass: 10000
}
```

普通 UI 不直接展示 `colorChannels`，而是使用统一的“亮度参与”复选框映射：未勾选为 `ab`，勾选为 `lab`。`maxSamplesPerClass` 在 B2 固定为 10000，仅持久化和进入签名，不开放编辑。

“亮度参与”只是统一的业务交互文案，不是跨后端共用的配置字段。HSV 一维模式仍保存 `backendConfigs.hsvHistogram.brightnessEnabled`，GMM 保存 `backendConfigs.cielabGmm.colorChannels`；CIELAB `L` 与 HSV `V` 不得解释为相同数值特征。

### 3.4 GMM 模型

```text
backendModels.cielabGmm = {
  state: empty | ready | ready_with_warning | stale | invalid | failed,
  status,
  message,
  algorithmVersion,
  featureSchemaVersion,
  colorChannels,
  trainingDataHash,
  buildParamsHash,
  serializedGmmBase64,
  serializedSize,
  serializedSha256,
  classIdOrder,
  classes[],
  builtAtUtc
}
```

`building` 是 Dialog 的瞬时内存状态，不属于上述持久化枚举，也不写入 JSON。建模失败保留上一个合法产物，但模型状态变为 `failed` 或 `stale`，生产端不得使用旧产物；错误 status/message 用于 UI 定位。

## 4. UI 与交互

在 `ColorTemplateDialog` 的算法参数区域增加：

- “识别算法”下拉框：`HSV 直方图（推荐纯色/少样本）`、`CIELAB GMM（推荐复杂颜色）`。HSV 后端内部继续由现有特征类型控件选择“一维兼容”或“二维 H/S 推荐”模式。
- 统一的“亮度参与”复选框，并根据所选后端映射到各自配置。
- 模型状态标签：未建立、建模中、可用、可用但样本偏少、已失效、校验失败、建模失败。
- “建立 GMM 模型”或“重新建立”按钮。
- 类别 ROI 数、实际训练像素数、中心范围和 warning 的只读诊断摘要。

“亮度参与”的交互规则固定为：

- HSV 一维直方图：可操作，关闭表示 H/S，开启表示 H/S/V。
- HSV 二维 H/S：固定关闭并禁用，因为该模式不使用亮度。
- CIELAB GMM：可操作，关闭映射 `colorChannels=ab`，开启映射 `colorChannels=lab`。

选择 HSV 时显示现有特征类型和灵敏度控件；选择 GMM 时隐藏或禁用 HSV 专属参数，显示 GMM 模型状态和建模按钮。切换后端时复选框回显当前后端自己的值，不将 HSV 的选择复制给 GMM，也不删除另一后端模型。

建模按钮执行以下校验后调用 `buildGmmTemplateModel()`：至少两个有效类别、每类至少一个具有无损 GMM 图像的 ROI、样本 ID 唯一、位深元数据完整。建模期间禁用保存、重复建模和样本修改；完成后恢复控件并显示结果。

模板编辑器保存时允许保存 `empty/stale/failed` 状态，但给出明确确认提示。GMM 建模不得在普通“保存模板”动作中隐式触发。

## 5. stale 状态规则

以下公共事实变化同时使 HSV 和 GMM 派生模型失效：

- 新增、删除或替换样本 ROI 图。
- 修改样本所属 `classId`。
- 新增、删除类别，或改变类别与 classId 的映射。
- 修改样本 `pixelFormat`、`validBits`、`bitShift`。

标签只改显示名称但 classId 不变时，GMM 的概率分布无需重训，但持久化标签合同已经变化。B2 统一将 GMM 标记为 stale，避免序列化模型诊断与显示标签不一致；HSV 样本标签同步后继续按现有规则处理。

以下变化只使 GMM stale：

- GMM 模式下切换“亮度参与”，使 `colorChannels` 在 `ab` 与 `lab` 间变化。
- 固定采样上限或后续 GMM 建模参数变化。
- GMM 算法、特征 schema 或 HALCON 主次版本不匹配。

以下变化只使 HSV stale：

- HSV 特征类型、灵敏度、亮度或平滑合同变化。

单纯切换 `recognitionBackend` 不修改任何模型状态。stale 操作不得清空 serialized item；保留它用于诊断和导出，但运行端必须依据 state 拒绝使用。

## 6. 保存、加载与校验

保存前对 ready 模型检查必填字段、Base64、size 和 SHA-256 一致性。重新打开或导入模板时：

1. 解析 schema 并迁移旧 HSV 模板。
2. 校验 `sampleId` 唯一性和 GMM 原始图字段。
3. 对 `ready/ready_with_warning` 的 GMM 调用 `validateGmmModelArtifact()`。
4. 校验失败时将内存态改为 `invalid`，保留原始 JSON 数据和错误信息。
5. 比较当前配置/公共训练事实与已保存 hash；不一致时改为 `stale`。

加载校验不得自动重训，不得自动改用 HSV。旧模板没有 GMM 节点时初始化为 `empty`。

## 7. Adapter 行为

Adapter 解析 `recognitionBackend`：

- `hsv_histogram`：继续构造现有 `ColorRecognitionHalconConfig` 并调用 HSV `run()`。
- `cielab_gmm`：B2 固定返回 `unsupported_gmm_detection_phase_b2`，payload 写入后端、模型状态和“请等待 B3 检测接入”的诊断。
- 未知值：返回 `unsupported_recognition_backend`。

禁止 GMM 错误或未接入状态 fallback 到 HSV。

## 8. 错误合同

B2 新增稳定状态：

- `gmm_raw_sample_missing`
- `gmm_sample_id_invalid`
- `gmm_model_empty`
- `gmm_model_stale`
- `gmm_model_invalid`
- `gmm_model_build_in_progress`
- `gmm_model_save_invalid`
- `unsupported_gmm_detection_phase_b2`
- `unsupported_recognition_backend`

B1 的 HALCON runtime、图像元数据、训练和产物校验错误原样向 UI 透传，不改写成模糊的“建模失败”。

## 9. 验证

新增 B2 模型生命周期 smoke，并覆盖：

1. 新采集 8 位 ROI 保存稳定 sampleId、无损 GMM PNG、输入元数据和 SHA-256。
2. 16 位 ROI 编解码后深度和值不变，并能通过 B1 建模。
3. 两类样本从 Dialog 数据转换为 B1 输入并建立 ready/ready_with_warning 模型。
4. 工程保存、重新加载、模板导出和导入后，GMM Base64/size/hash 不变且反序列化成功。
5. 旧 HSV 模板迁移后仍能运行，GMM 状态为 empty。
6. 缺少无损 GMM 图的旧样本建模时返回 `gmm_raw_sample_missing`。
7. 修改公共样本或标签后 GMM stale；修改 HSV 参数不误伤 GMM；在 GMM 模式切换“亮度参与”只使 GMM stale。
8. 在不同后端间切换时，“亮度参与”分别回显 HSV 和 GMM 的独立值，不发生跨后端覆盖。
9. 切换后端不删除模型、不改变 ready/stale 状态。
10. GMM 模式执行测试运行返回 `unsupported_gmm_detection_phase_b2`，不调用 HSV。
11. 篡改模型 Base64/hash 后加载为 invalid，且界面不崩溃。
12. 空图、Mono、无效 ROI、缺类别和缺 HALCON runtime 返回明确错误。
13. Qt 主工程、现有 HSV smoke 和 GMM B1 smoke 均通过。

## 10. 完成标准

- 用户能在颜色模板 UI 选择 GMM、采集原始 ROI、建立模型、看到状态、保存并重新打开。
- ready 模型经过严格 hash 和 HALCON 反序列化校验；参数或样本变化后状态准确变为 stale。
- HSV 与 GMM 配置和模型互不覆盖，旧 HSV 模板保持可运行。
- GMM 测试运行明确报告 B2 尚未接检测，不发生静默降级。
- 不修改颜色比较和注册分类功能，不引入新的核心算法依赖。
- 主工程和相关 smoke 在影子目录构建通过。
