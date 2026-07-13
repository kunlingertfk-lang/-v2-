# 颜色比较 V2 设计决策记录

## 状态

- 日期：2026-07-13
- 状态：用户已确认
- 当前完整功能规范：`docs/FID/ColorComparison/颜色比较V2设计说明.md`

## 已确认决策

1. 颜色比较 V2 使用固定 32×32 H/S 联合直方图，layout 为 `hue_major`。
2. `histo_2dim(Region, H, S)` 必须按 row=Hue、column=Saturation 解释。
3. 相似度使用 HALCON `tuple_min2 + tuple_sum` 直方图交集；同一分布为 100 分。
4. 删除伪 Bhattacharyya、单主峰覆盖、C++ Gaussian kernel、经验权重和 OpenCV `compareHist` 生产旁路。
5. “亮度使能”改为“光照补偿”，默认关闭；它负责受限亮度归一化，不把 V 差异直接加入扣分。
6. 灰度来源按智能相机语义返回无效 NG；必须保留原始像素格式元数据，不能只检查归一化后的 BGR `cv::Mat`。
7. 模型升级为 version 2，具有状态、维度、layout、归一化、参考图哈希、提取参数哈希和输入签名。
8. V1 裸特征不直接迁移；有参考图时 stale 并重新取样，无参考图时 `model_rebuild_required`。
9. 基础/全部只控制 UI 显隐，不改变配置或模板状态。
10. 色谱特征本阶段禁用并返回 unsupported；后续可单独研究 `class_2dim_sup`，但不宣称复刻海康内部算法。
11. 位置修正本阶段不实现，只保留接口；旧配置请求时继续原始 ROI 并输出未应用 warning。
12. 颜色比较采用独立 V2 提取路径，本轮不修改颜色识别现有行为。

## 已知阻塞

- 本机 HALCON 符号能力已确认，但 licensed smoke 当前报 license `#2036`。
- 在 licensed smoke 通过前，可以完成配置、模型和无 license 契约测试，但不能宣称算法数值验证完成。
