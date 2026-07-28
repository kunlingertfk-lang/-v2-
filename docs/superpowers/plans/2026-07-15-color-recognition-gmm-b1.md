# 颜色识别 CIELAB GMM 阶段 B1 实施计划

**Goal:** 在不改变现有 HSV 生产链和颜色比较代码的前提下，实现 HALCON 20.11 CIELAB GMM 核心建模、内存序列化和严格产物验证。

**Architecture:** `ColorRecognitionHalconRunner` 暴露强类型建模入口，内部委托独立的 `ColorRecognitionGmmHalconBackend`。GMM 使用独立 runtime symbol profile；所有 GMM/serialized handle 由调用线程内 RAII 管理。

**Tech Stack:** C++17、Qt 5.15.2 Core/Gui、OpenCV `cv::Mat` 输入容器、HALCON 20.11 C API 动态加载、qmake。

## 全局约束

- 核心颜色转换、位深处理、区域抽样、GMM 训练和序列化只使用 HALCON。
- 不实现 GMM 检测、Adapter、UI 或 `recognitionBackend` 派发。
- 不修改颜色比较生产代码，不抽公共颜色算法底座。
- 保持现有 HSV smoke 通过，缺 GMM 符号不得阻断 HSV。
- 使用影子构建；构建产物不提交。

### Task 1：建立强类型接口和 RED smoke

**Files:**

- Modify: `src/algorithms/recognition/ColorRecognitionHalconRunner.h`
- Create: `src/algorithms/recognition/ColorRecognitionGmmHalconBackend.h`
- Create: `tests/color_recognition_gmm_build_smoke.cpp`
- Create: `smoke/color_recognition_gmm_build_smoke.pro`

- [ ] 定义 label、sample、build config、class diagnostics、artifact 和 build result 类型。
- [ ] 在 `ColorRecognitionHalconRunner` 声明 `buildGmmTemplateModel()`。
- [ ] 先写两类纯色、输入校验、状态和签名 smoke，确认缺实现时编译失败。

### Task 2：实现独立 Gmm runtime profile

**Files:**

- Create: `src/algorithms/recognition/ColorRecognitionGmmHalconBackend.cpp`
- Modify: `src/algorithms/recognition/ColorRecognitionHalconRunner.cpp`
- Modify: `qt_ui_test.pro`
- Modify: `smoke/color_recognition_gmm_build_smoke.pro`

- [ ] 加载 Common/Gmm 所需 HALCON 20.11 符号。
- [ ] tuple 版本使用 `T_create_class_gmm`、`T_train_class_gmm`。
- [ ] 标量 handle 接口加载 add/serialize/deserialize/get/create/clear。
- [ ] 缺符号返回 `halcon_symbol_profile_missing` 并列出符号名。
- [ ] 添加 GMM、serialized item 和 iconic object RAII 清理。

### Task 3：实现样本校验和 CIELAB 输入链

- [ ] 校验至少两类、classId/label 唯一映射、每类样本和稳定 sampleId。
- [ ] 计算逐行有效像素的 canonical SHA-256 并验证 `imageSha256`。
- [ ] 校验 ROI、8/16 位、三/四通道和 pixelFormat。
- [ ] 使用 HALCON 完成 16 位右移、范围检查、real `[0,1]` 映射和 CIELAB 转换。
- [ ] alpha 明确丢弃，Mono 和非法元数据返回稳定错误。

### Task 4：实现确定性等量采样和 GMM 训练

- [ ] 按 classId、sampleId 稳定排序。
- [ ] 统计每 ROI 可用像素并计算类间相等目标数。
- [ ] 实现类内均分、小 ROI 配额重分配。
- [ ] 使用 `get_region_points`、等间索引和 `gen_region_points` 构造无重复采样 region。
- [ ] 按 ROI 次数生成每类中心范围和 Ready/ReadyWithWarning。
- [ ] create/add/train GMM，输出每类采样诊断。

### Task 5：实现严格签名和内存模型产物

- [ ] 生成 canonical `trainingDataHash` 和 `buildParamsHash`。
- [ ] serialize GMM，将 serialized item 指针立即复制到 `QByteArray`。
- [ ] 校验 8 MiB 上限、size、SHA-256 和 Base64 往返。
- [ ] 通过 deserialize 完成产物可读性验证并释放验证 handle。
- [ ] 失败不返回 Base64 半成品。

### Task 6：补齐回归与构建验证

- [ ] 覆盖 ab/lab、1/3/5/10 ROI、重复建模、8/16 位和错误输入。
- [ ] 覆盖序列化篡改和缺 GMM 符号诊断。
- [ ] 运行 GMM build smoke 和现有 HSV phase A smoke。
- [ ] 使用 HALCONROOT=/opt/halcon 运行 qmake 与 Qt 主工程构建。
- [ ] 更新 V2 实施状态和提示词字段事实源，只标记实际完成能力。
