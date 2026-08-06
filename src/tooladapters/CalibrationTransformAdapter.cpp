#include "tooladapters/CalibrationTransformAdapter.h"

#include "calibration/CalibrationFileLoader.h"

#include <QFileInfo>
#include <QJsonValue>
#include <QSet>
#include <QStringList>

#include <array>
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

QJsonValue valueAtObjectPath(const QJsonObject &object, const QString &path)
{
    const QStringList parts = path.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return QJsonValue();
    QJsonValue current(object);
    for (const QString &part : parts) {
        if (!current.isObject())
            return QJsonValue();
        current = current.toObject().value(part);
        if (current.isUndefined())
            return QJsonValue();
    }
    return current;
}

bool resolveInput(const QJsonObject &binding,
                  const QJsonObject &runtimeContext,
                  const QString &currentFrame,
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
    if (producer.isEmpty()
            || producer.value(QStringLiteral("toolId")).toString().trimmed() != producerId
            || !producer.value(QStringLiteral("success")).toBool(false)
            || !producer.value(QStringLiteral("ok")).toBool(false)) {
        if (error)
            *error = QStringLiteral("bound producer is unavailable: %1").arg(producerId);
        return false;
    }
    const QJsonObject payload = producer.value(QStringLiteral("payload")).toObject();
    QJsonValue value = valueAtObjectPath(payload, outputKey);
    if (value.isUndefined())
        value = valueAtObjectPath(producer, outputKey);
    if (!finiteNumber(value, output)) {
        if (error)
            *error = QStringLiteral("bound output is invalid: %1.%2").arg(producerId, outputKey);
        return false;
    }
    const QString producerFrame = payload.value(QStringLiteral("frameId")).toString();
    if (!currentFrame.isEmpty()
            && (producerFrame.isEmpty() || currentFrame != producerFrame)) {
        if (error)
            *error = QStringLiteral("bound output belongs to another frame");
        return false;
    }
    return true;
}

bool readPose(const QJsonObject &config,
              const QJsonObject &runtimeContext,
              const QString &currentFrame,
              CalibrationTransformPose *pose,
              QString *error)
{
    if (!pose)
        return false;
    pose->enabled = config.value(QStringLiteral("enabled")).toBool(false);
    if (!pose->enabled)
        return true;
    return resolveInput(config.value(QStringLiteral("x")).toObject(), runtimeContext,
                        currentFrame,
                        &pose->x, error)
            && resolveInput(config.value(QStringLiteral("y")).toObject(), runtimeContext,
                            currentFrame,
                            &pose->y, error)
            && resolveInput(config.value(QStringLiteral("joint0Angle")).toObject(), runtimeContext,
                            currentFrame,
                            &pose->joint0AngleDeg, error)
            && resolveInput(config.value(QStringLiteral("joint1Angle")).toObject(), runtimeContext,
                            currentFrame,
                            &pose->joint1AngleDeg, error);
}

bool resolveMainInputs(const QJsonObject &params,
                       const ToolRequest &request,
                       double *inputX,
                       double *inputY,
                       double *inputAngle,
                       QString *producerId,
                       ToolType *producerType,
                       QString *status,
                       QString *error)
{
    const QJsonObject xBinding = params.value(QStringLiteral("inputX")).toObject();
    const QJsonObject yBinding = params.value(QStringLiteral("inputY")).toObject();
    const QJsonObject angleBinding = params.value(QStringLiteral("inputAngle")).toObject();
    const std::array<QJsonObject, 3> bindings{{xBinding, yBinding, angleBinding}};
    for (const QJsonObject &binding : bindings) {
        if (binding.value(QStringLiteral("mode")).toString() != QStringLiteral("binding")) {
            if (status)
                *status = QStringLiteral("input_binding_required");
            if (error)
                *error = QStringLiteral("标定转换 X/Y/Angle 必须订阅前序图像坐标，不能使用常量");
            return false;
        }
    }

    const QString sourceId = xBinding.value(QStringLiteral("producerId")).toString().trimmed();
    if (sourceId.isEmpty()
            || yBinding.value(QStringLiteral("producerId")).toString().trimmed() != sourceId
            || angleBinding.value(QStringLiteral("producerId")).toString().trimmed() != sourceId) {
        if (status)
            *status = QStringLiteral("mixed_binding_producers");
        if (error)
            *error = QStringLiteral("标定转换 X/Y/Angle 必须来自同一个前序节点");
        return false;
    }

    const QJsonObject source = request.runtimeContext
            .value(QStringLiteral("toolResultsById")).toObject().value(sourceId).toObject();
    if (source.isEmpty()) {
        if (status)
            *status = QStringLiteral("source_unavailable");
        if (error)
            *error = QStringLiteral("订阅的前序坐标来源不可用: %1").arg(sourceId);
        return false;
    }
    const ToolType sourceType = toolTypeFromString(
                source.value(QStringLiteral("toolType")).toString());
    if (source.value(QStringLiteral("toolId")).toString().trimmed() != sourceId
            || (sourceType != ToolType::TemplateLocation
                && sourceType != ToolType::PositionCorrection)) {
        if (status)
            *status = QStringLiteral("source_invalid");
        if (error)
            *error = QStringLiteral("订阅来源身份或工具类型不符合坐标合同");
        return false;
    }

    const QStringList expectedKeys = sourceType == ToolType::TemplateLocation
            ? QStringList{QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("angle")}
            : QStringList{QStringLiteral("runPose.x"), QStringLiteral("runPose.y"),
                          QStringLiteral("runPose.angleDeg")};
    const QStringList actualKeys{
        xBinding.value(QStringLiteral("outputKey")).toString(),
        yBinding.value(QStringLiteral("outputKey")).toString(),
        angleBinding.value(QStringLiteral("outputKey")).toString()
    };
    if (actualKeys != expectedKeys) {
        if (status)
            *status = QStringLiteral("input_binding_contract_invalid");
        if (error)
            *error = QStringLiteral("订阅字段不符合该前序节点的 X/Y/Angle 坐标合同");
        return false;
    }
    if (!source.value(QStringLiteral("success")).toBool(false)
            || !source.value(QStringLiteral("ok")).toBool(false)) {
        if (status)
            *status = QStringLiteral("source_not_ok");
        if (error) {
            const QString message = source.value(QStringLiteral("message")).toString().trimmed();
            *error = message.isEmpty() ? QStringLiteral("前序坐标来源本帧未输出 OK 结果")
                                       : message;
        }
        return false;
    }
    const QJsonObject payload = source.value(QStringLiteral("payload")).toObject();
    const QString currentFrame = request.frameId.trimmed().isEmpty()
            ? request.runtimeContext.value(QStringLiteral("frameId")).toString().trimmed()
            : request.frameId.trimmed();
    const QString sourceFrame = payload.value(QStringLiteral("frameId")).toString().trimmed();
    if (!currentFrame.isEmpty()
            && (sourceFrame.isEmpty() || sourceFrame != currentFrame)) {
        if (status)
            *status = QStringLiteral("source_frame_mismatch");
        if (error)
            *error = QStringLiteral("前序坐标来源不属于当前帧");
        return false;
    }
    if (sourceType == ToolType::TemplateLocation) {
        const QString coordinateSystem = payload.value(QStringLiteral("coordinateSystem"))
                .toString().trimmed();
        const QString angleUnit = payload.value(QStringLiteral("angleUnit"))
                .toString().trimmed();
        if (coordinateSystem != QStringLiteral("image_pixel")
                || angleUnit != QStringLiteral("degree")) {
            if (status)
                *status = QStringLiteral("source_coordinate_contract_invalid");
            if (error)
                *error = QStringLiteral("前序输出不是图像像素坐标或角度单位不是 degree");
            return false;
        }
    }
    if (!finiteNumber(valueAtObjectPath(payload, expectedKeys.at(0)), inputX)
            || !finiteNumber(valueAtObjectPath(payload, expectedKeys.at(1)), inputY)
            || !finiteNumber(valueAtObjectPath(payload, expectedKeys.at(2)), inputAngle)) {
        if (status)
            *status = QStringLiteral("input_binding_invalid");
        if (error)
            *error = QStringLiteral("前序坐标来源缺少有限的 X/Y/Angle 输出");
        return false;
    }
    if (producerId)
        *producerId = sourceId;
    if (producerType)
        *producerType = sourceType;
    return true;
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
    const QFileInfo calibrationInfo(filePath);
    if (!calibrationInfo.exists() || !calibrationInfo.isFile())
        return failure(config, QStringLiteral("calibration_file_missing"),
                       QStringLiteral("标定文件不存在: %1").arg(filePath));

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
    const QString coordinateType = params.value(QStringLiteral("coordinateType"))
            .toString(QStringLiteral("image"));
    if (coordinateType == QStringLiteral("physical")) {
        return failure(config, QStringLiteral("physical_coordinate_unsupported"),
                       QStringLiteral("当前标定转换仅支持图像像素坐标转物理坐标"));
    }
    if (coordinateType != QStringLiteral("image"))
        return failure(config, QStringLiteral("invalid_coordinate_type"),
                       QStringLiteral("坐标类型必须为 image"));

    const QJsonObject fingerprint = model.imageBinding
            .value(QStringLiteral("inputFingerprint")).toObject();
    const int expectedWidth = fingerprint.value(QStringLiteral("width")).toInt(
                model.imageBinding.value(QStringLiteral("imageWidth")).toInt());
    const int expectedHeight = fingerprint.value(QStringLiteral("height")).toInt(
                model.imageBinding.value(QStringLiteral("imageHeight")).toInt());
    if (expectedWidth > 0 && expectedHeight > 0) {
        if (request.image.empty()) {
            ToolResult result = failure(config, QStringLiteral("calibration_image_missing"),
                                        QStringLiteral("标定文件包含图像尺寸绑定，但当前运行图像为空"));
            result.payload.insert(QStringLiteral("expectedImageWidth"), expectedWidth);
            result.payload.insert(QStringLiteral("expectedImageHeight"), expectedHeight);
            return result;
        }
        if (request.image.cols != expectedWidth || request.image.rows != expectedHeight) {
            ToolResult result = failure(config, QStringLiteral("calibration_stale"),
                                        QStringLiteral("当前图像尺寸与标定图像不一致，需重新标定"));
            result.payload.insert(QStringLiteral("expectedImageWidth"), expectedWidth);
            result.payload.insert(QStringLiteral("expectedImageHeight"), expectedHeight);
            result.payload.insert(QStringLiteral("actualImageWidth"), request.image.cols);
            result.payload.insert(QStringLiteral("actualImageHeight"), request.image.rows);
            return result;
        }
    }

    double inputX = 0.0;
    double inputY = 0.0;
    double inputAngle = 0.0;
    QString inputError;
    QString inputStatus;
    QString inputProducerId;
    ToolType inputProducerType = ToolType::Unknown;
    if (!singleProducerBinding(params.value(QStringLiteral("calibrationPose")).toObject(),
                                      {QStringLiteral("x"), QStringLiteral("y"),
                                       QStringLiteral("joint0Angle"), QStringLiteral("joint1Angle")},
                                      &inputError)
            || !singleProducerBinding(params.value(QStringLiteral("runPose")).toObject(),
                                      {QStringLiteral("x"), QStringLiteral("y"),
                                       QStringLiteral("joint0Angle"), QStringLiteral("joint1Angle")},
                                      &inputError)) {
        return failure(config, QStringLiteral("mixed_binding_producers"), inputError);
    }
    if (!resolveMainInputs(params, request,
                           &inputX, &inputY, &inputAngle,
                           &inputProducerId, &inputProducerType,
                           &inputStatus, &inputError)) {
        return failure(config,
                       inputStatus.isEmpty() ? QStringLiteral("input_binding_invalid")
                                             : inputStatus,
                       inputError);
    }
    CalibrationTransformPose calibrationPose;
    CalibrationTransformPose runPose;
    const QString currentFrame = request.frameId.trimmed().isEmpty()
            ? request.runtimeContext.value(QStringLiteral("frameId")).toString().trimmed()
            : request.frameId.trimmed();
    if (!readPose(params.value(QStringLiteral("calibrationPose")).toObject(),
                  request.runtimeContext, currentFrame, &calibrationPose, &inputError)
            || !readPose(params.value(QStringLiteral("runPose")).toObject(),
                         request.runtimeContext, currentFrame, &runPose, &inputError)) {
        return failure(config, QStringLiteral("pose_binding_invalid"), inputError);
    }

    const CalibrationTransformHalconResult converted = m_runner.run(
                model,
                coordinateType,
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
    result.payload.insert(QStringLiteral("inputProducerId"), inputProducerId);
    result.payload.insert(QStringLiteral("inputProducerType"),
                          toolTypeToString(inputProducerType));
    return result;
}
