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
#include <QSpinBox>
#include <QThread>
#include <QToolButton>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>
#include <iostream>

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

ToolConfig v2Config(const QString &templateMode,
                    const ColorComparisonModelV2 &model = readyModel())
{
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
                           colorComparisonModelToJson(model));
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

    ReferenceImageProvider::instance().setReferenceFrame(colorFrame, colorMetadata);
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
