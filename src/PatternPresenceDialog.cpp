#include "PatternPresenceDialog.h"
#include "ui_PatternPresenceDialog.h"

#include "PlanDialogUtils.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDebug>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QtGlobal>
#include <QUuid>

#include <exception>

#include <opencv2/imgproc.hpp>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolRequest.h"

namespace {

QImage imageFromFrame(const cv::Mat &frame)
{
    return MatImageConverter::matToDisplayImage(frame, QStringLiteral("PatternPresenceDialog"));
}

int labelDisplayWidth(const QLabel *label)
{
    if (!label)
        return 640;

    const int width = label->contentsRect().width();
    return width > 80 ? width : 640;
}

QString boolDisplayText(const bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString shapeTypeFromButtons(const bool rectChecked)
{
    return rectChecked ? QStringLiteral("rectangle") : QStringLiteral("polygon");
}

QString detectRegionTypeFromButtons(const bool drawChecked,
                                    const bool rectChecked,
                                    const bool circleChecked)
{
    Q_UNUSED(drawChecked)
    Q_UNUSED(rectChecked)
    if (circleChecked)
        return QStringLiteral("circle");
    return QStringLiteral("rectangle");
}

QString sensitivityModeFromButtons(const bool manualChecked)
{
    return manualChecked ? QStringLiteral("manual") : QStringLiteral("auto");
}

QString judgeModeFromResultBasis(const QString &resultBasis)
{
    if (resultBasis == QObject::tr("最低得分"))
        return QStringLiteral("score");
    if (resultBasis == QObject::tr("结果有无"))
        return QStringLiteral("presence");
    return QStringLiteral("unknown");
}

QString makePresenceStatusTooltipText(const ToolResult &result)
{
    return QStringLiteral("status: %1\nmessage: %2\nscore: %3\ncount: %4\nok: %5")
            .arg(result.status,
                 result.message,
                 QString::number(result.score, 'f', 6),
                 QString::number(result.count),
                 boolDisplayText(result.ok));
}

bool shapeModelContoursTooSparse(const ToolResult &result)
{
    return result.payload.value(QStringLiteral("showContourPointsRequested")).toBool(false) &&
           !result.payload.value(QStringLiteral("showContourPointsApplied")).toBool(false) &&
           result.payload.value(QStringLiteral("contourSource")).toString() ==
           QStringLiteral("shape_model_contours_too_sparse");
}

QString makePresenceErrorTooltipText(const QString &status, const QString &message)
{
    return QStringLiteral("status: %1\nmessage: %2\nscore: 0.000000\ncount: 0\nok: false")
            .arg(status, message);
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

    const QRectF rect(json.value(QStringLiteral("x")).toDouble(fallback.x()),
                      json.value(QStringLiteral("y")).toDouble(fallback.y()),
                      json.value(QStringLiteral("width")).toDouble(fallback.width()),
                      json.value(QStringLiteral("height")).toDouble(fallback.height()));
    return rect.width() > 0.0 && rect.height() > 0.0 ? rect : fallback;
}

QJsonObject pointToJson(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
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
        const QJsonObject json = value.toObject();
        points.append(QPointF(json.value(QStringLiteral("x")).toDouble(),
                              json.value(QStringLiteral("y")).toDouble()));
    }
    return points;
}

QRectF boundingRectForPoints(const QVector<QPointF> &points)
{
    if (points.isEmpty())
        return QRectF();

    double left = points.first().x();
    double top = points.first().y();
    double right = left;
    double bottom = top;
    for (const QPointF &point : points) {
        left = qMin(left, point.x());
        top = qMin(top, point.y());
        right = qMax(right, point.x());
        bottom = qMax(bottom, point.y());
    }

    const QRectF rect(QPointF(qBound(0.0, left, 1.0), qBound(0.0, top, 1.0)),
                      QPointF(qBound(0.0, right, 1.0), qBound(0.0, bottom, 1.0)));
    return rect.normalized();
}

void setComboBoxValue(QComboBox *comboBox, const QString &value)
{
    if (!comboBox || value.isEmpty())
        return;

    const int index = comboBox->findText(value);
    if (index >= 0)
        comboBox->setCurrentIndex(index);
}

void configureRoiToolButton(QToolButton *button,
                            const QString &text,
                            const QString &tooltip)
{
    if (!button)
        return;

    if (!text.isNull())
        button->setText(text);
    button->setToolTip(tooltip);
    button->setMinimumSize(QSize(52, 38));
    button->setMaximumSize(QSize(52, 38));
    button->setProperty("actionRole", QStringLiteral("toolbarIcon"));
}

} // namespace

PatternPresenceDialog::PatternPresenceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PatternPresenceDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_basicTemplateRegionGroup(new QButtonGroup(this))
    , m_templateRegionGroup(new QButtonGroup(this))
    , m_basicDetectionRegionGroup(new QButtonGroup(this))
    , m_detectionRegionGroup(new QButtonGroup(this))
    , m_templateSensitivityGroup(new QButtonGroup(this))
    , m_basicResultPresenceGroup(new QButtonGroup(this))
    , m_resultPresenceGroup(new QButtonGroup(this))
    , m_modelCacheKey(QStringLiteral("pattern_presence_model_%1")
                              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    ui->setupUi(this);
    m_toolId = QStringLiteral("pattern_presence_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_testToolEngine.registerAdapter(&m_testPatternPresenceAdapter);
    m_continuousTimer = new QTimer(this);
    m_continuousTimer->setInterval(500);
    connect(m_continuousTimer, &QTimer::timeout, this, &PatternPresenceDialog::runContinuousTick);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    connectControls();
    setUiMode(PresenceUiMode::Edit);
}

PatternPresenceDialog::~PatternPresenceDialog()
{
    delete ui;
}

PatternPresenceConfig PatternPresenceDialog::configuration() const
{
    const bool basicMode = ui->patternParamsStackedWidget->currentWidget() == ui->basicParamsPage;
    const QString resultBasisText = basicMode
            ? ui->basicResultBasisComboBox->currentText()
            : ui->resultBasisComboBox->currentText();
    const int angleMin = ui->minAngleSpinBox->value();
    const int angleMax = ui->maxAngleSpinBox->value();
    const bool templatePolygonMode = basicMode
            ? ui->basicTemplatePolygonButton->isChecked()
            : ui->templatePolygonButton->isChecked();

    PatternPresenceConfig config;
    config.templateRoiNormalized = templatePolygonMode && m_templatePolygonNormalized.size() >= 3
            ? boundingRectForPoints(m_templatePolygonNormalized)
            : m_templateRoiNormalized;
    config.templateSource = QStringLiteral("referenceImage");
    config.modelAutoCreate = true;
    config.modelCacheKey = m_modelCacheKey;
    config.templateShapeType = templatePolygonMode
            ? QStringLiteral("polygon")
            : shapeTypeFromButtons(true);
    config.templatePolygonNormalized = m_templatePolygonNormalized;
    config.templateSensitivityMode = basicMode
            ? QStringLiteral("auto")
            : sensitivityModeFromButtons(ui->templateManualButton->isChecked());
    config.templateSensitivity = basicMode
            ? ui->basicSensitivitySpinBox->value()
            : ui->sensitivitySpinBox->value();
    config.detectRegionType = basicMode
            ? detectRegionTypeFromButtons(ui->basicDetectionDrawButton->isChecked(),
                                          ui->basicDetectionRectButton->isChecked(),
                                          ui->basicDetectionCircleButton->isChecked())
            : detectRegionTypeFromButtons(ui->detectionDrawButton->isChecked(),
                                          ui->detectionRectButton->isChecked(),
                                          ui->detectionCircleButton->isChecked());
    config.enablePositionCorrection = basicMode
            ? ui->basicPositionCorrectionSwitch->isChecked()
            : ui->positionCorrectionSwitch->isChecked();
    config.positionCorrectionSource = basicMode
            ? ui->basicPositionCorrectionComboBox->currentText()
            : ui->positionCorrectionComboBox->currentText();
    config.minScore = ui->minScoreSpinBox->value();
    config.polarity = ui->matchPolarityComboBox->currentText();
    config.scaleMin = ui->minScaleSpinBox->value();
    config.scaleMax = ui->maxScaleSpinBox->value();
    config.angleStart = angleMin;
    config.angleExtent = qMax(0, angleMax - angleMin);
    config.timeoutMs = ui->timeoutSpinBox->value();
    config.showContourPoints = basicMode
            ? ui->basicShowContourSwitch->isChecked()
            : ui->showContourSwitch->isChecked();
    config.sortMode = ui->sortModeComboBox->currentText();
    config.judgeBasis = judgeModeFromResultBasis(resultBasisText);
    config.existOk = basicMode
            ? ui->basicPresentOkButton->isChecked()
            : ui->presentOkButton->isChecked();
    config.scoreThreshold = basicMode
            ? ui->basicResultMinScoreSpinBox->value()
            : ui->resultMinScoreSpinBox->value();
    return config;
}

ToolConfig PatternPresenceDialog::toToolConfig() const
{
    const PatternPresenceConfig presenceConfig = configuration();
    const bool basicMode = ui->patternParamsStackedWidget->currentWidget() == ui->basicParamsPage;
    const QString judgeBasisText = basicMode
            ? ui->basicResultBasisComboBox->currentText()
            : ui->resultBasisComboBox->currentText();

    const QString toolId = m_toolId;

    QJsonObject params;
    params.insert(QStringLiteral("templateRoiNormalized"),
                  rectToJson(presenceConfig.templateRoiNormalized));
    params.insert(QStringLiteral("templateSource"), presenceConfig.templateSource);
    params.insert(QStringLiteral("templateImagePath"), presenceConfig.templateImagePath);
    params.insert(QStringLiteral("modelPath"), presenceConfig.modelPath);
    params.insert(QStringLiteral("modelAutoCreate"), presenceConfig.modelAutoCreate);
    params.insert(QStringLiteral("modelCacheKey"),
                  presenceConfig.modelCacheKey.isEmpty()
                          ? QStringLiteral("%1_shape_model").arg(toolId)
                          : presenceConfig.modelCacheKey);
    params.insert(QStringLiteral("templateShapeType"), presenceConfig.templateShapeType);
    params.insert(QStringLiteral("templatePolygonNormalized"),
                  pointsToJson(presenceConfig.templatePolygonNormalized));
    params.insert(QStringLiteral("templateSensitivityMode"), presenceConfig.templateSensitivityMode);
    params.insert(QStringLiteral("templateSensitivity"), presenceConfig.templateSensitivity);
    params.insert(QStringLiteral("detectRegionType"), presenceConfig.detectRegionType);
    params.insert(QStringLiteral("enablePositionCorrection"), presenceConfig.enablePositionCorrection);
    params.insert(QStringLiteral("positionCorrectionSource"), presenceConfig.positionCorrectionSource);
    params.insert(QStringLiteral("minScore"), presenceConfig.minScore);
    params.insert(QStringLiteral("polarity"), presenceConfig.polarity);
    params.insert(QStringLiteral("scaleMin"), presenceConfig.scaleMin);
    params.insert(QStringLiteral("scaleMax"), presenceConfig.scaleMax);
    params.insert(QStringLiteral("angleStart"), presenceConfig.angleStart);
    params.insert(QStringLiteral("angleExtent"), presenceConfig.angleExtent);
    params.insert(QStringLiteral("angleEnd"), presenceConfig.angleStart + presenceConfig.angleExtent);
    params.insert(QStringLiteral("timeoutMs"), presenceConfig.timeoutMs);
    params.insert(QStringLiteral("showContourPoints"), presenceConfig.showContourPoints);
    params.insert(QStringLiteral("debugPatternPolygonLog"), false);
    params.insert(QStringLiteral("sortMode"), presenceConfig.sortMode);
    params.insert(QStringLiteral("judgeBasis"), presenceConfig.judgeBasis);
    params.insert(QStringLiteral("judgeBasisText"), judgeBasisText);
    params.insert(QStringLiteral("existOk"), presenceConfig.existOk);
    params.insert(QStringLiteral("scoreThreshold"), presenceConfig.scoreThreshold);

    QJsonObject judgeRule;
    judgeRule.insert(QStringLiteral("mode"), presenceConfig.judgeBasis);
    judgeRule.insert(QStringLiteral("existOk"), presenceConfig.existOk);
    judgeRule.insert(QStringLiteral("scoreThreshold"), presenceConfig.scoreThreshold);

    ToolConfig config;
    config.toolId = toolId;
    config.toolName = tr("图案有无");
    config.toolType = ToolType::PatternPresence;
    config.category = ToolCategory::Presence;
    config.enabled = m_enabled;
    config.roiNormalized = m_roiNormalized;
    config.params = params;
    config.judgeRule = judgeRule;
    config.displayName = tr("图案有无");
    config.summary = summaryText();
    return config;
}

ToolConfig PatternPresenceDialog::toolConfig() const
{
    return toToolConfig();
}

ToolPreviewSnapshot PatternPresenceDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void PatternPresenceDialog::loadFromConfig(const ToolConfig &config)
{
    if (!config.toolId.trimmed().isEmpty())
        m_toolId = config.toolId;
    m_enabled = config.enabled;
    if (config.roiNormalized.width() > 0.0 && config.roiNormalized.height() > 0.0)
        m_roiNormalized = config.roiNormalized;

    const QJsonObject params = config.params;
    m_templateRoiNormalized = rectFromJson(params.value(QStringLiteral("templateRoiNormalized")).toObject(),
                                           m_templateRoiNormalized);
    m_templatePolygonNormalized = pointsFromJson(params.value(QStringLiteral("templatePolygonNormalized")).toArray());
    if (m_templatePolygonNormalized.size() >= 3)
        m_templateRoiNormalized = boundingRectForPoints(m_templatePolygonNormalized);
    const QString modelCacheKey = params.value(QStringLiteral("modelCacheKey")).toString();
    if (!modelCacheKey.trimmed().isEmpty())
        m_modelCacheKey = modelCacheKey;

    const bool allMode = params.value(QStringLiteral("templateSensitivityMode")).toString() == QStringLiteral("manual")
            || params.contains(QStringLiteral("scaleMin"))
            || params.contains(QStringLiteral("timeoutMs"));
    ui->basicSegmentButton->setChecked(!allMode);
    ui->allSegmentButton->setChecked(allMode);
    ui->patternParamsStackedWidget->setCurrentWidget(allMode ? ui->allParamsPage : ui->basicParamsPage);

    const bool positionCorrection = params.value(QStringLiteral("enablePositionCorrection")).toBool(ui->basicPositionCorrectionSwitch->isChecked());
    ui->basicPositionCorrectionSwitch->setChecked(positionCorrection);
    ui->positionCorrectionSwitch->setChecked(positionCorrection);
    setComboBoxValue(ui->basicPositionCorrectionComboBox, params.value(QStringLiteral("positionCorrectionSource")).toString());
    setComboBoxValue(ui->positionCorrectionComboBox, params.value(QStringLiteral("positionCorrectionSource")).toString());
    const int sensitivity = params.value(QStringLiteral("templateSensitivity")).toInt(ui->basicSensitivitySpinBox->value());
    ui->basicSensitivitySpinBox->setValue(sensitivity);
    ui->sensitivitySpinBox->setValue(sensitivity);
    ui->templateManualButton->setChecked(params.value(QStringLiteral("templateSensitivityMode")).toString() == QStringLiteral("manual"));
    ui->templateAutoButton->setChecked(!ui->templateManualButton->isChecked());
    const bool polygonTemplate = params.value(QStringLiteral("templateShapeType")).toString()
            == QStringLiteral("polygon") && m_templatePolygonNormalized.size() >= 3;
    ui->basicTemplateRectButton->setChecked(!polygonTemplate);
    ui->templateRectButton->setChecked(!polygonTemplate);
    ui->basicTemplatePolygonButton->setChecked(polygonTemplate);
    ui->templatePolygonButton->setChecked(polygonTemplate);
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    ui->minScoreSpinBox->setValue(params.value(QStringLiteral("minScore")).toInt(ui->minScoreSpinBox->value()));
    setComboBoxValue(ui->matchPolarityComboBox, params.value(QStringLiteral("polarity")).toString());
    ui->minScaleSpinBox->setValue(params.value(QStringLiteral("scaleMin")).toInt(ui->minScaleSpinBox->value()));
    ui->maxScaleSpinBox->setValue(params.value(QStringLiteral("scaleMax")).toInt(ui->maxScaleSpinBox->value()));
    const int angleStart = params.value(QStringLiteral("angleStart")).toInt(ui->minAngleSpinBox->value());
    const int angleEnd = params.contains(QStringLiteral("angleEnd"))
            ? params.value(QStringLiteral("angleEnd")).toInt()
            : angleStart + params.value(QStringLiteral("angleExtent")).toInt(ui->maxAngleSpinBox->value() - angleStart);
    ui->minAngleSpinBox->setValue(angleStart);
    ui->maxAngleSpinBox->setValue(angleEnd);
    ui->timeoutSpinBox->setValue(params.value(QStringLiteral("timeoutMs")).toInt(ui->timeoutSpinBox->value()));
    const bool showContour = params.value(QStringLiteral("showContourPoints")).toBool(ui->basicShowContourSwitch->isChecked());
    ui->basicShowContourSwitch->setChecked(showContour);
    ui->showContourSwitch->setChecked(showContour);
    setComboBoxValue(ui->sortModeComboBox, params.value(QStringLiteral("sortMode")).toString());

    const QString judgeBasis = params.value(QStringLiteral("judgeBasis")).toString();
    const int resultIndex = judgeBasis == QStringLiteral("score") ? 1 : 0;
    ui->basicResultBasisComboBox->setCurrentIndex(resultIndex);
    ui->resultBasisComboBox->setCurrentIndex(resultIndex);
    ui->basicResultBasisStackedWidget->setCurrentIndex(ui->basicResultBasisComboBox->currentIndex());
    ui->resultBasisStackedWidget->setCurrentIndex(ui->resultBasisComboBox->currentIndex());
    const bool existOk = params.value(QStringLiteral("existOk")).toBool(true);
    ui->basicPresentOkButton->setChecked(existOk);
    ui->basicAbsentOkButton->setChecked(!existOk);
    ui->presentOkButton->setChecked(existOk);
    ui->absentOkButton->setChecked(!existOk);
    const int scoreThreshold = params.value(QStringLiteral("scoreThreshold")).toInt(ui->basicResultMinScoreSpinBox->value());
    ui->basicResultMinScoreSpinBox->setValue(scoreThreshold);
    ui->resultMinScoreSpinBox->setValue(scoreThreshold);

    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    m_roiEditTarget = RoiEditTarget::DetectRoi;
    if (m_previewHelper)
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    const QString roiText = detectRoiStatusText();
    setViewerStatusText(roiText, roiText);
}

QString PatternPresenceDialog::summaryText() const
{
    const PatternPresenceConfig config = configuration();
    const QString judgeText = config.judgeBasis == QStringLiteral("score")
            ? tr("最低得分")
            : tr("结果有无");

    if (config.judgeBasis == QStringLiteral("score")) {
        return tr("模板: %1, 检测区: %2, 判断: %3 >= %4")
            .arg(config.templateShapeType,
                 config.detectRegionType,
                 judgeText,
                 QString::number(config.scoreThreshold));
    }

    return tr("模板: %1, 检测区: %2, 判断: %3, %4")
        .arg(config.templateShapeType,
             config.detectRegionType,
             judgeText,
             config.existOk ? tr("存在OK") : tr("不存在OK"));
}

void PatternPresenceDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void PatternPresenceDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 图案有无"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    applyAdaptiveWindowSize();

    m_exitTestButton = new QPushButton(tr("退出测试"), this);
    m_exitTestButton->setObjectName(QStringLiteral("exitTestButton"));
    m_exitTestButton->setMinimumSize(120, 48);
    m_exitTestButton->setProperty("actionRole", QStringLiteral("secondary"));
    ui->horizontalLayout_actions->addWidget(m_exitTestButton);

    ui->patternParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
    ui->basicSegmentButton->setChecked(true);
    ui->allSegmentButton->setChecked(false);
    ui->basicResultBasisStackedWidget->setCurrentIndex(ui->basicResultBasisComboBox->currentIndex());
    ui->resultBasisStackedWidget->setCurrentIndex(ui->resultBasisComboBox->currentIndex());
    ui->matchPolarityComboBox->setCurrentIndex(1);
    ui->sortModeComboBox->setCurrentIndex(4);
    configureRoiToolButton(ui->basicTemplateRectButton, QStringLiteral("□"), tr("模板矩形 ROI"));
    configureRoiToolButton(ui->templateRectButton, QStringLiteral("□"), tr("模板矩形 ROI"));
    configureRoiToolButton(ui->basicTemplatePolygonButton, QStringLiteral("⬡"), tr("模板多边形 ROI"));
    configureRoiToolButton(ui->templatePolygonButton, QStringLiteral("⬡"), tr("模板多边形 ROI"));
    configureRoiToolButton(ui->basicDetectionDrawButton, QStringLiteral("✎"), tr("自由绘制检测 ROI 暂未实现"));
    configureRoiToolButton(ui->detectionDrawButton, QStringLiteral("✎"), tr("自由绘制检测 ROI 暂未实现"));
    configureRoiToolButton(ui->basicDetectionRectButton, QStringLiteral("□"), tr("矩形检测 ROI"));
    configureRoiToolButton(ui->detectionRectButton, QStringLiteral("□"), tr("矩形检测 ROI"));
    configureRoiToolButton(ui->basicDetectionCircleButton, QStringLiteral("○"), tr("圆形检测 ROI 暂未实现"));
    configureRoiToolButton(ui->detectionCircleButton, QStringLiteral("○"), tr("圆形检测 ROI 暂未实现"));

    ui->viewerStatusBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ui->viewerStatusBar->setMinimumHeight(42);
    ui->viewerStatusBar->setMaximumHeight(42);
    ui->viewerStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    ui->viewerStatusLabel->setMinimumWidth(0);
    ui->viewerStatusLabel->setMaximumHeight(42);
    ui->viewerStatusLabel->setWordWrap(false);
    ui->viewerStatusLabel->setTextFormat(Qt::PlainText);
    ui->viewerStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->horizontalLayout_viewerStatus->setStretch(0, 1);
    ui->horizontalLayout_viewerStatus->setStretch(1, 0);
    ui->horizontalLayout_viewerStatus->setStretch(2, 0);
    const QString templateStatus = templateRoiStatusText();
    setViewerStatusText(templateStatus, templateStatus);
}

void PatternPresenceDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &PatternPresenceDialog::reject);
    connect(ui->referenceTestButton, &QPushButton::clicked, this, &PatternPresenceDialog::runReferenceTest);
    connect(ui->testRunButton, &QPushButton::clicked, this, &PatternPresenceDialog::handleTestRunButton);
    connect(ui->finishButton, &QPushButton::clicked, this, &PatternPresenceDialog::handleFinishButton);
    connect(m_exitTestButton, &QPushButton::clicked, this, &PatternPresenceDialog::exitTestMode);

    connect(ui->basicResultBasisComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            ui->basicResultBasisStackedWidget,
            &QStackedWidget::setCurrentIndex);
    connect(ui->resultBasisComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            ui->resultBasisStackedWidget,
            &QStackedWidget::setCurrentIndex);

    m_segmentGroup->setExclusive(true);
    m_segmentGroup->addButton(ui->basicSegmentButton, 0);
    m_segmentGroup->addButton(ui->allSegmentButton, 1);
    connect(ui->basicSegmentButton, &QPushButton::clicked, this, [this]() {
        ui->patternParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
    });
    connect(ui->allSegmentButton, &QPushButton::clicked, this, [this]() {
        ui->patternParamsStackedWidget->setCurrentWidget(ui->allParamsPage);
    });

    m_basicTemplateRegionGroup->setExclusive(true);
    m_basicTemplateRegionGroup->addButton(ui->basicTemplateRectButton, 0);
    m_basicTemplateRegionGroup->addButton(ui->basicTemplatePolygonButton, 1);
    m_templateRegionGroup->setExclusive(true);
    m_templateRegionGroup->addButton(ui->templateRectButton, 0);
    m_templateRegionGroup->addButton(ui->templatePolygonButton, 1);

    connect(ui->basicTemplateRectButton, &QToolButton::clicked, this, &PatternPresenceDialog::startTemplateRoiEditing);
    connect(ui->templateRectButton, &QToolButton::clicked, this, &PatternPresenceDialog::startTemplateRoiEditing);
    connect(ui->basicTemplateFinishButton, &QPushButton::clicked, this, &PatternPresenceDialog::finishTemplateRoiEditing);
    connect(ui->templateFinishButton, &QPushButton::clicked, this, &PatternPresenceDialog::finishTemplateRoiEditing);
    connect(ui->basicTemplatePolygonButton, &QToolButton::clicked, this, &PatternPresenceDialog::startTemplatePolygonEditing);
    connect(ui->templatePolygonButton, &QToolButton::clicked, this, &PatternPresenceDialog::startTemplatePolygonEditing);

    m_basicDetectionRegionGroup->setExclusive(true);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionDrawButton, 0);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionRectButton, 1);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionCircleButton, 2);
    m_detectionRegionGroup->setExclusive(true);
    m_detectionRegionGroup->addButton(ui->detectionDrawButton, 0);
    m_detectionRegionGroup->addButton(ui->detectionRectButton, 1);
    m_detectionRegionGroup->addButton(ui->detectionCircleButton, 2);

    connect(ui->basicDetectionDrawButton, &QToolButton::clicked, this, [this]() {
        ui->basicDetectionRectButton->setChecked(true);
        ui->detectionRectButton->setChecked(true);
        m_editingTemplateRoi = false;
        m_roiEditTarget = RoiEditTarget::DetectRoi;
        if (m_previewHelper) {
            m_previewHelper->setRoiDrawingEnabled(false);
            m_previewHelper->setPolygonDrawingEnabled(false);
            m_previewHelper->setRoiRectNormalized(m_roiNormalized);
        }
        const QString text = tr("自由绘制检测 ROI 暂未实现，请使用矩形检测区域");
        setViewerStatusText(text, text);
    });
    connect(ui->basicDetectionRectButton, &QToolButton::clicked, this, [this]() {
        startDetectRoiEditing(tr("绘制矩形检测区域"));
    });
    connect(ui->basicDetectionCircleButton, &QToolButton::clicked, this, [this]() {
        ui->basicDetectionRectButton->setChecked(true);
        m_editingTemplateRoi = false;
        m_roiEditTarget = RoiEditTarget::DetectRoi;
        if (m_previewHelper) {
            m_previewHelper->setRoiDrawingEnabled(false);
            m_previewHelper->setRoiRectNormalized(m_roiNormalized);
        }
        const QString text = tr("圆形 ROI 暂未实现，请使用矩形检测区域");
        setViewerStatusText(text, text);
    });
    connect(ui->detectionDrawButton, &QToolButton::clicked, this, [this]() {
        ui->basicDetectionRectButton->setChecked(true);
        ui->detectionRectButton->setChecked(true);
        m_editingTemplateRoi = false;
        m_roiEditTarget = RoiEditTarget::DetectRoi;
        if (m_previewHelper) {
            m_previewHelper->setRoiDrawingEnabled(false);
            m_previewHelper->setPolygonDrawingEnabled(false);
            m_previewHelper->setRoiRectNormalized(m_roiNormalized);
        }
        const QString text = tr("自由绘制检测 ROI 暂未实现，请使用矩形检测区域");
        setViewerStatusText(text, text);
    });
    connect(ui->detectionRectButton, &QToolButton::clicked, this, [this]() {
        startDetectRoiEditing(tr("绘制矩形检测区域"));
    });
    connect(ui->detectionCircleButton, &QToolButton::clicked, this, [this]() {
        ui->detectionRectButton->setChecked(true);
        m_editingTemplateRoi = false;
        m_roiEditTarget = RoiEditTarget::DetectRoi;
        if (m_previewHelper) {
            m_previewHelper->setRoiDrawingEnabled(false);
            m_previewHelper->setRoiRectNormalized(m_roiNormalized);
        }
        const QString text = tr("圆形 ROI 暂未实现，请使用矩形检测区域");
        setViewerStatusText(text, text);
    });

    m_templateSensitivityGroup->setExclusive(true);
    m_templateSensitivityGroup->addButton(ui->templateManualButton, 0);
    m_templateSensitivityGroup->addButton(ui->templateAutoButton, 1);

    m_basicResultPresenceGroup->setExclusive(true);
    m_basicResultPresenceGroup->addButton(ui->basicPresentOkButton, 0);
    m_basicResultPresenceGroup->addButton(ui->basicAbsentOkButton, 1);
    m_resultPresenceGroup->setExclusive(true);
    m_resultPresenceGroup->addButton(ui->presentOkButton, 0);
    m_resultPresenceGroup->addButton(ui->absentOkButton, 1);

    connect(ui->minScaleSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                if (value > ui->maxScaleSpinBox->value())
                    ui->maxScaleSpinBox->setValue(value);
            });
    connect(ui->maxScaleSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                if (value < ui->minScaleSpinBox->value())
                    ui->minScaleSpinBox->setValue(value);
            });
    connect(ui->minAngleSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                if (value > ui->maxAngleSpinBox->value())
                    ui->maxAngleSpinBox->setValue(value);
            });
    connect(ui->maxAngleSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                if (value < ui->minAngleSpinBox->value())
                    ui->minAngleSpinBox->setValue(value);
            });

    if (m_previewHelper) {
        connect(m_previewHelper,
                &FrameViewHelper::roiChanged,
                this,
                &PatternPresenceDialog::handleRoiChanged);
        connect(m_previewHelper,
                &FrameViewHelper::roiSelectionRejected,
                this,
                [this](const QRectF &) {
                    handleRoiSelectionRejected();
                });
        connect(m_previewHelper,
                &FrameViewHelper::polygonChanged,
                this,
                &PatternPresenceDialog::handleTemplatePolygonChanged);
        connect(m_previewHelper,
                &FrameViewHelper::polygonSelectionRejected,
                this,
                &PatternPresenceDialog::handlePolygonSelectionRejected);
    }

    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) {
                if (m_uiMode == PresenceUiMode::Edit || m_uiMode == PresenceUiMode::TestReady)
                    showReferenceImage();
            });
}

void PatternPresenceDialog::finishConfiguration()
{
    if (isTemplatePolygonMode() &&
        m_roiEditTarget == RoiEditTarget::TemplateRoi &&
        m_previewHelper &&
        m_previewHelper->isPolygonDrawingEnabled() &&
        !m_previewHelper->finishPolygonDrawing()) {
        handlePolygonSelectionRejected(m_templatePolygonNormalized.size());
        return;
    }

    if (isTemplatePolygonMode() && m_templatePolygonNormalized.size() < 3) {
        handlePolygonSelectionRejected(m_templatePolygonNormalized.size());
        return;
    }

    accept();
}

void PatternPresenceDialog::showProviderImage(const QImage &image)
{
    showLiveImage(image);
}

void PatternPresenceDialog::handleTestRunButton()
{
    if (m_uiMode == PresenceUiMode::Edit) {
        if (!ensureTemplatePolygonReadyForTest())
            return;
        enterTestMode();
        return;
    }

    if (!ensureTemplatePolygonReadyForTest())
        return;
    startContinuousRun();
}

void PatternPresenceDialog::handleFinishButton()
{
    if (m_uiMode == PresenceUiMode::Edit) {
        finishConfiguration();
        return;
    }

    runOnceInTestMode();
}

void PatternPresenceDialog::enterTestMode()
{
    setUiMode(PresenceUiMode::TestReady);
}

void PatternPresenceDialog::exitTestMode()
{
    stopContinuousRun();
    setUiMode(PresenceUiMode::Edit);
}

void PatternPresenceDialog::runOnceInTestMode()
{
    stopContinuousRun();
    setUiMode(PresenceUiMode::SingleShot);
}

void PatternPresenceDialog::applyAdaptiveWindowSize()
{
    PlanDialogUtils::applyLargeWindow(this);
}

void PatternPresenceDialog::setUiMode(PresenceUiMode mode)
{
    if (m_frameUpdatedConnection) {
        QObject::disconnect(m_frameUpdatedConnection);
        m_frameUpdatedConnection = QMetaObject::Connection();
    }

    m_uiMode = mode;
    if (m_uiMode != PresenceUiMode::Edit) {
        m_editingTemplateRoi = false;
        m_roiEditTarget = RoiEditTarget::None;
        if (m_previewHelper) {
            m_previewHelper->setRoiDrawingEnabled(false);
            m_previewHelper->setPolygonDrawingEnabled(false);
        }
    }

    switch (m_uiMode) {
    case PresenceUiMode::Edit:
        if (m_previewHelper)
            m_previewHelper->clearToolOverlays();
        showReferenceImage();
        break;
    case PresenceUiMode::TestReady:
        showReferenceImage();
        break;
    case PresenceUiMode::Continuous:
        m_frameUpdatedConnection = connect(&CameraFrameProvider::instance(),
                                           &CameraFrameProvider::frameUpdated,
                                           this,
                                           &PatternPresenceDialog::showProviderImage);
        showLiveImage(CameraFrameProvider::instance().currentImage());
        break;
    case PresenceUiMode::SingleShot:
        showSingleShotImage();
        break;
    }

    updateBottomButtons();
}

void PatternPresenceDialog::startTemplateRoiEditing()
{
    if (!m_previewHelper)
        return;

    if (m_uiMode != PresenceUiMode::Edit)
        setUiMode(PresenceUiMode::Edit);

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->setRoiDrawingEnabled(false);
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        const QString text = tr("请先设置基准图后再选择模板区域");
        setViewerStatusText(text, text);
        return;
    }

    m_roiEditTarget = RoiEditTarget::TemplateRoi;
    m_editingTemplateRoi = true;
    ui->basicTemplateRectButton->setChecked(true);
    ui->templateRectButton->setChecked(true);
    ui->basicTemplatePolygonButton->setChecked(false);
    ui->templatePolygonButton->setChecked(false);
    ui->viewerTitleLabel->setText(tr("模板区域"));
    showReferenceImage();
    ui->viewerTitleLabel->setText(tr("模板区域"));
    m_previewHelper->clearToolOverlays();
    m_previewHelper->clearPolygonRoi();
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->setRoiRectNormalized(m_templateRoiNormalized);
    m_previewHelper->setRoiDrawingEnabled(true);

    const QString text = tr("请在右侧基准图上拖拽矩形选择模板区域");
    setViewerStatusText(text, text);
}

void PatternPresenceDialog::startTemplatePolygonEditing()
{
    if (!m_previewHelper)
        return;

    if (m_uiMode != PresenceUiMode::Edit)
        setUiMode(PresenceUiMode::Edit);

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->setPolygonDrawingEnabled(false);
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        const QString text = tr("请先设置基准图后再选择模板区域");
        setViewerStatusText(text, text);
        return;
    }

    m_roiEditTarget = RoiEditTarget::TemplateRoi;
    m_editingTemplateRoi = true;
    ui->basicTemplateRectButton->setChecked(false);
    ui->templateRectButton->setChecked(false);
    ui->basicTemplatePolygonButton->setChecked(true);
    ui->templatePolygonButton->setChecked(true);
    ui->viewerTitleLabel->setText(tr("模板多边形区域"));
    showReferenceImage();
    ui->viewerTitleLabel->setText(tr("模板多边形区域"));
    m_previewHelper->clearToolOverlays();
    m_previewHelper->clearRoi();
    if (m_templatePolygonNormalized.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(m_templatePolygonNormalized);
    else
        m_previewHelper->clearPolygonRoi();
    m_previewHelper->setPolygonDrawingEnabled(true);

    const QString text = tr("当前编辑：多边形 ROI。左键添加点，靠近首点点击自动闭合，右键撤销，Esc 取消。");
    setViewerStatusText(text, text);
}

void PatternPresenceDialog::finishTemplateRoiEditing()
{
    if (isTemplatePolygonMode()) {
        if (m_previewHelper && !m_previewHelper->finishPolygonDrawing()) {
            handlePolygonSelectionRejected(m_templatePolygonNormalized.size());
            return;
        }

        if (m_templatePolygonNormalized.size() < 3) {
            handlePolygonSelectionRejected(m_templatePolygonNormalized.size());
            return;
        }

        m_templateRoiNormalized = boundingRectForPoints(m_templatePolygonNormalized);
        m_editingTemplateRoi = false;
        m_roiEditTarget = RoiEditTarget::TemplateRoi;
        if (m_previewHelper) {
            if (m_previewHelper->isPolygonDrawingEnabled())
                m_previewHelper->setPolygonDrawingEnabled(false);
            m_previewHelper->setRoiDrawingEnabled(false);
            showReferenceImage();
            ui->viewerTitleLabel->setText(tr("模板多边形区域"));
        }

        const QString text = templateRoiStatusText();
        setViewerStatusText(text, text);
        return;
    }

    m_editingTemplateRoi = false;
    m_roiEditTarget = RoiEditTarget::TemplateRoi;
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setPolygonDrawingEnabled(false);
        showReferenceImage();
        ui->viewerTitleLabel->setText(tr("模板区域"));
    }

    const QString text = templateRoiStatusText();
    setViewerStatusText(text, text);
}

void PatternPresenceDialog::startDetectRoiEditing(const QString &title)
{
    if (!m_previewHelper)
        return;

    if (m_uiMode != PresenceUiMode::Edit)
        setUiMode(PresenceUiMode::Edit);

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->setRoiDrawingEnabled(false);
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        const QString text = tr("请先设置基准图后再选择检测区域");
        setViewerStatusText(text, text);
        return;
    }

    m_editingTemplateRoi = false;
    m_roiEditTarget = RoiEditTarget::DetectRoi;
    ui->viewerTitleLabel->setText(title);
    showReferenceImage();
    ui->viewerTitleLabel->setText(title);
    m_previewHelper->clearToolOverlays();
    m_previewHelper->clearPolygonRoi();
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    m_previewHelper->setRoiDrawingEnabled(true);

    const QString text = tr("请在右侧基准图上拖拽矩形选择检测区域");
    setViewerStatusText(text, text);
}

void PatternPresenceDialog::showTemplateRoiTodo(const QString &message)
{
    m_editingTemplateRoi = false;
    m_roiEditTarget = RoiEditTarget::TemplateRoi;
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setPolygonDrawingEnabled(false);
        showReferenceImage();
        ui->viewerTitleLabel->setText(tr("模板区域"));
    }
    setViewerStatusText(message, message);
}

void PatternPresenceDialog::handleRoiChanged(const QRectF &roi)
{
    if (!m_previewHelper)
        return;

    if (m_roiEditTarget == RoiEditTarget::TemplateRoi) {
        m_templateRoiNormalized = roi;
        m_templatePolygonNormalized.clear();
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->setRoiRectNormalized(m_templateRoiNormalized);
        const QString text = templateRoiStatusText();
        setViewerStatusText(text, text);
        qDebug() << "[PatternPresenceDialog] Template ROI normalized:" << m_templateRoiNormalized;
        return;
    }

    m_roiNormalized = roi;
    m_previewHelper->clearToolOverlays();
    m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    const QString roiText = detectRoiStatusText();
    setViewerStatusText(roiText, roiText);
    qDebug() << "[PatternPresenceDialog] ROI normalized:" << m_roiNormalized;
}

void PatternPresenceDialog::handleTemplatePolygonChanged(const QVector<QPointF> &points)
{
    if (points.size() < 3)
        return;

    m_templatePolygonNormalized = points;
    m_templateRoiNormalized = boundingRectForPoints(m_templatePolygonNormalized);
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearRoi();
        m_previewHelper->setPolygonRoiNormalized(m_templatePolygonNormalized);
    }

    const QString text = templateRoiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[PatternPresenceDialog] Template polygon normalized points:" << m_templatePolygonNormalized.size()
             << "bounding:" << m_templateRoiNormalized;
}

void PatternPresenceDialog::handlePolygonSelectionRejected(int pointCount)
{
    Q_UNUSED(pointCount)
    const QString text = tr("多边形至少需要 3 个点");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void PatternPresenceDialog::handleRoiSelectionRejected()
{
    const QString text = m_roiEditTarget == RoiEditTarget::TemplateRoi
            ? tr("模板 ROI 无效，请拖拽宽高至少 2 像素的矩形")
            : tr("ROI 无效，请拖拽宽高至少 2 像素的矩形");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void PatternPresenceDialog::refreshDisplayedRoiOverlay()
{
    if (!m_previewHelper)
        return;

    if (m_roiEditTarget == RoiEditTarget::DetectRoi) {
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
        return;
    }

    if (isTemplatePolygonMode() && m_templatePolygonNormalized.size() >= 3) {
        m_previewHelper->clearRoi();
        m_previewHelper->setPolygonRoiNormalized(m_templatePolygonNormalized);
        return;
    }

    m_previewHelper->clearPolygonRoi();
    m_previewHelper->setRoiRectNormalized(m_templateRoiNormalized);
}

bool PatternPresenceDialog::isTemplatePolygonMode() const
{
    const bool basicMode = ui->patternParamsStackedWidget->currentWidget() == ui->basicParamsPage;
    return basicMode ? ui->basicTemplatePolygonButton->isChecked()
                     : ui->templatePolygonButton->isChecked();
}

QString PatternPresenceDialog::templateRoiStatusText() const
{
    if (isTemplatePolygonMode()) {
        if (m_templatePolygonNormalized.size() < 3)
            return tr("模板多边形 ROI 未完成：多边形至少需要 3 个点");

        return tr("模板多边形 ROI 点数=%1，外接矩形 x=%2 y=%3 w=%4 h=%5；算法优先使用多边形域")
                .arg(m_templatePolygonNormalized.size())
                .arg(m_templateRoiNormalized.x(), 0, 'f', 3)
                .arg(m_templateRoiNormalized.y(), 0, 'f', 3)
                .arg(m_templateRoiNormalized.width(), 0, 'f', 3)
                .arg(m_templateRoiNormalized.height(), 0, 'f', 3);
    }

    if (qFuzzyIsNull(m_templateRoiNormalized.x()) &&
        qFuzzyIsNull(m_templateRoiNormalized.y()) &&
        qAbs(m_templateRoiNormalized.width() - 1.0) < 0.000001 &&
        qAbs(m_templateRoiNormalized.height() - 1.0) < 0.000001) {
        return tr("模板 ROI 暂用整张基准图: x=0.000 y=0.000 w=1.000 h=1.000");
    }

    return tr("模板 ROI x=%1 y=%2 w=%3 h=%4")
            .arg(m_templateRoiNormalized.x(), 0, 'f', 3)
            .arg(m_templateRoiNormalized.y(), 0, 'f', 3)
            .arg(m_templateRoiNormalized.width(), 0, 'f', 3)
            .arg(m_templateRoiNormalized.height(), 0, 'f', 3);
}

QString PatternPresenceDialog::detectRoiStatusText() const
{
    return tr("ROI: x=%1 y=%2 w=%3 h=%4")
            .arg(m_roiNormalized.x(), 0, 'f', 3)
            .arg(m_roiNormalized.y(), 0, 'f', 3)
            .arg(m_roiNormalized.width(), 0, 'f', 3)
            .arg(m_roiNormalized.height(), 0, 'f', 3);
}

void PatternPresenceDialog::showReferenceImage()
{
    if (!m_previewHelper)
        return;

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        return;
    }

    ui->viewerTitleLabel->setText(tr("基准图"));
    m_previewHelper->setImage(image);
    refreshDisplayedRoiOverlay();
}

void PatternPresenceDialog::showLiveImage(const QImage &image)
{
    if (m_uiMode != PresenceUiMode::Continuous || !m_previewHelper)
        return;

    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("当前无图像"));
        return;
    }

    ui->viewerTitleLabel->setText(tr("测试图像"));
    m_previewHelper->setImage(image);
    m_previewHelper->setRoiRectNormalized(m_roiNormalized);
}

void PatternPresenceDialog::showSingleShotImage()
{
    if (!m_previewHelper)
        return;

    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        runPatternPresenceOnFrame(frame, tr("当前帧为空"));
        return;
    }

    const cv::Mat snapshot = frame.clone();
    const QImage image = imageFromFrame(snapshot);
    if (!image.isNull()) {
        ui->viewerTitleLabel->setText(tr("单次测试快照"));
        m_previewHelper->setImage(image);
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    }

    runPatternPresenceOnFrame(snapshot, tr("单次测试快照"));
}

void PatternPresenceDialog::startContinuousRun()
{
    setUiMode(PresenceUiMode::Continuous);
    if (m_continuousTimer && !m_continuousTimer->isActive())
        m_continuousTimer->start();
    runContinuousTick();
}

void PatternPresenceDialog::stopContinuousRun()
{
    if (m_continuousTimer)
        m_continuousTimer->stop();

    if (m_frameUpdatedConnection) {
        QObject::disconnect(m_frameUpdatedConnection);
        m_frameUpdatedConnection = QMetaObject::Connection();
    }
}

void PatternPresenceDialog::runContinuousTick()
{
    if (m_uiMode != PresenceUiMode::Continuous || m_presenceRunning)
        return;

    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        runPatternPresenceOnFrame(frame, tr("当前帧为空"));
        return;
    }

    const QImage image = imageFromFrame(frame);
    if (!image.isNull())
        showLiveImage(image);

    runPatternPresenceOnFrame(frame.clone(), tr("测试图像"));
}

void PatternPresenceDialog::runReferenceTest()
{
    if (m_presenceRunning)
        return;

    if (!ensureTemplatePolygonReadyForTest())
        return;

    const cv::Mat referenceImage = ReferenceImageProvider::instance().referenceFrame();
    if (referenceImage.empty()) {
        displayPatternPresenceError(QStringLiteral("no_reference_image"),
                                    tr("基准图为空，无法测试"));
        return;
    }

    m_editingTemplateRoi = false;
    m_roiEditTarget = RoiEditTarget::DetectRoi;
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        QImage image = ReferenceImageProvider::instance().referenceImage();
        if (image.isNull())
            image = imageFromFrame(referenceImage);
        if (!image.isNull()) {
            ui->viewerTitleLabel->setText(tr("基准图"));
            m_previewHelper->setImage(image);
            m_previewHelper->setRoiRectNormalized(m_roiNormalized);
        }
    }

    m_presenceRunning = true;

    ToolConfig config = toToolConfig();
    config.roiNormalized = m_roiNormalized;
    config.params.insert(QStringLiteral("debugPatternPolygonLog"), true);

    ToolRequest request;
    request.config = config;
    request.image = referenceImage.clone();
    request.referenceImage = referenceImage.clone();

    ToolResult result;
    try {
        result = m_testToolEngine.runTool(request);
    } catch (const std::exception &error) {
        m_presenceRunning = false;
        displayPatternPresenceError(QStringLiteral("PatternPresence HALCON error"),
                                    QString::fromLocal8Bit(error.what()));
        return;
    } catch (...) {
        m_presenceRunning = false;
        displayPatternPresenceError(QStringLiteral("PatternPresence HALCON error"),
                                    tr("未知图案检测异常"));
        return;
    }
    ui->viewerTitleLabel->setText(tr("基准图"));
    displayPatternPresenceResult(result);
    m_referencePreviewSnapshot = makeReferenceToolPreviewSnapshot(config, result, m_roiNormalized);

    m_presenceRunning = false;
}

void PatternPresenceDialog::runPatternPresenceOnFrame(const cv::Mat &frame,
                                                      const QString &imageTitle,
                                                      bool referenceTest)
{
    if (m_presenceRunning)
        return;

    if (!ensureTemplatePolygonReadyForTest())
        return;

    m_presenceRunning = true;

    ToolConfig config = toToolConfig();
    config.roiNormalized = m_roiNormalized;
    config.params.insert(QStringLiteral("debugPatternPolygonLog"),
                         m_uiMode != PresenceUiMode::Continuous);

    ToolRequest request;
    request.config = config;
    request.image = frame;
    request.referenceImage = ReferenceImageProvider::instance().referenceFrame();

    ToolResult result;
    try {
        result = m_testToolEngine.runTool(request);
    } catch (const std::exception &error) {
        m_presenceRunning = false;
        displayPatternPresenceError(QStringLiteral("PatternPresence HALCON error"),
                                    QString::fromLocal8Bit(error.what()));
        return;
    } catch (...) {
        m_presenceRunning = false;
        displayPatternPresenceError(QStringLiteral("PatternPresence HALCON error"),
                                    tr("未知图案检测异常"));
        return;
    }
    if (!imageTitle.isEmpty())
        ui->viewerTitleLabel->setText(imageTitle);
    displayPatternPresenceResult(result);
    if (referenceTest)
        m_referencePreviewSnapshot = makeReferenceToolPreviewSnapshot(config, result, m_roiNormalized);

    m_presenceRunning = false;
}

void PatternPresenceDialog::displayPatternPresenceResult(const ToolResult &result)
{
    qDebug() << "[PatternPresenceDialog] ToolResult"
             << "status=" << result.status
             << "message=" << result.message
             << "score=" << result.score
             << "count=" << result.count
             << "ok=" << result.ok;

    QString displayText = result.success
            ? tr("PatternPresence: %1 | score:%2 | count:%3 | %4")
              .arg(result.status,
                   QString::number(result.score, 'f', 3),
                   QString::number(result.count),
                   result.ok ? QStringLiteral("OK") : QStringLiteral("NG"))
            : tr("PatternPresence: %1 | %2").arg(result.status, result.message);
    if (shapeModelContoursTooSparse(result))
        displayText = tr("模型有效轮廓过少，请调整灵敏度或模板区域。");
    setViewerStatusText(displayText, makePresenceStatusTooltipText(result));

    if (m_previewHelper) {
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
        m_previewHelper->setToolOverlays(result.overlays);
    }
}

void PatternPresenceDialog::displayPatternPresenceError(const QString &status, const QString &message)
{
    qDebug() << "[PatternPresenceDialog] ToolResult"
             << "status=" << status
             << "message=" << message
             << "score=" << 0.0
             << "count=" << 0
             << "ok=" << false;

    const QString displayText = tr("PatternPresence: %1 | %2").arg(status, message);
    setViewerStatusText(displayText, makePresenceErrorTooltipText(status, message));
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    }
}

void PatternPresenceDialog::setViewerStatusText(const QString &displayText, const QString &tooltipText)
{
    if (!ui || !ui->viewerStatusLabel)
        return;

    QLabel *label = ui->viewerStatusLabel;
    const QString elidedText = label->fontMetrics().elidedText(displayText,
                                                               Qt::ElideRight,
                                                               labelDisplayWidth(label));
    label->setText(elidedText);
    label->setToolTip(tooltipText.isEmpty() ? displayText : tooltipText);
}

bool PatternPresenceDialog::ensureTemplatePolygonReadyForTest()
{
    if (!isTemplatePolygonMode())
        return true;

    if (m_previewHelper && m_previewHelper->isPolygonDrawingEnabled()) {
        displayPatternPresenceError(QStringLiteral("invalid_template_polygon"),
                                    tr("模板多边形 ROI 未完成：请先点击完成闭合多边形"));
        return false;
    }

    if (m_templatePolygonNormalized.size() < 3) {
        displayPatternPresenceError(QStringLiteral("invalid_template_polygon"),
                                    tr("多边形至少需要 3 个点"));
        return false;
    }

    return true;
}

void PatternPresenceDialog::updateBottomButtons()
{
    if (!m_exitTestButton)
        return;

    if (m_uiMode != PresenceUiMode::Edit) {
        ui->referenceTestButton->hide();
        ui->testRunButton->setText(tr("连续运行"));
        ui->finishButton->setText(tr("运行一次"));
        m_exitTestButton->show();
        return;
    }

    ui->referenceTestButton->show();
    ui->testRunButton->setText(tr("测试运行"));
    ui->finishButton->setText(tr("完成"));
    m_exitTestButton->hide();
}

void PatternPresenceDialog::fitPreview()
{
    if (m_previewHelper)
        m_previewHelper->fitToView();
}
