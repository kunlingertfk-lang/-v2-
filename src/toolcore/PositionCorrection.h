#ifndef TOOLCORE_POSITIONCORRECTION_H
#define TOOLCORE_POSITIONCORRECTION_H

#include <QJsonObject>
#include <QString>

struct PositionCorrectionConfig
{
    bool enabled = false;
    QString source;
};

namespace PositionCorrection {

QString defaultSource();
QString notImplementedReason();

PositionCorrectionConfig fromParams(
        const QJsonObject &params,
        bool defaultEnabled = false,
        const QString &defaultSourceText = PositionCorrection::defaultSource());

void writeParams(const PositionCorrectionConfig &config, QJsonObject *params);
void writeNotAppliedPayload(const PositionCorrectionConfig &config, QJsonObject *payload);

} // namespace PositionCorrection

#endif // TOOLCORE_POSITIONCORRECTION_H
