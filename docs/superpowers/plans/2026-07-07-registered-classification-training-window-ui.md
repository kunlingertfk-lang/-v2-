# Registered Classification Training Window UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Implement the registered classification training-window second-phase UI described in `注册分类提示词规范.md`.

**Architecture:** Keep the change inside `RegisteredClassificationTrainingDialog` and the existing dialog smoke test. Use a small in-dialog session state for imported/captured images, categories, ROI caches, thumbnail selection, and category preview; do not persist datasets or invoke training.

**Tech Stack:** Qt Widgets/C++17, existing `FrameViewHelper`, existing `CameraFrameProvider`, existing smoke test project `smoke/registered_classification_dialog_smoke.pro`.

## Global Constraints

- Only implement training-window UI and interaction closure.
- Do not implement real HALCON training.
- Do not implement real dataset persistence.
- Do not generate model files.
- Do not support `.scbin`.
- Do not modify the main registered classification Dialog inference, Adapter, or Runner chain.
- Do not introduce OpenCV, custom classifiers, or third-party libraries as core classification algorithms.
- Reuse the existing `RegisteredClassificationTrainingDialog` visual style.

---

### Task 1: Write Failing Smoke Coverage

**Files:**
- Modify: `smoke/registered_classification_dialog_smoke.cpp`

**Interfaces:**
- Consumes: Existing training dialog object names `trainingCameraCaptureButton`, `trainingRectRoiButton`, `trainingPolygonRoiButton`, `registeredTrainingPreviewHelper`.
- Produces: Smoke assertions for new object names:
  - `registeredTrainingThumbnailList`
  - `registeredTrainingClassList`
  - `registeredTrainingClassCountLabel`
  - `registeredTrainingCreateClassButton`
  - `registeredTrainingClearAllMarksButton`
  - `registeredTrainingPreviewClassButton_0`
  - `registeredTrainingDeleteClassMarkButton_0`

- [x] **Step 1: Add Qt includes**

Add:

```cpp
#include <QListWidget>
```

- [x] **Step 2: Add smoke assertions after camera capture**

After the existing camera capture and ROI button assertions in the training dialog block, assert:

```cpp
QListWidget *thumbnailList = trainingDialog->findChild<QListWidget *>(
            QStringLiteral("registeredTrainingThumbnailList"));
check(thumbnailList != nullptr, "training dialog must have thumbnail list");
check(thumbnailList && thumbnailList->count() == 1,
      "camera capture must add one thumbnail");
check(thumbnailList && thumbnailList->currentRow() == 0,
      "camera capture thumbnail must be selected");
check(thumbnailList && thumbnailList->item(0)->text().contains(QStringLiteral("相机抓图")),
      "camera capture thumbnail must show source name");

QWidget *classList = trainingDialog->findChild<QWidget *>(
            QStringLiteral("registeredTrainingClassList"));
QLabel *classCountLabel = trainingDialog->findChild<QLabel *>(
            QStringLiteral("registeredTrainingClassCountLabel"));
QPushButton *createClassButton = buttonByText(*trainingDialog, QStringLiteral("+ 新建"));
QToolButton *previewClassButton = toolButtonByObjectName(*trainingDialog,
            QStringLiteral("registeredTrainingPreviewClassButton_0"));
QToolButton *deleteClassMarkButton = toolButtonByObjectName(*trainingDialog,
            QStringLiteral("registeredTrainingDeleteClassMarkButton_0"));
QPushButton *clearAllMarksButton = buttonByText(*trainingDialog, QStringLiteral("清除全部标注"));
check(classList != nullptr, "training dialog must use class list container");
check(classCountLabel && classCountLabel->text() == QStringLiteral("分类列表(1)"),
      "class list title must show default class count");
check(createClassButton != nullptr, "training dialog must have create class button");
check(previewClassButton && previewClassButton->toolTip() == QStringLiteral("预览类别 ROI"),
      "class row preview button must have semantic tooltip");
check(deleteClassMarkButton && deleteClassMarkButton->toolTip() == QStringLiteral("删除当前 ROI"),
      "class row delete button must have semantic tooltip");
check(clearAllMarksButton != nullptr, "training dialog must have clear all marks button");
```

- [x] **Step 3: Add ROI/cache interaction assertions**

After the existing polygon toggle checks, add:

```cpp
if (trainingPreviewHelper && rectRoiButton && thumbnailList &&
    previewClassButton && deleteClassMarkButton && clearAllMarksButton) {
    trainingPreviewHelper->setRoiRectNormalized(QRectF(0.12, 0.15, 0.32, 0.28));
    emit trainingPreviewHelper->roiChanged(QRectF(0.12, 0.15, 0.32, 0.28));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    check(thumbnailList->item(0)->text().contains(QStringLiteral("已标注")),
          "ROI completion must mark current thumbnail as annotated");
    check(classCountLabel && classCountLabel->text().contains(QStringLiteral("1")),
          "ROI completion must update class list count");
    clickAndProcess(previewClassButton);
    check(!trainingPreviewHelper->isRoiDrawingEnabled() &&
          !trainingPreviewHelper->isPolygonDrawingEnabled(),
          "class preview must leave ROI drawing modes idle");
    clickAndProcess(deleteClassMarkButton);
    check(!thumbnailList->item(0)->text().contains(QStringLiteral("已标注")),
          "deleting current ROI must clear thumbnail annotation state");
    trainingPreviewHelper->setPolygonRoiNormalized(QVector<QPointF>()
            << QPointF(0.1, 0.1) << QPointF(0.4, 0.1) << QPointF(0.2, 0.4));
    emit trainingPreviewHelper->polygonChanged(QVector<QPointF>()
            << QPointF(0.1, 0.1) << QPointF(0.4, 0.1) << QPointF(0.2, 0.4));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    clickAndProcess(clearAllMarksButton);
    check(!thumbnailList->item(0)->text().contains(QStringLiteral("已标注")),
          "clear all marks must clear thumbnail annotation state");
}
if (createClassButton && classCountLabel) {
    clickAndProcess(createClassButton);
    check(classCountLabel->text() == QStringLiteral("分类列表(2)"),
          "create class button must append a new class");
    check(toolButtonByObjectName(*trainingDialog,
          QStringLiteral("registeredTrainingPreviewClassButton_1")) != nullptr,
          "new class row must have preview button");
}
```

- [x] **Step 4: Run smoke and verify it fails**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: executable builds, then fails because new controls/object names are missing.

### Task 2: Implement Training-Window Session UI

**Files:**
- Modify: `src/RegisteredClassificationTrainingDialog.cpp`

**Interfaces:**
- Consumes: `FrameViewHelper::setImage`, `setRoiRectNormalized`, `setPolygonRoiNormalized`, `clearRoi`, `clearPolygonRoi`, `setRoiDrawingEnabled`, `setPolygonDrawingEnabled`.
- Produces: New UI object names and in-session behavior asserted by Task 1.

- [x] **Step 1: Add includes**

Add Qt includes for `QListWidget`, `QListWidgetItem`, `QMap`, `QScrollArea`, and `QSharedPointer`.

- [x] **Step 2: Replace temporary class table**

Replace the `QTableWidget` class table with:

```cpp
QLabel *classCountLabel = new QLabel(tr("分类列表(1)"), markCard);
classCountLabel->setObjectName(QStringLiteral("registeredTrainingClassCountLabel"));
QPushButton *createClassButton = smallButton(markCard, tr("+ 新建"));
createClassButton->setObjectName(QStringLiteral("registeredTrainingCreateClassButton"));
QPushButton *clearAllMarksButton = smallButton(markCard, tr("清除全部标注"));
clearAllMarksButton->setObjectName(QStringLiteral("registeredTrainingClearAllMarksButton"));
QWidget *classListWidget = new QWidget(markCard);
classListWidget->setObjectName(QStringLiteral("registeredTrainingClassList"));
```

Render each class row with labels for class name and `目标总数/图像总数`, plus semantic icon buttons with tooltips `重命名`, `预览类别 ROI`, and `删除当前 ROI`.

- [x] **Step 3: Add thumbnail strip**

Below the preview status bar, add:

```cpp
QListWidget *thumbnailList = new QListWidget(previewPanel);
thumbnailList->setObjectName(QStringLiteral("registeredTrainingThumbnailList"));
thumbnailList->setViewMode(QListView::IconMode);
thumbnailList->setIconSize(QSize(96, 72));
thumbnailList->setFixedHeight(150);
```

- [x] **Step 4: Add session state and refresh lambdas**

Use local structs in the constructor:

```cpp
struct MarkState { QString type; QRectF rect; QVector<QPointF> polygon; bool hasMark = false; };
struct TrainingImageState { QImage image; QString name; QMap<int, MarkState> marksByClass; };
struct TrainingState { QVector<TrainingImageState> images; QStringList classes; int currentImage = -1; int currentClass = 0; };
```

Initialize `classes` with `Classification0`. Add lambdas:
- `currentImageState()`
- `imageHasAnyMark(int imageIndex)`
- `classTargetCount(int classIndex)`
- `classImageCount(int classIndex)`
- `refreshStatus()`
- `refreshThumbnails()`
- `refreshClassList()`
- `showCurrentImage()`
- `storeCurrentMark(const MarkState &mark)`
- `clearCurrentClassMark()`
- `clearAllMarks()`

- [x] **Step 5: Wire interactions**

Update existing camera/external import lambdas to append a `TrainingImageState`, add/select thumbnail, and call `showCurrentImage()`.

Wire:
- thumbnail `currentRowChanged` to switch current image.
- `+ 新建` to append `ClassificationN`.
- preview button to show current class ROI on the left and disable drawing modes.
- delete button to remove current class mark from current image.
- clear all button to remove all marks and clear ROI overlays.
- `roiChanged` and `polygonChanged` to store marks for the current image/current class, update counts, and mark thumbnail as `已标注`.

If no image exists when ROI mode is clicked, keep drawing disabled and show `请先添加注册图`.

- [x] **Step 6: Run smoke and verify it passes**

Run the same smoke command from Task 1.

Expected: `registered_classification_dialog_smoke: all checks passed`.

### Task 3: Build and Verify

**Files:**
- Modify: `src/RegisteredClassificationTrainingDialog.cpp`
- Modify: `smoke/registered_classification_dialog_smoke.cpp`
- Optional docs: `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`

**Interfaces:**
- Consumes: Completed Task 1 and Task 2.
- Produces: Verified project build and smoke coverage.

- [x] **Step 1: Run smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

- [x] **Step 2: Run main build**

Run from repo root:

```bash
mkdir -p build
cd build
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
make -j8
```

- [x] **Step 3: Run diff check**

Run:

```bash
git diff --check
```

- [x] **Step 4: Commit**

Commit source, smoke, plan, and any implementation record update:

```bash
git add src/RegisteredClassificationTrainingDialog.cpp smoke/registered_classification_dialog_smoke.cpp docs/superpowers/plans/2026-07-07-registered-classification-training-window-ui.md
git commit -m "feat: update registered classification training window ui"
```
