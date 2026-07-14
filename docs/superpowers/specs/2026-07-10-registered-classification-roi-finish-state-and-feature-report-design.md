# 注册分类 ROI 完成状态与当前特征报告设计

## 背景

主注册分类 Dialog 点击检测区域“完成”后，内部 `m_roiEditing` 已经关闭，但 ROI 按钮没有同步取消高亮。原因是 `finishRoiEditing()` 未调用 `refreshUiState()`。

同时，当前模型使用固定的 HALCON 28 维 ROI 统计特征。现有运行模型只有一张训练图、7 个样本，类别样本数为 Circle 2、Rectangle 3、Polay 2，因此模型输出偏向某一类别属于数据和 ROI 质量问题，不代表算法只支持圆形。

## 目标

- 点击检测区“完成”后退出矩形 ROI 编辑并取消 ROI 按钮高亮。
- 保留当前 ROI 坐标、ROI overlay、最近测试结果和持续测试模式。
- 增加 smoke 回归，锁定内部状态和 UI 高亮状态的一致性。
- 在注册分类功能文档中记录当前 28 维特征和模型偏置诊断依据。

## 非目标

- 不修改 HALCON 特征定义、MLP 结构或模型格式。
- 不在本次修复中增加新特征或改变训练样本策略。
- 不清除用户已经绘制的 ROI，不改变持续测试模式。

## 实现设计

`RegisteredClassificationDialog::finishRoiEditing()` 按以下顺序执行：

1. 设置 `m_roiEditing=false`。
2. 调用 `FrameViewHelper::setRoiDrawingEnabled(false)`。
3. 调用 `refreshUiState()`，使矩形 ROI 按钮根据内部状态取消 checked。
4. 调用 `refreshRoiOverlay()`，保留当前 ROI 显示。
5. 更新状态文本，不清除结果或持续测试状态。

## 测试设计

在 `registered_classification_dialog_smoke` 中覆盖：

- 点击矩形 ROI 图标后按钮 checked 且 helper 处于绘制状态。
- 发出有效 ROI 后点击检测区“完成”。
- 完成后 helper 不再绘制、ROI 按钮取消 checked、最新 ROI 坐标仍保留。
- 持续测试模式仍保持 checked。
- 主 Qt shadow build 和 `git diff --check` 通过。

## 当前特征诊断记录

当前 `halcon_mlp_roi_stats_v1` 为 28 维：

- ROI 几何：`roiAspect`、`roiAreaRatio`。
- 前景统计：`foregroundAreaRatio`、`foregroundCenterX`、`foregroundCenterY`。
- 二阶矩：`momentRa`、`momentRb`、`momentPhi`。
- 灰度统计：`grayMean`、`grayMin`、`grayMax`、`grayDeviation`。
- 灰度直方图：`grayHist00` 至 `grayHist15`，共 16 维。

HALCON 提取流程为 `rgb1_to_gray`、`gen_rectangle1`、`reduce_domain`、`intensity`、`threshold`、`area_center`、`moments_region_2nd`、`gray_histo`。

当前模型偏置诊断依据：训练报告显示有效样本 `Circle=2`、`Rectangle=3`、`Polay=2`，且只有一张训练图；训练报告同时警告 Circle 和 Polay 少于 3 个样本。后续改善应优先增加每类多张图和均衡 ROI，确保每个 ROI 只覆盖一个目标并减少背景混入。

## 状态

- `[已确认]` 修复完成按钮后的 ROI 状态同步。
- `[待实现]` smoke 回归断言。
- `[后续优化]` 扩充训练图和类别均衡策略。
