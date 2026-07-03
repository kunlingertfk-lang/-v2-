#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONHALCONRUNNER_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONHALCONRUNNER_H

#include "toolcore/ToolOverlay.h"
#include "toolcore/PositionCorrection.h"

#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

// 注册分类 HALCON 推理配置。字段对齐 docs/FID/RegisteredClassification/
// registered_classification_function_implementation.md 的配置字段表。
struct RegisteredClassificationHalconConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString modelPath;
    QString modelName;
    QString modelType = QStringLiteral("halcon_dl_classification");
    QString detectRegionType = QStringLiteral("full"); // full | rectangle
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    PositionCorrectionConfig positionCorrection;
    int topK = 1;
    QString judgeMode = QStringLiteral("class_match"); // class_match | min_score
    QString expectedLabel;
    int minScore = 80;
};

// 单个候选类别结果。
struct RegisteredClassificationClassScore
{
    QString label;
    int classId = -1;
    double score = 0.0; // 0-100
};

struct RegisteredClassificationHalconResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    QString predictedLabel;
    int predictedClassId = -1;
    double score = 0.0; // 0-100
    QVector<RegisteredClassificationClassScore> topClasses;
    qint64 elapsedMs = 0;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class RegisteredClassificationHalconRunner
{
public:
    RegisteredClassificationHalconResult run(
            const cv::Mat &image,
            const RegisteredClassificationHalconConfig &config) const;
};

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONHALCONRUNNER_H
