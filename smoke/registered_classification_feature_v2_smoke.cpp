#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QtMath>

#include <iostream>
#include <cmath>
#include <limits>
#include <opencv2/imgproc.hpp>

namespace {

int g_failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++g_failures;
    }
}

QString smokeModelDir(const QString &name)
{
    const QString path = QDir(QDir::tempPath()).filePath(
            QStringLiteral("registered_classification_feature_v2_smoke/%1").arg(name));
    QDir(path).removeRecursively();
    QDir().mkpath(path);
    return path;
}

bool createEmptyFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate);
}

bool writeSentinelFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write("gnc-sentinel") > 0;
}

bool writeMetadataJson(const QString &modelDir, const QJsonObject &metadata)
{
    QFile file(registeredClassificationMetadataPath(modelDir));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(QJsonDocument(metadata).toJson(QJsonDocument::Compact)) > 0;
}

bool writeClassStatsJson(const QString &modelDir, const QJsonObject &classStats)
{
    QFile file(registeredClassificationClassStatsPath(modelDir));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(QJsonDocument(classStats).toJson(QJsonDocument::Compact)) > 0;
}

QJsonObject classStatsWithFirstClassField(const QJsonObject &classStats,
                                          const QString &field,
                                          const QJsonValue &value)
{
    QJsonObject mutated = classStats;
    QJsonArray classes = mutated.value(QStringLiteral("classes")).toArray();
    QJsonObject firstClass = classes.at(0).toObject();
    firstClass.insert(field, value);
    classes.replace(0, firstClass);
    mutated.insert(QStringLiteral("classes"), classes);
    return mutated;
}

cv::Mat makePartImage(double angleDegrees, double scale,
                      const cv::Scalar &foreground = cv::Scalar(40, 190, 230),
                      const cv::Scalar &background = cv::Scalar(28, 28, 28),
                      bool circle = false)
{
    cv::Mat image(256, 256, CV_8UC3, background);
    if (circle) {
        cv::circle(image, cv::Point(128, 128), qRound(50.0 * scale), foreground, cv::FILLED);
        return image;
    }

    const cv::RotatedRect part(cv::Point2f(128.0f, 128.0f),
                               cv::Size2f(static_cast<float>(112.0 * scale),
                                          static_cast<float>(58.0 * scale)),
                               static_cast<float>(angleDegrees));
    cv::Point2f corners[4];
    part.points(corners);
    std::vector<cv::Point> polygon;
    for (const cv::Point2f &corner : corners)
        polygon.emplace_back(cvRound(corner.x), cvRound(corner.y));
    cv::fillConvexPoly(image, polygon, foreground);
    return image;
}

double vectorNorm(const QVector<double> &values)
{
    double squaredNorm = 0.0;
    for (double value : values)
        squaredNorm += value * value;
    return std::sqrt(squaredNorm);
}

bool allFinite(const QVector<double> &values)
{
    for (double value : values) {
        if (!std::isfinite(value))
            return false;
    }
    return true;
}

QString existingNonHalconLibrary()
{
    const QStringList candidates = {
        QStringLiteral("/lib/x86_64-linux-gnu/libm.so.6"),
        QStringLiteral("/usr/lib/x86_64-linux-gnu/libm.so.6"),
        QStringLiteral("/lib/x86_64-linux-gnu/libc.so.6")
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString();
}

QStringList expectedFeatureNamesV2()
{
    return {
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
        QStringLiteral("shapeMomentPsi4"),
        QStringLiteral("occupancyR0C0"),
        QStringLiteral("occupancyR0C1"),
        QStringLiteral("occupancyR0C2"),
        QStringLiteral("occupancyR0C3"),
        QStringLiteral("occupancyR1C0"),
        QStringLiteral("occupancyR1C1"),
        QStringLiteral("occupancyR1C2"),
        QStringLiteral("occupancyR1C3"),
        QStringLiteral("occupancyR2C0"),
        QStringLiteral("occupancyR2C1"),
        QStringLiteral("occupancyR2C2"),
        QStringLiteral("occupancyR2C3"),
        QStringLiteral("occupancyR3C0"),
        QStringLiteral("occupancyR3C1"),
        QStringLiteral("occupancyR3C2"),
        QStringLiteral("occupancyR3C3"),
        QStringLiteral("grayMean"),
        QStringLiteral("grayDeviation"),
        QStringLiteral("grayHist00"),
        QStringLiteral("grayHist01"),
        QStringLiteral("grayHist02"),
        QStringLiteral("grayHist03"),
        QStringLiteral("grayHist04"),
        QStringLiteral("grayHist05"),
        QStringLiteral("grayHist06"),
        QStringLiteral("grayHist07"),
        QStringLiteral("grayHist08"),
        QStringLiteral("grayHist09"),
        QStringLiteral("grayHist10"),
        QStringLiteral("grayHist11"),
        QStringLiteral("grayHist12"),
        QStringLiteral("grayHist13"),
        QStringLiteral("grayHist14"),
        QStringLiteral("grayHist15"),
        QStringLiteral("labMeanL"),
        QStringLiteral("labMeanA"),
        QStringLiteral("labMeanB"),
        QStringLiteral("labDeviationL"),
        QStringLiteral("labDeviationA"),
        QStringLiteral("labDeviationB"),
        QStringLiteral("textureEntropy"),
        QStringLiteral("textureAnisotropy"),
        QStringLiteral("coocEnergy"),
        QStringLiteral("coocCorrelation"),
        QStringLiteral("coocHomogeneity"),
        QStringLiteral("coocContrast")
    };
}

QJsonObject expectedSegmentationContract()
{
    return {
        {QStringLiteral("thresholdMethod"), QStringLiteral("max_separability")},
        {QStringLiteral("thresholdPolarities"),
         QJsonArray({QStringLiteral("light"), QStringLiteral("dark")})},
        {QStringLiteral("morphologyRadiusMinimum"), 1.0},
        {QStringLiteral("morphologyRadiusRoiScale"), 0.005},
        {QStringLiteral("candidateAreaRatioMin"), 0.02},
        {QStringLiteral("candidateAreaRatioMax"), 0.98},
        {QStringLiteral("borderBandRatio"), 0.01},
        {QStringLiteral("borderTouchDivisor"), 0.05},
        {QStringLiteral("objectScoreCenterWeight"), 0.55},
        {QStringLiteral("objectScoreBorderWeight"), 0.30},
        {QStringLiteral("objectScoreAreaWeight"), 0.15}
    };
}

QJsonObject expectedCanonicalizationContract()
{
    return {
        {QStringLiteral("width"), 128},
        {QStringLiteral("height"), 128},
        {QStringLiteral("paddingRatio"), 0.08},
        {QStringLiteral("nearEqualAxisThreshold"), 0.05},
        {QStringLiteral("occupancyOrientationGridRows"), 4},
        {QStringLiteral("occupancyOrientationGridColumns"), 4}
    };
}

QJsonObject expectedFeatureGroupsContract()
{
    return {
        {QStringLiteral("names"), QJsonArray({
             QStringLiteral("shape"), QStringLiteral("occupancy"),
             QStringLiteral("gray"), QStringLiteral("lab"),
             QStringLiteral("texture")})},
        {QStringLiteral("dimensions"), QJsonArray({13, 16, 18, 6, 6})},
        {QStringLiteral("weights"), QJsonArray({0.35, 0.25, 0.15, 0.15, 0.10})}
    };
}

RegisteredClassificationKnnModelMetadata validMetadata()
{
    RegisteredClassificationKnnModelMetadata metadata;
    metadata.modelType = registeredClassificationKnnModelType();
    metadata.schemaVersion = 2;
    metadata.featureVersion = registeredClassificationFeatureVersionV2();
    metadata.featureNames = registeredClassificationFeatureNamesV2();
    metadata.featureLength = metadata.featureNames.size();
    metadata.halconVersion = QStringLiteral("24.11-test");
    metadata.classLabels = {{0, QStringLiteral("A")}, {1, QStringLiteral("B")}};
    metadata.trainingSampleCount = 6;
    return metadata;
}

RegisteredClassificationClassStatsDocument validClassStats()
{
    RegisteredClassificationClassStatsDocument document;
    document.schemaVersion = 2;
    document.featureVersion = registeredClassificationFeatureVersionV2();

    RegisteredClassificationClassStats first;
    first.classId = 0;
    first.sampleCount = 3;
    first.radiusEnabled = true;
    first.radius = 0.35;
    first.meanDistance = 0.20;
    first.stdDevDistance = 0.05;
    first.maxDistance = 0.30;
    document.classes.append(first);

    RegisteredClassificationClassStats second;
    second.classId = 1;
    second.sampleCount = 3;
    second.radiusEnabled = true;
    second.radius = 0.10;
    document.classes.append(second);
    return document;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    check(registeredClassificationKnnModelType()
          == QStringLiteral("halcon_knn_registered_classification"),
          "V2 model type must be HALCON KNN");
    check(registeredClassificationLegacyMlpModelType()
          == QStringLiteral("halcon_mlp_registered_classification"),
          "legacy MLP model type must remain recognizable");
    check(registeredClassificationFeatureVersionV2()
          == QStringLiteral("halcon_registered_feature_v2"),
          "V2 feature version must be stable");

    const QStringList featureNames = registeredClassificationFeatureNamesV2();
    check(featureNames.size() == 59, "V2 feature contract must contain 59 names");
    check(featureNames == expectedFeatureNamesV2(),
          "V2 feature names must match the exact 59-name sequence");
    check(QSet<QString>(featureNames.cbegin(), featureNames.cend()).size() == featureNames.size(),
          "V2 feature names must be unique");
    check(registeredClassificationFeatureGroupDimensions() == QVector<int>({13, 16, 18, 6, 6}),
          "V2 feature groups must be 13/16/18/6/6");

    const QVector<double> groupWeights = registeredClassificationFeatureGroupWeights();
    check(groupWeights == QVector<double>({0.35, 0.25, 0.15, 0.15, 0.10}),
          "V2 feature weights must match the exact group vector");
    check(registeredClassificationSegmentationContractV2()
                  == expectedSegmentationContract(),
          "public segmentation builder must expose the exact V2 contract");
    check(registeredClassificationCanonicalizationContractV2()
                  == expectedCanonicalizationContract(),
          "public canonicalization builder must expose the exact V2 contract");
    check(registeredClassificationFeatureGroupsContractV2()
                  == expectedFeatureGroupsContract(),
          "public feature-group builder must expose the exact V2 contract");

    RegisteredClassificationFeatureExtractor extractor;
    RegisteredClassificationFeatureConfig extractorConfig;
    RegisteredClassificationFeatureRegion region;
    region.type = QStringLiteral("rectangle");
    region.rectNormalized = QRectF(0.05, 0.05, 0.90, 0.90);

    const RegisteredClassificationFeatureResult base =
            extractor.extract(makePartImage(0.0, 1.0), region, extractorConfig);
    const RegisteredClassificationFeatureResult rotated =
            extractor.extract(makePartImage(45.0, 1.0), region, extractorConfig);
    const RegisteredClassificationFeatureResult scaled =
            extractor.extract(makePartImage(90.0, 0.70), region, extractorConfig);
    const RegisteredClassificationFeatureResult rotated180 =
            extractor.extract(makePartImage(180.0, 1.0), region, extractorConfig);
    const RegisteredClassificationFeatureResult scaledUp =
            extractor.extract(makePartImage(0.0, 1.30), region, extractorConfig);
    check(base.success && rotated.success && scaled.success
                  && rotated180.success && scaledUp.success,
          "rotation and scale variants must extract");
    check(base.feature.size() == 59, "V2 extractor must return 59 values");
    check(base.featureNames == registeredClassificationFeatureNamesV2(),
          "V2 extractor must use the exact public feature order");
    check(allFinite(base.feature), "V2 features must be finite");
    check(qAbs(vectorNorm(base.feature) - 1.0) < 1e-6,
          "V2 output must be L2 normalized");
    check(base.payload.value(QStringLiteral("canonicalWidth")).toInt() == 128
                  && base.payload.value(QStringLiteral("canonicalHeight")).toInt() == 128,
          "V2 payload must report the fixed canonical size");

    const RegisteredClassificationFeatureResult light = extractor.extract(
            makePartImage(0.0, 1.0, cv::Scalar(230, 230, 230), cv::Scalar(25, 25, 25)),
            region, extractorConfig);
    const RegisteredClassificationFeatureResult dark = extractor.extract(
            makePartImage(0.0, 1.0, cv::Scalar(25, 25, 25), cv::Scalar(230, 230, 230)),
            region, extractorConfig);
    check(light.success && light.foregroundPolarity == QStringLiteral("light"),
          "light foreground polarity must extract");
    check(dark.success && dark.foregroundPolarity == QStringLiteral("dark"),
          "dark foreground polarity must extract");

    const RegisteredClassificationFeatureResult sameColorCircle = extractor.extract(
            makePartImage(0.0, 1.0, cv::Scalar(40, 190, 230), cv::Scalar(28, 28, 28), true),
            region, extractorConfig);
    const RegisteredClassificationFeatureResult differentColorRectangle = extractor.extract(
            makePartImage(0.0, 1.0, cv::Scalar(220, 70, 40), cv::Scalar(28, 28, 28)),
            region, extractorConfig);
    check(sameColorCircle.success && differentColorRectangle.success,
          "shape and color comparison fixtures must extract");
    const double rotationDistance = registeredClassificationFeatureDistance(
            base.feature, rotated.feature);
    const double shapeDistance = registeredClassificationFeatureDistance(
            base.feature, sameColorCircle.feature);
    const double colorDistance = registeredClassificationFeatureDistance(
            base.feature, differentColorRectangle.feature);
    check(shapeDistance > rotationDistance,
          "rectangle-to-circle distance must exceed rectangle rotation distance");
    check(shapeDistance > 1e-6, "same-color different-shape features must differ");
    check(colorDistance > 1e-6, "same-shape different-color features must differ");

    cv::Mat polygonFixture(256, 256, CV_8UC3, cv::Scalar(25, 25, 25));
    cv::circle(polygonFixture, cv::Point(122, 42), 14, cv::Scalar(235, 235, 235), cv::FILLED);
    RegisteredClassificationFeatureRegion polygonRegion;
    polygonRegion.type = QStringLiteral("polygon");
    polygonRegion.polygonNormalized = {
        QPointF(0.10, 0.10), QPointF(0.10, 0.90), QPointF(0.55, 0.50)
    };
    const RegisteredClassificationFeatureResult polygonExcluded =
            extractor.extract(polygonFixture, polygonRegion, extractorConfig);
    check(!polygonExcluded.success
                  && polygonExcluded.status == QStringLiteral("foreground_not_found"),
          "polygon ROI must use the true polygon instead of its bounding rectangle");

    RegisteredClassificationFeatureRegion asymmetricPolygon;
    asymmetricPolygon.type = QStringLiteral("polygon");
    asymmetricPolygon.polygonNormalized = {
        QPointF(0.475, 0.10), QPointF(0.525, 0.10),
        QPointF(0.99, 0.90), QPointF(0.01, 0.90)
    };
    const cv::Scalar centroidColor(30, 150, 30);
    const cv::Scalar boundsCenterColor(30, 30, 220);
    cv::Mat twoCandidates(256, 256, CV_8UC3, cv::Scalar(20, 20, 20));
    cv::circle(twoCandidates, cv::Point(128, 159), 14, centroidColor, cv::FILLED);
    cv::circle(twoCandidates, cv::Point(128, 128), 14, boundsCenterColor, cv::FILLED);
    cv::Mat centroidCandidateOnly(256, 256, CV_8UC3, cv::Scalar(20, 20, 20));
    cv::circle(centroidCandidateOnly, cv::Point(128, 159), 14, centroidColor, cv::FILLED);
    cv::Mat boundsCenterCandidateOnly(256, 256, CV_8UC3, cv::Scalar(20, 20, 20));
    cv::circle(boundsCenterCandidateOnly, cv::Point(128, 128), 14, boundsCenterColor, cv::FILLED);
    const RegisteredClassificationFeatureResult polygonChoice =
            extractor.extract(twoCandidates, asymmetricPolygon, extractorConfig);
    const RegisteredClassificationFeatureResult centroidReference =
            extractor.extract(centroidCandidateOnly, asymmetricPolygon, extractorConfig);
    const RegisteredClassificationFeatureResult boundsCenterReference =
            extractor.extract(boundsCenterCandidateOnly, asymmetricPolygon, extractorConfig);
    check(polygonChoice.success && centroidReference.success && boundsCenterReference.success,
          "asymmetric polygon candidate-scoring fixtures must extract");
    check(registeredClassificationFeatureDistance(polygonChoice.feature, centroidReference.feature)
                  < registeredClassificationFeatureDistance(
                      polygonChoice.feature, boundsCenterReference.feature),
          "polygon candidate scoring must use the actual ROI centroid");

    const RegisteredClassificationFeatureResult uniform = extractor.extract(
            cv::Mat(256, 256, CV_8UC3, cv::Scalar(128, 128, 128)),
            region, extractorConfig);
    check(!uniform.success && uniform.status == QStringLiteral("foreground_not_found"),
          "uniform ROI must not fall back to the whole ROI");

    RegisteredClassificationFeatureConfig missingSymbolConfig;
    missingSymbolConfig.halconSoPath = existingNonHalconLibrary();
    const RegisteredClassificationFeatureResult missingSymbol = extractor.extract(
            makePartImage(0.0, 1.0), region, missingSymbolConfig);
    check(!missingSymbolConfig.halconSoPath.isEmpty(),
          "missing-symbol test requires an existing non-HALCON shared object");
    check(!missingSymbol.success
                  && missingSymbol.status == QStringLiteral("halcon_symbol_missing")
                  && missingSymbol.message.contains(QStringLiteral("SetHcInterfaceStringEncodingIsUtf8")),
          "missing HALCON symbol must identify the first missing symbol");

    QVector<double> unit(59, 1.0);
    check(normalizeRegisteredClassificationFeature(&unit),
          "non-zero V2 vector must normalize");
    check(qAbs(registeredClassificationFeatureDistance(unit, unit)) < 1e-9,
          "identical vectors must have zero distance");
    check(qAbs(registeredClassificationSimilarityFromDistance(0.0) - 1.0) < 1e-9,
          "zero distance must map to similarity one");

    QVector<double> zero(59, 0.0);
    check(!normalizeRegisteredClassificationFeature(&zero), "zero V2 vector must be rejected");
    check(qIsNaN(registeredClassificationFeatureDistance(QVector<double>(59, 1.0),
                                                          QVector<double>(58, 1.0))),
          "feature distance must reject different lengths");

    const QVector<QVector<double>> twoSamples = {
        QVector<double>(59, 1.0),
        QVector<double>(59, 0.5)
    };
    const QVector<double> center = registeredClassificationClassCenter(twoSamples);
    check(center.size() == 59, "class center must have V2 length");
    const RegisteredClassificationRadiusStats disabledRadius =
            registeredClassificationRadiusStats(twoSamples, center);
    check(!disabledRadius.enabled, "radius must be disabled below three samples");
    check(disabledRadius.radius == 0.0,
          "disabled feature-space radius must be exactly zero");

    QVector<double> axis0(59, 0.0);
    QVector<double> axis1(59, 0.0);
    axis0[0] = 1.0;
    axis1[1] = 1.0;
    const QVector<QVector<double>> populationSamples = {axis0, axis0, axis1};
    const QVector<double> populationCenter =
            registeredClassificationClassCenter(populationSamples);
    const RegisteredClassificationRadiusStats populationRadius =
            registeredClassificationRadiusStats(populationSamples, populationCenter);
    const double distance0 = registeredClassificationFeatureDistance(axis0, populationCenter);
    const double distance1 = registeredClassificationFeatureDistance(axis1, populationCenter);
    const double expectedMean = (distance0 + distance0 + distance1) / 3.0;
    const double expectedStdDev = std::sqrt(
            ((distance0 - expectedMean) * (distance0 - expectedMean)
             + (distance0 - expectedMean) * (distance0 - expectedMean)
             + (distance1 - expectedMean) * (distance1 - expectedMean)) / 3.0);
    const double expectedRadius = qBound(
            0.10, qMax(distance1 * 1.10, expectedMean + 2.5 * expectedStdDev), 2.00);
    check(populationRadius.enabled, "radius must be enabled at three samples");
    check(qAbs(populationRadius.meanDistance - expectedMean) < 1e-12,
          "radius mean must use all population distances");
    check(qAbs(populationRadius.stdDevDistance - expectedStdDev) < 1e-12,
          "radius deviation must use population standard deviation");
    check(qAbs(populationRadius.radius - expectedRadius) < 1e-12,
          "enabled radius must use the fixed max/formula contract");

    const RegisteredClassificationRadiusStats lowerClampedRadius =
            registeredClassificationRadiusStats({axis0, axis0, axis0}, axis0);
    check(lowerClampedRadius.enabled && lowerClampedRadius.radius == 0.10,
          "enabled radius must clamp to the exact lower bound");
    QVector<double> negativeAxis0 = axis0;
    negativeAxis0[0] = -1.0;
    const RegisteredClassificationRadiusStats upperClampedRadius =
            registeredClassificationRadiusStats(
                    {negativeAxis0, negativeAxis0, negativeAxis0}, axis0);
    check(upperClampedRadius.enabled && upperClampedRadius.radius == 2.00,
          "enabled radius must clamp to the exact upper bound");

    const QString modelDir = smokeModelDir(QStringLiteral("schema_2"));
    const RegisteredClassificationKnnModelMetadata metadata = validMetadata();
    check(writeRegisteredClassificationKnnMetadata(modelDir, metadata).success,
          "schema 2 metadata must write");

    RegisteredClassificationKnnModelMetadata loadedMetadata;
    check(readRegisteredClassificationKnnMetadata(modelDir, &loadedMetadata).success,
          "schema 2 metadata must round-trip");
    check(loadedMetadata.featureNames == metadata.featureNames,
          "schema 2 feature names must round-trip");

    QFile metadataFile(registeredClassificationMetadataPath(modelDir));
    check(metadataFile.open(QIODevice::ReadOnly), "schema 2 metadata JSON must be readable");
    const QJsonObject metadataJson = metadataFile.isOpen()
            ? QJsonDocument::fromJson(metadataFile.readAll()).object()
            : QJsonObject();
    const QJsonObject knnJson = metadataJson.value(QStringLiteral("knn")).toObject();
    check(metadataJson.value(QStringLiteral("segmentation")).toObject()
                  == expectedSegmentationContract(),
          "schema 2 metadata must persist the exact segmentation contract");
    check(metadataJson.value(QStringLiteral("canonicalization")).toObject()
                  == expectedCanonicalizationContract(),
          "schema 2 metadata must persist the exact canonicalization contract");
    check(metadataJson.value(QStringLiteral("featureGroups")).toObject()
                  == expectedFeatureGroupsContract(),
          "schema 2 metadata must persist the exact feature-group contract");
    check(knnJson.value(QStringLiteral("method")).toString()
                  == QStringLiteral("classes_distance"),
          "schema 2 metadata must persist KNN method");
    check(knnJson.contains(QStringLiteral("normalization"))
                  && !knnJson.value(QStringLiteral("normalization")).toBool(true),
          "schema 2 metadata must persist disabled KNN normalization");

    const auto checkInvalidMetadata = [&](const QJsonObject &mutatedMetadata,
                                          const char *message) {
        check(writeMetadataJson(modelDir, mutatedMetadata),
              "mutated metadata fixture must write");
        RegisteredClassificationKnnModelMetadata rejectedMetadata;
        const RegisteredClassificationModelPackageResult readResult =
                readRegisteredClassificationKnnMetadata(modelDir, &rejectedMetadata);
        check(!readResult.success, message);
        check(readResult.status.startsWith(QStringLiteral("invalid_")),
              "malformed schema 2 metadata must report an actionable invalid_* status");
    };

    const QStringList persistedRootFields = {
        QStringLiteral("modelType"), QStringLiteral("schemaVersion"),
        QStringLiteral("featureVersion"), QStringLiteral("halconVersion"),
        QStringLiteral("classLabels"), QStringLiteral("featureNames"),
        QStringLiteral("featureLength"), QStringLiteral("segmentation"),
        QStringLiteral("canonicalization"), QStringLiteral("featureGroups"),
        QStringLiteral("knn"), QStringLiteral("thresholds"),
        QStringLiteral("trainingSampleCount")
    };
    for (const QString &field : persistedRootFields) {
        QJsonObject missingRootField = metadataJson;
        missingRootField.remove(field);
        checkInvalidMetadata(missingRootField,
                             "every persisted schema 2 root field must be required");
    }

    const QVector<QPair<QString, QJsonValue>> wrongRootTypes = {
        {QStringLiteral("modelType"), true},
        {QStringLiteral("schemaVersion"), QStringLiteral("2")},
        {QStringLiteral("featureVersion"), 2},
        {QStringLiteral("halconVersion"), 24.11},
        {QStringLiteral("classLabels"), QJsonObject()},
        {QStringLiteral("featureNames"), QJsonObject()},
        {QStringLiteral("featureLength"), QStringLiteral("59")},
        {QStringLiteral("segmentation"), QJsonArray()},
        {QStringLiteral("canonicalization"), QStringLiteral("fixed")},
        {QStringLiteral("featureGroups"), QJsonArray()},
        {QStringLiteral("knn"), QJsonArray()},
        {QStringLiteral("thresholds"), QJsonArray()},
        {QStringLiteral("trainingSampleCount"), QStringLiteral("6")}
    };
    for (const auto &wrongType : wrongRootTypes) {
        QJsonObject malformedRoot = metadataJson;
        malformedRoot.insert(wrongType.first, wrongType.second);
        checkInvalidMetadata(malformedRoot,
                             "every persisted schema 2 root field must be type checked");
    }

    QJsonObject emptyHalconVersion = metadataJson;
    emptyHalconVersion.insert(QStringLiteral("halconVersion"), QStringLiteral("  "));
    checkInvalidMetadata(emptyHalconVersion,
                         "schema 2 HALCON version must be a non-empty string");

    QJsonObject malformedClassLabel = metadataJson;
    QJsonArray malformedClassLabels = malformedClassLabel.value(
            QStringLiteral("classLabels")).toArray();
    malformedClassLabels.replace(0, QStringLiteral("A"));
    malformedClassLabel.insert(QStringLiteral("classLabels"), malformedClassLabels);
    checkInvalidMetadata(malformedClassLabel,
                         "schema 2 class labels must contain JSON objects");

    const auto checkClassLabelField = [&](const QString &field, const QJsonValue &value,
                                          const char *message) {
        QJsonObject mutatedMetadata = metadataJson;
        QJsonArray labels = mutatedMetadata.value(QStringLiteral("classLabels")).toArray();
        QJsonObject firstLabel = labels.at(0).toObject();
        firstLabel.insert(field, value);
        labels.replace(0, firstLabel);
        mutatedMetadata.insert(QStringLiteral("classLabels"), labels);
        checkInvalidMetadata(mutatedMetadata, message);
    };
    checkClassLabelField(QStringLiteral("id"), QStringLiteral("0"),
                         "schema 2 class label ids must be integer JSON numbers");
    checkClassLabelField(QStringLiteral("id"), 0.5,
                         "schema 2 class label ids must reject fractional numbers");
    checkClassLabelField(QStringLiteral("name"), 0,
                         "schema 2 class label names must be JSON strings");
    checkClassLabelField(QStringLiteral("name"), QStringLiteral("  "),
                         "schema 2 class label names must be non-empty");

    QJsonObject malformedFeatureName = metadataJson;
    QJsonArray malformedFeatureNames = malformedFeatureName.value(
            QStringLiteral("featureNames")).toArray();
    malformedFeatureNames.replace(0, 7);
    malformedFeatureName.insert(QStringLiteral("featureNames"), malformedFeatureNames);
    checkInvalidMetadata(malformedFeatureName,
                         "schema 2 feature names must all be JSON strings");

    const auto checkFixedObjectTamper = [&](const QString &section,
                                            const QString &field,
                                            const QJsonValue &value,
                                            const char *message) {
        QJsonObject mutatedMetadata = metadataJson;
        QJsonObject contract = mutatedMetadata.value(section).toObject();
        contract.insert(field, value);
        mutatedMetadata.insert(section, contract);
        checkInvalidMetadata(mutatedMetadata, message);
    };
    const auto checkMissingFixedObjectField = [&](const QString &section,
                                                  const QString &field) {
        QJsonObject mutatedMetadata = metadataJson;
        QJsonObject contract = mutatedMetadata.value(section).toObject();
        contract.remove(field);
        mutatedMetadata.insert(section, contract);
        checkInvalidMetadata(mutatedMetadata,
                             "fixed non-KNN contract fields must be explicitly persisted");
    };
    for (const QString &field : expectedSegmentationContract().keys())
        checkMissingFixedObjectField(QStringLiteral("segmentation"), field);
    for (const QString &field : expectedCanonicalizationContract().keys())
        checkMissingFixedObjectField(QStringLiteral("canonicalization"), field);
    for (const QString &field : expectedFeatureGroupsContract().keys())
        checkMissingFixedObjectField(QStringLiteral("featureGroups"), field);
    checkFixedObjectTamper(QStringLiteral("segmentation"),
                           QStringLiteral("thresholdPolarities"), QStringLiteral("light,dark"),
                           "segmentation polarities must retain their JSON array type");
    checkFixedObjectTamper(QStringLiteral("canonicalization"),
                           QStringLiteral("width"), QStringLiteral("128"),
                           "canonical dimensions must retain their integer JSON type");
    checkFixedObjectTamper(QStringLiteral("featureGroups"),
                           QStringLiteral("dimensions"), QStringLiteral("13,16,18,6,6"),
                           "feature-group dimensions must retain their JSON array type");
    checkFixedObjectTamper(QStringLiteral("segmentation"),
                           QStringLiteral("morphologyRadiusRoiScale"),
                           0.005000000000001,
                           "tiny segmentation float tamper must be rejected exactly");
    checkFixedObjectTamper(QStringLiteral("segmentation"),
                           QStringLiteral("objectScoreCenterWeight"),
                           0.550000000000001,
                           "tiny object-score weight tamper must be rejected exactly");
    checkFixedObjectTamper(QStringLiteral("canonicalization"),
                           QStringLiteral("paddingRatio"),
                           0.080000000000001,
                           "tiny canonicalization float tamper must be rejected exactly");
    QJsonArray tamperedGroupWeights = expectedFeatureGroupsContract().value(
            QStringLiteral("weights")).toArray();
    tamperedGroupWeights.replace(0, 0.350000000000001);
    checkFixedObjectTamper(QStringLiteral("featureGroups"), QStringLiteral("weights"),
                           tamperedGroupWeights,
                           "tiny feature-group weight tamper must be rejected exactly");
    checkFixedObjectTamper(QStringLiteral("knn"), QStringLiteral("epsilon"), 1e-15,
                           "tiny KNN epsilon tamper must be rejected exactly");
    checkFixedObjectTamper(QStringLiteral("knn"), QStringLiteral("sampleWeight"),
                           0.700000000000001,
                           "tiny KNN sample weight tamper must be rejected exactly");
    checkFixedObjectTamper(QStringLiteral("knn"), QStringLiteral("centerWeight"),
                           0.300000000000001,
                           "tiny KNN center weight tamper must be rejected exactly");

    for (const QString &section : {QStringLiteral("segmentation"),
                                  QStringLiteral("canonicalization"),
                                  QStringLiteral("featureGroups"),
                                  QStringLiteral("knn"),
                                  QStringLiteral("thresholds")}) {
        checkFixedObjectTamper(section, QStringLiteral("unexpected"), true,
                               "fixed schema 2 contract objects must reject extra keys");
    }
    check(writeMetadataJson(modelDir, metadataJson),
          "valid metadata fixture must restore after root contract checks");

    RegisteredClassificationKnnModelMetadata badSchema = metadata;
    badSchema.schemaVersion = 1;
    check(!writeRegisteredClassificationKnnMetadata(
                  smokeModelDir(QStringLiteral("bad_schema")), badSchema).success,
          "schema 1 must be rejected by the schema 2 writer");

    RegisteredClassificationKnnModelMetadata badFeatureLength = metadata;
    --badFeatureLength.featureLength;
    const RegisteredClassificationModelPackageResult badFeatureResult =
            writeRegisteredClassificationKnnMetadata(
                    smokeModelDir(QStringLiteral("bad_feature_length")), badFeatureLength);
    check(!badFeatureResult.success, "schema 2 feature length mismatch must fail");
    check(badFeatureResult.status == QStringLiteral("feature_length_mismatch"),
          "schema 2 feature length mismatch must identify its error");

    const RegisteredClassificationClassStatsDocument stats = validClassStats();
    check(writeRegisteredClassificationClassStats(modelDir, stats).success,
          "class stats must write");
    RegisteredClassificationClassStatsDocument loadedStats;
    check(readRegisteredClassificationClassStats(modelDir, &loadedStats).success,
          "class stats must round-trip");
    check(loadedStats.classes.size() == 2, "class stats classes must round-trip");
    check(loadedStats.classes.value(0).radiusEnabled,
          "class stats radius enabled flag must round-trip");
    check(qAbs(loadedStats.classes.value(0).radius - 0.35) < 1e-9,
          "class stats radius must round-trip");

    RegisteredClassificationClassStatsDocument tooFewEnabled = stats;
    tooFewEnabled.classes[0].sampleCount = 2;
    const RegisteredClassificationModelPackageResult tooFewEnabledResult =
            writeRegisteredClassificationClassStats(
                    smokeModelDir(QStringLiteral("too_few_enabled")), tooFewEnabled);
    check(!tooFewEnabledResult.success
                  && tooFewEnabledResult.status == QStringLiteral("invalid_class_stats"),
          "radius must be disabled below three samples");

    RegisteredClassificationClassStatsDocument enoughDisabled = stats;
    enoughDisabled.classes[0].radiusEnabled = false;
    enoughDisabled.classes[0].radius = 0.0;
    const RegisteredClassificationModelPackageResult enoughDisabledResult =
            writeRegisteredClassificationClassStats(
                    smokeModelDir(QStringLiteral("enough_disabled")), enoughDisabled);
    check(!enoughDisabledResult.success
                  && enoughDisabledResult.status == QStringLiteral("invalid_class_stats"),
          "radius must be enabled at three or more samples");

    RegisteredClassificationClassStatsDocument incoherentDisabled = stats;
    incoherentDisabled.classes[0].sampleCount = 2;
    incoherentDisabled.classes[0].radiusEnabled = false;
    incoherentDisabled.classes[0].radius = 0.35;
    const RegisteredClassificationModelPackageResult incoherentDisabledResult =
            writeRegisteredClassificationClassStats(
                    smokeModelDir(QStringLiteral("incoherent_disabled")), incoherentDisabled);
    check(!incoherentDisabledResult.success
                  && incoherentDisabledResult.status == QStringLiteral("invalid_class_stats"),
          "disabled class radius must be exactly zero");

    RegisteredClassificationClassStatsDocument nanClassStats = stats;
    nanClassStats.classes[0].meanDistance = qQNaN();
    const RegisteredClassificationModelPackageResult nanClassStatsResult =
            writeRegisteredClassificationClassStats(
                    smokeModelDir(QStringLiteral("nan_class_stats")), nanClassStats);
    check(!nanClassStatsResult.success,
          "non-finite class stats values must be rejected by the validator");
    check(nanClassStatsResult.status == QStringLiteral("invalid_class_stats"),
          "non-finite class stats values must report invalid class stats");

    RegisteredClassificationClassStatsDocument infiniteClassStats = stats;
    infiniteClassStats.classes[0].maxDistance = std::numeric_limits<double>::infinity();
    const RegisteredClassificationModelPackageResult infiniteClassStatsResult =
            writeRegisteredClassificationClassStats(
                    smokeModelDir(QStringLiteral("infinite_class_stats")), infiniteClassStats);
    check(!infiniteClassStatsResult.success,
          "infinite class stats values must be rejected by the validator");
    check(infiniteClassStatsResult.status == QStringLiteral("invalid_class_stats"),
          "infinite class stats values must report invalid class stats");

    const RegisteredClassificationModelPackageResult incompletePackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!incompletePackage.success,
          "schema 2 package without both KNN files must be incomplete");
    check(incompletePackage.status == QStringLiteral("model_package_incomplete"),
          "missing KNN files must report an incomplete package");

    check(createEmptyFile(registeredClassificationSampleKnnPath(modelDir)),
          "sample KNN file fixture must write");
    check(createEmptyFile(registeredClassificationCenterKnnPath(modelDir)),
          "center KNN file fixture must write");
    const RegisteredClassificationModelPackageResult zeroBytePackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!zeroBytePackage.success,
          "zero-byte KNN files must leave the schema 2 package incomplete");
    check(zeroBytePackage.status == QStringLiteral("model_package_incomplete"),
          "zero-byte KNN files must report an incomplete package");
    const RegisteredClassificationModelInspection zeroByteInspection =
            inspectRegisteredClassificationModelPackage(modelDir);
    check(!zeroByteInspection.runnable,
          "zero-byte KNN files must not be inspected as runnable");

    check(writeSentinelFile(registeredClassificationSampleKnnPath(modelDir)),
          "sample KNN sentinel fixture must write");
    check(writeSentinelFile(registeredClassificationCenterKnnPath(modelDir)),
          "center KNN sentinel fixture must write");
    check(validateRegisteredClassificationKnnPackage(modelDir).success,
          "complete schema 2 package must validate");

    const QString overflowDir = smokeModelDir(QStringLiteral("overflow_sample_total"));
    RegisteredClassificationKnnModelMetadata overflowMetadata = validMetadata();
    overflowMetadata.classLabels.append({2, QStringLiteral("C")});
    overflowMetadata.trainingSampleCount = 6;
    check(writeRegisteredClassificationKnnMetadata(overflowDir, overflowMetadata).success,
          "overflow metadata fixture must write");
    RegisteredClassificationClassStatsDocument overflowStats;
    overflowStats.featureVersion = registeredClassificationFeatureVersionV2();
    for (int classId = 0; classId < 3; ++classId) {
        RegisteredClassificationClassStats classStats;
        classStats.classId = classId;
        classStats.sampleCount = classId < 2 ? std::numeric_limits<int>::max() : 8;
        classStats.radiusEnabled = true;
        classStats.radius = 0.10;
        overflowStats.classes.append(classStats);
    }
    check(writeRegisteredClassificationClassStats(overflowDir, overflowStats).success,
          "overflow class stats fixture must write");
    check(writeSentinelFile(registeredClassificationSampleKnnPath(overflowDir))
                  && writeSentinelFile(registeredClassificationCenterKnnPath(overflowDir)),
          "overflow KNN sentinel fixtures must write");
    const RegisteredClassificationModelPackageResult overflowPackage =
            validateRegisteredClassificationKnnPackage(overflowDir);
    check(!overflowPackage.success
                  && overflowPackage.status == QStringLiteral("class_stats_mismatch"),
          "class sample totals must not wrap signed int accumulation to a metadata match");

    QFile classStatsFile(registeredClassificationClassStatsPath(modelDir));
    check(classStatsFile.open(QIODevice::ReadOnly), "class stats JSON must be readable");
    const QJsonObject classStatsJson = classStatsFile.isOpen()
            ? QJsonDocument::fromJson(classStatsFile.readAll()).object()
            : QJsonObject();

    const auto checkInvalidClassStats = [&](const QJsonObject &mutatedClassStats,
                                            const char *message) {
        check(writeClassStatsJson(modelDir, mutatedClassStats),
              "mutated class stats fixture must write");
        const RegisteredClassificationModelPackageResult packageResult =
                validateRegisteredClassificationKnnPackage(modelDir);
        check(!packageResult.success, message);
        check(packageResult.status == QStringLiteral("invalid_class_stats"),
              "malformed class stats must report actionable contract status");
    };
    const auto checkMissingClassStatsField = [&](const QString &field) {
        QJsonObject missingFieldClassStats = classStatsJson;
        missingFieldClassStats.remove(field);
        checkInvalidClassStats(missingFieldClassStats,
                               "missing top-level class stats field must invalidate the package");
    };
    checkMissingClassStatsField(QStringLiteral("schemaVersion"));
    checkMissingClassStatsField(QStringLiteral("featureVersion"));
    checkMissingClassStatsField(QStringLiteral("classes"));

    QJsonObject wrongSchemaType = classStatsJson;
    wrongSchemaType.insert(QStringLiteral("schemaVersion"), QStringLiteral("2"));
    checkInvalidClassStats(wrongSchemaType,
                           "string class stats schema version must invalidate the package");
    QJsonObject wrongFeatureType = classStatsJson;
    wrongFeatureType.insert(QStringLiteral("featureVersion"), 2);
    checkInvalidClassStats(wrongFeatureType,
                           "numeric class stats feature version must invalidate the package");
    QJsonObject wrongClassesType = classStatsJson;
    wrongClassesType.insert(QStringLiteral("classes"), QJsonObject());
    checkInvalidClassStats(wrongClassesType,
                           "object class stats classes field must invalidate the package");

    const auto checkMissingPerClassField = [&](const QString &field) {
        QJsonObject missingFieldClassStats = classStatsJson;
        QJsonArray classes = missingFieldClassStats.value(QStringLiteral("classes")).toArray();
        QJsonObject firstClass = classes.at(0).toObject();
        firstClass.remove(field);
        classes.replace(0, firstClass);
        missingFieldClassStats.insert(QStringLiteral("classes"), classes);
        checkInvalidClassStats(missingFieldClassStats,
                               "missing per-class stats field must invalidate the package");
    };
    checkMissingPerClassField(QStringLiteral("classId"));
    checkMissingPerClassField(QStringLiteral("sampleCount"));
    checkMissingPerClassField(QStringLiteral("radiusEnabled"));
    checkMissingPerClassField(QStringLiteral("radius"));
    checkMissingPerClassField(QStringLiteral("meanDistance"));
    checkMissingPerClassField(QStringLiteral("stdDevDistance"));
    checkMissingPerClassField(QStringLiteral("maxDistance"));

    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("classId"), QStringLiteral("0")),
                           "string class id must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("sampleCount"), true),
                           "boolean sample count must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("radiusEnabled"), QStringLiteral("true")),
                           "string radius enabled flag must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("radius"), false),
                           "boolean radius must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("meanDistance"), QStringLiteral("0.2")),
                           "string mean distance must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("stdDevDistance"), QJsonObject()),
                           "object standard deviation must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("maxDistance"), QJsonArray()),
                           "array max distance must invalidate the package");

    QJsonObject nonObjectClass = classStatsJson;
    QJsonArray nonObjectClasses = nonObjectClass.value(QStringLiteral("classes")).toArray();
    nonObjectClasses.replace(0, QStringLiteral("not an object"));
    nonObjectClass.insert(QStringLiteral("classes"), nonObjectClasses);
    checkInvalidClassStats(nonObjectClass,
                           "non-object class stats entry must invalidate the package");

    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("classId"), 0.5),
                           "non-integral class id must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("sampleCount"), 3.5),
                           "non-integral sample count must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("sampleCount"), 0),
                           "zero sample count must invalidate the package");
    const QStringList distanceFields = {
        QStringLiteral("radius"),
        QStringLiteral("meanDistance"),
        QStringLiteral("stdDevDistance"),
        QStringLiteral("maxDistance")
    };
    for (const QString &field : distanceFields) {
        checkInvalidClassStats(classStatsWithFirstClassField(classStatsJson, field, -0.01),
                               "negative class stats distance must invalidate the package");
    }
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("radius"), 0.09),
                           "enabled radius below minimum must invalidate the package");
    checkInvalidClassStats(classStatsWithFirstClassField(
                               classStatsJson, QStringLiteral("radius"), 2.01),
                           "enabled radius above maximum must invalidate the package");

    QJsonObject duplicateClassIds = classStatsJson;
    QJsonArray duplicateClasses = duplicateClassIds.value(QStringLiteral("classes")).toArray();
    QJsonObject secondClass = duplicateClasses.at(1).toObject();
    secondClass.insert(QStringLiteral("classId"), 0);
    duplicateClasses.replace(1, secondClass);
    duplicateClassIds.insert(QStringLiteral("classes"), duplicateClasses);
    checkInvalidClassStats(duplicateClassIds,
                           "duplicate class ids must invalidate the package");

    QJsonObject unsupportedSchema = classStatsJson;
    unsupportedSchema.insert(QStringLiteral("schemaVersion"), 3);
    check(writeClassStatsJson(modelDir, unsupportedSchema),
          "unsupported class stats schema fixture must write");
    const RegisteredClassificationModelPackageResult unsupportedSchemaResult =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!unsupportedSchemaResult.success,
          "unsupported class stats schema must invalidate the package");
    check(unsupportedSchemaResult.status == QStringLiteral("unsupported_schema_version"),
          "unsupported class stats schema must retain its existing status");

    QJsonObject unsupportedFeature = classStatsJson;
    unsupportedFeature.insert(QStringLiteral("featureVersion"), QStringLiteral("other_feature"));
    check(writeClassStatsJson(modelDir, unsupportedFeature),
          "unsupported class stats feature fixture must write");
    const RegisteredClassificationModelPackageResult unsupportedFeatureResult =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!unsupportedFeatureResult.success,
          "unsupported class stats feature must invalidate the package");
    check(unsupportedFeatureResult.status == QStringLiteral("unsupported_feature_version"),
          "unsupported class stats feature must retain its existing status");

    check(writeClassStatsJson(modelDir, classStatsJson),
          "valid class stats fixture must restore after mutation checks");

    const auto checkMissingFixedContractField = [&](const QString &section,
                                                    const QString &field) {
        QJsonObject missingFieldMetadata = metadataJson;
        QJsonObject sectionJson = missingFieldMetadata.value(section).toObject();
        sectionJson.remove(field);
        missingFieldMetadata.insert(section, sectionJson);
        check(writeMetadataJson(modelDir, missingFieldMetadata),
              "missing fixed contract field fixture must write");
        const RegisteredClassificationModelPackageResult missingFieldPackage =
                validateRegisteredClassificationKnnPackage(modelDir);
        check(!missingFieldPackage.success,
              "missing fixed KNN/fusion/rejection field must invalidate the package");
        check(missingFieldPackage.status == QStringLiteral("invalid_knn_parameters"),
              "missing fixed contract field must report actionable parameter status");
    };
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("method"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("normalization"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("numTrees"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("numChecks"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("epsilon"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("sampleWeight"));
    checkMissingFixedContractField(QStringLiteral("knn"), QStringLiteral("centerWeight"));
    checkMissingFixedContractField(QStringLiteral("thresholds"), QStringLiteral("minSimilarity"));
    checkMissingFixedContractField(QStringLiteral("thresholds"), QStringLiteral("minMargin"));

    QJsonObject malformedNumberMetadata = metadataJson;
    QJsonObject malformedNumberKnn = malformedNumberMetadata.value(QStringLiteral("knn")).toObject();
    malformedNumberKnn.insert(QStringLiteral("numTrees"), QStringLiteral("4"));
    malformedNumberMetadata.insert(QStringLiteral("knn"), malformedNumberKnn);
    check(writeMetadataJson(modelDir, malformedNumberMetadata),
          "malformed KNN number fixture must write");
    const RegisteredClassificationModelPackageResult malformedNumberPackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!malformedNumberPackage.success,
          "string KNN number must invalidate the package");
    check(malformedNumberPackage.status == QStringLiteral("invalid_knn_parameters"),
          "string KNN number must report actionable parameter status");

    QJsonObject malformedBoolMetadata = metadataJson;
    QJsonObject malformedBoolKnn = malformedBoolMetadata.value(QStringLiteral("knn")).toObject();
    malformedBoolKnn.insert(QStringLiteral("normalization"), QStringLiteral("false"));
    malformedBoolMetadata.insert(QStringLiteral("knn"), malformedBoolKnn);
    check(writeMetadataJson(modelDir, malformedBoolMetadata),
          "malformed KNN boolean fixture must write");
    const RegisteredClassificationModelPackageResult malformedBoolPackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!malformedBoolPackage.success,
          "string KNN boolean must invalidate the package");
    check(malformedBoolPackage.status == QStringLiteral("invalid_knn_parameters"),
          "string KNN boolean must report actionable parameter status");

    QJsonObject malformedThresholdMetadata = metadataJson;
    QJsonObject malformedThresholds = malformedThresholdMetadata.value(
            QStringLiteral("thresholds")).toObject();
    malformedThresholds.insert(QStringLiteral("minSimilarity"), QStringLiteral("80"));
    malformedThresholdMetadata.insert(QStringLiteral("thresholds"), malformedThresholds);
    check(writeMetadataJson(modelDir, malformedThresholdMetadata),
          "malformed threshold number fixture must write");
    const RegisteredClassificationModelPackageResult malformedThresholdPackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!malformedThresholdPackage.success,
          "string rejection threshold must invalidate the package");
    check(malformedThresholdPackage.status == QStringLiteral("invalid_knn_parameters"),
          "string rejection threshold must report actionable parameter status");

    QJsonObject alteredMethodMetadata = metadataJson;
    QJsonObject alteredMethodKnn = alteredMethodMetadata.value(QStringLiteral("knn")).toObject();
    alteredMethodKnn.insert(QStringLiteral("method"), QStringLiteral("nearest_neighbor"));
    alteredMethodMetadata.insert(QStringLiteral("knn"), alteredMethodKnn);
    check(writeMetadataJson(modelDir, alteredMethodMetadata),
          "altered KNN method fixture must write");
    const RegisteredClassificationModelPackageResult alteredMethodPackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!alteredMethodPackage.success,
          "altered KNN method must invalidate the package");
    check(alteredMethodPackage.status == QStringLiteral("invalid_knn_parameters"),
          "altered KNN method must report actionable parameter status");

    QJsonObject alteredNormalizationMetadata = metadataJson;
    QJsonObject alteredNormalizationKnn = alteredNormalizationMetadata.value(QStringLiteral("knn")).toObject();
    alteredNormalizationKnn.insert(QStringLiteral("normalization"), true);
    alteredNormalizationMetadata.insert(QStringLiteral("knn"), alteredNormalizationKnn);
    check(writeMetadataJson(modelDir, alteredNormalizationMetadata),
          "altered KNN normalization fixture must write");
    const RegisteredClassificationModelPackageResult alteredNormalizationPackage =
            validateRegisteredClassificationKnnPackage(modelDir);
    check(!alteredNormalizationPackage.success,
          "altered KNN normalization must invalidate the package");
    check(alteredNormalizationPackage.status == QStringLiteral("invalid_knn_parameters"),
          "altered KNN normalization must report actionable parameter status");

    const QString legacyDir = smokeModelDir(QStringLiteral("legacy"));
    const QJsonObject legacyMetadata{
        {QStringLiteral("modelType"), QStringLiteral("halcon_mlp_registered_classification")},
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("featureVersion"), QStringLiteral("halcon_mlp_roi_stats_v1")},
        {QStringLiteral("classLabels"), QJsonArray{
             QJsonObject{{QStringLiteral("id"), 0}, {QStringLiteral("name"), QStringLiteral("A")} },
             QJsonObject{{QStringLiteral("id"), 1}, {QStringLiteral("name"), QStringLiteral("B")} }}},
        {QStringLiteral("trainingSampleCount"), 4}
    };
    check(writeMetadataJson(legacyDir, legacyMetadata),
          "schema 1 metadata fixture must write");
    RegisteredClassificationKnnModelMetadata legacyAsKnn;
    const RegisteredClassificationModelPackageResult legacyRead =
            readRegisteredClassificationKnnMetadata(legacyDir, &legacyAsKnn);
    check(!legacyRead.success, "schema 1 metadata must not pass the V2 reader");
    check(legacyRead.status == QStringLiteral("legacy_model_requires_retraining"),
          "schema 1 V2 read must require retraining");
    const RegisteredClassificationModelInspection legacyInspection =
            inspectRegisteredClassificationModelPackage(legacyDir);
    check(legacyInspection.success, "legacy package inspection must parse metadata");
    check(legacyInspection.legacy, "schema 1 package must be marked legacy");
    check(!legacyInspection.runnable, "schema 1 package must not be runnable");
    check(legacyInspection.status == QStringLiteral("legacy_model_requires_retraining"),
          "schema 1 inspection must require retraining");
    check(legacyInspection.classCount == 2 && legacyInspection.trainingSampleCount == 4,
          "legacy inspection must preserve class and sample counts");

    if (g_failures != 0)
        return 1;

    std::cout << "registered_classification_feature_v2_smoke: feature contract and schema 2 package checks passed"
              << std::endl;
    return 0;
}
