# Registered Classification ROI Finish State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Synchronize the main registered classification ROI button highlight with the actual ROI editing state when the user clicks “完成”, while preserving the ROI and continuous test mode.

**Architecture:** Keep the fix inside `RegisteredClassificationDialog::finishRoiEditing()`. The method already owns the editing state transition and helper shutdown; adding `refreshUiState()` synchronizes the checkable button state without changing ROI data or test behavior. A focused smoke assertion locks the regression.

**Tech Stack:** Qt 5 widgets, existing `FrameViewHelper`, registered classification dialog smoke executable.

## Global Constraints

- Do not change HALCON feature extraction, MLP training, model format, or classification behavior.
- Do not clear the current ROI, result overlay, or continuous reference-test mode.
- Preserve unrelated user changes in `projects/scheme_0d5611a7/reference.png`, `projects/scheme_0d5611a7/scheme.json`, and `docs/FID/RegisteredClassification/tempFunc.md`.
- Current feature report remains documentation only: 28 HALCON ROI statistics in `halcon_mlp_roi_stats_v1`.

## File Map

- Modify `smoke/registered_classification_dialog_smoke.cpp`: add the failing assertion that “完成” disables ROI drawing and clears the ROI button highlight while preserving ROI.
- Modify `src/RegisteredClassificationDialog.cpp`: call `refreshUiState()` during ROI finish state transition.
- Modify `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`: record the completed fix and current feature/data diagnosis.
- Modify `docs/FID/RegisteredClassification/注册分类提示词规范.md`: specify the required finish-button state contract.

### Task 1: Add the failing regression assertion

**Files:**
- Modify: `smoke/registered_classification_dialog_smoke.cpp`

**Interfaces:**
- Use `registeredClassificationRectRegionButton`, `registeredClassificationRoiFinishButton`, and the existing `FrameViewHelper` child.
- Use `RegisteredClassificationDialog::referencePreviewSnapshot()` to verify the latest ROI remains stored.

- [ ] **Step 1: Add the assertion**

After emitting a valid rectangle ROI and clicking the stable ROI finish button, assert:

```cpp
check(!previewHelper->isRoiDrawingEnabled(), "ROI finish must disable helper drawing");
check(!rectButton->isChecked(), "ROI finish must clear rectangle ROI button highlight");
check(qAbs(dialog.referencePreviewSnapshot().roiNormalized.x() - roi.x()) < 0.0001,
      "ROI finish must preserve the current ROI");
```

- [ ] **Step 2: Run the focused smoke and verify it fails**

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: the helper assertion passes, but the rectangle button highlight assertion fails because `finishRoiEditing()` does not refresh UI state.

### Task 2: Synchronize the finish state

**Files:**
- Modify: `src/RegisteredClassificationDialog.cpp`

- [ ] **Step 1: Apply the minimal root-cause fix**

Update `RegisteredClassificationDialog::finishRoiEditing()` to execute this order:

```cpp
m_roiEditing = false;
if (m_previewHelper)
    m_previewHelper->setRoiDrawingEnabled(false);
refreshUiState();
refreshRoiOverlay();
setViewerStatusText(roiStatusText()
                    + (m_referenceTestMode ? tr(" | 基准图持续测试已启用") : QString()));
```

- [ ] **Step 2: Run the focused smoke and verify it passes**

Run the Task 1 command. Expected output: `registered_classification_dialog_smoke: all checks passed`.

- [ ] **Step 3: Commit the code and regression test**

```bash
git add src/RegisteredClassificationDialog.cpp smoke/registered_classification_dialog_smoke.cpp
git commit -m "fix: clear registered classification roi finish highlight"
```

### Task 3: Update documentation and complete verification

**Files:**
- Modify: `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`
- Modify: `docs/FID/RegisteredClassification/注册分类提示词规范.md`

- [ ] **Step 1: Record the completed fix and feature diagnosis**

Add a dated `[已完成]` record stating that ROI finish calls `refreshUiState()`, disables ROI drawing, clears only the button highlight, and preserves ROI/result/continuous-test state. Keep the 28-feature list and the current model sample imbalance diagnosis as `[后续优化]` data-quality guidance, not as an algorithm change.

- [ ] **Step 2: Add the strict prompt rule**

Specify that clicking the detection-area “完成” button must leave `FrameViewHelper::isRoiDrawingEnabled()==false`, the rectangle ROI button unchecked, and the latest ROI unchanged.

- [ ] **Step 3: Run the full verification gate**

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
cd ..
mkdir -p build/task-registered-classification-roi-finish
cd build/task-registered-classification-roi-finish
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
make -j$(nproc)
cd ../..
git diff --check
```

Expected: smoke prints `all checks passed`, Qt build exits with status 0, and `git diff --check` prints no diagnostics.

- [ ] **Step 4: Commit documentation**

```bash
git add docs/FID/RegisteredClassification/registered_classification_function_implementation.md docs/FID/RegisteredClassification/注册分类提示词规范.md
git commit -m "docs: record registered classification roi finish state"
```
