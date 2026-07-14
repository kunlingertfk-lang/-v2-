#ifndef TOOLADAPTERS_EDGEPRESENCEADAPTER_H
#define TOOLADAPTERS_EDGEPRESENCEADAPTER_H

#include "algorithms/presence/EdgePresenceHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class EdgePresenceAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    EdgePresenceHalconRunner m_runner;
};

#endif // TOOLADAPTERS_EDGEPRESENCEADAPTER_H
