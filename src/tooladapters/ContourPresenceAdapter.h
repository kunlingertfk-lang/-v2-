#ifndef TOOLADAPTERS_CONTOURPRESENCEADAPTER_H
#define TOOLADAPTERS_CONTOURPRESENCEADAPTER_H

#include "algorithms/presence/ContourPresenceHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class ContourPresenceAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    ContourPresenceHalconRunner m_runner;
};

#endif // TOOLADAPTERS_CONTOURPRESENCEADAPTER_H
