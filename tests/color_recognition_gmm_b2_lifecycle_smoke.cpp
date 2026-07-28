#include "ColorTemplateDialog.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "tooladapters/ColorRecognitionAdapter.h"
#include "toolcore/ToolRequest.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

#include <opencv2/imgcodecs.hpp>

#include <iostream>

namespace {

int check(bool condition, const char *message)
{
    if (condition)
        return 0;
    std::cerr << message << std::endl;
    return 1;
}

QString imageHash(const cv::Mat &image)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArray::number(image.rows) + 'x' + QByteArray::number(image.cols) + ':' +
                 QByteArray::number(image.type()) + ':');
    const int rowBytes = image.cols * static_cast<int>(image.elemSize());
    for (int row = 0; row < image.rows; ++row)
        hash.addData(reinterpret_cast<const char *>(image.ptr(row)), rowBytes);
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(hash.result().toHex()));
}

ColorRecognitionSampleData sample(const QString &id,
                                  int classId,
                                  const QString &label,
                                  const cv::Scalar &bgr)
{
    const cv::Mat image(32, 32, CV_8UC3, bgr);
    std::vector<uchar> png;
    cv::imencode(".png", image, png);
    const QByteArray bytes(reinterpret_cast<const char *>(png.data()),
                           static_cast<int>(png.size()));
    ColorRecognitionSampleData result;
    result.sampleId = id;
    result.classId = classId;
    result.label = label;
    result.feature = {1.0};
    result.featureSignature = QStringLiteral("hsv-test-signature");
    result.gmmRoiImagePngBase64 = QString::fromLatin1(bytes.toBase64());
    result.gmmImageSha256 = imageHash(image);
    result.gmmImageWidth = image.cols;
    result.gmmImageHeight = image.rows;
    result.pixelFormat = QStringLiteral("BGR8");
    result.validBits = 8;
    result.bitShift = 0;
    return result;
}

void activate(QComboBox *combo, int index)
{
    combo->setCurrentIndex(index);
    QMetaObject::invokeMethod(combo, "activated", Qt::DirectConnection, Q_ARG(int, index));
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    const cv::Mat bgraReference(10, 12, CV_8UC4, cv::Scalar(10, 20, 30, 255));
    FrameInputMetadata sourceMetadata;
    sourceMetadata.colorMode = QStringLiteral("color");
    sourceMetadata.pixelFormat = QStringLiteral("RGBX8");
    sourceMetadata.originalChannels = 4;
    sourceMetadata.originalDepth = 8;
    sourceMetadata.source = QStringLiteral("camera");
    ReferenceImageProvider::instance().setReferenceFrame(bgraReference, sourceMetadata);
    const ReferenceFrameSnapshot normalizedReference =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    if (check(normalizedReference.frame.type() == CV_8UC3 &&
              normalizedReference.metadata.pixelFormat == QStringLiteral("BGR8") &&
              normalizedReference.metadata.originalChannels == 3 &&
              normalizedReference.metadata.validBits == 8 &&
              normalizedReference.metadata.bitShift == 0,
              "Reference runtime metadata must describe the normalized BGR8 Mat")) return 1;
    const cv::Mat bgr16Reference(8, 9, CV_16UC3, cv::Scalar(17, 2048, 4095));
    ReferenceImageProvider::instance().setReferenceFrame(bgr16Reference, sourceMetadata);
    const ReferenceFrameSnapshot normalizedReference16 =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    if (check(normalizedReference16.frame.type() == CV_16UC3 &&
              normalizedReference16.metadata.pixelFormat == QStringLiteral("BGR16") &&
              normalizedReference16.metadata.validBits == 16 &&
              normalizedReference16.metadata.bitShift == 0 &&
              cv::norm(normalizedReference16.frame, bgr16Reference, cv::NORM_INF) == 0.0,
              "Reference normalization must preserve 16-bit depth, values, and BGR16 metadata")) return 1;
    ReferenceImageProvider::instance().clearReferenceFrame();

    const cv::Mat image16(8, 9, CV_16UC3, cv::Scalar(17, 2048, 4095));
    std::vector<uchar> png16;
    cv::imencode(".png", image16, png16);
    const cv::Mat decoded16 = cv::imdecode(png16, cv::IMREAD_UNCHANGED);
    if (check(decoded16.type() == CV_16UC3 && decoded16.size() == image16.size() &&
              cv::norm(decoded16, image16, cv::NORM_INF) == 0.0,
              "B2 PNG bridge must preserve 16-bit ROI depth and values")) return 1;

    const cv::Mat fullReference(80, 120, CV_8UC3, cv::Scalar(40, 50, 60));
    ReferenceImageProvider::instance().setReferenceFrame(
                fullReference, FrameInputMetadata::fromMat(fullReference, QStringLiteral("reference")));

    ColorRecognitionTemplateData data;
    data.templateId = QStringLiteral("gmm-b2-smoke");
    data.recognitionBackend = QStringLiteral("cielab_gmm");
    data.gmmColorChannels = QStringLiteral("ab");
    data.gmmModel.state = QStringLiteral("empty");
    data.labels = {{QStringLiteral("red"), 10}, {QStringLiteral("green"), 20}};
    data.samples = {
        sample(QStringLiteral("red-1"), 10, QStringLiteral("red"), cv::Scalar(0, 0, 255)),
        sample(QStringLiteral("green-1"), 20, QStringLiteral("green"), cv::Scalar(0, 255, 0))
    };

    ColorTemplateDialog dialog;
    dialog.setTemplateData(data);
    QComboBox *backend = dialog.findChild<QComboBox *>(QStringLiteral("recognitionBackendComboBox"));
    QComboBox *feature = dialog.findChild<QComboBox *>(QStringLiteral("featureTypeComboBox"));
    QComboBox *sensitivity = dialog.findChild<QComboBox *>(QStringLiteral("sensitivityComboBox"));
    QCheckBox *brightness = dialog.findChild<QCheckBox *>(QStringLiteral("brightnessEnabledCheckBox"));
    QPushButton *build = dialog.findChild<QPushButton *>(QStringLiteral("buildGmmModelButton"));
    QWidget *featureRow = dialog.findChild<QWidget *>(QStringLiteral("featureTypeRowWidget"));
    QLabel *buildFeedback = dialog.findChild<QLabel *>(QStringLiteral("gmmBuildFeedbackLabel"));
    QLabel *hsvState = dialog.findChild<QLabel *>(QStringLiteral("hsvModelStateLabel"));
    QLabel *hsvFeedback = dialog.findChild<QLabel *>(QStringLiteral("hsvBuildFeedbackLabel"));
    QPushButton *rebuildHsv = dialog.findChild<QPushButton *>(QStringLiteral("rebuildHsvFeaturesButton"));
    QListWidget *roiSamples = dialog.findChild<QListWidget *>(QStringLiteral("roiSampleListWidget"));
    QLabel *viewerTitle = dialog.findChild<QLabel *>(QStringLiteral("viewerTitleLabel"));
    QGraphicsView *preview = dialog.findChild<QGraphicsView *>();
    FrameViewHelper *previewHelper = dialog.findChild<FrameViewHelper *>();
    QToolButton *sampleRectButton = dialog.findChild<QToolButton *>(
                QStringLiteral("sampleRectRoiButton"));
    if (check(backend && feature && sensitivity && brightness && build && featureRow && buildFeedback &&
              hsvState && hsvFeedback && rebuildHsv && roiSamples && viewerTitle && preview &&
              previewHelper && sampleRectButton,
              "B2 GMM lifecycle controls must exist")) return 1;
    if (check(previewHelper->navigationEnabled() && !sampleRectButton->isChecked() &&
              !previewHelper->isRoiDrawingEnabled(),
              "Template view must enable navigation without entering persisted ROI drawing")) return 1;
    const QRectF restoredRoi = previewHelper->roiRectNormalized();
    sampleRectButton->click();
    if (check(sampleRectButton->isChecked() && previewHelper->isRoiDrawingEnabled(),
              "First sample ROI click must enter drawing mode")) return 1;
    sampleRectButton->click();
    if (check(!sampleRectButton->isChecked() && !previewHelper->isRoiDrawingEnabled() &&
              previewHelper->roiRectNormalized() == restoredRoi,
              "Second sample ROI click must exit drawing while preserving ROI")) return 1;
    if (check(backend->currentIndex() == 1 && !brightness->isChecked(),
              "ab GMM must map to brightness disabled")) return 1;
    if (check(featureRow->isHidden(), "GMM mode must hide the complete HSV feature row")) return 1;
    if (check(roiSamples->currentRow() == 0,
              "Reloaded templates must select the first persisted ROI sample")) return 1;
    if (check(viewerTitle->text() == QStringLiteral("基准图") && preview->scene() &&
              qRound(preview->scene()->sceneRect().width()) == fullReference.cols &&
              qRound(preview->scene()->sceneRect().height()) == fullReference.rows,
              "Selecting a persisted ROI must keep the full reference canvas")) return 1;
    if (check(build->isEnabled(),
              "An empty GMM with persisted samples must enable the build button")) return 1;

    brightness->click();
    QTimer::singleShot(10, []() {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            box->accept();
    });
    build->click();
    ColorRecognitionTemplateData built = dialog.templateData();
    if (check(built.gmmColorChannels == QStringLiteral("lab"),
              "GMM brightness enabled must map to lab")) return 1;
    if (check(built.gmmModel.state == QStringLiteral("ready_with_warning") &&
              built.gmmModel.samplingAlgorithmVersion ==
                  colorRecognitionGmmSamplingAlgorithmVersion() &&
              !built.gmmModel.serializedGmmBase64.isEmpty() &&
              built.gmmModel.classes.size() == 2,
              "GMM build button must persist a validated B1 artifact")) return 1;
    if (check(buildFeedback->property("statusTone").toString() == QStringLiteral("warning") &&
              buildFeedback->text().contains(QStringLiteral("已建立")),
              "Successful limited-sample build must keep warning feedback near the button")) return 1;
    if (check(colorRecognitionGmmTrainingDataHash(built) == built.gmmModel.trainingDataHash,
              "B2 stale signature must match the B1 trainingDataHash contract")) return 1;

    activate(backend, 0);
    if (check(!featureRow->isHidden(), "HSV mode must restore the HSV feature row")) return 1;
    if (check(!brightness->isChecked(),
              "switching to HSV must restore the independent HSV brightness value")) return 1;
    activate(feature, 1);
    brightness->click();
    activate(sensitivity, 2);
    const ColorRecognitionTemplateData hsvChanged = dialog.templateData();
    if (check(hsvChanged.modelState == QStringLiteral("stale"),
              "HSV parameter change must stale HSV samples")) return 1;
    if (check(hsvChanged.gmmModel.state == QStringLiteral("ready_with_warning"),
              "HSV parameter change must not stale GMM")) return 1;
    if (check(hsvState->text().contains(QStringLiteral("失效")) &&
              hsvFeedback->property("statusTone").toString() == QStringLiteral("stale"),
              "Switching to stale HSV must show an actionable status")) return 1;

    QTimer::singleShot(10, []() {
        if (QMessageBox *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            box->accept();
    });
    rebuildHsv->click();
    const ColorRecognitionTemplateData hsvRebuilt = dialog.templateData();
    if (check(hsvRebuilt.modelState == QStringLiteral("ready") &&
              !hsvRebuilt.samples.at(0).featureSignature.isEmpty() &&
              hsvRebuilt.samples.at(0).featureSignature == hsvRebuilt.samples.at(1).featureSignature,
              "HSV rebuild must transactionally regenerate a common strict signature")) return 1;
    if (check(hsvRebuilt.gmmModel.state == QStringLiteral("ready_with_warning"),
              "HSV rebuild must preserve the ready GMM artifact")) return 1;

    activate(backend, 1);
    if (check(brightness->isChecked(),
              "switching back to GMM must restore the independent lab selection")) return 1;
    brightness->click();
    const ColorRecognitionTemplateData gmmChanged = dialog.templateData();
    if (check(gmmChanged.gmmColorChannels == QStringLiteral("ab") &&
              gmmChanged.gmmModel.state == QStringLiteral("stale"),
              "GMM brightness change must map to ab and stale only GMM")) return 1;

    ToolRequest request;
    request.config.toolId = QStringLiteral("gmm-b2-adapter");
    request.config.toolType = ToolType::ColorRecognition;
    request.config.roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    request.image = cv::Mat(40, 40, CV_8UC3, cv::Scalar(0, 0, 255)).clone();
    request.runtimeContext.insert(QStringLiteral("input"), QJsonObject{
                                      {QStringLiteral("pixelFormat"), QStringLiteral("BGR8")},
                                      {QStringLiteral("validBits"), 8},
                                      {QStringLiteral("bitShift"), 0}});
    QJsonObject gmmModel;
    gmmModel.insert(QStringLiteral("state"), built.gmmModel.state);
    gmmModel.insert(QStringLiteral("algorithmVersion"), built.gmmModel.algorithmVersion);
    gmmModel.insert(QStringLiteral("featureSchemaVersion"), built.gmmModel.featureSchemaVersion);
    gmmModel.insert(QStringLiteral("colorChannels"), built.gmmModel.colorChannels);
    gmmModel.insert(QStringLiteral("trainingDataHash"), built.gmmModel.trainingDataHash);
    gmmModel.insert(QStringLiteral("buildParamsHash"), built.gmmModel.buildParamsHash);
    gmmModel.insert(QStringLiteral("serializedGmmBase64"), built.gmmModel.serializedGmmBase64);
    gmmModel.insert(QStringLiteral("serializedSize"), static_cast<double>(built.gmmModel.serializedSize));
    gmmModel.insert(QStringLiteral("serializedSha256"), built.gmmModel.serializedSha256);
    QJsonArray classIdOrder;
    for (int classId : built.gmmModel.classIdOrder) classIdOrder.append(classId);
    gmmModel.insert(QStringLiteral("classIdOrder"), classIdOrder);
    QJsonArray classDiagnostics;
    for (const ColorRecognitionGmmClassDiagnostics &item : built.gmmModel.classes) {
        classDiagnostics.append(QJsonObject{{QStringLiteral("classId"), item.classId},
                                            {QStringLiteral("label"), item.label},
                                            {QStringLiteral("roiCount"), item.roiCount},
                                            {QStringLiteral("availablePixels"), static_cast<double>(item.availablePixels)},
                                            {QStringLiteral("trainingPixels"), item.trainingPixels},
                                            {QStringLiteral("minCenters"), item.minCenters},
                                            {QStringLiteral("maxCenters"), item.maxCenters}});
    }
    gmmModel.insert(QStringLiteral("classes"), classDiagnostics);
    QJsonObject backendModels;
    backendModels.insert(QStringLiteral("cielabGmm"), gmmModel);
    QJsonObject backendConfigs;
    backendConfigs.insert(QStringLiteral("cielabGmm"), QJsonObject{
                              {QStringLiteral("colorChannels"), built.gmmColorChannels},
                              {QStringLiteral("maxSamplesPerClass"), built.gmmMaxSamplesPerClass},
                              {QStringLiteral("gmmRejectionThreshold"), 0.0}});
    QJsonArray labelsJson;
    for (const ColorRecognitionLabelData &label : built.labels)
        labelsJson.append(QJsonObject{{QStringLiteral("name"), label.name},
                                      {QStringLiteral("classId"), label.classId}});
    QJsonObject templateJson;
    templateJson.insert(QStringLiteral("templateId"), QStringLiteral("active"));
    templateJson.insert(QStringLiteral("recognitionBackend"), QStringLiteral("cielab_gmm"));
    templateJson.insert(QStringLiteral("backendConfigs"), backendConfigs);
    templateJson.insert(QStringLiteral("backendModels"), backendModels);
    templateJson.insert(QStringLiteral("labels"), labelsJson);
    QJsonObject colorModel;
    colorModel.insert(QStringLiteral("activeTemplateId"), QStringLiteral("active"));
    colorModel.insert(QStringLiteral("templates"), QJsonArray{templateJson});
    request.config.params.insert(QStringLiteral("colorModel"), colorModel);
    request.config.judgeRule.insert(QStringLiteral("mode"), QStringLiteral("min_score"));
    request.config.judgeRule.insert(QStringLiteral("minScore"), 90);
    request.config.judgeRule.insert(QStringLiteral("minCategoryConfidence"), 90);
    request.config.judgeRule.insert(QStringLiteral("minClassifiedCoverage"), 90);
    const ToolResult routed = ColorRecognitionAdapter().run(request);
    if (check(routed.success && routed.ok && routed.text == QStringLiteral("red") &&
              routed.payload.value(QStringLiteral("backend")).toString() == QStringLiteral("cielab_gmm"),
              "Adapter must complete B3 GMM production detection without HSV fallback")) return 1;

    // 完整样本图只保留在当前编辑会话；列表增删不得把它替换成后来变化的基准图。
    ReferenceImageProvider::instance().setReferenceFrame(
                fullReference, FrameInputMetadata::fromMat(fullReference, QStringLiteral("reference")));
    ColorTemplateDialog sessionSourceDialog;
    sessionSourceDialog.setTemplateData(data);
    QPushButton *addCurrentSource = sessionSourceDialog.findChild<QPushButton *>(
                QStringLiteral("addCurrentSampleImageButton"));
    QPushButton *deleteRoi = sessionSourceDialog.findChild<QPushButton *>(
                QStringLiteral("deleteCurrentRoiSampleButton"));
    FrameViewHelper *sessionPreview = sessionSourceDialog.findChild<FrameViewHelper *>();
    QGraphicsView *sessionView = sessionSourceDialog.findChild<QGraphicsView *>();
    if (check(addCurrentSource && deleteRoi && sessionPreview && sessionView,
              "Session-source ROI controls must expose stable object names")) return 1;
    addCurrentSource->click();
    const cv::Mat replacementReference(36, 54, CV_8UC3, cv::Scalar(90, 80, 70));
    ReferenceImageProvider::instance().setReferenceFrame(
                replacementReference,
                FrameInputMetadata::fromMat(replacementReference, QStringLiteral("reference")));
    deleteRoi->click();
    if (check(sessionPreview->hasImage() &&
              qRound(sessionView->scene()->sceneRect().width()) == fullReference.cols &&
              qRound(sessionView->scene()->sceneRect().height()) == fullReference.rows,
              "Deleting an ROI must preserve the complete image selected for this dialog session")) return 1;

    // 没有任何完整图源时仍只保留后端 ROI 数据，右侧视图不得显示 ROI 裁剪预览。
    ReferenceImageProvider::instance().clearReferenceFrame();
    CameraFrameProvider::instance().clearFrame();
    ColorTemplateDialog roiOnlyDialog;
    roiOnlyDialog.setTemplateData(data);
    FrameViewHelper *roiOnlyPreview = roiOnlyDialog.findChild<FrameViewHelper *>();
    QLabel *roiOnlyTitle = roiOnlyDialog.findChild<QLabel *>(QStringLiteral("viewerTitleLabel"));
    if (check(roiOnlyPreview && roiOnlyTitle && !roiOnlyPreview->hasImage() &&
              roiOnlyTitle->text() == QStringLiteral("当前无图像"),
              "Persisted ROI crops must never replace the complete-image canvas")) return 1;

    std::cout << "color_recognition_gmm_b2_lifecycle_smoke: PASS" << std::endl;
    return 0;
}
