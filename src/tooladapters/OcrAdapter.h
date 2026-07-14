#ifndef TOOLADAPTERS_OCRADAPTER_H
#define TOOLADAPTERS_OCRADAPTER_H

#include "algorithms/ocr/OcrHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class OcrAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    OcrHalconRunner m_runner;
};

#endif // TOOLADAPTERS_OCRADAPTER_H
