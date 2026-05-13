#ifndef TOOLADAPTERS_CIRCLEPRESENCEADAPTER_H
#define TOOLADAPTERS_CIRCLEPRESENCEADAPTER_H

#include "algorithms/presence/CirclePresenceHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class CirclePresenceAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    CirclePresenceHalconRunner m_runner;
};

#endif // TOOLADAPTERS_CIRCLEPRESENCEADAPTER_H
