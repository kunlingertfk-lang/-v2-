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

QString PositionCorrection::defaultSourceId()
{
    return QStringLiteral("reference.positionCorrection");
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
    config.sourceId = stringParam(params,
                                  QStringLiteral("positionCorrectionSourceId"),
                                  defaultSourceId());
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
    params->insert(QStringLiteral("positionCorrectionSourceId"),
                   config.sourceId.trimmed().isEmpty() ? defaultSourceId() : config.sourceId);
    params->insert(QStringLiteral("positionCorrectionSource"),
                   config.source.trimmed().isEmpty() ? defaultSource() : config.source);
}

void PositionCorrection::writeNotAppliedPayload(const PositionCorrectionConfig &config,
                                                QJsonObject *payload)
{
    if (!payload)
        return;

    payload->insert(QStringLiteral("enablePositionCorrection"), config.enabled);
    payload->insert(QStringLiteral("positionCorrectionSourceId"),
                    config.sourceId.trimmed().isEmpty() ? defaultSourceId() : config.sourceId);
    payload->insert(QStringLiteral("positionCorrectionSource"),
                    config.source.trimmed().isEmpty() ? defaultSource() : config.source);
    payload->insert(QStringLiteral("positionCorrectionApplied"), false);
    payload->insert(QStringLiteral("positionCorrectionReason"), notImplementedReason());
}

QVector<PositionCorrectionSource> PositionCorrection::sourcesBefore(
        const QVector<ToolConfig> &tools,
        int consumerIndex,
        bool referenceEnabled)
{
    QVector<PositionCorrectionSource> sources;
    if (referenceEnabled) {
        sources.append(PositionCorrectionSource{defaultSourceId(),
                                                defaultSource(),
                                                -1,
                                                true});
    }

    const int limit = qBound(0, consumerIndex, tools.size());
    for (int index = 0; index < limit; ++index) {
        const ToolConfig &tool = tools.at(index);
        if (!tool.enabled || tool.toolType != ToolType::PositionCorrection
                || tool.toolId.trimmed().isEmpty()) {
            continue;
        }

        const QString name = tool.displayName.trimmed().isEmpty()
                ? QStringLiteral("位置修正")
                : tool.displayName.trimmed();
        sources.append(PositionCorrectionSource{
                           tool.toolId,
                           QStringLiteral("%1 %2.位置修正信息").arg(index + 1).arg(name),
                           index,
                           false});
    }
    return sources;
}

bool PositionCorrection::isSourceAvailable(
        const QVector<PositionCorrectionSource> &sources,
        const QString &sourceId)
{
    const QString id = sourceId.trimmed();
    for (const PositionCorrectionSource &source : sources) {
        if (source.sourceId == id)
            return true;
    }
    return false;
}

ReferencePositionCorrectionConfig PositionCorrection::referenceFromJson(
        const QJsonObject &json)
{
    ReferencePositionCorrectionConfig config;
    config.version = json.value(QStringLiteral("version")).toInt(1);
    config.enabled = json.value(QStringLiteral("enabled")).toBool(false);
    config.templateRegionType = stringParam(json,
                                            QStringLiteral("templateRegionType"),
                                            QStringLiteral("rectangle"));
    const QJsonObject roi = json.value(QStringLiteral("templateRoiNormalized")).toObject();
    config.templateRoiNormalized = QRectF(roi.value(QStringLiteral("x")).toDouble(),
                                          roi.value(QStringLiteral("y")).toDouble(),
                                          roi.value(QStringLiteral("width")).toDouble(),
                                          roi.value(QStringLiteral("height")).toDouble());
    config.templatePolygonNormalized =
            json.value(QStringLiteral("templatePolygonNormalized")).toArray();
    return config;
}

QJsonObject PositionCorrection::referenceToJson(
        const ReferencePositionCorrectionConfig &config)
{
    QJsonObject roi;
    roi.insert(QStringLiteral("x"), config.templateRoiNormalized.x());
    roi.insert(QStringLiteral("y"), config.templateRoiNormalized.y());
    roi.insert(QStringLiteral("width"), config.templateRoiNormalized.width());
    roi.insert(QStringLiteral("height"), config.templateRoiNormalized.height());

    QJsonObject json;
    json.insert(QStringLiteral("version"), config.version);
    json.insert(QStringLiteral("enabled"), config.enabled);
    json.insert(QStringLiteral("templateRegionType"),
                config.templateRegionType.trimmed().isEmpty()
                    ? QStringLiteral("rectangle")
                    : config.templateRegionType);
    json.insert(QStringLiteral("templateRoiNormalized"), roi);
    json.insert(QStringLiteral("templatePolygonNormalized"),
                config.templatePolygonNormalized);
    return json;
}
