# 目标检测 UI 迁移阶段 2.5 Git 安全报告

## 1. git status 原始摘要

执行目录：`~/桌面/qt_znxj_v2_work_1920_515`

命令：

```bash
git status --short
```

原始输出：

```text
 M qt_ui_test.pro
 M src/BlobPresenceDialog.cpp
 M src/BlobPresenceDialog.h
 M src/CameraParamsDialog.cpp
 M src/CameraParamsDialog.h
 M src/CharacterRecognitionDialog.cpp
 M src/CharacterRecognitionDialog.h
 M src/CirclePresenceDialog.cpp
 M src/CirclePresenceDialog.h
 M src/LoginWindow.cpp
 M src/MainWindow.cpp
 M src/MainWindow.h
 M src/OutputDialog.cpp
 M src/OutputDialog.h
 M src/PatternPresenceDialog.cpp
 M src/PatternPresenceDialog.h
 M src/PlanDialogUtils.cpp
 M src/PlanDialogUtils.h
 M src/ReferenceImageDialog.cpp
 M src/ReferenceImageDialog.h
 M src/ToolLibraryDialog.cpp
 M src/ToolLibraryDialog.h
 M src/ToolsDialog.cpp
 M src/ToolsDialog.h
D  src/WindowUtils.cpp
D  src/WindowUtils.h
 M src/algorithms/ocr/OcrHalconRunner.cpp
 M src/algorithms/ocr/OcrHalconRunner.h
 M src/algorithms/presence/BlobPresenceHalconRunner.cpp
 M src/algorithms/presence/BlobPresenceHalconRunner.h
 M src/algorithms/presence/CirclePresenceHalconRunner.cpp
 M src/algorithms/presence/CirclePresenceHalconRunner.h
 M src/algorithms/presence/PatternPresenceHalconRunner.cpp
 M src/algorithms/presence/PatternPresenceHalconRunner.h
 M src/frame/CameraFrameProvider.cpp
 M src/frame/CameraFrameProvider.h
 M src/frame/FrameViewHelper.cpp
 M src/frame/FrameViewHelper.h
 M src/frame/ReferenceImageProvider.cpp
 M src/main.cpp
 M src/tooladapters/BlobPresenceAdapter.cpp
 M src/tooladapters/CirclePresenceAdapter.cpp
 M src/tooladapters/OcrAdapter.cpp
 M src/tooladapters/PatternPresenceAdapter.cpp
 M styles/app.qss
 M ui/BlobPresenceDialog.ui
 M ui/CameraParamsDialog.ui
 M ui/CirclePresenceDialog.ui
 M ui/LoginWindow.ui
 M ui/MainWindow.ui
 M ui/OutputDialog.ui
 M ui/PatternPresenceDialog.ui
 M ui/ReferenceImageDialog.ui
 M ui/ToolLibraryDialog.ui
 M ui/ToolsDialog.ui
?? backup/codex_layout_fix_20260513_120811/
?? docs/
?? projects/
?? src/ContourPresenceDialog.cpp
?? src/ContourPresenceDialog.h
?? src/EdgePresenceDialog.cpp
?? src/LinePresenceDialog.cpp
?? src/ObjectDetectionDialog.cpp
?? src/ObjectDetectionDialog.h
?? src/SchemeStore.cpp
?? src/SchemeStore.h
?? src/algorithms/halcon/
?? src/algorithms/presence/ContourPresenceHalconRunner.cpp
?? src/algorithms/presence/ContourPresenceHalconRunner.h
?? src/algorithms/presence/EdgePresenceHalconRunner.cpp
?? src/algorithms/presence/EdgePresenceHalconRunner.h
?? src/algorithms/presence/LinePresenceHalconRunner.cpp
?? src/algorithms/presence/LinePresenceHalconRunner.h
?? src/algorithms/presence/PatternPresenceAutoModelDomain.cpp
?? src/algorithms/presence/PatternPresenceAutoModelDomain.h
?? src/algorithms/presence/PatternPresenceHalconApi.cpp
?? src/algorithms/presence/PatternPresenceHalconApi.h
?? src/frame/MatImageConverter.cpp
?? src/frame/MatImageConverter.h
?? src/tooladapters/ContourPresenceAdapter.cpp
?? src/tooladapters/ContourPresenceAdapter.h
?? src/tooladapters/EdgePresenceAdapter.cpp
?? src/tooladapters/EdgePresenceAdapter.h
?? src/tooladapters/LinePresenceAdapter.cpp
?? src/tooladapters/LinePresenceAdapter.h
?? src/toolcore/ToolPreviewSnapshot.h
?? ui/ContourPresenceDialog.ui
?? ui/EdgePresenceDialog.ui
?? ui/LinePresenceDialog.ui
?? ui/ObjectDetectionDialog.ui
```

说明：`git status --short` 会折叠未跟踪目录；下方未跟踪列表使用 `git status --short -uall` / `git ls-files --others --exclude-standard` 展开。

## 2. 已修改文件列表

工作区已修改的已跟踪文件：

```text
qt_ui_test.pro
src/BlobPresenceDialog.cpp
src/BlobPresenceDialog.h
src/CameraParamsDialog.cpp
src/CameraParamsDialog.h
src/CharacterRecognitionDialog.cpp
src/CharacterRecognitionDialog.h
src/CirclePresenceDialog.cpp
src/CirclePresenceDialog.h
src/LoginWindow.cpp
src/MainWindow.cpp
src/MainWindow.h
src/OutputDialog.cpp
src/OutputDialog.h
src/PatternPresenceDialog.cpp
src/PatternPresenceDialog.h
src/PlanDialogUtils.cpp
src/PlanDialogUtils.h
src/ReferenceImageDialog.cpp
src/ReferenceImageDialog.h
src/ToolLibraryDialog.cpp
src/ToolLibraryDialog.h
src/ToolsDialog.cpp
src/ToolsDialog.h
src/algorithms/ocr/OcrHalconRunner.cpp
src/algorithms/ocr/OcrHalconRunner.h
src/algorithms/presence/BlobPresenceHalconRunner.cpp
src/algorithms/presence/BlobPresenceHalconRunner.h
src/algorithms/presence/CirclePresenceHalconRunner.cpp
src/algorithms/presence/CirclePresenceHalconRunner.h
src/algorithms/presence/PatternPresenceHalconRunner.cpp
src/algorithms/presence/PatternPresenceHalconRunner.h
src/frame/CameraFrameProvider.cpp
src/frame/CameraFrameProvider.h
src/frame/FrameViewHelper.cpp
src/frame/FrameViewHelper.h
src/frame/ReferenceImageProvider.cpp
src/main.cpp
src/tooladapters/BlobPresenceAdapter.cpp
src/tooladapters/CirclePresenceAdapter.cpp
src/tooladapters/OcrAdapter.cpp
src/tooladapters/PatternPresenceAdapter.cpp
styles/app.qss
ui/BlobPresenceDialog.ui
ui/CameraParamsDialog.ui
ui/CirclePresenceDialog.ui
ui/LoginWindow.ui
ui/MainWindow.ui
ui/OutputDialog.ui
ui/PatternPresenceDialog.ui
ui/ReferenceImageDialog.ui
ui/ToolLibraryDialog.ui
ui/ToolsDialog.ui
```

已暂存删除：

```text
src/WindowUtils.cpp
src/WindowUtils.h
```

本轮未修改算法文件，未接 Adapter/Runner。

## 3. 未跟踪文件列表

展开后的未跟踪文件：

```text
backup/codex_layout_fix_20260513_120811/CameraParamsDialog.ui
backup/codex_layout_fix_20260513_120811/OutputDialog.ui
backup/codex_layout_fix_20260513_120811/ReferenceImageDialog.ui
backup/codex_layout_fix_20260513_120811/ToolsDialog.ui
docs/analysis/display_bgr_rgb_blue_tint_fix_report.md
docs/analysis/display_blue_tint_second_diagnosis_report.md
docs/analysis/display_blue_tint_vm_capture_conclusion_cleanup_report.md
docs/analysis/display_convert_check_bgr_imwrite.png
docs/analysis/display_convert_check_qimage_bgr.png
docs/analysis/display_convert_check_qimage_bgra.png
docs/analysis/display_convert_check_qimage_gray.png
docs/analysis/object_detection_dialog_ui_migration_stage1_report.md
docs/analysis/object_detection_dialog_ui_migration_stage2_report.md
docs/analysis/pattern_contour_auto_display_tuning_report.md
docs/analysis/pattern_model_contour_clean_display_report.md
docs/analysis/pattern_model_contour_diagnosis_report.md
docs/analysis/pattern_model_points_quality_fix_report.md
docs/analysis/pattern_polygon_crash_and_contour_fix_report.md
docs/analysis/pattern_presence_polygon_auto_domain_debug_fix_report.md
docs/analysis/pattern_presence_polygon_roi_actual_ui_audit.md
docs/analysis/pattern_presence_runner_simplify_auto_domain_report.md
docs/analysis/pattern_show_contour_points_real_model_report.md
docs/analysis/presence_area_roi_full_fix_report.md
docs/analysis/presence_tools_full_hik_fix_report.md
docs/analysis/presence_tools_gap_analysis_after_regression.md
docs/analysis/v2_presence_algorithm_audit.md
docs/analysis/v2_presence_algorithm_phase2_p1_p3_regression_report.md
docs/analysis/v2_presence_algorithm_phase2_p1_p3_report.md
docs/analysis/v2_presence_algorithm_phase2_p4_p5_report.md
docs/analysis/v2_presence_algorithm_phase2_p6_contour_report.md
docs/analysis/v3_presence_tools_real_usage_audit.md
docs/patches/object_detection_ui_stage1_stage2_before_algorithm.patch
projects/scheme_1/reference.png
projects/scheme_1/scheme.json
projects/scheme_25e68a4c/reference.png
projects/scheme_25e68a4c/scheme.json
projects/scheme_58fbca01/reference.png
projects/scheme_58fbca01/scheme.json
projects/scheme_5cf2bb7b/reference.png
projects/scheme_5cf2bb7b/scheme.json
projects/scheme_75381ddd/reference.png
projects/scheme_75381ddd/scheme.json
projects/scheme_d955a9fc/reference.png
projects/scheme_d955a9fc/scheme.json
projects/scheme_dafbbc35/reference.png
projects/scheme_dafbbc35/scheme.json
src/ContourPresenceDialog.cpp
src/ContourPresenceDialog.h
src/EdgePresenceDialog.cpp
src/EdgePresenceDialog.h
src/LinePresenceDialog.cpp
src/LinePresenceDialog.h
src/ObjectDetectionDialog.cpp
src/ObjectDetectionDialog.h
src/SchemeStore.cpp
src/SchemeStore.h
src/algorithms/halcon/HalconRuntimePaths.cpp
src/algorithms/halcon/HalconRuntimePaths.h
src/algorithms/presence/ContourPresenceHalconRunner.cpp
src/algorithms/presence/ContourPresenceHalconRunner.h
src/algorithms/presence/EdgePresenceHalconRunner.cpp
src/algorithms/presence/EdgePresenceHalconRunner.h
src/algorithms/presence/LinePresenceHalconRunner.cpp
src/algorithms/presence/LinePresenceHalconRunner.h
src/algorithms/presence/PatternPresenceAutoModelDomain.cpp
src/algorithms/presence/PatternPresenceAutoModelDomain.h
src/algorithms/presence/PatternPresenceHalconApi.cpp
src/algorithms/presence/PatternPresenceHalconApi.h
src/frame/MatImageConverter.cpp
src/frame/MatImageConverter.h
src/tooladapters/ContourPresenceAdapter.cpp
src/tooladapters/ContourPresenceAdapter.h
src/tooladapters/EdgePresenceAdapter.cpp
src/tooladapters/EdgePresenceAdapter.h
src/tooladapters/LinePresenceAdapter.cpp
src/tooladapters/LinePresenceAdapter.h
src/toolcore/ToolPreviewSnapshot.h
ui/ContourPresenceDialog.ui
ui/EdgePresenceDialog.ui
ui/LinePresenceDialog.ui
ui/ObjectDetectionDialog.ui
```

## 4. 新增文件列表

没有已暂存的 `A` 新增文件。当前新增内容均处于 `??` 未跟踪状态。

与目标检测 UI 迁移直接相关的新增/未跟踪文件：

```text
ui/ObjectDetectionDialog.ui
src/ObjectDetectionDialog.h
src/ObjectDetectionDialog.cpp
docs/analysis/object_detection_dialog_ui_migration_stage1_report.md
docs/analysis/object_detection_dialog_ui_migration_stage2_report.md
docs/patches/object_detection_ui_stage1_stage2_before_algorithm.patch
docs/analysis/object_detection_ui_stage2_5_git_safety_report.md
```

其中本报告在生成后也属于未跟踪文件。

## 5. 重点文件存在性与 Git 跟踪状态

检查命令：

```bash
git ls-files -- ui/ObjectDetectionDialog.ui src/ObjectDetectionDialog.h src/ObjectDetectionDialog.cpp docs/analysis/object_detection_dialog_ui_migration_stage1_report.md docs/analysis/object_detection_dialog_ui_migration_stage2_report.md
```

输出为空，说明以下文件均未被 git 跟踪。

| 文件 | 是否存在 | 是否被 git 跟踪 |
|---|---:|---:|
| `ui/ObjectDetectionDialog.ui` | 是 | 否 |
| `src/ObjectDetectionDialog.h` | 是 | 否 |
| `src/ObjectDetectionDialog.cpp` | 是 | 否 |
| `docs/analysis/object_detection_dialog_ui_migration_stage1_report.md` | 是 | 否 |
| `docs/analysis/object_detection_dialog_ui_migration_stage2_report.md` | 是 | 否 |

结论：`ObjectDetectionDialog` 三个核心文件当前都存在，但都没有纳入版本管理。

## 6. Patch 备份

已生成 patch。

路径：

```text
docs/patches/object_detection_ui_stage1_stage2_before_algorithm.patch
```

文件大小：

```text
326542 bytes
```

Patch 当前包含：

- `qt_ui_test.pro`
- `src/MainWindow.cpp`
- `src/MainWindow.h`
- `src/ToolLibraryDialog.cpp`
- `src/ToolLibraryDialog.h`
- `src/ToolsDialog.cpp`
- `src/ToolsDialog.h`
- `ui/ToolLibraryDialog.ui`
- `ui/ObjectDetectionDialog.ui`
- `src/ObjectDetectionDialog.h`
- `src/ObjectDetectionDialog.cpp`
- `docs/analysis/object_detection_dialog_ui_migration_stage1_report.md`
- `docs/analysis/object_detection_dialog_ui_migration_stage2_report.md`

当前工程内没有保留独立 smoke 源文件；阶段二 smoke 夹具位于 `/tmp/object_detection_stage2_smoke`，不在工程目录内，因此未纳入 patch。

注意：对已跟踪但未提交的文件，patch 使用的是这些文件的当前完整 diff；如果这些文件同时包含非目标检测改动，patch 也会一并包含。

## 7. 是否适合进入算法迁移分析

适合进入算法迁移分析的只读阶段。

不建议在未完成版本固化前进入算法实现阶段。原因：

- `ObjectDetectionDialog.ui/.h/.cpp` 仍未被 git 跟踪。
- 两份阶段报告仍未被 git 跟踪。
- `qt_ui_test.pro`、`ToolLibraryDialog`、`ToolsDialog`、`MainWindow` 中与入口和编辑分发相关的改动仍未提交。
- 当前 patch 已降低丢失风险，但 patch 本身也未被 git 跟踪。

推荐在算法实现前至少执行一次人工确认：将目标检测 UI 迁移相关文件 `git add` 到明确变更集，或将当前 patch 复制到外部稳定位置。

## 8. 风险提示

如果不 `git add`、不提交，也不保留外部备份，以下文件后续最容易丢失或被误覆盖：

```text
ui/ObjectDetectionDialog.ui
src/ObjectDetectionDialog.h
src/ObjectDetectionDialog.cpp
docs/analysis/object_detection_dialog_ui_migration_stage1_report.md
docs/analysis/object_detection_dialog_ui_migration_stage2_report.md
docs/analysis/object_detection_ui_stage2_5_git_safety_report.md
docs/patches/object_detection_ui_stage1_stage2_before_algorithm.patch
```

以下已跟踪文件中包含目标检测入口或编辑分发相关改动，虽然文件本身受 git 管理，但当前改动未提交，仍可能在后续合并、还原或手工覆盖时丢失：

```text
qt_ui_test.pro
src/ToolLibraryDialog.cpp
src/ToolLibraryDialog.h
ui/ToolLibraryDialog.ui
src/ToolsDialog.cpp
src/ToolsDialog.h
src/MainWindow.cpp
src/MainWindow.h
```

本轮未提交 commit，未删除文件，未回退文件，未修改算法，未接 V3 / 旧版算法，未新增 Adapter/Runner。
