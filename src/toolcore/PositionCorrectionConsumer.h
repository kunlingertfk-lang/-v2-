#ifndef TOOLCORE_POSITIONCORRECTIONCONSUMER_H
#define TOOLCORE_POSITIONCORRECTIONCONSUMER_H

#include "toolcore/ToolOverlay.h"
#include "toolcore/ToolRequest.h"

#include <QString>
#include <QVector>

struct PositionCorrectionConsumerOptions
{
    bool requested = false;
    QString sourceId;
    bool showMatchContour = true;
};

struct PositionCorrectionContext
{
    bool requested = false;
    bool applied = false;
    bool showMatchContour = true;
    QString sourceId;
    QString sourceStatus;
    QVector<double> referenceToRunHomMat2D;
    QVector<double> runToReferenceHomMat2D;
    double referenceScale = 1.0;
    double runScale = 1.0;
    double scaleRatio = 1.0;
    QVector<ToolOverlay> matchContours;
    QVector<ToolOverlay> matchOrigins;
};

struct PositionCorrectionResolveResult
{
    bool success = false;
    QString status;
    QString message;
    PositionCorrectionContext context;
};

namespace PositionCorrectionConsumer {

PositionCorrectionResolveResult resolve(
        const ToolRequest &request,
        const PositionCorrectionConsumerOptions &options);

} // namespace PositionCorrectionConsumer

#endif // TOOLCORE_POSITIONCORRECTIONCONSUMER_H
