#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONDLCAPABILITY_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONDLCAPABILITY_H

#include <QJsonObject>
#include <QString>
#include <QVector>

struct RegisteredClassificationDlDeviceInfo
{
    qint64 id = -1;
    QString name;
    QString type;
    bool inferenceOnly = false;
};

struct RegisteredClassificationDlCapabilityResult
{
    bool success = false;
    QString status;
    QString message;
    QVector<RegisteredClassificationDlDeviceInfo> devices;
    QJsonObject payload;
};

class RegisteredClassificationDlCapability
{
public:
    RegisteredClassificationDlCapabilityResult probe() const;
};

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONDLCAPABILITY_H
