#include "ColorComparisonFeatureView.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>
#include <functional>

namespace {

constexpr int kBins = 32;
constexpr int kHsValues = kBins * kBins;
constexpr double kNormalizationTolerance = 1e-5;

enum class ChartKind { Hue, Saturation, Value, Joint };

QString chartName(ChartKind kind)
{
    switch (kind) {
    case ChartKind::Hue: return QStringLiteral("H");
    case ChartKind::Saturation: return QStringLiteral("S");
    case ChartKind::Value: return QStringLiteral("V");
    case ChartKind::Joint: return QStringLiteral("HS");
    }
    return QString();
}

QColor channelColor(ChartKind kind)
{
    switch (kind) {
    case ChartKind::Hue: return QColor(255, 122, 0);
    case ChartKind::Saturation: return QColor(31, 189, 255);
    case ChartKind::Value: return QColor(164, 224, 75);
    case ChartKind::Joint: return QColor(132, 204, 22);
    }
    return QColor(255, 122, 0);
}

QColor blendedCellColor(double overlap,
                        double templateExcess,
                        double detectionExcess,
                        double maximum)
{
    if (maximum <= 0.0)
        return QColor(24, 32, 51);

    const double total = overlap + templateExcess + detectionExcess;
    if (total <= 0.0)
        return QColor(24, 32, 51);

    const QColor overlapColor(163, 230, 53);
    const QColor templateColor(249, 115, 22);
    const QColor detectionColor(6, 182, 212);
    const double red = (overlap * overlapColor.red()
                        + templateExcess * templateColor.red()
                        + detectionExcess * detectionColor.red()) / total;
    const double green = (overlap * overlapColor.green()
                          + templateExcess * templateColor.green()
                          + detectionExcess * detectionColor.green()) / total;
    const double blue = (overlap * overlapColor.blue()
                         + templateExcess * templateColor.blue()
                         + detectionExcess * detectionColor.blue()) / total;
    const double intensity = std::sqrt(qBound(0.0, total / maximum, 1.0));
    const QColor background(24, 32, 51);
    return QColor(qRound(background.red() + (red - background.red()) * intensity),
                  qRound(background.green() + (green - background.green()) * intensity),
                  qRound(background.blue() + (blue - background.blue()) * intensity));
}

class FeatureChart : public QWidget
{
public:
    FeatureChart(ChartKind kind, QWidget *parent = nullptr)
        : QWidget(parent), m_kind(kind)
    {
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        updateIdentity();
    }

    void setKind(ChartKind kind)
    {
        m_kind = kind;
        updateIdentity();
        update();
    }

    void setHistograms(const QVector<double> &templateValues,
                       const QVector<double> &detectionValues)
    {
        m_template = templateValues;
        m_detection = detectionValues;
        update();
    }

    void setExpanded(bool expanded)
    {
        m_expanded = expanded;
        updateIdentity();
        update();
    }

    void setClickHandler(const std::function<void(ChartKind)> &handler)
    {
        m_handler = handler;
    }

protected:
    void enterEvent(QEvent *event) override
    {
        m_hovered = true;
        update();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hovered = false;
        update();
        QWidget::leaveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos())
                && m_handler) {
            m_handler(m_kind);
        }
        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.fillRect(rect(), QColor(37, 42, 49));
        painter.setPen(QPen(m_hovered ? QColor(245, 158, 11)
                                     : QColor(17, 24, 39),
                            m_hovered ? 2.0 : 1.0));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));

        painter.setPen(QColor(209, 213, 219));
        QFont labelFont = font();
        labelFont.setBold(true);
        labelFont.setPixelSize(m_expanded ? 22 : 18);
        painter.setFont(labelFont);
        painter.drawText(QRect(6, 3, width() - 12, 20),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         chartName(m_kind));
        painter.setPen(QColor(245, 158, 11));
        painter.drawText(QRect(width() - 24, 3, 18, 20),
                         Qt::AlignCenter, QStringLiteral("⌕"));

        if (m_kind == ChartKind::Joint)
            paintJoint(&painter);
        else
            paintBars(&painter);
    }

private:
    void paintBars(QPainter *painter)
    {
        const QRectF plot = QRectF(rect()).adjusted(m_expanded ? 22 : 6,
                                                   m_expanded ? 36 : 24,
                                                   m_expanded ? -18 : -5,
                                                   m_expanded ? -22 : -5);
        if (plot.width() <= 0 || plot.height() <= 0)
            return;

        double maximum = 0.0;
        for (double value : m_template)
            maximum = qMax(maximum, value);
        for (double value : m_detection)
            maximum = qMax(maximum, value);
        if (maximum <= 0.0)
            return;

        const int count = qMax(m_template.size(), m_detection.size());
        if (count <= 0)
            return;
        const double slotWidth = plot.width() / count;
        const QColor baseColor = channelColor(m_kind);
        for (int index = 0; index < count; ++index) {
            const double templateValue = index < m_template.size()
                    ? m_template.at(index) : 0.0;
            const double detectionValue = index < m_detection.size()
                    ? m_detection.at(index) : 0.0;
            const double templateHeight = plot.height() * templateValue / maximum;
            const double detectionHeight = plot.height() * detectionValue / maximum;
            const double overlapHeight = qMin(templateHeight, detectionHeight);
            const QRectF templateRect(plot.left() + index * slotWidth,
                                      plot.bottom() - templateHeight,
                                      qMax(1.0, slotWidth - 0.7),
                                      templateHeight);
            painter->fillRect(templateRect, baseColor);

            if (overlapHeight > 0.0) {
                const QRectF overlapRect(plot.left() + index * slotWidth,
                                         plot.bottom() - overlapHeight,
                                         qMax(1.0, slotWidth - 0.7),
                                         overlapHeight);
                const QColor overlapFill(163, 230, 53, 235);
                const QColor overlapEdge(54, 83, 20);
                painter->fillRect(overlapRect, overlapFill);
                painter->setPen(QPen(overlapEdge, m_expanded ? 1.5 : 1.0));
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(overlapRect);

                if (overlapHeight > 1.0 && slotWidth >= 2.0) {
                    painter->save();
                    painter->setClipRect(overlapRect);
                    painter->setPen(QPen(QColor(54, 83, 20, 150), 1.0));
                    const double left = overlapRect.left();
                    for (double y = plot.bottom();
                         y >= plot.bottom() - overlapHeight - slotWidth;
                         y -= 7.0) {
                        painter->drawLine(QPointF(left, y),
                                          QPointF(left + slotWidth,
                                                  y - slotWidth));
                    }
                    painter->restore();
                }
            }

            if (detectionHeight > 0.0) {
                const QRectF detectionRect(plot.left() + index * slotWidth + 0.8,
                                           plot.bottom() - detectionHeight,
                                           qMax(1.0, slotWidth - 2.0),
                                           detectionHeight);
                painter->setPen(QPen(QColor(226, 232, 240),
                                     m_expanded ? 1.5 : 1.0));
                painter->setBrush(Qt::NoBrush);
                painter->drawRect(detectionRect);
            }
        }
    }

    void updateIdentity()
    {
        setMinimumSize(m_kind == ChartKind::Joint ? QSize(118, 118)
                                                   : QSize(76, 72));
        setObjectName(m_expanded
                      ? QStringLiteral("colorComparisonExpandedFeatureChart")
                      : QStringLiteral("colorComparison%1FeatureChart")
                        .arg(chartName(m_kind)));
        setAccessibleName(chartName(m_kind));
    }

    void paintJoint(QPainter *painter)
    {
        const QRectF available = QRectF(rect()).adjusted(m_expanded ? 38 : 18,
                                                        m_expanded ? 34 : 24,
                                                        m_expanded ? -22 : -8,
                                                        m_expanded ? -32 : -18);
        const double side = qMin(available.width(), available.height());
        const QRectF plot(available.center().x() - side / 2.0,
                          available.center().y() - side / 2.0,
                          side, side);
        setProperty("jointPlotWidth", plot.width());
        setProperty("jointPlotHeight", plot.height());
        if (plot.width() <= 0 || plot.height() <= 0)
            return;

        double maximum = 0.0;
        for (double value : m_template)
            maximum = qMax(maximum, value);
        for (double value : m_detection)
            maximum = qMax(maximum, value);
        if (maximum <= 0.0)
            return;

        const double cellWidth = plot.width() / kBins;
        const double cellHeight = plot.height() / kBins;
        for (int hue = 0; hue < kBins; ++hue) {
            for (int saturation = 0; saturation < kBins; ++saturation) {
                const int index = hue * kBins + saturation;
                const double templateValue = index < m_template.size()
                        ? m_template.at(index) : 0.0;
                const double detectionValue = index < m_detection.size()
                        ? m_detection.at(index) : 0.0;
                const double overlap = qMin(templateValue, detectionValue);
                const QColor color = blendedCellColor(
                            overlap,
                            qMax(0.0, templateValue - detectionValue),
                            qMax(0.0, detectionValue - templateValue),
                            maximum);
                painter->fillRect(QRectF(plot.left() + hue * cellWidth,
                                         plot.top()
                                         + (kBins - 1 - saturation) * cellHeight,
                                         qMax(1.0, cellWidth + 0.2),
                                         qMax(1.0, cellHeight + 0.2)),
                                  color);
            }
        }
        painter->setPen(QColor(203, 213, 225));
        painter->drawText(QRectF(2, plot.top(), 16, plot.height()),
                          Qt::AlignHCenter | Qt::AlignTop,
                          QStringLiteral("S"));
        painter->drawText(QRectF(plot.left(), plot.bottom() + 2,
                                 plot.width(), 16),
                          Qt::AlignRight | Qt::AlignVCenter,
                          QStringLiteral("H →"));
    }

    ChartKind m_kind;
    QVector<double> m_template;
    QVector<double> m_detection;
    std::function<void(ChartKind)> m_handler;
    bool m_hovered = false;
    bool m_expanded = false;
};

QFrame *legendSwatch(const QColor &color,
                     bool outline,
                     const QString &objectName,
                     QWidget *parent)
{
    Q_UNUSED(color)
    Q_UNUSED(outline)
    QFrame *swatch = new QFrame(parent);
    swatch->setObjectName(objectName);
    if (objectName.contains(QStringLiteral("Template")))
        swatch->setProperty("legendRole", QStringLiteral("template"));
    else if (objectName.contains(QStringLiteral("Detection")))
        swatch->setProperty("legendRole", QStringLiteral("detection"));
    else
        swatch->setProperty("legendRole", QStringLiteral("overlap"));
    swatch->setFixedSize(18, 14);
    return swatch;
}

void addLegendItem(QHBoxLayout *layout,
                   const QString &text,
                   const QColor &color,
                   bool outline,
                   const QString &objectName,
                   QWidget *parent)
{
    layout->addWidget(legendSwatch(color, outline, objectName, parent));
    QLabel *label = new QLabel(text, parent);
    label->setProperty("role", QStringLiteral("featureLegend"));
    layout->addWidget(label);
}

class ZoomOverlay : public QWidget
{
public:
    explicit ZoomOverlay(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("colorComparisonFeatureZoomOverlay"));
        setProperty("panelRole", QStringLiteral("featureZoomOverlay"));
        setAttribute(Qt::WA_StyledBackground, true);
        setFocusPolicy(Qt::StrongFocus);

        QVBoxLayout *outer = new QVBoxLayout(this);
        outer->setContentsMargins(36, 36, 36, 36);
        outer->addStretch();
        m_card = new QFrame(this);
        m_card->setObjectName(QStringLiteral("colorComparisonFeatureZoomCard"));
        m_card->setProperty("panelRole", QStringLiteral("featureZoomCard"));
        m_card->setMinimumWidth(720);
        QVBoxLayout *cardLayout = new QVBoxLayout(m_card);
        cardLayout->setContentsMargins(0, 0, 0, 12);
        QHBoxLayout *header = new QHBoxLayout;
        header->setContentsMargins(14, 10, 10, 4);
        m_title = new QLabel(m_card);
        m_title->setObjectName(QStringLiteral("colorComparisonFeatureZoomTitle"));
        m_title->setProperty("role", QStringLiteral("featureZoomTitle"));
        QPushButton *closeButton = new QPushButton(QObject::tr("× 关闭"), m_card);
        closeButton->setObjectName(QStringLiteral("colorComparisonFeatureZoomClose"));
        closeButton->setProperty("actionRole", QStringLiteral("featureZoomClose"));
        header->addWidget(m_title);
        header->addStretch();
        header->addWidget(closeButton);
        cardLayout->addLayout(header);

        m_chart = new FeatureChart(ChartKind::Hue, m_card);
        m_chart->setExpanded(true);
        m_chart->setCursor(Qt::ArrowCursor);
        m_scroll = new QScrollArea(m_card);
        m_scroll->setObjectName(
                    QStringLiteral("colorComparisonFeatureZoomScrollArea"));
        m_scroll->setWidgetResizable(false);
        m_scroll->setAlignment(Qt::AlignCenter);
        m_scroll->setFrameShape(QFrame::NoFrame);
        m_scroll->setWidget(m_chart);
        m_scroll->viewport()->installEventFilter(this);
        cardLayout->addWidget(m_scroll, 1);
        m_details = new QLabel(m_card);
        m_details->setObjectName(QStringLiteral("colorComparisonFeatureZoomDetails"));
        m_details->setProperty("role", QStringLiteral("featureZoomDetails"));
        m_details->setWordWrap(true);
        cardLayout->addWidget(m_details);
        outer->addWidget(m_card, 0, Qt::AlignHCenter);
        outer->addStretch();
        connect(closeButton, &QPushButton::clicked, this, &ZoomOverlay::hide);
        setProperty("zoomPercent", 100);
        hide();
    }

    void showChart(ChartKind kind,
                   const QVector<double> &templateValues,
                   const QVector<double> &detectionValues,
                   const QString &details)
    {
        const bool wasVisible = isVisible();
        const bool resetZoom = !wasVisible || kind != m_activeKind;
        if (resetZoom)
            m_zoomIndex = 0;
        m_activeKind = kind;
        m_chart->setKind(kind);
        m_chart->setHistograms(templateValues, detectionValues);
        m_title->setText(kind == ChartKind::Joint
                         ? QObject::tr("HS 二维联合直方图（32×32）")
                         : QObject::tr("%1 直方图对比（32 bins）")
                           .arg(chartName(kind)));
        m_details->setText(details);
        syncGeometry(resetZoom);
        show();
        raise();
        setFocus(Qt::OtherFocusReason);
    }

    void syncGeometry(bool recalculateBase = true)
    {
        if (!parentWidget())
            return;
        setGeometry(parentWidget()->rect());
        const QSize available = parentWidget()->size();
        const QSize cardSize(qMax(720, qRound(available.width() * 0.85)),
                             qMax(520, qRound(available.height() * 0.85)));
        m_card->setFixedSize(qMax(1, qMin(cardSize.width(),
                                         available.width() - 24)),
                             qMax(1, qMin(cardSize.height(),
                                         available.height() - 24)));
        if (!recalculateBase)
            return;

        const int availableWidth = qMax(1, m_card->width() - 28);
        const int availableHeight = qMax(1, m_card->height() - 170);
        if (m_activeKind == ChartKind::Joint) {
            const int edge = qMin(availableWidth, availableHeight);
            m_baseChartSize = QSize(edge, edge);
        } else {
            int width = availableWidth;
            int height = qRound(width * 9.0 / 16.0);
            if (height > availableHeight) {
                height = availableHeight;
                width = qRound(height * 16.0 / 9.0);
            }
            m_baseChartSize = QSize(width, height);
        }
        applyZoom();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (m_scroll && watched == m_scroll->viewport()
                && event->type() == QEvent::Wheel) {
            QWheelEvent *wheel = static_cast<QWheelEvent *>(event);
            if (wheel->angleDelta().y() > 0)
                m_zoomIndex = qMin(m_zoomIndex + 1, m_zoomSteps.size() - 1);
            else if (wheel->angleDelta().y() < 0)
                m_zoomIndex = qMax(m_zoomIndex - 1, 0);
            applyZoom();
            wheel->accept();
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            hide();
            event->accept();
            return;
        }
        QWidget::keyPressEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        if (m_card && !m_card->geometry().contains(event->pos())) {
            hide();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

private:
    void applyZoom()
    {
        const int percent = m_zoomSteps.at(m_zoomIndex);
        m_chart->setFixedSize(qRound(m_baseChartSize.width() * percent / 100.0),
                              qRound(m_baseChartSize.height() * percent / 100.0));
        setProperty("zoomPercent", percent);
    }

    QFrame *m_card = nullptr;
    QLabel *m_title = nullptr;
    FeatureChart *m_chart = nullptr;
    QScrollArea *m_scroll = nullptr;
    QLabel *m_details = nullptr;
    QVector<int> m_zoomSteps{100, 125, 150, 175, 200};
    int m_zoomIndex = 0;
    QSize m_baseChartSize;
    ChartKind m_activeKind = ChartKind::Hue;
};

} // namespace

class ColorComparisonFeatureView::Impl
{
public:
    explicit Impl(ColorComparisonFeatureView *owner)
        : q(owner)
    {
        QVBoxLayout *root = new QVBoxLayout(q);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(7);

        QHBoxLayout *legend = new QHBoxLayout;
        legend->setContentsMargins(0, 0, 0, 0);
        legend->setSpacing(4);
        addLegendItem(legend, QObject::tr("模板"), QColor(249, 115, 22), false,
                      QStringLiteral("colorComparisonTemplateLegend"), q);
        legend->addSpacing(5);
        addLegendItem(legend, QObject::tr("检测"), QColor(100, 116, 139), true,
                      QStringLiteral("colorComparisonDetectionLegend"), q);
        legend->addSpacing(5);
        addLegendItem(legend, QObject::tr("重合"), QColor(132, 204, 22), false,
                      QStringLiteral("colorComparisonOverlapLegend"), q);
        legend->addStretch();
        root->addLayout(legend);

        QHBoxLayout *oneDimensional = new QHBoxLayout;
        oneDimensional->setContentsMargins(0, 0, 0, 0);
        oneDimensional->setSpacing(6);
        hue = new FeatureChart(ChartKind::Hue, q);
        saturation = new FeatureChart(ChartKind::Saturation, q);
        value = new FeatureChart(ChartKind::Value, q);
        oneDimensional->addWidget(hue);
        oneDimensional->addWidget(saturation);
        oneDimensional->addWidget(value);
        root->addLayout(oneDimensional);

        QHBoxLayout *jointRow = new QHBoxLayout;
        jointRow->setContentsMargins(0, 0, 0, 0);
        jointRow->setSpacing(8);
        joint = new FeatureChart(ChartKind::Joint, q);
        joint->setMaximumWidth(150);
        jointRow->addWidget(joint, 1);
        metrics = new QLabel(q);
        metrics->setObjectName(QStringLiteral("colorComparisonHistogramMetrics"));
        metrics->setProperty("role", QStringLiteral("featureMetrics"));
        metrics->setWordWrap(true);
        metrics->setMinimumWidth(104);
        jointRow->addWidget(metrics, 1);
        root->addLayout(jointRow);

        state = new QLabel(QObject::tr("等待测试运行"), q);
        state->setObjectName(QStringLiteral("colorComparisonHistogramState"));
        state->setProperty("role", QStringLiteral("featureState"));
        state->setWordWrap(true);
        root->addWidget(state);

        const std::function<void(ChartKind)> handler = [this](ChartKind kind) {
            showZoom(kind);
        };
        hue->setClickHandler(handler);
        saturation->setClickHandler(handler);
        value->setClickHandler(handler);
        joint->setClickHandler(handler);
        refresh();
    }

    void refresh()
    {
        QVector<double> templateHue;
        QVector<double> templateSaturation;
        QVector<double> detectionHue;
        QVector<double> detectionSaturation;
        ColorComparisonFeatureView::hsMarginals(templateHs,
                                                &templateHue,
                                                &templateSaturation);
        ColorComparisonFeatureView::hsMarginals(detectionHs,
                                                &detectionHue,
                                                &detectionSaturation);
        hue->setHistograms(templateHue, detectionHue);
        saturation->setHistograms(templateSaturation, detectionSaturation);
        value->setHistograms(templateV, detectionV);
        joint->setHistograms(templateHs, detectionHs);
        const QString smoothedText = detectionAvailable && smoothedHsScore >= 0.0
                ? QString::number(smoothedHsScore, 'f', 1)
                : QStringLiteral("--");
        const QString brightnessText = detectionAvailable && brightnessFactor >= 0.0
                ? QStringLiteral("%1（系数 %2）")
                  .arg(brightnessState.isEmpty()
                       ? QObject::tr("未知") : brightnessState,
                       QString::number(brightnessFactor, 'f', 3))
                : QStringLiteral("--");
        const QString saturationText = detectionAvailable && saturationFactor >= 0.0
                ? QString::number(saturationFactor, 'f', 3)
                : QStringLiteral("--");
        metrics->setText(QObject::tr("HS 原始重合：%1\n平滑 HS 分数：%2\n亮度处理：%3\n饱和度系数：%4\n最终得分：%5\n判定阈值：%6")
                         .arg(detectionAvailable
                              ? QString::number(rawIntersection * 100.0, 'f', 1)
                                + QStringLiteral("%")
                              : QStringLiteral("--"),
                              smoothedText,
                              brightnessText,
                              saturationText,
                              detectionAvailable
                              ? QString::number(score, 'f', 1)
                              : QStringLiteral("--"),
                              QString::number(threshold)));
        if (zoom && zoom->isVisible())
            showZoom(activeKind);
    }

    void ensureZoom()
    {
        QWidget *host = q->window();
        if (!host)
            return;
        if (!zoom || zoom->parentWidget() != host) {
            if (zoom)
                delete zoom;
            zoom = new ZoomOverlay(host);
        }
        zoom->setGeometry(host->rect());
    }

    void showZoom(ChartKind kind)
    {
        ensureZoom();
        if (!zoom)
            return;
        activeKind = kind;
        QVector<double> templateValues;
        QVector<double> detectionValues;
        if (kind == ChartKind::Joint) {
            templateValues = templateHs;
            detectionValues = detectionHs;
        } else if (kind == ChartKind::Value) {
            templateValues = templateV;
            detectionValues = detectionV;
        } else {
            QVector<double> templateHue;
            QVector<double> templateSaturation;
            QVector<double> detectionHue;
            QVector<double> detectionSaturation;
            ColorComparisonFeatureView::hsMarginals(templateHs,
                                                    &templateHue,
                                                    &templateSaturation);
            ColorComparisonFeatureView::hsMarginals(detectionHs,
                                                    &detectionHue,
                                                    &detectionSaturation);
            templateValues = kind == ChartKind::Hue
                    ? templateHue : templateSaturation;
            detectionValues = kind == ChartKind::Hue
                    ? detectionHue : detectionSaturation;
        }
        const QString detail = detectionAvailable
                ? QObject::tr("模板：实心柱　检测：空心柱　重合：斜纹/绿色　"
                              "联合重合：%1%　得分：%2　阈值：%3")
                  .arg(QString::number(rawIntersection * 100.0, 'f', 1),
                       QString::number(score, 'f', 1),
                       QString::number(threshold))
                : QObject::tr("当前没有可用检测特征；仅显示模板分布。");
        zoom->showChart(kind, templateValues, detectionValues, detail);
    }

    ColorComparisonFeatureView *q = nullptr;
    FeatureChart *hue = nullptr;
    FeatureChart *saturation = nullptr;
    FeatureChart *value = nullptr;
    FeatureChart *joint = nullptr;
    QLabel *metrics = nullptr;
    QLabel *state = nullptr;
    ZoomOverlay *zoom = nullptr;
    ChartKind activeKind = ChartKind::Hue;
    QVector<double> templateHs;
    QVector<double> templateV;
    QVector<double> detectionHs;
    QVector<double> detectionV;
    double rawIntersection = 0.0;
    double score = 0.0;
    double smoothedHsScore = -1.0;
    double brightnessFactor = -1.0;
    double saturationFactor = -1.0;
    QString brightnessState;
    int threshold = 80;
    bool detectionAvailable = false;
};

ColorComparisonFeatureView::ColorComparisonFeatureView(QWidget *parent)
    : QWidget(parent), d(new Impl(this))
{
    setObjectName(QStringLiteral("colorComparisonFeatureView"));
    setProperty("panelRole", QStringLiteral("colorComparisonFeatureView"));
}

ColorComparisonFeatureView::~ColorComparisonFeatureView()
{
    closeZoom();
}

bool ColorComparisonFeatureView::validNormalizedHistogram(
        const QVector<double> &values, int expectedSize)
{
    if (values.size() != expectedSize)
        return false;
    double sum = 0.0;
    for (double value : values) {
        if (!std::isfinite(value) || value < 0.0)
            return false;
        sum += value;
    }
    return std::isfinite(sum) && std::abs(sum - 1.0) <= kNormalizationTolerance;
}

void ColorComparisonFeatureView::hsMarginals(
        const QVector<double> &hsHistogram,
        QVector<double> *hue,
        QVector<double> *saturation)
{
    if (hue)
        hue->fill(0.0, kBins);
    if (saturation)
        saturation->fill(0.0, kBins);
    if (hsHistogram.size() != kHsValues)
        return;
    for (int h = 0; h < kBins; ++h) {
        for (int s = 0; s < kBins; ++s) {
            const double value = hsHistogram.at(h * kBins + s);
            if (hue)
                (*hue)[h] += value;
            if (saturation)
                (*saturation)[s] += value;
        }
    }
}

void ColorComparisonFeatureView::setTemplateHistograms(
        const QVector<double> &hsHistogram,
        const QVector<double> &valueHistogram)
{
    if (!validNormalizedHistogram(hsHistogram, kHsValues)
            || !validNormalizedHistogram(valueHistogram, kBins)) {
        clearTemplate();
        return;
    }
    d->templateHs = hsHistogram;
    d->templateV = valueHistogram;
    d->state->setText(d->detectionAvailable
                      ? tr("显示最新检测特征")
                      : tr("等待测试运行"));
    d->refresh();
}

void ColorComparisonFeatureView::clearTemplate()
{
    d->templateHs.clear();
    d->templateV.clear();
    clearDetection(tr("模型特征不可用"));
    closeZoom();
    d->refresh();
}

bool ColorComparisonFeatureView::setDetectionHistograms(
        const QVector<double> &hsHistogram,
        const QVector<double> &valueHistogram,
        double rawIntersection,
        double score,
        int threshold,
        double smoothedHsScore,
        double brightnessFactor,
        double saturationFactor,
        const QString &brightnessState)
{
    if (!validNormalizedHistogram(hsHistogram, kHsValues)
            || !validNormalizedHistogram(valueHistogram, kBins)
            || !std::isfinite(rawIntersection)
            || !std::isfinite(score)) {
        clearDetection(tr("检测特征不可用：诊断字段无效"));
        return false;
    }
    d->detectionHs = hsHistogram;
    d->detectionV = valueHistogram;
    d->rawIntersection = qBound(0.0, rawIntersection, 1.0);
    d->score = score;
    d->smoothedHsScore = smoothedHsScore;
    d->brightnessFactor = brightnessFactor;
    d->saturationFactor = saturationFactor;
    d->brightnessState = brightnessState;
    d->threshold = threshold;
    d->detectionAvailable = true;
    d->state->setText(tr("显示最新检测特征"));
    d->refresh();
    return true;
}

void ColorComparisonFeatureView::clearDetection(const QString &reason)
{
    d->detectionHs.clear();
    d->detectionV.clear();
    d->rawIntersection = 0.0;
    d->score = 0.0;
    d->smoothedHsScore = -1.0;
    d->brightnessFactor = -1.0;
    d->saturationFactor = -1.0;
    d->brightnessState.clear();
    d->detectionAvailable = false;
    d->state->setText(reason.isEmpty() ? tr("等待测试运行") : reason);
    d->refresh();
}

void ColorComparisonFeatureView::setThreshold(int threshold)
{
    d->threshold = threshold;
    d->refresh();
}

void ColorComparisonFeatureView::closeZoom()
{
    if (d && d->zoom)
        d->zoom->hide();
}

void ColorComparisonFeatureView::syncZoomGeometry()
{
    if (d && d->zoom && d->zoom->parentWidget())
        d->zoom->syncGeometry();
}

bool ColorComparisonFeatureView::zoomVisible() const
{
    return d && d->zoom && d->zoom->isVisible();
}

void ColorComparisonFeatureView::hideEvent(QHideEvent *event)
{
    closeZoom();
    QWidget::hideEvent(event);
}
