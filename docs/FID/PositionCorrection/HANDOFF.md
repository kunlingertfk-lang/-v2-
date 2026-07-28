# 位置修正功能交接

## 当前事实（2026-07-23，优先于全文历史记录）

- 方案级基准图位置修正固定 ID 为 `reference.positionCorrection`，界面显示编号固定为
  `0 基准图`；编号只用于显示，运行引用始终使用稳定 ID。
- 工具级位置修正允许订阅两类运行姿态来源：合法前置 `TemplateLocation`，以及已开启、
  已创建的方案级基准图位置修正。选择后者时可直接导入其基准位姿，运行期订阅同一来源的
  `x/y/angle/scale`。
- 工具级位置修正是纯姿态变换节点，不拥有模板 ROI，不执行模板匹配；旧版
  `templateRegionType/templateRoiNormalized/templatePolygonNormalized` 仅作为迁移输入，
  下次保存时删除。
- `ToolEngine` 每帧重新建立 `toolResultsById` 与 `positionCorrectionsById`，拒绝调用方注入
  的旧动态结果；消费者同时校验来源 ID、类型和 `frameId`，禁止跨帧沿用矩阵。
- 方案级和工具级位置修正对 X/Y/角度/尺度执行严格有限数值校验，非法值明确失败，不再归零。
- 当前已经完整消费位置修正矩阵的下游工具为颜色比较和 Blob 有无；其他工具的 UI 占位或
  未应用 payload 不代表已经接入，本轮不扩展其他工具链。
- 当前仍只有单基准图数据字段；多基准图文件、缩略槽位和持久化数组尚未实现。
- 当前事实以本节和当前代码为准。下文按日期保留实施过程；其中“尚未实现”“唯一消费者”
  “基准图不能作为工具级来源”等表述只代表当时状态，不得再作为当前结论。

> 新会话第一句话：`先读：《docs/FID/PositionCorrection/HANDOFF.md》`

## 历史记录

### 2026-07-22 基准图位姿订阅补充

- `PositionCorrectionDialog` 的运行位姿链接菜单支持上层传入的基准位姿来源列表。
- `ToolsDialog` 在方案级基准位置修正“开启 + 已创建”时加入
  `reference.positionCorrection`；关闭或尚未创建时不显示。
- 选择该来源后可直接导入其基准位姿，并在运行期订阅同一来源的 `x/y/angle`。
- 测试运行改用当前相机帧作为运行图；没有当前帧时明确失败。

### 0. 2026-07-20 当时的架构决策

模板定位现已完成独立 Dialog、Adapter、HALCON Runner、持久模型、多匹配和 smoke；本文后续“模板定位仍是占位”的历史段落不再代表当前状态。

下一阶段的详细规划见：`docs/FID/PositionCorrection/位置修正下一阶段实施计划.md`。该计划优先于本文早期关于节点关系和实施顺序的描述，核心结论是：

- 工具位置修正只消费前置定位工具的姿态并计算变换，自身不做模板匹配，也不再拥有模板 ROI。
- 当时方案把基准图位置修正限定为下游修正来源；该限制已在 2026-07-22 取消，当前允许工具级
  位置修正订阅其 X/Y/角度/尺度。
- 两类位置修正共享变换结果合同，但配置、模型、基准姿态和稳定 ID 完全独立。
- 下游基准 ROI 使用 `referenceToRunHomMat2D = vector_angle_to_rigid(referencePose, runPose)` 移动到运行图；反向矩阵必须另名输出，不能混用。
- 基准图位置修正代表产品级全局坐标系，工具位置修正代表局部对象坐标系。两者可同时存在，但局部位置修正输出最终绝对矩阵；消费局部源时不得再重复叠加全局矩阵。
- 已确认工具 Dialog 的 X/Y/角度是运行姿态来源；创建基准时在基准图上执行同一来源并冻结为基准姿态。现有代码键名为 `runPoint*`、默认显示却写“基准点”，后续必须修正文案并迁移为 `runPoseSource`。

## 0.1 2026-07-21 最新实现状态

本轮已按 `位置修正下一阶段实施计划.md` 完成阶段 A、阶段 B，以及阶段 C 中“工具级位置修正运行后端”的安全可执行部分。

已完成：

- HALCON runtime、license、头文件声明和动态符号已核对；`vector_angle_to_rigid`、`affine_trans_region`、`affine_trans_contour_xld`、`hom_mat2d_invert`、`hom_mat2d_compose` 可用。
- `src/toolcore/PositionCorrection.{h,cpp}` 已锁定姿态来源合同：
  - 新配置为 version 2 `runPoseSource`；
  - X/Y/角度必须来自同一个上游实例；
  - 当前只允许前置 `TemplateLocation` 作为姿态生产者；
  - 旧 `runPointX/runPointY/runAngle` 可迁移，混合来源返回 `inconsistent_pose_source`。
- `src/PositionCorrectionDialog.{h,cpp}` 已修正来源语义与绑定模型：
  - 三项显示为“运行点 X / 运行点 Y / 运行角度”；
  - 菜单只列出真实前置姿态生产者；
  - 不再把基准图或任意工具虚构为运行姿态来源；
  - 完成校验要求有效前置 `runPoseSource`。
- `src/toolcore/ToolEngine.cpp` 已维护逐帧 `runtimeContext.toolResultsById` 和 `runtimeContext.positionCorrectionsById`，保证后续工具能读取前置结果。
- 新增 `src/algorithms/location/PositionCorrectionHalconRunner.{h,cpp}`：
  - 使用 HALCON `VectorAngleToRigid(referencePose, runPose)`；
  - 输出 `referenceToRunHomMat2D` 和 `runToReferenceHomMat2D`；
  - 保持 HALCON native row/col 六元组语义。
- 新增 `src/tooladapters/PositionCorrectionAdapter.{h,cpp}`：
  - 工具级位置修正不做模板匹配；
  - 从同一上游 TemplateLocation 结果读取运行姿态；
  - 读取已保存的 `referencePose`；
  - 输出本地绝对修正结果，不叠加全局矩阵。
- `MainWindow` 已注册 `PositionCorrectionAdapter`，`qt_ui_test.pro` 已纳入新增文件。
- 新增并通过 smoke：
  - `position_correction_engine_context_smoke`
  - `position_correction_backend_smoke`

验证结果：

```text
position_correction_smoke: all checks passed
position_correction_ui_smoke: all checks passed
position_correction_engine_context_smoke: all checks passed
position_correction_backend_smoke: all checks passed
主工程 qmake + make: passed
```

验证均使用 `/tmp` 影子目录。HALCON 相关可执行文件运行时需要带上：

```bash
export LD_LIBRARY_PATH=/opt/halcon/lib/x64-linux:${OPENCV_ROOT}/lib:${QT_ROOT}/lib:${LD_LIBRARY_PATH:-}
```

本轮仍未完成：

- 工具级 Dialog 的“创建基准”在 0.2 节已接入；本处保留为 0.1 当时边界。
- 基准图方案级位置修正的私有模板定位器尚未接入。
- 下游视觉工具尚未实际用 `referenceToRunHomMat2D` 修正 ROI。
- `affine_trans_region` / `affine_trans_contour_xld` 尚未接到具体 ROI 消费链路。
- 触发、曝光、增益、像素格式一致性约束按用户确认暂不纳入本阶段判断。

## 0.2 2026-07-21 续：工具级创建基准已接入

本轮继续推进工具级位置修正，已完成 Dialog 内可执行闭环：

- “创建基准”按钮现在会读取当前 `runPoseSource`，找到同一个前置 `TemplateLocation` 实例，并在当前基准图上执行一次该上游实例。
- 只有上游模板定位 `success && ok`，且结果 payload 中存在绑定的 X/Y/角度字段时，才保存：
  - `referenceCreated=true`
  - `referencePose`
  - `referencePoseSourceId`
  - `referencePoseSource`
  - `referencePoseStatus`
  - `referencePoseScore`
  - `referencePoseElapsedMs`
- 创建基准失败时会清除 `referencePose`，并保持 `referenceCreated=false`。
- “测试运行”按钮现在会先在基准图上执行同一上游 `TemplateLocation`，再调用 `PositionCorrectionAdapter` 计算 `referenceToRunHomMat2D`、`runToReferenceHomMat2D` 和 delta。
- 该测试运行只验证工具级位置修正链路，不代表下游工具 ROI 已经被修正。

验证结果：

```text
position_correction_smoke: all checks passed
position_correction_ui_smoke: all checks passed
position_correction_engine_context_smoke: all checks passed
position_correction_backend_smoke: all checks passed
主工程 qmake + make: passed
```

0.2 当时仍未完成：

- 基准图方案级位置修正的私有模板定位器尚未接入；基准图页面位置修正仍不能作为完整产品功能测试。
- 下游视觉工具尚未实际使用 `referenceToRunHomMat2D` 修正 ROI。
- `affine_trans_region` / `affine_trans_contour_xld` 尚未接到具体 ROI 消费链路。

## 0.3 2026-07-21 续：基准图私有模板定位器已接入建模自匹配

本轮继续推进基准图方案级位置修正，已完成“ROI 完成时立即建模并自匹配验证”的可用性闭环。

已完成：

- `ReferencePositionCorrectionConfig` 升级到 version 2，新增：
  - `referenceCreated`
  - `referencePose`
  - `modelCacheKey`
  - `status`
  - `message`
  - `score`
  - `elapsedMs`
- `ReferenceImageDialog` 内部持有私有 `TemplateLocationHalconRunner`，不依赖工具链中的模板定位实例。
- 基准图位置修正 ROI 点击“完成”后，会立即使用当前基准图执行 HALCON shape model 建模和自匹配。
- 只有自匹配 `success && ok` 且输出 `x/y/angleDeg` 均为有限值时，才写入 `referenceCreated=true` 和 `referencePose`。
- ROI 编辑、抓取新基准图、导入新基准图都会使旧 `referencePose` 失效。
- 基准图位置修正“测试运行”按钮现在显示私有模型可用状态，不再显示“后端尚未实现”。

默认私有定位参数：

```text
modelCacheKey = reference.positionCorrection.private_template
searchRegionType = full
minScore = 50
angleStart = -45
angleExtent = 90
scaleMin = 100
scaleMax = 100
maxMatches = 1
originMode = centroid
timeoutMs = 2000
```

新增验证：

```text
reference_position_correction_private_model_smoke: all checks passed
```

0.3 当时仍未完成：

- 基准图位置修正在运行图上的实时匹配、矩阵计算与 `positionCorrectionsById[reference.positionCorrection]` 注册尚未接入主运行链路。
- 下游视觉工具仍未实际使用 `referenceToRunHomMat2D` 修正 ROI。
- `affine_trans_region` / `affine_trans_contour_xld` 尚未接到具体下游 ROI 消费链路。
- 基准图私有模板定位器高级参数尚未暴露到 UI。

## 0.4 2026-07-21 续：基准图位置修正运行期注册已接入

本轮继续把方案级基准图位置修正接入主运行链路的前置阶段。

已完成：

- `MainWindow` 会把当前方案 `referencePositionCorrection` 写入每帧 `runtimeContext`。
- `ToolEngine::runTools()` 在真实工具列表执行前检查该配置：
  - 未启用或 `referenceCreated=false` 时跳过；
  - 启用且有 `referencePose` 时，用私有 `TemplateLocationHalconRunner` 在运行图上定位；
  - 再用 `PositionCorrectionHalconRunner` 计算 `referenceToRunHomMat2D` 和 `runToReferenceHomMat2D`；
  - 成功后注册到 `positionCorrectionsById[reference.positionCorrection]`；
  - 同时写入 `toolResultsById[reference.positionCorrection]` 便于诊断。
- 运行结果 payload 标记：
  - `sourceKind=reference`
  - `scope=global`
  - `sourceId=reference.positionCorrection`
  - `positionCorrectionApplied=true`

验证结果仍为：

```text
position_correction_smoke: all checks passed
position_correction_ui_smoke: all checks passed
position_correction_engine_context_smoke: all checks passed
position_correction_backend_smoke: all checks passed
reference_position_correction_private_model_smoke: all checks passed
主工程 qmake + make: passed
```

当前最新仍未完成：

- 下游视觉工具尚未实际消费 `positionCorrectionsById`，也尚未用 HALCON `referenceToRunHomMat2D` 修正 ROI。
- 工具级局部源与基准图全局源的下游互斥选择应用尚未落到各 Adapter。
- `affine_trans_region` / `affine_trans_contour_xld` 尚未接到具体下游 ROI 消费链路。
- 基准图私有模板定位器高级参数尚未暴露到 UI。

## 0.5 2026-07-21 续：基准图自匹配轮廓与质心显示

已补齐基准图位置修正私有模板自匹配的预览反馈：

- ROI 点击“完成”且 HALCON 自匹配成功后，基准图画布显示最佳匹配的绿色模型轮廓和质心十字。
- “测试运行”会重新执行一次基准图自匹配，并刷新轮廓、质心、姿态和分数，不再只显示缓存状态文字。
- 重新打开已有有效配置时，会通过持久模型缓存自匹配一次并恢复匹配标记。
- 开始编辑 ROI、切换当前图像、关闭位置修正、更换基准图、匹配失败时立即清除旧轮廓和质心，防止显示失效结果。
- 预览只显示 `matchIndex=0` 的 `match_result` 和 `match_center`，不额外显示全图搜索 ROI 或分数文字。

验证结果：

```text
reference_position_correction_private_model_smoke: all checks passed
主工程 qmake + make: passed
git diff --check: passed
```

专项 smoke 已增加断言，确认存在模型轮廓、两条质心十字线，且十字交点与主位姿 `x/y` 一致。

## 0.6 2026-07-22 续：基准图自定义定位点已接入

已按 `基准图自定义定位点设计.md` 完成基准图定位原点选择，并接入真实定位与位置修正链路：

- 基准图位置修正新增“质心 / 自定义点”模式；旧方案缺少新字段时继续默认使用模板质心。
- 自定义点可在整张基准图任意位置选取，不受模板 ROI 限制；配置以全图归一化坐标 `customOriginNormalized` 保存和回显。
- 基准图画布用青色十字显示自定义点；HALCON 自匹配结果仍用绿色模型轮廓和绿色匹配点显示，二者语义区分明确。
- `ReferenceImageDialog` 建模自匹配和 `ToolEngine` 运行图匹配均把同一 `originMode/customOriginNormalized` 传给 `TemplateLocationHalconRunner`。
- 自定义点通过现有 HALCON `affine_trans_point_2d` 参与位姿输出；冻结的 `referencePose` 和运行期 `runPose` 都以该点为原点，不是仅用于预览的标记。
- 切换定位点模式、重新选择点、修改 ROI 或更换基准图都会使旧 `referencePose` 失效；必须重新点击“测试运行”或完成 ROI 建模后才能恢复有效状态。
- 非法模式或越界/非有限自定义点在建模及运行期统一返回 `invalid_custom_origin`，不静默回退到质心。

本轮验证：

```text
position_correction_smoke: all checks passed
reference_position_correction_private_model_smoke: all checks passed
position_correction_engine_context_smoke: all checks passed
position_correction_ui_smoke: all checks passed
主工程独立影子目录 qmake + make: passed
git diff --check（本功能文件）: passed
```

其中专项 HALCON smoke 使用位于模板 ROI 外的全图归一化点 `(0.82, 0.16)`，确认自匹配输出 `x/y` 跟随自定义点；引擎上下文 smoke 确认运行期仍能注册方案级位置修正结果。

尚未完成桌面环境下的人工交互验收：需要实际点击“选择点”，检查缩放/适配窗口后的落点、重新打开方案后的回显，以及更换基准图后中心重置与失效提示。

## 0.7 2026-07-22 续：修复颜色比较对话框基准图测试缺少位置修正结果

用户实测发现：基准图位置修正已经建模可用，颜色比较选择“基准图.位置修正信息”后执行基准图测试，仍返回 `position_correction_source_missing`；关闭颜色比较的位置修正开关则检测正常。

根因与修复：

- 主运行链路通过 `ToolEngine::runTools()` 先运行方案级基准图位置修正，再向下游注入 `positionCorrectionsById`。
- 颜色比较 Dialog 的测试链路此前绕过 `ToolEngine`，直接调用 `ColorComparisonAdapter::run()`，且请求中没有 `referencePositionCorrection` 配置，因此必然找不到来源。
- `ColorComparisonDialog::makeTestRequest()` 现在把当前方案的 `referencePositionCorrection` 序列化到测试请求上下文。
- `ColorComparisonDialog::launchTestRequest()` 现在通过临时 `ToolEngine` 执行颜色比较，使基准图测试、单次测试和连续测试均复用主运行链路的方案级位置修正前置步骤。
- 保留 Adapter 的严格失败合同；配置未启用、基准未创建或实时定位失败时，不伪造矩阵，也不回退到未修正检测。

验证结果：

```text
color_comparison_dialog_integration_smoke: passed
color_comparison_position_correction_smoke: all checks passed
主工程增量 qmake/make: passed
```

`color_comparison_position_correction_smoke` 已增加引擎级回归：由 HALCON 私有模板定位器生成方案级修正矩阵，再确认颜色比较获取并应用 `reference.positionCorrection`，不再返回 `position_correction_source_missing`。

## 0.8 2026-07-22 续：修复颜色比较对话框未执行工具级位置修正

用户选择 `2 位置修正.位置修正信息` 后，在颜色比较对话框测试仍出现
`position_correction_source_missing`。这与 0.7 的方案级基准图来源不是同一问题：
请求虽然已经进入 `ToolEngine`，但传入的配置列表仍只有颜色比较自身，所选工具级位置修正
及其模板定位生产者没有在本帧执行。

现已在 `ColorComparisonDialog` 提交异步任务前解析最小依赖闭包：

- 工具级位置修正订阅普通模板定位时，按方案顺序执行
  `模板定位 → 位置修正 → 颜色比较`；
- 工具级位置修正订阅 `reference.positionCorrection` 时，不重复加入普通模板定位，
  由 `ToolEngine` 的方案级基准图前置步骤生成位姿，再执行
  `位置修正 → 颜色比较`；
- 仅补齐当前选择的上游节点，不执行方案中的无关工具；最终按颜色比较的稳定 `toolId`
  返回结果；
- 缺失、禁用、后向引用仍不补链，继续返回真实的来源缺失错误。

验证结果：颜色比较 Dialog 集成 smoke、位置修正 UI/引擎上下文 smoke、颜色比较位置修正
smoke 和主工程 qmake/make 均通过。

## 1. 我们正在做什么

当前任务是在 Qt 工业视觉工程中实现“位置修正”完整链路。产品模型已经确定：

- 基准图页面有一份方案级独立位置修正配置，固定来源 ID 为 `reference.positionCorrection`，不占工具序号。
- 工具栏可以创建多个独立的位置修正实例，每个实例使用稳定 `toolId`。
- 下游视觉工具开启位置修正后，可以指定基准图位置修正或位于自身之前的某个位置修正实例。
- 定位工具负责产生运行姿态 `X / Y / 角度`；位置修正负责比较基准姿态和运行姿态并生成平移、旋转变换；二者必须分开实现、通过稳定字段合同连接。

历史说明：2026-07-20 轮次主要完成了位置修正的文档、UI、配置引用，以及基准图模板 ROI 绘制。2026-07-21 已新增工具级位置修正 HALCON 变换后端；基准图私有模板定位器和下游 ROI 变换链路仍未完成。

## 2. 必读资料与截图

开始改代码前依次阅读：

1. `AGENTS.md`
2. `docs/FID/Function_Docs.md`
3. `docs/FID/PositionCorrection/位置修正UI设计规范.md`
4. `docs/FID/PositionCorrection/position_correction_function_implementation.md`
5. `docs/FID/PositionCorrection/位置修正提示词规范.md`
6. `docs/FID/PositionCorrection/screenshots/README.md`

用户提供的七张原始截图位于：

```text
docs/FID/PositionCorrection/1.png ... 7.png
```

不得裁剪、覆盖或删除这些原图。`screenshots/README.md` 记录了截图编号与场景映射。

## 3. 已完成的功能

### 3.1 公共类型、配置和稳定引用

- 已有 `ToolType::PositionCorrection`，分类为 `ToolCategory::Location`，并具备字符串序列化/反序列化。
- `src/toolcore/PositionCorrection.{h,cpp}` 已提供：
  - 固定基准图来源 ID `reference.positionCorrection`；
  - `enablePositionCorrection`、`positionCorrectionSourceId`、`positionCorrectionSource` 的兼容读写；
  - 按执行顺序筛选前置位置修正来源；
  - 稳定来源 ID 可用性判断；
  - 基准图位置修正配置 JSON 编解码；
  - 后端未实现阶段的 `positionCorrectionApplied=false`、`positionCorrectionReason="not implemented"` payload。
- `SchemeState`/`SchemeStore` 已增加方案级 `referencePositionCorrection`，旧方案缺少字段时默认关闭。
- 基准图配置与工具实例配置相互独立，不会互相覆盖。

### 3.2 基准图页面的位置修正 UI

主要代码：`src/ReferenceImageDialog.{h,cpp}`。

已完成：

- 位置修正开关、说明态与模板区域设置态。
- 矩形 ROI：在右侧基准图上按住左键拖拽；小于有效尺寸时拒绝。
- 多边形 ROI：左键逐点添加，靠近首点或双击闭合，右键撤销，Esc 取消。
- 矩形和多边形绘制模式互斥。
- 点击“完成”后校验 ROI，并退出绘制模式。
- ROI 使用归一化坐标保存；矩形保存 `templateRoiNormalized`，多边形保存 `templatePolygonNormalized` 及其外接矩形。
- 保存、另存为、重新打开方案后能够恢复 ROI。
- 切换实时图、关闭位置修正或切换 ROI 类型时清理不适用的编辑 overlay。
- 无基准图、无模板区域、尚在编辑时均有明确提示。
- “测试运行”在配置有效后仍明确显示“位置修正后端尚未实现”，不会伪造 OK/NG。

最近修复：多边形完成后底部状态文字较长，`QLabel::sizeHint()` 会撑宽左侧滚动内容，使右侧画布看起来整体向右偏移。`referencePositionStatusLabel` 已设置自动换行、最小宽度为 0，并使用横向 `QSizePolicy::Ignored`。ROI 坐标本身没有偏移。

### 3.3 工具库和独立位置修正实例

- 工具库“定位工具”分类已显示“位置修正”入口，按钮可选中并返回 `ToolType::PositionCorrection`。
- 已使用现有资源图标，工具列表也能显示位置修正图标。
- `ToolsDialog` 已支持创建和编辑多个位置修正实例；每个实例保留独立稳定 `toolId`。
- 已新增 `PositionCorrectionDialog` 及 `.ui`：
  - `基础 / 全部`；
  - 运行点 X、运行点 Y、运行角度三项绑定；
  - 链接按钮的二级菜单；
  - 创建基准；
  - 矩形/多边形模板类型；
  - 基准图预览；
  - 测试运行和完成按钮；
  - 配置保存与重新打开回显。
- 预览区布局曾出现上部空白、图像沉底问题，已通过设置 stretch 修正。
- 当前功能新增函数已补充用途注释。

### 3.4 下游工具的位置修正来源

- `ToolsDialog` 打开工具 Dialog 时，会查找 objectName 包含 `positionCorrection` 的来源下拉框。
- 来源菜单目前会列出已启用的方案级基准图来源，以及当前消费工具之前已启用的位置修正实例。
- 保存时同时保留稳定 `positionCorrectionSourceId` 和兼容显示文本 `positionCorrectionSource`。
- 已保存来源失效时会插入红色“来源不可用”项，不会静默改选第一项。

这部分目前只是 UI 与配置闭环，尚未实际改变任何下游工具的检测 ROI。

### 3.5 测试和构建

最近一次已通过：

- `position_correction_smoke`
- `position_correction_ui_smoke`
- `frame_view_helper_navigation_smoke`
- `qmake` + Qt 主工程完整 `make`
- 位置修正相关文件的 `git diff --check`

最后一次主工程影子构建目录：

```text
build/position-correction-roi
```

## 4. 历史：当时卡点和未完成内容

本节保留 UI/配置阶段的历史缺口，当前状态以文首“当前事实”为准。

### 4.1 定位工具尚未形成真实上游输出

- 工具库中已有模板定位、边缘定位、圆定位入口，但仍是占位入口。
- `ToolsDialog` 对 `TemplateLocation` 明确输出 `dialog is not implemented yet`；边缘定位和圆定位同样没有完整 Dialog/Adapter/Runner 闭环。
- 位置修正需要的运行点 `X / Y / 角度` 因此没有可靠的真实生产者。
- 下一阶段推荐优先实现“模板定位”，因为模板定位可以自然输出 `X / Y / 角度 / 分数`。单圆只能可靠输出圆心，不能从旋转对称目标得到角度；不得伪造圆的旋转角。

### 4.2 独立位置修正 Dialog 的 ROI 仍是占位

- `PositionCorrectionDialog` 中点击矩形或多边形只修改 `templateRegionType` 并显示“后续接入”的提示。
- 独立实例尚未像基准图页面一样接入 `FrameViewHelper` 的矩形/多边形真实绘制、归一化保存和回显。
- `referenceCreated` 仍固定为 `false`，“创建基准”只返回未实现提示。

### 4.3 输出字段绑定菜单仍是临时实现

- 当前菜单把所有前置且启用的工具都当作生产者，并统一暴露 `x / y / angle`，没有检查工具是否真的提供这些输出。
- 还没有统一的“输出字段描述/类型注册表”，也没有区分坐标、角度、分数等字段类型。
- 当前默认还把基准图节点作为 X/Y/角度生产者，这只是 UI 占位，不等于基准图已有真实运行定位结果。
- `validateForFinish()` 只检查 `producerId` 和 `outputKey` 是否非空，没有校验生产者存在、是否前置、是否启用、字段类型是否匹配、基准图是否存在、模板 ROI 是否有效。
- 当时显示编号存在 0/1 混用；当前已统一为 `0 基准图`，历史编号 1 只作为迁移输入，
  固定 ID 始终为 `reference.positionCorrection`。

### 4.4 没有独立 Adapter 和 HALCON Runner

- 尚无 `PositionCorrectionAdapter`。
- 尚无 `PositionCorrectionHalconRunner`。
- `ToolEngine` 尚未注册位置修正运行链路。
- 尚未创建/加载 HALCON 模板、查找运行姿态、计算二维刚性变换或输出变换矩阵。
- 尚无真实 `ToolResult`、overlay、耗时、失败码和测试运行结果。

### 4.5 下游工具尚未应用位置变换

- 下游工具虽然可以保存来源 ID，但 Adapter/Runner 并未解析来源实例的运行结果来变换检测 ROI。
- 删除、禁用、后移来源后的“禁止保存/禁止测试”尚未在所有消费工具中落实；目前主要是显示失效项并保留 ID。
- 没有执行期依赖解析，也没有把位置修正结果传递给后续工具。

### 4.6 文档存在旧状态段落

- `position_correction_function_implementation.md` 前部“当前工程状态（2026-07-10）”仍写着没有 ToolType/Dialog 等旧结论，但后面的 2026-07-11、2026-07-20 实施记录已经说明这些内容部分完成。
- `位置修正UI设计规范.md` 第 11 节仍保留“本阶段只写文档、不新增 ToolType/Dialog”的历史范围。
- `Function_Docs.md` 仍有“公共 helper 尚未读写 positionCorrectionSourceId”的旧描述，而当前 helper 已经读写该字段。
- 下一会话应先整理这些状态描述，保留历史记录但将“当前状态”更新为代码事实。

## 5. 下一步实施计划

建议严格按以下顺序推进。

### 第一步：锁定定位输出合同

定义统一、带类型的定位输出，例如：

```text
positionX    coordinate
positionY    coordinate
angle        angle
score        score
matchCount   integer
```

输出菜单必须依据生产者真实声明生成，不能继续给所有工具虚构 `x/y/angle`。显示文本可动态变化，内部引用必须使用 `producerId + outputKey`。

### 第二步：优先实现模板定位闭环

为模板定位补齐：

- Dialog 和 ROI；
- `ToolConfig` 参数保存/回显；
- Adapter；
- HALCON Runner；
- `X / Y / 角度 / 分数` 输出；
- 正常、空图、无模板、低分和 HALCON 异常 smoke。

开始算法代码前必须在本机确认 HALCON runtime、license 和所需算子/符号。候选方向是形状模板、模板查找和二维刚性变换，但必须实机确认后才能锁定接口。

### 第三步：完善独立位置修正 Dialog

- 复用基准图页面已验证的矩形/多边形 ROI 交互。
- 保存和回显每个实例自己的归一化 ROI，绝不能覆盖方案级基准图 ROI。
- 根据真实输出注册表生成二级绑定菜单。
- 完整校验生产者、执行顺序、字段类型、基准图和模板区域。
- “创建基准”只有在真实算法成功后才能设置 `referenceCreated=true`。

### 第四步：实现位置修正 HALCON 链路

- 新增 `src/tooladapters/PositionCorrectionAdapter.{h,cpp}`。
- 新增 `src/algorithms/<合适定位域>/PositionCorrectionHalconRunner.{h,cpp}`。
- 在 `ToolEngine` 和工程文件中注册。
- 输入基准姿态与运行姿态，输出 `deltaX / deltaY / deltaAngle` 和二维变换。
- 使用 HALCON 区域/轮廓变换能力生成修正后的 ROI；不得使用 OpenCV 或自写算法替代核心能力。
- 明确处理无图、无模板、绑定失效、定位失败、HALCON runtime/license/符号异常。

### 第五步：接入下游消费工具

- 执行时按稳定来源 ID 获取前置位置修正结果。
- 在各工具进入核心检测前修正 ROI。
- 来源删除、禁用或变为后置时保留 ID、显示错误，并禁止测试/完成；不得自动回退。
- 增加重排、复制、删除和多实例回归。

### 第六步：更新文档并完成验证

- 清理第 4.6 节列出的旧状态描述。
- 更新专项实现记录，不把未实现能力写成已完成。
- 运行主工程 qmake/make、位置修正 smoke、定位 smoke 和实际 UI 手测。

## 5.5 2026-07-21 颜色比较消费链路

- 本历史节点中颜色比较曾是唯一消费者；当前颜色比较和 Blob 有无均已完整消费位置修正结果。
- 配置来源以 `positionCorrectionSourceId` 为稳定 ID；运行时只查询 `positionCorrectionsById[sourceId]`，不组合基准图全局矩阵和工具级局部矩阵。
- ROI 方向固定为 `referenceToRunHomMat2D`。有效检测区由 HALCON `affine_trans_region` 变换，再由 HALCON `clip_region` 裁剪；原始归一化配置不写回。
- 失败合同：`position_correction_source_missing`、`source_invalid`、`invalid_position_correction_matrix`、`corrected_detect_roi_empty`。
- 专项 `color_comparison_position_correction_smoke` 已验证 X 正向位移后颜色比较命中修正区域，以及来源缺失不沿用历史矩阵。
- 工具级位置修正没有内部模板；基准图位置修正私有模板必须保留，并与普通模板定位的实例和缓存键隔离。颜色比较直方图模型不是位置模板。
- 仍未完成：其他下游工具的 ROI 消费链路。

## 6. 绝对不要再踩的坑

1. **不要伪造成功。** 真实 HALCON 后端完成前，不能显示 OK/NG、匹配分数、修正矩阵或 `positionCorrectionApplied=true`；统一返回 `not implemented`。
2. **不要用 OpenCV 或自写算法实现核心视觉逻辑。** 项目红线要求核心定位、模板匹配和变换必须使用已确认可用的 HALCON 能力。
3. **不要用显示序号做引用。** 当前 `0 基准图` 和工具实例显示序号都不是运行键；
   运行必须依赖 `reference.positionCorrection` 或稳定 `toolId`。
4. **来源失效不能静默切换。** 删除、禁用或后移来源后必须保留原 ID 并提示失效，不能自动选基准图、第一项或上一次结果。
5. **工具级位置修正不能再拥有模板 ROI。** 模板 ROI 只属于
   `SchemeState.referencePositionCorrection` 的私有定位器，工具实例只保存姿态来源和基准姿态。
6. **不要让所有前置工具假装有 X/Y/角度。** 必须建立真实输出字段注册与类型校验；尤其单圆没有天然旋转角。
7. **不要让普通按钮误触发 Dialog 关闭。** 链接、ROI、创建基准和测试按钮不能连接到 `accept()`、`reject()` 或默认按钮行为。
8. **多边形必须使用图像归一化坐标。** 不能保存窗口/画布像素坐标；缩放、适配窗口和重新打开后必须保持几何位置。
9. **长状态文本不能撑宽布局。** 状态 `QLabel` 要允许换行，并避免横向 `sizeHint` 改变左侧面板宽度；此前已发生“点击完成后整体右移”的视觉问题。
10. **不要覆盖用户工作区。** 当前工作树存在大量颜色识别、颜色比较、工程配置、测试图片和构建产物改动，很多与位置修正无关。禁止 `git reset --hard`、`git checkout --`、全量格式化或全量暂存。
11. **不要提交构建产物。** 优先在影子目录构建；不要暂存 `build/`、`*.o`、Makefile、moc/ui 生成文件、可执行文件和日志。
12. **不要照抄旧状态段落。** 专项文档存在历史描述滞后，判断完成度必须同时检查当前代码、后部实施记录和测试。

## 7. 关键文件导航

```text
docs/FID/PositionCorrection/                  专项文档与原始截图
docs/FID/Function_Docs.md                     公共位置修正规范
src/toolcore/PositionCorrection.{h,cpp}       公共配置、来源和 JSON helper
src/toolcore/ToolTypes.h                      PositionCorrection 类型
src/SchemeStore.{h,cpp}                       方案级基准图位置修正持久化
src/ReferenceImageDialog.{h,cpp}              基准图入口与已完成 ROI 绘制
src/PositionCorrectionDialog.{h,cpp}           独立工具配置 Dialog
ui/PositionCorrectionDialog.ui                独立工具 UI
src/ToolLibraryDialog.{h,cpp}                 工具库入口
ui/ToolLibraryDialog.ui                       工具库位置修正按钮
src/ToolsDialog.{h,cpp}                       多实例创建、编辑及来源菜单注入
src/frame/FrameViewHelper.{h,cpp}              矩形/多边形交互和坐标映射
smoke/position_correction_smoke.cpp            配置、来源和 JSON smoke
smoke/position_correction_ui_smoke.cpp         Dialog/工具库 UI smoke
```

## 8. 建议验证命令

主工程继续使用影子目录：

```bash
cd build/position-correction-roi
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
make -j$(nproc)
```

修改位置修正后至少重新构建并运行：

```text
position_correction_smoke
position_correction_ui_smoke
frame_view_helper_navigation_smoke
```

涉及 UI 时还要手动验证：矩形、多边形、完成、保存、重新打开、无基准图、切换实时图、来源失效、取消 Dialog，以及长状态文字不再使布局横向偏移。

## 9. Git 与工作区状态提醒

- 当前分支和工作树包含大量并行开发改动，不能假设所有修改都属于位置修正。
- 不要执行清理或回退命令。
- 如需提交，只逐文件核对并暂存本功能源码、UI、专项文档和 smoke；不要使用 `git add .`。
- 构建目录中存在已生成文件的变更，不属于交付源码。

## 10. 是否写入公有文档

不建议把整份 `HANDOFF.md` 写入 `docs/FID/Function_Docs.md`。原因是交接文档包含当前进度、临时卡点、验证目录和工作区提醒，会快速过时。

公有文档只保留长期稳定的跨功能合同，例如：

- 基准图固定节点与工具多实例模型；
- 稳定来源 ID；
- 禁止后向引用、循环引用和失效来源静默回退；
- `positionCorrectionSourceId`/`positionCorrectionSource` 字段语义；
- 后端未实现时不得伪造应用结果。

这些规则已经基本写入 `Function_Docs.md`。下一会话只需要修正其中“公共 helper 尚未读写稳定 ID”的过时描述，不要把实施日志和待办列表继续堆入公有文档。
