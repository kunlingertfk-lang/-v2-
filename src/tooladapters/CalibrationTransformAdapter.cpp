#include "tooladapters/CalibrationTransformAdapter.h"

#include "calibration/CalibrationFileLoader.h"
#include "calibration/CalibrationSourceFingerprint.h"

#include <QFileInfo>
#include <QJsonValue>
#include <QSet>
#include <QStringList>

#include <array>
#include <cmath>

namespace {

// 严格读取 ToolConfig/runtimeContext 中的有限数值。
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

// 按点分隔路径读取嵌套 JSON 字段。
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

// 解析 constant 或同帧 tool_result 绑定，并返回可定位的失败原因。
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

// 解析机构位姿 X/Y/Joint0/Joint1 的四个绑定。
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

// 从同一个前序 ToolResult 解析主 X/Y/(Angle)，并强制工具类型、字段和 frameId 合同。
bool resolveMainInputs(const QJsonObject &params,
                       const ToolRequest &request,
                       bool angleRequired,
                       double *inputX,
                       double *inputY,
                       double *inputAngle,
                       QString *producerId,
                       ToolType *producerType,
                       QJsonObject *producerPayload,
                       QString *status,
                       QString *error)
{
    const QJsonObject xBinding = params.value(QStringLiteral("inputX")).toObject();
    const QJsonObject yBinding = params.value(QStringLiteral("inputY")).toObject();
    const QJsonObject angleBinding = params.value(QStringLiteral("inputAngle")).toObject();
    const std::array<QJsonObject, 2> bindings{{xBinding, yBinding}};
    for (const QJsonObject &binding : bindings) {
        if (binding.value(QStringLiteral("mode")).toString() != QStringLiteral("binding")) {
            if (status)
                *status = QStringLiteral("input_binding_required");
            if (error)
                *error = QStringLiteral("标定转换 X/Y 必须订阅前序图像坐标，不能使用常量");
            return false;
        }
    }

    const QString sourceId = xBinding.value(QStringLiteral("producerId")).toString().trimmed();
    if (sourceId.isEmpty()
            || yBinding.value(QStringLiteral("producerId")).toString().trimmed() != sourceId
            || (angleRequired
                && (angleBinding.value(QStringLiteral("mode")).toString()
                    != QStringLiteral("binding")
                    || angleBinding.value(QStringLiteral("producerId"))
                       .toString().trimmed() != sourceId))) {
        if (status)
            *status = QStringLiteral("mixed_binding_producers");
        if (error)
            *error = angleRequired
                    ? QStringLiteral("姿态映射的 X/Y/Angle 必须来自同一个前序节点")
                    : QStringLiteral("标定转换 X/Y 必须来自同一个前序节点");
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
    QStringList actualKeys{
        xBinding.value(QStringLiteral("outputKey")).toString(),
        yBinding.value(QStringLiteral("outputKey")).toString()
    };
    if (angleRequired)
        actualKeys.append(angleBinding.value(QStringLiteral("outputKey")).toString());
    QStringList requiredKeys{expectedKeys.at(0), expectedKeys.at(1)};
    if (angleRequired)
        requiredKeys.append(expectedKeys.at(2));
    if (actualKeys != requiredKeys) {
        if (status)
            *status = QStringLiteral("input_binding_contract_invalid");
        if (error)
            *error = angleRequired
                    ? QStringLiteral("订阅字段不符合该前序节点的 X/Y/Angle 坐标合同")
                    : QStringLiteral("订阅字段不符合该前序节点的 X/Y 坐标合同");
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
                || (angleRequired && angleUnit != QStringLiteral("degree"))) {
            if (status)
                *status = QStringLiteral("source_coordinate_contract_invalid");
            if (error)
                *error = angleRequired
                        ? QStringLiteral("前序输出不是图像像素坐标或角度单位不是 degree")
                        : QStringLiteral("前序输出不是图像像素坐标");
            return false;
        }
    }
    if (!finiteNumber(valueAtObjectPath(payload, expectedKeys.at(0)), inputX)
            || !finiteNumber(valueAtObjectPath(payload, expectedKeys.at(1)), inputY)
            || (angleRequired
                && !finiteNumber(valueAtObjectPath(payload, expectedKeys.at(2)),
                                 inputAngle))) {
        if (status)
            *status = QStringLiteral("input_binding_invalid");
        if (error)
            *error = angleRequired
                    ? QStringLiteral("前序坐标来源缺少有限的 X/Y/Angle 输出")
                    : QStringLiteral("前序坐标来源缺少有限的 X/Y 输出");
        return false;
    }
    if (!angleRequired)
        *inputAngle = 0.0;
    if (producerId)
        *producerId = sourceId;
    if (producerType)
        *producerType = sourceType;
    if (producerPayload)
        *producerPayload = payload;
    return true;
}

// 穿透位置修正中间节点，还原真正模板定位来源并构造运行时指纹。
QJsonObject effectiveCoordinateSourceFingerprint(
        const QString &inputProducerId,
        ToolType inputProducerType,
        const QJsonObject &inputProducerPayload,
        const QJsonObject &directOutputContract,
        QString *error)
{
    QString effectiveProducerId = inputProducerId;
    ToolType effectiveProducerType = inputProducerType;
    if (inputProducerType == ToolType::PositionCorrection) {
        effectiveProducerId = inputProducerPayload
                .value(QStringLiteral("poseProducerId")).toString().trimmed();
        effectiveProducerType = ToolType::TemplateLocation;
        if (effectiveProducerId.isEmpty()) {
            if (error) {
                *error = QStringLiteral(
                            "位置修正结果缺少 poseProducerId，无法验证标定来源");
            }
            return QJsonObject();
        }
    }
    QJsonObject outputContract = directOutputContract;
    if (inputProducerType == ToolType::PositionCorrection) {
        const QJsonObject propagatedContract = inputProducerPayload.value(
                    QStringLiteral("coordinateSourceOutputContract")).toObject();
        if (!propagatedContract.isEmpty())
            outputContract = propagatedContract;
    }
    return CalibrationSourceFingerprint::makeFingerprint(
                effectiveProducerId,
                effectiveProducerType,
                inputProducerPayload,
                outputContract);
}

// 将配置时来源验证状态写入结果 payload，便于 UI 区分已验证与不可验证。
void annotateBindingValidation(ToolResult *result,
                               bool verified,
                               const QString &status,
                               const QString &warning = QString())
{
    if (!result)
        return;
    result->payload.insert(QStringLiteral("calibrationBindingVerified"), verified);
    result->payload.insert(QStringLiteral("calibrationBindingStatus"), status);
    result->payload.insert(QStringLiteral("calibrationSourceValidation"), status);
    if (!warning.trimmed().isEmpty())
        result->payload.insert(QStringLiteral("calibrationBindingWarning"), warning);
}

// 校验主 X/Y/(Angle) 全部来自同一个前序工具结果。
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

// 构造带稳定状态码的标定转换错误结果。
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
    // 适配层先完成所有文件、订阅与来源门禁，Runner 只接收已归一化的强类型输入。
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
        QString loadStatus = QStringLiteral("calibration_file_invalid");
        if (loadError.startsWith(
                    QStringLiteral("unsupported_calibration_schema"))) {
            loadStatus = QStringLiteral("unsupported_calibration_schema");
        } else if (loadError.startsWith(
                       QStringLiteral("invalid_calibration_structure"))) {
            loadStatus = QStringLiteral("invalid_calibration_structure");
        } else if (loadError.startsWith(QStringLiteral("unsupported_format"))) {
            loadStatus = QStringLiteral("unsupported_format");
        }
        return failure(config,
                       loadStatus,
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
    QJsonObject inputProducerPayload;
    const bool inputAngleRequired = model.mode
            == NPointCalibrationMode::TwelvePointPoseMapping;
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
    if (!resolveMainInputs(params, request, inputAngleRequired,
                           &inputX, &inputY, &inputAngle,
                           &inputProducerId, &inputProducerType,
                           &inputProducerPayload,
                           &inputStatus, &inputError)) {
        return failure(config,
                       inputStatus.isEmpty() ? QStringLiteral("input_binding_invalid")
                                             : inputStatus,
                       inputError);
    }

    bool calibrationBindingVerified = false;
    QString calibrationBindingStatus = QStringLiteral("legacy_unverifiable");
    QString calibrationBindingWarning = QStringLiteral(
                "该标定文件未记录坐标来源指纹，已执行转换，但无法确认模板原点或配置是否发生变化");
    QJsonObject expectedSourceFingerprint = model.imageBinding
            .value(QStringLiteral("coordinateSourceFingerprint")).toObject();
    if (expectedSourceFingerprint.isEmpty()) {
        expectedSourceFingerprint = fingerprint
                .value(QStringLiteral("coordinateSourceFingerprint")).toObject();
    }
    if (!expectedSourceFingerprint.isEmpty()) {
        const QString expectedSignature = expectedSourceFingerprint
                .value(QStringLiteral("coordinateSourceSignature"))
                .toString().trimmed();
        const QString expectedMode = expectedSourceFingerprint
                .value(QStringLiteral("mode")).toString().trimmed();
        if (expectedMode == QStringLiteral("manual") && expectedSignature.isEmpty()) {
            calibrationBindingStatus = QStringLiteral("manual_unverifiable");
            calibrationBindingWarning = QStringLiteral(
                        "该标定文件由手动坐标生成，无法验证当前模板定位来源");
        } else if (expectedSignature.isEmpty()
                   || expectedSignature
                      != CalibrationSourceFingerprint::coordinateSourceSignature(
                          expectedSourceFingerprint)
                   || expectedSourceFingerprint
                      .value(QStringLiteral("producerId")).toString().trimmed().isEmpty()
                   || expectedSourceFingerprint
                      .value(QStringLiteral("producerType")).toString().trimmed().isEmpty()
                   || expectedSourceFingerprint
                      .value(QStringLiteral("outputContract")).toObject().isEmpty()
                   || expectedSourceFingerprint
                      .value(QStringLiteral("originMode")).toString().trimmed().isEmpty()
                   || expectedSourceFingerprint
                      .value(QStringLiteral("customOriginNormalized")).toObject().isEmpty()
                   || expectedSourceFingerprint
                      .value(QStringLiteral("modelSignature")).toString().trimmed().isEmpty()
                   || expectedSourceFingerprint
                      .value(QStringLiteral("coordinateSourceConfigSignature"))
                      .toString().trimmed().isEmpty()) {
            ToolResult result = failure(
                        config,
                        QStringLiteral("calibration_stale"),
                        QStringLiteral("标定文件的坐标来源指纹不完整，需重新标定"));
            annotateBindingValidation(&result, false,
                                      QStringLiteral("fingerprint_invalid"));
            result.payload.insert(QStringLiteral("expectedCoordinateSourceFingerprint"),
                                  expectedSourceFingerprint);
            return result;
        } else {
            QString fingerprintError;
            QJsonObject directOutputContract =
                    CalibrationSourceFingerprint::templateOutputContract();
            directOutputContract.insert(
                        QStringLiteral("x"),
                        params.value(QStringLiteral("inputX")).toObject()
                        .value(QStringLiteral("outputKey")).toString());
            directOutputContract.insert(
                        QStringLiteral("y"),
                        params.value(QStringLiteral("inputY")).toObject()
                        .value(QStringLiteral("outputKey")).toString());
            directOutputContract.insert(
                        QStringLiteral("angle"),
                        inputProducerType == ToolType::PositionCorrection
                        ? QStringLiteral("runPose.angleDeg")
                        : QStringLiteral("angle"));
            const QJsonObject actualSourceFingerprint =
                    effectiveCoordinateSourceFingerprint(
                        inputProducerId,
                        inputProducerType,
                        inputProducerPayload,
                        directOutputContract,
                        &fingerprintError);
            QString mismatchField;
            if (actualSourceFingerprint.isEmpty()
                    || !CalibrationSourceFingerprint::matches(
                        expectedSourceFingerprint,
                        actualSourceFingerprint,
                        &mismatchField)) {
                ToolResult result = failure(
                            config,
                            QStringLiteral("calibration_stale"),
                            fingerprintError.isEmpty()
                            ? QStringLiteral("当前坐标来源与标定时不一致（%1），需重新标定")
                              .arg(mismatchField.isEmpty()
                                   ? QStringLiteral("unknown") : mismatchField)
                            : fingerprintError);
                annotateBindingValidation(&result, false,
                                          QStringLiteral("fingerprint_mismatch"));
                result.payload.insert(QStringLiteral("calibrationBindingMismatchField"),
                                      mismatchField);
                result.payload.insert(QStringLiteral("expectedCoordinateSourceFingerprint"),
                                      expectedSourceFingerprint);
                result.payload.insert(QStringLiteral("actualCoordinateSourceFingerprint"),
                                      actualSourceFingerprint);
                return result;
            }
            if (expectedSourceFingerprint.contains(
                        QStringLiteral("referenceImageSignature"))) {
                calibrationBindingVerified = true;
                calibrationBindingStatus = QStringLiteral("verified");
                calibrationBindingWarning.clear();
            } else {
                calibrationBindingVerified = false;
                calibrationBindingStatus =
                        QStringLiteral("reference_unverifiable");
                calibrationBindingWarning = QStringLiteral(
                            "该坐标来源签名来自兼容旧版，未记录基准图内容签名；"
                            "已核对模板原点与配置，但无法完整验证基准图");
            }
        }
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
                runPose,
                params.value(QStringLiteral("allowBoundaryForProduction"))
                .toBool(false));
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::CalibrationTransform;
    result.success = converted.success;
    result.ok = converted.productionAllowed;
    result.status = converted.status;
    result.message = converted.message;
    if (converted.coordinateAvailable)
        result.value = converted.outputX;
    result.elapsedMs = converted.elapsedMs;
    result.text = result.ok ? QStringLiteral("OK") : QStringLiteral("NG");
    result.payload = converted.payload;
    result.payload.insert(QStringLiteral("calibrationFile"), filePath);
    result.payload.insert(QStringLiteral("calibrationFormat"), loader->formatId());
    result.payload.insert(QStringLiteral("calibrationSchemaVersion"),
                          model.schemaVersion);
    result.payload.insert(QStringLiteral("inputProducerId"), inputProducerId);
    result.payload.insert(QStringLiteral("inputProducerType"),
                          toolTypeToString(inputProducerType));
    result.payload.insert(QStringLiteral("inputAngleRequired"),
                          inputAngleRequired);
    annotateBindingValidation(&result,
                              calibrationBindingVerified,
                              calibrationBindingStatus,
                              calibrationBindingWarning);
    return result;
}
