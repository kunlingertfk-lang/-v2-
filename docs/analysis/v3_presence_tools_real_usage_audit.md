# V3 六类有无/检测工具真实使用方式审计报告

只读审计对象：`D:\视觉软件v3\HZZHVision_LogicFrame\HZZHVision_LogicFrame`

目标：不是复述“用了什么 HALCON 算子”，而是审计 V3 旧工程中六类有无/检测工具到底如何被用户创建、配置、训练、保存、运行、判定和显示，并判断哪些思路适合迁移到 V2：`ToolCategory -> ToolType -> Dialog -> ToolConfig -> ToolEngine -> ToolAdapter -> Runner -> ToolResult -> Overlay`。

## 0. 安全检查和输出路径

- 已执行 `git status --short`，返回为空：V3 是 git 仓库，且当前工作区干净。
- 本轮没有执行 `git reset`、`git checkout`、删除、格式化或源码修改。
- 用户要求的 Linux 路径 `/home/hjl-ubuntu/桌面/qt_znxj_v2_work_1920_525analy/docs/analysis/v3_presence_tools_real_usage_audit.md` 在当前 Windows/PowerShell 会话不可达：`Test-Path` 为 `False`，`\\wsl$\Ubuntu\...` 不存在，`wsl` 命令不可用。
- 因此报告实际写到已确认存在的共享目录：`D:\vmare-shared\pattern_contour_debug\v3_presence_tools_real_usage_audit.md`。

## 1. V3 工具总体结构证据

V3 工具通过插件反射进入工具箱，而不是固定菜单硬编码：

- `VisionCore\Core\PluginHelper.cs`：`InitPlugin()` 加载 `Plugins\Plugin.*.dll`；`GetPluginInfo()` 读取 `[Category]`、`[DisplayName]`，配对 `BaseTool` 派生类和窗体类。
- `HZZHVision_LogicFrame\Main_Form\SubForm\Form_ToolBox.cs`：`DispalyVisionTool()` 按 category 展示工具；双击/拖拽时调用 `flow.CreateToolToFlow(pluginInfo)`。
- `VisionCore\Core\Flow.cs`：`CreateToolToFlow()` 用 `Activator.CreateInstance(info.ModObjType)` 创建工具；运行时依次 `bt.Run()`；`GetShowData()` 汇总每个工具的 `_ToolShowRes.ListRegionExtra` 和 `_ToolShowRes.ListTextExtra`。
- `HZZHVision_LogicFrame\Main_Form\SubForm\Form_ToolSetup.cs`：`ShowToolSetup(BaseTool tool)` 通过 `PluginsInfo.ModFormType` 打开参数窗体。
- `VisionCore\BasePlugin\BaseTool.cs`：`Run()` 包装 `InternalRun(ref message)`；`RunStatus.Result` 表示执行结果，`RunStatus.Success` 表示检测成功/失败。部分 V3 工具存在 `Result=Succeed` 但 `Success=false` 的混合表达，V2 不应照搬。
- `VisionCore\BasePlugin\BaseToolCollection.cs`、`VisionCore\Core\Project.cs`：工具对象 XML 保存/加载；加载后调用 `tool.Initialize()`。
- `VisionCore\Method\HWndCtrller.cs`：`Result_Base`、`Item_Obj`、`Text_Obj` 是 V3 Overlay 的核心表达。

结论：判断成熟度必须看入口、UI、训练、运行、结果、Overlay、保存加载是否闭环，不能只看某个算子是否出现。

## 2. V3 实际使用方式总表

| 工具 | V3 文件 | 用户入口 | 训练方式 | 运行方式 | 参数映射 | OK/NG 判定 | Overlay | 保存加载 | 成熟度 | 可抄等级 | V2 借鉴方式 |
|---|---|---|---|---|---|---|---|---|---:|---|---|
| 图案有无 | `Plugin.ImageMatching\形状匹配\ImageMatching_Obj.cs` / `ImageMatching_Form.cs`；参考 `Ncc匹配` | `[Category(识别检测)]` + `[DisplayName(形状匹配/NCC匹配)]`，工具箱可创建 | 用户画模板 ROI，点“创建模板”；支持模板 mask；默认全图搜索 ROI | 缓存 `modelHandle`，运行 `FindScaledShapeModel`；加载后由模板图和 ROI 重建模型 | MinScore、Angle、MaxOverlap、NumLevels、Greediness、MinContrast、TimeOut；Scale 固定 0.9~1.1 | 按 found count 与 `NumMatches` 判定；score 由 find 返回；存在 `Succeed` + `Success=false` 混用 | 搜索 ROI、score 文本、`GetShapeModelContours` 变换后的模型轮廓、mask/ROI | XML 保存参数；模板图/mask base data；不保存模型文件 | 4 | A | 照抄思路，重写为 V2 Config/Runner/Result/Overlay |
| 斑点有无 | `Plugin.BlobTool\BlobTool_Obj.cs`；参考 `DetectionTool_Area` | Blob 为 `[Category(识别检测)]` + `[DisplayName(斑点处理)]` | 无训练；用户画矩形/旋转矩形/圆 ROI | `Threshold -> Connection -> SelectShape/AreaCenter`；面积工具算 threshold 面积占比 | 灰度范围、面积范围、最大区域；面积工具为阈值范围和占比上下限 | BlobTool 更像特征提取，无严格 presence OK/NG；AreaTool 用面积占比上下限 | 显示真实 region、ROI、面积/占比文本 | 参数和 ROI XML 保存 | 3 | B | 参考 region 流程和显示，V2 判定要重写 |
| 圆有无 | `CircularTool`、`CircleCaliper` | 圆度工具在非标工具；圆卡尺在卡尺工具 | 画圆 ROI，无模板训练 | 区域圆：阈值+形态学+Circularity；圆卡尺：MeasurePos 采样点 + FitCircle | 圆度 limit/innerCircle；圆卡尺 sampleCount、threshold、sampleLimit、polarity、select、方向 | 圆度低于阈值 NG；圆卡尺拟合半径 > 0 即 OK | 圆度 region/text；圆卡尺拟合圆、采样矩形、边缘点 | ROI/HRegion base data + XML | 3 | B | V2 推荐区域圆 + 圆卡尺混合；不要搬硬编码 |
| 边缘有无 | `Plugin.EdgeDetectionTool\EdgeDetectionTool_Obj.cs` / Form | `[Category(边缘对齐)]` + `[DisplayName(边缘检测工具)]` | 无训练；用户画测量矩形 | `GenMeasureRectangle2 -> MeasurePos`，统计 edge count | Amplitude -> threshold；sigma=1；transition/select 固定 all/all；LimitLow/LimitUp | edge count 在范围内 OK，否则 NG | 测量矩形、边缘点短标记、数量文本 | 参数和 ROI XML 保存 | 3 | B | V2 应增加 MeasurePos 模式并补 polarity/select |
| 直线有无 | `卡尺工具\Plugin.LineCaliper\直线卡尺` | `[Category(卡尺工具)]` + `[DisplayName(直线卡尺)]` | 无训练；用户画 Rectangle2 线卡尺 ROI | `CreateMetrologyModel -> AddMetrologyObjectLineMeasure -> ApplyMetrologyModel` | sampleCount、threshold、polarity、select、minScore | 返回 4 个 line 参数即 OK；无长度/角度/fitError 范围 | 采样矩形、采样点、拟合线箭头、端点文本 | ROI/HRegion base data + XML | 4 | A | V2 LinePresence 建议以 Metrology Line 为主参考 |
| 轮廓有无 | `Plugin.ImageMatching\XLD匹配\ImageMatchingXLD_Obj.cs` | `[Category(识别检测)]` + `[DisplayName(轮廓匹配)]` | 用户编辑 region/XLD 模板再建模 | `GenContourRegionXld -> CreateScaledShapeModelXld -> FindScaledShapeModel` | MinScore、Angle、MaxOverlap、NumLevels、Greediness、MinContrast；Scale 固定 | 按模板匹配 count/score；不是纯轮廓长度/形状判定 | 显示模板模型轮廓变换结果，不是当前图 raw XLD | 模板图和 templateXldData 保存，加载重建 | 2 | C | 不适合直接做 V2 ContourPresence，只参考 XLD 模板匹配算子 |

## 3. 图案有无 / Pattern Presence 深审

### 3.1 对应实现和入口

找到完整实现：

- `Plugin.ImageMatching\形状匹配\ImageMatching_Obj.cs`，类 `ImageMatching_Obj`。
- `Plugin.ImageMatching\形状匹配\ImageMatching_Form.cs`，参数窗体和训练按钮。
- `Plugin.ImageMatching\Ncc匹配\ImageNccMatching_Obj.cs`，NCC 辅助路线。

入口证据：`ImageMatching_Obj` 带 `[Category(ProjectStructure.识别检测)]`、`[DisplayName(ProjectStructure.形状匹配)]`；NCC 工具带 `[DisplayName("NCC匹配")]`。用户从工具箱创建后，通过 `Form_ToolSetup.ShowToolSetup()` 打开窗体。

### 3.2 UI 参数、默认值和真实映射

`ImageMatching_Obj` 默认参数：`ht_NumMatches=1`，`ht_MinScore=40`，`ht_AngleStart=-45`，`ht_AngleExtent=90`，`ht_MaxOverlap=0.5`，`ht_NumLevels=5`，`ht_Greediness=0.5`，`ht_MinContrast="auto"`，`ht_TimeOut=200`，显示 ROI/轮廓/mask/search scope 的开关默认为 true。

真实映射：

- `ht_MinScore / 100` -> `FindScaledShapeModel` 的 MinScore。
- `ht_AngleStart/ht_AngleExtent` -> 建模和查找角度范围，角度转弧度。
- `ht_MaxOverlap` -> `FindScaledShapeModel` MaxOverlap。
- `ht_NumLevels` -> 运行时 `FindScaledShapeModel` NumLevels；建模时 `CreateScaledShapeModel` 的 NumLevels 是 `auto`。
- `ht_Greediness` -> `FindScaledShapeModel` Greediness。
- `ht_MinContrast` -> `CreateScaledShapeModel` MinContrast；combo 可选 `auto,1,2,3,5,...100`。
- `ht_TimeOut` -> `SetShapeModelParam(modelID, "timeout", ...)`。
- `ScaleMin/ScaleMax` 固定为 `0.9/1.1`，没有 UI。
- `Metric` 固定 `use_polarity`，`Optimization` 和 `Contrast` 为 `auto`。

NCC 工具参数类似，但 `ImageNccMatching_Obj.CreateNccModel()` 计算了 `MinContrast` 后没有把它传给 `CreateNccModel`，所以 NCC 的 MinContrast 更像残留参数，不能照搬。

### 3.3 ROI、模板设置和训练阶段

`ImageMatching_Form.button_创建模板_Click()` 是真实训练入口：用户画模板 ROI 后点击创建模板，代码调用 `CreateShapeModel(toolObj.InputImage, ShowWindow.roiCollects.GetRegion(), ...)`。模板 ROI 保存到 `ho_RegionSigns`；若搜索 ROI 为空，自动生成整图 `Rectangle1` 作为 `ho_SearchScopeSigns`。Mask 通过 `Form_Erase` 编辑，保存到 `ho_templateMaskRegionObj/ho_templateMaskRegionData`。

`ImageMatching_Obj.CreateShapeModel()` 的真实流程：

1. 输入图转灰度。
2. 如果存在 mask：`Difference(templateROI, mask)`，否则直接用模板 ROI。
3. `ReduceDomain(grayImage, modelRegion)`。
4. `CropDomain(imageReduced)` 只用于模板预览。
5. `CreateScaledShapeModel(imageReduced, "auto", AngleStart, AngleExtent, "auto", 0.9, 1.1, "auto", "auto", "use_polarity", MinContrast, "auto", out modelID)`。
6. `SetShapeModelParam(modelID, "timeout", ht_TimeOut)`。
7. 在模板图上 `FindScaledShapeModel` 验证模型，成功后保存基准 row/col/angle/score。
8. `GetShapeModelContours(modelID, 1)` 获取模型轮廓，平移到裁图中心用于模板预览。

重要结论：V3 没有自动 `threshold/connection/select_shape/fill_up/dilation` 生成 model domain；model domain 主要来自用户 ROI 减 mask。模板和搜索 ROI 是分开的；运行时使用 `ReduceDomain` 而不是裁图，所以匹配坐标仍是原图坐标，不需要裁图坐标映射。

### 3.4 运行、判定、结果和显示

`ImageMatching_Obj.InternalRun()` 会先确保 `modelHandle` 存在，然后调用 `Run(InputImage, modelHandle, out row, column, angle, score)`。`Run()` 如有搜索 ROI 就 `ReduceDomain(image, searchROI)`，然后 `FindScaledShapeModel`。

OK/NG：

- `ht_NumMatches == 0 && row.Count <= 0`：未匹配到目标，`RunStatus.Success=false`。
- `ht_NumMatches != 0 && row.Count != ht_NumMatches`：模板个数不匹配，`RunStatus.Success=false`。
- 最小分数由 HALCON 查找控制，不再二次判定。
- 缺陷：数量 NG 后函数末尾仍可能 `return ToolRunStatusConstant.Succeed`，V2 必须把执行状态和检测状态分清。

结果：`ModelMatch_Info 定位结果` 保存基准 `X/Y/Phi` 和匹配列表 `Coord_Info(X,Y,Phi,Data)`，其中 `Data` 是 score；没有保存 scale。

Overlay：

- 搜索 ROI 显示为黄色 region。
- 每个匹配显示 score 文本。
- 主轮廓来自 `GetShapeModelContours`，再用 `VectorAngleToRigid` + `AffineTransContourXld` 变换到匹配位置。
- 这不是用 ROI 边界、raw edge 或匹配框冒充模型轮廓；这一点对 V2 PatternPresence 当前轮廓显示问题很有价值。

### 3.5 保存/加载和成熟度

保存：参数随 XML 序列化；模板图 `ho_ImageSignData`、mask `ho_templateMaskRegionData` 用 `HObjectXmlHelper.GetBaseData()` 保存。加载：`Initialize()` 用 base data 恢复 HObject，再 `CreateModelHandel()` 重建模型。不保存 `.shm` 模型文件。

成熟度：4/5，可抄等级 A。它有真实入口、UI、训练、运行、判定、结果、Overlay、保存加载。缺点是没有自动 model domain、scale 固定、状态语义混乱。

V2 借鉴：Dialog 分模板 ROI/搜索 ROI/mask/高级参数；ToolConfig 保存 Contrast/MinContrast、ScaleMin/ScaleMax、Angle、MinScore、NumMatches；Runner 缓存模型；ToolResult 保存 score/angle/scale/count/轮廓；Overlay 显示真实模型轮廓。灵敏度可以映射到 Contrast/MinContrast，但要允许高级参数展开。还应参考 HALCON 官方 `inspect_shape_model`/自动 model domain/轮廓点数检查例程。

## 4. 斑点有无 / Blob Presence 深审

### 4.1 对应实现和入口

找到两条相关实现：

- `Plugin.BlobTool\BlobTool_Obj.cs`，类 `BlobTool_Obj`，入口 `[Category(ProjectStructure.识别检测)]` + `[DisplayName(ProjectStructure.斑点处理)]`。
- `检测工具\Plugin.DetectionTool_Area\DetectionToolArea_Obj.cs`，类 `DetectionToolArea_Obj`，入口 `[DisplayName("面积工具")]`。

BlobTool 更像“斑点特征提取”；AreaTool 更像“面积占比有无判定”。

### 4.2 参数、ROI 和运行

BlobTool 默认参数：`ht_MinGray=0`，`ht_MaxGray=255`，`ht_MinArea=0`，`ht_MaxArea=99999999`，`ht_ChckMaxArea=true`，显示 ROI/Blob region/面积文本均为 true。

用户可画矩形、旋转矩形、圆 ROI。`BlobTool_Form.UserWindow1_RoiDelegateEvent()` 保存 ROI；如果绑定定位工具，运行时会用 `GetReflection()` 对 ROI 做定位跟随。

BlobTool `Run()` 流程：

1. `ReduceDomain(image, roi)`。
2. `Threshold(imageReduced, ht_MinGray, ht_MaxGray)`。
3. `Connection()`。
4. 若 `ht_ChckMaxArea=true`，用 `AreaCenter + TupleMax + SelectShape` 只保留最大区域。
5. 否则 `SelectShape("area", ht_MinArea, ht_MaxArea)`。
6. `AreaCenter()` 和 `RegionFeatures("orientation")` 输出区域特征。

AreaTool 流程：`ReduceDomain -> Threshold(Amplitude,AmplitudeMax) -> AreaCenter(thresholdRegion) / AreaCenter(ROI) -> rate = area*100/roiArea -> LimitLow/LimitUp 判定`。

### 4.3 判定、结果和 Overlay

BlobTool 的关键问题：它没有成熟 presence OK/NG 判定。`InternalRun()` 主要填 `_RoiFeatures_Info.X/Y/Phi/Area` 并 `SetVarList(... DataMode.Roi特征 ...)`，即使没有区域也不一定 NG。它更像后续流程用的 ROI 特征工具。

AreaTool 有明确 OK/NG：面积占比超出 `[LimitLow, LimitUp]` 时 `RunStatus.Success=false`，显示 NG 文本和红色 region。

结果：BlobTool 输出 `RoiFeatures_Info`，保存中心、方向、面积列表；AreaTool 输出 `CurrentRate` 和 `Result`。两者都没有形成 V2 需要的统一 BlobPresence 结果：count、areas、centers、totalArea、largestArea、areaRate、OK reason。

Overlay：BlobTool 显示真实二值/筛选 region、ROI、面积文本，不只是 bbox；AreaTool 显示 ROI、threshold region、面积占比文本。

### 4.4 成熟度和 V2 借鉴

成熟度：3/5，可抄等级 B。

可借鉴：threshold、connection、select_shape、最大区域、定位跟随、真实 region overlay、面积占比判定。

不能照搬：BlobTool 的“只输出特征不判有无”；AreaTool 的过简化面积占比；默认阈值/上下限语义不完整。

V2 建议：BlobPresence Dialog 明确亮/暗/灰度范围、面积范围、数量范围、最大区域、总面积占比、开闭运算。Runner 用 `ReduceDomain -> Threshold -> Morphology -> Connection -> SelectShape -> AreaCenter`。ToolResult 保存 count、regions、centers、areas、totalArea、largestArea、areaRate、OK/NG reason。Overlay 以真实 region 为主，bbox 为辅。

## 5. 圆有无 / Circle Presence 深审

### 5.1 对应实现和真实路线

V3 有两类圆：

- 区域圆/圆度：`众力非标工具\Plugin.CircularTool\CircularTool_Obj.cs`，类 `CircularTool_Obj`，入口 `[Category(ProjectStructure.非标工具)]` + `[DisplayName("圆度工具ZL")]`。
- 圆卡尺：`卡尺工具\Plugin.CircleCaliper\CircleCaliper_Obj.cs`，类 `CircleCaliper_Obj`，入口 `[Category(ProjectStructure.卡尺工具)]` + `[DisplayName("圆卡尺")]`。

因此 V3 不是单一路线，而是区域圆和边缘圆并存。更成熟的是圆卡尺；区域圆工具项目定制强。

### 5.2 区域圆 CircularTool

用户画圆 ROI，窗体保存到 `runParam._Roi`、`HRegion`、`HRegBaseData`。默认参数：`_limit=90`，`_innerCircle=0`。

`CircularTool_Obj.InternalRun()`：

1. 检查 `HRegion`。
2. `AffineTransRegion(HRegion, HoMat2d)`，支持定位跟随。
3. 生成外圆/内圆并做 `Difference`，但差分区域后续有效性有限，存在死逻辑味道。
4. `ReduceDomain(InputImage, objCheckRegion)`。
5. 调用 `GetCircularity()` 得到 region 和 circularity。
6. `_圆度 = circularity * 100`。
7. `_圆度 < _limit` 判 NG，否则 OK。

`GetCircularity()` 的真实链：

`ReduceDomain -> ScaleImage -> BinaryThreshold(max_separability, light) -> OpeningCircle(15) -> Connection -> SelectShape(area>1500) -> Union1 -> ClosingRectangle1(100,100) -> ClosingCircle(15) -> FillUp -> Connection -> SelectShape(area>2000) -> ClosingCircle(50) -> FillUp -> Connection -> SelectShape(area>2000) -> Union1 -> ClosingCircle(10) -> FillUp -> ShapeTrans(convex) -> ErosionCircle(15) -> Circularity`

判断：这是路线 A“区域圆”，但阈值、面积、形态学尺寸大量硬编码，不是通用 Presence 工具。它适合作为某项目经验参考，不适合照搬。

### 5.3 圆卡尺 CircleCaliper

用户画圆 ROI；`CircleCaliper_Form` 保存 `runParam.dcCir`、`HRegion`、`HRegBaseData`。默认参数：`sampleCount=20`，`threshold=30`，`sampleLimit=30`，`eTrans=最强`，`eDir=最前`，`eMatchingDirection=由内向外`；`MeasurePos` sigma 固定 2。

`CircleCaliper_Obj.ProcessMeasureCircle()`：

1. `eTrans` 映射 polarity：白到黑 `negative`，黑到白 `positive`，最强 `all`。
2. `eDir` 映射 select：最前 `first`，最后 `last`，其他 `all`。
3. 沿圆周按 `sampleCount` 生成多个测量矩形。
4. 每个矩形执行 `GenMeasureRectangle2 -> MeasurePos`。
5. 收集边缘点 row/column。
6. `GenContourPolygonXld` 由边缘点生成 XLD。
7. `FitCircleContourXld(... "geotukey" ...)` 拟合圆。
8. 拟合半径大于 0 则成功。

结果：`RunResult.circle`、`RunResult.point`。缺少 fitError、score、edgeCount、validSampleRatio、半径范围判定。

Overlay：拟合圆、采样 region、边缘点 cross、圆心/半径文本。这个显示链很适合 V2。

### 5.4 成熟度和 V2 借鉴

成熟度：3/5，可抄等级 B。

- `CircularTool`：2/5，区域圆思路可参考，硬编码不要搬。
- `CircleCaliper`：4/5，边缘圆卡尺流程成熟，但判定结果还不是完整 presence。

V2 推荐混合路线：

- 基础模式：区域圆，`threshold -> connection -> select_shape(area/radius/circularity) -> smallest_circle/circularity`，适合孔、圆斑、圆形区域。
- 高级模式：圆卡尺，`MeasurePos` 采样边缘点 + `FitCircleContourXld`，适合边界清晰但灰度区域不稳定的圆。

下一步 V2 任务：`CirclePresenceConfig` 增加 `mode=region/caliper`；region 模式加阈值、面积、半径、圆度、数量；caliper 模式加 sampleCount、threshold、polarity、select、direction、minHitRatio、fitError。ToolResult 保存 center、radius、circularity、edgePointCount、fitError、OK/NG reason。

## 6. 边缘有无 / Edge Presence 深审

### 6.1 对应实现和入口

找到：`Plugin.EdgeDetectionTool\EdgeDetectionTool_Obj.cs`，类 `EdgeDetectionTool_Obj`；窗体 `EdgeDetectionTool_Form.cs`。类上有 `[Category(ProjectStructure.边缘对齐)]` 和 `[DisplayName("边缘检测工具")]`，会进入工具箱。

### 6.2 用户配置和 ROI

用户画一个测量矩形/线型区域，本质是 `Rectangle2`。窗体把 ROI 保存到 `runParam.DC_DetectRegion`。`GetFollowRegion()` 支持 `HoMat2d` 仿射跟随，变换后再 `SmallestRectangle2` 取测量矩形参数。

默认参数：`Amplitude=120`，`LimitLow=0`，`LimitUp=0`。`MeasurePos` 的 sigma 固定 1，transition 固定 `all`，select 固定 `all`。

### 6.3 运行链和参数映射

`EdgeDetectionTool_Obj.InternalRun()`：

1. 检查输入图和 ROI。
2. RGB 转灰度，单通道选择对象。
3. `GenRectangle2` 生成检测 region 用于显示。
4. `GenMeasureRectangle2(row, col, rad, length1, length2, width, height, "nearest_neighbor", out measureHandle)`。
5. `MeasurePos(imgGray, measureHandle, 1, runParam.Amplitude, "all", "all", out rowEdge, colEdge, amp, dist)`。
6. `edgeCount = rowEdge.Length`。
7. `edgeCount < LimitLow || edgeCount > LimitUp` 则 NG，否则 OK。

这证明 V3 的边缘有无本质是“沿测量矩形检测灰度跳变点”，不是 Canny/XLD 区域边缘检测。

### 6.4 结果、Overlay 和成熟度

结果：`RunResult.CurValue` 保存 edge count；`InRange/Message/UseTimeSec` 字段存在但实际赋值不完整。每个 edge 的 row/column/amplitude/distance 没有进入结构化结果，只在运行时用于显示。

Overlay：显示测量矩形；每个边缘点生成一个短 `Rectangle2` 标记；显示边缘数量文本，OK/NG 颜色区分。不是 raw XLD 轮廓显示。

成熟度：3/5，可抄等级 B。

优点：入口、UI、ROI、运行、OK/NG、Overlay、保存加载完整；MeasurePos 思路工业化。缺点：polarity/select 没暴露；默认 0/0 上下限不友好；结果字段太少。

V2 建议：EdgePresence 增加 MeasurePos 模式，Dialog 提供测量矩形、Amplitude、Sigma、Polarity、Select(first/last/strongest/all)、CountMin/CountMax。ToolResult 保存 edgePoints、amplitudes、distances、count、OK/NG reason。可以保留当前 edges_sub_pix/canny 作为“区域边缘”模式，但工业“边缘有无”应优先 MeasurePos。

## 7. 直线有无 / Line Presence 深审

### 7.1 对应实现和入口

找到成熟实现：`卡尺工具\Plugin.LineCaliper\直线卡尺\LineCaliper_Obj.cs`，类 `LineCaliper_Obj`；窗体 `LineCaliper_Form.cs`。类上有 `[Category(ProjectStructure.卡尺工具)]` 和 `[DisplayName(ProjectStructure.直线卡尺)]`。另有 `Plugin.NonAlignEdges\NonAlignEdges_Obj.cs` 用 line metrology + fitline 做线束边缘对齐，是专用工具，只作补充参考。

### 7.2 用户配置、ROI 和默认参数

用户画 `Rectangle2` 线卡尺 ROI。窗体在 ROI 事件中保存 `runParam.rect2`、`HRegion`、`HRegBaseData`；`HoMat2d` 支持定位跟随。

默认参数：`sampleCount=20`，`threshold=30`，`eTrans=最强`，`eDir=最前`，`score=0.3`。其中 `eTrans` 映射 `measure_transition`：黑到白 `positive`，白到黑 `negative`，最强 `all`；`eDir` 映射 `measure_select`：最前 `first`，最后 `last`，其他 `all`。

### 7.3 运行链

`LineCaliper_Obj.ProcessMeasureLine()` 是主流程：

1. `CreateMetrologyModel()`。
2. 根据 `rect2` 计算线段起点/终点。
3. 根据 ROI 和 `sampleCount` 推导测量矩形大小。
4. `AddMetrologyObjectLineMeasure(model, startRow,startCol,endRow,endCol, len1, len2, 1, threshold, ..., out index)`。
5. `SetMetrologyObjectParam("num_instances", 1)`。
6. `SetMetrologyObjectParam("num_measures", sampleCount)`。
7. `SetMetrologyObjectParam("measure_select", first/last/all)`。
8. `SetMetrologyObjectParam("measure_transition", positive/negative/all)`。
9. `SetMetrologyObjectParam("min_score", score)`。
10. `ApplyMetrologyModel()`。
11. `GetMetrologyObjectResult(... "all_param")`，预期返回 4 个 line 参数。
12. `GetMetrologyObjectMeasures()` 得到采样 region 和采样点。
13. 成功则写入 `RunResult.line` 和中点；失败则 `RunStatus.Success=false`。

文件内有 `FitLineContourXld` 备用路线的注释代码，但实际运行使用 MetrologyModel。

### 7.4 判定、结果、Overlay

OK/NG：只要 Metrology 返回 4 个 line 参数即 OK；失败时 Warning + `Success=false`。没有长度范围、角度范围、fitError、sample hit count 判定。`HRegion == null` 时返回 Succeed，这个默认行为不适合 V2。

结果：`RunResult.line`、`RunResult.point`。没有 score、fitError、angle、length、hitCount 等字段。

Overlay：采样 region、采样点 cross、拟合线 arrow、端点文本。这个 Overlay 表达成熟，能让用户判断线是否由真实边缘点拟合出来。

### 7.5 成熟度和 V2 借鉴

成熟度：4/5，可抄等级 A。

优点：入口、UI、ROI、保存加载、Metrology 算子链、结果、Overlay 都完整；参数接近工业卡尺语言。缺点：空 ROI 状态、疑似通道转换代码、结果字段和判定范围不足。

V2 建议：LinePresence 主路线采用或新增 Metrology Line。Dialog 保留 sampleCount、threshold、polarity、select、minScore，同时增加 length range、angle range、maxFitError、minHitCount。ToolResult 保存 endpoints、midpoint、angle、length、score、fitError、samplePoints、hitCount、OK/NG reason。Overlay 必须显示采样矩形、采样点和拟合线，不要只画 ROI。

## 8. 轮廓有无 / Contour Presence 深审

### 8.1 是否存在纯轮廓有无

未找到成熟的“纯轮廓有无”工具。找到的是 `Plugin.ImageMatching\XLD匹配\ImageMatchingXLD_Obj.cs`，类 `ImageMatchingXLD_Obj`，入口 `[Category(ProjectStructure.识别检测)]` + `[DisplayName("轮廓匹配")]`。它是 XLD/轮廓模板匹配，不是检测当前图 raw contour 的 length/count/shape presence。

### 8.2 模板创建和运行

用户需要通过窗体编辑 `templateXld`，再点击创建模板。`ImageMatchingXLD_Form` 调用 `CreateShapeModel(toolObj.InputImage, toolObj.templateXld, ...)`。核心证据在 `ImageMatchingXLD_Obj.CreateShapeModel()`：它对传入 region 调用 `GenContourRegionXld(modelRegon, "border")`，再 `CreateScaledShapeModelXld`。因此模板 XLD 本质上来自用户 region 边界，不是对模板图 `edges_sub_pix` 后得到的真实边缘轮廓。

默认参数：`ht_NumMatches=1`，`ht_MinScore=60`，`ht_AngleStart=-30`，`ht_AngleExtent=60`，`ht_MaxOverlap=0.5`，`ht_NumLevels=4`，`ht_Greediness=0.7`，`ht_MinContrast="auto"`，`ht_TimeOut=200`。Scale 固定 `0.9~1.1`，metric 为 `ignore_local_polarity`。

运行链：

1. `GenContourRegionXld(region, "border")`。
2. `CreateScaledShapeModelXld(contours, 4, angleStart, angleExtent, "auto", 0.9,1.1,"auto","auto","ignore_local_polarity", MinContrast, out modelID)`。
3. 运行时可 `ReduceDomain(searchROI)`。
4. `FindScaledShapeModel(grayImage, modelID, angleStart, angleExtent, 0.9,1.1,minScore,numMatches,maxOverlap,"least_squares",numLevels,greediness,...)`。
5. `GetShapeModelContours` + affine 显示模型轮廓。

### 8.3 判定、结果、Overlay

OK/NG：按模板匹配 count/score，不按 `length_xld`、contour count、轮廓面积、轮廓形状判定。

结果：和形状匹配一样写 `ModelMatch_Info`，score 在 `Coord_Info.Data` 中；scale 不保存。

Overlay：显示的是模板模型轮廓变换结果，不是检测图中 raw `edges_sub_pix` 的 XLD。它可用于“轮廓模板匹配”，不适合证明 V3 已有成熟的纯 ContourPresence。

### 8.4 成熟度和 V2 借鉴

成熟度：2/5，可抄等级 C。

作为 XLD 模板匹配，它有入口、参数、运行、显示、保存加载；作为 V2 纯轮廓有无，它语义不契合。V2 不应把它直接作为 ContourPresence。

V2 建议：如果做“轮廓模板匹配”，单独定义 ContourMatchPresence，参考 `CreateScaledShapeModelXld`。如果做“纯轮廓有无”，应按 `ReduceDomain -> edges_sub_pix -> segment_contours_xld/select_contours_xld -> length_xld/shape filter -> count/length 判定` 自己实现。Overlay 应显示当前图真实 XLD、选中轮廓、长度/数量文本。

## 9. 边缘、直线、轮廓的边界定义

| 类型 | V3 真实含义 | 核心算法 | V2 建议语义 |
|---|---|---|---|
| 边缘有无 | 沿测量矩形找灰度跳变点 | `GenMeasureRectangle2 -> MeasurePos` | 是否存在指定数量/方向/幅值的灰度跳变 |
| 直线有无 | 多个卡尺采样点拟合一条线 | `MetrologyModel Line` | 是否存在满足长度/角度/score/fitError 的直线 |
| 轮廓有无 | V3 没有纯轮廓；只有轮廓模板匹配 | `CreateScaledShapeModelXld -> FindScaledShapeModel` | 当前图真实 XLD 是否满足长度/形状/数量；模板匹配应另列 |

代码复用情况：V3 里 Edge、Line、Circle 都有卡尺/测量思想，但实现分散，没有统一 Caliper Engine。Line 使用 MetrologyModel，Edge 直接 MeasurePos，Circle 手写多卡尺后 fit circle。Contour XLD匹配和 Pattern Shape 匹配高度相似，但语义是模板匹配。

V2 参数必须分开：Edge 关注 threshold/polarity/select/count；Line 关注 sampleCount/minScore/angle/length/fitError；Contour 关注 edge extractor、contour length/count/shape filters。

## 10. “能不能照抄思路”的判断

| 工具 | 成熟度 | 可抄等级 | 判断依据 |
|---|---:|---|---|
| Pattern / 图案有无 | 4 | A | 真实 UI 入口、模板训练、搜索 ROI、mask、运行、count/score 判定、模型轮廓 Overlay、保存加载完整；状态语义和 Scale/Domain 需重写 |
| Blob / 斑点有无 | 3 | B | BlobTool UI/ROI/预览/region overlay 完整，但无严格 OK/NG；AreaTool 有 OK/NG 但简化；V2 判定需重写 |
| Circle / 圆有无 | 3 | B | 圆卡尺成熟，区域圆项目硬编码多；V2 应混合区域圆和圆卡尺 |
| Edge / 边缘有无 | 3 | B | MeasurePos 入口、ROI、判定、显示完整；参数和结果不足 |
| Line / 直线有无 | 4 | A | Metrology Line 流程成熟，Overlay 好；结果字段和判定要增强 |
| Contour / 轮廓有无 | 2 | C | V3 是 XLD 模板匹配，不是纯轮廓有无；只参考算子和模板显示 |

A 级可直接照抄思路：Pattern、Line。  
B 级主流程可参考：Blob、Circle、Edge。  
C 级只参考算子：Contour/XLD 模板匹配、CircularTool 的区域圆硬编码流程、NCC 中无效的 MinContrast 设计。  
D 级本次未发现必须完全丢弃的整类工具，但 V3 的 WinForms 事件、大对象状态、全局式流程、状态混用不建议迁移。

## 11. 对 V2 六个有无工具的迁移建议

### 11.1 PatternPresence

建议照抄 V3 思路，重写为 V2 架构。

照抄：模板 ROI 与搜索 ROI 分离、mask、建模/运行分离、缓存模型、`CreateScaledShapeModel/FindScaledShapeModel`、`GetShapeModelContours` affine 后显示、score/count 判定。

不照抄：Scale 固定 0.9~1.1、只靠 ROI 做 model domain、`Result=Succeed` 但 `Success=false` 的状态表达、WinForms 按钮逻辑。

需 HALCON 官方补充：自动 model domain、`inspect_shape_model`、模型轮廓点数检查、Contrast/MinContrast 质量策略。

下一批 V2 修改任务：新增 Pattern build/search config；灵敏度拆成 Contrast/MinContrast 或高级参数；ToolResult 增加 scale、modelContour、transformedContour、matchCount、OKNG reason。

### 11.2 BlobPresence

建议参考主流程，不照抄 V3 判定。

照抄：ROI 内 threshold、connection、select_shape、最大区域、真实 region overlay、面积占比思路。

不照抄：BlobTool 只输出特征不判有无；AreaTool 不做连通域筛选的简化逻辑。

需 HALCON 官方补充：亮/暗目标、开闭运算、count 范围、shape filters、总面积和最大面积双判定。

下一批 V2 修改任务：BlobPresence 增加 count range、area range、totalAreaRate、largestOnly、morphology；ToolResult 输出 regions、count、areas、centers、totalArea、largestArea。

### 11.3 CirclePresence

建议采用 V3 思路的混合版。

照抄：CircleCaliper 的多测量矩形采样、`MeasurePos`、`FitCircleContourXld`、采样点和拟合圆显示。

不照抄：CircularTool 的固定阈值/形态学尺寸；只有“拟合出半径就 OK”的弱判定。

需 HALCON 官方补充：区域圆的 `circularity/smallest_circle/radius range`，边缘圆的 fitError、最小有效点比例、半径范围。

推荐：V2 默认区域圆，增加圆卡尺高级模式。

下一批 V2 修改任务：`mode=region/caliper`；Region mode 做 threshold/area/radius/circularity/count；Caliper mode 做 sampleCount/threshold/polarity/select/direction/minHitRatio/fitError。

### 11.4 EdgePresence

建议参考 V3 MeasurePos 主流程。

照抄：`GenMeasureRectangle2 -> MeasurePos`、edge count 判定、测量矩形和 edge point overlay。

不照抄：polarity/select 固定 all/all、默认上下限 0/0、结果只保存 CurValue。

需 HALCON 官方补充：polarity、first/last/strongest、edge amplitude、edge distance 的完整语义。

下一批 V2 修改任务：新增 MeasurePos 型 Edge Runner；Dialog 增加 polarity、select、threshold、sigma、count range；ToolResult 保存 edgePoints/amplitudes/distances。

### 11.5 LinePresence

建议照抄 V3 Metrology Line 思路。

照抄：Metrology Line、sampleCount、threshold、polarity、select、minScore、采样矩形/采样点/拟合线 Overlay。

不照抄：空 ROI 返回成功、疑似通道转换错误、只靠是否返回 4 个参数判定 OK。

需 HALCON 官方补充：fitError、line score、sample hit count、angle/length range。

下一批 V2 修改任务：新增或替换为 MetrologyLineRunner；ToolResult 增加 endpoint、midpoint、angle、length、score、fitError、hitCount。

### 11.6 ContourPresence

不建议照抄 V3 XLD匹配作为纯轮廓有无。

照抄：如果做“轮廓模板匹配”，可参考 `CreateScaledShapeModelXld` 和模板轮廓 affine display。

不照抄：把 region boundary 当成检测图真实轮廓；把模板匹配 count 当成纯轮廓 presence。

需 HALCON 官方补充：`edges_sub_pix`、`segment_contours_xld`、`select_contours_xld`、`length_xld`、轮廓形状筛选。

下一批 V2 修改任务：先分清 `ContourPresence` 与 `ContourMatchPresence`；纯轮廓有无按 raw XLD 长度/数量/形状判定。

## 12. 如果 V3 没有契合例程怎么办

本次结论不是“V3 没有就不能做”，也不是“HALCON 有例程就忽略 V3 工程经验”。建议流程：

1. 先按 V2 当前 UI 明确工具语义。
2. 再从 V3 提取成熟工业流程。
3. 再从 HALCON 官方例程找算子原语。
4. 最后组合成 V2 Runner。
5. 不强行找“一模一样”的例程。
6. 不因为 V3 没有某条路线就否定 HALCON 官方例程。
7. 不因为 HALCON 例程没有完整工具 UI 就忽略 V3 的用户配置、判定和 Overlay 经验。

## 13. 最终结论

最值得借鉴：

- `Plugin.ImageMatching\形状匹配`：Pattern 的模板 ROI、搜索 ROI、mask、缓存模型、真实 model contour overlay、score/count 判定。
- `卡尺工具\Plugin.LineCaliper\直线卡尺`：Line 的 Metrology Line、采样点、拟合线显示。
- `卡尺工具\Plugin.CircleCaliper`：Circle 的高级边缘圆卡尺流程。
- `Plugin.EdgeDetectionTool`：Edge 的 MeasurePos 灰度跳变检测。

只作为参考：

- `Plugin.BlobTool`：连通域和真实 region overlay 好，但 OK/NG 不完整。
- `DetectionTool_Area`：面积占比判定可借鉴，但不够通用。
- `ImageNccMatching`：NCC 匹配可参考，部分参数保存但未生效。

不要直接搬：

- `CircularTool.GetCircularity()` 的硬编码形态学和面积阈值。
- `ImageMatchingXLD` 作为纯 ContourPresence。
- V3 的 WinForms 槽函数、大对象状态、全局式流程、状态混用。

对 V2 ToolCore 架构的帮助：V3 的价值主要在“成熟工具如何让用户配置、训练、保存、运行、判定和看 Overlay”，不是代码组织。V2 应保留现有架构，在 Runner 吸收 V3 算法链，在 ToolResult/Overlay 吸收 V3 的可解释显示方式。


