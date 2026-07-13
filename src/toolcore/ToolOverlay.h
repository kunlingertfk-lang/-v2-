#ifndef TOOLCORE_TOOLOVERLAY_H
#define TOOLCORE_TOOLOVERLAY_H

#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

enum class ToolOverlayType {
    Unknown,
    Rect,
    Line,
    Circle,
    Polygon,
    Text
};

inline QString toolOverlayTypeToString(ToolOverlayType type)
{
    switch (type) {
    case ToolOverlayType::Rect:
        return QStringLiteral("Rect");
    case ToolOverlayType::Line:
        return QStringLiteral("Line");
    case ToolOverlayType::Circle:
        return QStringLiteral("Circle");
    case ToolOverlayType::Polygon:
        return QStringLiteral("Polygon");
    case ToolOverlayType::Text:
        return QStringLiteral("Text");
    case ToolOverlayType::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

inline ToolOverlayType toolOverlayTypeFromString(const QString &value)
{
    const QString key = value.trimmed().toLower();
    if (key == QStringLiteral("rect"))
        return ToolOverlayType::Rect;
    if (key == QStringLiteral("line"))
        return ToolOverlayType::Line;
    if (key == QStringLiteral("circle"))
        return ToolOverlayType::Circle;
    if (key == QStringLiteral("polygon"))
        return ToolOverlayType::Polygon;
    if (key == QStringLiteral("text"))
        return ToolOverlayType::Text;
    return ToolOverlayType::Unknown;
}

struct ToolOverlay {
    ToolOverlayType type = ToolOverlayType::Unknown;
    QRectF rect;
    QPointF p1;
    QPointF p2;
    QPointF center;
    double radius = 0.0;
    QVector<QPointF> points;
    QString text;
    QString label;
    double score = 0.0;
    QJsonObject extra;

    ToolOverlay() = default;

    QJsonObject toJson() const
    {
        QJsonObject json;
        json.insert(QStringLiteral("type"), toolOverlayTypeToString(type));
        json.insert(QStringLiteral("rect"), rectToJson(rect));
        json.insert(QStringLiteral("p1"), pointToJson(p1));
        json.insert(QStringLiteral("p2"), pointToJson(p2));
        json.insert(QStringLiteral("center"), pointToJson(center));
        json.insert(QStringLiteral("radius"), radius);
        json.insert(QStringLiteral("points"), pointsToJson(points));
        json.insert(QStringLiteral("text"), text);
        json.insert(QStringLiteral("label"), label);
        json.insert(QStringLiteral("score"), score);
        json.insert(QStringLiteral("extra"), extra);
        return json;
    }

    static ToolOverlay fromJson(const QJsonObject &json)
    {
        ToolOverlay overlay;
        overlay.type = toolOverlayTypeFromString(json.value(QStringLiteral("type")).toString());
        overlay.rect = rectFromJson(json.value(QStringLiteral("rect")).toObject());
        overlay.p1 = pointFromJson(json.value(QStringLiteral("p1")).toObject());
        overlay.p2 = pointFromJson(json.value(QStringLiteral("p2")).toObject());
        overlay.center = pointFromJson(json.value(QStringLiteral("center")).toObject());
        overlay.radius = json.value(QStringLiteral("radius")).toDouble();
        overlay.points = pointsFromJson(json.value(QStringLiteral("points")).toArray());
        overlay.text = json.value(QStringLiteral("text")).toString();
        overlay.label = json.value(QStringLiteral("label")).toString();
        overlay.score = json.value(QStringLiteral("score")).toDouble();
        overlay.extra = json.value(QStringLiteral("extra")).toObject();
        return overlay;
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

    static QJsonObject pointToJson(const QPointF &point)
    {
        QJsonObject json;
        json.insert(QStringLiteral("x"), point.x());
        json.insert(QStringLiteral("y"), point.y());
        return json;
    }

    static QPointF pointFromJson(const QJsonObject &json)
    {
        return QPointF(json.value(QStringLiteral("x")).toDouble(),
                       json.value(QStringLiteral("y")).toDouble());
    }

    static QJsonArray pointsToJson(const QVector<QPointF> &points)
    {
        QJsonArray array;
        for (const QPointF &point : points)
            array.append(pointToJson(point));
        return array;
    }

    static QVector<QPointF> pointsFromJson(const QJsonArray &array)
    {
        QVector<QPointF> points;
        points.reserve(array.size());
        for (const QJsonValue &value : array)
            points.append(pointFromJson(value.toObject()));
        return points;
    }
};

#endif // TOOLCORE_TOOLOVERLAY_H
