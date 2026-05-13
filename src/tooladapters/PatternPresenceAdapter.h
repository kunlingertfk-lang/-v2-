#ifndef TOOLADAPTERS_PATTERNPRESENCEADAPTER_H
#define TOOLADAPTERS_PATTERNPRESENCEADAPTER_H

#include "algorithms/presence/PatternPresenceHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class PatternPresenceAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    PatternPresenceHalconRunner m_runner;
};

#endif // TOOLADAPTERS_PATTERNPRESENCEADAPTER_H
