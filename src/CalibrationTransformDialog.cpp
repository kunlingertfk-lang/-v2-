#include "CalibrationTransformDialog.h"
#include "ui_CalibrationTransformDialog.h"

#include "PlanDialogUtils.h"
#include "SchemeStore.h"
#include "calibration/CalibrationFileLoader.h"
#include "calibration/CalibrationSourceFingerprint.h"
#include "frame/FrameInputMetadata.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolEngine.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QStyle>
#include <QSignalBlocker>
#include <QToolButton>
#include <QUuid>

#include <opencv2/imgcodecs.hpp>

#include <cmath>

namespace {

QJsonObject constantBinding(double value)
{
    return QJsonObject{{QStringLiteral("mode"), QStringLiteral("constant")},
                       {QStringLiteral("value"), value}};
}

void addConstantItem(QComboBox *combo)
{
    combo->clear();
    combo->addItem(QObject::tr("自定义"), QJsonObject{{QStringLiteral("mode"),
                                                       QStringLiteral("constant")}});
}

void addUnboundItem(QComboBox *combo)
{
    combo->clear();
    combo->addItem(QObject::tr("未绑定"),
                   QJsonObject{{QStringLiteral("mode"), QStringLiteral("unbound")}});
}

QJsonObject bindingItem(const QString &producerId,
                        const QString &outputKey,
                        const QString &displayPath)
{
    return QJsonObject{{QStringLiteral("mode"), QStringLiteral("binding")},
                       {QStringLiteral("producerId"), producerId},
                       {QStringLiteral("outputKey"), outputKey},
                       {QStringLiteral("displayPath"), displayPath}};
}

QString payloadNumber(const QJsonObject &payload,
                      const QString &key,
                      int precision)
{
    const QJsonValue value = payload.value(key);
    if (!value.isDouble())
        return QStringLiteral("--");
    const double number = value.toDouble();
    return std::isfinite(number)
            ? QString::number(number, 'f', precision)
            : QStringLiteral("--");
}

QString calibrationRegionDisplayText(const QString &region)
{
    if (region == QStringLiteral("safe"))
        return QObject::tr("Safe（安全区）");
    if (region == QStringLiteral("boundary"))
        return QObject::tr("Boundary（边界警戒带）");
    if (region == QStringLiteral("extrapolation"))
        return QObject::tr("Extrapolation（凸包外推）");
    if (region == QStringLiteral("invalid"))
        return QObject::tr("Invalid（无效）");
    return region.trimmed().isEmpty() ? QStringLiteral("--") : region;
}

QString rotationCoverageDisplayText(const QString &coverage)
{
    if (coverage == QStringLiteral("not_configured"))
        return QObject::tr("未启用（9点XY）");
    if (coverage == QStringLiteral("verified"))
        return QObject::tr("已验证");
    if (coverage == QStringLiteral("in_range"))
        return QObject::tr("范围内");
    if (coverage == QStringLiteral("out_of_range"))
        return QObject::tr("超出范围");
    if (coverage == QStringLiteral("unverified"))
        return QObject::tr("未验证");
    if (coverage == QStringLiteral("invalid"))
        return QObject::tr("无效");
    return coverage.trimmed().isEmpty() ? QStringLiteral("--") : coverage;
}

QString calibrationModeDisplayText(NPointCalibrationMode mode)
{
    switch (mode) {
    case NPointCalibrationMode::NinePointXY:
        return QObject::tr("9点 XY标定");
    case NPointCalibrationMode::TwelvePointAxisTrace:
        return QObject::tr("12点 旋转中心/轴轨迹标定");
    case NPointCalibrationMode::TwelvePointPoseMapping:
        return QObject::tr("12点 位置与姿态标定");
    }
    return QObject::tr("未知模式");
}

QString angleMappingDisplayText(const CalibrationAngleMapping &mapping)
{
    switch (mapping.status) {
    case CalibrationAngleMappingStatus::NotConfigured:
        return QObject::tr("未配置");
    case CalibrationAngleMappingStatus::Verified:
        return QObject::tr("已验证，机械角范围 %1° ~ %2°")
                .arg(mapping.machineMinDeg, 0, 'f', 2)
                .arg(mapping.machineMaxDeg, 0, 'f', 2);
    case CalibrationAngleMappingStatus::Invalid:
        return QObject::tr("无效");
    }
    return QObject::tr("未知");
}

bool loadCalibrationModel(const QString &filePath,
                          CalibrationModel *model,
                          QString *errorMessage)
{
    ProjectXmlCalibrationLoader project;
    HikXmlCalibrationLoader hikXml;
    HikIwcalCalibrationLoader iwcal;
    const CalibrationFileLoader *loader = project.canLoad(filePath)
            ? static_cast<const CalibrationFileLoader *>(&project)
            : iwcal.canLoad(filePath)
              ? static_cast<const CalibrationFileLoader *>(&iwcal)
              : hikXml.canLoad(filePath)
                ? static_cast<const CalibrationFileLoader *>(&hikXml) : nullptr;
    if (!loader) {
        if (errorMessage)
            *errorMessage = QObject::tr("不支持的标定文件格式");
        return false;
    }
    return loader->load(filePath, model, errorMessage);
}

QJsonObject snapshotPayload(const ToolPreviewSnapshot &snapshot)
{
    if (!snapshot.valid)
        return QJsonObject();
    if (!snapshot.result.payload.isEmpty())
        return snapshot.result.payload;
    return QJsonObject::fromVariantMap(snapshot.payload);
}

QString fingerprintFieldText(const QString &field)
{
    if (field == QStringLiteral("producerId"))
        return QObject::tr("来源节点");
    if (field == QStringLiteral("producerType"))
        return QObject::tr("来源类型");
    if (field == QStringLiteral("outputContract"))
        return QObject::tr("输出字段");
    if (field == QStringLiteral("originMode"))
        return QObject::tr("原点模式");
    if (field == QStringLiteral("customOriginNormalized"))
        return QObject::tr("自定义原点");
    if (field == QStringLiteral("modelSignature"))
        return QObject::tr("模板模型");
    if (field == QStringLiteral("referenceImageSignature"))
        return QObject::tr("基准图");
    if (field == QStringLiteral("coordinateSourceConfigSignature"))
        return QObject::tr("模板配置");
    return field.isEmpty() ? QObject::tr("来源信息") : field;
}

bool completeSignedFingerprint(const QJsonObject &fingerprint)
{
    const QString signature = fingerprint
            .value(QStringLiteral("coordinateSourceSignature")).toString().trimmed();
    return !signature.isEmpty()
            && signature == CalibrationSourceFingerprint::coordinateSourceSignature(
                fingerprint)
            && !fingerprint.value(QStringLiteral("producerId")).toString().trimmed().isEmpty()
            && fingerprint.value(QStringLiteral("producerType")).toString()
               == toolTypeToString(ToolType::TemplateLocation)
            && !fingerprint.value(QStringLiteral("outputContract")).toObject().isEmpty()
            && !fingerprint.value(QStringLiteral("originMode")).toString().trimmed().isEmpty()
            && !fingerprint.value(QStringLiteral("customOriginNormalized")).toObject().isEmpty()
            && !fingerprint.value(QStringLiteral("modelSignature")).toString().trimmed().isEmpty()
            && !fingerprint.value(QStringLiteral("coordinateSourceConfigSignature"))
                .toString().trimmed().isEmpty();
}

} // namespace

CalibrationTransformDialog::CalibrationTransformDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CalibrationTransformDialog)
{
    ui->setupUi(this);
    PlanDialogUtils::configureDialogWindow(this, tr("标定转换"));
    ui->headerFrame->setObjectName(QStringLiteral("calibrationTransformHeader"));
    ui->footerFrame->setObjectName(QStringLiteral("calibrationTransformFooter"));
    ui->configScrollArea->setObjectName(QStringLiteral("calibrationTransformConfigScroll"));
    ui->resultFrame->setObjectName(QStringLiteral("calibrationTransformPreview"));
    ui->titleLabel->setProperty("role", QStringLiteral("dialogTitle"));
    ui->sectionTitle->setProperty("role", QStringLiteral("editorTitle"));
    ui->basicModeButton->setProperty("segmentRole", QStringLiteral("mode"));
    ui->allModeButton->setProperty("segmentRole", QStringLiteral("mode"));
    ui->inputGroup->setProperty("panelRole", QStringLiteral("configCard"));
    ui->fileGroup->setProperty("panelRole", QStringLiteral("configCard"));
    ui->poseGroup->setProperty("panelRole", QStringLiteral("configCard"));
    ui->regionGroup->setProperty("panelRole", QStringLiteral("configCard"));
    for (QLabel *label : {ui->coordinateTypeLabel, ui->inputXLabel, ui->inputYLabel,
                          ui->inputAngleLabel,
                          ui->calXLabel, ui->calYLabel,
                          ui->calJ0Label, ui->calJ1Label, ui->runXLabel,
                          ui->runYLabel, ui->runJ0Label, ui->runJ1Label,
                          ui->regionSafeMarginLabel, ui->regionValidCountLabel,
                          ui->regionSafeCountLabel, ui->regionGenerationLabel})
        label->setProperty("role", QStringLiteral("rowField"));
    ui->closeButton->setProperty("actionRole", QStringLiteral("windowClose"));
    ui->importButton->setProperty("actionRole", QStringLiteral("secondary"));
    ui->calibrationTransformPcImportButton->setProperty(
                "actionRole", QStringLiteral("secondary"));
    ui->calibrationTransformPcImportButton->setProperty("optionalEntry", true);
    ui->exitPcTestButton->setProperty("actionRole", QStringLiteral("secondary"));
    ui->testButton->setProperty("actionRole", QStringLiteral("secondary"));
    ui->finishButton->setProperty("actionRole", QStringLiteral("highlight"));
    ui->conversionResultOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->conversionResultLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->conversionResultLabel->setProperty("resultState", QStringLiteral("idle"));
    ui->viewerCanvasLayout->setCurrentWidget(ui->conversionResultOverlay);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);
    updateReferenceImage(ReferenceImageProvider::instance().referenceImage());
    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this, &CalibrationTransformDialog::updateReferenceImage);
    m_initialConfig.toolId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ui->coordinateTypeCombo->addItem(tr("图像坐标"), QStringLiteral("image"));
    ui->coordinateTypeLabel->hide();
    ui->coordinateTypeCombo->hide();
    addUnboundItem(ui->inputXSourceCombo);
    addUnboundItem(ui->inputYSourceCombo);
    addUnboundItem(ui->inputAngleSourceCombo);
    ui->inputXSourceCombo->hide();
    ui->inputYSourceCombo->hide();
    ui->inputAngleSourceCombo->hide();
    m_inputXLinkButton = createInputBindingButton(ui->inputGroup, tr("坐标 X"));
    m_inputXLinkButton->setObjectName(QStringLiteral("inputXLinkButton"));
    m_inputYLinkButton = createInputBindingButton(ui->inputGroup, tr("坐标 Y"));
    m_inputYLinkButton->setObjectName(QStringLiteral("inputYLinkButton"));
    m_inputAngleLinkButton = createInputBindingButton(ui->inputGroup, tr("图像角度"));
    m_inputAngleLinkButton->setObjectName(QStringLiteral("inputAngleLinkButton"));
    ui->xLayout->addWidget(m_inputXLinkButton);
    ui->yLayout->addWidget(m_inputYLinkButton);
    ui->angleLayout->addWidget(m_inputAngleLinkButton);
    updateInputAngleRequirement(false);
    updateMainInputUi();
    setupPoseSourceUi();
    ui->basicModeButton->setChecked(true);
    ui->poseGroup->setVisible(false);
    ui->regionGroup->setVisible(false);

    connect(ui->closeButton, &QToolButton::clicked, this, &QDialog::reject);
    connect(ui->finishButton, &QPushButton::clicked,
            this, &CalibrationTransformDialog::finishConfiguration);
    connect(ui->importButton, &QPushButton::clicked,
            this, &CalibrationTransformDialog::importCalibrationFile);
    connect(ui->calibrationTransformPcImportButton, &QPushButton::clicked,
            this, &CalibrationTransformDialog::importTestImageFromPc);
    connect(ui->exitPcTestButton, &QPushButton::clicked,
            this, &CalibrationTransformDialog::exitImportedTestMode);
    connect(ui->testButton, &QPushButton::clicked,
            this, &CalibrationTransformDialog::runTest);
    connect(ui->calibrationFileCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        invalidatePreviewSnapshot();
        refreshCalibrationRegionSummary();
        refreshCalibrationSourceValidation();
    });
    connect(ui->allowBoundaryForProductionCheck, &QCheckBox::toggled,
            this, [this](bool) { invalidatePreviewSnapshot(); });
    connect(ui->calibrationPoseEnabled, &QCheckBox::toggled,
            this, [this](bool) { invalidatePreviewSnapshot(); });
    connect(ui->runPoseEnabled, &QCheckBox::toggled,
            this, [this](bool) { invalidatePreviewSnapshot(); });
    for (QDoubleSpinBox *spin : {ui->calXSpin, ui->calYSpin,
                                 ui->calJ0Spin, ui->calJ1Spin,
                                 ui->runXSpin, ui->runYSpin,
                                 ui->runJ0Spin, ui->runJ1Spin}) {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double) { invalidatePreviewSnapshot(); });
    }
    for (QComboBox *combo : m_calibrationPoseSources) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { invalidatePreviewSnapshot(); });
    }
    for (QComboBox *combo : m_runPoseSources) {
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { invalidatePreviewSnapshot(); });
    }
    connect(ui->basicModeButton, &QPushButton::clicked, this, [this]() {
        ui->basicModeButton->setChecked(true);
        ui->allModeButton->setChecked(false);
        ui->poseGroup->setVisible(false);
        ui->regionGroup->setVisible(false);
    });
    connect(ui->allModeButton, &QPushButton::clicked, this, [this]() {
        ui->basicModeButton->setChecked(false);
        ui->allModeButton->setChecked(true);
        ui->poseGroup->setVisible(true);
        ui->regionGroup->setVisible(true);
    });
    // 新建工具没有 loadFromConfig() 调用，也应能看到当前方案的标定资产。
    updateImportedTestUi();
    mergeSchemeCalibrationFiles();
    refreshFileList(m_calibrationFiles, QString());
}

void CalibrationTransformDialog::setupPoseSourceUi()
{
    ui->calXLabel->setText(tr("标定位坐标点 X"));
    ui->calYLabel->setText(tr("标定位坐标点 Y"));
    ui->calJ0Label->setText(tr("标定位旋转角度"));
    ui->calJ1Label->setText(tr("标定位关节1角度"));
    ui->runXLabel->setText(tr("运行位坐标点 X"));
    ui->runYLabel->setText(tr("运行位坐标点 Y"));
    ui->runJ0Label->setText(tr("运行位旋转角度"));
    ui->runJ1Label->setText(tr("运行位关节1角度"));

    const QList<QWidget *> designedWidgets{
        ui->calibrationPoseEnabled, ui->calXLabel, ui->calYLabel,
        ui->calJ0Label, ui->calJ1Label, ui->runPoseEnabled,
        ui->runXLabel, ui->runYLabel, ui->runJ0Label, ui->runJ1Label
    };
    for (QWidget *widget : designedWidgets)
        ui->poseLayout->removeWidget(widget);
    ui->poseLayout->addWidget(ui->calibrationPoseEnabled, 0, 0, 1, 2);
    ui->poseLayout->addWidget(ui->calXLabel, 1, 0);
    ui->poseLayout->addWidget(ui->calYLabel, 2, 0);
    ui->poseLayout->addWidget(ui->calJ0Label, 3, 0);
    ui->poseLayout->addWidget(ui->runPoseEnabled, 4, 0, 1, 2);
    ui->poseLayout->addWidget(ui->runXLabel, 5, 0);
    ui->poseLayout->addWidget(ui->runYLabel, 6, 0);
    ui->poseLayout->addWidget(ui->runJ0Label, 7, 0);

    auto wrapField = [this](QDoubleSpinBox *spin, int row, int column,
                            QComboBox **stateCombo, QToolButton **linkButton,
                            const QString &fieldName) -> QWidget * {
        ui->poseLayout->removeWidget(spin);
        QWidget *container = new QWidget(ui->poseGroup);
        container->setProperty("role", QStringLiteral("bindingField"));
        QHBoxLayout *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);
        spin->setParent(container);
        layout->addWidget(spin, 1);
        *stateCombo = new QComboBox(container);
        addConstantItem(*stateCombo);
        (*stateCombo)->hide();
        *linkButton = createBindingButton(*stateCombo, spin, container, fieldName);
        layout->addWidget(*linkButton);
        if (row >= 0)
            ui->poseLayout->addWidget(container, row, column);
        return container;
    };
    wrapField(ui->calXSpin, 1, 1, &m_calibrationPoseSources[0],
              &m_calibrationPoseLinkButtons[0], ui->calXLabel->text());
    wrapField(ui->calYSpin, 2, 1, &m_calibrationPoseSources[1],
              &m_calibrationPoseLinkButtons[1], ui->calYLabel->text());
    wrapField(ui->calJ0Spin, 3, 1, &m_calibrationPoseSources[2],
              &m_calibrationPoseLinkButtons[2], ui->calJ0Label->text());
    QWidget *calibrationJoint1Field = wrapField(
                ui->calJ1Spin, -1, 1, &m_calibrationPoseSources[3],
                &m_calibrationPoseLinkButtons[3], ui->calJ1Label->text());
    wrapField(ui->runXSpin, 5, 1, &m_runPoseSources[0],
              &m_runPoseLinkButtons[0], ui->runXLabel->text());
    wrapField(ui->runYSpin, 6, 1, &m_runPoseSources[1],
              &m_runPoseLinkButtons[1], ui->runYLabel->text());
    wrapField(ui->runJ0Spin, 7, 1, &m_runPoseSources[2],
              &m_runPoseLinkButtons[2], ui->runJ0Label->text());
    QWidget *runJoint1Field = wrapField(
                ui->runJ1Spin, -1, 1, &m_runPoseSources[3],
                &m_runPoseLinkButtons[3], ui->runJ1Label->text());

    // Joint1 尚无平面运动学合同：保留隐藏控件以回显旧配置，
    // 但不在当前 X/Y/旋转角度界面中暴露数值或订阅入口。
    ui->calJ1Label->hide();
    ui->runJ1Label->hide();
    calibrationJoint1Field->hide();
    runJoint1Field->hide();
}

QToolButton *CalibrationTransformDialog::createBindingButton(QComboBox *stateCombo,
                                                              QDoubleSpinBox *valueSpin,
                                                              QWidget *parent,
                                                              const QString &fieldName)
{
    QToolButton *button = new QToolButton(parent);
    button->setIcon(QIcon(QStringLiteral(":/icons/external-link.svg")));
    button->setIconSize(QSize(22, 22));
    button->setPopupMode(QToolButton::InstantPopup);
    button->setProperty("actionRole", QStringLiteral("bindingLink"));
    QMenu *menu = new QMenu(button);
    button->setMenu(menu);
    connect(menu, &QMenu::aboutToShow, this,
            [this, menu, stateCombo, valueSpin, button, fieldName]() {
        menu->clear();
        for (int index = 0; index < stateCombo->count(); ++index) {
            if (index > 0
                    && stateCombo->itemData(index - 1).toJsonObject()
                       .value(QStringLiteral("mode")).toString()
                       != QStringLiteral("binding")
                    && stateCombo->itemData(index).toJsonObject()
                       .value(QStringLiteral("mode")).toString()
                       == QStringLiteral("binding"))
                menu->addSeparator();
            const QString mode = stateCombo->itemData(index).toJsonObject()
                    .value(QStringLiteral("mode")).toString();
            QAction *action = menu->addAction(
                        mode == QStringLiteral("constant")
                        ? tr("使用自定义值") : stateCombo->itemText(index));
            action->setCheckable(true);
            action->setChecked(index == stateCombo->currentIndex());
            connect(action, &QAction::triggered, this,
                    [this, stateCombo, valueSpin, button, fieldName, index]() {
                stateCombo->setCurrentIndex(index);
                updateBindingButton(stateCombo, valueSpin, button, fieldName);
            });
        }
        bool hasBinding = false;
        for (int index = 0; index < stateCombo->count(); ++index) {
            hasBinding = hasBinding
                    || stateCombo->itemData(index).toJsonObject()
                       .value(QStringLiteral("mode")).toString()
                       == QStringLiteral("binding");
        }
        if (!hasBinding) {
            menu->addSeparator();
            QAction *empty = menu->addAction(tr("无可用前序输出"));
            empty->setEnabled(false);
        }
    });
    connect(stateCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this, stateCombo, valueSpin, button, fieldName](int) {
        updateBindingButton(stateCombo, valueSpin, button, fieldName);
    });
    updateBindingButton(stateCombo, valueSpin, button, fieldName);
    return button;
}

QToolButton *CalibrationTransformDialog::createInputBindingButton(
        QWidget *parent,
        const QString &fieldName)
{
    QToolButton *button = new QToolButton(parent);
    button->setIcon(QIcon(QStringLiteral(":/icons/external-link.svg")));
    button->setIconSize(QSize(22, 22));
    button->setPopupMode(QToolButton::InstantPopup);
    button->setProperty("actionRole", QStringLiteral("bindingLink"));
    QMenu *menu = new QMenu(button);
    button->setMenu(menu);
    connect(menu, &QMenu::aboutToShow, this, [this, menu]() {
        menu->clear();
        for (int index = 0; index < m_inputProducers.size(); ++index) {
            const InputProducerContract &producer = m_inputProducers.at(index);
            QMenu *producerMenu = menu->addMenu(producer.displayName);
            QAction *poseAction = producerMenu->addAction(
                        m_inputAngleRequired
                        ? tr("图像坐标 X / Y / Angle")
                        : tr("图像坐标 X / Y"));
            poseAction->setCheckable(true);
            const QJsonObject selected = ui->inputXSourceCombo->currentData().toJsonObject();
            poseAction->setChecked(m_mainInputSourceAvailable
                                   && selected.value(QStringLiteral("producerId")).toString()
                                      == producer.producerId);
            connect(poseAction, &QAction::triggered, this,
                    [this, index]() { applyInputProducer(index); });
        }
        if (m_inputProducers.isEmpty()) {
            QAction *empty = menu->addAction(tr("无可用前序坐标输出"));
            empty->setEnabled(false);
        }
    });
    button->setToolTip(tr("为%1选择前序图像坐标来源").arg(fieldName));
    return button;
}

void CalibrationTransformDialog::updateBindingButton(QComboBox *stateCombo,
                                                      QDoubleSpinBox *valueSpin,
                                                      QToolButton *button,
                                                      const QString &fieldName)
{
    const QString mode = stateCombo->currentData().toJsonObject()
            .value(QStringLiteral("mode")).toString();
    const bool bound = mode == QStringLiteral("binding");
    const bool custom = mode == QStringLiteral("constant");
    button->setProperty("bindingActive", bound);
    button->setToolTip(bound
                       ? tr("%1 已订阅：%2").arg(fieldName, stateCombo->currentText())
                       : custom ? tr("%1 使用自定义值").arg(fieldName)
                                : tr("请为%1选择输入方式").arg(fieldName));
    valueSpin->setEnabled(custom);
    button->style()->unpolish(button);
    button->style()->polish(button);
}

void CalibrationTransformDialog::applyInputProducer(int producerIndex)
{
    if (producerIndex < 0 || producerIndex >= m_inputProducers.size())
        return;
    const int comboIndex = producerIndex + 1;
    if (comboIndex >= ui->inputXSourceCombo->count()
            || comboIndex >= ui->inputYSourceCombo->count()
            || comboIndex >= ui->inputAngleSourceCombo->count()) {
        return;
    }
    ui->inputXSourceCombo->setCurrentIndex(comboIndex);
    ui->inputYSourceCombo->setCurrentIndex(comboIndex);
    ui->inputAngleSourceCombo->setCurrentIndex(comboIndex);
    m_mainInputSourceAvailable = true;
    invalidatePreviewSnapshot();
    updateMainInputUi();
    refreshCalibrationSourceValidation();
}

void CalibrationTransformDialog::invalidatePreviewSnapshot()
{
    m_snapshot = ToolPreviewSnapshot();
    clearDisplayedConversionResult();
}

void CalibrationTransformDialog::clearDisplayedConversionResult()
{
    ui->conversionResultLabel->clear();
    ui->conversionResultLabel->setProperty("resultState", QStringLiteral("idle"));
    ui->conversionResultLabel->hide();
    ui->conversionResultLabel->style()->unpolish(ui->conversionResultLabel);
    ui->conversionResultLabel->style()->polish(ui->conversionResultLabel);
}

void CalibrationTransformDialog::updateMainInputUi()
{
    const std::array<QComboBox *, 3> combos{{ui->inputXSourceCombo,
                                             ui->inputYSourceCombo,
                                             ui->inputAngleSourceCombo}};
    const std::array<QLineEdit *, 3> edits{{ui->inputXEdit,
                                            ui->inputYEdit,
                                            ui->inputAngleEdit}};
    const std::array<QToolButton *, 3> buttons{{m_inputXLinkButton,
                                                m_inputYLinkButton,
                                                m_inputAngleLinkButton}};
    for (int index = 0; index < 3; ++index) {
        const QJsonObject binding = combos[static_cast<size_t>(index)]
                ->currentData().toJsonObject();
        const bool bound = binding.value(QStringLiteral("mode")).toString()
                == QStringLiteral("binding");
        QString text;
        if (!bound) {
            text = tr("未绑定（请选择前序坐标）");
        } else if (!m_mainInputSourceAvailable) {
            text = tr("来源不可用：%1").arg(combos[static_cast<size_t>(index)]->currentText());
        } else {
            text = combos[static_cast<size_t>(index)]->currentText();
        }
        edits[static_cast<size_t>(index)]->setText(text);
        edits[static_cast<size_t>(index)]->setProperty(
                    "bindingState", bound && m_mainInputSourceAvailable
                    ? QStringLiteral("active") : QStringLiteral("invalid"));
        edits[static_cast<size_t>(index)]->style()->unpolish(
                    edits[static_cast<size_t>(index)]);
        edits[static_cast<size_t>(index)]->style()->polish(
                    edits[static_cast<size_t>(index)]);
        if (buttons[static_cast<size_t>(index)]) {
            buttons[static_cast<size_t>(index)]->setProperty(
                        "bindingActive", bound && m_mainInputSourceAvailable);
            buttons[static_cast<size_t>(index)]->setToolTip(
                        bound && m_mainInputSourceAvailable
                        ? tr("已订阅：%1").arg(text)
                        : tr("选择前序图像坐标来源"));
            buttons[static_cast<size_t>(index)]->style()->unpolish(
                        buttons[static_cast<size_t>(index)]);
            buttons[static_cast<size_t>(index)]->style()->polish(
                        buttons[static_cast<size_t>(index)]);
        }
    }
}

void CalibrationTransformDialog::updateInputAngleRequirement(bool required)
{
    m_inputAngleRequired = required;
    ui->inputAngleLabel->setVisible(required);
    ui->inputAngleEdit->setVisible(required);
    if (m_inputAngleLinkButton)
        m_inputAngleLinkButton->setVisible(required);
    ui->inputAngleLabel->setText(required ? tr("图像角度 *") : tr("图像角度"));
    updateMainInputUi();
}

CalibrationTransformDialog::~CalibrationTransformDialog()
{
    delete ui;
}

void CalibrationTransformDialog::updateReferenceImage(const QImage &image)
{
    invalidatePreviewSnapshot();
    if (m_importedTestActive && !m_importedTestFrame.empty()) {
        displayImportedTestImage();
        ui->statusLabel->setProperty("sourceValidationState",
                                     QStringLiteral("testPending"));
        ui->statusLabel->setStyleSheet(QStringLiteral("color: #edf1f5;"));
        ui->statusLabel->setText(
                    tr("基准图已更新，请重新运行当前 PC 导入图片"));
        return;
    }
    displayReferenceImage(image);
}

void CalibrationTransformDialog::displayReferenceImage(const QImage &image)
{
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        ui->viewerStatusLabel->setText(tr("当前方案没有可用的基准图"));
        return;
    }
    ui->viewerTitleLabel->setText(tr("基准图"));
    ui->viewerStatusLabel->setText(tr("基准图已加载 | %1 × %2")
                                   .arg(image.width()).arg(image.height()));
    m_previewHelper->setImage(image);
}

void CalibrationTransformDialog::displayImportedTestImage()
{
    if (!m_importedTestActive || m_importedTestFrame.empty())
        return;
    const QImage image = MatImageConverter::matToDisplayImage(
                m_importedTestFrame,
                QStringLiteral("CalibrationTransformDialog.pcImport"));
    ui->viewerTitleLabel->setText(
                m_importedTestImageTitle.trimmed().isEmpty()
                ? tr("PC导入图片") : m_importedTestImageTitle);
    ui->viewerStatusLabel->setText(
                tr("PC导入图片已加载 | %1 × %2")
                .arg(m_importedTestFrame.cols).arg(m_importedTestFrame.rows));
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerStatusLabel->setText(
                    tr("PC导入图片已加载，但当前像素格式无法预览 | %1 × %2")
                    .arg(m_importedTestFrame.cols).arg(m_importedTestFrame.rows));
        return;
    }
    m_previewHelper->setImage(image);
}

void CalibrationTransformDialog::updateImportedTestUi()
{
    const bool active = m_importedTestActive && !m_importedTestFrame.empty();
    ui->exitPcTestButton->setVisible(active);
    ui->testButton->setText(active ? tr("测试运行（导入图）")
                                   : tr("测试运行"));
    ui->testButton->setToolTip(
                active
                ? tr("重新运行当前 PC 导入图片：%1")
                  .arg(m_importedTestImageTitle)
                : QString());
    ui->finishButton->setText(active ? tr("运行一次") : tr("完成"));
    ui->finishButton->setToolTip(
                active ? tr("导入图测试结果不保存；请先退出图片测试")
                       : QString());
}

void CalibrationTransformDialog::setProducerTools(const QVector<ToolConfig> &tools,
                                                   int consumerIndex,
                                                   const QMap<QString, ToolPreviewSnapshot> &snapshots)
{
    invalidatePreviewSnapshot();
    const QJsonObject savedInputX = ui->inputXSourceCombo->currentData().toJsonObject();
    const QJsonObject savedInputY = ui->inputYSourceCombo->currentData().toJsonObject();
    const QJsonObject savedInputAngle = ui->inputAngleSourceCombo->currentData().toJsonObject();
    std::array<QJsonObject, 4> savedCalibrationPose;
    std::array<QJsonObject, 4> savedRunPose;
    for (int i = 0; i < 4; ++i) {
        savedCalibrationPose[static_cast<size_t>(i)] =
                m_calibrationPoseSources[static_cast<size_t>(i)]->currentData().toJsonObject();
        savedRunPose[static_cast<size_t>(i)] =
                m_runPoseSources[static_cast<size_t>(i)]->currentData().toJsonObject();
    }
    m_inputProducers.clear();
    m_producerConfigs.clear();
    m_producerSnapshots.clear();
    addUnboundItem(ui->inputXSourceCombo);
    addUnboundItem(ui->inputYSourceCombo);
    addUnboundItem(ui->inputAngleSourceCombo);
    for (QComboBox *combo : m_calibrationPoseSources)
        addConstantItem(combo);
    for (QComboBox *combo : m_runPoseSources)
        addConstantItem(combo);
    const int limit = qBound(0, consumerIndex, tools.size());
    for (int index = 0; index < limit; ++index) {
        const ToolConfig &tool = tools.at(index);
        if (!tool.toolId.trimmed().isEmpty()) {
            m_producerConfigs.insert(tool.toolId, tool);
            const auto snapshotIt = snapshots.constFind(tool.toolId);
            if (snapshotIt != snapshots.cend())
                m_producerSnapshots.insert(tool.toolId, snapshotIt.value());
        }
        if (!tool.enabled || tool.toolId.trimmed().isEmpty()) {
            continue;
        }
        if (tool.toolType != ToolType::TemplateLocation
                && tool.toolType != ToolType::PositionCorrection) {
            continue;
        }
        const QString title = tool.displayName.trimmed().isEmpty()
                ? tr("%1 %2").arg(index + 1).arg(
                      tool.toolType == ToolType::TemplateLocation
                      ? tr("模板定位") : tr("位置修正"))
                : tr("%1 %2").arg(index + 1).arg(tool.displayName);
        const bool templateLocation = tool.toolType == ToolType::TemplateLocation;
        const QString xKey = templateLocation
                ? QStringLiteral("x") : QStringLiteral("runPose.x");
        const QString yKey = templateLocation
                ? QStringLiteral("y") : QStringLiteral("runPose.y");
        const QString angleKey = templateLocation
                ? QStringLiteral("angle") : QStringLiteral("runPose.angleDeg");
        m_inputProducers.append(InputProducerContract{tool.toolId, title,
                                                       xKey, yKey, angleKey});
        ui->inputXSourceCombo->addItem(title + tr(".运行点X"),
                                      bindingItem(tool.toolId, xKey, title));
        ui->inputYSourceCombo->addItem(title + tr(".运行点Y"),
                                      bindingItem(tool.toolId, yKey, title));
        ui->inputAngleSourceCombo->addItem(title + tr(".运行角度"),
                                          bindingItem(tool.toolId, angleKey, title));
        if (!templateLocation) {
            continue;
        }
        const QStringList outputKeys{QStringLiteral("x"), QStringLiteral("y"),
                                     QStringLiteral("angle"), QStringLiteral("angle")};
        const QStringList outputNames{tr("运行点X"), tr("运行点Y"),
                                      tr("运行角度"), tr("运行角度")};
        for (int field = 0; field < 4; ++field) {
            const QJsonObject binding = bindingItem(tool.toolId, outputKeys.at(field), title);
            m_calibrationPoseSources[static_cast<size_t>(field)]->addItem(
                        title + QStringLiteral(".") + outputNames.at(field), binding);
            m_runPoseSources[static_cast<size_t>(field)]->addItem(
                        title + QStringLiteral(".") + outputNames.at(field), binding);
        }
    }
    restoreMainInputBindings(savedInputX, savedInputY, savedInputAngle);
    for (int i = 0; i < 4; ++i) {
        restoreBinding(m_calibrationPoseSources[static_cast<size_t>(i)],
                       savedCalibrationPose[static_cast<size_t>(i)]);
        restoreBinding(m_runPoseSources[static_cast<size_t>(i)],
                       savedRunPose[static_cast<size_t>(i)]);
    }
    refreshCalibrationSourceValidation();
}

void CalibrationTransformDialog::setToolChainTestContext(
        const QVector<ToolConfig> &tools,
        int consumerIndex,
        ToolEngine *sharedToolEngine,
        const ReferencePositionCorrectionConfig &referencePositionCorrection)
{
    invalidatePreviewSnapshot();
    const int limit = consumerIndex < 0
            ? tools.size() : qBound(0, consumerIndex, tools.size());
    m_testToolPrefix = tools.mid(0, limit);
    m_sharedToolEngine = sharedToolEngine;
    m_referencePositionCorrection = referencePositionCorrection;
}

void CalibrationTransformDialog::restoreMainInputBindings(
        const QJsonObject &x,
        const QJsonObject &y,
        const QJsonObject &angle)
{
    const QString producerId = x.value(QStringLiteral("producerId")).toString().trimmed();
    const bool complete = x.value(QStringLiteral("mode")).toString()
            == QStringLiteral("binding")
            && y.value(QStringLiteral("mode")).toString() == QStringLiteral("binding")
            && !producerId.isEmpty()
            && y.value(QStringLiteral("producerId")).toString() == producerId;
    if (!complete) {
        ui->inputXSourceCombo->setCurrentIndex(0);
        ui->inputYSourceCombo->setCurrentIndex(0);
        ui->inputAngleSourceCombo->setCurrentIndex(0);
        m_mainInputSourceAvailable = false;
        updateMainInputUi();
        return;
    }

    int sourceIndex = -1;
    for (int index = 0; index < m_inputProducers.size(); ++index) {
        const InputProducerContract &producer = m_inputProducers.at(index);
        if (producer.producerId == producerId
                && x.value(QStringLiteral("outputKey")).toString() == producer.xKey
                && y.value(QStringLiteral("outputKey")).toString() == producer.yKey) {
            sourceIndex = index;
            break;
        }
    }
    if (sourceIndex >= 0) {
        applyInputProducer(sourceIndex);
        return;
    }

    const QString unavailable = x.value(QStringLiteral("displayPath"))
            .toString(tr("已保存的订阅来源"));
    ui->inputXSourceCombo->addItem(unavailable + tr(".运行点X"), x);
    ui->inputYSourceCombo->addItem(unavailable + tr(".运行点Y"), y);
    const QJsonObject unavailableAngle =
            angle.value(QStringLiteral("mode")).toString() == QStringLiteral("binding")
            && angle.value(QStringLiteral("producerId")).toString() == producerId
            ? angle
            : QJsonObject{{QStringLiteral("mode"), QStringLiteral("unbound")}};
    ui->inputAngleSourceCombo->addItem(unavailable + tr(".运行角度"),
                                      unavailableAngle);
    ui->inputXSourceCombo->setCurrentIndex(ui->inputXSourceCombo->count() - 1);
    ui->inputYSourceCombo->setCurrentIndex(ui->inputYSourceCombo->count() - 1);
    ui->inputAngleSourceCombo->setCurrentIndex(ui->inputAngleSourceCombo->count() - 1);
    m_mainInputSourceAvailable = false;
    updateMainInputUi();
}

QJsonObject CalibrationTransformDialog::bindingFor(QComboBox *combo,
                                                   double constantValue) const
{
    const QJsonObject selected = combo->currentData().toJsonObject();
    if (selected.value(QStringLiteral("mode")).toString() == QStringLiteral("binding"))
        return selected;
    return constantBinding(constantValue);
}

QJsonObject CalibrationTransformDialog::mainInputBinding(QComboBox *combo) const
{
    const QJsonObject selected = combo->currentData().toJsonObject();
    if (selected.value(QStringLiteral("mode")).toString() == QStringLiteral("binding"))
        return selected;
    return QJsonObject{{QStringLiteral("mode"), QStringLiteral("unbound")}};
}

void CalibrationTransformDialog::restoreBinding(QComboBox *combo,
                                                const QJsonObject &binding)
{
    if (binding.value(QStringLiteral("mode")).toString() != QStringLiteral("binding")) {
        combo->setCurrentIndex(0);
        return;
    }
    for (int i = 1; i < combo->count(); ++i) {
        const QJsonObject candidate = combo->itemData(i).toJsonObject();
        if (candidate.value(QStringLiteral("producerId"))
                    == binding.value(QStringLiteral("producerId"))
                && candidate.value(QStringLiteral("outputKey"))
                    == binding.value(QStringLiteral("outputKey"))) {
            combo->setCurrentIndex(i);
            return;
        }
    }
    combo->addItem(binding.value(QStringLiteral("displayPath")).toString(
                       tr("已保存的订阅来源")), binding);
    combo->setCurrentIndex(combo->count() - 1);
}

void CalibrationTransformDialog::loadFromConfig(const ToolConfig &config)
{
    m_importedTestActive = false;
    m_importedTestFrame.release();
    m_importedTestImageTitle.clear();
    updateImportedTestUi();
    invalidatePreviewSnapshot();
    ui->resultText->clear();
    displayReferenceImage(ReferenceImageProvider::instance().referenceImage());
    const QString stableToolId = m_initialConfig.toolId;
    m_initialConfig = config;
    if (m_initialConfig.toolId.trimmed().isEmpty())
        m_initialConfig.toolId = stableToolId;
    const QJsonObject params = config.params.value(QStringLiteral("calibrationTransform")).toObject();
    ui->coordinateTypeCombo->setCurrentIndex(0);
    const QJsonObject x = params.value(QStringLiteral("inputX")).toObject();
    const QJsonObject y = params.value(QStringLiteral("inputY")).toObject();
    const QJsonObject angle = params.value(QStringLiteral("inputAngle")).toObject();
    restoreMainInputBindings(x, y, angle);

    const QJsonArray files = params.value(QStringLiteral("calibrationFiles")).toArray();
    m_calibrationFiles.clear();
    for (const QJsonValue &value : files) {
        const QString path = value.toString().trimmed();
        if (!path.isEmpty() && !m_calibrationFiles.contains(path))
            m_calibrationFiles.append(path);
    }
    const QString active = params.value(QStringLiteral("activeCalibrationFile")).toString();
    if (!active.isEmpty() && !m_calibrationFiles.contains(active))
        m_calibrationFiles.append(active);
    mergeSchemeCalibrationFiles();
    refreshFileList(m_calibrationFiles, active);
    ui->allowBoundaryForProductionCheck->setChecked(
                params.value(QStringLiteral("allowBoundaryForProduction"))
                .toBool(false));

    const auto restorePose = [this](const QJsonObject &pose, bool calibration,
                                    Ui::CalibrationTransformDialog *ui) {
        QCheckBox *enabled = calibration ? ui->calibrationPoseEnabled : ui->runPoseEnabled;
        QDoubleSpinBox *xSpin = calibration ? ui->calXSpin : ui->runXSpin;
        QDoubleSpinBox *ySpin = calibration ? ui->calYSpin : ui->runYSpin;
        QDoubleSpinBox *j0Spin = calibration ? ui->calJ0Spin : ui->runJ0Spin;
        QDoubleSpinBox *j1Spin = calibration ? ui->calJ1Spin : ui->runJ1Spin;
        enabled->setChecked(pose.value(QStringLiteral("enabled")).toBool(false));
        xSpin->setValue(pose.value(QStringLiteral("x")).toObject().value(QStringLiteral("value")).toDouble());
        ySpin->setValue(pose.value(QStringLiteral("y")).toObject().value(QStringLiteral("value")).toDouble());
        j0Spin->setValue(pose.value(QStringLiteral("joint0Angle")).toObject().value(QStringLiteral("value")).toDouble());
        j1Spin->setValue(pose.value(QStringLiteral("joint1Angle")).toObject().value(QStringLiteral("value")).toDouble());
        const QStringList keys{QStringLiteral("x"), QStringLiteral("y"),
                               QStringLiteral("joint0Angle"), QStringLiteral("joint1Angle")};
        const std::array<QComboBox *, 4> &sources = calibration
                ? m_calibrationPoseSources : m_runPoseSources;
        for (int i = 0; i < 4; ++i)
            restoreBinding(sources[static_cast<size_t>(i)], pose.value(keys.at(i)).toObject());
    };
    restorePose(params.value(QStringLiteral("calibrationPose")).toObject(), true, ui);
    restorePose(params.value(QStringLiteral("runPose")).toObject(), false, ui);
    refreshCalibrationSourceValidation();
}

QJsonObject CalibrationTransformDialog::poseConfig(bool calibration) const
{
    QCheckBox *enabled = calibration ? ui->calibrationPoseEnabled : ui->runPoseEnabled;
    QDoubleSpinBox *xSpin = calibration ? ui->calXSpin : ui->runXSpin;
    QDoubleSpinBox *ySpin = calibration ? ui->calYSpin : ui->runYSpin;
    QDoubleSpinBox *j0Spin = calibration ? ui->calJ0Spin : ui->runJ0Spin;
    QDoubleSpinBox *j1Spin = calibration ? ui->calJ1Spin : ui->runJ1Spin;
    const std::array<QComboBox *, 4> &sources = calibration
            ? m_calibrationPoseSources : m_runPoseSources;
    return QJsonObject{{QStringLiteral("enabled"), enabled->isChecked()},
                       {QStringLiteral("x"), bindingFor(sources[0], xSpin->value())},
                       {QStringLiteral("y"), bindingFor(sources[1], ySpin->value())},
                       {QStringLiteral("joint0Angle"), bindingFor(sources[2], j0Spin->value())},
                       {QStringLiteral("joint1Angle"), bindingFor(sources[3], j1Spin->value())}};
}

ToolConfig CalibrationTransformDialog::toolConfig() const
{
    ToolConfig config = m_initialConfig;
    config.toolType = ToolType::CalibrationTransform;
    config.category = ToolCategory::Location;
    config.toolName = QStringLiteral("CalibrationTransform");
    if (config.displayName.trimmed().isEmpty())
        config.displayName = tr("标定转换");
    QJsonArray files;
    for (const QString &path : m_calibrationFiles)
        files.append(path);
    QJsonObject params{
        {QStringLiteral("version"), 4},
        {QStringLiteral("coordinateType"), QStringLiteral("image")},
        {QStringLiteral("inputX"), mainInputBinding(ui->inputXSourceCombo)},
        {QStringLiteral("inputY"), mainInputBinding(ui->inputYSourceCombo)},
        {QStringLiteral("inputAngle"),
         m_inputAngleRequired
         ? mainInputBinding(ui->inputAngleSourceCombo)
         : QJsonObject{{QStringLiteral("mode"), QStringLiteral("unbound")}}},
        {QStringLiteral("calibrationFiles"), files},
        {QStringLiteral("activeCalibrationFile"), ui->calibrationFileCombo->currentData().toString()},
        {QStringLiteral("allowBoundaryForProduction"),
         ui->allowBoundaryForProductionCheck->isChecked()},
        {QStringLiteral("calibrationPose"), poseConfig(true)},
        {QStringLiteral("runPose"), poseConfig(false)}
    };
    config.params.insert(QStringLiteral("calibrationTransform"), params);
    config.summary = ui->calibrationFileCombo->currentText();
    return config;
}

ToolPreviewSnapshot CalibrationTransformDialog::referencePreviewSnapshot() const
{
    return m_snapshot;
}

void CalibrationTransformDialog::refreshFileList(const QStringList &paths,
                                                const QString &activePath)
{
    const QSignalBlocker blocker(ui->calibrationFileCombo);
    ui->calibrationFileCombo->clear();
    for (const QString &path : paths)
        ui->calibrationFileCombo->addItem(QFileInfo(path).fileName(), path);
    const int activeIndex = ui->calibrationFileCombo->findData(activePath);
    if (activeIndex >= 0)
        ui->calibrationFileCombo->setCurrentIndex(activeIndex);
    refreshCalibrationRegionSummary();
    refreshCalibrationSourceValidation();
}

void CalibrationTransformDialog::refreshCalibrationRegionSummary()
{
    const auto showSummary = [this](const QString &margin,
                                    const QString &validCount,
                                    const QString &safeCount,
                                    const QString &message,
                                    const QString &state) {
        ui->regionSafeMarginEdit->setText(margin);
        ui->regionValidCountEdit->setText(validCount);
        ui->regionSafeCountEdit->setText(safeCount);
        ui->regionGenerationValue->setText(message);
        ui->regionGenerationValue->setProperty("regionState", state);
        ui->regionGenerationValue->style()->unpolish(ui->regionGenerationValue);
        ui->regionGenerationValue->style()->polish(ui->regionGenerationValue);
    };

    const QString filePath = ui->calibrationFileCombo->currentData().toString().trimmed();
    if (filePath.isEmpty()) {
        updateInputAngleRequirement(false);
        showSummary(QStringLiteral("--"), QStringLiteral("--"),
                    QStringLiteral("--"), tr("尚未加载标定文件"),
                    QStringLiteral("idle"));
        return;
    }

    CalibrationModel model;
    QString error;
    if (!QFileInfo::exists(filePath)
            || !loadCalibrationModel(filePath, &model, &error)) {
        updateInputAngleRequirement(false);
        showSummary(QStringLiteral("--"), QStringLiteral("--"),
                    QStringLiteral("--"),
                    error.trimmed().isEmpty() ? tr("标定文件无法读取") : error,
                    QStringLiteral("error"));
        return;
    }

    const bool regionsReady = model.validRegion.size() >= 3
            && model.safeRegion.size() >= 3
            && std::isfinite(model.safeMarginPx)
            && model.safeMarginPx >= 0.0;
    const bool poseMapping = model.mode
            == NPointCalibrationMode::TwelvePointPoseMapping;
    updateInputAngleRequirement(poseMapping);
    const QString modeText = calibrationModeDisplayText(model.mode);
    const QString rotationText = rotationCoverageDisplayText(
                calibrationRotationRangeStatusToString(
                    model.rotationRange.status));
    const QString angleMappingText = angleMappingDisplayText(model.angleMapping);
    showSummary(tr("%1 px").arg(model.safeMarginPx, 0, 'f', 2),
                QString::number(model.validRegion.size()),
                QString::number(model.safeRegion.size()),
                regionsReady
                ? tr("平移点凸包 + HALCON 均匀内缩；%1；轴轨迹：%2；角度映射：%3")
                  .arg(modeText, rotationText, angleMappingText)
                : tr("区域结构无效，禁止生产使用"),
                regionsReady ? QStringLiteral("ok") : QStringLiteral("error"));
}

void CalibrationTransformDialog::mergeSchemeCalibrationFiles()
{
    const QString schemeDir = SchemeStore::instance().currentScheme().schemeDir;
    if (schemeDir.trimmed().isEmpty())
        return;
    const QDir assetDir(QDir(schemeDir).filePath(QStringLiteral("calibrations")));
    const QFileInfoList candidates = assetDir.entryInfoList(
                QStringList{QStringLiteral("*.xml"), QStringLiteral("*.iwcal")},
                QDir::Files | QDir::Readable, QDir::Time);
    for (const QFileInfo &candidate : candidates) {
        const QString path = candidate.absoluteFilePath();
        if (!m_calibrationFiles.contains(path))
            m_calibrationFiles.append(path);
    }
}

void CalibrationTransformDialog::importCalibrationFile()
{
    const QString selected = QFileDialog::getOpenFileName(
                this, tr("导入标定文件"), QString(),
                tr("标定文件 (*.xml *.iwcal);;所有文件 (*)"));
    if (selected.isEmpty())
        return;
    ProjectXmlCalibrationLoader project;
    HikXmlCalibrationLoader hikXml;
    HikIwcalCalibrationLoader iwcal;
    CalibrationModel probe;
    QString error;
    const CalibrationFileLoader *loader = project.canLoad(selected)
            ? static_cast<const CalibrationFileLoader *>(&project)
            : iwcal.canLoad(selected)
              ? static_cast<const CalibrationFileLoader *>(&iwcal)
              : static_cast<const CalibrationFileLoader *>(&hikXml);
    if (!loader->load(selected, &probe, &error)) {
        QMessageBox::warning(this, tr("导入失败"), error);
        return;
    }
    if (!m_calibrationFiles.contains(selected))
        m_calibrationFiles.append(selected);
    refreshFileList(m_calibrationFiles, selected);
    refreshCalibrationSourceValidation();
}

bool CalibrationTransformDialog::loadTestImageFromFile(
        const QString &filePath,
        QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    const QString normalizedPath = filePath.trimmed();
    if (normalizedPath.isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("未选择测试图片");
        return false;
    }

    const cv::Mat decoded = cv::imread(
                normalizedPath.toLocal8Bit().constData(),
                cv::IMREAD_UNCHANGED);
    if (decoded.empty()) {
        if (errorMessage)
            *errorMessage = tr("无法读取所选图片");
        return false;
    }

    m_importedTestFrame = decoded.clone();
    m_importedTestImageTitle = QFileInfo(normalizedPath).fileName();
    m_importedTestActive = true;
    clearDisplayedConversionResult();
    ui->resultText->clear();
    ui->statusLabel->setProperty("sourceValidationState",
                                 QStringLiteral("testPending"));
    ui->statusLabel->setStyleSheet(QStringLiteral("color: #edf1f5;"));
    ui->statusLabel->setText(
                tr("PC图片 %1 已导入，等待测试运行")
                .arg(m_importedTestImageTitle));
    updateImportedTestUi();
    displayImportedTestImage();
    return true;
}

void CalibrationTransformDialog::importTestImageFromPc()
{
    const QString selected = QFileDialog::getOpenFileName(
                this,
                tr("PC导入测试图片"),
                QString(),
                tr("图片 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;所有文件 (*.*)"));
    if (selected.trimmed().isEmpty())
        return;

    QString error;
    if (!loadTestImageFromFile(selected, &error)) {
        QMessageBox::warning(this, tr("PC导入图片"), error);
        return;
    }
    runTest();
}

void CalibrationTransformDialog::exitImportedTestMode()
{
    if (!m_importedTestActive && m_importedTestFrame.empty())
        return;
    m_importedTestActive = false;
    m_importedTestFrame.release();
    m_importedTestImageTitle.clear();
    updateImportedTestUi();
    clearDisplayedConversionResult();
    ui->resultText->clear();
    displayReferenceImage(ReferenceImageProvider::instance().referenceImage());
    refreshCalibrationSourceValidation();
}

CalibrationTransformDialog::SourceValidationResult
CalibrationTransformDialog::evaluateCalibrationSource() const
{
    SourceValidationResult validation;
    const QString filePath = ui->calibrationFileCombo->currentData().toString().trimmed();
    if (filePath.isEmpty()) {
        validation.message = tr("请选择标定文件");
        return validation;
    }
    if (!QFileInfo::exists(filePath)) {
        validation.message = tr("标定文件不存在");
        return validation;
    }

    CalibrationModel model;
    QString loadError;
    if (!loadCalibrationModel(filePath, &model, &loadError)) {
        validation.message = loadError.isEmpty() ? tr("标定文件无法读取") : loadError;
        return validation;
    }

    const QJsonObject imageBinding = model.imageBinding;
    QJsonObject expected = imageBinding
            .value(QStringLiteral("coordinateSourceFingerprint")).toObject();
    if (expected.isEmpty()) {
        expected = imageBinding.value(QStringLiteral("inputFingerprint")).toObject()
                .value(QStringLiteral("coordinateSourceFingerprint")).toObject();
    }
    if (expected.isEmpty()) {
        validation.state = SourceValidationState::Unverifiable;
        validation.message = imageBinding.value(QStringLiteral("mode")).toString()
                == QStringLiteral("manual")
                ? tr("该文件由手动图像坐标生成，未记录可追踪的模板来源")
                : tr("旧版标定文件未记录模板原点和配置签名");
        return validation;
    }
    if (expected.value(QStringLiteral("mode")).toString()
            == QStringLiteral("manual")) {
        validation.state = SourceValidationState::Unverifiable;
        validation.message = tr("该文件由手动图像坐标生成，未记录可追踪的模板来源");
        return validation;
    }
    if (!completeSignedFingerprint(expected)) {
        validation.state = SourceValidationState::Stale;
        validation.message = tr("标定文件中的坐标来源签名不完整或已损坏");
        return validation;
    }

    const QJsonObject xBinding = mainInputBinding(ui->inputXSourceCombo);
    const QJsonObject yBinding = mainInputBinding(ui->inputYSourceCombo);
    const QJsonObject angleBinding = mainInputBinding(ui->inputAngleSourceCombo);
    const QString selectedProducerId = xBinding
            .value(QStringLiteral("producerId")).toString().trimmed();
    if (!m_mainInputSourceAvailable
            || selectedProducerId.isEmpty()
            || yBinding.value(QStringLiteral("producerId")).toString()
               != selectedProducerId
            || (m_inputAngleRequired
                && angleBinding.value(QStringLiteral("producerId")).toString()
                   != selectedProducerId)) {
        validation.message = m_inputAngleRequired
                ? tr("请先选择完整的当前 X/Y/Angle 坐标来源")
                : tr("请先选择完整的当前 X/Y 坐标来源");
        return validation;
    }
    const auto selectedConfigIt = m_producerConfigs.constFind(selectedProducerId);
    if (selectedConfigIt == m_producerConfigs.cend()) {
        validation.state = SourceValidationState::Stale;
        validation.message = tr("标定时使用的坐标来源节点已不存在");
        return validation;
    }

    const ToolConfig &selectedConfig = selectedConfigIt.value();
    QString effectiveProducerId = selectedProducerId;
    ToolConfig effectiveTemplateConfig;
    bool hasEffectiveTemplateConfig = false;
    QJsonObject identityPayload;

    if (selectedConfig.toolType == ToolType::TemplateLocation) {
        const QJsonObject outputContract = expected
                .value(QStringLiteral("outputContract")).toObject();
        if (xBinding.value(QStringLiteral("outputKey")).toString()
                != outputContract.value(QStringLiteral("x")).toString()
                || yBinding.value(QStringLiteral("outputKey")).toString()
                != outputContract.value(QStringLiteral("y")).toString()
                || (m_inputAngleRequired
                    && angleBinding.value(QStringLiteral("outputKey")).toString()
                       != outputContract.value(QStringLiteral("angle")).toString())) {
            validation.state = SourceValidationState::Stale;
            validation.message = tr("当前订阅的输出字段与标定时不一致");
            return validation;
        }
        effectiveTemplateConfig = selectedConfig;
        hasEffectiveTemplateConfig = true;
        identityPayload = snapshotPayload(m_producerSnapshots.value(selectedProducerId));
    } else if (selectedConfig.toolType == ToolType::PositionCorrection) {
        if (xBinding.value(QStringLiteral("outputKey")).toString()
                != QStringLiteral("runPose.x")
                || yBinding.value(QStringLiteral("outputKey")).toString()
                   != QStringLiteral("runPose.y")
                || (m_inputAngleRequired
                    && angleBinding.value(QStringLiteral("outputKey")).toString()
                       != QStringLiteral("runPose.angleDeg"))) {
            validation.state = SourceValidationState::Stale;
            validation.message = tr("当前位置修正输出字段与标定转换坐标合同不一致");
            return validation;
        }
        const QJsonObject correctionPayload = snapshotPayload(
                    m_producerSnapshots.value(selectedProducerId));
        const QString snapshotProducerId = correctionPayload
                .value(QStringLiteral("poseProducerId")).toString().trimmed();
        const PositionRunPoseSource traced = PositionCorrection::runPoseSourceFromConfig(
                    selectedConfig.params.value(
                        QStringLiteral("positionCorrection")).toObject());
        if (traced.valid && !snapshotProducerId.isEmpty()
                && traced.producerId != snapshotProducerId) {
            validation.state = SourceValidationState::Stale;
            validation.message = tr("位置修正配置与其最近输出指向不同的模板来源");
            return validation;
        }
        effectiveProducerId = traced.valid ? traced.producerId : snapshotProducerId;
        if (effectiveProducerId.isEmpty()) {
            validation.state = SourceValidationState::Unverifiable;
            validation.message = tr("位置修正未提供可追溯的模板定位来源");
            return validation;
        }
        identityPayload = correctionPayload;
        const auto templateConfigIt = m_producerConfigs.constFind(effectiveProducerId);
        if (templateConfigIt != m_producerConfigs.cend()
                && templateConfigIt->toolType == ToolType::TemplateLocation) {
            effectiveTemplateConfig = templateConfigIt.value();
            hasEffectiveTemplateConfig = true;
        }
        if (identityPayload.value(QStringLiteral("modelSignature"))
                .toString().trimmed().isEmpty()) {
            identityPayload = snapshotPayload(
                        m_producerSnapshots.value(effectiveProducerId));
        }
    } else {
        validation.state = SourceValidationState::Stale;
        validation.message = tr("当前节点不再提供标定所需的图像坐标");
        return validation;
    }

    if (expected.value(QStringLiteral("producerId")).toString().trimmed()
            != effectiveProducerId) {
        validation.state = SourceValidationState::Stale;
        validation.message = tr("当前坐标来源节点与标定时不一致");
        return validation;
    }

    if (hasEffectiveTemplateConfig) {
        const QString cachedConfigSignature = identityPayload.value(
                    QStringLiteral("coordinateSourceConfigSignature"))
                .toString().trimmed();
        const QString cachedReferenceSignature = identityPayload.value(
                    QStringLiteral("coordinateSourceReferenceSignature"))
                .toString().trimmed();
        const QString currentConfigSignature =
                CalibrationSourceFingerprint::coordinateSourceConfigSignature(
                    effectiveTemplateConfig);
        const QString currentReferenceSignature =
                CalibrationSourceFingerprint::imageSignature(
                    ReferenceImageProvider::instance().referenceFrame());
        const bool expectsReferenceSignature = expected.contains(
                    QStringLiteral("referenceImageSignature"));
        const bool snapshotFresh = !cachedConfigSignature.isEmpty()
                && cachedConfigSignature == currentConfigSignature
                && (!expectsReferenceSignature
                    || (!currentReferenceSignature.isEmpty()
                        && cachedReferenceSignature
                           == currentReferenceSignature));
        if (!snapshotFresh)
            identityPayload.remove(QStringLiteral("modelSignature"));
        const QJsonObject params = effectiveTemplateConfig.params;
        identityPayload.insert(
                    QStringLiteral("originMode"),
                    params.value(QStringLiteral("originMode"))
                    .toString(QStringLiteral("centroid")));
        identityPayload.insert(
                    QStringLiteral("customOriginNormalized"),
                    CalibrationSourceFingerprint::normalizedPoint(
                        params.value(QStringLiteral("customOriginNormalized")).toObject()));
        CalibrationSourceFingerprint::enrichTemplatePayload(
                    effectiveTemplateConfig, &identityPayload);
        identityPayload.insert(
                    QStringLiteral("coordinateSourceReferenceSignature"),
                    currentReferenceSignature);
    }

    const bool hasCurrentModelSignature = !identityPayload
            .value(QStringLiteral("modelSignature")).toString().trimmed().isEmpty();
    const bool hasCurrentConfigSignature = !identityPayload
            .value(QStringLiteral("coordinateSourceConfigSignature"))
            .toString().trimmed().isEmpty();
    if (!hasCurrentModelSignature || !hasCurrentConfigSignature) {
        // Even without a current preview, configuration and origin changes are
        // still observable and must invalidate the calibration immediately.
        const QString currentConfigSignature = identityPayload
                .value(QStringLiteral("coordinateSourceConfigSignature"))
                .toString().trimmed();
        if (!currentConfigSignature.isEmpty()
                && currentConfigSignature
                != expected.value(QStringLiteral("coordinateSourceConfigSignature"))
                   .toString()) {
            validation.state = SourceValidationState::Stale;
            validation.message = tr("模板定位配置已改变");
            return validation;
        }
        const QString currentOriginMode = identityPayload
                .value(QStringLiteral("originMode")).toString().trimmed();
        if (!currentOriginMode.isEmpty()
                && currentOriginMode
                != expected.value(QStringLiteral("originMode")).toString()) {
            validation.state = SourceValidationState::Stale;
            validation.message = tr("模板定位原点模式已改变");
            return validation;
        }
        if (expected.value(QStringLiteral("originMode")).toString()
                == QStringLiteral("custom")
                && !identityPayload.value(QStringLiteral("customOriginNormalized"))
                   .toObject().isEmpty()
                && CalibrationSourceFingerprint::canonicalJson(
                    identityPayload.value(QStringLiteral("customOriginNormalized")))
                != CalibrationSourceFingerprint::canonicalJson(
                    expected.value(QStringLiteral("customOriginNormalized")))) {
            validation.state = SourceValidationState::Stale;
            validation.message = tr("模板定位自定义原点已改变");
            return validation;
        }
        if (expected.contains(QStringLiteral("referenceImageSignature"))
                && identityPayload.value(QStringLiteral(
                                             "coordinateSourceReferenceSignature"))
                   .toString()
                   != expected.value(QStringLiteral("referenceImageSignature"))
                      .toString()) {
            validation.state = SourceValidationState::Stale;
            validation.message = tr("当前基准图与标定时不一致");
            return validation;
        }
        validation.state = SourceValidationState::Unverifiable;
        validation.message = tr("当前模板尚无有效预览结果，无法核对模板模型签名");
        return validation;
    }

    const QJsonObject actual = CalibrationSourceFingerprint::makeFingerprint(
                effectiveProducerId,
                ToolType::TemplateLocation,
                identityPayload);
    QString mismatchField;
    if (!CalibrationSourceFingerprint::matches(expected, actual, &mismatchField)) {
        validation.state = SourceValidationState::Stale;
        validation.message = tr("%1已改变").arg(fingerprintFieldText(mismatchField));
        return validation;
    }

    if (!expected.contains(QStringLiteral("referenceImageSignature"))) {
        validation.state = SourceValidationState::Unverifiable;
        validation.message = tr("兼容旧版来源签名未记录基准图内容；"
                                "模板原点与配置一致，但基准图无法完整核对");
        return validation;
    }

    validation.state = SourceValidationState::Verified;
    validation.message = tr("当前来源、模板原点及配置与标定文件一致");
    return validation;
}

void CalibrationTransformDialog::showSourceValidationStatus(
        const SourceValidationResult &validation)
{
    QString state;
    QString text;
    QString color = QStringLiteral("#edf1f5");
    switch (validation.state) {
    case SourceValidationState::Verified:
        state = QStringLiteral("verified");
        text = tr("标定来源已验证 | %1").arg(validation.message);
        color = QStringLiteral("#20c933");
        break;
    case SourceValidationState::Unverifiable:
        state = QStringLiteral("unverifiable");
        text = tr("来源无法验证，建议重新标定 | %1").arg(validation.message);
        color = QStringLiteral("#ff9d18");
        break;
    case SourceValidationState::Stale:
        state = QStringLiteral("stale");
        text = tr("标定已失效 | %1").arg(validation.message);
        color = QStringLiteral("#ff2020");
        break;
    case SourceValidationState::NotReady:
        state = QStringLiteral("notReady");
        text = validation.message.isEmpty() ? tr("尚未验证标定来源") : validation.message;
        break;
    }
    ui->statusLabel->setProperty("sourceValidationState", state);
    ui->statusLabel->setText(text);
    ui->statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(color));
}

void CalibrationTransformDialog::refreshCalibrationSourceValidation()
{
    m_sourceValidation = evaluateCalibrationSource();
    showSourceValidationStatus(m_sourceValidation);
}

bool CalibrationTransformDialog::validateConfiguration(QString *errorMessage) const
{
    const QString filePath = ui->calibrationFileCombo->currentData().toString().trimmed();
    if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
        if (errorMessage)
            *errorMessage = tr("请选择有效的标定文件");
        return false;
    }
    CalibrationModel probe;
    QString loadError;
    if (!loadCalibrationModel(filePath, &probe, &loadError)) {
        if (errorMessage)
            *errorMessage = loadError.isEmpty() ? tr("标定文件无法读取") : loadError;
        return false;
    }

    const bool angleRequired = probe.mode
            == NPointCalibrationMode::TwelvePointPoseMapping;
    const QJsonObject x = mainInputBinding(ui->inputXSourceCombo);
    const QJsonObject y = mainInputBinding(ui->inputYSourceCombo);
    const QJsonObject angle = mainInputBinding(ui->inputAngleSourceCombo);
    const QString producerId = x.value(QStringLiteral("producerId")).toString().trimmed();
    const bool xyReady = m_mainInputSourceAvailable
            && x.value(QStringLiteral("mode")).toString()
               == QStringLiteral("binding")
            && y.value(QStringLiteral("mode")).toString()
               == QStringLiteral("binding")
            && !producerId.isEmpty()
            && y.value(QStringLiteral("producerId")).toString() == producerId;
    const bool angleReady = !angleRequired
            || (angle.value(QStringLiteral("mode")).toString()
                == QStringLiteral("binding")
                && angle.value(QStringLiteral("producerId")).toString()
                   == producerId);
    if (!xyReady || !angleReady) {
        if (errorMessage) {
            *errorMessage = angleRequired
                    ? tr("当前姿态映射文件要求订阅同一前序节点的 X/Y/Angle 图像坐标")
                    : tr("请先订阅一个可用前序节点的 X/Y 图像坐标");
        }
        return false;
    }
    const SourceValidationResult sourceValidation = evaluateCalibrationSource();
    if (sourceValidation.state == SourceValidationState::Stale) {
        if (errorMessage)
            *errorMessage = tr("标定已失效：%1").arg(sourceValidation.message);
        return false;
    }
    return true;
}

void CalibrationTransformDialog::finishConfiguration()
{
    if (m_importedTestActive) {
        runTest();
        return;
    }
    QString error;
    if (!validateConfiguration(&error)) {
        const SourceValidationResult sourceValidation = evaluateCalibrationSource();
        if (sourceValidation.state == SourceValidationState::Stale) {
            m_sourceValidation = sourceValidation;
            showSourceValidationStatus(sourceValidation);
        } else {
            ui->statusLabel->setProperty("sourceValidationState",
                                         QStringLiteral("invalid"));
            ui->statusLabel->setText(tr("NG | %1").arg(error));
            ui->statusLabel->setStyleSheet(QStringLiteral("color: #ff2020;"));
        }
        return;
    }
    accept();
}

void CalibrationTransformDialog::displayConversionResult(const ToolResult &result)
{
    const QJsonObject payload = result.payload;
    const QString region = payload.value(QStringLiteral("calibrationRegion"))
            .toString().trimmed();
    const QString rotationCoverage = payload.value(QStringLiteral("rotationCoverage"))
            .toString().trimmed();
    const bool coordinateAvailable = payload
            .value(QStringLiteral("coordinateAvailable")).toBool(false);
    const bool productionAllowed = payload
            .value(QStringLiteral("productionAllowed")).toBool(result.ok);
    const bool angleProductionAllowed = payload
            .value(QStringLiteral("angleProductionAllowed")).toBool(false);
    const bool angleValid = payload.value(QStringLiteral("angleValid"))
            .toBool(false);
    const QString angleMappingStatus = payload
            .value(QStringLiteral("angleMappingStatus")).toString();
    const QString rotationModel = payload.value(QStringLiteral("rotationModel"))
            .toString().trimmed();

    QString reason = result.message.trimmed();
    if (reason.isEmpty())
        reason = result.status.trimmed();
    if (reason.isEmpty())
        reason = tr("未知状态");

    const bool boundaryWarning = result.success
            && result.status == QStringLiteral("converted_boundary");
    QString state = boundaryWarning
            ? QStringLiteral("warning")
            : result.ok ? QStringLiteral("ok") : QStringLiteral("ng");
    QStringList lines;
    if (result.success) {
        lines.append(result.ok
                     ? tr("标定转换输出为OK")
                     : tr("标定转换输出为NG（数学转换成功，生产门禁阻止）"));
        if (coordinateAvailable) {
            QString coordinateLine = tr("转换坐标X：%1，转换坐标Y：%2")
                    .arg(payloadNumber(payload, QStringLiteral("machineX"), 2))
                    .arg(payloadNumber(payload, QStringLiteral("machineY"), 2));
            if (angleValid) {
                coordinateLine += tr("，机械角度：%1°")
                        .arg(payloadNumber(payload,
                                           QStringLiteral("machineAngle"), 2));
            }
            coordinateLine += tr("，单像素精度：%1")
                    .arg(payloadNumber(payload, QStringLiteral("pixelAccuracy"), 2));
            lines.append(coordinateLine);
        } else {
            lines.append(tr("未返回可诊断的转换坐标"));
        }
        lines.append(tr("区域：%1，旋转覆盖：%2，生产允许：%3")
                     .arg(calibrationRegionDisplayText(region),
                          rotationCoverageDisplayText(rotationCoverage),
                          productionAllowed ? tr("是") : tr("否")));
        if (rotationModel
                == QStringLiteral("pose_mapping_eccentric_compensation")) {
            lines.append(tr("姿态映射偏心补偿：已使用映射机械角；补偿量：(%1, %2)")
                         .arg(payloadNumber(payload,
                                            QStringLiteral("rotationCorrectionX"), 4))
                         .arg(payloadNumber(payload,
                                            QStringLiteral("rotationCorrectionY"), 4)));
            lines.append(tr("偏心半径：%1；位置拟合 RMSE/Max：%2/%3")
                         .arg(payloadNumber(payload,
                                            QStringLiteral("rotationRadiusMm"), 4))
                         .arg(payloadNumber(payload,
                                            QStringLiteral("rotationFitRmseMm"), 4))
                         .arg(payloadNumber(payload,
                                            QStringLiteral("rotationMaxErrorMm"), 4)));
        } else if (rotationModel == QStringLiteral("pose_mapping_coaxial")) {
            lines.append(tr("姿态映射：已验证共轴，无需位置偏心补偿"));
        } else if (rotationModel == QStringLiteral("axis_trace_diagnostic")) {
            lines.append(tr("轴轨迹仅用于诊断，不执行生产动态补偿"));
        }
        lines.append(tr("角度映射：%1；角度有效：%2；角度生产允许：%3")
                     .arg(angleMappingStatus.isEmpty()
                          ? tr("未配置") : angleMappingStatus,
                          angleValid ? tr("是") : tr("否"),
                          angleProductionAllowed ? tr("是") : tr("否")));
        lines.append(tr("距SafeROI边界：%1 px，距ValidROI边界：%2 px")
                     .arg(payloadNumber(payload,
                                        QStringLiteral("distanceToSafeBoundaryPx"), 2),
                          payloadNumber(payload,
                                        QStringLiteral("distanceToValidBoundaryPx"), 2)));
        if (!result.ok || reason != tr("标定转换成功"))
            lines.append(tr("诊断：%1").arg(reason));
    } else {
        lines.append(tr("标定转换输出为NG"));
        lines.append(tr("区域：%1，生产允许：否")
                     .arg(calibrationRegionDisplayText(region)));
        lines.append(tr("失败原因：%1").arg(reason));
    }

    ui->conversionResultLabel->setText(lines.join(QLatin1Char('\n')));
    ui->conversionResultLabel->setProperty("resultState", state);
    ui->conversionResultLabel->style()->unpolish(ui->conversionResultLabel);
    ui->conversionResultLabel->style()->polish(ui->conversionResultLabel);
    ui->conversionResultLabel->show();
    ui->conversionResultOverlay->raise();
    ui->conversionResultLabel->raise();
}

void CalibrationTransformDialog::runTest()
{
    const ToolConfig config = toolConfig();
    ToolResult result;
    const bool useImportedFrame = m_importedTestActive;
    const cv::Mat referenceFrame =
            ReferenceImageProvider::instance().referenceFrame();
    const cv::Mat frame = useImportedFrame
            ? m_importedTestFrame.clone() : referenceFrame.clone();
    if (!m_sharedToolEngine) {
        result = ToolResult::error(config.toolId, config.toolType,
                                   tr("测试运行引擎不可用"),
                                   QStringLiteral("test_engine_unavailable"));
    } else if (frame.empty()) {
        result = ToolResult::error(config.toolId, config.toolType,
                                   useImportedFrame
                                   ? tr("PC导入图片为空，无法运行同帧订阅链")
                                   : tr("基准图为空，无法运行同帧订阅链"),
                                   QStringLiteral("test_image_missing"));
    } else if (referenceFrame.empty()) {
        result = ToolResult::error(config.toolId, config.toolType,
                                   tr("基准图为空，无法为前序模板定位提供参考图"),
                                   QStringLiteral("test_reference_image_missing"));
    } else {
        QVector<ToolConfig> testChain = m_testToolPrefix;
        testChain.append(config);
        QJsonObject runtimeContext;
        const QString frameId = QStringLiteral("calibration-transform-test-%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        runtimeContext.insert(QStringLiteral("frameId"), frameId);
        runtimeContext.insert(
                    QStringLiteral("input"),
                    FrameInputMetadata::fromMat(
                        frame,
                        useImportedFrame ? QStringLiteral("file")
                                         : QStringLiteral("reference")).toJson());
        runtimeContext.insert(
                    QStringLiteral("referencePositionCorrection"),
                    PositionCorrection::referenceToJson(m_referencePositionCorrection));
        const QVector<ToolResult> results = m_sharedToolEngine->runTools(
                    testChain, frame, referenceFrame, runtimeContext);
        bool found = false;
        for (auto it = results.crbegin(); it != results.crend(); ++it) {
            if (it->toolId != config.toolId)
                continue;
            result = *it;
            found = true;
            break;
        }
        if (!found) {
            result = ToolResult::error(config.toolId, config.toolType,
                                       tr("标定转换未进入测试工具链"),
                                       QStringLiteral("test_result_missing"));
        }
    }
    if (result.success) {
        const QJsonObject payload = result.payload;
        const QString region = payload.value(QStringLiteral("calibrationRegion"))
                .toString().trimmed();
        const bool coordinateAvailable = payload
                .value(QStringLiteral("coordinateAvailable")).toBool(false);
        const bool boundaryWarning =
                result.status == QStringLiteral("converted_boundary");
        ui->statusLabel->setProperty("sourceValidationState",
                                     boundaryWarning
                                     ? QStringLiteral("testWarning")
                                     : result.ok ? QStringLiteral("testOk")
                                                 : QStringLiteral("testNg"));
        ui->statusLabel->setStyleSheet(
                    QStringLiteral("color: %1;")
                    .arg(boundaryWarning ? QStringLiteral("#ff9d18")
                                         : result.ok ? QStringLiteral("#20c933")
                                                     : QStringLiteral("#ff2020")));
        QString coordinateText = tr("无有效转换坐标");
        if (coordinateAvailable) {
            coordinateText = tr("物理 X:%1  Y:%2")
                    .arg(payloadNumber(payload, QStringLiteral("machineX"), 3),
                         payloadNumber(payload, QStringLiteral("machineY"), 3));
            if (payload.value(QStringLiteral("angleValid")).toBool(false)) {
                coordinateText += tr("  机械角度:%1°")
                        .arg(payloadNumber(payload,
                                           QStringLiteral("machineAngle"), 3));
            }
        }
        const QString prefix = useImportedFrame
                ? tr("PC图片 %1 | ").arg(m_importedTestImageTitle)
                : QString();
        ui->statusLabel->setText(
                    tr("%1%2 | %3 | 区域:%4 | %5ms")
                    .arg(prefix,
                         boundaryWarning ? tr("Warning")
                                         : result.ok ? QStringLiteral("OK")
                                                     : QStringLiteral("NG"),
                         coordinateText,
                         calibrationRegionDisplayText(region))
                    .arg(result.elapsedMs));
    } else {
        ui->statusLabel->setProperty("sourceValidationState",
                                     QStringLiteral("testNg"));
        ui->statusLabel->setStyleSheet(QStringLiteral("color: #ff2020;"));
        ui->statusLabel->setText(
                    useImportedFrame
                    ? tr("PC图片 %1 | NG | %2")
                      .arg(m_importedTestImageTitle, result.message)
                    : tr("NG | %1").arg(result.message));
    }
    ui->resultText->setPlainText(QString::fromUtf8(
                                    QJsonDocument(result.toJson()).toJson(QJsonDocument::Indented)));
    displayConversionResult(result);
    if (!useImportedFrame)
        m_snapshot = makeReferenceToolPreviewSnapshot(config, result, QRectF());
}
