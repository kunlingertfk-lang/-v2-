#include "ToolLibraryDialog.h"
#include "ui_ToolLibraryDialog.h"

#include "PlanDialogUtils.h"

#include <QButtonGroup>
#include <QPushButton>
#include <QToolButton>
#include <QWidget>


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

//返回选择工具id
ToolLibraryDialog::ToolId ToolLibraryDialog::selectedTool() const
{
    return static_cast<ToolId>(m_buttonGroup->checkedId());
}

//返回选择工具类型
ToolType ToolLibraryDialog::selectedToolType() const
{
    return m_selectedToolType;
}


//初始化各个工具组状态（隐藏显示）
void ToolLibraryDialog::setupUiState()
{
    setWindowTitle(tr("工具库"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::centerWindowOnScreen(this, parentWidget(), 40);

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

//添加以实现的工具到我的按钮组中，只有这些才可以被选择和使用
void ToolLibraryDialog::setupButtonGroup()
{
    m_buttonGroup->setExclusive(true);
    m_buttonGroup->addButton(ui->presenceToolButton, Presence);
    m_buttonGroup->addButton(ui->counterToolButton, Counter);
    m_buttonGroup->addButton(ui->judgeToolButton, Judge);
    m_buttonGroup->addButton(ui->categoryToolButton, Category);
    m_buttonGroup->addButton(ui->colorAreaToolButton, ColorArea);
    m_buttonGroup->addButton(ui->ocrToolButton, CharacterRecognition);  //字符识别
    m_buttonGroup->addButton(ui->codeToolButton, Code);

    m_buttonGroup->addButton(ui->blobPresenceButton, BlobPresence);
    m_buttonGroup->addButton(ui->circlePresenceButton, CirclePresence);
    m_buttonGroup->addButton(ui->edgePresenceButton, EdgePresence);
    m_buttonGroup->addButton(ui->linePresenceButton, LinePresence);
    m_buttonGroup->addButton(ui->contourPresenceButton, ContourPresence);

    //定位工具
    m_buttonGroup->addButton(ui->templateLocationButton, TemplateLocation); //模板定位
    m_buttonGroup->addButton(ui->edgeLocationButton, EdgeLocationButton);   //边缘定位
    m_buttonGroup->addButton(ui->circleLocationButton, CircleLocationButton);   //圆定位
    m_buttonGroup->addButton(ui->positionCorrectionToolButton, PositionCorrectionTool);

    //识别工具
    m_buttonGroup->addButton(ui->colorRecognitionToolButton, ColorRecognition); //颜色识别
    m_buttonGroup->addButton(ui->colorComparisonToolButton, ColorComparison); //颜色比较
    m_buttonGroup->addButton(ui->registrationClassToolButton, RegistrationClass);  //注册分类
    m_buttonGroup->addButton(ui->registrationClassDetectionToolButton, RegistrationClassDetection);  //注册目标检测
    // m_buttonGroup->addButton(ui->circleLocationButton, RegisteredObjectDetection); //注册目标检测


    ui->dlDetectButton->setCheckable(true);
    m_buttonGroup->addButton(ui->dlDetectButton, ObjectDetection);
    ui->ClassifyButton->setCheckable(true);
    m_buttonGroup->addButton(ui->ClassifyButton, Classification); //分类检测

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_buttonGroup, &QButtonGroup::idClicked, this, &ToolLibraryDialog::updatePreview);
#else
    connect(m_buttonGroup,
            static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
            this,
            &ToolLibraryDialog::updatePreview);
#endif
}

//确认选择的工具
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

    if (tool == EdgePresence) {
        m_selectedToolType = ToolType::EdgePresence;
        accept();
        return;
    }

    if (tool == LinePresence) {
        m_selectedToolType = ToolType::LinePresence;
        accept();
        return;
    }

    if (tool == ContourPresence) {
        m_selectedToolType = ToolType::ContourPresence;
        accept();
        return;
    }

    if (tool == ObjectDetection) {
        m_selectedToolType = ToolType::AiDetection;
        accept();
        return;
    }

    if (tool == Classification) {
        m_selectedToolType = ToolType::AiClassification;
        accept();
        return;
    }

    if (tool == PositionCorrectionTool) {
        m_selectedToolType = ToolType::PositionCorrection;
        accept();
        return;
    }

/*===========================tfk add===========================*/
    // if (tool == Classification) {
    //     m_selectedToolType = ToolType::TemplateLocation;
    //     accept();
    //     return;
    // }

    // if (tool == Classification) {
    //     m_selectedToolType = ToolType::EdgeLocation;
    //     accept();
    //     return;
    // }

    // if (tool == Classification) {//注册分类
    //     m_selectedToolType = ToolType::EdgeLocation;
    //     accept();
    //     return;
    // }

    if (tool == ColorRecognition) {//颜色识别
        m_selectedToolType = ToolType::ColorRecognition;
        accept();
        return;
    }

    if (tool == ColorComparison) {//颜色比较
        m_selectedToolType = ToolType::ColorComparison;
        accept();
        return;
    }

    if (tool == RegistrationClass) {//注册分类
        m_selectedToolType = ToolType::RegisteredClassification;
        accept();
        return;
    }

    if (tool == RegistrationClassDetection) {//注册目标检测
        m_selectedToolType = ToolType::RegisteredClassificationDetection;
        accept();
        return;
    }
/*===========================tfk end===========================*/




    ui->toolLibraryTipLabel->setText(tr("当前仅支持图案有无、斑点有无、圆有无、边缘有无、直线有无、轮廓有无、字符识别、目标检测、分类工具配置"));
}

//更新到视图上
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
    case EdgePresence:
        ui->previewTitleLabel->setText(tr("边缘有无"));
        ui->previewDescriptionLabel->setText(tr("判断检测区域内边缘是否存在"));
        break;
    case LinePresence:
        ui->previewTitleLabel->setText(tr("直线有无"));
        ui->previewDescriptionLabel->setText(tr("判断检测区域内直线是否存在"));
        break;
    case ContourPresence:
        ui->previewTitleLabel->setText(tr("轮廓有无"));
        ui->previewDescriptionLabel->setText(tr("判断检测区域内轮廓是否存在"));
        break;
    case ObjectDetection:
        ui->previewTitleLabel->setText(tr("目标检测"));
        ui->previewDescriptionLabel->setText(tr("配置深度学习目标检测参数"));
        break;
    case Classification:
        ui->previewTitleLabel->setText(tr("分类"));
        ui->previewDescriptionLabel->setText(tr("配置深度学习分类模型与结果判断参数"));
        break;
    case PositionCorrectionTool:
        ui->previewTitleLabel->setText(tr("位置修正"));
        ui->previewDescriptionLabel->setText(tr("根据基准与运行位置计算平移和旋转修正信息"));
        break;

/*===========================tfk add===========================*/       
    // case TemplateLocation:
    //     ui->previewTitleLabel->setText(tr("模板定位"));
    //     ui->previewDescriptionLabel->setText(tr("模板定位"));
    //     break;
    // case EdgeLocationButton:
    //     ui->previewTitleLabel->setText(tr("边缘定位"));
    //     ui->previewDescriptionLabel->setText(tr("边缘定位"));
    //     break;
    // case CircleLocationButton:
    //     ui->previewTitleLabel->setText(tr("圆定位"));
    //     ui->previewDescriptionLabel->setText(tr("对圆进行定位"));
    //     break;

    case ColorRecognition:
        ui->previewTitleLabel->setText(tr("颜色识别"));
        ui->previewDescriptionLabel->setText(tr("识别检测区域内目标颜色占比"));
        break;
    case ColorComparison:
        ui->previewTitleLabel->setText(tr("颜色比较"));
        ui->previewDescriptionLabel->setText(tr("比较模板区域与检测区域的颜色相似度"));
        break;
    case RegistrationClass:
        ui->previewTitleLabel->setText(tr("注册分类"));
        ui->previewDescriptionLabel->setText(tr("根据已注册类别对检测区域图像进行分类"));
        break;
    case RegistrationClassDetection:
        ui->previewTitleLabel->setText(tr("注册目标检测"));
        ui->previewDescriptionLabel->setText(tr("复刻注册分类配置界面，预留目标检测功能接入"));
        break;

/*===========================tfk end===========================*/



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
