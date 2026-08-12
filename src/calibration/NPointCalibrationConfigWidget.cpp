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
#include <QJsonArray>
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
#include <utility>

namespace {

const QString kImagePointX = QStringLiteral("imagePointX");
const QString kImagePointY = QStringLiteral("imagePointY");
const QString kImageAngle = QStringLiteral("imageAngle");
const QString kPhysicalPointX = QStringLiteral("physicalPointX");
const QString kPhysicalPointY = QStringLiteral("physicalPointY");
const QString kPhysicalAngle = QStringLiteral("physicalAngle");
const QString kNinePointXY = QStringLiteral("nine_point_xy");
const QString kTwelvePointAxisTrace = QStringLiteral("twelve_point_axis_trace");
const QString kTwelvePointPoseMapping = QStringLiteral("twelve_point_pose_mapping");

bool isKnownCalibrationMode(const QString &mode)
{
    return mode == kNinePointXY || mode == kTwelvePointAxisTrace
            || mode == kTwelvePointPoseMapping;
}

QString calibrationModeForCounts(int translationCount, int rotationCount)
{
    if (translationCount == 9 && rotationCount == 0)
        return kNinePointXY;
    if (translationCount == 9 && rotationCount == 3)
        return kTwelvePointAxisTrace;
    return QString();
}

double wrappedAngleDelta(double fromDeg, double toDeg)
{
    double delta = std::fmod(toDeg - fromDeg, 360.0);
    if (delta >= 180.0)
        delta -= 360.0;
    else if (delta < -180.0)
        delta += 360.0;
    return delta;
}

bool anglesAreObservable(const QVector<double> &anglesDeg,
                         double minimumSpanDeg = 5.0)
{
    if (anglesDeg.size() < 3)
        return false;
    QVector<double> unwrapped;
    unwrapped.reserve(anglesDeg.size());
    unwrapped.append(anglesDeg.first());
    for (int index = 1; index < anglesDeg.size(); ++index) {
        unwrapped.append(unwrapped.last()
                         + wrappedAngleDelta(anglesDeg.at(index - 1),
                                             anglesDeg.at(index)));
    }
    double minimum = unwrapped.first();
    double maximum = unwrapped.first();
    QVector<double> independent;
    for (double angle : std::as_const(unwrapped)) {
        minimum = qMin(minimum, angle);
        maximum = qMax(maximum, angle);
        bool duplicate = false;
        for (double existing : std::as_const(independent)) {
            if (std::abs(angle - existing) < 0.01) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            independent.append(angle);
    }
    return independent.size() >= 3
            && maximum - minimum >= minimumSpanDeg;
}

QTableWidgetItem *numberItem(double value)
{
    QTableWidgetItem *item = new QTableWidgetItem(QString::number(value, 'f', 6));
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

QTableWidgetItem *completionItem(bool completed)
{
    QTableWidgetItem *item = new QTableWidgetItem;
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable
                   | Qt::ItemIsUserCheckable);
    item->setCheckState(completed ? Qt::Checked : Qt::Unchecked);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

QTableWidgetItem *typeItem(const QString &type)
{
    QTableWidgetItem *item = new QTableWidgetItem(type);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
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

bool isFiniteJsonNumber(const QJsonValue &value)
{
    return value.isDouble() && std::isfinite(value.toDouble());
}

bool isJsonIntegerInRange(const QJsonValue &value, int minimum, int maximum)
{
    if (!isFiniteJsonNumber(value))
        return false;
    const double number = value.toDouble();
    return std::floor(number) == number && number >= minimum && number <= maximum;
}

int comboIndexForData(const QComboBox *combo, const QString &data)
{
    if (!combo)
        return -1;
    for (int index = 0; index < combo->count(); ++index) {
        if (combo->itemData(index).toString() == data)
            return index;
    }
    return -1;
}

struct CollapsibleCard
{
    QFrame *card = nullptr;
    QWidget *content = nullptr;
    QVBoxLayout *contentLayout = nullptr;
};

CollapsibleCard createCollapsibleCard(QWidget *parent,
                                      const QString &objectName,
                                      const QString &title)
{
    CollapsibleCard result;
    result.card = new QFrame(parent);
    result.card->setObjectName(objectName);
    result.card->setProperty("panelRole", QStringLiteral("configCard"));

    QVBoxLayout *cardLayout = new QVBoxLayout(result.card);
    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *titleLabel = new QLabel(title, result.card);
    titleLabel->setObjectName(objectName + QStringLiteral("Title"));
    titleLabel->setProperty("role", QStringLiteral("cardTitle"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    QToolButton *collapseButton = new QToolButton(result.card);
    collapseButton->setObjectName(objectName + QStringLiteral("CollapseButton"));
    collapseButton->setProperty("role", QStringLiteral("collapseCard"));
    collapseButton->setCheckable(true);
    collapseButton->setText(QStringLiteral("⌄"));
    collapseButton->setToolTip(QObject::tr("折叠或展开%1").arg(title));
    collapseButton->setAccessibleName(QObject::tr("%1折叠按钮").arg(title));
    headerLayout->addWidget(collapseButton);
    cardLayout->addLayout(headerLayout);

    result.content = new QWidget(result.card);
    result.content->setObjectName(objectName + QStringLiteral("Content"));
    result.contentLayout = new QVBoxLayout(result.content);
    result.contentLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->addWidget(result.content);

    QObject::connect(collapseButton, &QToolButton::clicked, result.content,
                     [card = result.card,
                      content = result.content,
                      collapseButton](bool collapsed) {
        content->setVisible(!collapsed);
        collapseButton->setText(collapsed ? QStringLiteral("›")
                                          : QStringLiteral("⌄"));
        card->setProperty("collapsed", collapsed);
    });
    return result;
}

} // namespace

void NPointCalibrationConfigWidget::setParameterMode(bool showAll)
{
    m_basicButton->setChecked(!showAll);
    m_allButton->setChecked(showAll);
    // These two cards are retained for persisted-configuration compatibility,
    // but their values are not consumed by the current HALCON solver.  Keep
    // them hidden until robot path planning / robust fitting is implemented.
    m_physicalCoordinateCard->setVisible(false);
    m_runtimeParametersCard->setVisible(false);
    m_qualityCard->setVisible(showAll);
    m_effectiveRegionCard->setVisible(showAll);
}

void NPointCalibrationConfigWidget::setRegionSummary(
        int validRegionPointCount,
        int safeRegionPointCount,
        const QString &message,
        const QString &state)
{
    if (!m_validRegionPointCount || !m_safeRegionPointCount
            || !m_regionGenerationStatus) {
        return;
    }
    m_validRegionPointCount->setText(QString::number(qMax(0, validRegionPointCount)));
    m_safeRegionPointCount->setText(QString::number(qMax(0, safeRegionPointCount)));
    m_regionGenerationStatus->setText(message.trimmed().isEmpty()
                                      ? tr("尚未生成") : message);
    const QString normalizedState = state == QStringLiteral("ok")
            || state == QStringLiteral("warning")
            || state == QStringLiteral("error")
            ? state : QStringLiteral("idle");
    m_regionGenerationStatus->setProperty("regionState", normalizedState);
    m_regionGenerationStatus->style()->unpolish(m_regionGenerationStatus);
    m_regionGenerationStatus->style()->polish(m_regionGenerationStatus);
}

void NPointCalibrationConfigWidget::updateCaptureModeUi()
{
    if (!m_captureSubscriptionWidget || !m_manualCaptureButton)
        return;
    m_captureSubscriptionWidget->setVisible(!m_manualCaptureButton->isChecked());
}

void NPointCalibrationConfigWidget::editPoints()
{
    if (m_pointEditorOpen)
        return;
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("CalibrationPointEditorDialog"));
    dialog.setWindowTitle(tr("编辑标定点"));
    dialog.setMinimumSize(1120, 560);
    QVBoxLayout *root = new QVBoxLayout(&dialog);
    QHBoxLayout *toolbar = new QHBoxLayout;
    QPushButton *importButton = new QPushButton(tr("导入"), &dialog);
    QPushButton *exportButton = new QPushButton(tr("导出"), &dialog);
    QPushButton *resetButton = new QPushButton(tr("恢复当前点数"), &dialog);
    QPushButton *clearButton = new QPushButton(tr("清空"), &dialog);
    for (QPushButton *button : {importButton, exportButton, resetButton, clearButton})
        button->setProperty("actionRole", QStringLiteral("secondary"));
    toolbar->addWidget(importButton);
    toolbar->addWidget(exportButton);
    toolbar->addWidget(resetButton);
    toolbar->addWidget(clearButton);
    toolbar->addStretch();
    root->addLayout(toolbar);

    QTableWidget *editor = new QTableWidget(m_sampleTable->rowCount(), 9, &dialog);
    editor->setHorizontalHeaderLabels({tr("序号"), tr("类型"), tr("图像坐标X"),
                                       tr("图像坐标Y"), tr("图像角度"),
                                       tr("物理坐标X"), tr("物理坐标Y"),
                                       tr("物理角度"), tr("完成")});
    editor->verticalHeader()->setVisible(false);
    editor->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    QVector<bool> editorCompleted = m_sampleCompleted;
    editorCompleted.resize(editor->rowCount());
    int editorTranslationCount = m_translationCount->value();
    int editorRotationCount = m_rotationCount->value();
    bool editorUpdating = false;
    auto populateEditor = [&]() {
        editorUpdating = true;
        editor->setRowCount(m_sampleTable->rowCount());
        editorCompleted.resize(editor->rowCount());
        for (int row = 0; row < m_sampleTable->rowCount(); ++row) {
            const QTableWidgetItem *indexSource = m_sampleTable->item(row, 0);
            QTableWidgetItem *indexItem = new QTableWidgetItem(
                        indexSource ? indexSource->text() : QString::number(row + 1));
            indexItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            indexItem->setTextAlignment(Qt::AlignCenter);
            editor->setItem(row, 0, indexItem);
            editor->setItem(row, 1, typeItem(row < editorTranslationCount
                                             ? tr("平移") : tr("旋转")));
            for (int column = 1; column <= 6; ++column) {
                const QTableWidgetItem *source = m_sampleTable->item(row, column);
                editor->setItem(row, column + 1,
                                new QTableWidgetItem(source ? source->text()
                                                            : QStringLiteral("0.000000")));
            }
            editor->setItem(row, 8, completionItem(editorCompleted.at(row)));
        }
        editorUpdating = false;
    };
    auto fillEditorWithZeros = [&](int translationCount, int rotationCount) {
        editorUpdating = true;
        editorTranslationCount = translationCount;
        editorRotationCount = rotationCount;
        const int count = translationCount + rotationCount;
        editor->setRowCount(count);
        editorCompleted = QVector<bool>(count, false);
        for (int row = 0; row < count; ++row) {
            QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
            indexItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            indexItem->setTextAlignment(Qt::AlignCenter);
            editor->setItem(row, 0, indexItem);
            editor->setItem(row, 1, typeItem(row < translationCount
                                             ? tr("平移") : tr("旋转")));
            for (int column = 2; column <= 7; ++column)
                editor->setItem(row, column, numberItem(0.0));
            editor->setItem(row, 8, completionItem(false));
        }
        editorUpdating = false;
    };
    populateEditor();
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

    connect(editor, &QTableWidget::itemChanged, &dialog,
            [&editorCompleted, &editorUpdating](QTableWidgetItem *item) {
        if (editorUpdating || !item || item->column() != 8)
            return;
        if (item->row() >= 0 && item->row() < editorCompleted.size())
            editorCompleted[item->row()] = item->checkState() == Qt::Checked;
    });
    connect(importButton, &QPushButton::clicked, this,
            [this, &editorTranslationCount, &editorRotationCount,
             &editorCompleted, &editorUpdating, editor]() {
        editorUpdating = true;
        bool retryRequested = false;
        do {
            retryRequested = false;
            importPoints(editor, &editorTranslationCount,
                         &editorRotationCount, &editorCompleted,
                         &retryRequested);
        } while (retryRequested && editor->isVisible());
        editorUpdating = false;
    });
    connect(exportButton, &QPushButton::clicked, this,
            [this, editor, &editorTranslationCount]() {
        exportPoints(editor, editorTranslationCount);
    });
    connect(resetButton, &QPushButton::clicked, this,
            [&fillEditorWithZeros, &editorTranslationCount,
             &editorRotationCount]() {
        fillEditorWithZeros(editorTranslationCount, editorRotationCount);
    });
    connect(clearButton, &QPushButton::clicked, editor,
            [&fillEditorWithZeros, &editorTranslationCount, &editorRotationCount]() {
        fillEditorWithZeros(editorTranslationCount, editorRotationCount);
    });
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(confirm, &QPushButton::clicked, &dialog,
            [this, editor, &dialog, &editorTranslationCount,
             &editorRotationCount]() {
        for (int row = 0; row < editor->rowCount(); ++row) {
            for (int column = 2; column <= 7; ++column) {
                bool ok = false;
                QTableWidgetItem *item = editor->item(row, column);
                const QString text = item ? item->text().trimmed() : QString();
                const double value = text.isEmpty() ? 0.0 : text.toDouble(&ok);
                if (text.isEmpty())
                    ok = true;
                if (ok && std::isfinite(value)) {
                    if (item)
                        item->setText(QString::number(value, 'f', 6));
                    else
                        editor->setItem(row, column, numberItem(value));
                } else {
                    editor->setCurrentCell(row, column);
                    QMessageBox::warning(
                                &dialog, QObject::tr("标定点无效"),
                                QObject::tr("第 %1 行第 %2 列必须是有限浮点数")
                                .arg(row + 1).arg(column + 1));
                    return;
                }
            }
        }
        const QString mode = calibrationMode();
        const int expectedRotationCount = mode == kNinePointXY ? 0 : 3;
        if (editorTranslationCount != 9
                || editorRotationCount != expectedRotationCount
                || editor->rowCount() != 9 + expectedRotationCount) {
            QMessageBox::warning(
                        &dialog, tr("标定点无效"),
                        tr("当前标定模式的点数必须固定为 %1")
                        .arg(mode == kNinePointXY ? tr("9平移+0旋转")
                                                  : tr("9平移+3旋转")));
            return;
        }
        if (expectedRotationCount > 0) {
            QVector<double> imageAngles;
            QVector<double> machineAngles;
            int completedRotations = 0;
            for (int row = editorTranslationCount;
                 row < editor->rowCount(); ++row) {
                const QTableWidgetItem *completion = editor->item(row, 8);
                if (!completion || completion->checkState() != Qt::Checked)
                    continue;
                ++completedRotations;
                machineAngles.append(editor->item(row, 7)->text().toDouble());
                if (mode == kTwelvePointPoseMapping)
                    imageAngles.append(editor->item(row, 4)->text().toDouble());
            }
            if (completedRotations == expectedRotationCount
                    && !anglesAreObservable(machineAngles)) {
                QMessageBox::warning(
                            &dialog, tr("旋转角度无效"),
                            tr("3个旋转点的机械角度不可区分或跨度不足5°，请重新录入"));
                return;
            }
            if (completedRotations == expectedRotationCount
                    && mode == kTwelvePointPoseMapping
                    && !anglesAreObservable(imageAngles)) {
                QMessageBox::warning(
                            &dialog, tr("图像角度不可观测"),
                            tr("位置与姿态模式需要3个独立图像角度且跨度不少于5°；不会自动降级为轴轨迹模式"));
                return;
            }
        }
        dialog.accept();
    });
    m_pointEditorOpen = true;
    const int dialogResult = dialog.exec();
    m_pointEditorOpen = false;
    if (dialogResult != QDialog::Accepted)
        return;

    {
        const QSignalBlocker translationBlocker(m_translationCount);
        const QSignalBlocker rotationBlocker(m_rotationCount);
        m_translationCount->setValue(editorTranslationCount);
        m_rotationCount->setValue(editorRotationCount);
    }
    m_sampleTable->setRowCount(editor->rowCount());
    for (int row = 0; row < editor->rowCount(); ++row) {
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        m_sampleTable->setItem(row, 0, indexItem);
        for (int column = 1; column <= 6; ++column) {
            const QTableWidgetItem *source = editor->item(row, column + 1);
            m_sampleTable->setItem(row, column,
                                   new QTableWidgetItem(source ? source->text()
                                                               : QStringLiteral("0.000000")));
        }
    }
    m_sampleCompleted = editorCompleted;
    m_sampleCompleted.resize(editor->rowCount());
    resetCaptureCursorToFirstIncomplete();
    m_sampleStatus->setText(tr("已编辑 %1 组对应点，完成 %2 组")
                            .arg(editor->rowCount())
                            .arg(completedSampleCount()));
    emit sampleStateChanged(m_sampleStatus->text(), true);
    emit sampleDataChanged();
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

    const CollapsibleCard movement = createCollapsibleCard(
                this, QStringLiteral("nPointMovementCard"), tr("机械臂移动参数"));
    QFrame *movementCard = movement.card;
    QLabel *movementHint = new QLabel(tr("机械臂平移路径须覆盖有效标定区域"),
                                      movement.content);
    movementHint->setProperty("role", QStringLiteral("cardHint"));
    movement.contentLayout->addWidget(movementHint);
    QFormLayout *movementForm = new QFormLayout;
    m_imageProducer = new QComboBox(movement.content);
    m_imageProducer->setObjectName(QStringLiteral("nPointImageProducerCombo"));
    m_imageProducer->addItem(tr("手动输入图像坐标"), QJsonObject{
                                 {QStringLiteral("mode"), QStringLiteral("manual")}});
    m_imageProducer->hide();
    m_calibrationMode = new QComboBox(movement.content);
    m_calibrationMode->setObjectName(QStringLiteral("nPointCalibrationMode"));
    m_calibrationMode->addItem(tr("9点 XY标定"), kNinePointXY);
    m_calibrationMode->addItem(tr("12点 旋转中心/轴轨迹标定"),
                               kTwelvePointAxisTrace);
    m_calibrationMode->addItem(tr("12点 位置与姿态标定"),
                               kTwelvePointPoseMapping);
    m_calibrationMode->setToolTip(
                tr("模式必须在采样前明确选择；系统不会根据导入文件或求解结果自动切换模式"));
    m_translationCount = new QSpinBox(movement.content);
    m_translationCount->setObjectName(QStringLiteral("nPointTranslationCount"));
    m_translationCount->setRange(3, 99);
    m_translationCount->setValue(9);
    m_translationCount->setReadOnly(true);
    m_translationCount->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_translationCount->setToolTip(tr("由标定模式固定为 9 个平移点"));
    m_rotationCount = new QSpinBox(movement.content);
    m_rotationCount->setObjectName(QStringLiteral("nPointRotationCount"));
    m_rotationCount->setRange(0, 99);
    m_rotationCount->setValue(0);
    m_rotationCount->setReadOnly(true);
    m_rotationCount->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_rotationCount->setToolTip(
                tr("由标定模式固定为 0 或 3；两种12点模式均为9个平移点+3个旋转点"));
    QLabel *calibrationModeLabel = new QLabel(tr("标定模式"), movement.content);
    QLabel *translationLabel = new QLabel(tr("平移次数"), movement.content);
    QLabel *rotationLabel = new QLabel(tr("旋转次数"), movement.content);
    calibrationModeLabel->setProperty("role", QStringLiteral("rowField"));
    translationLabel->setProperty("role", QStringLiteral("rowField"));
    rotationLabel->setProperty("role", QStringLiteral("rowField"));
    movementForm->addRow(calibrationModeLabel, m_calibrationMode);
    movementForm->addRow(translationLabel, m_translationCount);
    movementForm->addRow(rotationLabel, m_rotationCount);
    QPushButton *editButton = new QPushButton(tr("编辑"), movement.content);
    editButton->setObjectName(QStringLiteral("nPointEditPointsButton"));
    editButton->setProperty("actionRole", QStringLiteral("secondary"));
    QLabel *editLabel = new QLabel(tr("编辑标定点"), movement.content);
    editLabel->setProperty("role", QStringLiteral("rowField"));
    movementForm->addRow(editLabel, editButton);
    movement.contentLayout->addLayout(movementForm);
    layout->addWidget(movementCard);

    const CollapsibleCard capture = createCollapsibleCard(
                this, QStringLiteral("nPointCalibrationParametersCard"), tr("标定参数"));
    QFrame *captureCard = capture.card;
    QFormLayout *captureModeForm = new QFormLayout;
    captureModeForm->setHorizontalSpacing(12);
    captureModeForm->setVerticalSpacing(10);

    QFrame *captureModeFrame = new QFrame(capture.content);
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
    captureModeForm->addRow(fieldLabel(tr("标定点获取"), QString(), capture.content),
                            captureModeFrame);
    capture.contentLayout->addLayout(captureModeForm);

    m_captureSubscriptionWidget = new QWidget(capture.content);
    m_captureSubscriptionWidget->setObjectName(
                QStringLiteral("nPointCaptureSubscriptionWidget"));
    QFormLayout *captureSubscriptionForm =
            new QFormLayout(m_captureSubscriptionWidget);
    captureSubscriptionForm->setContentsMargins(0, 0, 0, 0);
    captureSubscriptionForm->setHorizontalSpacing(12);
    captureSubscriptionForm->setVerticalSpacing(10);
    capture.contentLayout->addWidget(m_captureSubscriptionWidget);

    auto addBindingRow = [this, captureSubscriptionForm](const QString &fieldKey,
                                                          const QString &fieldName,
                                                          const QString &objectName,
                                                          bool imageGroup) {
        QWidget *field = new QWidget(m_captureSubscriptionWidget);
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
        captureSubscriptionForm->addRow(
                    fieldLabel(fieldName,
                               tr("该字段预留前序输出或通信订阅接口，本轮不执行通用订阅解析"),
                               m_captureSubscriptionWidget),
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
                tr("等待采样；无设备模式可通过“编辑标定点”手动录入"),
                capture.content);
    m_sampleStatus->setProperty("role", QStringLiteral("cardHint"));
    m_sampleStatus->setWordWrap(true);
    capture.contentLayout->addWidget(m_sampleStatus);
    layout->addWidget(captureCard);

    m_sampleTable = new QTableWidget(9, 7, this);
    m_sampleTable->setObjectName(QStringLiteral("nPointSampleTable"));
    m_sampleTable->setHorizontalHeaderLabels({tr("序号"), tr("Column(px)"), tr("Row(px)"),
                                               tr("图像角度"), tr("X(mm)"), tr("Y(mm)"), tr("物理角度")});
    m_sampleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_sampleTable->verticalHeader()->setVisible(false);
    m_sampleTable->hide();

    const CollapsibleCard physical = createCollapsibleCard(
                this, QStringLiteral("nPointPhysicalCoordinateCard"),
                tr("物理坐标系参数"));
    m_physicalCoordinateCard = physical.card;
    QFormLayout *physicalForm = new QFormLayout;
    m_referenceX = coordinateSpinBox(QStringLiteral("nPointReferenceX"), 0.0,
                                     physical.content);
    m_referenceY = coordinateSpinBox(QStringLiteral("nPointReferenceY"), 0.0,
                                     physical.content);
    m_offsetX = coordinateSpinBox(QStringLiteral("nPointOffsetX"), 1.0,
                                  physical.content);
    m_offsetY = coordinateSpinBox(QStringLiteral("nPointOffsetY"), 1.0,
                                  physical.content);
    m_movePriority = new QComboBox(physical.content);
    m_movePriority->setObjectName(QStringLiteral("nPointMovePriority"));
    m_movePriority->addItem(tr("X优先"), QStringLiteral("x_first"));
    m_movePriority->addItem(tr("Y优先"), QStringLiteral("y_first"));
    m_directionChangeCount = new QSpinBox(physical.content);
    m_directionChangeCount->setObjectName(QStringLiteral("nPointDirectionChangeCount"));
    m_directionChangeCount->setRange(0, 99);
    m_directionChangeCount->setValue(3);
    m_referenceAngle = coordinateSpinBox(QStringLiteral("nPointReferenceAngle"), 0.0,
                                         physical.content);
    m_angleOffset = coordinateSpinBox(QStringLiteral("nPointAngleOffset"), 1.0,
                                      physical.content);
    m_calibrationOrigin = new QSpinBox(physical.content);
    m_calibrationOrigin->setObjectName(QStringLiteral("nPointCalibrationOrigin"));
    m_calibrationOrigin->setRange(1, 99);
    m_calibrationOrigin->setValue(4);
    physicalForm->addRow(fieldLabel(tr("基准点X"), QString(), physical.content), m_referenceX);
    physicalForm->addRow(fieldLabel(tr("基准点Y"), QString(), physical.content), m_referenceY);
    physicalForm->addRow(fieldLabel(tr("偏移X"), QString(), physical.content), m_offsetX);
    physicalForm->addRow(fieldLabel(tr("偏移Y"), QString(), physical.content), m_offsetY);
    physicalForm->addRow(fieldLabel(tr("移动优先"), QString(), physical.content), m_movePriority);
    physicalForm->addRow(fieldLabel(tr("换向移动次数"), QString(), physical.content),
                         m_directionChangeCount);
    physicalForm->addRow(fieldLabel(tr("基准角度"), QString(), physical.content),
                         m_referenceAngle);
    physicalForm->addRow(fieldLabel(tr("角度偏移"), QString(), physical.content), m_angleOffset);
    physicalForm->addRow(fieldLabel(tr("标定原点"), QString(), physical.content),
                         m_calibrationOrigin);
    physical.contentLayout->addLayout(physicalForm);
    layout->addWidget(m_physicalCoordinateCard);

    const CollapsibleCard runtime = createCollapsibleCard(
                this, QStringLiteral("nPointRuntimeParametersCard"), tr("运行参数"));
    m_runtimeParametersCard = runtime.card;
    QFormLayout *runtimeForm = new QFormLayout;
    m_cameraMotionMode = new QComboBox(runtime.content);
    m_cameraMotionMode->setObjectName(QStringLiteral("nPointCameraMotionMode"));
    m_cameraMotionMode->addItem(tr("相机运动"), QStringLiteral("camera_motion"));
    m_degreesOfFreedom = new QComboBox(runtime.content);
    m_degreesOfFreedom->setObjectName(QStringLiteral("nPointDegreesOfFreedom"));
    m_degreesOfFreedom->addItem(tr("缩放、旋转、纵横比、倾斜"),
                                QStringLiteral("scale_rotation_aspect_tilt"));
    m_weightFunction = new QComboBox(runtime.content);
    m_weightFunction->setObjectName(QStringLiteral("nPointWeightFunction"));
    m_weightFunction->addItem(QStringLiteral("Tukey"), QStringLiteral("tukey"));
    m_weightCoefficient = new QSpinBox(runtime.content);
    m_weightCoefficient->setObjectName(QStringLiteral("nPointWeightCoefficient"));
    m_weightCoefficient->setRange(0, 100000);
    m_weightCoefficient->setValue(20);
    runtimeForm->addRow(fieldLabel(tr("相机模式"), QString(), runtime.content),
                        m_cameraMotionMode);
    runtimeForm->addRow(fieldLabel(tr("自由度"), QString(), runtime.content),
                        m_degreesOfFreedom);
    runtimeForm->addRow(fieldLabel(tr("权重函数"), QString(), runtime.content),
                        m_weightFunction);
    runtimeForm->addRow(fieldLabel(tr("权重系数"), QString(), runtime.content),
                        m_weightCoefficient);
    runtime.contentLayout->addLayout(runtimeForm);
    layout->addWidget(m_runtimeParametersCard);

    const CollapsibleCard quality = createCollapsibleCard(
                this, QStringLiteral("nPointQualityCard"), tr("质量门限"));
    m_qualityCard = quality.card;
    QFormLayout *thresholds = new QFormLayout;
    m_rmseLimit = new QDoubleSpinBox(quality.content);
    m_rmseLimit->setObjectName(QStringLiteral("nPointRmseLimit"));
    m_rmseLimit->setDecimals(4);
    m_rmseLimit->setRange(0.0001, 1000.0);
    m_rmseLimit->setValue(0.10);
    m_maxErrorLimit = new QDoubleSpinBox(quality.content);
    m_maxErrorLimit->setObjectName(QStringLiteral("nPointMaxErrorLimit"));
    m_maxErrorLimit->setDecimals(4);
    m_maxErrorLimit->setRange(0.0001, 1000.0);
    m_maxErrorLimit->setValue(0.25);
    m_rotationRmseLimit = new QDoubleSpinBox(quality.content);
    m_rotationRmseLimit->setObjectName(
                QStringLiteral("nPointRotationRmseLimit"));
    m_rotationRmseLimit->setDecimals(4);
    m_rotationRmseLimit->setRange(0.0001, 1000.0);
    m_rotationRmseLimit->setValue(0.10);
    m_rotationRmseLimit->setToolTip(
                tr("旋转轴迹的机械角与圆周相位关系位置RMSE门限，"
                   "与9点XY拟合RMSE门限独立"));
    m_rotationMaxErrorLimit = new QDoubleSpinBox(quality.content);
    m_rotationMaxErrorLimit->setObjectName(
                QStringLiteral("nPointRotationMaxErrorLimit"));
    m_rotationMaxErrorLimit->setDecimals(4);
    m_rotationMaxErrorLimit->setRange(0.0001, 1000.0);
    m_rotationMaxErrorLimit->setValue(0.25);
    m_rotationMaxErrorLimit->setToolTip(
                tr("旋转轴迹的机械角与圆周相位关系最大位置误差门限，"
                   "与9点XY拟合最大误差门限独立"));
    QLabel *rmseLabel = new QLabel(tr("XY RMSE门限(mm)"), quality.content);
    QLabel *maxLabel = new QLabel(tr("XY最大误差门限(mm)"), quality.content);
    m_rotationRmseLimitLabel = new QLabel(
                tr("轴迹RMSE门限(mm)"), quality.content);
    m_rotationMaxErrorLimitLabel = new QLabel(
                tr("轴迹最大误差门限(mm)"), quality.content);
    rmseLabel->setProperty("role", QStringLiteral("rowField"));
    maxLabel->setProperty("role", QStringLiteral("rowField"));
    m_rotationRmseLimitLabel->setProperty("role", QStringLiteral("rowField"));
    m_rotationMaxErrorLimitLabel->setProperty("role",
                                               QStringLiteral("rowField"));
    thresholds->addRow(rmseLabel, m_rmseLimit);
    thresholds->addRow(maxLabel, m_maxErrorLimit);
    thresholds->addRow(m_rotationRmseLimitLabel, m_rotationRmseLimit);
    thresholds->addRow(m_rotationMaxErrorLimitLabel,
                       m_rotationMaxErrorLimit);
    quality.contentLayout->addLayout(thresholds);
    layout->addWidget(m_qualityCard);

    const CollapsibleCard effectiveRegion = createCollapsibleCard(
                this, QStringLiteral("nPointEffectiveRegionCard"), tr("有效区域"));
    m_effectiveRegionCard = effectiveRegion.card;
    QFormLayout *regionForm = new QFormLayout;
    m_safeMarginPx = new QDoubleSpinBox(effectiveRegion.content);
    m_safeMarginPx->setObjectName(QStringLiteral("nPointSafeMarginPx"));
    m_safeMarginPx->setDecimals(2);
    // HALCON 20.11 ErosionCircle maps the configured margin to radius
    // (safeMarginPx + 0.5), whose supported upper radius is 511.5 px.
    m_safeMarginPx->setRange(0.0, 511.0);
    m_safeMarginPx->setValue(0.0);
    m_safeMarginPx->setSuffix(tr(" px"));
    m_safeMarginPx->setToolTip(
                tr("ValidROI 使用 HALCON ErosionCircle 均匀内缩的像素距离；"
                   "0 表示 SafeROI 与 ValidROI 等价"));
    m_validRegionPointCount = new QLabel(QStringLiteral("0"),
                                         effectiveRegion.content);
    m_validRegionPointCount->setObjectName(
                QStringLiteral("nPointValidRegionPointCount"));
    m_safeRegionPointCount = new QLabel(QStringLiteral("0"),
                                        effectiveRegion.content);
    m_safeRegionPointCount->setObjectName(
                QStringLiteral("nPointSafeRegionPointCount"));
    m_regionGenerationStatus = new QLabel(tr("尚未生成"),
                                          effectiveRegion.content);
    m_regionGenerationStatus->setObjectName(
                QStringLiteral("nPointRegionGenerationStatus"));
    m_regionGenerationStatus->setProperty("role", QStringLiteral("cardHint"));
    m_regionGenerationStatus->setProperty("regionState", QStringLiteral("idle"));
    m_regionGenerationStatus->setWordWrap(true);
    regionForm->addRow(fieldLabel(tr("安全内缩距离(px)"),
                                  tr("只允许向 ValidROI 内部收缩，不支持凸包外扩"),
                                  effectiveRegion.content),
                       m_safeMarginPx);
    regionForm->addRow(fieldLabel(tr("ValidROI顶点数"), QString(),
                                  effectiveRegion.content),
                       m_validRegionPointCount);
    regionForm->addRow(fieldLabel(tr("SafeROI顶点数"), QString(),
                                  effectiveRegion.content),
                       m_safeRegionPointCount);
    regionForm->addRow(fieldLabel(tr("区域生成状态"), QString(),
                                  effectiveRegion.content),
                       m_regionGenerationStatus);
    effectiveRegion.contentLayout->addLayout(regionForm);
    layout->addWidget(m_effectiveRegionCard);
    layout->addStretch();

    connect(editButton, &QPushButton::clicked, this, [this]() { editPoints(); });
    connect(m_basicButton, &QPushButton::clicked, this, [this]() { setParameterMode(false); });
    connect(m_allButton, &QPushButton::clicked, this, [this]() { setParameterMode(true); });
    connect(m_manualCaptureButton, &QPushButton::toggled,
            this, [this](bool) { updateCaptureModeUi(); });
    connect(m_calibrationMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NPointCalibrationConfigWidget::handleCalibrationModeChanged);
    connect(m_safeMarginPx, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) {
        setRegionSummary(0, 0, tr("安全内缩距离已改变，请重新执行"),
                         QStringLiteral("warning"));
        emit sampleDataChanged();
    });
    updateCalibrationModeUi();
    fillDefaultGrid();
    updateCaptureModeUi();
    setParameterMode(false);
}

QString NPointCalibrationConfigWidget::calibrationMode() const
{
    return m_calibrationMode
            ? m_calibrationMode->currentData().toString() : kNinePointXY;
}

bool NPointCalibrationConfigWidget::requiresImageAngle() const
{
    return calibrationMode() == kTwelvePointPoseMapping;
}

void NPointCalibrationConfigWidget::handleCalibrationModeChanged(int)
{
    const QString selectedMode = calibrationMode();
    if (!isKnownCalibrationMode(selectedMode)
            || selectedMode == m_lastCalibrationMode) {
        return;
    }
    if (completedSampleCount() > 0) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
                    this, tr("切换标定模式"),
                    tr("切换标定模式会按固定点数重建点表，并清空当前已完成数据。是否继续？"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            const QSignalBlocker blocker(m_calibrationMode);
            const int previousIndex = comboIndexForData(
                        m_calibrationMode, m_lastCalibrationMode);
            if (previousIndex >= 0)
                m_calibrationMode->setCurrentIndex(previousIndex);
            return;
        }
    }
    applyCalibrationMode(selectedMode, true);
}

void NPointCalibrationConfigWidget::applyCalibrationMode(
        const QString &mode, bool resetPoints)
{
    if (!isKnownCalibrationMode(mode))
        return;
    const int modeIndex = comboIndexForData(m_calibrationMode, mode);
    {
        const QSignalBlocker modeBlocker(m_calibrationMode);
        const QSignalBlocker translationBlocker(m_translationCount);
        const QSignalBlocker rotationBlocker(m_rotationCount);
        if (modeIndex >= 0)
            m_calibrationMode->setCurrentIndex(modeIndex);
        m_translationCount->setValue(9);
        m_rotationCount->setValue(mode == kNinePointXY ? 0 : 3);
    }
    m_lastCalibrationMode = mode;
    updateCalibrationModeUi();
    if (resetPoints)
        fillDefaultGrid();
}

void NPointCalibrationConfigWidget::updateCalibrationModeUi()
{
    const QString mode = calibrationMode();
    const bool poseMapping = mode == kTwelvePointPoseMapping;
    const bool hasRotation = mode != kNinePointXY;
    for (QWidget *widget : {static_cast<QWidget *>(m_rotationRmseLimitLabel),
                            static_cast<QWidget *>(m_rotationRmseLimit),
                            static_cast<QWidget *>(m_rotationMaxErrorLimitLabel),
                            static_cast<QWidget *>(m_rotationMaxErrorLimit)}) {
        if (widget)
            widget->setVisible(hasRotation);
    }
    if (QLineEdit *imageAngle = m_captureBindingEdits.value(kImageAngle, nullptr)) {
        imageAngle->setProperty("requiredField", poseMapping);
        imageAngle->setPlaceholderText(poseMapping
                                       ? tr("必填：同一前置工具的图像角度")
                                       : tr("可选：当前模式不使用图像角度映射"));
        imageAngle->setToolTip(poseMapping
                               ? tr("位置与姿态模式必须取得可观测的图像角度")
                               : tr("当前模式允许记录图像角度，但不参与角度映射"));
    }
    if (QToolButton *imageAngleButton =
            m_captureBindingButtons.value(kImageAngle, nullptr)) {
        imageAngleButton->setProperty("requiredField", poseMapping);
    }
    if (QLineEdit *physicalAngle =
            m_captureBindingEdits.value(kPhysicalAngle, nullptr)) {
        physicalAngle->setProperty("requiredField", hasRotation);
        physicalAngle->setPlaceholderText(hasRotation
                                          ? tr("旋转点必填：机械角度 A")
                                          : tr("可选：9点XY不使用机械角度"));
        physicalAngle->setToolTip(hasRotation
                                  ? tr("3个旋转点必须记录同一采样时刻的机械角度 A")
                                  : tr("9点XY只使用机械X/Y"));
    }
    if (QToolButton *physicalAngleButton =
            m_captureBindingButtons.value(kPhysicalAngle, nullptr)) {
        physicalAngleButton->setProperty("requiredField", hasRotation);
    }
    for (QWidget *widget : {static_cast<QWidget *>(
                                m_captureBindingEdits.value(kImageAngle, nullptr)),
                            static_cast<QWidget *>(
                                m_captureBindingButtons.value(kImageAngle, nullptr)),
                            static_cast<QWidget *>(
                                m_captureBindingEdits.value(kPhysicalAngle, nullptr)),
                            static_cast<QWidget *>(
                                m_captureBindingButtons.value(kPhysicalAngle, nullptr))}) {
        if (!widget)
            continue;
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    syncLegacyImageProducer();
}

void NPointCalibrationConfigWidget::fillDefaultGrid()
{
    const int count = m_translationCount->value() + m_rotationCount->value();
    fillZeroPointTable(m_sampleTable, count);
    m_sampleCompleted = QVector<bool>(count, false);
    m_nextCaptureRow = 0;
    m_physicalSampleValid = false;
    if (m_sampleStatus) {
        if (calibrationMode() == kNinePointXY) {
            m_sampleStatus->setText(tr("点表已重置：9点 XY标定；角度不参与求解"));
        } else if (calibrationMode() == kTwelvePointAxisTrace) {
            m_sampleStatus->setText(
                        tr("点表已重置：9平移+3旋转；图像角度可忽略，机械角度用于轴轨迹诊断"));
        } else {
            m_sampleStatus->setText(
                        tr("点表已重置：9平移+3旋转；旋转点必须包含可观测图像角度和机械角度"));
        }
    }
    emit sampleDataChanged();
}

QString NPointCalibrationConfigWidget::sampleTypeForRow(int row) const
{
    return row >= 0 && row < m_translationCount->value()
            ? tr("平移") : tr("旋转");
}

QVector<QLineF> NPointCalibrationConfigWidget::completedTranslationSegments() const
{
    QVector<QLineF> segments;
    const int count = qMin(m_translationCount->value(), m_sampleTable->rowCount());
    for (int row = 1; row < count; ++row) {
        if (row >= m_sampleCompleted.size()
                || !m_sampleCompleted.at(row) || !m_sampleCompleted.at(row - 1)) {
            continue;
        }
        bool x1Ok = false;
        bool y1Ok = false;
        bool x2Ok = false;
        bool y2Ok = false;
        const double x1 = m_sampleTable->item(row - 1, 1)
                ? m_sampleTable->item(row - 1, 1)->text().toDouble(&x1Ok) : 0.0;
        const double y1 = m_sampleTable->item(row - 1, 2)
                ? m_sampleTable->item(row - 1, 2)->text().toDouble(&y1Ok) : 0.0;
        const double x2 = m_sampleTable->item(row, 1)
                ? m_sampleTable->item(row, 1)->text().toDouble(&x2Ok) : 0.0;
        const double y2 = m_sampleTable->item(row, 2)
                ? m_sampleTable->item(row, 2)->text().toDouble(&y2Ok) : 0.0;
        if (x1Ok && y1Ok && x2Ok && y2Ok
                && std::isfinite(x1) && std::isfinite(y1)
                && std::isfinite(x2) && std::isfinite(y2)) {
            segments.append(QLineF(QPointF(x1, y1), QPointF(x2, y2)));
        }
    }
    return segments;
}

QVector<QLineF> NPointCalibrationConfigWidget::completedRotationSegments() const
{
    QVector<QLineF> segments;
    const int first = qMin(m_translationCount->value(), m_sampleTable->rowCount());
    const int end = qMin(first + m_rotationCount->value(), m_sampleTable->rowCount());
    for (int row = first + 1; row < end; ++row) {
        if (row >= m_sampleCompleted.size()
                || !m_sampleCompleted.at(row) || !m_sampleCompleted.at(row - 1)) {
            continue;
        }
        bool x1Ok = false;
        bool y1Ok = false;
        bool x2Ok = false;
        bool y2Ok = false;
        const double x1 = m_sampleTable->item(row - 1, 1)
                ? m_sampleTable->item(row - 1, 1)->text().toDouble(&x1Ok) : 0.0;
        const double y1 = m_sampleTable->item(row - 1, 2)
                ? m_sampleTable->item(row - 1, 2)->text().toDouble(&y1Ok) : 0.0;
        const double x2 = m_sampleTable->item(row, 1)
                ? m_sampleTable->item(row, 1)->text().toDouble(&x2Ok) : 0.0;
        const double y2 = m_sampleTable->item(row, 2)
                ? m_sampleTable->item(row, 2)->text().toDouble(&y2Ok) : 0.0;
        if (x1Ok && y1Ok && x2Ok && y2Ok
                && std::isfinite(x1) && std::isfinite(y1)
                && std::isfinite(x2) && std::isfinite(y2)) {
            segments.append(QLineF(QPointF(x1, y1), QPointF(x2, y2)));
        }
    }
    return segments;
}

int NPointCalibrationConfigWidget::completedTranslationSampleCount() const
{
    int count = 0;
    const int limit = qMin(m_translationCount->value(), m_sampleCompleted.size());
    for (int row = 0; row < limit; ++row) {
        if (m_sampleCompleted.at(row))
            ++count;
    }
    return count;
}

int NPointCalibrationConfigWidget::translationSampleCount() const
{
    return m_translationCount ? m_translationCount->value() : 0;
}

void NPointCalibrationConfigWidget::resetCaptureCursorToFirstIncomplete()
{
    const int count = m_sampleTable ? m_sampleTable->rowCount() : 0;
    if (m_sampleCompleted.size() != count)
        m_sampleCompleted.resize(count);
    m_nextCaptureRow = count;
    for (int row = 0; row < count; ++row) {
        if (!m_sampleCompleted.at(row)) {
            m_nextCaptureRow = row;
            break;
        }
    }
    if (m_nextCaptureRow < count)
        m_sampleTable->selectRow(m_nextCaptureRow);
    else
        m_sampleTable->clearSelection();
}

int NPointCalibrationConfigWidget::completedSampleCount() const
{
    int completed = 0;
    for (bool value : m_sampleCompleted) {
        if (value)
            ++completed;
    }
    return completed;
}

int NPointCalibrationConfigWidget::sampleCount() const
{
    return m_sampleTable ? m_sampleTable->rowCount() : 0;
}

QVector<CalibrationSample> NPointCalibrationConfigWidget::samplesFromTable(
        QString *errorMessage) const
{
    QVector<CalibrationSample> samples;
    const int translationRows = qMin(m_translationCount->value(),
                                     m_sampleTable->rowCount());
    for (int row = 0; row < translationRows; ++row) {
        if (row >= m_sampleCompleted.size() || !m_sampleCompleted.at(row))
            continue;
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

bool NPointCalibrationConfigWidget::validateCompletedRotationAngles(
        QString *errorMessage) const
{
    const QString mode = calibrationMode();
    if (mode == kNinePointXY)
        return true;
    const int firstRotationRow = m_translationCount->value();
    const int rotationCount = m_rotationCount->value();
    int completedRotationCount = 0;
    QVector<double> imageAngles;
    QVector<double> machineAngles;
    for (int row = firstRotationRow;
         row < firstRotationRow + rotationCount
         && row < m_sampleTable->rowCount(); ++row) {
        if (row >= m_sampleCompleted.size() || !m_sampleCompleted.at(row))
            continue;
        ++completedRotationCount;
        bool imageOk = false;
        bool machineOk = false;
        const double imageAngle = m_sampleTable->item(row, 3)
                ? m_sampleTable->item(row, 3)->text().toDouble(&imageOk) : 0.0;
        const double machineAngle = m_sampleTable->item(row, 6)
                ? m_sampleTable->item(row, 6)->text().toDouble(&machineOk) : 0.0;
        if (!machineOk || !std::isfinite(machineAngle)) {
            if (errorMessage) {
                *errorMessage = tr("第 %1 个旋转点缺少有效机械角度 A")
                        .arg(row - firstRotationRow + 1);
            }
            return false;
        }
        if (mode == kTwelvePointPoseMapping
                && (!imageOk || !std::isfinite(imageAngle))) {
            if (errorMessage) {
                *errorMessage = tr("第 %1 个旋转点缺少有效图像角度；位置与姿态模式不能降级为轴轨迹模式")
                        .arg(row - firstRotationRow + 1);
            }
            return false;
        }
        machineAngles.append(machineAngle);
        if (mode == kTwelvePointPoseMapping)
            imageAngles.append(imageAngle);
    }
    // Partial point tables must remain saveable as recoverable drafts.  Full
    // mode validation is applied as soon as all three rotation rows complete.
    if (completedRotationCount < rotationCount)
        return true;
    if (!anglesAreObservable(machineAngles)) {
        if (errorMessage) {
            *errorMessage = tr("3个旋转点的机械角度不可区分或跨度不足5°，请重新采集；系统不会自动切换标定模式");
        }
        return false;
    }
    if (mode == kTwelvePointPoseMapping
            && !anglesAreObservable(imageAngles)) {
        if (errorMessage) {
            *errorMessage = tr("旋转样本的图像角度不可观测（需3个独立角度且跨度不少于5°）；请使用有明确方向的模板，或手动改选轴轨迹模式");
        }
        return false;
    }
    return true;
}

CalibrationDraft NPointCalibrationConfigWidget::draft(QString *errorMessage) const
{
    if (errorMessage)
        errorMessage->clear();
    const QString mode = calibrationMode();
    const int expectedRotationCount = mode == kNinePointXY ? 0 : 3;
    if (!isKnownCalibrationMode(mode)
            || m_translationCount->value() != 9
            || m_rotationCount->value() != expectedRotationCount
            || m_sampleTable->rowCount() != 9 + expectedRotationCount) {
        if (errorMessage)
            *errorMessage = tr("标定模式与固定点数不一致，请重新选择标定模式");
        return CalibrationDraft();
    }
    CalibrationDraft result;
    result.samples = samplesFromTable(errorMessage);
    if (errorMessage && !errorMessage->isEmpty())
        return CalibrationDraft();
    result.parameters.insert(QStringLiteral("calibrationMode"), mode);
    result.parameters.insert(QStringLiteral("rmseLimit"), m_rmseLimit->value());
    result.parameters.insert(QStringLiteral("maxErrorLimit"), m_maxErrorLimit->value());
    result.parameters.insert(QStringLiteral("rotationRmseLimitMm"),
                             m_rotationRmseLimit->value());
    result.parameters.insert(QStringLiteral("rotationMaxErrorLimitMm"),
                             m_rotationMaxErrorLimit->value());
    result.parameters.insert(QStringLiteral("safeMarginPx"), m_safeMarginPx->value());
    result.parameters.insert(QStringLiteral("translationCount"), m_translationCount->value());
    result.parameters.insert(QStringLiteral("rotationCount"), m_rotationCount->value());
    result.parameters.insert(QStringLiteral("totalSampleCount"), m_sampleTable->rowCount());
    result.parameters.insert(QStringLiteral("completedSampleCount"), completedSampleCount());
    result.parameters.insert(QStringLiteral("completedTranslationSampleCount"),
                             completedTranslationSampleCount());
    QJsonArray rotationSamples;
    for (int row = m_translationCount->value(); row < m_sampleTable->rowCount(); ++row) {
        if (row >= m_sampleCompleted.size() || !m_sampleCompleted.at(row))
            continue;
        const QStringList keys{QStringLiteral("column"), QStringLiteral("row"),
                               QStringLiteral("imageAngleDeg"), QStringLiteral("machineX"),
                               QStringLiteral("machineY"), QStringLiteral("machineAngleDeg")};
        QJsonObject sample{{QStringLiteral("index"), row + 1},
                           {QStringLiteral("type"), QStringLiteral("rotation")}};
        for (int column = 1; column <= 6; ++column) {
            bool ok = false;
            const double value = m_sampleTable->item(row, column)
                    ? m_sampleTable->item(row, column)->text().toDouble(&ok) : 0.0;
            if (!ok || !std::isfinite(value)) {
                if (errorMessage)
                    *errorMessage = tr("第%1行第%2列不是有效数字")
                            .arg(row + 1).arg(column + 1);
                return CalibrationDraft();
            }
            sample.insert(keys.at(column - 1), value);
        }
        rotationSamples.append(sample);
    }
    result.parameters.insert(QStringLiteral("rotationSamples"), rotationSamples);
    if (!validateCompletedRotationAngles(errorMessage))
        return CalibrationDraft();
    result.parameters.insert(
                QStringLiteral("imageBinding"),
                m_manualCaptureButton && m_manualCaptureButton->isChecked()
                ? QJsonObject{{QStringLiteral("mode"), QStringLiteral("manual")}}
                : m_imageProducer->currentData().toJsonObject());
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

QJsonObject NPointCalibrationConfigWidget::persistentSettings() const
{
    return QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("parameterMode"),
         m_allButton && m_allButton->isChecked()
            ? QStringLiteral("all") : QStringLiteral("basic")},
        {QStringLiteral("calibrationMode"), calibrationMode()},
        {QStringLiteral("translationCount"), m_translationCount->value()},
        {QStringLiteral("rotationCount"), m_rotationCount->value()},
        {QStringLiteral("captureBindings"), captureBindings()},
        {QStringLiteral("physicalCoordinateParameters"), QJsonObject{
             {QStringLiteral("referenceX"), m_referenceX->value()},
             {QStringLiteral("referenceY"), m_referenceY->value()},
             {QStringLiteral("offsetX"), m_offsetX->value()},
             {QStringLiteral("offsetY"), m_offsetY->value()},
             {QStringLiteral("movePriority"), m_movePriority->currentData().toString()},
             {QStringLiteral("directionChangeCount"), m_directionChangeCount->value()},
             {QStringLiteral("referenceAngle"), m_referenceAngle->value()},
             {QStringLiteral("angleOffset"), m_angleOffset->value()},
             {QStringLiteral("calibrationOrigin"), m_calibrationOrigin->value()}
         }},
        {QStringLiteral("runtimeParameters"), QJsonObject{
             {QStringLiteral("cameraMotionMode"), m_cameraMotionMode->currentData().toString()},
             {QStringLiteral("degreesOfFreedom"), m_degreesOfFreedom->currentData().toString()},
             {QStringLiteral("weightFunction"), m_weightFunction->currentData().toString()},
             {QStringLiteral("weightCoefficient"), m_weightCoefficient->value()}
         }},
        {QStringLiteral("qualityParameters"), QJsonObject{
             {QStringLiteral("rmseLimit"), m_rmseLimit->value()},
             {QStringLiteral("maxErrorLimit"), m_maxErrorLimit->value()},
             {QStringLiteral("rotationRmseLimitMm"),
              m_rotationRmseLimit->value()},
             {QStringLiteral("rotationMaxErrorLimitMm"),
              m_rotationMaxErrorLimit->value()}
         }},
        {QStringLiteral("regionParameters"), QJsonObject{
             {QStringLiteral("safeMarginPx"), m_safeMarginPx->value()}
         }}
    };
}

bool NPointCalibrationConfigWidget::restorePersistentSettings(
        const QJsonObject &settings, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    if (settings.isEmpty())
        return true;
    if (settings.value(QStringLiteral("version")).toInt(1) != 1) {
        if (errorMessage)
            *errorMessage = tr("N点稳定配置版本不兼容");
        return false;
    }

    const int translationCount = settings.value(QStringLiteral("translationCount"))
            .toInt(m_translationCount->value());
    const int rotationCount = settings.value(QStringLiteral("rotationCount"))
            .toInt(m_rotationCount->value());
    const QString storedMode = settings.value(QStringLiteral("calibrationMode"))
            .toString();
    const QString mode = storedMode.isEmpty()
            ? calibrationModeForCounts(translationCount, rotationCount)
            : storedMode;
    const int expectedRotationCount = mode == kNinePointXY ? 0 : 3;
    if (!isKnownCalibrationMode(mode)
            || translationCount != 9
            || rotationCount != expectedRotationCount) {
        if (errorMessage)
            *errorMessage = tr("N点标定模式与点数无效；仅支持9+0或9+3");
        return false;
    }

    const QJsonObject physical = settings.value(
                QStringLiteral("physicalCoordinateParameters")).toObject();
    const QJsonObject runtime = settings.value(
                QStringLiteral("runtimeParameters")).toObject();
    const QJsonObject quality = settings.value(
                QStringLiteral("qualityParameters")).toObject();
    const QJsonObject region = settings.value(
                QStringLiteral("regionParameters")).toObject();
    const auto validDouble = [](const QJsonObject &object, const QString &key,
                                const QDoubleSpinBox *spin) {
        if (!object.contains(key))
            return true;
        return isFiniteJsonNumber(object.value(key))
                && object.value(key).toDouble() >= spin->minimum()
                && object.value(key).toDouble() <= spin->maximum();
    };
    const auto validInt = [](const QJsonObject &object, const QString &key,
                             const QSpinBox *spin) {
        if (!object.contains(key))
            return true;
        return isJsonIntegerInRange(object.value(key), spin->minimum(), spin->maximum());
    };
    if (!validDouble(physical, QStringLiteral("referenceX"), m_referenceX)
            || !validDouble(physical, QStringLiteral("referenceY"), m_referenceY)
            || !validDouble(physical, QStringLiteral("offsetX"), m_offsetX)
            || !validDouble(physical, QStringLiteral("offsetY"), m_offsetY)
            || !validInt(physical, QStringLiteral("directionChangeCount"),
                         m_directionChangeCount)
            || !validDouble(physical, QStringLiteral("referenceAngle"),
                            m_referenceAngle)
            || !validDouble(physical, QStringLiteral("angleOffset"), m_angleOffset)
            || !validInt(physical, QStringLiteral("calibrationOrigin"),
                         m_calibrationOrigin)
            || !validInt(runtime, QStringLiteral("weightCoefficient"),
                         m_weightCoefficient)
            || !validDouble(quality, QStringLiteral("rmseLimit"), m_rmseLimit)
            || !validDouble(quality, QStringLiteral("maxErrorLimit"),
                            m_maxErrorLimit)
            || !validDouble(quality, QStringLiteral("rotationRmseLimitMm"),
                            m_rotationRmseLimit)
            || !validDouble(quality,
                            QStringLiteral("rotationMaxErrorLimitMm"),
                            m_rotationMaxErrorLimit)
            || !validDouble(region, QStringLiteral("safeMarginPx"),
                            m_safeMarginPx)) {
        if (errorMessage)
            *errorMessage = tr("N点稳定配置包含超出范围或非有限参数");
        return false;
    }

    const auto validateCombo = [](const QJsonObject &object, const QString &key,
                                  const QComboBox *combo) {
        return !object.contains(key)
                || comboIndexForData(combo, object.value(key).toString()) >= 0;
    };
    if (!validateCombo(physical, QStringLiteral("movePriority"), m_movePriority)
            || !validateCombo(runtime, QStringLiteral("cameraMotionMode"),
                              m_cameraMotionMode)
            || !validateCombo(runtime, QStringLiteral("degreesOfFreedom"),
                              m_degreesOfFreedom)
            || !validateCombo(runtime, QStringLiteral("weightFunction"),
                              m_weightFunction)) {
        if (errorMessage)
            *errorMessage = tr("N点稳定配置包含未知枚举值");
        return false;
    }

    {
        const QSignalBlocker modeBlocker(m_calibrationMode);
        const QSignalBlocker translationBlocker(m_translationCount);
        const QSignalBlocker rotationBlocker(m_rotationCount);
        const int modeIndex = comboIndexForData(m_calibrationMode, mode);
        if (modeIndex >= 0)
            m_calibrationMode->setCurrentIndex(modeIndex);
        m_translationCount->setValue(translationCount);
        m_rotationCount->setValue(rotationCount);
    }
    m_lastCalibrationMode = mode;
    updateCalibrationModeUi();
    fillDefaultGrid();
    const auto setDouble = [](const QJsonObject &object, const QString &key,
                              QDoubleSpinBox *spin) {
        if (object.contains(key))
            spin->setValue(object.value(key).toDouble());
    };
    const auto setInt = [](const QJsonObject &object, const QString &key,
                           QSpinBox *spin) {
        if (object.contains(key))
            spin->setValue(object.value(key).toInt());
    };
    const auto setCombo = [](const QJsonObject &object, const QString &key,
                             QComboBox *combo) {
        if (!object.contains(key))
            return;
        const int index = comboIndexForData(combo, object.value(key).toString());
        if (index >= 0)
            combo->setCurrentIndex(index);
    };
    setDouble(physical, QStringLiteral("referenceX"), m_referenceX);
    setDouble(physical, QStringLiteral("referenceY"), m_referenceY);
    setDouble(physical, QStringLiteral("offsetX"), m_offsetX);
    setDouble(physical, QStringLiteral("offsetY"), m_offsetY);
    setCombo(physical, QStringLiteral("movePriority"), m_movePriority);
    setInt(physical, QStringLiteral("directionChangeCount"), m_directionChangeCount);
    setDouble(physical, QStringLiteral("referenceAngle"), m_referenceAngle);
    setDouble(physical, QStringLiteral("angleOffset"), m_angleOffset);
    setInt(physical, QStringLiteral("calibrationOrigin"), m_calibrationOrigin);
    setCombo(runtime, QStringLiteral("cameraMotionMode"), m_cameraMotionMode);
    setCombo(runtime, QStringLiteral("degreesOfFreedom"), m_degreesOfFreedom);
    setCombo(runtime, QStringLiteral("weightFunction"), m_weightFunction);
    setInt(runtime, QStringLiteral("weightCoefficient"), m_weightCoefficient);
    setDouble(quality, QStringLiteral("rmseLimit"), m_rmseLimit);
    setDouble(quality, QStringLiteral("maxErrorLimit"), m_maxErrorLimit);
    setDouble(quality, QStringLiteral("rotationRmseLimitMm"),
              m_rotationRmseLimit);
    setDouble(quality, QStringLiteral("rotationMaxErrorLimitMm"),
              m_rotationMaxErrorLimit);
    {
        const QSignalBlocker safeMarginBlocker(m_safeMarginPx);
        setDouble(region, QStringLiteral("safeMarginPx"), m_safeMarginPx);
    }

    const QJsonObject bindings = settings.value(QStringLiteral("captureBindings")).toObject();
    for (const QString &fieldKey : {kImagePointX, kImagePointY, kImageAngle,
                                    kPhysicalPointX, kPhysicalPointY, kPhysicalAngle}) {
        if (bindings.value(fieldKey).isObject())
            setCaptureBinding(fieldKey, bindings.value(fieldKey).toObject());
    }
    const QString captureMode = bindings.value(QStringLiteral("captureMode"))
            .toString(QStringLiteral("trigger"));
    if (captureMode == QStringLiteral("manual"))
        m_manualCaptureButton->setChecked(true);
    else
        m_triggerCaptureButton->setChecked(true);
    updateCaptureModeUi();
    setParameterMode(settings.value(QStringLiteral("parameterMode")).toString()
                     == QStringLiteral("all"));
    return true;
}

QJsonObject NPointCalibrationConfigWidget::draftState() const
{
    QJsonArray samples;
    for (int row = 0; row < m_sampleTable->rowCount(); ++row) {
        QJsonObject sample{
            {QStringLiteral("index"), row + 1},
            {QStringLiteral("type"),
             row < m_translationCount->value()
                ? QStringLiteral("translation") : QStringLiteral("rotation")},
            {QStringLiteral("completed"),
             row < m_sampleCompleted.size() && m_sampleCompleted.at(row)}
        };
        const QStringList keys{QStringLiteral("column"), QStringLiteral("row"),
                               QStringLiteral("imageAngleDeg"), QStringLiteral("machineX"),
                               QStringLiteral("machineY"), QStringLiteral("machineAngleDeg")};
        for (int column = 1; column <= 6; ++column) {
            bool ok = false;
            const double value = m_sampleTable->item(row, column)
                    ? m_sampleTable->item(row, column)->text().toDouble(&ok) : 0.0;
            sample.insert(keys.at(column - 1),
                          ok && std::isfinite(value)
                          ? QJsonValue(value) : QJsonValue(0.0));
        }
        samples.append(sample);
    }
    return QJsonObject{
        {QStringLiteral("version"), 3},
        {QStringLiteral("calibrationMode"), calibrationMode()},
        {QStringLiteral("translationCount"), m_translationCount->value()},
        {QStringLiteral("rotationCount"), m_rotationCount->value()},
        {QStringLiteral("sampleCount"), m_sampleTable->rowCount()},
        {QStringLiteral("nextCaptureRow"), m_nextCaptureRow},
        {QStringLiteral("completedSampleCount"), completedSampleCount()},
        {QStringLiteral("samples"), samples}
    };
}

bool NPointCalibrationConfigWidget::restoreDraftState(
        const QJsonObject &state, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    const int version = state.value(QStringLiteral("version")).toInt();
    if (version != 1 && version != 2 && version != 3) {
        if (errorMessage)
            *errorMessage = tr("N点草稿版本不兼容");
        return false;
    }
    const int count = state.value(QStringLiteral("sampleCount")).toInt();
    const int translationCount = version == 1
            ? count : state.value(QStringLiteral("translationCount")).toInt();
    const int rotationCount = version == 1
            ? 0 : state.value(QStringLiteral("rotationCount")).toInt();
    const QString mode = version >= 3
            ? state.value(QStringLiteral("calibrationMode")).toString()
            : calibrationModeForCounts(translationCount, rotationCount);
    const int expectedRotationCount = mode == kNinePointXY ? 0 : 3;
    const QJsonArray samples = state.value(QStringLiteral("samples")).toArray();
    if ((version >= 3 && mode != calibrationMode())
            || !isKnownCalibrationMode(mode)
            || translationCount != 9
            || rotationCount != expectedRotationCount
            || count != translationCount + rotationCount
            || samples.size() != count) {
        if (errorMessage)
            *errorMessage = version >= 3 && mode != calibrationMode()
                    ? tr("N点草稿标定模式与当前稳定配置不一致")
                    : tr("N点草稿行数无效");
        return false;
    }
    const QStringList keys{QStringLiteral("column"), QStringLiteral("row"),
                           QStringLiteral("imageAngleDeg"), QStringLiteral("machineX"),
                           QStringLiteral("machineY"), QStringLiteral("machineAngleDeg")};
    QVector<QVector<double>> values;
    QVector<bool> completed;
    values.reserve(count);
    completed.reserve(count);
    for (int row = 0; row < count; ++row) {
        const QJsonObject sample = samples.at(row).toObject();
        const QString expectedType = row < translationCount
                ? QStringLiteral("translation") : QStringLiteral("rotation");
        if (sample.value(QStringLiteral("index")).toInt() != row + 1
                || (version >= 2
                    && sample.value(QStringLiteral("type")).toString() != expectedType)
                || !sample.value(QStringLiteral("completed")).isBool()) {
            if (errorMessage)
                *errorMessage = tr("N点草稿第 %1 行状态无效").arg(row + 1);
            return false;
        }
        QVector<double> rowValues;
        for (const QString &key : keys) {
            if (!isFiniteJsonNumber(sample.value(key))) {
                if (errorMessage)
                    *errorMessage = tr("N点草稿第 %1 行坐标无效").arg(row + 1);
                return false;
            }
            rowValues.append(sample.value(key).toDouble());
        }
        values.append(rowValues);
        completed.append(sample.value(QStringLiteral("completed")).toBool());
    }

    {
        const QSignalBlocker modeBlocker(m_calibrationMode);
        const QSignalBlocker translationBlocker(m_translationCount);
        const QSignalBlocker rotationBlocker(m_rotationCount);
        const int modeIndex = comboIndexForData(m_calibrationMode, mode);
        if (modeIndex >= 0)
            m_calibrationMode->setCurrentIndex(modeIndex);
        m_translationCount->setValue(translationCount);
        m_rotationCount->setValue(rotationCount);
    }
    m_lastCalibrationMode = mode;
    updateCalibrationModeUi();
    m_sampleTable->setRowCount(count);
    m_sampleCompleted = completed;
    for (int row = 0; row < count; ++row) {
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        m_sampleTable->setItem(row, 0, indexItem);
        for (int column = 0; column < values.at(row).size(); ++column)
            m_sampleTable->setItem(row, column + 1, numberItem(values.at(row).at(column)));
    }
    resetCaptureCursorToFirstIncomplete();
    m_sampleStatus->setText(tr("已恢复 %1/%2 个标定点")
                            .arg(completedSampleCount()).arg(count));
    emit sampleDataChanged();
    return true;
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
    QStringList requiredFields{kImagePointX, kImagePointY};
    if (requiresImageAngle())
        requiredFields.append(kImageAngle);
    for (const QString &fieldKey : std::as_const(requiredFields)) {
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
    const bool currentPointIsRotation = m_nextCaptureRow >= 0
            && m_translationCount
            && m_nextCaptureRow >= m_translationCount->value();
    const bool angleRequired = calibrationMode() != kNinePointXY
            && currentPointIsRotation;
    const bool angleFinite = std::isfinite(angleDeg);
    m_physicalSampleValid = valid && std::isfinite(x) && std::isfinite(y)
            && (!angleRequired || angleFinite);
    m_physicalX = x;
    m_physicalY = y;
    m_physicalAngle = angleFinite ? angleDeg : 0.0;
}

QString NPointCalibrationConfigWidget::captureImageProducerId() const
{
    if (!m_triggerCaptureButton || !m_triggerCaptureButton->isChecked())
        return QString();

    QString producerId;
    QStringList requiredFields{kImagePointX, kImagePointY};
    if (requiresImageAngle())
        requiredFields.append(kImageAngle);
    for (const QString &fieldKey : std::as_const(requiredFields)) {
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
    if (m_pointEditorOpen) {
        if (errorMessage)
            *errorMessage = tr("正在编辑标定点，暂不接受触发采样");
        emit sampleStateChanged(errorMessage ? *errorMessage
                                             : tr("正在编辑标定点"), false);
        return false;
    }
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
    const bool imageAngleAvailable =
            snapshot.payload.value(QStringLiteral("angle")).isDouble()
            && std::isfinite(snapshot.payload.value(QStringLiteral("angle")).toDouble());
    const double imageAngle = imageAngleAvailable
            ? snapshot.payload.value(QStringLiteral("angle")).toDouble() : 0.0;
    if (!snapshot.valid || !snapshot.payload.value(QStringLiteral("x")).isDouble()
            || !snapshot.payload.value(QStringLiteral("y")).isDouble()
            || !std::isfinite(imageX) || !std::isfinite(imageY)
            || (requiresImageAngle() && !imageAngleAvailable)) {
        if (errorMessage)
            *errorMessage = requiresImageAngle()
                    ? tr("位置与姿态模式要求前序工具返回有效 X、Y 和图像角度")
                    : tr("所选前序工具尚无有效图像 X、Y");
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
    if (m_sampleCompleted.size() != m_sampleTable->rowCount())
        m_sampleCompleted.resize(m_sampleTable->rowCount());
    m_sampleCompleted[row] = true;
    resetCaptureCursorToFirstIncomplete();
    m_sampleStatus->setText(tr("第 %1 点采样完成：图像(%2,%3)%4")
                            .arg(row + 1)
                            .arg(imageX, 0, 'f', 3)
                            .arg(imageY, 0, 'f', 3)
                            .arg(m_physicalSampleValid ? tr("，已合并通信物理 X/Y/A")
                                                       : tr("，物理坐标沿用表格值")));
    emit sampleStateChanged(m_sampleStatus->text(), true);
    emit sampleDataChanged();
    return true;
}

bool NPointCalibrationConfigWidget::importPoints(
        QTableWidget *editor,
        int *editorTranslationCount,
        int *editorRotationCount,
        QVector<bool> *editorCompleted,
        bool *retryRequested)
{
    if (!editor || !editorTranslationCount || !editorRotationCount
            || !editorCompleted || !retryRequested) {
        return false;
    }
    *retryRequested = false;
    const QString path = QFileDialog::getOpenFileName(
                this, tr("导入标定点"), QString(), tr("点集文件 (*.txt *.csv)"));
    if (path.isEmpty())
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导入失败"), tr("无法读取点集文件"));
        return false;
    }
    QVector<QStringList> rows;
    QVector<QString> rowTypes;
    QVector<bool> rowCompleted;
    QVector<bool> rowHasAngleColumns;
    bool hasExplicitPointType = false;
    bool hasUntypedPoint = false;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        QStringList values = line.split(QRegExp(QStringLiteral("[,;\\s]+")),
                                        Qt::SkipEmptyParts);
        QString type = QStringLiteral("translation");
        bool rowHasExplicitPointType = false;
        const QString first = values.value(0).trimmed().toLower();
        if (first == QStringLiteral("translation") || first == tr("平移")) {
            hasExplicitPointType = true;
            rowHasExplicitPointType = true;
            values.removeFirst();
        } else if (first == QStringLiteral("rotation") || first == tr("旋转")) {
            hasExplicitPointType = true;
            rowHasExplicitPointType = true;
            type = QStringLiteral("rotation");
            values.removeFirst();
        }
        bool firstValueIsNumber = false;
        values.value(0).toDouble(&firstValueIsNumber);
        if (!firstValueIsNumber && rows.isEmpty())
            continue;
        if (values.size() != 4 && values.size() != 6
                && values.size() != 7) {
            QMessageBox::warning(
                        this, tr("导入失败"),
                        tr("每行必须为4列简格式、6列完整格式，或6列坐标加完成状态"));
            return false;
        }
        bool completed = true;
        if (values.size() == 7) {
            const QString token = values.takeLast().trimmed().toLower();
            if (token == QStringLiteral("1") || token == QStringLiteral("true")
                    || token == QStringLiteral("yes")
                    || token == QStringLiteral("completed")
                    || token == tr("完成")) {
                completed = true;
            } else if (token == QStringLiteral("0")
                       || token == QStringLiteral("false")
                       || token == QStringLiteral("no")
                       || token == QStringLiteral("incomplete")
                       || token == tr("未完成")) {
                completed = false;
            } else {
                QMessageBox::warning(
                            this, tr("导入失败"),
                            tr("完成状态必须为 1/0、true/false 或 完成/未完成"));
                return false;
            }
        }
        const bool hasAngleColumns = values.size() == 6;
        if (!rowHasExplicitPointType)
            hasUntypedPoint = true;
        rows.append(values);
        rowTypes.append(type);
        rowCompleted.append(completed);
        rowHasAngleColumns.append(hasAngleColumns);
    }
    file.close();
    if (hasExplicitPointType && hasUntypedPoint) {
        QMessageBox::warning(
                    this, tr("导入失败"),
                    tr("类型列必须全部填写或全部省略，不能混合导入"));
        return false;
    }

    const int expectedTranslationCount = *editorTranslationCount;
    const int expectedRotationCount = *editorRotationCount;
    const int expectedPointCount = expectedTranslationCount
            + expectedRotationCount;
    int importedTranslationCount = 0;
    int importedRotationCount = 0;
    bool rotationStarted = false;
    if (hasExplicitPointType) {
        for (const QString &type : rowTypes) {
            if (type == QStringLiteral("rotation")) {
                rotationStarted = true;
                ++importedRotationCount;
            } else {
                if (rotationStarted) {
                    QMessageBox::warning(this, tr("导入失败"),
                                         tr("平移点必须排列在旋转点之前"));
                    return false;
                }
                ++importedTranslationCount;
            }
        }
    }

    auto handlePointCountMismatch = [&]() {
        const QString importedSummary = hasExplicitPointType
                ? tr("平移 %1 点、旋转 %2 点，共 %3 点")
                  .arg(importedTranslationCount)
                  .arg(importedRotationCount)
                  .arg(rows.size())
                : tr("共 %1 点（文件未包含类型列）").arg(rows.size());
        QMessageBox warning(QMessageBox::Warning,
                            tr("标定点数量不一致"),
                            tr("当前已选择：平移 %1 点、旋转 %2 点，共 %3 点。\n"
                               "导入文件：%4。\n\n"
                               "导入未生效。请选择重新导入，或返回配置页重新选择标定模式。")
                            .arg(expectedTranslationCount)
                            .arg(expectedRotationCount)
                            .arg(expectedPointCount)
                            .arg(importedSummary),
                            QMessageBox::NoButton, editor->window());
        QPushButton *reimportButton = warning.addButton(
                    tr("重新导入"), QMessageBox::AcceptRole);
        QPushButton *reselectButton = warning.addButton(
                    tr("重新选择标定模式"), QMessageBox::ActionRole);
        QPushButton *cancelButton = warning.addButton(
                    tr("取消"), QMessageBox::RejectRole);
        warning.setDefaultButton(reimportButton);
        warning.setEscapeButton(cancelButton);
        warning.exec();
        if (warning.clickedButton() == reselectButton) {
            if (QDialog *pointDialog = qobject_cast<QDialog *>(editor->window()))
                pointDialog->reject();
            return false;
        }
        return warning.clickedButton() == reimportButton;
    };

    const bool countMismatch = rows.size() != expectedPointCount
            || (hasExplicitPointType
                && (importedTranslationCount != expectedTranslationCount
                    || importedRotationCount != expectedRotationCount));
    if (countMismatch) {
        const bool reimport = handlePointCountMismatch();
        *retryRequested = reimport;
        return false;
    }

    if (!hasExplicitPointType) {
        rowTypes.clear();
        rowTypes.reserve(rows.size());
        for (int row = 0; row < rows.size(); ++row) {
            rowTypes.append(row < expectedTranslationCount
                            ? QStringLiteral("translation")
                            : QStringLiteral("rotation"));
        }
    }
    QVector<QStringList> normalizedRows;
    normalizedRows.reserve(rows.size());
    QVector<double> importedImageAngles;
    QVector<double> importedMachineAngles;
    for (int row = 0; row < rows.size(); ++row) {
        const QStringList &values = rows.at(row);
        const bool rotationRow = rowTypes.at(row) == QStringLiteral("rotation");
        if (rotationRow && calibrationMode() != kNinePointXY
                && !rowHasAngleColumns.at(row)) {
            QMessageBox::warning(
                        this, tr("导入失败"),
                        calibrationMode() == kTwelvePointPoseMapping
                        ? tr("位置与姿态模式的旋转点必须包含图像角度和机械角度；导入未生效")
                        : tr("轴轨迹模式的旋转点必须包含机械角度 A；导入未生效"));
            return false;
        }
        const QStringList normalized = values.size() >= 6
                ? values.mid(0, 6)
                : QStringList{values.at(0), values.at(1), QStringLiteral("0"),
                              values.at(2), values.at(3), QStringLiteral("0")};
        for (int column = 0; column < normalized.size(); ++column) {
            bool ok = false;
            const double numeric = normalized.at(column).toDouble(&ok);
            if (!ok || !std::isfinite(numeric)) {
                QMessageBox::warning(
                            this, tr("导入失败"),
                            tr("第 %1 行第 %2 个坐标不是有限浮点数")
                            .arg(row + 1).arg(column + 1));
                return false;
            }
        }
        if (rotationRow) {
            importedMachineAngles.append(normalized.at(5).toDouble());
            if (calibrationMode() == kTwelvePointPoseMapping)
                importedImageAngles.append(normalized.at(2).toDouble());
        }
        normalizedRows.append(normalized);
    }
    if (calibrationMode() != kNinePointXY
            && !anglesAreObservable(importedMachineAngles)) {
        QMessageBox::warning(
                    this, tr("导入失败"),
                    tr("3个旋转点的机械角度不可区分或跨度不足5°；导入未生效，请重新导入或重新选择标定模式"));
        return false;
    }
    if (calibrationMode() == kTwelvePointPoseMapping
            && !anglesAreObservable(importedImageAngles)) {
        QMessageBox::warning(
                    this, tr("导入失败"),
                    tr("旋转点的图像角度不可观测（需3个独立角度且跨度不少于5°）；导入未生效，不会自动降级为轴轨迹模式"));
        return false;
    }
    *editorCompleted = rowCompleted;
    editor->setRowCount(normalizedRows.size());
    for (int row = 0; row < normalizedRows.size(); ++row) {
        const QStringList &normalized = normalizedRows.at(row);
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
        indexItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        indexItem->setTextAlignment(Qt::AlignCenter);
        editor->setItem(row, 0, indexItem);
        editor->setItem(row, 1, typeItem(
                            rowTypes.at(row) == QStringLiteral("translation")
                            ? tr("平移") : tr("旋转")));
        for (int column = 0; column < 6; ++column) {
            editor->setItem(row, column + 2,
                            numberItem(normalized.at(column).toDouble()));
        }
        editor->setItem(row, 8, completionItem(editorCompleted->at(row)));
    }
    return true;
}

void NPointCalibrationConfigWidget::exportPoints(
        const QTableWidget *editor,
        int translationCount)
{
    if (!editor)
        return;
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
    stream << "type,column,row,imageAngleDeg,machineX,machineY,machineAngleDeg,completed\n";
    for (int row = 0; row < editor->rowCount(); ++row) {
        QStringList values{row < translationCount
                           ? QStringLiteral("translation")
                           : QStringLiteral("rotation")};
        for (int column = 2; column <= 7; ++column) {
            const QTableWidgetItem *item = editor->item(row, column);
            const QString text = item ? item->text().trimmed() : QString();
            bool ok = text.isEmpty();
            const double value = text.isEmpty() ? 0.0 : text.toDouble(&ok);
            if (!ok || !std::isfinite(value)) {
                QMessageBox::warning(
                            this, tr("导出失败"),
                            tr("第 %1 行第 %2 列不是有限浮点数")
                            .arg(row + 1).arg(column + 1));
                file.cancelWriting();
                return;
            }
            values.append(QString::number(value, 'f', 6));
        }
        const QTableWidgetItem *completion = editor->item(row, 8);
        values.append(completion && completion->checkState() == Qt::Checked
                      ? QStringLiteral("1") : QStringLiteral("0"));
        stream << values.join(QLatin1Char(',')) << '\n';
    }
    if (!file.commit())
        QMessageBox::warning(this, tr("导出失败"), tr("点集文件提交失败"));
}
