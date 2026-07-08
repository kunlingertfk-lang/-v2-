# Registered Classification Detection Training Style Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `RegisteredClassificationDetectionTrainingDialog` visually match `RegisteredClassificationTrainingDialog` by replacing its low-contrast QSS with the registered-classification training style.

**Architecture:** This is a style-only change scoped to `RegisteredClassificationDetectionTrainingDialog.cpp`. The dialog keeps its existing widgets, object names, state, signals, and validation logic; only the stylesheet and smoke assertions change.

**Tech Stack:** Qt 5.15 Widgets, qmake, existing C++ smoke tests.

## Global Constraints

- Only modify `src/RegisteredClassificationDetectionTrainingDialog.cpp` and `smoke/registered_classification_detection_dialog_smoke.cpp` for implementation.
- Do not modify `RegisteredClassificationTrainingDialog`.
- Do not change detection training behavior or add new controls.
- Keep the style aligned with `docs/FID/Function_Docs.md` training/detection control style rules.
- Existing smoke and main project build must pass.

---

### Task 1: Detection Training QSS Alignment

**Files:**
- Modify: `smoke/registered_classification_detection_dialog_smoke.cpp`
- Modify: `src/RegisteredClassificationDetectionTrainingDialog.cpp`

**Interfaces:**
- Consumes: existing `RegisteredClassificationDetectionTrainingDialog` widget tree.
- Produces: same dialog behavior with high-contrast QSS aligned to `RegisteredClassificationTrainingDialog`.

- [ ] **Step 1: Add smoke assertions for high-contrast style**

In `smoke/registered_classification_detection_dialog_smoke.cpp`, after the training dialog is opened, assert that the stylesheet contains the registered-classification training style markers:

```cpp
const QString trainingStyle = trainingDialog->styleSheet();
check(trainingStyle.contains(QStringLiteral("QDialog{background:#ffffff;color:#0f172a;font-size:18px;}")),
      "detection training dialog must use white high-contrast root style");
check(trainingStyle.contains(QStringLiteral("QFrame[panelRole=\"previewPanel\"],QFrame[panelRole=\"trainingCard\"]{background:#ffffff;border:3px solid #60a5fa;border-radius:8px;}")),
      "detection training preview panel and cards must use registered classification training borders");
check(trainingStyle.contains(QStringLiteral("QPushButton[actionRole=\"primary\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;font-size:20px;font-weight:800;}")),
      "detection training primary button must use registered classification training style");
```

- [ ] **Step 2: Run smoke and verify it fails before implementation**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_detection_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_detection_dialog/bin/registered_classification_detection_dialog_smoke
```

Expected: FAIL with one of the new high-contrast style assertion messages.

- [ ] **Step 3: Replace detection training QSS**

In `src/RegisteredClassificationDetectionTrainingDialog.cpp`, replace the existing `setStyleSheet(styleSheet() + QStringLiteral(...))` block at the end of the constructor with a `setStyleSheet(QStringLiteral(...))` block matching the registered-classification training style, preserving detection-specific selectors:

```cpp
    setStyleSheet(QStringLiteral(
        "QDialog{background:#ffffff;color:#0f172a;font-size:18px;}"
        "QFrame[panelRole=\"previewPanel\"],QFrame[panelRole=\"trainingCard\"]{background:#ffffff;border:3px solid #60a5fa;border-radius:8px;}"
        "QFrame[panelRole=\"rightPanel\"]{background:#ffffff;border:0;}"
        "QFrame[panelRole=\"previewToolbar\"]{background:#ffffff;border-bottom:4px solid #ff7a00;border-top-left-radius:8px;border-top-right-radius:8px;}"
        "QFrame[panelRole=\"statusBar\"]{background:#fff7ed;border-top:3px solid #ffb366;color:#7c2d12;}"
        "QFrame[panelRole=\"classHeader\"]{background:#f1f5f9;border:0;border-radius:4px;}"
        "QFrame[panelRole=\"roiPreviewPage\"]{background:#ffffff;border:0;}"
        "QFrame[panelRole=\"roiPreviewCard\"]{background:#f8fafc;border:2px solid #cbd5e1;border-radius:6px;}"
        "QFrame[panelRole=\"classRow\"]{background:#dcfce7;border:2px solid #22c55e;border-radius:6px;}"
        "QGraphicsView[panelRole=\"trainingCanvas\"]{border:0;background:#05070a;}"
        "QLabel{background:transparent;font-size:18px;color:#0f172a;font-weight:600;}"
        "QLabel[role=\"windowTitle\"]{font-size:28px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"cardTitle\"]{font-size:23px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"sectionTitle\"]{font-size:20px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"tip\"]{font-size:18px;color:#64748b;font-weight:600;}"
        "QLabel[role=\"filterBox\"]{background:#ffffff;border:2px solid #4094ff;border-radius:6px;color:#0f172a;padding:8px 18px;font-size:18px;font-weight:700;}"
        "QLabel[role=\"previewPageTitle\"]{font-size:26px;font-weight:800;color:#1f2937;}"
        "QLabel[role=\"previewEmpty\"]{font-size:22px;font-weight:700;color:#94a3b8;}"
        "QLabel[role=\"stateLabel\"]{font-size:24px;font-weight:800;color:#c2410c;}"
        "QPushButton,QToolButton,QComboBox{background:#ffffff;color:#0f172a;border:2px solid #4094ff;border-radius:6px;padding:10px;font-size:18px;font-weight:700;}"
        "QPushButton:hover,QToolButton:hover{background:#e0f2fe;border-color:#0284c7;}"
        "QToolButton:checked{background:#fff7ed;color:#c2410c;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"primary\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;font-size:20px;font-weight:800;}"
        "QPushButton:disabled,QToolButton:disabled{background:#f8fafc;color:#64748b;border-color:#94a3b8;}"
        "QListWidget{background:#27303c;color:#ffffff;border-top:3px solid #4094ff;font-size:15px;font-weight:700;}"
        "QListWidget::item{background:#344155;color:#ffffff;border:2px solid transparent;padding:6px;margin:5px;}"
        "QListWidget::item:selected{background:#0ea5e9;color:#ffffff;border-color:#ff7a00;}"
        "QScrollArea{background:#ffffff;border:0;}"
        "QScrollArea > QWidget > QWidget{background:#ffffff;}"
        "QCheckBox{font-size:18px;font-weight:700;color:#0f172a;}"
        "QCheckBox::indicator{width:36px;height:24px;border:2px solid #94a3b8;border-radius:4px;background:#ffffff;}"
        "QCheckBox::indicator:checked{background:#22c55e;border-color:#16a34a;}"));
```

- [ ] **Step 4: Run focused smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_detection_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_detection_dialog/bin/registered_classification_detection_dialog_smoke
```

Expected: PASS.

- [ ] **Step 5: Run regression smoke and main build**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j8
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke

cd ..
mkdir -p build_verify
cd build_verify
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
make -j$(nproc)
```

Expected: both commands pass.
