#ifndef TOOLCORE_TOOLPREVIEWSNAPSHOT_H
#define TOOLCORE_TOOLPREVIEWSNAPSHOT_H

#include "ToolConfig.h"
#include "ToolResult.h"

#include <QDateTime>
#include <QJsonObject>
#include <QJsonValue>
#include <QRectF>
#include <QString>
#include <QVariantMap>
#include <QVector>

struct ToolPreviewSnapshot
{
    bool valid = false;
    QString toolId;
    ToolType toolType = ToolType::Unknown;
    QString sourceType;
    ToolResult result;
    QVector<ToolOverlay> overlays;
    QString statusText;
    bool ok = false;
    double score = 0.0;
    int count = 0;
    QVariantMap payload;
    QRectF roiNormalized;
    QDateTime timestamp;
};

inline QString toolPreviewStatusText(const ToolResult &result)
{
    if (result.status.isEmpty())
        return result.message;

    if (result.message.isEmpty())
        return result.status;

    return QStringLiteral("%1 | %2").arg(result.status, result.message);
}

inline ToolPreviewSnapshot makeReferenceToolPreviewSnapshot(const ToolConfig &config,
                                                            const ToolResult &result,
                                                            const QRectF &roiNormalized)
{
    ToolPreviewSnapshot snapshot;
    snapshot.valid = true;
    snapshot.toolId = config.toolId;
    snapshot.toolType = config.toolType;
    snapshot.sourceType = QStringLiteral("referenceImage");
    snapshot.result = result;
    snapshot.overlays = result.overlays;
    snapshot.statusText = toolPreviewStatusText(result);
    snapshot.ok = result.ok;
    snapshot.score = result.score;
    snapshot.count = result.count;
    snapshot.payload = result.payload.toVariantMap();
    snapshot.roiNormalized = roiNormalized;
    snapshot.timestamp = QDateTime::currentDateTime();
    return snapshot;
}

inline QJsonObject toolPreviewRectToJson(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

inline QRectF toolPreviewRectFromJson(const QJsonObject &json)
{
    return QRectF(json.value(QStringLiteral("x")).toDouble(),
                  json.value(QStringLiteral("y")).toDouble(),
                  json.value(QStringLiteral("width")).toDouble(),
                  json.value(QStringLiteral("height")).toDouble());
}

inline QJsonObject toolPreviewSnapshotToJson(const ToolPreviewSnapshot &snapshot)
{
    QJsonObject json;
    json.insert(QStringLiteral("valid"), snapshot.valid);
    json.insert(QStringLiteral("toolId"), snapshot.toolId);
    json.insert(QStringLiteral("toolType"), toolTypeToString(snapshot.toolType));
    json.insert(QStringLiteral("sourceType"), snapshot.sourceType);
    json.insert(QStringLiteral("result"), snapshot.result.toJson());
    json.insert(QStringLiteral("statusText"), snapshot.statusText);
    json.insert(QStringLiteral("ok"), snapshot.ok);
    json.insert(QStringLiteral("score"), snapshot.score);
    json.insert(QStringLiteral("count"), snapshot.count);
    json.insert(QStringLiteral("payload"), QJsonObject::fromVariantMap(snapshot.payload));
    json.insert(QStringLiteral("roiNormalized"), toolPreviewRectToJson(snapshot.roiNormalized));
    json.insert(QStringLiteral("timestamp"), snapshot.timestamp.toString(Qt::ISODate));
    return json;
}

inline ToolPreviewSnapshot toolPreviewSnapshotFromJson(const QJsonObject &json)
{
    ToolPreviewSnapshot snapshot;
    snapshot.valid = json.value(QStringLiteral("valid")).toBool(false);
    snapshot.toolId = json.value(QStringLiteral("toolId")).toString();
    snapshot.toolType = toolTypeFromString(json.value(QStringLiteral("toolType")).toString());
    snapshot.sourceType = json.value(QStringLiteral("sourceType")).toString();
    snapshot.result = ToolResult::fromJson(json.value(QStringLiteral("result")).toObject());
    snapshot.overlays = snapshot.result.overlays;
    snapshot.statusText = json.value(QStringLiteral("statusText")).toString();
    snapshot.ok = json.value(QStringLiteral("ok")).toBool(snapshot.result.ok);
    snapshot.score = json.value(QStringLiteral("score")).toDouble(snapshot.result.score);
    snapshot.count = json.value(QStringLiteral("count")).toInt(snapshot.result.count);
    snapshot.payload = json.value(QStringLiteral("payload")).toObject().toVariantMap();
    if (snapshot.payload.isEmpty())
        snapshot.payload = snapshot.result.payload.toVariantMap();
    snapshot.roiNormalized = toolPreviewRectFromJson(json.value(QStringLiteral("roiNormalized")).toObject());
    snapshot.timestamp = QDateTime::fromString(json.value(QStringLiteral("timestamp")).toString(), Qt::ISODate);
    return snapshot;
}

#endif // TOOLCORE_TOOLPREVIEWSNAPSHOT_H
