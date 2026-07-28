#include "algorithms/recognition/RegisteredClassificationTrainingRunner.h"
#include "tooladapters/RegisteredClassificationAdapter.h"
#include "toolcore/ToolAdapter.h"
#include "toolcore/ToolEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QPolygonF>

#include <cmath>
#include <iostream>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace {

constexpr int kWidth = 100;
constexpr int kHeight = 80;
const QRectF kReferenceRoi(0.2, 0.2, 0.4, 0.4);
const QString kCorrectionId = QStringLiteral("pc-registered-smoke");
const QString kFrameId = QStringLiteral("registered-position-frame");

cv::Mat classImage(bool brightClass, int columnOffset = 0)
{
    const uchar background = brightClass ? 30 : 220;
    const uchar foreground = brightClass ? 220 : 30;
    cv::Mat image(kHeight, kWidth, CV_8UC3,
                  cv::Scalar(background, background, background));
    cv::rectangle(image,
                  cv::Rect(24 + columnOffset, 20, 24, 20),
                  cv::Scalar(foreground, foreground, foreground),
                  -1);
    return image;
}

QString trainModel()
{
    const QString modelDir = QDir::temp().filePath(
                QStringLiteral("registered_classification_position_correction_smoke"));
    QDir(modelDir).removeRecursively();
    QDir().mkpath(modelDir);

    RegisteredClassificationTrainingRequest request;
    request.outputModelDir = modelDir;
    request.classLabels = {
        {0, QStringLiteral("BRIGHT")},
        {1, QStringLiteral("DARK")}
    };
    for (int index = 0; index < 4; ++index) {
        RegisteredClassificationTrainingSample bright;
        bright.image = classImage(true);
        bright.region.type = QStringLiteral("rectangle");
        bright.region.rectNormalized = kReferenceRoi;
        bright.classId = 0;
        request.samples.append(bright);

        RegisteredClassificationTrainingSample dark;
        dark.image = classImage(false);
        dark.region = bright.region;
        dark.classId = 1;
        request.samples.append(dark);
    }

    RegisteredClassificationTrainingRunner trainer;
    const RegisteredClassificationTrainingResult trained = trainer.train(request);
    if (!trained.success) {
        std::cerr << "training fixture failed: "
                  << trained.status.toStdString() << " "
                  << trained.message.toStdString() << std::endl;
        return QString();
    }
    return modelDir;
}

ToolConfig classificationConfig(const QString &modelDir,
                                bool correctionEnabled = true,
                                bool showContour = true)
{
    ToolConfig config;
    config.toolId = QStringLiteral("registered-position-consumer");
    config.toolName = QStringLiteral("RegisteredClassification");
    config.toolType = ToolType::RegisteredClassification;
    config.category = ToolCategory::Recognition;
    config.enabled = true;
    config.roiNormalized = kReferenceRoi;

    QJsonObject params{
        {QStringLiteral("version"), 2},
        {QStringLiteral("modelPath"), modelDir},
        {QStringLiteral("modelName"), QStringLiteral("PositionSmokeModel")},
        {QStringLiteral("modelType"), registeredClassificationKnnModelType()},
        {QStringLiteral("detectRegionType"), QStringLiteral("rectangle")},
        {QStringLiteral("roiNormalized"), QJsonObject{
             {QStringLiteral("x"), kReferenceRoi.x()},
             {QStringLiteral("y"), kReferenceRoi.y()},
             {QStringLiteral("width"), kReferenceRoi.width()},
             {QStringLiteral("height"), kReferenceRoi.height()}}},
        {QStringLiteral("enablePositionCorrection"), correctionEnabled},
        {QStringLiteral("positionCorrectionSource"), QStringLiteral("前置位置修正")},
        {QStringLiteral("positionCorrectionSourceId"), kCorrectionId},
        {QStringLiteral("showPositionCorrectionMatchContour"), showContour},
        {QStringLiteral("topK"), 2},
        {QStringLiteral("minSimilarity"), 0},
        {QStringLiteral("minMargin"), 0}
    };
    config.params.insert(QStringLiteral("registeredClassification"), params);
    config.params.insert(QStringLiteral("enablePositionCorrection"),
                         correctionEnabled);
    config.params.insert(QStringLiteral("positionCorrectionSource"),
                         QStringLiteral("前置位置修正"));
    config.params.insert(QStringLiteral("positionCorrectionSourceId"),
                         kCorrectionId);
    config.params.insert(QStringLiteral("showPositionCorrectionMatchContour"),
                         showContour);
    config.judgeRule = {
        {QStringLiteral("mode"), QStringLiteral("class_match")},
        {QStringLiteral("expectedLabel"), QStringLiteral("BRIGHT")},
        {QStringLiteral("minScore"), 0}
    };
    return config;
}

ToolResult correctionResult(const QJsonArray &matrix)
{
    ToolResult result;
    result.toolId = kCorrectionId;
    result.toolType = ToolType::PositionCorrection;
    result.success = true;
    result.ok = true;
    result.status = QStringLiteral("corrected");
    result.payload = {
        {QStringLiteral("sourceId"), kCorrectionId},
        {QStringLiteral("frameId"), kFrameId},
        {QStringLiteral("positionCorrectionApplied"), true},
        {QStringLiteral("referenceToRunHomMat2D"), matrix},
        {QStringLiteral("referenceScale"), 1.0},
        {QStringLiteral("runScale"), 1.0},
        {QStringLiteral("scaleRatio"), 1.0}
    };

    ToolOverlay contour;
    contour.type = ToolOverlayType::Polygon;
    contour.label = QStringLiteral("match_result");
    contour.points = {
        QPointF(54.0, 20.0), QPointF(78.0, 20.0),
        QPointF(78.0, 40.0), QPointF(54.0, 40.0)
    };
    result.overlays.append(contour);

    ToolOverlay origin;
    origin.type = ToolOverlayType::Line;
    origin.label = QStringLiteral("match_center");
    origin.p1 = QPointF(62.0, 30.0);
    origin.p2 = QPointF(70.0, 30.0);
    result.overlays.append(origin);
    return result;
}

QJsonObject runtimeContext(const QJsonArray &matrix)
{
    return {
        {QStringLiteral("frameId"), kFrameId},
        {QStringLiteral("positionCorrectionsById"),
         QJsonObject{{kCorrectionId, correctionResult(matrix).toJson()}}}
    };
}

const ToolOverlay *overlayWithRole(const ToolResult &result,
                                   const QString &role)
{
    for (const ToolOverlay &overlay : result.overlays) {
        if (overlay.extra.value(QStringLiteral("role")).toString() == role)
            return &overlay;
    }
    return nullptr;
}

class CorrectionSourceAdapter final : public ToolAdapter
{
public:
    bool supports(ToolType type) const override
    {
        return type == ToolType::PositionCorrection;
    }

    ToolResult run(const ToolRequest &) override
    {
        return correctionResult(QJsonArray{
            1.0, 0.0, 0.0,
            0.0, 1.0, 30.0
        });
    }
};

bool require(bool condition, const char *message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << std::endl;
    return condition;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString modelDir = trainModel();
    if (modelDir.isEmpty())
        return 1;

    RegisteredClassificationAdapter adapter;
    const cv::Mat shiftedImage = classImage(true, 30);
    const QJsonArray translated{
        1.0, 0.0, 0.0,
        0.0, 1.0, 30.0
    };

    ToolRequest correctedRequest;
    correctedRequest.config = classificationConfig(modelDir);
    correctedRequest.image = shiftedImage;
    correctedRequest.frameId = kFrameId;
    correctedRequest.runtimeContext = runtimeContext(translated);
    const ToolResult corrected = adapter.run(correctedRequest);
    const ToolOverlay *detectRoi =
            overlayWithRole(corrected, QStringLiteral("detect_roi"));
    const ToolOverlay *matchContour = overlayWithRole(
                corrected, QStringLiteral("position_correction_match_contour"));
    const ToolOverlay *matchOrigin = overlayWithRole(
                corrected, QStringLiteral("position_correction_match_origin"));
    const QRectF detectBounds = detectRoi
            ? (detectRoi->type == ToolOverlayType::Polygon
               ? QPolygonF(detectRoi->points).boundingRect()
               : detectRoi->rect)
            : QRectF();
    if (!require(corrected.success && corrected.ok,
                 "translated ROI must classify the shifted BRIGHT sample")
            || !require(corrected.text == QStringLiteral("BRIGHT"),
                        "translated sample must preserve the trained class")
            || !require(corrected.payload.value(
                            QStringLiteral("positionCorrectionApplied")).toBool(),
                        "payload must report position correction applied")
            || !require(corrected.payload.value(
                            QStringLiteral("referenceToRunHomMat2D")).toArray()
                        == translated,
                        "payload must expose the consumed affine matrix")
            || !require(corrected.payload.value(
                            QStringLiteral("positionCorrectionOperation")).toString()
                        == QStringLiteral("affine_trans_region+clip_region"),
                        "runner must report the HALCON Region operation")
            || !require(detectRoi
                        && detectRoi->type == ToolOverlayType::Polygon
                        && std::fabs(detectBounds.x() - 50.0) < 0.01
                        && std::fabs(detectBounds.width() - 40.0) < 0.01,
                        "detect_roi overlay must use the same translated matrix")
            || !require(matchContour && matchOrigin,
                        "match contour and origin roles must be propagated")) {
        std::cerr << "status=" << corrected.status.toStdString()
                  << " message=" << corrected.message.toStdString()
                  << " bounds=" << detectBounds.x() << ","
                  << detectBounds.y() << " "
                  << detectBounds.width() << "x"
                  << detectBounds.height() << std::endl;
        return 1;
    }

    ToolRequest hiddenContourRequest = correctedRequest;
    hiddenContourRequest.config = classificationConfig(modelDir, true, false);
    const ToolResult hiddenContour = adapter.run(hiddenContourRequest);
    if (!require(hiddenContour.success,
                 "hiding match contour must not disable ROI correction")
            || !require(!overlayWithRole(
                            hiddenContour,
                            QStringLiteral("position_correction_match_contour")),
                        "disabled match contour must not be emitted")
            || !require(overlayWithRole(
                            hiddenContour,
                            QStringLiteral("position_correction_match_origin")),
                        "match origin must remain when contour is hidden")) {
        return 1;
    }

    ToolRequest missingSourceRequest = correctedRequest;
    missingSourceRequest.runtimeContext = QJsonObject{
        {QStringLiteral("frameId"), kFrameId}
    };
    const ToolResult missingSource = adapter.run(missingSourceRequest);
    if (!require(!missingSource.success
                 && missingSource.status
                    == QStringLiteral("position_correction_source_missing"),
                 "missing correction source must fail without fallback")
            || !require(missingSource.payload.value(
                            QStringLiteral("positionCorrectionReason")).toString()
                        == QStringLiteral("position_correction_source_missing"),
                        "missing-source failure must remain diagnosable")) {
        return 1;
    }

    ToolRequest outOfImageRequest = correctedRequest;
    outOfImageRequest.runtimeContext = runtimeContext(QJsonArray{
        1.0, 0.0, 0.0,
        0.0, 1.0, 200.0
    });
    const ToolResult outOfImage = adapter.run(outOfImageRequest);
    if (!require(!outOfImage.success
                 && outOfImage.status
                    == QStringLiteral("corrected_roi_out_of_image"),
                 "fully out-of-image corrected ROI must fail explicitly")
            || !require(outOfImage.payload.value(
                            QStringLiteral("positionCorrectionOperation")).toString()
                        == QStringLiteral("affine_trans_region+clip_region"),
                        "out-of-image failure must retain the HALCON ROI operation")
            || !require(outOfImage.overlays.isEmpty(),
                        "failed corrected ROI must not emit stale runtime overlays")) {
        return 1;
    }

    CorrectionSourceAdapter correctionAdapter;
    ToolEngine engine;
    engine.registerAdapter(&correctionAdapter);
    engine.registerAdapter(&adapter);
    ToolConfig sourceConfig;
    sourceConfig.toolId = kCorrectionId;
    sourceConfig.toolName = QStringLiteral("PositionCorrection");
    sourceConfig.toolType = ToolType::PositionCorrection;
    sourceConfig.category = ToolCategory::Location;
    sourceConfig.enabled = true;
    const QVector<ToolResult> chain = engine.runTools(
                QVector<ToolConfig>{
                    sourceConfig,
                    classificationConfig(modelDir)
                },
                shiftedImage,
                cv::Mat(),
                QJsonObject{{QStringLiteral("frameId"), kFrameId}});
    if (!require(chain.size() == 2
                 && chain.last().success
                 && chain.last().ok,
                 "ToolEngine prefix chain must register and consume correction")
            || !require(overlayWithRole(
                            chain.last(), QStringLiteral("detect_roi")),
                        "ToolEngine result must contain the corrected detect ROI")) {
        return 1;
    }

    std::cout << "registered_classification_position_correction_smoke passed"
              << std::endl;
    return 0;
}
