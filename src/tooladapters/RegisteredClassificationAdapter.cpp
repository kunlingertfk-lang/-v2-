#include "tooladapters/RegisteredClassificationAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/PositionCorrectionConsumer.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>
#include <QRectF>
#include <QtGlobal>

namespace {

QJsonObject registeredClassificationParams(const ToolConfig &config)
{
    const QJsonObject nested =
            config.params.value(QStringLiteral("registeredClassification")).toObject();
    return nested.isEmpty() ? config.params : nested;
}

QString stringParam(const QJsonObject &object,
                    const QString &key,
                    const QString &defaultValue = QString())
{
    const QString value = object.value(key).toString().trimmed();
    return value.isEmpty() ? defaultValue : value;
}

int intParam(const QJsonObject &object, const QString &key, int defaultValue)
{
    const QJsonValue value = object.value(key);
    if (value.isDouble())
        return value.toInt(defaultValue);
    bool ok = false;
    const int parsed = value.toString().trimmed().toInt(&ok);
    return ok ? parsed : defaultValue;
}

QRectF rectFromJson(const QJsonObject &json, const QRectF &fallback)
{
    if (json.isEmpty())
        return fallback;
    const double x = json.value(QStringLiteral("x")).toDouble(-1.0);
    const double y = json.value(QStringLiteral("y")).toDouble(-1.0);
    const double width = json.value(QStringLiteral("width")).toDouble(-1.0);
    const double height = json.value(QStringLiteral("height")).toDouble(-1.0);
    if (width <= 0.0 || height <= 0.0)
        return fallback;
    return QRectF(x, y, width, height);
}

RegisteredClassificationHalconConfig toRunnerConfig(const ToolConfig &config)
{
    const QJsonObject params = registeredClassificationParams(config);
    const QJsonObject judgeRule = config.judgeRule;

    RegisteredClassificationHalconConfig runnerConfig;
    runnerConfig.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                stringParam(params, QStringLiteral("halconSoPath")),
                &runnerConfig.halconSoPathCandidates);
    runnerConfig.modelPath = stringParam(params, QStringLiteral("modelPath"));
    runnerConfig.modelName = stringParam(params, QStringLiteral("modelName"));
    runnerConfig.modelType = stringParam(params,
                                         QStringLiteral("modelType"),
                                         registeredClassificationKnnModelType());
    runnerConfig.detectRegionType = stringParam(params,
                                                 QStringLiteral("detectRegionType"),
                                                 runnerConfig.detectRegionType);
    runnerConfig.roiNormalized = rectFromJson(
                params.value(QStringLiteral("roiNormalized")).toObject(),
                config.roiNormalized.width() > 0.0 && config.roiNormalized.height() > 0.0
                    ? config.roiNormalized
                    : runnerConfig.roiNormalized);
    const PositionCorrectionConfig correction =
            PositionCorrection::fromParams(params);
    runnerConfig.enablePositionCorrection = correction.enabled;
    runnerConfig.positionCorrectionSource = correction.source;
    runnerConfig.topK = qMax(1, intParam(params,
                                         QStringLiteral("topK"),
                                         runnerConfig.topK));
    runnerConfig.minSimilarity = qBound(
                0, intParam(params, QStringLiteral("minSimilarity"), 80), 100);
    runnerConfig.minMargin = qBound(
                0, intParam(params, QStringLiteral("minMargin"), 8), 100);
    runnerConfig.judgeMode = stringParam(judgeRule,
                                         QStringLiteral("mode"),
                                         runnerConfig.judgeMode);
    runnerConfig.expectedLabel = stringParam(judgeRule,
                                             QStringLiteral("expectedLabel"),
                                             runnerConfig.expectedLabel);
    runnerConfig.minScore = qBound(0,
                                   intParam(judgeRule,
                                            QStringLiteral("minScore"),
                                            runnerConfig.minScore),
                                   100);
    return runnerConfig;
}

ToolResult makeError(const ToolConfig &config,
                     const QString &status,
                     const QString &message)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::RegisteredClassification;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.text = message;
    return result;
}

} // namespace

bool RegisteredClassificationAdapter::supports(ToolType type) const
{
    return type == ToolType::RegisteredClassification;
}

ToolResult RegisteredClassificationAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::RegisteredClassification) {
        return makeError(
                    config,
                    QStringLiteral("invalid_tool_type"),
                    QStringLiteral("RegisteredClassificationAdapter only supports ToolType::RegisteredClassification."));
    }

    RegisteredClassificationHalconConfig runnerConfig =
            toRunnerConfig(config);
    const QJsonObject params = registeredClassificationParams(config);
    const PositionCorrectionConfig savedCorrection =
            PositionCorrection::fromParams(params);
    PositionCorrectionConsumerOptions correctionOptions;
    correctionOptions.requested = savedCorrection.enabled;
    correctionOptions.sourceId = savedCorrection.sourceId;
    correctionOptions.showMatchContour = params.value(
                QStringLiteral("showPositionCorrectionMatchContour"))
            .toBool(true);
    const PositionCorrectionResolveResult correction =
            PositionCorrectionConsumer::resolve(request, correctionOptions);
    if (!correction.success) {
        ToolResult result = makeError(
                    config, correction.status, correction.message);
        result.payload.insert(QStringLiteral("positionCorrectionRequested"),
                              correctionOptions.requested);
        result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
        result.payload.insert(QStringLiteral("positionCorrectionSource"),
                              savedCorrection.source);
        result.payload.insert(QStringLiteral("positionCorrectionSourceId"),
                              PositionCorrection::normalizedSourceId(
                                  correctionOptions.sourceId));
        result.payload.insert(QStringLiteral("positionCorrectionReason"),
                              correction.status);
        result.payload.insert(QStringLiteral("errorCode"), correction.status);
        result.payload.insert(QStringLiteral("errorMessage"), correction.message);
        return result;
    }
    runnerConfig.positionCorrection = correction.context;

    const RegisteredClassificationHalconResult runnerResult =
            m_runner.run(request.image, runnerConfig);

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::RegisteredClassification;
    result.success = runnerResult.success;
    result.ok = runnerResult.ok;
    result.status = runnerResult.status;
    result.message = runnerResult.message;
    result.score = runnerResult.score;
    result.text = runnerResult.predictedLabel;
    result.elapsedMs = runnerResult.elapsedMs;
    result.overlays = runnerResult.overlays;
    result.payload = runnerResult.payload;
    return result;
}
