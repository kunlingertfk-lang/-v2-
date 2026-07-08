# 注册训练 ROI 右键删除菜单问题记录

## 背景

注册分类训练窗口和注册目标检测训练窗口都支持在大图编辑页右键命中 ROI 后，通过 `删除当前 ROI` 菜单删除单个 ROI。实际使用中发现两个窗口存在相同交互问题。

## 问题

1. 大图右键删除 ROI 后，删除提示菜单没有及时消失；用户再点击残留的删除图标/菜单时，期望它应随 ROI 删除一起消失。
2. 右键 ROI 时偶发未等待用户点击菜单动作，ROI 就被直接删除。

## 根因

- 两个窗口的大图 ROI 右键事件过滤器同时处理 `QEvent::MouseButtonPress` 和 `QEvent::ContextMenu`。
- 在右键按下阶段就弹出 `QMenu`，菜单可能出现在鼠标释放位置下方，后续释放事件有机会触发菜单 action，表现为“右键直接删除 ROI”。
- 删除 action 只关闭当前菜单对象；在 UI 刷新、预览卡/大图页重建后，顶层 ROI 删除菜单可能仍可见或延迟销毁，表现为“删除图标/提示未及时消除”。

## 修复

- 注册分类训练窗口和注册目标检测训练窗口的大图 ROI 菜单事件过滤器只响应 `QEvent::ContextMenu`。
- 删除 ROI 时统一关闭相关 ROI 删除菜单：
  - 注册分类：`registeredTrainingRoiContextMenu`、`registeredTrainingPreviewRoiContextMenu`
  - 注册目标检测：`registeredDetectionTrainingRoiContextMenu`、`registeredDetectionTrainingPreviewRoiContextMenu`
- 菜单 action 触发后执行 `hide()`、`close()`、`deleteLater()`，再删除 ROI 并刷新 UI。

## 回归覆盖

- `registered_classification_dialog_smoke`
  - 验证右键按下 ROI 不会提前弹出删除菜单。
  - 验证右键按下 ROI 不会直接删除 ROI。
  - 验证 `ContextMenu` 事件才会弹出大图 ROI 删除菜单。
  - 验证触发删除后菜单消失，类别计数更新为 0。
- `registered_classification_detection_dialog_smoke`
  - 验证右键按下 ROI 不会提前弹出删除菜单。
  - 验证右键按下 ROI 不会直接删除 ROI。
  - 验证 `ContextMenu` 事件才会弹出大图 ROI 删除菜单。
  - 验证触发删除后菜单消失，目标计数更新为 0。

## 验证

- `registered_classification_dialog_smoke`: passed
- `registered_classification_detection_dialog_smoke`: passed
