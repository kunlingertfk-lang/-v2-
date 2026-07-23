#ifndef ALGORITHMS_LOCATION_POSITIONCORRECTIONHALCONRUNNER_H
#define ALGORITHMS_LOCATION_POSITIONCORRECTIONHALCONRUNNER_H

#include <QJsonObject>
#include <QString>
#include <QtGlobal>

struct PositionPose
{
    double x = 0.0;
    double y = 0.0;
    double angleDeg = 0.0;
    double scale = 1.0;
};

struct PositionCorrectionHalconResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    qint64 elapsedMs = 0;
    QJsonObject payload;
};

class PositionCorrectionHalconRunner
{
public:
    PositionCorrectionHalconResult run(const PositionPose &referencePose,
                                       const PositionPose &runPose) const;
};

#endif // ALGORITHMS_LOCATION_POSITIONCORRECTIONHALCONRUNNER_H
