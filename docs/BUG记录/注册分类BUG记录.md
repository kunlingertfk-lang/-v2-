# 注册分类 BUG 记录

## 维护约定

本文件持续记录注册分类工具的模型训练、参数区、样式接入和运行 Bug。注册目标检测是独立功能，记录在“注册目标检测 BUG 记录”。

## 问题索引

| 编号 | 发现时间 | 状态 | 问题 |
|---|---|---|---|
| RC-20260805-01 | 2026-08-05 15:03 CST | 已修复，待手工视觉复测 | 注册目标检测修复后，注册分类仍未接入统一样式 |

## RC-20260805-01 2026-08-05 15:03 CST — 未完整接入统一工具级样式

### 基本信息

- 修复时间：2026-08-05 15:06 CST
- 状态：已修复，构建通过，待手工视觉复测
- 严重程度：中
- 来源任务：排查颜色识别 ROI 不全 BUG（019fcfba-e665-7811-84d6-4499935942db）

### 问题现象

注册目标检测窗口完成样式修复后，独立的注册分类窗口仍保留深灰参数区和不一致的模型按钮。

### 根因

`RegisteredClassificationDialog` 与 `RegisteredClassificationDetectionDialog` 是独立窗口和独立 `.ui`。注册分类自身仍缺少：

- `PlanDialogUtils::applyToolLevelStyle()`；
- `paramsScrollArea`、viewport 和 `paramsScrollContents` 的浅色作用域；
- 五个模型按钮的 `actionRole=secondary`。

### 修复方案

1. 在 Designer 属性和运行时代码中设置五个模型按钮为次级操作。
2. 初始化时显式调用 `PlanDialogUtils::applyToolLevelStyle(this)`。
3. 为注册分类自己的滚动参数区增加精确浅灰 QSS。

### 防复发

注册分类必须作为独立功能验收；对注册目标检测的回归不能替代本窗口的检查。

### 验证记录

- Qt UI 生成、`qmake`、`make -j4`、`git diff --check`：通过。
- 历史环境未安装 `xmllint`，但 `uic` 和完整构建成功。
- 尚需手工检查不同入口、五按钮 hover/disabled 和参数区背景。

### 相关文件

- `ui/RegisteredClassificationDialog.ui`
- `src/RegisteredClassificationDialog.cpp`
- `styles/app.qss`
- `src/PlanDialogUtils.cpp`
- `docs/BUG记录/注册目标检测BUG记录.md`
