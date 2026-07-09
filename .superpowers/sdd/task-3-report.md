# Task 3 Report

- Status: DONE
- Commits made: `cbe08c1fc7eb216a84eaf4f5276f53f842b0b536` (`feat: train registered classification mlp models`)

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
  - `registered_classification_mlp_backend_smoke: metadata and feature checks passed`
- Root Qt build succeeded and linked `build/qt_ui_test/bin/qt_ui_test`.

## Self-review notes

- The new runner stays inside the task write scope and reuses `RegisteredClassificationFeatureExtractor` instead of duplicating HALCON feature extraction.
- HALCON is used for the MLP training path only; OpenCV remains the image container/bridge.
- The runner validates output dir, class labels, and per-class valid sample coverage before training.
- Model package writes go through `<outputModelDir>.tmp` and only swap into place after `model.gmc`, `metadata.json`, and `training_report.json` all exist.
- The smoke’s `tempModelDir()` helper clears the shared temp root on each call, so the later one-class negative check removes the earlier trained output after it has already been asserted. The checks still pass, but the trained artifacts do not remain on disk after the smoke exits.

## Concerns

- Minor: the current smoke success line still says `metadata and feature checks passed` even though it now also covers training.
