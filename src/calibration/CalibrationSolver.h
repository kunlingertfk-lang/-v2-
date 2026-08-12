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
    /// 使用 HALCON 从严格 9 组 Column/Row -> Machine X/Y 样本拟合二维仿射模型、逆矩阵和有效区域。
    CalibrationSolveResult solveNPoint(const QVector<CalibrationSample> &samples,
                                       double rmseLimit = 0.10,
                                       double maxErrorLimit = 0.25,
                                       double safeMarginPx = 0.0) const;

    /// 用 3 个旋转样本拟合图像方向到机械角度的正/反向关系、偏置及误差门禁。
    CalibrationAngleMapping buildAngleMapping(
            const QVector<CalibrationSample> &rotationSamples,
            const std::array<double, 6> &forward,
            double minimumSpanDeg = 5.0,
            double rmseLimitDeg = 1.0,
            double maxErrorLimitDeg = 2.0) const;

    /// 用旋转样本在仿射后的物理平面拟合旋转圆、方向、相位、偏心量及质量状态。
    CalibrationRotationRange buildAxisTrace(
            const QVector<CalibrationSample> &rotationSamples,
            const std::array<double, 6> &forward,
            double minimumSpanDeg = 5.0,
            double rmseLimitMm = 0.10,
            double maxErrorLimitMm = 0.25) const;
};

#endif // CALIBRATION_CALIBRATIONSOLVER_H
