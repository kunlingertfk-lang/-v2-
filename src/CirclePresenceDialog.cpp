#include "CirclePresenceDialog.h"
#include "ui_CirclePresenceDialog.h"

#include <QButtonGroup>
#include <QDebug>
#include <QFontMetrics>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QUuid>

#include <opencv2/imgproc.hpp>

#include "WindowUtils.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolRequest.h"

namespace {

QImage imageFromFrame(const cv::Mat &frame)
{
    if (frame.empty())
        return QImage();

    if (frame.type() == CV_8UC1) {
        QImage image(frame.data,
                     frame.cols,
                     frame.rows,
                     static_cast<int>(frame.step),
                     QImage::Format_Grayscale8);
        return image.copy();
    }

    if (frame.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
        QImage image(rgb.data,
                     rgb.cols,
                     rgb.rows,
                     static_cast<int>(rgb.step),
                     QImage::Format_RGB888);
        return image.copy();
    }

    if (frame.type() == CV_8UC4) {
        cv::Mat rgba;
        cv::cvtColor(frame, rgba, cv::COLOR_BGRA2RGBA);
        QImage image(rgba.data,
                     rgba.cols,
                     rgba.rows,
                     static_cast<int>(rgba.step),
                     QImage::Format_RGBA8888);
        return image.copy();
    }

    return QImage();
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

} // namespace

CirclePresenceDialog::CirclePresenceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CirclePresenceDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_basicResultPresenceGroup(new QButtonGroup(this))
    , m_resultPresenceGroup(new QButtonGroup(this))
{
    ui->setupUi(this);
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

    CirclePresenceConfig config;
    config.detectRegionType = QStringLiteral("rect");
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
    const QString toolId = QStringLiteral("circle_presence_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    QJsonObject params;
    params.insert(QStringLiteral("detectRegionType"), circleConfig.detectRegionType);
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
    config.enabled = true;
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

    connect(ui->basicDetectionRectButton, &QToolButton::clicked, this, &CirclePresenceDialog::startDetectRoiEditing);
    connect(ui->detectionRectButton, &QToolButton::clicked, this, &CirclePresenceDialog::startDetectRoiEditing);
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
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }

    runCirclePresenceOnFrame(referenceImage.clone(),
                             referenceImage.clone(),
                             tr("基准图"),
                             tr("基准图为空，无法测试"));
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
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }

    runCirclePresenceOnFrame(snapshot,
                             ReferenceImageProvider::instance().referenceFrame(),
                             tr("测试图像"),
                             tr("当前图像为空，无法测试"));
}

void CirclePresenceDialog::applyAdaptiveWindowSize()
{
    WindowUtils::applyLargeWindow(this);
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
    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
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
    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
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
    m_previewHelper->setRoiDrawingEnabled(true);
    const QString text = tr("请框选圆检测区域");
    setViewerStatusText(text, text);
}

void CirclePresenceDialog::showDetectRoiTodo(const QString &message)
{
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    setViewerStatusText(message, message);
}

void CirclePresenceDialog::resetDetectRoi()
{
    m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    }
    const QString text = detectRoiStatusText();
    setViewerStatusText(text, text);
}

void CirclePresenceDialog::handleRoiChanged(const QRectF &roi)
{
    m_roiNormalized = roi;
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    const QString roiText = detectRoiStatusText();
    setViewerStatusText(roiText, roiText);
    qDebug() << "[CirclePresenceDialog] ROI normalized:" << m_roiNormalized;
}

void CirclePresenceDialog::handleRoiSelectionRejected()
{
    const QString text = tr("ROI 无效，请拖拽宽高至少 2 像素的矩形");
    setViewerStatusText(text, text);
    if (m_previewHelper)
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void CirclePresenceDialog::runCirclePresenceOnFrame(const cv::Mat &frame,
                                                    const cv::Mat &referenceImage,
                                                    const QString &imageTitle,
                                                    const QString &emptyFrameMessage)
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

    const QString displayText = tr("%1 | count:%2 | %3")
            .arg(result.status,
                 QString::number(result.count),
                 result.ok ? QStringLiteral("OK") : QStringLiteral("NG"));
    setViewerStatusText(displayText, makeCirclePresenceStatusTooltipText(result));

    if (m_previewHelper) {
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
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
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
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
    return tr("检测 ROI x=%1 y=%2 w=%3 h=%4")
            .arg(roi.x(), 0, 'f', 3)
            .arg(roi.y(), 0, 'f', 3)
            .arg(roi.width(), 0, 'f', 3)
            .arg(roi.height(), 0, 'f', 3);
}

QRectF CirclePresenceDialog::effectiveRoiNormalized() const
{
    if (m_roiNormalized.width() <= 0.0 || m_roiNormalized.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    const QRectF roi = m_roiNormalized.normalized().intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (roi.width() <= 0.0 || roi.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    return roi;
}
