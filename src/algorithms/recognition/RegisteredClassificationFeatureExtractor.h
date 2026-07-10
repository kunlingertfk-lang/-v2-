#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATUREEXTRACTOR_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATUREEXTRACTOR_H

#include <QJsonObject>
#include <QPointF>
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

struct RegisteredClassificationFeatureRegion
{
    QString type = QStringLiteral("rectangle");
    QRectF rectNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QVector<QPointF> polygonNormalized;
};

struct RegisteredClassificationFeatureResult
{
    bool success = false;
    QString status;
    QString message;
    QVector<double> feature;
    QStringList featureNames;
    QRect roiPixels;
    QString foregroundPolarity;
    double foregroundAreaRatio = 0.0;
    double foregroundObjectScore = 0.0;
    QJsonObject payload;
};

class RegisteredClassificationFeatureExtractor
{
public:
    RegisteredClassificationFeatureResult extract(
            const cv::Mat &image,
            const QRectF &roiNormalized,
            const RegisteredClassificationFeatureConfig &config) const;
    RegisteredClassificationFeatureResult extractV2(
            const cv::Mat &image,
            const RegisteredClassificationFeatureRegion &region,
            const RegisteredClassificationFeatureConfig &config) const;
};

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATUREEXTRACTOR_H
