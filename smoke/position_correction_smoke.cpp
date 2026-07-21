#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolConfig.h"

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
    check(defaults.sourceId == PositionCorrection::defaultSourceId(),
          "empty params must default to the stable reference source id");
    check(toolTypeFromString(QStringLiteral("PositionCorrection")) == ToolType::PositionCorrection,
          "PositionCorrection tool type must parse from string");
    check(toolTypeToString(ToolType::PositionCorrection) == QStringLiteral("PositionCorrection"),
          "PositionCorrection tool type must serialize to string");

    QJsonObject params;
    params.insert(QStringLiteral("enablePositionCorrection"), true);
    params.insert(QStringLiteral("positionCorrectionSource"), QStringLiteral("2 定位.位置修正信息"));
    params.insert(QStringLiteral("positionCorrectionSourceId"), QStringLiteral("tool-position-2"));
    const PositionCorrectionConfig parsed = PositionCorrection::fromParams(params);
    check(parsed.enabled, "params must parse enablePositionCorrection=true");
    check(parsed.source == QStringLiteral("2 定位.位置修正信息"),
          "params must parse positionCorrectionSource");
    check(parsed.sourceId == QStringLiteral("tool-position-2"),
          "params must parse positionCorrectionSourceId");

    QJsonObject written;
    PositionCorrection::writeParams(PositionCorrectionConfig{
                                            true,
                                            QStringLiteral("3 基准.位置修正信息"),
                                            QStringLiteral("tool-position-3")},
                                    &written);
    check(written.value(QStringLiteral("enablePositionCorrection")).toBool(false),
          "writeParams must write enabled flag");
    check(written.value(QStringLiteral("positionCorrectionSource")).toString()
                  == QStringLiteral("3 基准.位置修正信息"),
          "writeParams must write source");
    check(written.value(QStringLiteral("positionCorrectionSourceId")).toString()
                  == QStringLiteral("tool-position-3"),
          "writeParams must write stable source id");

    QVector<ToolConfig> tools;
    ToolConfig firstCorrection;
    firstCorrection.toolId = QStringLiteral("pc-first");
    firstCorrection.toolType = ToolType::PositionCorrection;
    firstCorrection.category = ToolCategory::Location;
    firstCorrection.enabled = true;
    firstCorrection.displayName = QStringLiteral("位置修正");
    tools.append(firstCorrection);
    ToolConfig disabledCorrection = firstCorrection;
    disabledCorrection.toolId = QStringLiteral("pc-disabled");
    disabledCorrection.enabled = false;
    tools.append(disabledCorrection);
    ToolConfig consumer;
    consumer.toolId = QStringLiteral("consumer");
    consumer.toolType = ToolType::ColorComparison;
    consumer.category = ToolCategory::Recognition;
    tools.append(consumer);
    ToolConfig laterCorrection = firstCorrection;
    laterCorrection.toolId = QStringLiteral("pc-later");
    tools.append(laterCorrection);

    const QVector<PositionCorrectionSource> sources =
            PositionCorrection::sourcesBefore(tools, 2, true);
    check(sources.size() == 2,
          "source registry must include reference and one enabled preceding correction");
    check(sources.at(0).sourceId == PositionCorrection::defaultSourceId(),
          "reference correction must be the first source");
    check(sources.at(1).sourceId == QStringLiteral("pc-first"),
          "source registry must retain stable tool id");
    check(PositionCorrection::isSourceAvailable(sources, QStringLiteral("pc-first")),
          "registered preceding source must be available");
    check(!PositionCorrection::isSourceAvailable(sources, QStringLiteral("pc-later")),
          "later correction must not be available to the consumer");

    const ReferencePositionCorrectionConfig referenceDefaults =
            PositionCorrection::referenceFromJson(QJsonObject());
    check(!referenceDefaults.enabled && referenceDefaults.version == 1,
          "missing scheme reference correction must default to disabled version 1");
    ReferencePositionCorrectionConfig referenceConfig;
    referenceConfig.enabled = true;
    referenceConfig.templateRegionType = QStringLiteral("rectangle");
    referenceConfig.templateRoiNormalized = QRectF(0.1, 0.2, 0.3, 0.4);
    const QJsonObject referenceJson = PositionCorrection::referenceToJson(referenceConfig);
    const ReferencePositionCorrectionConfig referenceRoundTrip =
            PositionCorrection::referenceFromJson(referenceJson);
    check(referenceRoundTrip.enabled
          && referenceRoundTrip.templateRoiNormalized == referenceConfig.templateRoiNormalized,
          "scheme reference correction must round trip enabled state and template ROI");

    ReferencePositionCorrectionConfig polygonConfig;
    polygonConfig.enabled = true;
    polygonConfig.templateRegionType = QStringLiteral("polygon");
    polygonConfig.templateRoiNormalized = QRectF(0.1, 0.1, 0.7, 0.6);
    polygonConfig.templatePolygonNormalized = QJsonArray{
        QJsonObject{{QStringLiteral("x"), 0.1}, {QStringLiteral("y"), 0.2}},
        QJsonObject{{QStringLiteral("x"), 0.8}, {QStringLiteral("y"), 0.1}},
        QJsonObject{{QStringLiteral("x"), 0.6}, {QStringLiteral("y"), 0.7}}
    };
    const ReferencePositionCorrectionConfig polygonRoundTrip =
            PositionCorrection::referenceFromJson(
                PositionCorrection::referenceToJson(polygonConfig));
    check(polygonRoundTrip.templateRegionType == QStringLiteral("polygon")
          && polygonRoundTrip.templatePolygonNormalized == polygonConfig.templatePolygonNormalized
          && polygonRoundTrip.templateRoiNormalized == polygonConfig.templateRoiNormalized,
          "scheme reference correction must round trip polygon ROI and its bounding rect");

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
