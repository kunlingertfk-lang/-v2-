#include "tooladapters/RegisteredClassificationAdapter.h"
#include "algorithms/recognition/RegisteredClassificationTrainingRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonObject>
#include <QFileInfo>
#include <iostream>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace {

int g_failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++g_failures;
    }
}

// 构造一个带注册分类参数的 ToolConfig。可按需覆盖单个字段。
ToolConfig makeConfig(const QString &modelPath,
                      const QString &modelName,
                      const QString &modelType,
                      const QString &detectRegionType,
                      const QRectF &roi,
                      const QString &halconSoPath,
                      const QString &judgeMode,
                      const QString &expectedLabel,
                      int minScore,
                      int minSimilarity = 80,
                      int minMargin = 8)
{
    ToolConfig config;
    config.toolId = QStringLiteral("registered_classification_smoke");
    config.toolName = QStringLiteral("RegisteredClassification");
    config.toolType = ToolType::RegisteredClassification;
    config.category = ToolCategory::Recognition;
    config.roiNormalized = roi;

    QJsonObject nested;
    nested.insert(QStringLiteral("version"), 2);
    nested.insert(QStringLiteral("modelName"), modelName);
    nested.insert(QStringLiteral("modelPath"), modelPath);
    nested.insert(QStringLiteral("modelType"), modelType);
    nested.insert(QStringLiteral("detectRegionType"), detectRegionType);
    nested.insert(QStringLiteral("roiNormalized"), QJsonObject{
            {QStringLiteral("x"), roi.x()},
            {QStringLiteral("y"), roi.y()},
            {QStringLiteral("width"), roi.width()},
            {QStringLiteral("height"), roi.height()}});
    nested.insert(QStringLiteral("enablePositionCorrection"), false);
    nested.insert(QStringLiteral("positionCorrectionSource"),
                  QStringLiteral("0 基准图.位置修正信息"));
    nested.insert(QStringLiteral("positionCorrectionSourceId"),
                  QStringLiteral("reference.positionCorrection"));
    nested.insert(QStringLiteral("topK"), 1);
    nested.insert(QStringLiteral("minSimilarity"), minSimilarity);
    nested.insert(QStringLiteral("minMargin"), minMargin);
    if (!halconSoPath.isNull())
        nested.insert(QStringLiteral("halconSoPath"), halconSoPath);
    config.params.insert(QStringLiteral("registeredClassification"), nested);

    config.judgeRule.insert(QStringLiteral("mode"), judgeMode);
    if (!expectedLabel.isNull())
        config.judgeRule.insert(QStringLiteral("expectedLabel"), expectedLabel);
    config.judgeRule.insert(QStringLiteral("minScore"), minScore);
    return config;
}

ToolRequest requestWithImage(const ToolConfig &config, const cv::Mat &image)
{
    ToolRequest request;
    request.config = config;
    request.image = image;
    return request;
}

QString smokeModelDir(const QString &name)
{
    QDir dir(QDir::tempPath());
    const QString path = dir.filePath(
                QStringLiteral("registered_classification_adapter_smoke_model/%1").arg(name));
    QDir(path).removeRecursively();
    dir.mkpath(path);
    return path;
}

cv::Mat makeBrightTrainingImage()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(30, 30, 30));
    cv::rectangle(image, cv::Rect(20, 20, 40, 30), cv::Scalar(220, 220, 220), -1);
    return image;
}

cv::Mat makeDarkTrainingImage()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(220, 220, 220));
    cv::rectangle(image, cv::Rect(20, 20, 40, 30), cv::Scalar(30, 30, 30), -1);
    return image;
}

RegisteredClassificationTrainingRequest makeTrainingRequest(const QString &modelDir)
{
    RegisteredClassificationTrainingRequest request;
    request.outputModelDir = modelDir;
    request.classLabels = {
        {0, QStringLiteral("OK")},
        {1, QStringLiteral("NG")}
    };
    for (int i = 0; i < 4; ++i) {
        RegisteredClassificationTrainingSample bright;
        bright.image = makeBrightTrainingImage();
        bright.region.type = QStringLiteral("rectangle");
        bright.region.rectNormalized = QRectF(0.2, 0.2, 0.4, 0.4);
        bright.classId = 0;
        request.samples.append(bright);
        RegisteredClassificationTrainingSample dark;
        dark.image = makeDarkTrainingImage();
        dark.region.type = QStringLiteral("rectangle");
        dark.region.rectNormalized = QRectF(0.2, 0.2, 0.4, 0.4);
        dark.classId = 1;
        request.samples.append(dark);
    }
    return request;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    RegisteredClassificationAdapter adapter;
    check(adapter.supports(ToolType::RegisteredClassification),
          "adapter must support RegisteredClassification");

    // 一张有效占位图像，用于需要非空图像的异常路径。
    cv::Mat dummyImage = cv::Mat::zeros(64, 64, CV_8UC3);
    const QString trainedModelDir = smokeModelDir(QStringLiteral("trained"));
    RegisteredClassificationTrainingRunner trainer;
    const RegisteredClassificationTrainingResult trainResult =
            trainer.train(makeTrainingRequest(trainedModelDir));
    check(trainResult.success, "KNN training fixture must succeed");
    check(QFileInfo(registeredClassificationMetadataPath(trainedModelDir)).exists(),
          "fixture metadata.json must exist");
    check(QFileInfo(registeredClassificationSampleKnnPath(trainedModelDir)).exists(),
          "fixture model.gnc must exist");
    check(QFileInfo(registeredClassificationCenterKnnPath(trainedModelDir)).exists(),
          "fixture class_centers.gnc must exist");

    // 1. 空图像 -> image_empty
    {
        ToolConfig config = makeConfig(QStringLiteral("/tmp/DemoModel.hdl"),
                                       QStringLiteral("DemoModel"),
                                       registeredClassificationKnnModelType(),
                                       QStringLiteral("full"),
                                       QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, cv::Mat()));
        check(result.status == QStringLiteral("image_empty"), "empty image must yield image_empty");
        check(!result.success && !result.ok, "empty image must not report success/ok");
    }

    // 2. 无 modelPath -> model_path_empty
    {
        ToolConfig config = makeConfig(QString(), QString(),
                                       registeredClassificationKnnModelType(),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        check(result.status == QStringLiteral("model_path_empty"),
              "missing model path must yield model_path_empty");
    }

    // 3. schema 1 modelType -> retraining is required
    {
        ToolConfig config = makeConfig(QStringLiteral("/tmp/LegacyModel.hdl"),
                                       QStringLiteral("LegacyModel"),
                                       registeredClassificationLegacyMlpModelType(),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        check(result.status == QStringLiteral("legacy_model_requires_retraining"),
              "schema 1 model type must require retraining");
    }

    // 4. 模型目录不存在 -> model_file_not_found
    {
        ToolConfig config = makeConfig(QStringLiteral("/tmp/__nonexistent_model__"),
                                       QStringLiteral("NonExist"),
                                       registeredClassificationKnnModelType(),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        check(result.status == QStringLiteral("model_file_not_found"),
              "missing model file must yield model_file_not_found");
    }

    // 5. 矩形检测 ROI 无效 -> invalid_roi
    //    modelPath 用 smoke 自身可执行文件（一定存在）让模型存在性校验通过；
    //    ROI 用极小归一化矩形，在小图上换算后像素 < kMinRoiPixelSize(2)，触发 invalid_roi。
    {
        ToolConfig config = makeConfig(trainedModelDir,
                                       QStringLiteral("FixtureModel"),
                                       registeredClassificationKnnModelType(),
                                       QStringLiteral("rectangle"), QRectF(0, 0, 0.05, 0.05),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const cv::Mat tinyImage = cv::Mat::zeros(8, 8, CV_8UC3);
        const ToolResult result = adapter.run(requestWithImage(config, tinyImage));
        check(result.status == QStringLiteral("invalid_roi"),
              "invalid rectangle ROI must yield invalid_roi");
    }

    // 6. 未开启位置修正时仍须输出明确的未请求诊断。
    {
        ToolConfig config = makeConfig(QStringLiteral("/tmp/DemoModel.hdl"),
                                       QStringLiteral("DemoModel"),
                                       registeredClassificationKnnModelType(),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        check(!result.payload.value(QStringLiteral("enablePositionCorrection")).toBool(true),
              "payload must carry disabled enablePositionCorrection");
        check(!result.payload.value(QStringLiteral("positionCorrectionApplied")).toBool(true),
              "position correction must not be applied");
        check(result.payload.value(QStringLiteral("positionCorrectionReason")).toString()
                      == QStringLiteral("not_requested"),
              "disabled position correction reason must be not_requested");
        check(result.payload.value(QStringLiteral("errorCode")).toString() == result.status,
              "payload errorCode must match status");
    }

    // 7. V2 thresholds must reach the KNN runner and force an ambiguous UNKNOWN.
    {
        ToolConfig config = makeConfig(trainedModelDir,
                                       QStringLiteral("FixtureModel"),
                                       registeredClassificationKnnModelType(),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80, 80, 100);
        const ToolResult result = adapter.run(requestWithImage(config, makeBrightTrainingImage()));
        check(result.success && !result.ok,
              "ambiguous V2 result must be a successful NG execution");
        check(result.status == QStringLiteral("classification_rejected_ambiguous"),
              "minMargin=100 must produce an ambiguous rejection");
        check(result.text == QStringLiteral("UNKNOWN"),
              "ambiguous rejection must expose UNKNOWN text");
        check(result.payload.value(QStringLiteral("algorithm")).toString()
                      == registeredClassificationKnnModelType(),
              "adapter payload must identify the V2 KNN model type");
        check(result.payload.value(QStringLiteral("featureVersion")).toString()
                      == registeredClassificationFeatureVersionV2(),
              "adapter payload must identify the V2 feature version");
        check(result.payload.value(QStringLiteral("minSimilarity")).toInt(-1) == 80,
              "adapter payload must receive minSimilarity=80");
        check(result.payload.value(QStringLiteral("minMargin")).toInt(-1) == 100,
              "adapter payload must receive minMargin=100");
    }

    // 8. UNKNOWN remains NG when the judge basis is min_score.
    {
        ToolConfig config = makeConfig(trainedModelDir,
                                       QStringLiteral("FixtureModel"),
                                       registeredClassificationKnnModelType(),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("min_score"),
                                       QString(), 0, 80, 100);
        const ToolResult result = adapter.run(requestWithImage(config, makeBrightTrainingImage()));
        check(result.status == QStringLiteral("classification_rejected_ambiguous"),
              "min_score must preserve the UNKNOWN rejection status");
        check(result.success && !result.ok && result.text == QStringLiteral("UNKNOWN"),
              "UNKNOWN must remain NG under min_score");
    }

    if (g_failures > 0) {
        std::cerr << g_failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "registered_classification_adapter_smoke: all checks passed" << std::endl;
    return 0;
}
