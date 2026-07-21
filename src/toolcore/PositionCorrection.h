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

    /** 创建默认关闭且未指定来源的位置修正消费配置。 */
    PositionCorrectionConfig() = default;
    /** 创建带启用状态、显示文本和可选稳定来源 ID 的消费配置。 */
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

/** 返回方案级基准图位置修正来源的默认显示文本。 */
QString defaultSource();
/** 返回方案级基准图位置修正来源的固定稳定 ID。 */
QString defaultSourceId();
/** 返回后端未接入阶段统一使用的英文原因码。 */
QString notImplementedReason();

/** 从工具参数中兼容解析位置修正开关、稳定来源 ID 和显示文本。 */
PositionCorrectionConfig fromParams(
        const QJsonObject &params,
        bool defaultEnabled = false,
        const QString &defaultSourceText = PositionCorrection::defaultSource());

/** 将位置修正消费配置写回工具参数，空来源自动补充基准图默认值。 */
void writeParams(const PositionCorrectionConfig &config, QJsonObject *params);
/** 将“已配置但未应用”的事实状态写入工具运行结果 payload。 */
void writeNotAppliedPayload(const PositionCorrectionConfig &config, QJsonObject *payload);

/** 返回消费工具之前可用的位置修正节点，禁止后向引用和自引用。 */
QVector<PositionCorrectionSource> sourcesBefore(const QVector<ToolConfig> &tools,
                                                int consumerIndex,
                                                bool referenceEnabled);
/** 判断稳定来源 ID 是否仍存在于当前有效来源集合中。 */
bool isSourceAvailable(const QVector<PositionCorrectionSource> &sources,
                       const QString &sourceId);
/** 从方案 JSON 解析独立的基准图位置修正配置。 */
ReferencePositionCorrectionConfig referenceFromJson(const QJsonObject &json);
/** 将独立的基准图位置修正配置序列化为方案 JSON。 */
QJsonObject referenceToJson(const ReferencePositionCorrectionConfig &config);

} // namespace PositionCorrection

#endif // TOOLCORE_POSITIONCORRECTION_H
