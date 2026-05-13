#include "ToolLibraryDialog.h"
#include "ui_ToolLibraryDialog.h"

#include <QButtonGroup>
#include <QPushButton>
#include <QToolButton>
#include <QWidget>

#include "WindowUtils.h"

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

ToolType ToolLibraryDialog::selectedToolType() const
{
    return m_selectedToolType;
}

void ToolLibraryDialog::setupUiState()
{
    setWindowTitle(tr("工具库"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    WindowUtils::centerWindowOnScreen(this, parentWidget(), 40);

    connect(ui->closeButton, &QToolButton::clicked, this, &ToolLibraryDialog::reject);
    connect(ui->cancelButton, &QPushButton::clicked, this, &ToolLibraryDialog::reject);
    connect(ui->confirmButton, &QPushButton::clicked, this, &ToolLibraryDialog::confirmSelection);

    const auto hideAllCategoryFrames = [this]() {
        ui->measurementCategoryFrame->hide();
        ui->countCategoryFrame->hide();
        ui->recognitionCategoryFrame->hide();
        ui->presenceCategoryFrame->hide();
        ui->logicCategoryFrame->hide();
        ui->locationCategoryFrame->hide();
        ui->deepLearningCategoryFrame->hide();
        ui->defectCategoryFrame->hide();
    };

    const auto clearNavigation = [this]() {
        ui->allToolsButton->setChecked(false);
        ui->measureToolsButton->setChecked(false);
        ui->countToolsButton->setChecked(false);
        ui->recognitionToolsButton->setChecked(false);
        ui->presenceToolsButton->setChecked(false);
        ui->logicToolsButton->setChecked(false);
        ui->locationToolsButton->setChecked(false);
        ui->deepLearningToolsButton->setChecked(false);
        ui->defectToolsButton->setChecked(false);
    };

    const auto showAllCategories = [this, hideAllCategoryFrames, clearNavigation]() {
        hideAllCategoryFrames();
        ui->measurementCategoryFrame->show();
        ui->countCategoryFrame->show();
        ui->recognitionCategoryFrame->show();
        ui->presenceCategoryFrame->show();
        ui->logicCategoryFrame->show();
        ui->locationCategoryFrame->show();
        ui->deepLearningCategoryFrame->show();
        ui->defectCategoryFrame->show();
        clearNavigation();
        ui->allToolsButton->setChecked(true);
        ui->toolLibraryTipLabel->clear();
    };

    const auto showSingleCategory = [this, hideAllCategoryFrames, clearNavigation](QWidget *categoryFrame,QPushButton *navigationButton) {
        hideAllCategoryFrames();
        if (categoryFrame) {
            categoryFrame->show();
        }
        clearNavigation();
        if (navigationButton) {
            navigationButton->setChecked(true);
        }
        ui->toolLibraryTipLabel->clear();
    };

    hideAllCategoryFrames();
    clearNavigation();

    connect(ui->allToolsButton, &QPushButton::clicked, this, showAllCategories);
    connect(ui->measureToolsButton, &QPushButton::clicked, this, [showSingleCategory, this]() {
        showSingleCategory(ui->measurementCategoryFrame, ui->measureToolsButton);
    });
    connect(ui->countToolsButton, &QPushButton::clicked, this, [showSingleCategory, this]() {
        showSingleCategory(ui->countCategoryFrame, ui->countToolsButton);
    });
    connect(ui->recognitionToolsButton, &QPushButton::clicked, this, [showSingleCategory, this]() {
        showSingleCategory(ui->recognitionCategoryFrame, ui->recognitionToolsButton);
    });
    connect(ui->presenceToolsButton, &QPushButton::clicked, this, [showSingleCategory, this]() {
        showSingleCategory(ui->presenceCategoryFrame, ui->presenceToolsButton);
    });
    connect(ui->logicToolsButton, &QPushButton::clicked, this, [showSingleCategory, this]() {
        showSingleCategory(ui->logicCategoryFrame, ui->logicToolsButton);
    });
    connect(ui->locationToolsButton, &QPushButton::clicked, this, [showSingleCategory, this]() {
        showSingleCategory(ui->locationCategoryFrame, ui->locationToolsButton);
    });
    connect(ui->deepLearningToolsButton, &QPushButton::clicked, this, [showSingleCategory, this]() {
        showSingleCategory(ui->deepLearningCategoryFrame, ui->deepLearningToolsButton);
    });
    connect(ui->defectToolsButton, &QPushButton::clicked, this, [showSingleCategory, this]() {
        showSingleCategory(ui->defectCategoryFrame, ui->defectToolsButton);
    });

    ui->toolLibraryTipLabel->clear();
    updatePreview(NoTool);
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
    m_buttonGroup->addButton(ui->blobPresenceButton, BlobPresence);
    m_buttonGroup->addButton(ui->circlePresenceButton, CirclePresence);

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

    if (tool == Presence) {
        m_selectedToolType = ToolType::PatternPresence;
        accept();
        return;
    }

    if (tool == CharacterRecognition) {
        m_selectedToolType = ToolType::Ocr;
        accept();
        return;
    }

    if (tool == BlobPresence) {
        m_selectedToolType = ToolType::BlobPresence;
        accept();
        return;
    }

    if (tool == CirclePresence) {
        m_selectedToolType = ToolType::CirclePresence;
        accept();
        return;
    }

    ui->toolLibraryTipLabel->setText(tr("当前仅支持图案有无、斑点有无、圆有无、字符识别工具配置"));
}

void ToolLibraryDialog::updatePreview(int id)
{
    ui->toolLibraryTipLabel->clear();

    switch (static_cast<ToolId>(id)) {
    case Presence:
        ui->previewTitleLabel->setText(tr("图案有无"));
        ui->previewDescriptionLabel->setText(tr("判断检测区域内图案是否存在"));
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
    case BlobPresence:
        ui->previewTitleLabel->setText(tr("斑点有无"));
        ui->previewDescriptionLabel->setText(tr("判断检测区域内斑点是否存在"));
        break;
    case CirclePresence:
        ui->previewTitleLabel->setText(tr("圆有无"));
        ui->previewDescriptionLabel->setText(tr("判断检测区域内圆形目标是否存在"));
        break;
    case NoTool:
        ui->previewTitleLabel->setText(tr("请选择工具"));
        ui->previewDescriptionLabel->setText(tr("请点击左侧工具分类查看工具列表"));
        break;
    case CharacterRecognition:
    default:
        ui->previewTitleLabel->setText(tr("字符识别"));
        ui->previewDescriptionLabel->setText(tr("在检测ROI范围内，识别字符串"));
        break;
    }
}
