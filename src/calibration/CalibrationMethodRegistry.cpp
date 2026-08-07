#include "calibration/CalibrationMethodRegistry.h"
#include "calibration/NPointCalibrationConfigWidget.h"

#include <QJsonArray>

#include <cmath>
#include <utility>

namespace {

class NPointCalibrationMethod final : public ICalibrationMethod
{
public:
    QString methodId() const override { return QStringLiteral("n_point"); }
    QString displayName() const override { return QStringLiteral("N点标定"); }
    QString description() const override
    {
        return QStringLiteral("通过N组图像坐标与物理坐标对应点生成二维标定文件");
    }
    QString iconPath() const override { return QStringLiteral(":/icons/target.svg"); }
    CalibrationMethodAvailability availability() const override
    {
        return CalibrationMethodAvailability::Available;
    }
    CalibrationMethodCapabilities capabilities() const override
    {
        CalibrationMethodCapabilities value;
        value.requiresImage = true;
        value.supportsMultipleFrames = true;
        return value;
    }
    CalibrationMethodConfigWidget *createConfigWidget(QWidget *parent) const override
    {
        return new NPointCalibrationConfigWidget(parent);
    }
    bool validateDraft(const CalibrationDraft &draft, QString *errorMessage) const override
    {
        if (draft.samples.size() < 3) {
            if (errorMessage)
                *errorMessage = QStringLiteral("N点标定至少需要3组有效对应点，正式流程建议9点");
            return false;
        }
        const int expectedCount = draft.parameters.value(
                    QStringLiteral("translationCount")).toInt(draft.samples.size());
        if (draft.samples.size() != expectedCount) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("标定点尚未完成：%1/%2，请采集或手动录入全部点")
                        .arg(draft.samples.size()).arg(expectedCount);
            }
            return false;
        }
        return true;
    }
    CalibrationSolveResult solve(const CalibrationDraft &draft) const override
    {
        CalibrationSolveResult result = CalibrationSolver().solveNPoint(
                    draft.samples,
                    draft.parameters.value(QStringLiteral("rmseLimit")).toDouble(0.10),
                    draft.parameters.value(QStringLiteral("maxErrorLimit")).toDouble(0.25));
        if (result.success) {
            result.model.imageBinding = draft.parameters.value(
                        QStringLiteral("imageBinding")).toObject();
            result.model.methodData.insert(QStringLiteral("translationCount"),
                                           draft.parameters.value(
                                               QStringLiteral("translationCount")).toInt(
                                               draft.samples.size()));
            result.model.methodData.insert(QStringLiteral("rotationCount"),
                                           draft.parameters.value(
                                               QStringLiteral("rotationCount")).toInt(0));
            result.model.methodData.insert(QStringLiteral("rotationSamples"),
                                           draft.parameters.value(
                                               QStringLiteral("rotationSamples")).toArray());
        }
        return result;
    }
    bool validateResult(const CalibrationSolveResult &result,
                        QString *errorMessage) const override
    {
        if (!result.success || !result.model.quality.passed) {
            if (errorMessage)
                *errorMessage = result.message;
            return false;
        }
        const double inverseError = result.model.methodData.value(
                    QStringLiteral("inverseRoundTripMaxPx")).toDouble(qQNaN());
        if (!std::isfinite(inverseError) || inverseError > 1e-6) {
            if (errorMessage)
                *errorMessage = QStringLiteral("正逆矩阵独立一致性验证未通过");
            return false;
        }
        return true;
    }
    CalibrationModel buildCalibrationModel(
            const CalibrationSolveResult &result) const override
    {
        return result.model;
    }
    QJsonObject createResultSummary(
            const CalibrationSolveResult &result) const override
    {
        return result.model.summaryJson();
    }
};

class PlannedCalibrationMethod final : public ICalibrationMethod
{
public:
    PlannedCalibrationMethod(QString id,
                             QString name,
                             QString description,
                             bool rotation,
                             bool board)
        : m_id(std::move(id))
        , m_name(std::move(name))
        , m_description(std::move(description))
        , m_rotation(rotation)
        , m_board(board)
    {
    }

    QString methodId() const override { return m_id; }
    QString displayName() const override { return m_name; }
    QString description() const override { return m_description; }
    QString iconPath() const override { return QStringLiteral(":/icons/target.svg"); }
    CalibrationMethodAvailability availability() const override
    {
        return CalibrationMethodAvailability::Planned;
    }
    CalibrationMethodCapabilities capabilities() const override
    {
        CalibrationMethodCapabilities value;
        value.requiresImage = true;
        value.supportsRotation = m_rotation;
        value.requiresCalibrationBoard = m_board;
        value.supportsMultipleFrames = true;
        return value;
    }
    CalibrationMethodConfigWidget *createConfigWidget(QWidget *) const override { return nullptr; }
    bool validateDraft(const CalibrationDraft &, QString *errorMessage) const override
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("%1尚未实现").arg(m_name);
        return false;
    }
    CalibrationSolveResult solve(const CalibrationDraft &) const override
    {
        CalibrationSolveResult result;
        result.status = QStringLiteral("unsupported");
        result.message = QStringLiteral("%1尚未实现").arg(m_name);
        return result;
    }
    bool validateResult(const CalibrationSolveResult &, QString *errorMessage) const override
    {
        if (errorMessage)
            *errorMessage = QStringLiteral("%1尚未实现").arg(m_name);
        return false;
    }
    CalibrationModel buildCalibrationModel(const CalibrationSolveResult &) const override
    {
        return CalibrationModel();
    }
    QJsonObject createResultSummary(const CalibrationSolveResult &result) const override
    {
        return QJsonObject{{QStringLiteral("status"), result.status},
                           {QStringLiteral("message"), result.message}};
    }

private:
    QString m_id;
    QString m_name;
    QString m_description;
    bool m_rotation = false;
    bool m_board = false;
};

} // namespace

CalibrationMethodRegistry &CalibrationMethodRegistry::instance()
{
    static CalibrationMethodRegistry registry;
    return registry;
}

CalibrationMethodRegistry::CalibrationMethodRegistry()
{
    m_methods.emplace_back(std::make_unique<NPointCalibrationMethod>());
    m_methods.emplace_back(std::make_unique<PlannedCalibrationMethod>(
                         QStringLiteral("translation_rotation"),
                         QStringLiteral("平移旋转标定"),
                         QStringLiteral("通过平移与旋转采样求取转换关系和旋转中心"),
                         true,
                         false));
    m_methods.emplace_back(std::make_unique<PlannedCalibrationMethod>(
                         QStringLiteral("calibration_board"),
                         QStringLiteral("标定板标定"),
                         QStringLiteral("通过标定板建立图像坐标与物理坐标映射"),
                         false,
                         true));
}

QVector<const ICalibrationMethod *> CalibrationMethodRegistry::methods() const
{
    QVector<const ICalibrationMethod *> output;
    output.reserve(m_methods.size());
    for (const std::unique_ptr<ICalibrationMethod> &method : m_methods)
        output.append(method.get());
    return output;
}

const ICalibrationMethod *CalibrationMethodRegistry::method(const QString &methodId) const
{
    for (const std::unique_ptr<ICalibrationMethod> &method : m_methods) {
        if (method->methodId() == methodId)
            return method.get();
    }
    return nullptr;
}

QString calibrationAvailabilityText(CalibrationMethodAvailability availability)
{
    switch (availability) {
    case CalibrationMethodAvailability::Available:
        return QStringLiteral("可用");
    case CalibrationMethodAvailability::Planned:
        return QStringLiteral("后续支持");
    case CalibrationMethodAvailability::Unsupported:
    default:
        return QStringLiteral("不支持");
    }
}
