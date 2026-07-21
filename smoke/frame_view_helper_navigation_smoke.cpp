#include "frame/FrameViewHelper.h"

#include <QApplication>
#include <QAbstractGraphicsShapeItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLineF>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void check(bool condition, const char *message)
{
    if (condition)
        return;

    std::cerr << "frame_view_helper_navigation_smoke: " << message << std::endl;
    std::exit(1);
}

bool near(qreal actual, qreal expected, qreal tolerance = 0.001)
{
    return std::abs(actual - expected) <= tolerance;
}

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

void sendMouseDoubleClick(QWidget *viewport,
                          const QPoint &position,
                          Qt::KeyboardModifiers modifiers)
{
    QMouseEvent event(QEvent::MouseButtonDblClick, QPointF(position),
                      Qt::LeftButton, Qt::LeftButton, modifiers);
    QApplication::sendEvent(viewport, &event);
    QApplication::processEvents();
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QGraphicsView view;
    view.resize(500, 380);
    FrameViewHelper helper(&view);
    helper.setImage(QImage(640, 480, QImage::Format_RGB32));
    helper.setRoiRectNormalized(QRectF(0.2, 0.25, 0.3, 0.35));
    view.show();
    QApplication::processEvents();

    int roiSignalCount = 0;
    QObject::connect(&helper, &FrameViewHelper::roiChanged,
                     [&roiSignalCount](const QRectF &) { ++roiSignalCount; });

    check(!helper.navigationEnabled(), "navigation must remain opt-in");
    const QPoint disabledCursor(330, 210);
    const QTransform disabledTransform = view.transform();
    sendWheel(view.viewport(), disabledCursor, 120, Qt::NoModifier);
    sendMousePress(view.viewport(), QPoint(260, 190), Qt::NoModifier);
    sendMouseMove(view.viewport(), QPoint(300, 220), Qt::NoModifier);
    sendMouseRelease(view.viewport(), QPoint(300, 220), Qt::NoModifier);
    check(view.transform() == disabledTransform && near(helper.viewScale(), 1.0),
          "wheel and drag must not navigate while navigation is disabled");

    helper.setNavigationEnabled(true);
    check(helper.navigationEnabled() && helper.isFitToView()
          && near(helper.viewScale(), 1.0),
          "enabling navigation must start at fit scale");
    helper.zoomIn();
    check(near(helper.viewScale(), 1.25) && !helper.isFitToView(),
          "zoomIn must use the public navigation scale contract");
    helper.zoomOut();
    check(near(helper.viewScale(), 1.0) && helper.isFitToView(),
          "zoomOut must stop at fit scale");

    QGraphicsView legacyView;
    legacyView.resize(500, 380);
    FrameViewHelper legacyHelper(&legacyView);
    legacyHelper.setImage(QImage(320, 240, QImage::Format_RGB32));
    legacyView.show();
    QApplication::processEvents();

    const QPoint legacyCenter = legacyView.viewport()->rect().center();
    const QTransform legacyTransform = legacyView.transform();
    const QPointF legacySceneCenter = legacyView.mapToScene(legacyCenter);
    sendWheel(legacyView.viewport(), QPoint(100, 80), 120, Qt::NoModifier);
    sendMousePress(legacyView.viewport(), QPoint(220, 170), Qt::NoModifier);
    sendMouseMove(legacyView.viewport(), QPoint(260, 200), Qt::NoModifier);
    sendMouseRelease(legacyView.viewport(), QPoint(260, 200), Qt::NoModifier);
    check(!legacyHelper.navigationEnabled()
          && near(legacyHelper.viewScale(), 1.0)
          && legacyView.transform() == legacyTransform
          && QLineF(legacyView.mapToScene(legacyCenter),
                    legacySceneCenter).length() < 0.001,
          "legacy helpers must not acquire navigation unless explicitly enabled");

    const QPoint cursor(330, 210);
    const QPointF beforeAnchor = view.mapToScene(cursor);
    sendWheel(view.viewport(), cursor, 120, Qt::NoModifier);
    const QPointF afterAnchor = view.mapToScene(cursor);
    check(near(helper.viewScale(), 1.25)
          && QLineF(beforeAnchor, afterAnchor).length() < 0.75,
          "wheel zoom must keep the scene point under the cursor");
    check(helper.roiRectNormalized() == QRectF(0.2, 0.25, 0.3, 0.35),
          "view zoom must not change normalized ROI");

    for (int i = 0; i < 20; ++i)
        sendWheel(view.viewport(), cursor, 120, Qt::NoModifier);
    check(near(helper.viewScale(), 8.0), "wheel zoom must stop at 8x");
    for (int i = 0; i < 30; ++i)
        sendWheel(view.viewport(), cursor, -120, Qt::NoModifier);
    check(near(helper.viewScale(), 1.0) && helper.isFitToView(),
          "wheel zoom must stop at fit scale");
    check(roiSignalCount == 0, "wheel zoom must not emit roiChanged");

    helper.fitToView();
    helper.setRoiDrawingEnabled(true);
    sendMousePress(view.viewport(), QPoint(-10, -10), Qt::NoModifier);
    sendMouseMove(view.viewport(), QPoint(160, 140), Qt::NoModifier);
    sendMouseRelease(view.viewport(), QPoint(160, 140), Qt::NoModifier);
    check(helper.roiRectNormalized() == QRectF(0.2, 0.25, 0.3, 0.35)
          && roiSignalCount == 0,
          "an ROI draft must not start outside the image");

    sendWheel(view.viewport(), cursor, 120, Qt::NoModifier);
    check(near(helper.viewScale(), 1.0),
          "plain wheel must not zoom while ROI drawing is active");
    sendWheel(view.viewport(), cursor, 120, Qt::ControlModifier);
    check(near(helper.viewScale(), 1.25),
          "Ctrl+wheel must zoom while ROI drawing is active");

    const QTransform drawingTransform = view.transform();
    sendMouseDoubleClick(view.viewport(), cursor, Qt::ControlModifier);
    check(near(helper.viewScale(), 1.25) && !helper.isFitToView()
          && view.transform() == drawingTransform,
          "double click must not reset navigation while ROI drawing is active");

    const QPointF roiBeforePan = helper.roiRectNormalized().topLeft();
    sendMousePress(view.viewport(), QPoint(260, 190), Qt::ControlModifier);
    sendMouseMove(view.viewport(), QPoint(300, 220), Qt::NoModifier);
    sendMouseRelease(view.viewport(), QPoint(300, 220), Qt::NoModifier);
    check(helper.roiRectNormalized().topLeft() == roiBeforePan
          && roiSignalCount == 0,
          "pan mode must stay locked after Ctrl is released mid-drag");

    helper.setRoiDrawingEnabled(false);
    const QPoint viewportCenter = view.viewport()->rect().center();
    const QPointF beforePan = view.mapToScene(viewportCenter);
    sendMousePress(view.viewport(), QPoint(260, 190), Qt::NoModifier);
    sendMouseMove(view.viewport(), QPoint(300, 220), Qt::NoModifier);
    sendMouseRelease(view.viewport(), QPoint(300, 220), Qt::NoModifier);
    const QPointF afterPan = view.mapToScene(viewportCenter);
    check(QLineF(beforePan, afterPan).length() > 1.0,
          "plain left drag must pan outside drawing mode");
    check(helper.roiRectNormalized() == QRectF(0.2, 0.25, 0.3, 0.35)
          && roiSignalCount == 0,
          "view pan must not change normalized ROI");

    sendWheel(view.viewport(), viewportCenter, 120, Qt::NoModifier);
    const QTransform zoomedTransform = view.transform();
    const QPointF zoomedCenter = view.mapToScene(viewportCenter);
    view.resize(540, 400);
    QApplication::processEvents();
    check(view.transform() == zoomedTransform
          && QLineF(view.mapToScene(view.viewport()->rect().center()), zoomedCenter).length() < 1.0,
          "resize must preserve a zoomed transform and center");

    sendMouseDoubleClick(view.viewport(), view.viewport()->rect().center(), Qt::NoModifier);
    check(near(helper.viewScale(), 1.0) && helper.isFitToView(),
          "double click must restore fit scale");

    ToolOverlay templateRoi;
    templateRoi.type = ToolOverlayType::Rect;
    templateRoi.rect = QRectF(40.0, 40.0, 180.0, 140.0);
    templateRoi.label = QStringLiteral("styled-template-roi");
    templateRoi.extra.insert(QStringLiteral("displayRole"),
                             QStringLiteral("color_template_roi"));
    templateRoi.extra.insert(QStringLiteral("emphasis"),
                             QStringLiteral("muted"));
    ToolOverlay detectMask;
    detectMask.type = ToolOverlayType::Polygon;
    detectMask.points = {
        QPointF(200.0, 120.0), QPointF(500.0, 120.0),
        QPointF(500.0, 360.0), QPointF(200.0, 360.0)
    };
    detectMask.label = QStringLiteral("styled-detect-mask");
    detectMask.extra.insert(QStringLiteral("displayRole"),
                            QStringLiteral("color_detect_mask"));
    detectMask.extra.insert(QStringLiteral("emphasis"),
                            QStringLiteral("active"));
    detectMask.extra.insert(QStringLiteral("clipGeometry"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("rect")},
        {QStringLiteral("rect"), QJsonObject{
             {QStringLiteral("x"), 240.0},
             {QStringLiteral("y"), 160.0},
             {QStringLiteral("width"), 160.0},
             {QStringLiteral("height"), 120.0}
         }}
    });
    helper.setToolOverlays({templateRoi, detectMask});
    bool foundMutedTemplate = false;
    bool foundEffectiveMask = false;
    bool foundOutsideWarning = false;
    for (QGraphicsItem *item : view.scene()->items()) {
        if (item->toolTip() == templateRoi.label) {
            QAbstractGraphicsShapeItem *shape =
                    dynamic_cast<QAbstractGraphicsShapeItem *>(item);
            foundMutedTemplate = shape
                    && item->opacity() < 0.31
                    && shape->pen().color() == QColor(255, 122, 0);
        } else if (item->toolTip() == detectMask.label) {
            QGraphicsPathItem *path = dynamic_cast<QGraphicsPathItem *>(item);
            foundEffectiveMask = path
                    && !path->path().isEmpty()
                    && path->pen().widthF() > 3.0
                    && path->brush().style() == Qt::BDiagPattern;
        } else if (item->toolTip().contains(
                       QStringLiteral("outside owner ROI"))) {
            QGraphicsPathItem *outside = dynamic_cast<QGraphicsPathItem *>(item);
            foundOutsideWarning = outside
                    && outside->pen().style() == Qt::DashLine
                    && outside->brush().style() == Qt::NoBrush;
        }
    }
    check(foundMutedTemplate,
          "template overlay role must render orange and muted");
    check(foundEffectiveMask,
          "mask overlay must render active clipped hatch path");
    check(foundOutsideWarning,
          "mask area outside owner ROI must render as red dashed warning");

    std::cout << "frame_view_helper_navigation_smoke: all checks passed" << std::endl;
    return 0;
}
