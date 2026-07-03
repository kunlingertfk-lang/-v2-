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

QRectF normalizedRectToPixels(const QRectF &normalizedRect, const cv::Mat &image)
{
    if (image.empty())
        return QRectF();

    const QRectF normalized = normalizedRect.normalized();
    const double left = qBound(0.0, normalized.left(), 1.0);
    const double top = qBound(0.0, normalized.top(), 1.0);
    const double right = qBound(0.0, normalized.right(), 1.0);
    const double bottom = qBound(0.0, normalized.bottom(), 1.0);
    const QRectF clamped(QPointF(left, top), QPointF(right, bottom));
    return QRectF(clamped.x() * image.cols,
                  clamped.y() * image.rows,
                  clamped.width() * image.cols,
                  clamped.height() * image.rows).normalized();
}

QVector<QPointF> normalizedPointsToPixels(const QVector<QPointF> &points, const cv::Mat &image)
{
    QVector<QPointF> pixels;
    if (image.empty())
        return pixels;

    pixels.reserve(points.size());
    for (const QPointF &point : points) {
        pixels.append(QPointF(qBound(0.0, point.x(), 1.0) * image.cols,
                              qBound(0.0, point.y(), 1.0) * image.rows));
    }
    return pixels;
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

bool isHisto2DimFeatureType(const QString &featureType)
{
    const QString normalized = featureType.trimmed().toLower();
    return normalized == QStringLiteral("histogram_2dim_hs") ||
            normalized == QStringLiteral("histo_2dim") ||
            normalized == QStringLiteral("histogram_2dim");
}

QString normalizedComparisonMode(const QString &mode)
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QStringLiteral("bhattacharyya") ||
        normalized == QStringLiteral("bhattacharyya_histogram") ||
        normalized == QStringLiteral("halcon_bhattacharyya"))
        return QStringLiteral("bhattacharyya_histogram");
    return QStringLiteral("dominant_hue_coverage");
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
    result.payload.insert(QStringLiteral("comparisonMode"), normalizedComparisonMode(config.comparisonMode));
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));
    result.payload.insert(QStringLiteral("comparisonMethod"),
                          QStringLiteral("hsv_histogram_soft_kernel_weighted"));
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

QVector<double> smoothedHistogram(const QVector<double> &histogram, bool circular)
{
    if (histogram.isEmpty())
        return histogram;

    QVector<double> smoothed;
    smoothed.resize(histogram.size());
    for (int i = 0; i < histogram.size(); ++i) {
        const int previous = i == 0 ? (circular ? histogram.size() - 1 : 0) : i - 1;
        const int next = i == histogram.size() - 1 ? (circular ? 0 : histogram.size() - 1) : i + 1;
        smoothed[i] = histogram.at(i) * 0.6 +
                histogram.at(previous) * 0.2 +
                histogram.at(next) * 0.2;
    }

    double sum = 0.0;
    for (double value : smoothed)
        sum += qMax(0.0, value);
    if (sum > 0.0) {
        for (double &value : smoothed)
            value = qMax(0.0, value) / sum;
    }
    return smoothed;
}

double colorBinDistance(int lhs, int rhs, int bins, bool circular)
{
    const int direct = std::abs(lhs - rhs);
    if (!circular)
        return static_cast<double>(direct);
    return static_cast<double>(std::min(direct, bins - direct));
}

double colorKernelSigma(int bins, bool hueChannel)
{
    if (hueChannel) {
        if (bins <= 8)
            return 1.5;
        if (bins >= 32)
            return 4.0;
        return 2.6;
    }
    if (bins <= 8)
        return 1.0;
    if (bins >= 32)
        return 2.0;
    return 1.5;
}

int dominantHistogramBin(const QVector<double> &histogram)
{
    int bestIndex = -1;
    double bestValue = 0.0;
    for (int i = 0; i < histogram.size(); ++i) {
        const double value = qMax(0.0, histogram.at(i));
        if (value > bestValue) {
            bestValue = value;
            bestIndex = i;
        }
    }
    return bestIndex;
}

double dominantHueSigma(int bins)
{
    if (bins <= 8)
        return 1.25;
    if (bins >= 32)
        return 3.4;
    return 1.7;
}

double templateDominantHueCoverage(const QVector<double> &templateHue,
                                   const QVector<double> &detectHue)
{
    const int count = qMin(templateHue.size(), detectHue.size());
    const int dominantBin = dominantHistogramBin(templateHue.mid(0, count));
    if (count <= 0 || dominantBin < 0)
        return 0.0;

    double detectSum = 0.0;
    for (int i = 0; i < count; ++i)
        detectSum += qMax(0.0, detectHue.at(i));
    if (detectSum <= 0.0)
        return 0.0;

    const double sigma = dominantHueSigma(count);
    const double denominator = 2.0 * sigma * sigma;
    double coverage = 0.0;
    for (int i = 0; i < count; ++i) {
        const double detectValue = qMax(0.0, detectHue.at(i)) / detectSum;
        if (detectValue <= 0.0)
            continue;
        const double distance = colorBinDistance(dominantBin, i, count, true);
        const double weight = std::exp(-(distance * distance) / denominator);
        coverage += detectValue * weight;
    }
    return qBound(0.0, coverage, 1.0);
}

double histogramSoftSimilarity(const QVector<double> &a,
                               const QVector<double> &b,
                               bool circular)
{
    const int count = qMin(a.size(), b.size());
    if (count <= 0)
        return 0.0;

    double sumA = 0.0;
    double sumB = 0.0;
    for (int i = 0; i < count; ++i) {
        sumA += qMax(0.0, a.at(i));
        sumB += qMax(0.0, b.at(i));
    }
    if (sumA <= 0.0 || sumB <= 0.0)
        return 0.0;

    const double sigma = colorKernelSigma(count, circular);
    const double denominator = 2.0 * sigma * sigma;
    double similarity = 0.0;
    for (int i = 0; i < count; ++i) {
        const double av = qMax(0.0, a.at(i)) / sumA;
        if (av <= 0.0)
            continue;
        for (int j = 0; j < count; ++j) {
            const double bv = qMax(0.0, b.at(j)) / sumB;
            if (bv <= 0.0)
                continue;
            const double distance = colorBinDistance(i, j, count, circular);
            const double weight = std::exp(-(distance * distance) / denominator);
            similarity += av * bv * weight;
        }
    }
    return qBound(0.0, similarity, 1.0);
}

ColorComparisonHsvSimilarity compareHsvHistogramFeature(const QVector<double> &templateFeature,
                                                        const QVector<double> &detectFeature,
                                                        const ColorComparisonHalconConfig &config)
{
    return compareColorComparisonHsvHistograms(templateFeature,
                                              detectFeature,
                                              histogramBinsForSensitivity(config.sensitivity),
                                              config.brightnessEnabled);
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

ToolOverlay circleOverlay(const QPointF &center, double radius, const QString &label, const QString &role)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Circle;
    overlay.center = center;
    overlay.radius = radius;
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

ToolOverlay textOverlay(const QPointF &position,
                        const QString &text,
                        double score,
                        bool ok,
                        const QRectF &anchorRect)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.text = text;
    overlay.label = QStringLiteral("color_result_text");
    overlay.score = score;
    overlay.p1 = position;
    overlay.extra.insert(QStringLiteral("status"), ok ? QStringLiteral("OK") : QStringLiteral("NG"));
    overlay.extra.insert(QStringLiteral("anchorRect"), rectToJson(anchorRect));
    return overlay;
}

} // namespace

ColorComparisonHsvSimilarity compareColorComparisonHsvHistograms(
        const QVector<double> &templateFeature,
        const QVector<double> &detectFeature,
        int bins,
        bool brightnessEnabled)
{
    ColorComparisonHsvSimilarity result;
    result.bins = bins;
    result.brightnessUsed = brightnessEnabled;

    const int minSize = qMin(templateFeature.size(), detectFeature.size());
    if (bins <= 0 || minSize < bins * 2)
        return result;

    const QVector<double> templateHue =
            smoothedHistogram(templateFeature.mid(0, bins), true);
    const QVector<double> detectHue =
            smoothedHistogram(detectFeature.mid(0, bins), true);
    const QVector<double> templateSaturation =
            smoothedHistogram(templateFeature.mid(bins, bins), false);
    const QVector<double> detectSaturation =
            smoothedHistogram(detectFeature.mid(bins, bins), false);

    const double hueSoftSimilarity = histogramSoftSimilarity(templateHue, detectHue, true);
    const double hueDominantCoverage = templateDominantHueCoverage(templateHue, detectHue);
    result.hue = qBound(0.0, hueDominantCoverage * 0.80 + hueSoftSimilarity * 0.20, 1.0);
    result.saturation = histogramSoftSimilarity(templateSaturation, detectSaturation, false);

    if (brightnessEnabled && minSize >= bins * 3) {
        const QVector<double> templateValue =
                smoothedHistogram(templateFeature.mid(bins * 2, bins), false);
        const QVector<double> detectValue =
                smoothedHistogram(detectFeature.mid(bins * 2, bins), false);
        result.value = histogramSoftSimilarity(templateValue, detectValue, false);
        result.combined = qBound(0.0,
                                 result.hue * 0.70 +
                                 result.saturation * 0.15 +
                                 result.value * 0.15,
                                 1.0);
    } else {
        result.brightnessUsed = false;
        result.combined = qBound(0.0,
                                 result.hue * 0.80 +
                                 result.saturation * 0.20,
                                 1.0);
    }

    return result;
}

ColorComparisonHalconResult ColorComparisonHalconRunner::run(
        const cv::Mat &image,
        const ColorComparisonHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    const QString featureType = config.featureType.trimmed().toLower();
    const QString comparisonMode = normalizedComparisonMode(config.comparisonMode);
    if (featureType == QStringLiteral("spectrum")) {//色谱特征
        return errorResult(QStringLiteral("unsupported_feature"),
                           QStringLiteral("Spectrum feature is reserved but not implemented."),
                           config,
                           timer.elapsed());
    }
    if (featureType != QStringLiteral("histogram") && !isHisto2DimFeatureType(featureType)) {//直方图特征
        return errorResult(QStringLiteral("unsupported_feature"),
                           QStringLiteral("Only histogram and histogram_2dim_hs features are supported."),
                           config,
                           timer.elapsed());
    }
    if (isHisto2DimFeatureType(featureType) &&
            comparisonMode != QStringLiteral("bhattacharyya_histogram")) {
        return errorResult(QStringLiteral("unsupported_feature"),
                           QStringLiteral("histogram_2dim_hs is only supported by Bhattacharyya histogram mode."),
                           config,
                           timer.elapsed());
    }
    if (image.empty()) {
        return errorResult(QStringLiteral("image_empty"),
                           QStringLiteral("Input image is empty."),
                           config,
                           timer.elapsed());
    }
    if (!validRect(config.templateRoiNormalized)) { //模板roi有效
        return errorResult(QStringLiteral("invalid_template_roi"),
                           QStringLiteral("Template ROI is invalid."),
                           config,
                           timer.elapsed());
    }
    if (!validRect(config.detectRoiNormalized)) { //检测roi有效
        return errorResult(QStringLiteral("invalid_detect_roi"),
                           QStringLiteral("Detect ROI is invalid."),
                           config,
                           timer.elapsed());
    }
    if (config.templateFeature.isEmpty()) { //模板特征是否取到
        return errorResult(QStringLiteral("no_template_feature"),
                           QStringLiteral("Saved template feature is empty. Run reference-image modeling first."),
                           config,
                           timer.elapsed());
    }

    ColorRecognitionHalconRunner featureRunner;
    ColorRecognitionHalconFeatureResult templateFeature;
    templateFeature.success = true;
    templateFeature.status = QStringLiteral("ok");
    templateFeature.message = QStringLiteral("saved template feature loaded");
    templateFeature.feature = config.templateFeature;
    templateFeature.payload.insert(QStringLiteral("source"), QStringLiteral("saved_config"));
    templateFeature.payload.insert(QStringLiteral("featureLength"), templateFeature.feature.size());
    templateFeature.payload.insert(QStringLiteral("histogramBins"),
                                   histogramBinsForSensitivity(config.sensitivity));
    templateFeature.payload.insert(QStringLiteral("brightnessEnabled"), config.brightnessEnabled);

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

    ColorComparisonHsvSimilarity channelSimilarity;
    double bhattacharyyaDistance = -1.0;
    double similarity = 0.0;

    if (comparisonMode == QStringLiteral("bhattacharyya_histogram")) {
        ColorRecognitionHalconConfig compareConfig =
                featureConfig(config,
                              config.detectRoiNormalized,
                              config.detectMaskPolygonNormalized,
                              circleMode);
        const ColorRecognitionHalconHistogramCompareResult histogramComparison =
                featureRunner.compareHistogramBhattacharyya(templateFeature.feature,
                                                            detectFeature.feature,
                                                            compareConfig);
        if (!histogramComparison.success) {
            ColorComparisonHalconResult result =
                    errorResult(histogramComparison.status,
                                histogramComparison.message,
                                config,
                                timer.elapsed());
            result.payload.insert(QStringLiteral("templateFeaturePayload"), templateFeature.payload);
            result.payload.insert(QStringLiteral("detectFeaturePayload"), detectFeature.payload);
            result.payload.insert(QStringLiteral("histogramComparePayload"), histogramComparison.payload);
            return result;
        }
        bhattacharyyaDistance = qMax(0.0, histogramComparison.distance);
        similarity = qBound(0.0, 1.0 - bhattacharyyaDistance, 1.0);
    } else {
        channelSimilarity = compareHsvHistogramFeature(templateFeature.feature, detectFeature.feature, config);
        similarity = channelSimilarity.combined;
    }

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
    const QRectF templateRoiPixels = normalizedRectToPixels(config.templateRoiNormalized, image);
    const QRectF detectRoiPixels = normalizedRectToPixels(config.detectRoiNormalized, image);
    const QRectF detectCirclePixels = normalizedRectToPixels(
                validRect(config.detectCircleBoundingRectNormalized)
                ? config.detectCircleBoundingRectNormalized
                : config.detectRoiNormalized,
                image);
    const QRectF resultAnchorRect = circleMode && detectCirclePixels.isValid()
            ? detectCirclePixels
            : detectRoiPixels;

    result.overlays.append(rectOverlay(templateRoiPixels,
                                       QStringLiteral("template_roi"),
                                       QStringLiteral("template_roi")));
    if (!circleMode) {
        result.overlays.append(rectOverlay(detectRoiPixels,
                                           QStringLiteral("detect_roi"),
                                           QStringLiteral("detect_roi")));
    } else if (config.detectCircleRadiusNormalized > 0.0) {
        const double radiusPixels = config.detectCircleRadiusNormalized *
                static_cast<double>(qMax(image.cols, image.rows));
        result.overlays.append(circleOverlay(QPointF(config.detectCircleCenterNormalized.x() * image.cols,
                                                     config.detectCircleCenterNormalized.y() * image.rows),
                                            radiusPixels,
                                            QStringLiteral("detect_roi"),
                                            QStringLiteral("detect_circle")));
    }
    if (config.templateMaskPolygonNormalized.size() >= 3) {
        result.overlays.append(polygonOverlay(normalizedPointsToPixels(config.templateMaskPolygonNormalized, image),
                                             QStringLiteral("template_mask"),
                                             QStringLiteral("template_mask")));
    }
    if (config.detectMaskPolygonNormalized.size() >= 3) {
        result.overlays.append(polygonOverlay(normalizedPointsToPixels(config.detectMaskPolygonNormalized, image),
                                             QStringLiteral("detect_mask"),
                                             QStringLiteral("detect_mask")));
    }
    result.overlays.append(textOverlay(QPointF(resultAnchorRect.x(), resultAnchorRect.y()),
                                      QStringLiteral("%1 score:%2")
                                      .arg(ok ? QStringLiteral("OK") : QStringLiteral("NG"),
                                           QString::number(score, 'f', 1)),
                                      score,
                                      ok,
                                      resultAnchorRect));

    result.payload.insert(QStringLiteral("algorithm"),
                          QStringLiteral("halcon_hsv_histogram_comparison"));
    result.payload.insert(QStringLiteral("featureType"),
                          isHisto2DimFeatureType(config.featureType)
                          ? QStringLiteral("histogram_2dim_hs")
                          : QStringLiteral("histogram"));
    result.payload.insert(QStringLiteral("sensitivity"), config.sensitivity);
    result.payload.insert(QStringLiteral("brightnessEnabled"), config.brightnessEnabled);
    result.payload.insert(QStringLiteral("comparisonMode"), comparisonMode);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));
    result.payload.insert(QStringLiteral("lightingNormalizationMode"),
                          isHisto2DimFeatureType(config.featureType)
                          ? QStringLiteral("hue_saturation_2dim_soft_kernel")
                          :
                          config.brightnessEnabled
                          ? QStringLiteral("include_value_channel")
                          : QStringLiteral("hue_saturation_priority"));
    result.payload.insert(QStringLiteral("comparisonMethod"),
                          comparisonMode == QStringLiteral("bhattacharyya_histogram")
                          ? QStringLiteral("halcon_tuple_bhattacharyya_histogram")
                          : QStringLiteral("hsv_histogram_soft_kernel_weighted"));
    result.payload.insert(QStringLiteral("scoreDirection"),
                          QStringLiteral("higher_is_better"));
        result.payload.insert(QStringLiteral("scoreFormula"),
                              comparisonMode == QStringLiteral("bhattacharyya_histogram")
                              ? QStringLiteral("max(0.0, 1.0 - bhattacharyya_distance) * 100")
                              : QStringLiteral("template_dominant_hue_coverage_weighted_similarity_x100"));
    result.payload.insert(QStringLiteral("similarity"), similarity);
    if (comparisonMode == QStringLiteral("bhattacharyya_histogram")) {
        result.payload.insert(QStringLiteral("distance"), bhattacharyyaDistance);
        result.payload.insert(QStringLiteral("halconOperators"),
                              isHisto2DimFeatureType(config.featureType)
                              ? QStringLiteral("T_histo_2dim,T_get_grayval,T_tuple_mult,T_tuple_sqrt,T_tuple_sum")
                              : QStringLiteral("T_gray_histo_range,T_tuple_mult,T_tuple_sqrt,T_tuple_sum"));
        result.payload.insert(QStringLiteral("halconCompareMethod"),
                              QStringLiteral("bhattacharyya_by_base_tuple_operators"));
    } else {
        result.payload.insert(QStringLiteral("hueSimilarity"), channelSimilarity.hue);
        result.payload.insert(QStringLiteral("saturationSimilarity"), channelSimilarity.saturation);
        result.payload.insert(QStringLiteral("valueSimilarity"), channelSimilarity.value);
        result.payload.insert(QStringLiteral("hsvWeights"),
                              channelSimilarity.brightnessUsed
                              ? QStringLiteral("hue=0.70,saturation=0.15,value=0.15")
                              : QStringLiteral("hue=0.80,saturation=0.20"));
        result.payload.insert(QStringLiteral("hueComparisonMode"),
                              QStringLiteral("template_dominant_hue_coverage_0.80_plus_histogram_soft_similarity_0.20"));
        result.payload.insert(QStringLiteral("histogramSmoothing"),
                              QStringLiteral("neighbor_bins_0.6_0.2_0.2_plus_soft_distance_kernel"));
    }
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
