#include "ContourPresenceDialog.h"
#include "ui_ContourPresenceDialog.h"

#include "PlanDialogUtils.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFontMetrics>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QUuid>
#include <QtGlobal>

#include <opencv2/imgproc.hpp>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolRequest.h"

namespace {

QImage imageFromFrame(const cv::Mat &frame)
{
    return MatImageConverter::matToDisplayImage(frame);
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

    return QRectF(QPointF(qBound(0.0, left, 1.0), qBound(0.0, top, 1.0)),
                  QPointF(qBound(0.0, right, 1.0), qBound(0.0, bottom, 1.0))).normalized();
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

QString judgeModeFromResultBasis(const QString &text)
{
    return text.contains(QObject::tr("最低得分"))
            ? QStringLiteral("score")
            : QStringLiteral("presence");
}

QString scaleModeFromButtons(const bool manual)
{
    return manual ? QStringLiteral("manual") : QStringLiteral("auto");
}

QString thresholdModeFromButtons(const bool manual)
{
    return manual ? QStringLiteral("manual") : QStringLiteral("auto");
}

QString chainModeFromButtons(const bool manual)
{
    return manual ? QStringLiteral("manual") : QStringLiteral("auto");
}

QString makeContourPresenceStatusTooltipText(const ToolResult &result)
{
    return QStringLiteral("status: %1\nmessage: %2\ntext: %3\nscore: %4\nvalue: %5\ncount: %6\nok: %7")
            .arg(result.status,
                 result.message,
                 result.text,
                 QString::number(result.score, 'f', 6),
                 QString::number(result.value, 'f', 6),
                 QString::number(result.count),
                 boolDisplayText(result.ok));
}

QString makeContourPresenceErrorTooltipText(const QString &status, const QString &message)
{
    return QStringLiteral("status: %1\nmessage: %2\ntext: error\nscore: 0.000000\nvalue: 0.000000\ncount: 0\nok: false")
            .arg(status, message);
}

} // namespace

ContourPresenceDialog::ContourPresenceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ContourPresenceDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_basicTemplateRegionGroup(new QButtonGroup(this))
    , m_templateRegionGroup(new QButtonGroup(this))
    , m_basicDetectionRegionGroup(new QButtonGroup(this))
    , m_detectionRegionGroup(new QButtonGroup(this))
    , m_scaleModeGroup(new QButtonGroup(this))
    , m_thresholdModeGroup(new QButtonGroup(this))
    , m_chainModeGroup(new QButtonGroup(this))
    , m_basicResultPresenceGroup(new QButtonGroup(this))
    , m_resultPresenceGroup(new QButtonGroup(this))
{
    ui->setupUi(this);
    m_toolId = QStringLiteral("contour_presence_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_testToolEngine.registerAdapter(&m_testContourPresenceAdapter);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    connectControls();
    showReferenceImage();
}

ContourPresenceDialog::~ContourPresenceDialog()
{
    delete ui;
}

ContourPresenceConfig ContourPresenceDialog::configuration() const
{
    const bool basicMode = ui->contourParamsStackedWidget->currentWidget() == ui->basicParamsPage;
    const QString resultBasisText = basicMode
            ? ui->basicResultBasisComboBox->currentText()
            : ui->resultBasisComboBox->currentText();
    const int angleMin = ui->minAngleSpinBox->value();
    const int angleMax = ui->maxAngleSpinBox->value();

    ContourPresenceConfig config;
    config.templateRoiNormalized = effectiveTemplateRoiNormalized();
    config.templatePolygonNormalized = m_templatePolygonNormalized;
    config.templateSource = QStringLiteral("referenceImage");
    config.detectRegionType = isDetectPolygonMode() ? QStringLiteral("polygon") : QStringLiteral("rect");
    config.detectPolygonNormalized = m_detectPolygonNormalized;
    config.templateShapeType = isTemplatePolygonMode() ? QStringLiteral("polygon") : QStringLiteral("rect");
    config.enablePositionCorrection = basicMode
            ? ui->basicPositionCorrectionSwitch->isChecked()
            : ui->positionCorrectionSwitch->isChecked();
    config.positionCorrectionSource = basicMode
            ? ui->basicPositionCorrectionComboBox->currentText()
            : ui->positionCorrectionComboBox->currentText();
    config.minScore = qBound(0.0, static_cast<double>(ui->minScoreSpinBox->value()) / 100.0, 1.0);
    config.polarity = ui->matchPolarityComboBox->currentText();
    config.thresholdType = ui->thresholdTypeComboBox->currentText();
    config.scaleMode = scaleModeFromButtons(ui->scaleManualButton->isChecked());
    config.speedScale = ui->speedScaleSpinBox->value();
    config.featureScale = ui->featureScaleSpinBox->value();
    config.thresholdMode = thresholdModeFromButtons(ui->thresholdManualButton->isChecked());
    config.grayThreshold = ui->grayThresholdSpinBox->value();
    config.chainMode = chainModeFromButtons(ui->chainManualButton->isChecked());
    config.minChainLength = ui->minChainLengthSpinBox->value();
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
    const int scoreThresholdPercent = basicMode
            ? ui->basicResultMinScoreSpinBox->value()
            : ui->resultMinScoreSpinBox->value();
    config.scoreThreshold = qBound(0.0,
                                   static_cast<double>(scoreThresholdPercent) / 100.0,
                                   1.0);
    return config;
}

ToolConfig ContourPresenceDialog::toToolConfig() const
{
    const ContourPresenceConfig contourConfig = configuration();
    const QString toolId = m_toolId;

    QJsonObject params;
    params.insert(QStringLiteral("templateRoiNormalized"),
                  rectToJson(contourConfig.templateRoiNormalized));
    params.insert(QStringLiteral("templatePolygonNormalized"),
                  pointsToJson(contourConfig.templatePolygonNormalized));
    params.insert(QStringLiteral("templateSource"), contourConfig.templateSource);
    params.insert(QStringLiteral("detectRegionType"), contourConfig.detectRegionType);
    params.insert(QStringLiteral("detectPolygonNormalized"),
                  pointsToJson(contourConfig.detectPolygonNormalized));
    params.insert(QStringLiteral("templateShapeType"), contourConfig.templateShapeType);
    params.insert(QStringLiteral("enablePositionCorrection"), contourConfig.enablePositionCorrection);
    params.insert(QStringLiteral("positionCorrectionSource"), contourConfig.positionCorrectionSource);
    params.insert(QStringLiteral("minScore"), contourConfig.minScore);
    params.insert(QStringLiteral("polarity"), contourConfig.polarity);
    params.insert(QStringLiteral("thresholdType"), contourConfig.thresholdType);
    params.insert(QStringLiteral("scaleMode"), contourConfig.scaleMode);
    params.insert(QStringLiteral("speedScale"), contourConfig.speedScale);
    params.insert(QStringLiteral("featureScale"), contourConfig.featureScale);
    params.insert(QStringLiteral("thresholdMode"), contourConfig.thresholdMode);
    params.insert(QStringLiteral("grayThreshold"), contourConfig.grayThreshold);
    params.insert(QStringLiteral("chainMode"), contourConfig.chainMode);
    params.insert(QStringLiteral("minChainLength"), contourConfig.minChainLength);
    params.insert(QStringLiteral("scaleMin"), contourConfig.scaleMin);
    params.insert(QStringLiteral("scaleMax"), contourConfig.scaleMax);
    params.insert(QStringLiteral("angleStart"), contourConfig.angleStart);
    params.insert(QStringLiteral("angleExtent"), contourConfig.angleExtent);
    params.insert(QStringLiteral("timeoutMs"), contourConfig.timeoutMs);
    params.insert(QStringLiteral("showContourPoints"), contourConfig.showContourPoints);
    params.insert(QStringLiteral("sortMode"), contourConfig.sortMode);
    params.insert(QStringLiteral("judgeBasis"), contourConfig.judgeBasis);
    params.insert(QStringLiteral("existOk"), contourConfig.existOk);
    params.insert(QStringLiteral("scoreThreshold"), contourConfig.scoreThreshold);

    QJsonObject judgeRule;
    judgeRule.insert(QStringLiteral("mode"), contourConfig.judgeBasis);
    judgeRule.insert(QStringLiteral("existOk"), contourConfig.existOk);
    judgeRule.insert(QStringLiteral("scoreThreshold"), contourConfig.scoreThreshold);

    ToolConfig config;
    config.toolId = toolId;
    config.toolName = tr("轮廓有无");
    config.toolType = ToolType::ContourPresence;
    config.category = ToolCategory::Presence;
    config.enabled = m_enabled;
    config.roiNormalized = effectiveRoiNormalized();
    config.params = params;
    config.judgeRule = judgeRule;
    config.displayName = tr("轮廓有无");
    config.summary = summaryText();
    return config;
}

ToolConfig ContourPresenceDialog::toolConfig() const
{
    return m_hasAcceptedToolConfig ? m_acceptedToolConfig : toToolConfig();
}

ToolPreviewSnapshot ContourPresenceDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void ContourPresenceDialog::loadFromConfig(const ToolConfig &config)
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
    m_detectPolygonNormalized = pointsFromJson(params.value(QStringLiteral("detectPolygonNormalized")).toArray());
    const bool templatePolygonMode = params.value(QStringLiteral("templateShapeType")).toString().trimmed().toLower()
            == QStringLiteral("polygon") && m_templatePolygonNormalized.size() >= 3;
    const bool detectPolygonMode = params.value(QStringLiteral("detectRegionType")).toString().trimmed().toLower()
            == QStringLiteral("polygon") && m_detectPolygonNormalized.size() >= 3;
    if (detectPolygonMode)
        m_roiNormalized = boundingRectForPoints(m_detectPolygonNormalized);

    const bool allMode = params.contains(QStringLiteral("scaleMode"))
            || params.contains(QStringLiteral("thresholdMode"))
            || params.contains(QStringLiteral("chainMode"));
    ui->basicSegmentButton->setChecked(!allMode);
    ui->allSegmentButton->setChecked(allMode);
    ui->contourParamsStackedWidget->setCurrentWidget(allMode ? ui->allParamsPage : ui->basicParamsPage);

    const bool positionCorrection = params.value(QStringLiteral("enablePositionCorrection")).toBool(ui->basicPositionCorrectionSwitch->isChecked());
    ui->basicPositionCorrectionSwitch->setChecked(positionCorrection);
    ui->positionCorrectionSwitch->setChecked(positionCorrection);
    setComboBoxValue(ui->basicPositionCorrectionComboBox, params.value(QStringLiteral("positionCorrectionSource")).toString());
    setComboBoxValue(ui->positionCorrectionComboBox, params.value(QStringLiteral("positionCorrectionSource")).toString());
    ui->minScoreSpinBox->setValue(qRound(params.value(QStringLiteral("minScore")).toDouble(ui->minScoreSpinBox->value() / 100.0) * 100.0));
    setComboBoxValue(ui->matchPolarityComboBox, params.value(QStringLiteral("polarity")).toString());
    setComboBoxValue(ui->thresholdTypeComboBox, params.value(QStringLiteral("thresholdType")).toString());
    const bool scaleManual = params.value(QStringLiteral("scaleMode")).toString() == QStringLiteral("manual");
    ui->scaleManualButton->setChecked(scaleManual);
    ui->scaleAutoButton->setChecked(!scaleManual);
    ui->speedScaleSpinBox->setValue(params.value(QStringLiteral("speedScale")).toInt(ui->speedScaleSpinBox->value()));
    ui->featureScaleSpinBox->setValue(params.value(QStringLiteral("featureScale")).toInt(ui->featureScaleSpinBox->value()));
    const bool thresholdManual = params.value(QStringLiteral("thresholdMode")).toString() == QStringLiteral("manual");
    ui->thresholdManualButton->setChecked(thresholdManual);
    ui->thresholdAutoButton->setChecked(!thresholdManual);
    ui->grayThresholdSpinBox->setValue(params.value(QStringLiteral("grayThreshold")).toInt(ui->grayThresholdSpinBox->value()));
    const bool chainManual = params.value(QStringLiteral("chainMode")).toString() == QStringLiteral("manual");
    ui->chainManualButton->setChecked(chainManual);
    ui->chainAutoButton->setChecked(!chainManual);
    ui->minChainLengthSpinBox->setValue(params.value(QStringLiteral("minChainLength")).toInt(ui->minChainLengthSpinBox->value()));
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
    const int scoreThreshold = qRound(params.value(QStringLiteral("scoreThreshold")).toDouble(0.5) * 100.0);
    ui->basicResultMinScoreSpinBox->setValue(scoreThreshold);
    ui->resultMinScoreSpinBox->setValue(scoreThreshold);

    ui->basicTemplateRectButton->setChecked(!templatePolygonMode);
    ui->templateRectButton->setChecked(!templatePolygonMode);
    ui->basicTemplatePolygonButton->setChecked(templatePolygonMode);
    ui->templatePolygonButton->setChecked(templatePolygonMode);
    ui->basicDetectionRectButton->setChecked(!detectPolygonMode);
    ui->detectionRectButton->setChecked(!detectPolygonMode);
    ui->basicDetectionPolygonButton->setChecked(detectPolygonMode);
    ui->detectionPolygonButton->setChecked(detectPolygonMode);
    ui->basicDetectionDrawButton->setChecked(false);
    ui->detectionDrawButton->setChecked(false);
    ui->basicDetectionCircleButton->setChecked(false);
    ui->detectionCircleButton->setChecked(false);
    m_roiEditTarget = RoiEditTarget::DetectRoi;
    m_hasAcceptedToolConfig = false;
    m_acceptedToolConfig = ToolConfig();
    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    refreshDisplayedRoiOverlay();
    const QString roiText = detectRoiStatusText();
    setViewerStatusText(roiText, roiText);
}

QString ContourPresenceDialog::summaryText() const
{
    const ContourPresenceConfig config = configuration();
    const QString templateShape = config.templateShapeType == QStringLiteral("polygon")
            ? tr("多边形")
            : tr("矩形");
    const QString detectShape = config.detectRegionType == QStringLiteral("polygon")
            ? tr("多边形")
            : tr("矩形");
    if (config.judgeBasis == QStringLiteral("score")) {
        return tr("模板: %1, 检测区: %2, 判断: 最低得分 >= %3")
                .arg(templateShape,
                     detectShape)
                .arg(QString::number(config.scoreThreshold, 'f', 2));
    }

    return tr("模板: %1, 检测区: %2, 判断: %3, 最小得分: %4")
            .arg(templateShape,
                 detectShape,
                 config.existOk ? tr("存在OK") : tr("不存在OK"))
            .arg(QString::number(config.minScore, 'f', 2));
}

void ContourPresenceDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void ContourPresenceDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 轮廓有无"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    applyAdaptiveWindowSize();

    ui->matchPolarityComboBox->setCurrentIndex(1);
    ui->thresholdTypeComboBox->setCurrentIndex(0);
    ui->sortModeComboBox->setCurrentIndex(4);
    ui->basicSegmentButton->setChecked(true);
    ui->allSegmentButton->setChecked(false);
    ui->contourParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
    ui->basicResultBasisStackedWidget->setCurrentIndex(ui->basicResultBasisComboBox->currentIndex());
    ui->resultBasisStackedWidget->setCurrentIndex(ui->resultBasisComboBox->currentIndex());
    ui->basicTemplateRectButton->setChecked(true);
    ui->templateRectButton->setChecked(true);
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    ui->basicDetectionDrawButton->setChecked(false);
    ui->detectionDrawButton->setChecked(false);
    ui->basicDetectionCircleButton->setChecked(false);
    ui->detectionCircleButton->setChecked(false);
    ui->basicDetectionPolygonButton->setChecked(false);
    ui->detectionPolygonButton->setChecked(false);
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
    configureRoiToolButton(ui->basicDetectionPolygonButton, QStringLiteral("⬡"), tr("多边形检测 ROI"));
    configureRoiToolButton(ui->detectionPolygonButton, QStringLiteral("⬡"), tr("多边形检测 ROI"));
    ui->scaleSpeedRow->hide();
    ui->scaleFeatureRow->hide();
    ui->thresholdGrayRow->hide();
    ui->chainMinRow->hide();

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

void ContourPresenceDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &ContourPresenceDialog::reject);
    connect(ui->referenceTestButton, &QPushButton::clicked, this, &ContourPresenceDialog::runReferenceTest);
    connect(ui->testRunButton, &QPushButton::clicked, this, &ContourPresenceDialog::runCameraTest);
    connect(ui->finishButton, &QPushButton::clicked, this, &ContourPresenceDialog::finishConfiguration);

    connect(ui->basicResultBasisComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                const QSignalBlocker blocker(ui->resultBasisComboBox);
                ui->resultBasisComboBox->setCurrentIndex(index);
                ui->basicResultBasisStackedWidget->setCurrentIndex(index);
                ui->resultBasisStackedWidget->setCurrentIndex(index);
            });
    connect(ui->resultBasisComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                const QSignalBlocker blocker(ui->basicResultBasisComboBox);
                ui->basicResultBasisComboBox->setCurrentIndex(index);
                ui->basicResultBasisStackedWidget->setCurrentIndex(index);
                ui->resultBasisStackedWidget->setCurrentIndex(index);
            });
    connect(ui->basicShowContourSwitch, &QCheckBox::toggled, this, [this](bool checked) {
        const QSignalBlocker blocker(ui->showContourSwitch);
        ui->showContourSwitch->setChecked(checked);
    });
    connect(ui->showContourSwitch, &QCheckBox::toggled, this, [this](bool checked) {
        const QSignalBlocker blocker(ui->basicShowContourSwitch);
        ui->basicShowContourSwitch->setChecked(checked);
    });

    const auto applyParamMode = [this](bool allMode) {
        ui->basicSegmentButton->setChecked(!allMode);
        ui->allSegmentButton->setChecked(allMode);
        ui->contourParamsStackedWidget->setCurrentWidget(allMode ? ui->allParamsPage : ui->basicParamsPage);
    };

    m_segmentGroup->setExclusive(true);
    m_segmentGroup->addButton(ui->basicSegmentButton, 0);
    m_segmentGroup->addButton(ui->allSegmentButton, 1);
    connect(ui->basicSegmentButton, &QPushButton::clicked, this, [applyParamMode]() {
        applyParamMode(false);
    });
    connect(ui->allSegmentButton, &QPushButton::clicked, this, [applyParamMode]() {
        applyParamMode(true);
    });

    m_basicTemplateRegionGroup->setExclusive(true);
    m_basicTemplateRegionGroup->addButton(ui->basicTemplateRectButton, 0);
    m_basicTemplateRegionGroup->addButton(ui->basicTemplatePolygonButton, 1);
    m_templateRegionGroup->setExclusive(true);
    m_templateRegionGroup->addButton(ui->templateRectButton, 0);
    m_templateRegionGroup->addButton(ui->templatePolygonButton, 1);

    connect(ui->basicTemplateRectButton, &QToolButton::clicked, this, &ContourPresenceDialog::startTemplateRoiEditing);
    connect(ui->templateRectButton, &QToolButton::clicked, this, &ContourPresenceDialog::startTemplateRoiEditing);
    connect(ui->basicTemplateFinishButton, &QPushButton::clicked, this, &ContourPresenceDialog::finishTemplateRoiEditing);
    connect(ui->templateFinishButton, &QPushButton::clicked, this, &ContourPresenceDialog::finishTemplateRoiEditing);
    connect(ui->basicTemplatePolygonButton, &QToolButton::clicked, this, &ContourPresenceDialog::startTemplatePolygonEditing);
    connect(ui->templatePolygonButton, &QToolButton::clicked, this, &ContourPresenceDialog::startTemplatePolygonEditing);

    m_basicDetectionRegionGroup->setExclusive(true);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionDrawButton, 0);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionRectButton, 1);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionCircleButton, 2);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionPolygonButton, 3);
    m_detectionRegionGroup->setExclusive(true);
    m_detectionRegionGroup->addButton(ui->detectionDrawButton, 0);
    m_detectionRegionGroup->addButton(ui->detectionRectButton, 1);
    m_detectionRegionGroup->addButton(ui->detectionCircleButton, 2);
    m_detectionRegionGroup->addButton(ui->detectionPolygonButton, 3);

    connect(ui->basicDetectionRectButton, &QToolButton::clicked, this, &ContourPresenceDialog::startDetectRoiEditing);
    connect(ui->detectionRectButton, &QToolButton::clicked, this, &ContourPresenceDialog::startDetectRoiEditing);
    connect(ui->basicDetectionPolygonButton, &QToolButton::clicked, this, &ContourPresenceDialog::startDetectPolygonEditing);
    connect(ui->detectionPolygonButton, &QToolButton::clicked, this, &ContourPresenceDialog::startDetectPolygonEditing);
    connect(ui->basicDetectionDrawButton, &QToolButton::clicked, this, [this]() {
        ui->basicDetectionRectButton->setChecked(true);
        ui->detectionRectButton->setChecked(true);
        showDetectRoiTodo(tr("自由绘制检测 ROI 暂未实现，请使用矩形或多边形检测区域"));
    });
    connect(ui->detectionDrawButton, &QToolButton::clicked, this, [this]() {
        ui->basicDetectionRectButton->setChecked(true);
        ui->detectionRectButton->setChecked(true);
        showDetectRoiTodo(tr("自由绘制检测 ROI 暂未实现，请使用矩形或多边形检测区域"));
    });
    connect(ui->basicDetectionCircleButton, &QToolButton::clicked, this, [this]() {
        ui->basicDetectionRectButton->setChecked(true);
        ui->detectionRectButton->setChecked(true);
        showDetectRoiTodo(tr("圆形检测 ROI 暂未实现，请使用矩形或多边形检测区域"));
    });
    connect(ui->detectionCircleButton, &QToolButton::clicked, this, [this]() {
        ui->basicDetectionRectButton->setChecked(true);
        ui->detectionRectButton->setChecked(true);
        showDetectRoiTodo(tr("圆形检测 ROI 暂未实现，请使用矩形或多边形检测区域"));
    });

    connect(ui->templateMaskEditButton, &QPushButton::clicked, this, [this]() {
        showTemplateRoiTodo(tr("屏蔽区域暂未实现"));
    });
    connect(ui->detectionMaskEditButton, &QPushButton::clicked, this, [this]() {
        showDetectRoiTodo(tr("屏蔽区域暂未实现"));
    });

    m_scaleModeGroup->setExclusive(true);
    m_scaleModeGroup->addButton(ui->scaleManualButton, 0);
    m_scaleModeGroup->addButton(ui->scaleAutoButton, 1);

    m_thresholdModeGroup->setExclusive(true);
    m_thresholdModeGroup->addButton(ui->thresholdManualButton, 0);
    m_thresholdModeGroup->addButton(ui->thresholdAutoButton, 1);

    m_chainModeGroup->setExclusive(true);
    m_chainModeGroup->addButton(ui->chainManualButton, 0);
    m_chainModeGroup->addButton(ui->chainAutoButton, 1);

    const auto updateScaleRows = [this]() {
        const bool manual = ui->scaleManualButton->isChecked();
        ui->scaleSpeedRow->setVisible(manual);
        ui->scaleFeatureRow->setVisible(manual);
    };
    const auto updateThresholdRows = [this]() {
        ui->thresholdGrayRow->setVisible(ui->thresholdManualButton->isChecked());
    };
    const auto updateChainRows = [this]() {
        ui->chainMinRow->setVisible(ui->chainManualButton->isChecked());
    };
    connect(ui->scaleManualButton, &QPushButton::toggled, this, [updateScaleRows]() {
        updateScaleRows();
    });
    connect(ui->scaleAutoButton, &QPushButton::toggled, this, [updateScaleRows]() {
        updateScaleRows();
    });
    connect(ui->thresholdManualButton, &QPushButton::toggled, this, [updateThresholdRows]() {
        updateThresholdRows();
    });
    connect(ui->thresholdAutoButton, &QPushButton::toggled, this, [updateThresholdRows]() {
        updateThresholdRows();
    });
    connect(ui->chainManualButton, &QPushButton::toggled, this, [updateChainRows]() {
        updateChainRows();
    });
    connect(ui->chainAutoButton, &QPushButton::toggled, this, [updateChainRows]() {
        updateChainRows();
    });
    updateScaleRows();
    updateThresholdRows();
    updateChainRows();

    m_basicResultPresenceGroup->setExclusive(true);
    m_basicResultPresenceGroup->addButton(ui->basicPresentOkButton, 0);
    m_basicResultPresenceGroup->addButton(ui->basicAbsentOkButton, 1);
    m_resultPresenceGroup->setExclusive(true);
    m_resultPresenceGroup->addButton(ui->presentOkButton, 0);
    m_resultPresenceGroup->addButton(ui->absentOkButton, 1);

    connect(ui->minScaleSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value) {
        if (value > ui->maxScaleSpinBox->value())
            ui->maxScaleSpinBox->setValue(value);
    });
    connect(ui->maxScaleSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value) {
        if (value < ui->minScaleSpinBox->value())
            ui->minScaleSpinBox->setValue(value);
    });
    connect(ui->minAngleSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value) {
        if (value > ui->maxAngleSpinBox->value())
            ui->maxAngleSpinBox->setValue(value);
    });
    connect(ui->maxAngleSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [this](int value) {
        if (value < ui->minAngleSpinBox->value())
            ui->minAngleSpinBox->setValue(value);
    });

    if (m_previewHelper) {
        connect(m_previewHelper,
                &FrameViewHelper::roiChanged,
                this,
                &ContourPresenceDialog::handleRoiChanged);
        connect(m_previewHelper,
                &FrameViewHelper::roiSelectionRejected,
                this,
                [this](const QRectF &) {
                    handleRoiSelectionRejected();
                });
        connect(m_previewHelper,
                &FrameViewHelper::polygonChanged,
                this,
                &ContourPresenceDialog::handlePolygonChanged);
        connect(m_previewHelper,
                &FrameViewHelper::polygonSelectionRejected,
                this,
                &ContourPresenceDialog::handlePolygonSelectionRejected);
    }

    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) {
                if (!m_contourPresenceRunning)
                    showReferenceImage();
            });
}

void ContourPresenceDialog::finishConfiguration()
{
    if ((isTemplatePolygonMode() || isDetectPolygonMode()) &&
        m_previewHelper &&
        m_previewHelper->isPolygonDrawingEnabled() &&
        !m_previewHelper->finishPolygonDrawing()) {
        const int pointCount = m_roiEditTarget == RoiEditTarget::TemplateRoi
                ? m_templatePolygonNormalized.size()
                : m_detectPolygonNormalized.size();
        handlePolygonSelectionRejected(pointCount);
        return;
    }

    if (isTemplatePolygonMode() && m_templatePolygonNormalized.size() < 3) {
        handlePolygonSelectionRejected(m_templatePolygonNormalized.size());
        return;
    }
    if (isDetectPolygonMode() && m_detectPolygonNormalized.size() < 3) {
        handlePolygonSelectionRejected(m_detectPolygonNormalized.size());
        return;
    }

    m_acceptedToolConfig = toToolConfig();
    m_hasAcceptedToolConfig = true;
    accept();
}

void ContourPresenceDialog::runReferenceTest()
{
    const cv::Mat referenceImage = ReferenceImageProvider::instance().referenceFrame();
    if (referenceImage.empty()) {
        displayContourPresenceError(QStringLiteral("no_reference_image"),
                                    tr("基准图为空，无法测试"));
        return;
    }

    QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull())
        image = imageFromFrame(referenceImage);
    if (!image.isNull() && m_previewHelper) {
        ui->viewerTitleLabel->setText(tr("基准图"));
        m_previewHelper->setImage(image);
        refreshDisplayedRoiOverlay();
    }

    runContourPresenceOnFrame(referenceImage.clone(),
                              referenceImage.clone(),
                              tr("基准图"),
                              tr("基准图为空，无法测试"),
                              true);
}

void ContourPresenceDialog::runCameraTest()
{
    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        displayContourPresenceError(QStringLiteral("image_empty"),
                                    tr("当前图像为空，无法测试"));
        return;
    }

    const cv::Mat snapshot = frame.clone();
    const QImage image = imageFromFrame(snapshot);
    if (!image.isNull() && m_previewHelper) {
        ui->viewerTitleLabel->setText(tr("测试图像"));
        m_previewHelper->setImage(image);
        refreshDisplayedRoiOverlay();
    }

    runContourPresenceOnFrame(snapshot,
                              ReferenceImageProvider::instance().referenceFrame(),
                              tr("测试图像"),
                              tr("当前图像为空，无法测试"),
                              false);
}

void ContourPresenceDialog::applyAdaptiveWindowSize()
{
    PlanDialogUtils::applyLargeWindow(this);
}

void ContourPresenceDialog::fitPreview()
{
    if (m_previewHelper)
        m_previewHelper->fitToView();
}

void ContourPresenceDialog::showReferenceImage()
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

void ContourPresenceDialog::showFrameForRoiEditing()
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
        const QString text = tr("请先设置基准图或提供当前图像后再选择检测区域");
        setViewerStatusText(text, text);
        return;
    }

    ui->viewerTitleLabel->setText(title);
    m_previewHelper->setImage(image);
    m_previewHelper->clearToolOverlays();
    refreshDisplayedRoiOverlay();
}

void ContourPresenceDialog::startTemplateRoiEditing()
{
    if (!m_previewHelper)
        return;

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        const QString text = tr("请先设置基准图后再选择模板区域");
        setViewerStatusText(text, text);
        return;
    }

    m_roiEditTarget = RoiEditTarget::TemplateRoi;
    ui->basicTemplateRectButton->setChecked(true);
    ui->templateRectButton->setChecked(true);
    ui->basicTemplatePolygonButton->setChecked(false);
    ui->templatePolygonButton->setChecked(false);
    m_templatePolygonNormalized.clear();
    showReferenceImage();
    ui->viewerTitleLabel->setText(tr("模板区域"));
    m_previewHelper->clearToolOverlays();
    m_previewHelper->clearPolygonRoi();
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->setRoiRectNormalized(effectiveTemplateRoiNormalized());
    m_previewHelper->setRoiDrawingEnabled(true);
    const QString text = tr("请在右侧基准图上拖拽矩形选择模板 ROI");
    setViewerStatusText(text, text);
}

void ContourPresenceDialog::startTemplatePolygonEditing()
{
    if (!m_previewHelper)
        return;

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->setPolygonDrawingEnabled(false);
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        const QString text = tr("请先设置基准图后再选择模板区域");
        setViewerStatusText(text, text);
        return;
    }

    m_roiEditTarget = RoiEditTarget::TemplateRoi;
    ui->basicTemplateRectButton->setChecked(false);
    ui->templateRectButton->setChecked(false);
    ui->basicTemplatePolygonButton->setChecked(true);
    ui->templatePolygonButton->setChecked(true);
    showReferenceImage();
    ui->viewerTitleLabel->setText(tr("模板多边形区域"));
    m_previewHelper->clearToolOverlays();
    m_previewHelper->clearRoi();
    m_previewHelper->setRoiDrawingEnabled(false);
    if (m_templatePolygonNormalized.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(m_templatePolygonNormalized);
    else
        m_previewHelper->clearPolygonRoi();
    m_previewHelper->setPolygonDrawingEnabled(true);

    const QString text = tr("当前编辑：多边形 ROI。左键添加点，靠近首点点击自动闭合，右键撤销，Esc 取消。");
    setViewerStatusText(text, text);
}

void ContourPresenceDialog::finishTemplateRoiEditing()
{
    m_roiEditTarget = RoiEditTarget::TemplateRoi;
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
    }

    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setPolygonDrawingEnabled(false);
        showReferenceImage();
        ui->viewerTitleLabel->setText(tr("模板区域"));
    }

    const QString text = templateRoiStatusText();
    setViewerStatusText(text, text);
}

void ContourPresenceDialog::startDetectRoiEditing()
{
    if (!m_previewHelper)
        return;

    m_roiEditTarget = RoiEditTarget::DetectRoi;
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    ui->basicDetectionPolygonButton->setChecked(false);
    ui->detectionPolygonButton->setChecked(false);
    m_detectPolygonNormalized.clear();
    showFrameForRoiEditing();
    if (m_previewHelper->imageSize().isEmpty())
        return;

    m_previewHelper->clearPolygonRoi();
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    m_previewHelper->setRoiDrawingEnabled(true);
    const QString text = tr("请在右侧图像上拖拽矩形选择检测 ROI");
    setViewerStatusText(text, text);
}

void ContourPresenceDialog::startDetectPolygonEditing()
{
    if (!m_previewHelper)
        return;

    m_roiEditTarget = RoiEditTarget::DetectRoi;
    ui->basicDetectionRectButton->setChecked(false);
    ui->detectionRectButton->setChecked(false);
    ui->basicDetectionCircleButton->setChecked(false);
    ui->detectionCircleButton->setChecked(false);
    ui->basicDetectionPolygonButton->setChecked(true);
    ui->detectionPolygonButton->setChecked(true);
    showFrameForRoiEditing();
    if (m_previewHelper->imageSize().isEmpty())
        return;

    m_previewHelper->clearToolOverlays();
    m_previewHelper->clearRoi();
    m_previewHelper->setRoiDrawingEnabled(false);
    if (m_detectPolygonNormalized.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(m_detectPolygonNormalized);
    else
        m_previewHelper->clearPolygonRoi();
    m_previewHelper->setPolygonDrawingEnabled(true);

    const QString text = tr("当前编辑：多边形 ROI。左键添加点，靠近首点点击自动闭合，右键撤销，Esc 取消。");
    setViewerStatusText(text, text);
}

void ContourPresenceDialog::showTemplateRoiTodo(const QString &message)
{
    m_roiEditTarget = RoiEditTarget::TemplateRoi;
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setPolygonDrawingEnabled(false);
        showReferenceImage();
        refreshDisplayedRoiOverlay();
    }
    setViewerStatusText(message, message);
}

void ContourPresenceDialog::showDetectRoiTodo(const QString &message)
{
    m_roiEditTarget = RoiEditTarget::DetectRoi;
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setPolygonDrawingEnabled(false);
        refreshDisplayedRoiOverlay();
    }
    setViewerStatusText(message, message);
}

void ContourPresenceDialog::handleRoiChanged(const QRectF &roi)
{
    if (m_roiEditTarget == RoiEditTarget::TemplateRoi) {
        m_templateRoiNormalized = roi;
        m_templatePolygonNormalized.clear();
        ui->basicTemplateRectButton->setChecked(true);
        ui->templateRectButton->setChecked(true);
        ui->basicTemplatePolygonButton->setChecked(false);
        ui->templatePolygonButton->setChecked(false);
        if (m_previewHelper) {
            m_previewHelper->clearToolOverlays();
            m_previewHelper->clearPolygonRoi();
            m_previewHelper->setRoiRectNormalized(effectiveTemplateRoiNormalized());
        }
        const QString text = templateRoiStatusText();
        setViewerStatusText(text, text);
        qDebug() << "[ContourPresenceDialog] Template ROI normalized:" << m_templateRoiNormalized;
        return;
    }

    m_roiNormalized = roi;
    m_detectPolygonNormalized.clear();
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    ui->basicDetectionPolygonButton->setChecked(false);
    ui->detectionPolygonButton->setChecked(false);
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    const QString text = detectRoiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[ContourPresenceDialog] Detect ROI normalized:" << m_roiNormalized;
}

void ContourPresenceDialog::handlePolygonChanged(const QVector<QPointF> &points)
{
    if (points.size() < 3)
        return;

    if (m_roiEditTarget == RoiEditTarget::TemplateRoi) {
        m_templatePolygonNormalized = points;
        m_templateRoiNormalized = boundingRectForPoints(m_templatePolygonNormalized);
        ui->basicTemplateRectButton->setChecked(false);
        ui->templateRectButton->setChecked(false);
        ui->basicTemplatePolygonButton->setChecked(true);
        ui->templatePolygonButton->setChecked(true);
        if (m_previewHelper) {
            m_previewHelper->clearToolOverlays();
            m_previewHelper->clearRoi();
            m_previewHelper->setPolygonRoiNormalized(m_templatePolygonNormalized);
        }
        const QString text = templateRoiStatusText();
        setViewerStatusText(text, text);
        qDebug() << "[ContourPresenceDialog] Template polygon ROI points:" << m_templatePolygonNormalized.size()
                 << "bounding:" << m_templateRoiNormalized;
        return;
    }

    m_detectPolygonNormalized = points;
    m_roiNormalized = boundingRectForPoints(m_detectPolygonNormalized);
    ui->basicDetectionRectButton->setChecked(false);
    ui->detectionRectButton->setChecked(false);
    ui->basicDetectionCircleButton->setChecked(false);
    ui->detectionCircleButton->setChecked(false);
    ui->basicDetectionPolygonButton->setChecked(true);
    ui->detectionPolygonButton->setChecked(true);
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearRoi();
        m_previewHelper->setPolygonRoiNormalized(m_detectPolygonNormalized);
    }

    const QString text = detectRoiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[ContourPresenceDialog] Detect polygon ROI points:" << m_detectPolygonNormalized.size()
             << "bounding:" << m_roiNormalized;
}

void ContourPresenceDialog::handlePolygonSelectionRejected(int pointCount)
{
    Q_UNUSED(pointCount)
    const QString text = tr("多边形至少需要 3 个点。");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void ContourPresenceDialog::handleRoiSelectionRejected()
{
    const QString text = m_roiEditTarget == RoiEditTarget::TemplateRoi
            ? tr("模板 ROI 无效，请拖拽宽高至少 2 像素的矩形")
            : tr("检测 ROI 无效，请拖拽宽高至少 2 像素的矩形");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void ContourPresenceDialog::refreshDisplayedRoiOverlay()
{
    if (!m_previewHelper)
        return;

    m_previewHelper->clearPolygonRoi();
    m_previewHelper->clearRoi();

    if (m_roiEditTarget == RoiEditTarget::TemplateRoi) {
        if (isTemplatePolygonMode() && m_templatePolygonNormalized.size() >= 3)
            m_previewHelper->setPolygonRoiNormalized(m_templatePolygonNormalized);
        else
            m_previewHelper->setRoiRectNormalized(effectiveTemplateRoiNormalized());
        return;
    }

    if (isDetectPolygonMode() && m_detectPolygonNormalized.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(m_detectPolygonNormalized);
    else
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void ContourPresenceDialog::runContourPresenceOnFrame(const cv::Mat &frame,
                                                      const cv::Mat &referenceImage,
                                                      const QString &imageTitle,
                                                      const QString &emptyFrameMessage,
                                                      bool referenceTest)
{
    if (m_contourPresenceRunning)
        return;

    if (frame.empty()) {
        displayContourPresenceError(QStringLiteral("image_empty"), emptyFrameMessage);
        return;
    }

    m_contourPresenceRunning = true;
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        refreshDisplayedRoiOverlay();
    }

    ToolConfig config = toToolConfig();
    config.roiNormalized = effectiveRoiNormalized();

    ToolRequest request;
    request.config = config;
    request.image = frame.clone();
    request.referenceImage = referenceImage.empty() ? cv::Mat() : referenceImage.clone();
    request.runtimeContext.insert(QStringLiteral("referenceTest"), referenceTest);

    const ToolResult result = m_testToolEngine.runTool(request);
    if (!imageTitle.isEmpty())
        ui->viewerTitleLabel->setText(imageTitle);
    displayContourPresenceResult(result);
    if (referenceTest)
        m_referencePreviewSnapshot = makeReferenceToolPreviewSnapshot(config, result, effectiveRoiNormalized());

    m_contourPresenceRunning = false;
}

void ContourPresenceDialog::displayContourPresenceResult(const ToolResult &result)
{
    qDebug() << "[ContourPresenceDialog] ToolResult"
             << "status=" << result.status
             << "message=" << result.message
             << "score=" << result.score
             << "value=" << result.value
             << "count=" << result.count
             << "ok=" << result.ok;

    const QString displayText = result.success
            ? tr("ContourPresence: %1 | score:%2 | count:%3 | %4")
              .arg(result.text.isEmpty() ? result.status : result.text,
                   QString::number(result.score, 'f', 3),
                   QString::number(result.count),
                   result.ok ? QStringLiteral("OK") : QStringLiteral("NG"))
            : tr("ContourPresence: %1 | %2").arg(result.status, result.message);
    setViewerStatusText(displayText, makeContourPresenceStatusTooltipText(result));

    if (m_previewHelper) {
        refreshDisplayedRoiOverlay();
        m_previewHelper->setToolOverlays(result.overlays);
    }
}

void ContourPresenceDialog::displayContourPresenceError(const QString &status, const QString &message)
{
    qDebug() << "[ContourPresenceDialog] ToolResult"
             << "status=" << status
             << "message=" << message
             << "score=" << 0.0
             << "value=" << 0.0
             << "count=" << 0
             << "ok=" << false;

    const QString displayText = tr("ContourPresence: %1 | %2").arg(status, message);
    setViewerStatusText(displayText, makeContourPresenceErrorTooltipText(status, message));
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        refreshDisplayedRoiOverlay();
    }
}

void ContourPresenceDialog::setViewerStatusText(const QString &displayText, const QString &tooltipText)
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

QString ContourPresenceDialog::templateRoiStatusText() const
{
    const QRectF roi = effectiveTemplateRoiNormalized();
    if (isTemplatePolygonMode()) {
        if (m_templatePolygonNormalized.size() < 3)
            return tr("模板多边形 ROI 未完成：多边形至少需要 3 个点");

        return tr("模板多边形 ROI 点数=%1，外接矩形 x=%2 y=%3 w=%4 h=%5")
                .arg(m_templatePolygonNormalized.size())
                .arg(roi.x(), 0, 'f', 3)
                .arg(roi.y(), 0, 'f', 3)
                .arg(roi.width(), 0, 'f', 3)
                .arg(roi.height(), 0, 'f', 3);
    }

    if (qFuzzyIsNull(roi.x()) &&
        qFuzzyIsNull(roi.y()) &&
        qAbs(roi.width() - 1.0) < 0.000001 &&
        qAbs(roi.height() - 1.0) < 0.000001) {
        return tr("模板 ROI 默认整张基准图: x=0.000 y=0.000 w=1.000 h=1.000");
    }

    return tr("模板 ROI x=%1 y=%2 w=%3 h=%4")
            .arg(roi.x(), 0, 'f', 3)
            .arg(roi.y(), 0, 'f', 3)
            .arg(roi.width(), 0, 'f', 3)
            .arg(roi.height(), 0, 'f', 3);
}

QString ContourPresenceDialog::detectRoiStatusText() const
{
    const QRectF roi = effectiveRoiNormalized();
    if (isDetectPolygonMode()) {
        if (m_detectPolygonNormalized.size() < 3)
            return tr("检测多边形 ROI 未完成：多边形至少需要 3 个点");

        return tr("检测多边形 ROI 点数=%1，外接矩形 x=%2 y=%3 w=%4 h=%5")
                .arg(m_detectPolygonNormalized.size())
                .arg(roi.x(), 0, 'f', 3)
                .arg(roi.y(), 0, 'f', 3)
                .arg(roi.width(), 0, 'f', 3)
                .arg(roi.height(), 0, 'f', 3);
    }

    return tr("检测 ROI x=%1 y=%2 w=%3 h=%4")
            .arg(roi.x(), 0, 'f', 3)
            .arg(roi.y(), 0, 'f', 3)
            .arg(roi.width(), 0, 'f', 3)
            .arg(roi.height(), 0, 'f', 3);
}

QRectF ContourPresenceDialog::effectiveTemplateRoiNormalized() const
{
    if (isTemplatePolygonMode() && m_templatePolygonNormalized.size() >= 3) {
        const QRectF polygonRect = boundingRectForPoints(m_templatePolygonNormalized);
        if (polygonRect.width() > 0.0 && polygonRect.height() > 0.0)
            return polygonRect;
    }

    if (m_templateRoiNormalized.width() <= 0.0 || m_templateRoiNormalized.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    const QRectF roi = m_templateRoiNormalized.normalized().intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (roi.width() <= 0.0 || roi.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    return roi;
}

QRectF ContourPresenceDialog::effectiveRoiNormalized() const
{
    if (isDetectPolygonMode() && m_detectPolygonNormalized.size() >= 3) {
        const QRectF polygonRect = boundingRectForPoints(m_detectPolygonNormalized);
        if (polygonRect.width() > 0.0 && polygonRect.height() > 0.0)
            return polygonRect;
    }

    if (m_roiNormalized.width() <= 0.0 || m_roiNormalized.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    const QRectF roi = m_roiNormalized.normalized().intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (roi.width() <= 0.0 || roi.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    return roi;
}

bool ContourPresenceDialog::isTemplatePolygonMode() const
{
    const bool basicMode = ui->contourParamsStackedWidget->currentWidget() == ui->basicParamsPage;
    return basicMode ? ui->basicTemplatePolygonButton->isChecked()
                     : ui->templatePolygonButton->isChecked();
}

bool ContourPresenceDialog::isDetectPolygonMode() const
{
    const bool basicMode = ui->contourParamsStackedWidget->currentWidget() == ui->basicParamsPage;
    return basicMode ? ui->basicDetectionPolygonButton->isChecked()
                     : ui->detectionPolygonButton->isChecked();
}
