#include "calibration/CalibrationMethodRegistry.h"
#include "calibration/NPointCalibrationConfigWidget.h"

#include <QJsonArray>

#include <cmath>
#include <utility>

namespace {

// 严格读取旋转样本 JSON 中的有限数值字段。
bool finiteJsonNumber(const QJsonValue &value, double *output)
{
    if (!output || !value.isDouble())
        return false;
    const double parsed = value.toDouble();
    if (!std::isfinite(parsed))
        return false;
    *output = parsed;
    return true;
}

// 将三条旋转样本 JSON 解码为强类型数据，并按模式检查 ImageAngle。
bool parseRotationSamples(const QJsonArray &array,
                          bool requireImageAngle,
                          int firstIndex,
                          QVector<CalibrationSample> *samples,
                          QString *errorMessage)
{
    if (!samples)
        return false;
    samples->clear();
    samples->reserve(array.size());
    for (int index = 0; index < array.size(); ++index) {
        if (!array.at(index).isObject()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("第%1个旋转样本不是对象").arg(index + 1);
            return false;
        }
        const QJsonObject object = array.at(index).toObject();
        CalibrationSample sample;
        sample.index = object.value(QStringLiteral("index"))
                .toInt(firstIndex + index);
        if (!finiteJsonNumber(object.value(QStringLiteral("column")), &sample.column)
                || !finiteJsonNumber(object.value(QStringLiteral("row")), &sample.row)
                || !finiteJsonNumber(object.value(QStringLiteral("machineX")),
                                     &sample.machineX)
                || !finiteJsonNumber(object.value(QStringLiteral("machineY")),
                                     &sample.machineY)
                || !finiteJsonNumber(object.value(QStringLiteral("machineAngleDeg")),
                                     &sample.machineAngleDeg)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("第%1个旋转样本包含缺失或非有限字段")
                        .arg(index + 1);
            return false;
        }
        const QJsonValue imageAngleValue = object.value(
                    QStringLiteral("imageAngleDeg"));
        if (imageAngleValue.isUndefined() && !requireImageAngle) {
            sample.imageAngleDeg = 0.0;
        } else if (!finiteJsonNumber(imageAngleValue,
                                     &sample.imageAngleDeg)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "第%1个旋转样本缺少有效图像角度")
                        .arg(index + 1);
            }
            return false;
        }
        if (sample.index != firstIndex + index) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "旋转样本序号必须从%1开始连续排列")
                        .arg(firstIndex);
            }
            return false;
        }
        sample.source = object.value(QStringLiteral("source"))
                .toString(QStringLiteral("manual"));
        sample.capturedAt = object.value(QStringLiteral("capturedAt")).toString();
        samples->append(sample);
    }
    return true;
}

// 从草稿参数读取 N 点模式；未知模式立即拒绝，禁止按默认模式求解。
bool draftMode(const QJsonObject &parameters,
               NPointCalibrationMode *mode,
               QString *errorMessage)
{
    const QString modeText = parameters.value(
                QStringLiteral("calibrationMode")).toString();
    if (!nPointCalibrationModeFromString(modeText, mode)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "请选择有效标定模式：9点XY、12点轴轨迹或12点位置与姿态");
        }
        return false;
    }
    return true;
}

// 将轴轨迹验证码翻译为可直接指导现场复采的失败说明。
QString rotationFailureMessage(const CalibrationRotationRange &range)
{
    const QString details = QStringLiteral(
                "（机械角跨度 %1°，偏心半径 %2 mm，"
                "位置RMSE %3 mm/门限 %4 mm，"
                "最大误差 %5 mm/门限 %6 mm）")
            .arg(range.machineMaxDeg - range.machineMinDeg, 0, 'f', 3)
            .arg(range.radiusMm, 0, 'f', 6)
            .arg(range.fitRmseMm, 0, 'f', 6)
            .arg(range.rmseLimitMm, 0, 'f', 6)
            .arg(range.maxErrorMm, 0, 'f', 6)
            .arg(range.maxErrorLimitMm, 0, 'f', 6);
    if (range.validationCode == QStringLiteral("machine_span_too_small")) {
        return QStringLiteral("机械旋转角度跨度不足，请扩大旋转采样范围%1")
                .arg(details);
    }
    if (range.validationCode
            == QStringLiteral("insufficient_independent_machine_angles")) {
        return QStringLiteral(
                    "至少需要3个不同的机械旋转姿态；0°与360°属于同一姿态%1")
                .arg(details);
    }
    if (range.validationCode == QStringLiteral("direction_ambiguous")) {
        return QStringLiteral(
                    "当前旋转样本无法区分机械角正向与反向，请在不同角度补采样本并避免仅使用0°/180°/360°%1")
                .arg(details);
    }
    if (range.validationCode == QStringLiteral("coaxial_bias_exceeded")) {
        return QStringLiteral(
                    "标定点旋转轨迹接近共轴，但转换坐标与轴原点偏差超过质量门限%1")
                .arg(details);
    }
    if (range.validationCode == QStringLiteral("trajectory_span_invalid")
            || range.validationCode == QStringLiteral("trajectory_circle_invalid")
            || range.validationCode == QStringLiteral("halcon_trajectory_fit_failed")) {
        return QStringLiteral(
                    "标定点中心未形成可稳定拟合的旋转圆弧，请检查每个角度是否重新定位以及标定采样的机械X/Y/A是否对应同一帧%1")
                .arg(details);
    }
    if (range.validationCode == QStringLiteral("position_fit_error_exceeded")) {
        return QStringLiteral(
                    "旋转圆弧与机械角度对应关系的位置误差超过门限，请检查采样顺序、轴角正方向和机械坐标字段%1")
                .arg(details);
    }
    return QStringLiteral("旋转轴偏心位置模型求解失败（%1）%2")
            .arg(range.validationCode, details);
}

// 将姿态角映射验证码翻译为可直接指导现场复采的失败说明。
QString angleMappingFailureMessage(const CalibrationAngleMapping &mapping)
{
    const QString details = QStringLiteral(
                "（图像角跨度 %1°，机械角跨度 %2°，RMSE %3°，最大误差 %4°）")
            .arg(mapping.imageMaxDeg - mapping.imageMinDeg, 0, 'f', 3)
            .arg(mapping.machineMaxDeg - mapping.machineMinDeg, 0, 'f', 3)
            .arg(mapping.rmseDeg, 0, 'f', 6)
            .arg(mapping.maxErrorDeg, 0, 'f', 6);
    if (mapping.validationCode == QStringLiteral("image_span_too_small")) {
        return QStringLiteral(
                    "图像角度变化不足，无法建立姿态映射；请使用方向可观测的非对称模板%1")
                .arg(details);
    }
    if (mapping.validationCode == QStringLiteral("machine_span_too_small")) {
        return QStringLiteral("机械角度变化不足，请扩大旋转采样范围%1")
                .arg(details);
    }
    if (mapping.validationCode
            == QStringLiteral("insufficient_independent_angles")) {
        return QStringLiteral(
                    "姿态映射要求3个独立的图像角度和机械角度样本%1")
                .arg(details);
    }
    if (mapping.validationCode == QStringLiteral("fit_error_exceeded")) {
        return QStringLiteral(
                    "图像角度到机械角度的拟合误差超过门限，请检查角度定义和采样同步%1")
                .arg(details);
    }
    return QStringLiteral("图像角度到机械角度映射求解失败（%1）%2")
            .arg(mapping.validationCode, details);
}

// N 点方式编排层：连接点表草稿、HALCON XY 求解、轴轨迹/角度映射与结果门禁。
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
        NPointCalibrationMode mode;
        if (!draftMode(draft.parameters, &mode, errorMessage))
            return false;

        const int expectedRotationCount =
                mode == NPointCalibrationMode::NinePointXY ? 0 : 3;
        const int expectedCount = draft.parameters.value(
                    QStringLiteral("translationCount")).toInt(-1);
        const int rotationCount = draft.parameters.value(
                    QStringLiteral("rotationCount")).toInt(-1);
        const int declaredTotal = draft.parameters.value(
                    QStringLiteral("totalSampleCount")).toInt(-1);
        if (expectedCount != 9 || rotationCount != expectedRotationCount
                || declaredTotal != 9 + expectedRotationCount) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "当前标定模式要求严格使用9平移+%1旋转点，配置计数不一致")
                        .arg(expectedRotationCount);
            }
            return false;
        }
        if (draft.samples.size() != 9) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "平移标定点尚未完成：%1/9，请采集或手动录入全部点")
                        .arg(draft.samples.size());
            }
            return false;
        }
        const QJsonArray rotationSamples = draft.parameters.value(
                    QStringLiteral("rotationSamples")).toArray();
        if (rotationSamples.size() != expectedRotationCount) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                            "旋转点尚未完成：%1/%2；12点模式必须完成3个旋转点")
                        .arg(rotationSamples.size()).arg(expectedRotationCount);
            }
            return false;
        }
        return true;
    }
    CalibrationSolveResult solve(const CalibrationDraft &draft) const override
    {
        QString draftError;
        if (!validateDraft(draft, &draftError)) {
            CalibrationSolveResult invalid;
            invalid.status = QStringLiteral("CAL-SOL-004");
            invalid.message = draftError;
            return invalid;
        }
        NPointCalibrationMode mode;
        draftMode(draft.parameters, &mode, nullptr);
        const bool needsImageAngle =
                mode == NPointCalibrationMode::TwelvePointPoseMapping;
        QVector<CalibrationSample> rotationSamples;
        QString rotationError;
        const QJsonArray rotationSampleJson = draft.parameters.value(
                    QStringLiteral("rotationSamples")).toArray();
        if (!parseRotationSamples(rotationSampleJson, needsImageAngle, 10,
                                  &rotationSamples, &rotationError)) {
            CalibrationSolveResult invalid;
            invalid.status = QStringLiteral("CAL-SOL-004");
            invalid.message = rotationError;
            return invalid;
        }
        CalibrationSolver solver;
        CalibrationSolveResult result = solver.solveNPoint(
                    draft.samples,
                    draft.parameters.value(QStringLiteral("rmseLimit")).toDouble(0.10),
                    draft.parameters.value(QStringLiteral("maxErrorLimit")).toDouble(0.25),
                    draft.parameters.value(QStringLiteral("safeMarginPx")).toDouble(0.0));
        if (result.success) {
            const int translationCount = 9;
            const int rotationCount = rotationSamples.size();
            result.model.mode = mode;
            result.model.imageBinding = draft.parameters.value(
                        QStringLiteral("imageBinding")).toObject();
            result.model.rotationSamples = rotationSamples;
            result.model.translationSampleCount = translationCount;
            result.model.configuredRotationSampleCount = rotationCount;
            result.model.totalSampleCount = translationCount + rotationCount;
            result.model.methodData.insert(QStringLiteral("calibrationMode"),
                                           nPointCalibrationModeToString(mode));
            result.model.methodData.insert(QStringLiteral("translationCount"),
                                           translationCount);
            result.model.methodData.insert(QStringLiteral("rotationCount"),
                                           rotationCount);
            result.model.methodData.insert(QStringLiteral("sampleCount"),
                                           translationCount + rotationCount);
            result.model.methodData.insert(QStringLiteral("translationSampleCount"),
                                           translationCount);
            result.model.methodData.insert(QStringLiteral("rotationSampleCount"),
                                           rotationSamples.size());
            result.model.methodData.insert(QStringLiteral("totalSampleCount"),
                                           translationCount + rotationCount);
            result.model.methodData.insert(QStringLiteral("xyFitSampleCount"),
                                           result.model.samples.size());
            result.model.methodData.insert(
                        QStringLiteral("rotationCapability"),
                        mode == NPointCalibrationMode::NinePointXY
                        ? QStringLiteral("not_configured")
                        : mode == NPointCalibrationMode::TwelvePointAxisTrace
                          ? QStringLiteral("axis_trace_diagnostic")
                          : QStringLiteral("pose_mapping"));
            if (rotationCount > 0) {
                result.model.rotationRange = solver.buildAxisTrace(
                            rotationSamples, result.model.forward,
                            draft.parameters.value(
                                QStringLiteral("rotationMinimumSpanDeg"))
                            .toDouble(5.0),
                            draft.parameters.value(
                                QStringLiteral("rotationRmseLimitMm"))
                            .toDouble(0.10),
                            draft.parameters.value(
                                QStringLiteral("rotationMaxErrorLimitMm"))
                            .toDouble(0.25));
            }
            result.model.methodData.insert(
                        QStringLiteral("axisTraceStatus"),
                        calibrationRotationRangeStatusToString(
                            result.model.rotationRange.status));
            result.model.methodData.insert(
                        QStringLiteral("axisTraceDirection"),
                        calibrationRotationDirectionToString(
                            result.model.rotationRange.direction));
            result.model.methodData.insert(
                        QStringLiteral("axisTraceCoaxial"),
                        result.model.rotationRange.coaxial);
            result.model.methodData.insert(
                        QStringLiteral("rotationCenterOffsetX"),
                        result.model.rotationRange.centerOffsetX);
            result.model.methodData.insert(
                        QStringLiteral("rotationCenterOffsetY"),
                        result.model.rotationRange.centerOffsetY);
            result.model.methodData.insert(
                        QStringLiteral("rotationRadiusMm"),
                        result.model.rotationRange.radiusMm);
            result.model.methodData.insert(
                        QStringLiteral("rotationPhaseOffsetDeg"),
                        result.model.rotationRange.phaseOffsetDeg);
            result.model.methodData.insert(
                        QStringLiteral("rotationFitRmseMm"),
                        result.model.rotationRange.fitRmseMm);
            result.model.methodData.insert(
                        QStringLiteral("rotationMaxErrorMm"),
                        result.model.rotationRange.maxErrorMm);
            result.model.methodData.insert(
                        QStringLiteral("axisTraceValidationCode"),
                        result.model.rotationRange.validationCode);
            if (result.model.rotationRange.status
                    == CalibrationRotationRangeStatus::Invalid) {
                result.success = false;
                result.status = QStringLiteral("CAL-SOL-004");
                result.message = rotationFailureMessage(
                            result.model.rotationRange);
                return result;
            }
            if (mode != NPointCalibrationMode::NinePointXY
                    && result.model.rotationRange.status
                       != CalibrationRotationRangeStatus::Verified) {
                result.success = false;
                result.status = QStringLiteral("CAL-SOL-004");
                result.message = QStringLiteral(
                            "已配置旋转样本，但旋转轴偏心位置模型未通过验证");
                return result;
            }

            if (mode == NPointCalibrationMode::TwelvePointPoseMapping) {
                result.model.angleMapping = solver.buildAngleMapping(
                            rotationSamples, result.model.forward,
                            draft.parameters.value(
                                QStringLiteral("angleMinimumSpanDeg"))
                            .toDouble(5.0),
                            draft.parameters.value(
                                QStringLiteral("angleRmseLimitDeg"))
                            .toDouble(1.0),
                            draft.parameters.value(
                                QStringLiteral("angleMaxErrorLimitDeg"))
                            .toDouble(2.0));
                if (!result.model.angleMapping.isVerified()) {
                    result.success = false;
                    result.status = QStringLiteral("CAL-SOL-005");
                    result.message = angleMappingFailureMessage(
                                result.model.angleMapping);
                    return result;
                }
            }
            result.model.methodData.insert(
                        QStringLiteral("angleMappingStatus"),
                        calibrationAngleMappingStatusToString(
                            result.model.angleMapping.status));
            result.model.methodData.insert(
                        QStringLiteral("angleMappingDirection"),
                        calibrationAngleDirectionToString(
                            result.model.angleMapping.direction));
            result.model.methodData.insert(
                        QStringLiteral("angleMappingOffsetDeg"),
                        result.model.angleMapping.offsetDeg);
            result.model.methodData.insert(
                        QStringLiteral("angleMappingRmseDeg"),
                        result.model.angleMapping.rmseDeg);
            result.model.methodData.insert(
                        QStringLiteral("angleMappingMaxErrorDeg"),
                        result.model.angleMapping.maxErrorDeg);
            result.model.methodData.insert(
                        QStringLiteral("angleMappingValidationCode"),
                        result.model.angleMapping.validationCode);
            QString modelError;
            if (!result.model.isValid(&modelError)) {
                result.success = false;
                result.status = QStringLiteral("CAL-SOL-004");
                result.message = modelError;
                return result;
            }
            if (result.model.quality.passed) {
                if (mode == NPointCalibrationMode::NinePointXY) {
                    result.message = QStringLiteral("9点XY标定求解成功");
                } else if (mode
                           == NPointCalibrationMode::TwelvePointAxisTrace) {
                    result.message = QStringLiteral(
                                "9点XY + 3点轴轨迹模型求解成功；轴轨迹仅用于模型与诊断");
                } else {
                    result.message = QStringLiteral(
                                "9点XY + 3点轴轨迹及姿态角映射求解成功");
                }
            }
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

// 未实现方式的显式占位，所有求解入口均返回 unsupported，禁止静默降级。
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
