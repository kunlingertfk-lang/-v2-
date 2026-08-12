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

    CalibrationAngleMapping buildAngleMapping(
            const QVector<CalibrationSample> &rotationSamples,
            const std::array<double, 6> &forward,
            double minimumSpanDeg = 5.0,
            double rmseLimitDeg = 1.0,
            double maxErrorLimitDeg = 2.0) const;

    CalibrationRotationRange buildAxisTrace(
            const QVector<CalibrationSample> &rotationSamples,
            const std::array<double, 6> &forward,
            double minimumSpanDeg = 5.0,
            double rmseLimitMm = 0.10,
            double maxErrorLimitMm = 0.25) const;
};

#endif // CALIBRATION_CALIBRATIONSOLVER_H
