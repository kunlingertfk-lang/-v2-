#include "PositionCorrectionDialog.h"

#include <QJsonValue>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QUuid>
#include <QtGlobal>

#include <cmath>

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolRequest.h"
#include "toolcore/ToolResult.h"
#include "ui_PositionCorrectionDialog.h"

namespace {

PositionRunPoseSource runPoseSource(const QString &producerId,
                                    const QString &displayText)
{
    PositionRunPoseSource source;
    source.producerId = producerId;
    source.displayText = displayText;
    source.xKey = QStringLiteral("x");
    source.yKey = QStringLiteral("y");
    source.angleKey = QStringLiteral("angle");
    source.scaleKey = QStringLiteral("scale");
    source.valid = !producerId.trimmed().isEmpty();
    return source;
}

QString fieldDisplay(const PositionRunPoseSource &source, const QString &fieldText)
{
    const QString nodeText = source.displayText.trimmed().isEmpty()
            ? source.producerId
            : source.displayText.trimmed();
    return nodeText.trimmed().isEmpty()
            ? QString()
            : QStringLiteral("%1.%2").arg(nodeText, fieldText);
}

bool finiteJsonNumber(const QJsonObject &payload, const QString &key, double *value)
{
    if (!value || !payload.contains(key))
        return false;
    const QJsonValue jsonValue = payload.value(key);
    bool ok = true;
    const double number = jsonValue.isString()
            ? jsonValue.toString().toDouble(&ok)
            : jsonValue.toDouble(qQNaN());
    if (!ok || !std::isfinite(number))
        return false;
    *value = number;
    return true;
}

QJsonObject poseJson(double x, double y, double angleDeg, double scale = 1.0)
{
    return QJsonObject{
        {QStringLiteral("x"), x},
        {QStringLiteral("y"), y},
        {QStringLiteral("angleDeg"), angleDeg},
        {QStringLiteral("scale"), scale}
    };
}

} // namespace

// 初始化位置修正 Dialog 的布局、默认配置、基准图预览和交互信号。
PositionCorrectionDialog::PositionCorrectionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PositionCorrectionDialog)
{
    ui->setupUi(this);
    ui->previewLayout->setStretch(0, 0);
    ui->previewLayout->setStretch(1, 1);
    ui->previewLayout->setStretch(2, 0);
    setWindowTitle(tr("方案编辑 - 位置修正"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    connect(ui->closeButton, &QToolButton::clicked, this, &QDialog::reject);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);

    m_config.toolId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_config.toolType = ToolType::PositionCorrection;
    m_config.category = ToolCategory::Location;
    m_config.toolName = tr("位置修正");
    m_config.displayName = tr("位置修正");
    m_config.enabled = true;

    m_correction.insert(QStringLiteral("version"), 2);
    m_correction.insert(QStringLiteral("referenceCreated"), false);

    setupBindings();

    const QImage reference = ReferenceImageProvider::instance().referenceImage();
    if (reference.isNull()) {
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
    } else {
        ui->viewerTitleLabel->setText(tr("基准图"));
        m_previewHelper->setImage(reference);
    }

    // 基础与全部页签保持互斥；两者当前共用位置修正核心参数。
    connect(ui->basicModeButton, &QPushButton::clicked, this, [this]() {
        ui->basicModeButton->setChecked(true);
        ui->allModeButton->setChecked(false);
    });
    connect(ui->allModeButton, &QPushButton::clicked, this, [this]() {
        ui->allModeButton->setChecked(true);
        ui->basicModeButton->setChecked(false);
        ui->statusLabel->setText(tr("全部参数与基础参数保持同步"));
    });
    connect(ui->createReferenceButton, &QPushButton::clicked,
            this, &PositionCorrectionDialog::createReferencePose);
    connect(ui->testRunButton, &QPushButton::clicked,
            this, &PositionCorrectionDialog::runPositionCorrectionTest);
    connect(ui->finishButton, &QPushButton::clicked, this, [this]() {
        if (validateForFinish())
            accept();
    });
}

// 释放 Qt Designer 生成的界面对象。
PositionCorrectionDialog::~PositionCorrectionDialog()
{
    delete ui;
}

// 将三项默认/已有绑定回显到输入框，并建立可选来源菜单。
void PositionCorrectionDialog::setupBindings()
{
    updateRunPoseDisplay();

    rebuildBindingMenus();
}

// 保存完整工具链和当前消费位置，仅允许菜单引用当前工具之前的节点。
void PositionCorrectionDialog::setAvailableProducers(const QVector<ToolConfig> &tools,
                                                      int consumerIndex,
                                                      const QVector<PositionReferencePoseProducer> &referenceProducers)
{
    m_producers = tools;
    m_consumerIndex = qBound(0, consumerIndex, tools.size());
    m_referencePoseProducers = referenceProducers;
    m_poseProducers = PositionCorrection::poseProducersBefore(m_producers, m_consumerIndex);
    for (int index = m_referencePoseProducers.size() - 1; index >= 0; --index) {
        const PositionReferencePoseProducer &reference = m_referencePoseProducers.at(index);
        if (reference.sourceId.trimmed().isEmpty() || reference.referencePose.isEmpty())
            continue;
        m_poseProducers.prepend(PositionPoseProducer{
                                    reference.sourceId,
                                    reference.displayText,
                                    -1,
                                    QVector<PositionPoseField>{
                                        {QStringLiteral("x"), tr("基准位姿X"), QStringLiteral("coordinate")},
                                        {QStringLiteral("y"), tr("基准位姿Y"), QStringLiteral("coordinate")},
                                        {QStringLiteral("angle"), tr("基准位姿角度"), QStringLiteral("angle")}
                                    }});
    }
    rebuildBindingMenus();
}

// 为 X、Y、角度分别创建“上游节点 -> 输出字段”的二级绑定菜单。
void PositionCorrectionDialog::rebuildBindingMenus()
{
    const QList<QPushButton *> targets{
        ui->runPointXLinkButton,
        ui->runPointYLinkButton,
        ui->runAngleLinkButton
    };
    for (QPushButton *button : targets) {
        QMenu *menu = new QMenu(button);
        if (m_poseProducers.isEmpty()) {
            QAction *emptyAction = menu->addAction(tr("无可用位姿来源"));
            emptyAction->setEnabled(false);
        }
        for (const PositionPoseProducer &producer : m_poseProducers) {
            QMenu *nodeMenu = menu->addMenu(producer.displayText);
            QAction *selectAction = nodeMenu->addAction(tr("订阅运行点X/Y/运行角度"));
            connect(selectAction, &QAction::triggered, this,
                    [this, producer]() {
                setRunPoseSource(runPoseSource(producer.producerId,
                                               producer.displayText));
            });
            nodeMenu->addSeparator();
            for (const PositionPoseField &field : producer.fields) {
                QAction *fieldAction = nodeMenu->addAction(
                            QStringLiteral("%1 (%2)").arg(field.displayText, field.outputKey));
                fieldAction->setEnabled(false);
            }
        }
        button->setMenu(menu);
    }
}

void PositionCorrectionDialog::setRunPoseSource(const PositionRunPoseSource &source)
{
    PositionCorrection::writeRunPoseSource(source, &m_correction);
    updateRunPoseDisplay();
}

void PositionCorrectionDialog::updateRunPoseDisplay()
{
    const PositionRunPoseSource source =
            PositionCorrection::runPoseSourceFromConfig(m_correction);
    ui->runPointXEdit->setText(fieldDisplay(source, tr("运行点X")));
    ui->runPointYEdit->setText(fieldDisplay(source, tr("运行点Y")));
    ui->runAngleEdit->setText(fieldDisplay(source, tr("运行角度")));
    if (source.inconsistent)
        ui->statusLabel->setText(tr("X/Y/角度必须来自同一个上游实例，请重新选择运行姿态来源"));
}

// 加载已有实例配置，同时强制保持位置修正的工具类型和定位分类。
void PositionCorrectionDialog::loadFromConfig(const ToolConfig &config)
{
    m_config = config;
    m_config.toolType = ToolType::PositionCorrection;
    m_config.category = ToolCategory::Location;
    m_correction = config.params.value(QStringLiteral("positionCorrection")).toObject();
    if (!m_correction.contains(QStringLiteral("version")))
        m_correction.insert(QStringLiteral("version"), 1);
    const PositionRunPoseSource source =
            PositionCorrection::runPoseSourceFromConfig(m_correction);
    if (source.valid)
        PositionCorrection::writeRunPoseSource(source, &m_correction);
    updateRunPoseDisplay();
    const ReferenceFrameSnapshot snapshot =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    if (!snapshot.frame.empty()) {
        showFrameWithPoseInfo(snapshot.frame, tr("基准图"), source,
                              m_correction.value(QStringLiteral("referencePose")).toObject(),
                              QJsonObject());
    }
}

// 将当前 UI 状态写回 ToolConfig；旧版工具级模板 ROI 字段在保存时迁移移除。
ToolConfig PositionCorrectionDialog::toolConfig() const
{
    ToolConfig config = m_config;
    config.toolType = ToolType::PositionCorrection;
    config.category = ToolCategory::Location;
    config.toolName = tr("位置修正");
    config.displayName = tr("位置修正");
    config.summary = m_correction.value(QStringLiteral("referenceCreated")).toBool(false)
            ? tr("已创建基准，运行时计算位置修正")
            : tr("未创建基准");
    QJsonObject correction = m_correction;
    correction.remove(QStringLiteral("templateRegionType"));
    correction.remove(QStringLiteral("templateRoiNormalized"));
    correction.remove(QStringLiteral("templatePolygonNormalized"));
    config.params.insert(QStringLiteral("positionCorrection"), correction);
    return config;
}

// 工具级位置修正不拥有模板 ROI，预览由基准姿态与运行姿态信息直接驱动。
ToolPreviewSnapshot PositionCorrectionDialog::referencePreviewSnapshot() const
{
    return ToolPreviewSnapshot();
}

bool PositionCorrectionDialog::findToolProducer(const QString &producerId,
                                                ToolConfig *config) const
{
    for (const ToolConfig &candidate : m_producers) {
        if (candidate.toolId == producerId) {
            if (config)
                *config = candidate;
            return true;
        }
    }
    return false;
}

bool PositionCorrectionDialog::findReferenceProducer(
        const QString &producerId,
        PositionReferencePoseProducer *producer) const
{
    for (const PositionReferencePoseProducer &candidate : m_referencePoseProducers) {
        if (candidate.sourceId == producerId) {
            if (producer)
                *producer = candidate;
            return true;
        }
    }
    return false;
}

QVector<ToolOverlay> PositionCorrectionDialog::poseInfoOverlays(
        const QSize &imageSize,
        const PositionRunPoseSource &source,
        const QJsonObject &referencePose,
        const QJsonObject &runPose) const
{
    QVector<ToolOverlay> overlays;
    if (imageSize.isEmpty())
        return overlays;

    const qreal right = qMax(0, imageSize.width() - 12);
    auto appendText = [&overlays, right](qreal y, const QString &text) {
        ToolOverlay overlay;
        overlay.type = ToolOverlayType::Text;
        overlay.label = QStringLiteral("position_pose_info");
        overlay.text = text;
        overlay.p1 = QPointF(right, y);
        overlays.append(overlay);
    };
    const QString sourceText = source.displayText.trimmed().isEmpty()
            ? source.producerId : source.displayText.trimmed();
    appendText(16, tr("订阅来源：%1").arg(sourceText));

    auto poseText = [](const QString &title, const QJsonObject &pose) {
        if (pose.isEmpty())
            return QStringLiteral("%1：--").arg(title);
        const double angle = pose.contains(QStringLiteral("angleDeg"))
                ? pose.value(QStringLiteral("angleDeg")).toDouble()
                : pose.value(QStringLiteral("angle")).toDouble();
        return QStringLiteral("%1：X=%2  Y=%3  A=%4°  S=%5")
                .arg(title)
                .arg(pose.value(QStringLiteral("x")).toDouble(), 0, 'f', 3)
                .arg(pose.value(QStringLiteral("y")).toDouble(), 0, 'f', 3)
                .arg(angle, 0, 'f', 3)
                .arg(pose.value(QStringLiteral("scale")).toDouble(1.0), 0, 'f', 4);
    };
    appendText(44, poseText(tr("基准位姿"), referencePose));
    appendText(72, poseText(tr("运行位姿"), runPose));
    return overlays;
}

void PositionCorrectionDialog::showFrameWithPoseInfo(
        const cv::Mat &frame,
        const QString &title,
        const PositionRunPoseSource &source,
        const QJsonObject &referencePose,
        const QJsonObject &runPose,
        const QVector<ToolOverlay> &sourceOverlays)
{
    if (!m_previewHelper || frame.empty())
        return;
    const QImage image = MatImageConverter::matToDisplayImage(
                frame, QStringLiteral("PositionCorrectionDialog"));
    if (image.isNull())
        return;
    ui->viewerTitleLabel->setText(title);
    m_previewHelper->setImage(image);
    QVector<ToolOverlay> overlays = sourceOverlays;
    overlays += poseInfoOverlays(image.size(), source, referencePose, runPose);
    m_previewHelper->setToolOverlays(overlays);
}

void PositionCorrectionDialog::createReferencePose()
{
    const PositionRunPoseSource source =
            PositionCorrection::runPoseSourceFromConfig(m_correction);
    if (source.inconsistent) {
        ui->statusLabel->setText(tr("X/Y/角度必须来自同一个上游实例，请重新选择运行姿态来源"));
        return;
    }
    if (!source.valid) {
        ui->statusLabel->setText(tr("请先绑定运行点坐标X/Y/运行角度"));
        return;
    }

    const ReferenceFrameSetSnapshot referenceSet =
            ReferenceImageProvider::instance().referenceFrameSetSnapshot();
    const ReferenceFrameSnapshot &snapshot = referenceSet.primary;
    if (snapshot.frame.empty()) {
        ui->statusLabel->setText(tr("请先设置基准图"));
        return;
    }

    PositionReferencePoseProducer referenceProducer;
    if (findReferenceProducer(source.producerId, &referenceProducer)) {
        double x = 0.0;
        double y = 0.0;
        double angle = 0.0;
        double scale = 1.0;
        if (!finiteJsonNumber(referenceProducer.referencePose,
                              QStringLiteral("x"), &x)
                || !finiteJsonNumber(referenceProducer.referencePose,
                                     QStringLiteral("y"), &y)
                || (!finiteJsonNumber(referenceProducer.referencePose,
                                      QStringLiteral("angleDeg"), &angle)
                    && !finiteJsonNumber(referenceProducer.referencePose,
                                         QStringLiteral("angle"), &angle))) {
            ui->statusLabel->setText(tr("创建基准失败：基准图位姿字段无效"));
            return;
        }
        if (referenceProducer.referencePose.contains(QStringLiteral("scale"))
                && (!finiteJsonNumber(referenceProducer.referencePose,
                                      QStringLiteral("scale"), &scale)
                    || scale <= 0.0)) {
            ui->statusLabel->setText(tr("创建基准失败：基准图尺度必须大于 0"));
            return;
        }
        const QJsonObject importedPose = poseJson(x, y, angle, scale);
        m_correction.insert(QStringLiteral("referenceCreated"), true);
        m_correction.insert(QStringLiteral("referencePose"), importedPose);
        m_correction.insert(QStringLiteral("referencePoseSourceId"), source.producerId);
        m_correction.insert(QStringLiteral("referencePoseSource"), source.displayText);
        m_correction.insert(QStringLiteral("referencePoseStatus"),
                            QStringLiteral("imported"));
        showFrameWithPoseInfo(snapshot.frame, tr("基准图"), source,
                              importedPose, QJsonObject());
        ui->statusLabel->setText(tr("已导入基准图位姿：X=%1，Y=%2，角度=%3°，尺度=%4")
                                 .arg(x, 0, 'f', 3)
                                 .arg(y, 0, 'f', 3)
                                 .arg(angle, 0, 'f', 3)
                                 .arg(scale, 0, 'f', 4));
        return;
    }

    ToolConfig producerConfig;
    const bool foundProducer = findToolProducer(source.producerId, &producerConfig);
    if (!foundProducer || producerConfig.toolType != ToolType::TemplateLocation) {
        m_correction.insert(QStringLiteral("referenceCreated"), false);
        m_correction.remove(QStringLiteral("referencePose"));
        ui->statusLabel->setText(tr("运行姿态来源不可用，请选择前置模板定位工具"));
        return;
    }
    if (!producerConfig.enabled) {
        m_correction.insert(QStringLiteral("referenceCreated"), false);
        m_correction.remove(QStringLiteral("referencePose"));
        ui->statusLabel->setText(tr("运行姿态来源已禁用，不能创建基准"));
        return;
    }

    ToolRequest request;
    request.config = producerConfig;
    request.image = snapshot.frame;
    request.referenceImage = snapshot.frame;
    request.referenceImages = referenceSet.frames;
    request.referenceImageRevisions = referenceSet.contentRevisions;
    request.primaryReferenceBaseId = referenceSet.primaryBaseId;
    request.frameId = QStringLiteral("reference");
    request.imageFormat = QStringLiteral("reference");
    request.runtimeContext.insert(QStringLiteral("frameId"), QStringLiteral("reference"));
    const ToolResult result = m_templateLocationAdapter.run(request);
    if (!result.success || !result.ok) {
        m_correction.insert(QStringLiteral("referenceCreated"), false);
        m_correction.remove(QStringLiteral("referencePose"));
        ui->statusLabel->setText(tr("创建基准失败：%1").arg(
                                     result.message.trimmed().isEmpty()
                                     ? result.status
                                     : result.message));
        return;
    }

    double x = 0.0;
    double y = 0.0;
    double angle = 0.0;
    double scale = 1.0;
    if (!finiteJsonNumber(result.payload, source.xKey, &x)
            || !finiteJsonNumber(result.payload, source.yKey, &y)
            || !finiteJsonNumber(result.payload, source.angleKey, &angle)) {
        m_correction.insert(QStringLiteral("referenceCreated"), false);
        m_correction.remove(QStringLiteral("referencePose"));
        ui->statusLabel->setText(tr("创建基准失败：上游结果缺少绑定的 X/Y/角度字段"));
        return;
    }
    if (result.payload.contains(source.scaleKey)
            && (!finiteJsonNumber(result.payload, source.scaleKey, &scale)
                || scale <= 0.0)) {
        m_correction.insert(QStringLiteral("referenceCreated"), false);
        m_correction.remove(QStringLiteral("referencePose"));
        ui->statusLabel->setText(tr("创建基准失败：上游尺度字段必须大于 0"));
        return;
    }

    m_correction.insert(QStringLiteral("referenceCreated"), true);
    m_correction.insert(QStringLiteral("referencePose"), poseJson(x, y, angle, scale));
    m_correction.insert(QStringLiteral("referencePoseSourceId"), source.producerId);
    m_correction.insert(QStringLiteral("referencePoseSource"), source.displayText);
    m_correction.insert(QStringLiteral("referencePoseStatus"), result.status);
    m_correction.insert(QStringLiteral("referencePoseScore"), result.score);
    m_correction.insert(QStringLiteral("referencePoseElapsedMs"),
                        static_cast<double>(result.elapsedMs));
    showFrameWithPoseInfo(snapshot.frame, tr("基准图"), source,
                          m_correction.value(QStringLiteral("referencePose")).toObject(),
                          QJsonObject(), result.overlays);
    ui->statusLabel->setText(tr("基准创建成功：X=%1，Y=%2，角度=%3°，尺度=%4")
                             .arg(x, 0, 'f', 3)
                             .arg(y, 0, 'f', 3)
                             .arg(angle, 0, 'f', 3)
                             .arg(scale, 0, 'f', 4));
}

void PositionCorrectionDialog::runPositionCorrectionTest()
{
    const PositionRunPoseSource source =
            PositionCorrection::runPoseSourceFromConfig(m_correction);
    if (source.inconsistent) {
        ui->statusLabel->setText(tr("X/Y/角度必须来自同一个上游实例，请重新选择运行姿态来源"));
        return;
    }
    if (!source.valid) {
        ui->statusLabel->setText(tr("请先绑定运行点坐标X/Y/运行角度"));
        return;
    }
    if (!m_correction.value(QStringLiteral("referenceCreated")).toBool(false)
            || m_correction.value(QStringLiteral("referencePose")).toObject().isEmpty()) {
        ui->statusLabel->setText(tr("请先创建基准"));
        return;
    }

    const ReferenceFrameSetSnapshot referenceSet =
            ReferenceImageProvider::instance().referenceFrameSetSnapshot();
    const ReferenceFrameSnapshot &referenceSnapshot = referenceSet.primary;
    if (referenceSnapshot.frame.empty()) {
        ui->statusLabel->setText(tr("请先设置基准图"));
        return;
    }

    const CameraFrameSnapshot runSnapshot =
            CameraFrameProvider::instance().currentFrameSnapshot();
    if (runSnapshot.frame.empty()) {
        ui->statusLabel->setText(tr("当前运行图像为空；测试运行不会再用基准图代替运行图"));
        return;
    }

    ToolResult correctionResult;
    ToolResult producerResult;
    QJsonObject runPose;
    QVector<ToolOverlay> sourceOverlays;
    PositionReferencePoseProducer referenceProducer;
    if (findReferenceProducer(source.producerId, &referenceProducer)) {
        ToolEngine engine;
        engine.registerAdapter(&m_positionCorrectionAdapter);
        QJsonObject runtimeContext;
        runtimeContext.insert(QStringLiteral("frameId"),
                              QStringLiteral("position-correction-test"));
        runtimeContext.insert(
                    QStringLiteral("referencePositionCorrection"),
                    referenceProducer.runtimeConfig);
        const QVector<ToolResult> results = engine.runTools(
                    QVector<ToolConfig>{toolConfig()},
                    runSnapshot.frame,
                    referenceSnapshot.frame,
                    runtimeContext,
                    nullptr,
                    referenceSet.frames,
                    referenceSet.contentRevisions,
                    referenceSet.primaryBaseId);
        if (results.isEmpty()) {
            ui->statusLabel->setText(tr("测试运行失败：位置修正工具未执行"));
            return;
        }
        correctionResult = results.first();
        runPose = correctionResult.payload.value(QStringLiteral("runPose")).toObject();
    } else {
        ToolConfig producerConfig;
        const bool foundProducer = findToolProducer(source.producerId, &producerConfig);
        if (!foundProducer || producerConfig.toolType != ToolType::TemplateLocation) {
            ui->statusLabel->setText(tr("运行姿态来源不可用，请选择基准图或前置模板定位工具"));
            return;
        }

        ToolRequest producerRequest;
        producerRequest.config = producerConfig;
        producerRequest.image = runSnapshot.frame;
        producerRequest.referenceImage = referenceSnapshot.frame;
        producerRequest.referenceImages = referenceSet.frames;
        producerRequest.referenceImageRevisions =
                referenceSet.contentRevisions;
        producerRequest.primaryReferenceBaseId =
                referenceSet.primaryBaseId;
        producerRequest.frameId = QStringLiteral("position-correction-test");
        producerRequest.imageFormat = QStringLiteral("runtime");
        producerRequest.runtimeContext.insert(
                    QStringLiteral("frameId"),
                    QStringLiteral("position-correction-test"));
        producerResult = m_templateLocationAdapter.run(producerRequest);
        if (!producerResult.success || !producerResult.ok) {
            ui->statusLabel->setText(tr("测试运行失败：上游定位失败：%1").arg(
                                         producerResult.message.trimmed().isEmpty()
                                         ? producerResult.status
                                         : producerResult.message));
            return;
        }

        double x = 0.0;
        double y = 0.0;
        double angle = 0.0;
        double scale = 1.0;
        if (!finiteJsonNumber(producerResult.payload, source.xKey, &x)
                || !finiteJsonNumber(producerResult.payload, source.yKey, &y)
                || !finiteJsonNumber(producerResult.payload, source.angleKey, &angle)) {
            ui->statusLabel->setText(tr("测试运行失败：上游结果缺少绑定的 X/Y/角度字段"));
            return;
        }
        if (producerResult.payload.contains(source.scaleKey)
                && (!finiteJsonNumber(producerResult.payload, source.scaleKey, &scale)
                    || scale <= 0.0)) {
            ui->statusLabel->setText(tr("测试运行失败：上游尺度字段必须大于 0"));
            return;
        }
        runPose = poseJson(x, y, angle, scale);
        sourceOverlays = producerResult.overlays;
        producerResult.payload.insert(
                    QStringLiteral("frameId"),
                    QStringLiteral("position-correction-test"));

        QJsonObject toolResults;
        toolResults.insert(producerResult.toolId, producerResult.toJson());
        QJsonObject runtimeContext;
        runtimeContext.insert(QStringLiteral("frameId"),
                              QStringLiteral("position-correction-test"));
        runtimeContext.insert(QStringLiteral("toolResultsById"), toolResults);
        runtimeContext.insert(QStringLiteral("positionCorrectionsById"), QJsonObject());

        ToolRequest correctionRequest;
        correctionRequest.config = toolConfig();
        correctionRequest.image = runSnapshot.frame;
        correctionRequest.referenceImage = referenceSnapshot.frame;
        correctionRequest.frameId = QStringLiteral("position-correction-test");
        correctionRequest.runtimeContext = runtimeContext;
        correctionResult = m_positionCorrectionAdapter.run(correctionRequest);
    }
    if (!correctionResult.success || !correctionResult.ok) {
        ui->statusLabel->setText(tr("测试运行失败：%1").arg(
                                     correctionResult.message.trimmed().isEmpty()
                                     ? correctionResult.status
                                     : correctionResult.message));
        return;
    }

    if (runPose.isEmpty())
        runPose = correctionResult.payload.value(QStringLiteral("runPose")).toObject();
    showFrameWithPoseInfo(
                runSnapshot.frame,
                tr("运行图"),
                source,
                m_correction.value(QStringLiteral("referencePose")).toObject(),
                runPose,
                sourceOverlays);

    ui->statusLabel->setText(tr("测试运行成功：运行X=%1，运行Y=%2，运行角度=%3°，尺度比=%4；ΔX=%5，ΔY=%6，Δ角度=%7°")
                             .arg(runPose.value(QStringLiteral("x")).toDouble(), 0, 'f', 3)
                             .arg(runPose.value(QStringLiteral("y")).toDouble(), 0, 'f', 3)
                             .arg(runPose.value(QStringLiteral("angleDeg")).toDouble(), 0, 'f', 3)
                             .arg(correctionResult.payload.value(QStringLiteral("scaleRatio")).toDouble(1.0), 0, 'f', 4)
                             .arg(correctionResult.payload.value(QStringLiteral("deltaX")).toDouble(), 0, 'f', 3)
                             .arg(correctionResult.payload.value(QStringLiteral("deltaY")).toDouble(), 0, 'f', 3)
                             .arg(correctionResult.payload.value(QStringLiteral("deltaAngleDeg")).toDouble(), 0, 'f', 3));
}

// 确保运行点 X、Y 和角度均包含有效的来源 ID 与输出字段键。
bool PositionCorrectionDialog::validateForFinish()
{
    const PositionRunPoseSource source =
            PositionCorrection::runPoseSourceFromConfig(m_correction);
    if (source.inconsistent) {
        ui->statusLabel->setText(tr("X/Y/角度必须来自同一个上游实例，请重新选择运行姿态来源"));
        return false;
    }
    if (!source.valid) {
        ui->statusLabel->setText(tr("请绑定运行点坐标X/Y/运行角度"));
        return false;
    }
    bool available = false;
    for (const PositionPoseProducer &producer : m_poseProducers) {
        if (producer.producerId == source.producerId) {
            available = true;
            break;
        }
    }
    if (!available) {
        ui->statusLabel->setText(tr("运行姿态来源不可用，请选择前置模板定位工具"));
        return false;
    }
    return true;
}
