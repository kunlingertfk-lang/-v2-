#ifndef CALIBRATION_CALIBRATIONSOURCEFINGERPRINT_H
#define CALIBRATION_CALIBRATIONSOURCEFINGERPRINT_H

#include "toolcore/ToolConfig.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

#include <algorithm>

/**
 * Stable identity contract for a producer whose image-space pose is used to
 * create or execute a calibration.  This is deliberately independent from
 * TemplateLocationHalconRunner's model cache signature: the latter identifies
 * the generated HALCON model, while this contract also covers output semantics
 * such as the selected origin and the subscribed fields.
 */
namespace CalibrationSourceFingerprint {

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

inline QString sha256(const QJsonValue &value)
{
    return QString::fromLatin1(QCryptographicHash::hash(
                                  canonicalJson(value),
                                  QCryptographicHash::Sha256).toHex());
}

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

inline QJsonObject normalizedPoint(const QJsonObject &point,
                                   double fallbackX = 0.5,
                                   double fallbackY = 0.5)
{
    return QJsonObject{
        {QStringLiteral("x"), point.value(QStringLiteral("x")).toDouble(fallbackX)},
        {QStringLiteral("y"), point.value(QStringLiteral("y")).toDouble(fallbackY)}
    };
}

inline QJsonObject templateConfigContract(const ToolConfig &config)
{
    QJsonObject params = config.params;
    // These values control execution environment/cache lifecycle, not the
    // image-space coordinate produced by a successful match.
    params.remove(QStringLiteral("halconSoPath"));
    params.remove(QStringLiteral("timeoutMs"));
    params.remove(QStringLiteral("modelCacheKey"));
    params.remove(QStringLiteral("modelCreated"));
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

inline QString coordinateSourceConfigSignature(const ToolConfig &config)
{
    return sha256(templateConfigContract(config));
}

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

inline QString coordinateSourceSignature(const QJsonObject &fingerprint)
{
    QJsonObject unsignedFingerprint = fingerprint;
    unsignedFingerprint.remove(QStringLiteral("coordinateSourceSignature"));
    return sha256(unsignedFingerprint);
}

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
    fingerprint.insert(QStringLiteral("coordinateSourceSignature"),
                       coordinateSourceSignature(fingerprint));
    return fingerprint;
}

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
        const QString signature = fingerprint.value(
                    QStringLiteral("coordinateSourceSignature"))
                .toString().trimmed();
        if (!signature.isEmpty()
                && signature != coordinateSourceSignature(fingerprint)) {
            if (mismatchField)
                *mismatchField = QStringLiteral("coordinateSourceSignature");
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
    if (mismatchField)
        mismatchField->clear();
    return true;
}

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

inline void propagateIdentityFields(const QJsonObject &source, QJsonObject *target)
{
    if (!target)
        return;
    const QStringList fields{
        QStringLiteral("coordinateSourceContractVersion"),
        QStringLiteral("coordinateSourceOutputContract"),
        QStringLiteral("coordinateSourceReferenceSignature"),
        QStringLiteral("coordinateSourceConfigSignature"),
        QStringLiteral("coordinateSourceSignature"),
        QStringLiteral("originMode"),
        QStringLiteral("customOriginNormalized"),
        QStringLiteral("modelSignature"),
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
