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
    bool productionAllowed = false;
    bool coordinateAvailable = false;
    CalibrationRegion region = CalibrationRegion::Invalid;
    CalibrationRotationCoverage rotationCoverage =
            CalibrationRotationCoverage::NotConfigured;
    bool angleVerified = false;
    bool angleProductionAllowed = false;
    QString status;
    QString message;
    double outputX = 0.0;
    double outputY = 0.0;
    double outputAngleDeg = 0.0;
    double pixelAccuracy = 0.0;
    double distanceToSafeBoundaryPx = 0.0;
    double distanceToValidBoundaryPx = 0.0;
    bool poseCompensationApplied = false;
    qint64 elapsedMs = 0;
    QJsonObject payload;
};

/// 使用 HALCON 执行像素到物理坐标、区域分类、姿态及可选旋转偏心补偿。
class CalibrationTransformHalconRunner
{
public:
    /// 转换单个已归一化输入点；数学成功与生产放行分别由 success/productionAllowed 表达。
    CalibrationTransformHalconResult run(
            const CalibrationModel &model,
            const QString &inputCoordinateType,
            double inputX,
            double inputY,
            double inputAngleDeg,
            const CalibrationTransformPose &calibrationPose = CalibrationTransformPose(),
            const CalibrationTransformPose &runPose = CalibrationTransformPose(),
            bool allowBoundaryForProduction = false) const;
};

// DynamicMechanicalAxisCompensationExtension is intentionally not consumed
// here.  It is a future-only hook declared by CalibrationModel.h; the current
// fixed-camera transform has no external mechanical-axis runtime input.

#endif // ALGORITHMS_LOCATION_CALIBRATIONTRANSFORMHALCONRUNNER_H
