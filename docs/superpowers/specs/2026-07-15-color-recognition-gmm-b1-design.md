# 颜色识别 CIELAB GMM 阶段 B1 设计

日期：2026-07-15
状态：待实施
上位设计：`docs/FID/ColorRecognition/颜色识别算法V2整理设计.md`

## 1. 目标与边界

本阶段建立颜色识别的 HALCON CIELAB GMM 核心建模闭环，为后续像素分类和双后端派发提供可验证模型产物。

本阶段包括：

- 8/16 位彩色样本的 HALCON 规范化和 CIELAB 转换。
- 按类别、ROI 和固定规则进行确定性等量像素采样。
- HALCON GMM 创建、追加样本、训练、内存序列化和严格产物校验。
- 强类型建模输入、输出、模型状态、签名和错误合同。
- Runner 级建模 smoke。

本阶段不包括：

- `recognitionBackend` 生产派发。
- `classify_image_class_gmm` 检测链和面积诊断。
- Adapter、Dialog、异步建模 UI 和方案 JSON 迁移。
- HSV 行为变更、颜色比较改造或颜色公共底座抽取。

## 2. 组件边界

采用独立 GMM 内部后端：

```text
ColorRecognitionHalconRunner
  └─ buildGmmTemplateModel(samples, config)
       └─ ColorRecognitionGmmHalconBackend
            ├─ Gmm runtime symbol profile
            ├─ HALCON RGB real [0,1] → CIELAB
            ├─ deterministic balanced sampling
            ├─ create/add/train class_gmm
            └─ serialize/validate artifact
```

新增 `ColorRecognitionGmmHalconBackend.{h,cpp}`，放在 `src/algorithms/recognition/`。它只服务颜色识别，不被颜色比较依赖。`ColorRecognitionHalconRunner` 暴露稳定建模入口并负责公共结果类型，现有 HSV 提取和运行链保持原样。

HALCON runtime 按 `Gmm` profile 延迟加载。缺少 GMM 符号只影响 GMM 建模，不得使 HsvHistogram profile 获取失败。函数指针可跨线程共享，单次训练创建的 GMM、serialized item 和临时 HALCON object 只在调用线程内使用，并由 RAII 在所有路径清理。

## 3. 公开数据合同

### 3.1 建模样本

`ColorRecognitionGmmBuildSample` 至少包含：

- `sampleId`：非空稳定 ID，用于确定性排序。
- `classId`：稳定业务类别 ID，不要求连续。
- `label`：同一 classId 必须唯一对应一个非空名称。
- `image`：8/16 位三或四通道 `cv::Mat`，仅作为输入容器。
- `roiNormalized`：样本有效区域。
- `pixelFormat`：RGB/BGR/RGBA/BGRA 与位深信息。
- `validBits`、`bitShift`：16 位容器必须显式提供。
- `imageSha256`：对 rows、cols、type 和逐行有效像素字节计算的校验值；不包含 `cv::Mat` stride padding。

### 3.2 建模配置

`ColorRecognitionGmmBuildConfig` 包含：

- HALCON so 路径及候选诊断。
- 声明的稳定类别列表 `labels`；至少两类，每个 classId 唯一且名称非空。
- `colorChannels = ab | lab`，默认 `ab`。
- 固定 `maxSamplesPerClass = 10000`。
- 固定训练合同：full covariance、none preprocessing、seed 42、MaxIter 100、Threshold 1e-4、uniform priors、Regularize 1e-4。
- 固定 `samplingAlgorithmVersion`。

这些训练参数首版不开放普通 UI。检测期 rejection 阈值不进入本建模配置和模型 hash。

### 3.3 建模结果

`ColorRecognitionGmmBuildResult` 包含：

- `success`、`status`、`message`、`elapsedMs`。
- `state = Ready | ReadyWithWarning`。
- `serializedGmmBase64`、`serializedSize`、`serializedSha256`。
- `buildParamsHash`、`trainingDataHash`。
- `classIdOrder`、每类标签、ROI 数、可用像素数、实际训练像素数和中心范围。
- `algorithmVersion = halcon_cielab_gmm_classification_v1`。
- `featureSchemaVersion = cielab_gmm_pixel_classification_v1`。
- `colorSpace = cielab`、实际 channels、HALCON 版本和采样算法版本。
- 供 smoke 使用的严格诊断 payload。

序列化字节解码后上限为 8 MiB。Base64、size 和 SHA-256 任一不一致均视为无效产物。

## 4. HALCON 数据流

```text
多类别原始 ROI 样本
  → 校验 sampleId/classId/label/hash/ROI/位深元数据
  → 按 classId 升序生成 classIdOrder
  → 按 sampleId 排序每类 ROI
  → gen_image_interleaved
  → 16 位 bit_rshift + min_max_gray 合法性检查
  → convert_image_type('real') + scale_image 到 [0,1]
  → trans_from_rgb(..., 'cielab')
  → compose2(A,B) 或 compose3(L,A,B)
  → get_region_points 获得稳定行扫描点
  → 等间索引生成无重复确定性采样 region
  → create_class_gmm
  → add_samples_image_class_gmm（每 ROI 一次）
  → train_class_gmm
  → serialize_class_gmm
  → get_serialized_item_ptr 并立即复制到 QByteArray
  → size/SHA-256/Base64 校验
  → clear_serialized_item + clear_class_gmm
```

颜色转换、位深映射、区域构造和 GMM 训练均使用 HALCON。OpenCV 不参与颜色转换、抽样算法、分类或统计。

## 5. 采样与中心策略

- 每类至少 1 个有效 ROI，累计可用像素不少于 256。
- 每类训练像素数取 `min(所有类别可用像素数, 10000)`，保证类间等量。
- 类内尽量均分到各 ROI；余数按 sampleId 顺序分配。小 ROI 不足时，配额继续分给本类其他 ROI。
- 单 ROI 使用 HALCON `get_region_points` 的稳定行扫描点序列，按等间索引选择无重复点，再通过 `gen_region_points` 构造 region。
- 采样结果不得依赖 UI 列表顺序。

每类中心范围按独立 ROI 数决定：

| ROI 数 | 中心范围 | 模型状态 |
|---:|---:|---|
| 1–2 | `[1,1]` | ReadyWithWarning |
| 3–4 | `[1,1]` | Ready |
| 5–9 | `[1,2]` | Ready |
| ≥10 | `[1,3]` | Ready |

`NumCenters` 按 classIdOrder 展开为长度 `2 * NumClasses` 的 tuple，并使用 `T_create_class_gmm`。训练使用 `T_train_class_gmm`。

## 6. 签名与状态

`trainingDataHash` 覆盖排序后的 sampleId、classId、label、imageSha256、ROI、pixelFormat、validBits 和 bitShift。

`buildParamsHash` 覆盖：

- model/algorithm/feature schema 版本。
- CIELAB channels 和维数。
- classIdOrder 与每类中心范围。
- covariance、preprocessing、seed、迭代、阈值、先验和正则化参数。
- samplingAlgorithmVersion、每类实际样本数和 10000 上限。
- HALCON major/minor 版本。
- trainingDataHash。

每类只有 1–2 个 ROI 时，训练成功返回 `ReadyWithWarning`；所有类至少 3 个 ROI 时返回 `Ready`。本阶段不持久化 Stale/Invalid，它们由后续模型层在参数或产物校验失败时设置，但 Runner 必须返回足够诊断支持该状态转换。

## 7. 错误合同

失败不返回半成品模型，主要状态包括：

- `gmm_samples_missing`
- `gmm_class_sample_missing`
- `gmm_sample_image_invalid`
- `gmm_sample_hash_mismatch`
- `unsupported_mono_for_color_recognition`
- `unsupported_image_type`
- `missing_pixel_format_metadata`
- `invalid_pixel_format_metadata`
- `pixel_value_out_of_range`
- `invalid_roi`
- `gmm_insufficient_pixels`
- `halcon_runtime_not_found`
- `halcon_symbol_profile_missing`
- `halcon_license_error`
- `gmm_training_failed`
- `model_artifact_invalid`
- `model_artifact_too_large`

HALCON 异常必须转换为稳定 status，并在 message/payload 保留可定位的算子阶段和 HALCON 错误码。失败路径必须清理 GMM handle、serialized item 和临时 iconic object。

## 8. 验证

新增独立 GMM 建模 smoke，至少验证：

1. 红/绿两类纯色样本成功训练并得到非空合法产物。
2. `ab` 与 `lab` 的维数和 buildParamsHash 不同。
3. 1/3/5/10 个 ROI 对应预期 warning/ready 和中心范围。
4. 类间训练像素数相等，重复建模的采样诊断和 hash 一致。
5. 8 位与正确编码 16 位样本进入一致规范化尺度。
6. Mono、缺元数据、非法有效位和越界像素返回明确错误。
7. 序列化 → Base64 → SHA-256 → 反序列化往返成功。
8. 篡改 Base64、size 或 hash 时产物校验失败。
9. GMM profile 缺符号时返回明确错误，现有 HSV smoke 仍通过。
10. 正常、HALCON 异常和校验失败路径无 handle 泄漏或崩溃。

验证命令使用影子构建目录，并显式确认 HALCON 20.11 runtime 与 license。阶段 B1 完成时同时运行现有颜色识别 HSV smoke，证明未改变阶段 A 行为。

## 9. 完成标准

- GMM 建模只依赖 HALCON 核心算子，且 HALCON 20.11 所需符号已按 Gmm profile 加载。
- 同一有效输入可重复得到相同 classIdOrder、采样诊断和严格 hash。
- 产物可在内存中完成序列化校验和反序列化，不写临时模型文件。
- 所有失败均返回稳定错误，不崩溃、不产生可被误用的半成品。
- 现有 HSV 识别链与颜色比较代码无行为变化。
- Qt 主工程和新旧 smoke 构建、运行通过。
