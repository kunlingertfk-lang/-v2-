# 颜色比较 V2 设计决策记录

## 状态

- 日期：2026-07-13
- 状态：用户已确认
- 当前完整功能规范：`docs/FID/ColorComparison/颜色比较V2设计说明.md`

## 已确认决策

1. 颜色比较 V2 使用固定 32×32 H/S 联合直方图，layout 为 `hue_major`。
2. `histo_2dim(Region, H, S)` 固定使用 `ImageCol=Hue`、`ImageRow=Saturation`；按 `row=Saturation,column=Hue` 读取，并以 `hueBin*32+saturationBin` 组织 hue-major 模型。
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
13. `templateRegionMode="custom"` 时，检测 ROI/圆形/检测 Mask 变化不使模型失效；`templateRegionMode="sync"` 时，检测区域形状、几何和检测 Mask 都属于模板提取参数，任一变化都使模型 stale。sync 建模先应用同步检测 Mask，再叠加模板专用 Mask。
14. 圆形半径统一为 `radiusPixels/max(width,height)`；Adapter、Runner、overlay、payload 和跨宽高比 sync 预览使用同一合同，越界圆在 UI 侧立即拒绝。
15. ready 模型加载和运行前都核对当前参考图哈希；参考图变化或清空立即进入 `model_stale`。
16. ready 输入签名只接受 `color|unknown`、8-bit 和实现支持的彩色像素格式；原生 stale 的非空 payload 仍执行完整维度、有限非负、归一化和元数据校验。
17. 已有配置的 `colorComparison` 缺失/非对象、`version` 缺失/非整数/越界，以及 V2 非法字段或畸形非 ready 模型，均只读原样保留；只有受生命周期约束的 legacy placeholder 可以显式重建。
18. 输入合同失败仍输出稳定 payload，保留实际阈值、模板像素、圆形 ROI、光照和位置请求；位置修正请求始终包含 `position_correction_not_implemented` warning。

## 验证状态

- 本机 HALCON 符号能力和有效 license 已确认，显式 licensed 算法 smoke 已通过。
- 使用无效 `HALCON_LICENSE_FILE` 强制模拟缺 license 时仍返回 `#2036` 并非零退出，错误合同没有被跳过或隐藏。
- `histo_2dim` 算子单页正文与其 `ImageCol/ImageRow` 参数名、`class_2dim_sup` 及 MVTec Classification Solution Guide 存在轴文字冲突；当前决策采用后面三者和 24.11.1 最小 C canary 一致的可执行语义。
