# 位置修正功能设计与实现记录

## 文档用途

本文档记录位置修正的产品边界、当前工程状态、目标链路、配置合同、实施阶段和验证要求。UI 细节以 `位置修正UI设计规范.md` 为准，项目级规则以根目录 `AGENTS.md` 和 `docs/FID/Function_Docs.md` 为准。

## 2026-07-22 - 基准图位姿加入链接来源

- 工具级位置修正的链接菜单新增基准图位姿来源；仅当方案级基准图位置修正已开启、
  已创建基准且位姿有效时显示。
- 基准图来源使用稳定 ID `reference.positionCorrection`，X/Y/角度可作为一组统一订阅，
  不使用显示序号做运行引用。
- 来源接口采用列表形式，能够逐项展示上层传入的多个有效基准来源；当前方案模型实际只保存
  一张基准图，多基准图的文件存储和导入槽位仍需单独扩展。
- 选择基准图来源后，“创建基准”直接导入其真实基准位姿，不再得到全 0 占位值。
- 测试运行改用当前运行帧生成运行位姿，禁止再以基准图冒充运行图；图像右上角显示订阅来源、
  基准位姿和运行位姿。
- 方案级基准位置修正运行结果补充顶层 `x/y/angle/angleDeg`，供工具级位置修正稳定订阅。

验证：`position_correction_ui_smoke`、`position_correction_engine_context_smoke`、
`color_comparison_position_correction_smoke` 通过；主工程 qmake/make 通过。

## 2026-07-22 - 下游失败原因透传

- 位置修正执行失败的结果也注册到本帧 `positionCorrectionsById`。
- 下游可区分来源缺失、模板未找到和其他来源执行失败；模板未命中使用
  `position_correction_match_not_found`。
- 位置修正已启用但尚未创建基准姿态时输出 `reference_pose_missing`，不再静默表现为来源缺失。
- 自动回归和主工程构建均通过。

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

## 2026-07-21 阶段 A/B/C 实施记录

本节记录“位置修正下一阶段实施计划”的当前落地状态。上方 2026-07-10 旧状态段落保留为历史背景；判断当前代码能力时以本节和当前源码为准。

### HALCON runtime、license 与算子核对

本机 HALCON 能力已核对，允许继续算法实现：

- `HALCONROOT=/opt/halcon`
- `libhalconc.so` 解析到 `libhalconc.so.20.11.1`
- license 文件存在且可读：`HALCON_LICENSE_FILE=/home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/license/license_support_halcon24.11_steady_2026_07.dat`
- `ldd` 需要运行环境显式带上 `/opt/halcon/lib/x64-linux`，否则 `libhalcon.so.20.11.1` 会显示 `not found`
- 已确认声明与动态符号：
  - `vector_angle_to_rigid`
  - `affine_trans_region`
  - `affine_trans_contour_xld`
  - `hom_mat2d_invert`
  - `hom_mat2d_compose`

本阶段核心变换使用 HALCON `VectorAngleToRigid`、`HomMat2dScale` 和
`HomMat2dInvert`，没有使用 OpenCV 或自写算法替代核心算法。

### 阶段 A：姿态来源与配置合同

已完成：

- 在 `src/toolcore/PositionCorrection.{h,cpp}` 中新增位置姿态字段、姿态生产者、运行姿态来源结构。
- 当前仅 `ToolType::TemplateLocation` 声明可作为位置修正姿态生产者；UI 显示字段仍为
  `x / y / angle`，尺度通过同一生产者的 `scale` 输出隐式传递，不增加绑定控件。
- 工具级位置修正使用 `runPoseSource` version 2 合同，要求 X/Y/角度来自同一个上游实例；
  `scaleKey` 缺省为 `scale`。
- 旧配置 `runPointX/runPointY/runAngle` 可迁移到 `runPoseSource`；若三项来自不同生产者，返回 `inconsistent_pose_source`，不静默兼容。
- 写回新配置时同步保留旧 `runPointX/runPointY/runAngle`，用于旧界面或旧配置读取兼容。
- `PositionCorrectionDialog` 的三项显示语义修正为“运行点 X / 运行点 Y / 运行角度”，不再误写“基准点”。
- `PositionCorrectionDialog` 绑定菜单只列出前置真实姿态生产者；不再把基准图节点或任意工具虚构为 X/Y/角度来源。
- 完成校验要求存在有效 `runPoseSource`，并且生产者仍位于当前工具之前。

### 阶段 B：运行上下文传递

已完成：

- `ToolEngine::runTools` 改为维护逐帧可变 `frameContext`。
- 每个工具运行后，结果写入 `runtimeContext.toolResultsById[toolId]`，后续工具可读取前置工具输出。
- 若工具结果标记 `positionCorrectionApplied=true`，同步注册到 `runtimeContext.positionCorrectionsById[sourceId]`。
- 该逻辑保持顺序执行语义，未引入后向引用或循环依赖。

### 阶段 C：工具级位置修正 HALCON 后端

已完成安全可执行部分：

- 新增 `src/algorithms/location/PositionCorrectionHalconRunner.{h,cpp}`。
- 新增 `src/tooladapters/PositionCorrectionAdapter.{h,cpp}`。
- `MainWindow` 注册 `PositionCorrectionAdapter`，`qt_ui_test.pro` 纳入新增源文件。
- Adapter 读取 `runPoseSource`，从 `runtimeContext.toolResultsById[producerId]` 获取同一上游实例输出的运行姿态。
- Adapter 要求 `referenceCreated=true` 且存在 `referencePose`；当前工具级位置修正不执行模板匹配。
- `referencePose.scale` 在创建基准时从同一模板定位实例自动保存；旧配置或旧结果缺少
  `scale` 时按 `1.0` 兼容。
- Runner 先使用 HALCON `VectorAngleToRigid(referencePose, runPose)` 计算刚性位姿，
  再以运行锚点为固定点调用 `HomMat2dScale` 应用
  `scaleRatio = runScale / referenceScale`，最后用 `HomMat2dInvert` 生成逆矩阵，输出：
  - `referenceToRunHomMat2D`
  - `runToReferenceHomMat2D`
  - `deltaX`
  - `deltaY`
  - `deltaAngleDeg`
  - `referenceScale`
  - `runScale`
  - `scaleRatio`
  - `referencePose`
  - `runPose`
- `referenceToRunHomMat2D` 是下游 ROI 从基准图移动到运行图时使用的矩阵；反向矩阵仅用于诊断或明确需要反向时使用。
- HALCON `HomMat2D` 六元组保持 HALCON native row/col 语义。对 region/contour 变换应直接交给 HALCON `affine_trans_region` / `affine_trans_contour_xld`，不要手动按 X/Y 猜测索引。

错误状态：

| 状态 | 含义 |
| --- | --- |
| `inconsistent_pose_source` | 旧三字段迁移发现 X/Y/角度来源不一致 |
| `incomplete_input_binding` | 运行姿态来源字段不完整 |
| `reference_pose_missing` | 尚未创建基准或缺少 `referencePose` |
| `source_unavailable` | 上游实例结果不存在 |
| `source_invalid` | 上游实例本帧失败或 NG |
| `input_binding_not_found` | 上游结果缺少绑定字段 |
| `invalid_pose` | 基准姿态或运行姿态存在非有限数值 |
| `invalid_pose_scale` | 基准或运行尺度不是有限正数 |
| `halcon_error` | HALCON 抛出异常 |
| `execution_error` | 非 HALCON 运行异常 |

### 验证记录

全部验证均使用 `/tmp` 影子目录构建，未要求提交构建产物：

- `position_correction_smoke`: passed
- `position_correction_ui_smoke`: passed
- `position_correction_engine_context_smoke`: passed
- `position_correction_backend_smoke`: passed
- 主工程 `qmake qt_ui_test.pro BUILD_ROOT=/tmp/.../out && make -j$(nproc)`: passed

运行 HALCON 相关 smoke 和主程序时，应确保运行环境包含：

```bash
export LD_LIBRARY_PATH=/opt/halcon/lib/x64-linux:${OPENCV_ROOT}/lib:${QT_ROOT}/lib:${LD_LIBRARY_PATH:-}
```

### 仍未完成

- 基准图方案级位置修正的私有模板定位器尚未接入本轮后端。
- 工具级位置修正 Dialog 的“创建基准”已在后续记录中接入；本小节原状态保留为阶段 C 当时边界。
- 下游视觉工具尚未在执行核心检测前统一应用 `referenceToRunHomMat2D` 修正 ROI。
- 尚未接入 `affine_trans_region` / `affine_trans_contour_xld` 到具体下游 ROI 消费链路。
- 触发、曝光、增益、像素格式一致性约束已确认暂不纳入本阶段判断。

## 2026-07-21 续：工具级创建基准与测试运行

在上一节工具级位置修正 HALCON 后端基础上，继续完成 `PositionCorrectionDialog` 的可操作闭环。

### 已完成

- `PositionCorrectionDialog` 的“创建基准”按钮已接入真实执行逻辑：
  - 读取当前 `runPoseSource`；
  - 查找同一个前置 `TemplateLocation` 实例；
  - 使用 `ReferenceImageProvider` 当前基准图作为测试图和参考图；
  - 调用 `TemplateLocationAdapter` 在基准图上执行一次同一上游实例；
  - 只有上游定位 `success && ok` 且 payload 中存在绑定的 X/Y/角度字段时，才写入：
    - `referenceCreated=true`
    - `referencePose`
    - `referencePoseSourceId`
    - `referencePoseSource`
    - `referencePoseStatus`
    - `referencePoseScore`
    - `referencePoseElapsedMs`
  - 失败时清除 `referencePose` 并保持 `referenceCreated=false`。
- `PositionCorrectionDialog` 的“测试运行”按钮已接入真实工具级测试：
  - 要求已创建 `referencePose`；
  - 先在基准图上执行同一上游 `TemplateLocation` 取得运行姿态；
  - 再调用 `PositionCorrectionAdapter` 计算 `referenceToRunHomMat2D`、`runToReferenceHomMat2D` 和 delta；
  - 成功时只显示位置修正测试结果，不伪造下游 ROI 修正结果。
- `toolConfig().summary` 已由“后端尚未实现”改为基于 `referenceCreated` 的真实状态。
- `position_correction_ui_smoke.pro` 已补齐 `TemplateLocationAdapter`、`PositionCorrectionAdapter` 和两个 HALCON runner 的链接依赖。

### 验证记录

全部验证继续使用 `/tmp` 影子目录：

- `position_correction_smoke`: passed
- `position_correction_ui_smoke`: passed
- `position_correction_engine_context_smoke`: passed
- `position_correction_backend_smoke`: passed
- 主工程 `qmake qt_ui_test.pro BUILD_ROOT=/tmp/.../out && make -j$(nproc)`: passed

### 当前仍不可声明完成的范围

- 基准图方案级位置修正仍未实现私有模板定位器；基准图页面的位置修正不能作为完整产品功能测试。
- 下游视觉工具尚未实际应用 `referenceToRunHomMat2D` 修正 ROI。
- 当前工具级“测试运行”使用基准图作为测试帧验证链路；真实相机运行帧下的 UI 联调仍需结合主运行链路与下游 ROI 消费阶段完成。

## 2026-07-21 续：基准图私有模板建模与自匹配

基准图方案级位置修正已接入私有 HALCON 模板定位器的创建与自匹配验证。该能力只负责让基准图位置修正节点变为“可用来源”，下游 ROI 实际应用仍在后续阶段。

### 已完成

- `ReferencePositionCorrectionConfig` 升级到 version 2，并新增持久字段：
  - `referenceCreated`
  - `referencePose`
  - `modelCacheKey`
  - `status`
  - `message`
  - `score`
  - `elapsedMs`
- `ReferenceImageDialog` 持有私有 `TemplateLocationHalconRunner`。
- 基准图位置修正 ROI 发生变化时，立即将旧模型状态失效：
  - `referenceCreated=false`
  - 清空 `referencePose`
  - `status=editing` 或 `pending_validation`
- 点击基准图位置修正 ROI 的“完成”后，会立即执行：
  - 校验基准图存在；
  - 校验矩形或多边形模板 ROI 有效；
  - 构造私有 `TemplateLocationHalconConfig`；
  - 使用当前基准图作为 reference 和 run image 调用 HALCON shape model；
  - 自匹配 `success && ok` 且 payload 含有限的 `x/y/angleDeg` 后，才写入 `referenceCreated=true` 和 `referencePose`。
- 抓取或导入新的基准图后，旧基准图位置修正模型会失效，要求重新完成 ROI。
- 基准图位置修正“测试运行”按钮现在显示私有模型自匹配状态，不再显示“后端尚未实现”。

当前默认参数：

```text
modelCacheKey = reference.positionCorrection.private_template
searchRegionType = full
searchRoiNormalized = full image
minScore = 50
angleStart = -45
angleExtent = 90
scaleMin = 100
scaleMax = 100
maxMatches = 1
minMatchCount = 1
maxMatchCount = 1
originMode = centroid
timeoutMs = 2000
```

### 验证记录

全部验证使用 `/tmp` 影子目录：

- `position_correction_smoke`: passed
- `position_correction_ui_smoke`: passed
- `position_correction_engine_context_smoke`: passed
- `position_correction_backend_smoke`: passed
- `reference_position_correction_private_model_smoke`: passed
- 主工程 `qmake qt_ui_test.pro BUILD_ROOT=/tmp/.../out && make -j$(nproc)`: passed

### 仍未完成

- 下游视觉工具尚未读取基准图位置修正源 `reference.positionCorrection` 并应用 `referenceToRunHomMat2D`。
- 基准图位置修正在运行图上的实时匹配和变换结果注册已在后续记录中接入；本条保留为本节当时边界。
- `affine_trans_region` / `affine_trans_contour_xld` 尚未接到下游 ROI 消费链路。
- 基准图私有模板定位器参数目前使用保守默认值，尚未暴露高级 UI 参数。

## 2026-07-21 续：基准图位置修正运行期注册

已将方案级基准图位置修正接入主运行链路的前置阶段。该阶段只负责生成和注册修正结果，不直接修改下游工具 ROI。

### 已完成

- `MainWindow` 在构造 `runtimeContext` 时写入当前方案的 `referencePositionCorrection` JSON。
- `ToolEngine::runTools()` 在执行工具列表前，会检查 `runtimeContext.referencePositionCorrection`：
  - 未启用或 `referenceCreated=false` 时跳过；
  - 启用且已有 `referencePose` 时，先用私有 `TemplateLocationHalconRunner` 在运行图上定位；
  - 再用 `PositionCorrectionHalconRunner` 计算 `referenceToRunHomMat2D` 和 `runToReferenceHomMat2D`；
  - 成功后注册到 `positionCorrectionsById[reference.positionCorrection]`；
  - 同时写入 `toolResultsById[reference.positionCorrection]`，便于诊断和后续工具读取。
- 基准图位置修正运行结果 payload 标记：
  - `sourceKind=reference`
  - `scope=global`
  - `sourceId=reference.positionCorrection`
  - `positionCorrectionApplied=true`

### 验证记录

全部验证继续使用 `/tmp` 影子目录：

- `position_correction_smoke`: passed
- `position_correction_ui_smoke`: passed
- `position_correction_engine_context_smoke`: passed
- `position_correction_backend_smoke`: passed
- `reference_position_correction_private_model_smoke`: passed
- 主工程 `qmake qt_ui_test.pro BUILD_ROOT=/tmp/.../out && make -j$(nproc)`: passed

### 当前仍未完成

- 除颜色比较外，其他下游视觉工具尚未实际消费 `positionCorrectionsById` 并用 HALCON `referenceToRunHomMat2D` 修正 ROI。
- 工具级局部位置修正和基准图全局位置修正的下游单来源选择，目前只落到颜色比较 Adapter。
- `affine_trans_region` 已接入颜色比较有效检测区域；其他具体 ROI 消费链路仍待实现。

## 2026-07-21：颜色比较首个下游消费工具

### 已完成

- `ColorComparisonAdapter` 在位置修正开启时，只读取当前工具选中的一个稳定来源 ID，并从本帧 `runtimeContext.positionCorrectionsById[sourceId]` 取结果。
- 来源结果必须同时满足 `success=true`、`ok=true`、`positionCorrectionApplied=true`，且 payload 的 `sourceId` 与选择值一致。
- 只读取 `referenceToRunHomMat2D`；没有读取或回退到 `runToReferenceHomMat2D`。
- 来源不存在返回 `position_correction_source_missing`，来源本帧失败或合同不成立返回 `source_invalid`，矩阵不是 6 个有限值时返回 `invalid_position_correction_matrix`。
- `ColorComparisonHalconRunner` 在本次运行中先按图像尺寸生成基准检测区域及屏蔽区的有效区域，再使用 HALCON `affine_trans_region` 应用绝对矩阵，最后用 HALCON `clip_region` 裁剪到当前图像范围。
- 修正后的临时区域只供本帧颜色直方图提取使用，不写回 `detectRoiNormalized`、圆形区域或屏蔽区配置。
- 检测 overlay 同步显示修正后的矩形多边形、圆心和屏蔽区，并记录实际来源 ID。
- 颜色比较 Dialog 的位置修正控件已解除禁用，保存稳定来源 ID；旧版把基准图显示文本写进 `sourceId` 的配置会迁移到 `reference.positionCorrection`。

### 模板边界说明

- 工具级位置修正不拥有也不执行内部模板匹配。
- 基准图位置修正继续保留私有 `TemplateLocationHalconRunner` 和独立 `modelCacheKey`，这是它生成运行位姿所必需的能力。
- 普通模板定位工具仍使用自己的工具实例与缓存键；三者不共享模型。
- 颜色比较的模板是颜色直方图样本模型，不是位置模板，不参与位置修正定位，也不会与上述模板缓存冲突。

### 验证记录

- `/tmp/color-pc-smoke.783srr`：`color_comparison_position_correction_smoke` passed；覆盖 X 正向位移、反向矩阵不命中、HALCON 区域变换、非法矩阵和来源缺失失败。
- `/tmp/color-pc-ui.*`：`color_comparison_dialog_integration_smoke` 在影子布局中 passed。
- `/tmp/color-pc-main.yAskT0`：主工程 qmake/make passed。
- `git diff --check`：本次相关文件 passed。

### 后续范围

- 本阶段只接入颜色比较；其他下游视觉工具仍未消费位置修正结果。
- 颜色比较 Dialog 内的单工具预览不具备整条前序工具执行上下文；正式位置修正联调应通过 `ToolEngine` 顺序运行方案。

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
- 基准图页已接入矩形/多边形 ROI 绘制、归一化配置保存和重新打开回显；独立工具实例 ROI、HALCON 匹配和下游 ROI 变换仍未实现，不得显示伪造成功结果。

### 2026-07-20 - 基准图模板 ROI 完善

- 基准图位置修正的矩形按钮接入拖拽绘制，矩形宽高不足 2 像素时明确拒绝。
- 多边形按钮接入逐点绘制，支持靠近首点或双击闭合、右键撤销和 Esc 取消，少于 3 点时禁止完成。
- 矩形和多边形编辑互斥，切换当前图像或关闭位置修正时退出编辑并清理预览 ROI。
- ROI 使用归一化坐标写入 `referencePositionCorrection`；保存、另存为及重新打开均保持类型和区域。
- 测试运行在无基准图或无模板区域时给出前置提示；配置有效时仍返回“位置修正后端尚未实现”，不伪造 OK/NG。

### 2026-07-20 - 模板定位完成后的下一阶段架构确认

- 模板定位已成为真实位姿生产者；位置修正后端应直接消费其 `x/y/angle` 主位姿，不再把所有前置工具虚构成定位来源。
- 工具位置修正收敛为纯变换节点：创建基准时在基准图上执行绑定的定位生产者并保存基准姿态，每帧读取同一生产者的运行姿态；不再自行绘制模板 ROI 或执行模板匹配。
- 基准图位置修正保留方案级固定节点身份，内部使用独立配置和缓存的私有模板定位器完成匹配，再复用公共位置变换能力输出与工具位置修正相同的结果合同。
- 基准图位置修正只作为下游修正来源，不作为工具位置修正的 X/Y/角度来源。
- 下游 ROI 使用从基准坐标到运行坐标的 `referenceToRunHomMat2D`；反向 `runToReferenceHomMat2D` 单独输出，禁止用反向矩阵移动检测 ROI。
- 基准图位置修正建立产品级全局坐标系，工具位置修正建立局部对象坐标系；只有启用并选择对应稳定来源 ID 的消费工具才应用该矩阵。
- 全局与局部可同时存在。局部定位工具可以先使用全局矩阵修正搜索 ROI，但工具位置修正根据局部定位实际位姿生成的是最终绝对矩阵；消费局部修正时不得再次叠加全局矩阵造成双重补偿。
- 海康官方语义核对后确认：工具 Dialog 订阅的是运行姿态 X/Y/角度；创建基准时在基准图上执行同一来源，将当次姿态保存为 `referencePose`，后续帧输出为 `runPose`。内部需要两组姿态，但 UI 不再单独绑定一套基准字段。
- 当前占位代码的 `runPointX/runPointY/runAngle` 键名与“基准点坐标/基准角度”默认显示文本冲突；正式接入时迁移为 `runPoseSource` 并统一显示为运行点语义。
- 配置迁移、运行上下文、分阶段实现和验证清单见 `位置修正下一阶段实施计划.md`。
