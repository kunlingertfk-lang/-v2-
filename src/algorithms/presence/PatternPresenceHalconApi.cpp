#include "algorithms/presence/PatternPresenceHalconApi.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#include <dlfcn.h>

namespace {

template <typename Function>
bool resolveRequired(void *handle, Function &target, const char *symbolName, QString &errorMessage)
{
    dlerror();
    void *symbol = dlsym(handle, symbolName);
    const char *symbolError = dlerror();
    if (symbolError != nullptr || symbol == nullptr) {
        errorMessage = QStringLiteral("Missing HALCON symbol %1: %2")
                .arg(QString::fromLatin1(symbolName),
                     symbolError ? QString::fromLocal8Bit(symbolError) : QStringLiteral("not found"));
        return false;
    }

    target = reinterpret_cast<Function>(symbol);
    return true;
}

template <typename Function>
void resolveOptional(void *handle, Function &target, const char *symbolName)
{
    dlerror();
    void *symbol = dlsym(handle, symbolName);
    const char *symbolError = dlerror();
    if (symbolError == nullptr && symbol != nullptr)
        target = reinterpret_cast<Function>(symbol);
}

QMutex &libraryCacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, QSharedPointer<PatternPresenceHalconLibrary>> &libraryCache()
{
    static QHash<QString, QSharedPointer<PatternPresenceHalconLibrary>> cache;
    return cache;
}

} // namespace

bool PatternPresenceHalconApi::hasAutoModelDomainOperators() const
{
    return threshold &&
           connection &&
           selectShape &&
           fillUp &&
           openingCircle &&
           closingCircle &&
           dilationCircle &&
           areaCenter &&
           genContourRegionXld &&
           reduceDomain &&
           countObj &&
           selectObj;
}

bool PatternPresenceHalconApi::hasContourTransformOperators() const
{
    return vectorAngleToRigid &&
           affineTransContourXld &&
           getShapeModelContours &&
           countObj &&
           selectObj &&
           getContourXld;
}

bool PatternPresenceHalconApi::hasXldShapeModelOperators() const
{
    return createScaledShapeModelXld &&
           findScaledShapeModel &&
           getShapeModelContours &&
           vectorAngleToRigid &&
           affineTransContourXld &&
           clearShapeModel;
}

bool PatternPresenceHalconApi::hasContourExtractionOperators() const
{
    return edgesSubPix &&
           segmentContoursXld &&
           selectContoursXld &&
           lengthXld &&
           genContourRegionXld &&
           threshold &&
           connection &&
           reduceDomain &&
           countObj &&
           selectObj &&
           getContourXld;
}

PatternPresenceHalconLibrary::~PatternPresenceHalconLibrary()
{
    if (m_handle)
        dlclose(m_handle);
}

bool PatternPresenceHalconLibrary::load(const QString &path,
                                        QString &errorMessage,
                                        bool &symbolMissing)
{
    symbolMissing = false;
    const QByteArray encodedPath = path.trimmed().toLocal8Bit();
    m_handle = dlopen(encodedPath.constData(), RTLD_NOW | RTLD_LOCAL);
    if (!m_handle) {
        const char *loadError = dlerror();
        errorMessage = loadError ? QString::fromLocal8Bit(loadError)
                                 : QStringLiteral("dlopen returned a null handle.");
        return false;
    }

    resolveOptional(m_handle, api.setUtf8, "SetHcInterfaceStringEncodingIsUtf8");
    resolveOptional(m_handle, api.setCheck, "set_check");
    resolveOptional(m_handle, api.genRegionPolygon, "T_gen_region_polygon");
    if (!api.genRegionPolygon)
        resolveOptional(m_handle, api.genRegionPolygon, "gen_region_polygon");
    resolveOptional(m_handle, api.reduceDomain, "reduce_domain");
    resolveOptional(m_handle, api.countObj, "count_obj");
    resolveOptional(m_handle, api.selectObj, "select_obj");
    resolveOptional(m_handle, api.threshold, "threshold");
    resolveOptional(m_handle, api.connection, "connection");
    resolveOptional(m_handle, api.selectShape, "select_shape");
    resolveOptional(m_handle, api.fillUp, "fill_up");
    resolveOptional(m_handle, api.openingCircle, "opening_circle");
    resolveOptional(m_handle, api.closingCircle, "closing_circle");
    resolveOptional(m_handle, api.dilationCircle, "dilation_circle");
    resolveOptional(m_handle, api.areaCenter, "T_area_center");
    resolveOptional(m_handle, api.genContourRegionXld, "gen_contour_region_xld");
    resolveOptional(m_handle, api.getShapeModelContours, "T_get_shape_model_contours");
    resolveOptional(m_handle, api.vectorAngleToRigid, "T_vector_angle_to_rigid");
    resolveOptional(m_handle, api.homMat2dScaleLocal, "T_hom_mat2d_scale_local");
    resolveOptional(m_handle, api.affineTransContourXld, "T_affine_trans_contour_xld");
    resolveOptional(m_handle, api.getContourXld, "T_get_contour_xld");
    resolveOptional(m_handle, api.createScaledShapeModelXld, "T_create_scaled_shape_model_xld");
    resolveOptional(m_handle, api.edgesSubPix, "edges_sub_pix");
    resolveOptional(m_handle, api.segmentContoursXld, "segment_contours_xld");
    resolveOptional(m_handle, api.selectContoursXld, "select_contours_xld");
    resolveOptional(m_handle, api.lengthXld, "T_length_xld");
    resolveOptional(m_handle, api.binaryThreshold, "binary_threshold");

    if (!resolveRequired(m_handle, api.getErrorText, "get_error_text", errorMessage) ||
        !resolveRequired(m_handle, api.createTuple, "F_create_tuple", errorMessage) ||
        !resolveRequired(m_handle, api.createTupleInt, "F_create_tuple_i", errorMessage) ||
        !resolveRequired(m_handle, api.createTupleDouble, "F_create_tuple_d", errorMessage) ||
        !resolveRequired(m_handle, api.createTupleString, "F_create_tuple_s", errorMessage) ||
        !resolveRequired(m_handle, api.setDouble, "F_set_d", errorMessage) ||
        !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
        !resolveRequired(m_handle, api.getHandle, "F_get_h", errorMessage) ||
        !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
        !resolveRequired(m_handle, api.genImage1, "gen_image1", errorMessage) ||
        !resolveRequired(m_handle, api.genImageInterleaved, "gen_image_interleaved", errorMessage) ||
        !resolveRequired(m_handle, api.rgb1ToGray, "rgb1_to_gray", errorMessage) ||
        !resolveRequired(m_handle, api.createShapeModel, "T_create_shape_model", errorMessage) ||
        !resolveRequired(m_handle, api.findShapeModel, "T_find_shape_model", errorMessage) ||
        !resolveRequired(m_handle, api.createScaledShapeModel, "T_create_scaled_shape_model", errorMessage) ||
        !resolveRequired(m_handle, api.findScaledShapeModel, "T_find_scaled_shape_model", errorMessage) ||
        !resolveRequired(m_handle, api.setShapeModelParam, "T_set_shape_model_param", errorMessage) ||
        !resolveRequired(m_handle, api.clearShapeModel, "T_clear_shape_model", errorMessage) ||
        !resolveRequired(m_handle, api.clearObj, "clear_obj", errorMessage)) {
        symbolMissing = true;
        dlclose(m_handle);
        m_handle = nullptr;
        return false;
    }

    if (api.setUtf8)
        api.setUtf8(1);
    if (api.setCheck)
        api.setCheck("~give_error");

    return true;
}

QString PatternPresenceHalconLibrary::errorText(const Herror status) const
{
    char buffer[1024] = {0};
    if (api.getErrorText && patternPresenceHalconStatusOk(api.getErrorText(static_cast<Hlong>(status), buffer))) {
        const QString message = QString::fromUtf8(buffer).trimmed();
        if (!message.isEmpty())
            return message;
    }
    return QStringLiteral("HALCON error code %1").arg(static_cast<qlonglong>(status));
}

bool patternPresenceHalconStatusOk(const Herror status)
{
    return status == H_MSG_OK || status == H_MSG_TRUE || status == H_MSG_FALSE;
}

bool patternPresenceHalconObjectAllocated(const Hobject object)
{
    return object != 0 && object != NO_OBJECTS && object != EMPTY_REGION;
}

QSharedPointer<PatternPresenceHalconLibrary> sharedPatternPresenceHalconLibrary(const QString &path,
                                                                                QString &errorMessage,
                                                                                bool &symbolMissing)
{
    const QString normalizedPath = path.trimmed();
    QMutexLocker locker(&libraryCacheMutex());

    auto it = libraryCache().constFind(normalizedPath);
    if (it != libraryCache().constEnd()) {
        symbolMissing = false;
        return it.value();
    }

    QSharedPointer<PatternPresenceHalconLibrary> library(new PatternPresenceHalconLibrary);
    if (!library->load(normalizedPath, errorMessage, symbolMissing))
        return QSharedPointer<PatternPresenceHalconLibrary>();

    libraryCache().insert(normalizedPath, library);
    return library;
}
