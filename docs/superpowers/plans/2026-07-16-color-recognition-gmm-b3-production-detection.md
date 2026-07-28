# 颜色识别 GMM B3 生产检测实施计划

规格：`docs/superpowers/specs/2026-07-16-color-recognition-gmm-b3-production-detection-design.md`

## 1. 强类型合同与签名

- 在 `ColorRecognitionHalconRunner.h` 增加 GMM 检测配置、类别测量和运行结果。
- 为 B1/B3 提供同一份 buildParamsHash 复算合同，检测前校验模型版本、类别顺序和中心诊断。
- 为 Runner 增加 `runGmmModel()` facade，保持 HSV `run()` 不分支。

## 2. HALCON 检测

- 扩展 Gmm runtime profile：分类、ROI/mask、domain、intersection、area_center。
- 复用 8/16 位 RGB real → CIELAB 转换和严格产物反序列化。
- 构造矩形/圆形/mask effectiveRegion，执行 `classify_image_class_gmm`。
- 按 classIdOrder 统计每类面积、未分类面积、score、纯度和覆盖率。
- 实现完整判定、稳定 failure reasons 和轻量 overlay。

## 3. Adapter 与配置

- 解析活动模板 GMM 模型、类别顺序、产物、阈值和输入元数据。
- GMM ready/ready_with_warning 实际运行；其他状态返回明确错误。
- HSV 保持原路径；任何 GMM 错误禁止 fallback。

## 4. Dialog/UI

- 在颜色识别“全部”参数中加入 rejection、最低类别置信度和最低分类覆盖率。
- judgeRule 保存 expectedClassId，并兼容唯一 expectedLabel 迁移。
- GMM 结果显示预测类、score、类别纯度、覆盖率和未分类占比。

## 5. 验证

- 新增 GMM B3 production smoke：纯色、混色面积、rejection、四项判定、ROI/mask、模型异常和 Adapter 派发。
- 回归 GMM B1、B2 lifecycle、HSV Phase A 和 Qt 主工程。
- 检查颜色比较、注册分类无 B3 依赖，生产代码无 OpenCV 核心算法。
