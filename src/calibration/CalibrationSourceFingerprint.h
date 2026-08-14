#ifndef CALIBRATION_CALIBRATIONSOURCEFINGERPRINT_H
#define CALIBRATION_CALIBRATIONSOURCEFINGERPRINT_H

#include "algorithms/location/TemplateLocationConfig.h"
#include "toolcore/ToolConfig.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>

/**
 * Stable identity contract for a producer whose image-space pose is used to
 * create or execute a calibration.  This is deliberately independent from
 * TemplateLocationHalconRunner's model cache signature: the latter identifies
 * the generated HALCON model, while this contract also covers output semantics
 * such as the selected origin and the subscribed fields.
 */
namespace CalibrationSourceFingerprint {

/// 递归按键排序 JSON 对象，生成与插入顺序无关的稳定表示。
inline QJsonValue canonicalValue(const QJsonValue &value)
{
    if (value.isArray()) {
        QJsonArray array;
        const QJsonArray source = value.toArray();
        for (const QJsonValue &entry : source)
            array.append(canonicalValue(entry));
        return array;
    }
    if (value.isObject()) {
        const QJsonObject source = value.toObject();
        QStringList keys = source.keys();
        std::sort(keys.begin(), keys.end());
        QJsonObject object;
        for (const QString &key : keys)
            object.insert(key, canonicalValue(source.value(key)));
        return object;
    }
    return value;
}

/// 将任意 JSON 值编码为紧凑、稳定的字节序列。
inline QByteArray canonicalJson(const QJsonValue &value)
{
    if (value.isObject()) {
        return QJsonDocument(canonicalValue(value).toObject())
                .toJson(QJsonDocument::Compact);
    }
    QJsonArray wrapper;
    wrapper.append(canonicalValue(value));
    const QByteArray encoded = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
    return encoded.mid(1, qMax(0, encoded.size() - 2));
}

/// 计算规范化 JSON 的 SHA-256 十六进制摘要。
inline QString sha256(const QJsonValue &value)
{
    return QString::fromLatin1(QCryptographicHash::hash(
                                  canonicalJson(value),
                                  QCryptographicHash::Sha256).toHex());
}

/// 按尺寸、类型和逐行原始像素计算图像内容签名。
inline QString imageSignature(const cv::Mat &image)
{
    if (image.empty())
        return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArray::number(image.rows));
    hash.addData("x", 1);
    hash.addData(QByteArray::number(image.cols));
    hash.addData(":", 1);
    hash.addData(QByteArray::number(image.type()));
    const int rowBytes = static_cast<int>(image.cols * image.elemSize());
    for (int row = 0; row < image.rows; ++row) {
        hash.addData(reinterpret_cast<const char *>(image.ptr(row)), rowBytes);
    }
    return QString::fromLatin1(hash.result().toHex());
}

/**
 * Reference-pixel identity consumed by a TemplateLocation producer.
 *
 * v4/v5 retain the historical primary-image signature.  v6 signs every Base
 * used by an enabled template, including the stable Base ID, content revision
 * and normalized runtime pixels.  This prevents a calibration bound to B02
 * from being validated against unrelated primary Base pixels.
 */
struct TemplateReferenceDependency
{
    bool valid = false;
    bool composite = false;
    bool legacyCompositePrimarySignature = false;
    QString signature;
    QString contract;
    QStringList baseIds;
    QJsonObject contentRevisions;
    QString errorCode;
    QString errorMessage;
};

inline QString templateReferenceDependencyContract()
{
    return QStringLiteral("template_reference_dependencies_v1");
}

inline TemplateReferenceDependency templateReferenceDependency(
        const ToolConfig &config,
        const QMap<QString, cv::Mat> &referenceImages,
        const QMap<QString, QString> &referenceRevisions,
        const QString &primaryBaseId,
        const cv::Mat &legacyReferenceImage,
        const QJsonObject &expectedFingerprint = QJsonObject())
{
    TemplateReferenceDependency result;
    const TemplateLocationConfig::EnvelopeInspection envelope =
            TemplateLocationConfig::inspectEnvelope(config.params);
    if (!envelope.supported) {
        result.errorCode = envelope.status;
        result.errorMessage = envelope.message;
        return result;
    }

    if (envelope.version !=
            TemplateLocationConfig::CompositeBankParamsVersion) {
        cv::Mat primary = legacyReferenceImage;
        const QString primaryId = primaryBaseId.trimmed();
        if (primary.empty() && !primaryId.isEmpty())
            primary = referenceImages.value(primaryId);
        if (primary.empty()) {
            result.errorCode = QStringLiteral("reference_image_missing");
            result.errorMessage = QStringLiteral(
                        "The primary reference image required by the legacy locator is unavailable.");
            return result;
        }
        result.signature = imageSignature(primary);
        if (result.signature.isEmpty()) {
            result.errorCode = QStringLiteral("reference_signature_failed");
            result.errorMessage = QStringLiteral(
                        "The primary reference image could not be fingerprinted.");
            return result;
        }
        result.valid = true;
        if (!primaryId.isEmpty()) {
            result.baseIds.append(primaryId);
            result.contentRevisions.insert(
                        primaryId, referenceRevisions.value(primaryId));
        }
        return result;
    }

    result.composite = true;
    const QJsonArray templates = config.params.value(
                QStringLiteral("templates")).toArray();
    const bool legacyCompositePrimaryContract =
            expectedFingerprint.contains(QStringLiteral("referenceImageSignature"))
            && expectedFingerprint.value(
                QStringLiteral("referenceDependencyContract"))
               .toString().trimmed().isEmpty();

    if (legacyCompositePrimaryContract) {
        // Historical v6 fingerprints were produced before the dependency-set
        // contract and signed request.referenceImage (the primary Base mirror).
        // Preserve that comparison only as a compatibility signal.  Callers
        // must downgrade it to unverifiable because non-primary dependencies
        // were not covered by the saved identity.
        cv::Mat historicalPrimary = legacyReferenceImage;
        const QString primaryId = primaryBaseId.trimmed();
        if (historicalPrimary.empty() && !primaryId.isEmpty())
            historicalPrimary = referenceImages.value(primaryId);
        if (historicalPrimary.empty()) {
            result.errorCode = QStringLiteral("reference_image_missing");
            result.errorMessage = QStringLiteral(
                        "The historical v6 primary Base image is unavailable.");
            return result;
        }
        result.signature = imageSignature(historicalPrimary);
        if (result.signature.isEmpty()) {
            result.errorCode = QStringLiteral("reference_signature_failed");
            result.errorMessage = QStringLiteral(
                        "The historical v6 primary Base could not be fingerprinted.");
            return result;
        }
        result.valid = true;
        result.legacyCompositePrimarySignature = true;
        if (!primaryId.isEmpty()) {
            result.baseIds.append(primaryId);
            result.contentRevisions.insert(
                        primaryId, referenceRevisions.value(primaryId));
        }
        return result;
    }

    QStringList dependencyIds;
    QSet<QString> enabledBaseIds;
    for (const QJsonValue &value : templates) {
        const QJsonObject item = value.toObject();
        if (!item.value(QStringLiteral("enabled")).toBool(true))
            continue;
        const QString baseId = item.value(
                    QStringLiteral("sourceBaseId")).toString().trimmed();
        if (baseId.isEmpty()) {
            result.errorCode = QStringLiteral("reference_base_identity_missing");
            result.errorMessage = QStringLiteral(
                        "An enabled v6 template has no stable Base binding.");
            return result;
        }
        enabledBaseIds.insert(baseId);
    }
    if (enabledBaseIds.isEmpty()) {
        result.errorCode = QStringLiteral("reference_base_identity_missing");
        result.errorMessage = QStringLiteral(
                    "The v6 locator has no enabled Base dependency.");
        return result;
    }

    const QJsonArray bindings = config.params.value(
                QStringLiteral("baseBindings")).toArray();
    for (const QJsonValue &value : bindings) {
        const QString baseId = value.toObject().value(
                    QStringLiteral("baseId")).toString().trimmed();
        if (!enabledBaseIds.remove(baseId))
            continue;
        dependencyIds.append(baseId);
    }
    if (!enabledBaseIds.isEmpty()) {
        result.errorCode = QStringLiteral("reference_base_binding_missing");
        result.errorMessage = QStringLiteral(
                    "The v6 locator refers to Base '%1' without a matching Base binding.")
                .arg(*enabledBaseIds.constBegin());
        return result;
    }

    QJsonArray signedBases;
    for (const QString &baseId : dependencyIds) {
        const cv::Mat frame = referenceImages.value(baseId);
        if (frame.empty()) {
            result.errorCode = QStringLiteral("reference_image_for_base_missing");
            result.errorMessage = QStringLiteral(
                        "The v6 locator requires unavailable Base '%1'.")
                    .arg(baseId);
            return result;
        }
        const QString pixels = imageSignature(frame);
        if (pixels.isEmpty()) {
            result.errorCode = QStringLiteral("reference_signature_failed");
            result.errorMessage = QStringLiteral(
                        "Base '%1' could not be fingerprinted.").arg(baseId);
            return result;
        }
        const QString revision = referenceRevisions.value(baseId).trimmed();
        if (revision.isEmpty()) {
            result.errorCode = QStringLiteral(
                        "reference_revision_for_base_missing");
            result.errorMessage = QStringLiteral(
                        "The v6 locator requires a non-empty content revision for Base '%1'.")
                    .arg(baseId);
            return result;
        }
        result.baseIds.append(baseId);
        result.contentRevisions.insert(baseId, revision);
        signedBases.append(QJsonObject{
            {QStringLiteral("baseId"), baseId},
            {QStringLiteral("contentRevision"), revision},
            {QStringLiteral("imageSignature"), pixels}
        });
    }

    result.contract = templateReferenceDependencyContract();
    result.signature = sha256(QJsonObject{
        {QStringLiteral("contract"), result.contract},
        {QStringLiteral("bases"), signedBases}
    });
    result.valid = !result.signature.isEmpty();
    return result;
}

inline void applyTemplateReferenceDependency(
        const TemplateReferenceDependency &dependency,
        QJsonObject *payload)
{
    if (!payload || !dependency.valid)
        return;
    payload->insert(QStringLiteral("coordinateSourceReferenceSignature"),
                    dependency.signature);
    if (dependency.contract.isEmpty()) {
        payload->remove(QStringLiteral("coordinateSourceReferenceContract"));
        payload->remove(QStringLiteral("coordinateSourceReferenceBaseIds"));
        payload->remove(QStringLiteral("coordinateSourceReferenceRevisions"));
        return;
    }
    QJsonArray baseIds;
    for (const QString &baseId : dependency.baseIds)
        baseIds.append(baseId);
    payload->insert(QStringLiteral("coordinateSourceReferenceContract"),
                    dependency.contract);
    payload->insert(QStringLiteral("coordinateSourceReferenceBaseIds"),
                    baseIds);
    payload->insert(QStringLiteral("coordinateSourceReferenceRevisions"),
                    dependency.contentRevisions);
}

inline QJsonObject normalizedPoint(const QJsonObject &point,
                                   double fallbackX = 0.5,
                                   double fallbackY = 0.5)
{
    return QJsonObject{
        {QStringLiteral("x"), point.value(QStringLiteral("x")).toDouble(fallbackX)},
        {QStringLiteral("y"), point.value(QStringLiteral("y")).toDouble(fallbackY)}
    };
}

/**
 * Public output-origin identity for the Base selected by TemplateLocation.
 *
 * In v6, baseBindings is the sole authority for output origin semantics.  A
 * selected Base identity is therefore mandatory and must resolve exactly one
 * binding.  v4/v5 retain their historical top-level origin fields.
 */
struct TemplateOriginIdentity
{
    bool valid = false;
    bool composite = false;
    QString selectedBaseId;
    QString originMode;
    QJsonObject customOriginNormalized;
    QString errorCode;
    QString errorMessage;
};

inline TemplateOriginIdentity templateOriginIdentity(
        const ToolConfig &config,
        const QString &selectedBaseId)
{
    TemplateOriginIdentity result;
    const TemplateLocationConfig::EnvelopeInspection envelope =
            TemplateLocationConfig::inspectEnvelope(config.params);
    if (!envelope.supported) {
        result.errorCode = envelope.status;
        result.errorMessage = envelope.message;
        return result;
    }

    QJsonObject originOwner = config.params;
    if (envelope.version ==
            TemplateLocationConfig::CompositeBankParamsVersion) {
        result.composite = true;
        result.selectedBaseId = selectedBaseId.trimmed();
        if (result.selectedBaseId.isEmpty()) {
            result.errorCode = QStringLiteral("selected_base_identity_missing");
            result.errorMessage = QStringLiteral(
                        "The v6 coordinate-source fingerprint has no selected Base identity.");
            return result;
        }

        int matchingBindings = 0;
        const QJsonArray bindings = config.params.value(
                    QStringLiteral("baseBindings")).toArray();
        for (const QJsonValue &value : bindings) {
            const QJsonObject binding = value.toObject();
            if (binding.value(QStringLiteral("baseId")).toString().trimmed()
                    != result.selectedBaseId) {
                continue;
            }
            originOwner = binding;
            ++matchingBindings;
        }
        if (matchingBindings == 0) {
            result.errorCode = QStringLiteral("selected_base_binding_missing");
            result.errorMessage = QStringLiteral(
                        "The selected Base '%1' has no v6 Base binding.")
                    .arg(result.selectedBaseId);
            return result;
        }
        if (matchingBindings != 1) {
            result.errorCode = QStringLiteral("selected_base_binding_ambiguous");
            result.errorMessage = QStringLiteral(
                        "The selected Base '%1' has duplicate v6 Base bindings.")
                    .arg(result.selectedBaseId);
            return result;
        }
    }

    result.originMode = originOwner.value(QStringLiteral("originMode"))
            .toString().trimmed().toLower();
    if (result.originMode.isEmpty())
        result.originMode = QStringLiteral("centroid");
    if (result.originMode != QStringLiteral("centroid")
            && result.originMode != QStringLiteral("custom")) {
        result.errorCode = QStringLiteral("origin_mode_invalid");
        result.errorMessage = QStringLiteral(
                    "Template location origin mode must be centroid or custom.");
        return result;
    }
    result.customOriginNormalized = normalizedPoint(
                originOwner.value(
                    QStringLiteral("customOriginNormalized")).toObject());
    if (result.originMode == QStringLiteral("custom")) {
        const double x = result.customOriginNormalized.value(
                    QStringLiteral("x")).toDouble();
        const double y = result.customOriginNormalized.value(
                    QStringLiteral("y")).toDouble();
        if (!std::isfinite(x) || !std::isfinite(y)
                || x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) {
            result.errorCode = QStringLiteral("custom_origin_invalid");
            result.errorMessage = QStringLiteral(
                        "Template location custom origin must be normalized to [0, 1].");
            return result;
        }
    }
    result.valid = true;
    return result;
}

inline void applyTemplateOriginIdentity(
        const TemplateOriginIdentity &origin,
        QJsonObject *payload)
{
    if (!payload || !origin.valid)
        return;
    payload->insert(QStringLiteral("originMode"), origin.originMode);
    payload->insert(QStringLiteral("customOriginNormalized"),
                    origin.customOriginNormalized);
}

inline bool matchesTemplateOriginIdentity(
        const QJsonObject &expectedFingerprint,
        const TemplateOriginIdentity &actualOrigin,
        QString *mismatchField = nullptr)
{
    if (!actualOrigin.valid) {
        if (mismatchField)
            *mismatchField = actualOrigin.errorCode;
        return false;
    }
    if (expectedFingerprint.value(QStringLiteral("originMode")).toString()
            != actualOrigin.originMode) {
        if (mismatchField)
            *mismatchField = QStringLiteral("originMode");
        return false;
    }
    const QJsonObject actualPoint = actualOrigin.originMode
            == QStringLiteral("custom")
            ? actualOrigin.customOriginNormalized
            : QJsonObject{{QStringLiteral("x"), 0.5},
                          {QStringLiteral("y"), 0.5}};
    if (canonicalJson(expectedFingerprint.value(
                          QStringLiteral("customOriginNormalized")))
            != canonicalJson(actualPoint)) {
        if (mismatchField)
            *mismatchField = QStringLiteral("customOriginNormalized");
        return false;
    }
    if (mismatchField)
        mismatchField->clear();
    return true;
}

/// 递归移除模板库中的运行环境、持久缓存和建模生命周期字段。
inline QJsonValue stripTemplateLifecycleFields(const QJsonValue &value)
{
    if (value.isArray()) {
        QJsonArray output;
        for (const QJsonValue &entry : value.toArray())
            output.append(stripTemplateLifecycleFields(entry));
        return output;
    }
    if (!value.isObject())
        return value;

    const QJsonObject source = value.toObject();
    const bool templateItem = source.contains(QStringLiteral("templateId")) ||
            (source.contains(QStringLiteral("templateRegionType")) &&
             source.contains(QStringLiteral("modelCacheKey")));
    QJsonObject output;
    for (auto it = source.constBegin(); it != source.constEnd(); ++it) {
        if (it.key() == QStringLiteral("halconSoPath") ||
                it.key() == QStringLiteral("timeoutMs") ||
                it.key() == QStringLiteral("modelCacheKey") ||
                it.key() == QStringLiteral("modelCreated") ||
                (templateItem &&
                 (it.key() == QStringLiteral("name") ||
                  it.key() == QStringLiteral("templateName")))) {
            continue;
        }
        output.insert(it.key(), stripTemplateLifecycleFields(it.value()));
    }
    return output;
}

/// 提取会影响模板图像坐标语义的稳定配置，排除运行环境和缓存生命周期字段。
inline QJsonObject templateConfigContract(const ToolConfig &config)
{
    QJsonObject params = stripTemplateLifecycleFields(config.params).toObject();
    // Origin semantics are represented explicitly in the fingerprint.  Keep
    // them out of the config hash so an inactive custom point in centroid mode
    // does not invalidate an otherwise identical calibration.
    params.remove(QStringLiteral("originMode"));
    params.remove(QStringLiteral("customOriginNormalized"));

    return QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("toolType"), toolTypeToString(config.toolType)},
        {QStringLiteral("roiNormalized"), QJsonObject{
             {QStringLiteral("x"), config.roiNormalized.x()},
             {QStringLiteral("y"), config.roiNormalized.y()},
             {QStringLiteral("width"), config.roiNormalized.width()},
             {QStringLiteral("height"), config.roiNormalized.height()}}},
        {QStringLiteral("params"), params}
    };
}

/// 计算模板坐标来源配置合同的稳定签名。
inline QString coordinateSourceConfigSignature(const ToolConfig &config)
{
    return sha256(templateConfigContract(config));
}

/// 返回模板定位对标定公开的 X/Y/Angle 字段及坐标单位合同。
inline QJsonObject templateOutputContract()
{
    return QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("x"), QStringLiteral("x")},
        {QStringLiteral("y"), QStringLiteral("y")},
        {QStringLiteral("angle"), QStringLiteral("angle")},
        {QStringLiteral("coordinateSystem"), QStringLiteral("image_pixel")},
        {QStringLiteral("angleUnit"), QStringLiteral("degree")}
    };
}

/// 计算去除自签名字段后的完整来源指纹摘要。
inline QString coordinateSourceSignature(const QJsonObject &fingerprint)
{
    QJsonObject unsignedFingerprint = fingerprint;
    unsignedFingerprint.remove(QStringLiteral("coordinateSourceSignature"));
    return sha256(unsignedFingerprint);
}

/// 从生产者身份、输出 payload 与字段合同构造带自校验签名的来源指纹。
inline QJsonObject makeFingerprint(const QString &producerId,
                                   ToolType producerType,
                                   const QJsonObject &payload,
                                   const QJsonObject &outputContract =
                                        templateOutputContract())
{
    const QString originMode = payload.value(QStringLiteral("originMode"))
            .toString(QStringLiteral("centroid"));
    const QJsonObject customOrigin = originMode == QStringLiteral("custom")
            ? normalizedPoint(payload.value(
                                  QStringLiteral("customOriginNormalized")).toObject())
            : QJsonObject{{QStringLiteral("x"), 0.5},
                          {QStringLiteral("y"), 0.5}};
    QJsonObject fingerprint{
        {QStringLiteral("version"), 1},
        {QStringLiteral("producerId"), producerId.trimmed()},
        {QStringLiteral("producerType"), toolTypeToString(producerType)},
        {QStringLiteral("outputContract"), outputContract},
        {QStringLiteral("originMode"), originMode},
        {QStringLiteral("customOriginNormalized"), customOrigin},
        {QStringLiteral("modelSignature"),
         payload.value(QStringLiteral("modelSignature")).toString().trimmed()},
        {QStringLiteral("coordinateSourceConfigSignature"),
         payload.value(QStringLiteral("coordinateSourceConfigSignature"))
             .toString().trimmed()}
    };
    const QString referenceSignature = payload.value(
                QStringLiteral("coordinateSourceReferenceSignature"))
            .toString().trimmed();
    if (!referenceSignature.isEmpty()) {
        fingerprint.insert(QStringLiteral("referenceImageSignature"),
                           referenceSignature);
    }
    const QString referenceContract = payload.value(
                QStringLiteral("coordinateSourceReferenceContract"))
            .toString().trimmed();
    if (!referenceContract.isEmpty()) {
        fingerprint.insert(QStringLiteral("referenceDependencyContract"),
                           referenceContract);
        fingerprint.insert(QStringLiteral("referenceBaseIds"),
                           payload.value(QStringLiteral(
                               "coordinateSourceReferenceBaseIds")));
        fingerprint.insert(QStringLiteral("referenceContentRevisions"),
                           payload.value(QStringLiteral(
                               "coordinateSourceReferenceRevisions")));
    }
    const QStringList optionalIdentityFields{
        QStringLiteral("modelSetSignature"),
        QStringLiteral("selectedTemplateId"),
        QStringLiteral("selectedBaseId"),
        QStringLiteral("primaryMatchStrategy"),
        QStringLiteral("lockedTemplateId")
    };
    for (const QString &field : optionalIdentityFields) {
        if (payload.contains(field) && !payload.value(field).toString().trimmed().isEmpty())
            fingerprint.insert(field, payload.value(field));
    }
    fingerprint.insert(QStringLiteral("coordinateSourceSignature"),
                       coordinateSourceSignature(fingerprint));
    return fingerprint;
}

/// 校验必需字段、版本和自签名，返回首个缺失/损坏字段。
inline bool isComplete(const QJsonObject &fingerprint,
                       QString *missingField = nullptr)
{
    const QStringList stringFields{
        QStringLiteral("producerId"),
        QStringLiteral("producerType"),
        QStringLiteral("originMode"),
        QStringLiteral("modelSignature"),
        QStringLiteral("coordinateSourceConfigSignature"),
        QStringLiteral("coordinateSourceSignature")
    };
    if (fingerprint.value(QStringLiteral("version")).toInt() != 1) {
        if (missingField)
            *missingField = QStringLiteral("version");
        return false;
    }
    for (const QString &field : stringFields) {
        if (fingerprint.value(field).toString().trimmed().isEmpty()) {
            if (missingField)
                *missingField = field;
            return false;
        }
    }
    for (const QString &field : {QStringLiteral("outputContract"),
                                 QStringLiteral("customOriginNormalized")}) {
        if (fingerprint.value(field).toObject().isEmpty()) {
            if (missingField)
                *missingField = field;
            return false;
        }
    }
    const QString contractField = QStringLiteral(
                "referenceDependencyContract");
    const QString baseIdsField = QStringLiteral("referenceBaseIds");
    const QString revisionsField = QStringLiteral(
                "referenceContentRevisions");
    const bool hasDependencyMetadata = fingerprint.contains(contractField)
            || fingerprint.contains(baseIdsField)
            || fingerprint.contains(revisionsField);
    if (hasDependencyMetadata) {
        const QJsonValue contractValue = fingerprint.value(contractField);
        if (!contractValue.isString()
                || contractValue.toString()
                   != templateReferenceDependencyContract()) {
            if (missingField)
                *missingField = contractField;
            return false;
        }
        if (!fingerprint.value(QStringLiteral("referenceImageSignature"))
                .isString()
                || fingerprint.value(QStringLiteral(
                    "referenceImageSignature"))
                   .toString().trimmed().isEmpty()) {
            if (missingField)
                *missingField = QStringLiteral("referenceImageSignature");
            return false;
        }

        const QJsonValue baseIdsValue = fingerprint.value(baseIdsField);
        if (!baseIdsValue.isArray() || baseIdsValue.toArray().isEmpty()) {
            if (missingField)
                *missingField = baseIdsField;
            return false;
        }
        QSet<QString> dependencyBaseIds;
        for (const QJsonValue &value : baseIdsValue.toArray()) {
            if (!value.isString()) {
                if (missingField)
                    *missingField = baseIdsField;
                return false;
            }
            const QString baseId = value.toString();
            if (baseId.isEmpty() || baseId != baseId.trimmed()
                    || dependencyBaseIds.contains(baseId)) {
                if (missingField)
                    *missingField = baseIdsField;
                return false;
            }
            dependencyBaseIds.insert(baseId);
        }

        const QJsonValue revisionsValue = fingerprint.value(revisionsField);
        if (!revisionsValue.isObject()) {
            if (missingField)
                *missingField = revisionsField;
            return false;
        }
        const QJsonObject revisions = revisionsValue.toObject();
        if (revisions.size() != dependencyBaseIds.size()) {
            if (missingField)
                *missingField = revisionsField;
            return false;
        }
        for (const QString &baseId : dependencyBaseIds) {
            const QJsonValue revision = revisions.value(baseId);
            if (!revision.isString()
                    || revision.toString().trimmed().isEmpty()) {
                if (missingField)
                    *missingField = revisionsField;
                return false;
            }
        }

        const QJsonValue selectedBaseValue = fingerprint.value(
                    QStringLiteral("selectedBaseId"));
        const QString selectedBaseId = selectedBaseValue.toString();
        if (!selectedBaseValue.isString()
                || selectedBaseId.isEmpty()
                || selectedBaseId != selectedBaseId.trimmed()
                || !dependencyBaseIds.contains(selectedBaseId)) {
            if (missingField)
                *missingField = QStringLiteral("selectedBaseId");
            return false;
        }
    } else if (fingerprint.contains(QStringLiteral("selectedBaseId"))) {
        // Historical v6 fingerprints predate the dependency-set contract but
        // retain the selected Base together with the scalar primary-image
        // signature.  A selected Base left behind without that signature is a
        // stripped composite fingerprint, not a valid v4/v5 identity.
        const QJsonValue selectedBaseValue = fingerprint.value(
                    QStringLiteral("selectedBaseId"));
        const QString selectedBaseId = selectedBaseValue.toString();
        if (!selectedBaseValue.isString()
                || selectedBaseId.isEmpty()
                || selectedBaseId != selectedBaseId.trimmed()) {
            if (missingField)
                *missingField = QStringLiteral("selectedBaseId");
            return false;
        }
        const QJsonValue referenceSignature = fingerprint.value(
                    QStringLiteral("referenceImageSignature"));
        if (!referenceSignature.isString()
                || referenceSignature.toString().trimmed().isEmpty()) {
            if (missingField)
                *missingField = QStringLiteral("referenceImageSignature");
            return false;
        }
    }
    if (fingerprint.value(QStringLiteral("coordinateSourceSignature")).toString()
            != coordinateSourceSignature(fingerprint)) {
        if (missingField)
            *missingField = QStringLiteral("coordinateSourceSignature");
        return false;
    }
    if (missingField)
        missingField->clear();
    return true;
}

/// 比较标定时与运行时来源语义；仅当期望记录了基准图签名时强制比较该字段。
inline bool matches(const QJsonObject &expected,
                    const QJsonObject &actual,
                    QString *mismatchField = nullptr)
{
    const QStringList fields{
        QStringLiteral("version"),
        QStringLiteral("producerId"),
        QStringLiteral("producerType"),
        QStringLiteral("outputContract"),
        QStringLiteral("originMode"),
        QStringLiteral("customOriginNormalized"),
        QStringLiteral("modelSignature"),
        QStringLiteral("coordinateSourceConfigSignature")
    };
    for (const QJsonObject &fingerprint : {expected, actual}) {
        QString invalidField;
        if (!isComplete(fingerprint, &invalidField)) {
            if (mismatchField)
                *mismatchField = invalidField;
            return false;
        }
    }
    for (const QString &field : fields) {
        if (canonicalJson(expected.value(field)) != canonicalJson(actual.value(field))) {
            if (mismatchField)
                *mismatchField = field;
            return false;
        }
    }
    if (expected.contains(QStringLiteral("referenceImageSignature"))
            && canonicalJson(expected.value(QStringLiteral("referenceImageSignature")))
               != canonicalJson(actual.value(QStringLiteral("referenceImageSignature")))) {
        if (mismatchField)
            *mismatchField = QStringLiteral("referenceImageSignature");
        return false;
    }
    const QStringList dependencyMetadataFields{
        QStringLiteral("referenceDependencyContract"),
        QStringLiteral("referenceBaseIds"),
        QStringLiteral("referenceContentRevisions")
    };
    for (const QString &field : dependencyMetadataFields) {
        if (expected.contains(field) != actual.contains(field)
                || (expected.contains(field)
                    && canonicalJson(expected.value(field))
                       != canonicalJson(actual.value(field)))) {
            if (mismatchField)
                *mismatchField = field;
            return false;
        }
    }
    const QStringList optionalIdentityFields{
        QStringLiteral("modelSetSignature"),
        QStringLiteral("selectedTemplateId"),
        QStringLiteral("selectedBaseId"),
        QStringLiteral("primaryMatchStrategy"),
        QStringLiteral("lockedTemplateId")
    };
    for (const QString &field : optionalIdentityFields) {
        if (expected.contains(field) &&
                canonicalJson(expected.value(field)) != canonicalJson(actual.value(field))) {
            if (mismatchField)
                *mismatchField = field;
            return false;
        }
    }
    if (mismatchField)
        mismatchField->clear();
    return true;
}

/// 给模板定位结果补充来源合同与配置签名，供后序链路传播。
inline void enrichTemplatePayload(const ToolConfig &config, QJsonObject *payload)
{
    if (!payload)
        return;
    payload->insert(QStringLiteral("coordinateSourceContractVersion"), 1);
    payload->insert(QStringLiteral("coordinateSourceOutputContract"),
                    templateOutputContract());
    payload->insert(QStringLiteral("coordinateSourceConfigSignature"),
                    coordinateSourceConfigSignature(config));
}

/// 将来源身份字段透传到位置修正等中间节点结果，保留可追溯性。
inline void propagateIdentityFields(const QJsonObject &source, QJsonObject *target)
{
    if (!target)
        return;
    const QStringList fields{
        QStringLiteral("coordinateSourceContractVersion"),
        QStringLiteral("coordinateSourceOutputContract"),
        QStringLiteral("coordinateSourceReferenceSignature"),
        QStringLiteral("coordinateSourceReferenceContract"),
        QStringLiteral("coordinateSourceReferenceBaseIds"),
        QStringLiteral("coordinateSourceReferenceRevisions"),
        QStringLiteral("coordinateSourceConfigSignature"),
        QStringLiteral("coordinateSourceSignature"),
        QStringLiteral("originMode"),
        QStringLiteral("customOriginNormalized"),
        QStringLiteral("modelSignature"),
        QStringLiteral("modelSetSignature"),
        QStringLiteral("selectedTemplateId"),
        QStringLiteral("selectedTemplateName"),
        QStringLiteral("selectedBaseId"),
        QStringLiteral("primaryMatchStrategy"),
        QStringLiteral("lockedTemplateId"),
        QStringLiteral("coordinateSystem"),
        QStringLiteral("angleUnit")
    };
    for (const QString &field : fields) {
        if (source.contains(field))
            target->insert(field, source.value(field));
    }
}

} // namespace CalibrationSourceFingerprint

#endif // CALIBRATION_CALIBRATIONSOURCEFINGERPRINT_H
