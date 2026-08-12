#include "EdgePresenceDialog.h"
#include "ui_EdgePresenceDialog.h"

#include "PlanDialogUtils.h"
#include "PositionCorrectionDialogTestHelper.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QEventLoop>
#include <QFontMetrics>
#include <QImage>
#include <QJsonObject>
#include <QLabel>
#include <QLineF>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QUuid>
#include <QtGlobal>

#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cmath>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolRequest.h"
#include "toolcore/PositionCorrection.h"

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

QString edgePolarityDisplayText(const QString &value)
{
    if (value == QStringLiteral("black_to_white"))
        return QObject::tr("黑到白");
    if (value == QStringLiteral("white_to_black"))
        return QObject::tr("白到黑");
    return QObject::tr("任意");
}

QString makeEdgePresenceStatusTooltipText(const ToolResult &result)
{
    return QStringLiteral("status: %1\nmessage: %2\nscore: %3\nvalue: %4\ncount: %5\nok: %6")
            .arg(result.status,
                 result.message,
                 QString::number(result.score, 'f', 6),
                 QString::number(result.value, 'f', 3),
                 QString::number(result.count),
                 boolDisplayText(result.ok));
}

QString makeEdgePresenceErrorTooltipText(const QString &status, const QString &message)
{
    return QStringLiteral("status: %1\nmessage: %2\nscore: 0.000000\nvalue: 0.000\ncount: 0\nok: false")
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

QJsonObject pointToJson(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
}

QPointF pointFromJson(const QJsonObject &json, const QPointF &fallback)
{
    if (json.isEmpty())
        return fallback;
    return QPointF(json.value(QStringLiteral("x")).toDouble(fallback.x()),
                   json.value(QStringLiteral("y")).toDouble(fallback.y()));
}

LineBandRoi defaultLineBandRoi()
{
    LineBandRoi roi;
    roi.p1Normalized = QPointF(0.15, 0.5);
    roi.p2Normalized = QPointF(0.85, 0.5);
    roi.widthNormalized = 0.08;
    roi.valid = true;
    return roi;
}

bool validLineBandRoi(const LineBandRoi &roi)
{
    return roi.valid &&
           std::isfinite(roi.p1Normalized.x()) &&
           std::isfinite(roi.p1Normalized.y()) &&
           std::isfinite(roi.p2Normalized.x()) &&
           std::isfinite(roi.p2Normalized.y()) &&
           std::isfinite(roi.widthNormalized) &&
           roi.widthNormalized > 0.0 &&
           QLineF(roi.p1Normalized, roi.p2Normalized).length() > 0.001;
}

LineBandRoi lineBandFromParams(const QJsonObject &params, const LineBandRoi &fallback)
{
    LineBandRoi roi;
    roi.p1Normalized = pointFromJson(params.value(QStringLiteral("searchLineP1")).toObject(),
                                     fallback.p1Normalized);
    roi.p2Normalized = pointFromJson(params.value(QStringLiteral("searchLineP2")).toObject(),
                                     fallback.p2Normalized);
    roi.widthNormalized = params.value(QStringLiteral("searchBandWidth")).toDouble(fallback.widthNormalized);
    roi.valid = validLineBandRoi(roi);
    return roi.valid ? roi : fallback;
}

void configureLineBandButton(QToolButton *button, const QString &tooltip)
{
    if (!button)
        return;

    button->setText(QStringLiteral("╱"));
    button->setToolTip(tooltip);
    button->setMinimumSize(QSize(52, 38));
    button->setMaximumSize(QSize(52, 38));
    button->setProperty("actionRole", QStringLiteral("toolbarIcon"));
}

} // namespace

EdgePresenceDialog::EdgePresenceDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::EdgePresenceDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_basicResultPresenceGroup(new QButtonGroup(this))
    , m_resultPresenceGroup(new QButtonGroup(this))
{
    ui->setupUi(this);
    m_lineBandRoi = defaultLineBandRoi();
    m_roiNormalized = QRectF(0.15, 0.46, 0.70, 0.08);
    m_toolId = QStringLiteral("edge_presence_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_testToolEngine.registerAdapter(&m_testTemplateLocationAdapter);
    m_testToolEngine.registerAdapter(&m_testPositionCorrectionAdapter);
    m_testToolEngine.registerAdapter(&m_testEdgePresenceAdapter);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);
    setupUiState();
    connectControls();
    showReferenceImage();
}

EdgePresenceDialog::~EdgePresenceDialog()
{
    delete ui;
}

EdgePresenceConfig EdgePresenceDialog::configuration() const
{
    const bool basicMode = ui->edgeParamsStackedWidget->currentWidget() == ui->basicParamsPage;

    EdgePresenceConfig config;
    const LineBandRoi lineBand = effectiveLineBandRoi();
    config.detectRegionType = QStringLiteral("line_band");
    config.searchLineP1 = lineBand.p1Normalized;
    config.searchLineP2 = lineBand.p2Normalized;
    config.searchBandWidth = lineBand.widthNormalized;
    config.enablePositionCorrection = basicMode
            ? ui->basicPositionCorrectionSwitch->isChecked()
            : ui->positionCorrectionSwitch->isChecked();
    config.positionCorrectionSource = basicMode
            ? ui->basicPositionCorrectionComboBox->currentText()
            : ui->positionCorrectionComboBox->currentText();
    config.positionCorrectionSourceId = basicMode
            ? ui->basicPositionCorrectionComboBox->currentData().toString().trimmed()
            : ui->positionCorrectionComboBox->currentData().toString().trimmed();
    config.sensitivity = basicMode
            ? ui->edgeBasicSensitivitySpinBox->value()
            : ui->edgeSensitivitySpinBox->value();
    config.edgePolarity = edgePolarityFromText(ui->edgePolarityComboBox->currentText());
    config.judgeBasis = QStringLiteral("presence");
    config.existOk = basicMode ? ui->basicPresentOkButton->isChecked()
                               : ui->presentOkButton->isChecked();
    return config;
}

ToolConfig EdgePresenceDialog::toToolConfig() const
{
    const EdgePresenceConfig edgeConfig = configuration();
    const QString toolId = m_toolId;

    QJsonObject params;
    params.insert(QStringLiteral("detectRegionType"), edgeConfig.detectRegionType);
    params.insert(QStringLiteral("searchLineP1"), pointToJson(edgeConfig.searchLineP1));
    params.insert(QStringLiteral("searchLineP2"), pointToJson(edgeConfig.searchLineP2));
    params.insert(QStringLiteral("searchBandWidth"), edgeConfig.searchBandWidth);
    params.insert(QStringLiteral("lineBandWidthUnit"), QStringLiteral("normalized_max_dimension"));
    PositionCorrectionConfig correction;
    correction.enabled = edgeConfig.enablePositionCorrection;
    correction.source = edgeConfig.positionCorrectionSource;
    correction.sourceId = edgeConfig.positionCorrectionSourceId;
    PositionCorrection::writeParams(correction, &params);
    params.insert(QStringLiteral("showPositionCorrectionMatchContour"),
                  edgeConfig.showPositionCorrectionMatchContour);
    params.insert(QStringLiteral("sensitivity"), edgeConfig.sensitivity);
    params.insert(QStringLiteral("edgePolarity"), edgeConfig.edgePolarity);
    params.insert(QStringLiteral("judgeBasis"), edgeConfig.judgeBasis);
    params.insert(QStringLiteral("existOk"), edgeConfig.existOk);

    QJsonObject judgeRule;
    judgeRule.insert(QStringLiteral("mode"), edgeConfig.judgeBasis);
    judgeRule.insert(QStringLiteral("existOk"), edgeConfig.existOk);

    ToolConfig config;
    config.toolId = toolId;
    config.toolName = tr("边缘有无");
    config.toolType = ToolType::EdgePresence;
    config.category = ToolCategory::Presence;
    config.enabled = m_enabled;
    config.roiNormalized = effectiveRoiNormalized();
    config.params = params;
    config.judgeRule = judgeRule;
    config.displayName = tr("边缘有无");
    config.summary = summaryText();
    return config;
}

ToolConfig EdgePresenceDialog::toolConfig() const
{
    return m_hasAcceptedToolConfig ? m_acceptedToolConfig : toToolConfig();
}

ToolPreviewSnapshot EdgePresenceDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void EdgePresenceDialog::loadFromConfig(const ToolConfig &config)
{
    m_importedTestActive = false;
    m_importedTestFrame.release();
    m_importedTestImageTitle.clear();
    updateBottomButtons();
    if (!config.toolId.trimmed().isEmpty())
        m_toolId = config.toolId;
    m_enabled = config.enabled;
    if (config.roiNormalized.width() > 0.0 && config.roiNormalized.height() > 0.0)
        m_roiNormalized = config.roiNormalized;

    const QJsonObject params = config.params;
    m_lineBandRoi = lineBandFromParams(params, defaultLineBandRoi());
    const bool allMode = params.value(QStringLiteral("edgePolarity")).toString() != QStringLiteral("any");
    ui->basicSegmentButton->setChecked(!allMode);
    ui->allSegmentButton->setChecked(allMode);
    ui->edgeParamsStackedWidget->setCurrentWidget(allMode ? ui->allParamsPage : ui->basicParamsPage);

    const PositionCorrectionConfig correction = PositionCorrection::fromParams(
                params, ui->basicPositionCorrectionSwitch->isChecked());
    const bool positionCorrection = correction.enabled;
    ui->basicPositionCorrectionSwitch->setChecked(positionCorrection);
    ui->positionCorrectionSwitch->setChecked(positionCorrection);
    setComboBoxValue(ui->basicPositionCorrectionComboBox, correction.source);
    setComboBoxValue(ui->positionCorrectionComboBox, correction.source);
    const int sensitivity = params.value(QStringLiteral("sensitivity")).toInt(ui->edgeBasicSensitivitySpinBox->value());
    ui->edgeBasicSensitivitySpinBox->setValue(sensitivity);
    ui->edgeSensitivitySpinBox->setValue(sensitivity);
    setComboBoxValue(ui->edgePolarityComboBox, edgePolarityDisplayText(params.value(QStringLiteral("edgePolarity")).toString()));

    const bool existOk = params.value(QStringLiteral("existOk")).toBool(true);
    ui->basicPresentOkButton->setChecked(existOk);
    ui->basicAbsentOkButton->setChecked(!existOk);
    ui->presentOkButton->setChecked(existOk);
    ui->absentOkButton->setChecked(!existOk);

    m_hasAcceptedToolConfig = false;
    m_acceptedToolConfig = ToolConfig();
    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    if (m_previewHelper) {
        m_previewHelper->clearRoi();
        m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
        m_roiNormalized = m_previewHelper->lineBandBoundingRectNormalized(effectiveLineBandRoi());
    }
    const QString roiText = detectRoiStatusText();
    setViewerStatusText(roiText, roiText);
    updateBottomButtons();
}

void EdgePresenceDialog::setToolChainTestContext(
        const QVector<ToolConfig> &toolConfigs,
        int currentToolIndex,
        const ReferencePositionCorrectionConfig &referencePositionCorrection)
{
    m_toolChainTestContext.toolConfigs = toolConfigs;
    m_toolChainTestContext.currentToolIndex = currentToolIndex;
    m_toolChainTestContext.referencePositionCorrection =
            referencePositionCorrection;
    m_toolChainTestContext.valid = true;
}

QString EdgePresenceDialog::summaryText() const
{
    const EdgePresenceConfig config = configuration();
    return tr("灵敏度: %1, 边缘极性: %2, %3")
            .arg(config.sensitivity)
            .arg(edgePolarityDisplayText(config.edgePolarity),
                 config.existOk ? tr("存在OK") : tr("不存在OK"));
}

void EdgePresenceDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void EdgePresenceDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 边缘有无"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    applyAdaptiveWindowSize();
    m_exitTestButton = new QPushButton(tr("退出测试"), this);
    m_exitTestButton->setObjectName(QStringLiteral("exitTestButton"));
    m_exitTestButton->setMinimumSize(120, 48);
    m_exitTestButton->setProperty("actionRole", QStringLiteral("secondary"));
    ui->horizontalLayout_actions->addWidget(m_exitTestButton);
    m_pcImportButton = new QPushButton(tr("PC导入图片"), this);
    m_pcImportButton->setObjectName(QStringLiteral("edgePcImportButton"));
    m_pcImportButton->setMinimumHeight(38);
    ui->horizontalLayout_editorHeader->insertWidget(2, m_pcImportButton);

    ui->basicSegmentButton->setChecked(true);
    ui->allSegmentButton->setChecked(false);
    ui->edgeParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
    ui->edgeBasicSensitivitySpinBox->setRange(0, 100);
    ui->edgeSensitivitySpinBox->setRange(0, 100);
    ui->edgeBasicSensitivitySpinBox->setValue(60);
    ui->edgeSensitivitySpinBox->setValue(60);
    ui->edgePolarityComboBox->setCurrentIndex(2);
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    configureLineBandButton(ui->basicDetectionRectButton, tr("线型搜索 ROI"));
    configureLineBandButton(ui->detectionRectButton, tr("线型搜索 ROI"));

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

void EdgePresenceDialog::connectControls()
{
    connect(ui->basicPositionCorrectionSwitch, &QCheckBox::toggled,
            ui->positionCorrectionSwitch, &QCheckBox::setChecked);
    connect(ui->positionCorrectionSwitch, &QCheckBox::toggled,
            ui->basicPositionCorrectionSwitch, &QCheckBox::setChecked);
    const auto syncCorrectionSource = [](QComboBox *source, QComboBox *target, int index) {
        if (!source || !target || index < 0)
            return;
        const int targetIndex = target->findData(source->itemData(index));
        if (targetIndex >= 0 && targetIndex != target->currentIndex())
            target->setCurrentIndex(targetIndex);
    };
    connect(ui->basicPositionCorrectionComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, syncCorrectionSource](int index) {
        syncCorrectionSource(ui->basicPositionCorrectionComboBox,
                             ui->positionCorrectionComboBox, index);
    });
    connect(ui->positionCorrectionComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, syncCorrectionSource](int index) {
        syncCorrectionSource(ui->positionCorrectionComboBox,
                             ui->basicPositionCorrectionComboBox, index);
    });
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &EdgePresenceDialog::reject);
    connect(ui->referenceTestButton, &QPushButton::clicked, this, &EdgePresenceDialog::runReferenceTest);
    connect(ui->testRunButton, &QPushButton::clicked, this, &EdgePresenceDialog::runCameraTest);
    connect(m_pcImportButton, &QPushButton::clicked,
            this, &EdgePresenceDialog::importTestImageFromPc);
    connect(m_exitTestButton, &QPushButton::clicked,
            this, &EdgePresenceDialog::exitTestMode);
    connect(ui->finishButton, &QPushButton::clicked, this, &EdgePresenceDialog::finishConfiguration);

    const auto applyParamMode = [this](bool allMode) {
        ui->basicSegmentButton->setChecked(!allMode);
        ui->allSegmentButton->setChecked(allMode);
        ui->edgeParamsStackedWidget->setCurrentWidget(allMode ? ui->allParamsPage : ui->basicParamsPage);
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

    connect(ui->basicDetectionRectButton,
            &QToolButton::clicked,
            this,
            &EdgePresenceDialog::startDetectRoiEditing);
    connect(ui->detectionRectButton,
            &QToolButton::clicked,
            this,
            &EdgePresenceDialog::startDetectRoiEditing);
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
                &EdgePresenceDialog::handleRoiChanged);
        connect(m_previewHelper,
                &FrameViewHelper::roiSelectionRejected,
                this,
                [this](const QRectF &) {
                    handleRoiSelectionRejected();
                });
        connect(m_previewHelper,
                &FrameViewHelper::lineBandChanged,
                this,
                &EdgePresenceDialog::handleLineBandChanged);
        connect(m_previewHelper,
                &FrameViewHelper::lineBandSelectionRejected,
                this,
                &EdgePresenceDialog::handleLineBandSelectionRejected);
    }

    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) {
                showReferenceImage();
            });
}

void EdgePresenceDialog::finishConfiguration()
{
    if (m_importedTestActive) {
        rerunImportedTest();
        return;
    }
    m_acceptedToolConfig = toToolConfig();
    m_hasAcceptedToolConfig = true;
    accept();
}

void EdgePresenceDialog::runReferenceTest()
{
    const cv::Mat referenceImage = ReferenceImageProvider::instance().referenceFrame();
    if (referenceImage.empty()) {
        displayEdgePresenceError(QStringLiteral("no_reference_image"),
                                 tr("基准图为空，无法测试"));
        return;
    }

    QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull())
        image = imageFromFrame(referenceImage);
    if (!image.isNull() && m_previewHelper) {
        ui->viewerTitleLabel->setText(tr("基准图"));
        m_previewHelper->setImage(image);
        m_previewHelper->clearRoi();
        m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
    }

    runEdgePresenceOnFrame(referenceImage.clone(),
                           referenceImage.clone(),
                           tr("基准图"),
                           tr("基准图为空，无法测试"),
                           true);
}

void EdgePresenceDialog::runCameraTest()
{
    const bool imported = m_importedTestActive && !m_importedTestFrame.empty();
    const cv::Mat frame = imported
            ? m_importedTestFrame.clone()
            : CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        displayEdgePresenceError(QStringLiteral("image_empty"),
                                 tr("当前图像为空，无法测试"));
        return;
    }

    const cv::Mat snapshot = frame.clone();
    const QImage image = imageFromFrame(snapshot);
    if (!image.isNull() && m_previewHelper) {
        ui->viewerTitleLabel->setText(
                    imported ? m_importedTestImageTitle : tr("测试图像"));
        m_previewHelper->setImage(image);
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearRoi();
        m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
    }

    runEdgePresenceOnFrame(snapshot,
                           ReferenceImageProvider::instance().referenceFrame(),
                           imported ? m_importedTestImageTitle : tr("测试图像"),
                           tr("当前图像为空，无法测试"));
}

void EdgePresenceDialog::importTestImageFromPc()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this, tr("PC导入测试图片"), QString(),
                tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All files (*.*)"));
    if (fileName.trimmed().isEmpty())
        return;
    const cv::Mat frame = cv::imread(fileName.toLocal8Bit().constData(),
                                     cv::IMREAD_UNCHANGED);
    if (frame.empty()) {
        QMessageBox::warning(this, tr("PC导入图片"), tr("无法读取所选图片"));
        return;
    }
    m_importedTestFrame = frame.clone();
    m_importedTestImageTitle = QFileInfo(fileName).fileName();
    m_importedTestActive = true;
    updateBottomButtons();
    rerunImportedTest();
}

void EdgePresenceDialog::exitTestMode()
{
    m_importedTestActive = false;
    m_importedTestFrame.release();
    m_importedTestImageTitle.clear();
    updateBottomButtons();
    showReferenceImage();
    setViewerStatusText(tr("已退出离线测试，可使用相机执行测试运行"));
}

void EdgePresenceDialog::updateBottomButtons()
{
    const bool imported = m_importedTestActive && !m_importedTestFrame.empty();
    ui->referenceTestButton->setVisible(!imported);
    ui->testRunButton->setText(
                imported ? tr("测试运行（导入图）") : tr("测试运行"));
    ui->finishButton->setText(imported ? tr("运行一次") : tr("完成"));
    if (m_exitTestButton)
        m_exitTestButton->setVisible(imported);
}

void EdgePresenceDialog::rerunImportedTest()
{
    if (!m_importedTestActive || m_importedTestFrame.empty()
            || m_edgePresenceRunning) {
        return;
    }
    showReferenceImage();
    setViewerStatusText(tr("正在重新执行模板定位、位置修正和边缘检测…"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    runCameraTest();
}

void EdgePresenceDialog::applyAdaptiveWindowSize()
{
    PlanDialogUtils::applyLargeWindow(this);
}

void EdgePresenceDialog::fitPreview()
{
    if (m_previewHelper)
        m_previewHelper->fitToView();
}

void EdgePresenceDialog::showReferenceImage()
{
    if (!m_previewHelper)
        return;

    const QImage image = m_importedTestActive && !m_importedTestFrame.empty()
            ? imageFromFrame(m_importedTestFrame)
            : ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        return;
    }

    ui->viewerTitleLabel->setText(
                m_importedTestActive ? m_importedTestImageTitle : tr("基准图"));
    m_previewHelper->setImage(image);
    m_previewHelper->clearToolOverlays();
    m_previewHelper->clearRoi();
    m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
}

void EdgePresenceDialog::showFrameForRoiEditing()
{
    if (!m_previewHelper)
        return;

    QImage image = m_importedTestActive && !m_importedTestFrame.empty()
            ? imageFromFrame(m_importedTestFrame)
            : ReferenceImageProvider::instance().referenceImage();
    QString title = m_importedTestActive
            ? m_importedTestImageTitle : tr("基准图");
    if (image.isNull()) {
        image = CameraFrameProvider::instance().currentImage();
        title = tr("当前图像");
    }

    if (image.isNull()) {
        m_previewHelper->setLineBandDrawingEnabled(false);
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("当前无图像"));
        const QString text = tr("请先设置基准图或提供当前图像后再选择检测区域");
        setViewerStatusText(text, text);
        return;
    }

    ui->viewerTitleLabel->setText(title);
    m_previewHelper->setImage(image);
    m_previewHelper->clearToolOverlays();
    m_previewHelper->clearRoi();
    m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
}

void EdgePresenceDialog::startDetectRoiEditing()
{
    if (!m_previewHelper)
        return;

    showFrameForRoiEditing();
    if (m_previewHelper->imageSize().isEmpty())
        return;

    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    m_previewHelper->setLineBandDrawingEnabled(true);
    const QString text = tr("拖拽中心线，移动鼠标拉出搜索带宽度，再次左键确认");
    setViewerStatusText(text, text);
}

void EdgePresenceDialog::showDetectRoiTodo(const QString &message)
{
    ui->basicDetectionRectButton->setChecked(true);
    ui->detectionRectButton->setChecked(true);
    if (m_previewHelper) {
        m_previewHelper->setLineBandDrawingEnabled(false);
        m_previewHelper->clearRoi();
        m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
    }
    setViewerStatusText(message, message);
}

void EdgePresenceDialog::handleLineBandChanged(const LineBandRoi &roi)
{
    m_lineBandRoi = roi;
    if (m_previewHelper) {
        m_roiNormalized = m_previewHelper->lineBandBoundingRectNormalized(roi);
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearRoi();
        m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
    }
    const QString text = detectRoiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[EdgePresenceDialog] line-band ROI"
             << "p1=" << m_lineBandRoi.p1Normalized
             << "p2=" << m_lineBandRoi.p2Normalized
             << "width=" << m_lineBandRoi.widthNormalized
             << "bounding=" << m_roiNormalized;
    rerunImportedTest();
}

void EdgePresenceDialog::handleLineBandSelectionRejected()
{
    const QString text = tr("线型搜索 ROI 无效，请拖拽一条有效中心线");
    setViewerStatusText(text, text);
    if (m_previewHelper)
        m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
}

void EdgePresenceDialog::handleRoiChanged(const QRectF &roi)
{
    m_roiNormalized = roi;
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
    }
    const QString roiText = detectRoiStatusText();
    setViewerStatusText(roiText, roiText);
    qDebug() << "[EdgePresenceDialog] ROI normalized:" << m_roiNormalized;
    rerunImportedTest();
}

void EdgePresenceDialog::handleRoiSelectionRejected()
{
    const QString text = tr("线型搜索 ROI 无效，请拖拽一条有效中心线");
    setViewerStatusText(text, text);
    if (m_previewHelper)
        m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
}

void EdgePresenceDialog::runEdgePresenceOnFrame(const cv::Mat &frame,
                                                const cv::Mat &referenceImage,
                                                const QString &imageTitle,
                                                const QString &emptyFrameMessage,
                                                bool referenceTest)
{
    if (m_edgePresenceRunning)
        return;

    if (frame.empty()) {
        displayEdgePresenceError(QStringLiteral("image_empty"), emptyFrameMessage);
        return;
    }

    m_edgePresenceRunning = true;

    ToolConfig config = toToolConfig();
    config.roiNormalized = effectiveRoiNormalized();

    ToolRequest request;
    request.config = config;
    request.image = frame.clone();
    request.referenceImage = referenceImage.empty() ? cv::Mat() : referenceImage.clone();

    const ToolResult result = runPositionCorrectionAwareDialogTest(
                m_testToolEngine,
                request.config,
                request.image,
                request.referenceImage,
                &m_toolChainTestContext);
    if (!imageTitle.isEmpty())
        ui->viewerTitleLabel->setText(imageTitle);
    displayEdgePresenceResult(result);
    if (referenceTest)
        m_referencePreviewSnapshot = makeReferenceToolPreviewSnapshot(config, result, effectiveRoiNormalized());

    m_edgePresenceRunning = false;
}

void EdgePresenceDialog::displayEdgePresenceResult(const ToolResult &result)
{
    qDebug() << "[EdgePresenceDialog] ToolResult"
             << "status=" << result.status
             << "message=" << result.message
             << "score=" << result.score
             << "value=" << result.value
             << "count=" << result.count
             << "ok=" << result.ok;

    const QString displayText = result.success
            ? tr("%1 | count:%2 | length:%3 | %4")
              .arg(result.status,
                   QString::number(result.count),
                   QString::number(result.value, 'f', 1),
                   result.ok ? QStringLiteral("OK") : QStringLiteral("NG"))
            : tr("%1 | %2").arg(result.status, result.message);
    setViewerStatusText(displayText, makeEdgePresenceStatusTooltipText(result));

    if (m_previewHelper) {
        m_previewHelper->clearRoi();
        bool hasRuntimeDetectRoi = false;
        for (const ToolOverlay &overlay : result.overlays) {
            if (overlay.extra.value(QStringLiteral("role")).toString()
                    == QStringLiteral("detect_roi")) {
                hasRuntimeDetectRoi = true;
                break;
            }
        }
        if (hasRuntimeDetectRoi)
            m_previewHelper->clearLineBandRoi();
        else
            m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
        m_previewHelper->setToolOverlays(result.overlays);
    }
}

void EdgePresenceDialog::displayEdgePresenceError(const QString &status, const QString &message)
{
    qDebug() << "[EdgePresenceDialog] ToolResult"
             << "status=" << status
             << "message=" << message
             << "score=" << 0.0
             << "value=" << 0.0
             << "count=" << 0
             << "ok=" << false;

    const QString displayText = tr("EdgePresence: %1 | %2").arg(status, message);
    setViewerStatusText(displayText, makeEdgePresenceErrorTooltipText(status, message));
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->clearRoi();
        m_previewHelper->setLineBandRoiNormalized(effectiveLineBandRoi());
    }
}

void EdgePresenceDialog::setViewerStatusText(const QString &displayText, const QString &tooltipText)
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

QString EdgePresenceDialog::detectRoiStatusText() const
{
    const LineBandRoi roi = effectiveLineBandRoi();
    const QRectF bounds = effectiveRoiNormalized();
    return tr("线型 ROI p1=(%1,%2) p2=(%3,%4) width=%5 bounds=(%6,%7,%8,%9)")
            .arg(roi.p1Normalized.x(), 0, 'f', 3)
            .arg(roi.p1Normalized.y(), 0, 'f', 3)
            .arg(roi.p2Normalized.x(), 0, 'f', 3)
            .arg(roi.p2Normalized.y(), 0, 'f', 3)
            .arg(roi.widthNormalized, 0, 'f', 3)
            .arg(bounds.x(), 0, 'f', 3)
            .arg(bounds.y(), 0, 'f', 3)
            .arg(bounds.width(), 0, 'f', 3)
            .arg(bounds.height(), 0, 'f', 3);
}

QRectF EdgePresenceDialog::effectiveRoiNormalized() const
{
    if (m_roiNormalized.width() <= 0.0 || m_roiNormalized.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    const QRectF roi = m_roiNormalized.normalized().intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    if (roi.width() <= 0.0 || roi.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    return roi;
}

LineBandRoi EdgePresenceDialog::effectiveLineBandRoi() const
{
    return validLineBandRoi(m_lineBandRoi) ? m_lineBandRoi : defaultLineBandRoi();
}
