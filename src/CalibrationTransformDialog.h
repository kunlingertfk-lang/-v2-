#ifndef CALIBRATIONTRANSFORMDIALOG_H
#define CALIBRATIONTRANSFORMDIALOG_H

#include "toolcore/ToolConfig.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolPreviewSnapshot.h"

#include <QDialog>
#include <QMap>
#include <QVector>

#include <opencv2/core/mat.hpp>

#include <array>

namespace Ui { class CalibrationTransformDialog; }

class FrameViewHelper;
class QComboBox;
class QDoubleSpinBox;
class QHBoxLayout;
class QImage;
class QLineEdit;
class QToolButton;
class ToolEngine;

/// 标定转换配置与测试窗口：保存同帧订阅、标定文件和可选机构位姿，并展示转换门禁。
class CalibrationTransformDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CalibrationTransformDialog(QWidget *parent = nullptr);
    ~CalibrationTransformDialog() override;

    /// 恢复 calibrationTransform 配置、文件列表、绑定和位姿字段。
    void loadFromConfig(const ToolConfig &config);
    /// 汇总当前 UI 为版本 4 的 CalibrationTransform ToolConfig。
    ToolConfig toolConfig() const;
    /// 返回最近一次基准图测试快照；PC 导入图测试不会污染该快照。
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    /// 注入当前工具之前的合法坐标生产者及其预览结果，建立同源 X/Y/Angle 选项。
    void setProducerTools(const QVector<ToolConfig> &tools,
                          int consumerIndex,
                          const QMap<QString, ToolPreviewSnapshot> &snapshots = {});
    /// 注入完整前序工具链和共享引擎，供配置页执行真实同帧测试。
    void setToolChainTestContext(
            const QVector<ToolConfig> &tools,
            int consumerIndex,
            ToolEngine *sharedToolEngine,
            const ReferencePositionCorrectionConfig &referencePositionCorrection);
    /// 读取 PC 图片并进入临时测试模式；只改变测试帧，不保存到工具配置。
    bool loadTestImageFromFile(const QString &filePath,
                               QString *errorMessage = nullptr);

private slots:
    /// 导入并验证一个标定文件，将其加入工具候选列表。
    void importCalibrationFile();
    /// 选择 PC 图片、进入临时模式并立即执行一次完整工具链测试。
    void importTestImageFromPc();
    /// 清理 PC 测试帧并恢复基准图预览与来源状态。
    void exitImportedTestMode();
    /// 运行“前序工具链 -> 标定转换”，展示统一 ToolResult 和门禁信息。
    void runTest();
    /// 保存前执行文件、订阅和来源失效校验；测试模式下改为运行一次。
    void finishConfiguration();

private:
    enum class SourceValidationState
    {
        NotReady,
        Verified,
        Unverifiable,
        Stale
    };

    struct SourceValidationResult
    {
        SourceValidationState state = SourceValidationState::NotReady;
        QString message;
    };

    struct InputProducerContract
    {
        QString producerId;
        QString displayName;
        QString xKey;
        QString yKey;
        QString angleKey;
    };

    QJsonObject bindingFor(QComboBox *combo, double constantValue) const;
    QJsonObject mainInputBinding(QComboBox *combo) const;
    void restoreBinding(QComboBox *combo, const QJsonObject &binding);
    void restoreMainInputBindings(const QJsonObject &x,
                                  const QJsonObject &y,
                                  const QJsonObject &angle);
    QToolButton *createBindingButton(QComboBox *stateCombo,
                                     QDoubleSpinBox *valueSpin,
                                     QWidget *parent,
                                     const QString &fieldName);
    QToolButton *createInputBindingButton(QWidget *parent,
                                          const QString &fieldName);
    void updateBindingButton(QComboBox *stateCombo,
                             QDoubleSpinBox *valueSpin,
                             QToolButton *button,
                             const QString &fieldName);
    void applyInputProducer(int producerIndex);
    void updateMainInputUi();
    void updateInputAngleRequirement(bool required);
    void invalidatePreviewSnapshot();
    /// 比对 XML 中来源指纹与当前模板/位置修正链，区分已验证、不可验证和失效。
    SourceValidationResult evaluateCalibrationSource() const;
    void refreshCalibrationSourceValidation();
    void showSourceValidationStatus(const SourceValidationResult &validation);
    /// 校验文件可加载、X/Y/(Angle) 同源订阅完整且来源没有失效。
    bool validateConfiguration(QString *errorMessage) const;
    QJsonObject poseConfig(bool calibration) const;
    void setupPoseSourceUi();
    void updateReferenceImage(const QImage &image);
    void displayReferenceImage(const QImage &image);
    void displayImportedTestImage();
    void clearDisplayedConversionResult();
    void updateImportedTestUi();
    void refreshFileList(const QStringList &paths, const QString &activePath);
    /// 加载当前模型并展示模式、ValidROI/SafeROI、轴轨迹和角度映射状态。
    void refreshCalibrationRegionSummary();
    /// 合并当前方案 calibrations 目录内的 XML/IWCAL 资产，不改变活动文件。
    void mergeSchemeCalibrationFiles();
    /// 将坐标、区域、旋转覆盖和生产放行状态呈现在结果叠加层。
    void displayConversionResult(const ToolResult &result);

    Ui::CalibrationTransformDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    ToolConfig m_initialConfig;
    ToolPreviewSnapshot m_snapshot;
    QStringList m_calibrationFiles;
    QVector<InputProducerContract> m_inputProducers;
    QMap<QString, ToolConfig> m_producerConfigs;
    QMap<QString, ToolPreviewSnapshot> m_producerSnapshots;
    SourceValidationResult m_sourceValidation;
    bool m_mainInputSourceAvailable = false;
    bool m_inputAngleRequired = false;
    cv::Mat m_importedTestFrame;
    QString m_importedTestImageTitle;
    bool m_importedTestActive = false;
    QVector<ToolConfig> m_testToolPrefix;
    ToolEngine *m_sharedToolEngine = nullptr;
    ReferencePositionCorrectionConfig m_referencePositionCorrection;
    QToolButton *m_inputXLinkButton = nullptr;
    QToolButton *m_inputYLinkButton = nullptr;
    QToolButton *m_inputAngleLinkButton = nullptr;
    std::array<QComboBox *, 4> m_calibrationPoseSources{{nullptr, nullptr, nullptr, nullptr}};
    std::array<QComboBox *, 4> m_runPoseSources{{nullptr, nullptr, nullptr, nullptr}};
    std::array<QToolButton *, 4> m_calibrationPoseLinkButtons{{nullptr, nullptr, nullptr, nullptr}};
    std::array<QToolButton *, 4> m_runPoseLinkButtons{{nullptr, nullptr, nullptr, nullptr}};
};

#endif // CALIBRATIONTRANSFORMDIALOG_H
