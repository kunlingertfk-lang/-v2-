# Task 4 Report

- Status: DONE
- Scope: Task 4 write scope only
- Commits made:
  - `feat: run registered classification mlp inference`

## Files changed

- `src/algorithms/recognition/RegisteredClassificationHalconRunner.h`
- `src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp`
- `smoke/registered_classification_mlp_backend_smoke.cpp`
- `smoke/registered_classification_mlp_backend_smoke.pro`
- `.superpowers/sdd/task-4-report.md`

## Commands run

1. Read task brief and required skills/docs:
   - `sed -n '1,260p' .superpowers/sdd/task-4-brief.md`
   - `sed -n '1,220p' /home/tt/.codex/plugins/cache/openai-curated-remote/superpowers/6.1.1/skills/using-superpowers/SKILL.md`
   - `sed -n '1,260p' /home/tt/.codex/plugins/cache/openai-curated-remote/superpowers/6.1.1/skills/brainstorming/SKILL.md`
   - `sed -n '1,260p' /home/tt/.codex/plugins/cache/openai-curated-remote/superpowers/6.1.1/skills/test-driven-development/SKILL.md`
   - `sed -n '1,260p' /home/tt/.codex/plugins/cache/openai-curated-remote/superpowers/6.1.1/skills/verification-before-completion/SKILL.md`

2. Inspect current implementation and dependencies:
   - `git status --short`
   - `sed -n '1,260p' src/algorithms/recognition/RegisteredClassificationHalconRunner.h`
   - `sed -n '1,320p' src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp`
   - `sed -n '1,260p' smoke/registered_classification_mlp_backend_smoke.cpp`
   - `sed -n '1,220p' smoke/registered_classification_mlp_backend_smoke.pro`
   - `rg -n "registeredClassificationMlpModelType|featureVersion|readRegisteredClassificationMetadata|classification_rejected|classification_ambiguous|read_class_mlp|classify_class_mlp" src smoke -S`
   - `sed -n '1,260p' src/algorithms/recognition/RegisteredClassificationModelPackage.h`
   - `sed -n '1,320p' src/algorithms/recognition/RegisteredClassificationModelPackage.cpp`
   - `sed -n '1,260p' src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h`
   - `sed -n '1,360p' src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp`
   - `sed -n '1,260p' src/toolcore/ToolOverlay.h`
   - `sed -n '1,260p' src/toolcore/PositionCorrection.h`
   - `sed -n '1,320p' src/toolcore/PositionCorrection.cpp`
   - `sed -n '1,260p' src/algorithms/halcon/HalconRuntimePaths.h`
   - `sed -n '1,320p' src/algorithms/halcon/HalconRuntimePaths.cpp`

3. Verify HALCON MLP inference symbols locally:
   - `nm -D /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/lib/x64-linux/libhalconc.so | rg "read_class_mlp|classify_class_mlp|clear_class_mlp|write_class_mlp"`
   - `rg -n "classify_class_mlp|read_class_mlp" /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/include/HProto.h -C 2`
   - `sed -n '640,690p' /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/doc/html/reference/operators/classify_class_mlp.html`
   - `sed -n '620,690p' /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/doc/html/reference/operators/add_sample_class_mlp.html`

4. TDD red run after adding smoke coverage:
   - `cd smoke`
   - `/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro && make -j$(nproc) && ../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke`

5. Verification after implementation:
   - `cd smoke`
   - `make -j$(nproc) && ../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke`
   - `mkdir -p /home/tt/tfk/WorkerSpace/project/V2_615_1628/qt_znxj_v2_work_1920_515/build/task4`
   - `/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro`
   - `make -j$(nproc)` (in `build/task4`)
   - `git diff --check`

6. Commit:
   - `git add .superpowers/sdd/task-4-report.md smoke/registered_classification_mlp_backend_smoke.cpp smoke/registered_classification_mlp_backend_smoke.pro src/algorithms/recognition/RegisteredClassificationHalconRunner.h src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp`
   - `git commit -m "feat: run registered classification mlp inference"`

## Key outputs

- Initial smoke extension exposed an additional smoke target dependency gap:
  - linker error: `undefined reference to PositionCorrection::writeNotAppliedPayload(...)`
- Local HALCON symbol inspection confirmed MLP inference support is available:
  - `T_read_class_mlp`
  - `T_classify_class_mlp`
  - `T_clear_class_mlp`
- First behavioral smoke after implementation failed with:
  - `FAIL: inference status must be ok`
- Diagnostic run showed returned label mapping was flipped:
  - `infer status=ng label=Dark score=99.9999 message=Classification result does not satisfy the judge rule.`
- Final smoke passed:
  - `registered_classification_mlp_backend_smoke: metadata, feature, training, and inference checks passed`
- Root project shadow build passed:
  - `qmake ../../qt_ui_test.pro` exit code `0`
  - `make -j$(nproc)` exit code `0`
- Whitespace check passed:
  - `git diff --check` exit code `0`

## Self-review notes

- Replaced the DL-only registered classification runner with an MLP package flow:
  - metadata read and validation
  - `model.gmc` presence check
  - HALCON feature extraction reuse
  - HALCON `classify_class_mlp` inference
  - old `halcon_dl_classification` rejection at runner level
- Kept changes inside Task 4 scope and did not revert unrelated work.
- Added the smoke target source linkage needed for `PositionCorrection`, since the smoke now links the real runner.
- One subtle fix was required after the first green attempt: HALCON’s returned class number needed one-based mapping preference to align with trained class order.

## Concerns

- None at handoff time.

---

## Review fix follow-up (metadata thresholds + smoke coverage)

- Scope for this follow-up:
  - `src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp`
  - `smoke/registered_classification_mlp_backend_smoke.cpp`
  - `.superpowers/sdd/task-4-report.md`
- Existing unrelated worktree state observed and preserved:
  - untracked `docs/FID/RegisteredClassification/tempFunc.mc`

### Findings addressed

1. `RegisteredClassificationHalconRunner` now uses metadata thresholds as Task 4 source of truth:
   - `effectiveRejectScore = qBound(0, metadata.thresholds.rejectScore, 100)`
   - `effectiveTop2Gap = qMax(0, metadata.thresholds.top2Gap)`
   - applied to `classification_rejected`, `classification_ambiguous`, result fields, and payload
2. Smoke now asserts higher-risk inference behavior:
   - `predictedLabel == "Bright"`
   - `predictedClassId == 0`
   - payload `topClasses` exists and first entry matches predicted label/class id
   - payload `rejectScore` reflects metadata threshold
   - metadata-driven `classification_ambiguous`
   - metadata-driven `classification_rejected`

### Commands run for review fix

1. Test-first smoke update and red run:
   - `cd smoke && /home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro && make -j$(nproc) && ../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke`

   Output:
   - `FAIL: inference must use metadata rejectScore`
   - `FAIL: payload rejectScore must come from metadata`
   - `FAIL: metadata top2Gap must trigger classification_ambiguous`
   - `FAIL: ambiguity result must expose metadata top2Gap`
   - `FAIL: ambiguity payload top2Gap must come from metadata`
   - `FAIL: rejection result must expose metadata rejectScore`
   - `FAIL: rejection payload rejectScore must come from metadata`
   - `FAIL: metadata rejectScore must trigger classification_rejected`

2. Verification after runner fix:
   - `cd smoke && /home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro && make -j$(nproc) && ../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke`

   Output:
   - `registered_classification_mlp_backend_smoke: metadata, feature, training, and inference checks passed`

3. Project build verification:
   - `mkdir -p build/task4 && cd build/task4 && /home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../qt_ui_test.pro && make -j$(nproc)`

   Output:
   - `qmake ../../qt_ui_test.pro` exit code `0`
   - `make -j$(nproc)` exit code `0`

4. Whitespace verification:
   - `git diff --check`

   Output:
   - exit code `0`
