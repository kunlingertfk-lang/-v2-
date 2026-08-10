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

QString calibrationRotationRangeStatusToString(
        CalibrationRotationRangeStatus status)
{
    switch (status) {
    case CalibrationRotationRangeStatus::Verified:
        return QStringLiteral("verified");
    case CalibrationRotationRangeStatus::Unverified:
        return QStringLiteral("unverified");
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
    if (normalized == QStringLiteral("verified")) {
        *status = CalibrationRotationRangeStatus::Verified;
        return true;
    }
    if (normalized == QStringLiteral("unverified")) {
        *status = CalibrationRotationRangeStatus::Unverified;
        return true;
    }
    if (normalized == QStringLiteral("invalid")) {
        *status = CalibrationRotationRangeStatus::Invalid;
        return true;
    }
    return false;
}

bool CalibrationRotationRange::isFinite() const
{
    return std::isfinite(periodDeg) && periodDeg > 0.0
            && std::isfinite(imageMinDeg) && std::isfinite(imageMaxDeg)
            && std::isfinite(imageCenterDeg)
            && std::isfinite(machineMinDeg) && std::isfinite(machineMaxDeg)
            && std::isfinite(machineCenterDeg);
}

bool CalibrationModel::isFinite() const
{
    if (!finiteArray(forward) || !finiteArray(inverse))
        return false;
    for (const CalibrationSample &sample : samples) {
        if (!std::isfinite(sample.column) || !std::isfinite(sample.row)
                || !std::isfinite(sample.machineX) || !std::isfinite(sample.machineY)
                || !std::isfinite(sample.imageAngleDeg)
                || !std::isfinite(sample.machineAngleDeg)
                || !std::isfinite(sample.residualX)
                || !std::isfinite(sample.residualY)
                || !std::isfinite(sample.residual)) {
            return false;
        }
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
            && rotationRange.isFinite()
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
    if (schemaVersion != QStringLiteral("1.0")
            && schemaVersion != QStringLiteral("1.1")) {
        return fail(QStringLiteral("unsupported schema version: %1").arg(schemaVersion));
    }
    if (calibrationId.trimmed().isEmpty())
        return fail(QStringLiteral("calibrationId is empty"));
    if (modelType != QStringLiteral("affine_2d"))
        return fail(QStringLiteral("unsupported model type: %1").arg(modelType));
    if (!std::isfinite(safeMarginPx) || safeMarginPx < 0.0
            || safeMarginPx > 511.0) {
        return fail(QStringLiteral(
                        "safe region margin must be within 0..511 px"));
    }
    if (!isFinite())
        return fail(QStringLiteral("calibration contains non-finite values"));
    if (!isInvertible())
        return fail(QStringLiteral("calibration transform is singular"));
    if (!regionStructureValid(validRegion))
        return fail(QStringLiteral("validRegion is missing or degenerate"));
    if (!regionStructureValid(safeRegion))
        return fail(QStringLiteral("safeRegion is missing or degenerate"));
    if (validRegionCoordinateSystem != QStringLiteral("image_pixel_column_row")
            || safeRegionCoordinateSystem != QStringLiteral("image_pixel_column_row")) {
        return fail(QStringLiteral("unsupported calibration region coordinate system"));
    }
    if (validRegionType != QStringLiteral("convex_hull")
            || safeRegionType != QStringLiteral("uniform_inset")
            || safeRegionSource != QStringLiteral("validRegion")) {
        return fail(QStringLiteral("unsupported calibration region generation metadata"));
    }
    if (!safeRegionBelongsToValidRegion(validRegion, safeRegion))
        return fail(QStringLiteral("safeRegion is not contained by validRegion"));
    if (std::abs(rotationRange.periodDeg - 360.0) > 1e-9)
        return fail(QStringLiteral("rotation period must be 360 degrees"));
    if (rotationRange.status == CalibrationRotationRangeStatus::Verified) {
        constexpr double kRangeTolerance = 1e-9;
        const double imageSpan = rotationRange.imageMaxDeg
                - rotationRange.imageMinDeg;
        const double machineSpan = rotationRange.machineMaxDeg
                - rotationRange.machineMinDeg;
        if (imageSpan <= kRangeTolerance
                || machineSpan <= kRangeTolerance
                || imageSpan > rotationRange.periodDeg + kRangeTolerance
                || machineSpan > rotationRange.periodDeg + kRangeTolerance
                || std::abs(rotationRange.imageCenterDeg
                            - (rotationRange.imageMinDeg
                               + rotationRange.imageMaxDeg) * 0.5)
                    > kRangeTolerance
                || std::abs(rotationRange.machineCenterDeg
                            - (rotationRange.machineMinDeg
                               + rotationRange.machineMaxDeg) * 0.5)
                    > kRangeTolerance) {
            return fail(QStringLiteral(
                            "verified rotation range is degenerate, exceeds one period, or is inconsistent"));
        }
    }
    return true;
}

bool CalibrationModel::isExecutable(QString *errorMessage) const
{
    if (!isValid(errorMessage))
        return false;
    if (!methodIsKnown()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("unknown calibration method: %1").arg(methodId);
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
        {QStringLiteral("calibrationId"), calibrationId},
        {QStringLiteral("methodId"), methodId},
        {QStringLiteral("modelType"), modelType},
        {QStringLiteral("forwardTransform"), forwardJson},
        {QStringLiteral("inverseTransform"), inverseJson},
        {QStringLiteral("fitRmseMm"), quality.rmse},
        {QStringLiteral("fitMaxErrorMm"), quality.maxError},
        {QStringLiteral("qualityPassed"), quality.passed},
        {QStringLiteral("validRegionPointCount"), validRegion.size()},
        {QStringLiteral("safeRegionPointCount"), safeRegion.size()},
        {QStringLiteral("safeMarginPx"), safeMarginPx},
        {QStringLiteral("rotationRangeStatus"),
         calibrationRotationRangeStatusToString(rotationRange.status)},
        {QStringLiteral("rotationImageMinDeg"), rotationRange.imageMinDeg},
        {QStringLiteral("rotationImageMaxDeg"), rotationRange.imageMaxDeg},
        {QStringLiteral("rotationMachineMinDeg"), rotationRange.machineMinDeg},
        {QStringLiteral("rotationMachineMaxDeg"), rotationRange.machineMaxDeg},
        {QStringLiteral("createdAt"), createdAt}
    };
}
