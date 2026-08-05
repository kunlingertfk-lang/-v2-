#include "algorithms/location/CalibrationTransformHalconRunner.h"

#include <HalconCpp.h>

#include <QElapsedTimer>

#include <cmath>

namespace {

using namespace HalconCpp;

constexpr double kPi = 3.14159265358979323846;

double radians(double degrees)
{
    return degrees * kPi / 180.0;
}

double degrees(double radiansValue)
{
    return radiansValue * 180.0 / kPi;
}

HTuple tupleFor(const std::array<double, 6> &matrix)
{
    HTuple tuple;
    for (double value : matrix)
        tuple.Append(value);
    return tuple;
}

bool finitePose(const CalibrationTransformPose &pose)
{
    return std::isfinite(pose.x) && std::isfinite(pose.y)
            && std::isfinite(pose.joint0AngleDeg)
            && std::isfinite(pose.joint1AngleDeg);
}

CalibrationTransformHalconResult fail(const QString &status,
                                      const QString &message,
                                      qint64 elapsed)
{
    CalibrationTransformHalconResult result;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsed;
    return result;
}

void transformPoint(const HTuple &matrix,
                    double x,
                    double y,
                    double *outputX,
                    double *outputY)
{
    HTuple tx;
    HTuple ty;
    AffineTransPoint2d(matrix, HTuple(x), HTuple(y), &tx, &ty);
    *outputX = tx[0].D();
    *outputY = ty[0].D();
}

HTuple relativePose(const CalibrationTransformPose &from,
                    const CalibrationTransformPose &to)
{
    HTuple matrix;
    VectorAngleToRigid(from.y, from.x,
                       radians(from.joint0AngleDeg),
                       to.y, to.x,
                       radians(to.joint0AngleDeg),
                       &matrix);
    return matrix;
}

} // namespace

CalibrationTransformHalconResult CalibrationTransformHalconRunner::run(
        const CalibrationModel &model,
        const QString &inputCoordinateType,
        double inputX,
        double inputY,
        double inputAngleDeg,
        const CalibrationTransformPose &calibrationPose,
        const CalibrationTransformPose &runPose) const
{
    QElapsedTimer timer;
    timer.start();
    QString modelError;
    if (!model.isValid(&modelError))
        return fail(QStringLiteral("invalid_calibration"), modelError, timer.elapsed());
    if (!model.methodIsKnown())
        return fail(QStringLiteral("unsupported_method"), modelError.isEmpty()
                    ? QStringLiteral("未知标定方式只能读取公共摘要，不能执行转换")
                    : modelError, timer.elapsed());
    if (!model.quality.passed)
        return fail(QStringLiteral("quality_not_passed"),
                    QStringLiteral("标定模型未通过质量门禁"), timer.elapsed());
    if (!std::isfinite(inputX) || !std::isfinite(inputY)
            || !std::isfinite(inputAngleDeg)
            || !finitePose(calibrationPose) || !finitePose(runPose)) {
        return fail(QStringLiteral("invalid_input"),
                    QStringLiteral("坐标、角度或位姿包含非有限值"), timer.elapsed());
    }
    if ((calibrationPose.enabled && std::abs(calibrationPose.joint1AngleDeg) > 1e-9)
            || (runPose.enabled && std::abs(runPose.joint1AngleDeg) > 1e-9)) {
        return fail(QStringLiteral("unsupported_joint_pose"),
                    QStringLiteral("当前版本尚未定义非零Joint1Angle的运动学补偿"),
                    timer.elapsed());
    }
    if (inputCoordinateType != QStringLiteral("image")
            && inputCoordinateType != QStringLiteral("physical")) {
        return fail(QStringLiteral("invalid_coordinate_type"),
                    QStringLiteral("坐标类型必须为image或physical"), timer.elapsed());
    }

    try {
        CalibrationTransformPose effectiveCalibration = calibrationPose;
        CalibrationTransformPose effectiveRun = runPose;
        if (!effectiveCalibration.enabled)
            effectiveCalibration = CalibrationTransformPose();
        if (!effectiveRun.enabled)
            effectiveRun = CalibrationTransformPose();
        const bool poseApplied = calibrationPose.enabled || runPose.enabled;
        const HTuple forward = tupleFor(model.forward);
        const HTuple inverse = tupleFor(model.inverse);
        const double vx = std::cos(radians(inputAngleDeg));
        const double vy = std::sin(radians(inputAngleDeg));

        double baseX = inputX;
        double baseY = inputY;
        double directionX = inputX + vx;
        double directionY = inputY + vy;
        if (inputCoordinateType == QStringLiteral("image")) {
            transformPoint(forward, baseX, baseY, &baseX, &baseY);
            transformPoint(forward, directionX, directionY,
                           &directionX, &directionY);
            if (poseApplied) {
                const HTuple calibrationToRun = relativePose(effectiveCalibration,
                                                             effectiveRun);
                transformPoint(calibrationToRun, baseX, baseY, &baseX, &baseY);
                transformPoint(calibrationToRun, directionX, directionY,
                               &directionX, &directionY);
            }
        } else {
            if (poseApplied) {
                const HTuple runToCalibration = relativePose(effectiveRun,
                                                             effectiveCalibration);
                transformPoint(runToCalibration, baseX, baseY, &baseX, &baseY);
                transformPoint(runToCalibration, directionX, directionY,
                               &directionX, &directionY);
            }
            transformPoint(inverse, baseX, baseY, &baseX, &baseY);
            transformPoint(inverse, directionX, directionY,
                           &directionX, &directionY);
        }

        const double outputAngle = degrees(std::atan2(directionY - baseY,
                                                       directionX - baseX));
        const double scaleX = std::hypot(model.forward[0], model.forward[3]);
        const double scaleY = std::hypot(model.forward[1], model.forward[4]);
        CalibrationTransformHalconResult result;
        result.success = true;
        result.status = QStringLiteral("converted");
        result.message = QStringLiteral("标定转换成功");
        result.outputX = baseX;
        result.outputY = baseY;
        result.outputAngleDeg = outputAngle;
        result.pixelAccuracy = (scaleX + scaleY) / 2.0;
        result.poseCompensationApplied = poseApplied;
        result.elapsedMs = timer.elapsed();
        result.payload = QJsonObject{
            {QStringLiteral("inputCoordinateType"), inputCoordinateType},
            {QStringLiteral("inputPoint"), QJsonObject{
                 {QStringLiteral("x"), inputX},
                 {QStringLiteral("y"), inputY},
                 {QStringLiteral("angleDeg"), inputAngleDeg}}},
            {QStringLiteral("convertedPoint"), QJsonObject{
                 {QStringLiteral("x"), baseX},
                 {QStringLiteral("y"), baseY}}},
            {QStringLiteral("convertedAngleDeg"), outputAngle},
            {QStringLiteral("pixelAccuracy"), result.pixelAccuracy},
            {QStringLiteral("pixelAccuracyUnit"), QStringLiteral("physical_per_pixel")},
            {QStringLiteral("calibrationId"), model.calibrationId},
            {QStringLiteral("methodId"), model.methodId},
            {QStringLiteral("poseCompensationApplied"), poseApplied},
            {QStringLiteral("poseCompensationStatus"),
             poseApplied ? QStringLiteral("applied") : QStringLiteral("disabled")},
            {QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs)}
        };
        if (inputCoordinateType == QStringLiteral("image")) {
            result.payload.insert(QStringLiteral("machineX"), baseX);
            result.payload.insert(QStringLiteral("machineY"), baseY);
        } else {
            result.payload.insert(QStringLiteral("column"), baseX);
            result.payload.insert(QStringLiteral("row"), baseY);
        }
        return result;
    } catch (const HException &exception) {
        return fail(QStringLiteral("halcon_error"),
                    QStringLiteral("HALCON %1: %2")
                    .arg(static_cast<qlonglong>(exception.ErrorCode()))
                    .arg(QString::fromUtf8(exception.ErrorMessage().Text())),
                    timer.elapsed());
    }
}
