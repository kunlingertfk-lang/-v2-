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

CalibrationSolveResult failure(const QString &status, const QString &message)
{
    CalibrationSolveResult result;
    result.status = status;
    result.message = message;
    return result;
}

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

} // namespace

CalibrationSolveResult CalibrationSolver::solveNPoint(
        const QVector<CalibrationSample> &input,
        double rmseLimit,
        double maxErrorLimit,
        double safeMarginPx) const
{
    if (input.size() < 3)
        return failure(QStringLiteral("CAL-SOL-001"), QStringLiteral("N点标定至少需要3组有效对应点"));
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
        model.calibrationId = QStringLiteral("CAL-%1-%2")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmsszzz")),
                     QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
        model.createdAt = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        for (int i = 0; i < 6; ++i) {
            model.forward[static_cast<size_t>(i)] = forward[i].D();
            model.inverse[static_cast<size_t>(i)] = inverse[i].D();
        }

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
        model.methodData.insert(QStringLiteral("version"), 1);
        model.methodData.insert(QStringLiteral("fitProcedure"),
                                QStringLiteral("HALCON.VectorToHomMat2d"));
        model.methodData.insert(QStringLiteral("sampleCount"), input.size());
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

CalibrationRotationRange CalibrationSolver::buildRotationRange(
        const QVector<CalibrationSample> &rotationSamples) const
{
    CalibrationRotationRange range;
    if (rotationSamples.size() < 3)
        return range;

    for (const CalibrationSample &sample : rotationSamples) {
        if (!std::isfinite(sample.imageAngleDeg)
                || !std::isfinite(sample.machineAngleDeg)) {
            range.status = CalibrationRotationRangeStatus::Invalid;
            return range;
        }
    }

    const QVector<double> imageAngles = unwrapAngles(rotationSamples, true);
    const QVector<double> machineAngles = unwrapAngles(rotationSamples, false);
    angleRange(imageAngles, &range.imageMinDeg, &range.imageMaxDeg,
               &range.imageCenterDeg);
    angleRange(machineAngles, &range.machineMinDeg, &range.machineMaxDeg,
               &range.machineCenterDeg);
    constexpr double kCoverageToleranceDeg = 1e-9;
    const double imageSpan = range.imageMaxDeg - range.imageMinDeg;
    const double machineSpan = range.machineMaxDeg - range.machineMinDeg;
    if (imageSpan <= kCoverageToleranceDeg
            || machineSpan <= kCoverageToleranceDeg
            || imageSpan > range.periodDeg + kCoverageToleranceDeg
            || machineSpan > range.periodDeg + kCoverageToleranceDeg) {
        range.status = CalibrationRotationRangeStatus::Invalid;
        return range;
    }
    range.status = CalibrationRotationRangeStatus::Verified;
    return range;
}
