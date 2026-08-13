#ifndef ALGORITHMS_LOCATION_TEMPLATELOCATIONCONFIG_H
#define ALGORITHMS_LOCATION_TEMPLATELOCATIONCONFIG_H

#include "toolcore/ToolConfig.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <cmath>

// Keep this scalar contract identical to the legacy declaration in
// TemplateLocationHalconRunner.h until that header is switched to include this
// file.  Existing callers can therefore keep using the scalar Runner wrapper
// while the model-bank path is introduced incrementally.
struct TemplateLocationHalconConfig
{
    QString toolId;
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString modelCacheKey;
    QString templateRegionType = QStringLiteral("rectangle");
    QRectF templateRoiNormalized;
    QVector<QPointF> templatePolygonNormalized;
    QString templateMaskRegionType = QStringLiteral("none");
    QRectF templateMaskRoiNormalized;
    QVector<QPointF> templateMaskPolygonNormalized;
    QPointF templateMaskCircleCenterNormalized;
    double templateMaskCircleRadiusNormalized = 0.0;
    QString searchRegionType = QStringLiteral("full");
    QRectF searchRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> searchPolygonNormalized;
    QPointF searchCircleCenterNormalized;
    double searchCircleRadiusNormalized = 0.0;
    int minScore = 50;
    int angleStart = -45;
    int angleExtent = 90;
    int scaleMin = 100;
    int scaleMax = 100;
    QString polarity = QStringLiteral("use_polarity");
    QString contrastMode = QStringLiteral("auto");
    int contrast = 40;
    int minContrast = 10;
    int numLevels = 0;
    QString subPixel = QStringLiteral("least_squares");
    double greediness = 0.5;
    int timeoutMs = 2000;
    int maxMatches = 1;
    int minMatchCount = 1;
    int maxMatchCount = 1;
    double maxOverlap = 0.5;
    QString originMode = QStringLiteral("centroid");
    QPointF customOriginNormalized;
};

/// One independently cached HALCON shape model in a template-location bank.
struct TemplateLocationTemplateConfig
{
    QString templateId;
    QString name;
    bool enabled = true;
    int priority = 0;
    QString templateRegionType = QStringLiteral("rectangle");
    QRectF templateRoiNormalized;
    QVector<QPointF> templatePolygonNormalized;
    QString templateMaskRegionType = QStringLiteral("none");
    QRectF templateMaskRoiNormalized;
    QVector<QPointF> templateMaskPolygonNormalized;
    QPointF templateMaskCircleCenterNormalized;
    double templateMaskCircleRadiusNormalized = 0.0;
    QString modelCacheKey;
    bool modelCreated = false;

    // Unknown item fields are retained so a read/write cycle does not erase a
    // contract introduced by a newer UI. Known fields always win on output.
    QJsonObject extra;
};

struct TemplateLocationFusionConfig
{
    bool enabled = true;
    double positionTolerancePx = 5.0;
    double angleToleranceDeg = 2.0;
    double scaleTolerance = 0.05;
};

/// Shared build/search/output configuration plus 1..N alternative templates.
struct TemplateLocationModelBankConfig
{
    int version = 4;
    // Decode state belongs to the common v4/v5 contract so every consumer
    // makes the same forward-compatibility decision. Unsupported or malformed
    // envelopes are read-only and serialize from extraParams verbatim.
    bool decodeSupported = true;
    bool rawPassthrough = false;
    QString decodeStatus;
    QString decodeMessage;
    QString toolId;
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString templateMode = QStringLiteral("alternatives");
    QVector<TemplateLocationTemplateConfig> templates;

    QString searchRegionType = QStringLiteral("full");
    QRectF searchRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> searchPolygonNormalized;
    QPointF searchCircleCenterNormalized;
    double searchCircleRadiusNormalized = 0.0;
    int minScore = 50;
    int angleStart = -45;
    int angleExtent = 90;
    int scaleMin = 100;
    int scaleMax = 100;
    QString polarity = QStringLiteral("use_polarity");
    QString contrastMode = QStringLiteral("auto");
    int contrast = 40;
    int minContrast = 10;
    int numLevels = 0;
    QString subPixel = QStringLiteral("least_squares");
    double greediness = 0.5;
    int timeoutMs = 2000;
    int maxMatches = 1;
    int minMatchCount = 1;
    int maxMatchCount = 1;
    double maxOverlap = 0.5;
    QString originMode = QStringLiteral("centroid");
    QPointF customOriginNormalized = QPointF(0.5, 0.5);
    QString primaryMatchStrategy = QStringLiteral("best_score");
    QString primaryTemplateId;
    TemplateLocationFusionConfig fusion;

    // Top-level unknown fields are preserved, subject to v4/v5 source-of-truth
    // cleanup performed by toToolParams().
    QJsonObject extraParams;
};

struct TemplateLocationConfigValidationResult
{
    bool valid = true;
    QString code;
    QString message;
    QString templateId;
    int templateIndex = -1;
};

namespace TemplateLocationConfig {

constexpr int LegacyParamsVersion = 4;
constexpr int ModelBankParamsVersion = 5;
constexpr int MaximumTemplateCount = 8;

struct EnvelopeInspection
{
    bool supported = true;
    int version = LegacyParamsVersion;
    QString status;
    QString message;
};

/// Inspect only the v4/v5 envelope shape. This header-only gate is shared by
/// persistence code that must decide whether it is safe to rewrite a locator
/// without pulling the full codec implementation into every small consumer.
/// It deliberately mirrors the first stage of fromToolParams().
inline EnvelopeInspection inspectEnvelope(const QJsonObject &params)
{
    EnvelopeInspection result;
    const bool hasVersion = params.contains(QStringLiteral("version"));
    const bool hasTemplates = params.contains(QStringLiteral("templates"));
    result.version = hasTemplates ? ModelBankParamsVersion
                                  : LegacyParamsVersion;
    if (hasVersion) {
        const QJsonValue value = params.value(QStringLiteral("version"));
        const double number = value.toDouble();
        if (!value.isDouble() || !std::isfinite(number)
                || std::floor(number) != number) {
            result.supported = false;
            result.status = QStringLiteral("invalid_config_version");
            result.message = QStringLiteral(
                        "Template location version must be the exact JSON integer 4 or 5.");
            return result;
        }
        if (number != LegacyParamsVersion
                && number != ModelBankParamsVersion) {
            result.supported = false;
            result.status = QStringLiteral("unsupported_config_version");
            result.message = QStringLiteral(
                        "Template location supports configuration versions 4 and 5.");
            return result;
        }
        result.version = static_cast<int>(number);
    }
    if (result.version == LegacyParamsVersion && hasTemplates) {
        result.supported = false;
        result.status = QStringLiteral("config_schema_conflict");
        result.message = QStringLiteral(
                    "Version 4 is a flat template contract and cannot contain templates[].");
        return result;
    }
    if (result.version == ModelBankParamsVersion) {
        if (!hasTemplates) {
            result.supported = false;
            result.status = QStringLiteral("missing_templates");
            result.message = QStringLiteral(
                        "Version 5 requires a templates array.");
            return result;
        }
        const QJsonValue templatesValue = params.value(
                    QStringLiteral("templates"));
        if (!templatesValue.isArray()) {
            result.supported = false;
            result.status = QStringLiteral("invalid_templates_type");
            result.message = QStringLiteral(
                        "Version 5 templates must be a JSON array.");
            return result;
        }
        const QJsonArray templates = templatesValue.toArray();
        for (int index = 0; index < templates.size(); ++index) {
            if (templates.at(index).isObject())
                continue;
            result.supported = false;
            result.status = QStringLiteral("invalid_template_item_type");
            result.message = QStringLiteral(
                        "Version 5 template item %1 must be a JSON object.")
                    .arg(index);
            return result;
        }
    }
    return result;
}

/// Parse ToolConfig.params. A missing version is inferred from templates[];
/// an explicitly present version must be the exact JSON number 4 or 5. v4
/// flat fields are wrapped as one in-memory item; v5 templates[] is the only
/// template source and flat item fields are ignored.
TemplateLocationModelBankConfig fromToolConfig(const ToolConfig &config);
TemplateLocationModelBankConfig fromToolParams(
        const QJsonObject &params,
        const QString &toolId = QString());

/// Serialize one-item v4 banks as the legacy flat contract. A v5 bank always
/// emits templates[] and removes stale flat item mirrors.
QJsonObject toToolParams(const TemplateLocationModelBankConfig &config);

/// Scalar compatibility conversions used by the existing Runner wrapper and
/// by selected-template build/self-test operations.
TemplateLocationTemplateConfig templateConfigFromScalar(
        const TemplateLocationHalconConfig &config,
        const QString &templateId = QStringLiteral("legacy-template"),
        const QString &name = QStringLiteral("模板1"),
        bool enabled = true,
        int priority = 0);
TemplateLocationModelBankConfig modelBankFromScalar(
        const TemplateLocationHalconConfig &config,
        const QString &templateId = QStringLiteral("legacy-template"),
        const QString &name = QStringLiteral("模板1"));
TemplateLocationHalconConfig scalarConfigForTemplate(
        const TemplateLocationModelBankConfig &bank,
        const TemplateLocationTemplateConfig &item);

/// Fill missing/duplicate v5 IDs and missing per-item cache identities. Existing
/// unique IDs and cache keys are never changed.
void ensureStableTemplateIds(TemplateLocationModelBankConfig *config);
TemplateLocationModelBankConfig withStableTemplateIds(
        const TemplateLocationModelBankConfig &config);

int enabledTemplateCount(const TemplateLocationModelBankConfig &config);
bool allEnabledModelsReady(
        const TemplateLocationModelBankConfig &config,
        QStringList *notReadyTemplateIds = nullptr);

/// Validate the first-release alternatives bank. Model readiness is optional so
/// the same validator can be used before drawing/building and before execution.
TemplateLocationConfigValidationResult validateModelBank(
        const TemplateLocationModelBankConfig &config,
        bool requireReadyModels = false);

const TemplateLocationTemplateConfig *findTemplate(
        const TemplateLocationModelBankConfig &config,
        const QString &templateId);
TemplateLocationTemplateConfig *findTemplate(
        TemplateLocationModelBankConfig *config,
        const QString &templateId);

} // namespace TemplateLocationConfig

#endif // ALGORITHMS_LOCATION_TEMPLATELOCATIONCONFIG_H
