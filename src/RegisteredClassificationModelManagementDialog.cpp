#include "RegisteredClassificationModelManagementDialog.h"

#include "RegisteredClassificationTrainingDialog.h"
#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "ui_RegisteredClassificationModelManagementDialog.h"
#include "ui_CreateDatasetDialog.h"

#include <algorithm>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPolygonF>
#include <QPixmap>
#include <QPushButton>
#include <QSharedPointer>
#include <QSize>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGlobal>

#include <functional>

namespace {

struct ModelRecord
{
    QString modelDir;
    QString modelName;
    QString dateText;
    RegisteredClassificationModelInspection inspection;
    QDateTime lastModified;
    bool hasTrainingSession = false;
};

QSize initialDialogSize(QWidget *parent, const QSize &fallback)
{
    if (!parent)
        return fallback;

    QSize parentSize = parent->size();
    if (!parentSize.isValid() || parentSize.width() <= 0 || parentSize.height() <= 0)
        parentSize = parent->window()->size();
    if (!parentSize.isValid() || parentSize.width() <= 0 || parentSize.height() <= 0)
        return fallback;

    return QSize(qMax(740, qRound(parentSize.width() * 0.76)),
                 qMax(500, qRound(parentSize.height() * 0.76)));
}

QIcon datasetTypeIcon()
{
    QPixmap pixmap(110, 78);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#ff7a00")));
    painter.drawRoundedRect(QRectF(18, 18, 58, 42), 5, 5);
    painter.setBrush(QColor(QStringLiteral("#243241")));
    painter.drawRoundedRect(QRectF(26, 25, 42, 28), 3, 3);
    painter.setBrush(QColor(QStringLiteral("#ffb020")));
    painter.drawPolygon(QPolygonF()
                        << QPointF(30, 50)
                        << QPointF(43, 33)
                        << QPointF(57, 50));
    painter.setBrush(QColor(QStringLiteral("#0ea5e9")));
    painter.drawEllipse(QPointF(62, 30), 4, 4);
    painter.setBrush(QColor(QStringLiteral("#ff7a00")));
    for (int index = 0; index < 3; ++index)
        painter.drawRoundedRect(QRectF(82, 24 + index * 13, 24, 6), 2, 2);

    return QIcon(pixmap);
}

QIcon actionIcon(const QString &kind)
{
    QPixmap pixmap(36, 36);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(QStringLiteral("#0f172a")), 2.2));
    painter.setBrush(Qt::NoBrush);

    if (kind == QStringLiteral("rename")) {
        painter.drawRoundedRect(QRectF(8, 9, 16, 19), 2, 2);
        painter.drawLine(QPointF(12, 14), QPointF(20, 14));
        painter.drawLine(QPointF(12, 18), QPointF(20, 18));
        painter.drawLine(QPointF(12, 22), QPointF(18, 22));
        painter.setPen(QPen(QColor(QStringLiteral("#ff7a00")), 3.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(23, 25), QPointF(29, 19));
        painter.drawLine(QPointF(21, 27), QPointF(23, 25));
    } else if (kind == QStringLiteral("export")) {
        painter.setPen(QPen(QColor(QStringLiteral("#0ea5e9")), 2.3));
        painter.drawRoundedRect(QRectF(9, 22, 18, 7), 2, 2);
        painter.setPen(QPen(QColor(QStringLiteral("#22c55e")), 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(18, 7), QPointF(18, 22));
        painter.drawLine(QPointF(12, 16), QPointF(18, 22));
        painter.drawLine(QPointF(24, 16), QPointF(18, 22));
    } else {
        painter.setPen(QPen(QColor(QStringLiteral("#ef4444")), 3.2, Qt::SolidLine, Qt::RoundCap));
        painter.drawEllipse(QPointF(18, 18), 11, 11);
        painter.drawLine(QPointF(14, 14), QPointF(22, 22));
        painter.drawLine(QPointF(22, 14), QPointF(14, 22));
    }

    return QIcon(pixmap);
}

QFrame *datasetTile(QWidget *parent, const QString &name, const QString &detail)
{
    QFrame *tile = new QFrame(parent);
    tile->setProperty("panelRole", QStringLiteral("datasetTile"));
    QVBoxLayout *layout = new QVBoxLayout(tile);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);
    QLabel *nameLabel = new QLabel(name, tile);
    nameLabel->setProperty("role", QStringLiteral("itemTitle"));
    QLabel *detailLabel = new QLabel(detail, tile);
    detailLabel->setProperty("role", QStringLiteral("itemSubtle"));
    layout->addWidget(nameLabel);
    layout->addWidget(detailLabel);
    layout->addStretch(1);
    return tile;
}

QString registeredClassificationModelRoot()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(
                QStringLiteral("ModelFiles/RegisteredClass"));
}

QVector<ModelRecord> scanModelRecords()
{
    QVector<ModelRecord> records;
    const QDir root(registeredClassificationModelRoot());
    if (!root.exists())
        return records;

    const QFileInfoList dateDirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                      QDir::Name);
    for (const QFileInfo &dateDirInfo : dateDirs) {
        const QDir dateDir(dateDirInfo.absoluteFilePath());
        const QFileInfoList modelDirs = dateDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                              QDir::Time | QDir::Reversed);
        for (const QFileInfo &modelDirInfo : modelDirs) {
            const QString modelDir = modelDirInfo.absoluteFilePath();
            if (!QFileInfo::exists(registeredClassificationMetadataPath(modelDir)))
                continue;
            const RegisteredClassificationModelInspection inspection =
                    inspectRegisteredClassificationModelPackage(modelDir);
            if (!inspection.success) {
                qWarning().noquote() << QStringLiteral("Registered classification model skipped: %1 (%2)")
                                        .arg(modelDir, inspection.message);
                continue;
            }

            ModelRecord record;
            record.modelDir = modelDir;
            record.modelName = modelDirInfo.fileName();
            record.dateText = dateDirInfo.fileName();
            record.inspection = inspection;
            record.lastModified = modelDirInfo.lastModified();
            record.hasTrainingSession = inspection.hasTrainingSession;
            records.append(record);
        }
    }

    std::sort(records.begin(), records.end(), [](const ModelRecord &left, const ModelRecord &right) {
        return left.lastModified > right.lastModified;
    });
    return records;
}

QFrame *modelRow(QWidget *parent,
                 const ModelRecord &record,
                 int index,
                 const std::function<void(const ModelRecord &)> &useModel,
                 const std::function<void(const ModelRecord &)> &retrainModel)
{
    QFrame *row = new QFrame(parent);
    row->setProperty("panelRole", QStringLiteral("modelRow"));
    row->setProperty("modelDir", record.modelDir);
    row->setProperty("modelName", record.modelName);
    row->setObjectName(QStringLiteral("registeredClassificationModelRow_%1").arg(index));
    row->setToolTip(QObject::tr("%1\n重新训练状态：%2")
                    .arg(record.modelDir,
                         record.hasTrainingSession ? QObject::tr("可恢复历史数据")
                                                   : QObject::tr("缺少历史训练数据")));
    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);
    QVBoxLayout *texts = new QVBoxLayout;
    QLabel *nameLabel = new QLabel(record.modelName, row);
    nameLabel->setProperty("role", QStringLiteral("itemTitle"));
    const QString modelStatus = record.inspection.legacy
            ? QObject::tr("旧版，需重新训练")
            : (record.inspection.runnable ? QObject::tr("可运行")
                                          : QObject::tr("不可运行：%1").arg(record.inspection.status));
    const QString detail = QObject::tr("%1 / 类别 %2 / 样本 %3 / %4 / %5 / %6")
            .arg(record.dateText)
            .arg(record.inspection.classCount)
            .arg(record.inspection.trainingSampleCount)
            .arg(record.inspection.featureVersion)
            .arg(modelStatus)
            .arg(record.hasTrainingSession ? QObject::tr("可重新训练")
                                           : QObject::tr("缺少历史训练数据"));
    QLabel *detailLabel = new QLabel(detail, row);
    detailLabel->setProperty("role", QStringLiteral("itemSubtle"));
    texts->addWidget(nameLabel);
    texts->addWidget(detailLabel);
    layout->addLayout(texts, 1);

    QPushButton *useButton = new QPushButton(QObject::tr("使用模型"), row);
    useButton->setObjectName(QStringLiteral("useModelButton"));
    useButton->setProperty("actionRole", QStringLiteral("primary"));
    useButton->setMinimumHeight(44);
    useButton->setEnabled(record.inspection.runnable);
    useButton->setToolTip(record.inspection.runnable
                          ? QObject::tr("使用模型")
                          : QObject::tr("旧模型不能运行，请先重新训练"));
    layout->addWidget(useButton);
    QObject::connect(useButton, &QPushButton::clicked, row, [record, useModel]() {
        useModel(record);
    });

    QPushButton *retrainButton = new QPushButton(QObject::tr("重新训练"), row);
    retrainButton->setObjectName(QStringLiteral("retrainModelButton"));
    retrainButton->setProperty("actionRole", QStringLiteral("secondary"));
    retrainButton->setMinimumHeight(44);
    retrainButton->setEnabled(true);
    layout->addWidget(retrainButton);
    QObject::connect(retrainButton, &QPushButton::clicked, row, [record, retrainModel]() {
        retrainModel(record);
    });

    struct RowAction
    {
        QString objectName;
        QString iconKind;
        QString tooltip;
    };
    const QList<RowAction> actions = QList<RowAction>()
            << RowAction{QStringLiteral("renameModelButton"), QStringLiteral("rename"), QObject::tr("重命名")}
            << RowAction{QStringLiteral("exportModelButton"), QStringLiteral("export"), QObject::tr("导出")}
            << RowAction{QStringLiteral("deleteModelButton"), QStringLiteral("delete"), QObject::tr("删除")};
    for (const RowAction &action : actions) {
        QToolButton *button = new QToolButton(row);
        button->setObjectName(action.objectName);
        button->setToolTip(action.tooltip);
        button->setIcon(actionIcon(action.iconKind));
        button->setIconSize(QSize(26, 26));
        button->setMinimumSize(44, 44);
        button->setProperty("buttonRole", action.iconKind);
        layout->addWidget(button);
    }
    return row;
}

#if 0
class CreateDatasetLegacyDialog : public QDialog
{
public:
    explicit CreateDatasetLegacyDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QObject::tr("创建数据集"));
        setModal(true);
        resize(620, 420);
        setMinimumSize(560, 360);

        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        QFrame *header = new QFrame(this);
        header->setProperty("panelRole", QStringLiteral("datasetDialogHeader"));
        QHBoxLayout *headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(24, 12, 18, 12);
        QLabel *title = new QLabel(QObject::tr("创建数据集"), header);
        title->setProperty("role", QStringLiteral("dialogTitle"));
        QToolButton *closeButton = new QToolButton(header);
        closeButton->setText(QStringLiteral("×"));
        closeButton->setProperty("buttonRole", QStringLiteral("closeButton"));
        closeButton->setMinimumSize(36, 36);
        headerLayout->addWidget(title);
        headerLayout->addStretch(1);
        headerLayout->addWidget(closeButton);
        root->addWidget(header);

        QFrame *content = new QFrame(this);
        content->setObjectName(QStringLiteral("datasetDialogContent"));
        content->setProperty("panelRole", QStringLiteral("datasetDialogContent"));
        QVBoxLayout *contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(34, 28, 34, 28);
        contentLayout->setSpacing(22);

        QHBoxLayout *nameLayout = new QHBoxLayout;
        QLabel *nameLabel = new QLabel(QObject::tr("数据集名称"), content);
        nameLabel->setProperty("role", QStringLiteral("fieldLabel"));
        QLabel *required = new QLabel(QStringLiteral("*"), content);
        required->setProperty("role", QStringLiteral("requiredMark"));
        m_nameEdit = new QLineEdit(content);
        m_nameEdit->setText(QStringLiteral("Untitled0"));
        m_nameEdit->setMinimumHeight(46);
        nameLayout->addWidget(nameLabel);
        nameLayout->addWidget(required);
        nameLayout->addSpacing(16);
        nameLayout->addWidget(m_nameEdit, 1);
        contentLayout->addLayout(nameLayout);

        QLabel *typeLabel = new QLabel(QObject::tr("选择模型训练类型"), content);
        typeLabel->setProperty("role", QStringLiteral("fieldLabel"));
        contentLayout->addWidget(typeLabel);

        QFrame *typeTile = new QFrame(content);
        typeTile->setProperty("panelRole", QStringLiteral("datasetTypeTile"));
        QVBoxLayout *typeLayout = new QVBoxLayout(typeTile);
        typeLayout->setContentsMargins(24, 22, 24, 22);
        QLabel *iconLabel = new QLabel(typeTile);
        iconLabel->setPixmap(datasetTypeIcon().pixmap(110, 78));
        QLabel *typeName = new QLabel(QObject::tr("注册分类"), typeTile);
        typeName->setProperty("role", QStringLiteral("datasetTypeName"));
        typeLayout->addWidget(iconLabel);
        typeLayout->addStretch(1);
        typeLayout->addWidget(typeName);
        contentLayout->addWidget(typeTile, 0, Qt::AlignLeft);
        contentLayout->addStretch(1);
        root->addWidget(content, 1);

        QFrame *footer = new QFrame(this);
        footer->setProperty("panelRole", QStringLiteral("datasetDialogFooter"));
        QHBoxLayout *footerLayout = new QHBoxLayout(footer);
        footerLayout->setContentsMargins(24, 14, 24, 14);
        QPushButton *okButton = new QPushButton(QObject::tr("确定"), footer);
        QPushButton *cancelButton = new QPushButton(QObject::tr("取消"), footer);
        okButton->setProperty("actionRole", QStringLiteral("confirm"));
        cancelButton->setProperty("actionRole", QStringLiteral("cancel"));
        okButton->setMinimumSize(150, 48);
        cancelButton->setMinimumSize(150, 48);
        footerLayout->addStretch(1);
        footerLayout->addWidget(okButton);
        footerLayout->addWidget(cancelButton);
        root->addWidget(footer);

        connect(closeButton, &QToolButton::clicked, this, &QDialog::reject);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        connect(okButton, &QPushButton::clicked, this, &QDialog::accept);

        setStyleSheet(QStringLiteral(
            "QDialog{background:#ffffff;color:#111827;font-size:20px;}"
            "QFrame[panelRole=\"datasetDialogHeader\"]{background:#4b5563;border-top:4px solid #ff7a00;}"
            "QFrame[panelRole=\"datasetDialogContent\"]{background:#ffffff;}"
            "QFrame[panelRole=\"datasetDialogFooter\"]{background:#e5e7eb;}"
            "QFrame[panelRole=\"datasetTypeTile\"]{background:#27323f;border:4px solid #ff7a00;border-radius:0;}"
            "QLabel{background:transparent;color:#111827;font-size:20px;}"
            "QLabel[role=\"dialogTitle\"]{color:#ffffff;font-size:22px;font-weight:700;}"
            "QLabel[role=\"fieldLabel\"]{color:#374151;font-size:21px;font-weight:600;}"
            "QLabel[role=\"requiredMark\"]{color:#ef4444;font-size:24px;font-weight:800;}"
            "QLabel[role=\"datasetTypeName\"]{color:#ffffff;font-size:22px;font-weight:800;}"
            "QLineEdit{background:#ffffff;color:#111827;border:2px solid #cbd5e1;border-radius:2px;padding:8px;font-size:22px;}"
            "QPushButton{background:#ffffff;color:#1f2937;border:2px solid #cbd5e1;border-radius:3px;font-size:20px;font-weight:700;}"
            "QPushButton[actionRole=\"confirm\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;}"
            "QPushButton[actionRole=\"cancel\"]{background:#ffffff;color:#374151;border-color:#cbd5e1;}"
            "QToolButton[buttonRole=\"closeButton\"]{background:transparent;color:#ffffff;border:0;font-size:28px;font-weight:800;}"));
    }

private:
    QLineEdit *m_nameEdit = nullptr;
};
#endif

class CreateDatasetDialog : public QDialog
{
public:
    explicit CreateDatasetDialog(QWidget *parent = nullptr)
        : QDialog(parent),
          ui(new Ui::CreateDatasetDialog)
    {
        ui->setupUi(this);
        m_nameEdit = ui->datasetNameLineEdit;
        ui->datasetTypeIconLabel->setPixmap(datasetTypeIcon().pixmap(72, 48));
        connect(ui->closeButton, &QToolButton::clicked, this, &QDialog::reject);
        connect(ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        connect(ui->okButton, &QPushButton::clicked, this, &QDialog::accept);
    }

    ~CreateDatasetDialog() override
    {
        delete ui;
    }

private:
    Ui::CreateDatasetDialog *ui = nullptr;
    QLineEdit *m_nameEdit = nullptr;
};

} // namespace

RegisteredClassificationModelManagementDialog::RegisteredClassificationModelManagementDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::RegisteredClassificationModelManagementDialog)
{
    ui->setupUi(this);
    resize(initialDialogSize(parent, QSize(1040, 640)));
    ui->createDatasetButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    ui->importDatasetButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    ui->datasetGrid->addWidget(datasetTile(ui->datasetCard, tr("默认数据集"), tr("0 张图像 / 0 个标注")), 0, 0);

    QPushButton *createDatasetButton = ui->createDatasetButton;
    QFrame *modelCard = ui->modelCard;
    QVBoxLayout *modelListLayout = ui->modelListLayout;

    auto clearModelList = [modelListLayout]() {
        while (QLayoutItem *item = modelListLayout->takeAt(0)) {
            if (QWidget *widget = item->widget())
                widget->deleteLater();
            delete item;
        }
    };

    QSharedPointer<std::function<void(const QString &)>> refreshModelList(
                new std::function<void(const QString &)>);
    auto openTrainingForRecord = [this, refreshModelList](const ModelRecord &record) {
        QWidget *trainingParent = parentWidget() ? parentWidget() : this;
        auto *trainingDialog = new RegisteredClassificationTrainingDialog(trainingParent);
        trainingDialog->setObjectName(QStringLiteral("registeredClassificationRetrainDialog"));
        trainingDialog->setProperty("updateTargetModelDir", record.modelDir);
        trainingDialog->setAttribute(Qt::WA_DeleteOnClose);
        trainingDialog->setUpdateTargetModelDir(record.modelDir);
        connect(trainingDialog, &RegisteredClassificationTrainingDialog::trainingCompleted,
                this, [this, refreshModelList, record](const QString &modelDir, const QString &modelName) {
            if (*refreshModelList)
                (*refreshModelList)(modelDir);
            const RegisteredClassificationModelInspection inspection =
                    inspectRegisteredClassificationModelPackage(modelDir);
            if (inspection.success && inspection.runnable) {
                emit modelSelected(modelDir,
                                   modelName.trimmed().isEmpty() ? record.modelName : modelName);
            }
        });
        trainingDialog->show();
    };

    *refreshModelList = [this, modelCard, modelListLayout, clearModelList, openTrainingForRecord](const QString &selectedModelDir) {
        clearModelList();
        const QVector<ModelRecord> records = scanModelRecords();
        if (records.isEmpty()) {
            QLabel *emptyLabel = new QLabel(tr("暂无已训练模型"), modelCard);
            emptyLabel->setProperty("role", QStringLiteral("itemSubtle"));
            modelListLayout->addWidget(emptyLabel);
            return;
        }

        for (int index = 0; index < records.size(); ++index) {
            const ModelRecord record = records.at(index);
            QFrame *row = modelRow(
                        modelCard,
                        record,
                        index,
                        [this](const ModelRecord &selected) {
                if (selected.inspection.runnable)
                    emit modelSelected(selected.modelDir, selected.modelName);
            },
                        [openTrainingForRecord](const ModelRecord &selected) {
                openTrainingForRecord(selected);
            });
            if (record.modelDir == selectedModelDir)
                row->setProperty("selected", true);
            modelListLayout->addWidget(row);
        }
    };

    connect(createDatasetButton, &QPushButton::clicked, this, [this, refreshModelList]() {
        CreateDatasetDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted)
            return;

        QWidget *trainingParent = parentWidget() ? parentWidget() : this;
        auto *trainingDialog = new RegisteredClassificationTrainingDialog(trainingParent);
        trainingDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(trainingDialog, &RegisteredClassificationTrainingDialog::trainingCompleted,
                this, [this, refreshModelList](const QString &modelDir, const QString &modelName) {
            if (*refreshModelList)
                (*refreshModelList)(modelDir);
            emit modelSelected(modelDir, modelName);
        });
        trainingDialog->show();
    });

    (*refreshModelList)(QString());

    setStyleSheet(QStringLiteral(
        "QDialog{background:#ffffff;color:#0f172a;font-size:18px;}"
        "QFrame[panelRole=\"managementCard\"]{background:#ffffff;border:3px solid #60a5fa;border-radius:8px;}"
        "QFrame[panelRole=\"datasetTile\"],QFrame[panelRole=\"modelRow\"]{background:#f8fbff;border:2px solid #93c5fd;border-radius:6px;}"
        "QLabel{background:transparent;font-size:18px;color:#0f172a;}"
        "QLabel[role=\"cardTitle\"]{font-size:26px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"itemTitle\"]{font-size:22px;font-weight:800;color:#0f172a;background:transparent;}"
        "QLabel[role=\"itemSubtle\"]{font-size:18px;font-weight:600;color:#334155;background:transparent;}"
        "QLabel[role=\"tipLabel\"]{font-size:18px;font-weight:700;color:#08386f;background:#e0f2fe;padding:8px;}"
        "QPushButton,QToolButton,QComboBox,QLineEdit{background:#ffffff;color:#0f172a;border:2px solid #4094ff;border-radius:6px;padding:10px;font-size:18px;font-weight:700;}"
        "QPushButton:hover,QToolButton:hover{background:#e0f2fe;border-color:#0284c7;}"
        "QPushButton[actionRole=\"primary\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;font-size:18px;font-weight:800;}"
        "QPushButton[actionRole=\"secondary\"]{background:#0ea5e9;color:#ffffff;border-color:#0ea5e9;font-size:18px;font-weight:800;}"));
}

RegisteredClassificationModelManagementDialog::~RegisteredClassificationModelManagementDialog()
{
    delete ui;
}
