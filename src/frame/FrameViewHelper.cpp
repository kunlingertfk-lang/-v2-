#include "frame/FrameViewHelper.h"

#include <QBrush>
#include <QColor>
#include <QEvent>
#include <QFrame>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsPolygonItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QLineF>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QScrollBar>
#include <QSizePolicy>
#include <QtGlobal>
#include <QWidget>
#include <QFont>

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
    const QString label = overlay.label.trimmed().toLower();
    if (label == QStringLiteral("roi") || label == QStringLiteral("detect_roi"))
        return QColor(255, 122, 0);
    if (label == QStringLiteral("template_roi"))
        return QColor(0, 170, 255);
    if (label == QStringLiteral("candidate_contour"))
        return QColor(255, 190, 0);
    if (label == QStringLiteral("contour_points") ||
        label == QStringLiteral("contour_line") ||
        label == QStringLiteral("contour_model_line") ||
        label == QStringLiteral("contour_model_points"))
        return QColor(0, 210, 255);
    if (label == QStringLiteral("line_band_center"))
        return QColor(0, 210, 255);
    if (label == QStringLiteral("line_band_boundary"))
        return QColor(255, 122, 0);
    if (label == QStringLiteral("measure_regions") ||
        label == QStringLiteral("measure_rectangle"))
        return QColor(255, 190, 0);
    if (label == QStringLiteral("sample_points") ||
        label == QStringLiteral("edge_points"))
        return QColor(255, 70, 70);
    if (label == QStringLiteral("fitted_line"))
        return QColor(0, 255, 130);
    if (label == QStringLiteral("blob_bbox"))
        return QColor(0, 200, 120);
    if (label == QStringLiteral("blob_center") ||
        label == QStringLiteral("circle_center"))
        return QColor(255, 70, 70);
    if (label == QStringLiteral("circle"))
        return QColor(0, 210, 255);
    if (label == QStringLiteral("blob_area_text") ||
        label == QStringLiteral("blob_count_text") ||
        label == QStringLiteral("circle_radius_text") ||
        label == QStringLiteral("circularity_text") ||
        label == QStringLiteral("circle_count_text") ||
        label == QStringLiteral("edge_count_text") ||
        label == QStringLiteral("line_result_text"))
        return QColor(255, 255, 255);
    if (label == QStringLiteral("color_result_text")) {
        const QString status = overlay.extra.value(QStringLiteral("status")).toString().trimmed().toUpper();
        if (status == QStringLiteral("OK"))
            return QColor(0, 210, 120);
        if (status == QStringLiteral("NG"))
            return QColor(255, 70, 70);
        if (status == QStringLiteral("MASKED"))
            return QColor(150, 90, 35);
        return QColor(255, 255, 255);
    }
    if (label == QStringLiteral("match_result") ||
        label == QStringLiteral("match_rect") ||
        label == QStringLiteral("match_bbox") ||
        label == QStringLiteral("match_center"))
        return QColor(0, 200, 120);
    if (overlay.type == ToolOverlayType::Text)
        return QColor(255, 255, 255);
    return QColor(0, 200, 120);
}

qreal overlayZValue(const ToolOverlay &overlay)
{
    const QString label = overlay.label.trimmed().toLower();
    if (label == QStringLiteral("template_roi"))
        return 104.0;
    if (label == QStringLiteral("roi") || label == QStringLiteral("detect_roi"))
        return 106.0;
    if (label == QStringLiteral("match_result") ||
        label == QStringLiteral("match_rect") ||
        label == QStringLiteral("match_bbox") ||
        label == QStringLiteral("match_center"))
        return 112.0;
    if (label == QStringLiteral("contour_points") ||
        label == QStringLiteral("contour_line") ||
        label == QStringLiteral("contour_model_line") ||
        label == QStringLiteral("contour_model_points"))
        return 114.0;
    if (label == QStringLiteral("line_band_center") || label == QStringLiteral("line_band_boundary"))
        return 108.0;
    if (label == QStringLiteral("measure_regions") ||
        label == QStringLiteral("measure_rectangle"))
        return 110.0;
    if (label == QStringLiteral("sample_points") ||
        label == QStringLiteral("edge_points") ||
        label == QStringLiteral("fitted_line"))
        return 114.0;
    if (label == QStringLiteral("blob_bbox") ||
        label == QStringLiteral("circle"))
        return 112.0;
    if (label == QStringLiteral("blob_center") ||
        label == QStringLiteral("circle_center"))
        return 114.0;
    if (label == QStringLiteral("blob_area_text") ||
        label == QStringLiteral("blob_count_text") ||
        label == QStringLiteral("circle_radius_text") ||
        label == QStringLiteral("circularity_text") ||
        label == QStringLiteral("circle_count_text") ||
        label == QStringLiteral("edge_count_text") ||
        label == QStringLiteral("line_result_text"))
        return 116.0;
    if (label == QStringLiteral("score_text") ||
        label == QStringLiteral("match_score_text") ||
        label == QStringLiteral("color_result_text") ||
        overlay.type == ToolOverlayType::Text)
        return 116.0;
    return 110.0;
}

QRectF rectFromOverlayExtra(const QJsonObject &extra)
{
    const QJsonObject json = extra.value(QStringLiteral("anchorRect")).toObject();
    if (json.isEmpty())
        return QRectF();

    return QRectF(json.value(QStringLiteral("x")).toDouble(),
                  json.value(QStringLiteral("y")).toDouble(),
                  json.value(QStringLiteral("width")).toDouble(),
                  json.value(QStringLiteral("height")).toDouble()).normalized();
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

    m_polygonItem = m_scene->addPolygon(QPolygonF(), cosmeticPen(QColor(0, 170, 255), 2.0));
    m_polygonItem->setZValue(102.0);
    m_polygonItem->hide();

    m_circleItem = m_scene->addEllipse(QRectF(), cosmeticPen(QColor(0, 170, 255), 2.0));
    m_circleItem->setZValue(102.0);
    m_circleItem->hide();

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
    m_view->viewport()->setFocusPolicy(Qt::StrongFocus);
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

    const QSize previousSize = m_lastImage.size();
    m_lastImage = image;
    m_imageRect = QRectF(QPointF(0.0, 0.0), QSizeF(m_lastImage.size()));
    const QPixmap pixmap = QPixmap::fromImage(m_lastImage);
    m_pixmapItem->setPixmap(pixmap);
    updateRoiItem();
    updatePolygonItem();
    updateCircleItem();
    updateLineBandItem();

    if (m_scene->sceneRect().size() != m_imageRect.size() || previousSize != m_lastImage.size()) {
        m_scene->setSceneRect(m_imageRect);
        fitToView();
    }
}

void FrameViewHelper::clear()
{
    m_lastImage = QImage();
    clearToolOverlays();
    clearDraftRoiItem();
    clearDraftPolygonItem();
    clearPolygonVertexItems();
    clearDraftCircleItem();
    clearLineBandItems(&m_lineBandDraftItems);
    clearLineBandItems(&m_lineBandItems);

    if (!m_view || !m_pixmapItem) {
        return;
    }

    if (m_view->viewport())
        m_view->viewport()->unsetCursor();
    m_pixmapItem->setPixmap(QPixmap());
    m_imageRect = QRectF();
    if (m_roiItem)
        m_roiItem->hide();
    if (m_polygonItem)
        m_polygonItem->hide();
    clearPolygonVertexItems();
    if (m_circleItem)
        m_circleItem->hide();
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

bool FrameViewHelper::hasImage() const
{
    return !m_lastImage.isNull();
}

QPointF FrameViewHelper::viewToImage(const QPoint &viewPos) const
{
    QPointF imagePoint;
    return viewPosToImagePoint(viewPos, &imagePoint) ? imagePoint : QPointF();
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
    if (enabled && m_polygonDrawingEnabled)
        setPolygonDrawingEnabled(false);
    if (enabled && m_circleDrawingEnabled)
        setCircleDrawingEnabled(false);
    if (enabled && m_lineBandDrawingEnabled)
        setLineBandDrawingEnabled(false);

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

void FrameViewHelper::setPolygonDrawingEnabled(bool enabled)
{
    if (enabled && m_roiDrawingEnabled)
        setRoiDrawingEnabled(false);
    if (enabled && m_circleDrawingEnabled)
        setCircleDrawingEnabled(false);
    if (enabled && m_lineBandDrawingEnabled)
        setLineBandDrawingEnabled(false);

    m_polygonDrawingEnabled = enabled;
    m_polygonDrawingState = enabled
            ? PolygonDrawingState::DrawingPolygon
            : PolygonDrawingState::Idle;
    m_polygonHoverPointValid = false;
    if (enabled)
        m_draftPolygonImagePoints.clear();
    if (m_view && m_view->viewport()) {
        if (enabled) {
            m_view->viewport()->setCursor(Qt::CrossCursor);
            m_view->viewport()->setFocus();
        } else {
            m_view->viewport()->unsetCursor();
        }
    }

    if (!enabled) {
        m_draftPolygonImagePoints.clear();
        clearDraftPolygonItem();
    }
}

bool FrameViewHelper::isPolygonDrawingEnabled() const
{
    return m_polygonDrawingEnabled;
}

bool FrameViewHelper::finishPolygonDrawing()
{
    if (m_polygonDrawingState != PolygonDrawingState::DrawingPolygon ||
        m_draftPolygonImagePoints.isEmpty()) {
        return m_hasPolygonRoi && m_polygonNormalized.size() >= 3;
    }

    if (m_draftPolygonImagePoints.size() < 3) {
        emit polygonSelectionRejected(m_draftPolygonImagePoints.size());
        return false;
    }

    return completeDraftPolygon();
}

void FrameViewHelper::setPolygonRoiNormalized(const QVector<QPointF> &points)
{
    const QVector<QPointF> validPoints = validNormalizedPolygon(points);
    if (validPoints.size() < 3) {
        clearPolygonRoi();
        return;
    }

    m_polygonNormalized = validPoints;
    m_hasPolygonRoi = true;
    updatePolygonItem();
}

QVector<QPointF> FrameViewHelper::polygonRoiNormalized() const
{
    return m_hasPolygonRoi ? m_polygonNormalized : QVector<QPointF>();
}

void FrameViewHelper::clearPolygonRoi()
{
    m_hasPolygonRoi = false;
    m_polygonNormalized.clear();
    m_polygonDragging = false;
    m_draggingPolygonVertexIndex = -1;
    m_polygonDragStartNormalized.clear();
    if (m_polygonItem)
        m_polygonItem->hide();
    clearPolygonVertexItems();
}

void FrameViewHelper::setCircleDrawingEnabled(bool enabled)
{
    if (enabled && m_roiDrawingEnabled)
        setRoiDrawingEnabled(false);
    if (enabled && m_polygonDrawingEnabled)
        setPolygonDrawingEnabled(false);
    if (enabled && m_lineBandDrawingEnabled)
        setLineBandDrawingEnabled(false);

    m_circleDrawingEnabled = enabled;
    if (m_view && m_view->viewport()) {
        if (enabled) {
            m_view->viewport()->setCursor(Qt::CrossCursor);
            m_view->viewport()->setFocus();
        } else {
            m_view->viewport()->unsetCursor();
        }
    }

    if (!enabled) {
        m_circleDrawing = false;
        clearDraftCircleItem();
    }
}

bool FrameViewHelper::isCircleDrawingEnabled() const
{
    return m_circleDrawingEnabled;
}

void FrameViewHelper::setCircleRoiNormalized(const CircleRoi &roi)
{
    const CircleRoi validRoi = validCircleRoi(roi);
    if (!validRoi.valid) {
        clearCircleRoi();
        return;
    }

    m_circleRoi = validRoi;
    m_hasCircleRoi = true;
    updateCircleItem();
}

CircleRoi FrameViewHelper::circleRoiNormalized() const
{
    return m_hasCircleRoi ? m_circleRoi : CircleRoi();
}

QRectF FrameViewHelper::circleBoundingRectNormalized(const CircleRoi &roi) const
{
    return validCircleRoi(roi).boundingRectNormalized;
}

void FrameViewHelper::clearCircleRoi()
{
    m_hasCircleRoi = false;
    m_circleRoi = CircleRoi();
    if (m_circleItem)
        m_circleItem->hide();
}

void FrameViewHelper::setLineBandDrawingEnabled(bool enabled)
{
    if (enabled && m_roiDrawingEnabled)
        setRoiDrawingEnabled(false);
    if (enabled && m_polygonDrawingEnabled)
        setPolygonDrawingEnabled(false);
    if (enabled && m_circleDrawingEnabled)
        setCircleDrawingEnabled(false);

    m_lineBandDrawingEnabled = enabled;
    if (m_view && m_view->viewport()) {
        if (enabled)
            m_view->viewport()->setCursor(Qt::CrossCursor);
        else
            m_view->viewport()->unsetCursor();
    }

    if (!enabled) {
        m_lineBandDrawingLine = false;
        m_lineBandAdjustingWidth = false;
        clearLineBandItems(&m_lineBandDraftItems);
    }
}

bool FrameViewHelper::isLineBandDrawingEnabled() const
{
    return m_lineBandDrawingEnabled;
}

void FrameViewHelper::setLineBandRoiNormalized(const LineBandRoi &roi)
{
    const LineBandRoi validRoi = validLineBand(roi);
    if (!validRoi.valid) {
        clearLineBandRoi();
        return;
    }

    m_lineBandRoi = validRoi;
    m_hasLineBandRoi = true;
    updateLineBandItem();
}

LineBandRoi FrameViewHelper::lineBandRoiNormalized() const
{
    return m_hasLineBandRoi ? m_lineBandRoi : LineBandRoi();
}

QRectF FrameViewHelper::lineBandBoundingRectNormalized(const LineBandRoi &roi) const
{
    if (!isValidLineBand(roi))
        return QRectF();

    QVector<QPointF> points = lineBandPolygonImagePoints(roi);
    if (points.isEmpty()) {
        const QPointF p1 = roi.p1Normalized;
        const QPointF p2 = roi.p2Normalized;
        const double pad = qBound(0.001, roi.widthNormalized / 2.0, 0.5);
        const QRectF rect(QPointF(qMin(p1.x(), p2.x()) - pad, qMin(p1.y(), p2.y()) - pad),
                          QPointF(qMax(p1.x(), p2.x()) + pad, qMax(p1.y(), p2.y()) + pad));
        return validNormalizedRect(rect);
    }

    const QPointF first = imagePointToNormalized(points.first());
    double left = first.x();
    double top = first.y();
    double right = left;
    double bottom = top;
    for (const QPointF &point : points) {
        const QPointF normalized = imagePointToNormalized(point);
        left = qMin(left, normalized.x());
        top = qMin(top, normalized.y());
        right = qMax(right, normalized.x());
        bottom = qMax(bottom, normalized.y());
    }
    return validNormalizedRect(QRectF(QPointF(left, top), QPointF(right, bottom)));
}

void FrameViewHelper::clearLineBandRoi()
{
    m_hasLineBandRoi = false;
    m_lineBandRoi = LineBandRoi();
    clearLineBandItems(&m_lineBandItems);
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
            addOverlayItem(item, overlayZValue(overlay));
            break;
        }
        case ToolOverlayType::Line: {
            if (!finitePoint(overlay.p1) || !finitePoint(overlay.p2))
                break;

            QGraphicsLineItem *item = m_scene->addLine(QLineF(clampedImagePoint(overlay.p1),
                                                             clampedImagePoint(overlay.p2)),
                                                       cosmeticPen(color, 2.0));
            item->setToolTip(overlay.label);
            addOverlayItem(item, overlayZValue(overlay));
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
            addOverlayItem(item, overlayZValue(overlay));
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
            addOverlayItem(item, overlayZValue(overlay));
            break;
        }
        case ToolOverlayType::Text: {
            if (!finitePoint(overlay.p1))
                break;

            const QString fullText = overlay.text.isEmpty() ? overlay.label : overlay.text;
            const QString text = overlayTextForDisplay(fullText);
            QGraphicsSimpleTextItem *item = m_scene->addSimpleText(text);
            if (overlay.label.trimmed().compare(QStringLiteral("color_result_text"), Qt::CaseInsensitive) == 0) {
                QFont font = item->font();
                font.setPointSize(18);
                font.setBold(true);
                item->setFont(font);
            }
            item->setBrush(QBrush(color));
            item->setPen(cosmeticPen(QColor(0, 0, 0), 1.0));
            QPointF textPosition = clampedImagePoint(overlay.p1);
            const QRectF anchorRect = rectFromOverlayExtra(overlay.extra).intersected(m_imageRect);
            if (overlay.label.trimmed().compare(QStringLiteral("color_result_text"), Qt::CaseInsensitive) == 0 &&
                anchorRect.width() > 0.0 && anchorRect.height() > 0.0) {
                const QRectF bounds = item->boundingRect();
                const qreal margin = 8.0;
                if (anchorRect.width() >= bounds.width() + margin * 2.0 &&
                    anchorRect.height() >= bounds.height() + margin * 2.0) {
                    textPosition = QPointF(anchorRect.center().x() - bounds.width() / 2.0,
                                           anchorRect.center().y() - bounds.height() / 2.0);
                } else if (anchorRect.top() - bounds.height() - margin >= m_imageRect.top()) {
                    textPosition = QPointF(anchorRect.left(), anchorRect.top() - bounds.height() - margin);
                } else if (anchorRect.bottom() + bounds.height() + margin <= m_imageRect.bottom()) {
                    textPosition = QPointF(anchorRect.left(), anchorRect.bottom() + margin);
                } else {
                    textPosition = QPointF(anchorRect.right() + margin, anchorRect.top());
                }
            }
            item->setPos(clampedTextPosition(textPosition, item->boundingRect()));
            item->setToolTip(fullText);
            item->setFlag(QGraphicsItem::ItemIgnoresTransformations, true);
            addOverlayItem(item, overlayZValue(overlay));
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

    if (m_view && obj == m_view->viewport() && m_lineBandDrawingEnabled && !m_lastImage.isNull()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                if (m_lineBandAdjustingWidth) {
                    const LineBandRoi roi = imageLineBandToNormalized(m_lineBandDraftP1,
                                                                       m_lineBandDraftP2,
                                                                       m_lineBandDraftWidthPixels);
                    if (!roi.valid) {
                        emit lineBandSelectionRejected();
                        return true;
                    }
                    setLineBandRoiNormalized(roi);
                    clearLineBandItems(&m_lineBandDraftItems);
                    m_lineBandAdjustingWidth = false;
                    emit lineBandChanged(m_lineBandRoi);
                    return true;
                }

                m_lineBandDrawingLine = true;
                m_lineBandDraftP1 = viewToImage(mouseEvent->pos());
                m_lineBandDraftP2 = m_lineBandDraftP1;
                m_lineBandDraftWidthPixels = 24.0;
                updateDraftLineBandItem();
                return true;
            }
        }

        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            const QPointF imagePoint = viewToImage(mouseEvent->pos());
            if (m_lineBandDrawingLine) {
                m_lineBandDraftP2 = imagePoint;
                updateDraftLineBandItem();
                return true;
            }
            if (m_lineBandAdjustingWidth) {
                const QLineF line(m_lineBandDraftP1, m_lineBandDraftP2);
                const double length = line.length();
                if (length > 0.001) {
                    const double distance = std::abs((imagePoint.x() - m_lineBandDraftP1.x()) *
                                                     (m_lineBandDraftP2.y() - m_lineBandDraftP1.y()) -
                                                     (imagePoint.y() - m_lineBandDraftP1.y()) *
                                                     (m_lineBandDraftP2.x() - m_lineBandDraftP1.x())) / length;
                    m_lineBandDraftWidthPixels = qBound(4.0, distance * 2.0, qMax(m_imageRect.width(), m_imageRect.height()));
                    updateDraftLineBandItem();
                }
                return true;
            }
        }

        if (event->type() == QEvent::MouseButtonRelease && m_lineBandDrawingLine) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_lineBandDrawingLine = false;
                m_lineBandDraftP2 = viewToImage(mouseEvent->pos());
                if (QLineF(m_lineBandDraftP1, m_lineBandDraftP2).length() < 2.0) {
                    clearLineBandItems(&m_lineBandDraftItems);
                    emit lineBandSelectionRejected();
                    return true;
                }
                m_lineBandDraftWidthPixels = qBound(12.0,
                                                    QLineF(m_lineBandDraftP1, m_lineBandDraftP2).length() / 6.0,
                                                    80.0);
                m_lineBandAdjustingWidth = true;
                updateDraftLineBandItem();
                return true;
            }
        }
    }

    if (m_view && obj == m_view->viewport() && m_circleDrawingEnabled && !m_lastImage.isNull()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QPointF imagePoint;
                if (!viewPosToImagePoint(mouseEvent->pos(), &imagePoint))
                    return true;

                m_circleDrawing = true;
                m_circleDraftCenter = imagePoint;
                m_circleDraftRadiusPixels = 0.0;
                updateDraftCircleItem();
                return true;
            }
        }

        if (event->type() == QEvent::MouseMove && m_circleDrawing) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            QPointF imagePoint;
            if (!viewPosToImagePoint(mouseEvent->pos(), &imagePoint))
                return true;

            m_circleDraftRadiusPixels = QLineF(m_circleDraftCenter, imagePoint).length();
            updateDraftCircleItem();
            return true;
        }

        if (event->type() == QEvent::MouseButtonRelease && m_circleDrawing) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QPointF imagePoint;
                if (!viewPosToImagePoint(mouseEvent->pos(), &imagePoint))
                    return true;

                m_circleDrawing = false;
                m_circleDraftRadiusPixels = QLineF(m_circleDraftCenter, imagePoint).length();
                clearDraftCircleItem();
                if (m_circleDraftRadiusPixels < 2.0) {
                    emit circleSelectionRejected();
                    return true;
                }

                const CircleRoi roi = imageCircleToNormalized(m_circleDraftCenter,
                                                              m_circleDraftRadiusPixels);
                if (!roi.valid) {
                    emit circleSelectionRejected();
                    return true;
                }

                setCircleRoiNormalized(roi);
                emit circleChanged(m_circleRoi);
                return true;
            }
        }
    }

    if (m_view && obj == m_view->viewport() && !m_roiDrawingEnabled && !m_polygonDrawingEnabled &&
        !m_circleDrawingEnabled && !m_lineBandDrawingEnabled &&
        m_hasPolygonRoi && m_polygonNormalized.size() >= 3 && !m_lastImage.isNull()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QPointF imagePoint;
                if (!viewPosToImagePoint(mouseEvent->pos(), &imagePoint))
                    return false;

                const int vertexIndex = polygonVertexIndexAt(imagePoint);
                if (vertexIndex >= 0) {
                    m_draggingPolygonVertexIndex = vertexIndex;
                    return true;
                }

                if (polygonContainsImagePoint(imagePoint)) {
                    m_polygonDragging = true;
                    m_polygonDragStartImagePoint = imagePoint;
                    m_polygonDragStartNormalized = m_polygonNormalized;
                    return true;
                }
            }
        }

        if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (m_draggingPolygonVertexIndex >= 0 && (mouseEvent->buttons() & Qt::LeftButton)) {
                QPointF imagePoint;
                if (!viewPosToImagePoint(mouseEvent->pos(), &imagePoint))
                    return true;

                QVector<QPointF> points = m_polygonNormalized;
                if (m_draggingPolygonVertexIndex < points.size()) {
                    points[m_draggingPolygonVertexIndex] = imagePointToNormalized(imagePoint);
                    setPolygonRoiNormalized(points);
                    emit polygonChanged(m_polygonNormalized);
                }
                return true;
            }

            if (m_polygonDragging && (mouseEvent->buttons() & Qt::LeftButton)) {
                QPointF imagePoint;
                if (!viewPosToImagePoint(mouseEvent->pos(), &imagePoint))
                    return true;

                setPolygonRoiNormalized(translatedPolygonNormalized(imagePoint - m_polygonDragStartImagePoint));
                emit polygonChanged(m_polygonNormalized);
                return true;
            }
        }

        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton &&
                (m_draggingPolygonVertexIndex >= 0 || m_polygonDragging)) {
                m_draggingPolygonVertexIndex = -1;
                m_polygonDragging = false;
                m_polygonDragStartNormalized.clear();
                emit polygonChanged(m_polygonNormalized);
                return true;
            }
        }
    }

    if (m_view && obj == m_view->viewport() && m_polygonDrawingEnabled && !m_lastImage.isNull()) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                m_draftPolygonImagePoints.clear();
                m_polygonHoverPointValid = false;
                clearDraftPolygonItem();
                setPolygonDrawingEnabled(false);
                return true;
            }
        }

        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                QPointF imagePoint;
                if (!viewPosToImagePoint(mouseEvent->pos(), &imagePoint))
                    return true;

                if (m_draftPolygonImagePoints.size() >= 3 &&
                    QLineF(imagePoint, m_draftPolygonImagePoints.first()).length() <= polygonCloseThresholdPixels()) {
                    completeDraftPolygon();
                    return true;
                }

                m_draftPolygonImagePoints.append(imagePoint);
                m_polygonHoverPointValid = false;
                updateDraftPolygonItem();
                return true;
            }

            if (mouseEvent->button() == Qt::RightButton) {
                if (m_draftPolygonImagePoints.isEmpty()) {
                    setPolygonDrawingEnabled(false);
                    return true;
                }

                m_draftPolygonImagePoints.removeLast();
                m_polygonHoverPointValid = false;
                if (m_draftPolygonImagePoints.isEmpty()) {
                    clearDraftPolygonItem();
                } else {
                    updateDraftPolygonItem();
                }
                return true;
            }
        }

        if (event->type() == QEvent::MouseMove && !m_draftPolygonImagePoints.isEmpty()) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            QPointF imagePoint;
            if (viewPosToImagePoint(mouseEvent->pos(), &imagePoint)) {
                m_polygonHoverPoint = imagePoint;
                m_polygonHoverPointValid = true;
            } else {
                m_polygonHoverPointValid = false;
            }
            updateDraftPolygonItem();
            return true;
        }

        if (event->type() == QEvent::MouseButtonDblClick) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                if (m_draftPolygonImagePoints.size() >= 3) {
                    completeDraftPolygon();
                } else {
                    emit polygonSelectionRejected(m_draftPolygonImagePoints.size());
                }
                return true;
            }
        }
    }

    if (m_view && obj == m_view->viewport() && m_polygonDrawingEnabled && m_lastImage.isNull()) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                setPolygonDrawingEnabled(false);
                return true;
            }
        }
    }

    if (m_view && obj == m_view->viewport() && m_circleDrawingEnabled && m_lastImage.isNull()) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                setCircleDrawingEnabled(false);
                return true;
            }
        }
    }

    if (m_view && obj == m_view->viewport() && m_circleDrawingEnabled && !m_lastImage.isNull()) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                setCircleDrawingEnabled(false);
                return true;
            }
        }
    }

    if (m_view && obj == m_view->viewport() && m_polygonDrawingEnabled && !m_lastImage.isNull()) {
        if (event->type() == QEvent::MouseMove && m_draftPolygonImagePoints.isEmpty()) {
            m_polygonHoverPointValid = false;
            return true;
        }
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

bool FrameViewHelper::viewPosToImagePoint(const QPoint &viewPos, QPointF *imagePoint) const
{
    if (!imagePoint || !m_view || m_lastImage.isNull() || m_imageRect.isEmpty())
        return false;

    const QPointF scenePoint = m_view->mapToScene(viewPos);
    if (!finitePoint(scenePoint))
        return false;

    const QPointF clamped = clampedImagePoint(scenePoint);
    if (!finitePoint(clamped))
        return false;

    *imagePoint = clamped;
    return true;
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

QPointF FrameViewHelper::imagePointToNormalized(const QPointF &point) const
{
    if (m_lastImage.isNull())
        return QPointF();

    const QPointF clamped = clampedImagePoint(point);
    return QPointF(clamped.x() / static_cast<double>(m_lastImage.width()),
                   clamped.y() / static_cast<double>(m_lastImage.height()));
}

QPointF FrameViewHelper::normalizedToImagePoint(const QPointF &point) const
{
    if (m_lastImage.isNull())
        return QPointF();

    const QPointF normalized(qBound(0.0, point.x(), 1.0),
                             qBound(0.0, point.y(), 1.0));
    return clampedImagePoint(QPointF(normalized.x() * m_lastImage.width(),
                                     normalized.y() * m_lastImage.height()));
}

QVector<QPointF> FrameViewHelper::validNormalizedPolygon(const QVector<QPointF> &points) const
{
    QVector<QPointF> validPoints;
    validPoints.reserve(points.size());
    for (const QPointF &point : points) {
        if (!finitePoint(point))
            continue;
        validPoints.append(QPointF(qBound(0.0, point.x(), 1.0),
                                   qBound(0.0, point.y(), 1.0)));
    }
    return validPoints;
}

bool FrameViewHelper::completeDraftPolygon()
{
    if (m_draftPolygonImagePoints.size() < 3) {
        emit polygonSelectionRejected(m_draftPolygonImagePoints.size());
        return false;
    }

    QVector<QPointF> normalizedPoints;
    normalizedPoints.reserve(m_draftPolygonImagePoints.size());
    for (const QPointF &point : qAsConst(m_draftPolygonImagePoints))
        normalizedPoints.append(imagePointToNormalized(point));

    setPolygonRoiNormalized(normalizedPoints);
    m_draftPolygonImagePoints.clear();
    m_polygonHoverPointValid = false;
    clearDraftPolygonItem();
    m_polygonDrawingState = PolygonDrawingState::CompletedPolygon;
    m_polygonDrawingEnabled = false;
    if (m_view && m_view->viewport())
        m_view->viewport()->unsetCursor();

    emit polygonChanged(m_polygonNormalized);
    return true;
}

double FrameViewHelper::polygonCloseThresholdPixels() const
{
    return 12.0;
}

int FrameViewHelper::polygonVertexIndexAt(const QPointF &imagePoint) const
{
    if (m_lastImage.isNull() || m_polygonNormalized.isEmpty())
        return -1;

    const double hitRadius = 9.0;
    for (int index = 0; index < m_polygonNormalized.size(); ++index) {
        if (QLineF(imagePoint, normalizedToImagePoint(m_polygonNormalized.at(index))).length() <= hitRadius)
            return index;
    }
    return -1;
}

bool FrameViewHelper::polygonContainsImagePoint(const QPointF &imagePoint) const
{
    if (m_polygonNormalized.size() < 3)
        return false;

    QPolygonF polygon;
    for (const QPointF &point : qAsConst(m_polygonNormalized))
        polygon << normalizedToImagePoint(point);
    return polygon.containsPoint(imagePoint, Qt::OddEvenFill);
}

QVector<QPointF> FrameViewHelper::translatedPolygonNormalized(const QPointF &deltaImage) const
{
    QVector<QPointF> translated;
    if (m_lastImage.isNull() || m_polygonDragStartNormalized.isEmpty())
        return translated;

    translated.reserve(m_polygonDragStartNormalized.size());
    for (const QPointF &point : qAsConst(m_polygonDragStartNormalized))
        translated.append(imagePointToNormalized(normalizedToImagePoint(point) + deltaImage));
    return translated;
}

bool FrameViewHelper::isValidCircleRoi(const CircleRoi &roi) const
{
    if (!finitePoint(roi.centerNormalized) || !finiteValue(roi.radiusNormalized))
        return false;
    return roi.radiusNormalized > 0.0;
}

CircleRoi FrameViewHelper::validCircleRoi(const CircleRoi &roi) const
{
    CircleRoi validRoi;
    if (!isValidCircleRoi(roi))
        return validRoi;

    validRoi.centerNormalized = QPointF(qBound(0.0, roi.centerNormalized.x(), 1.0),
                                        qBound(0.0, roi.centerNormalized.y(), 1.0));
    validRoi.radiusNormalized = qBound(0.0, roi.radiusNormalized, 1.0);
    if (roi.boundingRectNormalized.isValid()) {
        validRoi.boundingRectNormalized = validNormalizedRect(roi.boundingRectNormalized);
    } else {
        const double maxDimension = m_lastImage.isNull()
                ? 1.0
                : static_cast<double>(qMax(m_lastImage.width(), m_lastImage.height()));
        const double xRadius = m_lastImage.isNull() || m_lastImage.width() <= 0
                ? validRoi.radiusNormalized
                : validRoi.radiusNormalized * maxDimension / static_cast<double>(m_lastImage.width());
        const double yRadius = m_lastImage.isNull() || m_lastImage.height() <= 0
                ? validRoi.radiusNormalized
                : validRoi.radiusNormalized * maxDimension / static_cast<double>(m_lastImage.height());
        validRoi.boundingRectNormalized = validNormalizedRect(
                    QRectF(validRoi.centerNormalized.x() - xRadius,
                           validRoi.centerNormalized.y() - yRadius,
                           xRadius * 2.0,
                           yRadius * 2.0));
    }
    if (validRoi.boundingRectNormalized.width() <= 0.0 ||
        validRoi.boundingRectNormalized.height() <= 0.0) {
        validRoi.valid = false;
        return validRoi;
    }

    validRoi.valid = true;
    return validRoi;
}

CircleRoi FrameViewHelper::imageCircleToNormalized(const QPointF &center, double radiusPixels) const
{
    CircleRoi roi;
    if (m_lastImage.isNull() || radiusPixels <= 0.0)
        return roi;

    const double maxDimension = static_cast<double>(qMax(m_lastImage.width(), m_lastImage.height()));
    if (maxDimension <= 0.0)
        return roi;

    roi.centerNormalized = imagePointToNormalized(center);
    roi.radiusNormalized = radiusPixels / maxDimension;
    roi.boundingRectNormalized = imageRectToNormalized(QRectF(center.x() - radiusPixels,
                                                             center.y() - radiusPixels,
                                                             radiusPixels * 2.0,
                                                             radiusPixels * 2.0));
    roi.valid = roi.radiusNormalized > 0.0 &&
            roi.boundingRectNormalized.width() > 0.0 &&
            roi.boundingRectNormalized.height() > 0.0;
    return roi;
}

QRectF FrameViewHelper::circleBoundingRectImage(const CircleRoi &roi) const
{
    if (m_lastImage.isNull() || !isValidCircleRoi(roi))
        return QRectF();

    const QPointF center = normalizedToImagePoint(roi.centerNormalized);
    const double maxDimension = static_cast<double>(qMax(m_lastImage.width(), m_lastImage.height()));
    const double radius = roi.radiusNormalized * maxDimension;
    return QRectF(center.x() - radius,
                  center.y() - radius,
                  radius * 2.0,
                  radius * 2.0);
}

bool FrameViewHelper::isValidLineBand(const LineBandRoi &roi) const
{
    if (!finitePoint(roi.p1Normalized) || !finitePoint(roi.p2Normalized))
        return false;
    if (!finiteValue(roi.widthNormalized) || roi.widthNormalized <= 0.0)
        return false;

    const QPointF p1(qBound(0.0, roi.p1Normalized.x(), 1.0),
                     qBound(0.0, roi.p1Normalized.y(), 1.0));
    const QPointF p2(qBound(0.0, roi.p2Normalized.x(), 1.0),
                     qBound(0.0, roi.p2Normalized.y(), 1.0));
    return QLineF(p1, p2).length() > 0.001;
}

LineBandRoi FrameViewHelper::validLineBand(const LineBandRoi &roi) const
{
    LineBandRoi validRoi;
    validRoi.p1Normalized = QPointF(qBound(0.0, roi.p1Normalized.x(), 1.0),
                                    qBound(0.0, roi.p1Normalized.y(), 1.0));
    validRoi.p2Normalized = QPointF(qBound(0.0, roi.p2Normalized.x(), 1.0),
                                    qBound(0.0, roi.p2Normalized.y(), 1.0));
    validRoi.widthNormalized = qBound(0.001, roi.widthNormalized, 1.0);
    validRoi.valid = isValidLineBand(validRoi);
    return validRoi;
}

LineBandRoi FrameViewHelper::imageLineBandToNormalized(const QPointF &p1,
                                                       const QPointF &p2,
                                                       double widthPixels) const
{
    LineBandRoi roi;
    if (m_lastImage.isNull())
        return roi;

    const double maxDimension = qMax(1, qMax(m_lastImage.width(), m_lastImage.height()));
    roi.p1Normalized = imagePointToNormalized(p1);
    roi.p2Normalized = imagePointToNormalized(p2);
    roi.widthNormalized = qBound(0.001, widthPixels / maxDimension, 1.0);
    roi.valid = isValidLineBand(roi);
    return roi;
}

QVector<QPointF> FrameViewHelper::lineBandPolygonImagePoints(const LineBandRoi &roi) const
{
    QVector<QPointF> points;
    if (m_lastImage.isNull() || !isValidLineBand(roi))
        return points;

    const QPointF p1 = normalizedToImagePoint(roi.p1Normalized);
    const QPointF p2 = normalizedToImagePoint(roi.p2Normalized);
    const QLineF line(p1, p2);
    const double length = line.length();
    if (length <= 0.001)
        return points;

    const double widthPixels = roi.widthNormalized *
            static_cast<double>(qMax(m_lastImage.width(), m_lastImage.height()));
    const double halfWidth = qMax(1.0, widthPixels / 2.0);
    const double nx = -(p2.y() - p1.y()) / length;
    const double ny = (p2.x() - p1.x()) / length;
    const QPointF offset(nx * halfWidth, ny * halfWidth);

    points.reserve(4);
    points << clampedImagePoint(p1 + offset)
           << clampedImagePoint(p2 + offset)
           << clampedImagePoint(p2 - offset)
           << clampedImagePoint(p1 - offset);
    return points;
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

void FrameViewHelper::updatePolygonItem()
{
    if (!m_polygonItem)
        return;

    if (!m_hasPolygonRoi || m_lastImage.isNull() || m_polygonNormalized.size() < 3) {
        m_polygonItem->hide();
        clearPolygonVertexItems();
        return;
    }

    QPolygonF polygon;
    for (const QPointF &point : qAsConst(m_polygonNormalized))
        polygon << normalizedToImagePoint(point);

    m_polygonItem->setPolygon(polygon);
    m_polygonItem->show();
    updatePolygonVertexItems();
}

void FrameViewHelper::updatePolygonVertexItems()
{
    clearPolygonVertexItems();
    if (!m_scene || !m_hasPolygonRoi || m_lastImage.isNull() || m_polygonNormalized.size() < 3)
        return;

    const double side = 10.0;
    for (const QPointF &point : qAsConst(m_polygonNormalized)) {
        const QPointF imagePoint = normalizedToImagePoint(point);
        QGraphicsRectItem *item = m_scene->addRect(QRectF(imagePoint.x() - side / 2.0,
                                                          imagePoint.y() - side / 2.0,
                                                          side,
                                                          side),
                                                   cosmeticPen(QColor(0, 210, 255, 160), 1.2),
                                                   QBrush(QColor(0, 210, 255, 35)));
        item->setZValue(105.0);
        m_polygonVertexItems.append(item);
    }
}

void FrameViewHelper::clearPolygonVertexItems()
{
    if (!m_scene) {
        m_polygonVertexItems.clear();
        return;
    }

    for (QGraphicsItem *item : m_polygonVertexItems) {
        if (item) {
            m_scene->removeItem(item);
            delete item;
        }
    }
    m_polygonVertexItems.clear();
}

void FrameViewHelper::updateCircleItem()
{
    if (!m_circleItem)
        return;

    if (!m_hasCircleRoi || m_lastImage.isNull() || !isValidCircleRoi(m_circleRoi)) {
        m_circleItem->hide();
        return;
    }

    const QRectF circleRect = circleBoundingRectImage(m_circleRoi);
    if (circleRect.width() <= 0.0 || circleRect.height() <= 0.0) {
        m_circleItem->hide();
        return;
    }

    m_circleItem->setRect(circleRect);
    m_circleItem->show();
}

void FrameViewHelper::updateLineBandItem()
{
    clearLineBandItems(&m_lineBandItems);
    if (!m_hasLineBandRoi || m_lastImage.isNull() || !isValidLineBand(m_lineBandRoi))
        return;

    addLineBandItems(m_lineBandRoi, &m_lineBandItems, 104.0);
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

void FrameViewHelper::updateDraftPolygonItem()
{
    if (!m_scene)
        return;

    if (m_draftPolygonImagePoints.isEmpty()) {
        clearDraftPolygonItem();
        return;
    }

    if (!m_draftPolygonItem) {
        m_draftPolygonItem = m_scene->addPath(QPainterPath(),
                                              cosmeticPen(QColor(0, 210, 255), 1.5));
        m_draftPolygonItem->setZValue(103.0);
    }

    QPainterPath path;
    const QPointF firstPoint = clampedImagePoint(m_draftPolygonImagePoints.first());
    path.moveTo(firstPoint);
    for (int index = 1; index < m_draftPolygonImagePoints.size(); ++index)
        path.lineTo(clampedImagePoint(m_draftPolygonImagePoints.at(index)));
    if (m_polygonHoverPointValid)
        path.lineTo(clampedImagePoint(m_polygonHoverPoint));

    for (QGraphicsItem *item : m_draftPolygonPointItems) {
        if (item) {
            m_scene->removeItem(item);
            delete item;
        }
    }
    m_draftPolygonPointItems.clear();

    const double radius = 3.5;
    for (const QPointF &point : qAsConst(m_draftPolygonImagePoints)) {
        const QPointF clamped = clampedImagePoint(point);
        QGraphicsEllipseItem *pointItem = m_scene->addEllipse(QRectF(clamped.x() - radius,
                                                                     clamped.y() - radius,
                                                                     radius * 2.0,
                                                                     radius * 2.0),
                                                              cosmeticPen(QColor(0, 210, 255), 1.2),
                                                              QBrush(QColor(0, 210, 255, 90)));
        pointItem->setZValue(104.0);
        m_draftPolygonPointItems.append(pointItem);
    }

    m_draftPolygonItem->setPath(path);
    m_draftPolygonItem->show();
}

void FrameViewHelper::clearDraftPolygonItem()
{
    if (!m_scene) {
        m_draftPolygonItem = nullptr;
        m_draftPolygonPointItems.clear();
        return;
    }

    if (m_draftPolygonItem) {
        m_scene->removeItem(m_draftPolygonItem);
        delete m_draftPolygonItem;
        m_draftPolygonItem = nullptr;
    }

    for (QGraphicsItem *item : m_draftPolygonPointItems) {
        if (item) {
            m_scene->removeItem(item);
            delete item;
        }
    }
    m_draftPolygonPointItems.clear();
}

void FrameViewHelper::updateDraftCircleItem()
{
    if (!m_scene)
        return;

    if (!m_draftCircleItem) {
        m_draftCircleItem = m_scene->addEllipse(QRectF(),
                                                cosmeticPen(QColor(0, 210, 255), 1.5));
        m_draftCircleItem->setZValue(103.0);
    }

    const double radius = qMax(0.0, m_circleDraftRadiusPixels);
    m_draftCircleItem->setRect(QRectF(m_circleDraftCenter.x() - radius,
                                      m_circleDraftCenter.y() - radius,
                                      radius * 2.0,
                                      radius * 2.0));
    m_draftCircleItem->show();
}

void FrameViewHelper::clearDraftCircleItem()
{
    if (!m_draftCircleItem || !m_scene)
        return;

    m_scene->removeItem(m_draftCircleItem);
    delete m_draftCircleItem;
    m_draftCircleItem = nullptr;
}

void FrameViewHelper::updateDraftLineBandItem()
{
    clearLineBandItems(&m_lineBandDraftItems);
    if (m_lastImage.isNull())
        return;

    const LineBandRoi roi = imageLineBandToNormalized(m_lineBandDraftP1,
                                                      m_lineBandDraftP2,
                                                      m_lineBandDraftWidthPixels);
    if (!roi.valid)
        return;

    addLineBandItems(roi, &m_lineBandDraftItems, 105.0);
}

void FrameViewHelper::clearLineBandItems(QVector<QGraphicsItem *> *items)
{
    if (!items)
        return;

    if (!m_scene) {
        items->clear();
        return;
    }

    for (QGraphicsItem *item : *items) {
        if (item) {
            m_scene->removeItem(item);
            delete item;
        }
    }
    items->clear();
}

void FrameViewHelper::addLineBandItems(const LineBandRoi &roi,
                                       QVector<QGraphicsItem *> *items,
                                       qreal zValue)
{
    if (!m_scene || !items || !isValidLineBand(roi))
        return;

    const QVector<QPointF> polygonPoints = lineBandPolygonImagePoints(roi);
    if (polygonPoints.size() < 4)
        return;

    QPolygonF polygon;
    for (const QPointF &point : polygonPoints)
        polygon << point;

    QGraphicsPolygonItem *bandItem = m_scene->addPolygon(polygon,
                                                         cosmeticPen(QColor(255, 122, 0), 2.0));
    bandItem->setToolTip(QStringLiteral("line_band"));
    bandItem->setZValue(zValue);
    items->append(bandItem);

    const QPointF p1 = normalizedToImagePoint(roi.p1Normalized);
    const QPointF p2 = normalizedToImagePoint(roi.p2Normalized);
    QGraphicsLineItem *centerItem = m_scene->addLine(QLineF(p1, p2),
                                                     cosmeticPen(QColor(0, 210, 255), 2.0));
    centerItem->setToolTip(QStringLiteral("line_band_center"));
    centerItem->setZValue(zValue + 1.0);
    items->append(centerItem);

    const double radius = 4.0;
    QGraphicsEllipseItem *p1Item = m_scene->addEllipse(QRectF(p1.x() - radius,
                                                              p1.y() - radius,
                                                              radius * 2.0,
                                                              radius * 2.0),
                                                       cosmeticPen(QColor(0, 210, 255), 2.0),
                                                       QBrush(QColor(0, 210, 255, 80)));
    p1Item->setToolTip(QStringLiteral("line_band_p1"));
    p1Item->setZValue(zValue + 2.0);
    items->append(p1Item);

    QGraphicsEllipseItem *p2Item = m_scene->addEllipse(QRectF(p2.x() - radius,
                                                              p2.y() - radius,
                                                              radius * 2.0,
                                                              radius * 2.0),
                                                       cosmeticPen(QColor(0, 210, 255), 2.0),
                                                       QBrush(QColor(0, 210, 255, 80)));
    p2Item->setToolTip(QStringLiteral("line_band_p2"));
    p2Item->setZValue(zValue + 2.0);
    items->append(p2Item);
}

void FrameViewHelper::addOverlayItem(QGraphicsItem *item, qreal zValue)
{
    if (!item)
        return;

    item->setZValue(zValue);
    m_overlayItems.append(item);
}

void FrameViewHelper::restoreImageSceneRect()
{
    if (m_scene)
        m_scene->setSceneRect(m_imageRect);
}
