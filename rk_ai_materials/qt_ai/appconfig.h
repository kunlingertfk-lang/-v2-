#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

struct CameraConfig {
    QString name;
    QString device;
    int preferredWidth = 1920;
    int preferredHeight = 1080;
    int preferredFps = 30;
    int fallbackWidth = 640;
    int fallbackHeight = 480;
    int fallbackFps = 30;
    bool preferHardwareDecode = true;
    QString preferredDecoder = QStringLiteral("mppjpegdec");
};

struct YoloConfig {
    bool enabled = true;
    QString sourceRoot;
    QString modelPath;
    QString labelPath;
    QString outputDir;
    QString buildDir;
    QString scriptPath;
    int classCount = 2;
    double boxThreshold = 0.02;
    int timeoutMs = 120000;
};

struct AppConfig {
    CameraConfig camera1;
    CameraConfig camera2;
    YoloConfig yolo;
    int captureSourceIndex = 0;
    QString configPath;

    static AppConfig defaults(const QString &appDirPath);
    static AppConfig load(const QString &appDirPath, QString *errorMessage = nullptr);
    bool save(QString *errorMessage = nullptr) const;
};

#endif
