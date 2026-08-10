#ifndef CALIBRATION_CALIBRATIONMODEL_H
#define CALIBRATION_CALIBRATIONMODEL_H

#include <QDateTime>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>

#include <array>

enum class CalibrationRegion
{
    Safe,
    Boundary,
    Extrapolation,
    Invalid
};

enum class CalibrationRotationCoverage
{
    InRange,
    OutOfRange,
    Unverified,
    Invalid
};

enum class CalibrationRotationRangeStatus
{
    Verified,
    Unverified,
    Invalid
};

QString calibrationRegionToString(CalibrationRegion region);
QString calibrationRotationCoverageToString(CalibrationRotationCoverage coverage);
QString calibrationRotationRangeStatusToString(CalibrationRotationRangeStatus status);
bool calibrationRotationRangeStatusFromString(
        const QString &text,
        CalibrationRotationRangeStatus *status);

struct CalibrationSample
{
    int index = 0;
    double column = 0.0;
    double row = 0.0;
    double machineX = 0.0;
    double machineY = 0.0;
    double imageAngleDeg = 0.0;
    double machineAngleDeg = 0.0;
    double residualX = 0.0;
    double residualY = 0.0;
    double residual = 0.0;
    QString source = QStringLiteral("manual");
    QString capturedAt;
};

struct CalibrationQuality
{
    double meanError = 0.0;
    double rmse = 0.0;
    double maxError = 0.0;
    double rmseX = 0.0;
    double rmseY = 0.0;
    double rmseLimit = 0.10;
    double maxErrorLimit = 0.25;
    bool passed = false;
};

struct CalibrationRotationRange
{
    CalibrationRotationRangeStatus status =
            CalibrationRotationRangeStatus::Unverified;
    double periodDeg = 360.0;
    double imageMinDeg = 0.0;
    double imageMaxDeg = 0.0;
    double imageCenterDeg = 0.0;
    double machineMinDeg = 0.0;
    double machineMaxDeg = 0.0;
    double machineCenterDeg = 0.0;

    bool isFinite() const;
    bool isVerified() const
    {
        return status == CalibrationRotationRangeStatus::Verified;
    }
};

struct CalibrationModel
{
    QString schemaVersion = QStringLiteral("1.1");
    QString calibrationId;
    QString methodId = QStringLiteral("n_point");
    QString modelType = QStringLiteral("affine_2d");
    QString coordinateConvention = QStringLiteral("pixel_column_row_to_machine_xy");
    std::array<double, 6> forward{{0, 0, 0, 0, 0, 0}};
    std::array<double, 6> inverse{{0, 0, 0, 0, 0, 0}};
    QVector<CalibrationSample> samples;
    QVector<QPointF> validRegion;
    QVector<QPointF> safeRegion;
    double safeMarginPx = 0.0;
    QString validRegionCoordinateSystem =
            QStringLiteral("image_pixel_column_row");
    QString validRegionType = QStringLiteral("convex_hull");
    QString safeRegionCoordinateSystem =
            QStringLiteral("image_pixel_column_row");
    QString safeRegionSource = QStringLiteral("validRegion");
    QString safeRegionType = QStringLiteral("uniform_inset");
    CalibrationRotationRange rotationRange;
    CalibrationQuality quality;
    QJsonObject imageBinding;
    QJsonObject methodData;
    QString createdAt;
    QString checksum;

    bool isFinite() const;
    bool isInvertible(double epsilon = 1e-12) const;
    bool isValid(QString *errorMessage = nullptr) const;
    bool isExecutable(QString *errorMessage = nullptr) const;
    bool methodIsKnown() const;
    double determinant() const;
    QJsonObject summaryJson() const;
};

#endif // CALIBRATION_CALIBRATIONMODEL_H
