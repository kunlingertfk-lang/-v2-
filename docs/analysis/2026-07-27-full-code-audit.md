# 全量代码审计报告（2026-07-27）

## 1. 审计结论

本次基于当前工作区快照进行只读代码审计、严格编译和可执行测试，未修改业务代码。

- 审计规模：`src/` 下 80 个 `.cpp`、89 个 `.h`，合计 77,285 行；20 个 Qt Designer `.ui`；35 个 smoke/test `.pro`。
- 主工程 Release 构建通过。
- 增加 `-Wpedantic -Wshadow -Wconversion -Wsign-conversion` 的严格构建通过，项目自身产生 57 条告警。
- ASan + UBSan 主工程构建通过；`QT_QPA_PLATFORM=offscreen` 启动 10 秒未报告内存越界、UAF 或未定义行为。
- 已执行且通过的独立冒烟/回归共 27 项。
- 发现 1 组严重安全缺陷、4 组高风险运行/数据一致性缺陷、5 组中风险缺陷，以及测试工程和未完成功能方面的工程问题。

审计不是对所有设备、模型和人工交互组合的穷举证明。真实相机阻塞、RK 远程推理、外部 ONNX/HDL 模型和人工 ROI 操作仍需在目标环境补充动态验证。

## 2. 严重和高风险问题

### CRITICAL-01：登录认证等同于明文固定口令

证据：

- `src/LoginWindow.cpp:53-55` 将密码框预填为 `123456`。
- `src/LoginWindow.cpp:92-100` 提供管理员、工程师、操作员三个角色。
- `src/LoginWindow.cpp:102-116` 仅比较同一个硬编码字符串 `123456`，通过后直接把所选角色作为显示信息传给主窗口。

影响：

- 可从二进制或源码直接获得口令，而且用户打开程序时无需输入。
- 三个角色没有独立身份、凭据或权限校验，选择“管理员”即可获得同样会话。
- 若该程序用于产线控制，登录页不能构成安全边界。

建议：

- 明确产品是否需要真实认证。若不需要，应移除误导性的“安全登录”语义；若需要，应接入受控账号源。
- 禁止客户端保存明文口令；采用加盐密码散列或服务端认证。
- 建立角色到操作权限的强制映射，并补失败次数限制、审计日志和会话生命周期。

### HIGH-01：相机线程超时后强制终止，可能死锁或破坏采集资源

证据：

- `src/frame/CameraFrameProvider.cpp:121-137` 在采集线程 3 秒未退出时调用 `QThread::terminate()`。
- `src/frame/CameraFrameProvider.cpp:419-443` 的线程可能正持有 `m_cameraMutex` 并阻塞在 `VideoCapture::grab/retrieve`。

影响：

- 在线程持锁时强杀可能让互斥锁永远保持锁定；之后关闭或重开相机时可能死锁。
- OpenCV/驱动资源可能在任意指令点被中断，产生不可预测状态或进程崩溃。

建议：

- 不使用 `terminate()`；让抓帧调用具备可中断/有界超时能力。
- 将设备关闭或取消采集设计为协作式停止，并保证线程退出后再销毁资源。
- 增加“驱动永久阻塞”专项测试，并验证关闭、切换方案、退出程序和重连。

### HIGH-02：保存失败后仍切换页面，用户修改可能丢失

证据：

- `src/ReferenceImageDialog.cpp:286-307` 的 `saveCurrentScheme()` 返回 `void`，验证或保存失败只能提前返回。
- `src/ReferenceImageDialog.cpp:1398-1414` 无论保存是否成功都替换当前对话框。
- `src/CameraParamsDialog.cpp:178-186` 与 `210-225` 存在同样问题。
- `src/OutputDialog.cpp:260-279` 忽略 `commitOutputStateToScheme(true)` 的返回值并继续切页。
- `src/MainWindow.cpp:576-581` 忽略 `persistCurrentSchemeState(...)` 的结果并打开设置页。

影响：

- ROI 验证失败、磁盘只读、空间不足或 JSON 写入失败时，界面仍离开当前页。
- 内存状态、屏幕状态和磁盘方案可能不一致，用户误以为保存成功。

建议：

- 所有保存函数统一返回 `bool`/错误对象。
- 页面切换仅在提交成功后发生；失败时保留当前页和编辑状态。
- 为磁盘不可写、ROI 未完成、基准图写入失败补导航回归。

### HIGH-03：方案保存不是事务，且保存任意状态时错误依赖全局基准图

证据：

- `src/SchemeStore.cpp:404-421` 先更新全局 `ReferenceImageProvider` 和当前方案，再落盘；落盘失败时没有回滚。
- `src/SchemeStore.cpp:499-517` 的 `saveSchemeToFile(const SchemeState &state)` 不使用该状态自己的图像来源，而总是读取全局当前基准图。
- `src/SchemeStore.cpp:216-242` 的“另存为”也依赖这个全局图像。
- `src/SchemeStore.cpp:509-517` 在 `referenceImagePath` 非空但全局图像为空时不写图，却仍可写出引用 `reference.png` 的 JSON。

影响：

- 图像写入或 JSON 提交失败后，内存/UI 已切换到新基准图，磁盘仍是旧版本。
- 公共 `saveScheme(state)` 若将来保存非当前方案，可能用当前方案的基准图覆盖目标方案。
- “另存为”可能生成引用不存在图片的方案。

建议：

- 将基准图作为保存输入的一部分，禁止从无关全局单例隐式取值。
- 图片与 JSON 采用临时文件/临时目录写入，全部成功后原子切换。
- 失败时恢复 `m_currentScheme` 和 `ReferenceImageProvider`。

### HIGH-04：目标检测界面承诺的参数与真实推理行为不一致

证据：

- `src/ObjectDetectionDialog.cpp:508-510` 的“测试运行”仍提示算法未接入，但运行引擎已经注册 `AiDetectionAdapter`。
- `src/tooladapters/AiDetectionAdapter.cpp:126-135` 无论界面选择什么模型，都固定使用 `/home/cat/model/yolov8_red_black.rknn`、固定标签和 2 类。
- `src/tooladapters/AiDetectionAdapter.cpp:142-155,209` 读取 NMS 阈值但不传给远端脚本。
- `src/algorithms/ai/AiDetectionRunner.cpp:624-631` 角度过滤、出界过滤和排序只生成 warning，不应用配置。
- Runner 之后仍可返回 `success=true`。

影响：

- 用户保存的模型、NMS、角度、边界和排序配置不等于实际执行配置。
- 结果可能以“成功”呈现，但判断语义与用户设置不同，属于工业检测中的静默误判风险。

建议：

- 未支持的控件禁用或在 Adapter 层返回明确 `unsupported_*`，不能只写 payload warning。
- 建立模型名到受控模型包的真实映射，模型不存在时失败。
- 让对话框测试按钮走与工具链相同的 Adapter/Runner。

## 3. 中风险问题

### MEDIUM-01：工具新增/编辑忽略落盘失败

证据：

- `src/ToolsDialog.cpp:528-545` 先修改 `SchemeStore` 单例，再保存；失败时不回滚。
- `src/ToolsDialog.cpp:748-756` 新增工具后忽略 `commitToolStateToScheme(true)` 返回值并返回成功。
- `src/ToolsDialog.cpp:823-834` 编辑工具存在同样问题。

影响：界面列表和内存方案已改变，但磁盘未改变；再次打开方案时配置“消失”。

### MEDIUM-02：AI 临时图像长期残留，且请求 ID 可能碰撞

证据：

- `src/algorithms/ai/AiDetectionRunner.cpp:747-763` 仅以当前毫秒作为请求 ID，在持久临时目录生成输入和结果图。
- 成功和多数失败路径均未清理本地输入、本地结果或远端输入。

影响：

- 并发请求在同一毫秒可能互相覆盖。
- 产线图像长期积累，带来磁盘占满和图像数据泄露风险。

建议：使用 UUID/安全临时目录；通过作用域清理器覆盖所有退出路径；仅在明确调试开关开启时保留文件，并设置容量/保留期。

### MEDIUM-03：字符识别小数配置重新打开后被截断

证据：

- `ui/CharacterRecognitionDialog.ui` 将字符面积和宽高声明为 `QDoubleSpinBox`。
- `src/CharacterRecognitionDialog.cpp:216-221` 保存这些参数的浮点值。
- `src/CharacterRecognitionDialog.cpp:298-303` 回显时却调用 `QJsonValue::toInt(...)`。
- 严格编译对应产生 6 条 `-Wfloat-conversion` 告警。

影响：例如 `12.5` 会回显成 `12`，保存—重开不满足配置闭环。

### MEDIUM-04：方案路径缺少根目录约束

证据：

- `src/SchemeStore.cpp:312-315,342-349` 直接将 JSON 中的 `referenceImagePath` 与方案目录拼接，没有 canonical path/根目录校验。
- `src/SchemeStore.cpp:638-648` 保留调用方传入的任意非空 `schemeDir`。
- `saveScheme(const SchemeState &state)` 是公共接口。

影响：被修改的本地方案 JSON 可用绝对路径或 `../` 读取方案目录外图片；错误使用公共保存接口可向项目根之外写文件。当前未发现外部调用 `saveScheme(state)`，因此实际暴露面有限，但接口不变量不安全。

### MEDIUM-05：多个可配置能力被静默忽略或降级

代表性证据：

- `src/algorithms/presence/ContourPresenceHalconRunner.cpp:920-966` 将明确标识为 unsupported 的检测区域降级成包围矩形并继续。
- `src/algorithms/presence/EdgePresenceHalconRunner.cpp:287-313` 标记极性、线带和 mask 未应用。
- `src/algorithms/presence/LinePresenceHalconRunner.cpp:793-799` 角度范围和 mask 未应用。
- `src/algorithms/presence/CirclePresenceHalconRunner.cpp:357-389,790-792` edge type、caliper mode、score threshold 和 mask 未应用。
- `src/algorithms/presence/PatternPresenceHalconRunner.cpp:792-859` sort、mask 和多种 overlay 能力未应用。

影响：旧配置、手工修改配置或仍可操作的 UI 控件可能产生“运行成功但参数未生效”的结果，违反工程中“未支持能力必须明确返回 unsupported”的约束。

建议：逐项建立 UI → `ToolConfig` → Adapter → Runner → payload 的契约表；未闭环项在 UI 禁用，并在 Adapter/Runner 明确失败。

## 4. 工程与测试问题

### 4.1 冒烟测试工程已与源码依赖漂移

以下 5 个测试工程当前不能完成构建：

1. `smoke/halcon_runtime_smoke.pro`：缺少 `PositionCorrectionTransform.cpp` 和 `PositionCorrectionHalconTransform.cpp`。
2. `smoke/color_comparison_smoke.pro`：测试源码仍引用已删除的旧 ColorComparison API/字段。
3. `smoke/color_comparison_feature_diagnostics_smoke.pro`：缺少位置修正 transform 源文件。
4. `tests/color_recognition_gmm_b2_lifecycle_smoke.pro`：缺少 `PositionCorrectionConsumer.cpp`。
5. `smoke/registered_classification_detection_dialog_smoke.pro`：缺少训练 runner/session/model package、`FrameInputMetadata` 等新依赖。

这会导致 CI 无法覆盖对应功能，不代表被测产品代码本身已确认失败，但属于回归防线失效。

以下 3 个项目是需要外部夹具/模型的工具，不属于自包含自动回归：

- `registered_classification_embedding_inference_smoke`
- `registered_classification_public_backbone_probe`
- `registered_classification_public_backbone_provision`

### 4.2 严格编译告警

项目自身共 57 条：

- 47 条 `-Wconversion`
- 6 条 `-Wfloat-conversion`
- 1 条 `-Wsign-conversion`
- 3 条 `-Wshadow`

除已确认的 OCR 小数截断外，大部分是 HALCON `Hlong/INT4_8`、`qint64` 向 `int/double` 转换。常规图像尺寸通常不会触发，但缺少范围校验，超大对象数量或异常 tuple 长度时存在截断/溢出风险。

### 4.3 已暴露但未形成运行闭环的功能

- `AiClassification` 和 `RegisteredClassificationDetection` 在工具库/对话框中可见，但部分运行链路仍明确标记“第一版未实现”或缺少 Adapter 注册。
- 多个 ROI/mask 控件仍是占位入口。

建议在发布版本中隐藏或禁用未完成入口，并展示版本能力说明；不要让用户创建一个只能在运行时得到 unsupported 的工具。

## 5. 验证记录

### 5.1 已通过

- 主工程 Release：qmake + make。
- 严格主工程：qmake + make，启用额外警告选项。
- ASan + UBSan 主工程：构建通过；无界面启动 10 秒，无 sanitizer 报告。
- `git diff --check`：通过。
- 冒烟/回归 27 项通过，包括：
  - FrameViewHelper 导航与 ROI 编辑；
  - 位置修正核心、UI、后端、消费者变换、Engine context、私有模型及 presence/OCR 契约；
  - Blob/Circle/ColorComparison 的位置修正；
  - ColorComparison feature view 和对话框集成；
  - ColorRecognition phase A、GMM build、GMM detection；
  - TemplateLocation runner 与 UI；
  - AI detection bridge；
  - RegisteredClassification Adapter、dataset、dialog、DL capability、feature V2、KNN backend、position correction。

`ai_detection_bridge_smoke` 和 `color_comparison_dialog_integration_smoke` 首次在自定义构建目录运行时因相对路径假设失败；从工程根目录按预期布局复跑均通过，已从产品失败项中排除。

### 5.2 未覆盖或需要目标环境

- 相机驱动永久阻塞、拔插、重连和退出。
- 登录后的完整人工页面遍历及全部 ROI 手势。
- RK 板 SSH/SCP、真实模型和远端脚本。
- 外部 ONNX/HDL 模型的 registered classification backbone 工具。
- 磁盘满、只读文件系统、断电等故障注入。
- HALCON license 缺失、符号缺失和各类真实工业图像边界数据的全组合。

## 6. 建议修复顺序

1. 明确并修复登录认证/角色权限边界。
2. 移除相机线程 `terminate()`，建立可中断关闭协议。
3. 统一所有页面的事务式保存和失败阻断导航。
4. 修复 `SchemeStore` 基准图事务及状态隔离。
5. 让目标检测和 presence 工具对不支持参数明确失败，禁止静默忽略。
6. 修复 OCR 小数回显、AI 临时文件和方案路径约束。
7. 修复 5 个失效测试工程，并把关键失败注入纳入 CI。
8. 分批清理类型转换告警。

## 7. 工作区说明

审计开始时工作区已有大量未提交修改和未跟踪文件；这些均视为用户现有工作并予以保留。本报告只新增当前文档，未对业务源码进行修复或重构。
