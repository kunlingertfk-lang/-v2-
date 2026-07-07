# Registered Classification ROI Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a left-side ROI preview page, multi-ROI annotation, category deletion semantics, and single-ROI deletion from the preview page for the registered classification training window.

**Architecture:** Keep behavior inside `RegisteredClassificationTrainingDialog` session state. Upgrade per-image/per-class ROI storage from a single mark to a list, add a preview-page mode in the existing left preview panel, and extend the existing dialog smoke test before changing production code.

**Tech Stack:** Qt Widgets/C++17, existing `FrameViewHelper`, existing `CameraFrameProvider`/`ReferenceImageProvider`, existing smoke test `smoke/registered_classification_dialog_smoke.pro`.

## Global Constraints

- Do not implement real HALCON training.
- Do not implement real dataset persistence.
- Do not generate model files.
- Do not support `.scbin`.
- Do not modify the main registered classification Dialog inference, Adapter, or Runner chain.
- Do not implement big-image overlay click selection; record it only as a future optimization.
- Keep `projects/scheme_0d5611a7/scheme.json` out of commits; it currently contains only an unrelated `updatedAt` runtime change.

---

### Task 1: Multi-ROI and Preview Page Smoke Coverage

**Files:**
- Modify: `smoke/registered_classification_dialog_smoke.cpp`

**Interfaces:**
- Consumes existing helpers: `toolButtonByObjectName`, `buttonByText`, `labelByText`, `clickAndProcess`.
- Produces smoke requirements for new object names:
  - `registeredTrainingPreviewPage`
  - `registeredTrainingPreviewCloseButton`
  - `registeredTrainingRoiPreviewCard_0_0_0`
  - `registeredTrainingRoiPreviewCard_0_0_1`
  - `registeredTrainingRoiNumberLabel_0`
  - `registeredTrainingRoiNumberLabel_1`

- [ ] **Step 1: Add Qt includes**

Add:

```cpp
#include <QMenu>
```

- [ ] **Step 2: Add second ROI assertion**

Inside the existing training dialog block, after the first `roiChanged(...)` assertion that marks the thumbnail as annotated, emit a second ROI:

```cpp
trainingPreviewHelper->setRoiRectNormalized(QRectF(0.46, 0.18, 0.22, 0.25));
emit trainingPreviewHelper->roiChanged(QRectF(0.46, 0.18, 0.22, 0.25));
QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
check(labelByText(*trainingDialog, QStringLiteral("2")) != nullptr,
      "second ROI must show number label 2 on the big image");
```

- [ ] **Step 3: Add preview page assertions**

After clicking `previewClassButton`, assert the preview page and cards:

```cpp
QWidget *previewPage = trainingDialog->findChild<QWidget *>(
            QStringLiteral("registeredTrainingPreviewPage"));
check(previewPage != nullptr && previewPage->isVisible(),
      "class preview must show ROI preview page");
check(labelByText(*trainingDialog, QStringLiteral("Widget | 已标注目标：2")) != nullptr,
      "preview page header must show class name and ROI count");
check(trainingDialog->findChild<QWidget *>(QStringLiteral("registeredTrainingRoiPreviewCard_0_0_0")) != nullptr,
      "preview page must show first ROI preview card");
check(trainingDialog->findChild<QWidget *>(QStringLiteral("registeredTrainingRoiPreviewCard_0_0_1")) != nullptr,
      "preview page must show second ROI preview card");
```

- [ ] **Step 4: Add toggle-close assertions**

After the preview page assertions:

```cpp
clickAndProcess(previewClassButton);
check(!previewPage->isVisible(),
      "clicking the same preview button again must return to big image");
clickAndProcess(previewClassButton);
QToolButton *previewCloseButton = toolButtonByObjectName(*trainingDialog,
            QStringLiteral("registeredTrainingPreviewCloseButton"));
check(previewCloseButton != nullptr, "preview page must have close button");
clickAndProcess(previewCloseButton);
check(!previewPage->isVisible(),
      "preview close button must return to big image");
clickAndProcess(previewClassButton);
```

- [ ] **Step 5: Add single ROI deletion assertion**

After reopening preview page, right-click the second card:

```cpp
QWidget *secondCard = trainingDialog->findChild<QWidget *>(
            QStringLiteral("registeredTrainingRoiPreviewCard_0_0_1"));
if (secondCard) {
    secondCard->setFocus();
    QContextMenuEvent event(QContextMenuEvent::Mouse,
                            secondCard->rect().center(),
                            secondCard->mapToGlobal(secondCard->rect().center()));
    QApplication::sendEvent(secondCard, &event);
    QMenu *menu = nullptr;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        menu = qobject_cast<QMenu *>(widget);
        if (menu)
            break;
    }
    check(menu != nullptr, "right-clicking selected ROI card must open context menu");
    if (menu) {
        QAction *deleteAction = nullptr;
        for (QAction *action : menu->actions()) {
            if (action && action->text() == QStringLiteral("删除当前 ROI"))
                deleteAction = action;
        }
        check(deleteAction != nullptr, "ROI context menu must contain delete action");
        if (deleteAction)
            deleteAction->trigger();
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}
check(trainingDialog->findChild<QWidget *>(QStringLiteral("registeredTrainingRoiPreviewCard_0_0_1")) == nullptr,
      "deleting one ROI from preview page must remove only that ROI card");
check(labelByText(*trainingDialog, QStringLiteral("Widget | 已标注目标：1")) != nullptr,
      "preview page header must update after deleting one ROI");
```

- [ ] **Step 6: Add category deletion semantics assertions**

After the existing create-class assertion, delete the new category and then delete the last remaining category:

```cpp
QToolButton *deleteSecondClassButton = toolButtonByObjectName(*trainingDialog,
            QStringLiteral("registeredTrainingDeleteClassMarkButton_1"));
check(deleteSecondClassButton != nullptr, "second class row must have delete button");
clickAndProcess(deleteSecondClassButton);
check(classCountLabel->text() == QStringLiteral("分类列表(1)"),
      "deleting a non-last class must remove the class row");
QToolButton *deleteLastClassButton = toolButtonByObjectName(*trainingDialog,
            QStringLiteral("registeredTrainingDeleteClassMarkButton_0"));
clickAndProcess(deleteLastClassButton);
check(classCountLabel->text() == QStringLiteral("分类列表(1)"),
      "deleting the last class must keep one default class");
check(!thumbnailList->item(0)->text().contains(QStringLiteral("已标注")),
      "deleting the last class must clear its ROI marks");
```

- [ ] **Step 7: Run smoke and verify red**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: build succeeds, executable fails on missing preview page/multi-ROI behavior.

### Task 2: Implement Multi-ROI Session Model

**Files:**
- Modify: `src/RegisteredClassificationTrainingDialog.cpp`

**Interfaces:**
- Consumes existing local structs `TrainingMarkState`, `TrainingImageState`, `TrainingSessionState`.
- Produces new local struct `TrainingRoiMark` and `QMap<int, QVector<TrainingRoiMark>> marksByClass`.

- [ ] **Step 1: Replace mark struct**

Replace:

```cpp
struct TrainingMarkState
{
    QString type;
    QRectF rect;
    QVector<QPointF> polygon;
    bool hasMark = false;
};
```

with:

```cpp
struct TrainingRoiMark
{
    QString type;
    QRectF rect;
    QVector<QPointF> polygon;
};
```

Change `TrainingImageState::marksByClass` to:

```cpp
QMap<int, QVector<TrainingRoiMark>> marksByClass;
```

- [ ] **Step 2: Update count helpers**

Change `imageHasAnyMark`, `classTargetCount`, `classImageCount`, and `totalMarkCount` to count non-empty ROI vectors rather than `hasMark`.

- [ ] **Step 3: Update mark storage**

Change `storeCurrentMark` to append:

```cpp
imageState->marksByClass[state->currentClass].append(mark);
```

Do not replace existing ROI marks.

- [ ] **Step 4: Update full/rect/polygon mark creation**

Create `TrainingRoiMark` values without `hasMark`, set `type`, `rect` or `polygon`, and pass them to `storeCurrentMark`.

### Task 3: Add Preview Page and ROI Cards

**Files:**
- Modify: `src/RegisteredClassificationTrainingDialog.cpp`

**Interfaces:**
- Consumes multi-ROI model from Task 2.
- Produces new widgets/object names:
  - `registeredTrainingPreviewPage`
  - `registeredTrainingPreviewCloseButton`
  - `registeredTrainingRoiPreviewCard_<class>_<image>_<roi>`

- [ ] **Step 1: Add includes**

Add:

```cpp
#include <QContextMenuEvent>
#include <QMenu>
#include <QStackedWidget>
```

- [ ] **Step 2: Wrap left workspace in stacked widget**

Replace direct `previewLayout->addWidget(view, 1)` with a `QStackedWidget` containing:

- `imagePage`: the existing `QGraphicsView`.
- `previewPage`: a QWidget named `registeredTrainingPreviewPage`.

Keep the status bar and thumbnail list outside the stacked widget.

- [ ] **Step 3: Build preview page layout**

Create:

- Header label for `<类别名> | 已标注目标：N`.
- Close tool button named `registeredTrainingPreviewCloseButton`.
- Scroll area with card container.
- Empty-state label `当前类别暂无 ROI 标注`.

- [ ] **Step 4: Add crop helper**

Add a local helper lambda/function that returns a QImage:

- full ROI: whole image.
- rect ROI: normalized rect mapped to image pixels.
- polygon ROI: bounding rect of normalized points mapped to image pixels.

Clamp crop rect to the image bounds and return null if invalid.

- [ ] **Step 5: Add render preview page lambda**

`renderPreviewPage(classIndex)` should:

- Set `state->previewClass = classIndex`.
- Switch stacked widget to preview page.
- Clear old cards.
- Render one card per ROI for the class across all images.
- Card object name format: `registeredTrainingRoiPreviewCard_%1_%2_%3`.
- Card label: `<image name> #<roi number>`.
- Enable click selection with selected styling.
- Enable context menu `删除当前 ROI` for selected card.

- [ ] **Step 6: Add return-to-image lambda**

`returnToImagePage(message)` should:

- Clear preview state.
- Switch stacked widget back to image page.
- Call existing `showCurrentImage(message)`.

### Task 4: Wire Preview Toggle and Category Delete

**Files:**
- Modify: `src/RegisteredClassificationTrainingDialog.cpp`

**Interfaces:**
- Consumes preview-page functions from Task 3.
- Produces final preview toggle/delete behavior.

- [ ] **Step 1: Update preview button**

Replace preview button behavior:

- If currently previewing same class, call return-to-image.
- Else call renderPreviewPage(classIndex).

- [ ] **Step 2: Update close button**

Close button calls return-to-image with `当前注册图：<name>`.

- [ ] **Step 3: Update delete class button**

Delete button behavior:

- If more than one class:
  - Remove the class name.
  - Remove all ROI vectors for the class.
  - Reindex class keys greater than deleted class down by one.
- If exactly one class:
  - Keep the class.
  - Clear all ROI vectors for class 0.
- Refresh status, thumbnails, class list, preview/image page.

- [ ] **Step 4: Update showCurrentImage**

Display all ROI marks for current image/current class. If direct drawing through `FrameViewHelper` can only show one ROI, use `ToolOverlay` rectangles/text labels to show all ROI and numeric labels. Keep `FrameViewHelper` ROI/polygon drawing state for the most recent ROI only if needed.

- [ ] **Step 5: Update ROI number labels**

Create text labels through overlay or child labels with object names:

```text
registeredTrainingRoiNumberLabel_0
registeredTrainingRoiNumberLabel_1
```

### Task 5: Verify and Document

**Files:**
- Modify: `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`
- Modify: `docs/superpowers/plans/2026-07-07-registered-classification-roi-preview.md`

**Interfaces:**
- Consumes completed code and smoke test.
- Produces updated implementation record and completed checklist.

- [ ] **Step 1: Run smoke**

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: `registered_classification_dialog_smoke: all checks passed`.

- [ ] **Step 2: Run main build**

```bash
mkdir -p build
cd build
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
make -j8
```

Expected: build exits 0.

- [ ] **Step 3: Run diff check**

```bash
git diff --check
```

Expected: exits 0.

- [ ] **Step 4: Update implementation record**

Append a dated note describing:

- Multi-ROI session model.
- ROI preview page and close/toggle behavior.
- Preview-page single ROI right-click delete.
- Category delete semantics.
- Big-image overlay selection remains future optimization.

- [ ] **Step 5: Mark plan checkboxes complete**

Replace each `- [ ]` with `- [x]` after the step has actually been completed.

- [ ] **Step 6: Commit**

```bash
git add src/RegisteredClassificationTrainingDialog.cpp \
        smoke/registered_classification_dialog_smoke.cpp \
        docs/FID/RegisteredClassification/registered_classification_function_implementation.md \
        docs/superpowers/plans/2026-07-07-registered-classification-roi-preview.md
git commit -m "feat: add registered classification roi preview page"
```
