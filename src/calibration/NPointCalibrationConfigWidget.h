#ifndef CALIBRATION_NPOINTCALIBRATIONCONFIGWIDGET_H
#define CALIBRATION_NPOINTCALIBRATIONCONFIGWIDGET_H

#include "calibration/CalibrationMethodRegistry.h"

#include <QJsonObject>
#include <QMap>
#include <QVector>

class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class QSpinBox;
class QToolButton;

/// N 点标定点表与采样配置页，负责采集/导入数据，不在 UI 层重做 HALCON 求解。
class NPointCalibrationConfigWidget final : public CalibrationMethodConfigWidget
{
public:
    explicit NPointCalibrationConfigWidget(QWidget *parent = nullptr);

    /// 仅收集显式标记为“完成”的点，生成求解草稿及三模式参数。
    CalibrationDraft draft(QString *errorMessage = nullptr) const override;
    /// 刷新模板定位候选及其当前帧输出，并保持已有绑定可回显。
    void setProducerSnapshots(
            const QVector<CalibrationProducerSnapshot> &snapshots) override;
    /// 缓存通信 Capture 的机械坐标，供下一次 captureCurrentSample 合并落表。
    void setLatestPhysicalSample(bool valid,
                                 double x,
                                 double y,
                                 double angleDeg) override;
    /// 校验图像字段是否同源，并返回当前模板定位工具 ID。
    QString captureImageProducerId() const override;
    /// 写入当前模板定位 X/Y/(Angle)；最近机械量有效时一并写入下一个未完成行。
    bool captureCurrentSample(QString *errorMessage = nullptr) override;
    /// 返回已完成平移点在图像 Column/Row 平面中的相邻连线，用于向导叠加预览。
    QVector<QLineF> completedTranslationSegments() const override;
    /// 返回已完成旋转点在图像 Column/Row 平面中的相邻连线，用于向导叠加预览。
    QVector<QLineF> completedRotationSegments() const override;
    /// 返回已完成的平移点数量。
    int completedTranslationSampleCount() const override;
    /// 返回当前模式固定要求的平移点数量。
    int translationSampleCount() const override;

    /// 返回在线采样字段绑定；描述符写入草稿，但求解器不直接消费绑定对象。
    QJsonObject captureBindings() const;
    /// 修改指定采样字段的来源绑定，并同步兼容字段。
    void setCaptureBinding(const QString &fieldKey, const QJsonObject &binding);

    /// 导出可复用的 N 点稳定配置，不包含点坐标与通信运行态。
    QJsonObject persistentSettings() const;
    /// 严格恢复稳定配置；不恢复连接、最近机械量或点表草稿。
    bool restorePersistentSettings(const QJsonObject &settings,
                                   QString *errorMessage = nullptr);

    /// 导出未完成采样状态；完成标记独立保存，合法全零点不会被视为空行。
    QJsonObject draftState() const;
    /// 校验模式、点数、数值与完成标记后恢复未完成采样状态。
    bool restoreDraftState(const QJsonObject &state,
                           QString *errorMessage = nullptr);
    /// 返回当前点表全部已完成行数。
    int completedSampleCount() const;
    /// 返回当前模式固定的点表总行数。
    int sampleCount() const;
    /// 返回当前 N 点模式的稳定字符串 ID。
    QString calibrationMode() const;
    /// 返回当前模式是否要求模板定位提供 ImageAngle。
    bool requiresImageAngle() const;

    /// 显示 HALCON 求解器生成的区域摘要；UI 不重复计算区域几何。
    void setRegionSummary(int validRegionPointCount,
                          int safeRegionPointCount,
                          const QString &message,
                          const QString &state = QStringLiteral("idle"));

private:
    /// 按当前模式固定重建 9 个平移点及可选 3 个旋转点，并全部标为未完成。
    void fillDefaultGrid();
    /// 处理模式切换确认，防止无提示丢弃已采样点。
    void handleCalibrationModeChanged(int index);
    /// 同步模式、固定点数和相关控件；resetPoints 决定是否清空点表。
    void applyCalibrationMode(const QString &mode, bool resetPoints);
    /// 按 9 点/轴轨迹/姿态映射切换角度字段和质量参数的显隐、必填状态。
    void updateCalibrationModeUi();
    /// 检查旋转点的机械角（姿态模式还含图像角）是否至少三点且跨度可观测。
    bool validateCompletedRotationAngles(QString *errorMessage) const;
    /// 返回点表行的稳定类型：translation 或 rotation。
    QString sampleTypeForRow(int row) const;
    /// 严格导入点表，校验列、类型、数量、完成标记和角度可观测性。
    bool importPoints(QTableWidget *editor,
                      int *translationCount,
                      int *rotationCount,
                      QVector<bool> *completed,
                      bool *retryRequested);
    /// 将编辑器点表按带类型和完成状态的 CSV 合同原子导出。
    void exportPoints(const QTableWidget *editor,
                      int translationCount);
    /// 打开点表编辑对话框，并在确认时完成严格导入。
    void editPoints();
    /// 切换基础/全部参数卡片的可见范围。
    void setParameterMode(bool showAll);
    /// 将图像字段绑定到选定的同一模板定位生产者。
    void applyImageProducerBinding(const QString &fieldKey, int producerIndex);
    /// 开关机械字段的 communication 绑定。
    void applyPhysicalCommunicationBinding(const QString &fieldKey, bool enabled);
    /// 从 X/Y/(Angle) 绑定同步旧版单一 imageProducer 字段。
    void syncLegacyImageProducer();
    /// 按手动/触发采样状态刷新按钮与提示。
    void updateCaptureModeUi();
    /// 刷新单个采样字段的来源文本与按钮状态。
    void updateCaptureBindingUi(const QString &fieldKey);
    /// 将自动采样游标移动到第一条未完成记录；全完成时指向末尾。
    void resetCaptureCursorToFirstIncomplete();
    /// 把已完成行解析为 CalibrationSample，并拒绝非有限字段。
    QVector<CalibrationSample> samplesFromTable(QString *errorMessage) const;

    QComboBox *m_imageProducer = nullptr;
    QComboBox *m_calibrationMode = nullptr;
    QSpinBox *m_translationCount = nullptr;
    QSpinBox *m_rotationCount = nullptr;
    QTableWidget *m_sampleTable = nullptr;
    QDoubleSpinBox *m_rmseLimit = nullptr;
    QDoubleSpinBox *m_maxErrorLimit = nullptr;
    QLabel *m_rotationRmseLimitLabel = nullptr;
    QLabel *m_rotationMaxErrorLimitLabel = nullptr;
    QDoubleSpinBox *m_rotationRmseLimit = nullptr;
    QDoubleSpinBox *m_rotationMaxErrorLimit = nullptr;
    QLabel *m_sampleStatus = nullptr;
    QPushButton *m_basicButton = nullptr;
    QPushButton *m_allButton = nullptr;
    QPushButton *m_triggerCaptureButton = nullptr;
    QPushButton *m_manualCaptureButton = nullptr;
    QWidget *m_captureSubscriptionWidget = nullptr;
    QWidget *m_physicalCoordinateCard = nullptr;
    QWidget *m_runtimeParametersCard = nullptr;
    QWidget *m_qualityCard = nullptr;
    QWidget *m_effectiveRegionCard = nullptr;
    QDoubleSpinBox *m_referenceX = nullptr;
    QDoubleSpinBox *m_referenceY = nullptr;
    QDoubleSpinBox *m_offsetX = nullptr;
    QDoubleSpinBox *m_offsetY = nullptr;
    QComboBox *m_movePriority = nullptr;
    QSpinBox *m_directionChangeCount = nullptr;
    QDoubleSpinBox *m_referenceAngle = nullptr;
    QDoubleSpinBox *m_angleOffset = nullptr;
    QSpinBox *m_calibrationOrigin = nullptr;
    QComboBox *m_cameraMotionMode = nullptr;
    QComboBox *m_degreesOfFreedom = nullptr;
    QComboBox *m_weightFunction = nullptr;
    QSpinBox *m_weightCoefficient = nullptr;
    QDoubleSpinBox *m_safeMarginPx = nullptr;
    QLabel *m_validRegionPointCount = nullptr;
    QLabel *m_safeRegionPointCount = nullptr;
    QLabel *m_regionGenerationStatus = nullptr;
    QMap<QString, QLineEdit *> m_captureBindingEdits;
    QMap<QString, QToolButton *> m_captureBindingButtons;
    QMap<QString, QJsonObject> m_captureBindingValues;
    QVector<CalibrationProducerSnapshot> m_snapshots;
    bool m_physicalSampleValid = false;
    double m_physicalX = 0.0;
    double m_physicalY = 0.0;
    double m_physicalAngle = 0.0;
    int m_nextCaptureRow = 0;
    QString m_lastCalibrationMode = QStringLiteral("nine_point_xy");
    QVector<bool> m_sampleCompleted;
    bool m_pointEditorOpen = false;
};

#endif // CALIBRATION_NPOINTCALIBRATIONCONFIGWIDGET_H
