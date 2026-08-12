#ifndef TOOLADAPTERS_TEMPLATELOCATIONADAPTER_H
#define TOOLADAPTERS_TEMPLATELOCATIONADAPTER_H

#include "algorithms/location/TemplateLocationHalconRunner.h"
#include "toolcore/ToolAdapter.h"

/// 模板定位适配器；普通工具链和快速标定 Capture 都通过它获取图像 X/Y/Angle。
class TemplateLocationAdapter : public ToolAdapter
{
public:
    /// 仅声明支持模板定位工具类型。
    bool supports(ToolType type) const override;
    /// 解析模板参数、调用 HALCON Runner，并补充标定来源所需的输出合同和签名。
    ToolResult run(const ToolRequest &request) override;

private:
    TemplateLocationHalconRunner m_runner;
};

#endif // TOOLADAPTERS_TEMPLATELOCATIONADAPTER_H
