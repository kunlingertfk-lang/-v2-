#ifndef COLORTEMPLATEDIALOG_H
#define COLORTEMPLATEDIALOG_H

#include <QDialog>
#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <QVector>

#include "algorithms/recognition/ColorRecognitionHalconRunner.h"

class FrameViewHelper;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QResizeEvent;
class QMouseEvent;
class QSpinBox;
class QToolButton;

struct ColorRecognitionLabelData
{
    QString name;
    int classId = 0;
};

struct ColorRecognitionSampleData
{
    QString label;
    int classId = 0;
    QVector<double> feature;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString roiImagePngBase64;
    int roiImageWidth = 0;
    int roiImageHeight = 0;
};

struct ColorRecognitionTemplateData
{
    QString templateId;
    QString name = QStringLiteral("颜色模板");
    QString featureType = QStringLiteral("histogram");
    QString sensitivity = QStringLiteral("medium");
    bool brightnessEnabled = true;
    int knnK = 3;
    QString knnDistance = QStringLiteral("halcon_default");
    QVector<ColorRecognitionLabelData> labels;
    QVector<ColorRecognitionSampleData> samples;
};

class ColorTemplateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColorTemplateDialog(QWidget *parent = nullptr);

    ColorRecognitionTemplateData templateData() const;
    void setTemplateData(const ColorRecognitionTemplateData &data);
    void setInitialSampleRoi(const QRectF &roi);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void adjustInitialGeometry();
    void buildUi();
    void setupUiState();
    void connectControls();
    void addLabel();
    void renameCurrentLabel();
    void deleteCurrentLabel();
    void addSampleFromCurrentRoi();
    void addCurrentImage();
    void addImageFromPc();
    void deleteCurrentRoiSample();
    void startRectangleRoiEditing();
    void showUnsupportedRegionMessage();
    void handleRoiChanged(const QRectF &roi);
    void handleRoiSelectionRejected();
    void showPreviewImage();
    void refreshDisplayedRoiOverlay();
    void fitPreview();
    void updateLabelList();
    void updateRoiSampleList();
    QImage currentDisplayImageForSamples() const;
    QImage cropRoiImage(const QRectF &roiNormalized) const;
    QImage roiThumbnailForSample(const ColorRecognitionSampleData &sample) const;
    void updateSampleCount();
    void ensureDefaultLabel();
    int nextClassId() const;
    int currentClassId() const;
    int currentSampleIndex() const;
    QString currentLabelName() const;
    QRectF effectiveRoiNormalized() const;
    ColorRecognitionHalconConfig featureExtractionConfig() const;
    void setStatusText(const QString &displayText, const QString &tooltipText = QString());

    QButtonGroup *m_regionGroup = nullptr;
    FrameViewHelper *m_previewHelper = nullptr;
    ColorRecognitionHalconRunner m_featureRunner;
    ColorRecognitionTemplateData m_template;
    QRectF m_sampleRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    cv::Mat m_sampleImage;
    QImage m_sampleDisplayImage;
    QString m_sampleImageTitle;
    bool m_draggingWindow = false;
    QPoint m_dragStartGlobalPos;
    QPoint m_dragStartFramePos;

    QLineEdit *m_templateNameLineEdit = nullptr;
    QListWidget *m_labelListWidget = nullptr;
    QPushButton *m_addLabelButton = nullptr;
    QPushButton *m_renameLabelButton = nullptr;
    QPushButton *m_deleteLabelButton = nullptr;
    QListWidget *m_roiSampleListWidget = nullptr;
    QLabel *m_sampleCountLabel = nullptr;
    QComboBox *m_featureTypeComboBox = nullptr;
    QComboBox *m_sensitivityComboBox = nullptr;
    QCheckBox *m_brightnessCheckBox = nullptr;
    QSpinBox *m_knnKSpinBox = nullptr;
    QComboBox *m_knnDistanceComboBox = nullptr;
    QToolButton *m_regionRectButton = nullptr;
    QPushButton *m_addCurrentImageButton = nullptr;
    QPushButton *m_addImageButton = nullptr;
    QPushButton *m_deleteCurrentRoiButton = nullptr;
    QPushButton *m_addSampleButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QLabel *m_viewerTitleLabel = nullptr;
    QGraphicsView *m_previewGraphicsView = nullptr;
    QLabel *m_statusLabel = nullptr;
};

#endif // COLORTEMPLATEDIALOG_H
