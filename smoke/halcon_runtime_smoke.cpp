#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/ocr/OcrHalconRunner.h"
#include "algorithms/presence/BlobPresenceHalconRunner.h"
#include "algorithms/presence/CirclePresenceHalconRunner.h"
#include "algorithms/presence/EdgePresenceHalconRunner.h"
#include "algorithms/presence/LinePresenceHalconRunner.h"
#include "algorithms/presence/PatternPresenceHalconRunner.h"

#include <QCoreApplication>
#include <QDebug>

#include <opencv2/imgproc.hpp>

namespace {

template<typename Result>
void printResult(const char *name, const Result &result)
{
    qInfo().noquote() << QStringLiteral("%1 success=%2 status=%3 message=%4")
                         .arg(QString::fromLatin1(name),
                              result.success ? QStringLiteral("true") : QStringLiteral("false"),
                              result.status,
                              result.message);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QString license = HalconRuntimePaths::initializeHalconEnvironment();
    qInfo().noquote() << QStringLiteral("license=%1").arg(license);

    const QString halconLib = HalconRuntimePaths::resolveHalconLibPath();
    qInfo().noquote() << QStringLiteral("library=%1").arg(halconLib);

    cv::Mat image(480, 640, CV_8UC3, cv::Scalar(20, 20, 20));
    cv::rectangle(image, cv::Rect(180, 130, 280, 220), cv::Scalar(230, 230, 230), -1);
    cv::circle(image, cv::Point(320, 240), 75, cv::Scalar(20, 20, 20), 10);
    cv::line(image, cv::Point(100, 240), cv::Point(540, 240), cv::Scalar(255, 255, 255), 4);

    BlobPresenceHalconConfig blobConfig;
    blobConfig.halconSoPath = halconLib;
    blobConfig.grayMin = 180;
    blobConfig.grayMax = 255;
    printResult("blob", BlobPresenceHalconRunner().run(image, blobConfig));

    CirclePresenceHalconConfig circleConfig;
    circleConfig.halconSoPath = halconLib;
    printResult("circle", CirclePresenceHalconRunner().run(image, circleConfig));

    EdgePresenceHalconConfig edgeConfig;
    edgeConfig.halconSoPath = halconLib;
    edgeConfig.searchLineP1 = QPointF(0.5, 0.1);
    edgeConfig.searchLineP2 = QPointF(0.5, 0.9);
    printResult("edge", EdgePresenceHalconRunner().run(image, edgeConfig));

    LinePresenceHalconConfig lineConfig;
    lineConfig.halconSoPath = halconLib;
    printResult("line", LinePresenceHalconRunner().run(image, lineConfig));

    PatternPresenceHalconConfig patternConfig;
    patternConfig.halconSoPath = halconLib;
    patternConfig.templateRoiNormalized = QRectF(0.2, 0.2, 0.6, 0.6);
    printResult("pattern", PatternPresenceHalconRunner().run(image, image, patternConfig));

    OcrHalconConfig ocrConfig;
    ocrConfig.halconSoPath = halconLib;
    ocrConfig.ocrModelPath = HalconRuntimePaths::resolveOcrModelPath();
    printResult("ocr", OcrHalconRunner().run(image, ocrConfig));
    return 0;
}
