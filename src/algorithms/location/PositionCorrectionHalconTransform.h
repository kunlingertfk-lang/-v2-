#ifndef ALGORITHMS_LOCATION_POSITIONCORRECTIONHALCONTRANSFORM_H
#define ALGORITHMS_LOCATION_POSITIONCORRECTIONHALCONTRANSFORM_H

#include <HalconC.h>

#include <QString>
#include <QVector>

struct PositionCorrectionHalconRegionApi
{
    using CreateTupleFn = void (*)(Htuple *, Hlong);
    using SetDoubleFn = void (*)(Htuple *, double, Hlong);
    using SetStringFn = void (*)(Htuple *, const char *, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using AffineTransRegionFn = Herror (*)(const Hobject, Hobject *,
                                           const Htuple, const Htuple);
    using ClipRegionFn = Herror (*)(const Hobject, Hobject *,
                                    const Htuple, const Htuple,
                                    const Htuple, const Htuple);
    using AreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);

    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    AffineTransRegionFn affineTransRegion = nullptr;
    ClipRegionFn clipRegion = nullptr;
    AreaCenterFn areaCenter = nullptr;
};

struct PositionCorrectionHalconTransformResult
{
    bool success = false;
    QString status;
    QString operation;
    Herror halconStatus = H_MSG_OK;
    double area = 0.0;
};

namespace PositionCorrectionHalconTransform {

PositionCorrectionHalconTransformResult transformAndClipRegion(
        const PositionCorrectionHalconRegionApi &api,
        Hobject sourceRegion,
        Hobject *transformedRegion,
        Hobject *clippedRegion,
        const QVector<double> &referenceToRunHomMat2D,
        int imageWidth,
        int imageHeight);

} // namespace PositionCorrectionHalconTransform

#endif // ALGORITHMS_LOCATION_POSITIONCORRECTIONHALCONTRANSFORM_H
