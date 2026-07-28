#ifndef FRAME_FRAMEVIEWHELPER_H
#define FRAME_FRAMEVIEWHELPER_H

#include <QObject>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QVector>

#include "frame/RoiEditorController.h"
#include "frame/RoiGeometry.h"
#include "toolcore/ToolOverlay.h"

class QEvent;
class QGraphicsItem;
class QGraphicsEllipseItem;
class QGraphicsPathItem;
class QGraphicsPixmapItem;
class QGraphicsPolygonItem;
class QGraphicsRectItem;
class QGraphicsScene;
class QGraphicsView;

class FrameViewHelper : public QObject
{
    Q_OBJECT

public:
    explicit FrameViewHelper(QGraphicsView *view, QObject *parent = nullptr);

    void setImage(const QImage &image);
    void clear();
    void fitToView();
    bool hasImage() const;
    void setNavigationEnabled(bool enabled);
    bool navigationEnabled() const;
    qreal viewScale() const;
    bool isFitToView() const;
    void zoomIn();
    void zoomOut();

    QPointF viewToImage(const QPoint &viewPos) const;
    QRectF imageRectToNormalized(const QRectF &imageRect) const;
    QRectF normalizedToImageRect(const QRectF &normalized) const;
    QSize imageSize() const;

    void setRoiDrawingEnabled(bool enabled);
    bool isRoiDrawingEnabled() const;
    void setRoiRectNormalized(const QRectF &roi);
    QRectF roiRectNormalized() const;
    void clearRoi();

    void setPolygonDrawingEnabled(bool enabled);
    bool isPolygonDrawingEnabled() const;
    bool finishPolygonDrawing();
    void setPolygonRoiNormalized(const QVector<QPointF> &points);
    QVector<QPointF> polygonRoiNormalized() const;
    void clearPolygonRoi();

    void setCircleDrawingEnabled(bool enabled);
    bool isCircleDrawingEnabled() const;
    void setCircleRoiNormalized(const CircleRoi &roi);
    CircleRoi circleRoiNormalized() const;
    QRectF circleBoundingRectNormalized(const CircleRoi &roi) const;
    void clearCircleRoi();

    void setPointSelectionEnabled(bool enabled);
    bool isPointSelectionEnabled() const;

    void setLineBandDrawingEnabled(bool enabled);
    bool isLineBandDrawingEnabled() const;
    void setLineBandRoiNormalized(const LineBandRoi &roi);
    LineBandRoi lineBandRoiNormalized() const;
    QRectF lineBandBoundingRectNormalized(const LineBandRoi &roi) const;
    void clearLineBandRoi();

    void setToolOverlays(const QVector<ToolOverlay> &overlays);
    void clearToolOverlays();

signals:
    void viewTransformChanged(qreal scale, bool fitToView);
    void roiChanged(const QRectF &roiNormalized);
    void roiSelectionRejected(const QRectF &imageRect);
    void polygonChanged(const QVector<QPointF> &pointsNormalized);
    void polygonSelectionRejected(int pointCount);
    void circleChanged(const CircleRoi &roi);
    void circleSelectionRejected();
    void pointSelected(const QPointF &pointNormalized);
    void lineBandChanged(const LineBandRoi &roi);
    void lineBandSelectionRejected();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    enum class PolygonDrawingState {
        Idle,
        DrawingPolygon,
        CompletedPolygon
    };

    bool drawingInteractionActive() const;
    bool navigationGestureAllowed(Qt::KeyboardModifiers modifiers) const;
    bool viewPositionInsideImage(const QPoint &viewPosition) const;
    qreal sceneUnitsForViewportPixels(qreal pixels) const;
    void applyWheelZoom(const QPoint &viewPosition, int angleDeltaY);
    void beginPan(const QPoint &viewPosition);
    void updatePan(const QPoint &viewPosition);
    void endPan();
    bool viewPosToImagePoint(const QPoint &viewPos, QPointF *imagePoint) const;
    QRectF clampedImageRect(const QRectF &rect) const;
    QPointF clampedImagePoint(const QPointF &point) const;
    QPointF imagePointToNormalized(const QPointF &point) const;
    QPointF normalizedToImagePoint(const QPointF &point) const;
    QVector<QPointF> validNormalizedPolygon(const QVector<QPointF> &points) const;
    bool completeDraftPolygon();
    double polygonCloseThresholdPixels() const;
    int polygonVertexIndexAt(const QPointF &imagePoint) const;
    bool polygonContainsImagePoint(const QPointF &imagePoint) const;
    QVector<QPointF> translatedPolygonNormalized(const QPointF &deltaImage) const;
    bool isValidCircleRoi(const CircleRoi &roi) const;
    CircleRoi validCircleRoi(const CircleRoi &roi) const;
    CircleRoi imageCircleToNormalized(const QPointF &center, double radiusPixels) const;
    QRectF circleBoundingRectImage(const CircleRoi &roi) const;
    RoiEditorController::CircleGeometry circleEditGeometry(const CircleRoi &roi) const;
    bool isValidLineBand(const LineBandRoi &roi) const;
    LineBandRoi validLineBand(const LineBandRoi &roi) const;
    LineBandRoi imageLineBandToNormalized(const QPointF &p1,
                                          const QPointF &p2,
                                          double widthPixels) const;
    RoiEditorController::LineBandGeometry lineBandEditGeometry(const LineBandRoi &roi) const;
    QVector<QPointF> lineBandPolygonImagePoints(const LineBandRoi &roi) const;
    QPointF clampedTextPosition(const QPointF &position, const QRectF &textBounds) const;
    QRectF validNormalizedRect(const QRectF &rect) const;
    bool roiCoversFullImage() const;
    void updateRoiItem();
    void updateRoiHandleItems();
    void clearRoiHandleItems();
    void updateRoiEditCursor(const QPointF &imagePoint);
    void updatePolygonItem();
    void updatePolygonVertexItems();
    void clearPolygonVertexItems();
    void updateCircleItem();
    void updateCircleHandleItems();
    void clearCircleHandleItems();
    void updateCircleEditCursor(const QPointF &imagePoint);
    void updateLineBandItem();
    void updateLineBandWidthHandleItem();
    void clearLineBandWidthHandleItem();
    void updateLineBandEditCursor(const QPointF &imagePoint);
    void updateDraftRoiItem(const QRectF &imageRect);
    void clearDraftRoiItem();
    void updateDraftPolygonItem();
    void clearDraftPolygonItem();
    void updateDraftCircleItem();
    void clearDraftCircleItem();
    void updateDraftLineBandItem();
    void clearLineBandItems(QVector<QGraphicsItem *> *items);
    void addLineBandItems(const LineBandRoi &roi, QVector<QGraphicsItem *> *items, qreal zValue);
    void addOverlayItem(QGraphicsItem *item, qreal zValue = 110.0);
    void restoreImageSceneRect();

    QGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    QGraphicsRectItem *m_roiItem = nullptr;
    QGraphicsPolygonItem *m_polygonItem = nullptr;
    QGraphicsEllipseItem *m_circleItem = nullptr;
    QGraphicsRectItem *m_draftRoiItem = nullptr;
    QGraphicsPathItem *m_draftPolygonItem = nullptr;
    QGraphicsEllipseItem *m_draftCircleItem = nullptr;
    QImage m_lastImage;
    QRectF m_imageRect;
    QRectF m_roiNormalized;
    QVector<QPointF> m_polygonNormalized;
    QVector<QPointF> m_draftPolygonImagePoints;
    QVector<QGraphicsItem *> m_draftPolygonPointItems;
    QVector<QGraphicsItem *> m_polygonVertexItems;
    QPointF m_polygonHoverPoint;
    bool m_polygonHoverPointValid = false;
    QPointF m_polygonDragStartImagePoint;
    QVector<QPointF> m_polygonDragStartNormalized;
    int m_draggingPolygonVertexIndex = -1;
    CircleRoi m_circleRoi;
    QPointF m_circleDraftCenter;
    double m_circleDraftRadiusPixels = 0.0;
    LineBandRoi m_lineBandRoi;
    QPointF m_lineBandDraftP1;
    QPointF m_lineBandDraftP2;
    double m_lineBandDraftWidthPixels = 24.0;
    bool m_hasRoi = false;
    bool m_hasPolygonRoi = false;
    bool m_hasCircleRoi = false;
    bool m_hasLineBandRoi = false;
    bool m_roiDrawingEnabled = false;
    bool m_polygonDrawingEnabled = false;
    bool m_circleDrawingEnabled = false;
    bool m_pointSelectionEnabled = false;
    bool m_lineBandDrawingEnabled = false;
    bool m_roiDrawing = false;
    bool m_roiTransforming = false;
    bool m_polygonDragging = false;
    bool m_circleDrawing = false;
    bool m_circleTransforming = false;
    bool m_lineBandDrawingLine = false;
    bool m_lineBandAdjustingWidth = false;
    bool m_lineBandTransforming = false;
    bool m_navigationEnabled = false;
    bool m_isFitToView = true;
    qreal m_viewScale = 1.0;
    bool m_panning = false;
    QPoint m_lastPanPosition;
    PolygonDrawingState m_polygonDrawingState = PolygonDrawingState::Idle;
    QPointF m_roiDrawStart;
    RoiEditorController m_roiEditor;
    QVector<QGraphicsItem *> m_roiHandleItems;
    QVector<QGraphicsItem *> m_circleHandleItems;
    QVector<QGraphicsItem *> m_overlayItems;
    QVector<QGraphicsItem *> m_lineBandItems;
    QVector<QGraphicsItem *> m_lineBandDraftItems;
    QGraphicsItem *m_lineBandWidthHandleItem = nullptr;
};

#endif // FRAME_FRAMEVIEWHELPER_H
