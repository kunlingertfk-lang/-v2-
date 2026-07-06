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
class QToolButton;

struct ColorRecognitionLabelData
{
    // 模板类别名及其稳定 classId。
    QString name;
    int classId = 0;
};

struct ColorRecognitionSampleData
{
    // 单个样本保存标签、特征、ROI 和样本缩略图，用于后续识别匹配。
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
    // 一个颜色模板的完整配置：算法参数、类别集合和样本集合。
    QString templateId;
    QString name = QStringLiteral("颜色模板");
    QString featureType = QStringLiteral("histogram");
    QString sensitivity = QStringLiteral("medium");
    bool brightnessEnabled = false;
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

    // 返回模板编辑器当前编辑完成的数据。
    ColorRecognitionTemplateData templateData() const;
    // 用已有模板数据回显模板编辑器。
    void setTemplateData(const ColorRecognitionTemplateData &data);
    // 从主对话框传入初始样本 ROI，便于新建模板时沿用检测区域。
    void setInitialSampleRoi(const QRectF &roi);

protected:
    // 尺寸变化后重新适配预览图。
    void resizeEvent(QResizeEvent *event) override;
    // 捕获无边框标题栏拖拽和预览控件事件。
    bool eventFilter(QObject *watched, QEvent *event) override;
    // 处理窗口拖拽起点。
    void mousePressEvent(QMouseEvent *event) override;
    // 处理窗口拖拽过程。
    void mouseMoveEvent(QMouseEvent *event) override;
    // 结束窗口拖拽。
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void adjustInitialGeometry();                // 初次显示时按屏幕大小调整模板窗口尺寸和位置。
    void buildUi();                              // 代码构建模板编辑器界面控件。
    void setupUiState();                         // 初始化控件默认状态和预览 helper。
    void connectControls();                      // 连接标签、样本、ROI 和保存按钮的信号。
    void addLabel();                             // 新增颜色类别标签。
    void renameCurrentLabel();                   // 重命名当前类别标签并同步样本标签名。
    void deleteCurrentLabel();                   // 删除当前类别及其关联样本。
    void addSampleFromCurrentRoi();              // 从当前图像 ROI 提取特征并加入样本列表。
    void addCurrentImage();                      // 使用主界面当前图像作为样本来源。
    void addImageFromPc();                       // 从本地文件选择样本图片。
    void deleteCurrentRoiSample();               // 删除当前选中的 ROI 样本。
    void startRectangleRoiEditing();             // 启动样本矩形 ROI 绘制。
    void showUnsupportedRegionMessage();         // 提示当前模板采样暂不支持的 ROI 模式。
    void handleRoiChanged(const QRectF &roi);    // 保存样本 ROI 编辑结果。
    void handleRoiSelectionRejected();           // 处理样本 ROI 绘制取消。
    void showPreviewImage();                     // 显示样本来源图并刷新 ROI overlay。
    void refreshDisplayedRoiOverlay();           // 重绘样本采集 ROI overlay。
    void fitPreview();                           // 适配预览画布尺寸。
    void updateLabelList();                      // 刷新类别标签列表。
    void updateRoiSampleList();                  // 刷新 ROI 样本列表和缩略图。
    QImage currentDisplayImageForSamples() const; // 返回当前可用于样本展示/裁剪的图像。
    QImage cropRoiImage(const QRectF &roiNormalized) const; // 裁剪 ROI 缩略图。
    QImage roiThumbnailForSample(const ColorRecognitionSampleData &sample) const; // 生成样本列表缩略图。
    void updateSampleCount();                    // 刷新样本数量显示。
    void ensureDefaultLabel();                   // 确保至少存在一个默认类别。
    int nextClassId() const;                     // 生成下一个未使用 classId。
    int currentClassId() const;                  // 返回当前类别 classId。
    int currentSampleIndex() const;              // 返回当前样本索引。
    QString currentLabelName() const;            // 返回当前类别名称。
    QRectF effectiveRoiNormalized() const;       // 返回当前样本采集 ROI。
    ColorRecognitionHalconConfig featureExtractionConfig() const; // 构造样本特征提取用 runner 配置。
    void setStatusText(const QString &displayText, const QString &tooltipText = QString()); // 更新模板窗口状态栏。

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
