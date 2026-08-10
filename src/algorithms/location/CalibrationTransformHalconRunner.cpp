#include "algorithms/location/CalibrationTransformHalconRunner.h"

#include <HalconCpp.h>

#include <QElapsedTimer>

#include <cmath>

namespace {

using namespace HalconCpp;

constexpr double kPi = 3.14159265358979323846;
constexpr double kBoundaryTolerancePx = 1e-6;
constexpr double kRotationToleranceDeg = 1e-9;

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

bool finitePolygon(const QVector<QPointF> &polygon)
{
    if (polygon.size() < 3)
        return false;
    for (const QPointF &point : polygon) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
            return false;
    }
    return true;
}

bool contourForPolygon(const QVector<QPointF> &polygon, HObject *contour)
{
    if (!contour || !finitePolygon(polygon))
        return false;
    HTuple rows;
    HTuple columns;
    for (const QPointF &point : polygon) {
        rows.Append(point.y());
        columns.Append(point.x());
    }
    HObject polygonContour;
    GenContourPolygonXld(&polygonContour, rows, columns);
    ShapeTransXld(polygonContour, contour, HTuple("convex"));
    return true;
}

bool classifyAgainstContour(const HObject &contour,
                            double column,
                            double row,
                            bool *insideOrBoundary,
                            double *distanceToBoundary)
{
    if (!insideOrBoundary || !distanceToBoundary)
        return false;
    HTuple isInside;
    HTuple distanceMin;
    HTuple distanceMax;
    TestXldPoint(contour, HTuple(row), HTuple(column), &isInside);
    DistancePc(contour, HTuple(row), HTuple(column), &distanceMin, &distanceMax);
    if (distanceMin.Length() == 0)
        return false;
    const double distance = distanceMin[0].D();
    if (!std::isfinite(distance))
        return false;
    const bool onBoundary = distance <= kBoundaryTolerancePx;
    *insideOrBoundary = (isInside.Length() > 0 && isInside[0].I() != 0)
            || onBoundary;
    *distanceToBoundary = distance;
    return true;
}

double angleNearCenter(double angleDeg, double centerDeg, double periodDeg)
{
    return angleDeg + periodDeg * std::round((centerDeg - angleDeg) / periodDeg);
}

bool validRotationRange(const CalibrationRotationRange &range)
{
    const double imageSpan = range.imageMaxDeg - range.imageMinDeg;
    const double machineSpan = range.machineMaxDeg - range.machineMinDeg;
    return std::isfinite(range.periodDeg) && range.periodDeg > 0.0
            && std::isfinite(range.imageMinDeg)
            && std::isfinite(range.imageMaxDeg)
            && std::isfinite(range.imageCenterDeg)
            && std::isfinite(range.machineMinDeg)
            && std::isfinite(range.machineMaxDeg)
            && std::isfinite(range.machineCenterDeg)
            && range.imageMinDeg <= range.imageMaxDeg
            && range.machineMinDeg <= range.machineMaxDeg
            && imageSpan > kRotationToleranceDeg
            && machineSpan > kRotationToleranceDeg
            && imageSpan <= range.periodDeg + kRotationToleranceDeg
            && machineSpan <= range.periodDeg + kRotationToleranceDeg
            && range.imageCenterDeg >= range.imageMinDeg
            && range.imageCenterDeg <= range.imageMaxDeg
            && range.machineCenterDeg >= range.machineMinDeg
            && range.machineCenterDeg <= range.machineMaxDeg;
}

bool angleInRange(double angleDeg,
                  double minimumDeg,
                  double maximumDeg,
                  double centerDeg,
                  double periodDeg,
                  double *unwrappedAngleDeg)
{
    const double unwrapped = angleNearCenter(angleDeg, centerDeg, periodDeg);
    if (unwrappedAngleDeg)
        *unwrappedAngleDeg = unwrapped;
    return unwrapped >= minimumDeg - kRotationToleranceDeg
            && unwrapped <= maximumDeg + kRotationToleranceDeg;
}

CalibrationTransformHalconResult fail(const QString &status,
                                      const QString &message,
                                      qint64 elapsed)
{
    CalibrationTransformHalconResult result;
    result.region = CalibrationRegion::Invalid;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsed;
    result.payload.insert(QStringLiteral("calibrationRegion"),
                          calibrationRegionToString(result.region));
    result.payload.insert(QStringLiteral("rotationCoverage"),
                          calibrationRotationCoverageToString(
                              result.rotationCoverage));
    result.payload.insert(QStringLiteral("productionAllowed"), false);
    result.payload.insert(QStringLiteral("coordinateAvailable"), false);
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
        const CalibrationTransformPose &runPose,
        bool allowBoundaryForProduction) const
{
    QElapsedTimer timer;
    timer.start();
    QString modelError;
    if (!model.isValid(&modelError)) {
        const bool regionOrRotationInvalid =
                modelError.contains(QStringLiteral("region"), Qt::CaseInsensitive)
                || modelError.contains(QStringLiteral("rotation"), Qt::CaseInsensitive);
        if (regionOrRotationInvalid && !model.quality.passed) {
            return fail(QStringLiteral("quality_not_passed"),
                        QStringLiteral("标定模型未通过质量门禁"), timer.elapsed());
        }
        CalibrationTransformHalconResult result = fail(
                    regionOrRotationInvalid
                    ? QStringLiteral("calibration_region_invalid")
                    : QStringLiteral("invalid_calibration"),
                    modelError, timer.elapsed());
        if (model.rotationRange.status == CalibrationRotationRangeStatus::Invalid
                || modelError.contains(QStringLiteral("rotation"),
                                       Qt::CaseInsensitive)) {
            result.rotationCoverage = CalibrationRotationCoverage::Invalid;
            result.payload.insert(
                        QStringLiteral("rotationCoverage"),
                        calibrationRotationCoverageToString(
                            result.rotationCoverage));
        }
        return result;
    }
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
    if (!finitePolygon(model.validRegion) || !finitePolygon(model.safeRegion)
            || !std::isfinite(model.safeMarginPx) || model.safeMarginPx < 0.0) {
        CalibrationTransformHalconResult result = fail(
                    QStringLiteral("calibration_region_invalid"),
                    QStringLiteral("标定文件的有效区域或安全区域无效"), timer.elapsed());
        result.payload.insert(QStringLiteral("calibrationId"), model.calibrationId);
        result.payload.insert(QStringLiteral("validRegionPointCount"),
                              model.validRegion.size());
        result.payload.insert(QStringLiteral("safeRegionPointCount"),
                              model.safeRegion.size());
        result.payload.insert(QStringLiteral("safeMarginPx"), model.safeMarginPx);
        return result;
    }
    if (model.rotationRange.status == CalibrationRotationRangeStatus::Invalid
            || (model.rotationRange.status == CalibrationRotationRangeStatus::Verified
                && !validRotationRange(model.rotationRange))) {
        CalibrationTransformHalconResult result = fail(
                    QStringLiteral("calibration_region_invalid"),
                    QStringLiteral("标定文件的旋转覆盖范围无效"), timer.elapsed());
        result.rotationCoverage = CalibrationRotationCoverage::Invalid;
        result.payload.insert(QStringLiteral("rotationCoverage"),
                              calibrationRotationCoverageToString(
                                  result.rotationCoverage));
        result.payload.insert(QStringLiteral("calibrationId"), model.calibrationId);
        return result;
    }

    try {
        HObject validHullContour;
        HObject safeHullContour;
        if (!contourForPolygon(model.validRegion, &validHullContour)
                || !contourForPolygon(model.safeRegion, &safeHullContour)) {
            return fail(QStringLiteral("calibration_region_invalid"),
                        QStringLiteral("无法构造标定有效区域"), timer.elapsed());
        }
        bool insideSafe = false;
        bool insideValid = false;
        double distanceToSafeBoundary = 0.0;
        double distanceToValidBoundary = 0.0;
        if (!classifyAgainstContour(safeHullContour, inputX, inputY,
                                    &insideSafe, &distanceToSafeBoundary)
                || !classifyAgainstContour(validHullContour, inputX, inputY,
                                            &insideValid,
                                            &distanceToValidBoundary)) {
            return fail(QStringLiteral("calibration_region_invalid"),
                        QStringLiteral("无法判断输入点所在标定区域"), timer.elapsed());
        }
        const CalibrationRegion region = insideSafe
                ? CalibrationRegion::Safe
                : insideValid ? CalibrationRegion::Boundary
                              : CalibrationRegion::Extrapolation;

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
        CalibrationRotationCoverage rotationCoverage =
                CalibrationRotationCoverage::Unverified;
        double unwrappedInputAngle = inputAngleDeg;
        double unwrappedOutputAngle = outputAngle;
        if (model.rotationRange.status == CalibrationRotationRangeStatus::Verified) {
            const bool imageAngleInRange = angleInRange(
                        inputAngleDeg,
                        model.rotationRange.imageMinDeg,
                        model.rotationRange.imageMaxDeg,
                        model.rotationRange.imageCenterDeg,
                        model.rotationRange.periodDeg,
                        &unwrappedInputAngle);
            const bool machineAngleInRange = angleInRange(
                        outputAngle,
                        model.rotationRange.machineMinDeg,
                        model.rotationRange.machineMaxDeg,
                        model.rotationRange.machineCenterDeg,
                        model.rotationRange.periodDeg,
                        &unwrappedOutputAngle);
            rotationCoverage = imageAngleInRange && machineAngleInRange
                    ? CalibrationRotationCoverage::InRange
                    : CalibrationRotationCoverage::OutOfRange;
        }
        const double scaleX = std::hypot(model.forward[0], model.forward[3]);
        const double scaleY = std::hypot(model.forward[1], model.forward[4]);
        CalibrationTransformHalconResult result;
        result.success = true;
        result.coordinateAvailable = true;
        result.region = region;
        result.rotationCoverage = rotationCoverage;
        result.productionAllowed = region == CalibrationRegion::Safe
                || (region == CalibrationRegion::Boundary
                    && allowBoundaryForProduction);
        if (region == CalibrationRegion::Extrapolation
                || rotationCoverage == CalibrationRotationCoverage::OutOfRange
                || rotationCoverage == CalibrationRotationCoverage::Invalid) {
            result.productionAllowed = false;
        }
        if (region == CalibrationRegion::Extrapolation) {
            result.status = QStringLiteral("converted_extrapolation");
            result.message = QStringLiteral(
                        "输入像素坐标位于标定有效区域外，转换坐标仅供诊断");
        } else if (rotationCoverage == CalibrationRotationCoverage::OutOfRange) {
            result.status = QStringLiteral("rotation_out_of_range");
            result.message = QStringLiteral(
                        "输入或输出角度超出已标定旋转覆盖范围");
        } else if (region == CalibrationRegion::Boundary) {
            result.status = QStringLiteral("converted_boundary");
            result.message = allowBoundaryForProduction
                    ? QStringLiteral("输入像素坐标位于边界警戒带，已按工具配置放行")
                    : QStringLiteral("输入像素坐标位于边界警戒带，默认禁止生产使用");
        } else {
            result.status = QStringLiteral("converted_safe");
            result.message = QStringLiteral("标定转换成功");
        }
        if (rotationCoverage == CalibrationRotationCoverage::Unverified) {
            result.message += QStringLiteral("；旋转覆盖范围尚未验证");
        }
        result.outputX = baseX;
        result.outputY = baseY;
        result.outputAngleDeg = outputAngle;
        result.pixelAccuracy = (scaleX + scaleY) / 2.0;
        result.distanceToSafeBoundaryPx = distanceToSafeBoundary;
        result.distanceToValidBoundaryPx = distanceToValidBoundary;
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
            {QStringLiteral("calibrationRegion"),
             calibrationRegionToString(region)},
            {QStringLiteral("rotationCoverage"),
             calibrationRotationCoverageToString(rotationCoverage)},
            {QStringLiteral("productionAllowed"), result.productionAllowed},
            {QStringLiteral("coordinateAvailable"), true},
            {QStringLiteral("distanceToSafeBoundaryPx"),
             distanceToSafeBoundary},
            {QStringLiteral("distanceToValidBoundaryPx"),
             distanceToValidBoundary},
            {QStringLiteral("validRegionPointCount"), model.validRegion.size()},
            {QStringLiteral("safeRegionPointCount"), model.safeRegion.size()},
            {QStringLiteral("safeMarginPx"), model.safeMarginPx},
            {QStringLiteral("allowBoundaryForProduction"),
             allowBoundaryForProduction},
            {QStringLiteral("validRegionBoundaryTolerancePx"), kBoundaryTolerancePx},
            {QStringLiteral("safeRegionBoundaryTolerancePx"), kBoundaryTolerancePx},
            {QStringLiteral("inputAngleUnwrappedDeg"), unwrappedInputAngle},
            {QStringLiteral("outputAngleUnwrappedDeg"), unwrappedOutputAngle},
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
