# HALCON 20.11 迁移与颜色比较连续评分实施计划

> **执行要求：** 按任务顺序实施；每个任务先运行失败验证，再做最小修改使其通过。不得改动用户当前的 `.gitignore`、`scripts/` 和 `projects/scheme_0d5611a7/` 文件。

**目标：** 将工程 HALCON 编译/运行环境统一到 `/opt/halcon` 20.11.1，并用 HALCON 官方各向异性高斯频域算子消除颜色比较 H/S 硬量化导致的零分断崖。

**架构：** 新增一个共享 qmake 片段统一所有 HALCON 构建入口；`HalconRuntimePaths` 只提供 `/opt/halcon` 20.11 的默认运行时、OCR 和 license；颜色比较继续保存 V2 原始直方图，运行时通过动态加载的 `gen_gauss_filter + rft_generic + convol_fft` 生成连续评分，并在其后应用灰度分支和受限明暗因子。

**技术栈：** Qt 5.15.2、qmake、C++17、HALCON C API 20.11.1（运行时动态加载）、OpenCV 仅作输入容器和 smoke 图像构造。

**设计依据：** `docs/superpowers/specs/2026-07-14-halcon-20-11-color-comparison-continuous-scoring-design.md`

---

## Task 1：统一 qmake 到 HALCON 20.11.1

**文件：**

- 新建：`qmake/halcon_20_11.pri`
- 修改：`qt_ui_test.pro`
- 修改：`smoke/halcon_runtime_smoke.pro`
- 修改：`smoke/color_comparison_smoke.pro`
- 修改：`smoke/color_comparison_model_smoke.pro`
- 修改：`smoke/color_comparison_dialog_smoke.pro`
- 修改：`smoke/registered_classification_adapter_smoke.pro`
- 修改：`smoke/registered_classification_detection_dialog_smoke.pro`
- 修改：`smoke/registered_classification_dialog_smoke.pro`
- 修改：`smoke/registered_classification_feature_v2_smoke.pro`
- 修改：`smoke/registered_classification_knn_backend_smoke.pro`

### Step 1：记录当前失败合同

运行：

```bash
rg -n 'HALCON-24\.11|HALCON_ROOT = \$\$\(HALCONROOT\)' qt_ui_test.pro smoke/*.pro
```

预期：列出上述 10 个入口，证明它们仍包含 24.11 默认回退。

### Step 2：新增共享 qmake 片段

`qmake/halcon_20_11.pri` 固定以下合同：

```qmake
HALCON_ROOT = $$(HALCONROOT)
isEmpty(HALCON_ROOT): HALCON_ROOT = /opt/halcon

HALCON_VERSION_HEADER = $$HALCON_ROOT/include/HVersNum.h
!exists($$HALCON_VERSION_HEADER) {
    error("HALCON 20.11 header not found: $$HALCON_VERSION_HEADER")
}
!system(grep -Eq "HLIB_MAJOR_NUM[[:space:]]+20" $$shell_quote($$HALCON_VERSION_HEADER)) {
    error("HALCON major version must be 20")
}
!system(grep -Eq "HLIB_MINOR_NUM[[:space:]]+11" $$shell_quote($$HALCON_VERSION_HEADER)) {
    error("HALCON minor version must be 11")
}

INCLUDEPATH += $$HALCON_ROOT/include
message("Using HALCON 20.11 root: $$HALCON_ROOT")
```

主工程使用 `include(qmake/halcon_20_11.pri)`；smoke 使用 `include(../qmake/halcon_20_11.pri)`。删除每个 `.pro` 中重复的旧 HALCON_ROOT 块。

### Step 3：验证版本闸门

运行：

```bash
mkdir -p build/plan-qmake-valid
cd build/plan-qmake-valid
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
```

预期：输出 `Using HALCON 20.11 root: /opt/halcon`。

再运行：

```bash
mkdir -p ../plan-qmake-invalid
cd ../plan-qmake-invalid
HALCONROOT=/home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady \
  /home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
```

预期：qmake 非零退出并明确报告 major/minor 版本不符。

### Step 4：提交

```bash
git add qmake/halcon_20_11.pri qt_ui_test.pro smoke/*.pro
git commit -m "build: require HALCON 20.11"
```

提交前用 `git diff --cached --name-only` 确认没有暂存无关文件。

---

## Task 2：收敛公共运行时、OCR 与 license 路径

**文件：**

- 修改：`src/algorithms/halcon/HalconRuntimePaths.cpp`
- 修改：`src/algorithms/halcon/HalconRuntimePaths.h`
- 修改：`smoke/halcon_runtime_smoke.cpp`
- 修改：`src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp`

### Step 1：先把运行时 smoke 改成断言型测试

为 `halcon_runtime_smoke.cpp` 增加 `check()` 和失败计数，断言：

```cpp
check(HalconRuntimePaths::defaultHalconRoot() == QStringLiteral("/opt/halcon"),
      "default HALCON root must be /opt/halcon");
check(halconLib == QStringLiteral("/opt/halcon/lib/x64-linux/libhalconc.so"),
      "default HALCON runtime must resolve the 20.11 symlink");
check(HalconRuntimePaths::halconLibCandidates().contains(
          QStringLiteral("/opt/halcon/lib/x64-linux/libhalconc.so.20.11.1")),
      "versioned HALCON 20.11 runtime must be a candidate");
check(!HalconRuntimePaths::halconLibCandidates().join(';').contains("24.11"),
      "default runtime candidates must not contain HALCON 24.11");
check(HalconRuntimePaths::resolveOcrModelPath().startsWith("/opt/halcon/ocr/"),
      "OCR model must resolve below /opt/halcon/ocr");
check(license == QStringLiteral("/opt/halcon/license/license.txt"),
      "default readable HALCON license must be selected");
```

执行：

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake halcon_runtime_smoke.pro
make -j"$(nproc)"
../build/smoke/halcon_runtime/bin/halcon_runtime_smoke
```

预期：至少路径断言失败，因为当前实现仍返回 24.11。

### Step 2：最小化 `HalconRuntimePaths`

删除所有 24.11 和旧用户目录常量，固定：

```cpp
const QString kHalconRoot = QStringLiteral("/opt/halcon");
const QString kHalconRuntime =
        kHalconRoot + QStringLiteral("/lib/x64-linux/libhalconc.so");
const QString kHalconRuntimeVersioned =
        kHalconRoot + QStringLiteral("/lib/x64-linux/libhalconc.so.20.11.1");
const QString kHalconLicense =
        kHalconRoot + QStringLiteral("/license/license.txt");
```

候选顺序固定为：20.11 symlink、20.11 versioned、非空显式路径。这样旧方案里保存的 24.11 路径不会盖过本机 20.11。OCR 同样先查 `/opt/halcon/ocr/OCRB_0-9A-Z_NoRej.omc`，再查显式路径。

`initializeHalconEnvironment()` 的规则：

- 已设置 `HALCON_LICENSE_FILE`：保持用户值，不回退、不改写；无效时让 HALCON 返回明确授权错误。
- 未设置且 `/opt/halcon/license/license.txt` 可读：设置并返回该文件。
- 不可读：返回空字符串，不搜索旧 license。

在头文件增加只读版本方法：

```cpp
QString expectedHalconVersion(); // 固定返回 "20.11.1"
```

注册分类训练元数据的生产版本从 `24.11.1` 改成 `HalconRuntimePaths::expectedHalconVersion()`；不修改旧模型兼容测试中的历史 fixture。

### Step 3：验证

重新构建运行 `halcon_runtime_smoke`，预期全部断言通过且各 runner 不因路径迁移崩溃。额外运行：

```bash
rg -n 'HALCON-24\.11|halcon-24\.11|24\.11\.1' \
  src/algorithms/halcon src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp \
  qt_ui_test.pro smoke/*.pro
```

预期：生产路径和生产版本不再匹配；历史兼容 fixture 可单独保留并注明用途。

### Step 4：提交

```bash
git add src/algorithms/halcon/HalconRuntimePaths.cpp \
        src/algorithms/halcon/HalconRuntimePaths.h \
        src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp \
        smoke/halcon_runtime_smoke.cpp
git commit -m "fix: resolve HALCON 20.11 runtime paths"
```

---

## Task 3：先建立连续评分失败用例

**文件：**

- 修改：`smoke/color_comparison_smoke.cpp`

### Step 1：替换旧位移实现的静态合同

`checkRunnerSourceContract()` 改为要求以下 HALCON 20.11 符号，并禁止旧入口：

```cpp
check(code.contains("\"T_gen_image1\"") &&
      code.contains("\"T_gen_gauss_filter\"") &&
      code.contains("\"T_rft_generic\"") &&
      code.contains("\"T_convol_fft\""),
      "continuous scoring must resolve HALCON frequency-domain Gaussian symbols");
check(!code.contains("shiftedHistogramIntersection"),
      "continuous scoring must remove global histogram shift search");
```

继续要求 `T_tuple_min2` 和 `T_tuple_sum`，不再要求评分路径使用 `T_tuple_max`。

### Step 2：新增 licensed 数值矩阵

添加辅助函数，使用 `hsvFullToBgr()` 构造整幅纯色色块并完成“建模后检测”。至少覆盖：

```cpp
struct HsCase {
    const char *name;
    int templateHue;
    int templateSaturation;
    int detectHue;
    int detectSaturation;
    double minScore;
    double maxScore;
};
```

矩阵必须包含：

- 六色（红、黄、绿、青、蓝、紫）同色：`>= 99`。
- 六色各自 `H + 8, S - 32` 的 medium 相似色：`>= 80`。
- 六色各自 `H + 40, S - 32` 的 medium 明显色偏：`< 80`。
- 当前复现图的 HALCON bin 对：`H10/S31 -> H11/S27`，medium `80..90`。
- 对应明显色偏：`H10/S31 -> H15/S27`，medium `< 60`。
- 红色 `hue=1 -> hue=254`：`>= 99`，验证 Hue 循环。
- high、medium、low 对同一边界样本满足 `high < medium < low`。
- 相邻偏移序列输出有限、非负分数，不能从正分直接跳到精确 0。

注意：图像 HSV 的 0..255 值与 32-bin 坐标不同；当前 bin 对应使用直接构造的 V2 模型直方图测试辅助入口，六色矩阵使用真实 HALCON 特征提取链路。若不希望公开新 API，可在同一 `.cpp` 内以 Runner 完整链路验证真实图像，并用峰值断言确认落点。

### Step 3：运行并确认 RED

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake color_comparison_smoke.pro
make -j"$(nproc)"
RUN_HALCON_LICENSED_SMOKE=1 \
HALCONROOT=/opt/halcon \
HALCON_LICENSE_FILE=/opt/halcon/license/license.txt \
LD_LIBRARY_PATH=/opt/halcon/lib/x64-linux \
../build/smoke/color_comparison/bin/color_comparison_smoke
```

预期：静态合同以及相近颜色连续评分断言失败；相同色、明显异色等既有用例仍给出诊断。

### Step 4：提交测试

```bash
git add smoke/color_comparison_smoke.cpp
git commit -m "test: specify continuous color comparison scoring"
```

---

## Task 4：实现 HALCON 各向异性高斯评分

**文件：**

- 修改：`src/algorithms/recognition/ColorComparisonHalconRunner.cpp`

### Step 1：扩展动态 HALCON C API

在 `HalconCApi` 增加 `F_set_s` 和以下官方接口的精确函数类型：

```cpp
using SetStringFn = void (*)(Htuple *, const char *, Hlong);
using GenImage1Fn = Herror (*)(Hobject *, const Htuple, const Htuple,
                               const Htuple, const Htuple);
using GenGaussFilterFn = Herror (*)(Hobject *, const Htuple, const Htuple,
                                    const Htuple, const Htuple, const Htuple,
                                    const Htuple, const Htuple);
using RftGenericFn = Herror (*)(const Hobject, Hobject *, const Htuple,
                                const Htuple, const Htuple, const Htuple);
using ConvolFftFn = Herror (*)(const Hobject, const Hobject, Hobject *);
```

`HalconLibrary::load()` 必须解析：

```text
F_set_s
T_gen_image1
T_gen_gauss_filter
T_rft_generic
T_convol_fft
```

缺少任一符号沿用 `halcon_symbol_missing`，不得退回旧交集。

### Step 2：定义固定 profile

用结构体替换 `sensitivityRadius()`：

```cpp
struct SmoothingProfile {
    QString sensitivity;
    double hueSigma;
    double saturationSigma;
};

// high   = H 1.5 / S 4.0
// medium = H 3.0 / S 8.0
// low    = H 4.0 / S 10.0
```

无效字符串仍返回 `invalid_sensitivity`。

### Step 3：实现平滑函数

新增 `smoothHsHistogram()`：

1. 校验输入恰为 1024、值有限且非负。
2. 生成 `64×96` 的 `QVector<float>`：Hue 三周期，Saturation 左右各 16 列零填充。
3. 使用 `T_gen_image1` 创建 HALCON `real` 图像。像素指针通过单元素 `Hlong` tuple 传入，缓冲在整个算子调用期间保持有效。
4. 调用：

   ```text
   gen_gauss_filter(SaturationSigma,HueSigma,0,"n","rft",64,96)
   rft_generic(input,"to_freq","none","complex",64)
   convol_fft
   rft_generic(convolved,"from_freq","none","real",64)
   ```

5. 用现有 `T_get_grayval` 读取 row `32..63`、column `16..47`。
6. 对非有限、显著负值或总和不正抛出 `invalid_smoothed_histogram`；归一化继续使用 `T_tuple_sum + T_tuple_div`。

不得把平滑值写回 `ColorComparisonModelV2::values`。

### Step 4：替换旧评分

新增无位移 `histogramIntersection()`，用 `T_tuple_min2 + T_tuple_sum` 计算：

```cpp
const double rawIntersection = histogramIntersection(
        api, config.model.values, extracted.hsHistogram);
const QVector<double> smoothedTemplate = smoothHsHistogram(
        api, config.model.values, profile);
const QVector<double> smoothedDetect = smoothHsHistogram(
        api, extracted.hsHistogram, profile);
const double smoothedIntersection = histogramIntersection(
        api, smoothedTemplate, smoothedDetect);
```

删除 `shiftedHistogramIntersection()` 和全部 delta 搜索。

### Step 5：运行 Task 3 smoke

执行 Task 3 的 licensed 命令。预期：连续评分、六色、异色、Hue 循环和灵敏度顺序全部通过；若完整矩阵无法同时满足边界，停止实施并回到设计，不得调高总分或加入颜色特例。

### Step 6：提交

```bash
git add src/algorithms/recognition/ColorComparisonHalconRunner.cpp
git commit -m "feat: add HALCON continuous color scoring"
```

---

## Task 5：实现明暗和低饱和度连续语义

**文件：**

- 修改：`smoke/color_comparison_smoke.cpp`
- 修改：`src/algorithms/recognition/ColorComparisonHalconRunner.cpp`

### Step 1：先添加失败测试

licensed smoke 新增：

- 彩色同 H/S，V 差 `<= 0.10*255`：亮度因子为 1，分数不扣。
- V 差从 `0.10` 到 `0.40`：分数单调下降。
- V 差 `>= 0.40`：亮度因子固定 0.70。
- 两侧平均 S `<= 0.10`：同灰阶接近 100，V 差 0.25 接近 50，V 差 0.50 接近 0。
- 一侧 S `<= 0.10`、另一侧 S `>= 0.20`：分数 `<= 40`。
- 两侧 S 位于 0.10～0.20：分数连续，不在阈值处跳变。
- 开启既有光照补偿后，明暗因子使用 `detectMeanAfter`；关闭时使用 `detectMeanBefore`。

先运行 licensed smoke，预期新增断言失败。

### Step 2：实现平均饱和度和亮度函数

平均饱和度使用原始 32×32 H/S 直方图计算，不用平滑直方图：

```text
meanS = sum(histogram[h,s] * s / 31.0)
d = abs(templateMeanV - detectMeanV) / 255.0
```

彩色分支：

```text
d <= .10       factor = 1.00
.10 < d < .40  factor = 1.10 - d
d >= .40       factor = .70
colorScore = hsScore * factor
```

灰度与过渡分支采用确定性线性公式：

```text
grayScore = 100 * max(0, 1 - d / .50)
grayWeight = clamp((.20 - max(templateMeanS, detectMeanS)) / .10, 0, 1)
score = grayWeight * grayScore + (1 - grayWeight) * colorScore
```

若 `minMeanS <= .10`，再应用彩灰不匹配上限：

```text
mismatchProgress = clamp((maxMeanS - .10) / .10, 0, 1)
mismatchCap = 100 - 60 * mismatchProgress
score = min(score, mismatchCap)
```

这使 `.10..20` 连续过渡，并在一侧灰、一侧彩时最终封顶 40。

### Step 3：验证并提交

重新运行 licensed color smoke，预期新增测试及既有 brightness compensation 测试均通过。

```bash
git add smoke/color_comparison_smoke.cpp \
        src/algorithms/recognition/ColorComparisonHalconRunner.cpp
git commit -m "feat: add color brightness and grayscale scoring"
```

---

## Task 6：补齐 payload、错误状态和 Adapter 闭环

**文件：**

- 修改：`src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
- 修改：`smoke/color_comparison_smoke.cpp`
- 修改：`smoke/color_comparison_model_smoke.cpp`
- 如测试证明字段被 Adapter 丢弃才修改：`src/tooladapters/ColorComparisonAdapter.cpp`

### Step 1：添加 payload 失败断言

成功结果必须包含并校验类型/范围：

```text
rawIntersection
smoothedIntersection
hsScore
templateMeanSaturation
detectMeanSaturation
brightnessDifference
brightnessFactor
smoothingProfile.sensitivity
smoothingProfile.hueSigma
smoothingProfile.saturationSigma
smoothingProfile.hueCircular = true
smoothingProfile.saturationBoundary = "zero_pad"
halconRuntimePath
halconRuntimeVersion = "20.11.1"
```

还要断言：

- `similarity` 与最终 `score/100` 保持一致，避免旧 UI/Adapter 消费者语义断裂。
- `detectFeature` 仍是原始 1024 维直方图。
- 人为传入缺符号动态库时为 `halcon_symbol_missing`。
- 无法归一化的平滑输入走 `invalid_smoothed_histogram`，且 `measurementValid=false`，不得输出伪 0 测量。

先运行 smoke，预期新字段断言失败。

### Step 2：写入成功和失败 payload

`baseRunPayload()` 先放稳定默认值；成功分支覆盖真实值。runtime path 使用实际 `config.halconSoPath` 的绝对路径，版本使用 `HalconRuntimePaths::expectedHalconVersion()` 或同一受测常量。

Adapter 当前整体转发 Runner payload；只有 smoke 证明它覆盖/丢失字段时才做最小修改，不新增重复计算。

### Step 3：验证模型兼容和 Adapter

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake color_comparison_model_smoke.pro
make -j"$(nproc)"
../build/smoke/color_comparison_model/bin/color_comparison_model_smoke

/home/tt/Qt/5.15.2/gcc_64/bin/qmake color_comparison_smoke.pro
make -j"$(nproc)"
RUN_HALCON_LICENSED_SMOKE=1 \
HALCONROOT=/opt/halcon \
HALCON_LICENSE_FILE=/opt/halcon/license/license.txt \
LD_LIBRARY_PATH=/opt/halcon/lib/x64-linux \
../build/smoke/color_comparison/bin/color_comparison_smoke
```

预期：旧 V2 模型无需重建，新增 payload 和失败状态通过。

### Step 4：提交

```bash
git add src/algorithms/recognition/ColorComparisonHalconRunner.cpp \
        src/tooladapters/ColorComparisonAdapter.cpp \
        smoke/color_comparison_smoke.cpp smoke/color_comparison_model_smoke.cpp
git commit -m "feat: expose continuous color score diagnostics"
```

只暂存实际发生修改的文件。

---

## Task 7：更新颜色比较正式功能文档

**文件：**

- 修改：`docs/FID/ColorComparison/颜色比较V2设计说明.md`
- 修改：`docs/FID/ColorComparison/color_comparison_function_implementation.md`

### Step 1：消除过时合同

将正式 V2 文档中的：

- HALCON 24.11 改为 `/opt/halcon` 20.11.1。
- `deltaH/deltaS` 全局位移改为各向异性高斯 profile。
- HALCON 接口清单补充 `gen_image1`、`gen_gauss_filter`、`rft_generic`、`convol_fft`。
- 评分公式补充 raw/smoothed 交集、灰度过渡和亮度因子。
- payload、错误状态和回归结果同步到真实实现。

实现记录必须写明哪些自动化验证已执行；人工 GUI 未执行时明确标记“待人工验证”。

### Step 2：文档一致性检查

```bash
rg -n '24\.11|deltaH|deltaS|shiftedHistogramIntersection|kernelSize' \
  docs/FID/ColorComparison/颜色比较V2设计说明.md \
  docs/FID/ColorComparison/color_comparison_function_implementation.md
```

预期：只保留有明确“历史方案/已废弃”说明的匹配。

### Step 3：提交

```bash
git add docs/FID/ColorComparison/颜色比较V2设计说明.md \
        docs/FID/ColorComparison/color_comparison_function_implementation.md
git commit -m "docs: document HALCON 20.11 color scoring"
```

---

## Task 8：完整自动化与 GUI 验收

**文件：**

- 验证为主；若发现真实回归，只修改对应最小范围文件。

### Step 1：颜色比较三组 smoke

```bash
cd smoke
for pro in color_comparison_model_smoke.pro \
           color_comparison_smoke.pro \
           color_comparison_dialog_smoke.pro; do
  /home/tt/Qt/5.15.2/gcc_64/bin/qmake "$pro"
  make -j"$(nproc)"
done

../build/smoke/color_comparison_model/bin/color_comparison_model_smoke
RUN_HALCON_LICENSED_SMOKE=1 HALCONROOT=/opt/halcon \
HALCON_LICENSE_FILE=/opt/halcon/license/license.txt \
LD_LIBRARY_PATH=/opt/halcon/lib/x64-linux \
../build/smoke/color_comparison/bin/color_comparison_smoke
QT_QPA_PLATFORM=offscreen \
../build/smoke/color_comparison_dialog/bin/color_comparison_dialog_smoke
```

预期：全部通过，overlay 分数与状态栏一致。

### Step 2：HALCON 与注册分类回归

依次 qmake、make、运行：

```text
halcon_runtime_smoke.pro
registered_classification_feature_v2_smoke.pro
registered_classification_knn_backend_smoke.pro
registered_classification_adapter_smoke.pro
registered_classification_dialog_smoke.pro
registered_classification_detection_dialog_smoke.pro
```

需要 license 的用例统一设置：

```bash
HALCONROOT=/opt/halcon \
HALCON_LICENSE_FILE=/opt/halcon/license/license.txt \
LD_LIBRARY_PATH=/opt/halcon/lib/x64-linux
```

预期：迁移只改变运行时版本和路径，不改变其他算子行为。

### Step 3：完整 Qt 影子构建

```bash
cd ..
mkdir -p build/qt_ui_test_20_11
cd build/qt_ui_test_20_11
HALCONROOT=/opt/halcon \
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
make -j"$(nproc)"
```

预期：qmake 和 make 均成功，产物只在 `build/`。

### Step 4：人工 GUI 检查

在颜色比较对话框逐项检查：

1. 红、黄、绿、青、蓝、紫同色与近似色。
2. 红色 Hue 首尾。
3. 灰色、黑白和不同灰阶。
4. `tests/Images/1.png` 当前绿色复现场景。
5. 真实瓶盖/按钮图像。
6. high/medium/low 切换后分数符合严格到宽松顺序。
7. 画布 `OK/NG score` 与底部状态栏完全一致。

人工未执行时，不得写“功能全部验收完成”。

### Step 5：最终卫生检查和提交

```bash
git diff --check
git status --short
git ls-files | rg '(^|/)(Makefile|moc_.*\.cpp|ui_.*\.h|.*\.o|qt_ui_test|.*_smoke|.*\.log)$'
```

预期：没有新增构建产物；用户原有 `.gitignore`、`scripts/` 和方案文件仍保持其原状态且未进入本功能提交。

如验证修复产生代码改动，单独提交：

```bash
git commit -m "test: verify HALCON 20.11 color comparison"
```

