# Registered Classification Training Prompt Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Update the registered classification prompt guide so it matches the current training-window UI phase and provides a copy-ready prompt for the next UI modification.

**Architecture:** This is a documentation-only change. Keep the existing prompt guide structure, edit only conflicting sections, and add a new second-phase training-window UI prompt at the end.

**Tech Stack:** Markdown documentation in `docs/FID/RegisteredClassification/注册分类提示词规范.md`; verification with `rg`, `sed`, and `git diff --check`.

## Global Constraints

- Only modify `docs/FID/RegisteredClassification/注册分类提示词规范.md`.
- Do not modify C++ code, smoke tests, qmake files, or `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`.
- Preserve HALCON as the required core vision algorithm constraint.
- Preserve `.scbin` as unsupported unless a HALCON-native reader or official conversion chain is confirmed.
- Keep real HALCON training, real dataset persistence, model generation, and non-HALCON classifier logic out of scope.
- Distinguish main configuration dialog detection ROI from training-window annotation ROI.
- Training-window control style must inherit the existing project/window style rather than introducing a new visual system.

---

### Task 1: Update Prompt Guide Boundaries

**Files:**
- Modify: `docs/FID/RegisteredClassification/注册分类提示词规范.md`

**Interfaces:**
- Consumes: Approved spec `docs/superpowers/specs/2026-07-07-registered-classification-training-prompt-design.md`.
- Produces: Prompt guide sections that no longer contradict the current training-window UI phase.

- [x] **Step 1: Replace the fixed-range heading and scope wording**

Edit the section headed `## 第一版固定范围` to `## 当前阶段固定范围`.

Within that section, replace the opening sentence with:

```markdown
当前阶段以已完成的“推理闭环”为基础，继续推进“注册训练窗口 UI 交互闭环”。目标是让主工具能创建、保存、回显、导入 HALCON 可用分类模型、执行分类推理、输出 OK/NG 和错误状态；注册训练窗口可按参考图完善注册图、标注 ROI、类别列表和预览交互，但仍不接入真实 HALCON 训练、真实数据集落盘或模型生成。
```

- [x] **Step 2: Update In/Out bullets for training-window UI**

In the same section, add these `In:` bullets near the existing UI bullets:

```markdown
- 注册训练窗口允许继续完善注册图导入/抓图、缩略图列表、图片切换、标注 ROI、类别列表、新建类别和类别 ROI 预览等 UI 交互闭环。
- 注册训练窗口标注 ROI 支持全屏、矩形和多边形的绘制/预览语义；当前只作为 UI 标注交互，不保存为真实训练样本。
```

Replace the old `Out:` bullet:

```markdown
- 注册图像窗口可做 UI 占位闭环，但不得伪装已接入真实标注、数据集管理或 HALCON 训练。
```

with:

```markdown
- 注册训练窗口不得伪装已接入真实 HALCON 训练、真实数据集落盘或模型生成。
```

- [x] **Step 3: Rewrite conflicting UI-stage lines**

In the copy-ready `## UI 实现提示词` block, replace the old target lines that require only placeholder training/model-management windows with wording that allows UI interaction:

```markdown
- `注册训练` 按钮打开注册训练窗口；窗口可继续完善注册图、标注 ROI、类别列表、缩略图和类别预览等 UI 交互闭环，但不得伪造真实训练结果、真实数据集落盘或模型生成。
- `模型管理` 可提供创建数据集占位弹窗，确认后进入注册训练窗口；仍不得伪造真实数据集落盘或真实训练。
```

Also replace the `Out:` bullet:

```markdown
- 不实现注册图像、标注、训练、模型管理真实训练能力。
```

with:

```markdown
- 不实现真实 HALCON 训练、真实数据集落盘、模型生成或模型管理真实训练能力。
```

- [x] **Step 4: Split ROI requirements by dialog**

In `## ROI 交互要求`, keep the main dialog requirement as full and rectangle only, then append:

```markdown
注册训练窗口标注 ROI 与主配置对话框检测 ROI 分开处理：
- 主配置对话框检测 ROI 仍只要求 `Full` 和 `DetectRect`，用于推理检测区域保存、回显和 overlay。
- 注册训练窗口标注 ROI 可支持 `全屏框选`、`矩形框选`、`多边形框选` 的 UI 绘制和预览。
- 注册训练窗口 ROI 按钮必须互斥；再次点击已高亮按钮时退出当前绘制状态。
- 注册训练窗口 ROI 当前不写入真实训练样本或数据集；后续如要落盘，必须另起训练/数据集阶段设计。
```

- [x] **Step 5: Add the second-phase prompt at the end**

Append a new section after the existing `## 一阶段新需求` section:

```markdown
## 注册训练窗口二阶段 UI 修改提示词

后续可以直接复制以下提示词，对注册训练窗口按参考图继续修改：

```markdown
基于当前项目修改注册分类的注册训练窗口。请先阅读：
- `AGENTS.md`
- `docs/FID/Function_Docs.md`
- `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`
- `docs/FID/RegisteredClassification/注册分类提示词规范.md`
- `src/RegisteredClassificationTrainingDialog.cpp`
- `src/RegisteredClassificationTrainingDialog.h`
- `src/RegisteredClassificationDialog.cpp`
- `src/frame/FrameViewHelper.h`
- `src/frame/FrameViewHelper.cpp`
- `qt_ui_test.pro`

## 目标

只修改注册训练窗口 UI 和交互闭环，控件样式继承当前窗口和项目原貌。参考用户给的图2、图3，将当前图1结构调整为：
- 左侧为大图预览区，显示当前注册图像；有标注时显示 ROI overlay，无标注时明确显示无标注状态。
- 左侧底部增加注册图缩略图列表，显示导入图片缩略图、文件名、选中状态和已标注状态。
- 支持上/下张或左右切换当前注册图，按循环列表语义切换。
- 点击缩略图切换大图，选中缩略图高亮。
- 右侧 `2/ 标注图像` 下方使用分类列表样式，不再使用临时表格符号。
- 分类列表显示：类别名称、目标总数/图像总数、重命名、预览/查看、删除。
- 点击某个类别的预览/查看按钮时，左侧显示该类别已标注 ROI 的预览图；点击不同类别预览图时，左侧切换对应 ROI。
- 支持 `+ 新建` 类别，创建后出现在分类列表中。
- 支持清除全部标注入口，清除当前窗口内 ROI 缓存和类别预览缓存。
- 保留 `模型类型`、`任务类型`、注册状态和 `开始训练` 按钮；开始训练仍禁用或返回训练未接入提示。

## 范围

In:
- 修改 `RegisteredClassificationTrainingDialog` 的布局和交互。
- 可新增训练窗口内部的小型数据结构，用于保存当前会话内的图片列表、类别列表和 ROI 预览缓存。
- 可补充必要的自动化 smoke 断言，验证关键控件存在、缩略图切换、类别新建、ROI 预览按钮、删除/清除动作不会崩溃。

Out:
- 不实现真实 HALCON 训练。
- 不实现真实数据集落盘。
- 不生成模型文件。
- 不支持 `.scbin`。
- 不改主注册分类 Dialog 的推理、Adapter 或 Runner 链路。
- 不引入 OpenCV、自写分类器或第三方库作为核心分类算法。
- 不重新设计控件样式；必须继承当前项目风格，仅按参考图调整布局、状态和交互。

## 交互要求

- `相机抓图`：从当前相机帧取图，加入缩略图列表，并显示到左侧大图。
- `外部导入`：打开图片文件选择，只允许图片格式；导入后加入缩略图列表并显示到左侧大图。
- 当前相机帧为空、图片读取失败时，在状态栏显示明确错误，不崩溃。
- 无图片时，ROI 绘制按钮不可产生有效标注，并显示请先添加注册图。
- ROI 按钮互斥；再次点击已选按钮退出绘制状态。
- 矩形和多边形 ROI 完成后，当前图片标记为已标注，并更新当前类别的目标总数/图像总数。
- 点击类别行的预览/查看图标时，左侧进入该类别 ROI 预览状态；关闭预览或切回缩略图后恢复当前图片大图。
- 删除动作至少删除当前类别在当前图片上的 ROI 缓存；如只做当前会话缓存，必须在状态栏说明。
- 清除全部标注清空当前会话内所有 ROI 缓存、已标注状态和类别预览缓存。

## UI 要求

- 顶部标题栏、右侧卡片、按钮边框、字体、颜色和高亮状态继承当前 `RegisteredClassificationTrainingDialog` 的样式。
- 图标按钮必须有 tooltip，不使用 `...`、`×` 这类临时文本符号作为最终操作表达。
- 分类列表行要像参考图一样能显示选中/异常或不同类别状态，但颜色应沿用当前项目高对比规则。
- 左侧底部缩略图区域应占用固定高度，不挤压右侧参数区。
- 文案以图中语义为准：`1/ 添加注册图`、`2/ 标注图像`、`分类列表`、`目标总数/图像总数`、`+ 新建`、`模型类型`、`任务类型`、`未注册`、`开始训练`。

## 验证

运行：
- `git diff --check`
- `/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro`
- `make -j8`

如修改或新增 smoke：
- 运行对应 `smoke/registered_classification_dialog_smoke` 或新 smoke。

手动验证：
- 注册训练窗口能打开。
- 抓图/外部导入能显示大图并生成缩略图。
- 点击缩略图能切换当前大图。
- ROI 按钮高亮、互斥、二次点击退出。
- 矩形/多边形 ROI 能在大图显示，并更新已标注状态。
- 新建类别、预览类别 ROI、删除当前 ROI、清除全部标注不崩溃。
- 开始训练不伪造成功，仍明确提示训练算法未接入或保持禁用。
```
```

- [x] **Step 6: Verify documentation text**

Run:

```bash
rg -n "注册图像窗口可做 UI 占位闭环|不得伪装已接入真实标注|注册训练.*只能|不实现注册图像、标注" docs/FID/RegisteredClassification/注册分类提示词规范.md
git diff --check -- docs/FID/RegisteredClassification/注册分类提示词规范.md
```

Expected:
- `rg` returns no lines for removed conflict wording.
- `git diff --check` exits 0.

- [x] **Step 7: Commit**

```bash
git add docs/FID/RegisteredClassification/注册分类提示词规范.md
git commit -m "docs: update registered classification training prompt"
```
