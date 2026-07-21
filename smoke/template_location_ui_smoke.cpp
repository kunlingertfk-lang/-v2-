#include "PlanDialogUtils.h"
#include "TemplateLocationDialog.h"
#include "ToolLibraryDialog.h"
#include "frame/ReferenceImageProvider.h"

#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QGraphicsView>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>

#include <opencv2/imgproc.hpp>

#include <iostream>
#include <cmath>

namespace PlanDialogUtils {
void applyLargeWindow(QWidget *)
{
}

void centerWindowOnScreen(QWidget *, QWidget *, int)
{
}
}

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << "FAIL: " << message << std::endl;
    ++failures;
}

QJsonObject rectJson(double x, double y, double width, double height)
{
    return QJsonObject{{QStringLiteral("x"), x},
                       {QStringLiteral("y"), y},
                       {QStringLiteral("width"), width},
                       {QStringLiteral("height"), height}};
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    cv::Mat reference(360, 520, CV_8UC3, cv::Scalar(28, 28, 28));
    cv::rectangle(reference, cv::Rect(170, 105, 128, 102),
                  cv::Scalar(225, 225, 225), -1);
    cv::rectangle(reference, cv::Rect(184, 119, 43, 29),
                  cv::Scalar(42, 42, 42), -1);
    cv::circle(reference, cv::Point(264, 174), 17,
               cv::Scalar(60, 60, 60), -1);
    cv::line(reference, cv::Point(190, 190), cv::Point(240, 155),
             cv::Scalar(250, 250, 250), 5);
    ReferenceImageProvider::instance().setReferenceFrame(reference);
    TemplateLocationDialog dialog;

    ToolConfig input;
    input.toolId = QStringLiteral("template-location-stable-id");
    input.toolName = QStringLiteral("模板定位");
    input.toolType = ToolType::TemplateLocation;
    input.category = ToolCategory::Location;
    input.params = QJsonObject{
        {QStringLiteral("templateRegionType"), QStringLiteral("polygon")},
        {QStringLiteral("templateRoiNormalized"), rectJson(0.2, 0.2, 0.3, 0.4)},
        {QStringLiteral("templatePolygonNormalized"), QJsonArray{
             QJsonObject{{QStringLiteral("x"), 0.2}, {QStringLiteral("y"), 0.2}},
             QJsonObject{{QStringLiteral("x"), 0.5}, {QStringLiteral("y"), 0.2}},
             QJsonObject{{QStringLiteral("x"), 0.4}, {QStringLiteral("y"), 0.6}}}},
        {QStringLiteral("searchRegionType"), QStringLiteral("rectangle")},
        {QStringLiteral("searchRoiNormalized"), rectJson(0.1, 0.1, 0.8, 0.8)},
        {QStringLiteral("minScore"), 63},
        {QStringLiteral("angleStart"), -30},
        {QStringLiteral("angleEnd"), 35},
        {QStringLiteral("scaleMin"), 90},
        {QStringLiteral("scaleMax"), 112},
        {QStringLiteral("polarity"), QStringLiteral("ignore_local_polarity")},
        {QStringLiteral("contrastMode"), QStringLiteral("auto")},
        {QStringLiteral("contrast"), 48},
        {QStringLiteral("minContrast"), 12},
        {QStringLiteral("numLevels"), 0},
        {QStringLiteral("subPixel"), QStringLiteral("least_squares")},
        {QStringLiteral("greediness"), 0.6},
        {QStringLiteral("timeoutMs"), 2400},
        {QStringLiteral("maxMatches"), 4},
        {QStringLiteral("modelCacheKey"), QStringLiteral("stable-model-cache-key")},
        {QStringLiteral("modelCreated"), true}
    };

    dialog.loadFromConfig(input);
    const ToolConfig output = dialog.toolConfig();
    check(output.toolId == input.toolId, "edit round trip preserves the stable tool id");
    check(output.toolType == ToolType::TemplateLocation,
          "dialog produces TemplateLocation tool type");
    check(output.category == ToolCategory::Location,
          "dialog produces Location category");
    check(output.params.value(QStringLiteral("templateRegionType")).toString() ==
              QStringLiteral("polygon"),
          "template polygon mode round trips");
    check(output.params.value(QStringLiteral("minScore")).toInt() == 63 &&
              output.params.value(QStringLiteral("angleStart")).toInt() == -30 &&
              output.params.value(QStringLiteral("angleEnd")).toInt() == 35,
          "score and angle range round trip");
    check(output.params.value(QStringLiteral("modelCacheKey")).toString() ==
              QStringLiteral("stable-model-cache-key") &&
              output.params.value(QStringLiteral("modelCreated")).toBool(),
          "model cache key and valid state round trip");
    check(output.params.value(QStringLiteral("maxMatches")).toInt() == 4,
          "maximum match count round trips");
    check(output.params.value(QStringLiteral("minMatchCount")).toInt() == 1 &&
              output.params.value(QStringLiteral("maxMatchCount")).toInt() == 4 &&
              output.params.value(QStringLiteral("maxOverlap")).toInt() == 50,
          "legacy configs receive quantity judgment and MaxOverlap defaults");
    check(output.params.value(QStringLiteral("originMode")).toString() ==
              QStringLiteral("centroid"),
          "template centroid is the default output origin");
    QPushButton *createTemplate = dialog.findChild<QPushButton *>(
                QStringLiteral("createTemplateButton"));
    QPushButton *deleteTemplate = dialog.findChild<QPushButton *>(
                QStringLiteral("deleteTemplateButton"));
    check(createTemplate && createTemplate->text() == QStringLiteral("重新创建模板") &&
              deleteTemplate && deleteTemplate->isEnabled(),
          "loaded model exposes rebuild and delete actions");
    if (deleteTemplate)
        deleteTemplate->click();
    check(!dialog.toolConfig().params.value(QStringLiteral("modelCreated")).toBool() &&
              deleteTemplate && !deleteTemplate->isEnabled() &&
              createTemplate && createTemplate->text() == QStringLiteral("创建模板"),
          "deleting the model clears its valid state and keeps recreation available");

    ToolConfig creationInput = input;
    creationInput.params.insert(QStringLiteral("templateRegionType"),
                                QStringLiteral("rectangle"));
    creationInput.params.insert(QStringLiteral("templateRoiNormalized"),
                                rectJson(155.0 / 520.0, 90.0 / 360.0,
                                         160.0 / 520.0, 135.0 / 360.0));
    creationInput.params.insert(QStringLiteral("templatePolygonNormalized"), QJsonArray());
    creationInput.params.insert(QStringLiteral("searchRegionType"), QStringLiteral("full"));
    creationInput.params.insert(QStringLiteral("searchRoiNormalized"),
                                rectJson(0.0, 0.0, 1.0, 1.0));
    creationInput.params.insert(QStringLiteral("minScore"), 45);
    creationInput.params.insert(QStringLiteral("modelCreated"), false);
    dialog.loadFromConfig(creationInput);
    if (createTemplate)
        createTemplate->click();
    const ToolPreviewSnapshot creationPreview = dialog.referencePreviewSnapshot();
    bool hasOrangeTemplateRole = false;
    for (const ToolOverlay &overlay : creationPreview.result.overlays) {
        if (overlay.label == QStringLiteral("template_model") &&
                overlay.extra.value(QStringLiteral("displayRole")).toString() ==
                    QStringLiteral("template_location_model")) {
            hasOrangeTemplateRole = true;
            break;
        }
    }
    check(dialog.toolConfig().params.value(QStringLiteral("modelCreated")).toBool() &&
              hasOrangeTemplateRole,
          "creating a model immediately exposes the dedicated template contour overlay");
    if (deleteTemplate)
        deleteTemplate->click();

    QFrame *advancedCard = dialog.findChild<QFrame *>(QStringLiteral("advancedCard"));
    QPushButton *allButton = dialog.findChild<QPushButton *>(QStringLiteral("allModeButton"));
    QComboBox *contrastMode = dialog.findChild<QComboBox *>(QStringLiteral("contrastModeComboBox"));
    QSpinBox *contrast = dialog.findChild<QSpinBox *>(QStringLiteral("contrastSpinBox"));
    QSpinBox *minContrast = dialog.findChild<QSpinBox *>(QStringLiteral("minContrastSpinBox"));
    QSpinBox *maxMatches = dialog.findChild<QSpinBox *>(QStringLiteral("maxMatchesSpinBox"));
    QSpinBox *minMatchCount = dialog.findChild<QSpinBox *>(QStringLiteral("minMatchCountSpinBox"));
    QSpinBox *maxMatchCount = dialog.findChild<QSpinBox *>(QStringLiteral("maxMatchCountSpinBox"));
    QSpinBox *maxOverlap = dialog.findChild<QSpinBox *>(QStringLiteral("maxOverlapSpinBox"));
    QLabel *contrastLabel = dialog.findChild<QLabel *>(QStringLiteral("contrastLabel"));
    QLabel *minContrastLabel = dialog.findChild<QLabel *>(QStringLiteral("minContrastLabel"));
    QLabel *autoValue = dialog.findChild<QLabel *>(QStringLiteral("autoContrastValueLabel"));
    check(advancedCard && advancedCard->isHidden(), "basic page hides advanced parameters");
    if (allButton)
        allButton->click();
    check(advancedCard && !advancedCard->isHidden(), "all page shows advanced parameters");
    check(contrastMode && contrastMode->currentIndex() == 0,
          "automatic contrast is restored");
    check(contrast && minContrast && !contrast->isEnabled() && !minContrast->isEnabled(),
          "automatic mode locks manual contrast fields");
    check(contrastLabel && minContrastLabel && !contrastLabel->isEnabled() &&
              !minContrastLabel->isEnabled(),
          "automatic mode grays both manual contrast labels");
    check(autoValue && !autoValue->isHidden(), "automatic result field is visible");
    check(maxMatches && maxMatches->minimum() == 1 && maxMatches->maximum() == 100,
          "maximum match count is configurable from 1 to 100");
    check(minMatchCount && maxMatchCount && maxOverlap &&
              maxMatchCount->maximum() == maxMatches->value() && maxOverlap->value() == 50,
          "quantity judgment is bounded by the search limit and MaxOverlap is exposed");
    if (contrastMode)
        contrastMode->setCurrentIndex(1);
    check(contrast && minContrast && contrast->isEnabled() && minContrast->isEnabled(),
          "manual mode enables Contrast and MinContrast");
    check(autoValue && autoValue->isHidden(), "manual mode hides automatic result field");

    const ToolConfig dirtyOutput = dialog.toolConfig();
    check(!dirtyOutput.params.value(QStringLiteral("modelCreated")).toBool(),
          "changing a model parameter invalidates the model");
    QGraphicsView *preview = dialog.findChild<QGraphicsView *>(QStringLiteral("previewGraphicsView"));
    check(preview && preview->backgroundBrush().color() == QColor(0, 0, 0),
          "image workspace uses a black canvas");
    check(dialog.findChild<QToolButton *>(QStringLiteral("templateRectButton")) != nullptr &&
              dialog.findChild<QToolButton *>(QStringLiteral("templatePolygonButton")) != nullptr &&
              dialog.findChild<QToolButton *>(QStringLiteral("searchRectButton")) != nullptr &&
              dialog.findChild<QToolButton *>(QStringLiteral("searchCircleButton")) != nullptr &&
              dialog.findChild<QToolButton *>(QStringLiteral("searchPolygonButton")) != nullptr,
          "template and search ROI controls include the polygon mode");
    QGridLayout *templateLayout = dialog.findChild<QGridLayout *>(QStringLiteral("templateLayout"));
    QGridLayout *searchLayout = dialog.findChild<QGridLayout *>(QStringLiteral("searchLayout"));
    check(templateLayout && searchLayout && templateLayout->columnStretch(0) == 1 &&
              searchLayout->columnStretch(0) == 1,
          "ROI label columns absorb free width so icon groups stay right aligned");
    QToolButton *globalSearch = dialog.findChild<QToolButton *>(QStringLiteral("searchGlobalButton"));
    QToolButton *searchRectangle = dialog.findChild<QToolButton *>(QStringLiteral("searchRectButton"));
    QToolButton *searchCircle = dialog.findChild<QToolButton *>(QStringLiteral("searchCircleButton"));
    QToolButton *searchPolygon = dialog.findChild<QToolButton *>(QStringLiteral("searchPolygonButton"));
    check(globalSearch && searchRectangle && searchCircle && searchPolygon &&
              !globalSearch->icon().isNull() && !searchRectangle->icon().isNull() &&
              !searchCircle->icon().isNull() && !searchPolygon->icon().isNull() &&
              globalSearch->text().isEmpty() && searchRectangle->text().isEmpty() &&
              searchCircle->text().isEmpty() && searchPolygon->text().isEmpty(),
          "global, rectangle, circle and polygon use the unified vector icon family");
    check(globalSearch && globalSearch->isChecked(),
          "global search is visibly selected by default");
    QFrame *resultBar = dialog.findChild<QFrame *>(QStringLiteral("resultBar"));
    QHBoxLayout *resultLayout = dialog.findChild<QHBoxLayout *>(QStringLiteral("resultLayout"));
    QFrame *matchDrawer = dialog.findChild<QFrame *>(QStringLiteral("matchResultDrawer"));
    QPushButton *matchToggle = dialog.findChild<QPushButton *>(QStringLiteral("matchResultToggle"));
    QTableWidget *matchTable = dialog.findChild<QTableWidget *>(QStringLiteral("matchResultTable"));
    check(resultBar && resultBar->minimumHeight() == 48 && resultBar->maximumHeight() == 48,
          "result bar is limited to a compact two-line height");
    check(resultLayout && resultLayout->contentsMargins().top() == 2 &&
              resultLayout->contentsMargins().bottom() == 2 &&
              resultLayout->stretch(0) == 1,
          "result text receives enough height and horizontal space without being clipped");
    check(matchTable && matchTable->columnCount() == 6,
          "multi-match table exposes index, pose, scale and score columns");
    check(matchDrawer && matchToggle && matchTable && matchDrawer->height() == 36 &&
              matchTable->isHidden(),
          "match results default to a compact collapsed summary drawer");
    if (matchToggle)
        matchToggle->click();
    check(matchDrawer && matchTable && matchDrawer->height() == 152 &&
              !matchTable->isHidden(),
          "expanded result drawer keeps a bounded scrollable table height");
    if (matchToggle)
        matchToggle->click();
    check(deleteTemplate && deleteTemplate->width() == 88 &&
              searchPolygon && searchPolygon->width() == 42,
          "template commands and ROI icons use the compact aligned widths");

    FrameViewHelper *previewHelper = dialog.findChild<FrameViewHelper *>();
    if (searchPolygon)
        searchPolygon->click();
    check(searchPolygon && searchPolygon->isChecked() && previewHelper &&
              previewHelper->isPolygonDrawingEnabled(),
          "polygon search stays highlighted while drawing");
    if (previewHelper) {
        previewHelper->polygonChanged(QVector<QPointF>{QPointF(0.1, 0.1),
                                                       QPointF(0.8, 0.1),
                                                       QPointF(0.5, 0.8)});
    }
    check(searchPolygon && searchPolygon->isChecked() && previewHelper &&
              previewHelper->isPolygonDrawingEnabled() &&
              dialog.toolConfig().params.value(QStringLiteral("searchRegionType")).toString() ==
                  QStringLiteral("polygon"),
          "completed polygon search immediately returns to continuous redraw mode");
    if (searchPolygon)
        searchPolygon->click();
    check(searchPolygon && searchPolygon->isChecked() && previewHelper &&
              previewHelper->isPolygonDrawingEnabled(),
          "clicking the selected search mode does not cancel it");

    QComboBox *originMode = dialog.findChild<QComboBox *>(QStringLiteral("originModeComboBox"));
    QPushButton *selectOrigin = dialog.findChild<QPushButton *>(QStringLiteral("selectOriginButton"));
    if (originMode)
        originMode->setCurrentIndex(1);
    if (selectOrigin)
        selectOrigin->click();
    check(originMode && originMode->currentIndex() == 1 && selectOrigin &&
              selectOrigin->isChecked() && previewHelper &&
              previewHelper->isPointSelectionEnabled(),
          "custom origin mode enables persistent point selection");
    if (previewHelper)
        previewHelper->pointSelected(QPointF(0.42, 0.37));
    const ToolConfig customOriginOutput = dialog.toolConfig();
    check(customOriginOutput.params.value(QStringLiteral("originMode")).toString() ==
              QStringLiteral("custom") &&
          std::abs(customOriginOutput.params.value(QStringLiteral("customOriginNormalized"))
                   .toObject().value(QStringLiteral("x")).toDouble() - 0.42) < 1e-6,
          "selected custom origin is saved as a normalized point");
    if (globalSearch)
        globalSearch->click();
    const ToolConfig globalOutput = dialog.toolConfig();
    check(globalOutput.params.value(QStringLiteral("searchRegionType")).toString() ==
              QStringLiteral("full") &&
          globalOutput.params.value(QStringLiteral("searchRoiNormalized")).toObject()
              .value(QStringLiteral("width")).toDouble() == 1.0,
          "global search switches the search domain to the full image");

    ToolConfig circleInput = dialog.toolConfig();
    circleInput.params.insert(QStringLiteral("searchRegionType"), QStringLiteral("circle"));
    circleInput.params.insert(QStringLiteral("searchRoiNormalized"),
                              rectJson(0.25, 0.20, 0.50, 0.60));
    circleInput.params.insert(QStringLiteral("searchCircleCenterNormalized"),
                              QJsonObject{{QStringLiteral("x"), 0.50},
                                          {QStringLiteral("y"), 0.50}});
    circleInput.params.insert(QStringLiteral("searchCircleRadiusNormalized"), 0.20);
    dialog.loadFromConfig(circleInput);
    const ToolConfig circleOutput = dialog.toolConfig();
    check(circleOutput.params.value(QStringLiteral("searchRegionType")).toString() ==
              QStringLiteral("circle") &&
          std::abs(circleOutput.params.value(
                       QStringLiteral("searchCircleRadiusNormalized")).toDouble() - 0.20) < 1e-6,
          "circle search geometry round trips through the dialog config");

    ToolLibraryDialog library;
    QToolButton *templateLocation = library.findChild<QToolButton *>(
                QStringLiteral("templateLocationButton"));
    check(templateLocation && templateLocation->isCheckable(),
          "tool library exposes a selectable template location entry");
    if (templateLocation)
        templateLocation->click();
    check(library.selectedTool() == ToolLibraryDialog::TemplateLocation,
          "template location entry is selected");
    QPushButton *confirm = library.findChild<QPushButton *>(QStringLiteral("confirmButton"));
    if (confirm)
        confirm->click();
    check(library.result() == QDialog::Accepted &&
              library.selectedToolType() == ToolType::TemplateLocation,
          "tool library confirms TemplateLocation type");

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "template_location_ui_smoke: all checks passed" << std::endl;
    return 0;
}
