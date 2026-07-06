#ifndef ALGORITHMS_RECOGNITION_COLORRECOGNITIONHALCONRUNNER_H
#define ALGORITHMS_RECOGNITION_COLORRECOGNITIONHALCONRUNNER_H

#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <opencv2/core.hpp>

// 颜色模板中的类别定义：UI 保存标签名，runner 使用 classId 做稳定匹配。
struct ColorRecognitionHalconLabel
{
    QString name;
    int classId = 0;
};

// 单个模板样本：包含样本所属类别、已提取的 HALCON 颜色特征和样本 ROI。
struct ColorRecognitionHalconSample
{
    QString label;
    int classId = 0;
    QVector<double> feature;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
};

// Runner 输入配置：由 Adapter 从 ToolConfig.params/judgeRule 解析而来。
struct ColorRecognitionHalconConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString detectRegionType = QStringLiteral("rectangle");
    QPointF detectCircleCenterNormalized;
    double detectCircleRadiusNormalized = 0.0;
    QRectF detectCircleBoundingRectNormalized;
    QString colorDecisionMode = QStringLiteral("dominant_ratio");
    QString featureType = QStringLiteral("histogram");
    QString sensitivity = QStringLiteral("medium");
    bool brightnessEnabled = true;
    bool enablePositionCorrection = false;
    QString positionCorrectionSource;
    int knnK = 3;
    QString knnDistance = QStringLiteral("halcon_default");
    QVector<QPointF> detectMaskPolygonNormalized;
    QVector<ColorRecognitionHalconLabel> labels;
    QVector<ColorRecognitionHalconSample> samples;
    QString judgeMode = QStringLiteral("min_score");
    int minScore = 80;
    QString expectedLabel;
};

struct ColorRecognitionHalconFeatureResult
{
    bool success = false;
    QString status;
    QString message;
    QVector<double> feature;
    qint64 elapsedMs = 0;
    QJsonObject payload;
};

// 直方图巴氏距离比较结果，供颜色比较等复用颜色识别的 HALCON 特征能力。
struct ColorRecognitionHalconHistogramCompareResult
{
    bool success = false;
    QString status;
    QString message;
    double distance = 1.0;
    qint64 elapsedMs = 0;
    QJsonObject payload;
};

// 颜色识别完整运行结果：包含 OK/NG、预测类别、分数、overlay 与诊断 payload。
struct ColorRecognitionHalconResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    QString predictedLabel;
    int predictedClassId = -1;
    double score = 0.0;
    double rating = 0.0;
    int sampleCount = 0;
    qint64 elapsedMs = 0;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class ColorRecognitionHalconRunner
{
public:
    // 从输入图像和检测 ROI 中提取 HSV/HS 直方图特征，模板采集和正式识别共用。
    ColorRecognitionHalconFeatureResult extractFeature(
            const cv::Mat &image,
            const ColorRecognitionHalconConfig &config) const;
    // 使用 HALCON tuple 算子计算两组直方图的 Bhattacharyya 距离。
    ColorRecognitionHalconHistogramCompareResult compareHistogramBhattacharyya(
            const QVector<double> &referenceHistogram,
            const QVector<double> &testHistogram,
            const ColorRecognitionHalconConfig &config) const;
    // 颜色识别主入口：提取检测特征、匹配模板样本、执行判定并生成展示 overlay。
    ColorRecognitionHalconResult run(
            const cv::Mat &image,
            const ColorRecognitionHalconConfig &config) const;
};

#endif // ALGORITHMS_RECOGNITION_COLORRECOGNITIONHALCONRUNNER_H
