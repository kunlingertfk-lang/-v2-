# Task 3 Report

- Status: DONE
- Commits made:
  - `test: preserve registered classification trained smoke package`

## Files changed

- `src/algorithms/recognition/RegisteredClassificationTrainingRunner.h`
- `src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp`
- `smoke/registered_classification_mlp_backend_smoke.cpp`
- `smoke/registered_classification_mlp_backend_smoke.pro`
- `qt_ui_test.pro`
- `.superpowers/sdd/task-3-report.md`

## Exact commands run

1. `sed -n '1,260p' .superpowers/sdd/task-3-brief.md`
2. `rg -n "RegisteredClassification" src smoke qt_ui_test.pro`
3. `sed -n '1,320p' src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp`
4. `sed -n '1,360p' src/algorithms/recognition/RegisteredClassificationModelPackage.cpp`
5. `sed -n '1,320p' src/algorithms/halcon/HalconRuntimePaths.cpp`
6. `sed -n '1,260p' src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp`
7. `git status --short`
8. `git log --oneline -5 -- .superpowers/sdd/task-3-brief.md src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp smoke/registered_classification_mlp_backend_smoke.cpp`
9. `/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro && make -j$(nproc)` in `smoke/` before adding the runner files
10. `ls /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/lib/x64-linux/libhalconc.so*`
11. `nm -D /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/lib/x64-linux/libhalconc.so | rg "(create_class_mlp|add_sample_class_mlp|train_class_mlp|write_class_mlp|clear_class_mlp|F_create_tuple|F_set_d|F_set_i|F_set_s|F_destroy_tuple|F_get_d)"`
12. `rg -n "create_class_mlp|add_sample_class_mlp|train_class_mlp|write_class_mlp|clear_class_mlp" /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/include/HalconC.h /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/include`
13. `/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro && make -j$(nproc)` in `smoke/` after implementation
14. `../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke`
15. `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro && make -j$(nproc)` in repo root
16. `git diff -- src/algorithms/recognition/RegisteredClassificationTrainingRunner.h src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp smoke/registered_classification_mlp_backend_smoke.cpp smoke/registered_classification_mlp_backend_smoke.pro qt_ui_test.pro`
17. `git rev-parse --abbrev-ref HEAD && git rev-parse --short HEAD`
18. `git add qt_ui_test.pro smoke/registered_classification_mlp_backend_smoke.cpp smoke/registered_classification_mlp_backend_smoke.pro src/algorithms/recognition/RegisteredClassificationTrainingRunner.h src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp`
19. `git add -f .superpowers/sdd/task-3-report.md`
20. `git commit -m "feat: train registered classification mlp models"`
21. `git add -f .superpowers/sdd/task-3-report.md && git commit --amend --no-edit`

## Key outputs

- Red step smoke build failed as expected before implementation:
  - `WARNING: Failure to find: ../src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp`
  - `fatal error: algorithms/recognition/RegisteredClassificationTrainingRunner.h: 没有那个文件或目录`
- HALCON runtime inspection confirmed required symbols are present:
  - `T_add_sample_class_mlp`
  - `T_clear_class_mlp`
  - `T_create_class_mlp`
  - `T_train_class_mlp`
  - `T_write_class_mlp`
  - `F_create_tuple`
  - `F_destroy_tuple`
  - `F_get_d`
  - `F_set_d`
  - `F_set_i`
  - `F_set_s`
- Post-implementation smoke build succeeded.
- Smoke executable output:
  - `registered_classification_mlp_backend_smoke: metadata, feature, and training checks passed`
- Root Qt build succeeded and linked `build/qt_ui_test/bin/qt_ui_test`.
- Final task commit is recorded in git history.

## Self-review notes

- The new runner stays inside the task write scope and reuses `RegisteredClassificationFeatureExtractor` instead of duplicating HALCON feature extraction.
- HALCON is used for the MLP training path only; OpenCV remains the image container/bridge.
- The runner validates output dir, class labels, and per-class valid sample coverage before training.
- Model package writes go through `<outputModelDir>.tmp` and only swap into place after `model.gmc`, `metadata.json`, and `training_report.json` all exist.
- The smoke now writes the positive training output under `/tmp/registered_classification_mlp_backend_smoke_model/trained` and keeps the one-class negative case in a separate sibling directory, so the trained artifacts remain present after the smoke exits.

## Concerns

- None.

## Follow-up Fix

The review finding about post-smoke artifact preservation is fixed in `smoke/registered_classification_mlp_backend_smoke.cpp`:

- Positive training output stays in `/tmp/registered_classification_mlp_backend_smoke_model/trained`.
- The one-class negative case uses its own temp root and no longer clears the trained output.
- The smoke now re-checks `model.gmc`, `metadata.json`, and `training_report.json` after the negative case completes.
- The success banner now reads `registered_classification_mlp_backend_smoke: metadata, feature, and training checks passed`.

### Commands run

1. `cd smoke && /home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro && make -j$(nproc) && ../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke`
2. `test -f /tmp/registered_classification_mlp_backend_smoke_model/trained/model.gmc && test -f /tmp/registered_classification_mlp_backend_smoke_model/trained/metadata.json && test -f /tmp/registered_classification_mlp_backend_smoke_model/trained/training_report.json`
3. `mkdir -p build && cd build && /home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro && make -j$(nproc)`

### Outputs

- Smoke run output:
  - `registered_classification_mlp_backend_smoke: metadata, feature, and training checks passed`
- Artifact existence check succeeded with exit status `0`.
- Qt build succeeded.

## Task 3 Review Fixes

- Fixed `training_report.json` field casing in `RegisteredClassificationTrainingRunner.cpp` from lowercase `error` / `errorLog` to HALCON-spec `Error` / `ErrorLog`.
- Hardened `HalconMlpHandleGuard` so cleanup only runs after `create_class_mlp` succeeds: `outPtr()` no longer marks the handle valid, and `markCreated()` is called only after a successful create.
- Extended `registered_classification_mlp_backend_smoke.cpp` to open `training_report.json` after training and assert the exact `Error` and `ErrorLog` keys exist.

### Review-fix commands run

1. `cd smoke && /home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro && make -j$(nproc) && ../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke`
2. `test -f /tmp/registered_classification_mlp_backend_smoke_model/trained/model.gmc && test -f /tmp/registered_classification_mlp_backend_smoke_model/trained/metadata.json && test -f /tmp/registered_classification_mlp_backend_smoke_model/trained/training_report.json`
3. `mkdir -p build && cd build && /home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro && make -j$(nproc)`

### Review-fix outputs

- Red step before the runner patch:
  - `FAIL: training_report.json must contain Error`
  - `FAIL: training_report.json must contain ErrorLog`
- Post-fix smoke run:
  - `registered_classification_mlp_backend_smoke: metadata, feature, and training checks passed`
- Artifact existence check succeeded with exit status `0`.
- Repo build completed and linked `build/qt_ui_test/bin/qt_ui_test`.
