#include "calibration/NPointCalibrationConfigWidget.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QComboBox>
#include <QButtonGroup>
#include <QDateTime>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegExp>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTableWidget>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace {

const QString kImagePointX = QStringLiteral("imagePointX");
const QString kImagePointY = QStringLiteral("imagePointY");
const QString kImageAngle = QStringLiteral("imageAngle");
const QString kPhysicalPointX = QStringLiteral("physicalPointX");
const QString kPhysicalPointY = QStringLiteral("physicalPointY");
const QString kPhysicalAngle = QStringLiteral("physicalAngle");

QTableWidgetItem *numberItem(double value)
{
    QTableWidgetItem *item = new QTableWidgetItem(QString::number(value, 'f', 6));
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

void fillZeroPointTable(QTableWidget *table, int count)
{
    if (!table)
        return;
    table->setRowCount(count);
    for (int row = 0; row < count; ++row) {
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        table->setItem(row, 0, indexItem);
        for (int column = 1; column < 7; ++column)
            table->setItem(row, column, numberItem(0.0));
    }
    if (count > 0)
        table->selectRow(0);
}

QLabel *fieldLabel(const QString &text, const QString &toolTip, QWidget *parent)
{
    QLabel *label = new QLabel(text, parent);
    label->setProperty("role", QStringLiteral("rowField"));
    if (!toolTip.isEmpty()) {
        label->setText(text + QStringLiteral("  ⓘ"));
        label->setToolTip(toolTip);
    }
    return label;
}

QDoubleSpinBox *coordinateSpinBox(const QString &objectName,
                                  double value,
                                  QWidget *parent)
{
    QDoubleSpinBox *spin = new QDoubleSpinBox(parent);
    spin->setObjectName(objectName);
    spin->setDecimals(2);
    spin->setRange(-999999999.0, 999999999.0);
    spin->setValue(value);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    return spin;
}

} // namespace

void NPointCalibrationConfigWidget::setParameterMode(bool showAll)
{
    m_basicButton->setChecked(!showAll);
    m_allButton->setChecked(showAll);
    m_physicalCoordinateCard->setVisible(showAll);
    m_runtimeParametersCard->setVisible(showAll);
    m_qualityCard->setVisible(showAll);
}

void NPointCalibrationConfigWidget::editPoints()
{
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("CalibrationPointEditorDialog"));
    dialog.setWindowTitle(tr("编辑标定点"));
    dialog.setMinimumSize(1120, 560);
    QVBoxLayout *root = new QVBoxLayout(&dialog);
    QHBoxLayout *toolbar = new QHBoxLayout;
    QPushButton *importButton = new QPushButton(tr("导入"), &dialog);
    QPushButton *exportButton = new QPushButton(tr("导出"), &dialog);
    QPushButton *resetButton = new QPushButton(tr("恢复9点"), &dialog);
    QPushButton *clearButton = new QPushButton(tr("清空"), &dialog);
    for (QPushButton *button : {importButton, exportButton, resetButton, clearButton})
        button->setProperty("actionRole", QStringLiteral("secondary"));
    toolbar->addWidget(importButton);
    toolbar->addWidget(exportButton);
    toolbar->addWidget(resetButton);
    toolbar->addWidget(clearButton);
    toolbar->addStretch();
    root->addLayout(toolbar);

    QTableWidget *editor = new QTableWidget(m_sampleTable->rowCount(), 7, &dialog);
    editor->setHorizontalHeaderLabels({tr("序号"), tr("Column(px)"), tr("Row(px)"),
                                       tr("图像角度"), tr("X(mm)"), tr("Y(mm)"), tr("物理角度")});
    editor->verticalHeader()->setVisible(false);
    editor->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    for (int row = 0; row < m_sampleTable->rowCount(); ++row) {
        for (int column = 0; column < m_sampleTable->columnCount(); ++column) {
            const QTableWidgetItem *source = m_sampleTable->item(row, column);
            editor->setItem(row, column, new QTableWidgetItem(source ? source->text() : QString()));
        }
    }
    root->addWidget(editor, 1);
    QHBoxLayout *actions = new QHBoxLayout;
    actions->addStretch();
    QPushButton *cancel = new QPushButton(tr("取消"), &dialog);
    QPushButton *confirm = new QPushButton(tr("确定"), &dialog);
    cancel->setProperty("actionRole", QStringLiteral("secondary"));
    confirm->setProperty("actionRole", QStringLiteral("highlight"));
    actions->addWidget(cancel);
    actions->addWidget(confirm);
    root->addLayout(actions);

    connect(importButton, &QPushButton::clicked, this, [this, editor]() {
        importPoints();
        editor->setRowCount(m_sampleTable->rowCount());
        for (int row = 0; row < m_sampleTable->rowCount(); ++row) {
            for (int column = 0; column < m_sampleTable->columnCount(); ++column) {
                const QTableWidgetItem *source = m_sampleTable->item(row, column);
                editor->setItem(row, column,
                                new QTableWidgetItem(source ? source->text() : QString()));
            }
        }
    });
    connect(exportButton, &QPushButton::clicked, this, [this]() { exportPoints(); });
    connect(resetButton, &QPushButton::clicked, this, [editor]() {
        fillZeroPointTable(editor, 9);
    });
    connect(clearButton, &QPushButton::clicked, editor, [editor]() {
        fillZeroPointTable(editor, editor->rowCount());
    });
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(confirm, &QPushButton::clicked, &dialog, &QDialog::accept);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QSignalBlocker blocker(m_translationCount);
    m_translationCount->setValue(editor->rowCount());
    m_sampleTable->setRowCount(editor->rowCount());
    for (int row = 0; row < editor->rowCount(); ++row) {
        for (int column = 0; column < editor->columnCount(); ++column) {
            const QTableWidgetItem *source = editor->item(row, column);
            m_sampleTable->setItem(row, column,
                                   new QTableWidgetItem(source ? source->text() : QString()));
        }
    }
    m_nextCaptureRow = 0;
    if (m_sampleTable->rowCount() > 0)
        m_sampleTable->selectRow(0);
    m_sampleStatus->setText(tr("已编辑 %1 组对应点").arg(editor->rowCount()));
    emit sampleStateChanged(m_sampleStatus->text(), true);
}

NPointCalibrationConfigWidget::NPointCalibrationConfigWidget(QWidget *parent)
    : CalibrationMethodConfigWidget(parent)
{
    setObjectName(QStringLiteral("NPointCalibrationConfigWidget"));
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QHBoxLayout *heading = new QHBoxLayout;
    QLabel *title = new QLabel(tr("N点标定"), this);
    title->setProperty("role", QStringLiteral("editorTitle"));
    heading->addWidget(title);
    heading->addStretch();
    QFrame *segment = new QFrame(this);
    segment->setObjectName(QStringLiteral("segmentFrame"));
    QHBoxLayout *segmentLayout = new QHBoxLayout(segment);
    segmentLayout->setContentsMargins(2, 2, 2, 2);
    segmentLayout->setSpacing(0);
    m_basicButton = new QPushButton(tr("基础"), segment);
    m_allButton = new QPushButton(tr("全部"), segment);
    m_basicButton->setObjectName(QStringLiteral("nPointBasicModeButton"));
    m_allButton->setObjectName(QStringLiteral("nPointAllModeButton"));
    m_basicButton->setCheckable(true);
    m_allButton->setCheckable(true);
    QButtonGroup *modeGroup = new QButtonGroup(segment);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_basicButton);
    modeGroup->addButton(m_allButton);
    segmentLayout->addWidget(m_basicButton);
    segmentLayout->addWidget(m_allButton);
    heading->addWidget(segment);
    layout->addLayout(heading);

    QFrame *movementCard = new QFrame(this);
    movementCard->setObjectName(QStringLiteral("nPointMovementCard"));
    movementCard->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *movementLayout = new QVBoxLayout(movementCard);
    QLabel *movementTitle = new QLabel(tr("机械臂移动参数"), movementCard);
    movementTitle->setProperty("role", QStringLiteral("cardTitle"));
    movementLayout->addWidget(movementTitle);
    QLabel *movementHint = new QLabel(tr("机械臂平移路径须覆盖有效标定区域"), movementCard);
    movementHint->setProperty("role", QStringLiteral("cardHint"));
    movementLayout->addWidget(movementHint);
    QFormLayout *movementForm = new QFormLayout;
    m_imageProducer = new QComboBox(this);
    m_imageProducer->setObjectName(QStringLiteral("nPointImageProducerCombo"));
    m_imageProducer->addItem(tr("手动输入图像坐标"), QJsonObject{
                                 {QStringLiteral("mode"), QStringLiteral("manual")}});
    m_imageProducer->hide();
    m_translationCount = new QSpinBox(this);
    m_translationCount->setObjectName(QStringLiteral("nPointTranslationCount"));
    m_translationCount->setRange(3, 99);
    m_translationCount->setValue(9);
    m_rotationCount = new QSpinBox(this);
    m_rotationCount->setObjectName(QStringLiteral("nPointRotationCount"));
    m_rotationCount->setRange(0, 0);
    m_rotationCount->setValue(0);
    m_rotationCount->setToolTip(tr("N点方式首期只求二维平移仿射模型，旋转采样由平移旋转标定方式接入"));
    QLabel *translationLabel = new QLabel(tr("平移次数"), movementCard);
    QLabel *rotationLabel = new QLabel(tr("旋转次数"), movementCard);
    translationLabel->setProperty("role", QStringLiteral("rowField"));
    rotationLabel->setProperty("role", QStringLiteral("rowField"));
    movementForm->addRow(translationLabel, m_translationCount);
    movementForm->addRow(rotationLabel, m_rotationCount);
    QPushButton *editButton = new QPushButton(tr("编辑"), movementCard);
    editButton->setObjectName(QStringLiteral("nPointEditPointsButton"));
    editButton->setProperty("actionRole", QStringLiteral("secondary"));
    QLabel *editLabel = new QLabel(tr("编辑标定点"), movementCard);
    editLabel->setProperty("role", QStringLiteral("rowField"));
    movementForm->addRow(editLabel, editButton);
    movementLayout->addLayout(movementForm);
    layout->addWidget(movementCard);

    QFrame *captureCard = new QFrame(this);
    captureCard->setObjectName(QStringLiteral("nPointCalibrationParametersCard"));
    captureCard->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *captureLayout = new QVBoxLayout(captureCard);
    QLabel *captureTitle = new QLabel(tr("标定参数"), captureCard);
    captureTitle->setProperty("role", QStringLiteral("cardTitle"));
    captureLayout->addWidget(captureTitle);
    QFormLayout *captureForm = new QFormLayout;
    captureForm->setHorizontalSpacing(12);
    captureForm->setVerticalSpacing(10);

    QFrame *captureModeFrame = new QFrame(captureCard);
    captureModeFrame->setObjectName(QStringLiteral("nPointCaptureModeFrame"));
    QHBoxLayout *captureModeLayout = new QHBoxLayout(captureModeFrame);
    captureModeLayout->setContentsMargins(0, 0, 0, 0);
    captureModeLayout->setSpacing(0);
    m_triggerCaptureButton = new QPushButton(tr("触发获取"), captureModeFrame);
    m_manualCaptureButton = new QPushButton(tr("手动输入"), captureModeFrame);
    m_triggerCaptureButton->setObjectName(QStringLiteral("nPointCaptureTriggerButton"));
    m_manualCaptureButton->setObjectName(QStringLiteral("nPointCaptureManualButton"));
    m_triggerCaptureButton->setToolTip(
                tr("选择通信或订阅触发采样模式；通用调度接口将在后续接入"));
    m_manualCaptureButton->setToolTip(
                tr("选择手动录入模式；坐标请通过“编辑标定点”维护"));
    m_triggerCaptureButton->setCheckable(true);
    m_manualCaptureButton->setCheckable(true);
    QButtonGroup *captureModeGroup = new QButtonGroup(captureModeFrame);
    captureModeGroup->setExclusive(true);
    captureModeGroup->addButton(m_triggerCaptureButton);
    captureModeGroup->addButton(m_manualCaptureButton);
    captureModeLayout->addWidget(m_triggerCaptureButton);
    captureModeLayout->addWidget(m_manualCaptureButton);
    m_triggerCaptureButton->setChecked(true);
    captureForm->addRow(fieldLabel(tr("标定点获取"), QString(), captureCard),
                        captureModeFrame);

    auto addBindingRow = [this, captureCard, captureForm](const QString &fieldKey,
                                                          const QString &fieldName,
                                                          const QString &objectName,
                                                          bool imageGroup) {
        QWidget *field = new QWidget(captureCard);
        QHBoxLayout *fieldLayout = new QHBoxLayout(field);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setSpacing(4);
        QLineEdit *display = new QLineEdit(field);
        display->setObjectName(objectName);
        display->setReadOnly(true);
        display->setPlaceholderText(tr("未绑定"));
        display->setProperty("bindingField", true);
        QToolButton *link = new QToolButton(field);
        link->setObjectName(objectName + QStringLiteral("Button"));
        link->setIcon(QIcon(QStringLiteral(":/icons/external-link.svg")));
        link->setIconSize(QSize(22, 22));
        link->setPopupMode(QToolButton::InstantPopup);
        link->setProperty("actionRole", QStringLiteral("bindingLink"));
        link->setProperty("bindingFieldKey", fieldKey);
        QMenu *menu = new QMenu(link);
        link->setMenu(menu);
        connect(menu, &QMenu::aboutToShow, this,
                [this, menu, imageGroup, fieldKey]() {
            menu->clear();
            QAction *manual = menu->addAction(tr("使用手动输入"));
            connect(manual, &QAction::triggered, this,
                    [this, imageGroup, fieldKey]() {
                if (imageGroup)
                    applyImageProducerBinding(fieldKey, -1);
                else
                    applyPhysicalCommunicationBinding(fieldKey, false);
            });
            menu->addSeparator();
            if (imageGroup) {
                QString outputName;
                if (fieldKey == kImagePointX)
                    outputName = tr("匹配点X");
                else if (fieldKey == kImagePointY)
                    outputName = tr("匹配点Y");
                else
                    outputName = tr("运行角度");
                const QJsonObject selectedBinding =
                        m_captureBindingValues.value(fieldKey);
                for (int index = 0; index < m_snapshots.size(); ++index) {
                    const CalibrationProducerSnapshot &snapshot = m_snapshots.at(index);
                    QAction *source = menu->addAction(
                                tr("%1 · %2").arg(snapshot.displayName, outputName));
                    source->setCheckable(true);
                    source->setChecked(
                                selectedBinding.value(QStringLiteral("mode")).toString()
                                == QStringLiteral("binding")
                                && selectedBinding.value(QStringLiteral("producerId")).toString()
                                == snapshot.producerId);
                    connect(source, &QAction::triggered, this,
                            [this, fieldKey, index]() {
                        applyImageProducerBinding(fieldKey, index);
                    });
                }
                if (m_snapshots.isEmpty()) {
                    QAction *empty = menu->addAction(tr("无可用前序图像坐标"));
                    empty->setEnabled(false);
                }
            } else {
                QAction *communication = menu->addAction(
                            tr("通信解析 X / Y / Angle"));
                communication->setCheckable(true);
                communication->setChecked(
                            m_captureBindingValues.value(fieldKey)
                            .value(QStringLiteral("mode")).toString()
                            == QStringLiteral("communication"));
                connect(communication, &QAction::triggered, this,
                        [this, fieldKey]() {
                    applyPhysicalCommunicationBinding(fieldKey, true);
                });
            }
            menu->addSeparator();
            QAction *reserved = menu->addAction(tr("其他订阅接口待接入"));
            reserved->setEnabled(false);
        });
        fieldLayout->addWidget(display, 1);
        fieldLayout->addWidget(link);
        m_captureBindingEdits.insert(fieldKey, display);
        m_captureBindingButtons.insert(fieldKey, link);
        m_captureBindingValues.insert(fieldKey, QJsonObject{
            {QStringLiteral("mode"), QStringLiteral("unbound")}
        });
        captureForm->addRow(fieldLabel(fieldName,
                                       tr("该字段预留前序输出或通信订阅接口，本轮不执行通用订阅解析"),
                                       captureCard),
                            field);
        updateCaptureBindingUi(fieldKey);
    };
    addBindingRow(kImagePointX, tr("图像点X"),
                  QStringLiteral("nPointImageXBinding"), true);
    addBindingRow(kImagePointY, tr("图像点Y"),
                  QStringLiteral("nPointImageYBinding"), true);
    addBindingRow(kImageAngle, tr("图像角度"),
                  QStringLiteral("nPointImageAngleBinding"), true);
    addBindingRow(kPhysicalPointX, tr("物理点X"),
                  QStringLiteral("nPointPhysicalXBinding"), false);
    addBindingRow(kPhysicalPointY, tr("物理点Y"),
                  QStringLiteral("nPointPhysicalYBinding"), false);
    addBindingRow(kPhysicalAngle, tr("物理角度"),
                  QStringLiteral("nPointPhysicalAngleBinding"), false);

    m_sampleStatus = new QLabel(
                tr("等待采样；无设备模式可通过“编辑标定点”手动录入"), captureCard);
    m_sampleStatus->setProperty("role", QStringLiteral("cardHint"));
    m_sampleStatus->setWordWrap(true);
    captureLayout->addLayout(captureForm);
    captureLayout->addWidget(m_sampleStatus);
    layout->addWidget(captureCard);

    m_sampleTable = new QTableWidget(9, 7, this);
    m_sampleTable->setObjectName(QStringLiteral("nPointSampleTable"));
    m_sampleTable->setHorizontalHeaderLabels({tr("序号"), tr("Column(px)"), tr("Row(px)"),
                                               tr("图像角度"), tr("X(mm)"), tr("Y(mm)"), tr("物理角度")});
    m_sampleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_sampleTable->verticalHeader()->setVisible(false);
    m_sampleTable->hide();

    m_physicalCoordinateCard = new QFrame(this);
    m_physicalCoordinateCard->setObjectName(QStringLiteral("nPointPhysicalCoordinateCard"));
    m_physicalCoordinateCard->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *physicalLayout = new QVBoxLayout(m_physicalCoordinateCard);
    QLabel *physicalTitle = new QLabel(tr("物理坐标系参数"), m_physicalCoordinateCard);
    physicalTitle->setProperty("role", QStringLiteral("cardTitle"));
    physicalLayout->addWidget(physicalTitle);
    QFormLayout *physicalForm = new QFormLayout;
    m_referenceX = coordinateSpinBox(QStringLiteral("nPointReferenceX"), 0.0,
                                     m_physicalCoordinateCard);
    m_referenceY = coordinateSpinBox(QStringLiteral("nPointReferenceY"), 0.0,
                                     m_physicalCoordinateCard);
    m_offsetX = coordinateSpinBox(QStringLiteral("nPointOffsetX"), 1.0,
                                  m_physicalCoordinateCard);
    m_offsetY = coordinateSpinBox(QStringLiteral("nPointOffsetY"), 1.0,
                                  m_physicalCoordinateCard);
    m_movePriority = new QComboBox(m_physicalCoordinateCard);
    m_movePriority->setObjectName(QStringLiteral("nPointMovePriority"));
    m_movePriority->addItem(tr("X优先"), QStringLiteral("x_first"));
    m_movePriority->addItem(tr("Y优先"), QStringLiteral("y_first"));
    m_directionChangeCount = new QSpinBox(m_physicalCoordinateCard);
    m_directionChangeCount->setObjectName(QStringLiteral("nPointDirectionChangeCount"));
    m_directionChangeCount->setRange(0, 99);
    m_directionChangeCount->setValue(3);
    m_referenceAngle = coordinateSpinBox(QStringLiteral("nPointReferenceAngle"), 0.0,
                                         m_physicalCoordinateCard);
    m_angleOffset = coordinateSpinBox(QStringLiteral("nPointAngleOffset"), 1.0,
                                      m_physicalCoordinateCard);
    m_calibrationOrigin = new QSpinBox(m_physicalCoordinateCard);
    m_calibrationOrigin->setObjectName(QStringLiteral("nPointCalibrationOrigin"));
    m_calibrationOrigin->setRange(1, 99);
    m_calibrationOrigin->setValue(4);
    physicalForm->addRow(fieldLabel(tr("基准点X"), QString(), m_physicalCoordinateCard), m_referenceX);
    physicalForm->addRow(fieldLabel(tr("基准点Y"), QString(), m_physicalCoordinateCard), m_referenceY);
    physicalForm->addRow(fieldLabel(tr("偏移X"), QString(), m_physicalCoordinateCard), m_offsetX);
    physicalForm->addRow(fieldLabel(tr("偏移Y"), QString(), m_physicalCoordinateCard), m_offsetY);
    physicalForm->addRow(fieldLabel(tr("移动优先"), QString(), m_physicalCoordinateCard), m_movePriority);
    physicalForm->addRow(fieldLabel(tr("换向移动次数"), QString(), m_physicalCoordinateCard),
                         m_directionChangeCount);
    physicalForm->addRow(fieldLabel(tr("基准角度"), QString(), m_physicalCoordinateCard),
                         m_referenceAngle);
    physicalForm->addRow(fieldLabel(tr("角度偏移"), QString(), m_physicalCoordinateCard), m_angleOffset);
    physicalForm->addRow(fieldLabel(tr("标定原点"), QString(), m_physicalCoordinateCard),
                         m_calibrationOrigin);
    physicalLayout->addLayout(physicalForm);
    layout->addWidget(m_physicalCoordinateCard);

    m_runtimeParametersCard = new QFrame(this);
    m_runtimeParametersCard->setObjectName(QStringLiteral("nPointRuntimeParametersCard"));
    m_runtimeParametersCard->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *runtimeLayout = new QVBoxLayout(m_runtimeParametersCard);
    QLabel *runtimeTitle = new QLabel(tr("运行参数"), m_runtimeParametersCard);
    runtimeTitle->setProperty("role", QStringLiteral("cardTitle"));
    runtimeLayout->addWidget(runtimeTitle);
    QFormLayout *runtimeForm = new QFormLayout;
    m_cameraMotionMode = new QComboBox(m_runtimeParametersCard);
    m_cameraMotionMode->setObjectName(QStringLiteral("nPointCameraMotionMode"));
    m_cameraMotionMode->addItem(tr("相机运动"), QStringLiteral("camera_motion"));
    m_degreesOfFreedom = new QComboBox(m_runtimeParametersCard);
    m_degreesOfFreedom->setObjectName(QStringLiteral("nPointDegreesOfFreedom"));
    m_degreesOfFreedom->addItem(tr("缩放、旋转、纵横比、倾斜"),
                                QStringLiteral("scale_rotation_aspect_tilt"));
    m_weightFunction = new QComboBox(m_runtimeParametersCard);
    m_weightFunction->setObjectName(QStringLiteral("nPointWeightFunction"));
    m_weightFunction->addItem(QStringLiteral("Tukey"), QStringLiteral("tukey"));
    m_weightCoefficient = new QSpinBox(m_runtimeParametersCard);
    m_weightCoefficient->setObjectName(QStringLiteral("nPointWeightCoefficient"));
    m_weightCoefficient->setRange(0, 100000);
    m_weightCoefficient->setValue(20);
    runtimeForm->addRow(fieldLabel(tr("相机模式"), QString(), m_runtimeParametersCard),
                        m_cameraMotionMode);
    runtimeForm->addRow(fieldLabel(tr("自由度"), QString(), m_runtimeParametersCard),
                        m_degreesOfFreedom);
    runtimeForm->addRow(fieldLabel(tr("权重函数"), QString(), m_runtimeParametersCard),
                        m_weightFunction);
    runtimeForm->addRow(fieldLabel(tr("权重系数"), QString(), m_runtimeParametersCard),
                        m_weightCoefficient);
    runtimeLayout->addLayout(runtimeForm);
    layout->addWidget(m_runtimeParametersCard);

    m_qualityCard = new QFrame(this);
    m_qualityCard->setObjectName(QStringLiteral("nPointQualityCard"));
    m_qualityCard->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *advancedLayout = new QVBoxLayout(m_qualityCard);
    QLabel *advancedTitle = new QLabel(tr("质量门限"), m_qualityCard);
    advancedTitle->setProperty("role", QStringLiteral("cardTitle"));
    advancedLayout->addWidget(advancedTitle);
    QFormLayout *thresholds = new QFormLayout;
    m_rmseLimit = new QDoubleSpinBox(m_qualityCard);
    m_rmseLimit->setObjectName(QStringLiteral("nPointRmseLimit"));
    m_rmseLimit->setDecimals(4);
    m_rmseLimit->setRange(0.0001, 1000.0);
    m_rmseLimit->setValue(0.10);
    m_maxErrorLimit = new QDoubleSpinBox(m_qualityCard);
    m_maxErrorLimit->setObjectName(QStringLiteral("nPointMaxErrorLimit"));
    m_maxErrorLimit->setDecimals(4);
    m_maxErrorLimit->setRange(0.0001, 1000.0);
    m_maxErrorLimit->setValue(0.25);
    QLabel *rmseLabel = new QLabel(tr("RMSE门限(mm)"), m_qualityCard);
    QLabel *maxLabel = new QLabel(tr("最大误差门限(mm)"), m_qualityCard);
    rmseLabel->setProperty("role", QStringLiteral("rowField"));
    maxLabel->setProperty("role", QStringLiteral("rowField"));
    thresholds->addRow(rmseLabel, m_rmseLimit);
    thresholds->addRow(maxLabel, m_maxErrorLimit);
    advancedLayout->addLayout(thresholds);
    layout->addWidget(m_qualityCard);
    layout->addStretch();

    connect(editButton, &QPushButton::clicked, this, [this]() { editPoints(); });
    connect(m_basicButton, &QPushButton::clicked, this, [this]() { setParameterMode(false); });
    connect(m_allButton, &QPushButton::clicked, this, [this]() { setParameterMode(true); });
    connect(m_translationCount, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int count) { fillDefaultGrid(count); });
    fillDefaultGrid(9);
    setParameterMode(false);
}

void NPointCalibrationConfigWidget::fillDefaultGrid(int count)
{
    fillZeroPointTable(m_sampleTable, count);
    m_nextCaptureRow = 0;
}

QVector<CalibrationSample> NPointCalibrationConfigWidget::samplesFromTable(
        QString *errorMessage) const
{
    QVector<CalibrationSample> samples;
    for (int row = 0; row < m_sampleTable->rowCount(); ++row) {
        CalibrationSample sample;
        sample.index = row + 1;
        double *values[] = {&sample.column, &sample.row, &sample.imageAngleDeg,
                            &sample.machineX, &sample.machineY, &sample.machineAngleDeg};
        for (int column = 1; column <= 6; ++column) {
            bool ok = false;
            const double value = m_sampleTable->item(row, column)
                    ? m_sampleTable->item(row, column)->text().toDouble(&ok) : 0.0;
            if (!ok || !std::isfinite(value)) {
                if (errorMessage)
                    *errorMessage = tr("第%1行第%2列不是有效数字").arg(row + 1).arg(column + 1);
                return {};
            }
            *values[column - 1] = value;
        }
        sample.source = m_imageProducer->currentIndex() > 0
                ? QStringLiteral("binding") : QStringLiteral("manual");
        sample.capturedAt = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        samples.append(sample);
    }
    return samples;
}

CalibrationDraft NPointCalibrationConfigWidget::draft(QString *errorMessage) const
{
    CalibrationDraft result;
    result.samples = samplesFromTable(errorMessage);
    result.parameters.insert(QStringLiteral("rmseLimit"), m_rmseLimit->value());
    result.parameters.insert(QStringLiteral("maxErrorLimit"), m_maxErrorLimit->value());
    result.parameters.insert(QStringLiteral("translationCount"), m_translationCount->value());
    result.parameters.insert(QStringLiteral("rotationCount"), m_rotationCount->value());
    result.parameters.insert(QStringLiteral("imageBinding"),
                             m_imageProducer->currentData().toJsonObject());
    result.parameters.insert(QStringLiteral("captureBindings"), captureBindings());
    result.parameters.insert(QStringLiteral("physicalCoordinateParameters"), QJsonObject{
        {QStringLiteral("referenceX"), m_referenceX->value()},
        {QStringLiteral("referenceY"), m_referenceY->value()},
        {QStringLiteral("offsetX"), m_offsetX->value()},
        {QStringLiteral("offsetY"), m_offsetY->value()},
        {QStringLiteral("movePriority"), m_movePriority->currentData().toString()},
        {QStringLiteral("directionChangeCount"), m_directionChangeCount->value()},
        {QStringLiteral("referenceAngle"), m_referenceAngle->value()},
        {QStringLiteral("angleOffset"), m_angleOffset->value()},
        {QStringLiteral("calibrationOrigin"), m_calibrationOrigin->value()}
    });
    result.parameters.insert(QStringLiteral("runtimeParameters"), QJsonObject{
        {QStringLiteral("cameraMotionMode"), m_cameraMotionMode->currentData().toString()},
        {QStringLiteral("degreesOfFreedom"), m_degreesOfFreedom->currentData().toString()},
        {QStringLiteral("weightFunction"), m_weightFunction->currentData().toString()},
        {QStringLiteral("weightCoefficient"), m_weightCoefficient->value()}
    });
    return result;
}

QJsonObject NPointCalibrationConfigWidget::captureBindings() const
{
    QJsonObject bindings;
    bindings.insert(QStringLiteral("version"), 1);
    bindings.insert(QStringLiteral("captureMode"),
                    m_manualCaptureButton && m_manualCaptureButton->isChecked()
                    ? QStringLiteral("manual") : QStringLiteral("trigger"));
    for (auto it = m_captureBindingValues.constBegin();
         it != m_captureBindingValues.constEnd(); ++it) {
        bindings.insert(it.key(), it.value());
    }
    return bindings;
}

void NPointCalibrationConfigWidget::setCaptureBinding(
        const QString &fieldKey, const QJsonObject &binding)
{
    if (!m_captureBindingValues.contains(fieldKey))
        return;
    m_captureBindingValues.insert(fieldKey, binding);
    updateCaptureBindingUi(fieldKey);
    if (fieldKey == kImagePointX || fieldKey == kImagePointY
            || fieldKey == kImageAngle) {
        syncLegacyImageProducer();
    }
}

void NPointCalibrationConfigWidget::updateCaptureBindingUi(const QString &fieldKey)
{
    QLineEdit *display = m_captureBindingEdits.value(fieldKey, nullptr);
    QToolButton *button = m_captureBindingButtons.value(fieldKey, nullptr);
    if (!display || !button)
        return;
    const QJsonObject binding = m_captureBindingValues.value(fieldKey);
    const QString mode = binding.value(QStringLiteral("mode")).toString();
    const QString displayPath = binding.value(QStringLiteral("displayPath")).toString();
    bool sourceAvailable = true;
    const bool imageField = fieldKey == kImagePointX || fieldKey == kImagePointY
            || fieldKey == kImageAngle;
    if (imageField && mode == QStringLiteral("binding")) {
        sourceAvailable = false;
        const QString producerId = binding.value(QStringLiteral("producerId")).toString();
        for (const CalibrationProducerSnapshot &snapshot : m_snapshots) {
            if (snapshot.producerId == producerId) {
                sourceAvailable = true;
                break;
            }
        }
    }
    const bool selected = mode == QStringLiteral("binding")
            || mode == QStringLiteral("communication");
    const bool active = selected && sourceAvailable;
    display->setText(selected && !sourceAvailable
                     ? tr("来源不可用：%1").arg(displayPath) : displayPath);
    display->setProperty("bindingState", active ? QStringLiteral("active")
                                : selected ? QStringLiteral("invalid")
                                           : QStringLiteral("unbound"));
    button->setProperty("bindingActive", active);
    button->setToolTip(active ? tr("已选择来源：%1；通用订阅解析将在后续接入")
                                .arg(displayPath)
                              : selected ? tr("已保存来源当前不可用：%1")
                                           .arg(displayPath)
                                         : tr("选择来源；其他订阅接口将在后续接入"));
    display->style()->unpolish(display);
    display->style()->polish(display);
    button->style()->unpolish(button);
    button->style()->polish(button);
}

void NPointCalibrationConfigWidget::applyImageProducerBinding(
        const QString &fieldKey, int producerIndex)
{
    if (producerIndex < 0 || producerIndex >= m_snapshots.size()) {
        const QJsonObject manual{
            {QStringLiteral("mode"), QStringLiteral("manual")},
            {QStringLiteral("displayPath"), tr("手动输入")}
        };
        setCaptureBinding(fieldKey, manual);
        return;
    }

    const CalibrationProducerSnapshot &snapshot = m_snapshots.at(producerIndex);
    QString outputKey;
    QString outputName;
    if (fieldKey == kImagePointX) {
        outputKey = QStringLiteral("x");
        outputName = tr("匹配点X");
    } else if (fieldKey == kImagePointY) {
        outputKey = QStringLiteral("y");
        outputName = tr("匹配点Y");
    } else if (fieldKey == kImageAngle) {
        outputKey = QStringLiteral("angle");
        outputName = tr("运行角度");
    } else {
        return;
    }
    setCaptureBinding(fieldKey, QJsonObject{
        {QStringLiteral("mode"), QStringLiteral("binding")},
        {QStringLiteral("producerId"), snapshot.producerId},
        {QStringLiteral("outputKey"), outputKey},
        {QStringLiteral("displayPath"), snapshot.displayName
                                           + QStringLiteral(",") + outputName}
    });
}

void NPointCalibrationConfigWidget::applyPhysicalCommunicationBinding(
        const QString &fieldKey, bool enabled)
{
    if (!enabled) {
        const QJsonObject manual{
            {QStringLiteral("mode"), QStringLiteral("manual")},
            {QStringLiteral("displayPath"), tr("手动输入")}
        };
        setCaptureBinding(fieldKey, manual);
        return;
    }

    QString outputKey;
    QString displayPath;
    if (fieldKey == kPhysicalPointX) {
        outputKey = QStringLiteral("x");
        displayPath = tr("通信解析X");
    } else if (fieldKey == kPhysicalPointY) {
        outputKey = QStringLiteral("y");
        displayPath = tr("通信解析Y");
    } else if (fieldKey == kPhysicalAngle) {
        outputKey = QStringLiteral("angle");
        displayPath = tr("通信解析角度");
    } else {
        return;
    }
    setCaptureBinding(fieldKey, QJsonObject{
        {QStringLiteral("mode"), QStringLiteral("communication")},
        {QStringLiteral("sourceId"),
         QStringLiteral("calibration.communication.capture")},
        {QStringLiteral("outputKey"), outputKey},
        {QStringLiteral("displayPath"), displayPath}
    });
}

void NPointCalibrationConfigWidget::syncLegacyImageProducer()
{
    QString producerId;
    for (const QString &fieldKey : {kImagePointX, kImagePointY, kImageAngle}) {
        const QJsonObject binding = m_captureBindingValues.value(fieldKey);
        if (binding.value(QStringLiteral("mode")).toString()
                != QStringLiteral("binding")) {
            m_imageProducer->setCurrentIndex(0);
            return;
        }
        const QString fieldProducerId =
                binding.value(QStringLiteral("producerId")).toString();
        if (producerId.isEmpty())
            producerId = fieldProducerId;
        else if (fieldProducerId != producerId) {
            m_imageProducer->setCurrentIndex(0);
            return;
        }
    }
    for (int index = 0; index < m_snapshots.size(); ++index) {
        if (m_snapshots.at(index).producerId == producerId) {
            m_imageProducer->setCurrentIndex(index + 1);
            return;
        }
    }
    m_imageProducer->setCurrentIndex(0);
}

void NPointCalibrationConfigWidget::setProducerSnapshots(
        const QVector<CalibrationProducerSnapshot> &snapshots)
{
    const QString previousProducerId = m_imageProducer->currentData().toJsonObject()
            .value(QStringLiteral("producerId")).toString();
    m_snapshots = snapshots;
    while (m_imageProducer->count() > 1)
        m_imageProducer->removeItem(1);
    for (const CalibrationProducerSnapshot &snapshot : snapshots) {
        m_imageProducer->addItem(snapshot.displayName, QJsonObject{
            {QStringLiteral("mode"), QStringLiteral("binding")},
            {QStringLiteral("producerId"), snapshot.producerId},
            {QStringLiteral("xOutputKey"), QStringLiteral("x")},
            {QStringLiteral("yOutputKey"), QStringLiteral("y")},
            {QStringLiteral("angleOutputKey"), QStringLiteral("angle")}
        });
    }
    bool hasConfiguredImageBinding = false;
    for (const QString &fieldKey : {kImagePointX, kImagePointY, kImageAngle}) {
        hasConfiguredImageBinding = hasConfiguredImageBinding
                || m_captureBindingValues.value(fieldKey)
                   .value(QStringLiteral("mode")).toString()
                   == QStringLiteral("binding");
        updateCaptureBindingUi(fieldKey);
    }
    syncLegacyImageProducer();
    if (!hasConfiguredImageBinding && !previousProducerId.isEmpty()) {
        for (int index = 0; index < snapshots.size(); ++index) {
            if (snapshots.at(index).producerId == previousProducerId) {
                m_imageProducer->setCurrentIndex(index + 1);
                break;
            }
        }
    }
}

void NPointCalibrationConfigWidget::setLatestPhysicalSample(
        bool valid, double x, double y, double angleDeg)
{
    m_physicalSampleValid = valid && std::isfinite(x) && std::isfinite(y)
            && std::isfinite(angleDeg);
    m_physicalX = x;
    m_physicalY = y;
    m_physicalAngle = angleDeg;
}

QString NPointCalibrationConfigWidget::captureImageProducerId() const
{
    if (!m_triggerCaptureButton || !m_triggerCaptureButton->isChecked())
        return QString();

    QString producerId;
    for (const QString &fieldKey : {kImagePointX, kImagePointY, kImageAngle}) {
        const QJsonObject binding = m_captureBindingValues.value(fieldKey);
        if (binding.value(QStringLiteral("mode")).toString()
                != QStringLiteral("binding")) {
            return QString();
        }
        const QString fieldProducerId = binding.value(QStringLiteral("producerId"))
                .toString().trimmed();
        if (fieldProducerId.isEmpty())
            return QString();
        if (producerId.isEmpty())
            producerId = fieldProducerId;
        else if (producerId != fieldProducerId)
            return QString();
    }
    return producerId;
}

bool NPointCalibrationConfigWidget::captureCurrentSample(QString *errorMessage)
{
    if (!m_triggerCaptureButton || !m_triggerCaptureButton->isChecked()) {
        if (errorMessage)
            *errorMessage = tr("当前为手动输入模式，请在“编辑标定点”中录入坐标");
        emit sampleStateChanged(errorMessage ? *errorMessage : tr("当前不是触发获取模式"), false);
        return false;
    }
    if (m_nextCaptureRow < 0 || m_nextCaptureRow >= m_sampleTable->rowCount()) {
        if (errorMessage)
            *errorMessage = tr("标定点已采集完成，请清空或调整点数后再试");
        emit sampleStateChanged(errorMessage ? *errorMessage : tr("标定点已采集完成"), false);
        return false;
    }
    const int selected = m_imageProducer->currentIndex() - 1;
    if (selected < 0 || selected >= m_snapshots.size()) {
        if (errorMessage)
            *errorMessage = tr("当前为手动输入来源，请在“编辑标定点”中录入坐标");
        emit sampleStateChanged(errorMessage ? *errorMessage : tr("当前没有可触发的图像点来源"), false);
        return false;
    }
    const CalibrationProducerSnapshot &snapshot = m_snapshots.at(selected);
    const double imageX = snapshot.payload.value(QStringLiteral("x")).toDouble();
    const double imageY = snapshot.payload.value(QStringLiteral("y")).toDouble();
    const double imageAngle = snapshot.payload.value(QStringLiteral("angle")).toDouble();
    if (!snapshot.valid || !snapshot.payload.value(QStringLiteral("x")).isDouble()
            || !snapshot.payload.value(QStringLiteral("y")).isDouble()
            || !snapshot.payload.value(QStringLiteral("angle")).isDouble()
            || !std::isfinite(imageX) || !std::isfinite(imageY)
            || !std::isfinite(imageAngle)) {
        if (errorMessage)
            *errorMessage = tr("所选前序工具尚无有效运行位姿");
        emit sampleStateChanged(errorMessage ? *errorMessage : tr("前序运行位姿无效"), false);
        return false;
    }
    const int row = m_nextCaptureRow;
    m_sampleTable->setItem(row, 1, numberItem(imageX));
    m_sampleTable->setItem(row, 2, numberItem(imageY));
    m_sampleTable->setItem(row, 3, numberItem(imageAngle));
    if (m_physicalSampleValid) {
        m_sampleTable->setItem(row, 4, numberItem(m_physicalX));
        m_sampleTable->setItem(row, 5, numberItem(m_physicalY));
        m_sampleTable->setItem(row, 6, numberItem(m_physicalAngle));
    }
    ++m_nextCaptureRow;
    if (m_nextCaptureRow < m_sampleTable->rowCount())
        m_sampleTable->selectRow(m_nextCaptureRow);
    else
        m_sampleTable->clearSelection();
    m_sampleStatus->setText(tr("第 %1 点采样完成：图像(%2,%3)%4")
                            .arg(row + 1)
                            .arg(imageX, 0, 'f', 3)
                            .arg(imageY, 0, 'f', 3)
                            .arg(m_physicalSampleValid ? tr("，已合并通信物理坐标")
                                                       : tr("，物理坐标沿用表格值")));
    emit sampleStateChanged(m_sampleStatus->text(), true);
    return true;
}

void NPointCalibrationConfigWidget::importPoints()
{
    const QString path = QFileDialog::getOpenFileName(
                this, tr("导入标定点"), QString(), tr("点集文件 (*.txt *.csv)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导入失败"), tr("无法读取点集文件"));
        return;
    }
    QVector<QStringList> rows;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList values = line.split(QRegExp(QStringLiteral("[,;\\s]+")),
                                              Qt::SkipEmptyParts);
        bool firstValueIsNumber = false;
        values.value(0).toDouble(&firstValueIsNumber);
        if (!firstValueIsNumber && rows.isEmpty())
            continue;
        if (values.size() < 4) {
            QMessageBox::warning(this, tr("导入失败"), tr("每行至少需要 Column,Row,X,Y 四个字段"));
            return;
        }
        rows.append(values);
    }
    if (rows.size() < 3 || rows.size() > 99) {
        QMessageBox::warning(this, tr("导入失败"), tr("点集数量必须为3到99组"));
        return;
    }
    const QSignalBlocker countBlocker(m_translationCount);
    m_translationCount->setValue(rows.size());
    m_sampleTable->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        const QStringList &values = rows.at(row);
        m_sampleTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        const QStringList normalized = values.size() >= 6
                ? values.mid(0, 6)
                : QStringList{values.at(0), values.at(1), QStringLiteral("0"),
                              values.at(2), values.at(3), QStringLiteral("0")};
        for (int column = 0; column < 6; ++column)
            m_sampleTable->setItem(row, column + 1, new QTableWidgetItem(normalized.at(column)));
    }
    m_nextCaptureRow = 0;
    if (m_sampleTable->rowCount() > 0)
        m_sampleTable->selectRow(0);
}

void NPointCalibrationConfigWidget::exportPoints()
{
    const QString path = QFileDialog::getSaveFileName(
                this, tr("导出标定点"), QString(), tr("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法写入点集文件"));
        return;
    }
    QTextStream stream(&file);
    stream << "column,row,imageAngleDeg,machineX,machineY,machineAngleDeg\n";
    for (int row = 0; row < m_sampleTable->rowCount(); ++row) {
        QStringList values;
        for (int column = 1; column <= 6; ++column)
            values.append(m_sampleTable->item(row, column)
                          ? m_sampleTable->item(row, column)->text() : QString());
        stream << values.join(QLatin1Char(',')) << '\n';
    }
    if (!file.commit())
        QMessageBox::warning(this, tr("导出失败"), tr("点集文件提交失败"));
}
