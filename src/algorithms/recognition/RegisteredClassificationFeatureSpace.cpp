#include "algorithms/recognition/RegisteredClassificationFeatureSpace.h"

#include <QtGlobal>

#include <cmath>
#include <limits>

namespace {

constexpr int kFeatureLength = 59;

bool isFiniteV2Feature(const QVector<double> &feature)
{
    if (feature.size() != kFeatureLength)
        return false;
    for (double value : feature) {
        if (!std::isfinite(value))
            return false;
    }
    return true;
}

} // namespace

QString registeredClassificationFeatureVersionV2()
{
    return QStringLiteral("halcon_registered_feature_v2");
}

QStringList registeredClassificationFeatureNamesV2()
{
    QStringList names = {
        QStringLiteral("shapeAspectShortLong"),
        QStringLiteral("shapeFillRatio"),
        QStringLiteral("shapeCircularity"),
        QStringLiteral("shapeCompactnessReciprocal"),
        QStringLiteral("shapeConvexity"),
        QStringLiteral("shapeRectangularity"),
        QStringLiteral("shapeAnisometryReciprocal"),
        QStringLiteral("shapeBulkiness"),
        QStringLiteral("shapeStructureFactor"),
        QStringLiteral("shapeMomentPsi1"),
        QStringLiteral("shapeMomentPsi2"),
        QStringLiteral("shapeMomentPsi3"),
        QStringLiteral("shapeMomentPsi4")
    };

    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            names.append(QStringLiteral("occupancyR%1C%2").arg(row).arg(column));
        }
    }

    names.append(QStringLiteral("grayMean"));
    names.append(QStringLiteral("grayDeviation"));
    for (int bin = 0; bin < 16; ++bin)
        names.append(QStringLiteral("grayHist%1").arg(bin, 2, 10, QLatin1Char('0')));

    names << QStringLiteral("labMeanL")
          << QStringLiteral("labMeanA")
          << QStringLiteral("labMeanB")
          << QStringLiteral("labDeviationL")
          << QStringLiteral("labDeviationA")
          << QStringLiteral("labDeviationB")
          << QStringLiteral("textureEntropy")
          << QStringLiteral("textureAnisotropy")
          << QStringLiteral("coocEnergy")
          << QStringLiteral("coocCorrelation")
          << QStringLiteral("coocHomogeneity")
          << QStringLiteral("coocContrast");
    return names;
}

QVector<int> registeredClassificationFeatureGroupDimensions()
{
    return {13, 16, 18, 6, 6};
}

QVector<double> registeredClassificationFeatureGroupWeights()
{
    return {0.35, 0.25, 0.15, 0.15, 0.10};
}

bool normalizeRegisteredClassificationFeature(QVector<double> *feature)
{
    if (!feature || !isFiniteV2Feature(*feature))
        return false;

    double squaredNorm = 0.0;
    for (double value : *feature)
        squaredNorm += value * value;
    if (!std::isfinite(squaredNorm) || squaredNorm <= 0.0)
        return false;

    const double norm = std::sqrt(squaredNorm);
    if (!std::isfinite(norm) || norm <= 0.0)
        return false;

    for (double &value : *feature)
        value /= norm;
    return true;
}

double registeredClassificationFeatureDistance(const QVector<double> &left,
                                               const QVector<double> &right)
{
    if (left.size() != right.size() || !isFiniteV2Feature(left) || !isFiniteV2Feature(right))
        return std::numeric_limits<double>::quiet_NaN();

    double squaredDistance = 0.0;
    for (int index = 0; index < left.size(); ++index) {
        const double difference = left.at(index) - right.at(index);
        squaredDistance += difference * difference;
    }
    return std::sqrt(squaredDistance);
}

double registeredClassificationSimilarityFromDistance(double distance)
{
    if (!std::isfinite(distance))
        return 0.0;
    return qBound(0.0, 1.0 - distance * distance / 2.0, 1.0);
}

QVector<double> registeredClassificationClassCenter(
        const QVector<QVector<double>> &features)
{
    if (features.isEmpty())
        return {};

    QVector<double> center(kFeatureLength, 0.0);
    for (const QVector<double> &feature : features) {
        if (!isFiniteV2Feature(feature))
            return {};
        for (int index = 0; index < kFeatureLength; ++index)
            center[index] += feature.at(index);
    }
    for (double &value : center)
        value /= static_cast<double>(features.size());

    return normalizeRegisteredClassificationFeature(&center) ? center : QVector<double>();
}

RegisteredClassificationRadiusStats registeredClassificationRadiusStats(
        const QVector<QVector<double>> &features,
        const QVector<double> &center)
{
    RegisteredClassificationRadiusStats stats;
    if (features.size() < 3 || !isFiniteV2Feature(center))
        return stats;

    QVector<double> distances;
    distances.reserve(features.size());
    double total = 0.0;
    double maxDistance = 0.0;
    for (const QVector<double> &feature : features) {
        const double distance = registeredClassificationFeatureDistance(feature, center);
        if (!std::isfinite(distance))
            return RegisteredClassificationRadiusStats();
        distances.append(distance);
        total += distance;
        maxDistance = qMax(maxDistance, distance);
    }

    const double meanDistance = total / static_cast<double>(distances.size());
    double squaredDeviation = 0.0;
    for (double distance : distances) {
        const double deviation = distance - meanDistance;
        squaredDeviation += deviation * deviation;
    }
    const double stdDevDistance = std::sqrt(
            squaredDeviation / static_cast<double>(distances.size()));

    stats.enabled = true;
    stats.meanDistance = meanDistance;
    stats.stdDevDistance = stdDevDistance;
    stats.maxDistance = maxDistance;
    stats.radius = qBound(0.10,
                          qMax(maxDistance * 1.10,
                               meanDistance + 2.5 * stdDevDistance),
                          2.00);
    return stats;
}
