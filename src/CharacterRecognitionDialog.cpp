#include "CharacterRecognitionDialog.h"
#include "ui_CharacterRecognitionDialog.h"

#include "PlanDialogUtils.h"

#include "algorithms/halcon/HalconRuntimePaths.h"

#include <QButtonGroup>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QComboBox>
#include <QJsonObject>
#include <QUuid>

#include <opencv2/imgproc.hpp>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolRequest.h"

namespace {

QString defaultHalconOcrModelPath()
{
    QStringList triedPaths;
    const QString resolvedPath = HalconRuntimePaths::resolveOcrModelPath(QString(), &triedPaths);
    if (!resolvedPath.isEmpty())
        return resolvedPath;

    return triedPaths.isEmpty() ? QString() : triedPaths.first();
}

QString judgeModeFromResultBasis(const QString &resultBasis)
{
    if (resultBasis == QObject::tr("字符个数"))
        return QStringLiteral("char_count");
    if (resultBasis == QObject::tr("字符得分"))
        return QStringLiteral("score");
    if (resultBasis == QObject::tr("基准字符"))
        return QStringLiteral("baseline_text");
    return QStringLiteral("unknown");
}

QImage imageFromFrame(const cv::Mat &frame)
{
    return MatImageConverter::matToDisplayImage(frame);
}

int textPixelWidth(const QFontMetrics &metrics, const QString &text)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
    return metrics.horizontalAdvance(text);
#else
    return metrics.width(text);
#endif
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

QString ocrTextForUi(const QString &text)
{
    return text.isEmpty() ? QStringLiteral("<empty>") : text;
}

QString makeOcrStatusTooltipText(const ToolResult &result)
{
    return QStringLiteral("status: %1\nmessage: %2\ntext: %3\nscore: %4\ncount: %5\nok: %6")
            .arg(result.status,
                 result.message,
                 result.text,
                 QString::number(result.score, 'f', 6),
                 QString::number(result.count),
                 boolDisplayText(result.ok));
}

QString makeOcrStatusDisplayText(const ToolResult &result,
                                 const QFontMetrics &metrics,
                                 const int maxPixelWidth)
{
    const int width = maxPixelWidth > 80 ? maxPixelWidth : 640;
    const QString text = ocrTextForUi(result.text);
    const QString prefix = QStringLiteral("OCR: %1 | 文本:").arg(result.status);
    const QString suffix = QStringLiteral(" | 置信度:%1 | 字符数:%2 | %3")
            .arg(QString::number(result.score, 'f', 3),
                 QString::number(result.count),
                 result.ok ? QStringLiteral("OK") : QStringLiteral("NG"));
    const QString fullText = prefix + text + suffix;

    const int reservedWidth = textPixelWidth(metrics, prefix + suffix);
    const int textWidth = width - reservedWidth;
    if (textWidth > textPixelWidth(metrics, QStringLiteral("..."))) {
        const QString elidedText = metrics.elidedText(text, Qt::ElideRight, textWidth);
        const QString candidate = prefix + elidedText + suffix;
        if (textPixelWidth(metrics, candidate) <= width)
            return candidate;
    }

    return metrics.elidedText(fullText, Qt::ElideRight, width);
}

QString makeOcrErrorTooltipText(const QString &status, const QString &message)
{
    return QStringLiteral("status: %1\nmessage: %2\ntext: \nscore: 0.000000\ncount: 0\nok: false")
            .arg(status, message);
}

void setComboBoxValue(QComboBox *comboBox, const QString &value)
{
    if (!comboBox || value.isEmpty())
        return;

    const int index = comboBox->findText(value);
    if (index >= 0)
        comboBox->setCurrentIndex(index);
}
}

CharacterRecognitionDialog::CharacterRecognitionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CharacterRecognitionDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_regionGroup(new QButtonGroup(this))
    , m_previewHelper(nullptr)
{
    ui->setupUi(this);
    m_toolId = QStringLiteral("ocr_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_testToolEngine.registerAdapter(&m_testOcrAdapter);
    m_continuousTimer = new QTimer(this);
    m_continuousTimer->setInterval(500);
    connect(m_continuousTimer, &QTimer::timeout, this, &CharacterRecognitionDialog::runContinuousTick);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    connectControls();
    setUiMode(OcrUiMode::Edit);
}

CharacterRecognitionDialog::~CharacterRecognitionDialog()
{
    delete ui;
}

CharacterRecognitionConfig CharacterRecognitionDialog::configuration() const
{
    CharacterRecognitionConfig config;
    config.independentPositionCorrection = ui->positionCorrectionSwitch->isChecked();
    config.positionCorrection = ui->positionCorrectionComboBox->currentText();
    config.resultBasis = ui->resultBasisComboBox->currentText();
    config.minCount = ui->minCountSpinBox->value();
    config.maxCount = ui->maxCountSpinBox->value();
    config.minScore = ui->minScoreSpinBox->value();
    config.baselineText = ui->baselineTextLineEdit->text();
    config.modelName = ui->modelComboBox->currentText();
    return config;
}

ToolConfig CharacterRecognitionDialog::toToolConfig() const
{
    const CharacterRecognitionConfig ocrConfig = configuration();
    const QString judgeMode = judgeModeFromResultBasis(ocrConfig.resultBasis);
    QString modelPath = ui->modelPathLineEdit->text().trimmed();
    if (modelPath.isEmpty())
        modelPath = defaultHalconOcrModelPath();

    QJsonObject params;
    params.insert(QStringLiteral("modelName"), ocrConfig.modelName);
    params.insert(QStringLiteral("modelPath"), modelPath);
    params.insert(QStringLiteral("ocrModelPath"), modelPath);
    params.insert(QStringLiteral("ocr_model_path"), modelPath);
    params.insert(QStringLiteral("resultBasis"), ocrConfig.resultBasis);
    params.insert(QStringLiteral("minCount"), ocrConfig.minCount);
    params.insert(QStringLiteral("maxCount"), ocrConfig.maxCount);
    params.insert(QStringLiteral("minScore"), ocrConfig.minScore);
    params.insert(QStringLiteral("minConfidence"), ui->minConfidenceSpinBox->value());
    params.insert(QStringLiteral("baselineText"), ocrConfig.baselineText);
    params.insert(QStringLiteral("polarity"), ui->polarityComboBox->currentText());
    params.insert(QStringLiteral("binaryThreshold"), ui->binaryThresholdSpinBox->value());
    params.insert(QStringLiteral("minCharArea"), ui->minCharAreaSpinBox->value());
    params.insert(QStringLiteral("maxCharArea"), ui->maxCharAreaSpinBox->value());
    params.insert(QStringLiteral("minCharWidth"), ui->minCharWidthSpinBox->value());
    params.insert(QStringLiteral("minCharHeight"), ui->minCharHeightSpinBox->value());
    params.insert(QStringLiteral("maxCharWidth"), ui->maxCharWidthSpinBox->value());
    params.insert(QStringLiteral("maxCharHeight"), ui->maxCharHeightSpinBox->value());
    params.insert(QStringLiteral("minAspectRatio"), ui->minAspectRatioSpinBox->value());
    params.insert(QStringLiteral("maxAspectRatio"), ui->maxAspectRatioSpinBox->value());
    params.insert(QStringLiteral("independentPositionCorrection"),
                  ocrConfig.independentPositionCorrection);
    params.insert(QStringLiteral("positionCorrection"), ocrConfig.positionCorrection);

    QJsonObject judgeRule;
    judgeRule.insert(QStringLiteral("mode"), judgeMode);
    if (judgeMode == QStringLiteral("char_count")) {
        judgeRule.insert(QStringLiteral("minCount"), ocrConfig.minCount);
        judgeRule.insert(QStringLiteral("maxCount"), ocrConfig.maxCount);
    } else if (judgeMode == QStringLiteral("score")) {
        judgeRule.insert(QStringLiteral("minScore"), ocrConfig.minScore);
    } else if (judgeMode == QStringLiteral("baseline_text")) {
        judgeRule.insert(QStringLiteral("expectedText"), ocrConfig.baselineText);
        judgeRule.insert(QStringLiteral("matchRule"), QStringLiteral("exact"));
    }

    ToolConfig config;
    config.toolId = m_toolId;
    config.toolName = tr("字符识别");
    config.toolType = ToolType::Ocr;
    config.category = ToolCategory::Recognition;
    config.enabled = m_enabled;
    config.roiNormalized = m_roiNormalized;
    config.params = params;
    config.judgeRule = judgeRule;
    config.displayName = tr("字符识别");
    config.summary = summaryText();
    return config;
}

ToolConfig CharacterRecognitionDialog::toolConfig() const
{
    return toToolConfig();
}

ToolPreviewSnapshot CharacterRecognitionDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void CharacterRecognitionDialog::loadFromConfig(const ToolConfig &config)
{
    if (!config.toolId.trimmed().isEmpty())
        m_toolId = config.toolId;
    m_enabled = config.enabled;
    if (config.roiNormalized.width() > 0.0 && config.roiNormalized.height() > 0.0)
        m_roiNormalized = config.roiNormalized;

    const QJsonObject params = config.params;
    setComboBoxValue(ui->modelComboBox, params.value(QStringLiteral("modelName")).toString());
    const QString modelPath = params.value(QStringLiteral("modelPath")).toString(
                params.value(QStringLiteral("ocrModelPath")).toString());
    if (!modelPath.trimmed().isEmpty())
        ui->modelPathLineEdit->setText(modelPath);
    setComboBoxValue(ui->resultBasisComboBox, params.value(QStringLiteral("resultBasis")).toString());
    ui->resultBasisStackedWidget->setCurrentIndex(ui->resultBasisComboBox->currentIndex());
    ui->minCountSpinBox->setValue(params.value(QStringLiteral("minCount")).toInt(ui->minCountSpinBox->value()));
    ui->maxCountSpinBox->setValue(params.value(QStringLiteral("maxCount")).toInt(ui->maxCountSpinBox->value()));
    ui->minScoreSpinBox->setValue(params.value(QStringLiteral("minScore")).toInt(ui->minScoreSpinBox->value()));
    ui->minConfidenceSpinBox->setValue(params.value(QStringLiteral("minConfidence")).toInt(ui->minConfidenceSpinBox->value()));
    ui->baselineTextLineEdit->setText(params.value(QStringLiteral("baselineText")).toString(ui->baselineTextLineEdit->text()));
    setComboBoxValue(ui->polarityComboBox, params.value(QStringLiteral("polarity")).toString());
    ui->binaryThresholdSpinBox->setValue(params.value(QStringLiteral("binaryThreshold")).toInt(ui->binaryThresholdSpinBox->value()));
    ui->minCharAreaSpinBox->setValue(params.value(QStringLiteral("minCharArea")).toInt(ui->minCharAreaSpinBox->value()));
    ui->maxCharAreaSpinBox->setValue(params.value(QStringLiteral("maxCharArea")).toInt(ui->maxCharAreaSpinBox->value()));
    ui->minCharWidthSpinBox->setValue(params.value(QStringLiteral("minCharWidth")).toInt(ui->minCharWidthSpinBox->value()));
    ui->minCharHeightSpinBox->setValue(params.value(QStringLiteral("minCharHeight")).toInt(ui->minCharHeightSpinBox->value()));
    ui->maxCharWidthSpinBox->setValue(params.value(QStringLiteral("maxCharWidth")).toInt(ui->maxCharWidthSpinBox->value()));
    ui->maxCharHeightSpinBox->setValue(params.value(QStringLiteral("maxCharHeight")).toInt(ui->maxCharHeightSpinBox->value()));
    ui->minAspectRatioSpinBox->setValue(params.value(QStringLiteral("minAspectRatio")).toDouble(ui->minAspectRatioSpinBox->value()));
    ui->maxAspectRatioSpinBox->setValue(params.value(QStringLiteral("maxAspectRatio")).toDouble(ui->maxAspectRatioSpinBox->value()));
    ui->positionCorrectionSwitch->setChecked(params.value(QStringLiteral("independentPositionCorrection")).toBool(ui->positionCorrectionSwitch->isChecked()));
    setComboBoxValue(ui->positionCorrectionComboBox, params.value(QStringLiteral("positionCorrection")).toString());

    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    if (m_previewHelper)
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    const QString roiText = tr("ROI: x=%1 y=%2 w=%3 h=%4")
            .arg(m_roiNormalized.x(), 0, 'f', 3)
            .arg(m_roiNormalized.y(), 0, 'f', 3)
            .arg(m_roiNormalized.width(), 0, 'f', 3)
            .arg(m_roiNormalized.height(), 0, 'f', 3);
    setViewerStatusText(roiText, roiText);
}

QString CharacterRecognitionDialog::summaryText() const
{
    const CharacterRecognitionConfig config = configuration();

    if (config.resultBasis == tr("字符得分")) {
        return tr("模型: %1, 判断: %2, 最小得分: %3")
            .arg(config.modelName)
            .arg(config.resultBasis)
            .arg(config.minScore);
    }

    if (config.resultBasis == tr("基准字符")) {
        return tr("模型: %1, 判断: %2, 基准字符: %3")
            .arg(config.modelName)
            .arg(config.resultBasis)
            .arg(config.baselineText);
    }

    return tr("模型: %1, 判断: %2, 字符数: %3-%4")
        .arg(config.modelName)
        .arg(config.resultBasis)
        .arg(config.minCount)
        .arg(config.maxCount);
}

void CharacterRecognitionDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void CharacterRecognitionDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 字符识别"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    applyAdaptiveWindowSize();

    m_exitTestButton = new QPushButton(tr("退出测试"), this);
    m_exitTestButton->setObjectName(QStringLiteral("exitTestButton"));
    m_exitTestButton->setMinimumSize(120, 48);
    m_exitTestButton->setProperty("actionRole", QStringLiteral("secondary"));
    ui->horizontalLayout_actions->addWidget(m_exitTestButton);

    ui->ocrConfigScrollArea->setWidgetResizable(true);
    ui->ocrConfigScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->verticalLayout_editor->setStretch(1, 1);
    ui->configStackedWidget->setCurrentIndex(0);
    ui->resultBasisStackedWidget->setCurrentIndex(ui->resultBasisComboBox->currentIndex());
    ui->minScoreSpinBox->setValue(70);
    ui->minConfidenceSpinBox->setValue(ui->minScoreSpinBox->value());
    if (ui->modelPathLineEdit->text().trimmed().isEmpty())
        ui->modelPathLineEdit->setText(defaultHalconOcrModelPath());

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
}

void CharacterRecognitionDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &CharacterRecognitionDialog::reject);
    connect(ui->referenceTestButton, &QPushButton::clicked, this, &CharacterRecognitionDialog::runReferenceTest);
    connect(ui->testRunButton, &QPushButton::clicked, this, &CharacterRecognitionDialog::handleTestRunButton);
    connect(ui->finishButton, &QPushButton::clicked, this, &CharacterRecognitionDialog::handleFinishButton);
    connect(m_exitTestButton, &QPushButton::clicked, this, &CharacterRecognitionDialog::exitTestMode);
    connect(ui->resultBasisComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            ui->resultBasisStackedWidget,
            &QStackedWidget::setCurrentIndex);

    m_segmentGroup->setExclusive(true);
    m_segmentGroup->addButton(ui->basicSegmentButton, 0);
    m_segmentGroup->addButton(ui->allSegmentButton, 1);
    connect(ui->basicSegmentButton, &QPushButton::clicked, this, [this]() {
        ui->configStackedWidget->setCurrentIndex(0);
    });
    connect(ui->allSegmentButton, &QPushButton::clicked, this, [this]() {
        ui->configStackedWidget->setCurrentIndex(1);
    });

    connect(ui->minScoreSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                const QSignalBlocker blocker(ui->minConfidenceSpinBox);
                ui->minConfidenceSpinBox->setValue(value);
            });
    connect(ui->minConfidenceSpinBox,
            static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                const QSignalBlocker blocker(ui->minScoreSpinBox);
                ui->minScoreSpinBox->setValue(value);
            });
    connect(ui->selectModelButton, &QPushButton::clicked, this, [this]() {
        const QString currentPath = ui->modelPathLineEdit->text().trimmed();
        const QString startDir = currentPath.isEmpty()
                ? QFileInfo(defaultHalconOcrModelPath()).absolutePath()
                : QFileInfo(currentPath).absolutePath();
        const QString path = QFileDialog::getOpenFileName(this,
                                                          tr("选择 OCR 模型"),
                                                          startDir,
                                                          tr("OCR 模型 (*.omc *.occ);;所有文件 (*)"));
        if (path.isEmpty())
            return;

        ui->modelPathLineEdit->setText(path);
        if (path.endsWith(QStringLiteral(".occ"), Qt::CaseInsensitive)) {
            qWarning() << "[CharacterRecognitionDialog] Selected .occ OCR model;"
                       << "current HALCON runner uses MLP .omc models:" << path;
        }
    });

    m_regionGroup->setExclusive(true);
    m_regionGroup->addButton(ui->regionDrawButton, 0);
    m_regionGroup->addButton(ui->regionRectButton, 1);
    connect(ui->regionDrawButton, &QToolButton::clicked, this, [this]() {
        if (m_previewHelper) {
            m_previewHelper->setRoiDrawingEnabled(true);
            ui->viewerTitleLabel->setText(tr("绘制检测区域"));
        }
    });
    connect(ui->regionRectButton, &QToolButton::clicked, this, [this]() {
        if (m_previewHelper) {
            m_previewHelper->setRoiDrawingEnabled(true);
            ui->viewerTitleLabel->setText(tr("绘制矩形检测区域"));
        }
    });
    if (m_previewHelper) {
        connect(m_previewHelper,
                &FrameViewHelper::roiChanged,
                this,
                [this](const QRectF &roi) {
                    m_roiNormalized = roi;
                    if (m_previewHelper)
                        m_previewHelper->clearToolOverlays();
                    const QString roiText = tr("ROI: x=%1 y=%2 w=%3 h=%4")
                            .arg(m_roiNormalized.x(), 0, 'f', 3)
                            .arg(m_roiNormalized.y(), 0, 'f', 3)
                            .arg(m_roiNormalized.width(), 0, 'f', 3)
                            .arg(m_roiNormalized.height(), 0, 'f', 3);
                    setViewerStatusText(roiText, roiText);
                    qDebug() << "[CharacterRecognitionDialog] ROI normalized:" << m_roiNormalized;
                });
    }

    connect(ui->minCountSpinBox,static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,[this](int value) {
        if (value > ui->maxCountSpinBox->value()) {
        ui->maxCountSpinBox->setValue(value);
        }
    });
    connect(ui->maxCountSpinBox,static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,[this](int value) {
        if (value < ui->minCountSpinBox->value()) {
        ui->minCountSpinBox->setValue(value);
        }
    });

    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) {
                if (m_uiMode == OcrUiMode::Edit || m_uiMode == OcrUiMode::TestReady) {
                    showReferenceImage();
                }
            });
}

void CharacterRecognitionDialog::finishConfiguration()
{
    accept();
}

void CharacterRecognitionDialog::showProviderImage(const QImage &image)
{
    showLiveImage(image);
}

void CharacterRecognitionDialog::handleTestRunButton()
{
    if (m_uiMode == OcrUiMode::Edit) {
        setUiMode(OcrUiMode::TestReady);
        return;
    }

    startContinuousRun();
}

void CharacterRecognitionDialog::handleFinishButton()
{
    if (m_uiMode == OcrUiMode::Edit) {
        finishConfiguration();
        return;
    }

    runOnceInTestMode();
}

void CharacterRecognitionDialog::enterTestMode()
{
    setUiMode(OcrUiMode::TestReady);
}

void CharacterRecognitionDialog::exitTestMode()
{
    stopContinuousRun();
    setUiMode(OcrUiMode::Edit);
}

void CharacterRecognitionDialog::runOnceInTestMode()
{
    stopContinuousRun();
    setUiMode(OcrUiMode::SingleShot);
}

void CharacterRecognitionDialog::applyAdaptiveWindowSize()
{
    PlanDialogUtils::applyLargeWindow(this);
}

void CharacterRecognitionDialog::setUiMode(OcrUiMode mode)
{
    if (m_frameUpdatedConnection) {
        QObject::disconnect(m_frameUpdatedConnection);
        m_frameUpdatedConnection = QMetaObject::Connection();
    }

    m_uiMode = mode;

    switch (m_uiMode) {
    case OcrUiMode::Edit:
        if (m_previewHelper)
            m_previewHelper->clearToolOverlays();
        showReferenceImage();
        break;
    case OcrUiMode::TestReady:
        showReferenceImage();
        break;
    case OcrUiMode::Continuous:
        m_frameUpdatedConnection = connect(&CameraFrameProvider::instance(),
                                           &CameraFrameProvider::frameUpdated,
                                           this,
                                           &CharacterRecognitionDialog::showProviderImage);
        showLiveImage(CameraFrameProvider::instance().currentImage());
        break;
    case OcrUiMode::SingleShot:
        showSingleShotImage();
        break;
    }

    updateBottomButtons();
}

void CharacterRecognitionDialog::showReferenceImage()
{
    if (!m_previewHelper) {
        return;
    }

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        return;
    }

    ui->viewerTitleLabel->setText(tr("基准图"));
    m_previewHelper->setImage(image);
    m_previewHelper->setRoiRectNormalized(m_roiNormalized);
}

void CharacterRecognitionDialog::showLiveImage(const QImage &image)
{
    if (m_uiMode != OcrUiMode::Continuous || !m_previewHelper) {
        return;
    }

    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("当前无图像"));
        return;
    }

    ui->viewerTitleLabel->setText(tr("测试图像"));
    m_previewHelper->setImage(image);
    m_previewHelper->setRoiRectNormalized(m_roiNormalized);
}

void CharacterRecognitionDialog::showSingleShotImage()
{
    if (!m_previewHelper) {
        return;
    }

    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        displayOcrError(QStringLiteral("image_empty"), tr("当前帧为空"));
        return;
    }

    const cv::Mat snapshot = frame.clone();
    const QImage image = imageFromFrame(snapshot);
    if (!image.isNull()) {
        ui->viewerTitleLabel->setText(tr("单次测试快照"));
        m_previewHelper->setImage(image);
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    }

    runOcrOnFrame(snapshot, tr("单次测试快照"));
}

void CharacterRecognitionDialog::startContinuousRun()
{
    setUiMode(OcrUiMode::Continuous);
    if (m_continuousTimer && !m_continuousTimer->isActive())
        m_continuousTimer->start();
    runContinuousTick();
}

void CharacterRecognitionDialog::stopContinuousRun()
{
    if (m_continuousTimer)
        m_continuousTimer->stop();

    if (m_frameUpdatedConnection) {
        QObject::disconnect(m_frameUpdatedConnection);
        m_frameUpdatedConnection = QMetaObject::Connection();
    }
}

void CharacterRecognitionDialog::runContinuousTick()
{
    if (m_uiMode != OcrUiMode::Continuous || m_ocrRunning)
        return;

    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        displayOcrError(QStringLiteral("image_empty"), tr("当前帧为空"));
        return;
    }

    const QImage image = imageFromFrame(frame);
    if (!image.isNull())
        showLiveImage(image);

    runOcrOnFrame(frame.clone(), tr("测试图像"));
}

void CharacterRecognitionDialog::runReferenceTest()
{
    const cv::Mat frame = ReferenceImageProvider::instance().referenceFrame();
    if (frame.empty()) {
        displayOcrError(QStringLiteral("no_reference_image"), tr("请先设置基准图"));
        return;
    }

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (!image.isNull() && m_previewHelper) {
        ui->viewerTitleLabel->setText(tr("基准图"));
        m_previewHelper->setImage(image);
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    }

    runOcrOnFrame(frame, tr("基准图"), true);
}

void CharacterRecognitionDialog::runOcrOnFrame(const cv::Mat &frame,
                                               const QString &imageTitle,
                                               bool referenceTest)
{
    if (m_ocrRunning)
        return;

    if (frame.empty()) {
        displayOcrError(QStringLiteral("image_empty"), tr("OCR input image is empty."));
        return;
    }

    m_ocrRunning = true;

    ToolConfig config = toToolConfig();
    config.roiNormalized = m_roiNormalized;

    ToolRequest request;
    request.config = config;
    request.image = frame;

    const ToolResult result = m_testToolEngine.runTool(request);
    if (!imageTitle.isEmpty())
        ui->viewerTitleLabel->setText(imageTitle);
    displayOcrResult(result);
    if (referenceTest)
        m_referencePreviewSnapshot = makeReferenceToolPreviewSnapshot(config, result, m_roiNormalized);

    m_ocrRunning = false;
}

void CharacterRecognitionDialog::displayOcrResult(const ToolResult &result)
{
    const QJsonObject payload = result.payload;
    qDebug() << "[CharacterRecognitionDialog] OCR ToolResult"
             << "status=" << result.status
             << "message=" << result.message
             << "text=" << result.text
             << "score=" << result.score
             << "count=" << result.count
             << "ok=" << result.ok
             << "rawText=" << payload.value(QStringLiteral("rawText")).toString()
             << "filteredText=" << payload.value(QStringLiteral("filteredText")).toString()
             << "rawCount=" << payload.value(QStringLiteral("rawCharCount")).toInt()
             << "filteredCount=" << payload.value(QStringLiteral("filteredCharCount")).toInt()
             << "judgeMode=" << payload.value(QStringLiteral("judgeMode")).toString()
             << "expectedText=" << payload.value(QStringLiteral("expectedText")).toString()
             << "minCount=" << payload.value(QStringLiteral("judgeMinCount")).toInt()
             << "maxCount=" << payload.value(QStringLiteral("judgeMaxCount")).toInt()
             << "minConfidence=" << payload.value(QStringLiteral("minConfidence")).toDouble();

    const QString displayText = makeOcrStatusDisplayText(result,
                                                         ui->viewerStatusLabel->fontMetrics(),
                                                         labelDisplayWidth(ui->viewerStatusLabel));
    setViewerStatusText(displayText, makeOcrStatusTooltipText(result));

    if (m_previewHelper) {
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
        m_previewHelper->setToolOverlays(displayOverlaysWithoutRoi(result.overlays));
    }
}

void CharacterRecognitionDialog::displayOcrError(const QString &status, const QString &message)
{
    qDebug() << "[CharacterRecognitionDialog] OCR ToolResult"
             << "status=" << status
             << "message=" << message
             << "text=" << QString()
             << "score=" << 0.0
             << "count=" << 0
             << "ok=" << false;

    const QString displayText = tr("OCR: %1 | %2").arg(status, message);
    setViewerStatusText(displayText, makeOcrErrorTooltipText(status, message));
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setRoiRectNormalized(m_roiNormalized);
    }
}

void CharacterRecognitionDialog::setViewerStatusText(const QString &displayText, const QString &tooltipText)
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

QVector<ToolOverlay> CharacterRecognitionDialog::displayOverlaysWithoutRoi(const QVector<ToolOverlay> &overlays) const
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

void CharacterRecognitionDialog::updateBottomButtons()
{
    if (!m_exitTestButton) {
        return;
    }

    if (m_uiMode != OcrUiMode::Edit) {
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


void CharacterRecognitionDialog::fitPreview()
{
    if (m_previewHelper) {
        m_previewHelper->fitToView();
    }
}
