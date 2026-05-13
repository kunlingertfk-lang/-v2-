#ifndef TOOLADAPTERS_BLOBPRESENCEADAPTER_H
#define TOOLADAPTERS_BLOBPRESENCEADAPTER_H

#include "algorithms/presence/BlobPresenceHalconRunner.h"
#include "toolcore/ToolAdapter.h"

class BlobPresenceAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override;
    ToolResult run(const ToolRequest &request) override;

private:
    BlobPresenceHalconRunner m_runner;
};

#endif // TOOLADAPTERS_BLOBPRESENCEADAPTER_H
