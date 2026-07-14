# Registered Classification Training Session Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist registered classification training images, names, classes, and ROI marks inside each model package and restore them when the user starts retraining.

**Architecture:** Add a focused session codec for versioned `training_session/session.json` plus PNG assets. Extend the existing HALCON training request so the runner writes those assets into its temporary output package before atomically replacing the target model directory. The training dialog remains the owner of UI state and uses the codec to restore state; old packages without a session remain usable but open with an explicit empty-session warning.

**Tech Stack:** Qt 5 JSON/file/image APIs, existing `cv::Mat` bridge for training samples, existing HALCON MLP runner and temporary-directory replacement flow, Qt smoke executable.

## Global Constraints

- Core classification and training remain HALCON MLP operations; OpenCV remains only an image/container bridge.
- New model packages are written under `ModelFiles/RegisteredClass/yyyyMMdd/model_HHmmss_zzz/` for new training.
- Retraining writes to the selected existing model directory and must preserve it when any step fails.
- Session image paths are relative to `training_session/` and must not escape that directory after canonicalization.
- Old packages without `training_session/session.json` remain valid for inference and show an explicit no-history-data message when retraining.
- Do not modify unrelated tools or the user’s existing dirty files.

## File Map

- Create `src/algorithms/recognition/RegisteredClassificationTrainingSession.h/.cpp`: versioned session manifest structures, JSON encoding/decoding, PNG asset validation and load/save helpers.
- Modify `src/algorithms/recognition/RegisteredClassificationTrainingRunner.h/.cpp`: carry optional session assets in the training request and write them into the runner’s temporary package before atomic replacement.
- Modify `src/RegisteredClassificationTrainingDialog.h/.cpp`: convert the in-memory UI session to runner session assets, load a selected package session, refresh all views/readiness, and expose test state.
- Modify `src/RegisteredClassificationModelManagementDialog.cpp`: pass the selected model directory into the training dialog and surface missing/corrupt session status without hiding usable models.
- Modify `smoke/registered_classification_dialog_smoke.cpp`: cover serialization, restoration, missing history, invalid paths, and same-directory retraining.
- Modify `qt_ui_test.pro` and the smoke `.pro` only if qmake requires explicit source registration.
- Update `docs/FID/RegisteredClassification/registered_classification_function_implementation.md` and `docs/FID/RegisteredClassification/注册分类提示词规范.md` with the completed package contract and status.

### Task 1: Add the versioned training-session codec

**Files:**
- Create: `src/algorithms/recognition/RegisteredClassificationTrainingSession.h`
- Create: `src/algorithms/recognition/RegisteredClassificationTrainingSession.cpp`
- Test: `smoke/registered_classification_dialog_smoke.cpp`

**Interfaces:**
- `RegisteredClassificationTrainingSessionAsset { QString relativePath; cv::Mat image; }`
- `RegisteredClassificationTrainingSessionPayload { int schemaVersion; QStringList classes; QJsonObject manifest; QVector<RegisteredClassificationTrainingSessionAsset> assets; }`
- `RegisteredClassificationTrainingSessionResult { bool success; QString status; QString message; QJsonObject payload; }`
- `writeRegisteredClassificationTrainingSession(const QString &sessionRoot, const QJsonObject &manifest, const QVector<...> &assets)`
- `readRegisteredClassificationTrainingSession(const QString &sessionRoot, RegisteredClassificationTrainingSessionPayload *payload)`

- [ ] **Step 1: Write the failing smoke assertions**

Add a small manifest with two classes, two named images, one rectangle ROI and one polygon ROI. Assert that a round trip returns the same class names, image names, ROI JSON, and decoded image dimensions. Add an invalid `../outside.png` asset assertion expecting `invalid_session_path`.

- [ ] **Step 2: Run the focused smoke build and verify the expected failure**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: compilation fails because the session codec interfaces do not yet exist, or the new assertions fail with the missing codec behavior.

- [ ] **Step 3: Implement the codec minimally**

Use `QSaveFile` for `session.json`, `QDir::mkpath` for `images`, `QImage::fromData`/`QImage::save` for PNG assets, and `QFileInfo::canonicalFilePath` plus a canonical session-root prefix check on read. Reject unsupported schema versions, malformed JSON, missing image files, duplicate relative paths, absolute paths, and paths that escape `training_session`.

- [ ] **Step 4: Run the focused smoke and confirm it passes**

Run the same qmake/make/smoke command. Expected output includes `all checks passed`.

- [ ] **Step 5: Commit the codec**

```bash
git add src/algorithms/recognition/RegisteredClassificationTrainingSession.* smoke/registered_classification_dialog_smoke.cpp
git commit -m "feat: persist registered classification training sessions"
```

### Task 2: Write session assets atomically with HALCON model output

**Files:**
- Modify: `src/algorithms/recognition/RegisteredClassificationTrainingRunner.h`
- Modify: `src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp`
- Modify: `src/algorithms/recognition/RegisteredClassificationModelPackage.h/.cpp` only if package path helpers are needed
- Test: `smoke/registered_classification_dialog_smoke.cpp`

**Interfaces:**
- Extend `RegisteredClassificationTrainingRequest` with an optional `QJsonObject trainingSessionManifest` and `QVector<RegisteredClassificationTrainingSessionAsset> trainingSessionAssets`.
- Successful `train()` writes `training_session/session.json` and PNG assets inside the same temporary model package as `model.gmc`, `metadata.json`, and `training_report.json`.

- [ ] **Step 1: Add a failing package assertion**

Train a valid two-class request carrying a manifest and two image assets. Assert that the output package contains `training_session/session.json` and both PNG files, and that a forced invalid asset path returns failure while the pre-existing target package’s model and session files remain readable.

- [ ] **Step 2: Run the smoke to verify the assertion fails**

Run the focused smoke command from Task 1. Expected: the existing model files are produced but the new `training_session` files are absent, or the invalid-path case is not rejected.

- [ ] **Step 3: Integrate session writing into the runner temporary directory**

After HALCON training and report generation succeed, invoke the codec against the runner’s temporary output path before `swapModelDirectory()`. If session writing fails, remove the temporary directory, return a session-specific failure status, and leave the original target directory untouched. Keep the existing `.bak` avoidance and replacement order unchanged.

- [ ] **Step 4: Run the smoke and verify atomic success/failure behavior**

Run the focused smoke command. Expected: package completeness assertions pass and the invalid session case leaves the old package unchanged.

- [ ] **Step 5: Commit runner integration**

```bash
git add src/algorithms/recognition/RegisteredClassificationTrainingRunner.* src/algorithms/recognition/RegisteredClassificationTrainingSession.* smoke/registered_classification_dialog_smoke.cpp
git commit -m "feat: include training sessions in model packages"
```

### Task 3: Restore the session in the training dialog

**Files:**
- Modify: `src/RegisteredClassificationTrainingDialog.h`
- Modify: `src/RegisteredClassificationTrainingDialog.cpp`
- Modify: `src/RegisteredClassificationModelManagementDialog.cpp`
- Test: `smoke/registered_classification_dialog_smoke.cpp`

**Interfaces:**
- Add `bool restoreTrainingSessionFromModelDir(const QString &modelDir)` to the training dialog’s internal flow.
- Add test-visible JSON fields to `buildTrainingRequestPreviewForTest()`: `restoredSessionStatus`, `restoredImageNames`, `restoredClassNames`, and `restoredRoiCount`.
- `setUpdateTargetModelDir()` invokes restore once, preserves the target directory even when restoration fails, and updates the status label with the actionable reason.

- [ ] **Step 1: Write failing restoration assertions**

Create a valid package with a saved session, open a retraining dialog, call `setUpdateTargetModelDir(modelDir)`, and assert the preview reports the original image count, names, class names, and ROI count. Add assertions for an old package without session data: target directory remains set, preview reports `missing_training_session`, and the dialog exposes a message containing “没有历史训练数据”.

- [ ] **Step 2: Run smoke and verify restoration fails**

Run the focused smoke command. Expected: `setUpdateTargetModelDir()` only sets the output target; restored counts remain zero and the old-package status field is absent.

- [ ] **Step 3: Implement restore and request conversion**

Load the codec payload, recreate `TrainingImageState` objects from PNG assets, rebuild `marksByClass`, replace default classes only after a valid manifest is loaded, reset transient selection fields, call the existing thumbnail/class/preview refresh lambdas, and then call `refreshTrainingReadiness()`. On missing or invalid sessions, keep a blank state and set a precise status without silently treating the model as restorable. When building the next training request, include the current state manifest and PNG assets so edits are persisted.

- [ ] **Step 4: Wire model management retraining and verify UI state**

Keep `setUpdateTargetModelDir(record.modelDir)` as the single entry point. Add a model-row tooltip/status distinction for restorable versus missing-session packages, while retaining both packages in the list. Do not block using a model because its training session is missing.

- [ ] **Step 5: Run smoke and confirm restored retraining state**

Run the focused smoke command. Expected: retraining opens with the original classes, names, ROI count, and same output directory; old packages display the warning and remain usable.

- [ ] **Step 6: Commit dialog restoration**

```bash
git add src/RegisteredClassificationTrainingDialog.* src/RegisteredClassificationModelManagementDialog.cpp smoke/registered_classification_dialog_smoke.cpp
git commit -m "feat: restore registered classification retraining sessions"
```

### Task 4: Update feature documents and run the full verification gate

**Files:**
- Modify: `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`
- Modify: `docs/FID/RegisteredClassification/注册分类提示词规范.md`
- Test: `smoke/registered_classification_dialog_smoke.cpp`

- [ ] **Step 1: Add failing documentation/status checks if the repository’s doc smoke supports them**

If no documentation checker exists, use a manual checklist against the design spec: package tree, manifest fields, restoration behavior, old-package behavior, atomic failure behavior, and status labels must all be present in both relevant feature documents.

- [ ] **Step 2: Update the documents**

Record the new package contract, restore flow, compatibility behavior, validation rules, and the implementation commit/verification result. Mark only behavior verified by tests as `[已完成]`; leave any unverified manual behavior as `[后续优化]` or `[待实现]`.

- [ ] **Step 3: Run the complete verification suite**

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
cd ..
mkdir -p build/task-registered-classification-session
cd build/task-registered-classification-session
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
make -j$(nproc)
cd ../..
git diff --check
```

Expected: the smoke prints `all checks passed`, the Qt build exits with status 0, and `git diff --check` prints no diagnostics.

- [ ] **Step 4: Review the final diff and commit documentation**

```bash
git diff --stat
git status --short
git add docs/FID/RegisteredClassification/registered_classification_function_implementation.md docs/FID/RegisteredClassification/注册分类提示词规范.md
git commit -m "docs: document registered classification session recovery"
```

Preserve the unrelated dirty files `projects/scheme_0d5611a7/reference.png`, `projects/scheme_0d5611a7/scheme.json`, and `docs/FID/RegisteredClassification/tempFunc.md`.
