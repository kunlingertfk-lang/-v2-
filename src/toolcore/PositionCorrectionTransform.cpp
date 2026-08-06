#include "toolcore/PositionCorrectionTransform.h"

#include <QPolygonF>

#include <cmath>

bool PositionCorrectionTransform::isValidHomMat2D(
        const QVector<double> &homMat2D)
{
    if (homMat2D.size() != 6)
        return false;
    for (double value : homMat2D) {
        if (!std::isfinite(value))
            return false;
    }
    return true;
}

QPointF PositionCorrectionTransform::transformPoint(
        const QPointF &point,
        const QVector<double> &homMat2D)
{
    if (!isValidHomMat2D(homMat2D))
        return point;
    const double row = point.y();
    const double column = point.x();
    return QPointF(homMat2D.at(3) * row
                   + homMat2D.at(4) * column
                   + homMat2D.at(5),
                   homMat2D.at(0) * row
                   + homMat2D.at(1) * column
                   + homMat2D.at(2));
}

QVector<QPointF> PositionCorrectionTransform::transformPoints(
        const QVector<QPointF> &points,
        const QVector<double> &homMat2D)
{
    QVector<QPointF> transformed;
    transformed.reserve(points.size());
    for (const QPointF &point : points)
        transformed.append(transformPoint(point, homMat2D));
    return transformed;
}

ToolOverlay PositionCorrectionTransform::transformOverlay(
        const ToolOverlay &overlay,
        const QVector<double> &homMat2D)
{
    if (!isValidHomMat2D(homMat2D))
        return overlay;

    ToolOverlay transformed = overlay;
    switch (overlay.type) {
    case ToolOverlayType::Rect: {
        transformed.type = ToolOverlayType::Polygon;
        transformed.points = transformPoints({
            overlay.rect.topLeft(),
            overlay.rect.topRight(),
            overlay.rect.bottomRight(),
            overlay.rect.bottomLeft()
        }, homMat2D);
        transformed.rect = QPolygonF(transformed.points).boundingRect();
        break;
    }
    case ToolOverlayType::Line:
        transformed.p1 = transformPoint(overlay.p1, homMat2D);
        transformed.p2 = transformPoint(overlay.p2, homMat2D);
        break;
    case ToolOverlayType::Circle: {
        transformed.center = transformPoint(overlay.center, homMat2D);
        const QPointF radiusPoint = transformPoint(
                    QPointF(overlay.center.x() + overlay.radius,
                            overlay.center.y()),
                    homMat2D);
        transformed.radius = std::hypot(
                    radiusPoint.x() - transformed.center.x(),
                    radiusPoint.y() - transformed.center.y());
        break;
    }
    case ToolOverlayType::Polygon:
        transformed.points = transformPoints(overlay.points, homMat2D);
        break;
    case ToolOverlayType::Text:
        transformed.p1 = transformPoint(overlay.p1, homMat2D);
        break;
    case ToolOverlayType::Unknown:
    default:
        break;
    }
    transformed.extra.insert(QStringLiteral("positionCorrectionApplied"), true);
    return transformed;
}

QVector<ToolOverlay> PositionCorrectionTransform::matchContourOverlays(
        const QVector<ToolOverlay> &contours,
        const QString &sourceId)
{
    QVector<ToolOverlay> overlays;
    overlays.reserve(contours.size());
    for (ToolOverlay contour : contours) {
        contour.extra.insert(QStringLiteral("role"),
                             QStringLiteral("position_correction_match_contour"));
        contour.extra.insert(QStringLiteral("positionCorrectionSourceId"),
                             sourceId);
        overlays.append(contour);
    }
    return overlays;
}

QVector<ToolOverlay> PositionCorrectionTransform::matchOriginOverlays(
        const QVector<ToolOverlay> &origins,
        const QString &sourceId)
{
    QVector<ToolOverlay> overlays;
    overlays.reserve(origins.size());
    for (ToolOverlay origin : origins) {
        origin.extra.insert(QStringLiteral("role"),
                            QStringLiteral("position_correction_match_origin"));
        origin.extra.insert(QStringLiteral("positionCorrectionSourceId"),
                            sourceId);
        overlays.append(origin);
    }
    return overlays;
}
