# Task 2 Report: HALCON Registered Classification Feature V2

## Status

Implemented and verified on base commit `6a69e7e97863a950a7cb49c5b09dcc7f21b6df35`.

## Files

- `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h`
- `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp`
- `smoke/registered_classification_feature_v2_smoke.cpp`
- `smoke/registered_classification_feature_v2_smoke.pro`
- `.superpowers/sdd/task-2-report.md`

No KNN runtime, training, inference, adapter, or UI implementation was added. The legacy
`extract(image, QRectF, config)` entry point remains for staged compilation.

## RED Evidence

The V2 smoke was extended before production code with rotation (0/45/90/180 degrees), scale
(0.7/1.0/1.3), dual polarity, shape-versus-rotation, same-color/different-shape,
same-shape/different-color, true polygon ROI, uniform-image rejection, and missing-symbol cases.

Command:

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile
make -C build -f registered_classification_feature_v2.Makefile -j8
```

Observed failure included:

```text
error: 'RegisteredClassificationFeatureRegion' was not declared in this scope
error: 'class RegisteredClassificationFeatureExtractor' has no member named 'extractV2'
error: 'RegisteredClassificationFeatureResult' has no member named 'foregroundPolarity'
```

## Implementation

- Added the public `RegisteredClassificationFeatureRegion`, V2 result diagnostics, and `extractV2`.
- Resolved all required operators through the existing `dlopen`/`dlsym` style and wrapped HALCON
  objects, tuples, and the library handle in scope-bound cleanup.
- Built rectangle/full/polygon ROI regions in HALCON and used the Task 1 segmentation contract for
  Gaussian smoothing, max-separability light/dark thresholding, morphology, 2%-98% gating, border
  measurement, and exact candidate scoring.
- Canonicalized with HALCON rigid transforms, 8% padded crop, 128x128 image/region zoom, long-axis
  alignment, and quantized occupancy-vector orientation disambiguation.
- Produced the exact 13/16/18/6/6 shape, occupancy, gray, Lab, and texture sequence, then applied the
  public group weights and complete-vector L2 normalization.
- Returned `foreground_not_found`, `invalid_feature_value`, and symbol-specific
  `halcon_symbol_missing` errors without fallback algorithms.

## HALCON Verification

The task text referenced `/opt/halcon-24.11.1`, which is absent in this environment. The configured
`HALCONROOT` is:

```text
/home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady
```

Exact function signatures were read from its `include/HProto.h`, and all required symbols were
confirmed in `lib/x64-linux/libhalconc.so.24.11.1` with `nm -D`. No required symbol was absent.

## GREEN Evidence

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_feature_v2_smoke.pro -o build/registered_classification_feature_v2.Makefile
make -C build -f registered_classification_feature_v2.Makefile -j8
./build/smoke/registered_classification_feature_v2/bin/registered_classification_feature_v2_smoke
```

Result: exit 0, `registered_classification_feature_v2_smoke: feature contract and schema 2 package checks passed`.

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/registered_classification_mlp_backend_smoke.pro -o build/registered_classification_mlp_backend.Makefile
make -C build -f registered_classification_mlp_backend.Makefile -j8
./build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
```

Result: exit 0, `registered_classification_mlp_backend_smoke: metadata, feature, training, and inference checks passed`.

```bash
mkdir -p build/task-2-main
cd build/task-2-main
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro
make -j8
```

Result: exit 0; `build/qt_ui_test/bin/qt_ui_test` linked successfully.

## Deviations And Concerns

- No design deviation in the feature pipeline. The HALCON installation path differs from the path
  named in the task, but the available runtime is the required 24.11.1 release.
- `valgrind` is not installed, so leak verification is by RAII review and repeated smoke execution,
  not an external heap checker.

## Local Review Correction

The first implementation built the ROI boundary band from `opening_circle(ROI)`. Local review
replaced that operation with HALCON `T_erosion_circle`, then computes
`difference(ROI, erodedROI)`, so the configured band width represents a real inward border for
rectangles and polygons. The V2 smoke, legacy MLP smoke, and main Qt build passed again after this
correction.

## Review Fixes

- Polygon candidate scoring now uses the centroid returned by HALCON `area_center(roi)` instead of
  the polygon bounding-box center. The smoke includes an asymmetric trapezoid with competing
  candidates and verifies selection toward the true ROI centroid.
- Every HALCON numeric output consumed by the V2 pipeline is checked for tuple presence and
  finiteness before clamping, mapping, weighting, or normalization. Missing or non-finite outputs
  now return `invalid_feature_value` instead of becoming a valid zero or bounded value.

After these fixes, the feature V2 smoke rebuilt without warnings and passed.
