#include "RegisteredClassificationDialog.h"

#include "PlanDialogUtils.h"
#include "RegisteredClassificationModelManagementDialog.h"
#include "RegisteredClassificationTrainingDialog.h"
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolRequest.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
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

void setComboBoxText(QComboBox *comboBox, const QString &text)
{
    if (!comboBox || text.trimmed().isEmpty())
        return;
    const int index = comboBox->findText(text);
    if (index >= 0) {
        comboBox->setCurrentIndex(index);
        return;
    }

    if (text.contains(QObject::tr("任意"))) {
        const int anyIndex = comboBox->findText(judgeTypeText(QStringLiteral("any_ok")));
        if (anyIndex >= 0)
            comboBox->setCurrentIndex(anyIndex);
        return;
    }

    if (text.contains(QObject::tr("所有"))) {
        const int allIndex = comboBox->findText(judgeTypeText(QStringLiteral("all_ok")));
        if (allIndex >= 0)
            comboBox->setCurrentIndex(allIndex);
    }
}

bool copyDirectoryRecursively(const QString &sourcePath,
                              const QString &targetPath,
                              QString *errorMessage)
{
    const QDir sourceDir(sourcePath);
    if (!sourceDir.exists()) {
        if (errorMessage)
            *errorMessage = QObject::tr("源模型目录不存在。");
        return false;
    }

    QDir targetDir(targetPath);
    if (!targetDir.exists() && !QDir().mkpath(targetPath)) {
        if (errorMessage)
            *errorMessage = QObject::tr("目标模型目录创建失败。");
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(
                QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo &entry : entries) {
        const QString targetEntryPath = targetDir.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryRecursively(entry.absoluteFilePath(), targetEntryPath, errorMessage))
                return false;
            continue;
        }
        if (QFile::exists(targetEntryPath) && !QFile::remove(targetEntryPath)) {
            if (errorMessage)
                *errorMessage = QObject::tr("目标模型文件覆盖失败：%1").arg(entry.fileName());
            return false;
        }
        if (!QFile::copy(entry.absoluteFilePath(), targetEntryPath)) {
            if (errorMessage)
                *errorMessage = QObject::tr("模型文件复制失败：%1").arg(entry.fileName());
            return false;
        }
    }
    return true;
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
    m_positionCorrectionEnabled = false;
    m_positionCorrectionSource = PositionCorrection::fromParams(params).source;

    const bool allMode = params.value(QStringLiteral("paramMode")).toString()
            == QStringLiteral("all");
    setAllParamsMode(allMode);

    if (m_topKSpinBox)
        m_topKSpinBox->setValue(qBound(1, params.value(QStringLiteral("topK")).toInt(1), 10));
    if (m_minSimilaritySpinBox) {
        m_minSimilaritySpinBox->setValue(
                    qBound(0, params.value(QStringLiteral("minSimilarity")).toInt(68), 100));
    }
    if (m_modelTypeComboBox) {
        const QString modelType = params.value(QStringLiteral("modelType"))
                .toString(registeredClassificationMlpModelType());
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
    if (m_judgeTypeComboBox) {
        QString typeText = rule.value(QStringLiteral("judgeTypeText")).toString();
        if (typeText.trimmed().isEmpty())
            typeText = judgeTypeText(rule.value(QStringLiteral("judgeType")).toString());
        setComboBoxText(m_judgeTypeComboBox, typeText);
    }

    refreshUiState();
    refreshPreview();
}

QString RegisteredClassificationDialog::summaryText() const
{
    const QString modelText = m_modelName.trimmed().isEmpty() ? tr("未导入模型") : m_modelName;
    const QString typeText = m_judgeTypeComboBox
            ? m_judgeTypeComboBox->currentText()
            : judgeTypeText(QStringLiteral("all_ok"));
    if (judgeMode() == QStringLiteral("min_score"))
        return tr("模型：%1；最低得分：%2；%3")
                .arg(modelText)
                .arg(m_minScoreSpinBox->value())
                .arg(typeText);
    return tr("模型：%1；类别：%2；%3")
            .arg(modelText,
                 m_expectedLabelLineEdit->text().trimmed().isEmpty()
                 ? tr("未设置")
                 : m_expectedLabelLineEdit->text().trimmed(),
                 typeText);
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
    const cv::Mat frame = ReferenceImageProvider::instance().referenceFrame();
    if (frame.empty()) {
        m_referenceTestMode = false;
        refreshUiState();
        setViewerStatusText(tr("注册分类: no_reference_image | 请先设置基准图"));
        if (m_previewHelper)
            m_previewHelper->clearToolOverlays();
        return;
    }

    m_referenceTestMode = !m_referenceTestMode;
    refreshUiState();
    if (m_referenceTestMode)
        executeReferenceTest();
    else
        setViewerStatusText(roiStatusText());
}

void RegisteredClassificationDialog::executeReferenceTest()
{
    const cv::Mat frame = ReferenceImageProvider::instance().referenceFrame();
    if (frame.empty()) {
        m_referenceTestMode = false;
        refreshUiState();
        setViewerStatusText(tr("注册分类: no_reference_image | 请先设置基准图"));
        if (m_previewHelper)
            m_previewHelper->clearToolOverlays();
        return;
    }

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (!image.isNull() && m_previewHelper) {
        m_viewerTitleLabel->setText(tr("基准图"));
        m_previewHelper->setImage(image);
        refreshRoiOverlay();
    }

    ToolRequest request;
    request.config = toToolConfig();
    request.image = frame.clone();
    request.referenceImage = frame.clone();
    const ToolResult result = m_placeholderAdapter.run(request);
    m_referencePreviewSnapshot =
            makeReferenceToolPreviewSnapshot(request.config, result, effectiveRoiNormalized());
    displayResult(result);
    if (m_referenceTestMode)
        setViewerStatusText(resultStatusText(result) + tr(" | 基准图持续测试已启用"));
}

void RegisteredClassificationDialog::runTest()
{
    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        setViewerStatusText(tr("注册分类: image_empty | 当前相机帧为空"));
        if (m_previewHelper)
            m_previewHelper->clearToolOverlays();
        return;
    }

    const QImage image = MatImageConverter::matToDisplayImage(
                frame, QStringLiteral("RegisteredClassificationDialog"));
    if (!image.isNull() && m_previewHelper) {
        m_viewerTitleLabel->setText(tr("测试图像"));
        m_previewHelper->setImage(image);
        refreshRoiOverlay();
    }

    ToolRequest request;
    request.config = toToolConfig();
    request.image = frame.clone();
    request.referenceImage = ReferenceImageProvider::instance().referenceFrame();
    const ToolResult result = m_placeholderAdapter.run(request);
    displayResult(result);
}

void RegisteredClassificationDialog::importModel()
{
    const QString path = QFileDialog::getExistingDirectory(
                this,
                tr("导入注册分类模型包目录"),
                QString());
    if (path.trimmed().isEmpty())
        return;

    QString errorMessage;
    if (!validateImportedModelPath(path, &errorMessage)) {
        QMessageBox::warning(this, tr("导入模型"), errorMessage);
        return;
    }

    const QFileInfo info(QDir::cleanPath(path));
    m_modelPath = info.absoluteFilePath();
    m_modelName = info.fileName();
    updateModelLabels();
    setViewerStatusText(tr("模型已导入：%1").arg(m_modelName));
}

void RegisteredClassificationDialog::exportModel()
{
    if (m_modelPath.trimmed().isEmpty()) {
        QMessageBox::information(this, tr("导出模型"), tr("当前未导入模型。"));
        return;
    }

    const QFileInfo sourceInfo(QDir::cleanPath(m_modelPath));
    const QString targetParent = QFileDialog::getExistingDirectory(
                this,
                tr("选择注册分类模型包导出目录"),
                QString());
    if (targetParent.trimmed().isEmpty())
        return;

    const QString target = QDir(targetParent).filePath(sourceInfo.fileName());
    if (QFileInfo::exists(target)) {
        QMessageBox::warning(this,
                             tr("导出模型"),
                             tr("目标目录已存在：%1").arg(target));
        return;
    }

    QString errorMessage;
    if (!copyDirectoryRecursively(m_modelPath, target, &errorMessage)) {
        QMessageBox::warning(this, tr("导出模型"), errorMessage);
        return;
    }
    setViewerStatusText(tr("模型已导出：%1").arg(target));
}

void RegisteredClassificationDialog::deleteModel()
{
    m_modelPath.clear();
    m_modelName.clear();
    updateModelLabels();
    setViewerStatusText(tr("模型已删除。"));
}

void RegisteredClassificationDialog::openRegisterTraining()
{
    auto *dialog = new RegisteredClassificationTrainingDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &RegisteredClassificationTrainingDialog::trainingCompleted,
            this, [this](const QString &modelDir, const QString &modelName) {
        m_modelPath = modelDir;
        m_modelName = modelName.trimmed().isEmpty()
                ? QFileInfo(modelDir).fileName()
                : modelName;
        if (m_modelTypeComboBox) {
            const int index = m_modelTypeComboBox->findData(registeredClassificationMlpModelType());
            if (index >= 0)
                m_modelTypeComboBox->setCurrentIndex(index);
        }
        updateModelLabels();
        setViewerStatusText(tr("注册分类模型训练完成：%1").arg(m_modelName));
    });
    dialog->show();
}

void RegisteredClassificationDialog::openModelManagement()
{
    auto *dialog = new RegisteredClassificationModelManagementDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &RegisteredClassificationModelManagementDialog::modelSelected,
            this, [this](const QString &modelDir, const QString &modelName) {
        m_modelPath = modelDir;
        m_modelName = modelName.trimmed().isEmpty()
                ? QFileInfo(modelDir).fileName()
                : modelName;
        if (m_modelTypeComboBox) {
            const int index = m_modelTypeComboBox->findData(registeredClassificationMlpModelType());
            if (index >= 0)
                m_modelTypeComboBox->setCurrentIndex(index);
        }
        updateModelLabels();
        setViewerStatusText(tr("已选择注册分类模型：%1").arg(m_modelName));
    });
    dialog->show();
}

void RegisteredClassificationDialog::startGlobalDetection()
{
    m_detectRegionType = QStringLiteral("full");
    m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    m_roiEditing = false;
    if (m_previewHelper)
        m_previewHelper->setRoiDrawingEnabled(false);
    refreshUiState();
    refreshRoiOverlay();
    if (m_referenceTestMode)
        executeReferenceTest();
}

void RegisteredClassificationDialog::startRectangleRoiEditing()
{
    m_detectRegionType = QStringLiteral("rectangle");
    m_roiEditing = true;
    if (m_previewHelper) {
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
        m_previewHelper->setRoiDrawingEnabled(true);
    }
    refreshUiState();
    setViewerStatusText(tr("请在右侧图像上绘制矩形检测区域，完成后点击“完成”。"));
}

void RegisteredClassificationDialog::finishRoiEditing()
{
    m_roiEditing = false;
    if (m_previewHelper)
        m_previewHelper->setRoiDrawingEnabled(false);
    refreshUiState();
    refreshRoiOverlay();
    setViewerStatusText(roiStatusText()
                        + (m_referenceTestMode ? tr(" | 基准图持续测试已启用") : QString()));
}

void RegisteredClassificationDialog::handleRoiChanged(const QRectF &roi)
{
    m_roiNormalized = normalizedRoiOrDefault(roi);
    m_detectRegionType = QStringLiteral("rectangle");
    refreshUiState();
    if (m_referenceTestMode)
        executeReferenceTest();
    else
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
    m_globalRegionButton->setObjectName(QStringLiteral("registeredClassificationGlobalRegionButton"));
    m_globalRegionButton->setText(QStringLiteral("▣"));
    m_globalRegionButton->setToolTip(tr("全屏检测"));
    m_globalRegionButton->setCheckable(true);
    m_rectRegionButton = new QToolButton(regionButtons);
    m_rectRegionButton->setObjectName(QStringLiteral("registeredClassificationRectRegionButton"));
    m_rectRegionButton->setText(QStringLiteral("□"));
    m_rectRegionButton->setToolTip(tr("矩形检测区域"));
    m_rectRegionButton->setCheckable(true);
    m_regionGroup->setExclusive(true);
    m_regionGroup->addButton(m_globalRegionButton, 0);
    m_regionGroup->addButton(m_rectRegionButton, 1);
    m_roiFinishButton = new QPushButton(tr("完成"), regionButtons);
    m_roiFinishButton->setObjectName(QStringLiteral("registeredClassificationRoiFinishButton"));
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
    paramsLayout->addWidget(detectCard);

    m_advancedCard = card(scrollContent, tr("参数设置"));
    QVBoxLayout *advancedLayout = qobject_cast<QVBoxLayout *>(m_advancedCard->layout());
    m_modelTypeComboBox = new QComboBox(m_advancedCard);
    m_modelTypeComboBox->addItem(tr("HALCON MLP 注册分类"),
                                 registeredClassificationMlpModelType());
    m_modelTypeComboBox->hide();
    m_topKSpinBox = new QSpinBox(m_advancedCard);
    m_topKSpinBox->setRange(1, 10);
    m_topKSpinBox->setValue(1);
    m_minSimilaritySpinBox = new QSpinBox(m_advancedCard);
    m_minSimilaritySpinBox->setRange(0, 100);
    m_minSimilaritySpinBox->setValue(68);
    advancedLayout->addLayout(row(tr("前K个类别"), m_topKSpinBox));
    advancedLayout->addLayout(row(tr("最小相似度"), m_minSimilaritySpinBox));
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
    m_judgeTypeComboBox = new QComboBox(judgeCard);
    m_judgeTypeComboBox->addItem(tr("所有检测区域输出结果为 OK"));
    m_judgeTypeComboBox->addItem(tr("任意检测区域输出结果为 OK"));
    judgeLayout->addLayout(row(tr("判断依据"), m_resultBasisComboBox));
    judgeLayout->addLayout(row(tr("类别名称"), m_expectedLabelLineEdit));
    judgeLayout->addLayout(row(tr("最低得分"), m_minScoreSpinBox));
    judgeLayout->addLayout(row(tr("判断类型"), m_judgeTypeComboBox));
    paramsLayout->addWidget(judgeCard);
    paramsLayout->addStretch(1);

    QHBoxLayout *bottomButtons = new QHBoxLayout;
    m_referenceTestButton = new QPushButton(tr("基准图测试"), leftPanel);
    m_testRunButton = new QPushButton(tr("测试运行"), leftPanel);
    m_finishButton = new QPushButton(tr("完成"), leftPanel);
    m_referenceTestButton->setCheckable(true);
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
    m_positionCorrectionCheckBox->setChecked(false);
    m_positionCorrectionCheckBox->setEnabled(false);
    m_positionCorrectionCheckBox->setToolTip(tr("位置修正补偿尚未实现，当前版本默认关闭。"));
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
    m_referenceTestButton->setChecked(m_referenceTestMode);
    m_regionGroup->setExclusive(false);
    m_globalRegionButton->setChecked(m_detectRegionType == QStringLiteral("full") && !m_roiEditing);
    m_rectRegionButton->setChecked(m_roiEditing);
    m_regionGroup->setExclusive(true);
    m_positionCorrectionCheckBox->setChecked(m_positionCorrectionEnabled);
    m_positionCorrectionCheckBox->setEnabled(false);
    if (m_positionSourceRow)
        m_positionSourceRow->setVisible(false);
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

void RegisteredClassificationDialog::displayResult(const ToolResult &result)
{
    setViewerStatusText(resultStatusText(result));
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        if (!result.overlays.isEmpty())
            m_previewHelper->setToolOverlays(result.overlays);
        else
            refreshRoiOverlay();
    }
}

QString RegisteredClassificationDialog::resultStatusText(const ToolResult &result) const
{
    if (!result.success) {
        return tr("注册分类: %1 | %2 | 耗时:%3ms")
                .arg(result.status,
                     result.message,
                     QString::number(result.elapsedMs));
    }

    const QString state = result.ok ? QStringLiteral("OK") : QStringLiteral("NG");
    const QString label = result.payload.value(QStringLiteral("predictedLabel"))
            .toString(result.text);
    const double score = result.payload.value(QStringLiteral("score")).toDouble(result.score);

    QStringList topKTexts;
    const QJsonArray topClasses = result.payload.value(QStringLiteral("topClasses")).toArray();
    for (const QJsonValue &value : topClasses) {
        const QJsonObject item = value.toObject();
        const QString itemLabel = item.value(QStringLiteral("label")).toString();
        if (itemLabel.trimmed().isEmpty())
            continue;
        topKTexts.append(QStringLiteral("%1:%2%")
                         .arg(itemLabel,
                              QString::number(item.value(QStringLiteral("score")).toDouble(),
                                              'f',
                                              1)));
    }

    const QString topKText = topKTexts.isEmpty()
            ? QStringLiteral("-")
            : topKTexts.join(QStringLiteral(", "));
    return tr("注册分类: %1 | 类别:%2 | 分数:%3% | TopK:%4 | 耗时:%5ms")
            .arg(state,
                 label.trimmed().isEmpty() ? QStringLiteral("-") : label,
                 QString::number(score, 'f', 1),
                 topKText,
                 QString::number(result.elapsedMs));
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
    QString modelType = m_modelTypeComboBox
            ? m_modelTypeComboBox->currentData().toString()
            : QString();
    if (modelType.trimmed().isEmpty())
        modelType = registeredClassificationMlpModelType();
    params.insert(QStringLiteral("modelType"), modelType);
    params.insert(QStringLiteral("detectRegionType"), m_detectRegionType);
    params.insert(QStringLiteral("roiNormalized"), rectToJson(effectiveRoiNormalized()));
    PositionCorrection::writeParams(PositionCorrectionConfig{
                                        m_positionCorrectionEnabled,
                                        m_positionCorrectionSource},
                                    &params);
    params.insert(QStringLiteral("topK"), m_topKSpinBox->value());
    params.insert(QStringLiteral("minSimilarity"), m_minSimilaritySpinBox->value());
    return params;
}

QJsonObject RegisteredClassificationDialog::judgeRule() const
{
    QJsonObject rule;
    rule.insert(QStringLiteral("mode"), judgeMode());
    rule.insert(QStringLiteral("resultBasis"), resultBasisText());
    rule.insert(QStringLiteral("expectedLabel"), m_expectedLabelLineEdit->text().trimmed());
    rule.insert(QStringLiteral("minScore"), m_minScoreSpinBox->value());
    const QString typeText = m_judgeTypeComboBox
            ? m_judgeTypeComboBox->currentText()
            : judgeTypeText(QStringLiteral("all_ok"));
    rule.insert(QStringLiteral("judgeType"), judgeTypeMode(typeText));
    rule.insert(QStringLiteral("judgeTypeText"), typeText);
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
    const QFileInfo info(QDir::cleanPath(path));
    if (!info.exists() || !info.isDir()) {
        if (errorMessage)
            *errorMessage = tr("请选择注册分类模型包目录。");
        return false;
    }

    if (!QFileInfo::exists(registeredClassificationMetadataPath(info.absoluteFilePath())) ||
        !QFileInfo::exists(registeredClassificationMlpPath(info.absoluteFilePath()))) {
        if (errorMessage)
            *errorMessage = tr("模型包目录必须包含 metadata.json 和 model.gmc。");
        return false;
    }

    RegisteredClassificationModelMetadata metadata;
    const RegisteredClassificationModelPackageResult readResult =
            readRegisteredClassificationMetadata(info.absoluteFilePath(), &metadata);
    if (!readResult.success) {
        if (errorMessage)
            *errorMessage = readResult.message;
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
