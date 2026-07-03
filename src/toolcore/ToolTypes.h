#ifndef TOOLCORE_TOOLTYPES_H
#define TOOLCORE_TOOLTYPES_H

#include <QString>

enum class ToolCategory {
    Measure,
    Count,
    Recognition,
    Presence,
    Logic,
    Location,
    DeepLearning,
    Defect,
    Unknown
};

enum class ToolType {
    Unknown,

    // Measure
    GrayArea,
    BrightnessAverage,
    ContrastMeasure,
    WidthMeasure,
    DiameterMeasure,
    ColorMeasure,
    ColorArea,
    PointPointMeasure,
    PointLineMeasure,
    LineLineAngle,
    StraightLineAngle,

    // Count
    PatternCount,
    AreaCount,
    EdgeCount,

    // Recognition
    Ocr,
    CodeReader,
    CategoryRecognition,
    ColorRecognition,
    ColorComparison,
    RegisteredClassification,

    // Presence
    PatternPresence,
    BlobPresence,
    EdgePresence,
    ColorPresence,
    LinePresence,
    CirclePresence,
    ContourPresence,

    // Location
    TemplateLocation,
    EdgeLocation,
    CircleLocation,

    // Deep learning
    AiClassification,
    AiDetection,
    AiSegmentation,

    // Defect
    ScratchDefect,
    StainDefect,
    MissingDefect,

    // Logic
    Judge,
    ConditionBranch,
    VariableCalculation,
    OutputLogic
};

inline QString toolCategoryToString(ToolCategory category)
{
    switch (category) {
    case ToolCategory::Measure:
        return QStringLiteral("Measure");
    case ToolCategory::Count:
        return QStringLiteral("Count");
    case ToolCategory::Recognition:
        return QStringLiteral("Recognition");
    case ToolCategory::Presence:
        return QStringLiteral("Presence");
    case ToolCategory::Logic:
        return QStringLiteral("Logic");
    case ToolCategory::Location:
        return QStringLiteral("Location");
    case ToolCategory::DeepLearning:
        return QStringLiteral("DeepLearning");
    case ToolCategory::Defect:
        return QStringLiteral("Defect");
    case ToolCategory::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

inline ToolCategory toolCategoryFromString(const QString &value)
{
    const QString key = value.trimmed().toLower();
    if (key == QStringLiteral("measure"))
        return ToolCategory::Measure;
    if (key == QStringLiteral("count"))
        return ToolCategory::Count;
    if (key == QStringLiteral("recognition"))
        return ToolCategory::Recognition;
    if (key == QStringLiteral("presence"))
        return ToolCategory::Presence;
    if (key == QStringLiteral("logic"))
        return ToolCategory::Logic;
    if (key == QStringLiteral("location"))
        return ToolCategory::Location;
    if (key == QStringLiteral("deeplearning") || key == QStringLiteral("deep_learning"))
        return ToolCategory::DeepLearning;
    if (key == QStringLiteral("defect"))
        return ToolCategory::Defect;
    return ToolCategory::Unknown;
}

inline QString toolTypeToString(ToolType type)
{
    switch (type) {
    case ToolType::GrayArea:
        return QStringLiteral("GrayArea");
    case ToolType::BrightnessAverage:
        return QStringLiteral("BrightnessAverage");
    case ToolType::ContrastMeasure:
        return QStringLiteral("ContrastMeasure");
    case ToolType::WidthMeasure:
        return QStringLiteral("WidthMeasure");
    case ToolType::DiameterMeasure:
        return QStringLiteral("DiameterMeasure");
    case ToolType::ColorMeasure:
        return QStringLiteral("ColorMeasure");
    case ToolType::ColorArea:
        return QStringLiteral("ColorArea");
    case ToolType::PointPointMeasure:
        return QStringLiteral("PointPointMeasure");
    case ToolType::PointLineMeasure:
        return QStringLiteral("PointLineMeasure");
    case ToolType::LineLineAngle:
        return QStringLiteral("LineLineAngle");
    case ToolType::StraightLineAngle:
        return QStringLiteral("StraightLineAngle");
    case ToolType::PatternCount:
        return QStringLiteral("PatternCount");
    case ToolType::AreaCount:
        return QStringLiteral("AreaCount");
    case ToolType::EdgeCount:
        return QStringLiteral("EdgeCount");
    case ToolType::Ocr:
        return QStringLiteral("Ocr");
    case ToolType::CodeReader:
        return QStringLiteral("CodeReader");
    case ToolType::CategoryRecognition:
        return QStringLiteral("CategoryRecognition");
    case ToolType::ColorRecognition:
        return QStringLiteral("ColorRecognition");
    case ToolType::ColorComparison:
        return QStringLiteral("ColorComparison");
    case ToolType::RegisteredClassification:
        return QStringLiteral("RegisteredClassification");
    case ToolType::PatternPresence:
        return QStringLiteral("PatternPresence");
    case ToolType::BlobPresence:
        return QStringLiteral("BlobPresence");
    case ToolType::EdgePresence:
        return QStringLiteral("EdgePresence");
    case ToolType::ColorPresence:
        return QStringLiteral("ColorPresence");
    case ToolType::LinePresence:
        return QStringLiteral("LinePresence");
    case ToolType::CirclePresence:
        return QStringLiteral("CirclePresence");
    case ToolType::ContourPresence:
        return QStringLiteral("ContourPresence");
    case ToolType::TemplateLocation:
        return QStringLiteral("TemplateLocation");
    case ToolType::EdgeLocation:
        return QStringLiteral("EdgeLocation");
    case ToolType::CircleLocation:
        return QStringLiteral("CircleLocation");
    case ToolType::AiClassification:
        return QStringLiteral("AiClassification");
    case ToolType::AiDetection:
        return QStringLiteral("AiDetection");
    case ToolType::AiSegmentation:
        return QStringLiteral("AiSegmentation");
    case ToolType::ScratchDefect:
        return QStringLiteral("ScratchDefect");
    case ToolType::StainDefect:
        return QStringLiteral("StainDefect");
    case ToolType::MissingDefect:
        return QStringLiteral("MissingDefect");
    case ToolType::Judge:
        return QStringLiteral("Judge");
    case ToolType::ConditionBranch:
        return QStringLiteral("ConditionBranch");
    case ToolType::VariableCalculation:
        return QStringLiteral("VariableCalculation");
    case ToolType::OutputLogic:
        return QStringLiteral("OutputLogic");
    case ToolType::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}

inline ToolType toolTypeFromString(const QString &value)
{
    const QString key = value.trimmed().toLower();
    if (key == QStringLiteral("grayarea"))
        return ToolType::GrayArea;
    if (key == QStringLiteral("brightnessaverage"))
        return ToolType::BrightnessAverage;
    if (key == QStringLiteral("contrastmeasure"))
        return ToolType::ContrastMeasure;
    if (key == QStringLiteral("widthmeasure"))
        return ToolType::WidthMeasure;
    if (key == QStringLiteral("diametermeasure"))
        return ToolType::DiameterMeasure;
    if (key == QStringLiteral("colormeasure"))
        return ToolType::ColorMeasure;
    if (key == QStringLiteral("colorarea"))
        return ToolType::ColorArea;
    if (key == QStringLiteral("pointpointmeasure"))
        return ToolType::PointPointMeasure;
    if (key == QStringLiteral("pointlinemeasure"))
        return ToolType::PointLineMeasure;
    if (key == QStringLiteral("linelineangle"))
        return ToolType::LineLineAngle;
    if (key == QStringLiteral("straightlineangle"))
        return ToolType::StraightLineAngle;
    if (key == QStringLiteral("patterncount"))
        return ToolType::PatternCount;
    if (key == QStringLiteral("areacount"))
        return ToolType::AreaCount;
    if (key == QStringLiteral("edgecount"))
        return ToolType::EdgeCount;
    if (key == QStringLiteral("ocr"))
        return ToolType::Ocr;
    if (key == QStringLiteral("codereader"))
        return ToolType::CodeReader;
    if (key == QStringLiteral("categoryrecognition"))
        return ToolType::CategoryRecognition;
    if (key == QStringLiteral("colorrecognition") ||
        key == QStringLiteral("color_recognition") ||
        key.contains(QStringLiteral("颜色识别")))
        return ToolType::ColorRecognition;
    if (key == QStringLiteral("colorcomparison") ||
        key == QStringLiteral("color_comparison") ||
        key.contains(QStringLiteral("颜色比较")))
        return ToolType::ColorComparison;
    if (key == QStringLiteral("registeredclassification") ||
        key == QStringLiteral("registered_classification") ||
        key == QStringLiteral("registrationclass") ||
        key.contains(QStringLiteral("注册分类")))
        return ToolType::RegisteredClassification;
    if (key == QStringLiteral("patternpresence"))
        return ToolType::PatternPresence;
    if (key == QStringLiteral("blobpresence") ||
        key == QStringLiteral("blob_presence") ||
        key == QStringLiteral("spotpresence") ||
        key == QStringLiteral("spot_presence"))
        return ToolType::BlobPresence;
    if (key == QStringLiteral("edgepresence"))
        return ToolType::EdgePresence;
    if (key == QStringLiteral("colorpresence"))
        return ToolType::ColorPresence;
    if (key == QStringLiteral("linepresence"))
        return ToolType::LinePresence;
    if (key == QStringLiteral("circlepresence") ||
        key == QStringLiteral("circle_presence") ||
        key.contains(QStringLiteral("圆有无")))
        return ToolType::CirclePresence;
    if (key == QStringLiteral("contourpresence"))
        return ToolType::ContourPresence;
    if (key == QStringLiteral("templatelocation"))
        return ToolType::TemplateLocation;
    if (key == QStringLiteral("edgelocation"))
        return ToolType::EdgeLocation;
    if (key == QStringLiteral("circlelocation"))
        return ToolType::CircleLocation;
    if (key == QStringLiteral("aiclassification"))
        return ToolType::AiClassification;
    if (key == QStringLiteral("aidetection"))
        return ToolType::AiDetection;
    if (key == QStringLiteral("aisegmentation"))
        return ToolType::AiSegmentation;
    if (key == QStringLiteral("scratchdefect"))
        return ToolType::ScratchDefect;
    if (key == QStringLiteral("staindefect"))
        return ToolType::StainDefect;
    if (key == QStringLiteral("missingdefect"))
        return ToolType::MissingDefect;
    if (key == QStringLiteral("judge"))
        return ToolType::Judge;
    if (key == QStringLiteral("conditionbranch"))
        return ToolType::ConditionBranch;
    if (key == QStringLiteral("variablecalculation"))
        return ToolType::VariableCalculation;
    if (key == QStringLiteral("outputlogic"))
        return ToolType::OutputLogic;
    return ToolType::Unknown;
}

#endif // TOOLCORE_TOOLTYPES_H
