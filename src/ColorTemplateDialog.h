#ifndef COLORTEMPLATEDIALOG_H
#define COLORTEMPLATEDIALOG_H

#include <QDialog>
#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QString>
#include <QVector>

#include "algorithms/recognition/ColorRecognitionHalconRunner.h"
#include "frame/FrameInputMetadata.h"

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
class QWidget;
namespace Ui { class ColorTemplateDialog; }

struct ColorRecognitionLabelData
{
    // 模板类别名及其稳定 classId。
    QString name;
    int classId = 0;
};

struct ColorRecognitionSampleData
{
    // 单个样本保存标签、特征、ROI 和样本缩略图，用于后续识别匹配。
    QString sampleId;
    QString label;
    int classId = 0;
    QVector<double> feature;
    QString featureSignature;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    QString roiImagePngBase64;
    int roiImageWidth = 0;
    int roiImageHeight = 0;
    QString gmmRoiImagePngBase64;
    QString gmmImageSha256;
    int gmmImageWidth = 0;
    int gmmImageHeight = 0;
    QString pixelFormat;
    int validBits = -1;
    int bitShift = -1;
};

struct ColorRecognitionGmmModelData
{
    QString state = QStringLiteral("empty");
    QString status;
    QString message;
    QString algorithmVersion;
    QString featureSchemaVersion;
    QString samplingAlgorithmVersion;
    QString colorChannels = QStringLiteral("ab");
    QString trainingDataHash;
    QString buildParamsHash;
    QString serializedGmmBase64;
    qint64 serializedSize = 0;
    QString serializedSha256;
    QVector<int> classIdOrder;
    QVector<ColorRecognitionGmmClassDiagnostics> classes;
    QString builtAtUtc;
};

struct ColorRecognitionTemplateData
{
    // 一个颜色模板的完整配置：算法参数、类别集合和样本集合。
    QString templateId;
    QString name = QStringLiteral("颜色模板");
    int modelSchemaVersion = 3;
    QString recognitionBackend = QStringLiteral("hsv_histogram");
    QString modelState = QStringLiteral("ready");
    QString algorithmVersion = QStringLiteral("halcon_hs_histogram_transition_v2");
    QString featureSchemaVersion;
    QString featureType = QStringLiteral("histogram_2dim_hs");
    QString sensitivity = QStringLiteral("medium");
    bool brightnessEnabled = false;
    QString hsvClassifierVersion = colorRecognitionHsvCurrentClassifierVersion();
    QString hsvClassifierParamsHash = colorRecognitionHsvClassifierParamsHash(hsvClassifierVersion);
    QString gmmColorChannels = QStringLiteral("ab");
    int gmmMaxSamplesPerClass = 10000;
    double gmmRejectionThreshold = kColorRecognitionGmmDefaultRejectionThreshold;
    ColorRecognitionGmmModelData gmmModel;
    QVector<ColorRecognitionLabelData> labels;
    QVector<ColorRecognitionSampleData> samples;
};

// 按 B1 的规范顺序计算公共 GMM 训练事实哈希，供保存/加载 stale 校验和 smoke 共用。
QString colorRecognitionGmmTrainingDataHash(const ColorRecognitionTemplateData &colorTemplate);

class ColorTemplateDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColorTemplateDialog(QWidget *parent = nullptr);
    ~ColorTemplateDialog() override;

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
    enum class EditState {
        None,
        SampleRect
    };

    void adjustInitialGeometry();                // 初次显示时按屏幕大小调整模板窗口尺寸和位置。
    void buildUi();                              // 代码构建模板编辑器界面控件。
    void setupUiState();                         // 初始化控件默认状态和预览 helper。
    void connectControls();                      // 连接标签、样本、ROI 和保存按钮的信号。
    void updateAlgorithmUi();                    // 按后端和特征类型刷新参数、亮度和 GMM 模型控件。
    void updateHsvModelUi();                     // 刷新 HSV 派生特征状态和批量重建按钮。
    void updateGmmModelUi();                     // 刷新 GMM 状态、诊断和建模按钮。
    void setHsvRebuildUiBusy(bool busy);          // HSV 批量重建期间统一锁定/恢复模板控件。
    void setGmmBuildUiBusy(bool busy);           // 建模期间统一锁定/恢复模板编辑控件。
    void showGmmBuildFailure(const QString &status, const QString &message); // 统一显示建模失败状态与弹窗。
    void markHsvModelStale();                    // HSV 参数变化后只使 HSV 派生特征失效。
    void markGmmModelStale();                    // GMM 参数变化后只使 GMM 模型失效。
    void markCommonModelsStale();                // 公共标签/样本变化后同时使两套模型失效。
    void finishTemplate();                       // 保存前提示不可用的 GMM 模型状态。
    void rebuildHsvFeatures();                   // 从保存的无损 ROI 事务式重建全部 HSV 特征。
    void buildGmmModel();                        // 从已保存的无损 ROI 调用 HALCON GMM 建模。
    void addLabel();                             // 新增颜色类别标签。
    void renameCurrentLabel();                   // 重命名当前类别标签并同步样本标签名。
    void deleteCurrentLabel();                   // 删除当前类别及其关联样本。
    void addSampleFromCurrentRoi();              // 从当前图像 ROI 提取特征并加入样本列表。
    void addCurrentImage();                      // 使用主界面当前图像作为样本来源。
    void addImageFromPc();                       // 从本地文件选择样本图片。
    void deleteCurrentRoiSample();               // 删除当前选中的 ROI 样本。
    void startRectangleRoiEditing();             // 启动样本矩形 ROI 绘制。
    void setEditState(EditState state);           // 切换样本 ROI 活动绘制态，不修改已保存 ROI。
    void showUnsupportedRegionMessage();         // 提示当前模板采样暂不支持的 ROI 模式。
    void handleRoiChanged(const QRectF &roi);    // 保存样本 ROI 编辑结果。
    void handleRoiSelectionRejected();           // 处理样本 ROI 绘制取消。
    void showPreviewImage();                     // 显示样本来源图并刷新 ROI overlay。
    void refreshDisplayedRoiOverlay();           // 重绘样本采集 ROI overlay。
    void fitPreview();                           // 适配预览画布尺寸。
    void updateLabelList();                      // 刷新类别标签列表。
    void updateRoiSampleList();                  // 刷新 ROI 样本列表和缩略图。
    bool restoreSampleContext(int sampleIndex, QString *errorMessage = nullptr); // 校验保存的无损 ROI 并恢复坐标，不在主视图显示 ROI 裁剪图。
    QImage currentDisplayImageForSamples() const; // 返回当前可用于样本展示/裁剪的图像。
    QImage cropRoiImage(const QRectF &roiNormalized) const; // 裁剪 ROI 缩略图。
    cv::Mat cropGmmRoiMat(const cv::Mat &frame, const QRectF &roiNormalized) const; // 从原始 Mat 无损裁剪训练 ROI。
    QImage roiThumbnailForSample(const ColorRecognitionSampleData &sample) const; // 生成样本列表缩略图。
    void updateSampleCount();                    // 刷新样本数量显示。
    void ensureDefaultLabel();                   // 确保至少存在一个默认类别。
    int nextClassId() const;                     // 生成下一个未使用 classId。
    int currentClassId() const;                  // 返回当前类别 classId。
    int currentSampleIndex() const;              // 返回当前样本索引。
    QString currentLabelName() const;            // 返回当前类别名称。
    QRectF effectiveRoiNormalized() const;       // 返回当前样本采集 ROI。
    ColorRecognitionHalconConfig featureExtractionConfig(
            const FrameInputMetadata &metadata) const; // 构造样本特征提取用 runner 配置。
    void setStatusText(const QString &displayText, const QString &tooltipText = QString()); // 更新模板窗口状态栏。

    QButtonGroup *m_regionGroup = nullptr;
    FrameViewHelper *m_previewHelper = nullptr;
    ColorRecognitionHalconRunner m_featureRunner;
    ColorRecognitionTemplateData m_template;
    QRectF m_sampleRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    cv::Mat m_sampleImage;
    FrameInputMetadata m_sampleImageMetadata;
    QImage m_sampleDisplayImage;
    QString m_sampleImageTitle;
    bool m_draggingWindow = false;
    QPoint m_dragStartGlobalPos;
    QPoint m_dragStartFramePos;

    QLineEdit *m_templateNameLineEdit = nullptr;
    QComboBox *m_recognitionBackendComboBox = nullptr;
    QWidget *m_featureTypeRowWidget = nullptr;
    QListWidget *m_labelListWidget = nullptr;
    QPushButton *m_addLabelButton = nullptr;
    QPushButton *m_renameLabelButton = nullptr;
    QPushButton *m_deleteLabelButton = nullptr;
    QListWidget *m_roiSampleListWidget = nullptr;
    QLabel *m_sampleCountLabel = nullptr;
    QComboBox *m_featureTypeComboBox = nullptr;
    QComboBox *m_sensitivityComboBox = nullptr;
    QCheckBox *m_brightnessCheckBox = nullptr;
    QLabel *m_gmmModelStateLabel = nullptr;
    QLabel *m_gmmDiagnosticsLabel = nullptr;
    QPushButton *m_buildGmmButton = nullptr;
    QLabel *m_gmmBuildFeedbackLabel = nullptr;
    QLabel *m_hsvModelStateLabel = nullptr;
    QPushButton *m_rebuildHsvButton = nullptr;
    QLabel *m_hsvBuildFeedbackLabel = nullptr;
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
    bool m_hsvRebuildInProgress = false;
    bool m_gmmBuildInProgress = false;
    EditState m_editState = EditState::None;
    Ui::ColorTemplateDialog *ui = nullptr;
};

#endif // COLORTEMPLATEDIALOG_H
