# 颜色识别双后端恢复实施计划

日期：2026-07-16
设计：`docs/superpowers/specs/2026-07-16-color-recognition-dual-backend-recovery-design.md`

## 目标

修复参考图实际 Mat 与运行时元数据不一致、模板重开后训练上下文丢失、新模板 GMM 按钮不刷新，以及 HSV/GMM 切换后 HSV 缺少严格状态校验、批量重建和反馈的问题。

## 实施步骤

1. 在 `ReferenceImageProvider` 的格式规范化边界，根据最终 Mat 重新生成运行时 `FrameInputMetadata`；在 `SchemeStore` 从 `reference.png` 重载时保证快照元数据描述实际 BGR8 Mat。
2. 在 `ColorTemplateDialog` 增加当前保存样本恢复逻辑：自动选中首个样本，解码无损 ROI，恢复 Mat、元数据、预览和全图训练 ROI。
3. 集中计算 HSV `empty/ready/stale/invalid` 状态，切换后端和样本/参数变化后统一刷新。
4. 增加“重新提取 HSV 特征”按钮和附近状态标签；批量重建使用临时副本，全部成功后一次提交。
5. HSV 非 ready 时增加保存前确认；保持 GMM 现有确认及两套模型独立状态。
6. 样本增删后无条件刷新 GMM UI，修复 empty 分支导致的新模板建模按钮不更新。
7. 扩展 lifecycle smoke，覆盖重载上下文、混合 HSV 签名、事务式重建、后端往返切换和按钮状态。
8. 增加参考图 Mat/metadata 一致性回归，并运行 HSV、GMM B1/B2/B3、Qt 主工程构建。

## 完成条件

- 重启后无需重新框选即可重新建立 GMM 或批量重建 HSV。
- 切换后端只校验和提示，不自动训练、不覆盖另一后端模型。
- 实际 Mat 与 pixelFormat 始终一致，格式错误包含可定位信息。
- 新模板首个样本加入后建模按钮立即可用。
- 相关 smoke 和主工程构建通过。
