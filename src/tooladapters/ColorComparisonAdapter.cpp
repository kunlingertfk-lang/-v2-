#include "tooladapters/ColorComparisonAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/PositionCorrectionConsumer.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr double kGeometryEpsilon = 1e-12;
constexpr double kMaxExactJsonInteger = 9007199254740991.0;

struct ParseResult
{
    bool success = false;
    QString status;
    QString message;
    ColorComparisonHalconConfig config;
};

ParseResult parseFailure(const QString &status, const QString &message)
{
    ParseResult result;
    result.status = status;
    result.message = message;
    return result;
}

bool finiteNumber(const QJsonValue &value, double *number)
{
    if (!value.isDouble())
        return false;
    const double parsed = value.toDouble();
    if (!std::isfinite(parsed))
        return false;
    if (number)
        *number = parsed;
    return true;
}

bool integerNumber(const QJsonValue &value, int *number)
{
    double parsed = 0.0;
    if (!finiteNumber(value, &parsed) || std::floor(parsed) != parsed
            || parsed < static_cast<double>(std::numeric_limits<int>::min())
            || parsed > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    if (number)
        *number = static_cast<int>(parsed);
    return true;
}

struct DialogLifecycleFailure
{
    bool recognized = false;
    QString status;
    QString message;
};

DialogLifecycleFailure restoredDialogLifecycleFailure(
        const QJsonObject &colorComparison,
        ColorComparisonModelState modelState)
{
    const QJsonValue lifecycleValue =
            colorComparison.value(QStringLiteral("dialogLifecycle"));
    if (!lifecycleValue.isObject())
        return {};

    const QJsonObject lifecycle = lifecycleValue.toObject();
    int originVersion = 0;
    if (!integerNumber(lifecycle.value(QStringLiteral("originVersion")),
                       &originVersion)
            || !lifecycle.value(QStringLiteral("status")).isString()
            || !lifecycle.value(QStringLiteral("reason")).isString()) {
        return {};
    }

    const QString status = lifecycle.value(QStringLiteral("status")).toString();
    const QString reason = lifecycle.value(QStringLiteral("reason")).toString();
    if (reason.size() > 1024)
        return {};

    const bool legacyRebuild = originVersion == 1
            && modelState == ColorComparisonModelState::Unsupported
            && status == QStringLiteral("model_rebuild_required");
    const bool legacyStale = originVersion == 1
            && modelState == ColorComparisonModelState::Stale
            && status == QStringLiteral("model_stale");
    const bool unknownUnsupported = originVersion != 1 && originVersion != 2
            && modelState == ColorComparisonModelState::Unsupported
            && status == QStringLiteral("unsupported_model_version");
    if (!legacyRebuild && !legacyStale && !unknownUnsupported)
        return {};

    return {true, status, reason};
}

bool validNormalizedRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y())
            && std::isfinite(rect.width()) && std::isfinite(rect.height())
            && rect.x() >= 0.0 && rect.y() >= 0.0
            && rect.width() > 0.0 && rect.height() > 0.0
            && rect.x() + rect.width() <= 1.0
            && rect.y() + rect.height() <= 1.0;
}

bool parseRect(const QJsonValue &value, QRectF *rect)
{
    if (!value.isObject())
        return false;
    const QJsonObject object = value.toObject();
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    if (!finiteNumber(object.value(QStringLiteral("x")), &x)
            || !finiteNumber(object.value(QStringLiteral("y")), &y)
            || !finiteNumber(object.value(QStringLiteral("width")), &width)
            || !finiteNumber(object.value(QStringLiteral("height")), &height)) {
        return false;
    }
    const QRectF parsed(x, y, width, height);
    if (!validNormalizedRect(parsed))
        return false;
    if (rect)
        *rect = parsed;
    return true;
}

bool parsePoint(const QJsonValue &value, QPointF *point)
{
    if (!value.isObject())
        return false;
    const QJsonObject object = value.toObject();
    double x = 0.0;
    double y = 0.0;
    if (!finiteNumber(object.value(QStringLiteral("x")), &x)
            || !finiteNumber(object.value(QStringLiteral("y")), &y)
            || x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) {
        return false;
    }
    if (point)
        *point = QPointF(x, y);
    return true;
}

bool validCircleForImage(const QPointF &center,
                         double radius,
                         const cv::Mat &image)
{
    if (image.cols <= 0 || image.rows <= 0 || !std::isfinite(center.x())
            || !std::isfinite(center.y()) || !std::isfinite(radius)
            || radius <= 0.0) {
        return false;
    }

    const double centerX = center.x() * static_cast<double>(image.cols);
    const double centerY = center.y() * static_cast<double>(image.rows);
    const double radiusPixels = radius
            * static_cast<double>(std::max(image.cols, image.rows));
    return centerX - radiusPixels >= -kGeometryEpsilon
            && centerX + radiusPixels
                <= static_cast<double>(image.cols) + kGeometryEpsilon
            && centerY - radiusPixels >= -kGeometryEpsilon
            && centerY + radiusPixels
                <= static_cast<double>(image.rows) + kGeometryEpsilon;
}

bool validPolygon(const QVector<QPointF> &points)
{
    if (points.isEmpty())
        return true;
    if (points.size() < 3)
        return false;

    double twiceArea = 0.0;
    for (int index = 0; index < points.size(); ++index) {
        const QPointF &point = points.at(index);
        const QPointF &next = points.at((index + 1) % points.size());
        twiceArea += point.x() * next.y() - next.x() * point.y();
    }
    return std::abs(twiceArea) > kGeometryEpsilon;
}

bool parsePolygon(const QJsonValue &value, QVector<QPointF> *points)
{
    if (!value.isArray())
        return false;
    QVector<QPointF> parsed;
    const QJsonArray array = value.toArray();
    parsed.reserve(array.size());
    for (const QJsonValue &entry : array) {
        QPointF point;
        if (!parsePoint(entry, &point))
            return false;
        parsed.append(point);
    }
    if (!validPolygon(parsed))
        return false;
    if (points)
        *points = parsed;
    return true;
}

bool parseInputSignature(const QJsonObject &runtimeContext,
                         const QString &key,
                         ColorComparisonInputSignature *signature,
                         QString *status,
                         QString *message)
{
    const QJsonValue metadataValue = runtimeContext.value(key);
    if (metadataValue.isUndefined() || metadataValue.isNull())
        return true;
    if (!metadataValue.isObject()) {
        if (status)
            *status = QStringLiteral("unsupported_pixel_format");
        if (message)
            *message = QStringLiteral("Input metadata must be an object.");
        return false;
    }

    const QJsonObject metadata = metadataValue.toObject();
    if (metadata.contains(QStringLiteral("colorMode"))) {
        const QJsonValue value = metadata.value(QStringLiteral("colorMode"));
        if (!value.isString()) {
            if (status)
                *status = QStringLiteral("unsupported_color_input");
            if (message)
                *message = QStringLiteral("Input colorMode must be a string.");
            return false;
        }
        const QString mode = value.toString().trimmed().toLower();
        if (mode != QStringLiteral("color") && mode != QStringLiteral("mono")
                && mode != QStringLiteral("unknown")) {
            if (status)
                *status = QStringLiteral("unsupported_color_input");
            if (message)
                *message = QStringLiteral("Input colorMode is unsupported.");
            return false;
        }
        signature->colorMode = mode;
    }

    if (signature->colorMode == QStringLiteral("mono"))
        return true;

    if (metadata.contains(QStringLiteral("pixelFormat"))) {
        const QJsonValue value = metadata.value(QStringLiteral("pixelFormat"));
        if (!value.isString()) {
            if (status)
                *status = QStringLiteral("unsupported_pixel_format");
            if (message)
                *message = QStringLiteral("Input pixelFormat must be a string.");
            return false;
        }
        signature->pixelFormat = value.toString().trimmed();
    }

    if (metadata.contains(QStringLiteral("originalChannels"))) {
        int channels = 0;
        if (!integerNumber(metadata.value(QStringLiteral("originalChannels")),
                           &channels)
                || channels < 0) {
            if (status)
                *status = QStringLiteral("unsupported_pixel_format");
            if (message)
                *message = QStringLiteral("Input originalChannels is invalid.");
            return false;
        }
    }

    if (metadata.contains(QStringLiteral("originalDepth"))) {
        int depth = -1;
        if (!integerNumber(metadata.value(QStringLiteral("originalDepth")), &depth)) {
            if (status)
                *status = QStringLiteral("unsupported_pixel_format");
            if (message)
                *message = QStringLiteral("Input originalDepth is invalid.");
            return false;
        }
        signature->bitDepth = depth;
    }

    if (metadata.contains(QStringLiteral("whiteBalance")))
        signature->whiteBalance = metadata.value(QStringLiteral("whiteBalance"));
    if (metadata.contains(QStringLiteral("ccm")))
        signature->ccm = metadata.value(QStringLiteral("ccm"));
    if (metadata.contains(QStringLiteral("exposure")))
        signature->exposure = metadata.value(QStringLiteral("exposure"));
    if (metadata.contains(QStringLiteral("gain")))
        signature->gain = metadata.value(QStringLiteral("gain"));
    return true;
}

ToolResult makeError(const ToolConfig &config,
                     const QString &status,
                     const QString &message,
                     const cv::Mat &targetImage,
                     const QJsonObject &payloadPatch = QJsonObject())
{
    int threshold = 0;
    const QJsonValue thresholdValue =
            config.judgeRule.value(QStringLiteral("minScore"));
    if (!integerNumber(thresholdValue, &threshold)
            || threshold < 0 || threshold > 100) {
        threshold = 0;
    }

    const QJsonValue colorComparisonValue =
            config.params.value(QStringLiteral("colorComparison"));
    const QJsonObject colorComparison = colorComparisonValue.isObject()
            ? colorComparisonValue.toObject() : QJsonObject();

    double effectiveTemplatePixels = 0.0;
    const QJsonValue modelValue =
            colorComparison.value(QStringLiteral("model"));
    if (modelValue.isObject()) {
        const QJsonValue countValue = modelValue.toObject().value(
                    QStringLiteral("effectivePixelCount"));
        double count = 0.0;
        if (finiteNumber(countValue, &count) && std::floor(count) == count
                && count >= 0.0 && count <= kMaxExactJsonInteger) {
            effectiveTemplatePixels = count;
        }
    }

    bool brightnessEnabled = false;
    const QJsonValue comparisonValue =
            colorComparison.value(QStringLiteral("comparison"));
    if (comparisonValue.isObject()) {
        const QJsonValue enabledValue = comparisonValue.toObject().value(
                    QStringLiteral("brightnessCompensation"));
        if (enabledValue.isBool())
            brightnessEnabled = enabledValue.toBool();
    }

    bool positionRequested = false;
    QString positionSource;
    const QJsonValue positionValue =
            colorComparison.value(QStringLiteral("positionCorrection"));
    if (positionValue.isObject()) {
        const QJsonObject position = positionValue.toObject();
        if (position.value(QStringLiteral("enabled")).isBool()) {
            positionRequested =
                    position.value(QStringLiteral("enabled")).toBool();
        }
        if (position.value(QStringLiteral("sourceId")).isString()) {
            positionSource =
                    position.value(QStringLiteral("sourceId")).toString();
        }
    }
    const QString stablePositionSource = config.params
            .value(QStringLiteral("positionCorrectionSourceId"))
            .toString().trimmed();
    if (!stablePositionSource.isEmpty())
        positionSource = stablePositionSource;
    positionSource = PositionCorrection::normalizedSourceId(positionSource);

    QJsonArray warnings;
    QJsonObject detectionRoi{
        {QStringLiteral("type"), QStringLiteral("rectangle")},
        {QStringLiteral("angle"), 0.0},
        {QStringLiteral("centerX"), 0.5},
        {QStringLiteral("centerY"), 0.5},
        {QStringLiteral("width"), 0.0},
        {QStringLiteral("height"), 0.0}
    };
    const QJsonValue detectTypeValue =
            colorComparison.value(QStringLiteral("detectRegionType"));
    const QString detectType = detectTypeValue.isString()
            ? detectTypeValue.toString().trimmed().toLower() : QString();
    if (detectType == QStringLiteral("rectangle")) {
        QRectF rect;
        if (parseRect(colorComparison.value(QStringLiteral("detectRoiNormalized")),
                      &rect)) {
            detectionRoi.insert(QStringLiteral("centerX"),
                                rect.x() + rect.width() * 0.5);
            detectionRoi.insert(QStringLiteral("centerY"),
                                rect.y() + rect.height() * 0.5);
            detectionRoi.insert(QStringLiteral("width"), rect.width());
            detectionRoi.insert(QStringLiteral("height"), rect.height());
        }
    } else if (detectType == QStringLiteral("circle")) {
        const QJsonValue circleValue = colorComparison.value(
                    QStringLiteral("detectCircleNormalized"));
        if (circleValue.isObject()) {
            const QJsonObject circle = circleValue.toObject();
            QPointF center;
            double radius = 0.0;
            if (parsePoint(circle.value(QStringLiteral("center")), &center)
                    && finiteNumber(circle.value(QStringLiteral("radius")),
                                    &radius)
                    && radius > 0.0) {
                const double radiusPixels = targetImage.cols > 0
                        && targetImage.rows > 0
                        ? radius * static_cast<double>(
                              std::max(targetImage.cols, targetImage.rows))
                        : 0.0;
                const double normalizedWidth = targetImage.cols > 0
                        ? radiusPixels * 2.0
                          / static_cast<double>(targetImage.cols)
                        : radius * 2.0;
                const double normalizedHeight = targetImage.rows > 0
                        ? radiusPixels * 2.0
                          / static_cast<double>(targetImage.rows)
                        : radius * 2.0;
                detectionRoi.insert(QStringLiteral("type"),
                                    QStringLiteral("circle"));
                detectionRoi.insert(QStringLiteral("centerX"), center.x());
                detectionRoi.insert(QStringLiteral("centerY"), center.y());
                detectionRoi.insert(QStringLiteral("width"), normalizedWidth);
                detectionRoi.insert(QStringLiteral("height"), normalizedHeight);
                detectionRoi.insert(QStringLiteral("radius"), radius);
                detectionRoi.insert(QStringLiteral("radiusNormalized"), radius);
                detectionRoi.insert(QStringLiteral("radiusPixels"), radiusPixels);
            }
        }
    }

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::ColorComparison;
    result.status = status;
    result.message = message;
    result.text = QStringLiteral("0.00");
    result.payload = {
        {QStringLiteral("status"), status},
        {QStringLiteral("message"), message},
        {QStringLiteral("measurementValid"), false},
        {QStringLiteral("passed"), false},
        {QStringLiteral("algorithm"), QStringLiteral("histogram_intersection")},
        {QStringLiteral("featureType"), QStringLiteral("histogram_hs_2d")},
        {QStringLiteral("modelVersion"), 2},
        {QStringLiteral("score"), 0.0},
        {QStringLiteral("similarity"), 0.0},
        {QStringLiteral("threshold"), static_cast<double>(threshold)},
        {QStringLiteral("effectiveTemplatePixels"), effectiveTemplatePixels},
        {QStringLiteral("effectiveDetectionPixels"), 0.0},
        {QStringLiteral("brightnessCompensation"), QJsonObject{
             {QStringLiteral("enabled"), brightnessEnabled},
             {QStringLiteral("applied"), false},
             {QStringLiteral("templateMean"), 0.0},
             {QStringLiteral("detectMeanBefore"), 0.0},
             {QStringLiteral("detectMeanAfter"), 0.0},
             {QStringLiteral("scale"), 1.0},
             {QStringLiteral("clippedRatio"), 0.0}
         }},
        {QStringLiteral("positionCorrection"), QJsonObject{
             {QStringLiteral("requested"), positionRequested},
             {QStringLiteral("applied"), false},
             {QStringLiteral("sourceId"), positionSource}
         }},
        {QStringLiteral("detectionRoi"), detectionRoi},
        {QStringLiteral("warnings"), warnings},
        {QStringLiteral("elapsedMs"), 0.0}
    };
    for (auto iterator = payloadPatch.constBegin();
         iterator != payloadPatch.constEnd(); ++iterator) {
        result.payload.insert(iterator.key(), iterator.value());
    }
    return result;
}

ColorComparisonTemplateBuildResult makeBuildError(const QString &status,
                                                   const QString &message)
{
    ColorComparisonTemplateBuildResult result;
    result.status = status;
    result.message = message;
    result.payload = {
        {QStringLiteral("status"), status},
        {QStringLiteral("message"), message},
        {QStringLiteral("algorithm"), QStringLiteral("histogram_intersection")},
        {QStringLiteral("featureType"), QStringLiteral("histogram_hs_2d")},
        {QStringLiteral("modelVersion"), 2}
    };
    return result;
}

ToolResult mapRunnerResult(const ToolConfig &config,
                           const ColorComparisonHalconResult &runnerResult)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::ColorComparison;
    result.success = runnerResult.success;
    result.ok = runnerResult.ok;
    result.status = runnerResult.status;
    result.message = runnerResult.message;
    result.score = runnerResult.score;
    result.value = runnerResult.similarity;
    result.count = runnerResult.measurementValid ? 1 : 0;
    result.elapsedMs = runnerResult.elapsedMs;
    result.text = QString::number(runnerResult.score, 'f', 2);
    result.overlays = runnerResult.overlays;
    result.payload = runnerResult.payload;
    return result;
}

ToolResult mapInputContractFailure(
        const ToolConfig &config,
        const cv::Mat &targetImage,
        const ColorComparisonHalconResult &runnerResult)
{
    ToolResult result = mapRunnerResult(config, runnerResult);
    const ToolResult configuredPayload = makeError(
                config, result.status, result.message, targetImage);
    const QString configuredKeys[] = {
        QStringLiteral("algorithm"),
        QStringLiteral("featureType"),
        QStringLiteral("modelVersion"),
        QStringLiteral("threshold"),
        QStringLiteral("effectiveTemplatePixels"),
        QStringLiteral("brightnessCompensation"),
        QStringLiteral("positionCorrection"),
        QStringLiteral("detectionRoi")
    };
    for (const QString &key : configuredKeys)
        result.payload.insert(key, configuredPayload.payload.value(key));

    QJsonArray warnings = result.payload.value(
                QStringLiteral("warnings")).toArray();
    const QJsonArray configuredWarnings = configuredPayload.payload.value(
                QStringLiteral("warnings")).toArray();
    for (const QJsonValue &warning : configuredWarnings) {
        if (!warnings.contains(warning))
            warnings.append(warning);
    }
    result.payload.insert(QStringLiteral("warnings"), warnings);
    return result;
}

bool parseVersion(const ToolRequest &request,
                  QJsonObject *colorComparison,
                  int *version,
                  QString *status,
                  QString *message)
{
    const QJsonValue colorComparisonValue =
            request.config.params.value(QStringLiteral("colorComparison"));
    if (!colorComparisonValue.isObject()) {
        if (status)
            *status = QStringLiteral("unsupported_model_version");
        if (message)
            *message = QStringLiteral("Color comparison parameters are missing.");
        return false;
    }
    const QJsonObject parsedObject = colorComparisonValue.toObject();
    int parsedVersion = 0;
    if (!integerNumber(parsedObject.value(QStringLiteral("version")),
                       &parsedVersion)
            || (parsedVersion != 1 && parsedVersion != 2)) {
        if (status)
            *status = QStringLiteral("unsupported_model_version");
        if (message)
            *message = QStringLiteral("Unsupported color comparison model version.");
        return false;
    }
    if (colorComparison)
        *colorComparison = parsedObject;
    if (version)
        *version = parsedVersion;
    return true;
}

bool supportedOriginalPixelFormat(
        const ColorComparisonInputSignature &signature)
{
    const QString pixelFormat = signature.pixelFormat.trimmed();
    if (pixelFormat.isEmpty())
        return true;

    static const QStringList formats = {
        QStringLiteral("BGR8"),
        QStringLiteral("BGRA8"),
        QStringLiteral("UYVY8"),
        QStringLiteral("NV12"),
        QStringLiteral("RGB8"),
        QStringLiteral("RGBX8"),
        QStringLiteral("ARGB8"),
        QStringLiteral("ARGB8_Premultiplied"),
        QStringLiteral("RGBA8"),
        QStringLiteral("RGBA8_Premultiplied")
    };
    return formats.contains(pixelFormat);
}

bool inputContractFailure(const cv::Mat &image,
                          const ColorComparisonInputSignature &signature,
                          QString *status,
                          QString *message)
{
    if (signature.colorMode == QStringLiteral("mono")) {
        if (status)
            *status = QStringLiteral("unsupported_color_input");
        if (message) {
            *message = QStringLiteral(
                        "Original input is monochrome; color comparison is invalid.");
        }
        return true;
    }
    if ((signature.bitDepth != -1 && signature.bitDepth != 8)
            || !supportedOriginalPixelFormat(signature)
            || (image.type() != CV_8UC3 && image.type() != CV_8UC4)) {
        if (status)
            *status = QStringLiteral("unsupported_pixel_format");
        if (message) {
            *message = QStringLiteral(
                        "Only original 8-bit CV_8UC3 and CV_8UC4 inputs are supported.");
        }
        return true;
    }
    return false;
}

ParseResult parseConfig(const ToolRequest &request,
                        bool templateBuild,
                        const cv::Mat &targetImage,
                        const QJsonObject &colorComparison,
                        int version,
                        const ColorComparisonInputSignature &inputSignature)
{
    ParseResult result;
    ColorComparisonHalconConfig &config = result.config;
    config.inputSignature = inputSignature;

    if (colorComparison.contains(QStringLiteral("templateRegionMode"))) {
        const QJsonValue modeValue =
                colorComparison.value(QStringLiteral("templateRegionMode"));
        if (!modeValue.isString()) {
            return parseFailure(QStringLiteral("invalid_template_roi"),
                                QStringLiteral("templateRegionMode must be a string."));
        }
        config.templateRegionMode = modeValue.toString().trimmed().toLower();
    }
    if (config.templateRegionMode != QStringLiteral("custom")
            && config.templateRegionMode != QStringLiteral("sync")) {
        return parseFailure(QStringLiteral("invalid_template_roi"),
                            QStringLiteral("templateRegionMode must be custom or sync."));
    }

    if (colorComparison.contains(QStringLiteral("templateRoiNormalized"))
            && !parseRect(colorComparison.value(QStringLiteral("templateRoiNormalized")),
                          &config.templateRoiNormalized)) {
        return parseFailure(QStringLiteral("invalid_template_roi"),
                            QStringLiteral("Template ROI is malformed or out of range."));
    }
    if (config.templateRegionMode == QStringLiteral("custom")
            && !validNormalizedRect(config.templateRoiNormalized)) {
        return parseFailure(QStringLiteral("invalid_template_roi"),
                            QStringLiteral("Template ROI is invalid."));
    }

    if (colorComparison.contains(QStringLiteral("templateMaskPolygon"))
            && !parsePolygon(colorComparison.value(QStringLiteral("templateMaskPolygon")),
                             &config.templateMaskPolygonNormalized)) {
        return parseFailure(QStringLiteral("invalid_template_mask"),
                            QStringLiteral("Template mask is malformed or degenerate."));
    }

    if (colorComparison.contains(QStringLiteral("detectRegionType"))) {
        const QJsonValue typeValue =
                colorComparison.value(QStringLiteral("detectRegionType"));
        if (!typeValue.isString()) {
            return parseFailure(QStringLiteral("invalid_detect_roi"),
                                QStringLiteral("detectRegionType must be a string."));
        }
        config.detectRegionType = typeValue.toString().trimmed().toLower();
    }
    if (config.detectRegionType != QStringLiteral("rectangle")
            && config.detectRegionType != QStringLiteral("circle")) {
        return parseFailure(QStringLiteral("invalid_detect_roi"),
                            QStringLiteral("detectRegionType must be rectangle or circle."));
    }

    if (colorComparison.contains(QStringLiteral("detectRoiNormalized"))
            && !parseRect(colorComparison.value(QStringLiteral("detectRoiNormalized")),
                          &config.detectRoiNormalized)) {
        return parseFailure(QStringLiteral("invalid_detect_roi"),
                            QStringLiteral("Detection ROI is malformed or out of range."));
    }

    if (colorComparison.contains(QStringLiteral("detectCircleNormalized"))) {
        const QJsonValue circleValue =
                colorComparison.value(QStringLiteral("detectCircleNormalized"));
        if (!circleValue.isObject()) {
            return parseFailure(QStringLiteral("invalid_detect_roi"),
                                QStringLiteral("Detection circle must be an object."));
        }
        const QJsonObject circle = circleValue.toObject();
        if (!parsePoint(circle.value(QStringLiteral("center")),
                        &config.detectCircleCenterNormalized)
                || !finiteNumber(circle.value(QStringLiteral("radius")),
                                 &config.detectCircleRadiusNormalized)) {
            return parseFailure(QStringLiteral("invalid_detect_roi"),
                                QStringLiteral("Detection circle is malformed."));
        }
    }
    if (config.detectRegionType == QStringLiteral("circle")) {
        const QPointF center = config.detectCircleCenterNormalized;
        const double radius = config.detectCircleRadiusNormalized;
        if (!validCircleForImage(center, radius, targetImage)) {
            return parseFailure(QStringLiteral("invalid_detect_roi"),
                                QStringLiteral("Detection circle is outside the target image."));
        }
    }

    if (colorComparison.contains(QStringLiteral("detectMaskPolygon"))
            && !parsePolygon(colorComparison.value(QStringLiteral("detectMaskPolygon")),
                             &config.detectMaskPolygonNormalized)) {
        return parseFailure(QStringLiteral("invalid_detect_mask"),
                            QStringLiteral("Detection mask is malformed or degenerate."));
    }

    if (version == 1) {
        QString featureType = QStringLiteral("histogram");
        if (colorComparison.contains(QStringLiteral("featureType"))) {
            const QJsonValue featureValue =
                    colorComparison.value(QStringLiteral("featureType"));
            if (!featureValue.isString()) {
                return parseFailure(QStringLiteral("unsupported_feature"),
                                    QStringLiteral("Legacy featureType must be a string."));
            }
            featureType = featureValue.toString().trimmed().toLower();
        }
        if (featureType == QStringLiteral("spectrum")) {
            return parseFailure(QStringLiteral("unsupported_feature"),
                                QStringLiteral("Spectrum color comparison is not implemented."));
        }
        if (featureType != QStringLiteral("histogram")
                && featureType != QStringLiteral("histogram_2dim_hs")
                && featureType != QStringLiteral("histogram_hs_2d")) {
            return parseFailure(QStringLiteral("unsupported_feature"),
                                QStringLiteral("Legacy color comparison feature is unsupported."));
        }

        config.model = ColorComparisonModelV2();
        if (!templateBuild) {
            const ColorComparisonModelReadResult modelRead =
                    readColorComparisonModel(colorComparison,
                                             !request.referenceImage.empty());
            return parseFailure(modelRead.status, modelRead.message);
        }
    } else {
        const QJsonObject modelObject =
                colorComparison.value(QStringLiteral("model")).toObject();
        const QJsonValue featureValue =
                modelObject.value(QStringLiteral("featureType"));
        if (featureValue.isString()
                && featureValue.toString() != QStringLiteral("histogram_hs_2d")) {
            return parseFailure(QStringLiteral("unsupported_feature"),
                                QStringLiteral("Only histogram_hs_2d is implemented."));
        }

        const ColorComparisonModelReadResult modelRead = readColorComparisonModel(
                    colorComparison, !request.referenceImage.empty());
        QString modelStatus = modelRead.status;
        QString modelMessage = modelRead.message;
        const DialogLifecycleFailure lifecycle =
                restoredDialogLifecycleFailure(colorComparison,
                                                modelRead.model.state);
        if (lifecycle.recognized) {
            modelStatus = lifecycle.status;
            modelMessage = lifecycle.message;
        }
        const bool buildableState = templateBuild
                && (modelStatus == QStringLiteral("model_empty")
                    || modelStatus == QStringLiteral("model_stale"));
        if (!modelRead.success && !buildableState) {
            const QString status =
                    modelStatus == QStringLiteral("model_unsupported")
                    ? QStringLiteral("model_rebuild_required") : modelStatus;
            return parseFailure(status, modelMessage);
        }
        config.model = modelRead.model;
    }

    const QJsonValue comparisonValue =
            colorComparison.value(QStringLiteral("comparison"));
    if (!comparisonValue.isUndefined() && !comparisonValue.isNull()) {
        if (!comparisonValue.isObject()) {
            return parseFailure(QStringLiteral("invalid_sensitivity"),
                                QStringLiteral("comparison must be an object."));
        }
        const QJsonObject comparison = comparisonValue.toObject();
        if (comparison.contains(QStringLiteral("sensitivity"))) {
            const QJsonValue sensitivityValue =
                    comparison.value(QStringLiteral("sensitivity"));
            if (!sensitivityValue.isString()) {
                return parseFailure(QStringLiteral("invalid_sensitivity"),
                                    QStringLiteral("sensitivity must be a string."));
            }
            config.sensitivity = sensitivityValue.toString().trimmed().toLower();
        }
        if (config.sensitivity != QStringLiteral("high")
                && config.sensitivity != QStringLiteral("medium")
                && config.sensitivity != QStringLiteral("low")) {
            return parseFailure(QStringLiteral("invalid_sensitivity"),
                                QStringLiteral("sensitivity must be high, medium, or low."));
        }
        if (comparison.contains(QStringLiteral("brightnessCompensation"))) {
            const QJsonValue brightnessValue =
                    comparison.value(QStringLiteral("brightnessCompensation"));
            if (!brightnessValue.isBool()) {
                return parseFailure(QStringLiteral("invalid_illumination"),
                                    QStringLiteral("brightnessCompensation must be boolean."));
            }
            config.brightnessCompensation = brightnessValue.toBool();
        }
    }

    const QJsonValue positionValue =
            colorComparison.value(QStringLiteral("positionCorrection"));
    if (!positionValue.isUndefined() && !positionValue.isNull()) {
        if (!positionValue.isObject()) {
            return parseFailure(QStringLiteral("invalid_position_correction"),
                                QStringLiteral("positionCorrection must be an object."));
        }
        const QJsonObject position = positionValue.toObject();
        if (position.contains(QStringLiteral("enabled"))) {
            if (!position.value(QStringLiteral("enabled")).isBool()) {
                return parseFailure(QStringLiteral("invalid_position_correction"),
                                    QStringLiteral("positionCorrection.enabled must be boolean."));
            }
            config.positionCorrection.requested =
                    position.value(QStringLiteral("enabled")).toBool();
        }
        if (position.contains(QStringLiteral("sourceId"))) {
            if (!position.value(QStringLiteral("sourceId")).isString()) {
                return parseFailure(QStringLiteral("invalid_position_correction"),
                                    QStringLiteral("positionCorrection.sourceId must be a string."));
            }
            config.positionCorrection.sourceId =
                    position.value(QStringLiteral("sourceId")).toString().trimmed();
        }
        if (position.contains(QStringLiteral("showMatchContour"))) {
            if (!position.value(QStringLiteral("showMatchContour")).isBool()) {
                return parseFailure(QStringLiteral("invalid_position_correction"),
                                    QStringLiteral("positionCorrection.showMatchContour must be boolean."));
            }
            config.positionCorrection.showMatchContour =
                    position.value(QStringLiteral("showMatchContour")).toBool();
        }
        if (position.contains(QStringLiteral("interfaceVersion"))) {
            int interfaceVersion = 0;
            if (!integerNumber(position.value(QStringLiteral("interfaceVersion")),
                               &interfaceVersion)
                    || interfaceVersion != 1) {
                return parseFailure(QStringLiteral("invalid_position_correction"),
                                    QStringLiteral("Only position-correction interface version 1 is reserved."));
            }
        }
    }

    if (config.positionCorrection.requested) {
        const QString stableSourceId = request.config.params
                .value(QStringLiteral("positionCorrectionSourceId"))
                .toString().trimmed();
        if (!stableSourceId.isEmpty())
            config.positionCorrection.sourceId = stableSourceId;
        config.positionCorrection.sourceId =
                PositionCorrection::normalizedSourceId(config.positionCorrection.sourceId);
    }

    const QJsonObject judgeRule = request.config.judgeRule;
    if (!judgeRule.value(QStringLiteral("mode")).isString()
            || judgeRule.value(QStringLiteral("mode")).toString().trimmed().toLower()
                    != QStringLiteral("min_score")) {
        return parseFailure(QStringLiteral("invalid_judge_rule"),
                            QStringLiteral("judgeRule.mode must be min_score."));
    }
    int minScore = 0;
    if (!integerNumber(judgeRule.value(QStringLiteral("minScore")), &minScore)
            || minScore < 0 || minScore > 100) {
        return parseFailure(QStringLiteral("invalid_judge_rule"),
                            QStringLiteral("judgeRule.minScore must be an integer in [0,100]."));
    }
    config.minScore = minScore;

    const QJsonValue halconPathValue =
            colorComparison.value(QStringLiteral("halconSoPath"));
    if (!halconPathValue.isUndefined() && !halconPathValue.isNull()
            && !halconPathValue.isString()) {
        return parseFailure(QStringLiteral("halcon_load_failed"),
                            QStringLiteral("halconSoPath must be a string."));
    }
    const QString requestedHalconPath = halconPathValue.toString().trimmed();
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                requestedHalconPath, &config.halconSoPathCandidates);

    result.success = true;
    return result;
}

ParseResult resolvePositionCorrection(const ToolRequest &request,
                                      ParseResult parsed)
{
    ColorComparisonHalconConfig &config = parsed.config;
    if (!parsed.success)
        return parsed;

    PositionCorrectionConsumerOptions options;
    options.requested = config.positionCorrection.requested;
    options.sourceId = config.positionCorrection.sourceId;
    options.showMatchContour = config.positionCorrection.showMatchContour;
    const PositionCorrectionResolveResult correction =
            PositionCorrectionConsumer::resolve(request, options);
    if (!correction.success)
        return parseFailure(correction.status, correction.message);
    config.positionCorrection = correction.context;
    return parsed;
}

} // namespace

bool ColorComparisonAdapter::supports(ToolType type) const
{
    return type == ToolType::ColorComparison;
}

ColorComparisonTemplateBuildResult ColorComparisonAdapter::buildTemplateModel(
        const ToolRequest &request)
{
    if (request.config.toolType != ToolType::ColorComparison) {
        return makeBuildError(
                    QStringLiteral("invalid_tool_type"),
                    QStringLiteral("ColorComparisonAdapter only supports ToolType::ColorComparison."));
    }
    if (request.referenceImage.empty()) {
        return makeBuildError(QStringLiteral("image_empty"),
                              QStringLiteral("Reference image is empty."));
    }

    QString status;
    QString message;
    ColorComparisonInputSignature inputSignature;
    if (!parseInputSignature(request.runtimeContext,
                             QStringLiteral("referenceInput"),
                             &inputSignature,
                             &status,
                             &message)) {
        return makeBuildError(status, message);
    }
    if (inputContractFailure(request.referenceImage,
                             inputSignature,
                             &status,
                             &message)) {
        return makeBuildError(status, message);
    }

    QJsonObject colorComparison;
    int version = 0;
    if (!parseVersion(request, &colorComparison, &version, &status, &message))
        return makeBuildError(status, message);

    const ParseResult parsed = parseConfig(request,
                                           true,
                                           request.referenceImage,
                                           colorComparison,
                                           version,
                                           inputSignature);
    if (!parsed.success)
        return makeBuildError(parsed.status, parsed.message);
    return m_runner.buildTemplateModel(request.referenceImage, parsed.config);
}

ToolResult ColorComparisonAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::ColorComparison) {
        return makeError(config,
                         QStringLiteral("invalid_tool_type"),
                         QStringLiteral("ColorComparisonAdapter only supports ToolType::ColorComparison."),
                         request.image);
    }
    if (request.image.empty()) {
        return makeError(config,
                         QStringLiteral("image_empty"),
                         QStringLiteral("Input image is empty."),
                         request.image);
    }

    QString status;
    QString message;
    ColorComparisonInputSignature inputSignature;
    if (!parseInputSignature(request.runtimeContext,
                             QStringLiteral("input"),
                             &inputSignature,
                             &status,
                             &message)) {
        return makeError(config, status, message, request.image);
    }
    if (inputContractFailure(request.image,
                             inputSignature,
                             &status,
                             &message)) {
        ColorComparisonHalconConfig preflightConfig;
        preflightConfig.inputSignature = inputSignature;
        return mapInputContractFailure(
                    config,
                    request.image,
                    m_runner.run(request.image, preflightConfig));
    }

    QJsonObject colorComparison;
    int version = 0;
    if (!parseVersion(request, &colorComparison, &version, &status, &message))
        return makeError(config, status, message, request.image);

    ParseResult parsed = parseConfig(request,
                                     false,
                                     request.image,
                                     colorComparison,
                                     version,
                                     inputSignature);
    if (!parsed.success)
        return makeError(config, parsed.status, parsed.message, request.image);

    parsed = resolvePositionCorrection(request, parsed);
    if (!parsed.success)
        return makeError(config, parsed.status, parsed.message, request.image);

    if (parsed.config.model.state == ColorComparisonModelState::Ready) {
        const QString actualReferenceHash = request.referenceImage.empty()
                ? QString()
                : colorComparisonReferenceHash(request.referenceImage);
        if (actualReferenceHash != parsed.config.model.referenceImageHash) {
            return makeError(
                        config,
                        QStringLiteral("model_stale"),
                        QStringLiteral("Current reference image does not match the sampled model."),
                        request.image,
                        QJsonObject{
                            {QStringLiteral("expectedReferenceImageHash"),
                             parsed.config.model.referenceImageHash},
                            {QStringLiteral("actualReferenceImageHash"),
                             actualReferenceHash}
                        });
        }
    }

    const ColorComparisonHalconResult runnerResult =
            m_runner.run(request.image, parsed.config);
    ToolResult result = mapRunnerResult(config, runnerResult);
    if (result.success
            && parsed.config.positionCorrection.applied
            && parsed.config.positionCorrection.showMatchContour
            && parsed.config.positionCorrection.matchContours.isEmpty()) {
        const QString notice = QStringLiteral(
                    "位置修正匹配成功，但未获得可显示的匹配轮廓。");
        result.message = result.message.trimmed().isEmpty()
                ? notice : QStringLiteral("%1 | %2").arg(result.message, notice);
        result.payload.insert(QStringLiteral("positionCorrectionContourStatus"),
                              QStringLiteral("position_correction_contour_unavailable"));
    }
    return result;
}
