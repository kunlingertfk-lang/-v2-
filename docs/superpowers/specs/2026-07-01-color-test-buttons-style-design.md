# 颜色识别 / 颜色比较 测试按钮族样式设计

- 日期：2026-07-01
- 分支：colorComparsion
- 范围：颜色识别（`ColorRecognitionDialog`）与颜色比较（`ColorComparisonDialog`）底部"测试按钮族"的视觉样式与点击反馈
- 方向：C · 分层权重，**仅改样式，不动任何点击/状态逻辑**

## 背景与现状

两个对话框的底部测试按钮族结构一致：

| 按钮 | 文案（随状态切换） | actionRole | 现状 |
|---|---|---|---|
| 完成 / 运行一次 | 完成 ↔ 运行一次 | `testPrimary` | 白底深边 |
| 测试运行 | 测试运行 / 连续运行 / 停止运行 | `testAction` | 白底深边 |
| 基准图测试 | 基准图测试 | `testAction` | 白底深边 |
| 退出测试 | 退出测试 | `testAction` | 白底深边（识别侧有 objectName `exitTestButton`，比较侧无） |

当前内联 QSS（两个对话框相同）：

```
QPushButton[actionRole="testAction"],QPushButton[actionRole="testPrimary"]
  { background:#ffffff; color:#111827; border:1px solid #111827; border-radius:4px; padding:0; font-size:15px; }
QPushButton[actionRole="testAction"]:pressed,QPushButton[actionRole="testPrimary"]:pressed,
QPushButton[actionRole="testAction"][flash="true"],QPushButton[actionRole="testPrimary"][flash="true"]
  { background:#111827; color:#ffffff; border-color:#111827; }
```

### 现状缺口

1. 无 `:hover` 规则 —— 悬停无反馈。
2. `running` 属性已设置（`testRunButton` 进入连续运行态设 `running=true` 并 `refreshButtonStyle`），但 QSS 无 `[running="true"]` 选择器，连续运行态视觉上与普通态无区别。
3. 无 `:disabled` 规则 —— 检测在途按钮禁用态无区分。
4. 三个测试按钮共用 `actionRole="testAction"`，QSS 无法区分"退出测试"做弱化。
5. flash 为硬切反色，无过渡（Qt QSS 不支持 `transition`，此点不在本次修复范围）。

## Qt QSS 能力约束（关键）

Qt 样式表**不支持** `box-shadow`、`@keyframes`/`animation`、`transition`、伪元素 `::after`。因此浏览器原型中的 hover 阴影、running 呼吸光晕/呼吸点、过渡动画均无法在 QSS 中实现，需用纯色/边框变化等效降级。三档视觉层级通过底色与边框色区分，不依赖阴影或动画。

## 设计：C 方案 Qt QSS 可行版

### 视觉层级

| 按钮 | 角色 | 默认 | hover | pressed / flash | running | disabled |
|---|---|---|---|---|---|---|
| 完成 / 运行一次 | `testPrimary` | 深色实心 `#111827` 白字 | `#000` | 反色：白底 `#111827` 字 | — | 灰化 |
| 测试运行 / 基准图测试 | `testAction` | 白底 + 灰边 `#9ca3af` | 浅灰底 `#f9fafb` + 深边 `#111827` | 反色：深底 `#111827` 白字 | 橙色实心 `#ff7a00` 白字（静态） | 灰化 |
| 退出测试 | `#exitTestButton` | 透明底 + 浅灰边 `#d1d5db` | 浅红底 `#fef2f2` + 红字红边 `#dc2626` | 红底 `#dc2626` 白字 | — | 灰化 |

### QSS 规则（替换两个对话框现有内联块）

```
/* 主操作：深色实心 */
QPushButton[actionRole="testPrimary"]{background:#111827;color:#ffffff;border:1px solid #111827;border-radius:4px;padding:0;font-size:15px;}
QPushButton[actionRole="testPrimary"]:hover{background:#000;border-color:#000;}
QPushButton[actionRole="testPrimary"]:pressed,
QPushButton[actionRole="testPrimary"][flash="true"]{background:#ffffff;color:#111827;border-color:#111827;}
QPushButton[actionRole="testPrimary"]:disabled{background:#e5e7eb;color:#9ca3af;border-color:#e5e7eb;}

/* 测试操作：白底灰边，running 态橙色实心 */
QPushButton[actionRole="testAction"]{background:#ffffff;color:#111827;border:1px solid #9ca3af;border-radius:4px;padding:0;font-size:15px;}
QPushButton[actionRole="testAction"]:hover{background:#f9fafb;border-color:#111827;}
QPushButton[actionRole="testAction"]:pressed,
QPushButton[actionRole="testAction"][flash="true"]{background:#111827;color:#ffffff;border-color:#111827;}
QPushButton[actionRole="testAction"][running="true"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;}
QPushButton[actionRole="testAction"][running="true"]:hover{background:#e66e00;border-color:#e66e00;}
QPushButton[actionRole="testAction"]:disabled{background:#f3f4f6;color:#9ca3af;border-color:#e5e7eb;}

/* 退出测试：弱视觉，hover 红警示（靠 objectName 定向，覆盖上面 testAction 默认态） */
QPushButton#exitTestButton{background:transparent;color:#6b7280;border:1px solid #d1d5db;}
QPushButton#exitTestButton:hover{background:#fef2f2;color:#dc2626;border-color:#dc2626;}
QPushButton#exitTestButton:pressed,
QPushButton#exitTestButton[flash="true"]{background:#dc2626;color:#ffffff;border-color:#dc2626;}
QPushButton#exitTestButton:disabled{background:#f3f4f6;color:#9ca3af;border-color:#e5e7eb;}

/* 工具按钮 pressed（保留现有） */
QToolButton:pressed{background:#ffe1bf;color:#ff7a00;border-color:#ff7a00;}
```

> 说明：`#exitTestButton` 用 ID 选择器覆盖 `testAction` 默认态。`flash` 反馈仍由现有 `installActionButtonFlash` 设置 `flash=true` 触发，逻辑不变；退出测试点击时 flash 会切到红底白字，与 hover 红警示一致。

## 实施范围

### 改动 1：`src/ColorRecognitionDialog.cpp` 内联 QSS 块（`setupUiState` 内）

将现有 `testAction/testPrimary` 两条规则替换为上面的完整规则集。该侧按钮已具备所需属性：
- `ui->testRunButton`：`actionRole=testAction` + `running` 属性 ✓
- `ui->finishButton`：`actionRole=testPrimary` ✓
- `m_referenceTestButton`：`actionRole=testAction` ✓
- `m_exitTestButton`：`setObjectName("exitTestButton")` + `actionRole=testAction` ✓

无需补 objectName。

### 改动 2：`src/ColorComparisonDialog.cpp` 内联 QSS 块（`buildUi` 内）

同样替换为完整规则集。该侧需补一行 objectName：
- `m_exitTestButton->setObjectName("exitTestButton")` —— 当前缺失，补上后 QSS 的 `#exitTestButton` 才能定向。

此行属于样式定向，不触碰任何点击/状态逻辑。

### 不改动

- `installActionButtonFlash`（flash 反色/红闪由它触发，逻辑不动）
- `running` 属性设置点（`updateBottomButtons` 等，逻辑不动）
- `refreshButtonStyle`、`applyBottomActionButtonMetrics`
- 所有信号槽连接
- `ColorTemplateDialog` 等其他对话框

## 验证

1. `qmake qt_ui_test.pro && make` 构建通过。
2. 颜色识别对话框：
   - 完成/运行一次：深色实心，hover 更深，点击反色闪一下。
   - 测试运行：白底灰边，hover 浅灰+深边；点击反色闪一下；进入连续运行后变橙色实心。
   - 基准图测试：白底灰边，与测试运行一致。
   - 退出测试：透明描边，hover 变红，点击红底白字闪一下。
3. 颜色比较对话框：同上 4 项。
4. 切换"基础/全部"分段、ROI 绘制、完成/退出等流程不受影响。
5. 检测在途按钮 disabled 时灰化。

## 非目标

- 不引入阴影/动画/过渡（QSS 不支持，且属本次范围外）。
- 不改 flash 时长、不改 running 属性逻辑。
- 不改其他对话框的测试按钮。
