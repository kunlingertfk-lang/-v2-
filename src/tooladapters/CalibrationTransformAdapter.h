#ifndef TOOLADAPTERS_CALIBRATIONTRANSFORMADAPTER_H
#define TOOLADAPTERS_CALIBRATIONTRANSFORMADAPTER_H

#include "algorithms/location/CalibrationTransformHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class CalibrationTransformAdapter final : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    CalibrationTransformHalconRunner m_runner;
};

#endif // TOOLADAPTERS_CALIBRATIONTRANSFORMADAPTER_H
