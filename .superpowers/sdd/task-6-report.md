# Task 6 Report: 注册分类 KNN V2 文档更新

## Scope

- Base: `7728ac0`
- Modified only the three target FID documents and this report.
- No UI/source code was changed and no full build or UI verification was run; those remain with the parent task.

## Documentation Changes

- Replaced stale current-state MLP/DL descriptions with the HALCON Feature V2 59-dimensional contract.
- Documented feature groups `13/16/18/6/6`, weights `0.35/0.25/0.15/0.15/0.10`, real polygon ROI construction, and the online full/rectangle boundary.
- Documented the sample/center KNN pair, fixed parameters, `model.gnc`, `class_centers.gnc`, `class_stats.json`, class-radius formula, and schema-2 package layout.
- Documented fusion `0.70 * sampleSimilarity + 0.30 * centerSimilarity`, fixed thresholds `80/8`, and rejection states `classification_rejected_low_similarity`, `classification_rejected_ambiguous`, and `classification_rejected_out_of_radius`.
- Documented successful UNKNOWN rejection semantics: `success=true`, `ok=false`, `predictedClassId=-1`, candidate and diagnostic payload retained.
- Documented RAII cleanup, new dated model directories, same-directory atomic retraining, legacy listing/retraining/no direct run, and unapplied position correction.
- Kept MLP/DL references only in explicitly labeled history or prohibition clauses; no current path describes MLP/DL execution.

## Static Checks

Commands run after editing:

```bash
rg -n "T_(create|add_sample|train|write|read|classify|clear)_class_mlp|classify_class_mlp" \
  src/algorithms/recognition src/tooladapters/RegisteredClassificationAdapter.cpp \
  src/RegisteredClassificationDialog.cpp src/RegisteredClassificationTrainingDialog.cpp

rg -n "halcon_registered_feature_v2|59|model\\.gnc|class_centers\\.gnc|class_stats\\.json|classes_distance|0\\.70|0\\.30|80|8|UNKNOWN|classification_rejected_(low_similarity|ambiguous|out_of_radius)|positionCorrectionApplied=false" \
  src docs/FID/RegisteredClassification smoke

git diff --check -- \
  docs/FID/RegisteredClassification/registered_classification_function_implementation.md \
  docs/FID/RegisteredClassification/注册分类算法当前实现说明.md \
  docs/FID/RegisteredClassification/注册分类提示词规范.md \
  .superpowers/sdd/task-6-report.md

git status --short
```

Results:

- The MLP HALCON operator search produced no output in the current recognition/adapter/dialog implementation files.
- The KNN/Feature V2 consistency search covered source, smoke and all three FID documents, including the package files, formula, thresholds, UNKNOWN semantics, rejection states and position-correction payload.
- `git diff --check` produced no output.
- Status was limited to the four documentation files before commit; no source/UI/build artifact was included.

## Verification Boundary

The parent task owns the requested qmake/make, registered-classification smoke, main shadow build and manual UI verification. This task intentionally performed documentation/static checks only.

## Review Corrections

- Clarified that `80/8` are the metadata/default values. Runtime `minSimilarity` and `minMargin`
  are configurable in `[0,100]`; the three-state rejection order remains fixed.
- Clarified the package boundary: runtime validation requires metadata, both KNN files, and
  `class_stats.json`; `training_report.json` is a trainer artifact and `training_session/` is
  optional recovery data.
