# 注册目标检测 BUG 记录

## 维护约定

本文件持续记录注册目标检测工具的模型训练、参数区、样式接入和运行 Bug。注册分类是独立功能，记录在“注册分类 BUG 记录”。

## 问题索引

| 编号 | 发现时间 | 状态 | 问题 |
|---|---|---|---|
| RCD-20260805-01 | 2026-08-05 14:51 CST | 已修复，待手工视觉复测 | 左侧参数区露出深灰背景且五个模型按钮不一致 |

## RCD-20260805-01 2026-08-05 14:51 CST — 未完整接入统一工具级样式

### 基本信息

- 修复时间：2026-08-05 14:55 CST
- 状态：已修复，构建通过，待手工视觉复测
- 严重程度：中
- 来源任务：排查颜色识别 ROI 不全 BUG（019fcfba-e665-7811-84d6-4499935942db）

### 问题现象

浅色配置卡片之间和参数区下方露出深灰背景；导入、导出、删除、注册训练、模型管理五个按钮样式不统一。

### 根因

- `paramsScrollArea` 的 viewport 和 `paramsScrollContents` 未纳入浅色背景作用域；
- 五个模型按钮未完整设置 `actionRole=secondary`；
- 对话框未稳定调用 `PlanDialogUtils::applyToolLevelStyle()`；
- 导入按钮还可能命中旧的无作用域对象名规则。

### 修复方案

1. Designer 属性和运行时代码同时为五个模型按钮设置 `actionRole=secondary`。
2. 对话框初始化时调用 `PlanDialogUtils::applyToolLevelStyle(this)`。
3. 使用对话框级 QSS 覆盖滚动区、viewport 和内容区。

### 防复发

注册目标检测与注册分类是两个独立对话框，必须分别接入并分别回归，不能假设修改一处会自动覆盖另一处。

### 验证记录

- Qt UI 生成、`qmake`、`make -j4`、`git diff --check`：通过。
- 尚需手工检查不同入口、五按钮 hover/disabled 和参数区背景。

### 相关文件

- `ui/RegisteredClassificationDetectionDialog.ui`
- `src/RegisteredClassificationDetectionDialog.cpp`
- `styles/app.qss`
- `src/PlanDialogUtils.cpp`
- `docs/BUG记录/注册分类BUG记录.md`
