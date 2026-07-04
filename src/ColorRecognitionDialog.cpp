#include "ColorRecognitionDialog.h"
#include "ui_ColorRecognitionDialog.h"

#include "PlanDialogUtils.h"

#include <QButtonGroup>
#include <QBrush>
#include <QByteArray>
#include <QCheckBox>
#include <QColor>
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
#include <QWidget>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
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
constexpr int kActionButtonFlashMs = 120;

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

QJsonObject pointToJson(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
}

QPointF pointFromJson(const QJsonObject &json)
{
    return QPointF(json.value(QStringLiteral("x")).toDouble(),
                   json.value(QStringLiteral("y")).toDouble());
}

QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points)
        array.append(pointToJson(point));
    return array;
}

QVector<QPointF> pointsFromJson(const QJsonArray &array)
{
    QVector<QPointF> points;
    points.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QPointF point = pointFromJson(value.toObject());
        if (finiteValue(point.x()) && finiteValue(point.y()))
            points.append(QPointF(qBound(0.0, point.x(), 1.0),
                                  qBound(0.0, point.y(), 1.0)));
    }
    return points;
}

QJsonObject circleToJson(const CircleRoi &circle)
{
    QJsonObject json;
    json.insert(QStringLiteral("center"), pointToJson(circle.centerNormalized));
    json.insert(QStringLiteral("radius"), circle.radiusNormalized);
    json.insert(QStringLiteral("boundingRect"), rectToJson(circle.boundingRectNormalized));
    json.insert(QStringLiteral("valid"), circle.valid);
    return json;
}

CircleRoi circleFromJson(const QJsonObject &json)
{
    CircleRoi circle;
    circle.centerNormalized = pointFromJson(json.value(QStringLiteral("center")).toObject());
    circle.radiusNormalized = json.value(QStringLiteral("radius")).toDouble();
    const QRectF fallback(circle.centerNormalized.x() - circle.radiusNormalized,
                          circle.centerNormalized.y() - circle.radiusNormalized,
                          circle.radiusNormalized * 2.0,
                          circle.radiusNormalized * 2.0);
    const QJsonObject rectJson = json.value(QStringLiteral("boundingRect")).toObject();
    const QRectF boundingRect(rectJson.value(QStringLiteral("x")).toDouble(fallback.x()),
                              rectJson.value(QStringLiteral("y")).toDouble(fallback.y()),
                              rectJson.value(QStringLiteral("width")).toDouble(fallback.width()),
                              rectJson.value(QStringLiteral("height")).toDouble(fallback.height()));
    circle.boundingRectNormalized = normalizedRoiOrDefault(boundingRect);
    circle.valid = json.value(QStringLiteral("valid")).toBool(circle.radiusNormalized > 0.0) &&
            finiteValue(circle.centerNormalized.x()) &&
            finiteValue(circle.centerNormalized.y()) &&
            finiteValue(circle.radiusNormalized) &&
            circle.radiusNormalized > 0.0;
    return circle;
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

QString colorDecisionModeFromUi(const QString &text)
{
    return text.contains(QStringLiteral("整体"))
            ? QStringLiteral("histogram_intersection")
            : QStringLiteral("dominant_ratio");
}

QString colorDecisionModeToUi(const QString &value)
{
    return value == QStringLiteral("histogram_intersection")
            ? QStringLiteral("整体相似度")
            : QStringLiteral("主颜色占比");
}

QString featureTypeToUi(const QString &value)
{
    return value == QStringLiteral("spectrum")
            ? QStringLiteral("色谱特征")
            : QStringLiteral("直方图特征");
}

void applyBottomActionButtonMetrics(QPushButton *button)
{
    if (!button)
        return;

    button->setMinimumSize(120, 48);
    button->setMaximumSize(120, 48);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setAutoDefault(false);
    button->setDefault(false);
}

void refreshButtonStyle(QWidget *button)
{
    if (!button)
        return;

    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
}

void installActionButtonFlash(QPushButton *button)
{
    if (!button)
        return;

    QObject::connect(button, &QPushButton::clicked, button, [button]() {
        button->setProperty("flash", true);
        refreshButtonStyle(button);
        QTimer::singleShot(kActionButtonFlashMs, button, [button]() {
            button->setProperty("flash", false);
            refreshButtonStyle(button);
        });
    });
}

void disableDialogDefaultButtonGrowth(QWidget *root)
{
    if (!root)
        return;

    const QList<QPushButton *> buttons = root->findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (!button)
            continue;
        button->setAutoDefault(false);
        button->setDefault(false);
    }
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
        if (overlay.type == ToolOverlayType::Circle &&
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
        if (generation == m_testRunGeneration)
            displayResult(result, referenceSource);
        // 检测在途期间又有新请求（运行一次 / 恢复连续 / 改 ROI）时，结束后补跑一次，避免点击被吞。
        if (m_pendingRerun) {
            m_pendingRerun = false;
            rerunLiveTest();
        }
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
    params.insert(QStringLiteral("detectRegionType"),
                  (!m_globalDetection && m_detectRegionType == QStringLiteral("circle") && m_circleRoiNormalized.valid)
                  ? QStringLiteral("circle")
                  : QStringLiteral("rectangle"));
    params.insert(QStringLiteral("detectCircleNormalized"),
                  (!m_globalDetection && m_circleRoiNormalized.valid)
                  ? circleToJson(m_circleRoiNormalized)
                  : circleToJson(CircleRoi()));
    params.insert(QStringLiteral("detectMaskType"), m_maskPolygonNormalized.size() >= 3
                  ? QStringLiteral("polygon")
                  : QStringLiteral("none"));
    params.insert(QStringLiteral("detectMaskPolygon"), pointsToJson(m_maskPolygonNormalized));
    params.insert(QStringLiteral("detectMaskApplied"), false);
    params.insert(QStringLiteral("detectMaskReason"),
                  m_maskPolygonNormalized.size() >= 3
                  ? QStringLiteral("UI configured; HALCON color runner applies mask during detection")
                  : QStringLiteral("not configured"));
    params.insert(QStringLiteral("enablePositionCorrection"),
                  ui->positionCorrectionSwitch->isChecked());
    params.insert(QStringLiteral("positionCorrectionSource"),
                  ui->positionCorrectionSourceComboBox->currentText());
    params.insert(QStringLiteral("colorDecisionMode"),
                  colorDecisionModeFromUi(ui->colorDecisionModeComboBox->currentText()));
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
    m_detectRegionType = params.value(QStringLiteral("detectRegionType")).toString(QStringLiteral("rectangle")).trimmed().toLower();
    m_circleRoiNormalized = circleFromJson(params.value(QStringLiteral("detectCircleNormalized")).toObject());
    if (m_detectRegionType == QStringLiteral("circle") && m_circleRoiNormalized.valid)
        m_roiNormalized = normalizedRoiOrDefault(m_circleRoiNormalized.boundingRectNormalized);
    else
        m_detectRegionType = QStringLiteral("rectangle");
    m_maskPolygonNormalized = pointsFromJson(params.value(QStringLiteral("detectMaskPolygon")).toArray());
    if (m_maskPolygonNormalized.size() < 3)
        m_maskPolygonNormalized.clear();
    m_maskEditing = false;
    ui->positionCorrectionSwitch->setChecked(
                params.value(QStringLiteral("enablePositionCorrection")).toBool(false));
    setComboBoxText(ui->positionCorrectionSourceComboBox,
                    params.value(QStringLiteral("positionCorrectionSource"))
                    .toString(QStringLiteral("1 基准图.位置修正信息")));

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
    setComboBoxText(ui->colorDecisionModeComboBox,
                    colorDecisionModeToUi(params.value(QStringLiteral("colorDecisionMode"))
                                          .toString(QStringLiteral("dominant_ratio"))));
    ui->minScoreSpinBox->setValue(judgeRule.value(QStringLiteral("minScore")).toInt(ui->minScoreSpinBox->value()));
    updateTemplateList();
    updateExpectedLabelCombo();
    setComboBoxText(ui->expectedLabelComboBox,
                    judgeRule.value(QStringLiteral("expectedLabel")).toString());
    updateJudgementControls();
    syncMaskControls();
    refreshPositionCorrectionControls();

    if (m_detectRegionType == QStringLiteral("circle") && m_circleRoiNormalized.valid) {
        const QSignalBlocker blockDraw(ui->regionDrawButton);
        const QSignalBlocker blockRect(ui->regionRectButton);
        const QSignalBlocker blockCircle(ui->regionCircleButton);
        ui->regionDrawButton->setChecked(false);
        ui->regionRectButton->setChecked(false);
        ui->regionCircleButton->setChecked(true);
    } else {
        syncRegionButtons(true);
    }
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
    if (m_testUiMode != TestUiMode::Edit) {
        runOnceInTestMode();
        return;
    }

    stopLiveTestRun();
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setCircleDrawingEnabled(false);
    }
    accept();
}

void ColorRecognitionDialog::runTest()
{
    if (m_testUiMode == TestUiMode::Continuous) {
        stopLiveTestRun();
        m_testUiMode = TestUiMode::TestPaused;
        updateBottomButtons();
        setViewerStatusText(tr("连续运行已停止，视图保留最后一帧，可继续绘制检测区域即时重测"));
        return;
    }

    m_testUiMode = TestUiMode::Continuous;
    m_liveTestSource = LiveTestSource::Camera;
    m_liveTestRunning = true;
    m_displayedSampleIndex = -1;
    updateBottomButtons();
    if (m_previewHelper && !m_maskEditing && !m_globalDetection && m_displayedSampleIndex < 0) {
        const bool circleMode = m_detectRegionType == QStringLiteral("circle");
        m_previewHelper->setRoiDrawingEnabled(!circleMode);
        m_previewHelper->setCircleDrawingEnabled(circleMode);
    }
    refreshDisplayedRoiOverlay();
    performTestRun();
    if (m_testRunTimer)
        m_testRunTimer->start();
}

void ColorRecognitionDialog::performTestRun()
{
    // 连续运行定时器触发：以最新相机帧重跑（m_liveTestSource == Camera, Continuous）。
    rerunLiveTest();
}

void ColorRecognitionDialog::rerunLiveTest()
{
    if (m_liveTestSource == LiveTestSource::None)
        return;

    cv::Mat frame;
    bool referenceSource = false;
    if (m_liveTestSource == LiveTestSource::Reference) {
        frame = ReferenceImageProvider::instance().referenceFrame();
        referenceSource = true;
        if (frame.empty()) {
            displayError(QStringLiteral("no_reference_image"), tr("请先设置基准图"));
            return;
        }
    } else {
        // Camera：连续态用最新帧，停止/运行一次态用缓存的快照帧（即上一帧检测结果所在帧）。
        if (m_testUiMode == TestUiMode::Continuous)
            frame = CameraFrameProvider::instance().currentFrame();
        else
            frame = m_liveTestFrameSnapshot;
        if (frame.empty()) {
            displayError(QStringLiteral("image_empty"), tr("当前图像为空"));
            if (m_testUiMode == TestUiMode::Continuous) {
                stopLiveTestRun();
                m_testUiMode = TestUiMode::TestPaused;
                updateBottomButtons();
            }
            return;
        }
    }

    launchDetection(frame, referenceSource);
}

void ColorRecognitionDialog::launchDetection(const cv::Mat &frame, bool referenceSource)
{
    if (m_testRunBusy) {
        // 检测在途：标记待补跑，等当前结束后用最新来源重跑，避免点击/绘制被吞。
        m_pendingRerun = true;
        return;
    }
    if (frame.empty())
        return;

    const QImage image = MatImageConverter::matToDisplayImage(frame, QStringLiteral("ColorRecognitionDialog"));
    if (!image.isNull() && m_previewHelper) {
        ui->viewerTitleLabel->setText(referenceSource ? tr("基准图") : tr("测试图像"));
        m_previewHelper->setImage(image);
        refreshDisplayedRoiOverlay();
    }

    if (!referenceSource)
        m_liveTestFrameSnapshot = frame;  // 缓存为停止态重测的快照帧

    ToolRequest request;
    request.config = toToolConfig();
    request.image = frame.clone();
    const int generation = ++m_testRunGeneration;
    m_testRunBusy = true;
    m_testRunWatcher->setFuture(QtConcurrent::run([request, referenceSource, generation]() mutable {
        ColorRecognitionAdapter adapter;
        ToolResult result = adapter.run(request);
        result.payload.insert(QStringLiteral("_referenceSource"), referenceSource);
        result.payload.insert(QStringLiteral("_testRunGeneration"), generation);
        return result;
    }));
}

void ColorRecognitionDialog::runReferenceTest()
{
    stopLiveTestRun();
    m_liveTestSource = LiveTestSource::Reference;
    m_testUiMode = TestUiMode::Edit;
    updateBottomButtons();

    const cv::Mat frame = ReferenceImageProvider::instance().referenceFrame();
    if (frame.empty()) {
        displayError(QStringLiteral("no_reference_image"), tr("请先设置基准图"));
        return;
    }
    launchDetection(frame, true);
    setViewerStatusText(tr("已进入基准图测试，可继续绘制检测区域，松开即自动判别"));
}

void ColorRecognitionDialog::runOnceInTestMode()
{
    stopLiveTestRun();
    m_testUiMode = TestUiMode::TestPaused;
    m_liveTestSource = LiveTestSource::Camera;
    m_liveTestFrameSnapshot = CameraFrameProvider::instance().currentFrame();
    updateBottomButtons();
    rerunLiveTest();
    setViewerStatusText(tr("已运行一次，视图锁定当前帧，可继续绘制检测区域即时重测"));
}

void ColorRecognitionDialog::exitTestMode()
{
    stopLiveTestRun();
    m_liveTestSource = LiveTestSource::None;
    m_testUiMode = TestUiMode::Edit;
    updateBottomButtons();
    showPreviewImage();
}

void ColorRecognitionDialog::stopLiveTestRun()
{
    if (m_testRunTimer)
        m_testRunTimer->stop();
    ++m_testRunGeneration;
    m_liveTestRunning = false;
    if (m_previewHelper && !m_maskEditing && !m_globalDetection && m_displayedSampleIndex < 0) {
        const bool circleMode = m_detectRegionType == QStringLiteral("circle");
        m_previewHelper->setRoiDrawingEnabled(!circleMode);
        m_previewHelper->setCircleDrawingEnabled(circleMode);
    }
}

void ColorRecognitionDialog::updateBottomButtons()
{
    const bool testMode = m_testUiMode != TestUiMode::Edit;
    if (m_referenceTestButton)
        m_referenceTestButton->setVisible(!testMode);
    if (m_exitTestButton)
        m_exitTestButton->setVisible(testMode);
    if (ui && ui->finishButton)
        ui->finishButton->setText(testMode ? tr("运行一次") : tr("完成"));
    if (ui && ui->testRunButton) {
        ui->testRunButton->setText(m_testUiMode == TestUiMode::Continuous ? tr("停止运行") :
                                   testMode ? tr("连续运行") : tr("测试运行"));
        ui->testRunButton->setProperty("running", m_testUiMode == TestUiMode::Continuous);
        refreshButtonStyle(ui->testRunButton);
    }
}

void ColorRecognitionDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 颜色识别"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(this);
    setStyleSheet(styleSheet() + QStringLiteral(
        "QPushButton[actionRole=\"testPrimary\"]{background:#111827;color:#ffffff;border:1px solid #111827;border-radius:4px;padding:0;font-size:15px;font-weight:600;min-width:120px;min-height:48px;}"
        "QPushButton[actionRole=\"testPrimary\"]:hover{background:#000;border-color:#000;}"
        "QPushButton[actionRole=\"testPrimary\"]:pressed,QPushButton[actionRole=\"testPrimary\"][flash=\"true\"]{background:#ffffff;color:#111827;border-color:#111827;}"
        "QPushButton[actionRole=\"testPrimary\"]:disabled{background:#e5e7eb;color:#9ca3af;border-color:#e5e7eb;}"
        "QPushButton[actionRole=\"testAction\"]{background:#ffffff;color:#111827;border:1px solid #9ca3af;border-radius:4px;padding:0;font-size:15px;font-weight:600;min-width:120px;min-height:48px;}"
        "QPushButton[actionRole=\"testAction\"]:hover{background:#f9fafb;border-color:#111827;}"
        "QPushButton[actionRole=\"testAction\"]:pressed,QPushButton[actionRole=\"testAction\"][flash=\"true\"]{background:#111827;color:#ffffff;border-color:#111827;}"
        "QPushButton[actionRole=\"testAction\"][running=\"true\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"testAction\"][running=\"true\"]:hover{background:#e66e00;border-color:#e66e00;}"
        "QPushButton[actionRole=\"testAction\"]:disabled{background:#f3f4f6;color:#9ca3af;border-color:#e5e7eb;}"
        "QPushButton#exitTestButton{background:transparent;color:#6b7280;border:1px solid #d1d5db;}"
        "QPushButton#exitTestButton:hover{background:#fef2f2;color:#dc2626;border-color:#dc2626;}"
        "QPushButton#exitTestButton:pressed,QPushButton#exitTestButton[flash=\"true\"]{background:#dc2626;color:#ffffff;border-color:#dc2626;}"
        "QPushButton#exitTestButton:disabled{background:#f3f4f6;color:#9ca3af;border-color:#e5e7eb;}"
        "QToolButton:pressed{background:#ffe1bf;color:#ff7a00;border-color:#ff7a00;}"));

    ui->minScoreSpinBox->setRange(0, 100);
    ui->minScoreSpinBox->setValue(80);
    ui->viewerTitleLabel->setText(tr("基准图"));
    ui->previewGraphicsView->setBackgroundBrush(QBrush(QColor(255, 255, 255)));
    ui->viewerStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    ui->viewerStatusLabel->setMinimumWidth(0);
    ui->viewerStatusLabel->setWordWrap(false);
    ui->viewerStatusLabel->setTextFormat(Qt::PlainText);
    ui->viewerStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->positionCorrectionSwitch->setObjectName(QStringLiteral("positionCorrectionSwitch"));
    ui->positionCorrectionSwitch->setChecked(false);
    setComboBoxText(ui->positionCorrectionSourceComboBox,
                    QStringLiteral("1 基准图.位置修正信息"));
    refreshPositionCorrectionControls();
    applyBottomActionButtonMetrics(ui->testRunButton);
    applyBottomActionButtonMetrics(ui->finishButton);
    ui->testRunButton->setProperty("actionRole", QStringLiteral("testAction"));
    ui->testRunButton->setProperty("running", false);
    ui->finishButton->setProperty("actionRole", QStringLiteral("testPrimary"));
    installActionButtonFlash(ui->testRunButton);
    installActionButtonFlash(ui->finishButton);

    if (!m_referenceTestButton) {
        m_referenceTestButton = new QPushButton(tr("基准图测试"), this);
        applyBottomActionButtonMetrics(m_referenceTestButton);
        m_referenceTestButton->setProperty("actionRole", QStringLiteral("testAction"));
        installActionButtonFlash(m_referenceTestButton);
        ui->bottomButtonLayout->insertWidget(1, m_referenceTestButton);
    }

    if (!m_exitTestButton) {
        m_exitTestButton = new QPushButton(tr("退出测试"), this);
        m_exitTestButton->setObjectName(QStringLiteral("exitTestButton"));
        applyBottomActionButtonMetrics(m_exitTestButton);
        m_exitTestButton->setProperty("actionRole", QStringLiteral("testAction"));
        installActionButtonFlash(m_exitTestButton);
        ui->bottomButtonLayout->addWidget(m_exitTestButton);
    }

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
        collapseButton->setCheckable(true);
        collapseButton->setProperty("role", QStringLiteral("collapseCard"));
        headerLayout->addWidget(titleLabel);
        headerLayout->addStretch(1);
        headerLayout->addWidget(collapseButton);
        maskLayout->addLayout(headerLayout);

        QWidget *maskContent = new QWidget(m_maskCard);
        QVBoxLayout *maskContentLayout = new QVBoxLayout(maskContent);
        maskContentLayout->setContentsMargins(0, 0, 0, 0);
        maskContentLayout->setSpacing(10);

        QHBoxLayout *editLayout = new QHBoxLayout;
        QLabel *fieldLabel = new QLabel(tr("屏蔽区"));
        fieldLabel->setMinimumWidth(118);
        fieldLabel->setProperty("role", QStringLiteral("rowField"));
        m_maskEditButton = new QPushButton(tr("编辑"));
        m_maskEditButton->setProperty("actionRole", QStringLiteral("plain"));
        m_maskEditButton->setToolTip(tr("编辑屏蔽区域"));
        m_maskPolygonButton = new QToolButton;
        m_maskPolygonButton->setText(QStringLiteral("⬡"));
        m_maskPolygonButton->setMinimumSize(74, 36);
        m_maskPolygonButton->setCheckable(true);
        m_maskPolygonButton->setToolTip(tr("绘制多边形屏蔽区域"));
        m_maskPolygonButton->setProperty("actionRole", QStringLiteral("toolbarIcon"));
        m_maskFinishButton = new QPushButton(tr("完成"));
        m_maskFinishButton->setProperty("actionRole", QStringLiteral("plain"));
        editLayout->addWidget(fieldLabel);
        editLayout->addStretch(1);
        editLayout->addWidget(m_maskEditButton);
        editLayout->addWidget(m_maskPolygonButton);
        editLayout->addWidget(m_maskFinishButton);
        maskContentLayout->addLayout(editLayout);
        maskLayout->addWidget(maskContent);
        connect(collapseButton, &QToolButton::clicked, this, [collapseButton, maskContent](bool collapsed) {
            maskContent->setVisible(!collapsed);
            collapseButton->setText(collapsed ? QStringLiteral("›") : QStringLiteral("⌄"));
        });

        ui->basicParamsLayout->insertWidget(2, m_maskCard);
    }

    setAllParamsMode(false);
    if (m_detectRegionType == QStringLiteral("circle") && m_circleRoiNormalized.valid) {
        const QSignalBlocker blockDraw(ui->regionDrawButton);
        const QSignalBlocker blockRect(ui->regionRectButton);
        const QSignalBlocker blockCircle(ui->regionCircleButton);
        ui->regionDrawButton->setChecked(false);
        ui->regionRectButton->setChecked(false);
        ui->regionCircleButton->setChecked(true);
    } else {
        syncRegionButtons(true);
    }
    updateTemplateList();
    updateJudgementControls();
    syncMaskControls();
    setViewerStatusText(roiStatusText(), roiStatusText());
    disableDialogDefaultButtonGrowth(this);
    updateBottomButtons();
}

void ColorRecognitionDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &ColorRecognitionDialog::reject);
    connect(ui->headerMaximizeButton, &QToolButton::clicked, this, &ColorRecognitionDialog::showMaximized);
    connect(ui->finishButton, &QPushButton::clicked, this, &ColorRecognitionDialog::finishConfiguration);
    connect(ui->testRunButton, &QPushButton::clicked, this, &ColorRecognitionDialog::runTest);
    if (m_referenceTestButton)
        connect(m_referenceTestButton, &QPushButton::clicked, this, &ColorRecognitionDialog::runReferenceTest);
    if (m_exitTestButton)
        connect(m_exitTestButton, &QPushButton::clicked, this, &ColorRecognitionDialog::exitTestMode);
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
    connect(ui->positionCorrectionSwitch,
            &QCheckBox::toggled,
            this,
            &ColorRecognitionDialog::refreshPositionCorrectionControls);
    if (m_maskEditButton)
        connect(m_maskEditButton, &QPushButton::clicked, this, &ColorRecognitionDialog::startMaskEditing);
    if (m_maskPolygonButton)
        connect(m_maskPolygonButton, &QToolButton::clicked, this, &ColorRecognitionDialog::startMaskPolygonDrawing);
    if (m_maskFinishButton)
        connect(m_maskFinishButton, &QPushButton::clicked, this, &ColorRecognitionDialog::finishMaskEditing);

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
                     ui->regionCircleButton,
                     ui->positionCorrectionEnableLabel,
                     ui->positionCorrectionSwitch,
                     ui->positionCorrectionSourceRow});
    connectCollapse(ui->judgeCollapseButton,
                    {ui->resultBasisLabel,
                     ui->resultBasisComboBox,
                     ui->colorDecisionModeLabel,
                     ui->colorDecisionModeComboBox,
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
    connect(ui->regionCircleButton, &QToolButton::clicked, this, &ColorRecognitionDialog::startCircleRoiEditing);

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
        connect(m_previewHelper,
                &FrameViewHelper::circleChanged,
                this,
                &ColorRecognitionDialog::handleCircleRoiChanged);
        connect(m_previewHelper,
                &FrameViewHelper::circleSelectionRejected,
                this,
                &ColorRecognitionDialog::handleCircleRoiSelectionRejected);
        connect(m_previewHelper,
                &FrameViewHelper::polygonChanged,
                this,
                &ColorRecognitionDialog::handleMaskPolygonChanged);
        connect(m_previewHelper,
                &FrameViewHelper::polygonSelectionRejected,
                this,
                &ColorRecognitionDialog::handleMaskPolygonSelectionRejected);
    }

    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) {
        if (m_testUiMode == TestUiMode::Edit)
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
    m_displayedSampleIndex = -1;
    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    ++m_testRunGeneration;
    m_pendingRerun = false;
    updateTemplateList();
    refreshDisplayedRoiOverlay();
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
    m_detectRegionType = QStringLiteral("rectangle");
    m_circleRoiNormalized = CircleRoi();
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
        m_previewHelper->setCircleDrawingEnabled(false);
        m_previewHelper->clearRoi();
        m_previewHelper->clearCircleRoi();
        m_previewHelper->clearToolOverlays();
    }
    const QString text = tr("全局检测：测试运行将检测当前整张图像");
    setViewerStatusText(text, text);
}

void ColorRecognitionDialog::startRectangleRoiEditing()
{
    m_globalDetection = false;
    m_detectRegionType = QStringLiteral("rectangle");
    m_circleRoiNormalized = CircleRoi();
    m_displayedSampleIndex = -1;
    syncRegionButtons(true);
    showFrameForRoiEditing();

    if (!m_previewHelper || !m_previewHelper->hasImage())
        return;

    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    m_previewHelper->clearCircleRoi();
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->setRoiDrawingEnabled(true);
    const QString text = tr("请框选颜色识别矩形 ROI");
    setViewerStatusText(text, text);
}

void ColorRecognitionDialog::startCircleRoiEditing()
{
    m_globalDetection = false;
    m_detectRegionType = QStringLiteral("circle");
    m_displayedSampleIndex = -1;
    showFrameForRoiEditing();

    const QSignalBlocker blockDraw(ui->regionDrawButton);
    const QSignalBlocker blockRect(ui->regionRectButton);
    const QSignalBlocker blockCircle(ui->regionCircleButton);
    ui->regionDrawButton->setChecked(false);
    ui->regionRectButton->setChecked(false);
    ui->regionCircleButton->setChecked(true);

    if (!m_previewHelper || !m_previewHelper->hasImage())
        return;

    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->clearRoi();
    if (m_circleRoiNormalized.valid)
        m_previewHelper->setCircleRoiNormalized(m_circleRoiNormalized);
    else
        m_previewHelper->clearCircleRoi();
    m_previewHelper->setCircleDrawingEnabled(true);
    const QString text = tr("请框选颜色识别圆形 ROI：按住左键从圆心拖拽半径");
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
    m_detectRegionType = QStringLiteral("rectangle");
    m_circleRoiNormalized = CircleRoi();
    m_displayedSampleIndex = -1;
    syncRegionButtons(true);
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearCircleRoi();
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    const QString text = roiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[ColorRecognitionDialog] ROI normalized:" << m_roiNormalized;
    if (!m_maskEditing)
        rerunLiveTest();  // 测试态下绘制完成立即以新 ROI 重跑检测
}

void ColorRecognitionDialog::handleRoiSelectionRejected()
{
    const QString text = tr("ROI 无效，请拖拽宽高至少 2 像素的矩形");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void ColorRecognitionDialog::handleCircleRoiChanged(const CircleRoi &roi)
{
    if (!roi.valid)
        return;

    m_circleRoiNormalized = roi;
    m_roiNormalized = normalizedRoiOrDefault(roi.boundingRectNormalized);
    m_detectRegionType = QStringLiteral("circle");
    m_globalDetection = false;
    m_displayedSampleIndex = -1;

    const QSignalBlocker blockDraw(ui->regionDrawButton);
    const QSignalBlocker blockRect(ui->regionRectButton);
    const QSignalBlocker blockCircle(ui->regionCircleButton);
    ui->regionDrawButton->setChecked(false);
    ui->regionRectButton->setChecked(false);
    ui->regionCircleButton->setChecked(true);

    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearRoi();
        m_previewHelper->setCircleRoiNormalized(m_circleRoiNormalized);
    }

    const QString text = roiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[ColorRecognitionDialog] Circle ROI center:" << m_circleRoiNormalized.centerNormalized
             << "radius:" << m_circleRoiNormalized.radiusNormalized
             << "bounding:" << m_roiNormalized;
    if (!m_maskEditing)
        rerunLiveTest();  // 测试态下绘制完成立即以新 ROI 重跑检测
}

void ColorRecognitionDialog::handleCircleRoiSelectionRejected()
{
    const QString text = tr("ROI 无效，请从圆心拖拽半径至少 2 像素的圆形");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void ColorRecognitionDialog::startMaskEditing()
{
    stopLiveTestRun();
    m_maskEditing = true;
    m_displayedSampleIndex = -1;
    showFrameForRoiEditing();

    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setCircleDrawingEnabled(false);
        m_previewHelper->clearRoi();
        m_previewHelper->clearCircleRoi();
        m_previewHelper->clearToolOverlays();
        if (m_maskPolygonNormalized.size() >= 3)
            m_previewHelper->setPolygonRoiNormalized(m_maskPolygonNormalized);
        else
            m_previewHelper->clearPolygonRoi();
    }

    syncMaskControls();
    const QString text = tr("屏蔽区域编辑：点击多边形工具后左键添加角点，双击完成");
    setViewerStatusText(text, text);
}

void ColorRecognitionDialog::startMaskPolygonDrawing()
{
    if (!m_maskEditing)
        startMaskEditing();

    if (!m_previewHelper || !m_previewHelper->hasImage()) {
        const QString text = tr("当前无图像，无法绘制屏蔽区域");
        setViewerStatusText(text, text);
        syncMaskControls();
        return;
    }

    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->setPolygonDrawingEnabled(true);
    if (m_maskPolygonButton)
        m_maskPolygonButton->setChecked(true);
    const QString text = tr("绘制屏蔽多边形：左键添加角点，双击完成；完成后可拖动整体或拖动顶点");
    setViewerStatusText(text, text);
}

void ColorRecognitionDialog::finishMaskEditing()
{
    if (m_previewHelper) {
        if (m_previewHelper->isPolygonDrawingEnabled())
            m_previewHelper->finishPolygonDrawing();
        m_previewHelper->setPolygonDrawingEnabled(false);
    }

    m_maskEditing = false;
    syncMaskControls();
    refreshDisplayedRoiOverlay();
    const QString text = m_maskPolygonNormalized.size() >= 3
            ? tr("屏蔽区域已设置：%1 个角点").arg(m_maskPolygonNormalized.size())
            : tr("屏蔽区域未设置");
    setViewerStatusText(text, text);
}

void ColorRecognitionDialog::handleMaskPolygonChanged(const QVector<QPointF> &points)
{
    if (!m_maskEditing)
        return;

    m_maskPolygonNormalized = points.size() >= 3 ? points : QVector<QPointF>();
    syncMaskControls();
    const QString text = m_maskPolygonNormalized.size() >= 3
            ? tr("屏蔽多边形：%1 个角点，可拖动整体或顶点微调").arg(m_maskPolygonNormalized.size())
            : tr("屏蔽多边形点数不足");
    setViewerStatusText(text, text);
}

void ColorRecognitionDialog::handleMaskPolygonSelectionRejected(int pointCount)
{
    if (!m_maskEditing)
        return;

    const QString text = tr("屏蔽多边形无效：至少需要 3 个角点，当前 %1 个").arg(pointCount);
    setViewerStatusText(text, text);
    syncMaskControls();
}

void ColorRecognitionDialog::syncMaskControls()
{
    if (m_maskEditButton)
        m_maskEditButton->setVisible(!m_maskEditing);
    if (m_maskPolygonButton) {
        m_maskPolygonButton->setVisible(m_maskEditing);
        m_maskPolygonButton->setChecked(m_previewHelper && m_previewHelper->isPolygonDrawingEnabled());
    }
    if (m_maskFinishButton)
        m_maskFinishButton->setVisible(m_maskEditing);
}

void ColorRecognitionDialog::refreshPositionCorrectionControls()
{
    if (!ui || !ui->positionCorrectionSwitch || !ui->positionCorrectionSourceRow)
        return;

    ui->positionCorrectionSourceRow->setVisible(ui->positionCorrectionSwitch->isChecked());
}

void ColorRecognitionDialog::refreshDisplayedRoiOverlay()
{
    if (!m_previewHelper)
        return;

    m_previewHelper->clearRoi();
    m_previewHelper->clearCircleRoi();
    if (m_maskEditing && m_maskPolygonNormalized.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(m_maskPolygonNormalized);
    else
        m_previewHelper->clearPolygonRoi();
    if (m_displayedSampleIndex >= 0) {
        if (const ColorRecognitionTemplateData *colorTemplate = activeTemplate()) {
            if (m_displayedSampleIndex < colorTemplate->samples.size()) {
                m_previewHelper->setRoiRectNormalized(
                            normalizedRoiOrDefault(colorTemplate->samples.at(m_displayedSampleIndex).roiNormalized));
            }
        }
        return;
    }

    if (!m_globalDetection && !m_maskEditing) {
        if (m_detectRegionType == QStringLiteral("circle") && m_circleRoiNormalized.valid)
            m_previewHelper->setCircleRoiNormalized(m_circleRoiNormalized);
        else
            m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
}

void ColorRecognitionDialog::displayResult(const ToolResult &result, bool referenceSource)
{
    if (m_previewHelper) {
        // 任何实时测试态（基准图 / 相机连续 / 单次）都保持检测区域可绘制，以便"绘制即重测"。
        const bool keepDetectRoiEditable =
                m_liveTestSource != LiveTestSource::None && !m_globalDetection &&
                !m_maskEditing && m_displayedSampleIndex < 0;
        const bool keepCircleEditable =
                keepDetectRoiEditable && m_detectRegionType == QStringLiteral("circle");
        m_previewHelper->setRoiDrawingEnabled(keepDetectRoiEditable && !keepCircleEditable);
        m_previewHelper->setCircleDrawingEnabled(keepCircleEditable);
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setToolOverlays(keepDetectRoiEditable
                                         ? colorRecognitionPreviewOverlaysWithoutRoi(result.overlays)
                                         : result.overlays);
        if (keepCircleEditable && m_circleRoiNormalized.valid)
            m_previewHelper->setCircleRoiNormalized(m_circleRoiNormalized);
        else if (keepDetectRoiEditable)
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

    if (m_detectRegionType == QStringLiteral("circle") && m_circleRoiNormalized.valid) {
        return tr("圆形检测 ROI cx=%1 cy=%2 r=%3")
                .arg(m_circleRoiNormalized.centerNormalized.x(), 0, 'f', 3)
                .arg(m_circleRoiNormalized.centerNormalized.y(), 0, 'f', 3)
                .arg(m_circleRoiNormalized.radiusNormalized, 0, 'f', 3);
    }

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
