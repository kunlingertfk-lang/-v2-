#include "algorithms/location/CalibrationTransformHalconRunner.h"

#include <HalconCpp.h>

#include <QElapsedTimer>

#include <cmath>

namespace {

using namespace HalconCpp;

constexpr double kPi = 3.14159265358979323846;
constexpr double kBoundaryTolerancePx = 1e-6;
constexpr double kRotationToleranceDeg = 1e-9;

// 角度转弧度，供 HALCON 刚性位姿矩阵使用。
double radians(double degreesValue)
{
    return degreesValue * kPi / 180.0;
}

// 弧度转角度，供结果 payload 和范围门禁使用。
double degrees(double radiansValue)
{
    return radiansValue * 180.0 / kPi;
}

// 将项目 2x3 仿射矩阵转换为 HALCON HomMat2D 元组。
HTuple tupleFor(const std::array<double, 6> &matrix)
{
    HTuple tuple;
    for (double value : matrix)
        tuple.Append(value);
    return tuple;
}

// 校验机构位姿四个自由度均为有限值。
bool finitePose(const CalibrationTransformPose &pose)
{
    return std::isfinite(pose.x) && std::isfinite(pose.y)
            && std::isfinite(pose.joint0AngleDeg)
            && std::isfinite(pose.joint1AngleDeg);
}

// 校验区域至少三点且全部为有限 Column/Row。
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

// 将 XML 中的区域多边形转换为 HALCON 凸 XLD 轮廓。
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

// 使用 HALCON 判断点在区域内/边界上，并返回到边界的最小像素距离。
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

// 将周期角归一到最靠近标定范围中心的等价值。
double angleNearCenter(double angleDeg, double centerDeg, double periodDeg)
{
    return angleDeg + periodDeg * std::round((centerDeg - angleDeg) / periodDeg);
}

// 判断周期角是否落在标定覆盖范围内，并返回归一后的角度。
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

// 校验运行期可消费的轴轨迹派生模型合同。
bool validAxisTrace(const CalibrationRotationRange &range)
{
    const double machineSpan = range.machineMaxDeg - range.machineMinDeg;
    const double trajectorySpan = range.trajectoryMaxDeg
            - range.trajectoryMinDeg;
    return range.isFinite() && range.isVerified()
            && range.sampleCount >= 3
            && range.machineMinDeg <= range.machineMaxDeg
            && machineSpan >= range.minimumSpanDeg - kRotationToleranceDeg
            && machineSpan <= range.periodDeg + kRotationToleranceDeg
            && range.machineCenterDeg >= range.machineMinDeg
            && range.machineCenterDeg <= range.machineMaxDeg
            && (range.coaxial
                || (range.direction != CalibrationRotationDirection::Unknown
                    && range.radiusMm > kRotationToleranceDeg
                    && range.trajectoryMinDeg <= range.trajectoryMaxDeg
                    && trajectorySpan >= range.minimumSpanDeg
                       - kRotationToleranceDeg
                    && trajectorySpan <= range.periodDeg
                       + kRotationToleranceDeg
                    && range.trajectoryCenterDeg >= range.trajectoryMinDeg
                    && range.trajectoryCenterDeg <= range.trajectoryMaxDeg))
            && range.fitRmseMm <= range.rmseLimitMm + kRotationToleranceDeg
            && range.maxErrorMm <= range.maxErrorLimitMm
               + kRotationToleranceDeg;
}

// 校验运行期可消费的姿态角映射合同。
bool validAngleMapping(const CalibrationAngleMapping &mapping)
{
    const double imageSpan = mapping.imageMaxDeg - mapping.imageMinDeg;
    const double machineSpan = mapping.machineMaxDeg - mapping.machineMinDeg;
    return mapping.isFinite() && mapping.isVerified()
            && mapping.sampleCount >= 3
            && mapping.direction != CalibrationAngleDirection::Unknown
            && mapping.periodDeg > 0.0
            && mapping.imageMinDeg <= mapping.imageMaxDeg
            && mapping.machineMinDeg <= mapping.machineMaxDeg
            && imageSpan >= mapping.minimumSpanDeg - kRotationToleranceDeg
            && machineSpan >= mapping.minimumSpanDeg - kRotationToleranceDeg
            && imageSpan <= mapping.periodDeg + kRotationToleranceDeg
            && machineSpan <= mapping.periodDeg + kRotationToleranceDeg
            && mapping.imageCenterDeg >= mapping.imageMinDeg
            && mapping.imageCenterDeg <= mapping.imageMaxDeg
            && mapping.machineCenterDeg >= mapping.machineMinDeg
            && mapping.machineCenterDeg <= mapping.machineMaxDeg
            && mapping.rmseDeg <= mapping.rmseLimitDeg
               + kRotationToleranceDeg
            && mapping.maxErrorDeg <= mapping.maxErrorLimitDeg
               + kRotationToleranceDeg;
}

// 构造 Runner 失败结果并附带已耗时。
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
    result.payload.insert(QStringLiteral("coordinateProductionAllowed"), false);
    result.payload.insert(QStringLiteral("coordinateAvailable"), false);
    result.payload.insert(QStringLiteral("angleValid"), false);
    result.payload.insert(QStringLiteral("angleVerified"), false);
    result.payload.insert(QStringLiteral("angleProductionAllowed"), false);
    return result;
}

// 用 HALCON AffineTransPoint2d 转换单个 Column/Row 或 X/Y 点。
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

// 在机械 X/Y/Joint0 坐标约定下生成标定位到运行位的刚性变换。
HTuple relativePose(const CalibrationTransformPose &from,
                    const CalibrationTransformPose &to)
{
    HTuple matrix;
    // The pose values are already expressed in machine X/Y coordinates.
    // Keep the matrix axes in that same order because transformPoint() applies
    // it as (X, Y); swapping to image Row/Column here exchanges translations.
    VectorAngleToRigid(from.x, from.y,
                       radians(from.joint0AngleDeg),
                       to.x, to.y,
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
    // success 表示数学链路完成；productionAllowed 由区域与角度覆盖门禁独立决定。
    QElapsedTimer timer;
    timer.start();

    QString modelError;
    if (!model.isValid(&modelError)) {
        if (!model.quality.passed) {
            return fail(QStringLiteral("quality_not_passed"),
                        QStringLiteral("标定模型未通过质量门禁"), timer.elapsed());
        }
        QString status = QStringLiteral("invalid_calibration");
        if (modelError.contains(QStringLiteral("region"), Qt::CaseInsensitive))
            status = QStringLiteral("calibration_region_invalid");
        else if (modelError.contains(QStringLiteral("angle"), Qt::CaseInsensitive))
            status = QStringLiteral("angle_mapping_invalid");
        else if (modelError.contains(QStringLiteral("rotation"), Qt::CaseInsensitive)
                 || modelError.contains(QStringLiteral("axis"), Qt::CaseInsensitive))
            status = QStringLiteral("axis_trace_invalid");
        return fail(status, modelError, timer.elapsed());
    }
    if (!model.methodIsKnown()) {
        return fail(QStringLiteral("unsupported_method"),
                    modelError.isEmpty()
                    ? QStringLiteral("未知标定方式不能执行转换") : modelError,
                    timer.elapsed());
    }
    if (!model.quality.passed) {
        return fail(QStringLiteral("quality_not_passed"),
                    QStringLiteral("标定模型未通过质量门禁"), timer.elapsed());
    }

    const bool poseMapping = model.mode
            == NPointCalibrationMode::TwelvePointPoseMapping;
    const bool axisTraceConfigured = model.mode
            != NPointCalibrationMode::NinePointXY;
    if (!std::isfinite(inputX) || !std::isfinite(inputY)
            || (poseMapping && !std::isfinite(inputAngleDeg))
            || !finitePose(calibrationPose) || !finitePose(runPose)) {
        return fail(QStringLiteral("invalid_input"),
                    poseMapping
                    ? QStringLiteral("坐标、图像角度或位姿包含非有限值")
                    : QStringLiteral("坐标或位姿包含非有限值"),
                    timer.elapsed());
    }
    if ((calibrationPose.enabled
         && std::abs(calibrationPose.joint1AngleDeg) > 1e-9)
            || (runPose.enabled
                && std::abs(runPose.joint1AngleDeg) > 1e-9)) {
        return fail(QStringLiteral("unsupported_joint_pose"),
                    QStringLiteral("当前版本尚未定义非零Joint1Angle的运动学补偿"),
                    timer.elapsed());
    }
    if (inputCoordinateType == QStringLiteral("physical")) {
        // 物理坐标到图像坐标的逆变换暂不对生产功能开放。
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
                    QStringLiteral("标定文件的有效区域或安全区域无效"),
                    timer.elapsed());
        result.payload.insert(QStringLiteral("calibrationId"), model.calibrationId);
        result.payload.insert(QStringLiteral("validRegionPointCount"),
                              model.validRegion.size());
        result.payload.insert(QStringLiteral("safeRegionPointCount"),
                              model.safeRegion.size());
        result.payload.insert(QStringLiteral("safeMarginPx"), model.safeMarginPx);
        return result;
    }
    if (axisTraceConfigured && !validAxisTrace(model.rotationRange)) {
        return fail(QStringLiteral("axis_trace_invalid"),
                    QStringLiteral("标定文件的旋转中心/轴轨迹模型无效"),
                    timer.elapsed());
    }
    if (poseMapping && !validAngleMapping(model.angleMapping)) {
        return fail(QStringLiteral("angle_mapping_invalid"),
                    QStringLiteral("标定文件的图像角度到机械角度映射无效"),
                    timer.elapsed());
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
                        QStringLiteral("无法判断输入点所在标定区域"),
                        timer.elapsed());
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

        double machineX = inputX;
        double machineY = inputY;
        transformPoint(forward, machineX, machineY, &machineX, &machineY);

        double affineImageAngle = 0.0;
        double derivedMachineAngle = 0.0;
        double unwrappedInputAngle = 0.0;
        double unwrappedMachineAngle = 0.0;
        double directionX = machineX;
        double directionY = machineY;
        CalibrationRotationCoverage rotationCoverage =
                CalibrationRotationCoverage::NotConfigured;
        if (poseMapping) {
            double transformedOriginX = 0.0;
            double transformedOriginY = 0.0;
            double transformedTipX = std::cos(radians(inputAngleDeg));
            double transformedTipY = std::sin(radians(inputAngleDeg));
            transformPoint(forward, transformedOriginX, transformedOriginY,
                           &transformedOriginX, &transformedOriginY);
            transformPoint(forward, transformedTipX, transformedTipY,
                           &transformedTipX, &transformedTipY);
            affineImageAngle = degrees(std::atan2(
                        transformedTipY - transformedOriginY,
                        transformedTipX - transformedOriginX));
            const double angleSign = model.angleMapping.direction
                    == CalibrationAngleDirection::SameSign ? 1.0 : -1.0;
            derivedMachineAngle = angleSign * affineImageAngle
                    + model.angleMapping.offsetDeg;

            const bool imageAngleInRange = angleInRange(
                        inputAngleDeg,
                        model.angleMapping.imageMinDeg,
                        model.angleMapping.imageMaxDeg,
                        model.angleMapping.imageCenterDeg,
                        model.angleMapping.periodDeg,
                        &unwrappedInputAngle);
            const bool machineAngleInRange = angleInRange(
                        derivedMachineAngle,
                        model.angleMapping.machineMinDeg,
                        model.angleMapping.machineMaxDeg,
                        model.angleMapping.machineCenterDeg,
                        model.angleMapping.periodDeg,
                        &unwrappedMachineAngle);
            rotationCoverage = imageAngleInRange && machineAngleInRange
                    ? CalibrationRotationCoverage::InRange
                    : CalibrationRotationCoverage::OutOfRange;
            derivedMachineAngle = unwrappedMachineAngle;
            directionX = machineX + std::cos(radians(derivedMachineAngle));
            directionY = machineY + std::sin(radians(derivedMachineAngle));
        }

        double rotationCorrectionX = 0.0;
        double rotationCorrectionY = 0.0;
        bool rotationCompensationApplied = false;
        if (poseMapping && !model.rotationRange.coaxial) {
            const double trajectorySign = model.rotationRange.direction
                    == CalibrationRotationDirection::SameSign ? 1.0 : -1.0;
            const double phase = radians(
                        trajectorySign * derivedMachineAngle
                        + model.rotationRange.phaseOffsetDeg);
            rotationCorrectionX = model.rotationRange.centerOffsetX
                    + model.rotationRange.radiusMm * std::cos(phase);
            rotationCorrectionY = model.rotationRange.centerOffsetY
                    + model.rotationRange.radiusMm * std::sin(phase);
            machineX -= rotationCorrectionX;
            machineY -= rotationCorrectionY;
            directionX -= rotationCorrectionX;
            directionY -= rotationCorrectionY;
            rotationCompensationApplied = true;
        }

        if (poseApplied) {
            const HTuple calibrationToRun = relativePose(effectiveCalibration,
                                                         effectiveRun);
            transformPoint(calibrationToRun, machineX, machineY,
                           &machineX, &machineY);
            if (poseMapping) {
                transformPoint(calibrationToRun, directionX, directionY,
                               &directionX, &directionY);
            }
        }

        const double outputAngle = poseMapping
                ? degrees(std::atan2(directionY - machineY,
                                     directionX - machineX))
                : 0.0;
        const double unwrappedOutputAngle = poseMapping
                ? angleNearCenter(outputAngle, derivedMachineAngle,
                                  model.angleMapping.periodDeg)
                : 0.0;
        const double scaleX = std::hypot(model.forward[0], model.forward[3]);
        const double scaleY = std::hypot(model.forward[1], model.forward[4]);

        CalibrationTransformHalconResult result;
        result.success = true;
        result.coordinateAvailable = true;
        result.region = region;
        result.rotationCoverage = rotationCoverage;
        result.angleVerified = poseMapping;
        result.productionAllowed = region == CalibrationRegion::Safe
                || (region == CalibrationRegion::Boundary
                    && allowBoundaryForProduction);
        if (region == CalibrationRegion::Extrapolation
                || (poseMapping
                    && rotationCoverage
                       != CalibrationRotationCoverage::InRange)) {
            result.productionAllowed = false;
        }
        result.angleProductionAllowed = result.productionAllowed
                && poseMapping
                && rotationCoverage == CalibrationRotationCoverage::InRange;

        if (region == CalibrationRegion::Extrapolation) {
            result.status = QStringLiteral("converted_extrapolation");
            result.message = QStringLiteral(
                        "输入像素坐标位于标定有效区域外，转换坐标仅供诊断");
        } else if (rotationCoverage == CalibrationRotationCoverage::OutOfRange) {
            result.status = QStringLiteral("angle_out_of_range");
            result.message = QStringLiteral(
                        "图像角度或映射机械角度超出标定覆盖范围，坐标仅供诊断");
        } else if (region == CalibrationRegion::Boundary) {
            result.status = QStringLiteral("converted_boundary");
            result.message = allowBoundaryForProduction
                    ? QStringLiteral("输入像素坐标位于边界警戒带，已按工具配置放行")
                    : QStringLiteral("输入像素坐标位于边界警戒带，默认禁止生产使用");
        } else {
            result.status = QStringLiteral("converted_safe");
            result.message = model.mode
                    == NPointCalibrationMode::TwelvePointAxisTrace
                    ? QStringLiteral("标定转换成功；轴轨迹仅作诊断，未执行动态补偿")
                    : QStringLiteral("标定转换成功");
        }

        result.outputX = machineX;
        result.outputY = machineY;
        result.outputAngleDeg = poseMapping ? unwrappedOutputAngle : 0.0;
        result.pixelAccuracy = (scaleX + scaleY) / 2.0;
        result.distanceToSafeBoundaryPx = distanceToSafeBoundary;
        result.distanceToValidBoundaryPx = distanceToValidBoundary;
        result.poseCompensationApplied = poseApplied;
        result.elapsedMs = timer.elapsed();

        const QString rotationModel = model.mode
                == NPointCalibrationMode::NinePointXY
                ? QStringLiteral("not_configured")
                : model.mode == NPointCalibrationMode::TwelvePointAxisTrace
                  ? QStringLiteral("axis_trace_diagnostic")
                  : model.rotationRange.coaxial
                    ? QStringLiteral("pose_mapping_coaxial")
                    : QStringLiteral("pose_mapping_eccentric_compensation");
        const QJsonValue noAngle(QJsonValue::Null);
        result.payload = QJsonObject{
            {QStringLiteral("calibrationMode"),
             nPointCalibrationModeToString(model.mode)},
            {QStringLiteral("inputCoordinateType"), inputCoordinateType},
            {QStringLiteral("inputPoint"), QJsonObject{
                 {QStringLiteral("x"), inputX},
                 {QStringLiteral("y"), inputY},
                 {QStringLiteral("angleDeg"),
                  poseMapping ? QJsonValue(inputAngleDeg) : noAngle}}},
            {QStringLiteral("convertedPoint"), QJsonObject{
                 {QStringLiteral("x"), machineX},
                 {QStringLiteral("y"), machineY}}},
            {QStringLiteral("convertedAngleDeg"),
             poseMapping ? QJsonValue(unwrappedOutputAngle) : noAngle},
            {QStringLiteral("affineImageAngleDeg"),
             poseMapping ? QJsonValue(affineImageAngle) : noAngle},
            {QStringLiteral("derivedMachineAngleDeg"),
             poseMapping ? QJsonValue(derivedMachineAngle) : noAngle},
            {QStringLiteral("pixelAccuracy"), result.pixelAccuracy},
            {QStringLiteral("pixelAccuracyUnit"),
             QStringLiteral("physical_per_pixel")},
            {QStringLiteral("calibrationId"), model.calibrationId},
            {QStringLiteral("methodId"), model.methodId},
            {QStringLiteral("calibrationRegion"),
             calibrationRegionToString(region)},
            {QStringLiteral("rotationCoverage"),
             calibrationRotationCoverageToString(rotationCoverage)},
            {QStringLiteral("angleValid"), poseMapping},
            {QStringLiteral("angleMappingStatus"),
             calibrationAngleMappingStatusToString(model.angleMapping.status)},
            {QStringLiteral("angleMappingDirection"),
             calibrationAngleDirectionToString(model.angleMapping.direction)},
            {QStringLiteral("angleMappingOffsetDeg"),
             model.angleMapping.offsetDeg},
            {QStringLiteral("angleMappingRmseDeg"),
             model.angleMapping.rmseDeg},
            {QStringLiteral("angleMappingMaxErrorDeg"),
             model.angleMapping.maxErrorDeg},
            {QStringLiteral("angleVerified"), result.angleVerified},
            {QStringLiteral("angleProductionAllowed"),
             result.angleProductionAllowed},
            {QStringLiteral("coordinateProductionAllowed"),
             result.productionAllowed},
            {QStringLiteral("rotationDirection"),
             calibrationRotationDirectionToString(
                 model.rotationRange.direction)},
            {QStringLiteral("rotationModel"), rotationModel},
            {QStringLiteral("axisTraceStatus"),
             calibrationRotationRangeStatusToString(
                 model.rotationRange.status)},
            {QStringLiteral("axisTraceVerified"),
             axisTraceConfigured && model.rotationRange.isVerified()},
            {QStringLiteral("rotationCompensationApplied"),
             rotationCompensationApplied},
            {QStringLiteral("rotationCoaxial"),
             model.rotationRange.coaxial},
            {QStringLiteral("rotationCenterOffsetX"),
             model.rotationRange.centerOffsetX},
            {QStringLiteral("rotationCenterOffsetY"),
             model.rotationRange.centerOffsetY},
            {QStringLiteral("rotationRadiusMm"),
             model.rotationRange.radiusMm},
            {QStringLiteral("rotationPhaseOffsetDeg"),
             model.rotationRange.phaseOffsetDeg},
            {QStringLiteral("rotationCorrectionX"), rotationCorrectionX},
            {QStringLiteral("rotationCorrectionY"), rotationCorrectionY},
            {QStringLiteral("rotationFitRmseMm"),
             model.rotationRange.fitRmseMm},
            {QStringLiteral("rotationMaxErrorMm"),
             model.rotationRange.maxErrorMm},
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
            {QStringLiteral("validRegionBoundaryTolerancePx"),
             kBoundaryTolerancePx},
            {QStringLiteral("safeRegionBoundaryTolerancePx"),
             kBoundaryTolerancePx},
            {QStringLiteral("inputAngleUnwrappedDeg"),
             poseMapping ? QJsonValue(unwrappedInputAngle) : noAngle},
            {QStringLiteral("outputAngleUnwrappedDeg"),
             poseMapping ? QJsonValue(unwrappedOutputAngle) : noAngle},
            {QStringLiteral("poseCompensationApplied"), poseApplied},
            {QStringLiteral("poseCompensationStatus"),
             poseApplied ? QStringLiteral("applied")
                         : QStringLiteral("disabled")},
            {QStringLiteral("elapsedMs"),
             static_cast<double>(result.elapsedMs)}
        };
        result.payload.insert(QStringLiteral("machineX"), machineX);
        result.payload.insert(QStringLiteral("machineY"), machineY);
        result.payload.insert(QStringLiteral("machineAngle"),
                              poseMapping
                              ? QJsonValue(unwrappedOutputAngle) : noAngle);
        return result;
    } catch (const HException &exception) {
        return fail(QStringLiteral("halcon_error"),
                    QStringLiteral("HALCON %1: %2")
                    .arg(static_cast<qlonglong>(exception.ErrorCode()))
                    .arg(QString::fromUtf8(exception.ErrorMessage().Text())),
                    timer.elapsed());
    }
}
