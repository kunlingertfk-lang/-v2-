# 颜色识别双后端切换与训练上下文恢复设计

日期：2026-07-16
状态：设计已确认，待实施

## 1. 问题与现场证据

颜色识别模板已经采用公共样本、HSV 与 CIELAB GMM 双后端结构，但当前编辑和运行闭环存在以下缺口：

- 新建模板添加首个样本后，GMM 建模按钮可能仍保持禁用。
- 编辑模板时切换 HSV/GMM 只更新活动后端和控件，没有校验目标后端是否可用。
- 切回 HSV 后不会自动提取特征，也没有 HSV `ready/stale/empty` 状态和完成反馈。
- 模板保存前只检查 GMM 状态，HSV stale 时仍可无提示保存。
- 软件重启后能够回显标签和 ROI 缩略图，但没有恢复当前样本、训练 Mat、输入元数据和活动 ROI 上下文，后续训练可能误报没有 ROI。
- 参考图经过格式规范化或从 PNG 重新加载后，实际 Mat 与保存的原始输入元数据可能不一致，导致 `pixelFormat does not match input depth/channel count`。

当前实际方案数据进一步证明这是混合状态问题：模板共有 9 个公共样本，均保存了 GMM 无损 ROI，其中 6 个 HSV `featureSignature` 为空，只有 3 个具有严格 HSV 签名；活动后端已切换为 HSV，HSV 模型状态为 stale。参考图重载后实际为三通道 BGR8，但方案仍可能声明 RGBX8。

## 2. 目标与边界

本次修复建立以下完整闭环：

```text
保存公共无损 ROI
  → 重启后恢复训练上下文
  → HSV/GMM 独立校验和显示状态
  → 用户显式重建目标后端
  → 保存前校验活动后端
  → 生产检测严格选择活动后端
```

本次采用“公共无损 ROI + 独立派生模型”方案：

- 标签、classId、无损 ROI 图、位深和像素格式是公共训练事实。
- HSV 保存 feature、严格 featureSignature 和独立状态。
- GMM 保存 serialized model、训练签名和独立状态。
- 单纯切换活动后端不训练、不清除模型，也不使另一后端 stale。
- 不拆分两套样本集合，不复制 ROI，不修改颜色比较和其他视觉工具。
- OpenCV 继续只承担 Mat 容器、裁剪和无损 PNG 编解码；HSV、CIELAB 转换、特征提取和 GMM 核心全部由 HALCON 完成。

## 3. 数据模型与状态

公共样本继续保存：

```text
sampleId
classId / label
roiNormalized
roiImagePngBase64
gmmRoiImagePngBase64
gmmImageSha256
gmmImageWidth / gmmImageHeight
pixelFormat / validBits / bitShift
```

HSV 派生数据继续保存于每个样本：

```text
feature
featureSignature
```

模板的 HSV 状态统一解释为：

- `empty`：没有公共样本。
- `ready`：全部样本都有特征，且严格签名与当前 HSV 参数及输入合同一致。
- `stale`：任一样本特征缺失、签名缺失或签名不匹配。
- `invalid`：保存的公共 ROI 或元数据损坏，无法重建。

GMM 继续沿用：

```text
empty | ready | ready_with_warning | stale | invalid | failed
```

公共样本新增、删除、替换，或 classId、位深元数据变化时，两套派生模型均 stale。HSV 参数变化只使 HSV stale；GMM a/b 与 L/a/b 或建模参数变化只使 GMM stale。

## 4. 重新打开后的训练上下文恢复

`ColorTemplateDialog::setTemplateData()`加载模板后执行：

1. 恢复标签和公共样本。
2. 校验每个无损 ROI 的 Base64、PNG 解码、宽高、hash 和像素元数据。
3. 自动选择第一个有效标签和该标签下第一个有效样本。
4. 从选中样本的 `gmmRoiImagePngBase64`解码实际训练 Mat。
5. 使用样本保存的 `pixelFormat/validBits/bitShift`恢复输入元数据。
6. 因保存图已经是裁剪后的 ROI，恢复后的活动训练 ROI 固定为 `[0,0,1,1]`。
7. 右侧预览显示该保存样本，状态栏明确显示当前类别和 sampleId。
8. 分别计算 HSV 和 GMM 状态，并刷新对应操作按钮。

如果没有样本，显示“尚未采样”；有样本但未选中时显示“请选择 ROI 样本”；数据缺失或损坏时显示具体错误。不得把这些情况统一显示成“没有 ROI”。

用户点击其他样本缩略图时，同样恢复该样本的 Mat、元数据和全图训练 ROI。切换样本只改变当前编辑上下文，不修改已保存训练事实。

## 5. HSV 显式重建流程

HSV 模式新增独立状态标签和“重新提取 HSV 特征”按钮。后端切换到 HSV 时只校验，不自动运算：

- 全部样本严格匹配：显示“HSV 特征已就绪”，按钮可显示“重新提取”。
- 任一样本缺特征或签名不匹配：显示“HSV 特征已失效”，启用重建按钮。
- 没有公共样本：显示“HSV 尚未采样”，禁用按钮。
- 公共 ROI 损坏或缺失：显示“HSV 无法重建”，提示需要重新采样。

用户点击重建后：

1. 锁定保存、算法切换、样本增删和重复重建操作。
2. 逐个解码保存的无损 ROI，并校验尺寸、hash 和元数据。
3. 对每个裁剪 ROI 使用整图 `[0,0,1,1]`调用现有 HALCON HSV 特征提取。
4. 所有样本成功后一次性提交新的 feature、featureSignature、algorithmVersion 和 featureSchemaVersion。
5. 将 HSV 状态设为 ready，显示按钮附近成功状态，并弹出完成提示。
6. 任一样本失败时整体不提交，保留旧特征；状态保持 stale/invalid，并显示 sampleId、类别和失败原因。

该事务式更新避免一半样本使用新签名、一半样本使用旧签名。

## 6. GMM 建模按钮与后端切换

样本增删、标签变化和模型状态变化后必须始终刷新 GMM UI。`markGmmModelStale()`在模型为 empty 时可以不改变状态，但不能跳过 `updateGmmModelUi()`。

切换到 GMM 时显示现有状态：

- `ready/ready_with_warning`：允许直接检测和重新建立。
- `stale/empty/failed`：启用建立或重新建立按钮。
- `invalid`：显示校验失败，只有公共 ROI 仍完整时才允许重新建立。

切换 HSV/GMM 时分别恢复两套独立参数。单纯切换不改变任何派生模型状态。

## 7. 参考图运行时元数据

`ReferenceFrameSnapshot.metadata`必须描述快照中实际保存的 Mat，而不是转换前的采集图：

```text
输入相机图/文件
  → ReferenceImageProvider 规范化 Mat
  → 根据规范化后的 Mat 重新生成运行时 FrameInputMetadata
  → snapshot.frame 与 snapshot.metadata 成对保存
```

当 `SchemeStore`使用 `imread(..., IMREAD_COLOR)`重新加载 `reference.png`时，运行时元数据必须是实际的 BGR8、8 valid bits、0 bit shift。原始相机元数据如需保留，只能作为来源/追溯字段，不能继续充当运行输入合同。

HSV 和 GMM 后端继续严格验证 Mat 深度、通道数与 pixelFormat，不在后端静默改写或猜测，以便及时发现 RGB/BGR 语义错误。

## 8. 保存与生产运行规则

模板保存前校验当前活动后端：

- HSV 非 ready：提示“HSV 特征尚未就绪，保存后不能用于检测”，默认选择取消；用户明确确认后允许保存 stale 模板。
- GMM 非 ready/ready_with_warning：沿用现有确认行为。
- 保存 stale 模板后，生产检测必须返回稳定错误，不得回退另一后端。

Adapter 继续只依据保存的 `recognitionBackend`路由。HSV runner 必须保留逐样本严格签名比较；GMM runner 必须保留模型版本、训练签名和输入格式校验。

## 9. 错误与诊断合同

增加或规范以下状态：

- `hsv_feature_empty`
- `hsv_feature_stale`
- `hsv_rebuild_sample_failed`
- `sample_roi_not_selected`
- `sample_roi_data_missing`
- `sample_roi_data_invalid`
- `input_metadata_mismatch`

相关错误 payload 至少包含适用字段：

```text
sampleId
classId
declaredPixelFormat
actualMatDepth
actualChannelCount
expectedFeatureSignature
sampleFeatureSignature
```

UI 必须显示可执行建议，例如“选择样本”“重新提取 HSV 特征”“重新建立 GMM”“重新采样”，不能只显示 `error` 或“没有 ROI”。

## 10. 兼容策略

- 旧 HSV 样本只有 feature、缺少严格签名时标记 stale，禁止仅按长度放行。
- 旧样本缺少无损 ROI 时不能批量重建 HSV 或 GMM，提示重新采样。
- 已有合法 GMM serialized model 在 HSV 重建期间保持不变。
- 当前实际模板中 6 个缺 HSV 签名的样本，可直接使用已保存的 GMM 无损 ROI 批量重建，无需重新框选。
- 不在加载、后端切换或保存动作中隐式训练。

## 11. 验证

新增或扩展颜色识别生命周期 smoke，覆盖：

1. 新建模板添加首个样本后，GMM 建模按钮立即可用。
2. 保存并重新加载后自动选择首个有效标签和样本，恢复 Mat、元数据和全图训练 ROI。
3. 重启后使用已保存公共 ROI 成功重新建立 GMM。
4. 混合样本中存在空 HSV 签名时状态为 stale，切换 HSV 后显示明确提示。
5. 批量重建全部成功时一次性生成一致 HSV 签名，显示成功状态和完成弹窗。
6. 任一样本 ROI 缺失、损坏或元数据无效时，HSV 重建不部分提交。
7. HSV/GMM 往返切换不覆盖彼此参数和模型状态。
8. HSV stale 和 GMM stale 保存前分别得到确认提示。
9. 参考图由 RGBX/BGRA/16 位来源规范化或从 PNG 重载后，snapshot metadata 与实际 Mat 一致。
10. 输入格式仍不一致时，错误包含声明格式、实际深度和通道数。
11. 当前 9 样本方案能够从保存的无损 ROI 重建 HSV，不要求重新框选。
12. 运行 HSV Phase A、GMM B1/B2/B3、相关 UI lifecycle smoke，以及主工程 qmake 和 make。

## 12. 非目标

- 不改变 HSV 或 GMM 的数学算法和 HALCON 算子链。
- 不自动调节最低分数、拒识阈值、类别置信度或分类覆盖率。
- 不抽取颜色识别与颜色比较公共底座。
- 不实现位置修正。
- 不修改无关工具、公共 UI 布局或方案格式。
