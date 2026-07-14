#ifndef TOOLADAPTERS_LINEPRESENCEADAPTER_H
#define TOOLADAPTERS_LINEPRESENCEADAPTER_H

#include "algorithms/presence/LinePresenceHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class LinePresenceAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    LinePresenceHalconRunner m_runner;
};

#endif // TOOLADAPTERS_LINEPRESENCEADAPTER_H
