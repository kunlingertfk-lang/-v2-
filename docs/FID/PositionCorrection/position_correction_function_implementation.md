# 位置修正功能设计与实现记录

## 文档用途

本文档记录位置修正的产品边界、当前工程状态、目标链路、配置合同、实施阶段和验证要求。UI 细节以 `位置修正UI设计规范.md` 为准，项目级规则以根目录 `AGENTS.md` 和 `docs/FID/Function_Docs.md` 为准。

## 功能目标

位置修正用于根据基准姿态与运行姿态生成统一的平移、旋转修正信息，供后续视觉工具修正检测区域。功能必须支持：

- 基准图页面的一份方案级位置修正配置；
- 工具栏中可创建多个独立位置修正实例；
- 下游视觉工具指定基准图或某个前置位置修正实例；
- 保存、重新打开、复制、删除、重排后的稳定引用；
- 未配置、来源失效、后端未实现和 HALCON 异常时返回明确状态。

## 参考资料

- UI 规范：`docs/FID/PositionCorrection/位置修正UI设计规范.md`
- 截图素材清单：`docs/FID/PositionCorrection/screenshots/README.md`
- 公共位置修正规则：`docs/FID/Function_Docs.md`
- 公共字段占位：`src/toolcore/PositionCorrection.h`、`src/toolcore/PositionCorrection.cpp`
- 基准图页面：`src/ReferenceImageDialog.cpp`、`ui/ReferenceImageDialog.ui`
- 工具库与工具页：`src/ToolLibraryDialog.cpp`、`src/ToolsDialog.cpp`

## 当前工程状态（2026-07-10）

### 已存在

- `ui/ReferenceImageDialog.ui` 已有位置修正说明卡、开关和示意区，但没有完整模板区域、配置保存和测试闭环。
- `src/toolcore/PositionCorrection.{h,cpp}` 已统一解析和写回 `enablePositionCorrection`、`positionCorrectionSource`。
- 公共 helper 会输出 `positionCorrectionApplied=false`、`positionCorrectionReason=not implemented`。
- `smoke/position_correction_smoke.cpp` 已覆盖现有字段解析、写回和未应用 payload。
- 多个已实现视觉工具已经展示独立位置修正开关和来源文本，但来源大多是静态默认项。
- 工具库已有“定位工具”分类以及模板、边缘、圆定位占位入口。

### 尚未实现

- 没有 `ToolType::PositionCorrection`。
- 没有独立 `PositionCorrectionDialog`、Adapter 或 HALCON Runner。
- 基准图位置修正没有进入 `SchemeState` 持久化。
- 工具页不能创建多个位置修正实例。
- 消费工具没有稳定的 `positionCorrectionSourceId`。
- 没有统一的上游输出描述和字段绑定菜单。
- 没有真实模板匹配、姿态计算、ROI 修正或运行结果。

因此当前任何 `positionCorrectionApplied=true` 都是不符合事实的。真实后端接入前必须继续返回 `not implemented`。

## 已确认架构

### 节点关系

```text
方案级基准图位置修正
  id = reference.positionCorrection
               |
               +---------------------------+
               |                           |
               v                           v
工具位置修正实例 A                  工具位置修正实例 B
  id = stable toolId                  id = stable toolId
               |                           |
               +-------------+-------------+
                             v
                    后续视觉工具消费修正信息
```

基准图位置修正与工具实例互不覆盖。工具实例可以有多个，所有节点引用使用稳定 ID，界面序号只用于显示。

### 目标工具链

```text
ToolLibraryDialog / ToolsDialog
        |
        v
PositionCorrectionDialog
        |
        v
ToolConfig.params.positionCorrection
        |
        v
PositionCorrectionAdapter
        |
        v
PositionCorrectionHalconRunner
        |
        v
ToolResult / overlays / payload
```

基准图入口不创建 `ToolConfig`，其配置由 `SchemeState.referencePositionCorrection` 独立保存。算法阶段可与工具实例共享配置结构、HALCON runner 能力和输出结构，但 UI 组合保持独立。

## 配置与引用设计

### 基准图位置修正

目标方案字段：

```json
{
  "referencePositionCorrection": {
    "version": 1,
    "enabled": false,
    "templateRegionType": "rectangle",
    "templateRoiNormalized": {"x": 0.0, "y": 0.0, "width": 0.0, "height": 0.0},
    "templatePolygonNormalized": []
  }
}
```

兼容规则：旧方案缺少该对象时按关闭处理，不改变已有基准图路径、工具配置或输出配置。

### 工具位置修正实例

目标工具类型和分类：

```text
toolType = PositionCorrection
category = Location
```

实例配置保存到 `ToolConfig.params.positionCorrection`：

```text
version
runPointX: producerId + outputKey + displayPath
runPointY: producerId + outputKey + displayPath
runAngle: producerId + outputKey + displayPath
templateRegionType
templateRoiNormalized
templatePolygonNormalized
referenceCreated
```

每个实例拥有独立模板区域和绑定，不读取或覆盖基准图位置修正的模板配置。

### 消费工具来源

消费工具继续保留现有字段，并增加稳定 ID：

```text
enablePositionCorrection: bool
positionCorrectionSourceId: string
positionCorrectionSource: string
```

- `positionCorrectionSourceId`：运行和引用校验使用的稳定键。
- `positionCorrectionSource`：界面显示和旧配置兼容文本。
- 基准图固定 ID：`reference.positionCorrection`。

旧文本迁移只能在唯一匹配时进行；无法唯一匹配时进入失效状态，不允许静默回退。

## 执行顺序和依赖规则

- 基准图节点视为所有工具之前的固定节点。
- 位置修正工具的三项运行值只能绑定当前实例之前的有效输出。
- 消费工具只能选择它之前的位置修正节点。
- 工具复制生成新 `toolId`；已有消费关系仍指向原实例。
- 工具重排后，若来源仍位于消费工具之前，引用保持有效。
- 来源被移动到消费工具之后时，稳定 ID 保留，但引用状态变为无效。
- 来源删除或禁用时不得自动切换到基准图。

## UI 第一阶段实施边界

UI 第一阶段应完成：

- 基准图位置修正关闭态、开启态、模板 ROI 和草稿保存回显。
- 工具库位置修正入口。
- 多实例创建、编辑、复制、删除和工具列表展示。
- 独立位置修正 Dialog 的字段绑定、模板 ROI、校验和保存回显。
- 消费工具动态来源选择、稳定 ID 保存和失效态提示。
- 后端未实现时统一展示 `not implemented`。

UI 第一阶段不得：

- 新增非 HALCON 核心算法。
- 伪造创建基准成功、匹配分数、修正矩阵或 OK/NG。
- 用显示序号代替稳定 ID。
- 在来源删除后静默写回默认值。
- 为截图中的多基准图缩略区扩展方案数据模型。

## 后续 HALCON 阶段边界

真实算法实施前必须先确认本机 HALCON runtime、license 和所需算子/符号。候选能力包括形状模板创建、模板查找、二维刚性变换和区域仿射变换；实际接口必须在算法专项设计中通过本机环境确认后锁定。

若所需 HALCON 能力或符号不可用，应停止算法实现并报告阻塞，不允许使用 OpenCV、自写匹配或第三方库替代。

## 目标结果合同

未来真实 `ToolResult.payload` 至少应区分请求、来源、应用状态和修正量：

```text
positionCorrectionSourceId
positionCorrectionSource
positionCorrectionApplied
positionCorrectionReason
referenceX
referenceY
referenceAngle
runX
runY
runAngle
deltaX
deltaY
deltaAngle
elapsedMs
```

UI 第一阶段只允许输出：

```text
positionCorrectionApplied = false
positionCorrectionReason = "not implemented"
```

## 错误和状态设计

| 场景 | 目标状态 |
| --- | --- |
| 基准图为空 | `no_reference_image` |
| 模板区域为空 | `no_template_region` |
| 模板区域无效 | `invalid_template_region` |
| X/Y/角度绑定不完整 | `incomplete_input_binding` |
| 上游输出不存在 | `input_binding_not_found` |
| 位置修正来源被删除 | `position_correction_source_missing` |
| 来源禁用 | `position_correction_source_disabled` |
| 来源位于消费工具之后 | `position_correction_source_not_preceding` |
| 后端尚未接入 | `not implemented` |
| HALCON runtime 或符号异常 | 使用项目统一 HALCON 错误状态 |

## 分阶段验证

### 文档阶段

- 截图编号、文件名和场景一一对应。
- UI 规范、本文档和公共规范术语一致。
- 不把未实现能力写成已完成。
- Markdown 相对路径有效。
- `git diff --check` 通过。

### UI 第一阶段

- 基准图配置与工具实例配置互不覆盖。
- 可以创建并回显多个位置修正实例。
- 下游来源菜单只显示基准图和合法前置实例。
- 插入、删除、复制、禁用、重排后稳定 ID 行为符合设计。
- 无基准图、无模板、绑定缺失、来源失效时不崩溃。
- 后端未实现时不出现伪造 OK/NG。
- `qmake qt_ui_test.pro` 和 `make` 通过。

### 后端阶段

- 补充独立 smoke，覆盖正常修正、空图、无模板、无效绑定和 HALCON 异常。
- 验证 runner 输出修正信息和 overlay。
- 验证消费工具按修正结果移动/旋转检测 ROI。

## 实现记录

### 2026-07-10 - UI 规范和功能设计建立

- 确认正式名称为“位置修正”。
- 确认基准图配置独立，工具栏允许创建多个实例。
- 确认其他视觉工具可以指定基准图或前置位置修正实例。
- 定义稳定来源 ID、动态显示序号、失效来源和禁止静默回退规则。
- 建立七张原始截图的固定文件名和对照场景。
- 本次只修改文档，没有新增 Qt UI、配置迁移或算法实现。

### 2026-07-11 - UI 第一阶段代码接入

- 新增 `ToolType::PositionCorrection` 及字符串映射。
- 公共位置修正配置新增稳定 `positionCorrectionSourceId`，并保持旧两参数初始化兼容。
- 新增前置位置修正来源过滤和方案级基准图配置 JSON 编解码。
- `SchemeState` 新增 `referencePositionCorrection`，旧方案缺失时默认关闭。
- 基准图页面位置修正卡接入开关持久化、开启态模板设置和未实现状态。
- 新增 `PositionCorrectionDialog`，支持三项绑定回显、模板类型、保存回显和明确 `not implemented`。
- 工具库“定位工具”增加位置修正入口，工具页支持创建、编辑和展示多个独立实例。
- 其他工具的既有位置修正下拉框在打开时按基准图和前置已启用位置修正实例动态填充，并保存稳定来源 ID；失效来源保留并显示错误项。
- 新增 `position_correction_ui_smoke`，覆盖 Dialog 配置回显和关键控件。
- 真实 ROI 绘制、上游输出二级菜单、HALCON 匹配和下游 ROI 变换仍未实现，不得显示伪造成功结果。
