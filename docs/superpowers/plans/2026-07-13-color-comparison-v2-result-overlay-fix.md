# 颜色比较 V2 检测结果文字恢复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 恢复颜色比较 V2 成功测量后检测 ROI 附近的 `OK/NG score:x.x` 结果文字，并用 licensed HALCON smoke 防止再次回归。

**Architecture:** `ColorComparisonHalconRunner` 继续负责生成完整结果 overlay；检测矩形/圆形和检测 Mask 保持现有顺序，最后追加一个 `color_result_text`。Adapter、Dialog 和 `FrameViewHelper` 不增加分支，继续原样传递和渲染 Runner 输出。

**Tech Stack:** C++17、Qt 5.15.2、HALCON 24.11.1 C API、qmake、现有 console/offscreen smoke。

## Global Constraints

- 只修改 `.worktrees/color-comparison-v2`，不修改当前 `main` 工作树。
- 不修改 HALCON H/S 二维直方图提取、直方图交集评分、灵敏度、阈值和模型合同。
- 不新增 OpenCV 生产算法；OpenCV 仍只用于 smoke 图像生成和 `cv::Mat` 桥接。
- 当前图不得叠加模板 ROI、模板 Mask 或同步模板几何。
- 仅成功且 `measurementValid=true` 的结果追加文字；失败路径不得伪造 `0.0` 分数。
- 保留用户测试产生的 `projects/scheme_0d5611a7/scheme.json` 和 `reference.png` 修改，不暂存、不提交。
- 构建产物和日志不得提交。

---

### Task 1: 以 TDD 恢复 Runner 结果文字 overlay

**Files:**
- Modify: `smoke/color_comparison_smoke.cpp:19-32, 470-690`
- Modify: `src/algorithms/recognition/ColorComparisonHalconRunner.cpp:1503-1537, 1813-1831`

**Interfaces:**
- Consumes: `ToolOverlay`, `ToolOverlayType::Text`, `rectToJson(const QRectF &)`, `circlePixelGeometry(...)` 和 `FrameViewHelper` 已支持的 `label=color_result_text` 合同。
- Produces: `detectionOverlays(const ColorComparisonHalconConfig &, const cv::Mat &, double score, bool passed)`，输出顺序固定为检测几何、可选检测 Mask、结果文字。

- [ ] **Step 1: 写入会失败的结果文字断言**

在 `near(...)` 后增加以下测试 helper：

```cpp
void checkResultTextOverlay(const QVector<ToolOverlay> &overlays,
                            int index,
                            double expectedScore,
                            bool expectedOk,
                            const QRectF &expectedAnchor,
                            const char *message)
{
    if (index < 0 || index >= overlays.size()) {
        check(false, message);
        return;
    }

    const ToolOverlay &overlay = overlays.at(index);
    const QJsonObject anchor = overlay.extra
            .value(QStringLiteral("anchorRect")).toObject();
    const QString expectedStatus = expectedOk
            ? QStringLiteral("OK") : QStringLiteral("NG");
    const QString expectedText = QStringLiteral("%1 score:%2")
            .arg(expectedStatus, QString::number(expectedScore, 'f', 1));
    check(overlay.type == ToolOverlayType::Text
          && overlay.label == QStringLiteral("color_result_text")
          && overlay.text == expectedText
          && near(overlay.score, expectedScore, 1e-9)
          && overlay.extra.value(QStringLiteral("status")).toString()
                 == expectedStatus
          && near(overlay.p1.x(), expectedAnchor.x(), 1e-9)
          && near(overlay.p1.y(), expectedAnchor.y(), 1e-9)
          && near(anchor.value(QStringLiteral("x")).toDouble(),
                  expectedAnchor.x(), 1e-9)
          && near(anchor.value(QStringLiteral("y")).toDouble(),
                  expectedAnchor.y(), 1e-9)
          && near(anchor.value(QStringLiteral("width")).toDouble(),
                  expectedAnchor.width(), 1e-9)
          && near(anchor.value(QStringLiteral("height")).toDouble(),
                  expectedAnchor.height(), 1e-9),
          message);
}
```

将现有成功用例的 overlay 数量改为包含文字，并在对应结果后加入以下断言：

```cpp
checkResultTextOverlay(ratio.overlays, 1, ratio.score, false,
                       QRectF(0.0, 0.0, 64.0, 64.0),
                       "NG comparison must expose score text at the detection ROI");

check(rectangle.success && near(rectangle.score, 100.0, 1e-6) &&
      rectangle.overlays.size() == 2 &&
      rectangle.overlays.at(0).type == ToolOverlayType::Rect,
      "custom template rectangle and independent detection rectangle must match");
checkResultTextOverlay(rectangle.overlays, 1, rectangle.score, true,
                       QRectF(32.0, 0.0, 32.0, 64.0),
                       "rectangle comparison must anchor score text to the detection rectangle");

const double circleRadiusPixels = 0.24 * 64.0;
check(circle.success && near(circle.score, 100.0, 1e-6) &&
      circle.overlays.size() == 2 &&
      circle.overlays.at(0).type == ToolOverlayType::Circle,
      "sync mode must build and run with the actual circular region");
checkResultTextOverlay(circle.overlays, 1, circle.score, true,
                       QRectF(32.0 - circleRadiusPixels,
                              32.0 - circleRadiusPixels,
                              circleRadiusPixels * 2.0,
                              circleRadiusPixels * 2.0),
                       "circle comparison must anchor score text to the circle bounds");

check(landscape.success && landscape.overlays.size() == 2 &&
      landscape.overlays.at(0).type == ToolOverlayType::Circle &&
      near(landscape.overlays.at(0).center.x(), imageWidth * 0.5, 1e-9) &&
      near(landscape.overlays.at(0).center.y(), imageHeight * 0.5, 1e-9) &&
      near(landscape.overlays.at(0).radius, radiusPixels, 1e-9),
      "landscape circle overlay must use the same pixel center and maximum-side radius");
checkResultTextOverlay(landscape.overlays, 1, landscape.score, true,
                       QRectF(imageWidth * 0.5 - radiusPixels,
                              imageHeight * 0.5 - radiusPixels,
                              radiusPixels * 2.0,
                              radiusPixels * 2.0),
                       "landscape circle score text must use the same pixel geometry");

check(detectMasked.success && detectMasked.score > 99.0 &&
      detectMasked.overlays.size() == 3 &&
      detectMasked.overlays.at(0).type == ToolOverlayType::Rect &&
      detectMasked.overlays.at(1).type == ToolOverlayType::Polygon,
      "detection mask overlays must retain geometry before the result text");
checkResultTextOverlay(detectMasked.overlays, 2, detectMasked.score, true,
                       QRectF(0.0, 0.0, 64.0, 64.0),
                       "masked comparison must keep score text after detection geometry");
```

- [ ] **Step 2: 重新构建并运行 licensed smoke，确认 RED**

Run:

```bash
cd build/smoke/color_comparison_qmake
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../../smoke/color_comparison_smoke.pro
make -j$(nproc)
RUN_HALCON_LICENSED_SMOKE=1 ../color_comparison/bin/color_comparison_smoke
```

如果目录不存在，先执行：

```bash
mkdir -p build/smoke/color_comparison_qmake
```

Expected: binary exit `1`，至少出现 `rectangle comparison must anchor score text to the detection rectangle`，失败原因是 Runner 尚未生成 `ToolOverlayType::Text`。

- [ ] **Step 3: 实现最小结果文字 overlay**

在 `detectionOverlays(...)` 前加入：

```cpp
ToolOverlay resultTextOverlay(const QRectF &anchorRect,
                              double score,
                              bool passed)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.label = QStringLiteral("color_result_text");
    overlay.text = QStringLiteral("%1 score:%2")
            .arg(passed ? QStringLiteral("OK") : QStringLiteral("NG"),
                 QString::number(score, 'f', 1));
    overlay.score = score;
    overlay.p1 = anchorRect.topLeft();
    overlay.extra.insert(QStringLiteral("status"),
                         passed ? QStringLiteral("OK") : QStringLiteral("NG"));
    overlay.extra.insert(QStringLiteral("anchorRect"), rectToJson(anchorRect));
    return overlay;
}
```

将 `detectionOverlays(...)` 替换为：

```cpp
QVector<ToolOverlay> detectionOverlays(const ColorComparisonHalconConfig &config,
                                       const cv::Mat &image,
                                       double score,
                                       bool passed)
{
    QVector<ToolOverlay> overlays;
    ToolOverlay roi;
    QRectF anchorRect;
    if (normalizedDetectRegionType(config.detectRegionType)
            == QStringLiteral("circle")) {
        const CirclePixelGeometry geometry = circlePixelGeometry(
                    config.detectCircleCenterNormalized,
                    config.detectCircleRadiusNormalized,
                    image.cols,
                    image.rows);
        roi.type = ToolOverlayType::Circle;
        roi.center = geometry.center;
        roi.radius = geometry.radius;
        anchorRect = QRectF(geometry.center.x() - geometry.radius,
                            geometry.center.y() - geometry.radius,
                            geometry.radius * 2.0,
                            geometry.radius * 2.0);
    } else {
        roi.type = ToolOverlayType::Rect;
        roi.rect = normalizedRectToPixels(config.detectRoiNormalized, image);
        anchorRect = roi.rect;
    }
    roi.label = QStringLiteral("Detection ROI");
    roi.extra.insert(QStringLiteral("role"), QStringLiteral("detect_roi"));
    overlays.append(roi);

    if (!config.detectMaskPolygonNormalized.isEmpty()) {
        ToolOverlay mask;
        mask.type = ToolOverlayType::Polygon;
        mask.points = normalizedPointsToPixels(
                    config.detectMaskPolygonNormalized, image);
        mask.label = QStringLiteral("Detection Mask");
        mask.extra.insert(QStringLiteral("role"),
                          QStringLiteral("detect_mask"));
        overlays.append(mask);
    }

    overlays.append(resultTextOverlay(anchorRect, score, passed));
    return overlays;
}
```

将成功路径调用改为：

```cpp
result.overlays = detectionOverlays(config, image, score, passed);
```

- [ ] **Step 4: 运行目标 smoke，确认 GREEN**

Run:

```bash
cd build/smoke/color_comparison_qmake
make -j$(nproc)
RUN_HALCON_LICENSED_SMOKE=1 ../color_comparison/bin/color_comparison_smoke
```

Expected: binary exit `0`，输出 `Color comparison V2 licensed smoke passed.`，且没有 `FAIL:`。

- [ ] **Step 5: 检查并提交 Runner 与目标 smoke**

Run:

```bash
git diff --check -- smoke/color_comparison_smoke.cpp src/algorithms/recognition/ColorComparisonHalconRunner.cpp
git status --short
```

Expected: `git diff --check` exit `0`；`git status` 除这两个文件外仍只包含用户已有的 scheme/reference 修改。

Commit:

```bash
git add smoke/color_comparison_smoke.cpp src/algorithms/recognition/ColorComparisonHalconRunner.cpp
git commit -m "fix: restore color comparison score overlay"
```

---

### Task 2: 记录修复并完成完整回归

**Files:**
- Modify: `docs/FID/ColorComparison/color_comparison_function_implementation.md`

**Interfaces:**
- Consumes: Task 1 的 `color_result_text` 合同和 licensed smoke 结果。
- Produces: 可追溯的修复原因、验证命令和剩余人工 GUI 检查记录。

- [ ] **Step 1: 在 V2 实现记录中增加结果 overlay 修复记录**

在当前 V2 实现总结的“独立复审后的合同修正”之后增加：

```markdown
### 2026-07-13 - 恢复检测区域结果文字

- 根因：V2 `detectionOverlays()` 只保留检测几何，遗漏了旧版 `color_result_text`，且 smoke 的固定数量断言把该回归固化。
- 修复：成功且测量有效的结果按“检测几何、可选检测 Mask、结果文字”顺序输出 overlay；文字显示 `OK/NG score:x.x`，矩形使用检测框、圆形使用像素外接矩形作为 `anchorRect`。
- 边界：未修改 HALCON H/S 二维直方图、直方图交集评分、灵敏度、阈值、模板模型和失败状态。
- 验证：licensed Runner smoke 覆盖 OK、NG、矩形、圆形、非方形圆和检测 Mask；Model、Dialog smoke 与完整 Qt 工程继续通过。
- 人工检查：仍需在图形界面确认小 ROI 的框外文字位置以及 OK/NG 颜色。
```

- [ ] **Step 2: 构建并运行 Model smoke**

Run:

```bash
mkdir -p build/smoke/color_comparison_model_qmake
cd build/smoke/color_comparison_model_qmake
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../../smoke/color_comparison_model_smoke.pro
make -j$(nproc)
../color_comparison_model/bin/color_comparison_model_smoke
```

Expected: qmake/make exit `0`，binary exit `0`，没有 `FAIL:`。

- [ ] **Step 3: 构建并运行 Dialog offscreen smoke**

Run:

```bash
mkdir -p build/smoke/color_comparison_dialog_qmake
cd build/smoke/color_comparison_dialog_qmake
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../../../smoke/color_comparison_dialog_smoke.pro
make -j$(nproc)
QT_QPA_PLATFORM=offscreen ../color_comparison_dialog/bin/color_comparison_dialog_smoke
```

Expected: binary exit `0`，输出 `color_comparison_dialog_smoke: V2 Dialog checks passed`。

- [ ] **Step 4: 重新构建完整 V2 Qt 工程**

Run:

```bash
cd build
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
make -j$(nproc)
make -q
```

Expected: qmake/make/make-q 均 exit `0`，可执行文件为 `build/qt_ui_test/bin/qt_ui_test`。

- [ ] **Step 5: 完成静态检查并提交实现记录**

Run:

```bash
git diff --check
git status --short
```

Expected: `git diff --check` exit `0`；不出现构建产物；用户已有的 `projects/scheme_0d5611a7/scheme.json` 和 `reference.png` 仍未暂存。

Commit:

```bash
git add docs/FID/ColorComparison/color_comparison_function_implementation.md
git commit -m "docs: record color comparison overlay regression fix"
```

- [ ] **Step 6: 人工 GUI 验收交接**

运行 `build/qt_ui_test/bin/qt_ui_test`，用矩形、圆形和小 ROI 各执行一次颜色比较：

```text
矩形：检测框内或相邻位置显示 OK/NG score:x.x
圆形：文字围绕圆形外接矩形自动摆放
小 ROI：空间不足时文字移动到框上、框下或框右
颜色：OK 为绿色，NG 为红色
底部状态栏：分数与画布分数一致
模板几何：当前检测图上不出现模板 ROI/Mask
```

Expected: 所有项目符合；若只能完成自动化验证，交接说明必须明确标记“人工 GUI 未执行”，不得宣称已人工通过。
