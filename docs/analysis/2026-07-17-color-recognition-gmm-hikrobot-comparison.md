# 颜色识别 GMM 与海康产品语义对照分析

## 1. 分析边界

本报告对照以下事实源：

- `docs/智能相机客户端_用户手册_V3.2.2_20260127.pdf` 第 274–278 个 PDF 页面（手册印刷页 265–269）。
- 海康机器人在线帮助链接。该站点当前返回 403 安全策略拦截，因此不能把网页中无法读取的内容作为证据。
- 本机 HALCON 20.11.1.1 operator reference：`create_class_gmm`、`add_samples_image_class_gmm`、`train_class_gmm`、`evaluate_class_gmm`、`classify_image_class_gmm`。
- 当前 `ColorRecognitionGmmHalconBackend`、Dialog、Adapter 和 smoke 实现。

海康公开手册只说明产品行为，不公开内部颜色空间、分类器、采样和得分公式。因此可以对齐产品语义，但不能断言海康使用 GMM、KNN、直方图交集或某个具体模型。

## 2. 海康公开产品合同

手册能够确认：

- 颜色识别是基于颜色模板的多类别整体分类，不是单纯颜色面积测量。
- 可导入 1–10 张训练图，在图像上绘制多个标签 ROI。
- 提供高/中/低灵敏度、直方图/色谱图特征、亮度参与开关。
- 光照变化场景可关闭亮度。
- 判定支持最低分数和类别判断。
- 输出颜色名称、最佳分数和相似度，且相似度越高表示与模板颜色越接近。

合理推断是：产品内部会聚合多个标签 ROI，并通过灵敏度和亮度开关控制颜色特征的容差。但手册不足以证明它采用像素 GMM，也不足以证明“最佳分数”和“相似度”的内部公式。

## 3. 当前 GMM 实际链路

```text
8/16 位 RGB/BGR ROI
  → HALCON 位深校验、右移和 real [0,1] 规范化
  → trans_from_rgb(..., 'cielab')
  → a/b 或 L/a/b 特征图
  → 类别间取最小可用像素数，限制到 maxSamplesPerClass
  → 类内向各 ROI 均衡分配配额
  → ROI 内二维分层选点
  → add_samples_image_class_gmm
  → create/train_class_gmm（full covariance、uniform priors）
  → serialize_class_gmm

检测 ROI
  → 同一 CIELAB 特征链
  → classify_image_class_gmm(RejectionThreshold)
  → 统计各类别接受像素面积
  → 预测面积最大类别
  → score / categoryConfidence / classifiedCoverage 联合判定
```

合理部分：

- 8/16 位输入先规范化，再进入 CIELAB，避免位深尺度漂移。
- `a/b` 可弱化亮度变化，`L/a/b` 可用于必须区分深浅的场景。
- 类别间训练像素等量、先均衡 ROI，再做空间分布，能抑制大 ROI 和样本数量对训练的直接支配。
- 使用 HALCON `class_gmm`、`add_samples_image_class_gmm`、`train_class_gmm` 和 `classify_image_class_gmm`，模型核心方向正确。
- 模型序列化、hash、schema、stale 和错误状态完整。
- `score`、已分类像素纯度和有效覆盖率分开输出，比单一最高类别更容易定位拒识和混色问题。

## 4. 关键问题

### P0：拒识阈值语义和默认值错误

HALCON `classify_image_class_gmm` 的 `RejectionThreshold` 是对 `KSigmaProb` 的阈值。低于该值的像素不分配给任何类别。HALCON 20.11 `evaluate_class_gmm` 文档明确说明，典型需要拒绝的值约低于 `0.0001`。

当前实现存在：

- struct、Adapter、模板加载和 UI 默认值均为 `0.5`。
- UI 只有 2 位小数、步长 `0.05`，无法输入 `0.0001`。
- “拒识阈值”文案容易被理解为普通分类置信度。

后果是大量本来属于已知类的像素可能被拒绝，造成 `classifiedCoverage` 过低、最佳类别面积降低、检测频繁 NG。该参数不会改变 GMM 模型，但会直接改变每帧分类区域。

建议第一批代码修改：

- 新建模板默认改为 `0.0001`。
- spinbox 改为至少 6 位小数，步长 `0.0001`。
- 文案改成“像素拒识阈值（K-sigma）”，tooltip 明确“越高越严格”。
- 已保存模板保持原值，不能把用户选择的 `0.5` 静默改写；对 `>=0.01` 的值显示“可能过度拒识”的提示。
- smoke 增加 `0`、`0.0001`、较高阈值三档覆盖率对照。

### P0：二维采样的核心选点在 C++ 中实现

当前 `sampledRegion()` 使用 HALCON `get_region_points` 取出像素，但网格划分、中心距离和补点都在 C++ 中完成，再用 `gen_region_points` 返回 HALCON 区域。算法本身是确定且合理的，但属于核心视觉采样逻辑，不符合项目“核心算法必须使用 HALCON 算子”的红线。

建议将采样替换为 HALCON 原生区域链，例如基于 `gen_grid_region(...,'points')`、`move_region`、`intersection`、`area_center` 等形成确定性网格区域；若要求每 ROI 严格等量，需要在设计中明确允许 C++ 只计算步长/配额，像素选择仍由 HALCON 完成。替换后升级 `samplingAlgorithmVersion`，现有 GMM 模型必须 stale 并重建。

### P1：中心数与 ROI 数绑定，不等于颜色分布复杂度

当前最大中心数规则是：ROI 少于 5 个取 1，5–9 个取 2，至少 10 个取 3。ROI 数只是独立观测数量，不直接代表类别在 a/b 或 L/a/b 空间中有几个模式：

- 一个包含阴影和高光的大 ROI 可能需要多个中心。
- 十个稳定纯色 ROI 可能只需要一个中心。

HALCON 已支持 `[min,max]` 并通过 MML 选择中心数。建议将候选上限与有效训练像素、特征维度和验证表现关联，而不是仅与 ROI 数关联。首轮可保持较小上限，通过独立验证集比较 `[1,1]`、`[1,2]`、`[1,3]`，不要直接开放普通 UI。

### P1：训练返回值未用于质量判定

`train_class_gmm` 返回实际 `Centers` 和每类 `Iter`。当前代码创建并接收这两个 tuple，但没有保存、输出或检查。

HALCON 文档说明：当某类 `Iter == MaxIter` 时可能是提前终止，中心数和误差不一定最优；不同随机种子也可能进入局部极小值。因此应：

- 将实际中心数、迭代次数写入建模诊断和模型合同。
- 任一类达到 `MaxIter` 时至少进入 `ReadyWithWarning`，不能无提示标成 Ready。
- 用固定的独立 ROI 验证集或 leave-one-ROI-out 评估，而不是只验证模型能反序列化。
- 多随机种子训练只有在存在稳定验证指标后才有意义，不应仅按训练误差选模型。

### P1：缺少类间重叠和留出验证

当前所有样本都进入训练，建模完成只验证结构、序列化和最少 ROI 数，没有回答：

- 不同类别在 CIELAB 空间是否严重重叠。
- 某个 ROI 是否与自身类别不一致。
- 新光照或批次下的准确率和拒识率。

建议训练报告增加：每类留一 ROI 验证、混淆矩阵、已知类拒识率、Top1/Top2 面积差、覆盖率分布。每类少于 3 个独立 ROI 时只能给出“可训练但不可评估”的警告。

### P1：`similarity` 当前只是 `bestScore` 别名

当前 GMM：

```text
bestScore = 预测类接受像素 / 有效 ROI 像素
similarity = bestScore
categoryConfidence = 预测类接受像素 / 全部已分类像素
classifiedCoverage = 全部已分类像素 / 有效 ROI 像素
```

面积比例是合理的整体颜色分类得分，但不是 GMM 后验概率或模板距离。海康手册分别列出“最佳分数”和“相似度”，当前实现通过 `similarityAliasOf=bestScore` 已避免隐瞒，但产品语义仍不完全一致。

短期继续保持显式 alias，不伪造概率。若后续确实需要独立相似度，应使用 HALCON `evaluate_class_gmm`/`classify_class_gmm` 在确定性检测采样点上计算获胜类后验或 K-sigma 适配度，并明确聚合公式；不能把面积比例改名成 GMM 置信度。

### P2：每帧反序列化 GMM

当前检测每帧都从 Base64 解码、校验并 `deserialize_class_gmm`，连续检测会增加延迟和抖动。模型正确性稳定后，可按 artifact SHA-256、runtime 路径和通道合同缓存只读 GMM handle；替换模型和进程退出时清理。缓存属于性能优化，不能先于 P0/P1 正确性修改。

## 5. 与 HSV-C1 的定位关系

| 场景 | HSV-C1 | CIELAB GMM |
|---|---|---|
| 每类 1–3 个纯色 ROI | 更稳、更容易标定 | 可训练，但协方差和验证证据不足 |
| 光照变化但不需区分深浅 | H/S 可用 | a/b 更自然 |
| 同类存在多个色团、渐变、反光 | 全局直方图可表达分布，但空间无关 | 多中心可表达像素颜色分布 |
| ROI 混入背景 | 直方图相似度下降 | 背景可能被分到某类或拒识，依赖阈值和覆盖率 |
| 未知色拒识 | 最低分数；margin 当前仅诊断 | K-sigma 像素拒识 + 覆盖率/纯度判定 |
| 可解释性 | 类别 Top-N 样本相似度 | 类别面积、覆盖率、纯度、拒识面积 |

GMM 不应替代 HSV。对纯色、少样本任务，HSV-C1 通常更稳；对多色分布、同类多个颜色模式或需要像素级拒识的任务，修正后的 GMM 更有优势。

## 6. 推荐实施顺序

1. **GMM-D1 参数语义修复**：默认 K-sigma 阈值、UI 精度、文案、旧配置提示和检测 smoke。该阶段不重建模型。
2. **GMM-D2 训练诊断**：持久化实际中心数/迭代次数，MaxIter 警告，补留出验证数据结构。
3. **GMM-D3 HALCON 原生采样**：替换 C++ 选点，升级采样版本并使旧模型 stale。
4. **GMM-D4 验证集标定**：每类至少 3 个训练 ROI，推荐 5–10 个；另准备每类至少 20 张不参与训练的检测图，统计准确率、未知色拒识率、覆盖率和连续帧抖动。
5. **GMM-D5 独立相似度与缓存**：有数据证明需求后再增加后验/K-sigma 聚合和 runtime handle 缓存。

### 6.1 本轮实施状态（2026-07-17）

- GMM-D1 已完成：默认像素拒识阈值改为 `0.0001`，UI 支持 6 位小数并在 `>=0.01` 时提示高拒识风险；旧配置值保留，不静默覆盖。
- GMM-D3 已完成：采样升级为 `halcon_region_grid_points_v3`。C++ 仅计算二维网格密度，像素区域由 HALCON `gen_grid_region('points')`、`move_region`、`intersection` 生成；请求数、实际数和行列步长进入诊断，实际数进入模型 hash。
- 因 HALCON 20.11 网格步长离散化，实际采样数可能略低于请求配额。例如 80×40 ROI 请求 256 时实际为 240；保证不超过配额，并要求每类实际数达到请求数的 80% 以上。
- V2 及更早采样模型会因采样版本/构建签名不一致进入 stale，必须重新建立，避免新旧采样规则混用。
- GMM 建模、生产检测和 B2 UI 生命周期 smoke 均已通过。GMM-D2、D4、D5 仍按上述顺序保留为后续工作。
