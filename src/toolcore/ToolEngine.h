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

    /// 注册工具类型适配器；重复或空指针会被忽略。
    void registerAdapter(ToolAdapter *adapter);
    /// 选择适配器运行一个工具，并补齐工具身份、帧号和耗时。
    ToolResult runTool(const ToolRequest &request) const;
    /// 按配置顺序运行同一帧工具链，并把每个结果注入后序工具可见的帧内上下文。
    QVector<ToolResult> runTools(const QVector<ToolConfig> &configs,
                                  const cv::Mat &image,
                                  const cv::Mat &referenceImage = cv::Mat(),
                                  const QJsonObject &runtimeContext = QJsonObject(),
                                  ToolResult *referenceCorrectionResult = nullptr) const;

private:
    /// 返回首个声明支持指定 ToolType 的已注册适配器。
    ToolAdapter *findAdapter(ToolType type) const;

    QVector<ToolAdapter *> m_adapters;
};

#endif // TOOLCORE_TOOLENGINE_H
