#include "frame/FramePixelProbe.h"

#include <QColor>

namespace {

bool isGrayImage(const QImage &image)
{
    switch (image.format()) {
    case QImage::Format_Grayscale8:
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    case QImage::Format_Grayscale16:
#endif
        return true;
    case QImage::Format_Mono:
    case QImage::Format_MonoLSB:
    case QImage::Format_Indexed8:
        return image.isGrayscale();
    default:
        return false;
    }
}

bool isSixteenBitFormat(QImage::Format format)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    switch (format) {
    case QImage::Format_Grayscale16:
    case QImage::Format_RGBX64:
    case QImage::Format_RGBA64:
    case QImage::Format_RGBA64_Premultiplied:
        return true;
    default:
        break;
    }
#else
    Q_UNUSED(format)
#endif
    return false;
}

} // namespace

FramePixelSample FramePixelProbe::sample(const QImage &image,
                                         const QPoint &imagePosition)
{
    FramePixelSample result;
    if (image.isNull()
            || imagePosition.x() < 0 || imagePosition.x() >= image.width()
            || imagePosition.y() < 0 || imagePosition.y() >= image.height()) {
        return result;
    }

    const QColor color = image.pixelColor(imagePosition);
    if (!color.isValid())
        return result;

    result.valid = true;
    result.imagePosition = imagePosition;
    result.bitDepth = isSixteenBitFormat(image.format()) ? 16 : 8;

    if (isGrayImage(image)) {
        result.channelModel = FramePixelChannelModel::Gray;
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
        if (image.format() == QImage::Format_Grayscale16) {
            result.gray = color.rgba64().red();
            return result;
        }
#endif
        result.gray = static_cast<quint16>(color.red());
        return result;
    }

    result.channelModel = FramePixelChannelModel::Rgb;
    if (result.bitDepth == 16) {
        const QRgba64 rgba = color.rgba64();
        result.red = rgba.red();
        result.green = rgba.green();
        result.blue = rgba.blue();
    } else {
        result.red = static_cast<quint16>(color.red());
        result.green = static_cast<quint16>(color.green());
        result.blue = static_cast<quint16>(color.blue());
    }
    return result;
}

QString FramePixelProbe::displayText(const FramePixelSample &sample)
{
    if (!sample.valid)
        return emptyDisplayText();

    if (sample.channelModel == FramePixelChannelModel::Gray) {
        return QStringLiteral("X: %1  Y: %2  |  Gray: %3")
                .arg(sample.imagePosition.x())
                .arg(sample.imagePosition.y())
                .arg(sample.gray);
    }

    if (sample.channelModel == FramePixelChannelModel::Rgb) {
        return QStringLiteral("X: %1  Y: %2  |  R: %3  G: %4  B: %5")
                .arg(sample.imagePosition.x())
                .arg(sample.imagePosition.y())
                .arg(sample.red)
                .arg(sample.green)
                .arg(sample.blue);
    }

    return emptyDisplayText();
}

QString FramePixelProbe::emptyDisplayText()
{
    return QStringLiteral("X: --  Y: --  |  R: --  G: --  B: --");
}

bool FramePixelProbe::equal(const FramePixelSample &left,
                            const FramePixelSample &right)
{
    return left.valid == right.valid
            && left.imagePosition == right.imagePosition
            && left.channelModel == right.channelModel
            && left.bitDepth == right.bitDepth
            && left.red == right.red
            && left.green == right.green
            && left.blue == right.blue
            && left.gray == right.gray;
}
