#include "RegisteredClassificationDialog.h"
#include "ui_RegisteredClassificationDialog.h"

#include "PlanDialogUtils.h"
#include "RegisteredClassificationModelManagementDialog.h"
#include "RegisteredClassificationTrainingDialog.h"
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameInputMetadata.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolRequest.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
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
#include <opencv2/imgcodecs.hpp>

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
        comboBox->addItem(source.displayText, source.sourceId);
        if (source.sourceId == selectedId)
            selectedIndex = comboBox->count() - 1;
    }
    if (selectedIndex < 0 && !selectedId.trimmed().isEmpty()) {
        comboBox->insertItem(
                    0,
                    QObject::tr("来源不可用：%1").arg(
                        selectedText.trimmed().isEmpty()
                        ? selectedId : selectedText),
                    selectedId);
        selectedIndex = 0;
    }
    if (selectedIndex < 0 && comboBox->count() > 0)
        selectedIndex = 0;
    if (selectedIndex >= 0)
        comboBox->setCurrentIndex(selectedIndex);
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
    , ui(new Ui::RegisteredClassificationDialog)
    , m_toolId(QStringLiteral("registered_classification_%1")
                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    m_testToolEngine.registerAdapter(&m_testTemplateLocationAdapter);
    m_testToolEngine.registerAdapter(&m_testPositionCorrectionAdapter);
    m_testToolEngine.registerAdapter(&m_testRegisteredClassificationAdapter);
    buildUi();
    connectControls();
    refreshUiState();
    refreshPreview();
}

RegisteredClassificationDialog::~RegisteredClassificationDialog()
{
    delete ui;
}

ToolConfig RegisteredClassificationDialog::toToolConfig() const
{
    ToolConfig config;
    config.toolId = m_toolId;
    config.toolName = QStringLiteral("RegisteredClassification");
    config.toolType = ToolType::RegisteredClassification;
    config.category = ToolCategory::Recognition;
    config.enabled = m_enabled;
    config.roiNormalized = effectiveRoiNormalized();
    const QJsonObject classificationParams = registeredClassificationParams();
    config.params.insert(QStringLiteral("registeredClassification"),
                         classificationParams);
    const PositionCorrectionConfig correction =
            PositionCorrection::fromParams(classificationParams);
    PositionCorrection::writeParams(correction, &config.params);
    config.params.insert(QStringLiteral("showPositionCorrectionMatchContour"),
                         m_showPositionCorrectionMatchContour);
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
    const PositionCorrectionConfig correction =
            PositionCorrection::fromParams(params);
    m_positionCorrectionEnabled = correction.enabled;
    m_positionCorrectionSource = correction.source;
    m_positionCorrectionSourceId = correction.sourceId;
    m_showPositionCorrectionMatchContour = params.value(
                QStringLiteral("showPositionCorrectionMatchContour"))
            .toBool(config.params.value(
                        QStringLiteral("showPositionCorrectionMatchContour"))
                    .toBool(true));

    const bool allMode = params.value(QStringLiteral("paramMode")).toString()
            == QStringLiteral("all");
    setAllParamsMode(allMode);

    if (m_topKSpinBox)
        m_topKSpinBox->setValue(qBound(1, params.value(QStringLiteral("topK")).toInt(1), 10));
    if (m_minSimilaritySpinBox) {
        m_minSimilaritySpinBox->setValue(
                    qBound(0, params.value(QStringLiteral("minSimilarity")).toInt(80), 100));
    }
    if (m_minMarginSpinBox) {
        m_minMarginSpinBox->setValue(
                    qBound(0, params.value(QStringLiteral("minMargin")).toInt(8), 100));
    }
    if (m_modelTypeComboBox) {
        const QString modelType = params.value(QStringLiteral("modelType"))
                .toString(registeredClassificationKnnModelType());
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

void RegisteredClassificationDialog::setToolChainTestContext(
        const QVector<ToolConfig> &toolConfigs,
        int currentToolIndex,
        const ReferencePositionCorrectionConfig &referencePositionCorrection)
{
    m_toolChainTestConfigs = toolConfigs;
    m_toolChainTestIndex = qBound(0, currentToolIndex, toolConfigs.size());
    m_referencePositionCorrection = referencePositionCorrection;
    const QVector<PositionCorrectionSource> sources =
            PositionCorrection::sourcesBefore(
                m_toolChainTestConfigs,
                m_toolChainTestIndex,
                m_referencePositionCorrection.enabled);
    populatePositionCorrectionCombo(
                m_positionSourceComboBox,
                sources,
                m_positionCorrectionSourceId,
                m_positionCorrectionSource);
    if (m_positionSourceComboBox
            && m_positionSourceComboBox->currentIndex() >= 0) {
        m_positionCorrectionSourceId =
                m_positionSourceComboBox->currentData().toString().trimmed();
        m_positionCorrectionSource =
                m_positionSourceComboBox->currentText();
    }
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
    if (m_importedTestActive) {
        runTest();
        return;
    }
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
    else {
        if (m_previewHelper) {
            m_previewHelper->clearToolOverlays();
            refreshRoiOverlay();
        }
        setViewerStatusText(roiStatusText());
    }
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

    const ToolConfig config = toToolConfig();
    const ToolResult result = runOnFrame(
                frame,
                frame,
                QStringLiteral("reference"));
    m_referencePreviewSnapshot =
            makeReferenceToolPreviewSnapshot(config, result, effectiveRoiNormalized());
    displayResult(result);
    if (m_referenceTestMode)
        setViewerStatusText(resultStatusText(result) + tr(" | 基准图持续测试已启用"));
}

void RegisteredClassificationDialog::runTest()
{
    const cv::Mat frame = m_importedTestActive
            ? m_importedTestFrame.clone()
            : CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        setViewerStatusText(tr("注册分类: image_empty | 当前相机帧为空"));
        if (m_previewHelper)
            m_previewHelper->clearToolOverlays();
        return;
    }

    const QImage image = MatImageConverter::matToDisplayImage(
                frame, QStringLiteral("RegisteredClassificationDialog"));
    if (!image.isNull() && m_previewHelper) {
        m_viewerTitleLabel->setText(
                    m_importedTestActive
                    ? (m_importedTestImageTitle.trimmed().isEmpty()
                       ? tr("PC导入图片") : m_importedTestImageTitle)
                    : tr("测试图像"));
        m_previewHelper->setImage(image);
        refreshRoiOverlay();
    }

    const ToolResult result = runOnFrame(
                frame,
                ReferenceImageProvider::instance().referenceFrame(),
                m_importedTestActive
                ? QStringLiteral("file")
                : QStringLiteral("camera"));
    displayResult(result);
}

void RegisteredClassificationDialog::importTestImageFromPc()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("PC导入注册分类测试图片"),
                QString(),
                tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All files (*.*)"));
    if (fileName.trimmed().isEmpty())
        return;
    const cv::Mat frame = cv::imread(
                fileName.toLocal8Bit().constData(),
                cv::IMREAD_UNCHANGED);
    if (frame.empty()) {
        QMessageBox::warning(this, tr("PC导入图片"), tr("无法读取所选图片"));
        return;
    }
    m_importedTestFrame = frame.clone();
    m_importedTestImageTitle = QFileInfo(fileName).fileName();
    m_importedTestActive = true;
    m_referenceTestMode = false;
    updateTestButtons();
    runTest();
}

void RegisteredClassificationDialog::exitTestMode()
{
    m_importedTestActive = false;
    m_importedTestFrame.release();
    m_importedTestImageTitle.clear();
    m_referenceTestMode = false;
    updateTestButtons();
    refreshPreview();
    if (m_previewHelper)
        m_previewHelper->clearToolOverlays();
    refreshRoiOverlay();
    setViewerStatusText(tr("已退出离线测试，可使用相机执行测试运行"));
}

ToolResult RegisteredClassificationDialog::runOnFrame(
        const cv::Mat &frame,
        const cv::Mat &referenceImage,
        const QString &inputSource)
{
    ToolConfig config = toToolConfig();
    config.enabled = true;
    const QString frameId = QStringLiteral("registered-classification-dialog-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QJsonObject runtimeContext;
    runtimeContext.insert(QStringLiteral("frameId"), frameId);
    runtimeContext.insert(
                QStringLiteral("input"),
                FrameInputMetadata::fromMat(frame, inputSource).toJson());
    runtimeContext.insert(
                QStringLiteral("referencePositionCorrection"),
                PositionCorrection::referenceToJson(
                    m_referencePositionCorrection));

    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        refreshRoiOverlay();
    }
    const PositionCorrectionConfig correction =
            PositionCorrection::fromParams(
                config.params.value(
                    QStringLiteral("registeredClassification")).toObject());
    setViewerStatusText(
                correction.enabled
                ? tr("正在重新执行模板定位、位置修正和注册分类…")
                : tr("正在重新执行注册分类…"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    if (correction.enabled) {
        if (m_toolChainTestIndex < 0
                || m_toolChainTestIndex > m_toolChainTestConfigs.size()) {
            return ToolResult::error(
                        config.toolId,
                        config.toolType,
                        tr("位置修正测试缺少当前方案的前置工具配置"),
                        QStringLiteral("position_correction_test_context_missing"));
        }
        QVector<ToolConfig> prefix;
        prefix.reserve(m_toolChainTestIndex + 1);
        for (int index = 0; index < m_toolChainTestIndex; ++index) {
            if (m_toolChainTestConfigs.at(index).enabled)
                prefix.append(m_toolChainTestConfigs.at(index));
        }
        prefix.append(config);
        ToolResult referenceCorrectionResult;
        const QVector<ToolResult> results = m_testToolEngine.runTools(
                    prefix,
                    frame.clone(),
                    referenceImage.empty() ? cv::Mat() : referenceImage.clone(),
                    runtimeContext,
                    &referenceCorrectionResult);
        for (auto it = results.crbegin(); it != results.crend(); ++it) {
            if (it->toolId == config.toolId)
                return *it;
        }
        return ToolResult::error(
                    config.toolId,
                    config.toolType,
                    tr("前置工具链没有返回注册分类结果"),
                    QStringLiteral("tool_chain_result_missing"));
    }

    ToolRequest request;
    request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.frameId = frameId;
    request.config = config;
    request.image = frame.clone();
    request.referenceImage =
            referenceImage.empty() ? cv::Mat() : referenceImage.clone();
    request.runtimeContext = runtimeContext;
    return m_testToolEngine.runTool(request);
}

void RegisteredClassificationDialog::updateTestButtons()
{
    const bool imported =
            m_importedTestActive && !m_importedTestFrame.empty();
    if (m_referenceTestButton)
        m_referenceTestButton->setVisible(!imported);
    if (m_testRunButton)
        m_testRunButton->setText(
                    imported ? tr("测试运行（导入图）") : tr("测试运行"));
    if (m_finishButton)
        m_finishButton->setText(imported ? tr("运行一次") : tr("完成"));
    if (m_exitTestButton)
        m_exitTestButton->setVisible(imported);
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
            const int index = m_modelTypeComboBox->findData(registeredClassificationKnnModelType());
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
            const int index = m_modelTypeComboBox->findData(registeredClassificationKnnModelType());
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
    if (m_importedTestActive) {
        runTest();
        return;
    }
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
    if (m_importedTestActive)
        runTest();
}

void RegisteredClassificationDialog::handleRoiChanged(const QRectF &roi)
{
    m_roiNormalized = normalizedRoiOrDefault(roi);
    m_detectRegionType = QStringLiteral("rectangle");
    refreshUiState();
    if (m_importedTestActive)
        runTest();
    else if (m_referenceTestMode)
        executeReferenceTest();
    else
        setViewerStatusText(roiStatusText());
}

void RegisteredClassificationDialog::handleRoiSelectionRejected()
{
    setViewerStatusText(tr("检测区域无效，请重新绘制矩形 ROI。"));
}

#if 0
void RegisteredClassificationDialog::buildLegacyUi()
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
    header->setObjectName(QStringLiteral("setupTopBar"));
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(28, 0, 22, 0);
    QLabel *headerTitle = new QLabel(tr("方案编辑"), header);
    headerTitle->setObjectName(QStringLiteral("setupWindowTitleLabel"));
    QToolButton *closeButton = new QToolButton(header);
    closeButton->setObjectName(QStringLiteral("registeredClassificationCloseButton"));
    closeButton->setProperty("actionRole", QStringLiteral("windowClose"));
    closeButton->setText(QStringLiteral("×"));
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);
    root->addWidget(header, 0);

    QHBoxLayout *content = new QHBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);
    root->addLayout(content, 1);

    QFrame *leftPanel = new QFrame(this);
    leftPanel->setObjectName(QStringLiteral("setupEditorPanel"));
    leftPanel->setMinimumWidth(610);
    leftPanel->setMaximumWidth(610);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(24, 18, 24, 18);
    leftLayout->setSpacing(14);
    content->addWidget(leftPanel, 0);

    QHBoxLayout *titleLayout = new QHBoxLayout;
    QLabel *dialogTitle = new QLabel(tr("注册分类"), leftPanel);
    dialogTitle->setObjectName(QStringLiteral("editorTitleLabel"));
    m_basicButton = new QPushButton(tr("基础"), leftPanel);
    m_allButton = new QPushButton(tr("全部"), leftPanel);
    m_basicButton->setObjectName(QStringLiteral("basicSegmentButton"));
    m_allButton->setObjectName(QStringLiteral("allSegmentButton"));
    m_pcImportButton = new QPushButton(tr("PC导入图片"), leftPanel);
    m_pcImportButton->setObjectName(
                QStringLiteral("registeredClassificationPcImportButton"));
    m_pcImportButton->setProperty("actionRole", QStringLiteral("secondary"));
    m_basicButton->setCheckable(true);
    m_allButton->setCheckable(true);
    m_segmentGroup->addButton(m_basicButton, 0);
    m_segmentGroup->addButton(m_allButton, 1);
    titleLayout->addWidget(dialogTitle);
    titleLayout->addWidget(m_pcImportButton);
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
    m_globalRegionButton->setProperty("actionRole", QStringLiteral("toolbarIcon"));
    m_rectRegionButton = new QToolButton(regionButtons);
    m_rectRegionButton->setObjectName(QStringLiteral("registeredClassificationRectRegionButton"));
    m_rectRegionButton->setText(QStringLiteral("□"));
    m_rectRegionButton->setToolTip(tr("矩形检测区域"));
    m_rectRegionButton->setCheckable(true);
    m_rectRegionButton->setProperty("actionRole", QStringLiteral("toolbarIcon"));
    m_regionGroup->setExclusive(true);
    m_regionGroup->addButton(m_globalRegionButton, 0);
    m_regionGroup->addButton(m_rectRegionButton, 1);
    m_roiFinishButton = new QPushButton(tr("完成"), regionButtons);
    m_roiFinishButton->setObjectName(QStringLiteral("registeredClassificationRoiFinishButton"));
    m_roiFinishButton->setProperty("actionRole", QStringLiteral("secondary"));
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
    m_positionSourceComboBox->setObjectName(
                QStringLiteral("registeredClassificationPositionCorrectionSourceComboBox"));
    m_positionSourceComboBox->addItem(PositionCorrection::defaultSource(),
                                      PositionCorrection::defaultSourceId());
    positionSourceLayout->addWidget(positionLabel);
    positionSourceLayout->addWidget(m_positionSourceComboBox, 1);
    detectLayout->addWidget(m_positionSourceRow);

    m_positionContourRow = new QWidget(detectCard);
    QHBoxLayout *positionContourLayout =
            new QHBoxLayout(m_positionContourRow);
    positionContourLayout->setContentsMargins(0, 0, 0, 0);
    positionContourLayout->addWidget(
                new QLabel(tr("显示匹配轮廓"), m_positionContourRow));
    positionContourLayout->addStretch(1);
    m_positionContourCheckBox = new QCheckBox(m_positionContourRow);
    m_positionContourCheckBox->setObjectName(
                QStringLiteral("registeredClassificationPositionCorrectionContourSwitch"));
    m_positionContourCheckBox->setChecked(true);
    positionContourLayout->addWidget(m_positionContourCheckBox);
    detectLayout->addWidget(m_positionContourRow);

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
    m_modelTypeComboBox->addItem(tr("HALCON KNN 注册分类"),
                                 registeredClassificationKnnModelType());
    m_modelTypeComboBox->hide();
    m_topKSpinBox = new QSpinBox(m_advancedCard);
    m_topKSpinBox->setRange(1, 10);
    m_topKSpinBox->setValue(1);
    m_minSimilaritySpinBox = new QSpinBox(m_advancedCard);
    m_minSimilaritySpinBox->setRange(0, 100);
    m_minSimilaritySpinBox->setValue(80);
    m_minMarginSpinBox = new QSpinBox(m_advancedCard);
    m_minMarginSpinBox->setObjectName(
                QStringLiteral("registeredClassificationMinMarginSpinBox"));
    m_minMarginSpinBox->setRange(0, 100);
    m_minMarginSpinBox->setValue(8);
    advancedLayout->addLayout(row(tr("前K个类别"), m_topKSpinBox));
    advancedLayout->addLayout(row(tr("最小相似度"), m_minSimilaritySpinBox));
    advancedLayout->addLayout(row(tr("最小类别差值"), m_minMarginSpinBox));
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
    m_exitTestButton = new QPushButton(tr("退出测试"), leftPanel);
    m_referenceTestButton->setCheckable(true);
    applyActionButtonMetrics(m_referenceTestButton);
    applyActionButtonMetrics(m_testRunButton);
    applyActionButtonMetrics(m_finishButton);
    applyActionButtonMetrics(m_exitTestButton);
    m_finishButton->setProperty("actionRole", QStringLiteral("testPrimary"));
    bottomButtons->addStretch(1);
    bottomButtons->addWidget(m_referenceTestButton);
    bottomButtons->addWidget(m_testRunButton);
    bottomButtons->addWidget(m_exitTestButton);
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
    QLabel *viewerCursorLabel = new QLabel(rightPanel);
    viewerCursorLabel->setObjectName(QStringLiteral("viewerCursorLabel"));
    viewerCursorLabel->setMinimumHeight(32);
    viewerCursorLabel->setContentsMargins(18, 0, 18, 0);
    viewerCursorLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rightLayout->addWidget(m_viewerTitleLabel);
    rightLayout->addWidget(m_previewGraphicsView, 1);
    rightLayout->addWidget(m_viewerStatusLabel);
    rightLayout->addWidget(viewerCursorLabel);
    content->addWidget(rightPanel, 1);

    m_previewHelper = new FrameViewHelper(m_previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(viewerCursorLabel);

    setStyleSheet(styleSheet() + QStringLiteral(
        "QPushButton[actionRole=\"testPrimary\"]{background:#111827;color:#ffffff;border:1px solid #111827;}"
        "QPushButton[actionRole=\"testPrimary\"]:hover{background:#000000;border-color:#000000;}"
        "QCheckBox{color:#111827;}"));

    m_basicButton->setChecked(true);
    m_globalRegionButton->setChecked(true);
    m_positionCorrectionCheckBox->setChecked(false);
    m_positionCorrectionCheckBox->setEnabled(true);
    m_positionCorrectionCheckBox->setToolTip(
                tr("启用后按所选稳定来源执行前置位置修正工具链。"));
    m_exitTestButton->hide();
}

#endif

void RegisteredClassificationDialog::buildUi()
{
    ui->setupUi(this);
    setWindowTitle(tr("方案编辑 - 注册分类"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(this);

    m_segmentGroup = new QButtonGroup(this);
    m_regionGroup = new QButtonGroup(this);
    m_judgeGroup = new QButtonGroup(this);

    m_basicButton = ui->basicSegmentButton;
    m_allButton = ui->allSegmentButton;
    m_pcImportButton = ui->registeredClassificationPcImportButton;
    m_globalRegionButton = ui->registeredClassificationGlobalRegionButton;
    m_rectRegionButton = ui->registeredClassificationRectRegionButton;
    m_roiFinishButton = ui->registeredClassificationRoiFinishButton;
    m_positionCorrectionCheckBox = ui->positionCorrectionSwitch;
    m_positionSourceRow = ui->positionSourceRow;
    m_positionSourceComboBox = ui->registeredClassificationPositionCorrectionSourceComboBox;
    m_positionContourRow = ui->positionContourRow;
    m_positionContourCheckBox = ui->registeredClassificationPositionCorrectionContourSwitch;
    m_modelNameLabel = ui->modelNameLabel;
    m_modelPathLabel = ui->modelPathLabel;
    m_importModelButton = ui->importModelButton;
    m_exportModelButton = ui->exportModelButton;
    m_deleteModelButton = ui->deleteModelButton;
    m_registerTrainingButton = ui->registerTrainingButton;
    m_modelManagementButton = ui->modelManagementButton;
    m_advancedCard = ui->advancedCard;
    m_modelTypeComboBox = ui->modelTypeComboBox;
    m_topKSpinBox = ui->topKSpinBox;
    m_minSimilaritySpinBox = ui->minSimilaritySpinBox;
    m_minMarginSpinBox = ui->registeredClassificationMinMarginSpinBox;
    m_resultBasisComboBox = ui->resultBasisComboBox;
    m_expectedLabelLineEdit = ui->expectedLabelLineEdit;
    m_minScoreSpinBox = ui->minScoreSpinBox;
    m_judgeTypeComboBox = ui->judgeTypeComboBox;
    m_referenceTestButton = ui->referenceTestButton;
    m_testRunButton = ui->testRunButton;
    m_exitTestButton = ui->exitTestButton;
    m_finishButton = ui->finishButton;
    m_viewerTitleLabel = ui->viewerTitleLabel;
    m_viewerStatusLabel = ui->viewerStatusLabel;
    m_previewGraphicsView = ui->previewGraphicsView;

    m_segmentGroup->addButton(m_basicButton, 0);
    m_segmentGroup->addButton(m_allButton, 1);
    m_regionGroup->setExclusive(true);
    m_regionGroup->addButton(m_globalRegionButton, 0);
    m_regionGroup->addButton(m_rectRegionButton, 1);

    m_positionSourceComboBox->addItem(PositionCorrection::defaultSource(),
                                      PositionCorrection::defaultSourceId());
    m_modelTypeComboBox->addItem(tr("HALCON KNN 注册分类"),
                                 registeredClassificationKnnModelType());
    m_resultBasisComboBox->addItem(tr("类别判断"), QStringLiteral("class_match"));
    m_resultBasisComboBox->addItem(tr("最低得分"), QStringLiteral("min_score"));
    m_judgeTypeComboBox->addItem(tr("所有检测区域输出结果为 OK"));
    m_judgeTypeComboBox->addItem(tr("任意检测区域输出结果为 OK"));

    applyActionButtonMetrics(m_referenceTestButton);
    applyActionButtonMetrics(m_testRunButton);
    applyActionButtonMetrics(m_finishButton);
    applyActionButtonMetrics(m_exitTestButton);

    m_previewHelper = new FrameViewHelper(m_previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);

    setStyleSheet(styleSheet() + QStringLiteral(
        "QPushButton[actionRole=\"testPrimary\"]{background:#111827;color:#ffffff;border:1px solid #111827;}"
        "QPushButton[actionRole=\"testPrimary\"]:hover{background:#000000;border-color:#000000;}"
        "QCheckBox{color:#111827;}"));

    m_basicButton->setChecked(true);
    m_globalRegionButton->setChecked(true);
    m_positionCorrectionCheckBox->setChecked(false);
    m_positionCorrectionCheckBox->setEnabled(true);
    m_positionCorrectionCheckBox->setToolTip(
                tr("启用后按所选稳定来源执行前置位置修正工具链。"));
    m_exitTestButton->hide();
}

void RegisteredClassificationDialog::connectControls()
{
    if (QToolButton *closeButton =
            findChild<QToolButton *>(QStringLiteral("registeredClassificationCloseButton"))) {
        connect(closeButton, &QToolButton::clicked, this, &RegisteredClassificationDialog::reject);
    }
    connect(m_basicButton, &QPushButton::clicked, this, [this]() { setAllParamsMode(false); });
    connect(m_allButton, &QPushButton::clicked, this, [this]() { setAllParamsMode(true); });
    connect(m_pcImportButton, &QPushButton::clicked,
            this, &RegisteredClassificationDialog::importTestImageFromPc);
    connect(m_exitTestButton, &QPushButton::clicked,
            this, &RegisteredClassificationDialog::exitTestMode);
    connect(m_globalRegionButton, &QToolButton::clicked, this, &RegisteredClassificationDialog::startGlobalDetection);
    connect(m_rectRegionButton, &QToolButton::clicked, this, &RegisteredClassificationDialog::startRectangleRoiEditing);
    connect(m_roiFinishButton, &QPushButton::clicked, this, &RegisteredClassificationDialog::finishRoiEditing);
    connect(m_positionCorrectionCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_positionCorrectionEnabled = checked;
        refreshUiState();
    });
    connect(m_positionSourceComboBox, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        m_positionCorrectionSource = text;
        m_positionCorrectionSourceId =
                m_positionSourceComboBox->currentData().toString().trimmed();
    });
    connect(m_positionContourCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
        m_showPositionCorrectionMatchContour = checked;
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
    m_positionCorrectionCheckBox->setEnabled(true);
    if (m_positionSourceRow)
        m_positionSourceRow->setVisible(m_positionCorrectionEnabled);
    if (m_positionContourRow)
        m_positionContourRow->setVisible(m_positionCorrectionEnabled);
    if (m_positionContourCheckBox) {
        const QSignalBlocker contourBlocker(m_positionContourCheckBox);
        m_positionContourCheckBox->setChecked(
                    m_showPositionCorrectionMatchContour);
    }
    const bool classMode = judgeMode() == QStringLiteral("class_match");
    m_expectedLabelLineEdit->setVisible(classMode);
    m_minScoreSpinBox->setVisible(!classMode);
    updateModelLabels();
    updateTestButtons();
}

void RegisteredClassificationDialog::refreshPreview()
{
    const QImage reference = ReferenceImageProvider::instance().referenceImage();
    if (m_previewHelper) {
        if (!reference.isNull()) {
            m_previewHelper->setImage(reference);
            m_previewHelper->clearToolOverlays();
        } else {
            m_previewHelper->clear();
        }
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
        bool hasRuntimeDetectRoi = false;
        for (const ToolOverlay &overlay : result.overlays) {
            if (overlay.extra.value(QStringLiteral("role")).toString()
                    == QStringLiteral("detect_roi")
                    || overlay.label == QStringLiteral("detect_roi")) {
                hasRuntimeDetectRoi = true;
                break;
            }
        }
        if (hasRuntimeDetectRoi)
            m_previewHelper->clearRoi();
        else
            refreshRoiOverlay();
        if (!result.overlays.isEmpty())
            m_previewHelper->setToolOverlays(result.overlays);
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
    params.insert(QStringLiteral("version"), 2);
    params.insert(QStringLiteral("paramMode"), m_allParamsMode ? QStringLiteral("all") : QStringLiteral("basic"));
    params.insert(QStringLiteral("modelPath"), m_modelPath);
    params.insert(QStringLiteral("modelName"), m_modelName);
    QString modelType = m_modelTypeComboBox
            ? m_modelTypeComboBox->currentData().toString()
            : QString();
    if (modelType.trimmed().isEmpty())
        modelType = registeredClassificationKnnModelType();
    params.insert(QStringLiteral("modelType"), modelType);
    params.insert(QStringLiteral("detectRegionType"), m_detectRegionType);
    params.insert(QStringLiteral("roiNormalized"), rectToJson(effectiveRoiNormalized()));
    PositionCorrection::writeParams(PositionCorrectionConfig{
                                        m_positionCorrectionEnabled,
                                        m_positionCorrectionSource,
                                        m_positionSourceComboBox
                                        ? m_positionSourceComboBox
                                          ->currentData().toString().trimmed()
                                        : m_positionCorrectionSourceId},
                                    &params);
    params.insert(QStringLiteral("showPositionCorrectionMatchContour"),
                  m_showPositionCorrectionMatchContour);
    params.insert(QStringLiteral("topK"), m_topKSpinBox->value());
    params.insert(QStringLiteral("minSimilarity"), m_minSimilaritySpinBox->value());
    params.insert(QStringLiteral("minMargin"), m_minMarginSpinBox->value());
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

    const RegisteredClassificationModelPackageResult packageResult =
            validateRegisteredClassificationKnnPackage(info.absoluteFilePath());
    if (!packageResult.success) {
        if (errorMessage) {
            *errorMessage = packageResult.status == QStringLiteral("legacy_model_requires_retraining")
                    ? tr("旧版模型不能直接运行，请在模型管理中重新训练")
                    : packageResult.message;
        }
        return false;
    }

    return true;
}

bool RegisteredClassificationDialog::validateModelPackageForTest(const QString &path,
                                                                 QString *errorMessage) const
{
    return validateImportedModelPath(path, errorMessage);
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
