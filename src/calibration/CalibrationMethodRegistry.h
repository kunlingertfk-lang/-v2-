#ifndef CALIBRATION_CALIBRATIONMETHODREGISTRY_H
#define CALIBRATION_CALIBRATIONMETHODREGISTRY_H

#include "calibration/CalibrationSolver.h"

#include <QJsonObject>
#include <QLineF>
#include <QWidget>
#include <QString>
#include <QVector>

#include <memory>
#include <vector>

enum class CalibrationMethodAvailability
{
    Available,
    Planned,
    Unsupported
};

struct CalibrationMethodCapabilities
{
    bool requiresCommunication = false;
    bool requiresImage = true;
    bool supportsRotation = false;
    bool requiresCalibrationBoard = false;
    bool supportsMultipleFrames = false;
};

struct CalibrationDraft
{
    QVector<CalibrationSample> samples;
    QJsonObject parameters;
};

struct CalibrationProducerSnapshot
{
    QString producerId;
    QString displayName;
    bool valid = false;
    QJsonObject payload;
};

class CalibrationMethodConfigWidget : public QWidget
{
    Q_OBJECT
public:
    explicit CalibrationMethodConfigWidget(QWidget *parent = nullptr) : QWidget(parent) {}
    ~CalibrationMethodConfigWidget() override = default;

    /// 汇总当前点表和求解参数，形成与具体标定方式无关的草稿合同。
    virtual CalibrationDraft draft(QString *errorMessage = nullptr) const = 0;
    /// 注入可作为图像坐标来源的前序工具最近快照。
    virtual void setProducerSnapshots(
            const QVector<CalibrationProducerSnapshot> &snapshots) = 0;
    /// 注入通信层最近一次 Capture 报文中的机械 X/Y/Angle。
    virtual void setLatestPhysicalSample(bool valid,
                                         double x,
                                         double y,
                                         double angleDeg) = 0;
    /// 返回触发采样时应运行的图像坐标生产者 ID。
    virtual QString captureImageProducerId() const = 0;
    /// 写入当前图像定位结果；最近机械坐标有效时一并写入下一个未完成点。
    virtual bool captureCurrentSample(QString *errorMessage = nullptr) = 0;
    virtual QVector<QLineF> completedTranslationSegments() const { return {}; }
    virtual QVector<QLineF> completedRotationSegments() const { return {}; }
    virtual int completedTranslationSampleCount() const { return 0; }
    virtual int translationSampleCount() const { return 0; }

signals:
    void sampleStateChanged(const QString &text, bool ok);
    void sampleDataChanged();
};

/// 单种标定方式的配置、草稿校验、求解及结果门禁接口。
class ICalibrationMethod
{
public:
    virtual ~ICalibrationMethod() = default;
    /// 返回持久化使用的稳定方式 ID。
    virtual QString methodId() const = 0;
    /// 返回方式选择页展示名称。
    virtual QString displayName() const = 0;
    /// 返回方式用途说明。
    virtual QString description() const = 0;
    /// 返回方式选择页图标资源路径。
    virtual QString iconPath() const = 0;
    /// 返回当前方式可执行、规划中或不支持状态。
    virtual CalibrationMethodAvailability availability() const = 0;
    /// 返回方式对图像、通信、旋转与多帧的能力声明。
    virtual CalibrationMethodCapabilities capabilities() const = 0;
    /// 创建该方式专用配置页；不可用方式返回 nullptr。
    virtual CalibrationMethodConfigWidget *createConfigWidget(QWidget *parent) const = 0;
    /// 在求解前严格校验点数、模式与必需参数。
    virtual bool validateDraft(const CalibrationDraft &draft,
                               QString *errorMessage) const = 0;
    /// 调用该方式的算法编排并返回强类型模型与诊断状态。
    virtual CalibrationSolveResult solve(const CalibrationDraft &draft) const = 0;
    /// 执行求解后的质量与独立一致性门禁。
    virtual bool validateResult(const CalibrationSolveResult &result,
                                QString *errorMessage) const = 0;
    /// 从已通过门禁的求解结果提取可持久化模型。
    virtual CalibrationModel buildCalibrationModel(
            const CalibrationSolveResult &result) const = 0;
    /// 生成供 UI 展示的精简结果摘要。
    virtual QJsonObject createResultSummary(
            const CalibrationSolveResult &result) const = 0;
};

/// 标定方式单例注册表；当前 N 点可执行，其余方式以明确 Planned 状态占位。
class CalibrationMethodRegistry
{
public:
    /// 返回进程内唯一注册表实例。
    static CalibrationMethodRegistry &instance();

    /// 返回所有已注册方式，供向导生成方式选择页。
    QVector<const ICalibrationMethod *> methods() const;
    /// 按稳定 methodId 查找方式；不存在时返回 nullptr。
    const ICalibrationMethod *method(const QString &methodId) const;

private:
    CalibrationMethodRegistry();
    std::vector<std::unique_ptr<ICalibrationMethod>> m_methods;
};

QString calibrationAvailabilityText(CalibrationMethodAvailability availability);

#endif // CALIBRATION_CALIBRATIONMETHODREGISTRY_H
