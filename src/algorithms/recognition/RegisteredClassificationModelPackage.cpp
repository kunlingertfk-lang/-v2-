#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <cmath>
#include <limits>

namespace {

RegisteredClassificationModelPackageResult result(bool success,
                                                  const QString &status,
                                                  const QString &message)
{
    RegisteredClassificationModelPackageResult r;
    r.success = success;
    r.status = status;
    r.message = message;
    r.payload.insert(QStringLiteral("errorCode"), status);
    r.payload.insert(QStringLiteral("errorMessage"), message);
    return r;
}

QJsonArray classLabelsToJson(const QVector<RegisteredClassificationClassLabel> &labels)
{
    QJsonArray array;
    for (const RegisteredClassificationClassLabel &label : labels) {
        QJsonObject item;
        item.insert(QStringLiteral("id"), label.id);
        item.insert(QStringLiteral("name"), label.name);
        array.append(item);
    }
    return array;
}

QVector<RegisteredClassificationClassLabel> classLabelsFromJson(const QJsonArray &array)
{
    QVector<RegisteredClassificationClassLabel> labels;
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        RegisteredClassificationClassLabel label;
        label.id = object.value(QStringLiteral("id")).toInt(-1);
        label.name = object.value(QStringLiteral("name")).toString();
        labels.append(label);
    }
    return labels;
}

QJsonArray stringListToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values)
        array.append(value);
    return array;
}

QStringList stringListFromJson(const QJsonArray &array)
{
    QStringList values;
    for (const QJsonValue &value : array)
        values.append(value.toString());
    return values;
}

bool isFiniteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool isJsonInteger(const QJsonValue &value)
{
    if (!value.isDouble())
        return false;

    const double number = value.toDouble();
    return std::isfinite(number)
            && std::floor(number) == number
            && number >= static_cast<double>(std::numeric_limits<int>::min())
            && number <= static_cast<double>(std::numeric_limits<int>::max());
}

bool hasExactKeys(const QJsonObject &object, const QStringList &keys)
{
    return object.keys() == keys;
}

bool isExactJsonInteger(const QJsonValue &value, int expected)
{
    return isJsonInteger(value) && static_cast<int>(value.toDouble()) == expected;
}

bool isExactJsonDouble(const QJsonValue &value, double expected)
{
    return value.isDouble() && std::isfinite(value.toDouble())
            && value.toDouble() == expected;
}

RegisteredClassificationModelPackageResult readJsonObject(const QString &path,
                                                           QJsonObject *object)
{
    QFile file(path);
    if (!file.exists()) {
        return result(false, QStringLiteral("model_file_not_found"),
                      QStringLiteral("%1 does not exist.").arg(QFileInfo(path).fileName()));
    }
    if (!file.open(QIODevice::ReadOnly))
        return result(false, QStringLiteral("model_file_not_found"), file.errorString());

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return result(false, QStringLiteral("model_file_invalid"),
                      parseError.errorString());
    }
    *object = document.object();
    return result(true, QStringLiteral("ok"), QStringLiteral("json read"));
}

RegisteredClassificationModelPackageResult validateKnnContractJson(const QJsonObject &root)
{
    const QJsonValue knnValue = root.value(QStringLiteral("knn"));
    const QJsonValue thresholdsValue = root.value(QStringLiteral("thresholds"));
    if (!knnValue.isObject() || !thresholdsValue.isObject()) {
        return result(false, QStringLiteral("invalid_knn_parameters"),
                      QStringLiteral("Schema 2 metadata must explicitly persist KNN and threshold objects."));
    }

    const QJsonObject knn = knnValue.toObject();
    const QJsonObject thresholds = thresholdsValue.toObject();
    const bool validKnn = hasExactKeys(knn, {
                                  QStringLiteral("centerWeight"),
                                  QStringLiteral("epsilon"),
                                  QStringLiteral("method"),
                                  QStringLiteral("normalization"),
                                  QStringLiteral("numChecks"),
                                  QStringLiteral("numTrees"),
                                  QStringLiteral("sampleWeight")})
            && hasExactKeys(thresholds, {
                                QStringLiteral("minMargin"),
                                QStringLiteral("minSimilarity")})
            && knn.value(QStringLiteral("method")).isString()
            && knn.value(QStringLiteral("method")).toString() == QStringLiteral("classes_distance")
            && knn.value(QStringLiteral("normalization")).isBool()
            && !knn.value(QStringLiteral("normalization")).toBool()
            && isExactJsonInteger(knn.value(QStringLiteral("numTrees")), 4)
            && isExactJsonInteger(knn.value(QStringLiteral("numChecks")), 0)
            && isExactJsonDouble(knn.value(QStringLiteral("epsilon")), 0.0)
            && isExactJsonDouble(knn.value(QStringLiteral("sampleWeight")), 0.70)
            && isExactJsonDouble(knn.value(QStringLiteral("centerWeight")), 0.30)
            && isExactJsonInteger(thresholds.value(QStringLiteral("minSimilarity")), 80)
            && isExactJsonInteger(thresholds.value(QStringLiteral("minMargin")), 8);
    if (!validKnn) {
        return result(false, QStringLiteral("invalid_knn_parameters"),
                      QStringLiteral("Schema 2 metadata must explicitly persist the fixed KNN, "
                                     "fusion, and rejection contract values with JSON types."));
    }
    return result(true, QStringLiteral("ok"), QStringLiteral("KNN JSON contract is valid"));
}

RegisteredClassificationModelPackageResult validateKnnMetadataRootJson(const QJsonObject &root)
{
    const QJsonValue modelType = root.value(QStringLiteral("modelType"));
    const QJsonValue schemaVersion = root.value(QStringLiteral("schemaVersion"));
    const QJsonValue featureVersion = root.value(QStringLiteral("featureVersion"));
    const QJsonValue halconVersion = root.value(QStringLiteral("halconVersion"));
    const QJsonValue classLabels = root.value(QStringLiteral("classLabels"));
    const QJsonValue featureNames = root.value(QStringLiteral("featureNames"));
    const QJsonValue featureLength = root.value(QStringLiteral("featureLength"));
    const QJsonValue segmentation = root.value(QStringLiteral("segmentation"));
    const QJsonValue canonicalization = root.value(QStringLiteral("canonicalization"));
    const QJsonValue featureGroups = root.value(QStringLiteral("featureGroups"));
    const QJsonValue trainingSampleCount = root.value(QStringLiteral("trainingSampleCount"));

    if (!modelType.isString() || !isJsonInteger(schemaVersion)
            || !featureVersion.isString() || !halconVersion.isString()
            || halconVersion.toString().trimmed().isEmpty()
            || !classLabels.isArray() || !featureNames.isArray()
            || !isJsonInteger(featureLength) || !segmentation.isObject()
            || !canonicalization.isObject() || !featureGroups.isObject()
            || !trainingSampleCount.isDouble() || !isJsonInteger(trainingSampleCount)) {
        return result(false, QStringLiteral("invalid_metadata_contract"),
                      QStringLiteral("Schema 2 metadata root fields are missing, empty, or use incorrect JSON types."));
    }

    const QJsonArray labels = classLabels.toArray();
    for (int index = 0; index < labels.size(); ++index) {
        if (!labels.at(index).isObject()) {
            return result(false, QStringLiteral("invalid_class_label"),
                          QStringLiteral("classLabels[%1] must be a JSON object.").arg(index));
        }
        const QJsonObject label = labels.at(index).toObject();
        const QJsonValue id = label.value(QStringLiteral("id"));
        const QJsonValue name = label.value(QStringLiteral("name"));
        if (!isJsonInteger(id) || !name.isString() || name.toString().trimmed().isEmpty()) {
            return result(false, QStringLiteral("invalid_class_label"),
                          QStringLiteral("classLabels[%1] requires an integer id and non-empty string name.")
                                  .arg(index));
        }
    }

    const QJsonArray names = featureNames.toArray();
    for (int index = 0; index < names.size(); ++index) {
        if (!names.at(index).isString()) {
            return result(false, QStringLiteral("invalid_feature_contract"),
                          QStringLiteral("featureNames[%1] must be a JSON string.").arg(index));
        }
    }

    if (segmentation.toObject() != registeredClassificationSegmentationContractV2()) {
        return result(false, QStringLiteral("invalid_segmentation_contract"),
                      QStringLiteral("segmentation must exactly match the fixed V2 contract."));
    }
    if (canonicalization.toObject() != registeredClassificationCanonicalizationContractV2()) {
        return result(false, QStringLiteral("invalid_canonicalization_contract"),
                      QStringLiteral("canonicalization must exactly match the fixed V2 contract."));
    }
    if (featureGroups.toObject() != registeredClassificationFeatureGroupsContractV2()) {
        return result(false, QStringLiteral("invalid_feature_groups_contract"),
                      QStringLiteral("featureGroups must exactly match the fixed V2 contract."));
    }
    return validateKnnContractJson(root);
}

RegisteredClassificationKnnModelMetadata knnMetadataFromJson(const QJsonObject &root)
{
    RegisteredClassificationKnnModelMetadata loaded;
    loaded.modelType = root.value(QStringLiteral("modelType")).toString();
    loaded.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(0);
    loaded.featureVersion = root.value(QStringLiteral("featureVersion")).toString();
    loaded.halconVersion = root.value(QStringLiteral("halconVersion")).toString();
    loaded.classLabels = classLabelsFromJson(root.value(QStringLiteral("classLabels")).toArray());
    loaded.featureNames = stringListFromJson(root.value(QStringLiteral("featureNames")).toArray());
    loaded.featureLength = root.value(QStringLiteral("featureLength")).toInt(0);
    loaded.segmentation = root.value(QStringLiteral("segmentation")).toObject();
    loaded.canonicalization = root.value(QStringLiteral("canonicalization")).toObject();
    loaded.featureGroups = root.value(QStringLiteral("featureGroups")).toObject();
    loaded.trainingSampleCount = root.value(QStringLiteral("trainingSampleCount")).toInt(0);

    const QJsonObject knn = root.value(QStringLiteral("knn")).toObject();
    loaded.knn.method = knn.value(QStringLiteral("method")).toString();
    loaded.knn.normalization = knn.value(QStringLiteral("normalization")).toBool(true);
    loaded.knn.numTrees = knn.value(QStringLiteral("numTrees")).toInt(4);
    loaded.knn.numChecks = knn.value(QStringLiteral("numChecks")).toInt(0);
    loaded.knn.epsilon = knn.value(QStringLiteral("epsilon")).toDouble(0.0);
    loaded.knn.sampleWeight = knn.value(QStringLiteral("sampleWeight")).toDouble(0.70);
    loaded.knn.centerWeight = knn.value(QStringLiteral("centerWeight")).toDouble(0.30);

    const QJsonObject thresholds = root.value(QStringLiteral("thresholds")).toObject();
    loaded.thresholds.minSimilarity = thresholds.value(QStringLiteral("minSimilarity")).toInt(80);
    loaded.thresholds.minMargin = thresholds.value(QStringLiteral("minMargin")).toInt(8);
    return loaded;
}

QJsonObject knnMetadataToJson(const RegisteredClassificationKnnModelMetadata &metadata)
{
    QJsonObject root;
    root.insert(QStringLiteral("modelType"), metadata.modelType);
    root.insert(QStringLiteral("schemaVersion"), metadata.schemaVersion);
    root.insert(QStringLiteral("featureVersion"), metadata.featureVersion);
    root.insert(QStringLiteral("halconVersion"), metadata.halconVersion);
    root.insert(QStringLiteral("classLabels"), classLabelsToJson(metadata.classLabels));
    root.insert(QStringLiteral("featureNames"), stringListToJson(metadata.featureNames));
    root.insert(QStringLiteral("featureLength"), metadata.featureLength);
    root.insert(QStringLiteral("segmentation"), metadata.segmentation);
    root.insert(QStringLiteral("canonicalization"), metadata.canonicalization);
    root.insert(QStringLiteral("featureGroups"), metadata.featureGroups);
    root.insert(QStringLiteral("trainingSampleCount"), metadata.trainingSampleCount);

    QJsonObject knn;
    knn.insert(QStringLiteral("method"), metadata.knn.method);
    knn.insert(QStringLiteral("normalization"), metadata.knn.normalization);
    knn.insert(QStringLiteral("numTrees"), metadata.knn.numTrees);
    knn.insert(QStringLiteral("numChecks"), metadata.knn.numChecks);
    knn.insert(QStringLiteral("epsilon"), metadata.knn.epsilon);
    knn.insert(QStringLiteral("sampleWeight"), metadata.knn.sampleWeight);
    knn.insert(QStringLiteral("centerWeight"), metadata.knn.centerWeight);
    root.insert(QStringLiteral("knn"), knn);

    QJsonObject thresholds;
    thresholds.insert(QStringLiteral("minSimilarity"), metadata.thresholds.minSimilarity);
    thresholds.insert(QStringLiteral("minMargin"), metadata.thresholds.minMargin);
    root.insert(QStringLiteral("thresholds"), thresholds);
    return root;
}

RegisteredClassificationModelPackageResult classStatsFromJson(
        const QJsonObject &root,
        RegisteredClassificationClassStatsDocument *document)
{
    if (!document) {
        return result(false, QStringLiteral("invalid_argument"),
                      QStringLiteral("class stats output is null."));
    }

    const QJsonValue schemaVersion = root.value(QStringLiteral("schemaVersion"));
    if (!isJsonInteger(schemaVersion)) {
        return result(false, QStringLiteral("invalid_class_stats"),
                      QStringLiteral("Class stats schemaVersion must be an integer JSON number."));
    }
    const QJsonValue featureVersion = root.value(QStringLiteral("featureVersion"));
    if (!featureVersion.isString()) {
        return result(false, QStringLiteral("invalid_class_stats"),
                      QStringLiteral("Class stats featureVersion must be a JSON string."));
    }
    const QJsonValue classesValue = root.value(QStringLiteral("classes"));
    if (!classesValue.isArray()) {
        return result(false, QStringLiteral("invalid_class_stats"),
                      QStringLiteral("Class stats classes must be a JSON array."));
    }

    RegisteredClassificationClassStatsDocument loaded;
    loaded.schemaVersion = static_cast<int>(schemaVersion.toDouble());
    loaded.featureVersion = featureVersion.toString();
    const QJsonArray classes = classesValue.toArray();
    for (const QJsonValue &value : classes) {
        const int classIndex = loaded.classes.size();
        if (!value.isObject()) {
            return result(false, QStringLiteral("invalid_class_stats"),
                          QStringLiteral("Class stats entry %1 must be a JSON object.")
                                  .arg(classIndex));
        }
        const QJsonObject object = value.toObject();
        const QJsonValue classId = object.value(QStringLiteral("classId"));
        const QJsonValue sampleCount = object.value(QStringLiteral("sampleCount"));
        const QJsonValue radiusEnabled = object.value(QStringLiteral("radiusEnabled"));
        const QJsonValue radius = object.value(QStringLiteral("radius"));
        const QJsonValue meanDistance = object.value(QStringLiteral("meanDistance"));
        const QJsonValue stdDevDistance = object.value(QStringLiteral("stdDevDistance"));
        const QJsonValue maxDistance = object.value(QStringLiteral("maxDistance"));
        if (!isJsonInteger(classId) || !isJsonInteger(sampleCount)
                || !radiusEnabled.isBool()
                || !radius.isDouble() || !isFiniteNonNegative(radius.toDouble())
                || !meanDistance.isDouble() || !isFiniteNonNegative(meanDistance.toDouble())
                || !stdDevDistance.isDouble() || !isFiniteNonNegative(stdDevDistance.toDouble())
                || !maxDistance.isDouble() || !isFiniteNonNegative(maxDistance.toDouble())) {
            return result(false, QStringLiteral("invalid_class_stats"),
                          QStringLiteral("Class stats entry %1 has missing or invalid contract fields.")
                                  .arg(classIndex));
        }

        RegisteredClassificationClassStats stats;
        stats.classId = static_cast<int>(classId.toDouble());
        stats.sampleCount = static_cast<int>(sampleCount.toDouble());
        stats.radiusEnabled = radiusEnabled == QJsonValue(true);
        stats.radius = radius.toDouble();
        stats.meanDistance = meanDistance.toDouble();
        stats.stdDevDistance = stdDevDistance.toDouble();
        stats.maxDistance = maxDistance.toDouble();
        loaded.classes.append(stats);
    }
    *document = loaded;
    return result(true, QStringLiteral("ok"), QStringLiteral("class stats JSON contract is valid"));
}

QJsonObject classStatsToJson(const RegisteredClassificationClassStatsDocument &document)
{
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), document.schemaVersion);
    root.insert(QStringLiteral("featureVersion"), document.featureVersion);
    QJsonArray classes;
    for (const RegisteredClassificationClassStats &stats : document.classes) {
        QJsonObject object;
        object.insert(QStringLiteral("classId"), stats.classId);
        object.insert(QStringLiteral("sampleCount"), stats.sampleCount);
        object.insert(QStringLiteral("radiusEnabled"), stats.radiusEnabled);
        object.insert(QStringLiteral("radius"), stats.radius);
        object.insert(QStringLiteral("meanDistance"), stats.meanDistance);
        object.insert(QStringLiteral("stdDevDistance"), stats.stdDevDistance);
        object.insert(QStringLiteral("maxDistance"), stats.maxDistance);
        classes.append(object);
    }
    root.insert(QStringLiteral("classes"), classes);
    return root;
}

RegisteredClassificationModelPackageResult validateClassStatsDocument(
        const RegisteredClassificationClassStatsDocument &document)
{
    if (document.schemaVersion != 2) {
        return result(false, QStringLiteral("unsupported_schema_version"),
                      QStringLiteral("Only schemaVersion=2 class stats are supported."));
    }
    if (document.featureVersion != registeredClassificationFeatureVersionV2()) {
        return result(false, QStringLiteral("unsupported_feature_version"),
                      QStringLiteral("Class stats must use halcon_registered_feature_v2."));
    }
    if (document.classes.isEmpty()) {
        return result(false, QStringLiteral("class_stats_empty"),
                      QStringLiteral("Class stats must contain at least one class."));
    }

    QSet<int> classIds;
    for (const RegisteredClassificationClassStats &stats : document.classes) {
        if (stats.classId < 0 || stats.sampleCount <= 0 || classIds.contains(stats.classId)) {
            return result(false, QStringLiteral("invalid_class_stats"),
                          QStringLiteral("Class stats require unique non-negative ids and samples."));
        }
        if (!isFiniteNonNegative(stats.radius)
                || !isFiniteNonNegative(stats.meanDistance)
                || !isFiniteNonNegative(stats.stdDevDistance)
                || !isFiniteNonNegative(stats.maxDistance)) {
            return result(false, QStringLiteral("invalid_class_stats"),
                          QStringLiteral("Class stats distances must be finite and non-negative."));
        }
        if (stats.sampleCount < 3 && (stats.radiusEnabled || stats.radius != 0.0)) {
            return result(false, QStringLiteral("invalid_class_stats"),
                          QStringLiteral("Classes below three samples require radiusEnabled=false and radius=0."));
        }
        if (stats.sampleCount >= 3 && !stats.radiusEnabled) {
            return result(false, QStringLiteral("invalid_class_stats"),
                          QStringLiteral("Classes with three or more samples require radiusEnabled=true."));
        }
        if (stats.radiusEnabled && (stats.radius < 0.10 || stats.radius > 2.00)) {
            return result(false, QStringLiteral("invalid_class_stats"),
                          QStringLiteral("Enabled class radius must be within [0.10, 2.00]."));
        }
        classIds.insert(stats.classId);
    }
    return result(true, QStringLiteral("ok"), QStringLiteral("class stats are valid"));
}

} // namespace

QString registeredClassificationKnnModelType()
{
    return QStringLiteral("halcon_knn_registered_classification");
}

QString registeredClassificationLegacyMlpModelType()
{
    return QStringLiteral("halcon_mlp_registered_classification");
}

QJsonObject registeredClassificationSegmentationContractV2()
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

QJsonObject registeredClassificationCanonicalizationContractV2()
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

QJsonObject registeredClassificationFeatureGroupsContractV2()
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

QString registeredClassificationMetadataPath(const QString &modelDir)
{
    return QDir(modelDir).filePath(QStringLiteral("metadata.json"));
}

QString registeredClassificationLegacyModelPath(const QString &modelDir)
{
    return QDir(modelDir).filePath(QStringLiteral("model.gmc"));
}

QString registeredClassificationTrainingReportPath(const QString &modelDir)
{
    return QDir(modelDir).filePath(QStringLiteral("training_report.json"));
}

QString registeredClassificationSampleKnnPath(const QString &modelDir)
{
    return QDir(modelDir).filePath(QStringLiteral("model.gnc"));
}

QString registeredClassificationCenterKnnPath(const QString &modelDir)
{
    return QDir(modelDir).filePath(QStringLiteral("class_centers.gnc"));
}

QString registeredClassificationClassStatsPath(const QString &modelDir)
{
    return QDir(modelDir).filePath(QStringLiteral("class_stats.json"));
}

RegisteredClassificationModelPackageResult validateRegisteredClassificationKnnMetadata(
        const RegisteredClassificationKnnModelMetadata &metadata)
{
    if (metadata.modelType != registeredClassificationKnnModelType()) {
        return result(false, QStringLiteral("unsupported_model_type"),
                      QStringLiteral("Only halcon_knn_registered_classification is supported."));
    }
    if (metadata.schemaVersion != 2) {
        return result(false, QStringLiteral("unsupported_schema_version"),
                      QStringLiteral("Only schemaVersion=2 is supported."));
    }
    if (metadata.featureVersion != registeredClassificationFeatureVersionV2()) {
        return result(false, QStringLiteral("unsupported_feature_version"),
                      QStringLiteral("Only halcon_registered_feature_v2 is supported."));
    }
    if (metadata.halconVersion.trimmed().isEmpty()) {
        return result(false, QStringLiteral("invalid_halcon_version"),
                      QStringLiteral("halconVersion must be a non-empty runtime version string."));
    }
    if (metadata.featureNames != registeredClassificationFeatureNamesV2()) {
        return result(false, QStringLiteral("feature_contract_mismatch"),
                      QStringLiteral("featureNames must exactly match the V2 feature contract."));
    }
    if (metadata.featureLength != metadata.featureNames.size() || metadata.featureLength != 59) {
        return result(false, QStringLiteral("feature_length_mismatch"),
                      QStringLiteral("featureLength must be 59 and match featureNames."));
    }
    if (metadata.classLabels.size() < 2) {
        return result(false, QStringLiteral("training_not_enough_classes"),
                      QStringLiteral("At least two class labels are required."));
    }
    QSet<int> classIds;
    for (const RegisteredClassificationClassLabel &label : metadata.classLabels) {
        if (label.id < 0 || label.name.trimmed().isEmpty() || classIds.contains(label.id)) {
            return result(false, QStringLiteral("invalid_class_label"),
                          QStringLiteral("Class labels must have unique non-negative ids and names."));
        }
        classIds.insert(label.id);
    }
    if (metadata.trainingSampleCount <= 0) {
        return result(false, QStringLiteral("invalid_training_sample_count"),
                      QStringLiteral("trainingSampleCount must be positive."));
    }
    if (metadata.segmentation != registeredClassificationSegmentationContractV2()) {
        return result(false, QStringLiteral("invalid_segmentation_contract"),
                      QStringLiteral("segmentation must exactly match the fixed V2 contract."));
    }
    if (metadata.canonicalization != registeredClassificationCanonicalizationContractV2()) {
        return result(false, QStringLiteral("invalid_canonicalization_contract"),
                      QStringLiteral("canonicalization must exactly match the fixed V2 contract."));
    }
    if (metadata.featureGroups != registeredClassificationFeatureGroupsContractV2()) {
        return result(false, QStringLiteral("invalid_feature_groups_contract"),
                      QStringLiteral("featureGroups must exactly match the fixed V2 contract."));
    }
    if (metadata.knn.method != QStringLiteral("classes_distance")
            || metadata.knn.normalization
            || metadata.knn.numTrees != 4 || metadata.knn.numChecks != 0
            || metadata.knn.epsilon != 0.0
            || metadata.knn.sampleWeight != 0.70
            || metadata.knn.centerWeight != 0.30) {
        return result(false, QStringLiteral("invalid_knn_parameters"),
                      QStringLiteral("KNN method must be classes_distance, normalization must be false, "
                                     "and parameters must match the fixed V2 contract."));
    }
    if (metadata.thresholds.minSimilarity != 80 || metadata.thresholds.minMargin != 8) {
        return result(false, QStringLiteral("invalid_knn_parameters"),
                      QStringLiteral("KNN rejection thresholds must match the fixed V2 contract."));
    }
    return result(true, QStringLiteral("ok"), QStringLiteral("schema 2 metadata is valid"));
}

RegisteredClassificationModelPackageResult writeRegisteredClassificationKnnMetadata(
        const QString &modelDir,
        const RegisteredClassificationKnnModelMetadata &metadata)
{
    const RegisteredClassificationModelPackageResult validation =
            validateRegisteredClassificationKnnMetadata(metadata);
    if (!validation.success)
        return validation;

    QDir dir(modelDir);
    if (!dir.exists() && !QDir().mkpath(modelDir)) {
        return result(false, QStringLiteral("model_write_failed"),
                      QStringLiteral("Failed to create model directory."));
    }
    QFile file(registeredClassificationMetadataPath(modelDir));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return result(false, QStringLiteral("model_write_failed"), file.errorString());
    file.write(QJsonDocument(knnMetadataToJson(metadata)).toJson(QJsonDocument::Indented));
    return result(true, QStringLiteral("ok"), QStringLiteral("schema 2 metadata written"));
}

RegisteredClassificationModelPackageResult readRegisteredClassificationKnnMetadata(
        const QString &modelDir,
        RegisteredClassificationKnnModelMetadata *metadata)
{
    if (!metadata) {
        return result(false, QStringLiteral("invalid_argument"),
                      QStringLiteral("metadata output is null."));
    }

    QJsonObject root;
    const RegisteredClassificationModelPackageResult readResult =
            readJsonObject(registeredClassificationMetadataPath(modelDir), &root);
    if (!readResult.success)
        return readResult;
    if (root.value(QStringLiteral("schemaVersion")).toInt(0) == 1
            || root.value(QStringLiteral("modelType")).toString()
                    == registeredClassificationLegacyMlpModelType()) {
        return result(false, QStringLiteral("legacy_model_requires_retraining"),
                      QStringLiteral("Schema 1 MLP models must be retrained as V2 KNN models."));
    }

    const RegisteredClassificationModelPackageResult contractResult =
            validateKnnMetadataRootJson(root);
    if (!contractResult.success)
        return contractResult;

    const RegisteredClassificationKnnModelMetadata loaded = knnMetadataFromJson(root);
    const RegisteredClassificationModelPackageResult validation =
            validateRegisteredClassificationKnnMetadata(loaded);
    if (!validation.success)
        return validation;
    *metadata = loaded;
    return result(true, QStringLiteral("ok"), QStringLiteral("schema 2 metadata read"));
}

RegisteredClassificationModelPackageResult writeRegisteredClassificationClassStats(
        const QString &modelDir,
        const RegisteredClassificationClassStatsDocument &document)
{
    const RegisteredClassificationModelPackageResult validation =
            validateClassStatsDocument(document);
    if (!validation.success)
        return validation;

    QDir dir(modelDir);
    if (!dir.exists() && !QDir().mkpath(modelDir)) {
        return result(false, QStringLiteral("model_write_failed"),
                      QStringLiteral("Failed to create model directory."));
    }
    QFile file(registeredClassificationClassStatsPath(modelDir));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return result(false, QStringLiteral("model_write_failed"), file.errorString());
    file.write(QJsonDocument(classStatsToJson(document)).toJson(QJsonDocument::Indented));
    return result(true, QStringLiteral("ok"), QStringLiteral("class stats written"));
}

RegisteredClassificationModelPackageResult readRegisteredClassificationClassStats(
        const QString &modelDir,
        RegisteredClassificationClassStatsDocument *document)
{
    if (!document) {
        return result(false, QStringLiteral("invalid_argument"),
                      QStringLiteral("class stats output is null."));
    }

    QJsonObject root;
    const RegisteredClassificationModelPackageResult readResult =
            readJsonObject(registeredClassificationClassStatsPath(modelDir), &root);
    if (!readResult.success)
        return readResult;
    RegisteredClassificationClassStatsDocument loaded;
    const RegisteredClassificationModelPackageResult parseResult =
            classStatsFromJson(root, &loaded);
    if (!parseResult.success)
        return parseResult;
    const RegisteredClassificationModelPackageResult validation =
            validateClassStatsDocument(loaded);
    if (!validation.success)
        return validation;
    *document = loaded;
    return result(true, QStringLiteral("ok"), QStringLiteral("class stats read"));
}

RegisteredClassificationModelPackageResult validateRegisteredClassificationKnnPackage(
        const QString &modelDir)
{
    RegisteredClassificationKnnModelMetadata metadata;
    const RegisteredClassificationModelPackageResult metadataResult =
            readRegisteredClassificationKnnMetadata(modelDir, &metadata);
    if (!metadataResult.success)
        return metadataResult;

    const QStringList requiredKnnFiles = {
        registeredClassificationSampleKnnPath(modelDir),
        registeredClassificationCenterKnnPath(modelDir)
    };
    for (const QString &path : requiredKnnFiles) {
        const QFileInfo info(path);
        if (!info.exists() || !info.isFile() || info.size() <= 0) {
            return result(false, QStringLiteral("model_package_incomplete"),
                          QStringLiteral("Required KNN model file is missing, not regular, or empty: %1.")
                                  .arg(info.fileName()));
        }
    }

    const QFileInfo classStatsInfo(registeredClassificationClassStatsPath(modelDir));
    if (!classStatsInfo.exists() || !classStatsInfo.isFile()) {
        return result(false, QStringLiteral("model_package_incomplete"),
                      QStringLiteral("Required model package file is missing: %1.")
                              .arg(classStatsInfo.fileName()));
    }

    RegisteredClassificationClassStatsDocument stats;
    const RegisteredClassificationModelPackageResult statsResult =
            readRegisteredClassificationClassStats(modelDir, &stats);
    if (!statsResult.success)
        return statsResult;
    if (stats.featureVersion != metadata.featureVersion
            || stats.classes.size() != metadata.classLabels.size()) {
        return result(false, QStringLiteral("class_stats_mismatch"),
                      QStringLiteral("Class stats do not match the schema 2 metadata."));
    }

    QSet<int> metadataClassIds;
    for (const RegisteredClassificationClassLabel &label : metadata.classLabels)
        metadataClassIds.insert(label.id);
    qint64 sampleCount = 0;
    for (const RegisteredClassificationClassStats &classStats : stats.classes) {
        if (!metadataClassIds.contains(classStats.classId)) {
            return result(false, QStringLiteral("class_stats_mismatch"),
                          QStringLiteral("Class stats include an unknown class id."));
        }
        const qint64 classSampleCount = static_cast<qint64>(classStats.sampleCount);
        if (classSampleCount > std::numeric_limits<qint64>::max() - sampleCount) {
            return result(false, QStringLiteral("invalid_class_stats_overflow"),
                          QStringLiteral("Class stats sample total exceeds the supported range."));
        }
        sampleCount += classSampleCount;
    }
    if (sampleCount != static_cast<qint64>(metadata.trainingSampleCount)) {
        return result(false, QStringLiteral("class_stats_mismatch"),
                      QStringLiteral("Class stats sample count does not match metadata."));
    }
    return result(true, QStringLiteral("ok"), QStringLiteral("schema 2 model package is complete"));
}

RegisteredClassificationModelInspection inspectRegisteredClassificationModelPackage(
        const QString &modelDir)
{
    RegisteredClassificationModelInspection inspection;
    inspection.hasTrainingSession = QFileInfo(
            QDir(modelDir).filePath(QStringLiteral("training_session/session.json"))).exists();

    const QFileInfo metadataInfo(registeredClassificationMetadataPath(modelDir));
    const QFileInfo legacyModelInfo(registeredClassificationLegacyModelPath(modelDir));
    if (!metadataInfo.isFile() && legacyModelInfo.isFile()) {
        inspection.success = true;
        inspection.runnable = false;
        inspection.legacy = true;
        inspection.modelType = registeredClassificationLegacyMlpModelType();
        inspection.featureVersion = QStringLiteral("halcon_mlp_roi_stats_v1");
        inspection.status = QStringLiteral("legacy_model_requires_retraining");
        inspection.message = QStringLiteral("Legacy model.gmc requires V2 KNN retraining.");
        return inspection;
    }

    QJsonObject root;
    const RegisteredClassificationModelPackageResult readResult =
            readJsonObject(registeredClassificationMetadataPath(modelDir), &root);
    if (!readResult.success) {
        inspection.status = readResult.status;
        inspection.message = readResult.message;
        return inspection;
    }

    inspection.modelType = root.value(QStringLiteral("modelType")).toString();
    inspection.featureVersion = root.value(QStringLiteral("featureVersion")).toString();
    inspection.classCount = root.value(QStringLiteral("classLabels")).toArray().size();
    inspection.trainingSampleCount = root.value(QStringLiteral("trainingSampleCount")).toInt(0);

    if (root.value(QStringLiteral("schemaVersion")).toInt(0) == 1
            || inspection.modelType == registeredClassificationLegacyMlpModelType()) {
        inspection.success = true;
        inspection.legacy = true;
        inspection.status = QStringLiteral("legacy_model_requires_retraining");
        inspection.message = QStringLiteral("Schema 1 MLP model requires V2 KNN retraining.");
        return inspection;
    }

    RegisteredClassificationKnnModelMetadata metadata;
    const RegisteredClassificationModelPackageResult metadataResult =
            readRegisteredClassificationKnnMetadata(modelDir, &metadata);
    if (!metadataResult.success) {
        inspection.status = metadataResult.status;
        inspection.message = metadataResult.message;
        return inspection;
    }

    inspection.success = true;
    inspection.metadata = metadata;
    inspection.modelType = metadata.modelType;
    inspection.featureVersion = metadata.featureVersion;
    inspection.classCount = metadata.classLabels.size();
    inspection.trainingSampleCount = metadata.trainingSampleCount;

    const RegisteredClassificationModelPackageResult packageResult =
            validateRegisteredClassificationKnnPackage(modelDir);
    inspection.runnable = packageResult.success;
    inspection.status = packageResult.status;
    inspection.message = packageResult.message;
    return inspection;
}
