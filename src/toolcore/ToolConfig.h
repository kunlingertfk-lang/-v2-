#ifndef TOOLCORE_TOOLCONFIG_H
#define TOOLCORE_TOOLCONFIG_H

#include "ToolTypes.h"

#include <QJsonObject>
#include <QRectF>
#include <QString>

struct ToolConfig {
    int schemaVersion = 1;
    QString toolId;
    QString toolName;
    ToolType toolType = ToolType::Unknown;
    ToolCategory category = ToolCategory::Unknown;
    bool enabled = true;
    QRectF roiNormalized;
    QJsonObject params;
    QJsonObject judgeRule;
    QString displayName;
    QString summary;

    ToolConfig() = default;

    bool isValid() const
    {
        return !toolId.trimmed().isEmpty()
                && toolType != ToolType::Unknown
                && category != ToolCategory::Unknown;
    }

    QJsonObject toJson() const
    {
        QJsonObject json;
        json.insert(QStringLiteral("schemaVersion"), schemaVersion);
        json.insert(QStringLiteral("toolId"), toolId);
        json.insert(QStringLiteral("toolName"), toolName);
        json.insert(QStringLiteral("toolType"), toolTypeToString(toolType));
        json.insert(QStringLiteral("category"), toolCategoryToString(category));
        json.insert(QStringLiteral("enabled"), enabled);
        json.insert(QStringLiteral("roiNormalized"), rectToJson(roiNormalized));
        json.insert(QStringLiteral("params"), params);
        json.insert(QStringLiteral("judgeRule"), judgeRule);
        json.insert(QStringLiteral("displayName"), displayName);
        json.insert(QStringLiteral("summary"), summary);
        return json;
    }

    static ToolConfig fromJson(const QJsonObject &json)
    {
        ToolConfig config;
        config.schemaVersion = json.value(QStringLiteral("schemaVersion")).toInt(1);
        config.toolId = json.value(QStringLiteral("toolId")).toString();
        config.toolName = json.value(QStringLiteral("toolName")).toString();
        config.toolType = toolTypeFromString(json.value(QStringLiteral("toolType")).toString());
        config.category = toolCategoryFromString(json.value(QStringLiteral("category")).toString());
        config.enabled = json.value(QStringLiteral("enabled")).toBool(true);
        config.roiNormalized = rectFromJson(json.value(QStringLiteral("roiNormalized")).toObject());
        config.params = json.value(QStringLiteral("params")).toObject();
        config.judgeRule = json.value(QStringLiteral("judgeRule")).toObject();
        config.displayName = json.value(QStringLiteral("displayName")).toString();
        config.summary = json.value(QStringLiteral("summary")).toString();
        return config;
    }

private:
    static QJsonObject rectToJson(const QRectF &rect)
    {
        QJsonObject json;
        json.insert(QStringLiteral("x"), rect.x());
        json.insert(QStringLiteral("y"), rect.y());
        json.insert(QStringLiteral("width"), rect.width());
        json.insert(QStringLiteral("height"), rect.height());
        return json;
    }

    static QRectF rectFromJson(const QJsonObject &json)
    {
        return QRectF(json.value(QStringLiteral("x")).toDouble(),
                      json.value(QStringLiteral("y")).toDouble(),
                      json.value(QStringLiteral("width")).toDouble(),
                      json.value(QStringLiteral("height")).toDouble());
    }
};

#endif // TOOLCORE_TOOLCONFIG_H
