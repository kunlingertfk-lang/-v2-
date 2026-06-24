#include "ObjectDetectionDialog.h"
#include "ui_ObjectDetectionDialog.h"

#include "PlanDialogUtils.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QWidget>
#include <QUuid>
#include <QtGlobal>

#include <cmath>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"

namespace {

QString judgeModeFromResultBasis(const QString &resultBasis)
{
    if (resultBasis == QObject::tr("数量判断"))
        return QStringLiteral("count");
    if (resultBasis == QObject::tr("最低得分"))
        return QStringLiteral("score");
    if (resultBasis == QObject::tr("类别判断"))
        return QStringLiteral("category");
    return QStringLiteral("unknown");
}

QString resultBasisFromJudgeMode(const QString &mode)
{
    if (mode == QStringLiteral("count"))
        return QObject::tr("数量判断");
    if (mode == QStringLiteral("score"))
        return QObject::tr("最低得分");
    if (mode == QStringLiteral("category"))
        return QObject::tr("类别判断");
    return QString();
}

QString detectRegionTypeFromButtons(const bool rectangleChecked)
{
    Q_UNUSED(rectangleChecked)
    return QStringLiteral("rectangle");
}

void setComboBoxValue(QComboBox *comboBox, const QString &value)
{
    if (!comboBox || value.trimmed().isEmpty())
        return;

    const int index = comboBox->findText(value);
    if (index >= 0) {
        comboBox->setCurrentIndex(index);
        return;
    }

    comboBox->addItem(value);
    comboBox->setCurrentIndex(comboBox->count() - 1);
}

bool finiteValue(const qreal value)
{
    return std::isfinite(static_cast<double>(value));
}

bool finiteRect(const QRectF &rect)
{
    return finiteValue(rect.x()) &&
           finiteValue(rect.y()) &&
           finiteValue(rect.width()) &&
           finiteValue(rect.height());
}

QRectF normalizedRoiOrDefault(const QRectF &roi)
{
    if (!finiteRect(roi) || roi.width() <= 0.0 || roi.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

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

} // namespace

ObjectDetectionDialog::ObjectDetectionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ObjectDetectionDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_regionGroup(new QButtonGroup(this))
    , m_allRegionGroup(new QButtonGroup(this))
    , m_toolId(QStringLiteral("ai_detection_%1")
                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    ui->setupUi(this);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    connectControls();
    showPreviewImage();
}

ObjectDetectionDialog::~ObjectDetectionDialog()
{
    delete ui;
}

ObjectDetectionConfig ObjectDetectionDialog::configuration() const
{
    const bool allMode = ui->objectDetectionParamsStackedWidget->currentWidget() == ui->allParamsPage;

    ObjectDetectionConfig config;
    config.independentPositionCorrection = allMode ? ui->allPositionCorrectionSwitch->isChecked()
                                                   : ui->positionCorrectionSwitch->isChecked();
    config.positionCorrection = allMode ? ui->allPositionCorrectionComboBox->currentText()
                                        : ui->positionCorrectionComboBox->currentText();
    config.modelName = allMode ? ui->allModelComboBox->currentText()
                               : ui->modelComboBox->currentText();
    config.maxFindCount = ui->maxFindCountSpinBox->value();
    config.minScore = ui->detectMinScoreSpinBox->value();
    config.maxOverlap = ui->maxOverlapSpinBox->value();
    config.sortType = ui->sortTypeComboBox->currentText();
    config.angleEnabled = ui->angleEnableSwitch->isChecked();
    config.minAngle = ui->minAngleSpinBox->value();
    config.maxAngle = ui->maxAngleSpinBox->value();
    config.widthEnabled = ui->widthEnableSwitch->isChecked();
    config.minWidth = ui->minWidthSpinBox->value();
    config.maxWidth = ui->maxWidthSpinBox->value();
    config.heightEnabled = ui->heightEnableSwitch->isChecked();
    config.minHeight = ui->minHeightSpinBox->value();
    config.maxHeight = ui->maxHeightSpinBox->value();
    config.boundaryFilterEnabled = ui->boundaryEnableSwitch->isChecked();
    config.overlapRatio = ui->overlapRatioSpinBox->value();
    config.classFilterEnabled = ui->classFilterSwitch->isChecked();
    config.classFilter = ui->classFilterLineEdit->text();
    config.resultBasis = allMode ? ui->allResultBasisComboBox->currentText()
                                 : ui->resultBasisComboBox->currentText();
    config.minCount = allMode ? ui->allMinCountSpinBox->value()
                              : ui->minCountSpinBox->value();
    config.maxCount = allMode ? ui->allMaxCountSpinBox->value()
                              : ui->maxCountSpinBox->value();
    config.resultMinScore = allMode ? ui->allMinScoreSpinBox->value()
                                    : ui->minScoreSpinBox->value();
    config.category = allMode ? ui->allCategoryLineEdit->text()
                              : ui->categoryLineEdit->text();
    return config;
}

ToolConfig ObjectDetectionDialog::toToolConfig() const
{
    const bool allMode = ui->objectDetectionParamsStackedWidget->currentWidget() == ui->allParamsPage;
    const ObjectDetectionConfig detectionConfig = configuration();
    const QString judgeMode = judgeModeFromResultBasis(detectionConfig.resultBasis);
    const int nmsIouThreshold = detectionConfig.maxOverlap;

    QJsonObject params;
    params.insert(QStringLiteral("paramMode"), allMode ? QStringLiteral("all") : QStringLiteral("basic"));
    params.insert(QStringLiteral("positionCorrectionEnabled"), detectionConfig.independentPositionCorrection);
    params.insert(QStringLiteral("positionCorrectionSource"), detectionConfig.positionCorrection);
    params.insert(QStringLiteral("detectRegionType"),
                  allMode ? detectRegionTypeFromButtons(ui->allRegionRectButton->isChecked())
                          : detectRegionTypeFromButtons(ui->regionRectButton->isChecked()));
    params.insert(QStringLiteral("modelName"), detectionConfig.modelName);
    params.insert(QStringLiteral("maxDetections"), detectionConfig.maxFindCount);
    params.insert(QStringLiteral("confidenceThreshold"), detectionConfig.minScore);
    params.insert(QStringLiteral("detectMinScore"), detectionConfig.minScore);
    params.insert(QStringLiteral("maxOverlap"), detectionConfig.maxOverlap);
    params.insert(QStringLiteral("nmsIouThreshold"), nmsIouThreshold);
    params.insert(QStringLiteral("sortMode"), detectionConfig.sortType);
    params.insert(QStringLiteral("angleFilterEnabled"), detectionConfig.angleEnabled);
    params.insert(QStringLiteral("minAngle"), detectionConfig.minAngle);
    params.insert(QStringLiteral("maxAngle"), detectionConfig.maxAngle);
    params.insert(QStringLiteral("widthFilterEnabled"), detectionConfig.widthEnabled);
    params.insert(QStringLiteral("minWidth"), detectionConfig.minWidth);
    params.insert(QStringLiteral("maxWidth"), detectionConfig.maxWidth);
    params.insert(QStringLiteral("heightFilterEnabled"), detectionConfig.heightEnabled);
    params.insert(QStringLiteral("minHeight"), detectionConfig.minHeight);
    params.insert(QStringLiteral("maxHeight"), detectionConfig.maxHeight);
    params.insert(QStringLiteral("boundaryFilterEnabled"), detectionConfig.boundaryFilterEnabled);
    params.insert(QStringLiteral("boundaryOverlapRatio"), detectionConfig.overlapRatio);
    params.insert(QStringLiteral("classFilterEnabled"), detectionConfig.classFilterEnabled);
    params.insert(QStringLiteral("classFilterText"), detectionConfig.classFilter);
    params.insert(QStringLiteral("showBoxes"), m_showBoxes);
    params.insert(QStringLiteral("showLabels"), m_showLabels);
    params.insert(QStringLiteral("showScores"), m_showScores);

    QJsonObject judgeRule;
    judgeRule.insert(QStringLiteral("mode"), judgeMode);
    judgeRule.insert(QStringLiteral("resultBasis"), detectionConfig.resultBasis);
    judgeRule.insert(QStringLiteral("minCount"), detectionConfig.minCount);
    judgeRule.insert(QStringLiteral("maxCount"), detectionConfig.maxCount);
    judgeRule.insert(QStringLiteral("minScore"), detectionConfig.resultMinScore);
    judgeRule.insert(QStringLiteral("category"), detectionConfig.category);

    ToolConfig config;
    config.toolId = m_toolId;
    config.toolName = tr("目标检测");
    config.toolType = ToolType::AiDetection;
    config.category = ToolCategory::DeepLearning;
    config.enabled = m_enabled;
    config.roiNormalized = effectiveRoiNormalized();
    config.params = params;
    config.judgeRule = judgeRule;
    config.displayName = tr("目标检测");
    config.summary = summaryText();
    return config;
}

ToolConfig ObjectDetectionDialog::toolConfig() const
{
    return toToolConfig();
}

ToolPreviewSnapshot ObjectDetectionDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void ObjectDetectionDialog::loadFromConfig(const ToolConfig &config)
{
    if (!config.toolId.trimmed().isEmpty())
        m_toolId = config.toolId;
    m_enabled = config.enabled;
    m_roiNormalized = normalizedRoiOrDefault(config.roiNormalized);

    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;
    const bool allMode = params.value(QStringLiteral("paramMode")).toString() == QStringLiteral("all");
    setAllParamsMode(allMode);

    const QString detectRegionType = params.value(QStringLiteral("detectRegionType")).toString();
    const bool rectangleRegion = detectRegionType.isEmpty() ||
            detectRegionType == QStringLiteral("rectangle") ||
            detectRegionType == QStringLiteral("rect");
    syncRegionButtons(rectangleRegion);

    const bool positionCorrectionEnabled = params.value(QStringLiteral("positionCorrectionEnabled")).toBool(
                ui->positionCorrectionSwitch->isChecked());
    ui->positionCorrectionSwitch->setChecked(positionCorrectionEnabled);
    ui->allPositionCorrectionSwitch->setChecked(positionCorrectionEnabled);
    const QString positionCorrectionSource = params.value(QStringLiteral("positionCorrectionSource")).toString();
    setComboBoxValue(ui->positionCorrectionComboBox, positionCorrectionSource);
    setComboBoxValue(ui->allPositionCorrectionComboBox, positionCorrectionSource);

    const QString modelName = params.value(QStringLiteral("modelName")).toString();
    setComboBoxValue(ui->modelComboBox, modelName);
    setComboBoxValue(ui->allModelComboBox, modelName);

    ui->maxFindCountSpinBox->setValue(params.value(QStringLiteral("maxDetections")).toInt(ui->maxFindCountSpinBox->value()));
    ui->detectMinScoreSpinBox->setValue(params.value(QStringLiteral("confidenceThreshold")).toInt(
                                            params.value(QStringLiteral("detectMinScore")).toInt(ui->detectMinScoreSpinBox->value())));
    ui->maxOverlapSpinBox->setValue(params.value(QStringLiteral("maxOverlap")).toInt(
                                        params.value(QStringLiteral("nmsIouThreshold")).toInt(ui->maxOverlapSpinBox->value())));
    setComboBoxValue(ui->sortTypeComboBox, params.value(QStringLiteral("sortMode")).toString());
    ui->angleEnableSwitch->setChecked(params.value(QStringLiteral("angleFilterEnabled")).toBool(ui->angleEnableSwitch->isChecked()));
    ui->minAngleSpinBox->setValue(params.value(QStringLiteral("minAngle")).toInt(ui->minAngleSpinBox->value()));
    ui->maxAngleSpinBox->setValue(params.value(QStringLiteral("maxAngle")).toInt(ui->maxAngleSpinBox->value()));
    ui->widthEnableSwitch->setChecked(params.value(QStringLiteral("widthFilterEnabled")).toBool(ui->widthEnableSwitch->isChecked()));
    ui->minWidthSpinBox->setValue(params.value(QStringLiteral("minWidth")).toInt(ui->minWidthSpinBox->value()));
    ui->maxWidthSpinBox->setValue(params.value(QStringLiteral("maxWidth")).toInt(ui->maxWidthSpinBox->value()));
    ui->heightEnableSwitch->setChecked(params.value(QStringLiteral("heightFilterEnabled")).toBool(ui->heightEnableSwitch->isChecked()));
    ui->minHeightSpinBox->setValue(params.value(QStringLiteral("minHeight")).toInt(ui->minHeightSpinBox->value()));
    ui->maxHeightSpinBox->setValue(params.value(QStringLiteral("maxHeight")).toInt(ui->maxHeightSpinBox->value()));
    ui->boundaryEnableSwitch->setChecked(params.value(QStringLiteral("boundaryFilterEnabled")).toBool(ui->boundaryEnableSwitch->isChecked()));
    ui->overlapRatioSpinBox->setValue(params.value(QStringLiteral("boundaryOverlapRatio")).toInt(ui->overlapRatioSpinBox->value()));
    ui->classFilterSwitch->setChecked(params.value(QStringLiteral("classFilterEnabled")).toBool(ui->classFilterSwitch->isChecked()));
    ui->classFilterLineEdit->setText(params.value(QStringLiteral("classFilterText")).toString(ui->classFilterLineEdit->text()));
    m_showBoxes = params.value(QStringLiteral("showBoxes")).toBool(m_showBoxes);
    m_showLabels = params.value(QStringLiteral("showLabels")).toBool(m_showLabels);
    m_showScores = params.value(QStringLiteral("showScores")).toBool(m_showScores);

    QString resultBasis = judgeRule.value(QStringLiteral("resultBasis")).toString();
    if (resultBasis.trimmed().isEmpty())
        resultBasis = resultBasisFromJudgeMode(judgeRule.value(QStringLiteral("mode")).toString());
    setComboBoxValue(ui->resultBasisComboBox, resultBasis);
    setComboBoxValue(ui->allResultBasisComboBox, resultBasis);
    const int basisIndex = ui->resultBasisComboBox->currentIndex();
    ui->resultBasisStackedWidget->setCurrentIndex(basisIndex);
    ui->allResultBasisStackedWidget->setCurrentIndex(basisIndex);

    const int minCount = judgeRule.value(QStringLiteral("minCount")).toInt(ui->minCountSpinBox->value());
    const int maxCount = judgeRule.value(QStringLiteral("maxCount")).toInt(ui->maxCountSpinBox->value());
    const int minScore = judgeRule.value(QStringLiteral("minScore")).toInt(ui->minScoreSpinBox->value());
    const QString category = judgeRule.value(QStringLiteral("category")).toString(ui->categoryLineEdit->text());
    ui->minCountSpinBox->setValue(minCount);
    ui->maxCountSpinBox->setValue(maxCount);
    ui->minScoreSpinBox->setValue(minScore);
    ui->categoryLineEdit->setText(category);
    ui->allMinCountSpinBox->setValue(minCount);
    ui->allMaxCountSpinBox->setValue(maxCount);
    ui->allMinScoreSpinBox->setValue(minScore);
    ui->allCategoryLineEdit->setText(category);

    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    showPreviewImage();
    const QString roiText = roiStatusText();
    setViewerStatusText(roiText, roiText);
}

QString ObjectDetectionDialog::summaryText() const
{
    const ObjectDetectionConfig config = configuration();

    if (config.resultBasis == tr("最低得分")) {
        return tr("%1：%2；排序：%3")
            .arg(config.resultBasis)
            .arg(config.resultMinScore)
            .arg(config.sortType);
    }

    if (config.resultBasis == tr("类别判断")) {
        return tr("%1：%2；排序：%3")
            .arg(config.resultBasis)
            .arg(config.category)
            .arg(config.sortType);
    }

    return tr("%1：%2-%3；排序：%4")
        .arg(config.resultBasis)
        .arg(config.minCount)
        .arg(config.maxCount)
        .arg(config.sortType);
}

void ObjectDetectionDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void ObjectDetectionDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 目标检测"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(this);

    ui->basicSegmentButton->setChecked(true);
    ui->allSegmentButton->setChecked(false);
    syncRegionButtons(true);
    ui->sortTypeComboBox->setCurrentIndex(4);
    ui->objectDetectionParamsStackedWidget->setCurrentWidget(ui->basicParamsPage);
    ui->resultBasisStackedWidget->setCurrentIndex(ui->resultBasisComboBox->currentIndex());
    ui->allResultBasisStackedWidget->setCurrentIndex(ui->allResultBasisComboBox->currentIndex());
    ui->regionDrawButton->setToolTip(tr("暂未接入自由区域，当前支持矩形区域"));
    ui->allRegionDrawButton->setToolTip(tr("暂未接入自由区域，当前支持矩形区域"));
    ui->regionRectButton->setToolTip(tr("绘制矩形检测 ROI"));
    ui->allRegionRectButton->setToolTip(tr("绘制矩形检测 ROI"));

    ui->angleRangeRowFrame->setVisible(ui->angleEnableSwitch->isChecked());
    ui->widthRangeRowFrame->setVisible(ui->widthEnableSwitch->isChecked());
    ui->heightRangeRowFrame->setVisible(ui->heightEnableSwitch->isChecked());
    ui->boundaryFilterRowFrame->setVisible(ui->boundaryEnableSwitch->isChecked());
    ui->classFilterRowFrame->setVisible(ui->classFilterSwitch->isChecked());
    ui->viewerTitleLabel->setText(tr("基准图"));
    setViewerStatusText(roiStatusText(), roiStatusText());
    if (m_previewHelper)
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void ObjectDetectionDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &ObjectDetectionDialog::reject);
    connect(ui->headerMaximizeButton, &QToolButton::clicked, this, &ObjectDetectionDialog::showMaximized);
    connect(ui->finishButton, &QPushButton::clicked, this, &ObjectDetectionDialog::finishConfiguration);

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

    m_allRegionGroup->setExclusive(true);
    m_allRegionGroup->addButton(ui->allRegionDrawButton, 0);
    m_allRegionGroup->addButton(ui->allRegionRectButton, 1);

    connect(ui->regionRectButton, &QToolButton::clicked, this, &ObjectDetectionDialog::startRectangleRoiEditing);
    connect(ui->allRegionRectButton, &QToolButton::clicked, this, &ObjectDetectionDialog::startRectangleRoiEditing);
    connect(ui->regionDrawButton, &QToolButton::clicked, this, &ObjectDetectionDialog::showFreeRoiUnsupported);
    connect(ui->allRegionDrawButton, &QToolButton::clicked, this, &ObjectDetectionDialog::showFreeRoiUnsupported);

    const auto syncResultBasis = [this](int index) {
        const QSignalBlocker blockBasic(ui->resultBasisComboBox);
        const QSignalBlocker blockAll(ui->allResultBasisComboBox);
        ui->resultBasisComboBox->setCurrentIndex(index);
        ui->allResultBasisComboBox->setCurrentIndex(index);
        ui->resultBasisStackedWidget->setCurrentIndex(index);
        ui->allResultBasisStackedWidget->setCurrentIndex(index);
    };

    connect(ui->resultBasisComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, syncResultBasis);
    connect(ui->allResultBasisComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, syncResultBasis);

    const auto syncComboText = [this](QComboBox *source, QComboBox *target) {
        connect(source, &QComboBox::currentTextChanged, this, [target](const QString &text) {
            const QSignalBlocker blocker(target);
            setComboBoxValue(target, text);
        });
    };
    syncComboText(ui->positionCorrectionComboBox, ui->allPositionCorrectionComboBox);
    syncComboText(ui->allPositionCorrectionComboBox, ui->positionCorrectionComboBox);
    syncComboText(ui->modelComboBox, ui->allModelComboBox);
    syncComboText(ui->allModelComboBox, ui->modelComboBox);

    const auto syncCheckBox = [this](QCheckBox *source, QCheckBox *target) {
        connect(source, &QCheckBox::toggled, this, [target](bool checked) {
            const QSignalBlocker blocker(target);
            target->setChecked(checked);
        });
    };
    syncCheckBox(ui->positionCorrectionSwitch, ui->allPositionCorrectionSwitch);
    syncCheckBox(ui->allPositionCorrectionSwitch, ui->positionCorrectionSwitch);

    const auto syncSpinBox = [this](QSpinBox *source, QSpinBox *target) {
        connect(source, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [target](int value) {
            const QSignalBlocker blocker(target);
            target->setValue(value);
        });
    };
    syncSpinBox(ui->minCountSpinBox, ui->allMinCountSpinBox);
    syncSpinBox(ui->allMinCountSpinBox, ui->minCountSpinBox);
    syncSpinBox(ui->maxCountSpinBox, ui->allMaxCountSpinBox);
    syncSpinBox(ui->allMaxCountSpinBox, ui->maxCountSpinBox);
    syncSpinBox(ui->minScoreSpinBox, ui->allMinScoreSpinBox);
    syncSpinBox(ui->allMinScoreSpinBox, ui->minScoreSpinBox);

    const auto syncLineEdit = [this](QLineEdit *source, QLineEdit *target) {
        connect(source, &QLineEdit::textChanged, this, [target](const QString &text) {
            const QSignalBlocker blocker(target);
            target->setText(text);
        });
    };
    syncLineEdit(ui->categoryLineEdit, ui->allCategoryLineEdit);
    syncLineEdit(ui->allCategoryLineEdit, ui->categoryLineEdit);

    const auto bindVisibility = [](QCheckBox *switchButton, QWidget *targetRow) {
        QObject::connect(switchButton, &QCheckBox::toggled, targetRow, &QWidget::setVisible);
    };
    bindVisibility(ui->angleEnableSwitch, ui->angleRangeRowFrame);
    bindVisibility(ui->widthEnableSwitch, ui->widthRangeRowFrame);
    bindVisibility(ui->heightEnableSwitch, ui->heightRangeRowFrame);
    bindVisibility(ui->boundaryEnableSwitch, ui->boundaryFilterRowFrame);
    bindVisibility(ui->classFilterSwitch, ui->classFilterRowFrame);

    bindRange(ui->minCountSpinBox, ui->maxCountSpinBox);
    bindRange(ui->allMinCountSpinBox, ui->allMaxCountSpinBox);
    bindRange(ui->minAngleSpinBox, ui->maxAngleSpinBox);
    bindRange(ui->minWidthSpinBox, ui->maxWidthSpinBox);
    bindRange(ui->minHeightSpinBox, ui->maxHeightSpinBox);

    if (m_previewHelper) {
        connect(m_previewHelper,
                &FrameViewHelper::roiChanged,
                this,
                &ObjectDetectionDialog::handleRoiChanged);
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

    connect(ui->testRunButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, tr("目标检测"), tr("目标检测算法尚未接入"));
    });

    const auto connectTodoButton = [this](QAbstractButton *button, const QString &actionName) {
        if (!button)
            return;
        connect(button, &QAbstractButton::clicked, this, [this, actionName]() {
            showStageTodoMessage(actionName);
        });
    };
    connectTodoButton(ui->setupExternalEditButton, tr("外部编辑"));
    connectTodoButton(ui->objectDetectionExternalEditButton, tr("外部编辑"));
    connectTodoButton(ui->setupSaveButton, tr("保存"));
    connectTodoButton(ui->setupSaveAsButton, tr("另存为"));
    connectTodoButton(ui->setupExportButton, tr("IO 输出"));
    connectTodoButton(ui->importModelButton, tr("导入模型"));
    connectTodoButton(ui->allImportModelButton, tr("导入模型"));
    connectTodoButton(ui->screenRegionEditButton, tr("屏蔽区域编辑"));
    connectTodoButton(ui->viewerGridButton, tr("网格显示"));
    connectTodoButton(ui->viewerZoomSearchButton, tr("缩放定位"));
    connectTodoButton(ui->viewerZoomOutButton, tr("缩小"));
    connectTodoButton(ui->viewerZoomInButton, tr("放大"));
    connectTodoButton(ui->viewerFullButton, tr("全屏预览"));
}

void ObjectDetectionDialog::bindRange(QSpinBox *minimumSpinBox, QSpinBox *maximumSpinBox)
{
    connect(minimumSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [minimumSpinBox, maximumSpinBox](int value) {
        if (value > maximumSpinBox->value()) {
            maximumSpinBox->setValue(value);
        }
        minimumSpinBox->setMaximum(maximumSpinBox->maximum());
    });
    connect(maximumSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, [minimumSpinBox](int value) {
        if (value < minimumSpinBox->value()) {
            minimumSpinBox->setValue(value);
        }
    });
}

void ObjectDetectionDialog::setAllParamsMode(bool allMode)
{
    ui->basicSegmentButton->setChecked(!allMode);
    ui->allSegmentButton->setChecked(allMode);
    ui->objectDetectionParamsStackedWidget->setCurrentWidget(allMode ? ui->allParamsPage : ui->basicParamsPage);
}

void ObjectDetectionDialog::showStageTodoMessage(const QString &actionName)
{
    QMessageBox::information(this,
                             tr("目标检测"),
                             tr("%1本阶段暂未接入").arg(actionName));
}

void ObjectDetectionDialog::finishConfiguration()
{
    if (m_previewHelper)
        m_previewHelper->setRoiDrawingEnabled(false);
    accept();
}

void ObjectDetectionDialog::fitPreview()
{
    if (m_previewHelper)
        m_previewHelper->fitToView();
}

void ObjectDetectionDialog::showPreviewImage()
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

void ObjectDetectionDialog::showFrameForRoiEditing()
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
        const QString text = tr("当前无图像，ROI 默认全图；请先设置基准图或提供当前图像后再框选");
        setViewerStatusText(text, text);
        return;
    }

    ui->viewerTitleLabel->setText(title);
    m_previewHelper->setImage(image);
    m_previewHelper->clearToolOverlays();
    refreshDisplayedRoiOverlay();
}

void ObjectDetectionDialog::startRectangleRoiEditing()
{
    syncRegionButtons(true);
    showFrameForRoiEditing();

    if (!m_previewHelper || !m_previewHelper->hasImage())
        return;

    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    m_previewHelper->setRoiDrawingEnabled(true);
    const QString text = tr("请框选目标检测矩形 ROI");
    setViewerStatusText(text, text);
}

void ObjectDetectionDialog::showFreeRoiUnsupported()
{
    syncRegionButtons(true);
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        refreshDisplayedRoiOverlay();
    }
    const QString text = tr("暂未接入自由区域，当前支持矩形区域");
    setViewerStatusText(text, text);
}

void ObjectDetectionDialog::syncRegionButtons(bool rectangleRegion)
{
    if (!rectangleRegion)
        rectangleRegion = true;

    const QSignalBlocker blockDraw(ui->regionDrawButton);
    const QSignalBlocker blockRect(ui->regionRectButton);
    const QSignalBlocker blockAllDraw(ui->allRegionDrawButton);
    const QSignalBlocker blockAllRect(ui->allRegionRectButton);
    ui->regionDrawButton->setChecked(!rectangleRegion);
    ui->regionRectButton->setChecked(rectangleRegion);
    ui->allRegionDrawButton->setChecked(!rectangleRegion);
    ui->allRegionRectButton->setChecked(rectangleRegion);
}

void ObjectDetectionDialog::handleRoiChanged(const QRectF &roi)
{
    m_roiNormalized = normalizedRoiOrDefault(roi);
    syncRegionButtons(true);
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    const QString text = roiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[ObjectDetectionDialog] ROI normalized:" << m_roiNormalized;
}

void ObjectDetectionDialog::handleRoiSelectionRejected()
{
    const QString text = tr("ROI 无效，请拖拽宽高至少 2 像素的矩形");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void ObjectDetectionDialog::refreshDisplayedRoiOverlay()
{
    if (!m_previewHelper)
        return;

    m_previewHelper->clearRoi();
    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void ObjectDetectionDialog::setViewerStatusText(const QString &displayText, const QString &tooltipText)
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

QString ObjectDetectionDialog::roiStatusText() const
{
    const QRectF roi = effectiveRoiNormalized();
    return tr("矩形检测 ROI x=%1 y=%2 w=%3 h=%4")
            .arg(roi.x(), 0, 'f', 3)
            .arg(roi.y(), 0, 'f', 3)
            .arg(roi.width(), 0, 'f', 3)
            .arg(roi.height(), 0, 'f', 3);
}

QRectF ObjectDetectionDialog::effectiveRoiNormalized() const
{
    return normalizedRoiOrDefault(m_roiNormalized);
}
