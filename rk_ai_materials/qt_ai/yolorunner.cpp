#include "yolorunner.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

YoloRunner::YoloRunner(const YoloConfig &config, const QString &appDirPath, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_appDirPath(appDirPath)
    , m_process(new QProcess(this))
{
    qRegisterMetaType<YoloResult>("YoloResult");

    connect(m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            &YoloRunner::handleProcessFinished);
}

bool YoloRunner::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

void YoloRunner::setConfig(const YoloConfig &config)
{
    m_config = config;
}

bool YoloRunner::run(const QString &inputImagePath)
{
    if (isRunning()) {
        return false;
    }

    if (!m_config.enabled) {
        finishWithError(inputImagePath, QStringLiteral("YOLO 推理已在配置中禁用"));
        return false;
    }

    const QString scriptPath = resolvePath(m_config.scriptPath);
    if (!QFileInfo::exists(scriptPath)) {
        finishWithError(inputImagePath, QStringLiteral("找不到 RKNN 启动脚本: %1").arg(scriptPath));
        return false;
    }

    const QString sourceRoot = resolvePath(m_config.sourceRoot);
    const QString modelPath = resolvePath(m_config.modelPath);
    const QString labelPath = resolvePath(m_config.labelPath);
    const QString outputDir = resolvePath(m_config.outputDir);
    const QString buildDir = resolvePath(m_config.buildDir);

    if (m_config.modelPath.trimmed().isEmpty()) {
        finishWithError(inputImagePath, QStringLiteral("模型路径为空，请先在界面中选择 .rknn 文件"));
        return false;
    }
    if (m_config.labelPath.trimmed().isEmpty()) {
        finishWithError(inputImagePath, QStringLiteral("标签路径为空，请先在界面中选择 .txt 文件"));
        return false;
    }
    if (m_config.classCount <= 0) {
        finishWithError(inputImagePath, QStringLiteral("类别数必须大于 0"));
        return false;
    }
    if (m_config.boxThreshold < 0.0 || m_config.boxThreshold > 1.0) {
        finishWithError(inputImagePath, QStringLiteral("检测阈值必须在 0 到 1 之间"));
        return false;
    }

    QDir().mkpath(outputDir);
    QDir().mkpath(buildDir);

    const QString baseName = QFileInfo(inputImagePath).completeBaseName();
    m_expectedOutputImage = QDir(outputDir).filePath(baseName + QStringLiteral("_out.png"));
    QFile::remove(m_expectedOutputImage);
    m_currentInputImage = inputImagePath;

    QStringList arguments;
    arguments << sourceRoot
              << modelPath
              << labelPath
              << QString::number(m_config.classCount)
              << QString::number(m_config.boxThreshold, 'f', 3)
              << inputImagePath
              << outputDir
              << buildDir;

    m_process->setProgram(scriptPath);
    m_process->setArguments(arguments);
    m_process->setWorkingDirectory(m_appDirPath);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    emit inferenceStarted(QStringLiteral("开始执行 YOLOv8 RKNN 推理"));
    m_process->start();

    if (!m_process->waitForStarted(3000)) {
        finishWithError(inputImagePath, QStringLiteral("RKNN 推理进程启动失败"), m_process->errorString());
        return false;
    }

    return true;
}

QString YoloRunner::resolvePath(const QString &path) const
{
    if (QFileInfo(path).isAbsolute()) {
        return path;
    }
    return QDir(m_appDirPath).filePath(path);
}

void YoloRunner::finishWithError(const QString &inputImagePath, const QString &errorText, const QString &stderrText)
{
    YoloResult result;
    result.inputImagePath = inputImagePath;
    result.errorText = errorText;
    result.stderrText = stderrText;
    result.summaryText = errorText;
    emit inferenceFinished(result);
}

void YoloRunner::handleProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    YoloResult result;
    result.inputImagePath = m_currentInputImage;
    result.stdoutText = QString::fromLocal8Bit(m_process->readAllStandardOutput());
    result.stderrText = QString::fromLocal8Bit(m_process->readAllStandardError());
    result.outputImagePath = m_expectedOutputImage;
    result.commandLine = m_process->program() + QLatin1Char(' ') + m_process->arguments().join(QLatin1Char(' '));

    const QStringList lines = result.stdoutText.split(QChar(10), Qt::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("RESULT_IMAGE="))) {
            result.outputImagePath = line.section(QLatin1Char('='), 1);
        } else if (line.startsWith(QStringLiteral("DETECTION_LINE="))) {
            result.detections << line.section(QLatin1Char('='), 1);
        }
    }

    QRegularExpression countRegex(QStringLiteral("^DETECTION_COUNT=(\\d+)$"), QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = countRegex.match(result.stdoutText);
    int detectionCount = result.detections.size();
    if (match.hasMatch()) {
        detectionCount = match.captured(1).toInt();
    }

    const bool hasOutput = QFileInfo::exists(result.outputImagePath);
    result.success = exitStatus == QProcess::NormalExit && exitCode == 0 && hasOutput;

    if (result.success) {
        result.summaryText = QStringLiteral("检测完成，目标数: %1").arg(detectionCount);
        if (result.detections.isEmpty()) {
            result.detections << QStringLiteral("未解析到框信息，可能模型未命中目标。");
        }
    } else {
        if (!result.stderrText.trimmed().isEmpty()) {
            result.errorText = result.stderrText.trimmed();
        } else if (!hasOutput) {
            result.errorText = QStringLiteral("RKNN 推理未生成结果图，当前主机可能不是 RK3588/aarch64。");
        } else {
            result.errorText = QStringLiteral("RKNN 推理进程异常退出，exitCode=%1").arg(exitCode);
        }
        result.summaryText = result.errorText;
    }

    emit inferenceFinished(result);
}
