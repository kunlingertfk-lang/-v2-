# Position Correction UI Stage 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the documented position-correction UI/configuration loop without implementing or simulating the HALCON correction algorithm.

**Architecture:** Keep the reference-image correction as scheme-level JSON and tool corrections as ordinary `ToolConfig` instances. Introduce a small toolcore source registry for stable-ID filtering and reuse it from the tools page and consumer dialogs. The standalone dialog saves bindings and ROI intent while all test actions report `not implemented`.

**Tech Stack:** Qt 5.15 Widgets/UI, qmake, QJsonObject, existing ToolConfig/SchemeStore/FrameViewHelper infrastructure.

## Global Constraints

- Core visual algorithms remain HALCON-only; this stage adds no algorithm.
- Reference node ID is exactly `reference.positionCorrection`.
- Dynamic list numbers are display-only; references use stable IDs.
- Existing uncommitted user files and unrelated tool behavior are preserved.

---

### Task 1: Stable source/config model

**Files:**
- Modify: `src/toolcore/PositionCorrection.{h,cpp}`
- Modify: `src/toolcore/ToolTypes.h`
- Test: `smoke/position_correction_smoke.cpp`

- [ ] Add failing checks for `PositionCorrection` type mapping, default source ID, source-ID round trip, preceding-source filtering, and invalid retained sources.
- [ ] Run the smoke and confirm failures are caused by missing APIs.
- [ ] Add `positionCorrectionSourceId` support and a focused source-registry API based on `ToolConfig.toolId` and tool order.
- [ ] Re-run the smoke and confirm all checks pass.

### Task 2: Scheme-level reference correction

**Files:**
- Modify: `src/SchemeStore.{h,cpp}`
- Modify: `src/ReferenceImageDialog.{h,cpp}` and `ui/ReferenceImageDialog.ui`
- Test: `smoke/position_correction_ui_smoke.cpp`

- [ ] Add failing checks for default-disabled scheme config and JSON-preserving save/reload behavior.
- [ ] Add `referencePositionCorrection` to `SchemeState` and serialization.
- [ ] Wire the existing reference-page switch to persisted config and add the enabled template-area UI state.
- [ ] Keep ROI/test actions disabled with explicit no-image/no-template/not-implemented messages.

### Task 3: Position correction tool and dialog

**Files:**
- Create: `src/PositionCorrectionDialog.{h,cpp}`, `ui/PositionCorrectionDialog.ui`
- Modify: `src/ToolLibraryDialog.{h,cpp}`, `ui/ToolLibraryDialog.ui`
- Modify: `src/ToolsDialog.cpp`, `qt_ui_test.pro`
- Test: `smoke/position_correction_ui_smoke.{cpp,pro}`

- [ ] Add failing UI/config checks for tool-library selection, independent tool IDs, dialog save/load, three bindings, template mode, and `not implemented` status.
- [ ] Implement the dialog using the existing large tool-dialog structure and screenshot copy/layout.
- [ ] Add the Location-category entry and ToolsDialog add/edit/card support.
- [ ] Verify two instances retain independent configs and IDs.

### Task 4: Consumer source selection and verification

**Files:**
- Create: `src/toolcore/PositionCorrectionUiBinding.{h,cpp}`
- Modify: `src/ToolsDialog.cpp`, `qt_ui_test.pro`
- Test: `smoke/position_correction_ui_smoke.cpp`

- [ ] Add failing checks that consumer menus contain only the reference node and enabled preceding correction instances.
- [ ] Populate existing position-correction combo boxes generically before each tool dialog opens and write the selected stable ID after acceptance.
- [ ] Preserve invalid IDs and expose an error item instead of silently selecting the reference node.
- [ ] Run both position-correction smokes, qmake, make, and `git diff --check`.
