#include "frame/MatImageConverter.h"

#include <QByteArray>
#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include <opencv2/imgproc.hpp>

namespace {

bool displayConvertDebugEnabled()
{
    static const bool enabled = []() {
        const QByteArray value = qgetenv("V2_DISPLAY_CONVERT_DEBUG").trimmed().toLower();
        return !value.isEmpty()
                && value != QByteArrayLiteral("0")
                && value != QByteArrayLiteral("false")
                && value != QByteArrayLiteral("off");
    }();
    return enabled;
}

QString qImageFormatName(QImage::Format format)
{
    switch (format) {
    case QImage::Format_Grayscale8:
        return QStringLiteral("Grayscale8");
    case QImage::Format_RGB888:
        return QStringLiteral("RGB888");
    case QImage::Format_RGBA8888:
        return QStringLiteral("RGBA8888");
    default:
        return QStringLiteral("format=%1").arg(static_cast<int>(format));
    }
}

void logDisplayConversion(const QString &source,
                          const cv::Mat &mat,
                          const QString &route,
                          QImage::Format format)
{
    if (!displayConvertDebugEnabled())
        return;

    static QMutex mutex;
    static QHash<QString, int> counts;

    const QString key = source.trimmed().isEmpty()
            ? QStringLiteral("MatImageConverter")
            : source.trimmed();

    QMutexLocker locker(&mutex);
    const int count = counts.value(key, 0);
    if (count >= 3)
        return;
    counts.insert(key, count + 1);

    qDebug().noquote()
            << QStringLiteral("[DisplayConvert] source=%1 mat=%2x%3 type=%4 channels=%5 route=%6 qformat=%7")
               .arg(key)
               .arg(mat.cols)
               .arg(mat.rows)
               .arg(mat.type())
               .arg(mat.channels())
               .arg(route)
               .arg(qImageFormatName(format));
}

} // namespace

QImage MatImageConverter::matToDisplayImage(const cv::Mat &mat, QString *debugInfo)
{
    return matToDisplayImage(mat, QString(), debugInfo);
}

QImage MatImageConverter::matToDisplayImage(const cv::Mat &mat,
                                            const QString &source,
                                            QString *debugInfo)
{
    if (debugInfo)
        debugInfo->clear();

    if (mat.empty()) {
        if (debugInfo)
            *debugInfo = QStringLiteral("empty");
        return QImage();
    }

    if (mat.type() == CV_8UC1) {
        if (debugInfo)
            *debugInfo = QStringLiteral("CV_8UC1->Grayscale8");
        logDisplayConversion(source, mat, QStringLiteral("gray"), QImage::Format_Grayscale8);
        QImage image(mat.data,
                     mat.cols,
                     mat.rows,
                     static_cast<int>(mat.step),
                     QImage::Format_Grayscale8);
        return image.copy();
    }

    if (mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        if (debugInfo)
            *debugInfo = QStringLiteral("CV_8UC3 BGR->RGB888");
        logDisplayConversion(source, mat, QStringLiteral("BGR2RGB"), QImage::Format_RGB888);
        QImage image(rgb.data,
                     rgb.cols,
                     rgb.rows,
                     static_cast<int>(rgb.step),
                     QImage::Format_RGB888);
        return image.copy();
    }

    if (mat.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        if (debugInfo)
            *debugInfo = QStringLiteral("CV_8UC4 BGRA->RGBA8888");
        logDisplayConversion(source, mat, QStringLiteral("BGRA2RGBA"), QImage::Format_RGBA8888);
        QImage image(rgba.data,
                     rgba.cols,
                     rgba.rows,
                     static_cast<int>(rgba.step),
                     QImage::Format_RGBA8888);
        return image.copy();
    }

    if (debugInfo) {
        *debugInfo = QStringLiteral("unsupported type=%1 channels=%2 depth=%3")
                .arg(mat.type())
                .arg(mat.channels())
                .arg(mat.depth());
    }
    return QImage();
}
