#include "HalconCpp.h"
#include "HOperatorSet.h"
#include "HDevEngineCpp.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

using namespace HalconCpp;
using namespace HDevEngineCpp;

namespace {

QString sha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return QString();
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(hash.result().toHex()));
}

bool writeJson(const QString &path, const QJsonObject &object)
{
    QFile file(path);
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
            && file.write(bytes) == bytes.size();
}

HTuple createPreprocessParam(const HTuple &model, const QString &procedureRoot)
{
    HDevEngine().SetProcedurePath(procedureRoot.toUtf8().constData());
    HDevProcedure procedure("create_dl_preprocess_param_from_model");
    HDevProcedureCall call(procedure);
    call.SetInputCtrlParamTuple("DLModelHandle", model);
    call.SetInputCtrlParamTuple("NormalizationType", "constant_values");
    call.SetInputCtrlParamTuple("DomainHandling", "full_domain");
    call.SetInputCtrlParamTuple("SetBackgroundID", HTuple());
    call.SetInputCtrlParamTuple("ClassIDsBackground", HTuple());
    call.SetInputCtrlParamTuple("GenParam", HTuple());
    call.Execute();
    return call.GetOutputCtrlParamTuple("DLPreprocessParam");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 5) {
        std::cerr << "Usage: registered_classification_public_backbone_provision "
                     "<model.onnx> <feature-model-root> <embedding-layer> <embedding-length>"
                  << std::endl;
        return 2;
    }

    const QString sourceModel = QString::fromLocal8Bit(argv[1]);
    const QString featureModelRoot = QString::fromLocal8Bit(argv[2]);
    const QString embeddingLayer = QString::fromLocal8Bit(argv[3]);
    bool lengthOk = false;
    const int embeddingLength = QString::fromLocal8Bit(argv[4]).toInt(&lengthOk);
    if (!lengthOk || embeddingLength <= 0) {
        std::cerr << "Embedding length must be a positive integer." << std::endl;
        return 2;
    }

    const QString modelId = QStringLiteral("public_mobilenet_v2");
    const QString modelVersion = QStringLiteral("onnx_model_zoo_opset7");
    const QString modelDirectory = QDir(featureModelRoot).filePath(
                modelId + QLatin1Char('/') + modelVersion);
    if (!QDir().mkpath(modelDirectory)) {
        std::cerr << "Cannot create model directory." << std::endl;
        return 1;
    }

    const QString modelPath = QDir(modelDirectory).filePath(QStringLiteral("feature_model.hdl"));
    const QString preprocessPath = QDir(modelDirectory).filePath(
                QStringLiteral("preprocess_params.hdict"));
    const QString descriptorPath = QDir(modelDirectory).filePath(
                QStringLiteral("descriptor.json"));

    try {
        HTuple model;
        ReadDlModel(sourceModel.toUtf8().constData(), &model);
        HTuple modelType;
        GetDlModelParam(model, "type", &modelType);
        if (modelType.Length() == 1
                && QString::fromUtf8(modelType[0].S().Text()) == QStringLiteral("generic")) {
            SetDlModelParam(model, "type", "classification");
        }

        HTuple width;
        HTuple height;
        HTuple channels;
        GetDlModelParam(model, "image_width", &width);
        GetDlModelParam(model, "image_height", &height);
        GetDlModelParam(model, "image_num_channels", &channels);

        const QString halconRoot = QString::fromLocal8Bit(qgetenv("HALCONROOT")).isEmpty()
                ? QStringLiteral("/opt/halcon")
                : QString::fromLocal8Bit(qgetenv("HALCONROOT"));
        const HTuple preprocess = createPreprocessParam(
                    model, QDir(halconRoot).filePath(QStringLiteral("procedures")));

        WriteDlModel(model, modelPath.toUtf8().constData());
        WriteDict(preprocess, preprocessPath.toUtf8().constData(), HTuple(), HTuple());
        ClearDlModel(model);

        const QString modelHash = sha256(modelPath);
        const QString preprocessHash = sha256(preprocessPath);
        if (modelHash.isEmpty() || preprocessHash.isEmpty()) {
            std::cerr << "Cannot hash provisioned files." << std::endl;
            return 1;
        }

        const QJsonObject descriptor{
            {QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("modelId"), modelId},
            {QStringLiteral("modelVersion"), modelVersion},
            {QStringLiteral("featureModelFile"), QStringLiteral("feature_model.hdl")},
            {QStringLiteral("featureModelSha256"), modelHash},
            {QStringLiteral("preprocessFile"), QStringLiteral("preprocess_params.hdict")},
            {QStringLiteral("preprocessSha256"), preprocessHash},
            {QStringLiteral("embeddingLayer"), embeddingLayer},
            {QStringLiteral("embeddingLength"), embeddingLength},
            {QStringLiteral("inputWidth"), static_cast<int>(width[0].I())},
            {QStringLiteral("inputHeight"), static_cast<int>(height[0].I())},
            {QStringLiteral("inputChannels"), static_cast<int>(channels[0].I())},
            {QStringLiteral("resizeMode"), QStringLiteral("keep_aspect_pad")},
            {QStringLiteral("paddingValue"), 0.0},
            {QStringLiteral("runtimePreference"), QStringLiteral("cpu")}
        };
        if (!writeJson(descriptorPath, descriptor)) {
            std::cerr << "Cannot write descriptor." << std::endl;
            return 1;
        }

        std::cout << "Provisioned: " << modelDirectory.toStdString() << "\n"
                  << "Model SHA-256: " << modelHash.toStdString() << "\n"
                  << "Preprocess SHA-256: " << preprocessHash.toStdString() << std::endl;
        return 0;
    } catch (const HDevEngineException &exception) {
        std::cerr << "HDevEngine error: " << exception.Message() << std::endl;
        return 1;
    } catch (const HException &exception) {
        std::cerr << "HALCON error " << exception.ErrorCode() << ": "
                  << exception.ErrorMessage().Text() << std::endl;
        return 1;
    }
}
