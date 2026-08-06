#include "tooladapters/CalibrationTransformAdapter.h"

#include "calibration/CalibrationFileLoader.h"

#include <QJsonValue>
#include <QSet>
#include <QStringList>

#include <cmath>

namespace {

bool finiteNumber(const QJsonValue &value, double *output)
{
    if (!output)
        return false;
    bool ok = true;
    const double parsed = value.isString()
            ? value.toString().toDouble(&ok)
            : value.isDouble() ? value.toDouble() : qQNaN();
    if (!ok || !std::isfinite(parsed))
        return false;
    *output = parsed;
    return true;
}

bool resolveInput(const QJsonObject &binding,
                  const QJsonObject &runtimeContext,
                  double *output,
                  QString *error)
{
    const QString mode = binding.value(QStringLiteral("mode"))
            .toString(QStringLiteral("constant"));
    if (mode == QStringLiteral("constant")) {
        if (finiteNumber(binding.value(QStringLiteral("value")), output))
            return true;
        if (error)
            *error = QStringLiteral("constant input is invalid");
        return false;
    }
    if (mode != QStringLiteral("binding")) {
        if (error)
            *error = QStringLiteral("input mode is invalid");
        return false;
    }
    const QString producerId = binding.value(QStringLiteral("producerId"))
            .toString().trimmed();
    const QString outputKey = binding.value(QStringLiteral("outputKey"))
            .toString().trimmed();
    const QJsonObject producer = runtimeContext.value(QStringLiteral("toolResultsById"))
            .toObject().value(producerId).toObject();
    if (producer.isEmpty() || !producer.value(QStringLiteral("success")).toBool(false)) {
        if (error)
            *error = QStringLiteral("bound producer is unavailable: %1").arg(producerId);
        return false;
    }
    const QJsonObject payload = producer.value(QStringLiteral("payload")).toObject();
    QJsonValue value = payload.value(outputKey);
    if (value.isUndefined())
        value = producer.value(outputKey);
    if (!finiteNumber(value, output)) {
        if (error)
            *error = QStringLiteral("bound output is invalid: %1.%2").arg(producerId, outputKey);
        return false;
    }
    const QString currentFrame = runtimeContext.value(QStringLiteral("frameId")).toString();
    const QString producerFrame = payload.value(QStringLiteral("frameId")).toString();
    if (!currentFrame.isEmpty() && !producerFrame.isEmpty() && currentFrame != producerFrame) {
        if (error)
            *error = QStringLiteral("bound output belongs to another frame");
        return false;
    }
    return true;
}

bool readPose(const QJsonObject &config,
              const QJsonObject &runtimeContext,
              CalibrationTransformPose *pose,
              QString *error)
{
    if (!pose)
        return false;
    pose->enabled = config.value(QStringLiteral("enabled")).toBool(false);
    if (!pose->enabled)
        return true;
    return resolveInput(config.value(QStringLiteral("x")).toObject(), runtimeContext,
                        &pose->x, error)
            && resolveInput(config.value(QStringLiteral("y")).toObject(), runtimeContext,
                            &pose->y, error)
            && resolveInput(config.value(QStringLiteral("joint0Angle")).toObject(), runtimeContext,
                            &pose->joint0AngleDeg, error)
            && resolveInput(config.value(QStringLiteral("joint1Angle")).toObject(), runtimeContext,
                            &pose->joint1AngleDeg, error);
}

bool singleProducerBinding(const QJsonObject &group,
                           const QStringList &keys,
                           QString *error)
{
    QSet<QString> producers;
    for (const QString &key : keys) {
        const QJsonObject binding = group.value(key).toObject();
        if (binding.value(QStringLiteral("mode")).toString() != QStringLiteral("binding"))
            continue;
        const QString producerId = binding.value(QStringLiteral("producerId")).toString().trimmed();
        if (!producerId.isEmpty())
            producers.insert(producerId);
    }
    if (producers.size() <= 1)
        return true;
    if (error)
        *error = QStringLiteral("同一坐标组禁止跨生产者混订");
    return false;
}

ToolResult failure(const ToolConfig &config,
                   const QString &status,
                   const QString &message)
{
    return ToolResult::error(config.toolId, config.toolType, message, status);
}

} // namespace

bool CalibrationTransformAdapter::supports(ToolType type) const
{
    return type == ToolType::CalibrationTransform;
}

ToolResult CalibrationTransformAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::CalibrationTransform)
        return failure(config, QStringLiteral("invalid_tool_type"),
                       QStringLiteral("CalibrationTransformAdapter仅支持标定转换工具"));
    const QJsonObject params = config.params.value(QStringLiteral("calibrationTransform")).toObject();
    const QString filePath = params.value(QStringLiteral("activeCalibrationFile"))
            .toString().trimmed();
    if (filePath.isEmpty())
        return failure(config, QStringLiteral("calibration_file_missing"),
                       QStringLiteral("尚未选择标定文件"));

    CalibrationModel model;
    QString loadError;
    ProjectXmlCalibrationLoader projectLoader;
    HikXmlCalibrationLoader hikXmlLoader;
    HikIwcalCalibrationLoader iwcalLoader;
    const CalibrationFileLoader *loader = nullptr;
    if (projectLoader.canLoad(filePath))
        loader = &projectLoader;
    else if (iwcalLoader.canLoad(filePath))
        loader = &iwcalLoader;
    else if (hikXmlLoader.canLoad(filePath))
        loader = &hikXmlLoader;
    if (!loader || !loader->load(filePath, &model, &loadError)) {
        return failure(config,
                       loadError.startsWith(QStringLiteral("unsupported_format"))
                       ? QStringLiteral("unsupported_format")
                       : QStringLiteral("calibration_file_invalid"),
                       loadError.isEmpty() ? QStringLiteral("无法识别标定文件") : loadError);
    }

    double inputX = 0.0;
    double inputY = 0.0;
    double inputAngle = 0.0;
    QString inputError;
    const QJsonObject inputGroup{
        {QStringLiteral("inputX"), params.value(QStringLiteral("inputX"))},
        {QStringLiteral("inputY"), params.value(QStringLiteral("inputY"))},
        {QStringLiteral("inputAngle"), params.value(QStringLiteral("inputAngle"))}
    };
    if (!singleProducerBinding(inputGroup,
                               {QStringLiteral("inputX"), QStringLiteral("inputY"),
                                QStringLiteral("inputAngle")}, &inputError)
            || !singleProducerBinding(params.value(QStringLiteral("calibrationPose")).toObject(),
                                      {QStringLiteral("x"), QStringLiteral("y"),
                                       QStringLiteral("joint0Angle"), QStringLiteral("joint1Angle")},
                                      &inputError)
            || !singleProducerBinding(params.value(QStringLiteral("runPose")).toObject(),
                                      {QStringLiteral("x"), QStringLiteral("y"),
                                       QStringLiteral("joint0Angle"), QStringLiteral("joint1Angle")},
                                      &inputError)) {
        return failure(config, QStringLiteral("mixed_binding_producers"), inputError);
    }
    if (!resolveInput(params.value(QStringLiteral("inputX")).toObject(),
                      request.runtimeContext, &inputX, &inputError)
            || !resolveInput(params.value(QStringLiteral("inputY")).toObject(),
                             request.runtimeContext, &inputY, &inputError)
            || !resolveInput(params.value(QStringLiteral("inputAngle")).toObject(),
                             request.runtimeContext, &inputAngle, &inputError)) {
        return failure(config, QStringLiteral("input_binding_invalid"), inputError);
    }
    CalibrationTransformPose calibrationPose;
    CalibrationTransformPose runPose;
    if (!readPose(params.value(QStringLiteral("calibrationPose")).toObject(),
                  request.runtimeContext, &calibrationPose, &inputError)
            || !readPose(params.value(QStringLiteral("runPose")).toObject(),
                         request.runtimeContext, &runPose, &inputError)) {
        return failure(config, QStringLiteral("pose_binding_invalid"), inputError);
    }

    const CalibrationTransformHalconResult converted = m_runner.run(
                model,
                params.value(QStringLiteral("coordinateType")).toString(QStringLiteral("image")),
                inputX,
                inputY,
                inputAngle,
                calibrationPose,
                runPose);
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::CalibrationTransform;
    result.success = converted.success;
    result.ok = converted.success;
    result.status = converted.status;
    result.message = converted.message;
    result.value = converted.outputX;
    result.elapsedMs = converted.elapsedMs;
    result.text = converted.success ? QStringLiteral("OK") : QStringLiteral("NG");
    result.payload = converted.payload;
    result.payload.insert(QStringLiteral("calibrationFile"), filePath);
    result.payload.insert(QStringLiteral("calibrationFormat"), loader->formatId());
    return result;
}
