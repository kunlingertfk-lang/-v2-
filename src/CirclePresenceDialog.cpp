#include "CirclePresenceDialog.h"
#include "ui_CirclePresenceDialog.h"

#include "PlanDialogUtils.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QTimer>
#include <QUuid>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameInputMetadata.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolRequest.h"

namespace {

constexpr int kComboFullTextRole = Qt::UserRole + 100;

QString normalizedSourceText(QString text)
{
    const QString unavailablePrefix = QObject::tr("来源不可用：");
    text = text.trimmed();
    while (text.startsWith(unavailablePrefix + unavailablePrefix))
        text.remove(0, unavailablePrefix.size());
    return text;
}

void resetScrollAreaToTop(QScrollArea *scrollArea)
{
    if (!scrollArea)
        return;

    QTimer::singleShot(0, scrollArea, [scrollArea]() {
        if (QScrollBar *scrollBar = scrollArea->verticalScrollBar())
            scrollBar->setValue(scrollBar->minimum());
    });
}

QString comboSelectedFullText(const QComboBox *comboBox)
{
    if (!comboBox)
        return QString();
    const int index = comboBox->currentIndex();
    if (index < 0)
        return comboBox->currentText();
    const QString fullText = comboBox->itemData(index, kComboFullTextRole).toString();
    return fullText.isEmpty() ? comboBox->itemText(index) : fullText;
}

void updateComboToolTip(QComboBox *comboBox)
{
    if (!comboBox || comboBox->currentIndex() < 0)
        return;

    const QString fullText = comboSelectedFullText(comboBox);
    comboBox->setToolTip(fullText);
}

void configureSourceCombo(QComboBox *comboBox)
{
    if (!comboBox)
        return;
    comboBox->setEditable(false);
    for (int index = 0; index < comboBox->count(); ++index)
        comboBox->setItemData(index, comboBox->itemText(index), kComboFullTextRole);
    QObject::connect(comboBox,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     comboBox,
                     [comboBox]() { updateComboToolTip(comboBox); });
    updateComboToolTip(comboBox);
}

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

    int index = -1;
    for (int i = 0; i < comboBox->count(); ++i) {
        if (comboBox->itemData(i, kComboFullTextRole).toString() == value) {
            index = i;
            break;
        }
    }
    if (index < 0)
        index = comboBox->findText(value);
    if (index >= 0)
        comboBox->setCurrentIndex(index);
}

void populatePositionCorrectionCombo(
        QComboBox *comboBox,
        const QVector<PositionCorrectionSource> &sources,
        const QString &selectedId,
        const QString &selectedText)
{
    if (!comboBox)
        return;

    const QSignalBlocker blocker(comboBox);
    comboBox->clear();

    int selectedIndex = -1;
    for (const PositionCorrectionSource &source : sources) {
        const QString displayText = normalizedSourceText(source.displayText);
        comboBox->addItem(displayText, source.sourceId);
        comboBox->setItemData(comboBox->count() - 1,
                              displayText,
                              kComboFullTextRole);
        if (source.sourceId == selectedId)
            selectedIndex = comboBox->count() - 1;
    }

    if (selectedIndex < 0 && !selectedId.trimmed().isEmpty()) {
        const QString unavailableSource = normalizedSourceText(
                    selectedText.trimmed().isEmpty() ? selectedId : selectedText);
        const QString unavailablePrefix = QObject::tr("来源不可用：");
        const QString unavailableText = unavailableSource.startsWith(unavailablePrefix)
                ? unavailableSource
                : unavailablePrefix + unavailableSource;
        comboBox->insertItem(0, unavailableText, selectedId);
        comboBox->setItemData(0, unavailableText, kComboFullTextRole);
        selectedIndex = 0;
    }

    if (selectedIndex < 0 && comboBox->count() > 0)
        selectedIndex = 0;

    if (selectedIndex >= 0)
        comboBox->setCurrentIndex(selectedIndex);
    updateComboToolTip(comboBox);
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
    m_testToolEngine.registerAdapter(&m_testTemplateLocationAdapter);
    m_testToolEngine.registerAdapter(&m_testPositionCorrectionAdapter);
    m_testToolEngine.registerAdapter(&m_testCirclePresenceAdapter);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);
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
    config.positionCorrectionSource = comboSelectedFullText(
                basicMode ? ui->basicPositionCorrectionComboBox
                          : ui->positionCorrectionComboBox);

    config.positionCorrectionSourceId = basicMode
            ? ui->basicPositionCorrectionComboBox->currentData().toString().trimmed()
            : ui->positionCorrectionComboBox->currentData().toString().trimmed();

    if (config.positionCorrectionSourceId.isEmpty())
        config.positionCorrectionSourceId = m_loadedPositionCorrectionSourceId;
    config.showPositionCorrectionMatchContour = m_showPositionCorrectionMatchContour;

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
    PositionCorrectionConfig correctionConfig;
    correctionConfig.enabled = circleConfig.enablePositionCorrection;
    correctionConfig.source = circleConfig.positionCorrectionSource;
    correctionConfig.sourceId = circleConfig.positionCorrectionSourceId;
    PositionCorrection::writeParams(correctionConfig, &params);
    params.insert(QStringLiteral("showPositionCorrectionMatchContour"),
                    circleConfig.showPositionCorrectionMatchContour);
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
    const PositionCorrectionConfig correctionConfig = PositionCorrection::fromParams(params);

    m_loadedPositionCorrectionSourceId = correctionConfig.sourceId;
    m_showPositionCorrectionMatchContour = params.value(QStringLiteral("showPositionCorrectionMatchContour")).toBool(true);

    const bool allMode = params.contains(QStringLiteral("roundness"))
            || params.value(QStringLiteral("edgePolarity")).toString() != QStringLiteral("any")
            || params.value(QStringLiteral("edgeType")).toString() != QStringLiteral("strongest");
    ui->basicSegmentButton->setChecked(!allMode);
    ui->allSegmentButton->setChecked(allMode);
    ui->circleParamsStackedWidget->setCurrentWidget(allMode ? ui->allParamsPage : ui->basicParamsPage);

    const bool positionCorrection = correctionConfig.enabled;
    ui->basicPositionCorrectionSwitch->setChecked(positionCorrection);
    ui->positionCorrectionSwitch->setChecked(positionCorrection);
    setComboBoxValue(ui->basicPositionCorrectionComboBox, correctionConfig.source);
    setComboBoxValue(ui->positionCorrectionComboBox, correctionConfig.source);
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

void CirclePresenceDialog::setToolChainTestContext(
        const QVector<ToolConfig> &toolConfigs,
        int currentToolIndex,
        ToolEngine *sharedToolEngine,
        const ReferencePositionCorrectionConfig &referencePositionCorrection)
{
    m_toolChainTestConfigs = toolConfigs;
    m_toolChainTestIndex =
            qBound(0, currentToolIndex, toolConfigs.size());
    m_sharedToolEngine = sharedToolEngine;
    m_referencePositionCorrection = referencePositionCorrection;

    const CirclePresenceConfig current = configuration();
    const QString selectedId =
            current.positionCorrectionSourceId.trimmed().isEmpty()
            ? m_loadedPositionCorrectionSourceId
            : current.positionCorrectionSourceId;

    const QVector<PositionCorrectionSource> sources =
            PositionCorrection::sourcesBefore(
                m_toolChainTestConfigs,
                m_toolChainTestIndex,
                m_referencePositionCorrection.enabled);

    populatePositionCorrectionCombo(
                ui->basicPositionCorrectionComboBox,
                sources,
                selectedId,
                current.positionCorrectionSource);
    populatePositionCorrectionCombo(
                ui->positionCorrectionComboBox,
                sources,
                selectedId,
                current.positionCorrectionSource);
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
    m_exitTestButton = new QPushButton(tr("退出测试"), this);
    m_exitTestButton->setObjectName(QStringLiteral("exitTestButton"));
    m_exitTestButton->setMinimumSize(120, 48);
    m_exitTestButton->setProperty(
            "actionRole",
            QStringLiteral("secondary"));
    ui->horizontalLayout_actions->addWidget(m_exitTestButton);

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
    configureSourceCombo(ui->basicPositionCorrectionComboBox);
    configureSourceCombo(ui->positionCorrectionComboBox);
    ui->basicGridLayout_position->setAlignment(
                ui->basicPositionCorrectionSwitch, Qt::AlignRight);
    ui->basicGridLayout_position->setAlignment(
                ui->basicPositionCorrectionComboBox, Qt::AlignRight);
    ui->gridLayout_position->setAlignment(
                ui->positionCorrectionSwitch, Qt::AlignRight);
    ui->gridLayout_position->setAlignment(
                ui->positionCorrectionComboBox, Qt::AlignRight);
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
    ui->basicSegmentButton->setFocus(Qt::OtherFocusReason);
    resetScrollAreaToTop(ui->circleBasicParamsScrollArea);
    resetScrollAreaToTop(ui->circleAllParamsScrollArea);
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
    updateBottomButtons();
}

void CirclePresenceDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &CirclePresenceDialog::reject);
    connect(ui->referenceTestButton, &QPushButton::clicked, this, &CirclePresenceDialog::runReferenceTest);
    connect(ui->testRunButton, &QPushButton::clicked, this, &CirclePresenceDialog::runCameraTest);
    connect(ui->circlePcImportButton,
            &QPushButton::clicked,
            this,
            &CirclePresenceDialog::importTestImageFromPc);
    connect(m_exitTestButton,
            &QPushButton::clicked,
            this,
            &CirclePresenceDialog::exitTestMode);
    connect(ui->finishButton, &QPushButton::clicked, this, &CirclePresenceDialog::finishConfiguration);

    m_segmentGroup->setExclusive(true);
    m_segmentGroup->addButton(ui->basicSegmentButton, 0);
    m_segmentGroup->addButton(ui->allSegmentButton, 1);
    connect(ui->basicSegmentButton, &QPushButton::clicked, this, [this]() {
        ui->circleParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
        resetScrollAreaToTop(ui->circleBasicParamsScrollArea);
    });
    connect(ui->allSegmentButton, &QPushButton::clicked, this, [this]() {
        ui->circleParamsStackedWidget->setCurrentWidget(ui->allParamsPage);
        resetScrollAreaToTop(ui->circleAllParamsScrollArea);
    });
    connect(ui->basicPositionCorrectionSwitch, &QCheckBox::toggled,
            ui->positionCorrectionSwitch, &QCheckBox::setChecked);
    connect(ui->positionCorrectionSwitch, &QCheckBox::toggled,
            ui->basicPositionCorrectionSwitch, &QCheckBox::setChecked);

    const auto syncPositionCorrectionSource =  [](QComboBox *source,QComboBox *target, int index){
        if(!source || !target || index < 0)
            return;
        const QString sourceId = source->itemData(index).toString();
        const int targetIndex = target->findData(sourceId);
        if(targetIndex >= 0 && targetIndex != target->currentIndex())
            target->setCurrentIndex(targetIndex);
    };

    connect(ui->basicPositionCorrectionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, syncPositionCorrectionSource](int index){
        syncPositionCorrectionSource(ui->basicPositionCorrectionComboBox, ui->positionCorrectionComboBox, index);
    });
    connect(ui->positionCorrectionComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, syncPositionCorrectionSource](int index){
        syncPositionCorrectionSource(ui->positionCorrectionComboBox, ui->basicPositionCorrectionComboBox, index);
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

    if (m_importedTestActive) {
        rerunImportedTest();
        return;
    }
    accept();
}

void CirclePresenceDialog::runReferenceTest()
{
    const ReferenceFrameSetSnapshot referenceSet =
            ReferenceImageProvider::instance().referenceFrameSetSnapshot();
    const cv::Mat referenceImage = referenceSet.primary.frame;
    if (referenceImage.empty()) {
        displayCirclePresenceError(QStringLiteral("no_reference_image"),
                                   tr("基准图为空，无法测试"));
        return;
    }

    const QImage image = imageFromFrame(referenceImage);
    if (!image.isNull() && m_previewHelper) {
        ui->viewerTitleLabel->setText(tr("基准图"));
        m_previewHelper->setImage(image);
        refreshDisplayedRoiOverlay();
    }

    runCirclePresenceOnFrame(referenceImage.clone(),
                             tr("基准图"),
                             tr("基准图为空，无法测试"),
                             true,
                             &referenceSet);
}

void CirclePresenceDialog::runCameraTest()
{
    const bool useImportedFrame =
            m_importedTestActive && !m_importedTestFrame.empty();

    const cv::Mat frame = useImportedFrame
            ? m_importedTestFrame.clone()
            : CameraFrameProvider::instance().currentFrame();

    if (frame.empty()) {
        displayCirclePresenceError(
                QStringLiteral("image_empty"),
                tr("当前图像为空，无法测试"));
        return;
    }

    const cv::Mat snapshot = frame.clone();
    const QImage image = imageFromFrame(snapshot);

    const QString imageTitle = useImportedFrame
            ? (m_importedTestImageTitle.trimmed().isEmpty()
                       ? tr("PC导入图片")
                       : m_importedTestImageTitle)
            : tr("测试图像");

    if (!image.isNull() && m_previewHelper) {
        ui->viewerTitleLabel->setText(imageTitle);
        m_previewHelper->setImage(image);
        refreshDisplayedRoiOverlay();
    }

    runCirclePresenceOnFrame(
            snapshot,
            imageTitle,
            tr("当前图像为空，无法测试"));
}

void CirclePresenceDialog::importTestImageFromPc()
{
    const QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("PC导入测试图片"),
            QString(),
            tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All files (*.*)"));

    if (fileName.trimmed().isEmpty())
        return;

    const cv::Mat frame = cv::imread(
            fileName.toLocal8Bit().constData(),
            cv::IMREAD_UNCHANGED);

    if (frame.empty()) {
        QMessageBox::warning(
                this,
                tr("PC导入图片"),
                tr("无法读取所选图片"));
        return;
    }

    m_importedTestFrame = frame.clone();
    m_importedTestImageTitle = QFileInfo(fileName).fileName();
    m_importedTestActive = true;
    updateBottomButtons();

    const QImage image = imageFromFrame(m_importedTestFrame);
    if (!image.isNull() && m_previewHelper) {
        ui->viewerTitleLabel->setText(m_importedTestImageTitle);
        m_previewHelper->setImage(image);
        refreshDisplayedRoiOverlay();
    }

    runCirclePresenceOnFrame(
            m_importedTestFrame,
            m_importedTestImageTitle,
            tr("导入图片为空，无法测试"));
}

void CirclePresenceDialog::exitTestMode()
{
    m_importedTestActive = false;
    m_importedTestFrame.release();
    m_importedTestImageTitle.clear();

    updateBottomButtons();
    showReferenceImage();
    setViewerStatusText(tr("已退出离线测试，可使用相机执行测试运行"));
}

void CirclePresenceDialog::updateBottomButtons()
{
    const bool imported =
            m_importedTestActive && !m_importedTestFrame.empty();

    ui->referenceTestButton->setVisible(!imported);
    ui->finishButton->setText(imported ? tr("运行一次") : tr("完成"));
    ui->testRunButton->setText(
            imported ? tr("测试运行（导入图）") : tr("测试运行"));
    ui->testRunButton->setToolTip(
            imported
                    ? tr("重新填充并测试当前 PC 导入图片：%1")
                              .arg(m_importedTestImageTitle)
                    : QString());

    if (m_exitTestButton)
        m_exitTestButton->setVisible(imported);
}

void CirclePresenceDialog::rerunImportedTest()
{
    if (!m_importedTestActive || m_importedTestFrame.empty())
        return;

    runCameraTest();
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
    if (!m_previewHelper || m_importedTestActive)
        return;

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        return;
    }

    ui->viewerTitleLabel->setText(tr("基准图"));
    m_previewHelper->setImage(image);
    m_previewHelper->clearToolOverlays();
    refreshDisplayedRoiOverlay();
}

void CirclePresenceDialog::showFrameForRoiEditing()
{
    if (!m_previewHelper)
        return;

    QImage image;
    QString title;

    if (m_importedTestActive && !m_importedTestFrame.empty()) {
        image = imageFromFrame(m_importedTestFrame);
        title = m_importedTestImageTitle.trimmed().isEmpty()
                ? tr("PC导入图片")
                : m_importedTestImageTitle;
    } else {
        image = ReferenceImageProvider::instance().referenceImage();
        title = tr("基准图");
    }

    if (image.isNull() && !m_importedTestActive) {
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
    rerunImportedTest();
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
    rerunImportedTest();
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
    rerunImportedTest();
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
    rerunImportedTest();
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
                                                    const QString &imageTitle,
                                                    const QString &emptyFrameMessage,
                                                    bool referenceTest,
                                                    const ReferenceFrameSetSnapshot *capturedReferenceSet)
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
    config.enabled = true;

    const PositionCorrectionConfig correctionConfig =
        PositionCorrection::fromParams(config.params);

    const QString frameId = QStringLiteral("circle-dialog-test-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    const QString inputSource = referenceTest
        ? QStringLiteral("reference")
        : (m_importedTestActive
                   ? QStringLiteral("file")
                   : QStringLiteral("camera"));

    const FrameInputMetadata inputMetadata =
        FrameInputMetadata::fromMat(frame, inputSource);

    QJsonObject runtimeContext;
    runtimeContext.insert(QStringLiteral("frameId"), frameId);
    runtimeContext.insert(QStringLiteral("input"), inputMetadata.toJson());
    const ReferenceFrameSetSnapshot ownedReferenceSet = capturedReferenceSet
            ? ReferenceFrameSetSnapshot()
            : ReferenceImageProvider::instance().referenceFrameSetSnapshot();
    const ReferenceFrameSetSnapshot &referenceSet = capturedReferenceSet
            ? *capturedReferenceSet : ownedReferenceSet;
    const cv::Mat referenceImage = referenceSet.primary.frame;
    runtimeContext.insert(QStringLiteral("referenceInput"),
                          referenceSet.primary.metadata.toJson());
    runtimeContext.insert(
        QStringLiteral("referencePositionCorrection"),
        PositionCorrection::referenceToJson(m_referencePositionCorrection));
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        refreshDisplayedRoiOverlay();
    }
    setViewerStatusText(
        correctionConfig.enabled
                ? tr("正在重新执行模板定位、位置修正和圆检测…")
                : tr("正在重新执行圆检测…"));

    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    if (correctionConfig.enabled
        && (m_toolChainTestIndex < 0
            || m_toolChainTestIndex > m_toolChainTestConfigs.size())) {
        m_circlePresenceRunning = false;
        displayCirclePresenceError(
            QStringLiteral("position_correction_test_context_missing"),
            tr("位置修正测试缺少当前方案的前置工具配置"));
        return;
    }

    QVector<ToolConfig> testConfigs;

    if (correctionConfig.enabled) {
        testConfigs.reserve(m_toolChainTestIndex + 1);

        for (int index = 0; index < m_toolChainTestIndex; ++index)
            testConfigs.append(m_toolChainTestConfigs.at(index));

        testConfigs.append(config);
    }
    ToolResult result;

    if (correctionConfig.enabled) {
        ToolResult referenceCorrectionResult;

        ToolEngine *testEngine = m_sharedToolEngine
                ? m_sharedToolEngine
                : &m_testToolEngine;

        const QVector<ToolResult> results =
                testEngine->runTools(
                        testConfigs,
                        frame.clone(),
                        referenceImage.empty()
                                ? cv::Mat()
                                : referenceImage.clone(),
                        runtimeContext,
                        &referenceCorrectionResult,
                        referenceSet.frames,
                        referenceSet.contentRevisions,
                        referenceSet.primaryBaseId);

        for (auto it = results.crbegin(); it != results.crend(); ++it) {
            if (it->toolId == config.toolId) {
                result = *it;
                break;
            }
        }
        if (result.toolId.isEmpty()) {
            result = ToolResult::error(
                    config.toolId,
                    config.toolType,
                    tr("前置工具链没有返回圆检测结果"),
                    QStringLiteral("tool_chain_result_missing"));
        }
    } else {
        ToolRequest request;
        request.requestId =
                QUuid::createUuid().toString(QUuid::WithoutBraces);
        request.frameId = frameId;
        request.config = config;
        request.image = frame.clone();
        request.referenceImage =
                referenceImage.empty()
                        ? cv::Mat()
                        : referenceImage.clone();
        request.referenceImages = referenceSet.frames;
        request.referenceImageRevisions = referenceSet.contentRevisions;
        request.primaryReferenceBaseId = referenceSet.primaryBaseId;
        request.runtimeContext = runtimeContext;

        result = m_testToolEngine.runTool(request);
    }
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
        bool hasRuntimeDetectRoi = false;

        for (const ToolOverlay &overlay : result.overlays) {
            const QString role =
                    overlay.extra.value(QStringLiteral("role")).toString();
            const QString label = overlay.label.trimmed().toLower();

            if (role == QStringLiteral("detect_roi")
                    || label == QStringLiteral("detect_roi")
                    || label == QStringLiteral("detection roi")) {
                hasRuntimeDetectRoi = true;
                break;
            }
        }

        // 位置修正后的动态检测区域由 Runner 返回。
        // 此时清除配置阶段的原始 ROI，避免同时显示两套检测区域。
        if (hasRuntimeDetectRoi) {
            m_previewHelper->clearRoi();
            m_previewHelper->clearPolygonRoi();
            m_previewHelper->clearCircleRoi();
        } else {
            refreshDisplayedRoiOverlay();
        }

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
