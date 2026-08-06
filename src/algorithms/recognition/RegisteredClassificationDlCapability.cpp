#include "algorithms/recognition/RegisteredClassificationDlCapability.h"

#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QJsonArray>

#include <HalconCpp.h>

#include <exception>

namespace {

QString tupleString(const HalconCpp::HTuple &value)
{
    return value.Length() > 0
            ? QString::fromUtf8(value[0].S().Text())
            : QString();
}

qint64 tupleInteger(const HalconCpp::HTuple &value, qint64 fallback = -1)
{
    return value.Length() > 0
            ? static_cast<qint64>(value[0].L())
            : fallback;
}

RegisteredClassificationDlCapabilityResult errorResult(
        const QString &status,
        const QString &message,
        const QString &licensePath)
{
    RegisteredClassificationDlCapabilityResult result;
    result.status = status;
    result.message = message;
    result.payload.insert(QStringLiteral("halconVersion"), HalconRuntimePaths::expectedHalconVersion());
    result.payload.insert(QStringLiteral("licensePath"), licensePath);
    return result;
}

} // namespace

RegisteredClassificationDlCapabilityResult
RegisteredClassificationDlCapability::probe() const
{
    const QString licensePath = HalconRuntimePaths::initializeHalconEnvironment();
    try {
        const HalconCpp::HDlDeviceArray deviceArray =
                HalconCpp::HDlDevice::QueryAvailableDlDevices(
                    HalconCpp::HTuple(), HalconCpp::HTuple());
        if (deviceArray.Length() <= 0) {
            return errorResult(
                        QStringLiteral("dl_device_unavailable"),
                        QStringLiteral("HALCON reported no deep-learning-capable CPU or GPU device."),
                        licensePath);
        }

        RegisteredClassificationDlCapabilityResult result;
        result.success = true;
        result.status = QStringLiteral("ok");
        result.message = QStringLiteral("HALCON deep-learning device capability is available.");
        QJsonArray devicesJson;
        const HalconCpp::HDlDevice *devices = deviceArray.Tools();
        for (Hlong index = 0; index < deviceArray.Length(); ++index) {
            RegisteredClassificationDlDeviceInfo info;
            info.id = tupleInteger(devices[index].GetDlDeviceParam("id"));
            info.name = tupleString(devices[index].GetDlDeviceParam("name"));
            info.type = tupleString(devices[index].GetDlDeviceParam("type")).toLower();
            info.inferenceOnly =
                    tupleString(devices[index].GetDlDeviceParam("inference_only")).toLower()
                    == QStringLiteral("true");
            result.devices.append(info);

            QJsonObject deviceJson;
            deviceJson.insert(QStringLiteral("id"), static_cast<double>(info.id));
            deviceJson.insert(QStringLiteral("name"), info.name);
            deviceJson.insert(QStringLiteral("type"), info.type);
            deviceJson.insert(QStringLiteral("inferenceOnly"), info.inferenceOnly);
            devicesJson.append(deviceJson);
        }
        result.payload.insert(QStringLiteral("halconVersion"), HalconRuntimePaths::expectedHalconVersion());
        result.payload.insert(QStringLiteral("licensePath"), licensePath);
        result.payload.insert(QStringLiteral("deviceCount"), result.devices.size());
        result.payload.insert(QStringLiteral("devices"), devicesJson);
        return result;
    } catch (const HalconCpp::HException &exception) {
        return errorResult(
                    QStringLiteral("dl_license_or_runtime_unavailable"),
                    QStringLiteral("HALCON %1 in %2: %3")
                    .arg(static_cast<qlonglong>(exception.ErrorCode()))
                    .arg(QString::fromUtf8(exception.ProcName().Text()))
                    .arg(QString::fromUtf8(exception.ErrorMessage().Text())),
                    licensePath);
    } catch (const std::exception &exception) {
        return errorResult(
                    QStringLiteral("dl_capability_probe_failed"),
                    QString::fromLocal8Bit(exception.what()),
                    licensePath);
    }
}
