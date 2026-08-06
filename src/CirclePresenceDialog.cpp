#include "CirclePresenceDialog.h"
#include "ui_CirclePresenceDialog.h"

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
#include <QToolButton>
#include <QUuid>

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

QString edgePolarityFromText(const QString &text)
{
    const QString key = text.trimmed().toLower();
    if (key == QStringLiteral("black_to_white") || text.contains(QStringLiteral("黑到白")))
        return QStringLiteral("black_to_white");
    if (key == QStringLiteral("white_to_black") || text.contains(QStringLiteral("白到黑")))
        return QStringLiteral("white_to_black");
    return QStringLiteral("any");
}

QString edgeTypeFromText(const QString &text)
{
    const QString key = text.trimmed().toLower();
    if (key == QStringLiteral("maximum") || text.contains(QStringLiteral("最大")))
        return QStringLiteral("maximum");
    if (key == QStringLiteral("minimum") || text.contains(QStringLiteral("最小")))
        return QStringLiteral("minimum");
    if (key == QStringLiteral("manual") || text.contains(QStringLiteral("手动")))
        return QStringLiteral("manual");
    return QStringLiteral("strongest");
}

QString edgePolarityDisplayText(const QString &value)
{
    if (value == QStringLiteral("black_to_white"))
        return QObject::tr("黑到白");
    if (value == QStringLiteral("white_to_black"))
        return QObject::tr("白到黑");
    return QObject::tr("任意");
}

QString edgeTypeDisplayText(const QString &value)
{
    if (value == QStringLiteral("maximum"))
        return QObject::tr("最大");
    if (value == QStringLiteral("minimum"))
        return QObject::tr("最小");
    if (value == QStringLiteral("manual"))
        return QObject::tr("手动选择");
    return QObject::tr("最强");
}

QString makeCirclePresenceStatusTooltipText(const ToolResult &result)
{
    return QStringLiteral("status: %1\nmessage: %2\nscore: %3\ncount: %4\nok: %5")
            .arg(result.status,
                 result.message,
                 QString::number(result.score, 'f', 6),
                 QString::number(result.count),
                 boolDisplayText(result.ok));
}

QString makeCirclePresenceErrorTooltipText(const QString &status, const QString &message)
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

    return QRectF(QPointF(qBound(0.0, left, 1.0), qBound(0.0, top, 1.0)),
                  QPointF(qBound(0.0, right, 1.0), qBound(0.0, bottom, 1.0))).normalized();
}

QJsonObject circleToJson(const CircleRoi &roi)
{
    QJsonObject json;
    json.insert(QStringLiteral("center"), pointToJson(roi.centerNormalized));
    json.insert(QStringLiteral("radius"), roi.radiusNormalized);
    json.insert(QStringLiteral("boundingRect"), rectToJson(roi.boundingRectNormalized));
    json.insert(QStringLiteral("valid"), roi.valid);
    return json;
}

CircleRoi circleFromJson(const QJsonObject &json)
{
    CircleRoi roi;
    if (json.isEmpty())
        return roi;

    const QJsonObject center = json.value(QStringLiteral("center")).toObject();
    roi.centerNormalized = QPointF(center.value(QStringLiteral("x")).toDouble(),
                                   center.value(QStringLiteral("y")).toDouble());
    roi.radiusNormalized = json.value(QStringLiteral("radius")).toDouble();
    roi.boundingRectNormalized = rectFromJson(json.value(QStringLiteral("boundingRect")).toObject(),
                                              QRectF());
    roi.valid = roi.radiusNormalized > 0.0 &&
            roi.boundingRectNormalized.width() > 0.0 &&
            roi.boundingRectNormalized.height() > 0.0;
    return roi;
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

    button->setText(text);
    button->setToolTip(tooltip);
    button->setMinimumSize(QSize(52, 38));
    button->setMaximumSize(QSize(52, 38));
    button->setProperty("actionRole", QStringLiteral("toolbarIcon"));
}

} // namespace

CirclePresenceDialog::CirclePresenceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CirclePresenceDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_basicDetectionRegionGroup(new QButtonGroup(this))
    , m_detectionRegionGroup(new QButtonGroup(this))
    , m_basicResultPresenceGroup(new QButtonGroup(this))
    , m_resultPresenceGroup(new QButtonGroup(this))
{
    ui->setupUi(this);
    m_toolId = QStringLiteral("circle_presence_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_testToolEngine.registerAdapter(&m_testCirclePresenceAdapter);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    connectControls();
    showReferenceImage();
}

CirclePresenceDialog::~CirclePresenceDialog()
{
    delete ui;
}

CirclePresenceConfig CirclePresenceDialog::configuration() const
{
    const bool basicMode = ui->circleParamsStackedWidget->currentWidget() == ui->basicParamsPage;
    const bool polygonMode = basicMode
            ? ui->basicDetectionPolygonButton->isChecked()
            : ui->detectionPolygonButton->isChecked();
    const bool circleMode = basicMode
            ? ui->basicDetectionCircleButton->isChecked()
            : ui->detectionCircleButton->isChecked();

    CirclePresenceConfig config;
    config.detectRegionType = polygonMode
            ? QStringLiteral("polygon")
            : (circleMode ? QStringLiteral("circle") : QStringLiteral("rect"));
    config.detectPolygonNormalized = m_detectPolygonNormalized;
    config.detectCircleNormalized = m_detectCircleNormalized;
    config.enablePositionCorrection = basicMode
            ? ui->basicPositionCorrectionSwitch->isChecked()
            : ui->positionCorrectionSwitch->isChecked();
    config.positionCorrectionSource = basicMode
            ? ui->basicPositionCorrectionComboBox->currentText()
            : ui->positionCorrectionComboBox->currentText();
    config.sensitivity = basicMode
            ? ui->circleBasicSensitivitySpinBox->value()
            : ui->circleSensitivitySpinBox->value();
    config.roundness = basicMode ? 25 : ui->roundnessSpinBox->value();
    config.edgePolarity = basicMode
            ? QStringLiteral("any")
            : edgePolarityFromText(ui->edgePolarityComboBox->currentText());
    config.edgeType = basicMode
            ? QStringLiteral("strongest")
            : edgeTypeFromText(ui->edgeTypeComboBox->currentText());
    config.judgeBasis = QStringLiteral("presence");
    config.existOk = basicMode ? ui->basicPresentOkButton->isChecked()
                               : ui->presentOkButton->isChecked();
    config.timeoutMs = 1000;
    return config;
}

ToolConfig CirclePresenceDialog::toToolConfig() const
{
    const CirclePresenceConfig circleConfig = configuration();
    const QString toolId = m_toolId;

    QJsonObject params;
    params.insert(QStringLiteral("detectRegionType"), circleConfig.detectRegionType);
    params.insert(QStringLiteral("detectPolygonNormalized"),
                  pointsToJson(circleConfig.detectPolygonNormalized));
    params.insert(QStringLiteral("detectCircleNormalized"),
                  circleToJson(circleConfig.detectCircleNormalized));
    params.insert(QStringLiteral("enablePositionCorrection"), circleConfig.enablePositionCorrection);
    params.insert(QStringLiteral("positionCorrectionSource"), circleConfig.positionCorrectionSource);
    params.insert(QStringLiteral("sensitivity"), circleConfig.sensitivity);
    params.insert(QStringLiteral("roundness"), circleConfig.roundness);
    params.insert(QStringLiteral("edgePolarity"), circleConfig.edgePolarity);
    params.insert(QStringLiteral("edgeType"), circleConfig.edgeType);
    params.insert(QStringLiteral("judgeBasis"), circleConfig.judgeBasis);
    params.insert(QStringLiteral("existOk"), circleConfig.existOk);
    params.insert(QStringLiteral("timeoutMs"), circleConfig.timeoutMs);

    QJsonObject judgeRule;
    judgeRule.insert(QStringLiteral("mode"), circleConfig.judgeBasis);
    judgeRule.insert(QStringLiteral("existOk"), circleConfig.existOk);

    ToolConfig config;
    config.toolId = toolId;
    config.toolName = tr("圆有无");
    config.toolType = ToolType::CirclePresence;
    config.category = ToolCategory::Presence;
    config.enabled = m_enabled;
    config.roiNormalized = effectiveRoiNormalized();
    config.params = params;
    config.judgeRule = judgeRule;
    config.displayName = tr("圆有无");
    config.summary = summaryText();
    return config;
}

ToolConfig CirclePresenceDialog::toolConfig() const
{
    return toToolConfig();
}

ToolPreviewSnapshot CirclePresenceDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void CirclePresenceDialog::loadFromConfig(const ToolConfig &config)
{
    if (!config.toolId.trimmed().isEmpty())
        m_toolId = config.toolId;
    m_enabled = config.enabled;
    if (config.roiNormalized.width() > 0.0 && config.roiNormalized.height() > 0.0)
        m_roiNormalized = config.roiNormalized;

    const QJsonObject params = config.params;
    const bool allMode = params.contains(QStringLiteral("roundness"))
            || params.value(QStringLiteral("edgePolarity")).toString() != QStringLiteral("any")
            || params.value(QStringLiteral("edgeType")).toString() != QStringLiteral("strongest");
    ui->basicSegmentButton->setChecked(!allMode);
    ui->allSegmentButton->setChecked(allMode);
    ui->circleParamsStackedWidget->setCurrentWidget(allMode ? ui->allParamsPage : ui->basicParamsPage);

    const bool positionCorrection = params.value(QStringLiteral("enablePositionCorrection")).toBool(ui->basicPositionCorrectionSwitch->isChecked());
    ui->basicPositionCorrectionSwitch->setChecked(positionCorrection);
    ui->positionCorrectionSwitch->setChecked(positionCorrection);
    setComboBoxValue(ui->basicPositionCorrectionComboBox, params.value(QStringLiteral("positionCorrectionSource")).toString());
    setComboBoxValue(ui->positionCorrectionComboBox, params.value(QStringLiteral("positionCorrectionSource")).toString());
    const int sensitivity = params.value(QStringLiteral("sensitivity")).toInt(ui->circleBasicSensitivitySpinBox->value());
    ui->circleBasicSensitivitySpinBox->setValue(sensitivity);
    ui->circleSensitivitySpinBox->setValue(sensitivity);
    ui->roundnessSpinBox->setValue(params.value(QStringLiteral("roundness")).toInt(ui->roundnessSpinBox->value()));
    setComboBoxValue(ui->edgePolarityComboBox, edgePolarityDisplayText(params.value(QStringLiteral("edgePolarity")).toString()));
    setComboBoxValue(ui->edgeTypeComboBox, edgeTypeDisplayText(params.value(QStringLiteral("edgeType")).toString()));

    m_detectPolygonNormalized = pointsFromJson(params.value(QStringLiteral("detectPolygonNormalized")).toArray());
    m_detectCircleNormalized = circleFromJson(params.value(QStringLiteral("detectCircleNormalized")).toObject());
    const QString detectRegionType = params.value(QStringLiteral("detectRegionType")).toString().trimmed().toLower();
    const bool polygonMode = detectRegionType == QStringLiteral("polygon") &&
            m_detectPolygonNormalized.size() >= 3;
    const bool circleMode = detectRegionType == QStringLiteral("circle") &&
            m_detectCircleNormalized.valid;
    if (polygonMode)
        m_roiNormalized = boundingRectForPoints(m_detectPolygonNormalized);
    else if (circleMode)
        m_roiNormalized = m_detectCircleNormalized.boundingRectNormalized;

    ui->basicDetectionRectButton->setChecked(!polygonMode && !circleMode);
    ui->detectionRectButton->setChecked(!polygonMode && !circleMode);
    ui->basicDetectionPolygonButton->setChecked(polygonMode);
    ui->detectionPolygonButton->setChecked(polygonMode);
    ui->basicDetectionCircleButton->setChecked(circleMode);
    ui->detectionCircleButton->setChecked(circleMode);

    const bool existOk = params.value(QStringLiteral("existOk")).toBool(true);
    ui->basicPresentOkButton->setChecked(existOk);
    ui->basicAbsentOkButton->setChecked(!existOk);
    ui->presentOkButton->setChecked(existOk);
    ui->absentOkButton->setChecked(!existOk);

    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    refreshDisplayedRoiOverlay();
    const QString roiText = detectRoiStatusText();
    setViewerStatusText(roiText, roiText);
}

QString CirclePresenceDialog::summaryText() const
{
    const CirclePresenceConfig config = configuration();
    return tr("灵敏度: %1, 圆度: %2, 边缘: %3/%4, %5")
            .arg(config.sensitivity)
            .arg(config.roundness)
            .arg(edgePolarityDisplayText(config.edgePolarity),
                 edgeTypeDisplayText(config.edgeType),
                 config.existOk ? tr("存在OK") : tr("不存在OK"));
}

void CirclePresenceDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void CirclePresenceDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 圆有无"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    applyAdaptiveWindowSize();

    ui->circleParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
    ui->basicSegmentButton->setChecked(true);
    ui->allSegmentButton->setChecked(false);

    ui->circleBasicSensitivitySpinBox->setRange(0, 100);
    ui->circleSensitivitySpinBox->setRange(0, 100);
    ui->roundnessSpinBox->setRange(0, 100);
    ui->circleBasicSensitivitySpinBox->setValue(60);
    ui->circleSensitivitySpinBox->setValue(60);
    ui->roundnessSpinBox->setValue(25);
    ui->edgePolarityComboBox->setCurrentIndex(2);
    ui->edgeTypeComboBox->setCurrentIndex(0);
    ui->basicPositionCorrectionSwitch->setChecked(false);
    ui->positionCorrectionSwitch->setChecked(false);
    ui->basicPresentOkButton->setChecked(true);
    ui->presentOkButton->setChecked(true);
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    ui->basicDetectionCircleButton->setChecked(false);
    ui->detectionCircleButton->setChecked(false);
    ui->basicDetectionPolygonButton->setChecked(false);
    ui->detectionPolygonButton->setChecked(false);
    configureRoiToolButton(ui->basicDetectionRectButton, QStringLiteral("□"), tr("矩形检测 ROI"));
    configureRoiToolButton(ui->detectionRectButton, QStringLiteral("□"), tr("矩形检测 ROI"));
    configureRoiToolButton(ui->basicDetectionCircleButton, QStringLiteral("○"), tr("圆形检测 ROI"));
    configureRoiToolButton(ui->detectionCircleButton, QStringLiteral("○"), tr("圆形检测 ROI"));
    configureRoiToolButton(ui->basicDetectionPolygonButton, QStringLiteral("⬡"), tr("多边形检测 ROI"));
    configureRoiToolButton(ui->detectionPolygonButton, QStringLiteral("⬡"), tr("多边形检测 ROI"));
    configureRoiToolButton(ui->basicDetectionResetButton, QStringLiteral("⟳"), tr("恢复全图检测"));
    configureRoiToolButton(ui->detectionResetButton, QStringLiteral("⟳"), tr("恢复全图检测"));

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
    const QString roiText = detectRoiStatusText();
    setViewerStatusText(roiText, roiText);
}

void CirclePresenceDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &CirclePresenceDialog::reject);
    connect(ui->referenceTestButton, &QPushButton::clicked, this, &CirclePresenceDialog::runReferenceTest);
    connect(ui->testRunButton, &QPushButton::clicked, this, &CirclePresenceDialog::runCameraTest);
    connect(ui->finishButton, &QPushButton::clicked, this, &CirclePresenceDialog::finishConfiguration);

    m_segmentGroup->setExclusive(true);
    m_segmentGroup->addButton(ui->basicSegmentButton, 0);
    m_segmentGroup->addButton(ui->allSegmentButton, 1);
    connect(ui->basicSegmentButton, &QPushButton::clicked, this, [this]() {
        ui->circleParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
    });
    connect(ui->allSegmentButton, &QPushButton::clicked, this, [this]() {
        ui->circleParamsStackedWidget->setCurrentWidget(ui->allParamsPage);
    });

    m_basicDetectionRegionGroup->setExclusive(true);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionRectButton, 0);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionCircleButton, 1);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionPolygonButton, 2);
    m_detectionRegionGroup->setExclusive(true);
    m_detectionRegionGroup->addButton(ui->detectionRectButton, 0);
    m_detectionRegionGroup->addButton(ui->detectionCircleButton, 1);
    m_detectionRegionGroup->addButton(ui->detectionPolygonButton, 2);

    connect(ui->basicDetectionRectButton, &QToolButton::clicked, this, &CirclePresenceDialog::startDetectRoiEditing);
    connect(ui->detectionRectButton, &QToolButton::clicked, this, &CirclePresenceDialog::startDetectRoiEditing);
    connect(ui->basicDetectionCircleButton, &QToolButton::clicked, this, &CirclePresenceDialog::startDetectCircleEditing);
    connect(ui->detectionCircleButton, &QToolButton::clicked, this, &CirclePresenceDialog::startDetectCircleEditing);
    connect(ui->basicDetectionPolygonButton, &QToolButton::clicked, this, &CirclePresenceDialog::startDetectPolygonEditing);
    connect(ui->detectionPolygonButton, &QToolButton::clicked, this, &CirclePresenceDialog::startDetectPolygonEditing);
    connect(ui->basicDetectionResetButton, &QToolButton::clicked, this, &CirclePresenceDialog::resetDetectRoi);
    connect(ui->detectionResetButton, &QToolButton::clicked, this, &CirclePresenceDialog::resetDetectRoi);
    connect(ui->basicDetectionMaskEditButton, &QPushButton::clicked, this, [this]() {
        showDetectRoiTodo(tr("屏蔽区域暂未实现"));
    });
    connect(ui->detectionMaskEditButton, &QPushButton::clicked, this, [this]() {
        showDetectRoiTodo(tr("屏蔽区域暂未实现"));
    });

    m_basicResultPresenceGroup->setExclusive(true);
    m_basicResultPresenceGroup->addButton(ui->basicPresentOkButton, 0);
    m_basicResultPresenceGroup->addButton(ui->basicAbsentOkButton, 1);
    m_resultPresenceGroup->setExclusive(true);
    m_resultPresenceGroup->addButton(ui->presentOkButton, 0);
    m_resultPresenceGroup->addButton(ui->absentOkButton, 1);

    if (m_previewHelper) {
        connect(m_previewHelper,
                &FrameViewHelper::roiChanged,
                this,
                &CirclePresenceDialog::handleRoiChanged);
        connect(m_previewHelper,
                &FrameViewHelper::roiSelectionRejected,
                this,
                [this](const QRectF &) {
                    handleRoiSelectionRejected();
                });
        connect(m_previewHelper,
                &FrameViewHelper::polygonChanged,
                this,
                &CirclePresenceDialog::handlePolygonChanged);
        connect(m_previewHelper,
                &FrameViewHelper::polygonSelectionRejected,
                this,
                &CirclePresenceDialog::handlePolygonSelectionRejected);
        connect(m_previewHelper,
                &FrameViewHelper::circleChanged,
                this,
                &CirclePresenceDialog::handleCircleChanged);
        connect(m_previewHelper,
                &FrameViewHelper::circleSelectionRejected,
                this,
                &CirclePresenceDialog::handleCircleSelectionRejected);
    }

    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) {
                showReferenceImage();
            });
}

void CirclePresenceDialog::finishConfiguration()
{
    if (isDetectPolygonMode() &&
        m_previewHelper &&
        m_previewHelper->isPolygonDrawingEnabled() &&
        !m_previewHelper->finishPolygonDrawing()) {
        handlePolygonSelectionRejected(m_detectPolygonNormalized.size());
        return;
    }

    if (isDetectPolygonMode() && m_detectPolygonNormalized.size() < 3) {
        handlePolygonSelectionRejected(m_detectPolygonNormalized.size());
        return;
    }
    if (isDetectCircleMode() && !m_detectCircleNormalized.valid) {
        handleCircleSelectionRejected();
        return;
    }

    accept();
}

void CirclePresenceDialog::runReferenceTest()
{
    const cv::Mat referenceImage = ReferenceImageProvider::instance().referenceFrame();
    if (referenceImage.empty()) {
        displayCirclePresenceError(QStringLiteral("no_reference_image"),
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

    runCirclePresenceOnFrame(referenceImage.clone(),
                             referenceImage.clone(),
                             tr("基准图"),
                             tr("基准图为空，无法测试"),
                             true);
}

void CirclePresenceDialog::runCameraTest()
{
    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        displayCirclePresenceError(QStringLiteral("image_empty"),
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

    runCirclePresenceOnFrame(snapshot,
                             ReferenceImageProvider::instance().referenceFrame(),
                             tr("测试图像"),
                             tr("当前图像为空，无法测试"));
}

void CirclePresenceDialog::applyAdaptiveWindowSize()
{
    PlanDialogUtils::applyLargeWindow(this);
}

void CirclePresenceDialog::fitPreview()
{
    if (m_previewHelper)
        m_previewHelper->fitToView();
}

void CirclePresenceDialog::showReferenceImage()
{
    if (!m_previewHelper)
        return;

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        return;
    }

    ui->viewerTitleLabel->setText(tr("基准图"));
    m_previewHelper->setImage(image);
    refreshDisplayedRoiOverlay();
}

void CirclePresenceDialog::showFrameForRoiEditing()
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

void CirclePresenceDialog::startDetectRoiEditing()
{
    if (!m_previewHelper)
        return;

    showFrameForRoiEditing();
    if (m_previewHelper->imageSize().isEmpty())
        return;

    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    ui->basicDetectionPolygonButton->setChecked(false);
    ui->detectionPolygonButton->setChecked(false);
    ui->basicDetectionCircleButton->setChecked(false);
    ui->detectionCircleButton->setChecked(false);
    m_detectPolygonNormalized.clear();
    m_detectCircleNormalized = CircleRoi();
    m_previewHelper->clearPolygonRoi();
    m_previewHelper->clearCircleRoi();
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    m_previewHelper->setRoiDrawingEnabled(true);
    const QString text = tr("请框选圆检测区域");
    setViewerStatusText(text, text);
}

void CirclePresenceDialog::startDetectPolygonEditing()
{
    if (!m_previewHelper)
        return;

    showFrameForRoiEditing();
    if (m_previewHelper->imageSize().isEmpty())
        return;

    ui->basicDetectionRectButton->setChecked(false);
    ui->detectionRectButton->setChecked(false);
    ui->basicDetectionCircleButton->setChecked(false);
    ui->detectionCircleButton->setChecked(false);
    ui->basicDetectionPolygonButton->setChecked(true);
    ui->detectionPolygonButton->setChecked(true);
    m_detectCircleNormalized = CircleRoi();
    m_previewHelper->clearToolOverlays();
    m_previewHelper->clearRoi();
    m_previewHelper->clearCircleRoi();
    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    if (m_detectPolygonNormalized.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(m_detectPolygonNormalized);
    else
        m_previewHelper->clearPolygonRoi();
    m_previewHelper->setPolygonDrawingEnabled(true);

    const QString text = tr("当前编辑：多边形 ROI。左键添加点，靠近首点点击自动闭合，右键撤销，Esc 取消。");
    setViewerStatusText(text, text);
}

void CirclePresenceDialog::startDetectCircleEditing()
{
    if (!m_previewHelper)
        return;

    showFrameForRoiEditing();
    if (m_previewHelper->imageSize().isEmpty())
        return;

    ui->basicDetectionRectButton->setChecked(false);
    ui->detectionRectButton->setChecked(false);
    ui->basicDetectionPolygonButton->setChecked(false);
    ui->detectionPolygonButton->setChecked(false);
    ui->basicDetectionCircleButton->setChecked(true);
    ui->detectionCircleButton->setChecked(true);
    m_detectPolygonNormalized.clear();
    m_previewHelper->clearToolOverlays();
    m_previewHelper->clearRoi();
    m_previewHelper->clearPolygonRoi();
    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    if (m_detectCircleNormalized.valid)
        m_previewHelper->setCircleRoiNormalized(m_detectCircleNormalized);
    else
        m_previewHelper->clearCircleRoi();
    m_previewHelper->setCircleDrawingEnabled(true);

    const QString text = tr("当前编辑：圆形 ROI。按住左键从圆心拖拽半径，Esc 取消。");
    setViewerStatusText(text, text);
}

void CirclePresenceDialog::showDetectRoiTodo(const QString &message)
{
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setPolygonDrawingEnabled(false);
        m_previewHelper->setCircleDrawingEnabled(false);
        refreshDisplayedRoiOverlay();
    }
    setViewerStatusText(message, message);
}

void CirclePresenceDialog::resetDetectRoi()
{
    m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    m_detectPolygonNormalized.clear();
    m_detectCircleNormalized = CircleRoi();
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    ui->basicDetectionPolygonButton->setChecked(false);
    ui->detectionPolygonButton->setChecked(false);
    ui->basicDetectionCircleButton->setChecked(false);
    ui->detectionCircleButton->setChecked(false);
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setPolygonDrawingEnabled(false);
        m_previewHelper->setCircleDrawingEnabled(false);
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->clearCircleRoi();
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    }
    const QString text = detectRoiStatusText();
    setViewerStatusText(text, text);
}

void CirclePresenceDialog::handleRoiChanged(const QRectF &roi)
{
    m_roiNormalized = roi;
    m_detectPolygonNormalized.clear();
    m_detectCircleNormalized = CircleRoi();
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    ui->basicDetectionPolygonButton->setChecked(false);
    ui->detectionPolygonButton->setChecked(false);
    ui->basicDetectionCircleButton->setChecked(false);
    ui->detectionCircleButton->setChecked(false);
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->clearCircleRoi();
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    const QString roiText = detectRoiStatusText();
    setViewerStatusText(roiText, roiText);
    qDebug() << "[CirclePresenceDialog] ROI normalized:" << m_roiNormalized;
}

void CirclePresenceDialog::handlePolygonChanged(const QVector<QPointF> &points)
{
    if (points.size() < 3)
        return;

    m_detectPolygonNormalized = points;
    m_detectCircleNormalized = CircleRoi();
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
        m_previewHelper->clearCircleRoi();
        m_previewHelper->setPolygonRoiNormalized(m_detectPolygonNormalized);
    }

    const QString text = detectRoiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[CirclePresenceDialog] Polygon ROI points:" << m_detectPolygonNormalized.size()
             << "bounding:" << m_roiNormalized;
}

void CirclePresenceDialog::handleCircleChanged(const CircleRoi &roi)
{
    if (!roi.valid)
        return;

    m_detectCircleNormalized = roi;
    m_detectPolygonNormalized.clear();
    m_roiNormalized = roi.boundingRectNormalized;
    ui->basicDetectionRectButton->setChecked(false);
    ui->detectionRectButton->setChecked(false);
    ui->basicDetectionPolygonButton->setChecked(false);
    ui->detectionPolygonButton->setChecked(false);
    ui->basicDetectionCircleButton->setChecked(true);
    ui->detectionCircleButton->setChecked(true);
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearRoi();
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->setCircleRoiNormalized(m_detectCircleNormalized);
    }

    const QString text = detectRoiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[CirclePresenceDialog] Circle ROI center:" << m_detectCircleNormalized.centerNormalized
             << "radius:" << m_detectCircleNormalized.radiusNormalized
             << "bounding:" << m_roiNormalized;
}

void CirclePresenceDialog::handlePolygonSelectionRejected(int pointCount)
{
    Q_UNUSED(pointCount)
    const QString text = tr("多边形至少需要 3 个点。");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void CirclePresenceDialog::handleCircleSelectionRejected()
{
    const QString text = tr("圆形 ROI 无效，请拖拽出半径至少 2 像素的圆。");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void CirclePresenceDialog::handleRoiSelectionRejected()
{
    const QString text = tr("ROI 无效，请拖拽宽高至少 2 像素的矩形");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void CirclePresenceDialog::refreshDisplayedRoiOverlay()
{
    if (!m_previewHelper)
        return;

    m_previewHelper->clearPolygonRoi();
    m_previewHelper->clearCircleRoi();
    m_previewHelper->clearRoi();

    if (isDetectPolygonMode() && m_detectPolygonNormalized.size() >= 3) {
        m_previewHelper->setPolygonRoiNormalized(m_detectPolygonNormalized);
        return;
    }

    if (isDetectCircleMode() && m_detectCircleNormalized.valid) {
        m_previewHelper->setCircleRoiNormalized(m_detectCircleNormalized);
        return;
    }

    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void CirclePresenceDialog::runCirclePresenceOnFrame(const cv::Mat &frame,
                                                    const cv::Mat &referenceImage,
                                                    const QString &imageTitle,
                                                    const QString &emptyFrameMessage,
                                                    bool referenceTest)
{
    if (m_circlePresenceRunning)
        return;

    if (frame.empty()) {
        displayCirclePresenceError(QStringLiteral("image_empty"), emptyFrameMessage);
        return;
    }

    m_circlePresenceRunning = true;

    ToolConfig config = toToolConfig();
    config.roiNormalized = effectiveRoiNormalized();

    ToolRequest request;
    request.config = config;
    request.image = frame.clone();
    request.referenceImage = referenceImage.empty() ? cv::Mat() : referenceImage.clone();

    const ToolResult result = m_testToolEngine.runTool(request);
    if (!imageTitle.isEmpty())
        ui->viewerTitleLabel->setText(imageTitle);
    displayCirclePresenceResult(result);
    if (referenceTest)
        m_referencePreviewSnapshot = makeReferenceToolPreviewSnapshot(config, result, effectiveRoiNormalized());

    m_circlePresenceRunning = false;
}

void CirclePresenceDialog::displayCirclePresenceResult(const ToolResult &result)
{
    qDebug() << "[CirclePresenceDialog] ToolResult"
             << "status=" << result.status
             << "message=" << result.message
             << "score=" << result.score
             << "count=" << result.count
             << "ok=" << result.ok;

    const QString displayText = result.success
            ? tr("%1 | count:%2 | %3")
              .arg(result.status,
                   QString::number(result.count),
                   result.ok ? QStringLiteral("OK") : QStringLiteral("NG"))
            : tr("%1 | %2").arg(result.status, result.message);
    setViewerStatusText(displayText, makeCirclePresenceStatusTooltipText(result));

    if (m_previewHelper) {
        refreshDisplayedRoiOverlay();
        m_previewHelper->setToolOverlays(result.overlays);
    }
}

void CirclePresenceDialog::displayCirclePresenceError(const QString &status, const QString &message)
{
    qDebug() << "[CirclePresenceDialog] ToolResult"
             << "status=" << status
             << "message=" << message
             << "score=" << 0.0
             << "count=" << 0
             << "ok=" << false;

    const QString displayText = tr("CirclePresence: %1 | %2").arg(status, message);
    setViewerStatusText(displayText, makeCirclePresenceErrorTooltipText(status, message));
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        refreshDisplayedRoiOverlay();
    }
}

void CirclePresenceDialog::setViewerStatusText(const QString &displayText, const QString &tooltipText)
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

QString CirclePresenceDialog::detectRoiStatusText() const
{
    const QRectF roi = effectiveRoiNormalized();
    const QString shape = isDetectPolygonMode()
            ? tr("多边形")
            : (isDetectCircleMode() ? tr("圆形") : tr("矩形"));
    return tr("%1检测 ROI x=%2 y=%3 w=%4 h=%5")
            .arg(shape)
            .arg(roi.x(), 0, 'f', 3)
            .arg(roi.y(), 0, 'f', 3)
            .arg(roi.width(), 0, 'f', 3)
            .arg(roi.height(), 0, 'f', 3);
}

QRectF CirclePresenceDialog::effectiveRoiNormalized() const
{
    if (isDetectPolygonMode() && m_detectPolygonNormalized.size() >= 3) {
        const QRectF polygonRect = boundingRectForPoints(m_detectPolygonNormalized);
        if (polygonRect.width() > 0.0 && polygonRect.height() > 0.0)
            return polygonRect;
    }

    if (isDetectCircleMode() && m_detectCircleNormalized.valid &&
        m_detectCircleNormalized.boundingRectNormalized.width() > 0.0 &&
        m_detectCircleNormalized.boundingRectNormalized.height() > 0.0) {
        return m_detectCircleNormalized.boundingRectNormalized.normalized()
                .intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    }

    if (m_roiNormalized.width() <= 0.0 || m_roiNormalized.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    const QRectF roi = m_roiNormalized.normalized().intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (roi.width() <= 0.0 || roi.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    return roi;
}

bool CirclePresenceDialog::isDetectPolygonMode() const
{
    const bool basicMode = ui->circleParamsStackedWidget->currentWidget() == ui->basicParamsPage;
    return basicMode ? ui->basicDetectionPolygonButton->isChecked()
                     : ui->detectionPolygonButton->isChecked();
}

bool CirclePresenceDialog::isDetectCircleMode() const
{
    const bool basicMode = ui->circleParamsStackedWidget->currentWidget() == ui->basicParamsPage;
    return basicMode ? ui->basicDetectionCircleButton->isChecked()
                     : ui->detectionCircleButton->isChecked();
}
