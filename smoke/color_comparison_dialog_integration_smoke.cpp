#include <QAbstractItemView>
#include <QApplication>
#include <QColor>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QRectF>
#include <QScrollArea>
#include <QToolButton>
#include <QVector>
#include <QWidget>

#include <opencv2/core.hpp>

#include "algorithms/recognition/ColorComparisonModel.h"
#include "frame/FrameInputMetadata.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "tooladapters/ColorComparisonAdapter.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"
#include "toolcore/ToolRequest.h"

#define private public
#include "ColorComparisonDialog.h"
#undef private

#include "ColorComparisonFeatureView.h"

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    }
}

QJsonArray oneHotHistogram(int size, int activeIndex)
{
    QJsonArray values;
    for (int index = 0; index < size; ++index)
        values.append(index == activeIndex ? 1.0 : 0.0);
    return values;
}

bool imageHasOnlyColor(const QImage &image, const QColor &expected)
{
    if (image.isNull())
        return false;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) != expected)
                return false;
        }
    }
    return true;
}

ToolResult validDiagnosticResult()
{
    QJsonObject diagnostics;
    diagnostics.insert(QStringLiteral("available"), true);
    diagnostics.insert(QStringLiteral("hueBins"), 32);
    diagnostics.insert(QStringLiteral("saturationBins"), 32);
    diagnostics.insert(QStringLiteral("layout"), QStringLiteral("hue_major"));
    diagnostics.insert(QStringLiteral("detectHsHistogram"),
                       oneHotHistogram(1024, 7 * 32 + 11));
    diagnostics.insert(QStringLiteral("detectValueHistogram"),
                       oneHotHistogram(32, 18));
    diagnostics.insert(QStringLiteral("rawIntersection"), 0.42);

    ToolResult result;
    result.success = true;
    result.ok = true;
    result.status = QStringLiteral("ok");
    result.score = 73.5;
    result.payload.insert(QStringLiteral("histogramDiagnostics"), diagnostics);
    result.payload.insert(QStringLiteral("hsScore"), 81.25);
    result.payload.insert(QStringLiteral("brightnessFactor"), 0.92);
    result.payload.insert(QStringLiteral("saturationFactor"), 0.75);
    result.payload.insert(QStringLiteral("brightnessCompensation"), QJsonObject{
                              {QStringLiteral("enabled"), true},
                              {QStringLiteral("requested"), true},
                              {QStringLiteral("applied"), false},
                              {QStringLiteral("fallback"), true},
                              {QStringLiteral("fallbackReason"),
                               QStringLiteral("scale_out_of_range")}
                          });
    return result;
}

} // namespace

// ColorComparisonDialog only needs this PlanDialogUtils entry while building
// its widget tree. Keeping the smoke stub local avoids linking MainWindow and
// every unrelated application dialog.
namespace PlanDialogUtils {
void applyLargeWindow(QWidget *window)
{
    if (window)
        window->resize(1280, 800);
}
} // namespace PlanDialogUtils

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    const QString stylePath = QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("../../../../styles/app.qss"));
    QFile styleFile(stylePath);
    check(styleFile.open(QIODevice::ReadOnly | QIODevice::Text),
          "public app.qss must be available to the dialog integration smoke");
    if (styleFile.isOpen())
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));

    ColorComparisonDialog dialog;
    dialog.show();
    QApplication::processEvents();

    check(dialog.objectName() == QStringLiteral("ColorComparisonDialog"),
          "dialog must expose the scoped-QSS object name");
    check(dialog.styleSheet().isEmpty(),
          "dialog must not override the public QSS with an inline stylesheet");
    check(dialog.findChild<QScrollArea *>(
              QStringLiteral("colorComparisonParamsScrollArea")) != nullptr,
          "parameter cards must live in the scrolling content area");
    check(dialog.findChild<QFrame *>(
              QStringLiteral("colorComparisonBottomActionBar")) != nullptr,
          "bottom actions must remain outside the scrolling content");

    ColorComparisonFeatureView *featureView =
            dialog.findChild<ColorComparisonFeatureView *>(
                QStringLiteral("colorComparisonFeatureView"));
    check(featureView != nullptr && featureView == dialog.m_featureView,
          "dialog must construct and retain the shared feature view");
    check(dialog.m_previewHelper != nullptr,
          "dialog must construct its FrameViewHelper");
    check(dialog.m_previewHelper && dialog.m_previewHelper->navigationEnabled(),
          "ColorComparisonDialog must explicitly opt in to image navigation");
    QLabel *cursorLabel = dialog.findChild<QLabel *>(QStringLiteral("viewerCursorLabel"));
    check(cursorLabel != nullptr,
          "ColorComparisonDialog must expose the shared cursor pixel label");
    check(cursorLabel
          && cursorLabel->palette().color(QPalette::WindowText) == QColor(QStringLiteral("#f8fafc")),
          "cursor pixel label must remain visible on the dark viewer status area");
    check(dialog.m_pcImportButton != nullptr
          && dialog.m_pcImportButton->objectName()
                 == QStringLiteral("colorComparisonPcImportButton")
          && dialog.m_pcImportButton->isVisible(),
          "PC image import button must be visible and independently addressable");
    check(dialog.m_pcImportButton && dialog.m_basicButton
          && dialog.m_pcImportButton->geometry().left()
                 < dialog.m_basicButton->geometry().left(),
          "PC image import button must be placed to the left of Basic");

    dialog.setAllParamsMode(true);
    dialog.m_positionCorrectionEnabled = true;
    dialog.refreshPositionCorrectionControls();
    QApplication::processEvents();
    check(dialog.m_positionCorrectionContourCheckBox != nullptr
          && dialog.m_positionCorrectionContourCheckBox->objectName()
             == QStringLiteral("positionCorrectionContourSwitch")
          && dialog.m_positionCorrectionContourRow->isVisible()
          && dialog.m_positionCorrectionContourCheckBox->isChecked(),
          "position-correction contour switch must be visible and default on");
    dialog.m_positionCorrectionContourCheckBox->click();
    const ToolConfig contourOffConfig = dialog.toToolConfig();
    check(!contourOffConfig.params.value(QStringLiteral("colorComparison"))
          .toObject().value(QStringLiteral("positionCorrection")).toObject()
          .value(QStringLiteral("showMatchContour")).toBool(true),
          "contour switch state must be serialized");
    dialog.loadFromConfig(contourOffConfig);
    check(!dialog.m_showPositionCorrectionMatchContour
          && !dialog.m_positionCorrectionContourCheckBox->isChecked(),
          "saved contour switch state must be restored");
    dialog.setAllParamsMode(false);

    const cv::Mat testFrame(16, 16, CV_8UC3, cv::Scalar(0, 0, 0));
    const ToolRequest testRequest = dialog.makeTestRequest(
                testFrame,
                FrameInputMetadata::fromMat(testFrame, QStringLiteral("smoke")));
    check(testRequest.runtimeContext.value(
              QStringLiteral("referencePositionCorrection")).isObject(),
          "dialog test request must carry the scheme reference position correction config");

    const QStringList comboObjectNames = {
        QStringLiteral("colorComparisonTemplateRegionModeCombo"),
        QStringLiteral("colorComparisonFeatureTypeCombo"),
        QStringLiteral("colorComparisonPositionCorrectionCombo"),
        QStringLiteral("colorComparisonSensitivityCombo")
    };
    for (const QString &objectName : comboObjectNames) {
        QComboBox *combo = dialog.findChild<QComboBox *>(objectName);
        check(combo != nullptr,
              qPrintable(QStringLiteral("missing combo: %1").arg(objectName)));
        check(combo
              && combo->property("uiRole").toString()
                 == QStringLiteral("lightField")
              && combo->view()
              && combo->view()->property("uiRole").toString()
                 == QStringLiteral("lightComboPopup")
              && combo->view()->viewport()
              && combo->view()->viewport()->property("uiRole").toString()
                 == QStringLiteral("lightComboPopupViewport")
              && combo->view()->parentWidget()
              && combo->view()->parentWidget()->property("uiRole").toString()
                 == QStringLiteral("lightComboPopupContainer"),
              qPrintable(QStringLiteral("combo popup role missing: %1")
                         .arg(objectName)));

        if (combo && objectName
                == QStringLiteral("colorComparisonTemplateRegionModeCombo")) {
            combo->showPopup();
            QApplication::processEvents();
            const QImage popupImage = combo->view()->viewport()->grab().toImage();
            int lightPixels = 0;
            const int pixelCount = popupImage.width() * popupImage.height();
            for (int y = 0; y < popupImage.height(); ++y) {
                for (int x = 0; x < popupImage.width(); ++x) {
                    if (qGray(popupImage.pixel(x, y)) >= 150)
                        ++lightPixels;
                }
            }
            check(!popupImage.isNull() && pixelCount > 0,
                  "combo popup must render in the offscreen smoke");
            check(pixelCount > 0 && lightPixels * 100 / pixelCount >= 65,
                  "light combo popup must not render as a dark or gray panel");
            combo->hidePopup();
        }
    }

    const auto checkedDetectDrawingButtons = [&dialog]() {
        return (dialog.m_detectRectButton->isChecked() ? 1 : 0)
                + (dialog.m_detectCircleButton->isChecked() ? 1 : 0);
    };
    check(dialog.m_editState == ColorComparisonDialog::EditState::None,
          "saved rectangle ROI must not imply active drawing");
    check(checkedDetectDrawingButtons() == 0,
          "detection drawing buttons must be idle initially");

    dialog.m_detectRectButton->click();
    QApplication::processEvents();
    check(dialog.m_editState == ColorComparisonDialog::EditState::DetectRect
          && dialog.m_detectRectButton->isChecked()
          && dialog.m_previewHelper->isRoiDrawingEnabled(),
          "first rectangle click must enter and highlight rectangle drawing");

    const QRectF redrawnRect(0.18, 0.22, 0.31, 0.27);
    dialog.handleRoiChanged(redrawnRect);
    check(dialog.m_editState == ColorComparisonDialog::EditState::DetectRect
          && dialog.m_previewHelper->isRoiDrawingEnabled(),
          "finishing one rectangle must keep continuous drawing active");
    dialog.m_detectRectButton->click();
    check(dialog.m_editState == ColorComparisonDialog::EditState::None
          && !dialog.m_detectRectButton->isChecked()
          && !dialog.m_previewHelper->isRoiDrawingEnabled(),
          "second rectangle click must exit drawing");
    check(dialog.m_detectRoi == redrawnRect,
          "exiting rectangle drawing must preserve the last ROI");

    // Isolate the remaining checks even when running against the pre-fix RED
    // implementation, which cannot exit by clicking the active tool.
    dialog.setEditState(ColorComparisonDialog::EditState::None);
    dialog.m_detectRectButton->click();
    dialog.m_detectCircleButton->click();
    check(dialog.m_editState == ColorComparisonDialog::EditState::DetectCircle
          && !dialog.m_detectRectButton->isChecked()
          && dialog.m_detectCircleButton->isChecked()
          && !dialog.m_previewHelper->isRoiDrawingEnabled()
          && dialog.m_previewHelper->isCircleDrawingEnabled()
          && checkedDetectDrawingButtons() == 1,
          "switching rectangle to circle must leave only circle drawing active");

    CircleRoi drawnCircle;
    drawnCircle.centerNormalized = QPointF(0.55, 0.48);
    drawnCircle.radiusNormalized = 0.12;
    drawnCircle.valid = true;
    dialog.m_previewImage = QImage(640, 480, QImage::Format_RGB32);
    dialog.handleCircleChanged(drawnCircle);
    const CircleRoi savedCircle = dialog.m_detectCircle;
    dialog.m_detectCircleButton->click();
    check(dialog.m_editState == ColorComparisonDialog::EditState::None
          && !dialog.m_detectCircleButton->isChecked()
          && !dialog.m_previewHelper->isCircleDrawingEnabled(),
          "second circle click must exit drawing");
    check(dialog.m_detectCircle.valid == savedCircle.valid
          && dialog.m_detectCircle.centerNormalized
             == savedCircle.centerNormalized
          && qFuzzyCompare(dialog.m_detectCircle.radiusNormalized,
                           savedCircle.radiusNormalized),
          "exiting circle drawing must preserve the last circle ROI");

    dialog.setEditState(ColorComparisonDialog::EditState::None);
    dialog.m_detectRectButton->click();
    dialog.m_detectGlobalButton->click();
    check(dialog.m_globalDetection
          && dialog.m_editState == ColorComparisonDialog::EditState::None
          && checkedDetectDrawingButtons() == 0
          && !dialog.m_previewHelper->isRoiDrawingEnabled()
          && !dialog.m_previewHelper->isCircleDrawingEnabled(),
          "selecting global detection must exit every drawing mode");

    dialog.setEditState(ColorComparisonDialog::EditState::None);
    dialog.m_templateRectButton->click();
    check(dialog.m_editState == ColorComparisonDialog::EditState::TemplateRect
          && dialog.m_templateRectButton->isChecked(),
          "template rectangle icon must enter drawing");
    const QRectF savedTemplateRoi = dialog.m_templateRoi;
    dialog.m_templateRectButton->click();
    check(dialog.m_editState == ColorComparisonDialog::EditState::None
          && !dialog.m_templateRectButton->isChecked()
          && dialog.m_templateRoi == savedTemplateRoi,
          "second template rectangle click must exit without clearing ROI");

    const QVector<QPointF> polygon = {
        QPointF(0.2, 0.2), QPointF(0.7, 0.2), QPointF(0.5, 0.7)
    };
    dialog.setEditState(ColorComparisonDialog::EditState::None);
    dialog.m_templateMaskPolygonButton->click();
    check(dialog.m_editState
          == ColorComparisonDialog::EditState::TemplateMaskPolygon
          && dialog.m_previewHelper->isPolygonDrawingEnabled(),
          "template mask icon must enter polygon drawing");
    dialog.m_previewHelper->setPolygonDrawingEnabled(false);
    dialog.handlePolygonChanged(polygon);
    check(!dialog.m_previewHelper->isPolygonDrawingEnabled()
          && dialog.m_previewHelper->polygonRoiNormalized() == polygon,
          "completed template mask must stay available for vertex editing");
    dialog.m_templateMaskRedrawButton->click();
    check(dialog.m_previewHelper->isPolygonDrawingEnabled()
          && dialog.m_templateMask == polygon,
          "template mask redraw must preserve the saved polygon until completion");
    dialog.m_previewHelper->setPolygonDrawingEnabled(false);
    dialog.m_templateMaskPolygonButton->click();
    check(dialog.m_editState == ColorComparisonDialog::EditState::None
          && !dialog.m_templateMaskPolygonButton->isChecked()
          && dialog.m_templateMask == polygon,
          "second template mask click must exit and preserve polygon");

    dialog.setEditState(ColorComparisonDialog::EditState::None);
    dialog.m_detectMaskPolygonButton->click();
    check(dialog.m_editState
          == ColorComparisonDialog::EditState::DetectMaskPolygon
          && dialog.m_previewHelper->isPolygonDrawingEnabled(),
          "detection mask icon must enter polygon drawing");
    dialog.m_previewHelper->setPolygonDrawingEnabled(false);
    dialog.handlePolygonChanged(polygon);
    check(!dialog.m_previewHelper->isPolygonDrawingEnabled()
          && dialog.m_previewHelper->polygonRoiNormalized() == polygon,
          "completed detection mask must stay available for vertex editing");
    dialog.m_detectMaskPolygonButton->click();
    check(dialog.m_editState == ColorComparisonDialog::EditState::None
          && !dialog.m_detectMaskPolygonButton->isChecked()
          && dialog.m_detectMask == polygon,
          "second detection mask click must exit and preserve polygon");

    dialog.m_model.state = ColorComparisonModelState::Ready;
    dialog.m_modelStatus = QStringLiteral("ok");
    dialog.m_templateRegionMode = QStringLiteral("sync");
    dialog.handleDetectionMaskChanged(QStringLiteral("detect_mask_changed"));
    check(dialog.m_model.state == ColorComparisonModelState::Ready,
          "detection mask must not stale a synchronized template model");
    dialog.handleDetectionGeometryChanged(QStringLiteral("detect_roi_changed"));
    check(dialog.m_model.state == ColorComparisonModelState::Stale,
          "detection geometry must stale a synchronized template model");

    dialog.m_templateRegionMode = QStringLiteral("sync");
    dialog.refreshTemplateRegionControls();
    check(!dialog.m_templateRectButton->isEnabled()
          && dialog.m_templateSyncHintLabel->isVisible(),
          "sync mode must disable template rectangle and explain independent mask ownership");
    dialog.setEditState(ColorComparisonDialog::EditState::TemplateRect);
    check(dialog.m_editState != ColorComparisonDialog::EditState::TemplateRect,
          "sync mode must reject template rectangle activation beyond button state");
    dialog.m_templateRegionMode = QStringLiteral("custom");
    dialog.refreshTemplateRegionControls();
    check(dialog.m_templateRectButton->isEnabled(),
          "custom mode must restore template rectangle editing");

    dialog.m_previewImage = QImage(640, 480, QImage::Format_RGB32);
    dialog.m_templateRoi = QRectF(0.05, 0.05, 0.25, 0.30);
    dialog.m_templateMask = polygon;
    dialog.m_detectRegionType = QStringLiteral("rectangle");
    dialog.m_globalDetection = false;
    dialog.m_detectRoi = QRectF(0.40, 0.15, 0.30, 0.35);
    dialog.m_detectMask = {
        QPointF(0.45, 0.20), QPointF(0.60, 0.20), QPointF(0.55, 0.35)
    };
    dialog.m_liveTestSource = ColorComparisonDialog::LiveTestSource::None;
    dialog.setEditState(ColorComparisonDialog::EditState::None);
    const QVector<ToolOverlay> configuredOverlays =
            dialog.configurationGeometryOverlays();
    int templateOverlays = 0;
    int detectOverlays = 0;
    int maskOverlays = 0;
    for (const ToolOverlay &overlay : configuredOverlays) {
        const QString owner = overlay.extra.value(
                    QStringLiteral("owner")).toString();
        if (owner == QStringLiteral("template"))
            ++templateOverlays;
        if (owner == QStringLiteral("detect"))
            ++detectOverlays;
        if (overlay.extra.value(QStringLiteral("displayRole")).toString()
                .endsWith(QStringLiteral("_mask"))) {
            ++maskOverlays;
            check(!overlay.extra.value(QStringLiteral("clipGeometry"))
                  .toObject().isEmpty(),
                  "each mask overlay must be clipped by its owner ROI");
        }
    }
    check(templateOverlays >= 3 && detectOverlays >= 3 && maskOverlays == 2,
          "configuration view must retain template/detection ROI and masks together");
    dialog.m_liveTestSource = ColorComparisonDialog::LiveTestSource::Camera;
    const QVector<ToolOverlay> detectionOverlays =
            dialog.detectionGeometryOverlays();
    bool testContainsTemplate = false;
    for (const ToolOverlay &overlay : detectionOverlays) {
        if (overlay.extra.value(QStringLiteral("owner")).toString()
                == QStringLiteral("template"))
            testContainsTemplate = true;
    }
    check(!testContainsTemplate && !detectionOverlays.isEmpty(),
          "test frame geometry must contain detection owner only");

    ToolOverlay correctedRuntimeRoi;
    correctedRuntimeRoi.type = ToolOverlayType::Polygon;
    correctedRuntimeRoi.label = QStringLiteral("Detection ROI");
    correctedRuntimeRoi.points = {
        QPointF(320.0, 140.0), QPointF(470.0, 180.0),
        QPointF(430.0, 340.0), QPointF(280.0, 300.0)
    };
    correctedRuntimeRoi.extra.insert(QStringLiteral("role"),
                                     QStringLiteral("detect_roi"));
    ToolOverlay correctedRuntimeMask;
    correctedRuntimeMask.type = ToolOverlayType::Polygon;
    correctedRuntimeMask.label = QStringLiteral("Detection Mask");
    correctedRuntimeMask.points = {
        QPointF(340.0, 190.0), QPointF(390.0, 205.0),
        QPointF(360.0, 250.0)
    };
    correctedRuntimeMask.extra.insert(QStringLiteral("role"),
                                      QStringLiteral("detect_mask"));
    dialog.m_runtimeResultOverlays = {
        correctedRuntimeRoi, correctedRuntimeMask
    };
    const QVector<ToolOverlay> correctedDisplay =
            dialog.combinedDisplayOverlays();
    bool retainedRuntimeRoi = false;
    bool retainedConfiguredGeometry = false;
    for (const ToolOverlay &overlay : correctedDisplay) {
        if (overlay.extra.value(QStringLiteral("role")).toString()
                == QStringLiteral("detect_roi")
                && overlay.points == correctedRuntimeRoi.points) {
            retainedRuntimeRoi = true;
        }
        if (overlay.extra.value(QStringLiteral("owner")).toString()
                == QStringLiteral("detect")) {
            retainedConfiguredGeometry = true;
        }
    }
    check(retainedRuntimeRoi && !retainedConfiguredGeometry,
          "successful test display must prefer the Runner-corrected ROI over configured geometry");

    dialog.m_runtimeResultOverlays.clear();
    const QVector<ToolOverlay> failedDisplayFallback =
            dialog.combinedDisplayOverlays();
    bool retainedFailureReferenceRoi = false;
    for (const ToolOverlay &overlay : failedDisplayFallback) {
        if (overlay.extra.value(QStringLiteral("owner")).toString()
                == QStringLiteral("detect")) {
            retainedFailureReferenceRoi = true;
        }
    }
    check(retainedFailureReferenceRoi,
          "failed test without a runtime ROI must retain configured geometry for diagnostics");
    dialog.m_liveTestSource = ColorComparisonDialog::LiveTestSource::None;

    dialog.m_model.state = ColorComparisonModelState::Ready;
    dialog.m_templateRegionMode = QStringLiteral("custom");
    const QRectF templateBeforeClear = dialog.m_templateRoi;
    dialog.clearTemplateMask();
    check(dialog.m_templateMask.isEmpty()
          && dialog.m_templateRoi == templateBeforeClear
          && dialog.m_model.state == ColorComparisonModelState::Stale,
          "clearing template mask must preserve ROI and stale the template model");
    dialog.m_model.state = ColorComparisonModelState::Ready;
    dialog.m_templateRegionMode = QStringLiteral("sync");
    const QRectF detectBeforeClear = dialog.m_detectRoi;
    dialog.clearDetectionMask();
    check(dialog.m_detectMask.isEmpty()
          && dialog.m_detectRoi == detectBeforeClear
          && dialog.m_model.state == ColorComparisonModelState::Ready,
          "clearing detection mask must preserve ROI without staling sync template");

    dialog.setEditState(ColorComparisonDialog::EditState::None);
    dialog.m_detectRegionType = QStringLiteral("circle");
    dialog.runReferenceTest();
    check(dialog.m_editState == ColorComparisonDialog::EditState::None
          && checkedDetectDrawingButtons() == 0,
          "test mode must not activate drawing from the saved ROI type");

    ToolResult valid = validDiagnosticResult();
    check(dialog.updateDetectionFeaturePreview(valid),
          "valid runtime histogram diagnostics must refresh the feature view");
    QLabel *state = dialog.findChild<QLabel *>(
                QStringLiteral("colorComparisonHistogramState"));
    QLabel *metrics = dialog.findChild<QLabel *>(
                QStringLiteral("colorComparisonHistogramMetrics"));
    check(state && state->text().contains(QStringLiteral("最新检测特征")),
          "valid diagnostics must expose the latest-detection state");
    check(metrics && metrics->text().contains(QStringLiteral("42.0%"))
          && metrics->text().contains(QStringLiteral("81.3"))
          && metrics->text().contains(QStringLiteral("0.750"))
          && metrics->text().contains(QStringLiteral("73.5"))
          && metrics->text().contains(QStringLiteral("scale_out_of_range")),
          "valid diagnostics must expose raw, smoothed, penalty and final metrics");

    cv::Mat source(20, 30, CV_8UC3, cv::Scalar(3, 17, 91));
    ReferenceImageProvider::instance().setReferenceFrame(source);
    dialog.m_templateRegionMode = QStringLiteral("custom");
    dialog.m_templateRoi = QRectF(0.2, 0.25, 0.5, 0.5);
    dialog.m_templateMask = {
        QPointF(0.2, 0.25), QPointF(0.7, 0.25), QPointF(0.7, 0.75)
    };
    const QImage rawRoi = dialog.templateRawRoiImage();
    check(rawRoi.size() == QSize(15, 10),
          "raw ROI thumbnail must use source-image normalized coordinates");
    const QColor sourceColor(91, 17, 3);
    check(imageHasOnlyColor(rawRoi, sourceColor),
          "ROI border and mask overlays must never modify thumbnail source pixels");

    dialog.m_templateRegionMode = QStringLiteral("sync");
    dialog.m_globalDetection = false;
    dialog.m_detectRegionType = QStringLiteral("rectangle");
    dialog.m_detectRoi = QRectF(0.1, 0.2, 0.4, 0.5);
    dialog.m_detectMask = {
        QPointF(0.1, 0.2), QPointF(0.5, 0.2), QPointF(0.5, 0.7)
    };
    const QImage syncRectRoi = dialog.templateRawRoiImage();
    check(syncRectRoi.size() == QSize(12, 10)
          && imageHasOnlyColor(syncRectRoi, sourceColor),
          "synchronized rectangle thumbnail must preserve raw source pixels");

    dialog.m_detectRegionType = QStringLiteral("circle");
    dialog.m_detectCircle.centerNormalized = QPointF(0.5, 0.5);
    dialog.m_detectCircle.radiusNormalized = 0.1;
    dialog.m_detectCircle.valid = true;
    const QImage syncCircleRoi = dialog.templateRawRoiImage();
    check(syncCircleRoi.size() == QSize(6, 6)
          && imageHasOnlyColor(syncCircleRoi, sourceColor),
          "synchronized circle thumbnail must use its raw bounding rectangle");
    ReferenceImageProvider::instance().clearReferenceFrame();

    ToolResult unavailable;
    unavailable.status = QStringLiteral("no_measurement");
    unavailable.payload.insert(QStringLiteral("histogramDiagnostics"),
                               QJsonObject{{QStringLiteral("available"), false}});
    check(!dialog.updateDetectionFeaturePreview(unavailable),
          "unavailable diagnostics must be rejected");
    check(state && state->text().contains(QStringLiteral("检测特征不可用")),
          "unavailable diagnostics must replace the previous valid state");
    check(metrics && metrics->text().contains(QStringLiteral("--")),
          "unavailable diagnostics must clear stale metrics");

    if (failures == 0)
        std::cout << "color comparison dialog integration smoke passed"
                  << std::endl;
    return failures == 0 ? 0 : 1;
}
