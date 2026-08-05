#include "calibration/NPointCalibrationConfigWidget.h"

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
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegExp>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include <cmath>

namespace {

QTableWidgetItem *numberItem(double value)
{
    QTableWidgetItem *item = new QTableWidgetItem(QString::number(value, 'f', 6));
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

} // namespace

void NPointCalibrationConfigWidget::setParameterMode(bool showAll)
{
    m_basicButton->setChecked(!showAll);
    m_allButton->setChecked(showAll);
    m_advancedCard->setVisible(showAll);
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
    connect(resetButton, &QPushButton::clicked, this, [this, editor]() {
        const QSignalBlocker blocker(m_translationCount);
        m_translationCount->setValue(9);
        fillDefaultGrid(9);
        editor->setRowCount(9);
        for (int row = 0; row < 9; ++row) {
            for (int column = 0; column < 7; ++column)
                editor->setItem(row, column, new QTableWidgetItem(m_sampleTable->item(row, column)->text()));
        }
    });
    connect(clearButton, &QPushButton::clicked, editor, [editor]() {
        for (int row = 0; row < editor->rowCount(); ++row) {
            editor->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
            for (int column = 1; column < editor->columnCount(); ++column)
                editor->setItem(row, column, new QTableWidgetItem);
        }
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
    m_imageProducer->addItem(tr("手动输入图像坐标"), QJsonObject{
                                 {QStringLiteral("mode"), QStringLiteral("manual")}});
    m_translationCount = new QSpinBox(this);
    m_translationCount->setRange(3, 99);
    m_translationCount->setValue(9);
    m_rotationCount = new QSpinBox(this);
    m_rotationCount->setRange(0, 0);
    m_rotationCount->setValue(0);
    m_rotationCount->setToolTip(tr("N点方式首期只求二维平移仿射模型，旋转采样由平移旋转标定方式接入"));
    QLabel *translationLabel = new QLabel(tr("平移次数"), movementCard);
    QLabel *rotationLabel = new QLabel(tr("旋转次数"), movementCard);
    translationLabel->setProperty("role", QStringLiteral("rowField"));
    rotationLabel->setProperty("role", QStringLiteral("rowField"));
    movementForm->addRow(translationLabel, m_translationCount);
    movementForm->addRow(rotationLabel, m_rotationCount);
    QPushButton *editButton = new QPushButton(tr("编辑标定点"), movementCard);
    editButton->setProperty("actionRole", QStringLiteral("secondary"));
    QLabel *editLabel = new QLabel(tr("对应点列表"), movementCard);
    editLabel->setProperty("role", QStringLiteral("rowField"));
    movementForm->addRow(editLabel, editButton);
    movementLayout->addLayout(movementForm);
    layout->addWidget(movementCard);

    QFrame *captureCard = new QFrame(this);
    captureCard->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *captureLayout = new QVBoxLayout(captureCard);
    QLabel *captureTitle = new QLabel(tr("标定参数"), captureCard);
    captureTitle->setProperty("role", QStringLiteral("cardTitle"));
    captureLayout->addWidget(captureTitle);
    QFormLayout *captureForm = new QFormLayout;
    QLabel *sourceLabel = new QLabel(tr("图像点来源"), captureCard);
    sourceLabel->setProperty("role", QStringLiteral("rowField"));
    captureForm->addRow(sourceLabel, m_imageProducer);
    QPushButton *captureButton = new QPushButton(tr("触发获取当前点"), captureCard);
    captureButton->setProperty("actionRole", QStringLiteral("highlight"));
    QLabel *captureLabel = new QLabel(tr("标定点获取"), captureCard);
    captureLabel->setProperty("role", QStringLiteral("rowField"));
    captureForm->addRow(captureLabel, captureButton);
    m_sampleStatus = new QLabel(tr("等待采样；无设备模式可在对应点列表中手动编辑"), captureCard);
    m_sampleStatus->setProperty("role", QStringLiteral("cardHint"));
    m_sampleStatus->setWordWrap(true);
    captureLayout->addLayout(captureForm);
    captureLayout->addWidget(m_sampleStatus);
    layout->addWidget(captureCard);

    m_sampleTable = new QTableWidget(9, 7, this);
    m_sampleTable->setHorizontalHeaderLabels({tr("序号"), tr("Column(px)"), tr("Row(px)"),
                                               tr("图像角度"), tr("X(mm)"), tr("Y(mm)"), tr("物理角度")});
    m_sampleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_sampleTable->verticalHeader()->setVisible(false);
    m_sampleTable->hide();

    m_advancedCard = new QFrame(this);
    m_advancedCard->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *advancedLayout = new QVBoxLayout(m_advancedCard);
    QLabel *advancedTitle = new QLabel(tr("质量门限"), m_advancedCard);
    advancedTitle->setProperty("role", QStringLiteral("cardTitle"));
    advancedLayout->addWidget(advancedTitle);
    QFormLayout *thresholds = new QFormLayout;
    m_rmseLimit = new QDoubleSpinBox(this);
    m_rmseLimit->setDecimals(4);
    m_rmseLimit->setRange(0.0001, 1000.0);
    m_rmseLimit->setValue(0.10);
    m_maxErrorLimit = new QDoubleSpinBox(this);
    m_maxErrorLimit->setDecimals(4);
    m_maxErrorLimit->setRange(0.0001, 1000.0);
    m_maxErrorLimit->setValue(0.25);
    QLabel *rmseLabel = new QLabel(tr("RMSE门限(mm)"), m_advancedCard);
    QLabel *maxLabel = new QLabel(tr("最大误差门限(mm)"), m_advancedCard);
    rmseLabel->setProperty("role", QStringLiteral("rowField"));
    maxLabel->setProperty("role", QStringLiteral("rowField"));
    thresholds->addRow(rmseLabel, m_rmseLimit);
    thresholds->addRow(maxLabel, m_maxErrorLimit);
    advancedLayout->addLayout(thresholds);
    layout->addWidget(m_advancedCard);
    layout->addStretch();

    connect(editButton, &QPushButton::clicked, this, [this]() { editPoints(); });
    connect(captureButton, &QPushButton::clicked, this, [this]() {
        QString error;
        if (!captureCurrentSample(&error))
            QMessageBox::information(this, tr("触发获取"), error);
    });
    connect(m_basicButton, &QPushButton::clicked, this, [this]() { setParameterMode(false); });
    connect(m_allButton, &QPushButton::clicked, this, [this]() { setParameterMode(true); });
    connect(m_translationCount, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int count) { fillDefaultGrid(count); });
    fillDefaultGrid(9);
    setParameterMode(false);
}

void NPointCalibrationConfigWidget::fillDefaultGrid(int count)
{
    m_sampleTable->setRowCount(count);
    for (int row = 0; row < count; ++row) {
        m_sampleTable->setItem(row, 0, new QTableWidgetItem(QString::number(row + 1)));
        const int gridX = row % 3;
        const int gridY = row / 3;
        m_sampleTable->setItem(row, 1, numberItem(200.0 + gridX * 300.0));
        m_sampleTable->setItem(row, 2, numberItem(180.0 + gridY * 240.0));
        m_sampleTable->setItem(row, 3, numberItem(0.0));
        m_sampleTable->setItem(row, 4, numberItem(-50.0 + gridX * 50.0));
        m_sampleTable->setItem(row, 5, numberItem(-40.0 + gridY * 40.0));
        m_sampleTable->setItem(row, 6, numberItem(0.0));
    }
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
    return result;
}

void NPointCalibrationConfigWidget::setProducerSnapshots(
        const QVector<CalibrationProducerSnapshot> &snapshots)
{
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
}

void NPointCalibrationConfigWidget::setLatestPhysicalSample(
        bool valid, double x, double y, double angleDeg)
{
    m_physicalSampleValid = valid;
    m_physicalX = x;
    m_physicalY = y;
    m_physicalAngle = angleDeg;
}

bool NPointCalibrationConfigWidget::captureCurrentSample(QString *errorMessage)
{
    const int selected = m_imageProducer->currentIndex() - 1;
    if (selected < 0 || selected >= m_snapshots.size()) {
        if (errorMessage)
            *errorMessage = tr("当前为手动输入来源，请在“编辑标定点”中录入坐标");
        emit sampleStateChanged(errorMessage ? *errorMessage : tr("当前没有可触发的图像点来源"), false);
        return false;
    }
    const CalibrationProducerSnapshot &snapshot = m_snapshots.at(selected);
    if (!snapshot.valid || !snapshot.payload.value(QStringLiteral("x")).isDouble()
            || !snapshot.payload.value(QStringLiteral("y")).isDouble()) {
        if (errorMessage)
            *errorMessage = tr("所选前序工具尚无有效运行位姿");
        emit sampleStateChanged(errorMessage ? *errorMessage : tr("前序运行位姿无效"), false);
        return false;
    }
    int row = m_sampleTable->currentRow();
    if (row < 0)
        row = 0;
    m_sampleTable->setItem(row, 1, numberItem(snapshot.payload.value(QStringLiteral("x")).toDouble()));
    m_sampleTable->setItem(row, 2, numberItem(snapshot.payload.value(QStringLiteral("y")).toDouble()));
    m_sampleTable->setItem(row, 3, numberItem(snapshot.payload.value(QStringLiteral("angle")).toDouble()));
    if (m_physicalSampleValid) {
        m_sampleTable->setItem(row, 4, numberItem(m_physicalX));
        m_sampleTable->setItem(row, 5, numberItem(m_physicalY));
        m_sampleTable->setItem(row, 6, numberItem(m_physicalAngle));
    }
    m_sampleTable->selectRow(qMin(row + 1, m_sampleTable->rowCount() - 1));
    m_sampleStatus->setText(tr("第 %1 点采样完成：图像(%2,%3)%4")
                            .arg(row + 1)
                            .arg(snapshot.payload.value(QStringLiteral("x")).toDouble(), 0, 'f', 3)
                            .arg(snapshot.payload.value(QStringLiteral("y")).toDouble(), 0, 'f', 3)
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
