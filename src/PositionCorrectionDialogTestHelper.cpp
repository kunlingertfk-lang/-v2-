#include "PositionCorrectionDialogTestHelper.h"

#include "SchemeStore.h"
#include "toolcore/PositionCorrection.h"

#include <QJsonObject>
#include <QUuid>

ToolResult runPositionCorrectionAwareDialogTest(
        const ToolEngine &engine,
        const ToolConfig &currentConfig,
        const cv::Mat &image,
        const cv::Mat &referenceImage,
        const PositionCorrectionDialogTestContext *testContext)
{
    const PositionCorrectionConfig correction =
            PositionCorrection::fromParams(currentConfig.params);
    if (!correction.enabled) {
        ToolRequest request;
        request.config = currentConfig;
        request.image = image;
        request.referenceImage = referenceImage;
        return engine.runTool(request);
    }

    const SchemeState &scheme = SchemeStore::instance().currentScheme();
    const QVector<ToolConfig> &availableConfigs =
            testContext && testContext->valid
            ? testContext->toolConfigs : scheme.toolConfigs;
    const ReferencePositionCorrectionConfig &referenceCorrection =
            testContext && testContext->valid
            ? testContext->referencePositionCorrection
            : scheme.referencePositionCorrection;
    QVector<ToolConfig> prefix;
    int currentIndex = testContext && testContext->valid
            ? qBound(0, testContext->currentToolIndex, availableConfigs.size())
            : availableConfigs.size();
    for (int index = 0; index < availableConfigs.size(); ++index) {
        if (availableConfigs.at(index).toolId == currentConfig.toolId) {
            currentIndex = index;
            break;
        }
    }
    for (int index = 0; index < currentIndex; ++index) {
        if (availableConfigs.at(index).enabled)
            prefix.append(availableConfigs.at(index));
    }

    ToolConfig testConfig = currentConfig;
    testConfig.enabled = true;
    prefix.append(testConfig);

    QJsonObject runtimeContext;
    runtimeContext.insert(
                QStringLiteral("referencePositionCorrection"),
                PositionCorrection::referenceToJson(
                    referenceCorrection));
    runtimeContext.insert(
                QStringLiteral("frameId"),
                QUuid::createUuid().toString(QUuid::WithoutBraces));

    ToolResult referenceCorrectionResult;
    const QVector<ToolResult> results =
            engine.runTools(prefix,
                            image,
                            referenceImage,
                            runtimeContext,
                            &referenceCorrectionResult);
    for (auto it = results.crbegin(); it != results.crend(); ++it) {
        if (it->toolId == testConfig.toolId)
            return *it;
    }

    ToolResult result;
    result.toolId = testConfig.toolId;
    result.toolType = testConfig.toolType;
    result.success = false;
    result.ok = false;
    result.status = QStringLiteral("tool_chain_result_missing");
    result.message = QStringLiteral("工具链没有返回当前工具结果");
    return result;
}
