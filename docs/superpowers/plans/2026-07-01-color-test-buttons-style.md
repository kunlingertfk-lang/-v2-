# 颜色识别/比较 测试按钮族样式实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 把颜色识别/比较对话框底部测试按钮族改为 C 方案分层权重样式（深色实心主操作 / 白底灰边测试操作含 running 橙色态 / 透明弱化退出测试红 hover），补齐 hover/running/disabled 视觉。

**架构：** 纯 QSS 改动 + 比较侧补一行 objectName 定向。替换两个对话框各自的内联 QSS 块为完整规则集，不动任何点击/状态逻辑（`installActionButtonFlash`、`running` 属性、信号槽连接全部保留）。

**技术栈：** Qt Widgets、QSS 样式表、qmake 构建。

**规格：** `docs/superpowers/specs/2026-07-01-color-test-buttons-style-design.md`

**关于测试策略：** 本计划是纯 QSS 视觉改动，Qt QSS 无法用单元测试断言渲染像素，故采用「构建验证 + 手动 UI 检查清单」替代 TDD（符合项目 `AGENTS.md`「涉及 UI 交互时，应手动检查关键按钮…状态不崩溃」的验证要求）。每个任务以「构建通过 + 手动检查项」作为完成判据。

---

## 文件结构

- 修改：`src/ColorRecognitionDialog.cpp` —— 替换 `setupUiState()` 内的内联 QSS 块（约 855-860 行）。该侧按钮属性已齐全，无需补 objectName。
- 修改：`src/ColorComparisonDialog.cpp` —— 替换 `buildUi()` 内的内联 QSS 块（约 500-504 行）；并在 `m_exitTestButton` 创建后补一行 `setObjectName("exitTestButton")`。

不创建新文件。不改动 `installActionButtonFlash`、`refreshButtonStyle`、`applyBottomActionButtonMetrics`、`running` 属性设置点、任何信号槽连接、`ColorTemplateDialog` 等其他对话框。

## 统一 QSS 规则集（两个对话框替换后内容一致）

替换后的内联 QSS 块（注意：这是拼接在一个 `QStringLiteral(...)` 里的字符串字面量，每条规则用双引号包裹、转义内嵌双引号）：

```cpp
        "QPushButton[actionRole=\"testPrimary\"]{background:#111827;color:#ffffff;border:1px solid #111827;border-radius:4px;padding:0;font-size:15px;}"
        "QPushButton[actionRole=\"testPrimary\"]:hover{background:#000;border-color:#000;}"
        "QPushButton[actionRole=\"testPrimary\"]:pressed,QPushButton[actionRole=\"testPrimary\"][flash=\"true\"]{background:#ffffff;color:#111827;border-color:#111827;}"
        "QPushButton[actionRole=\"testPrimary\"]:disabled{background:#e5e7eb;color:#9ca3af;border-color:#e5e7eb;}"
        "QPushButton[actionRole=\"testAction\"]{background:#ffffff;color:#111827;border:1px solid #9ca3af;border-radius:4px;padding:0;font-size:15px;}"
        "QPushButton[actionRole=\"testAction\"]:hover{background:#f9fafb;border-color:#111827;}"
        "QPushButton[actionRole=\"testAction\"]:pressed,QPushButton[actionRole=\"testAction\"][flash=\"true\"]{background:#111827;color:#ffffff;border-color:#111827;}"
        "QPushButton[actionRole=\"testAction\"][running=\"true\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"testAction\"][running=\"true\"]:hover{background:#e66e00;border-color:#e66e00;}"
        "QPushButton[actionRole=\"testAction\"]:disabled{background:#f3f4f6;color:#9ca3af;border-color:#e5e7eb;}"
        "QPushButton#exitTestButton{background:transparent;color:#6b7280;border:1px solid #d1d5db;}"
        "QPushButton#exitTestButton:hover{background:#fef2f2;color:#dc2626;border-color:#dc2626;}"
        "QPushButton#exitTestButton:pressed,QPushButton#exitTestButton[flash=\"true\"]{background:#dc2626;color:#ffffff;border-color:#dc2626;}"
        "QPushButton#exitTestButton:disabled{background:#f3f4f6;color:#9ca3af;border-color:#e5e7eb;}"
        "QToolButton:pressed{background:#ffe1bf;color:#ff7a00;border-color:#ff7a00;}"
```

---

### 任务 1：颜色识别对话框 QSS 替换

**文件：**
- 修改：`src/ColorRecognitionDialog.cpp`（`setupUiState()` 内约 855-860 行的内联 QSS 块）

- [ ] **步骤 1：替换内联 QSS 块**

用 Edit 工具，将以下 old_string（唯一匹配，含 `setStyleSheet(styleSheet() + QStringLiteral(` 起始与 `QToolButton:pressed...}));` 结尾的三行规则块）替换为新规则集。

old_string（精确匹配现状）：
```cpp
    setStyleSheet(styleSheet() + QStringLiteral(
        "QPushButton[actionRole=\"testAction\"],QPushButton[actionRole=\"testPrimary\"]{background:#ffffff;color:#111827;border:1px solid #111827;border-radius:4px;padding:0;font-size:15px;}"
        "QPushButton[actionRole=\"testAction\"]:pressed,QPushButton[actionRole=\"testPrimary\"]:pressed,QPushButton[actionRole=\"testAction\"][flash=\"true\"],QPushButton[actionRole=\"testPrimary\"][flash=\"true\"]{background:#111827;color:#ffffff;border-color:#111827;}"
        "QToolButton:pressed{background:#ffe1bf;color:#ff7a00;border-color:#ff7a00;}"));
```

new_string（统一 QSS 规则集，含原 `setStyleSheet(styleSheet() + QStringLiteral(` 起始与 `}));` 结尾）：
```cpp
    setStyleSheet(styleSheet() + QStringLiteral(
        "QPushButton[actionRole=\"testPrimary\"]{background:#111827;color:#ffffff;border:1px solid #111827;border-radius:4px;padding:0;font-size:15px;}"
        "QPushButton[actionRole=\"testPrimary\"]:hover{background:#000;border-color:#000;}"
        "QPushButton[actionRole=\"testPrimary\"]:pressed,QPushButton[actionRole=\"testPrimary\"][flash=\"true\"]{background:#ffffff;color:#111827;border-color:#111827;}"
        "QPushButton[actionRole=\"testPrimary\"]:disabled{background:#e5e7eb;color:#9ca3af;border-color:#e5e7eb;}"
        "QPushButton[actionRole=\"testAction\"]{background:#ffffff;color:#111827;border:1px solid #9ca3af;border-radius:4px;padding:0;font-size:15px;}"
        "QPushButton[actionRole=\"testAction\"]:hover{background:#f9fafb;border-color:#111827;}"
        "QPushButton[actionRole=\"testAction\"]:pressed,QPushButton[actionRole=\"testAction\"][flash=\"true\"]{background:#111827;color:#ffffff;border-color:#111827;}"
        "QPushButton[actionRole=\"testAction\"][running=\"true\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"testAction\"][running=\"true\"]:hover{background:#e66e00;border-color:#e66e00;}"
        "QPushButton[actionRole=\"testAction\"]:disabled{background:#f3f4f6;color:#9ca3af;border-color:#e5e7eb;}"
        "QPushButton#exitTestButton{background:transparent;color:#6b7280;border:1px solid #d1d5db;}"
        "QPushButton#exitTestButton:hover{background:#fef2f2;color:#dc2626;border-color:#dc2626;}"
        "QPushButton#exitTestButton:pressed,QPushButton#exitTestButton[flash=\"true\"]{background:#dc2626;color:#ffffff;border-color:#dc2626;}"
        "QPushButton#exitTestButton:disabled{background:#f3f4f6;color:#9ca3af;border-color:#e5e7eb;}"
        "QToolButton:pressed{background:#ffe1bf;color:#ff7a00;border-color:#ff7a00;}"));
```

> 该侧 `m_exitTestButton` 已有 `setObjectName("exitTestButton")`（见原文件 894 行），无需补。

- [ ] **步骤 2：构建验证**

运行：`cd /home/tt/tfk/WorkerSpace/project/V2_615_1628/qt_znxj_v2_work_1920_515 && qmake qt_ui_test.pro && make -j$(nproc) 2>&1 | tail -5`
预期：编译通过，无报错（QSS 是字符串字面量，不会引入编译错误；此步主要防止意外破坏字符串拼接/括号）。

- [ ] **步骤 3：手动 UI 检查（颜色识别对话框）**

运行程序，打开颜色识别方案编辑对话框，逐项核对：

| 检查项 | 预期视觉 |
|---|---|
| 「完成」按钮 | 深色实心（#111827 底白字）；hover 变更黑 #000；点击瞬间反色（白底深字）闪一下 |
| 「测试运行」按钮 | 白底灰边（#9ca3af）；hover 浅灰底 #f9fafb + 深边；点击反色闪一下 |
| 进入连续运行态后「测试运行」 | 变橙色实心 #ff7a00 白字（原本与普通态无区别，现在应明显区分）|
| 「基准图测试」按钮 | 白底灰边，与测试运行默认态一致 |
| 「退出测试」按钮 | 透明描边浅灰字；hover 变浅红底 + 红字红边 #dc2626；点击红底白字闪一下 |
| 检测在途按钮禁用 | 灰化（浅灰底 #f3f4f6 + 灰字 #9ca3af）|

- [ ] **步骤 4：Commit**

```bash
git add src/ColorRecognitionDialog.cpp
git commit -m "style(color-recognition): 测试按钮族改 C 方案分层权重样式

主操作深色实心、测试操作白底灰边含 running 橙色态、退出测试透明弱化红 hover，
补齐 hover/running/disabled 视觉。仅改内联 QSS，不动点击/状态逻辑。

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

### 任务 2：颜色比较对话框 QSS 替换 + 补 objectName

**文件：**
- 修改：`src/ColorComparisonDialog.cpp`（`buildUi()` 内约 500-504 行的内联 QSS 块；`m_exitTestButton` 创建处约 468 行附近补 objectName）

- [ ] **步骤 1：为退出测试按钮补 objectName**

用 Edit 工具，将以下 old_string 替换为 new_string（在 `m_exitTestButton` 设置 actionRole 之后补 objectName，使 QSS `#exitTestButton` 能定向到它）。

old_string：
```cpp
    m_exitTestButton->setProperty("actionRole", QStringLiteral("testAction"));
    installActionButtonFlash(m_referenceTestButton);
```

new_string：
```cpp
    m_exitTestButton->setProperty("actionRole", QStringLiteral("testAction"));
    m_exitTestButton->setObjectName(QStringLiteral("exitTestButton"));
    installActionButtonFlash(m_referenceTestButton);
```

- [ ] **步骤 2：替换内联 QSS 块**

用 Edit 工具，将以下 old_string（含上方 `QPushButton:checked,QToolButton:checked...` 行作为唯一性锚点，到 `QToolButton:pressed...}));` 结尾）替换为新规则集。

old_string（精确匹配现状）：
```cpp
        "QPushButton:checked,QToolButton:checked{background:#fff3e6;color:#ff7a00;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"testAction\"],QPushButton[actionRole=\"testPrimary\"]{background:#ffffff;color:#111827;border:1px solid #111827;border-radius:4px;padding:0;font-size:15px;}"
        "QPushButton[actionRole=\"testAction\"]:pressed,QPushButton[actionRole=\"testPrimary\"]:pressed,QPushButton[actionRole=\"testAction\"][flash=\"true\"],QPushButton[actionRole=\"testPrimary\"][flash=\"true\"]{background:#111827;color:#ffffff;border-color:#111827;}"
        "QToolButton:pressed{background:#ffe1bf;color:#ff7a00;border-color:#ff7a00;}"));
```

new_string（保留首行 `QPushButton:checked...` 不变，其后替换为统一规则集）：
```cpp
        "QPushButton:checked,QToolButton:checked{background:#fff3e6;color:#ff7a00;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"testPrimary\"]{background:#111827;color:#ffffff;border:1px solid #111827;border-radius:4px;padding:0;font-size:15px;}"
        "QPushButton[actionRole=\"testPrimary\"]:hover{background:#000;border-color:#000;}"
        "QPushButton[actionRole=\"testPrimary\"]:pressed,QPushButton[actionRole=\"testPrimary\"][flash=\"true\"]{background:#ffffff;color:#111827;border-color:#111827;}"
        "QPushButton[actionRole=\"testPrimary\"]:disabled{background:#e5e7eb;color:#9ca3af;border-color:#e5e7eb;}"
        "QPushButton[actionRole=\"testAction\"]{background:#ffffff;color:#111827;border:1px solid #9ca3af;border-radius:4px;padding:0;font-size:15px;}"
        "QPushButton[actionRole=\"testAction\"]:hover{background:#f9fafb;border-color:#111827;}"
        "QPushButton[actionRole=\"testAction\"]:pressed,QPushButton[actionRole=\"testAction\"][flash=\"true\"]{background:#111827;color:#ffffff;border-color:#111827;}"
        "QPushButton[actionRole=\"testAction\"][running=\"true\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"testAction\"][running=\"true\"]:hover{background:#e66e00;border-color:#e66e00;}"
        "QPushButton[actionRole=\"testAction\"]:disabled{background:#f3f4f6;color:#9ca3af;border-color:#e5e7eb;}"
        "QPushButton#exitTestButton{background:transparent;color:#6b7280;border:1px solid #d1d5db;}"
        "QPushButton#exitTestButton:hover{background:#fef2f2;color:#dc2626;border-color:#dc2626;}"
        "QPushButton#exitTestButton:pressed,QPushButton#exitTestButton[flash=\"true\"]{background:#dc2626;color:#ffffff;border-color:#dc2626;}"
        "QPushButton#exitTestButton:disabled{background:#f3f4f6;color:#9ca3af;border-color:#e5e7eb;}"
        "QToolButton:pressed{background:#ffe1bf;color:#ff7a00;border-color:#ff7a00;}"));
```

- [ ] **步骤 3：构建验证**

运行：`cd /home/tt/tfk/WorkerSpace/project/V2_615_1628/qt_znxj_v2_work_1920_515 && qmake qt_ui_test.pro && make -j$(nproc) 2>&1 | tail -5`
预期：编译通过，无报错。

- [ ] **步骤 4：手动 UI 检查（颜色比较对话框）**

运行程序，打开颜色比较方案编辑对话框，逐项核对（与任务 1 步骤 3 相同的 6 项检查表，应用到比较对话框）：

| 检查项 | 预期视觉 |
|---|---|
| 「完成」按钮 | 深色实心 #111827 白字；hover #000；点击反色闪一下 |
| 「测试运行」按钮 | 白底灰边 #9ca3af；hover 浅灰 #f9fafb + 深边；点击反色闪一下 |
| 连续运行态「测试运行」 | 橙色实心 #ff7a00 白字 |
| 「基准图测试」按钮 | 白底灰边，与测试运行默认态一致 |
| 「退出测试」按钮 | 透明描边浅灰字；hover 浅红 + 红字红边；点击红底白字闪一下 |
| 检测在途按钮禁用 | 灰化 |

额外回归检查（确保未影响其他控件）：
- 「基础/全部」分段按钮仍为橙色 checked 态（`QPushButton:checked` 规则保留）。
- 检测区域矩形/圆形工具按钮、模板编辑等 QToolButton pressed 仍为橙色。
- ROI 绘制、完成/退出流程正常，无崩溃、无误触发关闭。

- [ ] **步骤 5：Commit**

```bash
git add src/ColorComparisonDialog.cpp
git commit -m "style(color-comparison): 测试按钮族改 C 方案分层权重样式 + 补 exitTestButton objectName

主操作深色实心、测试操作白底灰边含 running 橙色态、退出测试透明弱化红 hover，
补齐 hover/running/disabled 视觉。仅改内联 QSS 与一行 objectName，不动逻辑。

Co-Authored-By: Claude <noreply@anthropic.com>"
```

---

## 自检结果

**1. 规格覆盖度：**
- 规格「改动 1」识别侧 QSS 替换 → 任务 1 ✓
- 规格「改动 2」比较侧 QSS 替换 + 补 objectName → 任务 2 ✓
- 规格「视觉层级」三档（testPrimary / testAction / #exitTestButton）+ hover/running/disabled → 统一 QSS 规则集逐条对应 ✓
- 规格「不改动」清单 → 文件结构章节明确列出 ✓
- 规格「验证」5 项 → 任务 1/2 的构建验证 + 手动检查表覆盖 ✓

**2. 占位符扫描：** 无 TODO/待定；每个步骤含完整 old_string/new_string 与精确命令。✓

**3. 类型一致性：** `actionRole`、`running`、`flash`、`exitTestButton` 等属性名/objectName 与规格及现有代码一致；`QStringLiteral` / `setProperty` 用法沿用现有代码。✓

无遗漏，无需补充任务。
