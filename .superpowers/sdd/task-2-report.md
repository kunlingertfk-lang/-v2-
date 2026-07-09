# Task 2 Report: Deterministic ROI Feature Extractor

## Status

DONE_WITH_CONCERNS

## Files changed

- `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h`
- `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp`
- `smoke/registered_classification_mlp_backend_smoke.cpp`
- `smoke/registered_classification_mlp_backend_smoke.pro`
- `qt_ui_test.pro`
- `.superpowers/sdd/task-2-report.md`

## Commits made

- Base dependency commit present before Task 2 work: `f50bf55` (`feat: add registered classification model package`)
- Task 2 implementation commit: current `HEAD` at handoff with message `feat: extract registered classification roi features`

## Exact commands run

```bash
sed -n '1,220p' /home/tt/.codex/plugins/cache/openai-curated-remote/superpowers/6.1.1/skills/using-superpowers/SKILL.md
sed -n '1,260p' /home/tt/.codex/plugins/cache/openai-curated-remote/superpowers/6.1.1/skills/brainstorming/SKILL.md
sed -n '1,260p' /home/tt/.codex/plugins/cache/openai-curated-remote/superpowers/6.1.1/skills/test-driven-development/SKILL.md
sed -n '1,260p' .superpowers/sdd/task-2-brief.md
sed -n '261,520p' .superpowers/sdd/task-2-brief.md
git status --short
rg -n "registeredClassificationFeatureNamesV1|RegisteredClassificationModelPackage|RegisteredClassificationFeatureExtractor|mlp_backend_smoke" src smoke qt_ui_test.pro
git show --stat --oneline f50bf55 --
sed -n '1,260p' smoke/registered_classification_mlp_backend_smoke.cpp
sed -n '1,220p' smoke/registered_classification_mlp_backend_smoke.pro
sed -n '1,240p' qt_ui_test.pro
sed -n '1,240p' src/algorithms/recognition/RegisteredClassificationModelPackage.h
sed -n '1,240p' src/algorithms/halcon/HalconRuntimePaths.h
sed -n '1,260p' src/algorithms/halcon/HalconRuntimePaths.cpp
rg -n "dlopen|dlsym|GetErrorText|GenImageInterleaved|rgb1_to_gray|gray_histo|moments_region_2nd|create_tuple|set_tuple|destroy_tuple" src/algorithms -g'*.cpp' -g'*.h'
sed -n '1,320p' src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro && make -j$(nproc)
sed -n '70,130p' src/algorithms/recognition/RegisteredClassificationModelPackage.cpp
for p in /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/lib/x64-linux/libhalconc.so /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/lib/x64-linux/libhalconc.so.24.11.2; do if [ -e "$p" ]; then HALCON_LIB="$p"; break; fi; done; nm -D "$HALCON_LIB" | rg 'gen_image_interleaved|rgb1_to_gray|gen_rectangle1|reduce_domain|threshold$|area_center|moments_region_2nd|intensity$|gray_histo$|clear_obj|get_error_text|F_create_tuple|F_set_d|F_set_i|F_set_s|F_destroy_tuple|F_get_d|F_get_i'
rg -n "gray_histo\\(|gray_histo_range|T_gray_histo_range|F_get_d|F_get_i|F_destroy_tuple|HTUPLE_INITIALIZER|tupleLength|length\\(" src/algorithms/recognition/ColorRecognitionHalconRunner.cpp src/algorithms/ocr/OcrHalconRunner.cpp src/algorithms/presence/PatternPresenceHalconApi.cpp
sed -n '500,760p' src/algorithms/recognition/ColorRecognitionHalconRunner.cpp
sed -n '150,320p' src/algorithms/ocr/OcrHalconRunner.cpp
rg -n "Hobject .*H_EMPTY|HTUPLE_INITIALIZER|clear_obj\\(|Hobject .* =" src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp src/algorithms/recognition/ColorRecognitionHalconRunner.cpp src/algorithms/ocr/OcrHalconRunner.cpp
sed -n '320,520p' src/algorithms/ocr/OcrHalconRunner.cpp
grep -n "typedef struct Hobject" -n /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/include/HalconC.h | head
rg -n "halconObjectAllocated|NO_OBJECTS" src/algorithms/recognition/ColorRecognitionHalconRunner.cpp src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp src/algorithms/ocr/OcrHalconRunner.cpp | sed -n '1,120p'
sed -n '1,60p' src/algorithms/ocr/OcrHalconRunner.cpp
make -j$(nproc)
../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
sed -n '1,260p' /home/tt/.codex/plugins/cache/openai-curated-remote/superpowers/6.1.1/skills/systematic-debugging/SKILL.md
gdb -q -batch -ex run -ex bt --args ../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke >/tmp/rc_mlp_smoke.out 2>/tmp/rc_mlp_smoke.err; echo EXIT:$?; tail -n 40 /tmp/rc_mlp_smoke.out; tail -n 40 /tmp/rc_mlp_smoke.err
rg -n "moments_region_2nd|area_center|intensity|gray_histo|threshold" /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/include/HProto.h | head -n 80
rg -n "elliptic_axis|moments_region_2nd\\(" /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady/include/HProto.h | head -n 40
sed -n '20,170p' src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp
sed -n '170,320p' src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp
sed -n '320,520p' src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp
sed -n '520,760p' src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp
make -j$(nproc)
../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
mkdir -p build && cd build && /home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro && make -j$(nproc)
sed -n '1,260p' /home/tt/.codex/plugins/cache/openai-curated-remote/superpowers/6.1.1/skills/verification-before-completion/SKILL.md
git status --short
sed -n '1,240p' .superpowers/sdd/task-2-report.md
git rev-parse --short HEAD
```

## Key outputs

- RED step succeeded as intended:
  - `registered_classification_mlp_backend_smoke.cpp:2:10: fatal error: algorithms/recognition/RegisteredClassificationFeatureExtractor.h: 没有那个文件或目录`
- HALCON symbol inspection from `nm -D` showed:
  - `T_area_center`
  - `T_moments_region_2nd`
  - `T_intensity`
  - `T_gray_histo`
  - plain `gray_histo` was not exported from `libhalconc.so`
- First smoke run after initial implementation crashed:
  - `Thread 1 "registered_clas" received signal SIGSEGV`
  - backtrace top frames:
    - `CLIStoreOCD`
    - `moments_region_2nd`
    - `RegisteredClassificationFeatureExtractor::extract(...)`
- Root cause evidence from `HProto.h`:
  - `T_moments_region_2nd(const Hobject Regions, Htuple *M11, Htuple *M20, Htuple *M02, Htuple *Ia, Htuple *Ib);`
  - `moments_region_2nd(const Hobject Regions, double *M11, double *M20, double *M02, double *Ia, double *Ib);`
  - `T_area_center(...)` and `T_intensity(...)` are the tuple-based exports
- Final smoke output:
  - `registered_classification_mlp_backend_smoke: metadata checks passed`
- Main project build completed with exit 0 and linked:
  - `qt_ui_test/bin/qt_ui_test`

## What changed

- Added `RegisteredClassificationFeatureExtractor` with HALCON-backed ROI feature extraction.
- Kept OpenCV usage limited to converting input into a continuous `CV_8UC3` BGR buffer for `gen_image_interleaved`.
- Implemented the feature vector in `registeredClassificationFeatureNamesV1()` order:
  - ROI geometry features
  - foreground area/center features
  - second-moment-derived features
  - gray mean/min/max/deviation
  - 16 compressed gray histogram bins
- Extended the existing MLP backend smoke with:
  - successful synthetic feature extraction check
  - feature length/name parity check
  - empty image failure check
  - tiny ROI failure check
- Registered the new extractor files in both `smoke/registered_classification_mlp_backend_smoke.pro` and `qt_ui_test.pro`.

## Tests run

1. RED verification
   - `cd smoke && /home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro && make -j$(nproc)`
   - Result: failed because `RegisteredClassificationFeatureExtractor.h` did not exist yet

2. Smoke build after implementation
   - `cd smoke && make -j$(nproc)`
   - Result: success

3. Smoke runtime verification
   - `cd smoke && ../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke`
   - Result: success after ABI/signature fix

4. Main Qt project verification
   - `mkdir -p build && cd build && /home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro && make -j$(nproc)`
   - Result: success

## Self-review notes

- I stayed inside the Task 2 write scope for production code and build wiring.
- I did not replace HALCON feature computation with OpenCV; OpenCV only prepares the BGR bridge buffer.
- I verified the initial smoke failure before adding production code.
- I hit one real integration bug: I initially used the wrong HALCON C ABI for tuple-returning operators, reproduced the crash under `gdb`, then corrected the implementation against the local `HProto.h`.
- I used the Task 1 package metadata interface already present in commit `f50bf55`.

## Concerns

- The brief’s simplified `moments_region_2nd`/`area_center`/`intensity` shape does not match the actual exported HALCON C ABI on this machine. The working implementation uses the tuple-based `T_*` exports where required, and derives `momentPhi` from `M11/M20/M02`.
- The smoke only checks feature extraction contract and basic error handling; it does not yet validate numerical expectations for every feature component.
