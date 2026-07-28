# 全工具 ROI 移动、缩放、旋转统一改造规划

日期：2026-07-24  
状态：只读盘点与实施规划，尚未修改代码

## 1. 理解复述

### 1.1 目标

检查当前工程中所有需要绘制模板 ROI、检测/搜索 ROI、训练标注 ROI 的已有功能，解决“只能重新绘制，不能在已有 ROI 上继续调整”的问题。

目标交互参考海康机器人智能相机客户端：

- 绘制完成后可再次选中 ROI。
- 拖动 ROI 内部可整体移动。
- 拖动控制点可缩放或改变几何大小。
- 对有方向的 ROI 可通过旋转控制点调整角度。
- 多边形可拖动顶点，也可整体移动、缩放、旋转。
- 可删除选中的 ROI；多 ROI 场景能明确选中对象和编号。
- 编辑结果能够保存、重新打开回显，并被 Adapter、HALCON runner 和结果 overlay 按同一几何使用。

### 1.2 输入、输出与边界

输入是当前工程中已存在的 `FrameViewHelper`、各工具 Dialog、`ToolConfig.params` / `roiNormalized`、Adapter 和 HALCON runner 链路。

本规划输出：

1. 当前 ROI 能力与工具覆盖清单。
2. 统一交互和公共组件设计。
3. 配置兼容及 HALCON 算法接入原则。
4. 分阶段实施顺序。
5. 自动化与人工验收矩阵。

本阶段不修改源码、不改变现有配置语义、不实现自由画笔，也不扩展尚未进入工具库的工具。

### 1.3 参考依据

仓库内《智能相机客户端 用户手册 V3.2.2》（2026-01-27）第 7.4.2 节“绘制 ROI”（PDF 第 96～101 页）明确描述：

- 直线 ROI：选中后可移动、调角度和大小。
- 四边形 ROI：选中后可移动、调角度和大小。
- 多边形 ROI：选中后可移动，并通过交点调整。
- 圆 ROI：选中后可移动，并通过圆周控制点调半径。
- 选中 ROI 后支持 Delete 或右键删除。
- 多 ROI 使用数字角标标记绘制顺序。

本项目只对齐这些可观察交互，不宣称复制海康内部实现或专有格式。

## 2. 已知事实、信息缺口与默认假设

### 2.1 已知事实

公共画布能力集中在：

- `src/frame/FrameViewHelper.h`
- `src/frame/FrameViewHelper.cpp`

当前几何与交互现状：

| 几何 | 当前数据 | 当前绘制 | 已有二次编辑 | 主要缺口 |
|---|---|---:|---:|---|
| 轴对齐矩形 | `QRectF` | 有 | 无 | 不能选中、移动、缩放、旋转 |
| 多边形 | 点集 | 有 | 部分有 | 可拖顶点、可整体移动，但没有统一选中态、包围盒缩放、整体旋转和删除 |
| 圆 | 圆心、半径 | 有 | 无 | 不能移动、调半径；圆本身无可见旋转语义 |
| 线带 | 两端点、宽度 | 有 | 无 | 不能移动端点、整体旋转、调长度和宽度 |
| 点 | 单点 | 有 | 无 | 当前只用于定位点选择，不属于本次主要 ROI 范围 |
| 运行结果 overlay | `ToolOverlay` | 不适用 | 只读 | 必须与配置 ROI 编辑层保持分离 |

`FrameViewHelper` 目前同时承担图像显示、导航、ROI 草图、少量多边形编辑和结果 overlay，事件分支已经较多。继续为每种 ROI 直接追加独立鼠标分支，会显著增加状态冲突风险。

现有矩形配置普遍只保存 `QRectF`，没有角度字段。若仅在 UI 上旋转，Adapter/runner 仍按轴对齐外接矩形处理，将造成显示区域与实际检测区域不一致，因此旋转矩形必须完成配置、Adapter、HALCON Region、overlay 全链路后才可在工具中启用。

### 2.2 信息缺口

- 用户尚未指定控制柄的最终视觉稿、颜色和快捷键细节。
- 尚未要求第一阶段必须覆盖屏蔽区域；但屏蔽区同样使用多边形编辑，若不共用公共编辑器会形成不一致体验。
- 不同工具是否允许一个检测区域包含多个 ROI，必须保持各自现有产品合同，不能因公共编辑器支持集合而自动放开数量。

### 2.3 可安全采用的默认假设

- 参考海康的交互语义，但视觉样式沿用本项目橙色模板 ROI、蓝色检测 ROI和现有 QSS。
- 单 ROI 工具仍只允许一个 ROI；新绘制替换旧 ROI。
- 已支持多 ROI 的训练/模板场景保留多 ROI 和原编号，不扩展其他工具的 ROI 数量。
- 圆只提供移动和半径缩放；“旋转圆”没有几何效果，不显示旋转柄。
- 多边形提供顶点编辑、整体移动、等比缩放和整体旋转；不自动增加/删除顶点，顶点增删可作为后续能力。
- ROI 编辑使用原图像素空间计算、`0..1` 归一化坐标保存；画布缩放和平移只改变显示变换。
- 越界处理默认采用“保持整体形状并限制在图像内”，而不是逐点裁断导致形状畸变。

## 3. 全工具 ROI 盘点

### 3.1 工具库已有功能

以下清单以 `ToolLibraryDialog::confirmSelection()` 当前可创建的工具为准。

| 工具 | ROI 角色 | 当前形状 | 计划编辑能力 | 优先级与备注 |
|---|---|---|---|---|
| 图案有无 `PatternPresence` | 模板 ROI、检测 ROI | 模板矩形/多边形；检测矩形 | 矩形移动/缩放/旋转；多边形顶点/移动/缩放/旋转 | P1 试点；模板旋转会使模型失效并要求重新创建 |
| 斑点有无 `BlobPresence` | 检测 ROI | 矩形/多边形/圆 | 按形状完整编辑 | P1 试点；三种公共几何覆盖最全 |
| 圆有无 `CirclePresence` | 检测 ROI | 矩形/多边形/圆 | 按形状完整编辑 | P2；复用 Blob 接入模式 |
| 边缘有无 `EdgePresence` | 检测/搜索 ROI | 线带 | 移动、端点调长度/角度、侧边调宽度 | P1 试点；HALCON 已使用 rectangle2 测量语义 |
| 直线有无 `LinePresence` | 检测/搜索 ROI | 线带 | 同 Edge | P2 |
| 轮廓有无 `ContourPresence` | 模板 ROI、检测 ROI | 模板矩形/多边形；检测矩形/多边形 | 按形状完整编辑 | P2；模板变化要求重新建模 |
| 字符识别 `Ocr` | 检测 ROI | 矩形 | 移动/缩放/旋转 | P2；旋转 ROI 必须由 OCR HALCON Region 消费 |
| AI 目标检测 `AiDetection` | 检测 ROI | 矩形 | 移动/缩放；旋转需先确认 AI bridge 是否接受 polygon/mask | P2；不能用外接矩形假装支持旋转 |
| AI 分类 `AiClassification` | 检测 ROI | 矩形 | 移动/缩放；旋转需先确认裁剪合同 | P2；若后端只接受矩形 crop，首版禁用旋转 |
| 模板定位 `TemplateLocation` | 模板 ROI、搜索 ROI | 模板矩形/多边形；搜索矩形/多边形/圆 | 按形状完整编辑 | P1 试点；模板变化使模型 dirty |
| 颜色识别 `ColorRecognition` | 检测 ROI、检测 Mask | 矩形/圆；Mask 多边形 | ROI 完整编辑；Mask 复用多边形编辑 | P2 |
| 颜色比较 `ColorComparison` | 模板 ROI、检测 ROI、模板/检测 Mask | 模板矩形；检测矩形/圆；Mask 多边形 | ROI 完整编辑；Mask 复用多边形编辑 | P1 试点；四层选中与层级最复杂 |
| 注册分类 `RegisteredClassification` | 测试检测 ROI | 矩形 | 移动/缩放/旋转 | P2；训练标注另见辅助窗口 |
| 注册目标检测 `RegisteredClassificationDetection` | 测试检测 ROI | 矩形 | 移动/缩放/旋转 | P2；训练标注另见辅助窗口 |
| 位置修正 `PositionCorrection` | 工具 Dialog 内无本地 ROI | 无 | 不接入 ROI 编辑器 | 其基准模板 ROI 位于“设置基准图”窗口 |

### 3.2 必须一起覆盖的辅助窗口

| 窗口 | ROI 角色 | 当前形状 | 计划 |
|---|---|---|---|
| `ReferenceImageDialog` | 位置修正模板 ROI | 矩形/多边形 | 完整编辑；任一变换都使旧基准姿态/私有模型失效 |
| `ColorTemplateDialog` | 颜色模板样本 ROI | 矩形，多样本 | 选中当前样本 ROI 后移动/缩放/旋转；重新裁剪并重建样本事实 |
| `RegisteredClassificationTrainingDialog` | 多图、多类别、多 ROI 标注 | 矩形/多边形 | 支持按编号命中、单选编辑、移动/缩放/旋转、删除 |
| `RegisteredClassificationDetectionTrainingDialog` | 多目标训练标注 | 矩形/多边形 | 同上，保持类别和目标编号绑定 |

### 3.3 不应误纳入的对象

- `MainWindow`、`ToolsDialog`、`OutputDialog` 中运行产生的 overlay 是结果展示，不是配置 ROI。
- 位置修正后的运行态 ROI 是基准 ROI 经 HALCON/公共矩阵变换后的结果，不应在运行态直接改写基准配置。
- 模板轮廓、匹配框、OK/NG 文字、得分和直方图不是 ROI 编辑对象。

## 4. 目标交互规范

### 4.1 状态模型

统一为三态：

1. `Idle`：普通浏览；滚轮缩放，左键拖动画布。
2. `Draw`：创建新 ROI；完成有效绘制后，单 ROI 工具自动进入 `Edit`，多 ROI 工具按业务设置决定继续绘制或进入编辑。
3. `Edit`：选中并调整已有 ROI；画布导航使用中键或 `Ctrl + 左键`，避免和 ROI 拖动冲突。

同一时刻只能有一个活动编辑目标。模板 ROI、检测 ROI、模板 Mask、检测 Mask 通过稳定的 target id 区分，不能仅依赖“当前画了哪种形状”猜测归属。

### 4.2 选中与命中

- 单击轮廓或填充内部选中 ROI。
- 多 ROI 重叠时，优先命中当前层级中最上方 ROI；重复单击可后续扩展循环选择。
- 选中对象使用实线高亮，未选对象降低透明度。
- 控制柄保持固定屏幕像素大小，不随图像缩放变得过大或过小。
- 点击空白取消选中，但不删除、不清空几何。
- 结果 overlay 不参与命中。

### 4.3 各形状控制柄

#### 旋转矩形

- 内部拖动：整体移动。
- 四角柄：同时改变长和宽；按住 `Shift` 等比缩放。
- 四边中点柄：只改变对应方向尺寸。
- 顶部独立旋转柄：绕中心旋转。
- `Shift + 旋转`：15° 吸附；角度显示统一为度。

#### 多边形

- 内部拖动：整体移动。
- 顶点柄：调整单顶点，保留现有能力。
- 选中包围框角柄：绕几何中心等比缩放所有顶点。
- 旋转柄：绕几何中心旋转所有顶点。
- 自交、面积过小或缩放退化时拒绝提交并恢复到本次手势开始值。

#### 圆

- 内部拖动或圆心柄：移动圆心。
- 圆周柄：调整半径。
- 不显示旋转柄。

#### 线带

- 中心区域拖动：整体移动。
- 两端柄：调整端点，从而改变长度和角度。
- 两侧宽度柄：调整带宽。
- 旋转等价于绕中心同步旋转两端点，可由旋转柄或拖端点完成；为贴近海康交互，建议保留独立旋转柄。

### 4.4 键盘、右键与事务

- `Delete`：删除当前选中 ROI；单 ROI 工具删除后恢复“无有效 ROI”或工具既有默认全图语义。
- 右键：提供“删除当前 ROI”“重画”“重置为全图”（仅支持全图的工具）。
- `Esc`：绘制中取消草图；变换中恢复手势开始值；已完成编辑态按一次 Esc 只退出选中。
- 鼠标释放时提交一次业务变更信号，拖动过程中可发预览信号但不得每帧触发建模或测试运行。
- Dialog 的“取消”恢复进入窗口前配置；“完成/确定”才提交配置。

## 5. 公共实现方案

### 5.1 几何合同

建议在 `src/frame/` 或 `src/toolcore/` 增加公共几何类型，避免各 Dialog 继续维护互不兼容的字段组合：

```cpp
struct RotatedRectRoi {
    QPointF centerNormalized;
    double halfWidthNormalized;
    double halfHeightNormalized;
    double angleDeg;
    bool valid;
};

struct RoiGeometry {
    QString id;
    RoiShapeType shape;
    RoiEditTarget target;
    RotatedRectRoi rotatedRect;
    CircleRoi circle;
    LineBandRoi lineBand;
    QVector<QPointF> polygonNormalized;
};
```

角度合同必须固定：

- 存储单位：度。
- 原点：ROI 中心。
- 图像坐标：X 向右、Y 向下。
- 正方向：建议 UI 采用顺时针为正；进入 HALCON 时统一转换为其笛卡尔坐标/弧度合同。
- 角度规范化：`[-180°, 180°)`。

不得直接复用位置修正 payload 中的 `angleDeg` 而不声明方向，因为位置修正和 ROI 编辑可能处于不同坐标约定。

### 5.2 配置向后兼容

采用“新增精确几何 + 保留旧字段”的渐进迁移：

- 旧 `roiNormalized` / `templateRoiNormalized` / `detectRoiNormalized` 继续写入精确几何的轴对齐包围框，用于旧版本回退和摘要显示。
- 新增按角色命名的精确几何对象，例如 `detectRoiGeometry`、`templateRoiGeometry`、`searchRoiGeometry`。
- 旧配置只有 `QRectF` 时按 `angleDeg = 0` 迁入。
- 新版 Adapter 优先读取精确几何；缺失时读取旧字段。
- 保存后不得丢失多边形、圆、线带现有专用字段；待所有消费者迁移完成后再决定是否统一清理，清理不属于本轮任务。

示例：

```json
{
  "type": "rotated_rect",
  "center": {"x": 0.42, "y": 0.51},
  "halfWidth": 0.18,
  "halfHeight": 0.09,
  "angleDeg": 27.5
}
```

### 5.3 编辑器职责拆分

建议保留 `FrameViewHelper` 作为图像视图与坐标转换入口，新增内部 `RoiEditorController`（名称可在实施时调整）：

- `FrameViewHelper`：图像、scene、缩放、平移、view/image/normalized 转换、结果 overlay。
- `RoiEditorController`：ROI 集合、选中、命中测试、控制柄、鼠标手势、边界约束、事务快照。
- Dialog：决定当前业务 target、允许的形状和数量、接收最终几何、更新配置与模型 dirty 状态。

建议的新接口方向：

```cpp
void setEditableRois(const QVector<RoiGeometry> &rois);
void setActiveRoiTarget(const QString &id, RoiInteractionMode mode);
void setAllowedRoiShapes(const QSet<RoiShapeType> &shapes);
QVector<RoiGeometry> editableRois() const;

signals:
void roiGeometryPreviewChanged(const RoiGeometry &geometry);
void roiGeometryCommitted(const RoiGeometry &before,
                          const RoiGeometry &after);
void roiDeleteRequested(const QString &id);
void roiSelectionChanged(const QString &id);
```

旧的 `setRoiDrawingEnabled()`、`roiChanged()` 等 API 首阶段保留适配，确保未迁移窗口行为不变。禁止一次性删除旧 API 并同时改造所有 Dialog。

### 5.4 屏幕绘制与命中细节

- 主轮廓使用 `QGraphicsPathItem` 或统一 path，旋转矩形先转四点 polygon 再绘制。
- 控制柄使用固定 8～10 px 命中半径，并设置 `ItemIgnoresTransformations` 或按 `viewScale` 反算场景尺寸。
- 命中优先级：控制柄 > 边 > 内部 > 空白。
- 控制柄只出现在活动 ROI 上。
- 变换计算基于手势开始时的不可变快照，避免累积浮点漂移。
- 几何提交前统一验证有限数值、最小 2 px 尺寸、最小多边形面积和图像边界。
- 画布适应、缩放、平移、窗口 resize 不发 ROI 变更信号。

## 6. HALCON 与业务链路要求

### 6.1 不允许 UI 假支持

以下任一链路未完成时，对应工具不得显示旋转柄：

`Dialog 几何 → ToolConfig → Adapter 解析 → HALCON Region/模型 → ToolResult overlay → 保存回显`

特别是 AI 分类、AI 检测等桥接工具，如果后端合同只支持轴对齐 crop，则首版只能提供移动和缩放。旋转能力应返回明确 unsupported 或在 UI 禁用，不能静默使用外接矩形。

### 6.2 HALCON 几何映射

实施前需在本机 HALCON 20.11 环境确认所需符号：

- 旋转矩形 Region：优先使用 `gen_rectangle2` 语义。
- 多边形 Region：使用现有 HALCON polygon/region 生成链路。
- 圆 Region：使用 `gen_circle` 语义。
- 线带测量：继续使用现有 `gen_measure_rectangle2`/rectangle2 几何合同。
- 位置修正：精确几何先在基准图坐标创建，再通过现有 `referenceToRunHomMat2D` 变换，显示 overlay 与算法 Region 共用同一矩阵。

若本机缺少所需 HALCON 符号，必须停止对应形状的算法接入并向用户确认，不得用 OpenCV 或自写像素算法替代。

### 6.3 模板失效与重建

模板 ROI 的移动、缩放、旋转、顶点调整都属于模板事实变化：

- `PatternPresence`
- `ContourPresence`
- `TemplateLocation`
- `ReferenceImageDialog` 中位置修正模板
- `ColorTemplateDialog` 样本 ROI

这些窗口在 `roiGeometryCommitted` 后应标记模型 dirty；拖动预览阶段不反复重建，鼠标释放后再更新缩略图/模型状态。用户必须显式重新创建模板或按现有自动重建合同执行。

## 7. 分阶段实施抓手

### 阶段 0：冻结合同与建立保护测试

涉及模块：

- `src/frame/FrameViewHelper.*`
- 新增公共 ROI geometry/serialization 文件
- `smoke/frame_view_helper_navigation_smoke.cpp`
- 新增 `smoke/frame_view_helper_roi_edit_smoke.cpp/.pro`

动作：

1. 固定坐标、角度、最小尺寸、边界和信号提交合同。
2. 为旧矩形、多边形、圆、线带绘制和导航补回归。
3. 为旧配置迁移和新配置往返序列化补测试。
4. 确认控制柄不会被 `setToolOverlays()` 清理或误命中。

完成证据：未迁移 Dialog 的旧行为与当前一致，导航 smoke 全部通过。

### 阶段 1：公共编辑器 MVP

动作：

1. 实现单选、移动、控制柄和事务快照。
2. 先完成轴对齐矩形移动/缩放。
3. 完成圆移动/半径、线带端点/宽度。
4. 把现有多边形顶点与整体移动迁入统一控制器。
5. 增加多边形包围框等比缩放、整体旋转。
6. 实现 Delete、右键删除和 Esc 取消。

试点窗口：

- `BlobPresenceDialog`：覆盖矩形、多边形、圆。
- `EdgePresenceDialog`：覆盖线带。
- `PatternPresenceDialog`：覆盖模板/检测 target 切换。
- `ColorComparisonDialog`：验证多角色层级与 Mask。

完成证据：四个试点窗口能绘制后直接调整、保存、回显；未启用旋转矩形的 runner 不出现旋转柄。

### 阶段 2：旋转矩形端到端

动作：

1. 增加 `RotatedRectRoi` 和 JSON 兼容读写。
2. 为每个 HALCON Adapter/runner 增加精确 Region 解析。
3. 更新 ToolOverlay，旋转矩形以四点 polygon 显示。
4. 更新位置修正消费层，确保基准旋转矩形经矩阵后仍为精确 polygon。
5. 每个工具通过算法 smoke 后才启用旋转柄。

建议接入顺序：

1. `TemplateLocation`
2. `PatternPresence`、`ContourPresence`
3. `BlobPresence`、`CirclePresence`
4. `Ocr`
5. `ColorRecognition`、`ColorComparison`
6. 注册分类与注册目标检测
7. AI 分类/AI 检测（以后端合同结论为准）

完成证据：同一旋转 ROI 的配置 polygon、HALCON Region 和结果 overlay 四角坐标在容差内一致。

### 阶段 3：剩余单 ROI Dialog 批量接入

按同类窗口分批：

- 有无类：Circle、Line、Contour。
- 识别类：CharacterRecognition、ColorRecognition。
- AI 类：ObjectDetection、Classification。
- 注册类测试窗口：RegisteredClassification、RegisteredClassificationDetection。
- 基准与定位：ReferenceImage、TemplateLocation。
- 颜色模板样本：ColorTemplate。

每批只替换 Dialog 的编辑状态与信号接入，不顺带重构算法参数、UI 布局或测试运行流程。

### 阶段 4：多 ROI 训练窗口

动作：

1. 为每个训练 ROI 分配稳定 id，id 与图片、类别、ROI 编号绑定。
2. 将当前图/当前类别 ROI 集合交给公共编辑器。
3. 点击 overlay 选中单个 ROI，编辑后只更新该 ROI。
4. 保持序号稳定；删除后是否重排沿用窗口现有合同。
5. 切图、切类别、进入 ROI 预览页前提交或取消当前手势。
6. 修改训练 ROI 后使旧模型/特征状态失效。

完成证据：重叠多 ROI 可选中正确对象，修改一个 ROI 不改变其他图片、类别或 ROI。

### 阶段 5：体验收口与文档

动作：

- 统一鼠标光标、控制柄颜色、tooltip、状态栏文案和快捷键。
- 在 `docs/FID/Function_Docs.md` 记录公共 ROI 编辑规范。
- 在各受影响 `docs/FID/<工具>/` 文档记录实际接入状态、配置字段、限制和验证结果。
- 对尚不支持旋转的后端明确标记限制。

## 8. 验证设计

### 8.1 公共几何自动化

`frame_view_helper_roi_edit_smoke` 至少覆盖：

- 矩形内部拖动后中心变化、尺寸不变。
- 四角/边柄缩放的锚点和最小尺寸。
- 旋转柄角度、15° 吸附、跨 ±180° 连续性。
- 圆移动与半径调整。
- 线带端点、宽度、整体移动。
- 多边形顶点、整体移动、等比缩放、旋转。
- 图像边界约束。
- 画布缩放/平移不改变 ROI 数据。
- 拖动过程中只有 preview，释放时只有一次 committed。
- Esc 回滚、Delete 删除、右键命中。
- 结果 overlay 不可选。
- 高 DPI 与 1x/8x 缩放下控制柄命中范围稳定。

### 8.2 配置与算法测试

每种工具需要验证：

1. 旧 `QRectF` 配置打开后角度为 0。
2. 新几何保存、关闭、重开后中心、尺寸、角度和点集一致。
3. Adapter 实际读取精确几何，不退回外接矩形。
4. HALCON Region 与配置 geometry 一致。
5. 位置修正开启后，移动、旋转、缩放方向正确。
6. 空图、无效 ROI、越界、缺模型、缺 HALCON 符号时返回明确错误且不崩溃。
7. 模板 ROI 变化后旧模型不能继续伪装有效。

### 8.3 Dialog 集成测试

已有 smoke 应扩展：

- `template_location_ui_smoke`
- `registered_classification_dialog_smoke`
- `registered_classification_detection_dialog_smoke`
- `color_comparison_dialog_integration_smoke`
- 颜色识别 GMM 生命周期 smoke
- Presence、OCR、位置修正相关 smoke

建议为没有 Dialog smoke 的工具补最小用例，至少验证：

- 进入绘制。
- 绘制完成自动进入编辑。
- 移动或缩放后 Dialog 成员配置更新。
- 完成保存。
- 重开回显。
- 测试运行使用修改后的 ROI。

### 8.4 构建与人工验收

每一批次都执行影子构建：

```bash
mkdir -p build
cd build
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
make -j$(nproc)
```

人工 GUI 验收必须在可交互桌面执行，录屏或截图保留以下证据：

- 1x、放大和拖动画布后三类操作仍准确。
- 模板/检测 ROI 样式和选中层级正确。
- 矩形旋转时算法实际区域与显示框一致。
- 多 ROI 编号、选中、删除对象正确。
- 测试运行、停止、重新编辑之间不会残留错误控制柄。
- 普通操作不误触发 Dialog 关闭、程序退出或无效保存。

## 9. 风险与控制

| 风险 | 控制方式 |
|---|---|
| UI 已旋转但算法仍用外接矩形 | 旋转柄按工具 feature gate；端到端 smoke 通过后开启 |
| `FrameViewHelper` 事件继续膨胀 | 把选中/命中/变换拆入 ROI controller，保留 helper 的视图职责 |
| 导航与编辑争夺左键 | 明确 Idle/Draw/Edit；编辑态用 Ctrl+左键或中键导航 |
| 多边形缩放产生自交或退化 | 提交前验证；无效时回滚手势快照 |
| 老方案打不开 | 旧字段继续读取；新增字段为可选；旧 QRectF 迁入角度 0 |
| 模板编辑后旧模型仍有效 | committed 信号统一触发 dirty，预览信号不建模 |
| 多 ROI 修改错对象 | 稳定 id + target + image/class 上下文，不用列表下标单独标识 |
| 位置修正方向或角度约定混乱 | 单独坐标合同测试，验证 reference-to-run 方向 |
| 用户现有改动被覆盖 | 分批改动，实施前逐文件检查工作树；不清理、不覆盖无关修改 |

## 10. 推荐实施顺序与里程碑

推荐不要一次改完所有 Dialog，按以下里程碑交付：

1. **M1 公共编辑器可用**：矩形移动/缩放、圆、线带、多边形编辑；不开放旋转矩形。
2. **M2 四个试点闭环**：Blob、Edge、Pattern、ColorComparison 保存回显与测试运行通过。
3. **M3 旋转矩形 HALCON 闭环**：TemplateLocation 首个通过，再按 runner 批量迁移。
4. **M4 全部单 ROI 工具接入**：工具库所有具备本地 ROI 的 Dialog 完成。
5. **M5 多 ROI 训练接入**：两类注册训练窗口和颜色模板样本完成。
6. **M6 GUI 回归与文档收口**：主工程构建、smoke、人工交互和功能文档齐全。

建议把 M1、M2 作为第一开发批次。它能先解决大部分“画完不能再调”的痛点，同时把旋转矩形的算法风险隔离在 M3，不会产生表面可用、实际检测错误的半成品。

## 11. 执行记录

### 2026-07-25：M1 第一批——公共矩形移动与缩放

已完成：

- 新增 `src/frame/RoiEditorController.h`，抽取矩形命中、整体移动、八方向缩放、最小尺寸、图像边界和手势回滚的纯几何逻辑。
- `FrameViewHelper` 接入公共控制层，保留原有 `setRoiDrawingEnabled()`、`setRoiRectNormalized()` 和 `roiChanged(QRectF)` 接口。
- 矩形编辑态显示 8 个固定屏幕尺寸控制柄。
- 矩形内部拖动可整体移动；四角和四边控制柄可缩放。
- 鼠标释放只提交一次 `roiChanged`；拖动过程只更新预览。
- `Esc` 可回滚当前未提交变换；退出编辑态保留最后一次已提交 ROI。
- 尚未开放矩形旋转，配置和 HALCON runner 语义保持不变。

新增验证：

- `smoke/frame_view_helper_roi_edit_smoke.cpp`
- `smoke/frame_view_helper_roi_edit_smoke.pro`

验证结果：

- `frame_view_helper_roi_edit_smoke`：通过。
- `frame_view_helper_navigation_smoke`：通过。
- 主工程 `qmake qt_ui_test.pro` + `make -C build -j2`：通过。
- `git diff --check`：通过。

下一批：

1. 将圆的圆心移动与半径控制柄接入公共控制层。
2. 将线带的整体移动、端点和宽度控制柄接入公共控制层。
3. 把现有多边形顶点/移动逻辑迁入控制层，并增加整体缩放。
4. 在四类几何稳定后进行 Blob、Edge、Pattern、ColorComparison 的 GUI 试点检查。

### 2026-07-25：M1 第二批——圆与线带编辑

已完成：

- 新增 `src/frame/RoiGeometry.h`，将 `RoiShapeType`、`RoiEditTarget`、`CircleRoi`、`LineBandRoi` 从 `FrameViewHelper.h` 抽到公共几何层；`FrameViewHelper.h` 继续包含该头文件，现有 Dialog 无需修改 include。
- `RoiEditorController` 增加圆和线带的纯几何命中、变换、边界限制及回滚。
- 圆 ROI 支持拖动圆内区域移动圆心、拖动圆周控制柄调整半径。
- 线带 ROI 支持整体移动、分别拖动两个端点调整长度和角度、拖动法向宽度柄调整带宽。
- 圆和线带控制柄使用固定屏幕尺寸，画布缩放不改变命中视觉大小。
- 保留原有 `circleChanged(CircleRoi)` 和 `lineBandChanged(LineBandRoi)` 信号；拖动预览不提交，鼠标释放提交一次。
- `Esc` 可分别回滚圆或线带的当前手势。

验证结果：

- `frame_view_helper_roi_edit_smoke`：通过，已新增圆移动、圆半径、线带移动、端点和宽度的真实鼠标事件覆盖。
- `frame_view_helper_navigation_smoke`：通过。
- 主工程增量 `make -C build -j2`：通过，随后再次执行为无待构建目标。
- `git diff --check`：通过。
- `template_location_ui_smoke`：源码编译通过，但链接失败；失败符号来自该 smoke `.pro` 未纳入已有位置修正实现文件（`PositionCorrectionTransform`、`PositionCorrectionHalconTransform`、`PositionCorrectionHalconRunner`、`PositionCorrection`），与本次 ROI 编辑代码无关。本批未扩大范围修改该历史 smoke 工程清单。

下一批：

1. 将多边形顶点命中、整体移动迁入 `RoiEditorController`。
2. 增加多边形整体等比缩放控制柄和手势回滚。
3. 处理“持续重画”工具在已有多边形上的编辑优先级。
4. 在稳定后再设计多边形整体旋转；旋转矩形仍等待配置与 HALCON 全链路。

### 2026-07-25：ROI 外部重画安全区

按交互补充要求完成：

- 矩形、圆和线带在已有 ROI 外部支持直接重新绘制。
- 以当前 ROI 轮廓向外 16 个屏幕像素作为安全区；安全区内不会误触重画。
- 鼠标距离轮廓超过 16 px 后，按下并拖动进入原有新建流程，新 ROI 替换旧 ROI。
- 16 px 使用 viewport 到 scene 的换算，画布缩放后视觉距离保持一致。
- 圆按圆周外距离计算；线带按中心线距离、带宽一半和外部安全距离共同计算。

验证结果：

- 专项 smoke 覆盖矩形外 10 px 不重画、超过 16 px 可重画，以及圆/线带公共距离判定：通过。
- `frame_view_helper_navigation_smoke`：通过。
- 主工程 `make -C build -j2`：通过。
- `git diff --check`：通过。

### 2026-07-25：默认全图 ROI 直接重画

问题与处理：

- 多个工具使用 `(0, 0, 1, 1)` 表示默认算法范围为全图；公共编辑器此前将其识别为普通矩形，导致整个画布都命中“移动”，用户必须先缩小 ROI 才能重画。
- 公共编辑层现在识别覆盖全图的矩形 ROI：鼠标位于内部时显示十字光标并直接开始新 ROI 绘制。
- 全图 ROI 的八个边缘/角点控制柄继续保留缩放能力，不改变保存配置或算法对全图范围的解释。
- 非全图 ROI 继续保持内部拖动移动、控制柄缩放、轮廓外 16 px 安全区及更远位置重画的既有规则。

验证结果：

- 专项 smoke 新增“全图 ROI 内部直接重画”真实鼠标事件：通过。
- 普通矩形移动、缩放、Esc 回滚及外部安全区用例：通过。
- `frame_view_helper_navigation_smoke`：通过。
- 主工程 `make -C build -j2`：通过。
- `git diff --check`：通过。
