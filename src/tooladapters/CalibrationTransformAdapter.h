#ifndef TOOLADAPTERS_CALIBRATIONTRANSFORMADAPTER_H
#define TOOLADAPTERS_CALIBRATIONTRANSFORMADAPTER_H

#include "algorithms/location/CalibrationTransformHalconRunner.h"
#include "toolcore/ToolAdapter.h"

/// 标定转换适配器：解析 ToolConfig/同帧订阅，校验标定来源后调用 HALCON Runner。
class CalibrationTransformAdapter final : public ToolAdapter
{
public:
    /// 仅声明支持 ToolType::CalibrationTransform。
    bool supports(ToolType type) const override;
    /// 完成文件加载、输入与来源门禁、位姿解析、Runner 调用和 ToolResult 映射。
    ToolResult run(const ToolRequest &request) override;

private:
    CalibrationTransformHalconRunner m_runner;
};

#endif // TOOLADAPTERS_CALIBRATIONTRANSFORMADAPTER_H
