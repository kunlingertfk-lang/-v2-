#include "calibration/CalibrationSolver.h"

#include <HalconCpp.h>

#include <QDateTime>
#include <QUuid>

#include <algorithm>
#include <cmath>

namespace {

using namespace HalconCpp;

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

} // namespace

CalibrationSolveResult CalibrationSolver::solveNPoint(
        const QVector<CalibrationSample> &input,
        double rmseLimit,
        double maxErrorLimit) const
{
    if (input.size() < 3)
        return failure(QStringLiteral("CAL-SOL-001"), QStringLiteral("N点标定至少需要3组有效对应点"));
    if (!std::isfinite(rmseLimit) || !std::isfinite(maxErrorLimit)
            || rmseLimit <= 0.0 || maxErrorLimit <= 0.0) {
        return failure(QStringLiteral("CAL-SOL-001"), QStringLiteral("误差阈值必须为有限正数"));
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
