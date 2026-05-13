#include "BlobPresenceDialog.h"
#include "ui_BlobPresenceDialog.h"

#include <QButtonGroup>
#include <QDebug>
#include <QFontMetrics>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QUuid>
#include <QtGlobal>

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

QString makeBlobPresenceStatusTooltipText(const ToolResult &result)
{
    return QStringLiteral("status: %1\nmessage: %2\nscore: %3\ncount: %4\nok: %5")
            .arg(result.status,
                 result.message,
                 QString::number(result.score, 'f', 6),
                 QString::number(result.count),
                 boolDisplayText(result.ok));
}

QString makeBlobPresenceErrorTooltipText(const QString &status, const QString &message)
{
    return QStringLiteral("status: %1\nmessage: %2\nscore: 0.000000\ncount: 0\nok: false")
            .arg(status, message);
}

} // namespace

BlobPresenceDialog::BlobPresenceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BlobPresenceDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_basicDetectionRegionGroup(new QButtonGroup(this))
    , m_detectionRegionGroup(new QButtonGroup(this))
    , m_basicResultPresenceGroup(new QButtonGroup(this))
    , m_resultPresenceGroup(new QButtonGroup(this))
{
    ui->setupUi(this);
    m_testToolEngine.registerAdapter(&m_testBlobPresenceAdapter);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    connectControls();
    showReferenceImage();
}

BlobPresenceDialog::~BlobPresenceDialog()
{
    delete ui;
}

BlobPresenceConfig BlobPresenceDialog::configuration() const
{
    const bool basicMode = ui->spotParamsStackedWidget->currentWidget() == ui->basicParamsPage;

    BlobPresenceConfig config;
    config.detectRegionType = QStringLiteral("rect");
    config.enablePositionCorrection = basicMode
            ? ui->basicPositionCorrectionSwitch->isChecked()
            : ui->positionCorrectionSwitch->isChecked();
    config.positionCorrectionSource = basicMode
            ? ui->basicPositionCorrectionComboBox->currentText()
            : ui->positionCorrectionComboBox->currentText();
    config.grayMin = basicMode ? ui->basicMinGraySpinBox->value() : ui->minGraySpinBox->value();
    config.grayMax = basicMode ? ui->basicMaxGraySpinBox->value() : ui->maxGraySpinBox->value();
    config.invertRange = basicMode ? false : ui->invertRangeSwitch->isChecked();
    config.areaMin = basicMode ? 10 : ui->minAreaSpinBox->value();
    config.areaMax = basicMode ? 999999 : ui->maxAreaSpinBox->value();
    config.maskOutputEnabled = basicMode ? false : ui->maskOutputSwitch->isChecked();
    config.judgeBasis = QStringLiteral("presence");
    config.existOk = basicMode ? ui->basicPresentOkButton->isChecked()
                               : ui->presentOkButton->isChecked();
    config.timeoutMs = 1000;
    return config;
}

ToolConfig BlobPresenceDialog::toToolConfig() const
{
    const BlobPresenceConfig blobConfig = configuration();
    const QString toolId = QStringLiteral("blob_presence_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    QJsonObject params;
    params.insert(QStringLiteral("detectRegionType"), blobConfig.detectRegionType);
    params.insert(QStringLiteral("enablePositionCorrection"), blobConfig.enablePositionCorrection);
    params.insert(QStringLiteral("positionCorrectionSource"), blobConfig.positionCorrectionSource);
    params.insert(QStringLiteral("grayMin"), blobConfig.grayMin);
    params.insert(QStringLiteral("grayMax"), blobConfig.grayMax);
    params.insert(QStringLiteral("invertRange"), blobConfig.invertRange);
    params.insert(QStringLiteral("areaMin"), blobConfig.areaMin);
    params.insert(QStringLiteral("areaMax"), blobConfig.areaMax);
    params.insert(QStringLiteral("maskOutputEnabled"), blobConfig.maskOutputEnabled);
    params.insert(QStringLiteral("judgeBasis"), blobConfig.judgeBasis);
    params.insert(QStringLiteral("existOk"), blobConfig.existOk);
    params.insert(QStringLiteral("timeoutMs"), blobConfig.timeoutMs);

    QJsonObject judgeRule;
    judgeRule.insert(QStringLiteral("mode"), blobConfig.judgeBasis);
    judgeRule.insert(QStringLiteral("existOk"), blobConfig.existOk);

    ToolConfig config;
    config.toolId = toolId;
    config.toolName = tr("斑点有无");
    config.toolType = ToolType::BlobPresence;
    config.category = ToolCategory::Presence;
    config.enabled = true;
    config.roiNormalized = effectiveRoiNormalized();
    config.params = params;
    config.judgeRule = judgeRule;
    config.displayName = tr("斑点有无");
    config.summary = summaryText();
    return config;
}

ToolConfig BlobPresenceDialog::toolConfig() const
{
    return toToolConfig();
}

QString BlobPresenceDialog::summaryText() const
{
    const BlobPresenceConfig config = configuration();
    return tr("灰度: %1-%2, 面积: %3-%4, %5")
            .arg(config.grayMin)
            .arg(config.grayMax)
            .arg(config.areaMin)
            .arg(config.areaMax)
            .arg(config.existOk ? tr("存在OK") : tr("不存在OK"));
}

void BlobPresenceDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void BlobPresenceDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 斑点有无"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    applyAdaptiveWindowSize();

    ui->spotParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
    ui->basicSegmentButton->setChecked(true);
    ui->allSegmentButton->setChecked(false);

    ui->basicMinGraySpinBox->setMinimum(0);
    ui->minGraySpinBox->setMinimum(0);
    ui->basicMinGraySpinBox->setValue(0);
    ui->basicMaxGraySpinBox->setValue(255);
    ui->minGraySpinBox->setValue(0);
    ui->maxGraySpinBox->setValue(255);
    ui->minAreaSpinBox->setValue(10);
    ui->maxAreaSpinBox->setValue(999999);
    ui->invertRangeSwitch->setChecked(false);
    ui->maskOutputSwitch->setChecked(false);

    ui->basicDetectionDrawButton->setChecked(false);
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionDrawButton->setChecked(false);
    ui->detectionRectButton->setChecked(true);

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

void BlobPresenceDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &BlobPresenceDialog::reject);
    connect(ui->referenceTestButton, &QPushButton::clicked, this, &BlobPresenceDialog::runReferenceTest);
    connect(ui->testRunButton, &QPushButton::clicked, this, &BlobPresenceDialog::runCameraTest);
    connect(ui->finishButton, &QPushButton::clicked, this, &BlobPresenceDialog::finishConfiguration);

    m_segmentGroup->setExclusive(true);
    m_segmentGroup->addButton(ui->basicSegmentButton, 0);
    m_segmentGroup->addButton(ui->allSegmentButton, 1);
    connect(ui->basicSegmentButton, &QPushButton::clicked, this, [this]() {
        ui->spotParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
    });
    connect(ui->allSegmentButton, &QPushButton::clicked, this, [this]() {
        ui->spotParamsStackedWidget->setCurrentWidget(ui->allParamsPage);
    });

    m_basicDetectionRegionGroup->setExclusive(true);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionDrawButton, 0);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionRectButton, 1);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionCircleButton, 2);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionPolygonButton, 3);
    m_basicDetectionRegionGroup->addButton(ui->basicDetectionResetButton, 4);

    m_detectionRegionGroup->setExclusive(true);
    m_detectionRegionGroup->addButton(ui->detectionDrawButton, 0);
    m_detectionRegionGroup->addButton(ui->detectionRectButton, 1);
    m_detectionRegionGroup->addButton(ui->detectionCircleButton, 2);
    m_detectionRegionGroup->addButton(ui->detectionPolygonButton, 3);
    m_detectionRegionGroup->addButton(ui->detectionResetButton, 4);

    connect(ui->basicDetectionRectButton, &QToolButton::clicked, this, &BlobPresenceDialog::startDetectRoiEditing);
    connect(ui->detectionRectButton, &QToolButton::clicked, this, &BlobPresenceDialog::startDetectRoiEditing);
    connect(ui->basicDetectionDrawButton, &QToolButton::clicked, this, [this]() {
        showDetectRoiTodo(tr("自由绘制 ROI 暂未实现，请使用矩形检测区域"));
    });
    connect(ui->detectionDrawButton, &QToolButton::clicked, this, [this]() {
        showDetectRoiTodo(tr("自由绘制 ROI 暂未实现，请使用矩形检测区域"));
    });
    connect(ui->basicDetectionCircleButton, &QToolButton::clicked, this, [this]() {
        showDetectRoiTodo(tr("圆形 ROI 暂未实现，请使用矩形检测区域"));
    });
    connect(ui->detectionCircleButton, &QToolButton::clicked, this, [this]() {
        showDetectRoiTodo(tr("圆形 ROI 暂未实现，请使用矩形检测区域"));
    });
    connect(ui->basicDetectionPolygonButton, &QToolButton::clicked, this, [this]() {
        showDetectRoiTodo(tr("多边形 ROI 暂未实现，请使用矩形检测区域"));
    });
    connect(ui->detectionPolygonButton, &QToolButton::clicked, this, [this]() {
        showDetectRoiTodo(tr("多边形 ROI 暂未实现，请使用矩形检测区域"));
    });
    connect(ui->basicDetectionResetButton, &QToolButton::clicked, this, &BlobPresenceDialog::resetDetectRoi);
    connect(ui->detectionResetButton, &QToolButton::clicked, this, &BlobPresenceDialog::resetDetectRoi);
    connect(ui->detectionMaskEditButton, &QPushButton::clicked, this, [this]() {
        showDetectRoiTodo(tr("检测屏蔽区域暂未实现，第一版不参与算法"));
    });

    m_basicResultPresenceGroup->setExclusive(true);
    m_basicResultPresenceGroup->addButton(ui->basicPresentOkButton, 0);
    m_basicResultPresenceGroup->addButton(ui->basicAbsentOkButton, 1);
    m_resultPresenceGroup->setExclusive(true);
    m_resultPresenceGroup->addButton(ui->presentOkButton, 0);
    m_resultPresenceGroup->addButton(ui->absentOkButton, 1);

    const auto connectMinMax = [this](QSpinBox *minSpinBox, QSpinBox *maxSpinBox) {
        connect(minSpinBox,
                static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this,
                [maxSpinBox](int value) {
                    if (value > maxSpinBox->value())
                        maxSpinBox->setValue(value);
                });
        connect(maxSpinBox,
                static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
                this,
                [minSpinBox](int value) {
                    if (value < minSpinBox->value())
                        minSpinBox->setValue(value);
                });
    };
    connectMinMax(ui->basicMinGraySpinBox, ui->basicMaxGraySpinBox);
    connectMinMax(ui->minGraySpinBox, ui->maxGraySpinBox);
    connectMinMax(ui->minAreaSpinBox, ui->maxAreaSpinBox);

    if (m_previewHelper) {
        connect(m_previewHelper,
                &FrameViewHelper::roiChanged,
                this,
                &BlobPresenceDialog::handleRoiChanged);
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

void BlobPresenceDialog::finishConfiguration()
{
    accept();
}

void BlobPresenceDialog::runReferenceTest()
{
    const cv::Mat referenceImage = ReferenceImageProvider::instance().referenceFrame();
    if (referenceImage.empty()) {
        displayBlobPresenceError(QStringLiteral("no_reference_image"),
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

    runBlobPresenceOnFrame(referenceImage.clone(),
                           referenceImage.clone(),
                           tr("基准图"),
                           tr("基准图为空，无法测试"));
}

void BlobPresenceDialog::runCameraTest()
{
    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        displayBlobPresenceError(QStringLiteral("image_empty"),
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

    runBlobPresenceOnFrame(snapshot,
                           ReferenceImageProvider::instance().referenceFrame(),
                           tr("测试图像"),
                           tr("当前图像为空，无法测试"));
}

void BlobPresenceDialog::applyAdaptiveWindowSize()
{
    WindowUtils::applyLargeWindow(this);
}

void BlobPresenceDialog::fitPreview()
{
    if (m_previewHelper)
        m_previewHelper->fitToView();
}

void BlobPresenceDialog::showReferenceImage()
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

void BlobPresenceDialog::showFrameForRoiEditing()
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

void BlobPresenceDialog::startDetectRoiEditing()
{
    if (!m_previewHelper)
        return;

    showFrameForRoiEditing();
    if (m_previewHelper->imageSize().isEmpty())
        return;

    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    m_previewHelper->setRoiDrawingEnabled(true);
    const QString text = tr("请框选斑点检测区域");
    setViewerStatusText(text, text);
}

void BlobPresenceDialog::showDetectRoiTodo(const QString &message)
{
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    setViewerStatusText(message, message);
}

void BlobPresenceDialog::resetDetectRoi()
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

void BlobPresenceDialog::handleRoiChanged(const QRectF &roi)
{
    m_roiNormalized = roi;
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    const QString roiText = detectRoiStatusText();
    setViewerStatusText(roiText, roiText);
    qDebug() << "[BlobPresenceDialog] ROI normalized:" << m_roiNormalized;
}

void BlobPresenceDialog::handleRoiSelectionRejected()
{
    const QString text = tr("ROI 无效，请拖拽宽高至少 2 像素的矩形");
    setViewerStatusText(text, text);
    if (m_previewHelper)
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void BlobPresenceDialog::runBlobPresenceOnFrame(const cv::Mat &frame,
                                                const cv::Mat &referenceImage,
                                                const QString &imageTitle,
                                                const QString &emptyFrameMessage)
{
    if (m_blobPresenceRunning)
        return;

    if (frame.empty()) {
        displayBlobPresenceError(QStringLiteral("image_empty"), emptyFrameMessage);
        return;
    }

    m_blobPresenceRunning = true;

    ToolConfig config = toToolConfig();
    config.roiNormalized = effectiveRoiNormalized();

    ToolRequest request;
    request.config = config;
    request.image = frame.clone();
    request.referenceImage = referenceImage.empty() ? cv::Mat() : referenceImage.clone();

    const ToolResult result = m_testToolEngine.runTool(request);
    if (!imageTitle.isEmpty())
        ui->viewerTitleLabel->setText(imageTitle);
    displayBlobPresenceResult(result);

    m_blobPresenceRunning = false;
}

void BlobPresenceDialog::displayBlobPresenceResult(const ToolResult &result)
{
    qDebug() << "[BlobPresenceDialog] ToolResult"
             << "status=" << result.status
             << "message=" << result.message
             << "score=" << result.score
             << "count=" << result.count
             << "ok=" << result.ok;

    const QString displayText = tr("%1 | count:%2 | %3")
            .arg(result.status,
                 QString::number(result.count),
                 result.ok ? QStringLiteral("OK") : QStringLiteral("NG"));
    setViewerStatusText(displayText, makeBlobPresenceStatusTooltipText(result));

    if (m_previewHelper) {
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
        m_previewHelper->setToolOverlays(result.overlays);
    }
}

void BlobPresenceDialog::displayBlobPresenceError(const QString &status, const QString &message)
{
    qDebug() << "[BlobPresenceDialog] ToolResult"
             << "status=" << status
             << "message=" << message
             << "score=" << 0.0
             << "count=" << 0
             << "ok=" << false;

    const QString displayText = tr("BlobPresence: %1 | %2").arg(status, message);
    setViewerStatusText(displayText, makeBlobPresenceErrorTooltipText(status, message));
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
}

void BlobPresenceDialog::setViewerStatusText(const QString &displayText, const QString &tooltipText)
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

QString BlobPresenceDialog::detectRoiStatusText() const
{
    const QRectF roi = effectiveRoiNormalized();
    return tr("检测 ROI x=%1 y=%2 w=%3 h=%4")
            .arg(roi.x(), 0, 'f', 3)
            .arg(roi.y(), 0, 'f', 3)
            .arg(roi.width(), 0, 'f', 3)
            .arg(roi.height(), 0, 'f', 3);
}

QRectF BlobPresenceDialog::effectiveRoiNormalized() const
{
    if (m_roiNormalized.width() <= 0.0 || m_roiNormalized.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    const QRectF roi = m_roiNormalized.normalized().intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (roi.width() <= 0.0 || roi.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    return roi;
}
