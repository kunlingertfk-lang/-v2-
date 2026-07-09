#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATUREEXTRACTOR_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATUREEXTRACTOR_H

#include <QJsonObject>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <opencv2/core.hpp>

struct RegisteredClassificationFeatureConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    bool normalizeGray = true;
    bool smooth = true;
};

struct RegisteredClassificationFeatureResult
{
    bool success = false;
    QString status;
    QString message;
    QVector<double> feature;
    QStringList featureNames;
    QRect roiPixels;
    QJsonObject payload;
};

class RegisteredClassificationFeatureExtractor
{
public:
    RegisteredClassificationFeatureResult extract(
            const cv::Mat &image,
            const QRectF &roiNormalized,
            const RegisteredClassificationFeatureConfig &config) const;
};

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATUREEXTRACTOR_H
