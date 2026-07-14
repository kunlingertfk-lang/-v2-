#ifndef ALGORITHMS_PRESENCE_PATTERNPRESENCEAUTOMODELDOMAIN_H
#define ALGORITHMS_PRESENCE_PATTERNPRESENCEAUTOMODELDOMAIN_H

#include "algorithms/presence/PatternPresenceHalconApi.h"

#include <QString>
#include <QVector>

struct PatternPresenceAutoModelDomainThresholdStats
{
    int thresholdHigh = 0;
    double thresholdRegionArea = 0.0;
    int connectedCount = 0;
    QVector<double> rawAreas;
    int areaCandidateCount = 0;
    double selectedArea = 0.0;
    double selectedAreaRatio = 0.0;
    QString rejectedReason;
};

struct PatternPresenceAutoModelDomainParams
{
    QString version = QStringLiteral("auto_model_domain_v2_multi_threshold_area_morphology");
    int thresholdHigh = 135;
    QVector<int> thresholdHighCandidates{100, 115, 130, 145, 160, 175};
    bool polygonTemplate = false;
    double minAreaRatio = 0.005;
    double maxAreaRatio = 0.85;
    double rejectMinAreaRatio = 0.005;
    double rejectMaxAreaRatio = 0.85;
    double polygonRejectMaxAreaRatio = 0.97;
    double borderMargin = 5.0;
    double openingRadius = 1.5;
    double closingRadius = 4.5;
    double dilationRadius = 5.5;
};

struct PatternPresenceAutoModelDomainResult
{
    bool applied = false;
    QString fallbackReason;
    QVector<int> thresholdTriedValues;
    int selectedThresholdHigh = 0;
    double thresholdRegionArea = 0.0;
    int connectedRegionCount = 0;
    int rawCandidateCount = 0;
    int areaCandidateCount = 0;
    double selectedCandidateArea = 0.0;
    int selectedCandidateIndex = -1;
    double selectedCandidateAreaRatio = 0.0;
    QString failureStage;
    QString failureReason;
    QVector<PatternPresenceAutoModelDomainThresholdStats> perThresholdStats;
    QString warning;
    QString route = QStringLiteral("threshold_connection_select_shape_fill_open_close_dilate");
    Hobject modelDomain = NO_OBJECTS;
    Hobject reducedImageForModel = NO_OBJECTS;
    Hobject targetRegion = NO_OBJECTS;
    Hobject displayContour = NO_OBJECTS;
    Hobject debugThresholdRegion = NO_OBJECTS;
    Hobject debugConnectedRegions = NO_OBJECTS;
    Hobject debugSelectedCandidateRegion = NO_OBJECTS;
    double targetArea = 0.0;
    double userRoiArea = 0.0;
    double targetAreaRatio = 0.0;
    double targetRow = 0.0;
    double targetColumn = 0.0;
    int candidateCount = 0;
    int noBorderCandidateCount = 0;
    int thresholdHigh = 135;
    double minAreaRatio = 0.01;
    double maxAreaRatio = 0.75;
    double openingRadius = 1.5;
    double closingRadius = 4.5;
    double dilationRadius = 5.5;
};

PatternPresenceAutoModelDomainParams defaultPatternPresenceAutoModelDomainParams();

void clearPatternPresenceAutoModelDomainResult(PatternPresenceHalconApi *api,
                                               PatternPresenceAutoModelDomainResult *result);

bool tryCreatePatternPresenceAutoModelDomain(PatternPresenceHalconApi *api,
                                             const Hobject graySource,
                                             const Hobject templateModelSource,
                                             double userRoiArea,
                                             const PatternPresenceAutoModelDomainParams &params,
                                             PatternPresenceAutoModelDomainResult *result);

#endif // ALGORITHMS_PRESENCE_PATTERNPRESENCEAUTOMODELDOMAIN_H
