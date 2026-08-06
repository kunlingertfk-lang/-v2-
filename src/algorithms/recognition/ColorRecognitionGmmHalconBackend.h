#ifndef ALGORITHMS_RECOGNITION_COLORRECOGNITIONGMMHALCONBACKEND_H
#define ALGORITHMS_RECOGNITION_COLORRECOGNITIONGMMHALCONBACKEND_H

#include "algorithms/recognition/ColorRecognitionHalconRunner.h"

class ColorRecognitionGmmHalconBackend
{
public:
    ColorRecognitionGmmBuildResult buildModel(
            const QVector<ColorRecognitionGmmBuildSample> &samples,
            const ColorRecognitionGmmBuildConfig &config) const;
    ColorRecognitionGmmArtifactValidationResult validateArtifact(
            const ColorRecognitionGmmArtifact &artifact,
            const ColorRecognitionGmmBuildConfig &config) const;
    ColorRecognitionGmmRunResult runModel(
            const cv::Mat &image,
            const ColorRecognitionGmmRunConfig &config) const;
};

#endif // ALGORITHMS_RECOGNITION_COLORRECOGNITIONGMMHALCONBACKEND_H
