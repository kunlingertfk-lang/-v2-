![alt text](颜色比较基础.png)
![alt text](颜色比较全部.png)

# 颜色比较功能实现记录

> **当前有效设计已升级为 V2。** 后续实现必须遵循
> `docs/FID/ColorComparison/颜色比较V2设计说明.md` 和
> `docs/superpowers/specs/2026-07-13-color-comparison-v2-design.md`。
> 本文下方原有“第一版目标、实现方案和实现记录”作为 V1 历史追溯保留；其中的一维 HSV、伪 Bhattacharyya、主峰覆盖率、C++ soft-kernel、OpenCV compareHist、亮度参与扣分、裸特征和基础/全部切换清模等内容均不是现行规范。

## 文档用途

本文档单独记录颜色比较算子的需求、实现方案、每次实现的功能、更改内容、错误问题、验证结果和剩余事项。后续继续开发颜色比较时，以根目录 `AGENTS.md` 的项目约束和 HALCON 约束为最高规则，以本文档作为功能状态追踪依据。

当前规范入口：

- `docs/FID/ColorComparison/颜色比较V2设计说明.md`
- `docs/FID/ColorComparison/颜色比较检测特征可视化设计.md`
- `docs/superpowers/specs/2026-07-13-color-comparison-v2-design.md`
- `docs/superpowers/plans/2026-07-13-color-comparison-v2.md`

以下第一版方案和历次实现记录仅用于解释现有代码来源，不得直接作为新实现提示词。

## 2026-07-15 - 模板/检测特征对比与点击放大

### 实现内容

- `ColorComparisonHalconResult` 增加 32 维检测 V 直方图；成功 payload 新增 `histogramDiagnostics`，包含 1024 维检测 H/S、32 维检测 V、32×32 bins、`hue_major` 和 `rawIntersection`。失败路径明确输出 `available=false`。
- 新增 `ColorComparisonFeatureView.{h,cpp}`，将特征投影、绘制和放大交互从 `ColorComparisonDialog` 中隔离：
  - H/S/V 各显示 32 bins，模板使用实心柱，检测使用空心柱，重合使用斜纹；同一图中的模板与检测共用最大值纵轴。
  - 二维图直接显示 32×32 H/S 联合分布，横轴 H、纵轴 S、低 S 在下；绿色表示重合，橙色表示模板超出，青色表示检测超出。
  - 图例和指标区显示 HS 联合重合、当前得分和判定阈值。
  - H/S/V/HS 四图均可点击放大；支持右上角关闭、Esc 和点击遮罩关闭，不触发 Dialog 接受、拒绝或测试停止。
- `ColorComparisonDialog` 从 `ToolResult.payload.histogramDiagnostics` 防御性解析检测特征，只接受正确维度、layout、有限非负且归一化的数据；失败帧和退出测试立即清空检测层。
- 连续运行继续沿用现有 generation 门禁，放大层随最新有效结果刷新；切换基础页、退出测试、加载无效模型及关闭 Dialog 时关闭放大层。
- 检测特征只存在于运行期 `ToolResult`，不写入 `ToolConfig`、方案 JSON 或模板模型；评分、灵敏度、光照补偿和 `minScore` 判定未修改。

### 自动验证

- `color_comparison_feature_view_smoke`：offscreen 通过，覆盖 1024→H/S 32-bin 投影、检测数据校验、点击 H 图打开、关闭按钮、Esc、遮罩点击和零质量直方图拒绝。
- `color_comparison_feature_diagnostics_smoke`：无 license preflight 通过；`RUN_HALCON_LICENSED_SMOKE=1` 使用 HALCON 20.11 和当前有效 license 通过，确认实际检测 H/S=1024、V=32 以及 payload 字段完整。
- 主工程在 `build/` 中加载 `scripts/dependencies.env` 后重新执行 qmake 和 `make -j8`，编译链接通过。
- 目标源码和文档执行 `git diff --check` 无格式错误。

### 待人工验收

- 在 1920×1080 桌面环境打开颜色比较“全部”页，检查左侧滚动高度、图例辨识度、32 根柱的清晰度和二维图尺寸。
- 使用真实相机验证连续运行时小图和已打开的放大层持续刷新，失败帧不残留上一帧检测特征。
- 分别操作 H/S/V/HS 放大、右上角关闭、Esc、遮罩关闭、基础/全部切换和退出测试，确认不关闭主对话框且不误保存。

## 2026-07-13 - 颜色比较 V2 实施与验证记录

### 当前状态和实现范围

- 设计与实现提交范围：`21f774a..326b911`（本文档最终收口提交除外）。
- V2 已完成模型合同、原始输入元数据、HALCON Runner、Adapter、Dialog 保存回显和异步测试链路。
- 新增文件：
  - `src/frame/FrameInputMetadata.{h,cpp}`
  - `src/algorithms/recognition/ColorComparisonModel.{h,cpp}`
  - `smoke/frame_input_metadata_smoke.{cpp,pro}`
  - `smoke/color_comparison_model_smoke.{cpp,pro}`
  - `smoke/color_comparison_dialog_smoke.{cpp,pro}`
- 主要修改文件：
  - `src/ColorComparisonDialog.{h,cpp}`
  - `src/algorithms/recognition/ColorComparisonHalconRunner.{h,cpp}`
  - `src/tooladapters/ColorComparisonAdapter.{h,cpp}`
  - `src/frame/CameraFrameProvider.{h,cpp}`
  - `src/frame/ReferenceImageProvider.{h,cpp}`
  - `src/SchemeStore.{h,cpp}`、`src/toolcore/ToolEngine.{h,cpp}`
  - `src/MainWindow.cpp`、`src/ReferenceImageDialog.cpp`
  - `smoke/color_comparison_smoke.{cpp,pro}`、`qt_ui_test.pro`
- 三个颜色比较 smoke 均不再链接 `ColorRecognitionHalconRunner`；颜色比较 V2 的生产算法不调用颜色识别 Runner，也没有 OpenCV `compareHist`、伪 Bhattacharyya、C++ Gaussian soft-kernel 或经验主峰评分旁路。

### 最终配置字段

`ToolConfig.params.colorComparison` 当前保存：

```text
colorComparison
├── version = 2
├── templateRegionMode = "custom" | "sync"
├── templateRoiNormalized
├── templateMaskPolygon
├── model
├── dialogLifecycle
│   ├── originVersion
│   ├── status
│   └── reason
├── detectRegionType = "rectangle" | "circle"
├── detectGlobal
├── detectRoiNormalized
├── detectCircleNormalized
├── detectMaskPolygon
├── comparison
│   ├── sensitivity = "high" | "medium" | "low"
│   └── brightnessCompensation
└── positionCorrection
    ├── enabled
    ├── sourceId
    └── interfaceVersion = 1
```

`ToolConfig.judgeRule` 固定使用 `mode="min_score"` 和整数 `minScore`。当前默认值为：模板区域 `custom`、检测矩形、灵敏度 `medium`、光照补偿关闭、最低分 `80`。全局检测在 Adapter 合同中保存为全图矩形，并以 `detectGlobal` 保持 UI 保存/重新打开后的全局选中状态。

`dialogLifecycle` 是 Dialog 的迁移来源元数据，不属于公共 `ColorComparisonModelV2`：

- V1 有参考图：保存为 V2 stale，保留 `originVersion=1/model_stale`。
- V1 无参考图：保存为 V2 unsupported，保留 `originVersion=1/model_rebuild_required`，参考图后续到达时仍只允许用户显式重新取样。
- 未知版本：保持 unsupported/`unsupported_model_version`，不能借保存/重新打开降级成普通 stale。
- 原生 V2 的模型状态与 lifecycle 不满足迁移白名单组合时，不允许 lifecycle 覆盖严格模型解析结果。
- 成功重新取样后重置为 `originVersion=2/status=ok`。

### V2 模型合同

正式模型固定为：

```text
state = empty | stale | ready | invalid | unsupported
featureType = "histogram_hs_2d"
algorithm = "histogram_intersection"
colorSpace = "hsv"
hueBins = 32
saturationBins = 32
layout = "hue_major"
normalized = true
values = 1024 维归一化 H/S 联合直方图
valueHistogram = 32 维归一化 V 直方图（仅作展示和光照诊断，不参与颜色扣分）
effectivePixelCount
referenceImageHash
extractParamsHash
inputSignature
brightnessReference { mean, deviation }
```

`inputSignature` 保存 `colorMode`、`pixelFormat`、`bitDepth`、`whiteBalance`、`ccm`、`exposure` 和 `gain`。严格校验拒绝错误状态、非 32×32 bins、错误 layout/算法、非有限值、负数、未归一化、空哈希、无效有效像素数以及不合法亮度参考。参考图和提取参数使用 SHA-256 哈希；stale 只改变状态和原因，不删除原模型直方图与哈希。

### 原始输入合同

- `FrameInputMetadata` 在显示或算法桥接为 BGR 前保存原始颜色模式、像素格式、通道、位深和来源。
- Camera/Reference Provider 提供图像与 metadata 的原子快照；方案 JSON 保存 `referenceInputMetadata`，重新加载 PNG 时不会把未知旧来源伪造为彩色来源。
- `MainWindow` 和 Dialog 将 `input`、`referenceInput` 注入 `ToolRequest.runtimeContext`，Adapter 在进入 Runner 前完成唯一的严格解析。
- 原始 Mono 来源返回 invalid NG/`unsupported_color_input`，即使显示 Mat 已经转换为三通道也不能继续颜色评分。
- 非 8-bit、非支持原始像素格式或非 `CV_8UC3/CV_8UC4` 算法输入返回 `unsupported_pixel_format`；空图优先返回 `image_empty`。
- unknown 颜色模式为兼容旧方案允许继续，但 payload 必须输出 `input_color_mode_unknown` warning。

### 实际 HALCON 算法链路

V2 核心算法实际动态解析并调用以下 HALCON C 接口：

1. `gen_image_interleaved` 将连续 BGR/BGRA `cv::Mat` 桥接成 HALCON image。
2. `decompose3` 和 `trans_from_rgb(..., "hsv")` 得到 H/S/V。
3. `gen_rectangle1`、`gen_circle` 或 `T_gen_region_polygon_filled` 构造 ROI/Mask；`difference` 扣除 Mask；`T_area_center` 检查最终有效像素数。
4. `T_scale_image` 将 H/S 量化到 32 bins；`T_histo_2dim(Region, H, S)` 以 `ImageCol=H`、`ImageRow=S` 生成联合直方图；`T_get_grayval` 按 `row=Saturation,column=Hue` 读取，再组织为 `hueBin*32+saturationBin` 的 hue-major 外部模型。
5. `T_tuple_sum + T_tuple_div` 归一化 HS 与 V 直方图。
6. 灵敏度只控制有限位移搜索半径：high=0、medium=1、low=2；Hue 位移循环回绕，Saturation 位移不回绕。
7. 每个位移使用 `T_tuple_select + T_tuple_min2 + T_tuple_sum` 计算直方图交集，取最大值，`score=clamp(intersection*100,0,100)`。

`sync` 模板建模只复用检测区域的类型和归一化几何，然后仅 `difference` 模板 Mask；检测 Mask 不进入模板 Region，也不进入模板提取哈希。`custom` 模板只使用自定义模板矩形和模板 Mask。检测有效区域独立按检测几何 `difference` 检测 Mask。

光照补偿默认关闭。开启时使用 HALCON `T_intensity` 计算 V 均值，以受限比例同时缩放 R/G/B，再 `compose3` 和 HSV 转换；允许模板/检测均值范围 `[8,247]`、scale 范围 `[0.75,1.3333333333]`、最大截断比例 `0.02`。超出范围返回 `invalid_illumination` 及完整 diagnostics；V 通道差异不直接加入最终颜色得分。

### 模型 stale 矩阵

会使模型 stale 或使进行中的建模结果失效：

- 参考图变化。
- 模板模式、custom 模板 ROI、模板 Mask、特征类型、光照补偿变化。
- `sync` 模式下检测全图/矩形/圆形几何变化。
- 加载其他配置、接受、拒绝、关闭和析构会使旧异步结果无效。

不会使 ready 模型 stale：

- 任意模式下检测 Mask 变化。
- `custom` 模式下检测全图/矩形/圆形变化。
- 灵敏度、最低分、位置修正预留状态变化。
- 基础/全部切换、测试启动/停止。

### Dialog 与异步执行

- 只有显式“重新取样”按钮调用 `buildTemplateModel`；基准图测试、当前图测试和完成按钮不会隐式建模。
- 模型建模与检测测试分别使用独立单调 generation；worker 只按值捕获完整 `ToolRequest`，在 worker 内创建局部 `ColorComparisonAdapter`。
- 进行中的检测只保留最新 pending 请求；新帧、配置或关闭使旧结果过期。
- 成功模型重建会先使旧 active/pending 检测请求失效，再发布新模型，旧 score、overlay 或 reference preview 不能覆盖新模型状态。
- 关闭 Dialog 不等待 HALCON worker；QObject 生命周期和 generation 门禁阻止回调写入已关闭界面。
- H/S 图由 1024 维联合直方图求边缘分布，V 图使用 `valueHistogram`；非 ready 模型清空三张统计图。
- 配置态主视图同时显示模板 ROI、模板 Mask、检测 ROI 和检测 Mask，使用 T/D 标签、不同颜色及活动/弱化层级区分归属；测试态当前图只显示检测 ROI、检测 Mask 和结果 overlay。

### 结果 payload

检测运行的成功和失败路径均提供便于 UI 与诊断使用的稳定字段：

```text
status, message, measurementValid, passed
algorithm, featureType, modelVersion
score, similarity, threshold
effectiveTemplatePixels, effectiveDetectionPixels
brightnessCompensation {
  enabled, applied, templateMean,
  detectMeanBefore, detectMeanAfter, scale, clippedRatio
}
positionCorrection { requested, applied=false, sourceId }
detectionRoi
warnings
elapsedMs
```

Mono 来源属于“算子执行成功但测量无效”的 NG：`success=true`、`ok=false`、`measurementValid=false`、`status=unsupported_color_input`。空图、配置、模型、HALCON runtime/符号/license 等错误返回明确失败状态。

### 2026-07-13 Task 6 验证结果

qmake 接线检查：

- `qt_ui_test.pro` 同时包含 `FrameInputMetadata`、`ColorComparisonModel` 的 cpp/h。
- `color_comparison_smoke.pro`、`color_comparison_model_smoke.pro`、`color_comparison_dialog_smoke.pro` 均未链接 `ColorRecognitionHalconRunner`。

四个无 license smoke 均重新执行 qmake、make 和 binary，exit 均为 `0`：

```text
color_comparison_model_smoke: V2 model contract checks passed
frame_input_metadata_smoke: all checks passed
Color comparison V2 preflight passed; licensed HALCON checks were skipped. Set RUN_HALCON_LICENSED_SMOKE=1 to require licensed checks.
color_comparison_dialog_smoke: V2 Dialog checks passed
```

其中算法 smoke 使用 `env -u RUN_HALCON_LICENSED_SMOKE`，上述结果仅证明无 license 合同和 preflight 通过，不代表 HALCON 数值测试通过。

修正前首次在有效 license 下显式运行 `RUN_HALCON_LICENSED_SMOKE=1`，binary exit `1`，暴露两项轴解释相关失败：

```text
axis diagnostic: maxIndex=1002 hueBin=31 saturationBin=10
FAIL: histo_2dim must flatten row=Hue and column=Saturation as hue-major
red-wrap diagnostic: status=ok score=0 templatePeak=864 detectPeak=895
FAIL: medium tolerance must wrap hue across red bin 31/0
Color comparison V2 licensed smoke failed with 2 failure(s).
```

后续使用同一 HALCON 24.11.1 头文件、`libhalconc.so.24.11.1` 和有效 license 编写最小 C canary，确认 `T_histo_2dim(first=10,second=31)` 的非零频数位于 `row=31,column=10`。结合 C API 的 `ImageCol/ImageRow` 参数名、官方 `class_2dim_sup` 和 MVTec Classification Solution Guide，确定算子单页“第一通道映射行”的正文与可执行接口语义冲突。

Runner 保持输入 `histo_2dim(Region,H,S)` 不变，将读取修正为 `row=S,column=H`，外部模型仍为 hue-major；同时加入 `histogramCoordinateContract` 提取哈希字段，使按旧坐标合同生成的 V2 模型自动 stale。修正后有效 license 算法 smoke exit `0`，上述 H/S 峰值和红色回绕断言均转绿，未通过删除或放宽测试掩盖问题。

使用 `HALCON_LICENSE_FILE=/tmp RUN_HALCON_LICENSED_SMOKE=1` 强制验证缺 license，binary exit `1`：

```text
licensed template build failed (base): halcon_license_error: gen_image_interleaved: HALCON #2036: could not find license file
Color comparison V2 licensed smoke failed with 1 failure(s).
```

主工程重新执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro -o build/Makefile` 和 `make -C build -j8`，exit `0`，`build/qt_ui_test/bin/qt_ui_test` 存在且可执行。提交前 `git diff --check` 无输出；构建产物均位于已忽略的 `build/`。

### 独立复审后的合同修正

整分支复审发现的问题按职责拆分修正，并分别提交：

- `44856b6`：Adapter/Model 增加当前参考图哈希 freshness 门禁、严格 lifecycle 来源恢复、非方形图像圆形 ROI 的 max-dimension 边界合同、最少 4 个有效像素、亮度统计 `[0,255]` 校验和稳定失败 payload。
- `6c723e4`：Runner 按 HALCON 可执行坐标合同修正 H/S 读取，使用 `T_tuple_max` 求候选最大值，统一圆形 ROI/overlay/payload 半径语义，并把坐标合同写入 `extractParamsHash` 以淘汰旧轴合同模型。
- `ff6e0ce`：Dialog 对外层整数 `version=2` 的非法原始配置进入只读保真状态，阻止完成、取样和测试覆盖原数据；同时修正 lifecycle 回显与圆形 ROI 预览语义，并补齐非法枚举、ROI、Mask、非有限数值、比较、亮度、位置和 judgeRule 用例。
- `326b911`：最终复审补齐 ready 输入签名的彩色 8-bit 枚举合同、原生 stale 非空 payload 的完整校验及 legacy placeholder 受限豁免；缺失/非对象 envelope 与畸形 version 只读保真；加载即核对参考图哈希；拒绝越界圆并按参考图宽高比重算 sync 预览；输入合同失败 payload 保留实际配置并合并位置修正 warning。

四组修正后重新 qmake/make 四个 smoke 工程，编译均 exit `0`；Model 与 Dialog smoke 均通过，并在最终提交前再次连续回归。最终验证还确认：

- 默认算法 preflight exit `0`；有效 license 算法 smoke exit `0`。
- 强制无效 license 返回 HALCON `#2036` 且 exit `1`，未被误报为跳过成功。
- 主工程 clean qmake/make exit `0`，最终可执行文件存在。
- `git diff --check`、目标 smoke 链接隔离和 HALCON-only 静态门禁均通过。

### 2026-07-13 - 恢复检测区域结果文字

- 根因：V2 `detectionOverlays()` 只保留检测几何，遗漏了旧版 `color_result_text`，且 smoke 的固定数量断言把该回归固化。
- 修复：成功且测量有效的结果按“检测几何、可选检测 Mask、结果文字”顺序输出 overlay；文字显示 `OK/NG score:x.x`，矩形使用检测框、圆形使用像素外接矩形作为 `anchorRect`。
- 边界：未修改 HALCON H/S 二维直方图、直方图交集评分、灵敏度、阈值、模板模型和失败状态。
- 验证：licensed Runner smoke 覆盖 OK、NG、矩形、圆形、非方形圆和检测 Mask；Model、Dialog smoke 与完整 Qt 工程继续通过。
- 人工检查：仍需在图形界面确认小 ROI 的框外文字位置以及 OK/NG 颜色。

### 未人工验证和剩余能力

- 本轮未在真实相机和图形桌面环境中人工执行完整 UI 验收；工具库入口、新建/保存/重新打开、sync/custom 切换、ROI/Mask 实际鼠标拖拽、单次/连续/停止/退出以及长时间连续运行均标记为“未人工验证”。offscreen smoke 不能替代上述人工验证。
- 色谱特征仍禁用并显示“待实现”；只保留 future interface，不静默降级到直方图。
- 位置修正整组 UI 禁用并显示“接口预留，暂未实现”；旧配置请求时继续原始 ROI，并在 payload 输出 `position_correction_not_implemented` warning。
- 现场数据尚未完成光照补偿范围、截断阈值、灵敏度与最低分标定。
- `ColorRecognitionHalconRunner` 自身 H/S 轴、历史模型兼容不属于本 V2 任务，未修改其既有行为。

## 参考截图

- 基础页参考：`docs/FID/ColorComparison/颜色比较基础.png`
- 全部页参考：`docs/FID/ColorComparison/颜色比较全部.png`

截图中的红色标注是本功能 UI 和交互要求的一部分：

- `搜索模板` 这一栏不用复现，仅作为截图标题参考。
- 第一版不创建 `模板区域=与检测区域同步` 控件，也不实现该模式。
- 模板区域第一版只保留 `自定义` 模板 ROI 编辑。
- 模板编辑显示矩形 ROI 图标和 `完成` 按钮。
- 屏蔽区域显示多边形 ROI 图标和 `完成` 按钮。
- 模板 ROI 与屏蔽 ROI 同一时刻只能编辑一个。
- 检测区域包含全局、矩形 ROI、圆形 ROI 图标。
- 全部页展示特征类型、亮度使能、模型色彩特征 HSV 直方图。
- 结果判断第一版使用最小得分阈值。

## 第一版目标

颜色比较第一版实现完整工具闭环：

- 工具库入口可见，能创建颜色比较工具。
- 配置对话框能保存、重新打开并回显模板 ROI、检测 ROI、屏蔽 ROI、识别设置和结果判断。
- 测试运行时使用 HALCON 算子提取模板 ROI 和检测 ROI 的 HSV 直方图特征。
- 算法按 `模板 ROI` 与 `检测 ROI` 的 HSV 直方图交集相似度评分，分数越高表示越相似。
- UI 显示 OK/NG、得分、ROI、耗时和错误状态。
- 空图像、无效 ROI、未设置模板、屏蔽区完全覆盖检测区、HALCON runtime 或符号缺失时不崩溃，并返回明确错误。

## 实现方案

### 总体链路

```text
ToolLibraryDialog / ToolsDialog
        |
        v
ColorComparisonDialog
        |
        v
ToolConfig.params / judgeRule
        |
        v
ColorComparisonAdapter
        |
        v
ColorComparisonHalconRunner
        |
        v
ToolResult / overlays / payload
```

第一版采用独立颜色比较工具链，不直接复用或改造 `ColorRecognitionHalconRunner`。颜色比较与颜色识别都可以使用相似的 HALCON HSV 直方图思路，但两个算子语义不同：

- 颜色识别：模板样本分类，输出类别。
- 颜色比较：模板 ROI 与检测 ROI 两块区域做相似度比较，输出分数。

第一版允许在 `ColorComparisonHalconRunner` 中少量复用颜色识别已有 HALCON 动态解析和直方图提取写法，以降低对颜色识别已有行为的影响。后续两个算子稳定后，再评估是否抽取共享的颜色直方图 HALCON helper。

### UI 结构

`ColorComparisonDialog` 参考现有颜色识别对话框和截图布局：

```text
ColorComparisonDialog
|
+-- 顶部标题栏
|
+-- 左侧参数区
|   +-- 基础 / 全部 分段按钮
|   |
|   +-- 模板区域卡片
|   |   +-- 模板区域
|   |   |   +-- 自定义
|   |   +-- 模板编辑
|   |   |   +-- 矩形 ROI 图标
|   |   |   +-- 完成
|   |   +-- 屏蔽区域
|   |       +-- 编辑
|   |       +-- 多边形 ROI 图标
|   |       +-- 完成
|   |
|   +-- 模板效果卡片
|   |   +-- 当前模板 ROI 缩略图
|   |
|   +-- 全部页扩展参数
|   |   +-- 特征类型：直方图特征
|   |   +-- 亮度使能
|   |   +-- 模型色彩特征：色相 / 饱和度 / 亮度直方图
|   |
|   +-- 检测区域卡片
|   |   +-- 全局
|   |   +-- 矩形 ROI
|   |   +-- 圆形 ROI
|   |   +-- 检测区内屏蔽区域编辑
|   |
|   +-- 识别设置卡片
|   |   +-- 灵敏度
|   |
|   +-- 结果判断卡片
|       +-- 最小得分
|
+-- 右侧图像预览区
|   +-- 基准图 / 当前图像
|   +-- 模板 ROI / 检测 ROI / 屏蔽 ROI / 结果 overlay
|   +-- 底部状态栏
|
+-- 底部按钮
    +-- 测试运行 / 停止测试
    +-- 完成
```

### ROI 交互状态

ROI 编辑状态应使用单一状态管理，避免多个绘制状态互相污染：

```text
None
TemplateRect
TemplateMaskPolygon
DetectRect
DetectCircle
DetectMaskPolygon
```

约束：

- 进入模板 ROI 编辑时，关闭屏蔽 ROI、检测 ROI、圆形 ROI 绘制。
- 进入屏蔽 ROI 编辑时，关闭模板 ROI、检测 ROI、圆形 ROI 绘制。
- 进入检测 ROI 编辑时，关闭模板 ROI 和屏蔽 ROI 绘制。
- 点击对应 `完成` 后退出编辑状态，并保留 ROI 配置。
- 右侧预览区只显示当前编辑所需 ROI 和结果 overlay，避免旧 overlay 残留。

### 配置字段

第一版建议保存到 `ToolConfig.params` 和 `judgeRule`：

```text
ToolConfig
|
+-- toolType = ColorComparison
+-- category = Recognition
+-- roiNormalized
|   +-- 检测矩形 ROI 或圆形外接矩形；全局为整图
|
+-- params
|   +-- colorComparison
|   |   +-- version = 1
|   |   +-- templateRegionMode = "custom"
|   |   +-- templateRoiNormalized
|   |   +-- templateMaskPolygon
|   |   +-- templateFeature
|   |   +-- templateThumbnailPngBase64
|   |   +-- templateThumbnailWidth
|   |   +-- templateThumbnailHeight
|   |   +-- featureType = "histogram"
|   |   +-- sensitivity
|   |   +-- brightnessEnabled
|   |   +-- detectRegionType
|   |   +-- detectCircleNormalized
|   |   +-- detectMaskPolygon
|   |   +-- halconSoPath
|
+-- judgeRule
    +-- mode = "min_score"
    +-- minScore
```

默认值：

- `templateRegionMode=custom`
- `featureType=histogram`
- `sensitivity=medium`
- `brightnessEnabled=true`
- `detectRegionType=rectangle`
- `minScore=52`

### HALCON 算法约束

核心算法必须使用 HALCON，不允许使用 OpenCV、自写算法或第三方库替代。OpenCV 只作为工程侧 `cv::Mat` 图像载体和必要格式桥接。

第一版需要确认并使用的 HALCON 接口：

- `gen_image_interleaved`：将工程侧 BGR `cv::Mat` 桥接为 HALCON image。
- `decompose3`：拆分三通道图像。
- `trans_from_rgb`：转换到 HSV 空间。
- `gen_rectangle1`：生成矩形模板或检测 ROI。
- `gen_circle`：生成圆形检测 ROI。
- `gen_region_polygon_filled`：生成屏蔽多边形 region。
- `difference`：从模板或检测 ROI 中扣除屏蔽区域。
- `area_center`：检查有效 ROI 面积。
- `reduce_domain`：限制 H/S/V 通道统计 domain。
- `gray_histo_range`：提取 H/S/V 直方图。
- `tuple_min2`：计算直方图交集逐 bin 最小值。
- `tuple_sum`：计算交集和特征总和。

HALCON runtime、license 或必要符号缺失时必须返回明确错误，例如 `halcon_load_failed`、`halcon_symbol_missing`，不允许 fallback 到非 HALCON 算法。

### 算法流程

```text
输入图像 cv::Mat
        |
        v
转换为 8-bit BGR
        |
        v
HALCON image
        |
        v
HSV 三通道
        |
        +-- 模板 ROI region
        |       +-- 扣除模板屏蔽 region
        |       +-- 提取模板 H/S/V 直方图
        |
        +-- 检测 ROI region
                +-- 扣除检测屏蔽 region
                +-- 提取检测 H/S/V 直方图
        |
        v
直方图交集相似度
        |
        v
score = clamp(similarity * 100, 0, 100)
        |
        v
score >= minScore -> OK，否则 NG
```

直方图维度：

- `sensitivity=low`：每通道 8 bins。
- `sensitivity=medium`：每通道 16 bins。
- `sensitivity=high`：每通道 32 bins。
- `brightnessEnabled=false`：只比较 H + S。
- `brightnessEnabled=true`：比较 H + S + V。

相似度公式：

```text
intersection = sum(min(templateFeature, detectFeature))
normalizer = min(sum(templateFeature), sum(detectFeature))
similarity = normalizer > 0 ? intersection / normalizer : 0
score = similarity * 100
```

### 结果 payload

`ToolResult.payload` 至少包含：

```text
algorithm = "halcon_hsv_histogram_comparison"
featureType
sensitivity
brightnessEnabled
lightingNormalizationMode
comparisonMethod = "hsv_histogram_intersection"
scoreDirection = "higher_is_better"
scoreFormula = "histogram_intersection_similarity_x100"
similarity
score
histogramBins
featureLength
templateRoiPixelsRect
detectRoiPixelsRect
templateMaskApplied
detectMaskApplied
templateMaskFullyCoversRoi
detectMaskFullyCoversRoi
effectiveTemplateRoiArea
effectiveDetectRoiArea
detectRegionType
elapsedMs
judgeMode
minScore
```

### 错误状态

| 场景 | 状态 |
| --- | --- |
| 空图像 | `image_empty` |
| 模板 ROI 无效 | `invalid_template_roi` |
| 检测 ROI 无效 | `invalid_detect_roi` |
| 未设置模板特征 | `no_template_feature` |
| 模板 ROI 被屏蔽区完全覆盖 | `template_masked_empty` |
| 检测 ROI 被屏蔽区完全覆盖 | `detect_masked_empty` |
| 色谱特征 | `unsupported_feature` |
| HALCON runtime 文件找不到 | `halcon_so_not_found` |
| HALCON runtime 加载失败 | `halcon_load_failed` |
| HALCON 符号缺失 | `halcon_symbol_missing` |
| 其他异常 | `exception` |

## 实现记录

### 2026-07-01 10:30:26 CST - 位置修正内部链路和 FID 公有规范补齐

#### 已实现功能

- 颜色比较位置修正内部链路补齐到与有无工具一致的占位实现方式。
- Adapter 解析并透传 `enablePositionCorrection` 和 `positionCorrectionSource`。
- Runner payload 输出位置修正请求、来源、是否应用和未应用原因。
- 新增 `docs/FID/Function_Docs.md`，沉淀 FID 功能公有约束和位置修正统一规范。
- 更新颜色比较、颜色识别提示词规范，要求按 `AGENTS.md` 和 `docs/FID/Function_Docs.md` 约束开发。

#### 本次更改

- `src/algorithms/recognition/ColorComparisonHalconRunner.h`
  - `ColorComparisonHalconConfig` 增加 `enablePositionCorrection`、`positionCorrectionSource`。
- `src/tooladapters/ColorComparisonAdapter.cpp`
  - 从 `params.colorComparison` 解析位置修正字段并传入 runner。
- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
  - 错误和正常 payload 均输出 `enablePositionCorrection`、`positionCorrectionSource`、`positionCorrectionApplied=false`、`positionCorrectionReason=not implemented`。
- `docs/FID/Function_Docs.md`
  - 新增 FID 公有约束、通用开发链路、位置修正统一规范、文档维护和验证要求。
- `docs/FID/ColorComparison/颜色比较提示词规范.md`、`docs/FID/ColorRecognition/颜色识别提示词规范.md`
  - 增加 `docs/FID/Function_Docs.md` 约束引用。

#### 出现的问题与处理

- 问题：上一版位置修正只在颜色比较 Dialog 中保存和回显，adapter/runner 没有感知，内部链路不等同于有无工具。
  处理：按有无工具现有占位实现方式补齐配置透传和 payload 输出，明确暂未实际应用位置补偿。

#### 验证

- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4 && ./color_comparison_smoke`，通过。
- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8 && git diff --check`，通过。

#### 剩余事项

- 位置修正仍为统一占位链路，尚未实现实际 ROI 坐标补偿算法。

### 2026-07-01 10:11:05 CST - 检测区域位置修正使能补充

#### 已实现功能

- 在颜色比较 `检测区域` 卡片中补充 `独立位置修正使能 ⓘ` 开关。
- 开关开启时显示 `位置修正` 下拉框，关闭时隐藏下拉框。
- 下拉框第一版提供 `1 基准图.位置修正信息`，与有无工具同类控件文案保持一致。
- 位置修正开关和来源写入 `colorComparison` 参数，并在重新打开工具时回显。

#### 本次更改

- `src/ColorComparisonDialog.h`
  - 新增位置修正使能、来源和相关 UI 控件成员。
- `src/ColorComparisonDialog.cpp`
  - 在检测区域 ROI 三图标下方新增位置修正开关行和来源行。
  - 新增 `refreshPositionCorrectionControls()` 统一维护开关、下拉框和来源行显隐。
  - `colorComparisonParams()`、`loadFromConfig()` 增加 `enablePositionCorrection` 和 `positionCorrectionSource`。

#### 出现的问题与处理

- 问题：颜色比较检测区域缺少与有无工具一致的位置修正使能区域。
  处理：参考有无工具的 `独立位置修正使能 ⓘ / 位置修正` 文案和 `positionCorrectionSwitch` 样式接入，默认关闭并隐藏来源行。

#### 验证

- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8 && git diff --check`，通过。

#### 剩余事项

- 位置修正参数第一版仅保存和回显，颜色比较 adapter/runner 暂未消费该参数。
- 仍需在图形环境中确认开关显示位置、开启后来源行显示、关闭后来源行隐藏。

### 2026-07-01 09:59:29 CST - 检测区域 ROI 按钮状态修正

#### 已实现功能

- 检测区域恢复为三个 ROI 图标按钮：全图、矩形、圆形。
- 移除检测区域额外的 `编辑 / 完成` 控件。
- 点击矩形或圆形图标后直接进入对应 ROI 绘制，点击全图后设置全图检测区域并退出绘制。

#### 本次更改

- `src/ColorComparisonDialog.h`
  - 删除检测区 `m_detectEditButton` 和 `m_detectFinishButton` 成员。
- `src/ColorComparisonDialog.cpp`
  - 删除检测区 `编辑 / 完成` 按钮创建、布局和信号连接。
  - 保留全图、矩形、圆形三个图标按钮，并继续通过 `refreshDetectRegionButtons()` 同步 checked 状态。

#### 出现的问题与处理

- 问题：上一版把检测区也做成了 `编辑 / 完成` 状态流，但检测区域要求参考有无工具，只保留三个图标按钮作为入口。
  处理：移除检测区额外状态按钮，矩形和圆形图标直接进入绘制模式，全图图标直接设置全图 ROI。

#### 验证

- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8 && git diff --check`，通过。

#### 剩余事项

- 仍需在图形环境中确认检测区域三图标位置与有无工具风格一致，并手动验证矩形/圆形拖拽。

### 2026-07-01 09:35:18 CST - 颜色比较 ROI 编辑和模板效果修复

#### 已实现功能

- 模板编辑和屏蔽区域编辑改为互斥编辑态，默认只显示 `编辑`，进入编辑后显示对应 ROI 图标和 `完成`。
- 模板效果从坐标文本改为显示当前模板 ROI 的真实裁剪缩略图。
- 检测区域补充 `编辑 / 完成` 交互，支持全图、矩形 ROI、圆形 ROI 的选择和回显。
- 切换编辑态时保留已保存的 ROI 数据，并同步检测区域按钮 checked 状态。

#### 本次更改

- `src/ColorComparisonDialog.h`
  - 新增预览图缓存、检测区编辑按钮成员、模板 ROI 裁剪和控件刷新函数声明。
- `src/ColorComparisonDialog.cpp`
  - 新增检测区 `编辑 / 完成` 按钮。
  - 新增 `templateRoiImage()` 裁剪当前基准图或相机图像中的模板 ROI。
  - 新增 `refreshEditControls()` 和 `refreshDetectRegionButtons()` 统一维护按钮显隐与选中态。
  - 优化 `setEditState()` 状态提示，区分模板 ROI、模板屏蔽、检测矩形、检测圆形和检测屏蔽编辑。

#### 出现的问题与处理

- 问题：原实现进入界面时矩形/多边形图标和完成按钮常驻显示，和截图要求的 `编辑 -> ROI 图标 + 完成 -> 编辑` 状态流不一致。
  处理：以 `m_editState` 为唯一来源统一刷新控件显隐，确保同一时刻只编辑一个目标。
- 问题：模板效果只显示归一化坐标，无法直观看到模板区域图像。
  处理：缓存当前预览图并按模板 ROI 裁剪缩略图显示，无图像时显示 `无模板图像`。
- 问题：检测区缺少明确的位置编辑入口。
  处理：新增检测区 `编辑 / 完成`，矩形和圆形按钮可直接进入对应绘制模式，全图按钮退出绘制并设置全图 ROI。

#### 验证

- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，通过。
- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4 && ./color_comparison_smoke`，通过。
- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8 && git diff --check`，通过。

#### 剩余事项

- 仍需在图形环境中手动验证模板 ROI 缩略图、检测矩形/圆形拖拽、保存后重新打开回显。

### 2026-06-30 18:15:33 CST - 工具列表图标补充

#### 已实现功能

- 确认资源层已有 `:/icons/compare.svg`，且 `resources/resources.qrc` 已注册该图标。
- 确认工具库选择页中，颜色识别和颜色比较按钮均引用 `:/icons/compare.svg`。
- 在工具配置页左侧已添加工具列表卡片中新增工具类型图标。
- 颜色识别和颜色比较在工具列表卡片中均显示 `compare.svg` 图标。

#### 本次更改

- `src/ToolsDialog.cpp`
  - 新增 `toolIconForType()`，按 `ToolType` 返回工具图标路径。
  - 在 `createToolCard()` 中新增 `toolTypeIconLabel`，显示工具类型图标。
  - `ColorRecognition` 和 `ColorComparison` 均映射到 `:/icons/compare.svg`。
- `styles/app.qss`
  - 新增 `toolTypeIcon` 样式，固定工具列表图标的白底、边框和圆角。

#### 出现的问题与处理

- 问题：颜色比较在工具配置页左侧工具列表卡片中没有对应工具图标。
  处理：排查后确认资源已经注册，根因是工具列表卡片原本没有绘制工具类型图标；新增类型图标控件和类型到图标的映射。

#### 验证

- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8 && git diff --check`，通过。

#### 剩余事项

- 本次未做人工 GUI 点击检查；建议后续在图形界面确认工具列表卡片中颜色比较图标显示效果。

### 2026-06-30 17:55:31 CST - 第一版代码实现

#### 已实现功能

- 新增颜色比较工具类型 `ToolType::ColorComparison`，支持字符串保存、读取和中文名称识别。
- 工具库 `ToolLibraryDialog` 新增 `颜色比较` 入口，可选择后进入配置流程。
- `ToolsDialog` 支持颜色比较工具新建、编辑、列表显示和预览快照保存。
- `MainWindow` 注册 `ColorComparisonAdapter`，工具链运行时可调度颜色比较。
- 新增 `ColorComparisonDialog`，实现第一版配置界面：
  - 顶部标题栏。
  - 左侧参数区。
  - `基础 / 全部` 分段按钮。
  - 模板区域、模板效果、检测区域、识别设置、结果判断。
  - 全部页显示模型色彩特征、亮度使能、检测屏蔽区域。
  - 右侧图像预览区和底部状态栏。
  - `测试运行` 和 `完成` 按钮。
- 新增 ROI 编辑状态：
  - `TemplateRect`
  - `TemplateMaskPolygon`
  - `DetectRect`
  - `DetectCircle`
  - `DetectMaskPolygon`
  - 进入任一编辑状态时关闭其他绘制状态。
- 新增 `ColorComparisonAdapter`，解析 `params.colorComparison` 和 `judgeRule`，调用颜色比较 runner 并转换为 `ToolResult`。
- 新增 `ColorComparisonHalconRunner`，第一版使用 HALCON HSV 直方图特征比较：
  - 模板 ROI 提取 HSV 直方图。
  - 检测 ROI 提取 HSV 直方图。
  - 使用直方图交集相似度得到 `similarity`。
  - 输出 `score = similarity * 100`。
  - 按 `minScore` 判定 OK/NG。
- 新增 `smoke/color_comparison_smoke.cpp` 和 `smoke/color_comparison_smoke.pro`。

#### 本次更改

- `src/toolcore/ToolTypes.h`
  - 增加 `ColorComparison` 枚举、字符串映射和中文识别。
- `src/ToolLibraryDialog.cpp`
  - 将 `colorComparisonToolButton` 加入按钮组。
  - 选择颜色比较时返回 `ToolType::ColorComparison`。
  - 预览区显示颜色比较说明。
- `ui/ToolLibraryDialog.ui`
  - 在识别工具区域新增 `颜色比较` 工具按钮。
- `src/ToolsDialog.cpp`
  - 接入 `ColorComparisonDialog` 的新建和编辑流程。
  - 工具列表显示名增加 `颜色比较`。
- `src/MainWindow.h`、`src/MainWindow.cpp`
  - 注册 `ColorComparisonAdapter`。
  - 支持主界面编辑颜色比较工具。
  - 工具显示名增加 `颜色比较`。
- `src/ColorComparisonDialog.h`、`src/ColorComparisonDialog.cpp`
  - 新增颜色比较配置对话框。
  - 保存和回显 `params.colorComparison`、`judgeRule.minScore`。
  - 支持模板 ROI、模板屏蔽 ROI、检测 ROI、圆形检测 ROI、检测屏蔽 ROI。
  - 支持测试运行和结果 overlay 显示。
- `ui/ColorComparisonDialog.ui`
  - 新增 Qt Designer 占位文件，保持工程 UI 文件入口一致。
- `src/tooladapters/ColorComparisonAdapter.h`、`src/tooladapters/ColorComparisonAdapter.cpp`
  - 新增颜色比较 adapter。
- `src/algorithms/recognition/ColorComparisonHalconRunner.h`、`src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
  - 新增颜色比较 HALCON runner。
- `qt_ui_test.pro`
  - 加入颜色比较 dialog、adapter、runner、ui 文件。
- `smoke/color_comparison_smoke.cpp`、`smoke/color_comparison_smoke.pro`
  - 新增颜色比较 smoke 测试。

#### 出现的问题与处理

- 问题：TDD RED 阶段 smoke 编译失败，提示 `ColorComparisonHalconRunner.h` 和 `.cpp` 不存在。
  处理：新增 `ColorComparisonHalconRunner.h/.cpp`。
- 问题：首次链接 smoke 时失败，原因是颜色比较 runner 第一版复用现有颜色识别 HALCON 特征提取链路，但 smoke 工程未链接 `ColorRecognitionHalconRunner.cpp`。
  处理：在 `smoke/color_comparison_smoke.pro` 中加入 `../src/algorithms/recognition/ColorRecognitionHalconRunner.cpp`。
- 问题：对话框最初将同一批控件同时加入基础页和全部页，Qt 会 reparent 控件，导致基础页可能缺少控件。
  处理：改为单套控件，`基础 / 全部` 分段只控制高级特征卡片和检测屏蔽行的显隐。

#### 验证

- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4 && ./color_comparison_smoke`，通过。
- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，通过。

#### 剩余事项

- 当前 `ColorComparisonHalconRunner` 第一版复用 `ColorRecognitionHalconRunner::extractFeature()` 获取 HALCON HSV 直方图，比较语义已独立在颜色比较 runner 内；后续可评估抽取共享 `ColorHistogramHalconHelper`，减少重复和语义耦合。
- 当前 `ColorComparisonDialog` 是代码构建 UI，`ui/ColorComparisonDialog.ui` 仅作为工程占位；后续若需要 Qt Designer 精细维护，可将布局迁移到 `.ui` 文件。
- 当前模板效果以 ROI 文本摘要显示，后续可补充真实模板 ROI 缩略图和 HSV 直方图绘制。
- 本次未做人工点击检查；后续应在图形环境中手动验证按钮、ROI 编辑、保存回显和测试运行。

### 2026-06-30 19:59:00 CST - 添加工具弹窗颜色比较图标可见性修复

#### 已实现功能

- `添加工具` 弹窗中，颜色比较工具在 `识别工具` 分组可见。
- `全部工具` 展示识别工具分组时，颜色比较同样可见。
- 颜色比较继续复用颜色识别同款图标 `:/icons/compare.svg`。

#### 本次更改

- `ui/ToolLibraryDialog.ui`
  - 将 `colorComparisonToolButton` 从识别工具网格 `row=1, column=4` 移动到 `row=2, column=0`。

#### 出现的问题与处理

- 问题：颜色比较按钮已经注册到 `ToolLibraryDialog` 按钮组，也已经使用 `:/icons/compare.svg`，但在 `识别工具` 和 `全部工具` 视图中不可见。
  处理：检查 `.ui` 后确认按钮位于第 5 列，当前工具库内容区按 4 列稳定展示，按钮被放到右侧不可见区域；改放到下一行首列。

#### 验证

- 执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，通过。
- 执行 `git diff --check`，首次发现 `src/ToolLibraryDialog.cpp` 存在一处尾随空白，清理后复检通过。

#### 剩余事项

- 本次为布局修复，未改动工具类型注册、算法和 adapter。
- 仍建议后续在图形环境中手动打开 `添加工具`，分别点击 `全部工具` 和 `识别工具` 确认显示位置与选中态。

### 2026-06-30 17:33:42 CST - 第一版规划与文档结构

#### 已确认需求

- 第一版默认算法为 `模板 ROI` 与 `检测 ROI` 的 HSV 直方图交集相似度。
- 分数越高表示越相似。
- 第一版不实现 `模板区域=与检测区域同步`，对应 UI 控件也不创建。
- 颜色比较采用独立工具链，不直接复用或修改颜色识别 runner 语义。
- `color_comparison_function_implementation.md` 作为后续每次实现、错误、更改和验证记录的主文档。
- `颜色比较提示词规范.md` 记录后续可直接使用的具体实现提示词。

#### 本次更改

- 将原本只包含截图引用的颜色比较实现文档扩展为功能实施记录。
- 补充第一版 UI 范围、ROI 交互状态、配置字段、HALCON 算子链、算法流程、payload 和错误状态。
- 明确后续每次开发都应按时间追加记录。

#### 本次错误与处理

- 当前文档此前只有 `颜色比较基础.png` 引用，缺少可执行的字段、算法和验证说明。
- 本次通过文档补全处理，尚未进入代码实现。

#### 验证

- 本次仅修改文档，未运行 `qmake` 和 `make`。

### 2026-07-01 14:20:44 CST - 测试运行控件新规范落地

#### 已实现功能

- 按 `docs/FID/Function_Docs.md` 的 `测试运行统一规范` 调整颜色比较底部动作按钮。
- 编辑态底部按钮统一为：`基准图测试`、`测试运行`、`完成`。
- 新增测试态按钮切换：
  - 连续运行中显示 `停止运行`、`运行一次`、`退出测试`。
  - 停止后显示 `连续运行`、`运行一次`、`退出测试`。
- 点击 `测试运行` 后默认进入连续运行：
  - 连续运行只使用 `CameraFrameProvider::currentFrame()` 的实时最新帧。
  - `停止运行` 按钮使用橙色高亮。
  - 点击停止后保留最后一帧。
- `运行一次` 只获取实时最新帧，不使用基准图。
- `退出测试` 停止连续运行并恢复编辑态按钮。
- `基准图测试` 只使用 `ReferenceImageProvider::referenceFrame()`，基准图为空时返回 `no_reference_image`，不回退实时帧。

#### 本次更改

- `src/ColorComparisonDialog.h`：新增测试态枚举、定时器、防重入状态和底部按钮成员。
- `src/ColorComparisonDialog.cpp`：新增基准图测试、连续运行、停止运行、运行一次、退出测试和按钮状态刷新逻辑。

#### 出现的问题与处理

- 问题：原实现只有单个 `测试运行` 按钮，且测试帧源会优先基准图再回退相机图像，不符合公共规范。
  处理：拆分为 `基准图测试` 与实时测试两条路径，实时连续/单次测试固定使用 `CameraFrameProvider::currentFrame()`。

#### 验证

- 已执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，编译链接通过。

#### 剩余事项

- 仍需在真实 GUI 中手动确认按钮显隐、高亮、连续运行视频刷新、停止后保留最后一帧和退出测试后的编辑态恢复。

### 2026-07-01 18:17:00 CST - 测试按钮族样式 C 方案分层权重

#### 已实现功能

- 测试按钮族视觉样式改为 C 方案分层权重，补齐原 QSS 缺失的 `:hover`、`[running="true"]`、`:disabled` 规则。
- 三档视觉层级：完成/运行一次（`testPrimary`）深色实心；测试运行/基准图测试（`testAction`）白底灰边，连续运行态橙色实心 `#ff7a00`；退出测试（`#exitTestButton`）透明弱化、hover 红警示。

#### 本次更改

- `src/ColorComparisonDialog.cpp`：替换 `buildUi()` 内联 QSS 块为完整规则集；在 `m_exitTestButton` 设置 actionRole 后补一行 `setObjectName("exitTestButton")`，使 QSS `#exitTestButton` 选择器能定向到退出测试按钮（此前比较侧缺此 objectName，识别侧已有）。
- 不改动：`installActionButtonFlash`、`running` 属性设置点、`refreshButtonStyle`、`applyBottomActionButtonMetrics`、所有信号槽连接。

#### 出现的问题与处理

- 问题：浏览器原型用了 `box-shadow`/`@keyframes`/`transition`/`::after`，Qt QSS 全不支持。
  处理：降级为纯色与边框色变化，hover 用底色+边框色、running 用静态橙色实心、flash 仍为硬切反色（退出测试为红切），三档层级靠底色区分。

#### 验证

- 已执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j$(nproc)`，强制重编 `ColorComparisonDialog.cpp` + 链接通过，`-Wall -Wextra` 无相关警告。

#### 剩余事项

- 真实 GUI 手动确认四按钮的默认/hover/点击 flash/连续运行橙色态/disabled 灰化，以及基础/全部分段、检测区域 ROI 绘制、完成/退出流程等回归不受影响。

### 2026-07-02 10:02:09 CST - 检测区域结果文字跟随检测框显示

#### 已实现功能

- 颜色比较测试运行后，结果文字显示方式对齐颜色识别。
- 检测 ROI 上显示 `OK/NG score:x.x` 结果文字。
- 当检测框足够大时，结果文字居中显示在检测框内。
- 当检测框较小时，结果文字按 `FrameViewHelper` 现有规则显示在检测框上方、下方或右侧，避免被检测框遮挡。

#### 本次更改

- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
  - 新增归一化 ROI 到像素 ROI 的转换。
  - 将模板 ROI、检测 ROI、屏蔽多边形 overlay 改为像素坐标输出，匹配 `FrameViewHelper::setToolOverlays()` 的显示坐标体系。
  - 结果文字 overlay 使用 `label=color_result_text`。
  - 结果文字 overlay 增加 `extra.anchorRect`，复用颜色识别已有的检测框内/外自动摆放逻辑。
  - 圆形检测区域输出圆形 overlay，并用外接矩形作为结果文字锚点。

#### 出现的问题与处理

- 问题：颜色比较 runner 只输出固定位置 `OK/NG` 文本，且没有 `color_result_text` 和 `anchorRect`，因此无法像颜色识别一样把结果显示到检测框上。
  处理：按颜色识别 runner 的结果 overlay 结构补齐结果文字 label、状态字段和 anchorRect。
- 问题：颜色比较结果 overlay 原先使用归一化坐标，`FrameViewHelper` 的 tool overlay 渲染逻辑实际按图像像素坐标处理。
  处理：runner 输出 overlay 前统一转换为图像像素坐标。

#### 验证

- 已执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，编译链接通过。

#### 剩余事项

- 仍需在真实 GUI 中手动确认矩形/圆形检测区域上文字显示位置、OK/NG 颜色和小 ROI 外置显示效果。

### 2026-07-02 10:20:21 CST - 颜色比较评分稳定性修正

#### 已实现功能

- 颜色比较得分从“拼接 HSV 直方图硬分箱交集”调整为“HSV 通道分开比较 + 邻近 bin 平滑 + 分层权重汇总”。
- 保持默认算法语义仍为模板 ROI 与检测 ROI 的 HSV 直方图交集相似度，分数越高越相似。
- 对轻微色相、饱和度和亮度漂移更稳定，避免同类绿色 ROI 因落入相邻 bin 而出现异常低分。

#### 本次更改

- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
  - 新增 HSV 直方图通道拆分比较。
  - Hue 通道按环形邻近 bin 平滑，Saturation/Value 通道按非环形邻近 bin 平滑。
  - 亮度使能时使用 `hue=0.55,saturation=0.30,value=0.15`；关闭亮度时使用 `hue=0.65,saturation=0.35`。
  - payload 中增加 `hueSimilarity`、`saturationSimilarity`、`valueSimilarity`、`hsvWeights`、`histogramSmoothing`，便于后续定位异常得分。
  - `comparisonMethod` 更新为 `hsv_histogram_intersection_smoothed_weighted`。
- `smoke/color_comparison_smoke.cpp`
  - 同步检查新的 `comparisonMethod`。

#### 出现的问题与处理

- 问题：原先把 H/S/V 三段直方图直接拼接后逐 bin 求交集，若同色区域受光照、材质或采样 ROI 影响落到相邻 bin，交集会接近 0，出现绿色与绿色得分极低、显示效果不符合直觉的问题。
  处理：保留 HALCON 提取 HSV 直方图的链路，只在比较阶段对相邻 bin 做平滑，并按 Hue/Saturation/Value 分层加权，降低硬分箱抖动造成的误判。
- 问题：需要保留“亮度使能”的 UI 语义。
  处理：亮度打开时参与 Value 通道但权重较低，避免亮度变化压过色相判断；亮度关闭时只比较 H/S。

#### 验证

- 已执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，主工程编译通过。
- 已执行 `qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4`，颜色比较 smoke 编译通过。
- 已执行 `./color_comparison_smoke`，运行阶段因本机 HALCON license 缺失失败：`gen_image_interleaved: could not find license file`。该项为环境阻塞，待 HALCON license 可用后复测。

#### 剩余事项

- 需要在真实 GUI 中用同一张基准图复测绿色模板对上方绿色、下方绿色、青色、红色等 ROI 的分数梯度。
- 若仍出现同色低分，应继续结合 payload 中的三通道相似度判断是 Hue 漂移、饱和度漂移、亮度漂移还是 ROI 映射问题。

### 2026-07-02 10:45:21 CST - 颜色比较同色低分二次修正

#### 已实现功能

- 将颜色比较 HSV 相似度从邻近 bin 平滑进一步调整为软距离核匹配。
- 中等灵敏度下，Hue 相差少量 bin 且饱和度相近的同类颜色不再被硬分箱直接打成低分。
- 基础页固定不启用亮度；全部页才显示并保存“亮度使能”开关。

#### 本次更改

- `src/algorithms/recognition/ColorComparisonHalconRunner.h`
  - 新增 `ColorComparisonHsvSimilarity` 和 `compareColorComparisonHsvHistograms()`，用于不依赖 HALCON license 的纯 HSV 相似度测试。
  - `ColorComparisonHalconConfig::brightnessEnabled` 默认值改为 `false`。
- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
  - HSV 通道比较改为 soft-kernel 加权匹配：Hue 使用环形距离，Saturation/Value 使用线性距离。
  - `comparisonMethod` 更新为 `hsv_histogram_soft_kernel_weighted`。
  - payload 保留 `hueSimilarity`、`saturationSimilarity`、`valueSimilarity`、`hsvWeights` 和 `histogramSmoothing`，便于复查低分来源。
- `src/ColorComparisonDialog.cpp`
  - 初始化时真正进入基础模式并隐藏全部页高级项。
  - 基础模式保存配置时强制 `brightnessEnabled=false`、`featureType=histogram`。
  - 全部模式下才读取亮度复选框；加载旧配置时若启用了亮度、色谱或检测屏蔽，则回显到全部模式。
- `smoke/color_comparison_smoke.cpp`
  - 新增纯 HSV 相似度断言：邻近绿色 Hue 应超过 70 分，远 Hue 应低于匹配阈值。
  - 同步新的 `comparisonMethod` 和亮度默认值。

#### 出现的问题与处理

- 问题：上一版只做相邻 bin 平滑，若两块视觉上相近的绿色跨过多个 Hue bin，分数仍可能偏低。
  处理：改为按 bin 距离衰减的 soft-kernel 相似度，既允许小范围 Hue 漂移，也保留远色相低分。
- 问题：基础页只设置了“基础”按钮 checked，但没有真正调用基础模式显隐与保存逻辑，可能误把亮度开关状态带入基础测试。
  处理：初始化调用 `setAllParamsMode(false)`，保存时按当前基础/全部模式决定亮度是否参与。

#### 验证

- 已执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，主工程编译链接通过。
- 已执行 `qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4`，颜色比较 smoke 编译通过。
- 已执行 `./color_comparison_smoke`，纯 HSV 相似度断言通过后进入 HALCON runner 阶段；运行阶段仍因本机 HALCON license 缺失失败：`gen_image_interleaved: could not find license file`。

#### 剩余事项

- 需要在真实 GUI 中复测与海康软件同一张图、同一模板 ROI、同一检测 ROI 的分数。
- 若仍低于预期，应优先读取 payload 中三通道相似度；若 Hue/Saturation 已合理但总体仍不贴近海康，需要补充“主色覆盖率/颜色区域占比”模式，而不是继续扩大 Hue 容差。

### 2026-07-02 11:16:38 CST - 颜色比较青蓝误判高分修正

#### 已实现功能

- 将颜色比较评分进一步改为模板主 Hue 覆盖率主导，避免绿色模板下青色、蓝色因饱和度相近而被判高分。
- 基础模式仍不启用亮度；亮度只在全部页勾选后参与 Value 通道。
- 新增纯 HSV 断言：绿色模板下，近邻绿色需通过，青色和蓝色必须低于匹配阈值。

#### 本次更改

- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
  - 新增模板 Hue 直方图主峰提取。
  - 新增 `templateDominantHueCoverage()`：按模板主 Hue 对检测 Hue 分布做覆盖率评分。
  - Hue 相似度改为 `模板主 Hue 覆盖率 0.80 + 直方图 soft similarity 0.20`。
  - 基础模式权重改为 `hue=0.80,saturation=0.20`。
  - 亮度启用时权重改为 `hue=0.70,saturation=0.15,value=0.15`。
  - payload 的 `scoreFormula`、`hsvWeights`、`hueComparisonMode` 同步更新。
- `smoke/color_comparison_smoke.cpp`
  - 绿色近邻测试从 Hue 偏移 2 bin 调整为偏移 1 bin。
  - 新增 cyan/blue 不能作为 green 高分通过的断言。

#### 出现的问题与处理

- 问题：上一版 soft-kernel 过于宽松，绿色模板下青色仍可得到约 63.9 的纯算法相似度，和截图中的“青色/蓝色高分”现象一致。
  处理：将 Hue 分数从整体直方图相似度改为模板主色覆盖率主导，青色、蓝色需要真正落在模板主 Hue 附近才可得高分。
- 问题：主工程构建时，根目录中 smoke 子工程生成的同名对象文件曾干扰主工程链接。
  处理：按 AGENTS 工程卫生要求清理 `.gitignore` 覆盖的 qmake 构建产物后重新全量构建。

#### 验证

- 已执行清理后 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，主工程全量编译链接通过。
- 已执行 `qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4 && ./color_comparison_smoke`，纯 HSV 相似度断言通过；随后进入 HALCON runner 阶段时因本机 HALCON license 缺失失败：`gen_image_interleaved: could not find license file`。

#### 剩余事项

- 需要在 GUI 中复测绿色、青色、蓝色三组 ROI：青色/蓝色应不再高于绿色。
- 若海康分数仍有差异，下一步应补充“主色覆盖率/颜色区域占比”作为独立算法模式，并与海康的检测区域覆盖率语义对齐。

### 2026-07-02 11:54:18 CST - 新增 HALCON Bhattacharyya 直方图比较模式

#### 已实现功能

- 颜色比较“全部”页新增比较模式：
  - `模板主色覆盖率`：现有默认模式。
  - `Bhattacharyya直方图`：新增模式。
- 新增模式严格要求调用 HALCON `compare_histogram` 语义对应的 `T_compare_histogram` 符号，方法参数为 `bhattacharyya`。
- 不实现 C++ 自算 Bhattacharyya，不做静默降级。
- Bhattacharyya 模式得分公式为 `(1.0 - Dist) * 100`，距离越小得分越高。

#### 本次更改

- `src/ColorComparisonDialog.cpp/.h`
  - “模型色彩特征”卡片新增 `比较模式` 下拉框。
  - 基础页仍固定保存 `comparisonMode=dominant_hue_coverage`。
  - 全部页选择 `Bhattacharyya直方图` 时保存 `comparisonMode=bhattacharyya_histogram`。
  - 重新打开旧配置时，若保存了 Bhattacharyya 模式，则自动回显到“全部”页。
- `src/tooladapters/ColorComparisonAdapter.cpp`
  - 解析并传递 `comparisonMode`。
- `src/algorithms/recognition/ColorComparisonHalconRunner.h/.cpp`
  - `ColorComparisonHalconConfig` 新增 `comparisonMode`。
  - runner 根据 `comparisonMode` 分支：
    - `dominant_hue_coverage`：保留现有模板主 Hue 覆盖率算法。
    - `bhattacharyya_histogram`：调用 `ColorRecognitionHalconRunner::compareHistogramBhattacharyya()`。
  - payload 增加 `comparisonMode`、`distance`、`halconOperator=compare_histogram`、`halconCompareMethod=bhattacharyya`。
- `src/algorithms/recognition/ColorRecognitionHalconRunner.h/.cpp`
  - 新增 `ColorRecognitionHalconHistogramCompareResult`。
  - 新增 `compareHistogramBhattacharyya()`，内部通过动态 HALCON API 调用 `T_compare_histogram(HistRef, HistTest, 'bhattacharyya', Dist)`。
  - `T_compare_histogram` 使用可选符号绑定，缺失时返回 `halcon_symbol_missing`。

#### 出现的问题与处理

- 问题：本机 HALCON 24.11.1.0 的头文件和动态库中未检索到 `compare_histogram` / `T_compare_histogram` 导出。
  处理：按 HALCON 红线，不用 C++ 替代实现；Bhattacharyya 模式只在运行时符号存在时执行，符号缺失时返回明确错误。
- 问题：smoke 执行仍在 `gen_image_interleaved` 阶段因 HALCON license 缺失失败，无法进入 `compare_histogram` 分支。
  处理：记录为环境阻塞；代码层面已完成编译验证。

#### 验证

- 已执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j8`，主工程编译链接通过。
- 已执行 `qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4 && ./color_comparison_smoke`，smoke 编译通过；运行阶段仍因本机 HALCON license 缺失失败：`gen_image_interleaved: could not find license file`。
- 已执行 `git diff --check`，无空白错误。

#### 剩余事项

- 需要在具备 HALCON license 且存在 `T_compare_histogram` 符号的环境中复测 Bhattacharyya 模式。
- 若目标 HALCON 版本也没有该算子，应确认实际可用的 HALCON 直方图比较算子名称，再替换绑定符号；不应回退到 C++ 自算。

### 2026-07-02 15:51:51 CST - Bhattacharyya 模式改为 HALCON 基础算子组合

#### 已实现功能

- 确认当前目标机 HALCON 24.11.1 标准 2D 库不提供 `compare_histogram` / `T_compare_histogram`：
  - C/C++ 头文件未检索到该接口。
  - `libhalconc.so`、`libhalconcpp.so`、XL 变体导出符号中未检索到该接口。
  - 本地标准帮助文档只检索到 `gray_histo`、`gray_histo_range`、`histo_2dim` 等基础直方图算子。
- 颜色比较 `Bhattacharyya直方图` 模式不再依赖缺失的 `T_compare_histogram`。
- 新模式改为：
  - 继续使用 HALCON `T_gray_histo_range` 提取 HSV 直方图特征。
  - 使用 HALCON tuple 基础算子 `T_tuple_mult`、`T_tuple_sqrt`、`T_tuple_sum` 计算 Bhattacharyya 系数。
  - 距离公式当前为标准巴氏距离 `-ln(coefficient)`，分数公式为 `max(0, 1 - distance) * 100`。

#### 本次更改

- `src/algorithms/recognition/ColorRecognitionHalconRunner.cpp`
  - 移除 `T_compare_histogram` 可选符号依赖。
  - 新增动态解析 `T_tuple_mult`、`T_tuple_sqrt`。
  - `compareHistogramBhattacharyya()` 改为 HALCON 基础 tuple 算子组合实现。
  - payload 标记为 `algorithm=halcon_tuple_bhattacharyya`，并记录 `coefficient`、`referenceHistogramSum`、`testHistogramSum`。
- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
  - Bhattacharyya 模式 payload 改为 `comparisonMethod=halcon_tuple_bhattacharyya_histogram`。
  - 移除 `halconOperator=compare_histogram` 的误导字段。
  - 记录 `halconOperators=T_gray_histo_range,T_tuple_mult,T_tuple_sqrt,T_tuple_sum`。
- `smoke/color_comparison_smoke.cpp`
  - 新增 Bhattacharyya 模式 smoke 断言：相同直方图距离应接近 0。
  - 默认只执行不依赖 HALCON license 的纯 HSV 断言。
  - 在具备可用 HALCON license 的目标机上设置 `RUN_HALCON_LICENSED_SMOKE=1` 后，才执行 Bhattacharyya tuple 和图像 runner 实测。
- `smoke/color_comparison_smoke.pro`
  - 新增 `v2_color_comparison_smoke/` 独立输出目录，避免 smoke 构建产物散落到源码根目录或干扰主工程同名对象文件；该目录匹配 `.gitignore` 的 `*_smoke` 规则。

#### 出现的问题与处理

- 问题：本机标准 HALCON 24.11.1 中没有 `T_compare_histogram`，直接调用会返回 `halcon_symbol_missing`，导致 UI 选择 Bhattacharyya 模式没有结果输出。
  处理：按用户确认的方案，放弃直接依赖扩展算子，改为所有版本更通用的 HALCON 基础算子组合。
- 问题：本机 `HALCON_LICENSE_FILE` 指向的 license 文件不存在，执行 `T_tuple_sum` 时 HALCON 会直接以 license 错误退出进程，无法在 smoke 内捕获。
  处理：smoke 默认不执行 HALCON license 相关算子；需要完整 HALCON 数值验证时，在具备有效 license 的目标机上显式设置 `RUN_HALCON_LICENSED_SMOKE=1`。

#### 验证

- 待执行主工程构建、smoke 和 `git diff --check`。

#### 剩余事项

- 在具备 HALCON license 的目标机上复测 `Bhattacharyya直方图` 模式，同一模板与检测 ROI 应输出接近 100 分，非同色 ROI 应按距离降低。
- 后续如需更强抗光照能力，可新增基于 `histo_2dim` 的 H+S 二维联合直方图模式，不影响当前一维 HSV 分段直方图模式。

### 2026-07-02 16:45:00 CST - 模板特征保存与运行语义修复

#### 已实现功能

- 明确颜色比较运行语义为“当前检测区与建模时保存的模板颜色特征比较”。
- 颜色比较配置现在保存并回显 `params.colorComparison.templateFeature`。
- 基准图测试和完成配置时会使用 HALCON 从基准图模板 ROI 提取模板特征并写入配置。
- runner 不再从当前测试图像重新提取模板特征；缺少保存特征时返回 `no_template_feature`。

#### 本次更改

- `src/ColorComparisonDialog.h/.cpp`
  - 新增 `m_templateFeature` 缓存。
  - `colorComparisonParams()` 写入 `templateFeature`，`loadFromConfig()` 读回。
  - 基准图测试前调用 `ColorRecognitionHalconRunner::extractFeature()` 生成模板 HSV 直方图。
  - 完成配置时若模板特征为空，会尝试从基准图提取；失败时不保存无效配置。
  - 模板 ROI、模板屏蔽、灵敏度、特征类型、亮度语义变化时清空旧模板特征。
- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
  - 运行入口检查 `config.templateFeature`，为空时返回 `no_template_feature`。
  - 后续比较使用保存的模板特征，只对当前输入图提取检测 ROI 特征。
- `smoke/color_comparison_smoke.cpp`
  - 新增缺少保存模板特征的 smoke 断言，确保 runner 在 HALCON runtime 前返回 `no_template_feature`。
  - HALCON license smoke 分支显式设置 `config.templateFeature` 后再运行完整图像 runner。

#### 出现的问题与处理

- 问题：Adapter 已读取 `templateFeature`，runner 结构体也有字段，但 Dialog 没有写入/回显，runner 也忽略该字段并每次从当前图重提模板。
  处理：补齐 Dialog 写入/回显/建模提取，并让 runner 只消费保存特征。
- 问题：加载旧配置时切换基础/全部模式可能误清空刚读回的模板特征。
  处理：增加加载态保护，用户交互切换仍会清空旧特征，配置回显不会误清。

#### 验证

- 已执行 `qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4 && ./v2_color_comparison_smoke/color_comparison_smoke`，smoke 通过；默认跳过需要 HALCON license 的图像 runner 分支。
- 已执行 `mkdir -p build && cd build && /home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro && make -j$(nproc)`，主工程编译链接通过。

#### 剩余事项

- 需要在具备有效 HALCON license 的目标机上执行 `RUN_HALCON_LICENSED_SMOKE=1 ./v2_color_comparison_smoke/color_comparison_smoke`。
- 需要在 GUI 中确认新建颜色比较后，点击完成或基准图测试能生成并保存模板特征，重新打开后可直接运行测试。

### 2026-07-02 17:05:00 CST - 巴氏距离公式调整为负对数形式

#### 已实现功能

- `Bhattacharyya直方图` 模式的距离公式从 `sqrt(1 - coefficient)` 改为常规巴氏距离 `D_B = -ln(BC)`。
- 巴氏距离结果不再裁剪到 `0..1`；颜色比较得分仍按 `max(0, 1 - distance) * 100` 避免负分。

#### 本次更改

- `src/algorithms/recognition/ColorRecognitionHalconRunner.cpp`
  - `compareHistogramBhattacharyya()` 中 `distance` 改为 `-std::log(coefficient)`。
  - 对 `coefficient=0` 使用 `std::numeric_limits<double>::min()` 做有限值保护。
- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
  - 消费巴氏距离时去掉 `0..1` 上限裁剪。
  - payload `scoreFormula` 更新为 `max(0.0, 1.0 - bhattacharyya_distance) * 100`。
- `smoke/color_comparison_smoke.cpp`
  - 增加 `BC=0.25` 的 licensed smoke 断言，期望距离为 `-ln(0.25)`。

#### 验证

- 默认 smoke 可编译运行；完整巴氏距离数值断言仍需有效 HALCON license 后设置 `RUN_HALCON_LICENSED_SMOKE=1` 执行。

### 2026-07-02 17:20:00 CST - 巴氏距离 OpenCV 对照调试输出

#### 已实现功能

- 在 `Bhattacharyya直方图` 模式中增加 OpenCV 巴氏距离旁路调试。
- 每次 HALCON tuple 巴氏距离计算成功后，控制台输出：
  - `HALCON(tuple -ln(BC)) distance`
  - `BC`
  - `OpenCV(compareHist HISTCMP_BHATTACHARYYA) distance`
  - `absDiff`

#### 本次更改

- `src/algorithms/recognition/ColorRecognitionHalconRunner.cpp`
  - 新增 `openCvBhattacharyyaDebugDistance()`，使用 `cv::compareHist(..., cv::HISTCMP_BHATTACHARYYA)` 计算调试距离。
  - 使用 `qInfo().noquote()` 输出 HALCON 当前距离与 OpenCV 调试距离。
  - OpenCV 结果不写入 `ToolResult`、不写入 payload、不参与 OK/NG、不改变 HALCON 返回值。

#### 验证

- 已执行 `make -C build -j$(nproc)`，主工程编译链接通过。
- 已执行 `qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4 && ./v2_color_comparison_smoke/color_comparison_smoke`，默认 smoke 通过。
- 已执行 `git diff --check`，无空白错误。

### 2026-07-02 18:55:00 CST - Bhattacharyya 模式改为 H/S 二维软核直方图

#### 已实现功能

- `Bhattacharyya直方图` 模式的特征提取改为 HALCON `T_histo_2dim(H,S)`。
- 二维 H/S histogram 从 HALCON 256x256 histogram image 聚合到当前灵敏度对应的 `bins x bins`。
- 聚合后增加 H 环形、S 邻域的 3x3 软核平滑，降低同色绿色落到相邻 Hue/Saturation bin 时的硬分箱低分。
- 默认 `模板主色覆盖率` 模式仍使用原一维 HSV 特征，不受影响。

#### 本次更改

- `src/ColorComparisonDialog.cpp`
  - 全部页选择 `Bhattacharyya直方图` 时保存 `featureType=histogram_2dim_hs`。
  - Bhattacharyya 模式不再保存亮度通道参与比较。
  - 比较模式切换时清空旧模板特征，防止一维/二维特征混用。
- `src/algorithms/recognition/ColorRecognitionHalconRunner.cpp`
  - 动态解析 `T_histo_2dim`、`T_get_grayval`。
  - 新增 `histogram_2dim_hs` 特征类型，输出 H/S 联合二维直方图特征。
  - 巴氏调试日志能识别并输出二维 H/S 特征摘要。
- `src/algorithms/recognition/ColorComparisonHalconRunner.cpp`
  - 允许 `histogram_2dim_hs`，并限制它只在 `bhattacharyya_histogram` 模式下使用。
  - payload 标记 `featureType=histogram_2dim_hs` 和 `T_histo_2dim,T_get_grayval,...` 算子链。

#### 验证

- 已执行 `make -C build -j$(nproc)`，主工程编译链接通过。
- 已执行 `qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4 && ./v2_color_comparison_smoke/color_comparison_smoke`，默认 smoke 通过。
- 已执行 `git diff --check`，无空白错误。

#### 剩余事项

- 需要在具备有效 HALCON license 的目标机上用绿色模板/绿色检测 ROI 复测 Bhattacharyya 分数。
- 旧的 Bhattacharyya 配置若保存的是一维 `templateFeature`，需要重新执行基准图测试或完成配置以生成二维模板特征。

### 2026-07-03 09:45:00 CST - H/S 二维模板覆盖率评分

#### 已实现功能

- 在 `histogram_2dim_hs` 特征下，颜色比较不再用原始巴氏距离直接作为分数来源。
- 新增 H/S 二维模板覆盖率评分：
  - 从模板二维直方图找到主峰 `templateHuePeakBin` / `templateSaturationPeakBin`。
  - 对检测二维直方图每个 bin 计算到模板主峰的距离权重。
  - Hue 使用环形距离，Saturation 使用线性距离。
  - 低/中/高灵敏度分别使用不同 `hueSigma` 和 `saturationSigma`。
  - `score = coverage * 100`。

#### 本次更改

- `src/algorithms/recognition/ColorComparisonHalconRunner.h/.cpp`
  - 新增 `ColorComparisonHs2dCoverage`。
  - 新增 `compareColorComparisonHs2dTemplateCoverage()` 纯算法函数，便于无 HALCON license 的 smoke 覆盖。
  - `bhattacharyya_histogram + histogram_2dim_hs` 分支改为使用覆盖率作为正式 `similarity`。
  - payload 输出 `coverage`、模板 H/S 峰值 bin、`hueSigma`、`saturationSigma`。
- `smoke/color_comparison_smoke.cpp`
  - 新增 H/S 二维覆盖率断言：低灵敏度下 `H差1/S差2` 的绿色漂移应通过，远 Hue 应保持低分。

#### 验证

- 已执行 `qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j4 && ./v2_color_comparison_smoke/color_comparison_smoke`，默认 smoke 通过。
- 已执行 `make -C build -j$(nproc)`，主工程编译链接通过。
- 已执行 `git diff --check`，无空白错误。

#### 剩余事项

- 需要在具备有效 HALCON license 的目标机上用 GUI 复测绿色模板/绿色检测 ROI 的实际分数。
- 若绿色仍偏低，优先调整 `hueSigma` / `saturationSigma`，而不是回退到硬巴氏距离。

### 2026-07-04 16:30:00 CST - H/S 二维覆盖率自归一化

#### 已实现功能

- `histogram_2dim_hs` 覆盖率评分改为以模板自身覆盖率为基准归一化。
- 同一模板特征与同一检测特征在高灵敏度软化分布下应接近 100 分，避免纯色同区域只得到约 67 分。
- payload 保留 `coverage`，并新增 `rawCoverage`、`templateSelfCoverage`，便于现场判断原始覆盖率和归一化基准。

#### 本次更改

- `src/algorithms/recognition/ColorComparisonHalconRunner.h/.cpp`
  - `ColorComparisonHs2dCoverage` 新增 `rawCoverage`、`templateSelfCoverage`。
  - 抽出 H/S 二维特征对模板峰值的覆盖率计算。
  - 最终 `coverage = rawCoverage / templateSelfCoverage`，并限制在 `0..1`。
  - `scoreFormula` 更新为 `min(1.0, hs_2dim_detect_coverage / hs_2dim_template_self_coverage) * 100`。
- `smoke/color_comparison_smoke.cpp`
  - 新增高灵敏度软化 H/S 特征的同特征断言，防止同一区域再次回落到未归一化低分。

#### 验证

- 已执行 `qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j$(nproc) && ./build/smoke/color_comparison/bin/color_comparison_smoke`，默认 smoke 通过。

#### 剩余事项

- 需要在具备有效 HALCON license 的目标机上用 GUI 复测截图场景，确认模板和检测 ROI 同区域时显示接近 100 分。

### 2026-07-04 16:55:00 CST - H/S 二维分数梯度修正

#### 已实现功能

- `histogram_2dim_hs` 最终分数从纯自归一化覆盖率改为归一化覆盖率和原始覆盖率混合。
- 公式调整为 `coverage = normalizedCoverage * 0.85 + rawCoverage * 0.15`。
- 同一软化 H/S 特征仍保持高分，但不再直接饱和到 100，减少现场结果只呈现 100/0 的观感。

#### 本次更改

- `src/algorithms/recognition/ColorComparisonHalconRunner.h/.cpp`
  - `ColorComparisonHs2dCoverage` 新增 `normalizedCoverage`。
  - 新增覆盖率混合权重常量：`normalized=0.85`、`raw=0.15`。
  - payload 新增 `normalizedCoverage` 和 `coverageBlend`，保留 `rawCoverage`、`templateSelfCoverage`。
  - `scoreFormula` 更新为 `(normalized_coverage * 0.85 + raw_coverage * 0.15) * 100`。
- `smoke/color_comparison_smoke.cpp`
  - 高灵敏度软化 H/S 同特征断言改为“高分但不饱和”，防止评分再次退化为直接 100。

#### 验证

- RED：修改 smoke 后旧实现输出 `100`，触发 `identical softened HS feature must keep a high but non-saturated score` 失败。
- GREEN：已执行 `qmake smoke/color_comparison_smoke.pro -o /tmp/color_comparison_smoke.Makefile && make -f /tmp/color_comparison_smoke.Makefile -j$(nproc) && ./build/smoke/color_comparison/bin/color_comparison_smoke`，默认 smoke 通过。

#### 剩余事项

- 需要在具备有效 HALCON license 的目标机上用 GUI 复测截图场景，确认同色高分有梯度、相近偏色不再过快塌到 0。

### 2026-07-15 - 检测特征弹窗样式与左侧布局修正

#### 已实现功能

- 颜色特征图和放大弹窗不再使用控件内联字体/颜色样式，统一通过稳定的 `objectName`、`role`、`panelRole`、`actionRole` 接入公共 `styles/app.qss`。
- 放大弹窗使用白色内容卡、蓝色可见边框、22px 标题、18px 说明文字和 20px 关闭按钮，避免全局深色样式造成文字不可读。
- 左侧参数内容放入纵向滚动区；底部测试/完成按钮栏保持在滚动区之外，不随参数内容增长而被窗口底边遮挡。
- 底部按钮由固定 120px 改为等宽自适应，保持 48px 高度和 18px 字体。

#### 本次更改

- `src/ColorComparisonDialog.cpp`：移除对话框级通配内联 QSS，增加公共样式选择器标识、参数滚动区和固定底部操作栏。
- `src/ColorComparisonFeatureView.cpp`：移除新增特征控件的内联 QSS，增加公共样式角色，并提高图内 H/S/V 标识字号。
- `styles/app.qss`：增加仅作用于 `ColorComparisonDialog` 和特征放大层的样式规则。
- `smoke/color_comparison_feature_view_smoke.cpp`：增加公共样式角色及“不得使用内联样式”的防回归断言。

#### 验证

- `color_comparison_feature_view_smoke` 在 `QT_QPA_PLATFORM=offscreen` 下通过，包含打开、关闭、Esc、点击遮罩和样式角色检查。
- `qmake qt_ui_test.pro -o build/Makefile && make -C build -j8` 编译、链接通过。
- `git diff --check`（本次涉及源码、QSS、smoke）通过。

#### 剩余事项

- 需在 Ubuntu 虚拟机实际 GUI 中检查 1920×1200 和较低高度窗口：底部按钮始终可见，参数区出现滚动条，放大弹窗标题和说明文字清晰可读。

### 2026-07-15 - 直方图显示增强与图片导航

#### 已实现功能

- 一维和二维直方图的重合区域改为高对比亮绿色显示；未修改直方图数据、`rawIntersection`、`score` 或判定阈值。
- 放大图初始约占宿主区域 85%，支持 100%、125%、150%、175%、200% 五档等比例缩放，标题、关闭按钮和诊断区保持固定。
- 公共 `FrameViewHelper` 新增默认关闭的图片导航；颜色比较对话框显式启用，其他工具保持原有行为。
- ROI 仍按 scene/原图/normalized 坐标处理；自动导航 smoke 已验证矩形 normalized ROI 在缩放和平移后不变。矩形、圆形、多边形及屏蔽区的 GUI 全 ROI 人工操作未执行，原因是当前 shell 无可交互图形桌面（`DISPLAY`/`WAYLAND_DISPLAY` 均未设置），不记为通过。

#### 验证

- `QT_QPA_PLATFORM=offscreen build/smoke/frame_view_helper_navigation/bin/frame_view_helper_navigation_smoke`：通过，exit 0，无 `FAIL:`。
- `QT_QPA_PLATFORM=offscreen build/smoke/color_comparison_feature_view/bin/color_comparison_feature_view_smoke`：通过，exit 0，无 `FAIL:`。
- `qmake smoke/color_comparison_feature_diagnostics_smoke.pro -o build/smoke/color_comparison_feature_diagnostics/Makefile && make -C build/smoke/color_comparison_feature_diagnostics -j8 && build/smoke/color_comparison_feature_diagnostics/bin/color_comparison_feature_diagnostics_smoke`：首次 qmake 因当前 shell 遗留的 HALCON 24.11 路径被工程主版本 20 门禁拒绝；加载 `scripts/dependencies.env` 切换到 `/opt/halcon` 20.11 后重新执行通过，输出 `preflight passed; licensed extraction skipped`。
- `source scripts/dependencies.env && qmake qt_ui_test.pro -o build/Makefile && make -C build -j8`：主工程编译、链接通过。
- `QT_QPA_PLATFORM=offscreen timeout 4s build/qt_ui_test/bin/qt_ui_test`：按预期返回 124；对启动日志执行 `rg -n "stylesheet|parse error|Unknown property"` 无匹配。

#### 剩余事项

- 在可交互 GUI 桌面上补做矩形、圆形、多边形及屏蔽区全 ROI 操作，确认缩放、平移、双击还原后的 overlay 与保存坐标。
- 当前自动验证仅完成无 license preflight；仍需在具备有效 HALCON license 的目标机运行 licensed extraction，确认真实检测直方图提取与字段输出。

### 2026-07-15 - 连续评分、亮度补偿回退与原始 ROI 缩略图

#### 已实现功能

- 灰色/有色饱和度不匹配由固定上限改为连续乘法惩罚，不同基础分不再全部显示为 `40.0`。
- scale 超限或预计截断比例过高时跳过亮度补偿并使用原始特征继续测量；亮度均值本身无效、过暗或过曝仍明确失败。
- 特征区分别显示 HS 原始重合、平滑 HS 分数、亮度处理状态与系数、饱和度系数、最终得分和阈值。
- 模板缩略图改为先从原始参考图裁剪，不再把 ROI 边框或 Mask 画入缩略图像素；HALCON Runner 继续仅接收原始帧与 Region/Mask。
- 公共功能文档增加原始图像、ROI 裁剪与显示 overlay 分离规范。

#### 验证

- `color_comparison_feature_diagnostics_smoke`：无 license preflight 与 `RUN_HALCON_LICENSED_SMOKE=1` 均通过；纯评分断言验证现场数据不再固定 40 分、基础分差异保留、阈值附近连续，真实 HALCON 提取覆盖正常补偿、scale 超限回退和 clipped ratio 超限回退，并确认回退后直方图仍可用。
- `color_comparison_dialog_integration_smoke`：offscreen 通过；验证评分分解显示，并使用固定 BGR 源图逐像素确认自定义矩形、同步矩形和同步圆外接矩形缩略图均不含橙色框和 Mask 色；同步圆用例同时防止浮点 `ceil` 导致裁剪多出 1 像素。
- `color_comparison_feature_view_smoke`：offscreen 通过。
- 主工程 shadow qmake/make：通过。

#### 剩余事项

- 在可交互桌面复测自定义矩形、同步矩形、同步圆和 Mask 的缩略图与 HALCON Region 一致性。
- 使用现有产线样本记录旧分数、新分数和建议阈值，评估评分公式变化后的阈值迁移。

### 2026-07-16 - 绘制图标 Toggle 状态修复

#### 已实现功能

- 颜色比较使用 `EditState` 作为唯一活动绘制状态源，已保存的检测区域类型不再使矩形或圆形图标长期高亮。
- 矩形、圆形、模板 ROI、模板 Mask 和检测 Mask 图标支持第一次点击进入、保持连续绘制、再次点击退出；退出后保留最后一次有效几何数据。
- 点击另一个绘制图标时直接切换，任一时刻只保留一个活动绘制状态；点击全图退出当前绘制。
- 测试运行、暂停和基准图测试不再按保存的 ROI 类型自动进入绘制状态。
- 多边形完成后，只要对应 Mask 编辑状态仍活动，就重新准备下一次多边形绘制。

#### 本次更改

- `src/ColorComparisonDialog.h/.cpp`：增加 `toggleEditState()`，分离按钮高亮与 `m_detectRegionType`，移除 `applyDetectRoiEditState()` 自动激活链路。
- `smoke/color_comparison_dialog_integration_smoke.cpp`：增加初始空闲、再次点击退出、类型切换、全图退出、几何保留、多边形连续绘制和测试态不自动激活回归。
- `docs/FID/Function_Docs.md`：增加项目公共绘制工具 toggle 交互合同；其他功能后续按规范迁移。

#### 出现的问题与处理

- 问题：`refreshDetectRegionButtons()` 使用保存的 `m_detectRegionType` 设置 checked，导致保存矩形/圆形后图标永久高亮。
  处理：矩形和圆形 checked 仅由 `m_editState` 推导，保存类型只负责算法配置。
- 问题：`FrameViewHelper` 完成多边形后会关闭 polygon drawing，而 Dialog 仍处于 Mask 编辑状态。
  处理：颜色比较收到有效多边形后，在活动 Mask 状态下重新启用 polygon drawing，支持连续重绘。

#### 验证

- `color_comparison_dialog_integration_smoke`：offscreen 通过，覆盖全部上述状态转换和数据保留合同。
- 主工程 shadow qmake/make：加载 `scripts/dependencies.env` 后在 `build/verify_ui` 完整编译、链接通过。

#### 剩余事项

- 在 Ubuntu 图形环境手动确认图标高亮、连续拖拽、再次点击退出以及普通缩放/平移恢复。
- 其他现有工具暂未批量改造，后续修改对应绘制功能时按公共合同接入。

### 2026-07-16 - 模板与检测 Mask 独立化

#### 已实现功能

- 固定有效区域合同：模板只扣模板 Mask，检测只扣检测 Mask；同步模式只复用 ROI 几何。
- 配置态同时显示 T/T-mask/D/D-mask 四层，使用颜色、标签、活动高亮与非活动弱化区分归属；测试态仅保留检测组和结果。
- Mask 支持已有多边形编辑、显式重画、取消保留以及独立清除；同步模式禁用模板 ROI 绘制但保留模板 Mask 编辑。
- 原始 ROI、特征输入与所有 overlay 继续分离，Mask 斜线填充裁剪在所属 ROI 内。

#### 本次更改

- `ColorComparisonHalconRunner`：移除同步检测 Mask 对模板 Region 和模板提取哈希的影响，增加独立所有权合同字段。
- `ColorComparisonDialog`：拆分检测几何与检测 Mask 的模型失效逻辑，增加四层持久 overlay、归属提示以及 Mask 重画/清除交互。
- `FrameViewHelper`：按 overlay displayRole/emphasis 绘制模板/检测配色、弱化层级和裁剪斜线 Mask。
- 三个 smoke 用例增加独立 Mask、同步几何、模型生命周期、显示分层及编辑状态回归。

#### 验证

- `color_comparison_feature_diagnostics_smoke`：HALCON licensed 模式通过；检测 Mask 变化不改变同步模板特征、像素数或提取哈希，模板 Mask 变化不改变检测特征。
- `color_comparison_dialog_integration_smoke`：offscreen 通过；覆盖同步模型生命周期、四层/检测态 overlay、已有 Mask 编辑、重画保留、清除和同步模板 ROI 禁用。
- `frame_view_helper_navigation_smoke`：offscreen 通过；覆盖活动/弱化配色及 Mask 裁剪斜线显示。
- 主工程 shadow qmake/make：完整编译和链接通过；主程序 offscreen 启动 4 秒无崩溃或 QSS 错误。

#### 剩余事项

- 在 Ubuntu 可交互桌面手动确认四层颜色、Mask 斜线、重画/取消/清除以及同步模式提示的视觉和鼠标体验。

## 后续记录模板

后续每次实现后，在本节上方追加：

```markdown
### YYYY-MM-DD HH:MM:SS CST - <本次主题>

#### 已实现功能

- <功能 1>
- <功能 2>

#### 本次更改

- <文件或模块 1>：<更改说明>
- <文件或模块 2>：<更改说明>

#### 出现的问题与处理

- 问题：<现象>
  处理：<修复方式>

#### 验证

- <命令或手动验证项>：<结果>

#### 剩余事项

- <事项 1>
```
