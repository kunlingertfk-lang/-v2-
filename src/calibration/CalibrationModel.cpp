#include "calibration/CalibrationModel.h"

#include <QJsonArray>
#include <QPolygonF>

#include <cmath>

namespace {

bool finiteArray(const std::array<double, 6> &values)
{
    for (double value : values) {
        if (!std::isfinite(value))
            return false;
    }
    return true;
}

bool finitePoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

bool finiteSample(const CalibrationSample &sample)
{
    return std::isfinite(sample.column) && std::isfinite(sample.row)
            && std::isfinite(sample.machineX) && std::isfinite(sample.machineY)
            && std::isfinite(sample.imageAngleDeg)
            && std::isfinite(sample.machineAngleDeg)
            && std::isfinite(sample.residualX)
            && std::isfinite(sample.residualY)
            && std::isfinite(sample.residual);
}

bool sequentialIndexes(const QVector<CalibrationSample> &samples,
                       int firstIndex)
{
    for (int index = 0; index < samples.size(); ++index) {
        if (samples.at(index).index != firstIndex + index)
            return false;
    }
    return true;
}

double signedArea(const QVector<QPointF> &polygon)
{
    double twiceArea = 0.0;
    for (int index = 0; index < polygon.size(); ++index) {
        const QPointF &a = polygon.at(index);
        const QPointF &b = polygon.at((index + 1) % polygon.size());
        twiceArea += a.x() * b.y() - b.x() * a.y();
    }
    return twiceArea * 0.5;
}

bool pointOnSegment(const QPointF &point,
                    const QPointF &a,
                    const QPointF &b,
                    double tolerance = 1e-7)
{
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double length = std::hypot(dx, dy);
    if (length <= tolerance)
        return std::hypot(point.x() - a.x(), point.y() - a.y()) <= tolerance;
    const double cross = (point.x() - a.x()) * dy
            - (point.y() - a.y()) * dx;
    if (std::abs(cross) > tolerance * length)
        return false;
    const double dot = (point.x() - a.x()) * dx
            + (point.y() - a.y()) * dy;
    return dot >= -tolerance && dot <= length * length + tolerance;
}

bool containsPointInclusive(const QVector<QPointF> &polygon,
                            const QPointF &point)
{
    for (int index = 0; index < polygon.size(); ++index) {
        if (pointOnSegment(point, polygon.at(index),
                           polygon.at((index + 1) % polygon.size()))) {
            return true;
        }
    }
    return QPolygonF(polygon).containsPoint(point, Qt::OddEvenFill);
}

bool regionStructureValid(const QVector<QPointF> &polygon)
{
    if (polygon.size() < 3 || std::abs(signedArea(polygon)) <= 1e-9)
        return false;
    constexpr double kDuplicatePointTolerance = 1e-9;
    for (int index = 0; index < polygon.size(); ++index) {
        if (!finitePoint(polygon.at(index)))
            return false;
        for (int other = index + 1; other < polygon.size(); ++other) {
            if (std::hypot(polygon.at(index).x() - polygon.at(other).x(),
                           polygon.at(index).y() - polygon.at(other).y())
                    <= kDuplicatePointTolerance) {
                return false;
            }
        }
    }
    return true;
}

bool safeRegionBelongsToValidRegion(const QVector<QPointF> &validRegion,
                                    const QVector<QPointF> &safeRegion)
{
    for (const QPointF &point : safeRegion) {
        if (!containsPointInclusive(validRegion, point))
            return false;
    }
    return true;
}

bool jsonCountMatches(const QJsonObject &object,
                      const QString &key,
                      int expected)
{
    if (!object.contains(key))
        return true;
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    return std::isfinite(number) && std::floor(number) == number
            && number == static_cast<double>(expected);
}

} // namespace

QString calibrationRegionToString(CalibrationRegion region)
{
    switch (region) {
    case CalibrationRegion::Safe:
        return QStringLiteral("safe");
    case CalibrationRegion::Boundary:
        return QStringLiteral("boundary");
    case CalibrationRegion::Extrapolation:
        return QStringLiteral("extrapolation");
    case CalibrationRegion::Invalid:
        return QStringLiteral("invalid");
    }
    return QStringLiteral("invalid");
}

QString calibrationRotationCoverageToString(CalibrationRotationCoverage coverage)
{
    switch (coverage) {
    case CalibrationRotationCoverage::NotConfigured:
        return QStringLiteral("not_configured");
    case CalibrationRotationCoverage::InRange:
        return QStringLiteral("in_range");
    case CalibrationRotationCoverage::OutOfRange:
        return QStringLiteral("out_of_range");
    case CalibrationRotationCoverage::Unverified:
        return QStringLiteral("unverified");
    case CalibrationRotationCoverage::Invalid:
        return QStringLiteral("invalid");
    }
    return QStringLiteral("invalid");
}

QString nPointCalibrationModeToString(NPointCalibrationMode mode)
{
    switch (mode) {
    case NPointCalibrationMode::NinePointXY:
        return QStringLiteral("nine_point_xy");
    case NPointCalibrationMode::TwelvePointAxisTrace:
        return QStringLiteral("twelve_point_axis_trace");
    case NPointCalibrationMode::TwelvePointPoseMapping:
        return QStringLiteral("twelve_point_pose_mapping");
    }
    return QStringLiteral("nine_point_xy");
}

bool nPointCalibrationModeFromString(const QString &text,
                                     NPointCalibrationMode *mode)
{
    if (!mode)
        return false;
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("nine_point_xy")) {
        *mode = NPointCalibrationMode::NinePointXY;
        return true;
    }
    if (normalized == QStringLiteral("twelve_point_axis_trace")) {
        *mode = NPointCalibrationMode::TwelvePointAxisTrace;
        return true;
    }
    if (normalized == QStringLiteral("twelve_point_pose_mapping")) {
        *mode = NPointCalibrationMode::TwelvePointPoseMapping;
        return true;
    }
    return false;
}

QString calibrationRotationRangeStatusToString(
        CalibrationRotationRangeStatus status)
{
    switch (status) {
    case CalibrationRotationRangeStatus::NotConfigured:
        return QStringLiteral("not_configured");
    case CalibrationRotationRangeStatus::Verified:
        return QStringLiteral("verified");
    case CalibrationRotationRangeStatus::Invalid:
        return QStringLiteral("invalid");
    }
    return QStringLiteral("invalid");
}

bool calibrationRotationRangeStatusFromString(
        const QString &text,
        CalibrationRotationRangeStatus *status)
{
    if (!status)
        return false;
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("not_configured")) {
        *status = CalibrationRotationRangeStatus::NotConfigured;
        return true;
    }
    if (normalized == QStringLiteral("verified")) {
        *status = CalibrationRotationRangeStatus::Verified;
        return true;
    }
    if (normalized == QStringLiteral("invalid")) {
        *status = CalibrationRotationRangeStatus::Invalid;
        return true;
    }
    return false;
}

QString calibrationRotationDirectionToString(
        CalibrationRotationDirection direction)
{
    switch (direction) {
    case CalibrationRotationDirection::SameSign:
        return QStringLiteral("same_sign");
    case CalibrationRotationDirection::OppositeSign:
        return QStringLiteral("opposite_sign");
    case CalibrationRotationDirection::Unknown:
        return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

bool calibrationRotationDirectionFromString(
        const QString &text,
        CalibrationRotationDirection *direction)
{
    if (!direction)
        return false;
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("same_sign")) {
        *direction = CalibrationRotationDirection::SameSign;
        return true;
    }
    if (normalized == QStringLiteral("opposite_sign")) {
        *direction = CalibrationRotationDirection::OppositeSign;
        return true;
    }
    if (normalized == QStringLiteral("unknown")) {
        *direction = CalibrationRotationDirection::Unknown;
        return true;
    }
    return false;
}

QString calibrationAngleMappingStatusToString(
        CalibrationAngleMappingStatus status)
{
    switch (status) {
    case CalibrationAngleMappingStatus::NotConfigured:
        return QStringLiteral("not_configured");
    case CalibrationAngleMappingStatus::Verified:
        return QStringLiteral("verified");
    case CalibrationAngleMappingStatus::Invalid:
        return QStringLiteral("invalid");
    }
    return QStringLiteral("invalid");
}

bool calibrationAngleMappingStatusFromString(
        const QString &text,
        CalibrationAngleMappingStatus *status)
{
    if (!status)
        return false;
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("not_configured")) {
        *status = CalibrationAngleMappingStatus::NotConfigured;
        return true;
    }
    if (normalized == QStringLiteral("verified")) {
        *status = CalibrationAngleMappingStatus::Verified;
        return true;
    }
    if (normalized == QStringLiteral("invalid")) {
        *status = CalibrationAngleMappingStatus::Invalid;
        return true;
    }
    return false;
}

QString calibrationAngleDirectionToString(CalibrationAngleDirection direction)
{
    switch (direction) {
    case CalibrationAngleDirection::SameSign:
        return QStringLiteral("same_sign");
    case CalibrationAngleDirection::OppositeSign:
        return QStringLiteral("opposite_sign");
    case CalibrationAngleDirection::Unknown:
        return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

bool calibrationAngleDirectionFromString(
        const QString &text,
        CalibrationAngleDirection *direction)
{
    if (!direction)
        return false;
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("same_sign")) {
        *direction = CalibrationAngleDirection::SameSign;
        return true;
    }
    if (normalized == QStringLiteral("opposite_sign")) {
        *direction = CalibrationAngleDirection::OppositeSign;
        return true;
    }
    if (normalized == QStringLiteral("unknown")) {
        *direction = CalibrationAngleDirection::Unknown;
        return true;
    }
    return false;
}

QString calibrationTransformParityToString(CalibrationTransformParity parity)
{
    switch (parity) {
    case CalibrationTransformParity::OrientationPreserving:
        return QStringLiteral("orientation_preserving");
    case CalibrationTransformParity::OrientationReversing:
        return QStringLiteral("orientation_reversing");
    case CalibrationTransformParity::Unknown:
        return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

bool calibrationTransformParityFromString(
        const QString &text,
        CalibrationTransformParity *parity)
{
    if (!parity)
        return false;
    const QString normalized = text.trimmed().toLower();
    if (normalized == QStringLiteral("orientation_preserving")) {
        *parity = CalibrationTransformParity::OrientationPreserving;
        return true;
    }
    if (normalized == QStringLiteral("orientation_reversing")) {
        *parity = CalibrationTransformParity::OrientationReversing;
        return true;
    }
    if (normalized == QStringLiteral("unknown")) {
        *parity = CalibrationTransformParity::Unknown;
        return true;
    }
    return false;
}

bool CalibrationRotationRange::isFinite() const
{
    return std::isfinite(periodDeg) && periodDeg > 0.0
            && std::isfinite(trajectoryMinDeg)
            && std::isfinite(trajectoryMaxDeg)
            && std::isfinite(trajectoryCenterDeg)
            && std::isfinite(machineMinDeg) && std::isfinite(machineMaxDeg)
            && std::isfinite(machineCenterDeg)
            && std::isfinite(centerOffsetX) && std::isfinite(centerOffsetY)
            && std::isfinite(radiusMm) && radiusMm >= 0.0
            && std::isfinite(phaseOffsetDeg)
            && std::isfinite(fitRmseMm) && fitRmseMm >= 0.0
            && std::isfinite(maxErrorMm) && maxErrorMm >= 0.0
            && std::isfinite(minimumSpanDeg) && minimumSpanDeg > 0.0
            && std::isfinite(rmseLimitMm) && rmseLimitMm > 0.0
            && std::isfinite(maxErrorLimitMm) && maxErrorLimitMm > 0.0;
}

bool CalibrationAngleMapping::isFinite() const
{
    return std::isfinite(offsetDeg)
            && std::isfinite(imageMinDeg) && std::isfinite(imageMaxDeg)
            && std::isfinite(imageCenterDeg)
            && std::isfinite(machineMinDeg) && std::isfinite(machineMaxDeg)
            && std::isfinite(machineCenterDeg)
            && std::isfinite(periodDeg) && periodDeg > 0.0
            && std::isfinite(rmseDeg) && rmseDeg >= 0.0
            && std::isfinite(maxErrorDeg) && maxErrorDeg >= 0.0
            && std::isfinite(minimumSpanDeg) && minimumSpanDeg > 0.0
            && std::isfinite(rmseLimitDeg) && rmseLimitDeg > 0.0
            && std::isfinite(maxErrorLimitDeg) && maxErrorLimitDeg > 0.0;
}

bool CalibrationModel::isFinite() const
{
    if (!finiteArray(forward) || !finiteArray(inverse))
        return false;
    for (const CalibrationSample &sample : samples) {
        if (!finiteSample(sample))
            return false;
    }
    for (const CalibrationSample &sample : rotationSamples) {
        if (!finiteSample(sample))
            return false;
    }
    for (const QPointF &point : validRegion) {
        if (!finitePoint(point))
            return false;
    }
    for (const QPointF &point : safeRegion) {
        if (!finitePoint(point))
            return false;
    }
    return std::isfinite(safeMarginPx) && safeMarginPx >= 0.0
            && rotationRange.isFinite() && angleMapping.isFinite()
            && std::isfinite(quality.meanError) && std::isfinite(quality.rmse)
            && std::isfinite(quality.maxError)
            && std::isfinite(quality.rmseX) && std::isfinite(quality.rmseY)
            && std::isfinite(quality.rmseLimit)
            && std::isfinite(quality.maxErrorLimit)
            && quality.rmseLimit > 0.0 && quality.maxErrorLimit > 0.0;
}

double CalibrationModel::determinant() const
{
    return forward[0] * forward[4] - forward[1] * forward[3];
}

bool CalibrationModel::isInvertible(double epsilon) const
{
    return std::isfinite(determinant()) && std::abs(determinant()) > epsilon;
}

bool CalibrationModel::methodIsKnown() const
{
    return methodId == QStringLiteral("n_point")
            || methodId == QStringLiteral("translation_rotation")
            || methodId == QStringLiteral("calibration_board");
}

bool CalibrationModel::isValid(QString *errorMessage) const
{
    auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
            *errorMessage = message;
        return false;
    };
    if (schemaVersion != QStringLiteral("1.4")) {
        return fail(QStringLiteral("unsupported_calibration_schema: only schemaVersion=1.4 is supported"));
    }
    if (calibrationId.trimmed().isEmpty())
        return fail(QStringLiteral("calibrationId is empty"));
    if (createdAt.trimmed().isEmpty())
        return fail(QStringLiteral("createdAt is empty"));
    if (modelType != QStringLiteral("affine_2d"))
        return fail(QStringLiteral("unsupported model type: %1").arg(modelType));
    if (coordinateConvention
            != QStringLiteral("pixel_column_row_to_machine_xy")) {
        return fail(QStringLiteral("unsupported coordinate convention: %1")
                    .arg(coordinateConvention));
    }
    if (!std::isfinite(safeMarginPx) || safeMarginPx < 0.0
            || safeMarginPx > 511.0) {
        return fail(QStringLiteral("safe region margin must be within 0..511 px"));
    }
    if (!isFinite())
        return fail(QStringLiteral("calibration contains non-finite values"));
    if (!isInvertible())
        return fail(QStringLiteral("calibration transform is singular"));

    const CalibrationTransformParity expectedParity = determinant() > 0.0
            ? CalibrationTransformParity::OrientationPreserving
            : CalibrationTransformParity::OrientationReversing;
    if (transformParity != expectedParity) {
        return fail(QStringLiteral(
                        "orientation parity does not match the affine determinant"));
    }
    if (translationSampleCount != samples.size()
            || configuredRotationSampleCount != rotationSamples.size()
            || totalSampleCount
               != translationSampleCount + configuredRotationSampleCount) {
        return fail(QStringLiteral("sample summary does not match stored samples"));
    }
    if ((methodData.contains(QStringLiteral("calibrationMode"))
         && methodData.value(QStringLiteral("calibrationMode")).toString()
            != nPointCalibrationModeToString(mode))
            || !jsonCountMatches(methodData,
                                 QStringLiteral("translationCount"),
                                 translationSampleCount)
            || !jsonCountMatches(methodData,
                                 QStringLiteral("rotationCount"),
                                 configuredRotationSampleCount)
            || !jsonCountMatches(methodData,
                                 QStringLiteral("sampleCount"),
                                 totalSampleCount)
            || !jsonCountMatches(methodData,
                                 QStringLiteral("translationSampleCount"),
                                 translationSampleCount)
            || !jsonCountMatches(methodData,
                                 QStringLiteral("rotationSampleCount"),
                                 configuredRotationSampleCount)
            || !jsonCountMatches(methodData,
                                 QStringLiteral("totalSampleCount"),
                                 totalSampleCount)) {
        return fail(QStringLiteral(
                        "methodData mode or sample counts are inconsistent"));
    }
    if (!sequentialIndexes(samples, 1)
            || !sequentialIndexes(rotationSamples,
                                  translationSampleCount + 1)) {
        return fail(QStringLiteral(
                        "N-point sample indexes must be unique and continuous"));
    }

    int expectedRotationCount = 0;
    switch (mode) {
    case NPointCalibrationMode::NinePointXY:
        expectedRotationCount = 0;
        break;
    case NPointCalibrationMode::TwelvePointAxisTrace:
    case NPointCalibrationMode::TwelvePointPoseMapping:
        expectedRotationCount = 3;
        break;
    }
    if (translationSampleCount != 9
            || configuredRotationSampleCount != expectedRotationCount
            || totalSampleCount != 9 + expectedRotationCount) {
        return fail(QStringLiteral(
                        "calibration mode and strict 9+0/9+3 sample counts are inconsistent"));
    }

    if (!regionStructureValid(validRegion))
        return fail(QStringLiteral("validRegion is missing or degenerate"));
    if (!regionStructureValid(safeRegion))
        return fail(QStringLiteral("safeRegion is missing or degenerate"));
    if (validRegionCoordinateSystem != QStringLiteral("image_pixel_column_row")
            || safeRegionCoordinateSystem
               != QStringLiteral("image_pixel_column_row")) {
        return fail(QStringLiteral(
                        "unsupported calibration region coordinate system"));
    }
    if (validRegionType != QStringLiteral("convex_hull")
            || safeRegionType != QStringLiteral("uniform_inset")
            || safeRegionSource != QStringLiteral("validRegion")) {
        return fail(QStringLiteral(
                        "unsupported calibration region generation metadata"));
    }
    if (!safeRegionBelongsToValidRegion(validRegion, safeRegion))
        return fail(QStringLiteral("safeRegion is not contained by validRegion"));

    constexpr double kTolerance = 1e-9;
    if (std::abs(rotationRange.periodDeg - 360.0) > kTolerance
            || std::abs(angleMapping.periodDeg - 360.0) > kTolerance) {
        return fail(QStringLiteral("rotation and angle periods must be 360 degrees"));
    }

    const bool needsAxisTrace = mode != NPointCalibrationMode::NinePointXY;
    if (!needsAxisTrace) {
        if (rotationRange.status
                != CalibrationRotationRangeStatus::NotConfigured
                || rotationRange.sampleCount != 0) {
            return fail(QStringLiteral(
                            "axis trace must be not_configured for nine-point XY"));
        }
    } else {
        if (rotationRange.status
                != CalibrationRotationRangeStatus::Verified
                || rotationRange.sampleCount != 3) {
            return fail(QStringLiteral(
                            "12-point calibration requires a verified 3-sample axis trace"));
        }
        const double machineSpan = rotationRange.machineMaxDeg
                - rotationRange.machineMinDeg;
        if (machineSpan < rotationRange.minimumSpanDeg - kTolerance
                || machineSpan > rotationRange.periodDeg + kTolerance
                || rotationRange.fitRmseMm
                   > rotationRange.rmseLimitMm + kTolerance
                || rotationRange.maxErrorMm
                   > rotationRange.maxErrorLimitMm + kTolerance
                || std::abs(rotationRange.machineCenterDeg
                            - (rotationRange.machineMinDeg
                               + rotationRange.machineMaxDeg) * 0.5)
                   > kTolerance) {
            return fail(QStringLiteral(
                            "verified axis trace is incomplete, over limit, or inconsistent"));
        }
        if (!rotationRange.coaxial) {
            const double trajectorySpan = rotationRange.trajectoryMaxDeg
                    - rotationRange.trajectoryMinDeg;
            if (rotationRange.direction == CalibrationRotationDirection::Unknown
                    || trajectorySpan
                       < rotationRange.minimumSpanDeg - kTolerance
                    || trajectorySpan
                       > rotationRange.periodDeg + kTolerance
                    || rotationRange.radiusMm <= kTolerance
                    || std::abs(rotationRange.trajectoryCenterDeg
                                - (rotationRange.trajectoryMinDeg
                                   + rotationRange.trajectoryMaxDeg) * 0.5)
                       > kTolerance) {
                return fail(QStringLiteral(
                                "verified eccentric axis trace is incomplete or inconsistent"));
            }
        }
    }

    const bool needsAngleMapping =
            mode == NPointCalibrationMode::TwelvePointPoseMapping;
    if (!needsAngleMapping) {
        if (angleMapping.status
                != CalibrationAngleMappingStatus::NotConfigured
                || angleMapping.sampleCount != 0) {
            return fail(QStringLiteral(
                            "angle mapping must be not_configured for this mode"));
        }
    } else {
        if (angleMapping.status != CalibrationAngleMappingStatus::Verified
                || angleMapping.direction == CalibrationAngleDirection::Unknown
                || angleMapping.sampleCount != 3) {
            return fail(QStringLiteral(
                            "pose mapping requires a verified 3-sample angle mapping"));
        }
        const double imageSpan = angleMapping.imageMaxDeg
                - angleMapping.imageMinDeg;
        const double machineSpan = angleMapping.machineMaxDeg
                - angleMapping.machineMinDeg;
        if (imageSpan < angleMapping.minimumSpanDeg - kTolerance
                || machineSpan < angleMapping.minimumSpanDeg - kTolerance
                || imageSpan > angleMapping.periodDeg + kTolerance
                || machineSpan > angleMapping.periodDeg + kTolerance
                || angleMapping.rmseDeg
                   > angleMapping.rmseLimitDeg + kTolerance
                || angleMapping.maxErrorDeg
                   > angleMapping.maxErrorLimitDeg + kTolerance
                || std::abs(angleMapping.imageCenterDeg
                            - (angleMapping.imageMinDeg
                               + angleMapping.imageMaxDeg) * 0.5)
                   > kTolerance
                || std::abs(angleMapping.machineCenterDeg
                            - (angleMapping.machineMinDeg
                               + angleMapping.machineMaxDeg) * 0.5)
                   > kTolerance) {
            return fail(QStringLiteral(
                            "verified angle mapping is incomplete, over limit, or inconsistent"));
        }
    }
    return true;
}

bool CalibrationModel::isExecutable(QString *errorMessage) const
{
    if (!isValid(errorMessage))
        return false;
    if (!methodIsKnown()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unknown calibration method: %1")
                    .arg(methodId);
        }
        return false;
    }
    if (!quality.passed) {
        if (errorMessage)
            *errorMessage = QStringLiteral("calibration quality gate did not pass");
        return false;
    }
    return true;
}

QJsonObject CalibrationModel::summaryJson() const
{
    QJsonArray forwardJson;
    QJsonArray inverseJson;
    for (double value : forward)
        forwardJson.append(value);
    for (double value : inverse)
        inverseJson.append(value);
    return QJsonObject{
        {QStringLiteral("schemaVersion"), schemaVersion},
        {QStringLiteral("calibrationMode"),
         nPointCalibrationModeToString(mode)},
        {QStringLiteral("calibrationId"), calibrationId},
        {QStringLiteral("methodId"), methodId},
        {QStringLiteral("modelType"), modelType},
        {QStringLiteral("translationSampleCount"), translationSampleCount},
        {QStringLiteral("rotationSampleCount"),
         configuredRotationSampleCount},
        {QStringLiteral("totalSampleCount"), totalSampleCount},
        {QStringLiteral("transformParity"),
         calibrationTransformParityToString(transformParity)},
        {QStringLiteral("forwardTransform"), forwardJson},
        {QStringLiteral("inverseTransform"), inverseJson},
        {QStringLiteral("fitRmseMm"), quality.rmse},
        {QStringLiteral("fitMaxErrorMm"), quality.maxError},
        {QStringLiteral("qualityPassed"), quality.passed},
        {QStringLiteral("validRegionPointCount"), validRegion.size()},
        {QStringLiteral("safeRegionPointCount"), safeRegion.size()},
        {QStringLiteral("safeMarginPx"), safeMarginPx},
        {QStringLiteral("axisTraceStatus"),
         calibrationRotationRangeStatusToString(rotationRange.status)},
        {QStringLiteral("axisTraceDirection"),
         calibrationRotationDirectionToString(rotationRange.direction)},
        {QStringLiteral("axisTraceCoaxial"), rotationRange.coaxial},
        {QStringLiteral("rotationCenterOffsetX"),
         rotationRange.centerOffsetX},
        {QStringLiteral("rotationCenterOffsetY"),
         rotationRange.centerOffsetY},
        {QStringLiteral("rotationRadiusMm"), rotationRange.radiusMm},
        {QStringLiteral("rotationPhaseOffsetDeg"),
         rotationRange.phaseOffsetDeg},
        {QStringLiteral("rotationFitRmseMm"), rotationRange.fitRmseMm},
        {QStringLiteral("rotationMaxErrorMm"), rotationRange.maxErrorMm},
        {QStringLiteral("angleMappingStatus"),
         calibrationAngleMappingStatusToString(angleMapping.status)},
        {QStringLiteral("angleDirection"),
         calibrationAngleDirectionToString(angleMapping.direction)},
        {QStringLiteral("angleOffsetDeg"), angleMapping.offsetDeg},
        {QStringLiteral("angleImageMinDeg"), angleMapping.imageMinDeg},
        {QStringLiteral("angleImageMaxDeg"), angleMapping.imageMaxDeg},
        {QStringLiteral("angleMachineMinDeg"), angleMapping.machineMinDeg},
        {QStringLiteral("angleMachineMaxDeg"), angleMapping.machineMaxDeg},
        {QStringLiteral("angleFitRmseDeg"), angleMapping.rmseDeg},
        {QStringLiteral("angleMaxErrorDeg"), angleMapping.maxErrorDeg},
        {QStringLiteral("createdAt"), createdAt}
    };
}
