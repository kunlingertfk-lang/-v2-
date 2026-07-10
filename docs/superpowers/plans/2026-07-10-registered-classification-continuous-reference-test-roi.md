# Registered Classification Continuous Reference Test ROI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make registered classification reference-image testing reusable so completed ROI edits automatically trigger another test while both test and ROI controls retain their highlighted state.

**Architecture:** Keep the behavior inside `RegisteredClassificationDialog`. Add explicit dialog state for reference-test mode and ROI-editing mode, drive the existing checkable controls from that state, and trigger the existing `runReferenceTest()` from valid ROI completion events. The existing `FrameViewHelper`, `ToolConfig`, adapter, and result display remain unchanged.

**Tech Stack:** Qt 5 widgets/signals, existing `FrameViewHelper`, existing registered classification adapter and smoke executable.

## Global Constraints

- Do not add a new controller, runner, model format, or dependency.
- Do not change the HALCON classification algorithm, model package, `ToolConfig` fields, or result judgment semantics.
- Keep “测试运行” bound to the current camera-frame test behavior.
- Main Dialog detection ROI remains limited to full-screen and rectangle ROI.
- A valid ROI completion event, not draft mouse movement, is the only trigger for automatic retesting.
- Preserve unrelated user changes in `projects/scheme_0d5611a7/reference.png`, `projects/scheme_0d5611a7/scheme.json`, and `docs/FID/RegisteredClassification/tempFunc.md`.

## File Map

- Modify `src/RegisteredClassificationDialog.h`: add continuous-test and ROI-editing state fields plus any test-visible accessors needed by smoke.
- Modify `src/RegisteredClassificationDialog.cpp`: implement mode toggling, state-driven button highlighting, ROI completion retest, full-screen behavior, and status text.
- Modify `smoke/registered_classification_dialog_smoke.cpp`: add failing assertions for persistent button states, repeated ROI-triggered testing, completion behavior, and no-reference handling.
- Modify `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`: document the completed interaction and verification.
- Modify `docs/FID/RegisteredClassification/注册分类提示词规范.md`: add the strict continuous-test interaction contract.

### Task 1: Add failing smoke coverage for continuous testing

**Files:**
- Modify: `smoke/registered_classification_dialog_smoke.cpp`
- Test target: `smoke/registered_classification_dialog_smoke.pro`

**Interfaces:**
- Use existing `RegisteredClassificationDialog` child controls: `基准图测试`, `测试运行`, `registeredClassificationRectRegionButton`, `registeredClassificationRoiFinishButton`.
- Use existing `FrameViewHelper::isRoiDrawingEnabled()` and `emit roiChanged(...)` test seam.

- [ ] **Step 1: Write the failing assertions**

After setting a reference frame and opening the dialog, assert that clicking `基准图测试` makes the test button checked. Click the rectangle ROI control, assert it is checked and the preview helper reports drawing enabled, emit two different valid ROI changes, and assert the test status/preview reflects each latest ROI without another test-button click. Click `完成`, assert ROI drawing is disabled while the test button remains checked and the stored ROI remains the second ROI. Click `基准图测试` again and assert it is unchecked while the ROI remains stored. Add a no-reference case asserting the test button is not left checked and status contains `no_reference_image`.

- [ ] **Step 2: Run the smoke to verify the new assertions fail**

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: the existing one-shot implementation fails the checked-state and repeated-ROI assertions.

### Task 2: Implement explicit continuous-test and ROI-editing state

**Files:**
- Modify: `src/RegisteredClassificationDialog.h`
- Modify: `src/RegisteredClassificationDialog.cpp`

**Interfaces:**
- Add `bool m_referenceTestMode = false;`.
- Add `bool m_roiEditing = false;`.
- Keep `runReferenceTest()`, `startRectangleRoiEditing()`, `finishRoiEditing()`, and `handleRoiChanged()` as the behavior boundaries.

- [ ] **Step 1: Implement test-mode toggle with precondition**

Change `runReferenceTest()` so it first checks the reference frame. If empty, set `m_referenceTestMode = false`, refresh UI state, and keep the existing `no_reference_image` status. If present, toggle `m_referenceTestMode`, refresh UI state, and execute the existing test request only when the new state is enabled. Keep the current ROI and result overlays unchanged when toggling off.

- [ ] **Step 2: Implement ROI state and automatic retest**

Set `m_roiEditing = true` when rectangle ROI editing starts and call the existing helper methods. Set it to false in `finishRoiEditing()` and disable helper drawing. In `handleRoiChanged()`, reject invalid rectangles as before; for valid rectangles update the config state and call `runReferenceTest()` only when `m_referenceTestMode` is true. Because `runReferenceTest()` toggles mode, introduce a private execution helper or a boolean-preserving path so automatic retest executes without turning the mode off.

- [ ] **Step 3: Preserve state-driven highlights and full-screen behavior**

Update `refreshUiState()` to set the test button checked from `m_referenceTestMode`, the rectangle control checked from `m_roiEditing`, and the global control checked only when full-screen mode is active and ROI editing is false. When full-screen mode is selected, stop ROI editing, keep the current test mode, and trigger one reference test if continuous mode is enabled.

- [ ] **Step 4: Keep status text actionable**

After entering continuous mode, show a status that includes the current ROI and indicates continuous reference testing is enabled. After automatic retest, preserve the result status and do not clear the test or ROI checked states. After clicking `完成`, show the existing ROI status plus the fact that continuous testing remains enabled when applicable.

- [ ] **Step 5: Run the focused smoke and confirm it passes**

Run the Task 1 command. Expected: `registered_classification_dialog_smoke: all checks passed`.

- [ ] **Step 6: Commit the behavior**

```bash
git add src/RegisteredClassificationDialog.h src/RegisteredClassificationDialog.cpp smoke/registered_classification_dialog_smoke.cpp
git commit -m "feat: add continuous registered classification reference testing"
```

### Task 3: Update feature rules and run the full verification gate

**Files:**
- Modify: `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`
- Modify: `docs/FID/RegisteredClassification/注册分类提示词规范.md`

- [ ] **Step 1: Update the implementation record**

Add a dated `[已完成]` section describing the persistent test mode, ROI auto-retest, button highlights, no-reference handling, and the behavior of “完成”. Include the implementation commit and exact smoke/build verification commands.

- [ ] **Step 2: Update the prompt specification**

Add the strict interaction contract: “基准图测试” is a toggleable continuous mode; rectangle ROI completion automatically retests; repeated ROI edits do not require another test click; “完成” exits only ROI editing; “测试运行” remains camera-frame one-shot behavior.

- [ ] **Step 3: Run all required verification commands**

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
cd ..
mkdir -p build/task-registered-classification-continuous-test
cd build/task-registered-classification-continuous-test
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
make -j$(nproc)
cd ../..
git diff --check
```

Expected: smoke prints `all checks passed`, the Qt build exits with status 0, and `git diff --check` has no output.

- [ ] **Step 4: Commit the documentation**

```bash
git add docs/FID/RegisteredClassification/registered_classification_function_implementation.md docs/FID/RegisteredClassification/注册分类提示词规范.md
git commit -m "docs: specify continuous reference testing behavior"
```
