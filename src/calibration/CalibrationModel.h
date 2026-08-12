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

/// 将区域分类枚举编码为 payload/XML 使用的稳定字符串。
QString calibrationRegionToString(CalibrationRegion region);
/// 将运行角度覆盖枚举编码为稳定字符串。
QString calibrationRotationCoverageToString(CalibrationRotationCoverage coverage);
/// 将 N 点三模式枚举编码为 XML 使用的稳定字符串。
QString nPointCalibrationModeToString(NPointCalibrationMode mode);
/// 严格解析 N 点模式字符串，不接受未知值。
bool nPointCalibrationModeFromString(const QString &text,
                                     NPointCalibrationMode *mode);
/// 将轴轨迹状态编码为稳定字符串。
QString calibrationRotationRangeStatusToString(
        CalibrationRotationRangeStatus status);
/// 严格解析轴轨迹状态字符串。
bool calibrationRotationRangeStatusFromString(
        const QString &text,
        CalibrationRotationRangeStatus *status);
/// 将轴轨迹旋转方向编码为稳定字符串。
QString calibrationRotationDirectionToString(
        CalibrationRotationDirection direction);
/// 严格解析轴轨迹旋转方向字符串。
bool calibrationRotationDirectionFromString(
        const QString &text,
        CalibrationRotationDirection *direction);
/// 将姿态角映射状态编码为稳定字符串。
QString calibrationAngleMappingStatusToString(
        CalibrationAngleMappingStatus status);
/// 严格解析姿态角映射状态字符串。
bool calibrationAngleMappingStatusFromString(
        const QString &text,
        CalibrationAngleMappingStatus *status);
/// 将姿态角方向编码为稳定字符串。
QString calibrationAngleDirectionToString(CalibrationAngleDirection direction);
/// 严格解析姿态角方向字符串。
bool calibrationAngleDirectionFromString(const QString &text,
                                         CalibrationAngleDirection *direction);
/// 将二维仿射朝向奇偶性编码为稳定字符串。
QString calibrationTransformParityToString(CalibrationTransformParity parity);
/// 严格解析二维仿射朝向奇偶性字符串。
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

    /// 校验轴轨迹的角度、偏心、误差和门限均为有限合法数值。
    bool isFinite() const;
    /// 返回轴轨迹是否通过标定期诊断门禁。
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

    /// 校验姿态角映射的范围、偏置、误差和门限均为有限合法数值。
    bool isFinite() const;
    /// 返回 ImageAngle -> MachineAngle 映射是否通过门禁。
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

    /// 检查矩阵、样本、区域、误差和旋转模型中的所有数值是否有限且范围有效。
    bool isFinite() const;
    /// 依据仿射线性部分行列式判断正变换是否可逆。
    bool isInvertible(double epsilon = 1e-12) const;
    /// 校验 XML 1.4 模型结构、模式点数、区域、质量和旋转派生模型合同。
    bool isValid(QString *errorMessage = nullptr) const;
    /// 在 isValid 基础上要求 methodId 为当前已知值且质量门禁通过。
    bool isExecutable(QString *errorMessage = nullptr) const;
    /// 判断 methodId 是否属于注册表定义的已知标定方式。
    bool methodIsKnown() const;
    /// 返回正向仿射矩阵线性部分的行列式，用于可逆性及朝向判断。
    double determinant() const;
    /// 生成供 UI 展示和诊断的模型摘要，不替代完整 XML 持久化。
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
