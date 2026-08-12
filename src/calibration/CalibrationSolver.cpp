#include "calibration/CalibrationSolver.h"

#include <HalconCpp.h>

#include <QDateTime>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace {

using namespace HalconCpp;

class HalconThreadClipRegionGuard
{
public:
    HalconThreadClipRegionGuard()
    {
        GetSystem(HTuple("tsp_clip_region"), &m_previousValue);
        SetSystem(HTuple("tsp_clip_region"), HTuple("false"));
        m_restore = true;
    }

    ~HalconThreadClipRegionGuard()
    {
        if (!m_restore)
            return;
        try {
            SetSystem(HTuple("tsp_clip_region"), m_previousValue);
        } catch (...) {
            // A destructor must not mask the original HALCON result/exception.
        }
    }

    HalconThreadClipRegionGuard(const HalconThreadClipRegionGuard &) = delete;
    HalconThreadClipRegionGuard &operator=(
            const HalconThreadClipRegionGuard &) = delete;

private:
    HTuple m_previousValue;
    bool m_restore = false;
};

// 构造统一的求解失败结果，保留稳定状态码和可诊断消息。
CalibrationSolveResult failure(const QString &status, const QString &message)
{
    CalibrationSolveResult result;
    result.status = status;
    result.message = message;
    return result;
}

// 校验平移样本的像素与机械 X/Y 均为有限数值。
bool finiteSample(const CalibrationSample &sample)
{
    return std::isfinite(sample.column) && std::isfinite(sample.row)
            && std::isfinite(sample.machineX) && std::isfinite(sample.machineY);
}

double twiceArea(const CalibrationSample &a,
                 const CalibrationSample &b,
                 const CalibrationSample &c,
                 bool pixel)
{
    const double ax = pixel ? a.column : a.machineX;
    const double ay = pixel ? a.row : a.machineY;
    const double bx = pixel ? b.column : b.machineX;
    const double by = pixel ? b.row : b.machineY;
    const double cx = pixel ? c.column : c.machineX;
    const double cy = pixel ? c.row : c.machineY;
    return std::abs((bx - ax) * (cy - ay) - (by - ay) * (cx - ax));
}

// 检查像素域或机械域至少存在一组三点不共线，避免退化仿射求解。
bool hasNonCollinearTriple(const QVector<CalibrationSample> &samples, bool pixel)
{
    for (int i = 0; i < samples.size() - 2; ++i) {
        for (int j = i + 1; j < samples.size() - 1; ++j) {
            for (int k = j + 1; k < samples.size(); ++k) {
                if (twiceArea(samples.at(i), samples.at(j), samples.at(k), pixel) > 1e-8)
                    return true;
            }
        }
    }
    return false;
}

// 用 HALCON 从样本点生成凸包轮廓，并输出像素 Column/Row 有效区域。
QVector<QPointF> convexValidRegion(const HTuple &rows, const HTuple &columns)
{
    HObject sampleContour;
    HObject hullContour;
    HTuple hullRows;
    HTuple hullColumns;
    GenContourPolygonXld(&sampleContour, rows, columns);
    ShapeTransXld(sampleContour, &hullContour, HTuple("convex"));
    GetContourXld(hullContour, &hullRows, &hullColumns);

    QVector<QPointF> result;
    const Hlong pointCount = std::min(hullRows.Length(), hullColumns.Length());
    result.reserve(static_cast<int>(pointCount));
    constexpr double kSamePointTolerance = 1e-9;
    for (Hlong index = 0; index < pointCount; ++index) {
        const QPointF point(hullColumns[index].D(), hullRows[index].D());
        if (!result.isEmpty()
                && std::hypot(result.constLast().x() - point.x(),
                              result.constLast().y() - point.y())
                   <= kSamePointTolerance) {
            continue;
        }
        result.append(point);
    }
    if (result.size() > 1
            && std::hypot(result.constFirst().x() - result.constLast().x(),
                          result.constFirst().y() - result.constLast().y())
               <= kSamePointTolerance) {
        result.removeLast();
    }
    return result;
}

// 清理 HALCON 轮廓导出的相邻重复点与闭合重复端点。
QVector<QPointF> compactPolygon(const HTuple &rows, const HTuple &columns)
{
    QVector<QPointF> result;
    const Hlong pointCount = std::min(rows.Length(), columns.Length());
    result.reserve(static_cast<int>(pointCount));
    constexpr double kSamePointTolerance = 1e-9;
    for (Hlong index = 0; index < pointCount; ++index) {
        const QPointF point(columns[index].D(), rows[index].D());
        if (!result.isEmpty()
                && std::hypot(result.constLast().x() - point.x(),
                              result.constLast().y() - point.y())
                   <= kSamePointTolerance) {
            continue;
        }
        result.append(point);
    }
    if (result.size() > 1
            && std::hypot(result.constFirst().x() - result.constLast().x(),
                          result.constFirst().y() - result.constLast().y())
               <= kSamePointTolerance) {
        result.removeLast();
    }
    return result;
}

void polygonTuples(const QVector<QPointF> &polygon,
                   HTuple *rows,
                   HTuple *columns)
{
    if (!rows || !columns)
        return;
    for (const QPointF &point : polygon) {
        rows->Append(point.y());
        columns->Append(point.x());
    }
}

// 用 HALCON 对有效区域按像素边距内缩，生成可生产放行的 SafeROI。
bool erodedSafeRegion(const QVector<QPointF> &validRegion,
                      double safeMarginPx,
                      QVector<QPointF> *safeRegion,
                      QString *errorMessage)
{
    if (!safeRegion)
        return false;
    safeRegion->clear();
    if (safeMarginPx == 0.0) {
        *safeRegion = validRegion;
        return true;
    }

    HTuple validRows;
    HTuple validColumns;
    polygonTuples(validRegion, &validRows, &validColumns);
    // Region generators otherwise clip to HALCON's current image format.  A
    // calibration polygon is expressed in source-image coordinates and must
    // not depend on whichever image size happened to run on this thread last.
    // Use the thread-specific setting and restore it before returning.
    HalconThreadClipRegionGuard clipRegionGuard;
    HObject validRegionObject;
    HObject erodedRegionObject;
    GenRegionPolygonFilled(&validRegionObject, validRows, validColumns);

    // HALCON's raster circle is centered without a half-pixel translation for
    // half-integer radii.  In the public contract safeMarginPx denotes the
    // number of pixels removed, hence the +0.5 mapping.
    ErosionCircle(validRegionObject, &erodedRegionObject,
                  HTuple(safeMarginPx + 0.5));

    HTuple area;
    HTuple centerRow;
    HTuple centerColumn;
    AreaCenter(erodedRegionObject, &area, &centerRow, &centerColumn);
    if (area.Length() == 0 || area[0].D() <= 0.0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("安全内缩距离过大，SafeROI为空");
        return false;
    }

    HTuple isSubset;
    TestSubsetRegion(erodedRegionObject, validRegionObject, &isSubset);
    if (isSubset.Length() == 0 || isSubset[0].I() == 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("HALCON生成的SafeROI不属于ValidROI");
        return false;
    }

    constexpr double kPolygonTolerancePx = 0.5;
    HTuple safeRows;
    HTuple safeColumns;
    GetRegionPolygon(erodedRegionObject, HTuple(kPolygonTolerancePx),
                     &safeRows, &safeColumns);
    *safeRegion = compactPolygon(safeRows, safeColumns);
    if (safeRegion->size() < 3) {
        if (errorMessage)
            *errorMessage = QStringLiteral("安全内缩后SafeROI退化，顶点不足3个");
        return false;
    }

    HTuple polygonRows;
    HTuple polygonColumns;
    polygonTuples(*safeRegion, &polygonRows, &polygonColumns);
    HObject serializedSafeRegion;
    GenRegionPolygonFilled(&serializedSafeRegion, polygonRows, polygonColumns);
    TestSubsetRegion(serializedSafeRegion, erodedRegionObject, &isSubset);
    if (isSubset.Length() == 0 || isSubset[0].I() == 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                        "SafeROI多边形近似超出HALCON内缩区域");
        return false;
    }
    return true;
}

// 按采样顺序展开机械角，消除跨越周期边界造成的跳变。
QVector<double> unwrapAngles(const QVector<CalibrationSample> &samples,
                             bool imageAngles)
{
    QVector<double> result;
    result.reserve(samples.size());
    if (samples.isEmpty())
        return result;
    result.append(imageAngles ? samples.constFirst().imageAngleDeg
                              : samples.constFirst().machineAngleDeg);
    for (int index = 1; index < samples.size(); ++index) {
        const double raw = imageAngles ? samples.at(index).imageAngleDeg
                                       : samples.at(index).machineAngleDeg;
        double delta = std::remainder(raw - result.constLast(), 360.0);
        if (delta <= -180.0)
            delta += 360.0;
        result.append(result.constLast() + delta);
    }
    return result;
}

void angleRange(const QVector<double> &angles,
                double *minimum,
                double *maximum,
                double *center)
{
    const auto bounds = std::minmax_element(angles.constBegin(), angles.constEnd());
    *minimum = *bounds.first;
    *maximum = *bounds.second;
    *center = (*minimum + *maximum) * 0.5;
}

double normalizedAngleDifference(double value)
{
    double normalized = std::remainder(value, 360.0);
    if (normalized <= -180.0)
        normalized += 360.0;
    return normalized;
}

QVector<double> unwrapAngleValues(const QVector<double> &angles)
{
    QVector<double> result;
    result.reserve(angles.size());
    if (angles.isEmpty())
        return result;
    result.append(angles.constFirst());
    for (int index = 1; index < angles.size(); ++index) {
        result.append(result.constLast()
                      + normalizedAngleDifference(
                          angles.at(index) - result.constLast()));
    }
    return result;
}

// 统计周期角度中的独立观测数，近似重复角只计一次。
int independentAngleCount(const QVector<double> &angles,
                          double toleranceDeg = 0.1)
{
    QVector<double> sorted;
    sorted.reserve(angles.size());
    for (double angle : angles) {
        double wrapped = std::fmod(angle, 360.0);
        if (wrapped < 0.0)
            wrapped += 360.0;
        sorted.append(wrapped);
    }
    std::sort(sorted.begin(), sorted.end());
    QVector<double> unique;
    for (double angle : sorted) {
        if (unique.isEmpty()
                || std::abs(angle - unique.constLast()) > toleranceDeg)
            unique.append(angle);
    }
    // The first and last wrapped values are neighbours across the 0/360 seam.
    // Treat 0 deg and 360 deg (including small measurement noise) as one
    // physical axis pose.
    if (unique.size() > 1
            && 360.0 - unique.constLast() + unique.constFirst()
               <= toleranceDeg) {
        unique.removeLast();
    }
    return unique.size();
}

// 用 HALCON 仿射线性部分把图像方向转换为机械平面方向角。
bool affineDirectionAngles(const std::array<double, 6> &forward,
                           const QVector<double> &imageAngles,
                           QVector<double> *outputAngles)
{
    if (!outputAngles)
        return false;
    HTuple matrix;
    for (double value : forward)
        matrix.Append(value);
    HTuple originsX;
    HTuple originsY;
    HTuple tipsX;
    HTuple tipsY;
    constexpr double kPi = 3.14159265358979323846;
    for (double angle : imageAngles) {
        const double radians = angle * kPi / 180.0;
        originsX.Append(0.0);
        originsY.Append(0.0);
        tipsX.Append(std::cos(radians));
        tipsY.Append(std::sin(radians));
    }
    HTuple transformedOriginsX;
    HTuple transformedOriginsY;
    HTuple transformedTipsX;
    HTuple transformedTipsY;
    AffineTransPoint2d(matrix, originsX, originsY,
                       &transformedOriginsX, &transformedOriginsY);
    AffineTransPoint2d(matrix, tipsX, tipsY,
                       &transformedTipsX, &transformedTipsY);
    if (transformedOriginsX.Length() != imageAngles.size()
            || transformedTipsX.Length() != imageAngles.size()) {
        return false;
    }
    QVector<double> wrapped;
    wrapped.reserve(imageAngles.size());
    for (int index = 0; index < imageAngles.size(); ++index) {
        const double dx = transformedTipsX[index].D()
                - transformedOriginsX[index].D();
        const double dy = transformedTipsY[index].D()
                - transformedOriginsY[index].D();
        if (!std::isfinite(dx) || !std::isfinite(dy)
                || std::hypot(dx, dy) <= 1e-12) {
            return false;
        }
        wrapped.append(std::atan2(dy, dx) * 180.0 / kPi);
    }
    *outputAngles = unwrapAngleValues(wrapped);
    return true;
}

struct RotationFitCandidate
{
    bool valid = false;
    CalibrationAngleDirection direction = CalibrationAngleDirection::Unknown;
    double offsetDeg = 0.0;
    double rmseDeg = 0.0;
    double maxErrorDeg = 0.0;
};

struct RotationPositionFitCandidate
{
    bool valid = false;
    CalibrationRotationDirection direction =
            CalibrationRotationDirection::Unknown;
    double phaseOffsetDeg = 0.0;
    double rmseMm = 0.0;
    double maxErrorMm = 0.0;
};

// 针对给定旋转方向拟合偏心圆轨迹，并计算位置残差候选。
RotationPositionFitCandidate fitRotationPositionCandidate(
        const QVector<double> &machineAngles,
        const QVector<double> &offsetXs,
        const QVector<double> &offsetYs,
        double centerX,
        double centerY,
        double radius,
        CalibrationRotationDirection direction)
{
    RotationPositionFitCandidate candidate;
    candidate.direction = direction;
    if (machineAngles.size() != offsetXs.size()
            || machineAngles.size() != offsetYs.size()
            || machineAngles.isEmpty() || radius <= 0.0) {
        return candidate;
    }

    const double sign = direction == CalibrationRotationDirection::SameSign
            ? 1.0 : -1.0;
    constexpr double kPi = 3.14159265358979323846;
    QVector<double> phaseSamples;
    phaseSamples.reserve(machineAngles.size());
    for (int index = 0; index < machineAngles.size(); ++index) {
        const double radialAngle = std::atan2(
                    offsetYs.at(index) - centerY,
                    offsetXs.at(index) - centerX) * 180.0 / kPi;
        const double rawPhase = radialAngle - sign * machineAngles.at(index);
        if (phaseSamples.isEmpty()) {
            phaseSamples.append(rawPhase);
        } else {
            phaseSamples.append(phaseSamples.constLast()
                                + normalizedAngleDifference(
                                    rawPhase - phaseSamples.constLast()));
        }
    }
    double phaseSum = 0.0;
    for (double phase : phaseSamples)
        phaseSum += phase;
    candidate.phaseOffsetDeg = normalizedAngleDifference(
                phaseSum / static_cast<double>(phaseSamples.size()));

    double sumSquares = 0.0;
    double maximum = 0.0;
    for (int index = 0; index < machineAngles.size(); ++index) {
        const double phase = (sign * machineAngles.at(index)
                              + candidate.phaseOffsetDeg) * kPi / 180.0;
        const double predictedX = centerX + radius * std::cos(phase);
        const double predictedY = centerY + radius * std::sin(phase);
        const double error = std::hypot(predictedX - offsetXs.at(index),
                                        predictedY - offsetYs.at(index));
        sumSquares += error * error;
        maximum = std::max(maximum, error);
    }
    candidate.rmseMm = std::sqrt(
                sumSquares / static_cast<double>(machineAngles.size()));
    candidate.maxErrorMm = maximum;
    candidate.valid = std::isfinite(candidate.phaseOffsetDeg)
            && std::isfinite(candidate.rmseMm)
            && std::isfinite(candidate.maxErrorMm);
    return candidate;
}

// 针对给定角度方向拟合 ImageAngle -> MachineAngle 的偏置与误差候选。
RotationFitCandidate fitRotationCandidate(
        const std::array<double, 6> &forward,
        const QVector<double> &imageAngles,
        const QVector<double> &machineAngles,
        CalibrationAngleDirection direction)
{
    RotationFitCandidate candidate;
    candidate.direction = direction;
    QVector<double> predicted;
    const double sign = direction == CalibrationAngleDirection::SameSign
            ? 1.0 : -1.0;
    if (!affineDirectionAngles(forward, imageAngles, &predicted)
            || predicted.size() != machineAngles.size()) {
        return candidate;
    }
    if (sign < 0.0) {
        for (double &angle : predicted)
            angle = -angle;
        predicted = unwrapAngleValues(predicted);
    }

    QVector<double> offsets;
    offsets.reserve(predicted.size());
    for (int index = 0; index < predicted.size(); ++index) {
        const double rawOffset = machineAngles.at(index) - predicted.at(index);
        if (offsets.isEmpty()) {
            offsets.append(rawOffset);
        } else {
            offsets.append(offsets.constLast()
                           + normalizedAngleDifference(
                               rawOffset - offsets.constLast()));
        }
    }
    double offsetSum = 0.0;
    for (double value : offsets)
        offsetSum += value;
    candidate.offsetDeg = normalizedAngleDifference(
                offsetSum / static_cast<double>(offsets.size()));

    double sumSquares = 0.0;
    double maximum = 0.0;
    for (int index = 0; index < predicted.size(); ++index) {
        const double residual = normalizedAngleDifference(
                    machineAngles.at(index)
                    - predicted.at(index) - candidate.offsetDeg);
        sumSquares += residual * residual;
        maximum = std::max(maximum, std::abs(residual));
    }
    candidate.rmseDeg = std::sqrt(
                sumSquares / static_cast<double>(predicted.size()));
    candidate.maxErrorDeg = maximum;
    candidate.valid = std::isfinite(candidate.offsetDeg)
            && std::isfinite(candidate.rmseDeg)
            && std::isfinite(candidate.maxErrorDeg);
    return candidate;
}

} // namespace

CalibrationSolveResult CalibrationSolver::solveNPoint(
        const QVector<CalibrationSample> &input,
        double rmseLimit,
        double maxErrorLimit,
        double safeMarginPx) const
{
    if (input.size() != 9) {
        return failure(QStringLiteral("CAL-SOL-001"),
                       QStringLiteral("当前N点标定要求严格使用9个平移点"));
    }
    if (!std::isfinite(rmseLimit) || !std::isfinite(maxErrorLimit)
            || rmseLimit <= 0.0 || maxErrorLimit <= 0.0) {
        return failure(QStringLiteral("CAL-SOL-001"), QStringLiteral("误差阈值必须为有限正数"));
    }
    if (!std::isfinite(safeMarginPx) || safeMarginPx < 0.0) {
        return failure(QStringLiteral("CAL-SOL-001"),
                       QStringLiteral("安全内缩距离必须为有限非负数"));
    }
    if (safeMarginPx > 511.0) {
        return failure(QStringLiteral("CAL-SOL-003"),
                       QStringLiteral("安全内缩距离超过HALCON ErosionCircle支持上限511 px"));
    }
    for (const CalibrationSample &sample : input) {
        if (!finiteSample(sample))
            return failure(QStringLiteral("CAL-SOL-001"), QStringLiteral("标定点包含非有限坐标"));
    }
    for (int i = 0; i < input.size(); ++i) {
        for (int j = i + 1; j < input.size(); ++j) {
            const CalibrationSample &a = input.at(i);
            const CalibrationSample &b = input.at(j);
            if (std::hypot(a.column - b.column, a.row - b.row) < 1e-6
                    || std::hypot(a.machineX - b.machineX,
                                  a.machineY - b.machineY) < 1e-9) {
                return failure(QStringLiteral("CAL-SOL-001"), QStringLiteral("标定点存在重复或近似重合"));
            }
        }
    }
    if (!hasNonCollinearTriple(input, true) || !hasNonCollinearTriple(input, false))
        return failure(QStringLiteral("CAL-SOL-001"), QStringLiteral("图像点或物理点近似共线"));

    try {
        HTuple columns;
        HTuple rows;
        HTuple machineXs;
        HTuple machineYs;
        for (const CalibrationSample &sample : input) {
            columns.Append(sample.column);
            rows.Append(sample.row);
            machineXs.Append(sample.machineX);
            machineYs.Append(sample.machineY);
        }

        HTuple forward;
        HTuple inverse;
        HTuple predictedXs;
        HTuple predictedYs;
        HTuple verifiedColumns;
        HTuple verifiedRows;
        VectorToHomMat2d(columns, rows, machineXs, machineYs, &forward);
        HomMat2dInvert(forward, &inverse);
        AffineTransPoint2d(forward, columns, rows, &predictedXs, &predictedYs);
        AffineTransPoint2d(inverse, predictedXs, predictedYs,
                           &verifiedColumns, &verifiedRows);
        if (forward.Length() != 6 || inverse.Length() != 6)
            return failure(QStringLiteral("CAL-SOL-001"), QStringLiteral("HALCON返回了非二维仿射矩阵"));

        CalibrationModel model;
        model.schemaVersion = QStringLiteral("1.4");
        model.mode = NPointCalibrationMode::NinePointXY;
        model.calibrationId = QStringLiteral("CAL-%1-%2")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmsszzz")),
                     QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
        model.createdAt = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        for (int i = 0; i < 6; ++i) {
            model.forward[static_cast<size_t>(i)] = forward[i].D();
            model.inverse[static_cast<size_t>(i)] = inverse[i].D();
        }
        model.transformParity = model.determinant() > 0.0
                ? CalibrationTransformParity::OrientationPreserving
                : CalibrationTransformParity::OrientationReversing;

        double sum = 0.0;
        double sumSquares = 0.0;
        double sumX2 = 0.0;
        double sumY2 = 0.0;
        double maxError = 0.0;
        double inverseRoundTripMaxPx = 0.0;
        model.samples.reserve(input.size());
        for (int i = 0; i < input.size(); ++i) {
            CalibrationSample sample = input.at(i);
            sample.index = i + 1;
            sample.residualX = predictedXs[i].D() - sample.machineX;
            sample.residualY = predictedYs[i].D() - sample.machineY;
            sample.residual = std::hypot(sample.residualX, sample.residualY);
            sum += sample.residual;
            sumSquares += sample.residual * sample.residual;
            sumX2 += sample.residualX * sample.residualX;
            sumY2 += sample.residualY * sample.residualY;
            maxError = std::max(maxError, sample.residual);
            inverseRoundTripMaxPx = std::max(
                        inverseRoundTripMaxPx,
                        std::hypot(verifiedColumns[i].D() - sample.column,
                                   verifiedRows[i].D() - sample.row));
            model.samples.append(sample);
        }
        model.translationSampleCount = model.samples.size();
        model.configuredRotationSampleCount = 0;
        model.totalSampleCount = model.translationSampleCount;
        model.validRegion = convexValidRegion(rows, columns);
        if (model.validRegion.size() < 3) {
            return failure(QStringLiteral("CAL-SOL-001"),
                           QStringLiteral("HALCON未能生成有效的标定区域凸包"));
        }
        QString safeRegionError;
        if (!erodedSafeRegion(model.validRegion, safeMarginPx,
                              &model.safeRegion, &safeRegionError)) {
            return failure(QStringLiteral("CAL-SOL-003"), safeRegionError);
        }
        model.safeMarginPx = safeMarginPx;
        const double count = static_cast<double>(input.size());
        model.quality.meanError = sum / count;
        model.quality.rmse = std::sqrt(sumSquares / count);
        model.quality.maxError = maxError;
        model.quality.rmseX = std::sqrt(sumX2 / count);
        model.quality.rmseY = std::sqrt(sumY2 / count);
        model.quality.rmseLimit = rmseLimit;
        model.quality.maxErrorLimit = maxErrorLimit;
        model.quality.passed = model.quality.rmse <= rmseLimit
                && model.quality.maxError <= maxErrorLimit;
        model.methodData.insert(QStringLiteral("version"), 3);
        model.methodData.insert(QStringLiteral("calibrationMode"),
                                nPointCalibrationModeToString(model.mode));
        model.methodData.insert(QStringLiteral("fitProcedure"),
                                QStringLiteral("HALCON.VectorToHomMat2d"));
        model.methodData.insert(QStringLiteral("sampleCount"), input.size());
        model.methodData.insert(QStringLiteral("translationSampleCount"),
                                input.size());
        model.methodData.insert(QStringLiteral("rotationSampleCount"), 0);
        model.methodData.insert(QStringLiteral("totalSampleCount"), input.size());
        model.methodData.insert(QStringLiteral("xyFitSampleCount"), input.size());
        model.methodData.insert(QStringLiteral("transformParity"),
                                calibrationTransformParityToString(
                                    model.transformParity));
        model.methodData.insert(QStringLiteral("inverseRoundTripMaxPx"),
                                inverseRoundTripMaxPx);
        model.methodData.insert(QStringLiteral("validRegionType"),
                                QStringLiteral("convex_hull"));
        model.methodData.insert(QStringLiteral("validRegionSource"),
                                QStringLiteral("translation_samples"));
        model.methodData.insert(QStringLiteral("validRegionCoordinateSystem"),
                                QStringLiteral("image_pixel_column_row"));
        model.methodData.insert(QStringLiteral("validRegionPointCount"),
                                model.validRegion.size());
        model.methodData.insert(QStringLiteral("safeRegionType"),
                                model.safeRegionType);
        model.methodData.insert(QStringLiteral("safeRegionSource"),
                                model.safeRegionSource);
        model.methodData.insert(QStringLiteral("safeRegionCoordinateSystem"),
                                model.safeRegionCoordinateSystem);
        model.methodData.insert(QStringLiteral("safeRegionPointCount"),
                                model.safeRegion.size());
        model.methodData.insert(QStringLiteral("safeMarginPx"), safeMarginPx);

        QString modelError;
        if (!model.isValid(&modelError))
            return failure(QStringLiteral("CAL-SOL-001"), modelError);

        CalibrationSolveResult result;
        result.success = true;
        result.status = model.quality.passed
                ? QStringLiteral("solved") : QStringLiteral("CAL-SOL-002");
        result.message = model.quality.passed
                ? QStringLiteral("N点标定求解成功")
                : QStringLiteral("标定已求解，但残差超过启用阈值");
        result.model = model;
        return result;
    } catch (const HException &exception) {
        return failure(QStringLiteral("CAL-SOL-001"),
                       QStringLiteral("HALCON %1: %2")
                       .arg(static_cast<qlonglong>(exception.ErrorCode()))
                       .arg(QString::fromUtf8(exception.ErrorMessage().Text())));
    } catch (const std::exception &exception) {
        return failure(QStringLiteral("CAL-SOL-001"),
                       QString::fromLocal8Bit(exception.what()));
    }
}

CalibrationAngleMapping CalibrationSolver::buildAngleMapping(
        const QVector<CalibrationSample> &rotationSamples,
        const std::array<double, 6> &forward,
        double minimumSpanDeg,
        double rmseLimitDeg,
        double maxErrorLimitDeg) const
{
    CalibrationAngleMapping mapping;
    mapping.sampleCount = rotationSamples.size();
    mapping.minimumSpanDeg = minimumSpanDeg;
    mapping.rmseLimitDeg = rmseLimitDeg;
    mapping.maxErrorLimitDeg = maxErrorLimitDeg;
    if (rotationSamples.isEmpty())
        return mapping;
    if (!std::isfinite(minimumSpanDeg) || minimumSpanDeg <= 0.0
            || minimumSpanDeg > 180.0
            || !std::isfinite(rmseLimitDeg) || rmseLimitDeg <= 0.0
            || !std::isfinite(maxErrorLimitDeg) || maxErrorLimitDeg <= 0.0) {
        mapping.status = CalibrationAngleMappingStatus::Invalid;
        mapping.validationCode = QStringLiteral("invalid_threshold");
        return mapping;
    }
    if (rotationSamples.size() != 3) {
        mapping.status = CalibrationAngleMappingStatus::Invalid;
        mapping.validationCode = QStringLiteral("sample_count_mismatch");
        return mapping;
    }

    for (const CalibrationSample &sample : rotationSamples) {
        if (!std::isfinite(sample.imageAngleDeg)
                || !std::isfinite(sample.machineAngleDeg)) {
            mapping.status = CalibrationAngleMappingStatus::Invalid;
            mapping.validationCode = QStringLiteral("non_finite_angle");
            return mapping;
        }
    }

    const QVector<double> imageAngles = unwrapAngles(rotationSamples, true);
    const QVector<double> machineAngles = unwrapAngles(rotationSamples, false);
    angleRange(imageAngles, &mapping.imageMinDeg, &mapping.imageMaxDeg,
               &mapping.imageCenterDeg);
    angleRange(machineAngles, &mapping.machineMinDeg, &mapping.machineMaxDeg,
               &mapping.machineCenterDeg);
    constexpr double kCoverageToleranceDeg = 1e-9;
    const double imageSpan = mapping.imageMaxDeg - mapping.imageMinDeg;
    const double machineSpan = mapping.machineMaxDeg - mapping.machineMinDeg;
    if (imageSpan > mapping.periodDeg + kCoverageToleranceDeg
            || machineSpan > mapping.periodDeg + kCoverageToleranceDeg) {
        mapping.status = CalibrationAngleMappingStatus::Invalid;
        mapping.validationCode = QStringLiteral("coverage_exceeds_period");
        return mapping;
    }
    if (imageSpan < minimumSpanDeg - kCoverageToleranceDeg) {
        mapping.status = CalibrationAngleMappingStatus::Invalid;
        mapping.validationCode = QStringLiteral("image_span_too_small");
        return mapping;
    }
    if (machineSpan < minimumSpanDeg - kCoverageToleranceDeg) {
        mapping.status = CalibrationAngleMappingStatus::Invalid;
        mapping.validationCode = QStringLiteral("machine_span_too_small");
        return mapping;
    }
    if (independentAngleCount(imageAngles) < 3
            || independentAngleCount(machineAngles) < 3) {
        mapping.status = CalibrationAngleMappingStatus::Invalid;
        mapping.validationCode = QStringLiteral(
                    "insufficient_independent_angles");
        return mapping;
    }

    try {
        const RotationFitCandidate same = fitRotationCandidate(
                    forward, imageAngles, machineAngles,
                    CalibrationAngleDirection::SameSign);
        const RotationFitCandidate opposite = fitRotationCandidate(
                    forward, imageAngles, machineAngles,
                    CalibrationAngleDirection::OppositeSign);
        const RotationFitCandidate selected = !same.valid ? opposite
                : !opposite.valid ? same
                : (same.rmseDeg < opposite.rmseDeg
                   || (std::abs(same.rmseDeg - opposite.rmseDeg) <= 1e-12
                       && same.maxErrorDeg <= opposite.maxErrorDeg))
                  ? same : opposite;
        if (!selected.valid) {
            mapping.status = CalibrationAngleMappingStatus::Invalid;
            mapping.validationCode = QStringLiteral("angle_transform_failed");
            return mapping;
        }
        mapping.direction = selected.direction;
        mapping.offsetDeg = selected.offsetDeg;
        mapping.rmseDeg = selected.rmseDeg;
        mapping.maxErrorDeg = selected.maxErrorDeg;
        if (mapping.rmseDeg > rmseLimitDeg + kCoverageToleranceDeg
                || mapping.maxErrorDeg
                   > maxErrorLimitDeg + kCoverageToleranceDeg) {
            mapping.status = CalibrationAngleMappingStatus::Invalid;
            mapping.validationCode = QStringLiteral("fit_error_exceeded");
            return mapping;
        }
    } catch (const HException &) {
        mapping.status = CalibrationAngleMappingStatus::Invalid;
        mapping.validationCode = QStringLiteral("halcon_angle_transform_failed");
        return mapping;
    }
    mapping.status = CalibrationAngleMappingStatus::Verified;
    mapping.validationCode = QStringLiteral("verified");
    return mapping;
}

CalibrationRotationRange CalibrationSolver::buildAxisTrace(
        const QVector<CalibrationSample> &rotationSamples,
        const std::array<double, 6> &forward,
        double minimumSpanDeg,
        double rmseLimitMm,
        double maxErrorLimitMm) const
{
    CalibrationRotationRange range;
    range.sampleCount = rotationSamples.size();
    range.minimumSpanDeg = minimumSpanDeg;
    range.rmseLimitMm = rmseLimitMm;
    range.maxErrorLimitMm = maxErrorLimitMm;
    if (rotationSamples.isEmpty())
        return range;
    if (!std::isfinite(minimumSpanDeg) || minimumSpanDeg <= 0.0
            || minimumSpanDeg > 180.0
            || !std::isfinite(rmseLimitMm) || rmseLimitMm <= 0.0
            || !std::isfinite(maxErrorLimitMm) || maxErrorLimitMm <= 0.0) {
        range.status = CalibrationRotationRangeStatus::Invalid;
        range.validationCode = QStringLiteral("invalid_position_threshold");
        return range;
    }
    if (rotationSamples.size() != 3) {
        range.status = CalibrationRotationRangeStatus::Invalid;
        range.validationCode = QStringLiteral("sample_count_mismatch");
        return range;
    }
    for (const CalibrationSample &sample : rotationSamples) {
        if (!finiteSample(sample) || !std::isfinite(sample.machineAngleDeg)) {
            range.status = CalibrationRotationRangeStatus::Invalid;
            range.validationCode = QStringLiteral("non_finite_rotation_sample");
            return range;
        }
    }

    const QVector<double> machineAngles = unwrapAngles(rotationSamples, false);
    angleRange(machineAngles, &range.machineMinDeg, &range.machineMaxDeg,
               &range.machineCenterDeg);
    constexpr double kTolerance = 1e-9;
    const double machineSpan = range.machineMaxDeg - range.machineMinDeg;
    if (machineSpan > range.periodDeg + kTolerance) {
        range.status = CalibrationRotationRangeStatus::Invalid;
        range.validationCode = QStringLiteral("coverage_exceeds_period");
        return range;
    }
    if (machineSpan < minimumSpanDeg - kTolerance) {
        range.status = CalibrationRotationRangeStatus::Invalid;
        range.validationCode = QStringLiteral("machine_span_too_small");
        return range;
    }
    if (independentAngleCount(machineAngles) < 3) {
        range.status = CalibrationRotationRangeStatus::Invalid;
        range.validationCode = QStringLiteral("insufficient_independent_machine_angles");
        return range;
    }

    try {
        HTuple matrix;
        for (double value : forward)
            matrix.Append(value);
        HTuple columns;
        HTuple rows;
        for (const CalibrationSample &sample : rotationSamples) {
            columns.Append(sample.column);
            rows.Append(sample.row);
        }
        HTuple transformedXs;
        HTuple transformedYs;
        AffineTransPoint2d(matrix, columns, rows,
                           &transformedXs, &transformedYs);
        if (transformedXs.Length() != rotationSamples.size()
                || transformedYs.Length() != rotationSamples.size()) {
            range.status = CalibrationRotationRangeStatus::Invalid;
            range.validationCode = QStringLiteral("position_transform_failed");
            return range;
        }

        QVector<double> offsetXs;
        QVector<double> offsetYs;
        offsetXs.reserve(rotationSamples.size());
        offsetYs.reserve(rotationSamples.size());
        double meanX = 0.0;
        double meanY = 0.0;
        for (int index = 0; index < rotationSamples.size(); ++index) {
            const double offsetX = transformedXs[index].D()
                    - rotationSamples.at(index).machineX;
            const double offsetY = transformedYs[index].D()
                    - rotationSamples.at(index).machineY;
            if (!std::isfinite(offsetX) || !std::isfinite(offsetY)) {
                range.status = CalibrationRotationRangeStatus::Invalid;
                range.validationCode = QStringLiteral("non_finite_position_offset");
                return range;
            }
            offsetXs.append(offsetX);
            offsetYs.append(offsetY);
            meanX += offsetX;
            meanY += offsetY;
        }
        meanX /= static_cast<double>(offsetXs.size());
        meanY /= static_cast<double>(offsetYs.size());
        double maximumSpread = 0.0;
        for (int index = 0; index < offsetXs.size(); ++index) {
            maximumSpread = std::max(
                        maximumSpread,
                        std::hypot(offsetXs.at(index) - meanX,
                                   offsetYs.at(index) - meanY));
        }

        // Only accept the coaxial shortcut when both the trajectory spread and
        // the absolute zero-correction residual pass the configured gate.  A
        // short eccentric arc may have little spread while sitting far away
        // from zero; that case must continue to the circle fit instead of being
        // misclassified as an invalid coaxial sample set.
        if (maximumSpread <= rmseLimitMm + kTolerance) {
            double sumSquares = 0.0;
            double maximum = 0.0;
            for (int index = 0; index < offsetXs.size(); ++index) {
                const double error = std::hypot(offsetXs.at(index),
                                                offsetYs.at(index));
                sumSquares += error * error;
                maximum = std::max(maximum, error);
            }
            const double zeroCorrectionRmse = std::sqrt(
                        sumSquares / static_cast<double>(offsetXs.size()));
            if (zeroCorrectionRmse <= rmseLimitMm + kTolerance
                    && maximum <= maxErrorLimitMm + kTolerance) {
                range.coaxial = true;
                range.fitRmseMm = zeroCorrectionRmse;
                range.maxErrorMm = maximum;
                range.status = CalibrationRotationRangeStatus::Verified;
                range.validationCode = QStringLiteral("verified_coaxial");
                return range;
            }
        }

        HTuple offsetColumns;
        HTuple offsetRows;
        for (int index = 0; index < offsetXs.size(); ++index) {
            offsetColumns.Append(offsetXs.at(index));
            offsetRows.Append(offsetYs.at(index));
        }
        HObject trajectoryContour;
        GenContourPolygonXld(&trajectoryContour, offsetRows, offsetColumns);
        HTuple centerRow;
        HTuple centerColumn;
        HTuple radius;
        HTuple startPhi;
        HTuple endPhi;
        HTuple pointOrder;
        FitCircleContourXld(trajectoryContour, HTuple("algebraic"),
                            HTuple(-1), HTuple(0), HTuple(0), HTuple(3),
                            HTuple(2), &centerRow, &centerColumn, &radius,
                            &startPhi, &endPhi, &pointOrder);
        if (centerRow.Length() != 1 || centerColumn.Length() != 1
                || radius.Length() != 1 || !std::isfinite(radius[0].D())
                || radius[0].D() <= kTolerance) {
            range.status = CalibrationRotationRangeStatus::Invalid;
            range.validationCode = QStringLiteral("trajectory_circle_invalid");
            return range;
        }
        range.centerOffsetX = centerColumn[0].D();
        range.centerOffsetY = centerRow[0].D();
        range.radiusMm = radius[0].D();

        QVector<double> trajectoryAngles;
        trajectoryAngles.reserve(offsetXs.size());
        constexpr double kPi = 3.14159265358979323846;
        for (int index = 0; index < offsetXs.size(); ++index) {
            trajectoryAngles.append(std::atan2(
                        offsetYs.at(index) - range.centerOffsetY,
                        offsetXs.at(index) - range.centerOffsetX)
                    * 180.0 / kPi);
        }
        trajectoryAngles = unwrapAngleValues(trajectoryAngles);
        angleRange(trajectoryAngles, &range.trajectoryMinDeg,
                   &range.trajectoryMaxDeg, &range.trajectoryCenterDeg);
        const double trajectorySpan = range.trajectoryMaxDeg
                - range.trajectoryMinDeg;
        if (trajectorySpan < minimumSpanDeg - kTolerance
                || trajectorySpan > range.periodDeg + kTolerance) {
            range.status = CalibrationRotationRangeStatus::Invalid;
            range.validationCode = QStringLiteral("trajectory_span_invalid");
            return range;
        }

        const RotationPositionFitCandidate same =
                fitRotationPositionCandidate(
                    machineAngles, offsetXs, offsetYs,
                    range.centerOffsetX, range.centerOffsetY,
                    range.radiusMm,
                    CalibrationRotationDirection::SameSign);
        const RotationPositionFitCandidate opposite =
                fitRotationPositionCandidate(
                    machineAngles, offsetXs, offsetYs,
                    range.centerOffsetX, range.centerOffsetY,
                    range.radiusMm,
                    CalibrationRotationDirection::OppositeSign);
        const RotationPositionFitCandidate selected = !same.valid ? opposite
                : !opposite.valid ? same
                : (same.rmseMm < opposite.rmseMm
                   || (std::abs(same.rmseMm - opposite.rmseMm) <= 1e-12
                       && same.maxErrorMm <= opposite.maxErrorMm))
                  ? same : opposite;
        if (!selected.valid) {
            range.status = CalibrationRotationRangeStatus::Invalid;
            range.validationCode = QStringLiteral("trajectory_phase_fit_failed");
            return range;
        }
        if (same.valid && opposite.valid) {
            const double rmseDelta = std::abs(same.rmseMm - opposite.rmseMm);
            const double maxDelta = std::abs(same.maxErrorMm
                                             - opposite.maxErrorMm);
            // If both directions explain the samples equally well within the
            // position gate, the mechanical/image handedness is not
            // observable from this sample set.  Do not silently pick one.
            if (rmseDelta <= std::max(kTolerance, rmseLimitMm * 0.01)
                    && maxDelta <= std::max(kTolerance,
                                            maxErrorLimitMm * 0.01)) {
                range.status = CalibrationRotationRangeStatus::Invalid;
                range.validationCode = QStringLiteral("direction_ambiguous");
                return range;
            }
        }
        range.direction = selected.direction;
        range.phaseOffsetDeg = selected.phaseOffsetDeg;
        range.fitRmseMm = selected.rmseMm;
        range.maxErrorMm = selected.maxErrorMm;
        if (range.fitRmseMm > rmseLimitMm + kTolerance
                || range.maxErrorMm > maxErrorLimitMm + kTolerance) {
            range.status = CalibrationRotationRangeStatus::Invalid;
            range.validationCode = QStringLiteral("position_fit_error_exceeded");
            return range;
        }
    } catch (const HException &) {
        range.status = CalibrationRotationRangeStatus::Invalid;
        range.validationCode = QStringLiteral("halcon_trajectory_fit_failed");
        return range;
    }
    range.status = CalibrationRotationRangeStatus::Verified;
    range.validationCode = QStringLiteral("verified_eccentric");
    return range;
}
