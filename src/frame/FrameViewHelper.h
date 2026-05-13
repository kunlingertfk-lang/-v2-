#ifndef FRAME_FRAMEVIEWHELPER_H
#define FRAME_FRAMEVIEWHELPER_H

#include <QObject>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QVector>

#include "toolcore/ToolOverlay.h"

class QEvent;
class QGraphicsItem;
class QGraphicsPixmapItem;
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

    QPointF viewToImage(const QPoint &viewPos) const;
    QRectF imageRectToNormalized(const QRectF &imageRect) const;
    QRectF normalizedToImageRect(const QRectF &normalized) const;
    QSize imageSize() const;

    void setRoiDrawingEnabled(bool enabled);
    bool isRoiDrawingEnabled() const;
    void setRoiRectNormalized(const QRectF &roi);
    QRectF roiRectNormalized() const;
    void clearRoi();

    void setToolOverlays(const QVector<ToolOverlay> &overlays);
    void clearToolOverlays();

signals:
    void roiChanged(const QRectF &roiNormalized);
    void roiSelectionRejected(const QRectF &imageRect);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QRectF clampedImageRect(const QRectF &rect) const;
    QPointF clampedImagePoint(const QPointF &point) const;
    QPointF clampedTextPosition(const QPointF &position, const QRectF &textBounds) const;
    QRectF validNormalizedRect(const QRectF &rect) const;
    void updateRoiItem();
    void updateDraftRoiItem(const QRectF &imageRect);
    void clearDraftRoiItem();
    void addOverlayItem(QGraphicsItem *item);
    void restoreImageSceneRect();

    QGraphicsView *m_view;
    QGraphicsScene *m_scene;
    QGraphicsPixmapItem *m_pixmapItem;
    QGraphicsRectItem *m_roiItem = nullptr;
    QGraphicsRectItem *m_draftRoiItem = nullptr;
    QImage m_lastImage;
    QRectF m_imageRect;
    QRectF m_roiNormalized;
    bool m_hasRoi = false;
    bool m_roiDrawingEnabled = false;
    bool m_roiDrawing = false;
    QPointF m_roiDrawStart;
    QVector<QGraphicsItem *> m_overlayItems;
};

#endif // FRAME_FRAMEVIEWHELPER_H
