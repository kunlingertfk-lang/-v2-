#include "algorithms/ocr/OcrHalconRunner.h"

#include <HalconC.h>

#include <QFileInfo>
#include <QJsonArray>
#include <QRect>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <dlfcn.h>
#include <exception>
#include <opencv2/core.hpp>

namespace {

bool halconStatusOk(const Herror status)
{
    return status == H_MSG_OK || status == H_MSG_TRUE || status == H_MSG_FALSE;
}

bool halconObjectAllocated(const Hobject object)
{
    return object != 0 && object != NO_OBJECTS && object != EMPTY_REGION;
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

QRect normalizedRoiToPixels(const QRectF &sourceRoi, const int width, const int height)
{
    if (width <= 0 || height <= 0)
        return QRect();

    QRectF roi = sourceRoi.normalized();
    if (roi.width() <= 0.0 || roi.height() <= 0.0)
        roi = QRectF(0.0, 0.0, 1.0, 1.0);

    const double left = qBound(0.0, roi.left(), 1.0);
    const double top = qBound(0.0, roi.top(), 1.0);
    const double right = qBound(0.0, roi.right(), 1.0);
    const double bottom = qBound(0.0, roi.bottom(), 1.0);

    int x1 = qBound(0, static_cast<int>(std::floor(left * width)), width - 1);
    int y1 = qBound(0, static_cast<int>(std::floor(top * height)), height - 1);
    int x2 = qBound(0, static_cast<int>(std::ceil(right * width)) - 1, width - 1);
    int y2 = qBound(0, static_cast<int>(std::ceil(bottom * height)) - 1, height - 1);

    if (x2 < x1)
        std::swap(x1, x2);
    if (y2 < y1)
        std::swap(y1, y2);

    return QRect(QPoint(x1, y1), QPoint(x2, y2));
}

ToolOverlay rectOverlay(const QRectF &rect, const QString &label, const double score = 0.0)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Rect;
    overlay.rect = rect;
    overlay.label = label;
    overlay.score = score;
    return overlay;
}

ToolOverlay textOverlay(const QPointF &position, const QString &text, const QString &label, const double score)
{
    ToolOverlay overlay;
    overlay.type = ToolOverlayType::Text;
    overlay.p1 = position;
    overlay.text = text;
    overlay.label = label;
    overlay.score = score;
    return overlay;
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

struct HalconCApi
{
    using SetUtf8Fn = void (*)(int);
    using GetErrorTextFn = Herror (*)(Hlong, char *);
    using CreateTupleFn = void (*)(Htuple *, Hlong);
    using CreateTupleStringFn = void (*)(Htuple *, const char *);
    using SetDoubleFn = void (*)(Htuple *, double, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GenImage1Fn = Herror (*)(Hobject *, const char *, Hlong, Hlong, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Rgb1ToGrayFn = Herror (*)(const Hobject, Hobject *);
    using GenRegionPolygonFilledFn = Herror (*)(Hobject *, const Htuple, const Htuple);
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using ThresholdFn = Herror (*)(const Hobject, Hobject *, double, double);
    using ConnectionFn = Herror (*)(const Hobject, Hobject *);
    using CountObjFn = Herror (*)(const Hobject, Hlong *);
    using SelectObjFn = Herror (*)(const Hobject, Hobject *, Hlong);
    using SmallestRectangle1Fn = Herror (*)(const Hobject, Hlong *, Hlong *, Hlong *, Hlong *);
    using SelectShapeFn = Herror (*)(const Hobject, Hobject *, const char *, const char *, double, double);
    using SortRegionFn = Herror (*)(const Hobject, Hobject *, const char *, const char *, const char *);
    using ReadOcrClassMlpFn = Herror (*)(const Htuple, Htuple *);
    using DoOcrMultiClassMlpFn = Herror (*)(const Hobject, const Hobject, const Htuple, Htuple *, Htuple *);
    using ClearOcrClassMlpFn = Herror (*)(const Htuple);
    using GetStringUtf8Fn = Hlong (*)(char *, Hlong, const Htuple *, Hlong);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    CreateTupleStringFn createTupleString = nullptr;
    SetDoubleFn setDouble = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GenImage1Fn genImage1 = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Rgb1ToGrayFn rgb1ToGray = nullptr;
    GenRegionPolygonFilledFn genRegionPolygonFilled = nullptr;
    ReduceDomainFn reduceDomain = nullptr;
    ThresholdFn threshold = nullptr;
    ConnectionFn connection = nullptr;
    CountObjFn countObj = nullptr;
    SelectObjFn selectObj = nullptr;
    SmallestRectangle1Fn smallestRectangle1 = nullptr;
    SelectShapeFn selectShape = nullptr;
    SortRegionFn sortRegion = nullptr;
    ReadOcrClassMlpFn readOcrClassMlp = nullptr;
    DoOcrMultiClassMlpFn doOcrMultiClassMlp = nullptr;
    ClearOcrClassMlpFn clearOcrClassMlp = nullptr;
    GetStringUtf8Fn getStringUtf8 = nullptr;
    ClearObjFn clearObj = nullptr;
};

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

class HalconLibrary
{
public:
    ~HalconLibrary()
    {
        if (m_handle)
            dlclose(m_handle);
    }

    bool load(const QString &path, QString &errorMessage, bool &symbolMissing)
    {
        symbolMissing = false;
        const QByteArray encodedPath = path.toLocal8Bit();
        m_handle = dlopen(encodedPath.constData(), RTLD_NOW | RTLD_LOCAL);
        if (!m_handle) {
            const char *loadError = dlerror();
            errorMessage = loadError ? QString::fromLocal8Bit(loadError)
                                     : QStringLiteral("dlopen returned a null handle.");
            return false;
        }

        resolveOptional(m_handle, api.setUtf8, "SetHcInterfaceStringEncodingIsUtf8");

        if (!resolveRequired(m_handle, api.getErrorText, "get_error_text", errorMessage) ||
            !resolveRequired(m_handle, api.createTuple, "F_create_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.createTupleString, "F_create_tuple_s", errorMessage) ||
            !resolveRequired(m_handle, api.setDouble, "F_set_d", errorMessage) ||
            !resolveRequired(m_handle, api.destroyTuple, "F_destroy_tuple", errorMessage) ||
            !resolveRequired(m_handle, api.getDouble, "F_get_d", errorMessage) ||
            !resolveRequired(m_handle, api.genImage1, "gen_image1", errorMessage) ||
            !resolveRequired(m_handle, api.genImageInterleaved, "gen_image_interleaved", errorMessage) ||
            !resolveRequired(m_handle, api.rgb1ToGray, "rgb1_to_gray", errorMessage) ||
            !resolveRequired(m_handle, api.genRegionPolygonFilled, "T_gen_region_polygon_filled", errorMessage) ||
            !resolveRequired(m_handle, api.reduceDomain, "reduce_domain", errorMessage) ||
            !resolveRequired(m_handle, api.threshold, "threshold", errorMessage) ||
            !resolveRequired(m_handle, api.connection, "connection", errorMessage) ||
            !resolveRequired(m_handle, api.countObj, "count_obj", errorMessage) ||
            !resolveRequired(m_handle, api.selectObj, "select_obj", errorMessage) ||
            !resolveRequired(m_handle, api.smallestRectangle1, "smallest_rectangle1", errorMessage) ||
            !resolveRequired(m_handle, api.selectShape, "select_shape", errorMessage) ||
            !resolveRequired(m_handle, api.sortRegion, "sort_region", errorMessage) ||
            !resolveRequired(m_handle, api.readOcrClassMlp, "T_read_ocr_class_mlp", errorMessage) ||
            !resolveRequired(m_handle, api.doOcrMultiClassMlp, "T_do_ocr_multi_class_mlp", errorMessage) ||
            !resolveRequired(m_handle, api.clearOcrClassMlp, "T_clear_ocr_class_mlp", errorMessage) ||
            !resolveRequired(m_handle, api.getStringUtf8, "F_get_s_to_utf8", errorMessage) ||
            !resolveRequired(m_handle, api.clearObj, "clear_obj", errorMessage)) {
            symbolMissing = true;
            dlclose(m_handle);
            m_handle = nullptr;
            return false;
        }

        if (api.setUtf8)
            api.setUtf8(1);

        return true;
    }

    HalconCApi api;

private:
    void *m_handle = nullptr;
};

} // namespace

OcrHalconResult OcrHalconRunner::run(const cv::Mat &image, const OcrHalconConfig &config)
{
    OcrHalconResult result;
    result.payload.insert(QStringLiteral("halconSoPath"), config.halconSoPath);
    result.payload.insert(QStringLiteral("halconSoPathCandidates"),
                          config.halconSoPathCandidates.join(QStringLiteral("; ")));
    result.payload.insert(QStringLiteral("ocrModelPath"), config.ocrModelPath);
    result.payload.insert(QStringLiteral("ocrModelPathCandidates"),
                          config.ocrModelPathCandidates.join(QStringLiteral("; ")));
    result.payload.insert(QStringLiteral("roi"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("roiNormalized"), rectToJson(config.roiNormalized));
    result.payload.insert(QStringLiteral("expectedText"), config.expectedText);
    result.payload.insert(QStringLiteral("matchRule"), config.matchRule);
    result.payload.insert(QStringLiteral("binaryThreshold"), config.binaryThreshold);
    result.payload.insert(QStringLiteral("polarity"), config.objectPolarity);
    result.payload.insert(QStringLiteral("minCharArea"), config.minCharArea);
    result.payload.insert(QStringLiteral("maxCharArea"), config.maxCharArea);
    result.payload.insert(QStringLiteral("minCharWidth"), config.minCharWidth);
    result.payload.insert(QStringLiteral("minCharHeight"), config.minCharHeight);
    result.payload.insert(QStringLiteral("maxCharWidth"), config.maxCharWidth);
    result.payload.insert(QStringLiteral("maxCharHeight"), config.maxCharHeight);
    result.payload.insert(QStringLiteral("minAspectRatio"), config.minAspectRatio);
    result.payload.insert(QStringLiteral("maxAspectRatio"), config.maxAspectRatio);
    result.payload.insert(QStringLiteral("aspectRatioFeature"), QStringLiteral("ratio_height_width"));
    result.payload.insert(QStringLiteral("minConfidence"), config.minConfidence);

    if (image.empty()) {
        result.status = QStringLiteral("image_empty");
        result.message = QStringLiteral("OCR input image is empty.");
        return result;
    }

    if (config.halconSoPath.trimmed().isEmpty() || !QFileInfo::exists(config.halconSoPath)) {
        const QString triedPaths = config.halconSoPathCandidates.isEmpty()
                ? config.halconSoPath
                : config.halconSoPathCandidates.join(QStringLiteral("; "));
        result.status = QStringLiteral("halcon_so_not_found");
        result.message = QStringLiteral("HALCON runtime file not found: %1. Tried: %2")
                .arg(config.halconSoPath, triedPaths);
        return result;
    }

    if (config.ocrModelPath.trimmed().isEmpty() || !QFileInfo::exists(config.ocrModelPath)) {
        const QString triedPaths = config.ocrModelPathCandidates.isEmpty()
                ? config.ocrModelPath
                : config.ocrModelPathCandidates.join(QStringLiteral("; "));
        result.status = QStringLiteral("model_not_found");
        result.message = QStringLiteral("OCR model file not found: %1. Tried: %2")
                .arg(config.ocrModelPath, triedPaths);
        return result;
    }

    if (image.depth() != CV_8U || (image.channels() != 1 && image.channels() != 3)) {
        result.status = QStringLiteral("unsupported_image_type");
        result.message = QStringLiteral("OCR supports CV_8UC1 and BGR CV_8UC3 images.");
        return result;
    }

    cv::Mat halconFrame = image;
    if (!halconFrame.isContinuous())
        halconFrame = halconFrame.clone();

    const QRect roiPixels = normalizedRoiToPixels(config.roiNormalized, halconFrame.cols, halconFrame.rows);
    if (roiPixels.isEmpty()) {
        result.status = QStringLiteral("invalid_roi");
        result.message = QStringLiteral("OCR ROI is invalid.");
        return result;
    }

    result.payload.insert(QStringLiteral("roiPixels"), rectToJson(QRectF(roiPixels)));
    result.payload.insert(QStringLiteral("roiPixelRect"), rectToJson(QRectF(roiPixels)));
    result.overlays.append(rectOverlay(QRectF(roiPixels), QStringLiteral("ROI")));

    HalconLibrary library;
    QString loadMessage;
    bool symbolMissing = false;
    if (!library.load(config.halconSoPath, loadMessage, symbolMissing)) {
        result.status = symbolMissing ? QStringLiteral("halcon_symbol_missing")
                                      : QStringLiteral("halcon_load_failed");
        result.message = loadMessage;
        return result;
    }

    HalconCApi *api = &library.api;

    Hobject inputImage = NO_OBJECTS;
    Hobject grayImage = NO_OBJECTS;
    Hobject roiRegion = NO_OBJECTS;
    Hobject reducedImage = NO_OBJECTS;
    Hobject thresholdRegion = NO_OBJECTS;
    Hobject connectedRegions = NO_OBJECTS;
    Hobject areaSelectedCharacters = NO_OBJECTS;
    Hobject widthSelectedCharacters = NO_OBJECTS;
    Hobject heightSelectedCharacters = NO_OBJECTS;
    Hobject selectedCharacters = NO_OBJECTS;
    Hobject sortedCharacters = NO_OBJECTS;
    Hobject selectedCharacter = NO_OBJECTS;
    Htuple rowTuple = HTUPLE_INITIALIZER;
    Htuple columnTuple = HTUPLE_INITIALIZER;
    Htuple ocrFileTuple = HTUPLE_INITIALIZER;
    Htuple ocrHandleTuple = HTUPLE_INITIALIZER;
    Htuple classTuple = HTUPLE_INITIALIZER;
    Htuple confidenceTuple = HTUPLE_INITIALIZER;
    bool roiTuplesCreated = false;
    bool ocrFileTupleCreated = false;
    bool ocrHandleCreated = false;
    bool ocrResultTuplesCreated = false;

    auto halconErrorText = [&](const Herror status) {
        char buffer[1024] = {0};
        if (api->getErrorText && halconStatusOk(api->getErrorText(static_cast<Hlong>(status), buffer))) {
            const QString message = QString::fromUtf8(buffer).trimmed();
            if (!message.isEmpty())
                return message;
        }
        return QStringLiteral("HALCON error code %1").arg(static_cast<qlonglong>(status));
    };

    auto clearObject = [&](Hobject &object) {
        if (halconObjectAllocated(object) && api->clearObj)
            api->clearObj(object);
        object = NO_OBJECTS;
    };

    auto destroyTuple = [&](Htuple &tuple) {
        if ((tuple.num > 0 || tuple.capacity > 0) && api->destroyTuple)
            api->destroyTuple(&tuple);
        tuple = HTUPLE_INITIALIZER;
    };

    auto cleanup = [&]() {
        clearObject(selectedCharacter);
        clearObject(sortedCharacters);
        clearObject(selectedCharacters);
        clearObject(heightSelectedCharacters);
        clearObject(widthSelectedCharacters);
        clearObject(areaSelectedCharacters);
        clearObject(connectedRegions);
        clearObject(thresholdRegion);
        clearObject(reducedImage);
        clearObject(roiRegion);
        clearObject(grayImage);
        clearObject(inputImage);

        if (ocrResultTuplesCreated) {
            destroyTuple(confidenceTuple);
            destroyTuple(classTuple);
            ocrResultTuplesCreated = false;
        }
        if (ocrHandleCreated) {
            if (api->clearOcrClassMlp)
                api->clearOcrClassMlp(ocrHandleTuple);
            destroyTuple(ocrHandleTuple);
            ocrHandleCreated = false;
        }
        if (ocrFileTupleCreated) {
            destroyTuple(ocrFileTuple);
            ocrFileTupleCreated = false;
        }
        if (roiTuplesCreated) {
            destroyTuple(columnTuple);
            destroyTuple(rowTuple);
            roiTuplesCreated = false;
        }
    };

    auto checkStatus = [&](const Herror status, const QString &stage) {
        if (!halconStatusOk(status))
            throw std::pair<QString, QString>(QStringLiteral("ocr_failed"),
                                              QStringLiteral("%1: %2").arg(stage, halconErrorText(status)));
    };

    auto syncFilteredAliases = [&]() {
        result.text = result.filteredText;
        result.averageConfidence = result.filteredAverageConfidence;
        result.charCount = result.filteredCharCount;
        result.chars = result.filteredChars;

        result.payload.insert(QStringLiteral("text"), result.text);
        result.payload.insert(QStringLiteral("charCount"), result.charCount);
        result.payload.insert(QStringLiteral("averageConfidence"), result.averageConfidence);
        result.payload.insert(QStringLiteral("chars"), charsToJson(result.chars));
        result.payload.insert(QStringLiteral("rawText"), result.rawText);
        result.payload.insert(QStringLiteral("filteredText"), result.filteredText);
        result.payload.insert(QStringLiteral("rawCharCount"), result.rawCharCount);
        result.payload.insert(QStringLiteral("filteredCharCount"), result.filteredCharCount);
        result.payload.insert(QStringLiteral("rawAverageConfidence"), result.rawAverageConfidence);
        result.payload.insert(QStringLiteral("filteredAverageConfidence"), result.filteredAverageConfidence);
        result.payload.insert(QStringLiteral("rawChars"), charsToJson(result.rawChars));
        result.payload.insert(QStringLiteral("filteredChars"), charsToJson(result.filteredChars));
    };

    try {
        const QPoint points[4] = {
            roiPixels.topLeft(),
            roiPixels.topRight(),
            roiPixels.bottomRight(),
            roiPixels.bottomLeft()
        };

        api->createTuple(&rowTuple, 4);
        api->createTuple(&columnTuple, 4);
        roiTuplesCreated = true;
        for (int i = 0; i < 4; ++i) {
            api->setDouble(&rowTuple, static_cast<double>(points[i].y()), i);
            api->setDouble(&columnTuple, static_cast<double>(points[i].x()), i);
        }

        const int channels = halconFrame.channels();
        if (channels == 1) {
            checkStatus(api->genImage1(&inputImage,
                                       "byte",
                                       halconFrame.cols,
                                       halconFrame.rows,
                                       reinterpret_cast<Hlong>(halconFrame.data)),
                        QStringLiteral("gen_image1"));
        } else {
            checkStatus(api->genImageInterleaved(&inputImage,
                                                 reinterpret_cast<Hlong>(halconFrame.data),
                                                 "bgr",
                                                 halconFrame.cols,
                                                 halconFrame.rows,
                                                 0,
                                                 "byte",
                                                 0,
                                                 0,
                                                 0,
                                                 0,
                                                 8,
                                                 0),
                        QStringLiteral("gen_image_interleaved"));
            checkStatus(api->rgb1ToGray(inputImage, &grayImage),
                        QStringLiteral("rgb1_to_gray"));
        }

        const Hobject graySource = channels == 1 ? inputImage : grayImage;
        checkStatus(api->genRegionPolygonFilled(&roiRegion, rowTuple, columnTuple),
                    QStringLiteral("gen_region_polygon_filled"));
        checkStatus(api->reduceDomain(graySource, roiRegion, &reducedImage),
                    QStringLiteral("reduce_domain"));

        const int thresholdValue = qBound(0, config.binaryThreshold, 255);
        const double minCharArea = std::max(1.0, config.minCharArea);
        const double maxCharArea = std::max(minCharArea, config.maxCharArea);
        const double minCharWidth = std::max(0.0, config.minCharWidth);
        const double minCharHeight = std::max(0.0, config.minCharHeight);
        const double maxCharWidth = std::max(minCharWidth, config.maxCharWidth);
        const double maxCharHeight = std::max(minCharHeight, config.maxCharHeight);
        const double minAspectRatio = std::max(0.0, config.minAspectRatio);
        const double maxAspectRatio = std::max(minAspectRatio, config.maxAspectRatio);
        const QString requestedPolarity = config.objectPolarity.trimmed().toLower();
        QString usedPolarity;

        auto extractCharacters = [&](const QString &polarity) {
            clearObject(sortedCharacters);
            clearObject(selectedCharacters);
            clearObject(heightSelectedCharacters);
            clearObject(widthSelectedCharacters);
            clearObject(areaSelectedCharacters);
            clearObject(connectedRegions);
            clearObject(thresholdRegion);

            if (polarity == QStringLiteral("bright")) {
                checkStatus(api->threshold(reducedImage, &thresholdRegion, thresholdValue, 255),
                            QStringLiteral("threshold(bright)"));
            } else {
                checkStatus(api->threshold(reducedImage, &thresholdRegion, 0, thresholdValue),
                            QStringLiteral("threshold(dark)"));
            }

            checkStatus(api->connection(thresholdRegion, &connectedRegions),
                        QStringLiteral("connection"));
            checkStatus(api->selectShape(connectedRegions,
                                         &areaSelectedCharacters,
                                         "area",
                                         "and",
                                         minCharArea,
                                         maxCharArea),
                        QStringLiteral("select_shape(area)"));
            checkStatus(api->selectShape(areaSelectedCharacters,
                                         &widthSelectedCharacters,
                                         "width",
                                         "and",
                                         minCharWidth,
                                         maxCharWidth),
                        QStringLiteral("select_shape(width)"));
            checkStatus(api->selectShape(widthSelectedCharacters,
                                         &heightSelectedCharacters,
                                         "height",
                                         "and",
                                         minCharHeight,
                                         maxCharHeight),
                        QStringLiteral("select_shape(height)"));
            checkStatus(api->selectShape(heightSelectedCharacters,
                                         &selectedCharacters,
                                         "ratio",
                                         "and",
                                         minAspectRatio,
                                         maxAspectRatio),
                        QStringLiteral("select_shape(ratio)"));
            checkStatus(api->sortRegion(selectedCharacters,
                                        &sortedCharacters,
                                        "character",
                                        "true",
                                        "row"),
                        QStringLiteral("sort_region"));

            Hlong count = 0;
            checkStatus(api->countObj(sortedCharacters, &count), QStringLiteral("count_obj"));
            return count;
        };

        Hlong segmentedCount = 0;
        if (requestedPolarity == QStringLiteral("auto")) {
            segmentedCount = extractCharacters(QStringLiteral("dark"));
            usedPolarity = QStringLiteral("dark");
            if (segmentedCount <= 0) {
                segmentedCount = extractCharacters(QStringLiteral("bright"));
                usedPolarity = QStringLiteral("bright");
            }
        } else if (requestedPolarity == QStringLiteral("bright")) {
            segmentedCount = extractCharacters(QStringLiteral("bright"));
            usedPolarity = QStringLiteral("bright");
        } else {
            segmentedCount = extractCharacters(QStringLiteral("dark"));
            usedPolarity = QStringLiteral("dark");
        }

        result.payload.insert(QStringLiteral("usedPolarity"), usedPolarity);
        result.payload.insert(QStringLiteral("segmentedCharCount"), static_cast<int>(segmentedCount));

        if (segmentedCount <= 0) {
            result.success = true;
            result.status = QStringLiteral("no_characters");
            result.message = QStringLiteral("OCR completed, but no characters were detected.");
            syncFilteredAliases();
            cleanup();
            return result;
        }

        const QByteArray modelPathBytes = config.ocrModelPath.toUtf8();
        api->createTupleString(&ocrFileTuple, modelPathBytes.constData());
        ocrFileTupleCreated = true;
        checkStatus(api->readOcrClassMlp(ocrFileTuple, &ocrHandleTuple),
                    QStringLiteral("read_ocr_class_mlp"));
        ocrHandleCreated = true;
        checkStatus(api->doOcrMultiClassMlp(sortedCharacters,
                                           graySource,
                                           ocrHandleTuple,
                                           &classTuple,
                                           &confidenceTuple),
                    QStringLiteral("do_ocr_multi_class_mlp"));
        ocrResultTuplesCreated = true;

        const int resultCount = qMin<int>(static_cast<int>(classTuple.num),
                                          static_cast<int>(confidenceTuple.num));
        if (resultCount <= 0) {
            result.success = true;
            result.status = QStringLiteral("no_characters");
            result.message = QStringLiteral("OCR completed, but no characters were recognized.");
            syncFilteredAliases();
            cleanup();
            return result;
        }

        double rawConfidenceSum = 0.0;
        result.rawChars.reserve(resultCount);
        for (int index = 0; index < resultCount; ++index) {
            QByteArray buffer(256, char(0));
            api->getStringUtf8(buffer.data(), buffer.size(), &classTuple, index);

            OcrCharResult charResult;
            charResult.character = QString::fromUtf8(buffer.constData()).trimmed();
            charResult.confidence = api->getDouble(&confidenceTuple, index);
            result.rawText += charResult.character;
            rawConfidenceSum += charResult.confidence;
            result.rawChars.append(charResult);
        }
        result.rawCharCount = result.rawChars.size();
        result.rawAverageConfidence = result.rawCharCount > 0
                ? rawConfidenceSum / static_cast<double>(result.rawCharCount)
                : 0.0;

        for (int index = 0; index < result.rawChars.size(); ++index) {
            clearObject(selectedCharacter);
            checkStatus(api->selectObj(sortedCharacters, &selectedCharacter, index + 1),
                        QStringLiteral("select_obj"));

            Hlong row1 = 0;
            Hlong column1 = 0;
            Hlong row2 = 0;
            Hlong column2 = 0;
            checkStatus(api->smallestRectangle1(selectedCharacter, &row1, &column1, &row2, &column2),
                        QStringLiteral("smallest_rectangle1"));

            const QRectF charBox(static_cast<double>(column1),
                                 static_cast<double>(row1),
                                 static_cast<double>(column2 - column1 + 1),
                                 static_cast<double>(row2 - row1 + 1));
            result.rawChars[index].box = charBox;
        }

        const double minConfidence = qBound(0.0, config.minConfidence, 1.0);
        double filteredConfidenceSum = 0.0;
        result.filteredChars.reserve(result.rawChars.size());
        for (const OcrCharResult &charResult : result.rawChars) {
            if (charResult.confidence < minConfidence)
                continue;

            result.filteredText += charResult.character;
            filteredConfidenceSum += charResult.confidence;
            result.filteredChars.append(charResult);
        }
        result.filteredCharCount = result.filteredChars.size();
        result.filteredAverageConfidence = result.filteredCharCount > 0
                ? filteredConfidenceSum / static_cast<double>(result.filteredCharCount)
                : 0.0;
        syncFilteredAliases();

        result.success = true;
        if (result.filteredCharCount <= 0) {
            result.status = QStringLiteral("no_characters");
            result.message = QStringLiteral("No characters passed confidence filter.");
            cleanup();
            return result;
        }

        result.status = QStringLiteral("ok");
        result.message = QStringLiteral("OCR completed.");
        for (int index = 0; index < result.filteredChars.size(); ++index) {
            const OcrCharResult &charResult = result.filteredChars.at(index);
            result.overlays.append(rectOverlay(charResult.box,
                                               QStringLiteral("#%1").arg(index + 1),
                                               charResult.confidence));
        }
        result.overlays.append(textOverlay(QPointF(16.0, 28.0),
                                           result.filteredText,
                                           QStringLiteral("OCR"),
                                           result.filteredAverageConfidence));
        syncFilteredAliases();

        cleanup();
        return result;
    } catch (const std::pair<QString, QString> &errorInfo) {
        cleanup();
        result.success = false;
        result.status = errorInfo.first;
        result.message = errorInfo.second;
        return result;
    } catch (const std::exception &error) {
        cleanup();
        result.success = false;
        result.status = QStringLiteral("ocr_failed");
        result.message = QString::fromLocal8Bit(error.what());
        return result;
    }
}
