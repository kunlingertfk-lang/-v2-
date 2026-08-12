#ifndef CALIBRATION_CALIBRATIONMODEL_H
#define CALIBRATION_CALIBRATIONMODEL_H

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
    NotConfigured,
    InRange,
    OutOfRange,
    Unverified,
    Invalid
};

enum class NPointCalibrationMode
{
    NinePointXY,
    TwelvePointAxisTrace,
    TwelvePointPoseMapping
};

enum class CalibrationRotationRangeStatus
{
    NotConfigured,
    Verified,
    Invalid
};

// Axis-trace direction and pose-angle direction deliberately use different
// enums.  The former describes the eccentric point travelling around an axis;
// the latter describes ImageAngle -> MachineAngle handedness.
enum class CalibrationRotationDirection
{
    SameSign,
    OppositeSign,
    Unknown
};

enum class CalibrationAngleMappingStatus
{
    NotConfigured,
    Verified,
    Invalid
};

enum class CalibrationAngleDirection
{
    SameSign,
    OppositeSign,
    Unknown
};

enum class CalibrationTransformParity
{
    OrientationPreserving,
    OrientationReversing,
    Unknown
};

QString calibrationRegionToString(CalibrationRegion region);
QString calibrationRotationCoverageToString(CalibrationRotationCoverage coverage);
QString nPointCalibrationModeToString(NPointCalibrationMode mode);
bool nPointCalibrationModeFromString(const QString &text,
                                     NPointCalibrationMode *mode);
QString calibrationRotationRangeStatusToString(
        CalibrationRotationRangeStatus status);
bool calibrationRotationRangeStatusFromString(
        const QString &text,
        CalibrationRotationRangeStatus *status);
QString calibrationRotationDirectionToString(
        CalibrationRotationDirection direction);
bool calibrationRotationDirectionFromString(
        const QString &text,
        CalibrationRotationDirection *direction);
QString calibrationAngleMappingStatusToString(
        CalibrationAngleMappingStatus status);
bool calibrationAngleMappingStatusFromString(
        const QString &text,
        CalibrationAngleMappingStatus *status);
QString calibrationAngleDirectionToString(CalibrationAngleDirection direction);
bool calibrationAngleDirectionFromString(const QString &text,
                                         CalibrationAngleDirection *direction);
QString calibrationTransformParityToString(CalibrationTransformParity parity);
bool calibrationTransformParityFromString(
        const QString &text,
        CalibrationTransformParity *parity);

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

// Axis trajectory/eccentricity model.  It is a calibration-time model and
// diagnostic result only.  No runtime mechanical R, axis identity, zero
// signature, or three-party contract is part of this structure.
struct CalibrationRotationRange
{
    CalibrationRotationRangeStatus status =
            CalibrationRotationRangeStatus::NotConfigured;
    CalibrationRotationDirection direction =
            CalibrationRotationDirection::Unknown;
    int sampleCount = 0;
    double periodDeg = 360.0;
    bool coaxial = false;
    double trajectoryMinDeg = 0.0;
    double trajectoryMaxDeg = 0.0;
    double trajectoryCenterDeg = 0.0;
    double machineMinDeg = 0.0;
    double machineMaxDeg = 0.0;
    double machineCenterDeg = 0.0;
    double centerOffsetX = 0.0;
    double centerOffsetY = 0.0;
    double radiusMm = 0.0;
    double phaseOffsetDeg = 0.0;
    double fitRmseMm = 0.0;
    double maxErrorMm = 0.0;
    double minimumSpanDeg = 5.0;
    double rmseLimitMm = 0.10;
    double maxErrorLimitMm = 0.25;
    QString validationCode = QStringLiteral("not_configured");

    bool isFinite() const;
    bool isVerified() const
    {
        return status == CalibrationRotationRangeStatus::Verified;
    }
};

struct CalibrationAngleMapping
{
    CalibrationAngleMappingStatus status =
            CalibrationAngleMappingStatus::NotConfigured;
    CalibrationAngleDirection direction = CalibrationAngleDirection::Unknown;
    int sampleCount = 0;
    double offsetDeg = 0.0;
    double imageMinDeg = 0.0;
    double imageMaxDeg = 0.0;
    double imageCenterDeg = 0.0;
    double machineMinDeg = 0.0;
    double machineMaxDeg = 0.0;
    double machineCenterDeg = 0.0;
    double periodDeg = 360.0;
    double rmseDeg = 0.0;
    double maxErrorDeg = 0.0;
    double minimumSpanDeg = 5.0;
    double rmseLimitDeg = 1.0;
    double maxErrorLimitDeg = 2.0;
    QString validationCode = QStringLiteral("not_configured");

    bool isFinite() const;
    bool isVerified() const
    {
        return status == CalibrationAngleMappingStatus::Verified;
    }
};

struct CalibrationModel
{
    QString schemaVersion = QStringLiteral("1.4");
    NPointCalibrationMode mode = NPointCalibrationMode::NinePointXY;
    QString calibrationId;
    QString methodId = QStringLiteral("n_point");
    QString modelType = QStringLiteral("affine_2d");
    QString coordinateConvention =
            QStringLiteral("pixel_column_row_to_machine_xy");
    std::array<double, 6> forward{{0, 0, 0, 0, 0, 0}};
    std::array<double, 6> inverse{{0, 0, 0, 0, 0, 0}};
    QVector<CalibrationSample> samples;
    QVector<CalibrationSample> rotationSamples;
    int translationSampleCount = 0;
    int configuredRotationSampleCount = 0;
    int totalSampleCount = 0;
    CalibrationTransformParity transformParity =
            CalibrationTransformParity::Unknown;
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
    CalibrationAngleMapping angleMapping;
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

// Future extension only.
// May later host camera-on-motion, changing capture-pose, or external live-axis
// compensation.  The current fixed-camera N-point flow does not configure,
// serialize, or call this interface.
class DynamicMechanicalAxisCompensationExtension
{
public:
    virtual ~DynamicMechanicalAxisCompensationExtension() = default;
    virtual bool isSupported() const { return false; }
};

#endif // CALIBRATION_CALIBRATIONMODEL_H
