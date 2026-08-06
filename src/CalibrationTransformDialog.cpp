#include "CalibrationTransformDialog.h"
#include "ui_CalibrationTransformDialog.h"

#include "PlanDialogUtils.h"
#include "SchemeStore.h"
#include "calibration/CalibrationFileLoader.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "tooladapters/CalibrationTransformAdapter.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QStyle>
#include <QToolButton>
#include <QUuid>

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

QJsonObject bindingItem(const QString &producerId,
                        const QString &outputKey,
                        const QString &displayPath)
{
    return QJsonObject{{QStringLiteral("mode"), QStringLiteral("binding")},
                       {QStringLiteral("producerId"), producerId},
                       {QStringLiteral("outputKey"), outputKey},
                       {QStringLiteral("displayPath"), displayPath}};
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
    for (QLabel *label : {ui->coordinateTypeLabel, ui->inputXLabel, ui->inputYLabel,
                          ui->inputAngleLabel, ui->calXLabel, ui->calYLabel,
                          ui->calJ0Label, ui->calJ1Label, ui->runXLabel,
                          ui->runYLabel, ui->runJ0Label, ui->runJ1Label})
        label->setProperty("role", QStringLiteral("rowField"));
    ui->closeButton->setProperty("actionRole", QStringLiteral("windowClose"));
    ui->importButton->setProperty("actionRole", QStringLiteral("secondary"));
    ui->testButton->setProperty("actionRole", QStringLiteral("secondary"));
    ui->finishButton->setProperty("actionRole", QStringLiteral("highlight"));
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);
    updateReferenceImage(ReferenceImageProvider::instance().referenceImage());
    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this, &CalibrationTransformDialog::updateReferenceImage);
    ui->coordinateTypeCombo->addItem(tr("图像坐标"), QStringLiteral("image"));
    ui->coordinateTypeCombo->addItem(tr("物理坐标"), QStringLiteral("physical"));
    addConstantItem(ui->inputXSourceCombo);
    addConstantItem(ui->inputYSourceCombo);
    addConstantItem(ui->inputAngleSourceCombo);
    ui->inputXSourceCombo->hide();
    ui->inputYSourceCombo->hide();
    ui->inputAngleSourceCombo->hide();
    m_inputXLinkButton = createBindingButton(ui->inputXSourceCombo, ui->inputXSpin,
                                             ui->inputGroup, tr("坐标 X"));
    m_inputYLinkButton = createBindingButton(ui->inputYSourceCombo, ui->inputYSpin,
                                             ui->inputGroup, tr("坐标 Y"));
    m_inputAngleLinkButton = createBindingButton(ui->inputAngleSourceCombo, ui->inputAngleSpin,
                                                 ui->inputGroup, tr("角度"));
    ui->xLayout->addWidget(m_inputXLinkButton);
    ui->yLayout->addWidget(m_inputYLinkButton);
    ui->angleLayout->addWidget(m_inputAngleLinkButton);
    setupPoseSourceUi();
    ui->basicModeButton->setChecked(true);
    ui->poseGroup->setVisible(false);

    connect(ui->closeButton, &QToolButton::clicked, this, &QDialog::reject);
    connect(ui->finishButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->importButton, &QPushButton::clicked,
            this, &CalibrationTransformDialog::importCalibrationFile);
    connect(ui->testButton, &QPushButton::clicked,
            this, &CalibrationTransformDialog::runTest);
    connect(ui->basicModeButton, &QPushButton::clicked, this, [this]() {
        ui->basicModeButton->setChecked(true);
        ui->allModeButton->setChecked(false);
        ui->poseGroup->setVisible(false);
    });
    connect(ui->allModeButton, &QPushButton::clicked, this, [this]() {
        ui->basicModeButton->setChecked(false);
        ui->allModeButton->setChecked(true);
        ui->poseGroup->setVisible(true);
    });
}

void CalibrationTransformDialog::setupPoseSourceUi()
{
    ui->calXLabel->setText(tr("标定位坐标点 X"));
    ui->calYLabel->setText(tr("标定位坐标点 Y"));
    ui->calJ0Label->setText(tr("标定位关节0角度"));
    ui->calJ1Label->setText(tr("标定位关节1角度"));
    ui->runXLabel->setText(tr("运行位坐标点 X"));
    ui->runYLabel->setText(tr("运行位坐标点 Y"));
    ui->runJ0Label->setText(tr("运行位关节0角度"));
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
    ui->poseLayout->addWidget(ui->calJ1Label, 4, 0);
    ui->poseLayout->addWidget(ui->runPoseEnabled, 5, 0, 1, 2);
    ui->poseLayout->addWidget(ui->runXLabel, 6, 0);
    ui->poseLayout->addWidget(ui->runYLabel, 7, 0);
    ui->poseLayout->addWidget(ui->runJ0Label, 8, 0);
    ui->poseLayout->addWidget(ui->runJ1Label, 9, 0);

    auto wrapField = [this](QDoubleSpinBox *spin, int row, int column,
                            QComboBox **stateCombo, QToolButton **linkButton,
                            const QString &fieldName) {
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
        ui->poseLayout->addWidget(container, row, column);
    };
    wrapField(ui->calXSpin, 1, 1, &m_calibrationPoseSources[0],
              &m_calibrationPoseLinkButtons[0], ui->calXLabel->text());
    wrapField(ui->calYSpin, 2, 1, &m_calibrationPoseSources[1],
              &m_calibrationPoseLinkButtons[1], ui->calYLabel->text());
    wrapField(ui->calJ0Spin, 3, 1, &m_calibrationPoseSources[2],
              &m_calibrationPoseLinkButtons[2], ui->calJ0Label->text());
    wrapField(ui->calJ1Spin, 4, 1, &m_calibrationPoseSources[3],
              &m_calibrationPoseLinkButtons[3], ui->calJ1Label->text());
    wrapField(ui->runXSpin, 6, 1, &m_runPoseSources[0],
              &m_runPoseLinkButtons[0], ui->runXLabel->text());
    wrapField(ui->runYSpin, 7, 1, &m_runPoseSources[1],
              &m_runPoseLinkButtons[1], ui->runYLabel->text());
    wrapField(ui->runJ0Spin, 8, 1, &m_runPoseSources[2],
              &m_runPoseLinkButtons[2], ui->runJ0Label->text());
    wrapField(ui->runJ1Spin, 9, 1, &m_runPoseSources[3],
              &m_runPoseLinkButtons[3], ui->runJ1Label->text());
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
            if (index == 1)
                menu->addSeparator();
            QAction *action = menu->addAction(index == 0
                                              ? tr("使用自定义值")
                                              : stateCombo->itemText(index));
            action->setCheckable(true);
            action->setChecked(index == stateCombo->currentIndex());
            connect(action, &QAction::triggered, this,
                    [this, stateCombo, valueSpin, button, fieldName, index]() {
                stateCombo->setCurrentIndex(index);
                updateBindingButton(stateCombo, valueSpin, button, fieldName);
            });
        }
        if (stateCombo->count() == 1) {
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

void CalibrationTransformDialog::updateBindingButton(QComboBox *stateCombo,
                                                      QDoubleSpinBox *valueSpin,
                                                      QToolButton *button,
                                                      const QString &fieldName)
{
    const bool bound = stateCombo->currentData().toJsonObject()
            .value(QStringLiteral("mode")).toString() == QStringLiteral("binding");
    button->setProperty("bindingActive", bound);
    button->setToolTip(bound
                       ? tr("%1 已订阅：%2").arg(fieldName, stateCombo->currentText())
                       : tr("为%1选择前序输出").arg(fieldName));
    valueSpin->setEnabled(!bound);
    button->style()->unpolish(button);
    button->style()->polish(button);
}

CalibrationTransformDialog::~CalibrationTransformDialog()
{
    delete ui;
}

void CalibrationTransformDialog::updateReferenceImage(const QImage &image)
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

void CalibrationTransformDialog::setProducerTools(const QVector<ToolConfig> &tools,
                                                  int consumerIndex,
                                                  const QMap<QString, ToolPreviewSnapshot> &snapshots)
{
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
    QJsonObject toolResults;
    addConstantItem(ui->inputXSourceCombo);
    addConstantItem(ui->inputYSourceCombo);
    addConstantItem(ui->inputAngleSourceCombo);
    for (QComboBox *combo : m_calibrationPoseSources)
        addConstantItem(combo);
    for (QComboBox *combo : m_runPoseSources)
        addConstantItem(combo);
    const int limit = qBound(0, consumerIndex, tools.size());
    for (int index = 0; index < limit; ++index) {
        const ToolConfig &tool = tools.at(index);
        if (!tool.enabled || tool.toolId.trimmed().isEmpty()
                || tool.toolType != ToolType::TemplateLocation) {
            continue;
        }
        const QString title = tool.displayName.trimmed().isEmpty()
                ? tr("%1 模板定位").arg(index + 1)
                : tr("%1 %2").arg(index + 1).arg(tool.displayName);
        const ToolPreviewSnapshot preview = snapshots.value(tool.toolId);
        if (preview.valid)
            toolResults.insert(tool.toolId, preview.result.toJson());
        ui->inputXSourceCombo->addItem(title + tr(".运行点X"),
                                      bindingItem(tool.toolId, QStringLiteral("x"), title));
        ui->inputYSourceCombo->addItem(title + tr(".运行点Y"),
                                      bindingItem(tool.toolId, QStringLiteral("y"), title));
        ui->inputAngleSourceCombo->addItem(title + tr(".运行角度"),
                                          bindingItem(tool.toolId, QStringLiteral("angle"), title));
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
    restoreBinding(ui->inputXSourceCombo, savedInputX);
    restoreBinding(ui->inputYSourceCombo, savedInputY);
    restoreBinding(ui->inputAngleSourceCombo, savedInputAngle);
    for (int i = 0; i < 4; ++i) {
        restoreBinding(m_calibrationPoseSources[static_cast<size_t>(i)],
                       savedCalibrationPose[static_cast<size_t>(i)]);
        restoreBinding(m_runPoseSources[static_cast<size_t>(i)],
                       savedRunPose[static_cast<size_t>(i)]);
    }
    m_testRuntimeContext.insert(QStringLiteral("toolResultsById"), toolResults);
}

QJsonObject CalibrationTransformDialog::bindingFor(QComboBox *combo,
                                                   double constantValue) const
{
    const QJsonObject selected = combo->currentData().toJsonObject();
    if (selected.value(QStringLiteral("mode")).toString() == QStringLiteral("binding"))
        return selected;
    return constantBinding(constantValue);
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
    m_initialConfig = config;
    const QJsonObject params = config.params.value(QStringLiteral("calibrationTransform")).toObject();
    ui->coordinateTypeCombo->setCurrentIndex(qMax(0, ui->coordinateTypeCombo->findData(
                                                      params.value(QStringLiteral("coordinateType"))
                                                      .toString(QStringLiteral("image")))));
    const QJsonObject x = params.value(QStringLiteral("inputX")).toObject();
    const QJsonObject y = params.value(QStringLiteral("inputY")).toObject();
    const QJsonObject angle = params.value(QStringLiteral("inputAngle")).toObject();
    ui->inputXSpin->setValue(x.value(QStringLiteral("value")).toDouble());
    ui->inputYSpin->setValue(y.value(QStringLiteral("value")).toDouble());
    ui->inputAngleSpin->setValue(angle.value(QStringLiteral("value")).toDouble());
    restoreBinding(ui->inputXSourceCombo, x);
    restoreBinding(ui->inputYSourceCombo, y);
    restoreBinding(ui->inputAngleSourceCombo, angle);

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
    if (config.toolId.trimmed().isEmpty())
        config.toolId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    config.toolType = ToolType::CalibrationTransform;
    config.category = ToolCategory::Location;
    config.toolName = QStringLiteral("CalibrationTransform");
    if (config.displayName.trimmed().isEmpty())
        config.displayName = tr("标定转换");
    QJsonArray files;
    for (const QString &path : m_calibrationFiles)
        files.append(path);
    QJsonObject params{
        {QStringLiteral("version"), 1},
        {QStringLiteral("coordinateType"), ui->coordinateTypeCombo->currentData().toString()},
        {QStringLiteral("inputX"), bindingFor(ui->inputXSourceCombo, ui->inputXSpin->value())},
        {QStringLiteral("inputY"), bindingFor(ui->inputYSourceCombo, ui->inputYSpin->value())},
        {QStringLiteral("inputAngle"), bindingFor(ui->inputAngleSourceCombo, ui->inputAngleSpin->value())},
        {QStringLiteral("calibrationFiles"), files},
        {QStringLiteral("activeCalibrationFile"), ui->calibrationFileCombo->currentData().toString()},
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
    ui->calibrationFileCombo->clear();
    for (const QString &path : paths)
        ui->calibrationFileCombo->addItem(QFileInfo(path).fileName(), path);
    const int activeIndex = ui->calibrationFileCombo->findData(activePath);
    if (activeIndex >= 0)
        ui->calibrationFileCombo->setCurrentIndex(activeIndex);
}

void CalibrationTransformDialog::mergeSchemeCalibrationFiles()
{
    const QString schemeDir = SchemeStore::instance().currentScheme().schemeDir;
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

QString CalibrationTransformDialog::importIntoScheme(const QString &sourcePath,
                                                     QString *errorMessage)
{
    const QString schemeDir = SchemeStore::instance().currentScheme().schemeDir;
    QDir assetDir(QDir(schemeDir).filePath(QStringLiteral("calibrations")));
    if (!assetDir.exists() && !assetDir.mkpath(QStringLiteral("."))) {
        if (errorMessage)
            *errorMessage = tr("无法创建方案标定资产目录");
        return QString();
    }
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = tr("无法读取标定文件");
        return QString();
    }
    const QByteArray hash = QCryptographicHash::hash(source.readAll(),
                                                     QCryptographicHash::Sha256).toHex().left(12);
    const QFileInfo info(sourcePath);
    const QString target = assetDir.filePath(QStringLiteral("%1_%2.%3")
                                             .arg(info.completeBaseName(),
                                                  QString::fromLatin1(hash),
                                                  info.suffix()));
    if (QFileInfo::exists(target))
        return target;
    if (!QFile::copy(sourcePath, target)) {
        if (errorMessage)
            *errorMessage = tr("无法复制标定文件到当前方案");
        return QString();
    }
    return target;
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
    const QString imported = importIntoScheme(selected, &error);
    if (imported.isEmpty()) {
        QMessageBox::warning(this, tr("导入失败"), error);
        return;
    }
    if (!m_calibrationFiles.contains(imported))
        m_calibrationFiles.append(imported);
    refreshFileList(m_calibrationFiles, imported);
    ui->statusLabel->setText(tr("已导入 %1").arg(QFileInfo(imported).fileName()));
}

void CalibrationTransformDialog::runTest()
{
    CalibrationTransformAdapter adapter;
    ToolRequest request;
    request.config = toolConfig();
    request.runtimeContext = m_testRuntimeContext;
    const ToolResult result = adapter.run(request);
    ui->statusLabel->setText(result.ok ? tr("OK | 转换成功")
                                      : tr("NG | %1").arg(result.message));
    ui->resultText->setPlainText(QString::fromUtf8(
                                    QJsonDocument(result.toJson()).toJson(QJsonDocument::Indented)));
    m_snapshot = makeReferenceToolPreviewSnapshot(request.config, result, QRectF());
}
