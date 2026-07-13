#ifndef TOOLADAPTERS_COLORRECOGNITIONADAPTER_H
#define TOOLADAPTERS_COLORRECOGNITIONADAPTER_H

#include "algorithms/recognition/ColorRecognitionHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class ColorRecognitionAdapter : public ToolAdapter
{
public:
    // 声明本 Adapter 只接收颜色识别工具类型，供 ToolEngine 分发时匹配。
    bool supports(ToolType type) const override;
    // 将 ToolRequest 中的通用配置转换为 HALCON runner 配置，并把识别结果映射回 ToolResult。
    ToolResult run(const ToolRequest &request) override;

private:
    // 颜色识别的核心算法执行器，Adapter 只负责配置解析和结果桥接。
    ColorRecognitionHalconRunner m_runner;
};

#endif // TOOLADAPTERS_COLORRECOGNITIONADAPTER_H
