#ifndef ALGORITHMS_PRESENCE_PATTERNPRESENCEHALCONAPI_H
#define ALGORITHMS_PRESENCE_PATTERNPRESENCEHALCONAPI_H

#include <HalconC.h>

#include <QSharedPointer>
#include <QString>

struct PatternPresenceHalconApi
{
    using SetUtf8Fn = void (*)(int);
    using SetCheckFn = Herror (*)(const char *);
    using GetErrorTextFn = Herror (*)(Hlong, char *);
    using CreateTupleFn = void (*)(Htuple *, Hlong);
    using CreateTupleIntFn = void (*)(Htuple *, Hlong);
    using CreateTupleDoubleFn = void (*)(Htuple *, double);
    using CreateTupleStringFn = void (*)(Htuple *, const char *);
    using SetDoubleFn = void (*)(Htuple *, double, Hlong);
    using SetStringFn = void (*)(Htuple *, const char *, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetHandleFn = Hphandle (*)(const Htuple *, Hlong);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GenImage1Fn = Herror (*)(Hobject *, const char *, Hlong, Hlong, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Rgb1ToGrayFn = Herror (*)(const Hobject, Hobject *);
    using GenRegionPolygonFn = Herror (*)(Hobject *, const Htuple, const Htuple);
    using GenRectangle1Fn = Herror (*)(Hobject *, double, double, double, double);
    using AffineTransRegionFn = Herror (*)(const Hobject, Hobject *,
                                           const Htuple, const Htuple);
    using ClipRegionFn = Herror (*)(const Hobject, Hobject *,
                                    const Htuple, const Htuple,
                                    const Htuple, const Htuple);
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using CountObjFn = Herror (*)(const Hobject, Hlong *);
    using SelectObjFn = Herror (*)(const Hobject, Hobject *, Hlong);
    using ThresholdFn = Herror (*)(const Hobject, Hobject *, double, double);
    using ConnectionFn = Herror (*)(const Hobject, Hobject *);
    using SelectShapeFn = Herror (*)(const Hobject, Hobject *, const char *, const char *, double, double);
    using FillUpFn = Herror (*)(const Hobject, Hobject *);
    using OpeningCircleFn = Herror (*)(const Hobject, Hobject *, double);
    using ClosingCircleFn = Herror (*)(const Hobject, Hobject *, double);
    using DilationCircleFn = Herror (*)(const Hobject, Hobject *, double);
    using AreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using GenContourRegionXldFn = Herror (*)(const Hobject, Hobject *, const char *);
    using CreateShapeModelFn = Herror (*)(const Hobject, const Htuple, const Htuple, const Htuple,
                                          const Htuple, const Htuple, const Htuple, const Htuple,
                                          const Htuple, Htuple *);
    using FindShapeModelFn = Herror (*)(const Hobject, const Htuple, const Htuple, const Htuple,
                                        const Htuple, const Htuple, const Htuple, const Htuple,
                                        const Htuple, const Htuple, Htuple *, Htuple *, Htuple *, Htuple *);
    using CreateScaledShapeModelFn = Herror (*)(const Hobject, const Htuple, const Htuple, const Htuple,
                                                const Htuple, const Htuple, const Htuple, const Htuple,
                                                const Htuple, const Htuple, const Htuple, const Htuple,
                                                Htuple *);
    using CreateScaledShapeModelXldFn = Herror (*)(const Hobject, const Htuple, const Htuple, const Htuple,
                                                   const Htuple, const Htuple, const Htuple, const Htuple,
                                                   const Htuple, const Htuple, const Htuple, Htuple *);
    using FindScaledShapeModelFn = Herror (*)(const Hobject, const Htuple, const Htuple, const Htuple,
                                              const Htuple, const Htuple, const Htuple, const Htuple,
                                              const Htuple, const Htuple, const Htuple, const Htuple,
                                              Htuple *, Htuple *, Htuple *, Htuple *, Htuple *);
    using SetShapeModelParamFn = Herror (*)(const Htuple, const Htuple, const Htuple);
    using GetShapeModelContoursFn = Herror (*)(Hobject *, const Htuple, const Htuple);
    using GetShapeModelParamsFn = Herror (*)(const Htuple, Htuple *, Htuple *, Htuple *, Htuple *,
                                             Htuple *, Htuple *, Htuple *, Htuple *, Htuple *);
    using VectorAngleToRigidFn = Herror (*)(const Htuple, const Htuple, const Htuple,
                                            const Htuple, const Htuple, const Htuple,
                                            Htuple *);
    using HomMat2dScaleLocalFn = Herror (*)(const Htuple, const Htuple, const Htuple, Htuple *);
    using AffineTransContourXldFn = Herror (*)(const Hobject, Hobject *, const Htuple);
    using GetContourXldFn = Herror (*)(const Hobject, Htuple *, Htuple *);
    using EdgesSubPixFn = Herror (*)(const Hobject, Hobject *, const char *, double, Hlong, Hlong);
    using SegmentContoursXldFn = Herror (*)(const Hobject, Hobject *, const char *, Hlong, double, double);
    using SelectContoursXldFn = Herror (*)(const Hobject, Hobject *, const char *, double, double, double, double);
    using LengthXldFn = Herror (*)(const Hobject, Htuple *);
    using BinaryThresholdFn = Herror (*)(const Hobject, Hobject *, const char *, const char *, Hlong *);
    using ClearShapeModelFn = Herror (*)(const Htuple);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    SetCheckFn setCheck = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    CreateTupleIntFn createTupleInt = nullptr;
    CreateTupleDoubleFn createTupleDouble = nullptr;
    CreateTupleStringFn createTupleString = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetHandleFn getHandle = nullptr;
    GetDoubleFn getDouble = nullptr;
    GenImage1Fn genImage1 = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Rgb1ToGrayFn rgb1ToGray = nullptr;
    GenRegionPolygonFn genRegionPolygon = nullptr;
    GenRectangle1Fn genRectangle1 = nullptr;
    AffineTransRegionFn affineTransRegion = nullptr;
    ClipRegionFn clipRegion = nullptr;
    ReduceDomainFn reduceDomain = nullptr;
    CountObjFn countObj = nullptr;
    SelectObjFn selectObj = nullptr;
    ThresholdFn threshold = nullptr;
    ConnectionFn connection = nullptr;
    SelectShapeFn selectShape = nullptr;
    FillUpFn fillUp = nullptr;
    OpeningCircleFn openingCircle = nullptr;
    ClosingCircleFn closingCircle = nullptr;
    DilationCircleFn dilationCircle = nullptr;
    AreaCenterFn areaCenter = nullptr;
    GenContourRegionXldFn genContourRegionXld = nullptr;
    CreateShapeModelFn createShapeModel = nullptr;
    FindShapeModelFn findShapeModel = nullptr;
    CreateScaledShapeModelFn createScaledShapeModel = nullptr;
    CreateScaledShapeModelXldFn createScaledShapeModelXld = nullptr;
    FindScaledShapeModelFn findScaledShapeModel = nullptr;
    SetShapeModelParamFn setShapeModelParam = nullptr;
    GetShapeModelContoursFn getShapeModelContours = nullptr;
    GetShapeModelParamsFn getShapeModelParams = nullptr;
    VectorAngleToRigidFn vectorAngleToRigid = nullptr;
    HomMat2dScaleLocalFn homMat2dScaleLocal = nullptr;
    AffineTransContourXldFn affineTransContourXld = nullptr;
    GetContourXldFn getContourXld = nullptr;
    EdgesSubPixFn edgesSubPix = nullptr;
    SegmentContoursXldFn segmentContoursXld = nullptr;
    SelectContoursXldFn selectContoursXld = nullptr;
    LengthXldFn lengthXld = nullptr;
    BinaryThresholdFn binaryThreshold = nullptr;
    ClearShapeModelFn clearShapeModel = nullptr;
    ClearObjFn clearObj = nullptr;

    bool hasAutoModelDomainOperators() const;
    bool hasContourTransformOperators() const;
    bool hasXldShapeModelOperators() const;
    bool hasContourExtractionOperators() const;
};

class PatternPresenceHalconLibrary
{
public:
    ~PatternPresenceHalconLibrary();

    bool load(const QString &path, QString &errorMessage, bool &symbolMissing);
    QString errorText(Herror status) const;

    PatternPresenceHalconApi api;

private:
    void *m_handle = nullptr;
};

bool patternPresenceHalconStatusOk(Herror status);
bool patternPresenceHalconObjectAllocated(Hobject object);

QSharedPointer<PatternPresenceHalconLibrary> sharedPatternPresenceHalconLibrary(const QString &path,
                                                                                QString &errorMessage,
                                                                                bool &symbolMissing);

#endif // ALGORITHMS_PRESENCE_PATTERNPRESENCEHALCONAPI_H
