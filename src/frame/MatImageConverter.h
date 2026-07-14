#ifndef FRAME_MATIMAGECONVERTER_H
#define FRAME_MATIMAGECONVERTER_H

#include <QImage>
#include <QString>

#include <opencv2/core.hpp>

class MatImageConverter
{
public:
    static QImage matToDisplayImage(const cv::Mat &mat, QString *debugInfo = nullptr);
    static QImage matToDisplayImage(const cv::Mat &mat,
                                    const QString &source,
                                    QString *debugInfo = nullptr);
};

#endif // FRAME_MATIMAGECONVERTER_H
