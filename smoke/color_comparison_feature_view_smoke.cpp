#include "ColorComparisonFeatureView.h"

#include <QApplication>
#include <QAbstractScrollArea>
#include <QColor>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollBar>
#include <QWheelEvent>

#include <cmath>
#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    }
}

void clickWidget(QWidget *widget, const QPoint &position = QPoint())
{
    const QPoint target = position.isNull() ? widget->rect().center() : position;
    QMouseEvent press(QEvent::MouseButtonPress, target,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, target,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(widget, &release);
    QApplication::processEvents();
}

void sendWheel(QWidget *widget, const QPoint &position, int angleDelta)
{
    QWheelEvent wheel(QPointF(position),
                      QPointF(widget->mapToGlobal(position)),
                      QPoint(), QPoint(0, angleDelta),
                      Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(widget, &wheel);
    QApplication::processEvents();
}

QColor jointCellColor(QWidget *chart, int hue, int saturation)
{
    QImage rendered(chart->size(), QImage::Format_ARGB32_Premultiplied);
    rendered.fill(Qt::transparent);
    chart->render(&rendered);
    const QRectF available = QRectF(chart->rect()).adjusted(18, 24, -8, -18);
    const double side = qMin(available.width(), available.height());
    const QRectF plot(available.center().x() - side / 2.0,
                      available.center().y() - side / 2.0,
                      side, side);
    const double cellWidth = plot.width() / 32.0;
    const double cellHeight = plot.height() / 32.0;
    const QPoint sample(qRound(plot.left() + (hue + 0.5) * cellWidth),
                        qRound(plot.top() + (31 - saturation + 0.5)
                               * cellHeight));
    return rendered.pixelColor(sample);
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QVector<double> hs(1024, 0.0);
    hs[7 * 32 + 11] = 1.0;
    QVector<double> value(32, 0.0);
    value[18] = 1.0;
    check(ColorComparisonFeatureView::validNormalizedHistogram(hs, 1024),
          "normalized 1024-value HS histogram must be valid");
    check(!ColorComparisonFeatureView::validNormalizedHistogram(
              QVector<double>(1023, 0.0), 1024),
          "wrong-sized HS histogram must be rejected");

    QVector<double> hue;
    QVector<double> saturation;
    ColorComparisonFeatureView::hsMarginals(hs, &hue, &saturation);
    check(hue.size() == 32 && saturation.size() == 32,
          "HS projection must produce two 32-bin marginals");
    check(std::abs(hue.at(7) - 1.0) < 1e-12
          && std::abs(saturation.at(11) - 1.0) < 1e-12,
          "hue_major projection must preserve H/S coordinates");

    QVector<double> templateHs(1024, 0.0);
    QVector<double> detectionHs(1024, 0.0);
    templateHs[7 * 32 + 11] = 0.5;
    detectionHs[7 * 32 + 11] = 0.5;
    templateHs[17 * 32 + 5] = 0.5;
    detectionHs[24 * 32 + 20] = 0.5;

    ColorComparisonFeatureView view;
    view.resize(1280, 900);
    view.setTemplateHistograms(templateHs, value);
    check(view.setDetectionHistograms(detectionHs, value, 0.5, 50.0, 80),
          "valid detection diagnostics must be accepted");
    view.show();
    QApplication::processEvents();

    QWidget *hueChart = view.findChild<QWidget *>(
                QStringLiteral("colorComparisonHFeatureChart"));
    check(hueChart != nullptr, "H chart must be discoverable");
    if (hueChart) {
        QImage rendered(hueChart->size(), QImage::Format_ARGB32_Premultiplied);
        rendered.fill(Qt::transparent);
        hueChart->render(&rendered);
        int brightOverlapPixels = 0;
        for (int y = 0; y < rendered.height(); ++y) {
            for (int x = 0; x < rendered.width(); ++x) {
                const QColor pixel = rendered.pixelColor(x, y);
                if (pixel.green() >= 210 && pixel.red() >= 120
                        && pixel.blue() <= 90) {
                    ++brightOverlapPixels;
                }
            }
        }
        check(brightOverlapPixels >= 8,
              "overlap must use a visible solid bright-green area");
        clickWidget(hueChart);
        check(view.zoomVisible(), "clicking H chart must open zoom overlay");
    }

    QWidget *overlay = view.findChild<QWidget *>(
                QStringLiteral("colorComparisonFeatureZoomOverlay"));
    QWidget *expanded = view.findChild<QWidget *>(
                QStringLiteral("colorComparisonExpandedFeatureChart"));
    QAbstractScrollArea *scroll = view.findChild<QAbstractScrollArea *>(
                QStringLiteral("colorComparisonFeatureZoomScrollArea"));
    check(overlay && overlay->property("zoomPercent").toInt() == 100,
          "newly opened chart must start at 100 percent");
    check(expanded && expanded->width() >= 900 && expanded->height() >= 480,
          "expanded 1D chart must start substantially larger than before");
    QSize hueBase;
    if (expanded && scroll) {
        hueBase = expanded->size();
        sendWheel(scroll->viewport(), scroll->viewport()->rect().center(), 120);
        check(overlay->property("zoomPercent").toInt() == 125
              && expanded->size() == QSize(qRound(hueBase.width() * 1.25),
                                           qRound(hueBase.height() * 1.25)),
              "one wheel step must scale the chart proportionally to 125 percent");
        for (int i = 0; i < 8; ++i)
            sendWheel(scroll->viewport(), scroll->viewport()->rect().center(), 120);
        check(overlay->property("zoomPercent").toInt() == 200,
              "wheel zoom must stop at 200 percent");
        for (int i = 0; i < 8; ++i)
            sendWheel(scroll->viewport(), scroll->viewport()->rect().center(), -120);
        check(overlay->property("zoomPercent").toInt() == 100
              && expanded->size() == hueBase,
              "wheel zoom must stop at 100 percent");
    }

    QPushButton *closeButton = view.findChild<QPushButton *>(
                QStringLiteral("colorComparisonFeatureZoomClose"));
    check(closeButton != nullptr, "zoom overlay must expose close button");
    check(closeButton
          && closeButton->property("actionRole").toString()
             == QStringLiteral("featureZoomClose"),
          "zoom close button must use the public-QSS action role");
    check(closeButton && closeButton->styleSheet().isEmpty(),
          "zoom close button must not override the public QSS inline");
    if (closeButton) {
        closeButton->click();
        QApplication::processEvents();
        check(!view.zoomVisible(), "close button must hide zoom overlay");
    }

    QWidget *jointChart = view.findChild<QWidget *>(
                QStringLiteral("colorComparisonHSFeatureChart"));
    check(jointChart != nullptr, "HS joint chart must be discoverable");
    if (jointChart) {
        const QColor overlap = jointCellColor(jointChart, 7, 11);
        const QColor templateOnly = jointCellColor(jointChart, 17, 5);
        const QColor detectionOnly = jointCellColor(jointChart, 24, 20);
        check(overlap.green() >= overlap.red() + 45
              && overlap.green() >= overlap.blue() + 100,
              "HS overlap cell must be visibly bright green");
        check(templateOnly.red() > templateOnly.green()
              && templateOnly.red() > templateOnly.blue(),
              "HS template excess cell must remain orange");
        check(detectionOnly.blue() > detectionOnly.red()
              && detectionOnly.green() > detectionOnly.red(),
              "HS detection excess cell must remain cyan");
        clickWidget(jointChart);
        QLabel *zoomTitle = view.findChild<QLabel *>(
                    QStringLiteral("colorComparisonFeatureZoomTitle"));
        QWidget *zoomCard = view.findChild<QWidget *>(
                    QStringLiteral("colorComparisonFeatureZoomCard"));
        check(view.zoomVisible(), "clicking HS chart must open zoom overlay");
        check(zoomTitle && zoomTitle->text().contains(QStringLiteral("32×32")),
              "HS zoom title must expose the 32x32 joint shape");
        check(zoomTitle
              && zoomTitle->property("role").toString()
                 == QStringLiteral("featureZoomTitle"),
              "zoom title must use the public-QSS title role");
        check(zoomTitle && zoomTitle->styleSheet().isEmpty(),
              "zoom title must not override the public QSS inline");
        check(zoomCard
              && zoomCard->property("panelRole").toString()
                 == QStringLiteral("featureZoomCard"),
              "zoom card must expose its public-QSS panel role");
        expanded = view.findChild<QWidget *>(
                    QStringLiteral("colorComparisonExpandedFeatureChart"));
        check(overlay && overlay->property("zoomPercent").toInt() == 100,
              "switching chart kind must reset zoom to 100 percent");
        check(expanded && expanded->width() == expanded->height(),
              "expanded HS chart canvas must be square");
        if (expanded && scroll) {
            QImage expandedRender(expanded->size(),
                                  QImage::Format_ARGB32_Premultiplied);
            expandedRender.fill(Qt::transparent);
            expanded->render(&expandedRender);
            const double plotWidth = expanded->property("jointPlotWidth").toDouble();
            const double plotHeight = expanded->property("jointPlotHeight").toDouble();
            check(plotWidth > 0.0 && qAbs(plotWidth - plotHeight) < 0.01,
                  "expanded HS actual plot area must be square");

            for (int i = 0; i < 4; ++i)
                sendWheel(scroll->viewport(), scroll->viewport()->rect().center(), 120);
            check(overlay->property("zoomPercent").toInt() == 200,
                  "HS chart must support zooming to 200 percent");
            QScrollBar *horizontal = scroll->horizontalScrollBar();
            QScrollBar *vertical = scroll->verticalScrollBar();
            check(horizontal->maximum() > 0 && vertical->maximum() > 0,
                  "200 percent HS chart must be scrollable on both axes");
            horizontal->setValue(qMax(1, horizontal->maximum() / 2));
            vertical->setValue(qMax(1, vertical->maximum() / 2));
            const int horizontalBefore = horizontal->value();
            const int verticalBefore = vertical->value();
            check(horizontalBefore > 0 && verticalBefore > 0,
                  "scroll preservation check must start at non-zero positions");
            check(view.setDetectionHistograms(detectionHs, value,
                                              0.5, 50.0, 80),
                  "updated diagnostics must remain valid");
            expanded = view.findChild<QWidget *>(
                        QStringLiteral("colorComparisonExpandedFeatureChart"));
            check(overlay->property("zoomPercent").toInt() == 200,
                  "same-kind histogram refresh must preserve zoom percent");
            check(horizontal->value() == horizontalBefore
                  && vertical->value() == verticalBefore,
                  "same-kind histogram refresh must preserve both scroll positions");
        }
        if (closeButton)
            closeButton->click();
        QApplication::processEvents();

        clickWidget(jointChart);
        check(overlay && overlay->property("zoomPercent").toInt() == 100,
              "closing and reopening the same chart must reset zoom");
        if (closeButton)
            closeButton->click();
        QApplication::processEvents();
    }

    if (hueChart) {
        clickWidget(hueChart);
        check(overlay != nullptr, "zoom overlay must be discoverable");
        if (overlay) {
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(overlay, &escape);
            QApplication::processEvents();
            check(!view.zoomVisible(), "Escape must hide zoom overlay");

            clickWidget(hueChart);
            clickWidget(overlay, QPoint(2, 2));
            check(!view.zoomVisible(), "clicking overlay shade must close zoom");
        }
    }

    view.clearDetection(QStringLiteral("test failure"));
    check(!view.setDetectionHistograms(QVector<double>(1024, 0.0),
                                       value, 0.0, 0.0, 80),
          "zero-mass detection histogram must be rejected");

    if (failures) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "color_comparison_feature_view_smoke: all checks passed"
              << std::endl;
    return 0;
}
