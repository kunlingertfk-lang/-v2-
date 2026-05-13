#include "frame/FrameViewHelper.h"

#include <QBrush>
#include <QColor>
#include <QEvent>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QScrollBar>
#include <QSizePolicy>
#include <QtGlobal>
#include <QWidget>

#include <cmath>

namespace {

const int kMaxOverlayTextChars = 32;

QPen cosmeticPen(const QColor &color, const qreal width = 2.0)
{
    QPen pen(color, width);
    pen.setCosmetic(true);
    return pen;
}

QColor overlayColor(const ToolOverlay &overlay)
{
    if (overlay.label.compare(QStringLiteral("ROI"), Qt::CaseInsensitive) == 0)
        return QColor(255, 122, 0);
    if (overlay.type == ToolOverlayType::Text)
        return QColor(255, 255, 255);
    return QColor(0, 200, 120);
}

bool finiteValue(const qreal value)
{
    return std::isfinite(static_cast<double>(value));
}

bool finitePoint(const QPointF &point)
{
    return finiteValue(point.x()) && finiteValue(point.y());
}

bool finiteRect(const QRectF &rect)
{
    return finiteValue(rect.x()) &&
           finiteValue(rect.y()) &&
           finiteValue(rect.width()) &&
           finiteValue(rect.height());
}

QString overlayTextForDisplay(const QString &text)
{
    if (text.size() <= kMaxOverlayTextChars)
        return text;

    return text.left(kMaxOverlayTextChars - 3) + QStringLiteral("...");
}

} // namespace

FrameViewHelper::FrameViewHelper(QGraphicsView *view, QObject *parent)
    : QObject(parent)
    , m_view(view)
    , m_scene(new QGraphicsScene(this))
    , m_pixmapItem(nullptr)
{
    if (!m_view) {
        return;
    }

    m_pixmapItem = m_scene->addPixmap(QPixmap());
    m_pixmapItem->setZValue(0.0);

    m_roiItem = m_scene->addRect(QRectF(), cosmeticPen(QColor(255, 122, 0), 2.0));
    m_roiItem->setZValue(100.0);
    m_roiItem->hide();

    m_view->setScene(m_scene);
    m_view->setBackgroundBrush(QColor(0, 0, 0));
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setAlignment(Qt::AlignCenter);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform, true);
    m_view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_view->setMinimumSize(0, 0);
    m_view->viewport()->installEventFilter(this);
    m_view->viewport()->setMouseTracking(true);
}

void FrameViewHelper::setImage(const QImage &image)
{
    if (!m_view || !m_pixmapItem) {
        return;
    }

    if (image.isNull()) {
        clear();
        return;
    }

    m_lastImage = image;
    m_imageRect = QRectF(QPointF(0.0, 0.0), QSizeF(m_lastImage.size()));
    const QPixmap pixmap = QPixmap::fromImage(m_lastImage);
    m_pixmapItem->setPixmap(pixmap);
    m_scene->setSceneRect(m_imageRect);
    updateRoiItem();
    fitToView();
}

void FrameViewHelper::clear()
{
    m_lastImage = QImage();
    clearToolOverlays();
    clearDraftRoiItem();

    if (!m_view || !m_pixmapItem) {
        return;
    }

    if (m_view->viewport())
        m_view->viewport()->unsetCursor();
    m_pixmapItem->setPixmap(QPixmap());
    m_imageRect = QRectF();
    if (m_roiItem)
        m_roiItem->hide();
    m_scene->setSceneRect(QRectF());
    m_view->resetTransform();
    m_view->viewport()->update();
}

void FrameViewHelper::fitToView()
{
    if (!m_view || m_imageRect.isEmpty()) {
        return;
    }

    m_view->resetTransform();
    m_view->fitInView(m_imageRect, Qt::KeepAspectRatio);
}

QPointF FrameViewHelper::viewToImage(const QPoint &viewPos) const
{
    if (!m_view || m_lastImage.isNull())
        return QPointF();

    const QPointF scenePoint = m_view->mapToScene(viewPos);
    return clampedImagePoint(scenePoint);
}

QRectF FrameViewHelper::imageRectToNormalized(const QRectF &imageRect) const
{
    if (m_lastImage.isNull())
        return QRectF();

    const QRectF rect = clampedImageRect(imageRect.normalized());
    if (rect.width() <= 0.0 || rect.height() <= 0.0)
        return QRectF();

    return validNormalizedRect(QRectF(rect.x() / static_cast<double>(m_lastImage.width()),
                                      rect.y() / static_cast<double>(m_lastImage.height()),
                                      rect.width() / static_cast<double>(m_lastImage.width()),
                                      rect.height() / static_cast<double>(m_lastImage.height())));
}

QRectF FrameViewHelper::normalizedToImageRect(const QRectF &normalized) const
{
    if (m_lastImage.isNull())
        return QRectF();

    const QRectF rect = validNormalizedRect(normalized);
    return clampedImageRect(QRectF(rect.x() * m_lastImage.width(),
                                   rect.y() * m_lastImage.height(),
                                   rect.width() * m_lastImage.width(),
                                   rect.height() * m_lastImage.height()));
}

QSize FrameViewHelper::imageSize() const
{
    return m_lastImage.size();
}

void FrameViewHelper::setRoiDrawingEnabled(bool enabled)
{
    m_roiDrawingEnabled = enabled;
    if (m_view && m_view->viewport()) {
        if (enabled)
            m_view->viewport()->setCursor(Qt::CrossCursor);
        else
            m_view->viewport()->unsetCursor();
    }

    if (!enabled) {
        m_roiDrawing = false;
        clearDraftRoiItem();
    }
}

bool FrameViewHelper::isRoiDrawingEnabled() const
{
    return m_roiDrawingEnabled;
}

void FrameViewHelper::setRoiRectNormalized(const QRectF &roi)
{
    const QRectF validRoi = validNormalizedRect(roi);
    if (validRoi.width() <= 0.0 || validRoi.height() <= 0.0) {
        clearRoi();
        return;
    }

    m_roiNormalized = validRoi;
    m_hasRoi = true;
    updateRoiItem();
}

QRectF FrameViewHelper::roiRectNormalized() const
{
    return m_hasRoi ? m_roiNormalized : QRectF();
}

void FrameViewHelper::clearRoi()
{
    m_hasRoi = false;
    m_roiNormalized = QRectF();
    if (m_roiItem)
        m_roiItem->hide();
}

void FrameViewHelper::setToolOverlays(const QVector<ToolOverlay> &overlays)
{
    clearToolOverlays();
    if (!m_scene || m_imageRect.isEmpty())
        return;

    for (const ToolOverlay &overlay : overlays) {
        const QColor color = overlayColor(overlay);
        switch (overlay.type) {
        case ToolOverlayType::Rect: {
            if (!finiteRect(overlay.rect))
                break;

            const QRectF rect = overlay.rect.normalized().intersected(m_imageRect);
            if (rect.width() <= 0.0 || rect.height() <= 0.0)
                break;

            QGraphicsRectItem *item = m_scene->addRect(rect,
                                                       cosmeticPen(color, 2.0));
            item->setToolTip(overlay.label);
            addOverlayItem(item);
            break;
        }
        case ToolOverlayType::Line: {
            if (!finitePoint(overlay.p1) || !finitePoint(overlay.p2))
                break;

            QGraphicsLineItem *item = m_scene->addLine(QLineF(clampedImagePoint(overlay.p1),
                                                             clampedImagePoint(overlay.p2)),
                                                       cosmeticPen(color, 2.0));
            item->setToolTip(overlay.label);
            addOverlayItem(item);
            break;
        }
        case ToolOverlayType::Circle: {
            if (!finitePoint(overlay.center) || !finiteValue(overlay.radius) || overlay.radius <= 0.0)
                break;

            const QPointF center = clampedImagePoint(overlay.center);
            const QRectF clampedRect = QRectF(center.x() - overlay.radius,
                                              center.y() - overlay.radius,
                                              overlay.radius * 2.0,
                                              overlay.radius * 2.0)
                    .intersected(m_imageRect);
            if (clampedRect.width() <= 0.0 || clampedRect.height() <= 0.0)
                break;

            QGraphicsEllipseItem *item = m_scene->addEllipse(clampedRect, cosmeticPen(color, 2.0));
            item->setToolTip(overlay.label);
            addOverlayItem(item);
            break;
        }
        case ToolOverlayType::Polygon: {
            QPolygonF polygon;
            for (const QPointF &point : overlay.points) {
                if (!finitePoint(point)) {
                    polygon.clear();
                    break;
                }
                polygon << clampedImagePoint(point);
            }
            if (polygon.size() < 3)
                break;

            QGraphicsPolygonItem *item = m_scene->addPolygon(polygon, cosmeticPen(color, 2.0));
            item->setToolTip(overlay.label);
            addOverlayItem(item);
            break;
        }
        case ToolOverlayType::Text: {
            if (!finitePoint(overlay.p1))
                break;

            const QString fullText = overlay.text.isEmpty() ? overlay.label : overlay.text;
            const QString text = overlayTextForDisplay(fullText);
            QGraphicsSimpleTextItem *item = m_scene->addSimpleText(text);
            item->setBrush(QBrush(color));
            item->setPen(cosmeticPen(QColor(0, 0, 0), 1.0));
            item->setPos(clampedTextPosition(clampedImagePoint(overlay.p1), item->boundingRect()));
            item->setToolTip(fullText);
            item->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
            addOverlayItem(item);
            break;
        }
        case ToolOverlayType::Unknown:
        default:
            break;
        }
    }

    restoreImageSceneRect();
}

void FrameViewHelper::clearToolOverlays()
{
    if (!m_scene) {
        m_overlayItems.clear();
        return;
    }

    for (QGraphicsItem *item : m_overlayItems) {
        if (item) {
            m_scene->removeItem(item);
            delete item;
        }
    }
    m_overlayItems.clear();
    restoreImageSceneRect();
}

bool FrameViewHelper::eventFilter(QObject *obj, QEvent *event)
{
    if (m_view && obj == m_view->viewport() && event->type() == QEvent::Resize) {
        fitToView();
    }

    if (m_view && obj == m_view->viewport() && m_roiDrawingEnabled && !m_lastImage.isNull()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_roiDrawing = true;
                m_roiDrawStart = viewToImage(mouseEvent->pos());
                updateDraftRoiItem(QRectF(m_roiDrawStart, QSizeF()));
                return true;
            }
        }

        if (event->type() == QEvent::MouseMove && m_roiDrawing) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            updateDraftRoiItem(QRectF(m_roiDrawStart, viewToImage(mouseEvent->pos())).normalized());
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease && m_roiDrawing) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_roiDrawing = false;
                const QRectF imageRect = clampedImageRect(QRectF(m_roiDrawStart,
                                                                 viewToImage(mouseEvent->pos())).normalized());
                clearDraftRoiItem();
                if (imageRect.width() >= 2.0 && imageRect.height() >= 2.0) {
                    const QRectF normalized = imageRectToNormalized(imageRect);
                    setRoiRectNormalized(normalized);
                    emit roiChanged(m_roiNormalized);
                } else {
                    emit roiSelectionRejected(imageRect);
                }
                return true;
            }
        }
    }

    return QObject::eventFilter(obj, event);
}

QRectF FrameViewHelper::clampedImageRect(const QRectF &rect) const
{
    if (m_imageRect.isEmpty() || !finiteRect(rect))
        return QRectF();

    const QRectF normalized = rect.normalized();
    const double left = qBound(m_imageRect.left(), normalized.left(), m_imageRect.right());
    const double top = qBound(m_imageRect.top(), normalized.top(), m_imageRect.bottom());
    const double right = qBound(m_imageRect.left(), normalized.right(), m_imageRect.right());
    const double bottom = qBound(m_imageRect.top(), normalized.bottom(), m_imageRect.bottom());
    return QRectF(QPointF(left, top), QPointF(right, bottom)).normalized();
}

QPointF FrameViewHelper::clampedImagePoint(const QPointF &point) const
{
    if (m_imageRect.isEmpty() || !finitePoint(point))
        return QPointF();

    return QPointF(qBound(m_imageRect.left(), point.x(), m_imageRect.right()),
                   qBound(m_imageRect.top(), point.y(), m_imageRect.bottom()));
}

QPointF FrameViewHelper::clampedTextPosition(const QPointF &position, const QRectF &textBounds) const
{
    if (m_imageRect.isEmpty())
        return QPointF();

    QPointF clamped = clampedImagePoint(position);

    if (textBounds.width() > 0.0 && textBounds.width() < m_imageRect.width()) {
        clamped.setX(qBound(m_imageRect.left() - textBounds.left(),
                            clamped.x(),
                            m_imageRect.right() - textBounds.right()));
    } else {
        clamped.setX(m_imageRect.left() - textBounds.left());
    }

    if (textBounds.height() > 0.0 && textBounds.height() < m_imageRect.height()) {
        clamped.setY(qBound(m_imageRect.top() - textBounds.top(),
                            clamped.y(),
                            m_imageRect.bottom() - textBounds.bottom()));
    } else {
        clamped.setY(m_imageRect.top() - textBounds.top());
    }

    return clamped;
}

QRectF FrameViewHelper::validNormalizedRect(const QRectF &rect) const
{
    QRectF normalized = rect.normalized();
    const double left = qBound(0.0, normalized.left(), 1.0);
    const double top = qBound(0.0, normalized.top(), 1.0);
    const double right = qBound(0.0, normalized.right(), 1.0);
    const double bottom = qBound(0.0, normalized.bottom(), 1.0);
    return QRectF(QPointF(left, top), QPointF(right, bottom)).normalized();
}

void FrameViewHelper::updateRoiItem()
{
    if (!m_roiItem)
        return;

    if (!m_hasRoi || m_lastImage.isNull()) {
        m_roiItem->hide();
        return;
    }

    m_roiItem->setRect(normalizedToImageRect(m_roiNormalized));
    m_roiItem->show();
}

void FrameViewHelper::updateDraftRoiItem(const QRectF &imageRect)
{
    if (!m_scene)
        return;

    if (!m_draftRoiItem) {
        m_draftRoiItem = m_scene->addRect(QRectF(),
                                          cosmeticPen(QColor(255, 185, 90), 1.5));
        m_draftRoiItem->setZValue(101.0);
    }

    m_draftRoiItem->setRect(clampedImageRect(imageRect));
    m_draftRoiItem->show();
}

void FrameViewHelper::clearDraftRoiItem()
{
    if (!m_draftRoiItem || !m_scene)
        return;

    m_scene->removeItem(m_draftRoiItem);
    delete m_draftRoiItem;
    m_draftRoiItem = nullptr;
}

void FrameViewHelper::addOverlayItem(QGraphicsItem *item)
{
    if (!item)
        return;

    item->setZValue(110.0);
    m_overlayItems.append(item);
}

void FrameViewHelper::restoreImageSceneRect()
{
    if (m_scene)
        m_scene->setSceneRect(m_imageRect);
}
