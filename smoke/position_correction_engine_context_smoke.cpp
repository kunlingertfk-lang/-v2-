#include "toolcore/ToolAdapter.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/PositionCorrection.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <iostream>

#include <opencv2/imgproc.hpp>

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    }
}

ToolConfig tool(const QString &id, ToolType type, ToolCategory category)
{
    ToolConfig config;
    config.toolId = id;
    config.toolName = id;
    config.displayName = id;
    config.toolType = type;
    config.category = category;
    config.enabled = true;
    return config;
}

class ContextEchoAdapter final : public ToolAdapter
{
public:
    bool supports(ToolType type) const override
    {
        return type == ToolType::TemplateLocation
                || type == ToolType::PositionCorrection
                || type == ToolType::ColorComparison;
    }

    ToolResult run(const ToolRequest &request) override
    {
        ToolResult result;
        result.toolId = request.config.toolId;
        result.toolType = request.config.toolType;
        result.success = true;
        result.ok = true;
        result.status = QStringLiteral("ok");
        result.payload.insert(QStringLiteral("seenToolResults"),
                              request.runtimeContext.value(
                                  QStringLiteral("toolResultsById")).toObject().size());
        result.payload.insert(QStringLiteral("seenPositionCorrections"),
                              request.runtimeContext.value(
                                  QStringLiteral("positionCorrectionsById")).toObject().size());
        if (request.config.toolType == ToolType::PositionCorrection) {
            result.payload.insert(QStringLiteral("positionCorrectionApplied"), true);
            result.payload.insert(QStringLiteral("sourceId"), request.config.toolId);
        }
        return result;
    }
};

class FailedCorrectionAdapter final : public ToolAdapter
{
public:
    bool supports(ToolType type) const override
    {
        return type == ToolType::PositionCorrection
                || type == ToolType::ColorComparison;
    }

    ToolResult run(const ToolRequest &request) override
    {
        ToolResult result;
        result.toolId = request.config.toolId;
        result.toolType = request.config.toolType;
        if (request.config.toolType == ToolType::PositionCorrection) {
            result.success = true;
            result.ok = false;
            result.status = QStringLiteral("not_found");
            result.payload.insert(QStringLiteral("sourceId"), request.config.toolId);
            result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
            return result;
        }
        result.success = true;
        result.ok = true;
        result.status = QStringLiteral("ok");
        result.payload.insert(QStringLiteral("seenPositionCorrections"),
                              request.runtimeContext.value(
                                  QStringLiteral("positionCorrectionsById")).toObject().size());
        return result;
    }
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    ContextEchoAdapter adapter;
    ToolEngine engine;
    engine.registerAdapter(&adapter);

    QVector<ToolConfig> configs{
        tool(QStringLiteral("template"), ToolType::TemplateLocation, ToolCategory::Location),
        tool(QStringLiteral("pc"), ToolType::PositionCorrection, ToolCategory::Location),
        tool(QStringLiteral("consumer"), ToolType::ColorComparison, ToolCategory::Recognition)
    };
    const QVector<ToolResult> results = engine.runTools(configs, cv::Mat());
    check(results.size() == 3, "engine must run all enabled tools");
    check(results.at(0).payload.value(QStringLiteral("seenToolResults")).toInt(-1) == 0,
          "first tool must see no previous result");
    check(results.at(1).payload.value(QStringLiteral("seenToolResults")).toInt(-1) == 1,
          "second tool must see first result");
    check(results.at(2).payload.value(QStringLiteral("seenToolResults")).toInt(-1) == 2,
          "third tool must see two previous results");
    check(results.at(2).payload.value(QStringLiteral("seenPositionCorrections")).toInt(-1) == 1,
          "consumer must see preceding applied position correction");

    ToolResult staleCorrection;
    staleCorrection.toolId = QStringLiteral("stale-pc");
    staleCorrection.toolType = ToolType::PositionCorrection;
    staleCorrection.success = true;
    staleCorrection.ok = true;
    staleCorrection.payload.insert(QStringLiteral("sourceId"),
                                   QStringLiteral("stale-pc"));
    staleCorrection.payload.insert(QStringLiteral("positionCorrectionApplied"), true);
    staleCorrection.payload.insert(QStringLiteral("frameId"),
                                   QStringLiteral("frame-old"));
    QJsonObject staleContext{
        {QStringLiteral("frameId"), QStringLiteral("frame-new")},
        {QStringLiteral("toolResultsById"),
         QJsonObject{{QStringLiteral("stale-pc"), staleCorrection.toJson()}}},
        {QStringLiteral("positionCorrectionsById"),
         QJsonObject{{QStringLiteral("stale-pc"), staleCorrection.toJson()}}}
    };
    const QVector<ToolResult> isolatedResults = engine.runTools(
                QVector<ToolConfig>{
                    tool(QStringLiteral("current"), ToolType::ColorComparison,
                         ToolCategory::Recognition)
                },
                cv::Mat(),
                cv::Mat(),
                staleContext);
    check(isolatedResults.size() == 1
          && isolatedResults.first().payload.value(
              QStringLiteral("seenToolResults")).toInt(-1) == 0
          && isolatedResults.first().payload.value(
              QStringLiteral("seenPositionCorrections")).toInt(-1) == 0,
          "engine must discard caller-provided dynamic results at frame start");
    check(isolatedResults.size() == 1
          && isolatedResults.first().payload.value(
              QStringLiteral("frameId")).toString()
          == QStringLiteral("frame-new"),
          "engine must stamp current frameId onto each result");

    FailedCorrectionAdapter failedAdapter;
    ToolEngine failedEngine;
    failedEngine.registerAdapter(&failedAdapter);
    const QVector<ToolResult> failedResults = failedEngine.runTools(
                QVector<ToolConfig>{
                    tool(QStringLiteral("failed-pc"), ToolType::PositionCorrection,
                         ToolCategory::Location),
                    tool(QStringLiteral("after-failed-pc"), ToolType::ColorComparison,
                         ToolCategory::Recognition)
                },
                cv::Mat());
    check(failedResults.size() == 2
          && failedResults.at(1).payload.value(
              QStringLiteral("seenPositionCorrections")).toInt(-1) == 1,
          "consumer must see a correction source that ran but failed");

    cv::Mat image(240, 320, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::rectangle(image, cv::Rect(90, 70, 80, 50), cv::Scalar(255, 255, 255), cv::FILLED);
    cv::line(image, cv::Point(90, 70), cv::Point(170, 120), cv::Scalar(0, 0, 0), 3);
    ReferencePositionCorrectionConfig referenceConfig;
    referenceConfig.enabled = true;
    referenceConfig.templateRegionType = QStringLiteral("rectangle");
    referenceConfig.templateRoiNormalized = QRectF(80.0 / 320.0, 60.0 / 240.0,
                                                   110.0 / 320.0, 80.0 / 240.0);
    referenceConfig.templateMaskRegionType = QStringLiteral("rectangle");
    referenceConfig.templateMaskRoiNormalized =
            QRectF(100.0 / 320.0, 78.0 / 240.0,
                   20.0 / 320.0, 16.0 / 240.0);
    referenceConfig.originMode = QStringLiteral("custom");
    referenceConfig.customOriginNormalized = QPointF(0.82, 0.16);
    referenceConfig.referenceCreated = true;
    referenceConfig.referencePose = QJsonObject{
        {QStringLiteral("x"), 0.82 * image.cols},
        {QStringLiteral("y"), 0.16 * image.rows},
        {QStringLiteral("angleDeg"), 0.0}
    };
    referenceConfig.modelCacheKey =
            QStringLiteral("reference.positionCorrection.private_template.engine_smoke");
    QJsonObject context;
    context.insert(QStringLiteral("frameId"),
                   QStringLiteral("reference-frame"));
    context.insert(QStringLiteral("referencePositionCorrection"),
                   PositionCorrection::referenceToJson(referenceConfig));
    ToolResult referenceCorrectionResult;
    const QVector<ToolResult> referenceResults = engine.runTools(
                QVector<ToolConfig>{
                    tool(QStringLiteral("after-reference"), ToolType::ColorComparison,
                         ToolCategory::Recognition)
                },
                image,
                image,
                context,
                &referenceCorrectionResult);
    check(referenceResults.size() == 1,
          "engine must still run configured tools after reference correction prelude");
    check(referenceResults.at(0).payload.value(QStringLiteral("seenPositionCorrections"))
          .toInt(-1) == 1,
          "first real tool must see the registered reference position correction");
    check(referenceResults.at(0).payload.value(QStringLiteral("seenToolResults")).toInt(-1) == 1,
          "first real tool must see the reference correction result in toolResultsById");
    bool hasReferenceOrigin = false;
    for (const ToolOverlay &overlay : referenceCorrectionResult.overlays) {
        if (overlay.extra.value(QStringLiteral("role")).toString()
                == QStringLiteral("position_correction_match_origin")) {
            hasReferenceOrigin = true;
        }
    }
    check(referenceCorrectionResult.success && referenceCorrectionResult.ok,
          "engine must expose the successful reference correction prelude");
    check(referenceCorrectionResult.payload.value(QStringLiteral("frameId")).toString()
          == QStringLiteral("reference-frame"),
          "engine must stamp frameId onto the reference correction prelude");
    check(referenceCorrectionResult.payload.value(
              QStringLiteral("locatorPayload")).toObject().value(
              QStringLiteral("templateMaskRegionType")).toString() ==
              QStringLiteral("rectangle"),
          "reference runtime locator must consume the saved template mask geometry");
    check(hasReferenceOrigin,
          "reference correction prelude must expose its actual match origin");

    ReferencePositionCorrectionConfig invalidPoseConfig = referenceConfig;
    invalidPoseConfig.referencePose.insert(
                QStringLiteral("x"), QStringLiteral("not-a-number"));
    QJsonObject invalidPoseContext;
    invalidPoseContext.insert(
                QStringLiteral("frameId"), QStringLiteral("invalid-pose-frame"));
    invalidPoseContext.insert(
                QStringLiteral("referencePositionCorrection"),
                PositionCorrection::referenceToJson(invalidPoseConfig));
    ToolResult invalidPoseResult;
    engine.runTools(
                QVector<ToolConfig>(),
                image,
                image,
                invalidPoseContext,
                &invalidPoseResult);
    check(!invalidPoseResult.success
          && invalidPoseResult.status == QStringLiteral("invalid_reference_pose"),
          "reference correction must reject non-numeric reference pose fields");

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "position_correction_engine_context_smoke: all checks passed" << std::endl;
    return 0;
}
