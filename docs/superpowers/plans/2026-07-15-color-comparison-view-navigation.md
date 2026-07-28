# 颜色比较直方图增强与图片视图导航实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 增强颜色比较直方图的重合辨识度和放大缩放，并通过公共 `FrameViewHelper` 为颜色比较右侧图片提供不破坏 ROI 坐标的缩放和平移。

**Architecture:** 图片浏览能力在 `FrameViewHelper` 内以默认关闭的可选能力实现，颜色比较显式启用，其他工具行为不变。直方图颜色和五档放大缩放继续封装在 `ColorComparisonFeatureView`，不进入公共视图层，也不改变 HALCON 特征、得分或阈值。

**Tech Stack:** C++17、Qt 5.15.2 Widgets/Graphics View、qmake、现有 offscreen smoke。

## Global Constraints

- 不修改 HALCON H/S/V 特征提取、32×32 `hue_major` 模型、score、灵敏度或 `minScore` 判定。
- 不新增 OpenCV 或第三方算法；本任务只修改 Qt 显示和交互。
- 公共图片导航默认关闭，本次只在 `ColorComparisonDialog` 显式启用。
- 图片缩放范围为适应窗口到 `8×`；ROI 绘制态只有 `Ctrl+滚轮` 和 `Ctrl+左拖`用于浏览。
- 直方图放大只允许 `100% / 125% / 150% / 175% / 200%` 五档，切换图表恢复 100%，同图数据刷新保持倍率。
- ROI 始终使用 `view -> scene -> 原图像素 -> 0..1` 链路；平移和缩放不得改变已有 ROI 数据或发出 ROI 变更信号。
- 不修改其他 Dialog 的默认鼠标交互，不重构无关 `FrameViewHelper` 绘制能力。
- 构建产物、日志和备份文件不得提交。当前 `.gitignore` 会忽略整个 `smoke/`；新增 smoke 源码按项目 AGENTS 约束使用精确 `git add -f <源码> <pro>`，不得放宽为强制添加整个目录。

---

### Task 1: 为 FrameViewHelper 增加可选图片导航

**Files:**
- Create: `smoke/frame_view_helper_navigation_smoke.cpp`
- Create: `smoke/frame_view_helper_navigation_smoke.pro`
- Modify: `src/frame/FrameViewHelper.h`
- Modify: `src/frame/FrameViewHelper.cpp`

**Interfaces:**
- Consumes: 现有 `setImage()`、`fitToView()`、`viewToImage()`、各类 ROI normalized setter/getter 和 viewport event filter。
- Produces: `setNavigationEnabled(bool)`、`navigationEnabled() const`、`viewScale() const`、`isFitToView() const`；默认关闭时保持旧行为。

- [ ] **Step 1: 创建公共视图导航 smoke 工程**

写入 `smoke/frame_view_helper_navigation_smoke.pro`：

```qmake
QT += core gui widgets
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = frame_view_helper_navigation_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/frame_view_helper_navigation
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
SOURCES += \
    frame_view_helper_navigation_smoke.cpp \
    ../src/frame/FrameViewHelper.cpp
HEADERS += \
    ../src/frame/FrameViewHelper.h \
    ../src/toolcore/ToolOverlay.h
```

- [ ] **Step 2: 写入会失败的默认关闭、缩放锚点和 ROI 不变测试**

在 `smoke/frame_view_helper_navigation_smoke.cpp` 创建 `QApplication`、`QGraphicsView` 和 `FrameViewHelper`，使用 640×480 图像。先写入确定性的事件 helper：

```cpp
void sendWheel(QWidget *viewport,
               const QPoint &position,
               int delta,
               Qt::KeyboardModifiers modifiers)
{
    QWheelEvent event(QPointF(position),
                      QPointF(viewport->mapToGlobal(position)),
                      QPoint(), QPoint(0, delta), Qt::NoButton,
                      modifiers, Qt::NoScrollPhase, false);
    QApplication::sendEvent(viewport, &event);
    QApplication::processEvents();
}

void sendMousePress(QWidget *viewport,
                    const QPoint &position,
                    Qt::KeyboardModifiers modifiers)
{
    QMouseEvent event(QEvent::MouseButtonPress, QPointF(position),
                      Qt::LeftButton, Qt::LeftButton, modifiers);
    QApplication::sendEvent(viewport, &event);
}

void sendMouseMove(QWidget *viewport,
                   const QPoint &position,
                   Qt::KeyboardModifiers modifiers)
{
    QMouseEvent event(QEvent::MouseMove, QPointF(position),
                      Qt::NoButton, Qt::LeftButton, modifiers);
    QApplication::sendEvent(viewport, &event);
}

void sendMouseRelease(QWidget *viewport,
                      const QPoint &position,
                      Qt::KeyboardModifiers modifiers)
{
    QMouseEvent event(QEvent::MouseButtonRelease, QPointF(position),
                      Qt::LeftButton, Qt::NoButton, modifiers);
    QApplication::sendEvent(viewport, &event);
    QApplication::processEvents();
}
```

测试核心内容如下：

```cpp
QGraphicsView view;
view.resize(500, 380);
FrameViewHelper helper(&view);
helper.setImage(QImage(640, 480, QImage::Format_RGB32));
helper.setRoiRectNormalized(QRectF(0.2, 0.25, 0.3, 0.35));
view.show();
QApplication::processEvents();

check(!helper.navigationEnabled(), "navigation must remain opt-in");
helper.setNavigationEnabled(true);
check(helper.navigationEnabled() && helper.isFitToView()
      && near(helper.viewScale(), 1.0),
      "enabling navigation must start at fit scale");

const QPoint cursor(330, 210);
const QPointF beforeAnchor = view.mapToScene(cursor);
sendWheel(view.viewport(), cursor, 120, Qt::NoModifier);
const QPointF afterAnchor = view.mapToScene(cursor);
check(near(helper.viewScale(), 1.25)
      && QLineF(beforeAnchor, afterAnchor).length() < 0.75,
      "wheel zoom must keep the scene point under the cursor");
check(helper.roiRectNormalized() == QRectF(0.2, 0.25, 0.3, 0.35),
      "view zoom must not change normalized ROI");
```

补充循环滚轮断言倍率上限为 `8.0`、向下滚轮下限为 `1.0`，并连接 `roiChanged` 计数器，验证缩放和平移期间计数保持 0。

- [ ] **Step 3: 写入会失败的绘制态 Ctrl 门禁和平移锁定测试**

在同一 smoke 中增加：

```cpp
helper.fitToView();
helper.setRoiDrawingEnabled(true);
sendWheel(view.viewport(), cursor, 120, Qt::NoModifier);
check(near(helper.viewScale(), 1.0),
      "plain wheel must not zoom while ROI drawing is active");
sendWheel(view.viewport(), cursor, 120, Qt::ControlModifier);
check(near(helper.viewScale(), 1.25),
      "Ctrl+wheel must zoom while ROI drawing is active");

const QPointF roiBeforePan = helper.roiRectNormalized().topLeft();
sendMousePress(view.viewport(), QPoint(260, 190), Qt::ControlModifier);
sendMouseMove(view.viewport(), QPoint(300, 220), Qt::NoModifier);
sendMouseRelease(view.viewport(), QPoint(300, 220), Qt::NoModifier);
check(helper.roiRectNormalized().topLeft() == roiBeforePan
      && roiSignalCount == 0,
      "pan mode must stay locked after Ctrl is released mid-drag");
```

再验证非绘制态普通左拖会改变 `mapToScene(viewportCenter)`，但不会改变 normalized ROI；双击恢复 `viewScale()==1.0`。

- [ ] **Step 4: 构建 smoke 并确认 RED**

Run:

```bash
mkdir -p build/smoke/frame_view_helper_navigation
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/frame_view_helper_navigation_smoke.pro \
  -o build/smoke/frame_view_helper_navigation/Makefile
make -C build/smoke/frame_view_helper_navigation -j8
```

Expected: compile FAIL，提示 `FrameViewHelper` 尚无 `setNavigationEnabled`、`navigationEnabled`、`viewScale` 和 `isFitToView`。

- [ ] **Step 5: 声明导航接口和内部状态**

在 `FrameViewHelper.h` public 区增加：

```cpp
void setNavigationEnabled(bool enabled);
bool navigationEnabled() const;
qreal viewScale() const;
bool isFitToView() const;
```

在 private 区增加：

```cpp
bool drawingInteractionActive() const;
bool navigationGestureAllowed(Qt::KeyboardModifiers modifiers) const;
bool viewPositionInsideImage(const QPoint &viewPosition) const;
qreal sceneUnitsForViewportPixels(qreal pixels) const;
void applyWheelZoom(const QPoint &viewPosition, int angleDeltaY);
void beginPan(const QPoint &viewPosition);
void updatePan(const QPoint &viewPosition);
void endPan();

bool m_navigationEnabled = false;
bool m_isFitToView = true;
qreal m_viewScale = 1.0;
bool m_panning = false;
QPoint m_lastPanPosition;
```

- [ ] **Step 6: 实现适应窗口、光标锚点缩放和 resize 保持**

保持 `fitToView()` 为唯一重置入口：先 `resetTransform()`、`fitInView(..., KeepAspectRatio)`，然后设置 `m_viewScale=1.0`、`m_isFitToView=true`。滚轮每档使用 1.25 倍并限制到 `[1.0, 8.0]`：

```cpp
const qreal next = qBound<qreal>(1.0,
        m_viewScale * (angleDeltaY > 0 ? 1.25 : 0.8), 8.0);
const qreal factor = next / m_viewScale;
const QPointF before = m_view->mapToScene(viewPosition);
m_view->scale(factor, factor);
const QPointF after = m_view->mapToScene(viewPosition);
m_view->translate(after.x() - before.x(), after.y() - before.y());
m_viewScale = next;
m_isFitToView = qFuzzyCompare(next, 1.0);
```

viewport `Resize` 事件仅在 `m_isFitToView` 时调用 `fitToView()`；用户已放大时保持 transform 和中心。

- [ ] **Step 7: 实现鼠标门禁、模式锁定和平移**

在 `eventFilter()` 的 ROI 分支之前处理导航事件：

```cpp
const bool allowed = !drawingInteractionActive()
        || (modifiers & Qt::ControlModifier);
```

允许时，Wheel 调用 `applyWheelZoom()`；LeftButton press 调用 `beginPan()` 并返回 true。之后的 move/release 只检查 `m_panning`，不再检查当前 Ctrl，从而锁定本次操作。平移使用隐藏 scrollbar 的 value 差值，结束时恢复当前绘制态对应的十字光标或普通光标。

普通左键未满足浏览门禁时继续落入现有矩形/圆形/多边形/线带绘制分支。无图片时导航事件不消费。

- [ ] **Step 8: 修正图像外点击和视口像素命中阈值**

新增 `viewPositionInsideImage()`，只在各种 ROI 的 MouseButtonPress 开始阶段判断：

```cpp
return m_imageRect.contains(m_view->mapToScene(viewPosition));
```

矩形、圆形、多边形和线带开始绘制前使用该判断，图像外不启动草稿。保留 `viewPosToImagePoint()` 对拖动过程的 clamp 行为，使已从图像内开始的 ROI 拖到视口外时停在图像边界，而不是跳到 `(0,0)`。`polygonCloseThresholdPixels()`、顶点命中半径等视口语义尺寸通过 `sceneUnitsForViewportPixels()` 换算，确保 8× 放大时屏幕命中范围不缩成不可操作的尺寸。

- [ ] **Step 9: 运行 smoke，确认 GREEN 并提交公共能力**

Run:

```bash
make -C build/smoke/frame_view_helper_navigation -j8
QT_QPA_PLATFORM=offscreen \
  build/smoke/frame_view_helper_navigation/bin/frame_view_helper_navigation_smoke
git diff --check -- src/frame/FrameViewHelper.h src/frame/FrameViewHelper.cpp \
  smoke/frame_view_helper_navigation_smoke.cpp smoke/frame_view_helper_navigation_smoke.pro
```

Expected: binary exit `0`，输出 `frame_view_helper_navigation_smoke: all checks passed`。

Commit:

```bash
git add src/frame/FrameViewHelper.h src/frame/FrameViewHelper.cpp
git add -f smoke/frame_view_helper_navigation_smoke.cpp \
  smoke/frame_view_helper_navigation_smoke.pro
git commit -m "feat: add opt-in image view navigation"
```

---

### Task 2: 增强直方图重合颜色和放大缩放

**Files:**
- Modify: `src/ColorComparisonFeatureView.cpp`
- Modify: `styles/app.qss`
- Modify: `smoke/color_comparison_feature_view_smoke.cpp`

**Interfaces:**
- Consumes: 现有 `FeatureChart`、`ZoomOverlay::showChart()`、32-bin/32×32 template/detection 向量和三种关闭方式。
- Produces: 高亮绿色真实重合区域、响应式 85% 初始放大卡、五档图表画布缩放；overlay 动态属性 `zoomPercent` 供自动检查读取。

- [ ] **Step 1: 增加会失败的重合颜色渲染断言**

在 feature view smoke 中令模板和检测的同一个 H bin 非零，调用 `hueChart->render(&image)`，遍历像素并断言存在足量高亮重合像素：

```cpp
int brightOverlapPixels = 0;
for (int y = 0; y < rendered.height(); ++y) {
    for (int x = 0; x < rendered.width(); ++x) {
        const QColor pixel = rendered.pixelColor(x, y);
        if (pixel.green() >= 210 && pixel.red() >= 120
                && pixel.blue() <= 90) {
            ++brightOverlapPixels;
        }
    }
}
check(brightOverlapPixels >= 8,
      "overlap must use a visible solid bright-green area");
```

二维 HS 图使用相同输入渲染，断言主峰 cell 的绿色分量明显高于红/蓝，同时保留模板超出橙和检测超出青的样本 cell。

- [ ] **Step 2: 增加会失败的初始尺寸和五档缩放断言**

将 smoke 顶层 view 调整为 `1280×900`。打开 H 图后查找：

```cpp
QWidget *overlay = view.findChild<QWidget *>(
        QStringLiteral("colorComparisonFeatureZoomOverlay"));
QWidget *expanded = view.findChild<QWidget *>(
        QStringLiteral("colorComparisonExpandedFeatureChart"));
QAbstractScrollArea *scroll = view.findChild<QAbstractScrollArea *>(
        QStringLiteral("colorComparisonFeatureZoomScrollArea"));

check(overlay && overlay->property("zoomPercent").toInt() == 100,
      "newly opened chart must start at 100 percent");
check(expanded && expanded->width() >= 900 && expanded->height() >= 480,
      "expanded 1D chart must start substantially larger than before");
const QSize base = expanded->size();
sendWheel(scroll->viewport(), scroll->viewport()->rect().center(), 120);
check(overlay->property("zoomPercent").toInt() == 125
      && expanded->size() == QSize(qRound(base.width() * 1.25),
                                   qRound(base.height() * 1.25)),
      "one wheel step must scale the chart proportionally to 125 percent");
```

继续滚轮验证上限 200%、下限 100%。切换到 HS 后断言恢复 100% 且绘图区宽高相等；同一 HS 图更新检测数据时倍率保持不变。

- [ ] **Step 3: 构建 feature smoke 并确认 RED**

Run:

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake smoke/color_comparison_feature_view_smoke.pro \
  -o build/smoke/color_comparison_feature_view/Makefile
make -C build/smoke/color_comparison_feature_view -j8
QT_QPA_PLATFORM=offscreen \
  build/smoke/color_comparison_feature_view/bin/color_comparison_feature_view_smoke
```

Expected: binary exit `1`，至少失败于亮绿色像素、`zoomPercent` 或初始尺寸断言。

- [ ] **Step 4: 实现高对比一维和二维重合绘制**

一维柱先绘制模板通道色，再对真实 `overlapHeight` 绘制亮绿色实心区域：

```cpp
const QColor overlapFill(163, 230, 53, 235);
const QColor overlapEdge(54, 83, 20);
painter->fillRect(overlapRect, overlapFill);
painter->setPen(QPen(overlapEdge, m_expanded ? 1.5 : 1.0));
painter->drawRect(overlapRect);
```

保留深色斜纹但提高间距，确保实心底色仍是主要识别手段。检测白色空心轮廓最后绘制，避免被重合填充覆盖。

二维 `blendedCellColor()` 将 overlap 基色提高为 `(163,230,53)`，非零 cell 的显示 intensity 使用 `sqrt(total/maximum)`，但零 cell 仍直接返回背景色；该函数只返回 QColor，不修改输入向量、交集或 score。

- [ ] **Step 5: 将放大卡改为固定头尾、可滚动图表画布**

在 `ZoomOverlay` 中加入 `QScrollArea`，objectName 为 `colorComparisonFeatureZoomScrollArea`。标题和说明放在 scroll 外，只有 `FeatureChart` 在 scroll widget 内。记录：

```cpp
QVector<int> m_zoomSteps{100, 125, 150, 175, 200};
int m_zoomIndex = 0;
QSize m_baseChartSize;
ChartKind m_activeKind = ChartKind::Hue;
QScrollArea *m_scroll = nullptr;
```

`syncGeometry()` 根据宿主尺寸设置卡片为可用区域约 85%；一维图 base size 保持 16:9，HS base size 使用可用边长的正方形。`applyZoom()` 使用 `setFixedSize(base * percent / 100)` 并设置 overlay 动态属性：

```cpp
setProperty("zoomPercent", m_zoomSteps.at(m_zoomIndex));
```

scroll area 设置 `setAlignment(Qt::AlignCenter)`，因此缩放后的画布以图表中心展开；wheel event 每次只移动一档，达到边界后消费事件但不改变尺寸。

在 `styles/app.qss` 增加稳定 objectName 规则，避免 scroll viewport 继承全局深色/边框样式：

```css
QDialog#ColorComparisonDialog QScrollArea#colorComparisonFeatureZoomScrollArea {
    background: #ffffff;
    border: none;
}

QDialog#ColorComparisonDialog QScrollArea#colorComparisonFeatureZoomScrollArea QWidget#qt_scrollarea_viewport {
    background: #ffffff;
}

QDialog#ColorComparisonDialog QFrame[legendRole="overlap"] {
    background: #a3e635;
    border: 1px solid #365314;
}
```

- [ ] **Step 6: 避免连续刷新重建图表并实现倍率生命周期**

为内部 `FeatureChart` 增加 `setKind(ChartKind)`，更新 objectName、accessibleName、minimumSize 后重绘；`showChart()` 不再 delete/new chart。

`showChart()` 先记录 `wasVisible`。当 `!wasVisible` 或 `kind != m_activeKind` 时设置 `m_zoomIndex=0` 并重新计算 base；当窗口保持打开、kind 相同且仅 histogram/details 更新时保持 `m_zoomIndex` 和 scrollbar 位置。关闭再重新点击同图按“新打开”处理，恢复 100%。

- [ ] **Step 7: 运行 feature smoke，确认 GREEN 并提交**

Run:

```bash
make -C build/smoke/color_comparison_feature_view -j8
QT_QPA_PLATFORM=offscreen \
  build/smoke/color_comparison_feature_view/bin/color_comparison_feature_view_smoke
git diff --check -- src/ColorComparisonFeatureView.cpp \
  styles/app.qss smoke/color_comparison_feature_view_smoke.cpp
```

Expected: binary exit `0`，原有点击、关闭、Esc、遮罩关闭断言和新增颜色/五档缩放断言全部通过。

Commit:

```bash
git add src/ColorComparisonFeatureView.cpp styles/app.qss
git add -f smoke/color_comparison_feature_view_smoke.cpp \
  smoke/color_comparison_feature_view_smoke.pro
git commit -m "feat: enhance color comparison histogram zoom"
```

---

### Task 3: 仅在颜色比较中启用公共图片导航

**Files:**
- Modify: `src/ColorComparisonDialog.cpp`
- Modify: `smoke/frame_view_helper_navigation_smoke.cpp`

**Interfaces:**
- Consumes: Task 1 的 `FrameViewHelper::setNavigationEnabled(true)` 和 helper 自己对绘制状态的 Ctrl 门禁。
- Produces: 颜色比较右侧图片非绘制态滚轮/左拖，绘制态 Ctrl+滚轮/Ctrl+左拖；其他 Dialog 不启用。

- [ ] **Step 1: 增加 opt-in 隔离回归断言**

在公共导航 smoke 中创建第二个 helper，不调用 enable：

```cpp
QGraphicsView legacyView;
FrameViewHelper legacyHelper(&legacyView);
legacyHelper.setImage(QImage(320, 240, QImage::Format_RGB32));
sendWheel(legacyView.viewport(), QPoint(100, 80), 120, Qt::NoModifier);
check(!legacyHelper.navigationEnabled()
      && near(legacyHelper.viewScale(), 1.0),
      "legacy helpers must not acquire navigation unless explicitly enabled");
```

- [ ] **Step 2: 在 ColorComparisonDialog 显式启用导航**

紧接 helper 构造增加：

```cpp
m_previewHelper = new FrameViewHelper(m_previewGraphicsView, this);
m_previewHelper->setNavigationEnabled(true);
```

删除 `ColorComparisonDialog::resizeEvent()` 中对 `m_previewHelper->fitToView()` 的无条件调用；viewport Resize 由 Task 1 helper 根据 `isFitToView()` 决定是否重适应。保留 `m_featureView->syncZoomGeometry()` 和模板预览更新。

- [ ] **Step 3: 运行公共 smoke 和主工程编译**

Run:

```bash
make -C build/smoke/frame_view_helper_navigation -j8
QT_QPA_PLATFORM=offscreen \
  build/smoke/frame_view_helper_navigation/bin/frame_view_helper_navigation_smoke
source scripts/dependencies.env
/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro -o build/Makefile
make -C build -j8
```

Expected: smoke exit `0`；主工程 qmake、compile、link exit `0`。

- [ ] **Step 4: 手工验证颜色比较全部 ROI 类型**

在 Ubuntu 虚拟机运行 `build/qt_ui_test/bin/qt_ui_test`，逐项检查：

1. 未选绘制工具：滚轮以光标为锚点缩放，左拖平移，双击恢复适应窗口。
2. 模板矩形/多边形、检测矩形/圆形、模板屏蔽区和检测屏蔽区按钮高亮：普通左键绘制，普通滚轮不缩放。
3. 上述每种绘制态：`Ctrl+滚轮` 缩放，`Ctrl+左拖` 平移；松开 Ctrl 后本次拖动仍保持平移。
4. 记录缩放前后的 normalized ROI 和对应原图像素矩形，确认坐标及大小一致。
5. 连续测试同分辨率帧保持倍率和中心；切换分辨率恢复适应窗口。

- [ ] **Step 5: 检查并提交颜色比较启用点**

Run:

```bash
git diff --check -- src/ColorComparisonDialog.cpp \
  smoke/frame_view_helper_navigation_smoke.cpp
```

Commit:

```bash
git add -p src/ColorComparisonDialog.cpp
git add -f smoke/frame_view_helper_navigation_smoke.cpp
git commit -m "feat: enable image navigation for color comparison"
```

---

### Task 4: 完整回归和实现记录

**Files:**
- Modify: `docs/FID/ColorComparison/color_comparison_function_implementation.md`

**Interfaces:**
- Consumes: Tasks 1-3 的公共导航、颜色增强、直方图缩放及验证结果。
- Produces: 实现状态、自动化结果和虚拟机人工检查记录。

- [ ] **Step 1: 运行两组目标 smoke**

Run:

```bash
QT_QPA_PLATFORM=offscreen \
  build/smoke/frame_view_helper_navigation/bin/frame_view_helper_navigation_smoke
QT_QPA_PLATFORM=offscreen \
  build/smoke/color_comparison_feature_view/bin/color_comparison_feature_view_smoke
```

Expected: 两个 binary 均 exit `0` 且无 `FAIL:`。

- [ ] **Step 2: 运行颜色比较诊断 smoke**

Run:

```bash
/home/tt/Qt/5.15.2/gcc_64/bin/qmake \
  smoke/color_comparison_feature_diagnostics_smoke.pro \
  -o build/smoke/color_comparison_feature_diagnostics/Makefile
make -C build/smoke/color_comparison_feature_diagnostics -j8
build/smoke/color_comparison_feature_diagnostics/bin/color_comparison_feature_diagnostics_smoke
```

Expected: exit `0`，确认 UI 改动没有改变 runner diagnostics 数组和交集字段。

- [ ] **Step 3: 重新执行主工程构建和离屏启动检查**

Run:

```bash
source scripts/dependencies.env
/home/tt/Qt/5.15.2/gcc_64/bin/qmake qt_ui_test.pro -o build/Makefile
make -C build -j8
QT_QPA_PLATFORM=offscreen timeout 4s \
  build/qt_ui_test/bin/qt_ui_test > /tmp/color_comparison_navigation_startup.log 2>&1 || test $? -eq 124
! rg -n "stylesheet|parse error|Unknown property" \
  /tmp/color_comparison_navigation_startup.log
```

Expected: qmake/make exit `0`；应用被 timeout 正常终止，日志无 QSS 解析错误。

- [ ] **Step 4: 更新颜色比较实现记录**

在实现记录追加日期为 2026-07-15 的章节，明确记录：

```markdown
### 2026-07-15 - 直方图显示增强与图片导航

- 一维和二维重合区域改为高对比亮绿色显示；没有修改直方图数据、rawIntersection、score 或阈值。
- 放大图初始约占宿主 85%，支持 100%-200% 五档等比例缩放，固定标题/关闭/诊断区。
- 公共 FrameViewHelper 增加默认关闭的图片导航；颜色比较显式启用，其他工具行为不变。
- ROI 始终按 scene/原图/normalized 坐标处理；自动 smoke 和虚拟机手工检查覆盖矩形、圆形、多边形及屏蔽区。
- 记录实际执行的 smoke、qmake/make 和人工检查结果；未执行的人工项必须明确记录未执行原因，不得写成已通过。
```

- [ ] **Step 5: 最终卫生检查并提交文档**

Run:

```bash
git diff --check -- src/frame/FrameViewHelper.h src/frame/FrameViewHelper.cpp \
  src/ColorComparisonFeatureView.cpp src/ColorComparisonDialog.cpp \
  smoke/frame_view_helper_navigation_smoke.cpp \
  smoke/frame_view_helper_navigation_smoke.pro \
  smoke/color_comparison_feature_view_smoke.cpp \
  docs/FID/ColorComparison/color_comparison_function_implementation.md
git status --short
```

Expected: 无 whitespace error；构建产物、日志、方案图片和用户其他修改不进入提交。

Commit:

```bash
git add -p docs/FID/ColorComparison/color_comparison_function_implementation.md
git commit -m "docs: record color comparison view navigation"
```
