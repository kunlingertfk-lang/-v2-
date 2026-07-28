# 注册目标检测开发 SOP

## 1. 文档定位

本文档是“注册目标检测”从界面占位推进到真实生产闭环的专项开发 SOP，约束需求确认、HALCON 技术路线、配置合同、模型生命周期、工程接入、测试和交付。

本文档不替代：

- 根目录 `AGENTS.md` 的项目级约束。
- `docs/FID/Function_Docs.md` 的 FID 公共规范。
- `registered_classification_detection_function_implementation.md` 的阶段状态记录。
- `注册分类检测提示词规范.md` 的开发提示词与 UI 约束。

发生冲突时，按 `AGENTS.md`、`Function_Docs.md`、本文档、功能实现记录和提示词规范的顺序执行。

## 2. 当前基线与成熟度

审计日期：2026-07-23。

当前成熟度：**界面占位**。不得标记为“部分可运行”“闭环可运行”或“生产就绪”。

### 2.1 已具备

- 工具库可选择 `注册目标检测`。
- 已有 `ToolType::RegisteredClassificationDetection` 及字符串映射。
- `ToolsDialog` 和 `MainWindow` 支持新建、编辑该工具。
- 已有独立 `RegisteredClassificationDetectionDialog`。
- 配置保存到 `ToolConfig.params.registeredClassificationDetection`，判断规则保存到 `judgeRule`。
- 主配置窗口支持基础/全部模式、模型路径、全屏/矩形检测区域、位置修正占位、结果判断和保存回显。
- 已有独立 `RegisteredClassificationDetectionTrainingDialog`。
- 训练窗口具备会话内添加图像、矩形/多边形标注、ROI 编号、目标计数、目标 ROI 预览、缩略图过滤和删除交互。
- 空相机图像返回 `image_empty`，有图但无后端时明确返回 `backend_not_implemented`。
- 当前工作树的主工程已在隔离副本中执行 `qmake qt_ui_test.pro` 和 `make -j4`，构建通过。

### 2.2 尚未具备

- 没有注册目标检测专用 Adapter。
- 没有注册目标检测训练 Runner、HALCON 检测 Runner 或生产模型缓存。
- `ToolEngine` 没有注册该功能的 Adapter。
- 训练图、目标标注和训练参数没有真实持久化。
- “开始训练”不执行 HALCON 建模，也不生成模型包。
- “测试运行”不调用 `ToolEngine`，始终返回 `backend_not_implemented`。
- 没有生产 bbox、轮廓、类别、角度、尺度、数量和得分输出。
- 模型管理仍复用注册分类窗口，不能证明检测模型包完整性。
- 导入模型除拒绝 `.scbin` 和非法文件名外，没有验证模型类型、schema、文件完整性和 HALCON 可读性。
- 位置修正开关仍被禁用，没有实际变换检测区域。
- 没有算法 Runner/Adapter smoke、模型生命周期 smoke 和固定图像回归集。

### 2.3 当前验证缺口

`smoke/registered_classification_detection_dialog_smoke.pro` 已落后于现有依赖。2026-07-23 在隔离副本中重新构建时，编译完成但链接失败，缺少的依赖包括：

- `RegisteredClassificationTrainingRunner`
- `RegisteredClassificationModelPackage`
- `RegisteredClassificationTrainingSession`
- `FrameInputMetadata.cpp`
- 以及上述实现继续依赖的注册分类算法源文件

因此，历史构建产物或文档中的旧“通过”记录不能作为当前绿色验收证据。开始算法开发前，应先修复专项 UI smoke 工程并重新运行。

## 3. 产品语义与路线决策

### 3.1 第一版产品语义

第一版默认按以下场景实现：

- 工业固定工位。
- 刚性或近似刚性目标。
- 用户通过少量注册图和目标 ROI 快速注册。
- 同一图像中允许出现零个、一个或多个目标实例。
- 需要输出目标位置、角度、尺度、得分、bbox/轮廓和数量。
- 需要较短建模时间、稳定 CPU 推理和可解释结果。

第一版不是通用语义目标检测，不承诺在大幅外观变化、任意复杂背景、严重遮挡或未注册视角下保持泛化能力。

如果现场需求不满足以上边界，必须回到本 SOP 的 S0 阶段重新选择后端，不得在实现中静默改变算法语义。

### 3.2 首选后端

第一版首选：

```text
HALCON 多模板 Shape Model 模型库
```

推荐核心算子：

- `reduce_domain`
- `crop_domain`
- `create_shape_model` / `create_scaled_shape_model`
- `find_shape_models` / `find_scaled_shape_models`
- `get_shape_model_contours`
- `set_shape_model_origin`
- `write_shape_model`
- `read_shape_model`
- `vector_angle_to_rigid`
- `hom_mat2d_scale`
- `affine_trans_region`
- `affine_trans_contour_xld`
- `smallest_rectangle1` / `smallest_rectangle2`
- `intersection`、`area_center` 等结果去重辅助算子

第一版选择 Shape Model 的原因：

- 少量注册样本即可形成可用模型，不依赖大规模训练集。
- 原生输出位置、角度、缩放和匹配得分。
- 可通过模型轮廓变换形成真实 Overlay。
- 当前 HALCON 20.11 环境具备 shape matching 头文件、库和既有项目使用经验。
- 当前开发机未发现 NVIDIA GPU、CUDA/cuDNN 运行环境或预训练 `.hdl` backbone，Shape Model 的落地风险更低。

### 3.3 可选后端

以下能力不得混入第一版默认实现，应作为明确的后端类型或后续版本：

#### 局部可变形模型

适用条件：

- 目标存在连续、可估计的局部形变。
- Shape Model 在固定回归集上因形变持续漏检。

候选算子：

- `create_local_deformable_model`
- `find_local_deformable_model`
- `write_deformable_model`
- `read_deformable_model`

该路线需要单独记录耗时、边界目标行为、变形平滑参数和轮廓回传方式，不得作为 Shape Model 失败后的静默 fallback。

#### HALCON DL Object Detection

适用条件：

- 需要复杂背景和大幅外观变化下的类别泛化。
- 已取得足量、代表性且经过复核的训练/验证/测试数据。
- 已确认训练和部署设备、HALCON DL license、预训练 backbone、CUDA/cuDNN 或受支持 CPU 路径。
- 已接受更长训练时间、模型体积和版本兼容成本。

候选算子：

- `create_dl_model_detection`
- `set_dl_model_param`
- `train_dl_model_batch`
- `apply_dl_model`
- `write_dl_model`
- `read_dl_model`

HALCON 20.11 检测合同以 `rectangle1` 或带方向的 `rectangle2` 为主。当前多边形标注若用于 DL 检测，必须在数据合同中明确转换为哪种包围框；不得把多边形直接声称为实例分割标注。

`.scbin` 是海康专有格式，本功能不解析、不转换，也不伪装兼容。

### 3.4 禁止路线

- 不得复用 `RegisteredClassificationAdapter` 或 `RegisteredClassificationHalconRunner` 包装出伪目标检测结果。
- 不得用 OpenCV 模板匹配、轮廓算法或 DNN 替代 HALCON 核心算法。
- 不得把 RKNN/YOLO 的 `AiDetectionRunner` 直接作为注册目标检测后端。
- 不得在 Shape Model 找不到目标时静默切换另一种算法。
- 不得只用裁剪图分类结果反推 bbox。

## 4. 目标架构

目标生产链路：

```text
ToolLibraryDialog / ToolsDialog / MainWindow
        |
        v
RegisteredClassificationDetectionDialog
        |
        +--> RegisteredClassificationDetectionTrainingDialog
        |
        v
ToolConfig.params.registeredClassificationDetection / judgeRule
        |
        v
RegisteredClassificationDetectionAdapter
        |
        v
RegisteredClassificationDetectionHalconRunner
        |
        v
ToolResult.payload.targets[] / overlays / elapsedMs
```

训练链路：

```text
注册图 + Target ROI + 训练参数
        |
        v
RegisteredClassificationDetectionTrainingSession
        |
        v
RegisteredClassificationDetectionTrainingRunner
        |
        +--> HALCON Shape Model 文件
        +--> manifest.json
        +--> training_report.json
        +--> training_session/
```

生产 Runner 不访问 UI，不直接管理对话框状态。Dialog 不承载生产核心算法。

## 5. 配置合同 v2

当前 `version=1` 中的 `halcon_dl_classification`、`topK`、`class_match` 等字段来自注册分类 UI，不应直接成为生产检测合同。

开发真实后端前必须设计并落地 `version=2`。推荐最小字段：

```json
{
  "version": 2,
  "backendType": "halcon_shape_model_bank",
  "modelPackagePath": "",
  "modelPackageId": "",
  "detectRegionType": "full",
  "roiNormalized": {
    "x": 0.0,
    "y": 0.0,
    "width": 1.0,
    "height": 1.0
  },
  "minScore": 0.68,
  "maxMatches": 0,
  "maxOverlap": 0.5,
  "angleEnabled": false,
  "angleStartDeg": 0.0,
  "angleExtentDeg": 0.0,
  "scaleEnabled": false,
  "scaleMin": 1.0,
  "scaleMax": 1.0,
  "polarity": "use_polarity",
  "subPixel": "interpolation",
  "numLevels": 0,
  "greediness": 0.8,
  "timeoutMs": 2000,
  "enablePositionCorrection": false,
  "positionCorrectionSource": "0 基准图.位置修正信息"
}
```

字段约束：

- `maxMatches=0` 表示返回所有达到阈值且通过去重的目标；若产品不接受该语义，应改成显式上限。
- `minScore` 在配置层统一使用 `0.0~1.0`，UI 百分比仅负责换算。
- 角度在配置和结果 payload 中使用度，进入 HALCON 前统一转弧度。
- `x=Column`、`y=Row`，不得混用。
- Scale 必须有限且大于零。
- 位置修正只变换检测 ROI/Mask；Shape Model 返回的目标姿态仍以当前原图坐标表示。
- v1 旧配置只能显式迁移或返回 `unsupported_config_version`，不得用隐式默认值伪造有效模型。

推荐 `judgeRule`：

```json
{
  "mode": "count_and_score",
  "expectedLabel": "Target",
  "minTargetCount": 1,
  "maxTargetCount": 1,
  "minTargetScore": 0.68,
  "aggregation": "all_targets"
}
```

执行成功与业务 OK/NG 必须分开：

- 无目标但算法正常执行：`success=true`，`status=no_target`，最终 `ok` 由 `judgeRule` 决定。
- 模型缺失、损坏、HALCON 异常：`success=false`。

## 6. 标注与训练会话合同

每张注册图至少保存：

- 稳定 `imageId`
- 原图相对路径
- 图像宽、高、通道、位深和输入签名
- 文件哈希
- 标注数组

每个标注至少保存：

- 稳定 `annotationId`
- `labelId` 和 `labelName`
- `shapeType=rectangle|polygon`
- 原始归一化几何
- 从原始几何计算出的 `rectangle1` 或 `rectangle2`
- 参考原点 `row/column`
- 是否参与训练

训练参数至少保存：

- Shape Model 类型
- 角度和尺度范围
- 极性
- `contrast`、`minContrast`
- pyramid 层级
- 优化参数
- 模型原点规则
- 最小有效 ROI 尺寸

矩形和多边形标注都必须先由 HALCON 生成 Region。Shape Model 以 Region 限定后的原始图像建模；缩略图、框线、编号和文字不得进入训练图。

## 7. 模型包合同

推荐目录：

```text
ModelFiles/RegisteredTargetDetection/<date>/<model_id>/
├── manifest.json
├── training_report.json
├── models/
│   ├── template_0000.shm
│   └── template_0001.shm
└── training_session/
    ├── session.json
    └── images/
```

`manifest.json` 至少包含：

- `schemaVersion`
- `modelPackageId`
- `modelType=halcon_shape_model_bank`
- 创建时间
- HALCON 版本
- 类别映射
- 模板文件清单和哈希
- 每个模板的来源图、来源 ROI、参考原点和模型参数
- 输入图像合同
- 训练参数哈希
- 支持的角度/尺度范围

模型状态：

- `empty`
- `training`
- `ready`
- `stale`
- `invalid`
- `unsupported`

以下变化必须使模型进入 `stale`，要求重新训练：

- 注册图或标注变化
- Shape Model 建模参数变化
- 输入位深、颜色/灰度合同变化
- 模型原点规则变化
- schema 或 HALCON 主版本不兼容

导入时必须校验 manifest、哈希、模型类型、HALCON 可读性和文件完整性；不得接受任意扩展名文件后直接标记为 ready。

## 8. Runner 输出合同

成功运行的 `ToolResult.payload` 至少包含：

```json
{
  "backendType": "halcon_shape_model_bank",
  "modelPackageId": "",
  "targetCount": 1,
  "targets": [
    {
      "targetId": "0",
      "labelId": "target",
      "label": "Target",
      "score": 0.91,
      "x": 320.0,
      "y": 240.0,
      "row": 240.0,
      "column": 320.0,
      "angleDeg": 12.5,
      "scale": 1.0,
      "bboxNormalized": {},
      "polygonNormalized": []
    }
  ],
  "positionCorrectionApplied": false,
  "positionCorrectionReason": "disabled"
}
```

Overlay 要求：

- 每个目标至少有 bbox 或变换后的真实模型轮廓。
- Overlay 标注包含类别、得分和稳定目标编号。
- ROI Overlay 与检测结果 Overlay 使用不同 `role`。
- bbox、polygon 和中心点全部映射回原图坐标。
- 多模板命中同一目标时先完成去重，再生成最终 Overlay。

稳定错误状态至少包含：

- `image_empty`
- `invalid_roi`
- `no_model`
- `model_stale`
- `model_corrupted`
- `unsupported_model`
- `unsupported_config_version`
- `halcon_runtime_missing`
- `halcon_license_error`
- `halcon_symbol_missing`
- `timeout`
- `no_target`
- `ok`

## 9. 分阶段执行 SOP

### S0：需求和后端冻结

动作：

1. 收集目标材质、刚性、尺寸、视角、数量、遮挡、背景和节拍要求。
2. 明确单类别还是多类别。
3. 明确输出 bbox、旋转框、精确轮廓中的哪几种。
4. 用代表性图像判断 Shape Model 是否符合产品边界。
5. 冻结第一版为 `halcon_shape_model_bank` 或停止并重新设计。

完成证据：

- 功能文档记录输入、输出、范围和非目标。
- 有固定训练图、验证图和预期结果。
- 后端决策经过评审。

### S1：HALCON 能力冒烟

先做独立、无 UI 的 Runner spike：

1. 从一张图和一个 ROI 创建 Shape Model。
2. 写入并重新读取模型。
3. 在另一张图中查找零个、一个和多个实例。
4. 返回 Row、Column、Angle、Scale、Score。
5. 获取并变换模型轮廓。
6. 验证 runtime、license、符号缺失和句柄清理。

门禁：

- 本阶段失败时停止后续生产实现。
- 不得改用非 HALCON 替代算法。

### S2：配置和模型合同

动作：

- 冻结 v2 配置、judgeRule、训练会话、manifest 和结果 payload。
- 定义 v1 迁移策略。
- 定义模型 stale、invalid、unsupported 状态。

完成证据：

- JSON 示例和字段表经过评审。
- 配置 round-trip 测试通过。
- 未知版本和损坏 manifest 能被拒绝。

### S3：训练会话持久化

动作：

- 保存注册图、ROI、目标名称和检测专属参数。
- 重开训练窗口后恢复全部图像和标注。
- 删除图片/ROI 时同步更新会话和模型状态。

完成证据：

- 新建、保存、关闭、重开、继续编辑流程通过。
- 空图、丢失图、损坏 session 返回明确错误。

### S4：TrainingRunner 和模型包

动作：

- 新增独立训练请求、结果和 Runner。
- 为有效标注生成模型文件。
- 写 manifest、训练报告和哈希。
- 使用临时目录完成训练，成功后原子发布模型包。

完成证据：

- 至少生成并重读两个模板。
- 训练失败不留下假 ready 模型。
- 重训、覆盖、取消和异常清理可验证。

### S5：HalconRunner

动作：

- 缓存已验证模型句柄。
- 在检测 ROI 内运行多模板、多实例匹配。
- 合并和去重跨模板结果。
- 输出目标姿态、bbox、polygon、轮廓和诊断。
- 确保正常、无目标、异常和中途返回都只清理一次资源。

完成证据：

- 固定图像 Runner smoke 通过。
- 结果坐标、角度和尺度与已知目标一致。
- 无目标与执行失败被正确区分。

### S6：Adapter、Engine 和位置修正

动作：

- 新增独立 Adapter，解析 v2 配置并转换 Runner 结果。
- 在 `MainWindow` 注册 Adapter。
- 接入位置修正公共消费链，来源失败时阻断修正。
- 当前帧失败时不得复用上一帧结果。

完成证据：

- 单工具和工具链运行都返回真实 `ToolResult`。
- 位置修正关闭、成功、来源缺失和来源 NG 均有专项测试。

### S7：Dialog 和模型管理收口

动作：

- “开始训练”调用 TrainingRunner。
- “测试运行”和“基准图测试”调用真实 ToolEngine。
- 模型管理改为检测专用模型包视图。
- 删除或迁移 `TopK`、`halcon_dl_classification` 等分类遗留语义。
- 显示目标数量、得分、类别、角度、尺度、耗时和 Overlay。

完成证据：

- 新建、保存、重开、编辑、训练、测试、模型导入/导出/删除闭环通过。
- 普通按钮不会误关闭主窗口。

### S8：回归和交付

动作：

- 运行主工程 qmake/make。
- 运行 UI、TrainingRunner、HalconRunner、Adapter、模型生命周期和位置修正 smoke。
- 使用固定图集和现场图集回归。
- 记录性能、稳定性和未覆盖项。

完成证据：

- 全部命令、退出码和关键输出写入实现记录或分析报告。
- 达到本 SOP 的 Definition of Done 后才能调整成熟度。

## 10. 最低测试矩阵

| 维度 | 必测项 |
| --- | --- |
| 图像 | 空图、灰度、彩色、尺寸变化、通道/位深不支持 |
| ROI | 全图、有效矩形、有效多边形、越界、零面积、边界目标 |
| 目标 | 无目标、单目标、多目标、相邻目标、部分遮挡、低对比 |
| 姿态 | 默认角度、旋转上下界、尺度上下界 |
| 模型 | 无模型、ready、stale、损坏、缺文件、未知 schema、HALCON 不可读 |
| 判断 | 数量不足、数量超限、低分、全部合格、部分不合格 |
| 位置修正 | 关闭、正常应用、来源缺失、来源失败、基准缺失 |
| 运行环境 | runtime 缺失、license 异常、符号缺失、超时 |
| UI | 新建、保存、取消、重开、连续运行、停止、快速切换、关闭 |
| 资源 | 重复训练、重复推理、模型切换、异常返回、长时间运行 |

建议固定回归集至少包含：

- 1 张训练图。
- 1 张完全无目标图。
- 1 张单目标平移图。
- 1 张多目标图。
- 1 张旋转目标图。
- 1 张尺度变化图。
- 1 张低对比或部分遮挡图。

## 11. 完成定义

### Definition of Ready

- 产品语义和第一版后端已冻结。
- 固定训练/验证/测试图像可获得。
- HALCON 能力 smoke 已通过。
- v2 配置、模型包、结果和错误合同已评审。
- 专项 UI smoke 工程已修复并恢复绿色。

### Definition of Done

- 工具入口、配置保存回显、训练会话、真实建模、模型管理、Adapter、Runner、Engine、结果和 Overlay 全部闭环。
- 主工程和全部专项 smoke 通过。
- 空图、无模型、损坏模型、无目标、多目标、位置修正和 HALCON 环境异常不崩溃。
- 固定回归集达到已约定的召回率、误检率、定位误差和耗时目标。
- 模型句柄、临时文件和异步任务生命周期经过重复运行验证。
- 实现记录更新了实际命令、退出码、验证结果、剩余风险和当前成熟度。

在完成以上条件前，不得把本功能标记为“闭环可运行”。

## 12. 官方 HALCON 参考

- Shape matching：`https://www.mvtec.com/doc/halcon/2011/en/find_shape_model.html`
- Scaled shape matching：`https://www.mvtec.com/doc/halcon/2011/en/find_scaled_shape_model.html`
- DL detection model：`https://www.mvtec.com/doc/halcon/2011/en/create_dl_model_detection.html`
- DL training：`https://www.mvtec.com/doc/halcon/2011/en/train_dl_model_batch.html`
- DL inference：`https://www.mvtec.com/doc/halcon/2011/en/apply_dl_model.html`

