#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <opencv2/core/mat.hpp>

enum class ColorComparisonModelState { Empty, Stale, Ready, Invalid, Unsupported };

struct ColorComparisonInputSignature {
    QString colorMode = QStringLiteral("unknown");
    QString pixelFormat;
    int bitDepth = -1;
    QJsonValue whiteBalance = QJsonValue::Null;
    QJsonValue ccm = QJsonValue::Null;
    QJsonValue exposure = QJsonValue::Null;
    QJsonValue gain = QJsonValue::Null;
};

struct ColorComparisonBrightnessReference {
    double mean = 0.0;
    double deviation = 0.0;
};

struct ColorComparisonModelV2 {
    ColorComparisonModelState state = ColorComparisonModelState::Empty;
    QString featureType = QStringLiteral("histogram_hs_2d");
    QString algorithm = QStringLiteral("histogram_intersection");
    QString colorSpace = QStringLiteral("hsv");
    int hueBins = 32;
    int saturationBins = 32;
    QString layout = QStringLiteral("hue_major");
    bool normalized = true;
    QVector<double> values;
    QVector<double> valueHistogram;
    qint64 effectivePixelCount = 0;
    QString referenceImageHash;
    QString extractParamsHash;
    ColorComparisonInputSignature inputSignature;
    ColorComparisonBrightnessReference brightnessReference;
};

struct ColorComparisonModelValidation {
    bool success = false;
    QString status;
    QString message;
};

struct ColorComparisonModelReadResult {
    bool success = false;
    bool requiresRebuild = false;
    QString status;
    QString message;
    ColorComparisonModelV2 model;
};

QString colorComparisonModelStateName(ColorComparisonModelState state);
QJsonObject colorComparisonModelToJson(const ColorComparisonModelV2 &model);
ColorComparisonModelReadResult readColorComparisonModel(
        const QJsonObject &colorComparison, bool referenceAvailable);
ColorComparisonModelValidation validateColorComparisonModel(
        const ColorComparisonModelV2 &model,
        const QString &expectedExtractParamsHash = QString());
QString colorComparisonReferenceHash(const cv::Mat &image);
QString colorComparisonExtractParamsHash(const QJsonObject &extractParams);
