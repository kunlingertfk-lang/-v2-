#include "PositionCorrectionDialog.h"

#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QUuid>

#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/PositionCorrection.h"
#include "ui_PositionCorrectionDialog.h"

namespace {

QJsonObject defaultBinding(const QString &outputKey, const QString &displayPath)
{
    return QJsonObject{
        {QStringLiteral("producerId"), PositionCorrection::defaultSourceId()},
        {QStringLiteral("outputKey"), outputKey},
        {QStringLiteral("displayPath"), displayPath}
    };
}

} // namespace

PositionCorrectionDialog::PositionCorrectionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::PositionCorrectionDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("方案编辑 - 位置修正"));
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    connect(ui->closeButton, &QToolButton::clicked, this, &QDialog::reject);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);

    m_config.toolId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_config.toolType = ToolType::PositionCorrection;
    m_config.category = ToolCategory::Location;
    m_config.toolName = tr("位置修正");
    m_config.displayName = tr("位置修正");
    m_config.enabled = true;

    m_correction.insert(QStringLiteral("version"), 1);
    m_correction.insert(QStringLiteral("runPointX"),
                        defaultBinding(QStringLiteral("x"), tr("1 基准图.基准点坐标X")));
    m_correction.insert(QStringLiteral("runPointY"),
                        defaultBinding(QStringLiteral("y"), tr("1 基准图.基准点坐标Y")));
    m_correction.insert(QStringLiteral("runAngle"),
                        defaultBinding(QStringLiteral("angle"), tr("1 基准图.基准角度")));
    m_correction.insert(QStringLiteral("templateRegionType"), QStringLiteral("rectangle"));
    m_correction.insert(QStringLiteral("referenceCreated"), false);

    setupBindings();
    updateTemplateButtons();

    const QImage reference = ReferenceImageProvider::instance().referenceImage();
    if (reference.isNull()) {
        ui->previewTitleLabel->setText(tr("请先设置基准图"));
    } else {
        ui->previewTitleLabel->setText(tr("基准图"));
        m_previewHelper->setImage(reference);
    }

    connect(ui->basicModeButton, &QPushButton::clicked, this, [this]() {
        ui->basicModeButton->setChecked(true);
        ui->allModeButton->setChecked(false);
    });
    connect(ui->allModeButton, &QPushButton::clicked, this, [this]() {
        ui->allModeButton->setChecked(true);
        ui->basicModeButton->setChecked(false);
        ui->statusLabel->setText(tr("全部参数将在 HALCON 算法阶段扩展"));
    });
    connect(ui->rectTemplateButton, &QPushButton::clicked, this, [this]() {
        m_correction.insert(QStringLiteral("templateRegionType"), QStringLiteral("rectangle"));
        updateTemplateButtons();
        ui->statusLabel->setText(tr("矩形模板区域绘制将在 ROI 联调阶段接入"));
    });
    connect(ui->polygonTemplateButton, &QPushButton::clicked, this, [this]() {
        m_correction.insert(QStringLiteral("templateRegionType"), QStringLiteral("polygon"));
        updateTemplateButtons();
        ui->statusLabel->setText(tr("多边形模板区域绘制将在 ROI 联调阶段接入"));
    });
    connect(ui->createReferenceButton, &QPushButton::clicked,
            this, &PositionCorrectionDialog::showNotImplemented);
    connect(ui->testRunButton, &QPushButton::clicked,
            this, &PositionCorrectionDialog::showNotImplemented);
    connect(ui->finishButton, &QPushButton::clicked, this, [this]() {
        if (validateForFinish())
            accept();
    });
}

PositionCorrectionDialog::~PositionCorrectionDialog()
{
    delete ui;
}

void PositionCorrectionDialog::setupBindings()
{
    setBinding(QStringLiteral("runPointX"), binding(QStringLiteral("runPointX")));
    setBinding(QStringLiteral("runPointY"), binding(QStringLiteral("runPointY")));
    setBinding(QStringLiteral("runAngle"), binding(QStringLiteral("runAngle")));

    rebuildBindingMenus();
}

void PositionCorrectionDialog::setAvailableProducers(const QVector<ToolConfig> &tools,
                                                      int consumerIndex)
{
    m_producers = tools;
    m_consumerIndex = qBound(0, consumerIndex, tools.size());
    rebuildBindingMenus();
}

void PositionCorrectionDialog::rebuildBindingMenus()
{
    const QList<QPair<QPushButton *, QString>> targets{
        {ui->runPointXLinkButton, QStringLiteral("runPointX")},
        {ui->runPointYLinkButton, QStringLiteral("runPointY")},
        {ui->runAngleLinkButton, QStringLiteral("runAngle")}
    };
    for (const auto &target : targets) {
        QMenu *menu = new QMenu(target.first);
        const auto addNode = [this, menu, target](const QString &producerId,
                                                  const QString &nodeText) {
            QMenu *nodeMenu = menu->addMenu(nodeText);
            const QList<QPair<QString, QString>> outputs{
                {QStringLiteral("x"), tr("匹配点X")},
                {QStringLiteral("y"), tr("匹配点Y")},
                {QStringLiteral("angle"), tr("匹配角度")}
            };
            for (const auto &output : outputs) {
                QAction *action = nodeMenu->addAction(output.second);
                connect(action, &QAction::triggered, this,
                        [this, target, producerId, nodeText, output]() {
                    setBinding(target.second,
                               QJsonObject{
                                   {QStringLiteral("producerId"), producerId},
                                   {QStringLiteral("outputKey"), output.first},
                                   {QStringLiteral("displayPath"),
                                    QStringLiteral("%1.%2").arg(nodeText, output.second)}
                               });
                });
            }
        };
        addNode(PositionCorrection::defaultSourceId(), tr("1 基准图"));
        for (int index = 0; index < m_consumerIndex; ++index) {
            const ToolConfig &tool = m_producers.at(index);
            if (!tool.enabled || tool.toolId.trimmed().isEmpty())
                continue;
            const QString name = tool.displayName.trimmed().isEmpty()
                    ? tool.toolName
                    : tool.displayName;
            addNode(tool.toolId,
                    QStringLiteral("%1 %2").arg(index + 1).arg(
                        name.trimmed().isEmpty() ? toolTypeToString(tool.toolType) : name));
        }
        target.first->setMenu(menu);
    }
}

void PositionCorrectionDialog::setBinding(const QString &key, const QJsonObject &value)
{
    m_correction.insert(key, value);
    QLineEdit *edit = nullptr;
    if (key == QStringLiteral("runPointX"))
        edit = ui->runPointXEdit;
    else if (key == QStringLiteral("runPointY"))
        edit = ui->runPointYEdit;
    else if (key == QStringLiteral("runAngle"))
        edit = ui->runAngleEdit;
    if (edit)
        edit->setText(value.value(QStringLiteral("displayPath")).toString());
}

QJsonObject PositionCorrectionDialog::binding(const QString &key) const
{
    return m_correction.value(key).toObject();
}

void PositionCorrectionDialog::loadFromConfig(const ToolConfig &config)
{
    m_config = config;
    m_config.toolType = ToolType::PositionCorrection;
    m_config.category = ToolCategory::Location;
    m_correction = config.params.value(QStringLiteral("positionCorrection")).toObject();
    if (!m_correction.contains(QStringLiteral("version")))
        m_correction.insert(QStringLiteral("version"), 1);
    setBinding(QStringLiteral("runPointX"), binding(QStringLiteral("runPointX")));
    setBinding(QStringLiteral("runPointY"), binding(QStringLiteral("runPointY")));
    setBinding(QStringLiteral("runAngle"), binding(QStringLiteral("runAngle")));
    updateTemplateButtons();
}

ToolConfig PositionCorrectionDialog::toolConfig() const
{
    ToolConfig config = m_config;
    config.toolType = ToolType::PositionCorrection;
    config.category = ToolCategory::Location;
    config.toolName = tr("位置修正");
    config.displayName = tr("位置修正");
    config.summary = tr("位置修正后端尚未实现");
    config.params.insert(QStringLiteral("positionCorrection"), m_correction);
    return config;
}

ToolPreviewSnapshot PositionCorrectionDialog::referencePreviewSnapshot() const
{
    return ToolPreviewSnapshot();
}

void PositionCorrectionDialog::updateTemplateButtons()
{
    const QString type = m_correction.value(QStringLiteral("templateRegionType"))
            .toString(QStringLiteral("rectangle"));
    ui->rectTemplateButton->setChecked(type == QStringLiteral("rectangle"));
    ui->polygonTemplateButton->setChecked(type == QStringLiteral("polygon"));
}

void PositionCorrectionDialog::showNotImplemented()
{
    if (ReferenceImageProvider::instance().referenceImage().isNull()) {
        ui->statusLabel->setText(tr("请先设置基准图"));
        return;
    }
    ui->statusLabel->setText(tr("位置修正后端尚未实现"));
}

bool PositionCorrectionDialog::validateForFinish()
{
    const QStringList keys{QStringLiteral("runPointX"),
                           QStringLiteral("runPointY"),
                           QStringLiteral("runAngle")};
    for (const QString &key : keys) {
        const QJsonObject value = binding(key);
        if (value.value(QStringLiteral("producerId")).toString().trimmed().isEmpty()
                || value.value(QStringLiteral("outputKey")).toString().trimmed().isEmpty()) {
            ui->statusLabel->setText(tr("请绑定运行点坐标X/Y/运行角度"));
            return false;
        }
    }
    return true;
}
