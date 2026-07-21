#include "ReferenceImageDialog.h"

#include <QDebug>
#include <QButtonGroup>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QLabel>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QToolButton>

#include <opencv2/imgproc.hpp>

#include "CameraParamsDialog.h"
#include "OutputDialog.h"
#include "PlanDialogUtils.h"
#include "SchemeStore.h"
#include "ToolsDialog.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameInputMetadata.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"
#include "ui_ReferenceImageDialog.h"

namespace {

// 将方案 JSON 中的归一化多边形点恢复为 FrameViewHelper 使用的点集合。
QVector<QPointF> polygonPointsFromJson(const QJsonArray &json)
{
    QVector<QPointF> points;
    points.reserve(json.size());
    for (const QJsonValue &value : json) {
        const QJsonObject point = value.toObject();
        if (!point.contains(QStringLiteral("x")) || !point.contains(QStringLiteral("y")))
            continue;
        points.append(QPointF(point.value(QStringLiteral("x")).toDouble(),
                              point.value(QStringLiteral("y")).toDouble()));
    }
    return points;
}

// 将 FrameViewHelper 返回的归一化多边形点序列化为方案 JSON。
QJsonArray polygonPointsToJson(const QVector<QPointF> &points)
{
    QJsonArray json;
    for (const QPointF &point : points) {
        json.append(QJsonObject{{QStringLiteral("x"), point.x()},
                                {QStringLiteral("y"), point.y()}});
    }
    return json;
}

// 计算归一化多边形的外接矩形，供配置摘要和后续算法快速取范围。
QRectF boundingRectForPoints(const QVector<QPointF> &points)
{
    if (points.isEmpty())
        return QRectF();

    qreal minX = points.first().x();
    qreal maxX = minX;
    qreal minY = points.first().y();
    qreal maxY = minY;
    for (const QPointF &point : points) {
        minX = qMin(minX, point.x());
        maxX = qMax(maxX, point.x());
        minY = qMin(minY, point.y());
        maxY = qMax(maxY, point.y());
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY)).normalized();
}

// 判断归一化矩形是否包含可用面积。
bool validNormalizedRect(const QRectF &rect)
{
    return rect.isValid() && rect.width() > 0.0 && rect.height() > 0.0;
}

} // namespace

ReferenceImageDialog::ReferenceImageDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ReferenceImageDialog)
{
    ui->setupUi(this);
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    setupUiState();
    setupReferenceImageControls();
    setupPositionCorrectionControls();
    connectNavigation();
    connect(&CameraFrameProvider::instance(),
            &CameraFrameProvider::frameUpdated,
            this,
            [this](const QImage &image) {
                if (m_liveCaptureMode && m_previewHelper) {
                    ui->viewerTitleLabel->setText(image.isNull() ? tr("当前无图像") : tr("当前图像"));
                    m_previewHelper->setImage(image);
                }
            });
    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &image) {
                if (!m_liveCaptureMode && m_previewHelper) {
                    m_previewHelper->setImage(image);
                    ui->viewerTitleLabel->setText(image.isNull() ? tr("请先设置基准图") : tr("基准图"));
                    restoreReferencePositionRoi();
                }
            });

    QString error;
    if (!SchemeStore::instance().ensureLoaded(&error)) {
        qWarning() << "[ReferenceImageDialog] 方案加载失败:" << error;
    } else if (!SchemeStore::instance().loadCurrentReferenceIntoProvider(&error)
               && !SchemeStore::instance().currentScheme().referenceImagePath.isEmpty()) {
        qWarning() << "[ReferenceImageDialog]" << error;
    }
    refreshSchemeHeader();
    loadPositionCorrectionConfig();
    showReferenceImageMode();
}

ReferenceImageDialog::~ReferenceImageDialog()
{
    delete ui;
}

void ReferenceImageDialog::setupUiState()
{
    PlanDialogUtils::configureDialogWindow(this, tr("方案编辑 - 基准图"));
    PlanDialogUtils::connectWindowButtons(this, ui->headerCloseButton, true);

    ui->cameraStepButton->setChecked(false);
    ui->referenceStepButton->setChecked(true);
    ui->toolsStepButton->setChecked(false);
    ui->outputStepButton->setChecked(false);
    refreshSchemeHeader();
}

void ReferenceImageDialog::connectNavigation()
{
    connect(ui->cameraStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openCameraParamsDialog);
    connect(ui->toolsStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openToolsDialog);
    connect(ui->outputStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openOutputDialog);
    connect(ui->previousButton, &QPushButton::clicked, this, &ReferenceImageDialog::openCameraParamsDialog);
    connect(ui->nextButton, &QPushButton::clicked, this, &ReferenceImageDialog::openToolsDialog);
    connect(ui->currentImageButton, &QPushButton::clicked, this, &ReferenceImageDialog::showCurrentImageMode);
    connect(m_captureImageButton, &QPushButton::clicked, this, &ReferenceImageDialog::captureReferenceImage);
    connect(m_exitCaptureButton, &QPushButton::clicked, this, &ReferenceImageDialog::showReferenceImageMode);
    connect(ui->setupExternalEditButton, &QToolButton::clicked, this, &ReferenceImageDialog::editCurrentSchemeName);
    connect(ui->setupSaveButton, &QToolButton::clicked, this, &ReferenceImageDialog::saveCurrentScheme);
    connect(ui->setupSaveAsButton, &QToolButton::clicked, this, &ReferenceImageDialog::saveCurrentSchemeAs);
    connect(ui->historyImageButton, &QPushButton::clicked, this, []() {
        qDebug() << "[ReferenceImageDialog] 历史图像暂未接入。";
    });
    connect(ui->pcImportButton, &QPushButton::clicked, this, &ReferenceImageDialog::importReferenceImageFromPc);
}

void ReferenceImageDialog::refreshSchemeHeader()
{
    ui->setupPageCodeLabel->setText(SchemeStore::instance().currentSchemeName());
}

void ReferenceImageDialog::editCurrentSchemeName()
{
    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        QMessageBox::warning(this, tr("方案名称"), tr("方案加载失败：%1").arg(error));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("编辑方案名"),
                                               tr("方案名"),
                                               QLineEdit::Normal,
                                               store.currentSchemeName(),
                                               &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    store.setSchemeName(name);
    saveCurrentScheme();
}

void ReferenceImageDialog::saveCurrentScheme()
{
    if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::None
            && !finishReferencePositionRoiEditing()) {
        return;
    }
    SchemeStore::instance().setReferencePositionCorrection(m_referencePositionCorrection);
    QString error;
    if (!SchemeStore::instance().saveCurrentScheme(&error)) {
        qWarning() << "[ReferenceImageDialog] 方案保存失败:" << error;
        QMessageBox::warning(this, tr("保存失败"), tr("方案保存失败：%1").arg(error));
        return;
    }
    refreshSchemeHeader();
    if (m_referencePositionCorrection.enabled && hasReferencePositionTemplateRoi())
        m_positionStatusLabel->setText(tr("配置已保存，尚未测试"));
}

void ReferenceImageDialog::setupPositionCorrectionControls()
{
    m_positionSettingsFrame = new QFrame(ui->positionCorrectionCard);
    m_positionSettingsFrame->setObjectName(QStringLiteral("positionCorrectionSettingsFrame"));
    m_positionSettingsFrame->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *settingsLayout = new QVBoxLayout(m_positionSettingsFrame);
    settingsLayout->setContentsMargins(0, 8, 0, 0);
    settingsLayout->setSpacing(10);

    QHBoxLayout *header = new QHBoxLayout;
    QLabel *title = new QLabel(tr("模板区域设置"), m_positionSettingsFrame);
    title->setProperty("role", QStringLiteral("cardTitle"));
    m_positionTestButton = new QPushButton(tr("测试运行"), m_positionSettingsFrame);
    m_positionTestButton->setObjectName(QStringLiteral("referencePositionTestButton"));
    m_positionTestButton->setToolTip(tr("测试基准图位置修正；当前后端尚未实现"));
    header->addWidget(title);
    header->addStretch(1);
    header->addWidget(m_positionTestButton);
    settingsLayout->addLayout(header);

    QHBoxLayout *tools = new QHBoxLayout;
    QLabel *field = new QLabel(tr("模板区域"), m_positionSettingsFrame);
    field->setProperty("role", QStringLiteral("rowField"));
    m_positionRectButton = new QPushButton(tr("矩形"), m_positionSettingsFrame);
    m_positionRectButton->setObjectName(QStringLiteral("referencePositionRectButton"));
    m_positionRectButton->setCheckable(true);
    m_positionPolygonButton = new QPushButton(tr("多边形"), m_positionSettingsFrame);
    m_positionPolygonButton->setObjectName(QStringLiteral("referencePositionPolygonButton"));
    m_positionPolygonButton->setCheckable(true);
    m_positionFinishButton = new QPushButton(tr("完成"), m_positionSettingsFrame);
    m_positionFinishButton->setObjectName(QStringLiteral("referencePositionRoiFinishButton"));
    m_positionFinishButton->setProperty("actionRole", QStringLiteral("primary"));
    tools->addWidget(field);
    tools->addStretch(1);
    tools->addWidget(m_positionRectButton);
    tools->addWidget(m_positionPolygonButton);
    tools->addWidget(m_positionFinishButton);
    settingsLayout->addLayout(tools);

    m_positionStatusLabel = new QLabel(tr("配置已保存，尚未测试"), m_positionSettingsFrame);
    m_positionStatusLabel->setObjectName(QStringLiteral("referencePositionStatusLabel"));
    m_positionStatusLabel->setProperty("hint", true);
    // 状态文字可能包含完整归一化坐标；允许换行并忽略横向 sizeHint，
    // 防止长文本把滚动区内容宽度和右侧预览画布一起撑偏。
    m_positionStatusLabel->setWordWrap(true);
    m_positionStatusLabel->setMinimumWidth(0);
    QSizePolicy statusSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    statusSizePolicy.setHeightForWidth(true);
    m_positionStatusLabel->setSizePolicy(statusSizePolicy);
    settingsLayout->addWidget(m_positionStatusLabel);
    ui->positionCorrectionCard->layout()->addWidget(m_positionSettingsFrame);

    QButtonGroup *roiTypeGroup = new QButtonGroup(this);
    roiTypeGroup->setExclusive(true);
    roiTypeGroup->addButton(m_positionRectButton);
    roiTypeGroup->addButton(m_positionPolygonButton);

    connect(ui->positionCorrectionCheckBox, &QCheckBox::toggled,
            this, &ReferenceImageDialog::updatePositionCorrectionUi);
    connect(m_positionRectButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::startReferencePositionRectEditing);
    connect(m_positionPolygonButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::startReferencePositionPolygonEditing);
    connect(m_positionFinishButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::finishReferencePositionRoiEditing);
    connect(m_positionTestButton, &QPushButton::clicked, this, [this]() {
        if (ReferenceImageProvider::instance().referenceImage().isNull()) {
            m_positionStatusLabel->setText(tr("请先设置基准图"));
            return;
        }
        if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::None) {
            m_positionStatusLabel->setText(tr("请先点击“完成”确认模板区域"));
            return;
        }
        if (!hasReferencePositionTemplateRoi()) {
            m_positionStatusLabel->setText(tr("请先设置模板区域"));
            return;
        }
        m_positionStatusLabel->setText(tr("位置修正后端尚未实现"));
    });

    if (m_previewHelper) {
        connect(m_previewHelper, &FrameViewHelper::roiChanged,
                this, &ReferenceImageDialog::handleReferencePositionRectChanged);
        connect(m_previewHelper, &FrameViewHelper::polygonChanged,
                this, &ReferenceImageDialog::handleReferencePositionPolygonChanged);
        connect(m_previewHelper, &FrameViewHelper::roiSelectionRejected,
                this, [this](const QRectF &) {
                    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::Rectangle)
                        m_positionStatusLabel->setText(tr("矩形 ROI 无效，请拖拽宽高至少 2 像素的区域"));
                });
        connect(m_previewHelper, &FrameViewHelper::polygonSelectionRejected,
                this, [this](int pointCount) {
                    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::Polygon) {
                        m_positionStatusLabel->setText(
                                    tr("多边形至少需要 3 个点，当前为 %1 个点").arg(pointCount));
                    }
                });
    }
}

void ReferenceImageDialog::loadPositionCorrectionConfig()
{
    m_referencePositionCorrection =
            SchemeStore::instance().currentScheme().referencePositionCorrection;
    ui->positionCorrectionCheckBox->setChecked(m_referencePositionCorrection.enabled);
    updatePositionCorrectionUi(m_referencePositionCorrection.enabled);
}

void ReferenceImageDialog::updatePositionCorrectionUi(bool enabled)
{
    m_referencePositionCorrection.enabled = enabled;
    ui->correctionExampleFrame->setVisible(!enabled);
    if (m_positionSettingsFrame)
        m_positionSettingsFrame->setVisible(enabled);
    if (m_positionRectButton)
        m_positionRectButton->setChecked(
                    m_referencePositionCorrection.templateRegionType == QStringLiteral("rectangle"));
    if (m_positionPolygonButton)
        m_positionPolygonButton->setChecked(
                    m_referencePositionCorrection.templateRegionType == QStringLiteral("polygon"));
    if (!enabled) {
        stopReferencePositionRoiEditing(false);
        if (m_previewHelper) {
            m_previewHelper->clearRoi();
            m_previewHelper->clearPolygonRoi();
        }
    } else {
        restoreReferencePositionRoi();
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(referencePositionRoiStatusText());
    }
}

// 切换到静态基准图，并启用矩形拖拽模式；已有矩形会先回显供继续调整。
void ReferenceImageDialog::startReferencePositionRectEditing()
{
    if (!m_previewHelper || ReferenceImageProvider::instance().referenceImage().isNull()) {
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("请先设置基准图"));
        return;
    }

    const bool hadRectangle =
            m_referencePositionCorrection.templateRegionType != QStringLiteral("polygon");
    showReferenceImageMode();
    m_referencePositionCorrection.templateRegionType = QStringLiteral("rectangle");
    m_positionRoiEditMode = PositionCorrectionRoiEditMode::Rectangle;
    m_positionRectButton->setChecked(true);
    m_positionPolygonButton->setChecked(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->clearPolygonRoi();
    if (hadRectangle
            && validNormalizedRect(m_referencePositionCorrection.templateRoiNormalized)) {
        m_previewHelper->setRoiRectNormalized(m_referencePositionCorrection.templateRoiNormalized);
    } else {
        m_previewHelper->clearRoi();
    }
    m_previewHelper->setRoiDrawingEnabled(true);
    ui->viewerTitleLabel->setText(tr("基准图 - 矩形模板区域"));
    m_positionStatusLabel->setText(tr("请在右侧基准图上按住左键拖拽矩形，完成后点击“完成”"));
}

// 切换到静态基准图并启用多边形逐点绘制模式，保留已确认多边形供回显。
void ReferenceImageDialog::startReferencePositionPolygonEditing()
{
    if (!m_previewHelper || ReferenceImageProvider::instance().referenceImage().isNull()) {
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("请先设置基准图"));
        return;
    }

    showReferenceImageMode();
    m_referencePositionCorrection.templateRegionType = QStringLiteral("polygon");
    m_positionRoiEditMode = PositionCorrectionRoiEditMode::Polygon;
    m_positionRectButton->setChecked(false);
    m_positionPolygonButton->setChecked(true);
    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->clearRoi();
    const QVector<QPointF> points = polygonPointsFromJson(
                m_referencePositionCorrection.templatePolygonNormalized);
    if (points.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(points);
    else
        m_previewHelper->clearPolygonRoi();
    m_previewHelper->setPolygonDrawingEnabled(true);
    ui->viewerTitleLabel->setText(tr("基准图 - 多边形模板区域"));
    m_positionStatusLabel->setText(
                tr("左键添加点，靠近首点或双击闭合，右键撤销，Esc 取消，完成后点击“完成”"));
}

// 完成当前绘制并校验 ROI；只有有效矩形或至少三个点的多边形可以确认。
bool ReferenceImageDialog::finishReferencePositionRoiEditing()
{
    if (!m_previewHelper || ReferenceImageProvider::instance().referenceImage().isNull()) {
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("请先设置基准图"));
        return false;
    }

    if (m_referencePositionCorrection.templateRegionType == QStringLiteral("polygon")) {
        if (m_previewHelper->isPolygonDrawingEnabled()
                && !m_previewHelper->finishPolygonDrawing()) {
            m_positionStatusLabel->setText(tr("多边形至少需要 3 个点"));
            return false;
        }
        const QVector<QPointF> points = m_previewHelper->polygonRoiNormalized();
        if (points.size() < 3) {
            m_positionStatusLabel->setText(tr("多边形至少需要 3 个点"));
            return false;
        }
        m_referencePositionCorrection.templatePolygonNormalized = polygonPointsToJson(points);
        m_referencePositionCorrection.templateRoiNormalized = boundingRectForPoints(points);
    } else {
        const QRectF roi = m_previewHelper->roiRectNormalized();
        if (!validNormalizedRect(roi)) {
            m_positionStatusLabel->setText(tr("请先在基准图上绘制有效矩形区域"));
            return false;
        }
        m_referencePositionCorrection.templateRoiNormalized = roi;
        m_referencePositionCorrection.templatePolygonNormalized = QJsonArray();
    }

    stopReferencePositionRoiEditing(true);
    SchemeStore::instance().setReferencePositionCorrection(m_referencePositionCorrection);
    m_positionStatusLabel->setText(
                tr("%1；请点击方案保存按钮持久化配置").arg(referencePositionRoiStatusText()));
    return true;
}

// 矩形拖拽完成后实时更新归一化配置，并清除不再适用的多边形数据。
void ReferenceImageDialog::handleReferencePositionRectChanged(const QRectF &roiNormalized)
{
    if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::Rectangle
            || !validNormalizedRect(roiNormalized)) {
        return;
    }

    m_referencePositionCorrection.templateRegionType = QStringLiteral("rectangle");
    m_referencePositionCorrection.templateRoiNormalized = roiNormalized;
    m_referencePositionCorrection.templatePolygonNormalized = QJsonArray();
    m_positionStatusLabel->setText(
                tr("矩形 ROI x=%1 y=%2 w=%3 h=%4，点击“完成”确认")
                .arg(roiNormalized.x(), 0, 'f', 3)
                .arg(roiNormalized.y(), 0, 'f', 3)
                .arg(roiNormalized.width(), 0, 'f', 3)
                .arg(roiNormalized.height(), 0, 'f', 3));
}

// 多边形闭合或编辑后实时更新点集，并同步维护其归一化外接矩形。
void ReferenceImageDialog::handleReferencePositionPolygonChanged(
        const QVector<QPointF> &pointsNormalized)
{
    const bool drawingPolygon =
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::Polygon;
    const bool adjustingConfirmedPolygon =
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::None
            && m_referencePositionCorrection.enabled
            && !m_liveCaptureMode
            && m_referencePositionCorrection.templateRegionType == QStringLiteral("polygon");
    if ((!drawingPolygon && !adjustingConfirmedPolygon) || pointsNormalized.size() < 3) {
        return;
    }

    m_referencePositionCorrection.templateRegionType = QStringLiteral("polygon");
    m_referencePositionCorrection.templatePolygonNormalized =
            polygonPointsToJson(pointsNormalized);
    m_referencePositionCorrection.templateRoiNormalized =
            boundingRectForPoints(pointsNormalized);
    m_positionStatusLabel->setText(drawingPolygon
            ? tr("多边形 ROI 已闭合，共 %1 个点，点击“完成”确认")
                  .arg(pointsNormalized.size())
            : tr("多边形 ROI 已调整，共 %1 个点，请保存方案")
                  .arg(pointsNormalized.size()));
}

// 在非编辑状态按配置类型恢复单一 ROI，避免矩形和多边形同时显示。
void ReferenceImageDialog::restoreReferencePositionRoi()
{
    if (!m_previewHelper)
        return;

    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_positionRoiEditMode = PositionCorrectionRoiEditMode::None;

    if (!m_referencePositionCorrection.enabled || m_liveCaptureMode
            || ReferenceImageProvider::instance().referenceImage().isNull()) {
        m_previewHelper->clearRoi();
        m_previewHelper->clearPolygonRoi();
        return;
    }

    if (m_referencePositionCorrection.templateRegionType == QStringLiteral("polygon")) {
        m_previewHelper->clearRoi();
        const QVector<QPointF> points = polygonPointsFromJson(
                    m_referencePositionCorrection.templatePolygonNormalized);
        if (points.size() >= 3)
            m_previewHelper->setPolygonRoiNormalized(points);
        else
            m_previewHelper->clearPolygonRoi();
    } else {
        m_previewHelper->clearPolygonRoi();
        if (validNormalizedRect(m_referencePositionCorrection.templateRoiNormalized))
            m_previewHelper->setRoiRectNormalized(m_referencePositionCorrection.templateRoiNormalized);
        else
            m_previewHelper->clearRoi();
    }
}

// 关闭公共预览控件的两种绘制模式；需要时重新显示最后确认的 ROI。
void ReferenceImageDialog::stopReferencePositionRoiEditing(bool restoreConfirmedRoi)
{
    if (!m_previewHelper)
        return;

    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_positionRoiEditMode = PositionCorrectionRoiEditMode::None;
    if (restoreConfirmedRoi)
        restoreReferencePositionRoi();
}

// 按当前模板类型检查矩形面积或多边形点数，防止测试空模板区域。
bool ReferenceImageDialog::hasReferencePositionTemplateRoi() const
{
    if (m_referencePositionCorrection.templateRegionType == QStringLiteral("polygon")) {
        return polygonPointsFromJson(
                    m_referencePositionCorrection.templatePolygonNormalized).size() >= 3;
    }
    return validNormalizedRect(m_referencePositionCorrection.templateRoiNormalized);
}

// 将归一化 ROI 转换为便于用户确认和排查配置的状态文本。
QString ReferenceImageDialog::referencePositionRoiStatusText() const
{
    if (!hasReferencePositionTemplateRoi())
        return tr("请先设置模板区域");

    const QRectF roi = m_referencePositionCorrection.templateRoiNormalized;
    if (m_referencePositionCorrection.templateRegionType == QStringLiteral("polygon")) {
        const int count = polygonPointsFromJson(
                    m_referencePositionCorrection.templatePolygonNormalized).size();
        return tr("多边形 ROI：%1 个点，外接范围 x=%2 y=%3 w=%4 h=%5")
                .arg(count)
                .arg(roi.x(), 0, 'f', 3)
                .arg(roi.y(), 0, 'f', 3)
                .arg(roi.width(), 0, 'f', 3)
                .arg(roi.height(), 0, 'f', 3);
    }
    return tr("矩形 ROI：x=%1 y=%2 w=%3 h=%4")
            .arg(roi.x(), 0, 'f', 3)
            .arg(roi.y(), 0, 'f', 3)
            .arg(roi.width(), 0, 'f', 3)
            .arg(roi.height(), 0, 'f', 3);
}

void ReferenceImageDialog::saveCurrentSchemeAs()
{
    if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::None
            && !finishReferencePositionRoiEditing()) {
        return;
    }
    SchemeStore::instance().setReferencePositionCorrection(m_referencePositionCorrection);

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("另存为"),
                                               tr("新方案名"),
                                               QLineEdit::Normal,
                                               SchemeStore::instance().currentSchemeName() + tr("_副本"),
                                               &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    QString error;
    if (!SchemeStore::instance().saveCurrentSchemeAs(name, &error)) {
        qWarning() << "[ReferenceImageDialog] 方案另存为失败:" << error;
        QMessageBox::warning(this, tr("另存为失败"), tr("方案另存为失败：%1").arg(error));
        return;
    }
    refreshSchemeHeader();
}

void ReferenceImageDialog::openCameraParamsDialog()
{
    saveCurrentScheme();
    PlanDialogUtils::replaceDialog(this, new CameraParamsDialog);
}

void ReferenceImageDialog::openToolsDialog()
{
    saveCurrentScheme();
    PlanDialogUtils::replaceDialog(this, new ToolsDialog);
}

void ReferenceImageDialog::openOutputDialog()
{
    saveCurrentScheme();
    PlanDialogUtils::replaceDialog(this, new OutputDialog);
}

void ReferenceImageDialog::showCurrentImageMode()
{
    stopReferencePositionRoiEditing(false);
    if (m_previewHelper) {
        m_previewHelper->clearRoi();
        m_previewHelper->clearPolygonRoi();
    }
    m_liveCaptureMode = true;
    updateReferenceImageControls();
    ui->viewerTitleLabel->setText(tr("当前图像"));
    ensureCameraRunning();
    refreshCurrentImage();
}

void ReferenceImageDialog::captureReferenceImage()
{
    const CameraFrameSnapshot snapshot =
            CameraFrameProvider::instance().currentFrameSnapshot();
    const cv::Mat frame = snapshot.frame;
    if (frame.empty()) {
        qWarning() << "[ReferenceImageDialog] 当前无图像，无法设置基准图。";
        ui->viewerTitleLabel->setText(tr("当前无图像"));
        if (m_previewHelper) {
            m_previewHelper->clear();
        }
        return;
    }

    QString error;
    if (!SchemeStore::instance().setReferenceFrame(frame, &error, snapshot.metadata)) {
        qWarning() << "[ReferenceImageDialog] 基准图保存失败:" << error;
        QMessageBox::warning(this, tr("基准图保存失败"), tr("基准图保存失败：%1").arg(error));
        return;
    }
    showReferenceImageMode();
    qDebug() << QString("[ReferenceImageDialog] 已抓取静态基准图: %1x%2 type=%3")
                    .arg(frame.cols)
                    .arg(frame.rows)
                    .arg(frame.type());
}

void ReferenceImageDialog::showReferenceImageMode()
{
    m_liveCaptureMode = false;
    updateReferenceImageControls();
    refreshReferenceImage();
}

void ReferenceImageDialog::importReferenceImageFromPc()
{
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          tr("PC导入基准图"),
                                                          QString(),
                                                          tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (fileName.isEmpty())
        return;

    QImage image(fileName);
    if (image.isNull()) {
        qWarning() << "[ReferenceImageDialog] 图片导入失败，无法读取:" << fileName;
        ui->viewerTitleLabel->setText(tr("图片导入失败"));
        QMessageBox::warning(this, tr("图片导入失败"), tr("无法读取所选图片。"));
        return;
    }

    const FrameInputMetadata metadata = FrameInputMetadata::fromQImage(
                image, QStringLiteral("file"));

    const QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgbFrame(rgbImage.height(),
                     rgbImage.width(),
                     CV_8UC3,
                     const_cast<uchar *>(rgbImage.constBits()),
                     static_cast<size_t>(rgbImage.bytesPerLine()));
    cv::Mat bgrFrame;
    cv::cvtColor(rgbFrame, bgrFrame, cv::COLOR_RGB2BGR);

    if (bgrFrame.empty()) {
        qWarning() << "[ReferenceImageDialog] 图片导入失败，转换为空图像:" << fileName;
        ui->viewerTitleLabel->setText(tr("图片导入失败"));
        QMessageBox::warning(this, tr("图片导入失败"), tr("图片转换失败。"));
        return;
    }

    QString error;
    if (!SchemeStore::instance().setReferenceFrame(bgrFrame, &error, metadata)) {
        qWarning() << "[ReferenceImageDialog] PC 基准图保存失败:" << error;
        QMessageBox::warning(this, tr("基准图保存失败"), tr("基准图保存失败：%1").arg(error));
        return;
    }
    showReferenceImageMode();
    qDebug() << QString("[ReferenceImageDialog] 已导入 PC 基准图: %1 size=%2x%3 type=%4")
                    .arg(fileName)
                    .arg(bgrFrame.cols)
                    .arg(bgrFrame.rows)
                    .arg(bgrFrame.type());
}

void ReferenceImageDialog::setupReferenceImageControls()
{
    m_captureImageButton = new QPushButton(tr("抓取图像"), this);
    m_captureImageButton->setObjectName(QStringLiteral("captureImageButton"));
    m_captureImageButton->setMinimumSize(150, 46);
    m_captureImageButton->setIcon(QIcon(QStringLiteral(":/icons/camera.svg")));
    m_captureImageButton->setIconSize(QSize(24, 24));
    ui->horizontalLayout_referenceButtons->addWidget(m_captureImageButton);

    m_exitCaptureButton = new QPushButton(tr("退出相机抓取"), this);
    m_exitCaptureButton->setObjectName(QStringLiteral("exitCaptureButton"));
    m_exitCaptureButton->setMinimumSize(150, 46);
    m_exitCaptureButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    m_exitCaptureButton->setIconSize(QSize(24, 24));
    ui->horizontalLayout_referenceButtons->addWidget(m_exitCaptureButton);

    updateReferenceImageControls();
}

void ReferenceImageDialog::ensureCameraRunning()
{
    CameraFrameProvider &provider = CameraFrameProvider::instance();

    if (!provider.isOpened() && !provider.openCamera(QStringLiteral("/dev/video0"))) {
        qWarning() << "[ReferenceImageDialog] 当前图像模式启动失败：相机打开失败。";
        return;
    }

    if (!provider.isGrabbing() && !provider.startGrab()) {
        qWarning() << "[ReferenceImageDialog] 当前图像模式启动失败：采集启动失败。";
    }
}

void ReferenceImageDialog::updateReferenceImageControls()
{
    ui->currentImageButton->setVisible(!m_liveCaptureMode);
    ui->historyImageButton->setVisible(!m_liveCaptureMode);
    ui->pcImportButton->setVisible(!m_liveCaptureMode);

    if (m_captureImageButton) {
        m_captureImageButton->setVisible(m_liveCaptureMode);
    }
    if (m_exitCaptureButton) {
        m_exitCaptureButton->setVisible(m_liveCaptureMode);
    }
}

void ReferenceImageDialog::refreshCurrentImage()
{
    if (!m_previewHelper) {
        return;
    }

    const QImage image = CameraFrameProvider::instance().currentImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("当前无图像"));
        return;
    }

    m_previewHelper->setImage(image);
}

void ReferenceImageDialog::refreshReferenceImage()
{
    if (!m_previewHelper) {
        return;
    }

    const QImage image = ReferenceImageProvider::instance().referenceImage();
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        return;
    }

    ui->viewerTitleLabel->setText(tr("基准图"));
    m_previewHelper->setImage(image);
    restoreReferencePositionRoi();
}
