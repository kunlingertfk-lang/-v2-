#ifndef YOLORUNNER_H
#define YOLORUNNER_H

#include <QObject>
#include <QProcess>
#include <QStringList>

#include "appconfig.h"

struct YoloResult {
    bool success = false;
    QString inputImagePath;
    QString outputImagePath;
    QString summaryText;
    QStringList detections;
    QString stdoutText;
    QString stderrText;
    QString errorText;
    QString commandLine;
};

Q_DECLARE_METATYPE(YoloResult)

class YoloRunner : public QObject
{
    Q_OBJECT

public:
    explicit YoloRunner(const YoloConfig &config, const QString &appDirPath, QObject *parent = nullptr);

    bool isRunning() const;
    bool run(const QString &inputImagePath);
    void setConfig(const YoloConfig &config);

signals:
    void inferenceStarted(const QString &message);
    void inferenceFinished(const YoloResult &result);

private:
    QString resolvePath(const QString &path) const;
    void finishWithError(const QString &inputImagePath, const QString &errorText, const QString &stderrText = QString());
    void handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    YoloConfig m_config;
    QString m_appDirPath;
    QProcess *m_process = nullptr;
    QString m_expectedOutputImage;
    QString m_currentInputImage;
};

#endif
