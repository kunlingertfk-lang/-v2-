#ifndef TOOLCORE_POSITIONCORRECTION_H
#define TOOLCORE_POSITIONCORRECTION_H

#include <QJsonObject>
#include <QJsonArray>
#include <QPointF>
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

struct PositionPoseField
{
    QString outputKey;
    QString displayText;
    QString valueKind;
};

struct PositionPoseProducer
{
    QString producerId;
    QString displayText;
    int toolIndex = -1;
    QVector<PositionPoseField> fields;
};

struct PositionReferencePoseProducer
{
    QString sourceId;
    QString displayText;
    QJsonObject referencePose;
    QJsonObject runtimeConfig;
};

struct PositionRunPoseSource
{
    int version = 2;
    QString producerId;
    QString xKey = QStringLiteral("x");
    QString yKey = QStringLiteral("y");
    QString angleKey = QStringLiteral("angle");
    QString scaleKey = QStringLiteral("scale");
    QString displayText;
    bool valid = false;
    bool inconsistent = false;
    QString errorCode;
};

struct ReferencePositionCorrectionConfig
{
    int version = 3;
    bool enabled = false;
    // Version 4 reference envelopes keep the complete template-location
    // contract below.  The nested object uses the public v4/v5 locator codec;
    // a v5 templates[] array is the only template source for a bank.
    QJsonObject locator;
    // Stable templateId -> frozen reference pose.  Values use the existing
    // locatorIdentityVersion/locatorTemplateId/locatorModelSignature fields.
    QJsonObject referencePosesByTemplateId;
    QString templateRegionType = QStringLiteral("rectangle");
    QRectF templateRoiNormalized;
    QJsonArray templatePolygonNormalized;
    QString templateMaskRegionType = QStringLiteral("none");
    QRectF templateMaskRoiNormalized;
    QJsonArray templateMaskPolygonNormalized;
    QPointF templateMaskCircleCenterNormalized;
    double templateMaskCircleRadiusNormalized = 0.0;
    QString originMode = QStringLiteral("centroid");
    QPointF customOriginNormalized = QPointF(0.5, 0.5);
    bool referenceCreated = false;
    QJsonObject referencePose;
    QString modelCacheKey;
    QString status;
    QString message;
    double score = 0.0;
    qint64 elapsedMs = 0;
    // Preserve extension fields (notably a future locator bank) across normal
    // scheme load/save even while the current reference editor cannot use them.
    QJsonObject extra;
};

namespace PositionCorrection {

/** 返回方案级基准图位置修正来源的默认显示文本。 */
QString defaultSource();
/** 返回方案级基准图位置修正来源的固定稳定 ID。 */
QString defaultSourceId();
/** 将当前/历史基准图显示文本归一化为固定稳定 ID。 */
QString normalizedSourceId(const QString &sourceIdOrDisplayText);
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
/** 返回指定工具真实声明的位置姿态输出字段；当前仅模板定位声明 x/y/angle。 */
QVector<PositionPoseField> poseFieldsForTool(const ToolConfig &tool);
/** 返回消费工具之前真实可作为 runPoseSource 的位姿生产者。 */
QVector<PositionPoseProducer> poseProducersBefore(const QVector<ToolConfig> &tools,
                                                  int consumerIndex);
/** 兼容读取新版 runPoseSource，或从旧 runPointX/runPointY/runAngle 收敛迁移。 */
PositionRunPoseSource runPoseSourceFromConfig(const QJsonObject &positionCorrection);
/** 将 runPoseSource 与旧三字段回显兼容对象同时写入配置。 */
void writeRunPoseSource(const PositionRunPoseSource &source,
                        QJsonObject *positionCorrection);
/** 从方案 JSON 解析独立的基准图位置修正配置。 */
ReferencePositionCorrectionConfig referenceFromJson(const QJsonObject &json);
/** 将独立的基准图位置修正配置序列化为方案 JSON。 */
QJsonObject referenceToJson(const ReferencePositionCorrectionConfig &config);

} // namespace PositionCorrection

#endif // TOOLCORE_POSITIONCORRECTION_H
