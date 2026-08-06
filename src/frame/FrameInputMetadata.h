#ifndef FRAME_FRAMEINPUTMETADATA_H
#define FRAME_FRAMEINPUTMETADATA_H

#include <QImage>
#include <QJsonObject>
#include <QString>

#include <opencv2/core.hpp>

struct FrameInputMetadata
{
    QString colorMode = QStringLiteral("unknown");
    QString pixelFormat;
    int originalChannels = 0;
    int originalDepth = -1;
    int validBits = -1;
    int bitShift = -1;
    QString source;

    static FrameInputMetadata fromMat(const cv::Mat &image,
                                      const QString &source);
    static FrameInputMetadata fromQImage(const QImage &image,
                                         const QString &source);
    static FrameInputMetadata fromJson(const QJsonObject &json);
    QJsonObject toJson() const;
    bool isMono() const;
    bool isSupportedColor8() const;
};

#endif // FRAME_FRAMEINPUTMETADATA_H
