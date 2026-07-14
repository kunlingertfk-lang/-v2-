#ifndef ALGORITHMS_OCR_OCRHALCONRUNNER_H
#define ALGORITHMS_OCR_OCRHALCONRUNNER_H

#include "toolcore/ToolOverlay.h"

#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <opencv2/core.hpp>

struct OcrHalconConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString ocrModelPath;
    QStringList ocrModelPathCandidates;

    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);

    int binaryThreshold = 128;
    QString objectPolarity = QStringLiteral("dark");

    double minCharArea = 20.0;
    double maxCharArea = 100000.0;
    double minCharWidth = 0.0;
    double minCharHeight = 0.0;
    double maxCharWidth = 99999.0;
    double maxCharHeight = 99999.0;
    double minAspectRatio = 0.10;
    double maxAspectRatio = 10.0;

    double minConfidence = 0.70;

    QString expectedText;
    QString matchRule = QStringLiteral("none");
};

struct OcrCharResult
{
    QString character;
    double confidence = 0.0;
    QRectF box;
};

struct OcrHalconResult
{
    bool success = false;
    QString status;
    QString message;

    QString text;
    double averageConfidence = 0.0;
    int charCount = 0;

    QVector<OcrCharResult> chars;
    QString rawText;
    QString filteredText;
    int rawCharCount = 0;
    int filteredCharCount = 0;
    double rawAverageConfidence = 0.0;
    double filteredAverageConfidence = 0.0;
    QVector<OcrCharResult> rawChars;
    QVector<OcrCharResult> filteredChars;
    QVector<ToolOverlay> overlays;
    QJsonObject payload;
};

class OcrHalconRunner
{
public:
    OcrHalconResult run(const cv::Mat &image, const OcrHalconConfig &config);
};

#endif // ALGORITHMS_OCR_OCRHALCONRUNNER_H
