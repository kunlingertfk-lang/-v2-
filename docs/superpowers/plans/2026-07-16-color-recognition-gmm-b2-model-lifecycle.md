# 颜色识别 GMM B2 模型生命周期实施计划

规格：`docs/superpowers/specs/2026-07-16-color-recognition-gmm-b2-model-lifecycle-design.md`

## 1. 数据结构与 JSON

- 扩展 `ColorRecognitionSampleData`，保存稳定 sampleId、无损 GMM ROI PNG、规范哈希和输入位深元数据。
- 扩展 `ColorRecognitionTemplateData`，保存 recognitionBackend、独立 HSV/GMM 配置以及 GMM 模型产物和诊断。
- 更新 `ColorRecognitionDialog.cpp` 的模板/样本 JSON 编解码、旧 HSV 模板迁移、ToolConfig 回写。
- 对 ready GMM 在加载时执行 Base64/size/hash/HALCON 反序列化校验。

## 2. 样本采集与状态

- 在 `ColorTemplateDialog` 从原始 cv::Mat 裁剪 ROI，并用 PNG 无损编码保留 8/16 位数据。
- 生成 UUID sampleId 和规范像素 SHA-256，保存 pixelFormat/validBits/bitShift。
- 将 HSV stale 与 GMM stale 拆开；公共标签/样本变化更新两者，后端专属参数只更新对应模型。

## 3. GMM 建模 UI

- 在模板信息区加入识别后端选择。
- 复用“亮度参与”控件：HSV 映射 V，GMM 映射 ab/lab。
- 增加 GMM 模型状态、只读诊断和建立/重建按钮。
- 将保存的 ROI 样本转换为 B1 build samples，调用 HALCON GMM 建模并保存产物。
- 建模失败保留诊断且不产生可用模型；保存 empty/stale/failed 时明确提示。

## 4. Adapter 边界

- 解析活动模板 recognitionBackend。
- HSV 继续现有运行链。
- GMM 固定返回 unsupported_gmm_detection_phase_b2；未知后端返回 unsupported_recognition_backend。
- 禁止任何 fallback。

## 5. 验证

- 新增 B2 lifecycle smoke，覆盖 JSON roundtrip、8/16 位无损 PNG、stale 隔离、GMM 建模和 Adapter unsupported。
- 运行 GMM B1 smoke、HSV Phase A smoke和 Qt 主工程影子构建。
- 静态检查颜色比较/注册分类未引入 GMM B2 依赖。
