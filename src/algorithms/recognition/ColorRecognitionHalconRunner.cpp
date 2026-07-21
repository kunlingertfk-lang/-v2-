#include "algorithms/recognition/ColorRecognitionHalconRunner.h"

#include <HalconC.h>

#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QRect>
#include <QMutex>
#include <QMutexLocker>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <exception>
#include <limits>
#include <memory>
#include <opencv2/imgproc.hpp>
#include <utility>

namespace {

constexpr int kMinRoiPixelSize = 2;
const char kAlgorithmVersion[] = "halcon_hs_histogram_transition_v2";
const char kHsJointFeatureSchemaVersion[] = "hs_joint_histogram_v2";
const char kHsLinearFeatureSchemaVersion[] = "hs_linear_histogram_v2";
const char kHsvLinearFeatureSchemaVersion[] = "hsv_linear_histogram_v2";
const char kHsSmoothingKernelVersion[] = "hs_anisotropic_3x3_v2";
const char kCurrentHsvClassifierVersion[] = "hsv_category_topn_intersection_v1";
const char kLegacyHsvClassifierVersion[] = "hsv_category_best_sample_v2";
constexpr int kHsvClassTopN = 3;

// 判断 HALCON C API 返回码是否属于正常状态。
bool halconStatusOk(const Herror status)
{
    return status == H_MSG_OK || status == H_MSG_TRUE || status == H_MSG_FALSE;
}

// 判断 HALCON 对象句柄是否实际分配，避免重复 clear 空对象。
bool halconObjectAllocated(const Hobject object)
{
    return object != 0 && object != NO_OBJECTS && object != EMPTY_REGION;
}

// 判断 QRectF 的四个分量是否都是有限值。
bool isFiniteRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) &&
           std::isfinite(rect.y()) &&
           std::isfinite(rect.width()) &&
           std::isfinite(rect.height());
}

// 判断归一化 ROI 是否有有效面积。
bool isValidNormalizedRoi(const QRectF &rect)
{
    return isFiniteRect(rect) && rect.width() > 0.0 && rect.height() > 0.0;
}

// 判断当前检测区域是否使用圆形 ROI。
bool isCircleRegionType(const QString &type)
{
    return type.trimmed().toLower() == QStringLiteral("circle");
}

// 规范化颜色判别方式，未知旧配置回退到新版默认主颜色占比模式。
QString normalizedColorDecisionMode(const QString &mode)
{
    const QString key = mode.trimmed().toLower();
    if (key == QStringLiteral("histogram_intersection") || key == QStringLiteral("similarity"))
        return QStringLiteral("histogram_intersection");
    if (key == QStringLiteral("halcon_color_segmentation") || key == QStringLiteral("halcon_color_cluster"))
        return QStringLiteral("halcon_color_segmentation");
    return QStringLiteral("dominant_ratio");
}

// 将 0-1 归一化 ROI 转成图像像素矩形，并过滤过小区域。
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
    const int x2 = qBound(0, static_cast<int>(std::ceil(right * width)), width);
    const int y2 = qBound(0, static_cast<int>(std::ceil(bottom * height)), height);
    const int roiWidth = x2 - x1;
    const int roiHeight = y2 - y1;
    if (roiWidth < kMinRoiPixelSize || roiHeight < kMinRoiPixelSize)
        return QRect();

    return QRect(x1, y1, roiWidth, roiHeight);
}

// 将矩形写入结果 payload，便于 UI 和日志定位 ROI。
QJsonObject rectToJson(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

// 将特征向量写入结果 payload。
QJsonArray vectorToJson(const QVector<double> &values)
{
    QJsonArray array;
    for (const double value : values)
        array.append(value);
    return array;
}

// 将类别占比映射转为 payload 数组，便于 UI、日志和调试读取每个类别的占比。
QJsonArray ratiosToJson(const QVector<QPair<QString, double>> &ratios)
{
    QJsonArray array;
    for (const QPair<QString, double> &ratio : ratios) {
        QJsonObject json;
        json.insert(QStringLiteral("label"), ratio.first);
        json.insert(QStringLiteral("ratio"), ratio.second);
        json.insert(QStringLiteral("score"), ratio.second * 100.0);
        array.append(json);
    }
    return array;
}

// 将归一化多边形点集写入结果 payload。
QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points) {
        QJsonObject json;
        json.insert(QStringLiteral("x"), point.x());
        json.insert(QStringLiteral("y"), point.y());
        array.append(json);
    }
    return array;
}

// 将点坐标写入结果 payload。
QJsonObject pointToJsonObject(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
}

// 构造矩形 ROI overlay。
ToolOverlay rectOverlay(const QRectF &rect, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.rect = rect;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

// 构造圆形 ROI overlay。
ToolOverlay circleOverlay(const QPointF &center,
                          const double radius,
                          const QString &label,
                          const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Circle;
    overlay.center = center;
    overlay.radius = radius;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

// 将矩形写入 overlay.extra 使用的 JSON 结构。
QJsonObject rectToJsonObject(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

// 构造文字结果 overlay，用于显示 OK/NG、类别和分数。
ToolOverlay textOverlay(const QPointF &position,
                        const QString &text,
                        const double score = 0.0,
                        const QString &label = QString())
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.p1 = position;
    overlay.text = text;
    overlay.label = label.isEmpty() ? text : label;
    overlay.score = score;
    return overlay;
}

// 按灵敏度选择直方图 bin 数：低/中/高对应 8/16/32。
int histogramBinsForSensitivity(const QString &sensitivity)
{
    const QString key = sensitivity.trimmed().toLower();
    if (key == QStringLiteral("low"))
        return 8;
    if (key == QStringLiteral("high"))
        return 32;
    return 16;
}

// 判断是否使用 HALCON H/S 二维联合直方图特征。
bool isHisto2DimFeatureType(const QString &featureType)
{
    const QString key = featureType.trimmed().toLower();
    return key == QStringLiteral("histogram_2dim_hs") ||
            key == QStringLiteral("histo_2dim") ||
            key == QStringLiteral("histogram_2dim");
}

bool effectiveBrightnessEnabled(const ColorRecognitionHalconConfig &config)
{
    return !isHisto2DimFeatureType(config.featureType) && config.brightnessEnabled;
}

QString actualFeatureType(const ColorRecognitionHalconConfig &config)
{
    if (isHisto2DimFeatureType(config.featureType))
        return QStringLiteral("histogram_2dim_hs");
    return effectiveBrightnessEnabled(config)
            ? QStringLiteral("histogram_1dim_hsv")
            : QStringLiteral("histogram_1dim_hs");
}

QString featureSchemaVersion(const ColorRecognitionHalconConfig &config)
{
    if (isHisto2DimFeatureType(config.featureType))
        return QString::fromLatin1(kHsJointFeatureSchemaVersion);
    return effectiveBrightnessEnabled(config)
            ? QString::fromLatin1(kHsvLinearFeatureSchemaVersion)
            : QString::fromLatin1(kHsLinearFeatureSchemaVersion);
}

QString inferredPixelFormat(const ColorRecognitionHalconConfig &config, const cv::Mat &image)
{
    if (!config.pixelFormat.trimmed().isEmpty())
        return config.pixelFormat.trimmed();
    if (image.depth() == CV_8U)
        return image.channels() == 4 ? QStringLiteral("BGRA8") : QStringLiteral("BGR8");
    if (image.depth() == CV_16U)
        return image.channels() == 4 ? QStringLiteral("BGRA16") : QStringLiteral("BGR16");
    return QStringLiteral("unsupported");
}

int resolvedValidBits(const ColorRecognitionHalconConfig &config, const cv::Mat &image)
{
    if (config.validBits > 0)
        return config.validBits;
    return image.depth() == CV_8U ? 8 : -1;
}

int resolvedBitShift(const ColorRecognitionHalconConfig &config, const cv::Mat &image)
{
    if (config.bitShift >= 0)
        return config.bitShift;
    return image.depth() == CV_8U ? 0 : -1;
}

QString featureSignature(const ColorRecognitionHalconConfig &config, const cv::Mat &image)
{
    QJsonObject contract;
    contract.insert(QStringLiteral("algorithmVersion"), QString::fromLatin1(kAlgorithmVersion));
    contract.insert(QStringLiteral("axisOrder"), QStringLiteral("row_hue_column_saturation"));
    contract.insert(QStringLiteral("bitShift"), resolvedBitShift(config, image));
    contract.insert(QStringLiteral("brightnessEnabled"), effectiveBrightnessEnabled(config));
    contract.insert(QStringLiteral("featureSchemaVersion"), featureSchemaVersion(config));
    contract.insert(QStringLiteral("featureType"), actualFeatureType(config));
    contract.insert(QStringLiteral("flattenOrder"), QStringLiteral("hue_major_saturation_minor"));
    contract.insert(QStringLiteral("histogramBins"), histogramBinsForSensitivity(config.sensitivity));
    contract.insert(QStringLiteral("hueCircular"), isHisto2DimFeatureType(config.featureType));
    contract.insert(QStringLiteral("pixelFormat"), inferredPixelFormat(config, image));
    contract.insert(QStringLiteral("smoothingKernelVersion"),
                    isHisto2DimFeatureType(config.featureType)
                    ? QString::fromLatin1(kHsSmoothingKernelVersion)
                    : QStringLiteral("none"));
    contract.insert(QStringLiteral("validBits"), resolvedValidBits(config, image));
    const QByteArray canonical = QJsonDocument(contract).toJson(QJsonDocument::Compact);
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(
                QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex()));
}

// 将三/四通道彩色输入转换为连续 BGR，并保留 8/16 位深度。
cv::Mat toBgrPreservingDepth(const cv::Mat &image)
{
    if (image.empty())
        return cv::Mat();
    if (image.depth() != CV_8U && image.depth() != CV_16U)
        return cv::Mat();

    cv::Mat bgr;
    if (image.channels() == 3)
        bgr = image;
    else if (image.channels() == 4)
        cv::cvtColor(image, bgr, cv::COLOR_BGRA2BGR);

    if (!bgr.empty() && !bgr.isContinuous())
        bgr = bgr.clone();
    return bgr;
}

bool validInputBitContract(const cv::Mat &image,
                           const ColorRecognitionHalconConfig &config,
                           QString *status,
                           QString *message)
{
    if (image.empty()) {
        *status = QStringLiteral("image_empty");
        *message = QStringLiteral("Input image is empty.");
        return false;
    }
    if (image.channels() == 1) {
        *status = QStringLiteral("unsupported_mono_for_color_recognition");
        *message = QStringLiteral("Monochrome input has no color information.");
        return false;
    }
    if (image.channels() != 3 && image.channels() != 4) {
        *status = QStringLiteral("unsupported_channel_count");
        *message = QStringLiteral("Color recognition requires three or four channels.");
        return false;
    }
    if (image.depth() == CV_8U)
        return true;
    if (image.depth() != CV_16U) {
        *status = QStringLiteral("unsupported_image_format");
        *message = QStringLiteral("Only unsigned 8-bit and 16-bit color input is supported.");
        return false;
    }
    if (config.validBits < 0 || config.bitShift < 0) {
        *status = QStringLiteral("missing_pixel_format_metadata");
        *message = QStringLiteral("16-bit color input requires validBits and bitShift metadata.");
        return false;
    }
    if (config.validBits < 1 || config.validBits > 16) {
        *status = QStringLiteral("invalid_valid_bits");
        *message = QStringLiteral("validBits must be in the range 1..16.");
        return false;
    }
    if (config.bitShift < 0 || config.bitShift > 15 ||
        config.validBits + config.bitShift > 16) {
        *status = QStringLiteral("invalid_bit_shift");
        *message = QStringLiteral("bitShift is incompatible with validBits.");
        return false;
    }
    return true;
}

// 生成特征提取阶段的错误结果，并保留图像、ROI、HALCON 路径等诊断信息。
ColorRecognitionHalconFeatureResult featureError(const QString &status,
                                                 const QString &message,
                                                 const ColorRecognitionHalconConfig &config,
                                                 const cv::Mat &image,
                                                 const qint64 elapsedMs)
{
    ColorRecognitionHalconFeatureResult result;
    result.success = false;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.payload.insert(QStringLiteral("error"), message);
    result.payload.insert(QStringLiteral("algorithmVersion"), QString::fromLatin1(kAlgorithmVersion));
    result.payload.insert(QStringLiteral("featureType"), actualFeatureType(config));
    result.payload.insert(QStringLiteral("featureSchemaVersion"), featureSchemaVersion(config));
    result.payload.insert(QStringLiteral("requestedFeatureType"), config.featureType);
    result.payload.insert(QStringLiteral("colorDecisionMode"),
                          normalizedColorDecisionMode(config.colorDecisionMode));
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("declaredPixelFormat"), config.pixelFormat);
    result.payload.insert(QStringLiteral("actualMatDepth"), image.empty() ? -1 : image.depth());
    result.payload.insert(QStringLiteral("actualChannelCount"), image.empty() ? 0 : image.channels());
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                          pointToJsonObject(config.detectCircleCenterNormalized));
    result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                          config.detectCircleRadiusNormalized);
    result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                          rectToJson(config.detectCircleBoundingRectNormalized));
    result.payload.insert(QStringLiteral("detectMaskPolygon"),
                          pointsToJson(config.detectMaskPolygonNormalized));
    result.payload.insert(QStringLiteral("detectMaskConfigured"),
                          config.detectMaskPolygonNormalized.size() >= 3);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("positionCorrectionSourceId"), config.positionCorrectionSourceId);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"),
                          config.halconSoPathCandidates.join(QStringLiteral("; ")));
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    return result;
}

// 生成完整识别阶段的错误结果，统一 Adapter/UI 可读取的 payload 字段。
ColorRecognitionHalconResult runError(const QString &status,
                                      const QString &message,
                                      const ColorRecognitionHalconConfig &config,
                                      const cv::Mat &image,
                                      const qint64 elapsedMs)
{
    ColorRecognitionHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    result.elapsedMs = elapsedMs;
    result.payload.insert(QStringLiteral("error"), message);
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_hsv_histogram_transition"));
    result.payload.insert(QStringLiteral("algorithmVersion"), QString::fromLatin1(kAlgorithmVersion));
    result.payload.insert(QStringLiteral("featureType"), actualFeatureType(config));
    result.payload.insert(QStringLiteral("featureSchemaVersion"), featureSchemaVersion(config));
    result.payload.insert(QStringLiteral("requestedFeatureType"), config.featureType);
    result.payload.insert(QStringLiteral("colorDecisionMode"),
                          normalizedColorDecisionMode(config.colorDecisionMode));
    result.payload.insert(QStringLiteral("hasImage"), !image.empty());
    result.payload.insert(QStringLiteral("imageWidth"), image.empty() ? 0 : image.cols);
    result.payload.insert(QStringLiteral("imageHeight"), image.empty() ? 0 : image.rows);
    result.payload.insert(QStringLiteral("declaredPixelFormat"), config.pixelFormat);
    result.payload.insert(QStringLiteral("actualMatDepth"), image.empty() ? -1 : image.depth());
    result.payload.insert(QStringLiteral("actualChannelCount"), image.empty() ? 0 : image.channels());
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                          pointToJsonObject(config.detectCircleCenterNormalized));
    result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                          config.detectCircleRadiusNormalized);
    result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                          rectToJson(config.detectCircleBoundingRectNormalized));
    result.payload.insert(QStringLiteral("detectMaskPolygon"),
                          pointsToJson(config.detectMaskPolygonNormalized));
    result.payload.insert(QStringLiteral("detectMaskConfigured"),
                          config.detectMaskPolygonNormalized.size() >= 3);
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("positionCorrectionSourceId"), config.positionCorrectionSourceId);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"),
                          config.halconSoPathCandidates.join(QStringLiteral("; ")));
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    return result;
}

// 生成 ROI 被屏蔽区完全覆盖时的明确结果和展示 overlay。
ColorRecognitionHalconResult maskedRoiResult(const ColorRecognitionHalconConfig &config,
                                             const cv::Mat &image,
                                             const qint64 elapsedMs)
{
    ColorRecognitionHalconResult result;
    result.success = false;
    result.ok = false;
    result.status = QStringLiteral("masked_roi_empty");
    result.message = QStringLiteral("检测 ROI 已完全被屏蔽区域覆盖，未执行颜色识别。");
    result.elapsedMs = elapsedMs;

    const QRect roiPixels = normalizedRoiToPixels(config.roiNormalized,
                                                  image.empty() ? 0 : image.cols,
                                                  image.empty() ? 0 : image.rows);
    if (!roiPixels.isEmpty()) {
        if (isCircleRegionType(config.detectRegionType) &&
            config.detectCircleRadiusNormalized > 0.0 &&
            !image.empty()) {
            const double radiusPixels = config.detectCircleRadiusNormalized *
                    static_cast<double>(qMax(image.cols, image.rows));
            result.overlays.append(circleOverlay(QPointF(config.detectCircleCenterNormalized.x() * image.cols,
                                                         config.detectCircleCenterNormalized.y() * image.rows),
                                                radiusPixels,
                                                QStringLiteral("ROI"),
                                                0.0));
        } else {
            result.overlays.append(rectOverlay(QRectF(roiPixels), QStringLiteral("ROI"), 0.0));
        }
        ToolOverlay statusText = textOverlay(QPointF(roiPixels.x(), roiPixels.y()),
                                             QStringLiteral("颜色识别 已屏蔽"),
                                             0.0,
                                             QStringLiteral("color_result_text"));
        statusText.extra.insert(QStringLiteral("status"), QStringLiteral("MASKED"));
        statusText.extra.insert(QStringLiteral("anchorRect"), rectToJsonObject(QRectF(roiPixels)));
        result.overlays.append(statusText);
    }

    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_hsv_histogram_transition"));
    result.payload.insert(QStringLiteral("algorithmVersion"), QString::fromLatin1(kAlgorithmVersion));
    result.payload.insert(QStringLiteral("featureType"), actualFeatureType(config));
    result.payload.insert(QStringLiteral("featureSchemaVersion"), featureSchemaVersion(config));
    result.payload.insert(QStringLiteral("requestedFeatureType"), config.featureType);
    result.payload.insert(QStringLiteral("colorDecisionMode"),
                          normalizedColorDecisionMode(config.colorDecisionMode));
    result.payload.insert(QStringLiteral("detectMaskApplied"), true);
    result.payload.insert(QStringLiteral("detectMaskFullyCoversRoi"), true);
    result.payload.insert(QStringLiteral("detectMaskPolygon"), pointsToJson(config.detectMaskPolygonNormalized));
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    result.payload.insert(QStringLiteral("circleDetectRoiApplied"), isCircleRegionType(config.detectRegionType));
    result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                          pointToJsonObject(config.detectCircleCenterNormalized));
    result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                          config.detectCircleRadiusNormalized);
    result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                          rectToJson(config.detectCircleBoundingRectNormalized));
    result.payload.insert(QStringLiteral("roiPixelsRect"), rectToJson(QRectF(roiPixels)));
    result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
    result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
    result.payload.insert(QStringLiteral("positionCorrectionSourceId"), config.positionCorrectionSourceId);
    result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
    result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));
    result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(elapsedMs));
    return result;
}

// HALCON C API 函数指针表：运行时动态解析，避免工程在编译期强链接 HALCON。
struct HalconCApi
{
    using SetUtf8Fn = void (*)(int);
    using GetErrorTextFn = Herror (*)(Hlong, char *);
    using CreateTupleFn = void (*)(Htuple *, Hlong);
    using SetDoubleFn = void (*)(Htuple *, double, Hlong);
    using SetIntFn = void (*)(Htuple *, Hlong, Hlong);
    using SetStringFn = void (*)(Htuple *, const char *, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GetIntFn = Hlong (*)(const Htuple *, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Decompose3Fn = Herror (*)(const Hobject, Hobject *, Hobject *, Hobject *);
    using ScaleImageFn = Herror (*)(const Hobject, Hobject *, double, double);
    using ConvertImageTypeFn = Herror (*)(const Hobject, Hobject *, const char *);
    using TransFromRgbFn = Herror (*)(const Hobject, const Hobject, const Hobject,
                                      Hobject *, Hobject *, Hobject *, const char *);
    using GenRectangle1Fn = Herror (*)(Hobject *, double, double, double, double);
    using GenCircleFn = Herror (*)(Hobject *, double, double, double);
    using GenRegionPolygonFilledFn = Herror (*)(Hobject *, const Htuple, const Htuple);
    using DifferenceFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using AreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using Histo2DimFn = Herror (*)(const Hobject, const Hobject, const Hobject, Hobject *);
    using GetGrayvalFn = Herror (*)(const Hobject, const Htuple, const Htuple, Htuple *);
    using GrayHistoRangeFn = Herror (*)(const Hobject, const Hobject, const Htuple,
                                        const Htuple, const Htuple, Htuple *, Htuple *);
    using TupleMin2Fn = Herror (*)(const Htuple, const Htuple, Htuple *);
    using TupleMultFn = Herror (*)(const Htuple, const Htuple, Htuple *);
    using TupleSqrtFn = Herror (*)(const Htuple, Htuple *);
    using TupleSumFn = Herror (*)(const Htuple, Htuple *);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetIntFn setInt = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GetIntFn getInt = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Decompose3Fn decompose3 = nullptr;
    ScaleImageFn scaleImage = nullptr;
    ConvertImageTypeFn convertImageType = nullptr;
    TransFromRgbFn transFromRgb = nullptr;
    GenRectangle1Fn genRectangle1 = nullptr;
    GenCircleFn genCircle = nullptr;
    GenRegionPolygonFilledFn genRegionPolygonFilled = nullptr;
    DifferenceFn difference = nullptr;
    AreaCenterFn areaCenter = nullptr;
    ReduceDomainFn reduceDomain = nullptr;
    Histo2DimFn histo2Dim = nullptr;
    GetGrayvalFn getGrayval = nullptr;
    GrayHistoRangeFn grayHistoRange = nullptr;
    TupleMin2Fn tupleMin2 = nullptr;
    TupleMultFn tupleMult = nullptr;
    TupleSqrtFn tupleSqrt = nullptr;
    TupleSumFn tupleSum = nullptr;
    ClearObjFn clearObj = nullptr;
};

// 解析必需 HALCON 符号，缺失时返回可诊断的错误信息。
template <typename Function>
bool resolveRequired(void *handle, Function &target, const char *symbolName, QString &errorMessage)
{
    dlerror();
    void *symbol = dlsym(handle, symbolName);
    const char *symbolError = dlerror();
    if (symbolError != nullptr || symbol == nullptr) {
        errorMessage = QStringLiteral("Missing HALCON symbol %1: %2")
                .arg(QString::fromLatin1(symbolName),
                     symbolError ? QString::fromLocal8Bit(symbolError) : QStringLiteral("not found"));
        return false;
    }
    target = reinterpret_cast<Function>(symbol);
    return true;
}

// 解析可选 HALCON 符号，缺失不影响主流程。
template <typename Function>
void resolveOptional(void *handle, Function &target, const char *symbolName)
{
    dlerror();
    void *symbol = dlsym(handle, symbolName);
    const char *symbolError = dlerror();
    if (symbolError == nullptr && symbol != nullptr)
        target = reinterpret_cast<Function>(symbol);
}

// 运行时加载 HALCON 动态库，并把颜色识别需要的 C API 全部解析到函数指针表。
class HalconLibrary
{
public:
    // 释放 dlopen 句柄。
    ~HalconLibrary()
    {
        if (m_handle)
            dlclose(m_handle);
    }

    // 加载指定 HALCON 动态库路径并解析必需/可选符号。
    bool load(const QString &path, QString &errorMessage, bool &symbolMissing)
    {
        symbolMissing = false;
        const QByteArray encodedPath = path.toLocal8Bit();
        m_handle = dlopen(encodedPath.constData(), RTLD_NOW | RTLD_LOCAL);
        if (!m_handle) {
            const char *loadError = dlerror();
            errorMessage = loadError ? QString::fromLocal8Bit(loadError)
                                     : QStringLiteral("dlopen returned a null handle.");
            return false;
        }

        resolveOptional(m_handle, api.setUtf8, "SetHcInterfaceStringEncodingIsUtf8");
        if (!resolveRequired(m_handle, api.getErrorText, "get_error_text", errorMessage) ||
            !resolveRequired(m_handle, api.createTuple, "F_create_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.setDouble, "F_set_d", errorMessage) ||
            !resolveRequired(m_handle, api.setInt, "F_set_i", errorMessage) ||
            !resolveRequired(m_handle, api.setString, "F_set_s", errorMessage) ||
            !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
            !resolveRequired(m_handle, api.getInt, "F_get_i", errorMessage) ||
            !resolveRequired(m_handle, api.genImageInterleaved, "gen_image_interleaved", errorMessage) ||
            !resolveRequired(m_handle, api.decompose3, "decompose3", errorMessage) ||
            !resolveRequired(m_handle, api.scaleImage, "scale_image", errorMessage) ||
            !resolveRequired(m_handle, api.convertImageType, "convert_image_type", errorMessage) ||
            !resolveRequired(m_handle, api.transFromRgb, "trans_from_rgb", errorMessage) ||
            !resolveRequired(m_handle, api.genRectangle1, "gen_rectangle1", errorMessage) ||
            !resolveRequired(m_handle, api.genCircle, "gen_circle", errorMessage) ||
            !resolveRequired(m_handle, api.genRegionPolygonFilled, "T_gen_region_polygon_filled", errorMessage) ||
            !resolveRequired(m_handle, api.difference, "difference", errorMessage) ||
            !resolveRequired(m_handle, api.areaCenter, "T_area_center", errorMessage) ||
            !resolveRequired(m_handle, api.reduceDomain, "reduce_domain", errorMessage) ||
            !resolveRequired(m_handle, api.histo2Dim, "T_histo_2dim", errorMessage) ||
            !resolveRequired(m_handle, api.getGrayval, "T_get_grayval", errorMessage) ||
            !resolveRequired(m_handle, api.grayHistoRange, "T_gray_histo_range", errorMessage) ||
            !resolveRequired(m_handle, api.tupleMin2, "T_tuple_min2", errorMessage) ||
            !resolveRequired(m_handle, api.tupleMult, "T_tuple_mult", errorMessage) ||
            !resolveRequired(m_handle, api.tupleSqrt, "T_tuple_sqrt", errorMessage) ||
            !resolveRequired(m_handle, api.tupleSum, "T_tuple_sum", errorMessage) ||
            !resolveRequired(m_handle, api.clearObj, "clear_obj", errorMessage)) {
            symbolMissing = true;
            dlclose(m_handle);
            m_handle = nullptr;
            return false;
        }
        if (api.setUtf8)
            api.setUtf8(1);
        return true;
    }

    HalconCApi api;

private:
    void *m_handle = nullptr;
};

// 颜色识别内部 runtime 缓存：同一路径/符号 profile 仅加载和解析一次。
std::shared_ptr<HalconLibrary> acquireHalconLibrary(const QString &path,
                                                   QString &errorMessage,
                                                   bool &symbolMissing)
{
    static QMutex mutex;
    static QHash<QString, std::shared_ptr<HalconLibrary>> cache;

    const QFileInfo fileInfo(path);
    QString normalizedPath = fileInfo.canonicalFilePath();
    if (normalizedPath.isEmpty())
        normalizedPath = fileInfo.absoluteFilePath();
    const QString cacheKey = normalizedPath + QStringLiteral("|legacy_hs");

    QMutexLocker locker(&mutex);
    const auto existing = cache.constFind(cacheKey);
    if (existing != cache.constEnd()) {
        symbolMissing = false;
        return existing.value();
    }

    std::shared_ptr<HalconLibrary> library = std::make_shared<HalconLibrary>();
    if (!library->load(normalizedPath, errorMessage, symbolMissing))
        return std::shared_ptr<HalconLibrary>();
    cache.insert(cacheKey, library);
    return library;
}

// HTuple RAII 封装：自动管理 HALCON tuple 生命周期，避免泄漏和重复释放。
class HalconTuple
{
public:
    // 绑定 HALCON API 表；未绑定时对象保持空 tuple。
    explicit HalconTuple(HalconCApi *api = nullptr)
        : m_api(api)
    {
    }

    HalconTuple(const HalconTuple &) = delete;
    HalconTuple &operator=(const HalconTuple &) = delete;

    // 移动构造转移 tuple 所有权。
    HalconTuple(HalconTuple &&other) noexcept
        : m_api(other.m_api)
        , m_tuple(other.m_tuple)
    {
        other.m_api = nullptr;
        other.m_tuple = HTUPLE_INITIALIZER;
    }

    // 移动赋值前释放当前 tuple，再接管来源对象。
    HalconTuple &operator=(HalconTuple &&other) noexcept
    {
        if (this == &other)
            return *this;

        destroy();
        m_api = other.m_api;
        m_tuple = other.m_tuple;
        other.m_api = nullptr;
        other.m_tuple = HTUPLE_INITIALIZER;
        return *this;
    }

    // 析构时释放 HALCON tuple 内存。
    ~HalconTuple()
    {
        destroy();
    }

    // 返回可传给 HALCON C API 的 tuple 指针。
    Htuple *ptr()
    {
        return &m_tuple;
    }

    // 返回只读 tuple 引用，供 HALCON C API 输入参数使用。
    const Htuple &value() const
    {
        return m_tuple;
    }

    // 返回 tuple 当前元素数量。
    int size() const
    {
        return static_cast<int>(m_tuple.num);
    }

    // 重新创建指定长度的 tuple。
    void create(const int size)
    {
        destroy();
        if (m_api && m_api->createTuple)
            m_api->createTuple(&m_tuple, size);
    }

    // 设置 double 元素。
    void setDouble(const int index, const double value)
    {
        if (m_api && m_api->setDouble)
            m_api->setDouble(&m_tuple, value, index);
    }

    // 设置整数元素。
    void setInt(const int index, const Hlong value)
    {
        if (m_api && m_api->setInt)
            m_api->setInt(&m_tuple, value, index);
    }

    // 设置字符串元素。
    void setString(const int index, const char *value)
    {
        if (m_api && m_api->setString)
            m_api->setString(&m_tuple, value, index);
    }

    // 读取 double 元素。
    double doubleAt(const int index) const
    {
        return m_api && m_api->getDouble ? m_api->getDouble(&m_tuple, index) : 0.0;
    }

    // 读取整数元素。
    Hlong intAt(const int index) const
    {
        return m_api && m_api->getInt ? m_api->getInt(&m_tuple, index) : 0;
    }

    // 释放当前 tuple 并重置为空。
    void destroy()
    {
        if (m_api && m_api->destroyTuple && (m_tuple.num > 0 || m_tuple.capacity > 0))
            m_api->destroyTuple(&m_tuple);
        m_tuple = HTUPLE_INITIALIZER;
    }

private:
    HalconCApi *m_api = nullptr;
    Htuple m_tuple = HTUPLE_INITIALIZER;
};

// 将 HALCON 错误码转换为可读文本。
QString halconErrorText(HalconCApi *api, const Herror status)
{
    char buffer[1024] = {0};
    if (api && api->getErrorText && halconStatusOk(api->getErrorText(static_cast<Hlong>(status), buffer))) {
        const QString message = QString::fromUtf8(buffer).trimmed();
        if (!message.isEmpty())
            return message;
    }
    return QStringLiteral("HALCON error code %1").arg(static_cast<qlonglong>(status));
}

// 检查 HALCON 调用结果，失败时抛出带 stage 的统一错误。
void checkStatus(HalconCApi *api, const Herror status, const QString &stage)
{
    if (!halconStatusOk(status)) {
        throw std::pair<QString, QString>(
                QStringLiteral("halcon_error"),
                QStringLiteral("%1: %2").arg(stage, halconErrorText(api, status)));
    }
}

// 将 C++ 特征向量转换为 HALCON HTuple。
HalconTuple featureToTuple(HalconCApi *api, const QVector<double> &feature)
{
    HalconTuple tuple(api);
    tuple.create(feature.size());
    for (int i = 0; i < feature.size(); ++i)
        tuple.setDouble(i, feature.at(i));
    return tuple;
}

// 调用 HALCON tuple_sum 并返回求和结果。
double tupleSumValue(HalconCApi *api, const HalconTuple &tuple, const QString &stage)
{
    HalconTuple sum(api);
    checkStatus(api, api->tupleSum(tuple.value(), sum.ptr()), stage + QStringLiteral(".tuple_sum"));
    return sum.size() > 0 ? sum.doubleAt(0) : 0.0;
}

// 将 double 数组写入已创建的 HALCON tuple。
void createDoubleArrayTuple(HalconTuple &tuple, const QVector<double> &values)
{
    tuple.create(values.size());
    for (int i = 0; i < values.size(); ++i)
        tuple.setDouble(i, values.at(i));
}

// 将归一化屏蔽多边形点转换为图像像素坐标。
QVector<QPointF> maskPolygonToPixels(const QVector<QPointF> &normalizedPoints,
                                     const int width,
                                     const int height)
{
    QVector<QPointF> pixels;
    if (width <= 0 || height <= 0 || normalizedPoints.size() < 3)
        return pixels;

    pixels.reserve(normalizedPoints.size());
    for (const QPointF &point : normalizedPoints) {
        if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
            continue;
        pixels.append(QPointF(qBound(0.0, point.x(), 1.0) * static_cast<double>(width - 1),
                              qBound(0.0, point.y(), 1.0) * static_cast<double>(height - 1)));
    }
    return pixels.size() >= 3 ? pixels : QVector<QPointF>();
}

// 通过 HALCON area_center 计算区域面积，用于判断有效 ROI 是否为空。
double regionArea(HalconCApi *api, const Hobject region, const QString &stage)
{
    HalconTuple area(api);
    HalconTuple row(api);
    HalconTuple column(api);
    checkStatus(api, api->areaCenter(region, area.ptr(), row.ptr(), column.ptr()),
                stage + QStringLiteral(".area_center"));
    return area.size() > 0 ? area.doubleAt(0) : 0.0;
}

// 使用 HALCON tuple_min2 + tuple_sum 计算直方图交集相似度。
double histogramIntersectionSimilarity(HalconCApi *api,
                                       const QVector<double> &queryFeature,
                                       const QVector<double> &sampleFeature)
{
    if (queryFeature.size() != sampleFeature.size() || queryFeature.isEmpty()) {
        throw std::pair<QString, QString>(
                QStringLiteral("invalid_feature_dimension"),
                QStringLiteral("Histogram feature dimensions do not match."));
    }

    HalconTuple queryTuple = featureToTuple(api, queryFeature);
    HalconTuple sampleTuple = featureToTuple(api, sampleFeature);
    HalconTuple minTuple(api);
    checkStatus(api, api->tupleMin2(queryTuple.value(), sampleTuple.value(), minTuple.ptr()),
                QStringLiteral("histogram_intersection.tuple_min2"));

    const double intersection = tupleSumValue(api, minTuple, QStringLiteral("histogram_intersection.minimum"));
    const double querySum = tupleSumValue(api, queryTuple, QStringLiteral("histogram_intersection.query"));
    const double sampleSum = tupleSumValue(api, sampleTuple, QStringLiteral("histogram_intersection.sample"));
    const double denominator = qMin(querySum, sampleSum);
    if (denominator <= 0.0)
        return 0.0;

    return qBound(0.0, intersection / denominator, 1.0);
}

struct ColorDecisionResult
{
    QString mode;
    QString comparisonMethod;
    QString predictedLabel;
    int predictedClassId = -1;
    int matchedSampleIndex = -1;
    QString matchedSampleLabel;
    double rating = 0.0;
    double score = 0.0;
    double secondClassScore = 0.0;
    double classMargin = 0.0;
    bool classificationAmbiguous = false;
    int topNSampleCount = 0;
    QVector<double> topNSimilarities;
    QVector<QPair<QString, double>> classSimilarities;
    QJsonArray classDiagnostics;
};

QJsonArray doubleArray(const QVector<double> &values, double scale = 1.0)
{
    QJsonArray result;
    for (double value : values)
        result.append(value * scale);
    return result;
}

// 根据 classId 在 labels 中查找显示名，找不到时回退到样本标签或 classId 文案。
QString labelNameForClass(const QVector<ColorRecognitionHalconLabel> &labels,
                          const QVector<ColorRecognitionHalconSample> &samples,
                          const int classId,
                          const QString &fallback)
{
    for (const ColorRecognitionHalconLabel &label : labels) {
        if (label.classId == classId && !label.name.trimmed().isEmpty())
            return label.name;
    }
    for (const ColorRecognitionHalconSample &sample : samples) {
        if (sample.classId == classId && !sample.label.trimmed().isEmpty())
            return sample.label;
    }
    return fallback.trimmed().isEmpty() ? QStringLiteral("#%1").arg(classId) : fallback;
}

// 保留旧版整体直方图交集判别：逐样本比较，选择相似度最高的样本。
ColorDecisionResult decideByHistogramIntersection(HalconCApi *api,
                                                  const ColorRecognitionHalconConfig &config,
                                                  const QVector<ColorRecognitionHalconSample> &usableSamples,
                                                  const QVector<double> &queryFeature)
{
    ColorDecisionResult decision;
    decision.mode = QStringLiteral("histogram_intersection");
    decision.comparisonMethod = QStringLiteral("histogram_intersection");

    double bestSimilarity = -1.0;
    int bestSampleIndex = -1;
    for (int sampleIndex = 0; sampleIndex < usableSamples.size(); ++sampleIndex) {
        const ColorRecognitionHalconSample &sample = usableSamples.at(sampleIndex);
        const double similarity = histogramIntersectionSimilarity(api, queryFeature, sample.feature);
        decision.classSimilarities.append(qMakePair(
                                              labelNameForClass(config.labels, usableSamples, sample.classId, sample.label),
                                              similarity));
        if (similarity > bestSimilarity) {
            bestSimilarity = similarity;
            bestSampleIndex = sampleIndex;
        }
    }

    if (bestSampleIndex < 0 || bestSimilarity < 0.0) {
        throw std::pair<QString, QString>(
                QStringLiteral("empty_similarity"),
                QStringLiteral("Histogram intersection returned no comparable sample."));
    }

    const ColorRecognitionHalconSample &bestSample = usableSamples.at(bestSampleIndex);
    decision.predictedClassId = bestSample.classId;
    decision.predictedLabel = labelNameForClass(config.labels, usableSamples, bestSample.classId, bestSample.label);
    decision.matchedSampleIndex = bestSampleIndex;
    decision.matchedSampleLabel = bestSample.label;
    decision.rating = qBound(0.0, bestSimilarity, 1.0);
    decision.score = qBound(0.0, decision.rating * 100.0, 100.0);
    return decision;
}

// 对每个类别取最佳模板的 HALCON 直方图交集，输出类别相似度而非像素面积占比。
ColorDecisionResult decideByDominantRatio(HalconCApi *api,
                                          const ColorRecognitionHalconConfig &config,
                                          const QVector<ColorRecognitionHalconSample> &usableSamples,
                                          const QVector<double> &queryFeature)
{
    ColorDecisionResult decision;
    // dominant_ratio 只作为旧配置输入保留；结果使用真实业务语义名称。
    decision.mode = QStringLiteral("category_similarity");
    decision.comparisonMethod = QStringLiteral("category_best_sample_histogram_intersection");

    QVector<int> classIds;
    QVector<double> classCoverage;
    QVector<int> classBestSampleIndex;
    QVector<double> classBestSimilarity;
    for (int sampleIndex = 0; sampleIndex < usableSamples.size(); ++sampleIndex) {
        const ColorRecognitionHalconSample &sample = usableSamples.at(sampleIndex);
        const double similarity = histogramIntersectionSimilarity(api, queryFeature, sample.feature);
        int classIndex = classIds.indexOf(sample.classId);
        if (classIndex < 0) {
            classIds.append(sample.classId);
            classCoverage.append(0.0);
            classBestSampleIndex.append(sampleIndex);
            classBestSimilarity.append(similarity);
            classIndex = classIds.size() - 1;
        }
        classCoverage[classIndex] = qMax(classCoverage.at(classIndex), similarity);
        if (similarity > classBestSimilarity.at(classIndex)) {
            classBestSimilarity[classIndex] = similarity;
            classBestSampleIndex[classIndex] = sampleIndex;
        }
    }

    if (classCoverage.isEmpty()) {
        throw std::pair<QString, QString>(
                QStringLiteral("empty_similarity"),
                QStringLiteral("Category similarity returned no comparable class."));
    }

    int bestClassIndex = -1;
    double bestSimilarity = -1.0;
    for (int i = 0; i < classIds.size(); ++i) {
        const QString label = labelNameForClass(config.labels, usableSamples, classIds.at(i), QString());
        const double similarity = qBound(0.0, classCoverage.at(i), 1.0);
        decision.classSimilarities.append(qMakePair(label, similarity));
        if (similarity > bestSimilarity) {
            bestSimilarity = similarity;
            bestClassIndex = i;
        }
    }

    if (bestClassIndex < 0) {
        throw std::pair<QString, QString>(
                QStringLiteral("empty_similarity"),
                QStringLiteral("Category similarity returned no predicted class."));
    }

    const int bestSampleIndex = classBestSampleIndex.at(bestClassIndex);
    const ColorRecognitionHalconSample &bestSample = usableSamples.at(bestSampleIndex);
    const double selectedSimilarity = qBound(0.0, classBestSimilarity.at(bestClassIndex), 1.0);
    decision.predictedClassId = classIds.at(bestClassIndex);
    decision.predictedLabel = labelNameForClass(config.labels,
                                                usableSamples,
                                                decision.predictedClassId,
                                                bestSample.label);
    decision.matchedSampleIndex = bestSampleIndex;
    decision.matchedSampleLabel = bestSample.label;
    decision.rating = selectedSimilarity;
    decision.score = qBound(0.0, decision.rating * 100.0, 100.0);
    return decision;
}

struct HsvClassScore
{
    int classId = -1;
    QString label;
    int declaredOrder = 0;
    int sampleCount = 0;
    int bestSampleIndex = -1;
    double score = 0.0;
    double bestSimilarity = 0.0;
    double worstTopNSimilarity = 0.0;
    QVector<double> topNSimilarities;
};

// 每类取最多三个最相似 ROI 的均值；类别之间独立聚合，避免全局 KNN 的样本数偏置。
ColorDecisionResult decideByClassTopNIntersection(
        HalconCApi *api,
        const ColorRecognitionHalconConfig &config,
        const QVector<ColorRecognitionHalconSample> &usableSamples,
        const QVector<double> &queryFeature)
{
    QHash<int, QVector<QPair<double, int>>> similaritiesByClass;
    QVector<int> classOrder;
    QHash<int, QString> labelByClass;
    for (const ColorRecognitionHalconLabel &label : config.labels) {
        if (!classOrder.contains(label.classId)) {
            classOrder.append(label.classId);
            labelByClass.insert(label.classId, label.name.trimmed());
        }
    }
    for (int sampleIndex = 0; sampleIndex < usableSamples.size(); ++sampleIndex) {
        const ColorRecognitionHalconSample &sample = usableSamples.at(sampleIndex);
        if (!classOrder.contains(sample.classId))
            classOrder.append(sample.classId);
        if (!labelByClass.contains(sample.classId))
            labelByClass.insert(sample.classId, sample.label.trimmed());
        similaritiesByClass[sample.classId].append(qMakePair(
                histogramIntersectionSimilarity(api, queryFeature, sample.feature), sampleIndex));
    }

    QVector<HsvClassScore> classes;
    for (int order = 0; order < classOrder.size(); ++order) {
        const int classId = classOrder.at(order);
        QVector<QPair<double, int>> similarities = similaritiesByClass.value(classId);
        if (similarities.isEmpty())
            continue;
        std::stable_sort(similarities.begin(), similarities.end(),
                         [](const QPair<double, int> &left,
                            const QPair<double, int> &right) {
            if (std::abs(left.first - right.first) > 1e-12)
                return left.first > right.first;
            return left.second < right.second;
        });

        HsvClassScore item;
        item.classId = classId;
        item.label = labelNameForClass(config.labels, usableSamples, classId,
                                       labelByClass.value(classId));
        item.declaredOrder = order;
        item.sampleCount = similarities.size();
        item.bestSampleIndex = similarities.first().second;
        item.bestSimilarity = similarities.first().first;
        const int topN = qMin(kHsvClassTopN, similarities.size());
        double sum = 0.0;
        for (int index = 0; index < topN; ++index) {
            const double similarity = qBound(0.0, similarities.at(index).first, 1.0);
            item.topNSimilarities.append(similarity);
            sum += similarity;
        }
        item.score = topN > 0 ? sum / topN : 0.0;
        item.worstTopNSimilarity = item.topNSimilarities.isEmpty()
                ? 0.0 : item.topNSimilarities.last();
        classes.append(item);
    }
    if (classes.isEmpty()) {
        throw std::pair<QString, QString>(
                QStringLiteral("empty_similarity"),
                QStringLiteral("Top-N category similarity returned no comparable class."));
    }

    std::stable_sort(classes.begin(), classes.end(),
                     [](const HsvClassScore &left, const HsvClassScore &right) {
        if (std::abs(left.score - right.score) > 1e-12)
            return left.score > right.score;
        if (std::abs(left.worstTopNSimilarity - right.worstTopNSimilarity) > 1e-12)
            return left.worstTopNSimilarity > right.worstTopNSimilarity;
        if (std::abs(left.bestSimilarity - right.bestSimilarity) > 1e-12)
            return left.bestSimilarity > right.bestSimilarity;
        return left.declaredOrder < right.declaredOrder;
    });

    ColorDecisionResult decision;
    decision.mode = QStringLiteral("category_similarity");
    decision.comparisonMethod = QStringLiteral("class_top_n_mean_histogram_intersection");
    const HsvClassScore &best = classes.first();
    decision.predictedClassId = best.classId;
    decision.predictedLabel = best.label;
    decision.matchedSampleIndex = best.bestSampleIndex;
    decision.matchedSampleLabel = usableSamples.at(best.bestSampleIndex).label;
    decision.rating = qBound(0.0, best.score, 1.0);
    decision.score = decision.rating * 100.0;
    decision.topNSampleCount = best.topNSimilarities.size();
    decision.topNSimilarities = best.topNSimilarities;
    decision.secondClassScore = classes.size() > 1 ? classes.at(1).score * 100.0 : 0.0;
    decision.classMargin = classes.size() > 1
            ? qMax(0.0, decision.score - decision.secondClassScore) : decision.score;
    decision.classificationAmbiguous = classes.size() > 1 && decision.classMargin <= 1e-9;

    for (const HsvClassScore &item : std::as_const(classes)) {
        decision.classSimilarities.append(qMakePair(item.label, item.score));
        QJsonObject diagnostics;
        diagnostics.insert(QStringLiteral("classId"), item.classId);
        diagnostics.insert(QStringLiteral("label"), item.label);
        diagnostics.insert(QStringLiteral("sampleCount"), item.sampleCount);
        diagnostics.insert(QStringLiteral("topNSampleCount"), item.topNSimilarities.size());
        diagnostics.insert(QStringLiteral("topNSimilarities"),
                           doubleArray(item.topNSimilarities, 100.0));
        diagnostics.insert(QStringLiteral("topNMeanSimilarity"), item.score * 100.0);
        diagnostics.insert(QStringLiteral("bestSampleSimilarity"), item.bestSimilarity * 100.0);
        diagnostics.insert(QStringLiteral("worstTopNSimilarity"),
                           item.worstTopNSimilarity * 100.0);
        diagnostics.insert(QStringLiteral("bestSampleIndex"), item.bestSampleIndex);
        decision.classDiagnostics.append(diagnostics);
    }
    return decision;
}

// 预留 HALCON 颜色分割/聚类接口，后续可替换当前直方图占比近似实现。
ColorDecisionResult decideByHalconColorSegmentationReserved(HalconCApi *api,
                                                            const ColorRecognitionHalconConfig &config,
                                                            const QVector<ColorRecognitionHalconSample> &usableSamples,
                                                            const QVector<double> &queryFeature)
{
    Q_UNUSED(api)
    Q_UNUSED(config)
    Q_UNUSED(usableSamples)
    Q_UNUSED(queryFeature)
    throw std::pair<QString, QString>(
            QStringLiteral("unsupported_feature"),
            QStringLiteral("HALCON color segmentation/cluster mode is reserved but not implemented."));
}

// 安全释放 HALCON Hobject，并把句柄重置为空对象。
void clearObject(HalconCApi *api, Hobject &object)
{
    if (api && api->clearObj && halconObjectAllocated(object))
        api->clearObj(object);
    object = NO_OBJECTS;
}

// 在指定 HALCON ROI 区域内提取单通道灰度直方图并归一化。
QVector<double> histogramForChannel(HalconCApi *api,
                                    const Hobject roiRegion,
                                    const Hobject channel,
                                    const int bins,
                                    const QString &stage)
{
    Hobject reducedChannel = NO_OBJECTS;
    HalconTuple minValue(api);
    HalconTuple maxValue(api);
    HalconTuple binCount(api);
    HalconTuple histo(api);
    HalconTuple binSize(api);

    minValue.create(1);
    minValue.setDouble(0, 0.0);
    maxValue.create(1);
    maxValue.setDouble(0, 255.0);
    binCount.create(1);
    binCount.setInt(0, bins);

    checkStatus(api, api->reduceDomain(channel, roiRegion, &reducedChannel),
                stage + QStringLiteral(".reduce_domain"));
    try {
        checkStatus(api, api->grayHistoRange(roiRegion,
                                             reducedChannel,
                                             minValue.value(),
                                             maxValue.value(),
                                             binCount.value(),
                                             histo.ptr(),
                                             binSize.ptr()),
                    stage + QStringLiteral(".gray_histo_range"));
    } catch (...) {
        clearObject(api, reducedChannel);
        throw;
    }
    clearObject(api, reducedChannel);

    double sum = 0.0;
    const int histoSize = histo.size();
    for (int i = 0; i < histoSize; ++i) {
        const Hlong value = histo.intAt(i);
        sum += static_cast<double>(value);
    }
    if (sum <= 0.0)
        sum = 1.0;

    QVector<double> normalized;
    normalized.reserve(histoSize);
    for (int i = 0; i < histoSize; ++i) {
        const Hlong value = histo.intAt(i);
        normalized.append(static_cast<double>(value) / sum);
    }
    return normalized;
}

// 将任意直方图向量按非负和归一化。
QVector<double> normalizeHistogramVector(const QVector<double> &histogram)
{
    double sum = 0.0;
    for (const double value : histogram)
        sum += qMax(0.0, value);

    QVector<double> normalized;
    normalized.reserve(histogram.size());
    if (sum <= 0.0) {
        for (int i = 0; i < histogram.size(); ++i)
            normalized.append(0.0);
        return normalized;
    }

    for (const double value : histogram)
        normalized.append(qMax(0.0, value) / sum);
    return normalized;
}

// 对 H/S 二维联合直方图做各向异性 3x3 平滑：H 循环，S 不循环。
QVector<double> softenHsJointHistogram(const QVector<double> &histogram, int bins)
{
    QVector<double> softened(bins * bins, 0.0);
    if (histogram.size() != bins * bins)
        return normalizeHistogramVector(histogram);

    constexpr double kHueWeights[3] = {0.25, 0.50, 0.25};
    constexpr double kSatWeights[3] = {0.15, 0.70, 0.15};
    for (int hueBin = 0; hueBin < bins; ++hueBin) {
        for (int saturationBin = 0; saturationBin < bins; ++saturationBin) {
            const double value = qMax(0.0, histogram.at(hueBin * bins + saturationBin));
            if (value <= 0.0)
                continue;

            for (int hueOffset = -1; hueOffset <= 1; ++hueOffset) {
                const int targetHue = (hueBin + hueOffset + bins) % bins;
                const double hueWeight = kHueWeights[hueOffset + 1];

                double validSaturationWeight = 0.0;
                for (int satOffset = -1; satOffset <= 1; ++satOffset) {
                    const int targetSaturation = saturationBin + satOffset;
                    if (targetSaturation >= 0 && targetSaturation < bins)
                        validSaturationWeight += kSatWeights[satOffset + 1];
                }
                for (int satOffset = -1; satOffset <= 1; ++satOffset) {
                    const int targetSaturation = saturationBin + satOffset;
                    if (targetSaturation < 0 || targetSaturation >= bins)
                        continue;
                    const double saturationWeight =
                            kSatWeights[satOffset + 1] / validSaturationWeight;
                    softened[targetHue * bins + targetSaturation] +=
                            value * hueWeight * saturationWeight;
                }
            }
        }
    }

    return normalizeHistogramVector(softened);
}

// 调用 HALCON histo_2dim 提取 H/S 联合直方图并降采样到配置 bin 数。
QVector<double> histogram2DimHsFeature(HalconCApi *api,
                                       const Hobject roiRegion,
                                       const Hobject hue,
                                       const Hobject saturation,
                                       const int bins)
{
    Hobject histo2Dim = NO_OBJECTS;
    auto cleanup = [&]() {
        clearObject(api, histo2Dim);
    };

    try {
        checkStatus(api,
                    api->histo2Dim(roiRegion, hue, saturation, &histo2Dim),
                    QStringLiteral("histo_2dim.hue_saturation"));

        QVector<double> rows;
        QVector<double> columns;
        rows.reserve(256 * 256);
        columns.reserve(256 * 256);
        for (int row = 0; row < 256; ++row) {
            for (int column = 0; column < 256; ++column) {
                rows.append(row);
                columns.append(column);
            }
        }

        HalconTuple rowTuple(api);
        HalconTuple columnTuple(api);
        HalconTuple grayValues(api);
        createDoubleArrayTuple(rowTuple, rows);
        createDoubleArrayTuple(columnTuple, columns);
        checkStatus(api,
                    api->getGrayval(histo2Dim, rowTuple.value(), columnTuple.value(), grayValues.ptr()),
                    QStringLiteral("histo_2dim.get_grayval"));

        QVector<double> jointHistogram(bins * bins, 0.0);
        for (int row = 0; row < 256; ++row) {
            const int hueBin = qBound(0, row * bins / 256, bins - 1);
            for (int column = 0; column < 256; ++column) {
                const int saturationBin = qBound(0, column * bins / 256, bins - 1);
                const int index = row * 256 + column;
                jointHistogram[hueBin * bins + saturationBin] +=
                        qMax(0.0, grayValues.doubleAt(index));
            }
        }

        cleanup();
        return softenHsJointHistogram(jointHistogram, bins);
    } catch (...) {
        cleanup();
        throw;
    }
}

struct HistogramExtractionResult
{
    QVector<double> feature;
    bool detectMaskApplied = false;
    bool detectMaskFullyCoversRoi = false;
    bool circleDetectRoiApplied = false;
    double effectiveRoiArea = 0.0;
};

// 颜色特征提取核心：OpenCV 图像桥接 HALCON，生成 ROI/屏蔽区后提取 HSV 直方图。
HistogramExtractionResult extractHistogramFeature(const cv::Mat &bgr,
                                                  const QRect &roiPixels,
                                                  const ColorRecognitionHalconConfig &config,
                                                  HalconCApi *api)
{
    Hobject image = NO_OBJECTS;
    Hobject red = NO_OBJECTS;
    Hobject green = NO_OBJECTS;
    Hobject blue = NO_OBJECTS;
    Hobject scaledRed = NO_OBJECTS;
    Hobject scaledGreen = NO_OBJECTS;
    Hobject scaledBlue = NO_OBJECTS;
    Hobject byteRed = NO_OBJECTS;
    Hobject byteGreen = NO_OBJECTS;
    Hobject byteBlue = NO_OBJECTS;
    Hobject hue = NO_OBJECTS;
    Hobject saturation = NO_OBJECTS;
    Hobject value = NO_OBJECTS;
    Hobject roiRegion = NO_OBJECTS;
    Hobject maskRegion = NO_OBJECTS;
    Hobject effectiveRegion = NO_OBJECTS;

    auto cleanup = [&]() {
        if (effectiveRegion != roiRegion)
            clearObject(api, effectiveRegion);
        clearObject(api, maskRegion);
        clearObject(api, roiRegion);
        clearObject(api, value);
        clearObject(api, saturation);
        clearObject(api, hue);
        clearObject(api, byteBlue);
        clearObject(api, byteGreen);
        clearObject(api, byteRed);
        clearObject(api, scaledBlue);
        clearObject(api, scaledGreen);
        clearObject(api, scaledRed);
        clearObject(api, blue);
        clearObject(api, green);
        clearObject(api, red);
        clearObject(api, image);
    };

    try {
        const bool input16 = bgr.depth() == CV_16U;
        const int validBits = resolvedValidBits(config, bgr);
        const int bitShift = resolvedBitShift(config, bgr);
        checkStatus(api, api->genImageInterleaved(&image,
                                                  reinterpret_cast<Hlong>(bgr.data),
                                                  input16 ? "bgr48" : "bgr",
                                                  bgr.cols,
                                                  bgr.rows,
                                                  0,
                                                  input16 ? "uint2" : "byte",
                                                  0,
                                                  0,
                                                  0,
                                                  0,
                                                  validBits,
                                                  bitShift),
                    QStringLiteral("gen_image_interleaved"));
        checkStatus(api, api->decompose3(image, &red, &green, &blue),
                    QStringLiteral("decompose3"));
        Hobject hsvRed = red;
        Hobject hsvGreen = green;
        Hobject hsvBlue = blue;
        if (input16) {
            const double maxCode = static_cast<double>((quint64(1) << validBits) - 1U);
            const double multiplier = 255.0 / maxCode;
            checkStatus(api, api->scaleImage(red, &scaledRed, multiplier, 0.0),
                        QStringLiteral("scale_image.red"));
            checkStatus(api, api->scaleImage(green, &scaledGreen, multiplier, 0.0),
                        QStringLiteral("scale_image.green"));
            checkStatus(api, api->scaleImage(blue, &scaledBlue, multiplier, 0.0),
                        QStringLiteral("scale_image.blue"));
            checkStatus(api, api->convertImageType(scaledRed, &byteRed, "byte"),
                        QStringLiteral("convert_image_type.red"));
            checkStatus(api, api->convertImageType(scaledGreen, &byteGreen, "byte"),
                        QStringLiteral("convert_image_type.green"));
            checkStatus(api, api->convertImageType(scaledBlue, &byteBlue, "byte"),
                        QStringLiteral("convert_image_type.blue"));
            hsvRed = byteRed;
            hsvGreen = byteGreen;
            hsvBlue = byteBlue;
        }
        checkStatus(api, api->transFromRgb(hsvRed, hsvGreen, hsvBlue,
                                           &hue, &saturation, &value, "hsv"),
                    QStringLiteral("trans_from_rgb"));
        HistogramExtractionResult result;

        //生成圆形或矩形区域
        if (isCircleRegionType(config.detectRegionType)) {
            const double radiusPixels = config.detectCircleRadiusNormalized *
                    static_cast<double>(qMax(bgr.cols, bgr.rows));
            if (radiusPixels < static_cast<double>(kMinRoiPixelSize)) {
                throw std::pair<QString, QString>(
                        QStringLiteral("invalid_roi"),
                        QStringLiteral("Color recognition circle ROI is invalid."));
            }
            const double centerColumn =
                    qBound(0.0,
                           config.detectCircleCenterNormalized.x() * static_cast<double>(bgr.cols),
                           static_cast<double>(bgr.cols - 1));
            const double centerRow =
                    qBound(0.0,
                           config.detectCircleCenterNormalized.y() * static_cast<double>(bgr.rows),
                           static_cast<double>(bgr.rows - 1));
            checkStatus(api, api->genCircle(&roiRegion,
                                            centerRow,
                                            centerColumn,
                                            radiusPixels),
                        QStringLiteral("gen_circle.detect_roi"));
            result.circleDetectRoiApplied = true;
        } else {
            checkStatus(api, api->genRectangle1(&roiRegion,
                                                roiPixels.y(),
                                                roiPixels.x(),
                                                roiPixels.y() + roiPixels.height() - 1,
                                                roiPixels.x() + roiPixels.width() - 1),
                        QStringLiteral("gen_rectangle1"));
        }
        effectiveRegion = roiRegion;

        //多边形有效屏蔽区域
        const QVector<QPointF> maskPixels =
                maskPolygonToPixels(config.detectMaskPolygonNormalized, bgr.cols, bgr.rows);
        if (maskPixels.size() >= 3) {
            QVector<double> rows;
            QVector<double> columns;
            rows.reserve(maskPixels.size());
            columns.reserve(maskPixels.size());
            for (const QPointF &point : maskPixels) {
                rows.append(qBound(0.0, point.y(), static_cast<double>(bgr.rows - 1)));
                columns.append(qBound(0.0, point.x(), static_cast<double>(bgr.cols - 1)));
            }

            HalconTuple rowTuple(api);
            HalconTuple columnTuple(api);
            createDoubleArrayTuple(rowTuple, rows);
            createDoubleArrayTuple(columnTuple, columns);
            checkStatus(api,
                        api->genRegionPolygonFilled(&maskRegion, rowTuple.value(), columnTuple.value()),
                        QStringLiteral("gen_region_polygon_filled.detect_mask"));
            checkStatus(api,
                        api->difference(roiRegion, maskRegion, &effectiveRegion),
                        QStringLiteral("difference.detect_mask"));
            result.detectMaskApplied = true;
        }

        result.effectiveRoiArea = regionArea(api, effectiveRegion, QStringLiteral("effective_roi"));
        if (result.effectiveRoiArea <= 0.0) {
            result.detectMaskFullyCoversRoi = result.detectMaskApplied;
            throw std::pair<QString, QString>(
                    QStringLiteral("masked_roi_empty"),
                    QStringLiteral("Detection ROI is fully covered by the mask ROI."));
        }
        
        //获取灵敏度，决定bin数
        const int bins = histogramBinsForSensitivity(config.sensitivity);
        if (isHisto2DimFeatureType(config.featureType)) {
            result.feature = histogram2DimHsFeature(api, effectiveRegion, hue, saturation, bins);
        } else {
            const bool includeValue = effectiveBrightnessEnabled(config);
            result.feature.reserve(includeValue ? bins * 3 : bins * 2);
            result.feature += histogramForChannel(api, effectiveRegion, hue, bins, QStringLiteral("hue"));
            result.feature += histogramForChannel(api, effectiveRegion, saturation, bins, QStringLiteral("saturation"));
            if (includeValue)
                result.feature += histogramForChannel(api, effectiveRegion, value, bins, QStringLiteral("value"));
        }

        cleanup();
        return result;
    } catch (...) {
        cleanup();
        throw;
    }
}

} // namespace

QString colorRecognitionHsvCurrentClassifierVersion()
{
    return QString::fromLatin1(kCurrentHsvClassifierVersion);
}

QString colorRecognitionHsvLegacyClassifierVersion()
{
    return QString::fromLatin1(kLegacyHsvClassifierVersion);
}

QString colorRecognitionHsvClassifierParamsHash(const QString &classifierVersion)
{
    const QString version = classifierVersion.trimmed();
    QJsonObject contract;
    contract.insert(QStringLiteral("classifierVersion"), version);
    contract.insert(QStringLiteral("similarityMetric"),
                    QStringLiteral("halcon_histogram_intersection"));
    if (version == QString::fromLatin1(kCurrentHsvClassifierVersion)) {
        contract.insert(QStringLiteral("aggregation"), QStringLiteral("class_top_n_mean"));
        contract.insert(QStringLiteral("topN"), kHsvClassTopN);
        contract.insert(QStringLiteral("weights"), QStringLiteral("uniform"));
        contract.insert(QStringLiteral("tieBreak"),
                        QStringLiteral("mean_worst_best_declared_order"));
    } else if (version == QString::fromLatin1(kLegacyHsvClassifierVersion)) {
        contract.insert(QStringLiteral("aggregation"), QStringLiteral("best_sample_per_class"));
        contract.insert(QStringLiteral("topN"), 1);
        contract.insert(QStringLiteral("weights"), QStringLiteral("not_applicable"));
        contract.insert(QStringLiteral("tieBreak"), QStringLiteral("sample_order"));
    } else {
        return QString();
    }

    const QByteArray digest = QCryptographicHash::hash(
                QJsonDocument(contract).toJson(QJsonDocument::Compact),
                QCryptographicHash::Sha256).toHex();
    return QStringLiteral("sha256:") + QString::fromLatin1(digest);
}

// 从输入图像中提取颜色识别特征，负责 HALCON runtime 校验、ROI 校验和诊断 payload。
ColorRecognitionHalconFeatureResult ColorRecognitionHalconRunner::extractFeature(
        const cv::Mat &image,
        const ColorRecognitionHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    try {
        if (config.featureType.trimmed().toLower() == QStringLiteral("spectrum")) {
            return featureError(QStringLiteral("unsupported_feature"),
                                QStringLiteral("Spectrum feature is reserved but not implemented."),
                                config,
                                image,
                                timer.elapsed());
        }
        if (config.featureType.trimmed().toLower() != QStringLiteral("histogram") &&
                !isHisto2DimFeatureType(config.featureType)) {
            return featureError(QStringLiteral("unsupported_feature"),
                                QStringLiteral("Only histogram and histogram_2dim_hs features are supported."),
                                config,
                                image,
                                timer.elapsed());
        }

        QString inputStatus;
        QString inputMessage;
        if (!validInputBitContract(image, config, &inputStatus, &inputMessage)) {
            return featureError(inputStatus,
                                inputMessage,
                                config,
                                image,
                                timer.elapsed());
        }
        const cv::Mat bgr = toBgrPreservingDepth(image);
        if (bgr.empty()) {
            return featureError(QStringLiteral("unsupported_image_format"),
                                QStringLiteral("Input image cannot be bridged to HALCON BGR."),
                                config,
                                image,
                                timer.elapsed());
        }

        const QRect roiPixels = normalizedRoiToPixels(config.roiNormalized, bgr.cols, bgr.rows);
        if (roiPixels.isEmpty()) {
            return featureError(QStringLiteral("invalid_roi"),
                                QStringLiteral("Color recognition ROI is invalid."),
                                config,
                                image,
                                timer.elapsed());
        }

        if (config.halconSoPath.trimmed().isEmpty() || !QFileInfo::exists(config.halconSoPath)) {
            const QString triedPaths = config.halconSoPathCandidates.isEmpty()
                    ? config.halconSoPath
                    : config.halconSoPathCandidates.join(QStringLiteral("; "));
            return featureError(QStringLiteral("halcon_so_not_found"),
                                QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                                .arg(config.halconSoPath, triedPaths),
                                config,
                                image,
                                timer.elapsed());
        }

        QString loadMessage;
        bool symbolMissing = false;
        const std::shared_ptr<HalconLibrary> library =
                acquireHalconLibrary(config.halconSoPath, loadMessage, symbolMissing);
        if (!library) {
            return featureError(symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                              : QStringLiteral("halcon_load_failed"),
                                loadMessage,
                                config,
                                image,
                                timer.elapsed());
        }

        const HistogramExtractionResult histogramResult =
                extractHistogramFeature(bgr, roiPixels, config, &library->api);
        ColorRecognitionHalconFeatureResult result;
        result.feature = histogramResult.feature;
        result.success = true;
        result.status = QStringLiteral("ok");
        result.message = QStringLiteral("feature extracted");
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_hsv_histogram_transition"));
        result.payload.insert(QStringLiteral("algorithmVersion"), QString::fromLatin1(kAlgorithmVersion));
        result.payload.insert(QStringLiteral("featureType"), actualFeatureType(config));
        result.payload.insert(QStringLiteral("featureSchemaVersion"), featureSchemaVersion(config));
        result.payload.insert(QStringLiteral("featureSignature"), featureSignature(config, image));
        result.payload.insert(QStringLiteral("requestedFeatureType"), config.featureType);
        result.payload.insert(QStringLiteral("featureLength"), result.feature.size());
        result.payload.insert(QStringLiteral("histogramBins"), histogramBinsForSensitivity(config.sensitivity));
        result.payload.insert(QStringLiteral("brightnessEnabled"), effectiveBrightnessEnabled(config));
        result.payload.insert(QStringLiteral("requestedBrightnessEnabled"), config.brightnessEnabled);
        result.payload.insert(QStringLiteral("pixelFormat"), inferredPixelFormat(config, image));
        result.payload.insert(QStringLiteral("validBits"), resolvedValidBits(config, image));
        result.payload.insert(QStringLiteral("bitShift"), resolvedBitShift(config, image));
        result.payload.insert(QStringLiteral("lightingNormalizationMode"),
                              isHisto2DimFeatureType(config.featureType)
                              ? QStringLiteral("hue_saturation_2dim_soft_kernel")
                              :
                              effectiveBrightnessEnabled(config)
                              ? QStringLiteral("include_value_channel")
                              : QStringLiteral("hue_saturation_priority"));
        result.payload.insert(QStringLiteral("roiPixelsRect"), rectToJson(QRectF(roiPixels)));
        result.payload.insert(QStringLiteral("detectRegionType"),
                              isCircleRegionType(config.detectRegionType)
                              ? QStringLiteral("circle")
                              : QStringLiteral("rectangle"));
        result.payload.insert(QStringLiteral("circleDetectRoiApplied"),
                              histogramResult.circleDetectRoiApplied);
        result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                              pointToJsonObject(config.detectCircleCenterNormalized));
        result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                              config.detectCircleRadiusNormalized);
        result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                              rectToJson(config.detectCircleBoundingRectNormalized));
        result.payload.insert(QStringLiteral("detectMaskApplied"), histogramResult.detectMaskApplied);
        result.payload.insert(QStringLiteral("detectMaskFullyCoversRoi"), false);
        result.payload.insert(QStringLiteral("effectiveRoiArea"), histogramResult.effectiveRoiArea);
        result.payload.insert(QStringLiteral("detectMaskPolygon"), pointsToJson(config.detectMaskPolygonNormalized));
        result.payload.insert(QStringLiteral("positionCorrectionSourceId"), config.positionCorrectionSourceId);
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const std::pair<QString, QString> &error) {
        return featureError(error.first, error.second, config, image, timer.elapsed());
    } catch (const std::exception &error) {
        return featureError(QStringLiteral("exception"),
                            QString::fromLocal8Bit(error.what()),
                            config,
                            image,
                            timer.elapsed());
    } catch (...) {
        return featureError(QStringLiteral("exception"),
                            QStringLiteral("Unknown exception while extracting color feature."),
                            config,
                            image,
                            timer.elapsed());
    }
}

// 使用 HALCON tuple 算子计算直方图 Bhattacharyya 距离，主要供颜色比较链路复用。
ColorRecognitionHalconHistogramCompareResult
ColorRecognitionHalconRunner::compareHistogramBhattacharyya(
        const QVector<double> &referenceHistogram,
        const QVector<double> &testHistogram,
        const ColorRecognitionHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    ColorRecognitionHalconHistogramCompareResult result;
    result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_tuple_bhattacharyya"));
    result.payload.insert(QStringLiteral("method"), QStringLiteral("bhattacharyya"));
    result.payload.insert(QStringLiteral("halconOperators"),
                          QStringLiteral("T_tuple_mult,T_tuple_sqrt,T_tuple_sum"));
    result.payload.insert(QStringLiteral("featureLength"), qMin(referenceHistogram.size(),
                                                                testHistogram.size()));

    try {
        if (referenceHistogram.isEmpty() || referenceHistogram.size() != testHistogram.size()) {
            result.status = QStringLiteral("invalid_feature_dimension");
            result.message = QStringLiteral("Histogram feature dimensions do not match.");
            result.elapsedMs = timer.elapsed();
            return result;
        }

        if (config.halconSoPath.trimmed().isEmpty() || !QFileInfo::exists(config.halconSoPath)) {
            const QString triedPaths = config.halconSoPathCandidates.isEmpty()
                    ? config.halconSoPath
                    : config.halconSoPathCandidates.join(QStringLiteral("; "));
            result.status = QStringLiteral("halcon_so_not_found");
            result.message = QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                    .arg(config.halconSoPath, triedPaths);
            result.elapsedMs = timer.elapsed();
            return result;
        }

        QString loadMessage;
        bool symbolMissing = false;
        const std::shared_ptr<HalconLibrary> library =
                acquireHalconLibrary(config.halconSoPath, loadMessage, symbolMissing);
        if (!library) {
            result.status = symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                          : QStringLiteral("halcon_load_failed");
            result.message = loadMessage;
            result.elapsedMs = timer.elapsed();
            return result;
        }

        HalconTuple referenceTuple = featureToTuple(&library->api, referenceHistogram);
        HalconTuple testTuple = featureToTuple(&library->api, testHistogram);
        const double referenceSum = tupleSumValue(&library->api,
                                                  referenceTuple,
                                                  QStringLiteral("bhattacharyya.reference"));
        const double testSum = tupleSumValue(&library->api,
                                             testTuple,
                                             QStringLiteral("bhattacharyya.test"));
        if (referenceSum <= 0.0 || testSum <= 0.0) {
            result.status = QStringLiteral("empty_histogram");
            result.message = QStringLiteral("Histogram sum is zero.");
            result.elapsedMs = timer.elapsed();
            return result;
        }

        HalconTuple multipliedTuple(&library->api);
        checkStatus(&library->api,
                    library->api.tupleMult(referenceTuple.value(),
                                          testTuple.value(),
                                          multipliedTuple.ptr()),
                    QStringLiteral("bhattacharyya.tuple_mult"));

        HalconTuple coefficientTerms(&library->api);
        checkStatus(&library->api,
                    library->api.tupleSqrt(multipliedTuple.value(), coefficientTerms.ptr()),
                    QStringLiteral("bhattacharyya.tuple_sqrt"));

        const double coefficientRaw = tupleSumValue(&library->api,
                                                    coefficientTerms,
                                                    QStringLiteral("bhattacharyya.coefficient"));
        const double coefficient = qBound(0.0,
                                          coefficientRaw / std::sqrt(referenceSum * testSum),
                                          1.0);
        const double safeCoefficient = qMax(coefficient, std::numeric_limits<double>::min());
        const double distance = -std::log(safeCoefficient);
        result.success = true;
        result.status = QStringLiteral("ok");
        result.distance = qMax(0.0, distance);
        result.message = QStringLiteral("distance=%1").arg(QString::number(result.distance, 'f', 6));
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("coefficient"), coefficient);
        result.payload.insert(QStringLiteral("referenceHistogramSum"), referenceSum);
        result.payload.insert(QStringLiteral("testHistogramSum"), testSum);
        result.payload.insert(QStringLiteral("distance"), result.distance);
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const std::pair<QString, QString> &error) {
        result.success = false;
        result.status = error.first;
        result.message = error.second;
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (const std::exception &error) {
        result.success = false;
        result.status = QStringLiteral("exception");
        result.message = QString::fromLocal8Bit(error.what());
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    } catch (...) {
        result.success = false;
        result.status = QStringLiteral("exception");
        result.message = QStringLiteral("Unknown exception while comparing histograms.");
        result.elapsedMs = timer.elapsed();
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
        return result;
    }
}

// 颜色识别主流程：提取检测 ROI 特征、对齐模板特征、分类判定并输出 ToolOverlay。
ColorRecognitionHalconResult ColorRecognitionHalconRunner::run(
        const cv::Mat &image,
        const ColorRecognitionHalconConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    if (config.enablePositionCorrection) {
        return runError(QStringLiteral("unsupported_position_correction"),
                        QStringLiteral("Position correction is not implemented for color recognition."),
                        config,
                        image,
                        timer.elapsed());
    }

    if (config.featureType.trimmed().toLower() == QStringLiteral("spectrum")) {
        return runError(QStringLiteral("unsupported_feature"),
                        QStringLiteral("Spectrum feature is reserved but not implemented."),
                        config,
                        image,
                        timer.elapsed());
    }//判断是否选择了色谱方式，暂未实现

    const ColorRecognitionHalconFeatureResult featureResult = extractFeature(image, config);
    if (!featureResult.success) {
        if (featureResult.status == QStringLiteral("masked_roi_empty"))
            return maskedRoiResult(config, image, timer.elapsed());

        ColorRecognitionHalconResult error = runError(featureResult.status,
                                                      featureResult.message,
                                                      config,
                                                      image,
                                                      timer.elapsed());
        error.payload.insert(QStringLiteral("featurePayload"), featureResult.payload);
        return error;
    }//特征直方图取值，与阈值进行判断是否达标

    if (config.samples.isEmpty()) {
        return runError(QStringLiteral("no_model_samples"),
                        QStringLiteral("Color model has no trained samples."),
                        config,
                        image,
                        timer.elapsed());
    }

    const QVector<double> comparisonFeature = featureResult.feature;
    const QString queryFeatureSignature = featureResult.payload
            .value(QStringLiteral("featureSignature")).toString();
    QVector<ColorRecognitionHalconSample> usableSamples;
    usableSamples.reserve(config.samples.size());
    for (const ColorRecognitionHalconSample &sample : config.samples) {
        if (sample.featureSignature.trimmed().isEmpty() ||
            sample.featureSignature != queryFeatureSignature ||
            sample.feature.size() != comparisonFeature.size()) {
            ColorRecognitionHalconResult error = runError(
                        QStringLiteral("model_stale_needs_resample"),
                        QStringLiteral("Color model feature signature does not match current parameters; resample the template."),
                        config,
                        image,
                        timer.elapsed());
            error.payload.insert(QStringLiteral("expectedFeatureSignature"), queryFeatureSignature);
            error.payload.insert(QStringLiteral("sampleFeatureSignature"), sample.featureSignature);
            error.payload.insert(QStringLiteral("expectedFeatureLength"), comparisonFeature.size());
            error.payload.insert(QStringLiteral("sampleFeatureLength"), sample.feature.size());
            return error;
        }
        usableSamples.append(sample);
    }
    if (usableSamples.isEmpty()) {
        return runError(QStringLiteral("invalid_model_samples"),
                        QStringLiteral("Color model samples do not match current feature dimension."),
                        config,
                        image,
                        timer.elapsed());
    }

    const QString classifierVersion = config.classifierVersion.trimmed();
    const QString expectedClassifierHash =
            colorRecognitionHsvClassifierParamsHash(classifierVersion);
    if (expectedClassifierHash.isEmpty()) {
        ColorRecognitionHalconResult error = runError(
                    QStringLiteral("unsupported_hsv_classifier"),
                    QStringLiteral("HSV classifier version is unknown; rebuild or migrate the template."),
                    config, image, timer.elapsed());
        error.payload.insert(QStringLiteral("classifierVersion"), classifierVersion);
        return error;
    }
    if (config.classifierParamsHash.trimmed() != expectedClassifierHash) {
        ColorRecognitionHalconResult error = runError(
                    QStringLiteral("hsv_classifier_signature_mismatch"),
                    QStringLiteral("HSV classifier parameters do not match the persisted classifier signature."),
                    config, image, timer.elapsed());
        error.payload.insert(QStringLiteral("classifierVersion"), classifierVersion);
        error.payload.insert(QStringLiteral("expectedClassifierParamsHash"), expectedClassifierHash);
        error.payload.insert(QStringLiteral("classifierParamsHash"),
                             config.classifierParamsHash.trimmed());
        return error;
    }

    QString loadMessage;
    bool symbolMissing = false;
    const std::shared_ptr<HalconLibrary> library =
            acquireHalconLibrary(config.halconSoPath, loadMessage, symbolMissing);
    if (!library) {
        return runError(symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                      : QStringLiteral("halcon_load_failed"),
                        loadMessage,
                        config,
                        image,
                        timer.elapsed());
    }

    HalconCApi *api = &library->api;

    try {
        const QString decisionMode = normalizedColorDecisionMode(config.colorDecisionMode);
        const bool useCurrentClassifier =
                classifierVersion == colorRecognitionHsvCurrentClassifierVersion();
        const ColorDecisionResult decision =
                decisionMode == QStringLiteral("histogram_intersection")
                ? decideByHistogramIntersection(api, config, usableSamples, comparisonFeature)
                : decisionMode == QStringLiteral("halcon_color_segmentation")
                  ? decideByHalconColorSegmentationReserved(api, config, usableSamples, comparisonFeature)
                : useCurrentClassifier
                  ? decideByClassTopNIntersection(api, config, usableSamples, comparisonFeature)
                  : decideByDominantRatio(api, config, usableSamples, comparisonFeature);

        bool ok = false;
        QString judgeMode = config.judgeMode.trimmed().toLower();
        const double minimumScore = static_cast<double>(qBound(0, config.minScore, 100));
        if (judgeMode == QStringLiteral("category")) {
            if (config.expectedLabel.trimmed().isEmpty()) {
                throw std::pair<QString, QString>(
                        QStringLiteral("invalid_expected_label"),
                        QStringLiteral("Expected label is empty for category judgement."));
            }
            // 类别正确但置信度不足仍应判 NG，避免低相似度样本被“硬分类”为 OK。
            ok = decision.predictedLabel == config.expectedLabel && decision.score >= minimumScore;
        } else {
            judgeMode = QStringLiteral("min_score");
            ok = decision.score >= minimumScore;
        }

        ColorRecognitionHalconResult result;
        result.success = true;
        result.ok = ok;
        result.status = ok ? QStringLiteral("ok") : QStringLiteral("ng");
        result.predictedLabel = decision.predictedLabel;
        result.predictedClassId = decision.predictedClassId;
        result.score = decision.score;
        result.rating = decision.rating;
        result.sampleCount = usableSamples.size();
        result.elapsedMs = timer.elapsed();
        result.message = QStringLiteral("label=%1, score=%2")
                .arg(decision.predictedLabel.isEmpty()
                     ? QStringLiteral("#%1").arg(decision.predictedClassId)
                     : decision.predictedLabel,
                     QString::number(decision.score, 'f', 2));

        const QRect roiPixels = normalizedRoiToPixels(config.roiNormalized,
                                                      image.empty() ? 0 : image.cols,
                                                      image.empty() ? 0 : image.rows);
        if (isCircleRegionType(config.detectRegionType) &&
            config.detectCircleRadiusNormalized > 0.0 &&
            !image.empty()) {
            const double radiusPixels = config.detectCircleRadiusNormalized *
                    static_cast<double>(qMax(image.cols, image.rows));
            result.overlays.append(circleOverlay(QPointF(config.detectCircleCenterNormalized.x() * image.cols,
                                                         config.detectCircleCenterNormalized.y() * image.rows),
                                                radiusPixels,
                                                QStringLiteral("ROI"),
                                                decision.score));
        } else {
            result.overlays.append(rectOverlay(QRectF(roiPixels), QStringLiteral("ROI"), decision.score));
        }
        ToolOverlay statusText = textOverlay(QPointF(roiPixels.x(), roiPixels.y()),
                                             QStringLiteral("%1 %2 %3")
                                             .arg(result.ok ? QStringLiteral("OK") : QStringLiteral("NG"),
                                                  decision.predictedLabel,
                                                  QString::number(decision.score, 'f', 1)),
                                             decision.score,
                                             QStringLiteral("color_result_text"));
        statusText.extra.insert(QStringLiteral("status"), result.ok ? QStringLiteral("OK") : QStringLiteral("NG"));
        statusText.extra.insert(QStringLiteral("anchorRect"), rectToJsonObject(QRectF(roiPixels)));
        result.overlays.append(statusText);

        result.payload.insert(QStringLiteral("algorithm"), QStringLiteral("halcon_hsv_histogram_transition"));
        result.payload.insert(QStringLiteral("algorithmVersion"), QString::fromLatin1(kAlgorithmVersion));
        result.payload.insert(QStringLiteral("featureType"), actualFeatureType(config));
        result.payload.insert(QStringLiteral("featureSchemaVersion"), featureSchemaVersion(config));
        result.payload.insert(QStringLiteral("featureSignature"), queryFeatureSignature);
        result.payload.insert(QStringLiteral("classifierVersion"), classifierVersion);
        result.payload.insert(QStringLiteral("classifierParamsHash"), expectedClassifierHash);
        result.payload.insert(QStringLiteral("requestedFeatureType"), config.featureType);
        result.payload.insert(QStringLiteral("colorDecisionMode"), decision.mode);
        result.payload.insert(QStringLiteral("predictedLabel"), decision.predictedLabel);
        result.payload.insert(QStringLiteral("predictedClassId"), decision.predictedClassId);
        result.payload.insert(QStringLiteral("matchedSampleIndex"), decision.matchedSampleIndex);
        result.payload.insert(QStringLiteral("matchedSampleLabel"), decision.matchedSampleLabel);
        result.payload.insert(QStringLiteral("score"), decision.score);
        result.payload.insert(QStringLiteral("rating"), decision.rating);
        result.payload.insert(QStringLiteral("similarity"), decision.rating);
        result.payload.insert(QStringLiteral("categorySimilarity"), decision.rating);
        result.payload.insert(QStringLiteral("classSimilarities"), ratiosToJson(decision.classSimilarities));
        result.payload.insert(QStringLiteral("scoreMeaning"),
                              useCurrentClassifier &&
                              decision.mode == QStringLiteral("category_similarity")
                              ? QStringLiteral("class_top_n_mean_histogram_intersection_similarity")
                              : QStringLiteral("best_class_histogram_intersection_similarity"));
        result.payload.insert(QStringLiteral("scoreDirection"), QStringLiteral("higher_is_better"));
        result.payload.insert(QStringLiteral("scoreFormula"),
                              decision.mode == QStringLiteral("category_similarity")
                              ? (useCurrentClassifier
                                 ? QStringLiteral("class_top_n_mean_histogram_intersection_similarity_x100")
                                 : QStringLiteral("best_class_histogram_intersection_similarity_x100"))
                              : QStringLiteral("histogram_intersection_similarity_x100"));
        result.payload.insert(QStringLiteral("ratingMode"),
                              decision.mode == QStringLiteral("category_similarity")
                              ? QStringLiteral("category_similarity")
                              : QStringLiteral("histogram_intersection_similarity"));
        result.payload.insert(QStringLiteral("comparisonMethod"), decision.comparisonMethod);
        result.payload.insert(QStringLiteral("secondClassScore"), decision.secondClassScore);
        result.payload.insert(QStringLiteral("classMargin"), decision.classMargin);
        result.payload.insert(QStringLiteral("classificationAmbiguous"),
                              decision.classificationAmbiguous);
        result.payload.insert(QStringLiteral("topNSampleCount"), decision.topNSampleCount);
        result.payload.insert(QStringLiteral("topNSimilarities"),
                              doubleArray(decision.topNSimilarities, 100.0));
        result.payload.insert(QStringLiteral("classDiagnostics"), decision.classDiagnostics);
        result.payload.insert(QStringLiteral("sampleCount"), result.sampleCount);
        result.payload.insert(QStringLiteral("featureLength"), featureResult.feature.size());
        result.payload.insert(QStringLiteral("comparisonFeatureLength"), comparisonFeature.size());
        result.payload.insert(QStringLiteral("featureAlignmentApplied"), false);
        result.payload.insert(QStringLiteral("queryFeature"), vectorToJson(comparisonFeature));
        result.payload.insert(QStringLiteral("roiPixelsRect"), rectToJson(QRectF(roiPixels)));
        result.payload.insert(QStringLiteral("detectRegionType"),
                              featureResult.payload.value(QStringLiteral("detectRegionType")).toString(QStringLiteral("rectangle")));
        result.payload.insert(QStringLiteral("circleDetectRoiApplied"),
                              featureResult.payload.value(QStringLiteral("circleDetectRoiApplied")).toBool(false));
        result.payload.insert(QStringLiteral("detectCircleCenterNormalized"),
                              pointToJsonObject(config.detectCircleCenterNormalized));
        result.payload.insert(QStringLiteral("detectCircleRadiusNormalized"),
                              config.detectCircleRadiusNormalized);
        result.payload.insert(QStringLiteral("detectCircleBoundingRectNormalized"),
                              rectToJson(config.detectCircleBoundingRectNormalized));
        result.payload.insert(QStringLiteral("detectMaskApplied"),
                              featureResult.payload.value(QStringLiteral("detectMaskApplied")).toBool(false));
        result.payload.insert(QStringLiteral("detectMaskFullyCoversRoi"), false);
        result.payload.insert(QStringLiteral("effectiveRoiArea"),
                              featureResult.payload.value(QStringLiteral("effectiveRoiArea")).toDouble());
        result.payload.insert(QStringLiteral("detectMaskPolygon"),
                              pointsToJson(config.detectMaskPolygonNormalized));
        result.payload.insert(QStringLiteral("judgeMode"), judgeMode);
        result.payload.insert(QStringLiteral("minScore"), qBound(0, config.minScore, 100));
        result.payload.insert(QStringLiteral("expectedLabel"), config.expectedLabel);
        result.payload.insert(QStringLiteral("histogramBins"), histogramBinsForSensitivity(config.sensitivity));
        result.payload.insert(QStringLiteral("brightnessEnabled"),
                              featureResult.payload.value(QStringLiteral("brightnessEnabled")).toBool(false));
        result.payload.insert(QStringLiteral("requestedBrightnessEnabled"), config.brightnessEnabled);
        result.payload.insert(QStringLiteral("lightingNormalizationMode"),
                              featureResult.payload.value(QStringLiteral("lightingNormalizationMode")).toString());
        result.payload.insert(QStringLiteral("enablePositionCorrection"), config.enablePositionCorrection);
        result.payload.insert(QStringLiteral("positionCorrectionSource"), config.positionCorrectionSource);
        result.payload.insert(QStringLiteral("positionCorrectionApplied"), false);
        result.payload.insert(QStringLiteral("positionCorrectionReason"), QStringLiteral("not implemented"));
        result.payload.insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));

        return result;
    } catch (const std::pair<QString, QString> &error) {
        return runError(error.first, error.second, config, image, timer.elapsed());
    } catch (const std::exception &error) {
        return runError(QStringLiteral("exception"),
                        QString::fromLocal8Bit(error.what()),
                        config,
                        image,
                        timer.elapsed());
    } catch (...) {
        return runError(QStringLiteral("exception"),
                        QStringLiteral("Unknown exception while classifying color feature."),
                        config,
                        image,
                        timer.elapsed());
    }
}
