#ifndef TOOLADAPTERS_TEMPLATELOCATIONADAPTER_H
#define TOOLADAPTERS_TEMPLATELOCATIONADAPTER_H

#include "algorithms/location/TemplateLocationHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class TemplateLocationAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    TemplateLocationHalconRunner m_runner;
};

#endif // TOOLADAPTERS_TEMPLATELOCATIONADAPTER_H
