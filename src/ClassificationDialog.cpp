#include "ClassificationDialog.h"
#include "ui_ClassificationDialog.h"

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
#include <QToolButton>
#include <QUuid>
#include <QtGlobal>

#include <cmath>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"

namespace {

QString judgeModeFromResultBasis(const QString &resultBasis)
{
    if (resultBasis == QObject::tr("最低得分"))
        return QStringLiteral("score");
    if (resultBasis == QObject::tr("类别判断"))
        return QStringLiteral("category");
    return QStringLiteral("score");
}

QString resultBasisFromJudgeMode(const QString &mode)
{
    return mode == QStringLiteral("category")
            ? QObject::tr("类别判断")
            : QObject::tr("最低得分");
}

QString judgeTypeMode(const QString &judgeType)
{
    return judgeType == QObject::tr("任意检测区域输出结果为 OK")
            ? QStringLiteral("any_ok")
            : QStringLiteral("all_ok");
}

QString judgeTypeText(const QString &mode)
{
    return mode == QStringLiteral("any_ok")
            ? QObject::tr("任意检测区域输出结果为 OK")
            : QObject::tr("所有检测区域输出结果为 OK");
}

void setComboBoxValue(QComboBox *comboBox, const QString &value)
{
    if (!comboBox || value.trimmed().isEmpty())
        return;

    const int index = comboBox->findText(value);
    if (index >= 0)
        comboBox->setCurrentIndex(index);
}

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

} // namespace

ClassificationDialog::ClassificationDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ClassificationDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_regionGroup(new QButtonGroup(this))
    , m_allRegionGroup(new QButtonGroup(this))
    , m_toolId(QStringLiteral("ai_classification_%1")
                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    ui->setupUi(this);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    connectControls();
    showPreviewImage();
}

ClassificationDialog::~ClassificationDialog()
{
    delete ui;
}

ClassificationConfig ClassificationDialog::configuration() const
{
    const bool allMode =
            ui->classificationParamsStackedWidget->currentWidget() == ui->allParamsPage;

    ClassificationConfig config;
    config.independentPositionCorrection =
            allMode ? ui->allPositionCorrectionSwitch->isChecked()
                    : ui->positionCorrectionSwitch->isChecked();
    config.positionCorrection =
            allMode ? ui->allPositionCorrectionComboBox->currentText()
                    : ui->positionCorrectionComboBox->currentText();
    config.modelName =
            allMode ? ui->allModelComboBox->currentText()
                    : ui->modelComboBox->currentText();
    config.resultBasis =
            allMode ? ui->allResultBasisComboBox->currentText()
                    : ui->resultBasisComboBox->currentText();
    config.minScore =
            allMode ? ui->allMinScoreSpinBox->value()
                    : ui->minScoreSpinBox->value();
    config.category =
            allMode ? ui->allCategoryLineEdit->text()
                    : ui->categoryLineEdit->text();
    config.judgeType = ui->allJudgeTypeComboBox->currentText();
    return config;
}

ToolConfig ClassificationDialog::toToolConfig() const
{
    const bool allMode =
            ui->classificationParamsStackedWidget->currentWidget() == ui->allParamsPage;
    const ClassificationConfig classificationConfig = configuration();

    QJsonObject params;
    params.insert(QStringLiteral("paramMode"),
                  allMode ? QStringLiteral("all") : QStringLiteral("basic"));
    params.insert(QStringLiteral("positionCorrectionEnabled"),
                  classificationConfig.independentPositionCorrection);
    params.insert(QStringLiteral("positionCorrectionSource"),
                  classificationConfig.positionCorrection);
    params.insert(QStringLiteral("detectRegionType"), QStringLiteral("rectangle"));
    params.insert(QStringLiteral("modelName"), classificationConfig.modelName);

    QJsonObject judgeRule;
    judgeRule.insert(QStringLiteral("mode"),
                     judgeModeFromResultBasis(classificationConfig.resultBasis));
    judgeRule.insert(QStringLiteral("resultBasis"), classificationConfig.resultBasis);
    judgeRule.insert(QStringLiteral("minScore"), classificationConfig.minScore);
    judgeRule.insert(QStringLiteral("category"), classificationConfig.category);
    judgeRule.insert(QStringLiteral("judgeType"),
                     judgeTypeMode(classificationConfig.judgeType));
    judgeRule.insert(QStringLiteral("judgeTypeText"), classificationConfig.judgeType);

    ToolConfig config;
    config.toolId = m_toolId;
    config.toolName = tr("分类");
    config.toolType = ToolType::AiClassification;
    config.category = ToolCategory::DeepLearning;
    config.enabled = m_enabled;
    config.roiNormalized = effectiveRoiNormalized();
    config.params = params;
    config.judgeRule = judgeRule;
    config.displayName = tr("分类");
    config.summary = summaryText();
    return config;
}

ToolConfig ClassificationDialog::toolConfig() const
{
    return toToolConfig();
}

ToolPreviewSnapshot ClassificationDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void ClassificationDialog::loadFromConfig(const ToolConfig &config)
{
    if (!config.toolId.trimmed().isEmpty())
        m_toolId = config.toolId;
    m_enabled = config.enabled;
    m_roiNormalized = normalizedRoiOrDefault(config.roiNormalized);

    const QJsonObject params = config.params;
    const QJsonObject judgeRule = config.judgeRule;
    setAllParamsMode(params.value(QStringLiteral("paramMode")).toString()
                     == QStringLiteral("all"));

    const bool positionCorrectionEnabled =
            params.value(QStringLiteral("positionCorrectionEnabled"))
            .toBool(ui->positionCorrectionSwitch->isChecked());
    ui->positionCorrectionSwitch->setChecked(positionCorrectionEnabled);
    ui->allPositionCorrectionSwitch->setChecked(positionCorrectionEnabled);

    const QString positionCorrectionSource =
            params.value(QStringLiteral("positionCorrectionSource")).toString();
    setComboBoxValue(ui->positionCorrectionComboBox, positionCorrectionSource);
    setComboBoxValue(ui->allPositionCorrectionComboBox, positionCorrectionSource);

    const QString modelName = params.value(QStringLiteral("modelName")).toString();
    if (!modelName.trimmed().isEmpty()) {
        if (ui->modelComboBox->findText(modelName) < 0)
            ui->modelComboBox->addItem(modelName);
        if (ui->allModelComboBox->findText(modelName) < 0)
            ui->allModelComboBox->addItem(modelName);
        setComboBoxValue(ui->modelComboBox, modelName);
        setComboBoxValue(ui->allModelComboBox, modelName);
    }

    QString resultBasis = judgeRule.value(QStringLiteral("resultBasis")).toString();
    if (resultBasis.trimmed().isEmpty())
        resultBasis = resultBasisFromJudgeMode(
                    judgeRule.value(QStringLiteral("mode")).toString());
    setComboBoxValue(ui->resultBasisComboBox, resultBasis);
    setComboBoxValue(ui->allResultBasisComboBox, resultBasis);

    const int basisIndex = ui->resultBasisComboBox->currentIndex();
    ui->resultBasisStackedWidget->setCurrentIndex(basisIndex);
    ui->allResultBasisStackedWidget->setCurrentIndex(basisIndex);

    const int minScore =
            judgeRule.value(QStringLiteral("minScore"))
            .toInt(ui->minScoreSpinBox->value());
    const QString category =
            judgeRule.value(QStringLiteral("category"))
            .toString(ui->categoryLineEdit->text());
    ui->minScoreSpinBox->setValue(minScore);
    ui->allMinScoreSpinBox->setValue(minScore);
    ui->categoryLineEdit->setText(category);
    ui->allCategoryLineEdit->setText(category);

    QString judgeType = judgeRule.value(QStringLiteral("judgeTypeText")).toString();
    if (judgeType.trimmed().isEmpty())
        judgeType = judgeTypeText(
                    judgeRule.value(QStringLiteral("judgeType")).toString());
    setComboBoxValue(ui->allJudgeTypeComboBox, judgeType);

    syncRegionButtons(true);
    m_referencePreviewSnapshot = ToolPreviewSnapshot();
    showPreviewImage();
    setViewerStatusText(roiStatusText(), roiStatusText());
}

QString ClassificationDialog::summaryText() const
{
    const ClassificationConfig config = configuration();
    if (config.resultBasis == tr("类别判断"))
        return tr("%1：%2；%3")
                .arg(config.resultBasis, config.category, config.judgeType);

    return tr("%1：%2；%3")
            .arg(config.resultBasis)
            .arg(config.minScore)
            .arg(config.judgeType);
}

void ClassificationDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void ClassificationDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 分类"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(this);

    setAllParamsMode(false);
    syncRegionButtons(true);
    ui->resultBasisComboBox->setCurrentIndex(0);
    ui->allResultBasisComboBox->setCurrentIndex(0);
    ui->resultBasisStackedWidget->setCurrentIndex(0);
    ui->allResultBasisStackedWidget->setCurrentIndex(0);
    ui->minScoreSpinBox->setRange(1, 100);
    ui->allMinScoreSpinBox->setRange(1, 100);
    ui->minScoreSpinBox->setValue(50);
    ui->allMinScoreSpinBox->setValue(50);
    ui->viewerTitleLabel->setText(tr("基准图"));
    setViewerStatusText(roiStatusText(), roiStatusText());
    if (m_previewHelper)
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void ClassificationDialog::connectControls()
{
    connect(ui->headerCloseButton,
            &QToolButton::clicked,
            this,
            &ClassificationDialog::reject);
    connect(ui->headerMaximizeButton,
            &QToolButton::clicked,
            this,
            &ClassificationDialog::showMaximized);
    connect(ui->finishButton,
            &QPushButton::clicked,
            this,
            &ClassificationDialog::finishConfiguration);

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

    connect(ui->regionRectButton,
            &QToolButton::clicked,
            this,
            &ClassificationDialog::startRectangleRoiEditing);
    connect(ui->allRegionRectButton,
            &QToolButton::clicked,
            this,
            &ClassificationDialog::startRectangleRoiEditing);
    connect(ui->regionDrawButton,
            &QToolButton::clicked,
            this,
            &ClassificationDialog::showFreeRoiUnsupported);
    connect(ui->allRegionDrawButton,
            &QToolButton::clicked,
            this,
            &ClassificationDialog::showFreeRoiUnsupported);

    const auto syncResultBasis = [this](int index) {
        const QSignalBlocker blockBasic(ui->resultBasisComboBox);
        const QSignalBlocker blockAll(ui->allResultBasisComboBox);
        ui->resultBasisComboBox->setCurrentIndex(index);
        ui->allResultBasisComboBox->setCurrentIndex(index);
        ui->resultBasisStackedWidget->setCurrentIndex(index);
        ui->allResultBasisStackedWidget->setCurrentIndex(index);
    };
    connect(ui->resultBasisComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            syncResultBasis);
    connect(ui->allResultBasisComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            syncResultBasis);

    const auto syncComboText = [this](QComboBox *source, QComboBox *target) {
        connect(source,
                &QComboBox::currentTextChanged,
                this,
                [target](const QString &text) {
            const QSignalBlocker blocker(target);
            const int index = target->findText(text);
            if (index >= 0)
                target->setCurrentIndex(index);
        });
    };
    syncComboText(ui->positionCorrectionComboBox,
                  ui->allPositionCorrectionComboBox);
    syncComboText(ui->allPositionCorrectionComboBox,
                  ui->positionCorrectionComboBox);
    syncComboText(ui->modelComboBox, ui->allModelComboBox);
    syncComboText(ui->allModelComboBox, ui->modelComboBox);

    const auto syncCheckBox = [this](QCheckBox *source, QCheckBox *target) {
        connect(source, &QCheckBox::toggled, this, [target](bool checked) {
            const QSignalBlocker blocker(target);
            target->setChecked(checked);
        });
    };
    syncCheckBox(ui->positionCorrectionSwitch,
                 ui->allPositionCorrectionSwitch);
    syncCheckBox(ui->allPositionCorrectionSwitch,
                 ui->positionCorrectionSwitch);

    const auto syncSpinBox = [this](QSpinBox *source, QSpinBox *target) {
        connect(source,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this,
                [target](int value) {
            const QSignalBlocker blocker(target);
            target->setValue(value);
        });
    };
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

    if (m_previewHelper) {
        connect(m_previewHelper,
                &FrameViewHelper::roiChanged,
                this,
                &ClassificationDialog::handleRoiChanged);
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
        QMessageBox::information(this,
                                 tr("分类"),
                                 tr("分类算法尚未接入，参数配置可正常保存"));
    });

    const auto connectTodoButton =
            [this](QAbstractButton *button, const QString &actionName) {
        if (!button)
            return;
        connect(button, &QAbstractButton::clicked, this, [this, actionName]() {
            showStageTodoMessage(actionName);
        });
    };
    connectTodoButton(ui->setupExternalEditButton, tr("外部编辑"));
    connectTodoButton(ui->classificationExternalEditButton, tr("外部编辑"));
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

void ClassificationDialog::setAllParamsMode(bool allMode)
{
    ui->basicSegmentButton->setChecked(!allMode);
    ui->allSegmentButton->setChecked(allMode);
    ui->classificationParamsStackedWidget->setCurrentWidget(
                allMode ? ui->allParamsPage : ui->basicParamsPage);
}

void ClassificationDialog::showStageTodoMessage(const QString &actionName)
{
    QMessageBox::information(this,
                             tr("分类"),
                             tr("%1本阶段暂未接入").arg(actionName));
}

void ClassificationDialog::finishConfiguration()
{
    if (m_previewHelper)
        m_previewHelper->setRoiDrawingEnabled(false);
    accept();
}

void ClassificationDialog::fitPreview()
{
    if (m_previewHelper)
        m_previewHelper->fitToView();
}

void ClassificationDialog::showPreviewImage()
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

void ClassificationDialog::showFrameForRoiEditing()
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
        const QString text =
                tr("当前无图像，ROI 默认全图；请先设置基准图或提供当前图像后再框选");
        setViewerStatusText(text, text);
        return;
    }

    ui->viewerTitleLabel->setText(title);
    m_previewHelper->setImage(image);
    m_previewHelper->clearToolOverlays();
    refreshDisplayedRoiOverlay();
}

void ClassificationDialog::startRectangleRoiEditing()
{
    syncRegionButtons(true);
    showFrameForRoiEditing();

    if (!m_previewHelper || !m_previewHelper->hasImage())
        return;

    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    m_previewHelper->setRoiDrawingEnabled(true);
    const QString text = tr("请框选分类矩形 ROI");
    setViewerStatusText(text, text);
}

void ClassificationDialog::showFreeRoiUnsupported()
{
    syncRegionButtons(true);
    if (m_previewHelper) {
        m_previewHelper->setRoiDrawingEnabled(false);
        refreshDisplayedRoiOverlay();
    }
    const QString text = tr("暂未接入自由区域，当前支持矩形区域");
    setViewerStatusText(text, text);
}

void ClassificationDialog::syncRegionButtons(bool rectangleRegion)
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

void ClassificationDialog::handleRoiChanged(const QRectF &roi)
{
    m_roiNormalized = normalizedRoiOrDefault(roi);
    syncRegionButtons(true);
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    const QString text = roiStatusText();
    setViewerStatusText(text, text);
    qDebug() << "[ClassificationDialog] ROI normalized:" << m_roiNormalized;
}

void ClassificationDialog::handleRoiSelectionRejected()
{
    const QString text = tr("ROI 无效，请拖拽宽高至少 2 像素的矩形");
    setViewerStatusText(text, text);
    refreshDisplayedRoiOverlay();
}

void ClassificationDialog::refreshDisplayedRoiOverlay()
{
    if (!m_previewHelper)
        return;

    m_previewHelper->clearRoi();
    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void ClassificationDialog::setViewerStatusText(const QString &displayText,
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

QString ClassificationDialog::roiStatusText() const
{
    const QRectF roi = effectiveRoiNormalized();
    return tr("矩形检测 ROI x=%1 y=%2 w=%3 h=%4")
            .arg(roi.x(), 0, 'f', 3)
            .arg(roi.y(), 0, 'f', 3)
            .arg(roi.width(), 0, 'f', 3)
            .arg(roi.height(), 0, 'f', 3);
}

QRectF ClassificationDialog::effectiveRoiNormalized() const
{
    return normalizedRoiOrDefault(m_roiNormalized);
}
