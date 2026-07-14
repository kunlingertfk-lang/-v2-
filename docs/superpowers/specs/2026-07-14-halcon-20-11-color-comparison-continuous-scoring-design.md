# HALCON 20.11 迁移与颜色比较连续评分设计

## 1. 目标

本设计同时完成两项收敛工作：

1. 将工程编译和运行时 HALCON 统一到 `/opt/halcon` 的 20.11.1，消除当前 24.11 头文件、绝对动态库路径和运行环境混用的风险。
2. 保留颜色比较 V2 原始模型合同，通过 HALCON 20.11 对 H/S 二维直方图进行循环边界平滑，消除相近颜色因硬量化落入不同 bin 而直接得到 `0.0` 的断崖。

评分语义采用“颜色为主、明显明暗差异才扣分”。不新增 UI 控件，不改变最低分判断方式，不引入 OpenCV 或自写视觉算法替代 HALCON。

## 2. 已确认问题

当前颜色比较使用 32×32 H/S 联合直方图和硬直方图交集。灵敏度只控制有限的全局 bin 位移：high=0、medium=1、low=2。只要模板和检测直方图的非零支撑集合没有对齐，交集就精确等于零。

在 HALCON 20.11 正式 Runner 上对 `tests/Images/1.png` 的绿色样本复现结果为：

```text
templatePeak = H10/S31
detectPeak   = H11/S27
high   score = 0
medium score = 0
low    score = 0.888889
```

这证明问题不是结果文字显示、最低分阈值或单一绿色特例，而是硬量化评分缺少相邻 bin 连续性。

## 3. 范围

### 3.1 本轮包含

- qmake、公共 HALCON 路径解析和相关 smoke 统一到 `/opt/halcon` 20.11.1。
- 颜色比较 Runner 增加 HALCON 直方图平滑与连续评分。
- Hue 首尾循环处理。
- V 明暗差异的受控扣分和低饱和度处理。
- 稳定诊断 payload、错误状态和完整回归矩阵。
- 保留现有颜色比较 V2 模型与配置结构。

### 3.2 本轮不包含

- 不新增多样本模板、色谱特征或位置修正实现。
- 不修改颜色识别、OCR、注册分类及有无类算子的算法语义；只验证它们在 HALCON 20.11 下仍能构建和运行。
- 不实现或修改 HALCON 授权机制。仅配置现有 `/opt/halcon/license/license.txt`；授权失败时保留明确错误。
- 不为现场数据自动学习阈值，也不新增用户可调平滑参数。

## 4. HALCON 20.11 迁移

### 4.1 编译环境

- `qt_ui_test.pro` 和所有使用 HALCON 头文件的 smoke 默认使用 `/opt/halcon/include`。
- 构建时读取 `HVersNum.h`，要求 major=20、minor=11；版本不符时 qmake 明确失败。
- 删除生产构建中的 24.11 默认回退，禁止 20.11 头文件与 24.11 动态库混用。

### 4.2 运行时与模型资源

公共 `HalconRuntimePaths` 默认根目录改为 `/opt/halcon`，动态库候选顺序固定为：

1. `/opt/halcon/lib/x64-linux/libhalconc.so`
2. `/opt/halcon/lib/x64-linux/libhalconc.so.20.11.1`

OCR 默认模型目录改为 `/opt/halcon/ocr`。已有配置中的 24.11 绝对路径不再优先于公共 20.11 路径；解析结果和候选列表继续进入 payload，便于定位实际加载库。

`initializeHalconEnvironment()` 在用户未显式配置 `HALCON_LICENSE_FILE` 时，仅在可读的情况下使用 `/opt/halcon/license/license.txt`。不修改文件内容，不吞掉 HALCON 授权错误。

### 4.3 模型兼容

HALCON 20.11 已确认提供并导出本设计需要的 `histo_2dim`、`get_grayval`、`gen_image1`、`gen_gauss_filter`、`rft_generic`、`convol_fft` 和 tuple 算子。

20.11 canary 与当前外部模型坐标合同一致：`histo_2dim(Region,H,S)` 按 `row=S,column=H` 读取后组织为 `hueBin*32+saturationBin`。因此继续使用 V2 原始模型，不升级 V3，不强制重新取样。若正式 canary 得到不同结果，实施必须停止，不允许自动转换模型。

## 5. 连续 H/S 评分

### 5.1 输入与输出

输入保持为模板和检测的 1024 维、非负、归一化、hue-major H/S 联合直方图。模型中仍保存未平滑原始值；运行时平滑结果不得写回模型。

输出包括：

- `rawIntersection`：原始硬交集，仅作诊断。
- `smoothedIntersection`：平滑并重新归一化后的交集。
- `hsScore = smoothedIntersection * 100`。
- 最终 `score`：应用低饱和度规则和明暗因子后的结果。

所有分数必须为有限值并限制在 `[0,100]`。

### 5.2 HALCON 平滑链路

对模板与检测使用完全相同的链路：

1. 在 Saturation 两侧各增加 16 列零填充，并将 Hue 方向复制三份，形成 `64×96` 的 hue-major HALCON `real` 图像。`real` 像素缓冲必须使用 32 位 `float`。
2. 使用 `gen_gauss_filter(SaturationSigma,HueSigma,0,'n','rft',64,96)` 生成 HALCON 20.11 官方各向异性高斯频域滤波器。
3. 使用 `rft_generic('to_freq','none','complex',64)`、`convol_fft`、`rft_generic('from_freq','none','real',64)` 完成平滑。
4. `get_grayval` 从中间 Hue 周期和中间 Saturation 区域取回 32×32、共 1024 个值。
5. 使用 HALCON tuple 求和并重新归一化。
6. 使用 `tuple_min2 + tuple_sum` 计算平滑直方图交集。

Hue 循环扩展保证红色在 0/360° 两侧相邻；Saturation 零填充且不循环，越界质量不得从低饱和度回绕到高饱和度。使用频域滤波不是自行实现高斯算法，而是为了调用 HALCON 20.11 支持不同 Hue/Saturation 标准差的官方算子链路。

### 5.3 灵敏度

高、中、低继续作为内部固定 profile，不新增 UI 参数。固定参数为：

| 灵敏度 | Hue Sigma | Saturation Sigma |
| --- | ---: | ---: |
| high | 1.5 | 4.0 |
| medium | 3.0 | 8.0 |
| low | 4.0 | 10.0 |

参数来自 HALCON 20.11 实测，而不是分数整体抬升。对当前复现样本，medium 的结果为：

```text
H10/S31 -> H11/S27（相似色）  81.5066
H10/S31 -> H15/S27（明确色偏）39.5854
```

原等方 `gauss_filter` 方案已被实测否决：其最大允许档位 11 对相似样本也只有 49.1903，无法达到最低分 80；继续扩大等方核还会同步抬高 Hue 异色分数。各向异性参数必须写入 Runner、smoke 断言和功能文档。默认 medium 必须使批准的相近数字色块判定 OK，同时所有明确色偏样本保持 NG；若完整回归矩阵不能满足边界，实施必须停止并返回设计阶段，不得增加经验旁路或整体抬分。

旧的全局位移搜索从最终评分中移除，避免同时叠加两套容差；`rawIntersection` 保留无位移硬交集用于诊断。

## 6. 明暗与低饱和度

### 6.1 彩色区域

从 H/S 直方图计算归一化平均饱和度，从 HALCON `intensity` 结果取得模板和检测平均 V。定义：

```text
brightnessDifference = abs(templateMeanV - detectMeanV) / 255
```

彩色区域的亮度因子为：

```text
d <= 0.10: brightnessFactor = 1.00
0.10 < d < 0.40: brightnessFactor = 1.10 - d
d >= 0.40: brightnessFactor = 0.70
```

最终彩色分数：

```text
score = hsScore * brightnessFactor
```

这保证普通亮度波动不扣分，明显明暗差异最多扣 30%，颜色仍是主体。

### 6.2 灰度区域

- 模板和检测平均饱和度均不高于 0.10 时，Hue 不参与判断，使用连续 V 差异：

  ```text
  grayScore = 100 * max(0, 1 - brightnessDifference / 0.50)
  ```

- 一侧平均饱和度不高于 0.10、另一侧不低于 0.20 时，彩色与灰色不应匹配，最终分数上限为 40。
- 饱和度位于 0.10～0.20 时，在灰度分数和彩色分数间线性过渡，避免阈值跳变。

## 7. Payload 与失败状态

成功结果增加：

```text
rawIntersection
smoothedIntersection
hsScore
templateMeanSaturation
detectMeanSaturation
brightnessDifference
brightnessFactor
smoothingProfile {
  sensitivity,
  hueSigma,
  saturationSigma,
  hueCircular=true,
  saturationBoundary="zero_pad"
}
halconRuntimePath
halconRuntimeVersion = "20.11.1"
```

失败行为：

- `/opt/halcon` 或 20.11 头文件缺失：qmake 失败。
- 动态库或新算子符号缺失：`halcon_symbol_missing`。
- license 错误：`halcon_license_error`，保留 HALCON 错误码和文本。
- 平滑结果包含非有限值、负数或无法归一化：`invalid_smoothed_histogram`。
- 20.11 坐标 canary 与模型合同冲突：验证失败并停止实施。

任何失败路径都不得静默退回旧硬交集算法，也不得生成伪造的 `0.0` 测量结果。

## 8. 回归测试

### 8.1 HALCON 运行时

`halcon_runtime_smoke` 必须验证：

- 默认根目录、头文件、动态库、OCR 模型均来自 `/opt/halcon`。
- 版本为 20.11.1。
- 24.11 路径不再出现在默认候选和成功 payload。
- 缺库、缺符号和 license 错误均返回明确状态。

### 8.2 颜色数值矩阵

licensed `color_comparison_smoke` 覆盖：

- 相同 ROI 接近 100。
- 红、黄、绿、青、蓝、紫的相近数字色块在 medium 下达到最低分 80。
- 明确色偏数字色块在 medium 下低于 80。
- 相邻 H/S bin 的分数连续变化，不因单个 bin 边界直接归零。
- 红色 Hue 首尾两侧保持高相似度。
- 同色正常、偏暗、强光为渐进下降；偏黄光、偏蓝光下降更明显。
- 灰色轻微明暗变化保持相似，黑白或明显灰阶差异扣分。
- 纯色块与真实物体产生可解释的连续分数；不要求必然 OK，但不得因量化边界无故为零。
- 所有明确负样本必须保留判定边界，不能通过整体抬分通过正样本。

### 8.3 工程回归

- `color_comparison_model_smoke`：旧 V2 模型继续通过。
- `color_comparison_dialog_smoke`：保存、回显、异步测试和 overlay 不回归。
- 注册分类相关 HALCON smoke 全部使用 20.11 重新构建执行。
- 完整 `qmake + make` 通过。
- 人工 GUI 检查红、绿、蓝、灰和真实瓶盖，确认画布分数与状态栏一致。

## 9. 完成标准

- 编译、动态加载和 payload 中的 HALCON 版本统一为 20.11.1。
- 默认 medium 对批准的相近数字色块判定 OK，明确色偏样本保持 NG。
- 当前绿色 `H10/S31` 对 `H11/S27` 不再因硬量化直接得到 0。
- 红色循环、亮度扣分和灰度分支均有 licensed HALCON 回归。
- 无 OpenCV、自写分类器或第三方算法进入颜色比较核心。
- 自动化验证通过；人工 GUI 未执行时必须明确标记，不能宣称视觉验收完成。
