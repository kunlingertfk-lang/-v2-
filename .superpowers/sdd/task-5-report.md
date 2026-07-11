# Task 5 Report: Migrate Managed Registered Classification Models to KNN

## Scope

Task 5 is complete on base `9df8772`. Model management now scans metadata-backed package directories through read-only package inspection. Complete schema-2 HALCON KNN packages are runnable; schema-1 MLP packages remain visible as legacy records that require retraining and cannot be selected for use.

The position-correction path was not changed.

## Implementation

- Replaced model-management metadata scanning with `RegisteredClassificationModelInspection` records.
- Kept all metadata-backed records visible, including legacy schema-1 MLP packages and incomplete V2 packages with explicit status.
- Disabled legacy use actions, added retraining guidance, and kept retraining enabled for every inspected record.
- Refreshed the same model directory after retraining and emitted `modelSelected` only after inspection confirmed a runnable V2 package.
- Strict main-dialog import continues to accept only a complete schema-2 package and now gives an explicit legacy retraining message.
- Removed `RegisteredClassificationMlpParams`, V1 feature-name APIs, and V1 metadata validation/read/write APIs. Legacy inspection retains only the legacy model-type string, `model.gmc` path inspection, and basic schema-1 JSON fields.
- Added dialog fixtures for complete V2, schema-1 MLP with a training session, and schema-1 MLP without a session.

## TDD Evidence

The dialog smoke was run before the production migration and failed because legacy rows were filtered and no legacy retrain action was available. After the implementation, the same smoke passed with coverage for:

- all three rows visible;
- only V2 use enabled;
- both legacy retrain actions enabled and labeled old/retrain;
- restored image names, classes, and ROI count;
- empty no-session training state with an actionable message;
- atomic same-directory upgrade producing `model.gnc` and `class_centers.gnc` while removing `model.gmc`;
- no legacy selection signal before successful upgrade;
- refreshed upgraded row and strict import rejection.

## Verification

- `QT_QPA_PLATFORM=offscreen ./build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke` passed.
- `./build/smoke/registered_classification_adapter/bin/registered_classification_adapter_smoke` passed.
- `./build/smoke/registered_classification_knn_backend/bin/registered_classification_knn_backend_smoke` passed.
- `./build/smoke/registered_classification_feature_v2/bin/registered_classification_feature_v2_smoke` passed.
- Shadow main build passed from `build/task-5-main` with Qt 5.15.2 qmake and `make -j8`.
- `git diff --check` passed.

## Concerns

- The offscreen Qt plugin still prints its expected `propagateSizeHints`, `raise`, and keyboard-grabbing notices; the dialog smoke exits 0.
- Legacy packages without a training session intentionally open as an empty training dialog and require the user to add valid samples before updating the directory.
