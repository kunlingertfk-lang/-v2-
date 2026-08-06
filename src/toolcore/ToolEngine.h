#ifndef TOOLCORE_TOOLENGINE_H
#define TOOLCORE_TOOLENGINE_H

#include "ToolAdapter.h"
#include "ToolConfig.h"
#include "ToolRequest.h"
#include "ToolResult.h"

#include <QJsonObject>
#include <QVector>
#include <opencv2/core.hpp>

class ToolEngine {
public:
    ToolEngine() = default;

    void registerAdapter(ToolAdapter *adapter);
    ToolResult runTool(const ToolRequest &request) const;
    QVector<ToolResult> runTools(const QVector<ToolConfig> &configs,
                                  const cv::Mat &image,
                                  const cv::Mat &referenceImage = cv::Mat(),
                                  const QJsonObject &runtimeContext = QJsonObject(),
                                  ToolResult *referenceCorrectionResult = nullptr) const;

private:
    ToolAdapter *findAdapter(ToolType type) const;

    QVector<ToolAdapter *> m_adapters;
};

#endif // TOOLCORE_TOOLENGINE_H
