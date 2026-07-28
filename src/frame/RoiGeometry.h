#ifndef FRAME_ROIGEOMETRY_H
#define FRAME_ROIGEOMETRY_H

#include <QPointF>
#include <QRectF>

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

#endif // FRAME_ROIGEOMETRY_H
