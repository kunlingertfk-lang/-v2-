# Registered Classification HALCON MLP Backend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the registered classification backend with a HALCON classic MLP training and inference pipeline.

**Architecture:** Split the backend into focused units: model package metadata, deterministic ROI feature extraction, HALCON MLP training, HALCON MLP inference, adapter parsing, and UI training handoff. The first tasks are backend-only and smoke-testable with synthetic images; the final tasks connect the existing training dialog and update user-facing docs.

**Tech Stack:** Qt 5.15.2, qmake, C++17, OpenCV as image container/bridge only, HALCON 24.11 C API loaded through the project’s existing runtime-path pattern, smoke tests under `smoke/`.

## Global Constraints

- All visual algorithm core logic must use HALCON; do not use OpenCV, custom classifiers, ONNX Runtime, TensorRT, or other non-HALCON classification engines.
- `halcon_dl_classification` is no longer supported for registered classification; old DL configs must return `unsupported_model_type`.
- Do not parse or emulate Hikvision `.scbin`; return unsupported errors instead.
- Do not implement template matching, `create_shape_model`, `find_shape_model`, pose normalization, or position correction in this backend.
- OpenCV may only be used for `cv::Mat` image storage, acquisition, display, and format bridging before HALCON processing.
- Use the existing `HalconRuntimePaths` runtime/license path logic.
- Build in `build/` or smoke shadow output directories; do not add qmake build products to git.
- Keep changes scoped to registered classification backend, smoke tests, project file entries, and registered-classification docs.

---

## File Structure

- Create `src/algorithms/recognition/RegisteredClassificationModelPackage.h/.cpp`: metadata structs, JSON read/write, feature-version constants, model package path validation.
- Create `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h/.cpp`: HALCON-backed ROI feature extraction with `featureVersion=halcon_mlp_roi_stats_v1`.
- Create `src/algorithms/recognition/RegisteredClassificationTrainingRunner.h/.cpp`: training request/result structs and HALCON MLP model package creation.
- Replace `src/algorithms/recognition/RegisteredClassificationHalconRunner.h/.cpp`: inference runner that reads model package, extracts features, calls `classify_class_mlp`, and emits `ToolResult`-ready payload.
- Modify `src/tooladapters/RegisteredClassificationAdapter.cpp`: parse MLP model type and reject old DL model type.
- Modify `src/RegisteredClassificationTrainingDialog.h/.cpp`: expose a backend-safe training request handoff only after backend smoke passes.
- Modify `qt_ui_test.pro`: add new backend source/header files.
- Create `smoke/registered_classification_mlp_backend_smoke.cpp/.pro`: backend model package, training, inference, and old-DL rejection smoke.
- Modify `smoke/registered_classification_adapter_smoke.cpp/.pro`: update old DL expectations and add model-package paths.
- Modify docs: `docs/FID/RegisteredClassification/registered_classification_function_implementation.md` and `docs/FID/RegisteredClassification/注册分类提示词规范.md`.

---

### Task 1: Model Package Metadata

**Files:**
- Create: `src/algorithms/recognition/RegisteredClassificationModelPackage.h`
- Create: `src/algorithms/recognition/RegisteredClassificationModelPackage.cpp`
- Create: `smoke/registered_classification_mlp_backend_smoke.cpp`
- Create: `smoke/registered_classification_mlp_backend_smoke.pro`
- Modify: `qt_ui_test.pro`

**Interfaces:**
- Produces: `RegisteredClassificationModelMetadata`, `RegisteredClassificationClassLabel`, `RegisteredClassificationMlpParams`, `RegisteredClassificationThresholds`.
- Produces: `QString registeredClassificationMlpModelType()`, `QString registeredClassificationFeatureVersionV1()`.
- Produces: `RegisteredClassificationModelPackageResult writeRegisteredClassificationMetadata(const QString&, const RegisteredClassificationModelMetadata&)`.
- Produces: `RegisteredClassificationModelPackageResult readRegisteredClassificationMetadata(const QString&, RegisteredClassificationModelMetadata*)`.
- Later tasks consume `metadata.featureLength`, `metadata.featureNames`, `metadata.classLabels`, `metadata.thresholds`, and package paths.

- [ ] **Step 1: Write the failing metadata smoke**

Replace `smoke/registered_classification_mlp_backend_smoke.cpp` with:

```cpp
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonObject>
#include <iostream>

namespace {
int g_failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++g_failures;
    }
}

QString tempModelDir()
{
    QDir dir(QDir::tempPath());
    const QString path = dir.filePath(QStringLiteral("registered_classification_mlp_backend_smoke_model"));
    QDir(path).removeRecursively();
    dir.mkpath(path);
    return path;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    RegisteredClassificationModelMetadata metadata;
    metadata.modelType = registeredClassificationMlpModelType();
    metadata.schemaVersion = 1;
    metadata.featureVersion = registeredClassificationFeatureVersionV1();
    metadata.featureNames = registeredClassificationFeatureNamesV1();
    metadata.featureLength = metadata.featureNames.size();
    metadata.classLabels = {
        {0, QStringLiteral("OK")},
        {1, QStringLiteral("NG")}
    };
    metadata.thresholds.minScore = 80;
    metadata.thresholds.rejectScore = 60;
    metadata.thresholds.top2Gap = 0;
    metadata.mlp.numHidden = 16;
    metadata.mlp.maxIterations = 100;
    metadata.mlp.randSeed = 42;
    metadata.trainingSampleCount = 6;

    const QString modelDir = tempModelDir();
    const RegisteredClassificationModelPackageResult writeResult =
            writeRegisteredClassificationMetadata(modelDir, metadata);
    check(writeResult.success, "metadata write must succeed");
    check(QFileInfo(QDir(modelDir).filePath(QStringLiteral("metadata.json"))).exists(),
          "metadata.json must exist");

    RegisteredClassificationModelMetadata loaded;
    const RegisteredClassificationModelPackageResult readResult =
            readRegisteredClassificationMetadata(modelDir, &loaded);
    check(readResult.success, "metadata read must succeed");
    check(loaded.modelType == registeredClassificationMlpModelType(), "modelType must round-trip");
    check(loaded.featureVersion == registeredClassificationFeatureVersionV1(), "featureVersion must round-trip");
    check(loaded.featureLength == 28, "featureLength must be 28 for v1");
    check(loaded.featureNames.size() == loaded.featureLength, "featureNames count must match featureLength");
    check(loaded.classLabels.size() == 2, "classLabels must round-trip");
    check(loaded.classLabels.value(1).name == QStringLiteral("NG"), "class label name must round-trip");
    check(loaded.thresholds.rejectScore == 60, "rejectScore must round-trip");

    RegisteredClassificationModelMetadata invalid = metadata;
    invalid.featureLength = 27;
    const RegisteredClassificationModelPackageResult invalidWrite =
            writeRegisteredClassificationMetadata(QDir(modelDir).filePath(QStringLiteral("invalid")), invalid);
    check(!invalidWrite.success, "featureLength mismatch must fail");
    check(invalidWrite.status == QStringLiteral("feature_length_mismatch"),
          "featureLength mismatch status must be feature_length_mismatch");

    RegisteredClassificationModelMetadata missing;
    const RegisteredClassificationModelPackageResult missingRead =
            readRegisteredClassificationMetadata(QDir(modelDir).filePath(QStringLiteral("missing")), &missing);
    check(!missingRead.success, "missing metadata must fail");
    check(missingRead.status == QStringLiteral("model_file_not_found"),
          "missing metadata status must be model_file_not_found");

    if (g_failures > 0)
        return 1;
    std::cout << "registered_classification_mlp_backend_smoke: metadata checks passed" << std::endl;
    return 0;
}
```

Create `smoke/registered_classification_mlp_backend_smoke.pro`:

```qmake
QT += core
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = registered_classification_mlp_backend_smoke

BUILD_ROOT = $$_PRO_FILE_PWD_/../build/smoke/registered_classification_mlp_backend
DESTDIR = $$BUILD_ROOT/bin
OBJECTS_DIR = $$BUILD_ROOT/obj
MOC_DIR = $$BUILD_ROOT/moc
RCC_DIR = $$BUILD_ROOT/rcc
UI_DIR = $$BUILD_ROOT/ui
system(mkdir -p $$DESTDIR $$OBJECTS_DIR $$MOC_DIR $$RCC_DIR $$UI_DIR)

INCLUDEPATH += ../src
OPENCV_ROOT = /home/tt/.local/opencv-4.8.0
INCLUDEPATH += $$OPENCV_ROOT/include/opencv4
HALCON_ROOT = $$(HALCONROOT)
!exists($$HALCON_ROOT/include/HalconC.h) {
    HALCON_ROOT = /home/tt/tfk/WorkerSpace/Software/HALCON-24.11.1.0-Progress-Steady
}
INCLUDEPATH += $$HALCON_ROOT/include

LIBS += -L$$OPENCV_ROOT/lib
LIBS += -Wl,-rpath,$$OPENCV_ROOT/lib
LIBS += -lopencv_core -lopencv_imgproc -ldl

SOURCES += \
    registered_classification_mlp_backend_smoke.cpp \
    ../src/algorithms/recognition/RegisteredClassificationModelPackage.cpp

HEADERS += \
    ../src/algorithms/recognition/RegisteredClassificationModelPackage.h
```

- [ ] **Step 2: Run the smoke to verify it fails**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro
make -j$(nproc)
```

Expected: compile fails because `RegisteredClassificationModelPackage.h` does not exist.

- [ ] **Step 3: Implement metadata package files**

Create `src/algorithms/recognition/RegisteredClassificationModelPackage.h`:

```cpp
#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONMODELPACKAGE_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONMODELPACKAGE_H

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct RegisteredClassificationClassLabel
{
    int id = -1;
    QString name;
};

struct RegisteredClassificationMlpParams
{
    int numHidden = 16;
    int maxIterations = 200;
    int randSeed = 42;
};

struct RegisteredClassificationThresholds
{
    int minScore = 80;
    int rejectScore = 60;
    int top2Gap = 0;
};

struct RegisteredClassificationModelMetadata
{
    QString modelType;
    int schemaVersion = 1;
    QString featureVersion;
    QString halconVersion;
    QVector<RegisteredClassificationClassLabel> classLabels;
    QStringList featureNames;
    int featureLength = 0;
    QJsonObject preprocess;
    RegisteredClassificationMlpParams mlp;
    RegisteredClassificationThresholds thresholds;
    int trainingSampleCount = 0;
};

struct RegisteredClassificationModelPackageResult
{
    bool success = false;
    QString status;
    QString message;
    QJsonObject payload;
};

QString registeredClassificationMlpModelType();
QString registeredClassificationFeatureVersionV1();
QStringList registeredClassificationFeatureNamesV1();
QString registeredClassificationMetadataPath(const QString &modelDir);
QString registeredClassificationMlpPath(const QString &modelDir);
QString registeredClassificationTrainingReportPath(const QString &modelDir);

RegisteredClassificationModelPackageResult validateRegisteredClassificationMetadata(
        const RegisteredClassificationModelMetadata &metadata);
RegisteredClassificationModelPackageResult writeRegisteredClassificationMetadata(
        const QString &modelDir,
        const RegisteredClassificationModelMetadata &metadata);
RegisteredClassificationModelPackageResult readRegisteredClassificationMetadata(
        const QString &modelDir,
        RegisteredClassificationModelMetadata *metadata);

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONMODELPACKAGE_H
```

Create `src/algorithms/recognition/RegisteredClassificationModelPackage.cpp` with these concrete rules:

```cpp
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

namespace {

RegisteredClassificationModelPackageResult result(bool success,
                                                  const QString &status,
                                                  const QString &message)
{
    RegisteredClassificationModelPackageResult r;
    r.success = success;
    r.status = status;
    r.message = message;
    r.payload.insert(QStringLiteral("errorCode"), status);
    r.payload.insert(QStringLiteral("errorMessage"), message);
    return r;
}

QJsonArray classLabelsToJson(const QVector<RegisteredClassificationClassLabel> &labels)
{
    QJsonArray array;
    for (const RegisteredClassificationClassLabel &label : labels) {
        QJsonObject item;
        item.insert(QStringLiteral("id"), label.id);
        item.insert(QStringLiteral("name"), label.name);
        array.append(item);
    }
    return array;
}

QVector<RegisteredClassificationClassLabel> classLabelsFromJson(const QJsonArray &array)
{
    QVector<RegisteredClassificationClassLabel> labels;
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        RegisteredClassificationClassLabel label;
        label.id = object.value(QStringLiteral("id")).toInt(-1);
        label.name = object.value(QStringLiteral("name")).toString();
        labels.append(label);
    }
    return labels;
}

QJsonArray stringListToJson(const QStringList &values)
{
    QJsonArray array;
    for (const QString &value : values)
        array.append(value);
    return array;
}

QStringList stringListFromJson(const QJsonArray &array)
{
    QStringList values;
    for (const QJsonValue &value : array)
        values.append(value.toString());
    return values;
}

}

QString registeredClassificationMlpModelType()
{
    return QStringLiteral("halcon_mlp_registered_classification");
}

QString registeredClassificationFeatureVersionV1()
{
    return QStringLiteral("halcon_mlp_roi_stats_v1");
}

QStringList registeredClassificationFeatureNamesV1()
{
    return QStringList()
            << QStringLiteral("roiAspect")
            << QStringLiteral("roiAreaRatio")
            << QStringLiteral("foregroundAreaRatio")
            << QStringLiteral("foregroundCenterX")
            << QStringLiteral("foregroundCenterY")
            << QStringLiteral("momentRa")
            << QStringLiteral("momentRb")
            << QStringLiteral("momentPhi")
            << QStringLiteral("grayMean")
            << QStringLiteral("grayMin")
            << QStringLiteral("grayMax")
            << QStringLiteral("grayDeviation")
            << QStringLiteral("grayHist00")
            << QStringLiteral("grayHist01")
            << QStringLiteral("grayHist02")
            << QStringLiteral("grayHist03")
            << QStringLiteral("grayHist04")
            << QStringLiteral("grayHist05")
            << QStringLiteral("grayHist06")
            << QStringLiteral("grayHist07")
            << QStringLiteral("grayHist08")
            << QStringLiteral("grayHist09")
            << QStringLiteral("grayHist10")
            << QStringLiteral("grayHist11")
            << QStringLiteral("grayHist12")
            << QStringLiteral("grayHist13")
            << QStringLiteral("grayHist14")
            << QStringLiteral("grayHist15");
}

QString registeredClassificationMetadataPath(const QString &modelDir)
{
    return QDir(modelDir).filePath(QStringLiteral("metadata.json"));
}

QString registeredClassificationMlpPath(const QString &modelDir)
{
    return QDir(modelDir).filePath(QStringLiteral("model.gmc"));
}

QString registeredClassificationTrainingReportPath(const QString &modelDir)
{
    return QDir(modelDir).filePath(QStringLiteral("training_report.json"));
}

RegisteredClassificationModelPackageResult validateRegisteredClassificationMetadata(
        const RegisteredClassificationModelMetadata &metadata)
{
    if (metadata.modelType != registeredClassificationMlpModelType())
        return result(false, QStringLiteral("unsupported_model_type"),
                      QStringLiteral("Only halcon_mlp_registered_classification is supported."));
    if (metadata.schemaVersion != 1)
        return result(false, QStringLiteral("unsupported_schema_version"),
                      QStringLiteral("Only schemaVersion=1 is supported."));
    if (metadata.featureVersion != registeredClassificationFeatureVersionV1())
        return result(false, QStringLiteral("unsupported_feature_version"),
                      QStringLiteral("Only halcon_mlp_roi_stats_v1 is supported."));
    if (metadata.featureLength != metadata.featureNames.size() || metadata.featureLength <= 0)
        return result(false, QStringLiteral("feature_length_mismatch"),
                      QStringLiteral("featureLength must match featureNames count and be positive."));
    if (metadata.classLabels.size() < 2)
        return result(false, QStringLiteral("training_not_enough_classes"),
                      QStringLiteral("At least two class labels are required."));
    for (const RegisteredClassificationClassLabel &label : metadata.classLabels) {
        if (label.id < 0 || label.name.trimmed().isEmpty())
            return result(false, QStringLiteral("invalid_class_label"),
                          QStringLiteral("Class labels must have non-negative ids and non-empty names."));
    }
    return result(true, QStringLiteral("ok"), QStringLiteral("metadata is valid"));
}

RegisteredClassificationModelPackageResult writeRegisteredClassificationMetadata(
        const QString &modelDir,
        const RegisteredClassificationModelMetadata &metadata)
{
    const RegisteredClassificationModelPackageResult validation =
            validateRegisteredClassificationMetadata(metadata);
    if (!validation.success)
        return validation;

    QDir dir(modelDir);
    if (!dir.exists() && !QDir().mkpath(modelDir))
        return result(false, QStringLiteral("model_write_failed"),
                      QStringLiteral("Failed to create model directory."));

    QJsonObject root;
    root.insert(QStringLiteral("modelType"), metadata.modelType);
    root.insert(QStringLiteral("schemaVersion"), metadata.schemaVersion);
    root.insert(QStringLiteral("featureVersion"), metadata.featureVersion);
    root.insert(QStringLiteral("halconVersion"), metadata.halconVersion);
    root.insert(QStringLiteral("classLabels"), classLabelsToJson(metadata.classLabels));
    root.insert(QStringLiteral("featureNames"), stringListToJson(metadata.featureNames));
    root.insert(QStringLiteral("featureLength"), metadata.featureLength);
    root.insert(QStringLiteral("preprocess"), metadata.preprocess);
    root.insert(QStringLiteral("trainingSampleCount"), metadata.trainingSampleCount);

    QJsonObject mlp;
    mlp.insert(QStringLiteral("numHidden"), metadata.mlp.numHidden);
    mlp.insert(QStringLiteral("maxIterations"), metadata.mlp.maxIterations);
    mlp.insert(QStringLiteral("randSeed"), metadata.mlp.randSeed);
    root.insert(QStringLiteral("mlp"), mlp);

    QJsonObject thresholds;
    thresholds.insert(QStringLiteral("minScore"), metadata.thresholds.minScore);
    thresholds.insert(QStringLiteral("rejectScore"), metadata.thresholds.rejectScore);
    thresholds.insert(QStringLiteral("top2Gap"), metadata.thresholds.top2Gap);
    root.insert(QStringLiteral("thresholds"), thresholds);

    QFile file(registeredClassificationMetadataPath(modelDir));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return result(false, QStringLiteral("model_write_failed"), file.errorString());
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return result(true, QStringLiteral("ok"), QStringLiteral("metadata written"));
}

RegisteredClassificationModelPackageResult readRegisteredClassificationMetadata(
        const QString &modelDir,
        RegisteredClassificationModelMetadata *metadata)
{
    if (!metadata)
        return result(false, QStringLiteral("invalid_argument"), QStringLiteral("metadata output is null."));
    QFile file(registeredClassificationMetadataPath(modelDir));
    if (!file.exists())
        return result(false, QStringLiteral("model_file_not_found"),
                      QStringLiteral("metadata.json does not exist."));
    if (!file.open(QIODevice::ReadOnly))
        return result(false, QStringLiteral("model_file_not_found"), file.errorString());

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();
    RegisteredClassificationModelMetadata loaded;
    loaded.modelType = root.value(QStringLiteral("modelType")).toString();
    loaded.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(0);
    loaded.featureVersion = root.value(QStringLiteral("featureVersion")).toString();
    loaded.halconVersion = root.value(QStringLiteral("halconVersion")).toString();
    loaded.classLabels = classLabelsFromJson(root.value(QStringLiteral("classLabels")).toArray());
    loaded.featureNames = stringListFromJson(root.value(QStringLiteral("featureNames")).toArray());
    loaded.featureLength = root.value(QStringLiteral("featureLength")).toInt(0);
    loaded.preprocess = root.value(QStringLiteral("preprocess")).toObject();
    loaded.trainingSampleCount = root.value(QStringLiteral("trainingSampleCount")).toInt(0);
    const QJsonObject mlp = root.value(QStringLiteral("mlp")).toObject();
    loaded.mlp.numHidden = mlp.value(QStringLiteral("numHidden")).toInt(16);
    loaded.mlp.maxIterations = mlp.value(QStringLiteral("maxIterations")).toInt(200);
    loaded.mlp.randSeed = mlp.value(QStringLiteral("randSeed")).toInt(42);
    const QJsonObject thresholds = root.value(QStringLiteral("thresholds")).toObject();
    loaded.thresholds.minScore = thresholds.value(QStringLiteral("minScore")).toInt(80);
    loaded.thresholds.rejectScore = thresholds.value(QStringLiteral("rejectScore")).toInt(60);
    loaded.thresholds.top2Gap = thresholds.value(QStringLiteral("top2Gap")).toInt(0);

    const RegisteredClassificationModelPackageResult validation =
            validateRegisteredClassificationMetadata(loaded);
    if (!validation.success)
        return validation;
    *metadata = loaded;
    return result(true, QStringLiteral("ok"), QStringLiteral("metadata read"));
}
```

Add these entries to `qt_ui_test.pro`:

```qmake
SOURCES += \
    src/algorithms/recognition/RegisteredClassificationModelPackage.cpp

HEADERS += \
    src/algorithms/recognition/RegisteredClassificationModelPackage.h
```

- [ ] **Step 4: Run the metadata smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
```

Expected: prints `registered_classification_mlp_backend_smoke: metadata checks passed`.

- [ ] **Step 5: Commit**

```bash
git add qt_ui_test.pro smoke/registered_classification_mlp_backend_smoke.cpp smoke/registered_classification_mlp_backend_smoke.pro src/algorithms/recognition/RegisteredClassificationModelPackage.h src/algorithms/recognition/RegisteredClassificationModelPackage.cpp
git commit -m "feat: add registered classification model package"
```

---

### Task 2: Deterministic ROI Feature Extractor

**Files:**
- Create: `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h`
- Create: `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp`
- Modify: `smoke/registered_classification_mlp_backend_smoke.cpp`
- Modify: `smoke/registered_classification_mlp_backend_smoke.pro`
- Modify: `qt_ui_test.pro`

**Interfaces:**
- Consumes: `registeredClassificationFeatureNamesV1()`.
- Produces: `RegisteredClassificationFeatureConfig`, `RegisteredClassificationFeatureResult`.
- Produces: `RegisteredClassificationFeatureExtractor::extract(const cv::Mat&, const QRectF&, const RegisteredClassificationFeatureConfig&) const`.
- Later training and inference tasks use the same extractor to guarantee feature-version parity.

- [ ] **Step 1: Extend the smoke with feature tests**

Append this helper and checks to `smoke/registered_classification_mlp_backend_smoke.cpp`:

```cpp
#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"
#include <opencv2/imgproc.hpp>

cv::Mat makeFeatureImage()
{
    cv::Mat image(80, 100, CV_8UC3, cv::Scalar(20, 20, 20));
    cv::rectangle(image, cv::Rect(20, 20, 40, 30), cv::Scalar(220, 220, 220), -1);
    return image;
}
```

Add before the final failure check:

```cpp
    RegisteredClassificationFeatureExtractor extractor;
    RegisteredClassificationFeatureConfig featureConfig;
    featureConfig.halconSoPath = QString();
    featureConfig.halconSoPathCandidates.clear();

    const RegisteredClassificationFeatureResult featureResult =
            extractor.extract(makeFeatureImage(), QRectF(0.1, 0.1, 0.7, 0.7), featureConfig);
    check(featureResult.success, "feature extraction must succeed on synthetic image");
    check(featureResult.feature.size() == 28, "feature vector must have v1 length");
    check(featureResult.featureNames == registeredClassificationFeatureNamesV1(),
          "feature names must match v1 metadata");
    check(featureResult.feature.value(0) > 0.0, "roiAspect must be positive");
    check(featureResult.feature.value(12) >= 0.0, "grayHist00 must be non-negative");

    const RegisteredClassificationFeatureResult emptyFeature =
            extractor.extract(cv::Mat(), QRectF(0, 0, 1, 1), featureConfig);
    check(!emptyFeature.success, "empty image feature extraction must fail");
    check(emptyFeature.status == QStringLiteral("image_empty"), "empty image status must be image_empty");

    const RegisteredClassificationFeatureResult invalidRoi =
            extractor.extract(makeFeatureImage(), QRectF(0, 0, 0.001, 0.001), featureConfig);
    check(!invalidRoi.success, "tiny ROI must fail");
    check(invalidRoi.status == QStringLiteral("invalid_roi"), "tiny ROI status must be invalid_roi");
```

- [ ] **Step 2: Run the smoke to verify it fails**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro
make -j$(nproc)
```

Expected: compile fails because `RegisteredClassificationFeatureExtractor.h` does not exist.

- [ ] **Step 3: Implement the feature extractor header**

Create `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h`:

```cpp
#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATUREEXTRACTOR_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATUREEXTRACTOR_H

#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <opencv2/core.hpp>

struct RegisteredClassificationFeatureConfig
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    bool normalizeGray = true;
    bool smooth = true;
};

struct RegisteredClassificationFeatureResult
{
    bool success = false;
    QString status;
    QString message;
    QVector<double> feature;
    QStringList featureNames;
    QRect roiPixels;
    QJsonObject payload;
};

class RegisteredClassificationFeatureExtractor
{
public:
    RegisteredClassificationFeatureResult extract(
            const cv::Mat &image,
            const QRectF &roiNormalized,
            const RegisteredClassificationFeatureConfig &config) const;
};

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONFEATUREEXTRACTOR_H
```

Create `src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp` with a HALCON-only core. The file must include `HalconC.h`, use `dlopen`/`dlsym` like the existing HALCON runners, and resolve these symbols before extracting features:

```cpp
#include "algorithms/recognition/RegisteredClassificationFeatureExtractor.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <HalconC.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QRect>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>

namespace {
constexpr int kMinRoiPixelSize = 2;

struct HalconFeatureApi
{
    using SetUtf8Fn = void (*)(int);
    using GetErrorTextFn = Herror (*)(Hlong, char *);
    using CreateTupleFn = void (*)(Htuple *, Hlong);
    using SetDoubleFn = void (*)(Htuple *, double, Hlong);
    using SetIntFn = void (*)(Htuple *, Hlong, Hlong);
    using SetStringFn = void (*)(Htuple *, const char *, Hlong);
    using DestroyTupleFn = void (*)(Htuple *);
    using GetDoubleFn = double (*)(const Htuple *, Hlong);
    using GetIntFn = Hlong (*)(const Htuple *, Hlong);
    using GenImageInterleavedFn = Herror (*)(Hobject *, Hlong, const char *, Hlong, Hlong, Hlong,
                                             const char *, Hlong, Hlong, Hlong, Hlong, Hlong, Hlong);
    using Rgb1ToGrayFn = Herror (*)(const Hobject, Hobject *);
    using GenRectangle1Fn = Herror (*)(Hobject *, double, double, double, double);
    using ReduceDomainFn = Herror (*)(const Hobject, const Hobject, Hobject *);
    using ThresholdFn = Herror (*)(const Hobject, Hobject *, double, double);
    using AreaCenterFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using MomentsRegion2ndFn = Herror (*)(const Hobject, Htuple *, Htuple *, Htuple *);
    using IntensityFn = Herror (*)(const Hobject, const Hobject, Htuple *, Htuple *);
    using GrayHistoFn = Herror (*)(const Hobject, const Hobject, Htuple *, Htuple *);
    using ClearObjFn = Herror (*)(const Hobject);

    SetUtf8Fn setUtf8 = nullptr;
    GetErrorTextFn getErrorText = nullptr;
    CreateTupleFn createTuple = nullptr;
    SetDoubleFn setDouble = nullptr;
    SetIntFn setInt = nullptr;
    SetStringFn setString = nullptr;
    DestroyTupleFn destroyTuple = nullptr;
    GetDoubleFn getDouble = nullptr;
    GetIntFn getInt = nullptr;
    GenImageInterleavedFn genImageInterleaved = nullptr;
    Rgb1ToGrayFn rgb1ToGray = nullptr;
    GenRectangle1Fn genRectangle1 = nullptr;
    ReduceDomainFn reduceDomain = nullptr;
    ThresholdFn threshold = nullptr;
    AreaCenterFn areaCenter = nullptr;
    MomentsRegion2ndFn momentsRegion2nd = nullptr;
    IntensityFn intensity = nullptr;
    GrayHistoFn grayHisto = nullptr;
    ClearObjFn clearObj = nullptr;
};

RegisteredClassificationFeatureResult errorResult(const QString &status, const QString &message)
{
    RegisteredClassificationFeatureResult result;
    result.success = false;
    result.status = status;
    result.message = message;
    result.payload.insert(QStringLiteral("errorCode"), status);
    result.payload.insert(QStringLiteral("errorMessage"), message);
    return result;
}

cv::Mat toBgr8ForHalcon(const cv::Mat &image)
{
    cv::Mat source;
    if (image.depth() == CV_8U)
        source = image;
    else
        image.convertTo(source, CV_8U);
    cv::Mat bgr;
    if (source.channels() == 1)
        cv::cvtColor(source, bgr, cv::COLOR_GRAY2BGR);
    else if (source.channels() == 3)
        bgr = source;
    else if (source.channels() == 4)
        cv::cvtColor(source, bgr, cv::COLOR_BGRA2BGR);
    if (!bgr.empty() && !bgr.isContinuous())
        bgr = bgr.clone();
    return bgr;
}

QJsonArray vectorToJson(const QVector<double> &values)
{
    QJsonArray array;
    for (double value : values)
        array.append(value);
    return array;
}
}
```

Then implement `RegisteredClassificationFeatureExtractor::extract()` with this exact HALCON flow. OpenCV conversion is limited to making a continuous 8-bit BGR buffer for `gen_image_interleaved`; all ROI, threshold, statistics, moments, and histogram values must come from HALCON:

```text
1. Return image_empty if cv::Mat is empty.
2. Convert the input to continuous CV_8UC3 BGR only for HALCON image creation.
3. Convert normalized ROI to pixel row/column bounds. Return invalid_roi if width or height is less than 2 pixels.
4. Load HALCON through HalconRuntimePaths and resolve the symbols listed in HalconFeatureApi.
5. Call gen_image_interleaved to create a HALCON image from the BGR buffer.
6. Call rgb1_to_gray to create a gray image.
7. Call gen_rectangle1(row1, col1, row2, col2) to create the ROI region.
8. Call reduce_domain(gray, roiRegion, roiGray).
9. Call intensity(roiRegion, gray, mean, deviation) for grayMean and grayDeviation.
10. Call threshold(roiGray, foreground, mean, 255) to create the foreground region.
11. Call area_center(foreground, area, row, column) for foregroundAreaRatio and center.
12. Call moments_region_2nd(foreground, ra, rb, phi) for momentRa, momentRb, momentPhi.
13. Call gray_histo(roiRegion, gray, absoluteHisto, relativeHisto) and compress the returned histogram into 16 normalized bins.
14. Build the 28 features in registeredClassificationFeatureNamesV1() order.
15. Clear every HALCON object and tuple handle before returning.
```

If any required symbol is unavailable, return `halcon_symbol_missing` with the symbol name in `message`. Do not replace missing HALCON feature operations with OpenCV calculations.

- [ ] **Step 4: Add project and smoke build entries**

Add to `qt_ui_test.pro`:

```qmake
SOURCES += \
    src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp

HEADERS += \
    src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h
```

Add to `smoke/registered_classification_mlp_backend_smoke.pro`:

```qmake
SOURCES += \
    ../src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp \
    ../src/algorithms/halcon/HalconRuntimePaths.cpp

HEADERS += \
    ../src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h \
    ../src/algorithms/halcon/HalconRuntimePaths.h
```

- [ ] **Step 5: Run the feature smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
```

Expected: metadata and feature checks pass.

- [ ] **Step 6: Commit**

```bash
git add qt_ui_test.pro smoke/registered_classification_mlp_backend_smoke.cpp smoke/registered_classification_mlp_backend_smoke.pro src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp
git commit -m "feat: extract registered classification roi features"
```

---

### Task 3: HALCON MLP Training Runner

**Files:**
- Create: `src/algorithms/recognition/RegisteredClassificationTrainingRunner.h`
- Create: `src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp`
- Modify: `smoke/registered_classification_mlp_backend_smoke.cpp`
- Modify: `smoke/registered_classification_mlp_backend_smoke.pro`
- Modify: `qt_ui_test.pro`

**Interfaces:**
- Consumes: `RegisteredClassificationFeatureExtractor`, `RegisteredClassificationModelMetadata`, model package path helpers.
- Produces: `RegisteredClassificationTrainingSample`, `RegisteredClassificationTrainingRequest`, `RegisteredClassificationTrainingResult`.
- Produces: `RegisteredClassificationTrainingRunner::train(const RegisteredClassificationTrainingRequest&) const`.
- Later inference task consumes model packages created by this runner.

- [ ] **Step 1: Add training checks to the smoke**

Append includes:

```cpp
#include "algorithms/recognition/RegisteredClassificationTrainingRunner.h"
```

Add helper:

```cpp
RegisteredClassificationTrainingRequest makeTrainingRequest(const QString &modelDir)
{
    RegisteredClassificationTrainingRequest request;
    request.halconSoPath = QString();
    request.outputModelDir = modelDir;
    request.classLabels = {
        {0, QStringLiteral("Bright")},
        {1, QStringLiteral("Dark")}
    };
    request.mlp.numHidden = 8;
    request.mlp.maxIterations = 100;
    request.mlp.randSeed = 42;
    for (int i = 0; i < 4; ++i) {
        cv::Mat bright(80, 100, CV_8UC3, cv::Scalar(30, 30, 30));
        cv::rectangle(bright, cv::Rect(20, 20, 40, 30), cv::Scalar(220, 220, 220), -1);
        request.samples.append({bright, QRectF(0.2, 0.2, 0.4, 0.4), 0});

        cv::Mat dark(80, 100, CV_8UC3, cv::Scalar(220, 220, 220));
        cv::rectangle(dark, cv::Rect(20, 20, 40, 30), cv::Scalar(30, 30, 30), -1);
        request.samples.append({dark, QRectF(0.2, 0.2, 0.4, 0.4), 1});
    }
    return request;
}
```

Add before final failure check:

```cpp
    const QString trainedModelDir = QDir(tempModelDir()).filePath(QStringLiteral("trained"));
    RegisteredClassificationTrainingRunner trainer;
    const RegisteredClassificationTrainingResult trainResult =
            trainer.train(makeTrainingRequest(trainedModelDir));
    check(trainResult.success, "MLP training must succeed");
    check(trainResult.status == QStringLiteral("ok"), "training status must be ok");
    check(QFileInfo(registeredClassificationMlpPath(trainedModelDir)).exists(),
          "training must create model.gmc");
    check(QFileInfo(registeredClassificationMetadataPath(trainedModelDir)).exists(),
          "training must create metadata.json");
    check(QFileInfo(registeredClassificationTrainingReportPath(trainedModelDir)).exists(),
          "training must create training_report.json");

    RegisteredClassificationTrainingRequest oneClass = makeTrainingRequest(QDir(tempModelDir()).filePath(QStringLiteral("one_class")));
    oneClass.classLabels = {{0, QStringLiteral("Only")}};
    const RegisteredClassificationTrainingResult oneClassResult = trainer.train(oneClass);
    check(!oneClassResult.success, "one-class training must fail");
    check(oneClassResult.status == QStringLiteral("training_not_enough_classes"),
          "one-class training status must be training_not_enough_classes");
```

- [ ] **Step 2: Run the smoke to verify it fails**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro
make -j$(nproc)
```

Expected: compile fails because `RegisteredClassificationTrainingRunner.h` does not exist.

- [ ] **Step 3: Implement training runner interfaces**

Create `src/algorithms/recognition/RegisteredClassificationTrainingRunner.h`:

```cpp
#ifndef ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGRUNNER_H
#define ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGRUNNER_H

#include "algorithms/recognition/RegisteredClassificationModelPackage.h"

#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <opencv2/core.hpp>

struct RegisteredClassificationTrainingSample
{
    cv::Mat image;
    QRectF roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    int classId = -1;
};

struct RegisteredClassificationTrainingRequest
{
    QString halconSoPath;
    QStringList halconSoPathCandidates;
    QString outputModelDir;
    QVector<RegisteredClassificationClassLabel> classLabels;
    QVector<RegisteredClassificationTrainingSample> samples;
    RegisteredClassificationMlpParams mlp;
    RegisteredClassificationThresholds thresholds;
};

struct RegisteredClassificationTrainingResult
{
    bool success = false;
    QString status;
    QString message;
    QString modelDir;
    int sampleCount = 0;
    QJsonObject payload;
};

class RegisteredClassificationTrainingRunner
{
public:
    RegisteredClassificationTrainingResult train(
            const RegisteredClassificationTrainingRequest &request) const;
};

#endif // ALGORITHMS_RECOGNITION_REGISTEREDCLASSIFICATIONTRAININGRUNNER_H
```

- [ ] **Step 4: Implement HALCON MLP training**

Implement `RegisteredClassificationTrainingRunner.cpp` with these concrete rules:

- Validate `outputModelDir` non-empty, at least two classes, at least one valid sample per class.
- Use `RegisteredClassificationFeatureExtractor` to create 28-dim vectors.
- Use HALCON `create_class_mlp`, `add_sample_class_mlp`, `train_class_mlp`, and `write_class_mlp`.
- Write `metadata.json` through `writeRegisteredClassificationMetadata`.
- Write `training_report.json` with sample counts and `ErrorLog`.
- Write to a temporary sibling directory named `<outputModelDir>.tmp`, then replace the target only after all files exist.
- Return `halcon_symbol_missing`, `mlp_create_failed`, `mlp_train_failed`, or `model_write_failed` on exact failure points.

The implementation must define these HALCON C API function pointer names in a local loader:

```cpp
T_create_class_mlp
T_add_sample_class_mlp
T_train_class_mlp
T_write_class_mlp
T_clear_class_mlp
F_create_tuple
F_set_d
F_set_i
F_set_s
F_destroy_tuple
F_get_d
```

Use `HalconRuntimePaths::resolveHalconLibPath(request.halconSoPath, &candidates)` and `dlopen`/`dlsym` consistently with existing HALCON runners.

- [ ] **Step 5: Add build entries**

Add to `qt_ui_test.pro`:

```qmake
SOURCES += \
    src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp

HEADERS += \
    src/algorithms/recognition/RegisteredClassificationTrainingRunner.h
```

Add to `smoke/registered_classification_mlp_backend_smoke.pro`:

```qmake
SOURCES += \
    ../src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp

HEADERS += \
    ../src/algorithms/recognition/RegisteredClassificationTrainingRunner.h
```

- [ ] **Step 6: Run training smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
```

Expected: metadata, feature, and training checks pass; model package files exist in `/tmp/registered_classification_mlp_backend_smoke_model/trained`.

- [ ] **Step 7: Commit**

```bash
git add qt_ui_test.pro smoke/registered_classification_mlp_backend_smoke.cpp smoke/registered_classification_mlp_backend_smoke.pro src/algorithms/recognition/RegisteredClassificationTrainingRunner.h src/algorithms/recognition/RegisteredClassificationTrainingRunner.cpp
git commit -m "feat: train registered classification mlp models"
```

---

### Task 4: MLP Inference Runner Replacement

**Files:**
- Modify: `src/algorithms/recognition/RegisteredClassificationHalconRunner.h`
- Replace: `src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp`
- Modify: `smoke/registered_classification_mlp_backend_smoke.cpp`
- Modify: `smoke/registered_classification_mlp_backend_smoke.pro`

**Interfaces:**
- Consumes: model package from Task 3.
- Keeps: `RegisteredClassificationHalconRunner::run(const cv::Mat&, const RegisteredClassificationHalconConfig&) const`.
- Produces: payload with `algorithm=halcon_mlp_registered_classification`, `featureVersion`, `topClasses`, `rejectScore`, and exact error codes.

- [ ] **Step 1: Add inference checks to the smoke**

Append:

```cpp
#include "algorithms/recognition/RegisteredClassificationHalconRunner.h"
```

Add after successful training:

```cpp
    RegisteredClassificationHalconConfig inferConfig;
    inferConfig.modelPath = trainedModelDir;
    inferConfig.modelName = QStringLiteral("smoke_mlp");
    inferConfig.modelType = registeredClassificationMlpModelType();
    inferConfig.detectRegionType = QStringLiteral("rectangle");
    inferConfig.roiNormalized = QRectF(0.2, 0.2, 0.4, 0.4);
    inferConfig.topK = 2;
    inferConfig.judgeMode = QStringLiteral("class_match");
    inferConfig.expectedLabel = QStringLiteral("Bright");
    inferConfig.minScore = 50;

    RegisteredClassificationHalconRunner inferRunner;
    cv::Mat bright = makeFeatureImage();
    const RegisteredClassificationHalconResult inferResult = inferRunner.run(bright, inferConfig);
    check(inferResult.success, "inference must succeed");
    check(inferResult.status == QStringLiteral("ok"), "inference status must be ok");
    check(!inferResult.predictedLabel.isEmpty(), "inference must produce a label");
    check(inferResult.score >= 0.0 && inferResult.score <= 100.0, "score must be 0-100");
    check(inferResult.payload.value(QStringLiteral("algorithm")).toString()
                  == registeredClassificationMlpModelType(),
          "payload algorithm must be MLP registered classification");
    check(inferResult.payload.value(QStringLiteral("featureVersion")).toString()
                  == registeredClassificationFeatureVersionV1(),
          "payload featureVersion must be v1");

    inferConfig.modelType = QStringLiteral("halcon_dl_classification");
    const RegisteredClassificationHalconResult oldDlResult = inferRunner.run(bright, inferConfig);
    check(!oldDlResult.success, "old DL model type must fail");
    check(oldDlResult.status == QStringLiteral("unsupported_model_type"),
          "old DL model type status must be unsupported_model_type");
```

- [ ] **Step 2: Run smoke to observe old behavior**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
```

Expected: inference checks fail because current runner still expects DL model files.

- [ ] **Step 3: Update the runner header defaults**

In `src/algorithms/recognition/RegisteredClassificationHalconRunner.h`, change:

```cpp
QString modelType = QStringLiteral("halcon_dl_classification");
```

to:

```cpp
QString modelType = QStringLiteral("halcon_mlp_registered_classification");
```

Add result fields:

```cpp
int rejectScore = 60;
int top2Gap = 0;
```

- [ ] **Step 4: Replace runner implementation**

Rewrite `RegisteredClassificationHalconRunner.cpp` so `run()` performs these exact stages:

1. Start `QElapsedTimer`.
2. If `image.empty()`, return `image_empty`.
3. If `config.modelPath.trimmed().isEmpty()`, return `model_path_empty`.
4. If `config.modelType != registeredClassificationMlpModelType()`, return `unsupported_model_type`.
5. Read metadata through `readRegisteredClassificationMetadata(config.modelPath, &metadata)`.
6. Verify `model.gmc` exists through `registeredClassificationMlpPath(config.modelPath)`.
7. Extract features with `RegisteredClassificationFeatureExtractor`.
8. Load HALCON MLP with `read_class_mlp`.
9. Call `classify_class_mlp` with `qBound(1, config.topK, metadata.classLabels.size())`.
10. Map class ids to `metadata.classLabels`.
11. Apply `classification_rejected` and `classification_ambiguous` before judge rule.
12. Apply `class_match` or `min_score`.
13. Return overlays for ROI rectangle and text.

Required local helper signatures:

```cpp
static RegisteredClassificationHalconResult makeError(
        const QString &status,
        const QString &message,
        const RegisteredClassificationHalconConfig &config,
        const cv::Mat &image,
        qint64 elapsedMs);

static QJsonArray topClassesToJson(
        const QVector<RegisteredClassificationClassScore> &topClasses);

static QRect normalizedRoiToPixels(
        const QRectF &sourceRoi,
        int width,
        int height);
```

Do not keep any `T_read_dl_model`, `T_apply_dl_model`, `T_create_dict`, or DL dictionary code.

- [ ] **Step 5: Run inference smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke
```

Expected: metadata, feature, training, inference, and old-DL rejection checks pass.

- [ ] **Step 6: Commit**

```bash
git add smoke/registered_classification_mlp_backend_smoke.cpp smoke/registered_classification_mlp_backend_smoke.pro src/algorithms/recognition/RegisteredClassificationHalconRunner.h src/algorithms/recognition/RegisteredClassificationHalconRunner.cpp
git commit -m "feat: run registered classification mlp inference"
```

---

### Task 5: Adapter Contract Update

**Files:**
- Modify: `src/tooladapters/RegisteredClassificationAdapter.cpp`
- Modify: `smoke/registered_classification_adapter_smoke.cpp`
- Modify: `smoke/registered_classification_adapter_smoke.pro`

**Interfaces:**
- Consumes: `RegisteredClassificationHalconRunner` MLP contract.
- Produces: `ToolResult` with old DL configs rejected as `unsupported_model_type`.

- [ ] **Step 1: Rewrite adapter smoke expectations**

In `smoke/registered_classification_adapter_smoke.cpp`, change `makeConfig()` default model type from:

```cpp
nested.insert(QStringLiteral("modelType"), QStringLiteral("halcon_dl_classification"));
```

to:

```cpp
nested.insert(QStringLiteral("modelType"), QStringLiteral("halcon_mlp_registered_classification"));
```

Add a `modelType` argument to `makeConfig()` and use it for one old-DL test:

```cpp
ToolConfig config = makeConfig(QStringLiteral("/tmp/LegacyModel.hdl"),
                               QStringLiteral("LegacyModel"),
                               QStringLiteral("halcon_dl_classification"),
                               QStringLiteral("full"),
                               QRectF(0, 0, 1, 1),
                               QString(),
                               QStringLiteral("class_match"),
                               QStringLiteral("OK"),
                               80);
const ToolResult result = adapter.run(requestWithImage(config, dummyImage));
check(result.status == QStringLiteral("unsupported_model_type"),
      "old DL model type must yield unsupported_model_type");
```

Replace old assertions:

```cpp
no_model
unsupported_model_format
invalid_model_name
model_load_failed
```

with MLP package assertions:

```cpp
model_path_empty
model_file_not_found
unsupported_model_type
invalid_roi
missing_expected_label
```

- [ ] **Step 2: Run adapter smoke to verify failures**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_adapter_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_adapter/bin/registered_classification_adapter_smoke
```

Expected: compile or runtime failures until adapter parsing is updated.

- [ ] **Step 3: Update adapter parsing**

In `src/tooladapters/RegisteredClassificationAdapter.cpp`, change default model type:

```cpp
runnerConfig.modelType = stringParam(params,
                                     QStringLiteral("modelType"),
                                     QStringLiteral("halcon_mlp_registered_classification"));
```

Ensure `modelPath` is treated as a model directory. Keep `topK`, `judgeRule`, ROI, and position-correction parsing unchanged.

- [ ] **Step 4: Update smoke project dependencies**

Add new backend files to `smoke/registered_classification_adapter_smoke.pro`:

```qmake
SOURCES += \
    ../src/algorithms/recognition/RegisteredClassificationModelPackage.cpp \
    ../src/algorithms/recognition/RegisteredClassificationFeatureExtractor.cpp

HEADERS += \
    ../src/algorithms/recognition/RegisteredClassificationModelPackage.h \
    ../src/algorithms/recognition/RegisteredClassificationFeatureExtractor.h
```

- [ ] **Step 5: Run adapter smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_adapter_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_adapter/bin/registered_classification_adapter_smoke
```

Expected: `registered_classification_adapter_smoke: all checks passed`.

- [ ] **Step 6: Commit**

```bash
git add smoke/registered_classification_adapter_smoke.cpp smoke/registered_classification_adapter_smoke.pro src/tooladapters/RegisteredClassificationAdapter.cpp
git commit -m "feat: route registered classification adapter to mlp backend"
```

---

### Task 6: Training Dialog Backend Handoff

**Files:**
- Modify: `src/RegisteredClassificationTrainingDialog.h`
- Modify: `src/RegisteredClassificationTrainingDialog.cpp`
- Modify: `qt_ui_test.pro`
- Modify: `smoke/registered_classification_dialog_smoke.cpp`
- Modify: `smoke/registered_classification_dialog_smoke.pro`

**Interfaces:**
- Consumes: `RegisteredClassificationTrainingRunner::train`.
- Produces: a training button path that converts current training-session image/class/ROI state into `RegisteredClassificationTrainingRequest`.
- Produces: user-visible success/failure messages without performing feature extraction in UI code.

- [ ] **Step 1: Expose a testable training request builder**

In `RegisteredClassificationTrainingDialog.h`, add:

```cpp
public:
    QJsonObject buildTrainingRequestPreviewForTest() const;
```

The preview must not expose `cv::Mat`; it returns counts and class names so smoke can verify UI state before backend training.

- [ ] **Step 2: Add smoke checks for training request preview**

In `smoke/registered_classification_dialog_smoke.cpp`, after existing ROI creation checks, add:

```cpp
QJsonObject preview = trainingDialog.buildTrainingRequestPreviewForTest();
check(preview.value(QStringLiteral("classCount")).toInt() >= 1,
      "training preview must include classes");
check(preview.value(QStringLiteral("imageCount")).toInt() >= 1,
      "training preview must include images after adding registration image");
check(preview.value(QStringLiteral("roiCount")).toInt() >= 1,
      "training preview must include ROI count after marking ROI");
```

- [ ] **Step 3: Run dialog smoke to verify failure**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: compile fails until `buildTrainingRequestPreviewForTest()` exists.

- [ ] **Step 4: Implement request preview and backend training call**

In `RegisteredClassificationTrainingDialog.cpp`, implement:

```cpp
QJsonObject RegisteredClassificationTrainingDialog::buildTrainingRequestPreviewForTest() const
{
    QJsonObject json;
    json.insert(QStringLiteral("classCount"), state_.classes.size());
    json.insert(QStringLiteral("imageCount"), state_.images.size());
    int roiCount = 0;
    for (const TrainingImageState &image : state_.images) {
        for (auto it = image.marksByClass.constBegin(); it != image.marksByClass.constEnd(); ++it)
            roiCount += it.value().size();
    }
    json.insert(QStringLiteral("roiCount"), roiCount);
    return json;
}
```

Add a private helper in the cpp:

```cpp
RegisteredClassificationTrainingRequest buildTrainingRequest(
        const TrainingSessionState &state,
        const QString &outputModelDir,
        const QString &halconSoPath)
```

Rules:

- Convert each `QImage` to `cv::Mat` through existing `MatImageConverter` utilities.
- Use each ROI mark’s normalized rect. For polygon marks, use the polygon bounding rect for first backend version and record a warning in the training result message.
- Map class row index to `RegisteredClassificationClassLabel{id, name}`.
- Do not call HALCON directly in UI; only call `RegisteredClassificationTrainingRunner`.

Wire the existing “开始训练” or equivalent training button to:

```cpp
RegisteredClassificationTrainingRunner runner;
const RegisteredClassificationTrainingResult result = runner.train(request);
```

On success, display the model directory and keep the dialog open. On failure, show `result.status` and `result.message`.

- [ ] **Step 5: Add project and smoke dependencies**

Ensure `qt_ui_test.pro` already includes `RegisteredClassificationTrainingRunner.cpp`. Add the runner/model/feature sources to `smoke/registered_classification_dialog_smoke.pro` if missing.

- [ ] **Step 6: Run dialog smoke**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: existing ROI UI checks and new training preview checks pass.

- [ ] **Step 7: Commit**

```bash
git add qt_ui_test.pro smoke/registered_classification_dialog_smoke.cpp smoke/registered_classification_dialog_smoke.pro src/RegisteredClassificationTrainingDialog.h src/RegisteredClassificationTrainingDialog.cpp
git commit -m "feat: connect registered classification training dialog to mlp requests"
```

---

### Task 7: Documentation and Full Verification

**Files:**
- Modify: `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`
- Modify: `docs/FID/RegisteredClassification/注册分类提示词规范.md`
- Modify: `docs/FID/Function_Docs.md` only if the function index needs the backend status reflected.

**Interfaces:**
- Consumes: final behavior from Tasks 1-6.
- Produces: docs that no longer present DL classification as the active registered classification backend.

- [ ] **Step 1: Update implementation record**

In `docs/FID/RegisteredClassification/registered_classification_function_implementation.md`, add a new section:

```markdown
## HALCON MLP 后端替换记录

[已完成] 注册分类后端主线已从 `halcon_dl_classification` 硬替换为 `halcon_mlp_registered_classification`。

- 模型包：目录结构为 `model.gmc`、`metadata.json`、`training_report.json`。
- 训练：注册图、类别和 ROI 转为固定 28 维 `halcon_mlp_roi_stats_v1` 特征，并通过 HALCON `create_class_mlp` / `add_sample_class_mlp` / `train_class_mlp` 训练。
- 推理：读取 `model.gmc` 和 `metadata.json`，提取同版本 ROI 特征，通过 `classify_class_mlp` 输出 TopK、类别和置信度。
- 旧配置：`halcon_dl_classification` 返回 `unsupported_model_type`，不再尝试读取 DL 模型。
- 非范围：模板匹配、形状模型、姿态归一化和位置修正不属于注册分类后端。
```

- [ ] **Step 2: Update prompt spec**

In `docs/FID/RegisteredClassification/注册分类提示词规范.md`, update the fixed backend context:

```markdown
注册分类真实算法后端使用 HALCON 经典 MLP 分类链路：
- `modelType=halcon_mlp_registered_classification`
- 训练模型包包含 `model.gmc`、`metadata.json`、`training_report.json`
- 不支持 `halcon_dl_classification`
- 不支持 `.scbin`
- 不在注册分类后端实现模板匹配或位置修正
```

- [ ] **Step 3: Run smoke verification**

Run:

```bash
cd smoke
/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_mlp_backend_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_mlp_backend/bin/registered_classification_mlp_backend_smoke

/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_adapter_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_adapter/bin/registered_classification_adapter_smoke

/home/tt/Qt/5.15.2/gcc_64/bin/qmake registered_classification_dialog_smoke.pro
make -j$(nproc)
../build/smoke/registered_classification_dialog/bin/registered_classification_dialog_smoke
```

Expected: all smoke commands exit 0.

- [ ] **Step 4: Run main project build**

Run:

```bash
mkdir -p build
cd build
/home/tt/Qt/5.15.2/gcc_64/bin/qmake ../qt_ui_test.pro
make -j$(nproc)
```

Expected: main project build exits 0.

- [ ] **Step 5: Run diff hygiene**

Run:

```bash
git diff --check
git status --short
```

Expected: `git diff --check` has no output. `git status --short` shows only intended source, smoke, project, and doc files plus any pre-existing unrelated user changes.

- [ ] **Step 6: Commit**

```bash
git add docs/FID/RegisteredClassification/registered_classification_function_implementation.md docs/FID/RegisteredClassification/注册分类提示词规范.md docs/FID/Function_Docs.md
git commit -m "docs: update registered classification mlp backend status"
```

---

## Plan Self-Review

- Spec coverage: The plan covers hard DL replacement, no `.scbin`, no template matching/position correction, model package, feature versioning, training, inference, adapter rejection, smoke tests, main build, and docs.
- Scope check: The spec is one subsystem: registered classification backend. Position correction and template matching remain outside the plan.
- Type consistency: Shared names are stable across tasks: `halcon_mlp_registered_classification`, `halcon_mlp_roi_stats_v1`, `RegisteredClassificationModelMetadata`, `RegisteredClassificationFeatureExtractor`, `RegisteredClassificationTrainingRunner`, and existing `RegisteredClassificationHalconRunner`.
- Risk callout: Task 2 must be HALCON-backed from its first passing implementation. If a required HALCON feature symbol is missing, stop and document the blocker instead of computing core feature values with OpenCV.
