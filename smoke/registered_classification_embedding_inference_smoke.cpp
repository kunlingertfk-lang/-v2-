#include "algorithms/recognition/RegisteredClassificationEmbeddingModelProvider.h"

#include "HalconCpp.h"
#include "HOperatorSet.h"
#include "HDevEngineCpp.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>

#include <cmath>
#include <iostream>

using namespace HalconCpp;
using namespace HDevEngineCpp;

namespace {

HTuple makeDlSample(const HObject &image, const HTuple &preprocess)
{
    HDevProcedure sampleProcedure("gen_dl_samples_from_images");
    HDevProcedureCall sampleCall(sampleProcedure);
    sampleCall.SetInputIconicParamObject("Images", image);
    sampleCall.Execute();
    const HTuple sample = sampleCall.GetOutputCtrlParamTuple("DLSampleBatch");

    HDevProcedure preprocessProcedure("preprocess_dl_samples");
    HDevProcedureCall preprocessCall(preprocessProcedure);
    preprocessCall.SetInputCtrlParamTuple("DLSampleBatch", sample);
    preprocessCall.SetInputCtrlParamTuple("DLPreprocessParam", preprocess);
    preprocessCall.Execute();
    return sample;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 3) {
        std::cerr << "Usage: registered_classification_embedding_inference_smoke "
                     "<feature-model-root> <image>"
                  << std::endl;
        return 2;
    }

    const QString root = QString::fromLocal8Bit(argv[1]);
    const QString imagePath = QString::fromLocal8Bit(argv[2]);
    RegisteredClassificationEmbeddingModelProvider provider(root);
    const RegisteredClassificationEmbeddingModelResolveResult resolved =
            provider.resolve(QStringLiteral("public_mobilenet_v2"),
                             QStringLiteral("onnx_model_zoo_opset7"));
    if (!resolved.success) {
        std::cerr << resolved.status.toStdString() << ": "
                  << resolved.message.toStdString() << std::endl;
        return 1;
    }

    try {
        const QByteArray procedurePath = QDir(QStringLiteral("/opt/halcon")).filePath(
                    QStringLiteral("procedures")).toUtf8();
        HDevEngine().SetProcedurePath(procedurePath.constData());

        HTuple model;
        ReadDlModel(resolved.descriptor.modelPath.toUtf8().constData(), &model);
        SetDlModelParam(model, "batch_size", 1);
        HTuple devices;
        QueryAvailableDlDevices("runtime", "cpu", &devices);
        if (devices.Length() < 1) {
            std::cerr << "No HALCON CPU deep-learning device." << std::endl;
            ClearDlModel(model);
            return 1;
        }
        SetDlModelParam(model, "device", devices[0]);
        SetDlModelParam(model, "extract_feature_maps",
                        resolved.descriptor.embeddingLayer.toUtf8().constData());

        HTuple preprocess;
        ReadDict(resolved.descriptor.preprocessPath.toUtf8().constData(),
                 HTuple(), HTuple(), &preprocess);
        HObject image;
        ReadImage(&image, imagePath.toUtf8().constData());
        const HTuple sample = makeDlSample(image, preprocess);

        QElapsedTimer timer;
        timer.start();
        HTuple result;
        ApplyDlModel(model, sample, HTuple(), &result);
        const qint64 elapsedMs = timer.elapsed();
        if (result.Length() != 1) {
            std::cerr << "Unexpected DL result count: " << result.Length() << std::endl;
            ClearDlModel(model);
            return 1;
        }

        HObject featureMaps;
        GetDictObject(&featureMaps, result[0],
                      resolved.descriptor.embeddingLayer.toUtf8().constData());
        HTuple width;
        HTuple height;
        HTuple channels;
        GetImageSize(featureMaps, &width, &height);
        CountChannels(featureMaps, &channels);
        HTuple embedding;
        GetGrayval(featureMaps, 0, 0, &embedding);

        double normSquared = 0.0;
        bool finite = embedding.Length() == resolved.descriptor.embeddingLength;
        for (Hlong index = 0; index < embedding.Length(); ++index) {
            const double value = embedding[index].D();
            finite = finite && std::isfinite(value);
            normSquared += value * value;
        }
        const double norm = std::sqrt(normSquared);
        const bool shapeOk = width[0].I() == 1
                && height[0].I() == 1
                && channels[0].I() == resolved.descriptor.embeddingLength;
        const bool pass = shapeOk && finite && norm > 0.0;

        std::cout << "embedding_shape=" << width[0].I() << "x" << height[0].I()
                  << "x" << channels[0].I()
                  << ", length=" << embedding.Length()
                  << ", l2_norm=" << norm
                  << ", inference_ms=" << elapsedMs << std::endl;
        std::cout << "registered_classification_embedding_inference_smoke: "
                  << (pass ? "PASS" : "FAIL") << std::endl;
        ClearDlModel(model);
        return pass ? 0 : 1;
    } catch (const HDevEngineException &exception) {
        std::cerr << "HDevEngine error: " << exception.Message() << std::endl;
        return 1;
    } catch (const HException &exception) {
        std::cerr << "HALCON error " << exception.ErrorCode() << ": "
                  << exception.ErrorMessage().Text() << std::endl;
        return 1;
    }
}
