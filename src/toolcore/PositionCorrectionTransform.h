#ifndef TOOLCORE_POSITIONCORRECTIONTRANSFORM_H
#define TOOLCORE_POSITIONCORRECTIONTRANSFORM_H

#include "toolcore/ToolOverlay.h"

#include <QPointF>
#include <QVector>

namespace PositionCorrectionTransform {

bool isValidHomMat2D(const QVector<double> &homMat2D);
QPointF transformPoint(const QPointF &point,
                       const QVector<double> &homMat2D);
QVector<QPointF> transformPoints(const QVector<QPointF> &points,
                                 const QVector<double> &homMat2D);
ToolOverlay transformOverlay(const ToolOverlay &overlay,
                             const QVector<double> &homMat2D);
QVector<ToolOverlay> matchContourOverlays(
        const QVector<ToolOverlay> &contours,
        const QString &sourceId);
QVector<ToolOverlay> matchOriginOverlays(
        const QVector<ToolOverlay> &origins,
        const QString &sourceId);

} // namespace PositionCorrectionTransform

#endif // TOOLCORE_POSITIONCORRECTIONTRANSFORM_H
