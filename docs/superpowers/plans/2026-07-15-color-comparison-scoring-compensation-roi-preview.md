# 颜色比较评分、亮度补偿与 ROI 缩略图更改计划

## 1. 目标

本次修改解决以下三个相互关联的问题：

1. 低饱和区域与较高饱和区域比较时，最终得分长期固定在 `40.0`，无法反映样本之间的连续差异。
2. 亮度补偿比例或截断比例超出安全范围时，整个测量直接返回 `invalid_illumination`，导致本来仍可计算的 H/S 特征和检测直方图不可用。
3. 模板 ROI 缩略图把橙色 ROI 边框和遮罩绘制到图像副本后再裁剪，视觉上容易让用户误以为标注像素参与了特征提取。

修改后必须保持 HALCON 为颜色特征提取核心，不能改变原始 ROI 坐标语义，也不能影响其他视觉工具。

## 2. 当前问题与证据

### 2.1 固定 40 分的来源

当前 `ColorComparisonHalconRunner.cpp` 在任一侧平均饱和度不高于 `0.10` 时，使用以下封顶逻辑：

```text
mismatchProgress = clamp((maximumSaturation - 0.10) / 0.10, 0, 1)
score = min(score, 100 - 60 * mismatchProgress)
```

当另一侧平均饱和度达到 `0.20` 后，封顶值恒为 `40`。本次现场数据为：

- 原始 HS 联合直方图交集：`0.021857`（界面显示 `2.2%`）。
- 平滑后 HS 分数：`89.1593`。
- 模板平均饱和度：`0.216956`。
- 检测平均饱和度：`0.099290`。
- 最终被硬封顶为：`40.0`。

因此 `40.0` 不是直方图交集本身，而是离散封顶策略形成的平台值。

### 2.2 亮度补偿失败策略

当前补偿公式 `scale = templateMean / detectMean` 本身正确，且 `[0.75, 1.333333]` 的安全范围用于避免过度缩放。本次现场值为：

- 模板 V 均值：`27.4951`。
- 检测 V 均值：`56.2385`。
- scale：`0.4889`，低于最小安全值 `0.75`。

问题不在公式，而在于 scale 或 clipped ratio 超限后立即中止整个检测，H/S 特征与诊断图也随之丢失。对暗色、反光材料，平均 V 对高光位置变化较敏感，这种失败策略过于激进。

### 2.3 ROI 缩略图标注层

`ColorComparisonDialog::templateRoiImage()` 当前流程是：

1. 将原始参考图转换为 `QImage::Format_ARGB32`。
2. 使用 `QPainter` 在图像副本上绘制橙色 ROI 边框和红色 Mask。
3. 从已经标注的图像中裁剪 ROI。
4. 将裁剪结果缩放后显示在模板缩略图中。

因此缩略图会包含 ROI 边框的内侧像素。当前算法提取并未使用该 `QImage`：Runner 接收原始 `cv::Mat`，通过 HALCON Region/Mask 提取特征，所以现阶段橙色框不会进入模型直方图。但显示路径和算法路径外观不一致，存在误判和未来误用风险。

## 3. 设计决策

### 3.1 用连续乘法惩罚替换固定分数封顶

保留“低饱和灰色与较高饱和颜色不应高分通过”的业务约束，但不再使用 `score = min(score, 固定上限)`。

建议计算：

```text
satMin  = min(templateMeanSaturation, detectMeanSaturation)
satMax  = max(templateMeanSaturation, detectMeanSaturation)

if satMin <= 0.10:
    mismatchProgress = smoothstep(0.10, 0.20, satMax)
else:
    mismatchProgress = 0

saturationFactor = 1.0 - 0.60 * mismatchProgress
finalScore = baseScore * saturationFactor
```

其中 `baseScore` 是平滑 HS 分数、亮度/灰度规则处理后的连续分数。`smoothstep` 使用 `t*t*(3-2*t)`，避免阈值附近跳变。即使 `saturationFactor` 达到 `0.4`，不同的 `baseScore` 仍会得到不同结果，不再全部停在 40 分。

结果 payload 新增：

- `baseScoreBeforeSaturationPenalty`
- `saturationMismatchProgress`
- `saturationFactor`
- `finalScore`

保留现有字段，避免破坏旧 UI 和日志解析。

### 3.2 亮度补偿采用“请求、应用、回退”三态

亮度补偿开关打开时：

1. 模板或检测均值非有限、过暗或过曝（不在 `[8,247]`）时，保持 `invalid_illumination`，因为颜色特征自身已经不可靠。
2. scale 超出 `[0.75,1.333333]` 时，不中止检测；跳过补偿，使用原始 R/G/B 转换得到的 H/S/V 继续测量。
3. 预计截断比例超过 `0.02` 时，同样跳过补偿并继续原始特征测量。
4. 回退时在 warnings 中增加 `brightness_compensation_skipped`，并保留具体原因。

亮度 diagnostics 调整为：

- `requested`：用户是否开启。
- `applied`：本次是否实际应用。
- `fallback`：是否回退到原图。
- `fallbackReason`：`scale_out_of_range`、`clip_ratio_exceeded` 等稳定枚举。
- `templateMean`、`detectMeanBefore`、`detectMeanAfter`、`scale`、`clippedRatio`。

UI 状态栏应显示“检测有效；亮度补偿已跳过”，不能显示成测量失败。

### 3.3 明确亮度对最终评分的影响

当前实现中的 `brightnessFactor`、`grayScore` 和 `grayWeight` 会影响最终分数，与现有文档“V 通道差异不直接加入最终颜色得分”的表述不一致。

本次计划以现有实现为准：亮度不进入 HS 直方图交集，但会作为灰度目标判别和亮度惩罚参与最终得分。同步修改颜色比较设计与实现文档，避免把“未进入 HS 交集”误写成“未影响最终得分”。

### 3.4 原始 ROI 裁剪与标注显示分离

将现有 `templateRoiImage()` 拆分为职责清晰的路径：

- `templateRawRoiImage()`：只从原始参考图按归一化 ROI 裁剪，不绘制任何边框、文字或 Mask。
- 缩略图默认显示 `templateRawRoiImage()`，确保用户看到的就是参与特征提取的原始像素范围。
- ROI 身份使用缩略图 QLabel 的外部 QSS 边框或旁边的“模板 ROI”图例表达，不修改缩略图像素。
- Mask 不直接填充到原图缩略图；如需要表达，应在独立 overlay widget 上显示半透明排除区域，并明确标注“显示层，不参与像素”。

算法路径继续只接收原始 `cv::Mat`，所有 ROI/Mask 通过 HALCON Region 运算。禁止把 QWidget grab、scene render 或带 overlay 的 QImage 传入模型构建与检测。

## 4. 实施任务

### Task 1：建立评分与补偿回归基线

修改/新增：

- `smoke/color_comparison_smoke.cpp`
- 必要时新增纯评分辅助函数测试入口，避免每个边界测试都依赖相机或 GUI。

测试用例：

1. 记录当前现场数据能够复现旧版 40 分平台。
2. base score 不同但饱和度组合相同时，最终分数必须保持差异。
3. 饱和度在 `0.10`、`0.20` 附近变化时分数连续、单调，无突跳。
4. 相同灰色、灰色对有色、高饱和同色、高饱和异色分别覆盖。
5. 记录 raw intersection、smoothed HS、亮度项、饱和度项和 final score。

### Task 2：替换饱和度固定封顶

修改：

- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
- 如需复用，调整对应 header 中的内部诊断结构。

要求：

1. 使用连续乘法因子，不再产生固定 40 分平台。
2. 不改变 HALCON HS 直方图提取和平滑交集实现。
3. payload 输出完整评分分解字段。
4. 阈值判断只使用最终分数。

### Task 3：实现亮度补偿安全回退

修改：

- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
- `src/tooladapters/ColorComparisonAdapter.cpp`（仅在新增字段解析或状态映射需要时）
- `smoke/color_comparison_feature_diagnostics_smoke.cpp`

要求：

1. scale/clipping 超限时返回有效测量和 warning，不返回 `invalid_illumination`。
2. 原始均值不可用、过暗、过曝仍返回明确失败。
3. 未开启、成功应用、scale 回退、clip 回退四条路径均有 diagnostics。
4. 回退路径仍输出检测 H/S 和 V 直方图，供 UI 对比。

### Task 4：修正 ROI 缩略图显示路径

修改：

- `src/ColorComparisonDialog.cpp`
- `src/ColorComparisonDialog.h`
- `styles/app.qss`
- `smoke/color_comparison_dialog_integration_smoke.cpp`

要求：

1. 原始裁剪发生在任何绘制之前。
2. 缩略图内容中不得出现橙色 ROI 框、红色 Mask 填充或结果文字。
3. ROI 提示通过控件外框和图例显示。
4. 自定义矩形、同步矩形、同步圆形三种模板区域均按原图坐标正确裁剪。
5. 缩放仅影响显示尺寸，不改变裁剪像素和归一化 ROI。

### Task 5：完善得分解释 UI

修改：

- `src/ColorComparisonFeatureView.cpp`
- `src/ColorComparisonFeatureView.h`
- `src/ColorComparisonDialog.cpp`
- `smoke/color_comparison_feature_view_smoke.cpp`

默认紧凑显示：

- `HS 原始重合`
- `平滑 HS 分数`
- `亮度系数/补偿状态`
- `饱和度系数`
- `最终得分`
- `判定阈值`

无 diagnostics 或旧方案缺字段时显示 `--`，不得用 0 冒充真实测量。

### Task 6：文档与完整验证

更新：

- `docs/FID/ColorComparison/颜色比较检测特征可视化设计.md`
- `docs/FID/ColorComparison/color_comparison_function_implementation.md`
- 必要时更新颜色比较 V2 设计说明，不修改颜色识别文档中的独立算法语义。

验证：

1. 构建并运行 color comparison runner、diagnostics、feature view、dialog integration smoke。
2. 执行主工程 qmake 和 make。
3. 在有效 HALCON license 环境下执行实际模板建模和检测。
4. 人工覆盖黑色反光线材、彩色线材、银色背景及 ROI 部分包含背景的场景。
5. 检查模板缩略图与原始 ROI 像素一致，检测直方图不受任何 UI overlay 影响。

## 5. 验收标准

- 不再出现不同样本长期固定 `40.0` 的平台现象。
- UI 能解释 raw overlap、平滑分数、亮度处理、饱和度惩罚和最终分数之间的关系。
- scale 或 clipping 超限时仍能获得有效 H/S 检测特征，并明确提示补偿已跳过。
- 真正过暗、过曝或无有效像素仍必须明确 NG，不能静默通过。
- 模板 ROI 缩略图不包含橙色框、Mask 填充或文字等显示层像素。
- 自动测试证明 Runner 的输入始终来自原始帧，UI overlay 不参与模板或检测特征。
- ROI 缩放、平移和绘制后，裁剪区域与 HALCON Region 使用相同归一化坐标。
- 其他工具因 `FrameViewHelper` 默认导航关闭及独立评分链路而不受影响。

## 6. 风险与兼容性

- 评分公式变化会改变旧方案的分数分布，不能直接保证旧阈值仍最优。验证阶段应保存旧分数与新分数对照，并给出阈值迁移建议。
- 亮度补偿回退会把过去的 `invalid_illumination` 变为有效测量；上层若依赖该错误码，需要同步检查。
- 暗色目标的 H/S 稳定性受噪声、白平衡和高光影响，不能仅通过放宽安全范围解决。
- 缩略图去掉内部橙色框后，需要保留清晰的外部图例，避免用户无法判断当前模板区域状态。

## 7. 建议实施顺序

按 `Task 1 → Task 2 → Task 3 → Task 4 → Task 5 → Task 6` 实施。先锁定评分和补偿行为，再调整显示，最后进行真实图像阈值回归，避免 UI 改动掩盖算法变化。
