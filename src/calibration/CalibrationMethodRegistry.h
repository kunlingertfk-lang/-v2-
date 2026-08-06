#ifndef CALIBRATION_CALIBRATIONMETHODREGISTRY_H
#define CALIBRATION_CALIBRATIONMETHODREGISTRY_H

#include "calibration/CalibrationSolver.h"

#include <QJsonObject>
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

    virtual CalibrationDraft draft(QString *errorMessage = nullptr) const = 0;
    virtual void setProducerSnapshots(
            const QVector<CalibrationProducerSnapshot> &snapshots) = 0;
    virtual void setLatestPhysicalSample(bool valid,
                                         double x,
                                         double y,
                                         double angleDeg) = 0;
    virtual QString captureImageProducerId() const = 0;
    virtual bool captureCurrentSample(QString *errorMessage = nullptr) = 0;

signals:
    void sampleStateChanged(const QString &text, bool ok);
};

class ICalibrationMethod
{
public:
    virtual ~ICalibrationMethod() = default;
    virtual QString methodId() const = 0;
    virtual QString displayName() const = 0;
    virtual QString description() const = 0;
    virtual QString iconPath() const = 0;
    virtual CalibrationMethodAvailability availability() const = 0;
    virtual CalibrationMethodCapabilities capabilities() const = 0;
    virtual CalibrationMethodConfigWidget *createConfigWidget(QWidget *parent) const = 0;
    virtual bool validateDraft(const CalibrationDraft &draft,
                               QString *errorMessage) const = 0;
    virtual CalibrationSolveResult solve(const CalibrationDraft &draft) const = 0;
    virtual bool validateResult(const CalibrationSolveResult &result,
                                QString *errorMessage) const = 0;
    virtual CalibrationModel buildCalibrationModel(
            const CalibrationSolveResult &result) const = 0;
    virtual QJsonObject createResultSummary(
            const CalibrationSolveResult &result) const = 0;
};

class CalibrationMethodRegistry
{
public:
    static CalibrationMethodRegistry &instance();

    QVector<const ICalibrationMethod *> methods() const;
    const ICalibrationMethod *method(const QString &methodId) const;

private:
    CalibrationMethodRegistry();
    std::vector<std::unique_ptr<ICalibrationMethod>> m_methods;
};

QString calibrationAvailabilityText(CalibrationMethodAvailability availability);

#endif // CALIBRATION_CALIBRATIONMETHODREGISTRY_H
