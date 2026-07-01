#include "algorithms/recognition/ColorComparisonHalconRunner.h"

#include "algorithms/recognition/ColorRecognitionHalconRunner.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace {

bool finiteValue(qreal value)
{
    return std::isfinite(static_cast<double>(value));
}

bool validRect(const QRectF &rect)
{
    return finiteValue(rect.x()) &&
            finiteValue(rect.y()) &&
            finiteValue(rect.width()) &&
            finiteValue(rect.height()) &&
            rect.width() > 0.0 &&
            rect.height() > 0.0;
}

QJsonObject rectToJson(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

QJsonObject pointToJson(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
}

QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points)
        array.append(pointToJson(point));
    return array;
}

QJsonArray featureToJson(const QVector<double> &feature)
{
    QJsonArray array;
    for (const double value : feature)
        array.append(value);
    return array;
}

int histogramBinsForSensitivity(const QString &sensitivity)
{
    const QString normalized = sensitivity.trimmed().toLower();
    if (normalized == QStringLiteral("low"))
        return 8;
    if (normalized == QStringLiteral("high"))
        return 32;
    return 16;
}

ColorComparisonHalconResult errorResult(const QString &status,
                                        const QString &message,
                                        const ColorComparisonHalconConfig &config,
                                        qint64 elapsedMs)
{
    ColorComparisonHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.payload.insert(QStringLiteral("algorithm"),
                          QStringLiteral("halcon_hsv_histogram_comparison"));
    result.payload.insert(QStringLiteral("featureType"), config.featureType);
    result.payload.insert(QStringLiteral("sensitivity"), config.sensitivity);
    result.payload.insert(QStringLiteral("brightnessEnabled"), config.brightnessEnabled);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));
    result.payload.insert(QStringLiteral("comparisonMethod"),
                          QStringLiteral("hsv_histogram_intersection"));
    result.payload.insert(QStringLiteral("scoreDirection"),
                          QStringLiteral("higher_is_better"));
    result.payload.insert(QStringLiteral("status"), status);
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    return result;
}

ColorRecognitionHalconConfig featureConfig(const ColorComparisonHalconConfig &config,
                                           const QRectF &roi,
                                           const QVector<QPointF> &maskPolygon,
                                           bool circleMode)
{
    ColorRecognitionHalconConfig featureConfig;
    featureConfig.halconSoPath = config.halconSoPath;
    featureConfig.halconSoPathCandidates = config.halconSoPathCandidates;
    featureConfig.roiNormalized = roi;
    featureConfig.detectRegionType = circleMode ? QStringLiteral("circle")
                                                : QStringLiteral("rectangle");
    featureConfig.detectCircleCenterNormalized = config.detectCircleCenterNormalized;
    featureConfig.detectCircleRadiusNormalized = config.detectCircleRadiusNormalized;
    featureConfig.detectCircleBoundingRectNormalized =
            validRect(config.detectCircleBoundingRectNormalized)
            ? config.detectCircleBoundingRectNormalized
            : roi;
    featureConfig.detectMaskPolygonNormalized = maskPolygon;
    featureConfig.featureType = config.featureType;
    featureConfig.sensitivity = config.sensitivity;
    featureConfig.brightnessEnabled = config.brightnessEnabled;
    return featureConfig;
}

double histogramIntersectionSimilarity(const QVector<double> &a, const QVector<double> &b)
{
    const int count = qMin(a.size(), b.size());
    if (count <= 0)
        return 0.0;

    double intersection = 0.0;
    double sumA = 0.0;
    double sumB = 0.0;
    for (int i = 0; i < count; ++i) {
        const double av = qMax(0.0, a.at(i));
        const double bv = qMax(0.0, b.at(i));
        intersection += std::min(av, bv);
        sumA += av;
        sumB += bv;
    }

    const double normalizer = std::min(sumA, sumB);
    return normalizer > 0.0 ? qBound(0.0, intersection / normalizer, 1.0) : 0.0;
}

ToolOverlay rectOverlay(const QRectF &rect, const QString &label, const QString &role)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.rect = rect;
    overlay.label = label;
    overlay.extra.insert(QStringLiteral("role"), role);
    return overlay;
}

ToolOverlay polygonOverlay(const QVector<QPointF> &points, const QString &label, const QString &role)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Polygon;
    overlay.points = points;
    overlay.label = label;
    overlay.extra.insert(QStringLiteral("role"), role);
    return overlay;
}

ToolOverlay textOverlay(const QString &text, double score, bool ok)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.text = text;
    overlay.score = score;
    overlay.p1 = QPointF(0.08, 0.08);
    overlay.extra.insert(QStringLiteral("status"), ok ? QStringLiteral("OK") : QStringLiteral("NG"));
    return overlay;
}

} // namespace

ColorComparisonHalconResult ColorComparisonHalconRunner::run(
        const cv::Mat &image,
        const ColorComparisonHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    if (config.featureType.trimmed().toLower() == QStringLiteral("spectrum")) {
        return errorResult(QStringLiteral("unsupported_feature"),
                           QStringLiteral("Spectrum feature is reserved but not implemented."),
                           config,
                           timer.elapsed());
    }
    if (config.featureType.trimmed().toLower() != QStringLiteral("histogram")) {
        return errorResult(QStringLiteral("unsupported_feature"),
                           QStringLiteral("Only histogram feature is supported."),
                           config,
                           timer.elapsed());
    }
    if (image.empty()) {
        return errorResult(QStringLiteral("image_empty"),
                           QStringLiteral("Input image is empty."),
                           config,
                           timer.elapsed());
    }
    if (!validRect(config.templateRoiNormalized)) {
        return errorResult(QStringLiteral("invalid_template_roi"),
                           QStringLiteral("Template ROI is invalid."),
                           config,
                           timer.elapsed());
    }
    if (!validRect(config.detectRoiNormalized)) {
        return errorResult(QStringLiteral("invalid_detect_roi"),
                           QStringLiteral("Detect ROI is invalid."),
                           config,
                           timer.elapsed());
    }

    ColorRecognitionHalconRunner featureRunner;
    const ColorRecognitionHalconFeatureResult templateFeature =
            featureRunner.extractFeature(
                image,
                featureConfig(config,
                              config.templateRoiNormalized,
                              config.templateMaskPolygonNormalized,
                              false));
    if (!templateFeature.success) {
        const QString status = templateFeature.status == QStringLiteral("masked_roi_empty")
                ? QStringLiteral("template_masked_empty")
                : templateFeature.status;
        ColorComparisonHalconResult result =
                errorResult(status, templateFeature.message, config, timer.elapsed());
        result.payload.insert(QStringLiteral("templateFeaturePayload"), templateFeature.payload);
        return result;
    }

    const bool circleMode = config.detectRegionType.trimmed().toLower() == QStringLiteral("circle");
    const ColorRecognitionHalconFeatureResult detectFeature =
            featureRunner.extractFeature(
                image,
                featureConfig(config,
                              config.detectRoiNormalized,
                              config.detectMaskPolygonNormalized,
                              circleMode));
    if (!detectFeature.success) {
        const QString status = detectFeature.status == QStringLiteral("masked_roi_empty")
                ? QStringLiteral("detect_masked_empty")
                : detectFeature.status == QStringLiteral("invalid_roi")
                  ? QStringLiteral("invalid_detect_roi")
                  : detectFeature.status;
        ColorComparisonHalconResult result =
                errorResult(status, detectFeature.message, config, timer.elapsed());
        result.payload.insert(QStringLiteral("templateFeaturePayload"), templateFeature.payload);
        result.payload.insert(QStringLiteral("detectFeaturePayload"), detectFeature.payload);
        return result;
    }

    const double similarity =
            histogramIntersectionSimilarity(templateFeature.feature, detectFeature.feature);
    const double score = qBound(0.0, similarity * 100.0, 100.0);
    const bool ok = score >= static_cast<double>(qBound(0, config.minScore, 100));

    ColorComparisonHalconResult result;
    result.success = true;
    result.ok = ok;
    result.status = ok ? QStringLiteral("ok") : QStringLiteral("ng");
    result.message = QStringLiteral("score=%1").arg(QString::number(score, 'f', 2));
    result.score = score;
    result.similarity = similarity;
    result.elapsedMs = timer.elapsed();
    result.templateFeature = templateFeature.feature;
    result.detectFeature = detectFeature.feature;
    result.overlays.append(rectOverlay(config.templateRoiNormalized,
                                       QStringLiteral("模板区域"),
                                       QStringLiteral("template_roi")));
    if (!circleMode) {
        result.overlays.append(rectOverlay(config.detectRoiNormalized,
                                           QStringLiteral("检测区域"),
                                           QStringLiteral("detect_roi")));
    }
    if (config.templateMaskPolygonNormalized.size() >= 3) {
        result.overlays.append(polygonOverlay(config.templateMaskPolygonNormalized,
                                             QStringLiteral("模板屏蔽"),
                                             QStringLiteral("template_mask")));
    }
    if (config.detectMaskPolygonNormalized.size() >= 3) {
        result.overlays.append(polygonOverlay(config.detectMaskPolygonNormalized,
                                             QStringLiteral("检测屏蔽"),
                                             QStringLiteral("detect_mask")));
    }
    result.overlays.append(textOverlay(ok ? QStringLiteral("OK") : QStringLiteral("NG"),
                                      score,
                                      ok));

    result.payload.insert(QStringLiteral("algorithm"),
                          QStringLiteral("halcon_hsv_histogram_comparison"));
    result.payload.insert(QStringLiteral("featureType"), QStringLiteral("histogram"));
    result.payload.insert(QStringLiteral("sensitivity"), config.sensitivity);
    result.payload.insert(QStringLiteral("brightnessEnabled"), config.brightnessEnabled);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));
    result.payload.insert(QStringLiteral("lightingNormalizationMode"),
                          config.brightnessEnabled
                          ? QStringLiteral("include_value_channel")
                          : QStringLiteral("hue_saturation_priority"));
    result.payload.insert(QStringLiteral("comparisonMethod"),
                          QStringLiteral("hsv_histogram_intersection"));
    result.payload.insert(QStringLiteral("scoreDirection"),
                          QStringLiteral("higher_is_better"));
    result.payload.insert(QStringLiteral("scoreFormula"),
                          QStringLiteral("histogram_intersection_similarity_x100"));
    result.payload.insert(QStringLiteral("similarity"), similarity);
    result.payload.insert(QStringLiteral("score"), score);
    result.payload.insert(QStringLiteral("histogramBins"),
                          histogramBinsForSensitivity(config.sensitivity));
    result.payload.insert(QStringLiteral("featureLength"), qMin(templateFeature.feature.size(),
                                                                detectFeature.feature.size()));
    result.payload.insert(QStringLiteral("templateFeature"), featureToJson(templateFeature.feature));
    result.payload.insert(QStringLiteral("detectFeature"), featureToJson(detectFeature.feature));
    result.payload.insert(QStringLiteral("templateRoiNormalized"), rectToJson(config.templateRoiNormalized));
    result.payload.insert(QStringLiteral("detectRoiNormalized"), rectToJson(config.detectRoiNormalized));
    result.payload.insert(QStringLiteral("templateMaskApplied"),
                          config.templateMaskPolygonNormalized.size() >= 3);
    result.payload.insert(QStringLiteral("detectMaskApplied"),
                          config.detectMaskPolygonNormalized.size() >= 3);
    result.payload.insert(QStringLiteral("templateMaskFullyCoversRoi"), false);
    result.payload.insert(QStringLiteral("detectMaskFullyCoversRoi"), false);
    result.payload.insert(QStringLiteral("effectiveTemplateRoiArea"),
                          templateFeature.payload.value(QStringLiteral("effectiveRoiArea")).toDouble());
    result.payload.insert(QStringLiteral("effectiveDetectRoiArea"),
                          detectFeature.payload.value(QStringLiteral("effectiveRoiArea")).toDouble());
    result.payload.insert(QStringLiteral("detectRegionType"),
                          circleMode ? QStringLiteral("circle") : QStringLiteral("rectangle"));
    result.payload.insert(QStringLiteral("templateMaskPolygon"),
                          pointsToJson(config.templateMaskPolygonNormalized));
    result.payload.insert(QStringLiteral("detectMaskPolygon"),
                          pointsToJson(config.detectMaskPolygonNormalized));
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
    result.payload.insert(QStringLiteral("judgeMode"), QStringLiteral("min_score"));
    result.payload.insert(QStringLiteral("minScore"), qBound(0, config.minScore, 100));
    result.payload.insert(QStringLiteral("templateFeaturePayload"), templateFeature.payload);
    result.payload.insert(QStringLiteral("detectFeaturePayload"), detectFeature.payload);
    return result;
}
