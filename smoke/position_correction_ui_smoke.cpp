#include "PositionCorrectionDialog.h"
#include "ToolLibraryDialog.h"
#include "PlanDialogUtils.h"

#include <QApplication>
#include <QFrame>
#include <QGraphicsView>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include <iostream>

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
    PositionCorrectionDialog dialog;

    ToolConfig input;
    input.toolId = QStringLiteral("pc-stable-id");
    input.toolType = ToolType::PositionCorrection;
    input.category = ToolCategory::Location;
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
