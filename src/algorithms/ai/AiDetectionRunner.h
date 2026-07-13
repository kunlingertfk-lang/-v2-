#ifndef ALGORITHMS_AI_AIDETECTIONRUNNER_H
#define ALGORITHMS_AI_AIDETECTIONRUNNER_H

#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>
#include <opencv2/core.hpp>

struct AiDetectionDetection
{
    QString className;
    double score = 0.0;
    QRectF bboxPixel;
    QRectF bboxNormalized;
};

struct AiDetectionConfig
{
    QString remoteUser = QStringLiteral("cat");
    QString remoteHost = QStringLiteral("192.168.31.88");
    QString modelName;
    QString modelPathOnRK = QStringLiteral("/home/cat/model/yolov8_red_black.rknn");
    QString labelPathOnRK = QStringLiteral("/home/cat/红黑线标签.txt");
    QString scriptPathOnRK = QStringLiteral("/home/cat/qt-AI/scripts/run_rknn_demo.sh");
    QString sourceRootOnRK = QStringLiteral("/home/cat/YOLOv8_RK3588_object_detect-main");
    QString remoteInputDir = QStringLiteral("/tmp/v2_ai_input");
    QString remoteOutputDir = QStringLiteral("/tmp/v2_ai_output");
    QString remoteBuildDir = QStringLiteral("/tmp/v2_ai_build");
    QString localTempRoot = QStringLiteral("/tmp/v2_ai_bridge");
    int classCount = 2;
    double boxThreshold = 0.02;
    double nmsIouThreshold = 0.5;
    int maxDetections = 0;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString detectRegionType = QStringLiteral("rectangle");
    bool classFilterEnabled = false;
    QString classFilterText;
    bool angleFilterEnabled = false;
    double minAngle = -180.0;
    double maxAngle = 180.0;
    bool widthFilterEnabled = false;
    double minWidth = 0.0;
    double maxWidth = 999999.0;
    bool heightFilterEnabled = false;
    double minHeight = 0.0;
    double maxHeight = 999999.0;
    bool boundaryFilterEnabled = false;
    double boundaryOverlapRatio = 0.0;
    QString sortMode;
    QString judgeMode = QStringLiteral("count");
    QString resultBasis;
    int minCount = 0;
    int maxCount = 999999;
    double minScore = 0.0;
    QString category;
    bool showBoxes = true;
    bool showLabels = true;
    bool showScores = true;
    int timeoutMs = 120000;
    bool allowLegacyScriptFallback = true;
    QJsonObject paramsSnapshot;
    QJsonObject judgeRuleSnapshot;
    QStringList warnings;
};

struct AiDetectionJudgeDecision
{
    QString mode;
    bool ok = false;
    QString status;
    QString message;
};

struct AiDetectionFilterResult
{
    QVector<AiDetectionDetection> detections;
    QStringList warnings;
};

struct AiDetectionRunnerResult
{
    bool success = false;
    bool ok = false;
    QString status;
    QString message;
    QString text;
    double score = 0.0;
    int count = 0;
    qint64 elapsedMs = 0;
    QVector<AiDetectionDetection> rawDetections;
    QVector<AiDetectionDetection> detections;
    QStringList rawDetectionLines;
    QString stdoutText;
    QString stderrText;
    QString remoteResultImage;
    QString localResultImage;
    QString localInputImage;
    QString remoteInputImage;
    QString scriptArgumentMode;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class AiDetectionRunner
{
public:
    AiDetectionRunnerResult run(const cv::Mat &image, const AiDetectionConfig &config) const;

    static bool parseDetectionLine(const QString &line,
                                   AiDetectionDetection *detection,
                                   QString *errorMessage = nullptr);
    static QVector<AiDetectionDetection> parseDetectionLines(const QStringList &lines,
                                                             const QSize &uploadedImageSize,
                                                             const QSize &originalImageSize,
                                                             const QPointF &imageOffset);
    static AiDetectionFilterResult applyFilters(const QVector<AiDetectionDetection> &detections,
                                                const AiDetectionConfig &config);
    static AiDetectionJudgeDecision judgeDetections(const QVector<AiDetectionDetection> &detections,
                                                    const AiDetectionConfig &config);
};

#endif // ALGORITHMS_AI_AIDETECTIONRUNNER_H
