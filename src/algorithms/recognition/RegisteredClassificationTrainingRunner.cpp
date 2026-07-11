#include "algorithms/recognition/RegisteredClassificationTrainingRunner.h"

#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"
#include "algorithms/recognition/RegisteredClassificationKnnRuntime.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>

namespace {

RegisteredClassificationTrainingResult errorResult(const QString &status, const QString &message)
{
    RegisteredClassificationTrainingResult result;
    result.status = status;
    result.message = message;
    result.payload.insert(QStringLiteral("errorCode"), status);
    result.payload.insert(QStringLiteral("errorMessage"), message);
    return result;
}

QJsonArray stringsToJson(const QStringList &values)
{
    return QJsonArray::fromStringList(values);
}

QJsonObject countToJson(const QMap<QString, int> &counts)
{
    QJsonObject json;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        json.insert(it.key(), it.value());
    return json;
}

QJsonObject classStatsToJson(const RegisteredClassificationClassStatsDocument &document)
{
    QJsonObject json;
    for (const RegisteredClassificationClassStats &stat : document.classes) {
        QJsonObject entry;
        entry.insert(QStringLiteral("sampleCount"), stat.sampleCount);
        entry.insert(QStringLiteral("radiusEnabled"), stat.radiusEnabled);
        entry.insert(QStringLiteral("radius"), stat.radius);
        entry.insert(QStringLiteral("meanDistance"), stat.meanDistance);
        entry.insert(QStringLiteral("stdDevDistance"), stat.stdDevDistance);
        entry.insert(QStringLiteral("maxDistance"), stat.maxDistance);
        json.insert(QString::number(stat.classId), entry);
    }
    return json;
}

bool ensureDirectoryEmpty(const QString &path)
{
    QDir dir(path);
    return (!dir.exists() || dir.removeRecursively()) && QDir().mkpath(path);
}

bool swapModelDirectory(const QString &temporaryPath, const QString &targetPath, QString *message)
{
    const QString backupPath = targetPath + QStringLiteral(".bak");
    if (QDir(backupPath).exists() && !QDir(backupPath).removeRecursively()) {
        *message = QStringLiteral("Failed to clear temporary backup directory %1.").arg(backupPath);
        return false;
    }
    const bool hadTarget = QDir(targetPath).exists();
    if (hadTarget && !QDir().rename(targetPath, backupPath)) {
        *message = QStringLiteral("Failed to move existing model directory %1.").arg(targetPath);
        return false;
    }
    if (!QDir().rename(temporaryPath, targetPath)) {
        if (hadTarget)
            QDir().rename(backupPath, targetPath);
        *message = QStringLiteral("Failed to atomically move the completed model package into place.");
        return false;
    }
    if (hadTarget)
        QDir(backupPath).removeRecursively();
    return true;
}

RegisteredClassificationFeatureRegion v2Region(const RegisteredClassificationTrainingSample &sample)
{
    if (sample.roiNormalized != QRectF(0.0, 0.0, 1.0, 1.0)) {
        RegisteredClassificationFeatureRegion region;
        region.type = QStringLiteral("rectangle");
        region.rectNormalized = sample.roiNormalized;
        return region;
    }
    return sample.region;
}

} // namespace

RegisteredClassificationTrainingResult RegisteredClassificationTrainingRunner::train(
        const RegisteredClassificationTrainingRequest &request) const
{
    if (request.outputModelDir.trimmed().isEmpty())
        return errorResult(QStringLiteral("invalid_argument"), QStringLiteral("Training outputModelDir must not be empty."));
    if (request.classLabels.size() < 2)
        return errorResult(QStringLiteral("training_not_enough_classes"), QStringLiteral("At least two class labels are required for KNN training."));

    QMap<int, RegisteredClassificationClassLabel> labelsById;
    QMap<QString, int> validByClass;
    QMap<QString, int> invalidByClass;
    for (const RegisteredClassificationClassLabel &label : request.classLabels) {
        if (label.id < 0 || label.name.trimmed().isEmpty() || labelsById.contains(label.id))
            return errorResult(QStringLiteral("invalid_class_label"), QStringLiteral("Class labels must have unique non-negative ids and names."));
        labelsById.insert(label.id, label);
        validByClass.insert(label.name, 0);
        invalidByClass.insert(label.name, 0);
    }

    RegisteredClassificationFeatureConfig featureConfig;
    featureConfig.halconSoPath = request.halconSoPath;
    featureConfig.halconSoPathCandidates = request.halconSoPathCandidates;
    RegisteredClassificationFeatureExtractor extractor;
    QMap<int, QVector<QVector<double>>> featuresByClass;
    QStringList warnings;
    int validSamples = 0;
    for (int index = 0; index < request.samples.size(); ++index) {
        const RegisteredClassificationTrainingSample &sample = request.samples.at(index);
        if (!labelsById.contains(sample.classId)) {
            warnings.append(QStringLiteral("sample[%1] skipped: invalid_class_id").arg(index));
            continue;
        }
        const RegisteredClassificationFeatureResult feature = extractor.extract(sample.image, v2Region(sample), featureConfig);
        const RegisteredClassificationClassLabel label = labelsById.value(sample.classId);
        if (!feature.success || feature.feature.size() != registeredClassificationFeatureNamesV2().size()) {
            ++invalidByClass[label.name];
            warnings.append(QStringLiteral("sample[%1] class=%2 skipped: %3")
                            .arg(index).arg(label.name, feature.success ? QStringLiteral("feature_length_mismatch") : feature.status));
            continue;
        }
        featuresByClass[sample.classId].append(feature.feature);
        ++validByClass[label.name];
        ++validSamples;
    }
    QStringList missingClasses;
    for (const RegisteredClassificationClassLabel &label : request.classLabels) {
        if (featuresByClass.value(label.id).isEmpty())
            missingClasses.append(label.name);
    }
    if (!missingClasses.isEmpty()) {
        RegisteredClassificationTrainingResult result = errorResult(
                    QStringLiteral("training_not_enough_classes"),
                    QStringLiteral("Each class must contribute at least one valid V2 feature."));
        result.payload.insert(QStringLiteral("missingValidSampleClasses"), stringsToJson(missingClasses));
        result.payload.insert(QStringLiteral("validSamplesByClass"), countToJson(validByClass));
        result.payload.insert(QStringLiteral("invalidSamplesByClass"), countToJson(invalidByClass));
        return result;
    }

    RegisteredClassificationClassStatsDocument statsDocument;
    statsDocument.featureVersion = registeredClassificationFeatureVersionV2();
    QVector<RegisteredClassificationKnnSample> sampleKnnSamples;
    QVector<RegisteredClassificationKnnSample> centerKnnSamples;
    for (const RegisteredClassificationClassLabel &label : request.classLabels) {
        const QVector<QVector<double>> features = featuresByClass.value(label.id);
        const QVector<double> center = registeredClassificationClassCenter(features);
        if (center.size() != registeredClassificationFeatureNamesV2().size())
            return errorResult(QStringLiteral("invalid_feature_value"), QStringLiteral("Could not calculate a normalized class center."));
        const RegisteredClassificationRadiusStats radius = registeredClassificationRadiusStats(features, center);
        RegisteredClassificationClassStats classStats;
        classStats.classId = label.id;
        classStats.sampleCount = features.size();
        classStats.radiusEnabled = radius.enabled;
        classStats.radius = radius.radius;
        classStats.meanDistance = radius.meanDistance;
        classStats.stdDevDistance = radius.stdDevDistance;
        classStats.maxDistance = radius.maxDistance;
        statsDocument.classes.append(classStats);
        centerKnnSamples.append({center, label.id});
        for (const QVector<double> &feature : features)
            sampleKnnSamples.append({feature, label.id});
    }

    const QString temporaryDir = request.outputModelDir + QStringLiteral(".tmp");
    if (!ensureDirectoryEmpty(temporaryDir))
        return errorResult(QStringLiteral("model_write_failed"), QStringLiteral("Failed to create temporary model directory."));
    const auto cleanup = [&temporaryDir]() { QDir(temporaryDir).removeRecursively(); };
    QElapsedTimer totalTimer;
    totalTimer.start();
    RegisteredClassificationKnnRuntimeConfig runtimeConfig;
    runtimeConfig.halconSoPath = request.halconSoPath;
    runtimeConfig.halconSoPathCandidates = request.halconSoPathCandidates;
    RegisteredClassificationKnnRuntime runtime;
    QElapsedTimer sampleTimer;
    sampleTimer.start();
    RegisteredClassificationKnnBuildRequest sampleRequest;
    sampleRequest.samples = sampleKnnSamples;
    sampleRequest.featureLength = registeredClassificationFeatureNamesV2().size();
    sampleRequest.numTrees = request.knn.numTrees;
    sampleRequest.k = sampleKnnSamples.size();
    sampleRequest.maxNumClasses = request.classLabels.size();
    sampleRequest.outputPath = registeredClassificationSampleKnnPath(temporaryDir);
    const RegisteredClassificationKnnRuntimeResult sampleBuild = runtime.buildAndWrite(runtimeConfig, sampleRequest);
    const qint64 sampleBuildMs = sampleTimer.elapsed();
    if (!sampleBuild.success) {
        cleanup();
        return errorResult(sampleBuild.status, sampleBuild.message);
    }
    QElapsedTimer centerTimer;
    centerTimer.start();
    RegisteredClassificationKnnBuildRequest centerRequest = sampleRequest;
    centerRequest.samples = centerKnnSamples;
    centerRequest.k = centerKnnSamples.size();
    centerRequest.outputPath = registeredClassificationCenterKnnPath(temporaryDir);
    const RegisteredClassificationKnnRuntimeResult centerBuild = runtime.buildAndWrite(runtimeConfig, centerRequest);
    const qint64 centerBuildMs = centerTimer.elapsed();
    if (!centerBuild.success) {
        cleanup();
        return errorResult(centerBuild.status, centerBuild.message);
    }

    RegisteredClassificationKnnModelMetadata metadata;
    metadata.modelType = registeredClassificationKnnModelType();
    metadata.schemaVersion = 2;
    metadata.featureVersion = registeredClassificationFeatureVersionV2();
    metadata.halconVersion = QStringLiteral("24.11.1");
    metadata.classLabels = request.classLabels;
    metadata.featureNames = registeredClassificationFeatureNamesV2();
    metadata.featureLength = registeredClassificationFeatureNamesV2().size();
    metadata.segmentation = registeredClassificationSegmentationContractV2();
    metadata.canonicalization = registeredClassificationCanonicalizationContractV2();
    metadata.featureGroups = registeredClassificationFeatureGroupsContractV2();
    metadata.knn = request.knn;
    metadata.thresholds = request.thresholds;
    metadata.trainingSampleCount = validSamples;
    const RegisteredClassificationModelPackageResult metadataWrite =
            writeRegisteredClassificationKnnMetadata(temporaryDir, metadata);
    if (!metadataWrite.success) {
        cleanup();
        return errorResult(metadataWrite.status, metadataWrite.message);
    }
    const RegisteredClassificationModelPackageResult statsWrite =
            writeRegisteredClassificationClassStats(temporaryDir, statsDocument);
    if (!statsWrite.success) {
        cleanup();
        return errorResult(statsWrite.status, statsWrite.message);
    }
    QJsonObject report;
    report.insert(QStringLiteral("sampleCount"), validSamples);
    report.insert(QStringLiteral("validSamplesByClass"), countToJson(validByClass));
    report.insert(QStringLiteral("invalidSamplesByClass"), countToJson(invalidByClass));
    report.insert(QStringLiteral("classStats"), classStatsToJson(statsDocument));
    report.insert(QStringLiteral("sampleKnnBuildMs"), static_cast<double>(sampleBuildMs));
    report.insert(QStringLiteral("centerKnnBuildMs"), static_cast<double>(centerBuildMs));
    report.insert(QStringLiteral("elapsedMs"), static_cast<double>(totalTimer.elapsed()));
    report.insert(QStringLiteral("warnings"), stringsToJson(warnings));
    QFile reportFile(registeredClassificationTrainingReportPath(temporaryDir));
    if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        cleanup();
        return errorResult(QStringLiteral("model_write_failed"), reportFile.errorString());
    }
    reportFile.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    reportFile.close();
    if (!request.trainingSessionManifest.isEmpty() || !request.trainingSessionAssets.isEmpty()) {
        const RegisteredClassificationTrainingSessionResult sessionWrite =
                writeRegisteredClassificationTrainingSession(QDir(temporaryDir).filePath(QStringLiteral("training_session")),
                                                             request.trainingSessionManifest,
                                                             request.trainingSessionAssets);
        if (!sessionWrite.success) {
            cleanup();
            return errorResult(QStringLiteral("training_session_write_failed"), sessionWrite.message);
        }
    }
    const RegisteredClassificationModelPackageResult packageValidation =
            validateRegisteredClassificationKnnPackage(temporaryDir);
    if (!packageValidation.success) {
        cleanup();
        return errorResult(packageValidation.status, packageValidation.message);
    }
    QString swapError;
    if (!swapModelDirectory(temporaryDir, request.outputModelDir, &swapError)) {
        cleanup();
        return errorResult(QStringLiteral("model_write_failed"), swapError);
    }
    RegisteredClassificationTrainingResult result;
    result.success = true;
    result.status = QStringLiteral("ok");
    result.message = QStringLiteral("HALCON dual KNN training succeeded.");
    result.modelDir = request.outputModelDir;
    result.sampleCount = validSamples;
    result.payload.insert(QStringLiteral("sampleCount"), validSamples);
    result.payload.insert(QStringLiteral("classStats"), classStatsToJson(statsDocument));
    result.payload.insert(QStringLiteral("sampleKnnBuildMs"), static_cast<double>(sampleBuildMs));
    result.payload.insert(QStringLiteral("centerKnnBuildMs"), static_cast<double>(centerBuildMs));
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(totalTimer.elapsed()));
    return result;
}
