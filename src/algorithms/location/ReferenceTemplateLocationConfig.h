#ifndef ALGORITHMS_LOCATION_REFERENCETEMPLATELOCATIONCONFIG_H
#define ALGORITHMS_LOCATION_REFERENCETEMPLATELOCATIONCONFIG_H

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/location/TemplateLocationConfig.h"
#include "toolcore/PositionCorrection.h"

#include <QJsonObject>

#include <cmath>

/**
 * Result of adapting the reference-position-correction envelope to the shared
 * template-location model-bank contract.
 *
 * The reference page still owns a single-template product shell.  Keeping the
 * rejection status beside the converted bank prevents future v5 envelopes from
 * being silently truncated to templates[0].
 */
struct ReferenceTemplateLocationConfigResult
{
    bool supported = false;
    bool bankEnvelope = false;
    bool valid = true;
    QString status;
    QString message;
    TemplateLocationModelBankConfig bank;
    QJsonObject referencePosesByTemplateId;
};

namespace ReferenceTemplateLocationConfig {

constexpr int LegacyEnvelopeVersion = 3;
constexpr int ModelBankEnvelopeVersion = 4;

inline bool exactSupportedBankVersion(const QJsonValue &value,
                                      int *version = nullptr)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (number != static_cast<double>(TemplateLocationConfig::LegacyParamsVersion)
            && number != static_cast<double>(
                TemplateLocationConfig::ModelBankParamsVersion)) {
        return false;
    }
    if (version)
        *version = static_cast<int>(number);
    return true;
}

inline ReferenceTemplateLocationConfigResult error(const QString &status,
                                                    const QString &message)
{
    ReferenceTemplateLocationConfigResult result;
    result.status = status;
    result.message = message;
    return result;
}

inline void resolveHalconRuntime(TemplateLocationModelBankConfig *bank)
{
    if (!bank)
        return;
    bank->halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                bank->halconSoPath, &bank->halconSoPathCandidates);
}

/**
 * Convert the current v3 flat reference envelope into the same in-memory bank
 * used by the functional TemplateLocation adapter.  Shared matching defaults
 * come exclusively from TemplateLocationConfig::fromToolParams().
 */
inline ReferenceTemplateLocationConfigResult fromLegacyConfig(
        const ReferencePositionCorrectionConfig &reference)
{
    if (reference.extra.contains(QStringLiteral("locator"))) {
        return error(
                    QStringLiteral("unsupported_reference_locator"),
                    QStringLiteral("The current reference-position-correction envelope cannot execute an embedded locator contract."));
    }
    if (reference.version > LegacyEnvelopeVersion) {
        return error(
                    QStringLiteral("unsupported_reference_config_version"),
                    QStringLiteral("The reference-position-correction envelope version is not supported."));
    }

    // Build the executable locator from the v3 fields explicitly.  Unknown
    // extension fields live in reference.extra solely for lossless round trips;
    // allowing them into the locator would make a future field silently alter
    // legacy execution (or even masquerade as a top-level templates[] bank).
    QJsonObject params{
        {QStringLiteral("version"), TemplateLocationConfig::LegacyParamsVersion},
        {QStringLiteral("templateRegionType"), reference.templateRegionType},
        {QStringLiteral("templateRoiNormalized"),
         QJsonObject{{QStringLiteral("x"), reference.templateRoiNormalized.x()},
                     {QStringLiteral("y"), reference.templateRoiNormalized.y()},
                     {QStringLiteral("width"), reference.templateRoiNormalized.width()},
                     {QStringLiteral("height"), reference.templateRoiNormalized.height()}}},
        {QStringLiteral("templatePolygonNormalized"), reference.templatePolygonNormalized},
        {QStringLiteral("templateMaskRegionType"), reference.templateMaskRegionType},
        {QStringLiteral("templateMaskRoiNormalized"),
         QJsonObject{{QStringLiteral("x"), reference.templateMaskRoiNormalized.x()},
                     {QStringLiteral("y"), reference.templateMaskRoiNormalized.y()},
                     {QStringLiteral("width"), reference.templateMaskRoiNormalized.width()},
                     {QStringLiteral("height"), reference.templateMaskRoiNormalized.height()}}},
        {QStringLiteral("templateMaskPolygonNormalized"),
         reference.templateMaskPolygonNormalized},
        {QStringLiteral("templateMaskCircleCenterNormalized"),
         QJsonObject{{QStringLiteral("x"),
                      reference.templateMaskCircleCenterNormalized.x()},
                     {QStringLiteral("y"),
                      reference.templateMaskCircleCenterNormalized.y()}}},
        {QStringLiteral("templateMaskCircleRadiusNormalized"),
         reference.templateMaskCircleRadiusNormalized},
        {QStringLiteral("originMode"), reference.originMode},
        {QStringLiteral("customOriginNormalized"),
         QJsonObject{{QStringLiteral("x"), reference.customOriginNormalized.x()},
                     {QStringLiteral("y"), reference.customOriginNormalized.y()}}},
        {QStringLiteral("modelCreated"), reference.referenceCreated}
    };
    if (!reference.modelCacheKey.trimmed().isEmpty()) {
        params.insert(QStringLiteral("modelCacheKey"),
                      reference.modelCacheKey.trimmed());
    }
    if (reference.modelCacheKey.trimmed().isEmpty()) {
        // Preserve the historical private locator cache identity.  Both the
        // editor self-test and ToolEngine runtime now obtain it from this one
        // factory instead of carrying separate literals.
        params.insert(QStringLiteral("modelCacheKey"),
                      QStringLiteral("reference.positionCorrection.private_template"));
    }
    ReferenceTemplateLocationConfigResult result;
    result.bank = TemplateLocationConfig::fromToolParams(
                params, PositionCorrection::defaultSourceId());
    TemplateLocationConfig::ensureStableTemplateIds(&result.bank);
    resolveHalconRuntime(&result.bank);
    if (!reference.referencePose.isEmpty()) {
        result.referencePosesByTemplateId.insert(
                    result.bank.templates.first().templateId,
                    reference.referencePose);
    }
    result.supported = true;
    return result;
}

/**
 * Resolve a runtime JSON envelope.  The v3 shell has only one unversioned
 * referencePose, so no nested locator bank is executable until that pose is
 * bound to templateId/model signature.  Multi-template banks keep their more
 * specific rejection code and are never reduced to templates[0].
 */
inline ReferenceTemplateLocationConfigResult fromJson(
        const QJsonObject &json,
        const ReferencePositionCorrectionConfig &legacyReference)
{
    const bool hasLocator = json.contains(QStringLiteral("locator"));
    if (!hasLocator)
        return fromLegacyConfig(legacyReference);

    int encodedReferenceVersion = 0;
    if (!exactSupportedBankVersion(json.value(QStringLiteral("version")),
                                   &encodedReferenceVersion)
            || (encodedReferenceVersion != ModelBankEnvelopeVersion
                && encodedReferenceVersion != 5)) {
        return error(
                    QStringLiteral("unsupported_reference_config_version"),
                    QStringLiteral("The embedded locator requires a supported reference-position-correction envelope version."));
    }

    const QJsonValue locatorValue = json.value(QStringLiteral("locator"));
    if (!locatorValue.isObject() || locatorValue.toObject().isEmpty()) {
        return error(
                    QStringLiteral("invalid_reference_locator"),
                    QStringLiteral("The embedded reference locator is empty or malformed."));
    }
    if (json.contains(QStringLiteral("referencePosesByTemplateId"))
            && !json.value(QStringLiteral("referencePosesByTemplateId"))
            .isObject()) {
        return error(
                    QStringLiteral("invalid_reference_pose_bank"),
                    QStringLiteral("The reference pose bank must be a JSON object."));
    }
    const QJsonObject locator = locatorValue.toObject();

    int encodedLocatorVersion = locator.contains(QStringLiteral("templates"))
            ? TemplateLocationConfig::ModelBankParamsVersion
            : TemplateLocationConfig::LegacyParamsVersion;
    if (locator.contains(QStringLiteral("version"))
            && !exactSupportedBankVersion(
                locator.value(QStringLiteral("version")),
                &encodedLocatorVersion)) {
        return error(
                    QStringLiteral("unsupported_reference_locator_version"),
                    QStringLiteral("The embedded reference locator version is not supported."));
    }

    TemplateLocationModelBankConfig bank =
            TemplateLocationConfig::fromToolParams(
                locator, PositionCorrection::defaultSourceId());
    if (!bank.decodeSupported || bank.rawPassthrough) {
        return error(
                    QStringLiteral("invalid_reference_locator"),
                    bank.decodeMessage.trimmed().isEmpty()
                    ? QStringLiteral("The embedded reference locator has an unsupported structure.")
                    : bank.decodeMessage);
    }
    const TemplateLocationConfigValidationResult validation =
            TemplateLocationConfig::validateModelBank(bank, false);
    if (!validation.valid) {
        // A known v4/v5 contract remains editable while its bank is incomplete
        // (for example immediately after adding a template).  Only an unknown
        // representation/version is read-only; readiness and geometry errors
        // are regular validation state shown by the editor/runtime.
        ReferenceTemplateLocationConfigResult result;
        result.supported = true;
        result.bankEnvelope = true;
        result.valid = false;
        result.status = validation.code.trimmed().isEmpty()
                ? QStringLiteral("invalid_reference_locator")
                : validation.code;
        result.message = validation.message.trimmed().isEmpty()
                ? QStringLiteral("The embedded reference locator is invalid.")
                : validation.message;
        result.bank = bank;
        resolveHalconRuntime(&result.bank);
        result.referencePosesByTemplateId =
                legacyReference.referencePosesByTemplateId;
        return result;
    }
    ReferenceTemplateLocationConfigResult result;
    result.supported = true;
    result.bankEnvelope = true;
    result.bank = bank;
    resolveHalconRuntime(&result.bank);
    result.referencePosesByTemplateId = legacyReference.referencePosesByTemplateId;
    return result;
}

inline bool finiteJsonNumber(const QJsonValue &value, double *number)
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

inline bool normalizedPoint(const QJsonValue &value, QPointF *point)
{
    if (!point || !value.isObject())
        return false;
    const QJsonObject object = value.toObject();
    double x = 0.0;
    double y = 0.0;
    if (!finiteJsonNumber(object.value(QStringLiteral("x")), &x)
            || !finiteJsonNumber(object.value(QStringLiteral("y")), &y)
            || x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) {
        return false;
    }
    *point = QPointF(x, y);
    return true;
}

/** Validate a strict v4-envelope pose before any model-bank execution. */
inline bool validateFrozenPose(const QJsonObject &pose,
                               const QString &templateId,
                               const TemplateLocationModelBankConfig &bank,
                               QString *message = nullptr)
{
    const auto fail = [message](const QString &text) {
        if (message)
            *message = text;
        return false;
    };
    if (pose.isEmpty()) {
        return fail(QStringLiteral("Template '%1' has no frozen reference pose.")
                    .arg(templateId));
    }
    const QJsonValue identityVersion = pose.value(
                QStringLiteral("locatorIdentityVersion"));
    if (!identityVersion.isDouble() || identityVersion.toInt(-1) != 1
            || identityVersion.toDouble() != 1.0) {
        return fail(QStringLiteral("Template '%1' has an unsupported or missing pose identity version.")
                    .arg(templateId));
    }
    if (pose.value(QStringLiteral("locatorTemplateId")).toString().trimmed()
            != templateId) {
        return fail(QStringLiteral("Template '%1' is paired with a pose for a different template ID.")
                    .arg(templateId));
    }
    if (pose.value(QStringLiteral("locatorModelSignature"))
            .toString().trimmed().isEmpty()) {
        return fail(QStringLiteral("Template '%1' has no frozen model signature.")
                    .arg(templateId));
    }
    double x = 0.0;
    double y = 0.0;
    double angle = 0.0;
    double scale = 1.0;
    const QJsonValue angleValue = pose.contains(QStringLiteral("angleDeg"))
            ? pose.value(QStringLiteral("angleDeg"))
            : pose.value(QStringLiteral("angle"));
    if (!finiteJsonNumber(pose.value(QStringLiteral("x")), &x)
            || !finiteJsonNumber(pose.value(QStringLiteral("y")), &y)
            || !finiteJsonNumber(angleValue, &angle)
            || (pose.contains(QStringLiteral("scale"))
                && (!finiteJsonNumber(pose.value(QStringLiteral("scale")), &scale)
                    || scale <= 0.0))) {
        return fail(QStringLiteral("Template '%1' has an invalid frozen reference pose.")
                    .arg(templateId));
    }
    const QString originMode = pose.value(
                QStringLiteral("locatorOriginMode")).toString().trimmed();
    if (originMode != bank.originMode
            || (originMode != QStringLiteral("centroid")
                && originMode != QStringLiteral("custom"))) {
        return fail(QStringLiteral("Template '%1' has a frozen origin mode that no longer matches the locator.")
                    .arg(templateId));
    }
    if (originMode == QStringLiteral("custom")) {
        QPointF frozenOrigin;
        if (!normalizedPoint(pose.value(
                             QStringLiteral("locatorCustomOriginNormalized")),
                             &frozenOrigin)
                || std::abs(frozenOrigin.x() - bank.customOriginNormalized.x()) > 1e-9
                || std::abs(frozenOrigin.y() - bank.customOriginNormalized.y()) > 1e-9) {
            return fail(QStringLiteral("Template '%1' has a frozen custom origin that no longer matches the locator.")
                        .arg(templateId));
        }
    }
    if (message)
        message->clear();
    return true;
}

inline bool validateAllEnabledFrozenPoses(
        const TemplateLocationModelBankConfig &bank,
        const QJsonObject &posesByTemplateId,
        QString *templateId = nullptr,
        QString *message = nullptr)
{
    for (const TemplateLocationTemplateConfig &item : bank.templates) {
        if (!item.enabled)
            continue;
        const QString id = item.templateId.trimmed();
        if (!validateFrozenPose(posesByTemplateId.value(id).toObject(),
                                id, bank, message)) {
            if (templateId)
                *templateId = id;
            return false;
        }
    }
    if (templateId)
        templateId->clear();
    if (message)
        message->clear();
    return true;
}

} // namespace ReferenceTemplateLocationConfig

#endif // ALGORITHMS_LOCATION_REFERENCETEMPLATELOCATIONCONFIG_H
