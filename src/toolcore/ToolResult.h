#ifndef TOOLCORE_TOOLRESULT_H
#define TOOLCORE_TOOLRESULT_H

#include "ToolOverlay.h"
#include "ToolTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <QtGlobal>

struct ToolResult {
    int schemaVersion = 1;
    QString toolId;
    ToolType toolType = ToolType::Unknown;
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    double score = 0.0;
    double value = 0.0;
    int count = 0;
    qint64 elapsedMs = 0;
    QString text;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;

    ToolResult() = default;

    static ToolResult error(const QString &toolId,
                            ToolType toolType,
                            const QString &message,
                            const QString &status = QString())
    {
        ToolResult result;
        result.toolId = toolId;
        result.toolType = toolType;
        result.success = false;
        result.ok = false;
        result.status = status.isEmpty() ? QStringLiteral("error") : status;
        result.message = message;
        return result;
    }

    static ToolResult unsupported(const QString &toolId,
                                  ToolType toolType,
                                  const QString &message = QString())
    {
        ToolResult result;
        result.toolId = toolId;
        result.toolType = toolType;
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("unsupported");
        result.message = message.isEmpty()
                ? QStringLiteral("No ToolAdapter registered for this ToolType.")
                : message;
        return result;
    }

    QJsonObject toJson() const
    {
        QJsonArray overlayArray;
        for (const ToolOverlay &overlay : overlays)
            overlayArray.append(overlay.toJson());

        QJsonObject json;
        json.insert(QStringLiteral("schemaVersion"), schemaVersion);
        json.insert(QStringLiteral("toolId"), toolId);
        json.insert(QStringLiteral("toolType"), toolTypeToString(toolType));
        json.insert(QStringLiteral("success"), success);
        json.insert(QStringLiteral("ok"), ok);
        json.insert(QStringLiteral("status"), status);
        json.insert(QStringLiteral("message"), message);
        json.insert(QStringLiteral("score"), score);
        json.insert(QStringLiteral("value"), value);
        json.insert(QStringLiteral("count"), count);
        json.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
        json.insert(QStringLiteral("text"), text);
        json.insert(QStringLiteral("overlays"), overlayArray);
        json.insert(QStringLiteral("payload"), payload);
        return json;
    }

    static ToolResult fromJson(const QJsonObject &json)
    {
        ToolResult result;
        result.schemaVersion = json.value(QStringLiteral("schemaVersion")).toInt(1);
        result.toolId = json.value(QStringLiteral("toolId")).toString();
        result.toolType = toolTypeFromString(json.value(QStringLiteral("toolType")).toString());
        result.success = json.value(QStringLiteral("success")).toBool();
        result.ok = json.value(QStringLiteral("ok")).toBool();
        result.status = json.value(QStringLiteral("status")).toString();
        result.message = json.value(QStringLiteral("message")).toString();
        result.score = json.value(QStringLiteral("score")).toDouble();
        result.value = json.value(QStringLiteral("value")).toDouble();
        result.count = json.value(QStringLiteral("count")).toInt();
        const QJsonValue elapsedValue = json.value(QStringLiteral("elapsedMs"));
        result.elapsedMs = elapsedValue.isString()
                ? elapsedValue.toString().toLongLong()
                : static_cast<qint64>(elapsedValue.toDouble());
        result.text = json.value(QStringLiteral("text")).toString();
        result.payload = json.value(QStringLiteral("payload")).toObject();

        const QJsonArray overlayArray = json.value(QStringLiteral("overlays")).toArray();
        result.overlays.reserve(overlayArray.size());
        for (const QJsonValue &value : overlayArray)
            result.overlays.append(ToolOverlay::fromJson(value.toObject()));
        return result;
    }
};

#endif // TOOLCORE_TOOLRESULT_H
