#include "algorithms/presence/ContourPresenceHalconRunner.h"

#include "algorithms/presence/PatternPresenceHalconApi.h"

#include <HalconC.h>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QRect>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <opencv2/core.hpp>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kMinRoiPixelSize = 2;
constexpr int kMaxContourOverlayLineSegments = 2048;
constexpr int kMinTemplateContourPointCount = 6;
constexpr double kSelectContourMaxLength = 1000000000.0;

struct GrayHalconImage
{
    Hobject inputImage = NO_OBJECTS;
    Hobject grayImage = NO_OBJECTS;
    Hobject graySource = NO_OBJECTS;
};

struct HalconFailure
{
    QString status;
    QString message;
    QString stage;
    Herror code = H_MSG_OK;
    QString halconMessage;
};

struct ScaleRangeSettings
{
    double minScale = 1.0;
    double maxScale = 1.0;
    bool fallback = false;
    bool autoModeExpandedDefault = false;
    QString fallbackReason;
};

struct ThresholdTypeSettings
{
    QString type = QStringLiteral("auto");
    bool fallback = false;
};

struct ContourExtractionSettings
{
    QString thresholdMode = QStringLiteral("auto");
    ThresholdTypeSettings thresholdType;
    QString chainMode = QStringLiteral("auto");
    int minChainLength = 4;
    double sigma = 1.0;
    bool manualThreshold = false;
};

struct XldReadResult
{
    QVector<QVector<QPointF>> contours;
    QVector<double> lengths;
    int objectCount = 0;
    int pointCount = 0;
    double totalLength = 0.0;
    QString error;
};

struct ContourCandidate
{
    Hobject contours = NO_OBJECTS;
    XldReadResult stats;
    QString mode;
    QString fallback;
    Hlong autoThreshold = -1;
};

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

QJsonArray doublesToJson(const QVector<double> &values)
{
    QJsonArray array;
    for (const double value : values)
        array.append(value);
    return array;
}

bool isFiniteRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) &&
           std::isfinite(rect.y()) &&
           std::isfinite(rect.width()) &&
           std::isfinite(rect.height());
}

bool isValidNormalizedRoi(const QRectF &rect)
{
    return isFiniteRect(rect) && rect.width() > 0.0 && rect.height() > 0.0;
}

QRect normalizedRoiToPixels(const QRectF &sourceRoi, const int width, const int height)
{
    if (width <= 0 || height <= 0 || !isValidNormalizedRoi(sourceRoi))
        return QRect();

    const QRectF roi = sourceRoi.normalized();
    const double left = qBound(0.0, roi.left(), 1.0);
    const double top = qBound(0.0, roi.top(), 1.0);
    const double right = qBound(0.0, roi.right(), 1.0);
    const double bottom = qBound(0.0, roi.bottom(), 1.0);
    if (right <= left || bottom <= top)
        return QRect();

    const int x1 = qBound(0, static_cast<int>(std::floor(left * width)), width - 1);
    const int y1 = qBound(0, static_cast<int>(std::floor(top * height)), height - 1);
    const int x2Exclusive = qBound(0, static_cast<int>(std::ceil(right * width)), width);
    const int y2Exclusive = qBound(0, static_cast<int>(std::ceil(bottom * height)), height);

    const int roiWidth = x2Exclusive - x1;
    const int roiHeight = y2Exclusive - y1;
    if (roiWidth < kMinRoiPixelSize || roiHeight < kMinRoiPixelSize)
        return QRect();

    return QRect(x1, y1, roiWidth, roiHeight);
}

QVector<QPointF> normalizedPolygonToPixels(const QVector<QPointF> &points,
                                           const int width,
                                           const int height)
{
    QVector<QPointF> pixelPoints;
    if (width <= 0 || height <= 0)
        return pixelPoints;

    pixelPoints.reserve(points.size());
    for (const QPointF &point : points) {
        pixelPoints.append(QPointF(qBound(0.0, point.x(), 1.0) * width,
                                   qBound(0.0, point.y(), 1.0) * height));
    }
    return pixelPoints;
}

QVector<QPointF> polygonToLocalClamped(const QVector<QPointF> &globalPoints,
                                       const QRect &boundingRect,
                                       const int width,
                                       const int height)
{
    QVector<QPointF> localPoints;
    if (width <= 0 || height <= 0)
        return localPoints;

    localPoints.reserve(globalPoints.size());
    for (const QPointF &point : globalPoints) {
        const double localX = point.x() - static_cast<double>(boundingRect.x());
        const double localY = point.y() - static_cast<double>(boundingRect.y());
        localPoints.append(QPointF(qBound(0.0, localX, static_cast<double>(width - 1)),
                                   qBound(0.0, localY, static_cast<double>(height - 1))));
    }
    return localPoints;
}

double polygonAreaPixels(const QVector<QPointF> &points)
{
    if (points.size() < 3)
        return 0.0;

    double sum = 0.0;
    for (int index = 0; index < points.size(); ++index) {
        const QPointF &a = points.at(index);
        const QPointF &b = points.at((index + 1) % points.size());
        sum += a.x() * b.y() - b.x() * a.y();
    }
    return std::abs(sum) * 0.5;
}

bool isRectType(const QString &value)
{
    const QString key = value.trimmed().toLower();
    return key.isEmpty() ||
           key == QStringLiteral("rect") ||
           key == QStringLiteral("rectangle") ||
           key.contains(QStringLiteral("矩形"));
}

bool isPolygonType(const QString &value)
{
    const QString key = value.trimmed().toLower();
    return key == QStringLiteral("polygon") ||
           key == QStringLiteral("poly") ||
           key.contains(QStringLiteral("多边形"));
}

bool isUnsupportedDetectType(const QString &value)
{
    const QString key = value.trimmed().toLower();
    return key == QStringLiteral("circle") ||
           key == QStringLiteral("free") ||
           key == QStringLiteral("freehand") ||
           key.contains(QStringLiteral("圆")) ||
           key.contains(QStringLiteral("自由"));
}

bool isManualMode(const QString &value)
{
    const QString key = value.trimmed().toLower();
    return key == QStringLiteral("manual") ||
           key == QStringLiteral("hand") ||
           key.contains(QStringLiteral("手动"));
}

bool isPresenceJudgeBasis(const QString &judgeBasis)
{
    const QString key = judgeBasis.trimmed().toLower();
    return key == QStringLiteral("presence") ||
           key == QStringLiteral("result_presence") ||
           key == QStringLiteral("hasresult") ||
           key == QStringLiteral("existence") ||
           key.contains(QStringLiteral("有无"));
}

bool isScoreJudgeBasis(const QString &judgeBasis)
{
    const QString key = judgeBasis.trimmed().toLower();
    return key == QStringLiteral("score") ||
           key == QStringLiteral("minscore") ||
           key == QStringLiteral("min_score") ||
           key == QStringLiteral("lowestscore") ||
           key.contains(QStringLiteral("得分"));
}

double normalizedScoreValue(const double value)
{
    if (value > 1.0)
        return qBound(0.0, value / 100.0, 1.0);
    return qBound(0.0, value, 1.0);
}

ScaleRangeSettings scaleRangeSettings(const ContourPresenceHalconConfig &config)
{
    ScaleRangeSettings settings;
    double minScale = config.scaleMin;
    double maxScale = config.scaleMax;
    const bool rawDefaultPercentRange = std::abs(minScale - 100.0) < 0.0001 &&
                                        std::abs(maxScale - 100.0) < 0.0001;
    const bool rawDefaultFactorRange = std::abs(minScale - 1.0) < 0.0001 &&
                                       std::abs(maxScale - 1.0) < 0.0001;
    if (minScale > 10.0 || maxScale > 10.0) {
        minScale /= 100.0;
        maxScale /= 100.0;
    }

    if (!std::isfinite(minScale) ||
        !std::isfinite(maxScale) ||
        minScale <= 0.0 ||
        maxScale <= 0.0 ||
        minScale > maxScale) {
        settings.minScale = 0.9;
        settings.maxScale = 1.1;
        settings.fallback = true;
        settings.fallbackReason = QStringLiteral("invalid scale range; fallback to 0.9-1.1");
        return settings;
    }

    const QString scaleModeKey = config.scaleMode.trimmed().toLower();
    const bool autoScaleMode = scaleModeKey.isEmpty() ||
                               scaleModeKey == QStringLiteral("auto") ||
                               scaleModeKey.contains(QStringLiteral("自动"));
    if (autoScaleMode &&
        (rawDefaultPercentRange || rawDefaultFactorRange) &&
        std::abs(minScale - 1.0) < 0.0001 &&
        std::abs(maxScale - 1.0) < 0.0001) {
        settings.minScale = 0.9;
        settings.maxScale = 1.1;
        settings.autoModeExpandedDefault = true;
        return settings;
    }

    settings.minScale = minScale;
    settings.maxScale = maxScale;
    return settings;
}

ThresholdTypeSettings thresholdTypeSettings(const QString &thresholdType)
{
    ThresholdTypeSettings settings;
    const QString key = thresholdType.trimmed().toLower();
    if (key.isEmpty() ||
        key == QStringLiteral("auto") ||
        key == QStringLiteral("any") ||
        key.contains(QStringLiteral("任意")) ||
        key.contains(QStringLiteral("自动"))) {
        settings.type = QStringLiteral("auto");
        return settings;
    }
    if (key == QStringLiteral("light") ||
        key == QStringLiteral("bright") ||
        key.contains(QStringLiteral("亮")) ||
        key.contains(QStringLiteral("白"))) {
        settings.type = QStringLiteral("light");
        return settings;
    }
    if (key == QStringLiteral("dark") ||
        key == QStringLiteral("black") ||
        key.contains(QStringLiteral("暗")) ||
        key.contains(QStringLiteral("黑"))) {
        settings.type = QStringLiteral("dark");
        return settings;
    }

    settings.type = QStringLiteral("auto");
    settings.fallback = true;
    return settings;
}

QString metricForPolarity(const QString &polarity, bool *fallback)
{
    const QString key = polarity.trimmed().toLower();
    if (fallback)
        *fallback = false;
    if (key == QStringLiteral("use_polarity") ||
        key.contains(QStringLiteral("use")) ||
        key.contains(QStringLiteral("考虑")) ||
        key.contains(QStringLiteral("使用"))) {
        return QStringLiteral("use_polarity");
    }
    if (key == QStringLiteral("ignore_global_polarity") ||
        key.contains(QStringLiteral("global")) ||
        key.contains(QStringLiteral("全局"))) {
        return QStringLiteral("ignore_global_polarity");
    }
    if (key == QStringLiteral("ignore_local_polarity") ||
        key == QStringLiteral("ignore_polarity") ||
        key.contains(QStringLiteral("local")) ||
        key.contains(QStringLiteral("ignore")) ||
        key.contains(QStringLiteral("忽略")) ||
        key.contains(QStringLiteral("任意"))) {
        return QStringLiteral("ignore_local_polarity");
    }

    if (fallback)
        *fallback = true;
    return QStringLiteral("ignore_local_polarity");
}

double sigmaFromFeatureScale(const int featureScale)
{
    return qBound(0.8, 0.5 + static_cast<double>(qBound(1, featureScale, 999)) * 0.2, 3.0);
}

double greedinessFromSpeedScale(const int speedScale)
{
    return qBound(0.5, 0.45 + static_cast<double>(qBound(1, speedScale, 999)) * 0.05, 0.9);
}

int minChainLengthUsed(const ContourPresenceHalconConfig &config, const QRect &templateRoiPixels)
{
    if (isManualMode(config.chainMode))
        return qMax(1, config.minChainLength);

    const int roiMinSide = qMax(1, qMin(templateRoiPixels.width(), templateRoiPixels.height()));
    const double speedFactor = 1.0 + static_cast<double>(qBound(-4, config.speedScale - 5, 12)) * 0.04;
    return qBound(4, qRound(static_cast<double>(roiMinSide) * 0.035 * speedFactor), 80);
}

ToolOverlay rectOverlay(const QRectF &rect, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.rect = rect;
    overlay.label = label;
    overlay.score = score;
    overlay.extra.insert(QStringLiteral("overlayName"), label);
    return overlay;
}

ToolOverlay polygonOverlay(const QVector<QPointF> &points, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Polygon;
    overlay.points = points;
    overlay.label = label;
    overlay.score = score;
    overlay.extra.insert(QStringLiteral("overlayName"), label);
    return overlay;
}

ToolOverlay lineOverlay(const QPointF &p1, const QPointF &p2, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Line;
    overlay.p1 = p1;
    overlay.p2 = p2;
    overlay.label = label;
    overlay.score = score;
    overlay.extra.insert(QStringLiteral("overlayName"), label);
    return overlay;
}

ToolOverlay textOverlay(const QPointF &position, const QString &text, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.p1 = position;
    overlay.text = text;
    overlay.label = label;
    overlay.score = score;
    overlay.extra.insert(QStringLiteral("overlayName"), label);
    return overlay;
}

void destroyTuple(PatternPresenceHalconApi *api, Htuple &tuple)
{
    if (api && api->destroyTuple && (tuple.num > 0 || tuple.capacity > 0))
        api->destroyTuple(&tuple);
    tuple = HTUPLE_INITIALIZER;
}

void clearObject(PatternPresenceHalconApi *api, Hobject &object)
{
    if (api && api->clearObj && patternPresenceHalconObjectAllocated(object))
        api->clearObj(object);
    object = NO_OBJECTS;
}

XldReadResult readXldContours(PatternPresenceHalconApi *api, const Hobject contours)
{
    XldReadResult result;
    if (!api || !api->countObj || !api->selectObj || !api->getContourXld || !api->lengthXld) {
        result.error = QStringLiteral("required XLD read operators unavailable");
        return result;
    }

    Hlong objectCount = 0;
    if (!patternPresenceHalconStatusOk(api->countObj(contours, &objectCount))) {
        result.error = QStringLiteral("count_obj failed");
        return result;
    }
    result.objectCount = qMax(0, static_cast<int>(objectCount));
    result.contours.reserve(result.objectCount);
    result.lengths.reserve(result.objectCount);

    for (Hlong objectIndex = 1; objectIndex <= objectCount; ++objectIndex) {
        Hobject selected = NO_OBJECTS;
        Htuple rows = HTUPLE_INITIALIZER;
        Htuple columns = HTUPLE_INITIALIZER;
        Htuple lengthTuple = HTUPLE_INITIALIZER;

        if (!patternPresenceHalconStatusOk(api->selectObj(contours, &selected, objectIndex))) {
            result.error = QStringLiteral("select_obj failed");
            clearObject(api, selected);
            return result;
        }

        double length = 0.0;
        if (patternPresenceHalconStatusOk(api->lengthXld(selected, &lengthTuple)) && lengthTuple.num > 0)
            length = api->getDouble(&lengthTuple, 0);
        destroyTuple(api, lengthTuple);

        if (!patternPresenceHalconStatusOk(api->getContourXld(selected, &rows, &columns))) {
            result.error = QStringLiteral("get_contour_xld failed");
            destroyTuple(api, columns);
            destroyTuple(api, rows);
            clearObject(api, selected);
            return result;
        }

        const int pointCount = qMin<int>(rows.num, columns.num);
        QVector<QPointF> points;
        points.reserve(qMax(0, pointCount));
        for (int pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
            const double row = api->getDouble(&rows, pointIndex);
            const double column = api->getDouble(&columns, pointIndex);
            points.append(QPointF(column, row));
        }

        result.pointCount += points.size();
        result.totalLength += length;
        result.lengths.append(length);
        if (!points.isEmpty())
            result.contours.append(points);

        destroyTuple(api, columns);
        destroyTuple(api, rows);
        clearObject(api, selected);
    }

    return result;
}

int contourSegmentCount(const QVector<QVector<QPointF>> &contours)
{
    int count = 0;
    for (const QVector<QPointF> &contour : contours) {
        if (contour.size() >= 2)
            count += contour.size() - 1;
    }
    return count;
}

QVector<QVector<QPointF>> translatedContours(const QVector<QVector<QPointF>> &contours,
                                             const QPointF &offset)
{
    QVector<QVector<QPointF>> translated;
    translated.reserve(contours.size());
    for (const QVector<QPointF> &contour : contours) {
        QVector<QPointF> points;
        points.reserve(contour.size());
        for (const QPointF &point : contour)
            points.append(point + offset);
        translated.append(points);
    }
    return translated;
}

int appendContourLineOverlays(QVector<ToolOverlay> *overlays,
                              const QVector<QVector<QPointF>> &contours,
                              const double score,
                              int *displayPointCount)
{
    if (!overlays)
        return 0;

    const int totalSegments = qMax(1, contourSegmentCount(contours));
    const int sampleStep = qMax(1,
                                static_cast<int>(std::ceil(static_cast<double>(totalSegments) /
                                                           static_cast<double>(kMaxContourOverlayLineSegments))));
    int lineSegmentCount = 0;
    int pointCount = 0;

    for (const QVector<QPointF> &contour : contours) {
        if (contour.size() < 2)
            continue;
        if (lineSegmentCount >= kMaxContourOverlayLineSegments)
            break;

        int previousIndex = 0;
        QPointF previousPoint = contour.at(previousIndex);
        bool countedFirstPoint = false;
        auto appendLineTo = [&](const int pointIndex) {
            if (pointIndex <= previousIndex ||
                pointIndex >= contour.size() ||
                lineSegmentCount >= kMaxContourOverlayLineSegments) {
                return;
            }

            const QPointF currentPoint = contour.at(pointIndex);
            overlays->append(lineOverlay(previousPoint,
                                         currentPoint,
                                         QStringLiteral("contour_model_line"),
                                         score));
            ++lineSegmentCount;
            if (!countedFirstPoint) {
                ++pointCount;
                countedFirstPoint = true;
            }
            ++pointCount;
            previousPoint = currentPoint;
            previousIndex = pointIndex;
        };

        for (int pointIndex = sampleStep;
             pointIndex < contour.size() && lineSegmentCount < kMaxContourOverlayLineSegments;
             pointIndex += sampleStep) {
            appendLineTo(pointIndex);
        }
        appendLineTo(contour.size() - 1);
    }

    if (displayPointCount)
        *displayPointCount = pointCount;
    return lineSegmentCount;
}

void fillPayload(ContourPresenceHalconResult &result,
                 const cv::Mat &image,
                 const cv::Mat &referenceImage,
                 const ContourPresenceHalconConfig &config)
{
    const ScaleRangeSettings scaleSettings = scaleRangeSettings(config);
    const ThresholdTypeSettings thresholdSettings = thresholdTypeSettings(config.thresholdType);
    bool polarityFallback = false;
    const QString metric = metricForPolarity(config.polarity, &polarityFallback);
    const bool manualThreshold = isManualMode(config.thresholdMode);
    const double sigma = sigmaFromFeatureScale(config.featureScale);
    const double greediness = greedinessFromSpeedScale(config.speedScale);

    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("xld_contour_template_matching"));
    result.payload.insert(QStringLiteral("modelType"), QStringLiteral("scaled_shape_model_xld"));
    result.payload.insert(QStringLiteral("shapeModelOperator"),
                          QStringLiteral("create_scaled_shape_model_xld/find_scaled_shape_model"));
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"),
                          config.halconSoPathCandidates.join(QStringLiteral("; ")));
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("hasReferenceImage"), !referenceImage.empty());
    result.payload.insert(QStringLiteral("referenceImageWidth"), referenceImage.empty() ? 0 : referenceImage.cols);
    result.payload.insert(QStringLiteral("referenceImageHeight"), referenceImage.empty() ? 0 : referenceImage.rows);
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("templateRoiNormalized"), rectToJson(config.templateRoiNormalized));
    result.payload.insert(QStringLiteral("templatePolygonNormalized"),
                          pointsToJson(config.templatePolygonNormalized));
    result.payload.insert(QStringLiteral("templateSource"), config.templateSource);
    result.payload.insert(QStringLiteral("templateShapeType"), config.templateShapeType);
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("detectPolygonNormalized"),
                          pointsToJson(config.detectPolygonNormalized));
    result.payload.insert(QStringLiteral("referenceTest"), config.referenceTest);
    result.payload.insert(QStringLiteral("found"), false);
    result.payload.insert(QStringLiteral("matchCount"), 0);
    result.payload.insert(QStringLiteral("foundCount"), 0);
    result.payload.insert(QStringLiteral("bestScore"), 0.0);
    result.payload.insert(QStringLiteral("bestAngleDeg"), 0.0);
    result.payload.insert(QStringLiteral("bestScale"), 0.0);
    result.payload.insert(QStringLiteral("bestRow"), -1.0);
    result.payload.insert(QStringLiteral("bestCol"), -1.0);
    result.payload.insert(QStringLiteral("templateContourCount"), 0);
    result.payload.insert(QStringLiteral("templateContourPointCount"), 0);
    result.payload.insert(QStringLiteral("templateContourTotalLength"), 0.0);
    result.payload.insert(QStringLiteral("templateContourExtractionMode"), QString());
    result.payload.insert(QStringLiteral("templateContourFallback"), QStringLiteral("none"));
    result.payload.insert(QStringLiteral("thresholdModeUsed"),
                          manualThreshold ? QStringLiteral("manual") : QStringLiteral("auto"));
    result.payload.insert(QStringLiteral("thresholdModeApplied"), true);
    result.payload.insert(QStringLiteral("grayThresholdUsed"), config.grayThreshold);
    result.payload.insert(QStringLiteral("grayThresholdApplied"), manualThreshold);
    result.payload.insert(QStringLiteral("thresholdTypeUsed"), thresholdSettings.type);
    result.payload.insert(QStringLiteral("thresholdTypeFallback"), thresholdSettings.fallback);
    result.payload.insert(QStringLiteral("chainModeUsed"),
                          isManualMode(config.chainMode) ? QStringLiteral("manual") : QStringLiteral("auto"));
    result.payload.insert(QStringLiteral("chainModeApplied"), true);
    result.payload.insert(QStringLiteral("minChainLengthUsed"), qMax(1, config.minChainLength));
    result.payload.insert(QStringLiteral("minChainLengthApplied"), true);
    result.payload.insert(QStringLiteral("scaleModeUsed"), config.scaleMode);
    result.payload.insert(QStringLiteral("scaleModeApplied"),
                          scaleSettings.autoModeExpandedDefault
                          ? QStringLiteral("auto_default_range_expanded")
                          : QStringLiteral("partially_applied"));
    result.payload.insert(QStringLiteral("scaleModeReason"),
                          QStringLiteral("auto mode expands default 100/100 to 0.9-1.1; explicit scaleMin/scaleMax still win"));
    result.payload.insert(QStringLiteral("speedScaleUsed"), config.speedScale);
    result.payload.insert(QStringLiteral("speedScaleApplied"), true);
    result.payload.insert(QStringLiteral("speedScaleMapping"),
                          QStringLiteral("mapped to find_scaled_shape_model greediness"));
    result.payload.insert(QStringLiteral("featureScaleUsed"), config.featureScale);
    result.payload.insert(QStringLiteral("featureScaleApplied"), !manualThreshold);
    result.payload.insert(QStringLiteral("featureScaleMapping"),
                          QStringLiteral("mapped to edges_sub_pix sigma in auto threshold mode"));
    result.payload.insert(QStringLiteral("sigmaUsed"), sigma);
    result.payload.insert(QStringLiteral("scaleMinUsed"), scaleSettings.minScale);
    result.payload.insert(QStringLiteral("scaleMaxUsed"), scaleSettings.maxScale);
    result.payload.insert(QStringLiteral("scaleRangeApplied"), true);
    result.payload.insert(QStringLiteral("scaleRangeFallback"), scaleSettings.fallback);
    result.payload.insert(QStringLiteral("scaleModeAutoExpandedDefaultRange"),
                          scaleSettings.autoModeExpandedDefault);
    result.payload.insert(QStringLiteral("scaleRangeFallbackReason"), scaleSettings.fallbackReason);
    result.payload.insert(QStringLiteral("angleStartDegUsed"), config.angleStart);
    result.payload.insert(QStringLiteral("angleExtentDegUsed"), config.angleExtent);
    result.payload.insert(QStringLiteral("searchMinScoreUsed"), normalizedScoreValue(config.minScore));
    result.payload.insert(QStringLiteral("judgeScoreThresholdUsed"), normalizedScoreValue(config.scoreThreshold));
    result.payload.insert(QStringLiteral("metricUsed"), metric);
    result.payload.insert(QStringLiteral("polarityFallback"), polarityFallback);
    result.payload.insert(QStringLiteral("numLevelsUsed"), 4);
    result.payload.insert(QStringLiteral("findNumLevelsUsed"), 0);
    result.payload.insert(QStringLiteral("greedinessUsed"), greediness);
    result.payload.insert(QStringLiteral("maxOverlapUsed"), 0.5);
    result.payload.insert(QStringLiteral("minContrastUsed"), 10);
    result.payload.insert(QStringLiteral("timeoutMsUsed"), config.timeoutMs);
    result.payload.insert(QStringLiteral("timeoutApplied"), false);
    result.payload.insert(QStringLiteral("coordinateMode"), QStringLiteral("detect_crop_plus_roi_offset"));
    result.payload.insert(QStringLiteral("modelContourOverlayApplied"), false);
    result.payload.insert(QStringLiteral("modelContourOverlayRequested"), config.showContourPoints);
    result.payload.insert(QStringLiteral("templateMaskApplied"), false);
    result.payload.insert(QStringLiteral("detectMaskApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("sortModeRequested"), config.sortMode);
    result.payload.insert(QStringLiteral("sortModeApplied"), false);
    result.payload.insert(QStringLiteral("sortModeReason"),
                          QStringLiteral("HALCON return order is kept; best match is selected by score"));
    result.payload.insert(QStringLiteral("okNgReason"), QString());
    result.payload.insert(QStringLiteral("judgeBasis"), config.judgeBasis);
    result.payload.insert(QStringLiteral("existOk"), config.existOk);
    result.payload.insert(QStringLiteral("minScore"), config.minScore);
    result.payload.insert(QStringLiteral("scoreThreshold"), config.scoreThreshold);
    result.payload.insert(QStringLiteral("showContourPoints"), config.showContourPoints);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("templateMaskReason"), QStringLiteral("UI entry exists but config mask data is not wired to runner"));
    result.payload.insert(QStringLiteral("detectMaskReason"), QStringLiteral("UI entry exists but config mask data is not wired to runner"));
}

ContourPresenceHalconResult makeParameterError(const QString &status,
                                               const QString &error,
                                               const cv::Mat &image,
                                               const cv::Mat &referenceImage,
                                               const ContourPresenceHalconConfig &config)
{
    ContourPresenceHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = error;
    result.text = QStringLiteral("error");
    result.payload.insert(QStringLiteral("error"), error);
    fillPayload(result, image, referenceImage, config);
    result.payload.insert(QStringLiteral("okNgReason"), error);
    return result;
}

} // namespace

ContourPresenceHalconResult ContourPresenceHalconRunner::run(
        const cv::Mat &image,
        const cv::Mat &referenceImage,
        const ContourPresenceHalconConfig &config)
{
    QElapsedTimer timer;
    timer.start();

    if (image.empty()) {
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("image_empty"),
                                                                QStringLiteral("input image is empty"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (referenceImage.empty()) {
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("reference_image_empty"),
                                                                QStringLiteral("reference image is empty"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (image.depth() != CV_8U || referenceImage.depth() != CV_8U ||
        (image.channels() != 1 && image.channels() != 3 && image.channels() != 4) ||
        (referenceImage.channels() != 1 && referenceImage.channels() != 3 && referenceImage.channels() != 4)) {
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_image_type"),
                                                                QStringLiteral("ContourPresence supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    const QString templateSource = config.templateSource.trimmed();
    if (!templateSource.isEmpty() &&
        templateSource.compare(QStringLiteral("referenceImage"), Qt::CaseInsensitive) != 0) {
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_template_source"),
                                                                QStringLiteral("template source is not supported"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (!isValidNormalizedRoi(config.templateRoiNormalized)) {
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_template_roi"),
                                                                QStringLiteral("template ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (!isValidNormalizedRoi(config.roiNormalized)) {
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                                QStringLiteral("detect ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (!isRectType(config.templateShapeType) && !isPolygonType(config.templateShapeType)) {
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_template_roi"),
                                                                QStringLiteral("template ROI shape is not supported"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (isPolygonType(config.templateShapeType) && config.templatePolygonNormalized.size() < 3) {
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_template_polygon"),
                                                                QStringLiteral("polygon template ROI requires at least 3 points"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    const QRect templateRoiPixels = normalizedRoiToPixels(config.templateRoiNormalized,
                                                          referenceImage.cols,
                                                          referenceImage.rows);
    const QRect detectRoiPixels = normalizedRoiToPixels(config.roiNormalized,
                                                        image.cols,
                                                        image.rows);
    if (templateRoiPixels.isEmpty()) {
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_template_roi"),
                                                                QStringLiteral("template ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (detectRoiPixels.isEmpty()) {
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                                QStringLiteral("detect ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (config.halconSoPath.trimmed().isEmpty() || !QFileInfo::exists(config.halconSoPath)) {
        const QString triedPaths = config.halconSoPathCandidates.isEmpty()
                ? config.halconSoPath
                : config.halconSoPathCandidates.join(QStringLiteral("; "));
        ContourPresenceHalconResult result = makeParameterError(QStringLiteral("halcon_so_not_found"),
                                                                QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                                                                .arg(config.halconSoPath, triedPaths),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    ContourPresenceHalconResult result;
    fillPayload(result, image, referenceImage, config);

    const bool polygonTemplate = isPolygonType(config.templateShapeType);
    const bool polygonDetectRequested = isPolygonType(config.detectRegionType);
    const bool unsupportedDetectRequested = isUnsupportedDetectType(config.detectRegionType);
    if (!isRectType(config.detectRegionType) && !polygonDetectRequested && !unsupportedDetectRequested) {
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("unsupported_detect_roi");
        result.message = QStringLiteral("detect ROI shape is not supported");
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.payload.insert(QStringLiteral("okNgReason"), result.message);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
    if (polygonDetectRequested && config.detectPolygonNormalized.size() < 3) {
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("invalid_detect_polygon");
        result.message = QStringLiteral("polygon detect ROI requires at least 3 points");
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.payload.insert(QStringLiteral("okNgReason"), result.message);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    const QVector<QPointF> templatePolygonPixels = polygonTemplate
            ? normalizedPolygonToPixels(config.templatePolygonNormalized,
                                        referenceImage.cols,
                                        referenceImage.rows)
            : QVector<QPointF>();
    const QVector<QPointF> detectPolygonPixels = polygonDetectRequested
            ? normalizedPolygonToPixels(config.detectPolygonNormalized,
                                        image.cols,
                                        image.rows)
            : QVector<QPointF>();
    result.payload.insert(QStringLiteral("templateRoiPixels"), rectToJson(QRectF(templateRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoiPixels"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("templatePolygonPixels"), pointsToJson(templatePolygonPixels));
    result.payload.insert(QStringLiteral("detectPolygonPixels"), pointsToJson(detectPolygonPixels));
    result.payload.insert(QStringLiteral("unsupportedDetectRegionType"), unsupportedDetectRequested);
    result.payload.insert(QStringLiteral("detectRegionFallback"),
                          unsupportedDetectRequested
                          ? QStringLiteral("unsupported_region_type_bounding_rect")
                          : QStringLiteral("none"));

    if (polygonDetectRequested)
        result.overlays.append(polygonOverlay(detectPolygonPixels, QStringLiteral("detect_roi")));
    else
        result.overlays.append(rectOverlay(QRectF(detectRoiPixels), QStringLiteral("detect_roi")));
    if (config.referenceTest && image.cols == referenceImage.cols && image.rows == referenceImage.rows) {
        if (polygonTemplate)
            result.overlays.append(polygonOverlay(templatePolygonPixels, QStringLiteral("template_roi")));
        else
            result.overlays.append(rectOverlay(QRectF(templateRoiPixels), QStringLiteral("template_roi")));
    }

    cv::Mat templateMat = referenceImage(cv::Rect(templateRoiPixels.x(),
                                                  templateRoiPixels.y(),
                                                  templateRoiPixels.width(),
                                                  templateRoiPixels.height())).clone();
    cv::Mat detectMat = image(cv::Rect(detectRoiPixels.x(),
                                       detectRoiPixels.y(),
                                       detectRoiPixels.width(),
                                       detectRoiPixels.height())).clone();
    if (!templateMat.isContinuous())
        templateMat = templateMat.clone();
    if (!detectMat.isContinuous())
        detectMat = detectMat.clone();

    QString loadMessage;
    bool symbolMissing = false;
    QSharedPointer<PatternPresenceHalconLibrary> library =
            sharedPatternPresenceHalconLibrary(config.halconSoPath, loadMessage, symbolMissing);
    if (!library) {
        result.success = false;
        result.ok = false;
        result.status = symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                      : QStringLiteral("halcon_load_failed");
        result.message = loadMessage;
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), loadMessage);
        result.payload.insert(QStringLiteral("okNgReason"), loadMessage);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    PatternPresenceHalconApi *api = &library->api;
    if (!api->hasXldShapeModelOperators() || !api->hasContourExtractionOperators()) {
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("halcon_symbol_missing");
        result.message = QStringLiteral("required HALCON XLD contour template matching symbols are unavailable");
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.payload.insert(QStringLiteral("okNgReason"), result.message);
        result.payload.insert(QStringLiteral("hasXldShapeModelOperators"), api->hasXldShapeModelOperators());
        result.payload.insert(QStringLiteral("hasContourExtractionOperators"), api->hasContourExtractionOperators());
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    GrayHalconImage templateImage;
    GrayHalconImage detectImage;
    Hobject templatePolygonRegion = NO_OBJECTS;
    Hobject templateReducedImage = NO_OBJECTS;
    Hobject templateSelectedContours = NO_OBJECTS;
    Hobject modelContours = NO_OBJECTS;
    Hobject detectPolygonRegion = NO_OBJECTS;
    Hobject detectReducedImage = NO_OBJECTS;
    Hobject transformedModelContours = NO_OBJECTS;
    Htuple modelIdTuple = HTUPLE_INITIALIZER;
    Htuple templatePolygonRowsTuple = HTUPLE_INITIALIZER;
    Htuple templatePolygonColumnsTuple = HTUPLE_INITIALIZER;
    Htuple detectPolygonRowsTuple = HTUPLE_INITIALIZER;
    Htuple detectPolygonColumnsTuple = HTUPLE_INITIALIZER;
    Htuple createNumLevelsTuple = HTUPLE_INITIALIZER;
    Htuple angleStartTuple = HTUPLE_INITIALIZER;
    Htuple angleExtentTuple = HTUPLE_INITIALIZER;
    Htuple angleStepTuple = HTUPLE_INITIALIZER;
    Htuple scaleMinTuple = HTUPLE_INITIALIZER;
    Htuple scaleMaxTuple = HTUPLE_INITIALIZER;
    Htuple scaleStepTuple = HTUPLE_INITIALIZER;
    Htuple optimizationTuple = HTUPLE_INITIALIZER;
    Htuple metricTuple = HTUPLE_INITIALIZER;
    Htuple minContrastTuple = HTUPLE_INITIALIZER;
    Htuple timeoutParamNameTuple = HTUPLE_INITIALIZER;
    Htuple timeoutValueTuple = HTUPLE_INITIALIZER;
    Htuple contourLevelTuple = HTUPLE_INITIALIZER;
    Htuple minScoreTuple = HTUPLE_INITIALIZER;
    Htuple numMatchesTuple = HTUPLE_INITIALIZER;
    Htuple maxOverlapTuple = HTUPLE_INITIALIZER;
    Htuple subPixelTuple = HTUPLE_INITIALIZER;
    Htuple findNumLevelsTuple = HTUPLE_INITIALIZER;
    Htuple greedinessTuple = HTUPLE_INITIALIZER;
    Htuple rowTuple = HTUPLE_INITIALIZER;
    Htuple columnTuple = HTUPLE_INITIALIZER;
    Htuple angleTuple = HTUPLE_INITIALIZER;
    Htuple scaleTuple = HTUPLE_INITIALIZER;
    Htuple scoreTuple = HTUPLE_INITIALIZER;
    Htuple displayRow1Tuple = HTUPLE_INITIALIZER;
    Htuple displayColumn1Tuple = HTUPLE_INITIALIZER;
    Htuple displayAngle1Tuple = HTUPLE_INITIALIZER;
    Htuple displayRow2Tuple = HTUPLE_INITIALIZER;
    Htuple displayColumn2Tuple = HTUPLE_INITIALIZER;
    Htuple displayAngle2Tuple = HTUPLE_INITIALIZER;
    Htuple displayHomMatTuple = HTUPLE_INITIALIZER;
    Htuple displayScaleXTuple = HTUPLE_INITIALIZER;
    Htuple displayScaleYTuple = HTUPLE_INITIALIZER;
    Htuple displayScaledHomMatTuple = HTUPLE_INITIALIZER;
    QVector<Htuple *> createdTuples;
    bool modelCreated = false;

    auto makeHalconFailure = [&](const Herror status, const QString &stage) {
        HalconFailure failure;
        failure.stage = stage;
        failure.code = status;
        failure.halconMessage = library->errorText(status);
        failure.status = stage.contains(QStringLiteral("create_scaled_shape_model_xld"))
                ? QStringLiteral("template contour model creation failed")
                : QStringLiteral("ContourPresence HALCON error");
        failure.message = QStringLiteral("%1: %2").arg(stage, failure.halconMessage);
        if (stage.contains(QStringLiteral("template_contour"))) {
            failure.status = QStringLiteral("template contour extraction failed");
            failure.message = failure.halconMessage;
        }
        return failure;
    };

    auto checkStatus = [&](const Herror status, const QString &stage) {
        if (!patternPresenceHalconStatusOk(status))
            throw makeHalconFailure(status, stage);
    };

    auto trackTuple = [&](Htuple &tuple) {
        if (!createdTuples.contains(&tuple))
            createdTuples.append(&tuple);
    };

    auto createIntTuple = [&](Htuple &tuple, const Hlong value) {
        api->createTupleInt(&tuple, value);
        trackTuple(tuple);
    };

    auto createDoubleTuple = [&](Htuple &tuple, const double value) {
        api->createTupleDouble(&tuple, value);
        trackTuple(tuple);
    };

    auto createStringTuple = [&](Htuple &tuple, const QByteArray &value) {
        api->createTupleString(&tuple, value.constData());
        trackTuple(tuple);
    };

    auto createDoubleArrayTuple = [&](Htuple &tuple, const QVector<double> &values) {
        api->createTuple(&tuple, static_cast<Hlong>(values.size()));
        trackTuple(tuple);
        for (int index = 0; index < values.size(); ++index)
            api->setDouble(&tuple, values.at(index), static_cast<Hlong>(index));
    };

    auto generateGrayImage = [&](const cv::Mat &mat, GrayHalconImage &halconImage, const QString &stage) {
        if (mat.channels() == 1) {
            checkStatus(api->genImage1(&halconImage.inputImage,
                                       "byte",
                                       mat.cols,
                                       mat.rows,
                                       reinterpret_cast<Hlong>(mat.data)),
                        stage + QStringLiteral(".gen_image1"));
            halconImage.graySource = halconImage.inputImage;
            return;
        }

        const char *colorFormat = mat.channels() == 4 ? "bgrx" : "bgr";
        checkStatus(api->genImageInterleaved(&halconImage.inputImage,
                                             reinterpret_cast<Hlong>(mat.data),
                                             colorFormat,
                                             mat.cols,
                                             mat.rows,
                                             0,
                                             "byte",
                                             0,
                                             0,
                                             0,
                                             0,
                                             8,
                                             0),
                    stage + QStringLiteral(".gen_image_interleaved"));
        checkStatus(api->rgb1ToGray(halconImage.inputImage, &halconImage.grayImage),
                    stage + QStringLiteral(".rgb1_to_gray"));
        halconImage.graySource = halconImage.grayImage;
    };

    auto cleanup = [&]() {
        if (modelCreated && api->clearShapeModel)
            api->clearShapeModel(modelIdTuple);
        modelCreated = false;
        destroyTuple(api, modelIdTuple);
        for (Htuple *tuple : createdTuples) {
            if (tuple)
                destroyTuple(api, *tuple);
        }
        createdTuples.clear();
        clearObject(api, transformedModelContours);
        clearObject(api, detectReducedImage);
        clearObject(api, detectPolygonRegion);
        clearObject(api, modelContours);
        clearObject(api, templateSelectedContours);
        clearObject(api, templateReducedImage);
        clearObject(api, templatePolygonRegion);
        clearObject(api, detectImage.grayImage);
        clearObject(api, detectImage.inputImage);
        clearObject(api, templateImage.grayImage);
        clearObject(api, templateImage.inputImage);
    };

    auto makeThresholdCandidate = [&](const Hobject source,
                                      const QString &type,
                                      const int minLength,
                                      const int grayThreshold) {
        ContourCandidate candidate;
        candidate.mode = QStringLiteral("manual_threshold_%1").arg(type);
        Hobject region = NO_OBJECTS;
        Hobject connected = NO_OBJECTS;
        Hobject contours = NO_OBJECTS;
        const double low = type == QStringLiteral("dark") ? 0.0 : static_cast<double>(grayThreshold);
        const double high = type == QStringLiteral("dark") ? static_cast<double>(grayThreshold) : 255.0;
        checkStatus(api->threshold(source, &region, low, high),
                    QStringLiteral("threshold.template_contour_%1").arg(type));
        checkStatus(api->connection(region, &connected),
                    QStringLiteral("connection.template_contour_%1").arg(type));
        checkStatus(api->genContourRegionXld(connected, &contours, "border"),
                    QStringLiteral("gen_contour_region_xld.template_contour_%1").arg(type));
        checkStatus(api->selectContoursXld(contours,
                                           &candidate.contours,
                                           "contour_length",
                                           static_cast<double>(minLength),
                                           kSelectContourMaxLength,
                                           -0.5,
                                           0.5),
                    QStringLiteral("select_contours_xld.template_contour_%1").arg(type));
        candidate.stats = readXldContours(api, candidate.contours);
        clearObject(api, contours);
        clearObject(api, connected);
        clearObject(api, region);
        return candidate;
    };

    auto makeAutoEdgesCandidate = [&](const Hobject source,
                                      const int minLength,
                                      const double sigma) {
        ContourCandidate candidate;
        candidate.mode = QStringLiteral("auto_edges_sub_pix");
        Hobject edges = NO_OBJECTS;
        Hobject segments = NO_OBJECTS;
        const Hlong low = 10;
        const Hlong high = 30;
        checkStatus(api->edgesSubPix(source,
                                     &edges,
                                     "canny",
                                     sigma,
                                     low,
                                     high),
                    QStringLiteral("edges_sub_pix.template_contour"));
        Herror segmentStatus = api->segmentContoursXld(edges,
                                                       &segments,
                                                       "lines_circles",
                                                       5,
                                                       4.0,
                                                       2.0);
        if (!patternPresenceHalconStatusOk(segmentStatus)) {
            clearObject(api, segments);
            checkStatus(api->segmentContoursXld(edges,
                                                &segments,
                                                "lines",
                                                5,
                                                4.0,
                                                2.0),
                        QStringLiteral("segment_contours_xld.template_contour"));
            candidate.fallback = QStringLiteral("segment_lines");
        }
        checkStatus(api->selectContoursXld(segments,
                                           &candidate.contours,
                                           "contour_length",
                                           static_cast<double>(minLength),
                                           kSelectContourMaxLength,
                                           -0.5,
                                           0.5),
                    QStringLiteral("select_contours_xld.template_contour"));
        candidate.stats = readXldContours(api, candidate.contours);
        clearObject(api, segments);
        clearObject(api, edges);
        return candidate;
    };

    auto candidateBetter = [](const ContourCandidate &left, const ContourCandidate &right) {
        if (left.stats.pointCount != right.stats.pointCount)
            return left.stats.pointCount > right.stats.pointCount;
        if (std::abs(left.stats.totalLength - right.stats.totalLength) > 0.001)
            return left.stats.totalLength > right.stats.totalLength;
        return left.stats.objectCount > right.stats.objectCount;
    };

    try {
        const ScaleRangeSettings scaleSettings = scaleRangeSettings(config);
        const ThresholdTypeSettings thresholdSettings = thresholdTypeSettings(config.thresholdType);
        const bool manualThreshold = isManualMode(config.thresholdMode);
        const double sigma = sigmaFromFeatureScale(config.featureScale);
        const double greediness = greedinessFromSpeedScale(config.speedScale);
        bool polarityFallback = false;
        const QString metric = metricForPolarity(config.polarity, &polarityFallback);
        const int minLength = minChainLengthUsed(config, templateRoiPixels);
        const double angleStartRad = config.angleStart * kPi / 180.0;
        const double angleExtentRad = qBound(0.0, config.angleExtent * kPi / 180.0, 2.0 * kPi);
        const double searchMinScore = normalizedScoreValue(config.minScore);
        const double judgeScoreThreshold = normalizedScoreValue(config.scoreThreshold);
        const int numMatches = 1;

        result.payload.insert(QStringLiteral("thresholdTypeUsed"), thresholdSettings.type);
        result.payload.insert(QStringLiteral("thresholdTypeFallback"), thresholdSettings.fallback);
        result.payload.insert(QStringLiteral("minChainLengthUsed"), minLength);
        result.payload.insert(QStringLiteral("sigmaUsed"), sigma);
        result.payload.insert(QStringLiteral("greedinessUsed"), greediness);
        result.payload.insert(QStringLiteral("scaleMinUsed"), scaleSettings.minScale);
        result.payload.insert(QStringLiteral("scaleMaxUsed"), scaleSettings.maxScale);
        result.payload.insert(QStringLiteral("scaleRangeFallback"), scaleSettings.fallback);
        result.payload.insert(QStringLiteral("scaleModeAutoExpandedDefaultRange"),
                              scaleSettings.autoModeExpandedDefault);
        result.payload.insert(QStringLiteral("scaleRangeFallbackReason"), scaleSettings.fallbackReason);
        result.payload.insert(QStringLiteral("metricUsed"), metric);
        result.payload.insert(QStringLiteral("polarityFallback"), polarityFallback);

        createIntTuple(createNumLevelsTuple, 4);
        createDoubleTuple(angleStartTuple, angleStartRad);
        createDoubleTuple(angleExtentTuple, angleExtentRad);
        createStringTuple(angleStepTuple, QByteArray("auto"));
        createDoubleTuple(scaleMinTuple, scaleSettings.minScale);
        createDoubleTuple(scaleMaxTuple, scaleSettings.maxScale);
        createStringTuple(scaleStepTuple, QByteArray("auto"));
        createStringTuple(optimizationTuple, QByteArray("auto"));
        createStringTuple(metricTuple, metric.toLatin1());
        createIntTuple(minContrastTuple, 10);
        createStringTuple(timeoutParamNameTuple, QByteArray("timeout"));
        createIntTuple(timeoutValueTuple, static_cast<Hlong>(qMax(0, config.timeoutMs)));
        createIntTuple(contourLevelTuple, 1);
        createDoubleTuple(minScoreTuple, searchMinScore);
        createIntTuple(numMatchesTuple, static_cast<Hlong>(numMatches));
        createDoubleTuple(maxOverlapTuple, 0.5);
        createStringTuple(subPixelTuple, QByteArray("least_squares"));
        createIntTuple(findNumLevelsTuple, 0);
        createDoubleTuple(greedinessTuple, greediness);

        generateGrayImage(templateMat, templateImage, QStringLiteral("template"));
        Hobject templateContourSource = templateImage.graySource;

        const QVector<QPointF> templatePolygonLocalPixels = polygonTemplate
                ? polygonToLocalClamped(templatePolygonPixels,
                                        templateRoiPixels,
                                        templateMat.cols,
                                        templateMat.rows)
                : QVector<QPointF>();
        result.payload.insert(QStringLiteral("templatePolygonLocalPoints"),
                              pointsToJson(templatePolygonLocalPixels));
        result.payload.insert(QStringLiteral("templatePolygonAreaPixels"),
                              polygonTemplate ? polygonAreaPixels(templatePolygonLocalPixels) : 0.0);
        if (polygonTemplate) {
            QVector<double> rows;
            QVector<double> columns;
            rows.reserve(templatePolygonLocalPixels.size());
            columns.reserve(templatePolygonLocalPixels.size());
            for (const QPointF &point : templatePolygonLocalPixels) {
                rows.append(point.y());
                columns.append(point.x());
            }
            createDoubleArrayTuple(templatePolygonRowsTuple, rows);
            createDoubleArrayTuple(templatePolygonColumnsTuple, columns);
            checkStatus(api->genRegionPolygon(&templatePolygonRegion,
                                              templatePolygonRowsTuple,
                                              templatePolygonColumnsTuple),
                        QStringLiteral("gen_region_polygon.template_roi"));
            checkStatus(api->reduceDomain(templateImage.graySource,
                                          templatePolygonRegion,
                                          &templateReducedImage),
                        QStringLiteral("reduce_domain.template_roi"));
            templateContourSource = templateReducedImage;
            result.payload.insert(QStringLiteral("templatePolygonApplied"), true);
        } else {
            result.payload.insert(QStringLiteral("templatePolygonApplied"), false);
        }

        ContourCandidate selectedCandidate;
        if (manualThreshold) {
            QVector<ContourCandidate> candidates;
            if (thresholdSettings.type == QStringLiteral("light") || thresholdSettings.type == QStringLiteral("auto"))
                candidates.append(makeThresholdCandidate(templateContourSource,
                                                        QStringLiteral("light"),
                                                        minLength,
                                                        config.grayThreshold));
            if (thresholdSettings.type == QStringLiteral("dark") || thresholdSettings.type == QStringLiteral("auto"))
                candidates.append(makeThresholdCandidate(templateContourSource,
                                                        QStringLiteral("dark"),
                                                        minLength,
                                                        config.grayThreshold));
            if (candidates.isEmpty())
                candidates.append(makeThresholdCandidate(templateContourSource,
                                                        QStringLiteral("light"),
                                                        minLength,
                                                        config.grayThreshold));
            int bestIndex = 0;
            for (int index = 1; index < candidates.size(); ++index) {
                if (candidateBetter(candidates.at(index), candidates.at(bestIndex)))
                    bestIndex = index;
            }
            selectedCandidate = candidates.at(bestIndex);
            for (int index = 0; index < candidates.size(); ++index) {
                if (index != bestIndex) {
                    Hobject object = candidates[index].contours;
                    clearObject(api, object);
                }
            }
        } else {
            selectedCandidate = makeAutoEdgesCandidate(templateContourSource, minLength, sigma);
        }
        templateSelectedContours = selectedCandidate.contours;
        selectedCandidate.contours = NO_OBJECTS;

        result.payload.insert(QStringLiteral("templateContourExtractionMode"), selectedCandidate.mode);
        result.payload.insert(QStringLiteral("templateContourFallback"),
                              selectedCandidate.fallback.isEmpty() ? QStringLiteral("none") : selectedCandidate.fallback);
        result.payload.insert(QStringLiteral("templateContourCount"), selectedCandidate.stats.objectCount);
        result.payload.insert(QStringLiteral("templateContourPointCount"), selectedCandidate.stats.pointCount);
        result.payload.insert(QStringLiteral("templateContourTotalLength"), selectedCandidate.stats.totalLength);
        result.payload.insert(QStringLiteral("templateContourLengths"), doublesToJson(selectedCandidate.stats.lengths));
        if (!selectedCandidate.stats.error.isEmpty())
            result.payload.insert(QStringLiteral("templateContourReadError"), selectedCandidate.stats.error);

        if (selectedCandidate.stats.objectCount <= 0 ||
            selectedCandidate.stats.pointCount < kMinTemplateContourPointCount ||
            selectedCandidate.stats.totalLength < static_cast<double>(minLength)) {
            result.success = false;
            result.ok = false;
            result.status = QStringLiteral("template_contour_extraction_failed");
            result.message = QStringLiteral("template contour extraction failed");
            result.text = QStringLiteral("error");
            result.payload.insert(QStringLiteral("okNgReason"),
                                  QStringLiteral("template contour extraction failed"));
            result.elapsedMs = timer.elapsed();
            result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
            cleanup();
            return result;
        }

        checkStatus(api->createScaledShapeModelXld(templateSelectedContours,
                                                   createNumLevelsTuple,
                                                   angleStartTuple,
                                                   angleExtentTuple,
                                                   angleStepTuple,
                                                   scaleMinTuple,
                                                   scaleMaxTuple,
                                                   scaleStepTuple,
                                                   optimizationTuple,
                                                   metricTuple,
                                                   minContrastTuple,
                                                   &modelIdTuple),
                    QStringLiteral("create_scaled_shape_model_xld"));
        modelCreated = true;
        if (config.timeoutMs > 0 && api->setShapeModelParam) {
            checkStatus(api->setShapeModelParam(modelIdTuple,
                                                timeoutParamNameTuple,
                                                timeoutValueTuple),
                        QStringLiteral("set_shape_model_param.timeout"));
            result.payload.insert(QStringLiteral("timeoutApplied"), QStringLiteral("model_param"));
        }
        checkStatus(api->getShapeModelContours(&modelContours, modelIdTuple, contourLevelTuple),
                    QStringLiteral("get_shape_model_contours"));
        const XldReadResult modelContourRead = readXldContours(api, modelContours);
        result.payload.insert(QStringLiteral("modelContourCount"), modelContourRead.objectCount);
        result.payload.insert(QStringLiteral("modelContourPointCount"), modelContourRead.pointCount);

        generateGrayImage(detectMat, detectImage, QStringLiteral("detect"));
        Hobject detectSearchSource = detectImage.graySource;
        if (polygonDetectRequested) {
            QVector<QPointF> detectPolygonLocalPixels =
                    polygonToLocalClamped(detectPolygonPixels,
                                          detectRoiPixels,
                                          detectMat.cols,
                                          detectMat.rows);
            QVector<double> rows;
            QVector<double> columns;
            rows.reserve(detectPolygonLocalPixels.size());
            columns.reserve(detectPolygonLocalPixels.size());
            for (const QPointF &point : detectPolygonLocalPixels) {
                rows.append(point.y());
                columns.append(point.x());
            }
            createDoubleArrayTuple(detectPolygonRowsTuple, rows);
            createDoubleArrayTuple(detectPolygonColumnsTuple, columns);
            checkStatus(api->genRegionPolygon(&detectPolygonRegion,
                                              detectPolygonRowsTuple,
                                              detectPolygonColumnsTuple),
                        QStringLiteral("gen_region_polygon.detect_roi"));
            checkStatus(api->reduceDomain(detectImage.graySource,
                                          detectPolygonRegion,
                                          &detectReducedImage),
                        QStringLiteral("reduce_domain.detect_roi"));
            detectSearchSource = detectReducedImage;
            result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), true);
            result.payload.insert(QStringLiteral("detectPolygonLocalPixels"),
                                  pointsToJson(detectPolygonLocalPixels));
        } else {
            result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), false);
        }

        checkStatus(api->findScaledShapeModel(detectSearchSource,
                                              modelIdTuple,
                                              angleStartTuple,
                                              angleExtentTuple,
                                              scaleMinTuple,
                                              scaleMaxTuple,
                                              minScoreTuple,
                                              numMatchesTuple,
                                              maxOverlapTuple,
                                              subPixelTuple,
                                              findNumLevelsTuple,
                                              greedinessTuple,
                                              &rowTuple,
                                              &columnTuple,
                                              &angleTuple,
                                              &scaleTuple,
                                              &scoreTuple),
                    QStringLiteral("find_scaled_shape_model"));

        const int matchCount = qMin<int>(qMin<int>(rowTuple.num, columnTuple.num),
                                         qMin<int>(qMin<int>(angleTuple.num, scaleTuple.num),
                                                   scoreTuple.num));
        const bool found = matchCount > 0;
        QJsonArray matchesArray;
        QVector<double> matchScores;
        double bestScore = 0.0;
        double bestRow = -1.0;
        double bestColumn = -1.0;
        double bestRawRow = -1.0;
        double bestRawColumn = -1.0;
        double bestAngle = 0.0;
        double bestScale = 1.0;

        for (int index = 0; index < matchCount; ++index) {
            const double localRow = api->getDouble(&rowTuple, index);
            const double localColumn = api->getDouble(&columnTuple, index);
            const double angle = api->getDouble(&angleTuple, index);
            const double scale = api->getDouble(&scaleTuple, index);
            const double score = api->getDouble(&scoreTuple, index);
            const double imageRow = localRow + static_cast<double>(detectRoiPixels.y());
            const double imageColumn = localColumn + static_cast<double>(detectRoiPixels.x());

            QJsonObject matchJson;
            matchJson.insert(QStringLiteral("row"), imageRow);
            matchJson.insert(QStringLiteral("column"), imageColumn);
            matchJson.insert(QStringLiteral("localRow"), localRow);
            matchJson.insert(QStringLiteral("localColumn"), localColumn);
            matchJson.insert(QStringLiteral("angle"), angle);
            matchJson.insert(QStringLiteral("angleDeg"), angle * 180.0 / kPi);
            matchJson.insert(QStringLiteral("scale"), scale);
            matchJson.insert(QStringLiteral("score"), score);
            matchesArray.append(matchJson);
            matchScores.append(score);

            if (index == 0 || score > bestScore) {
                bestScore = score;
                bestRow = imageRow;
                bestColumn = imageColumn;
                bestRawRow = localRow;
                bestRawColumn = localColumn;
                bestAngle = angle;
                bestScale = scale;
            }
        }

        const bool scoreJudge = isScoreJudgeBasis(config.judgeBasis);
        bool ok = false;
        QString okNgReason;
        if (scoreJudge) {
            result.payload.insert(QStringLiteral("judgeBasisCombination"),
                                  QStringLiteral("score_with_existOk"));
            if (!found) {
                ok = !config.existOk;
                result.payload.insert(QStringLiteral("noMatchAndExistOkFalse"), !config.existOk);
                okNgReason = config.existOk
                        ? QStringLiteral("no contour template match found")
                        : QStringLiteral("no contour template match found, existOk=false");
            } else if (!config.existOk) {
                ok = false;
                okNgReason = QStringLiteral("matched contour template while existOk=false");
            } else if (bestScore >= judgeScoreThreshold) {
                ok = true;
                okNgReason = QStringLiteral("matched contour template, score above threshold");
            } else {
                ok = false;
                okNgReason = QStringLiteral("matched contour template, score below threshold");
            }
        } else if (isPresenceJudgeBasis(config.judgeBasis)) {
            ok = config.existOk ? found : !found;
            if (found)
                okNgReason = config.existOk
                        ? QStringLiteral("matched contour template")
                        : QStringLiteral("matched contour template while existOk=false");
            else
                okNgReason = config.existOk
                        ? QStringLiteral("no contour template match found")
                        : QStringLiteral("no contour template match found, existOk=false");
        } else {
            ok = config.existOk ? found : !found;
            okNgReason = found ? QStringLiteral("matched contour template")
                               : QStringLiteral("no contour template match found");
        }

        result.success = true;
        result.ok = ok;
        result.score = bestScore;
        result.count = matchCount;
        result.text = found ? QStringLiteral("found") : QStringLiteral("missing");
        result.status = QStringLiteral("ContourPresence: %1 score=%2 count=%3")
                .arg(result.text,
                     QString::number(bestScore, 'f', 3),
                     QString::number(matchCount));
        result.message = okNgReason;

        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("matchCount"), matchCount);
        result.payload.insert(QStringLiteral("foundCount"), matchCount);
        result.payload.insert(QStringLiteral("bestScore"), bestScore);
        result.payload.insert(QStringLiteral("bestRow"), bestRow);
        result.payload.insert(QStringLiteral("bestCol"), bestColumn);
        result.payload.insert(QStringLiteral("bestColumn"), bestColumn);
        result.payload.insert(QStringLiteral("bestAngle"), bestAngle);
        result.payload.insert(QStringLiteral("bestAngleDeg"), bestAngle * 180.0 / kPi);
        result.payload.insert(QStringLiteral("bestScale"), bestScale);
        result.payload.insert(QStringLiteral("rawMatchRow"), bestRawRow);
        result.payload.insert(QStringLiteral("rawMatchCol"), bestRawColumn);
        result.payload.insert(QStringLiteral("globalMatchRow"), bestRow);
        result.payload.insert(QStringLiteral("globalMatchCol"), bestColumn);
        result.payload.insert(QStringLiteral("matches"), matchesArray);
        result.payload.insert(QStringLiteral("matchScores"), doublesToJson(matchScores));
        result.payload.insert(QStringLiteral("okNgReason"), okNgReason);

        if (found) {
            const QPointF center(bestColumn, bestRow);
            const double crossRadius = qBound(6.0,
                                              static_cast<double>(qMin(templateRoiPixels.width(),
                                                                       templateRoiPixels.height())) / 4.0,
                                              24.0);
            const QRectF matchBbox(bestColumn - static_cast<double>(templateRoiPixels.width()) * bestScale / 2.0,
                                   bestRow - static_cast<double>(templateRoiPixels.height()) * bestScale / 2.0,
                                   static_cast<double>(templateRoiPixels.width()) * bestScale,
                                   static_cast<double>(templateRoiPixels.height()) * bestScale);
            result.overlays.append(rectOverlay(matchBbox, QStringLiteral("match_bbox"), bestScore));
            result.overlays.append(lineOverlay(QPointF(center.x() - crossRadius, center.y()),
                                               QPointF(center.x() + crossRadius, center.y()),
                                               QStringLiteral("match_center"),
                                               bestScore));
            result.overlays.append(lineOverlay(QPointF(center.x(), center.y() - crossRadius),
                                               QPointF(center.x(), center.y() + crossRadius),
                                               QStringLiteral("match_center"),
                                               bestScore));
            result.overlays.append(textOverlay(QPointF(center.x() + crossRadius + 2.0,
                                                       center.y() + crossRadius + 2.0),
                                               QStringLiteral("score=%1 angle=%2 scale=%3")
                                               .arg(QString::number(bestScore, 'f', 3),
                                                    QString::number(bestAngle * 180.0 / kPi, 'f', 1),
                                                    QString::number(bestScale, 'f', 3)),
                                               QStringLiteral("match_score_text"),
                                               bestScore));
            result.payload.insert(QStringLiteral("matchBboxOverlayApplied"), true);
            result.payload.insert(QStringLiteral("matchRectX"), matchBbox.x());
            result.payload.insert(QStringLiteral("matchRectY"), matchBbox.y());
            result.payload.insert(QStringLiteral("matchRectW"), matchBbox.width());
            result.payload.insert(QStringLiteral("matchRectH"), matchBbox.height());
        }

        if (config.showContourPoints && found) {
            createDoubleTuple(displayRow1Tuple, 0.0);
            createDoubleTuple(displayColumn1Tuple, 0.0);
            createDoubleTuple(displayAngle1Tuple, 0.0);
            createDoubleTuple(displayRow2Tuple, bestRawRow);
            createDoubleTuple(displayColumn2Tuple, bestRawColumn);
            createDoubleTuple(displayAngle2Tuple, bestAngle);
            checkStatus(api->vectorAngleToRigid(displayRow1Tuple,
                                                displayColumn1Tuple,
                                                displayAngle1Tuple,
                                                displayRow2Tuple,
                                                displayColumn2Tuple,
                                                displayAngle2Tuple,
                                                &displayHomMatTuple),
                        QStringLiteral("vector_angle_to_rigid.model_contour"));
            const Htuple *homMatForContour = &displayHomMatTuple;
            bool contourScaleApplied = false;
            if (std::abs(bestScale - 1.0) > 0.0001 && api->homMat2dScaleLocal) {
                createDoubleTuple(displayScaleXTuple, bestScale);
                createDoubleTuple(displayScaleYTuple, bestScale);
                checkStatus(api->homMat2dScaleLocal(displayHomMatTuple,
                                                    displayScaleXTuple,
                                                    displayScaleYTuple,
                                                    &displayScaledHomMatTuple),
                            QStringLiteral("hom_mat2d_scale_local.model_contour"));
                homMatForContour = &displayScaledHomMatTuple;
                contourScaleApplied = true;
            }
            checkStatus(api->affineTransContourXld(modelContours,
                                                   &transformedModelContours,
                                                   *homMatForContour),
                        QStringLiteral("affine_trans_contour_xld.model_contour"));
            const XldReadResult transformedRead = readXldContours(api, transformedModelContours);
            QVector<QVector<QPointF>> globalContours =
                    translatedContours(transformedRead.contours,
                                       QPointF(static_cast<double>(detectRoiPixels.x()),
                                               static_cast<double>(detectRoiPixels.y())));
            int displayPointCount = 0;
            const int lineSegmentCount = appendContourLineOverlays(&result.overlays,
                                                                   globalContours,
                                                                   bestScore,
                                                                   &displayPointCount);
            const bool applied = lineSegmentCount > 0;
            result.payload.insert(QStringLiteral("modelContourOverlayApplied"), applied);
            result.payload.insert(QStringLiteral("modelContourScaleTransformApplied"), contourScaleApplied);
            result.payload.insert(QStringLiteral("contourOverlayLineSegmentCount"), lineSegmentCount);
            result.payload.insert(QStringLiteral("contourDisplayPointCount"), displayPointCount);
            result.payload.insert(QStringLiteral("displayContourObjectCount"), transformedRead.objectCount);
            result.payload.insert(QStringLiteral("displayContourPointCount"), transformedRead.pointCount);
            result.payload.insert(QStringLiteral("displayContourSource"),
                                  QStringLiteral("get_shape_model_contours_affine_trans_contour_xld"));
            if (!transformedRead.error.isEmpty())
                result.payload.insert(QStringLiteral("displayContourReadError"), transformedRead.error);
        } else {
            result.payload.insert(QStringLiteral("modelContourOverlayApplied"), false);
            result.payload.insert(QStringLiteral("displayContourSource"),
                                  config.showContourPoints ? QStringLiteral("no_match")
                                                           : QStringLiteral("showContourPoints_false"));
        }

        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("timeoutExceeded"),
                              config.timeoutMs > 0 && result.elapsedMs > config.timeoutMs);
        cleanup();
        return result;
    } catch (const HalconFailure &failure) {
        cleanup();
        result.success = false;
        result.ok = false;
        result.status = failure.status;
        result.message = failure.message;
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), failure.message);
        result.payload.insert(QStringLiteral("okNgReason"),
                              failure.message.isEmpty()
                              ? QStringLiteral("HALCON exception")
                              : failure.message);
        result.payload.insert(QStringLiteral("halconStage"), failure.stage);
        result.payload.insert(QStringLiteral("halconErrorCode"), static_cast<int>(failure.code));
        result.payload.insert(QStringLiteral("halconErrorMessage"), failure.halconMessage);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const std::exception &error) {
        cleanup();
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("ContourPresence HALCON error");
        result.message = QString::fromLocal8Bit(error.what());
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.payload.insert(QStringLiteral("okNgReason"),
                              QStringLiteral("HALCON exception: %1").arg(result.message));
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (...) {
        cleanup();
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("ContourPresence HALCON error");
        result.message = QStringLiteral("unknown ContourPresence HALCON error");
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.payload.insert(QStringLiteral("okNgReason"), QStringLiteral("HALCON exception: unknown"));
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
}
