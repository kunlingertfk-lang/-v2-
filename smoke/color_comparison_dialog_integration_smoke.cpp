#include <QApplication>
#include <QColor>
#include <QDialog>
#include <QFrame>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QRectF>
#include <QScrollArea>
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

    ColorComparisonDialog dialog;
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
    bool rawPixelsPreserved = !rawRoi.isNull();
    for (int y = 0; rawPixelsPreserved && y < rawRoi.height(); ++y) {
        for (int x = 0; x < rawRoi.width(); ++x) {
            const QColor pixel = rawRoi.pixelColor(x, y);
            if (pixel.red() != 91 || pixel.green() != 17 || pixel.blue() != 3) {
                rawPixelsPreserved = false;
                break;
            }
        }
    }
    check(rawPixelsPreserved,
          "ROI border and mask overlays must never modify thumbnail source pixels");
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
