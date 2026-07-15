#ifndef COLORCOMPARISONFEATUREVIEW_H
#define COLORCOMPARISONFEATUREVIEW_H

#include <QScopedPointer>
#include <QVector>
#include <QWidget>

class ColorComparisonFeatureView : public QWidget
{
public:
    explicit ColorComparisonFeatureView(QWidget *parent = nullptr);
    ~ColorComparisonFeatureView() override;

    void setTemplateHistograms(const QVector<double> &hsHistogram,
                               const QVector<double> &valueHistogram);
    void clearTemplate();
    bool setDetectionHistograms(const QVector<double> &hsHistogram,
                                const QVector<double> &valueHistogram,
                                double rawIntersection,
                                double score,
                                int threshold);
    void clearDetection(const QString &reason = QString());
    void setThreshold(int threshold);
    void closeZoom();
    void syncZoomGeometry();
    bool zoomVisible() const;

    static bool validNormalizedHistogram(const QVector<double> &values,
                                         int expectedSize);
    static void hsMarginals(const QVector<double> &hsHistogram,
                            QVector<double> *hue,
                            QVector<double> *saturation);

protected:
    void hideEvent(QHideEvent *event) override;

private:
    class Impl;
    QScopedPointer<Impl> d;
};

#endif // COLORCOMPARISONFEATUREVIEW_H
