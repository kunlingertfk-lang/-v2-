# Task 3 Report: HALCON Dual KNN Replacement

## RED

The renamed KNN backend smoke was written before the KNN runtime existed:

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_knn_backend_smoke.pro -o build/registered_classification_knn_backend.Makefile
make -C build -f registered_classification_knn_backend.Makefile -j8
```

It failed as expected because `RegisteredClassificationKnnRuntime.h/.cpp` did not exist and the new smoke referenced the missing KNN runtime contract.

## GREEN

- Added dynamically loaded HALCON KNN runtime with `T_create_class_knn`, `T_add_sample_class_knn`, `T_train_class_knn`, `T_set_params_class_knn`, `T_write_class_knn`, `T_read_class_knn`, `T_classify_class_knn`, and `T_clear_class_knn`.
- Verified local `HProto.h` signatures and exports from HALCON 24.11.1.0. The runtime also uses `T_get_sample_num_class_knn` to reapply the persisted `classes_distance` settings after read; HALCON read restores `max_num_classes` to one otherwise.
- Two KNN handles are marked only after a successful read/create and each guard clears exactly once.
- Training now extracts only V2 features, creates a sample KNN and a class-center KNN, persists V2 class-radius statistics, writes the complete schema-2 package in a temporary directory, validates it, then atomically swaps it into place.
- Inference fuses `0.70 * sampleSimilarity + 0.30 * centerSimilarity`, applies deterministic class-id tie breaks, and rejects in the required order: low similarity, ambiguity, then radius. Rejected results remain successful executions with `UNKNOWN`, candidate TopK, and diagnostics retained.
- The legacy V1 feature extractor is no longer compiled or publicly callable. MLP HALCON operators and `model.gmc` use were removed from the three core implementation files.

## Verification

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile
make -C build -f registered_classification_feature_v2.Makefile -j8
./build/smoke/registered_classification_feature_v2/bin/registered_classification_feature_v2_smoke
```

Output: `registered_classification_feature_v2_smoke: feature contract and schema 2 package checks passed`

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_knn_backend_smoke.pro -o build/registered_classification_knn_backend.Makefile
make -C build -f registered_classification_knn_backend.Makefile -j8
./build/smoke/registered_classification_knn_backend/bin/registered_classification_knn_backend_smoke
./build/smoke/registered_classification_knn_backend/bin/registered_classification_knn_backend_smoke
```

Both runs output: `registered_classification_knn_backend_smoke: dual KNN training and inference checks passed`

```bash
mkdir -p build/task-3-main && cd build/task-3-main
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
make -j8
```

The shadow build linked `build/qt_ui_test/bin/qt_ui_test`.

## Smoke Coverage

- `model.gnc`, `class_centers.gnc`, `class_stats.json`, and absence of `model.gmc`.
- One sample per class with radius disabled; three samples with radius enabled.
- Same image/class with multiple ROI shapes counted independently.
- Transformed same-class query, full payload, low-similarity, ambiguous, out-of-radius, and `UNKNOWN` semantics.
- Missing center model, mismatched KNN category sets, repeated classification, and missing KNN symbols.

## Cleanup Audit

The following search has no matches and exits one:

```bash
rg -n "T_(create|add_sample|train|write|read|classify|clear)_class_mlp|classify_class_mlp|model\\.gmc" \
  src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp \
  src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp \
  src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp
```

## Deviations And Concerns

- Existing UI/adapter source was intentionally not changed. Source-compatible legacy request/config fields remain in the Task 3 headers but are ignored by the KNN training path; they exist only so the required full Qt build can complete while the UI migration is handled separately.
- The schema-2 package reader intentionally reports a missing center model as `model_package_incomplete`, which is the existing actionable package-layer status.

## Local Review Corrections

- Removed the disabled `#if 0` schema-1 28-dimensional extractor body and its unused HALCON
  operators/helpers; the old implementation is no longer present in core source.
- Removed the temporary public `extractV2()` alias. `RegisteredClassificationFeatureExtractor`
  now exposes only `extract(image, RegisteredClassificationFeatureRegion, config)`, which is the
  V2 pipeline used by training, inference, and feature smoke tests.

## Review Fixes

- Rejected results now expose `predictedLabel="UNKNOWN"` and `predictedClassId=-1`; the best known
  candidate remains available through `topClasses` and explicit `bestCandidateClassId/Label`
  payload fields.
- Training verifies the complete byte count, flush result, JSON parse, and exact object round-trip
  of `training_report.json` before validating and promoting the temporary package.
- Explicit legacy model types and metadata-less `model.gmc` directories now return
  `legacy_model_requires_retraining` from inspection and runtime.
- KNN output class IDs are range-checked before narrowing from `Hlong`, and returned distances must
  be finite and non-negative.
