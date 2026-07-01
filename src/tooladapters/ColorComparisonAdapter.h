#ifndef TOOLADAPTERS_COLORCOMPARISONADAPTER_H
#define TOOLADAPTERS_COLORCOMPARISONADAPTER_H

#include "algorithms/recognition/ColorComparisonHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class ColorComparisonAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    ColorComparisonHalconRunner m_runner;
};

#endif // TOOLADAPTERS_COLORCOMPARISONADAPTER_H
