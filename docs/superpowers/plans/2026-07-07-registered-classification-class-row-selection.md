# Registered Classification Class Row Selection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make clicking the body of a label type row switch the active class in the registered classification training window, while keeping row action buttons as independent command actions.

**Architecture:** Keep this as a scoped `RegisteredClassificationTrainingDialog` interaction change. Add stable row object names and a row-body click handler that updates `currentClass`, refreshes selection state, exits drawing modes, and returns from ROI preview page to the big-image page when needed. Use the existing smoke test to pin the behavior before implementation.

**Tech Stack:** Qt Widgets/C++17, existing `RegisteredClassificationTrainingDialog`, existing `FrameViewHelper`, existing `smoke/registered_classification_dialog_smoke.pro`.

## Global Constraints

- Do not implement real HALCON training.
- Do not implement real dataset persistence.
- Do not generate model files.
- Do not support `.scbin`.
- Do not modify the main registered classification Dialog inference, Adapter, or Runner chain.
- Do not implement big-image overlay click selection; keep it as `[后续优化]`.
- Do not change the ROI preview page card right-click delete semantics.
- Keep `projects/scheme_0d5611a7/scheme.json` out of commits; it currently contains only an unrelated runtime `updatedAt` change.
- Implement option A from `docs/FID/RegisteredClassification/注册分类提示词规范.md`: clicking row blank/name/count area switches `currentClass`; clicking right-side row buttons only executes the button command.

---

### Task 1: Add Smoke Coverage for Class Row Selection

**Files:**
- Modify: `smoke/registered_classification_dialog_smoke.cpp`

**Interfaces:**
- Consumes existing helpers: `toolButtonByObjectName`, `buttonByText`, `labelByText`, `clickAndProcess`.
- Produces coverage for new stable row object names:
  - `registeredTrainingClassRow_0`
  - `registeredTrainingClassRow_1`

- [x] **Step 1: Add Qt includes**

Add the includes needed to click a non-button widget:

```cpp
#include <QFrame>
#include <QMouseEvent>
```

- [x] **Step 2: Add a widget click helper**

Add this helper next to `clickAndProcess`:

```cpp
void clickWidgetAndProcess(QWidget *widget)
{
    if (!widget)
        return;
    const QPoint center = widget->rect().center();
    QMouseEvent press(QEvent::MouseButtonPress,
                      center,
                      widget->mapToGlobal(center),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(widget, &press);
    QMouseEvent release(QEvent::MouseButtonRelease,
                        center,
                        widget->mapToGlobal(center),
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(widget, &release);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}
```

- [x] **Step 3: Add row body selection assertions**

Inside the existing `if (createClassButton && classCountLabel)` block, after the existing assertion that the new class row has a preview button, add:

```cpp
QFrame *firstClassRow = trainingDialog->findChild<QFrame *>(
            QStringLiteral("registeredTrainingClassRow_0"));
QFrame *secondClassRow = trainingDialog->findChild<QFrame *>(
            QStringLiteral("registeredTrainingClassRow_1"));
check(firstClassRow != nullptr, "first class row must expose stable object name");
check(secondClassRow != nullptr, "second class row must expose stable object name");
clickWidgetAndProcess(secondClassRow);
trainingPreviewHelper->setRoiRectNormalized(QRectF(0.20, 0.20, 0.18, 0.18));
emit trainingPreviewHelper->roiChanged(QRectF(0.20, 0.20, 0.18, 0.18));
QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
QToolButton *previewSecondClassButton = toolButtonByObjectName(*trainingDialog,
            QStringLiteral("registeredTrainingPreviewClassButton_1"));
check(previewSecondClassButton != nullptr, "second class row must keep preview button");
clickAndProcess(previewSecondClassButton);
check(labelByText(*trainingDialog, QStringLiteral("Classification1 | 已标注目标：1")) != nullptr,
      "row body selection must make new ROI belong to the selected class");
clickAndProcess(previewSecondClassButton);
```

- [x] **Step 4: Add preview-open row switching assertions**

After returning from the second class preview in Step 3, add:

```cpp
QToolButton *previewFirstClassButton = toolButtonByObjectName(*trainingDialog,
            QStringLiteral("registeredTrainingPreviewClassButton_0"));
check(previewFirstClassButton != nullptr, "first class row must keep preview button");
clickAndProcess(previewFirstClassButton);
QWidget *previewPageFromRowSwitch = trainingDialog->findChild<QWidget *>(
            QStringLiteral("registeredTrainingPreviewPage"));
check(previewPageFromRowSwitch != nullptr && previewPageFromRowSwitch->isVisible(),
      "first class preview must open preview page before row switch");
clickWidgetAndProcess(secondClassRow);
check(previewPageFromRowSwitch && !previewPageFromRowSwitch->isVisible(),
      "clicking another class row body from preview page must return to big image");
trainingPreviewHelper->setRoiRectNormalized(QRectF(0.55, 0.20, 0.18, 0.18));
emit trainingPreviewHelper->roiChanged(QRectF(0.55, 0.20, 0.18, 0.18));
QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
previewSecondClassButton = toolButtonByObjectName(*trainingDialog,
            QStringLiteral("registeredTrainingPreviewClassButton_1"));
clickAndProcess(previewSecondClassButton);
check(labelByText(*trainingDialog, QStringLiteral("Classification1 | 已标注目标：2")) != nullptr,
      "row body switch from preview page must make later ROI belong to the switched class");
clickAndProcess(previewSecondClassButton);
```

- [x] **Step 5: Keep existing button semantics assertions**

Keep the existing preview-button assertions unchanged:

```cpp
check(toolButtonByObjectName(*trainingDialog,
      QStringLiteral("registeredTrainingPreviewClassButton_1")) != nullptr,
      "new class row must have preview button");
```

The test must continue to prove that clicking the preview button opens/closes preview page, not that it is the only way to select the class.

- [x] **Step 6: Run smoke and verify red**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: build succeeds, executable fails because `registeredTrainingClassRow_0` / `_1` do not exist or row body click does not switch `currentClass`.

### Task 2: Implement Class Row Body Selection

**Files:**
- Modify: `src/RegisteredClassificationTrainingDialog.cpp`

**Interfaces:**
- Consumes existing local functions/lambdas: `showCurrentImage`, `returnToImagePage`, `refreshClassList`, `previewHelper`.
- Produces row object names:
  - `registeredTrainingClassRow_%1`
- Produces row body click behavior:
  - updates `state->currentClass`
  - exits ROI drawing modes
  - refreshes class row selected state
  - returns from preview page to big image when `state->previewClass >= 0`

- [x] **Step 1: Add `ClickableClassRow` local helper class**

Add this local class in the anonymous namespace near `TrainingRoiPreviewCard`:

```cpp
class ClickableClassRow : public QFrame
{
public:
    explicit ClickableClassRow(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
    }

    std::function<void()> clickHandler;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && clickHandler)
            clickHandler();
        QFrame::mouseReleaseEvent(event);
    }
};
```

`RegisteredClassificationTrainingDialog.cpp` already includes `<QMouseEvent>` from the ROI preview implementation.

- [x] **Step 2: Replace class row frame construction**

Inside `*refreshClassList`, replace:

```cpp
QFrame *rowFrame = new QFrame(classListWidget);
```

with:

```cpp
ClickableClassRow *rowFrame = new ClickableClassRow(classListWidget);
rowFrame->setObjectName(QStringLiteral("registeredTrainingClassRow_%1").arg(classIndex));
```

Keep the existing:

```cpp
rowFrame->setProperty("panelRole", QStringLiteral("classRow"));
```

- [x] **Step 3: Add `selectClassRow` lambda**

Before connecting the row action buttons, add this lambda inside the `for` loop after `classListLayout->addWidget(rowFrame);`:

```cpp
auto selectClassRow = [=]() {
    if (classIndex < 0 || classIndex >= state->classes.size())
        return;
    state->currentClass = classIndex;
    previewHelper->setRoiDrawingEnabled(false);
    previewHelper->setPolygonDrawingEnabled(false);
    if (state->previewClass >= 0) {
        (*returnToImagePage)(QObject::tr("当前类别：%1").arg(state->classes.value(classIndex)));
    } else {
        (*showCurrentImage)(QObject::tr("当前类别：%1").arg(state->classes.value(classIndex)));
    }
    (*refreshClassList)();
};
rowFrame->clickHandler = selectClassRow;
```

This keeps row selection on the row body only. The child `QToolButton` widgets continue to consume their own clicks and execute their own command actions.

- [x] **Step 4: Do not move action-button semantics into row click**

Keep the existing `renameButton`, `previewButton`, and `deleteButton` connections as command actions.

The preview button must continue to call:

```cpp
(*renderPreviewPage)(classIndex);
```

or:

```cpp
(*returnToImagePage)(...);
```

It must not become the only way to select `currentClass`.

- [x] **Step 5: Run smoke and verify green**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: `registered_classification_dialog_smoke: all checks passed`.

### Task 3: Update Completion Markers and Verify Build

**Files:**
- Modify: `docs/FID/RegisteredClassification/注册分类提示词规范.md`
- Modify: `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`
- Modify: `docs/superpowers/plans/2026-07-07-registered-classification-class-row-selection.md`

**Interfaces:**
- Consumes completed behavior from Tasks 1-2.
- Produces `[已完成]` completion marker for class row selection.

- [x] **Step 1: Update prompt completion status**

In `docs/FID/RegisteredClassification/注册分类提示词规范.md`, move:

```markdown
- `[待实现]` 标签类型列表行点击切换当前类别：点击分类列表中某个标签行的主体区域时，应切换当前类别；行内预览按钮只负责进入/退出 ROI 预览页，不再承担主要类别切换职责。
```

from `当前待实现项` into `当前已完成项`, changing it to:

```markdown
- `[已完成]` 标签类型列表行点击切换当前类别：点击分类列表中某个标签行的主体区域可切换 `currentClass` / `active class`；行内 `重命名`、`预览类别 ROI`、`删除类别` 仍作为独立 `command action`。提交 `<本任务最终提交号>`。已验证 `registered_classification_dialog_smoke`、主工程 `qmake && make`、`git diff --check`。
```

Also change the section marker:

```markdown
`[待实现]` 标签类型列表行点击切换当前类别。
```

to:

```markdown
`[已完成]` 标签类型列表行点击切换当前类别。
```

- [x] **Step 2: Update implementation record**

Append this dated note to `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`:

```markdown
### 2026-07-07 注册训练窗口四阶段：标签类型行点击切换当前类别

#### 已实现功能

- 分类列表行主体区域支持 `current item selection`，点击标签名称、统计区域或行空白区域会切换当前类别。
- 被选中类别成为后续 ROI 标注的 `active class` / `current class`。
- 行内 `重命名`、`预览类别 ROI`、`删除类别` 保持独立 `command action`，不把按钮区域作为行主体点击处理。
- 预览页打开时点击其他类别行主体区域，会返回大图页并切换到该类别。
- 切换类别时退出矩形/多边形 ROI 绘制状态，避免后续 ROI 写入错误类别。

#### 验证结果

- `smoke/registered_classification_dialog_smoke` 通过。
- 影子目录执行 `/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro && make -j8` 通过。
- `git diff --check` 通过。
```

- [x] **Step 3: Run smoke**

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: `registered_classification_dialog_smoke: all checks passed`.

- [x] **Step 4: Run main build**

```bash
mkdir -p build
cd build
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
make -j8
```

Expected: build exits 0.

- [x] **Step 5: Run diff check**

```bash
git diff --check
```

Expected: exits 0.

- [x] **Step 6: Mark plan checkboxes complete**

After implementation and verification, replace completed `- [ ]` checklist items in this plan with `- [x]`.

- [x] **Step 7: Commit**

```bash
git add src/RegisteredClassificationTrainingDialog.cpp \
        smoke/registered_classification_dialog_smoke.cpp \
        docs/FID/RegisteredClassification/注册分类提示词规范.md \
        docs/FID/RegisteredClassification/registered_classification_function_implementation.md \
        docs/superpowers/plans/2026-07-07-registered-classification-class-row-selection.md
git commit -m "feat: add registered classification class row selection"
```

- [x] **Step 8: Backfill completion commit hash**

After Step 7 creates the feature commit, run:

```bash
COMMIT=$(git rev-parse --short HEAD)
```

Replace `<本任务最终提交号>` in `docs/FID/RegisteredClassification/注册分类提示词规范.md` with that short hash, then amend the same commit:

```bash
git add docs/FID/RegisteredClassification/注册分类提示词规范.md
git commit --amend --no-edit
```
