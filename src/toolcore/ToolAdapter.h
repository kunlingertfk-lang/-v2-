#ifndef TOOLCORE_TOOLADAPTER_H
#define TOOLCORE_TOOLADAPTER_H

#include "ToolRequest.h"
#include "ToolResult.h"

class ToolAdapter {
public:
    virtual ~ToolAdapter() = default;

    virtual bool supports(ToolType type) const = 0;
    virtual ToolResult run(const ToolRequest &request) = 0;
};

#endif // TOOLCORE_TOOLADAPTER_H
