# Registered Target Detection Training Compact Continuous ROI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the registered target detection training dialog compact on the right side and keep ROI buttons in continuous checked drawing mode until clicked again.

**Architecture:** The implementation is scoped to the detection training dialog, its smoke test, and the FID prompt docs. The UI keeps the existing widget tree and state object, but changes ROI button state handling so drawing remains active after each completed ROI. Layout density is adjusted through card margins, right-panel spacing, and fixed target-list sizing.

**Tech Stack:** Qt 5.15 Widgets, qmake, C++ smoke tests.

## Global Constraints

- Modify only registered target detection files and docs needed by this change.
- Do not modify `RegisteredClassificationTrainingDialog`.
- Do not add real training, data persistence, Adapter, or Runner behavior.
- ROI buttons are continuous drawing toggles: checked means drawing mode is active; clicking the same button again exits.
- Rectangular and polygon ROI drawing modes are mutually exclusive.
- Keep `当前图像帧为空，已获取基准图`.

---

### Task 1: Continuous ROI Smoke Coverage

**Files:**
- Modify: `smoke/registered_classification_detection_dialog_smoke.cpp`

**Interfaces:**
- Consumes: `QToolButton *rectMarkButton`, `QToolButton *polygonMarkButton`, `FrameViewHelper *trainingPreviewHelper`.
- Produces: smoke assertions for continuous checked state and mutual exclusion.

- [ ] **Step 1: Add failing smoke assertions**

After the smoke clicks `相机抓图` and verifies `rectMarkButton` is enabled, add:

```cpp
clickAndProcess(rectMarkButton);
check(rectMarkButton->isChecked(),
      "rectangle mark button must stay checked after entering continuous ROI mode");
check(polygonMarkButton && !polygonMarkButton->isChecked(),
      "polygon mark button must remain unchecked when rectangle mode is active");
```

After emitting the first `roiChanged`, add:

```cpp
check(rectMarkButton->isChecked(),
      "rectangle mark button must remain checked after drawing one ROI");
```

Emit a second ROI and assert count becomes `2 / 1`:

```cpp
if (trainingPreviewHelper) {
    trainingPreviewHelper->setRoiRectNormalized(QRectF(0.45, 0.20, 0.20, 0.20));
    emit trainingPreviewHelper->roiChanged(QRectF(0.45, 0.20, 0.20, 0.20));
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}
check(labelByText(*trainingDialog, QStringLiteral("2 / 1")) != nullptr,
      "continuous rectangle mode must allow drawing a second target without clicking the button again");
```

Then verify toggling off and mutual exclusion:

```cpp
clickAndProcess(rectMarkButton);
check(!rectMarkButton->isChecked(),
      "clicking rectangle mark button again must exit ROI mode");
clickAndProcess(polygonMarkButton);
check(polygonMarkButton->isChecked(),
      "polygon mark button must enter continuous ROI mode");
check(!rectMarkButton->isChecked(),
      "rectangle mark button must be unchecked when polygon mode is active");
```

- [ ] **Step 2: Run focused smoke and verify it fails**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_detection_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_detection_dialog/bin/registered_classification_detection_dialog_smoke
```

Expected: FAIL before implementation because the rectangle button currently becomes unchecked in `showImage()`.

---

### Task 2: Continuous ROI Implementation And Compact Layout

**Files:**
- Modify: `src/RegisteredClassificationDetectionTrainingDialog.cpp`
- Modify: `docs/FID/RegisteredClassificationDetection/注册分类检测提示词规范.md`

**Interfaces:**
- Consumes: existing `showImage`, `startMarkMode`, `roiChanged`, `polygonChanged`, and right-panel layouts.
- Produces: continuous ROI button behavior, compact right-panel layout, updated prompt spec.

- [ ] **Step 1: Preserve active ROI mode during redraw**

In `showImage`, remove the unconditional button reset and only clear drawing helpers when needed. Replace:

```cpp
previewHelper->setRoiDrawingEnabled(false);
previewHelper->setPolygonDrawingEnabled(false);
rectButton->setChecked(false);
polygonButton->setChecked(false);
```

with no-op preservation of the current checked state. After `previewHelper->setImage(image->image);`, reapply drawing state:

```cpp
previewHelper->setRoiDrawingEnabled(rectButton->isChecked());
previewHelper->setPolygonDrawingEnabled(polygonButton->isChecked());
```

- [ ] **Step 2: Make startMarkMode a true toggle**

Keep the existing `requested = button->isChecked()` logic, but ensure:

```cpp
if (!requested) {
    previewHelper->setRoiDrawingEnabled(false);
    previewHelper->setPolygonDrawingEnabled(false);
    rectButton->setChecked(false);
    polygonButton->setChecked(false);
    trainingStatusLabel->setText(QObject::tr("已退出 ROI 绘制"));
    return;
}
```

When enabling rectangle mode:

```cpp
rectButton->setChecked(button == rectButton);
polygonButton->setChecked(false);
previewHelper->setRoiDrawingEnabled(true);
previewHelper->setPolygonDrawingEnabled(false);
```

When enabling polygon mode:

```cpp
rectButton->setChecked(false);
polygonButton->setChecked(button == polygonButton);
previewHelper->setRoiDrawingEnabled(false);
previewHelper->setPolygonDrawingEnabled(true);
```

- [ ] **Step 3: Keep drawing mode active after ROI creation**

After appending a rectangle or polygon mark and calling `showImage(...)`, do not uncheck buttons. Rely on Step 1 to reapply active drawing mode.

- [ ] **Step 4: Compact the right panel**

Adjust the helper and right-side layout:

```cpp
layout->setContentsMargins(14, 10, 14, 10);
layout->setSpacing(6);
rightLayout->setSpacing(6);
```

Keep `markCard` without stretch:

```cpp
rightLayout->addWidget(markCard);
```

Set a bounded target list area so it keeps some room without taking all vertical slack:

```cpp
targetHeader->setMinimumHeight(42);
targetRow->setMinimumHeight(62);
```

Reduce options spacing:

```cpp
angleRow->setContentsMargins(0, 0, 0, 0);
resolutionRow->setContentsMargins(0, 0, 0, 0);
```

- [ ] **Step 5: Update FID prompt spec**

In `docs/FID/RegisteredClassificationDetection/注册分类检测提示词规范.md`, update the ROI section so it states:

```markdown
- ROI 按钮为连续绘制开关：点击进入高亮框选模式，可连续绘制多个 ROI；再次点击同一按钮退出框选。
- `矩形框选` 与 `多边形框选` 互斥，切换模式时另一个按钮取消高亮。
```

Update the right layout section so it states:

```markdown
- 右侧卡片应压缩上下 padding 和 spacing，目标列表保留适当空白，但不得让标注图像或参数卡片撑出大块空白。
```

- [ ] **Step 6: Run focused smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_detection_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_detection_dialog/bin/registered_classification_detection_dialog_smoke
```

Expected: PASS.

---

### Task 3: Regression Verification

**Files:**
- No implementation files.

**Interfaces:**
- Consumes: completed Tasks 1-2.
- Produces: verification evidence.

- [ ] **Step 1: Run registered classification regression smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: PASS.

- [ ] **Step 2: Run main build**

Run:

```bash
mkdir -p build_verify
cd build_verify
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
make -j$(nproc)
```

Expected: PASS.

- [ ] **Step 3: Run diff check**

Run:

```bash
git diff --check
```

Expected: no output.
