#include "tooladapters/OcrAdapter.h"

#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QtGlobal>
#include <algorithm>
#include <limits>

namespace {

QString firstString(const QJsonObject &json, const QStringList &keys, const QString &defaultValue = QString())
{
    for (const QString &key : keys) {
        const QJsonValue value = json.value(key);
        if (value.isUndefined() || value.isNull())
            continue;

        const QString text = value.toString().trimmed();
        if (!text.isEmpty())
            return text;
    }

    return defaultValue;
}

double firstDouble(const QJsonObject &json, const QStringList &keys, const double defaultValue)
{
    for (const QString &key : keys) {
        const QJsonValue value = json.value(key);
        if (value.isUndefined() || value.isNull())
            continue;
        if (value.isDouble())
            return value.toDouble();

        bool ok = false;
        const double parsed = value.toString().trimmed().toDouble(&ok);
        if (ok)
            return parsed;
    }

    return defaultValue;
}

int firstInt(const QJsonObject &json, const QStringList &keys, const int defaultValue)
{
    return static_cast<int>(firstDouble(json, keys, defaultValue));
}

double normalizeScore(const double rawScore)
{
    if (rawScore > 1.0)
        return qBound(0.0, rawScore / 100.0, 1.0);

    return qBound(0.0, rawScore, 1.0);
}

QString normalizePolarity(const QString &value)
{
    const QString key = value.trimmed().toLower();
    if (key == QStringLiteral("auto") || key == QStringLiteral("automatic") || key == QStringLiteral("自动"))
        return QStringLiteral("auto");
    if (key == QStringLiteral("bright") || key == QStringLiteral("light") || key == QStringLiteral("white") ||
        key == QStringLiteral("亮") || key == QStringLiteral("亮字符") || key == QStringLiteral("亮目标"))
        return QStringLiteral("bright");

    return QStringLiteral("dark");
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

QJsonArray charsToJson(const QVector<OcrCharResult> &chars)
{
    QJsonArray array;
    for (const OcrCharResult &charResult : chars) {
        QJsonObject item;
        item.insert(QStringLiteral("character"), charResult.character);
        item.insert(QStringLiteral("confidence"), charResult.confidence);
        item.insert(QStringLiteral("box"), rectToJson(charResult.box));
        array.append(item);
    }
    return array;
}

QString rectToDebugString(const QRectF &rect)
{
    return QStringLiteral("x=%1,y=%2,w=%3,h=%4")
            .arg(rect.x(), 0, 'f', 3)
            .arg(rect.y(), 0, 'f', 3)
            .arg(rect.width(), 0, 'f', 3)
            .arg(rect.height(), 0, 'f', 3);
}

ToolResult makeOcrError(const ToolConfig &config, const QString &status, const QString &message)
{
    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::Ocr;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    return result;
}

QJsonArray stringListToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values)
        array.append(value);
    return array;
}

ToolResult makePathError(const ToolConfig &config,
                         const QString &status,
                         const QString &message,
                         const QString &payloadKey,
                         const QStringList &triedPaths)
{
    ToolResult result = makeOcrError(config, status, message);
    result.payload.insert(payloadKey, stringListToJson(triedPaths));
    return result;
}

struct OcrJudgeDecision
{
    QString mode;
    bool ok = false;
    QString status = QStringLiteral("ng");
    QString message;
    int minCount = 0;
    int maxCount = 0;
    double minScore = 0.0;
};

OcrHalconConfig toHalconConfig(const ToolConfig &config)
{
    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;

    OcrHalconConfig halconConfig;
    const QString requestedHalconSoPath = firstString(params,
                                                      {QStringLiteral("halconSoPath"),
                                                       QStringLiteral("so_path")});
    halconConfig.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                requestedHalconSoPath,
                &halconConfig.halconSoPathCandidates);
    const QString requestedOcrModelPath = firstString(params,
                                                     {QStringLiteral("ocrModelPath"),
                                                      QStringLiteral("ocr_model_path"),
                                                      QStringLiteral("modelPath")});
    halconConfig.ocrModelPath = HalconRuntimePaths::resolveOcrModelPath(
                requestedOcrModelPath,
                &halconConfig.ocrModelPathCandidates);

    halconConfig.roiNormalized = config.roiNormalized;
    if (halconConfig.roiNormalized.width() <= 0.0 || halconConfig.roiNormalized.height() <= 0.0)
        halconConfig.roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);

    halconConfig.binaryThreshold = qBound(0,
                                          firstInt(params,
                                                   {QStringLiteral("binaryThreshold"),
                                                    QStringLiteral("binary_threshold")},
                                                   halconConfig.binaryThreshold),
                                          255);
    halconConfig.objectPolarity = normalizePolarity(firstString(params,
                                                                {QStringLiteral("polarity"),
                                                                 QStringLiteral("object_polarity")},
                                                                halconConfig.objectPolarity));

    const double minCharArea = firstDouble(params,
                                           {QStringLiteral("minCharArea"),
                                            QStringLiteral("min_char_area")},
                                           halconConfig.minCharArea);
    const double maxCharArea = firstDouble(params,
                                           {QStringLiteral("maxCharArea"),
                                            QStringLiteral("max_char_area")},
                                           halconConfig.maxCharArea);
    halconConfig.minCharArea = minCharArea > 0.0 ? minCharArea : halconConfig.minCharArea;
    halconConfig.maxCharArea = maxCharArea > 0.0 ? maxCharArea : halconConfig.maxCharArea;

    halconConfig.minCharWidth = std::max(0.0,
                                         firstDouble(params,
                                                     {QStringLiteral("minCharWidth"),
                                                      QStringLiteral("min_char_width")},
                                                     halconConfig.minCharWidth));
    halconConfig.minCharHeight = std::max(0.0,
                                          firstDouble(params,
                                                      {QStringLiteral("minCharHeight"),
                                                       QStringLiteral("min_char_height")},
                                                      halconConfig.minCharHeight));
    halconConfig.maxCharWidth = std::max(halconConfig.minCharWidth,
                                         firstDouble(params,
                                                     {QStringLiteral("maxCharWidth"),
                                                      QStringLiteral("max_char_width")},
                                                     halconConfig.maxCharWidth));
    halconConfig.maxCharHeight = std::max(halconConfig.minCharHeight,
                                          firstDouble(params,
                                                      {QStringLiteral("maxCharHeight"),
                                                       QStringLiteral("max_char_height")},
                                                      halconConfig.maxCharHeight));
    halconConfig.minAspectRatio = std::max(0.0,
                                           firstDouble(params,
                                                       {QStringLiteral("minAspectRatio"),
                                                        QStringLiteral("min_aspect_ratio")},
                                                       halconConfig.minAspectRatio));
    halconConfig.maxAspectRatio = std::max(halconConfig.minAspectRatio,
                                           firstDouble(params,
                                                       {QStringLiteral("maxAspectRatio"),
                                                        QStringLiteral("max_aspect_ratio")},
                                                       halconConfig.maxAspectRatio));

    halconConfig.minConfidence = normalizeScore(firstDouble(params,
                                                            {QStringLiteral("minConfidence"),
                                                             QStringLiteral("min_confidence"),
                                                             QStringLiteral("minScore")},
                                                            halconConfig.minConfidence));

    halconConfig.expectedText = firstString(judgeRule,
                                            {QStringLiteral("expectedText")},
                                            firstString(params, {QStringLiteral("baselineText")}));
    halconConfig.matchRule = firstString(judgeRule,
                                         {QStringLiteral("matchRule")},
                                         firstString(params,
                                                     {QStringLiteral("match_rule")},
                                                     QStringLiteral("none"))).trimmed().toLower();
    if (halconConfig.matchRule.isEmpty())
        halconConfig.matchRule = QStringLiteral("none");

    return halconConfig;
}

OcrJudgeDecision judgeOcrResult(const OcrHalconResult &ocrResult,
                                const ToolConfig &config,
                                const OcrHalconConfig &halconConfig)
{
    OcrJudgeDecision decision;
    const QJsonObject judgeRule = config.judgeRule;
    decision.mode = judgeRule.value(QStringLiteral("mode")).toString(QStringLiteral("none")).trimmed().toLower();
    decision.minScore = halconConfig.minConfidence;

    if (decision.mode == QStringLiteral("char_count")) {
        decision.minCount = firstInt(judgeRule, {QStringLiteral("minCount"), QStringLiteral("min_count")}, 0);
        decision.maxCount = firstInt(judgeRule,
                                     {QStringLiteral("maxCount"), QStringLiteral("max_count")},
                                     std::numeric_limits<int>::max());
        if (decision.maxCount < decision.minCount)
            std::swap(decision.minCount, decision.maxCount);
        decision.ok = ocrResult.filteredCharCount >= decision.minCount &&
                      ocrResult.filteredCharCount <= decision.maxCount;
        decision.status = decision.ok ? QStringLiteral("ok") : QStringLiteral("count_out_of_range");
        decision.message = decision.ok
                ? QStringLiteral("OCR completed, character count is in range.")
                : QStringLiteral("OCR completed, character count is out of range.");
        return decision;
    }

    if (decision.mode == QStringLiteral("score")) {
        decision.minScore = normalizeScore(firstDouble(judgeRule,
                                                       {QStringLiteral("minScore"),
                                                        QStringLiteral("minConfidence"),
                                                        QStringLiteral("min_confidence")},
                                                       halconConfig.minConfidence));
        decision.ok = ocrResult.filteredAverageConfidence >= decision.minScore;
        decision.status = decision.ok ? QStringLiteral("ok") : QStringLiteral("score_too_low");
        decision.message = decision.ok
                ? QStringLiteral("OCR completed, confidence score passed.")
                : QStringLiteral("OCR completed, confidence score is too low.");
        return decision;
    }

    if (decision.mode == QStringLiteral("baseline_text")) {
        if (halconConfig.expectedText.trimmed().isEmpty()) {
            decision.ok = false;
            decision.status = QStringLiteral("baseline_text_empty");
            decision.message = QStringLiteral("Baseline text is empty.");
            qDebug() << "[OCR Debug] baseline_text_empty"
                     << "filteredText=" << ocrResult.filteredText
                     << "Please enter expectedText/baselineText explicitly.";
            return decision;
        }

        decision.ok = ocrResult.filteredText == halconConfig.expectedText;
        decision.status = decision.ok ? QStringLiteral("ok") : QStringLiteral("text_mismatch");
        decision.message = decision.ok
                ? QStringLiteral("OCR completed, baseline text matched.")
                : QStringLiteral("OCR completed, baseline text mismatched.");
        return decision;
    }

    decision.ok = false;
    decision.status = QStringLiteral("ng");
    decision.message = QStringLiteral("Unknown OCR judge mode.");
    return decision;
}

void logOcrDebug(const ToolResult &result, const OcrHalconConfig &halconConfig)
{
    const QJsonObject payload = result.payload;
    const QString polarity = payload.value(QStringLiteral("usedPolarity"))
            .toString(halconConfig.objectPolarity);
    qDebug().noquote()
            << QStringLiteral("[OCR Debug] roi=%1 threshold=%2 polarity=%3 minConfidence=%4 rawText=\"%5\" filteredText=\"%6\" rawCount=%7 filteredCount=%8 rawAvg=%9 filteredAvg=%10 judgeMode=%11 expectedText=\"%12\" minCount=%13 maxCount=%14 judgeMinScore=%15 status=%16 ok=%17")
               .arg(rectToDebugString(halconConfig.roiNormalized))
               .arg(halconConfig.binaryThreshold)
               .arg(polarity)
               .arg(QString::number(halconConfig.minConfidence, 'f', 3))
               .arg(payload.value(QStringLiteral("rawText")).toString())
               .arg(payload.value(QStringLiteral("filteredText")).toString())
               .arg(payload.value(QStringLiteral("rawCharCount")).toInt())
               .arg(payload.value(QStringLiteral("filteredCharCount")).toInt())
               .arg(QString::number(payload.value(QStringLiteral("rawAverageConfidence")).toDouble(), 'f', 3))
               .arg(QString::number(payload.value(QStringLiteral("filteredAverageConfidence")).toDouble(), 'f', 3))
               .arg(payload.value(QStringLiteral("judgeMode")).toString())
               .arg(payload.value(QStringLiteral("expectedText")).toString())
               .arg(payload.value(QStringLiteral("judgeMinCount")).toInt())
               .arg(payload.value(QStringLiteral("judgeMaxCount")).toInt())
               .arg(QString::number(payload.value(QStringLiteral("judgeMinScore")).toDouble(), 'f', 3))
               .arg(result.status)
               .arg(result.ok ? QStringLiteral("true") : QStringLiteral("false"));
}

} // namespace

bool OcrAdapter::supports(ToolType type) const
{
    return type == ToolType::Ocr;
}

ToolResult OcrAdapter::run(const ToolRequest &request)
{
    const ToolConfig &config = request.config;
    if (config.toolType != ToolType::Ocr)
        return makeOcrError(config,
                            QStringLiteral("invalid_tool_type"),
                            QStringLiteral("OcrAdapter only supports ToolType::Ocr."));

    if (request.image.empty())
        return makeOcrError(config,
                            QStringLiteral("image_empty"),
                            QStringLiteral("OCR input image is empty."));

    const OcrHalconConfig halconConfig = toHalconConfig(config);
    if (halconConfig.halconSoPath.isEmpty())
        return makePathError(config,
                             QStringLiteral("halcon_so_not_found"),
                             QStringLiteral("HALCON runtime file not found. Tried: %1")
                             .arg(HalconRuntimePaths::formatTriedPaths(halconConfig.halconSoPathCandidates)),
                             QStringLiteral("halconSoPathCandidates"),
                             halconConfig.halconSoPathCandidates);

    if (halconConfig.ocrModelPath.isEmpty())
        return makePathError(config,
                             QStringLiteral("model_not_found"),
                             QStringLiteral("OCR model file not found. Tried: %1")
                             .arg(HalconRuntimePaths::formatTriedPaths(halconConfig.ocrModelPathCandidates)),
                             QStringLiteral("ocrModelPathCandidates"),
                             halconConfig.ocrModelPathCandidates);

    const OcrHalconResult ocrResult = m_runner.run(request.image, halconConfig);

    ToolResult result;
    result.toolId = config.toolId;
    result.toolType = ToolType::Ocr;
    result.success = ocrResult.success;
    result.ok = false;
    result.status = ocrResult.status;
    result.message = ocrResult.message;
    result.text = ocrResult.filteredText;
    result.score = ocrResult.filteredAverageConfidence;
    result.value = ocrResult.filteredAverageConfidence;
    result.count = ocrResult.filteredCharCount;
    result.overlays = ocrResult.overlays;
    result.payload = ocrResult.payload;
    result.payload.insert(QStringLiteral("text"), result.text);
    result.payload.insert(QStringLiteral("charCount"), result.count);
    result.payload.insert(QStringLiteral("averageConfidence"), result.score);
    result.payload.insert(QStringLiteral("rawText"), ocrResult.rawText);
    result.payload.insert(QStringLiteral("filteredText"), ocrResult.filteredText);
    result.payload.insert(QStringLiteral("rawCharCount"), ocrResult.rawCharCount);
    result.payload.insert(QStringLiteral("filteredCharCount"), ocrResult.filteredCharCount);
    result.payload.insert(QStringLiteral("rawAverageConfidence"), ocrResult.rawAverageConfidence);
    result.payload.insert(QStringLiteral("filteredAverageConfidence"), ocrResult.filteredAverageConfidence);
    result.payload.insert(QStringLiteral("rawChars"), charsToJson(ocrResult.rawChars));
    result.payload.insert(QStringLiteral("filteredChars"), charsToJson(ocrResult.filteredChars));
    result.payload.insert(QStringLiteral("minConfidence"), halconConfig.minConfidence);
    result.payload.insert(QStringLiteral("binaryThreshold"), halconConfig.binaryThreshold);
    result.payload.insert(QStringLiteral("polarity"), halconConfig.objectPolarity);
    result.payload.insert(QStringLiteral("minCharArea"), halconConfig.minCharArea);
    result.payload.insert(QStringLiteral("maxCharArea"), halconConfig.maxCharArea);
    result.payload.insert(QStringLiteral("minCharWidth"), halconConfig.minCharWidth);
    result.payload.insert(QStringLiteral("minCharHeight"), halconConfig.minCharHeight);
    result.payload.insert(QStringLiteral("maxCharWidth"), halconConfig.maxCharWidth);
    result.payload.insert(QStringLiteral("maxCharHeight"), halconConfig.maxCharHeight);
    result.payload.insert(QStringLiteral("minAspectRatio"), halconConfig.minAspectRatio);
    result.payload.insert(QStringLiteral("maxAspectRatio"), halconConfig.maxAspectRatio);
    result.payload.insert(QStringLiteral("expectedText"), halconConfig.expectedText);
    result.payload.insert(QStringLiteral("matchRule"), halconConfig.matchRule);
    result.payload.insert(QStringLiteral("ocrModelPath"), halconConfig.ocrModelPath);
    result.payload.insert(QStringLiteral("roi"), rectToJson(halconConfig.roiNormalized));
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(halconConfig.roiNormalized));
    const QJsonObject judgeRule = config.judgeRule;
    const QString judgeMode = judgeRule.value(QStringLiteral("mode")).toString(QStringLiteral("none")).trimmed().toLower();
    int judgeMinCount = firstInt(judgeRule, {QStringLiteral("minCount"), QStringLiteral("min_count")}, 0);
    int judgeMaxCount = firstInt(judgeRule,
                                 {QStringLiteral("maxCount"), QStringLiteral("max_count")},
                                 0);
    if (judgeMaxCount < judgeMinCount)
        std::swap(judgeMinCount, judgeMaxCount);
    const double judgeMinScore = normalizeScore(firstDouble(judgeRule,
                                                           {QStringLiteral("minScore"),
                                                            QStringLiteral("minConfidence"),
                                                            QStringLiteral("min_confidence")},
                                                           halconConfig.minConfidence));
    result.payload.insert(QStringLiteral("judgeMode"), judgeMode);
    result.payload.insert(QStringLiteral("judgeMinCount"), judgeMinCount);
    result.payload.insert(QStringLiteral("judgeMaxCount"), judgeMaxCount);
    result.payload.insert(QStringLiteral("judgeMinScore"), judgeMinScore);

    if (!ocrResult.success) {
        result.ok = false;
        if (result.status.isEmpty())
            result.status = QStringLiteral("ocr_failed");
        logOcrDebug(result, halconConfig);
        return result;
    }

    if (result.count <= 0) {
        result.ok = false;
        result.status = QStringLiteral("no_characters");
        result.text.clear();
        result.score = 0.0;
        result.value = 0.0;
        result.payload.insert(QStringLiteral("text"), result.text);
        result.payload.insert(QStringLiteral("charCount"), result.count);
        result.payload.insert(QStringLiteral("averageConfidence"), result.score);
        result.payload.insert(QStringLiteral("filteredText"), result.text);
        result.payload.insert(QStringLiteral("filteredCharCount"), result.count);
        result.payload.insert(QStringLiteral("filteredAverageConfidence"), result.score);
        logOcrDebug(result, halconConfig);
        return result;
    }

    const OcrJudgeDecision decision = judgeOcrResult(ocrResult, config, halconConfig);
    result.ok = decision.ok;
    result.status = decision.status;
    result.message = decision.message;
    result.payload.insert(QStringLiteral("judgeMode"), decision.mode);
    result.payload.insert(QStringLiteral("judgeMinCount"), decision.minCount);
    result.payload.insert(QStringLiteral("judgeMaxCount"), decision.maxCount);
    result.payload.insert(QStringLiteral("judgeMinScore"), decision.minScore);

    logOcrDebug(result, halconConfig);
    return result;
}
