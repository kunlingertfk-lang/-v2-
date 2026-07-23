#include "algorithms/location/TemplateLocationHalconRunner.h"

#include <HalconCpp.h>

#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRect>
#include <QSaveFile>
#include <QStandardPaths>

#include <opencv2/imgproc.hpp>

#include <cmath>

namespace {

using namespace HalconCpp;

constexpr double kPi = 3.14159265358979323846;
QMutex modelCacheMutex;

QJsonObject rectToJson(const QRectF &rect)
{
    return QJsonObject{{QStringLiteral("x"), rect.x()},
                       {QStringLiteral("y"), rect.y()},
                       {QStringLiteral("width"), rect.width()},
                       {QStringLiteral("height"), rect.height()}};
}

QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points) {
        array.append(QJsonObject{{QStringLiteral("x"), point.x()},
                                 {QStringLiteral("y"), point.y()}});
    }
    return array;
}

QString cacheStem(const QString &modelCacheKey)
{
    const QByteArray digest = QCryptographicHash::hash(
                modelCacheKey.toUtf8(), QCryptographicHash::Sha256).toHex();
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (root.isEmpty())
        root = QDir::tempPath();
    QDir directory(root);
    directory.mkpath(QStringLiteral("template_location_models"));
    return directory.filePath(QStringLiteral("template_location_models/%1")
                              .arg(QString::fromLatin1(digest)));
}

QByteArray modelSignature(const cv::Mat &referenceGray,
                          const TemplateLocationHalconConfig &config)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(reinterpret_cast<const char *>(referenceGray.data),
                 static_cast<int>(referenceGray.total()));
    QJsonObject contract{
        {QStringLiteral("width"), referenceGray.cols},
        {QStringLiteral("height"), referenceGray.rows},
        {QStringLiteral("templateRegionType"), config.templateRegionType},
        {QStringLiteral("templateRoiNormalized"), rectToJson(config.templateRoiNormalized)},
        {QStringLiteral("templatePolygonNormalized"), pointsToJson(config.templatePolygonNormalized)},
        {QStringLiteral("angleStart"), config.angleStart},
        {QStringLiteral("angleExtent"), config.angleExtent},
        {QStringLiteral("scaleMin"), config.scaleMin},
        {QStringLiteral("scaleMax"), config.scaleMax},
        {QStringLiteral("polarity"), config.polarity},
        {QStringLiteral("contrastMode"), config.contrastMode},
        {QStringLiteral("contrast"), config.contrast},
        {QStringLiteral("minContrast"), config.minContrast},
        {QStringLiteral("numLevels"), config.numLevels}
    };
    hash.addData(QJsonDocument(contract).toJson(QJsonDocument::Compact));
    return hash.result().toHex();
}

bool writeSignature(const QString &path, const QByteArray &signature)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (file.write(signature) != signature.size())
        return false;
    return file.commit();
}

bool validNormalizedRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y()) &&
           std::isfinite(rect.width()) && std::isfinite(rect.height()) &&
           rect.width() > 0.0 && rect.height() > 0.0 &&
           rect.left() >= 0.0 && rect.top() >= 0.0 &&
           rect.right() <= 1.000001 && rect.bottom() <= 1.000001;
}

QRect pixelRect(const QRectF &normalized, int width, int height)
{
    if (!validNormalizedRect(normalized) || width <= 0 || height <= 0)
        return QRect();
    const int left = qBound(0, static_cast<int>(std::floor(normalized.left() * width)), width - 1);
    const int top = qBound(0, static_cast<int>(std::floor(normalized.top() * height)), height - 1);
    const int rightExclusive = qBound(0, static_cast<int>(std::ceil(normalized.right() * width)), width);
    const int bottomExclusive = qBound(0, static_cast<int>(std::ceil(normalized.bottom() * height)), height);
    const QRect result(left, top, rightExclusive - left, bottomExclusive - top);
    return result.width() >= 2 && result.height() >= 2 ? result : QRect();
}

cv::Mat grayImage(const cv::Mat &image)
{
    if (image.channels() == 1)
        return image.isContinuous() ? image : image.clone();
    cv::Mat gray;
    if (image.channels() == 3)
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    else if (image.channels() == 4)
        cv::cvtColor(image, gray, cv::COLOR_BGRA2GRAY);
    return gray;
}

HObject halconByteImage(const cv::Mat &gray)
{
    HObject image;
    GenImage1(&image,
                            HTuple("byte"),
                            HTuple(static_cast<Hlong>(gray.cols)),
                            HTuple(static_cast<Hlong>(gray.rows)),
                            HTuple(reinterpret_cast<Hlong>(gray.data)));
    return image;
}

HTuple rowsTuple(const QVector<QPointF> &points, int imageHeight, const QRect &crop)
{
    HTuple rows;
    for (int i = 0; i < points.size(); ++i)
        rows[i] = qBound(0.0, points.at(i).y() * imageHeight - crop.y(),
                         static_cast<double>(crop.height() - 1));
    return rows;
}

HTuple columnsTuple(const QVector<QPointF> &points, int imageWidth, const QRect &crop)
{
    HTuple columns;
    for (int i = 0; i < points.size(); ++i)
        columns[i] = qBound(0.0, points.at(i).x() * imageWidth - crop.x(),
                            static_cast<double>(crop.width() - 1));
    return columns;
}

HObject croppedDomain(const HObject &image,
                      const QRect &crop,
                      const QString &regionType,
                      const QVector<QPointF> &polygon,
                      const QPointF &circleCenter,
                      double circleRadiusNormalized,
                      int imageWidth,
                      int imageHeight)
{
    HObject cropped;
    CropRectangle1(image, &cropped,
                                 crop.y(), crop.x(),
                                 crop.y() + crop.height() - 1,
                                 crop.x() + crop.width() - 1);
    const QString type = regionType.trimmed().toLower();
    if (type != QStringLiteral("polygon") && type != QStringLiteral("circle"))
        return cropped;

    HObject region;
    HObject reduced;
    if (type == QStringLiteral("circle")) {
        const double radius = circleRadiusNormalized * qMax(imageWidth, imageHeight);
        GenCircle(&region,
                  circleCenter.y() * imageHeight - crop.y(),
                  circleCenter.x() * imageWidth - crop.x(),
                  radius);
    } else {
        GenRegionPolygon(&region,
                         rowsTuple(polygon, imageHeight, crop),
                         columnsTuple(polygon, imageWidth, crop));
    }
    ReduceDomain(cropped, region, &reduced);
    return reduced;
}

QString metricForPolarity(const QString &value)
{
    const QString key = value.trimmed().toLower();
    if (key == QStringLiteral("ignore_local_polarity"))
        return QStringLiteral("ignore_local_polarity");
    if (key == QStringLiteral("ignore_global_polarity"))
        return QStringLiteral("ignore_global_polarity");
    return QStringLiteral("use_polarity");
}

void addRegionOverlay(QVector<ToolOverlay> *overlays,
                      const QString &type,
                      const QRectF &rect,
                      const QVector<QPointF> &polygon,
                      const QPointF &circleCenter,
                      double circleRadiusNormalized,
                      int width,
                      int height,
                      const QString &label)
{
    ToolOverlay overlay;
    overlay.label = label;
    if (type == QStringLiteral("circle") && circleRadiusNormalized > 0.0) {
        overlay.type = ToolOverlayType::Circle;
        overlay.center = QPointF(circleCenter.x() * width,
                                 circleCenter.y() * height);
        overlay.radius = circleRadiusNormalized * qMax(width, height);
    } else if (type == QStringLiteral("polygon") && polygon.size() >= 3) {
        overlay.type = ToolOverlayType::Polygon;
        for (const QPointF &point : polygon)
            overlay.points.append(QPointF(point.x() * width, point.y() * height));
    } else {
        overlay.type = ToolOverlayType::Rect;
        overlay.rect = QRectF(rect.x() * width, rect.y() * height,
                              rect.width() * width, rect.height() * height);
    }
    overlays->append(overlay);
}

void addMatchOverlays(QVector<ToolOverlay> *overlays,
                      const HObject &modelContours,
                      double localRow,
                      double localColumn,
                      double globalRow,
                      double globalColumn,
                      double angle,
                      double scale,
                      double score,
                      int index,
                      const QPointF &searchOffset)
{
    HTuple rigid;
    HTuple scaled;
    HObject transformed;
    VectorAngleToRigid(0.0, 0.0, 0.0,
                                     localRow, localColumn, angle, &rigid);
    HomMat2dScaleLocal(rigid, scale, scale, &scaled);
    AffineTransContourXld(modelContours, &transformed, scaled);

    HTuple objectCount;
    CountObj(transformed, &objectCount);
    for (Hlong objectIndex = 1; objectIndex <= objectCount.L(); ++objectIndex) {
        HObject contour;
        HTuple rows;
        HTuple columns;
        SelectObj(transformed, &contour, objectIndex);
        GetContourXld(contour, &rows, &columns);
        if (rows.Length() < 2 || columns.Length() != rows.Length())
            continue;
        ToolOverlay line;
        line.type = ToolOverlayType::Polygon;
        line.label = QStringLiteral("match_result");
        line.score = score;
        line.extra.insert(QStringLiteral("matchIndex"), index);
        for (Hlong pointIndex = 0; pointIndex < rows.Length(); ++pointIndex) {
            line.points.append(QPointF(columns[pointIndex].D() + searchOffset.x(),
                                       rows[pointIndex].D() + searchOffset.y()));
        }
        overlays->append(line);
    }

    const double crossRadius = 8.0;
    ToolOverlay horizontal;
    horizontal.type = ToolOverlayType::Line;
    horizontal.label = QStringLiteral("match_center");
    horizontal.score = score;
    horizontal.extra.insert(QStringLiteral("matchIndex"), index);
    horizontal.extra.insert(QStringLiteral("role"),
                            QStringLiteral("match_origin"));
    horizontal.p1 = QPointF(globalColumn - crossRadius, globalRow);
    horizontal.p2 = QPointF(globalColumn + crossRadius, globalRow);
    overlays->append(horizontal);
    ToolOverlay vertical = horizontal;
    vertical.p1 = QPointF(globalColumn, globalRow - crossRadius);
    vertical.p2 = QPointF(globalColumn, globalRow + crossRadius);
    overlays->append(vertical);

    ToolOverlay scoreText;
    scoreText.type = ToolOverlayType::Text;
    scoreText.label = QStringLiteral("match_score");
    scoreText.score = score;
    scoreText.p1 = QPointF(globalColumn + crossRadius + 4.0,
                           globalRow - crossRadius - 4.0);
    scoreText.text = QStringLiteral("#%1  %2%")
            .arg(index + 1)
            .arg(score * 100.0, 0, 'f', 1);
    scoreText.extra.insert(QStringLiteral("matchIndex"), index);
    overlays->append(scoreText);
}

TemplateLocationHalconResult errorResult(const QString &status,
                                         const QString &message,
                                         qint64 elapsedMs)
{
    TemplateLocationHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.payload.insert(QStringLiteral("found"), false);
    result.payload.insert(QStringLiteral("errorCode"), status);
    result.payload.insert(QStringLiteral("errorMessage"), message);
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    return result;
}

} // namespace

bool TemplateLocationHalconRunner::clearPersistentCache(const QString &modelCacheKey)
{
    if (modelCacheKey.trimmed().isEmpty())
        return false;
    QMutexLocker locker(&modelCacheMutex);
    const QString stem = cacheStem(modelCacheKey);
    const bool modelRemoved = !QFileInfo::exists(stem + QStringLiteral(".shm")) ||
            QFile::remove(stem + QStringLiteral(".shm"));
    const bool signatureRemoved = !QFileInfo::exists(stem + QStringLiteral(".sha256")) ||
            QFile::remove(stem + QStringLiteral(".sha256"));
    return modelRemoved && signatureRemoved;
}

TemplateLocationHalconResult TemplateLocationHalconRunner::run(
        const cv::Mat &image,
        const cv::Mat &referenceImage,
        const TemplateLocationHalconConfig &config)
{
    QElapsedTimer timer;
    timer.start();
    if (image.empty())
        return errorResult(QStringLiteral("image_empty"), QStringLiteral("input image is empty"), timer.elapsed());
    if (referenceImage.empty())
        return errorResult(QStringLiteral("no_reference_image"), QStringLiteral("reference image is empty"), timer.elapsed());
    if (image.depth() != CV_8U || referenceImage.depth() != CV_8U ||
        (image.channels() != 1 && image.channels() != 3 && image.channels() != 4) ||
        (referenceImage.channels() != 1 && referenceImage.channels() != 3 && referenceImage.channels() != 4)) {
        return errorResult(QStringLiteral("unsupported_image_type"),
                           QStringLiteral("TemplateLocation supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
                           timer.elapsed());
    }
    if (!QFileInfo::exists(config.halconSoPath))
        return errorResult(QStringLiteral("halcon_runtime_missing"), QStringLiteral("HALCON runtime is unavailable"), timer.elapsed());
    if (config.templateRegionType == QStringLiteral("polygon") && config.templatePolygonNormalized.size() < 3)
        return errorResult(QStringLiteral("invalid_template_region"), QStringLiteral("template polygon requires at least 3 points"), timer.elapsed());
    if (config.searchRegionType == QStringLiteral("polygon") && config.searchPolygonNormalized.size() < 3)
        return errorResult(QStringLiteral("invalid_search_region"), QStringLiteral("search polygon requires at least 3 points"), timer.elapsed());
    if (config.searchRegionType == QStringLiteral("circle") &&
            (!std::isfinite(config.searchCircleCenterNormalized.x()) ||
             !std::isfinite(config.searchCircleCenterNormalized.y()) ||
             !std::isfinite(config.searchCircleRadiusNormalized) ||
             config.searchCircleRadiusNormalized <= 0.0)) {
        return errorResult(QStringLiteral("invalid_search_region"),
                           QStringLiteral("search circle is invalid"), timer.elapsed());
    }
    if (config.minMatchCount < 1 || config.maxMatchCount < config.minMatchCount ||
            config.maxMatchCount > config.maxMatches ||
            !std::isfinite(config.maxOverlap) || config.maxOverlap < 0.0 ||
            config.maxOverlap > 1.0) {
        return errorResult(QStringLiteral("invalid_parameter"),
                           QStringLiteral("match count or MaxOverlap is invalid"), timer.elapsed());
    }
    if (config.originMode == QStringLiteral("custom") &&
            (!std::isfinite(config.customOriginNormalized.x()) ||
             !std::isfinite(config.customOriginNormalized.y()) ||
             config.customOriginNormalized.x() < 0.0 || config.customOriginNormalized.x() > 1.0 ||
             config.customOriginNormalized.y() < 0.0 || config.customOriginNormalized.y() > 1.0)) {
        return errorResult(QStringLiteral("invalid_origin"),
                           QStringLiteral("custom origin point is invalid"), timer.elapsed());
    }

    const QRect templateRect = pixelRect(config.templateRoiNormalized,
                                         referenceImage.cols, referenceImage.rows);
    const QRect searchRect = pixelRect(config.searchRoiNormalized, image.cols, image.rows);
    if (templateRect.isEmpty())
        return errorResult(QStringLiteral("invalid_template_region"), QStringLiteral("template ROI is invalid"), timer.elapsed());
    if (searchRect.isEmpty())
        return errorResult(QStringLiteral("invalid_search_region"), QStringLiteral("search ROI is invalid"), timer.elapsed());

    HTuple modelId;
    bool modelCreated = false;
    try {
        const cv::Mat referenceGray = grayImage(referenceImage);
        const cv::Mat imageGray = grayImage(image);
        const HObject referenceHalcon = halconByteImage(referenceGray);
        const HObject imageHalcon = halconByteImage(imageGray);
        const HObject templateDomain = croppedDomain(referenceHalcon,
                                                      templateRect,
                                                      config.templateRegionType,
                                                      config.templatePolygonNormalized,
                                                      QPointF(), 0.0,
                                                      referenceImage.cols,
                                                      referenceImage.rows);
        const HObject searchDomain = croppedDomain(imageHalcon,
                                                    searchRect,
                                                    config.searchRegionType,
                                                    config.searchPolygonNormalized,
                                                    config.searchCircleCenterNormalized,
                                                    config.searchCircleRadiusNormalized,
                                                    image.cols,
                                                    image.rows);

        HObject templateRegion;
        HTuple templateArea;
        HTuple centroidRows;
        HTuple centroidColumns;
        GetDomain(templateDomain, &templateRegion);
        AreaCenter(templateRegion, &templateArea, &centroidRows, &centroidColumns);
        if (centroidRows.Length() < 1 || centroidColumns.Length() < 1)
            return errorResult(QStringLiteral("invalid_template_region"),
                               QStringLiteral("template region has no centroid"), timer.elapsed());
        const double centroidRow = centroidRows[0].D();
        const double centroidColumn = centroidColumns[0].D();
        double originDeltaRow = 0.0;
        double originDeltaColumn = 0.0;
        if (config.originMode == QStringLiteral("custom")) {
            originDeltaRow = config.customOriginNormalized.y() * referenceImage.rows -
                    templateRect.y() - centroidRow;
            originDeltaColumn = config.customOriginNormalized.x() * referenceImage.cols -
                    templateRect.x() - centroidColumn;
        }

        const double angleStart = config.angleStart * kPi / 180.0;
        const double angleExtent = config.angleExtent * kPi / 180.0;
        const double scaleMin = config.scaleMin / 100.0;
        const double scaleMax = config.scaleMax / 100.0;
        const HTuple levels = config.numLevels > 0 ? HTuple(config.numLevels) : HTuple("auto");
        const HTuple contrast = config.contrastMode == QStringLiteral("manual")
                ? HTuple(config.contrast) : HTuple("auto");
        const HTuple minContrast = config.contrastMode == QStringLiteral("manual")
                ? HTuple(config.minContrast) : HTuple("auto");

        const QByteArray signature = modelSignature(referenceGray, config);
        const QString stem = cacheStem(config.modelCacheKey);
        const QString modelPath = stem + QStringLiteral(".shm");
        const QString signaturePath = stem + QStringLiteral(".sha256");
        bool modelCacheHit = false;
        bool modelCachePersisted = false;
        {
            QMutexLocker cacheLocker(&modelCacheMutex);
            QFile signatureFile(signaturePath);
            if (QFileInfo::exists(modelPath) && signatureFile.open(QIODevice::ReadOnly) &&
                    signatureFile.readAll() == signature) {
                try {
                    ReadShapeModel(modelPath.toLocal8Bit().constData(), &modelId);
                    modelCreated = true;
                    modelCacheHit = true;
                } catch (const HException &) {
                    QFile::remove(modelPath);
                    QFile::remove(signaturePath);
                }
            }

            if (!modelCreated) {
                CreateScaledShapeModel(templateDomain,
                                             levels,
                                             angleStart,
                                             angleExtent,
                                             HTuple("auto"),
                                             scaleMin,
                                             scaleMax,
                                             HTuple("auto"),
                                             HTuple("auto"),
                                             metricForPolarity(config.polarity).toLatin1().constData(),
                                             contrast,
                                             minContrast,
                                             &modelId);
                modelCreated = true;
                try {
                    WriteShapeModel(modelId, modelPath.toLocal8Bit().constData());
                    modelCachePersisted = writeSignature(signaturePath, signature);
                } catch (const HException &) {
                    QFile::remove(modelPath);
                    QFile::remove(signaturePath);
                }
            } else {
                modelCachePersisted = true;
            }
        }
        if (config.timeoutMs > 0)
            SetShapeModelParam(modelId, "timeout", config.timeoutMs);

        HTuple actualLevels;
        HTuple actualAngleStart;
        HTuple actualAngleExtent;
        HTuple actualAngleStep;
        HTuple actualScaleMin;
        HTuple actualScaleMax;
        HTuple actualScaleStep;
        HTuple actualMetric;
        HTuple actualMinContrast;
        GetShapeModelParams(modelId,
                                          &actualLevels, &actualAngleStart, &actualAngleExtent,
                                          &actualAngleStep, &actualScaleMin, &actualScaleMax,
                                          &actualScaleStep, &actualMetric, &actualMinContrast);

        HTuple rows;
        HTuple columns;
        HTuple angles;
        HTuple scales;
        HTuple scores;
        FindScaledShapeModel(searchDomain,
                                           modelId,
                                           angleStart,
                                           angleExtent,
                                           scaleMin,
                                           scaleMax,
                                           config.minScore / 100.0,
                                           config.maxMatches,
                                           config.maxOverlap,
                                           config.subPixel == QStringLiteral("none") ? "none" : "least_squares",
                                           config.numLevels > 0 ? config.numLevels : 0,
                                           config.greediness,
                                           &rows, &columns, &angles, &scales, &scores);

        HObject modelContours;
        GetShapeModelContours(&modelContours, modelId, 1);
        const int count = static_cast<int>(qMin(qMin(rows.Length(), columns.Length()),
                                                qMin(qMin(angles.Length(), scales.Length()), scores.Length())));
        TemplateLocationHalconResult result;
        result.success = true;
        const bool found = count > 0;
        result.ok = count >= config.minMatchCount && count <= config.maxMatchCount;
        result.status = !found ? QStringLiteral("not_found")
                               : (result.ok ? QStringLiteral("found")
                                            : QStringLiteral("count_out_of_range"));
        result.message = !found
                ? QStringLiteral("TemplateLocation: no match reached the minimum score")
                : (result.ok
                   ? QStringLiteral("TemplateLocation: found %1 accepted match(es)").arg(count)
                   : QStringLiteral("TemplateLocation: found count %1 is outside %2..%3")
                     .arg(count).arg(config.minMatchCount).arg(config.maxMatchCount));
        result.count = count;
        result.score = count > 0 ? scores[0].D() : 0.0;

        addRegionOverlay(&result.overlays, config.searchRegionType,
                         config.searchRoiNormalized, config.searchPolygonNormalized,
                         config.searchCircleCenterNormalized,
                         config.searchCircleRadiusNormalized,
                         image.cols, image.rows, QStringLiteral("detect_roi"));
        QJsonArray matches;
        for (int index = 0; index < count; ++index) {
            const double localRow = rows[index].D();
            const double localColumn = columns[index].D();
            const double modelGlobalRow = localRow + searchRect.y();
            const double modelGlobalColumn = localColumn + searchRect.x();
            const double angle = angles[index].D();
            const double scale = scales[index].D();
            const double score = scores[index].D();
            HTuple anchorTransform;
            HTuple scaledAnchorTransform;
            HTuple anchorRows;
            HTuple anchorColumns;
            VectorAngleToRigid(0.0, 0.0, 0.0,
                               localRow, localColumn, angle, &anchorTransform);
            HomMat2dScaleLocal(anchorTransform, scale, scale, &scaledAnchorTransform);
            AffineTransPoint2d(scaledAnchorTransform,
                               originDeltaRow, originDeltaColumn,
                               &anchorRows, &anchorColumns);
            const double globalRow = anchorRows[0].D() + searchRect.y();
            const double globalColumn = anchorColumns[0].D() + searchRect.x();
            matches.append(QJsonObject{{QStringLiteral("index"), index},
                                       {QStringLiteral("x"), globalColumn},
                                       {QStringLiteral("y"), globalRow},
                                       {QStringLiteral("modelX"), modelGlobalColumn},
                                       {QStringLiteral("modelY"), modelGlobalRow},
                                       {QStringLiteral("angleDeg"), angle * 180.0 / kPi},
                                       {QStringLiteral("angle"), angle * 180.0 / kPi},
                                       {QStringLiteral("scale"), scale},
                                       {QStringLiteral("score"), score}});
            addMatchOverlays(&result.overlays, modelContours,
                             localRow, localColumn, globalRow, globalColumn,
                             angle, scale, score, index,
                             QPointF(searchRect.x(), searchRect.y()));
        }

        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("countAccepted"), result.ok);
        result.payload.insert(QStringLiteral("foundCount"), count);
        result.payload.insert(QStringLiteral("matches"), matches);
        result.payload.insert(QStringLiteral("x"), count > 0 ? matches.at(0).toObject().value(QStringLiteral("x")) : QJsonValue(-1.0));
        result.payload.insert(QStringLiteral("y"), count > 0 ? matches.at(0).toObject().value(QStringLiteral("y")) : QJsonValue(-1.0));
        result.payload.insert(QStringLiteral("angleDeg"), count > 0 ? matches.at(0).toObject().value(QStringLiteral("angleDeg")) : QJsonValue(0.0));
        result.payload.insert(QStringLiteral("angle"), count > 0 ? matches.at(0).toObject().value(QStringLiteral("angleDeg")) : QJsonValue(0.0));
        result.payload.insert(QStringLiteral("scale"), count > 0 ? matches.at(0).toObject().value(QStringLiteral("scale")) : QJsonValue(1.0));
        result.payload.insert(QStringLiteral("primaryMatchIndex"), count > 0 ? 0 : -1);
        result.payload.insert(QStringLiteral("pose"), count > 0
                              ? QJsonValue(matches.at(0).toObject()) : QJsonValue(QJsonObject()));
        result.payload.insert(QStringLiteral("coordinateSystem"), QStringLiteral("image_pixel"));
        result.payload.insert(QStringLiteral("angleUnit"), QStringLiteral("degree"));
        result.payload.insert(QStringLiteral("score"), result.score);
        result.payload.insert(QStringLiteral("maxMatches"), config.maxMatches);
        result.payload.insert(QStringLiteral("minMatchCount"), config.minMatchCount);
        result.payload.insert(QStringLiteral("maxMatchCount"), config.maxMatchCount);
        result.payload.insert(QStringLiteral("maxOverlap"), config.maxOverlap);
        result.payload.insert(QStringLiteral("originMode"), config.originMode);
        result.payload.insert(QStringLiteral("customOriginNormalized"),
                              QJsonObject{{QStringLiteral("x"), config.customOriginNormalized.x()},
                                          {QStringLiteral("y"), config.customOriginNormalized.y()}});
        result.payload.insert(QStringLiteral("contrastMode"), config.contrastMode);
        result.payload.insert(QStringLiteral("contrastUsed"), config.contrastMode == QStringLiteral("manual")
                              ? QJsonValue(config.contrast) : QJsonValue(QStringLiteral("auto")));
        if (actualMinContrast.Length() > 0)
            result.payload.insert(QStringLiteral("minContrastUsed"), actualMinContrast[0].D());
        if (actualLevels.Length() > 0)
            result.payload.insert(QStringLiteral("numLevelsUsed"), actualLevels[0].D());
        result.payload.insert(QStringLiteral("timeoutMsUsed"), config.timeoutMs);
        result.payload.insert(QStringLiteral("modelCacheKey"), config.modelCacheKey);
        result.payload.insert(QStringLiteral("modelCacheHit"), modelCacheHit);
        result.payload.insert(QStringLiteral("modelCachePersisted"), modelCachePersisted);
        result.payload.insert(QStringLiteral("modelCachePath"), modelPath);
        result.payload.insert(QStringLiteral("modelSignature"), QString::fromLatin1(signature));
        result.payload.insert(QStringLiteral("templateRoiNormalized"), rectToJson(config.templateRoiNormalized));
        result.payload.insert(QStringLiteral("searchRoiNormalized"), rectToJson(config.searchRoiNormalized));
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));

        ClearShapeModel(modelId);
        modelCreated = false;
        return result;
    } catch (const HException &exception) {
        if (modelCreated) {
            try { ClearShapeModel(modelId); } catch (...) {}
        }
        return errorResult(QStringLiteral("halcon_error"),
                           QStringLiteral("HALCON %1: %2")
                           .arg(static_cast<qlonglong>(exception.ErrorCode()))
                           .arg(QString::fromUtf8(exception.ErrorMessage().Text())),
                           timer.elapsed());
    } catch (const std::exception &exception) {
        if (modelCreated) {
            try { ClearShapeModel(modelId); } catch (...) {}
        }
        return errorResult(QStringLiteral("execution_error"),
                           QString::fromLocal8Bit(exception.what()), timer.elapsed());
    }
}
