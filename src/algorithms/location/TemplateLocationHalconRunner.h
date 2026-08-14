#ifndef ALGORITHMS_LOCATION_TEMPLATELOCATIONHALCONRUNNER_H
#define ALGORITHMS_LOCATION_TEMPLATELOCATIONHALCONRUNNER_H

#include "algorithms/location/TemplateLocationConfig.h"
#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <opencv2/core.hpp>

struct TemplateLocationHalconResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    double score = 0.0;
    int count = 0;
    qint64 elapsedMs = 0;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

/// 基于 HALCON 形状模板定位目标，输出快速标定采点所需的位置与方向。
class TemplateLocationHalconRunner
{
public:
    /// 清理指定模板配置对应的持久化 HALCON 模型缓存。
    static bool clearPersistentCache(const QString &modelCacheKey);
    /// 在当前图像中匹配参考模板，返回 X/Y/Angle、质量状态与叠加结果。
    TemplateLocationHalconResult run(const cv::Mat &image,
                                     const cv::Mat &referenceImage,
                                     const TemplateLocationHalconConfig &config);
    /// 以一次 HALCON 多模型搜索执行 1..N 个候选模板，并输出模板来源身份。
    TemplateLocationHalconResult run(const cv::Mat &image,
                                     const cv::Mat &referenceImage,
                                     const TemplateLocationModelBankConfig &config);
    /// Execute the v6 composite bank against its immutable Base image set.
    /// v4/v5 callers are also accepted and use the first available image as a
    /// compatibility fallback; their matching semantics remain unchanged.
    TemplateLocationHalconResult run(
            const cv::Mat &image,
            const QMap<QString, cv::Mat> &referenceImages,
            const QMap<QString, QString> &referenceImageRevisions,
            const TemplateLocationModelBankConfig &config,
            const QString &primaryBaseId = QString());
};

#endif // ALGORITHMS_LOCATION_TEMPLATELOCATIONHALCONRUNNER_H
