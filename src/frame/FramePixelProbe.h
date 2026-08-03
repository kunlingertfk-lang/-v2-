#ifndef FRAME_FRAMEPIXELPROBE_H
#define FRAME_FRAMEPIXELPROBE_H

#include <QImage>
#include <QMetaType>
#include <QPoint>
#include <QString>

enum class FramePixelChannelModel
{
    Unknown,
    Gray,
    Rgb
};

struct FramePixelSample
{
    bool valid = false;
    QPoint imagePosition;
    FramePixelChannelModel channelModel = FramePixelChannelModel::Unknown;
    int bitDepth = 0;
    quint16 red = 0;
    quint16 green = 0;
    quint16 blue = 0;
    quint16 gray = 0;
};

Q_DECLARE_METATYPE(FramePixelSample)

class FramePixelProbe
{
public:
    static FramePixelSample sample(const QImage &image, const QPoint &imagePosition);
    static QString displayText(const FramePixelSample &sample);
    static QString emptyDisplayText();
    static bool equal(const FramePixelSample &left, const FramePixelSample &right);
};

#endif // FRAME_FRAMEPIXELPROBE_H
