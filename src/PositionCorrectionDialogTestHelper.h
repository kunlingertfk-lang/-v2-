#ifndef POSITIONCORRECTIONDIALOGTESTHELPER_H
#define POSITIONCORRECTIONDIALOGTESTHELPER_H

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolResult.h"

#include <opencv2/core.hpp>

struct PositionCorrectionDialogTestContext
{
    QVector<ToolConfig> toolConfigs;
    int currentToolIndex = -1;
    ReferencePositionCorrectionConfig referencePositionCorrection;
    bool valid = false;
};

ToolResult runPositionCorrectionAwareDialogTest(
        const ToolEngine &engine,
        const ToolConfig &currentConfig,
        const cv::Mat &image,
        const cv::Mat &referenceImage,
        const PositionCorrectionDialogTestContext *testContext = nullptr);

#endif // POSITIONCORRECTIONDIALOGTESTHELPER_H
