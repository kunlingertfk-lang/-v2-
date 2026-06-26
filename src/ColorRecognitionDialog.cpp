#include "ColorRecognitionDialog.h"
#include "ui_ColorRecognitionDialog.h"

#include "PlanDialogUtils.h"
#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDebug>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QToolButton>
#include <QUuid>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolRequest.h"

namespace {

bool finiteValue(qreal value)
{
    return std::isfinite(static_cast<double>(value));
}

QRectF normalizedRoiOrDefault(const QRectF &roi)
{
    if (!finiteValue(roi.x()) ||
        !finiteValue(roi.y()) ||
        !finiteValue(roi.width()) ||
        !finiteValue(roi.height()) ||
        roi.width() <= 0.0 ||
        roi.height() <= 0.0) {
        return QRectF(0.0, 0.0, 1.0, 1.0);
    }

    const QRectF normalized = roi.normalized();
    const double left = qBound(0.0, normalized.left(), 1.0);
    const double top = qBound(0.0, normalized.top(), 1.0);
    const double right = qBound(0.0, normalized.right(), 1.0);
    const double bottom = qBound(0.0, normalized.bottom(), 1.0);
    const QRectF clamped(QPointF(left, top), QPointF(right, bottom));
    return clamped.width() > 0.0 && clamped.height() > 0.0
            ? clamped.normalized()
            : QRectF(0.0, 0.0, 1.0, 1.0);
}

int labelDisplayWidth(const QLabel *label)
{
    if (!label)
        return 640;

    const int width = label->contentsRect().width();
    return width > 80 ? width : 640;
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

QRectF rectFromJson(const QJsonObject &json, const QRectF &fallback)
{
    if (json.isEmpty())
        return fallback;

    return QRectF(json.value(QStringLiteral("x")).toDouble(fallback.x()),
                  json.value(QStringLiteral("y")).toDouble(fallback.y()),
                  json.value(QStringLiteral("width")).toDouble(fallback.width()),
                  json.value(QStringLiteral("height")).toDouble(fallback.height()));
}

QJsonArray featureToJson(const QVector<double> &feature)
{
    QJsonArray array;
    for (const double value : feature)
        array.append(value);
    return array;
}

QVector<double> featureFromJson(const QJsonArray &array)
{
    QVector<double> feature;
    feature.reserve(array.size());
    for (const QJsonValue &value : array)
        feature.append(value.toDouble());
    return feature;
}

QString featureTypeFromUi(const QString &text)
{
    return text.contains(QStringLiteral("色谱"))
            ? QStringLiteral("spectrum")
            : QStringLiteral("histogram");
}

QString featureTypeToUi(const QString &value)
{
    return value == QStringLiteral("spectrum")
            ? QStringLiteral("色谱特征")
            : QStringLiteral("直方图特征");
}

QString sensitivityFromUi(const QString &text)
{
    if (text.contains(QStringLiteral("低")))
        return QStringLiteral("low");
    if (text.contains(QStringLiteral("高")))
        return QStringLiteral("high");
    return QStringLiteral("medium");
}

QString sensitivityToUi(const QString &value)
{
    if (value == QStringLiteral("low"))
        return QStringLiteral("低敏感");
    if (value == QStringLiteral("high"))
        return QStringLiteral("高敏感");
    return QStringLiteral("中敏感");
}

QString judgeModeFromUi(const QString &text)
{
    return text.contains(QStringLiteral("类别"))
            ? QStringLiteral("category")
            : QStringLiteral("min_score");
}

QString judgeModeToUi(const QString &value)
{
    return value == QStringLiteral("category")
            ? QStringLiteral("类别判断")
            : QStringLiteral("最低分数");
}

void setComboBoxText(QComboBox *comboBox, const QString &text)
{
    if (!comboBox)
        return;

    const int index = comboBox->findText(text);
    if (index >= 0)
        comboBox->setCurrentIndex(index);
}

} // namespace

ColorRecognitionDialog::ColorRecognitionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ColorRecognitionDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_regionGroup(new QButtonGroup(this))
    , m_toolId(QStringLiteral("color_recognition_%1")
                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    ui->setupUi(this);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    connectControls();
    showPreviewImage();
}

ColorRecognitionDialog::~ColorRecognitionDialog()
{
    delete ui;
}

ColorRecognitionDialogConfig ColorRecognitionDialog::configuration() const
{
    ColorRecognitionDialogConfig config;
    config.modelName = ui->modelNameLineEdit->text().trimmed();
    if (config.modelName.isEmpty())
        config.modelName = QStringLiteral("颜色模型");
    config.featureType = featureTypeFromUi(ui->featureTypeComboBox->currentText());
    config.sensitivity = sensitivityFromUi(ui->sensitivityComboBox->currentText());
    config.brightnessEnabled = ui->brightnessCheckBox->isChecked();
    config.knnK = ui->knnKSpinBox->value();
    config.knnDistance = QStringLiteral("halcon_l2");
    config.labels = m_labels;
    config.samples = m_samples;
    config.judgeMode = judgeModeFromUi(ui->resultBasisComboBox->currentText());
    config.minScore = ui->minScoreSpinBox->value();
    config.expectedLabel = ui->expectedLabelComboBox->currentText().trimmed();
    return config;
}

ToolConfig ColorRecognitionDialog::toToolConfig() const
{
    const ColorRecognitionDialogConfig colorConfig = configuration();

    QJsonArray labelArray;
    for (const ColorRecognitionDialogLabel &label : colorConfig.labels) {
        QJsonObject json;
        json.insert(QStringLiteral("name"), label.name);
        json.insert(QStringLiteral("classId"), label.classId);
        labelArray.append(json);
    }

    QJsonArray sampleArray;
    for (const ColorRecognitionDialogSample &sample : colorConfig.samples) {
        QJsonObject json;
        json.insert(QStringLiteral("label"), sample.label);
        json.insert(QStringLiteral("classId"), sample.classId);
        json.insert(QStringLiteral("feature"), featureToJson(sample.feature));
        json.insert(QStringLiteral("roiNormalized"), rectToJson(sample.roiNormalized));
        sampleArray.append(json);
    }

    QJsonObject colorModel;
    colorModel.insert(QStringLiteral("modelName"), colorConfig.modelName);
    colorModel.insert(QStringLiteral("labels"), labelArray);
    colorModel.insert(QStringLiteral("samples"), sampleArray);

    QJsonObject params;
    params.insert(QStringLiteral("paramMode"),
                  ui->colorParamsStackedWidget->currentWidget() == ui->allParamsPage
                  ? QStringLiteral("all")
                  : QStringLiteral("basic"));
    params.insert(QStringLiteral("featureType"), colorConfig.featureType);
    params.insert(QStringLiteral("sensitivity"), colorConfig.sensitivity);
    params.insert(QStringLiteral("brightnessEnabled"), colorConfig.brightnessEnabled);
    params.insert(QStringLiteral("knnK"), colorConfig.knnK);
    params.insert(QStringLiteral("knnDistance"), colorConfig.knnDistance);
    params.insert(QStringLiteral("knnDistanceApplied"), QStringLiteral("halcon_l2_norm"));
    params.insert(QStringLiteral("detectRegionType"), QStringLiteral("rectangle"));
    params.insert(QStringLiteral("colorModel"), colorModel);

    QJsonObject judgeRule;
    judgeRule.insert(QStringLiteral("mode"), colorConfig.judgeMode);
    judgeRule.insert(QStringLiteral("resultBasis"), ui->resultBasisComboBox->currentText());
    judgeRule.insert(QStringLiteral("minScore"), colorConfig.minScore);
    judgeRule.insert(QStringLiteral("expectedLabel"), colorConfig.expectedLabel);

    ToolConfig config;
    config.toolId = m_toolId;
    config.toolName = tr("颜色识别");
    config.toolType = ToolType::ColorRecognition;
    config.category = ToolCategory::Recognition;
    config.enabled = m_enabled;
    config.roiNormalized = effectiveRoiNormalized();
    config.params = params;
    config.judgeRule = judgeRule;
    config.displayName = tr("颜色识别");
    config.summary = summaryText();
    return config;
}

ToolConfig ColorRecognitionDialog::toolConfig() const
{
    return toToolConfig();
}

ToolPreviewSnapshot ColorRecognitionDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void ColorRecognitionDialog::loadFromConfig(const ToolConfig &config)
{
    if (!config.toolId.trimmed().isEmpty())
        m_toolId = config.toolId;
    m_enabled = config.enabled;
    m_roiNormalized = normalizedRoiOrDefault(config.roiNormalized);

    const QJsonObject params = config.params;
    const QJsonObject colorModel = params.value(QStringLiteral("colorModel")).toObject();
    const QJsonObject judgeRule = config.judgeRule;
    setAllParamsMode(params.value(QStringLiteral("paramMode")).toString() == QStringLiteral("all"));
    ui->modelNameLineEdit->setText(colorModel.value(QStringLiteral("modelName"))
                                   .toString(ui->modelNameLineEdit->text()));
    setComboBoxText(ui->featureTypeComboBox,
                    featureTypeToUi(params.value(QStringLiteral("featureType")).toString(QStringLiteral("histogram"))));
    if (ui->featureTypeComboBox->currentText().contains(QStringLiteral("色谱")))
        ui->featureTypeComboBox->setCurrentIndex(0);
    setComboBoxText(ui->sensitivityComboBox,
                    sensitivityToUi(params.value(QStringLiteral("sensitivity")).toString(QStringLiteral("medium"))));
    ui->brightnessCheckBox->setChecked(params.value(QStringLiteral("brightnessEnabled")).toBool(true));
    ui->knnKSpinBox->setValue(params.value(QStringLiteral("knnK")).toInt(ui->knnKSpinBox->value()));

    m_labels.clear();
    const QJsonArray labels = colorModel.value(QStringLiteral("labels")).toArray();
    for (const QJsonValue &value : labels) {
        const QJsonObject json = value.toObject();
        ColorRecognitionDialogLabel label;
        label.name = json.value(QStringLiteral("name")).toString().trimmed();
        label.classId = json.value(QStringLiteral("classId")).toInt();
        if (!label.name.isEmpty() && label.classId > 0)
            m_labels.append(label);
    }

    m_samples.clear();
    const QJsonArray samples = colorModel.value(QStringLiteral("samples")).toArray();
    for (const QJsonValue &value : samples) {
        const QJsonObject json = value.toObject();
        ColorRecognitionDialogSample sample;
        sample.label = json.value(QStringLiteral("label")).toString().trimmed();
        sample.classId = json.value(QStringLiteral("classId")).toInt();
        sample.feature = featureFromJson(json.value(QStringLiteral("feature")).toArray());
        sample.roiNormalized = normalizedRoiOrDefault(
                    rectFromJson(json.value(QStringLiteral("roiNormalized")).toObject(),
                                 sample.roiNormalized));
        if (!sample.label.isEmpty() && sample.classId > 0 && !sample.feature.isEmpty())
            m_samples.append(sample);
    }

    ensureDefaultLabel();
    updateLabelCombos();
    setComboBoxText(ui->resultBasisComboBox,
                    judgeModeToUi(judgeRule.value(QStringLiteral("mode")).toString(QStringLiteral("min_score"))));
    ui->minScoreSpinBox->setValue(judgeRule.value(QStringLiteral("minScore")).toInt(ui->minScoreSpinBox->value()));
    setComboBoxText(ui->expectedLabelComboBox,
                    judgeRule.value(QStringLiteral("expectedLabel")).toString());
    updateJudgementControls();
    updateSampleCount();

    syncRegionButtons(true);
    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    showPreviewImage();
    setViewerStatusText(roiStatusText(), roiStatusText());
}

QString ColorRecognitionDialog::summaryText() const
{
    const ColorRecognitionDialogConfig config = configuration();
    return tr("%1；%2；样本 %3；最低分 %4")
            .arg(config.modelName,
                 featureTypeToUi(config.featureType))
            .arg(config.samples.size())
            .arg(config.minScore);
}

void ColorRecognitionDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void ColorRecognitionDialog::finishConfiguration()
{
    if (m_previewHelper)
        m_previewHelper->setRoiDrawingEnabled(false);
    accept();
}

void ColorRecognitionDialog::runTest()
{
    cv::Mat frame = ReferenceImageProvider::instance().referenceFrame();
    bool referenceSource = !frame.empty();
    if (frame.empty()) {
        frame = CameraFrameProvider::instance().currentFrame();
        referenceSource = false;
    }

    if (frame.empty()) {
        displayError(QStringLiteral("image_empty"), tr("当前无基准图或相机图像，无法测试"));
        return;
    }

    ToolRequest request;
    request.config = toToolConfig();
    request.image = frame.clone();
    const ToolResult result = m_testAdapter.run(request);
    displayResult(result, referenceSource);
}

void ColorRecognitionDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 颜色识别"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(this);

    ui->minScoreSpinBox->setRange(0, 100);
    ui->minScoreSpinBox->setValue(80);
    ui->knnKSpinBox->setRange(1, 99);
    ui->knnKSpinBox->setValue(3);
    ui->brightnessCheckBox->setChecked(true);
    ui->sensitivityComboBox->setCurrentIndex(1);
    ui->knnDistanceComboBox->setEnabled(false);
    ui->viewerTitleLabel->setText(tr("基准图"));
    ui->viewerStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    ui->viewerStatusLabel->setMinimumWidth(0);
    ui->viewerStatusLabel->setWordWrap(false);
    ui->viewerStatusLabel->setTextFormat(Qt::PlainText);
    ui->viewerStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    if (QStandardItemModel *model = qobject_cast<QStandardItemModel *>(ui->featureTypeComboBox->model())) {
        if (QStandardItem *item = model->item(1))
            item->setEnabled(false);
    }

    ensureDefaultLabel();
    updateLabelCombos();
    updateJudgementControls();
    updateSampleCount();
    setAllParamsMode(false);
    syncRegionButtons(true);
    setViewerStatusText(roiStatusText(), roiStatusText());
}

void ColorRecognitionDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &ColorRecognitionDialog::reject);
    connect(ui->headerMaximizeButton, &QToolButton::clicked, this, &ColorRecognitionDialog::showMaximized);
    connect(ui->finishButton, &QPushButton::clicked, this, &ColorRecognitionDialog::finishConfiguration);
    connect(ui->testRunButton, &QPushButton::clicked, this, &ColorRecognitionDialog::runTest);
    connect(ui->addLabelButton, &QPushButton::clicked, this, &ColorRecognitionDialog::addLabel);
    connect(ui->renameLabelButton, &QPushButton::clicked, this, &ColorRecognitionDialog::renameCurrentLabel);
    connect(ui->deleteLabelButton, &QPushButton::clicked, this, &ColorRecognitionDialog::deleteCurrentLabel);
    connect(ui->addSampleButton, &QPushButton::clicked, this, &ColorRecognitionDialog::addSampleFromCurrentRoi);
    connect(ui->labelComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ColorRecognitionDialog::updateSampleCount);
    connect(ui->resultBasisComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ColorRecognitionDialog::updateJudgementControls);

    m_segmentGroup->setExclusive(true);
    m_segmentGroup->addButton(ui->basicSegmentButton, 0);
    m_segmentGroup->addButton(ui->allSegmentButton, 1);
    connect(ui->basicSegmentButton, &QPushButton::clicked, this, [this]() {
        setAllParamsMode(false);
    });
    connect(ui->allSegmentButton, &QPushButton::clicked, this, [this]() {
        setAllParamsMode(true);
    });

    m_regionGroup->setExclusive(true);
    m_regionGroup->addButton(ui->regionDrawButton, 0);
    m_regionGroup->addButton(ui->regionRectButton, 1);
    m_regionGroup->addButton(ui->regionCircleButton, 2);
    connect(ui->regionRectButton, &QToolButton::clicked, this, &ColorRecognitionDialog::startRectangleRoiEditing);
    connect(ui->regionDrawButton, &QToolButton::clicked, this, &ColorRecognitionDialog::showUnsupportedRegionMessage);
    connect(ui->regionCircleButton, &QToolButton::clicked, this, &ColorRecognitionDialog::showUnsupportedRegionMessage);

    if (m_previewHelper) {
        connect(m_previewHelper,
                &FrameViewHelper::roiChanged,
                this,
                &ColorRecognitionDialog::handleRoiChanged);
        connect(m_previewHelper,
                &FrameViewHelper::roiSelectionRejected,
                this,
                [this](const QRectF &) {
            handleRoiSelectionRejected();
        });
    }

    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) {
        showPreviewImage();
    });
}

void ColorRecognitionDialog::setAllParamsMode(bool allMode)
{
    ui->basicSegmentButton->setChecked(!allMode);
    ui->allSegmentButton->setChecked(allMode);
    ui->colorParamsStackedWidget->setCurrentWidget(allMode ? ui->allParamsPage : ui->basicParamsPage);
}

void ColorRecognitionDialog::addLabel()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("添加标签"),
                                               tr("标签名"),
                                               QLineEdit::Normal,
                                               tr("类别%1").arg(nextClassId()),
                                               &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    for (const ColorRecognitionDialogLabel &label : m_labels) {
        if (label.name == name) {
            QMessageBox::warning(this, tr("颜色识别"), tr("标签已存在"));
            return;
        }
    }

    ColorRecognitionDialogLabel label;
    label.name = name;
    label.classId = nextClassId();
    m_labels.append(label);
    updateLabelCombos();
    setComboBoxText(ui->labelComboBox, name);
}

void ColorRecognitionDialog::renameCurrentLabel()
{
    const int index = ui->labelComboBox->currentIndex();
    if (index < 0 || index >= m_labels.size())
        return;

    bool ok = false;
    const QString oldName = m_labels.at(index).name;
    const QString newName = QInputDialog::getText(this,
                                                  tr("重命名标签"),
                                                  tr("标签名"),
                                                  QLineEdit::Normal,
                                                  oldName,
                                                  &ok).trimmed();
    if (!ok || newName.isEmpty() || newName == oldName)
        return;

    for (int i = 0; i < m_labels.size(); ++i) {
        if (i != index && m_labels.at(i).name == newName) {
            QMessageBox::warning(this, tr("颜色识别"), tr("标签已存在"));
            return;
        }
    }

    m_labels[index].name = newName;
    for (ColorRecognitionDialogSample &sample : m_samples) {
        if (sample.classId == m_labels.at(index).classId)
            sample.label = newName;
    }
    updateLabelCombos();
    setComboBoxText(ui->labelComboBox, newName);
}

void ColorRecognitionDialog::deleteCurrentLabel()
{
    const int index = ui->labelComboBox->currentIndex();
    if (index < 0 || index >= m_labels.size())
        return;

    const int classId = m_labels.at(index).classId;
    m_labels.removeAt(index);
    m_samples.erase(std::remove_if(m_samples.begin(),
                                   m_samples.end(),
                                   [classId](const ColorRecognitionDialogSample &sample) {
        return sample.classId == classId;
    }), m_samples.end());
    ensureDefaultLabel();
    updateLabelCombos();
    updateSampleCount();
}

void ColorRecognitionDialog::addSampleFromCurrentRoi()
{
    const QString labelName = currentLabelName();
    const int classId = currentClassId();
    if (labelName.isEmpty() || classId <= 0) {
        QMessageBox::warning(this, tr("颜色识别"), tr("请先创建标签"));
        return;
    }

    cv::Mat frame = ReferenceImageProvider::instance().referenceFrame();
    if (frame.empty())
        frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        displayError(QStringLiteral("image_empty"), tr("当前无基准图或相机图像，无法添加样本"));
        return;
    }

    ColorRecognitionHalconConfig config = featureExtractionConfig();
    const ColorRecognitionHalconFeatureResult featureResult =
            m_featureRunner.extractFeature(frame, config);
    if (!featureResult.success) {
        displayError(featureResult.status, featureResult.message);
        QMessageBox::warning(this, tr("颜色识别"), featureResult.message);
        return;
    }

    ColorRecognitionDialogSample sample;
    sample.label = labelName;
    sample.classId = classId;
    sample.feature = featureResult.feature;
    sample.roiNormalized = effectiveRoiNormalized();
    m_samples.append(sample);
    updateSampleCount();

    const QString text = tr("已添加样本：%1，特征维度 %2")
            .arg(labelName)
            .arg(featureResult.feature.size());
    setViewerStatusText(text, text);
}

void ColorRecognitionDialog::updateJudgementControls()
{
    const bool categoryMode = judgeModeFromUi(ui->resultBasisComboBox->currentText()) == QStringLiteral("category");
    ui->minScoreLabel->setVisible(!categoryMode);
    ui->minScoreSpinBox->setVisible(!categoryMode);
    ui->expectedLabelTitleLabel->setVisible(categoryMode);
    ui->expectedLabelComboBox->setVisible(categoryMode);
}

void ColorRecognitionDialog::updateLabelCombos()
{
    const QString currentLabel = ui->labelComboBox->currentText();
    const QString currentExpected = ui->expectedLabelComboBox->currentText();

    {
        const QSignalBlocker block(ui->labelComboBox);
        ui->labelComboBox->clear();
        for (const ColorRecognitionDialogLabel &label : m_labels)
            ui->labelComboBox->addItem(label.name, label.classId);
    }

    {
        const QSignalBlocker block(ui->expectedLabelComboBox);
        ui->expectedLabelComboBox->clear();
        for (const ColorRecognitionDialogLabel &label : m_labels)
            ui->expectedLabelComboBox->addItem(label.name, label.classId);
    }

    setComboBoxText(ui->labelComboBox, currentLabel);
    setComboBoxText(ui->expectedLabelComboBox, currentExpected);
    updateSampleCount();
}

void ColorRecognitionDialog::updateSampleCount()
{
    const int classId = currentClassId();
    int labelSamples = 0;
    for (const ColorRecognitionDialogSample &sample : m_samples) {
        if (sample.classId == classId)
            ++labelSamples;
    }

    ui->sampleCountLabel->setText(tr("%1 / 总计 %2").arg(labelSamples).arg(m_samples.size()));
}

void ColorRecognitionDialog::ensureDefaultLabel()
{
    if (!m_labels.isEmpty())
        return;

    ColorRecognitionDialogLabel label;
    label.name = tr("类别1");
    label.classId = 1;
    m_labels.append(label);
}

int ColorRecognitionDialog::nextClassId() const
{
    int maxId = 0;
    for (const ColorRecognitionDialogLabel &label : m_labels)
        maxId = qMax(maxId, label.classId);
    return maxId + 1;
}

int ColorRecognitionDialog::currentClassId() const
{
    return ui->labelComboBox->currentData().toInt();
}

QString ColorRecognitionDialog::currentLabelName() const
{
    return ui->labelComboBox->currentText().trimmed();
}

ColorRecognitionHalconConfig ColorRecognitionDialog::featureExtractionConfig() const
{
    ColorRecognitionHalconConfig config;
    QStringList tried;
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(QString(), &tried);
    config.halconSoPathCandidates = tried;
    config.roiNormalized = effectiveRoiNormalized();
    config.featureType = featureTypeFromUi(ui->featureTypeComboBox->currentText());
    config.sensitivity = sensitivityFromUi(ui->sensitivityComboBox->currentText());
    config.brightnessEnabled = ui->brightnessCheckBox->isChecked();
    config.knnK = ui->knnKSpinBox->value();
    return config;
}

void ColorRecognitionDialog::fitPreview()
{
    if (m_previewHelper)
        m_previewHelper->fitToView();
}

void ColorRecognitionDialog::showPreviewImage()
{
    if (!m_previewHelper)
        return;

    QImage image = ReferenceImageProvider::instance().referenceImage();
    QString title = tr("基准图");
    if (image.isNull()) {
        image = CameraFrameProvider::instance().currentImage();
        title = tr("当前图像");
    }

    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("当前无图像"));
        const QString text = tr("当前无图像，ROI 默认全图");
        setViewerStatusText(text, text);
        return;
    }

    ui->viewerTitleLabel->setText(title);
    m_previewHelper->setImage(image);
    refreshDisplayedRoiOverlay();
}

void ColorRecognitionDialog::showFrameForRoiEditing()
{
    if (!m_previewHelper)
        return;

    QImage image = ReferenceImageProvider::instance().referenceImage();
    QString title = tr("基准图");
    if (image.isNull()) {
        image = CameraFrameProvider::instance().currentImage();
        title = tr("当前图像");
    }

    if (image.isNull()) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("当前无图像"));
        const QString text = tr("当前无图像，ROI 默认全图；请先设置基准图或提供当前图像后再框选");
        setViewerStatusText(text, text);
        return;
    }

    ui->viewerTitleLabel->setText(title);
    m_previewHelper->setImage(image);
    m_previewHelper->clearToolOverlays();
    refreshDisplayedRoiOverlay();
}

void ColorRecognitionDialog::startRectangleRoiEditing()
{
    syncRegionButtons(true);
    showFrameForRoiEditing();

    if (!m_previewHelper || !m_previewHelper->hasImage())
        return;

    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    m_previewHelper->setRoiDrawingEnabled(true);
    const QString text = tr("请框选颜色识别矩形 ROI");
    setViewerStatusText(text, text);
}

void ColorRecognitionDialog::showUnsupportedRegionMessage()
{
    syncRegionButtons(true);
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        refreshDisplayedRoiOverlay();
    }
    const QString text = tr("第一版暂未接入自由/圆形区域，当前支持矩形区域");
    setViewerStatusText(text, text);
}

void ColorRecognitionDialog::syncRegionButtons(bool rectangleRegion)
{
    if (!rectangleRegion)
        rectangleRegion = true;

    const QSignalBlocker blockDraw(ui->regionDrawButton);
    const QSignalBlocker blockRect(ui->regionRectButton);
    const QSignalBlocker blockCircle(ui->regionCircleButton);
    ui->regionDrawButton->setChecked(!rectangleRegion);
    ui->regionRectButton->setChecked(rectangleRegion);
    ui->regionCircleButton->setChecked(false);
}

void ColorRecognitionDialog::handleRoiChanged(const QRectF &roi)
{
    m_roiNormalized = normalizedRoiOrDefault(roi);
    syncRegionButtons(true);
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    const QString text = roiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[ColorRecognitionDialog] ROI normalized:" << m_roiNormalized;
}

void ColorRecognitionDialog::handleRoiSelectionRejected()
{
    const QString text = tr("ROI 无效，请拖拽宽高至少 2 像素的矩形");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void ColorRecognitionDialog::refreshDisplayedRoiOverlay()
{
    if (!m_previewHelper)
        return;

    m_previewHelper->clearRoi();
    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void ColorRecognitionDialog::displayResult(const ToolResult &result, bool referenceSource)
{
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setToolOverlays(result.overlays);
    }

    const QString predictedLabel = result.payload.value(QStringLiteral("predictedLabel")).toString(result.text);
    const QString displayText = tr("ColorRecognition: %1 | label:%2 | score:%3 | sample:%4 | %5")
            .arg(result.success ? (result.ok ? QStringLiteral("OK") : QStringLiteral("NG"))
                                : QStringLiteral("error"),
                 predictedLabel,
                 QString::number(result.score, 'f', 2))
            .arg(result.count)
            .arg(result.message);
    setViewerStatusText(displayText, displayText);

    if (referenceSource && result.success)
        m_referencePreviewSnapshot = makeReferenceToolPreviewSnapshot(toToolConfig(), result, effectiveRoiNormalized());
}

void ColorRecognitionDialog::displayError(const QString &status, const QString &message)
{
    ToolResult result;
    result.toolId = m_toolId;
    result.toolType = ToolType::ColorRecognition;
    result.success = false;
    result.ok = false;
    result.status = status;
    result.message = message;
    displayResult(result, false);
}

void ColorRecognitionDialog::setViewerStatusText(const QString &displayText,
                                                 const QString &tooltipText)
{
    if (!ui || !ui->viewerStatusLabel)
        return;

    QLabel *label = ui->viewerStatusLabel;
    const QString elidedText =
            label->fontMetrics().elidedText(displayText,
                                            Qt::ElideRight,
                                            labelDisplayWidth(label));
    label->setText(elidedText);
    label->setToolTip(tooltipText.isEmpty() ? displayText : tooltipText);
}

QString ColorRecognitionDialog::roiStatusText() const
{
    const QRectF roi = effectiveRoiNormalized();
    return tr("矩形检测 ROI x=%1 y=%2 w=%3 h=%4")
            .arg(roi.x(), 0, 'f', 3)
            .arg(roi.y(), 0, 'f', 3)
            .arg(roi.width(), 0, 'f', 3)
            .arg(roi.height(), 0, 'f', 3);
}

QRectF ColorRecognitionDialog::effectiveRoiNormalized() const
{
    return normalizedRoiOrDefault(m_roiNormalized);
}
