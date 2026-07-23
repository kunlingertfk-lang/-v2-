#include "PositionCorrectionDialog.h"
#include "ToolLibraryDialog.h"
#include "PlanDialogUtils.h"
#include "frame/ReferenceImageProvider.h"

#include <QApplication>
#include <QFrame>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <iostream>

#include <opencv2/imgproc.hpp>

namespace PlanDialogUtils {
void centerWindowOnScreen(QWidget *, QWidget *, int)
{
}
}

namespace {
int failures = 0;
void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    }
}
}
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    cv::Mat reference(240, 320, CV_8UC3, cv::Scalar(32, 32, 32));
    cv::rectangle(reference, cv::Rect(90, 70, 80, 50),
                  cv::Scalar(255, 255, 255), cv::FILLED);
    ReferenceImageProvider::instance().setReferenceFrame(reference);
    PositionCorrectionDialog dialog;

    ToolConfig input;
    input.toolId = QStringLiteral("pc-stable-id");
    input.toolType = ToolType::PositionCorrection;
    input.category = ToolCategory::Location;
    ToolConfig upstream;
    upstream.toolId = QStringLiteral("upstream");
    upstream.toolType = ToolType::TemplateLocation;
    upstream.category = ToolCategory::Location;
    upstream.enabled = true;
    upstream.displayName = QStringLiteral("模板定位");
    QVector<ToolConfig> tools;
    tools.append(upstream);
    tools.append(input);
    QJsonObject correction;
    correction.insert(QStringLiteral("version"), 1);
    correction.insert(QStringLiteral("runPointX"), QJsonObject{
                          {QStringLiteral("producerId"), QStringLiteral("upstream")},
                          {QStringLiteral("outputKey"), QStringLiteral("x")},
                          {QStringLiteral("displayPath"), QStringLiteral("2 定位.匹配点X")}});
    correction.insert(QStringLiteral("runPointY"), QJsonObject{
                          {QStringLiteral("producerId"), QStringLiteral("upstream")},
                          {QStringLiteral("outputKey"), QStringLiteral("y")},
                          {QStringLiteral("displayPath"), QStringLiteral("2 定位.匹配点Y")}});
    correction.insert(QStringLiteral("runAngle"), QJsonObject{
                          {QStringLiteral("producerId"), QStringLiteral("upstream")},
                          {QStringLiteral("outputKey"), QStringLiteral("angle")},
                          {QStringLiteral("displayPath"), QStringLiteral("2 定位.匹配角度")}});
    correction.insert(QStringLiteral("templateRegionType"), QStringLiteral("polygon"));
    input.params.insert(QStringLiteral("positionCorrection"), correction);

    dialog.loadFromConfig(input);
    ReferencePositionCorrectionConfig referenceConfig;
    referenceConfig.enabled = true;
    referenceConfig.referenceCreated = true;
    referenceConfig.referencePose = QJsonObject{
        {QStringLiteral("x"), 160.25},
        {QStringLiteral("y"), 92.5},
        {QStringLiteral("angleDeg"), 7.25}
    };
    QVector<PositionReferencePoseProducer> referenceProducers{
        PositionReferencePoseProducer{
            PositionCorrection::defaultSourceId(),
            QStringLiteral("1 基准图"),
            referenceConfig.referencePose,
            PositionCorrection::referenceToJson(referenceConfig)}
    };
    dialog.setAvailableProducers(tools, 1, referenceProducers);
    const ToolConfig output = dialog.toolConfig();
    const QJsonObject outputCorrection = output.params.value(
                QStringLiteral("positionCorrection")).toObject();
    check(output.toolType == ToolType::PositionCorrection,
          "dialog must produce PositionCorrection tool type");
    check(output.category == ToolCategory::Location,
          "dialog must produce Location category");
    check(output.toolId == QStringLiteral("pc-stable-id"),
          "dialog edit round trip must preserve stable tool id");
    check(outputCorrection.value(QStringLiteral("runPointX")).toObject()
                  .value(QStringLiteral("producerId")).toString() == QStringLiteral("upstream"),
          "dialog must round trip stable producer binding");
    check(outputCorrection.value(QStringLiteral("version")).toInt() == 2,
          "dialog must migrate position correction config to version 2");
    check(outputCorrection.value(QStringLiteral("runPoseSource")).toObject()
                  .value(QStringLiteral("producerId")).toString() == QStringLiteral("upstream"),
          "dialog must write unified runPoseSource");
    check(outputCorrection.value(QStringLiteral("runPoseSource")).toObject()
                  .value(QStringLiteral("scaleKey")).toString() == QStringLiteral("scale"),
          "dialog must pass template scale implicitly without adding another binding control");
    check(outputCorrection.value(QStringLiteral("runPointX")).toObject()
                  .value(QStringLiteral("displayPath")).toString().contains(QStringLiteral("运行点X")),
          "dialog must display run pose semantics for X");
    check(outputCorrection.value(QStringLiteral("templateRegionType")).toString()
                  == QStringLiteral("polygon"),
          "dialog must round trip template region type");
    check(dialog.findChild<QLineEdit *>(QStringLiteral("runPointXEdit")) != nullptr,
          "dialog must expose run X field");
    check(dialog.findChild<QPushButton *>(QStringLiteral("testRunButton")) != nullptr,
          "dialog must expose test run button");
    QFrame *viewerHeader = dialog.findChild<QFrame *>(QStringLiteral("viewerHeader"));
    check(viewerHeader && viewerHeader->minimumHeight() == 44
          && viewerHeader->maximumHeight() == 44,
          "preview header must stay at a fixed compact height");
    QGraphicsView *previewView = dialog.findChild<QGraphicsView *>(
                QStringLiteral("previewGraphicsView"));
    check(previewView != nullptr, "dialog must expose preview graphics view");
    check(previewView && previewView->backgroundBrush().color() == QColor(0, 0, 0),
          "preview canvas must use a black background");

    QPushButton *xLinkButton = dialog.findChild<QPushButton *>(
                QStringLiteral("runPointXLinkButton"));
    QAction *referenceNodeAction = nullptr;
    if (xLinkButton && xLinkButton->menu()) {
        for (QAction *action : xLinkButton->menu()->actions()) {
            if (action->text() == QStringLiteral("1 基准图")) {
                referenceNodeAction = action;
                break;
            }
        }
    }
    check(referenceNodeAction && referenceNodeAction->menu(),
          "enabled reference pose must appear as a linkable source node");
    if (referenceNodeAction && referenceNodeAction->menu()
            && !referenceNodeAction->menu()->actions().isEmpty()) {
        referenceNodeAction->menu()->actions().first()->trigger();
    }
    QPushButton *createReferenceButton = dialog.findChild<QPushButton *>(
                QStringLiteral("createReferenceButton"));
    if (createReferenceButton)
        createReferenceButton->click();
    const QJsonObject importedCorrection = dialog.toolConfig().params
            .value(QStringLiteral("positionCorrection")).toObject();
    check(importedCorrection.value(QStringLiteral("runPoseSource")).toObject()
                  .value(QStringLiteral("producerId")).toString()
                  == PositionCorrection::defaultSourceId(),
          "reference pose selection must persist its stable source id");
    check(importedCorrection.value(QStringLiteral("referenceCreated")).toBool(false)
          && qAbs(importedCorrection.value(QStringLiteral("referencePose")).toObject()
                  .value(QStringLiteral("x")).toDouble() - 160.25) < 1e-9,
          "creating a baseline from a reference source must import its pose values");
    bool showsSource = false;
    bool showsReferencePose = false;
    if (previewView && previewView->scene()) {
        for (QGraphicsItem *item : previewView->scene()->items()) {
            QGraphicsSimpleTextItem *textItem =
                    qgraphicsitem_cast<QGraphicsSimpleTextItem *>(item);
            if (!textItem)
                continue;
            showsSource = showsSource
                    || textItem->text().contains(QStringLiteral("订阅来源"));
            showsReferencePose = showsReferencePose
                    || textItem->text().contains(QStringLiteral("基准位姿"));
        }
    }
    check(showsSource && showsReferencePose,
          "preview must show source and reference pose information overlays");

    ToolLibraryDialog library;
    QToolButton *positionCorrectionButton = library.findChild<QToolButton *>(
                QStringLiteral("positionCorrectionToolButton"));
    check(positionCorrectionButton != nullptr,
          "tool library must expose position correction button");
    check(positionCorrectionButton && positionCorrectionButton->isCheckable(),
          "position correction tool button must be selectable");
    if (positionCorrectionButton)
        positionCorrectionButton->click();
    check(library.selectedTool() == ToolLibraryDialog::PositionCorrectionTool,
          "clicking position correction must select it in the tool library");
    QPushButton *confirmButton = library.findChild<QPushButton *>(QStringLiteral("confirmButton"));
    check(confirmButton != nullptr, "tool library must expose confirm button");
    if (confirmButton)
        confirmButton->click();
    check(library.result() == QDialog::Accepted,
          "confirming position correction must accept the tool library");
    check(library.selectedToolType() == ToolType::PositionCorrection,
          "tool library must return PositionCorrection type");

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "position_correction_ui_smoke: all checks passed" << std::endl;
    return 0;
}
