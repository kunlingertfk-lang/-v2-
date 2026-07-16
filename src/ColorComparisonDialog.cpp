#include "ColorComparisonDialog.h"

#include "ColorComparisonFeatureView.h"
#include "PlanDialogUtils.h"
#include "frame/CameraFrameProvider.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolRequest.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QFutureWatcher>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QToolButton>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>
#include <limits>

namespace {

constexpr int kActionButtonFlashMs = 120;

bool finiteValue(qreal value)
{
    return std::isfinite(static_cast<double>(value));
}

bool strictJsonInteger(const QJsonValue &value, int *parsed)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
            || number < std::numeric_limits<int>::min()
            || number > std::numeric_limits<int>::max()) {
        return false;
    }
    if (parsed)
        *parsed = static_cast<int>(number);
    return true;
}

struct V2DialogConfigValidation
{
    bool success = true;
    QString status;
    QString message;
};

V2DialogConfigValidation invalidV2DialogConfig(const QString &status,
                                                const QString &message)
{
    V2DialogConfigValidation validation;
    validation.success = false;
    validation.status = status;
    validation.message = message;
    return validation;
}

bool strictJsonNumber(const QJsonValue &value, double *parsed)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number))
        return false;
    if (parsed)
        *parsed = number;
    return true;
}

bool normalizedHistogramFromJson(const QJsonValue &value,
                                 int expectedSize,
                                 QVector<double> *histogram)
{
    if (!value.isArray() || !histogram)
        return false;
    const QJsonArray array = value.toArray();
    if (array.size() != expectedSize)
        return false;
    QVector<double> parsed;
    parsed.reserve(expectedSize);
    for (const QJsonValue &entry : array) {
        if (!entry.isDouble() || !std::isfinite(entry.toDouble())
                || entry.toDouble() < 0.0) {
            return false;
        }
        parsed.append(entry.toDouble());
    }
    if (!ColorComparisonFeatureView::validNormalizedHistogram(parsed,
                                                               expectedSize)) {
        return false;
    }
    *histogram = parsed;
    return true;
}

bool strictNormalizedRect(const QJsonValue &value,
                          QRectF *parsed,
                          bool allowEmpty = false)
{
    if (!value.isObject())
        return false;

    const QJsonObject object = value.toObject();
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    if (!strictJsonNumber(object.value(QStringLiteral("x")), &x)
            || !strictJsonNumber(object.value(QStringLiteral("y")), &y)
            || !strictJsonNumber(object.value(QStringLiteral("width")), &width)
            || !strictJsonNumber(object.value(QStringLiteral("height")), &height)
            || x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) {
        return false;
    }

    const bool empty = width == 0.0 && height == 0.0;
    if (empty && allowEmpty) {
        if (parsed)
            *parsed = QRectF(x, y, width, height);
        return true;
    }
    if (width <= 0.0 || height <= 0.0
            || x + width > 1.0 || y + height > 1.0) {
        return false;
    }

    if (parsed)
        *parsed = QRectF(x, y, width, height);
    return true;
}

bool strictNormalizedPoint(const QJsonValue &value, QPointF *parsed)
{
    if (!value.isObject())
        return false;
    const QJsonObject object = value.toObject();
    double x = 0.0;
    double y = 0.0;
    if (!strictJsonNumber(object.value(QStringLiteral("x")), &x)
            || !strictJsonNumber(object.value(QStringLiteral("y")), &y)
            || x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0) {
        return false;
    }
    if (parsed)
        *parsed = QPointF(x, y);
    return true;
}

bool strictNormalizedPolygon(const QJsonValue &value)
{
    if (!value.isArray())
        return false;

    const QJsonArray array = value.toArray();
    if (!array.isEmpty() && array.size() < 3)
        return false;

    QVector<QPointF> points;
    points.reserve(array.size());
    for (const QJsonValue &entry : array) {
        QPointF point;
        if (!strictNormalizedPoint(entry, &point))
            return false;
        points.append(point);
    }
    if (points.isEmpty())
        return true;

    double twiceArea = 0.0;
    for (int index = 0; index < points.size(); ++index) {
        const QPointF &point = points.at(index);
        const QPointF &next = points.at((index + 1) % points.size());
        twiceArea += point.x() * next.y() - next.x() * point.y();
    }
    return std::abs(twiceArea) > 1e-12;
}

bool sameRect(const QRectF &left, const QRectF &right)
{
    constexpr double epsilon = 1e-9;
    return std::abs(left.x() - right.x()) <= epsilon
            && std::abs(left.y() - right.y()) <= epsilon
            && std::abs(left.width() - right.width()) <= epsilon
            && std::abs(left.height() - right.height()) <= epsilon;
}

V2DialogConfigValidation validateV2DialogConfig(const ToolConfig &config)
{
    const QJsonObject colorComparison = config.params
            .value(QStringLiteral("colorComparison")).toObject();

    QString templateRegionMode = QStringLiteral("custom");
    if (colorComparison.contains(QStringLiteral("templateRegionMode"))) {
        const QJsonValue value = colorComparison
                .value(QStringLiteral("templateRegionMode"));
        if (!value.isString()) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_template_roi"),
                        QStringLiteral("templateRegionMode must be a string."));
        }
        templateRegionMode = value.toString().trimmed().toLower();
    }
    if (templateRegionMode != QStringLiteral("custom")
            && templateRegionMode != QStringLiteral("sync")) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_template_roi"),
                    QStringLiteral("templateRegionMode must be custom or sync."));
    }

    if (colorComparison.contains(QStringLiteral("templateRoiNormalized"))
            && !strictNormalizedRect(
                colorComparison.value(QStringLiteral("templateRoiNormalized")),
                nullptr)) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_template_roi"),
                    QStringLiteral("Template ROI is malformed or out of range."));
    }
    if (colorComparison.contains(QStringLiteral("templateMaskPolygon"))
            && !strictNormalizedPolygon(
                colorComparison.value(QStringLiteral("templateMaskPolygon")))) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_template_mask"),
                    QStringLiteral("Template mask is malformed or degenerate."));
    }

    QString detectRegionType = QStringLiteral("rectangle");
    if (colorComparison.contains(QStringLiteral("detectRegionType"))) {
        const QJsonValue value = colorComparison
                .value(QStringLiteral("detectRegionType"));
        if (!value.isString()) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_detect_roi"),
                        QStringLiteral("detectRegionType must be a string."));
        }
        detectRegionType = value.toString().trimmed().toLower();
    }
    if (detectRegionType != QStringLiteral("rectangle")
            && detectRegionType != QStringLiteral("circle")) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_detect_roi"),
                    QStringLiteral("detectRegionType must be rectangle or circle."));
    }

    QRectF detectRoi;
    const bool hasDetectRoi = colorComparison.contains(
                QStringLiteral("detectRoiNormalized"));
    if (hasDetectRoi
            && !strictNormalizedRect(
                colorComparison.value(QStringLiteral("detectRoiNormalized")),
                &detectRoi)) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_detect_roi"),
                    QStringLiteral("Detection ROI is malformed or out of range."));
    }

    bool detectGlobal = false;
    if (colorComparison.contains(QStringLiteral("detectGlobal"))) {
        const QJsonValue value = colorComparison
                .value(QStringLiteral("detectGlobal"));
        if (!value.isBool()) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_detect_roi"),
                        QStringLiteral("detectGlobal must be boolean."));
        }
        detectGlobal = value.toBool();
    }
    if (detectGlobal
            && (detectRegionType != QStringLiteral("rectangle")
                || (hasDetectRoi
                    && !sameRect(detectRoi, QRectF(0.0, 0.0, 1.0, 1.0))))) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_detect_roi"),
                    QStringLiteral("Global detection requires rectangle/full-image ROI."));
    }

    const bool hasCircle = colorComparison.contains(
                QStringLiteral("detectCircleNormalized"));
    if (hasCircle) {
        const QJsonValue circleValue = colorComparison
                .value(QStringLiteral("detectCircleNormalized"));
        if (!circleValue.isObject()) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_detect_roi"),
                        QStringLiteral("Detection circle must be an object."));
        }

        const QJsonObject circle = circleValue.toObject();
        QPointF center;
        double radius = 0.0;
        if (!strictNormalizedPoint(circle.value(QStringLiteral("center")),
                                   &center)
                || !strictJsonNumber(circle.value(QStringLiteral("radius")),
                                     &radius)
                || radius < 0.0) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_detect_roi"),
                        QStringLiteral("Detection circle is malformed."));
        }

        bool valid = radius > 0.0;
        if (circle.contains(QStringLiteral("valid"))) {
            if (!circle.value(QStringLiteral("valid")).isBool()) {
                return invalidV2DialogConfig(
                            QStringLiteral("invalid_detect_roi"),
                            QStringLiteral("Detection circle valid flag must be boolean."));
            }
            valid = circle.value(QStringLiteral("valid")).toBool();
        }
        if (valid != (radius > 0.0)) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_detect_roi"),
                        QStringLiteral("Detection circle valid flag and radius disagree."));
        }

        if (circle.contains(QStringLiteral("boundingRect"))) {
            QRectF boundingRect;
            if (!strictNormalizedRect(
                    circle.value(QStringLiteral("boundingRect")),
                    &boundingRect,
                    radius == 0.0)) {
                return invalidV2DialogConfig(
                            QStringLiteral("invalid_detect_roi"),
                            QStringLiteral("Detection circle bounding rectangle is malformed."));
            }
            constexpr double epsilon = 1e-9;
            const bool centerMatches =
                    std::abs(boundingRect.center().x() - center.x()) <= epsilon
                    && std::abs(boundingRect.center().y() - center.y()) <= epsilon;
            const bool radiusMatches = radius == 0.0
                    ? boundingRect.width() == 0.0
                      && boundingRect.height() == 0.0
                    : std::abs(qMin(boundingRect.width(),
                                    boundingRect.height()) / 2.0 - radius)
                      <= epsilon;
            if (!centerMatches || !radiusMatches) {
                return invalidV2DialogConfig(
                            QStringLiteral("invalid_detect_roi"),
                            QStringLiteral("Detection circle geometry is inconsistent."));
            }
        }
        if (detectRegionType == QStringLiteral("circle")
                && (!valid || radius <= 0.0)) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_detect_roi"),
                        QStringLiteral("Active detection circle is invalid."));
        }
    } else if (detectRegionType == QStringLiteral("circle")) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_detect_roi"),
                    QStringLiteral("Active detection circle is missing."));
    }

    if (colorComparison.contains(QStringLiteral("detectMaskPolygon"))
            && !strictNormalizedPolygon(
                colorComparison.value(QStringLiteral("detectMaskPolygon")))) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_detect_mask"),
                    QStringLiteral("Detection mask is malformed or degenerate."));
    }

    const QJsonValue comparisonValue = colorComparison
            .value(QStringLiteral("comparison"));
    if (!comparisonValue.isUndefined()) {
        if (!comparisonValue.isObject()) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_sensitivity"),
                        QStringLiteral("comparison must be an object."));
        }
        const QJsonObject comparison = comparisonValue.toObject();
        if (comparison.contains(QStringLiteral("sensitivity"))) {
            const QJsonValue value = comparison
                    .value(QStringLiteral("sensitivity"));
            if (!value.isString()) {
                return invalidV2DialogConfig(
                            QStringLiteral("invalid_sensitivity"),
                            QStringLiteral("sensitivity must be a string."));
            }
            const QString sensitivity = value.toString().trimmed().toLower();
            if (sensitivity != QStringLiteral("high")
                    && sensitivity != QStringLiteral("medium")
                    && sensitivity != QStringLiteral("low")) {
                return invalidV2DialogConfig(
                            QStringLiteral("invalid_sensitivity"),
                            QStringLiteral("sensitivity must be high, medium, or low."));
            }
        }
        if (comparison.contains(QStringLiteral("brightnessCompensation"))
                && !comparison.value(
                    QStringLiteral("brightnessCompensation")).isBool()) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_illumination"),
                        QStringLiteral("brightnessCompensation must be boolean."));
        }
    }

    if (colorComparison.contains(QStringLiteral("sensitivity"))) {
        const QJsonValue value = colorComparison.value(QStringLiteral("sensitivity"));
        if (!value.isString()) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_sensitivity"),
                        QStringLiteral("Legacy sensitivity must be a string."));
        }
        const QString sensitivity = value.toString().trimmed().toLower();
        if (sensitivity != QStringLiteral("high")
                && sensitivity != QStringLiteral("medium")
                && sensitivity != QStringLiteral("low")) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_sensitivity"),
                        QStringLiteral("Legacy sensitivity is unsupported."));
        }
    }
    if (colorComparison.contains(QStringLiteral("brightnessEnabled"))
            && !colorComparison.value(
                QStringLiteral("brightnessEnabled")).isBool()) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_illumination"),
                    QStringLiteral("brightnessEnabled must be boolean."));
    }

    const QJsonValue positionValue = colorComparison
            .value(QStringLiteral("positionCorrection"));
    if (!positionValue.isUndefined()) {
        if (!positionValue.isObject()) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_position_correction"),
                        QStringLiteral("positionCorrection must be an object."));
        }
        const QJsonObject position = positionValue.toObject();
        if (position.contains(QStringLiteral("enabled"))
                && !position.value(QStringLiteral("enabled")).isBool()) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_position_correction"),
                        QStringLiteral("positionCorrection.enabled must be boolean."));
        }
        if (position.contains(QStringLiteral("sourceId"))
                && !position.value(QStringLiteral("sourceId")).isString()) {
            return invalidV2DialogConfig(
                        QStringLiteral("invalid_position_correction"),
                        QStringLiteral("positionCorrection.sourceId must be a string."));
        }
        if (position.contains(QStringLiteral("interfaceVersion"))) {
            int interfaceVersion = 0;
            if (!strictJsonInteger(
                    position.value(QStringLiteral("interfaceVersion")),
                    &interfaceVersion)
                    || interfaceVersion != 1) {
                return invalidV2DialogConfig(
                            QStringLiteral("invalid_position_correction"),
                            QStringLiteral("Only position-correction interface version 1 is reserved."));
            }
        }
    }
    if (colorComparison.contains(QStringLiteral("enablePositionCorrection"))
            && !colorComparison.value(
                QStringLiteral("enablePositionCorrection")).isBool()) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_position_correction"),
                    QStringLiteral("enablePositionCorrection must be boolean."));
    }
    if (colorComparison.contains(QStringLiteral("positionCorrectionSource"))
            && !colorComparison.value(
                QStringLiteral("positionCorrectionSource")).isString()) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_position_correction"),
                    QStringLiteral("positionCorrectionSource must be a string."));
    }

    const QJsonObject judgeRule = config.judgeRule;
    if (!judgeRule.value(QStringLiteral("mode")).isString()
            || judgeRule.value(QStringLiteral("mode"))
               .toString().trimmed().toLower() != QStringLiteral("min_score")) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_judge_rule"),
                    QStringLiteral("judgeRule.mode must be min_score."));
    }
    int minScore = 0;
    if (!strictJsonInteger(judgeRule.value(QStringLiteral("minScore")),
                           &minScore)
            || minScore < 0 || minScore > 100) {
        return invalidV2DialogConfig(
                    QStringLiteral("invalid_judge_rule"),
                    QStringLiteral("judgeRule.minScore must be an integer in [0,100]."));
    }

    const QJsonValue halconPath = colorComparison
            .value(QStringLiteral("halconSoPath"));
    if (!halconPath.isUndefined() && !halconPath.isString()) {
        return invalidV2DialogConfig(
                    QStringLiteral("halcon_load_failed"),
                    QStringLiteral("halconSoPath must be a string."));
    }

    const ColorComparisonModelReadResult modelRead =
            readColorComparisonModel(colorComparison, false);
    if (modelRead.status == QStringLiteral("model_invalid")) {
        return invalidV2DialogConfig(
                    QStringLiteral("model_invalid"),
                    modelRead.message.isEmpty()
                    ? QStringLiteral("Color comparison model is invalid.")
                    : modelRead.message);
    }

    return V2DialogConfigValidation();
}

bool restoreDialogLifecycle(const QJsonObject &colorComparison,
                            ColorComparisonModelState modelState,
                            int *originVersion,
                            QString *status,
                            QString *reason)
{
    const QJsonValue lifecycleValue =
            colorComparison.value(QStringLiteral("dialogLifecycle"));
    if (!lifecycleValue.isObject())
        return false;

    const QJsonObject lifecycle = lifecycleValue.toObject();
    int origin = 0;
    if (!strictJsonInteger(lifecycle.value(QStringLiteral("originVersion")),
                           &origin)
            || !lifecycle.value(QStringLiteral("status")).isString()
            || !lifecycle.value(QStringLiteral("reason")).isString()) {
        return false;
    }

    const QString storedStatus =
            lifecycle.value(QStringLiteral("status")).toString();
    const QString storedReason =
            lifecycle.value(QStringLiteral("reason")).toString();
    if (storedReason.size() > 1024)
        return false;

    const bool legacyRebuild = origin == 1
            && modelState == ColorComparisonModelState::Unsupported
            && storedStatus == QStringLiteral("model_rebuild_required");
    const bool legacyStale = origin == 1
            && modelState == ColorComparisonModelState::Stale
            && storedStatus == QStringLiteral("model_stale");
    const bool unknownUnsupported = origin != 1 && origin != 2
            && modelState == ColorComparisonModelState::Unsupported
            && storedStatus == QStringLiteral("unsupported_model_version");
    if (!legacyRebuild && !legacyStale && !unknownUnsupported) {
        return false;
    }

    if (originVersion)
        *originVersion = origin;
    if (status)
        *status = storedStatus;
    if (reason)
        *reason = storedReason;
    return true;
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

QRectF rectFromJson(const QJsonObject &json, const QRectF &fallback)
{
    if (json.isEmpty())
        return fallback;
    return QRectF(json.value(QStringLiteral("x")).toDouble(fallback.x()),
                  json.value(QStringLiteral("y")).toDouble(fallback.y()),
                  json.value(QStringLiteral("width")).toDouble(fallback.width()),
                  json.value(QStringLiteral("height")).toDouble(fallback.height()));
}

QJsonObject pointToJson(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
}

QPointF pointFromJson(const QJsonObject &json)
{
    return QPointF(json.value(QStringLiteral("x")).toDouble(),
                   json.value(QStringLiteral("y")).toDouble());
}

QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points)
        array.append(pointToJson(point));
    return array;
}

QVector<QPointF> pointsFromJson(const QJsonArray &array)
{
    QVector<QPointF> points;
    points.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QPointF point = pointFromJson(value.toObject());
        if (finiteValue(point.x()) && finiteValue(point.y())) {
            points.append(QPointF(qBound(0.0, point.x(), 1.0),
                                  qBound(0.0, point.y(), 1.0)));
        }
    }
    return points.size() >= 3 ? points : QVector<QPointF>();
}

QJsonObject circleToJson(const CircleRoi &circle)
{
    QJsonObject json;
    json.insert(QStringLiteral("center"), pointToJson(circle.centerNormalized));
    json.insert(QStringLiteral("radius"), circle.radiusNormalized);
    json.insert(QStringLiteral("boundingRect"), rectToJson(circle.boundingRectNormalized));
    json.insert(QStringLiteral("valid"), circle.valid);
    return json;
}

CircleRoi circleFromJson(const QJsonObject &json)
{
    CircleRoi circle;
    circle.centerNormalized = pointFromJson(json.value(QStringLiteral("center")).toObject());
    circle.radiusNormalized = json.value(QStringLiteral("radius")).toDouble();
    circle.boundingRectNormalized =
            rectFromJson(json.value(QStringLiteral("boundingRect")).toObject(),
                         QRectF(circle.centerNormalized.x() - circle.radiusNormalized,
                                circle.centerNormalized.y() - circle.radiusNormalized,
                                circle.radiusNormalized * 2.0,
                                circle.radiusNormalized * 2.0));
    circle.valid = json.value(QStringLiteral("valid")).toBool(circle.radiusNormalized > 0.0);
    return circle;
}

QRectF circleBoundingRectForImage(const CircleRoi &circle,
                                  int imageWidth,
                                  int imageHeight)
{
    if (!circle.valid || imageWidth <= 0 || imageHeight <= 0
            || !finiteValue(circle.centerNormalized.x())
            || !finiteValue(circle.centerNormalized.y())
            || !finiteValue(circle.radiusNormalized)
            || circle.radiusNormalized <= 0.0) {
        return QRectF();
    }

    const double maxDimension = static_cast<double>(
                qMax(imageWidth, imageHeight));
    const double radiusPixels = circle.radiusNormalized * maxDimension;
    const double xRadius = radiusPixels / static_cast<double>(imageWidth);
    const double yRadius = radiusPixels / static_cast<double>(imageHeight);
    return QRectF(circle.centerNormalized.x() - xRadius,
                  circle.centerNormalized.y() - yRadius,
                  xRadius * 2.0,
                  yRadius * 2.0);
}

bool circleBoundingRectFitsImage(const QRectF &rect)
{
    constexpr double epsilon = 1e-9;
    return rect.isValid()
            && rect.left() >= -epsilon
            && rect.top() >= -epsilon
            && rect.right() <= 1.0 + epsilon
            && rect.bottom() <= 1.0 + epsilon;
}

QImage imageFromFrame(const cv::Mat &frame)
{
    return MatImageConverter::matToDisplayImage(frame, QStringLiteral("ColorComparisonDialog"));
}

QFrame *card(QWidget *parent, const QString &title)
{
    QFrame *frame = new QFrame(parent);
    frame->setFrameShape(QFrame::NoFrame);
    frame->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);
    QLabel *titleLabel = new QLabel(title, frame);
    titleLabel->setProperty("role", QStringLiteral("cardTitle"));
    layout->addWidget(titleLabel);
    return frame;
}

QHBoxLayout *row(const QString &labelText, QWidget *field)
{
    QHBoxLayout *layout = new QHBoxLayout;
    QLabel *label = new QLabel(labelText);
    label->setMinimumWidth(118);
    label->setProperty("role", QStringLiteral("rowField"));
    layout->addWidget(label);
    layout->addStretch(1);
    if (field)
        layout->addWidget(field);
    return layout;
}

void applyBottomActionButtonMetrics(QPushButton *button)
{
    if (!button)
        return;

    button->setMinimumSize(0, 48);
    button->setMaximumSize(QWIDGETSIZE_MAX, 48);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setAutoDefault(false);
    button->setDefault(false);
}

void refreshButtonStyle(QWidget *button)
{
    if (!button)
        return;

    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
}

void installActionButtonFlash(QPushButton *button)
{
    if (!button)
        return;

    QObject::connect(button, &QPushButton::clicked, button, [button]() {
        button->setProperty("flash", true);
        refreshButtonStyle(button);
        QTimer::singleShot(kActionButtonFlashMs, button, [button]() {
            button->setProperty("flash", false);
            refreshButtonStyle(button);
        });
    });
}

} // namespace

ColorComparisonDialog::ColorComparisonDialog(QWidget *parent)
    : QDialog(parent)
    , m_toolId(QStringLiteral("color_comparison_%1")
               .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    buildUi();

    m_testWatcher = new QFutureWatcher<ToolResult>(this);
    m_modelBuildWatcher =
            new QFutureWatcher<ColorComparisonTemplateBuildResult>(this);
    connectAsyncWorkers();

    m_continuousTimer = new QTimer(this);
    m_continuousTimer->setInterval(500);
    connect(m_continuousTimer, &QTimer::timeout, this, &ColorComparisonDialog::runContinuousTick);
    connectControls();
    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) {
                markModelStale(QStringLiteral("reference_changed"));
                if (m_testUiMode == TestUiMode::Edit)
                    showPreviewImage();
            });
    setAllParamsMode(false);
    showPreviewImage();
    refreshRoiOverlay();
    updateTemplatePreview();
    updateModelStateUi();
    updateBottomButtons();
}

ColorComparisonDialog::~ColorComparisonDialog()
{
    if (m_continuousTimer)
        m_continuousTimer->stop();
    invalidateAsyncWork();
    invalidateModelBuild();
    if (m_testWatcher)
        disconnect(m_testWatcher, nullptr, this, nullptr);
    if (m_modelBuildWatcher)
        disconnect(m_modelBuildWatcher, nullptr, this, nullptr);
}

void ColorComparisonDialog::connectAsyncWorkers()
{
    connect(m_testWatcher,
            &QFutureWatcher<ToolResult>::finished,
            this,
            &ColorComparisonDialog::handleTestFinished);
    connect(m_modelBuildWatcher,
            &QFutureWatcher<ColorComparisonTemplateBuildResult>::finished,
            this,
            &ColorComparisonDialog::handleModelBuildFinished);
}

void ColorComparisonDialog::buildUi()
{
    setObjectName(QStringLiteral("ColorComparisonDialog"));
    setWindowTitle(tr("方案编辑 - 颜色比较"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(this);

    m_segmentGroup = new QButtonGroup(this);
    m_detectRegionGroup = new QButtonGroup(this);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *header = new QFrame(this);
    header->setObjectName(QStringLiteral("colorComparisonHeader"));
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(28, 0, 22, 0);
    QLabel *headerTitle = new QLabel(tr("方案编辑"), header);
    headerTitle->setObjectName(QStringLiteral("colorComparisonHeaderTitle"));
    QToolButton *closeButton = new QToolButton(header);
    closeButton->setObjectName(QStringLiteral("colorComparisonHeaderClose"));
    closeButton->setText(QStringLiteral("×"));
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);
    root->addWidget(header, 0);

    QHBoxLayout *content = new QHBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);
    root->addLayout(content, 1);

    QFrame *leftPanel = new QFrame(this);
    leftPanel->setObjectName(QStringLiteral("colorComparisonLeftPanel"));
    leftPanel->setMinimumWidth(420);
    leftPanel->setMaximumWidth(480);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(24, 18, 24, 18);
    leftLayout->setSpacing(14);
    content->addWidget(leftPanel, 0);

    QHBoxLayout *titleLayout = new QHBoxLayout;
    QLabel *dialogTitle = new QLabel(tr("颜色比较"), leftPanel);
    dialogTitle->setProperty("role", QStringLiteral("cardTitle"));
    m_basicButton = new QPushButton(tr("基础"), leftPanel);
    m_allButton = new QPushButton(tr("全部"), leftPanel);
    m_basicButton->setObjectName(QStringLiteral("colorComparisonBasicButton"));
    m_allButton->setObjectName(QStringLiteral("colorComparisonAllButton"));
    m_basicButton->setCheckable(true);
    m_allButton->setCheckable(true);
    m_segmentGroup->addButton(m_basicButton, 0);
    m_segmentGroup->addButton(m_allButton, 1);
    titleLayout->addWidget(dialogTitle);
    titleLayout->addStretch(1);
    titleLayout->addWidget(m_basicButton);
    titleLayout->addWidget(m_allButton);
    leftLayout->addLayout(titleLayout);

    QScrollArea *paramsScroll = new QScrollArea(leftPanel);
    paramsScroll->setObjectName(QStringLiteral("colorComparisonParamsScrollArea"));
    paramsScroll->setWidgetResizable(true);
    paramsScroll->setFrameShape(QFrame::NoFrame);
    paramsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    paramsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_paramsStack = new QStackedWidget(paramsScroll);
    m_paramsStack->setObjectName(QStringLiteral("colorComparisonParamsStack"));
    paramsScroll->setWidget(m_paramsStack);
    leftLayout->addWidget(paramsScroll, 1);

    auto buildPage = [this](bool allMode) {
        QWidget *page = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setSizeConstraint(QLayout::SetMinimumSize);
        layout->setContentsMargins(0, 8, 0, 0);
        layout->setSpacing(14);

        QFrame *templateCard = card(page, tr("模板区域"));
        QVBoxLayout *templateLayout = qobject_cast<QVBoxLayout *>(templateCard->layout());
        if (!m_templateRegionModeComboBox) {
            m_templateRegionModeComboBox = new QComboBox(this);
            m_templateRegionModeComboBox->setObjectName(
                        QStringLiteral("colorComparisonTemplateRegionModeCombo"));
            m_templateRegionModeComboBox->addItem(tr("与检测区域同步"),
                                                   QStringLiteral("sync"));
            m_templateRegionModeComboBox->addItem(tr("自定义"),
                                                   QStringLiteral("custom"));
            m_templateRegionModeComboBox->setCurrentIndex(1);
        }
        templateLayout->addLayout(row(tr("模板区域"),
                                      m_templateRegionModeComboBox));
        QWidget *templateEditRow = new QWidget(templateCard);
        QHBoxLayout *templateEditLayout = new QHBoxLayout(templateEditRow);
        templateEditLayout->setContentsMargins(0, 0, 0, 0);
        templateEditLayout->addWidget(new QLabel(tr("模板编辑"), templateEditRow));
        templateEditLayout->addStretch(1);
        if (!m_templateEditButton)
            m_templateEditButton = new QPushButton(tr("编辑"), this);
        m_templateEditButton->setObjectName(
                    QStringLiteral("colorComparisonTemplateEditButton"));
        if (!m_templateRectButton) {
            m_templateRectButton = new QToolButton(this);
            m_templateRectButton->setText(QStringLiteral("□"));
            m_templateRectButton->setCheckable(true);
        }
        m_templateRectButton->setObjectName(
                    QStringLiteral("colorComparisonTemplateRectButton"));
        if (!m_templateFinishButton)
            m_templateFinishButton = new QPushButton(tr("完成"), this);
        m_templateFinishButton->setObjectName(
                    QStringLiteral("colorComparisonTemplateFinishButton"));
        templateEditLayout->addWidget(m_templateEditButton);
        templateEditLayout->addWidget(m_templateRectButton);
        templateEditLayout->addWidget(m_templateFinishButton);
        templateLayout->addWidget(templateEditRow);

        QWidget *maskRow = new QWidget(templateCard);
        QHBoxLayout *maskLayout = new QHBoxLayout(maskRow);
        maskLayout->setContentsMargins(0, 0, 0, 0);
        maskLayout->addWidget(new QLabel(tr("屏蔽区域"), maskRow));
        maskLayout->addStretch(1);
        if (!m_templateMaskEditButton)
            m_templateMaskEditButton = new QPushButton(tr("编辑"), this);
        m_templateMaskEditButton->setObjectName(
                    QStringLiteral("colorComparisonTemplateMaskEditButton"));
        if (!m_templateMaskPolygonButton) {
            m_templateMaskPolygonButton = new QToolButton(this);
            m_templateMaskPolygonButton->setText(QStringLiteral("⬡"));
            m_templateMaskPolygonButton->setCheckable(true);
        }
        m_templateMaskPolygonButton->setObjectName(
                    QStringLiteral("colorComparisonTemplateMaskPolygonButton"));
        if (!m_templateMaskFinishButton)
            m_templateMaskFinishButton = new QPushButton(tr("完成"), this);
        m_templateMaskFinishButton->setObjectName(
                    QStringLiteral("colorComparisonTemplateMaskFinishButton"));
        maskLayout->addWidget(m_templateMaskEditButton);
        maskLayout->addWidget(m_templateMaskPolygonButton);
        maskLayout->addWidget(m_templateMaskFinishButton);
        templateLayout->addWidget(maskRow);
        layout->addWidget(templateCard);

        QFrame *effectCard = card(page, tr("模板效果"));
        QVBoxLayout *effectLayout = qobject_cast<QVBoxLayout *>(effectCard->layout());
        if (!m_templatePreviewLabel) {
            m_templatePreviewLabel = new QLabel(effectCard);
            m_templatePreviewLabel->setObjectName(
                        QStringLiteral("colorComparisonTemplatePreview"));
            m_templatePreviewLabel->setFixedSize(132, 86);
            m_templatePreviewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_templatePreviewLabel->setAlignment(Qt::AlignCenter);
            m_templatePreviewLabel->setStyleSheet(QStringLiteral("background:#ffffff; border:1px solid #d1d5db;"));
        }
        effectLayout->addWidget(m_templatePreviewLabel, 0, Qt::AlignHCenter);
        if (!m_rebuildModelButton) {
            m_rebuildModelButton = new QPushButton(tr("重新取样"), this);
            m_rebuildModelButton->setObjectName(
                        QStringLiteral("colorComparisonRebuildModelButton"));
        }
        if (!m_modelStateLabel) {
            m_modelStateLabel = new QLabel(effectCard);
            m_modelStateLabel->setObjectName(
                        QStringLiteral("colorComparisonModelStateLabel"));
            m_modelStateLabel->setWordWrap(true);
        }
        effectLayout->addWidget(m_rebuildModelButton, 0, Qt::AlignHCenter);
        effectLayout->addWidget(m_modelStateLabel);
        layout->addWidget(effectCard);

        if (allMode) {
            QFrame *featureCard = card(page, tr("模型色彩特征"));
            m_featureCard = featureCard;
            QVBoxLayout *featureLayout = qobject_cast<QVBoxLayout *>(featureCard->layout());
            if (!m_featureTypeComboBox) {
                m_featureTypeComboBox = new QComboBox(this);
                m_featureTypeComboBox->setObjectName(
                            QStringLiteral("colorComparisonFeatureTypeCombo"));
                m_featureTypeComboBox->addItem(tr("直方图特征"),
                                                QStringLiteral("histogram_hs_2d"));
                m_featureTypeComboBox->addItem(tr("色谱特征（待实现）"),
                                                QStringLiteral("spectrum"));
                if (QStandardItemModel *model =
                        qobject_cast<QStandardItemModel *>(m_featureTypeComboBox->model())) {
                    if (QStandardItem *item = model->item(1))
                        item->setEnabled(false);
                }
            }
            if (!m_brightnessCheckBox) {
                m_brightnessCheckBox = new QCheckBox(
                            tr("光照补偿（默认关闭）"), this);
                m_brightnessCheckBox->setObjectName(
                            QStringLiteral("colorComparisonBrightnessCompensation"));
                m_brightnessCheckBox->setChecked(false);
            }
            featureLayout->addLayout(row(tr("特征类型"), m_featureTypeComboBox));
            featureLayout->addWidget(m_brightnessCheckBox);

            if (!m_featureView)
                m_featureView = new ColorComparisonFeatureView(featureCard);
            featureLayout->addWidget(m_featureView);
            layout->addWidget(featureCard);
        }

        QFrame *detectCard = card(page, tr("检测区域"));
        QVBoxLayout *detectLayout = qobject_cast<QVBoxLayout *>(detectCard->layout());
        QWidget *detectButtons = new QWidget(detectCard);
        QHBoxLayout *detectButtonsLayout = new QHBoxLayout(detectButtons);
        detectButtonsLayout->setContentsMargins(0, 0, 0, 0);
        if (!m_detectGlobalButton) {
            m_detectGlobalButton = new QToolButton(this);
            m_detectGlobalButton->setText(QStringLiteral("▣"));
            m_detectGlobalButton->setCheckable(true);
            m_detectGlobalButton->setObjectName(
                        QStringLiteral("colorComparisonDetectGlobalButton"));
            m_detectRectButton = new QToolButton(this);
            m_detectRectButton->setText(QStringLiteral("□"));
            m_detectRectButton->setCheckable(true);
            m_detectRectButton->setObjectName(
                        QStringLiteral("colorComparisonDetectRectButton"));
            m_detectCircleButton = new QToolButton(this);
            m_detectCircleButton->setText(QStringLiteral("○"));
            m_detectCircleButton->setCheckable(true);
            m_detectCircleButton->setObjectName(
                        QStringLiteral("colorComparisonDetectCircleButton"));
            m_detectRegionGroup->addButton(m_detectGlobalButton, 0);
            m_detectRegionGroup->addButton(m_detectRectButton, 1);
            m_detectRegionGroup->addButton(m_detectCircleButton, 2);
        }
        detectButtonsLayout->addWidget(new QLabel(tr("检测区"), detectButtons));
        detectButtonsLayout->addStretch(1);
        detectButtonsLayout->addWidget(m_detectGlobalButton);
        detectButtonsLayout->addWidget(m_detectRectButton);
        detectButtonsLayout->addWidget(m_detectCircleButton);
        detectLayout->addWidget(detectButtons);

        m_positionCorrectionPanel = new QWidget(detectCard);
        m_positionCorrectionPanel->setObjectName(
                    QStringLiteral("colorComparisonPositionCorrectionPanel"));
        QVBoxLayout *positionPanelLayout =
                new QVBoxLayout(m_positionCorrectionPanel);
        positionPanelLayout->setContentsMargins(0, 0, 0, 0);
        QWidget *positionEnableRow = new QWidget(m_positionCorrectionPanel);
        QHBoxLayout *positionEnableLayout = new QHBoxLayout(positionEnableRow);
        positionEnableLayout->setContentsMargins(0, 0, 0, 0);
        positionEnableLayout->addWidget(new QLabel(tr("独立位置修正使能 ⓘ"), positionEnableRow));
        positionEnableLayout->addStretch(1);
        if (!m_positionCorrectionCheckBox) {
            m_positionCorrectionCheckBox = new QCheckBox(positionEnableRow);
            m_positionCorrectionCheckBox->setObjectName(QStringLiteral("positionCorrectionSwitch"));
        }
        positionEnableLayout->addWidget(m_positionCorrectionCheckBox);

        if (!m_positionCorrectionSourceRow) {
            m_positionCorrectionSourceRow = new QWidget(m_positionCorrectionPanel);
            QHBoxLayout *positionSourceLayout = new QHBoxLayout(m_positionCorrectionSourceRow);
            positionSourceLayout->setContentsMargins(0, 0, 0, 0);
            QLabel *positionSourceLabel = new QLabel(tr("位置修正"), m_positionCorrectionSourceRow);
            positionSourceLabel->setMinimumWidth(118);
            positionSourceLabel->setProperty("role", QStringLiteral("rowField"));
            positionSourceLayout->addWidget(positionSourceLabel);
            positionSourceLayout->addStretch(1);
            m_positionCorrectionComboBox = new QComboBox(m_positionCorrectionSourceRow);
            m_positionCorrectionComboBox->addItem(QStringLiteral("1 基准图.位置修正信息"));
            positionSourceLayout->addWidget(m_positionCorrectionComboBox);
        }
        positionPanelLayout->addWidget(positionEnableRow);
        positionPanelLayout->addWidget(m_positionCorrectionSourceRow);
        QLabel *positionHint = new QLabel(
                    tr("接口预留，暂未实现"), m_positionCorrectionPanel);
        positionPanelLayout->addWidget(positionHint);
        m_positionCorrectionPanel->setEnabled(false);
        detectLayout->addWidget(m_positionCorrectionPanel);

        if (allMode) {
            QWidget *detectMaskRow = new QWidget(detectCard);
            m_detectMaskRow = detectMaskRow;
            QHBoxLayout *detectMaskLayout = new QHBoxLayout(detectMaskRow);
            detectMaskLayout->setContentsMargins(0, 0, 0, 0);
            detectMaskLayout->addWidget(new QLabel(tr("屏蔽区域"), detectMaskRow));
            detectMaskLayout->addStretch(1);
            if (!m_detectMaskEditButton)
                m_detectMaskEditButton = new QPushButton(tr("编辑"), this);
            m_detectMaskEditButton->setObjectName(
                        QStringLiteral("colorComparisonDetectMaskEditButton"));
            if (!m_detectMaskPolygonButton) {
                m_detectMaskPolygonButton = new QToolButton(this);
                m_detectMaskPolygonButton->setText(QStringLiteral("⬡"));
                m_detectMaskPolygonButton->setCheckable(true);
            }
            m_detectMaskPolygonButton->setObjectName(
                        QStringLiteral("colorComparisonDetectMaskPolygonButton"));
            if (!m_detectMaskFinishButton)
                m_detectMaskFinishButton = new QPushButton(tr("完成"), this);
            m_detectMaskFinishButton->setObjectName(
                        QStringLiteral("colorComparisonDetectMaskFinishButton"));
            detectMaskLayout->addWidget(m_detectMaskEditButton);
            detectMaskLayout->addWidget(m_detectMaskPolygonButton);
            detectMaskLayout->addWidget(m_detectMaskFinishButton);
            detectLayout->addWidget(detectMaskRow);
        }
        layout->addWidget(detectCard);

        QFrame *settingsCard = card(page, tr("识别设置"));
        QVBoxLayout *settingsLayout = qobject_cast<QVBoxLayout *>(settingsCard->layout());
        if (!m_sensitivityComboBox) {
            m_sensitivityComboBox = new QComboBox(this);
            m_sensitivityComboBox->setObjectName(
                        QStringLiteral("colorComparisonSensitivityCombo"));
            m_sensitivityComboBox->addItem(tr("高（严格）"),
                                            QStringLiteral("high"));
            m_sensitivityComboBox->addItem(tr("中（标准）"),
                                            QStringLiteral("medium"));
            m_sensitivityComboBox->addItem(tr("低（宽松）"),
                                            QStringLiteral("low"));
            m_sensitivityComboBox->setCurrentIndex(1);
        }
        settingsLayout->addLayout(row(tr("灵敏度"), m_sensitivityComboBox));
        layout->addWidget(settingsCard);

        QFrame *judgeCard = card(page, tr("结果判断"));
        QVBoxLayout *judgeLayout = qobject_cast<QVBoxLayout *>(judgeCard->layout());
        if (!m_minScoreSpinBox) {
            m_minScoreSpinBox = new QSpinBox(this);
            m_minScoreSpinBox->setObjectName(
                        QStringLiteral("colorComparisonMinScore"));
            m_minScoreSpinBox->setRange(0, 100);
            m_minScoreSpinBox->setValue(80);
        }
        judgeLayout->addLayout(row(tr("最小得分"), m_minScoreSpinBox));
        layout->addWidget(judgeCard);
        layout->addStretch(1);
        return page;
    };

    m_paramsStack->addWidget(buildPage(true));

    QFrame *bottomBar = new QFrame(leftPanel);
    bottomBar->setObjectName(QStringLiteral("colorComparisonBottomActionBar"));
    bottomBar->setProperty("panelRole", QStringLiteral("bottomActionBar"));
    QHBoxLayout *bottomButtons = new QHBoxLayout(bottomBar);
    bottomButtons->setContentsMargins(0, 0, 0, 0);
    bottomButtons->setSpacing(8);
    m_referenceTestButton = new QPushButton(tr("基准图测试"), leftPanel);
    m_testRunButton = new QPushButton(tr("测试运行"), leftPanel);
    m_finishButton = new QPushButton(tr("完成"), leftPanel);
    m_exitTestButton = new QPushButton(tr("退出测试"), leftPanel);
    m_referenceTestButton->setObjectName(
                QStringLiteral("colorComparisonReferenceTestButton"));
    m_testRunButton->setObjectName(
                QStringLiteral("colorComparisonTestRunButton"));
    m_finishButton->setObjectName(
                QStringLiteral("colorComparisonFinishButton"));
    applyBottomActionButtonMetrics(m_referenceTestButton);
    applyBottomActionButtonMetrics(m_testRunButton);
    applyBottomActionButtonMetrics(m_finishButton);
    applyBottomActionButtonMetrics(m_exitTestButton);
    m_referenceTestButton->setProperty("actionRole", QStringLiteral("testAction"));
    m_testRunButton->setProperty("actionRole", QStringLiteral("testAction"));
    m_testRunButton->setProperty("running", false);
    m_finishButton->setProperty("actionRole", QStringLiteral("testPrimary"));
    m_exitTestButton->setProperty("actionRole", QStringLiteral("testAction"));
    m_exitTestButton->setObjectName(QStringLiteral("exitTestButton"));
    installActionButtonFlash(m_referenceTestButton);
    installActionButtonFlash(m_testRunButton);
    installActionButtonFlash(m_finishButton);
    installActionButtonFlash(m_exitTestButton);
    bottomButtons->addWidget(m_referenceTestButton, 1);
    bottomButtons->addWidget(m_testRunButton, 1);
    bottomButtons->addWidget(m_finishButton, 1);
    bottomButtons->addWidget(m_exitTestButton, 1);
    leftLayout->addWidget(bottomBar, 0);

    QFrame *rightPanel = new QFrame(this);
    rightPanel->setObjectName(QStringLiteral("colorComparisonRightPanel"));
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_viewerTitleLabel = new QLabel(tr("基准图"), rightPanel);
    m_viewerTitleLabel->setObjectName(
                QStringLiteral("colorComparisonViewerTitleLabel"));
    m_viewerTitleLabel->setMinimumHeight(48);
    m_viewerTitleLabel->setContentsMargins(18, 0, 0, 0);
    m_previewGraphicsView = new QGraphicsView(rightPanel);
    m_viewerStatusLabel = new QLabel(tr("请编辑模板区域和检测区域"), rightPanel);
    m_viewerStatusLabel->setObjectName(
                QStringLiteral("colorComparisonStatusLabel"));
    m_viewerStatusLabel->setMinimumHeight(42);
    m_viewerStatusLabel->setContentsMargins(18, 0, 0, 0);
    rightLayout->addWidget(m_viewerTitleLabel);
    rightLayout->addWidget(m_previewGraphicsView, 1);
    rightLayout->addWidget(m_viewerStatusLabel);
    content->addWidget(rightPanel, 1);

    m_previewHelper = new FrameViewHelper(m_previewGraphicsView, this);
    m_previewHelper->setNavigationEnabled(true);

    setAllParamsMode(false);
    if (m_detectRectButton)
        m_detectRectButton->setChecked(true);
    refreshEditControls();
    refreshPositionCorrectionControls();

    connect(closeButton, &QToolButton::clicked, this, &ColorComparisonDialog::reject);
    connect(m_finishButton, &QPushButton::clicked, this, &ColorComparisonDialog::finishConfiguration);
}

void ColorComparisonDialog::connectControls()
{
    connect(m_basicButton, &QPushButton::clicked, this, [this]() { setAllParamsMode(false); });
    connect(m_allButton, &QPushButton::clicked, this, [this]() { setAllParamsMode(true); });
    connect(m_templateRegionModeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                if (m_loadingConfig)
                    return;
                m_templateRegionMode = m_templateRegionModeComboBox->itemData(index)
                        .toString();
                markModelStale(QStringLiteral("template_region_mode_changed"));
                updateTemplatePreview();
            });
    connect(m_templateEditButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::TemplateRect); });
    connect(m_templateRectButton, &QToolButton::clicked, this, [this]() { setEditState(EditState::TemplateRect); });
    connect(m_templateFinishButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::None); });
    connect(m_templateMaskEditButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::TemplateMaskPolygon); });
    connect(m_templateMaskPolygonButton, &QToolButton::clicked, this, [this]() { setEditState(EditState::TemplateMaskPolygon); });
    connect(m_templateMaskFinishButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::None); });
    connect(m_detectGlobalButton, &QToolButton::clicked, this, [this]() {
        m_globalDetection = true;
        m_detectRegionType = QStringLiteral("rectangle");
        m_detectRoi = QRectF(0.0, 0.0, 1.0, 1.0);
        m_detectCircle = CircleRoi();
        setEditState(EditState::None);
        refreshDetectRegionButtons();
        handleDetectionConfigChanged(QStringLiteral("detect_region_changed"));
    });
    connect(m_detectRectButton, &QToolButton::clicked, this, [this]() {
        m_globalDetection = false;
        m_detectRegionType = QStringLiteral("rectangle");
        m_detectCircle = CircleRoi();
        setEditState(EditState::DetectRect);
        refreshDetectRegionButtons();
        handleDetectionConfigChanged(QStringLiteral("detect_region_changed"));
    });
    connect(m_detectCircleButton, &QToolButton::clicked, this, [this]() {
        m_globalDetection = false;
        m_detectRegionType = QStringLiteral("circle");
        setEditState(EditState::DetectCircle);
        refreshDetectRegionButtons();
        handleDetectionConfigChanged(QStringLiteral("detect_region_changed"));
    });
    connect(m_positionCorrectionCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_positionCorrectionEnabled = checked;
        refreshPositionCorrectionControls();
        invalidateAsyncWork();
    });
    connect(m_positionCorrectionComboBox, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        m_positionCorrectionSource = text;
        invalidateAsyncWork();
    });
    connect(m_sensitivityComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this]() {
                if (m_loadingConfig)
                    return;
                invalidateAsyncWork();
                if (m_liveTestSource != LiveTestSource::None)
                    rerunLiveComparison();
            });
    connect(m_featureTypeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this]() {
                if (!m_loadingConfig)
                    markModelStale(QStringLiteral("feature_type_changed"));
            });
    connect(m_brightnessCheckBox, &QCheckBox::toggled, this, [this]() {
        if (!m_loadingConfig)
            markModelStale(QStringLiteral("brightness_compensation_changed"));
    });
    connect(m_minScoreSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                if (m_featureView)
                    m_featureView->setThreshold(value);
                if (m_loadingConfig)
                    return;
                invalidateAsyncWork();
                if (m_liveTestSource != LiveTestSource::None)
                    rerunLiveComparison();
            });
    if (m_detectMaskEditButton)
        connect(m_detectMaskEditButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::DetectMaskPolygon); });
    if (m_detectMaskPolygonButton)
        connect(m_detectMaskPolygonButton, &QToolButton::clicked, this, [this]() { setEditState(EditState::DetectMaskPolygon); });
    if (m_detectMaskFinishButton)
        connect(m_detectMaskFinishButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::None); });
    connect(m_rebuildModelButton,
            &QPushButton::clicked,
            this,
            &ColorComparisonDialog::startExplicitModelRebuild);
    connect(m_referenceTestButton, &QPushButton::clicked, this, &ColorComparisonDialog::runReferenceTest);
    connect(m_testRunButton, &QPushButton::clicked, this, &ColorComparisonDialog::runTest);
    connect(m_exitTestButton, &QPushButton::clicked, this, &ColorComparisonDialog::exitTestMode);

    connect(m_previewHelper, &FrameViewHelper::roiChanged, this, &ColorComparisonDialog::handleRoiChanged);
    connect(m_previewHelper, &FrameViewHelper::circleChanged, this, &ColorComparisonDialog::handleCircleChanged);
    connect(m_previewHelper, &FrameViewHelper::polygonChanged, this, &ColorComparisonDialog::handlePolygonChanged);
}

void ColorComparisonDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (m_featureView)
        m_featureView->syncZoomGeometry();
    updateTemplatePreview();
}

void ColorComparisonDialog::setAllParamsMode(bool allMode)
{
    m_paramsStack->setCurrentIndex(0);
    m_basicButton->setChecked(!allMode);
    m_allButton->setChecked(allMode);
    if (m_featureCard)
        m_featureCard->setVisible(allMode);
    if (!allMode && m_featureView)
        m_featureView->closeZoom();
    if (m_detectMaskRow)
        m_detectMaskRow->setVisible(allMode);
    if (m_positionCorrectionPanel)
        m_positionCorrectionPanel->setVisible(allMode);
}

void ColorComparisonDialog::setEditState(EditState state)
{
    const bool templateEdit = state == EditState::TemplateRect
            || state == EditState::TemplateMaskPolygon;
    if (templateEdit && m_liveTestSource == LiveTestSource::Camera) {
        updateStatus(tr("相机测试态不可编辑模板区域，请先退出测试"));
        return;
    }

    m_editState = state;
    refreshEditControls();
    refreshDetectRegionButtons();
    if (!m_previewHelper)
        return;
    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->clearRoi();
    m_previewHelper->clearCircleRoi();
    m_previewHelper->clearPolygonRoi();
    if (state == EditState::TemplateRect || state == EditState::DetectRect)
        m_previewHelper->setRoiDrawingEnabled(true);
    if (state == EditState::DetectCircle)
        m_previewHelper->setCircleDrawingEnabled(true);
    if (state == EditState::TemplateMaskPolygon || state == EditState::DetectMaskPolygon)
        m_previewHelper->setPolygonDrawingEnabled(true);
    refreshRoiOverlay();

    QString statusText = tr("ROI 编辑完成");
    switch (state) {
    case EditState::TemplateRect:
        statusText = tr("请在右侧基准图上拖拽模板矩形区域");
        break;
    case EditState::TemplateMaskPolygon:
        statusText = tr("请在右侧基准图上绘制模板屏蔽多边形，双击完成");
        break;
    case EditState::DetectRect:
        statusText = tr("请在右侧基准图上拖拽检测矩形区域");
        break;
    case EditState::DetectCircle:
        statusText = tr("请在右侧基准图上拖拽检测圆形区域");
        break;
    case EditState::DetectMaskPolygon:
        statusText = tr("请在右侧基准图上绘制检测屏蔽多边形，双击完成");
        break;
    case EditState::None:
        break;
    }
    updateStatus(statusText);
}

void ColorComparisonDialog::showPreviewImage()
{
    const ReferenceFrameSnapshot reference =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    cv::Mat frame = reference.frame;
    m_previewUsesReferenceImage = !frame.empty();
    if (frame.empty()) {
        const CameraFrameSnapshot camera =
                CameraFrameProvider::instance().currentFrameSnapshot();
        frame = camera.frame;
    }
    if (m_viewerTitleLabel) {
        m_viewerTitleLabel->setText(m_previewUsesReferenceImage
                                    ? tr("基准图") : tr("当前图像"));
    }
    const QImage image = imageFromFrame(frame);
    m_previewImage = image;
    if (m_previewHelper)
        m_previewHelper->clearToolOverlays();
    if (!image.isNull()) {
        m_previewHelper->setImage(image);
        updateTemplatePreview();
    } else {
        m_previewImage = QImage();
        if (m_previewHelper)
            m_previewHelper->clear();
        updateTemplatePreview();
    }
}

void ColorComparisonDialog::showFrameImage(const cv::Mat &frame, const QString &title)
{
    const QImage image = imageFromFrame(frame);
    m_previewImage = image;
    if (m_viewerTitleLabel)
        m_viewerTitleLabel->setText(title);
    if (!m_previewHelper)
        return;

    m_previewHelper->clearToolOverlays();
    if (!image.isNull()) {
        m_previewHelper->setImage(image);
        updateTemplatePreview();
        refreshRoiOverlay();
    } else {
        m_previewHelper->clear();
        updateTemplatePreview();
    }
}

void ColorComparisonDialog::refreshRoiOverlay()
{
    if (!m_previewHelper)
        return;
    if (m_liveTestSource == LiveTestSource::Camera
            && (m_editState == EditState::TemplateRect
                || m_editState == EditState::TemplateMaskPolygon)) {
        m_previewHelper->clearRoi();
        m_previewHelper->clearPolygonRoi();
        return;
    }
    if (m_editState == EditState::TemplateRect)
        m_previewHelper->setRoiRectNormalized(m_templateRoi);
    else if (m_editState == EditState::DetectRect)
        m_previewHelper->setRoiRectNormalized(m_detectRoi);
    else if (m_editState == EditState::DetectCircle && m_detectCircle.valid)
        m_previewHelper->setCircleRoiNormalized(m_detectCircle);
    else if (m_editState == EditState::TemplateMaskPolygon && m_templateMask.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(m_templateMask);
    else if (m_editState == EditState::DetectMaskPolygon && m_detectMask.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(m_detectMask);
}

void ColorComparisonDialog::updateStatus(const QString &text)
{
    if (m_viewerStatusLabel)
        m_viewerStatusLabel->setText(text);
}

void ColorComparisonDialog::updateTemplatePreview()
{
    if (!m_templatePreviewLabel)
        return;

    const QImage roiImage = templateRawRoiImage();
    if (roiImage.isNull()) {
        m_templatePreviewLabel->clear();
        m_templatePreviewLabel->setText(tr("无模板图像"));
        return;
    }

    const QSize targetSize(132, 86);
    QPixmap canvas(targetSize);
    canvas.fill(Qt::white);
    const QPixmap scaled = QPixmap::fromImage(roiImage).scaled(targetSize - QSize(12, 12),
                                                               Qt::KeepAspectRatio,
                                                               Qt::SmoothTransformation);
    QPainter painter(&canvas);
    const QPoint topLeft((targetSize.width() - scaled.width()) / 2,
                         (targetSize.height() - scaled.height()) / 2);
    painter.drawPixmap(topLeft, scaled);
    m_templatePreviewLabel->setText(QString());
    m_templatePreviewLabel->setPixmap(canvas);
}

void ColorComparisonDialog::handleRoiChanged(const QRectF &roi)
{
    if (m_editState == EditState::TemplateRect) {
        m_templateRoi = normalizedRoiOrDefault(roi);
        markModelStale(QStringLiteral("template_roi_changed"));
        updateTemplatePreview();
        refreshRoiOverlay();
    } else if (m_editState == EditState::DetectRect) {
        m_detectRoi = normalizedRoiOrDefault(roi);
        m_globalDetection = false;
        refreshRoiOverlay();
        handleDetectionConfigChanged(QStringLiteral("detect_roi_changed"));
    }
}

void ColorComparisonDialog::handleCircleChanged(const CircleRoi &circle)
{
    if (m_editState != EditState::DetectCircle)
        return;

    const QRectF exactBounds = circleBoundingRectForImage(
                circle, m_previewImage.width(), m_previewImage.height());
    if (!circleBoundingRectFitsImage(exactBounds)) {
        updateStatus(tr("圆形检测区域无效或超出图像范围，请重新绘制"));
        if (m_previewHelper)
            m_previewHelper->clearCircleRoi();
        refreshRoiOverlay();
        return;
    }

    m_detectCircle = circle;
    m_detectCircle.boundingRectNormalized = exactBounds;
    m_detectCircle.valid = true;
    m_globalDetection = false;
    m_detectRoi = exactBounds;
    refreshRoiOverlay();
    handleDetectionConfigChanged(QStringLiteral("detect_circle_changed"));
}

void ColorComparisonDialog::handlePolygonChanged(const QVector<QPointF> &points)
{
    if (m_editState == EditState::TemplateMaskPolygon) {
        m_templateMask = points.size() >= 3 ? points : QVector<QPointF>();
        markModelStale(QStringLiteral("template_mask_changed"));
        updateTemplatePreview();
    } else if (m_editState == EditState::DetectMaskPolygon) {
        m_detectMask = points.size() >= 3 ? points : QVector<QPointF>();
        handleDetectionConfigChanged(QStringLiteral("detect_mask_changed"));
    }
    refreshRoiOverlay();
}

QRectF ColorComparisonDialog::normalizedRoiOrDefault(const QRectF &roi) const
{
    if (!finiteValue(roi.x()) || !finiteValue(roi.y()) ||
        !finiteValue(roi.width()) || !finiteValue(roi.height()) ||
        roi.width() <= 0.0 || roi.height() <= 0.0) {
        return QRectF(0.0, 0.0, 1.0, 1.0);
    }
    return roi.intersected(QRectF(0.0, 0.0, 1.0, 1.0));
}

QImage ColorComparisonDialog::templateRawRoiImage() const
{
    const ReferenceFrameSnapshot snapshot =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    const QImage referenceImage = imageFromFrame(snapshot.frame);
    if (referenceImage.isNull())
        return QImage();

    QRectF syncCircleRoi;
    QRectF effectiveRoi = m_templateRoi;
    if (m_templateRegionMode == QStringLiteral("sync")) {
        if (m_globalDetection) {
            effectiveRoi = QRectF(0.0, 0.0, 1.0, 1.0);
        } else if (m_detectRegionType == QStringLiteral("circle")
                   && m_detectCircle.valid) {
            syncCircleRoi = circleBoundingRectForImage(
                        m_detectCircle,
                        referenceImage.width(),
                        referenceImage.height());
            if (!circleBoundingRectFitsImage(syncCircleRoi))
                return QImage();
            effectiveRoi = syncCircleRoi;
        } else {
            effectiveRoi = m_detectRoi;
        }
    }
    const QRectF roi = normalizedRoiOrDefault(effectiveRoi);
    if (roi.width() <= 0.0 || roi.height() <= 0.0)
        return QImage();

    const int left = qBound(0,
                            qRound(roi.left() * referenceImage.width()),
                            referenceImage.width() - 1);
    const int top = qBound(0,
                           qRound(roi.top() * referenceImage.height()),
                           referenceImage.height() - 1);
    const int rightExclusive = qBound(
                left + 1,
                qRound(roi.right() * referenceImage.width()),
                referenceImage.width());
    const int bottomExclusive = qBound(
                top + 1,
                qRound(roi.bottom() * referenceImage.height()),
                referenceImage.height());
    const QRect sourceRect(left,
                           top,
                           rightExclusive - left,
                           bottomExclusive - top);
    return referenceImage.copy(sourceRect.intersected(referenceImage.rect()));
}

void ColorComparisonDialog::refreshEditControls()
{
    const bool editingTemplate = m_editState == EditState::TemplateRect;
    const bool editingTemplateMask = m_editState == EditState::TemplateMaskPolygon;
    const bool editingDetectMask = m_editState == EditState::DetectMaskPolygon;

    if (m_templateEditButton)
        m_templateEditButton->setVisible(!editingTemplate);
    if (m_templateRectButton) {
        m_templateRectButton->setVisible(editingTemplate);
        m_templateRectButton->setChecked(editingTemplate);
    }
    if (m_templateFinishButton)
        m_templateFinishButton->setVisible(editingTemplate);

    if (m_templateMaskEditButton)
        m_templateMaskEditButton->setVisible(!editingTemplateMask);
    if (m_templateMaskPolygonButton) {
        m_templateMaskPolygonButton->setVisible(editingTemplateMask);
        m_templateMaskPolygonButton->setChecked(editingTemplateMask);
    }
    if (m_templateMaskFinishButton)
        m_templateMaskFinishButton->setVisible(editingTemplateMask);

    if (m_detectMaskEditButton)
        m_detectMaskEditButton->setVisible(!editingDetectMask);
    if (m_detectMaskPolygonButton) {
        m_detectMaskPolygonButton->setVisible(editingDetectMask);
        m_detectMaskPolygonButton->setChecked(editingDetectMask);
    }
    if (m_detectMaskFinishButton)
        m_detectMaskFinishButton->setVisible(editingDetectMask);
}

void ColorComparisonDialog::refreshDetectRegionButtons()
{
    if (!m_detectGlobalButton || !m_detectRectButton || !m_detectCircleButton)
        return;

    const QSignalBlocker blockGlobal(m_detectGlobalButton);
    const QSignalBlocker blockRect(m_detectRectButton);
    const QSignalBlocker blockCircle(m_detectCircleButton);
    m_detectGlobalButton->setChecked(m_globalDetection);
    m_detectRectButton->setChecked(!m_globalDetection
                                   && m_detectRegionType == QStringLiteral("rectangle"));
    m_detectCircleButton->setChecked(m_detectRegionType == QStringLiteral("circle"));
}

void ColorComparisonDialog::refreshPositionCorrectionControls()
{
    if (m_positionCorrectionCheckBox) {
        const QSignalBlocker block(m_positionCorrectionCheckBox);
        m_positionCorrectionCheckBox->setChecked(m_positionCorrectionEnabled);
    }
    if (m_positionCorrectionSourceRow)
        m_positionCorrectionSourceRow->setVisible(m_positionCorrectionEnabled);
    if (m_positionCorrectionComboBox) {
        const int index = m_positionCorrectionComboBox->findText(m_positionCorrectionSource);
        if (index >= 0) {
            const QSignalBlocker block(m_positionCorrectionComboBox);
            m_positionCorrectionComboBox->setCurrentIndex(index);
        }
    }
}

void ColorComparisonDialog::handleDetectionConfigChanged(const QString &reason)
{
    if (m_loadingConfig)
        return;

    if (m_templateRegionMode == QStringLiteral("sync")) {
        markModelStale(reason);
        updateTemplatePreview();
    } else {
        invalidateAsyncWork();
    }

    if (m_liveTestSource != LiveTestSource::None)
        rerunLiveComparison();
}

void ColorComparisonDialog::invalidateAsyncWork()
{
    ++m_testGeneration;
    m_pendingContinuousRun = false;
    m_pendingTestRequest = ToolRequest();
    m_pendingImageTitle.clear();
    m_pendingReferenceSource = false;
    m_pendingTestGeneration = 0;
}

bool ColorComparisonDialog::invalidateModelBuild()
{
    ++m_modelBuildGeneration;
    const bool wasActive = m_modelBuildUiActive;
    m_modelBuildUiActive = false;
    return wasActive;
}

void ColorComparisonDialog::markModelStale(const QString &reason)
{
    if (m_loadingConfig)
        return;

    const bool buildWasActive = invalidateModelBuild();
    invalidateAsyncWork();
    if (m_model.state == ColorComparisonModelState::Ready
            || m_model.state == ColorComparisonModelState::Stale) {
        m_model.state = ColorComparisonModelState::Stale;
        m_modelStatus = QStringLiteral("model_stale");
        m_modelReason = tr("%1，请重新取样").arg(reason);
    } else if (m_model.state == ColorComparisonModelState::Empty) {
        m_modelStatus = QStringLiteral("model_empty");
        m_modelReason = tr("尚未取样，请点击重新取样");
    }
    updateModelStateUi();
    if (buildWasActive)
        displayStoredModelInstruction();
}

void ColorComparisonDialog::updateModelStateUi()
{
    const QString state = colorComparisonModelStateName(m_model.state);
    if (m_modelStateLabel) {
        QString instruction = m_modelReason;
        if (m_model.state == ColorComparisonModelState::Stale
                && !instruction.contains(tr("重新取样"))) {
            instruction += tr("，请重新取样");
        }
        m_modelStateLabel->setText(
                    tr("模型状态：%1 | %2%3")
                    .arg(state,
                         m_modelStatus,
                         instruction.isEmpty()
                         ? QString()
                         : QStringLiteral(" | ") + instruction));
    }
    updateFeaturePreview();
}

void ColorComparisonDialog::updateFeaturePreview()
{
    if (!m_featureView)
        return;
    m_featureView->setThreshold(m_minScoreSpinBox
                                ? m_minScoreSpinBox->value() : 80);
    if (m_model.state != ColorComparisonModelState::Ready
            || m_model.hueBins != 32 || m_model.saturationBins != 32
            || !ColorComparisonFeatureView::validNormalizedHistogram(
                m_model.values, 1024)
            || !ColorComparisonFeatureView::validNormalizedHistogram(
                m_model.valueHistogram, 32)) {
        m_featureView->clearTemplate();
        return;
    }
    m_featureView->setTemplateHistograms(m_model.values,
                                         m_model.valueHistogram);
}

bool ColorComparisonDialog::updateDetectionFeaturePreview(
        const ToolResult &result)
{
    if (!m_featureView)
        return false;
    const QJsonObject diagnostics = result.payload.value(
                QStringLiteral("histogramDiagnostics")).toObject();
    if (!diagnostics.value(QStringLiteral("available")).toBool()) {
        m_featureView->clearDetection(tr("检测特征不可用：%1")
                                      .arg(result.status));
        return false;
    }
    if (diagnostics.value(QStringLiteral("hueBins")).toInt(-1) != 32
            || diagnostics.value(QStringLiteral("saturationBins")).toInt(-1) != 32
            || diagnostics.value(QStringLiteral("layout")).toString()
               != QStringLiteral("hue_major")) {
        m_featureView->clearDetection(tr("检测特征不可用：维度或布局不匹配"));
        return false;
    }
    QVector<double> hsHistogram;
    QVector<double> valueHistogram;
    if (!normalizedHistogramFromJson(
                diagnostics.value(QStringLiteral("detectHsHistogram")),
                1024,
                &hsHistogram)
            || !normalizedHistogramFromJson(
                diagnostics.value(QStringLiteral("detectValueHistogram")),
                32,
                &valueHistogram)) {
        m_featureView->clearDetection(tr("检测特征不可用：直方图字段无效"));
        return false;
    }
    const double rawIntersection = diagnostics.value(
                QStringLiteral("rawIntersection")).toDouble(
                std::numeric_limits<double>::quiet_NaN());
    const QJsonObject brightness = result.payload.value(
                QStringLiteral("brightnessCompensation")).toObject();
    QString brightnessState = tr("关闭");
    const bool brightnessRequested = brightness.value(
                QStringLiteral("requested")).toBool(
                brightness.value(QStringLiteral("enabled")).toBool());
    if (brightness.value(QStringLiteral("fallback")).toBool()) {
        brightnessState = tr("已跳过：%1").arg(
                    brightness.value(QStringLiteral("fallbackReason")).toString());
    } else if (brightness.value(QStringLiteral("applied")).toBool()) {
        brightnessState = tr("已应用");
    } else if (brightnessRequested) {
        brightnessState = tr("未应用");
    }
    return m_featureView->setDetectionHistograms(
                hsHistogram,
                valueHistogram,
                rawIntersection,
                result.score,
                m_minScoreSpinBox ? m_minScoreSpinBox->value() : 80,
                result.payload.value(QStringLiteral("hsScore")).toDouble(-1.0),
                result.payload.value(QStringLiteral("brightnessFactor")).toDouble(-1.0),
                result.payload.value(QStringLiteral("saturationFactor")).toDouble(-1.0),
                brightnessState);
}

QJsonObject ColorComparisonDialog::colorComparisonParams() const
{
    QJsonObject params;
    params.insert(QStringLiteral("version"), 2);
    params.insert(QStringLiteral("templateRegionMode"), m_templateRegionMode);
    params.insert(QStringLiteral("templateRoiNormalized"), rectToJson(m_templateRoi));
    params.insert(QStringLiteral("templateMaskPolygon"), pointsToJson(m_templateMask));
    params.insert(QStringLiteral("model"), colorComparisonModelToJson(m_model));
    params.insert(QStringLiteral("dialogLifecycle"),
                  QJsonObject{
                      {QStringLiteral("originVersion"), m_modelOriginVersion},
                      {QStringLiteral("status"), m_modelStatus},
                      {QStringLiteral("reason"), m_modelReason}
                  });

    params.insert(QStringLiteral("detectRegionType"),
                  m_globalDetection ? QStringLiteral("rectangle")
                                    : m_detectRegionType);
    params.insert(QStringLiteral("detectGlobal"), m_globalDetection);
    params.insert(QStringLiteral("detectRoiNormalized"),
                  rectToJson(m_globalDetection
                             ? QRectF(0.0, 0.0, 1.0, 1.0)
                             : m_detectRoi));
    params.insert(QStringLiteral("detectCircleNormalized"), circleToJson(m_detectCircle));
    params.insert(QStringLiteral("detectMaskPolygon"), pointsToJson(m_detectMask));

    const QString sensitivity = m_sensitivityComboBox
            ? m_sensitivityComboBox->currentData().toString()
            : QStringLiteral("medium");
    params.insert(QStringLiteral("comparison"),
                  QJsonObject{
                      {QStringLiteral("sensitivity"),
                       sensitivity.isEmpty() ? QStringLiteral("medium")
                                             : sensitivity},
                      {QStringLiteral("brightnessCompensation"),
                       m_brightnessCheckBox
                               && m_brightnessCheckBox->isChecked()}
                  });
    params.insert(QStringLiteral("positionCorrection"),
                  QJsonObject{
                      {QStringLiteral("enabled"), m_positionCorrectionEnabled},
                      {QStringLiteral("sourceId"), m_positionCorrectionSource},
                      {QStringLiteral("interfaceVersion"), 1}
                  });
    return params;
}

ToolConfig ColorComparisonDialog::toToolConfig() const
{
    if (m_invalidConfigReadOnly)
        return m_originalInvalidConfig;

    ToolConfig config;
    config.toolId = m_toolId.isEmpty()
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : m_toolId;
    config.toolName = QStringLiteral("ColorComparison");
    config.displayName = tr("颜色比较");
    config.toolType = ToolType::ColorComparison;
    config.category = ToolCategory::Recognition;
    config.enabled = m_enabled;
    config.roiNormalized = m_globalDetection
            ? QRectF(0.0, 0.0, 1.0, 1.0)
            : m_detectRoi;
    QJsonObject params;
    params.insert(QStringLiteral("colorComparison"), colorComparisonParams());
    config.params = params;
    QJsonObject judge;
    judge.insert(QStringLiteral("mode"), QStringLiteral("min_score"));
    judge.insert(QStringLiteral("minScore"), m_minScoreSpinBox ? m_minScoreSpinBox->value() : 80);
    config.judgeRule = judge;
    config.summary = summaryText();
    return config;
}

ToolConfig ColorComparisonDialog::toolConfig() const
{
    return toToolConfig();
}

ToolPreviewSnapshot ColorComparisonDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void ColorComparisonDialog::updateInvalidConfigReadOnlyUi()
{
    const bool editable = !m_invalidConfigReadOnly;
    if (m_basicButton)
        m_basicButton->setEnabled(editable);
    if (m_allButton)
        m_allButton->setEnabled(editable);
    if (m_paramsStack)
        m_paramsStack->setEnabled(editable);
    if (m_referenceTestButton)
        m_referenceTestButton->setEnabled(editable);
    if (m_testRunButton)
        m_testRunButton->setEnabled(editable);
    if (m_finishButton)
        m_finishButton->setEnabled(editable);
    if (m_exitTestButton)
        m_exitTestButton->setEnabled(editable);
    if (m_rebuildModelButton) {
        m_rebuildModelButton->setEnabled(
                    editable
                    && (!m_modelBuildWatcher
                        || !m_modelBuildWatcher->isRunning()));
    }
    if (m_positionCorrectionPanel)
        m_positionCorrectionPanel->setEnabled(false);
}

void ColorComparisonDialog::enterInvalidConfigReadOnly(
        const ToolConfig &config,
        const QString &status,
        const QString &message)
{
    if (m_continuousTimer)
        m_continuousTimer->stop();
    invalidateAsyncWork();
    invalidateModelBuild();

    m_loadingConfig = true;
    m_invalidConfigReadOnly = true;
    m_originalInvalidConfig = config;
    m_invalidConfigStatus = status.isEmpty()
            ? QStringLiteral("invalid_v2_config") : status;
    m_invalidConfigMessage = tr("V2 配置无效，已只读打开，原始配置保持不变：%1")
            .arg(message.isEmpty() ? tr("配置字段不符合合同") : message);
    if (!config.toolId.isEmpty())
        m_toolId = config.toolId;
    m_enabled = config.enabled;
    m_model = ColorComparisonModelV2();
    m_model.state = ColorComparisonModelState::Invalid;
    m_modelOriginVersion = 2;
    m_modelStatus = m_invalidConfigStatus;
    m_modelReason = m_invalidConfigMessage;
    m_testUiMode = TestUiMode::Edit;
    m_liveTestSource = LiveTestSource::None;
    m_liveTestFrameSnapshot.release();
    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    setEditState(EditState::None);
    m_loadingConfig = false;

    updateBottomButtons();
    updateInvalidConfigReadOnlyUi();
    updateModelStateUi();
    displayError(m_invalidConfigStatus, m_invalidConfigMessage);
}

void ColorComparisonDialog::leaveInvalidConfigReadOnly()
{
    if (!m_invalidConfigReadOnly)
        return;

    m_invalidConfigReadOnly = false;
    m_originalInvalidConfig = ToolConfig();
    m_invalidConfigStatus.clear();
    m_invalidConfigMessage.clear();
    updateInvalidConfigReadOnlyUi();
}

bool ColorComparisonDialog::blockInvalidConfigAction()
{
    if (!m_invalidConfigReadOnly)
        return false;
    displayError(m_invalidConfigStatus, m_invalidConfigMessage);
    return true;
}

void ColorComparisonDialog::loadFromConfig(const ToolConfig &config)
{
    if (config.toolType != ToolType::Unknown && config.toolType != ToolType::ColorComparison)
        return;

    const QJsonValue colorComparisonValue = config.params
            .value(QStringLiteral("colorComparison"));
    const QJsonObject colorComparison = colorComparisonValue.toObject();
    if (!colorComparisonValue.isObject()) {
        enterInvalidConfigReadOnly(
                    config,
                    QStringLiteral("unsupported_model_version"),
                    tr("颜色比较参数缺失或不是对象；原配置已只读保留"));
        return;
    }
    int serializedVersion = 0;
    const bool hasIntegerVersion = strictJsonInteger(
                colorComparison.value(QStringLiteral("version")),
                &serializedVersion);
    if (!hasIntegerVersion) {
        enterInvalidConfigReadOnly(
                    config,
                    QStringLiteral("unsupported_model_version"),
                    tr("颜色比较模型版本必须是 int 范围内的整数；原配置已只读保留"));
        return;
    }
    if (serializedVersion == 2) {
        const V2DialogConfigValidation validation =
                validateV2DialogConfig(config);
        if (!validation.success) {
            enterInvalidConfigReadOnly(config,
                                       validation.status,
                                       validation.message);
            return;
        }
    }

    const bool recoveredFromInvalidConfig = m_invalidConfigReadOnly;
    leaveInvalidConfigReadOnly();

    const bool buildWasActive = invalidateModelBuild();
    invalidateAsyncWork();
    m_loadingConfig = true;
    if (!config.toolId.isEmpty())
        m_toolId = config.toolId;
    m_enabled = config.enabled;
    m_modelOriginVersion = serializedVersion;

    m_templateRegionMode = colorComparison
            .value(QStringLiteral("templateRegionMode"))
            .toString(QStringLiteral("custom")).trimmed().toLower();
    if (m_templateRegionMode != QStringLiteral("sync")
            && m_templateRegionMode != QStringLiteral("custom")) {
        m_templateRegionMode = QStringLiteral("custom");
    }
    m_templateRoi = normalizedRoiOrDefault(
                rectFromJson(colorComparison.value(QStringLiteral("templateRoiNormalized")).toObject(),
                             m_templateRoi));
    m_templateMask = pointsFromJson(colorComparison.value(QStringLiteral("templateMaskPolygon")).toArray());

    const QRectF nestedDetectRoi = rectFromJson(
                colorComparison.value(QStringLiteral("detectRoiNormalized")).toObject(),
                config.roiNormalized.width() > 0.0
                && config.roiNormalized.height() > 0.0
                ? config.roiNormalized : m_detectRoi);
    m_detectRoi = normalizedRoiOrDefault(nestedDetectRoi);
    m_detectRegionType = colorComparison.value(QStringLiteral("detectRegionType"))
            .toString(QStringLiteral("rectangle")).trimmed().toLower();
    m_globalDetection = colorComparison.value(QStringLiteral("detectGlobal"))
            .toBool(m_detectRegionType == QStringLiteral("global"));
    if (m_globalDetection)
        m_detectRegionType = QStringLiteral("rectangle");
    m_detectCircle = circleFromJson(colorComparison.value(QStringLiteral("detectCircleNormalized")).toObject());
    m_detectMask = pointsFromJson(colorComparison.value(QStringLiteral("detectMaskPolygon")).toArray());

    const QJsonObject comparison =
            colorComparison.value(QStringLiteral("comparison")).toObject();
    const QString sensitivity = comparison
            .value(QStringLiteral("sensitivity"))
            .toString(colorComparison.value(QStringLiteral("sensitivity"))
                      .toString(QStringLiteral("medium")));
    const bool brightnessEnabled = comparison
            .value(QStringLiteral("brightnessCompensation"))
            .toBool(colorComparison.value(QStringLiteral("brightnessEnabled"))
                    .toBool(false));

    const QJsonObject position =
            colorComparison.value(QStringLiteral("positionCorrection")).toObject();
    m_positionCorrectionEnabled = position
            .value(QStringLiteral("enabled"))
            .toBool(colorComparison.value(QStringLiteral("enablePositionCorrection"))
                    .toBool(false));
    m_positionCorrectionSource = position
            .value(QStringLiteral("sourceId"))
            .toString(colorComparison.value(QStringLiteral("positionCorrectionSource"))
                      .toString(QStringLiteral("1 基准图.位置修正信息")));

    const ReferenceFrameSnapshot reference =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    const ColorComparisonModelReadResult modelRead =
            readColorComparisonModel(colorComparison, !reference.frame.empty());
    m_model = modelRead.model;
    m_modelStatus = modelRead.status;
    m_modelReason = modelRead.message;
    if (serializedVersion == 2) {
        restoreDialogLifecycle(colorComparison,
                               m_model.state,
                               &m_modelOriginVersion,
                               &m_modelStatus,
                               &m_modelReason);
    }
    if (m_model.state == ColorComparisonModelState::Ready) {
        const QString currentReferenceHash = reference.frame.empty()
                ? QString()
                : colorComparisonReferenceHash(reference.frame);
        if (currentReferenceHash != m_model.referenceImageHash) {
            m_model.state = ColorComparisonModelState::Stale;
            m_modelStatus = QStringLiteral("model_stale");
            m_modelReason = tr("基准图已变化，请重新取样");
        }
    }

    if (m_templateRegionModeComboBox) {
        const QSignalBlocker blocker(m_templateRegionModeComboBox);
        const int index = m_templateRegionModeComboBox->findData(m_templateRegionMode);
        m_templateRegionModeComboBox->setCurrentIndex(index >= 0 ? index : 1);
    }
    if (m_sensitivityComboBox) {
        const QSignalBlocker blocker(m_sensitivityComboBox);
        const int index = m_sensitivityComboBox->findData(sensitivity);
        m_sensitivityComboBox->setCurrentIndex(index >= 0 ? index : 1);
    }
    if (m_featureTypeComboBox) {
        const QSignalBlocker blocker(m_featureTypeComboBox);
        m_featureTypeComboBox->setCurrentIndex(0);
    }
    if (m_brightnessCheckBox) {
        const QSignalBlocker blocker(m_brightnessCheckBox);
        m_brightnessCheckBox->setChecked(brightnessEnabled);
    }
    if (m_minScoreSpinBox) {
        const QSignalBlocker blocker(m_minScoreSpinBox);
        m_minScoreSpinBox->setValue(
                    config.judgeRule.value(QStringLiteral("minScore")).toInt(80));
    }

    setAllParamsMode(brightnessEnabled || m_detectMask.size() >= 3);
    refreshDetectRegionButtons();
    refreshPositionCorrectionControls();
    refreshEditControls();
    showPreviewImage();
    updateTemplatePreview();
    refreshRoiOverlay();
    m_loadingConfig = false;
    updateModelStateUi();
    if (buildWasActive || recoveredFromInvalidConfig) {
        if (m_model.state == ColorComparisonModelState::Ready)
            updateStatus(tr("模型已加载，可直接测试"));
        else
            displayStoredModelInstruction();
    }
}

QString ColorComparisonDialog::summaryText() const
{
    return tr("HSV直方图比较；最低分 %1").arg(m_minScoreSpinBox ? m_minScoreSpinBox->value() : 80);
}

void ColorComparisonDialog::runTest()
{
    if (blockInvalidConfigAction())
        return;

    if (m_testUiMode == TestUiMode::Continuous) {
        stopContinuousRun();
        m_testUiMode = TestUiMode::TestPaused;
        applyDetectRoiEditState();
        updateBottomButtons();
        updateStatus(tr("连续运行已停止，视图保留最后一帧，可继续绘制检测区域即时重测"));
        return;
    }

    startContinuousRun();
}

void ColorComparisonDialog::runReferenceTest()
{
    if (blockInvalidConfigAction())
        return;

    stopContinuousRun();
    m_liveTestSource = LiveTestSource::Reference;
    m_testUiMode = TestUiMode::Edit;
    applyDetectRoiEditState();
    updateBottomButtons();

    const ReferenceFrameSnapshot snapshot =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    if (snapshot.frame.empty()) {
        displayError(QStringLiteral("no_reference_image"), tr("请先设置基准图"));
        return;
    }

    showFrameImage(snapshot.frame, tr("基准图"));
    if (m_model.state != ColorComparisonModelState::Ready) {
        displayStoredModelInstruction();
        return;
    }
    runComparisonOnFrame(snapshot.frame,
                         snapshot.metadata,
                         tr("基准图"),
                         true);
}

void ColorComparisonDialog::startContinuousRun()
{
    if (blockInvalidConfigAction())
        return;

    invalidateAsyncWork();
    m_testUiMode = TestUiMode::Continuous;
    m_liveTestSource = LiveTestSource::Camera;
    applyDetectRoiEditState();
    updateBottomButtons();
    if (m_continuousTimer && !m_continuousTimer->isActive())
        m_continuousTimer->start();
    runContinuousTick();
}

void ColorComparisonDialog::stopContinuousRun()
{
    if (m_continuousTimer)
        m_continuousTimer->stop();
    invalidateAsyncWork();
}

void ColorComparisonDialog::runContinuousTick()
{
    if (blockInvalidConfigAction())
        return;

    if (m_testUiMode != TestUiMode::Continuous)
        return;

    const CameraFrameSnapshot snapshot =
            CameraFrameProvider::instance().currentFrameSnapshot();
    if (snapshot.frame.empty()) {
        displayError(QStringLiteral("image_empty"), tr("当前帧为空"));
        return;
    }

    showFrameImage(snapshot.frame, tr("测试图像"));
    runComparisonOnFrame(snapshot.frame,
                         snapshot.metadata,
                         tr("测试图像"),
                         false);
}

void ColorComparisonDialog::runSingleShotTest()
{
    if (blockInvalidConfigAction())
        return;

    stopContinuousRun();
    m_testUiMode = TestUiMode::TestPaused;
    m_liveTestSource = LiveTestSource::Camera;
    const CameraFrameSnapshot snapshot =
            CameraFrameProvider::instance().currentFrameSnapshot();
    m_liveTestFrameSnapshot = snapshot.frame.clone();
    m_liveTestFrameMetadata = snapshot.metadata;
    applyDetectRoiEditState();
    updateBottomButtons();

    if (m_liveTestFrameSnapshot.empty()) {
        displayError(QStringLiteral("image_empty"), tr("当前帧为空"));
        return;
    }

    showFrameImage(m_liveTestFrameSnapshot, tr("单次测试快照"));
    runComparisonOnFrame(m_liveTestFrameSnapshot,
                         m_liveTestFrameMetadata,
                         tr("单次测试快照"),
                         false);
}

void ColorComparisonDialog::rerunLiveComparison()
{
    if (blockInvalidConfigAction())
        return;

    if (m_liveTestSource == LiveTestSource::None)
        return;

    cv::Mat frame;
    FrameInputMetadata metadata;
    bool referenceSource = false;
    QString title;
    if (m_liveTestSource == LiveTestSource::Reference) {
        const ReferenceFrameSnapshot snapshot =
                ReferenceImageProvider::instance().referenceFrameSnapshot();
        frame = snapshot.frame;
        metadata = snapshot.metadata;
        referenceSource = true;
        title = tr("基准图");
        if (frame.empty()) {
            displayError(QStringLiteral("no_reference_image"), tr("请先设置基准图"));
            return;
        }
    } else {
        if (m_testUiMode == TestUiMode::Continuous) {
            const CameraFrameSnapshot snapshot =
                    CameraFrameProvider::instance().currentFrameSnapshot();
            frame = snapshot.frame;
            metadata = snapshot.metadata;
            title = tr("测试图像");
        } else {
            frame = m_liveTestFrameSnapshot;
            metadata = m_liveTestFrameMetadata;
            title = tr("单次测试快照");
        }
        if (frame.empty()) {
            displayError(QStringLiteral("image_empty"), tr("当前图像为空"));
            return;
        }
    }

    showFrameImage(frame, title);
    runComparisonOnFrame(frame, metadata, title, referenceSource);
}

void ColorComparisonDialog::applyDetectRoiEditState()
{
    if (m_detectRegionType == QStringLiteral("circle"))
        setEditState(EditState::DetectCircle);
    else if (m_detectRegionType == QStringLiteral("rectangle"))
        setEditState(EditState::DetectRect);
    else
        setEditState(EditState::None);
}

void ColorComparisonDialog::exitTestMode()
{
    stopContinuousRun();
    m_liveTestSource = LiveTestSource::None;
    m_testUiMode = TestUiMode::Edit;
    setEditState(EditState::None);
    updateBottomButtons();
    showPreviewImage();
    refreshRoiOverlay();
    if (m_featureView) {
        m_featureView->clearDetection();
        m_featureView->closeZoom();
    }
    updateStatus(tr("已退出测试"));
}

void ColorComparisonDialog::updateBottomButtons()
{
    if (!m_referenceTestButton || !m_testRunButton || !m_finishButton || !m_exitTestButton)
        return;

    const bool testMode = m_testUiMode != TestUiMode::Edit;
    m_referenceTestButton->setVisible(!testMode);
    m_exitTestButton->setVisible(testMode);
    m_finishButton->setText(testMode ? tr("运行一次") : tr("完成"));
    m_testRunButton->setText(m_testUiMode == TestUiMode::Continuous ? tr("停止运行") :
                             testMode ? tr("连续运行") : tr("测试运行"));
    m_testRunButton->setProperty("running", m_testUiMode == TestUiMode::Continuous);
    refreshButtonStyle(m_testRunButton);
}

void ColorComparisonDialog::runComparisonOnFrame(const cv::Mat &frame,
                                                 const FrameInputMetadata &metadata,
                                                 const QString &imageTitle,
                                                 bool referenceSource)
{
    if (blockInvalidConfigAction())
        return;

    if (frame.empty()) {
        displayError(QStringLiteral("image_empty"), tr("当前图像为空"));
        return;
    }

    const ToolRequest request = makeTestRequest(frame, metadata);
    if (!imageTitle.isEmpty() && m_viewerTitleLabel)
        m_viewerTitleLabel->setText(imageTitle);
    if (!referenceSource) {
        m_liveTestFrameSnapshot = frame.clone();
        m_liveTestFrameMetadata = metadata;
    }
    queueTestRequest(request, imageTitle, referenceSource);
}

ToolRequest ColorComparisonDialog::makeTestRequest(
        const cv::Mat &frame, const FrameInputMetadata &metadata) const
{
    ToolRequest request;
    request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.config = toToolConfig();
    request.image = frame.clone();
    request.runtimeContext.insert(QStringLiteral("input"), metadata.toJson());

    const ReferenceFrameSnapshot reference =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    request.referenceImage = reference.frame.clone();
    request.runtimeContext.insert(QStringLiteral("referenceInput"),
                                  reference.metadata.toJson());
    return request;
}

void ColorComparisonDialog::queueTestRequest(const ToolRequest &request,
                                             const QString &imageTitle,
                                             bool referenceSource)
{
    // 每个请求拥有独立代次。新帧进入 pending 时会立即使活动帧结果过期，
    // 防止旧 score/overlay 被应用到已经显示的新帧。
    const quint64 generation = ++m_testGeneration;
    if (m_testWatcher && m_testWatcher->isRunning()) {
        m_pendingTestRequest = request;
        m_pendingTestRequest.image = request.image.clone();
        m_pendingTestRequest.referenceImage = request.referenceImage.clone();
        m_pendingImageTitle = imageTitle;
        m_pendingReferenceSource = referenceSource;
        m_pendingTestGeneration = generation;
        m_pendingContinuousRun = true;
        return;
    }
    launchTestRequest(request, imageTitle, referenceSource, generation);
}

void ColorComparisonDialog::launchTestRequest(const ToolRequest &inputRequest,
                                              const QString &imageTitle,
                                              bool referenceSource,
                                              quint64 generation)
{
    if (!m_testWatcher)
        return;

    ToolRequest request = inputRequest;
    request.image = inputRequest.image.clone();
    request.referenceImage = inputRequest.referenceImage.clone();
    m_activeTestGeneration = generation;
    m_activeImageTitle = imageTitle;
    m_activeReferenceSource = referenceSource;
    updateStatus(tr("运行中…"));
    m_testWatcher->setFuture(QtConcurrent::run([request]() {
        ColorComparisonAdapter adapter;
        return adapter.run(request);
    }));
}

void ColorComparisonDialog::handleTestFinished()
{
    if (!m_testWatcher)
        return;

    const ToolResult result = m_testWatcher->result();
    const quint64 finishedGeneration = m_activeTestGeneration;
    const bool finishedReferenceSource = m_activeReferenceSource;
    if (finishedGeneration == m_testGeneration)
        displayResult(result, finishedReferenceSource);

    if (!m_pendingContinuousRun)
        return;

    ToolRequest pending = m_pendingTestRequest;
    pending.image = m_pendingTestRequest.image.clone();
    pending.referenceImage = m_pendingTestRequest.referenceImage.clone();
    const QString pendingTitle = m_pendingImageTitle;
    const bool pendingReference = m_pendingReferenceSource;
    const quint64 pendingGeneration = m_pendingTestGeneration;
    m_pendingContinuousRun = false;
    m_pendingTestRequest = ToolRequest();
    m_pendingImageTitle.clear();
    m_pendingReferenceSource = false;
    m_pendingTestGeneration = 0;

    if (pendingGeneration == m_testGeneration)
        launchTestRequest(pending, pendingTitle, pendingReference,
                          pendingGeneration);
}

void ColorComparisonDialog::displayResult(const ToolResult &result, bool referenceSource)
{
    if (m_previewHelper)
        m_previewHelper->setToolOverlays(result.overlays);
    const bool featureAvailable = updateDetectionFeaturePreview(result);
    const QJsonObject brightness = result.payload.value(
                QStringLiteral("brightnessCompensation")).toObject();
    const QString brightnessNotice = brightness.value(
                QStringLiteral("fallback")).toBool()
            ? tr(" | 光照补偿已跳过：%1").arg(
                  brightness.value(QStringLiteral("fallbackReason")).toString())
            : QString();
    updateStatus(tr("%1 | score:%2 | %3 | %4%5%6")
                 .arg(result.ok ? QStringLiteral("OK") : QStringLiteral("NG"),
                      QString::number(result.score, 'f', 2),
                      result.status,
                      result.message,
                      featureAvailable ? QString()
                                       : tr(" | 检测特征不可用"),
                      brightnessNotice));
    if (referenceSource) {
        m_referencePreviewSnapshot =
                makeReferenceToolPreviewSnapshot(toToolConfig(), result, m_detectRoi);
    }
}

void ColorComparisonDialog::displayError(const QString &status, const QString &message)
{
    ToolResult result;
    result.toolType = ToolType::ColorComparison;
    result.status = status;
    result.message = message;
    displayResult(result, false);
}

void ColorComparisonDialog::displayStoredModelInstruction()
{
    QString status = m_modelStatus;
    if (status.isEmpty()) {
        status = QStringLiteral("model_%1")
                .arg(colorComparisonModelStateName(m_model.state));
    }
    QString message = m_modelReason;
    if (message.isEmpty())
        message = tr("模型未就绪，请重新取样");
    displayError(status, message);
}

bool ColorComparisonDialog::rebuildTemplateModelFromFrame(
        const cv::Mat &frame,
        const FrameInputMetadata &metadata,
        QString *status,
        QString *message)
{
    if (m_invalidConfigReadOnly) {
        if (status)
            *status = m_invalidConfigStatus;
        if (message)
            *message = m_invalidConfigMessage;
        return false;
    }

    if (!m_modelBuildWatcher || m_modelBuildWatcher->isRunning()) {
        if (status)
            *status = QStringLiteral("model_build_busy");
        if (message)
            *message = tr("颜色比较模型正在取样");
        return false;
    }
    if (frame.empty()) {
        if (status)
            *status = QStringLiteral("image_empty");
        if (message)
            *message = tr("Reference image is empty.");
        return false;
    }

    ToolRequest request;
    request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.config = toToolConfig();
    if (m_model.state == ColorComparisonModelState::Unsupported
            && m_modelStatus == QStringLiteral("model_rebuild_required")) {
        ColorComparisonModelV2 buildableModel = m_model;
        buildableModel.state = ColorComparisonModelState::Stale;
        QJsonObject params = request.config.params;
        QJsonObject colorComparison =
                params.value(QStringLiteral("colorComparison")).toObject();
        colorComparison.insert(QStringLiteral("model"),
                               colorComparisonModelToJson(buildableModel));
        QJsonObject lifecycle = colorComparison.value(
                    QStringLiteral("dialogLifecycle")).toObject();
        lifecycle.insert(QStringLiteral("status"),
                         QStringLiteral("model_stale"));
        colorComparison.insert(QStringLiteral("dialogLifecycle"), lifecycle);
        params.insert(QStringLiteral("colorComparison"), colorComparison);
        request.config.params = params;

        const ColorComparisonModelReadResult migrationRead =
                readColorComparisonModel(colorComparison, true);
        if (migrationRead.status != QStringLiteral("model_stale")) {
            if (status)
                *status = QStringLiteral("model_rebuild_request_invalid");
            if (message)
                *message = tr("旧模型重建请求构造失败");
            return false;
        }
    }
    request.image = frame.clone();
    request.referenceImage = frame.clone();
    request.runtimeContext.insert(QStringLiteral("input"),
                                  metadata.toJson());
    request.runtimeContext.insert(QStringLiteral("referenceInput"),
                                  metadata.toJson());

    invalidateAsyncWork();
    m_activeModelBuildGeneration = ++m_modelBuildGeneration;
    m_modelBuildUiActive = true;
    m_rebuildModelButton->setEnabled(false);
    updateStatus(tr("正在重新取样…"));
    m_modelBuildWatcher->setFuture(QtConcurrent::run([request]() {
        ColorComparisonAdapter adapter;
        return adapter.buildTemplateModel(request);
    }));
    return true;
}

void ColorComparisonDialog::startExplicitModelRebuild()
{
    if (blockInvalidConfigAction())
        return;

    if (!m_modelBuildWatcher || m_modelBuildWatcher->isRunning())
        return;

    stopContinuousRun();
    m_testUiMode = TestUiMode::Edit;
    m_liveTestSource = LiveTestSource::None;
    updateBottomButtons();

    if ((m_model.state == ColorComparisonModelState::Unsupported
         && m_modelStatus != QStringLiteral("model_rebuild_required"))
            || m_model.state == ColorComparisonModelState::Invalid) {
        displayStoredModelInstruction();
        return;
    }

    const ReferenceFrameSnapshot snapshot =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    QString status;
    QString message;
    if (!rebuildTemplateModelFromFrame(snapshot.frame,
                                       snapshot.metadata,
                                       &status,
                                       &message)) {
        displayError(status, message);
    }
}

void ColorComparisonDialog::handleModelBuildFinished()
{
    if (!m_modelBuildWatcher)
        return;
    if (m_rebuildModelButton)
        m_rebuildModelButton->setEnabled(!m_invalidConfigReadOnly);

    const ColorComparisonTemplateBuildResult result =
            m_modelBuildWatcher->result();
    if (m_activeModelBuildGeneration != m_modelBuildGeneration)
        return;
    m_modelBuildUiActive = false;

    if (!result.success) {
        displayError(result.status, result.message);
        return;
    }

    invalidateAsyncWork();
    m_model = result.model;
    m_modelOriginVersion = 2;
    m_modelStatus = result.status.isEmpty()
            ? QStringLiteral("ok") : result.status;
    m_modelReason = tr("取样完成");
    updateModelStateUi();

    ToolResult display;
    display.toolType = ToolType::ColorComparison;
    display.success = true;
    display.ok = true;
    display.status = m_modelStatus;
    display.message = m_modelReason;
    displayResult(display, false);
}

void ColorComparisonDialog::finishConfiguration()
{
    if (blockInvalidConfigAction())
        return;

    if (m_testUiMode != TestUiMode::Edit) {
        runSingleShotTest();
        return;
    }

    setEditState(EditState::None);
    accept();
}

void ColorComparisonDialog::accept()
{
    if (blockInvalidConfigAction())
        return;

    if (m_continuousTimer)
        m_continuousTimer->stop();
    if (m_featureView)
        m_featureView->closeZoom();
    invalidateAsyncWork();
    invalidateModelBuild();
    QDialog::accept();
}

void ColorComparisonDialog::reject()
{
    if (m_continuousTimer)
        m_continuousTimer->stop();
    if (m_featureView)
        m_featureView->closeZoom();
    invalidateAsyncWork();
    invalidateModelBuild();
    QDialog::reject();
}

void ColorComparisonDialog::closeEvent(QCloseEvent *event)
{
    if (m_continuousTimer)
        m_continuousTimer->stop();
    if (m_featureView)
        m_featureView->closeZoom();
    invalidateAsyncWork();
    invalidateModelBuild();
    QDialog::closeEvent(event);
}
