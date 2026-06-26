#ifndef TOOLADAPTERS_COLORRECOGNITIONADAPTER_H
#define TOOLADAPTERS_COLORRECOGNITIONADAPTER_H

#include "algorithms/recognition/ColorRecognitionHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class ColorRecognitionAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    ColorRecognitionHalconRunner m_runner;
};

#endif // TOOLADAPTERS_COLORRECOGNITIONADAPTER_H
