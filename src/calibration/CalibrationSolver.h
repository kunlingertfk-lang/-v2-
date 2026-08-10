#ifndef CALIBRATION_CALIBRATIONSOLVER_H
#define CALIBRATION_CALIBRATIONSOLVER_H

#include "calibration/CalibrationModel.h"

#include <QString>

struct CalibrationSolveResult
{
    bool success = false;
    QString status;
    QString message;
    CalibrationModel model;
};

class CalibrationSolver
{
public:
    CalibrationSolveResult solveNPoint(const QVector<CalibrationSample> &samples,
                                       double rmseLimit = 0.10,
                                       double maxErrorLimit = 0.25,
                                       double safeMarginPx = 0.0) const;

    CalibrationRotationRange buildRotationRange(
            const QVector<CalibrationSample> &rotationSamples) const;
};

#endif // CALIBRATION_CALIBRATIONSOLVER_H
