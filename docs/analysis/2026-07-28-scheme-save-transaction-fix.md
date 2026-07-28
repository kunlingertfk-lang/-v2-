# 方案保存与基准图事务修复（2026-07-28）

## 修复范围

- Reference、Camera、Output 和 MainWindow 页面导航仅在方案保存成功后执行。
- ROI 尚未完成、方案目录不可写、JSON 写入或提交失败时保留当前页面。
- `SchemeStore::saveCurrentScheme()` 失败后恢复磁盘中最后一次成功提交的内存状态。
- 普通方案保存不再隐式读取全局 `ReferenceImageProvider` 并重写基准图。
- 基准图使用 `reference_a.png` / `reference_b.png` 双槽提交：
  1. 新图写入当前 JSON 未引用的槽位；
  2. 使用 `QSaveFile` 原子提交 `scheme.json`；
  3. 提交成功后更新 `m_currentScheme` 和 `ReferenceImageProvider`；
  4. 失败时旧 JSON、旧基准图引用和内存状态保持不变。
- “另存为”从当前方案磁盘文件复制基准图，成功后同步当前内存和 provider。

现有方案中的 `reference.png` 保持可读；下一次更新基准图时自动迁移到双槽格式。

## 验证

- 主工程 qmake + make：通过。
- `scheme_transaction_smoke`：通过。
  - A/B 双槽切换；
  - JSON 提交失败后的磁盘和内存回滚；
  - 配置保存失败后的内存状态回滚；
  - 普通保存不会被全局 provider 偷换基准图；
  - 另存为后磁盘与 provider 一致。
- `color_comparison_dialog_integration_smoke`：通过。
- 无界面启动 5 秒：无崩溃或断言。
- `git diff --check`（本次修改文件）：通过。

## 仍需人工验证

- 在 UI 中把方案目录临时设为不可写，分别从 Reference、Camera、Output 页面点击各导航按钮，确认出现保存失败提示且当前页面不关闭。
- 正常路径下依次完成“相机参数 → 基准图 → 功能 → 输出”，确认保存、重新打开和另存为回显正确。
