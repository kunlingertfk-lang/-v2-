#ifndef FRAME_ROIGEOMETRY_H
#define FRAME_ROIGEOMETRY_H

#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QtGlobal>

#include <cmath>

enum class RoiShapeType {
    Rectangle,
    Polygon,
    Circle,
    FreeDraw,
    LineBand
};

enum class RoiEditTarget {
    None,
    TemplateRoi,
    DetectRoi,
    TemplateMask,
    DetectMask
};

struct LineBandRoi
{
    QPointF p1Normalized;
    QPointF p2Normalized;
    double widthNormalized = 0.04;
    bool valid = false;
};

struct CircleRoi
{
    QPointF centerNormalized;
    double radiusNormalized = 0.0;
    QRectF boundingRectNormalized;
    bool valid = false;
};

// 将连续的归一化 ROI 转换为能完整覆盖它的半开像素矩形。
// 左/上边界向下取整，右/下边界向上取整，避免非整数起点导致末行或末列丢失。
inline QRect coveringPixelRect(const QRectF &sourceRoi, int imageWidth, int imageHeight)
{
    if (imageWidth <= 0 || imageHeight <= 0
            || !std::isfinite(sourceRoi.x())
            || !std::isfinite(sourceRoi.y())
            || !std::isfinite(sourceRoi.width())
            || !std::isfinite(sourceRoi.height())) {
        return QRect();
    }

    const QRectF roi = sourceRoi.normalized().intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (roi.width() <= 0.0 || roi.height() <= 0.0)
        return QRect();

    const int left = qBound(0, static_cast<int>(std::floor(roi.left() * imageWidth)), imageWidth);
    const int top = qBound(0, static_cast<int>(std::floor(roi.top() * imageHeight)), imageHeight);
    const int rightExclusive = qBound(left,
                                      static_cast<int>(std::ceil(roi.right() * imageWidth)),
                                      imageWidth);
    const int bottomExclusive = qBound(top,
                                       static_cast<int>(std::ceil(roi.bottom() * imageHeight)),
                                       imageHeight);
    if (rightExclusive <= left || bottomExclusive <= top)
        return QRect();
    return QRect(left, top, rightExclusive - left, bottomExclusive - top);
}

#endif // FRAME_ROIGEOMETRY_H
