#include "TemplateLocationDialog.h"

#include "PlanDialogUtils.h"
#include "UiStyleRoles.h"
#include "algorithms/location/TemplateLocationConfig.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceBaseSelector.h"
#include "frame/ReferenceImageProvider.h"
#include "ui_TemplateLocationDialog.h"

#include <QButtonGroup>
#include <QBrush>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QListView>
#include <QMap>
#include <QSet>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kTemplateIdRole = Qt::UserRole + 1;
constexpr int kMatchIdRole = Qt::UserRole + 2;
constexpr int kMaximumTemplateCount = 8;

class ItemWidgetOnlyDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *, const QStyleOptionViewItem &,
               const QModelIndex &) const override
    {
        // The compact template tab widget owns painting.  The QListWidgetItem
        // still carries text/check-state roles for persistence and tests.
    }
};

QRectF boundingRect(const QVector<QPointF> &points)
{
    if (points.isEmpty())
        return QRectF();
    double left = points.first().x();
    double right = left;
    double top = points.first().y();
    double bottom = top;
    for (const QPointF &point : points) {
        left = qMin(left, point.x());
        right = qMax(right, point.x());
        top = qMin(top, point.y());
        bottom = qMax(bottom, point.y());
    }
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

QPointF polygonCentroid(const QVector<QPointF> &points)
{
    if (points.size() < 3)
        return boundingRect(points).center();
    double twiceArea = 0.0;
    double weightedX = 0.0;
    double weightedY = 0.0;
    for (int index = 0; index < points.size(); ++index) {
        const QPointF &current = points.at(index);
        const QPointF &next = points.at((index + 1) % points.size());
        const double cross = current.x() * next.y() - next.x() * current.y();
        twiceArea += cross;
        weightedX += (current.x() + next.x()) * cross;
        weightedY += (current.y() + next.y()) * cross;
    }
    if (std::abs(twiceArea) < 1e-12)
        return boundingRect(points).center();
    return QPointF(weightedX / (3.0 * twiceArea),
                   weightedY / (3.0 * twiceArea));
}

bool validRect(const QRectF &rect)
{
    return std::isfinite(rect.x()) && std::isfinite(rect.y()) &&
           std::isfinite(rect.width()) && std::isfinite(rect.height()) &&
           rect.width() > 0.0 && rect.height() > 0.0 &&
           rect.left() >= 0.0 && rect.top() >= 0.0 &&
           rect.right() <= 1.000001 && rect.bottom() <= 1.000001;
}

bool validNormalizedPoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y()) &&
            point.x() >= 0.0 && point.x() <= 1.0 &&
            point.y() >= 0.0 && point.y() <= 1.0;
}

int nextBaseBindingOrder(
        const QVector<TemplateLocationBaseBindingConfig> &bindings)
{
    int maximumOrder = -1;
    for (const TemplateLocationBaseBindingConfig &binding : bindings)
        maximumOrder = qMax(maximumOrder, binding.order);
    return maximumOrder + 1;
}

void clearButtonChecks(QButtonGroup *group)
{
    if (!group)
        return;
    group->setExclusive(false);
    for (QAbstractButton *button : group->buttons())
        button->setChecked(false);
    group->setExclusive(true);
}

void appendTemplateMaskOverlay(QVector<ToolOverlay> *overlays,
                               const QString &type,
                               const QRectF &rect,
                               const QVector<QPointF> &points,
                               const CircleRoi &circle,
                               int imageWidth,
                               int imageHeight)
{
    if (!overlays || type == QStringLiteral("none") ||
            imageWidth <= 0 || imageHeight <= 0)
        return;
    ToolOverlay overlay;
    if (type == QStringLiteral("circle") && circle.valid) {
        overlay.type = ToolOverlayType::Circle;
        overlay.center = QPointF(circle.centerNormalized.x() * imageWidth,
                                 circle.centerNormalized.y() * imageHeight);
        overlay.radius = circle.radiusNormalized *
                qMax(imageWidth, imageHeight);
    } else if (type == QStringLiteral("polygon") && points.size() >= 3) {
        overlay.type = ToolOverlayType::Polygon;
        for (const QPointF &point : points) {
            overlay.points.append(QPointF(point.x() * imageWidth,
                                          point.y() * imageHeight));
        }
    } else if (type == QStringLiteral("rectangle") && validRect(rect)) {
        overlay.type = ToolOverlayType::Rect;
        overlay.rect = QRectF(rect.x() * imageWidth,
                              rect.y() * imageHeight,
                              rect.width() * imageWidth,
                              rect.height() * imageHeight);
    } else {
        return;
    }
    overlay.label = QStringLiteral("template_mask");
    overlay.extra.insert(QStringLiteral("displayRole"),
                         QStringLiteral("color_template_mask"));
    overlays->append(overlay);
}

} // namespace

TemplateLocationDialog::TemplateLocationDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::TemplateLocationDialog)
    , m_modeGroup(new QButtonGroup(this))
    , m_templateGroup(new QButtonGroup(this))
    , m_searchGroup(new QButtonGroup(this))
    , m_modelCacheKey(QStringLiteral("template_location_%1")
                      .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    ui->setupUi(this);
    m_config.toolId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_config.toolType = ToolType::TemplateLocation;
    m_config.category = ToolCategory::Location;
    m_config.toolName = tr("模板定位");
    m_config.displayName = tr("模板定位");
    m_config.enabled = true;

    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);
    m_previewHelper->setNavigationEnabled(true);
    m_testEngine.registerAdapter(&m_testAdapter);
    setupUiState();
    connectControls();
    showReferenceImage();
}

TemplateLocationDialog::~TemplateLocationDialog()
{
    if (m_frameConnection)
        disconnect(m_frameConnection);
    delete ui;
}

void TemplateLocationDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 模板定位"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(this);

    m_modeGroup->setExclusive(true);
    m_modeGroup->addButton(ui->basicModeButton, 0);
    m_modeGroup->addButton(ui->allModeButton, 1);
    m_templateGroup->setExclusive(true);
    m_templateGroup->addButton(ui->templateRectButton, 0);
    m_templateGroup->addButton(ui->templatePolygonButton, 1);
    m_searchGroup->setExclusive(true);
    m_searchGroup->addButton(ui->searchRectButton, 0);
    m_searchGroup->addButton(ui->searchCircleButton, 1);
    m_searchGroup->addButton(ui->searchPolygonButton, 2);
    m_searchGroup->addButton(ui->searchGlobalButton, 3);
    ui->searchGlobalButton->setChecked(true);

    setupTemplateBankUi();

    ui->templateLayout->removeWidget(ui->createTemplateButton);
    ui->templateLayout->removeWidget(ui->deleteTemplateButton);
    auto *templateCommandLayout = new QHBoxLayout;
    templateCommandLayout->setContentsMargins(0, 0, 0, 0);
    templateCommandLayout->setSpacing(4);
    templateCommandLayout->addWidget(ui->createTemplateButton, 1);
    templateCommandLayout->addWidget(ui->deleteTemplateButton);
    ui->templateLayout->addLayout(templateCommandLayout, 2, 0, 1, 3);

    m_templateMaskRow = new QWidget(ui->templateCard);
    m_templateMaskRow->setObjectName(QStringLiteral("templateMaskRow"));
    auto *templateMaskLayout = new QHBoxLayout(m_templateMaskRow);
    templateMaskLayout->setContentsMargins(0, 0, 0, 0);
    templateMaskLayout->setSpacing(4);
    auto *templateMaskLabel = new QLabel(tr("屏蔽区域"), m_templateMaskRow);
    templateMaskLabel->setProperty("role", QStringLiteral("rowField"));
    templateMaskLayout->addWidget(templateMaskLabel);
    templateMaskLayout->addStretch(1);
    m_templateMaskRectButton = new QToolButton(m_templateMaskRow);
    m_templateMaskRectButton->setObjectName(
                QStringLiteral("templateMaskRectButton"));
    m_templateMaskRectButton->setToolTip(tr("绘制矩形模板屏蔽区域"));
    m_templateMaskRectButton->setIcon(
                QIcon(QStringLiteral(":/icons/roi-rectangle.svg")));
    m_templateMaskCircleButton = new QToolButton(m_templateMaskRow);
    m_templateMaskCircleButton->setObjectName(
                QStringLiteral("templateMaskCircleButton"));
    m_templateMaskCircleButton->setToolTip(tr("绘制圆形模板屏蔽区域"));
    m_templateMaskCircleButton->setIcon(
                QIcon(QStringLiteral(":/icons/roi-circle.svg")));
    m_templateMaskPolygonButton = new QToolButton(m_templateMaskRow);
    m_templateMaskPolygonButton->setObjectName(
                QStringLiteral("templateMaskPolygonButton"));
    m_templateMaskPolygonButton->setToolTip(tr("绘制模板屏蔽区域"));
    m_templateMaskPolygonButton->setIcon(
                QIcon(QStringLiteral(":/icons/roi-polygon.svg")));
    const QList<QToolButton *> templateMaskButtons{
        m_templateMaskRectButton,
        m_templateMaskCircleButton,
        m_templateMaskPolygonButton
    };
    for (QToolButton *button : templateMaskButtons) {
        button->setIconSize(QSize(24, 24));
        button->setCheckable(true);
        button->setFixedSize(42, 38);
        button->setProperty("actionRole", QStringLiteral("toolbarIcon"));
    }
    m_templateMaskClearButton = new QPushButton(tr("清除"), m_templateMaskRow);
    m_templateMaskClearButton->setObjectName(
                QStringLiteral("templateMaskClearButton"));
    m_templateMaskClearButton->setProperty(
                "actionRole", QStringLiteral("secondary"));
    m_templateMaskClearButton->setEnabled(false);
    templateMaskLayout->addWidget(m_templateMaskRectButton);
    templateMaskLayout->addWidget(m_templateMaskCircleButton);
    templateMaskLayout->addWidget(m_templateMaskPolygonButton);
    templateMaskLayout->addWidget(m_templateMaskClearButton);
    ui->templateLayout->addWidget(m_templateMaskRow, 6, 0, 1, 3);
    m_templateGroup->addButton(m_templateMaskRectButton, 2);
    m_templateGroup->addButton(m_templateMaskCircleButton, 3);
    m_templateGroup->addButton(m_templateMaskPolygonButton, 4);

    ui->searchLayout->removeWidget(ui->searchGlobalButton);
    ui->searchLayout->removeWidget(ui->searchRectButton);
    ui->searchLayout->removeWidget(ui->searchCircleButton);
    ui->searchLayout->removeWidget(ui->searchPolygonButton);
    auto *searchRoiLayout = new QHBoxLayout;
    searchRoiLayout->setContentsMargins(0, 0, 0, 0);
    searchRoiLayout->setSpacing(4);
    searchRoiLayout->addStretch();
    searchRoiLayout->addWidget(ui->searchGlobalButton);
    searchRoiLayout->addWidget(ui->searchRectButton);
    searchRoiLayout->addWidget(ui->searchCircleButton);
    searchRoiLayout->addWidget(ui->searchPolygonButton);
    ui->searchLayout->addLayout(searchRoiLayout, 1, 1, 1, 2);

    ui->templateLayout->setColumnStretch(0, 1);
    ui->templateLayout->setColumnStretch(1, 0);
    ui->templateLayout->setColumnStretch(2, 0);
    ui->searchLayout->setColumnStretch(0, 1);
    ui->searchLayout->setColumnStretch(1, 0);
    ui->searchLayout->setColumnStretch(2, 0);
    ui->searchLayout->setColumnStretch(3, 0);
    ui->searchLayout->setColumnStretch(4, 0);

    const QList<QToolButton *> roiButtons{ui->templateRectButton,
                                         ui->templatePolygonButton,
                                         ui->searchRectButton,
                                         ui->searchCircleButton,
                                         ui->searchPolygonButton,
                                         ui->searchGlobalButton};
    for (QToolButton *button : roiButtons) {
        button->setFixedSize(42, 38);
        button->setProperty("actionRole", QStringLiteral("toolbarIcon"));
    }
    ui->deleteTemplateButton->setFixedWidth(88);
    ui->resultBar->setFixedHeight(48);
    // Keep the compact two-line result readable: Qt's default 9px vertical
    // margins leave only 30px of content inside a 48px status bar.
    ui->resultLayout->setContentsMargins(8, 2, 8, 2);
    ui->resultLayout->setSpacing(8);
    ui->resultLayout->setStretch(0, 1);
    ui->resultLayout->setStretch(1, 0);
    ui->resultLayout->setStretch(2, 0);
    ui->resultLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->resultLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->elapsedLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ui->selectOriginButton->setCheckable(true);
    ui->selectOriginButton->setFixedWidth(88);

    m_matchResultDrawer = new QFrame(ui->previewPanel);
    m_matchResultDrawer->setObjectName(QStringLiteral("matchResultDrawer"));
    m_matchResultDrawer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *drawerLayout = new QVBoxLayout(m_matchResultDrawer);
    drawerLayout->setContentsMargins(0, 0, 0, 0);
    drawerLayout->setSpacing(0);

    m_matchResultToggle = new QPushButton(m_matchResultDrawer);
    m_matchResultToggle->setObjectName(QStringLiteral("matchResultToggle"));
    m_matchResultToggle->setCheckable(true);
    m_matchResultToggle->setFixedHeight(36);
    m_matchResultToggle->setText(tr("▶ 匹配结果（0）"));
    drawerLayout->addWidget(m_matchResultToggle);

    m_matchResultTable = new QTableWidget(m_matchResultDrawer);
    m_matchResultTable->setObjectName(QStringLiteral("matchResultTable"));
    m_matchResultTable->setColumnCount(7);
    m_matchResultTable->setHorizontalHeaderLabels(
                QStringList{tr("序号"), tr("模板"), tr("X"), tr("Y"),
                            tr("角度"), tr("缩放"), tr("得分")});
    m_matchResultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_matchResultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_matchResultTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_matchResultTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_matchResultTable->verticalHeader()->setVisible(false);
    m_matchResultTable->verticalHeader()->setDefaultSectionSize(28);
    m_matchResultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_matchResultTable->setFixedHeight(116);
    m_matchResultTable->hide();
    drawerLayout->addWidget(m_matchResultTable);
    m_matchResultDrawer->setFixedHeight(36);
    m_matchResultDrawer->hide();
    ui->previewLayout->insertWidget(2, m_matchResultDrawer);
    ui->previewLayout->setStretch(0, 0);
    ui->previewLayout->setStretch(1, 1);
    ui->previewLayout->setStretch(2, 0);
    ui->previewLayout->setStretch(3, 0);
    ui->previewGraphicsView->setMinimumHeight(0);
    ui->previewGraphicsView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_matchResultToggle, &QPushButton::clicked,
            this, &TemplateLocationDialog::setMatchResultsExpanded);
    ui->contrastLabel->setProperty("autoParameter", true);
    ui->minContrastLabel->setProperty("autoParameter", true);
    ui->contrastSpinBox->setProperty("autoParameter", true);
    ui->minContrastSpinBox->setProperty("autoParameter", true);
    UiStyleRoles::applyLightComboBox(ui->polarityComboBox);
    UiStyleRoles::applyLightComboBox(ui->contrastModeComboBox);
    UiStyleRoles::applyLightComboBox(ui->subPixelComboBox);
    UiStyleRoles::applyLightComboBox(ui->originModeComboBox);
    setAdvancedVisible(false);
    updateContrastControls();
    updateOriginControls();
}

void TemplateLocationDialog::setupTemplateBankUi()
{
    m_templateBankCard = new QFrame(ui->parameterScrollContents);
    m_templateBankCard->setObjectName(QStringLiteral("templateBankCard"));
    m_templateBankCard->setProperty("panelRole", QStringLiteral("configCard"));

    auto *cardLayout = new QVBoxLayout(m_templateBankCard);
    cardLayout->setContentsMargins(12, 12, 12, 12);
    cardLayout->setSpacing(8);

    auto *title = new QLabel(tr("模板库"), m_templateBankCard);
    title->setObjectName(QStringLiteral("templateBankTitle"));
    title->setProperty("role", QStringLiteral("cardTitle"));
    cardLayout->addWidget(title);

    m_baseSelector = new ReferenceBaseSelector(m_templateBankCard);
    m_baseSelector->setObjectName(QStringLiteral("templateReferenceBaseSelector"));
    m_baseSelector->setValidateAllVisible(true);
    cardLayout->addWidget(m_baseSelector);
    m_selectedBaseId = m_baseSelector->selectedBaseId();
    if (m_selectedBaseId.isEmpty())
        m_selectedBaseId = ReferenceImageProvider::instance().primaryBaseId();
    seedBaseBindingsFromProvider();

    m_templateBankList = new QListWidget(m_templateBankCard);
    m_templateBankList->setObjectName(QStringLiteral("templateBankListWidget"));
    m_templateBankList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_templateBankList->setFlow(QListView::LeftToRight);
    m_templateBankList->setWrapping(false);
    m_templateBankList->setMovement(QListView::Static);
    m_templateBankList->setResizeMode(QListView::Adjust);
    m_templateBankList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_templateBankList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_templateBankList->setMinimumHeight(46);
    m_templateBankList->setMaximumHeight(46);
    m_templateBankList->setUniformItemSizes(true);
    m_templateBankList->setItemDelegate(
                new ItemWidgetOnlyDelegate(m_templateBankList));

    m_addTemplateItemButton = new QPushButton(tr("+"), m_templateBankCard);
    m_addTemplateItemButton->setObjectName(
                QStringLiteral("addTemplateItemButton"));
    m_addTemplateItemButton->setToolTip(tr("添加模板（最多 8 个）"));
    m_addTemplateItemButton->setFixedSize(46, 46);
    auto *tabRow = new QHBoxLayout;
    tabRow->setContentsMargins(0, 0, 0, 0);
    tabRow->setSpacing(0);
    tabRow->addWidget(m_templateBankList, 1);
    tabRow->addWidget(m_addTemplateItemButton);
    cardLayout->addLayout(tabRow);

    m_templateBankSummaryLabel = new QLabel(m_templateBankCard);
    m_templateBankSummaryLabel->setObjectName(
                QStringLiteral("templateBankSummaryLabel"));
    m_templateBankSummaryLabel->setProperty("hint", true);
    cardLayout->addWidget(m_templateBankSummaryLabel);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6);
    m_renameTemplateItemButton = new QPushButton(tr("重命名"), m_templateBankCard);
    m_renameTemplateItemButton->setObjectName(
                QStringLiteral("renameTemplateItemButton"));
    m_renameTemplateItemButton->setProperty("actionRole", QStringLiteral("plain"));
    m_deleteTemplateItemButton = new QPushButton(tr("删除模板项"), m_templateBankCard);
    m_deleteTemplateItemButton->setObjectName(
                QStringLiteral("deleteTemplateItemButton"));
    m_deleteTemplateItemButton->setProperty("actionRole", QStringLiteral("plain"));
    buttonLayout->addWidget(m_renameTemplateItemButton);
    buttonLayout->addWidget(m_deleteTemplateItemButton);
    m_renameTemplateItemButton->hide();
    m_deleteTemplateItemButton->hide();
    cardLayout->addLayout(buttonLayout);

    auto *parameterScopeLayout = new QHBoxLayout;
    parameterScopeLayout->setContentsMargins(0, 0, 0, 0);
    parameterScopeLayout->setSpacing(8);
    m_independentParametersCheckBox = new QCheckBox(
                tr("当前模板使用独立参数"), m_templateBankCard);
    m_independentParametersCheckBox->setObjectName(
                QStringLiteral("independentTemplateParametersCheckBox"));
    m_parameterScopeLabel = new QLabel(tr("当前：共享参数"), m_templateBankCard);
    m_parameterScopeLabel->setObjectName(
                QStringLiteral("templateParameterScopeLabel"));
    m_parameterScopeLabel->setProperty("hint", true);
    parameterScopeLayout->addWidget(m_independentParametersCheckBox);
    parameterScopeLayout->addStretch(1);
    parameterScopeLayout->addWidget(m_parameterScopeLabel);
    cardLayout->addLayout(parameterScopeLayout);

    ui->parameterLayout->insertWidget(0, m_templateBankCard);
    ui->templateTitle->setText(tr("当前模板设置"));
    ui->createTemplateButton->setText(tr("创建当前模型"));
    ui->deleteTemplateButton->setText(tr("清除当前模型"));

    TemplateItemState initial;
    initial.templateId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    initial.name = tr("模板1");
    initial.sourceBaseId = m_selectedBaseId.trimmed().isEmpty()
            ? QStringLiteral("base-0") : m_selectedBaseId;
    initial.order = 0;
    initial.priority = 0;
    initial.independentParameters = m_sharedParameters;
    initial.modelCacheKey = m_modelCacheKey;
    m_templates.append(initial);
    m_activeTemplateId = initial.templateId;
    refreshTemplateList();
    refreshActiveTemplateUi();
}

void TemplateLocationDialog::connectControls()
{
    connect(ui->closeButton, &QToolButton::clicked, this, &QDialog::reject);
    connect(ui->basicModeButton, &QPushButton::clicked, this, [this]() { setAdvancedVisible(false); });
    connect(ui->allModeButton, &QPushButton::clicked, this, [this]() { setAdvancedVisible(true); });
    connect(ui->fitViewButton, &QPushButton::clicked, m_previewHelper, &FrameViewHelper::fitToView);
    connect(m_addTemplateItemButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::addTemplateItem);
    connect(m_renameTemplateItemButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::renameActiveTemplateItem);
    connect(m_deleteTemplateItemButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::deleteActiveTemplateItem);
    connect(m_templateBankList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current, QListWidgetItem *) {
        if (m_updatingTemplateList || !current)
            return;
        switchActiveTemplate(current->data(kTemplateIdRole).toString());
    });
    connect(m_templateBankList, &QListWidget::itemChanged,
            this, &TemplateLocationDialog::handleTemplateItemChanged);
    connect(m_baseSelector, &ReferenceBaseSelector::selectedBaseChanged,
            this, [this](const QString &baseId) {
        if (baseId.trimmed().isEmpty() || baseId == m_selectedBaseId)
            return;
        syncCurrentBaseBinding();
        m_selectedBaseId = baseId;
        loadSelectedBaseBinding();
        showReferenceImage();
        ui->statusLabel->setText(
                    tr("验证图/搜索域已切换到 %1（不会改变模板来源 Base）")
                    .arg(baseDisplayName(baseId)));
    });
    connect(m_baseSelector, &ReferenceBaseSelector::validateAllRequested,
            this, &TemplateLocationDialog::validateAllReferenceBases);
    connect(m_independentParametersCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
        if (m_updatingParameterEditor || m_loadingConfig)
            return;
        const int index = activeTemplateIndex();
        if (index < 0)
            return;
        flushParameterEditor();
        ensureV6Editing();
        TemplateItemState &item = m_templates[index];
        if (checked && !item.useIndependentParameters) {
            const QJsonObject preservedExtra =
                    item.independentParameters.extra;
            item.independentParameters = m_sharedParameters;
            item.independentParameters.extra = preservedExtra;
        }
        item.useIndependentParameters = checked;
        item.independentParametersComplete = true;
        loadParameterEditor();
        markModelDirty();
        refreshTemplateList();
    });

    connect(ui->originModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (!m_loadingConfig)
            ensureV6Editing();
        m_originMode = index == 1 ? QStringLiteral("custom") : QStringLiteral("centroid");
        if (m_originMode == QStringLiteral("centroid")) {
            m_previewHelper->setPointSelectionEnabled(false);
            ui->selectOriginButton->setChecked(false);
        }
        updateOriginControls();
        syncCurrentBaseBinding();
        showReferenceImage();
    });
    connect(ui->selectOriginButton, &QPushButton::clicked, this, [this](bool checked) {
        stopEditing();
        showReferenceImage();
        ui->selectOriginButton->setChecked(checked);
        m_previewHelper->setPointSelectionEnabled(checked);
        ui->viewerTitleLabel->setText(checked ? tr("点击图像选择自定义定位点") : tr("基准图"));
    });
    connect(m_previewHelper, &FrameViewHelper::pointSelected, this,
            [this](const QPointF &point) {
        ensureV6Editing();
        m_originMode = QStringLiteral("custom");
        m_customOriginNormalized = point;
        ui->originModeComboBox->setCurrentIndex(1);
        updateOriginControls();
        syncCurrentBaseBinding();
        showReferenceImage();
        ui->statusLabel->setText(tr("自定义定位点已更新，可继续点击调整"));
    });

    connect(ui->templateRectButton, &QToolButton::clicked, this,
            [this]() { startEditing(EditTarget::TemplateRect); });
    connect(ui->templatePolygonButton, &QToolButton::clicked, this,
            [this]() { startEditing(EditTarget::TemplatePolygon); });
    connect(m_templateMaskRectButton, &QToolButton::clicked, this,
            [this]() { startEditing(EditTarget::TemplateMaskRect); });
    connect(m_templateMaskCircleButton, &QToolButton::clicked, this,
            [this]() { startEditing(EditTarget::TemplateMaskCircle); });
    connect(m_templateMaskPolygonButton, &QToolButton::clicked, this,
            [this]() { startEditing(EditTarget::TemplateMaskPolygon); });
    connect(m_templateMaskClearButton, &QPushButton::clicked, this, [this]() {
        stopEditing();
        if (m_templateMaskRegionType == QStringLiteral("none"))
            return;
        ensureV6Editing();
        m_templateExcludeEdited = true;
        m_templateMaskRegionType = QStringLiteral("none");
        m_templateMaskRoi = QRectF();
        m_templateMaskPolygon.clear();
        m_templateMaskCircle = CircleRoi();
        m_templateMaskClearButton->setEnabled(false);
        m_previewHelper->clearPolygonRoi();
        markModelDirty();
        flushActiveTemplateEditor();
        refreshActiveTemplateUi();
        showReferenceImage();
        ui->statusLabel->setText(tr("模板屏蔽区域已清除，请重新创建模板"));
    });
    connect(ui->searchRectButton, &QToolButton::clicked, this,
            [this]() { startEditing(EditTarget::SearchRect); });
    connect(ui->searchCircleButton, &QToolButton::clicked, this,
            [this]() { startEditing(EditTarget::SearchCircle); });
    connect(ui->searchPolygonButton, &QToolButton::clicked, this,
            [this]() { startEditing(EditTarget::SearchPolygon); });
    connect(ui->searchGlobalButton, &QToolButton::clicked, this,
            [this]() { startEditing(EditTarget::SearchGlobal); });

    connect(m_previewHelper, &FrameViewHelper::roiChanged, this, [this](const QRectF &roi) {
        ensureV6Editing();
        if (m_editTarget == EditTarget::TemplateRect) {
            m_templateIncludeEdited = true;
            m_templateRoi = roi;
            m_templatePolygon.clear();
            m_templateRegionType = QStringLiteral("rectangle");
            markModelDirty();
        } else if (m_editTarget == EditTarget::TemplateMaskRect) {
            m_templateExcludeEdited = true;
            m_templateMaskRegionType = QStringLiteral("rectangle");
            m_templateMaskRoi = roi;
            m_templateMaskPolygon.clear();
            m_templateMaskCircle = CircleRoi();
            m_templateMaskClearButton->setEnabled(true);
            markModelDirty();
            ui->statusLabel->setText(tr("矩形模板屏蔽区域已更新，请重新创建模板"));
        } else if (m_editTarget == EditTarget::SearchRect) {
            m_searchRoi = roi;
            m_searchPolygon.clear();
            m_searchCircle = CircleRoi();
            m_searchRegionType = QStringLiteral("rectangle");
            syncCurrentBaseBinding();
            ui->statusLabel->setText(tr("搜索区域已更新"));
        }
    });
    connect(m_previewHelper, &FrameViewHelper::polygonChanged, this,
            [this](const QVector<QPointF> &points) {
        if (points.size() < 3)
            return;
        ensureV6Editing();
        if (m_editTarget == EditTarget::TemplatePolygon) {
            m_templateIncludeEdited = true;
            m_templatePolygon = points;
            m_templateRoi = boundingRect(points);
            m_templateRegionType = QStringLiteral("polygon");
            markModelDirty();
        } else if (m_editTarget == EditTarget::TemplateMaskPolygon) {
            m_templateExcludeEdited = true;
            m_templateMaskRegionType = QStringLiteral("polygon");
            m_templateMaskRoi = boundingRect(points);
            m_templateMaskPolygon = points;
            m_templateMaskCircle = CircleRoi();
            m_templateMaskClearButton->setEnabled(true);
            markModelDirty();
            ui->statusLabel->setText(tr("模板屏蔽区域已更新，请重新创建模板"));
        } else if (m_editTarget == EditTarget::SearchPolygon) {
            m_searchPolygon = points;
            m_searchRoi = boundingRect(points);
            m_searchCircle = CircleRoi();
            m_searchRegionType = QStringLiteral("polygon");
            syncCurrentBaseBinding();
            // FrameViewHelper 完成多边形后会结束单次草图；这里立即恢复绘制态，
            // 让用户无需重复点击工具即可直接绘制下一个替代区域。
            m_previewHelper->setPolygonDrawingEnabled(true);
            ui->statusLabel->setText(tr("多边形搜索区域已更新，可继续重画"));
        }
    });
    connect(m_previewHelper, &FrameViewHelper::circleChanged, this,
            [this](const CircleRoi &circle) {
        if (!circle.valid)
            return;
        ensureV6Editing();
        if (m_editTarget == EditTarget::TemplateMaskCircle) {
            m_templateExcludeEdited = true;
            m_templateMaskRegionType = QStringLiteral("circle");
            m_templateMaskCircle = circle;
            m_templateMaskRoi = circle.boundingRectNormalized;
            m_templateMaskPolygon.clear();
            m_templateMaskClearButton->setEnabled(true);
            markModelDirty();
            ui->statusLabel->setText(tr("圆形模板屏蔽区域已更新，请重新创建模板"));
        } else if (m_editTarget == EditTarget::SearchCircle) {
            m_searchCircle = circle;
            m_searchRoi = circle.boundingRectNormalized;
            m_searchPolygon.clear();
            m_searchRegionType = QStringLiteral("circle");
            syncCurrentBaseBinding();
            ui->statusLabel->setText(tr("圆形搜索区域已更新"));
        }
    });
    connect(m_previewHelper, &FrameViewHelper::roiSelectionRejected, this,
            [this]() { ui->statusLabel->setText(tr("ROI 无效，请绘制宽高至少 2 像素的区域")); });
    connect(m_previewHelper, &FrameViewHelper::polygonSelectionRejected, this,
            [this]() { ui->statusLabel->setText(tr("多边形至少需要 3 个点")); });
    connect(m_previewHelper, &FrameViewHelper::circleSelectionRejected, this,
            [this]() { ui->statusLabel->setText(tr("圆形区域半径至少需要 2 像素")); });

    connect(ui->contrastModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() {
        updateContrastControls();
        handleParameterEditorChanged(true);
    });

    connect(ui->maxMatchesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
        if (!m_loadingConfig)
            ensureV6Editing();
        ui->minMatchCountSpinBox->setMaximum(value);
        ui->maxMatchCountSpinBox->setMaximum(value);
        if (ui->minMatchCountSpinBox->value() > value)
            ui->minMatchCountSpinBox->setValue(value);
        if (ui->maxMatchCountSpinBox->value() > value)
            ui->maxMatchCountSpinBox->setValue(value);
    });
    connect(ui->minMatchCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
        if (!m_loadingConfig)
            ensureV6Editing();
        if (ui->maxMatchCountSpinBox->value() < value)
            ui->maxMatchCountSpinBox->setValue(value);
    });
    connect(ui->maxMatchCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
        if (!m_loadingConfig)
            ensureV6Editing();
        if (ui->minMatchCountSpinBox->value() > value)
            ui->minMatchCountSpinBox->setValue(value);
    });

    connect(m_matchResultTable, &QTableWidget::cellClicked, this,
            [this](int row, int) {
        if (m_lastDisplayResult.overlays.isEmpty())
            return;
        const QTableWidgetItem *identityItem = m_matchResultTable->item(row, 0);
        const QString selectedMatchId = identityItem
                ? identityItem->data(kMatchIdRole).toString() : QString();
        QVector<ToolOverlay> overlays = m_lastDisplayResult.overlays;
        for (ToolOverlay &overlay : overlays) {
            const QString overlayMatchId = overlay.extra.value(
                        QStringLiteral("matchId")).toString();
            const bool hasStableIdentity = !selectedMatchId.isEmpty() &&
                    !overlayMatchId.isEmpty();
            if (!hasStableIdentity &&
                    !overlay.extra.contains(QStringLiteral("matchIndex")))
                continue;
            overlay.extra.insert(QStringLiteral("emphasis"),
                                 (hasStableIdentity
                                  ? overlayMatchId == selectedMatchId
                                  : overlay.extra.value(QStringLiteral("matchIndex")).toInt() == row)
                                 ? QStringLiteral("active") : QStringLiteral("muted"));
        }
        m_previewHelper->setToolOverlays(overlays);
    });

    const QList<QSpinBox *> modelSpinBoxes{ui->angleMinSpinBox, ui->angleMaxSpinBox,
                                           ui->scaleMinSpinBox, ui->scaleMaxSpinBox,
                                           ui->contrastSpinBox, ui->minContrastSpinBox,
                                           ui->numLevelsSpinBox};
    for (QSpinBox *spinBox : modelSpinBoxes) {
        connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this]() { handleParameterEditorChanged(true); });
    }
    connect(ui->polarityComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { handleParameterEditorChanged(true); });

    connect(ui->minScoreSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this]() { handleParameterEditorChanged(false); });
    connect(ui->subPixelComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { handleParameterEditorChanged(false); });
    connect(ui->greedinessSpinBox,
            QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() { handleParameterEditorChanged(false); });
    connect(ui->maxOverlapSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this]() { handleParameterEditorChanged(false); });
    connect(ui->timeoutSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this]() {
        if (!m_loadingConfig)
            ensureV6Editing();
    });

    connect(ui->createTemplateButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::createTemplate);
    connect(ui->deleteTemplateButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::deleteTemplate);
    connect(ui->referenceTestButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::runReferenceTest);
    connect(ui->testRunButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::toggleContinuousTest);
    connect(ui->finishButton, &QPushButton::clicked, this, [this]() {
        stopEditing();
        flushActiveTemplateEditor();
        QString message;
        if (!validateTemplateBank(true, &message) || !validateParameters(&message)) {
            ui->statusLabel->setText(message);
            return;
        }
        commitPendingCacheDeletes();
        accept();
    });
}

QString TemplateLocationDialog::selectedBaseId() const
{
    QString baseId = m_baseSelector
            ? m_baseSelector->selectedBaseId().trimmed() : QString();
    if (baseId.isEmpty())
        baseId = m_selectedBaseId.trimmed();
    if (baseId.isEmpty())
        baseId = ReferenceImageProvider::instance().primaryBaseId().trimmed();
    return baseId.isEmpty() ? QStringLiteral("base-0") : baseId;
}

QString TemplateLocationDialog::activeTemplateBaseId() const
{
    const int index = activeTemplateIndex();
    if (index >= 0 && !m_templates.at(index).sourceBaseId.trimmed().isEmpty())
        return m_templates.at(index).sourceBaseId.trimmed();
    // An existing v6 item owns an explicit immutable Base identity.  A missing
    // sourceBaseId is an invalid editable state, not permission to execute it
    // against whichever Base happens to be selected (or is currently primary).
    if (m_contractVersion ==
            TemplateLocationConfig::CompositeBankParamsVersion) {
        return QString();
    }
    return selectedBaseId();
}

bool TemplateLocationDialog::activeTemplateSourceAvailable() const
{
    if (activeTemplateIndex() < 0)
        return false;
    if (m_contractVersion !=
            TemplateLocationConfig::CompositeBankParamsVersion) {
        return ReferenceImageProvider::instance().hasReferenceFrame(
                    activeTemplateBaseId())
                || ReferenceImageProvider::instance().hasReferenceFrame();
    }
    const QString sourceBaseId = activeTemplateBaseId();
    return !sourceBaseId.isEmpty()
            && ReferenceImageProvider::instance().hasReferenceFrame(
                sourceBaseId);
}

bool TemplateLocationDialog::isTemplateGeometryTarget(EditTarget target) const
{
    return target == EditTarget::TemplateRect
            || target == EditTarget::TemplatePolygon
            || target == EditTarget::TemplateMaskRect
            || target == EditTarget::TemplateMaskCircle
            || target == EditTarget::TemplateMaskPolygon;
}

QString TemplateLocationDialog::baseDisplayName(const QString &baseId) const
{
    const QList<ReferenceFrameEntry> entries =
            ReferenceImageProvider::instance().referenceFrames();
    for (int index = 0; index < entries.size(); ++index) {
        const ReferenceFrameEntry &entry = entries.at(index);
        if (entry.baseId != baseId)
            continue;
        const QString name = entry.name.trimmed().isEmpty()
                ? tr("Base %1").arg(index + 1) : entry.name.trimmed();
        return tr("B%1 · %2").arg(index + 1, 2, 10, QLatin1Char('0'))
                .arg(name);
    }
    return baseId.trimmed().isEmpty() ? tr("未绑定 Base") : baseId;
}

void TemplateLocationDialog::seedBaseBindingsFromProvider()
{
    const QList<ReferenceFrameEntry> entries =
            ReferenceImageProvider::instance().referenceFrames();
    for (int index = 0; index < entries.size(); ++index) {
        const ReferenceFrameEntry &entry = entries.at(index);
        if (entry.baseId.trimmed().isEmpty()) {
            continue;
        }
        bool exists = false;
        for (const TemplateLocationBaseBindingConfig &binding :
             qAsConst(m_baseBindings)) {
            if (binding.baseId == entry.baseId) {
                exists = true;
                break;
            }
        }
        if (exists)
            continue;
        TemplateLocationBaseBindingConfig binding;
        binding.baseId = entry.baseId;
        binding.order = nextBaseBindingOrder(m_baseBindings);
        binding.searchRegionType = m_searchRegionType;
        binding.searchRoiNormalized = m_searchRoi;
        binding.searchPolygonNormalized = m_searchPolygon;
        binding.searchCircleCenterNormalized = m_searchCircle.centerNormalized;
        binding.searchCircleRadiusNormalized = m_searchCircle.radiusNormalized;
        binding.originMode = m_originMode;
        binding.customOriginNormalized = m_customOriginNormalized;
        m_baseBindings.append(binding);
    }
    if (!m_baseBindings.isEmpty())
        return;
    TemplateLocationBaseBindingConfig binding;
    binding.baseId = selectedBaseId();
    binding.order = 0;
    binding.searchRegionType = m_searchRegionType;
    binding.searchRoiNormalized = m_searchRoi;
    binding.searchPolygonNormalized = m_searchPolygon;
    binding.searchCircleCenterNormalized = m_searchCircle.centerNormalized;
    binding.searchCircleRadiusNormalized = m_searchCircle.radiusNormalized;
    binding.originMode = m_originMode;
    binding.customOriginNormalized = m_customOriginNormalized;
    m_baseBindings.append(binding);
}

void TemplateLocationDialog::syncCurrentBaseBinding()
{
    if (m_contractVersion != TemplateLocationConfig::CompositeBankParamsVersion)
        return;
    const QString baseId = m_selectedBaseId.trimmed().isEmpty()
            ? selectedBaseId() : m_selectedBaseId;
    if (baseId.isEmpty())
        return;
    TemplateLocationBaseBindingConfig *binding = nullptr;
    for (TemplateLocationBaseBindingConfig &candidate : m_baseBindings) {
        if (candidate.baseId == baseId) {
            binding = &candidate;
            break;
        }
    }
    if (!binding) {
        TemplateLocationBaseBindingConfig added;
        added.baseId = baseId;
        added.order = nextBaseBindingOrder(m_baseBindings);
        m_baseBindings.append(added);
        binding = &m_baseBindings.last();
    }
    binding->searchRegionType = m_searchRegionType;
    binding->searchRoiNormalized = m_searchRoi;
    binding->searchPolygonNormalized = m_searchPolygon;
    binding->searchCircleCenterNormalized = m_searchCircle.centerNormalized;
    binding->searchCircleRadiusNormalized = m_searchCircle.radiusNormalized;
    binding->originMode = m_originMode;
    binding->customOriginNormalized = m_customOriginNormalized;
}

void TemplateLocationDialog::loadSelectedBaseBinding()
{
    if (m_contractVersion != TemplateLocationConfig::CompositeBankParamsVersion)
        return;
    const TemplateLocationBaseBindingConfig *binding = nullptr;
    for (const TemplateLocationBaseBindingConfig &candidate :
         qAsConst(m_baseBindings)) {
        if (candidate.baseId == m_selectedBaseId) {
            binding = &candidate;
            break;
        }
    }
    if (!binding)
        return;
    m_searchRegionType = binding->searchRegionType;
    m_searchRoi = binding->searchRoiNormalized;
    m_searchPolygon = binding->searchPolygonNormalized;
    m_searchCircle.centerNormalized = binding->searchCircleCenterNormalized;
    m_searchCircle.radiusNormalized = binding->searchCircleRadiusNormalized;
    m_searchCircle.boundingRectNormalized = QRectF(
                m_searchCircle.centerNormalized.x() -
                    m_searchCircle.radiusNormalized,
                m_searchCircle.centerNormalized.y() -
                    m_searchCircle.radiusNormalized,
                m_searchCircle.radiusNormalized * 2.0,
                m_searchCircle.radiusNormalized * 2.0);
    m_searchCircle.valid = m_searchRegionType == QStringLiteral("circle")
            && m_searchCircle.radiusNormalized > 0.0
            && validRect(m_searchCircle.boundingRectNormalized);
    m_originMode = binding->originMode;
    m_customOriginNormalized = binding->customOriginNormalized;
    const QSignalBlocker blocker(ui->originModeComboBox);
    ui->originModeComboBox->setCurrentIndex(
                m_originMode == QStringLiteral("custom") ? 1 : 0);
    updateOriginControls();
}

QVector<TemplateLocationBaseBindingConfig>
TemplateLocationDialog::baseBindingsForOutput() const
{
    QVector<TemplateLocationBaseBindingConfig> bindings = m_baseBindings;
    if (m_contractVersion != TemplateLocationConfig::CompositeBankParamsVersion)
        return bindings;
    QSet<QString> referencedBaseIds;
    for (const TemplateItemState &item : qAsConst(m_templates)) {
        const QString sourceBaseId = item.sourceBaseId.trimmed();
        if (!sourceBaseId.isEmpty())
            referencedBaseIds.insert(sourceBaseId);
    }
    const QString baseId = selectedBaseId();
    if (!referencedBaseIds.contains(baseId)) {
        QVector<TemplateLocationBaseBindingConfig> filtered;
        for (const TemplateLocationBaseBindingConfig &binding :
             qAsConst(bindings)) {
            if (referencedBaseIds.contains(binding.baseId))
                filtered.append(binding);
        }
        return filtered;
    }
    bool found = false;
    for (TemplateLocationBaseBindingConfig &binding : bindings) {
        if (binding.baseId != baseId)
            continue;
        binding.searchRegionType = m_searchRegionType;
        binding.searchRoiNormalized = m_searchRoi;
        binding.searchPolygonNormalized = m_searchPolygon;
        binding.searchCircleCenterNormalized = m_searchCircle.centerNormalized;
        binding.searchCircleRadiusNormalized = m_searchCircle.radiusNormalized;
        binding.originMode = m_originMode;
        binding.customOriginNormalized = m_customOriginNormalized;
        found = true;
        break;
    }
    if (!found) {
        TemplateLocationBaseBindingConfig binding;
        binding.baseId = baseId;
        binding.order = nextBaseBindingOrder(bindings);
        binding.searchRegionType = m_searchRegionType;
        binding.searchRoiNormalized = m_searchRoi;
        binding.searchPolygonNormalized = m_searchPolygon;
        binding.searchCircleCenterNormalized = m_searchCircle.centerNormalized;
        binding.searchCircleRadiusNormalized = m_searchCircle.radiusNormalized;
        binding.originMode = m_originMode;
        binding.customOriginNormalized = m_customOriginNormalized;
        bindings.append(binding);
    }
    QVector<TemplateLocationBaseBindingConfig> filtered;
    for (const TemplateLocationBaseBindingConfig &binding :
         qAsConst(bindings)) {
        if (referencedBaseIds.contains(binding.baseId))
            filtered.append(binding);
    }
    return filtered;
}

TemplateLocationMatchParameters
TemplateLocationDialog::matchParametersFromUi(
        const TemplateLocationMatchParameters *preserved) const
{
    TemplateLocationMatchParameters parameters;
    if (preserved)
        parameters.extra = preserved->extra;
    parameters.minScore = ui->minScoreSpinBox->value();
    parameters.angleStart = ui->angleMinSpinBox->value();
    parameters.angleExtent = ui->angleMaxSpinBox->value()
            - ui->angleMinSpinBox->value();
    parameters.scaleMin = ui->scaleMinSpinBox->value();
    parameters.scaleMax = ui->scaleMaxSpinBox->value();
    parameters.polarity = polarityValue();
    parameters.contrastMode = contrastModeValue();
    parameters.contrast = ui->contrastSpinBox->value();
    parameters.minContrast = ui->minContrastSpinBox->value();
    parameters.numLevels = ui->numLevelsSpinBox->value();
    parameters.subPixel = ui->subPixelComboBox->currentText();
    parameters.greediness = ui->greedinessSpinBox->value();
    parameters.maxOverlap = ui->maxOverlapSpinBox->value() / 100.0;
    return parameters;
}

void TemplateLocationDialog::applyMatchParametersToUi(
        const TemplateLocationMatchParameters &parameters)
{
    m_updatingParameterEditor = true;
    const QSignalBlocker minScoreBlock(ui->minScoreSpinBox);
    const QSignalBlocker angleMinBlock(ui->angleMinSpinBox);
    const QSignalBlocker angleMaxBlock(ui->angleMaxSpinBox);
    const QSignalBlocker scaleMinBlock(ui->scaleMinSpinBox);
    const QSignalBlocker scaleMaxBlock(ui->scaleMaxSpinBox);
    const QSignalBlocker polarityBlock(ui->polarityComboBox);
    const QSignalBlocker contrastModeBlock(ui->contrastModeComboBox);
    const QSignalBlocker contrastBlock(ui->contrastSpinBox);
    const QSignalBlocker minContrastBlock(ui->minContrastSpinBox);
    const QSignalBlocker levelsBlock(ui->numLevelsSpinBox);
    const QSignalBlocker subPixelBlock(ui->subPixelComboBox);
    const QSignalBlocker greedinessBlock(ui->greedinessSpinBox);
    const QSignalBlocker overlapBlock(ui->maxOverlapSpinBox);
    ui->minScoreSpinBox->setValue(parameters.minScore);
    ui->angleMinSpinBox->setValue(parameters.angleStart);
    ui->angleMaxSpinBox->setValue(parameters.angleStart
                                  + parameters.angleExtent);
    ui->scaleMinSpinBox->setValue(parameters.scaleMin);
    ui->scaleMaxSpinBox->setValue(parameters.scaleMax);
    setPolarityValue(parameters.polarity);
    ui->contrastModeComboBox->setCurrentIndex(
                parameters.contrastMode == QStringLiteral("manual") ? 1 : 0);
    ui->contrastSpinBox->setValue(parameters.contrast);
    ui->minContrastSpinBox->setValue(parameters.minContrast);
    ui->numLevelsSpinBox->setValue(parameters.numLevels);
    ui->subPixelComboBox->setCurrentIndex(
                parameters.subPixel == QStringLiteral("none") ? 1 : 0);
    ui->greedinessSpinBox->setValue(parameters.greediness);
    ui->maxOverlapSpinBox->setValue(
                qRound(parameters.maxOverlap * 100.0));
    updateContrastControls();
    m_updatingParameterEditor = false;
}

void TemplateLocationDialog::flushParameterEditor()
{
    if (m_updatingParameterEditor)
        return;
    const int index = activeTemplateIndex();
    if (index >= 0 && m_templates.at(index).useIndependentParameters) {
        const TemplateLocationMatchParameters previous =
                m_templates.at(index).independentParameters;
        m_templates[index].independentParameters =
                matchParametersFromUi(&previous);
        m_templates[index].independentParametersComplete = true;
    } else {
        const TemplateLocationMatchParameters previous = m_sharedParameters;
        m_sharedParameters = matchParametersFromUi(&previous);
    }
}

void TemplateLocationDialog::loadParameterEditor()
{
    const int index = activeTemplateIndex();
    const bool independent = index >= 0
            && m_templates.at(index).useIndependentParameters;
    const TemplateLocationMatchParameters parameters = independent
            ? m_templates.at(index).independentParameters
            : m_sharedParameters;
    applyMatchParametersToUi(parameters);
    if (m_independentParametersCheckBox) {
        const QSignalBlocker blocker(m_independentParametersCheckBox);
        m_independentParametersCheckBox->setChecked(independent);
        m_independentParametersCheckBox->setEnabled(index >= 0);
    }
    if (m_parameterScopeLabel) {
        m_parameterScopeLabel->setText(independent
                                       ? tr("当前：模板独立参数")
                                       : tr("当前：共享参数"));
    }
}

void TemplateLocationDialog::handleParameterEditorChanged(bool rebuildModels)
{
    if (m_loadingConfig || m_updatingParameterEditor)
        return;
    ensureV6Editing();
    const int index = activeTemplateIndex();
    const bool independent = index >= 0
            && m_templates.at(index).useIndependentParameters;
    flushParameterEditor();
    if (rebuildModels) {
        if (independent)
            markModelDirty();
        else
            markAllModelsDirty();
    } else {
        invalidateRunPreview();
    }
}

void TemplateLocationDialog::ensureV6Editing()
{
    if (m_loadingConfig || m_configReadOnly
            || m_contractVersion ==
               TemplateLocationConfig::CompositeBankParamsVersion) {
        return;
    }
    const TemplateLocationMatchParameters previousShared = m_sharedParameters;
    m_sharedParameters = matchParametersFromUi(&previousShared);
    m_contractVersion = TemplateLocationConfig::CompositeBankParamsVersion;
    m_usesTemplateBankSchema = true;
    seedBaseBindingsFromProvider();
    const QString defaultBaseId =
            ReferenceImageProvider::instance().primaryBaseId().trimmed().isEmpty()
            ? selectedBaseId()
            : ReferenceImageProvider::instance().primaryBaseId();
    for (int index = 0; index < m_templates.size(); ++index) {
        TemplateItemState &item = m_templates[index];
        if (item.sourceBaseId.trimmed().isEmpty())
            item.sourceBaseId = defaultBaseId;
        item.order = index;
        item.priority = index;
        if (item.includeRegions.isEmpty()) {
            TemplateLocationRegionConfig include;
            include.regionType = item.regionType;
            include.roiNormalized = item.roi;
            include.polygonNormalized = item.polygon;
            item.includeRegions.append(include);
        }
        if (item.excludeRegions.isEmpty()
                && item.maskRegionType != QStringLiteral("none")) {
            TemplateLocationRegionConfig exclude;
            exclude.regionType = item.maskRegionType;
            exclude.roiNormalized = item.maskRoi;
            exclude.polygonNormalized = item.maskPolygon;
            exclude.circleCenterNormalized = item.maskCircle.centerNormalized;
            exclude.circleRadiusNormalized = item.maskCircle.radiusNormalized;
            item.excludeRegions.append(exclude);
        }
        item.useIndependentParameters = false;
        item.independentParameters = m_sharedParameters;
        item.independentParametersComplete = true;
    }
    syncCurrentBaseBinding();
    freezeCentroidOriginsForReferencedBases();
    loadSelectedBaseBinding();
    refreshTemplateList();
}

void TemplateLocationDialog::freezeCentroidOriginsForReferencedBases()
{
    if (m_contractVersion !=
            TemplateLocationConfig::CompositeBankParamsVersion) {
        return;
    }

    QSet<QString> referencedBaseIds;
    for (const TemplateItemState &item : qAsConst(m_templates)) {
        const QString sourceBaseId = item.sourceBaseId.trimmed();
        if (!sourceBaseId.isEmpty())
            referencedBaseIds.insert(sourceBaseId);
    }
    if (m_templates.size() <= 1 && referencedBaseIds.size() <= 1)
        return;

    for (const QString &baseId : qAsConst(referencedBaseIds)) {
        TemplateLocationBaseBindingConfig *binding = nullptr;
        for (TemplateLocationBaseBindingConfig &candidate : m_baseBindings) {
            if (candidate.baseId == baseId) {
                binding = &candidate;
                break;
            }
        }
        if (!binding) {
            TemplateLocationBaseBindingConfig added;
            added.baseId = baseId;
            added.order = nextBaseBindingOrder(m_baseBindings);
            added.searchRegionType = m_searchRegionType;
            added.searchRoiNormalized = m_searchRoi;
            added.searchPolygonNormalized = m_searchPolygon;
            added.searchCircleCenterNormalized =
                    m_searchCircle.centerNormalized;
            added.searchCircleRadiusNormalized =
                    m_searchCircle.radiusNormalized;
            added.originMode = m_originMode;
            added.customOriginNormalized = m_customOriginNormalized;
            m_baseBindings.append(added);
            binding = &m_baseBindings.last();
        }
        if (binding->originMode == QStringLiteral("custom")
                && validNormalizedPoint(binding->customOriginNormalized)) {
            continue;
        }

        QPointF origin = validNormalizedPoint(binding->customOriginNormalized)
                ? binding->customOriginNormalized : QPointF(0.5, 0.5);
        for (const TemplateItemState &candidate : qAsConst(m_templates)) {
            if (candidate.sourceBaseId != baseId || !validRect(candidate.roi))
                continue;
            const QPointF candidateOrigin =
                    candidate.regionType == QStringLiteral("polygon")
                    && candidate.polygon.size() >= 3
                    ? polygonCentroid(candidate.polygon)
                    : candidate.roi.center();
            if (validNormalizedPoint(candidateOrigin))
                origin = candidateOrigin;
            break;
        }
        binding->originMode = QStringLiteral("custom");
        binding->customOriginNormalized = origin;
    }
}

TemplateLocationTemplateConfig TemplateLocationDialog::templateConfigFromState(
        const TemplateItemState &item) const
{
    TemplateLocationTemplateConfig config;
    config.templateId = item.templateId;
    config.name = item.name;
    config.enabled = item.enabled;
    config.priority = item.priority;
    config.order = item.order;
    config.sourceBaseId = item.sourceBaseId;
    config.templateRegionType = item.regionType;
    config.templateRoiNormalized = item.roi;
    config.templatePolygonNormalized = item.polygon;
    config.templateMaskRegionType = item.maskRegionType;
    config.templateMaskRoiNormalized = item.maskRoi;
    config.templateMaskPolygonNormalized = item.maskPolygon;
    config.templateMaskCircleCenterNormalized =
            item.maskCircle.centerNormalized;
    config.templateMaskCircleRadiusNormalized =
            item.maskCircle.radiusNormalized;
    config.modelCacheKey = item.modelCacheKey;
    config.modelCreated = item.modelCreated;
    config.includeRegions = item.includeRegions;
    config.excludeRegions = item.excludeRegions;
    config.useIndependentParameters = item.useIndependentParameters;
    config.independentParameters = item.independentParameters;
    config.independentParametersComplete = item.independentParametersComplete;
    config.extra = item.extra;
    return config;
}

TemplateLocationDialog::TemplateItemState
TemplateLocationDialog::templateStateFromConfig(
        const TemplateLocationTemplateConfig &config,
        int fallbackIndex) const
{
    TemplateItemState item;
    item.templateId = config.templateId.trimmed().isEmpty()
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : config.templateId;
    item.name = config.name.trimmed().isEmpty()
            ? tr("模板%1").arg(fallbackIndex + 1) : config.name;
    item.enabled = config.enabled;
    item.priority = config.priority;
    item.order = config.order;
    item.sourceBaseId = config.sourceBaseId;
    item.regionType = config.templateRegionType;
    item.roi = config.templateRoiNormalized;
    item.polygon = config.templatePolygonNormalized;
    item.maskRegionType = config.templateMaskRegionType;
    item.maskRoi = config.templateMaskRoiNormalized;
    item.maskPolygon = config.templateMaskPolygonNormalized;
    item.maskCircle.centerNormalized =
            config.templateMaskCircleCenterNormalized;
    item.maskCircle.radiusNormalized =
            config.templateMaskCircleRadiusNormalized;
    item.maskCircle.boundingRectNormalized = QRectF(
                item.maskCircle.centerNormalized.x()
                    - item.maskCircle.radiusNormalized,
                item.maskCircle.centerNormalized.y()
                    - item.maskCircle.radiusNormalized,
                item.maskCircle.radiusNormalized * 2.0,
                item.maskCircle.radiusNormalized * 2.0);
    item.maskCircle.valid = item.maskRegionType == QStringLiteral("circle")
            && item.maskCircle.radiusNormalized > 0.0
            && validRect(item.maskCircle.boundingRectNormalized);
    item.modelCacheKey = config.modelCacheKey.trimmed().isEmpty()
            ? QStringLiteral("template_location_%1")
              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
            : config.modelCacheKey;
    item.modelCreated = config.modelCreated;
    item.includeRegions = config.includeRegions;
    item.excludeRegions = config.excludeRegions;
    item.useIndependentParameters = config.useIndependentParameters;
    item.independentParameters = config.independentParameters;
    item.independentParametersComplete =
            config.independentParametersComplete;
    item.extra = config.extra;
    return item;
}

TemplateLocationModelBankConfig TemplateLocationDialog::modelBankForOutput() const
{
    TemplateLocationModelBankConfig bank =
            TemplateLocationConfig::fromToolConfig(m_config);
    if (!bank.decodeSupported || bank.rawPassthrough)
        return bank;
    bank.version = m_contractVersion;
    bank.templateMode = QStringLiteral("alternatives");
    bank.primaryMatchStrategy = m_contractVersion ==
            TemplateLocationConfig::CompositeBankParamsVersion
            ? QStringLiteral("first_valid") : bank.primaryMatchStrategy;
    bank.primaryTemplateId = m_contractVersion ==
            TemplateLocationConfig::CompositeBankParamsVersion
            ? QString() : bank.primaryTemplateId;

    TemplateLocationMatchParameters shared = m_sharedParameters;
    const int activeIndex = activeTemplateIndex();
    if (activeIndex >= 0
            && !m_templates.at(activeIndex).useIndependentParameters)
        shared = matchParametersFromUi(&m_sharedParameters);
    bank.minScore = shared.minScore;
    bank.angleStart = shared.angleStart;
    bank.angleExtent = shared.angleExtent;
    bank.scaleMin = shared.scaleMin;
    bank.scaleMax = shared.scaleMax;
    bank.polarity = shared.polarity;
    bank.contrastMode = shared.contrastMode;
    bank.contrast = shared.contrast;
    bank.minContrast = shared.minContrast;
    bank.numLevels = shared.numLevels;
    bank.subPixel = shared.subPixel;
    bank.greediness = shared.greediness;
    bank.maxOverlap = shared.maxOverlap;
    bank.timeoutMs = ui->timeoutSpinBox->value();
    bank.maxMatches = ui->maxMatchesSpinBox->value();
    bank.minMatchCount = ui->minMatchCountSpinBox->value();
    bank.maxMatchCount = ui->maxMatchCountSpinBox->value();

    QList<TemplateItemState> states = m_templates;
    if (activeIndex >= 0 && activeIndex < states.size()) {
        TemplateItemState active = activeTemplateForOutput();
        states.replace(activeIndex, active);
    }
    bank.templates.clear();
    for (const TemplateItemState &state : qAsConst(states))
        bank.templates.append(templateConfigFromState(state));

    if (bank.version == TemplateLocationConfig::CompositeBankParamsVersion) {
        bool sourceIdentitiesComplete = true;
        for (const TemplateLocationTemplateConfig &item :
             qAsConst(bank.templates)) {
            if (item.sourceBaseId.trimmed().isEmpty()) {
                sourceIdentitiesComplete = false;
                break;
            }
        }
        // Do not silently bind a malformed persisted v6 template to the first
        // Base.  Preserve the incomplete identity so validation can report it
        // and the user can explicitly recreate/rebind the item.
        if (sourceIdentitiesComplete) {
            bank.baseBindings = baseBindingsForOutput();
            TemplateLocationConfig::ensureStableV6Ids(&bank);
        } else {
            bank.baseBindings = m_baseBindings;
        }
    } else {
        bank.baseBindings.clear();
        bank.searchRegionType = m_searchRegionType;
        bank.searchRoiNormalized = m_searchRoi;
        bank.searchPolygonNormalized = m_searchPolygon;
        bank.searchCircleCenterNormalized = m_searchCircle.centerNormalized;
        bank.searchCircleRadiusNormalized = m_searchCircle.radiusNormalized;
        bank.originMode = m_originMode;
        bank.customOriginNormalized = m_customOriginNormalized;
        TemplateLocationConfig::ensureStableTemplateIds(&bank);
    }
    return bank;
}

void TemplateLocationDialog::validateAllReferenceBases()
{
    stopEditing();
    flushActiveTemplateEditor();
    QString message;
    if (!validateTemplateBank(true, &message) || !validateParameters(&message)) {
        ui->statusLabel->setText(message);
        return;
    }
    const ReferenceFrameSetSnapshot referenceSnapshot =
            ReferenceImageProvider::instance().referenceFrameSetSnapshot();
    int validCount = 0;
    for (const ReferenceFrameEntry &base : referenceSnapshot.entries) {
        if (base.frame.empty())
            continue;
        runOnFrame(base.frame,
                   tr("验证 %1").arg(baseDisplayName(base.baseId)),
                   base.baseId,
                   &referenceSnapshot);
        if (m_lastDisplayResult.success && m_lastDisplayResult.ok)
            ++validCount;
    }
    ui->statusLabel->setText(tr("全部 Base 验证完成：%1/%2 通过")
                             .arg(validCount).arg(referenceSnapshot.entries.size()));
}

void TemplateLocationDialog::setAdvancedVisible(bool visible)
{
    if (!visible && (m_editTarget == EditTarget::TemplateMaskRect ||
                     m_editTarget == EditTarget::TemplateMaskCircle ||
                     m_editTarget == EditTarget::TemplateMaskPolygon))
        stopEditing();
    ui->advancedCard->setVisible(visible);
    if (m_templateMaskRow)
        m_templateMaskRow->setVisible(visible);
    ui->basicModeButton->setChecked(!visible);
    ui->allModeButton->setChecked(visible);
}

void TemplateLocationDialog::startEditing(EditTarget target)
{
    if (target == EditTarget::SearchGlobal) {
        if (m_running)
            toggleContinuousTest();
        stopEditing();
        ensureV6Editing();
        m_searchRegionType = QStringLiteral("full");
        m_searchRoi = QRectF(0.0, 0.0, 1.0, 1.0);
        m_searchPolygon.clear();
        m_searchCircle = CircleRoi();
        syncCurrentBaseBinding();
        ui->searchGlobalButton->setChecked(true);
        m_previewHelper->clearRoi();
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->clearCircleRoi();
        showReferenceImage();
        ui->statusLabel->setText(tr("已切换为全局搜索"));
        return;
    }
    const bool templateGeometry = isTemplateGeometryTarget(target);
    const QString editBaseId = templateGeometry
            ? activeTemplateBaseId() : selectedBaseId();
    if (editBaseId.isEmpty()
            || !ReferenceImageProvider::instance().hasReferenceFrame(
                editBaseId)) {
        if (templateGeometry
                && m_contractVersion ==
                   TemplateLocationConfig::CompositeBankParamsVersion) {
            ui->statusLabel->setText(
                        tr("当前模板来源 Base 不存在，无法编辑模板区域"));
        } else {
            ui->statusLabel->setText(tr("请先设置基准图"));
        }
        showReferenceImage();
        clearButtonChecks(m_templateGroup);
        clearButtonChecks(m_searchGroup);
        return;
    }
    if (m_running)
        toggleContinuousTest();

    // 已选模式保持高亮且持续有效；每次拖绘都会直接替换上一次 ROI。
    if (m_editTarget == target)
        return;
    stopEditing();

    m_editTarget = target;
    showReferenceImage();
    if (target == EditTarget::TemplateRect ||
            target == EditTarget::TemplateMaskRect ||
            target == EditTarget::SearchRect) {
        const bool isTemplate = target == EditTarget::TemplateRect;
        const bool isTemplateMask = target == EditTarget::TemplateMaskRect;
        if (isTemplateMask)
            m_templateMaskRectButton->setChecked(true);
        else
            (isTemplate ? ui->templateRectButton : ui->searchRectButton)->setChecked(true);
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->clearCircleRoi();
        const QRectF roi = isTemplateMask ? m_templateMaskRoi
                                          : (isTemplate ? m_templateRoi : m_searchRoi);
        if (validRect(roi))
            m_previewHelper->setRoiRectNormalized(roi);
        else
            m_previewHelper->clearRoi();
        m_previewHelper->setRoiDrawingEnabled(true);
        ui->viewerTitleLabel->setText(isTemplateMask
                ? tr("绘制矩形模板屏蔽区域 · %1")
                  .arg(baseDisplayName(editBaseId))
                : (isTemplate
                   ? tr("绘制模板矩形 · %1")
                     .arg(baseDisplayName(editBaseId))
                   : tr("绘制搜索矩形")));
    } else if (target == EditTarget::TemplatePolygon ||
               target == EditTarget::TemplateMaskPolygon ||
               target == EditTarget::SearchPolygon) {
        const bool isTemplate = target == EditTarget::TemplatePolygon;
        const bool isTemplateMask = target == EditTarget::TemplateMaskPolygon;
        if (isTemplateMask)
            m_templateMaskPolygonButton->setChecked(true);
        else
            (isTemplate ? ui->templatePolygonButton
                        : ui->searchPolygonButton)->setChecked(true);
        m_previewHelper->clearRoi();
        m_previewHelper->clearCircleRoi();
        const QVector<QPointF> &polygon = isTemplateMask
                ? m_templateMaskPolygon
                : (isTemplate ? m_templatePolygon : m_searchPolygon);
        if (polygon.size() >= 3)
            m_previewHelper->setPolygonRoiNormalized(polygon);
        else
            m_previewHelper->clearPolygonRoi();
        m_previewHelper->setPolygonDrawingEnabled(true);
        ui->viewerTitleLabel->setText(
                    isTemplateMask
                    ? tr("绘制模板屏蔽区域 · %1")
                      .arg(baseDisplayName(editBaseId))
                    : (isTemplate
                       ? tr("绘制模板多边形 · %1")
                         .arg(baseDisplayName(editBaseId))
                       : tr("绘制多边形搜索区域")));
    } else {
        const bool isTemplateMask = target == EditTarget::TemplateMaskCircle;
        (isTemplateMask ? m_templateMaskCircleButton
                        : ui->searchCircleButton)->setChecked(true);
        m_previewHelper->clearRoi();
        m_previewHelper->clearPolygonRoi();
        const CircleRoi &circle = isTemplateMask
                ? m_templateMaskCircle : m_searchCircle;
        if (circle.valid)
            m_previewHelper->setCircleRoiNormalized(circle);
        else
            m_previewHelper->clearCircleRoi();
        m_previewHelper->setCircleDrawingEnabled(true);
        ui->viewerTitleLabel->setText(isTemplateMask
                ? tr("绘制圆形模板屏蔽区域 · %1")
                  .arg(baseDisplayName(editBaseId))
                : tr("绘制圆形搜索区域"));
    }
}

void TemplateLocationDialog::stopEditing()
{
    if (!m_previewHelper)
        return;
    if (m_previewHelper->isPolygonDrawingEnabled())
        m_previewHelper->finishPolygonDrawing();
    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->setPointSelectionEnabled(false);
    ui->selectOriginButton->setChecked(false);
    m_editTarget = EditTarget::None;
    clearButtonChecks(m_templateGroup);
    clearButtonChecks(m_searchGroup);
    if (m_searchRegionType == QStringLiteral("full"))
        ui->searchGlobalButton->setChecked(true);
}

int TemplateLocationDialog::activeTemplateIndex() const
{
    for (int index = 0; index < m_templates.size(); ++index) {
        if (m_templates.at(index).templateId == m_activeTemplateId)
            return index;
    }
    return -1;
}

TemplateLocationDialog::TemplateItemState
TemplateLocationDialog::activeTemplateForOutput() const
{
    const int index = activeTemplateIndex();
    if (index < 0)
        return TemplateItemState();

    TemplateItemState item = m_templates.at(index);
    item.regionType = m_templateRegionType;
    item.roi = m_templateRoi;
    item.polygon = m_templatePolygon;
    item.maskRegionType = m_templateMaskRegionType;
    item.maskRoi = m_templateMaskRoi;
    item.maskPolygon = m_templateMaskPolygon;
    item.maskCircle = m_templateMaskCircle;
    item.modelCacheKey = m_modelCacheKey;
    item.modelCreated = m_modelCreated;
    item.displayOverlays = m_templateDisplayOverlays;
    if (m_contractVersion == TemplateLocationConfig::CompositeBankParamsVersion) {
        if (m_templateIncludeEdited) {
            TemplateLocationRegionConfig include;
            if (!item.includeRegions.isEmpty())
                include = item.includeRegions.first();
            include.regionType = item.regionType;
            include.roiNormalized = item.roi;
            include.polygonNormalized = item.polygon;
            include.circleCenterNormalized = QPointF();
            include.circleRadiusNormalized = 0.0;
            if (item.includeRegions.isEmpty())
                item.includeRegions.append(include);
            else
                item.includeRegions[0] = include;
        }
        if (m_templateExcludeEdited
                && item.maskRegionType == QStringLiteral("none")) {
            if (!item.excludeRegions.isEmpty())
                item.excludeRegions.removeFirst();
        } else if (m_templateExcludeEdited) {
            TemplateLocationRegionConfig exclude;
            if (!item.excludeRegions.isEmpty())
                exclude = item.excludeRegions.first();
            exclude.regionType = item.maskRegionType;
            exclude.roiNormalized = item.maskRoi;
            exclude.polygonNormalized = item.maskPolygon;
            exclude.circleCenterNormalized = item.maskCircle.centerNormalized;
            exclude.circleRadiusNormalized = item.maskCircle.radiusNormalized;
            if (item.excludeRegions.isEmpty())
                item.excludeRegions.append(exclude);
            else
                item.excludeRegions[0] = exclude;
        }
        if (item.useIndependentParameters) {
            const TemplateLocationMatchParameters previous =
                    item.independentParameters;
            item.independentParameters = matchParametersFromUi(&previous);
        }
    }
    return item;
}

void TemplateLocationDialog::flushActiveTemplateEditor()
{
    const int index = activeTemplateIndex();
    if (index < 0)
        return;
    TemplateItemState item = m_templates.at(index);
    item.regionType = m_templateRegionType;
    item.roi = m_templateRoi;
    item.polygon = m_templatePolygon;
    item.maskRegionType = m_templateMaskRegionType;
    item.maskRoi = m_templateMaskRoi;
    item.maskPolygon = m_templateMaskPolygon;
    item.maskCircle = m_templateMaskCircle;
    item.modelCacheKey = m_modelCacheKey;
    item.modelCreated = m_modelCreated;
    item.displayOverlays = m_templateDisplayOverlays;
    const bool removedFirstExclude =
            m_contractVersion ==
            TemplateLocationConfig::CompositeBankParamsVersion
            && m_templateExcludeEdited
            && item.maskRegionType == QStringLiteral("none");
    if (m_contractVersion == TemplateLocationConfig::CompositeBankParamsVersion) {
        if (m_templateIncludeEdited) {
            TemplateLocationRegionConfig include;
            if (!item.includeRegions.isEmpty())
                include = item.includeRegions.first();
            include.regionType = item.regionType;
            include.roiNormalized = item.roi;
            include.polygonNormalized = item.polygon;
            include.circleCenterNormalized = QPointF();
            include.circleRadiusNormalized = 0.0;
            if (item.includeRegions.isEmpty())
                item.includeRegions.append(include);
            else
                item.includeRegions[0] = include;
        }
        if (m_templateExcludeEdited
                && item.maskRegionType == QStringLiteral("none")) {
            if (!item.excludeRegions.isEmpty())
                item.excludeRegions.removeFirst();
        } else if (m_templateExcludeEdited) {
            TemplateLocationRegionConfig exclude;
            if (!item.excludeRegions.isEmpty())
                exclude = item.excludeRegions.first();
            exclude.regionType = item.maskRegionType;
            exclude.roiNormalized = item.maskRoi;
            exclude.polygonNormalized = item.maskPolygon;
            exclude.circleCenterNormalized = item.maskCircle.centerNormalized;
            exclude.circleRadiusNormalized = item.maskCircle.radiusNormalized;
            if (item.excludeRegions.isEmpty())
                item.excludeRegions.append(exclude);
            else
                item.excludeRegions[0] = exclude;
        }
        if (item.useIndependentParameters) {
            const TemplateLocationMatchParameters previous =
                    item.independentParameters;
            item.independentParameters = matchParametersFromUi(&previous);
            item.independentParametersComplete = true;
        } else {
            const TemplateLocationMatchParameters previous =
                    m_sharedParameters;
            m_sharedParameters = matchParametersFromUi(&previous);
        }
    }
    m_templates.replace(index, item);
    m_templateIncludeEdited = false;
    m_templateExcludeEdited = false;
    if (removedFirstExclude) {
        if (item.excludeRegions.isEmpty()) {
            m_templateMaskRegionType = QStringLiteral("none");
            m_templateMaskRoi = QRectF();
            m_templateMaskPolygon.clear();
            m_templateMaskCircle = CircleRoi();
        } else {
            const TemplateLocationRegionConfig &next =
                    item.excludeRegions.first();
            m_templateMaskRegionType = next.regionType;
            m_templateMaskRoi = next.roiNormalized;
            m_templateMaskPolygon = next.polygonNormalized;
            m_templateMaskCircle.centerNormalized =
                    next.circleCenterNormalized;
            m_templateMaskCircle.radiusNormalized =
                    next.circleRadiusNormalized;
            m_templateMaskCircle.boundingRectNormalized = QRectF(
                        next.circleCenterNormalized.x()
                        - next.circleRadiusNormalized,
                        next.circleCenterNormalized.y()
                        - next.circleRadiusNormalized,
                        next.circleRadiusNormalized * 2.0,
                        next.circleRadiusNormalized * 2.0);
            m_templateMaskCircle.valid =
                    next.regionType == QStringLiteral("circle")
                    && next.circleRadiusNormalized > 0.0
                    && validRect(
                        m_templateMaskCircle.boundingRectNormalized);
        }
    }
}

void TemplateLocationDialog::loadActiveTemplateEditor()
{
    const int index = activeTemplateIndex();
    if (index < 0)
        return;
    TemplateItemState &item = m_templates[index];
    if (item.modelCacheKey.trimmed().isEmpty()) {
        item.modelCacheKey = QStringLiteral("template_location_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    m_templateRegionType = item.regionType;
    m_templateRoi = item.roi;
    m_templatePolygon = item.polygon;
    m_templateMaskRegionType = item.maskRegionType;
    m_templateMaskRoi = item.maskRoi;
    m_templateMaskPolygon = item.maskPolygon;
    m_templateMaskCircle = item.maskCircle;
    m_modelCacheKey = item.modelCacheKey;
    m_modelCreated = item.modelCreated;
    m_templateDisplayOverlays = item.displayOverlays;
    m_templateIncludeEdited = false;
    m_templateExcludeEdited = false;
    loadParameterEditor();
    refreshActiveTemplateUi();
    showReferenceImage();
}

void TemplateLocationDialog::refreshActiveTemplateUi()
{
    const int index = activeTemplateIndex();
    const bool hasActive = index >= 0;
    const bool sourceAvailable = hasActive && activeTemplateSourceAvailable();
    ui->templateRectButton->setEnabled(sourceAvailable);
    ui->templatePolygonButton->setEnabled(sourceAvailable);
    if (m_templateMaskRectButton)
        m_templateMaskRectButton->setEnabled(sourceAvailable);
    if (m_templateMaskCircleButton)
        m_templateMaskCircleButton->setEnabled(sourceAvailable);
    if (m_templateMaskPolygonButton)
        m_templateMaskPolygonButton->setEnabled(sourceAvailable);
    if (m_independentParametersCheckBox)
        m_independentParametersCheckBox->setEnabled(hasActive);
    if (!hasActive) {
        ui->modelStatusLabel->setText(tr("未选择模板"));
        ui->createTemplateButton->setEnabled(false);
        ui->deleteTemplateButton->setEnabled(false);
        if (m_templateMaskClearButton)
            m_templateMaskClearButton->setEnabled(false);
        return;
    }

    const TemplateItemState item = activeTemplateForOutput();
    ui->createTemplateButton->setEnabled(sourceAvailable);
    ui->createTemplateButton->setText(item.modelCreated
                                      ? tr("重新创建模板") : tr("创建模板"));
    ui->deleteTemplateButton->setEnabled(item.modelCreated);
    if (!sourceAvailable
            && m_contractVersion ==
               TemplateLocationConfig::CompositeBankParamsVersion) {
        ui->modelStatusLabel->setText(
                    tr("模板来源 Base 不存在：%1")
                    .arg(item.sourceBaseId.trimmed().isEmpty()
                         ? tr("未绑定") : item.sourceBaseId));
    } else if (!item.modelError.trimmed().isEmpty())
        ui->modelStatusLabel->setText(tr("模板错误：%1").arg(item.modelError));
    else if (item.modelCreated)
        ui->modelStatusLabel->setText(tr("模板已创建"));
    else if (validRect(item.roi))
        ui->modelStatusLabel->setText(tr("需要创建模板"));
    else
        ui->modelStatusLabel->setText(tr("未创建模板"));
    if (m_templateMaskClearButton) {
        m_templateMaskClearButton->setEnabled(
                    sourceAvailable
                    && item.maskRegionType != QStringLiteral("none"));
    }
}

void TemplateLocationDialog::refreshTemplateList()
{
    if (!m_templateBankList)
        return;
    m_updatingTemplateList = true;
    const QSignalBlocker blocker(m_templateBankList);
    m_templateBankList->clear();

    int enabledCount = 0;
    int readyCount = 0;
    int selectedRow = -1;
    QMap<QString, int> baseOrders;
    for (const TemplateLocationBaseBindingConfig &binding :
         qAsConst(m_baseBindings)) {
        baseOrders.insert(binding.baseId, binding.order);
    }
    QVector<int> orderedIndexes;
    orderedIndexes.reserve(m_templates.size());
    for (int index = 0; index < m_templates.size(); ++index)
        orderedIndexes.append(index);
    std::stable_sort(orderedIndexes.begin(), orderedIndexes.end(),
                     [this, &baseOrders](int leftIndex, int rightIndex) {
        const TemplateItemState &left = m_templates.at(leftIndex);
        const TemplateItemState &right = m_templates.at(rightIndex);
        const int leftBaseOrder = baseOrders.value(
                    left.sourceBaseId, kMaximumTemplateCount + 1);
        const int rightBaseOrder = baseOrders.value(
                    right.sourceBaseId, kMaximumTemplateCount + 1);
        if (leftBaseOrder != rightBaseOrder)
            return leftBaseOrder < rightBaseOrder;
        if (left.sourceBaseId != right.sourceBaseId)
            return left.sourceBaseId < right.sourceBaseId;
        if (left.order != right.order)
            return left.order < right.order;
        return left.templateId < right.templateId;
    });
    for (int row = 0; row < orderedIndexes.size(); ++row) {
        const int index = orderedIndexes.at(row);
        TemplateItemState item = m_templates.at(index);
        if (item.templateId == m_activeTemplateId)
            item = activeTemplateForOutput();
        if (item.enabled)
            ++enabledCount;
        if (item.enabled && item.modelCreated)
            ++readyCount;

        QString status;
        if (!item.enabled)
            status = tr("已禁用");
        else if (!item.modelError.trimmed().isEmpty())
            status = tr("错误");
        else if (item.modelCreated)
            status = tr("已创建");
        else if (validRect(item.roi))
            status = tr("需重建");
        else
            status = tr("未定义区域");

        auto *listItem = new QListWidgetItem(
                    tr("T%1 · %2 · %3 · %4")
                    .arg(row)
                    .arg(baseDisplayName(item.sourceBaseId))
                    .arg(item.name)
                    .arg(status), m_templateBankList);
        listItem->setData(kTemplateIdRole, item.templateId);
        listItem->setToolTip(tr("模板 ID：%1\n来源 Base：%2\n参数：%3")
                             .arg(item.templateId,
                                  item.sourceBaseId,
                                  item.useIndependentParameters
                                  ? tr("独立") : tr("共享")));
        listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable);
        listItem->setCheckState(item.enabled ? Qt::Checked : Qt::Unchecked);
        listItem->setFlags(listItem->flags() & ~Qt::ItemIsUserCheckable);
        listItem->setForeground(QBrush(Qt::transparent));
        listItem->setSizeHint(QSize(166, 42));

        auto *tab = new QFrame(m_templateBankList);
        tab->setObjectName(QStringLiteral("templateLocationTemplateTab"));
        tab->setProperty("activeTemplate",
                         item.templateId == m_activeTemplateId);
        auto *tabLayout = new QHBoxLayout(tab);
        tabLayout->setContentsMargins(8, 0, 5, 0);
        tabLayout->setSpacing(4);
        auto *enabledButton = new QToolButton(tab);
        enabledButton->setObjectName(
                    QStringLiteral("templateLocationTemplateEnableButton"));
        enabledButton->setText(QString());
        enabledButton->setCheckable(true);
        enabledButton->setChecked(item.enabled);
        enabledButton->setToolTip(item.enabled
                ? tr("点击停用当前模板") : tr("点击启用当前模板"));
        enabledButton->setFixedSize(14, 14);
        auto *tabButton = new QPushButton(
                    tr("T%1  %2").arg(row).arg(item.name), tab);
        tabButton->setObjectName(
                    QStringLiteral("templateLocationTemplateTabButton"));
        tabButton->setProperty("activeTemplate",
                               item.templateId == m_activeTemplateId);
        tabButton->setToolTip(listItem->text());
        auto *closeButton = new QToolButton(tab);
        closeButton->setObjectName(
                    QStringLiteral("templateLocationTemplateCloseButton"));
        closeButton->setText(QStringLiteral("×"));
        closeButton->setToolTip(tr("删除该模板"));
        closeButton->setFixedSize(24, 30);
        closeButton->setEnabled(!m_configReadOnly && m_templates.size() > 1);
        tabLayout->addWidget(enabledButton, 0, Qt::AlignVCenter);
        tabLayout->addWidget(tabButton, 1);
        tabLayout->addWidget(closeButton);
        m_templateBankList->setItemWidget(listItem, tab);

        connect(tabButton, &QPushButton::clicked, this,
                [this, templateId = item.templateId]() {
            QTimer::singleShot(0, this, [this, templateId]() {
                if (!m_templateBankList)
                    return;
                for (int row = 0; row < m_templateBankList->count(); ++row) {
                    QListWidgetItem *candidate = m_templateBankList->item(row);
                    if (candidate->data(kTemplateIdRole).toString()
                            == templateId) {
                        m_templateBankList->setCurrentItem(candidate);
                        break;
                    }
                }
            });
        });
        connect(enabledButton, &QToolButton::toggled, this,
                [this, templateId = item.templateId](bool enabled) {
            QTimer::singleShot(0, this, [this, templateId, enabled]() {
                if (!m_templateBankList)
                    return;
                for (int row = 0; row < m_templateBankList->count(); ++row) {
                    QListWidgetItem *candidate = m_templateBankList->item(row);
                    if (candidate->data(kTemplateIdRole).toString()
                            == templateId) {
                        candidate->setCheckState(
                                    enabled ? Qt::Checked : Qt::Unchecked);
                        break;
                    }
                }
            });
        });
        connect(closeButton, &QToolButton::clicked, this,
                [this, templateId = item.templateId]() {
            QTimer::singleShot(0, this, [this, templateId]() {
                switchActiveTemplate(templateId);
                deleteActiveTemplateItem();
            });
        });
        if (item.templateId == m_activeTemplateId)
            selectedRow = row;
    }

    if (selectedRow < 0 && !m_templates.isEmpty()) {
        selectedRow = 0;
        m_activeTemplateId = m_templates.first().templateId;
    }
    if (selectedRow >= 0)
        m_templateBankList->setCurrentRow(selectedRow);

    m_templateBankSummaryLabel->setText(
                tr("共 %1 项，启用 %2 项，已就绪 %3 项")
                .arg(m_templates.size()).arg(enabledCount).arg(readyCount));
    m_addTemplateItemButton->setEnabled(
                m_templates.size() < kMaximumTemplateCount);
    m_renameTemplateItemButton->setEnabled(selectedRow >= 0);
    m_deleteTemplateItemButton->setEnabled(m_templates.size() > 1);
    m_updatingTemplateList = false;
}

void TemplateLocationDialog::switchActiveTemplate(const QString &templateId)
{
    if (templateId.trimmed().isEmpty() || templateId == m_activeTemplateId)
        return;
    int targetIndex = -1;
    for (int index = 0; index < m_templates.size(); ++index) {
        if (m_templates.at(index).templateId == templateId) {
            targetIndex = index;
            break;
        }
    }
    if (targetIndex < 0)
        return;
    if (m_running)
        toggleContinuousTest();
    stopEditing();
    flushActiveTemplateEditor();
    m_activeTemplateId = templateId;
    loadActiveTemplateEditor();
    updateMatchResultTable(QJsonArray());
    ui->resultLabel->setText(tr("尚未运行"));
    refreshTemplateList();
    ui->statusLabel->setText(
                tr("已切换到 %1").arg(m_templates.at(targetIndex).name));
}

void TemplateLocationDialog::addTemplateItem()
{
    if (m_templates.size() >= kMaximumTemplateCount) {
        ui->statusLabel->setText(tr("模板数量最多为 %1 个").arg(kMaximumTemplateCount));
        return;
    }
    if (m_running)
        toggleContinuousTest();
    stopEditing();
    flushActiveTemplateEditor();
    ensureV6Editing();

    QSet<QString> names;
    int maximumPriority = -1;
    int maximumBaseOrder = -1;
    const QString sourceBaseId = selectedBaseId();
    for (const TemplateItemState &item : qAsConst(m_templates)) {
        names.insert(item.name);
        maximumPriority = qMax(maximumPriority, item.priority);
        if (item.sourceBaseId == sourceBaseId)
            maximumBaseOrder = qMax(maximumBaseOrder, item.order);
    }
    int suffix = m_templates.size() + 1;
    QString name;
    do {
        name = tr("模板%1").arg(suffix++);
    } while (names.contains(name));

    TemplateItemState item;
    item.templateId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.name = name;
    item.priority = maximumPriority + 1;
    item.order = maximumBaseOrder + 1;
    item.sourceBaseId = sourceBaseId;
    item.independentParameters = m_sharedParameters;
    item.modelCacheKey = QStringLiteral("template_location_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_templates.append(item);
    freezeCentroidOriginsForReferencedBases();
    loadSelectedBaseBinding();
    if (m_templates.size() > 1)
        m_usesTemplateBankSchema = true;
    m_activeTemplateId = item.templateId;
    loadActiveTemplateEditor();
    invalidateRunPreview();
    refreshTemplateList();
    ui->statusLabel->setText(tr("已添加 %1，请绘制模板区域").arg(item.name));
}

void TemplateLocationDialog::renameActiveTemplateItem()
{
    const int index = activeTemplateIndex();
    if (index < 0)
        return;
    bool accepted = false;
    const QString name = QInputDialog::getText(
                this, tr("重命名模板"), tr("模板名称"), QLineEdit::Normal,
                m_templates.at(index).name, &accepted).trimmed();
    if (!accepted)
        return;
    if (name.isEmpty()) {
        ui->statusLabel->setText(tr("模板名称不能为空"));
        return;
    }
    for (int other = 0; other < m_templates.size(); ++other) {
        if (other != index && m_templates.at(other).name == name) {
            ui->statusLabel->setText(tr("模板名称不能重复"));
            return;
        }
    }
    ensureV6Editing();
    m_templates[index].name = name;
    refreshTemplateList();
    ui->statusLabel->setText(tr("模板已重命名为 %1").arg(name));
}

void TemplateLocationDialog::deleteActiveTemplateItem()
{
    if (m_templates.size() <= 1) {
        ui->statusLabel->setText(tr("至少保留一个模板"));
        return;
    }
    const int index = activeTemplateIndex();
    if (index < 0)
        return;
    const TemplateItemState item = activeTemplateForOutput();
    if (QMessageBox::question(
                this, tr("删除模板项"),
                tr("确定删除“%1”及其区域和模型状态吗？").arg(item.name),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    if (m_running)
        toggleContinuousTest();
    stopEditing();
    flushActiveTemplateEditor();
    ensureV6Editing();
    if (!item.modelCacheKey.trimmed().isEmpty())
        m_pendingCacheDeletes.append(item.modelCacheKey);
    m_templates.removeAt(index);
    const int nextIndex = qMin(index, m_templates.size() - 1);
    m_activeTemplateId = m_templates.at(nextIndex).templateId;
    loadActiveTemplateEditor();
    invalidateRunPreview();
    refreshTemplateList();
    ui->statusLabel->setText(tr("模板项已删除"));
}

void TemplateLocationDialog::handleTemplateItemChanged(QListWidgetItem *listItem)
{
    if (m_updatingTemplateList || !listItem)
        return;
    const QString templateId = listItem->data(kTemplateIdRole).toString();
    int index = -1;
    int enabledCount = 0;
    for (int candidate = 0; candidate < m_templates.size(); ++candidate) {
        if (m_templates.at(candidate).enabled)
            ++enabledCount;
        if (m_templates.at(candidate).templateId == templateId)
            index = candidate;
    }
    if (index < 0)
        return;
    const bool enabled = listItem->checkState() == Qt::Checked;
    if (!enabled && m_templates.at(index).enabled && enabledCount <= 1) {
        {
            const QSignalBlocker blocker(m_templateBankList);
            listItem->setCheckState(Qt::Checked);
        }
        refreshTemplateList();
        ui->statusLabel->setText(tr("至少保留一个启用模板"));
        return;
    }
    if (m_templates.at(index).enabled == enabled)
        return;
    ensureV6Editing();
    m_templates[index].enabled = enabled;
    invalidateRunPreview();
    refreshTemplateList();
    ui->statusLabel->setText(enabled
                            ? tr("模板已启用") : tr("模板已禁用"));
}

void TemplateLocationDialog::invalidateRunPreview()
{
    m_snapshot = ToolPreviewSnapshot();
    m_lastDisplayResult = ToolResult();
    updateMatchResultTable(QJsonArray());
    ui->resultLabel->setText(tr("尚未运行"));
    ui->elapsedLabel->setText(tr("算法耗时: 0ms"));
}

void TemplateLocationDialog::showReferenceImage()
{
    const QString templateBaseId = activeTemplateBaseId();
    if (m_contractVersion ==
            TemplateLocationConfig::CompositeBankParamsVersion
            && activeTemplateIndex() >= 0
            && (templateBaseId.isEmpty()
                || !ReferenceImageProvider::instance().hasReferenceFrame(
                    templateBaseId))) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(
                    tr("模板来源 Base 不存在 · %1")
                    .arg(templateBaseId.isEmpty() ? tr("未绑定")
                                                  : templateBaseId));
        return;
    }

    const bool templateGeometry = isTemplateGeometryTarget(m_editTarget);
    const QString baseId = templateGeometry ? templateBaseId
                                            : selectedBaseId();
    QImage image = ReferenceImageProvider::instance().referenceImage(baseId);
    if (image.isNull()
            && m_contractVersion !=
               TemplateLocationConfig::CompositeBankParamsVersion) {
        image = ReferenceImageProvider::instance().referenceImage();
    }
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(
                    m_contractVersion ==
                    TemplateLocationConfig::CompositeBankParamsVersion
                    ? tr("绑定的 Base 不存在 · %1").arg(baseId)
                    : tr("请先设置基准图"));
        return;
    }
    m_previewHelper->setImage(image);
    QVector<ToolOverlay> overlays;
    const bool displayingTemplateSource = baseId == templateBaseId;
    if (displayingTemplateSource && m_modelCreated
            && !m_templateDisplayOverlays.isEmpty())
        overlays = m_templateDisplayOverlays;
    if (displayingTemplateSource
            && m_editTarget != EditTarget::TemplateMaskRect &&
            m_editTarget != EditTarget::TemplateMaskCircle &&
            m_editTarget != EditTarget::TemplateMaskPolygon) {
        appendTemplateMaskOverlay(&overlays, m_templateMaskRegionType,
                                  m_templateMaskRoi, m_templateMaskPolygon,
                                  m_templateMaskCircle,
                                  image.width(), image.height());
    }
    QString displayOriginMode = m_originMode;
    QPointF displayOrigin = m_customOriginNormalized;
    if (m_contractVersion ==
            TemplateLocationConfig::CompositeBankParamsVersion) {
        for (const TemplateLocationBaseBindingConfig &binding :
             qAsConst(m_baseBindings)) {
            if (binding.baseId != baseId)
                continue;
            displayOriginMode = binding.originMode;
            displayOrigin = binding.customOriginNormalized;
            break;
        }
    }
    if (displayOriginMode == QStringLiteral("custom")) {
        const QPointF point(displayOrigin.x() * image.width(),
                            displayOrigin.y() * image.height());
        const qreal radius = 9.0;
        ToolOverlay horizontal;
        horizontal.type = ToolOverlayType::Line;
        horizontal.label = QStringLiteral("template_origin");
        horizontal.p1 = QPointF(point.x() - radius, point.y());
        horizontal.p2 = QPointF(point.x() + radius, point.y());
        horizontal.extra.insert(QStringLiteral("displayRole"),
                                QStringLiteral("template_location_origin"));
        overlays.append(horizontal);
        ToolOverlay vertical = horizontal;
        vertical.p1 = QPointF(point.x(), point.y() - radius);
        vertical.p2 = QPointF(point.x(), point.y() + radius);
        overlays.append(vertical);
    }
    if (overlays.isEmpty())
        m_previewHelper->clearToolOverlays();
    else
        m_previewHelper->setToolOverlays(overlays);
    ui->viewerTitleLabel->setText(tr("基准图 · %1")
                                  .arg(baseDisplayName(baseId)));
}

void TemplateLocationDialog::markModelDirty()
{
    if (!m_modelCreated && !validRect(m_templateRoi))
        return;
    m_modelCreated = false;
    m_templateDisplayOverlays.clear();
    m_snapshot = ToolPreviewSnapshot();
    ui->createTemplateButton->setText(tr("创建模板"));
    ui->deleteTemplateButton->setEnabled(false);
    m_previewHelper->clearToolOverlays();
    ui->modelStatusLabel->setText(tr("参数已改变，需要重新创建模板"));
    const int index = activeTemplateIndex();
    if (index >= 0)
        m_templates[index].modelError.clear();
    flushActiveTemplateEditor();
    refreshTemplateList();
    showReferenceImage();
}

void TemplateLocationDialog::markAllModelsDirty()
{
    flushActiveTemplateEditor();
    bool hadCreatedModel = false;
    for (TemplateItemState &item : m_templates) {
        hadCreatedModel = hadCreatedModel || item.modelCreated;
        item.modelCreated = false;
        item.modelError.clear();
        item.displayOverlays.clear();
    }
    m_modelCreated = false;
    m_templateDisplayOverlays.clear();
    invalidateRunPreview();
    ui->createTemplateButton->setText(tr("创建模板"));
    ui->deleteTemplateButton->setEnabled(false);
    ui->modelStatusLabel->setText(hadCreatedModel
                                  ? tr("共享参数已改变，需要重新创建全部模板")
                                  : tr("需要创建模板"));
    flushActiveTemplateEditor();
    refreshTemplateList();
    showReferenceImage();
}

void TemplateLocationDialog::deleteTemplate()
{
    if (m_running)
        toggleContinuousTest();
    stopEditing();
    ensureV6Editing();
    m_modelCreated = false;
    m_creatingTemplate = false;
    m_templateDisplayOverlays.clear();
    m_snapshot = ToolPreviewSnapshot();
    if (!m_modelCacheKey.trimmed().isEmpty())
        m_pendingCacheDeletes.append(m_modelCacheKey);
    m_modelCacheKey = QStringLiteral("template_location_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_previewHelper->clearToolOverlays();
    ui->createTemplateButton->setText(tr("创建模板"));
    ui->deleteTemplateButton->setEnabled(false);
    ui->modelStatusLabel->setText(tr("未创建模板"));
    ui->autoContrastValueLabel->setText(tr("建模后显示"));
    ui->resultLabel->setText(tr("尚未运行"));
    ui->elapsedLabel->setText(tr("算法耗时: 0ms"));
    updateMatchResultTable(QJsonArray());
    ui->statusLabel->setText(tr("模板已删除，模板区域已保留"));
    const int index = activeTemplateIndex();
    if (index >= 0)
        m_templates[index].modelError.clear();
    flushActiveTemplateEditor();
    refreshTemplateList();
    showReferenceImage();
}

void TemplateLocationDialog::commitPendingCacheDeletes()
{
    QSet<QString> uniqueKeys;
    for (const QString &key : qAsConst(m_pendingCacheDeletes)) {
        if (!key.trimmed().isEmpty())
            uniqueKeys.insert(key);
    }
    for (const QString &key : qAsConst(uniqueKeys))
        TemplateLocationHalconRunner::clearPersistentCache(key);
    m_pendingCacheDeletes.clear();
}

bool TemplateLocationDialog::validateTemplate(QString *message) const
{
    if (m_contractVersion ==
            TemplateLocationConfig::CompositeBankParamsVersion &&
            activeTemplateBaseId().isEmpty()) {
        if (message)
            *message = tr("当前模板未绑定 Base，请删除后在目标 Base 上重新添加");
        return false;
    }
    if (!ReferenceImageProvider::instance().hasReferenceFrame(
                activeTemplateBaseId())) {
        if (message) *message = tr("请先设置基准图");
        return false;
    }
    const bool valid = m_templateRegionType == QStringLiteral("polygon")
            ? m_templatePolygon.size() >= 3 && validRect(m_templateRoi)
            : validRect(m_templateRoi);
    if (!valid && message)
        *message = tr("请先绘制有效模板区域");
    if (valid && m_templateMaskRegionType == QStringLiteral("rectangle") &&
            !validRect(m_templateMaskRoi)) {
        if (message)
            *message = tr("请先绘制有效的矩形模板屏蔽区域");
        return false;
    }
    if (valid && m_templateMaskRegionType == QStringLiteral("circle") &&
            !m_templateMaskCircle.valid) {
        if (message)
            *message = tr("请先绘制有效的圆形模板屏蔽区域");
        return false;
    }
    if (valid && m_templateMaskRegionType == QStringLiteral("polygon") &&
            m_templateMaskPolygon.size() < 3) {
        if (message)
            *message = tr("模板屏蔽多边形至少需要 3 个点");
        return false;
    }
    return valid;
}

bool TemplateLocationDialog::validateTemplateItem(
        const TemplateItemState &item, QString *message) const
{
    const bool valid = item.regionType == QStringLiteral("polygon")
            ? item.polygon.size() >= 3 && validRect(item.roi)
            : validRect(item.roi);
    if (!valid) {
        if (message)
            *message = tr("模板“%1”的模板区域无效").arg(item.name);
        return false;
    }
    if (item.maskRegionType == QStringLiteral("rectangle") &&
            !validRect(item.maskRoi)) {
        if (message)
            *message = tr("模板“%1”的矩形屏蔽区域无效").arg(item.name);
        return false;
    }
    if (item.maskRegionType == QStringLiteral("circle") &&
            !item.maskCircle.valid) {
        if (message)
            *message = tr("模板“%1”的圆形屏蔽区域无效").arg(item.name);
        return false;
    }
    if (item.maskRegionType == QStringLiteral("polygon") &&
            item.maskPolygon.size() < 3) {
        if (message)
            *message = tr("模板“%1”的屏蔽多边形至少需要 3 个点").arg(item.name);
        return false;
    }
    return true;
}

bool TemplateLocationDialog::validateTemplateBank(
        bool requireModels, QString *message) const
{
    if (m_configReadOnly) {
        if (message) {
            *message = tr("当前模板定位配置只读（%1）：%2")
                    .arg(m_configReadOnlyStatus.trimmed().isEmpty()
                         ? QStringLiteral("invalid_config")
                         : m_configReadOnlyStatus,
                         m_configReadOnlyMessage.trimmed().isEmpty()
                         ? tr("请使用支持该配置合同的版本编辑")
                         : m_configReadOnlyMessage);
        }
        return false;
    }
    if (!ReferenceImageProvider::instance().hasReferenceFrame()) {
        if (message)
            *message = tr("请先设置基准图");
        return false;
    }
    if (m_templates.isEmpty() || m_templates.size() > kMaximumTemplateCount) {
        if (message)
            *message = tr("模板数量必须为 1~%1 个").arg(kMaximumTemplateCount);
        return false;
    }

    int enabledCount = 0;
    QSet<QString> ids;
    for (int index = 0; index < m_templates.size(); ++index) {
        TemplateItemState item = m_templates.at(index);
        if (item.templateId == m_activeTemplateId)
            item = activeTemplateForOutput();
        if (item.templateId.trimmed().isEmpty() || ids.contains(item.templateId)) {
            if (message)
                *message = tr("模板 ID 为空或重复");
            return false;
        }
        ids.insert(item.templateId);
        if (m_contractVersion ==
                TemplateLocationConfig::CompositeBankParamsVersion
                && !ReferenceImageProvider::instance().hasReferenceFrame(
                    item.sourceBaseId)) {
            if (message) {
                *message = tr("模板“%1”绑定的 Base 不存在：%2")
                        .arg(item.name, item.sourceBaseId);
            }
            return false;
        }
        if (!item.enabled)
            continue;
        ++enabledCount;
        if (!validateTemplateItem(item, message))
            return false;
        if (requireModels && !item.modelCreated) {
            if (message)
                *message = tr("模板“%1”尚未创建或需要重建").arg(item.name);
            return false;
        }
    }
    if (enabledCount == 0) {
        if (message)
            *message = tr("至少启用一个模板");
        return false;
    }

    // Keep Dialog validation and the Adapter/Runner contract identical.  This
    // catches fields not directly editable on the current page (for example a
    // locked primary template or fusion values retained from a newer config).
    const ToolConfig candidate = toolConfig();
    const TemplateLocationModelBankConfig bank =
            TemplateLocationConfig::fromToolConfig(candidate);
    const TemplateLocationConfigValidationResult validation =
            TemplateLocationConfig::validateModelBank(bank, requireModels);
    if (!validation.valid) {
        if (message) {
            *message = tr("模板库配置无效（%1）：%2")
                    .arg(validation.code, validation.message);
        }
        return false;
    }
    return true;
}

bool TemplateLocationDialog::validateParameters(QString *message) const
{
    if (ui->angleMinSpinBox->value() > ui->angleMaxSpinBox->value()) {
        if (message) *message = tr("最小角度不能大于最大角度");
        return false;
    }
    if (ui->scaleMinSpinBox->value() > ui->scaleMaxSpinBox->value()) {
        if (message) *message = tr("最小缩放不能大于最大缩放");
        return false;
    }
    if (contrastModeValue() == QStringLiteral("manual") &&
            ui->minContrastSpinBox->value() >= ui->contrastSpinBox->value()) {
        if (message) *message = tr("MinContrast 必须小于 Contrast");
        return false;
    }
    if (ui->minMatchCountSpinBox->value() > ui->maxMatchCountSpinBox->value() ||
            ui->maxMatchCountSpinBox->value() > ui->maxMatchesSpinBox->value()) {
        if (message) *message = tr("数量判定必须满足：最少 ≤ 最多 ≤ 最大查找数");
        return false;
    }
    if (m_originMode == QStringLiteral("custom") &&
            (!std::isfinite(m_customOriginNormalized.x()) ||
             !std::isfinite(m_customOriginNormalized.y()) ||
             m_customOriginNormalized.x() < 0.0 || m_customOriginNormalized.x() > 1.0 ||
             m_customOriginNormalized.y() < 0.0 || m_customOriginNormalized.y() > 1.0)) {
        if (message) *message = tr("请先选择有效的自定义定位点");
        return false;
    }
    if (m_searchRegionType == QStringLiteral("polygon") && m_searchPolygon.size() < 3) {
        if (message) *message = tr("搜索多边形至少需要 3 个点");
        return false;
    }
    if (m_searchRegionType == QStringLiteral("circle") && !m_searchCircle.valid) {
        if (message) *message = tr("请先绘制有效圆形搜索区域");
        return false;
    }
    if (!validRect(m_searchRoi)) {
        if (message) *message = tr("搜索区域无效");
        return false;
    }
    return true;
}

void TemplateLocationDialog::createTemplate()
{
    QString message;
    if (!validateTemplate(&message) || !validateParameters(&message)) {
        ui->statusLabel->setText(message);
        return;
    }
    stopEditing();
    ensureV6Editing();
    flushActiveTemplateEditor();
    m_modelCreated = true;
    m_creatingTemplate = true;
    const QString baseId = activeTemplateBaseId();
    const ReferenceFrameSetSnapshot referenceSnapshot =
            ReferenceImageProvider::instance().referenceFrameSetSnapshot();
    runOnFrame(referenceSnapshot.frames.value(baseId),
               tr("基准图建模 · %1").arg(baseDisplayName(baseId)),
               baseId,
               &referenceSnapshot);
    m_creatingTemplate = false;
    if (!m_snapshot.valid || !m_snapshot.result.success || !m_snapshot.result.ok ||
            m_templateDisplayOverlays.isEmpty()) {
        m_modelCreated = false;
        m_templateDisplayOverlays.clear();
        ui->deleteTemplateButton->setEnabled(false);
        ui->modelStatusLabel->setText(tr("模板创建失败"));
        const int index = activeTemplateIndex();
        if (index >= 0)
            m_templates[index].modelError = m_snapshot.result.message;
        flushActiveTemplateEditor();
        refreshTemplateList();
        return;
    }
    ui->createTemplateButton->setText(tr("重新创建模板"));
    ui->deleteTemplateButton->setEnabled(true);
    const QJsonObject payload = m_snapshot.result.payload;
    ui->modelStatusLabel->setText(payload.value(QStringLiteral("modelCacheHit")).toBool()
                                 ? tr("模板已创建（已加载持久缓存）")
                                 : tr("模板已创建（缓存已保存）"));
    const QString contrast = payload.value(QStringLiteral("contrastUsed")).isString()
            ? payload.value(QStringLiteral("contrastUsed")).toString()
            : QString::number(payload.value(QStringLiteral("contrastUsed")).toInt());
    const QString minContrast = payload.contains(QStringLiteral("minContrastUsed"))
            ? QString::number(payload.value(QStringLiteral("minContrastUsed")).toDouble(), 'f', 0)
            : tr("自动");
    ui->autoContrastValueLabel->setText(tr("Contrast %1 / MinContrast %2")
                                        .arg(contrast, minContrast));
    const int index = activeTemplateIndex();
    if (index >= 0)
        m_templates[index].modelError.clear();
    flushActiveTemplateEditor();
    refreshTemplateList();
}

void TemplateLocationDialog::runReferenceTest()
{
    stopEditing();
    flushActiveTemplateEditor();
    QString message;
    if (!validateTemplateBank(true, &message) || !validateParameters(&message)) {
        ui->statusLabel->setText(message);
        return;
    }
    const QString baseId = selectedBaseId();
    const ReferenceFrameSetSnapshot referenceSnapshot =
            ReferenceImageProvider::instance().referenceFrameSetSnapshot();
    runOnFrame(referenceSnapshot.frames.value(baseId),
               tr("基准图测试 · %1").arg(baseDisplayName(baseId)),
               baseId,
               &referenceSnapshot);
}

void TemplateLocationDialog::toggleContinuousTest()
{
    if (m_running) {
        if (m_frameConnection)
            disconnect(m_frameConnection);
        m_frameConnection = QMetaObject::Connection();
        m_running = false;
        ui->testRunButton->setText(tr("测试运行"));
        showReferenceImage();
        ui->statusLabel->setText(tr("已退出测试"));
        return;
    }

    stopEditing();
    flushActiveTemplateEditor();
    QString message;
    if (!validateTemplateBank(true, &message) || !validateParameters(&message)) {
        ui->statusLabel->setText(message);
        return;
    }
    if (!CameraFrameProvider::instance().hasFrame()) {
        ui->statusLabel->setText(tr("当前没有相机图像"));
        return;
    }
    m_running = true;
    ui->testRunButton->setText(tr("退出测试"));
    m_frameConnection = connect(&CameraFrameProvider::instance(),
                                &CameraFrameProvider::frameUpdatedMat,
                                this,
                                [this]() {
        if (m_running)
            runOnFrame(CameraFrameProvider::instance().currentFrame(), tr("实时图像"));
    });
    runOnFrame(CameraFrameProvider::instance().currentFrame(), tr("实时图像"));
}

void TemplateLocationDialog::runOnFrame(const cv::Mat &frame,
                                        const QString &title,
                                        const QString &displayBaseId,
                                        const ReferenceFrameSetSnapshot *referenceSnapshot)
{
    if (m_processing || frame.empty()) {
        if (frame.empty())
            ui->statusLabel->setText(tr("输入图像为空"));
        return;
    }
    m_processing = true;
    ToolRequest request;
    request.config = m_creatingTemplate ? activeTemplateToolConfig() : toolConfig();
    if (m_creatingTemplate) {
        if (request.config.params.value(QStringLiteral("version")).toInt()
                < TemplateLocationConfig::CompositeBankParamsVersion) {
            request.config.params.insert(QStringLiteral("modelCreated"), true);
        }
        request.config.params.insert(QStringLiteral("minMatchCount"), 1);
        request.config.params.insert(QStringLiteral("maxMatchCount"),
                                     ui->maxMatchesSpinBox->value());
    }
    request.image = frame;
    const ReferenceFrameSetSnapshot capturedReferenceSnapshot = referenceSnapshot
            ? *referenceSnapshot
            : ReferenceImageProvider::instance().referenceFrameSetSnapshot();
    request.referenceImage = capturedReferenceSnapshot.primary.frame;
    request.referenceImages = capturedReferenceSnapshot.frames;
    request.referenceImageRevisions = capturedReferenceSnapshot.contentRevisions;
    request.primaryReferenceBaseId = capturedReferenceSnapshot.primaryBaseId;
    m_runDisplayBaseId = displayBaseId.trimmed();
    m_runDisplayImageSize = QSize(frame.cols, frame.rows);
    const ToolResult result = m_testEngine.runTool(request);
    displayResult(result);
    ui->viewerTitleLabel->setText(title);
    m_processing = false;
}

void TemplateLocationDialog::displayResult(const ToolResult &result)
{
    m_lastDisplayResult = result;
    updateMatchResultTable(result.payload.value(QStringLiteral("matches")).toArray());
    if (!result.success) {
        ui->resultLabel->setText(tr("错误：%1").arg(result.message));
        ui->statusLabel->setText(result.status);
    } else if (!result.ok) {
        const int foundCount = result.payload.value(QStringLiteral("foundCount")).toInt();
        if (foundCount > 0) {
            ui->resultLabel->setText(tr("NG  找到 %1 个，要求 %2~%3 个")
                                     .arg(foundCount)
                                     .arg(result.payload.value(QStringLiteral("minMatchCount")).toInt())
                                     .arg(result.payload.value(QStringLiteral("maxMatchCount")).toInt()));
            ui->statusLabel->setText(tr("匹配数量不符合判定范围"));
        } else {
            ui->resultLabel->setText(tr("NG  未找到匹配目标"));
            ui->statusLabel->setText(tr("未找到达到最低得分的目标"));
        }
    } else {
        const QJsonObject payload = result.payload;
        ui->resultLabel->setText(tr("OK  找到 %1 个  最佳得分 %2%  X %3  Y %4  角度 %5°  缩放 %6")
                                 .arg(result.count)
                                 .arg(result.score * 100.0, 0, 'f', 1)
                                 .arg(payload.value(QStringLiteral("x")).toDouble(), 0, 'f', 2)
                                 .arg(payload.value(QStringLiteral("y")).toDouble(), 0, 'f', 2)
                                 .arg(payload.value(QStringLiteral("angleDeg")).toDouble(), 0, 'f', 2)
                                 .arg(payload.value(QStringLiteral("scale")).toDouble(), 0, 'f', 3));
        ui->statusLabel->setText(tr("模板定位成功"));
    }
    ui->elapsedLabel->setText(tr("算法耗时: %1ms").arg(result.elapsedMs));
    ToolResult previewResult = result;
    if (m_creatingTemplate) {
        QVector<ToolOverlay> templateOverlays;
        for (ToolOverlay overlay : result.overlays) {
            if (overlay.label != QStringLiteral("match_result"))
                continue;
            const QString overlayTemplateId = overlay.extra.value(
                        QStringLiteral("templateId")).toString();
            if ((!overlayTemplateId.isEmpty() &&
                 overlayTemplateId != m_activeTemplateId) ||
                    (overlayTemplateId.isEmpty() &&
                     overlay.extra.value(QStringLiteral("matchIndex")).toInt(-1) != 0))
                continue;
            overlay.label = QStringLiteral("template_model");
            overlay.extra.insert(QStringLiteral("displayRole"),
                                 QStringLiteral("template_location_model"));
            overlay.extra.insert(QStringLiteral("templateId"), m_activeTemplateId);
            if (!overlay.extra.contains(QStringLiteral("matchId"))) {
                overlay.extra.insert(QStringLiteral("matchId"),
                                     QStringLiteral("build:%1").arg(m_activeTemplateId));
            }
            templateOverlays.append(overlay);
        }
        m_templateDisplayOverlays = templateOverlays;
        appendTemplateMaskOverlay(&templateOverlays, m_templateMaskRegionType,
                                  m_templateMaskRoi, m_templateMaskPolygon,
                                  m_templateMaskCircle,
                                  m_runDisplayImageSize.width(),
                                  m_runDisplayImageSize.height());
        previewResult.overlays = templateOverlays;
    }
    m_previewHelper->setToolOverlays(previewResult.overlays);
    m_snapshot = makeReferenceToolPreviewSnapshot(toolConfig(), previewResult, m_searchRoi);
}

QString TemplateLocationDialog::polarityValue() const
{
    switch (ui->polarityComboBox->currentIndex()) {
    case 1: return QStringLiteral("ignore_local_polarity");
    case 2: return QStringLiteral("ignore_global_polarity");
    default: return QStringLiteral("use_polarity");
    }
}

void TemplateLocationDialog::setPolarityValue(const QString &value)
{
    if (value == QStringLiteral("ignore_local_polarity"))
        ui->polarityComboBox->setCurrentIndex(1);
    else if (value == QStringLiteral("ignore_global_polarity"))
        ui->polarityComboBox->setCurrentIndex(2);
    else
        ui->polarityComboBox->setCurrentIndex(0);
}

QString TemplateLocationDialog::contrastModeValue() const
{
    return ui->contrastModeComboBox->currentIndex() == 1
            ? QStringLiteral("manual") : QStringLiteral("auto");
}

void TemplateLocationDialog::updateContrastControls()
{
    const bool manual = contrastModeValue() == QStringLiteral("manual");
    ui->contrastLabel->setEnabled(manual);
    ui->minContrastLabel->setEnabled(manual);
    ui->contrastSpinBox->setEnabled(manual);
    ui->minContrastSpinBox->setEnabled(manual);
    ui->autoContrastValueLabel->setVisible(!manual);
    ui->autoContrastResultLabel->setVisible(!manual);
}

void TemplateLocationDialog::updateOriginControls()
{
    const bool custom = m_originMode == QStringLiteral("custom");
    ui->selectOriginButton->setVisible(custom);
    ui->originValueLabel->setText(custom
            ? tr("X %1  Y %2")
              .arg(m_customOriginNormalized.x(), 0, 'f', 4)
              .arg(m_customOriginNormalized.y(), 0, 'f', 4)
            : tr("使用模板质心"));
}

void TemplateLocationDialog::updateMatchResultTable(const QJsonArray &matches)
{
    if (!m_matchResultTable || !m_matchResultDrawer || !m_matchResultToggle)
        return;
    m_matchResultTable->setRowCount(matches.size());
    for (int row = 0; row < matches.size(); ++row) {
        const QJsonObject match = matches.at(row).toObject();
        QString templateName = match.value(QStringLiteral("templateName"))
                .toString().trimmed();
        if (templateName.isEmpty())
            templateName = match.value(QStringLiteral("templateId")).toString();
        const QStringList values{
            QString::number(row + 1),
            templateName,
            QString::number(match.value(QStringLiteral("x")).toDouble(), 'f', 2),
            QString::number(match.value(QStringLiteral("y")).toDouble(), 'f', 2),
            QString::number(match.value(QStringLiteral("angleDeg")).toDouble(), 'f', 2) + tr("°"),
            QString::number(match.value(QStringLiteral("scale")).toDouble(), 'f', 3),
            QString::number(match.value(QStringLiteral("score")).toDouble() * 100.0, 'f', 1) + tr("%")
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            item->setTextAlignment(Qt::AlignCenter);
            item->setData(kMatchIdRole,
                          match.value(QStringLiteral("matchId")).toString());
            m_matchResultTable->setItem(row, column, item);
        }
    }
    if (matches.isEmpty()) {
        setMatchResultsExpanded(false);
        m_matchResultDrawer->hide();
        return;
    }

    m_matchResultDrawer->show();
    setMatchResultsExpanded(m_matchResultsExpanded);
}

void TemplateLocationDialog::setMatchResultsExpanded(bool expanded)
{
    if (!m_matchResultDrawer || !m_matchResultToggle || !m_matchResultTable)
        return;

    m_matchResultsExpanded = expanded;
    m_matchResultToggle->setChecked(expanded);
    m_matchResultToggle->setText(
                tr("%1 匹配结果（%2）")
                .arg(expanded ? QStringLiteral("▼") : QStringLiteral("▶"))
                .arg(m_matchResultTable->rowCount()));
    m_matchResultTable->setVisible(expanded);
    // 36px summary + 116px table: show about three rows and scroll the rest.
    m_matchResultDrawer->setFixedHeight(expanded ? 152 : 36);
}

ToolConfig TemplateLocationDialog::toolConfig() const
{
    if (m_configReadOnly)
        return m_config;

    ToolConfig config = m_config;
    config.toolType = ToolType::TemplateLocation;
    config.category = ToolCategory::Location;
    config.toolName = tr("模板定位");
    config.displayName = tr("模板定位");
    config.roiNormalized = m_searchRoi;
    const TemplateLocationModelBankConfig bank = modelBankForOutput();
    config.params = TemplateLocationConfig::toToolParams(bank);
    const int enabledCount = TemplateLocationConfig::enabledTemplateCount(bank);
    config.summary = tr("模板定位，启用 %1/%2 个模板，查找 %3 个，数量 %4~%5，最低得分 %6%，角度 %7°~%8°")
            .arg(enabledCount)
            .arg(bank.templates.size())
            .arg(ui->maxMatchesSpinBox->value())
            .arg(ui->minMatchCountSpinBox->value())
            .arg(ui->maxMatchCountSpinBox->value())
            .arg(ui->minScoreSpinBox->value())
            .arg(ui->angleMinSpinBox->value())
            .arg(ui->angleMaxSpinBox->value());
    return config;
}

ToolConfig TemplateLocationDialog::activeTemplateToolConfig() const
{
    if (m_configReadOnly)
        return m_config;

    ToolConfig config = m_config;
    config.toolType = ToolType::TemplateLocation;
    config.category = ToolCategory::Location;
    TemplateLocationModelBankConfig bank = modelBankForOutput();
    const QString activeId = m_activeTemplateId;
    QVector<TemplateLocationTemplateConfig> activeTemplates;
    for (TemplateLocationTemplateConfig item : qAsConst(bank.templates)) {
        if (item.templateId != activeId)
            continue;
        item.enabled = true;
        item.modelCreated = true;
        item.order = 0;
        item.priority = 0;
        activeTemplates.append(item);
        break;
    }
    bank.templates = activeTemplates;
    bank.primaryMatchStrategy = bank.version ==
            TemplateLocationConfig::CompositeBankParamsVersion
            ? QStringLiteral("first_valid") : QStringLiteral("best_score");
    bank.primaryTemplateId = bank.version ==
            TemplateLocationConfig::CompositeBankParamsVersion
            ? QString() : activeId;
    config.params = TemplateLocationConfig::toToolParams(bank);
    return config;
}

void TemplateLocationDialog::loadFromConfig(const ToolConfig &config)
{
    if (m_running)
        toggleContinuousTest();
    stopEditing();
    m_loadingConfig = true;
    m_config = config;
    m_config.toolType = ToolType::TemplateLocation;
    m_config.category = ToolCategory::Location;
    const QJsonObject params = config.params;
    const TemplateLocationModelBankConfig decoded =
            TemplateLocationConfig::fromToolParams(params, config.toolId);
    m_configReadOnly = !decoded.decodeSupported || decoded.rawPassthrough;
    m_configReadOnlyStatus = decoded.decodeStatus;
    m_configReadOnlyMessage = decoded.decodeMessage;
    m_contractVersion = decoded.version;
    m_sharedParameters = TemplateLocationConfig::sharedMatchParameters(decoded);
    m_baseBindings = decoded.baseBindings;
    m_templateIncludeEdited = false;
    m_templateExcludeEdited = false;
    m_pendingCacheDeletes.clear();
    m_templates.clear();
    m_usesTemplateBankSchema = decoded.version >=
            TemplateLocationConfig::ModelBankParamsVersion;
    QSet<QString> ids;
    const QString defaultBaseId =
            ReferenceImageProvider::instance().primaryBaseId().trimmed().isEmpty()
            ? QStringLiteral("base-0")
            : ReferenceImageProvider::instance().primaryBaseId();
    for (int index = 0; index < decoded.templates.size(); ++index) {
        TemplateItemState item = templateStateFromConfig(
                    decoded.templates.at(index), index);
        if (ids.contains(item.templateId)) {
            item.templateId = QUuid::createUuid()
                    .toString(QUuid::WithoutBraces);
            item.modelCreated = false;
            item.modelError = tr("原配置包含重复模板 ID");
        }
        if (decoded.version !=
                TemplateLocationConfig::CompositeBankParamsVersion &&
                item.sourceBaseId.trimmed().isEmpty()) {
            item.sourceBaseId = defaultBaseId;
        }
        ids.insert(item.templateId);
        m_templates.append(item);
    }
    if (m_templates.isEmpty()) {
        m_activeTemplateId.clear();
        m_templateRegionType = QStringLiteral("rectangle");
        m_templateRoi = QRectF();
        m_templatePolygon.clear();
        m_templateMaskRegionType = QStringLiteral("none");
        m_templateMaskRoi = QRectF();
        m_templateMaskPolygon.clear();
        m_templateMaskCircle = CircleRoi();
        m_modelCacheKey.clear();
        m_modelCreated = false;
        m_templateDisplayOverlays.clear();
    } else {
        m_activeTemplateId = m_templates.first().templateId;
    }

    m_searchRegionType = decoded.searchRegionType;
    m_searchRoi = decoded.searchRoiNormalized;
    m_searchPolygon = decoded.searchPolygonNormalized;
    m_searchCircle.centerNormalized = decoded.searchCircleCenterNormalized;
    m_searchCircle.radiusNormalized = decoded.searchCircleRadiusNormalized;
    m_searchCircle.boundingRectNormalized = QRectF(
                m_searchCircle.centerNormalized.x() - m_searchCircle.radiusNormalized,
                m_searchCircle.centerNormalized.y() - m_searchCircle.radiusNormalized,
                m_searchCircle.radiusNormalized * 2.0,
                m_searchCircle.radiusNormalized * 2.0);
    m_searchCircle.valid = m_searchRegionType == QStringLiteral("circle") &&
            m_searchCircle.radiusNormalized > 0.0 && validRect(m_searchCircle.boundingRectNormalized);
    m_originMode = decoded.originMode;
    m_customOriginNormalized = decoded.customOriginNormalized;

    if (m_contractVersion == TemplateLocationConfig::CompositeBankParamsVersion) {
        seedBaseBindingsFromProvider();
        m_selectedBaseId = !m_templates.isEmpty()
                ? m_templates.first().sourceBaseId : defaultBaseId;
        if (m_baseSelector) {
            m_baseSelector->setSelectedBaseId(m_selectedBaseId);
            const QString validationBaseId =
                    m_baseSelector->selectedBaseId().trimmed();
            if (!validationBaseId.isEmpty())
                m_selectedBaseId = validationBaseId;
        }
        loadSelectedBaseBinding();
    } else {
        m_selectedBaseId = defaultBaseId;
        if (m_baseSelector)
            m_baseSelector->setSelectedBaseId(m_selectedBaseId);
    }

    applyMatchParametersToUi(m_sharedParameters);
    const QSignalBlocker timeoutBlock(ui->timeoutSpinBox);
    const QSignalBlocker maxMatchesBlock(ui->maxMatchesSpinBox);
    const QSignalBlocker minCountBlock(ui->minMatchCountSpinBox);
    const QSignalBlocker maxCountBlock(ui->maxMatchCountSpinBox);
    const QSignalBlocker originModeBlock(ui->originModeComboBox);
    ui->timeoutSpinBox->setValue(decoded.timeoutMs);
    ui->maxMatchesSpinBox->setValue(decoded.maxMatches);
    ui->minMatchCountSpinBox->setMaximum(decoded.maxMatches);
    ui->maxMatchCountSpinBox->setMaximum(decoded.maxMatches);
    ui->minMatchCountSpinBox->setValue(decoded.minMatchCount);
    ui->maxMatchCountSpinBox->setValue(decoded.maxMatchCount);
    ui->originModeComboBox->setCurrentIndex(
                m_originMode == QStringLiteral("custom") ? 1 : 0);
    updateOriginControls();
    updateMatchResultTable(QJsonArray());
    m_snapshot = ToolPreviewSnapshot();
    m_lastDisplayResult = ToolResult();
    loadActiveTemplateEditor();
    refreshTemplateList();
    refreshActiveTemplateUi();
    if (m_templates.isEmpty()) {
        showReferenceImage();
        ui->statusLabel->setText(
                    tr("配置中的模板库为空，请添加并创建至少一个模板"));
    }
    ui->parameterPanel->setEnabled(!m_configReadOnly);
    if (m_configReadOnly) {
        ui->statusLabel->setText(
                    tr("当前模板定位配置只读（%1）：%2")
                    .arg(m_configReadOnlyStatus.trimmed().isEmpty()
                         ? QStringLiteral("invalid_config")
                         : m_configReadOnlyStatus,
                         m_configReadOnlyMessage.trimmed().isEmpty()
                         ? tr("请使用支持该配置合同的版本编辑")
                         : m_configReadOnlyMessage));
    }
    m_loadingConfig = false;
}

ToolPreviewSnapshot TemplateLocationDialog::referencePreviewSnapshot() const
{
    return m_snapshot;
}
