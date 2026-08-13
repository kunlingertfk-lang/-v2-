#include "TemplateLocationDialog.h"

#include "PlanDialogUtils.h"
#include "UiStyleRoles.h"
#include "algorithms/location/TemplateLocationConfig.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "ui_TemplateLocationDialog.h"

#include <QButtonGroup>
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
#include <QSet>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include <cmath>

namespace {

constexpr int kTemplateIdRole = Qt::UserRole + 1;
constexpr int kMatchIdRole = Qt::UserRole + 2;
constexpr int kMaximumTemplateCount = 8;

QJsonObject rectToJson(const QRectF &rect)
{
    return QJsonObject{{QStringLiteral("x"), rect.x()},
                       {QStringLiteral("y"), rect.y()},
                       {QStringLiteral("width"), rect.width()},
                       {QStringLiteral("height"), rect.height()}};
}

QRectF rectFromJson(const QJsonObject &json, const QRectF &fallback)
{
    if (json.isEmpty())
        return fallback;
    return QRectF(json.value(QStringLiteral("x")).toDouble(fallback.x()),
                  json.value(QStringLiteral("y")).toDouble(fallback.y()),
                  json.value(QStringLiteral("width")).toDouble(fallback.width()),
                  json.value(QStringLiteral("height")).toDouble(fallback.height()));
}

QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points)
        array.append(QJsonObject{{QStringLiteral("x"), point.x()},
                                 {QStringLiteral("y"), point.y()}});
    return array;
}

QVector<QPointF> pointsFromJson(const QJsonArray &array)
{
    QVector<QPointF> points;
    points.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject point = value.toObject();
        points.append(QPointF(point.value(QStringLiteral("x")).toDouble(),
                              point.value(QStringLiteral("y")).toDouble()));
    }
    return points;
}

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

    m_templateBankList = new QListWidget(m_templateBankCard);
    m_templateBankList->setObjectName(QStringLiteral("templateBankListWidget"));
    m_templateBankList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_templateBankList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_templateBankList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_templateBankList->setMinimumHeight(116);
    m_templateBankList->setMaximumHeight(116);
    m_templateBankList->setUniformItemSizes(true);
    cardLayout->addWidget(m_templateBankList);

    m_templateBankSummaryLabel = new QLabel(m_templateBankCard);
    m_templateBankSummaryLabel->setObjectName(
                QStringLiteral("templateBankSummaryLabel"));
    m_templateBankSummaryLabel->setProperty("hint", true);
    cardLayout->addWidget(m_templateBankSummaryLabel);

    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6);
    m_addTemplateItemButton = new QPushButton(tr("+ 添加模板"), m_templateBankCard);
    m_addTemplateItemButton->setObjectName(
                QStringLiteral("addTemplateItemButton"));
    m_addTemplateItemButton->setProperty("actionRole", QStringLiteral("secondary"));
    m_renameTemplateItemButton = new QPushButton(tr("重命名"), m_templateBankCard);
    m_renameTemplateItemButton->setObjectName(
                QStringLiteral("renameTemplateItemButton"));
    m_renameTemplateItemButton->setProperty("actionRole", QStringLiteral("plain"));
    m_deleteTemplateItemButton = new QPushButton(tr("删除模板项"), m_templateBankCard);
    m_deleteTemplateItemButton->setObjectName(
                QStringLiteral("deleteTemplateItemButton"));
    m_deleteTemplateItemButton->setProperty("actionRole", QStringLiteral("plain"));
    buttonLayout->addWidget(m_addTemplateItemButton, 1);
    buttonLayout->addWidget(m_renameTemplateItemButton);
    buttonLayout->addWidget(m_deleteTemplateItemButton);
    cardLayout->addLayout(buttonLayout);

    ui->parameterLayout->insertWidget(0, m_templateBankCard);
    ui->templateTitle->setText(tr("当前模板设置"));
    ui->createTemplateButton->setText(tr("创建当前模型"));
    ui->deleteTemplateButton->setText(tr("清除当前模型"));

    TemplateItemState initial;
    initial.templateId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    initial.name = tr("模板1");
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

    connect(ui->originModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        m_originMode = index == 1 ? QStringLiteral("custom") : QStringLiteral("centroid");
        if (m_originMode == QStringLiteral("centroid")) {
            m_previewHelper->setPointSelectionEnabled(false);
            ui->selectOriginButton->setChecked(false);
        }
        updateOriginControls();
        showReferenceImage();
    });
    connect(ui->selectOriginButton, &QPushButton::clicked, this, [this](bool checked) {
        stopEditing();
        ui->selectOriginButton->setChecked(checked);
        m_previewHelper->setPointSelectionEnabled(checked);
        ui->viewerTitleLabel->setText(checked ? tr("点击图像选择自定义定位点") : tr("基准图"));
    });
    connect(m_previewHelper, &FrameViewHelper::pointSelected, this,
            [this](const QPointF &point) {
        m_originMode = QStringLiteral("custom");
        m_customOriginNormalized = point;
        ui->originModeComboBox->setCurrentIndex(1);
        updateOriginControls();
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
        m_templateMaskRegionType = QStringLiteral("none");
        m_templateMaskRoi = QRectF();
        m_templateMaskPolygon.clear();
        m_templateMaskCircle = CircleRoi();
        m_templateMaskClearButton->setEnabled(false);
        m_previewHelper->clearPolygonRoi();
        markModelDirty();
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
        if (m_editTarget == EditTarget::TemplateRect) {
            m_templateRoi = roi;
            m_templatePolygon.clear();
            m_templateRegionType = QStringLiteral("rectangle");
            markModelDirty();
        } else if (m_editTarget == EditTarget::TemplateMaskRect) {
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
            ui->statusLabel->setText(tr("搜索区域已更新"));
        }
    });
    connect(m_previewHelper, &FrameViewHelper::polygonChanged, this,
            [this](const QVector<QPointF> &points) {
        if (points.size() < 3)
            return;
        if (m_editTarget == EditTarget::TemplatePolygon) {
            m_templatePolygon = points;
            m_templateRoi = boundingRect(points);
            m_templateRegionType = QStringLiteral("polygon");
            markModelDirty();
        } else if (m_editTarget == EditTarget::TemplateMaskPolygon) {
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
        if (m_editTarget == EditTarget::TemplateMaskCircle) {
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
            this, [this]() { updateContrastControls(); markAllModelsDirty(); });

    connect(ui->maxMatchesSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
        ui->minMatchCountSpinBox->setMaximum(value);
        ui->maxMatchCountSpinBox->setMaximum(value);
        if (ui->minMatchCountSpinBox->value() > value)
            ui->minMatchCountSpinBox->setValue(value);
        if (ui->maxMatchCountSpinBox->value() > value)
            ui->maxMatchCountSpinBox->setValue(value);
    });
    connect(ui->minMatchCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
        if (ui->maxMatchCountSpinBox->value() < value)
            ui->maxMatchCountSpinBox->setValue(value);
    });
    connect(ui->maxMatchCountSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
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
                this, [this]() { markAllModelsDirty(); });
    }
    connect(ui->polarityComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { markAllModelsDirty(); });

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
        m_searchRegionType = QStringLiteral("full");
        m_searchRoi = QRectF(0.0, 0.0, 1.0, 1.0);
        m_searchPolygon.clear();
        m_searchCircle = CircleRoi();
        ui->searchGlobalButton->setChecked(true);
        m_previewHelper->clearRoi();
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->clearCircleRoi();
        showReferenceImage();
        ui->statusLabel->setText(tr("已切换为全局搜索"));
        return;
    }
    if (!ReferenceImageProvider::instance().hasReferenceFrame()) {
        ui->statusLabel->setText(tr("请先设置基准图"));
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
                ? tr("绘制矩形模板屏蔽区域")
                : (isTemplate ? tr("绘制模板矩形") : tr("绘制搜索矩形")));
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
                    isTemplateMask ? tr("绘制模板屏蔽区域")
                                   : (isTemplate ? tr("绘制模板多边形")
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
                ? tr("绘制圆形模板屏蔽区域") : tr("绘制圆形搜索区域"));
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
    m_templates.replace(index, item);
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
    refreshActiveTemplateUi();
    showReferenceImage();
}

void TemplateLocationDialog::refreshActiveTemplateUi()
{
    const int index = activeTemplateIndex();
    const bool hasActive = index >= 0;
    ui->templateRectButton->setEnabled(hasActive);
    ui->templatePolygonButton->setEnabled(hasActive);
    if (m_templateMaskRectButton)
        m_templateMaskRectButton->setEnabled(hasActive);
    if (m_templateMaskCircleButton)
        m_templateMaskCircleButton->setEnabled(hasActive);
    if (m_templateMaskPolygonButton)
        m_templateMaskPolygonButton->setEnabled(hasActive);
    if (!hasActive) {
        ui->modelStatusLabel->setText(tr("未选择模板"));
        ui->createTemplateButton->setEnabled(false);
        ui->deleteTemplateButton->setEnabled(false);
        if (m_templateMaskClearButton)
            m_templateMaskClearButton->setEnabled(false);
        return;
    }

    const TemplateItemState item = activeTemplateForOutput();
    ui->createTemplateButton->setEnabled(true);
    ui->createTemplateButton->setText(item.modelCreated
                                      ? tr("重新创建模板") : tr("创建模板"));
    ui->deleteTemplateButton->setEnabled(item.modelCreated);
    if (!item.modelError.trimmed().isEmpty())
        ui->modelStatusLabel->setText(tr("模板错误：%1").arg(item.modelError));
    else if (item.modelCreated)
        ui->modelStatusLabel->setText(tr("模板已创建"));
    else if (validRect(item.roi))
        ui->modelStatusLabel->setText(tr("需要创建模板"));
    else
        ui->modelStatusLabel->setText(tr("未创建模板"));
    if (m_templateMaskClearButton) {
        m_templateMaskClearButton->setEnabled(
                    item.maskRegionType != QStringLiteral("none"));
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
    for (int index = 0; index < m_templates.size(); ++index) {
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
                    tr("%1  · P%2 · %3")
                    .arg(item.name)
                    .arg(item.priority + 1)
                    .arg(status), m_templateBankList);
        listItem->setData(kTemplateIdRole, item.templateId);
        listItem->setToolTip(tr("模板 ID：%1").arg(item.templateId));
        listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable);
        listItem->setCheckState(item.enabled ? Qt::Checked : Qt::Unchecked);
        if (item.templateId == m_activeTemplateId)
            selectedRow = index;
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

    // Once a bank contains alternative ROIs, a centroid that silently follows
    // the selected item is not a common output anchor. Freeze the legacy first
    // item's current anchor before promotion so all items share one image point.
    if (m_templates.size() == 1 &&
            m_originMode == QStringLiteral("centroid")) {
        const TemplateItemState first = m_templates.first();
        if (validRect(first.roi)) {
            m_originMode = QStringLiteral("custom");
            m_customOriginNormalized = first.regionType == QStringLiteral("polygon") &&
                    !first.polygon.isEmpty()
                    ? polygonCentroid(first.polygon)
                    : first.roi.center();
            const QSignalBlocker blocker(ui->originModeComboBox);
            ui->originModeComboBox->setCurrentIndex(1);
            updateOriginControls();
        }
    }

    QSet<QString> names;
    int maximumPriority = -1;
    for (const TemplateItemState &item : qAsConst(m_templates)) {
        names.insert(item.name);
        maximumPriority = qMax(maximumPriority, item.priority);
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
    item.modelCacheKey = QStringLiteral("template_location_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_templates.append(item);
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
        const QSignalBlocker blocker(m_templateBankList);
        listItem->setCheckState(Qt::Checked);
        ui->statusLabel->setText(tr("至少保留一个启用模板"));
        return;
    }
    if (m_templates.at(index).enabled == enabled)
        return;
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
    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        return;
    }
    m_previewHelper->setImage(image);
    QVector<ToolOverlay> overlays;
    if (m_modelCreated && !m_templateDisplayOverlays.isEmpty())
        overlays = m_templateDisplayOverlays;
    if (m_editTarget != EditTarget::TemplateMaskRect &&
            m_editTarget != EditTarget::TemplateMaskCircle &&
            m_editTarget != EditTarget::TemplateMaskPolygon) {
        appendTemplateMaskOverlay(&overlays, m_templateMaskRegionType,
                                  m_templateMaskRoi, m_templateMaskPolygon,
                                  m_templateMaskCircle,
                                  image.width(), image.height());
    }
    if (m_originMode == QStringLiteral("custom")) {
        const QPointF point(m_customOriginNormalized.x() * image.width(),
                            m_customOriginNormalized.y() * image.height());
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
    ui->viewerTitleLabel->setText(tr("基准图"));
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
    if (!ReferenceImageProvider::instance().hasReferenceFrame()) {
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
    flushActiveTemplateEditor();
    m_modelCreated = true;
    m_creatingTemplate = true;
    runOnFrame(ReferenceImageProvider::instance().referenceFrame(), tr("基准图建模"));
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
    runOnFrame(ReferenceImageProvider::instance().referenceFrame(), tr("基准图测试"));
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

void TemplateLocationDialog::runOnFrame(const cv::Mat &frame, const QString &title)
{
    if (m_processing || frame.empty()) {
        if (frame.empty())
            ui->statusLabel->setText(tr("输入图像为空"));
        return;
    }
    m_processing = true;
    ToolRequest request;
    request.config = m_creatingTemplate ? activeTemplateToolConfig() : toolConfig();
    request.config.params.insert(QStringLiteral("modelCreated"), true);
    if (m_creatingTemplate) {
        request.config.params.insert(QStringLiteral("minMatchCount"), 1);
        request.config.params.insert(QStringLiteral("maxMatchCount"),
                                     ui->maxMatchesSpinBox->value());
    }
    request.image = frame;
    request.referenceImage = ReferenceImageProvider::instance().referenceFrame();
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
                                  ReferenceImageProvider::instance().referenceFrame().cols,
                                  ReferenceImageProvider::instance().referenceFrame().rows);
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

TemplateLocationDialog::TemplateItemState
TemplateLocationDialog::templateItemFromJson(const QJsonObject &json,
                                             int fallbackIndex) const
{
    TemplateItemState item;
    item.extra = json;
    item.templateId = json.value(QStringLiteral("templateId")).toString().trimmed();
    if (item.templateId.isEmpty())
        item.templateId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.name = json.value(QStringLiteral("name")).toString().trimmed();
    if (item.name.isEmpty())
        item.name = tr("模板%1").arg(fallbackIndex + 1);
    item.enabled = json.value(QStringLiteral("enabled")).toBool(true);
    item.priority = json.value(QStringLiteral("priority")).toInt(fallbackIndex);
    item.regionType = json.value(QStringLiteral("templateRegionType"))
            .toString(QStringLiteral("rectangle"));
    item.roi = rectFromJson(
                json.value(QStringLiteral("templateRoiNormalized")).toObject(),
                QRectF());
    item.polygon = pointsFromJson(
                json.value(QStringLiteral("templatePolygonNormalized")).toArray());
    item.maskRegionType = json.value(QStringLiteral("templateMaskRegionType"))
            .toString(json.value(QStringLiteral("templateMaskPolygonNormalized"))
                      .toArray().isEmpty()
                      ? QStringLiteral("none") : QStringLiteral("polygon"));
    item.maskRoi = rectFromJson(
                json.value(QStringLiteral("templateMaskRoiNormalized")).toObject(),
                QRectF());
    item.maskPolygon = pointsFromJson(
                json.value(QStringLiteral("templateMaskPolygonNormalized")).toArray());
    const QJsonObject circleCenter = json.value(
                QStringLiteral("templateMaskCircleCenterNormalized")).toObject();
    item.maskCircle.centerNormalized = QPointF(
                circleCenter.value(QStringLiteral("x")).toDouble(
                    item.maskRoi.center().x()),
                circleCenter.value(QStringLiteral("y")).toDouble(
                    item.maskRoi.center().y()));
    item.maskCircle.radiusNormalized = json.value(
                QStringLiteral("templateMaskCircleRadiusNormalized")).toDouble(
                    qMin(item.maskRoi.width(), item.maskRoi.height()) / 2.0);
    item.maskCircle.boundingRectNormalized = QRectF(
                item.maskCircle.centerNormalized.x() -
                    item.maskCircle.radiusNormalized,
                item.maskCircle.centerNormalized.y() -
                    item.maskCircle.radiusNormalized,
                item.maskCircle.radiusNormalized * 2.0,
                item.maskCircle.radiusNormalized * 2.0);
    item.maskCircle.valid = item.maskRegionType == QStringLiteral("circle") &&
            item.maskCircle.radiusNormalized > 0.0 &&
            validRect(item.maskCircle.boundingRectNormalized);
    item.modelCacheKey = json.value(QStringLiteral("modelCacheKey"))
            .toString().trimmed();
    if (item.modelCacheKey.isEmpty()) {
        item.modelCacheKey = QStringLiteral("template_location_%1")
                .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    item.modelCreated = json.value(QStringLiteral("modelCreated")).toBool(false);
    return item;
}

QJsonObject TemplateLocationDialog::templateItemToJson(
        const TemplateItemState &item) const
{
    QJsonObject json = item.extra;
    json.insert(QStringLiteral("templateId"), item.templateId);
    json.insert(QStringLiteral("name"), item.name);
    json.insert(QStringLiteral("enabled"), item.enabled);
    json.insert(QStringLiteral("priority"), item.priority);
    json.insert(QStringLiteral("templateRegionType"), item.regionType);
    json.insert(QStringLiteral("templateRoiNormalized"), rectToJson(item.roi));
    json.insert(QStringLiteral("templatePolygonNormalized"), pointsToJson(item.polygon));
    json.insert(QStringLiteral("templateMaskRegionType"), item.maskRegionType);
    json.insert(QStringLiteral("templateMaskRoiNormalized"), rectToJson(item.maskRoi));
    json.insert(QStringLiteral("templateMaskPolygonNormalized"), pointsToJson(item.maskPolygon));
    json.insert(QStringLiteral("templateMaskCircleCenterNormalized"),
                QJsonObject{{QStringLiteral("x"), item.maskCircle.centerNormalized.x()},
                            {QStringLiteral("y"), item.maskCircle.centerNormalized.y()}});
    json.insert(QStringLiteral("templateMaskCircleRadiusNormalized"),
                item.maskCircle.radiusNormalized);
    json.insert(QStringLiteral("modelCacheKey"), item.modelCacheKey);
    json.insert(QStringLiteral("modelCreated"), item.modelCreated);
    return json;
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

    QList<TemplateItemState> items = m_templates;
    const int activeIndex = activeTemplateIndex();
    if (activeIndex >= 0 && activeIndex < items.size())
        items.replace(activeIndex, activeTemplateForOutput());

    QJsonObject params = m_config.params;
    params.insert(QStringLiteral("searchRegionType"), m_searchRegionType);
    params.insert(QStringLiteral("searchRoiNormalized"), rectToJson(m_searchRoi));
    params.insert(QStringLiteral("searchPolygonNormalized"), pointsToJson(m_searchPolygon));
    params.insert(QStringLiteral("searchCircleCenterNormalized"),
                  QJsonObject{{QStringLiteral("x"), m_searchCircle.centerNormalized.x()},
                              {QStringLiteral("y"), m_searchCircle.centerNormalized.y()}});
    params.insert(QStringLiteral("searchCircleRadiusNormalized"), m_searchCircle.radiusNormalized);
    params.insert(QStringLiteral("minScore"), ui->minScoreSpinBox->value());
    params.insert(QStringLiteral("angleStart"), ui->angleMinSpinBox->value());
    params.insert(QStringLiteral("angleExtent"), ui->angleMaxSpinBox->value() - ui->angleMinSpinBox->value());
    params.insert(QStringLiteral("angleEnd"), ui->angleMaxSpinBox->value());
    params.insert(QStringLiteral("scaleMin"), ui->scaleMinSpinBox->value());
    params.insert(QStringLiteral("scaleMax"), ui->scaleMaxSpinBox->value());
    params.insert(QStringLiteral("polarity"), polarityValue());
    params.insert(QStringLiteral("contrastMode"), contrastModeValue());
    params.insert(QStringLiteral("contrast"), ui->contrastSpinBox->value());
    params.insert(QStringLiteral("minContrast"), ui->minContrastSpinBox->value());
    params.insert(QStringLiteral("numLevels"), ui->numLevelsSpinBox->value());
    params.insert(QStringLiteral("subPixel"), ui->subPixelComboBox->currentText());
    params.insert(QStringLiteral("greediness"), ui->greedinessSpinBox->value());
    params.insert(QStringLiteral("timeoutMs"), ui->timeoutSpinBox->value());
    params.insert(QStringLiteral("maxMatches"), ui->maxMatchesSpinBox->value());
    params.insert(QStringLiteral("minMatchCount"), ui->minMatchCountSpinBox->value());
    params.insert(QStringLiteral("maxMatchCount"), ui->maxMatchCountSpinBox->value());
    params.insert(QStringLiteral("maxOverlap"), ui->maxOverlapSpinBox->value());
    params.insert(QStringLiteral("originMode"), m_originMode);
    params.insert(QStringLiteral("customOriginNormalized"),
                  QJsonObject{{QStringLiteral("x"), m_customOriginNormalized.x()},
                              {QStringLiteral("y"), m_customOriginNormalized.y()}});
    if (m_config.params.contains(QStringLiteral("halconSoPath"))) {
        params.insert(QStringLiteral("halconSoPath"),
                      m_config.params.value(QStringLiteral("halconSoPath")));
    }

    int enabledCount = 0;
    bool allEnabledCreated = true;
    for (const TemplateItemState &item : qAsConst(items)) {
        if (!item.enabled)
            continue;
        ++enabledCount;
        allEnabledCreated = allEnabledCreated && item.modelCreated;
    }
    if (m_usesTemplateBankSchema || items.size() > 1) {
        params.insert(QStringLiteral("version"), 5);
        params.insert(QStringLiteral("templateMode"),
                      QStringLiteral("alternatives"));
        QJsonArray templates;
        for (const TemplateItemState &item : qAsConst(items))
            templates.append(templateItemToJson(item));
        params.insert(QStringLiteral("templates"), templates);
        params.insert(QStringLiteral("modelCreated"),
                      enabledCount > 0 && allEnabledCreated);
        params.insert(QStringLiteral("primaryMatchStrategy"),
                      m_config.params.value(QStringLiteral("primaryMatchStrategy"))
                      .toString(QStringLiteral("best_score")));
        params.insert(QStringLiteral("primaryTemplateId"),
                      m_config.params.value(QStringLiteral("primaryTemplateId")));
        if (m_config.params.contains(QStringLiteral("fusion")))
            params.insert(QStringLiteral("fusion"),
                          m_config.params.value(QStringLiteral("fusion")));
    } else {
        params.insert(QStringLiteral("version"), 4);
        const TemplateItemState item = items.isEmpty()
                ? TemplateItemState() : items.first();
        params.insert(QStringLiteral("templateRegionType"), item.regionType);
        params.insert(QStringLiteral("templateRoiNormalized"), rectToJson(item.roi));
        params.insert(QStringLiteral("templatePolygonNormalized"),
                      pointsToJson(item.polygon));
        params.insert(QStringLiteral("templateMaskRegionType"), item.maskRegionType);
        params.insert(QStringLiteral("templateMaskRoiNormalized"),
                      rectToJson(item.maskRoi));
        params.insert(QStringLiteral("templateMaskPolygonNormalized"),
                      pointsToJson(item.maskPolygon));
        params.insert(QStringLiteral("templateMaskCircleCenterNormalized"),
                      QJsonObject{
                          {QStringLiteral("x"), item.maskCircle.centerNormalized.x()},
                          {QStringLiteral("y"), item.maskCircle.centerNormalized.y()}});
        params.insert(QStringLiteral("templateMaskCircleRadiusNormalized"),
                      item.maskCircle.radiusNormalized);
        params.insert(QStringLiteral("modelCacheKey"), item.modelCacheKey);
        params.insert(QStringLiteral("modelCreated"), item.modelCreated);
    }
    TemplateLocationModelBankConfig normalizedBank =
            TemplateLocationConfig::fromToolParams(params, config.toolId);
    normalizedBank.version = (m_usesTemplateBankSchema || items.size() > 1)
            ? TemplateLocationConfig::ModelBankParamsVersion
            : TemplateLocationConfig::LegacyParamsVersion;
    config.params = TemplateLocationConfig::toToolParams(normalizedBank);
    config.summary = tr("模板定位，启用 %1/%2 个模板，查找 %3 个，数量 %4~%5，最低得分 %6%，角度 %7°~%8°")
            .arg(enabledCount)
            .arg(items.size())
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

    ToolConfig config = toolConfig();
    QJsonObject params = config.params;
    const TemplateItemState item = activeTemplateForOutput();
    TemplateItemState buildItem = item;
    buildItem.enabled = true;
    buildItem.modelCreated = true;
    params.insert(QStringLiteral("version"), 5);
    params.insert(QStringLiteral("templateMode"),
                  QStringLiteral("alternatives"));
    params.insert(QStringLiteral("templates"),
                  QJsonArray{templateItemToJson(buildItem)});
    params.insert(QStringLiteral("primaryMatchStrategy"),
                  QStringLiteral("best_score"));
    params.insert(QStringLiteral("primaryTemplateId"), item.templateId);
    params.remove(QStringLiteral("templateRegionType"));
    params.remove(QStringLiteral("templateRoiNormalized"));
    params.remove(QStringLiteral("templatePolygonNormalized"));
    params.remove(QStringLiteral("templateMaskRegionType"));
    params.remove(QStringLiteral("templateMaskRoiNormalized"));
    params.remove(QStringLiteral("templateMaskPolygonNormalized"));
    params.remove(QStringLiteral("templateMaskCircleCenterNormalized"));
    params.remove(QStringLiteral("templateMaskCircleRadiusNormalized"));
    params.remove(QStringLiteral("modelCacheKey"));
    params.insert(QStringLiteral("modelCreated"), true);
    config.params = params;
    return config;
}

void TemplateLocationDialog::loadFromConfig(const ToolConfig &config)
{
    if (m_running)
        toggleContinuousTest();
    stopEditing();
    m_config = config;
    m_config.toolType = ToolType::TemplateLocation;
    m_config.category = ToolCategory::Location;
    const QJsonObject params = config.params;
    const TemplateLocationModelBankConfig decoded =
            TemplateLocationConfig::fromToolParams(params, config.toolId);
    m_configReadOnly = !decoded.decodeSupported || decoded.rawPassthrough;
    m_configReadOnlyStatus = decoded.decodeStatus;
    m_configReadOnlyMessage = decoded.decodeMessage;
    m_pendingCacheDeletes.clear();
    m_templates.clear();
    const QJsonArray templateArray = params.value(QStringLiteral("templates")).toArray();
    m_usesTemplateBankSchema = params.value(QStringLiteral("version")).toInt() >= 5 ||
            params.contains(QStringLiteral("templates"));
    if (m_usesTemplateBankSchema) {
        QSet<QString> ids;
        for (int index = 0; index < templateArray.size(); ++index) {
            TemplateItemState item = templateItemFromJson(
                        templateArray.at(index).toObject(), index);
            if (ids.contains(item.templateId)) {
                item.templateId = QUuid::createUuid()
                        .toString(QUuid::WithoutBraces);
                item.modelCreated = false;
                item.modelError = tr("原配置包含重复模板 ID");
            }
            ids.insert(item.templateId);
            m_templates.append(item);
        }
    } else {
        QJsonObject legacyItem = params;
        legacyItem.insert(QStringLiteral("templateId"),
                          QStringLiteral("legacy-template"));
        legacyItem.insert(QStringLiteral("name"), tr("模板1"));
        legacyItem.insert(QStringLiteral("enabled"), true);
        legacyItem.insert(QStringLiteral("priority"), 0);
        TemplateItemState item = templateItemFromJson(legacyItem, 0);
        // Flat v4 extension fields belong to the top-level contract. They are
        // retained by m_config.params, but must not be duplicated into the
        // first templates[] item when the user promotes the config to v5.
        item.extra = QJsonObject();
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

    m_searchRegionType = params.value(QStringLiteral("searchRegionType")).toString(QStringLiteral("full"));
    m_searchRoi = rectFromJson(params.value(QStringLiteral("searchRoiNormalized")).toObject(),
                               QRectF(0.0, 0.0, 1.0, 1.0));
    m_searchPolygon = pointsFromJson(params.value(QStringLiteral("searchPolygonNormalized")).toArray());
    const QJsonObject circleCenter = params.value(
                QStringLiteral("searchCircleCenterNormalized")).toObject();
    m_searchCircle.centerNormalized = QPointF(
                circleCenter.value(QStringLiteral("x")).toDouble(m_searchRoi.center().x()),
                circleCenter.value(QStringLiteral("y")).toDouble(m_searchRoi.center().y()));
    m_searchCircle.radiusNormalized = params.value(
                QStringLiteral("searchCircleRadiusNormalized")).toDouble(
                qMin(m_searchRoi.width(), m_searchRoi.height()) / 2.0);
    m_searchCircle.boundingRectNormalized = QRectF(
                m_searchCircle.centerNormalized.x() - m_searchCircle.radiusNormalized,
                m_searchCircle.centerNormalized.y() - m_searchCircle.radiusNormalized,
                m_searchCircle.radiusNormalized * 2.0,
                m_searchCircle.radiusNormalized * 2.0);
    m_searchCircle.valid = m_searchRegionType == QStringLiteral("circle") &&
            m_searchCircle.radiusNormalized > 0.0 && validRect(m_searchCircle.boundingRectNormalized);
    m_originMode = params.value(QStringLiteral("originMode")).toString(QStringLiteral("centroid"));
    const QJsonObject customOrigin = params.value(QStringLiteral("customOriginNormalized")).toObject();
    m_customOriginNormalized = QPointF(
                customOrigin.value(QStringLiteral("x")).toDouble(0.5),
                customOrigin.value(QStringLiteral("y")).toDouble(0.5));

    const QSignalBlocker blockAngleMin(ui->angleMinSpinBox);
    const QSignalBlocker blockAngleMax(ui->angleMaxSpinBox);
    const QSignalBlocker blockScaleMin(ui->scaleMinSpinBox);
    const QSignalBlocker blockScaleMax(ui->scaleMaxSpinBox);
    const QSignalBlocker blockContrast(ui->contrastSpinBox);
    const QSignalBlocker blockMinContrast(ui->minContrastSpinBox);
    const QSignalBlocker blockLevels(ui->numLevelsSpinBox);
    const QSignalBlocker blockPolarity(ui->polarityComboBox);
    const QSignalBlocker blockMode(ui->contrastModeComboBox);
    const QSignalBlocker blockOriginMode(ui->originModeComboBox);
    ui->minScoreSpinBox->setValue(params.value(QStringLiteral("minScore")).toInt(50));
    const int angleStart = params.value(QStringLiteral("angleStart")).toInt(-45);
    ui->angleMinSpinBox->setValue(angleStart);
    ui->angleMaxSpinBox->setValue(params.contains(QStringLiteral("angleEnd"))
                                  ? params.value(QStringLiteral("angleEnd")).toInt(45)
                                  : angleStart + params.value(QStringLiteral("angleExtent")).toInt(90));
    ui->scaleMinSpinBox->setValue(params.value(QStringLiteral("scaleMin")).toInt(100));
    ui->scaleMaxSpinBox->setValue(params.value(QStringLiteral("scaleMax")).toInt(100));
    setPolarityValue(params.value(QStringLiteral("polarity")).toString(QStringLiteral("use_polarity")));
    ui->contrastModeComboBox->setCurrentIndex(params.value(QStringLiteral("contrastMode")).toString() == QStringLiteral("manual") ? 1 : 0);
    ui->contrastSpinBox->setValue(params.value(QStringLiteral("contrast")).toInt(40));
    ui->minContrastSpinBox->setValue(params.value(QStringLiteral("minContrast")).toInt(10));
    ui->numLevelsSpinBox->setValue(params.value(QStringLiteral("numLevels")).toInt(0));
    ui->subPixelComboBox->setCurrentIndex(params.value(QStringLiteral("subPixel")).toString() == QStringLiteral("none") ? 1 : 0);
    ui->greedinessSpinBox->setValue(params.value(QStringLiteral("greediness")).toDouble(0.5));
    ui->timeoutSpinBox->setValue(params.value(QStringLiteral("timeoutMs")).toInt(2000));
    ui->maxMatchesSpinBox->setValue(params.value(QStringLiteral("maxMatches")).toInt(1));
    ui->minMatchCountSpinBox->setMaximum(ui->maxMatchesSpinBox->value());
    ui->maxMatchCountSpinBox->setMaximum(ui->maxMatchesSpinBox->value());
    ui->minMatchCountSpinBox->setValue(params.value(QStringLiteral("minMatchCount")).toInt(1));
    ui->maxMatchCountSpinBox->setValue(params.value(QStringLiteral("maxMatchCount"))
                                       .toInt(ui->maxMatchesSpinBox->value()));
    ui->maxOverlapSpinBox->setValue(params.value(QStringLiteral("maxOverlap")).toInt(50));
    ui->originModeComboBox->setCurrentIndex(m_originMode == QStringLiteral("custom") ? 1 : 0);
    updateContrastControls();
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
                    tr("配置中的 v5 模板库为空，请添加并创建至少一个模板"));
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
}

ToolPreviewSnapshot TemplateLocationDialog::referencePreviewSnapshot() const
{
    return m_snapshot;
}
