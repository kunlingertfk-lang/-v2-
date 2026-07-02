#include "tooladapters/RegisteredClassificationAdapter.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <iostream>

#include <opencv2/core.hpp>

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
                      const QString &detectRegionType,
                      const QRectF &roi,
                      const QString &halconSoPath,
                      const QString &judgeMode,
                      const QString &expectedLabel,
                      int minScore)
{
    ToolConfig config;
    config.toolId = QStringLiteral("registered_classification_smoke");
    config.toolName = QStringLiteral("RegisteredClassification");
    config.toolType = ToolType::RegisteredClassification;
    config.category = ToolCategory::Recognition;
    config.roiNormalized = roi;

    QJsonObject nested;
    nested.insert(QStringLiteral("version"), 1);
    nested.insert(QStringLiteral("modelName"), modelName);
    nested.insert(QStringLiteral("modelPath"), modelPath);
    nested.insert(QStringLiteral("modelType"), QStringLiteral("halcon_dl_classification"));
    nested.insert(QStringLiteral("detectRegionType"), detectRegionType);
    nested.insert(QStringLiteral("roiNormalized"), QJsonObject{
            {QStringLiteral("x"), roi.x()},
            {QStringLiteral("y"), roi.y()},
            {QStringLiteral("width"), roi.width()},
            {QStringLiteral("height"), roi.height()}});
    nested.insert(QStringLiteral("enablePositionCorrection"), true);
    nested.insert(QStringLiteral("positionCorrectionSource"),
                  QStringLiteral("1 基准图.位置修正信息"));
    nested.insert(QStringLiteral("topK"), 1);
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

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    RegisteredClassificationAdapter adapter;
    check(adapter.supports(ToolType::RegisteredClassification),
          "adapter must support RegisteredClassification");

    // 一张有效占位图像，用于需要非空图像的异常路径。
    cv::Mat dummyImage = cv::Mat::zeros(64, 64, CV_8UC3);

    // 1. 空图像 -> image_empty
    {
        ToolConfig config = makeConfig(QStringLiteral("/tmp/DemoModel.hdl"),
                                       QStringLiteral("DemoModel"),
                                       QStringLiteral("full"),
                                       QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, cv::Mat()));
        check(result.status == QStringLiteral("image_empty"), "empty image must yield image_empty");
        check(!result.success && !result.ok, "empty image must not report success/ok");
    }

    // 2. 无 modelPath -> no_model
    {
        ToolConfig config = makeConfig(QString(), QString(),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        check(result.status == QStringLiteral("no_model"), "missing model must yield no_model");
    }

    // 3. .scbin 模型 -> unsupported_model_format
    {
        ToolConfig config = makeConfig(QStringLiteral("/tmp/Model.scbin"),
                                       QStringLiteral("Model"),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        check(result.status == QStringLiteral("unsupported_model_format"),
              ".scbin must yield unsupported_model_format");
    }

    // 4. 模型名含非法字符 -> invalid_model_name
    {
        ToolConfig config = makeConfig(QStringLiteral("/tmp/Bad-Name.hdl"),
                                       QStringLiteral("Bad-Name"),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        check(result.status == QStringLiteral("invalid_model_name"),
              "illegal model name must yield invalid_model_name");
    }

    // 5. 模型文件不存在 -> model_file_not_found
    {
        ToolConfig config = makeConfig(QStringLiteral("/tmp/__nonexistent_model__.hdl"),
                                       QStringLiteral("NonExist"),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        check(result.status == QStringLiteral("model_file_not_found"),
              "missing model file must yield model_file_not_found");
    }

    // 6. 矩形检测 ROI 无效 -> invalid_roi
    //    modelPath 用 smoke 自身可执行文件（一定存在）让模型存在性校验通过；
    //    ROI 用极小归一化矩形，在小图上换算后像素 < kMinRoiPixelSize(2)，触发 invalid_roi。
    {
        ToolConfig config = makeConfig(QCoreApplication::applicationFilePath(),
                                       QStringLiteral("DemoModel"),
                                       QStringLiteral("rectangle"), QRectF(0, 0, 0.05, 0.05),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const cv::Mat tinyImage = cv::Mat::zeros(8, 8, CV_8UC3);
        const ToolResult result = adapter.run(requestWithImage(config, tinyImage));
        check(result.status == QStringLiteral("invalid_roi"),
              "invalid rectangle ROI must yield invalid_roi");
    }

    // 7. class_match 缺 expectedLabel -> missing_expected_label
    {
        ToolConfig config = makeConfig(QCoreApplication::applicationFilePath(),
                                       QStringLiteral("DemoModel"),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QString(), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        check(result.status == QStringLiteral("missing_expected_label"),
              "class_match without expected label must yield missing_expected_label");
    }

    // 8. modelPath 指向非模型文件（smoke 自身可执行文件）-> read_dl_model 失败
    //    HALCON so 路径由 resolveHalconLibPath 容错回退到默认库，故不会走到 halcon_so_not_found；
    //    非模型文件会让 T_read_dl_model 失败，返回 model_load_failed 或 halcon_error。
    {
        ToolConfig config = makeConfig(QCoreApplication::applicationFilePath(),
                                       QStringLiteral("DemoModel"),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        const bool isModelLoadError =
                result.status == QStringLiteral("model_load_failed")
                || result.status == QStringLiteral("halcon_error")
                || result.status == QStringLiteral("exception");
        check(isModelLoadError,
              "non-model file must yield model_load_failed / halcon_error / exception");
        check(!result.success && !result.ok, "non-model file must not report success/ok");
    }

    // 9. 位置修正占位 payload 必须存在且未应用（用一个返回错误的场景验证 payload 字段）。
    {
        ToolConfig config = makeConfig(QStringLiteral("/tmp/DemoModel.hdl"),
                                       QStringLiteral("DemoModel"),
                                       QStringLiteral("full"), QRectF(0, 0, 1, 1),
                                       QString(), QStringLiteral("class_match"),
                                       QStringLiteral("OK"), 80);
        const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
        check(result.payload.value(QStringLiteral("enablePositionCorrection")).toBool(false),
              "payload must carry enablePositionCorrection");
        check(!result.payload.value(QStringLiteral("positionCorrectionApplied")).toBool(true),
              "position correction must not be applied");
        check(result.payload.value(QStringLiteral("positionCorrectionReason")).toString()
                      == QStringLiteral("not implemented"),
              "position correction reason must be not implemented");
        check(result.payload.value(QStringLiteral("errorCode")).toString() == result.status,
              "payload errorCode must match status");
    }

    if (g_failures > 0) {
        std::cerr << g_failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "registered_classification_adapter_smoke: all checks passed" << std::endl;
    return 0;
}
