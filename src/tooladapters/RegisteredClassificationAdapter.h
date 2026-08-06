#ifndef TOOLADAPTERS_REGISTEREDCLASSIFICATIONADAPTER_H
#define TOOLADAPTERS_REGISTEREDCLASSIFICATIONADAPTER_H

#include "algorithms/recognition/RegisteredClassificationHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class RegisteredClassificationAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    RegisteredClassificationHalconRunner m_runner;
};

#endif // TOOLADAPTERS_REGISTEREDCLASSIFICATIONADAPTER_H
