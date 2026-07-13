#include "algorithms/ai/AiDetectionRunner.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRect>
#include <QRegularExpression>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>
#include <opencv2/imgcodecs.hpp>

namespace {

struct ProcessResult
{
    QString program;
    QStringList arguments;
    QString stdoutText;
    QString stderrText;
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::CrashExit;
    bool started = false;
    bool timedOut = false;
    qint64 elapsedMs = 0;
};

QString shellQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
    return QStringLiteral("'%1'").arg(escaped);
}

QString commandLineForLog(const QString &program, const QStringList &arguments)
{
    QStringList parts;
    parts.reserve(arguments.size() + 1);
    parts << program;
    for (const QString &argument : arguments)
        parts << shellQuote(argument);
    return parts.join(QLatin1Char(' '));
}

QString remoteTarget(const AiDetectionConfig &config)
{
    return QStringLiteral("%1@%2").arg(config.remoteUser, config.remoteHost);
}

QString sshControlPath()
{
    return QStringLiteral("/tmp/v2_ai_bridge_mux_%r_%h_%p");
}

QStringList sshOptions()
{
    return {
        QStringLiteral("-o"), QStringLiteral("ConnectTimeout=10"),
        QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
        QStringLiteral("-o"), QStringLiteral("StrictHostKeyChecking=accept-new"),
        QStringLiteral("-o"), QStringLiteral("ControlMaster=auto"),
        QStringLiteral("-o"), QStringLiteral("ControlPath=%1").arg(sshControlPath()),
        QStringLiteral("-o"), QStringLiteral("ControlPersist=120")
    };
}

QString remoteCommand(const QString &program, const QStringList &arguments)
{
    QStringList parts;
    parts.reserve(arguments.size() + 1);
    parts << shellQuote(program);
    for (const QString &argument : arguments)
        parts << shellQuote(argument);
    return parts.join(QLatin1Char(' '));
}

QStringList scriptArguments8(const AiDetectionConfig &config, const QString &remoteInputImage)
{
    return {
        config.sourceRootOnRK,
        config.modelPathOnRK,
        config.labelPathOnRK,
        QString::number(config.classCount),
        QString::number(config.boxThreshold, 'f', 6),
        remoteInputImage,
        config.remoteOutputDir,
        config.remoteBuildDir
    };
}

QStringList scriptArguments5Legacy(const AiDetectionConfig &config, const QString &remoteInputImage)
{
    return {
        config.sourceRootOnRK,
        config.modelPathOnRK,
        remoteInputImage,
        config.remoteOutputDir,
        config.remoteBuildDir
    };
}

QJsonObject rectToJson(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
}

QJsonArray stringsToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values)
        array.append(value);
    return array;
}

QJsonObject detectionToJson(const AiDetectionDetection &detection)
{
    QJsonObject json;
    json.insert(QStringLiteral("className"), detection.className);
    json.insert(QStringLiteral("score"), detection.score);
    json.insert(QStringLiteral("bboxPixel"), rectToJson(detection.bboxPixel));
    json.insert(QStringLiteral("bboxNormalized"), rectToJson(detection.bboxNormalized));
    return json;
}

QJsonArray detectionsToJson(const QVector<AiDetectionDetection> &detections)
{
    QJsonArray array;
    for (const AiDetectionDetection &detection : detections)
        array.append(detectionToJson(detection));
    return array;
}

QJsonObject processToJson(const ProcessResult &process)
{
    QJsonObject json;
    json.insert(QStringLiteral("commandLine"), commandLineForLog(process.program, process.arguments));
    json.insert(QStringLiteral("exitCode"), process.exitCode);
    json.insert(QStringLiteral("exitStatus"), process.exitStatus == QProcess::NormalExit
                ? QStringLiteral("normal")
                : QStringLiteral("crash"));
    json.insert(QStringLiteral("started"), process.started);
    json.insert(QStringLiteral("timedOut"), process.timedOut);
    json.insert(QStringLiteral("elapsedMs"), static_cast<double>(process.elapsedMs));
    json.insert(QStringLiteral("stdout"), process.stdoutText);
    json.insert(QStringLiteral("stderr"), process.stderrText);
    return json;
}

bool isFiniteRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) &&
           std::isfinite(rect.y()) &&
           std::isfinite(rect.width()) &&
           std::isfinite(rect.height()) &&
           rect.width() > 0.0 &&
           rect.height() > 0.0;
}

QRectF normalizedRoiOrDefault(const QRectF &roi)
{
    if (!isFiniteRect(roi))
        return QRectF(0.0, 0.0, 1.0, 1.0);

    const QRectF normalized = roi.normalized();
    const double left = qBound(0.0, normalized.left(), 1.0);
    const double top = qBound(0.0, normalized.top(), 1.0);
    const double right = qBound(0.0, normalized.right(), 1.0);
    const double bottom = qBound(0.0, normalized.bottom(), 1.0);
    const QRectF clamped(QPointF(left, top), QPointF(right, bottom));
    return clamped.width() > 0.0 && clamped.height() > 0.0
            ? clamped.normalized()
            : QRectF(0.0, 0.0, 1.0, 1.0);
}

bool isFullImageRoi(const QRectF &roi)
{
    const QRectF normalized = normalizedRoiOrDefault(roi);
    return normalized.left() <= 0.000001 &&
           normalized.top() <= 0.000001 &&
           normalized.right() >= 0.999999 &&
           normalized.bottom() >= 0.999999;
}

QRect normalizedRoiToPixels(const QRectF &sourceRoi, const int width, const int height)
{
    if (width <= 0 || height <= 0)
        return QRect();

    const QRectF roi = normalizedRoiOrDefault(sourceRoi);
    const int left = qBound(0, static_cast<int>(std::floor(roi.left() * width)), width - 1);
    const int top = qBound(0, static_cast<int>(std::floor(roi.top() * height)), height - 1);
    const int right = qBound(left + 1, static_cast<int>(std::ceil(roi.right() * width)), width);
    const int bottom = qBound(top + 1, static_cast<int>(std::ceil(roi.bottom() * height)), height);
    return QRect(left, top, right - left, bottom - top);
}

QRectF normalizedRectFromPixelRect(const QRectF &rect, const QSize &imageSize)
{
    if (imageSize.width() <= 0 || imageSize.height() <= 0)
        return QRectF();

    const double left = qBound(0.0, rect.left() / imageSize.width(), 1.0);
    const double top = qBound(0.0, rect.top() / imageSize.height(), 1.0);
    const double right = qBound(0.0, rect.right() / imageSize.width(), 1.0);
    const double bottom = qBound(0.0, rect.bottom() / imageSize.height(), 1.0);
    return QRectF(QPointF(left, top), QPointF(right, bottom)).normalized();
}

ProcessResult runProcess(const QString &program,
                         const QStringList &arguments,
                         const int timeoutMs,
                         QStringList *warnings)
{
    ProcessResult result;
    result.program = program;
    result.arguments = arguments;

    QString effectiveProgram = program;
    QStringList effectiveArguments = arguments;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString sshPassword = QString::fromLocal8Bit(qgetenv("V2_AI_SSH_PASSWORD")).trimmed();
    if (!sshPassword.isEmpty() &&
        (program == QStringLiteral("ssh") || program == QStringLiteral("scp"))) {
        const QString sshpass = QStandardPaths::findExecutable(QStringLiteral("sshpass"));
        if (!sshpass.isEmpty()) {
            effectiveProgram = sshpass;
            effectiveArguments.prepend(program);
            effectiveArguments.prepend(QStringLiteral("-e"));
            environment.insert(QStringLiteral("SSHPASS"), sshPassword);
        } else if (warnings) {
            warnings->append(QStringLiteral("V2_AI_SSH_PASSWORD 已设置，但系统未找到 sshpass，仍使用 BatchMode ssh/scp。"));
        }
    }

    QElapsedTimer timer;
    timer.start();
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setProcessEnvironment(environment);
    process.start(effectiveProgram, effectiveArguments);
    result.started = process.waitForStarted(3000);
    if (!result.started) {
        result.stderrText = process.errorString();
        result.elapsedMs = timer.elapsed();
        return result;
    }

    const int boundedTimeoutMs = timeoutMs > 0 ? timeoutMs : 120000;
    if (!process.waitForFinished(boundedTimeoutMs)) {
        result.timedOut = true;
        process.kill();
        process.waitForFinished(3000);
    }

    result.stdoutText = QString::fromLocal8Bit(process.readAllStandardOutput());
    result.stderrText = QString::fromLocal8Bit(process.readAllStandardError());
    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.elapsedMs = timer.elapsed();
    if (effectiveProgram != program) {
        result.program = effectiveProgram;
        result.arguments = effectiveArguments;
    }
    return result;
}

bool processOk(const ProcessResult &process)
{
    return process.started &&
           !process.timedOut &&
           process.exitStatus == QProcess::NormalExit &&
           process.exitCode == 0;
}

ProcessResult runSshCommand(const AiDetectionConfig &config,
                            const QString &command,
                            const int timeoutMs,
                            QStringList *warnings)
{
    QStringList arguments = sshOptions();
    arguments << remoteTarget(config) << command;
    return runProcess(QStringLiteral("ssh"), arguments, timeoutMs, warnings);
}

ProcessResult runScpToRemote(const AiDetectionConfig &config,
                             const QString &localPath,
                             const QString &remotePath,
                             const int timeoutMs,
                             QStringList *warnings)
{
    QStringList arguments;
    arguments << QStringLiteral("-O");
    arguments << sshOptions();
    arguments << localPath << QStringLiteral("%1:%2").arg(remoteTarget(config), remotePath);
    ProcessResult result = runProcess(QStringLiteral("scp"), arguments, timeoutMs, warnings);
    if (!processOk(result) &&
        (result.stderrText.contains(QStringLiteral("unknown option"), Qt::CaseInsensitive) ||
         result.stderrText.contains(QStringLiteral("illegal option"), Qt::CaseInsensitive))) {
        QStringList fallbackArguments = sshOptions();
        fallbackArguments << localPath << QStringLiteral("%1:%2").arg(remoteTarget(config), remotePath);
        result = runProcess(QStringLiteral("scp"), fallbackArguments, timeoutMs, warnings);
    }
    return result;
}

ProcessResult runScpFromRemote(const AiDetectionConfig &config,
                               const QString &remotePath,
                               const QString &localPath,
                               const int timeoutMs,
                               QStringList *warnings)
{
    QStringList arguments;
    arguments << QStringLiteral("-O");
    arguments << sshOptions();
    arguments << QStringLiteral("%1:%2").arg(remoteTarget(config), remotePath) << localPath;
    ProcessResult result = runProcess(QStringLiteral("scp"), arguments, timeoutMs, warnings);
    if (!processOk(result) &&
        (result.stderrText.contains(QStringLiteral("unknown option"), Qt::CaseInsensitive) ||
         result.stderrText.contains(QStringLiteral("illegal option"), Qt::CaseInsensitive))) {
        QStringList fallbackArguments = sshOptions();
        fallbackArguments << QStringLiteral("%1:%2").arg(remoteTarget(config), remotePath) << localPath;
        result = runProcess(QStringLiteral("scp"), fallbackArguments, timeoutMs, warnings);
    }
    return result;
}

QStringList detectionLinesFromStdout(const QString &stdoutText)
{
    QStringList lines;
    const QStringList rawLines = stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &rawLine : rawLines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("DETECTION_LINE=")))
            lines << line.section(QLatin1Char('='), 1);
    }
    return lines;
}

QString resultImageFromStdout(const QString &stdoutText)
{
    const QStringList rawLines = stdoutText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &rawLine : rawLines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("RESULT_IMAGE=")))
            return line.section(QLatin1Char('='), 1).trimmed();
    }
    return QString();
}

int detectionCountFromStdout(const QString &stdoutText, const int defaultValue)
{
    const QRegularExpression regex(QStringLiteral("^DETECTION_COUNT=(\\d+)$"),
                                   QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = regex.match(stdoutText);
    return match.hasMatch() ? match.captured(1).toInt() : defaultValue;
}

QStringList classFilterTokens(const QString &text)
{
    QStringList tokens = text.split(QRegularExpression(QStringLiteral("[,;，；\\s]+")),
                                    Qt::SkipEmptyParts);
    for (QString &token : tokens)
        token = token.trimmed().toLower();
    tokens.removeDuplicates();
    return tokens;
}

double bestScore(const QVector<AiDetectionDetection> &detections)
{
    double best = 0.0;
    for (const AiDetectionDetection &detection : detections)
        best = std::max(best, detection.score);
    return best;
}

ToolOverlay rectOverlay(const AiDetectionDetection &detection)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.rect = detection.bboxPixel;
    overlay.label = detection.className;
    overlay.score = detection.score;
    overlay.extra.insert(QStringLiteral("source"), QStringLiteral("AiDetectionRunner"));
    return overlay;
}

ToolOverlay textOverlay(const AiDetectionDetection &detection, const bool showLabels, const bool showScores)
{
    QStringList parts;
    if (showLabels)
        parts << detection.className;
    if (showScores)
        parts << QString::number(detection.score, 'f', 2);

    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.p1 = QPointF(detection.bboxPixel.left(),
                         std::max(0.0, detection.bboxPixel.top() - 18.0));
    overlay.text = parts.join(QLatin1Char(' ')).trimmed();
    overlay.label = detection.className;
    overlay.score = detection.score;
    overlay.extra.insert(QStringLiteral("source"), QStringLiteral("AiDetectionRunner"));
    return overlay;
}

QVector<ToolOverlay> makeOverlays(const QVector<AiDetectionDetection> &detections,
                                  const AiDetectionConfig &config)
{
    QVector<ToolOverlay> overlays;
    for (const AiDetectionDetection &detection : detections) {
        if (config.showBoxes)
            overlays.append(rectOverlay(detection));
        if (config.showLabels || config.showScores)
            overlays.append(textOverlay(detection, config.showLabels, config.showScores));
    }
    return overlays;
}

QString failureMessage(const QString &prefix, const ProcessResult &process)
{
    QString detail = process.stderrText.trimmed();
    if (detail.isEmpty())
        detail = process.stdoutText.trimmed();
    if (detail.isEmpty())
        detail = process.timedOut
                ? QStringLiteral("process timed out")
                : QStringLiteral("exitCode=%1").arg(process.exitCode);
    return QStringLiteral("%1：%2").arg(prefix, detail);
}

void fillCommonPayload(QJsonObject *payload,
                       const AiDetectionConfig &config,
                       const AiDetectionRunnerResult &result,
                       const QStringList &warnings,
                       const bool roiApplied,
                       const QRect &roiPixels,
                       const int scriptReportedCount)
{
    payload->insert(QStringLiteral("remoteHost"), config.remoteHost);
    payload->insert(QStringLiteral("remoteUser"), config.remoteUser);
    payload->insert(QStringLiteral("modelName"), config.modelName);
    payload->insert(QStringLiteral("modelPathOnRK"), config.modelPathOnRK);
    payload->insert(QStringLiteral("labelPathOnRK"), config.labelPathOnRK);
    payload->insert(QStringLiteral("scriptPathOnRK"), config.scriptPathOnRK);
    payload->insert(QStringLiteral("sourceRootOnRK"), config.sourceRootOnRK);
    payload->insert(QStringLiteral("classCount"), config.classCount);
    payload->insert(QStringLiteral("boxThreshold"), config.boxThreshold);
    payload->insert(QStringLiteral("nmsIouThreshold"), config.nmsIouThreshold);
    payload->insert(QStringLiteral("maxOverlap"), config.nmsIouThreshold);
    payload->insert(QStringLiteral("maxDetections"), config.maxDetections);
    payload->insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    payload->insert(QStringLiteral("roiApplied"), roiApplied);
    payload->insert(QStringLiteral("roiPixel"), rectToJson(QRectF(roiPixels)));
    payload->insert(QStringLiteral("detectRegionType"), config.detectRegionType);
    payload->insert(QStringLiteral("localInputImage"), result.localInputImage);
    payload->insert(QStringLiteral("remoteInputImage"), result.remoteInputImage);
    payload->insert(QStringLiteral("remoteResultImage"), result.remoteResultImage);
    payload->insert(QStringLiteral("localResultImage"), result.localResultImage);
    payload->insert(QStringLiteral("rawDetectionCount"), result.rawDetections.size());
    payload->insert(QStringLiteral("scriptDetectionCount"), scriptReportedCount);
    payload->insert(QStringLiteral("detectionCount"), result.detections.size());
    payload->insert(QStringLiteral("rawDetections"), detectionsToJson(result.rawDetections));
    payload->insert(QStringLiteral("detections"), detectionsToJson(result.detections));
    payload->insert(QStringLiteral("rawDetectionLines"), stringsToJson(result.rawDetectionLines));
    payload->insert(QStringLiteral("stdout"), result.stdoutText);
    payload->insert(QStringLiteral("stderr"), result.stderrText);
    payload->insert(QStringLiteral("elapsedMs"), static_cast<double>(result.elapsedMs));
    payload->insert(QStringLiteral("warnings"), stringsToJson(warnings));
    payload->insert(QStringLiteral("params"), config.paramsSnapshot);
    payload->insert(QStringLiteral("judgeRule"), config.judgeRuleSnapshot);
    payload->insert(QStringLiteral("classFilterEnabled"), config.classFilterEnabled);
    payload->insert(QStringLiteral("classFilterText"), config.classFilterText);
    payload->insert(QStringLiteral("angleFilterEnabled"), config.angleFilterEnabled);
    payload->insert(QStringLiteral("minAngle"), config.minAngle);
    payload->insert(QStringLiteral("maxAngle"), config.maxAngle);
    payload->insert(QStringLiteral("widthFilterEnabled"), config.widthFilterEnabled);
    payload->insert(QStringLiteral("minWidth"), config.minWidth);
    payload->insert(QStringLiteral("maxWidth"), config.maxWidth);
    payload->insert(QStringLiteral("heightFilterEnabled"), config.heightFilterEnabled);
    payload->insert(QStringLiteral("minHeight"), config.minHeight);
    payload->insert(QStringLiteral("maxHeight"), config.maxHeight);
    payload->insert(QStringLiteral("boundaryFilterEnabled"), config.boundaryFilterEnabled);
    payload->insert(QStringLiteral("boundaryOverlapRatio"), config.boundaryOverlapRatio);
    payload->insert(QStringLiteral("sortMode"), config.sortMode);
    payload->insert(QStringLiteral("judgeMode"), config.judgeMode);
    payload->insert(QStringLiteral("resultBasis"), config.resultBasis);
    payload->insert(QStringLiteral("minCount"), config.minCount);
    payload->insert(QStringLiteral("maxCount"), config.maxCount);
    payload->insert(QStringLiteral("minScore"), config.minScore);
    payload->insert(QStringLiteral("category"), config.category);
    payload->insert(QStringLiteral("scriptArgumentMode"), result.scriptArgumentMode);
}

} // namespace

bool AiDetectionRunner::parseDetectionLine(const QString &line,
                                           AiDetectionDetection *detection,
                                           QString *errorMessage)
{
    QString text = line.trimmed();
    if (text.startsWith(QStringLiteral("DETECTION_LINE=")))
        text = text.section(QLatin1Char('='), 1).trimmed();

    static const QRegularExpression regex(
                QStringLiteral("^(.+?) @ \\(([-+0-9.]+)\\s+([-+0-9.]+)\\s+([-+0-9.]+)\\s+([-+0-9.]+)\\)\\s+([-+0-9.]+)"));
    const QRegularExpressionMatch match = regex.match(text);
    if (!match.hasMatch()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("DETECTION_LINE 格式不匹配：%1").arg(line);
        return false;
    }

    bool ok = false;
    const double left = match.captured(2).toDouble(&ok);
    if (!ok)
        return false;
    const double top = match.captured(3).toDouble(&ok);
    if (!ok)
        return false;
    const double right = match.captured(4).toDouble(&ok);
    if (!ok)
        return false;
    const double bottom = match.captured(5).toDouble(&ok);
    if (!ok)
        return false;
    const double score = match.captured(6).toDouble(&ok);
    if (!ok)
        return false;

    QRectF rect(QPointF(left, top), QPointF(right, bottom));
    rect = rect.normalized();

    if (detection) {
        detection->className = match.captured(1).trimmed();
        detection->score = score;
        detection->bboxPixel = QRectF(rect.left(),
                                      rect.top(),
                                      rect.width(),
                                      rect.height());
        detection->bboxNormalized = QRectF();
    }
    return true;
}

QVector<AiDetectionDetection> AiDetectionRunner::parseDetectionLines(const QStringList &lines,
                                                                     const QSize &uploadedImageSize,
                                                                     const QSize &originalImageSize,
                                                                     const QPointF &imageOffset)
{
    Q_UNUSED(uploadedImageSize)

    QVector<AiDetectionDetection> detections;
    detections.reserve(lines.size());
    for (const QString &line : lines) {
        AiDetectionDetection detection;
        if (!parseDetectionLine(line, &detection))
            continue;

        detection.bboxPixel.translate(imageOffset);
        detection.bboxNormalized = normalizedRectFromPixelRect(detection.bboxPixel, originalImageSize);
        detections.append(detection);
    }
    return detections;
}

AiDetectionFilterResult AiDetectionRunner::applyFilters(const QVector<AiDetectionDetection> &detections,
                                                        const AiDetectionConfig &config)
{
    AiDetectionFilterResult result;
    result.detections = detections;

    if (config.classFilterEnabled) {
        const QStringList allowed = classFilterTokens(config.classFilterText);
        if (allowed.isEmpty()) {
            result.warnings << QStringLiteral("classFilterEnabled=true，但 classFilterText 为空，类别过滤未生效。");
        } else {
            QVector<AiDetectionDetection> filtered;
            for (const AiDetectionDetection &detection : result.detections) {
                if (allowed.contains(detection.className.trimmed().toLower()))
                    filtered.append(detection);
            }
            result.detections = filtered;
        }
    }

    if (config.widthFilterEnabled) {
        double minWidth = config.minWidth;
        double maxWidth = config.maxWidth;
        if (maxWidth < minWidth)
            std::swap(minWidth, maxWidth);
        QVector<AiDetectionDetection> filtered;
        for (const AiDetectionDetection &detection : result.detections) {
            const double width = detection.bboxPixel.width();
            if (width >= minWidth && width <= maxWidth)
                filtered.append(detection);
        }
        result.detections = filtered;
    }

    if (config.heightFilterEnabled) {
        double minHeight = config.minHeight;
        double maxHeight = config.maxHeight;
        if (maxHeight < minHeight)
            std::swap(minHeight, maxHeight);
        QVector<AiDetectionDetection> filtered;
        for (const AiDetectionDetection &detection : result.detections) {
            const double height = detection.bboxPixel.height();
            if (height >= minHeight && height <= maxHeight)
                filtered.append(detection);
        }
        result.detections = filtered;
    }

    if (config.angleFilterEnabled)
        result.warnings << QStringLiteral("angleFilterEnabled 已读取，但 RK 输出没有 angle，桥接版暂未应用角度过滤。");

    if (config.boundaryFilterEnabled)
        result.warnings << QStringLiteral("boundaryFilterEnabled 已读取，但桥接版暂未完整实现出界过滤。");

    if (!config.sortMode.trimmed().isEmpty())
        result.warnings << QStringLiteral("sortMode 已读取，但桥接版暂未应用排序策略。");

    if (config.maxDetections > 0 && result.detections.size() > config.maxDetections)
        result.detections.resize(config.maxDetections);

    return result;
}

AiDetectionJudgeDecision AiDetectionRunner::judgeDetections(const QVector<AiDetectionDetection> &detections,
                                                            const AiDetectionConfig &config)
{
    AiDetectionJudgeDecision decision;
    QString mode = config.judgeMode.trimmed().toLower();
    const QString basis = config.resultBasis.trimmed();
    if (mode.isEmpty() || mode == QStringLiteral("unknown")) {
        if (basis.contains(QStringLiteral("最低得分")))
            mode = QStringLiteral("score");
        else if (basis.contains(QStringLiteral("类别")))
            mode = QStringLiteral("category");
        else
            mode = QStringLiteral("count");
    }
    decision.mode = mode;

    if (mode == QStringLiteral("score")) {
        const double best = bestScore(detections);
        decision.ok = !detections.isEmpty() && best >= config.minScore;
        decision.status = decision.ok ? QStringLiteral("ok") : QStringLiteral("ng");
        decision.message = QStringLiteral("bestScore=%1 minScore=%2")
                .arg(best, 0, 'f', 3)
                .arg(config.minScore, 0, 'f', 3);
        return decision;
    }

    if (mode == QStringLiteral("category")) {
        const QString expected = config.category.trimmed().toLower();
        if (expected.isEmpty()) {
            decision.ok = false;
            decision.status = QStringLiteral("ng");
            decision.message = QStringLiteral("类别判断未配置 category。");
            return decision;
        }

        bool matched = false;
        for (const AiDetectionDetection &detection : detections) {
            if (detection.className.trimmed().toLower() == expected) {
                matched = true;
                break;
            }
        }
        decision.ok = matched;
        decision.status = decision.ok ? QStringLiteral("ok") : QStringLiteral("ng");
        decision.message = QStringLiteral("category=%1 matched=%2")
                .arg(config.category, decision.ok ? QStringLiteral("true") : QStringLiteral("false"));
        return decision;
    }

    int minCount = config.minCount;
    int maxCount = config.maxCount;
    if (maxCount < minCount)
        std::swap(minCount, maxCount);
    const int count = detections.size();
    decision.mode = QStringLiteral("count");
    decision.ok = count >= minCount && count <= maxCount;
    decision.status = decision.ok ? QStringLiteral("ok") : QStringLiteral("ng");
    decision.message = QStringLiteral("count=%1 range=[%2,%3]").arg(count).arg(minCount).arg(maxCount);
    return decision;
}

AiDetectionRunnerResult AiDetectionRunner::run(const cv::Mat &image, const AiDetectionConfig &config) const
{
    QElapsedTimer timer;
    timer.start();

    AiDetectionRunnerResult result;
    result.scriptArgumentMode = QStringLiteral("8");
    QStringList warnings = config.warnings;
    const QRectF roiNormalized = normalizedRoiOrDefault(config.roiNormalized);
    const bool roiApplied = !isFullImageRoi(roiNormalized);
    QRect roiPixels;
    int scriptReportedCount = 0;

    if (image.empty()) {
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("empty_image");
        result.message = QStringLiteral("目标检测远程推理失败：当前图像为空。");
        result.elapsedMs = timer.elapsed();
        fillCommonPayload(&result.payload, config, result, warnings, false, QRect(), scriptReportedCount);
        return result;
    }

    const QSize originalImageSize(image.cols, image.rows);
    cv::Mat uploadImage = image;
    QPointF imageOffset(0.0, 0.0);
    if (roiApplied) {
        roiPixels = normalizedRoiToPixels(roiNormalized, image.cols, image.rows);
        if (roiPixels.isValid() && roiPixels.width() > 0 && roiPixels.height() > 0) {
            const cv::Rect cvRoi(roiPixels.x(), roiPixels.y(), roiPixels.width(), roiPixels.height());
            uploadImage = image(cvRoi).clone();
            imageOffset = QPointF(roiPixels.x(), roiPixels.y());
        } else {
            warnings << QStringLiteral("roiNormalized 已读取，但像素 ROI 无效，桥接版改用全图上传。");
            roiPixels = QRect(0, 0, image.cols, image.rows);
            uploadImage = image;
            imageOffset = QPointF(0.0, 0.0);
        }
    } else {
        roiPixels = QRect(0, 0, image.cols, image.rows);
    }

    if (config.detectRegionType.trimmed().toLower() != QStringLiteral("rectangle") &&
        config.detectRegionType.trimmed().toLower() != QStringLiteral("rect")) {
        warnings << QStringLiteral("detectRegionType 已读取，但桥接版仅支持矩形 ROI，自由 ROI 暂未生效。");
    }

    const QString requestId = QString::number(QDateTime::currentMSecsSinceEpoch());
    QDir localDir(config.localTempRoot);
    if (!localDir.exists() && !QDir().mkpath(config.localTempRoot)) {
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("local_temp_failed");
        result.message = QStringLiteral("目标检测远程推理失败：无法创建本地临时目录 %1。").arg(config.localTempRoot);
        result.elapsedMs = timer.elapsed();
        fillCommonPayload(&result.payload, config, result, warnings, roiApplied, roiPixels, scriptReportedCount);
        return result;
    }

    result.localInputImage = localDir.filePath(QStringLiteral("request_%1.jpg").arg(requestId));
    result.localResultImage = localDir.filePath(QStringLiteral("result_%1.png").arg(requestId));
    result.remoteInputImage = QStringLiteral("%1/request_%2.jpg").arg(config.remoteInputDir, requestId);

    if (!cv::imwrite(result.localInputImage.toLocal8Bit().constData(), uploadImage)) {
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("local_save_failed");
        result.message = QStringLiteral("目标检测远程推理失败：无法保存本地临时图 %1。").arg(result.localInputImage);
        result.elapsedMs = timer.elapsed();
        fillCommonPayload(&result.payload, config, result, warnings, roiApplied, roiPixels, scriptReportedCount);
        return result;
    }

    const QString mkdirCommand = QStringLiteral("mkdir -p %1 %2 %3")
            .arg(shellQuote(config.remoteInputDir),
                 shellQuote(config.remoteOutputDir),
                 shellQuote(config.remoteBuildDir));
    const ProcessResult mkdirResult = runSshCommand(config, mkdirCommand, config.timeoutMs, &warnings);
    result.payload.insert(QStringLiteral("mkdirProcess"), processToJson(mkdirResult));
    if (!processOk(mkdirResult)) {
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("remote_mkdir_failed");
        result.message = failureMessage(QStringLiteral("目标检测远程推理失败：远程临时目录创建失败"), mkdirResult);
        result.stdoutText = mkdirResult.stdoutText;
        result.stderrText = mkdirResult.stderrText;
        result.elapsedMs = timer.elapsed();
        fillCommonPayload(&result.payload, config, result, warnings, roiApplied, roiPixels, scriptReportedCount);
        return result;
    }

    const ProcessResult scpInputResult = runScpToRemote(config,
                                                       result.localInputImage,
                                                       result.remoteInputImage,
                                                       config.timeoutMs,
                                                       &warnings);
    result.payload.insert(QStringLiteral("scpInputProcess"), processToJson(scpInputResult));
    if (!processOk(scpInputResult)) {
        result.success = false;
        result.ok = false;
        result.status = QStringLiteral("scp_input_failed");
        result.message = failureMessage(QStringLiteral("目标检测远程推理失败：scp 上传输入图失败"), scpInputResult);
        result.stdoutText = scpInputResult.stdoutText;
        result.stderrText = scpInputResult.stderrText;
        result.elapsedMs = timer.elapsed();
        fillCommonPayload(&result.payload, config, result, warnings, roiApplied, roiPixels, scriptReportedCount);
        return result;
    }

    const QString primaryCommand = remoteCommand(QStringLiteral("bash"),
                                                QStringList{config.scriptPathOnRK} +
                                                scriptArguments8(config, result.remoteInputImage));
    ProcessResult scriptResult = runSshCommand(config, primaryCommand, config.timeoutMs, &warnings);
    result.payload.insert(QStringLiteral("scriptProcess8"), processToJson(scriptResult));

    if (!processOk(scriptResult) && config.allowLegacyScriptFallback) {
        warnings << QStringLiteral("8 参数 run_rknn_demo.sh 调用失败；当前 RK 板脚本疑似 5 参数旧版，已尝试 legacy fallback。");
        const QString legacyCommand = remoteCommand(QStringLiteral("bash"),
                                                   QStringList{config.scriptPathOnRK} +
                                                   scriptArguments5Legacy(config, result.remoteInputImage));
        const ProcessResult legacyResult = runSshCommand(config, legacyCommand, config.timeoutMs, &warnings);
        result.payload.insert(QStringLiteral("scriptProcess5Legacy"), processToJson(legacyResult));
        if (processOk(legacyResult)) {
            warnings << QStringLiteral("legacy 5 参数 fallback 已生效；labelPath/classCount/boxThreshold 未传入旧脚本。");
            scriptResult = legacyResult;
            result.scriptArgumentMode = QStringLiteral("5_legacy_fallback");
        }
    }

    result.stdoutText = scriptResult.stdoutText;
    result.stderrText = scriptResult.stderrText;

    if (!processOk(scriptResult)) {
        result.success = false;
        result.ok = false;
        result.status = scriptResult.timedOut
                ? QStringLiteral("remote_inference_timeout")
                : QStringLiteral("remote_inference_failed");
        result.message = failureMessage(QStringLiteral("目标检测远程推理失败：远程脚本执行失败"), scriptResult);
        result.elapsedMs = timer.elapsed();
        fillCommonPayload(&result.payload, config, result, warnings, roiApplied, roiPixels, scriptReportedCount);
        return result;
    }

    result.remoteResultImage = resultImageFromStdout(result.stdoutText);
    result.rawDetectionLines = detectionLinesFromStdout(result.stdoutText);
    scriptReportedCount = detectionCountFromStdout(result.stdoutText, result.rawDetectionLines.size());
    const QSize uploadedImageSize(uploadImage.cols, uploadImage.rows);
    result.rawDetections = parseDetectionLines(result.rawDetectionLines,
                                               uploadedImageSize,
                                               originalImageSize,
                                               imageOffset);

    if (scriptReportedCount != result.rawDetections.size()) {
        warnings << QStringLiteral("DETECTION_COUNT=%1，但成功解析 bbox=%2。")
                    .arg(scriptReportedCount)
                    .arg(result.rawDetections.size());
    }

    const AiDetectionFilterResult filterResult = applyFilters(result.rawDetections, config);
    result.detections = filterResult.detections;
    warnings << filterResult.warnings;
    result.score = bestScore(result.detections);
    result.count = result.detections.size();
    result.overlays = makeOverlays(result.detections, config);

    const AiDetectionJudgeDecision decision = judgeDetections(result.detections, config);
    result.success = true;
    result.ok = decision.ok;
    result.status = decision.status;
    result.message = QStringLiteral("目标检测完成，检测到 %1 个目标").arg(result.count);
    result.text = decision.message;

    if (result.remoteResultImage.trimmed().isEmpty()) {
        warnings << QStringLiteral("远程 stdout 未包含 RESULT_IMAGE。");
    } else {
        const ProcessResult scpResult = runScpFromRemote(config,
                                                        result.remoteResultImage,
                                                        result.localResultImage,
                                                        config.timeoutMs,
                                                        &warnings);
        result.payload.insert(QStringLiteral("scpResultImageProcess"), processToJson(scpResult));
        if (!processOk(scpResult)) {
            warnings << QStringLiteral("RESULT_IMAGE 回传失败，不影响 ToolResult：%1")
                        .arg(failureMessage(QStringLiteral("scp result image failed"), scpResult));
            result.localResultImage.clear();
        }
    }

    result.elapsedMs = timer.elapsed();
    fillCommonPayload(&result.payload, config, result, warnings, roiApplied, roiPixels, scriptReportedCount);
    return result;
}
