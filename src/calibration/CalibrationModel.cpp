#include "calibration/CalibrationModel.h"

#include <QJsonArray>

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

} // namespace

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
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
            return false;
    }
    return std::isfinite(quality.meanError) && std::isfinite(quality.rmse)
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
    if (schemaVersion != QStringLiteral("1.0"))
        return fail(QStringLiteral("unsupported schema version: %1").arg(schemaVersion));
    if (calibrationId.trimmed().isEmpty())
        return fail(QStringLiteral("calibrationId is empty"));
    if (modelType != QStringLiteral("affine_2d"))
        return fail(QStringLiteral("unsupported model type: %1").arg(modelType));
    if (!isFinite())
        return fail(QStringLiteral("calibration contains non-finite values"));
    if (!isInvertible())
        return fail(QStringLiteral("calibration transform is singular"));
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
        {QStringLiteral("createdAt"), createdAt}
    };
}
