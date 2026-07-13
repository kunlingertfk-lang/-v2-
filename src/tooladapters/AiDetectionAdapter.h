#ifndef TOOLADAPTERS_AIDETECTIONADAPTER_H
#define TOOLADAPTERS_AIDETECTIONADAPTER_H

#include "algorithms/ai/AiDetectionRunner.h"
#include "toolcore/ToolAdapter.h"

class AiDetectionAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    AiDetectionRunner m_runner;
};

#endif // TOOLADAPTERS_AIDETECTIONADAPTER_H
