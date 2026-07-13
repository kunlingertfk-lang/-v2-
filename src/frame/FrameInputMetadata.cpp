#include "frame/FrameInputMetadata.h"

#include <QStringList>

namespace {

int bitDepthForOpenCvDepth(int depth)
{
    switch (depth) {
    case CV_8U:
    case CV_8S:
        return 8;
    case CV_16U:
    case CV_16S:
    case CV_16F:
        return 16;
    case CV_32S:
    case CV_32F:
        return 32;
    case CV_64F:
        return 64;
    default:
        return -1;
    }
}

} // namespace

FrameInputMetadata FrameInputMetadata::fromMat(const cv::Mat &image,
                                               const QString &source)
{
    FrameInputMetadata metadata;
    metadata.source = source;
    if (image.empty())
        return metadata;

    metadata.originalChannels = image.channels();
    metadata.originalDepth = bitDepthForOpenCvDepth(image.depth());

    switch (image.type()) {
    case CV_8UC1:
        metadata.colorMode = QStringLiteral("mono");
        metadata.pixelFormat = QStringLiteral("Mono8");
        break;
    case CV_8UC2:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("UYVY8");
        break;
    case CV_8UC3:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("BGR8");
        break;
    case CV_8UC4:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("BGRA8");
        break;
    default:
        break;
    }

    return metadata;
}

FrameInputMetadata FrameInputMetadata::fromJson(const QJsonObject &json)
{
    FrameInputMetadata metadata;
    const QString colorMode = json.value(QStringLiteral("colorMode")).toString();
    if (colorMode == QStringLiteral("mono")
            || colorMode == QStringLiteral("color")
            || colorMode == QStringLiteral("unknown")) {
        metadata.colorMode = colorMode;
    }
    metadata.pixelFormat = json.value(QStringLiteral("pixelFormat")).toString();
    metadata.originalChannels = json.value(QStringLiteral("originalChannels")).toInt(0);
    metadata.originalDepth = json.value(QStringLiteral("originalDepth")).toInt(-1);
    metadata.source = json.value(QStringLiteral("source")).toString();
    return metadata;
}

QJsonObject FrameInputMetadata::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("colorMode"), colorMode);
    json.insert(QStringLiteral("pixelFormat"), pixelFormat);
    json.insert(QStringLiteral("originalChannels"), originalChannels);
    json.insert(QStringLiteral("originalDepth"), originalDepth);
    json.insert(QStringLiteral("source"), source);
    return json;
}

bool FrameInputMetadata::isMono() const
{
    return colorMode == QStringLiteral("mono");
}

bool FrameInputMetadata::isSupportedColor8() const
{
    static const QStringList supportedFormats = {
        QStringLiteral("BGR8"),
        QStringLiteral("BGRA8"),
        QStringLiteral("UYVY8"),
        QStringLiteral("NV12")
    };
    return colorMode == QStringLiteral("color")
            && originalDepth == 8
            && supportedFormats.contains(pixelFormat);
}
