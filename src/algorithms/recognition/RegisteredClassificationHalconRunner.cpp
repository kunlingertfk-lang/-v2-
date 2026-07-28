#include "algorithms/recognition/RegisteredClassificationHalconRunner.h"

#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"
#include "algorithms/recognition/RegisteredClassificationFeatureSpace.h"
#include "algorithms/recognition/RegisteredClassificationKnnRuntime.h"
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "toolcore/PositionCorrectionTransform.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QMap>

#include <algorithm>
#include <cmath>

namespace {

QJsonObject rectToJson(const QRectF &rect)
{
    return {{QStringLiteral("x"), rect.x()}, {QStringLiteral("y"), rect.y()},
            {QStringLiteral("width"), rect.width()}, {QStringLiteral("height"), rect.height()}};
}

QJsonArray topClassesToJson(const QVector<RegisteredClassificationClassScore> &classes)
{
    QJsonArray values;
    for (const RegisteredClassificationClassScore &entry : classes) {
        values.append(QJsonObject{{QStringLiteral("label"), entry.label},
                                  {QStringLiteral("classId"), entry.classId},
                                  {QStringLiteral("score"), entry.score},
                                  {QStringLiteral("sampleSimilarity"), entry.sampleSimilarity},
                                  {QStringLiteral("centerSimilarity"), entry.centerSimilarity},
                                  {QStringLiteral("centerDistance"), entry.centerDistance}});
    }
    return values;
}

QJsonArray numbersToJson(const QVector<double> &numbers)
{
    QJsonArray array;
    for (double number : numbers)
        array.append(number);
    return array;
}

void writePositionCorrectionPayload(
        const RegisteredClassificationHalconConfig &config,
        QJsonObject *payload)
{
    if (!payload)
        return;
    payload->insert(QStringLiteral("enablePositionCorrection"),
                    config.enablePositionCorrection);
    payload->insert(QStringLiteral("positionCorrectionRequested"),
                    config.positionCorrection.requested);
    payload->insert(QStringLiteral("positionCorrectionApplied"),
                    config.positionCorrection.applied);
    payload->insert(QStringLiteral("positionCorrectionSource"),
                    config.positionCorrectionSource);
    payload->insert(QStringLiteral("positionCorrectionSourceId"),
                    config.positionCorrection.sourceId);
    payload->insert(QStringLiteral("positionCorrectionReason"),
                    config.positionCorrection.applied
                    ? QStringLiteral("applied")
                    : (config.positionCorrection.requested
                       ? QStringLiteral("not_applied")
                       : QStringLiteral("not_requested")));
    payload->insert(QStringLiteral("referenceToRunHomMat2D"),
                    numbersToJson(config.positionCorrection.referenceToRunHomMat2D));
    payload->insert(QStringLiteral("referenceScale"),
                    config.positionCorrection.referenceScale);
    payload->insert(QStringLiteral("runScale"),
                    config.positionCorrection.runScale);
    payload->insert(QStringLiteral("scaleRatio"),
                    config.positionCorrection.scaleRatio);
    payload->insert(QStringLiteral("positionCorrectionMatchContourAvailable"),
                    !config.positionCorrection.matchContours.isEmpty());
    payload->insert(QStringLiteral("positionCorrectionMatchOriginAvailable"),
                    !config.positionCorrection.matchOrigins.isEmpty());
}

RegisteredClassificationHalconResult makeError(const QString &status,
                                                const QString &message,
                                                const RegisteredClassificationHalconConfig &config,
                                                const cv::Mat &image,
                                                qint64 elapsedMs)
{
    RegisteredClassificationHalconResult result;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.payload.insert(QStringLiteral("algorithm"), registeredClassificationKnnModelType());
    result.payload.insert(QStringLiteral("modelPath"), config.modelPath);
    result.payload.insert(QStringLiteral("modelName"), config.modelName);
    result.payload.insert(QStringLiteral("modelType"), config.modelType);
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    writePositionCorrectionPayload(config, &result.payload);
    result.payload.insert(QStringLiteral("errorCode"), status);
    result.payload.insert(QStringLiteral("errorMessage"), message);
    return result;
}

RegisteredClassificationFeatureRegion featureRegion(const RegisteredClassificationHalconConfig &config)
{
    RegisteredClassificationFeatureRegion region;
    if (config.detectRegionType.trimmed().toLower() == QStringLiteral("full")) {
        region.type = QStringLiteral("full");
    } else {
        region.type = QStringLiteral("rectangle");
        region.rectNormalized = config.roiNormalized;
    }
    return region;
}

ToolOverlay roiOverlay(const QRect &roi)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.label = QStringLiteral("detect_roi");
    overlay.rect = QRectF(roi);
    overlay.extra.insert(QStringLiteral("role"), QStringLiteral("detect_roi"));
    return overlay;
}

ToolOverlay configuredRoiOverlay(
        const RegisteredClassificationHalconConfig &config,
        const cv::Mat &image)
{
    const QRectF normalized = config.detectRegionType.trimmed().toLower()
            == QStringLiteral("full")
            ? QRectF(0.0, 0.0, 1.0, 1.0)
            : config.roiNormalized.normalized();
    ToolOverlay overlay = roiOverlay(QRect(
        static_cast<int>(std::floor(normalized.x() * image.cols)),
        static_cast<int>(std::floor(normalized.y() * image.rows)),
        qMax(1, static_cast<int>(std::ceil(normalized.width() * image.cols))),
        qMax(1, static_cast<int>(std::ceil(normalized.height() * image.rows)))));
    if (config.positionCorrection.applied) {
        overlay = PositionCorrectionTransform::transformOverlay(
                    overlay,
                    config.positionCorrection.referenceToRunHomMat2D);
        overlay.extra.insert(QStringLiteral("role"), QStringLiteral("detect_roi"));
        overlay.extra.insert(QStringLiteral("positionCorrectionSourceId"),
                             config.positionCorrection.sourceId);
    }
    return overlay;
}

ToolOverlay resultOverlay(const QRect &roi, const RegisteredClassificationHalconResult &result)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.label = QStringLiteral("result");
    overlay.rect = QRectF(roi);
    overlay.p1 = QPointF(roi.left(), qMax(0, roi.top() - 24));
    overlay.text = QStringLiteral("%1: %2 (%3%, %4 ms)")
            .arg(result.ok ? QStringLiteral("OK") : QStringLiteral("NG"), result.predictedLabel,
                 QString::number(result.score, 'f', 1), QString::number(result.elapsedMs));
    overlay.score = result.score;
    overlay.extra.insert(QStringLiteral("status"), result.status);
    return overlay;
}

} // namespace

RegisteredClassificationHalconResult RegisteredClassificationHalconRunner::run(
        const cv::Mat &image,
        const RegisteredClassificationHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();
    if (image.empty())
        return makeError(QStringLiteral("image_empty"), QStringLiteral("Input image is empty."), config, image, timer.elapsed());
    if (config.modelPath.trimmed().isEmpty())
        return makeError(QStringLiteral("model_path_empty"), QStringLiteral("Classification model path is empty."), config, image, timer.elapsed());
    if (config.modelType == registeredClassificationLegacyMlpModelType()) {
        return makeError(QStringLiteral("legacy_model_requires_retraining"),
                         QStringLiteral("Schema 1 MLP model requires V2 KNN retraining."),
                         config, image, timer.elapsed());
    }
    if (config.modelType != registeredClassificationKnnModelType())
        return makeError(QStringLiteral("unsupported_model_type"),
                         QStringLiteral("Only schema-2 HALCON KNN registered-classification packages can run."),
                         config, image, timer.elapsed());
    const RegisteredClassificationModelInspection inspection =
            inspectRegisteredClassificationModelPackage(config.modelPath);
    if (inspection.legacy) {
        return makeError(QStringLiteral("legacy_model_requires_retraining"),
                         inspection.message.isEmpty()
                         ? QStringLiteral("Schema 1 MLP model requires V2 KNN retraining.")
                         : inspection.message,
                         config, image, timer.elapsed());
    }
    const RegisteredClassificationModelPackageResult package =
            validateRegisteredClassificationKnnPackage(config.modelPath);
    if (!package.success)
        return makeError(package.status, package.message, config, image, timer.elapsed());
    RegisteredClassificationKnnModelMetadata metadata;
    RegisteredClassificationClassStatsDocument stats;
    const RegisteredClassificationModelPackageResult metadataRead =
            readRegisteredClassificationKnnMetadata(config.modelPath, &metadata);
    const RegisteredClassificationModelPackageResult statsRead =
            readRegisteredClassificationClassStats(config.modelPath, &stats);
    if (!metadataRead.success || !statsRead.success)
        return makeError(!metadataRead.success ? metadataRead.status : statsRead.status,
                         !metadataRead.success ? metadataRead.message : statsRead.message,
                         config, image, timer.elapsed());
    RegisteredClassificationFeatureConfig featureConfig;
    featureConfig.halconSoPath = config.halconSoPath;
    featureConfig.halconSoPathCandidates = config.halconSoPathCandidates;
    if (config.positionCorrection.applied) {
        featureConfig.referenceToRunHomMat2D =
                config.positionCorrection.referenceToRunHomMat2D;
    }
    RegisteredClassificationFeatureExtractor extractor;
    const RegisteredClassificationFeatureResult feature = extractor.extract(image, featureRegion(config), featureConfig);
    if (!feature.success) {
        RegisteredClassificationHalconResult error =
                makeError(feature.status, feature.message,
                          config, image, timer.elapsed());
        for (const QString &key : {
             QStringLiteral("referenceRoiPixels"),
             QStringLiteral("correctedRoiArea"),
             QStringLiteral("positionCorrectionOperation")}) {
            if (feature.payload.contains(key))
                error.payload.insert(key, feature.payload.value(key));
        }
        return error;
    }
    if (feature.featureNames != metadata.featureNames || feature.feature.size() != metadata.featureLength)
        return makeError(QStringLiteral("feature_contract_mismatch"),
                         QStringLiteral("Extracted V2 feature does not match the package feature contract."),
                         config, image, timer.elapsed());
    RegisteredClassificationKnnRuntimeConfig runtimeConfig;
    runtimeConfig.halconSoPath = config.halconSoPath;
    runtimeConfig.halconSoPathCandidates = config.halconSoPathCandidates;
    RegisteredClassificationKnnRuntime runtime;
    const RegisteredClassificationKnnRuntimeResult classified = runtime.classifyPair(
                runtimeConfig, registeredClassificationSampleKnnPath(config.modelPath),
                registeredClassificationCenterKnnPath(config.modelPath), feature.feature);
    if (!classified.success)
        return makeError(classified.status, classified.message, config, image, timer.elapsed());

    QMap<int, QString> labelById;
    QMap<int, RegisteredClassificationClassStats> statsById;
    for (const RegisteredClassificationClassLabel &label : metadata.classLabels)
        labelById.insert(label.id, label.name);
    for (const RegisteredClassificationClassStats &stat : stats.classes)
        statsById.insert(stat.classId, stat);
    QMap<int, double> sampleDistance;
    QMap<int, double> centerDistance;
    for (const RegisteredClassificationKnnDistance &distance : classified.sampleDistances)
        sampleDistance.insert(distance.classId, distance.distance);
    for (const RegisteredClassificationKnnDistance &distance : classified.centerDistances)
        centerDistance.insert(distance.classId, distance.distance);
    if (labelById.size() < 2 || labelById.keys() != sampleDistance.keys() ||
        labelById.keys() != centerDistance.keys() || labelById.keys() != statsById.keys()) {
        return makeError(QStringLiteral("knn_model_mismatch"),
                         QStringLiteral("KNN model class ids do not exactly match package metadata and class statistics."),
                         config, image, timer.elapsed());
    }
    QVector<RegisteredClassificationClassScore> scores;
    for (auto it = labelById.cbegin(); it != labelById.cend(); ++it) {
        RegisteredClassificationClassScore score;
        score.classId = it.key();
        score.label = it.value();
        score.sampleSimilarity = 100.0 * registeredClassificationSimilarityFromDistance(sampleDistance.value(it.key()));
        score.centerSimilarity = 100.0 * registeredClassificationSimilarityFromDistance(centerDistance.value(it.key()));
        score.centerDistance = centerDistance.value(it.key());
        score.score = 0.70 * score.sampleSimilarity + 0.30 * score.centerSimilarity;
        scores.append(score);
    }
    std::sort(scores.begin(), scores.end(), [](const RegisteredClassificationClassScore &left,
                                                const RegisteredClassificationClassScore &right) {
        return left.score != right.score ? left.score > right.score : left.classId < right.classId;
    });
    RegisteredClassificationHalconResult result;
    result.success = true;
    result.status = QStringLiteral("ok");
    result.message = QStringLiteral("HALCON dual KNN classification succeeded.");
    result.predictedClassId = scores.first().classId;
    result.predictedLabel = scores.first().label;
    result.score = scores.first().score;
    result.sampleSimilarity = scores.first().sampleSimilarity;
    result.centerSimilarity = scores.first().centerSimilarity;
    result.centerDistance = scores.first().centerDistance;
    result.secondScore = scores.size() > 1 ? scores.at(1).score : 0.0;
    result.scoreMargin = result.score - result.secondScore;
    const RegisteredClassificationClassStats winnerStats = statsById.value(result.predictedClassId);
    result.classRadius = winnerStats.radius;
    result.radiusEnabled = winnerStats.radiusEnabled;
    result.topClasses = scores.mid(0, qBound(1, config.topK, scores.size()));
    const int bestCandidateClassId = result.predictedClassId;
    const QString bestCandidateLabel = result.predictedLabel;
    result.elapsedMs = timer.elapsed();
    const int minSimilarity = qBound(0, config.minSimilarity, 100);
    const int minMargin = qBound(0, config.minMargin, 100);
    if (result.score < minSimilarity) {
        result.rejected = true;
        result.rejectionReason = QStringLiteral("classification_rejected_low_similarity");
    } else if (result.scoreMargin < minMargin) {
        result.rejected = true;
        result.rejectionReason = QStringLiteral("classification_rejected_ambiguous");
    } else if (result.radiusEnabled && result.centerDistance > result.classRadius) {
        result.rejected = true;
        result.rejectionReason = QStringLiteral("classification_rejected_out_of_radius");
    }
    if (result.rejected) {
        result.ok = false;
        result.status = result.rejectionReason;
        result.message = QStringLiteral("Classification was rejected: %1.").arg(result.rejectionReason);
        result.predictedLabel = QStringLiteral("UNKNOWN");
        result.predictedClassId = -1;
    } else if (config.judgeMode == QStringLiteral("min_score")) {
        result.ok = result.score >= config.minScore;
        if (!result.ok) {
            result.status = QStringLiteral("min_score");
            result.message = QStringLiteral("Classification score did not meet the configured judge threshold.");
        }
    } else {
        result.ok = config.expectedLabel.isEmpty() || result.predictedLabel == config.expectedLabel;
        if (!result.ok) {
            result.status = QStringLiteral("class_match");
            result.message = QStringLiteral("Classification label did not match the expected label.");
        }
    }
    result.payload.insert(QStringLiteral("algorithm"), registeredClassificationKnnModelType());
    result.payload.insert(QStringLiteral("featureVersion"), metadata.featureVersion);
    result.payload.insert(QStringLiteral("modelPath"), config.modelPath);
    result.payload.insert(QStringLiteral("modelName"), config.modelName);
    result.payload.insert(QStringLiteral("predictedClassId"), result.predictedClassId);
    result.payload.insert(QStringLiteral("predictedLabel"), result.predictedLabel);
    result.payload.insert(QStringLiteral("bestCandidateClassId"), bestCandidateClassId);
    result.payload.insert(QStringLiteral("bestCandidateLabel"), bestCandidateLabel);
    result.payload.insert(QStringLiteral("score"), result.score);
    result.payload.insert(QStringLiteral("sampleSimilarity"), result.sampleSimilarity);
    result.payload.insert(QStringLiteral("centerSimilarity"), result.centerSimilarity);
    result.payload.insert(QStringLiteral("centerDistance"), result.centerDistance);
    result.payload.insert(QStringLiteral("secondScore"), result.secondScore);
    result.payload.insert(QStringLiteral("scoreMargin"), result.scoreMargin);
    result.payload.insert(QStringLiteral("classRadius"), result.classRadius);
    result.payload.insert(QStringLiteral("radiusEnabled"), result.radiusEnabled);
    result.payload.insert(QStringLiteral("rejected"), result.rejected);
    result.payload.insert(QStringLiteral("rejectionReason"), result.rejectionReason);
    result.payload.insert(QStringLiteral("minSimilarity"), minSimilarity);
    result.payload.insert(QStringLiteral("minMargin"), minMargin);
    result.payload.insert(QStringLiteral("topClasses"), topClassesToJson(result.topClasses));
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("correctedRoiPixels"),
                          feature.payload.value(QStringLiteral("roiPixels")));
    result.payload.insert(QStringLiteral("correctedRoiArea"),
                          feature.payload.value(QStringLiteral("correctedRoiArea")));
    result.payload.insert(QStringLiteral("positionCorrectionOperation"),
                          feature.payload.value(QStringLiteral("positionCorrectionOperation")));
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
    writePositionCorrectionPayload(config, &result.payload);
    result.payload.insert(QStringLiteral("errorCode"), result.status);
    result.payload.insert(QStringLiteral("errorMessage"), result.message);
    result.overlays.append(configuredRoiOverlay(config, image));
    if (config.positionCorrection.applied
            && config.positionCorrection.showMatchContour) {
        result.overlays += PositionCorrectionTransform::matchContourOverlays(
                    config.positionCorrection.matchContours,
                    config.positionCorrection.sourceId);
    }
    if (config.positionCorrection.applied) {
        result.overlays += PositionCorrectionTransform::matchOriginOverlays(
                    config.positionCorrection.matchOrigins,
                    config.positionCorrection.sourceId);
    }
    result.overlays.append(resultOverlay(feature.roiPixels, result));
    return result;
}
