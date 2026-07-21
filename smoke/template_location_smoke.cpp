#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/location/TemplateLocationHalconRunner.h"
#include "algorithms/presence/PatternPresenceHalconRunner.h"
#include "tooladapters/TemplateLocationAdapter.h"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>

#include <opencv2/imgproc.hpp>

#include <cmath>

namespace {

int failures = 0;

void check(bool condition, const QString &message)
{
    if (condition) {
        qInfo().noquote() << QStringLiteral("PASS: %1").arg(message);
        return;
    }
    ++failures;
    qCritical().noquote() << QStringLiteral("FAIL: %1").arg(message);
}

QJsonObject rectJson(double x, double y, double width, double height)
{
    return QJsonObject{{QStringLiteral("x"), x},
                       {QStringLiteral("y"), y},
                       {QStringLiteral("width"), width},
                       {QStringLiteral("height"), height}};
}

cv::Mat referenceFixture()
{
    cv::Mat image(360, 520, CV_8UC3, cv::Scalar(28, 28, 28));
    cv::rectangle(image, cv::Rect(170, 105, 128, 102), cv::Scalar(225, 225, 225), -1);
    cv::rectangle(image, cv::Rect(184, 119, 43, 29), cv::Scalar(42, 42, 42), -1);
    cv::circle(image, cv::Point(264, 174), 17, cv::Scalar(60, 60, 60), -1);
    cv::line(image, cv::Point(190, 190), cv::Point(240, 155), cv::Scalar(250, 250, 250), 5);
    return image;
}

TemplateLocationHalconConfig baseConfig(const QString &halconLib, const QString &cacheKey)
{
    TemplateLocationHalconConfig config;
    config.toolId = QStringLiteral("template-location-smoke");
    config.halconSoPath = halconLib;
    config.modelCacheKey = cacheKey;
    config.templateRoiNormalized = QRectF(155.0 / 520.0, 90.0 / 360.0,
                                          160.0 / 520.0, 135.0 / 360.0);
    config.searchRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    config.minScore = 45;
    config.angleStart = -20;
    config.angleExtent = 40;
    config.scaleMin = 95;
    config.scaleMax = 105;
    return config;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    HalconRuntimePaths::initializeHalconEnvironment();
    const QString halconLib = HalconRuntimePaths::resolveHalconLibPath();
    check(!halconLib.isEmpty(), QStringLiteral("HALCON runtime path resolves"));

    const cv::Mat reference = referenceFixture();
    TemplateLocationHalconRunner runner;
    TemplateLocationHalconConfig automatic =
            baseConfig(halconLib, QStringLiteral("template-location-smoke-auto-v1"));
    TemplateLocationHalconRunner::clearPersistentCache(automatic.modelCacheKey);

    const TemplateLocationHalconResult baseline = runner.run(reference, reference, automatic);
    check(baseline.success && baseline.ok && baseline.status == QStringLiteral("found"),
          QStringLiteral("automatic contrast creates and finds the reference model"));
    check(baseline.payload.value(QStringLiteral("contrastMode")).toString() == QStringLiteral("auto"),
          QStringLiteral("automatic contrast mode is reported"));
    check(baseline.payload.value(QStringLiteral("contrastUsed")).toString() == QStringLiteral("auto"),
          QStringLiteral("automatic Contrast is reported as auto"));
    check(baseline.payload.contains(QStringLiteral("minContrastUsed")),
          QStringLiteral("actual automatic MinContrast is returned"));
    check(baseline.count == 1, QStringLiteral("maximum match count is one"));
    check(!baseline.payload.value(QStringLiteral("modelCacheHit")).toBool() &&
          baseline.payload.value(QStringLiteral("modelCachePersisted")).toBool(),
          QStringLiteral("first run writes the persistent shape model cache"));
    const TemplateLocationHalconResult cachedBaseline = runner.run(reference, reference, automatic);
    check(cachedBaseline.success && cachedBaseline.ok &&
          cachedBaseline.payload.value(QStringLiteral("modelCacheHit")).toBool(),
          QStringLiteral("second run restores the persisted HALCON shape model"));

    TemplateLocationHalconConfig circleSearch = automatic;
    circleSearch.searchRegionType = QStringLiteral("circle");
    circleSearch.searchCircleCenterNormalized = QPointF(234.0 / 520.0, 156.0 / 360.0);
    circleSearch.searchCircleRadiusNormalized = 120.0 / 520.0;
    circleSearch.searchRoiNormalized = QRectF((234.0 - 120.0) / 520.0,
                                              (156.0 - 120.0) / 360.0,
                                              240.0 / 520.0,
                                              240.0 / 360.0);
    const TemplateLocationHalconResult circleSearchResult =
            runner.run(reference, reference, circleSearch);
    bool hasCircleOverlay = false;
    for (const ToolOverlay &overlay : circleSearchResult.overlays) {
        if (overlay.type == ToolOverlayType::Circle &&
                overlay.label == QStringLiteral("detect_roi")) {
            hasCircleOverlay = true;
            break;
        }
    }
    check(circleSearchResult.success && circleSearchResult.ok && hasCircleOverlay,
          QStringLiteral("circle search domain is applied and exposed as a circle overlay"));

    cv::Mat repeated(reference.size(), reference.type(), cv::Scalar(28, 28, 28));
    const cv::Rect templatePixels(155, 90, 161, 135);
    const cv::Mat templatePatch = reference(templatePixels);
    templatePatch.copyTo(repeated(cv::Rect(8, 8, templatePixels.width, templatePixels.height)));
    templatePatch.copyTo(repeated(cv::Rect(184, 18, templatePixels.width, templatePixels.height)));
    templatePatch.copyTo(repeated(cv::Rect(350, 210, templatePixels.width, templatePixels.height)));
    TemplateLocationHalconConfig multiple =
            baseConfig(halconLib, QStringLiteral("template-location-smoke-multiple-v1"));
    multiple.maxMatches = 3;
    multiple.maxMatchCount = 3;
    multiple.minScore = 60;
    const TemplateLocationHalconResult multipleResult = runner.run(repeated, reference, multiple);
    check(multipleResult.success && multipleResult.ok && multipleResult.count == 3,
          QStringLiteral("maximum match count exposes three independent results"));
    check(multipleResult.payload.value(QStringLiteral("matches")).toArray().size() == 3 &&
          multipleResult.payload.value(QStringLiteral("maxMatches")).toInt() == 3,
          QStringLiteral("multi-match payload contains every configured result"));
    int scoreOverlayCount = 0;
    for (const ToolOverlay &overlay : multipleResult.overlays) {
        if (overlay.label == QStringLiteral("match_score") &&
                overlay.text.contains(QLatin1Char('%')))
            ++scoreOverlayCount;
    }
    check(scoreOverlayCount == multipleResult.count,
          QStringLiteral("every matched target has an indexed score overlay"));

    TemplateLocationHalconConfig countRejected = multiple;
    countRejected.maxMatchCount = 2;
    const TemplateLocationHalconResult countRejectedResult =
            runner.run(repeated, reference, countRejected);
    check(countRejectedResult.success && !countRejectedResult.ok &&
          countRejectedResult.status == QStringLiteral("count_out_of_range") &&
          !countRejectedResult.payload.value(QStringLiteral("countAccepted")).toBool(),
          QStringLiteral("match count outside the configured range is a normal NG"));
    check(std::abs(multipleResult.payload.value(QStringLiteral("maxOverlap")).toDouble() - 0.5) < 1e-9,
          QStringLiteral("configured MaxOverlap is passed to HALCON and reported"));

    const double dx = 54.0;
    const double dy = 31.0;
    cv::Mat translated;
    const cv::Mat translation = (cv::Mat_<double>(2, 3)
                                 << 1.0, 0.0, dx, 0.0, 1.0, dy);
    cv::warpAffine(reference, translated, translation,
                   reference.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT,
                   cv::Scalar(28, 28, 28));
    const TemplateLocationHalconResult moved = runner.run(translated, reference, automatic);
    check(moved.success && moved.ok, QStringLiteral("translated target is found"));
    const double baselineX = baseline.payload.value(QStringLiteral("x")).toDouble();
    const double baselineY = baseline.payload.value(QStringLiteral("y")).toDouble();
    const double movedX = moved.payload.value(QStringLiteral("x")).toDouble();
    const double movedY = moved.payload.value(QStringLiteral("y")).toDouble();
    check(std::abs((movedX - baselineX) - dx) <= 3.0 &&
          std::abs((movedY - baselineY) - dy) <= 3.0,
          QStringLiteral("x=Column and y=Row follow the known translation (%1,%2 -> %3,%4)")
          .arg(baselineX).arg(baselineY).arg(movedX).arg(movedY));

    TemplateLocationHalconConfig customOrigin = automatic;
    customOrigin.modelCacheKey = QStringLiteral("template-location-smoke-custom-origin-v1");
    customOrigin.originMode = QStringLiteral("custom");
    customOrigin.customOriginNormalized = QPointF((baselineX + 20.0) / reference.cols,
                                                   (baselineY + 10.0) / reference.rows);
    const TemplateLocationHalconResult customOriginResult =
            runner.run(reference, reference, customOrigin);
    check(customOriginResult.success && customOriginResult.ok &&
          std::abs(customOriginResult.payload.value(QStringLiteral("x")).toDouble() -
                   (baselineX + 20.0)) <= 2.0 &&
          std::abs(customOriginResult.payload.value(QStringLiteral("y")).toDouble() -
                   (baselineY + 10.0)) <= 2.0,
          QStringLiteral("custom origin replaces the default template centroid output"));

    TemplateLocationHalconConfig transformedConfig =
            baseConfig(halconLib, QStringLiteral("template-location-smoke-transform-v1"));
    transformedConfig.angleStart = -30;
    transformedConfig.angleExtent = 60;
    transformedConfig.scaleMin = 85;
    transformedConfig.scaleMax = 120;
    const double expectedAngle = 12.0;
    const double expectedScale = 1.10;
    const cv::Mat transform = cv::getRotationMatrix2D(cv::Point2f(234.0F, 156.0F),
                                                       expectedAngle, expectedScale);
    cv::Mat rotatedScaled;
    cv::warpAffine(reference, rotatedScaled, transform, reference.size(),
                   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(28, 28, 28));
    const TemplateLocationHalconResult transformed =
            runner.run(rotatedScaled, reference, transformedConfig);
    check(transformed.success && transformed.ok,
          QStringLiteral("rotated and scaled target is found"));
    check(std::abs(std::abs(transformed.payload.value(QStringLiteral("angleDeg")).toDouble()) -
                   expectedAngle) <= 2.0 &&
          std::abs(transformed.payload.value(QStringLiteral("scale")).toDouble() - expectedScale) <= 0.04,
          QStringLiteral("known rotation magnitude and scale are returned within tolerance"));
    check(transformed.payload.value(QStringLiteral("timeoutMsUsed")).toInt() == 2000,
          QStringLiteral("configured HALCON timeout is applied"));

    TemplateLocationHalconConfig manual =
            baseConfig(halconLib, QStringLiteral("template-location-smoke-manual-v1"));
    manual.contrastMode = QStringLiteral("manual");
    manual.contrast = 35;
    manual.minContrast = 8;
    const TemplateLocationHalconResult manualResult = runner.run(reference, reference, manual);
    check(manualResult.success && manualResult.ok,
          QStringLiteral("manual contrast creates and finds the model"));
    check(manualResult.payload.value(QStringLiteral("contrastUsed")).toInt() == 35 &&
          qRound(manualResult.payload.value(QStringLiteral("minContrastUsed")).toDouble()) == 8,
          QStringLiteral("manual Contrast and MinContrast are reported"));

    const TemplateLocationHalconResult emptyImage = runner.run(cv::Mat(), reference, automatic);
    check(!emptyImage.success && emptyImage.status == QStringLiteral("image_empty"),
          QStringLiteral("empty run image returns an execution error"));

    cv::Mat blank(reference.size(), reference.type(), cv::Scalar(28, 28, 28));
    const TemplateLocationHalconResult missing = runner.run(blank, reference, automatic);
    check(missing.success && !missing.ok && missing.status == QStringLiteral("not_found"),
          QStringLiteral("no match is a normal NG result"));

    TemplateLocationAdapter adapter;
    ToolRequest request;
    request.image = reference;
    request.referenceImage = reference;
    request.config.toolId = QStringLiteral("template-location-adapter-smoke");
    request.config.toolName = QStringLiteral("模板定位");
    request.config.toolType = ToolType::TemplateLocation;
    request.config.category = ToolCategory::Location;
    request.config.params.insert(QStringLiteral("modelCreated"), false);
    const ToolResult noModel = adapter.run(request);
    check(!noModel.success && noModel.status == QStringLiteral("no_model"),
          QStringLiteral("adapter rejects execution without a created model"));

    request.config.params = QJsonObject{
        {QStringLiteral("modelCreated"), true},
        {QStringLiteral("modelCacheKey"), QStringLiteral("template-location-adapter-auto-v1")},
        {QStringLiteral("templateRoiNormalized"), rectJson(155.0 / 520.0, 90.0 / 360.0,
                                                           160.0 / 520.0, 135.0 / 360.0)},
        {QStringLiteral("searchRoiNormalized"), rectJson(0.0, 0.0, 1.0, 1.0)},
        {QStringLiteral("contrastMode"), QStringLiteral("auto")},
        {QStringLiteral("minScore"), 45}
    };
    const ToolResult adapterFound = adapter.run(request);
    check(adapterFound.success && adapterFound.ok && adapterFound.toolType == ToolType::TemplateLocation,
          QStringLiteral("adapter returns a TemplateLocation ToolResult"));
    check(adapterFound.payload.contains(QStringLiteral("x")) &&
          adapterFound.payload.contains(QStringLiteral("y")) &&
          adapterFound.payload.contains(QStringLiteral("angleDeg")) &&
          adapterFound.payload.contains(QStringLiteral("scale")),
          QStringLiteral("result payload contains the public pose contract"));

    request.config.params.insert(QStringLiteral("contrastMode"), QStringLiteral("manual"));
    request.config.params.insert(QStringLiteral("contrast"), 20);
    request.config.params.insert(QStringLiteral("minContrast"), 20);
    const ToolResult invalidContrast = adapter.run(request);
    check(!invalidContrast.success && invalidContrast.status == QStringLiteral("invalid_parameter"),
          QStringLiteral("adapter rejects MinContrast greater than or equal to Contrast"));

    TemplateLocationHalconConfig invalidRoi = automatic;
    invalidRoi.templateRoiNormalized = QRectF();
    const TemplateLocationHalconResult invalidRoiResult = runner.run(reference, reference, invalidRoi);
    check(!invalidRoiResult.success &&
          invalidRoiResult.status == QStringLiteral("invalid_template_region"),
          QStringLiteral("invalid template ROI returns the public error code"));

    PatternPresenceHalconConfig legacyPattern;
    legacyPattern.toolId = QStringLiteral("pattern-presence-regression");
    legacyPattern.halconSoPath = halconLib;
    legacyPattern.modelCacheKey = QStringLiteral("pattern-presence-regression-mapped-v1");
    legacyPattern.templateRoiNormalized = automatic.templateRoiNormalized;
    legacyPattern.roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    const PatternPresenceHalconResult legacyPatternResult =
            PatternPresenceHalconRunner().run(reference, reference, legacyPattern);
    check(legacyPatternResult.success && legacyPatternResult.ok &&
          legacyPatternResult.payload.value(QStringLiteral("contrastMode")).toString() ==
              QStringLiteral("mapped"),
          QStringLiteral("existing PatternPresence mapped-contrast behavior still works"));

    qInfo().noquote() << QStringLiteral("TemplateLocation smoke failures=%1").arg(failures);
    return failures == 0 ? 0 : 1;
}
