#include "ColorRecognitionDialog.h"
#include "ui_ColorRecognitionDialog.h"

#include "PlanDialogUtils.h"

#include <QButtonGroup>
#include <QByteArray>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QListWidget>
#include <QMessageBox>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QToolButton>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolRequest.h"

namespace {

enum TemplateListRole {
    ItemKindRole = Qt::UserRole,
    TemplateIdRole,
    ClassIdRole,
    SampleIndexRole
};

constexpr int kTemplateItem = 1;
constexpr int kLabelItem = 2;
constexpr int kSampleItem = 3;
constexpr int kLiveTestIntervalMs = 500;

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

QString featureTypeToUi(const QString &value)
{
    return value == QStringLiteral("spectrum")
            ? QStringLiteral("色谱特征")
            : QStringLiteral("直方图特征");
}

void setComboBoxText(QComboBox *comboBox, const QString &text)
{
    if (!comboBox)
        return;

    const int index = comboBox->findText(text);
    if (index >= 0)
        comboBox->setCurrentIndex(index);
}

QJsonObject labelToJson(const ColorRecognitionLabelData &label)
{
    QJsonObject json;
    json.insert(QStringLiteral("name"), label.name);
    json.insert(QStringLiteral("classId"), label.classId);
    return json;
}

ColorRecognitionLabelData labelFromJson(const QJsonObject &json)
{
    ColorRecognitionLabelData label;
    label.name = json.value(QStringLiteral("name")).toString().trimmed();
    label.classId = json.value(QStringLiteral("classId")).toInt();
    return label;
}

QJsonObject sampleToJson(const ColorRecognitionSampleData &sample)
{
    QJsonObject json;
    json.insert(QStringLiteral("label"), sample.label);
    json.insert(QStringLiteral("classId"), sample.classId);
    json.insert(QStringLiteral("feature"), featureToJson(sample.feature));
    json.insert(QStringLiteral("roiNormalized"), rectToJson(sample.roiNormalized));
    json.insert(QStringLiteral("roiImagePngBase64"), sample.roiImagePngBase64);
    json.insert(QStringLiteral("roiImageWidth"), sample.roiImageWidth);
    json.insert(QStringLiteral("roiImageHeight"), sample.roiImageHeight);
    return json;
}

ColorRecognitionSampleData sampleFromJson(const QJsonObject &json)
{
    ColorRecognitionSampleData sample;
    sample.label = json.value(QStringLiteral("label")).toString().trimmed();
    sample.classId = json.value(QStringLiteral("classId")).toInt();
    sample.feature = featureFromJson(json.value(QStringLiteral("feature")).toArray());
    sample.roiNormalized = normalizedRoiOrDefault(
                rectFromJson(json.value(QStringLiteral("roiNormalized")).toObject(),
                             sample.roiNormalized));
    sample.roiImagePngBase64 = json.value(QStringLiteral("roiImagePngBase64")).toString();
    sample.roiImageWidth = json.value(QStringLiteral("roiImageWidth")).toInt();
    sample.roiImageHeight = json.value(QStringLiteral("roiImageHeight")).toInt();
    return sample;
}

QJsonObject templateToJson(const ColorRecognitionTemplateData &colorTemplate)
{
    QJsonArray labels;
    for (const ColorRecognitionLabelData &label : colorTemplate.labels)
        labels.append(labelToJson(label));

    QJsonArray samples;
    for (const ColorRecognitionSampleData &sample : colorTemplate.samples)
        samples.append(sampleToJson(sample));

    QJsonObject json;
    json.insert(QStringLiteral("templateId"), colorTemplate.templateId);
    json.insert(QStringLiteral("name"), colorTemplate.name);
    json.insert(QStringLiteral("featureType"), colorTemplate.featureType);
    json.insert(QStringLiteral("sensitivity"), colorTemplate.sensitivity);
    json.insert(QStringLiteral("brightnessEnabled"), colorTemplate.brightnessEnabled);
    json.insert(QStringLiteral("knnK"), colorTemplate.knnK);
    json.insert(QStringLiteral("knnDistance"), colorTemplate.knnDistance);
    json.insert(QStringLiteral("labels"), labels);
    json.insert(QStringLiteral("samples"), samples);
    return json;
}

ColorRecognitionTemplateData templateFromJson(const QJsonObject &json)
{
    ColorRecognitionTemplateData colorTemplate;
    colorTemplate.templateId = json.value(QStringLiteral("templateId")).toString().trimmed();
    colorTemplate.name = json.value(QStringLiteral("name")).toString(QStringLiteral("颜色模板")).trimmed();
    colorTemplate.featureType = json.value(QStringLiteral("featureType")).toString(QStringLiteral("histogram"));
    colorTemplate.sensitivity = json.value(QStringLiteral("sensitivity")).toString(QStringLiteral("medium"));
    colorTemplate.brightnessEnabled = json.value(QStringLiteral("brightnessEnabled")).toBool(true);
    colorTemplate.knnK = qMax(1, json.value(QStringLiteral("knnK")).toInt(3));
    colorTemplate.knnDistance = json.value(QStringLiteral("knnDistance")).toString(QStringLiteral("halcon_default"));

    const QJsonArray labels = json.value(QStringLiteral("labels")).toArray();
    for (const QJsonValue &value : labels) {
        const ColorRecognitionLabelData label = labelFromJson(value.toObject());
        if (!label.name.isEmpty() && label.classId > 0)
            colorTemplate.labels.append(label);
    }

    const QJsonArray samples = json.value(QStringLiteral("samples")).toArray();
    for (const QJsonValue &value : samples) {
        const ColorRecognitionSampleData sample = sampleFromJson(value.toObject());
        if (!sample.label.isEmpty() && sample.classId > 0 && !sample.feature.isEmpty())
            colorTemplate.samples.append(sample);
    }

    return colorTemplate;
}

int sampleCount(const ColorRecognitionTemplateData &colorTemplate)
{
    return colorTemplate.samples.size();
}

QVector<ToolOverlay> colorRecognitionPreviewOverlaysWithoutRoi(const QVector<ToolOverlay> &overlays)
{
    QVector<ToolOverlay> filtered;
    filtered.reserve(overlays.size());
    for (const ToolOverlay &overlay : overlays) {
        if (overlay.type == ToolOverlayType::Rect &&
            overlay.label.compare(QStringLiteral("ROI"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        filtered.append(overlay);
    }
    return filtered;
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
    m_testRunTimer = new QTimer(this);
    m_testRunTimer->setInterval(kLiveTestIntervalMs);
    connect(m_testRunTimer, &QTimer::timeout, this, &ColorRecognitionDialog::performTestRun);
    m_testRunWatcher = new QFutureWatcher<ToolResult>(this);
    connect(m_testRunWatcher, &QFutureWatcher<ToolResult>::finished, this, [this]() {
        m_testRunBusy = false;
        const ToolResult result = m_testRunWatcher->result();
        const int generation = result.payload.value(QStringLiteral("_testRunGeneration")).toInt();
        const bool referenceSource = result.payload.value(QStringLiteral("_referenceSource")).toBool();
        if (generation != m_testRunGeneration)
            return;
        displayResult(result, referenceSource);
    });
    setupUiState();
    connectControls();
    showPreviewImage();
}

ColorRecognitionDialog::~ColorRecognitionDialog()
{
    stopLiveTestRun();
    if (m_testRunWatcher && m_testRunWatcher->isRunning()) {
        m_testRunWatcher->cancel();
        m_testRunWatcher->waitForFinished();
    }
    delete ui;
}

ColorRecognitionDialogConfig ColorRecognitionDialog::configuration() const
{
    ColorRecognitionDialogConfig config;
    config.templates = m_templates;
    config.activeTemplateId = activeTemplateId();
    config.judgeMode = judgeModeFromUi(ui->resultBasisComboBox->currentText());
    config.minScore = ui->minScoreSpinBox->value();
    config.expectedLabel = ui->expectedLabelComboBox->currentText().trimmed();
    return config;
}

ToolConfig ColorRecognitionDialog::toToolConfig() const
{
    const ColorRecognitionDialogConfig colorConfig = configuration();
    const ColorRecognitionTemplateData *currentTemplate = activeTemplate();

    QJsonArray templateArray;
    for (const ColorRecognitionTemplateData &colorTemplate : colorConfig.templates)
        templateArray.append(templateToJson(colorTemplate));

    QJsonObject colorModel;
    colorModel.insert(QStringLiteral("activeTemplateId"), colorConfig.activeTemplateId);
    colorModel.insert(QStringLiteral("templates"), templateArray);

    QJsonObject params;
    params.insert(QStringLiteral("paramMode"),
                  ui->allSegmentButton->isChecked()
                  ? QStringLiteral("all")
                  : QStringLiteral("basic"));
    params.insert(QStringLiteral("detectRegionType"), QStringLiteral("rectangle"));
    params.insert(QStringLiteral("colorModel"), colorModel);
    if (currentTemplate) {
        params.insert(QStringLiteral("featureType"), currentTemplate->featureType);
        params.insert(QStringLiteral("sensitivity"), currentTemplate->sensitivity);
        params.insert(QStringLiteral("brightnessEnabled"), currentTemplate->brightnessEnabled);
        params.insert(QStringLiteral("knnK"), currentTemplate->knnK);
        params.insert(QStringLiteral("knnDistance"), currentTemplate->knnDistance);
        params.insert(QStringLiteral("knnDistanceApplied"), QStringLiteral("halcon_default"));
    }

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

    m_templates.clear();
    const QJsonArray templates = colorModel.value(QStringLiteral("templates")).toArray();
    for (const QJsonValue &value : templates) {
        ColorRecognitionTemplateData colorTemplate = templateFromJson(value.toObject());
        if (!colorTemplate.templateId.isEmpty() && !colorTemplate.name.isEmpty())
            m_templates.append(colorTemplate);
    }

    if (m_templates.isEmpty()) {
        ColorRecognitionTemplateData legacyTemplate;
        legacyTemplate.templateId = QStringLiteral("legacy_template");
        legacyTemplate.name = colorModel.value(QStringLiteral("modelName")).toString(QStringLiteral("颜色模板"));
        legacyTemplate.featureType = params.value(QStringLiteral("featureType")).toString(QStringLiteral("histogram"));
        legacyTemplate.sensitivity = params.value(QStringLiteral("sensitivity")).toString(QStringLiteral("medium"));
        legacyTemplate.brightnessEnabled = params.value(QStringLiteral("brightnessEnabled")).toBool(true);
        legacyTemplate.knnK = qMax(1, params.value(QStringLiteral("knnK")).toInt(3));
        legacyTemplate.knnDistance = params.value(QStringLiteral("knnDistance")).toString(QStringLiteral("halcon_default"));

        const QJsonArray labels = colorModel.value(QStringLiteral("labels")).toArray();
        for (const QJsonValue &value : labels) {
            const ColorRecognitionLabelData label = labelFromJson(value.toObject());
            if (!label.name.isEmpty() && label.classId > 0)
                legacyTemplate.labels.append(label);
        }
        const QJsonArray samples = colorModel.value(QStringLiteral("samples")).toArray();
        for (const QJsonValue &value : samples) {
            const ColorRecognitionSampleData sample = sampleFromJson(value.toObject());
            if (!sample.label.isEmpty() && sample.classId > 0 && !sample.feature.isEmpty())
                legacyTemplate.samples.append(sample);
        }
        if (!legacyTemplate.labels.isEmpty() || !legacyTemplate.samples.isEmpty())
            m_templates.append(legacyTemplate);
    }

    m_activeTemplateId = colorModel.value(QStringLiteral("activeTemplateId")).toString();
    if (m_activeTemplateId.isEmpty() && !m_templates.isEmpty())
        m_activeTemplateId = m_templates.first().templateId;

    setComboBoxText(ui->resultBasisComboBox,
                    judgeModeToUi(judgeRule.value(QStringLiteral("mode")).toString(QStringLiteral("min_score"))));
    ui->minScoreSpinBox->setValue(judgeRule.value(QStringLiteral("minScore")).toInt(ui->minScoreSpinBox->value()));
    updateTemplateList();
    updateExpectedLabelCombo();
    setComboBoxText(ui->expectedLabelComboBox,
                    judgeRule.value(QStringLiteral("expectedLabel")).toString());
    updateJudgementControls();

    syncRegionButtons(true);
    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    showPreviewImage();
    setViewerStatusText(roiStatusText(), roiStatusText());
}

QString ColorRecognitionDialog::summaryText() const
{
    const ColorRecognitionTemplateData *colorTemplate = activeTemplate();
    if (!colorTemplate)
        return tr("未选择颜色模板；最低分 %1").arg(ui->minScoreSpinBox->value());

    return tr("%1；%2；样本 %3；最低分 %4")
            .arg(colorTemplate->name,
                 featureTypeToUi(colorTemplate->featureType))
            .arg(sampleCount(*colorTemplate))
            .arg(ui->minScoreSpinBox->value());
}

void ColorRecognitionDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void ColorRecognitionDialog::finishConfiguration()
{
    stopLiveTestRun();
    if (m_previewHelper)
        m_previewHelper->setRoiDrawingEnabled(false);
    accept();
}

void ColorRecognitionDialog::runTest()
{
    if (m_liveTestRunning) {
        stopLiveTestRun();
        return;
    }

    m_liveTestRunning = true;
    ++m_testRunGeneration;
    m_displayedSampleIndex = -1;
    ui->testRunButton->setText(tr("停止测试"));
    if (m_previewHelper && !m_globalDetection && m_displayedSampleIndex < 0)
        m_previewHelper->setRoiDrawingEnabled(true);
    refreshDisplayedRoiOverlay();
    performTestRun();
    if (m_testRunTimer)
        m_testRunTimer->start();
}

void ColorRecognitionDialog::performTestRun()
{
    if (m_testRunBusy)
        return;

    cv::Mat frame = m_previewUsesReferenceImage
            ? ReferenceImageProvider::instance().referenceFrame()
            : CameraFrameProvider::instance().currentFrame();
    bool referenceSource = m_previewUsesReferenceImage && !frame.empty();
    if (frame.empty()) {
        frame = ReferenceImageProvider::instance().referenceFrame();
        referenceSource = !frame.empty();
    }
    if (frame.empty()) {
        frame = CameraFrameProvider::instance().currentFrame();
        referenceSource = false;
    }

    if (frame.empty()) {
        displayError(QStringLiteral("image_empty"), tr("当前无基准图或相机图像，无法测试"));
        stopLiveTestRun();
        return;
    }

    ToolRequest request;
    request.config = toToolConfig();
    request.image = frame.clone();
    const int generation = m_testRunGeneration;
    m_testRunBusy = true;
    m_testRunWatcher->setFuture(QtConcurrent::run([request, referenceSource, generation]() mutable {
        ColorRecognitionAdapter adapter;
        ToolResult result = adapter.run(request);
        result.payload.insert(QStringLiteral("_referenceSource"), referenceSource);
        result.payload.insert(QStringLiteral("_testRunGeneration"), generation);
        return result;
    }));
}

void ColorRecognitionDialog::stopLiveTestRun()
{
    if (m_testRunTimer)
        m_testRunTimer->stop();
    ++m_testRunGeneration;
    m_liveTestRunning = false;
    if (ui && ui->testRunButton)
        ui->testRunButton->setText(tr("测试运行"));
    if (m_previewHelper && !m_globalDetection && m_displayedSampleIndex < 0)
        m_previewHelper->setRoiDrawingEnabled(true);
}

void ColorRecognitionDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 颜色识别"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(this);

    ui->minScoreSpinBox->setRange(0, 100);
    ui->minScoreSpinBox->setValue(80);
    ui->viewerTitleLabel->setText(tr("基准图"));
    ui->viewerStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    ui->viewerStatusLabel->setMinimumWidth(0);
    ui->viewerStatusLabel->setWordWrap(false);
    ui->viewerStatusLabel->setTextFormat(Qt::PlainText);
    ui->viewerStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    if (!m_maskCard) {
        m_maskCard = new QFrame(this);
        m_maskCard->setFrameShape(QFrame::NoFrame);
        m_maskCard->setProperty("panelRole", QStringLiteral("configCard"));

        QVBoxLayout *maskLayout = new QVBoxLayout(m_maskCard);
        maskLayout->setContentsMargins(20, 18, 20, 18);
        maskLayout->setSpacing(14);

        QHBoxLayout *headerLayout = new QHBoxLayout;
        QLabel *titleLabel = new QLabel(tr("屏蔽区域"));
        titleLabel->setProperty("role", QStringLiteral("cardTitle"));
        QToolButton *collapseButton = new QToolButton;
        collapseButton->setText(QStringLiteral("⌄"));
        collapseButton->setProperty("role", QStringLiteral("collapseCard"));
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch(1);
        headerLayout->addWidget(collapseButton);
        maskLayout->addLayout(headerLayout);

        QHBoxLayout *editLayout = new QHBoxLayout;
        QLabel *fieldLabel = new QLabel(tr("屏蔽区"));
        fieldLabel->setMinimumWidth(118);
        fieldLabel->setProperty("role", QStringLiteral("rowField"));
        QToolButton *rectButton = new QToolButton;
        rectButton->setText(QStringLiteral("□"));
        rectButton->setMinimumSize(74, 36);
        rectButton->setCheckable(true);
        rectButton->setEnabled(false);
        rectButton->setToolTip(tr("屏蔽区域第一版仅按 UI 预留，暂不参与算法"));
        rectButton->setProperty("actionRole", QStringLiteral("toolbarIcon"));
        editLayout->addWidget(fieldLabel);
        editLayout->addStretch(1);
        editLayout->addWidget(rectButton);
        maskLayout->addLayout(editLayout);

        ui->basicParamsLayout->insertWidget(2, m_maskCard);
    }

    setAllParamsMode(false);
    syncRegionButtons(true);
    updateTemplateList();
    updateJudgementControls();
    setViewerStatusText(roiStatusText(), roiStatusText());
}

void ColorRecognitionDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &ColorRecognitionDialog::reject);
    connect(ui->headerMaximizeButton, &QToolButton::clicked, this, &ColorRecognitionDialog::showMaximized);
    connect(ui->finishButton, &QPushButton::clicked, this, &ColorRecognitionDialog::finishConfiguration);
    connect(ui->testRunButton, &QPushButton::clicked, this, &ColorRecognitionDialog::runTest);
    connect(ui->addTemplateButton, &QPushButton::clicked, this, &ColorRecognitionDialog::addTemplate);
    connect(ui->editTemplateButton, &QPushButton::clicked, this, &ColorRecognitionDialog::editCurrentTemplate);
    connect(ui->importTemplateButton, &QPushButton::clicked, this, &ColorRecognitionDialog::importTemplate);
    connect(ui->exportTemplateButton, &QPushButton::clicked, this, &ColorRecognitionDialog::exportCurrentTemplate);
    connect(ui->renameTemplateButton, &QPushButton::clicked, this, &ColorRecognitionDialog::renameCurrentTemplate);
    connect(ui->deleteTemplateButton, &QPushButton::clicked, this, &ColorRecognitionDialog::deleteCurrentTemplate);
    connect(ui->templateListWidget, &QListWidget::currentRowChanged, this, [this]() {
        if (const ColorRecognitionTemplateData *colorTemplate = activeTemplate())
            m_activeTemplateId = colorTemplate->templateId;
        updateActiveTemplateSummary();
        updateExpectedLabelCombo();
    });
    connect(ui->templateListWidget,
            &QListWidget::itemClicked,
            this,
            &ColorRecognitionDialog::handleTemplateListItemClicked);
    connect(ui->resultBasisComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ColorRecognitionDialog::updateJudgementControls);

    auto connectCollapse = [this](QToolButton *button, const QList<QWidget *> &widgets) {
        if (!button)
            return;
        button->setCheckable(true);
        button->setChecked(false);
        button->setText(QStringLiteral("⌄"));
        connect(button, &QToolButton::clicked, this, [this, button, widgets](bool collapsed) {
            for (QWidget *widget : widgets) {
                if (widget)
                    widget->setVisible(!collapsed);
            }
            button->setText(collapsed ? QStringLiteral("›") : QStringLiteral("⌄"));
            if (!collapsed && button == ui->judgeCollapseButton)
                updateJudgementControls();
        });
    };
    connectCollapse(ui->templateCollapseButton,
                    {ui->templateListWidget,
                     ui->activeTemplateSummaryLabel,
                     ui->addTemplateButton,
                     ui->editTemplateButton,
                     ui->renameTemplateButton,
                     ui->deleteTemplateButton,
                     ui->importTemplateButton,
                     ui->exportTemplateButton});
    connectCollapse(ui->regionCollapseButton,
                    {ui->regionLabel,
                     ui->regionDrawButton,
                     ui->regionRectButton,
                     ui->regionCircleButton});
    connectCollapse(ui->judgeCollapseButton,
                    {ui->resultBasisLabel,
                     ui->resultBasisComboBox,
                     ui->minScoreLabel,
                     ui->minScoreSpinBox,
                     ui->expectedLabelTitleLabel,
                     ui->expectedLabelComboBox});

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
    connect(ui->regionDrawButton, &QToolButton::clicked, this, &ColorRecognitionDialog::startGlobalDetection);
    connect(ui->regionRectButton, &QToolButton::clicked, this, &ColorRecognitionDialog::startRectangleRoiEditing);
    connect(ui->regionCircleButton, &QToolButton::clicked, this, &ColorRecognitionDialog::startRectangleRoiEditing);

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
    ui->colorParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
    if (m_maskCard)
        m_maskCard->setVisible(allMode);
}

void ColorRecognitionDialog::addTemplate()
{
    ColorRecognitionTemplateData colorTemplate;
    colorTemplate.templateId = QStringLiteral("color_template_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    colorTemplate.name = tr("颜色模板%1").arg(m_templates.size() + 1);

    ColorTemplateDialog dialog(this);
    dialog.setTemplateData(colorTemplate);
    dialog.setInitialSampleRoi(effectiveRoiNormalized());
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_templates.append(dialog.templateData());
    m_activeTemplateId = m_templates.last().templateId;
    updateTemplateList();
}

void ColorRecognitionDialog::editCurrentTemplate()
{
    const int index = currentTemplateIndex();
    if (index < 0 || index >= m_templates.size()) {
        QMessageBox::information(this, tr("颜色识别"), tr("请先添加模板"));
        return;
    }

    ColorTemplateDialog dialog(this);
    dialog.setTemplateData(m_templates.at(index));
    dialog.setInitialSampleRoi(effectiveRoiNormalized());
    if (dialog.exec() != QDialog::Accepted)
        return;

    m_templates[index] = dialog.templateData();
    m_activeTemplateId = m_templates.at(index).templateId;
    updateTemplateList();
}

void ColorRecognitionDialog::importTemplate()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("导入颜色模板"),
                QString(),
                tr("Color template (*.bin);;All files (*.*)"));
    if (fileName.trimmed().isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("导入颜色模板"), tr("无法打开模板文件"));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::warning(this,
                             tr("导入颜色模板"),
                             tr("模板文件格式无效：%1").arg(parseError.errorString()));
        return;
    }

    ColorRecognitionTemplateData colorTemplate = templateFromJson(document.object());
    if (colorTemplate.templateId.trimmed().isEmpty())
        colorTemplate.templateId = QStringLiteral("color_template_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (colorTemplate.name.trimmed().isEmpty())
        colorTemplate.name = tr("颜色模板%1").arg(m_templates.size() + 1);

    m_templates.append(colorTemplate);
    m_activeTemplateId = colorTemplate.templateId;
    updateTemplateList();
    setViewerStatusText(tr("已导入模板：%1").arg(colorTemplate.name));
}

void ColorRecognitionDialog::exportCurrentTemplate()
{
    const ColorRecognitionTemplateData *colorTemplate = activeTemplate();
    if (!colorTemplate) {
        QMessageBox::information(this, tr("导出颜色模板"), tr("请先选择模板"));
        return;
    }

    const QString defaultName = colorTemplate->name.trimmed().isEmpty()
            ? QStringLiteral("color_template.bin")
            : QStringLiteral("%1.bin").arg(colorTemplate->name);
    const QString fileName = QFileDialog::getSaveFileName(
                this,
                tr("导出颜色模板"),
                defaultName,
                tr("Color template (*.bin);;All files (*.*)"));
    if (fileName.trimmed().isEmpty())
        return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("导出颜色模板"), tr("无法写入模板文件"));
        return;
    }

    file.write(QJsonDocument(templateToJson(*colorTemplate)).toJson(QJsonDocument::Compact));
    setViewerStatusText(tr("已导出模板：%1").arg(colorTemplate->name));
}

void ColorRecognitionDialog::renameCurrentTemplate()
{
    ColorRecognitionTemplateData *colorTemplate = activeTemplate();
    if (!colorTemplate) {
        QMessageBox::information(this, tr("颜色识别"), tr("请先添加模板"));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("重命名模板"),
                                               tr("模板名"),
                                               QLineEdit::Normal,
                                               colorTemplate->name,
                                               &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    colorTemplate->name = name;
    updateTemplateList();
}

void ColorRecognitionDialog::deleteCurrentTemplate()
{
    const int index = currentTemplateIndex();
    if (index < 0 || index >= m_templates.size())
        return;

    m_templates.removeAt(index);
    m_activeTemplateId = m_templates.isEmpty()
            ? QString()
            : m_templates.at(qMin(index, m_templates.size() - 1)).templateId;
    updateTemplateList();
}

QImage firstTemplateSampleImage(const ColorRecognitionTemplateData &colorTemplate)
{
    for (const ColorRecognitionSampleData &sample : colorTemplate.samples) {
        if (sample.roiImagePngBase64.trimmed().isEmpty())
            continue;
        QImage image;
        image.loadFromData(QByteArray::fromBase64(sample.roiImagePngBase64.toLatin1()), "PNG");
        if (!image.isNull())
            return image;
    }
    return QImage();
}

QWidget *makeTemplateRoiSampleCard(const QImage &sourceImage,
                                   const QString &labelText,
                                   const QString &toolTip,
                                   QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setFrameShape(QFrame::NoFrame);
    card->setToolTip(toolTip);
    card->setStyleSheet(QStringLiteral(
        "QFrame { background:#ffffff; border:1px solid #cfd6df; border-radius:4px; }"
        "QLabel { background:#ffffff; border:0; color:#111827; font-size:12px; }"));

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);

    QLabel *imageLabel = new QLabel;
    imageLabel->setFixedSize(96, 62);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setToolTip(toolTip);
    imageLabel->setStyleSheet(QStringLiteral(
        "QLabel { background:#ffffff; border:1px solid #ff7a00; border-radius:2px; }"));

    if (!sourceImage.isNull()) {
        const QImage scaled = sourceImage.scaled(imageLabel->size(),
                                                Qt::KeepAspectRatioByExpanding,
                                                Qt::SmoothTransformation);
        imageLabel->setPixmap(QPixmap::fromImage(scaled));
    } else {
        imageLabel->setText(QStringLiteral("ROI"));
    }

    QLabel *textLabel = new QLabel(labelText);
    textLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    textLabel->setToolTip(toolTip);
    textLabel->setMinimumHeight(18);

    layout->addWidget(imageLabel, 0, Qt::AlignHCenter);
    layout->addWidget(textLabel);
    return card;
}

void ColorRecognitionDialog::updateTemplateList()
{
    const QString selectedId = activeTemplateId();
    const QSignalBlocker block(ui->templateListWidget);
    ui->templateListWidget->clear();
    ui->templateListWidget->setIconSize(QSize(96, 62));

    int selectedRow = -1;
    for (int i = 0; i < m_templates.size(); ++i) {
        const ColorRecognitionTemplateData &colorTemplate = m_templates.at(i);
        QListWidgetItem *item = new QListWidgetItem(
                    tr("%1（%2 样本）").arg(colorTemplate.name).arg(sampleCount(colorTemplate)));
        const QImage image = firstTemplateSampleImage(colorTemplate);
        if (!image.isNull()) {
            item->setIcon(QIcon(QPixmap::fromImage(image.scaled(QSize(72, 48),
                                                             Qt::KeepAspectRatioByExpanding,
                                                             Qt::SmoothTransformation))));
            item->setSizeHint(QSize(260, 58));
        }
        item->setData(ItemKindRole, kTemplateItem);
        item->setData(TemplateIdRole, colorTemplate.templateId);
        item->setData(SampleIndexRole, -1);
        QFont templateFont = item->font();
        templateFont.setBold(true);
        item->setFont(templateFont);
        ui->templateListWidget->addItem(item);
        if (colorTemplate.templateId == selectedId)
            selectedRow = ui->templateListWidget->count() - 1;

        for (const ColorRecognitionLabelData &label : colorTemplate.labels) {
            int labelSampleCount = 0;
            for (const ColorRecognitionSampleData &sample : colorTemplate.samples) {
                if (sample.classId == label.classId)
                    ++labelSampleCount;
            }
            QListWidgetItem *labelItem = new QListWidgetItem(
                        tr("  %1（%2）").arg(label.name).arg(labelSampleCount));
            labelItem->setData(ItemKindRole, kLabelItem);
            labelItem->setData(TemplateIdRole, colorTemplate.templateId);
            labelItem->setData(ClassIdRole, label.classId);
            labelItem->setData(SampleIndexRole, -1);
            ui->templateListWidget->addItem(labelItem);

            int labelRoiIndex = 1;
            for (int sampleIndex = 0; sampleIndex < colorTemplate.samples.size(); ++sampleIndex) {
                const ColorRecognitionSampleData &sample = colorTemplate.samples.at(sampleIndex);
                if (sample.classId != label.classId)
                    continue;

                const int displayIndex = labelRoiIndex++;
                QListWidgetItem *sampleItem = new QListWidgetItem;
                sampleItem->setText(QString());
                const QString sampleToolTip = tr("%1 ROI %2").arg(label.name).arg(displayIndex);
                sampleItem->setToolTip(sampleToolTip);
                QImage sampleImage;
                if (!sample.roiImagePngBase64.trimmed().isEmpty()) {
                    sampleImage.loadFromData(QByteArray::fromBase64(sample.roiImagePngBase64.toLatin1()),
                                             "PNG");
                }
                sampleItem->setSizeHint(QSize(260, 104));
                sampleItem->setData(ItemKindRole, kSampleItem);
                sampleItem->setData(TemplateIdRole, colorTemplate.templateId);
                sampleItem->setData(ClassIdRole, sample.classId);
                sampleItem->setData(SampleIndexRole, sampleIndex);
                ui->templateListWidget->addItem(sampleItem);
                ui->templateListWidget->setItemWidget(
                            sampleItem,
                            makeTemplateRoiSampleCard(sampleImage,
                                                      label.name,
                                                      sampleToolTip,
                                                      ui->templateListWidget));
            }
        }
    }

    if (selectedRow < 0 && !m_templates.isEmpty())
        selectedRow = 0;
    if (selectedRow >= 0) {
        ui->templateListWidget->setCurrentRow(selectedRow);
        if (QListWidgetItem *item = ui->templateListWidget->item(selectedRow)) {
            const QString templateId = item->data(TemplateIdRole).toString().trimmed();
            if (!templateId.isEmpty())
                m_activeTemplateId = templateId;
        }
    }

    updateActiveTemplateSummary();
    updateExpectedLabelCombo();
}

void ColorRecognitionDialog::updateActiveTemplateSummary()
{
    const ColorRecognitionTemplateData *colorTemplate = activeTemplate();
    const bool hasTemplate = colorTemplate != nullptr;
    ui->editTemplateButton->setEnabled(hasTemplate);
    ui->renameTemplateButton->setEnabled(hasTemplate);
    ui->deleteTemplateButton->setEnabled(hasTemplate);
    ui->exportTemplateButton->setEnabled(hasTemplate);

    if (!colorTemplate) {
        ui->activeTemplateSummaryLabel->setText(tr("未添加模板"));
        ui->allTemplateSummaryLabel->setText(tr("未添加模板"));
        return;
    }

    const QString text = tr("%1 | %2 | 标签 %3 | 样本 %4 | K=%5")
            .arg(colorTemplate->name,
                 featureTypeToUi(colorTemplate->featureType))
            .arg(colorTemplate->labels.size())
            .arg(colorTemplate->samples.size())
            .arg(colorTemplate->knnK);
    ui->activeTemplateSummaryLabel->setText(text);
    ui->allTemplateSummaryLabel->setText(text);
}

void ColorRecognitionDialog::updateExpectedLabelCombo()
{
    const QString currentExpected = ui->expectedLabelComboBox->currentText();
    const QSignalBlocker block(ui->expectedLabelComboBox);
    ui->expectedLabelComboBox->clear();

    if (const ColorRecognitionTemplateData *colorTemplate = activeTemplate()) {
        for (const ColorRecognitionLabelData &label : colorTemplate->labels)
            ui->expectedLabelComboBox->addItem(label.name, label.classId);
    }

    setComboBoxText(ui->expectedLabelComboBox, currentExpected);
}

void ColorRecognitionDialog::updateJudgementControls()
{
    const bool categoryMode = judgeModeFromUi(ui->resultBasisComboBox->currentText()) == QStringLiteral("category");
    ui->minScoreLabel->setVisible(!categoryMode);
    ui->minScoreSpinBox->setVisible(!categoryMode);
    ui->expectedLabelTitleLabel->setVisible(categoryMode);
    ui->expectedLabelComboBox->setVisible(categoryMode);
}

void ColorRecognitionDialog::handleTemplateListItemClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    const QString templateId = item->data(TemplateIdRole).toString().trimmed();
    if (templateId.isEmpty())
        return;

    const int clickedSampleIndex = item->data(ItemKindRole).toInt() == kSampleItem
            ? item->data(SampleIndexRole).toInt()
            : -1;
    const bool hideCurrentSample =
            clickedSampleIndex >= 0 &&
            templateId == m_activeTemplateId &&
            clickedSampleIndex == m_displayedSampleIndex;

    m_activeTemplateId = templateId;
    m_displayedSampleIndex = hideCurrentSample ? -1 : clickedSampleIndex;

    updateActiveTemplateSummary();
    updateExpectedLabelCombo();
    refreshDisplayedRoiOverlay();

    if (m_displayedSampleIndex >= 0) {
        const ColorRecognitionTemplateData *colorTemplate = activeTemplate();
        if (colorTemplate && m_displayedSampleIndex < colorTemplate->samples.size()) {
            const QRectF roi = normalizedRoiOrDefault(
                        colorTemplate->samples.at(m_displayedSampleIndex).roiNormalized);
            const QString text = tr("已选择样本 ROI x=%1 y=%2 w=%3 h=%4")
                    .arg(roi.x(), 0, 'f', 3)
                    .arg(roi.y(), 0, 'f', 3)
                    .arg(roi.width(), 0, 'f', 3)
                    .arg(roi.height(), 0, 'f', 3);
            setViewerStatusText(text, text);
            return;
        }
    }

    setViewerStatusText(roiStatusText(), roiStatusText());
}

int ColorRecognitionDialog::currentTemplateIndex() const
{
    QString id;
    if (ui && ui->templateListWidget) {
        if (const QListWidgetItem *item = ui->templateListWidget->currentItem())
            id = item->data(TemplateIdRole).toString().trimmed();
    }
    if (id.isEmpty())
        id = m_activeTemplateId;

    for (int i = 0; i < m_templates.size(); ++i) {
        if (m_templates.at(i).templateId == id)
            return i;
    }
    return -1;
}

ColorRecognitionTemplateData *ColorRecognitionDialog::activeTemplate()
{
    const int index = currentTemplateIndex();
    return index >= 0 && index < m_templates.size() ? &m_templates[index] : nullptr;
}

const ColorRecognitionTemplateData *ColorRecognitionDialog::activeTemplate() const
{
    const int index = currentTemplateIndex();
    return index >= 0 && index < m_templates.size() ? &m_templates.at(index) : nullptr;
}

QString ColorRecognitionDialog::activeTemplateId() const
{
    if (const ColorRecognitionTemplateData *colorTemplate = activeTemplate())
        return colorTemplate->templateId;
    return m_activeTemplateId;
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
    m_previewUsesReferenceImage = !image.isNull();
    if (image.isNull()) {
        image = CameraFrameProvider::instance().currentImage();
        title = tr("当前图像");
        m_previewUsesReferenceImage = false;
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
    m_previewUsesReferenceImage = !image.isNull();
    if (image.isNull()) {
        image = CameraFrameProvider::instance().currentImage();
        title = tr("当前图像");
        m_previewUsesReferenceImage = false;
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

void ColorRecognitionDialog::startGlobalDetection()
{
    showFrameForRoiEditing();
    m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    m_globalDetection = true;
    m_displayedSampleIndex = -1;
    const QSignalBlocker blockDraw(ui->regionDrawButton);
    const QSignalBlocker blockRect(ui->regionRectButton);
    const QSignalBlocker blockCircle(ui->regionCircleButton);
    ui->regionDrawButton->setChecked(true);
    ui->regionRectButton->setChecked(false);
    ui->regionCircleButton->setChecked(false);
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->clearRoi();
        m_previewHelper->clearToolOverlays();
    }
    const QString text = tr("全局检测：测试运行将检测当前整张图像");
    setViewerStatusText(text, text);
}

void ColorRecognitionDialog::startRectangleRoiEditing()
{
    m_globalDetection = false;
    m_displayedSampleIndex = -1;
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
    startRectangleRoiEditing();
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
    m_globalDetection = false;
    m_displayedSampleIndex = -1;
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
    if (m_displayedSampleIndex >= 0) {
        if (const ColorRecognitionTemplateData *colorTemplate = activeTemplate()) {
            if (m_displayedSampleIndex < colorTemplate->samples.size()) {
                m_previewHelper->setRoiRectNormalized(
                            normalizedRoiOrDefault(colorTemplate->samples.at(m_displayedSampleIndex).roiNormalized));
            }
        }
        return;
    }

    if (!m_globalDetection)
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void ColorRecognitionDialog::displayResult(const ToolResult &result, bool referenceSource)
{
    if (m_previewHelper) {
        const bool keepDetectRoiEditable =
                m_liveTestRunning && !m_globalDetection && m_displayedSampleIndex < 0;
        m_previewHelper->setRoiDrawingEnabled(keepDetectRoiEditable);
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setToolOverlays(keepDetectRoiEditable
                                         ? colorRecognitionPreviewOverlaysWithoutRoi(result.overlays)
                                         : result.overlays);
        if (keepDetectRoiEditable)
            m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
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

    if (referenceSource && result.success) {
        ToolResult snapshotResult = result;
        snapshotResult.overlays = colorRecognitionPreviewOverlaysWithoutRoi(result.overlays);
        m_referencePreviewSnapshot = makeReferenceToolPreviewSnapshot(toToolConfig(),
                                                                      snapshotResult,
                                                                      effectiveRoiNormalized());
    }
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
    if (m_globalDetection)
        return tr("全局检测：检测当前整张图像");

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
