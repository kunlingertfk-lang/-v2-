#include "frame/FramePixelProbe.h"
#include "frame/FrameViewHelper.h"

#include <QApplication>
#include <QEvent>
#include <QGraphicsView>
#include <QLabel>
#include <QMouseEvent>

#include <cstdlib>
#include <iostream>

namespace {

void check(bool condition, const char *message)
{
    if (condition)
        return;

    std::cerr << "frame_view_helper_pixel_probe_smoke: " << message << std::endl;
    std::exit(1);
}

void sendMouseMove(QWidget *viewport, const QPoint &position)
{
    QMouseEvent event(QEvent::MouseMove, QPointF(position),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(viewport, &event);
    QApplication::processEvents();
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QImage rgb(4, 3, QImage::Format_RGB888);
    rgb.fill(Qt::black);
    rgb.setPixelColor(1, 1, QColor(12, 34, 56));
    const FramePixelSample rgbSample = FramePixelProbe::sample(rgb, QPoint(1, 1));
    check(rgbSample.valid
          && rgbSample.channelModel == FramePixelChannelModel::Rgb
          && rgbSample.bitDepth == 8
          && rgbSample.red == 12 && rgbSample.green == 34 && rgbSample.blue == 56,
          "RGB888 sampling must preserve displayed RGB channels");

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QImage bgr(2, 1, QImage::Format_BGR888);
    bgr.setPixelColor(0, 0, QColor(90, 80, 70));
    const FramePixelSample bgrSample = FramePixelProbe::sample(bgr, QPoint(0, 0));
    check(bgrSample.valid && bgrSample.red == 90
          && bgrSample.green == 80 && bgrSample.blue == 70,
          "BGR888 storage must be reported as displayed RGB channels");
#endif

    QImage gray8(2, 1, QImage::Format_Grayscale8);
    gray8.setPixelColor(0, 0, QColor(73, 73, 73));
    const FramePixelSample gray8Sample = FramePixelProbe::sample(gray8, QPoint(0, 0));
    check(gray8Sample.valid
          && gray8Sample.channelModel == FramePixelChannelModel::Gray
          && gray8Sample.bitDepth == 8 && gray8Sample.gray == 73,
          "Grayscale8 sampling must return Gray 0-255");

#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    QImage gray16(2, 1, QImage::Format_Grayscale16);
    quint16 *gray16Row = reinterpret_cast<quint16 *>(gray16.scanLine(0));
    gray16Row[0] = 1024;
    gray16Row[1] = 65535;
    const FramePixelSample gray16Sample = FramePixelProbe::sample(gray16, QPoint(0, 0));
    check(gray16Sample.valid
          && gray16Sample.channelModel == FramePixelChannelModel::Gray
          && gray16Sample.bitDepth == 16 && gray16Sample.gray == 1024,
          "Grayscale16 sampling must preserve the 16-bit value");
#endif

    check(!FramePixelProbe::sample(QImage(), QPoint(0, 0)).valid,
          "empty images must return an invalid sample");
    check(!FramePixelProbe::sample(rgb, QPoint(-1, 0)).valid
          && !FramePixelProbe::sample(rgb, QPoint(rgb.width(), 0)).valid,
          "out-of-range coordinates must return an invalid sample");

    QGraphicsView view;
    view.resize(420, 320);
    FrameViewHelper helper(&view);
    QLabel statusLabel;
    helper.bindPixelStatusLabel(&statusLabel);
    helper.setImage(rgb);
    helper.setNavigationEnabled(true);
    view.show();
    QApplication::processEvents();

    const QPoint originalViewPoint = view.mapFromScene(QPointF(1.5, 1.5));
    sendMouseMove(view.viewport(), originalViewPoint);
    check(statusLabel.text() == QStringLiteral("X: 1  Y: 1  |  R: 12  G: 34  B: 56"),
          "fit-to-view hover must report the original image pixel");

    helper.zoomIn();
    const QPoint zoomedViewPoint = view.mapFromScene(QPointF(1.5, 1.5));
    sendMouseMove(view.viewport(), zoomedViewPoint);
    check(statusLabel.text() == QStringLiteral("X: 1  Y: 1  |  R: 12  G: 34  B: 56"),
          "zoom must not change original image coordinates or RGB values");

    const QPoint outsideViewPoint = view.mapFromScene(QPointF(-1.0, -1.0));
    sendMouseMove(view.viewport(), outsideViewPoint);
    check(statusLabel.text() == FramePixelProbe::emptyDisplayText(),
          "positions outside the image must not clamp to an edge pixel");

    sendMouseMove(view.viewport(), zoomedViewPoint);
    QEvent leaveEvent(QEvent::Leave);
    QApplication::sendEvent(view.viewport(), &leaveEvent);
    check(statusLabel.text() == FramePixelProbe::emptyDisplayText(),
          "leaving the viewport must clear the pixel status");

    helper.clear();
    check(statusLabel.text() == FramePixelProbe::emptyDisplayText(),
          "clearing the image must keep an invalid pixel status");

    std::cout << "frame_view_helper_pixel_probe_smoke: PASS" << std::endl;
    return 0;
}
