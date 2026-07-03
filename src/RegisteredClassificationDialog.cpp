#include "RegisteredClassificationDialog.h"

#include "PlanDialogUtils.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolRequest.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QtGlobal>

#include <cmath>

namespace {

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

    return QRectF(json.value(QStringLiteral("x")).toDouble(fallback.x()),
                  json.value(QStringLiteral("y")).toDouble(fallback.y()),
                  json.value(QStringLiteral("width")).toDouble(fallback.width()),
                  json.value(QStringLiteral("height")).toDouble(fallback.height()));
}

bool finiteRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) &&
           std::isfinite(rect.y()) &&
           std::isfinite(rect.width()) &&
           std::isfinite(rect.height());
}

QRectF normalizedRoiOrDefault(const QRectF &source)
{
    if (!finiteRect(source) || source.width() <= 0.0 || source.height() <= 0.0)
        return QRectF(0.0, 0.0, 1.0, 1.0);

    const QRectF normalized = source.normalized();
    const double left = qBound(0.0, normalized.left(), 1.0);
    const double top = qBound(0.0, normalized.top(), 1.0);
    const double right = qBound(0.0, normalized.right(), 1.0);
    const double bottom = qBound(0.0, normalized.bottom(), 1.0);
    QRectF clamped(QPointF(left, top), QPointF(right, bottom));
    clamped = clamped.normalized();
    return clamped.width() > 0.0 && clamped.height() > 0.0
            ? clamped
            : QRectF(0.0, 0.0, 1.0, 1.0);
}

QFrame *card(QWidget *parent, const QString &title)
{
    QFrame *frame = new QFrame(parent);
    frame->setProperty("panelRole", QStringLiteral("configCard"));
    frame->setStyleSheet(QStringLiteral(
        "QFrame[panelRole=\"configCard\"]{background:#ffffff;border:1px solid #d8dee8;border-radius:6px;}"
        "QLabel[role=\"cardTitle\"]{font-weight:600;color:#111827;}"
        "QLabel[role=\"rowField\"]{color:#4b5563;}"));

    QVBoxLayout *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(10);
    QLabel *titleLabel = new QLabel(title, frame);
    titleLabel->setProperty("role", QStringLiteral("cardTitle"));
    layout->addWidget(titleLabel);
    return frame;
}

QHBoxLayout *row(const QString &label, QWidget *field)
{
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    QLabel *labelWidget = new QLabel(label);
    labelWidget->setProperty("role", QStringLiteral("rowField"));
    labelWidget->setMinimumWidth(118);
    layout->addWidget(labelWidget);
    layout->addWidget(field, 1);
    return layout;
}

void applyActionButtonMetrics(QPushButton *button)
{
    if (!button)
        return;
    button->setMinimumSize(92, 34);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

} // namespace

RegisteredClassificationDialog::RegisteredClassificationDialog(QWidget *parent)
    : QDialog(parent)
    , m_toolId(QStringLiteral("registered_classification_%1")
                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    buildUi();
    connectControls();
    refreshUiState();
    refreshPreview();
}

RegisteredClassificationDialog::~RegisteredClassificationDialog() = default;

ToolConfig RegisteredClassificationDialog::toToolConfig() const
{
    ToolConfig config;
    config.toolId = m_toolId;
    config.toolName = QStringLiteral("RegisteredClassification");
    config.toolType = ToolType::RegisteredClassification;
    config.category = ToolCategory::Recognition;
    config.enabled = m_enabled;
    config.roiNormalized = effectiveRoiNormalized();
    config.params.insert(QStringLiteral("registeredClassification"),
                         registeredClassificationParams());
    config.judgeRule = judgeRule();
    config.displayName = tr("注册分类");
    config.summary = summaryText();
    return config;
}

ToolConfig RegisteredClassificationDialog::toolConfig() const
{
    return toToolConfig();
}

ToolPreviewSnapshot RegisteredClassificationDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void RegisteredClassificationDialog::loadFromConfig(const ToolConfig &config)
{
    if (config.toolType != ToolType::Unknown &&
        config.toolType != ToolType::RegisteredClassification) {
        qWarning() << "[RegisteredClassificationDialog] Ignore unexpected tool type"
                   << toolTypeToString(config.toolType);
        return;
    }

    if (!config.toolId.trimmed().isEmpty())
        m_toolId = config.toolId;
    m_enabled = config.enabled;

    QJsonObject params = config.params.value(QStringLiteral("registeredClassification")).toObject();
    if (params.isEmpty())
        params = config.params;

    m_modelPath = params.value(QStringLiteral("modelPath")).toString();
    m_modelName = params.value(QStringLiteral("modelName")).toString();
    m_detectRegionType =
            params.value(QStringLiteral("detectRegionType")).toString(QStringLiteral("full"));
    m_roiNormalized = normalizedRoiOrDefault(
                rectFromJson(params.value(QStringLiteral("roiNormalized")).toObject(),
                             config.roiNormalized.isNull()
                             ? QRectF(0.0, 0.0, 1.0, 1.0)
                             : config.roiNormalized));
    m_positionCorrectionEnabled =
            params.value(QStringLiteral("enablePositionCorrection")).toBool(true);
    m_positionCorrectionSource =
            params.value(QStringLiteral("positionCorrectionSource"))
            .toString(QStringLiteral("1 基准图.位置修正信息"));

    const bool allMode = params.value(QStringLiteral("paramMode")).toString()
            == QStringLiteral("all");
    setAllParamsMode(allMode);

    if (m_topKSpinBox)
        m_topKSpinBox->setValue(qBound(1, params.value(QStringLiteral("topK")).toInt(1), 10));
    if (m_modelTypeComboBox) {
        const QString modelType = params.value(QStringLiteral("modelType"))
                .toString(QStringLiteral("halcon_dl_classification"));
        const int index = m_modelTypeComboBox->findData(modelType);
        m_modelTypeComboBox->setCurrentIndex(index >= 0 ? index : 0);
    }

    const QJsonObject rule = config.judgeRule;
    const QString mode = rule.value(QStringLiteral("mode")).toString(QStringLiteral("class_match"));
    if (m_resultBasisComboBox) {
        m_resultBasisComboBox->setCurrentIndex(mode == QStringLiteral("min_score") ? 1 : 0);
    }
    if (m_expectedLabelLineEdit)
        m_expectedLabelLineEdit->setText(rule.value(QStringLiteral("expectedLabel")).toString());
    if (m_minScoreSpinBox)
        m_minScoreSpinBox->setValue(qBound(0, rule.value(QStringLiteral("minScore")).toInt(80), 100));

    refreshUiState();
    refreshPreview();
}

QString RegisteredClassificationDialog::summaryText() const
{
    const QString modelText = m_modelName.trimmed().isEmpty() ? tr("未导入模型") : m_modelName;
    if (judgeMode() == QStringLiteral("min_score"))
        return tr("模型：%1；最低得分：%2").arg(modelText).arg(m_minScoreSpinBox->value());
    return tr("模型：%1；类别：%2")
            .arg(modelText,
                 m_expectedLabelLineEdit->text().trimmed().isEmpty()
                 ? tr("未设置")
                 : m_expectedLabelLineEdit->text().trimmed());
}

void RegisteredClassificationDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (m_previewHelper)
        m_previewHelper->fitToView();
}

void RegisteredClassificationDialog::finishConfiguration()
{
    accept();
}

void RegisteredClassificationDialog::runReferenceTest()
{
    ToolRequest request;
    request.config = toToolConfig();
    request.referenceImage = ReferenceImageProvider::instance().referenceFrame();
    const ToolResult result = m_placeholderAdapter.run(request);
    m_referencePreviewSnapshot =
            makeReferenceToolPreviewSnapshot(request.config, result, effectiveRoiNormalized());
    displayPlaceholderResult(result);
}

void RegisteredClassificationDialog::runTest()
{
    ToolRequest request;
    request.config = toToolConfig();
    const ToolResult result = m_placeholderAdapter.run(request);
    displayPlaceholderResult(result);
}

void RegisteredClassificationDialog::importModel()
{
    const QString path = QFileDialog::getOpenFileName(
                this,
                tr("导入注册分类模型"),
                QString(),
                tr("模型文件 (*.*)"));
    if (path.trimmed().isEmpty())
        return;

    QString errorMessage;
    if (!validateImportedModelPath(path, &errorMessage)) {
        QMessageBox::warning(this, tr("导入模型"), errorMessage);
        return;
    }

    const QFileInfo info(path);
    m_modelPath = info.absoluteFilePath();
    m_modelName = info.completeBaseName();
    updateModelLabels();
    setViewerStatusText(tr("模型已导入：%1").arg(m_modelName));
}

void RegisteredClassificationDialog::exportModel()
{
    if (m_modelPath.trimmed().isEmpty()) {
        QMessageBox::information(this, tr("导出模型"), tr("当前未导入模型。"));
        return;
    }

    const QFileInfo sourceInfo(m_modelPath);
    const QString target = QFileDialog::getSaveFileName(
                this,
                tr("导出注册分类模型"),
                sourceInfo.fileName(),
                tr("模型文件 (*.*)"));
    if (target.trimmed().isEmpty())
        return;

    if (QFile::exists(target))
        QFile::remove(target);
    if (!QFile::copy(m_modelPath, target)) {
        QMessageBox::warning(this, tr("导出模型"), tr("模型文件导出失败。"));
        return;
    }
    setViewerStatusText(tr("模型已导出：%1").arg(QFileInfo(target).fileName()));
}

void RegisteredClassificationDialog::deleteModel()
{
    m_modelPath.clear();
    m_modelName.clear();
    updateModelLabels();
    setViewerStatusText(tr("模型已删除，后端推理仍未实现。"));
}

void RegisteredClassificationDialog::openRegisterTraining()
{
    showTodoMessage(tr("注册训练"));
}

void RegisteredClassificationDialog::openModelManagement()
{
    showTodoMessage(tr("模型管理"));
}

void RegisteredClassificationDialog::startGlobalDetection()
{
    m_detectRegionType = QStringLiteral("full");
    m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    if (m_previewHelper)
        m_previewHelper->setRoiDrawingEnabled(false);
    refreshUiState();
    refreshRoiOverlay();
}

void RegisteredClassificationDialog::startRectangleRoiEditing()
{
    m_detectRegionType = QStringLiteral("rectangle");
    if (m_previewHelper) {
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
        m_previewHelper->setRoiDrawingEnabled(true);
    }
    refreshUiState();
    setViewerStatusText(tr("请在右侧图像上绘制矩形检测区域，完成后点击“完成”。"));
}

void RegisteredClassificationDialog::finishRoiEditing()
{
    if (m_previewHelper)
        m_previewHelper->setRoiDrawingEnabled(false);
    refreshRoiOverlay();
    setViewerStatusText(roiStatusText());
}

void RegisteredClassificationDialog::handleRoiChanged(const QRectF &roi)
{
    m_roiNormalized = normalizedRoiOrDefault(roi);
    m_detectRegionType = QStringLiteral("rectangle");
    refreshUiState();
    setViewerStatusText(roiStatusText());
}

void RegisteredClassificationDialog::handleRoiSelectionRejected()
{
    setViewerStatusText(tr("检测区域无效，请重新绘制矩形 ROI。"));
}

void RegisteredClassificationDialog::buildUi()
{
    setWindowTitle(tr("方案编辑 - 注册分类"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(this);

    m_segmentGroup = new QButtonGroup(this);
    m_regionGroup = new QButtonGroup(this);
    m_judgeGroup = new QButtonGroup(this);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *header = new QFrame(this);
    header->setStyleSheet(QStringLiteral("background:#3f444e;color:#ffffff;"));
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(28, 0, 22, 0);
    QLabel *headerTitle = new QLabel(tr("方案编辑"), header);
    QToolButton *closeButton = new QToolButton(header);
    closeButton->setObjectName(QStringLiteral("registeredClassificationCloseButton"));
    closeButton->setText(QStringLiteral("×"));
    closeButton->setStyleSheet(QStringLiteral("color:#ffffff;font-size:24px;border:0;"));
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);
    root->addWidget(header, 0);

    QHBoxLayout *content = new QHBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);
    root->addLayout(content, 1);

    QFrame *leftPanel = new QFrame(this);
    leftPanel->setMinimumWidth(420);
    leftPanel->setMaximumWidth(500);
    leftPanel->setStyleSheet(QStringLiteral("background:#eef1f5;color:#111827;"));
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(24, 18, 24, 18);
    leftLayout->setSpacing(14);
    content->addWidget(leftPanel, 0);

    QHBoxLayout *titleLayout = new QHBoxLayout;
    QLabel *dialogTitle = new QLabel(tr("注册分类"), leftPanel);
    dialogTitle->setStyleSheet(QStringLiteral("font-size:18px;font-weight:600;color:#111827;"));
    m_basicButton = new QPushButton(tr("基础"), leftPanel);
    m_allButton = new QPushButton(tr("全部"), leftPanel);
    m_basicButton->setCheckable(true);
    m_allButton->setCheckable(true);
    m_segmentGroup->addButton(m_basicButton, 0);
    m_segmentGroup->addButton(m_allButton, 1);
    titleLayout->addWidget(dialogTitle);
    titleLayout->addStretch(1);
    titleLayout->addWidget(m_basicButton);
    titleLayout->addWidget(m_allButton);
    leftLayout->addLayout(titleLayout);

    QScrollArea *scrollArea = new QScrollArea(leftPanel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    QWidget *scrollContent = new QWidget(scrollArea);
    QVBoxLayout *paramsLayout = new QVBoxLayout(scrollContent);
    paramsLayout->setContentsMargins(0, 8, 0, 0);
    paramsLayout->setSpacing(14);
    scrollArea->setWidget(scrollContent);
    leftLayout->addWidget(scrollArea, 1);

    QFrame *detectCard = card(scrollContent, tr("检测区域"));
    QVBoxLayout *detectLayout = qobject_cast<QVBoxLayout *>(detectCard->layout());
    QWidget *regionButtons = new QWidget(detectCard);
    QHBoxLayout *regionLayout = new QHBoxLayout(regionButtons);
    regionLayout->setContentsMargins(0, 0, 0, 0);
    m_globalRegionButton = new QToolButton(regionButtons);
    m_globalRegionButton->setText(QStringLiteral("▣"));
    m_globalRegionButton->setToolTip(tr("全屏检测"));
    m_globalRegionButton->setCheckable(true);
    m_rectRegionButton = new QToolButton(regionButtons);
    m_rectRegionButton->setText(QStringLiteral("□"));
    m_rectRegionButton->setToolTip(tr("矩形检测区域"));
    m_rectRegionButton->setCheckable(true);
    m_regionGroup->setExclusive(true);
    m_regionGroup->addButton(m_globalRegionButton, 0);
    m_regionGroup->addButton(m_rectRegionButton, 1);
    m_roiFinishButton = new QPushButton(tr("完成"), regionButtons);
    regionLayout->addWidget(new QLabel(tr("检测区"), regionButtons));
    regionLayout->addStretch(1);
    regionLayout->addWidget(m_globalRegionButton);
    regionLayout->addWidget(m_rectRegionButton);
    regionLayout->addWidget(m_roiFinishButton);
    detectLayout->addWidget(regionButtons);

    QWidget *positionEnableRow = new QWidget(detectCard);
    QHBoxLayout *positionEnableLayout = new QHBoxLayout(positionEnableRow);
    positionEnableLayout->setContentsMargins(0, 0, 0, 0);
    positionEnableLayout->addWidget(new QLabel(tr("独立位置修正使能 ⓘ"), positionEnableRow));
    positionEnableLayout->addStretch(1);
    m_positionCorrectionCheckBox = new QCheckBox(positionEnableRow);
    m_positionCorrectionCheckBox->setObjectName(QStringLiteral("positionCorrectionSwitch"));
    positionEnableLayout->addWidget(m_positionCorrectionCheckBox);
    detectLayout->addWidget(positionEnableRow);

    m_positionSourceRow = new QWidget(detectCard);
    QHBoxLayout *positionSourceLayout = new QHBoxLayout(m_positionSourceRow);
    positionSourceLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *positionLabel = new QLabel(tr("位置修正"), m_positionSourceRow);
    positionLabel->setProperty("role", QStringLiteral("rowField"));
    positionLabel->setMinimumWidth(118);
    m_positionSourceComboBox = new QComboBox(m_positionSourceRow);
    m_positionSourceComboBox->addItem(QStringLiteral("1 基准图.位置修正信息"));
    positionSourceLayout->addWidget(positionLabel);
    positionSourceLayout->addWidget(m_positionSourceComboBox, 1);
    detectLayout->addWidget(m_positionSourceRow);
    paramsLayout->addWidget(detectCard);

    QFrame *modelCard = card(scrollContent, tr("模型训练"));
    QVBoxLayout *modelLayout = qobject_cast<QVBoxLayout *>(modelCard->layout());
    m_modelNameLabel = new QLabel(tr("未导入模型"), modelCard);
    m_modelPathLabel = new QLabel(tr("模型路径：未设置"), modelCard);
    m_modelPathLabel->setWordWrap(true);
    modelLayout->addWidget(m_modelNameLabel);
    modelLayout->addWidget(m_modelPathLabel);
    QHBoxLayout *modelButtonRow = new QHBoxLayout;
    m_importModelButton = new QPushButton(tr("导入模型"), modelCard);
    m_exportModelButton = new QPushButton(tr("导出模型"), modelCard);
    m_deleteModelButton = new QPushButton(tr("删除模型"), modelCard);
    modelButtonRow->addWidget(m_importModelButton);
    modelButtonRow->addWidget(m_exportModelButton);
    modelButtonRow->addWidget(m_deleteModelButton);
    modelLayout->addLayout(modelButtonRow);
    QHBoxLayout *trainingButtonRow = new QHBoxLayout;
    m_registerTrainingButton = new QPushButton(tr("注册训练"), modelCard);
    m_modelManagementButton = new QPushButton(tr("模型管理"), modelCard);
    trainingButtonRow->addWidget(m_registerTrainingButton);
    trainingButtonRow->addWidget(m_modelManagementButton);
    modelLayout->addLayout(trainingButtonRow);
    paramsLayout->addWidget(modelCard);

    m_advancedCard = card(scrollContent, tr("全部参数"));
    QVBoxLayout *advancedLayout = qobject_cast<QVBoxLayout *>(m_advancedCard->layout());
    m_modelTypeComboBox = new QComboBox(m_advancedCard);
    m_modelTypeComboBox->addItem(tr("HALCON DL 分类"), QStringLiteral("halcon_dl_classification"));
    m_topKSpinBox = new QSpinBox(m_advancedCard);
    m_topKSpinBox->setRange(1, 10);
    m_topKSpinBox->setValue(1);
    QLabel *runtimeLabel = new QLabel(tr("HALCON runtime：使用 HalconRuntimePaths 解析"), m_advancedCard);
    runtimeLabel->setWordWrap(true);
    advancedLayout->addLayout(row(tr("模型类型"), m_modelTypeComboBox));
    advancedLayout->addLayout(row(tr("TopK"), m_topKSpinBox));
    advancedLayout->addWidget(runtimeLabel);
    paramsLayout->addWidget(m_advancedCard);

    QFrame *judgeCard = card(scrollContent, tr("结果判断"));
    QVBoxLayout *judgeLayout = qobject_cast<QVBoxLayout *>(judgeCard->layout());
    m_resultBasisComboBox = new QComboBox(judgeCard);
    m_resultBasisComboBox->addItem(tr("类别判断"), QStringLiteral("class_match"));
    m_resultBasisComboBox->addItem(tr("最低得分"), QStringLiteral("min_score"));
    m_expectedLabelLineEdit = new QLineEdit(judgeCard);
    m_expectedLabelLineEdit->setPlaceholderText(tr("请输入类别名称"));
    m_minScoreSpinBox = new QSpinBox(judgeCard);
    m_minScoreSpinBox->setRange(0, 100);
    m_minScoreSpinBox->setValue(80);
    judgeLayout->addLayout(row(tr("判断依据"), m_resultBasisComboBox));
    judgeLayout->addLayout(row(tr("类别名称"), m_expectedLabelLineEdit));
    judgeLayout->addLayout(row(tr("最低得分"), m_minScoreSpinBox));
    paramsLayout->addWidget(judgeCard);
    paramsLayout->addStretch(1);

    QHBoxLayout *bottomButtons = new QHBoxLayout;
    m_referenceTestButton = new QPushButton(tr("基准图测试"), leftPanel);
    m_testRunButton = new QPushButton(tr("测试运行"), leftPanel);
    m_finishButton = new QPushButton(tr("完成"), leftPanel);
    applyActionButtonMetrics(m_referenceTestButton);
    applyActionButtonMetrics(m_testRunButton);
    applyActionButtonMetrics(m_finishButton);
    m_finishButton->setProperty("actionRole", QStringLiteral("testPrimary"));
    bottomButtons->addStretch(1);
    bottomButtons->addWidget(m_referenceTestButton);
    bottomButtons->addWidget(m_testRunButton);
    bottomButtons->addWidget(m_finishButton);
    leftLayout->addLayout(bottomButtons);

    QFrame *rightPanel = new QFrame(this);
    rightPanel->setStyleSheet(QStringLiteral("background:#111418;color:#f9fafb;"));
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_viewerTitleLabel = new QLabel(tr("基准图"), rightPanel);
    m_viewerTitleLabel->setMinimumHeight(48);
    m_viewerTitleLabel->setContentsMargins(18, 0, 0, 0);
    m_previewGraphicsView = new QGraphicsView(rightPanel);
    m_viewerStatusLabel = new QLabel(rightPanel);
    m_viewerStatusLabel->setMinimumHeight(42);
    m_viewerStatusLabel->setContentsMargins(18, 0, 0, 0);
    rightLayout->addWidget(m_viewerTitleLabel);
    rightLayout->addWidget(m_previewGraphicsView, 1);
    rightLayout->addWidget(m_viewerStatusLabel);
    content->addWidget(rightPanel, 1);

    m_previewHelper = new FrameViewHelper(m_previewGraphicsView, this);

    setStyleSheet(styleSheet() + QStringLiteral(
        "QPushButton,QToolButton,QComboBox,QSpinBox,QLineEdit{background:#ffffff;color:#111827;border:1px solid #cfd6df;border-radius:4px;padding:6px;}"
        "QPushButton:checked,QToolButton:checked{background:#fff3e6;color:#ff7a00;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"testPrimary\"]{background:#111827;color:#ffffff;border:1px solid #111827;}"
        "QPushButton[actionRole=\"testPrimary\"]:hover{background:#000000;border-color:#000000;}"
        "QCheckBox{color:#111827;}"));

    m_basicButton->setChecked(true);
    m_globalRegionButton->setChecked(true);
    m_positionCorrectionCheckBox->setChecked(true);
}

void RegisteredClassificationDialog::connectControls()
{
    if (QToolButton *closeButton =
            findChild<QToolButton *>(QStringLiteral("registeredClassificationCloseButton"))) {
        connect(closeButton, &QToolButton::clicked, this, &RegisteredClassificationDialog::reject);
    }
    connect(m_basicButton, &QPushButton::clicked, this, [this]() { setAllParamsMode(false); });
    connect(m_allButton, &QPushButton::clicked, this, [this]() { setAllParamsMode(true); });
    connect(m_globalRegionButton, &QToolButton::clicked, this, &RegisteredClassificationDialog::startGlobalDetection);
    connect(m_rectRegionButton, &QToolButton::clicked, this, &RegisteredClassificationDialog::startRectangleRoiEditing);
    connect(m_roiFinishButton, &QPushButton::clicked, this, &RegisteredClassificationDialog::finishRoiEditing);
    connect(m_positionCorrectionCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_positionCorrectionEnabled = checked;
        refreshUiState();
    });
    connect(m_positionSourceComboBox, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        m_positionCorrectionSource = text;
    });
    connect(m_importModelButton, &QPushButton::clicked, this, &RegisteredClassificationDialog::importModel);
    connect(m_exportModelButton, &QPushButton::clicked, this, &RegisteredClassificationDialog::exportModel);
    connect(m_deleteModelButton, &QPushButton::clicked, this, &RegisteredClassificationDialog::deleteModel);
    connect(m_registerTrainingButton, &QPushButton::clicked, this, &RegisteredClassificationDialog::openRegisterTraining);
    connect(m_modelManagementButton, &QPushButton::clicked, this, &RegisteredClassificationDialog::openModelManagement);
    connect(m_resultBasisComboBox, &QComboBox::currentTextChanged, this, [this]() { refreshUiState(); });
    connect(m_referenceTestButton, &QPushButton::clicked, this, &RegisteredClassificationDialog::runReferenceTest);
    connect(m_testRunButton, &QPushButton::clicked, this, &RegisteredClassificationDialog::runTest);
    connect(m_finishButton, &QPushButton::clicked, this, &RegisteredClassificationDialog::finishConfiguration);

    if (m_previewHelper) {
        connect(m_previewHelper, &FrameViewHelper::roiChanged,
                this, &RegisteredClassificationDialog::handleRoiChanged);
        connect(m_previewHelper, &FrameViewHelper::roiSelectionRejected,
                this, &RegisteredClassificationDialog::handleRoiSelectionRejected);
    }
}

void RegisteredClassificationDialog::setAllParamsMode(bool allMode)
{
    m_allParamsMode = allMode;
    refreshUiState();
}

void RegisteredClassificationDialog::refreshUiState()
{
    const QSignalBlocker positionBlocker(m_positionCorrectionCheckBox);
    m_basicButton->setChecked(!m_allParamsMode);
    m_allButton->setChecked(m_allParamsMode);
    if (m_advancedCard)
        m_advancedCard->setVisible(m_allParamsMode);
    m_globalRegionButton->setChecked(m_detectRegionType == QStringLiteral("full"));
    m_rectRegionButton->setChecked(m_detectRegionType == QStringLiteral("rectangle"));
    m_positionCorrectionCheckBox->setChecked(m_positionCorrectionEnabled);
    if (m_positionSourceRow)
        m_positionSourceRow->setVisible(m_positionCorrectionEnabled);
    const bool classMode = judgeMode() == QStringLiteral("class_match");
    m_expectedLabelLineEdit->setVisible(classMode);
    m_minScoreSpinBox->setVisible(!classMode);
    updateModelLabels();
}

void RegisteredClassificationDialog::refreshPreview()
{
    const QImage reference = ReferenceImageProvider::instance().referenceImage();
    if (m_previewHelper) {
        if (!reference.isNull())
            m_previewHelper->setImage(reference);
        else
            m_previewHelper->clear();
    }
    refreshRoiOverlay();
    setViewerStatusText(roiStatusText());
}

void RegisteredClassificationDialog::refreshRoiOverlay()
{
    if (!m_previewHelper)
        return;
    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    m_previewHelper->fitToView();
}

void RegisteredClassificationDialog::setViewerStatusText(const QString &text)
{
    if (m_viewerStatusLabel)
        m_viewerStatusLabel->setText(text);
}

void RegisteredClassificationDialog::displayPlaceholderResult(const ToolResult &result)
{
    setViewerStatusText(tr("注册分类: %1 | %2").arg(result.status, result.message));
}

void RegisteredClassificationDialog::showTodoMessage(const QString &actionName)
{
    QMessageBox::information(this,
                             actionName,
                             tr("%1 第一版暂未实现，当前仅预留控件入口。").arg(actionName));
}

void RegisteredClassificationDialog::updateModelLabels()
{
    if (!m_modelNameLabel || !m_modelPathLabel)
        return;
    m_modelNameLabel->setText(m_modelName.trimmed().isEmpty()
                              ? tr("未导入模型")
                              : tr("当前模型：%1").arg(m_modelName));
    m_modelPathLabel->setText(m_modelPath.trimmed().isEmpty()
                              ? tr("模型路径：未设置")
                              : tr("模型路径：%1").arg(m_modelPath));
}

QJsonObject RegisteredClassificationDialog::registeredClassificationParams() const
{
    QJsonObject params;
    params.insert(QStringLiteral("version"), 1);
    params.insert(QStringLiteral("paramMode"), m_allParamsMode ? QStringLiteral("all") : QStringLiteral("basic"));
    params.insert(QStringLiteral("modelPath"), m_modelPath);
    params.insert(QStringLiteral("modelName"), m_modelName);
    QString modelType = m_modelTypeComboBox->currentData().toString();
    if (modelType.trimmed().isEmpty())
        modelType = QStringLiteral("halcon_dl_classification");
    params.insert(QStringLiteral("modelType"), modelType);
    params.insert(QStringLiteral("detectRegionType"), m_detectRegionType);
    params.insert(QStringLiteral("roiNormalized"), rectToJson(effectiveRoiNormalized()));
    params.insert(QStringLiteral("enablePositionCorrection"), m_positionCorrectionEnabled);
    params.insert(QStringLiteral("positionCorrectionSource"), m_positionCorrectionSource);
    params.insert(QStringLiteral("topK"), m_topKSpinBox->value());
    return params;
}

QJsonObject RegisteredClassificationDialog::judgeRule() const
{
    QJsonObject rule;
    rule.insert(QStringLiteral("mode"), judgeMode());
    rule.insert(QStringLiteral("resultBasis"), resultBasisText());
    rule.insert(QStringLiteral("expectedLabel"), m_expectedLabelLineEdit->text().trimmed());
    rule.insert(QStringLiteral("minScore"), m_minScoreSpinBox->value());
    return rule;
}

QRectF RegisteredClassificationDialog::effectiveRoiNormalized() const
{
    return m_detectRegionType == QStringLiteral("rectangle")
            ? normalizedRoiOrDefault(m_roiNormalized)
            : QRectF(0.0, 0.0, 1.0, 1.0);
}

bool RegisteredClassificationDialog::validateImportedModelPath(const QString &path,
                                                               QString *errorMessage) const
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        if (errorMessage)
            *errorMessage = tr("模型文件不存在。");
        return false;
    }

    const QString suffix = info.suffix().trimmed().toLower();
    if (suffix == QStringLiteral("scbin")) {
        if (errorMessage)
            *errorMessage = tr(".scbin 为海康专有模型格式，HALCON 第一版不解析。");
        return false;
    }

    static const QRegularExpression namePattern(QStringLiteral("^[A-Za-z0-9_]+$"));
    if (!namePattern.match(info.completeBaseName()).hasMatch()) {
        if (errorMessage)
            *errorMessage = tr("模型文件名称只支持英文大小写字母、数字和下划线。");
        return false;
    }

    return true;
}

QString RegisteredClassificationDialog::judgeMode() const
{
    const QString mode = m_resultBasisComboBox->currentData().toString();
    return mode.trimmed().isEmpty() ? QStringLiteral("class_match") : mode;
}

QString RegisteredClassificationDialog::resultBasisText() const
{
    return m_resultBasisComboBox->currentText();
}

QString RegisteredClassificationDialog::roiStatusText() const
{
    if (m_detectRegionType == QStringLiteral("rectangle")) {
        const QRectF roi = effectiveRoiNormalized();
        return tr("检测区域：矩形 x=%1 y=%2 w=%3 h=%4")
                .arg(roi.x(), 0, 'f', 3)
                .arg(roi.y(), 0, 'f', 3)
                .arg(roi.width(), 0, 'f', 3)
                .arg(roi.height(), 0, 'f', 3);
    }
    return tr("检测区域：全屏");
}
