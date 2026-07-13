#include "appconfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

QJsonObject cameraToJson(const CameraConfig &config)
{
    QJsonObject object;
    object.insert(QStringLiteral("name"), config.name);
    object.insert(QStringLiteral("device"), config.device);
    object.insert(QStringLiteral("preferredWidth"), config.preferredWidth);
    object.insert(QStringLiteral("preferredHeight"), config.preferredHeight);
    object.insert(QStringLiteral("preferredFps"), config.preferredFps);
    object.insert(QStringLiteral("fallbackWidth"), config.fallbackWidth);
    object.insert(QStringLiteral("fallbackHeight"), config.fallbackHeight);
    object.insert(QStringLiteral("fallbackFps"), config.fallbackFps);
    object.insert(QStringLiteral("preferHardwareDecode"), config.preferHardwareDecode);
    object.insert(QStringLiteral("preferredDecoder"), config.preferredDecoder);
    return object;
}

CameraConfig cameraFromJson(const QJsonObject &object, const CameraConfig &defaults)
{
    CameraConfig config = defaults;
    config.name = object.value(QStringLiteral("name")).toString(config.name);
    config.device = object.value(QStringLiteral("device")).toString(config.device);
    config.preferredWidth = object.value(QStringLiteral("preferredWidth")).toInt(config.preferredWidth);
    config.preferredHeight = object.value(QStringLiteral("preferredHeight")).toInt(config.preferredHeight);
    config.preferredFps = object.value(QStringLiteral("preferredFps")).toInt(config.preferredFps);
    config.fallbackWidth = object.value(QStringLiteral("fallbackWidth")).toInt(config.fallbackWidth);
    config.fallbackHeight = object.value(QStringLiteral("fallbackHeight")).toInt(config.fallbackHeight);
    config.fallbackFps = object.value(QStringLiteral("fallbackFps")).toInt(config.fallbackFps);
    config.preferHardwareDecode = object.value(QStringLiteral("preferHardwareDecode")).toBool(config.preferHardwareDecode);
    config.preferredDecoder = object.value(QStringLiteral("preferredDecoder")).toString(config.preferredDecoder);
    return config;
}

QJsonObject yoloToJson(const YoloConfig &config)
{
    QJsonObject object;
    object.insert(QStringLiteral("enabled"), config.enabled);
    object.insert(QStringLiteral("sourceRoot"), config.sourceRoot);
    object.insert(QStringLiteral("modelPath"), config.modelPath);
    object.insert(QStringLiteral("labelPath"), config.labelPath);
    object.insert(QStringLiteral("outputDir"), config.outputDir);
    object.insert(QStringLiteral("buildDir"), config.buildDir);
    object.insert(QStringLiteral("scriptPath"), config.scriptPath);
    object.insert(QStringLiteral("classCount"), config.classCount);
    object.insert(QStringLiteral("boxThreshold"), config.boxThreshold);
    object.insert(QStringLiteral("timeoutMs"), config.timeoutMs);
    return object;
}

YoloConfig yoloFromJson(const QJsonObject &object, const YoloConfig &defaults)
{
    YoloConfig config = defaults;
    config.enabled = object.value(QStringLiteral("enabled")).toBool(config.enabled);
    config.sourceRoot = object.value(QStringLiteral("sourceRoot")).toString(config.sourceRoot);
    config.modelPath = object.value(QStringLiteral("modelPath")).toString(config.modelPath);
    config.labelPath = object.value(QStringLiteral("labelPath")).toString(config.labelPath);
    config.outputDir = object.value(QStringLiteral("outputDir")).toString(config.outputDir);
    config.buildDir = object.value(QStringLiteral("buildDir")).toString(config.buildDir);
    config.scriptPath = object.value(QStringLiteral("scriptPath")).toString(config.scriptPath);
    config.classCount = object.value(QStringLiteral("classCount")).toInt(config.classCount);
    config.boxThreshold = object.value(QStringLiteral("boxThreshold")).toDouble(config.boxThreshold);
    config.timeoutMs = object.value(QStringLiteral("timeoutMs")).toInt(config.timeoutMs);
    return config;
}

bool writeConfigFile(const QString &path, const AppConfig &config, QString *errorMessage)
{
    QJsonObject root;
    root.insert(QStringLiteral("captureSourceIndex"), config.captureSourceIndex);
    root.insert(QStringLiteral("camera1"), cameraToJson(config.camera1));
    root.insert(QStringLiteral("camera2"), cameraToJson(config.camera2));
    root.insert(QStringLiteral("yolo"), yoloToJson(config.yolo));

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入配置文件: %1").arg(path);
        }
        return false;
    }

    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size() || !file.commit()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("保存配置文件失败: %1").arg(path);
        }
        return false;
    }

    return true;
}

} // namespace

AppConfig AppConfig::defaults(const QString &appDirPath)
{
    AppConfig config;
    config.camera1.name = QStringLiteral("左侧相机");
    config.camera1.device = QStringLiteral("/dev/video0");
    config.camera2.name = QStringLiteral("右侧相机");
    config.camera2.device = QStringLiteral("/dev/video2");

    config.yolo.enabled = true;
    config.yolo.sourceRoot = QStringLiteral("/home/cat/YOLOv8_RK3588_object_detect-main");
    config.yolo.modelPath = QStringLiteral("/home/cat/model/yolov8_red_black.rknn");
    config.yolo.labelPath = QStringLiteral("/home/cat/红黑线标签.txt");
    config.yolo.outputDir = QStringLiteral("runtime/results");
    config.yolo.buildDir = QStringLiteral("runtime/rknn_build");
    config.yolo.scriptPath = QStringLiteral("scripts/run_rknn_demo.sh");
    config.yolo.classCount = 2;
    config.yolo.boxThreshold = 0.02;
    config.yolo.timeoutMs = 120000;

    config.captureSourceIndex = 0;
    config.configPath = QDir(appDirPath).filePath(QStringLiteral("app_config.json"));
    return config;
}

AppConfig AppConfig::load(const QString &appDirPath, QString *errorMessage)
{
    AppConfig config = defaults(appDirPath);
    const QString path = config.configPath;

    if (!QFileInfo::exists(path)) {
        writeConfigFile(path, config, errorMessage);
        return config;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取配置文件: %1").arg(path);
        }
        return config;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("配置文件格式错误，已回退默认配置: %1").arg(parseError.errorString());
        }
        return config;
    }

    const QJsonObject root = document.object();
    config.captureSourceIndex = root.value(QStringLiteral("captureSourceIndex")).toInt(config.captureSourceIndex);
    config.camera1 = cameraFromJson(root.value(QStringLiteral("camera1")).toObject(), config.camera1);
    config.camera2 = cameraFromJson(root.value(QStringLiteral("camera2")).toObject(), config.camera2);
    config.yolo = yoloFromJson(root.value(QStringLiteral("yolo")).toObject(), config.yolo);

    return config;
}

bool AppConfig::save(QString *errorMessage) const
{
    if (configPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("配置文件路径为空，无法保存。");
        }
        return false;
    }
    return writeConfigFile(configPath, *this, errorMessage);
}
