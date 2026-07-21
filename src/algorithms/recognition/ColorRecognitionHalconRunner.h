#ifndef ALGORITHMS_RECOGNITION_COLORRECOGNITIONHALCONRUNNER_H
#define ALGORITHMS_RECOGNITION_COLORRECOGNITIONHALCONRUNNER_H

#include "toolcore/ToolOverlay.h"

#include <QByteArray>
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
    QString featureSignature;
};

// HSV 分类合同与特征合同分离：切换聚合算法不要求重新提取 ROI 直方图。
QString colorRecognitionHsvCurrentClassifierVersion();
QString colorRecognitionHsvLegacyClassifierVersion();
QString colorRecognitionHsvClassifierParamsHash(const QString &classifierVersion);

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
    QString classifierVersion = colorRecognitionHsvCurrentClassifierVersion();
    QString classifierParamsHash = colorRecognitionHsvClassifierParamsHash(classifierVersion);
    QString featureType = QStringLiteral("histogram");
    QString sensitivity = QStringLiteral("medium");
    bool brightnessEnabled = true;
    bool enablePositionCorrection = false;
    QString positionCorrectionSourceId;
    QString positionCorrectionSource;
    QString pixelFormat;
    int validBits = -1;
    int bitShift = -1;
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

// GMM 建模使用的稳定类别定义；classId 不要求连续。
struct ColorRecognitionGmmLabel
{
    QString name;
    int classId = 0;
};

// 单个 GMM 训练 ROI；cv::Mat 仅作为无损图像输入容器。
struct ColorRecognitionGmmBuildSample
{
    QString sampleId;
    int classId = 0;
    QString label;
    cv::Mat image;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString pixelFormat;
    int validBits = -1;
    int bitShift = -1;
    QString imageSha256;
};

// GMM 建模参数；首版训练超参数固定，不开放普通 UI。
struct ColorRecognitionGmmBuildConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString colorChannels = QStringLiteral("ab");
    int maxSamplesPerClass = 10000;
    QVector<ColorRecognitionGmmLabel> labels;
};

// 当前 GMM ROI 像素采样契约版本；保存模型时持久化，用于加载时判定旧模型 stale。
QString colorRecognitionGmmSamplingAlgorithmVersion();

// HALCON classify_image_class_gmm 的 K-sigma 像素拒识默认值。
inline constexpr double kColorRecognitionGmmDefaultRejectionThreshold = 0.0001;

// 每个类别的建模诊断，用于状态、签名和 smoke 验证。
struct ColorRecognitionGmmClassDiagnostics
{
    int classId = 0;
    QString label;
    int roiCount = 0;
    qint64 availablePixels = 0;
    int requestedTrainingPixels = 0;
    int trainingPixels = 0;
    int minCenters = 1;
    int maxCenters = 1;
};

// HALCON GMM 内存序列化产物。
struct ColorRecognitionGmmArtifact
{
    QByteArray serializedBytes;
    QString serializedGmmBase64;
    qint64 serializedSize = 0;
    QString serializedSha256;
};

// 已保存 GMM 产物的严格校验结果，供后续模型加载链复用。
struct ColorRecognitionGmmArtifactValidationResult
{
    bool success = false;
    QString status;
    QString message;
    qint64 serializedSize = 0;
    QString serializedSha256;
    qint64 elapsedMs = 0;
    QJsonObject payload;
};

// GMM 建模结果；失败时 artifact 必须为空。
struct ColorRecognitionGmmBuildResult
{
    bool success = false;
    QString status;
    QString message;
    QString state;
    QString algorithmVersion;
    QString featureSchemaVersion;
    QString colorChannels;
    QString trainingDataHash;
    QString buildParamsHash;
    QVector<int> classIdOrder;
    QVector<ColorRecognitionGmmClassDiagnostics> classes;
    ColorRecognitionGmmArtifact artifact;
    qint64 elapsedMs = 0;
    QJsonObject payload;
};

// 已建 GMM 的生产检测配置。模型身份字段必须与保存时完全一致。
struct ColorRecognitionGmmRunConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString detectRegionType = QStringLiteral("rectangle");
    QPointF detectCircleCenterNormalized;
    double detectCircleRadiusNormalized = 0.0;
    QRectF detectCircleBoundingRectNormalized;
    QVector<QPointF> detectMaskPolygonNormalized;
    bool enablePositionCorrection = false;
    QString positionCorrectionSourceId;
    QString positionCorrectionSource;
    QString pixelFormat;
    int validBits = -1;
    int bitShift = -1;

    QString modelState;
    QString algorithmVersion;
    QString featureSchemaVersion;
    QString colorChannels = QStringLiteral("ab");
    QString trainingDataHash;
    QString buildParamsHash;
    int maxSamplesPerClass = 10000;
    QVector<int> classIdOrder;
    QVector<ColorRecognitionGmmLabel> labels;
    QVector<ColorRecognitionGmmClassDiagnostics> classes;
    ColorRecognitionGmmArtifact artifact;

    double gmmRejectionThreshold = kColorRecognitionGmmDefaultRejectionThreshold;
    QString judgeMode = QStringLiteral("min_score");
    int minScore = 80;
    int minCategoryConfidence = 80;
    int minClassifiedCoverage = 90;
    int expectedClassId = -1;
    QString expectedLabel;
};

struct ColorRecognitionGmmClassMeasurement
{
    int classId = -1;
    QString label;
    int classIndex = -1;
    double area = 0.0;
    double ratio = 0.0;
};

struct ColorRecognitionGmmRunResult
{
    bool success = false;
    bool ok = false;
    bool measurementValid = false;
    QString status;
    QString message;
    QString predictedLabel;
    int predictedClassId = -1;
    int predictedClassIndex = -1;
    double score = 0.0;
    double rating = 0.0;
    double predictedClassRatio = 0.0;
    double categoryConfidence = 0.0;
    double classifiedCoverage = 0.0;
    double effectiveArea = 0.0;
    double classifiedArea = 0.0;
    double unclassifiedArea = 0.0;
    double unclassifiedRatio = 0.0;
    QVector<ColorRecognitionGmmClassMeasurement> classes;
    QStringList failureReasons;
    QVector<ToolOverlay> overlays;
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
    // 从多类别原始 ROI 样本建立 CIELAB GMM，并返回经过验证的内存模型产物。
    ColorRecognitionGmmBuildResult buildGmmTemplateModel(
            const QVector<ColorRecognitionGmmBuildSample> &samples,
            const ColorRecognitionGmmBuildConfig &config) const;
    // 校验 Base64/size/hash，并通过 HALCON 反序列化确认 GMM 产物可读。
    ColorRecognitionGmmArtifactValidationResult validateGmmModelArtifact(
            const ColorRecognitionGmmArtifact &artifact,
            const ColorRecognitionGmmBuildConfig &config) const;
    // 使用保存的 HALCON GMM 对检测区域逐像素分类，并完成生产判定。
    ColorRecognitionGmmRunResult runGmmModel(
            const cv::Mat &image,
            const ColorRecognitionGmmRunConfig &config) const;
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
