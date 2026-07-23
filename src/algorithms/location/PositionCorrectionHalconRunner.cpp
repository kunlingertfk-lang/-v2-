#include "algorithms/location/PositionCorrectionHalconRunner.h"

#include <HalconCpp.h>

#include <QElapsedTimer>
#include <QJsonArray>

#include <cmath>

namespace {

using namespace HalconCpp;

constexpr double kPi = 3.14159265358979323846;

bool finitePose(const PositionPose &pose)
{
    return std::isfinite(pose.x) && std::isfinite(pose.y)
            && std::isfinite(pose.angleDeg)
            && std::isfinite(pose.scale)
            && pose.scale > 0.0;
}

double degreesToRadians(double value)
{
    return value * kPi / 180.0;
}

QJsonObject poseToJson(const PositionPose &pose)
{
    return QJsonObject{
        {QStringLiteral("x"), pose.x},
        {QStringLiteral("y"), pose.y},
        {QStringLiteral("angleDeg"), pose.angleDeg},
        {QStringLiteral("scale"), pose.scale}
    };
}

QJsonArray homMatToJson(const HTuple &homMat)
{
    QJsonArray values;
    for (Hlong index = 0; index < homMat.Length(); ++index)
        values.append(homMat[index].D());
    return values;
}

PositionCorrectionHalconResult errorResult(const QString &status,
                                           const QString &message,
                                           qint64 elapsedMs)
{
    PositionCorrectionHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), status);
    return result;
}

} // namespace

PositionCorrectionHalconResult PositionCorrectionHalconRunner::run(
        const PositionPose &referencePose,
        const PositionPose &runPose) const
{
    QElapsedTimer timer;
    timer.start();
    if (!finitePose(referencePose) || !finitePose(runPose)) {
        return errorResult(QStringLiteral("invalid_pose"),
                           QStringLiteral("referencePose and runPose must contain finite coordinates, angle, and a scale greater than zero"),
                           timer.elapsed());
    }

    try {
        const double scaleRatio = runPose.scale / referencePose.scale;
        if (!std::isfinite(scaleRatio) || scaleRatio <= 0.0) {
            return errorResult(QStringLiteral("invalid_pose_scale"),
                               QStringLiteral("run scale divided by reference scale must be finite and greater than zero"),
                               timer.elapsed());
        }
        HTuple rigidReferenceToRun;
        HTuple referenceToRun;
        HTuple runToReference;
        VectorAngleToRigid(referencePose.y, referencePose.x,
                           degreesToRadians(referencePose.angleDeg),
                           runPose.y, runPose.x,
                           degreesToRadians(runPose.angleDeg),
                           &rigidReferenceToRun);
        HomMat2dScale(rigidReferenceToRun,
                      scaleRatio,
                      scaleRatio,
                      runPose.y,
                      runPose.x,
                      &referenceToRun);
        HomMat2dInvert(referenceToRun, &runToReference);

        PositionCorrectionHalconResult result;
        result.success = true;
        result.ok = true;
        result.status = QStringLiteral("corrected");
        result.message = QStringLiteral("position correction transform calculated");
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("positionCorrectionApplied"), true);
        result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("applied"));
        result.payload.insert(QStringLiteral("referencePose"), poseToJson(referencePose));
        result.payload.insert(QStringLiteral("runPose"), poseToJson(runPose));
        result.payload.insert(QStringLiteral("referenceToRunHomMat2D"), homMatToJson(referenceToRun));
        result.payload.insert(QStringLiteral("runToReferenceHomMat2D"), homMatToJson(runToReference));
        result.payload.insert(QStringLiteral("deltaX"), runPose.x - referencePose.x);
        result.payload.insert(QStringLiteral("deltaY"), runPose.y - referencePose.y);
        result.payload.insert(QStringLiteral("deltaAngleDeg"),
                              runPose.angleDeg - referencePose.angleDeg);
        result.payload.insert(QStringLiteral("referenceScale"), referencePose.scale);
        result.payload.insert(QStringLiteral("runScale"), runPose.scale);
        result.payload.insert(QStringLiteral("scaleRatio"), scaleRatio);
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const HException &exception) {
        return errorResult(QStringLiteral("halcon_error"),
                           QStringLiteral("HALCON %1: %2")
                           .arg(static_cast<qlonglong>(exception.ErrorCode()))
                           .arg(QString::fromUtf8(exception.ErrorMessage().Text())),
                           timer.elapsed());
    } catch (const std::exception &exception) {
        return errorResult(QStringLiteral("execution_error"),
                           QString::fromLocal8Bit(exception.what()),
                           timer.elapsed());
    }
}
