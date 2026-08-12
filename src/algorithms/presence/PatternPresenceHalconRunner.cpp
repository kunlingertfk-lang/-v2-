#include "algorithms/presence/PatternPresenceHalconRunner.h"

#include "algorithms/presence/PatternPresenceAutoModelDomain.h"
#include "algorithms/presence/PatternPresenceHalconApi.h"
#include "algorithms/location/PositionCorrectionHalconTransform.h"
#include "toolcore/PositionCorrectionTransform.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <utility>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kMinRoiPixelSize = 2;
constexpr int kMaxContourOverlayLineSegments = 4096;
constexpr double kMinPolygonAreaPixels = 4.0;
constexpr double kMinReducedDomainAreaPixels = 8.0;

PositionCorrectionHalconRegionApi positionCorrectionRegionApi(
        const PatternPresenceHalconApi &api)
{
    PositionCorrectionHalconRegionApi transformApi;
    transformApi.createTuple = api.createTuple;
    transformApi.setDouble = api.setDouble;
    transformApi.setString = api.setString;
    transformApi.destroyTuple = api.destroyTuple;
    transformApi.getDouble = api.getDouble;
    transformApi.affineTransRegion = api.affineTransRegion;
    transformApi.clipRegion = api.clipRegion;
    transformApi.areaCenter = api.areaCenter;
    return transformApi;
}

double normalizedScore(const int percentScore)
{
    return qBound(0.0, static_cast<double>(percentScore) / 100.0, 1.0);
}

struct ShapeModelContrastSettings
{
    int sensitivity = 2;
    int contrast = 64;
    int minContrast = 32;
    QString version = QStringLiteral("pattern_shape_model_contrast_v3_noise_guard");
    QString mapping = QStringLiteral("contrast=70-s*3, minContrast=max(18,contrast/2)");
    QString source = QStringLiteral("templateSensitivity");
};

struct ScaleRangeSettings
{
    double minScale = 1.0;
    double maxScale = 1.0;
    bool fallback = false;
    QString fallbackReason;
};

ScaleRangeSettings scaleRangeSettings(const PatternPresenceHalconConfig &config)
{
    ScaleRangeSettings settings;
    if (config.scaleMin <= 0 || config.scaleMax <= 0 || config.scaleMin > config.scaleMax) {
        settings.minScale = 0.9;
        settings.maxScale = 1.1;
        settings.fallback = true;
        settings.fallbackReason = QStringLiteral("invalid UI scale range; fallback to 0.9-1.1");
        return settings;
    }

    settings.minScale = static_cast<double>(config.scaleMin) / 100.0;
    settings.maxScale = static_cast<double>(config.scaleMax) / 100.0;
    return settings;
}

ShapeModelContrastSettings shapeModelContrastSettings(const PatternPresenceHalconConfig &config)
{
    ShapeModelContrastSettings settings;
    settings.sensitivity = qBound(1, config.templateSensitivity, 10);
    settings.contrast = 70 - settings.sensitivity * 3;
    settings.minContrast = qMax(18, settings.contrast / 2);
    if (settings.minContrast >= settings.contrast)
        settings.minContrast = qMax(1, settings.contrast - 1);
    return settings;
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

QJsonArray intsToJson(const QVector<int> &values)
{
    QJsonArray array;
    for (const int value : values)
        array.append(value);
    return array;
}

QJsonArray doublesToJson(const QVector<double> &values)
{
    QJsonArray array;
    for (const double value : values)
        array.append(value);
    return array;
}

QString intsToKey(const QVector<int> &values)
{
    QStringList parts;
    parts.reserve(values.size());
    for (const int value : values)
        parts.append(QString::number(value));
    return parts.join(QLatin1Char(','));
}

QJsonObject autoDomainThresholdStatsToJson(const PatternPresenceAutoModelDomainThresholdStats &stats)
{
    QJsonObject json;
    json.insert(QStringLiteral("thresholdHigh"), stats.thresholdHigh);
    json.insert(QStringLiteral("thresholdRegionArea"), stats.thresholdRegionArea);
    json.insert(QStringLiteral("connectedCount"), stats.connectedCount);
    json.insert(QStringLiteral("rawAreas"), doublesToJson(stats.rawAreas));
    json.insert(QStringLiteral("areaCandidateCount"), stats.areaCandidateCount);
    json.insert(QStringLiteral("selectedArea"), stats.selectedArea);
    json.insert(QStringLiteral("selectedAreaRatio"), stats.selectedAreaRatio);
    json.insert(QStringLiteral("rejectedReason"), stats.rejectedReason);
    return json;
}

QJsonArray autoDomainStatsToJson(const QVector<PatternPresenceAutoModelDomainThresholdStats> &statsList)
{
    QJsonArray array;
    for (const PatternPresenceAutoModelDomainThresholdStats &stats : statsList)
        array.append(autoDomainThresholdStatsToJson(stats));
    return array;
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
    return std::abs(sum) / 2.0;
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

QString safeFileComponent(QString value)
{
    value = value.trimmed();
    if (value.isEmpty())
        value = QStringLiteral("tool");

    for (QChar &ch : value) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('_') && ch != QLatin1Char('-'))
            ch = QLatin1Char('_');
    }
    return value.left(64);
}

QString createPatternContourDebugDir(const QString &toolId)
{
    static QMutex mutex;
    static int sequence = 0;

    QMutexLocker locker(&mutex);
    const QString rootPath = QStringLiteral("/home/hjl-ubuntu/桌面/pattern_contour_debug");
    QDir root;
    root.mkpath(rootPath);

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    const QString dirName = QStringLiteral("%1_%2_%3")
            .arg(timestamp,
                 QString::number(++sequence),
                 safeFileComponent(toolId));
    QDir rootDir(rootPath);
    rootDir.mkpath(dirName);
    return rootDir.filePath(dirName);
}

cv::Mat debugGrayMat(const cv::Mat &source)
{
    cv::Mat gray;
    if (source.empty())
        return gray;

    if (source.channels() == 1)
        gray = source.clone();
    else if (source.channels() == 3)
        cv::cvtColor(source, gray, cv::COLOR_BGR2GRAY);
    else if (source.channels() == 4)
        cv::cvtColor(source, gray, cv::COLOR_BGRA2GRAY);
    return gray;
}

cv::Mat debugBgrMat(const cv::Mat &source)
{
    cv::Mat bgr;
    if (source.empty())
        return bgr;

    if (source.channels() == 1)
        cv::cvtColor(source, bgr, cv::COLOR_GRAY2BGR);
    else if (source.channels() == 3)
        bgr = source.clone();
    else if (source.channels() == 4)
        cv::cvtColor(source, bgr, cv::COLOR_BGRA2BGR);
    return bgr;
}

bool saveDebugImage(const QString &path, const cv::Mat &image)
{
    if (path.trimmed().isEmpty() || image.empty())
        return false;

    const QByteArray encodedPath = QFile::encodeName(path);
    return cv::imwrite(encodedPath.constData(), image);
}

bool saveDebugJson(const QString &path, const QJsonObject &json)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    return true;
}

cv::Point roundedCvPoint(const QPointF &point)
{
    return cv::Point(qRound(point.x()), qRound(point.y()));
}

void drawContourPolylines(cv::Mat &canvas,
                          const QVector<QVector<QPointF>> &contours,
                          const cv::Scalar &color)
{
    if (canvas.empty())
        return;

    for (const QVector<QPointF> &contour : contours) {
        if (contour.size() < 2)
            continue;
        for (int index = 1; index < contour.size(); ++index) {
            const QPointF &a = contour.at(index - 1);
            const QPointF &b = contour.at(index);
            if (!std::isfinite(a.x()) || !std::isfinite(a.y()) ||
                !std::isfinite(b.x()) || !std::isfinite(b.y())) {
                continue;
            }
            cv::line(canvas,
                     roundedCvPoint(a),
                     roundedCvPoint(b),
                     color,
                     1,
                     cv::LINE_AA);
        }
    }
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

QString rectToKey(const QRectF &rect)
{
    return QStringLiteral("%1,%2,%3,%4")
            .arg(QString::number(rect.x(), 'g', 17),
                 QString::number(rect.y(), 'g', 17),
                 QString::number(rect.width(), 'g', 17),
                 QString::number(rect.height(), 'g', 17));
}

QString pointsToKey(const QVector<QPointF> &points)
{
    QStringList parts;
    parts.reserve(points.size());
    for (const QPointF &point : points) {
        parts.append(QStringLiteral("%1,%2")
                     .arg(QString::number(point.x(), 'g', 17),
                          QString::number(point.y(), 'g', 17)));
    }
    return parts.join(QLatin1Char(';'));
}

QString rectToLogString(const QRectF &rect)
{
    return QStringLiteral("%1,%2,%3,%4")
            .arg(rect.x(), 0, 'f', 6)
            .arg(rect.y(), 0, 'f', 6)
            .arg(rect.width(), 0, 'f', 6)
            .arg(rect.height(), 0, 'f', 6);
}

QString pixelRectToLogString(const QRect &rect)
{
    return QStringLiteral("%1,%2,%3,%4")
            .arg(rect.x())
            .arg(rect.y())
            .arg(rect.width())
            .arg(rect.height());
}

QString matSizeString(const cv::Mat &mat)
{
    return mat.empty() ? QStringLiteral("--")
                       : QStringLiteral("%1x%2").arg(mat.cols).arg(mat.rows);
}

void mixHashByte(quint64 &hash, const uchar value)
{
    hash ^= static_cast<quint64>(value);
    hash *= 1099511628211ULL;
}

void mixHashInt(quint64 &hash, const int value)
{
    const quint32 unsignedValue = static_cast<quint32>(value);
    for (int shift = 0; shift < 32; shift += 8)
        mixHashByte(hash, static_cast<uchar>((unsignedValue >> shift) & 0xffU));
}

quint64 matContentHash(const cv::Mat &mat)
{
    quint64 hash = 1469598103934665603ULL;
    mixHashInt(hash, mat.rows);
    mixHashInt(hash, mat.cols);
    mixHashInt(hash, mat.type());
    if (mat.empty())
        return hash;

    const size_t rowBytes = static_cast<size_t>(mat.cols) * mat.elemSize();
    if (mat.isContinuous()) {
        const uchar *data = mat.ptr<uchar>(0);
        const size_t bytes = mat.total() * mat.elemSize();
        for (size_t index = 0; index < bytes; ++index)
            mixHashByte(hash, data[index]);
        return hash;
    }

    for (int row = 0; row < mat.rows; ++row) {
        const uchar *data = mat.ptr<uchar>(row);
        for (size_t index = 0; index < rowBytes; ++index)
            mixHashByte(hash, data[index]);
    }
    return hash;
}

QString buildShapeModelCacheKey(const PatternPresenceHalconConfig &config,
                                const cv::Mat &referenceImage,
                                const cv::Mat &templateMat,
                                const quint64 templateHash)
{
    const ShapeModelContrastSettings contrastSettings = shapeModelContrastSettings(config);
    PatternPresenceAutoModelDomainParams autoParams =
            defaultPatternPresenceAutoModelDomainParams();
    const QString templateShapeKey = config.templateShapeType.trimmed().toLower();
    autoParams.polygonTemplate = templateShapeKey == QStringLiteral("polygon") ||
                                 templateShapeKey == QStringLiteral("poly") ||
                                 templateShapeKey.contains(QStringLiteral("多边形"));
    const double autoDomainMaxRatio = autoParams.polygonTemplate
            ? autoParams.polygonRejectMaxAreaRatio
            : autoParams.rejectMaxAreaRatio;
    const QString baseKey = config.modelCacheKey.trimmed().isEmpty()
            ? QStringLiteral("%1_shape_model").arg(config.toolId.trimmed())
            : config.modelCacheKey.trimmed();
    return QStringList{
            baseKey,
            QStringLiteral("toolId=%1").arg(config.toolId.trimmed()),
            QStringLiteral("source=%1").arg(config.templateSource.trimmed()),
            QStringLiteral("ref=%1x%2").arg(referenceImage.cols).arg(referenceImage.rows),
            QStringLiteral("template=%1x%2:type%3:hash%4")
                    .arg(templateMat.cols)
                    .arg(templateMat.rows)
                    .arg(templateMat.type())
                    .arg(QString::number(templateHash, 16)),
            QStringLiteral("templateShape=%1").arg(config.templateShapeType.trimmed()),
            QStringLiteral("templateRoi=%1").arg(rectToKey(config.templateRoiNormalized)),
            QStringLiteral("templatePolygon=%1").arg(pointsToKey(config.templatePolygonNormalized)),
            QStringLiteral("sensitivityMappingVersion=%1").arg(contrastSettings.version),
            QStringLiteral("templateSensitivity=%1").arg(contrastSettings.sensitivity),
            QStringLiteral("contrast=%1:minContrast=%2")
                    .arg(contrastSettings.contrast)
                    .arg(contrastSettings.minContrast),
            QStringLiteral("contrastMode=%1:manual=%2:%3:numLevels=%4")
                    .arg(config.contrastMode.trimmed())
                    .arg(config.contrast)
                    .arg(config.minContrast)
                    .arg(config.numLevels),
            QStringLiteral("autoModelDomain=%1:thresholds=%2:minAreaRatio=%3:maxAreaRatio=%4:opening=%5:closing=%6:dilation=%7")
                    .arg(autoParams.version)
                    .arg(intsToKey(autoParams.thresholdHighCandidates))
                    .arg(autoParams.minAreaRatio, 0, 'g', 12)
                    .arg(autoDomainMaxRatio, 0, 'g', 12)
                    .arg(autoParams.openingRadius, 0, 'g', 12)
                    .arg(autoParams.closingRadius, 0, 'g', 12)
                    .arg(autoParams.dilationRadius, 0, 'g', 12),
            QStringLiteral("polarity=%1").arg(config.polarity.trimmed()),
            QStringLiteral("angle=%1:%2").arg(config.angleStart).arg(config.angleExtent),
            QStringLiteral("scale=%1:%2").arg(config.scaleMin).arg(config.scaleMax),
            QStringLiteral("halcon=%1").arg(config.halconSoPath.trimmed())
    }.join(QStringLiteral("|"));
}

QJsonArray scoresToJson(const QVector<double> &scores)
{
    QJsonArray array;
    for (const double score : scores)
        array.append(score);
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

ToolOverlay rectOverlay(const QRectF &rect, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.rect = rect;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

ToolOverlay polygonOverlay(const QVector<QPointF> &points, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Polygon;
    overlay.points = points;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

ToolOverlay lineOverlay(const QPointF &p1,
                        const QPointF &p2,
                        const QString &label,
                        const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Line;
    overlay.p1 = p1;
    overlay.p2 = p2;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

ToolOverlay textOverlay(const QPointF &position,
                        const QString &text,
                        const QString &label,
                        const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.p1 = position;
    overlay.text = text;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

QString halconMetricForPolarity(const QString &polarity, bool *fallback = nullptr)
{
    const QString key = polarity.trimmed().toLower();
    if (fallback)
        *fallback = false;
    if (key == QStringLiteral("use_polarity") ||
        key == QStringLiteral("consider") ||
        key.contains(QStringLiteral("use")) ||
        key.contains(QStringLiteral("考虑"))) {
        return QStringLiteral("use_polarity");
    }
    if (key == QStringLiteral("ignore_local_polarity") ||
        key.contains(QStringLiteral("local")) ||
        key.contains(QStringLiteral("局部"))) {
        return QStringLiteral("ignore_local_polarity");
    }
    if (key == QStringLiteral("ignore_polarity") ||
        key == QStringLiteral("ignore_global_polarity") ||
        key.contains(QStringLiteral("ignore")) ||
        key.contains(QStringLiteral("不考虑")) ||
        key.contains(QStringLiteral("忽略"))) {
        return QStringLiteral("ignore_global_polarity");
    }
    if (fallback)
        *fallback = true;
    return QStringLiteral("use_polarity");
}

bool isPresenceJudgeBasis(const QString &judgeBasis)
{
    const QString key = judgeBasis.trimmed().toLower();
    return key == QStringLiteral("presence") ||
           key == QStringLiteral("resultpresence") ||
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

bool isRectangleTemplateType(const QString &shapeType)
{
    const QString key = shapeType.trimmed().toLower();
    return key.isEmpty() ||
           key == QStringLiteral("rectangle") ||
           key == QStringLiteral("rect") ||
           key.contains(QStringLiteral("矩形"));
}

bool isPolygonTemplateType(const QString &shapeType)
{
    const QString key = shapeType.trimmed().toLower();
    return key == QStringLiteral("polygon") ||
           key == QStringLiteral("poly") ||
           key.contains(QStringLiteral("多边形"));
}

bool isSupportedTemplateType(const QString &shapeType)
{
    return isRectangleTemplateType(shapeType) || isPolygonTemplateType(shapeType);
}

bool isSupportedDetectRegionType(const QString &regionType)
{
    const QString key = regionType.trimmed().toLower();
    return key.isEmpty() ||
           key == QStringLiteral("rectangle") ||
           key == QStringLiteral("rect") ||
           key == QStringLiteral("polygon") ||
           key == QStringLiteral("poly") ||
           key == QStringLiteral("free") ||
           key.contains(QStringLiteral("矩形")) ||
           key.contains(QStringLiteral("多边形"));
}

bool isPolygonDetectRegionType(const QString &regionType)
{
    const QString key = regionType.trimmed().toLower();
    return key == QStringLiteral("polygon") ||
           key == QStringLiteral("poly") ||
           key.contains(QStringLiteral("多边形"));
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

void insertAutoModelDomainDefaults(QJsonObject &payload)
{
    const PatternPresenceAutoModelDomainParams params =
            defaultPatternPresenceAutoModelDomainParams();
    payload.insert(QStringLiteral("autoModelDomainEnabled"), true);
    payload.insert(QStringLiteral("autoModelDomainApplied"), false);
    payload.insert(QStringLiteral("autoModelDomainRoute"), params.version);
    payload.insert(QStringLiteral("autoModelDomainThresholdHigh"), params.thresholdHigh);
    payload.insert(QStringLiteral("autoModelDomainThresholdTriedValues"),
                   intsToJson(params.thresholdHighCandidates));
    payload.insert(QStringLiteral("thresholdTriedValues"),
                   intsToJson(params.thresholdHighCandidates));
    payload.insert(QStringLiteral("autoModelDomainSelectedThresholdHigh"), 0);
    payload.insert(QStringLiteral("selectedThresholdHigh"), 0);
    payload.insert(QStringLiteral("autoModelDomainMinAreaRatio"), params.minAreaRatio);
    payload.insert(QStringLiteral("autoModelDomainMaxAreaRatio"), params.maxAreaRatio);
    payload.insert(QStringLiteral("autoModelDomainOpeningRadius"), params.openingRadius);
    payload.insert(QStringLiteral("autoModelDomainClosingRadius"), params.closingRadius);
    payload.insert(QStringLiteral("autoModelDomainDilationRadius"), params.dilationRadius);
    payload.insert(QStringLiteral("autoModelDomainCandidateCount"), 0);
    payload.insert(QStringLiteral("autoModelDomainNoBorderCandidateCount"), 0);
    payload.insert(QStringLiteral("autoModelDomainThresholdRegionArea"), 0.0);
    payload.insert(QStringLiteral("thresholdRegionArea"), 0.0);
    payload.insert(QStringLiteral("autoModelDomainConnectedRegionCount"), 0);
    payload.insert(QStringLiteral("connectedRegionCount"), 0);
    payload.insert(QStringLiteral("autoModelDomainRawCandidateCount"), 0);
    payload.insert(QStringLiteral("rawCandidateCount"), 0);
    payload.insert(QStringLiteral("autoModelDomainAreaCandidateCount"), 0);
    payload.insert(QStringLiteral("areaCandidateCount"), 0);
    payload.insert(QStringLiteral("autoModelDomainSelectedCandidateArea"), 0.0);
    payload.insert(QStringLiteral("selectedCandidateArea"), 0.0);
    payload.insert(QStringLiteral("autoModelDomainSelectedCandidateIndex"), -1);
    payload.insert(QStringLiteral("selectedCandidateIndex"), -1);
    payload.insert(QStringLiteral("autoModelDomainSelectedCandidateAreaRatio"), 0.0);
    payload.insert(QStringLiteral("selectedCandidateAreaRatio"), 0.0);
    payload.insert(QStringLiteral("autoModelDomainTargetArea"), 0.0);
    payload.insert(QStringLiteral("autoModelDomainUserRoiArea"), 0.0);
    payload.insert(QStringLiteral("autoModelDomainTargetAreaRatio"), 0.0);
    payload.insert(QStringLiteral("autoModelDomainFallbackReason"), QString());
    payload.insert(QStringLiteral("autoModelDomainFailureStage"), QString());
    payload.insert(QStringLiteral("autoModelDomainFailureReason"), QString());
    payload.insert(QStringLiteral("failureStage"), QString());
    payload.insert(QStringLiteral("failureReason"), QString());
    payload.insert(QStringLiteral("autoModelDomainWarning"), QString());
    payload.insert(QStringLiteral("autoModelDomainPerThresholdStats"), QJsonArray());
    payload.insert(QStringLiteral("perThresholdStats"), QJsonArray());
    payload.insert(QStringLiteral("autoModelDomainTargetRow"), 0.0);
    payload.insert(QStringLiteral("autoModelDomainTargetColumn"), 0.0);
    payload.insert(QStringLiteral("modelDomainSource"), QStringLiteral("roi_reduce_domain_fallback"));
}

void fillPayload(PatternPresenceHalconResult &result,
                 const cv::Mat &image,
                 const cv::Mat &referenceImage,
                 const PatternPresenceHalconConfig &config)
{
    const ShapeModelContrastSettings contrastSettings = shapeModelContrastSettings(config);
    const ScaleRangeSettings scaleSettings = scaleRangeSettings(config);
    bool polarityFallback = false;
    const QString metric = halconMetricForPolarity(config.polarity, &polarityFallback);
    result.payload.insert(QStringLiteral("toolId"), config.toolId);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("scaled_shape_model"));
    result.payload.insert(QStringLiteral("shapeModelType"), QStringLiteral("scaled_shape_model"));
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
    result.payload.insert(QStringLiteral("detectRoiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("templateRoiNormalized"), rectToJson(config.templateRoiNormalized));
    result.payload.insert(QStringLiteral("templateSource"), config.templateSource);
    result.payload.insert(QStringLiteral("templateImagePath"), config.templateImagePath);
    result.payload.insert(QStringLiteral("modelPath"), config.modelPath);
    result.payload.insert(QStringLiteral("modelAutoCreate"), config.modelAutoCreate);
    result.payload.insert(QStringLiteral("modelCacheKey"), config.modelCacheKey);
    result.payload.insert(QStringLiteral("templateShapeType"), config.templateShapeType);
    result.payload.insert(QStringLiteral("templatePolygonNormalized"),
                          pointsToJson(config.templatePolygonNormalized));
    result.payload.insert(QStringLiteral("templateSensitivityMode"), config.templateSensitivityMode);
    result.payload.insert(QStringLiteral("templateSensitivity"), config.templateSensitivity);
    result.payload.insert(QStringLiteral("templateSensitivityClamped"), contrastSettings.sensitivity);
    result.payload.insert(QStringLiteral("sensitivityMappingVersion"), contrastSettings.version);
    result.payload.insert(QStringLiteral("createShapeModelContrast"), contrastSettings.contrast);
    result.payload.insert(QStringLiteral("createShapeModelMinContrast"), contrastSettings.minContrast);
    result.payload.insert(QStringLiteral("createShapeModelContrastUsed"), contrastSettings.contrast);
    result.payload.insert(QStringLiteral("createShapeModelMinContrastUsed"), contrastSettings.minContrast);
    result.payload.insert(QStringLiteral("createShapeModelContrastSource"), contrastSettings.source);
    result.payload.insert(QStringLiteral("createShapeModelContrastMapping"), contrastSettings.mapping);
    result.payload.insert(QStringLiteral("autoDomainContrastOverrideApplied"), false);
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("detectPolygonNormalized"),
                          pointsToJson(config.detectPolygonNormalized));
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("minScore"), config.minScore);
    result.payload.insert(QStringLiteral("polarity"), config.polarity);
    result.payload.insert(QStringLiteral("scaleMin"), config.scaleMin);
    result.payload.insert(QStringLiteral("scaleMax"), config.scaleMax);
    result.payload.insert(QStringLiteral("scaleRangeApplied"), true);
    result.payload.insert(QStringLiteral("scaleRangeFallback"), scaleSettings.fallback);
    result.payload.insert(QStringLiteral("scaleRangeFallbackReason"), scaleSettings.fallbackReason);
    result.payload.insert(QStringLiteral("scaleMinUsed"), scaleSettings.minScale);
    result.payload.insert(QStringLiteral("scaleMaxUsed"), scaleSettings.maxScale);
    result.payload.insert(QStringLiteral("angleStart"), config.angleStart);
    result.payload.insert(QStringLiteral("angleExtent"), config.angleExtent);
    result.payload.insert(QStringLiteral("angleStartDegUsed"), config.angleStart);
    result.payload.insert(QStringLiteral("angleExtentDegUsed"), config.angleExtent);
    result.payload.insert(QStringLiteral("timeoutMs"), config.timeoutMs);
    result.payload.insert(QStringLiteral("showContourPoints"), config.showContourPoints);
    result.payload.insert(QStringLiteral("showContourPointsRequested"), config.showContourPoints);
    result.payload.insert(QStringLiteral("showContourPointsApplied"), false);
    result.payload.insert(QStringLiteral("contourOverlayEmitted"), false);
    result.payload.insert(QStringLiteral("contourSource"),
                          config.showContourPoints ? QStringLiteral("pending")
                                                   : QStringLiteral("not_requested"));
    result.payload.insert(QStringLiteral("displayContourSource"), QString());
    result.payload.insert(QStringLiteral("displayContourObjectCount"), 0);
    result.payload.insert(QStringLiteral("displayContourPointCount"), 0);
    result.payload.insert(QStringLiteral("contourOverlayReason"), QString());
    result.payload.insert(QStringLiteral("shapeModelContourDebugAvailable"), false);
    result.payload.insert(QStringLiteral("shapeModelContourObjectCount"), 0);
    result.payload.insert(QStringLiteral("shapeModelContourPointCount"), 0);
    result.payload.insert(QStringLiteral("sortMode"), config.sortMode);
    result.payload.insert(QStringLiteral("sortModeRequested"), config.sortMode);
    result.payload.insert(QStringLiteral("sortModeApplied"), false);
    result.payload.insert(QStringLiteral("sortModeReason"), QStringLiteral("UI exists but runner does not support sorting yet"));
    result.payload.insert(QStringLiteral("judgeBasis"), config.judgeBasis);
    result.payload.insert(QStringLiteral("existOk"), config.existOk);
    result.payload.insert(QStringLiteral("scoreThreshold"), config.scoreThreshold);
    result.payload.insert(QStringLiteral("maxCount"), config.maxCount);
    result.payload.insert(QStringLiteral("expectedCount"), config.expectedCount);
    result.payload.insert(QStringLiteral("scaleApplied"), true);
    result.payload.insert(QStringLiteral("positionCorrectionRequested"),
                          config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"),
                          config.positionCorrection.applied);
    result.payload.insert(QStringLiteral("positionCorrectionSourceId"),
                          config.positionCorrection.sourceId);
    result.payload.insert(QStringLiteral("positionCorrectionReason"),
                          config.positionCorrection.applied
                          ? QStringLiteral("applied") : QStringLiteral("not_requested"));
    result.payload.insert(QStringLiteral("referenceScale"),
                          config.positionCorrection.referenceScale);
    result.payload.insert(QStringLiteral("runScale"),
                          config.positionCorrection.runScale);
    result.payload.insert(QStringLiteral("scaleRatio"),
                          config.positionCorrection.scaleRatio);
    result.payload.insert(QStringLiteral("maskApplied"), false);
    result.payload.insert(QStringLiteral("maskReason"), QStringLiteral("UI exists but runner does not support yet"));
    result.payload.insert(QStringLiteral("templateMaskApplied"), false);
    result.payload.insert(QStringLiteral("templatePolygonApplied"), false);
    result.payload.insert(QStringLiteral("polygonApplied"), false);
    result.payload.insert(QStringLiteral("polygonFallbackToBoundingRect"), false);
    result.payload.insert(QStringLiteral("polygonRegionArea"), 0.0);
    result.payload.insert(QStringLiteral("halconRegionArea"), 0.0);
    result.payload.insert(QStringLiteral("reducedDomainArea"), 0.0);
    result.payload.insert(QStringLiteral("templatePolygonHalconRegionArea"), 0.0);
    result.payload.insert(QStringLiteral("reduceDomainApplied"), false);
    result.payload.insert(QStringLiteral("modelPointStatus"), QStringLiteral("not_evaluated"));
    result.payload.insert(QStringLiteral("halconErrorCode"), 0);
    result.payload.insert(QStringLiteral("halconErrorMessage"), QString());
    result.payload.insert(QStringLiteral("detectMaskApplied"), false);
    result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), false);
    result.payload.insert(QStringLiteral("found"), false);
    result.payload.insert(QStringLiteral("foundCount"), 0);
    result.payload.insert(QStringLiteral("bestScore"), 0.0);
    result.payload.insert(QStringLiteral("usedMinScore"), normalizedScore(config.minScore));
    result.payload.insert(QStringLiteral("usedMetric"), metric);
    result.payload.insert(QStringLiteral("metricUsed"), metric);
    result.payload.insert(QStringLiteral("polarityFallback"), polarityFallback);
    result.payload.insert(QStringLiteral("numLevelsUsed"), QStringLiteral("auto"));
    result.payload.insert(QStringLiteral("findNumLevelsUsed"), 0);
    result.payload.insert(QStringLiteral("greedinessUsed"), 0.5);
    result.payload.insert(QStringLiteral("maxOverlapUsed"), 0.5);
    result.payload.insert(QStringLiteral("minContrastUsed"), contrastSettings.minContrast);
    result.payload.insert(QStringLiteral("timeoutMsUsed"), config.timeoutMs);
    result.payload.insert(QStringLiteral("modelContourOverlayApplied"), false);
    result.payload.insert(QStringLiteral("matchBboxOverlayApplied"), false);
    result.payload.insert(QStringLiteral("okNgReason"), QString());
    result.payload.insert(QStringLiteral("modelCacheHit"), false);
    result.payload.insert(QStringLiteral("modelBuildMs"), 0.0);
    result.payload.insert(QStringLiteral("findMs"), 0.0);
    result.payload.insert(QStringLiteral("elapsedMs"), 0.0);
    result.payload.insert(QStringLiteral("timeoutExceeded"), false);
    insertAutoModelDomainDefaults(result.payload);
}

PatternPresenceHalconResult makeParameterError(const QString &status,
                                               const QString &error,
                                               const cv::Mat &image,
                                               const cv::Mat &referenceImage,
                                               const PatternPresenceHalconConfig &config)
{
    PatternPresenceHalconResult result;
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

struct HalconFailure
{
    QString status;
    QString message;
    QString stage;
    Herror code = H_MSG_OK;
    QString halconMessage;
};

struct GrayHalconImage
{
    Hobject inputImage = NO_OBJECTS;
    Hobject grayImage = NO_OBJECTS;
    Hobject graySource = NO_OBJECTS;
};

void destroyTuple(PatternPresenceHalconApi *api, Htuple &tuple)
{
    if (api && api->destroyTuple && (tuple.num > 0 || tuple.capacity > 0))
        api->destroyTuple(&tuple);
    tuple = HTUPLE_INITIALIZER;
}

double halconRegionArea(PatternPresenceHalconApi *api, const Hobject region)
{
    if (!api || !api->areaCenter || !patternPresenceHalconObjectAllocated(region))
        return 0.0;

    Htuple areaTuple = HTUPLE_INITIALIZER;
    Htuple rowTuple = HTUPLE_INITIALIZER;
    Htuple columnTuple = HTUPLE_INITIALIZER;
    double area = 0.0;
    if (patternPresenceHalconStatusOk(api->areaCenter(region,
                                                      &areaTuple,
                                                      &rowTuple,
                                                      &columnTuple))) {
        for (int index = 0; index < areaTuple.num; ++index)
            area += qMax(0.0, api->getDouble(&areaTuple, index));
    }

    destroyTuple(api, columnTuple);
    destroyTuple(api, rowTuple);
    destroyTuple(api, areaTuple);
    return area;
}

void clearObject(PatternPresenceHalconApi *api, Hobject &object)
{
    if (api && api->clearObj && patternPresenceHalconObjectAllocated(object))
        api->clearObj(object);
    object = NO_OBJECTS;
}

struct XldContourReadResult
{
    QVector<QVector<QPointF>> contours;
    int objectCount = 0;
    int pointCount = 0;
    QString error;
};

XldContourReadResult readXldContours(PatternPresenceHalconApi *api, const Hobject contours)
{
    XldContourReadResult result;
    if (!api || !api->countObj || !api->selectObj || !api->getContourXld) {
        result.error = QStringLiteral("xld contour read operators unavailable");
        return result;
    }

    Hlong objectCount = 0;
    if (!patternPresenceHalconStatusOk(api->countObj(contours, &objectCount))) {
        result.error = QStringLiteral("count_obj failed");
        return result;
    }
    result.objectCount = qMax(0, static_cast<int>(objectCount));
    result.contours.reserve(result.objectCount);

    for (Hlong objectIndex = 1; objectIndex <= objectCount; ++objectIndex) {
        Hobject selected = NO_OBJECTS;
        Htuple rows = HTUPLE_INITIALIZER;
        Htuple columns = HTUPLE_INITIALIZER;

        if (!patternPresenceHalconStatusOk(api->selectObj(contours, &selected, objectIndex))) {
            result.error = QStringLiteral("select_obj failed");
            clearObject(api, selected);
            return result;
        }
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
        if (!points.isEmpty())
            result.contours.append(points);

        destroyTuple(api, columns);
        destroyTuple(api, rows);
        clearObject(api, selected);
    }

    return result;
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

cv::Mat maskedAutoDomainThreshold(const cv::Mat &gray,
                                  const int thresholdHigh,
                                  const bool polygonTemplate,
                                  const QVector<QPointF> &polygonLocalPoints)
{
    cv::Mat mask;
    if (gray.empty() || thresholdHigh <= 0)
        return mask;

    cv::threshold(gray, mask, static_cast<double>(thresholdHigh), 255.0, cv::THRESH_BINARY_INV);
    if (!polygonTemplate || polygonLocalPoints.size() < 3)
        return mask;

    cv::Mat polygonMask = cv::Mat::zeros(gray.size(), CV_8UC1);
    std::vector<cv::Point> polygon;
    polygon.reserve(static_cast<size_t>(polygonLocalPoints.size()));
    for (const QPointF &point : polygonLocalPoints) {
        polygon.emplace_back(qBound(0, qRound(point.x()), gray.cols - 1),
                             qBound(0, qRound(point.y()), gray.rows - 1));
    }
    const std::vector<std::vector<cv::Point>> polygons{polygon};
    cv::fillPoly(polygonMask, polygons, cv::Scalar(255));
    cv::bitwise_and(mask, polygonMask, mask);
    return mask;
}

std::vector<std::vector<cv::Point>> thresholdContours(const cv::Mat &mask)
{
    std::vector<std::vector<cv::Point>> contours;
    if (mask.empty())
        return contours;

    cv::Mat mutableMask = mask.clone();
    cv::findContours(mutableMask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    return contours;
}

int selectedContourIndex(const std::vector<std::vector<cv::Point>> &contours,
                         const double selectedArea)
{
    if (contours.empty())
        return -1;

    int bestIndex = -1;
    double bestMetric = std::numeric_limits<double>::max();
    double largestArea = 0.0;
    int largestIndex = -1;
    for (int index = 0; index < static_cast<int>(contours.size()); ++index) {
        const double area = std::abs(cv::contourArea(contours.at(static_cast<size_t>(index))));
        if (area > largestArea) {
            largestArea = area;
            largestIndex = index;
        }
        if (selectedArea > 0.0) {
            const double metric = std::abs(area - selectedArea);
            if (metric < bestMetric) {
                bestMetric = metric;
                bestIndex = index;
            }
        }
    }
    return selectedArea > 0.0 ? bestIndex : largestIndex;
}

void saveAutoDomainMaskDebugImages(QJsonObject &payload,
                                   const cv::Mat &templateMat,
                                   const cv::Mat &templateGray,
                                   const bool polygonTemplate,
                                   const QVector<QPointF> &polygonLocalPoints,
                                   const int thresholdHigh,
                                   const double selectedCandidateArea,
                                   const QString &thresholdMaskPath,
                                   const QString &connectedCandidatesPath,
                                   const QString &selectedCandidatePath)
{
    if (thresholdMaskPath.isEmpty() && connectedCandidatesPath.isEmpty() && selectedCandidatePath.isEmpty())
        return;

    const cv::Mat mask = maskedAutoDomainThreshold(templateGray,
                                                  thresholdHigh,
                                                  polygonTemplate,
                                                  polygonLocalPoints);
    payload.insert(QStringLiteral("contourDebugAutoDomainThresholdMaskSaved"),
                   saveDebugImage(thresholdMaskPath, mask));

    const std::vector<std::vector<cv::Point>> contours = thresholdContours(mask);
    cv::Mat connectedDebug = debugBgrMat(templateMat);
    if (!connectedDebug.empty()) {
        for (int index = 0; index < static_cast<int>(contours.size()); ++index) {
            cv::drawContours(connectedDebug,
                             contours,
                             index,
                             cv::Scalar(0, 180, 255),
                             1,
                             cv::LINE_AA);
        }
    }
    payload.insert(QStringLiteral("contourDebugAutoDomainConnectedCandidatesSaved"),
                   saveDebugImage(connectedCandidatesPath, connectedDebug));

    cv::Mat selectedDebug = debugBgrMat(templateMat);
    const int selectedIndex = selectedContourIndex(contours, selectedCandidateArea);
    if (!selectedDebug.empty() && selectedIndex >= 0) {
        cv::drawContours(selectedDebug,
                         contours,
                         selectedIndex,
                         cv::Scalar(0, 255, 0),
                         cv::FILLED,
                         cv::LINE_AA);
        cv::addWeighted(debugBgrMat(templateMat),
                        0.65,
                        selectedDebug,
                        0.35,
                        0.0,
                        selectedDebug);
        cv::drawContours(selectedDebug,
                         contours,
                         selectedIndex,
                         cv::Scalar(0, 255, 255),
                         1,
                         cv::LINE_AA);
    }
    payload.insert(QStringLiteral("contourDebugAutoDomainSelectedCandidateSaved"),
                   saveDebugImage(selectedCandidatePath, selectedDebug));
}

XldContourReadResult readRegionContours(PatternPresenceHalconApi *api, const Hobject region)
{
    XldContourReadResult result;
    if (!api || !api->genContourRegionXld || !patternPresenceHalconObjectAllocated(region))
        return result;

    Hobject contour = NO_OBJECTS;
    if (!patternPresenceHalconStatusOk(api->genContourRegionXld(region, &contour, "border"))) {
        result.error = QStringLiteral("gen_contour_region_xld failed");
        clearObject(api, contour);
        return result;
    }
    result = readXldContours(api, contour);
    clearObject(api, contour);
    return result;
}

std::vector<std::vector<cv::Point>> contoursToCvPolygons(const QVector<QVector<QPointF>> &contours,
                                                         const cv::Size &size)
{
    std::vector<std::vector<cv::Point>> polygons;
    polygons.reserve(static_cast<size_t>(contours.size()));
    for (const QVector<QPointF> &contour : contours) {
        if (contour.size() < 3)
            continue;
        std::vector<cv::Point> polygon;
        polygon.reserve(static_cast<size_t>(contour.size()));
        for (const QPointF &point : contour) {
            polygon.emplace_back(qBound(0, qRound(point.x()), size.width - 1),
                                 qBound(0, qRound(point.y()), size.height - 1));
        }
        polygons.push_back(std::move(polygon));
    }
    return polygons;
}

void saveAutoDomainHalconRegionDebugImages(PatternPresenceHalconApi *api,
                                           QJsonObject &payload,
                                           const cv::Mat &templateMat,
                                           const Hobject thresholdRegion,
                                           const Hobject connectedRegions,
                                           const Hobject selectedCandidateRegion,
                                           const QString &thresholdMaskPath,
                                           const QString &connectedCandidatesPath,
                                           const QString &selectedCandidatePath)
{
    if (!api || templateMat.empty())
        return;

    const cv::Size size(templateMat.cols, templateMat.rows);
    const XldContourReadResult thresholdRead = readRegionContours(api, thresholdRegion);
    cv::Mat thresholdMask = cv::Mat::zeros(size, CV_8UC1);
    const std::vector<std::vector<cv::Point>> thresholdPolygons =
            contoursToCvPolygons(thresholdRead.contours, size);
    if (!thresholdPolygons.empty())
        cv::fillPoly(thresholdMask, thresholdPolygons, cv::Scalar(255));
    payload.insert(QStringLiteral("contourDebugAutoDomainThresholdMaskSaved"),
                   saveDebugImage(thresholdMaskPath, thresholdMask));

    const XldContourReadResult connectedRead = readRegionContours(api, connectedRegions);
    cv::Mat connectedDebug = debugBgrMat(templateMat);
    drawContourPolylines(connectedDebug,
                         connectedRead.contours,
                         cv::Scalar(0, 180, 255));
    payload.insert(QStringLiteral("contourDebugAutoDomainConnectedCandidatesSaved"),
                   saveDebugImage(connectedCandidatesPath, connectedDebug));

    const XldContourReadResult selectedRead = readRegionContours(api, selectedCandidateRegion);
    cv::Mat selectedDebug = debugBgrMat(templateMat);
    cv::Mat selectedOverlay = selectedDebug.clone();
    const std::vector<std::vector<cv::Point>> selectedPolygons =
            contoursToCvPolygons(selectedRead.contours, size);
    if (!selectedPolygons.empty()) {
        cv::fillPoly(selectedOverlay, selectedPolygons, cv::Scalar(0, 255, 0));
        cv::addWeighted(selectedDebug, 0.65, selectedOverlay, 0.35, 0.0, selectedDebug);
    }
    drawContourPolylines(selectedDebug,
                         selectedRead.contours,
                         cv::Scalar(0, 255, 255));
    payload.insert(QStringLiteral("contourDebugAutoDomainSelectedCandidateSaved"),
                   saveDebugImage(selectedCandidatePath, selectedDebug));
    payload.insert(QStringLiteral("contourDebugAutoDomainThresholdMaskPointCount"),
                   thresholdRead.pointCount);
    payload.insert(QStringLiteral("contourDebugAutoDomainConnectedCandidatesPointCount"),
                   connectedRead.pointCount);
    payload.insert(QStringLiteral("contourDebugAutoDomainSelectedCandidatePointCount"),
                   selectedRead.pointCount);
}

void saveAutoDomainRegionDebugImages(PatternPresenceHalconApi *api,
                                     QJsonObject &payload,
                                     const cv::Mat &templateMat,
                                     const Hobject displayContour,
                                     const QString &regionPath,
                                     const QString &contourLocalPath)
{
    XldContourReadResult readResult;
    if (api && patternPresenceHalconObjectAllocated(displayContour))
        readResult = readXldContours(api, displayContour);

    cv::Mat regionDebugImage = debugBgrMat(templateMat);
    drawContourPolylines(regionDebugImage,
                         readResult.contours,
                         cv::Scalar(0, 180, 0));
    payload.insert(QStringLiteral("contourDebugAutoModelDomainRegionSaved"),
                   saveDebugImage(regionPath, regionDebugImage));

    cv::Mat localDebugImage = debugBgrMat(templateMat);
    drawContourPolylines(localDebugImage,
                         readResult.contours,
                         cv::Scalar(0, 255, 255));
    payload.insert(QStringLiteral("contourDebugAutoModelDomainContourLocalSaved"),
                   saveDebugImage(contourLocalPath, localDebugImage));
    payload.insert(QStringLiteral("contourDebugAutoModelDomainContourLocalObjectCount"),
                   readResult.objectCount);
    payload.insert(QStringLiteral("contourDebugAutoModelDomainContourLocalPointCount"),
                   readResult.pointCount);
    if (!readResult.error.isEmpty())
        payload.insert(QStringLiteral("contourDebugAutoModelDomainContourLocalError"),
                       readResult.error);
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
                                         QStringLiteral("contour_line"),
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

struct CachedShapeModel
{
    CachedShapeModel(const QString &cacheKey,
                     const QSharedPointer<PatternPresenceHalconLibrary> &halconLibrary,
                     const Htuple &modelTuple)
        : key(cacheKey)
        , library(halconLibrary)
        , modelIdTuple(modelTuple)
    {
    }

    ~CachedShapeModel()
    {
        if (!library)
            return;

        PatternPresenceHalconApi *api = &library->api;
        if (api->clearShapeModel && (modelIdTuple.num > 0 || modelIdTuple.capacity > 0))
            api->clearShapeModel(modelIdTuple);
        if (api->destroyTuple && (modelIdTuple.num > 0 || modelIdTuple.capacity > 0))
            api->destroyTuple(&modelIdTuple);
        modelIdTuple = HTUPLE_INITIALIZER;
        clearObject(api, displayContour);
        clearObject(api, modelContour);
    }

    QString key;
    QSharedPointer<PatternPresenceHalconLibrary> library;
    Htuple modelIdTuple = HTUPLE_INITIALIZER;
    bool templatePolygonApplied = false;
    bool autoModelDomainApplied = false;
    QString autoModelDomainFallbackReason;
    QString modelDomainSource = QStringLiteral("roi_reduce_domain_fallback");
    QString autoModelDomainRoute;
    Hobject displayContour = NO_OBJECTS;
    Hobject modelContour = NO_OBJECTS;
    double displayOriginRow = 0.0;
    double displayOriginColumn = 0.0;
    double targetArea = 0.0;
    double userRoiArea = 0.0;
    double targetAreaRatio = 0.0;
    int candidateCount = 0;
    int noBorderCandidateCount = 0;
    QVector<int> thresholdTriedValues;
    QJsonArray perThresholdStats;
    int selectedThresholdHigh = 0;
    double thresholdRegionArea = 0.0;
    int connectedRegionCount = 0;
    int rawCandidateCount = 0;
    int areaCandidateCount = 0;
    double selectedCandidateArea = 0.0;
    int selectedCandidateIndex = -1;
    double selectedCandidateAreaRatio = 0.0;
    QString failureStage;
    QString failureReason;
    QString warning;
    QMutex mutex;
};

QMutex &shapeModelCacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, QSharedPointer<CachedShapeModel>> &shapeModelCache()
{
    static QHash<QString, QSharedPointer<CachedShapeModel>> cache;
    return cache;
}

bool shouldLogPatternPerf(qint64 elapsedMs)
{
    static QMutex mutex;
    static int counter = 0;
    QMutexLocker locker(&mutex);
    ++counter;
    return elapsedMs > 100 || counter % 30 == 0;
}

void logPatternPerf(const PatternPresenceHalconConfig &config,
                    const cv::Mat &image,
                    const cv::Mat &referenceImage,
                    const cv::Mat &templateMat,
                    const cv::Mat &detectMat,
                    const QRect &templateRoiPixels,
                    const QRect &detectRoiPixels,
                    const bool modelCacheHit,
                    const QString &modelCacheKey,
                    const qint64 modelBuildMs,
                    const qint64 findMs,
                    const qint64 convertMs,
                    const qint64 totalMs,
                    const QString &rebuildReason)
{
    if (!shouldLogPatternPerf(totalMs))
        return;

    qInfo().noquote()
            << QStringList{
                   QStringLiteral("[PatternPerf]"),
                   QStringLiteral("toolId=%1").arg(config.toolId.trimmed()),
                   QStringLiteral("templateRoiNorm=%1").arg(rectToLogString(config.templateRoiNormalized)),
                   QStringLiteral("detectRoiNorm=%1").arg(rectToLogString(config.roiNormalized)),
                   QStringLiteral("templatePixel=%1").arg(pixelRectToLogString(templateRoiPixels)),
                   QStringLiteral("detectPixel=%1").arg(pixelRectToLogString(detectRoiPixels)),
                   QStringLiteral("templateMat=%1").arg(matSizeString(templateMat)),
                   QStringLiteral("detectMat=%1").arg(matSizeString(detectMat)),
                   QStringLiteral("modelCacheHit=%1").arg(modelCacheHit ? QStringLiteral("true") : QStringLiteral("false")),
                   QStringLiteral("modelCacheKey=%1").arg(modelCacheKey),
                   QStringLiteral("modelBuildMs=%1").arg(modelBuildMs),
                   QStringLiteral("findMs=%1").arg(findMs),
                   QStringLiteral("convertMs=%1").arg(convertMs),
                   QStringLiteral("totalMs=%1").arg(totalMs),
                   QStringLiteral("rebuildReason=%1").arg(rebuildReason),
                   QStringLiteral("referenceImageSize=%1").arg(matSizeString(referenceImage)),
                   QStringLiteral("currentImageSize=%1").arg(matSizeString(image)),
                   QStringLiteral("minScore=%1").arg(config.minScore),
                   QStringLiteral("angleStart=%1").arg(config.angleStart),
                   QStringLiteral("angleExtent=%1").arg(config.angleExtent),
                   QStringLiteral("scaleMin=%1").arg(config.scaleMin),
                   QStringLiteral("scaleMax=%1").arg(config.scaleMax),
                   QStringLiteral("polarity=%1").arg(config.polarity.trimmed())
               }.join(QLatin1Char(' '));
}

QString compactJsonValue(const QJsonValue &value)
{
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isDouble())
        return QString::number(value.toDouble(), 'f', 3);
    return value.toString();
}

void logPatternPolygonDebug(const PatternPresenceHalconConfig &config,
                            const QJsonObject &payload)
{
    if (!config.debugPolygonLog || !isPolygonTemplateType(config.templateShapeType))
        return;

    qInfo().noquote()
            << QStringList{
                   QStringLiteral("[PatternPolygon]"),
                   QStringLiteral("templateShapeType=%1").arg(config.templateShapeType.trimmed()),
                   QStringLiteral("pointsNorm=%1").arg(config.templatePolygonNormalized.size()),
                   QStringLiteral("globalPoints=%1").arg(compactJsonValue(payload.value(QStringLiteral("templatePolygonGlobalPoints")))),
                   QStringLiteral("localPoints=%1").arg(compactJsonValue(payload.value(QStringLiteral("templatePolygonLocalPoints")))),
                   QStringLiteral("polygonRegionArea=%1").arg(payload.value(QStringLiteral("polygonRegionArea")).toDouble(), 0, 'f', 2),
                   QStringLiteral("reduceDomainApplied=%1").arg(payload.value(QStringLiteral("reduceDomainApplied")).toBool() ? QStringLiteral("true") : QStringLiteral("false")),
                   QStringLiteral("autoModelDomainApplied=%1").arg(payload.value(QStringLiteral("autoModelDomainApplied")).toBool() ? QStringLiteral("true") : QStringLiteral("false")),
                   QStringLiteral("autoModelDomainCandidateCount=%1").arg(payload.value(QStringLiteral("autoModelDomainCandidateCount")).toInt()),
                   QStringLiteral("autoModelDomainTargetArea=%1").arg(payload.value(QStringLiteral("autoModelDomainTargetArea")).toDouble(), 0, 'f', 2),
                   QStringLiteral("autoModelDomainTargetAreaRatio=%1").arg(payload.value(QStringLiteral("autoModelDomainTargetAreaRatio")).toDouble(), 0, 'f', 4),
                   QStringLiteral("selectedThresholdHigh=%1").arg(payload.value(QStringLiteral("autoModelDomainSelectedThresholdHigh")).toInt()),
                   QStringLiteral("failureStage=%1").arg(payload.value(QStringLiteral("autoModelDomainFailureStage")).toString()),
                   QStringLiteral("fallbackReason=%1").arg(payload.value(QStringLiteral("autoModelDomainFallbackReason")).toString())
               }.join(QLatin1Char(' '));
}

void logPatternPresenceRoiDebug(const PatternPresenceHalconConfig &config,
                                const QJsonObject &payload,
                                const QString &status,
                                const QString &message)
{
    if (!config.debugPolygonLog && !config.showContourPoints)
        return;

    const QString roiType = payload.value(QStringLiteral("roiMode")).toString(
                config.templateShapeType.trimmed().isEmpty()
                ? QStringLiteral("rectangle")
                : config.templateShapeType.trimmed());
    const int imageWidth = payload.value(QStringLiteral("referenceImageWidth")).toInt(
                payload.value(QStringLiteral("imageWidth")).toInt());
    const int imageHeight = payload.value(QStringLiteral("referenceImageHeight")).toInt(
                payload.value(QStringLiteral("imageHeight")).toInt());
    const int halconErrorCode = payload.value(QStringLiteral("halconErrorCode")).toInt(0);
    const QString errorCode = halconErrorCode != 0
            ? QString::number(halconErrorCode)
            : (status.isEmpty() ? QStringLiteral("0") : status);
    const QString modelPointStatus =
            payload.value(QStringLiteral("modelPointStatus")).toString();
    const bool createModelSuccess = modelPointStatus == QStringLiteral("ok") ||
            payload.value(QStringLiteral("shapeModelOrigin")).toString() == QStringLiteral("model_cache") ||
            payload.value(QStringLiteral("shapeModelOrigin")).toString() == QStringLiteral("created_and_cached");

    qDebug().noquote()
            << QStringList{
                   QStringLiteral("[PatternPresence][ROI]"),
                   QStringLiteral("roiType=%1").arg(roiType),
                   QStringLiteral("imageWidth=%1").arg(imageWidth),
                   QStringLiteral("imageHeight=%1").arg(imageHeight),
                   QStringLiteral("templateRoiRect=%1")
                           .arg(compactJsonValue(payload.value(QStringLiteral("templateRoiPixels")))),
                   QStringLiteral("polygonPointCount=%1")
                           .arg(config.templatePolygonNormalized.size()),
                   QStringLiteral("polygonPointsImageCoord=%1")
                           .arg(compactJsonValue(payload.value(QStringLiteral("templatePolygonGlobalPoints")))),
                   QStringLiteral("polygonBBox=%1")
                           .arg(compactJsonValue(payload.value(QStringLiteral("templateBoundingRectPixel")))),
                   QStringLiteral("halconRegionArea=%1")
                           .arg(payload.value(QStringLiteral("halconRegionArea")).toDouble(), 0, 'f', 2),
                   QStringLiteral("reducedDomainArea=%1")
                           .arg(payload.value(QStringLiteral("reducedDomainArea")).toDouble(), 0, 'f', 2),
                   QStringLiteral("thresholdRegionArea=%1")
                           .arg(payload.value(QStringLiteral("thresholdRegionArea")).toDouble(), 0, 'f', 2),
                   QStringLiteral("xldContourCount=%1")
                           .arg(payload.value(QStringLiteral("shapeModelContourObjectCount")).toInt(
                                    payload.value(QStringLiteral("displayContourObjectCount")).toInt())),
                   QStringLiteral("modelPointCount=%1")
                           .arg(payload.value(QStringLiteral("shapeModelContourPointCount")).toInt(
                                    payload.value(QStringLiteral("displayContourPointCount")).toInt())),
                   QStringLiteral("createModelSuccess=%1")
                           .arg(createModelSuccess ? QStringLiteral("true") : QStringLiteral("false")),
                   QStringLiteral("errorCode=%1").arg(errorCode),
                   QStringLiteral("status=%1").arg(status),
                   QStringLiteral("message=%1").arg(message)
               }.join(QLatin1Char(' '));
}

void insertAutoModelDomainResultPayload(QJsonObject &payload,
                                        const PatternPresenceAutoModelDomainResult &autoDomain)
{
    payload.insert(QStringLiteral("autoModelDomainApplied"), autoDomain.applied);
    payload.insert(QStringLiteral("autoModelDomainFallbackReason"), autoDomain.fallbackReason);
    payload.insert(QStringLiteral("autoModelDomainRoute"), autoDomain.route);
    payload.insert(QStringLiteral("autoModelDomainThresholdHigh"), autoDomain.thresholdHigh);
    payload.insert(QStringLiteral("autoModelDomainThresholdTriedValues"),
                   intsToJson(autoDomain.thresholdTriedValues));
    payload.insert(QStringLiteral("thresholdTriedValues"),
                   intsToJson(autoDomain.thresholdTriedValues));
    payload.insert(QStringLiteral("autoModelDomainSelectedThresholdHigh"),
                   autoDomain.selectedThresholdHigh);
    payload.insert(QStringLiteral("selectedThresholdHigh"), autoDomain.selectedThresholdHigh);
    payload.insert(QStringLiteral("autoModelDomainMinAreaRatio"), autoDomain.minAreaRatio);
    payload.insert(QStringLiteral("autoModelDomainMaxAreaRatio"), autoDomain.maxAreaRatio);
    payload.insert(QStringLiteral("autoModelDomainOpeningRadius"), autoDomain.openingRadius);
    payload.insert(QStringLiteral("autoModelDomainClosingRadius"), autoDomain.closingRadius);
    payload.insert(QStringLiteral("autoModelDomainDilationRadius"), autoDomain.dilationRadius);
    payload.insert(QStringLiteral("autoModelDomainCandidateCount"), autoDomain.candidateCount);
    payload.insert(QStringLiteral("autoModelDomainNoBorderCandidateCount"),
                   autoDomain.noBorderCandidateCount);
    payload.insert(QStringLiteral("autoModelDomainThresholdRegionArea"),
                   autoDomain.thresholdRegionArea);
    payload.insert(QStringLiteral("thresholdRegionArea"), autoDomain.thresholdRegionArea);
    payload.insert(QStringLiteral("autoModelDomainConnectedRegionCount"),
                   autoDomain.connectedRegionCount);
    payload.insert(QStringLiteral("connectedRegionCount"), autoDomain.connectedRegionCount);
    payload.insert(QStringLiteral("autoModelDomainRawCandidateCount"),
                   autoDomain.rawCandidateCount);
    payload.insert(QStringLiteral("rawCandidateCount"), autoDomain.rawCandidateCount);
    payload.insert(QStringLiteral("autoModelDomainAreaCandidateCount"),
                   autoDomain.areaCandidateCount);
    payload.insert(QStringLiteral("areaCandidateCount"), autoDomain.areaCandidateCount);
    payload.insert(QStringLiteral("autoModelDomainSelectedCandidateArea"),
                   autoDomain.selectedCandidateArea);
    payload.insert(QStringLiteral("selectedCandidateArea"), autoDomain.selectedCandidateArea);
    payload.insert(QStringLiteral("autoModelDomainSelectedCandidateIndex"),
                   autoDomain.selectedCandidateIndex);
    payload.insert(QStringLiteral("selectedCandidateIndex"), autoDomain.selectedCandidateIndex);
    payload.insert(QStringLiteral("autoModelDomainSelectedCandidateAreaRatio"),
                   autoDomain.selectedCandidateAreaRatio);
    payload.insert(QStringLiteral("selectedCandidateAreaRatio"),
                   autoDomain.selectedCandidateAreaRatio);
    payload.insert(QStringLiteral("autoModelDomainTargetArea"), autoDomain.targetArea);
    payload.insert(QStringLiteral("autoModelDomainUserRoiArea"), autoDomain.userRoiArea);
    payload.insert(QStringLiteral("autoModelDomainTargetAreaRatio"), autoDomain.targetAreaRatio);
    payload.insert(QStringLiteral("autoModelDomainTargetRow"), autoDomain.targetRow);
    payload.insert(QStringLiteral("autoModelDomainTargetColumn"), autoDomain.targetColumn);
    payload.insert(QStringLiteral("autoModelDomainFailureStage"), autoDomain.failureStage);
    payload.insert(QStringLiteral("autoModelDomainFailureReason"), autoDomain.failureReason);
    payload.insert(QStringLiteral("failureStage"), autoDomain.failureStage);
    payload.insert(QStringLiteral("failureReason"), autoDomain.failureReason);
    payload.insert(QStringLiteral("autoModelDomainWarning"), autoDomain.warning);
    payload.insert(QStringLiteral("autoModelDomainPerThresholdStats"),
                   autoDomainStatsToJson(autoDomain.perThresholdStats));
    payload.insert(QStringLiteral("perThresholdStats"),
                   autoDomainStatsToJson(autoDomain.perThresholdStats));
    payload.insert(QStringLiteral("modelDomainSource"),
                   autoDomain.applied ? QStringLiteral("auto_model_domain")
                                      : QStringLiteral("roi_reduce_domain_fallback"));
}

void insertAutoModelDomainPayload(QJsonObject &payload,
                                  const CachedShapeModel &model)
{
    payload.insert(QStringLiteral("autoModelDomainApplied"), model.autoModelDomainApplied);
    payload.insert(QStringLiteral("autoModelDomainFallbackReason"), model.autoModelDomainFallbackReason);
    payload.insert(QStringLiteral("modelDomainSource"), model.modelDomainSource);
    payload.insert(QStringLiteral("autoModelDomainRoute"), model.autoModelDomainRoute);
    payload.insert(QStringLiteral("autoModelDomainCandidateCount"), model.candidateCount);
    payload.insert(QStringLiteral("autoModelDomainNoBorderCandidateCount"), model.noBorderCandidateCount);
    payload.insert(QStringLiteral("autoModelDomainTargetArea"), model.targetArea);
    payload.insert(QStringLiteral("autoModelDomainUserRoiArea"), model.userRoiArea);
    payload.insert(QStringLiteral("autoModelDomainTargetAreaRatio"), model.targetAreaRatio);
    payload.insert(QStringLiteral("autoModelDomainTargetRow"), model.displayOriginRow);
    payload.insert(QStringLiteral("autoModelDomainTargetColumn"), model.displayOriginColumn);
    payload.insert(QStringLiteral("autoModelDomainThresholdTriedValues"),
                   intsToJson(model.thresholdTriedValues));
    payload.insert(QStringLiteral("thresholdTriedValues"),
                   intsToJson(model.thresholdTriedValues));
    payload.insert(QStringLiteral("autoModelDomainSelectedThresholdHigh"),
                   model.selectedThresholdHigh);
    payload.insert(QStringLiteral("selectedThresholdHigh"), model.selectedThresholdHigh);
    payload.insert(QStringLiteral("autoModelDomainThresholdRegionArea"),
                   model.thresholdRegionArea);
    payload.insert(QStringLiteral("thresholdRegionArea"), model.thresholdRegionArea);
    payload.insert(QStringLiteral("autoModelDomainConnectedRegionCount"),
                   model.connectedRegionCount);
    payload.insert(QStringLiteral("connectedRegionCount"), model.connectedRegionCount);
    payload.insert(QStringLiteral("autoModelDomainRawCandidateCount"),
                   model.rawCandidateCount);
    payload.insert(QStringLiteral("rawCandidateCount"), model.rawCandidateCount);
    payload.insert(QStringLiteral("autoModelDomainAreaCandidateCount"),
                   model.areaCandidateCount);
    payload.insert(QStringLiteral("areaCandidateCount"), model.areaCandidateCount);
    payload.insert(QStringLiteral("autoModelDomainSelectedCandidateArea"),
                   model.selectedCandidateArea);
    payload.insert(QStringLiteral("selectedCandidateArea"), model.selectedCandidateArea);
    payload.insert(QStringLiteral("autoModelDomainSelectedCandidateIndex"),
                   model.selectedCandidateIndex);
    payload.insert(QStringLiteral("selectedCandidateIndex"), model.selectedCandidateIndex);
    payload.insert(QStringLiteral("autoModelDomainSelectedCandidateAreaRatio"),
                   model.selectedCandidateAreaRatio);
    payload.insert(QStringLiteral("selectedCandidateAreaRatio"),
                   model.selectedCandidateAreaRatio);
    payload.insert(QStringLiteral("autoModelDomainFailureStage"), model.failureStage);
    payload.insert(QStringLiteral("autoModelDomainFailureReason"), model.failureReason);
    payload.insert(QStringLiteral("failureStage"), model.failureStage);
    payload.insert(QStringLiteral("failureReason"), model.failureReason);
    payload.insert(QStringLiteral("autoModelDomainWarning"), model.warning);
    payload.insert(QStringLiteral("autoModelDomainPerThresholdStats"), model.perThresholdStats);
    payload.insert(QStringLiteral("perThresholdStats"), model.perThresholdStats);
}

} // namespace

PatternPresenceHalconResult PatternPresenceHalconRunner::run(
        const cv::Mat &image,
        const cv::Mat &referenceImage,
        const PatternPresenceHalconConfig &config)
{
    QElapsedTimer timer;
    timer.start();

    if (image.empty()) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("image_empty"),
                                                                QStringLiteral("input image is empty"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (referenceImage.empty()) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("reference_image_empty"),
                                                                QStringLiteral("reference image is empty"),
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
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_template_source"),
                                                                QStringLiteral("template source is not supported"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("todo"), QStringLiteral("template files and model paths are not implemented"));
        return result;
    }

    if (!isValidNormalizedRoi(config.templateRoiNormalized)) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_template_roi"),
                                                                QStringLiteral("template ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isValidNormalizedRoi(config.roiNormalized)) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                                QStringLiteral("detect ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isSupportedTemplateType(config.templateShapeType)) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_template_roi"),
                                                                QStringLiteral("template ROI shape is not supported"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("todo"), QStringLiteral("only rectangle and polygon template ROI are supported"));
        return result;
    }

    if (isPolygonTemplateType(config.templateShapeType) &&
        config.templatePolygonNormalized.size() < 3) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_template_polygon"),
                                                                QStringLiteral("多边形至少需要 3 个点"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    if (!isSupportedDetectRegionType(config.detectRegionType)) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_detect_roi"),
                                                                QStringLiteral("detect ROI shape is not supported"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("todo"), QStringLiteral("circle/polygon detect ROI is not implemented"));
        return result;
    }

    if (isPolygonDetectRegionType(config.detectRegionType) &&
        config.detectPolygonNormalized.size() < 3) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_polygon"),
                                                                QStringLiteral("polygon detect ROI requires at least 3 points"),
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
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("unsupported_image_type"),
                                                                QStringLiteral("PatternPresence supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
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
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("halcon_so_not_found"),
                                                                QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                                                                .arg(config.halconSoPath, triedPaths),
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
    if (templateRoiPixels.isEmpty()) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_template_roi"),
                                                                isPolygonTemplateType(config.templateShapeType)
                                                                ? QStringLiteral("模板区域过小")
                                                                : QStringLiteral("template ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    const QRect detectRoiPixels = normalizedRoiToPixels(config.roiNormalized,
                                                        image.cols,
                                                        image.rows);
    if (detectRoiPixels.isEmpty()) {
        PatternPresenceHalconResult result = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                                QStringLiteral("detect ROI is invalid"),
                                                                image,
                                                                referenceImage,
                                                                config);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    PatternPresenceHalconResult result;
    fillPayload(result, image, referenceImage, config);
    const bool polygonTemplate = isPolygonTemplateType(config.templateShapeType);
    const bool polygonDetectRoi = isPolygonDetectRegionType(config.detectRegionType);
    const QVector<QPointF> templatePolygonPixels = polygonTemplate
            ? normalizedPolygonToPixels(config.templatePolygonNormalized,
                                        referenceImage.cols,
                                        referenceImage.rows)
            : QVector<QPointF>();
    const QVector<QPointF> detectPolygonPixels = polygonDetectRoi
            ? normalizedPolygonToPixels(config.detectPolygonNormalized,
                                        image.cols,
                                        image.rows)
            : QVector<QPointF>();

    result.payload.insert(QStringLiteral("templateRoi"), rectToJson(QRectF(templateRoiPixels)));
    result.payload.insert(QStringLiteral("templateRoiPixels"), rectToJson(QRectF(templateRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoi"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("detectRoiPixels"), rectToJson(QRectF(detectRoiPixels)));
    result.payload.insert(QStringLiteral("shapeModelOperator"), QStringLiteral("create_scaled_shape_model/find_scaled_shape_model"));
    result.payload.insert(QStringLiteral("shapeModelOrigin"), QStringLiteral("halcon_default"));
    result.payload.insert(QStringLiteral("roiMode"),
                          polygonTemplate ? QStringLiteral("polygon_reduce_domain")
                                          : QStringLiteral("rectangle"));
    result.payload.insert(QStringLiteral("detectRoiCoordinates"), QStringLiteral("original_image_pixels"));
    result.payload.insert(QStringLiteral("templateRoiCoordinates"), QStringLiteral("reference_image_pixels"));
    result.payload.insert(QStringLiteral("templatePolygonPixels"), pointsToJson(templatePolygonPixels));
    result.payload.insert(QStringLiteral("templatePolygonGlobalPoints"), pointsToJson(templatePolygonPixels));
    result.payload.insert(QStringLiteral("templatePolygonLocalPoints"), QJsonArray());
    result.payload.insert(QStringLiteral("templateBoundingRectPixel"), rectToJson(QRectF(templateRoiPixels)));
    result.payload.insert(QStringLiteral("detectPolygonPixels"), pointsToJson(detectPolygonPixels));
    result.payload.insert(QStringLiteral("templateRoiPixelX"), templateRoiPixels.x());
    result.payload.insert(QStringLiteral("templateRoiPixelY"), templateRoiPixels.y());
    result.payload.insert(QStringLiteral("templateRoiPixelW"), templateRoiPixels.width());
    result.payload.insert(QStringLiteral("templateRoiPixelH"), templateRoiPixels.height());
    result.payload.insert(QStringLiteral("detectRoiPixelX"), detectRoiPixels.x());
    result.payload.insert(QStringLiteral("detectRoiPixelY"), detectRoiPixels.y());
    result.payload.insert(QStringLiteral("detectRoiPixelW"), detectRoiPixels.width());
    result.payload.insert(QStringLiteral("detectRoiPixelH"), detectRoiPixels.height());
    const bool correctionApplied = config.positionCorrection.applied;
    ToolOverlay detectOverlay = polygonDetectRoi && detectPolygonPixels.size() >= 3
            ? polygonOverlay(detectPolygonPixels, QStringLiteral("detect_roi"))
            : rectOverlay(QRectF(detectRoiPixels), QStringLiteral("detect_roi"));
    if (correctionApplied) {
        detectOverlay = PositionCorrectionTransform::transformOverlay(
                    detectOverlay,
                    config.positionCorrection.referenceToRunHomMat2D);
        detectOverlay.extra.insert(QStringLiteral("positionCorrectionSourceId"),
                                   config.positionCorrection.sourceId);
    }
    detectOverlay.extra.insert(QStringLiteral("role"), QStringLiteral("detect_roi"));
    result.overlays.append(detectOverlay);
    if (correctionApplied && config.positionCorrection.showMatchContour) {
        result.overlays += PositionCorrectionTransform::matchContourOverlays(
                    config.positionCorrection.matchContours,
                    config.positionCorrection.sourceId);
    }
    if (correctionApplied) {
        result.overlays += PositionCorrectionTransform::matchOriginOverlays(
                    config.positionCorrection.matchOrigins,
                    config.positionCorrection.sourceId);
    }

    cv::Mat templateMat = referenceImage(cv::Rect(templateRoiPixels.x(),
                                                  templateRoiPixels.y(),
                                                  templateRoiPixels.width(),
                                                  templateRoiPixels.height())).clone();
    cv::Mat detectMat = correctionApplied
            ? image.clone()
            : image(cv::Rect(detectRoiPixels.x(),
                             detectRoiPixels.y(),
                             detectRoiPixels.width(),
                             detectRoiPixels.height())).clone();
    if (templateMat.empty()) {
        PatternPresenceHalconResult errorResult = makeParameterError(QStringLiteral("invalid_template_roi"),
                                                                     QStringLiteral("template ROI is invalid"),
                                                                     image,
                                                                     referenceImage,
                                                                     config);
        errorResult.elapsedMs = timer.elapsed();
        errorResult.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(errorResult.elapsedMs));
        return errorResult;
    }
    if (detectMat.empty()) {
        PatternPresenceHalconResult errorResult = makeParameterError(QStringLiteral("invalid_detect_roi"),
                                                                     QStringLiteral("detect ROI is invalid"),
                                                                     image,
                                                                     referenceImage,
                                                                     config);
        errorResult.elapsedMs = timer.elapsed();
        errorResult.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(errorResult.elapsedMs));
        return errorResult;
    }
    if (!templateMat.isContinuous())
        templateMat = templateMat.clone();
    if (!detectMat.isContinuous())
        detectMat = detectMat.clone();

    QString contourDebugDir;
    QString templateCropDebugPath;
    QString templateGrayDebugPath;
    QString autoDomainThresholdMaskDebugPath;
    QString autoDomainConnectedCandidatesDebugPath;
    QString autoDomainSelectedCandidateDebugPath;
    QString autoDomainRegionDebugPath;
    QString autoDomainContourLocalDebugPath;
    QString matchAutoDomainContourGlobalDebugPath;
    QString debugInfoPath;
    cv::Mat templateGrayForDebug;
    const bool autoDomainDebugRequested = config.showContourPoints || config.debugPolygonLog;
    if (autoDomainDebugRequested) {
        contourDebugDir = createPatternContourDebugDir(config.toolId);
        templateCropDebugPath = QDir(contourDebugDir).filePath(QStringLiteral("template_crop.png"));
        templateGrayDebugPath = QDir(contourDebugDir).filePath(QStringLiteral("template_gray.png"));
        autoDomainThresholdMaskDebugPath = QDir(contourDebugDir).filePath(QStringLiteral("auto_domain_threshold_mask.png"));
        autoDomainConnectedCandidatesDebugPath = QDir(contourDebugDir).filePath(QStringLiteral("auto_domain_connected_candidates.png"));
        autoDomainSelectedCandidateDebugPath = QDir(contourDebugDir).filePath(QStringLiteral("auto_domain_selected_candidate.png"));
        autoDomainRegionDebugPath = QDir(contourDebugDir).filePath(QStringLiteral("auto_model_domain_region.png"));
        autoDomainContourLocalDebugPath = QDir(contourDebugDir).filePath(QStringLiteral("auto_model_domain_contour_local.png"));
        matchAutoDomainContourGlobalDebugPath = QDir(contourDebugDir).filePath(QStringLiteral("match_auto_model_domain_contour_global.png"));
        debugInfoPath = QDir(contourDebugDir).filePath(QStringLiteral("debug_info.json"));
        templateGrayForDebug = debugGrayMat(templateMat);
        result.payload.insert(QStringLiteral("contourDebugDir"), contourDebugDir);
        result.payload.insert(QStringLiteral("contourDebugTemplateCropPath"), templateCropDebugPath);
        result.payload.insert(QStringLiteral("contourDebugTemplateGrayPath"), templateGrayDebugPath);
        result.payload.insert(QStringLiteral("contourDebugAutoDomainThresholdMaskPath"), autoDomainThresholdMaskDebugPath);
        result.payload.insert(QStringLiteral("contourDebugAutoDomainConnectedCandidatesPath"), autoDomainConnectedCandidatesDebugPath);
        result.payload.insert(QStringLiteral("contourDebugAutoDomainSelectedCandidatePath"), autoDomainSelectedCandidateDebugPath);
        result.payload.insert(QStringLiteral("contourDebugAutoModelDomainRegionPath"), autoDomainRegionDebugPath);
        result.payload.insert(QStringLiteral("contourDebugAutoModelDomainContourLocalPath"), autoDomainContourLocalDebugPath);
        result.payload.insert(QStringLiteral("contourDebugMatchAutoModelDomainContourGlobalPath"), matchAutoDomainContourGlobalDebugPath);
        result.payload.insert(QStringLiteral("contourDebugInfoPath"), debugInfoPath);
        result.payload.insert(QStringLiteral("contourDebugTemplateCropSaved"),
                              saveDebugImage(templateCropDebugPath, templateMat));
        result.payload.insert(QStringLiteral("contourDebugTemplateGraySaved"),
                              saveDebugImage(templateGrayDebugPath, templateGrayForDebug));
    }

    const QVector<QPointF> templatePolygonLocalPixels = polygonTemplate
            ? polygonToLocalClamped(templatePolygonPixels,
                                    templateRoiPixels,
                                    templateMat.cols,
                                    templateMat.rows)
            : QVector<QPointF>();
    const double templatePolygonRegionArea = polygonTemplate
            ? polygonAreaPixels(templatePolygonLocalPixels)
            : 0.0;
    result.payload.insert(QStringLiteral("templatePolygonLocalPoints"),
                          pointsToJson(templatePolygonLocalPixels));
    result.payload.insert(QStringLiteral("templateMatWidth"), templateMat.cols);
    result.payload.insert(QStringLiteral("templateMatHeight"), templateMat.rows);
    result.payload.insert(QStringLiteral("detectMatWidth"), detectMat.cols);
    result.payload.insert(QStringLiteral("detectMatHeight"), detectMat.rows);
    result.payload.insert(QStringLiteral("polygonRegionArea"), templatePolygonRegionArea);
    if (!polygonTemplate) {
        const double rectangleDomainArea =
                static_cast<double>(templateMat.cols) * static_cast<double>(templateMat.rows);
        result.payload.insert(QStringLiteral("halconRegionArea"), rectangleDomainArea);
        result.payload.insert(QStringLiteral("reducedDomainArea"), rectangleDomainArea);
    }

    auto finishPolygonValidationError = [&](const QString &status,
                                            const QString &message,
                                            const QString &modelPointStatus) {
        result.success = false;
        result.ok = false;
        result.status = status;
        result.message = message;
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), message);
        result.payload.insert(QStringLiteral("modelPointStatus"), modelPointStatus);
        result.payload.insert(QStringLiteral("templatePolygonApplied"), false);
        result.payload.insert(QStringLiteral("polygonApplied"), false);
        result.payload.insert(QStringLiteral("polygonFallbackToBoundingRect"), false);
        result.payload.insert(QStringLiteral("reduceDomainApplied"), false);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        logPatternPolygonDebug(config, result.payload);
        logPatternPresenceRoiDebug(config, result.payload, result.status, result.message);
        return result;
    };

    if (polygonTemplate) {
        if (templatePolygonLocalPixels.size() < 3) {
            return finishPolygonValidationError(QStringLiteral("invalid_template_polygon"),
                                                QStringLiteral("多边形至少需要 3 个点"),
                                                QStringLiteral("empty_domain"));
        }
        if (templatePolygonRegionArea < kMinPolygonAreaPixels) {
            return finishPolygonValidationError(QStringLiteral("invalid_template_polygon"),
                                                QStringLiteral("多边形区域过小"),
                                                QStringLiteral("empty_domain"));
        }
        if (templateMat.cols < kMinRoiPixelSize || templateMat.rows < kMinRoiPixelSize) {
            return finishPolygonValidationError(QStringLiteral("template_region_too_small"),
                                                QStringLiteral("模板区域过小"),
                                                QStringLiteral("empty_domain"));
        }
        if (templatePolygonRegionArea < kMinReducedDomainAreaPixels) {
            return finishPolygonValidationError(QStringLiteral("template model creation failed"),
                                                QStringLiteral("模板区域有效特征过少，无法创建模型"),
                                                QStringLiteral("too_small"));
        }
    }

    const quint64 templateHash = matContentHash(templateMat);
    const QString effectiveModelCacheKey = buildShapeModelCacheKey(config,
                                                                    referenceImage,
                                                                    templateMat,
                                                                    templateHash);
    result.payload.insert(QStringLiteral("effectiveModelCacheKey"), effectiveModelCacheKey);
    result.payload.insert(QStringLiteral("templateHash"), QString::number(templateHash, 16));

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
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }

    PatternPresenceHalconApi *api = &library->api;
    result.payload.insert(QStringLiteral("shapeModelContourDebugAvailable"),
                          api->getShapeModelContours != nullptr);

    GrayHalconImage templateImage;
    GrayHalconImage detectImage;
    Hobject templatePolygonRegion = NO_OBJECTS;
    Hobject templateReducedImage = NO_OBJECTS;
    Hobject detectPolygonRegion = NO_OBJECTS;
    Hobject detectReferenceRegion = NO_OBJECTS;
    Hobject transformedDetectRegion = NO_OBJECTS;
    Hobject clippedDetectRegion = NO_OBJECTS;
    Hobject detectReducedImage = NO_OBJECTS;
    Hobject transformedDisplayContour = NO_OBJECTS;
    Htuple createNumLevelsTuple = HTUPLE_INITIALIZER;
    Htuple templatePolygonRowsTuple = HTUPLE_INITIALIZER;
    Htuple templatePolygonColumnsTuple = HTUPLE_INITIALIZER;
    Htuple detectPolygonRowsTuple = HTUPLE_INITIALIZER;
    Htuple detectPolygonColumnsTuple = HTUPLE_INITIALIZER;
    Htuple angleStartTuple = HTUPLE_INITIALIZER;
    Htuple angleExtentTuple = HTUPLE_INITIALIZER;
    Htuple angleStepTuple = HTUPLE_INITIALIZER;
    Htuple scaleMinTuple = HTUPLE_INITIALIZER;
    Htuple scaleMaxTuple = HTUPLE_INITIALIZER;
    Htuple scaleStepTuple = HTUPLE_INITIALIZER;
    Htuple optimizationTuple = HTUPLE_INITIALIZER;
    Htuple metricTuple = HTUPLE_INITIALIZER;
    Htuple contrastTuple = HTUPLE_INITIALIZER;
    Htuple minContrastTuple = HTUPLE_INITIALIZER;
    Htuple builtModelIdTuple = HTUPLE_INITIALIZER;
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
    Htuple paramsNumLevelsTuple = HTUPLE_INITIALIZER;
    Htuple paramsAngleStartTuple = HTUPLE_INITIALIZER;
    Htuple paramsAngleExtentTuple = HTUPLE_INITIALIZER;
    Htuple paramsAngleStepTuple = HTUPLE_INITIALIZER;
    Htuple paramsScaleMinTuple = HTUPLE_INITIALIZER;
    Htuple paramsScaleMaxTuple = HTUPLE_INITIALIZER;
    Htuple paramsScaleStepTuple = HTUPLE_INITIALIZER;
    Htuple paramsMetricTuple = HTUPLE_INITIALIZER;
    Htuple paramsMinContrastTuple = HTUPLE_INITIALIZER;
    QVector<Htuple *> createdTuples;
    QSharedPointer<CachedShapeModel> shapeModel;
    bool localModelCreated = false;
    bool modelCacheHit = false;
    bool outputTuplesCreated = false;
    qint64 modelBuildMs = 0;
    qint64 findMs = 0;
    qint64 convertMs = 0;
    QString rebuildReason = QStringLiteral("cache_miss");

    auto makeHalconFailure = [&](const Herror status, const QString &stage) {
        HalconFailure failure;
        failure.stage = stage;
        failure.code = status;
        failure.halconMessage = library->errorText(status);
        if ((stage == QStringLiteral("create_shape_model") ||
             stage == QStringLiteral("create_scaled_shape_model")) &&
            static_cast<int>(status) == 8510) {
            failure.status = QStringLiteral("template model creation failed");
            const QString autoDomainFailure =
                    result.payload.value(QStringLiteral("autoModelDomainFallbackReason")).toString().trimmed();
            const bool autoDomainApplied =
                    result.payload.value(QStringLiteral("autoModelDomainApplied")).toBool();
            failure.message = (!autoDomainApplied && !autoDomainFailure.isEmpty())
                    ? QStringLiteral("自动目标建模域失败：%1；fallback 旧 ROI 建模后模型点仍过少。")
                      .arg(autoDomainFailure)
                    : QStringLiteral("模板区域有效模型点过少，无法创建图案模型");
            failure.halconMessage = QStringLiteral("Number of shape model points too small");
            return failure;
        }
        failure.status = (stage == QStringLiteral("create_shape_model") ||
                          stage == QStringLiteral("create_scaled_shape_model"))
                ? QStringLiteral("template model creation failed")
                : QStringLiteral("PatternPresence HALCON error");
        failure.message = QStringLiteral("%1: %2").arg(stage, failure.halconMessage);
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

    auto validateModelHandle = [&](const Htuple &modelIdTuple) {
        if (modelIdTuple.num < 1) {
            HalconFailure failure;
            failure.status = QStringLiteral("template model creation failed");
            failure.message = QStringLiteral("create_shape_model: ModelID was not returned");
            failure.stage = QStringLiteral("create_shape_model");
            failure.halconMessage = failure.message;
            throw failure;
        }
        if (api->getHandle && api->getHandle(&modelIdTuple, 0) == HALCONC_HNULL) {
            HalconFailure failure;
            failure.status = QStringLiteral("template model creation failed");
            failure.message = QStringLiteral("create_shape_model: ModelID is invalid");
            failure.stage = QStringLiteral("create_shape_model");
            failure.halconMessage = failure.message;
            throw failure;
        }
    };

    auto generateGrayImage = [&](const cv::Mat &mat, GrayHalconImage &halconImage, const QString &stage) {
        const int channels = mat.channels();
        if (channels == 1) {
            checkStatus(api->genImage1(&halconImage.inputImage,
                                       "byte",
                                       mat.cols,
                                       mat.rows,
                                       reinterpret_cast<Hlong>(mat.data)),
                        stage + QStringLiteral(".gen_image1"));
            halconImage.graySource = halconImage.inputImage;
            return;
        }

        const char *colorFormat = channels == 4 ? "bgrx" : "bgr";
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
        if (localModelCreated && api->clearShapeModel) {
            api->clearShapeModel(builtModelIdTuple);
            localModelCreated = false;
        }
        destroyTuple(api, builtModelIdTuple);
        shapeModel.clear();

        if (outputTuplesCreated) {
            destroyTuple(api, scoreTuple);
            destroyTuple(api, scaleTuple);
            destroyTuple(api, angleTuple);
            destroyTuple(api, columnTuple);
            destroyTuple(api, rowTuple);
            outputTuplesCreated = false;
        }

        destroyTuple(api, displayScaledHomMatTuple);
        destroyTuple(api, displayHomMatTuple);
        for (Htuple *tuple : createdTuples) {
            if (tuple)
                destroyTuple(api, *tuple);
        }
        createdTuples.clear();

        clearObject(api, transformedDisplayContour);
        clearObject(api, detectReducedImage);
        clearObject(api, clippedDetectRegion);
        clearObject(api, transformedDetectRegion);
        clearObject(api, detectReferenceRegion);
        clearObject(api, detectPolygonRegion);
        clearObject(api, templateReducedImage);
        clearObject(api, templatePolygonRegion);
        clearObject(api, detectImage.grayImage);
        clearObject(api, detectImage.inputImage);
        clearObject(api, templateImage.grayImage);
        clearObject(api, templateImage.inputImage);
    };

    auto writeDebugInfo = [&]() {
        if (debugInfoPath.trimmed().isEmpty())
            return;

        const QStringList keys{
            QStringLiteral("contourDebugDir"),
            QStringLiteral("templateRoiPixels"),
            QStringLiteral("detectRoiPixels"),
            QStringLiteral("templateShapeType"),
            QStringLiteral("templatePolygonLocalPoints"),
            QStringLiteral("templatePolygonGlobalPoints"),
            QStringLiteral("templateBoundingRectPixel"),
            QStringLiteral("polygonRegionArea"),
            QStringLiteral("halconRegionArea"),
            QStringLiteral("reducedDomainArea"),
            QStringLiteral("found"),
            QStringLiteral("bestScore"),
            QStringLiteral("foundCount"),
            QStringLiteral("rawMatchRow"),
            QStringLiteral("rawMatchCol"),
            QStringLiteral("globalMatchRow"),
            QStringLiteral("globalMatchCol"),
            QStringLiteral("bestAngle"),
            QStringLiteral("usedMinScore"),
            QStringLiteral("usedMetric"),
            QStringLiteral("createShapeModelContrastUsed"),
            QStringLiteral("createShapeModelMinContrastUsed"),
            QStringLiteral("autoDomainContrastOverrideApplied"),
            QStringLiteral("autoModelDomainEnabled"),
            QStringLiteral("autoModelDomainApplied"),
            QStringLiteral("autoModelDomainRoute"),
            QStringLiteral("autoModelDomainThresholdHigh"),
            QStringLiteral("autoModelDomainThresholdTriedValues"),
            QStringLiteral("autoModelDomainSelectedThresholdHigh"),
            QStringLiteral("autoModelDomainMinAreaRatio"),
            QStringLiteral("autoModelDomainMaxAreaRatio"),
            QStringLiteral("autoModelDomainOpeningRadius"),
            QStringLiteral("autoModelDomainClosingRadius"),
            QStringLiteral("autoModelDomainDilationRadius"),
            QStringLiteral("autoModelDomainCandidateCount"),
            QStringLiteral("autoModelDomainNoBorderCandidateCount"),
            QStringLiteral("autoModelDomainThresholdRegionArea"),
            QStringLiteral("autoModelDomainConnectedRegionCount"),
            QStringLiteral("autoModelDomainRawCandidateCount"),
            QStringLiteral("autoModelDomainAreaCandidateCount"),
            QStringLiteral("autoModelDomainSelectedCandidateArea"),
            QStringLiteral("autoModelDomainSelectedCandidateIndex"),
            QStringLiteral("autoModelDomainSelectedCandidateAreaRatio"),
            QStringLiteral("autoModelDomainTargetArea"),
            QStringLiteral("autoModelDomainUserRoiArea"),
            QStringLiteral("autoModelDomainTargetAreaRatio"),
            QStringLiteral("autoModelDomainFallbackReason"),
            QStringLiteral("autoModelDomainFailureStage"),
            QStringLiteral("autoModelDomainFailureReason"),
            QStringLiteral("autoModelDomainWarning"),
            QStringLiteral("autoModelDomainPerThresholdStats"),
            QStringLiteral("thresholdTriedValues"),
            QStringLiteral("selectedThresholdHigh"),
            QStringLiteral("thresholdRegionArea"),
            QStringLiteral("connectedRegionCount"),
            QStringLiteral("rawCandidateCount"),
            QStringLiteral("areaCandidateCount"),
            QStringLiteral("selectedCandidateArea"),
            QStringLiteral("selectedCandidateIndex"),
            QStringLiteral("selectedCandidateAreaRatio"),
            QStringLiteral("failureStage"),
            QStringLiteral("failureReason"),
            QStringLiteral("perThresholdStats"),
            QStringLiteral("modelDomainSource"),
            QStringLiteral("contourDebugAutoDomainThresholdMaskPath"),
            QStringLiteral("contourDebugAutoDomainThresholdMaskSaved"),
            QStringLiteral("contourDebugAutoDomainConnectedCandidatesPath"),
            QStringLiteral("contourDebugAutoDomainConnectedCandidatesSaved"),
            QStringLiteral("contourDebugAutoDomainSelectedCandidatePath"),
            QStringLiteral("contourDebugAutoDomainSelectedCandidateSaved"),
            QStringLiteral("contourDebugAutoModelDomainRegionSaved"),
            QStringLiteral("contourDebugAutoModelDomainContourLocalSaved"),
            QStringLiteral("showContourPointsRequested"),
            QStringLiteral("showContourPointsApplied"),
            QStringLiteral("contourSource"),
            QStringLiteral("displayContourSource"),
            QStringLiteral("displayContourObjectCount"),
            QStringLiteral("displayContourPointCount"),
            QStringLiteral("contourOverlayReason"),
            QStringLiteral("modelCacheHit"),
            QStringLiteral("modelBuildMs"),
            QStringLiteral("findMs"),
            QStringLiteral("elapsedMs")
        };

        QJsonObject debug;
        debug.insert(QStringLiteral("debugDir"), contourDebugDir);
        debug.insert(QStringLiteral("template_crop"), templateCropDebugPath);
        debug.insert(QStringLiteral("template_gray"), templateGrayDebugPath);
        debug.insert(QStringLiteral("auto_domain_threshold_mask"), autoDomainThresholdMaskDebugPath);
        debug.insert(QStringLiteral("auto_domain_connected_candidates"),
                     autoDomainConnectedCandidatesDebugPath);
        debug.insert(QStringLiteral("auto_domain_selected_candidate"),
                     autoDomainSelectedCandidateDebugPath);
        debug.insert(QStringLiteral("auto_model_domain_region"), autoDomainRegionDebugPath);
        debug.insert(QStringLiteral("auto_model_domain_contour_local"), autoDomainContourLocalDebugPath);
        debug.insert(QStringLiteral("match_auto_model_domain_contour_global"),
                     matchAutoDomainContourGlobalDebugPath);
        for (const QString &key : keys)
            debug.insert(key, result.payload.value(key));
        result.payload.insert(QStringLiteral("contourDebugInfoSaved"),
                              saveDebugJson(debugInfoPath, debug));
    };

    try {
        const double angleStartRad = static_cast<double>(config.angleStart) * kPi / 180.0;
        const double angleExtentRad = qBound(0.0,
                                             static_cast<double>(config.angleExtent) * kPi / 180.0,
                                             2.0 * kPi);
        const ScaleRangeSettings scaleSettings = scaleRangeSettings(config);
        const double minScore = normalizedScore(config.minScore);
        const int numMatches = qMax(1, config.maxCount);
        bool polarityFallback = false;
        const QString metric = halconMetricForPolarity(config.polarity, &polarityFallback);
        const ShapeModelContrastSettings contrastSettings = shapeModelContrastSettings(config);

        if (config.numLevels > 0)
            createIntTuple(createNumLevelsTuple, static_cast<Hlong>(qBound(1, config.numLevels, 10)));
        else
            createStringTuple(createNumLevelsTuple, QByteArray("auto"));
        createDoubleTuple(angleStartTuple, angleStartRad);
        createDoubleTuple(angleExtentTuple, angleExtentRad);
        createStringTuple(angleStepTuple, QByteArray("auto"));
        createDoubleTuple(scaleMinTuple, scaleSettings.minScale);
        createDoubleTuple(scaleMaxTuple, scaleSettings.maxScale);
        createStringTuple(scaleStepTuple, QByteArray("auto"));
        createStringTuple(optimizationTuple, QByteArray("auto"));
        createStringTuple(metricTuple, metric.toLatin1());
        const QString contrastMode = config.contrastMode.trimmed().toLower();
        if (contrastMode == QStringLiteral("auto")) {
            createStringTuple(contrastTuple, QByteArray("auto"));
            createStringTuple(minContrastTuple, QByteArray("auto"));
        } else if (contrastMode == QStringLiteral("manual")) {
            createIntTuple(contrastTuple, static_cast<Hlong>(qBound(2, config.contrast, 255)));
            createIntTuple(minContrastTuple,
                           static_cast<Hlong>(qBound(1, config.minContrast,
                                                     qMax(1, qBound(2, config.contrast, 255) - 1))));
        } else {
            createIntTuple(contrastTuple, static_cast<Hlong>(contrastSettings.contrast));
            createIntTuple(minContrastTuple, static_cast<Hlong>(contrastSettings.minContrast));
        }
        createStringTuple(timeoutParamNameTuple, QByteArray("timeout"));
        createIntTuple(timeoutValueTuple, static_cast<Hlong>(qMax(0, config.timeoutMs)));
        createIntTuple(contourLevelTuple, 1);
        createDoubleTuple(minScoreTuple, minScore);
        createIntTuple(numMatchesTuple, static_cast<Hlong>(numMatches));
        createDoubleTuple(maxOverlapTuple, 0.5);
        createStringTuple(subPixelTuple,
                          config.subPixel.trimmed().toLower() == QStringLiteral("none")
                                  ? QByteArray("none") : QByteArray("least_squares"));
        createIntTuple(findNumLevelsTuple,
                       static_cast<Hlong>(config.numLevels > 0 ? qBound(1, config.numLevels, 10) : 0));
        createDoubleTuple(greedinessTuple, qBound(0.0, config.greediness, 1.0));

        result.payload.insert(QStringLiteral("scaleRangeFallback"), scaleSettings.fallback);
        result.payload.insert(QStringLiteral("scaleRangeFallbackReason"), scaleSettings.fallbackReason);
        result.payload.insert(QStringLiteral("scaleMinUsed"), scaleSettings.minScale);
        result.payload.insert(QStringLiteral("scaleMaxUsed"), scaleSettings.maxScale);
        result.payload.insert(QStringLiteral("angleStartDegUsed"), config.angleStart);
        result.payload.insert(QStringLiteral("angleExtentDegUsed"), config.angleExtent);
        result.payload.insert(QStringLiteral("searchMinScoreUsed"), minScore);
        result.payload.insert(QStringLiteral("judgeScoreThresholdUsed"), normalizedScore(config.scoreThreshold));
        result.payload.insert(QStringLiteral("metricUsed"), metric);
        result.payload.insert(QStringLiteral("polarityFallback"), polarityFallback);
        result.payload.insert(QStringLiteral("contrastMode"), contrastMode);

        {
            QMutexLocker cacheLocker(&shapeModelCacheMutex());
            auto cachedIt = shapeModelCache().constFind(effectiveModelCacheKey);
            if (cachedIt != shapeModelCache().constEnd()) {
                shapeModel = cachedIt.value();
                modelCacheHit = true;
                rebuildReason = QStringLiteral("cache_key_match");
            }
        }

        if (!shapeModel) {
            rebuildReason = QStringLiteral("cache_miss");

            QElapsedTimer convertTimer;
            convertTimer.start();
            generateGrayImage(templateMat, templateImage, QStringLiteral("template"));
            convertMs += convertTimer.elapsed();

            Hobject templateModelSource = templateImage.graySource;
            bool templatePolygonApplied = false;
            if (polygonTemplate) {
                if (!api->genRegionPolygon || !api->reduceDomain) {
                    result.payload.insert(QStringLiteral("templatePolygonApplyError"),
                                          QStringLiteral("HALCON gen_region_polygon/reduce_domain symbol unavailable"));
                    HalconFailure failure;
                    failure.status = QStringLiteral("template model creation failed");
                    failure.message = QStringLiteral("HALCON gen_region_polygon/reduce_domain 不可用，无法应用多边形模板 ROI");
                    failure.stage = QStringLiteral("gen_region_polygon.template_polygon_roi");
                    failure.halconMessage = failure.message;
                    throw failure;
                }

                QVector<double> rows;
                QVector<double> columns;
                rows.reserve(templatePolygonLocalPixels.size());
                columns.reserve(templatePolygonLocalPixels.size());
                for (const QPointF &point : templatePolygonLocalPixels) {
                    columns.append(point.x());
                    rows.append(point.y());
                }

                createDoubleArrayTuple(templatePolygonRowsTuple, rows);
                createDoubleArrayTuple(templatePolygonColumnsTuple, columns);
                checkStatus(api->genRegionPolygon(&templatePolygonRegion,
                                                  templatePolygonRowsTuple,
                                                  templatePolygonColumnsTuple),
                            QStringLiteral("gen_region_polygon.template_polygon_roi"));
                const double templatePolygonHalconArea =
                        halconRegionArea(api, templatePolygonRegion);
                result.payload.insert(QStringLiteral("halconRegionArea"),
                                      templatePolygonHalconArea);
                result.payload.insert(QStringLiteral("templatePolygonHalconRegionArea"),
                                      templatePolygonHalconArea);
                checkStatus(api->reduceDomain(templateImage.graySource,
                                              templatePolygonRegion,
                                              &templateReducedImage),
                            QStringLiteral("reduce_domain.template_polygon_roi"));
                result.payload.insert(QStringLiteral("reducedDomainArea"),
                                      templatePolygonHalconArea);
                templateModelSource = templateReducedImage;
                templatePolygonApplied = true;
                result.payload.insert(QStringLiteral("templatePolygonApplied"), true);
                result.payload.insert(QStringLiteral("polygonApplied"), true);
                result.payload.insert(QStringLiteral("templateMaskApplied"), true);
                result.payload.insert(QStringLiteral("reduceDomainApplied"), true);
                result.payload.insert(QStringLiteral("polygonFallbackToBoundingRect"), false);
                result.payload.insert(QStringLiteral("roiMode"), QStringLiteral("polygon_reduce_domain"));
            } else {
                result.payload.insert(QStringLiteral("templatePolygonApplied"), false);
                result.payload.insert(QStringLiteral("polygonApplied"), false);
                result.payload.insert(QStringLiteral("polygonFallbackToBoundingRect"), false);
                result.payload.insert(QStringLiteral("roiMode"), QStringLiteral("rectangle"));
            }

            PatternPresenceAutoModelDomainResult autoDomain;
            PatternPresenceAutoModelDomainParams autoDomainParams =
                    defaultPatternPresenceAutoModelDomainParams();
            autoDomainParams.polygonTemplate = polygonTemplate;
            const double userRoiArea = polygonTemplate
                    ? templatePolygonRegionArea
                    : static_cast<double>(templateMat.cols) * static_cast<double>(templateMat.rows);
            tryCreatePatternPresenceAutoModelDomain(api,
                                                    templateImage.graySource,
                                                    templateModelSource,
                                                    userRoiArea,
                                                    autoDomainParams,
                                                    &autoDomain);
            insertAutoModelDomainResultPayload(result.payload, autoDomain);
            if (!autoDomainThresholdMaskDebugPath.isEmpty()) {
                saveAutoDomainHalconRegionDebugImages(api,
                                                      result.payload,
                                                      templateMat,
                                                      autoDomain.debugThresholdRegion,
                                                      autoDomain.debugConnectedRegions,
                                                      autoDomain.debugSelectedCandidateRegion,
                                                      autoDomainThresholdMaskDebugPath,
                                                      autoDomainConnectedCandidatesDebugPath,
                                                      autoDomainSelectedCandidateDebugPath);
                saveAutoDomainRegionDebugImages(api,
                                                result.payload,
                                                templateMat,
                                                autoDomain.displayContour,
                                                autoDomainRegionDebugPath,
                                                autoDomainContourLocalDebugPath);
            }

            Hobject modelSourceForCreate = autoDomain.applied
                    ? autoDomain.reducedImageForModel
                    : templateModelSource;
            bool autoDomainUsedForCreate = autoDomain.applied;

            QElapsedTimer buildTimer;
            buildTimer.start();
            Herror createStatus = api->createScaledShapeModel(modelSourceForCreate,
                                                              createNumLevelsTuple,
                                                              angleStartTuple,
                                                              angleExtentTuple,
                                                              angleStepTuple,
                                                              scaleMinTuple,
                                                              scaleMaxTuple,
                                                              scaleStepTuple,
                                                              optimizationTuple,
                                                              metricTuple,
                                                              contrastTuple,
                                                              minContrastTuple,
                                                              &builtModelIdTuple);
            modelBuildMs = buildTimer.elapsed();
            if (!patternPresenceHalconStatusOk(createStatus) && autoDomainUsedForCreate) {
                destroyTuple(api, builtModelIdTuple);
                result.payload.insert(QStringLiteral("autoModelDomainApplied"), false);
                result.payload.insert(QStringLiteral("autoModelDomainFallbackReason"),
                                      QStringLiteral("create_scaled_shape_model_failed"));
                result.payload.insert(QStringLiteral("autoModelDomainFailureStage"),
                                      QStringLiteral("create_scaled_shape_model"));
                result.payload.insert(QStringLiteral("autoModelDomainFailureReason"),
                                      QStringLiteral("create_scaled_shape_model_failed"));
                result.payload.insert(QStringLiteral("failureStage"),
                                      QStringLiteral("create_scaled_shape_model"));
                result.payload.insert(QStringLiteral("failureReason"),
                                      QStringLiteral("create_scaled_shape_model_failed"));
                result.payload.insert(QStringLiteral("modelDomainSource"),
                                      QStringLiteral("roi_reduce_domain_fallback"));
                autoDomainUsedForCreate = false;
                modelSourceForCreate = templateModelSource;

                buildTimer.restart();
                createStatus = api->createScaledShapeModel(modelSourceForCreate,
                                                           createNumLevelsTuple,
                                                           angleStartTuple,
                                                           angleExtentTuple,
                                                           angleStepTuple,
                                                           scaleMinTuple,
                                                           scaleMaxTuple,
                                                           scaleStepTuple,
                                                           optimizationTuple,
                                                           metricTuple,
                                                           contrastTuple,
                                                           minContrastTuple,
                                                           &builtModelIdTuple);
                modelBuildMs += buildTimer.elapsed();
            }
            checkStatus(createStatus, QStringLiteral("create_scaled_shape_model"));
            localModelCreated = true;
            validateModelHandle(builtModelIdTuple);
            if (config.timeoutMs > 0 && api->setShapeModelParam) {
                checkStatus(api->setShapeModelParam(builtModelIdTuple,
                                                    timeoutParamNameTuple,
                                                    timeoutValueTuple),
                            QStringLiteral("set_shape_model_param.timeout"));
            }
            result.payload.insert(QStringLiteral("modelPointStatus"), QStringLiteral("ok"));

            QSharedPointer<CachedShapeModel> createdModel =
                    QSharedPointer<CachedShapeModel>::create(effectiveModelCacheKey,
                                                             library,
                                                             builtModelIdTuple);
            builtModelIdTuple = HTUPLE_INITIALIZER;
            localModelCreated = false;
            createdModel->templatePolygonApplied = templatePolygonApplied;
            createdModel->autoModelDomainApplied = autoDomainUsedForCreate;
            createdModel->autoModelDomainFallbackReason =
                    autoDomainUsedForCreate ? QString() : result.payload.value(QStringLiteral("autoModelDomainFallbackReason")).toString();
            createdModel->modelDomainSource = autoDomainUsedForCreate
                    ? QStringLiteral("auto_model_domain")
                    : QStringLiteral("roi_reduce_domain_fallback");
            createdModel->autoModelDomainRoute = autoDomainParams.version;
            createdModel->candidateCount = autoDomain.candidateCount;
            createdModel->noBorderCandidateCount = autoDomain.noBorderCandidateCount;
            createdModel->targetArea = autoDomain.targetArea;
            createdModel->userRoiArea = autoDomain.userRoiArea;
            createdModel->targetAreaRatio = autoDomain.targetAreaRatio;
            createdModel->displayOriginRow = autoDomain.targetRow;
            createdModel->displayOriginColumn = autoDomain.targetColumn;
            createdModel->thresholdTriedValues = autoDomain.thresholdTriedValues;
            createdModel->perThresholdStats =
                    result.payload.value(QStringLiteral("autoModelDomainPerThresholdStats")).toArray();
            createdModel->selectedThresholdHigh = autoDomain.selectedThresholdHigh;
            createdModel->thresholdRegionArea = autoDomain.thresholdRegionArea;
            createdModel->connectedRegionCount = autoDomain.connectedRegionCount;
            createdModel->rawCandidateCount = autoDomain.rawCandidateCount;
            createdModel->areaCandidateCount = autoDomain.areaCandidateCount;
            createdModel->selectedCandidateArea = autoDomain.selectedCandidateArea;
            createdModel->selectedCandidateIndex = autoDomain.selectedCandidateIndex;
            createdModel->selectedCandidateAreaRatio = autoDomain.selectedCandidateAreaRatio;
            createdModel->failureStage =
                    result.payload.value(QStringLiteral("autoModelDomainFailureStage")).toString();
            createdModel->failureReason =
                    result.payload.value(QStringLiteral("autoModelDomainFailureReason")).toString();
            createdModel->warning = autoDomain.warning;
            if (api->getShapeModelContours) {
                Herror contourStatus = api->getShapeModelContours(&createdModel->modelContour,
                                                                   createdModel->modelIdTuple,
                                                                   contourLevelTuple);
                if (patternPresenceHalconStatusOk(contourStatus) &&
                    patternPresenceHalconObjectAllocated(createdModel->modelContour)) {
                    const XldContourReadResult modelContourRead =
                            readXldContours(api, createdModel->modelContour);
                    result.payload.insert(QStringLiteral("shapeModelContourDebugAvailable"), true);
                    result.payload.insert(QStringLiteral("shapeModelContourObjectCount"),
                                          modelContourRead.objectCount);
                    result.payload.insert(QStringLiteral("shapeModelContourPointCount"),
                                          modelContourRead.pointCount);
                    result.payload.insert(QStringLiteral("modelContourSource"),
                                          QStringLiteral("get_shape_model_contours"));
                } else {
                    clearObject(api, createdModel->modelContour);
                    result.payload.insert(QStringLiteral("shapeModelContourDebugAvailable"), false);
                    result.payload.insert(QStringLiteral("modelContourError"),
                                          library->errorText(contourStatus));
                }
            }
            if (autoDomainUsedForCreate &&
                patternPresenceHalconObjectAllocated(autoDomain.displayContour)) {
                createdModel->displayContour = autoDomain.displayContour;
                autoDomain.displayContour = NO_OBJECTS;
            }
            clearPatternPresenceAutoModelDomainResult(api, &autoDomain);

            {
                QMutexLocker cacheLocker(&shapeModelCacheMutex());
                auto cachedIt = shapeModelCache().constFind(effectiveModelCacheKey);
                if (cachedIt != shapeModelCache().constEnd()) {
                    shapeModel = cachedIt.value();
                    modelCacheHit = true;
                    rebuildReason = QStringLiteral("cache_key_match_after_create");
                } else {
                    shapeModel = createdModel;
                    shapeModelCache().insert(effectiveModelCacheKey, shapeModel);
                }
            }
        }

        if (api->getShapeModelParams) {
            QMutexLocker modelLocker(&shapeModel->mutex);
            const Herror paramsStatus = api->getShapeModelParams(shapeModel->modelIdTuple,
                                                                 &paramsNumLevelsTuple,
                                                                 &paramsAngleStartTuple,
                                                                 &paramsAngleExtentTuple,
                                                                 &paramsAngleStepTuple,
                                                                 &paramsScaleMinTuple,
                                                                 &paramsScaleMaxTuple,
                                                                 &paramsScaleStepTuple,
                                                                 &paramsMetricTuple,
                                                                 &paramsMinContrastTuple);
            trackTuple(paramsNumLevelsTuple);
            trackTuple(paramsAngleStartTuple);
            trackTuple(paramsAngleExtentTuple);
            trackTuple(paramsAngleStepTuple);
            trackTuple(paramsScaleMinTuple);
            trackTuple(paramsScaleMaxTuple);
            trackTuple(paramsScaleStepTuple);
            trackTuple(paramsMetricTuple);
            trackTuple(paramsMinContrastTuple);
            if (patternPresenceHalconStatusOk(paramsStatus)) {
                if (paramsNumLevelsTuple.num > 0)
                    result.payload.insert(QStringLiteral("numLevelsUsed"),
                                          api->getDouble(&paramsNumLevelsTuple, 0));
                if (paramsMinContrastTuple.num > 0)
                    result.payload.insert(QStringLiteral("minContrastUsed"),
                                          api->getDouble(&paramsMinContrastTuple, 0));
            } else {
                result.payload.insert(QStringLiteral("shapeModelParamsWarning"),
                                      library->errorText(paramsStatus));
            }
        }
        result.payload.insert(QStringLiteral("contrastUsed"),
                              contrastMode == QStringLiteral("manual")
                                      ? QJsonValue(qBound(2, config.contrast, 255))
                                      : QJsonValue(QStringLiteral("auto")));

        if (!shapeModel) {
            HalconFailure failure;
            failure.status = QStringLiteral("PatternPresence HALCON error");
            failure.message = QStringLiteral("shape model cache did not return a model");
            failure.stage = QStringLiteral("shape_model_cache");
            failure.halconMessage = failure.message;
            throw failure;
        }

        {
            QMutexLocker modelLocker(&shapeModel->mutex);
            result.payload.insert(QStringLiteral("templatePolygonApplied"), shapeModel->templatePolygonApplied);
            result.payload.insert(QStringLiteral("polygonApplied"), shapeModel->templatePolygonApplied);
            result.payload.insert(QStringLiteral("templateMaskApplied"), shapeModel->templatePolygonApplied);
            result.payload.insert(QStringLiteral("reduceDomainApplied"), shapeModel->templatePolygonApplied);
            result.payload.insert(QStringLiteral("polygonFallbackToBoundingRect"),
                                  polygonTemplate && !shapeModel->templatePolygonApplied);
            result.payload.insert(QStringLiteral("roiMode"),
                                  shapeModel->templatePolygonApplied
                                          ? QStringLiteral("polygon_reduce_domain")
                                          : (polygonTemplate ? QStringLiteral("polygon_bounding_rect")
                                                             : QStringLiteral("rectangle")));
            insertAutoModelDomainPayload(result.payload, *shapeModel);
            if (patternPresenceHalconObjectAllocated(shapeModel->modelContour)) {
                const XldContourReadResult modelContourRead =
                        readXldContours(api, shapeModel->modelContour);
                result.payload.insert(QStringLiteral("shapeModelContourDebugAvailable"), true);
                result.payload.insert(QStringLiteral("shapeModelContourObjectCount"),
                                      modelContourRead.objectCount);
                result.payload.insert(QStringLiteral("shapeModelContourPointCount"),
                                      modelContourRead.pointCount);
                result.payload.insert(QStringLiteral("modelContourSource"),
                                      QStringLiteral("get_shape_model_contours"));
            }
            if (!autoDomainThresholdMaskDebugPath.isEmpty() && modelCacheHit) {
                saveAutoDomainMaskDebugImages(result.payload,
                                             templateMat,
                                             templateGrayForDebug,
                                             polygonTemplate,
                                             templatePolygonLocalPixels,
                                             result.payload.value(QStringLiteral("autoModelDomainSelectedThresholdHigh")).toInt(),
                                             result.payload.value(QStringLiteral("autoModelDomainSelectedCandidateArea")).toDouble(),
                                             autoDomainThresholdMaskDebugPath,
                                             autoDomainConnectedCandidatesDebugPath,
                                             autoDomainSelectedCandidateDebugPath);
                saveAutoDomainRegionDebugImages(api,
                                                result.payload,
                                                templateMat,
                                                shapeModel->displayContour,
                                                autoDomainRegionDebugPath,
                                                autoDomainContourLocalDebugPath);
            }
            if (result.payload.value(QStringLiteral("modelPointStatus")).toString() == QStringLiteral("not_evaluated"))
                result.payload.insert(QStringLiteral("modelPointStatus"), QStringLiteral("ok"));
        }

        {
            QElapsedTimer convertTimer;
            convertTimer.start();
            generateGrayImage(detectMat, detectImage, QStringLiteral("detect"));
            convertMs += convertTimer.elapsed();
        }

        Hobject detectSearchSource = detectImage.graySource;
        if (correctionApplied) {
            if (!api->reduceDomain || !api->genRectangle1
                    || !api->affineTransRegion || !api->clipRegion
                    || !api->areaCenter || !api->setString) {
                HalconFailure failure;
                failure.status = QStringLiteral("halcon_symbol_missing");
                failure.message = QStringLiteral("HALCON position-correction ROI symbols are unavailable");
                failure.stage = QStringLiteral("position_correction");
                failure.halconMessage = failure.message;
                throw failure;
            }
            if (polygonDetectRoi) {
                if (!api->genRegionPolygon) {
                    HalconFailure failure;
                    failure.status = QStringLiteral("halcon_symbol_missing");
                    failure.message = QStringLiteral("HALCON polygon ROI symbol is unavailable");
                    failure.stage = QStringLiteral("position_correction.reference_roi");
                    failure.halconMessage = failure.message;
                    throw failure;
                }
                QVector<double> rows;
                QVector<double> columns;
                rows.reserve(detectPolygonPixels.size());
                columns.reserve(detectPolygonPixels.size());
                for (const QPointF &point : detectPolygonPixels) {
                    rows.append(point.y());
                    columns.append(point.x());
                }
                createDoubleArrayTuple(detectPolygonRowsTuple, rows);
                createDoubleArrayTuple(detectPolygonColumnsTuple, columns);
                checkStatus(api->genRegionPolygon(&detectReferenceRegion,
                                                  detectPolygonRowsTuple,
                                                  detectPolygonColumnsTuple),
                            QStringLiteral("gen_region_polygon.reference_detect_roi"));
            } else {
                checkStatus(api->genRectangle1(&detectReferenceRegion,
                                               detectRoiPixels.top(),
                                               detectRoiPixels.left(),
                                               detectRoiPixels.bottom(),
                                               detectRoiPixels.right()),
                            QStringLiteral("gen_rectangle1.reference_detect_roi"));
            }
            const PositionCorrectionHalconTransformResult transformed =
                    PositionCorrectionHalconTransform::transformAndClipRegion(
                        positionCorrectionRegionApi(*api),
                        detectReferenceRegion,
                        &transformedDetectRegion,
                        &clippedDetectRegion,
                        config.positionCorrection.referenceToRunHomMat2D,
                        image.cols,
                        image.rows);
            if (!transformed.success || transformed.area <= 0.0) {
                HalconFailure failure;
                failure.status = !transformed.success
                        ? transformed.status
                        : QStringLiteral("corrected_roi_out_of_image");
                failure.message = QStringLiteral("Position-corrected Pattern ROI is invalid (%1)")
                        .arg(transformed.operation);
                failure.stage = QStringLiteral("position_correction.%1")
                        .arg(transformed.operation);
                failure.code = transformed.halconStatus;
                failure.halconMessage = failure.message;
                throw failure;
            }
            checkStatus(api->reduceDomain(detectImage.graySource,
                                          clippedDetectRegion,
                                          &detectReducedImage),
                        QStringLiteral("reduce_domain.corrected_roi"));
            detectSearchSource = detectReducedImage;
            result.payload.insert(QStringLiteral("correctedRoiArea"), transformed.area);
            result.payload.insert(QStringLiteral("detectMaskApplied"), true);
            result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), polygonDetectRoi);
        } else if (polygonDetectRoi) {
            if (detectPolygonPixels.size() < 3) {
                HalconFailure failure;
                failure.status = QStringLiteral("invalid_detect_polygon");
                failure.message = QStringLiteral("polygon detect ROI requires at least 3 points");
                failure.stage = QStringLiteral("detect_polygon");
                failure.halconMessage = failure.message;
                throw failure;
            }
            if (api->genRegionPolygon && api->reduceDomain) {
                QVector<double> rows;
                QVector<double> columns;
                rows.reserve(detectPolygonPixels.size());
                columns.reserve(detectPolygonPixels.size());
                for (const QPointF &point : detectPolygonPixels) {
                    rows.append(qBound(0.0,
                                       point.y() - static_cast<double>(detectRoiPixels.y()),
                                       static_cast<double>(detectMat.rows - 1)));
                    columns.append(qBound(0.0,
                                          point.x() - static_cast<double>(detectRoiPixels.x()),
                                          static_cast<double>(detectMat.cols - 1)));
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
                            QStringLiteral("reduce_domain.detect_polygon_roi"));
                detectSearchSource = detectReducedImage;
                result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), true);
                result.payload.insert(QStringLiteral("detectMaskApplied"), true);
            } else {
                result.payload.insert(QStringLiteral("polygonDetectRoiApplied"), false);
                result.payload.insert(QStringLiteral("detectPolygonApplyError"),
                                      QStringLiteral("HALCON gen_region_polygon/reduce_domain symbol unavailable"));
                HalconFailure failure;
                failure.status = QStringLiteral("PatternPresence HALCON error");
                failure.message = QStringLiteral("HALCON polygon detect ROI symbols are unavailable");
                failure.stage = QStringLiteral("detect_polygon");
                failure.halconMessage = failure.message;
                throw failure;
            }
        }

        outputTuplesCreated = true;
        {
            QMutexLocker modelLocker(&shapeModel->mutex);
            QElapsedTimer findTimer;
            findTimer.start();
            const Herror findStatus = api->findScaledShapeModel(detectSearchSource,
                                                                shapeModel->modelIdTuple,
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
                                                                &scoreTuple);
            findMs = findTimer.elapsed();
            checkStatus(findStatus, QStringLiteral("find_scaled_shape_model"));
        }

        const int matchCount = qMin<int>(qMin<int>(rowTuple.num, columnTuple.num),
                                         qMin<int>(qMin<int>(angleTuple.num, scaleTuple.num),
                                                   scoreTuple.num));
        QVector<double> matchScores;
        QJsonArray matchesArray;
        matchScores.reserve(qMax(0, matchCount));

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
            const double imageRow = localRow +
                    (correctionApplied ? 0.0 : static_cast<double>(detectRoiPixels.y()));
            const double imageColumn = localColumn +
                    (correctionApplied ? 0.0 : static_cast<double>(detectRoiPixels.x()));

            matchScores.append(score);
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

        const bool found = matchCount > 0;
        bool ok = false;
        const bool scoreJudge = isScoreJudgeBasis(config.judgeBasis);
        if (scoreJudge)
            ok = found && bestScore >= normalizedScore(config.scoreThreshold);
        else if (isPresenceJudgeBasis(config.judgeBasis))
            ok = config.existOk ? found : !found;
        else
            ok = config.existOk ? found : !found;

        QString okNgReason;
        if (scoreJudge) {
            if (!found)
                okNgReason = QStringLiteral("not found target, score judge");
            else if (bestScore >= normalizedScore(config.scoreThreshold))
                okNgReason = QStringLiteral("score meets threshold");
            else
                okNgReason = QStringLiteral("score below threshold");
        } else if (found) {
            okNgReason = config.existOk
                    ? QStringLiteral("found target, existOk=true")
                    : QStringLiteral("found target, existOk=false");
        } else {
            okNgReason = config.existOk
                    ? QStringLiteral("not found target, existOk=true")
                    : QStringLiteral("not found target, existOk=false");
        }

        result.success = true;
        result.ok = ok;
        result.score = bestScore;
        result.count = matchCount;
        result.text = found ? QStringLiteral("found") : QStringLiteral("missing");
        result.status = QStringLiteral("%1 score=%2")
                .arg(result.text, QString::number(bestScore, 'f', 3));
        result.message = QStringLiteral("PatternPresence: %1 score=%2 count=%3")
                .arg(result.text,
                     QString::number(bestScore, 'f', 3),
                     QString::number(matchCount));

        QRectF matchRect;
        if (found) {
            const double scaledWidth = static_cast<double>(templateRoiPixels.width()) * qMax(0.001, bestScale);
            const double scaledHeight = static_cast<double>(templateRoiPixels.height()) * qMax(0.001, bestScale);
            matchRect = QRectF(bestColumn - scaledWidth / 2.0,
                               bestRow - scaledHeight / 2.0,
                               scaledWidth,
                               scaledHeight);
        }

        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("foundCount"), matchCount);
        result.payload.insert(QStringLiteral("matchCount"), matchCount);
        result.payload.insert(QStringLiteral("bestScore"), bestScore);
        result.payload.insert(QStringLiteral("bestRow"), bestRow);
        result.payload.insert(QStringLiteral("bestColumn"), bestColumn);
        result.payload.insert(QStringLiteral("bestAngle"), bestAngle);
        result.payload.insert(QStringLiteral("bestAngleDeg"), bestAngle * 180.0 / kPi);
        result.payload.insert(QStringLiteral("bestScale"), bestScale);
        result.payload.insert(QStringLiteral("rawMatchRow"), bestRawRow);
        result.payload.insert(QStringLiteral("rawMatchCol"), bestRawColumn);
        result.payload.insert(QStringLiteral("globalMatchRow"), bestRow);
        result.payload.insert(QStringLiteral("globalMatchCol"), bestColumn);
        result.payload.insert(QStringLiteral("matchRectX"), matchRect.x());
        result.payload.insert(QStringLiteral("matchRectY"), matchRect.y());
        result.payload.insert(QStringLiteral("matchRectW"), matchRect.width());
        result.payload.insert(QStringLiteral("matchRectH"), matchRect.height());
        result.payload.insert(QStringLiteral("matchScores"), scoresToJson(matchScores));
        result.payload.insert(QStringLiteral("matches"), matchesArray);
        result.payload.insert(QStringLiteral("usedMinScore"), minScore);
        result.payload.insert(QStringLiteral("searchMinScoreUsed"), minScore);
        result.payload.insert(QStringLiteral("usedScoreThreshold"), normalizedScore(config.scoreThreshold));
        result.payload.insert(QStringLiteral("judgeScoreThresholdUsed"), normalizedScore(config.scoreThreshold));
        result.payload.insert(QStringLiteral("usedMetric"), metric);
        result.payload.insert(QStringLiteral("metricUsed"), metric);
        result.payload.insert(QStringLiteral("usedAngleStartRad"), angleStartRad);
        result.payload.insert(QStringLiteral("usedAngleExtentRad"), angleExtentRad);
        result.payload.insert(QStringLiteral("angleStartDegUsed"), config.angleStart);
        result.payload.insert(QStringLiteral("angleExtentDegUsed"), config.angleExtent);
        result.payload.insert(QStringLiteral("usedNumMatches"), numMatches);
        result.payload.insert(QStringLiteral("okNgReason"), okNgReason);
        result.payload.insert(QStringLiteral("text"), result.text);

        if (found) {
            const double crossRadius = qBound(6.0,
                                              static_cast<double>(qMin(templateMat.cols, templateMat.rows)) / 4.0,
                                              24.0);
            const QPointF center(bestColumn, bestRow);
            result.overlays.append(rectOverlay(matchRect,
                                               QStringLiteral("match_rect"),
                                               bestScore));
            result.payload.insert(QStringLiteral("matchBboxOverlayApplied"), true);
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
                                               QString::number(bestScore, 'f', 3),
                                               QStringLiteral("match_score"),
                                               bestScore));
        }

        if (config.showContourPoints && !found) {
            result.payload.insert(QStringLiteral("contourSource"), QStringLiteral("no_match"));
            result.payload.insert(QStringLiteral("contourOverlayReason"),
                                  QStringLiteral("no match, contour overlay suppressed"));
            if (!matchAutoDomainContourGlobalDebugPath.isEmpty()) {
                cv::Mat globalDebugImage = debugBgrMat(image);
                result.payload.insert(QStringLiteral("contourDebugMatchAutoModelDomainContourGlobalSaved"),
                                      saveDebugImage(matchAutoDomainContourGlobalDebugPath, globalDebugImage));
            }
        } else if (config.showContourPoints && found) {
            QMutexLocker modelLocker(&shapeModel->mutex);
            if (!patternPresenceHalconObjectAllocated(shapeModel->modelContour)) {
                result.payload.insert(QStringLiteral("showContourPointsApplied"), false);
                result.payload.insert(QStringLiteral("contourSource"),
                                      QStringLiteral("shape_model_contour_unavailable"));
                result.payload.insert(QStringLiteral("contourOverlayReason"),
                                      QStringLiteral("get_shape_model_contours did not produce drawable contour"));
            } else if (!api->hasContourTransformOperators()) {
                result.payload.insert(QStringLiteral("showContourPointsApplied"), false);
                result.payload.insert(QStringLiteral("contourSource"),
                                      QStringLiteral("shape_model_contour_unavailable"));
                result.payload.insert(QStringLiteral("contourOverlayReason"),
                                      QStringLiteral("shape model contour transform operators unavailable"));
            } else {
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
                bool contourScaleTransformApplied = false;
                if (std::abs(bestScale - 1.0) > 0.0001 && api->homMat2dScaleLocal) {
                    createDoubleTuple(displayScaleXTuple, bestScale);
                    createDoubleTuple(displayScaleYTuple, bestScale);
                    checkStatus(api->homMat2dScaleLocal(displayHomMatTuple,
                                                        displayScaleXTuple,
                                                        displayScaleYTuple,
                                                        &displayScaledHomMatTuple),
                                QStringLiteral("hom_mat2d_scale_local.model_contour"));
                    homMatForContour = &displayScaledHomMatTuple;
                    contourScaleTransformApplied = true;
                }
                checkStatus(api->affineTransContourXld(shapeModel->modelContour,
                                                       &transformedDisplayContour,
                                                       *homMatForContour),
                            QStringLiteral("affine_trans_contour_xld.model_contour"));

                const XldContourReadResult localRead =
                        readXldContours(api, shapeModel->modelContour);
                const XldContourReadResult transformedRead =
                        readXldContours(api, transformedDisplayContour);
                QVector<QVector<QPointF>> globalContours =
                        translatedContours(transformedRead.contours,
                                           correctionApplied
                                           ? QPointF()
                                           : QPointF(static_cast<double>(detectRoiPixels.x()),
                                                     static_cast<double>(detectRoiPixels.y())));

                int displayPointCount = 0;
                const int overlayLineCount = appendContourLineOverlays(&result.overlays,
                                                                       globalContours,
                                                                       bestScore,
                                                                       &displayPointCount);
                const bool applied = overlayLineCount > 0;
                result.payload.insert(QStringLiteral("showContourPointsApplied"), applied);
                result.payload.insert(QStringLiteral("contourOverlayEmitted"), applied);
                result.payload.insert(QStringLiteral("modelContourOverlayApplied"), applied);
                result.payload.insert(QStringLiteral("modelContourScaleTransformApplied"),
                                      contourScaleTransformApplied);
                result.payload.insert(QStringLiteral("contourSource"),
                                      applied ? QStringLiteral("get_shape_model_contours")
                                              : QStringLiteral("shape_model_empty_contour"));
                result.payload.insert(QStringLiteral("displayContourSource"),
                                      QStringLiteral("shape_model_contour"));
                result.payload.insert(QStringLiteral("displayContourObjectCount"),
                                      transformedRead.objectCount);
                result.payload.insert(QStringLiteral("displayContourPointCount"),
                                      transformedRead.pointCount);
                result.payload.insert(QStringLiteral("contourLineSegmentCount"), overlayLineCount);
                result.payload.insert(QStringLiteral("contourDisplayPointCount"), displayPointCount);
                if (!applied) {
                    result.payload.insert(QStringLiteral("contourOverlayReason"),
                                          transformedRead.error.isEmpty()
                                          ? QStringLiteral("auto model domain contour had no drawable segments")
                                          : transformedRead.error);
                }

                if (!autoDomainRegionDebugPath.isEmpty()) {
                    cv::Mat regionDebugImage = debugBgrMat(templateMat);
                    drawContourPolylines(regionDebugImage,
                                         localRead.contours,
                                         cv::Scalar(0, 180, 0));
                    result.payload.insert(QStringLiteral("contourDebugAutoModelDomainRegionSaved"),
                                          saveDebugImage(autoDomainRegionDebugPath, regionDebugImage));
                }
                if (!autoDomainContourLocalDebugPath.isEmpty()) {
                    cv::Mat localDebugImage = debugBgrMat(templateMat);
                    drawContourPolylines(localDebugImage,
                                         localRead.contours,
                                         cv::Scalar(0, 255, 255));
                    result.payload.insert(QStringLiteral("contourDebugAutoModelDomainContourLocalSaved"),
                                          saveDebugImage(autoDomainContourLocalDebugPath, localDebugImage));
                }
                if (!matchAutoDomainContourGlobalDebugPath.isEmpty()) {
                    cv::Mat globalDebugImage = debugBgrMat(image);
                    drawContourPolylines(globalDebugImage,
                                         globalContours,
                                         cv::Scalar(0, 255, 255));
                    result.payload.insert(QStringLiteral("contourDebugMatchAutoModelDomainContourGlobalSaved"),
                                          saveDebugImage(matchAutoDomainContourGlobalDebugPath, globalDebugImage));
                }
            }
        }

        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("modelCacheHit"), modelCacheHit);
        result.payload.insert(QStringLiteral("modelBuildMs"), static_cast<double>(modelBuildMs));
        result.payload.insert(QStringLiteral("modelRebuildReason"), rebuildReason);
        result.payload.insert(QStringLiteral("findMs"), static_cast<double>(findMs));
        result.payload.insert(QStringLiteral("convertMs"), static_cast<double>(convertMs));
        result.payload.insert(QStringLiteral("timeoutExceeded"),
                              config.timeoutMs > 0 && result.elapsedMs > config.timeoutMs);
        result.payload.insert(QStringLiteral("shapeModelOrigin"),
                              modelCacheHit ? QStringLiteral("model_cache")
                                            : QStringLiteral("created_and_cached"));

        writeDebugInfo();
        logPatternPerf(config,
                       image,
                       referenceImage,
                       templateMat,
                       detectMat,
                       templateRoiPixels,
                       detectRoiPixels,
                       modelCacheHit,
                       effectiveModelCacheKey,
                       modelBuildMs,
                       findMs,
                       convertMs,
                       result.elapsedMs,
                       rebuildReason);
        logPatternPolygonDebug(config, result.payload);
        logPatternPresenceRoiDebug(config, result.payload, result.status, result.message);
        cleanup();
        return result;
    } catch (const HalconFailure &errorInfo) {
        cleanup();
        result.success = false;
        result.ok = false;
        result.status = errorInfo.status;
        result.message = errorInfo.message;
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), errorInfo.message);
        result.payload.insert(QStringLiteral("okNgReason"), errorInfo.message);
        result.payload.insert(QStringLiteral("halconStage"), errorInfo.stage);
        result.payload.insert(QStringLiteral("halconErrorCode"), static_cast<int>(errorInfo.code));
        result.payload.insert(QStringLiteral("halconErrorMessage"), errorInfo.halconMessage);
        if ((errorInfo.stage == QStringLiteral("create_shape_model") ||
             errorInfo.stage == QStringLiteral("create_scaled_shape_model")) &&
            static_cast<int>(errorInfo.code) == 8510) {
            result.payload.insert(QStringLiteral("modelPointStatus"), QStringLiteral("too_small"));
        } else if (errorInfo.stage.contains(QStringLiteral("reduce_domain")) ||
                   errorInfo.stage.contains(QStringLiteral("gen_region_polygon"))) {
            result.payload.insert(QStringLiteral("modelPointStatus"), QStringLiteral("empty_domain"));
        }
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("modelCacheHit"), modelCacheHit);
        result.payload.insert(QStringLiteral("modelBuildMs"), static_cast<double>(modelBuildMs));
        result.payload.insert(QStringLiteral("modelRebuildReason"), rebuildReason);
        result.payload.insert(QStringLiteral("findMs"), static_cast<double>(findMs));
        result.payload.insert(QStringLiteral("convertMs"), static_cast<double>(convertMs));
        writeDebugInfo();
        logPatternPerf(config,
                       image,
                       referenceImage,
                       templateMat,
                       detectMat,
                       templateRoiPixels,
                       detectRoiPixels,
                       modelCacheHit,
                       effectiveModelCacheKey,
                       modelBuildMs,
                       findMs,
                       convertMs,
                       result.elapsedMs,
                       rebuildReason);
        logPatternPolygonDebug(config, result.payload);
        logPatternPresenceRoiDebug(config, result.payload, result.status, result.message);
        return result;
    } catch (const std::exception &error) {
        cleanup();
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("PatternPresence HALCON error");
        result.message = QString::fromLocal8Bit(error.what());
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.payload.insert(QStringLiteral("okNgReason"), result.message);
        result.payload.insert(QStringLiteral("halconErrorCode"), 0);
        result.payload.insert(QStringLiteral("halconErrorMessage"), result.message);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("modelCacheHit"), modelCacheHit);
        result.payload.insert(QStringLiteral("modelBuildMs"), static_cast<double>(modelBuildMs));
        result.payload.insert(QStringLiteral("modelRebuildReason"), rebuildReason);
        result.payload.insert(QStringLiteral("findMs"), static_cast<double>(findMs));
        result.payload.insert(QStringLiteral("convertMs"), static_cast<double>(convertMs));
        writeDebugInfo();
        logPatternPerf(config,
                       image,
                       referenceImage,
                       templateMat,
                       detectMat,
                       templateRoiPixels,
                       detectRoiPixels,
                       modelCacheHit,
                       effectiveModelCacheKey,
                       modelBuildMs,
                       findMs,
                       convertMs,
                       result.elapsedMs,
                       rebuildReason);
        logPatternPolygonDebug(config, result.payload);
        logPatternPresenceRoiDebug(config, result.payload, result.status, result.message);
        return result;
    } catch (...) {
        cleanup();
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("PatternPresence HALCON error");
        result.message = QStringLiteral("unknown PatternPresence HALCON error");
        result.text = QStringLiteral("error");
        result.payload.insert(QStringLiteral("error"), result.message);
        result.payload.insert(QStringLiteral("okNgReason"), result.message);
        result.payload.insert(QStringLiteral("halconErrorCode"), 0);
        result.payload.insert(QStringLiteral("halconErrorMessage"), result.message);
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        result.payload.insert(QStringLiteral("modelCacheHit"), modelCacheHit);
        result.payload.insert(QStringLiteral("modelBuildMs"), static_cast<double>(modelBuildMs));
        result.payload.insert(QStringLiteral("modelRebuildReason"), rebuildReason);
        result.payload.insert(QStringLiteral("findMs"), static_cast<double>(findMs));
        result.payload.insert(QStringLiteral("convertMs"), static_cast<double>(convertMs));
        writeDebugInfo();
        logPatternPerf(config,
                       image,
                       referenceImage,
                       templateMat,
                       detectMat,
                       templateRoiPixels,
                       detectRoiPixels,
                       modelCacheHit,
                       effectiveModelCacheKey,
                       modelBuildMs,
                       findMs,
                       convertMs,
                       result.elapsedMs,
                       rebuildReason);
        logPatternPolygonDebug(config, result.payload);
        logPatternPresenceRoiDebug(config, result.payload, result.status, result.message);
        return result;
    }
}
