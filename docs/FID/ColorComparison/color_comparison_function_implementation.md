![alt text](颜色比较基础.png)
![alt text](颜色比较全部.png)

# 颜色比较功能实现记录

## 文档用途

本文档单独记录颜色比较算子的需求、实现方案、每次实现的功能、更改内容、错误问题、验证结果和剩余事项。后续继续开发颜色比较时，以根目录 `AGENTS.md` 的项目约束和 HALCON 约束为最高规则，以本文档作为功能状态追踪依据。

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
