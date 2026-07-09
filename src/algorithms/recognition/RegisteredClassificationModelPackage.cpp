#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

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

} // namespace

QString registeredClassificationMlpModelType()
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
