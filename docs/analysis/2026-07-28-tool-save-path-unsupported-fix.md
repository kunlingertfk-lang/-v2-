# 工具保存回滚、方案路径边界与未支持参数修复

## 修复范围

- 工具新增或编辑后，如果方案文件提交失败，恢复修改前的工具配置、预览快照和选中项，不保留仅存在于界面的未落盘状态。
- 工具页另存为、进入输出页前检查提交结果；提交失败时停留当前页。
- 方案目录只允许使用 `projects/<schemeId>` 直接子目录。
- 基准图只允许使用方案目录内的普通文件名；拒绝绝对路径、`..`、子目录和符号链接。
- AI 检测不再对未知模型、非固定 NMS 阈值、未支持检测区域、角度过滤、边界过滤和角度排序静默降级。
- AI 检测已实现 X/Y/得分排序。
- 轮廓有无不再把圆形或自由检测区域退化为包围矩形。

## 明确错误状态

- `unsupported_model`
- `unsupported_nms_threshold`
- `unsupported_detect_roi`
- `unsupported_angle_filter`
- `unsupported_boundary_filter`
- `unsupported_sort_mode`
- `invalid_detect_roi`
- `invalid_class_filter`

## 验证结果

- `scheme_transaction_smoke`：通过。覆盖保存失败回滚、双基准图槽、越界方案目录和 `../` 基准图拒绝。
- `ai_detection_bridge_smoke`：通过。覆盖排序以及模型、NMS、角度过滤和检测区域的明确失败。
- `presence_ocr_position_correction_contract_smoke`：通过。覆盖轮廓未支持 ROI 不降级。
- `qmake ../../qt_ui_test.pro && make -j4`：通过。

## 手工回归建议

1. 打开工具页新增或编辑工具，将方案目录临时设为只读后保存，确认弹出失败提示且工具卡片恢复原状。
2. 恢复目录权限后再次保存，确认新建、编辑、重新打开和回显正常。
3. AI 检测启用角度或边界过滤，确认返回明确的不支持状态，不发起远程推理。
