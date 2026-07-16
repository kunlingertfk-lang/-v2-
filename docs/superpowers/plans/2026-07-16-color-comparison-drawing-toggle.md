# 颜色比较绘制工具 Toggle 交互实施计划

> **For agentic workers:** 按任务顺序执行。每个任务先补失败测试，再做最小实现并验证。不得批量修改其他工具 Dialog。

**Goal:** 让颜色比较的所有绘制图标以 `EditState` 为唯一高亮和绘制状态源，支持第一次点击进入、保持连续绘制、再次点击退出，并把该交互写入公共规范。

**Architecture:** 保留 `ColorComparisonDialog::EditState` 作为单一活动状态，在 Dialog 内增加统一 toggle 方法。已保存的 ROI 类型和几何数据继续独立保存，不再驱动绘制图标 checked。`FrameViewHelper` 继续负责具体绘制；颜色比较只负责启停对应模式和业务数据更新。

**Tech Stack:** C++17、Qt 5.15.2 Widgets、`FrameViewHelper`、qmake、现有 offscreen dialog smoke。

## 全局约束

- 本次只改颜色比较和公共文档，不批量迁移其他 Dialog。
- 不改变 HALCON 特征提取、模板模型、评分、阈值和结果判定。
- 不改变 ROI、圆形或 Mask 的配置字段及持久化格式。
- checked 只表示当前活动绘制模式；不得表示已经保存的 ROI 类型或有效性。
- 退出绘制模式不得清空最后一次有效 ROI、圆形或 Mask。
- 任一时刻最多一个绘制模式活动。
- 构建产物、日志和临时截图不得提交。

---

### Task 1：用 Dialog smoke 锁定 Toggle 状态合同

**Files:**

- Modify: `smoke/color_comparison_dialog_integration_smoke.cpp`

- [ ] **Step 1：补初始状态与矩形进入测试**

在现有 `#define private public` 测试能力下，断言默认保存类型为 rectangle 时仍不自动高亮：

```cpp
check(dialog.m_editState == ColorComparisonDialog::EditState::None,
      "saved rectangle ROI must not imply active drawing");
check(!dialog.m_detectRectButton->isChecked()
      && !dialog.m_detectCircleButton->isChecked(),
      "drawing buttons must be idle initially");

dialog.m_detectRectButton->click();
QApplication::processEvents();
check(dialog.m_editState == ColorComparisonDialog::EditState::DetectRect
      && dialog.m_detectRectButton->isChecked()
      && dialog.m_previewHelper->isRoiDrawingEnabled(),
      "first rectangle click must enter and highlight rectangle drawing");
```

- [ ] **Step 2：补连续重绘与再次点击退出测试**

保存进入前 ROI，模拟 `handleRoiChanged()` 更新，然后再次点击矩形图标：

```cpp
const QRectF redrawn(0.18, 0.22, 0.31, 0.27);
dialog.handleRoiChanged(redrawn);
check(dialog.m_editState == ColorComparisonDialog::EditState::DetectRect
      && dialog.m_previewHelper->isRoiDrawingEnabled(),
      "finishing one rectangle must keep continuous drawing active");

dialog.m_detectRectButton->click();
check(dialog.m_editState == ColorComparisonDialog::EditState::None
      && !dialog.m_detectRectButton->isChecked()
      && !dialog.m_previewHelper->isRoiDrawingEnabled(),
      "second rectangle click must exit drawing");
check(dialog.m_detectRoi == redrawn,
      "exiting drawing must preserve the last rectangle ROI");
```

- [ ] **Step 3：补矩形/圆形切换和全图退出测试**

依次执行矩形点击、圆形点击、圆形再次点击和全图点击，断言：

- `DetectRect -> DetectCircle` 时只有圆形 checked。
- `FrameViewHelper` 只开启 circle drawing。
- 圆形再次点击后进入 `None`，最后有效圆形数据不变。
- 全图点击后 `m_globalDetection=true`、`m_editState=None`，所有绘制 helper 关闭。
- 任一阶段矩形、圆形绘制图标 checked 数量不超过 1。

- [ ] **Step 4：补模板 ROI 和两个 Mask 的 Toggle 测试**

分别验证：

```text
TemplateRect -> None
TemplateMaskPolygon -> None
DetectMaskPolygon -> None
```

第一次进入时对应 helper 启用且图标 checked；第二次点击同一图标时退出且数据保留。

模拟一次有效多边形完成后 helper 自动关闭的场景，再调用 `handlePolygonChanged()`，断言只要 `m_editState` 仍是对应 Mask 状态，polygon drawing 会重新启用，以支持连续绘制。

- [ ] **Step 5：补测试运行不得按 ROI 类型自动进入绘制的回归**

直接覆盖或通过最小可控入口验证：当 `m_editState=None` 且保存类型为 rectangle/circle 时，测试模式状态同步不会自动进入 `DetectRect/DetectCircle`。

- [ ] **Step 6：构建并确认 RED**

Run:

```bash
source scripts/dependencies.env
mkdir -p build/smoke/color_comparison_dialog_integration
cd build/smoke/color_comparison_dialog_integration
$QT_ROOT/bin/qmake ../../../smoke/color_comparison_dialog_integration_smoke.pro
make -j8
QT_QPA_PLATFORM=offscreen ./bin/color_comparison_dialog_integration_smoke
```

Expected: 新增初始状态、再次点击退出或测试模式不自动进入绘制的断言失败。

---

### Task 2：分离绘制活动状态与已保存 ROI 类型

**Files:**

- Modify: `src/ColorComparisonDialog.h`
- Modify: `src/ColorComparisonDialog.cpp`

- [ ] **Step 1：声明统一 Toggle 方法**

在 private 方法区增加：

```cpp
void toggleEditState(EditState requestedState);
```

实现只负责状态切换：

```cpp
void ColorComparisonDialog::toggleEditState(EditState requestedState)
{
    setEditState(m_editState == requestedState
                 ? EditState::None
                 : requestedState);
}
```

不得在该方法中修改 ROI 类型、几何数据、模型或测试状态。

- [ ] **Step 2：改造模板和 Mask 图标连接**

以下图标的 `clicked` 连接改为 `toggleEditState(...)`：

- `m_templateRectButton`
- `m_templateMaskPolygonButton`
- `m_detectMaskPolygonButton`

保留现有“编辑”按钮作为显式进入入口，保留“完成”按钮调用 `setEditState(None)`。同一图标再次点击必须退出。

- [ ] **Step 3：改造检测矩形和圆形连接**

点击矩形或圆形时先判断是否正在激活该状态：

```cpp
const bool activating = m_editState != EditState::DetectRect;
if (activating) {
    m_globalDetection = false;
    m_detectRegionType = QStringLiteral("rectangle");
    m_detectCircle = CircleRoi();
}
toggleEditState(EditState::DetectRect);
refreshDetectRegionButtons();
if (activating)
    handleDetectionConfigChanged(QStringLiteral("detect_region_changed"));
```

圆形采用相同结构。仅退出绘制时配置没有变化，不得重复触发配置失效或即时重测。

- [ ] **Step 4：让检测按钮 checked 只跟随活动状态**

修改 `refreshDetectRegionButtons()`：

```cpp
m_detectGlobalButton->setChecked(m_globalDetection);
m_detectRectButton->setChecked(m_editState == EditState::DetectRect);
m_detectCircleButton->setChecked(m_editState == EditState::DetectCircle);
```

为可靠支持“非全图且没有活动绘制图标”的状态，在批量同步 checked 前临时关闭 `m_detectRegionGroup` 的 exclusive，设置完成后恢复；继续使用 `QSignalBlocker` 避免程序同步触发业务槽。

删除 `buildUi()` 中默认执行的 `m_detectRectButton->setChecked(true)`。初始化和配置回显完成后统一通过 `setEditState(None)`/刷新方法得到空闲高亮状态。

- [ ] **Step 5：取消测试流程按保存类型自动激活绘制**

删除 `applyDetectRoiEditState()` 的声明和实现，并移除以下流程中的调用：

- `runTest()` 暂停分支
- `runReferenceTest()`
- `startContinuousRun()`
- `runSingleShotTest()`

进入相机测试前，如果当前为模板 ROI 或模板 Mask 编辑，则显式 `setEditState(None)`，保持“相机测试态不可编辑模板”的原约束。检测 ROI 已经由用户显式开启时可以保持；处于 `None` 时不得根据 `m_detectRegionType` 自动开启。

- [ ] **Step 6：保持多边形连续绘制**

`FrameViewHelper::completeDraftPolygon()` 会在发送 `polygonChanged` 前关闭 polygon drawing。颜色比较的 `handlePolygonChanged()` 更新数据并刷新 overlay 后，如果 `m_editState` 仍是 `TemplateMaskPolygon` 或 `DetectMaskPolygon`，重新调用：

```cpp
m_previewHelper->setPolygonDrawingEnabled(true);
```

用户再次点击图标进入 `None` 后不得重新启用。

- [ ] **Step 7：运行 Dialog smoke，确认 GREEN**

Run:

```bash
source scripts/dependencies.env
make -C build/smoke/color_comparison_dialog_integration -j8
QT_QPA_PLATFORM=offscreen \
  build/smoke/color_comparison_dialog_integration/bin/color_comparison_dialog_integration_smoke
```

Expected: 输出 `color comparison dialog integration smoke passed`。

---

### Task 3：写入公共绘制交互规范和功能实现记录

**Files:**

- Modify: `docs/FID/Function_Docs.md`
- Modify: `docs/FID/ColorComparison/color_comparison_function_implementation.md`

- [ ] **Step 1：补公共 Toggle 约束**

在公共图像视图缩放、平移与 ROI 坐标规范中补充：

- 绘制图标第一次点击进入并高亮。
- 未再次点击时保持绘制模式，允许连续重绘。
- 再次点击当前图标退出并取消高亮。
- 点击其他绘制图标直接切换，最多一个高亮。
- checked 只表示活动绘制状态，不表示保存的 ROI 类型或有效性。
- 退出绘制保留最后有效几何数据。
- 活动状态与 ROI/Mask 配置状态必须分离。

注明本次只迁移颜色比较，其他功能后续改造时遵守。

- [ ] **Step 2：更新颜色比较实现记录**

记录：

- 根因：`m_detectRegionType` 被错误用作绘制按钮高亮来源。
- 修复：`EditState` 成为唯一绘制状态源。
- 测试模式不再按保存类型自动进入绘制。
- Mask 完成后在活动状态下重新准备连续绘制。
- 自动化和手动验证结果。

---

### Task 4：完整验证与范围审计

**Files:**

- Verify only: `src/ColorComparisonDialog.{h,cpp}`
- Verify only: `smoke/color_comparison_dialog_integration_smoke.cpp`
- Verify only: `docs/FID/Function_Docs.md`
- Verify only: `docs/FID/ColorComparison/color_comparison_function_implementation.md`

- [ ] **Step 1：运行专项 smoke**

```bash
source scripts/dependencies.env
QT_QPA_PLATFORM=offscreen \
  build/smoke/color_comparison_dialog_integration/bin/color_comparison_dialog_integration_smoke
```

- [ ] **Step 2：影子构建主工程**

```bash
source scripts/dependencies.env
mkdir -p build/verify_ui
cd build/verify_ui
$QT_ROOT/bin/qmake ../../qt_ui_test.pro BUILD_ROOT=$PWD/out
make -j8
```

- [ ] **Step 3：离屏启动和 QSS 检查**

```bash
QT_QPA_PLATFORM=offscreen timeout 3s \
  build/verify_ui/out/bin/qt_ui_test
```

Expected: 超时退出码为 `124`，启动期间无 stylesheet parse、unknown property 或崩溃信息。

- [ ] **Step 4：检查变更范围和格式**

```bash
git diff --check -- \
  src/ColorComparisonDialog.h \
  src/ColorComparisonDialog.cpp \
  smoke/color_comparison_dialog_integration_smoke.cpp \
  docs/FID/Function_Docs.md \
  docs/FID/ColorComparison/color_comparison_function_implementation.md
git diff --stat
```

确认没有修改 HALCON runner、adapter、模型字段、其他 Dialog 或构建产物。

- [ ] **Step 5：Ubuntu 手动交互验证**

在实际图形环境中验证：

1. 矩形/圆形第一次点击高亮并可连续重绘。
2. 同一图标再次点击取消高亮并恢复普通缩放/平移。
3. 矩形、圆形和 Mask 之间切换只高亮一个。
4. 退出后最后 ROI 仍显示，保存并重新打开后不自动进入绘制态。
5. 基准图测试、连续运行、暂停和单次运行不因保存的 ROI 类型自动高亮绘制图标。

---

## 完成标准

- 颜色比较所有绘制图标符合统一 Toggle 合同。
- `EditState` 是唯一活动绘制状态源。
- 保存 ROI 类型与图标高亮完全解耦。
- 连续重绘、图标切换、再次点击退出和测试模式均有自动化覆盖。
- 公共规范和颜色比较实现记录同步更新。
- 专项 smoke、主工程构建和离屏启动全部通过。
