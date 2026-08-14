#include "ToolEngine.h"

#include "algorithms/location/ReferenceTemplateLocationConfig.h"
#include "algorithms/location/PositionCorrectionHalconRunner.h"
#include "algorithms/location/TemplateLocationHalconRunner.h"
#include "toolcore/PositionCorrection.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>

#include <cmath>

namespace {

// 读取嵌套对象；缺失或类型不符时返回空对象。
QJsonObject ensureObject(const QJsonObject &parent, const QString &key)
{
    return parent.value(key).toObject();
}

// 将本帧结果登记到 toolResultsById；这是标定转换“订阅”前序坐标的进程内通信总线。
void registerResultInContext(const ToolResult &result, QJsonObject *context)
{
    if (!context || result.toolId.trimmed().isEmpty())
        return;

    QJsonObject toolResults = ensureObject(*context, QStringLiteral("toolResultsById"));
    toolResults.insert(result.toolId, result.toJson());
    context->insert(QStringLiteral("toolResultsById"), toolResults);

    // Keep both successful and failed position-correction results in the frame
    // context. Consumers must be able to distinguish "source did not run" from
    // "source ran, but template location failed".
    if (result.toolType == ToolType::PositionCorrection) {
        const QString sourceId = result.payload.value(QStringLiteral("sourceId"))
                .toString(result.toolId).trimmed();
        if (!sourceId.isEmpty()) {
            QJsonObject corrections = ensureObject(*context,
                                                   QStringLiteral("positionCorrectionsById"));
            corrections.insert(sourceId, result.toJson());
            context->insert(QStringLiteral("positionCorrectionsById"), corrections);
        }
    }
}

bool finiteJsonNumber(const QJsonValue &value, double *number)
{
    if (!number)
        return false;
    bool ok = true;
    const double parsed = value.isString()
            ? value.toString().toDouble(&ok)
            : value.isDouble() ? value.toDouble() : qQNaN();
    if (!ok || !std::isfinite(parsed))
        return false;
    *number = parsed;
    return true;
}

// 从方案级参考位姿配置读取有限 X/Y/Angle 及可选正 Scale。
bool poseFromJson(const QJsonObject &object,
                  PositionPose *pose,
                  bool *invalidScale = nullptr)
{
    if (invalidScale)
        *invalidScale = false;
    if (!pose || object.isEmpty())
        return false;

    const QJsonValue angleValue = object.contains(QStringLiteral("angleDeg"))
            ? object.value(QStringLiteral("angleDeg"))
            : object.value(QStringLiteral("angle"));
    if (!finiteJsonNumber(object.value(QStringLiteral("x")), &pose->x)
            || !finiteJsonNumber(object.value(QStringLiteral("y")), &pose->y)
            || !finiteJsonNumber(angleValue, &pose->angleDeg)) {
        return false;
    }

    pose->scale = 1.0;
    if (object.contains(QStringLiteral("scale"))
            && (!finiteJsonNumber(object.value(QStringLiteral("scale")), &pose->scale)
                || pose->scale <= 0.0)) {
        if (invalidScale)
            *invalidScale = true;
        return false;
    }
    return true;
}

bool validNormalizedPoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y())
            && point.x() >= 0.0 && point.x() <= 1.0
            && point.y() >= 0.0 && point.y() <= 1.0;
}

bool normalizedPointFromJson(const QJsonValue &value, QPointF *point)
{
    if (!point || !value.isObject())
        return false;
    const QJsonObject object = value.toObject();
    double x = 0.0;
    double y = 0.0;
    if (!finiteJsonNumber(object.value(QStringLiteral("x")), &x)
            || !finiteJsonNumber(object.value(QStringLiteral("y")), &y)) {
        return false;
    }
    *point = QPointF(x, y);
    return validNormalizedPoint(*point);
}

ToolResult referencePositionCorrectionError(const QString &status,
                                            const QString &message)
{
    ToolResult result;
    result.toolId = PositionCorrection::defaultSourceId();
    result.toolType = ToolType::PositionCorrection;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.payload.insert(QStringLiteral("sourceKind"), QStringLiteral("reference"));
    result.payload.insert(QStringLiteral("sourceId"), PositionCorrection::defaultSourceId());
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), status);
    return result;
}

// 在普通工具链之前运行方案级参考位置修正，并把结果注入同帧上下文。
ToolResult runReferencePositionCorrection(const cv::Mat &image,
                                          const cv::Mat &referenceImage,
                                          const QMap<QString, cv::Mat> &referenceImages,
                                          const QMap<QString, QString> &referenceRevisions,
                                          const QString &primaryReferenceBaseId,
                                          const QJsonObject &runtimeContext)
{
    const QJsonObject referenceJson = runtimeContext.value(
                QStringLiteral("referencePositionCorrection")).toObject();
    const ReferencePositionCorrectionConfig referenceConfig =
            PositionCorrection::referenceFromJson(referenceJson);
    if (!referenceConfig.enabled)
        return ToolResult();
    const ReferenceTemplateLocationConfigResult resolved =
            ReferenceTemplateLocationConfig::fromJson(referenceJson,
                                                      referenceConfig);
    if (!resolved.supported)
        return referencePositionCorrectionError(resolved.status,
                                                resolved.message);
    if (!resolved.valid)
        return referencePositionCorrectionError(resolved.status,
                                                resolved.message);
    const TemplateLocationModelBankConfig &locatorConfig = resolved.bank;
    if (!referenceConfig.referenceCreated)
        return referencePositionCorrectionError(
                    QStringLiteral("reference_pose_missing"),
                    QStringLiteral("Reference position correction has not been created."));
    if (image.empty())
        return referencePositionCorrectionError(QStringLiteral("image_empty"),
                                                QStringLiteral("input image is empty"));
    if (locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion
            && referenceImage.empty())
        return referencePositionCorrectionError(QStringLiteral("no_reference_image"),
                                                QStringLiteral("reference image is empty"));

    QJsonObject frozenPose;
    bool hasIdentityVersion = false;
    bool versionedIdentity = false;
    QString frozenModelSignature;
    QString frozenTemplateId;
    QString frozenOriginMode;
    QPointF frozenCustomOrigin;
    bool hasFrozenCustomOrigin = false;
    bool validFrozenCustomOrigin = false;
    if (resolved.bankEnvelope) {
        QStringList notReadyTemplateIds;
        if (!TemplateLocationConfig::allEnabledModelsReady(
                    locatorConfig, &notReadyTemplateIds)) {
            return referencePositionCorrectionError(
                        QStringLiteral("reference_pose_bank_incomplete"),
                        QStringLiteral("Enabled reference template model(s) are not ready: %1")
                        .arg(notReadyTemplateIds.join(QStringLiteral(", "))));
        }
        for (const TemplateLocationTemplateConfig &item : locatorConfig.templates) {
            if (!item.enabled)
                continue;
            if (!resolved.referencePosesByTemplateId.value(
                    item.templateId).isObject()) {
                return referencePositionCorrectionError(
                            QStringLiteral("reference_pose_bank_incomplete"),
                            QStringLiteral("Enabled template '%1' has no frozen reference pose.")
                            .arg(item.templateId));
            }
        }
        QString invalidTemplateId;
        QString identityMessage;
        if (!ReferenceTemplateLocationConfig::validateAllEnabledFrozenPoses(
                    locatorConfig, resolved.referencePosesByTemplateId,
                    &invalidTemplateId, &identityMessage)) {
            return referencePositionCorrectionError(
                        QStringLiteral("invalid_reference_locator_identity"),
                        identityMessage.trimmed().isEmpty()
                        ? QStringLiteral("Enabled template '%1' has an invalid frozen pose identity.")
                          .arg(invalidTemplateId)
                        : identityMessage);
        }
    } else {
        if (referenceConfig.referencePose.isEmpty()) {
            return referencePositionCorrectionError(
                        QStringLiteral("reference_pose_missing"),
                        QStringLiteral("reference position pose is missing"));
        }
        if (referenceConfig.originMode != QStringLiteral("centroid")
                && referenceConfig.originMode != QStringLiteral("custom")) {
            return referencePositionCorrectionError(
                        QStringLiteral("invalid_custom_origin"),
                        QStringLiteral("reference origin mode is invalid"));
        }
        if (referenceConfig.originMode == QStringLiteral("custom")
                && !validNormalizedPoint(referenceConfig.customOriginNormalized)) {
            return referencePositionCorrectionError(
                        QStringLiteral("invalid_custom_origin"),
                        QStringLiteral("reference custom origin is invalid"));
        }

        frozenPose = referenceConfig.referencePose;
        hasIdentityVersion = frozenPose.contains(
                QStringLiteral("locatorIdentityVersion"));
        const QJsonValue identityVersionValue = frozenPose.value(
                QStringLiteral("locatorIdentityVersion"));
        versionedIdentity = hasIdentityVersion &&
                identityVersionValue.isDouble() &&
                identityVersionValue.toInt(-1) == 1 &&
                identityVersionValue.toDouble() == 1.0;
        frozenModelSignature = frozenPose.value(
                QStringLiteral("locatorModelSignature")).toString().trimmed();
        frozenTemplateId = frozenPose.value(
                QStringLiteral("locatorTemplateId")).toString().trimmed();
        frozenOriginMode = frozenPose.value(
                QStringLiteral("locatorOriginMode")).toString().trimmed();
        hasFrozenCustomOrigin = frozenPose.contains(
                QStringLiteral("locatorCustomOriginNormalized"));
        validFrozenCustomOrigin = hasFrozenCustomOrigin &&
                normalizedPointFromJson(
                    frozenPose.value(
                        QStringLiteral("locatorCustomOriginNormalized")),
                    &frozenCustomOrigin);
        if (hasIdentityVersion &&
                (!versionedIdentity || frozenModelSignature.isEmpty()
                 || frozenTemplateId.isEmpty()
                 || (frozenOriginMode != QStringLiteral("centroid") &&
                     frozenOriginMode != QStringLiteral("custom"))
                 || (frozenOriginMode == QStringLiteral("custom") &&
                     !validFrozenCustomOrigin))) {
            return referencePositionCorrectionError(
                        QStringLiteral("invalid_reference_locator_identity"),
                        QStringLiteral("The versioned reference locator identity is incomplete or invalid."));
        }
    }

    TemplateLocationHalconRunner locator;
    const TemplateLocationHalconResult located =
            locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion
            ? locator.run(image, referenceImages, referenceRevisions,
                          locatorConfig, primaryReferenceBaseId)
            : locator.run(image, referenceImage, locatorConfig);
    if (!located.success || !located.ok) {
        ToolResult result = referencePositionCorrectionError(
                    located.status,
                    located.message.trimmed().isEmpty()
                    ? QStringLiteral("reference position locator failed")
                    : located.message);
        result.elapsedMs = located.elapsedMs;
        result.payload.insert(QStringLiteral("locatorPayload"), located.payload);
        return result;
    }

    const QString currentTemplateId = located.payload.value(
                QStringLiteral("selectedTemplateId")).toString().trimmed();
    QJsonObject currentPoseJson = located.payload.value(
                QStringLiteral("pose")).toObject();
    if (resolved.bankEnvelope) {
        const QJsonValue templateResultsValue = located.payload.value(
                    QStringLiteral("templateResults"));
        QJsonObject currentSignaturesByTemplateId;
        bool validTemplateResults = templateResultsValue.isArray();
        if (validTemplateResults) {
            const QJsonArray templateResults = templateResultsValue.toArray();
            for (const QJsonValue &value : templateResults) {
                if (!value.isObject()) {
                    validTemplateResults = false;
                    break;
                }
                const QJsonObject templateResult = value.toObject();
                const QString templateId = templateResult.value(
                            QStringLiteral("templateId")).toString().trimmed();
                const QString modelSignature = templateResult.value(
                            QStringLiteral("modelSignature")).toString().trimmed();
                const TemplateLocationTemplateConfig *item =
                        TemplateLocationConfig::findTemplate(
                            locatorConfig, templateId);
                if (templateId.isEmpty() || modelSignature.isEmpty()
                        || currentSignaturesByTemplateId.contains(templateId)
                        || !item || !item->enabled) {
                    validTemplateResults = false;
                    break;
                }
                currentSignaturesByTemplateId.insert(templateId,
                                                     modelSignature);
            }
        }
        if (!validTemplateResults ||
                currentSignaturesByTemplateId.size() !=
                TemplateLocationConfig::enabledTemplateCount(locatorConfig)) {
            ToolResult result = referencePositionCorrectionError(
                        QStringLiteral("invalid_reference_locator_identity"),
                        QStringLiteral("The locator result does not contain one model signature for every enabled reference template."));
            result.elapsedMs = located.elapsedMs;
            result.payload.insert(QStringLiteral("locatorPayload"), located.payload);
            return result;
        }
        for (const TemplateLocationTemplateConfig &item : locatorConfig.templates) {
            if (!item.enabled)
                continue;
            const QString currentSignature = currentSignaturesByTemplateId.value(
                        item.templateId).toString().trimmed();
            if (currentSignature.isEmpty()) {
                ToolResult result = referencePositionCorrectionError(
                            QStringLiteral("invalid_reference_locator_identity"),
                            QStringLiteral("Enabled template '%1' has no runtime model signature.")
                            .arg(item.templateId));
                result.elapsedMs = located.elapsedMs;
                result.payload.insert(QStringLiteral("locatorPayload"),
                                      located.payload);
                return result;
            }
            const QString frozenSignature =
                    resolved.referencePosesByTemplateId.value(item.templateId)
                    .toObject().value(QStringLiteral("locatorModelSignature"))
                    .toString().trimmed();
            if (frozenSignature != currentSignature) {
                ToolResult result = referencePositionCorrectionError(
                            QStringLiteral("reference_locator_changed"),
                            QStringLiteral("Enabled reference template '%1' no longer matches its frozen reference pose.")
                            .arg(item.templateId));
                result.elapsedMs = located.elapsedMs;
                result.payload.insert(QStringLiteral("locatorPayload"),
                                      located.payload);
                return result;
            }
        }

        const QString poseTemplateId = currentPoseJson.value(
                    QStringLiteral("templateId")).toString().trimmed();
        if (currentTemplateId.isEmpty() || poseTemplateId != currentTemplateId) {
            ToolResult result = referencePositionCorrectionError(
                        QStringLiteral("reference_pose_template_mismatch"),
                        QStringLiteral("The locator primary pose and selected template ID do not match."));
            result.elapsedMs = located.elapsedMs;
            result.payload.insert(QStringLiteral("locatorPayload"), located.payload);
            return result;
        }
        frozenPose = resolved.referencePosesByTemplateId.value(
                    currentTemplateId).toObject();
        const QString currentTemplateSignature = currentPoseJson.value(
                    QStringLiteral("templateModelSignature"))
                .toString().trimmed();
        if (currentTemplateSignature.isEmpty() ||
                currentSignaturesByTemplateId.value(currentTemplateId)
                .toString().trimmed() != currentTemplateSignature) {
            ToolResult result = referencePositionCorrectionError(
                        QStringLiteral("invalid_reference_locator_identity"),
                        QStringLiteral("The locator primary pose model signature is missing or inconsistent with its template result."));
            result.elapsedMs = located.elapsedMs;
            result.payload.insert(QStringLiteral("locatorPayload"), located.payload);
            return result;
        }
        if (frozenPose.value(QStringLiteral("locatorModelSignature"))
                .toString().trimmed() != currentTemplateSignature) {
            ToolResult result = referencePositionCorrectionError(
                        QStringLiteral("reference_locator_changed"),
                        QStringLiteral("The selected reference template model no longer matches its frozen reference pose."));
            result.elapsedMs = located.elapsedMs;
            result.payload.insert(QStringLiteral("locatorPayload"), located.payload);
            return result;
        }
    } else {
        const QString currentModelSignature = located.payload.value(
                    QStringLiteral("modelSignature")).toString().trimmed();
        const QString currentOriginMode = located.payload.value(
                    QStringLiteral("originMode")).toString().trimmed();
        bool originIdentityChanged =
                (versionedIdentity || !frozenOriginMode.isEmpty())
                && frozenOriginMode != currentOriginMode;
        if ((versionedIdentity && frozenOriginMode == QStringLiteral("custom")) ||
                (!versionedIdentity && hasFrozenCustomOrigin)) {
            QPointF currentOrigin;
            const bool validCurrent = normalizedPointFromJson(
                        located.payload.value(
                            QStringLiteral("customOriginNormalized")),
                        &currentOrigin);
            originIdentityChanged = originIdentityChanged
                    || !validFrozenCustomOrigin || !validCurrent
                    || std::abs(frozenCustomOrigin.x() - currentOrigin.x()) > 1e-9
                    || std::abs(frozenCustomOrigin.y() - currentOrigin.y()) > 1e-9;
        }
        if (((versionedIdentity || !frozenModelSignature.isEmpty()) &&
             frozenModelSignature != currentModelSignature) ||
                ((versionedIdentity || !frozenTemplateId.isEmpty()) &&
                 frozenTemplateId != currentTemplateId) ||
                originIdentityChanged) {
            ToolResult result = referencePositionCorrectionError(
                        QStringLiteral("reference_locator_changed"),
                        QStringLiteral("The reference locator no longer matches the model, template, or origin identity frozen with the reference pose."));
            result.elapsedMs = located.elapsedMs;
            result.payload.insert(QStringLiteral("locatorPayload"), located.payload);
            return result;
        }
        currentPoseJson = located.payload;
    }

    PositionPose referencePose;
    bool invalidReferenceScale = false;
    if (!poseFromJson(frozenPose, &referencePose,
                      &invalidReferenceScale)) {
        return referencePositionCorrectionError(
                    invalidReferenceScale
                    ? QStringLiteral("invalid_pose_scale")
                    : QStringLiteral("invalid_reference_pose"),
                    invalidReferenceScale
                    ? QStringLiteral("Reference pose scale must be finite and greater than zero.")
                    : QStringLiteral("Reference pose x/y/angle must be finite numbers."));
    }
    PositionPose runPose;
    bool invalidRunScale = false;
    if (!poseFromJson(currentPoseJson, &runPose, &invalidRunScale)) {
        ToolResult result = referencePositionCorrectionError(
                    invalidRunScale
                    ? QStringLiteral("invalid_pose_scale")
                    : QStringLiteral("invalid_run_pose"),
                    invalidRunScale
                    ? QStringLiteral("Run pose scale must be finite and greater than zero.")
                    : QStringLiteral("Run pose x/y/angle must be finite numbers."));
        result.elapsedMs = located.elapsedMs;
        result.payload.insert(QStringLiteral("locatorPayload"), located.payload);
        return result;
    }

    PositionCorrectionHalconRunner correctionRunner;
    const PositionCorrectionHalconResult corrected =
            correctionRunner.run(referencePose, runPose);
    ToolResult result;
    result.toolId = PositionCorrection::defaultSourceId();
    result.toolType = ToolType::PositionCorrection;
    result.success = corrected.success;
    result.ok = corrected.ok;
    result.status = corrected.status;
    result.message = corrected.message;
    result.score = located.score;
    result.count = located.count;
    result.elapsedMs = located.elapsedMs + corrected.elapsedMs;
    for (ToolOverlay overlay : located.overlays) {
        if (overlay.label == QStringLiteral("match_result")) {
            overlay.extra.insert(
                        QStringLiteral("role"),
                        QStringLiteral("position_correction_match_contour"));
        } else if (overlay.label == QStringLiteral("match_center")
                   || overlay.extra.value(QStringLiteral("role")).toString()
                   == QStringLiteral("match_origin")) {
            overlay.extra.insert(
                        QStringLiteral("role"),
                        QStringLiteral("position_correction_match_origin"));
        } else {
            continue;
        }
        overlay.extra.insert(QStringLiteral("positionCorrectionSourceId"),
                             PositionCorrection::defaultSourceId());
        result.overlays.append(overlay);
    }
    result.payload = corrected.payload;
    result.payload.insert(QStringLiteral("sourceKind"), QStringLiteral("reference"));
    result.payload.insert(QStringLiteral("scope"), QStringLiteral("global"));
    result.payload.insert(QStringLiteral("sourceId"), PositionCorrection::defaultSourceId());
    result.payload.insert(QStringLiteral("locatorStatus"), located.status);
    result.payload.insert(QStringLiteral("locatorScore"), located.score);
    result.payload.insert(QStringLiteral("locatorPayload"), located.payload);
    result.payload.insert(QStringLiteral("selectedTemplateId"),
                          currentTemplateId);
    result.payload.insert(QStringLiteral("selectedBaseId"),
                          located.payload.value(QStringLiteral("selectedBaseId")));
    result.payload.insert(QStringLiteral("referencePoseTemplateId"),
                          resolved.bankEnvelope
                          ? frozenPose.value(QStringLiteral("locatorTemplateId"))
                          : QJsonValue(currentTemplateId));
    result.payload.insert(QStringLiteral("referencePoseBaseId"),
                          resolved.bankEnvelope
                          ? frozenPose.value(QStringLiteral("locatorBaseId"))
                          : QJsonValue());
    result.payload.insert(QStringLiteral("referencePoseBankVersion"),
                          referenceConfig.version);
    result.payload.insert(QStringLiteral("x"), runPose.x);
    result.payload.insert(QStringLiteral("y"), runPose.y);
    result.payload.insert(QStringLiteral("angle"), runPose.angleDeg);
    result.payload.insert(QStringLiteral("angleDeg"), runPose.angleDeg);
    result.payload.insert(QStringLiteral("scale"), runPose.scale);
    result.payload.insert(QStringLiteral("frameId"),
                          runtimeContext.value(QStringLiteral("frameId")).toString());
    return result;
}

} // namespace

void ToolEngine::registerAdapter(ToolAdapter *adapter)
{
    if (!adapter || m_adapters.contains(adapter))
        return;

    m_adapters.append(adapter);
}

ToolResult ToolEngine::runTool(const ToolRequest &request) const
{
    const ToolConfig &config = request.config;
    if (!config.enabled) {
        return ToolResult::error(config.toolId,
                                 config.toolType,
                                 QStringLiteral("Tool is disabled."),
                                 QStringLiteral("disabled"));
    }

    if (!config.isValid()) {
        return ToolResult::error(config.toolId,
                                 config.toolType,
                                 QStringLiteral("Invalid ToolConfig."),
                                 QStringLiteral("invalid_config"));
    }

    ToolAdapter *adapter = findAdapter(config.toolType);
    if (!adapter)
        return ToolResult::unsupported(config.toolId, config.toolType);

    QElapsedTimer timer;
    timer.start();
    ToolResult result = adapter->run(request);
    if (result.toolId.isEmpty())
        result.toolId = config.toolId;
    if (result.toolType == ToolType::Unknown)
        result.toolType = config.toolType;
    if (result.elapsedMs <= 0)
        result.elapsedMs = timer.elapsed();
    if (!request.frameId.trimmed().isEmpty())
        result.payload.insert(QStringLiteral("frameId"), request.frameId.trimmed());
    return result;
}

QVector<ToolResult> ToolEngine::runTools(const QVector<ToolConfig> &configs,
                                         const cv::Mat &image,
                                         const cv::Mat &referenceImage,
                                         const QJsonObject &runtimeContext,
                                         ToolResult *referenceCorrectionResult,
                                         const QMap<QString, cv::Mat> &referenceImages,
                                         const QMap<QString, QString> &referenceRevisions,
                                         const QString &primaryReferenceBaseId) const
{
    // 工具顺序即依赖顺序；每次调用重建动态结果映射，禁止跨帧复用旧坐标。
    QVector<ToolResult> results;
    results.reserve(configs.size());
    QJsonObject frameContext = runtimeContext;
    // Dynamic results belong to this invocation only. Never accept caller-provided
    // result maps, otherwise a failed frame could reuse a previous frame's matrix.
    frameContext.insert(QStringLiteral("toolResultsById"), QJsonObject());
    frameContext.insert(QStringLiteral("positionCorrectionsById"), QJsonObject());
    const QString frameId = frameContext.value(QStringLiteral("frameId"))
            .toString().trimmed();

    ToolResult referenceCorrection =
            runReferencePositionCorrection(image, referenceImage,
                                           referenceImages,
                                           referenceRevisions,
                                           primaryReferenceBaseId,
                                           frameContext);
    if (!frameId.isEmpty())
        referenceCorrection.payload.insert(QStringLiteral("frameId"), frameId);
    if (referenceCorrectionResult)
        *referenceCorrectionResult = referenceCorrection;
    if (!referenceCorrection.toolId.trimmed().isEmpty())
        registerResultInContext(referenceCorrection, &frameContext);

    for (const ToolConfig &config : configs) {
        if (!config.enabled)
            continue;

        ToolRequest request;
        request.config = config;
        request.image = image;
        request.referenceImage = referenceImage;
        request.referenceImages = referenceImages;
        request.referenceImageRevisions = referenceRevisions;
        request.primaryReferenceBaseId = primaryReferenceBaseId;
        request.frameId = frameId;
        request.runtimeContext = frameContext;
        ToolResult result = runTool(request);
        registerResultInContext(result, &frameContext);
        results.append(result);
    }

    return results;
}

ToolAdapter *ToolEngine::findAdapter(ToolType type) const
{
    for (ToolAdapter *adapter : m_adapters) {
        if (adapter && adapter->supports(type))
            return adapter;
    }

    return nullptr;
}
