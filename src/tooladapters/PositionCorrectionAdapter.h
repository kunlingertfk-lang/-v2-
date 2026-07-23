#ifndef TOOLADAPTERS_POSITIONCORRECTIONADAPTER_H
#define TOOLADAPTERS_POSITIONCORRECTIONADAPTER_H

#include "algorithms/location/PositionCorrectionHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class PositionCorrectionAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    PositionCorrectionHalconRunner m_runner;
};

#endif // TOOLADAPTERS_POSITIONCORRECTIONADAPTER_H
