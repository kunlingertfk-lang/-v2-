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
    if (inputCoordinateType == QStringLiteral("physical")) {
        // 物理坐标到图像坐标的逆变换暂不对生产功能开放。模型仍保留
        // inverse 矩阵，后续明确输入合同和 UI 后可在此扩展。
        return fail(QStringLiteral("physical_coordinate_unsupported"),
                    QStringLiteral("当前仅支持图像像素坐标转物理坐标"),
                    timer.elapsed());
    }
    if (inputCoordinateType != QStringLiteral("image")) {
        return fail(QStringLiteral("invalid_coordinate_type"),
                    QStringLiteral("坐标类型必须为image"), timer.elapsed());
    }
    if (model.validRegion.size() < 3) {
        CalibrationTransformHalconResult result = fail(
                    QStringLiteral("valid_region_missing"),
                    QStringLiteral("标定文件缺少至少3个有效区域采样点"), timer.elapsed());
        result.payload.insert(QStringLiteral("calibrationId"), model.calibrationId);
        result.payload.insert(QStringLiteral("validRegionPointCount"),
                              model.validRegion.size());
        return result;
    }

    try {
        HTuple validRows;
        HTuple validColumns;
        for (const QPointF &point : model.validRegion) {
            validRows.Append(point.y());
            validColumns.Append(point.x());
        }
        HObject sampleContour;
        HObject validHullContour;
        GenContourPolygonXld(&sampleContour, validRows, validColumns);
        ShapeTransXld(sampleContour, &validHullContour, HTuple("convex"));
        HTuple isInside;
        TestXldPoint(validHullContour, HTuple(inputY), HTuple(inputX), &isInside);
        HTuple distanceMin;
        HTuple distanceMax;
        DistancePc(validHullContour, HTuple(inputY), HTuple(inputX),
                   &distanceMin, &distanceMax);
        constexpr double kBoundaryTolerancePx = 1e-6;
        const bool onBoundary = distanceMin.Length() > 0
                && distanceMin[0].D() <= kBoundaryTolerancePx;
        if ((isInside.Length() == 0 || isInside[0].I() == 0) && !onBoundary) {
            HTuple row1;
            HTuple column1;
            HTuple row2;
            HTuple column2;
            SmallestRectangle1Xld(validHullContour,
                                  &row1, &column1, &row2, &column2);
            CalibrationTransformHalconResult result = fail(
                        QStringLiteral("outside_valid_region"),
                        QStringLiteral("输入像素坐标超出标定有效区域"), timer.elapsed());
            result.payload.insert(QStringLiteral("calibrationId"), model.calibrationId);
            result.payload.insert(QStringLiteral("inputPoint"), QJsonObject{
                                      {QStringLiteral("x"), inputX},
                                      {QStringLiteral("y"), inputY},
                                      {QStringLiteral("angleDeg"), inputAngleDeg}});
            result.payload.insert(QStringLiteral("validRegionPointCount"),
                                  model.validRegion.size());
            result.payload.insert(QStringLiteral("validRegionBoundaryTolerancePx"),
                                  kBoundaryTolerancePx);
            if (row1.Length() > 0 && column1.Length() > 0
                    && row2.Length() > 0 && column2.Length() > 0) {
                result.payload.insert(QStringLiteral("validRegionBounds"), QJsonObject{
                                          {QStringLiteral("minColumn"), column1[0].D()},
                                          {QStringLiteral("minRow"), row1[0].D()},
                                          {QStringLiteral("maxColumn"), column2[0].D()},
                                          {QStringLiteral("maxRow"), row2[0].D()}});
                result.message += QStringLiteral(" (C:%1..%2, R:%3..%4)")
                        .arg(column1[0].D()).arg(column2[0].D())
                        .arg(row1[0].D()).arg(row2[0].D());
            }
            return result;
        }

        CalibrationTransformPose effectiveCalibration = calibrationPose;
        CalibrationTransformPose effectiveRun = runPose;
        if (!effectiveCalibration.enabled)
            effectiveCalibration = CalibrationTransformPose();
        if (!effectiveRun.enabled)
            effectiveRun = CalibrationTransformPose();
        const bool poseApplied = calibrationPose.enabled || runPose.enabled;
        const HTuple forward = tupleFor(model.forward);
        const double vx = std::cos(radians(inputAngleDeg));
        const double vy = std::sin(radians(inputAngleDeg));

        double baseX = inputX;
        double baseY = inputY;
        double directionX = inputX + vx;
        double directionY = inputY + vy;
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
            {QStringLiteral("validRegionPointCount"), model.validRegion.size()},
            {QStringLiteral("validRegionBoundaryTolerancePx"), kBoundaryTolerancePx},
            {QStringLiteral("poseCompensationApplied"), poseApplied},
            {QStringLiteral("poseCompensationStatus"),
             poseApplied ? QStringLiteral("applied") : QStringLiteral("disabled")},
            {QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs)}
        };
        result.payload.insert(QStringLiteral("machineX"), baseX);
        result.payload.insert(QStringLiteral("machineY"), baseY);
        return result;
    } catch (const HException &exception) {
        return fail(QStringLiteral("halcon_error"),
                    QStringLiteral("HALCON %1: %2")
                    .arg(static_cast<qlonglong>(exception.ErrorCode()))
                    .arg(QString::fromUtf8(exception.ErrorMessage().Text())),
                    timer.elapsed());
    }
}
