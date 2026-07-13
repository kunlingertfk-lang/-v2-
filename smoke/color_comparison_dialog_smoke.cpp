#include "ColorComparisonDialog.h"

#include "PlanDialogUtils.h"
#include "algorithms/recognition/ColorComparisonModel.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QGraphicsPolygonItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSemaphore>
#include <QSpinBox>
#include <QThread>
#include <QToolButton>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>

#include <opencv2/core.hpp>

namespace {

int g_failures = 0;

void check(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << "FAIL: " << message << std::endl;
    ++g_failures;
}

void check(bool condition, const QString &message)
{
    if (condition)
        return;
    std::cerr << "FAIL: " << message.toStdString() << std::endl;
    ++g_failures;
}

bool waitUntil(const std::function<bool()> &predicate, int timeoutMs = 4000)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (predicate())
            return true;
        QThread::msleep(5);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    return predicate();
}

QImage labelImage(const QLabel *label)
{
    if (!label)
        return QImage();
    return label->pixmap(Qt::ReturnByValue).toImage()
            .convertToFormat(QImage::Format_RGB32);
}

bool containsRedMask(const QImage &image)
{
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qRed(line[x]) > 180 && qGreen(line[x]) < 90
                    && qBlue(line[x]) < 90) {
                return true;
            }
        }
    }
    return false;
}

QRect nonWhiteContentBounds(const QImage &image)
{
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qRed(line[x]) < 245 || qGreen(line[x]) < 245
                    || qBlue(line[x]) < 245) {
                bounds = bounds.isNull() ? QRect(x, y, 1, 1)
                                         : bounds.united(QRect(x, y, 1, 1));
            }
        }
    }
    return bounds;
}

QJsonObject rectJson(double x, double y, double width, double height)
{
    return {
        {QStringLiteral("x"), x},
        {QStringLiteral("y"), y},
        {QStringLiteral("width"), width},
        {QStringLiteral("height"), height}
    };
}

QJsonObject pointJson(double x, double y)
{
    return {{QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
}

QVector<QPointF> polygonPoints()
{
    return {
        QPointF(0.12, 0.12), QPointF(0.24, 0.12),
        QPointF(0.24, 0.24), QPointF(0.12, 0.24)
    };
}

QVector<QPointF> detectMaskPoints()
{
    return {
        QPointF(0.36, 0.26), QPointF(0.47, 0.26),
        QPointF(0.47, 0.37), QPointF(0.36, 0.37)
    };
}

QJsonArray polygonJson()
{
    QJsonArray points;
    for (const QPointF &point : polygonPoints())
        points.append(pointJson(point.x(), point.y()));
    return points;
}

QJsonArray detectMaskJson()
{
    QJsonArray points;
    for (const QPointF &point : detectMaskPoints())
        points.append(pointJson(point.x(), point.y()));
    return points;
}

ColorComparisonModelV2 readyModel(const QString &suffix = QStringLiteral("base"))
{
    ColorComparisonModelV2 model;
    model.state = ColorComparisonModelState::Ready;
    model.values = QVector<double>(1024, 0.0);
    model.values[1 * 32 + 2] = 0.10;
    model.values[7 * 32 + 11] = 0.20;
    model.values[18 * 32 + 23] = 0.30;
    model.values[29 * 32 + 30] = 0.40;
    model.valueHistogram = QVector<double>(32, 0.0);
    model.valueHistogram[2] = 0.10;
    model.valueHistogram[9] = 0.20;
    model.valueHistogram[20] = 0.30;
    model.valueHistogram[30] = 0.40;
    model.effectivePixelCount = 4096;
    model.referenceImageHash = QStringLiteral("reference-hash-%1").arg(suffix);
    model.extractParamsHash = QStringLiteral("extract-hash-%1").arg(suffix);
    model.inputSignature.colorMode = QStringLiteral("color");
    model.inputSignature.pixelFormat = QStringLiteral("BGR8");
    model.inputSignature.bitDepth = 8;
    model.brightnessReference.mean = 128.0;
    model.brightnessReference.deviation = 12.0;
    return model;
}

ColorComparisonModelV2 staleModel(const QString &suffix)
{
    ColorComparisonModelV2 model = readyModel(suffix);
    model.state = ColorComparisonModelState::Stale;
    return model;
}

ColorComparisonTemplateBuildResult successfulBuildResult(const QString &suffix)
{
    ColorComparisonTemplateBuildResult result;
    result.success = true;
    result.status = QStringLiteral("ok");
    result.model = readyModel(suffix);
    return result;
}

ToolResult controlledOldTestResult(const QString &status,
                                   const QString &overlayLabel)
{
    ToolResult result;
    result.toolType = ToolType::ColorComparison;
    result.success = true;
    result.ok = true;
    result.status = status;
    result.message = QStringLiteral("controlled old-model test completion");
    result.score = 17.0;
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.rect = QRectF(8.0, 7.0, 24.0, 18.0);
    overlay.label = overlayLabel;
    result.overlays.append(overlay);
    return result;
}

ToolConfig v2Config(const QString &templateMode,
                    const ColorComparisonModelV2 &model = readyModel())
{
    ColorComparisonModelV2 serializedModel = model;
    if (serializedModel.state == ColorComparisonModelState::Ready) {
        const cv::Mat currentReference =
                ReferenceImageProvider::instance().referenceFrame();
        if (!currentReference.empty()) {
            serializedModel.referenceImageHash =
                    colorComparisonReferenceHash(currentReference);
        }
    }

    ToolConfig config;
    config.toolId = QStringLiteral("color-comparison-dialog-smoke");
    config.toolName = QStringLiteral("ColorComparison");
    config.displayName = QStringLiteral("颜色比较");
    config.toolType = ToolType::ColorComparison;
    config.category = ToolCategory::Recognition;
    config.enabled = true;
    config.roiNormalized = QRectF(0.30, 0.20, 0.40, 0.35);

    QJsonObject colorComparison;
    colorComparison.insert(QStringLiteral("version"), 2);
    colorComparison.insert(QStringLiteral("templateRegionMode"), templateMode);
    colorComparison.insert(QStringLiteral("templateRoiNormalized"),
                           rectJson(0.05, 0.06, 0.30, 0.28));
    colorComparison.insert(QStringLiteral("templateMaskPolygon"), QJsonArray());
    colorComparison.insert(QStringLiteral("detectRegionType"),
                           QStringLiteral("rectangle"));
    colorComparison.insert(QStringLiteral("detectRoiNormalized"),
                           rectJson(0.30, 0.20, 0.40, 0.35));
    colorComparison.insert(
            QStringLiteral("detectCircleNormalized"),
            QJsonObject{{QStringLiteral("center"), pointJson(0.50, 0.50)},
                        {QStringLiteral("radius"), 0.20},
                        {QStringLiteral("boundingRect"),
                         rectJson(0.30, 0.30, 0.40, 0.40)},
                        {QStringLiteral("valid"), true}});
    colorComparison.insert(QStringLiteral("detectMaskPolygon"), QJsonArray());
    colorComparison.insert(QStringLiteral("model"),
                           colorComparisonModelToJson(serializedModel));
    colorComparison.insert(
            QStringLiteral("comparison"),
            QJsonObject{{QStringLiteral("sensitivity"),
                         QStringLiteral("medium")},
                        {QStringLiteral("brightnessCompensation"), false}});
    colorComparison.insert(
            QStringLiteral("positionCorrection"),
            QJsonObject{{QStringLiteral("enabled"), false},
                        {QStringLiteral("sourceId"), QString()},
                        {QStringLiteral("interfaceVersion"), 1}});
    config.params.insert(QStringLiteral("colorComparison"), colorComparison);
    config.judgeRule = {
        {QStringLiteral("mode"), QStringLiteral("min_score")},
        {QStringLiteral("minScore"), 80}
    };
    return config;
}

ToolConfig legacyConfig(int version)
{
    ToolConfig config = v2Config(QStringLiteral("custom"));
    QJsonObject colorComparison;
    colorComparison.insert(QStringLiteral("version"), version);
    colorComparison.insert(QStringLiteral("templateRegionMode"),
                           QStringLiteral("custom"));
    colorComparison.insert(QStringLiteral("templateRoiNormalized"),
                           rectJson(0.05, 0.06, 0.30, 0.28));
    colorComparison.insert(QStringLiteral("templateFeature"),
                           QJsonArray{1.0, 0.0});
    colorComparison.insert(QStringLiteral("detectRegionType"),
                           QStringLiteral("rectangle"));
    colorComparison.insert(QStringLiteral("detectRoiNormalized"),
                           rectJson(0.30, 0.20, 0.40, 0.35));
    colorComparison.insert(QStringLiteral("detectMaskPolygon"), QJsonArray());
    colorComparison.insert(QStringLiteral("featureType"),
                           QStringLiteral("histogram"));
    colorComparison.insert(QStringLiteral("sensitivity"),
                           QStringLiteral("medium"));
    config.params.insert(QStringLiteral("colorComparison"), colorComparison);
    return config;
}

QJsonObject colorParams(const ColorComparisonDialog &dialog)
{
    return dialog.toolConfig().params
            .value(QStringLiteral("colorComparison")).toObject();
}

QJsonObject modelJson(const ColorComparisonDialog &dialog)
{
    return colorParams(dialog).value(QStringLiteral("model")).toObject();
}

QJsonObject dialogLifecycle(const ColorComparisonDialog &dialog)
{
    return colorParams(dialog).value(QStringLiteral("dialogLifecycle")).toObject();
}

QString modelState(const ColorComparisonDialog &dialog)
{
    return modelJson(dialog).value(QStringLiteral("state")).toString();
}

QString modelReferenceHash(const ColorComparisonDialog &dialog)
{
    return modelJson(dialog).value(QStringLiteral("referenceImageHash")).toString();
}

QString modelExtractHash(const ColorComparisonDialog &dialog)
{
    return modelJson(dialog).value(QStringLiteral("extractParamsHash")).toString();
}

template <typename T>
T *requiredChild(ColorComparisonDialog &dialog,
                 const QString &objectName,
                 const char *message)
{
    T *child = dialog.findChild<T *>(objectName);
    check(child != nullptr, message);
    return child;
}

void loadReady(ColorComparisonDialog &dialog,
               const QString &mode,
               const QString &suffix)
{
    dialog.loadFromConfig(v2Config(mode, readyModel(suffix)));
    check(modelState(dialog) == QStringLiteral("ready"),
          "ready V2 configuration must load as ready");
}

void checkHashesPreserved(const ColorComparisonDialog &dialog,
                          const QString &referenceHash,
                          const QString &extractHash,
                          const char *message)
{
    check(modelReferenceHash(dialog) == referenceHash
                  && modelExtractHash(dialog) == extractHash,
          message);
}

bool hasPixmap(const QLabel *label)
{
    return label && !label->pixmap(Qt::ReturnByValue).isNull();
}

QFutureWatcher<ColorComparisonTemplateBuildResult> *templateBuildWatcher(
        ColorComparisonDialog &dialog)
{
    for (QObject *child : dialog.children()) {
        if (auto *watcher = dynamic_cast<
                QFutureWatcher<ColorComparisonTemplateBuildResult> *>(child)) {
            return watcher;
        }
    }
    return nullptr;
}

QFutureWatcher<ToolResult> *testResultWatcher(ColorComparisonDialog &dialog)
{
    for (QObject *child : dialog.children()) {
        if (auto *watcher = dynamic_cast<QFutureWatcher<ToolResult> *>(child))
            return watcher;
    }
    return nullptr;
}

bool sceneHasToolTip(ColorComparisonDialog &dialog, const QString &toolTip)
{
    QGraphicsView *view = dialog.findChild<QGraphicsView *>();
    if (!view || !view->scene())
        return false;
    for (QGraphicsItem *item : view->scene()->items()) {
        if (item && item->isVisible() && item->toolTip() == toolTip)
            return true;
    }
    return false;
}

} // namespace

namespace PlanDialogUtils {

void applyLargeWindow(QWidget *)
{
}

} // namespace PlanDialogUtils

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    const cv::Mat colorFrame(96, 128, CV_8UC3, cv::Scalar(32, 96, 192));
    const FrameInputMetadata colorMetadata =
            FrameInputMetadata::fromMat(colorFrame, QStringLiteral("reference"));
    ReferenceImageProvider::instance().setReferenceFrame(colorFrame, colorMetadata);
    CameraFrameProvider::instance().setCurrentFrame(
            colorFrame,
            FrameInputMetadata::fromMat(colorFrame, QStringLiteral("camera")));

    {
        ColorComparisonDialog dialog;
        QPushButton *basic = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonBasicButton"),
                "Basic button must have a stable object name");
        QPushButton *all = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonAllButton"),
                "All button must have a stable object name");
        QComboBox *feature = requiredChild<QComboBox>(
                dialog, QStringLiteral("colorComparisonFeatureTypeCombo"),
                "feature type combo must have a stable object name");
        QCheckBox *brightness = requiredChild<QCheckBox>(
                dialog, QStringLiteral("colorComparisonBrightnessCompensation"),
                "brightness compensation must have a stable object name");
        QWidget *position = requiredChild<QWidget>(
                dialog, QStringLiteral("colorComparisonPositionCorrectionPanel"),
                "position correction panel must have a stable object name");
        QPushButton *rebuild = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonRebuildModelButton"),
                "explicit model sampling button must exist");
        QLabel *stateLabel = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonModelStateLabel"),
                "model state label must exist");
        QLabel *hue = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonHueHistogram"),
                "Hue histogram label must exist");
        QLabel *saturation = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonSaturationHistogram"),
                "Saturation histogram label must exist");
        QLabel *value = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonValueHistogram"),
                "Value histogram label must exist");
        QComboBox *sensitivity = requiredChild<QComboBox>(
                dialog, QStringLiteral("colorComparisonSensitivityCombo"),
                "sensitivity combo must have a stable object name");
        QSpinBox *minScore = requiredChild<QSpinBox>(
                dialog, QStringLiteral("colorComparisonMinScore"),
                "minimum score must have a stable object name");
        QPushButton *testRun = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonTestRunButton"),
                "test run button must have a stable object name");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "status label must have a stable object name");

        check(basic && all && feature && brightness && position && rebuild
                      && stateLabel && hue && saturation && value && sensitivity
                      && minScore && testRun && status,
              "all V2 controls must be discoverable");
        if (position)
            check(!position->isEnabled(),
                  "position correction UI must be disabled");
        if (feature && feature->count() > 1) {
            check(!(feature->model()->flags(feature->model()->index(1, 0))
                    & Qt::ItemIsEnabled),
                  "spectrum feature must be disabled");
        }
        if (brightness)
            check(brightness->text().contains(QStringLiteral("光照补偿")),
                  "brightness option must use the V2 compensation wording");
        if (sensitivity) {
            check(sensitivity->itemText(0).contains(QStringLiteral("严格"))
                          && sensitivity->itemText(1).contains(QStringLiteral("标准"))
                          && sensitivity->itemText(2).contains(QStringLiteral("宽松")),
                  "sensitivity wording must expose strict/standard/wide semantics");
        }

        const ToolConfig before = dialog.toolConfig();
        if (all)
            all->click();
        if (basic)
            basic->click();
        const ToolConfig after = dialog.toolConfig();
        check(before.params == after.params
                      && before.judgeRule == after.judgeRule,
              "Basic/All switching must be pure visibility");
        check(!before.toolId.isEmpty() && before.toolId == after.toolId
                      && after.toolId == dialog.toolConfig().toolId,
              "new Dialog toolId must remain stable across reads and view switching");
        check(colorParams(dialog).value(QStringLiteral("version")).toInt() == 2,
              "new Dialog configurations must save nested version 2");
        check(modelState(dialog) == QStringLiteral("empty"),
              "new Dialog model must start empty");

        QToolButton *global = requiredChild<QToolButton>(
                dialog, QStringLiteral("colorComparisonDetectGlobalButton"),
                "global detection button must have a stable object name");
        if (global)
            global->click();
        const QJsonObject globalParams = colorParams(dialog);
        const QJsonObject globalRoi = globalParams
                .value(QStringLiteral("detectRoiNormalized")).toObject();
        check(globalParams.value(QStringLiteral("detectRegionType")).toString()
                      == QStringLiteral("rectangle")
                      && globalRoi.value(QStringLiteral("x")).toDouble() == 0.0
                      && globalRoi.value(QStringLiteral("y")).toDouble() == 0.0
                      && globalRoi.value(QStringLiteral("width")).toDouble() == 1.0
                      && globalRoi.value(QStringLiteral("height")).toDouble() == 1.0,
              "global detection must serialize as Adapter-supported rectangle/full ROI");
        const ToolConfig savedGlobal = dialog.toolConfig();
        ColorComparisonDialog reopenedGlobal;
        reopenedGlobal.loadFromConfig(savedGlobal);
        QToolButton *reopenedGlobalButton = requiredChild<QToolButton>(
                reopenedGlobal, QStringLiteral("colorComparisonDetectGlobalButton"),
                "global detection must be restorable after save/reopen");
        check(globalParams.value(QStringLiteral("detectGlobal")).toBool()
                      && reopenedGlobalButton && reopenedGlobalButton->isChecked(),
              "global detection must survive save/reopen without changing to rectangle mode");
    }

    {
        ColorComparisonDialog dialog;
        loadReady(dialog, QStringLiteral("custom"), QStringLiteral("histograms"));
        QLabel *hue = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonHueHistogram"),
                "ready model Hue histogram must exist");
        QLabel *saturation = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonSaturationHistogram"),
                "ready model Saturation histogram must exist");
        QLabel *value = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonValueHistogram"),
                "ready model Value histogram must exist");
        check(hasPixmap(hue) && hasPixmap(saturation) && hasPixmap(value),
              "ready V2 model must render real H/S/V histogram pixmaps");
    }

    {
        const cv::Mat whiteReference(96, 128, CV_8UC3,
                                     cv::Scalar(255, 255, 255));
        ReferenceImageProvider::instance().setReferenceFrame(
                whiteReference,
                FrameInputMetadata::fromMat(whiteReference,
                                            QStringLiteral("reference")));
        ToolConfig masked = v2Config(QStringLiteral("custom"),
                                     readyModel(QStringLiteral("mask-preview")));
        QJsonObject root = masked.params;
        QJsonObject params = root.value(QStringLiteral("colorComparison")).toObject();
        params.insert(QStringLiteral("templateMaskPolygon"), polygonJson());
        root.insert(QStringLiteral("colorComparison"), params);
        masked.params = root;

        ColorComparisonDialog dialog;
        dialog.loadFromConfig(masked);
        QLabel *templatePreview = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonTemplatePreview"),
                "template preview must have a stable object name");
        check(containsRedMask(labelImage(templatePreview)),
              "template Mask must be drawn only in the independent reference preview");

        ToolConfig syncMasked = v2Config(QStringLiteral("sync"),
                                         readyModel(QStringLiteral("sync-mask-preview")));
        root = syncMasked.params;
        params = root.value(QStringLiteral("colorComparison")).toObject();
        params.insert(QStringLiteral("detectMaskPolygon"), detectMaskJson());
        root.insert(QStringLiteral("colorComparison"), params);
        syncMasked.params = root;
        dialog.loadFromConfig(syncMasked);
        check(containsRedMask(labelImage(templatePreview)),
              "sync detection Mask must be drawn in the independent reference preview");
        ReferenceImageProvider::instance().setReferenceFrame(colorFrame,
                                                              colorMetadata);
    }

    {
        ColorComparisonDialog dialog;
        loadReady(dialog, QStringLiteral("custom"), QStringLiteral("custom-edits"));
        const QString referenceHash = modelReferenceHash(dialog);
        const QString extractHash = modelExtractHash(dialog);
        FrameViewHelper *preview = dialog.findChild<FrameViewHelper *>();
        QToolButton *rectButton = requiredChild<QToolButton>(
                dialog, QStringLiteral("colorComparisonDetectRectButton"),
                "detection rectangle button must have a stable object name");
        QToolButton *circleButton = requiredChild<QToolButton>(
                dialog, QStringLiteral("colorComparisonDetectCircleButton"),
                "detection circle button must have a stable object name");
        QPushButton *maskEdit = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonDetectMaskEditButton"),
                "detection mask edit button must have a stable object name");
        QSpinBox *minScore = requiredChild<QSpinBox>(
                dialog, QStringLiteral("colorComparisonMinScore"),
                "minimum score control must exist");
        QComboBox *sensitivity = requiredChild<QComboBox>(
                dialog, QStringLiteral("colorComparisonSensitivityCombo"),
                "sensitivity control must exist");
        check(preview != nullptr, "Dialog preview helper must exist");

        if (rectButton)
            rectButton->click();
        if (preview)
            preview->roiChanged(QRectF(0.25, 0.25, 0.35, 0.30));
        check(modelState(dialog) == QStringLiteral("ready"),
              "custom detection rectangle edits must not stale the model");

        if (circleButton)
            circleButton->click();
        CircleRoi circle;
        circle.centerNormalized = QPointF(0.52, 0.48);
        circle.radiusNormalized = 0.18;
        circle.boundingRectNormalized = QRectF(0.34, 0.30, 0.36, 0.36);
        circle.valid = true;
        if (preview)
            preview->circleChanged(circle);
        check(modelState(dialog) == QStringLiteral("ready"),
              "custom detection circle edits must not stale the model");

        if (maskEdit)
            maskEdit->click();
        if (preview)
            preview->polygonChanged(detectMaskPoints());
        check(modelState(dialog) == QStringLiteral("ready"),
              "custom detection Mask edits must not stale the model");

        if (minScore)
            minScore->setValue(67);
        if (sensitivity)
            sensitivity->setCurrentIndex(0);
        check(modelState(dialog) == QStringLiteral("ready"),
              "minimum score and sensitivity must not stale the model");
        checkHashesPreserved(dialog, referenceHash, extractHash,
                             "non-staling edits must preserve both model hashes");
    }

    {
        ColorComparisonDialog dialog;
        FrameViewHelper *preview = dialog.findChild<FrameViewHelper *>();
        QToolButton *rectButton = requiredChild<QToolButton>(
                dialog, QStringLiteral("colorComparisonDetectRectButton"),
                "sync rectangle test needs the rectangle button");
        QToolButton *circleButton = requiredChild<QToolButton>(
                dialog, QStringLiteral("colorComparisonDetectCircleButton"),
                "sync circle test needs the circle button");
        QPushButton *maskEdit = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonDetectMaskEditButton"),
                "sync Mask test needs the Mask button");

        loadReady(dialog, QStringLiteral("sync"), QStringLiteral("sync-rect"));
        QLabel *templatePreview = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonTemplatePreview"),
                "sync live preview needs the template preview label");
        const QImage beforeSyncRect = labelImage(templatePreview);
        if (rectButton)
            rectButton->click();
        if (preview)
            preview->roiChanged(QRectF(0.18, 0.18, 0.44, 0.42));
        check(modelState(dialog) == QStringLiteral("stale"),
              "sync detection rectangle edits must stale the model");
        check(!beforeSyncRect.isNull()
                      && labelImage(templatePreview) != beforeSyncRect,
              "sync detection rectangle edits must refresh the template preview immediately");

        loadReady(dialog, QStringLiteral("sync"), QStringLiteral("sync-circle"));
        if (circleButton)
            circleButton->click();
        check(modelState(dialog) == QStringLiteral("stale"),
              "sync detection region type/circle edits must stale the model");

        loadReady(dialog, QStringLiteral("sync"), QStringLiteral("sync-mask"));
        const QImage beforeSyncMask = labelImage(templatePreview);
        if (maskEdit)
            maskEdit->click();
        if (preview)
            preview->polygonChanged(detectMaskPoints());
        check(modelState(dialog) == QStringLiteral("stale"),
              "sync detection Mask edits must stale the model");
        check(!beforeSyncMask.isNull()
                      && labelImage(templatePreview) != beforeSyncMask
                      && containsRedMask(labelImage(templatePreview)),
              "sync detection Mask edits must refresh the template preview immediately");
    }

    {
        ColorComparisonDialog dialog;
        FrameViewHelper *preview = dialog.findChild<FrameViewHelper *>();
        QPushButton *templateEdit = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonTemplateEditButton"),
                "template ROI edit button must have a stable object name");
        QPushButton *templateMaskEdit = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonTemplateMaskEditButton"),
                "template Mask edit button must have a stable object name");
        QComboBox *templateMode = requiredChild<QComboBox>(
                dialog, QStringLiteral("colorComparisonTemplateRegionModeCombo"),
                "template region mode combo must have a stable object name");
        QCheckBox *brightness = requiredChild<QCheckBox>(
                dialog, QStringLiteral("colorComparisonBrightnessCompensation"),
                "brightness compensation must exist");
        QLabel *hue = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonHueHistogram"),
                "Hue histogram must exist for stale clearing");

        loadReady(dialog, QStringLiteral("custom"), QStringLiteral("template-roi"));
        const QString roiReferenceHash = modelReferenceHash(dialog);
        const QString roiExtractHash = modelExtractHash(dialog);
        if (templateEdit)
            templateEdit->click();
        if (preview)
            preview->roiChanged(QRectF(0.08, 0.08, 0.36, 0.31));
        check(modelState(dialog) == QStringLiteral("stale"),
              "template ROI edits must stale the model");
        checkHashesPreserved(dialog, roiReferenceHash, roiExtractHash,
                             "markModelStale must preserve hashes after template ROI edits");
        check(!hasPixmap(hue),
              "stale models must clear histogram pixmaps");

        loadReady(dialog, QStringLiteral("custom"), QStringLiteral("template-mask"));
        QLabel *templatePreview = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonTemplatePreview"),
                "template live preview needs the template preview label");
        const QImage beforeTemplateMask = labelImage(templatePreview);
        if (templateMaskEdit)
            templateMaskEdit->click();
        if (preview)
            preview->polygonChanged(polygonPoints());
        check(modelState(dialog) == QStringLiteral("stale"),
              "template Mask edits must stale the model");
        check(!beforeTemplateMask.isNull()
                      && labelImage(templatePreview) != beforeTemplateMask
                      && containsRedMask(labelImage(templatePreview)),
              "template Mask edits must refresh the template preview immediately");

        loadReady(dialog, QStringLiteral("custom"), QStringLiteral("template-mode"));
        const QImage beforeTemplateMode = labelImage(templatePreview);
        if (templateMode) {
            const int syncIndex = templateMode->findData(QStringLiteral("sync"));
            templateMode->setCurrentIndex(syncIndex >= 0 ? syncIndex : 0);
        }
        check(modelState(dialog) == QStringLiteral("stale"),
              "template region mode changes must stale the model");
        check(!beforeTemplateMode.isNull()
                      && labelImage(templatePreview) != beforeTemplateMode,
              "template region mode changes must refresh the template preview immediately");

        loadReady(dialog, QStringLiteral("custom"), QStringLiteral("brightness"));
        if (brightness)
            brightness->setChecked(true);
        check(modelState(dialog) == QStringLiteral("stale"),
              "brightness compensation changes must stale the model");

        loadReady(dialog, QStringLiteral("custom"), QStringLiteral("reference"));
        const QString referenceHash = modelReferenceHash(dialog);
        const QString extractHash = modelExtractHash(dialog);
        const cv::Mat changedReference(96, 128, CV_8UC3,
                                       cv::Scalar(48, 112, 208));
        ReferenceImageProvider::instance().setReferenceFrame(
                changedReference,
                FrameInputMetadata::fromMat(changedReference,
                                            QStringLiteral("reference")));
        check(modelState(dialog) == QStringLiteral("stale"),
              "reference image changes must stale the model immediately");
        checkHashesPreserved(dialog, referenceHash, extractHash,
                             "reference staleness must preserve both model hashes");
        QLabel *stateLabel = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonModelStateLabel"),
                "state label must exist after reference change");
        check(stateLabel && stateLabel->text().contains(QStringLiteral("stale"))
                      && stateLabel->text().contains(QStringLiteral("重新取样")),
              "stale state UI must show the stored state and rebuild instruction");
    }

    ReferenceImageProvider::instance().setReferenceFrame(colorFrame, colorMetadata);
    {
        ColorComparisonDialog migrated;
        migrated.loadFromConfig(legacyConfig(1));
        const ToolConfig saved = migrated.toolConfig();
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(saved);
        check(colorParams(dialog).value(QStringLiteral("version")).toInt() == 2
                      && modelState(dialog) == QStringLiteral("stale")
                      && dialogLifecycle(dialog)
                         .value(QStringLiteral("originVersion")).toInt() == 1
                      && dialogLifecycle(dialog)
                         .value(QStringLiteral("status")).toString()
                         == QStringLiteral("model_stale"),
              "V1 with a reference must save back as stale V2");
        QLabel *stateLabel = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonModelStateLabel"),
                "legacy state label must exist");
        check(stateLabel && stateLabel->text().contains(QStringLiteral("旧模型"))
                      && stateLabel->text().contains(QStringLiteral("重新取样")),
              "V1 with reference must retain the old-model rebuild instruction after save/reopen");
    }

    ReferenceImageProvider::instance().clearReferenceFrame();
    {
        ColorComparisonDialog migrated;
        migrated.loadFromConfig(legacyConfig(1));
        const ToolConfig saved = migrated.toolConfig();
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(saved);
        check(modelState(dialog) == QStringLiteral("unsupported"),
              "V1 without a reference must remain unsupported after save/reopen");
        QLabel *stateLabel = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonModelStateLabel"),
                "unsupported legacy state label must exist");
        check(stateLabel && stateLabel->text().contains(
                          QStringLiteral("model_rebuild_required")),
              "V1 without reference must retain model_rebuild_required after save/reopen");
        check(dialogLifecycle(dialog)
                      .value(QStringLiteral("originVersion")).toInt() == 1,
              "V1 no-reference lifecycle metadata must retain originVersion 1");
        const QJsonObject beforeRebuild = modelJson(dialog);
        const cv::Mat monoReference(80, 120, CV_8UC1, cv::Scalar(112));
        ReferenceImageProvider::instance().setReferenceFrame(
                monoReference,
                FrameInputMetadata::fromMat(monoReference,
                                            QStringLiteral("reference")));
        check(modelState(dialog) == QStringLiteral("unsupported"),
              "adding a reference must not silently change unsupported V1 state");
        QPushButton *rebuild = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonRebuildModelButton"),
                "V1 no-reference recovery needs the explicit sampling button");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "V1 no-reference recovery needs the status label");
        if (rebuild)
            rebuild->click();
        check(status && status->text().contains(QStringLiteral("正在重新取样")),
              "V1 no-reference recovery must launch sampling asynchronously");
        check(waitUntil([status, rebuild]() {
                  return status && rebuild && rebuild->isEnabled()
                          && status->text().contains(
                                  QStringLiteral("unsupported_color_input"));
              }),
              "V1 without reference must become explicitly rebuildable once a reference arrives");
        check(modelJson(dialog) == beforeRebuild,
              "failed V1 recovery sampling must preserve the unsupported model exactly");
    }

    ReferenceImageProvider::instance().clearReferenceFrame();
    {
        ColorComparisonDialog migrated;
        migrated.loadFromConfig(legacyConfig(77));
        const ToolConfig saved = migrated.toolConfig();
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(saved);
        check(modelState(dialog) == QStringLiteral("unsupported"),
              "unknown model versions must remain unsupported after save/reopen");
        QLabel *stateLabel = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonModelStateLabel"),
                "unknown version state label must exist");
        check(stateLabel && stateLabel->text().contains(
                          QStringLiteral("unsupported_model_version")),
              "unknown versions must retain unsupported_model_version after save/reopen");
        check(dialogLifecycle(dialog)
                      .value(QStringLiteral("originVersion")).toInt() == 77,
              "unknown lifecycle metadata must retain its concrete origin version");
        ReferenceImageProvider::instance().setReferenceFrame(colorFrame,
                                                              colorMetadata);
        check(modelState(dialog) == QStringLiteral("unsupported"),
              "reference changes must not downgrade an unsupported version to stale");
        const QJsonObject beforeRebuild = modelJson(dialog);
        QPushButton *rebuild = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonRebuildModelButton"),
                "unknown version guard needs the explicit sampling button");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "unknown version guard needs the status label");
        if (rebuild)
            rebuild->click();
        check(waitUntil([status, rebuild]() {
                  return status && rebuild && rebuild->isEnabled()
                          && status->text().contains(
                                  QStringLiteral("unsupported_model_version"));
              }),
              "unknown-version sampling must preserve unsupported_model_version");
        check(modelJson(dialog) == beforeRebuild,
              "unknown-version sampling must preserve unsupported model content");
    }

    {
        QVector<QPair<QString, QJsonValue>> malformedVersions{
            {QStringLiteral("missing"), QJsonValue(QJsonValue::Undefined)},
            {QStringLiteral("string"), QJsonValue(QStringLiteral("2"))},
            {QStringLiteral("fractional"), QJsonValue(2.5)},
            {QStringLiteral("out-of-int-range"), QJsonValue(2147483648.0)}
        };
        for (const auto &entry : malformedVersions) {
            ToolConfig malformed = legacyConfig(77);
            QJsonObject root = malformed.params;
            QJsonObject params = root.value(QStringLiteral("colorComparison"))
                    .toObject();
            if (entry.second.isUndefined())
                params.remove(QStringLiteral("version"));
            else
                params.insert(QStringLiteral("version"), entry.second);
            root.insert(QStringLiteral("colorComparison"), params);
            malformed.params = root;

            ColorComparisonDialog dialog;
            dialog.loadFromConfig(malformed);
            QPushButton *finish = requiredChild<QPushButton>(
                    dialog, QStringLiteral("colorComparisonFinishButton"),
                    "malformed source version needs the Finish button");
            QLabel *status = requiredChild<QLabel>(
                    dialog, QStringLiteral("colorComparisonStatusLabel"),
                    "malformed source version needs the status label");
            check(finish && !finish->isEnabled()
                          && status
                          && status->text().contains(
                                 QStringLiteral("unsupported_model_version"))
                          && dialog.toolConfig().toJson() == malformed.toJson(),
                  QStringLiteral("%1 source version must remain exact read-only data instead of manufacturing originVersion=0")
                  .arg(entry.first));
        }

        QVector<QPair<QString, QJsonValue>> malformedEnvelopes{
            {QStringLiteral("missing"), QJsonValue(QJsonValue::Undefined)},
            {QStringLiteral("string"), QJsonValue(QStringLiteral("bad"))},
            {QStringLiteral("array"), QJsonValue(QJsonArray{1, 2})},
            {QStringLiteral("null"), QJsonValue(QJsonValue::Null)}
        };
        for (const auto &entry : malformedEnvelopes) {
            ToolConfig malformed = legacyConfig(77);
            QJsonObject params = malformed.params;
            if (entry.second.isUndefined())
                params.remove(QStringLiteral("colorComparison"));
            else
                params.insert(QStringLiteral("colorComparison"), entry.second);
            malformed.params = params;

            ColorComparisonDialog dialog;
            dialog.loadFromConfig(malformed);
            QPushButton *finish = requiredChild<QPushButton>(
                    dialog, QStringLiteral("colorComparisonFinishButton"),
                    "malformed color-comparison envelope needs the Finish button");
            check(finish && !finish->isEnabled()
                          && dialog.toolConfig().toJson() == malformed.toJson(),
                  QStringLiteral("%1 colorComparison envelope must remain exact read-only data")
                  .arg(entry.first));
        }
    }

    ReferenceImageProvider::instance().setReferenceFrame(colorFrame, colorMetadata);
    {
        ColorComparisonDialog dialog;
        loadReady(dialog, QStringLiteral("custom"),
                  QStringLiteral("reject-clipped-circle"));
        FrameViewHelper *preview = dialog.findChild<FrameViewHelper *>();
        QToolButton *circleButton = requiredChild<QToolButton>(
                dialog, QStringLiteral("colorComparisonDetectCircleButton"),
                "clipped circle test needs the circle button");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "clipped circle test needs the viewer status");
        if (circleButton)
            circleButton->click();

        CircleRoi clippedCircle;
        clippedCircle.centerNormalized = QPointF(10.0 / 128.0, 0.5);
        clippedCircle.radiusNormalized = 40.0 / 128.0;
        clippedCircle.boundingRectNormalized =
                QRectF(0.0, 8.0 / 96.0, 50.0 / 128.0, 80.0 / 96.0);
        clippedCircle.valid = true;
        if (preview)
            preview->circleChanged(clippedCircle);

        const ToolConfig saved = dialog.toolConfig();
        const QJsonObject savedCircle = saved.params
                .value(QStringLiteral("colorComparison")).toObject()
                .value(QStringLiteral("detectCircleNormalized")).toObject();
        check(std::abs(savedCircle.value(QStringLiteral("radius")).toDouble()
                       - 0.20) < 1e-9,
              "Dialog must reject an image-clipped circle instead of saving inconsistent geometry");
        check(status && status->text().contains(QStringLiteral("超出")),
              "Dialog must explain that a rejected circle exceeds the image boundary");

        ColorComparisonDialog reopened;
        reopened.loadFromConfig(saved);
        QPushButton *finish = requiredChild<QPushButton>(
                reopened, QStringLiteral("colorComparisonFinishButton"),
                "reopened clipped-circle result needs the Finish button");
        check(modelState(reopened) == QStringLiteral("ready")
                      && finish && finish->isEnabled(),
              "a rejected clipped circle must not create a V2 config that becomes invalid on reopen");
    }

    {
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(v2Config(QStringLiteral("custom"),
                                       readyModel(QStringLiteral("round-trip"))));
        const ToolConfig saved = dialog.toolConfig();
        ColorComparisonDialog reopened;
        reopened.loadFromConfig(saved);
        const ToolConfig savedAgain = reopened.toolConfig();
        check(saved.params == savedAgain.params
                      && saved.judgeRule == savedAgain.judgeRule,
              "ready nested V2 fields must survive save/reopen round-trip");
    }

    {
        const double radius = 0.10;
        const double shortAxisRadius = radius * 1920.0 / 515.0;
        ToolConfig aspectCircle = v2Config(
                    QStringLiteral("sync"),
                    readyModel(QStringLiteral("aspect-circle")));
        aspectCircle.roiNormalized = QRectF(0.40,
                                             0.50 - shortAxisRadius,
                                             0.20,
                                             shortAxisRadius * 2.0);
        QJsonObject root = aspectCircle.params;
        QJsonObject params = root.value(QStringLiteral("colorComparison"))
                .toObject();
        params.insert(QStringLiteral("detectRegionType"),
                      QStringLiteral("circle"));
        params.insert(QStringLiteral("detectRoiNormalized"),
                      rectJson(0.40,
                               0.50 - shortAxisRadius,
                               0.20,
                               shortAxisRadius * 2.0));
        params.insert(
                QStringLiteral("detectCircleNormalized"),
                QJsonObject{{QStringLiteral("center"), pointJson(0.50, 0.50)},
                            {QStringLiteral("radius"), radius},
                            {QStringLiteral("boundingRect"),
                             rectJson(0.40,
                                      0.50 - shortAxisRadius,
                                      0.20,
                                      shortAxisRadius * 2.0)},
                            {QStringLiteral("valid"), true}});
        root.insert(QStringLiteral("colorComparison"), params);
        aspectCircle.params = root;

        ColorComparisonDialog dialog;
        dialog.loadFromConfig(aspectCircle);
        QLabel *templatePreview = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonTemplatePreview"),
                "aspect-correct circle needs the template preview");
        QPushButton *finish = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonFinishButton"),
                "aspect-correct circle needs the Finish button");
        check(modelState(dialog) == QStringLiteral("ready")
                      && finish && finish->isEnabled(),
              "1920x515 aspect-correct circle bounding boxes must remain valid V2 input");
        const QRect previewContent = nonWhiteContentBounds(
                    labelImage(templatePreview));
        check(previewContent.width() >= 60,
              "sync circle template preview must recompute its max-dimension bounds for the current reference aspect ratio");
        const ToolConfig saved = dialog.toolConfig();
        ColorComparisonDialog reopened;
        reopened.loadFromConfig(saved);
        QPushButton *reopenedFinish = requiredChild<QPushButton>(
                reopened, QStringLiteral("colorComparisonFinishButton"),
                "reopened aspect-correct circle needs the Finish button");
        check(modelState(reopened) == QStringLiteral("ready")
                      && reopenedFinish && reopenedFinish->isEnabled(),
              "1920x515 aspect-correct circle must survive save/reopen");
    }

    {
        ToolConfig oldCircleWithoutBounds = v2Config(
                    QStringLiteral("sync"),
                    readyModel(QStringLiteral("circle-without-bounds")));
        QJsonObject root = oldCircleWithoutBounds.params;
        QJsonObject params = root.value(QStringLiteral("colorComparison"))
                .toObject();
        params.insert(QStringLiteral("detectRegionType"),
                      QStringLiteral("circle"));
        params.insert(
                QStringLiteral("detectCircleNormalized"),
                QJsonObject{{QStringLiteral("center"), pointJson(0.95, 0.95)},
                            {QStringLiteral("radius"), 0.10},
                            {QStringLiteral("valid"), true}});
        root.insert(QStringLiteral("colorComparison"), params);
        oldCircleWithoutBounds.params = root;

        ColorComparisonDialog dialog;
        dialog.loadFromConfig(oldCircleWithoutBounds);
        QPushButton *finish = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonFinishButton"),
                "old circle without boundingRect needs the Finish button");
        check(modelState(dialog) == QStringLiteral("ready")
                      && finish && finish->isEnabled(),
              "old valid V2 circles without boundingRect must defer image-bound checks to execution");
    }

    {
        ToolConfig invalid = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("invalid-enums")));
        invalid.schemaVersion = 9;
        invalid.toolName = QStringLiteral("ColorComparisonPreserveMe");
        invalid.displayName = QStringLiteral("invalid V2 display name");
        invalid.enabled = false;
        invalid.summary = QStringLiteral("invalid V2 summary must survive");
        QJsonObject root = invalid.params;
        root.insert(QStringLiteral("unrelatedRootField"),
                    QStringLiteral("must survive"));
        QJsonObject params = root.value(QStringLiteral("colorComparison"))
                .toObject();
        params.insert(QStringLiteral("templateRegionMode"),
                      QStringLiteral("future-template-mode"));
        params.insert(QStringLiteral("detectRegionType"),
                      QStringLiteral("capsule"));
        QJsonObject comparison = params.value(QStringLiteral("comparison"))
                .toObject();
        comparison.insert(QStringLiteral("sensitivity"),
                          QStringLiteral("future-sensitivity"));
        params.insert(QStringLiteral("comparison"), comparison);
        root.insert(QStringLiteral("colorComparison"), params);
        invalid.params = root;
        invalid.judgeRule.insert(QStringLiteral("futureJudgeField"), 17);

        ColorComparisonDialog dialog;
        dialog.loadFromConfig(invalid);
        QLabel *stateLabel = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonModelStateLabel"),
                "invalid V2 enum guard needs the model-state label");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "invalid V2 enum guard needs the status label");
        QPushButton *finish = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonFinishButton"),
                "invalid V2 enum guard needs the Finish button");
        QPushButton *rebuild = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonRebuildModelButton"),
                "invalid V2 enum guard needs the rebuild button");
        QPushButton *referenceTest = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonReferenceTestButton"),
                "invalid V2 enum guard needs the reference-test button");
        QPushButton *testRun = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonTestRunButton"),
                "invalid V2 enum guard needs the test-run button");

        check(dialog.toolConfig().toJson() == invalid.toJson(),
              "invalid V2 enums must preserve the complete original ToolConfig exactly");
        check(stateLabel && stateLabel->text().contains(QStringLiteral("invalid"),
                                                        Qt::CaseInsensitive)
                      && status
                      && status->text().contains(QStringLiteral("invalid"),
                                                 Qt::CaseInsensitive),
              "invalid V2 load must expose a clear invalid status and message");
        check(finish && rebuild && referenceTest && testRun
                      && !finish->isEnabled() && !rebuild->isEnabled()
                      && !referenceTest->isEnabled() && !testRun->isEnabled(),
              "invalid V2 load must disable Finish, rebuild, and both test actions");
        if (finish)
            finish->click();
        check(dialog.result() != QDialog::Accepted,
              "Finish must not accept an invalid V2 configuration");
        dialog.accept();
        check(dialog.result() != QDialog::Accepted,
              "direct accept must not close an invalid V2 configuration");
        check(dialog.toolConfig().toJson() == invalid.toJson(),
              "blocked invalid V2 actions must not overwrite the original ToolConfig");

        const ToolConfig recovered = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("recovered-valid")));
        dialog.loadFromConfig(recovered);
        check(modelState(dialog) == QStringLiteral("ready")
                      && modelReferenceHash(dialog)
                         == colorComparisonReferenceHash(
                                ReferenceImageProvider::instance()
                                .referenceFrame())
                      && finish && rebuild && referenceTest && testRun
                      && finish->isEnabled() && rebuild->isEnabled()
                      && referenceTest->isEnabled() && testRun->isEnabled(),
              "loading a valid V2 config must leave the invalid read-only state");
        check(status
                      && !status->text().contains(QStringLiteral("invalid"),
                                                 Qt::CaseInsensitive)
                      && !status->text().contains(QStringLiteral("只读")),
              "valid V2 recovery must clear the previous invalid read-only status");
    }

    {
        ToolConfig staleReference = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("stale-reference-on-load")));
        QJsonObject root = staleReference.params;
        QJsonObject params = root.value(QStringLiteral("colorComparison"))
                .toObject();
        QJsonObject model = params.value(QStringLiteral("model")).toObject();
        model.insert(QStringLiteral("referenceImageHash"),
                     QStringLiteral("different-reference-hash"));
        params.insert(QStringLiteral("model"), model);
        root.insert(QStringLiteral("colorComparison"), params);
        staleReference.params = root;

        ColorComparisonDialog dialog;
        dialog.loadFromConfig(staleReference);
        check(modelState(dialog) == QStringLiteral("stale")
                      && dialogLifecycle(dialog)
                         .value(QStringLiteral("status")).toString()
                         == QStringLiteral("model_stale")
                      && modelReferenceHash(dialog)
                         == QStringLiteral("different-reference-hash"),
              "loading a ready model against a changed current reference must immediately preserve its hash and mark it stale");
    }

    {
        ToolConfig invalidMask = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("invalid-mask")));
        QJsonObject root = invalidMask.params;
        QJsonObject params = root.value(QStringLiteral("colorComparison"))
                .toObject();
        params.insert(QStringLiteral("detectMaskPolygon"),
                      QJsonArray{pointJson(0.10, 0.10),
                                 QJsonObject{{QStringLiteral("x"), 0.30}},
                                 pointJson(0.42, 0.38)});
        root.insert(QStringLiteral("colorComparison"), params);
        invalidMask.params = root;

        ColorComparisonDialog dialog;
        dialog.loadFromConfig(invalidMask);
        QPushButton *finish = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonFinishButton"),
                "malformed V2 mask guard needs the Finish button");
        check(dialog.toolConfig().toJson() == invalidMask.toJson(),
              "malformed V2 mask points must not be dropped or repaired on load/save");
        check(finish && !finish->isEnabled(),
              "malformed V2 masks must enter the read-only invalid state");
    }

    {
        QVector<QPair<QString, ToolConfig>> malformedStaleModels;

        ToolConfig nonNumeric = v2Config(
                    QStringLiteral("custom"),
                    staleModel(QStringLiteral("malformed-stale-string")));
        QJsonObject root = nonNumeric.params;
        QJsonObject params = root.value(QStringLiteral("colorComparison"))
                .toObject();
        QJsonObject model = params.value(QStringLiteral("model")).toObject();
        QJsonArray values = model.value(QStringLiteral("values")).toArray();
        values[0] = QStringLiteral("not-a-number");
        model.insert(QStringLiteral("values"), values);
        params.insert(QStringLiteral("model"), model);
        root.insert(QStringLiteral("colorComparison"), params);
        nonNumeric.params = root;
        malformedStaleModels.append({QStringLiteral("non-numeric"), nonNumeric});

        ToolConfig wrongSize = v2Config(
                    QStringLiteral("custom"),
                    staleModel(QStringLiteral("malformed-stale-size")));
        root = wrongSize.params;
        params = root.value(QStringLiteral("colorComparison")).toObject();
        model = params.value(QStringLiteral("model")).toObject();
        values = model.value(QStringLiteral("values")).toArray();
        values.removeLast();
        model.insert(QStringLiteral("values"), values);
        params.insert(QStringLiteral("model"), model);
        root.insert(QStringLiteral("colorComparison"), params);
        wrongSize.params = root;
        malformedStaleModels.append({QStringLiteral("wrong-size"), wrongSize});

        ToolConfig negative = v2Config(
                    QStringLiteral("custom"),
                    staleModel(QStringLiteral("malformed-stale-negative")));
        root = negative.params;
        params = root.value(QStringLiteral("colorComparison")).toObject();
        model = params.value(QStringLiteral("model")).toObject();
        values = model.value(QStringLiteral("values")).toArray();
        values[0] = -0.1;
        model.insert(QStringLiteral("values"), values);
        params.insert(QStringLiteral("model"), model);
        root.insert(QStringLiteral("colorComparison"), params);
        negative.params = root;
        malformedStaleModels.append({QStringLiteral("negative"), negative});

        ToolConfig unnormalized = v2Config(
                    QStringLiteral("custom"),
                    staleModel(QStringLiteral("malformed-stale-sum")));
        root = unnormalized.params;
        params = root.value(QStringLiteral("colorComparison")).toObject();
        model = params.value(QStringLiteral("model")).toObject();
        values = model.value(QStringLiteral("values")).toArray();
        values[0] = 0.1;
        model.insert(QStringLiteral("values"), values);
        params.insert(QStringLiteral("model"), model);
        root.insert(QStringLiteral("colorComparison"), params);
        unnormalized.params = root;
        malformedStaleModels.append(
                    {QStringLiteral("unnormalized"), unnormalized});

        for (const auto &entry : malformedStaleModels) {
            ColorComparisonDialog dialog;
            dialog.loadFromConfig(entry.second);
            QPushButton *finish = requiredChild<QPushButton>(
                    dialog, QStringLiteral("colorComparisonFinishButton"),
                    "malformed stale model needs the Finish button");
            QLabel *status = requiredChild<QLabel>(
                    dialog, QStringLiteral("colorComparisonStatusLabel"),
                    "malformed stale model needs the status label");
            check(finish && !finish->isEnabled()
                          && status
                          && status->text().contains(
                                 QStringLiteral("model_invalid"))
                          && dialog.toolConfig().toJson()
                             == entry.second.toJson(),
                  QStringLiteral("%1 non-ready V2 model must enter exact read-only preservation instead of being canonicalized")
                  .arg(entry.first));
        }
    }

    {
        QVector<QPair<QString, ToolConfig>> invalidCases;

        ToolConfig invalidRect = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("invalid-rect")));
        QJsonObject rectRoot = invalidRect.params;
        QJsonObject rectParams = rectRoot.value(
                    QStringLiteral("colorComparison")).toObject();
        rectParams.insert(QStringLiteral("detectRoiNormalized"),
                          rectJson(0.80, 0.20, 0.40, 0.30));
        rectRoot.insert(QStringLiteral("colorComparison"), rectParams);
        invalidRect.params = rectRoot;
        invalidCases.append(qMakePair(QStringLiteral("out-of-range rectangle"),
                                      invalidRect));

        ToolConfig nonFiniteRect = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("nonfinite-rect")));
        QJsonObject nonFiniteRoot = nonFiniteRect.params;
        QJsonObject nonFiniteParams = nonFiniteRoot.value(
                    QStringLiteral("colorComparison")).toObject();
        nonFiniteParams.insert(
                    QStringLiteral("detectRoiNormalized"),
                    rectJson(std::numeric_limits<double>::infinity(),
                             0.20, 0.40, 0.30));
        nonFiniteRoot.insert(QStringLiteral("colorComparison"),
                             nonFiniteParams);
        nonFiniteRect.params = nonFiniteRoot;
        invalidCases.append(qMakePair(QStringLiteral("non-finite rectangle"),
                                      nonFiniteRect));

        ToolConfig invalidDetectEnum = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("invalid-detect-enum")));
        QJsonObject detectEnumRoot = invalidDetectEnum.params;
        QJsonObject detectEnumParams = detectEnumRoot.value(
                    QStringLiteral("colorComparison")).toObject();
        detectEnumParams.insert(QStringLiteral("detectRegionType"),
                                QStringLiteral("capsule"));
        detectEnumRoot.insert(QStringLiteral("colorComparison"),
                              detectEnumParams);
        invalidDetectEnum.params = detectEnumRoot;
        invalidCases.append(qMakePair(QStringLiteral("unknown detect-region enum"),
                                      invalidDetectEnum));

        ToolConfig invalidSensitivity = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("invalid-sensitivity")));
        QJsonObject sensitivityRoot = invalidSensitivity.params;
        QJsonObject sensitivityParams = sensitivityRoot.value(
                    QStringLiteral("colorComparison")).toObject();
        QJsonObject comparison = sensitivityParams.value(
                    QStringLiteral("comparison")).toObject();
        comparison.insert(QStringLiteral("sensitivity"),
                          QStringLiteral("future-sensitivity"));
        sensitivityParams.insert(QStringLiteral("comparison"), comparison);
        sensitivityRoot.insert(QStringLiteral("colorComparison"),
                               sensitivityParams);
        invalidSensitivity.params = sensitivityRoot;
        invalidCases.append(qMakePair(QStringLiteral("unknown sensitivity enum"),
                                      invalidSensitivity));

        ToolConfig outOfRangeMask = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("out-of-range-mask")));
        QJsonObject maskRoot = outOfRangeMask.params;
        QJsonObject maskParams = maskRoot.value(
                    QStringLiteral("colorComparison")).toObject();
        maskParams.insert(QStringLiteral("templateMaskPolygon"),
                          QJsonArray{pointJson(0.10, 0.10),
                                     pointJson(1.20, 0.10),
                                     pointJson(0.40, 0.40)});
        maskRoot.insert(QStringLiteral("colorComparison"), maskParams);
        outOfRangeMask.params = maskRoot;
        invalidCases.append(qMakePair(QStringLiteral("out-of-range mask point"),
                                      outOfRangeMask));

        const double radius = 0.10;
        const double shortAxisRadius = radius * 1920.0 / 515.0;
        ToolConfig invalidCircle = v2Config(
                    QStringLiteral("sync"),
                    readyModel(QStringLiteral("invalid-circle")));
        QJsonObject circleRoot = invalidCircle.params;
        QJsonObject circleParams = circleRoot.value(
                    QStringLiteral("colorComparison")).toObject();
        circleParams.insert(QStringLiteral("detectRegionType"),
                            QStringLiteral("circle"));
        circleParams.insert(
                QStringLiteral("detectCircleNormalized"),
                QJsonObject{{QStringLiteral("center"), pointJson(0.50, 0.80)},
                            {QStringLiteral("radius"), radius},
                            {QStringLiteral("boundingRect"),
                             rectJson(0.40,
                                      0.80 - shortAxisRadius,
                                      0.20,
                                      shortAxisRadius * 2.0)},
                            {QStringLiteral("valid"), true}});
        circleRoot.insert(QStringLiteral("colorComparison"), circleParams);
        invalidCircle.params = circleRoot;
        invalidCases.append(qMakePair(QStringLiteral("short-axis out-of-range circle"),
                                      invalidCircle));

        ToolConfig invalidComparison = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("invalid-comparison")));
        QJsonObject comparisonRoot = invalidComparison.params;
        QJsonObject comparisonParams = comparisonRoot.value(
                    QStringLiteral("colorComparison")).toObject();
        comparisonParams.insert(QStringLiteral("comparison"),
                                QStringLiteral("not-an-object"));
        comparisonRoot.insert(QStringLiteral("colorComparison"),
                              comparisonParams);
        invalidComparison.params = comparisonRoot;
        invalidCases.append(qMakePair(QStringLiteral("malformed comparison"),
                                      invalidComparison));

        ToolConfig invalidBrightness = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("invalid-brightness")));
        QJsonObject brightnessRoot = invalidBrightness.params;
        QJsonObject brightnessParams = brightnessRoot.value(
                    QStringLiteral("colorComparison")).toObject();
        QJsonObject brightnessComparison = brightnessParams.value(
                    QStringLiteral("comparison")).toObject();
        brightnessComparison.insert(
                    QStringLiteral("brightnessCompensation"), 1);
        brightnessParams.insert(QStringLiteral("comparison"),
                                brightnessComparison);
        brightnessRoot.insert(QStringLiteral("colorComparison"),
                              brightnessParams);
        invalidBrightness.params = brightnessRoot;
        invalidCases.append(qMakePair(QStringLiteral("malformed brightness"),
                                      invalidBrightness));

        ToolConfig invalidPosition = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("invalid-position")));
        QJsonObject positionRoot = invalidPosition.params;
        QJsonObject positionParams = positionRoot.value(
                    QStringLiteral("colorComparison")).toObject();
        positionParams.insert(
                    QStringLiteral("positionCorrection"),
                    QJsonObject{{QStringLiteral("enabled"), false},
                                {QStringLiteral("sourceId"), QString()},
                                {QStringLiteral("interfaceVersion"), 2}});
        positionRoot.insert(QStringLiteral("colorComparison"), positionParams);
        invalidPosition.params = positionRoot;
        invalidCases.append(qMakePair(QStringLiteral("illegal position interface"),
                                      invalidPosition));

        ToolConfig invalidJudge = v2Config(
                    QStringLiteral("custom"),
                    readyModel(QStringLiteral("invalid-judge")));
        invalidJudge.judgeRule.insert(QStringLiteral("minScore"), 101);
        invalidCases.append(qMakePair(QStringLiteral("out-of-range judge rule"),
                                      invalidJudge));

        for (const auto &invalidCase : invalidCases) {
            ColorComparisonDialog dialog;
            dialog.loadFromConfig(invalidCase.second);
            QPushButton *finish = dialog.findChild<QPushButton *>(
                        QStringLiteral("colorComparisonFinishButton"));
            check(dialog.toolConfig().toJson()
                          == invalidCase.second.toJson(),
                  QStringLiteral("%1 must preserve the original V2 ToolConfig")
                  .arg(invalidCase.first));
            check(finish && !finish->isEnabled(),
                  QStringLiteral("%1 must enter read-only invalid state")
                  .arg(invalidCase.first));
        }
    }

    {
        ToolConfig forged = v2Config(QStringLiteral("custom"),
                                     readyModel(QStringLiteral("native-metadata")));
        QJsonObject root = forged.params;
        QJsonObject params = root.value(QStringLiteral("colorComparison")).toObject();
        params.insert(QStringLiteral("dialogLifecycle"),
                      QJsonObject{
                          {QStringLiteral("originVersion"), 2},
                          {QStringLiteral("status"), QStringLiteral("ok")},
                          {QStringLiteral("reason"),
                           QStringLiteral("forged native lifecycle reason")}
                      });
        root.insert(QStringLiteral("colorComparison"), params);
        forged.params = root;

        ColorComparisonDialog dialog;
        dialog.loadFromConfig(forged);
        QLabel *stateLabel = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonModelStateLabel"),
                "native V2 lifecycle guard needs the model state label");
        check(modelState(dialog) == QStringLiteral("ready")
                      && dialogLifecycle(dialog)
                         .value(QStringLiteral("status")).toString()
                         == QStringLiteral("ok")
                      && dialogLifecycle(dialog)
                         .value(QStringLiteral("reason")).toString().isEmpty()
                      && stateLabel
                      && !stateLabel->text().contains(
                          QStringLiteral("forged native lifecycle reason")),
              "native V2 must ignore dialog lifecycle metadata and trust model validation");
    }

    {
        ReferenceImageProvider::instance().clearReferenceFrame();
        ColorComparisonDialog dialog;
        QPushButton *finish = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonFinishButton"),
                "Finish button must have a stable object name");
        if (finish)
            finish->click();
        check(dialog.result() == QDialog::Accepted,
              "Finish may accept an empty model without implicit sampling");
        check(modelState(dialog) == QStringLiteral("empty"),
              "Finish must never change an empty model to ready");
    }


    {
        ReferenceImageProvider::instance().setReferenceFrame(colorFrame,
                                                              colorMetadata);
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(v2Config(QStringLiteral("custom"),
                                       staleModel(QStringLiteral("finish-stale"))));
        const QJsonObject before = modelJson(dialog);
        QPushButton *finish = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonFinishButton"),
                "stale Finish test needs the Finish button");
        if (finish)
            finish->click();
        check(dialog.result() == QDialog::Accepted,
              "Finish may accept a stale model without implicit sampling");
        check(modelJson(dialog) == before,
              "Finish must preserve every stale model value and hash");
    }

    {
        ReferenceImageProvider::instance().setReferenceFrame(colorFrame, colorMetadata);
        ColorComparisonDialog dialog;
        QPushButton *referenceTest = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonReferenceTestButton"),
                "reference test button must have a stable object name");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "reference test status label must exist");
        if (referenceTest)
            referenceTest->click();
        check(waitUntil([status]() {
                  return status && status->text().contains(QStringLiteral("model_empty"));
              }),
              "reference test must report the stored empty model state");
        check(modelState(dialog) == QStringLiteral("empty"),
              "reference test must never rebuild an empty model");
    }


    {
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(v2Config(QStringLiteral("custom"),
                                       staleModel(QStringLiteral("reference-stale"))));
        const QJsonObject before = modelJson(dialog);
        QPushButton *referenceTest = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonReferenceTestButton"),
                "stale reference test needs the reference test button");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "stale reference test needs the status label");
        if (referenceTest)
            referenceTest->click();
        check(waitUntil([status]() {
                  return status && status->text().contains(QStringLiteral("model_stale"));
              }),
              "reference test must report model_stale before HALCON loading");
        check(modelJson(dialog) == before,
              "reference test must preserve every stale model value and hash");
    }

    {
        const cv::Mat monoFrame(80, 120, CV_8UC1, cv::Scalar(96));
        const FrameInputMetadata monoCamera =
                FrameInputMetadata::fromMat(monoFrame, QStringLiteral("camera"));
        ReferenceImageProvider::instance().setReferenceFrame(colorFrame,
                                                              colorMetadata);
        CameraFrameProvider::instance().setCurrentFrame(monoFrame, monoCamera);
        ColorComparisonDialog dialog;
        ToolConfig maskedCamera = v2Config(QStringLiteral("custom"),
                                           ColorComparisonModelV2());
        QJsonObject maskedRoot = maskedCamera.params;
        QJsonObject maskedParams = maskedRoot
                .value(QStringLiteral("colorComparison")).toObject();
        maskedParams.insert(QStringLiteral("templateMaskPolygon"), polygonJson());
        maskedRoot.insert(QStringLiteral("colorComparison"), maskedParams);
        maskedCamera.params = maskedRoot;
        dialog.loadFromConfig(maskedCamera);
        QPushButton *testRun = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonTestRunButton"),
                "Mono8 async test needs the test button");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "Mono8 async test needs the status label");
        QLabel *viewerTitle = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonViewerTitleLabel"),
                "viewer title must have a stable object name");
        if (testRun)
            testRun->click();
        check(status && status->text().contains(QStringLiteral("运行中")),
              "test click must expose an in-flight state before queued worker completion");
        check(waitUntil([status]() {
                  return status
                          && status->text().contains(QStringLiteral("NG"))
                          && status->text().contains(QStringLiteral("score:0.00"))
                          && status->text().contains(
                                  QStringLiteral("unsupported_color_input"))
                          && status->text().contains(QStringLiteral("monochrome"),
                                                     Qt::CaseInsensitive);
              }),
              "atomic Mono8 camera snapshot must asynchronously report invalid NG/unsupported_color_input");
        check(modelState(dialog) == QStringLiteral("empty"),
              "camera test must not rebuild the model");
        FrameViewHelper *preview = dialog.findChild<FrameViewHelper *>();
        QGraphicsView *graphicsView = dialog.findChild<QGraphicsView *>();
        if (graphicsView && graphicsView->scene()) {
            bool hasTemplateMaskPolygon = false;
            for (QGraphicsItem *item : graphicsView->scene()->items()) {
                if (item && item->isVisible()
                        && item->type() == QGraphicsPolygonItem::Type) {
                    hasTemplateMaskPolygon = true;
                    break;
                }
            }
            check(!hasTemplateMaskPolygon,
                  "current camera frames must exclude template-mask polygons");
        }
        int overlayItemCount = -1;
        if (preview && graphicsView && graphicsView->scene()) {
            ToolOverlay overlay;
            overlay.type = ToolOverlayType::Rect;
            overlay.rect = QRectF(4.0, 4.0, 20.0, 18.0);
            preview->setToolOverlays(QVector<ToolOverlay>{overlay});
            overlayItemCount = graphicsView->scene()->items().size();
        }
        if (testRun)
            testRun->click();
        QPushButton *exitTest = dialog.findChild<QPushButton *>(
                    QStringLiteral("exitTestButton"));
        if (exitTest)
            exitTest->click();
        if (overlayItemCount >= 0 && graphicsView && graphicsView->scene()) {
            check(graphicsView->scene()->items().size() < overlayItemCount,
                  "switching from camera test to reference preview must clear stale result overlays");
        }
        check(viewerTitle
                      && viewerTitle->text() == QStringLiteral("基准图"),
              "exiting camera test must restore the reference viewer title");
    }

    {
        const cv::Mat monoReference(80, 120, CV_8UC1, cv::Scalar(112));
        ReferenceImageProvider::instance().setReferenceFrame(
                monoReference,
                FrameInputMetadata::fromMat(monoReference,
                                            QStringLiteral("reference")));
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(v2Config(QStringLiteral("custom"),
                                       staleModel(QStringLiteral("failed-rebuild"))));
        const QJsonObject before = modelJson(dialog);
        QPushButton *rebuild = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonRebuildModelButton"),
                "explicit sampling button must exist for async sampling test");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "async sampling test needs the status label");
        if (rebuild)
            rebuild->click();
        check(status && status->text().contains(QStringLiteral("正在重新取样")),
              "explicit sampling click must expose an in-flight state before worker completion");
        check(waitUntil([status, rebuild]() {
                  return status && rebuild && rebuild->isEnabled()
                          && status->text().contains(
                                  QStringLiteral("unsupported_color_input"));
              }),
              "explicit Mono8 sampling must fail asynchronously before HALCON/license loading");
        check(modelJson(dialog) == before,
              "failed explicit sampling must preserve the existing stale model exactly");
    }

    {
        const cv::Mat monoFrame(80, 120, CV_8UC1, cv::Scalar(118));
        const FrameInputMetadata monoMetadata =
                FrameInputMetadata::fromMat(monoFrame,
                                            QStringLiteral("reference"));
        ReferenceImageProvider::instance().setReferenceFrame(monoFrame,
                                                              monoMetadata);
        CameraFrameProvider::instance().setCurrentFrame(
                monoFrame,
                FrameInputMetadata::fromMat(monoFrame,
                                            QStringLiteral("camera")));
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(v2Config(QStringLiteral("custom"),
                                       staleModel(QStringLiteral("epoch-base"))));
        QPushButton *rebuild = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonRebuildModelButton"),
                "build epoch test needs the explicit sampling button");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "build epoch test needs the status label");
        QFutureWatcher<ColorComparisonTemplateBuildResult> *watcher =
                templateBuildWatcher(dialog);
        check(watcher != nullptr,
              "build epoch test must locate the Dialog model-build watcher");

        if (rebuild)
            rebuild->click();
        check(waitUntil([status, rebuild]() {
                  return status && rebuild && rebuild->isEnabled()
                          && status->text().contains(
                                  QStringLiteral("unsupported_color_input"));
              }),
              "build epoch setup must complete a real asynchronous build failure");

        if (watcher) {
            const ColorComparisonTemplateBuildResult success =
                    successfulBuildResult(QStringLiteral("epoch-success"));
            watcher->setFuture(QtConcurrent::run([success]() {
                QThread::msleep(120);
                return success;
            }));
        }

        QComboBox *sensitivity = requiredChild<QComboBox>(
                dialog, QStringLiteral("colorComparisonSensitivityCombo"),
                "build epoch test needs sensitivity");
        QSpinBox *minScore = requiredChild<QSpinBox>(
                dialog, QStringLiteral("colorComparisonMinScore"),
                "build epoch test needs minimum score");
        QCheckBox *position = requiredChild<QCheckBox>(
                dialog, QStringLiteral("positionCorrectionSwitch"),
                "build epoch test needs position state");
        QToolButton *detectRect = requiredChild<QToolButton>(
                dialog, QStringLiteral("colorComparisonDetectRectButton"),
                "build epoch test needs custom detection geometry");
        QPushButton *testRun = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonTestRunButton"),
                "build epoch test needs test start/stop state");
        FrameViewHelper *preview = dialog.findChild<FrameViewHelper *>();

        if (sensitivity)
            sensitivity->setCurrentIndex((sensitivity->currentIndex() + 1)
                                         % sensitivity->count());
        if (minScore)
            minScore->setValue(minScore->value() == 79 ? 78 : 79);
        if (position)
            position->setChecked(!position->isChecked());
        if (detectRect)
            detectRect->click();
        if (preview)
            preview->roiChanged(QRectF(0.22, 0.18, 0.42, 0.36));
        if (testRun) {
            testRun->click();
            testRun->click();
        }

        check(waitUntil([&dialog]() {
                  return modelState(dialog) == QStringLiteral("ready");
              }),
              "non-template test/position/judge/custom-detection changes must not discard a successful build");
        check(modelReferenceHash(dialog)
                      == QStringLiteral("reference-hash-epoch-success"),
              "the successful build result must be the model applied after non-template changes");
        check(dialogLifecycle(dialog)
                      .value(QStringLiteral("originVersion")).toInt() == 2
                      && dialogLifecycle(dialog)
                         .value(QStringLiteral("status")).toString()
                         == QStringLiteral("ok"),
              "a successful rebuild must reset lifecycle provenance to native V2/ok");
    }

    {
        const cv::Mat monoReference(80, 120, CV_8UC1, cv::Scalar(122));
        ReferenceImageProvider::instance().setReferenceFrame(
                monoReference,
                FrameInputMetadata::fromMat(monoReference,
                                            QStringLiteral("reference")));
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(v2Config(QStringLiteral("custom"),
                                       readyModel(QStringLiteral("epoch-stale"))));
        const QString referenceHash = modelReferenceHash(dialog);
        const QString extractHash = modelExtractHash(dialog);
        QPushButton *rebuild = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonRebuildModelButton"),
                "template invalidation test needs the sampling button");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "template invalidation test needs the status label");
        QCheckBox *brightness = requiredChild<QCheckBox>(
                dialog, QStringLiteral("colorComparisonBrightnessCompensation"),
                "template invalidation test needs brightness compensation");

        if (rebuild)
            rebuild->click();
        if (brightness)
            brightness->setChecked(true);
        check(status
                      && !status->text().contains(QStringLiteral("正在重新取样"))
                      && status->text().contains(QStringLiteral("model_stale")),
              "template extraction changes must immediately replace sampling status with stored stale instruction");
        check(waitUntil([rebuild]() {
                  return rebuild && rebuild->isEnabled();
              }),
              "invalidated template build must still complete without blocking the Dialog");
        check(status && status->text().contains(QStringLiteral("model_stale"))
                      && !status->text().contains(
                          QStringLiteral("unsupported_color_input")),
              "an obsolete build completion must not overwrite the current stored stale instruction");
        check(modelState(dialog) == QStringLiteral("stale"),
              "template extraction changes during build must keep the stored model stale");
        checkHashesPreserved(dialog, referenceHash, extractHash,
                             "template build invalidation must preserve stored model hashes");
    }

    {
        const cv::Mat monoReference(80, 120, CV_8UC1, cv::Scalar(126));
        ReferenceImageProvider::instance().setReferenceFrame(
                monoReference,
                FrameInputMetadata::fromMat(monoReference,
                                            QStringLiteral("reference")));
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(v2Config(QStringLiteral("custom"),
                                       readyModel(QStringLiteral("old-active-test"))));
        QPushButton *rebuild = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonRebuildModelButton"),
                "active old-test race needs the sampling button");
        QPushButton *referenceTest = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonReferenceTestButton"),
                "active old-test race needs the reference-test button");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "active old-test race needs the status label");
        QFutureWatcher<ColorComparisonTemplateBuildResult> *buildWatcher =
                templateBuildWatcher(dialog);
        QFutureWatcher<ToolResult> *testWatcher = testResultWatcher(dialog);
        check(buildWatcher && testWatcher,
              "active old-test race must locate both Dialog watchers");

        if (rebuild)
            rebuild->click();
        check(waitUntil([status, rebuild]() {
                  return status && rebuild && rebuild->isEnabled()
                          && status->text().contains(
                                  QStringLiteral("unsupported_color_input"));
              }),
              "active old-test race must establish the current build epoch");

        QSemaphore buildGate;
        if (buildWatcher) {
            const ColorComparisonTemplateBuildResult success =
                    successfulBuildResult(QStringLiteral("new-active-model"));
            buildWatcher->setFuture(QtConcurrent::run([&buildGate, success]() {
                buildGate.acquire();
                return success;
            }));
        }
        if (referenceTest)
            referenceTest->click();
        check(waitUntil([&dialog, testWatcher]() {
                  return testWatcher && !testWatcher->isRunning()
                          && dialog.referencePreviewSnapshot().valid;
              }),
              "reference test created during build must first capture the old ready model");
        const ToolPreviewSnapshot baselinePreview =
                dialog.referencePreviewSnapshot();

        const QString oldOverlayLabel =
                QStringLiteral("controlled_old_active_overlay");
        QSemaphore oldTestGate;
        if (testWatcher) {
            const ToolResult oldResult = controlledOldTestResult(
                        QStringLiteral("controlled_old_active_result"),
                        oldOverlayLabel);
            testWatcher->setFuture(QtConcurrent::run([&oldTestGate, oldResult]() {
                oldTestGate.acquire();
                return oldResult;
            }));
        }

        buildGate.release();
        check(waitUntil([&dialog, buildWatcher]() {
                  return buildWatcher && !buildWatcher->isRunning()
                          && modelReferenceHash(dialog)
                          == QStringLiteral("reference-hash-new-active-model");
              }),
              "test activity during build must not cancel successful model application");
        check(testWatcher && testWatcher->isRunning()
                      && status
                      && status->text().contains(QStringLiteral("取样完成")),
              "new model status must be visible while the old active test is still blocked");

        oldTestGate.release();
        check(waitUntil([testWatcher]() {
                  return testWatcher && !testWatcher->isRunning();
              }),
              "controlled old active test must finish after the successful build");
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        const ToolPreviewSnapshot finalPreview =
                dialog.referencePreviewSnapshot();
        check(status
                      && status->text().contains(QStringLiteral("取样完成"))
                      && !status->text().contains(
                          QStringLiteral("controlled_old_active_result")),
              "old active test completion must not overwrite the rebuilt-model status");
        check(!sceneHasToolTip(dialog, oldOverlayLabel),
              "old active test completion must not restore an obsolete overlay");
        check(finalPreview.timestamp == baselinePreview.timestamp
                      && finalPreview.statusText == baselinePreview.statusText
                      && !finalPreview.statusText.contains(
                          QStringLiteral("controlled_old_active_result")),
              "old active reference-test completion must not overwrite the reference preview snapshot");
    }

    {
        const cv::Mat monoReference(80, 120, CV_8UC1, cv::Scalar(130));
        ReferenceImageProvider::instance().setReferenceFrame(
                monoReference,
                FrameInputMetadata::fromMat(monoReference,
                                            QStringLiteral("reference")));
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(v2Config(QStringLiteral("custom"),
                                       readyModel(QStringLiteral("old-pending-test"))));
        QPushButton *rebuild = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonRebuildModelButton"),
                "pending old-test race needs the sampling button");
        QPushButton *referenceTest = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonReferenceTestButton"),
                "pending old-test race needs the reference-test button");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "pending old-test race needs the status label");
        QFutureWatcher<ColorComparisonTemplateBuildResult> *buildWatcher =
                templateBuildWatcher(dialog);
        QFutureWatcher<ToolResult> *testWatcher = testResultWatcher(dialog);

        if (rebuild)
            rebuild->click();
        check(waitUntil([status, rebuild]() {
                  return status && rebuild && rebuild->isEnabled()
                          && status->text().contains(
                                  QStringLiteral("unsupported_color_input"));
              }),
              "pending old-test race must establish the current build epoch");

        QSemaphore buildGate;
        if (buildWatcher) {
            const ColorComparisonTemplateBuildResult success =
                    successfulBuildResult(QStringLiteral("new-pending-model"));
            buildWatcher->setFuture(QtConcurrent::run([&buildGate, success]() {
                buildGate.acquire();
                return success;
            }));
        }
        if (referenceTest)
            referenceTest->click();
        check(waitUntil([&dialog, testWatcher]() {
                  return testWatcher && !testWatcher->isRunning()
                          && dialog.referencePreviewSnapshot().valid;
              }),
              "pending race must establish a reference-preview baseline");
        const ToolPreviewSnapshot baselinePreview =
                dialog.referencePreviewSnapshot();

        QSemaphore activeTestGate;
        if (testWatcher) {
            const ToolResult activeResult = controlledOldTestResult(
                        QStringLiteral("controlled_obsolete_active_result"),
                        QStringLiteral("controlled_obsolete_active_overlay"));
            testWatcher->setFuture(QtConcurrent::run(
                        [&activeTestGate, activeResult]() {
                activeTestGate.acquire();
                return activeResult;
            }));
        }
        if (referenceTest)
            referenceTest->click();

        buildGate.release();
        check(waitUntil([&dialog, buildWatcher]() {
                  return buildWatcher && !buildWatcher->isRunning()
                          && modelReferenceHash(dialog)
                          == QStringLiteral("reference-hash-new-pending-model");
              }),
              "a pending old-model test must not cancel the successful build");
        check(testWatcher && testWatcher->isRunning()
                      && status
                      && status->text().contains(QStringLiteral("取样完成")),
              "successful build must apply before releasing the active test that guards pending work");

        activeTestGate.release();
        check(waitUntil([testWatcher]() {
                  return testWatcher && !testWatcher->isRunning();
              }),
              "active test must finish without launching an old pending request after rebuild");
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        const ToolPreviewSnapshot finalPreview =
                dialog.referencePreviewSnapshot();
        check(status && status->text().contains(QStringLiteral("取样完成"))
                      && !status->text().contains(
                          QStringLiteral("unsupported_color_input")),
              "pending old-model request must not overwrite rebuilt-model status");
        check(finalPreview.timestamp == baselinePreview.timestamp
                      && finalPreview.statusText == baselinePreview.statusText,
              "pending old reference request must not refresh the preview after model rebuild");
    }

    {
        const cv::Mat monoReference(80, 120, CV_8UC1, cv::Scalar(134));
        ReferenceImageProvider::instance().setReferenceFrame(
                monoReference,
                FrameInputMetadata::fromMat(monoReference,
                                            QStringLiteral("reference")));
        ColorComparisonDialog dialog;
        dialog.loadFromConfig(v2Config(QStringLiteral("custom"),
                                       readyModel(QStringLiteral("load-old"))));
        QPushButton *rebuild = requiredChild<QPushButton>(
                dialog, QStringLiteral("colorComparisonRebuildModelButton"),
                "active-build load replacement needs the sampling button");
        QLabel *status = requiredChild<QLabel>(
                dialog, QStringLiteral("colorComparisonStatusLabel"),
                "active-build load replacement needs the status label");
        if (rebuild)
            rebuild->click();
        dialog.loadFromConfig(v2Config(QStringLiteral("custom"),
                                       readyModel(QStringLiteral("load-ready"))));
        check(modelState(dialog) == QStringLiteral("ready")
                      && status
                      && !status->text().contains(QStringLiteral("NG"))
                      && !status->text().contains(QStringLiteral("模型未就绪"))
                      && (status->text().contains(QStringLiteral("已加载"))
                          || status->text().contains(QStringLiteral("可用"))),
              "loading a ready config over an active build must show ready/loaded status instead of a model-unready NG");
        check(waitUntil([rebuild]() {
                  return rebuild && rebuild->isEnabled();
              }),
              "obsolete build replaced by ready load must finish without blocking");
        check(status && !status->text().contains(QStringLiteral("模型未就绪")),
              "obsolete build completion must preserve the loaded-ready status");
    }

    ReferenceImageProvider::instance().clearReferenceFrame();
    CameraFrameProvider::instance().clearFrame();

    if (g_failures != 0) {
        std::cerr << "color_comparison_dialog_smoke: " << g_failures
                  << " failure(s)" << std::endl;
        return 1;
    }

    std::cout << "color_comparison_dialog_smoke: V2 Dialog checks passed"
              << std::endl;
    return 0;
}
