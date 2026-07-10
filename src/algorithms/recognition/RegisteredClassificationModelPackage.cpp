#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

#include <cmath>

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

bool sameDouble(double left, double right)
{
    return std::abs(left - right) < 1e-12;
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
    const bool validKnn = knn.value(QStringLiteral("method")).isString()
            && knn.value(QStringLiteral("method")).toString() == QStringLiteral("classes_distance")
            && knn.value(QStringLiteral("normalization")).isBool()
            && !knn.value(QStringLiteral("normalization")).toBool()
            && knn.value(QStringLiteral("numTrees")).isDouble()
            && sameDouble(knn.value(QStringLiteral("numTrees")).toDouble(), 4.0)
            && knn.value(QStringLiteral("numChecks")).isDouble()
            && sameDouble(knn.value(QStringLiteral("numChecks")).toDouble(), 0.0)
            && knn.value(QStringLiteral("epsilon")).isDouble()
            && sameDouble(knn.value(QStringLiteral("epsilon")).toDouble(), 0.0)
            && knn.value(QStringLiteral("sampleWeight")).isDouble()
            && sameDouble(knn.value(QStringLiteral("sampleWeight")).toDouble(), 0.70)
            && knn.value(QStringLiteral("centerWeight")).isDouble()
            && sameDouble(knn.value(QStringLiteral("centerWeight")).toDouble(), 0.30)
            && thresholds.value(QStringLiteral("minSimilarity")).isDouble()
            && sameDouble(thresholds.value(QStringLiteral("minSimilarity")).toDouble(), 80.0)
            && thresholds.value(QStringLiteral("minMargin")).isDouble()
            && sameDouble(thresholds.value(QStringLiteral("minMargin")).toDouble(), 8.0);
    if (!validKnn) {
        return result(false, QStringLiteral("invalid_knn_parameters"),
                      QStringLiteral("Schema 2 metadata must explicitly persist the fixed KNN, "
                                     "fusion, and rejection contract values with JSON types."));
    }
    return result(true, QStringLiteral("ok"), QStringLiteral("KNN JSON contract is valid"));
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

RegisteredClassificationClassStatsDocument classStatsFromJson(const QJsonObject &root)
{
    RegisteredClassificationClassStatsDocument document;
    document.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(0);
    document.featureVersion = root.value(QStringLiteral("featureVersion")).toString();
    const QJsonArray classes = root.value(QStringLiteral("classes")).toArray();
    for (const QJsonValue &value : classes) {
        const QJsonObject object = value.toObject();
        RegisteredClassificationClassStats stats;
        stats.classId = object.value(QStringLiteral("classId")).toInt(-1);
        stats.sampleCount = object.value(QStringLiteral("sampleCount")).toInt(0);
        stats.radiusEnabled = object.value(QStringLiteral("radiusEnabled")).toBool(false);
        stats.radius = object.value(QStringLiteral("radius")).toDouble(0.0);
        stats.meanDistance = object.value(QStringLiteral("meanDistance")).toDouble(0.0);
        stats.stdDevDistance = object.value(QStringLiteral("stdDevDistance")).toDouble(0.0);
        stats.maxDistance = object.value(QStringLiteral("maxDistance")).toDouble(0.0);
        document.classes.append(stats);
    }
    return document;
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
        if (stats.radiusEnabled && (stats.radius < 0.10 || stats.radius > 2.00)) {
            return result(false, QStringLiteral("invalid_class_stats"),
                          QStringLiteral("Enabled class radius must be within [0.10, 2.00]."));
        }
        classIds.insert(stats.classId);
    }
    return result(true, QStringLiteral("ok"), QStringLiteral("class stats are valid"));
}

} // namespace

QString registeredClassificationMlpModelType()
{
    return registeredClassificationLegacyMlpModelType();
}

QString registeredClassificationKnnModelType()
{
    return QStringLiteral("halcon_knn_registered_classification");
}

QString registeredClassificationLegacyMlpModelType()
{
    return QStringLiteral("halcon_mlp_registered_classification");
}

QString registeredClassificationFeatureVersionV1()
{
    return QStringLiteral("halcon_mlp_roi_stats_v1");
}

QStringList registeredClassificationFeatureNamesV1()
{
    return QStringList()
            << QStringLiteral("roiAspect")
            << QStringLiteral("roiAreaRatio")
            << QStringLiteral("foregroundAreaRatio")
            << QStringLiteral("foregroundCenterX")
            << QStringLiteral("foregroundCenterY")
            << QStringLiteral("momentRa")
            << QStringLiteral("momentRb")
            << QStringLiteral("momentPhi")
            << QStringLiteral("grayMean")
            << QStringLiteral("grayMin")
            << QStringLiteral("grayMax")
            << QStringLiteral("grayDeviation")
            << QStringLiteral("grayHist00")
            << QStringLiteral("grayHist01")
            << QStringLiteral("grayHist02")
            << QStringLiteral("grayHist03")
            << QStringLiteral("grayHist04")
            << QStringLiteral("grayHist05")
            << QStringLiteral("grayHist06")
            << QStringLiteral("grayHist07")
            << QStringLiteral("grayHist08")
            << QStringLiteral("grayHist09")
            << QStringLiteral("grayHist10")
            << QStringLiteral("grayHist11")
            << QStringLiteral("grayHist12")
            << QStringLiteral("grayHist13")
            << QStringLiteral("grayHist14")
            << QStringLiteral("grayHist15");
}

QString registeredClassificationMetadataPath(const QString &modelDir)
{
    return QDir(modelDir).filePath(QStringLiteral("metadata.json"));
}

QString registeredClassificationMlpPath(const QString &modelDir)
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

RegisteredClassificationModelPackageResult validateRegisteredClassificationMetadata(
        const RegisteredClassificationModelMetadata &metadata)
{
    if (metadata.modelType != registeredClassificationMlpModelType()) {
        return result(false, QStringLiteral("unsupported_model_type"),
                      QStringLiteral("Only halcon_mlp_registered_classification is supported."));
    }
    if (metadata.schemaVersion != 1) {
        return result(false, QStringLiteral("unsupported_schema_version"),
                      QStringLiteral("Only schemaVersion=1 is supported."));
    }
    if (metadata.featureVersion != registeredClassificationFeatureVersionV1()) {
        return result(false, QStringLiteral("unsupported_feature_version"),
                      QStringLiteral("Only halcon_mlp_roi_stats_v1 is supported."));
    }
    if (metadata.featureLength != metadata.featureNames.size() || metadata.featureLength <= 0) {
        return result(false, QStringLiteral("feature_length_mismatch"),
                      QStringLiteral("featureLength must match featureNames count and be positive."));
    }
    if (metadata.classLabels.size() < 2) {
        return result(false, QStringLiteral("training_not_enough_classes"),
                      QStringLiteral("At least two class labels are required."));
    }
    for (const RegisteredClassificationClassLabel &label : metadata.classLabels) {
        if (label.id < 0 || label.name.trimmed().isEmpty()) {
            return result(false, QStringLiteral("invalid_class_label"),
                          QStringLiteral("Class labels must have non-negative ids and non-empty names."));
        }
    }
    return result(true, QStringLiteral("ok"), QStringLiteral("metadata is valid"));
}

RegisteredClassificationModelPackageResult writeRegisteredClassificationMetadata(
        const QString &modelDir,
        const RegisteredClassificationModelMetadata &metadata)
{
    const RegisteredClassificationModelPackageResult validation =
            validateRegisteredClassificationMetadata(metadata);
    if (!validation.success)
        return validation;

    QDir dir(modelDir);
    if (!dir.exists() && !QDir().mkpath(modelDir)) {
        return result(false, QStringLiteral("model_write_failed"),
                      QStringLiteral("Failed to create model directory."));
    }

    QJsonObject root;
    root.insert(QStringLiteral("modelType"), metadata.modelType);
    root.insert(QStringLiteral("schemaVersion"), metadata.schemaVersion);
    root.insert(QStringLiteral("featureVersion"), metadata.featureVersion);
    root.insert(QStringLiteral("halconVersion"), metadata.halconVersion);
    root.insert(QStringLiteral("classLabels"), classLabelsToJson(metadata.classLabels));
    root.insert(QStringLiteral("featureNames"), stringListToJson(metadata.featureNames));
    root.insert(QStringLiteral("featureLength"), metadata.featureLength);
    root.insert(QStringLiteral("preprocess"), metadata.preprocess);
    root.insert(QStringLiteral("trainingSampleCount"), metadata.trainingSampleCount);

    QJsonObject mlp;
    mlp.insert(QStringLiteral("numHidden"), metadata.mlp.numHidden);
    mlp.insert(QStringLiteral("maxIterations"), metadata.mlp.maxIterations);
    mlp.insert(QStringLiteral("randSeed"), metadata.mlp.randSeed);
    root.insert(QStringLiteral("mlp"), mlp);

    QJsonObject thresholds;
    thresholds.insert(QStringLiteral("minScore"), metadata.thresholds.minScore);
    thresholds.insert(QStringLiteral("rejectScore"), metadata.thresholds.rejectScore);
    thresholds.insert(QStringLiteral("top2Gap"), metadata.thresholds.top2Gap);
    root.insert(QStringLiteral("thresholds"), thresholds);

    QFile file(registeredClassificationMetadataPath(modelDir));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return result(false, QStringLiteral("model_write_failed"), file.errorString());
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return result(true, QStringLiteral("ok"), QStringLiteral("metadata written"));
}

RegisteredClassificationModelPackageResult readRegisteredClassificationMetadata(
        const QString &modelDir,
        RegisteredClassificationModelMetadata *metadata)
{
    if (!metadata) {
        return result(false, QStringLiteral("invalid_argument"),
                      QStringLiteral("metadata output is null."));
    }
    QFile file(registeredClassificationMetadataPath(modelDir));
    if (!file.exists()) {
        return result(false, QStringLiteral("model_file_not_found"),
                      QStringLiteral("metadata.json does not exist."));
    }
    if (!file.open(QIODevice::ReadOnly))
        return result(false, QStringLiteral("model_file_not_found"), file.errorString());

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();
    RegisteredClassificationModelMetadata loaded;
    loaded.modelType = root.value(QStringLiteral("modelType")).toString();
    loaded.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(0);
    loaded.featureVersion = root.value(QStringLiteral("featureVersion")).toString();
    loaded.halconVersion = root.value(QStringLiteral("halconVersion")).toString();
    loaded.classLabels = classLabelsFromJson(root.value(QStringLiteral("classLabels")).toArray());
    loaded.featureNames = stringListFromJson(root.value(QStringLiteral("featureNames")).toArray());
    loaded.featureLength = root.value(QStringLiteral("featureLength")).toInt(0);
    loaded.preprocess = root.value(QStringLiteral("preprocess")).toObject();
    loaded.trainingSampleCount = root.value(QStringLiteral("trainingSampleCount")).toInt(0);

    const QJsonObject mlp = root.value(QStringLiteral("mlp")).toObject();
    loaded.mlp.numHidden = mlp.value(QStringLiteral("numHidden")).toInt(16);
    loaded.mlp.maxIterations = mlp.value(QStringLiteral("maxIterations")).toInt(200);
    loaded.mlp.randSeed = mlp.value(QStringLiteral("randSeed")).toInt(42);

    const QJsonObject thresholds = root.value(QStringLiteral("thresholds")).toObject();
    loaded.thresholds.minScore = thresholds.value(QStringLiteral("minScore")).toInt(80);
    loaded.thresholds.rejectScore = thresholds.value(QStringLiteral("rejectScore")).toInt(60);
    loaded.thresholds.top2Gap = thresholds.value(QStringLiteral("top2Gap")).toInt(0);

    const RegisteredClassificationModelPackageResult validation =
            validateRegisteredClassificationMetadata(loaded);
    if (!validation.success)
        return validation;
    *metadata = loaded;
    return result(true, QStringLiteral("ok"), QStringLiteral("metadata read"));
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
    if (metadata.knn.method != QStringLiteral("classes_distance")
            || metadata.knn.normalization
            || metadata.knn.numTrees != 4 || metadata.knn.numChecks != 0
            || !sameDouble(metadata.knn.epsilon, 0.0)
            || !sameDouble(metadata.knn.sampleWeight, 0.70)
            || !sameDouble(metadata.knn.centerWeight, 0.30)) {
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

    const RegisteredClassificationModelPackageResult contractResult = validateKnnContractJson(root);
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
    const RegisteredClassificationClassStatsDocument loaded = classStatsFromJson(root);
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
    int sampleCount = 0;
    for (const RegisteredClassificationClassStats &classStats : stats.classes) {
        if (!metadataClassIds.contains(classStats.classId)) {
            return result(false, QStringLiteral("class_stats_mismatch"),
                          QStringLiteral("Class stats include an unknown class id."));
        }
        sampleCount += classStats.sampleCount;
    }
    if (sampleCount != metadata.trainingSampleCount) {
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
