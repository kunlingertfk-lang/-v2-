#include "tooladapters/AiDetectionAdapter.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QtGlobal>
#include <exception>

namespace {

QString firstString(const QJsonObject &json,
                    const QStringList &keys,
                    const QString &defaultValue = QString())
{
    for (const QString &key : keys) {
        const QJsonValue value = json.value(key);
        if (value.isUndefined() || value.isNull())
            continue;

        const QString text = value.toString().trimmed();
        if (!text.isEmpty())
            return text;
    }
    return defaultValue;
}

double firstDouble(const QJsonObject &json,
                   const QStringList &keys,
                   const double defaultValue)
{
    for (const QString &key : keys) {
        const QJsonValue value = json.value(key);
        if (value.isUndefined() || value.isNull())
            continue;
        if (value.isDouble())
            return value.toDouble();

        bool ok = false;
        const double parsed = value.toString().trimmed().toDouble(&ok);
        if (ok)
            return parsed;
    }
    return defaultValue;
}

int firstInt(const QJsonObject &json,
             const QStringList &keys,
             const int defaultValue)
{
    return static_cast<int>(firstDouble(json, keys, defaultValue));
}

bool firstBool(const QJsonObject &json,
               const QStringList &keys,
               const bool defaultValue)
{
    for (const QString &key : keys) {
        const QJsonValue value = json.value(key);
        if (value.isUndefined() || value.isNull())
            continue;
        if (value.isBool())
            return value.toBool(defaultValue);

        const QString text = value.toString().trimmed().toLower();
        if (text == QStringLiteral("true") ||
            text == QStringLiteral("1") ||
            text == QStringLiteral("yes") ||
            text == QStringLiteral("on"))
            return true;
        if (text == QStringLiteral("false") ||
            text == QStringLiteral("0") ||
            text == QStringLiteral("no") ||
            text == QStringLiteral("off"))
            return false;
    }
    return defaultValue;
}

double normalizeScoreOrRatio(const double rawValue)
{
    if (rawValue > 1.0)
        return qBound(0.0, rawValue / 100.0, 1.0);
    return qBound(0.0, rawValue, 1.0);
}

QString envString(const char *name, const QString &defaultValue)
{
    const QString value = QString::fromLocal8Bit(qgetenv(name)).trimmed();
    return value.isEmpty() ? defaultValue : value;
}

bool envBool(const char *name, const bool defaultValue)
{
    const QString value = QString::fromLocal8Bit(qgetenv(name)).trimmed().toLower();
    if (value.isEmpty())
        return defaultValue;
    return !(value == QStringLiteral("0") ||
             value == QStringLiteral("false") ||
             value == QStringLiteral("no") ||
             value == QStringLiteral("off"));
}

bool isKnownRedBlackModelName(const QString &modelName)
{
    const QString trimmed = modelName.trimmed();
    if (trimmed.isEmpty())
        return true;

    const QString fileName = QFileInfo(trimmed).fileName().toLower();
    return fileName == QStringLiteral("yolov8_red_black")
            || fileName == QStringLiteral("yolov8_red_black.rknn")
            || fileName == QStringLiteral("red_black")
            || fileName == QStringLiteral("red_black.rknn")
            || trimmed == QStringLiteral("红黑线")
            || trimmed == QStringLiteral("红黑线检测");
}

AiDetectionConfig toRunnerConfig(const ToolConfig &config)
{
    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;

    AiDetectionConfig runnerConfig;
    runnerConfig.paramsSnapshot = params;
    runnerConfig.judgeRuleSnapshot = judgeRule;
    runnerConfig.remoteHost = envString("V2_AI_REMOTE_HOST", runnerConfig.remoteHost);
    runnerConfig.remoteUser = envString("V2_AI_REMOTE_USER", runnerConfig.remoteUser);
    runnerConfig.allowLegacyScriptFallback = envBool("V2_AI_ALLOW_LEGACY_RK_SCRIPT", true);

    runnerConfig.modelName = firstString(params, {QStringLiteral("modelName")});
    if (runnerConfig.modelName.trimmed().isEmpty()) {
        runnerConfig.warnings << QStringLiteral("modelName 为空，桥接版使用默认 yolov8_red_black.rknn。");
    }
    runnerConfig.modelPathOnRK = QStringLiteral("/home/cat/model/yolov8_red_black.rknn");
    runnerConfig.labelPathOnRK = QStringLiteral("/home/cat/红黑线标签.txt");
    runnerConfig.classCount = 2;

    runnerConfig.roiNormalized = config.roiNormalized;
    runnerConfig.detectRegionType = firstString(params,
                                                {QStringLiteral("detectRegionType")},
                                                runnerConfig.detectRegionType);

    runnerConfig.boxThreshold = normalizeScoreOrRatio(firstDouble(params,
                                                                  {QStringLiteral("detectMinScore"),
                                                                   QStringLiteral("confidenceThreshold")},
                                                                  0.02));
    runnerConfig.nmsIouThreshold = normalizeScoreOrRatio(firstDouble(params,
                                                                     {QStringLiteral("maxOverlap"),
                                                                      QStringLiteral("nmsIouThreshold")},
                                                                     0.5));
    runnerConfig.maxDetections = qMax(0,
                                      firstInt(params,
                                               {QStringLiteral("maxDetections"),
                                                QStringLiteral("maxFindCount")},
                                               0));
    runnerConfig.sortMode = firstString(params, {QStringLiteral("sortMode")});

    runnerConfig.classFilterEnabled = firstBool(params,
                                                {QStringLiteral("classFilterEnabled")},
                                                runnerConfig.classFilterEnabled);
    runnerConfig.classFilterText = firstString(params,
                                               {QStringLiteral("classFilterText"),
                                                QStringLiteral("classFilter")});

    runnerConfig.angleFilterEnabled = firstBool(params,
                                                {QStringLiteral("angleFilterEnabled")},
                                                runnerConfig.angleFilterEnabled);
    runnerConfig.minAngle = firstDouble(params, {QStringLiteral("minAngle")}, runnerConfig.minAngle);
    runnerConfig.maxAngle = firstDouble(params, {QStringLiteral("maxAngle")}, runnerConfig.maxAngle);

    runnerConfig.widthFilterEnabled = firstBool(params,
                                                {QStringLiteral("widthFilterEnabled")},
                                                runnerConfig.widthFilterEnabled);
    runnerConfig.minWidth = firstDouble(params, {QStringLiteral("minWidth")}, runnerConfig.minWidth);
    runnerConfig.maxWidth = firstDouble(params, {QStringLiteral("maxWidth")}, runnerConfig.maxWidth);

    runnerConfig.heightFilterEnabled = firstBool(params,
                                                 {QStringLiteral("heightFilterEnabled")},
                                                 runnerConfig.heightFilterEnabled);
    runnerConfig.minHeight = firstDouble(params, {QStringLiteral("minHeight")}, runnerConfig.minHeight);
    runnerConfig.maxHeight = firstDouble(params, {QStringLiteral("maxHeight")}, runnerConfig.maxHeight);

    runnerConfig.boundaryFilterEnabled = firstBool(params,
                                                   {QStringLiteral("boundaryFilterEnabled")},
                                                   runnerConfig.boundaryFilterEnabled);
    runnerConfig.boundaryOverlapRatio = normalizeScoreOrRatio(firstDouble(params,
                                                                          {QStringLiteral("boundaryOverlapRatio"),
                                                                           QStringLiteral("overlapRatio")},
                                                                          runnerConfig.boundaryOverlapRatio));

    runnerConfig.showBoxes = firstBool(params, {QStringLiteral("showBoxes")}, runnerConfig.showBoxes);
    runnerConfig.showLabels = firstBool(params, {QStringLiteral("showLabels")}, runnerConfig.showLabels);
    runnerConfig.showScores = firstBool(params, {QStringLiteral("showScores")}, runnerConfig.showScores);

    runnerConfig.judgeMode = firstString(judgeRule, {QStringLiteral("mode")}, runnerConfig.judgeMode);
    runnerConfig.resultBasis = firstString(judgeRule, {QStringLiteral("resultBasis")});
    runnerConfig.minCount = firstInt(judgeRule, {QStringLiteral("minCount"), QStringLiteral("min_count")}, 0);
    runnerConfig.maxCount = firstInt(judgeRule, {QStringLiteral("maxCount"), QStringLiteral("max_count")}, 999999);
    runnerConfig.minScore = normalizeScoreOrRatio(firstDouble(judgeRule,
                                                             {QStringLiteral("minScore"),
                                                              QStringLiteral("min_score")},
                                                             0.0));
    runnerConfig.category = firstString(judgeRule, {QStringLiteral("category")});
    runnerConfig.timeoutMs = qMax(1000,
                                  firstInt(params,
                                           {QStringLiteral("timeoutMs"),
                                            QStringLiteral("timeout_ms")},
                                           runnerConfig.timeoutMs));

    return runnerConfig;
}

ToolResult makeAiDetectionError(const ToolConfig &config,
                                const QString &status,
                                const QString &message)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::AiDetection;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    return result;
}

} // namespace

bool AiDetectionAdapter::supports(ToolType type) const
{
    return type == ToolType::AiDetection;
}

ToolResult AiDetectionAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::AiDetection) {
        return makeAiDetectionError(config,
                                    QStringLiteral("invalid_tool_type"),
                                    QStringLiteral("AiDetectionAdapter only supports ToolType::AiDetection."));
    }
    const QString modelName =
            firstString(config.params, {QStringLiteral("modelName")});
    if (!isKnownRedBlackModelName(modelName)) {
        return makeAiDetectionError(
                    config,
                    QStringLiteral("unsupported_model"),
                    QStringLiteral("当前 RK 桥接不支持模型 %1，未回退到默认模型。")
                    .arg(modelName));
    }
    const double nmsThreshold =
            normalizeScoreOrRatio(firstDouble(
                                      config.params,
                                      {QStringLiteral("maxOverlap"),
                                       QStringLiteral("nmsIouThreshold")},
                                      0.5));
    if (qAbs(nmsThreshold - 0.5) > 0.000001) {
        return makeAiDetectionError(
                    config,
                    QStringLiteral("unsupported_nms_threshold"),
                    QStringLiteral("当前 RK 脚本仅支持固定 NMS 阈值 0.5，配置值 %1 未被静默忽略。")
                    .arg(nmsThreshold, 0, 'f', 3));
    }

    AiDetectionRunnerResult runnerResult;
    try {
        runnerResult = m_runner.run(request.image, toRunnerConfig(config));
    } catch (const std::exception &error) {
        runnerResult.success = false;
        runnerResult.ok = false;
        runnerResult.status = QStringLiteral("ai_detection_exception");
        runnerResult.message = QStringLiteral("目标检测远程推理失败：%1")
                .arg(QString::fromLocal8Bit(error.what()));
        runnerResult.payload.insert(QStringLiteral("adapterCaughtException"), true);
    } catch (...) {
        runnerResult.success = false;
        runnerResult.ok = false;
        runnerResult.status = QStringLiteral("ai_detection_exception");
        runnerResult.message = QStringLiteral("目标检测远程推理失败：unknown exception");
        runnerResult.payload.insert(QStringLiteral("adapterCaughtException"), true);
    }

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::AiDetection;
    result.success = runnerResult.success;
    result.ok = runnerResult.ok;
    result.status = runnerResult.status;
    result.message = runnerResult.message;
    result.text = runnerResult.text;
    result.score = runnerResult.score;
    result.value = runnerResult.count;
    result.count = runnerResult.count;
    result.elapsedMs = runnerResult.elapsedMs;
    result.overlays = runnerResult.overlays;
    result.payload = runnerResult.payload;
    return result;
}
