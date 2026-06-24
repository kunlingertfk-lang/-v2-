#include "algorithms/ai/AiDetectionRunner.h"
#include "tooladapters/AiDetectionAdapter.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <cmath>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

namespace {

bool nearlyEqual(const double lhs, const double rhs)
{
    return std::abs(lhs - rhs) < 0.000001;
}

void check(const bool condition, const char *message, int *failures)
{
    if (condition)
        return;

    std::cerr << "FAIL: " << message << '\n';
    ++(*failures);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    const int remoteIndex = arguments.indexOf(QStringLiteral("--remote-rk"));
    if (remoteIndex >= 0) {
        if (remoteIndex + 1 >= arguments.size()) {
            std::cerr << "usage: ai_detection_bridge_smoke --remote-rk <image>\n";
            return 2;
        }

        const QString imagePath = arguments.at(remoteIndex + 1);
        const cv::Mat image = cv::imread(imagePath.toLocal8Bit().constData(), cv::IMREAD_COLOR);
        if (image.empty()) {
            std::cerr << "remote_success=0\nremote_status=empty_image\n";
            return 2;
        }

        AiDetectionConfig config;
        config.modelName = QStringLiteral("yolov8_red_black");
        config.boxThreshold = 0.02;
        config.maxDetections = 10;
        config.minCount = 0;
        config.maxCount = 999999;
        config.timeoutMs = 120000;

        AiDetectionRunner runner;
        const AiDetectionRunnerResult result = runner.run(image, config);
        std::cout << "remote_success=" << (result.success ? 1 : 0) << '\n'
                  << "remote_ok=" << (result.ok ? 1 : 0) << '\n'
                  << "remote_status=" << result.status.toLocal8Bit().constData() << '\n'
                  << "remote_message=" << result.message.toLocal8Bit().constData() << '\n'
                  << "remote_count=" << result.count << '\n'
                  << "remote_raw_count=" << result.rawDetections.size() << '\n'
                  << "remote_detection_count_payload="
                  << result.payload.value(QStringLiteral("scriptDetectionCount")).toInt(-1) << '\n'
                  << "remote_bbox_parsed=" << result.rawDetections.size() << '\n'
                  << "remote_script_argument_mode=" << result.scriptArgumentMode.toLocal8Bit().constData() << '\n'
                  << "remote_result_image=" << result.remoteResultImage.toLocal8Bit().constData() << '\n';
        return result.success ? 0 : 1;
    }

    int failures = 0;

    AiDetectionDetection parsed;
    check(AiDetectionRunner::parseDetectionLine(QStringLiteral("red @ (10 20 120 80) 0.91"), &parsed),
          "single DETECTION_LINE parses",
          &failures);
    check(parsed.className == QStringLiteral("red"), "className red", &failures);
    check(nearlyEqual(parsed.bboxPixel.left(), 10.0), "left=10", &failures);
    check(nearlyEqual(parsed.bboxPixel.top(), 20.0), "top=20", &failures);
    check(nearlyEqual(parsed.bboxPixel.right(), 120.0), "right=120", &failures);
    check(nearlyEqual(parsed.bboxPixel.bottom(), 80.0), "bottom=80", &failures);
    check(nearlyEqual(parsed.score, 0.91), "score=0.91", &failures);

    const QStringList lines = {
        QStringLiteral("red @ (10 20 120 80) 0.91"),
        QStringLiteral("black @ (5 5 30 50) 0.72")
    };
    const QVector<AiDetectionDetection> multi =
            AiDetectionRunner::parseDetectionLines(lines, QSize(200, 100), QSize(200, 100), QPointF(0.0, 0.0));
    check(multi.size() == 2, "multiple DETECTION_LINE parses", &failures);

    AiDetectionConfig countConfig;
    countConfig.judgeMode = QStringLiteral("count");
    countConfig.minCount = 0;
    countConfig.maxCount = 0;
    check(AiDetectionRunner::judgeDetections({}, countConfig).ok, "no detection can be OK for count 0..0", &failures);

    AiDetectionConfig classFilterConfig;
    classFilterConfig.classFilterEnabled = true;
    classFilterConfig.classFilterText = QStringLiteral("red");
    AiDetectionFilterResult classFiltered = AiDetectionRunner::applyFilters(multi, classFilterConfig);
    check(classFiltered.detections.size() == 1 &&
          classFiltered.detections.first().className == QStringLiteral("red"),
          "classFilterEnabled filters red/black",
          &failures);

    AiDetectionConfig maxConfig;
    maxConfig.maxDetections = 1;
    AiDetectionFilterResult truncated = AiDetectionRunner::applyFilters(multi, maxConfig);
    check(truncated.detections.size() == 1, "maxDetections truncates", &failures);

    AiDetectionConfig sizeConfig;
    sizeConfig.widthFilterEnabled = true;
    sizeConfig.minWidth = 100.0;
    sizeConfig.maxWidth = 200.0;
    sizeConfig.heightFilterEnabled = true;
    sizeConfig.minHeight = 50.0;
    sizeConfig.maxHeight = 100.0;
    AiDetectionFilterResult sizeFiltered = AiDetectionRunner::applyFilters(multi, sizeConfig);
    check(sizeFiltered.detections.size() == 1 &&
          sizeFiltered.detections.first().className == QStringLiteral("red"),
          "width/height filters apply",
          &failures);

    AiDetectionConfig judgeConfig;
    judgeConfig.judgeMode = QStringLiteral("count");
    judgeConfig.minCount = 1;
    judgeConfig.maxCount = 2;
    check(AiDetectionRunner::judgeDetections(sizeFiltered.detections, judgeConfig).ok,
          "minCount/maxCount OK",
          &failures);
    check(!AiDetectionRunner::judgeDetections({}, judgeConfig).ok,
          "minCount/maxCount NG",
          &failures);

    AiDetectionConfig sshFailureConfig;
    sshFailureConfig.remoteUser = QStringLiteral("invalid");
    sshFailureConfig.remoteHost = QStringLiteral("127.0.0.1");
    sshFailureConfig.timeoutMs = 100;
    sshFailureConfig.localTempRoot = QStringLiteral("/tmp/v2_ai_bridge_smoke");
    cv::Mat tinyImage(8, 8, CV_8UC3, cv::Scalar(0, 0, 0));
    AiDetectionRunner runner;
    const AiDetectionRunnerResult sshFailureResult = runner.run(tinyImage, sshFailureConfig);
    check(!sshFailureResult.success && !sshFailureResult.message.trimmed().isEmpty(),
          "SSH failure returns error without crashing",
          &failures);

    AiDetectionAdapter adapter;
    check(adapter.supports(ToolType::AiDetection), "ToolType::AiDetection adapter support", &failures);
    check(!adapter.supports(ToolType::Ocr), "AiDetectionAdapter rejects Ocr", &failures);

    check(QFileInfo::exists(QStringLiteral("ui/ObjectDetectionDialog.ui")),
          "ObjectDetectionDialog.ui still exists",
          &failures);

    if (failures == 0)
        std::cout << "ai_detection_bridge_smoke: PASS\n";

    return failures == 0 ? 0 : 1;
}
