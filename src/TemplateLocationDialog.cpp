#include "TemplateLocationDialog.h"

#include "PlanDialogUtils.h"
#include "UiStyleRoles.h"
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
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

#include <cmath>

namespace {

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

    // ROI 操作使用独立的右对齐行，避免搜索参数受图标列宽影响。
    ui->templateLayout->removeWidget(ui->templateRectButton);
    ui->templateLayout->removeWidget(ui->templatePolygonButton);
    auto *templateRoiLayout = new QHBoxLayout;
    templateRoiLayout->setContentsMargins(0, 0, 0, 0);
    templateRoiLayout->setSpacing(4);
    templateRoiLayout->addStretch();
    templateRoiLayout->addWidget(ui->templateRectButton);
    templateRoiLayout->addWidget(ui->templatePolygonButton);
    ui->templateLayout->addLayout(templateRoiLayout, 1, 1, 1, 2);

    ui->templateLayout->removeWidget(ui->createTemplateButton);
    ui->templateLayout->removeWidget(ui->deleteTemplateButton);
    auto *templateCommandLayout = new QHBoxLayout;
    templateCommandLayout->setContentsMargins(0, 0, 0, 0);
    templateCommandLayout->setSpacing(4);
    templateCommandLayout->addWidget(ui->createTemplateButton, 1);
    templateCommandLayout->addWidget(ui->deleteTemplateButton);
    ui->templateLayout->addLayout(templateCommandLayout, 2, 0, 1, 3);

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
    m_matchResultTable->setColumnCount(6);
    m_matchResultTable->setHorizontalHeaderLabels(
                QStringList{tr("序号"), tr("X"), tr("Y"), tr("角度"), tr("缩放"), tr("得分")});
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

void TemplateLocationDialog::connectControls()
{
    connect(ui->closeButton, &QToolButton::clicked, this, &QDialog::reject);
    connect(ui->basicModeButton, &QPushButton::clicked, this, [this]() { setAdvancedVisible(false); });
    connect(ui->allModeButton, &QPushButton::clicked, this, [this]() { setAdvancedVisible(true); });
    connect(ui->fitViewButton, &QPushButton::clicked, m_previewHelper, &FrameViewHelper::fitToView);

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
        if (m_editTarget != EditTarget::SearchCircle || !circle.valid)
            return;
        m_searchCircle = circle;
        m_searchRoi = circle.boundingRectNormalized;
        m_searchPolygon.clear();
        m_searchRegionType = QStringLiteral("circle");
        ui->statusLabel->setText(tr("圆形搜索区域已更新"));
    });
    connect(m_previewHelper, &FrameViewHelper::roiSelectionRejected, this,
            [this]() { ui->statusLabel->setText(tr("ROI 无效，请绘制宽高至少 2 像素的区域")); });
    connect(m_previewHelper, &FrameViewHelper::polygonSelectionRejected, this,
            [this]() { ui->statusLabel->setText(tr("多边形至少需要 3 个点")); });
    connect(m_previewHelper, &FrameViewHelper::circleSelectionRejected, this,
            [this]() { ui->statusLabel->setText(tr("圆形区域半径至少需要 2 像素")); });

    connect(ui->contrastModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { updateContrastControls(); markModelDirty(); });

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
        QVector<ToolOverlay> overlays = m_lastDisplayResult.overlays;
        for (ToolOverlay &overlay : overlays) {
            if (!overlay.extra.contains(QStringLiteral("matchIndex")))
                continue;
            overlay.extra.insert(QStringLiteral("emphasis"),
                                 overlay.extra.value(QStringLiteral("matchIndex")).toInt() == row
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
                this, [this]() { markModelDirty(); });
    }
    connect(ui->polarityComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { markModelDirty(); });

    connect(ui->createTemplateButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::createTemplate);
    connect(ui->deleteTemplateButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::deleteTemplate);
    connect(ui->referenceTestButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::runReferenceTest);
    connect(ui->testRunButton, &QPushButton::clicked,
            this, &TemplateLocationDialog::toggleContinuousTest);
    connect(ui->finishButton, &QPushButton::clicked, this, [this]() {
        QString message;
        if (!validateTemplate(&message) || !validateParameters(&message) || !m_modelCreated) {
            if (message.isEmpty())
                message = tr("模板参数已改变，请重新创建模板");
            ui->statusLabel->setText(message);
            return;
        }
        accept();
    });
}

void TemplateLocationDialog::setAdvancedVisible(bool visible)
{
    ui->advancedCard->setVisible(visible);
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
    if (target == EditTarget::TemplateRect || target == EditTarget::SearchRect) {
        const bool isTemplate = target == EditTarget::TemplateRect;
        (isTemplate ? ui->templateRectButton : ui->searchRectButton)->setChecked(true);
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->clearCircleRoi();
        const QRectF roi = isTemplate ? m_templateRoi : m_searchRoi;
        if (validRect(roi))
            m_previewHelper->setRoiRectNormalized(roi);
        else
            m_previewHelper->clearRoi();
        m_previewHelper->setRoiDrawingEnabled(true);
        ui->viewerTitleLabel->setText(isTemplate ? tr("绘制模板矩形") : tr("绘制搜索矩形"));
    } else if (target == EditTarget::TemplatePolygon || target == EditTarget::SearchPolygon) {
        const bool isTemplate = target == EditTarget::TemplatePolygon;
        (isTemplate ? ui->templatePolygonButton : ui->searchPolygonButton)->setChecked(true);
        m_previewHelper->clearRoi();
        m_previewHelper->clearCircleRoi();
        const QVector<QPointF> &polygon = isTemplate ? m_templatePolygon : m_searchPolygon;
        if (polygon.size() >= 3)
            m_previewHelper->setPolygonRoiNormalized(polygon);
        else
            m_previewHelper->clearPolygonRoi();
        m_previewHelper->setPolygonDrawingEnabled(true);
        ui->viewerTitleLabel->setText(isTemplate ? tr("绘制模板多边形")
                                                  : tr("绘制多边形搜索区域"));
    } else {
        ui->searchCircleButton->setChecked(true);
        m_previewHelper->clearRoi();
        m_previewHelper->clearPolygonRoi();
        if (m_searchCircle.valid)
            m_previewHelper->setCircleRoiNormalized(m_searchCircle);
        else
            m_previewHelper->clearCircleRoi();
        m_previewHelper->setCircleDrawingEnabled(true);
        ui->viewerTitleLabel->setText(tr("绘制圆形搜索区域"));
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
    TemplateLocationHalconRunner::clearPersistentCache(m_modelCacheKey);
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
    showReferenceImage();
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
    return valid;
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
}

void TemplateLocationDialog::runReferenceTest()
{
    QString message;
    if (!validateTemplate(&message) || !validateParameters(&message) || !m_modelCreated) {
        if (message.isEmpty()) message = tr("请先创建模板");
        ui->statusLabel->setText(message);
        return;
    }
    stopEditing();
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

    QString message;
    if (!validateTemplate(&message) || !validateParameters(&message) || !m_modelCreated) {
        if (message.isEmpty()) message = tr("请先创建模板");
        ui->statusLabel->setText(message);
        return;
    }
    if (!CameraFrameProvider::instance().hasFrame()) {
        ui->statusLabel->setText(tr("当前没有相机图像"));
        return;
    }
    stopEditing();
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
    request.config = toolConfig();
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
            if (overlay.label != QStringLiteral("match_result") ||
                    overlay.extra.value(QStringLiteral("matchIndex")).toInt(-1) != 0)
                continue;
            overlay.label = QStringLiteral("template_model");
            overlay.extra.insert(QStringLiteral("displayRole"),
                                 QStringLiteral("template_location_model"));
            templateOverlays.append(overlay);
        }
        m_templateDisplayOverlays = templateOverlays;
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
        const QStringList values{
            QString::number(row + 1),
            QString::number(match.value(QStringLiteral("x")).toDouble(), 'f', 2),
            QString::number(match.value(QStringLiteral("y")).toDouble(), 'f', 2),
            QString::number(match.value(QStringLiteral("angleDeg")).toDouble(), 'f', 2) + tr("°"),
            QString::number(match.value(QStringLiteral("scale")).toDouble(), 'f', 3),
            QString::number(match.value(QStringLiteral("score")).toDouble() * 100.0, 'f', 1) + tr("%")
        };
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values.at(column));
            item->setTextAlignment(Qt::AlignCenter);
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
    ToolConfig config = m_config;
    config.toolType = ToolType::TemplateLocation;
    config.category = ToolCategory::Location;
    config.toolName = tr("模板定位");
    config.displayName = tr("模板定位");
    config.roiNormalized = m_searchRoi;

    QJsonObject params;
    params.insert(QStringLiteral("version"), 2);
    params.insert(QStringLiteral("templateRegionType"), m_templateRegionType);
    params.insert(QStringLiteral("templateRoiNormalized"), rectToJson(m_templateRoi));
    params.insert(QStringLiteral("templatePolygonNormalized"), pointsToJson(m_templatePolygon));
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
    params.insert(QStringLiteral("modelCacheKey"), m_modelCacheKey);
    params.insert(QStringLiteral("modelCreated"), m_modelCreated);
    config.params = params;
    config.summary = tr("模板定位，查找 %1 个，数量 %2~%3，最低得分 %4%，角度 %5°~%6°")
            .arg(ui->maxMatchesSpinBox->value())
            .arg(ui->minMatchCountSpinBox->value())
            .arg(ui->maxMatchCountSpinBox->value())
            .arg(ui->minScoreSpinBox->value())
            .arg(ui->angleMinSpinBox->value())
            .arg(ui->angleMaxSpinBox->value());
    return config;
}

void TemplateLocationDialog::loadFromConfig(const ToolConfig &config)
{
    m_config = config;
    m_config.toolType = ToolType::TemplateLocation;
    m_config.category = ToolCategory::Location;
    const QJsonObject params = config.params;
    m_templateRegionType = params.value(QStringLiteral("templateRegionType")).toString(QStringLiteral("rectangle"));
    m_templateRoi = rectFromJson(params.value(QStringLiteral("templateRoiNormalized")).toObject(), QRectF());
    m_templatePolygon = pointsFromJson(params.value(QStringLiteral("templatePolygonNormalized")).toArray());
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
    m_modelCacheKey = params.value(QStringLiteral("modelCacheKey")).toString(m_modelCacheKey);
    m_modelCreated = params.value(QStringLiteral("modelCreated")).toBool(false);
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
    ui->modelStatusLabel->setText(m_modelCreated ? tr("模板已创建") : tr("未创建模板"));
    ui->createTemplateButton->setText(m_modelCreated ? tr("重新创建模板") : tr("创建模板"));
    ui->deleteTemplateButton->setEnabled(m_modelCreated);
    updateContrastControls();
    updateOriginControls();
    updateMatchResultTable(QJsonArray());
    stopEditing();
    showReferenceImage();
}

ToolPreviewSnapshot TemplateLocationDialog::referencePreviewSnapshot() const
{
    return m_snapshot;
}
