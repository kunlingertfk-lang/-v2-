#include "algorithms/presence/PatternPresenceAutoModelDomain.h"

#include <QtGlobal>

namespace {

void clearObject(PatternPresenceHalconApi *api, Hobject &object)
{
    if (api && api->clearObj && patternPresenceHalconObjectAllocated(object))
        api->clearObj(object);
    object = NO_OBJECTS;
}

void destroyTuple(PatternPresenceHalconApi *api, Htuple &tuple)
{
    if (api && api->destroyTuple && (tuple.num > 0 || tuple.capacity > 0))
        api->destroyTuple(&tuple);
    tuple = HTUPLE_INITIALIZER;
}

} // namespace

PatternPresenceAutoModelDomainParams defaultPatternPresenceAutoModelDomainParams()
{
    return PatternPresenceAutoModelDomainParams();
}

void clearPatternPresenceAutoModelDomainResult(PatternPresenceHalconApi *api,
                                               PatternPresenceAutoModelDomainResult *result)
{
    if (!result)
        return;

    clearObject(api, result->debugSelectedCandidateRegion);
    clearObject(api, result->debugConnectedRegions);
    clearObject(api, result->debugThresholdRegion);
    clearObject(api, result->displayContour);
    clearObject(api, result->targetRegion);
    clearObject(api, result->reducedImageForModel);
    clearObject(api, result->modelDomain);
    result->applied = false;
}

bool tryCreatePatternPresenceAutoModelDomain(PatternPresenceHalconApi *api,
                                             const Hobject graySource,
                                             const Hobject templateModelSource,
                                             const double userRoiArea,
                                             const PatternPresenceAutoModelDomainParams &params,
                                             PatternPresenceAutoModelDomainResult *result)
{
    if (!result)
        return false;

    clearPatternPresenceAutoModelDomainResult(api, result);
    result->route = params.version;
    result->thresholdHigh = params.thresholdHigh;
    result->minAreaRatio = params.minAreaRatio;
    const double effectiveMaxAreaRatio = params.polygonTemplate
            ? params.polygonRejectMaxAreaRatio
            : params.rejectMaxAreaRatio;
    result->maxAreaRatio = effectiveMaxAreaRatio;
    result->openingRadius = params.openingRadius;
    result->closingRadius = params.closingRadius;
    result->dilationRadius = params.dilationRadius;
    result->userRoiArea = userRoiArea;
    result->fallbackReason.clear();
    result->thresholdTriedValues.clear();
    result->selectedThresholdHigh = 0;
    result->thresholdRegionArea = 0.0;
    result->connectedRegionCount = 0;
    result->rawCandidateCount = 0;
    result->areaCandidateCount = 0;
    result->selectedCandidateArea = 0.0;
    result->selectedCandidateIndex = -1;
    result->selectedCandidateAreaRatio = 0.0;
    result->failureStage.clear();
    result->failureReason.clear();
    result->perThresholdStats.clear();
    result->warning.clear();
    result->targetArea = 0.0;
    result->targetAreaRatio = 0.0;
    result->targetRow = 0.0;
    result->targetColumn = 0.0;
    result->candidateCount = 0;
    result->noBorderCandidateCount = 0;

    auto fail = [&](const QString &stage, const QString &reason) {
        result->fallbackReason = reason;
        result->failureStage = stage;
        result->failureReason = reason;
        result->applied = false;
        return false;
    };

    if (!api || !api->hasAutoModelDomainOperators())
        return fail(QStringLiteral("operator_check"),
                    QStringLiteral("auto_model_domain_operator_unavailable"));
    if (!patternPresenceHalconObjectAllocated(graySource) ||
        !patternPresenceHalconObjectAllocated(templateModelSource)) {
        return fail(QStringLiteral("input_check"),
                    QStringLiteral("invalid_template_model_source"));
    }
    if (userRoiArea <= 0.0)
        return fail(QStringLiteral("input_check"),
                    QStringLiteral("invalid_user_roi_area"));

    auto callOk = [](const Herror status) {
        return patternPresenceHalconStatusOk(status);
    };

    QVector<int> thresholds = params.thresholdHighCandidates;
    if (thresholds.isEmpty())
        thresholds.append(params.thresholdHigh);
    for (const int high : thresholds)
        result->thresholdTriedValues.append(high);

    const double minRatio = params.rejectMinAreaRatio;
    const double maxRatio = effectiveMaxAreaRatio;
    const double minArea = qMax(1.0, userRoiArea * minRatio);
    const double maxArea = qMax(minArea, userRoiArea * maxRatio);

    bool sawThresholdRegion = false;
    bool sawConnectedRegion = false;
    bool sawAreaFilterNoCandidate = false;
    bool sawPositiveCandidate = false;
    bool sawCandidateAreaUnreadable = false;
    bool sawTargetTooSmall = false;
    bool sawTargetTooLarge = false;
    bool sawReduceFailed = false;
    bool sawDisplayFailed = false;

    PatternPresenceAutoModelDomainThresholdStats bestRejectedStats;
    bool hasBestRejected = false;
    double bestRejectedTargetArea = -1.0;
    QString bestRejectedStage;
    QString bestRejectedReason;
    int bestRejectedCandidateIndex = -1;
    double bestRejectedTargetAreaRatio = 0.0;
    double bestRejectedTargetRow = 0.0;
    double bestRejectedTargetColumn = 0.0;

    Hobject bestRejectedThresholdRegion = NO_OBJECTS;
    Hobject bestRejectedConnectedRegions = NO_OBJECTS;
    Hobject bestRejectedSelectedCandidate = NO_OBJECTS;
    Hobject bestRejectedClosedRegion = NO_OBJECTS;
    Hobject bestRejectedDisplayContour = NO_OBJECTS;

    auto clearBestRejectedObjects = [&]() {
        clearObject(api, bestRejectedDisplayContour);
        clearObject(api, bestRejectedClosedRegion);
        clearObject(api, bestRejectedSelectedCandidate);
        clearObject(api, bestRejectedConnectedRegions);
        clearObject(api, bestRejectedThresholdRegion);
    };

    auto rememberRejected = [&](PatternPresenceAutoModelDomainThresholdStats stats,
                                const QString &stage,
                                const QString &reason,
                                const int candidateIndex,
                                const double targetArea,
                                const double targetAreaRatio,
                                const double targetRow,
                                const double targetColumn,
                                Hobject &thresholdRegion,
                                Hobject &connectedRegions,
                                Hobject &selectedCandidate,
                                Hobject &closedRegion,
                                Hobject &displayContour) {
        if (hasBestRejected && targetArea < bestRejectedTargetArea)
            return;

        clearBestRejectedObjects();
        hasBestRejected = true;
        bestRejectedStats = stats;
        bestRejectedStats.rejectedReason = reason;
        bestRejectedStage = stage;
        bestRejectedReason = reason;
        bestRejectedCandidateIndex = candidateIndex;
        bestRejectedTargetArea = targetArea;
        bestRejectedTargetAreaRatio = targetAreaRatio;
        bestRejectedTargetRow = targetRow;
        bestRejectedTargetColumn = targetColumn;
        bestRejectedThresholdRegion = thresholdRegion;
        bestRejectedConnectedRegions = connectedRegions;
        bestRejectedSelectedCandidate = selectedCandidate;
        bestRejectedClosedRegion = closedRegion;
        bestRejectedDisplayContour = displayContour;
        thresholdRegion = NO_OBJECTS;
        connectedRegions = NO_OBJECTS;
        selectedCandidate = NO_OBJECTS;
        closedRegion = NO_OBJECTS;
        displayContour = NO_OBJECTS;
    };

    bool hasBestValid = false;
    PatternPresenceAutoModelDomainThresholdStats bestValidStats;
    int bestValidCandidateIndex = -1;
    double bestValidTargetArea = 0.0;
    double bestValidTargetAreaRatio = 0.0;
    double bestValidTargetRow = 0.0;
    double bestValidTargetColumn = 0.0;
    Hobject bestThresholdRegion = NO_OBJECTS;
    Hobject bestConnectedRegions = NO_OBJECTS;
    Hobject bestSelectedCandidate = NO_OBJECTS;
    Hobject bestClosedRegion = NO_OBJECTS;
    Hobject bestModelDomain = NO_OBJECTS;
    Hobject bestReducedImage = NO_OBJECTS;
    Hobject bestDisplayContour = NO_OBJECTS;

    auto clearBestValidObjects = [&]() {
        clearObject(api, bestDisplayContour);
        clearObject(api, bestReducedImage);
        clearObject(api, bestModelDomain);
        clearObject(api, bestClosedRegion);
        clearObject(api, bestSelectedCandidate);
        clearObject(api, bestConnectedRegions);
        clearObject(api, bestThresholdRegion);
    };

    for (const int thresholdHigh : thresholds) {
        PatternPresenceAutoModelDomainThresholdStats stats;
        stats.thresholdHigh = thresholdHigh;

        Hobject thresholdRegion = NO_OBJECTS;
        Hobject connectedRegions = NO_OBJECTS;
        Hobject areaSelectedRegions = NO_OBJECTS;
        Hobject selectedCandidate = NO_OBJECTS;
        Hobject fillRegion = NO_OBJECTS;
        Hobject openedRegion = NO_OBJECTS;
        Hobject closedRegion = NO_OBJECTS;
        Hobject modelDomain = NO_OBJECTS;
        Hobject reducedImage = NO_OBJECTS;
        Hobject displayContour = NO_OBJECTS;
        Htuple thresholdAreaTuple = HTUPLE_INITIALIZER;
        Htuple thresholdRowTuple = HTUPLE_INITIALIZER;
        Htuple thresholdColumnTuple = HTUPLE_INITIALIZER;
        Htuple rawAreaTuple = HTUPLE_INITIALIZER;
        Htuple rawRowTuple = HTUPLE_INITIALIZER;
        Htuple rawColumnTuple = HTUPLE_INITIALIZER;
        Htuple areaTuple = HTUPLE_INITIALIZER;
        Htuple rowTuple = HTUPLE_INITIALIZER;
        Htuple columnTuple = HTUPLE_INITIALIZER;
        Htuple targetAreaTuple = HTUPLE_INITIALIZER;
        Htuple targetRowTuple = HTUPLE_INITIALIZER;
        Htuple targetColumnTuple = HTUPLE_INITIALIZER;

        auto cleanupThreshold = [&]() {
            destroyTuple(api, targetColumnTuple);
            destroyTuple(api, targetRowTuple);
            destroyTuple(api, targetAreaTuple);
            destroyTuple(api, columnTuple);
            destroyTuple(api, rowTuple);
            destroyTuple(api, areaTuple);
            destroyTuple(api, rawColumnTuple);
            destroyTuple(api, rawRowTuple);
            destroyTuple(api, rawAreaTuple);
            destroyTuple(api, thresholdColumnTuple);
            destroyTuple(api, thresholdRowTuple);
            destroyTuple(api, thresholdAreaTuple);
            clearObject(api, displayContour);
            clearObject(api, reducedImage);
            clearObject(api, modelDomain);
            clearObject(api, closedRegion);
            clearObject(api, openedRegion);
            clearObject(api, fillRegion);
            clearObject(api, selectedCandidate);
            clearObject(api, areaSelectedRegions);
            clearObject(api, connectedRegions);
            clearObject(api, thresholdRegion);
        };

        auto rejectThreshold = [&](const QString &reason) {
            stats.rejectedReason = reason;
            result->perThresholdStats.append(stats);
            cleanupThreshold();
        };

        if (!callOk(api->threshold(templateModelSource,
                                   &thresholdRegion,
                                   0.0,
                                   static_cast<double>(thresholdHigh)))) {
            rejectThreshold(QStringLiteral("threshold_no_region"));
            continue;
        }

        if (!callOk(api->areaCenter(thresholdRegion,
                                    &thresholdAreaTuple,
                                    &thresholdRowTuple,
                                    &thresholdColumnTuple))) {
            rejectThreshold(QStringLiteral("threshold_no_region"));
            continue;
        }
        if (thresholdAreaTuple.num > 0)
            stats.thresholdRegionArea = api->getDouble(&thresholdAreaTuple, 0);
        if (stats.thresholdRegionArea <= 0.0) {
            rejectThreshold(QStringLiteral("threshold_no_region"));
            continue;
        }
        sawThresholdRegion = true;

        if (!callOk(api->connection(thresholdRegion, &connectedRegions))) {
            rejectThreshold(QStringLiteral("connection_no_region"));
            continue;
        }

        Hlong connectedCount = 0;
        if (!callOk(api->countObj(connectedRegions, &connectedCount))) {
            rejectThreshold(QStringLiteral("connection_no_region"));
            continue;
        }
        stats.connectedCount = qMax(0, static_cast<int>(connectedCount));
        if (connectedCount <= 0) {
            rejectThreshold(QStringLiteral("connection_no_region"));
            continue;
        }
        sawConnectedRegion = true;

        if (!callOk(api->areaCenter(connectedRegions,
                                    &rawAreaTuple,
                                    &rawRowTuple,
                                    &rawColumnTuple))) {
            sawCandidateAreaUnreadable = true;
            rejectThreshold(QStringLiteral("candidate_area_unreadable"));
            continue;
        }
        const int rawAreaCount = qMin<int>(rawAreaTuple.num,
                                           qMin<int>(rawRowTuple.num, rawColumnTuple.num));
        for (int index = 0; index < rawAreaCount; ++index)
            stats.rawAreas.append(api->getDouble(&rawAreaTuple, index));

        if (!callOk(api->selectShape(connectedRegions,
                                     &areaSelectedRegions,
                                     "area",
                                     "and",
                                     minArea,
                                     maxArea))) {
            rejectThreshold(QStringLiteral("area_filter_no_candidate"));
            continue;
        }

        Hlong areaSelectedCount = 0;
        if (!callOk(api->countObj(areaSelectedRegions, &areaSelectedCount))) {
            rejectThreshold(QStringLiteral("area_filter_no_candidate"));
            continue;
        }
        stats.areaCandidateCount = qMax(0, static_cast<int>(areaSelectedCount));
        if (areaSelectedCount <= 0)
            sawAreaFilterNoCandidate = true;

        Hobject candidateRegions = areaSelectedCount > 0
                ? areaSelectedRegions
                : connectedRegions;
        if (!callOk(api->areaCenter(candidateRegions, &areaTuple, &rowTuple, &columnTuple))) {
            sawCandidateAreaUnreadable = true;
            rejectThreshold(QStringLiteral("candidate_area_unreadable"));
            continue;
        }

        const int areaCount = qMin<int>(areaTuple.num, qMin<int>(rowTuple.num, columnTuple.num));
        if (areaCount <= 0) {
            if (areaSelectedCount <= 0) {
                rejectThreshold(QStringLiteral("area_filter_no_candidate"));
            } else {
                sawCandidateAreaUnreadable = true;
                rejectThreshold(QStringLiteral("candidate_area_unreadable"));
            }
            continue;
        }

        int bestCandidateIndex = -1;
        double bestCandidateArea = 0.0;
        int largestCandidateIndex = -1;
        double largestCandidateArea = 0.0;
        for (int index = 0; index < areaCount; ++index) {
            const double area = api->getDouble(&areaTuple, index);
            const double ratio = userRoiArea > 0.0 ? area / userRoiArea : 0.0;
            if (area > largestCandidateArea) {
                largestCandidateArea = area;
                largestCandidateIndex = index;
            }
            if (area > 0.0 &&
                ratio >= minRatio &&
                ratio <= maxRatio &&
                area > bestCandidateArea) {
                bestCandidateArea = area;
                bestCandidateIndex = index;
            }
        }
        if (bestCandidateIndex < 0) {
            bestCandidateIndex = largestCandidateIndex;
            bestCandidateArea = largestCandidateArea;
        }
        if (bestCandidateIndex < 0 || bestCandidateArea <= 0.0) {
            sawCandidateAreaUnreadable = true;
            rejectThreshold(QStringLiteral("candidate_area_unreadable"));
            continue;
        }
        sawPositiveCandidate = true;

        stats.selectedArea = bestCandidateArea;
        stats.selectedAreaRatio = userRoiArea > 0.0 ? bestCandidateArea / userRoiArea : 0.0;

        if (!callOk(api->selectObj(candidateRegions,
                                   &selectedCandidate,
                                   static_cast<Hlong>(bestCandidateIndex + 1)))) {
            sawTargetTooSmall = true;
            rejectThreshold(QStringLiteral("target_too_small"));
            continue;
        }

        if (!callOk(api->fillUp(selectedCandidate, &fillRegion))) {
            sawTargetTooSmall = true;
            rejectThreshold(QStringLiteral("target_too_small"));
            continue;
        }
        if (!callOk(api->openingCircle(fillRegion, &openedRegion, params.openingRadius))) {
            sawTargetTooSmall = true;
            rejectThreshold(QStringLiteral("target_too_small"));
            continue;
        }
        if (!callOk(api->closingCircle(openedRegion, &closedRegion, params.closingRadius))) {
            sawTargetTooSmall = true;
            rejectThreshold(QStringLiteral("target_too_small"));
            continue;
        }

        if (!callOk(api->areaCenter(closedRegion,
                                    &targetAreaTuple,
                                    &targetRowTuple,
                                    &targetColumnTuple))) {
            sawTargetTooSmall = true;
            rejectThreshold(QStringLiteral("target_too_small"));
            continue;
        }
        if (targetAreaTuple.num <= 0 || targetRowTuple.num <= 0 || targetColumnTuple.num <= 0) {
            sawTargetTooSmall = true;
            rejectThreshold(QStringLiteral("target_too_small"));
            continue;
        }

        const double targetArea = api->getDouble(&targetAreaTuple, 0);
        const double targetRow = api->getDouble(&targetRowTuple, 0);
        const double targetColumn = api->getDouble(&targetColumnTuple, 0);
        const double targetAreaRatio = userRoiArea > 0.0 ? targetArea / userRoiArea : 0.0;
        if (targetArea <= 0.0 || targetAreaRatio < minRatio) {
            sawTargetTooSmall = true;
            PatternPresenceAutoModelDomainThresholdStats rejectedStats = stats;
            rejectedStats.rejectedReason = QStringLiteral("target_too_small");
            api->genContourRegionXld(closedRegion, &displayContour, "border");
            rememberRejected(stats,
                             QStringLiteral("target_area"),
                             QStringLiteral("target_too_small"),
                             bestCandidateIndex + 1,
                             qMax(0.0, targetArea),
                             qMax(0.0, targetAreaRatio),
                             targetRow,
                             targetColumn,
                             thresholdRegion,
                             connectedRegions,
                             selectedCandidate,
                             closedRegion,
                             displayContour);
            result->perThresholdStats.append(rejectedStats);
            cleanupThreshold();
            continue;
        }
        if (targetAreaRatio > maxRatio) {
            sawTargetTooLarge = true;
            PatternPresenceAutoModelDomainThresholdStats rejectedStats = stats;
            rejectedStats.rejectedReason = QStringLiteral("target_too_large_or_background");
            api->genContourRegionXld(closedRegion, &displayContour, "border");
            rememberRejected(stats,
                             QStringLiteral("target_area"),
                             QStringLiteral("target_too_large_or_background"),
                             bestCandidateIndex + 1,
                             targetArea,
                             targetAreaRatio,
                             targetRow,
                             targetColumn,
                             thresholdRegion,
                             connectedRegions,
                             selectedCandidate,
                             closedRegion,
                             displayContour);
            result->perThresholdStats.append(rejectedStats);
            cleanupThreshold();
            continue;
        }

        if (!callOk(api->dilationCircle(closedRegion, &modelDomain, params.dilationRadius))) {
            sawReduceFailed = true;
            PatternPresenceAutoModelDomainThresholdStats rejectedStats = stats;
            rejectedStats.rejectedReason = QStringLiteral("reduce_domain_failed");
            rememberRejected(stats,
                             QStringLiteral("reduce_domain"),
                             QStringLiteral("reduce_domain_failed"),
                             bestCandidateIndex + 1,
                             targetArea,
                             targetAreaRatio,
                             targetRow,
                             targetColumn,
                             thresholdRegion,
                             connectedRegions,
                             selectedCandidate,
                             closedRegion,
                             displayContour);
            result->perThresholdStats.append(rejectedStats);
            cleanupThreshold();
            continue;
        }
        if (!callOk(api->reduceDomain(graySource, modelDomain, &reducedImage))) {
            sawReduceFailed = true;
            PatternPresenceAutoModelDomainThresholdStats rejectedStats = stats;
            rejectedStats.rejectedReason = QStringLiteral("reduce_domain_failed");
            rememberRejected(stats,
                             QStringLiteral("reduce_domain"),
                             QStringLiteral("reduce_domain_failed"),
                             bestCandidateIndex + 1,
                             targetArea,
                             targetAreaRatio,
                             targetRow,
                             targetColumn,
                             thresholdRegion,
                             connectedRegions,
                             selectedCandidate,
                             closedRegion,
                             displayContour);
            result->perThresholdStats.append(rejectedStats);
            cleanupThreshold();
            continue;
        }
        if (!callOk(api->genContourRegionXld(closedRegion, &displayContour, "border"))) {
            sawDisplayFailed = true;
            PatternPresenceAutoModelDomainThresholdStats rejectedStats = stats;
            rejectedStats.rejectedReason = QStringLiteral("display_contour_failed");
            rememberRejected(stats,
                             QStringLiteral("display_contour"),
                             QStringLiteral("display_contour_failed"),
                             bestCandidateIndex + 1,
                             targetArea,
                             targetAreaRatio,
                             targetRow,
                             targetColumn,
                             thresholdRegion,
                             connectedRegions,
                             selectedCandidate,
                             closedRegion,
                             displayContour);
            result->perThresholdStats.append(rejectedStats);
            cleanupThreshold();
            continue;
        }

        stats.rejectedReason.clear();
        result->perThresholdStats.append(stats);
        if (!hasBestValid || targetArea > bestValidTargetArea) {
            clearBestValidObjects();
            hasBestValid = true;
            bestValidStats = stats;
            bestValidCandidateIndex = bestCandidateIndex + 1;
            bestValidTargetArea = targetArea;
            bestValidTargetAreaRatio = targetAreaRatio;
            bestValidTargetRow = targetRow;
            bestValidTargetColumn = targetColumn;
            bestThresholdRegion = thresholdRegion;
            bestConnectedRegions = connectedRegions;
            bestSelectedCandidate = selectedCandidate;
            bestClosedRegion = closedRegion;
            bestModelDomain = modelDomain;
            bestReducedImage = reducedImage;
            bestDisplayContour = displayContour;
            thresholdRegion = NO_OBJECTS;
            connectedRegions = NO_OBJECTS;
            selectedCandidate = NO_OBJECTS;
            closedRegion = NO_OBJECTS;
            modelDomain = NO_OBJECTS;
            reducedImage = NO_OBJECTS;
            displayContour = NO_OBJECTS;
        }
        cleanupThreshold();
    }

    if (hasBestValid) {
        clearBestRejectedObjects();
        result->modelDomain = bestModelDomain;
        result->reducedImageForModel = bestReducedImage;
        result->targetRegion = bestClosedRegion;
        result->displayContour = bestDisplayContour;
        result->debugThresholdRegion = bestThresholdRegion;
        result->debugConnectedRegions = bestConnectedRegions;
        result->debugSelectedCandidateRegion = bestSelectedCandidate;
        result->applied = true;
        result->fallbackReason.clear();
        result->failureStage.clear();
        result->failureReason.clear();
        result->selectedThresholdHigh = bestValidStats.thresholdHigh;
        result->thresholdHigh = bestValidStats.thresholdHigh;
        result->thresholdRegionArea = bestValidStats.thresholdRegionArea;
        result->connectedRegionCount = bestValidStats.connectedCount;
        result->rawCandidateCount = bestValidStats.connectedCount;
        result->areaCandidateCount = bestValidStats.areaCandidateCount;
        result->selectedCandidateArea = bestValidStats.selectedArea;
        result->selectedCandidateIndex = bestValidCandidateIndex;
        result->selectedCandidateAreaRatio = bestValidStats.selectedAreaRatio;
        result->candidateCount = bestValidStats.connectedCount;
        result->noBorderCandidateCount = bestValidStats.areaCandidateCount;
        result->targetArea = bestValidTargetArea;
        result->targetAreaRatio = bestValidTargetAreaRatio;
        result->targetRow = bestValidTargetRow;
        result->targetColumn = bestValidTargetColumn;
        if (params.polygonTemplate &&
            result->targetAreaRatio > 0.85 &&
            result->targetAreaRatio < params.polygonRejectMaxAreaRatio) {
            result->warning = QStringLiteral("target area is close to polygon ROI area");
        }

        bestModelDomain = NO_OBJECTS;
        bestReducedImage = NO_OBJECTS;
        bestClosedRegion = NO_OBJECTS;
        bestDisplayContour = NO_OBJECTS;
        bestThresholdRegion = NO_OBJECTS;
        bestConnectedRegions = NO_OBJECTS;
        bestSelectedCandidate = NO_OBJECTS;
        clearBestValidObjects();
        return true;
    }

    clearBestValidObjects();

    QString finalStage;
    QString finalReason;
    if (sawReduceFailed) {
        finalStage = QStringLiteral("reduce_domain");
        finalReason = QStringLiteral("reduce_domain_failed");
    } else if (sawDisplayFailed) {
        finalStage = QStringLiteral("display_contour");
        finalReason = QStringLiteral("display_contour_failed");
    } else if (sawTargetTooLarge) {
        finalStage = QStringLiteral("target_area");
        finalReason = QStringLiteral("target_too_large_or_background");
    } else if (sawTargetTooSmall) {
        finalStage = QStringLiteral("target_area");
        finalReason = QStringLiteral("target_too_small");
    } else if (!sawThresholdRegion) {
        finalStage = QStringLiteral("threshold");
        finalReason = QStringLiteral("threshold_no_region");
    } else if (!sawConnectedRegion) {
        finalStage = QStringLiteral("connection");
        finalReason = QStringLiteral("connection_no_region");
    } else if (sawCandidateAreaUnreadable) {
        finalStage = QStringLiteral("area_center");
        finalReason = QStringLiteral("candidate_area_unreadable");
    } else if (!sawPositiveCandidate) {
        finalStage = sawAreaFilterNoCandidate ? QStringLiteral("select_shape_area")
                                               : QStringLiteral("candidate_selection");
        finalReason = sawAreaFilterNoCandidate ? QStringLiteral("area_filter_no_candidate")
                                                : QStringLiteral("candidate_area_zero");
    } else {
        finalStage = QStringLiteral("candidate_selection");
        finalReason = QStringLiteral("candidate_area_zero");
    }

    if (hasBestRejected) {
        result->debugThresholdRegion = bestRejectedThresholdRegion;
        result->debugConnectedRegions = bestRejectedConnectedRegions;
        result->debugSelectedCandidateRegion = bestRejectedSelectedCandidate;
        result->targetRegion = bestRejectedClosedRegion;
        result->displayContour = bestRejectedDisplayContour;
        result->selectedThresholdHigh = bestRejectedStats.thresholdHigh;
        result->thresholdHigh = bestRejectedStats.thresholdHigh;
        result->thresholdRegionArea = bestRejectedStats.thresholdRegionArea;
        result->connectedRegionCount = bestRejectedStats.connectedCount;
        result->rawCandidateCount = bestRejectedStats.connectedCount;
        result->areaCandidateCount = bestRejectedStats.areaCandidateCount;
        result->selectedCandidateArea = bestRejectedStats.selectedArea;
        result->selectedCandidateIndex = bestRejectedCandidateIndex;
        result->selectedCandidateAreaRatio = bestRejectedStats.selectedAreaRatio;
        result->targetArea = bestRejectedTargetArea > 0.0 ? bestRejectedTargetArea : 0.0;
        result->targetAreaRatio = bestRejectedTargetAreaRatio;
        result->targetRow = bestRejectedTargetRow;
        result->targetColumn = bestRejectedTargetColumn;
        result->candidateCount = bestRejectedStats.connectedCount;
        result->noBorderCandidateCount = bestRejectedStats.areaCandidateCount;
        bestRejectedThresholdRegion = NO_OBJECTS;
        bestRejectedConnectedRegions = NO_OBJECTS;
        bestRejectedSelectedCandidate = NO_OBJECTS;
        bestRejectedClosedRegion = NO_OBJECTS;
        bestRejectedDisplayContour = NO_OBJECTS;
        if (!bestRejectedReason.isEmpty()) {
            finalStage = bestRejectedStage;
            finalReason = bestRejectedReason;
        }
    }
    clearBestRejectedObjects();
    return fail(finalStage, finalReason);
}
