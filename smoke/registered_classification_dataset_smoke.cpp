#include "algorithms/recognition/RegisteredClassificationHalconRunner.h"
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "algorithms/recognition/RegisteredClassificationTrainingRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <opencv2/imgcodecs.hpp>

#include <iostream>

namespace {

struct DatasetClass
{
    int classId = -1;
    QString label;
    QStringList imagePaths;
};

QString defaultDatasetRoot()
{
    return QDir::current().filePath(
                QStringLiteral("tests/Images/hikrobot-presence-tests/"
                               "hikrobot-registration-classification"));
}

QString modelOutputDir()
{
    const QString path = QDir(QDir::tempPath()).filePath(
                QStringLiteral("registered_classification_dataset_smoke/model"));
    QDir(QFileInfo(path).absolutePath()).removeRecursively();
    QDir().mkpath(path);
    return path;
}

RegisteredClassificationFeatureRegion datasetRegion()
{
    RegisteredClassificationFeatureRegion region;
    region.type = QStringLiteral("rectangle");
    region.rectNormalized = QRectF(0.18, 0.18, 0.64, 0.64);
    return region;
}

QVector<DatasetClass> loadDataset(const QString &rootPath, QString *error)
{
    QVector<DatasetClass> classes;
    const QDir root(rootPath);
    if (!root.exists()) {
        if (error)
            *error = QStringLiteral("dataset directory does not exist: %1").arg(rootPath);
        return classes;
    }

    const QStringList classDirs = root.entryList(
                QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (int classId = 0; classId < classDirs.size(); ++classId) {
        const QString classDirName = classDirs.at(classId);
        const QDir classDir(root.filePath(classDirName));
        const QStringList fileNames = classDir.entryList(
                    {QStringLiteral("*.png")}, QDir::Files, QDir::Name);
        if (fileNames.size() < 6) {
            if (error) {
                *error = QStringLiteral("class %1 requires at least 6 PNG images, found %2")
                        .arg(classDirName)
                        .arg(fileNames.size());
            }
            return {};
        }

        DatasetClass datasetClass;
        datasetClass.classId = classId;
        datasetClass.label = classDirName;
        for (const QString &fileName : fileNames)
            datasetClass.imagePaths.append(classDir.filePath(fileName));
        classes.append(datasetClass);
    }

    if (classes.size() < 2 && error)
        *error = QStringLiteral("dataset requires at least two classes");
    return classes.size() >= 2 ? classes : QVector<DatasetClass>();
}

cv::Mat readImage(const QString &path)
{
    return cv::imread(path.toLocal8Bit().constData(), cv::IMREAD_COLOR);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString datasetRoot = argc > 1
            ? QString::fromLocal8Bit(argv[1])
            : defaultDatasetRoot();

    QString datasetError;
    const QVector<DatasetClass> classes = loadDataset(datasetRoot, &datasetError);
    if (classes.isEmpty()) {
        std::cerr << "FAIL: " << datasetError.toStdString() << std::endl;
        return 1;
    }

    const QVector<int> trainingIndexes = {0, 2, 4};
    const QVector<int> testIndexes = {1, 3, 5};
    RegisteredClassificationTrainingRequest request;
    request.outputModelDir = modelOutputDir();
    request.thresholds.minSimilarity = 80;
    request.thresholds.minMargin = 8;

    for (const DatasetClass &datasetClass : classes) {
        request.classLabels.append(
                    RegisteredClassificationClassLabel{
                        datasetClass.classId,
                        datasetClass.label});
        for (int imageIndex : trainingIndexes) {
            RegisteredClassificationTrainingSample sample;
            sample.image = readImage(datasetClass.imagePaths.at(imageIndex));
            if (sample.image.empty()) {
                std::cerr << "FAIL: cannot read training image "
                          << datasetClass.imagePaths.at(imageIndex).toStdString()
                          << std::endl;
                return 1;
            }
            sample.region = datasetRegion();
            sample.classId = datasetClass.classId;
            request.samples.append(sample);
        }
    }

    RegisteredClassificationTrainingRunner trainer;
    const RegisteredClassificationTrainingResult training = trainer.train(request);
    if (!training.success) {
        std::cerr << "FAIL: training " << training.status.toStdString()
                  << " | " << training.message.toStdString() << std::endl;
        return 1;
    }
    if (!validateRegisteredClassificationKnnPackage(request.outputModelDir).success) {
        std::cerr << "FAIL: trained model package is incomplete" << std::endl;
        return 1;
    }

    RegisteredClassificationHalconConfig config;
    config.modelPath = request.outputModelDir;
    config.modelName = QStringLiteral("hikrobot-registration-classification");
    config.modelType = registeredClassificationKnnModelType();
    config.detectRegionType = QStringLiteral("rectangle");
    config.roiNormalized = datasetRegion().rectNormalized;
    config.topK = classes.size();
    config.judgeMode = QStringLiteral("class_match");
    config.minSimilarity = 80;
    config.minMargin = 8;

    int correct = 0;
    int rejected = 0;
    int wrong = 0;
    int runtimeErrors = 0;
    RegisteredClassificationHalconRunner runner;

    for (const DatasetClass &datasetClass : classes) {
        for (int imageIndex : testIndexes) {
            const QString imagePath = datasetClass.imagePaths.at(imageIndex);
            const cv::Mat image = readImage(imagePath);
            if (image.empty()) {
                ++runtimeErrors;
                std::cerr << "ERROR read " << imagePath.toStdString() << std::endl;
                continue;
            }

            config.expectedLabel = datasetClass.label;
            const RegisteredClassificationHalconResult result = runner.run(image, config);
            const QString fileName = QFileInfo(imagePath).fileName();
            if (!result.success) {
                ++runtimeErrors;
                std::cout << "ERROR class=" << datasetClass.label.toStdString()
                          << " image=" << fileName.toStdString()
                          << " status=" << result.status.toStdString()
                          << std::endl;
            } else if (result.predictedClassId == datasetClass.classId) {
                ++correct;
                std::cout << "CORRECT class=" << datasetClass.label.toStdString()
                          << " image=" << fileName.toStdString()
                          << " score=" << result.score
                          << " margin=" << result.scoreMargin
                          << std::endl;
            } else if (result.predictedClassId < 0) {
                ++rejected;
                std::cout << "REJECT class=" << datasetClass.label.toStdString()
                          << " image=" << fileName.toStdString()
                          << " reason=" << result.rejectionReason.toStdString()
                          << " score=" << result.score
                          << " margin=" << result.scoreMargin
                          << std::endl;
            } else {
                ++wrong;
                std::cout << "WRONG expected=" << datasetClass.label.toStdString()
                          << " predicted=" << result.predictedLabel.toStdString()
                          << " image=" << fileName.toStdString()
                          << " score=" << result.score
                          << " margin=" << result.scoreMargin
                          << std::endl;
            }
        }
    }

    const int total = classes.size() * testIndexes.size();
    std::cout << "SUMMARY total=" << total
              << " correct=" << correct
              << " rejected=" << rejected
              << " wrong=" << wrong
              << " runtime_errors=" << runtimeErrors
              << std::endl;

    if (runtimeErrors > 0)
        return 1;
    if (correct + rejected + wrong != total)
        return 1;
    return 0;
}
