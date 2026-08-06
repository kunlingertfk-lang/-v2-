#ifndef ALGORITHMS_LOCATION_CALIBRATIONTRANSFORMHALCONRUNNER_H
#define ALGORITHMS_LOCATION_CALIBRATIONTRANSFORMHALCONRUNNER_H

#include "calibration/CalibrationModel.h"

#include <QJsonObject>
#include <QString>

struct CalibrationTransformPose
{
    bool enabled = false;
    double x = 0.0;
    double y = 0.0;
    double joint0AngleDeg = 0.0;
    double joint1AngleDeg = 0.0;
};

struct CalibrationTransformHalconResult
{
    bool success = false;
    QString status;
    QString message;
    double outputX = 0.0;
    double outputY = 0.0;
    double outputAngleDeg = 0.0;
    double pixelAccuracy = 0.0;
    bool poseCompensationApplied = false;
    qint64 elapsedMs = 0;
    QJsonObject payload;
};

class CalibrationTransformHalconRunner
{
public:
    CalibrationTransformHalconResult run(
            const CalibrationModel &model,
            const QString &inputCoordinateType,
            double inputX,
            double inputY,
            double inputAngleDeg,
            const CalibrationTransformPose &calibrationPose = CalibrationTransformPose(),
            const CalibrationTransformPose &runPose = CalibrationTransformPose()) const;
};

#endif // ALGORITHMS_LOCATION_CALIBRATIONTRANSFORMHALCONRUNNER_H
