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
#include <QSharedPointer>
#include <QStandardPaths>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

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
        {QStringLiteral("templateMaskRegionType"), config.templateMaskRegionType},
        {QStringLiteral("templateMaskRoiNormalized"),
         rectToJson(config.templateMaskRoiNormalized)},
        {QStringLiteral("templateMaskPolygonNormalized"),
         pointsToJson(config.templateMaskPolygonNormalized)},
        {QStringLiteral("templateMaskCircleCenterNormalized"),
         QJsonObject{{QStringLiteral("x"), config.templateMaskCircleCenterNormalized.x()},
                     {QStringLiteral("y"), config.templateMaskCircleCenterNormalized.y()}}},
        {QStringLiteral("templateMaskCircleRadiusNormalized"),
         config.templateMaskCircleRadiusNormalized},
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

HTuple localRowsTuple(const QVector<QPointF> &points, int imageHeight, const QRect &crop)
{
    HTuple rows;
    for (int i = 0; i < points.size(); ++i)
        rows[i] = points.at(i).y() * imageHeight - crop.y();
    return rows;
}

HTuple localColumnsTuple(const QVector<QPointF> &points, int imageWidth, const QRect &crop)
{
    HTuple columns;
    for (int i = 0; i < points.size(); ++i)
        columns[i] = points.at(i).x() * imageWidth - crop.x();
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
                      const QPointF &searchOffset,
                      const QJsonObject &identity = QJsonObject())
{
    const auto applyIdentity = [&identity](ToolOverlay *overlay) {
        if (!overlay)
            return;
        for (auto it = identity.constBegin(); it != identity.constEnd(); ++it)
            overlay->extra.insert(it.key(), it.value());
    };
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
        applyIdentity(&line);
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
    applyIdentity(&horizontal);
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
    const QString templateName = identity.value(
                QStringLiteral("templateName")).toString().trimmed();
    if (!templateName.isEmpty())
        scoreText.text += QStringLiteral("  %1").arg(templateName);
    scoreText.extra.insert(QStringLiteral("matchIndex"), index);
    applyIdentity(&scoreText);
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
    return run(image, referenceImage,
               TemplateLocationConfig::modelBankFromScalar(config));
}

TemplateLocationHalconResult TemplateLocationHalconRunner::run(
        const cv::Mat &image,
        const cv::Mat &referenceImage,
        const TemplateLocationModelBankConfig &inputConfig)
{
    QElapsedTimer timer;
    timer.start();
    if (image.empty())
        return errorResult(QStringLiteral("image_empty"),
                           QStringLiteral("input image is empty"), timer.elapsed());
    if (referenceImage.empty())
        return errorResult(QStringLiteral("no_reference_image"),
                           QStringLiteral("reference image is empty"), timer.elapsed());
    if (image.depth() != CV_8U || referenceImage.depth() != CV_8U ||
            (image.channels() != 1 && image.channels() != 3 && image.channels() != 4) ||
            (referenceImage.channels() != 1 && referenceImage.channels() != 3 &&
             referenceImage.channels() != 4)) {
        return errorResult(QStringLiteral("unsupported_image_type"),
                           QStringLiteral("TemplateLocation supports CV_8UC1, CV_8UC3 and CV_8UC4 images"),
                           timer.elapsed());
    }

    // Execution is strict: v5 identity repair belongs to an editor/migration
    // path that can persist the change.  Generating UUIDs here would make an
    // invalid runtime config change identity and cache path on every frame.
    const TemplateLocationModelBankConfig config = inputConfig;
    const TemplateLocationConfigValidationResult validation =
            TemplateLocationConfig::validateModelBank(config, false);
    if (!validation.valid)
        return errorResult(validation.code, validation.message, timer.elapsed());
    if (!QFileInfo::exists(config.halconSoPath))
        return errorResult(QStringLiteral("halcon_runtime_missing"),
                           QStringLiteral("HALCON runtime is unavailable"), timer.elapsed());

    const QRect searchRect = pixelRect(config.searchRoiNormalized,
                                       image.cols, image.rows);
    if (searchRect.isEmpty())
        return errorResult(QStringLiteral("invalid_search_region"),
                           QStringLiteral("search ROI is invalid"), timer.elapsed());

    struct PreparedModel {
        TemplateLocationTemplateConfig item;
        int sourceIndex = -1;
        QRect templateRect;
        QString maskType;
        bool maskApplied = false;
        double baseArea = 0.0;
        double effectiveArea = 0.0;
        double originDeltaRow = 0.0;
        double originDeltaColumn = 0.0;
        HTuple modelId;
        QSharedPointer<HObject> contours;
        QByteArray signature;
        QString modelPath;
        bool cacheHit = false;
        bool cachePersisted = false;
        int rawMatchCount = 0;
        HTuple actualLevels;
        HTuple actualMinContrast;
    };
    struct BankMatch {
        int preparedIndex = -1;
        int templateMatchIndex = -1;
        QString matchId;
        double localRow = 0.0;
        double localColumn = 0.0;
        double modelGlobalRow = 0.0;
        double modelGlobalColumn = 0.0;
        double row = 0.0;
        double column = 0.0;
        double angle = 0.0;
        double scale = 1.0;
        double score = 0.0;
        QStringList sourceTemplateIds;
        QStringList sourceMatchIds;
        QVector<int> sourceRawIndices;
    };
    class ShapeModelGuard
    {
    public:
        ~ShapeModelGuard()
        {
            for (const HTuple &id : m_ids) {
                try { ClearShapeModel(id); } catch (...) {}
            }
        }
        void add(const HTuple &id) { m_ids.append(id); }
    private:
        QVector<HTuple> m_ids;
    } guard;

    QVector<PreparedModel> prepared;
    QString activeTemplate;
    try {
        const cv::Mat referenceGray = grayImage(referenceImage);
        const cv::Mat imageGray = grayImage(image);
        const HObject referenceHalcon = halconByteImage(referenceGray);
        const HObject imageHalcon = halconByteImage(imageGray);
        const HObject searchDomain = croppedDomain(
                    imageHalcon, searchRect, config.searchRegionType,
                    config.searchPolygonNormalized,
                    config.searchCircleCenterNormalized,
                    config.searchCircleRadiusNormalized,
                    image.cols, image.rows);

        const double angleStart = config.angleStart * kPi / 180.0;
        const double angleExtent = config.angleExtent * kPi / 180.0;
        const double scaleMin = config.scaleMin / 100.0;
        const double scaleMax = config.scaleMax / 100.0;
        const HTuple levels = config.numLevels > 0
                ? HTuple(config.numLevels) : HTuple("auto");
        const HTuple contrast = config.contrastMode == QStringLiteral("manual")
                ? HTuple(config.contrast) : HTuple("auto");
        const HTuple minContrast = config.contrastMode == QStringLiteral("manual")
                ? HTuple(config.minContrast) : HTuple("auto");

        prepared.reserve(TemplateLocationConfig::enabledTemplateCount(config));
        for (int sourceIndex = 0; sourceIndex < config.templates.size(); ++sourceIndex) {
            const TemplateLocationTemplateConfig &item = config.templates.at(sourceIndex);
            if (!item.enabled)
                continue;
            activeTemplate = item.name.trimmed().isEmpty()
                    ? item.templateId : item.name;

            PreparedModel model;
            model.item = item;
            model.sourceIndex = sourceIndex;
            model.maskType = item.templateMaskRegionType.trimmed().toLower();
            if ((model.maskType.isEmpty() || model.maskType == QStringLiteral("none")) &&
                    !item.templateMaskPolygonNormalized.isEmpty()) {
                model.maskType = QStringLiteral("polygon");
            }
            if (model.maskType.isEmpty())
                model.maskType = QStringLiteral("none");
            model.maskApplied = model.maskType != QStringLiteral("none");
            model.templateRect = pixelRect(item.templateRoiNormalized,
                                           referenceImage.cols,
                                           referenceImage.rows);
            if (model.templateRect.isEmpty()) {
                return errorResult(QStringLiteral("invalid_template_region"),
                                   QStringLiteral("%1: template ROI is invalid")
                                   .arg(activeTemplate), timer.elapsed());
            }

            TemplateLocationHalconConfig scalar =
                    TemplateLocationConfig::scalarConfigForTemplate(config, item);
            const HObject templateBaseDomain = croppedDomain(
                        referenceHalcon, model.templateRect,
                        item.templateRegionType, item.templatePolygonNormalized,
                        QPointF(), 0.0,
                        referenceImage.cols, referenceImage.rows);
            HObject templateBaseRegion;
            HTuple templateBaseArea;
            HTuple centroidRows;
            HTuple centroidColumns;
            GetDomain(templateBaseDomain, &templateBaseRegion);
            AreaCenter(templateBaseRegion, &templateBaseArea,
                       &centroidRows, &centroidColumns);
            if (centroidRows.Length() < 1 || centroidColumns.Length() < 1) {
                return errorResult(QStringLiteral("invalid_template_region"),
                                   QStringLiteral("%1: template region has no centroid")
                                   .arg(activeTemplate), timer.elapsed());
            }
            model.baseArea = templateBaseArea.Length() > 0
                    ? templateBaseArea[0].D() : 0.0;
            const double centroidRow = centroidRows[0].D();
            const double centroidColumn = centroidColumns[0].D();
            double modelCentroidRow = centroidRow;
            double modelCentroidColumn = centroidColumn;
            HObject templateDomain = templateBaseDomain;
            model.effectiveArea = model.baseArea;

            if (model.maskApplied) {
                HObject maskRegion;
                if (model.maskType == QStringLiteral("rectangle")) {
                    GenRectangle1(&maskRegion,
                                  item.templateMaskRoiNormalized.top() * referenceImage.rows - model.templateRect.y(),
                                  item.templateMaskRoiNormalized.left() * referenceImage.cols - model.templateRect.x(),
                                  item.templateMaskRoiNormalized.bottom() * referenceImage.rows - model.templateRect.y(),
                                  item.templateMaskRoiNormalized.right() * referenceImage.cols - model.templateRect.x());
                } else if (model.maskType == QStringLiteral("circle")) {
                    GenCircle(&maskRegion,
                              item.templateMaskCircleCenterNormalized.y() * referenceImage.rows - model.templateRect.y(),
                              item.templateMaskCircleCenterNormalized.x() * referenceImage.cols - model.templateRect.x(),
                              item.templateMaskCircleRadiusNormalized * qMax(referenceImage.cols, referenceImage.rows));
                } else {
                    GenRegionPolygonFilled(
                                &maskRegion,
                                localRowsTuple(item.templateMaskPolygonNormalized,
                                               referenceImage.rows, model.templateRect),
                                localColumnsTuple(item.templateMaskPolygonNormalized,
                                                  referenceImage.cols, model.templateRect));
                }
                HObject clippedMask;
                Intersection(templateBaseRegion, maskRegion, &clippedMask);
                HTuple maskArea;
                HTuple maskRow;
                HTuple maskColumn;
                AreaCenter(clippedMask, &maskArea, &maskRow, &maskColumn);
                if (maskArea.Length() < 1 || maskArea[0].D() < 1.0) {
                    return errorResult(QStringLiteral("invalid_template_mask"),
                                       QStringLiteral("%1: template mask does not overlap the template region")
                                       .arg(activeTemplate), timer.elapsed());
                }
                if (maskArea[0].D() >= model.baseArea * 0.95) {
                    return errorResult(QStringLiteral("template_masked_empty"),
                                       QStringLiteral("%1: template mask leaves too little usable template area")
                                       .arg(activeTemplate), timer.elapsed());
                }
                HObject effectiveRegion;
                Difference(templateBaseRegion, clippedMask, &effectiveRegion);
                HTuple effectiveArea;
                HTuple effectiveRow;
                HTuple effectiveColumn;
                AreaCenter(effectiveRegion, &effectiveArea,
                           &effectiveRow, &effectiveColumn);
                if (effectiveArea.Length() < 1 ||
                        effectiveArea[0].D() < qMax(4.0, model.baseArea * 0.05)) {
                    return errorResult(QStringLiteral("template_masked_empty"),
                                       QStringLiteral("%1: template mask leaves too little usable template area")
                                       .arg(activeTemplate), timer.elapsed());
                }
                model.effectiveArea = effectiveArea[0].D();
                modelCentroidRow = effectiveRow[0].D();
                modelCentroidColumn = effectiveColumn[0].D();
                ReduceDomain(templateBaseDomain, effectiveRegion, &templateDomain);
            }

            model.originDeltaRow = centroidRow - modelCentroidRow;
            model.originDeltaColumn = centroidColumn - modelCentroidColumn;
            if (config.originMode == QStringLiteral("custom")) {
                model.originDeltaRow = config.customOriginNormalized.y() * referenceImage.rows
                        - model.templateRect.y() - modelCentroidRow;
                model.originDeltaColumn = config.customOriginNormalized.x() * referenceImage.cols
                        - model.templateRect.x() - modelCentroidColumn;
            }

            model.signature = modelSignature(referenceGray, scalar);
            const QString stem = cacheStem(item.modelCacheKey);
            model.modelPath = stem + QStringLiteral(".shm");
            const QString signaturePath = stem + QStringLiteral(".sha256");
            bool modelCreated = false;
            {
                QMutexLocker cacheLocker(&modelCacheMutex);
                QFile signatureFile(signaturePath);
                if (QFileInfo::exists(model.modelPath) &&
                        signatureFile.open(QIODevice::ReadOnly) &&
                        signatureFile.readAll() == model.signature) {
                    try {
                        ReadShapeModel(model.modelPath.toLocal8Bit().constData(),
                                       &model.modelId);
                        modelCreated = true;
                        model.cacheHit = true;
                    } catch (const HException &) {
                        QFile::remove(model.modelPath);
                        QFile::remove(signaturePath);
                    }
                }
                if (!modelCreated) {
                    CreateScaledShapeModel(
                                templateDomain, levels,
                                angleStart, angleExtent, HTuple("auto"),
                                scaleMin, scaleMax, HTuple("auto"),
                                HTuple("auto"),
                                metricForPolarity(config.polarity).toLatin1().constData(),
                                contrast, minContrast, &model.modelId);
                    modelCreated = true;
                    try {
                        WriteShapeModel(model.modelId,
                                        model.modelPath.toLocal8Bit().constData());
                        model.cachePersisted = writeSignature(signaturePath,
                                                              model.signature);
                    } catch (const HException &) {
                        QFile::remove(model.modelPath);
                        QFile::remove(signaturePath);
                    }
                } else {
                    model.cachePersisted = true;
                }
            }
            guard.add(model.modelId);
            if (config.timeoutMs > 0)
                SetShapeModelParam(model.modelId, "timeout", config.timeoutMs);

            HTuple actualAngleStart;
            HTuple actualAngleExtent;
            HTuple actualAngleStep;
            HTuple actualScaleMin;
            HTuple actualScaleMax;
            HTuple actualScaleStep;
            HTuple actualMetric;
            GetShapeModelParams(model.modelId,
                                &model.actualLevels,
                                &actualAngleStart, &actualAngleExtent,
                                &actualAngleStep, &actualScaleMin,
                                &actualScaleMax, &actualScaleStep,
                                &actualMetric, &model.actualMinContrast);
            model.contours.reset(new HObject);
            GetShapeModelContours(model.contours.data(), model.modelId, 1);
            prepared.append(model);
        }
        // Exceptions below belong to the bank search/fusion phase, not to the
        // last model that happened to be prepared.
        activeTemplate.clear();

        HTuple modelIds;
        HTuple angleStarts;
        HTuple angleExtents;
        HTuple scaleMins;
        HTuple scaleMaxs;
        HTuple minScores;
        HTuple numMatches;
        HTuple maxOverlaps;
        for (const PreparedModel &model : prepared) {
            modelIds.Append(model.modelId[0]);
            angleStarts.Append(angleStart);
            angleExtents.Append(angleExtent);
            scaleMins.Append(scaleMin);
            scaleMaxs.Append(scaleMax);
            minScores.Append(config.minScore / 100.0);
            numMatches.Append(config.maxMatches);
            // A tuple applies overlap suppression within each model. Cross-model
            // duplicates are fused below using the common public origin.
            maxOverlaps.Append(config.maxOverlap);
        }

        HTuple rows;
        HTuple columns;
        HTuple angles;
        HTuple scales;
        HTuple scores;
        HTuple models;
        FindScaledShapeModels(
                    searchDomain, modelIds,
                    angleStarts, angleExtents,
                    scaleMins, scaleMaxs,
                    minScores, numMatches, maxOverlaps,
                    HTuple(config.subPixel == QStringLiteral("none")
                           ? "none" : "least_squares"),
                    HTuple(config.numLevels > 0 ? config.numLevels : 0),
                    HTuple(config.greediness),
                    &rows, &columns, &angles, &scales, &scores, &models);

        const Hlong tupleCount = qMin(
                    qMin(qMin(rows.Length(), columns.Length()),
                         qMin(angles.Length(), scales.Length())),
                    qMin(scores.Length(), models.Length()));
        QVector<int> perTemplateMatchIndex(prepared.size(), 0);
        QVector<BankMatch> rawMatches;
        rawMatches.reserve(static_cast<int>(tupleCount));
        for (Hlong tupleIndex = 0; tupleIndex < tupleCount; ++tupleIndex) {
            const int modelIndex = static_cast<int>(models[tupleIndex].L());
            if (modelIndex < 0 || modelIndex >= prepared.size())
                continue;
            PreparedModel &model = prepared[modelIndex];
            BankMatch match;
            match.preparedIndex = modelIndex;
            match.templateMatchIndex = perTemplateMatchIndex[modelIndex]++;
            ++model.rawMatchCount;
            match.matchId = QStringLiteral("%1/%2")
                    .arg(model.item.templateId)
                    .arg(match.templateMatchIndex);
            match.localRow = rows[tupleIndex].D();
            match.localColumn = columns[tupleIndex].D();
            match.modelGlobalRow = match.localRow + searchRect.y();
            match.modelGlobalColumn = match.localColumn + searchRect.x();
            match.angle = angles[tupleIndex].D();
            match.scale = scales[tupleIndex].D();
            match.score = scores[tupleIndex].D();
            HTuple anchorTransform;
            HTuple scaledAnchorTransform;
            HTuple anchorRows;
            HTuple anchorColumns;
            VectorAngleToRigid(0.0, 0.0, 0.0,
                               match.localRow, match.localColumn, match.angle,
                               &anchorTransform);
            HomMat2dScaleLocal(anchorTransform, match.scale, match.scale,
                               &scaledAnchorTransform);
            AffineTransPoint2d(scaledAnchorTransform,
                               model.originDeltaRow, model.originDeltaColumn,
                               &anchorRows, &anchorColumns);
            match.row = anchorRows[0].D() + searchRect.y();
            match.column = anchorColumns[0].D() + searchRect.x();
            match.sourceTemplateIds.append(model.item.templateId);
            match.sourceMatchIds.append(match.matchId);
            rawMatches.append(match);
        }

        std::stable_sort(rawMatches.begin(), rawMatches.end(),
                         [&prepared](const BankMatch &left,
                                     const BankMatch &right) {
            if (left.score != right.score)
                return left.score > right.score;
            const PreparedModel &leftModel = prepared.at(left.preparedIndex);
            const PreparedModel &rightModel = prepared.at(right.preparedIndex);
            if (leftModel.item.priority != rightModel.item.priority)
                return leftModel.item.priority < rightModel.item.priority;
            if (leftModel.item.templateId != rightModel.item.templateId)
                return leftModel.item.templateId < rightModel.item.templateId;
            if (left.column != right.column)
                return left.column < right.column;
            return left.row < right.row;
        });

        const auto angleDistance = [](double left, double right) {
            double distance = std::fmod(std::abs(left - right) * 180.0 / kPi,
                                        360.0);
            if (distance > 180.0)
                distance = 360.0 - distance;
            return distance;
        };
        QVector<BankMatch> fusedMatches;
        for (int rawIndex = 0; rawIndex < rawMatches.size(); ++rawIndex) {
            const BankMatch &candidate = rawMatches.at(rawIndex);
            bool fused = false;
            if (config.fusion.enabled) {
                for (BankMatch &accepted : fusedMatches) {
                    const QString candidateTemplateId = prepared.at(
                                candidate.preparedIndex).item.templateId;
                    if (accepted.sourceTemplateIds.contains(candidateTemplateId))
                        continue;
                    if (std::hypot(accepted.column - candidate.column,
                                   accepted.row - candidate.row)
                            > config.fusion.positionTolerancePx ||
                            angleDistance(accepted.angle, candidate.angle)
                            > config.fusion.angleToleranceDeg ||
                            std::abs(accepted.scale - candidate.scale)
                            > config.fusion.scaleTolerance) {
                        continue;
                    }
                    accepted.sourceTemplateIds.append(candidateTemplateId);
                    accepted.sourceMatchIds.append(candidate.matchId);
                    accepted.sourceRawIndices.append(rawIndex);
                    fused = true;
                    break;
                }
            }
            if (!fused) {
                BankMatch accepted = candidate;
                accepted.sourceRawIndices.append(rawIndex);
                fusedMatches.append(accepted);
            }
        }

        const auto adoptRepresentative = [](BankMatch *aggregate,
                                            const BankMatch &representative) {
            if (!aggregate)
                return;
            const QStringList sourceTemplateIds = aggregate->sourceTemplateIds;
            const QStringList sourceMatchIds = aggregate->sourceMatchIds;
            const QVector<int> sourceRawIndices = aggregate->sourceRawIndices;
            *aggregate = representative;
            aggregate->sourceTemplateIds = sourceTemplateIds;
            aggregate->sourceMatchIds = sourceMatchIds;
            aggregate->sourceRawIndices = sourceRawIndices;
        };
        // The representative pose and its template identity must come from the
        // same raw match.  Rebase each fused cluster before ordering it by the
        // configured primary strategy.
        if (config.primaryMatchStrategy == QStringLiteral("template_priority")) {
            for (BankMatch &aggregate : fusedMatches) {
                int selectedRawIndex = -1;
                int selectedPriority = std::numeric_limits<int>::max();
                double selectedScore = -1.0;
                QString selectedId;
                for (const int rawIndex : aggregate.sourceRawIndices) {
                    const BankMatch &candidate = rawMatches.at(rawIndex);
                    const PreparedModel &candidateModel = prepared.at(
                                candidate.preparedIndex);
                    if (candidateModel.item.priority < selectedPriority ||
                            (candidateModel.item.priority == selectedPriority &&
                             (candidate.score > selectedScore ||
                              (candidate.score == selectedScore &&
                               candidateModel.item.templateId < selectedId)))) {
                        selectedRawIndex = rawIndex;
                        selectedPriority = candidateModel.item.priority;
                        selectedScore = candidate.score;
                        selectedId = candidateModel.item.templateId;
                    }
                }
                if (selectedRawIndex >= 0)
                    adoptRepresentative(&aggregate,
                                        rawMatches.at(selectedRawIndex));
            }
        } else if (config.primaryMatchStrategy == QStringLiteral("locked_template")) {
            for (BankMatch &aggregate : fusedMatches) {
                for (const int rawIndex : aggregate.sourceRawIndices) {
                    const BankMatch &candidate = rawMatches.at(rawIndex);
                    if (prepared.at(candidate.preparedIndex).item.templateId ==
                            config.primaryTemplateId) {
                        adoptRepresentative(&aggregate, candidate);
                        break;
                    }
                }
            }
        }

        const auto bestSourcePriority = [&prepared](const BankMatch &match) {
            int priority = std::numeric_limits<int>::max();
            for (const QString &templateId : match.sourceTemplateIds) {
                for (const PreparedModel &model : prepared) {
                    if (model.item.templateId == templateId)
                        priority = qMin(priority, model.item.priority);
                }
            }
            return priority;
        };
        if (config.primaryMatchStrategy == QStringLiteral("template_priority")) {
            std::stable_sort(fusedMatches.begin(), fusedMatches.end(),
                             [&bestSourcePriority](const BankMatch &left,
                                                   const BankMatch &right) {
                const int leftPriority = bestSourcePriority(left);
                const int rightPriority = bestSourcePriority(right);
                return leftPriority == rightPriority
                        ? left.score > right.score
                        : leftPriority < rightPriority;
            });
        } else if (config.primaryMatchStrategy == QStringLiteral("locked_template")) {
            std::stable_sort(fusedMatches.begin(), fusedMatches.end(),
                             [&config](const BankMatch &left,
                                       const BankMatch &right) {
                const bool leftLocked = left.sourceTemplateIds.contains(
                            config.primaryTemplateId);
                const bool rightLocked = right.sourceTemplateIds.contains(
                            config.primaryTemplateId);
                return leftLocked == rightLocked
                        ? left.score > right.score : leftLocked;
            });
        }
        if (fusedMatches.size() > config.maxMatches)
            fusedMatches.resize(config.maxMatches);

        int primaryIndex = fusedMatches.isEmpty() ? -1 : 0;
        if (config.primaryMatchStrategy == QStringLiteral("locked_template") &&
                (fusedMatches.isEmpty() ||
                 !fusedMatches.first().sourceTemplateIds.contains(
                     config.primaryTemplateId))) {
            primaryIndex = -1;
        }
        QString selectedTemplateId;
        if (primaryIndex >= 0) {
            const BankMatch &primary = fusedMatches.at(primaryIndex);
            selectedTemplateId = prepared.at(
                        primary.preparedIndex).item.templateId;
        }
        QString selectedTemplateName;
        for (const PreparedModel &model : prepared) {
            if (model.item.templateId == selectedTemplateId) {
                selectedTemplateName = model.item.name;
                break;
            }
        }

        QCryptographicHash modelSetHash(QCryptographicHash::Sha256);
        QJsonArray templateResults;
        bool allCacheHits = true;
        bool allCachesPersisted = true;
        for (const PreparedModel &model : prepared) {
            modelSetHash.addData(model.item.templateId.toUtf8());
            modelSetHash.addData("\0", 1);
            modelSetHash.addData(model.signature);
            modelSetHash.addData("\0", 1);
            allCacheHits = allCacheHits && model.cacheHit;
            allCachesPersisted = allCachesPersisted && model.cachePersisted;
            templateResults.append(QJsonObject{
                {QStringLiteral("templateId"), model.item.templateId},
                {QStringLiteral("templateName"), model.item.name},
                {QStringLiteral("templateIndex"), model.sourceIndex},
                {QStringLiteral("priority"), model.item.priority},
                {QStringLiteral("rawFoundCount"), model.rawMatchCount},
                {QStringLiteral("modelCacheHit"), model.cacheHit},
                {QStringLiteral("modelCachePersisted"), model.cachePersisted},
                {QStringLiteral("modelCachePath"), model.modelPath},
                {QStringLiteral("modelSignature"), QString::fromLatin1(model.signature)},
                {QStringLiteral("templateMaskApplied"), model.maskApplied},
                {QStringLiteral("templateBaseArea"), model.baseArea},
                {QStringLiteral("templateEffectiveArea"), model.effectiveArea}
            });
        }
        const QString modelSetSignature = QString::fromLatin1(
                    modelSetHash.result().toHex());

        TemplateLocationHalconResult result;
        result.success = true;
        result.count = fusedMatches.size();
        const bool found = primaryIndex >= 0;
        result.ok = found && result.count >= config.minMatchCount &&
                result.count <= config.maxMatchCount;
        result.status = !found ? QStringLiteral("not_found")
                               : (result.ok ? QStringLiteral("found")
                                            : QStringLiteral("count_out_of_range"));
        result.message = !found
                ? (rawMatches.isEmpty()
                   ? QStringLiteral("TemplateLocation: no template reached the minimum score")
                   : QStringLiteral("TemplateLocation: the locked template was not found"))
                : (result.ok
                   ? QStringLiteral("TemplateLocation: found %1 accepted match(es) from %2 template(s)")
                     .arg(result.count).arg(prepared.size())
                   : QStringLiteral("TemplateLocation: found count %1 is outside %2..%3")
                     .arg(result.count).arg(config.minMatchCount)
                     .arg(config.maxMatchCount));
        result.score = primaryIndex >= 0
                ? fusedMatches.at(primaryIndex).score : 0.0;
        addRegionOverlay(&result.overlays, config.searchRegionType,
                         config.searchRoiNormalized,
                         config.searchPolygonNormalized,
                         config.searchCircleCenterNormalized,
                         config.searchCircleRadiusNormalized,
                         image.cols, image.rows,
                         QStringLiteral("detect_roi"));

        QJsonArray matches;
        for (int index = 0; index < fusedMatches.size(); ++index) {
            const BankMatch &match = fusedMatches.at(index);
            const PreparedModel &model = prepared.at(match.preparedIndex);
            const bool isPrimary = index == primaryIndex;
            QJsonArray sourceTemplateIds;
            for (const QString &id : match.sourceTemplateIds)
                sourceTemplateIds.append(id);
            QJsonArray sourceMatchIds;
            for (const QString &id : match.sourceMatchIds)
                sourceMatchIds.append(id);
            const QJsonObject matchJson{
                {QStringLiteral("index"), index},
                {QStringLiteral("matchId"), match.matchId},
                {QStringLiteral("templateId"), model.item.templateId},
                {QStringLiteral("templateName"), model.item.name},
                {QStringLiteral("templateIndex"), model.sourceIndex},
                {QStringLiteral("templateMatchIndex"), match.templateMatchIndex},
                {QStringLiteral("templatePriority"), model.item.priority},
                {QStringLiteral("isPrimary"), isPrimary},
                {QStringLiteral("fused"), match.sourceTemplateIds.size() > 1},
                {QStringLiteral("fusedSourceTemplateIds"), sourceTemplateIds},
                {QStringLiteral("fusedSourceMatchIds"), sourceMatchIds},
                {QStringLiteral("templateModelSignature"),
                 QString::fromLatin1(model.signature)},
                {QStringLiteral("x"), match.column},
                {QStringLiteral("y"), match.row},
                {QStringLiteral("modelX"), match.modelGlobalColumn},
                {QStringLiteral("modelY"), match.modelGlobalRow},
                {QStringLiteral("angleDeg"), match.angle * 180.0 / kPi},
                {QStringLiteral("angle"), match.angle * 180.0 / kPi},
                {QStringLiteral("scale"), match.scale},
                {QStringLiteral("score"), match.score}
            };
            matches.append(matchJson);
            const QJsonObject overlayIdentity{
                {QStringLiteral("matchId"), match.matchId},
                {QStringLiteral("templateId"), model.item.templateId},
                {QStringLiteral("templateName"), model.item.name},
                {QStringLiteral("templateIndex"), model.sourceIndex},
                {QStringLiteral("isPrimary"), isPrimary}
            };
            addMatchOverlays(&result.overlays, *model.contours,
                             match.localRow, match.localColumn,
                             match.row, match.column,
                             match.angle, match.scale, match.score, index,
                             QPointF(searchRect.x(), searchRect.y()),
                             overlayIdentity);
        }

        const QJsonObject primaryPose = primaryIndex >= 0
                ? matches.at(primaryIndex).toObject() : QJsonObject();
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("found"), found);
        result.payload.insert(QStringLiteral("countAccepted"), result.ok);
        result.payload.insert(QStringLiteral("foundCount"), result.count);
        result.payload.insert(QStringLiteral("rawFoundCount"), rawMatches.size());
        result.payload.insert(QStringLiteral("matches"), matches);
        result.payload.insert(QStringLiteral("x"), primaryPose.isEmpty()
                              ? QJsonValue(-1.0) : primaryPose.value(QStringLiteral("x")));
        result.payload.insert(QStringLiteral("y"), primaryPose.isEmpty()
                              ? QJsonValue(-1.0) : primaryPose.value(QStringLiteral("y")));
        result.payload.insert(QStringLiteral("angleDeg"), primaryPose.isEmpty()
                              ? QJsonValue(0.0) : primaryPose.value(QStringLiteral("angleDeg")));
        result.payload.insert(QStringLiteral("angle"), primaryPose.isEmpty()
                              ? QJsonValue(0.0) : primaryPose.value(QStringLiteral("angleDeg")));
        result.payload.insert(QStringLiteral("scale"), primaryPose.isEmpty()
                              ? QJsonValue(1.0) : primaryPose.value(QStringLiteral("scale")));
        result.payload.insert(QStringLiteral("primaryMatchIndex"), primaryIndex);
        result.payload.insert(QStringLiteral("pose"), primaryPose);
        result.payload.insert(QStringLiteral("coordinateSystem"), QStringLiteral("image_pixel"));
        result.payload.insert(QStringLiteral("angleUnit"), QStringLiteral("degree"));
        result.payload.insert(QStringLiteral("score"), result.score);
        result.payload.insert(QStringLiteral("templateMode"), config.templateMode);
        result.payload.insert(QStringLiteral("templateCount"), config.templates.size());
        result.payload.insert(QStringLiteral("enabledTemplateCount"), prepared.size());
        result.payload.insert(QStringLiteral("templateResults"), templateResults);
        result.payload.insert(QStringLiteral("selectedTemplateId"), selectedTemplateId);
        result.payload.insert(QStringLiteral("selectedTemplateName"), selectedTemplateName);
        result.payload.insert(QStringLiteral("primaryMatchStrategy"), config.primaryMatchStrategy);
        if (config.primaryMatchStrategy == QStringLiteral("locked_template"))
            result.payload.insert(QStringLiteral("lockedTemplateId"), config.primaryTemplateId);
        result.payload.insert(QStringLiteral("modelSetSignature"), modelSetSignature);
        result.payload.insert(QStringLiteral("modelSignature"), prepared.size() == 1
                              ? QString::fromLatin1(prepared.first().signature)
                              : modelSetSignature);
        result.payload.insert(QStringLiteral("maxMatches"), config.maxMatches);
        result.payload.insert(QStringLiteral("minMatchCount"), config.minMatchCount);
        result.payload.insert(QStringLiteral("maxMatchCount"), config.maxMatchCount);
        result.payload.insert(QStringLiteral("maxOverlap"), config.maxOverlap);
        result.payload.insert(QStringLiteral("originMode"), config.originMode);
        result.payload.insert(QStringLiteral("customOriginNormalized"),
                              QJsonObject{{QStringLiteral("x"), config.customOriginNormalized.x()},
                                          {QStringLiteral("y"), config.customOriginNormalized.y()}});
        result.payload.insert(QStringLiteral("contrastMode"), config.contrastMode);
        result.payload.insert(QStringLiteral("contrastUsed"),
                              config.contrastMode == QStringLiteral("manual")
                              ? QJsonValue(config.contrast)
                              : QJsonValue(QStringLiteral("auto")));
        if (!prepared.isEmpty() && prepared.first().actualMinContrast.Length() > 0)
            result.payload.insert(QStringLiteral("minContrastUsed"),
                                  prepared.first().actualMinContrast[0].D());
        if (!prepared.isEmpty() && prepared.first().actualLevels.Length() > 0)
            result.payload.insert(QStringLiteral("numLevelsUsed"),
                                  prepared.first().actualLevels[0].D());
        result.payload.insert(QStringLiteral("timeoutMsUsed"), config.timeoutMs);
        result.payload.insert(QStringLiteral("modelCacheHit"), allCacheHits);
        result.payload.insert(QStringLiteral("modelCachePersisted"), allCachesPersisted);
        if (prepared.size() == 1) {
            const PreparedModel &model = prepared.first();
            result.payload.insert(QStringLiteral("modelCacheKey"), model.item.modelCacheKey);
            result.payload.insert(QStringLiteral("modelCachePath"), model.modelPath);
            result.payload.insert(QStringLiteral("templateRoiNormalized"),
                                  rectToJson(model.item.templateRoiNormalized));
            result.payload.insert(QStringLiteral("templateMaskApplied"), model.maskApplied);
            result.payload.insert(QStringLiteral("templateMaskRegionType"), model.maskType);
            result.payload.insert(QStringLiteral("templateMaskRoiNormalized"),
                                  rectToJson(model.item.templateMaskRoiNormalized));
            result.payload.insert(QStringLiteral("templateMaskPolygonNormalized"),
                                  pointsToJson(model.item.templateMaskPolygonNormalized));
            result.payload.insert(QStringLiteral("templateMaskCircleCenterNormalized"),
                                  QJsonObject{{QStringLiteral("x"), model.item.templateMaskCircleCenterNormalized.x()},
                                              {QStringLiteral("y"), model.item.templateMaskCircleCenterNormalized.y()}});
            result.payload.insert(QStringLiteral("templateMaskCircleRadiusNormalized"),
                                  model.item.templateMaskCircleRadiusNormalized);
            result.payload.insert(QStringLiteral("templateBaseArea"), model.baseArea);
            result.payload.insert(QStringLiteral("templateEffectiveArea"), model.effectiveArea);
        }
        result.payload.insert(QStringLiteral("searchRoiNormalized"),
                              rectToJson(config.searchRoiNormalized));
        result.payload.insert(QStringLiteral("elapsedMs"),
                              static_cast<double>(result.elapsedMs));
        return result;
    } catch (const HException &exception) {
        return errorResult(
                    QStringLiteral("halcon_error"),
                    QStringLiteral("%1HALCON %2: %3")
                    .arg(activeTemplate.isEmpty()
                         ? QString() : QStringLiteral("%1: ").arg(activeTemplate))
                    .arg(static_cast<qlonglong>(exception.ErrorCode()))
                    .arg(QString::fromUtf8(exception.ErrorMessage().Text())),
                    timer.elapsed());
    } catch (const std::exception &exception) {
        return errorResult(QStringLiteral("execution_error"),
                           QString::fromLocal8Bit(exception.what()), timer.elapsed());
    }
}
