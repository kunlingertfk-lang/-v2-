#ifndef TOOLCORE_POSITIONCORRECTION_H
#define TOOLCORE_POSITIONCORRECTION_H

#include <QJsonObject>
#include <QJsonArray>
#include <QRectF>
#include <QString>
#include <QVector>

#include "toolcore/ToolConfig.h"

struct PositionCorrectionConfig
{
    bool enabled = false;
    QString source;
    QString sourceId;

    PositionCorrectionConfig() = default;
    PositionCorrectionConfig(bool enabledValue,
                             const QString &sourceText,
                             const QString &stableSourceId = QString())
        : enabled(enabledValue)
        , source(sourceText)
        , sourceId(stableSourceId)
    {
    }
};

struct PositionCorrectionSource
{
    QString sourceId;
    QString displayText;
    int toolIndex = -1;
    bool referenceSource = false;
};

struct ReferencePositionCorrectionConfig
{
    int version = 1;
    bool enabled = false;
    QString templateRegionType = QStringLiteral("rectangle");
    QRectF templateRoiNormalized;
    QJsonArray templatePolygonNormalized;
};

namespace PositionCorrection {

QString defaultSource();
QString defaultSourceId();
QString notImplementedReason();

PositionCorrectionConfig fromParams(
        const QJsonObject &params,
        bool defaultEnabled = false,
        const QString &defaultSourceText = PositionCorrection::defaultSource());

void writeParams(const PositionCorrectionConfig &config, QJsonObject *params);
void writeNotAppliedPayload(const PositionCorrectionConfig &config, QJsonObject *payload);

QVector<PositionCorrectionSource> sourcesBefore(const QVector<ToolConfig> &tools,
                                                int consumerIndex,
                                                bool referenceEnabled);
bool isSourceAvailable(const QVector<PositionCorrectionSource> &sources,
                       const QString &sourceId);
ReferencePositionCorrectionConfig referenceFromJson(const QJsonObject &json);
QJsonObject referenceToJson(const ReferencePositionCorrectionConfig &config);

} // namespace PositionCorrection

#endif // TOOLCORE_POSITIONCORRECTION_H
