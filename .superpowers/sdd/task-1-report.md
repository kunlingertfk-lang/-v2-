# Task 1 Report: V2 Feature Space and Schema 2 Package Contract

## Implementation Summary

- Added `RegisteredClassificationFeatureSpace` with the fixed 59-name V2 contract, group dimensions `13/16/18/6/6`, group weights `0.35/0.25/0.15/0.15/0.10`, finite-value validation, complete-vector L2 normalization, distance/similarity conversion, normalized class centers, and bounded population-standard-deviation radius statistics.
- Added schema-2 KNN metadata, class-statistics, strict V2 readers/writers, package validation, path helpers, and non-strict legacy inspection to `RegisteredClassificationModelPackage`.
- Kept all schema-1 MLP structs and APIs intact. `registeredClassificationMlpModelType()` continues to return the legacy MLP type; strict V2 reads of schema 1 return `legacy_model_requires_retraining`.
- Added a Qt-Core-only V2 contract smoke. It covers feature names, group contract, vector behavior, radius disablement, schema and feature-length rejection, class-statistics round-trip, structural package completeness, strict legacy rejection, and legacy inspection.
- Registered the new FeatureSpace implementation with the application and existing registered-classification smoke qmake targets so current MLP callers remain linkable.

## RED

Command:

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile
make -C build -f registered_classification_feature_v2.Makefile -j8
```

Output excerpt:

```text
error: 'RegisteredClassificationKnnModelMetadata' does not name a type
error: 'registeredClassificationKnnModelType' was not declared in this scope
error: 'registeredClassificationFeatureVersionV2' was not declared in this scope
make: *** [registered_classification_feature_v2.Makefile:871: ...] Error 1
```

The failure was expected: the smoke was written before production code and referenced the missing V2 model-package and feature-space APIs.

## GREEN

Command:

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile
make -C build -f registered_classification_feature_v2.Makefile -j8
./build/smoke/registered_classification_feature_v2/bin/registered_classification_feature_v2_smoke
```

Output:

```text
registered_classification_feature_v2_smoke: feature contract and schema 2 package checks passed
```

Additional verification:

```bash
mkdir -p build/task-1 && cd build/task-1
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
make -j8
```

The project shadow build linked `build/qt_ui_test/bin/qt_ui_test`. A subsequent `make -C build/task-1 -j8` exited 0 with nothing left to build.

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_mlp_backend_smoke.pro -o build/registered_classification_mlp_backend.Makefile
make -C build -f registered_classification_mlp_backend.Makefile -j8
./build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
```

Output:

```text
registered_classification_mlp_backend_smoke: metadata, feature, training, and inference checks passed
```

`git diff --check` also exited 0.

## Files Changed

- `src/algorithms/recognition/RegisteredClassificationFeatureSpace.h`
- `src/algorithms/recognition/RegisteredClassificationFeatureSpace.cpp`
- `src/algorithms/recognition/RegisteredClassificationModelPackage.h`
- `src/algorithms/recognition/RegisteredClassificationModelPackage.cpp`
- `smoke/registered_classification_feature_v2_smoke.cpp`
- `smoke/registered_classification_feature_v2_smoke.pro`
- `qt_ui_test.pro`
- `smoke/registered_classification_mlp_backend_smoke.pro`
- `smoke/registered_classification_adapter_smoke.pro`
- `smoke/registered_classification_dialog_smoke.pro`
- `.superpowers/sdd/task-1-report.md`

## Self-Review

- Confirmed the V2 feature names and their ordering total exactly 59 values and preserve the specified five groups.
- Confirmed normalization rejects null, wrong-length, non-finite, and zero vectors; distance rejects mismatched or invalid V2 vectors with `NaN`; similarity applies `clamp(1-d*d/2, 0, 1)`.
- Confirmed radius use is gated at three samples and uses population standard deviation with the required `max(maxD*1.10, mean+2.5*stdDev)` and `[0.10, 2.00]` bounds.
- Confirmed strict KNN readers reject legacy schema-1 metadata while non-strict inspection retains old model type, feature version, class count, sample count, and training-session presence.
- Confirmed package validation cross-checks class IDs, number of classes, feature version, and total class-stat sample count against metadata, in addition to requiring both `.gnc` paths.
- Kept the prerequisite adapter-smoke source fix and all unrelated files untouched.

## Concerns

None within Task 1. The `.gnc` checks are intentionally structural in this Qt-Core-only contract layer; HALCON binary read/write validation belongs to the later KNN runtime task.

## Review Fixes

### RED

Command:

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile && make -C build -f registered_classification_feature_v2.Makefile -j8 && ./build/smoke/registered_classification_feature_v2/bin/registered_classification_feature_v2_smoke
```

Result: exit 1. The amended smoke failed because `metadata.json` omitted `knn.method` and `knn.normalization`; it also accepted zero-byte `model.gnc`/`class_centers.gnc` and marked the inspected package runnable. Altering either fixed KNN value was accepted as well.

### GREEN

The KNN contract now stores `method="classes_distance"` and `normalization=false` in `RegisteredClassificationKnnParams` and metadata JSON. Missing or altered values fail with `invalid_knn_parameters` and an actionable fixed-contract message. Package validation requires both `.gnc` paths to be regular, non-empty files; this remains structural only and does not HALCON-parse the binaries.

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile && make -C build -f registered_classification_feature_v2.Makefile -j8 && ./build/smoke/registered_classification_feature_v2/bin/registered_classification_feature_v2_smoke
```

Output: `registered_classification_feature_v2_smoke: feature contract and schema 2 package checks passed` (exit 0).

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_mlp_backend_smoke.pro -o build/registered_classification_mlp_backend.Makefile && make -C build -f registered_classification_mlp_backend.Makefile -j8 && ./build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
```

Output: `registered_classification_mlp_backend_smoke: metadata, feature, training, and inference checks passed` (exit 0).
