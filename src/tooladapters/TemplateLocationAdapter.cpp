#include "tooladapters/TemplateLocationAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "calibration/CalibrationSourceFingerprint.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>

bool TemplateLocationAdapter::supports(ToolType type) const
{
    return type == ToolType::TemplateLocation;
}

ToolResult TemplateLocationAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::TemplateLocation) {
        return ToolResult::error(config.toolId, config.toolType,
                                 QStringLiteral("TemplateLocationAdapter only supports TemplateLocation."),
                                 QStringLiteral("invalid_tool_type"));
    }
    TemplateLocationModelBankConfig bank =
            TemplateLocationConfig::fromToolConfig(config);

    if (!bank.decodeSupported || bank.rawPassthrough) {
        return ToolResult::error(
                    config.toolId, config.toolType,
                    bank.decodeMessage.trimmed().isEmpty()
                    ? QStringLiteral("Template location configuration cannot be decoded safely.")
                    : bank.decodeMessage,
                    bank.decodeStatus.trimmed().isEmpty()
                    ? QStringLiteral("invalid_config")
                    : bank.decodeStatus);
    }

    // Keep the long-standing v4 error contract: an unbuilt flat template is
    // reported as no_model even when its drawing geometry is not present yet.
    if (bank.version == TemplateLocationConfig::LegacyParamsVersion &&
            !TemplateLocationConfig::allEnabledModelsReady(bank)) {
        return ToolResult::error(config.toolId, config.toolType,
                                 QStringLiteral("Please create the template model first."),
                                 QStringLiteral("no_model"));
    }
    const TemplateLocationConfigValidationResult validation =
            TemplateLocationConfig::validateModelBank(bank, false);
    if (!validation.valid) {
        return ToolResult::error(config.toolId, config.toolType,
                                 validation.message, validation.code);
    }
    QStringList notReadyTemplateIds;
    if (!TemplateLocationConfig::allEnabledModelsReady(
                bank, &notReadyTemplateIds)) {
        return ToolResult::error(config.toolId, config.toolType,
                                 QStringLiteral("Please build enabled template model(s): %1")
                                 .arg(notReadyTemplateIds.join(QStringLiteral(", "))),
                                 QStringLiteral("no_model"));
    }
    bank.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                bank.halconSoPath, &bank.halconSoPathCandidates);

    QMap<QString, cv::Mat> referenceImages = request.referenceImages;
    QMap<QString, QString> referenceRevisions = request.referenceImageRevisions;
    QString primaryBaseId = request.primaryReferenceBaseId.trimmed();
    if (bank.version == TemplateLocationConfig::CompositeBankParamsVersion &&
            !request.referenceImage.empty()) {
        // Only an explicitly identified compatibility image may enter a v6
        // Base map. Guessing the first binding when primaryBaseId is absent
        // would execute immutable sourceBaseId data against unrelated pixels.
        if (!primaryBaseId.isEmpty() &&
                !referenceImages.contains(primaryBaseId)) {
            referenceImages.insert(primaryBaseId, request.referenceImage);
        }
    }
    const TemplateLocationHalconResult matched =
            bank.version == TemplateLocationConfig::CompositeBankParamsVersion
            ? m_runner.run(request.image, referenceImages, referenceRevisions,
                           bank, primaryBaseId)
            : m_runner.run(request.image, request.referenceImage, bank);
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::TemplateLocation;
    result.success = matched.success;
    result.ok = matched.ok;
    result.status = matched.status;
    result.message = matched.message;
    result.score = matched.score;
    result.value = matched.payload.value(QStringLiteral("x")).toDouble(-1.0);
    result.count = matched.count;
    result.elapsedMs = matched.elapsedMs;
    result.text = matched.status;
    result.overlays = matched.overlays;
    result.payload = matched.payload;
    const CalibrationSourceFingerprint::TemplateReferenceDependency
            referenceDependency =
            CalibrationSourceFingerprint::templateReferenceDependency(
                config,
                referenceImages,
                referenceRevisions,
                primaryBaseId,
                request.referenceImage);
    if (referenceDependency.valid) {
        CalibrationSourceFingerprint::applyTemplateReferenceDependency(
                    referenceDependency, &result.payload);
    } else {
        // A successful v6 run should already have proven every required Base
        // frame.  Preserve the algorithm result but make the missing identity
        // explicit so calibration sampling refuses an incomplete fingerprint.
        result.payload.insert(
                    QStringLiteral("coordinateSourceReferenceError"),
                    referenceDependency.errorCode);
    }
    CalibrationSourceFingerprint::enrichTemplatePayload(config, &result.payload);
    return result;
}
