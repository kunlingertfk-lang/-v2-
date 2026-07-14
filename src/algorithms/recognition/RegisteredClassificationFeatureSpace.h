#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATURESPACE_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATURESPACE_H

#include <QString>
#include <QStringList>
#include <QVector>

struct RegisteredClassificationRadiusStats
{
    bool enabled = false;
    double radius = 0.0;
    double meanDistance = 0.0;
    double stdDevDistance = 0.0;
    double maxDistance = 0.0;
};

QString registeredClassificationFeatureVersionV2();
QStringList registeredClassificationFeatureNamesV2();
QVector<int> registeredClassificationFeatureGroupDimensions();
QVector<double> registeredClassificationFeatureGroupWeights();
bool normalizeRegisteredClassificationFeature(QVector<double> *feature);
double registeredClassificationFeatureDistance(const QVector<double> &left,
                                               const QVector<double> &right);
double registeredClassificationSimilarityFromDistance(double distance);
QVector<double> registeredClassificationClassCenter(
        const QVector<QVector<double>> &features);
RegisteredClassificationRadiusStats registeredClassificationRadiusStats(
        const QVector<QVector<double>> &features,
        const QVector<double> &center);

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATURESPACE_H
