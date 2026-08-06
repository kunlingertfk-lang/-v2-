#include "algorithms/location/PositionCorrectionHalconTransform.h"

#include "toolcore/PositionCorrectionTransform.h"

namespace {

class TupleGuard
{
public:
    explicit TupleGuard(const PositionCorrectionHalconRegionApi &api)
        : m_api(api)
    {
    }

    ~TupleGuard()
    {
        if (m_tuple.num > 0 || m_tuple.capacity > 0)
            m_api.destroyTuple(&m_tuple);
    }

    Htuple *ptr() { return &m_tuple; }
    const Htuple &value() const { return m_tuple; }

private:
    const PositionCorrectionHalconRegionApi &m_api;
    Htuple m_tuple = HTUPLE_INITIALIZER;
};

bool validApi(const PositionCorrectionHalconRegionApi &api)
{
    return api.createTuple && api.setDouble && api.setString
            && api.destroyTuple && api.getDouble
            && api.affineTransRegion && api.clipRegion && api.areaCenter;
}

PositionCorrectionHalconTransformResult halconFailure(
        const QString &operation,
        Herror status)
{
    PositionCorrectionHalconTransformResult result;
    result.status = QStringLiteral("halcon_runtime_error");
    result.operation = operation;
    result.halconStatus = status;
    return result;
}

} // namespace

PositionCorrectionHalconTransformResult
PositionCorrectionHalconTransform::transformAndClipRegion(
        const PositionCorrectionHalconRegionApi &api,
        Hobject sourceRegion,
        Hobject *transformedRegion,
        Hobject *clippedRegion,
        const QVector<double> &referenceToRunHomMat2D,
        int imageWidth,
        int imageHeight)
{
    PositionCorrectionHalconTransformResult result;
    if (!validApi(api)) {
        result.status = QStringLiteral("halcon_symbol_missing");
        result.operation = QStringLiteral("position_correction_transform");
        return result;
    }
    if (!transformedRegion || !clippedRegion
            || imageWidth <= 0 || imageHeight <= 0
            || !PositionCorrectionTransform::isValidHomMat2D(
                referenceToRunHomMat2D)) {
        result.status = QStringLiteral("invalid_position_correction_matrix");
        result.operation = QStringLiteral("position_correction_transform");
        return result;
    }

    TupleGuard matrix(api);
    api.createTuple(matrix.ptr(), 6);
    for (int index = 0; index < 6; ++index)
        api.setDouble(matrix.ptr(), referenceToRunHomMat2D.at(index), index);

    TupleGuard interpolation(api);
    api.createTuple(interpolation.ptr(), 1);
    api.setString(interpolation.ptr(), "nearest_neighbor", 0);

    Herror status = api.affineTransRegion(
                sourceRegion, transformedRegion,
                matrix.value(), interpolation.value());
    if (status != H_MSG_OK && status != H_MSG_TRUE && status != H_MSG_FALSE)
        return halconFailure(QStringLiteral("affine_trans_region"), status);

    TupleGuard row1(api);
    TupleGuard column1(api);
    TupleGuard row2(api);
    TupleGuard column2(api);
    api.createTuple(row1.ptr(), 1);
    api.createTuple(column1.ptr(), 1);
    api.createTuple(row2.ptr(), 1);
    api.createTuple(column2.ptr(), 1);
    api.setDouble(row1.ptr(), 0.0, 0);
    api.setDouble(column1.ptr(), 0.0, 0);
    api.setDouble(row2.ptr(), imageHeight - 1.0, 0);
    api.setDouble(column2.ptr(), imageWidth - 1.0, 0);

    status = api.clipRegion(
                *transformedRegion, clippedRegion,
                row1.value(), column1.value(),
                row2.value(), column2.value());
    if (status != H_MSG_OK && status != H_MSG_TRUE && status != H_MSG_FALSE)
        return halconFailure(QStringLiteral("clip_region"), status);

    TupleGuard area(api);
    TupleGuard row(api);
    TupleGuard column(api);
    status = api.areaCenter(*clippedRegion, area.ptr(), row.ptr(), column.ptr());
    if (status != H_MSG_OK && status != H_MSG_TRUE && status != H_MSG_FALSE)
        return halconFailure(QStringLiteral("area_center"), status);

    result.success = true;
    result.area = area.value().num > 0 ? api.getDouble(&area.value(), 0) : 0.0;
    return result;
}
