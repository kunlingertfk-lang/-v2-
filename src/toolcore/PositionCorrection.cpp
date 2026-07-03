#include "toolcore/PositionCorrection.h"

#include <QJsonValue>

namespace {

bool boolParam(const QJsonObject &object, const QString &key, bool defaultValue)
{
    const QJsonValue value = object.value(key);
    if (value.isBool())
        return value.toBool(defaultValue);

    const QString text = value.toString().trimmed().toLower();
    if (text == QStringLiteral("true") || text == QStringLiteral("1") || text == QStringLiteral("yes"))
        return true;
    if (text == QStringLiteral("false") || text == QStringLiteral("0") || text == QStringLiteral("no"))
        return false;
    return defaultValue;
}

QString stringParam(const QJsonObject &object,
                    const QString &key,
                    const QString &defaultValue)
{
    const QString value = object.value(key).toString().trimmed();
    return value.isEmpty() ? defaultValue : value;
}

} // namespace

QString PositionCorrection::defaultSource()
{
    return QStringLiteral("1 基准图.位置修正信息");
}

QString PositionCorrection::notImplementedReason()
{
    return QStringLiteral("not implemented");
}

PositionCorrectionConfig PositionCorrection::fromParams(
        const QJsonObject &params,
        bool defaultEnabled,
        const QString &defaultSourceText)
{
    PositionCorrectionConfig config;
    config.enabled = boolParam(params,
                               QStringLiteral("enablePositionCorrection"),
                               defaultEnabled);
    config.source = stringParam(params,
                                QStringLiteral("positionCorrectionSource"),
                                defaultSourceText);
    return config;
}

void PositionCorrection::writeParams(const PositionCorrectionConfig &config,
                                     QJsonObject *params)
{
    if (!params)
        return;

    params->insert(QStringLiteral("enablePositionCorrection"), config.enabled);
    params->insert(QStringLiteral("positionCorrectionSource"),
                   config.source.trimmed().isEmpty() ? defaultSource() : config.source);
}

void PositionCorrection::writeNotAppliedPayload(const PositionCorrectionConfig &config,
                                                QJsonObject *payload)
{
    if (!payload)
        return;

    payload->insert(QStringLiteral("enablePositionCorrection"), config.enabled);
    payload->insert(QStringLiteral("positionCorrectionSource"),
                    config.source.trimmed().isEmpty() ? defaultSource() : config.source);
    payload->insert(QStringLiteral("positionCorrectionApplied"), false);
    payload->insert(QStringLiteral("positionCorrectionReason"), notImplementedReason());
}
