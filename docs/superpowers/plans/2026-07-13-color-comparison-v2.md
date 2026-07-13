# 颜色比较 V2 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将现有颜色比较升级为具有严格输入/模型合同、HALCON 32×32 H/S 联合直方图交集、光照补偿、可靠保存回显和完整诊断的 V2 工具。

**Architecture:** `FrameInputMetadata` 在图像转 BGR 前保留原始 Mono/Color 信息；`ColorComparisonModel` 统一 V2 JSON、状态、哈希和严格验证；`ColorComparisonHalconRunner` 独立完成模板建模与检测比较，不再调用颜色识别 Runner。Adapter 负责 ToolConfig/ToolRequest 到强类型配置的唯一映射，Dialog 只维护模型生命周期、UI 和异步运行。

**Tech Stack:** C++17、Qt 5.15.2 Core/Gui/Widgets/Concurrent/JSON、HALCON 24.11.1 C API 动态加载、OpenCV 4.8 `cv::Mat` 桥接与测试图生成、qmake smoke 工程。

## Global Constraints

- 设计依据为 `docs/FID/ColorComparison/颜色比较V2设计说明.md` 和 `docs/superpowers/specs/2026-07-13-color-comparison-v2-design.md`。
- 核心视觉与相似度必须使用 HALCON；OpenCV 只用于 `cv::Mat`、显示、格式桥接和测试 fixture。
- 正式模型固定为 `version=2`、`featureType="histogram_hs_2d"`、`algorithm="histogram_intersection"`、`hueBins=32`、`saturationBins=32`、`layout="hue_major"`。
- `histo_2dim(Region, H, S)` 固定解释为 row=Hue、column=Saturation，索引为 `hueBin*32+saturationBin`。
- 同一归一化分布必须得到 100 分；不得保留 C++ Gaussian kernel、主峰经验权重、伪 Bhattacharyya 或 OpenCV `compareHist` 生产旁路。
- 光照补偿是受限亮度归一化，不把 V 通道差异直接加入扣分；范围和截断门槛必须由 licensed/现场数据标定。
- V1 裸模型只允许 stale/unsupported 后重新取样，不允许转置、静默迁移或继续使用旧评分。
- 位置修正只保留接口；UI 禁用，运行不应用，payload 输出 warning。
- 色谱特征禁用并返回 `unsupported_feature`。
- `custom` 模式下检测 ROI/圆形/检测 Mask 变化不使模型失效；`sync` 模式下三者属于模板提取参数，变化必须 stale，sync 建模先应用检测 Mask 再叠加模板 Mask。
- 本轮不修改 `ColorRecognitionHalconRunner` 的算法或既有评分行为；其 H/S 轴问题另立任务。
- 不修改用户现有改动：`docs/FID/RegisteredClassification/tempFunc.md`、`projects/scheme_0d5611a7/reference.png`、`projects/scheme_0d5611a7/scheme.json` 和本地智能相机手册 PDF。
- 所有手工修改使用 `apply_patch`；构建使用 `build/` 影子目录；构建产物不入库。
- 显式 `RUN_HALCON_LICENSED_SMOKE=1` 时，HALCON license 缺失必须使测试失败。当前环境已知可能报 `#2036`，通过前不得宣称 V2 数值验证完成。

---

## File Map

**Create:**

- `src/frame/FrameInputMetadata.{h,cpp}`：原始颜色模式、像素格式、通道、位深和来源。
- `src/algorithms/recognition/ColorComparisonModel.{h,cpp}`：V2 模型、状态、JSON、哈希和严格验证。
- `smoke/color_comparison_model_smoke.{cpp,pro}`：模型、迁移和 Adapter 合同。
- `smoke/frame_input_metadata_smoke.{cpp,pro}`：来源元数据和方案 round-trip。
- `smoke/color_comparison_dialog_smoke.{cpp,pro}`：Dialog V2 保存回显和状态机。

**Modify:**

- `src/frame/CameraFrameProvider.{h,cpp}`
- `src/frame/ReferenceImageProvider.{h,cpp}`
- `src/SchemeStore.{h,cpp}`
- `src/toolcore/ToolEngine.{h,cpp}`
- `src/MainWindow.cpp`
- `src/algorithms/recognition/ColorComparisonHalconRunner.{h,cpp}`
- `src/tooladapters/ColorComparisonAdapter.{h,cpp}`
- `src/ColorComparisonDialog.{h,cpp}`
- `smoke/color_comparison_smoke.{cpp,pro}`
- `qt_ui_test.pro`
- `docs/FID/ColorComparison/color_comparison_function_implementation.md`

`ui/ColorComparisonDialog.ui` 当前只是占位文件，Dialog 由 `buildUi()` 构建；本轮不迁移到 Designer。

---

### Task 1: 固化 V2 模型、状态和迁移合同

**Files:**

- Create: `src/algorithms/recognition/ColorComparisonModel.h`
- Create: `src/algorithms/recognition/ColorComparisonModel.cpp`
- Create: `smoke/color_comparison_model_smoke.cpp`
- Create: `smoke/color_comparison_model_smoke.pro`
- Modify: `qt_ui_test.pro`

**Interfaces:**

- Produces: `ColorComparisonModelState`、`ColorComparisonInputSignature`、`ColorComparisonBrightnessReference`、`ColorComparisonModelV2`。
- Produces: `colorComparisonModelToJson(...)`、`readColorComparisonModel(...)`、`validateColorComparisonModel(...)`。
- Produces: `colorComparisonReferenceHash(...)`、`colorComparisonExtractParamsHash(...)`。

- [ ] **Step 1: 写模型合同失败测试**

`smoke/color_comparison_model_smoke.cpp` 的核心断言：

```cpp
ColorComparisonModelV2 model;
model.state = ColorComparisonModelState::Ready;
model.featureType = QStringLiteral("histogram_hs_2d");
model.algorithm = QStringLiteral("histogram_intersection");
model.colorSpace = QStringLiteral("hsv");
model.hueBins = 32;
model.saturationBins = 32;
model.layout = QStringLiteral("hue_major");
model.normalized = true;
model.values = QVector<double>(1024, 0.0);
model.values[5 * 32 + 20] = 1.0;
model.valueHistogram = QVector<double>(32, 0.0);
model.valueHistogram[18] = 1.0;
model.effectivePixelCount = 4096;
model.referenceImageHash = QStringLiteral("reference-hash");
model.extractParamsHash = QStringLiteral("extract-hash");
model.inputSignature.colorMode = QStringLiteral("color");
model.inputSignature.pixelFormat = QStringLiteral("BGR8");
model.inputSignature.bitDepth = 8;

check(validateColorComparisonModel(model).success,
      "valid V2 model must pass strict validation");

const QJsonObject root{
    {QStringLiteral("version"), 2},
    {QStringLiteral("model"), colorComparisonModelToJson(model)}
};
const ColorComparisonModelReadResult roundTrip =
        readColorComparisonModel(root, true);
check(roundTrip.success &&
      roundTrip.model.state == ColorComparisonModelState::Ready &&
      roundTrip.model.values.size() == 1024,
      "V2 model must round-trip as ready");

QJsonObject legacy;
legacy.insert(QStringLiteral("version"), 1);
legacy.insert(QStringLiteral("templateFeature"), QJsonArray{1.0, 0.0});
check(readColorComparisonModel(legacy, true).status == QStringLiteral("model_stale"),
      "V1 with reference must require re-sampling");
check(readColorComparisonModel(legacy, false).status ==
      QStringLiteral("model_rebuild_required"),
      "V1 without reference must be unsupported");
```

同一 smoke 断言：1023/1025 维、NaN、负数、总和不为 1、错误 bins/layout、空哈希、`effectivePixelCount<=0`、31 维 V 直方图均为 `model_invalid`；未知版本为 `unsupported_model_version`；相同输入哈希稳定，参数变化时哈希变化。

- [ ] **Step 2: 运行测试确认缺少模型 API**

```bash
mkdir -p build
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_model_smoke.pro -o build/color_comparison_model.Makefile
make -C build -f color_comparison_model.Makefile -j8
```

Expected: 编译失败，包含 `ColorComparisonModelV2 was not declared`。

- [ ] **Step 3: 实现公开模型合同**

`ColorComparisonModel.h` 固定公开：

```cpp
enum class ColorComparisonModelState { Empty, Stale, Ready, Invalid, Unsupported };

struct ColorComparisonInputSignature {
    QString colorMode = QStringLiteral("unknown");
    QString pixelFormat;
    int bitDepth = -1;
    QJsonValue whiteBalance = QJsonValue::Null;
    QJsonValue ccm = QJsonValue::Null;
    QJsonValue exposure = QJsonValue::Null;
    QJsonValue gain = QJsonValue::Null;
};

struct ColorComparisonBrightnessReference {
    double mean = 0.0;
    double deviation = 0.0;
};

struct ColorComparisonModelV2 {
    ColorComparisonModelState state = ColorComparisonModelState::Empty;
    QString featureType = QStringLiteral("histogram_hs_2d");
    QString algorithm = QStringLiteral("histogram_intersection");
    QString colorSpace = QStringLiteral("hsv");
    int hueBins = 32;
    int saturationBins = 32;
    QString layout = QStringLiteral("hue_major");
    bool normalized = true;
    QVector<double> values;
    QVector<double> valueHistogram;
    qint64 effectivePixelCount = 0;
    QString referenceImageHash;
    QString extractParamsHash;
    ColorComparisonInputSignature inputSignature;
    ColorComparisonBrightnessReference brightnessReference;
};

struct ColorComparisonModelValidation {
    bool success = false;
    QString status;
    QString message;
};

struct ColorComparisonModelReadResult {
    bool success = false;
    bool requiresRebuild = false;
    QString status;
    QString message;
    ColorComparisonModelV2 model;
};

QString colorComparisonModelStateName(ColorComparisonModelState state);
QJsonObject colorComparisonModelToJson(const ColorComparisonModelV2 &model);
ColorComparisonModelReadResult readColorComparisonModel(
        const QJsonObject &colorComparison, bool referenceAvailable);
ColorComparisonModelValidation validateColorComparisonModel(
        const ColorComparisonModelV2 &model,
        const QString &expectedExtractParamsHash = QString());
QString colorComparisonReferenceHash(const cv::Mat &image);
QString colorComparisonExtractParamsHash(const QJsonObject &extractParams);
```

- [ ] **Step 4: 实现严格验证、JSON 和哈希**

验证顺序固定为：

```cpp
if (model.state != ColorComparisonModelState::Ready)
    return {false,
            QStringLiteral("model_%1").arg(colorComparisonModelStateName(model.state)),
            QStringLiteral("颜色比较模型未处于 ready 状态")};
if (model.featureType != QStringLiteral("histogram_hs_2d") ||
        model.algorithm != QStringLiteral("histogram_intersection") ||
        model.colorSpace != QStringLiteral("hsv") ||
        model.hueBins != 32 || model.saturationBins != 32 ||
        model.layout != QStringLiteral("hue_major") || !model.normalized)
    return {false, QStringLiteral("model_invalid"),
            QStringLiteral("颜色比较模型合同不匹配")};
if (model.values.size() != 1024 || model.valueHistogram.size() != 32)
    return {false, QStringLiteral("model_invalid"),
            QStringLiteral("颜色比较模型维度错误")};

double sum = 0.0;
for (double value : model.values) {
    if (!std::isfinite(value) || value < 0.0)
        return {false, QStringLiteral("model_invalid"),
                QStringLiteral("颜色比较模型包含非法值")};
    sum += value;
}
if (std::abs(sum - 1.0) > 1e-6)
    return {false, QStringLiteral("model_invalid"),
            QStringLiteral("颜色比较模型未归一化")};
```

哈希使用 `QCryptographicHash::Sha256`；Mat 哈希逐行加入有效字节，并加入 rows/cols/type。V1 有参考图返回 Stale/`model_stale`，无参考图返回 Unsupported/`model_rebuild_required`。

- [ ] **Step 5: 构建运行并提交**

先把模型 codec 加入主工程：

```qmake
SOURCES += src/algorithms/recognition/ColorComparisonModel.cpp
HEADERS += src/algorithms/recognition/ColorComparisonModel.h
```

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_model_smoke.pro -o build/color_comparison_model.Makefile
make -C build -f color_comparison_model.Makefile -j8
./build/smoke/color_comparison_model/bin/color_comparison_model_smoke
git add src/algorithms/recognition/ColorComparisonModel.h \
        src/algorithms/recognition/ColorComparisonModel.cpp \
        smoke/color_comparison_model_smoke.cpp \
        smoke/color_comparison_model_smoke.pro qt_ui_test.pro
git commit -m "feat: add color comparison v2 model contract"
```

Expected: `color_comparison_model_smoke: V2 model contract checks passed`。

---

### Task 2: 保留并传递原始像素格式元数据

**Files:**

- Create: `src/frame/FrameInputMetadata.{h,cpp}`
- Create: `smoke/frame_input_metadata_smoke.{cpp,pro}`
- Modify: `src/frame/CameraFrameProvider.{h,cpp}`
- Modify: `src/frame/ReferenceImageProvider.{h,cpp}`
- Modify: `src/SchemeStore.{h,cpp}`
- Modify: `src/toolcore/ToolEngine.{h,cpp}`
- Modify: `src/MainWindow.cpp`
- Modify: `qt_ui_test.pro`

**Interfaces:**

- Produces: `FrameInputMetadata::fromMat/toJson/fromJson/isMono/isSupportedColor8`。
- Produces: Provider 的 `currentFrameMetadata()`、`referenceFrameMetadata()`。
- Produces: `SchemeState.referenceInputMetadata`。
- Changes: `ToolEngine::runTools(..., const QJsonObject &runtimeContext)`。

- [ ] **Step 1: 写来源元数据失败测试**

```cpp
const FrameInputMetadata mono =
        FrameInputMetadata::fromMat(cv::Mat(8, 8, CV_8UC1),
                                    QStringLiteral("camera"));
check(mono.isMono() && mono.pixelFormat == QStringLiteral("Mono8"),
      "CV_8UC1 must remain mono before BGR normalization");

const FrameInputMetadata color =
        FrameInputMetadata::fromMat(cv::Mat(8, 8, CV_8UC3),
                                    QStringLiteral("camera"));
check(color.isSupportedColor8(), "CV_8UC3 must be supported color8");
check(FrameInputMetadata::fromJson(color.toJson()).pixelFormat ==
      QStringLiteral("BGR8"), "metadata must JSON round-trip");

ReferenceImageProvider::instance().setReferenceFrame(
        cv::Mat(8, 8, CV_8UC1), mono);
check(ReferenceImageProvider::instance().referenceFrame().channels() == 3,
      "display frame may be normalized to BGR");
check(ReferenceImageProvider::instance().referenceFrameMetadata().isMono(),
      "reference metadata must preserve original mono source");
```

同一 smoke 保存/加载临时方案，断言 `referenceInputMetadata.colorMode=="mono"`。

- [ ] **Step 2: 运行测试确认 API 缺失**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/frame_input_metadata_smoke.pro -o build/frame_input_metadata.Makefile
make -C build -f frame_input_metadata.Makefile -j8
```

Expected: 编译失败，包含 `FrameInputMetadata has not been declared`。

- [ ] **Step 3: 实现元数据结构**

```cpp
struct FrameInputMetadata {
    QString colorMode = QStringLiteral("unknown");
    QString pixelFormat;
    int originalChannels = 0;
    int originalDepth = -1;
    QString source;

    static FrameInputMetadata fromMat(const cv::Mat &image,
                                      const QString &source);
    static FrameInputMetadata fromJson(const QJsonObject &json);
    QJsonObject toJson() const;
    bool isMono() const;
    bool isSupportedColor8() const;
};
```

映射固定：`CV_8UC1→mono/Mono8`、`CV_8UC3→color/BGR8`、`CV_8UC4→color/BGRA8`、`CV_8UC2→color/UYVY8`；其余为 unknown，不得强行标成 color。

- [ ] **Step 4: 在 Provider 和 SchemeStore 中保留 metadata**

Provider 使用：

```cpp
void setCurrentFrame(const cv::Mat &frame,
                     const FrameInputMetadata &metadata = FrameInputMetadata());
FrameInputMetadata currentFrameMetadata() const;

void setReferenceFrame(const cv::Mat &frame,
                       const FrameInputMetadata &metadata = FrameInputMetadata());
FrameInputMetadata referenceFrameMetadata() const;
```

未传 metadata 时从归一化前 Mat 推导。相机 worker 在 `decodeCapturedFrame()` 前保存 captured metadata；NV12 显式写 `color/NV12`，2 通道 V4L2 写 `color/UYVY8`。clear 同时清 metadata。

`SchemeState` 增加 `FrameInputMetadata referenceInputMetadata`，scheme JSON 键为 `referenceInputMetadata`。旧方案缺字段时为 unknown，不能根据重载后的 BGR PNG 伪造 color。

- [ ] **Step 5: 注入 ToolRequest.runtimeContext**

`ToolEngine` 改为：

```cpp
QVector<ToolResult> runTools(const QVector<ToolConfig> &configs,
                             const cv::Mat &image,
                             const cv::Mat &referenceImage = cv::Mat(),
                             const QJsonObject &runtimeContext = QJsonObject()) const;
```

循环设置 `request.runtimeContext = runtimeContext;`。MainWindow 在 QtConcurrent 前构造并按值捕获：

```cpp
QJsonObject runtimeContext;
runtimeContext.insert(QStringLiteral("input"),
        CameraFrameProvider::instance().currentFrameMetadata().toJson());
runtimeContext.insert(QStringLiteral("referenceInput"),
        ReferenceImageProvider::instance().referenceFrameMetadata().toJson());
```

- [ ] **Step 6: 运行、构建并提交**

先把元数据实现加入主工程：

```qmake
SOURCES += src/frame/FrameInputMetadata.cpp
HEADERS += src/frame/FrameInputMetadata.h
```

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/frame_input_metadata_smoke.pro -o build/frame_input_metadata.Makefile
make -C build -f frame_input_metadata.Makefile -j8
./build/smoke/frame_input_metadata/bin/frame_input_metadata_smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro -o build/Makefile
make -C build -j8
git add src/frame/FrameInputMetadata.h src/frame/FrameInputMetadata.cpp \
        src/frame/CameraFrameProvider.h src/frame/CameraFrameProvider.cpp \
        src/frame/ReferenceImageProvider.h src/frame/ReferenceImageProvider.cpp \
        src/SchemeStore.h src/SchemeStore.cpp \
        src/toolcore/ToolEngine.h src/toolcore/ToolEngine.cpp \
        src/MainWindow.cpp \
        smoke/frame_input_metadata_smoke.cpp smoke/frame_input_metadata_smoke.pro \
        qt_ui_test.pro
git commit -m "feat: preserve frame input metadata"
```

Expected: metadata smoke 通过，主工程编译成功。

---

### Task 3: 用独立 HALCON V2 路径替换旧评分

**Files:**

- Modify: `src/algorithms/recognition/ColorComparisonHalconRunner.{h,cpp}`
- Modify: `smoke/color_comparison_smoke.{cpp,pro}`

**Interfaces:**

- Consumes: Task 1 模型。
- Produces: `buildTemplateModel(...)`。
- Keeps: `run(const cv::Mat&, const ColorComparisonHalconConfig&) const`。
- Removes: 两个旧纯 C++ compare API 及对应 result structs。

- [ ] **Step 1: 用 V2 行为替换旧 smoke**

默认段先断言无需 HALCON 的前置状态：

```cpp
ColorComparisonHalconRunner runner;
ColorComparisonHalconConfig emptyModelConfig;
emptyModelConfig.model.state = ColorComparisonModelState::Empty;
const ColorComparisonHalconResult emptyModel =
        runner.run(cv::Mat(16, 16, CV_8UC3), emptyModelConfig);
check(!emptyModel.success &&
      emptyModel.status == QStringLiteral("model_empty"),
      "empty model must fail before HALCON loading");

ColorComparisonHalconConfig monoConfig;
monoConfig.inputSignature.colorMode = QStringLiteral("mono");
const ColorComparisonHalconResult mono =
        runner.run(cv::Mat(16, 16, CV_8UC3), monoConfig);
check(mono.success && !mono.ok && !mono.measurementValid &&
      mono.status == QStringLiteral("unsupported_color_input"),
      "original mono source must produce invalid NG");
```

licensed 段断言：

```cpp
const ColorComparisonTemplateBuildResult built =
        runner.buildTemplateModel(referenceImage, buildConfig);
check(built.success, "licensed template build must succeed");
runConfig.model = built.model;
const ColorComparisonHalconResult same =
        runner.run(referenceImage, runConfig);
check(same.success && same.ok &&
      std::abs(same.score - 100.0) < 1e-6,
      "identical image must score 100");
```

并覆盖真实 H/S 轴、红色回绕、相同边缘分布但不同 H-S 配对、多色比例、矩形/圆形、模板/检测 mask 和补偿 diagnostics。删除旧 soft-kernel、主峰、Bhattacharyya 和“同分布 94～99”断言。

- [ ] **Step 2: 运行测试确认旧接口失败**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_smoke.pro -o build/color_comparison.Makefile
make -C build -f color_comparison.Makefile -j8
```

Expected: 编译失败，包含 `buildTemplateModel is not a member`。

- [ ] **Step 3: 固化 Runner 结构**

```cpp
struct ColorComparisonHalconConfig {
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString templateRegionMode = QStringLiteral("custom");
    QRectF templateRoiNormalized = QRectF(0.0, 0.0, 0.5, 0.5);
    QVector<QPointF> templateMaskPolygonNormalized;
    QRectF detectRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString detectRegionType = QStringLiteral("rectangle");
    QPointF detectCircleCenterNormalized;
    double detectCircleRadiusNormalized = 0.0;
    QVector<QPointF> detectMaskPolygonNormalized;
    ColorComparisonModelV2 model;
    ColorComparisonInputSignature inputSignature;
    QString sensitivity = QStringLiteral("medium");
    bool brightnessCompensation = false;
    bool positionCorrectionRequested = false;
    QString positionCorrectionSourceId;
    int minScore = 80;
};

struct ColorComparisonTemplateBuildResult {
    bool success = false;
    QString status;
    QString message;
    ColorComparisonModelV2 model;
    QJsonObject payload;
};

struct ColorComparisonHalconResult {
    bool success = false;
    bool ok = false;
    bool measurementValid = false;
    QString status;
    QString message;
    double score = 0.0;
    double similarity = 0.0;
    qint64 elapsedMs = 0;
    QVector<double> detectFeature;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class ColorComparisonHalconRunner {
public:
    ColorComparisonTemplateBuildResult buildTemplateModel(
            const cv::Mat &referenceImage,
            const ColorComparisonHalconConfig &config) const;
    ColorComparisonHalconResult run(
            const cv::Mat &image,
            const ColorComparisonHalconConfig &config) const;
};
```

- [ ] **Step 4: 实现 HALCON 符号和 V2 特征**

删除 `ColorRecognitionHalconRunner.h` include。仅解析需要的 HALCON 符号：

```text
gen_image_interleaved, decompose3, compose3, trans_from_rgb,
gen_rectangle1, gen_circle, T_gen_region_polygon_filled,
difference, T_area_center, T_histo_2dim, T_get_grayval,
T_intensity, T_scale_image, T_tuple_select, T_tuple_min2,
T_tuple_sum, T_tuple_div, clear_obj
```

H/S 使用 `scale_image(...,31.0/255.0,0.0)` 量化，再对显式 Region 调用 `histo_2dim(Region,H,S)`。展平固定：

```cpp
QVector<double> feature(32 * 32, 0.0);
for (int hueBin = 0; hueBin < 32; ++hueBin) {
    for (int saturationBin = 0; saturationBin < 32; ++saturationBin) {
        feature[hueBin * 32 + saturationBin] =
                halconHistogramValue(hueBin, saturationBin);
    }
}
```

归一化使用 `tuple_sum/tuple_div`。V 生成 32 维诊断直方图，不加入 H/S 分数。

模板建模时，`templateRegionMode="custom"` 使用独立矩形模板 ROI；`templateRegionMode="sync"` 使用检测区域的实际形状和几何，检测区为圆形时模板也生成同一圆形 Region，并先对同步检测 mask 做 difference。模板 mask 随后在最终模板 Region 上再次做 difference。成功模型必须写入 referenceImageHash、覆盖模板模式/几何/有效 mask/补偿常量的 extractParamsHash、effectivePixelCount 和输入签名。

- [ ] **Step 5: 实现交集、容差和补偿**

无偏移评分固定：

```cpp
Htuple minimum;
Htuple intersection;
checkHalcon(symbols.tupleMin2(templateTuple, detectTuple, &minimum));
checkHalcon(symbols.tupleSum(minimum, &intersection));
const double similarity = qBound(0.0, tupleScalar(intersection), 1.0);
const double score = similarity * 100.0;
```

高档只比较 `deltaH=0,deltaS=0`；中档枚举 `deltaH=-1..1,deltaS=-1..1`；低档枚举 `deltaH=-2..2,deltaS=-2..2`。H 模 32 回绕，S 越界填 0；候选使用 `tuple_select + tuple_min2 + tuple_sum`，取最大 intersection。不得加入纯度、主峰或固定权重。

补偿用 `intensity` 得检测 V 均值，计算模板/检测 scale，对 RGB 三通道分别调用相同 `scale_image` 并 `compose3`，重新转 HSV。V2 初始常量固定为 `kMinBrightnessMean=8.0`、`kMaxBrightnessMean=247.0`、`kMinBrightnessScale=0.75`、`kMaxBrightnessScale=1.3333333333`、`kMaxClippedRatio=0.02`；越界返回 `invalid_illumination`。payload 输出 templateMean、detectMeanBefore/After、scale、clippedRatio。现场调整这些常量时必须改变 `extractParamsHash` 并重新取样验证。

- [ ] **Step 6: 实现状态、payload 和 overlay**

前置顺序：空图→原始 colorMode→Mat 类型→ROI/Mask→feature type→model validate→HALCON。`colorMode=mono` 为 `success=true/measurementValid=false/ok=false`；`colorMode=unknown` 在 Mat 类型合法时继续运行并添加 `input_color_mode_unknown` warning；正常 NG 为 `success=true/measurementValid=true/ok=false`。

payload 写入 measurementValid、passed、algorithm、featureType、modelVersion、score、similarity、threshold、有效像素数、brightnessCompensation、positionCorrection、detectionRoi、warnings。位置请求只添加 `position_correction_not_implemented`。overlay 只绘制当前检测 ROI/Mask。

- [ ] **Step 7: 运行并提交**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_smoke.pro -o build/color_comparison.Makefile
make -C build -f color_comparison.Makefile -j8
./build/smoke/color_comparison/bin/color_comparison_smoke
RUN_HALCON_LICENSED_SMOKE=1 ./build/smoke/color_comparison/bin/color_comparison_smoke
git add src/algorithms/recognition/ColorComparisonHalconRunner.h \
        src/algorithms/recognition/ColorComparisonHalconRunner.cpp \
        smoke/color_comparison_smoke.cpp smoke/color_comparison_smoke.pro
git commit -m "fix: replace color comparison with HALCON v2 scoring"
```

Expected: 默认 smoke 通过并明确 licensed 未执行；有效 license 下显式 smoke 全过。当前若仍报 `#2036`，显式 smoke 必须非零退出。

---

### Task 4: 统一 Adapter 的 V2 解析、建模和结果映射

**Files:**

- Modify: `src/tooladapters/ColorComparisonAdapter.{h,cpp}`
- Modify: `smoke/color_comparison_model_smoke.{cpp,pro}`

**Interfaces:**

- Consumes: Tasks 1、3。
- Produces: `ColorComparisonTemplateBuildResult buildTemplateModel(const ToolRequest&)`。
- Keeps: `supports(...)` 和 `run(...)`。

- [ ] **Step 1: 写 Adapter 失败测试**

```cpp
ToolRequest request;
request.config.toolId = QStringLiteral("color-comparison-v2");
request.config.toolType = ToolType::ColorComparison;
request.config.enabled = true;
request.image = cv::Mat(16, 16, CV_8UC3);
QJsonObject colorComparison;
colorComparison.insert(QStringLiteral("version"), 2);
colorComparison.insert(QStringLiteral("model"),
                       colorComparisonModelToJson(model));
colorComparison.insert(
        QStringLiteral("comparison"),
        QJsonObject{{QStringLiteral("sensitivity"),
                     QStringLiteral("medium")},
                    {QStringLiteral("brightnessCompensation"), false}});
request.config.params.insert(QStringLiteral("colorComparison"),
                             colorComparison);
request.config.judgeRule.insert(QStringLiteral("mode"),
                                QStringLiteral("min_score"));
request.config.judgeRule.insert(QStringLiteral("minScore"), 80);
request.runtimeContext.insert(QStringLiteral("input"),
        QJsonObject{{QStringLiteral("colorMode"), QStringLiteral("mono")},
                    {QStringLiteral("pixelFormat"), QStringLiteral("Mono8")},
                    {QStringLiteral("originalChannels"), 1},
                    {QStringLiteral("originalDepth"), 8}});

ColorComparisonAdapter adapter;
const ToolResult mono = adapter.run(request);
check(mono.success && !mono.ok &&
      mono.status == QStringLiteral("unsupported_color_input"),
      "Adapter must preserve grayscale NG semantics");
check(!mono.payload.value(QStringLiteral("measurementValid")).toBool(true),
      "grayscale measurement must be invalid");
```

再构造 unknown version、V1 有/无 reference、坏 V2、spectrum、position enabled，断言状态和 warnings。

- [ ] **Step 2: 运行测试确认 V1 fallback 失败**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_model_smoke.pro -o build/color_comparison_model.Makefile
make -C build -f color_comparison_model.Makefile -j8
```

Expected: 旧 Adapter 返回错误 fallback 或缺少 measurementValid，测试失败。

- [ ] **Step 3: 实现唯一配置映射**

头文件增加：

```cpp
ColorComparisonTemplateBuildResult buildTemplateModel(
        const ToolRequest &request);
```

严格读取：

```text
version
templateRegionMode
templateRoiNormalized / templateMaskPolygon
detectRegionType / detectRoiNormalized / detectCircleNormalized / detectMaskPolygon
model
comparison.sensitivity / comparison.brightnessCompensation
positionCorrection.enabled/sourceId/interfaceVersion
judgeRule.minScore
runtimeContext.input / runtimeContext.referenceInput
```

未知 enum 直接错误，不再 default。points parser 拒绝越界和退化多边形，不先 qBound。

- [ ] **Step 4: 映射结果和模板构建**

```cpp
result.success = runnerResult.success;
result.ok = runnerResult.ok;
result.status = runnerResult.status;
result.message = runnerResult.message;
result.score = runnerResult.score;
result.value = runnerResult.similarity;
result.count = runnerResult.measurementValid ? 1 : 0;
result.elapsedMs = runnerResult.elapsedMs;
result.text = QString::number(runnerResult.score, 'f', 2);
result.overlays = runnerResult.overlays;
result.payload = runnerResult.payload;
```

`buildTemplateModel` 使用 `request.referenceImage` 和 `runtimeContext.referenceInput`；缺参考图返回 `image_empty`。Adapter 不自动重建 V1，只有 Dialog 显式取样可以调用。

- [ ] **Step 5: 运行并提交**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_model_smoke.pro -o build/color_comparison_model.Makefile
make -C build -f color_comparison_model.Makefile -j8
./build/smoke/color_comparison_model/bin/color_comparison_model_smoke
git add src/tooladapters/ColorComparisonAdapter.h \
        src/tooladapters/ColorComparisonAdapter.cpp \
        smoke/color_comparison_model_smoke.cpp \
        smoke/color_comparison_model_smoke.pro
git commit -m "feat: map color comparison v2 requests"
```

Expected: 模型、迁移和 Adapter 合同全部通过。

---

### Task 4.5: 固化 sync 模式的检测 Mask 与 stale 合同

**Files:**

- Modify: `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
- Modify: `smoke/color_comparison_smoke.cpp`

**Interfaces:**

- Keeps: `ColorComparisonHalconRunner::buildTemplateModel(...)`、`run(...)` 公开签名不变。
- Changes: `templateRegionMode="sync"` 的模板有效 Region 与 `extractParamsHash` 同时纳入检测 Mask；`custom` 明确忽略检测几何和检测 Mask。

- [ ] **Step 1: 写 sync Mask 失败测试**

在 licensed smoke 中先构造 sync 配置，检测 ROI 使用全图，检测 Mask 使用非退化矩形多边形，模板 Mask 使用另一块不重叠多边形：

```cpp
ColorComparisonHalconConfig sync = baseConfig;
sync.templateRegionMode = QStringLiteral("sync");
sync.detectMaskPolygonNormalized = {
    QPointF(0.0, 0.0), QPointF(0.25, 0.0),
    QPointF(0.25, 1.0), QPointF(0.0, 1.0)
};
sync.templateMaskPolygonNormalized = {
    QPointF(0.75, 0.0), QPointF(1.0, 0.0),
    QPointF(1.0, 1.0), QPointF(0.75, 1.0)
};
const ColorComparisonTemplateBuildResult built =
        runner.buildTemplateModel(image, sync);
check(built.success && built.model.effectivePixelCount < image.total(),
      "sync template build must apply detection and template masks");

ColorComparisonHalconConfig changedSync = sync;
changedSync.model = built.model;
changedSync.detectMaskPolygonNormalized[1].setX(0.30);
const ColorComparisonHalconResult stale = runner.run(image, changedSync);
check(!stale.success && stale.status == QStringLiteral("model_stale"),
      "sync detection mask changes must stale the model");

ColorComparisonHalconConfig custom = sync;
custom.templateRegionMode = QStringLiteral("custom");
custom.detectMaskPolygonNormalized.clear();
const ColorComparisonTemplateBuildResult customBuilt =
        runner.buildTemplateModel(image, custom);
custom.model = customBuilt.model;
custom.detectMaskPolygonNormalized = sync.detectMaskPolygonNormalized;
custom.halconSoPath = QCoreApplication::applicationFilePath();
const ColorComparisonHalconResult customResult = runner.run(image, custom);
check(customResult.status != QStringLiteral("model_stale"),
      "custom detection mask changes must not stale the model");
```

- [ ] **Step 2: 运行测试确认新增断言失败**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_smoke.pro -o build/color_comparison.Makefile
make -C build -f color_comparison.Makefile -j8
RUN_HALCON_LICENSED_SMOKE=1 ./build/smoke/color_comparison/bin/color_comparison_smoke
```

Expected: 除已记录的本机 HALCON H/S 轴与红色回绕两项差异外，新增 sync Mask effective pixels 或 stale 断言失败。

- [ ] **Step 3: 实现双 Mask 模板 Region 和哈希**

`templateExtractParams(...)` 仅在 sync 模式加入以下字段；custom 模式不得写入空字段，以保持既有 custom V2 模型哈希兼容：

```cpp
if (mode == QStringLiteral("sync")) {
    params.insert(QStringLiteral("syncDetectionMaskPolygon"),
                  pointsToJson(config.detectMaskPolygonNormalized));
}
```

`createEffectiveRegion(...)` 的模板分支按顺序处理：基础检测几何 → sync 检测 Mask difference → 模板 Mask difference。custom 模式只处理模板 Mask；检测分支仍只处理检测 Mask。两个 difference 输出使用独立 `HalconObject`，禁止复用同一 HALCON 输出句柄或重复 clear。

- [ ] **Step 4: 构建运行并提交**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_smoke.pro -o build/color_comparison.Makefile
make -C build -f color_comparison.Makefile -j8
./build/smoke/color_comparison/bin/color_comparison_smoke
RUN_HALCON_LICENSED_SMOKE=1 ./build/smoke/color_comparison/bin/color_comparison_smoke
git diff --check
git add src/algorithms/recognition/ColorComparisonHalconRunner.cpp \
        smoke/color_comparison_smoke.cpp
git commit -m "fix: align sync color comparison masks"
```

Expected: 默认 preflight exit 0；licensed smoke 仅保留用户已裁决的本机 H/S 轴与红色回绕两项失败，新增 sync/custom Mask 断言通过。

---

### Task 5: 改造 Dialog 模型状态、UI 和异步运行

**Files:**

- Modify: `src/ColorComparisonDialog.{h,cpp}`
- Create: `smoke/color_comparison_dialog_smoke.{cpp,pro}`

**Interfaces:**

- Consumes: Tasks 1、2、4。
- Produces: V2 ToolConfig round-trip、模型状态、真实 H/S/V 图和非阻塞运行。

- [ ] **Step 1: 写 offscreen Dialog 失败测试**

```cpp
ColorComparisonDialog dialog;
QPushButton *basic = dialog.findChild<QPushButton *>(
        QStringLiteral("colorComparisonBasicButton"));
QPushButton *all = dialog.findChild<QPushButton *>(
        QStringLiteral("colorComparisonAllButton"));
QComboBox *feature = dialog.findChild<QComboBox *>(
        QStringLiteral("colorComparisonFeatureTypeCombo"));
QCheckBox *brightness = dialog.findChild<QCheckBox *>(
        QStringLiteral("colorComparisonBrightnessCompensation"));
QWidget *position = dialog.findChild<QWidget *>(
        QStringLiteral("colorComparisonPositionCorrectionPanel"));

check(basic && all && feature && brightness && position,
      "V2 controls must have stable object names");
check(!position->isEnabled(),
      "position correction UI must be disabled");
check(!(feature->model()->flags(feature->model()->index(1, 0)) &
        Qt::ItemIsEnabled),
      "spectrum feature must be disabled");

const ToolConfig before = dialog.toolConfig();
all->click();
basic->click();
const ToolConfig after = dialog.toolConfig();
check(before.params == after.params,
      "Basic/All switching must be pure visibility");
```

加载 ready V2 后断言：custom 模式检测 ROI/圆形/检测 Mask、minScore、sensitivity 不改 model hash；sync 模式检测 ROI/圆形/检测 Mask 使 state=stale；参考图 signal、模板 ROI/Mask、templateRegionMode、brightnessCompensation 使 state=stale；V1 显示重建提示；保存回显保持 version 2。

- [ ] **Step 2: 运行测试确认旧 UI 失败**

```bash
QT_QPA_PLATFORM=offscreen /home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_dialog_smoke.pro -o build/color_comparison_dialog.Makefile
make -C build -f color_comparison_dialog.Makefile -j8
QT_QPA_PLATFORM=offscreen ./build/smoke/color_comparison_dialog/bin/color_comparison_dialog_smoke
```

Expected: objectName、V2 state 或纯显隐断言失败。

- [ ] **Step 3: 改为强类型模型和明确失效**

头文件使用：

```cpp
ColorComparisonModelV2 m_model;
void markModelStale(const QString &reason);
bool rebuildTemplateModelFromFrame(const cv::Mat &frame,
                                   const FrameInputMetadata &metadata,
                                   QString *status,
                                   QString *message);
void updateModelStateUi();
void updateFeaturePreview();
QPixmap renderHistogram(const QVector<double> &values,
                        const QColor &color,
                        const QSize &size) const;
```

删除 `m_templateFeature` 和直接构造 `ColorRecognitionHalconConfig` 的代码。显式取样调用 `m_testAdapter.buildTemplateModel(request)`。

`setAllParamsMode` 只做 setVisible。参考图连接：

```cpp
connect(&ReferenceImageProvider::instance(),
        &ReferenceImageProvider::referenceFrameChanged,
        this,
        [this](const QImage &) {
            markModelStale(QStringLiteral("reference_changed"));
        });
```

模板模式/ROI/Mask/光照补偿置 stale；检测 ROI/圆形/检测 Mask 仅在 sync 模式置 stale，在 custom 模式不置 stale；sensitivity、minScore 始终不置 stale。`colorComparisonParams()` 始终保存 V2 嵌套结构，不读取 allButton 决定算法。

- [ ] **Step 4: 完成 UI 和真实统计图**

文案固定：

```text
模板区域：与检测区域同步 / 自定义
特征类型：直方图特征 / 色谱特征（待实现，禁用）
光照补偿：默认关闭
灵敏度：高（严格）/中（标准）/低（宽松）
位置修正：接口预留，暂未实现（整组禁用）
模型状态：empty/stale/ready/invalid/unsupported
```

从 1024 维 HS 计算 32 维 H/S 边缘分布，V 使用 `valueHistogram`，用 QPainter 在三个 QLabel 绘制真实柱形图。无 ready 模型时清空 pixmap 并显示状态文本。

- [ ] **Step 5: 将测试运行移出 UI 线程**

```cpp
QFutureWatcher<ToolResult> *m_testWatcher = nullptr;
quint64 m_testGeneration = 0;
bool m_pendingContinuousRun = false;
```

复制 ToolRequest 后使用 `QtConcurrent::run`；finished 只接受当前 generation。关闭 Dialog、退出测试和停止连续运行时递增 generation。

ToolRequest 注入：

```cpp
request.runtimeContext.insert(QStringLiteral("input"), metadata.toJson());
request.runtimeContext.insert(
        QStringLiteral("referenceInput"),
        ReferenceImageProvider::instance().referenceFrameMetadata().toJson());
```

状态栏同时显示 OK/NG、score、status、message。灰度显示无效 NG。当前图只显示检测 overlay，模板图单独显示模板 ROI/Mask。

- [ ] **Step 6: 运行、构建并提交**

```bash
QT_QPA_PLATFORM=offscreen /home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_dialog_smoke.pro -o build/color_comparison_dialog.Makefile
make -C build -f color_comparison_dialog.Makefile -j8
QT_QPA_PLATFORM=offscreen ./build/smoke/color_comparison_dialog/bin/color_comparison_dialog_smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro -o build/Makefile
make -C build -j8
git add src/ColorComparisonDialog.h src/ColorComparisonDialog.cpp \
        smoke/color_comparison_dialog_smoke.cpp \
        smoke/color_comparison_dialog_smoke.pro
git commit -m "feat: upgrade color comparison dialog to v2"
```

Expected: Dialog smoke 通过，主工程编译成功。

---

### Task 6: 工程核对、文档记录和完整验证

**Files:**

- Verify: `qt_ui_test.pro`
- Modify: `docs/FID/ColorComparison/color_comparison_function_implementation.md`
- Modify: `docs/FID/ColorComparison/颜色比较V2设计说明.md` only when implementation evidence corrects an inaccurate statement

- [ ] **Step 1: 核对 qmake**

```bash
rg -n 'FrameInputMetadata|ColorComparisonModel' qt_ui_test.pro
! rg -n 'ColorRecognitionHalconRunner' smoke/color_comparison_smoke.pro \
    smoke/color_comparison_model_smoke.pro \
    smoke/color_comparison_dialog_smoke.pro
```

Expected: 第一条同时找到新增 cpp 和 h；第二条无输出，确认颜色比较 smoke 不再链接颜色识别 Runner。

- [ ] **Step 2: 运行全部无 license smoke**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_model_smoke.pro -o build/color_comparison_model.Makefile
make -C build -f color_comparison_model.Makefile -j8
./build/smoke/color_comparison_model/bin/color_comparison_model_smoke

/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/frame_input_metadata_smoke.pro -o build/frame_input_metadata.Makefile
make -C build -f frame_input_metadata.Makefile -j8
./build/smoke/frame_input_metadata/bin/frame_input_metadata_smoke

/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_smoke.pro -o build/color_comparison.Makefile
make -C build -f color_comparison.Makefile -j8
./build/smoke/color_comparison/bin/color_comparison_smoke

QT_QPA_PLATFORM=offscreen /home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_dialog_smoke.pro -o build/color_comparison_dialog.Makefile
make -C build -f color_comparison_dialog.Makefile -j8
QT_QPA_PLATFORM=offscreen ./build/smoke/color_comparison_dialog/bin/color_comparison_dialog_smoke
```

Expected: 四个 smoke 均为 0；算法 smoke 明确 licensed 未执行。

- [ ] **Step 3: 运行显式 licensed smoke**

```bash
RUN_HALCON_LICENSED_SMOKE=1 ./build/smoke/color_comparison/bin/color_comparison_smoke
```

有效 license 下输出 `V2 HALCON checks passed`。当前环境若仍为 `#2036`，非零退出并记录阻塞；不能改成跳过成功。

- [ ] **Step 4: 影子构建和静态检查**

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro -o build/Makefile
make -C build -j8
git diff --check
git status --short
```

Expected: 构建成功，diff check 无输出；status 只含计划内文件和进入任务前已有用户改动，无构建产物。

- [ ] **Step 5: 手工验证 UI 闭环**

逐项记录：

```text
工具库入口仍可见
新建颜色比较
模板 sync/custom
模板矩形与模板 mask 互斥编辑
检测全图/矩形/圆形与检测 mask
显式重新取样生成 ready V2 model
保存、关闭、重新打开后 model hash 不变
基础/全部切换不改变配置
单次、连续、停止、退出测试不卡 UI
灰度来源显示无效 NG
色谱禁用并标注待实现
位置修正禁用并标注接口预留
当前图不显示模板 ROI overlay
错误同时显示 status 和 message
```

- [ ] **Step 6: 更新记录并提交**

在实现记录顶部 V2 区域追加日期、改动文件、最终字段、实际 HALCON 操作、无 license 输出、licensed 输出或 `#2036`、手工 UI 结果、色谱/位置剩余项。不能把无 license smoke 通过写成数值算法通过。

```bash
git add qt_ui_test.pro \
        docs/FID/ColorComparison/color_comparison_function_implementation.md \
        docs/FID/ColorComparison/颜色比较V2设计说明.md
git commit -m "docs: record color comparison v2 verification"
```

---

## Deferred Independent Work

以下内容不并入本计划：

- 修复 `ColorRecognitionHalconRunner.cpp` 中颜色识别自己的 H/S 轴解释与历史模型迁移。
- 实现位置修正坐标变换、旋转检测 ROI 和来源订阅。
- 研究并实现真正的色谱特征。
- 根据现场数据固化光照补偿 scale、截断率和灵敏度偏移范围。

这些工作分别需要独立设计、测试与兼容策略，不能以颜色比较 V2 完成为名静默扩展。
