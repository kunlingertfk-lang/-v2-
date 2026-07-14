#include "ToolEngine.h"

#include <QElapsedTimer>

void ToolEngine::registerAdapter(ToolAdapter *adapter)
{
    if (!adapter || m_adapters.contains(adapter))
        return;

    m_adapters.append(adapter);
}

ToolResult ToolEngine::runTool(const ToolRequest &request) const
{
    const ToolConfig &config = request.config;
    if (!config.enabled) {
        return ToolResult::error(config.toolId,
                                 config.toolType,
                                 QStringLiteral("Tool is disabled."),
                                 QStringLiteral("disabled"));
    }

    if (!config.isValid()) {
        return ToolResult::error(config.toolId,
                                 config.toolType,
                                 QStringLiteral("Invalid ToolConfig."),
                                 QStringLiteral("invalid_config"));
    }

    ToolAdapter *adapter = findAdapter(config.toolType);
    if (!adapter)
        return ToolResult::unsupported(config.toolId, config.toolType);

    QElapsedTimer timer;
    timer.start();
    ToolResult result = adapter->run(request);
    if (result.toolId.isEmpty())
        result.toolId = config.toolId;
    if (result.toolType == ToolType::Unknown)
        result.toolType = config.toolType;
    if (result.elapsedMs <= 0)
        result.elapsedMs = timer.elapsed();
    return result;
}

QVector<ToolResult> ToolEngine::runTools(const QVector<ToolConfig> &configs,
                                         const cv::Mat &image,
                                         const cv::Mat &referenceImage,
                                         const QJsonObject &runtimeContext) const
{
    QVector<ToolResult> results;
    results.reserve(configs.size());

    for (const ToolConfig &config : configs) {
        if (!config.enabled)
            continue;

        ToolRequest request;
        request.config = config;
        request.image = image;
        request.referenceImage = referenceImage;
        request.runtimeContext = runtimeContext;
        results.append(runTool(request));
    }

    return results;
}

ToolAdapter *ToolEngine::findAdapter(ToolType type) const
{
    for (ToolAdapter *adapter : m_adapters) {
        if (adapter && adapter->supports(type))
            return adapter;
    }

    return nullptr;
}
