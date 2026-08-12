#include "ToolsDialog.h"

#include <algorithm>

#include <QDebug>
#include <QColor>
#include <QComboBox>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPoint>
#include <QPushButton>
#include <QSet>
#include <QSize>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVariant>
#include <QVBoxLayout>

#include <opencv2/imgproc.hpp>

#include "CharacterRecognitionDialog.h"
#include "BlobPresenceDialog.h"
#include "CameraParamsDialog.h"
#include "CalibrationTransformDialog.h"
#include "ColorComparisonDialog.h"
#include "ColorRecognitionDialog.h"
#include "ClassificationDialog.h"
#include "CirclePresenceDialog.h"
#include "ContourPresenceDialog.h"
#include "EdgePresenceDialog.h"
#include "LinePresenceDialog.h"
#include "MainWindow.h"
#include "ObjectDetectionDialog.h"
#include "OutputDialog.h"
#include "PatternPresenceDialog.h"
#include "PositionCorrectionDialog.h"
#include "TemplateLocationDialog.h"
#include "PlanDialogUtils.h"
#include "ReferenceImageDialog.h"
#include "QuickCalibrationWizard.h"
#include "RegisteredClassificationDialog.h"
#include "RegisteredClassificationDetectionDialog.h"
#include "SchemeStore.h"
#include "ToolLibraryDialog.h"
#include "calibration/CalibrationFileLoader.h"
#include "calibration/CalibrationSourceFingerprint.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "ui_ToolsDialog.h"

namespace {

constexpr int kToolCardHeight = 96;
constexpr int kToolListMargin = 8;
constexpr int kToolListSpacing = 10;

void refreshWidgetStyle(QWidget *widget)
{
    if (!widget)
        return;

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

QImage imageFromFrame(const cv::Mat &frame)
{
    return MatImageConverter::matToDisplayImage(frame, QStringLiteral("ToolsDialog"));
}

const ToolConfig *toolById(const QVector<ToolConfig> &tools,
                           const QString &toolId)
{
    for (const ToolConfig &tool : tools) {
        if (tool.toolId == toolId)
            return &tool;
    }
    return nullptr;
}

bool validateGeneratedCalibrationTarget(
        const ToolConfig &target,
        const QVector<ToolConfig> &tools,
        const QMap<QString, ToolPreviewSnapshot> &snapshots,
        const QJsonObject &expectedFingerprint,
        bool requiresImageAngle,
        QString *errorMessage)
{
    if (expectedFingerprint.isEmpty()
            || expectedFingerprint.value(QStringLiteral("mode")).toString()
               == QStringLiteral("manual")) {
        return true;
    }
    QString invalidField;
    if (!CalibrationSourceFingerprint::isComplete(expectedFingerprint,
                                                   &invalidField)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("生成的标定文件来源指纹无效：%1")
                    .arg(invalidField);
        }
        return false;
    }

    const QJsonObject transform = target.params.value(
                QStringLiteral("calibrationTransform")).toObject();
    const QJsonObject xBinding = transform.value(QStringLiteral("inputX")).toObject();
    const QJsonObject yBinding = transform.value(QStringLiteral("inputY")).toObject();
    const QJsonObject angleBinding = transform.value(
                QStringLiteral("inputAngle")).toObject();
    const QString inputProducerId = xBinding.value(
                QStringLiteral("producerId")).toString().trimmed();
    const bool angleBindingValid = angleBinding.value(QStringLiteral("mode")).toString()
            == QStringLiteral("binding")
            && angleBinding.value(QStringLiteral("producerId")).toString()
               == inputProducerId;
    if (xBinding.value(QStringLiteral("mode")).toString()
            != QStringLiteral("binding")
            || yBinding.value(QStringLiteral("mode")).toString()
               != QStringLiteral("binding")
            || inputProducerId.isEmpty()
            || yBinding.value(QStringLiteral("producerId")).toString()
               != inputProducerId
            || (requiresImageAngle && !angleBindingValid)) {
        if (errorMessage) {
            *errorMessage = requiresImageAngle
                    ? QObject::tr("目标 %1 未绑定唯一完整且同源的 X/Y/Angle")
                      .arg(target.displayName)
                    : QObject::tr("目标 %1 未绑定唯一完整且同源的 X/Y")
                      .arg(target.displayName);
        }
        return false;
    }

    const ToolConfig *inputProducer = toolById(tools, inputProducerId);
    if (!inputProducer || !inputProducer->enabled) {
        if (errorMessage)
            *errorMessage = QObject::tr("目标 %1 的坐标来源不存在或已禁用")
                    .arg(target.displayName);
        return false;
    }

    const QJsonObject outputContract = expectedFingerprint.value(
                QStringLiteral("outputContract")).toObject();
    const QString expectedX = outputContract.value(QStringLiteral("x"))
            .toString(QStringLiteral("x"));
    const QString expectedY = outputContract.value(QStringLiteral("y"))
            .toString(QStringLiteral("y"));
    const QString expectedAngle = outputContract.value(QStringLiteral("angle"))
            .toString(QStringLiteral("angle"));
    const ToolConfig *effectiveProducer = inputProducer;
    if (inputProducer->toolType == ToolType::TemplateLocation) {
        if (xBinding.value(QStringLiteral("outputKey")).toString() != expectedX
                || yBinding.value(QStringLiteral("outputKey")).toString() != expectedY
                || (requiresImageAngle
                    && angleBinding.value(QStringLiteral("outputKey")).toString()
                       != expectedAngle)) {
            if (errorMessage)
                *errorMessage = QObject::tr("目标 %1 的输出字段与标定采样字段不一致")
                        .arg(target.displayName);
            return false;
        }
    } else if (inputProducer->toolType == ToolType::PositionCorrection) {
        if (xBinding.value(QStringLiteral("outputKey")).toString()
                != QStringLiteral("runPose.x")
                || yBinding.value(QStringLiteral("outputKey")).toString()
                   != QStringLiteral("runPose.y")
                || (requiresImageAngle
                    && angleBinding.value(QStringLiteral("outputKey")).toString()
                       != QStringLiteral("runPose.angleDeg"))) {
            if (errorMessage) {
                *errorMessage = QObject::tr("目标 %1 的位置修正输出字段不完整或顺序错误")
                        .arg(target.displayName);
            }
            return false;
        }
        const PositionRunPoseSource source = PositionCorrection::runPoseSourceFromConfig(
                    inputProducer->params.value(
                        QStringLiteral("positionCorrection")).toObject());
        effectiveProducer = toolById(tools, source.producerId);
        if (!source.valid || source.inconsistent || !effectiveProducer
                || effectiveProducer->toolType != ToolType::TemplateLocation
                || source.xKey != expectedX || source.yKey != expectedY
                || (requiresImageAngle && source.angleKey != expectedAngle)) {
            if (errorMessage) {
                *errorMessage = QObject::tr("目标 %1 的位置修正无法追溯到标定时模板来源")
                        .arg(target.displayName);
            }
            return false;
        }
    } else {
        if (errorMessage)
            *errorMessage = QObject::tr("目标 %1 的输入不是模板定位或位置修正")
                    .arg(target.displayName);
        return false;
    }

    QJsonObject payload;
    const QJsonObject params = effectiveProducer->params;
    payload.insert(QStringLiteral("originMode"),
                   params.value(QStringLiteral("originMode"))
                   .toString(QStringLiteral("centroid")));
    payload.insert(QStringLiteral("customOriginNormalized"),
                   params.value(QStringLiteral("customOriginNormalized"))
                   .toObject());
    const ToolPreviewSnapshot sourceSnapshot = snapshots.value(
                effectiveProducer->toolId);
    QJsonObject snapshotPayload = sourceSnapshot.result.payload;
    if (snapshotPayload.isEmpty())
        snapshotPayload = QJsonObject::fromVariantMap(sourceSnapshot.payload);
    const QString currentConfigSignature =
            CalibrationSourceFingerprint::coordinateSourceConfigSignature(
                *effectiveProducer);
    const QString currentReferenceSignature =
            CalibrationSourceFingerprint::imageSignature(
                ReferenceImageProvider::instance().referenceFrame());
    const bool snapshotFresh = sourceSnapshot.valid
            && snapshotPayload.value(QStringLiteral(
                                         "coordinateSourceConfigSignature"))
               .toString() == currentConfigSignature
            && !currentReferenceSignature.isEmpty()
            && snapshotPayload.value(QStringLiteral(
                                         "coordinateSourceReferenceSignature"))
               .toString() == currentReferenceSignature;
    QString modelSignature = snapshotFresh
            ? snapshotPayload.value(QStringLiteral("modelSignature"))
              .toString().trimmed()
            : QString();
    if (modelSignature.isEmpty()) {
        modelSignature = expectedFingerprint.value(
                    QStringLiteral("modelSignature")).toString();
    }
    payload.insert(QStringLiteral("modelSignature"), modelSignature);
    payload.insert(QStringLiteral("coordinateSourceConfigSignature"),
                   currentConfigSignature);
    payload.insert(QStringLiteral("coordinateSourceReferenceSignature"),
                   currentReferenceSignature);
    const QJsonObject actualFingerprint = CalibrationSourceFingerprint::makeFingerprint(
                effectiveProducer->toolId,
                effectiveProducer->toolType,
                payload,
                outputContract);
    QString mismatchField;
    if (!CalibrationSourceFingerprint::matches(expectedFingerprint,
                                               actualFingerprint,
                                               &mismatchField)) {
        if (errorMessage) {
            *errorMessage = QObject::tr("目标 %1 与标定来源不兼容（%2）")
                    .arg(target.displayName, mismatchField);
        }
        return false;
    }
    return true;
}

//当前实时图
QImage currentReferenceImage()
{
    return imageFromFrame(ReferenceImageProvider::instance().referenceFrame());
}

QString findParamString(const QJsonObject &object, const QString &key)
{
    const QString direct = object.value(key).toString().trimmed();
    if (!direct.isEmpty())
        return direct;
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (it.value().isObject()) {
            const QString nested = findParamString(it.value().toObject(), key);
            if (!nested.isEmpty())
                return nested;
        }
    }
    return QString();
}

QJsonObject writeSourceId(QJsonObject object,
                          const QString &sourceId,
                          const QString &sourceText)
{
    if (object.contains(QStringLiteral("positionCorrectionSource"))) {
        object.insert(QStringLiteral("positionCorrectionSourceId"), sourceId);
        object.insert(QStringLiteral("positionCorrectionSource"), sourceText);
    }
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (it.value().isObject())
            it.value() = writeSourceId(it.value().toObject(), sourceId, sourceText);
    }
    return object;
}

bool containsToolReference(const QJsonValue &value, const QString &toolId)
{
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue &entry : array) {
            if (containsToolReference(entry, toolId))
                return true;
        }
        return false;
    }

    if (!value.isObject())
        return false;

    const QJsonObject object = value.toObject();
    static const QStringList referenceKeys = {
        QStringLiteral("positionCorrectionSourceId"),
        QStringLiteral("producerId"),
        QStringLiteral("referencePoseSourceId")
    };
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (referenceKeys.contains(it.key())
                && it.value().toString().trimmed() == toolId) {
            return true;
        }
        if ((it.value().isObject() || it.value().isArray())
                && containsToolReference(it.value(), toolId)) {
            return true;
        }
    }
    return false;
}

void configurePositionSourceCombos(QWidget *dialog,
                                   const QVector<PositionCorrectionSource> &sources,
                                   const ToolConfig *initialConfig)
{
    const QString selectedId = initialConfig
            ? findParamString(initialConfig->params, QStringLiteral("positionCorrectionSourceId"))
            : PositionCorrection::defaultSourceId();
    const QString selectedText = initialConfig
            ? findParamString(initialConfig->params, QStringLiteral("positionCorrectionSource"))
            : PositionCorrection::defaultSource();
    for (QComboBox *combo : dialog->findChildren<QComboBox *>()) {
        if (!combo->objectName().toLower().contains(QStringLiteral("positioncorrection")))
            continue;
        combo->clear();
        int selectedIndex = -1;
        for (const PositionCorrectionSource &source : sources) {
            combo->addItem(source.displayText, source.sourceId);
            if (source.sourceId == selectedId)
                selectedIndex = combo->count() - 1;
        }
        if (selectedIndex < 0 && !selectedId.isEmpty()) {
            combo->insertItem(0,
                              QObject::tr("来源不可用：%1").arg(
                                  selectedText.isEmpty() ? selectedId : selectedText),
                              selectedId);
            combo->setItemData(0, QColor(QStringLiteral("#dc2626")), Qt::ForegroundRole);
            selectedIndex = 0;
        }
        if (selectedIndex >= 0)
            combo->setCurrentIndex(selectedIndex);
    }
}

void persistPositionSource(QWidget *dialog, ToolConfig *config)
{
    for (QComboBox *combo : dialog->findChildren<QComboBox *>()) {
        if (!combo->objectName().toLower().contains(QStringLiteral("positioncorrection")))
            continue;
        const QString sourceId = combo->currentData().toString();
        if (sourceId.isEmpty())
            continue;
        config->params.insert(QStringLiteral("positionCorrectionSourceId"), sourceId);
        config->params = writeSourceId(config->params, sourceId, combo->currentText());
        return;
    }
}

QString toolIconForType(ToolType type)
{
    switch (type) {
    case ToolType::ColorRecognition:
    case ToolType::RegisteredClassification:
    case ToolType::RegisteredClassificationDetection:
    case ToolType::ColorComparison:
        return QStringLiteral(":/icons/compare.svg");
    case ToolType::Ocr:
        return QStringLiteral(":/icons/tool.svg");
    case ToolType::AiDetection:
    case ToolType::AiClassification:
        return QStringLiteral(":/icons/monitor.svg");
    case ToolType::PatternPresence:
    case ToolType::BlobPresence:
    case ToolType::CirclePresence:
    case ToolType::EdgePresence:
    case ToolType::LinePresence:
    case ToolType::ContourPresence:
        return QStringLiteral(":/icons/eye.svg");
    case ToolType::PositionCorrection:
    case ToolType::CalibrationTransform:
        return QStringLiteral(":/icons/fit.svg");
    default:
        return QStringLiteral(":/icons/tool.svg");
    }
}

template <typename Dialog>
void configureProducerContext(Dialog *, ToolsDialog *, const ToolConfig *)
{
}

void configureProducerContext(PositionCorrectionDialog *dialog,
                              ToolsDialog *toolsDialog,
                              const ToolConfig *initialConfig)
{
    int index = toolsDialog->toolConfigs().size();
    if (initialConfig) {
        for (int i = 0; i < toolsDialog->toolConfigs().size(); ++i) {
            if (toolsDialog->toolConfigs().at(i).toolId == initialConfig->toolId) {
                index = i;
                break;
            }
        }
    }
    QVector<PositionReferencePoseProducer> referenceProducers;
    const ReferencePositionCorrectionConfig &reference =
            SchemeStore::instance().currentScheme().referencePositionCorrection;
    if (reference.enabled && reference.referenceCreated
            && !reference.referencePose.isEmpty()) {
        referenceProducers.append(PositionReferencePoseProducer{
                                      PositionCorrection::defaultSourceId(),
                                      QStringLiteral("0 基准图"),
                                      reference.referencePose,
                                      PositionCorrection::referenceToJson(reference)});
    }
    dialog->setAvailableProducers(toolsDialog->toolConfigs(), index,
                                  referenceProducers);
}

void configureProducerContext(CalibrationTransformDialog *dialog,
                              ToolsDialog *toolsDialog,
                              const ToolConfig *initialConfig)
{
    int index = toolsDialog->toolConfigs().size();
    if (initialConfig) {
        for (int i = 0; i < toolsDialog->toolConfigs().size(); ++i) {
            if (toolsDialog->toolConfigs().at(i).toolId == initialConfig->toolId) {
                index = i;
                break;
            }
        }
    }
    dialog->setProducerTools(toolsDialog->toolConfigs(), index,
                             toolsDialog->referencePreviewSnapshots());
    dialog->setToolChainTestContext(
                toolsDialog->toolConfigs(),
                index,
                toolsDialog->toolEngineForTesting(),
                SchemeStore::instance().currentScheme()
                .referencePositionCorrection);
}

void configureProducerContext(BlobPresenceDialog *dialog,
                              ToolsDialog *toolsDialog,
                              const ToolConfig *initialConfig)
{
    int index = toolsDialog->toolConfigs().size();
    if (initialConfig) {
        for (int i = 0; i < toolsDialog->toolConfigs().size(); ++i) {
            if (toolsDialog->toolConfigs().at(i).toolId
                    == initialConfig->toolId) {
                index = i;
                break;
            }
        }
    }
    dialog->setToolChainTestContext(
                toolsDialog->toolConfigs(),
                index,
                toolsDialog->toolEngineForTesting(),
                SchemeStore::instance().currentScheme()
                .referencePositionCorrection);
}

void configureProducerContext(CirclePresenceDialog *dialog,
                              ToolsDialog *toolsDialog,
                              const ToolConfig *initialConfig)
{
    int index = toolsDialog->toolConfigs().size();

    if (initialConfig) {
        for (int i = 0; i < toolsDialog->toolConfigs().size(); ++i) {
            if (toolsDialog->toolConfigs().at(i).toolId
                    == initialConfig->toolId) {
                index = i;
                break;
            }
        }
    }

    dialog->setToolChainTestContext(
                toolsDialog->toolConfigs(),
                index,
                toolsDialog->toolEngineForTesting(),
                SchemeStore::instance().currentScheme()
                .referencePositionCorrection);
}

template <typename Dialog>
void configurePositionConsumerContext(Dialog *dialog,
                                      ToolsDialog *toolsDialog,
                                      const ToolConfig *initialConfig)
{
    int index = toolsDialog->toolConfigs().size();
    if (initialConfig) {
        for (int i = 0; i < toolsDialog->toolConfigs().size(); ++i) {
            if (toolsDialog->toolConfigs().at(i).toolId
                    == initialConfig->toolId) {
                index = i;
                break;
            }
        }
    }
    dialog->setToolChainTestContext(
                toolsDialog->toolConfigs(),
                index,
                SchemeStore::instance().currentScheme()
                .referencePositionCorrection);
}

void configureProducerContext(PatternPresenceDialog *dialog,
                              ToolsDialog *toolsDialog,
                              const ToolConfig *initialConfig)
{
    configurePositionConsumerContext(dialog, toolsDialog, initialConfig);
}

void configureProducerContext(EdgePresenceDialog *dialog,
                              ToolsDialog *toolsDialog,
                              const ToolConfig *initialConfig)
{
    configurePositionConsumerContext(dialog, toolsDialog, initialConfig);
}

void configureProducerContext(LinePresenceDialog *dialog,
                              ToolsDialog *toolsDialog,
                              const ToolConfig *initialConfig)
{
    configurePositionConsumerContext(dialog, toolsDialog, initialConfig);
}

void configureProducerContext(ContourPresenceDialog *dialog,
                              ToolsDialog *toolsDialog,
                              const ToolConfig *initialConfig)
{
    configurePositionConsumerContext(dialog, toolsDialog, initialConfig);
}

void configureProducerContext(CharacterRecognitionDialog *dialog,
                              ToolsDialog *toolsDialog,
                              const ToolConfig *initialConfig)
{
    configurePositionConsumerContext(dialog, toolsDialog, initialConfig);
}

void configureProducerContext(RegisteredClassificationDialog *dialog,
                              ToolsDialog *toolsDialog,
                              const ToolConfig *initialConfig)
{
    configurePositionConsumerContext(dialog, toolsDialog, initialConfig);
}

template <typename Dialog>
bool runToolConfigDialog(QWidget *parent,
                         const ToolConfig *initialConfig,
                         ToolConfig *toolConfig,
                         ToolPreviewSnapshot *snapshot)
{
    Dialog configDialog(parent);
    PlanDialogUtils::applyToolLevelStyle(&configDialog);
    configDialog.setWindowModality(Qt::WindowModal);
    configDialog.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(&configDialog);

    if (initialConfig)
        configDialog.loadFromConfig(*initialConfig);

    if (ToolsDialog *toolsDialog = qobject_cast<ToolsDialog *>(parent)) {
        configureProducerContext(&configDialog, toolsDialog, initialConfig);
        configurePositionSourceCombos(&configDialog,
                                      toolsDialog->positionCorrectionSourcesFor(initialConfig),
                                      initialConfig);
    }

    QTimer::singleShot(0, &configDialog, [&configDialog]() {
        configDialog.raise();
        configDialog.activateWindow();
    });
    configDialog.raise();
    configDialog.activateWindow();

    if (configDialog.exec() != QDialog::Accepted)
        return false;

    if (toolConfig) {
        *toolConfig = configDialog.toolConfig();
        persistPositionSource(&configDialog, toolConfig);
    }
    if (snapshot)
        *snapshot = configDialog.referencePreviewSnapshot();
    return true;
}

} // namespace

ToolsDialog::ToolsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ToolsDialog)
    , m_toolSerial(0)
{
    ui->setupUi(this);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);
    setupUiState();
    connectNavigation();
    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) {
        m_toolPreviewSnapshots.clear();
        refreshReferencePreview();
    });
    QString error;
    if (SchemeStore::instance().ensureLoaded(&error)) {
        const SchemeState &scheme = SchemeStore::instance().currentScheme();
        setInitialToolState(scheme.toolConfigs, scheme.referencePreviewSnapshots);
    } else {
        qWarning() << "[ToolsDialog] 方案加载失败:" << error;
    }
    refreshSchemeHeader();
    refreshReferencePreview();
}

ToolsDialog::~ToolsDialog()
{
    delete ui;
}

void ToolsDialog::prepareForDisplay()
{
    QString error;
    if (SchemeStore::instance().ensureLoaded(&error)) {
        const SchemeState &scheme = SchemeStore::instance().currentScheme();
        setInitialToolState(scheme.toolConfigs, scheme.referencePreviewSnapshots);
    } else {
        qWarning() << "[ToolsDialog] 页面刷新失败:" << error;
    }
    refreshSchemeHeader();
    refreshReferencePreview();
}

const QVector<ToolConfig> &ToolsDialog::toolConfigs() const
{
    return m_toolConfigs;
}

const QMap<QString, ToolPreviewSnapshot> &ToolsDialog::referencePreviewSnapshots() const
{
    return m_toolPreviewSnapshots;
}

void ToolsDialog::setInitialToolState(const QVector<ToolConfig> &configs,
                                      const QMap<QString, ToolPreviewSnapshot> &snapshots)
{
    m_toolConfigs = configs;
    m_toolPreviewSnapshots.clear();

    for (const ToolConfig &config : m_toolConfigs) {
        const ToolPreviewSnapshot snapshot = snapshots.value(config.toolId);
        if (!snapshot.valid)
            continue;

        ToolPreviewSnapshot normalized = snapshot;
        normalized.toolId = config.toolId;
        normalized.toolType = config.toolType;
        m_toolPreviewSnapshots.insert(config.toolId, normalized);
    }

    m_selectedToolIndex = m_toolConfigs.isEmpty() ? -1 : 0;
    refreshSchemeHeader();
    refreshToolList();
    refreshReferencePreview();
}

bool ToolsDialog::openedOutputDialog() const
{
    return m_openedOutputDialog;
}

void ToolsDialog::setToolEngineForTesting(ToolEngine *engine)
{
    m_toolEngineForTesting = engine;
}

ToolEngine *ToolsDialog::toolEngineForTesting() const
{
    return m_toolEngineForTesting;
}

QVector<PositionCorrectionSource> ToolsDialog::positionCorrectionSourcesFor(
        const ToolConfig *consumer) const
{
    int consumerIndex = m_toolConfigs.size();
    if (consumer && !consumer->toolId.trimmed().isEmpty()) {
        for (int index = 0; index < m_toolConfigs.size(); ++index) {
            if (m_toolConfigs.at(index).toolId == consumer->toolId) {
                consumerIndex = index;
                break;
            }
        }
    }
    return PositionCorrection::sourcesBefore(
                m_toolConfigs,
                consumerIndex,
                SchemeStore::instance().currentScheme().referencePositionCorrection.enabled);
}

bool ToolsDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        QFrame *card = qobject_cast<QFrame *>(watched);
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (card && mouseEvent->button() == Qt::LeftButton) {
            const QVariant indexValue = card->property("toolIndex");
            if (indexValue.isValid()) {
                selectTool(indexValue.toInt());
                return true;
            }
        }
    }

    return QDialog::eventFilter(watched, event);
}

void ToolsDialog::setupUiState()
{
    PlanDialogUtils::configureDialogWindow(this, tr("方案编辑 - 工具"));
    if (qobject_cast<MainWindow *>(parentWidget())) {
        connect(ui->headerCloseButton, &QToolButton::clicked, this, &ToolsDialog::accept);
    } else {
        PlanDialogUtils::connectWindowButtons(this, ui->headerCloseButton, true);
    }


    ui->cameraStepButton->setChecked(false);
    ui->referenceStepButton->setChecked(false);
    ui->toolsStepButton->setChecked(true);
    ui->outputStepButton->setChecked(false);
    ui->setupQuickCalibrateButton->setEnabled(true);
    ui->setupQuickCalibrateButton->setProperty(
                "quickCalibrationState", QStringLiteral("available"));

    ui->verticalLayout_toolsList->setSpacing(kToolListSpacing);
    ui->verticalLayout_toolsList->setContentsMargins(kToolListMargin,
                                                     kToolListMargin,
                                                     kToolListMargin,
                                                     kToolListMargin);
    ui->scrollAreaWidgetContents->setSizePolicy(QSizePolicy::Preferred,
                                                QSizePolicy::MinimumExpanding);
    refreshSchemeHeader();
    refreshToolList();
}

void ToolsDialog::connectNavigation()
{
    connect(ui->addToolButton, &QToolButton::clicked, this, &ToolsDialog::openToolLibrary);
    connect(ui->copyToolButton, &QToolButton::clicked, this, &ToolsDialog::copySelectedTool);
    connect(ui->deleteToolButton, &QToolButton::clicked, this, &ToolsDialog::deleteSelectedTool);
    connect(ui->deleteAllToolsButton, &QToolButton::clicked, this, &ToolsDialog::deleteAllTools);
    connect(ui->cameraStepButton, &QToolButton::clicked, this, &ToolsDialog::openCameraParamsDialog);
    connect(ui->referenceStepButton, &QToolButton::clicked, this, &ToolsDialog::openReferenceImageDialog);
    connect(ui->outputStepButton, &QToolButton::clicked, this, &ToolsDialog::openOutputDialog);
    connect(ui->previousButton, &QPushButton::clicked, this, &ToolsDialog::openReferenceImageDialog);
    connect(ui->nextButton, &QPushButton::clicked, this, &ToolsDialog::openOutputDialog);
    connect(ui->setupExternalEditButton, &QToolButton::clicked, this, &ToolsDialog::editCurrentSchemeName);
    connect(ui->setupSaveButton, &QToolButton::clicked, this, &ToolsDialog::saveCurrentScheme);
    connect(ui->setupSaveAsButton, &QToolButton::clicked, this, &ToolsDialog::saveCurrentSchemeAs);
    connect(ui->setupQuickCalibrateButton, &QToolButton::clicked,
            this, &ToolsDialog::openQuickCalibration);
}

void ToolsDialog::openQuickCalibration()
{
    if (!commitToolStateToScheme(true))
        return;
    SchemeStore &store = SchemeStore::instance();
    const QJsonObject previousQuickConfiguration =
            store.currentScheme().quickCalibrationConfig;
    QuickCalibrationWizard wizard(this);
    wizard.setProducerTools(m_toolConfigs, m_toolPreviewSnapshots,
                            m_selectedToolIndex);
    wizard.setPersistentConfiguration(previousQuickConfiguration);
    wizard.setPreviewImage(currentReferenceImage());
    PlanDialogUtils::fitDialogToScreen(&wizard, this, 24);
    PlanDialogUtils::centerWindowOnScreen(&wizard, this, 24);
    wizard.exec();

    store.setQuickCalibrationConfig(wizard.persistentConfiguration());
    const QString generatedPath = wizard.generatedFilePath();
    const QStringList targetIds = wizard.targetCalibrationTransformIds();
    QString saveError;
    if (!generatedPath.isEmpty() && !targetIds.isEmpty()) {
        QStringList appliedNames;
        if (!applyGeneratedCalibrationToTransforms(
                    generatedPath, targetIds, &appliedNames, &saveError)) {
            store.setQuickCalibrationConfig(previousQuickConfiguration);
            QMessageBox::warning(
                        this, tr("标定文件未应用"),
                        tr("XML 已生成，但目标标定转换更新失败：%1\n%2")
                        .arg(saveError, generatedPath));
            return;
        }
        QMessageBox::information(
                    this, tr("快速标定"),
                    tr("标定文件已生成并应用到：%1\n%2")
                    .arg(appliedNames.join(QStringLiteral("、")), generatedPath));
        return;
    }

    if (!store.saveCurrentScheme(&saveError)) {
        store.setQuickCalibrationConfig(previousQuickConfiguration);
        QMessageBox::warning(this, tr("快速标定配置保存失败"), saveError);
        return;
    }
    if (!generatedPath.isEmpty()) {
        QMessageBox::information(
                    this, tr("快速标定"),
                    tr("标定文件已生成；当前未选择目标标定转换，运行配置未改变：\n%1")
                    .arg(generatedPath));
    }
}

bool ToolsDialog::applyGeneratedCalibrationToTransforms(
        const QString &filePath,
        const QStringList &targetToolIds,
        QStringList *appliedToolNames,
        QString *errorMessage)
{
    if (appliedToolNames)
        appliedToolNames->clear();
    if (errorMessage)
        errorMessage->clear();
    const QString normalizedPath = QFileInfo(filePath).absoluteFilePath();
    const QFileInfo fileInfo(normalizedPath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        if (errorMessage)
            *errorMessage = tr("标定文件不存在：%1").arg(normalizedPath);
        return false;
    }

    QSet<QString> requestedIds;
    for (const QString &id : targetToolIds) {
        const QString normalizedId = id.trimmed();
        if (!normalizedId.isEmpty())
            requestedIds.insert(normalizedId);
    }
    if (requestedIds.isEmpty())
        return true;

    QMap<QString, int> targetIndexes;
    for (int index = 0; index < m_toolConfigs.size(); ++index) {
        const ToolConfig &config = m_toolConfigs.at(index);
        if (requestedIds.contains(config.toolId))
            targetIndexes.insert(config.toolId, index);
    }
    if (targetIndexes.size() != requestedIds.size()) {
        if (errorMessage)
            *errorMessage = tr("部分目标工具不存在或已被删除");
        return false;
    }
    for (auto it = targetIndexes.constBegin(); it != targetIndexes.constEnd(); ++it) {
        if (m_toolConfigs.at(it.value()).toolType != ToolType::CalibrationTransform) {
            if (errorMessage)
                *errorMessage = tr("目标 %1 不是标定转换工具").arg(it.key());
            return false;
        }
    }

    ProjectXmlCalibrationLoader projectLoader;
    if (projectLoader.canLoad(normalizedPath)) {
        CalibrationModel generatedModel;
        QString loadError;
        if (!projectLoader.load(normalizedPath, &generatedModel, &loadError)) {
            if (errorMessage) {
                *errorMessage = loadError.isEmpty()
                        ? tr("生成的标定文件无法读取") : loadError;
            }
            return false;
        }
        const QJsonObject expectedFingerprint = generatedModel.imageBinding.value(
                    QStringLiteral("coordinateSourceFingerprint")).toObject();
        const bool requiresImageAngle = generatedModel.mode
                == NPointCalibrationMode::TwelvePointPoseMapping;
        for (auto it = targetIndexes.constBegin();
             it != targetIndexes.constEnd(); ++it) {
            QString compatibilityError;
            if (!validateGeneratedCalibrationTarget(
                        m_toolConfigs.at(it.value()),
                        m_toolConfigs,
                        m_toolPreviewSnapshots,
                        expectedFingerprint,
                        requiresImageAngle,
                        &compatibilityError)) {
                if (errorMessage)
                    *errorMessage = compatibilityError;
                return false;
            }
        }
    }

    const QVector<ToolConfig> configsBefore = m_toolConfigs;
    const QMap<QString, ToolPreviewSnapshot> snapshotsBefore =
            m_toolPreviewSnapshots;
    const int selectedIndexBefore = m_selectedToolIndex;
    QStringList names;
    for (auto it = targetIndexes.constBegin(); it != targetIndexes.constEnd(); ++it) {
        ToolConfig &config = m_toolConfigs[it.value()];
        QJsonObject transform = config.params
                .value(QStringLiteral("calibrationTransform")).toObject();
        QJsonArray files = transform.value(QStringLiteral("calibrationFiles")).toArray();
        if (!files.contains(normalizedPath))
            files.append(normalizedPath);
        transform.insert(QStringLiteral("version"), 4);
        transform.remove(QStringLiteral("rotationAxisAngle"));
        transform.insert(QStringLiteral("calibrationFiles"), files);
        transform.insert(QStringLiteral("activeCalibrationFile"), normalizedPath);
        config.params.insert(QStringLiteral("calibrationTransform"), transform);
        config.summary = fileInfo.fileName();
        m_toolPreviewSnapshots.remove(config.toolId);
        names.append(toolDisplayName(config));
    }

    if (!commitToolStateToScheme(true)) {
        restoreToolState(configsBefore, snapshotsBefore, selectedIndexBefore);
        if (errorMessage)
            *errorMessage = tr("方案保存或主运行态同步失败");
        return false;
    }
    refreshToolList();
    if (selectedIndexBefore >= 0 && selectedIndexBefore < m_toolConfigs.size())
        selectTool(selectedIndexBefore);
    if (appliedToolNames)
        *appliedToolNames = names;
    return true;
}

void ToolsDialog::refreshSchemeHeader()
{
    ui->setupPageCodeLabel->setText(SchemeStore::instance().currentSchemeName());
}

bool ToolsDialog::commitToolStateToScheme(bool saveToDisk)
{
    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        qWarning() << "[ToolsDialog] 方案状态初始化失败:" << error;
        return false;
    }

    store.setToolConfigs(m_toolConfigs, m_toolPreviewSnapshots);
    if (saveToDisk && !store.saveCurrentScheme(&error)) {
        qWarning() << "[ToolsDialog] 方案保存失败:" << error;
        QMessageBox::warning(this, tr("保存失败"), tr("方案保存失败：%1").arg(error));
        return false;
    }
    if (saveToDisk) {
        m_toolConfigs = store.currentScheme().toolConfigs;
        m_toolPreviewSnapshots = store.currentScheme().referencePreviewSnapshots;
        if (MainWindow *mainWindow = qobject_cast<MainWindow *>(parentWidget())) {
            mainWindow->applySavedSchemeTools(m_toolConfigs,
                                              m_toolPreviewSnapshots);
        }
        emit toolStateCommitted(m_toolConfigs, m_toolPreviewSnapshots);
    }

    refreshSchemeHeader();
    return true;
}

void ToolsDialog::restoreToolState(
        const QVector<ToolConfig> &configs,
        const QMap<QString, ToolPreviewSnapshot> &snapshots,
        int selectedIndex)
{
    m_toolConfigs = configs;
    m_toolPreviewSnapshots = snapshots;
    m_selectedToolIndex = selectedIndex;
    SchemeStore::instance().setToolConfigs(m_toolConfigs, m_toolPreviewSnapshots);
    refreshToolList();
    if (m_selectedToolIndex >= 0 && m_selectedToolIndex < m_toolConfigs.size())
        selectTool(m_selectedToolIndex);
    else
        refreshReferencePreview();
}

void ToolsDialog::copySelectedTool()
{
    if (m_selectedToolIndex < 0 || m_selectedToolIndex >= m_toolConfigs.size())
        return;

    const QVector<ToolConfig> configsBefore = m_toolConfigs;
    const QMap<QString, ToolPreviewSnapshot> snapshotsBefore = m_toolPreviewSnapshots;
    const int selectedIndexBefore = m_selectedToolIndex;
    const ToolConfig &sourceConfig = m_toolConfigs.at(m_selectedToolIndex);

    ToolConfig copiedConfig = sourceConfig;
    do {
        copiedConfig.toolId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    } while (std::any_of(m_toolConfigs.cbegin(), m_toolConfigs.cend(),
                         [&copiedConfig](const ToolConfig &config) {
        return config.toolId == copiedConfig.toolId;
    }));

    const int copiedIndex = m_selectedToolIndex + 1;
    m_toolConfigs.insert(copiedIndex, copiedConfig);
    const ToolPreviewSnapshot sourceSnapshot =
            m_toolPreviewSnapshots.value(sourceConfig.toolId);
    if (sourceSnapshot.valid) {
        ToolPreviewSnapshot copiedSnapshot = sourceSnapshot;
        copiedSnapshot.toolId = copiedConfig.toolId;
        copiedSnapshot.toolType = copiedConfig.toolType;
        copiedSnapshot.result.toolId = copiedConfig.toolId;
        m_toolPreviewSnapshots.insert(copiedConfig.toolId, copiedSnapshot);
    }

    m_selectedToolIndex = copiedIndex;
    refreshToolList();
    selectTool(copiedIndex);
    if (!commitToolStateToScheme(true))
        restoreToolState(configsBefore, snapshotsBefore, selectedIndexBefore);
}

void ToolsDialog::deleteSelectedTool()
{
    if (m_selectedToolIndex < 0 || m_selectedToolIndex >= m_toolConfigs.size())
        return;

    const QStringList dependents = dependentToolsFor(m_selectedToolIndex);
    if (!dependents.isEmpty()) {
        QMessageBox::warning(this,
                             tr("无法删除工具"),
                             tr("当前工具正被以下后续工具引用：\n%1\n\n"
                                "请先修改这些工具的位置修正或输入绑定。")
                             .arg(dependents.join(QLatin1Char('\n'))));
        return;
    }

    const ToolConfig selectedConfig = m_toolConfigs.at(m_selectedToolIndex);
    const QString selectedName = toolDisplayName(selectedConfig).trimmed().isEmpty()
            ? tr("未命名工具") : toolDisplayName(selectedConfig);
    const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                tr("删除工具"),
                tr("确认删除选中的“%1”工具？\n删除后将立即保存到当前方案。")
                .arg(selectedName),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    const QVector<ToolConfig> configsBefore = m_toolConfigs;
    const QMap<QString, ToolPreviewSnapshot> snapshotsBefore = m_toolPreviewSnapshots;
    const int selectedIndexBefore = m_selectedToolIndex;
    const int removedIndex = m_selectedToolIndex;
    m_toolConfigs.removeAt(removedIndex);
    m_toolPreviewSnapshots.remove(selectedConfig.toolId);
    m_selectedToolIndex = m_toolConfigs.isEmpty()
            ? -1 : qMin(removedIndex, m_toolConfigs.size() - 1);
    refreshToolList();
    if (m_selectedToolIndex >= 0)
        selectTool(m_selectedToolIndex);
    else
        refreshReferencePreview();
    if (!commitToolStateToScheme(true))
        restoreToolState(configsBefore, snapshotsBefore, selectedIndexBefore);
}

void ToolsDialog::deleteAllTools()
{
    if (m_toolConfigs.isEmpty())
        return;

    const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                tr("删除所有工具"),
                tr("确认删除当前方案中的全部 %1 个工具？\n"
                   "删除后将立即保存且无法撤销。")
                .arg(m_toolConfigs.size()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    const QVector<ToolConfig> configsBefore = m_toolConfigs;
    const QMap<QString, ToolPreviewSnapshot> snapshotsBefore = m_toolPreviewSnapshots;
    const int selectedIndexBefore = m_selectedToolIndex;
    m_toolConfigs.clear();
    m_toolPreviewSnapshots.clear();
    m_selectedToolIndex = -1;
    refreshToolList();
    refreshReferencePreview();
    if (!commitToolStateToScheme(true))
        restoreToolState(configsBefore, snapshotsBefore, selectedIndexBefore);
}

void ToolsDialog::editCurrentSchemeName()
{
    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        QMessageBox::warning(this, tr("方案名称"), tr("方案加载失败：%1").arg(error));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("编辑方案名"),
                                               tr("方案名"),
                                               QLineEdit::Normal,
                                               store.currentSchemeName(),
                                               &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    store.setSchemeName(name);
    commitToolStateToScheme(true);
}

void ToolsDialog::saveCurrentScheme()
{
    commitToolStateToScheme(true);
}

void ToolsDialog::saveCurrentSchemeAs()
{
    if (!commitToolStateToScheme(false))
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("另存为"),
                                               tr("新方案名"),
                                               QLineEdit::Normal,
                                               SchemeStore::instance().currentSchemeName() + tr("_副本"),
                                               &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    QString error;
    if (!SchemeStore::instance().saveCurrentSchemeAs(name, &error)) {
        qWarning() << "[ToolsDialog] 方案另存为失败:" << error;
        QMessageBox::warning(this, tr("另存为失败"), tr("方案另存为失败：%1").arg(error));
        return;
    }

    const SchemeState &scheme = SchemeStore::instance().currentScheme();
    setInitialToolState(scheme.toolConfigs, scheme.referencePreviewSnapshots);
    refreshSchemeHeader();
}

void ToolsDialog::refreshReferencePreview()
{
    if (!m_previewHelper)
        return;

    if (m_selectedToolIndex >= 0 && m_selectedToolIndex < m_toolConfigs.size()) {
        showSelectedToolPreview();
        return;
    }

    const QImage image = currentReferenceImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        ui->viewerStatusLabel->clear();
        return;
    }

    ui->viewerTitleLabel->setText(tr("基准图"));
    m_previewHelper->setImage(image);
    m_previewHelper->clearToolOverlays();
}

void ToolsDialog::openToolLibrary()
{
    ToolType selectedToolType = ToolType::Unknown;

    {
        ToolLibraryDialog library(this);
        library.setWindowModality(Qt::WindowModal);
        library.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        PlanDialogUtils::fitDialogToScreen(&library, this, 40);
        PlanDialogUtils::centerWindowOnScreen(&library, this, 40);

        QTimer::singleShot(0, &library, [&library]() {
            library.raise();
            library.activateWindow();
        });

        if (library.exec() != QDialog::Accepted) {
            return;
        }

        selectedToolType = library.selectedToolType();
    }

    openToolConfigDialogForAdd(selectedToolType);
}

void ToolsDialog::openCameraParamsDialog()
{
    if (!commitToolStateToScheme(true))
        return;
    if (!PlanDialogUtils::switchEmbeddedSetupPage(this, QStringLiteral("camera")))
        qWarning() << "[ToolsDialog] 未找到方案编辑宿主窗口";
}

void ToolsDialog::openReferenceImageDialog()
{
    if (!commitToolStateToScheme(true))
        return;
    if (!PlanDialogUtils::switchEmbeddedSetupPage(this, QStringLiteral("reference")))
        qWarning() << "[ToolsDialog] 未找到方案编辑宿主窗口";
}

void ToolsDialog::openOutputDialog()
{
    if (!commitToolStateToScheme(true))
        return;
    m_openedOutputDialog = true;

    if (!PlanDialogUtils::switchEmbeddedSetupPage(this, QStringLiteral("output")))
        qWarning() << "[ToolsDialog] 未找到方案编辑宿主窗口";
}

void ToolsDialog::addConfiguredTool(const ToolConfig &config, const ToolPreviewSnapshot &snapshot)
{
    if (!m_toolConfigs.isEmpty())
        m_selectedToolIndex = m_toolConfigs.size() - 1;

    storePreviewSnapshot(config, snapshot, false);
    refreshToolList();
    selectTool(m_selectedToolIndex);
}

bool ToolsDialog::openToolConfigDialogForAdd(ToolType type)
{
    ToolConfig config;
    ToolPreviewSnapshot snapshot;
    bool accepted = false;

    switch (type) {
    case ToolType::Ocr:
        accepted = runToolConfigDialog<CharacterRecognitionDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::ColorRecognition:
        accepted = runToolConfigDialog<ColorRecognitionDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::ColorComparison:
        accepted = runToolConfigDialog<ColorComparisonDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::RegisteredClassification:
        accepted = runToolConfigDialog<RegisteredClassificationDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::RegisteredClassificationDetection:
        accepted = runToolConfigDialog<RegisteredClassificationDetectionDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::PatternPresence:
        accepted = runToolConfigDialog<PatternPresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::BlobPresence:
        accepted = runToolConfigDialog<BlobPresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::CirclePresence:
        accepted = runToolConfigDialog<CirclePresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::EdgePresence:
        accepted = runToolConfigDialog<EdgePresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::LinePresence:
        accepted = runToolConfigDialog<LinePresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::ContourPresence:
        accepted = runToolConfigDialog<ContourPresenceDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::AiDetection:
        accepted = runToolConfigDialog<ObjectDetectionDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::AiClassification:
        accepted = runToolConfigDialog<ClassificationDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::PositionCorrection:
        accepted = runToolConfigDialog<PositionCorrectionDialog>(this, nullptr, &config, &snapshot);
        break;
    case ToolType::CalibrationTransform:
        accepted = runToolConfigDialog<CalibrationTransformDialog>(this, nullptr, &config, &snapshot);
        break;
/*============================tfk add=================================*/
    case ToolType::TemplateLocation:
        accepted = runToolConfigDialog<TemplateLocationDialog>(this, nullptr, &config, &snapshot);
        break;

/*============================tfk end=================================*/
    default:
        qDebug() << "[ToolsDialog] Unsupported tool add type:" << toolTypeToString(type);
        break;
    }

    if (!accepted)
        return false;

    const QVector<ToolConfig> configsBefore = m_toolConfigs;
    const QMap<QString, ToolPreviewSnapshot> snapshotsBefore =
            m_toolPreviewSnapshots;
    const int selectedIndexBefore = m_selectedToolIndex;
    m_toolConfigs.append(config);
    addConfiguredTool(config, snapshot);
    if (!commitToolStateToScheme(true)) {
        restoreToolState(configsBefore, snapshotsBefore, selectedIndexBefore);
        return false;
    }
    raise();
    activateWindow();
    return true;
}

bool ToolsDialog::openToolConfigDialogForEdit(int index)
{
    if (index < 0 || index >= m_toolConfigs.size())
        return false;

    const ToolConfig originalConfig = m_toolConfigs.at(index);
    ToolConfig editedConfig;
    ToolPreviewSnapshot snapshot;
    bool accepted = false;

    switch (originalConfig.toolType) {
    case ToolType::Ocr:
        accepted = runToolConfigDialog<CharacterRecognitionDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::ColorRecognition:
        accepted = runToolConfigDialog<ColorRecognitionDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::ColorComparison:
        accepted = runToolConfigDialog<ColorComparisonDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::RegisteredClassification:
        accepted = runToolConfigDialog<RegisteredClassificationDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::RegisteredClassificationDetection:
        accepted = runToolConfigDialog<RegisteredClassificationDetectionDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::PatternPresence:
        accepted = runToolConfigDialog<PatternPresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::BlobPresence:
        accepted = runToolConfigDialog<BlobPresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::CirclePresence:
        accepted = runToolConfigDialog<CirclePresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::EdgePresence:
        accepted = runToolConfigDialog<EdgePresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::LinePresence:
        accepted = runToolConfigDialog<LinePresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::ContourPresence:
        accepted = runToolConfigDialog<ContourPresenceDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::AiDetection:
        accepted = runToolConfigDialog<ObjectDetectionDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::AiClassification:
        accepted = runToolConfigDialog<ClassificationDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::PositionCorrection:
        accepted = runToolConfigDialog<PositionCorrectionDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::CalibrationTransform:
        accepted = runToolConfigDialog<CalibrationTransformDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    case ToolType::TemplateLocation:
        accepted = runToolConfigDialog<TemplateLocationDialog>(this, &originalConfig, &editedConfig, &snapshot);
        break;
    default:
        qDebug() << "[ToolsDialog] Unsupported tool edit type:" << toolTypeToString(originalConfig.toolType);
        break;
    }

    if (!accepted)
        return false;

    const QVector<ToolConfig> configsBefore = m_toolConfigs;
    const QMap<QString, ToolPreviewSnapshot> snapshotsBefore =
            m_toolPreviewSnapshots;
    const int selectedIndexBefore = m_selectedToolIndex;
    editedConfig.toolId = originalConfig.toolId;
    editedConfig.toolType = originalConfig.toolType;
    editedConfig.category = originalConfig.category;
    m_toolConfigs[index] = editedConfig;
    m_selectedToolIndex = index;
    storePreviewSnapshot(editedConfig, snapshot,
                         editedConfig.toolType != ToolType::CalibrationTransform);
    refreshToolList();
    selectTool(index);
    if (!commitToolStateToScheme(true)) {
        restoreToolState(configsBefore, snapshotsBefore, selectedIndexBefore);
        return false;
    }
    raise();
    activateWindow();
    return true;
}

void ToolsDialog::storePreviewSnapshot(const ToolConfig &config,
                                       const ToolPreviewSnapshot &snapshot,
                                       bool keepExistingWhenInvalid)
{
    if (config.toolId.trimmed().isEmpty())
        return;

    if (!snapshot.valid) {
        bool keepExisting = keepExistingWhenInvalid;
        if (keepExisting && config.toolType == ToolType::TemplateLocation) {
            const ToolPreviewSnapshot existing =
                    m_toolPreviewSnapshots.value(config.toolId);
            QJsonObject payload = existing.result.payload;
            if (payload.isEmpty())
                payload = QJsonObject::fromVariantMap(existing.payload);
            const QString currentConfigSignature =
                    CalibrationSourceFingerprint::coordinateSourceConfigSignature(
                        config);
            const QString currentReferenceSignature =
                    CalibrationSourceFingerprint::imageSignature(
                        ReferenceImageProvider::instance().referenceFrame());
            keepExisting = existing.valid
                    && payload.value(QStringLiteral(
                                         "coordinateSourceConfigSignature"))
                       .toString() == currentConfigSignature
                    && !currentReferenceSignature.isEmpty()
                    && payload.value(QStringLiteral(
                                         "coordinateSourceReferenceSignature"))
                       .toString() == currentReferenceSignature;
        }
        if (!keepExisting)
            m_toolPreviewSnapshots.remove(config.toolId);
        return;
    }

    ToolPreviewSnapshot normalized = snapshot;
    normalized.toolId = config.toolId;
    normalized.toolType = config.toolType;
    normalized.result.toolId = config.toolId;
    normalized.result.toolType = config.toolType;
    m_toolPreviewSnapshots.insert(config.toolId, normalized);
}

void ToolsDialog::refreshToolList()
{
    clearToolList();

    if (m_toolConfigs.isEmpty()) {
        m_selectedToolIndex = -1;
    } else if (m_selectedToolIndex < 0 || m_selectedToolIndex >= m_toolConfigs.size()) {
        m_selectedToolIndex = m_toolConfigs.size() - 1;
    }

    m_toolCards.reserve(m_toolConfigs.size());
    for (int index = 0; index < m_toolConfigs.size(); ++index) {
        QFrame *card = createToolCard(m_toolConfigs.at(index), index);
        m_toolCards.append(card);
        ui->verticalLayout_toolsList->addWidget(card);
    }

    ui->verticalLayout_toolsList->addStretch(1);
    updateToolCardSelection();
    m_toolSerial = m_toolConfigs.size();
    updateToolbarActionState();
}

void ToolsDialog::clearToolList()
{
    while (QLayoutItem *item = ui->verticalLayout_toolsList->takeAt(0)) {
        if (QWidget *widget = item->widget())
            delete widget;
        delete item;
    }
    m_toolCards.clear();
}

void ToolsDialog::updateToolbarActionState()
{
    const bool hasSelection = m_selectedToolIndex >= 0
            && m_selectedToolIndex < m_toolConfigs.size();
    ui->copyToolButton->setEnabled(hasSelection);
    ui->deleteToolButton->setEnabled(hasSelection);
    ui->deleteAllToolsButton->setEnabled(!m_toolConfigs.isEmpty());
}

QStringList ToolsDialog::dependentToolsFor(int producerIndex) const
{
    QStringList dependents;
    if (producerIndex < 0 || producerIndex >= m_toolConfigs.size())
        return dependents;

    const QString producerId = m_toolConfigs.at(producerIndex).toolId.trimmed();
    if (producerId.isEmpty())
        return dependents;

    for (int index = producerIndex + 1; index < m_toolConfigs.size(); ++index) {
        const ToolConfig &candidate = m_toolConfigs.at(index);
        if (!containsToolReference(candidate.params, producerId))
            continue;
        const QString name = toolDisplayName(candidate).trimmed().isEmpty()
                ? tr("未命名工具") : toolDisplayName(candidate);
        dependents.append(tr("#%1 %2").arg(index + 1).arg(name));
    }
    return dependents;
}

QFrame *ToolsDialog::createToolCard(const ToolConfig &config, int index)
{
    QFrame *card = new QFrame(ui->scrollAreaWidgetContents);
    card->setObjectName(QStringLiteral("toolItemCard"));
    card->setProperty("panelRole", QStringLiteral("toolItem"));
    card->setProperty("selected", false);
    card->setProperty("toolIndex", index);
    card->setProperty("toolId", config.toolId);
    card->setFixedHeight(kToolCardHeight);
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    card->setCursor(Qt::PointingHandCursor);
    card->installEventFilter(this);

    QHBoxLayout *rowLayout = new QHBoxLayout(card);
    rowLayout->setContentsMargins(14, 10, 12, 10);
    rowLayout->setSpacing(10);

    QLabel *indexBadge = new QLabel(QStringLiteral("#%1").arg(index + 1), card);
    indexBadge->setObjectName(QStringLiteral("toolIndexBadge"));
    indexBadge->setProperty("role", QStringLiteral("toolIndexBadge"));
    indexBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    rowLayout->addWidget(indexBadge);

    QLabel *statusBadge = new QLabel(config.enabled ? tr("启用") : tr("停用"), card);
    statusBadge->setObjectName(QStringLiteral("toolStatusBadge"));
    statusBadge->setProperty("role", QStringLiteral("toolStatusBadge"));
    statusBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    rowLayout->addWidget(statusBadge);

    QLabel *typeIcon = new QLabel(card);
    typeIcon->setObjectName(QStringLiteral("toolTypeIconLabel"));
    typeIcon->setProperty("role", QStringLiteral("toolTypeIcon"));
    typeIcon->setAlignment(Qt::AlignCenter);
    typeIcon->setFixedSize(34, 34);
    typeIcon->setPixmap(QIcon(toolIconForType(config.toolType)).pixmap(QSize(24, 24)));
    typeIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    rowLayout->addWidget(typeIcon);

    const QString title = toolDisplayName(config);
    QLabel *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("toolTitleLabel"));
    titleLabel->setProperty("role", QStringLiteral("toolTitle"));
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    titleLabel->setToolTip(title);
    rowLayout->addWidget(titleLabel, 1);

    const QString previewState = toolPreviewStateText(config);
    QLabel *resultBadge = new QLabel(previewState, card);
    resultBadge->setObjectName(QStringLiteral("toolResultBadge"));
    resultBadge->setProperty("role", QStringLiteral("toolResultBadge"));
    resultBadge->setProperty("testState", previewState == QStringLiteral("OK")
                             ? QStringLiteral("ok")
                             : (previewState == QStringLiteral("NG") ? QStringLiteral("ng") : QStringLiteral("untested")));
    resultBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
    rowLayout->addWidget(resultBadge);

    QToolButton *gearButton = new QToolButton(card);
    gearButton->setObjectName(QStringLiteral("toolGearButton"));
    gearButton->setProperty("role", QStringLiteral("toolGear"));
    gearButton->setIcon(QIcon(QStringLiteral(":/icons/settings-dark.svg")));
    gearButton->setIconSize(QSize(22, 22));
    gearButton->setToolTip(tr("编辑工具参数"));
    rowLayout->addWidget(gearButton);

    connect(gearButton, &QToolButton::clicked, this, [this, index]() {
        openToolConfigDialogForEdit(index);
    });

    return card;
}

void ToolsDialog::selectTool(int index)
{
    if (index < 0 || index >= m_toolConfigs.size())
        return;

    m_selectedToolIndex = index;
    updateToolCardSelection();
    showSelectedToolPreview();

    const ToolConfig &config = m_toolConfigs.at(index);
    qDebug() << "[ToolsDialog] 选中工具"
             << "index=" << index
             << "toolId=" << config.toolId
             << "toolType=" << toolTypeToString(config.toolType);
}

void ToolsDialog::updateToolCardSelection()
{
    for (int index = 0; index < m_toolCards.size(); ++index) {
        QFrame *card = m_toolCards.at(index);
        if (!card)
            continue;
        card->setProperty("selected", index == m_selectedToolIndex);
        refreshWidgetStyle(card);
    }
}

void ToolsDialog::showSelectedToolPreview()
{
    if (!m_previewHelper || m_selectedToolIndex < 0 || m_selectedToolIndex >= m_toolConfigs.size())
        return;

    const ToolConfig &config = m_toolConfigs.at(m_selectedToolIndex);
    const QString title = toolDisplayName(config);
    ui->viewerTitleLabel->setText(title.isEmpty() ? tr("工具预览") : title);

    const QImage image = currentReferenceImage();
    const bool hasReferenceImage = !image.isNull();
    if (hasReferenceImage)
        m_previewHelper->setImage(image);
    else
        m_previewHelper->clear();
    m_previewHelper->clearRoi();

    const ToolPreviewSnapshot snapshot = m_toolPreviewSnapshots.value(config.toolId);
    if (!snapshot.valid) {
        if (hasReferenceImage) {
            m_previewHelper->clearToolOverlays();
            ui->viewerStatusLabel->setText(tr("当前工具暂无基准图测试快照"));
        } else {
            ui->viewerStatusLabel->setText(tr("当前无基准图，且该工具暂无基准图测试快照"));
        }
        return;
    }

    if (!hasReferenceImage) {
        ui->viewerStatusLabel->setText(tr("当前无基准图，无法显示该工具基准图测试快照"));
        return;
    }

    m_previewHelper->setToolOverlays(snapshot.overlays);
    ui->viewerStatusLabel->setText(toolPreviewStatusLine(snapshot));
}

QString ToolsDialog::toolDisplayName(const ToolConfig &config) const
{
    if (!config.displayName.trimmed().isEmpty())
        return config.displayName;
    if (!config.toolName.trimmed().isEmpty())
        return config.toolName;

    switch (config.toolType) {
    case ToolType::Ocr:
        return tr("字符识别");
    case ToolType::ColorRecognition:
        return tr("颜色识别");
    case ToolType::ColorComparison:
        return tr("颜色比较");
    case ToolType::RegisteredClassification:
        return tr("注册分类");
    case ToolType::RegisteredClassificationDetection:
        return tr("注册目标检测");
    case ToolType::PatternPresence:
        return tr("图案有无");
    case ToolType::BlobPresence:
        return tr("斑点有无");
    case ToolType::CirclePresence:
        return tr("圆有无");
    case ToolType::EdgePresence:
        return tr("边缘有无");
    case ToolType::LinePresence:
        return tr("直线有无");
    case ToolType::ContourPresence:
        return tr("轮廓有无");
    case ToolType::AiDetection:
        return tr("目标检测");
    case ToolType::AiClassification:
        return tr("分类");
    case ToolType::PositionCorrection:
        return tr("位置修正");
    case ToolType::CalibrationTransform:
        return tr("标定转换");
    case ToolType::TemplateLocation:
        return tr("模板定位");
    default:
        return toolTypeToString(config.toolType);
    }
}

QString ToolsDialog::toolPreviewStateText(const ToolConfig &config) const
{
    const ToolPreviewSnapshot snapshot = m_toolPreviewSnapshots.value(config.toolId);
    if (!snapshot.valid)
        return tr("未测试");

    return snapshot.ok ? QStringLiteral("OK") : QStringLiteral("NG");
}

QString ToolsDialog::toolPreviewStatusLine(const ToolPreviewSnapshot &snapshot) const
{
    const QString state = snapshot.ok ? QStringLiteral("OK") : QStringLiteral("NG");
    const QString status = snapshot.statusText.trimmed().isEmpty()
            ? snapshot.result.status
            : snapshot.statusText;
    if (snapshot.result.success
            && (snapshot.toolType == ToolType::CalibrationTransform
                || snapshot.result.toolType == ToolType::CalibrationTransform)) {
        const QJsonObject payload = snapshot.result.payload;
        const QString angleText = payload.value(QStringLiteral("angleValid")).toBool()
                && payload.value(QStringLiteral("machineAngle")).isDouble()
                ? tr("机械角度:%1°").arg(
                      QString::number(payload.value(
                                          QStringLiteral("machineAngle")).toDouble(),
                                      'f', 3))
                : tr("机械角度:未配置");
        return tr("%1 | %2 | 物理X:%3 | 物理Y:%4 | %5 | %6ms")
                .arg(status,
                     state,
                     QString::number(payload.value(QStringLiteral("machineX")).toDouble(),
                                     'f', 3),
                     QString::number(payload.value(QStringLiteral("machineY")).toDouble(),
                                     'f', 3),
                     angleText,
                     QString::number(snapshot.result.elapsedMs));
    }
    return tr("%1 | %2 | score:%3 | count:%4")
            .arg(status,
                 state,
                 QString::number(snapshot.score, 'f', 3),
                 QString::number(snapshot.count));
}
