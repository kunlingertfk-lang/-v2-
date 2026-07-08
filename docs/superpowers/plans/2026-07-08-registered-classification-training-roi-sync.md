# Registered Classification Training ROI Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sync registered classification training ROI preview and deletion behavior with the registered target detection training window.

**Architecture:** Keep the change local to `RegisteredClassificationTrainingDialog.cpp`, mirroring the target detection window's existing event-filter, context-menu, and dark preview-page patterns. Use smoke coverage to lock tooltip, preview-card deletion, dark preview styling, and large-image right-click deletion.

**Tech Stack:** Qt 5.15.2 widgets, qmake, existing `FrameViewHelper`, existing smoke test executable.

## Global Constraints

- Do not implement real HALCON training, dataset persistence, model generation, or model export.
- Do not modify `RegisteredClassificationDialog`, `RegisteredClassificationAdapter`, or `RegisteredClassificationHalconRunner`.
- Do not extract shared ROI preview components in this change.
- Update `docs/FID/RegisteredClassification/注册分类提示词规范.md` so the completed prompt functionality is recorded there.
- Verify with `git diff --check`, `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro`, `make -j$(nproc)`, and the registered classification smoke.

---

### Task 1: Add Smoke Coverage For Synced ROI Preview And Deletion

**Files:**
- Modify: `smoke/registered_classification_dialog_smoke.cpp`

**Interfaces:**
- Consumes: existing `RegisteredClassificationTrainingDialog` object names:
  - `registeredTrainingPreviewPage`
  - `registeredTrainingRoiPreviewCard_<class>_<image>_<roi>`
  - `registeredTrainingDeleteRoiButton_<class>_<image>_<roi>` after implementation
  - `registeredTrainingPreviewRoiContextMenu` after implementation
  - `registeredTrainingRoiContextMenu` after implementation
  - `registeredTrainingPreviewView`
- Produces: failing smoke assertions for Task 2 to satisfy.

- [ ] **Step 1: Add required Qt includes**

Add these includes near the other Qt includes:

```cpp
#include <QContextMenuEvent>
#include <QGraphicsView>
#include <QMenu>
```

- [ ] **Step 2: Add preview-page style assertions**

After the smoke opens a registered classification ROI preview page, add assertions equivalent to:

```cpp
QWidget *previewPage = trainingDialog->findChild<QWidget *>(
            QStringLiteral("registeredTrainingPreviewPage"));
check(previewPage && previewPage->isVisible(),
      "classification preview button must open class ROI preview page");

QLabel *previewPageTitle = nullptr;
const QList<QLabel *> previewLabels = trainingDialog->findChildren<QLabel *>();
for (QLabel *label : previewLabels) {
    if (label && label->text().contains(QStringLiteral("已标注目标"))) {
        previewPageTitle = label;
        break;
    }
}
check(previewPageTitle && previewPageTitle->styleSheet().isEmpty(),
      "classification preview title should rely on role QSS rather than inline style");
check(trainingDialog->styleSheet().contains(
          QStringLiteral("QLabel[role=\"previewPageTitle\"]{font-size:26px;font-weight:800;color:#ffffff;}")),
      "classification ROI preview page title must use white text on dark preview page");
```

- [ ] **Step 3: Add preview-card delete assertions**

After locating the first classification ROI preview card, assert the delete icon and context menu:

```cpp
QToolButton *deleteRoiButton = trainingDialog->findChild<QToolButton *>(
            QStringLiteral("registeredTrainingDeleteRoiButton_0_0_0"));
check(deleteRoiButton != nullptr,
      "classification ROI preview card must expose delete ROI button");
check(deleteRoiButton && deleteRoiButton->toolTip() == QStringLiteral("删除当前 ROI"),
      "hovering classification ROI delete icon must explain delete current ROI");
check(deleteRoiButton && deleteRoiButton->statusTip() == QStringLiteral("删除当前 ROI"),
      "classification ROI delete icon must expose delete current ROI status tip");

QWidget *roiPreviewCard = trainingDialog->findChild<QWidget *>(
            QStringLiteral("registeredTrainingRoiPreviewCard_0_0_0"));
check(roiPreviewCard != nullptr,
      "classification ROI preview card must have a stable object name");
if (roiPreviewCard) {
    const QPoint cardCenter = roiPreviewCard->rect().center();
    QContextMenuEvent cardContextEvent(QContextMenuEvent::Mouse,
                                       cardCenter,
                                       roiPreviewCard->mapToGlobal(cardCenter));
    QApplication::sendEvent(roiPreviewCard, &cardContextEvent);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QMenu *previewMenu = nullptr;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        QMenu *candidate = qobject_cast<QMenu *>(widget);
        if (candidate && candidate->objectName() == QStringLiteral("registeredTrainingPreviewRoiContextMenu")) {
            previewMenu = candidate;
            break;
        }
    }
    check(previewMenu != nullptr,
          "right-clicking classification ROI preview card must open delete-current-ROI menu");
    if (previewMenu) {
        QAction *deleteAction = nullptr;
        for (QAction *action : previewMenu->actions()) {
            if (action && action->text() == QStringLiteral("删除当前 ROI"))
                deleteAction = action;
        }
        check(deleteAction != nullptr,
              "classification ROI preview context menu must contain delete-current-ROI action");
        if (deleteAction)
            deleteAction->trigger();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
}
```

- [ ] **Step 4: Add large-image right-click delete assertions**

After recreating a classification ROI and returning to the large-image page, add:

```cpp
QGraphicsView *trainingView = trainingDialog->findChild<QGraphicsView *>(
            QStringLiteral("registeredTrainingPreviewView"));
check(trainingView != nullptr,
      "classification training preview view must exist for ROI context menu");
if (trainingView) {
    const QPoint center = trainingView->viewport()->rect().center();
    QContextMenuEvent contextEvent(QContextMenuEvent::Mouse,
                                   center,
                                   trainingView->viewport()->mapToGlobal(center));
    QApplication::sendEvent(trainingView->viewport(), &contextEvent);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QMenu *menu = nullptr;
    for (QWidget *widget : QApplication::topLevelWidgets()) {
        QMenu *candidate = qobject_cast<QMenu *>(widget);
        if (!candidate)
            continue;
        for (QAction *action : candidate->actions()) {
            if (action && action->text() == QStringLiteral("删除当前 ROI")) {
                menu = candidate;
                break;
            }
        }
        if (menu)
            break;
    }
    check(menu != nullptr,
          "right-clicking selected classification ROI must open delete-current-ROI menu");
    if (menu) {
        for (QAction *action : menu->actions()) {
            if (action && action->text() == QStringLiteral("删除当前 ROI")) {
                action->trigger();
                break;
            }
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    check(labelByText(*trainingDialog, QStringLiteral("0 / 0")) != nullptr,
          "right-click delete current classification ROI must update class count to zero");
}
```

- [ ] **Step 5: Run smoke and confirm it fails before implementation**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
./registered_classification_dialog_smoke
```

Expected: FAIL on missing delete button object name, missing dark preview QSS, or missing large-image context menu.

### Task 2: Implement Registered Classification ROI Sync

**Files:**
- Modify: `src/RegisteredClassificationTrainingDialog.cpp`

**Interfaces:**
- Consumes: existing `TrainingRoiMark`, `TrainingImageState::marksByClass`, `TrainingSessionState::currentImage`, `TrainingSessionState::currentClass`, `FrameViewHelper::viewToImage`.
- Produces:
  - `registeredTrainingDeleteRoiButton_<class>_<image>_<roi>`
  - `registeredTrainingPreviewRoiContextMenu`
  - `registeredTrainingRoiContextMenu`
  - dark preview-page QSS matching target detection.

- [ ] **Step 1: Add required includes**

Add near existing includes:

```cpp
#include <QContextMenuEvent>
#include <QMenu>
```

If not already present, add:

```cpp
#include <functional>
```

- [ ] **Step 2: Add ROI hit-test helper**

Near `imageCropRect`, add:

```cpp
bool markContainsNormalizedPoint(const TrainingRoiMark &mark, const QPointF &point)
{
    if (point.x() < 0.0 || point.x() > 1.0 || point.y() < 0.0 || point.y() > 1.0)
        return false;
    if (mark.type == QStringLiteral("polygon") && mark.polygon.size() >= 3) {
        QPolygonF polygon;
        for (const QPointF &polygonPoint : mark.polygon)
            polygon << polygonPoint;
        return polygon.containsPoint(point, Qt::OddEvenFill);
    }
    return mark.rect.normalized().contains(point);
}
```

- [ ] **Step 3: Add viewport context-menu event filter**

Near the existing small helper classes, add:

```cpp
class TrainingRoiContextMenuFilter : public QObject
{
public:
    explicit TrainingRoiContextMenuFilter(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    std::function<void(const QPoint &, const QPoint &)> openMenu;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)
        if (!openMenu)
            return false;
        if (event->type() == QEvent::ContextMenu) {
            QContextMenuEvent *contextEvent = static_cast<QContextMenuEvent *>(event);
            openMenu(contextEvent->pos(), contextEvent->globalPos());
            event->accept();
            return true;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::RightButton) {
                openMenu(mouseEvent->pos(), mouseEvent->globalPos());
                event->accept();
                return true;
            }
        }
        return false;
    }
};
```

- [ ] **Step 4: Add transient selected ROI fields**

Extend `TrainingSessionState`:

```cpp
int selectedRoiImage = -1;
int selectedRoiClass = -1;
int selectedRoiIndex = -1;
```

- [ ] **Step 5: Add shared delete function**

After `returnToImagePage` and before preview-card rendering code, create a shared function pointer:

```cpp
QSharedPointer<std::function<void(int, int, int, const QString &)>> deleteRoi(
            new std::function<void(int, int, int, const QString &)>);
```

Then define it after `renderPreviewPage` can be called:

```cpp
*deleteRoi = [=](const int imageIndex, const int classIndex, const int roiIndex, const QString &message) {
    if (imageIndex < 0 || imageIndex >= state->images.size())
        return;
    if (classIndex < 0 || classIndex >= state->classes.size())
        return;
    QVector<TrainingRoiMark> &marks = state->images[imageIndex].marksByClass[classIndex];
    if (roiIndex < 0 || roiIndex >= marks.size())
        return;
    marks.removeAt(roiIndex);
    if (marks.isEmpty())
        state->images[imageIndex].marksByClass.remove(classIndex);
    state->selectedRoiImage = -1;
    state->selectedRoiClass = -1;
    state->selectedRoiIndex = -1;
    state->activeEditRoi = -1;
    refreshStatus();
    refreshThumbnails();
    refreshClassList();
    if (state->previewClass >= 0)
        (*renderPreviewPage)(state->previewClass);
    else
        (*showCurrentImage)(message);
    editStatusLabel->setText(message);
};
```

Adjust captures so `refreshStatus`, `refreshThumbnails`, `refreshClassList`, `renderPreviewPage`, and `showCurrentImage` are available.

- [ ] **Step 6: Update preview-card delete UI**

In `renderPreviewPage`, when creating the card, add a `QToolButton` delete icon with:

```cpp
deleteRoiButton->setObjectName(QStringLiteral("registeredTrainingDeleteRoiButton_%1_%2_%3")
                               .arg(classIndex)
                               .arg(imageIndex)
                               .arg(roiIndex));
deleteRoiButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
deleteRoiButton->setIconSize(QSize(20, 20));
deleteRoiButton->setToolTip(QObject::tr("删除当前 ROI"));
deleteRoiButton->setStatusTip(QObject::tr("删除当前 ROI"));
deleteRoiButton->setWhatsThis(QObject::tr("删除当前 ROI"));
deleteRoiButton->setMinimumSize(34, 34);
```

Connect it to:

```cpp
QObject::connect(deleteRoiButton, &QToolButton::clicked, card, [=]() {
    (*deleteRoi)(imageIndex, classIndex, roiIndex, QObject::tr("已删除当前 ROI"));
});
```

- [ ] **Step 7: Update preview-card context menu**

For the card, preview image, and caption, set `Qt::CustomContextMenu` and connect to a menu:

```cpp
auto openPreviewRoiMenu = [=](const QPoint &globalPos) {
    QMenu *menu = new QMenu(card);
    menu->setObjectName(QStringLiteral("registeredTrainingPreviewRoiContextMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QAction *deleteAction = menu->addAction(QObject::tr("删除当前 ROI"));
    QObject::connect(deleteAction, &QAction::triggered, menu, [=]() {
        menu->close();
        (*deleteRoi)(imageIndex, classIndex, roiIndex, QObject::tr("已删除当前 ROI"));
    });
    menu->popup(globalPos);
};
```

- [ ] **Step 8: Add large-image ROI context menu**

Create `roiIndexAtViewPos` and install the event filter on `view->viewport()`:

```cpp
auto roiIndexAtViewPos = [=](const QPoint &viewPos) {
    TrainingImageState *image = currentImageState();
    if (!image || image->image.isNull())
        return -1;
    const QPointF imagePoint = previewHelper->viewToImage(viewPos);
    if (imagePoint.x() < 0.0 || imagePoint.y() < 0.0 ||
        imagePoint.x() > image->image.width() || imagePoint.y() > image->image.height())
        return -1;
    const QPointF normalizedPoint(imagePoint.x() / qMax(1, image->image.width()),
                                  imagePoint.y() / qMax(1, image->image.height()));
    const QVector<TrainingRoiMark> marks = image->marksByClass.value(state->currentClass);
    for (int index = marks.size() - 1; index >= 0; --index) {
        if (markContainsNormalizedPoint(marks.at(index), normalizedPoint))
            return index;
    }
    return -1;
};

TrainingRoiContextMenuFilter *roiContextMenuFilter = new TrainingRoiContextMenuFilter(view);
roiContextMenuFilter->openMenu = [=](const QPoint &viewPos, const QPoint &globalPos) {
    if (state->currentImage < 0 || state->currentImage >= state->images.size()) {
        editStatusLabel->setText(QObject::tr("请先添加注册图"));
        return;
    }
    if (state->previewClass >= 0) {
        editStatusLabel->setText(QObject::tr("请先返回注册图大图"));
        return;
    }
    const int roiIndex = roiIndexAtViewPos(viewPos);
    if (roiIndex < 0) {
        editStatusLabel->setText(QObject::tr("请右键目标 ROI"));
        return;
    }
    state->selectedRoiImage = state->currentImage;
    state->selectedRoiClass = state->currentClass;
    state->selectedRoiIndex = roiIndex;
    QMenu *menu = new QMenu(view);
    menu->setObjectName(QStringLiteral("registeredTrainingRoiContextMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QAction *deleteAction = menu->addAction(QObject::tr("删除当前 ROI"));
    QObject::connect(deleteAction, &QAction::triggered, menu, [=]() {
        menu->close();
        (*deleteRoi)(state->selectedRoiImage,
                     state->selectedRoiClass,
                     state->selectedRoiIndex,
                     QObject::tr("已删除当前 ROI"));
    });
    menu->popup(globalPos);
    editStatusLabel->setText(QObject::tr("已选中 ROI %1").arg(roiIndex + 1));
};
view->viewport()->installEventFilter(roiContextMenuFilter);
```

- [ ] **Step 9: Sync dark preview-page QSS**

Change the registered classification training dialog style string:

```cpp
"QWidget[panelRole=\"roiPreviewPage\"]{background:#2d333f;border:0;}"
"QLabel[role=\"previewPageTitle\"]{font-size:26px;font-weight:800;color:#ffffff;}"
"QLabel[role=\"previewEmpty\"]{font-size:22px;font-weight:700;color:#e2e8f0;}"
```

- [ ] **Step 10: Run smoke until passing**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
./registered_classification_dialog_smoke
```

Expected: PASS.

### Task 3: Update Registered Classification Prompt Documentation

**Files:**
- Modify: `docs/FID/RegisteredClassification/注册分类提示词规范.md`
- Optional modify: `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`

**Interfaces:**
- Consumes: completed behavior from Task 2.
- Produces: prompt docs that no longer describe large-image ROI deletion as a future optimization.

- [ ] **Step 1: Update completed-status bullets**

In the existing completed status area, replace any statement that says large-image overlay ROI selection/deletion is future work with a completed note:

```markdown
- `[已完成]` 注册分类训练窗口 ROI 预览与删除同步：ROI 预览页已同步注册目标检测的深色背景和高对比标题/空态样式；预览卡提供 `删除当前 ROI` 图标 tooltip/statusTip/whatsThis 和右键菜单；大图编辑页支持右键命中当前图、当前类别 ROI 后删除单个 ROI。已验证 `registered_classification_dialog_smoke`、主工程 `qmake && make`、`git diff --check`。
```

- [ ] **Step 2: Update ROI interaction requirements**

In the ROI interaction section, replace:

```markdown
- 不引入大图 overlay 点击选中 ROI；该项仍为 `[后续优化]`。
```

with:

```markdown
- 大图编辑页支持右键命中当前图、当前类别的 ROI，弹出 `删除当前 ROI` 菜单并删除单个 ROI；未命中时只显示明确状态提示，不删除任何内容。
```

- [ ] **Step 3: Add verification bullets**

In the verification section for registered classification training, add:

```markdown
- smoke 覆盖 ROI 预览页深色背景、标题高对比样式和删除按钮 tooltip/statusTip。
- smoke 覆盖 ROI 预览卡右键删除单个 ROI。
- smoke 覆盖大图右键命中当前类别 ROI 后删除单个 ROI，并刷新类别计数。
```

- [ ] **Step 4: Review docs diff**

Run:

```bash
git diff -- docs/FID/RegisteredClassification/注册分类提示词规范.md docs/FID/RegisteredClassification/registered_classification_function_implementation.md
```

Expected: only status/requirements/verification text changes related to this feature.

### Task 4: Final Verification

**Files:**
- Verify all modified files.

**Interfaces:**
- Consumes: Tasks 1-3.
- Produces: verified working tree changes ready for the user.

- [ ] **Step 1: Run whitespace check**

Run:

```bash
git diff --check
```

Expected: no output.

- [ ] **Step 2: Run registered classification smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
./registered_classification_dialog_smoke
```

Expected: PASS.

- [ ] **Step 3: Run main qmake and make from shadow build**

Run:

```bash
mkdir -p build
cd build
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
make -j$(nproc)
```

Expected: build completes successfully.

- [ ] **Step 4: Summarize changed files and verification**

Report:

```text
Changed:
- src/RegisteredClassificationTrainingDialog.cpp
- smoke/registered_classification_dialog_smoke.cpp
- docs/FID/RegisteredClassification/注册分类提示词规范.md

Verified:
- git diff --check
- smoke/registered_classification_dialog_smoke
- qmake ../qt_ui_test.pro && make
```
