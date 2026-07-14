#include "PositionCorrectionDialog.h"

#include <QApplication>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
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

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "position_correction_ui_smoke: all checks passed" << std::endl;
    return 0;
}
