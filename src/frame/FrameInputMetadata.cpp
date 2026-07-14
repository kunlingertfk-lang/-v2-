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

FrameInputMetadata FrameInputMetadata::fromQImage(const QImage &image,
                                                  const QString &source)
{
    FrameInputMetadata metadata;
    metadata.source = source;
    if (image.isNull())
        return metadata;

    switch (image.format()) {
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
        metadata.colorMode = QStringLiteral("mono");
        metadata.pixelFormat = QStringLiteral("Mono1");
        metadata.originalChannels = 1;
        metadata.originalDepth = 1;
        break;
    case QImage::Format_Grayscale8:
        metadata.colorMode = QStringLiteral("mono");
        metadata.pixelFormat = QStringLiteral("Mono8");
        metadata.originalChannels = 1;
        metadata.originalDepth = 8;
        break;
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    case QImage::Format_Grayscale16:
        metadata.colorMode = QStringLiteral("mono");
        metadata.pixelFormat = QStringLiteral("Mono16");
        metadata.originalChannels = 1;
        metadata.originalDepth = 16;
        break;
#endif
    case QImage::Format_RGB888:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("RGB8");
        metadata.originalChannels = 3;
        metadata.originalDepth = 8;
        break;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    case QImage::Format_BGR888:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("BGR8");
        metadata.originalChannels = 3;
        metadata.originalDepth = 8;
        break;
#endif
    case QImage::Format_RGB32:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("RGBX8");
        metadata.originalChannels = 4;
        metadata.originalDepth = 8;
        break;
    case QImage::Format_ARGB32:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("ARGB8");
        metadata.originalChannels = 4;
        metadata.originalDepth = 8;
        break;
    case QImage::Format_ARGB32_Premultiplied:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("ARGB8_Premultiplied");
        metadata.originalChannels = 4;
        metadata.originalDepth = 8;
        break;
    case QImage::Format_RGBX8888:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("RGBX8");
        metadata.originalChannels = 4;
        metadata.originalDepth = 8;
        break;
    case QImage::Format_RGBA8888:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("RGBA8");
        metadata.originalChannels = 4;
        metadata.originalDepth = 8;
        break;
    case QImage::Format_RGBA8888_Premultiplied:
        metadata.colorMode = QStringLiteral("color");
        metadata.pixelFormat = QStringLiteral("RGBA8_Premultiplied");
        metadata.originalChannels = 4;
        metadata.originalDepth = 8;
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
        QStringLiteral("NV12"),
        QStringLiteral("RGB8"),
        QStringLiteral("RGBX8"),
        QStringLiteral("ARGB8"),
        QStringLiteral("ARGB8_Premultiplied"),
        QStringLiteral("RGBA8"),
        QStringLiteral("RGBA8_Premultiplied")
    };
    return colorMode == QStringLiteral("color")
            && originalDepth == 8
            && supportedFormats.contains(pixelFormat);
}
