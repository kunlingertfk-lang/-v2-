#include "toolcore/PositionCorrection.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <iostream>

namespace {

int g_failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++g_failures;
    }
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const PositionCorrectionConfig defaults =
            PositionCorrection::fromParams(QJsonObject());
    check(!defaults.enabled, "empty params must default position correction to disabled");
    check(defaults.source == PositionCorrection::defaultSource(),
          "empty params must default source to the common source text");

    QJsonObject params;
    params.insert(QStringLiteral("enablePositionCorrection"), true);
    params.insert(QStringLiteral("positionCorrectionSource"), QStringLiteral("2 定位.位置修正信息"));
    const PositionCorrectionConfig parsed = PositionCorrection::fromParams(params);
    check(parsed.enabled, "params must parse enablePositionCorrection=true");
    check(parsed.source == QStringLiteral("2 定位.位置修正信息"),
          "params must parse positionCorrectionSource");

    QJsonObject written;
    PositionCorrection::writeParams(PositionCorrectionConfig{
                                            true,
                                            QStringLiteral("3 基准.位置修正信息")},
                                    &written);
    check(written.value(QStringLiteral("enablePositionCorrection")).toBool(false),
          "writeParams must write enabled flag");
    check(written.value(QStringLiteral("positionCorrectionSource")).toString()
                  == QStringLiteral("3 基准.位置修正信息"),
          "writeParams must write source");

    QJsonObject payload;
    PositionCorrection::writeNotAppliedPayload(parsed, &payload);
    check(payload.value(QStringLiteral("enablePositionCorrection")).toBool(false),
          "payload must include requested enabled flag");
    check(payload.value(QStringLiteral("positionCorrectionSource")).toString()
                  == QStringLiteral("2 定位.位置修正信息"),
          "payload must include requested source");
    check(!payload.value(QStringLiteral("positionCorrectionApplied")).toBool(true),
          "payload must mark correction as not applied");
    check(payload.value(QStringLiteral("positionCorrectionReason")).toString()
                  == QStringLiteral("not implemented"),
          "payload must use common not implemented reason");

    if (g_failures > 0) {
        std::cerr << g_failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "position_correction_smoke: all checks passed" << std::endl;
    return 0;
}
