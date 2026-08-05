#include "frame/FrameViewHelper.h"
#include "frame/RoiEditorController.h"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void check(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << "frame_view_helper_roi_edit_smoke: " << message << std::endl;
    std::exit(1);
}

bool near(qreal actual, qreal expected, qreal tolerance = 0.002)
{
    return std::abs(actual - expected) <= tolerance;
}

bool nearRect(const QRectF &actual, const QRectF &expected, qreal tolerance = 0.002)
{
    return near(actual.x(), expected.x(), tolerance)
            && near(actual.y(), expected.y(), tolerance)
            && near(actual.width(), expected.width(), tolerance)
            && near(actual.height(), expected.height(), tolerance);
}

void sendPress(QWidget *viewport, const QPoint &position)
{
    QMouseEvent event(QEvent::MouseButtonPress, QPointF(position),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &event);
}

void sendMove(QWidget *viewport, const QPoint &position)
{
    QMouseEvent event(QEvent::MouseMove, QPointF(position),
                      Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &event);
}

void sendRelease(QWidget *viewport, const QPoint &position)
{
    QMouseEvent event(QEvent::MouseButtonRelease, QPointF(position),
                      Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &event);
    QApplication::processEvents();
}

void sendEscape(QWidget *viewport)
{
    QKeyEvent event(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
    QApplication::sendEvent(viewport, &event);
    QApplication::processEvents();
}

void dragScenePoint(QGraphicsView *view, const QPointF &from, const QPointF &to)
{
    const QPoint fromView = view->mapFromScene(from);
    const QPoint toView = view->mapFromScene(to);
    sendPress(view->viewport(), fromView);
    sendMove(view->viewport(), toView);
    sendRelease(view->viewport(), toView);
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    check(coveringPixelRect(QRectF(0.101, 0.203, 0.2, 0.3), 100, 100)
          == QRect(10, 20, 21, 31),
          "fractional ROI bounds must include every intersected edge pixel");
    check(coveringPixelRect(QRectF(0.8, 0.7, 0.2, 0.3), 100, 100)
          == QRect(80, 70, 20, 30),
          "ROI touching the right and bottom edges must stay inside the image");
    check(coveringPixelRect(QRectF(0.0, 0.0, 1.0, 1.0), 1448, 1086)
          == QRect(0, 0, 1448, 1086),
          "full-image ROI must preserve the complete source image");
    check(coveringPixelRect(QRectF(-0.1, -0.2, 1.3, 1.4), 100, 80)
          == QRect(0, 0, 100, 80),
          "out-of-image ROI bounds must be clipped without losing image pixels");

    // 纯几何层先验证边界、最小尺寸和命中，避免测试只依赖图形事件。
    RoiEditorController controller;
    controller.setBounds(QRectF(0.0, 0.0, 100.0, 80.0));
    controller.setMinimumSize(2.0);
    check(controller.hitTest(QRectF(20.0, 20.0, 40.0, 30.0),
                             QPointF(20.0, 20.0), 3.0)
          == RoiEditorController::Handle::TopLeft,
          "top-left handle must win hit testing");
    check(controller.beginRectangle(QRectF(20.0, 20.0, 40.0, 30.0),
                                    QPointF(40.0, 35.0), 3.0),
          "rectangle interior must begin a move gesture");
    check(nearRect(controller.updateRectangle(QPointF(-50.0, -50.0)),
                   QRectF(0.0, 0.0, 40.0, 30.0), 0.001),
          "moving must preserve size and clamp the whole rectangle to bounds");
    controller.finishRectangle();
    check(controller.beginRectangle(QRectF(20.0, 20.0, 40.0, 30.0),
                                    QPointF(20.0, 20.0), 3.0),
          "corner must begin a resize gesture");
    check(nearRect(controller.updateRectangle(QPointF(59.5, 49.5)),
                   QRectF(58.0, 48.0, 2.0, 2.0), 0.001),
          "resize must stop at the configured minimum size");
    check(nearRect(controller.cancel(), QRectF(20.0, 20.0, 40.0, 30.0), 0.001),
          "cancel must restore the gesture start rectangle");
    RoiEditorController::CircleGeometry nearCircle;
    nearCircle.center = QPointF(50.0, 40.0);
    nearCircle.radius = 20.0;
    check(controller.isNearCircle(nearCircle, QPointF(82.0, 40.0), 16.0)
          && !controller.isNearCircle(nearCircle, QPointF(87.0, 40.0), 16.0),
          "circle redraw exclusion must use the configured outside margin");
    RoiEditorController::LineBandGeometry nearLineBand;
    nearLineBand.p1 = QPointF(20.0, 40.0);
    nearLineBand.p2 = QPointF(80.0, 40.0);
    nearLineBand.width = 20.0;
    check(controller.isNearLineBand(nearLineBand, QPointF(50.0, 64.0), 16.0)
          && !controller.isNearLineBand(nearLineBand, QPointF(50.0, 67.0), 16.0),
          "line-band redraw exclusion must include half width and outside margin");

    QGraphicsView view;
    view.resize(700, 540);
    FrameViewHelper helper(&view);
    helper.setImage(QImage(640, 480, QImage::Format_RGB32));
    helper.setRoiRectNormalized(QRectF(0.2, 0.25, 0.3, 0.35));
    helper.setRoiDrawingEnabled(true);
    view.show();
    QApplication::processEvents();

    int commitCount = 0;
    QObject::connect(&helper, &FrameViewHelper::roiChanged,
                     [&commitCount](const QRectF &) { ++commitCount; });

    int handleCount = 0;
    for (QGraphicsItem *item : view.scene()->items()) {
        if (near(item->zValue(), 106.0, 0.001))
            ++handleCount;
    }
    check(handleCount == 8, "active rectangle must render eight fixed-size handles");

    // ROI 图像坐标：x=128, y=120, w=192, h=168。移动 (64, 48) 即归一化 (0.1, 0.1)。
    dragScenePoint(&view, QPointF(224.0, 204.0), QPointF(288.0, 252.0));
    check(nearRect(helper.roiRectNormalized(), QRectF(0.3, 0.35, 0.3, 0.35)),
          "dragging inside the rectangle must move it without changing size");
    check(commitCount == 1, "move must emit one committed roiChanged signal");

    // 拖动右边中点，将右边界从 0.6 调到 0.7。
    dragScenePoint(&view, QPointF(384.0, 252.0), QPointF(448.0, 252.0));
    check(nearRect(helper.roiRectNormalized(), QRectF(0.3, 0.35, 0.4, 0.35)),
          "dragging the right handle must resize only the right edge");
    check(commitCount == 2, "resize must emit one additional committed signal");

    // 进行一次未提交移动，然后 Esc，必须恢复且不发提交信号。
    const QRectF beforeCancel = helper.roiRectNormalized();
    const QPoint centerView = view.mapFromScene(QPointF(320.0, 252.0));
    const QPoint movedView = view.mapFromScene(QPointF(352.0, 276.0));
    sendPress(view.viewport(), centerView);
    sendMove(view.viewport(), movedView);
    check(!nearRect(helper.roiRectNormalized(), beforeCancel),
          "active gesture must update the visual ROI preview");
    sendEscape(view.viewport());
    check(nearRect(helper.roiRectNormalized(), beforeCancel),
          "Escape must restore the rectangle from the gesture snapshot");
    check(commitCount == 2, "Escape rollback must not emit a committed signal");

    // 轮廓外 10 px 仍在 16 px 安全区内，不应误触重新绘制。
    const QRectF beforeNearOutside = helper.roiRectNormalized();
    dragScenePoint(&view, QPointF(458.0, 252.0), QPointF(500.0, 300.0));
    check(nearRect(helper.roiRectNormalized(), beforeNearOutside)
          && commitCount == 2,
          "dragging inside the outside safety margin must not redraw the ROI");

    // 离 ROI 超过 16 个屏幕像素后允许直接拖出新的矩形。
    dragScenePoint(&view, QPointF(520.0, 100.0), QPointF(600.0, 160.0));
    check(nearRect(helper.roiRectNormalized(),
                   QRectF(0.8125, 100.0 / 480.0, 0.125, 0.125)),
          "dragging sufficiently far outside the ROI must redraw it");
    check(commitCount == 3,
          "outside redraw must emit one committed roiChanged signal");

    // 工具默认的全图 ROI 不应占满画布并拦截首次绘制；从内部任意位置直接重画。
    helper.setRoiRectNormalized(QRectF(0.0, 0.0, 1.0, 1.0));
    dragScenePoint(&view, QPointF(160.0, 120.0), QPointF(400.0, 300.0));
    check(nearRect(helper.roiRectNormalized(),
                   QRectF(0.25, 0.25, 0.375, 0.375)),
          "dragging inside a full-image default ROI must redraw directly");
    check(commitCount == 4,
          "redrawing a full-image default ROI must emit one committed signal");

    const QRectF redrawnRect = helper.roiRectNormalized();
    helper.setRoiDrawingEnabled(false);
    handleCount = 0;
    for (QGraphicsItem *item : view.scene()->items()) {
        if (near(item->zValue(), 106.0, 0.001))
            ++handleCount;
    }
    check(handleCount == 0, "leaving ROI edit mode must remove edit handles");
    check(nearRect(helper.roiRectNormalized(), redrawnRect),
          "leaving edit mode must preserve the last committed ROI");

    helper.clearRoi();
    CircleRoi circle;
    circle.centerNormalized = QPointF(0.4, 0.4);
    circle.radiusNormalized = 0.1;
    circle.valid = true;
    helper.setCircleRoiNormalized(circle);
    helper.setCircleDrawingEnabled(true);
    int circleCommitCount = 0;
    QObject::connect(&helper, &FrameViewHelper::circleChanged,
                     [&circleCommitCount](const CircleRoi &) {
        ++circleCommitCount;
    });

    // max(image dimension)=640，因此半径为 64 px，圆心为 (256, 192)。
    dragScenePoint(&view, QPointF(256.0, 192.0), QPointF(320.0, 240.0));
    CircleRoi editedCircle = helper.circleRoiNormalized();
    check(near(editedCircle.centerNormalized.x(), 0.5)
          && near(editedCircle.centerNormalized.y(), 0.5)
          && near(editedCircle.radiusNormalized, 0.1),
          "dragging inside a circle must move its center and preserve radius");
    check(circleCommitCount == 1,
          "circle move must emit one committed circleChanged signal");

    // 右侧半径柄由 (384, 240) 拖至 (416, 240)，半径从 64 px 变为 96 px。
    dragScenePoint(&view, QPointF(384.0, 240.0), QPointF(416.0, 240.0));
    editedCircle = helper.circleRoiNormalized();
    check(near(editedCircle.centerNormalized.x(), 0.5)
          && near(editedCircle.centerNormalized.y(), 0.5)
          && near(editedCircle.radiusNormalized, 0.15),
          "dragging the circle radius handle must resize around the fixed center");
    check(circleCommitCount == 2,
          "circle resize must emit one additional committed signal");

    helper.setCircleDrawingEnabled(false);
    helper.clearCircleRoi();
    LineBandRoi lineBand;
    lineBand.p1Normalized = QPointF(0.2, 0.4);
    lineBand.p2Normalized = QPointF(0.6, 0.4);
    lineBand.widthNormalized = 0.1;
    lineBand.valid = true;
    helper.setLineBandRoiNormalized(lineBand);
    helper.setLineBandDrawingEnabled(true);
    int lineBandCommitCount = 0;
    QObject::connect(&helper, &FrameViewHelper::lineBandChanged,
                     [&lineBandCommitCount](const LineBandRoi &) {
        ++lineBandCommitCount;
    });

    // 中心线整体移动 (64,48)，归一化偏移为 (0.1,0.1)。
    dragScenePoint(&view, QPointF(256.0, 192.0), QPointF(320.0, 240.0));
    LineBandRoi editedLineBand = helper.lineBandRoiNormalized();
    check(near(editedLineBand.p1Normalized.x(), 0.3)
          && near(editedLineBand.p1Normalized.y(), 0.5)
          && near(editedLineBand.p2Normalized.x(), 0.7)
          && near(editedLineBand.p2Normalized.y(), 0.5)
          && near(editedLineBand.widthNormalized, 0.1),
          "dragging inside a line band must move both endpoints");
    check(lineBandCommitCount == 1,
          "line-band move must emit one committed lineBandChanged signal");

    // 调整第二端点，验证长度和角度可独立变化。
    dragScenePoint(&view, QPointF(448.0, 240.0), QPointF(512.0, 192.0));
    editedLineBand = helper.lineBandRoiNormalized();
    check(near(editedLineBand.p1Normalized.x(), 0.3)
          && near(editedLineBand.p1Normalized.y(), 0.5)
          && near(editedLineBand.p2Normalized.x(), 0.8)
          && near(editedLineBand.p2Normalized.y(), 0.4),
          "dragging a line-band endpoint must change length and angle");
    check(lineBandCommitCount == 2,
          "endpoint adjustment must emit one additional committed signal");

    // 斜线宽度柄位置由公共控制层计算，沿法向拉远后宽度应增加。
    RoiEditorController::LineBandGeometry lineGeometry;
    lineGeometry.p1 = QPointF(192.0, 240.0);
    lineGeometry.p2 = QPointF(512.0, 192.0);
    lineGeometry.width = 64.0;
    const QPointF widthHandle =
            RoiEditorController::lineBandWidthHandle(lineGeometry);
    const QLineF centerLine(lineGeometry.p1, lineGeometry.p2);
    const QPointF normal(-(lineGeometry.p2.y() - lineGeometry.p1.y())
                         / centerLine.length(),
                         (lineGeometry.p2.x() - lineGeometry.p1.x())
                         / centerLine.length());
    dragScenePoint(&view, widthHandle, widthHandle + normal * 32.0);
    editedLineBand = helper.lineBandRoiNormalized();
    check(editedLineBand.widthNormalized > 0.19
          && editedLineBand.widthNormalized < 0.21,
          "dragging the width handle must resize line-band width");
    check(lineBandCommitCount == 3,
          "width adjustment must emit one additional committed signal");

    helper.setLineBandDrawingEnabled(false);
    std::cout << "frame_view_helper_roi_edit_smoke: all checks passed" << std::endl;
    return 0;
}
