#include "ToolLibraryDialog.h"
#include "ui_ToolLibraryDialog.h"
#include "CharacterRecognitionDialog.h"

#include <QButtonGroup>
#include <QPushButton>
#include <QToolButton>

ToolLibraryDialog::ToolLibraryDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ToolLibraryDialog)
    , m_buttonGroup(new QButtonGroup(this))
{
    ui->setupUi(this);
    setupUiState();
    setupButtonGroup();
}

ToolLibraryDialog::~ToolLibraryDialog()
{
    delete ui;
}

ToolLibraryDialog::ToolId ToolLibraryDialog::selectedTool() const
{
    return static_cast<ToolId>(m_buttonGroup->checkedId());
}

void ToolLibraryDialog::setupUiState()
{
    setWindowTitle(tr("工具库"));
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    resize(1180, 790);

    connect(ui->closeButton, &QToolButton::clicked, this, &ToolLibraryDialog::reject);
    connect(ui->cancelButton, &QPushButton::clicked, this, &ToolLibraryDialog::reject);
    connect(ui->confirmButton, &QPushButton::clicked, this, &ToolLibraryDialog::confirmSelection);

    ui->toolLibraryTipLabel->clear();
    updatePreview(CharacterRecognition);
}

void ToolLibraryDialog::setupButtonGroup()
{
    m_buttonGroup->setExclusive(true);
    m_buttonGroup->addButton(ui->presenceToolButton, Presence);
    m_buttonGroup->addButton(ui->counterToolButton, Counter);
    m_buttonGroup->addButton(ui->judgeToolButton, Judge);
    m_buttonGroup->addButton(ui->categoryToolButton, Category);
    m_buttonGroup->addButton(ui->colorAreaToolButton, ColorArea);
    m_buttonGroup->addButton(ui->ocrToolButton, CharacterRecognition);
    m_buttonGroup->addButton(ui->codeToolButton, Code);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_buttonGroup, &QButtonGroup::idClicked, this, &ToolLibraryDialog::updatePreview);
#else
    connect(m_buttonGroup,
            static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
            this,
            &ToolLibraryDialog::updatePreview);
#endif
}

void ToolLibraryDialog::confirmSelection()
{
    const ToolId tool = selectedTool();
    if (tool == NoTool) {
        ui->toolLibraryTipLabel->setText(tr("请选择工具"));
        return;
    }

    if (tool != CharacterRecognition) {
        ui->toolLibraryTipLabel->setText(tr("当前仅支持字符识别工具配置"));
        return;
    }
    // 1. 先隐藏自己（ToolLibraryDialog），不立即关闭
    this->hide();
    // 2. 隐藏父窗口 ToolsDialog
    QWidget *toolsDialog = parentWidget();
    if (toolsDialog) {
        toolsDialog->hide();
    }
    // 3. 打开字符识别窗口
    CharacterRecognitionDialog dlg;
    dlg.exec();
    // 4. 字符识别关闭后 → 重新显示 ToolsDialog
    if (toolsDialog) {
        toolsDialog->show();
    }
    this->close();
}

void ToolLibraryDialog::updatePreview(int id)
{
    ui->toolLibraryTipLabel->clear();

    switch (static_cast<ToolId>(id)) {
    case Presence:
        ui->previewTitleLabel->setText(tr("有无检测"));
        ui->previewDescriptionLabel->setText(tr("判断检测区域内目标是否存在"));
        break;
    case Counter:
        ui->previewTitleLabel->setText(tr("学习计数"));
        ui->previewDescriptionLabel->setText(tr("学习目标特征并统计数量"));
        break;
    case Judge:
        ui->previewTitleLabel->setText(tr("异常判断"));
        ui->previewDescriptionLabel->setText(tr("输出OK/NG异常判断结果"));
        break;
    case Category:
        ui->previewTitleLabel->setText(tr("种类识别"));
        ui->previewDescriptionLabel->setText(tr("识别并区分目标种类"));
        break;
    case ColorArea:
        ui->previewTitleLabel->setText(tr("颜色面积"));
        ui->previewDescriptionLabel->setText(tr("统计指定颜色区域面积"));
        break;
    case Code:
        ui->previewTitleLabel->setText(tr("码识别"));
        ui->previewDescriptionLabel->setText(tr("识别条码和二维码内容"));
        break;
    case CharacterRecognition:
    case NoTool:
    default:
        ui->previewTitleLabel->setText(tr("字符识别"));
        ui->previewDescriptionLabel->setText(tr("在检测ROI范围内，识别字符串"));
        break;
    }
}
