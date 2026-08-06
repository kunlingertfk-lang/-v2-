#include "ReferenceImageDialog.h"

#include <QDebug>
#include <QButtonGroup>
#include <QComboBox>
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
#include <QSignalBlocker>
#include <QToolButton>

#include <cmath>
#include <opencv2/imgproc.hpp>

#include "algorithms/halcon/HalconRuntimePaths.h"
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

bool validNormalizedPoint(const QPointF &point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y())
            && point.x() >= 0.0 && point.x() <= 1.0
            && point.y() >= 0.0 && point.y() <= 1.0;
}

CircleRoi referenceMaskCircle(
        const ReferencePositionCorrectionConfig &config)
{
    CircleRoi circle;
    circle.centerNormalized = config.templateMaskCircleCenterNormalized;
    circle.radiusNormalized = config.templateMaskCircleRadiusNormalized;
    circle.boundingRectNormalized = QRectF(
                circle.centerNormalized.x() - circle.radiusNormalized,
                circle.centerNormalized.y() - circle.radiusNormalized,
                circle.radiusNormalized * 2.0,
                circle.radiusNormalized * 2.0);
    circle.valid = circle.radiusNormalized > 0.0 &&
            validNormalizedPoint(circle.centerNormalized) &&
            validNormalizedRect(circle.boundingRectNormalized);
    return circle;
}

void appendReferenceRegionOverlay(
        QVector<ToolOverlay> *overlays,
        const QString &type,
        const QRectF &rect,
        const QVector<QPointF> &polygon,
        const CircleRoi &circle,
        const QSize &imageSize,
        const QString &label,
        const QString &displayRole)
{
    if (!overlays || imageSize.isEmpty())
        return;
    ToolOverlay overlay;
    if (type == QStringLiteral("rectangle") && validNormalizedRect(rect)) {
        overlay.type = ToolOverlayType::Rect;
        overlay.rect = QRectF(rect.x() * imageSize.width(),
                              rect.y() * imageSize.height(),
                              rect.width() * imageSize.width(),
                              rect.height() * imageSize.height());
    } else if (type == QStringLiteral("circle") && circle.valid) {
        overlay.type = ToolOverlayType::Circle;
        overlay.center = QPointF(circle.centerNormalized.x() * imageSize.width(),
                                 circle.centerNormalized.y() * imageSize.height());
        overlay.radius = circle.radiusNormalized *
                qMax(imageSize.width(), imageSize.height());
    } else if (type == QStringLiteral("polygon") && polygon.size() >= 3) {
        overlay.type = ToolOverlayType::Polygon;
        for (const QPointF &point : polygon) {
            overlay.points.append(QPointF(point.x() * imageSize.width(),
                                          point.y() * imageSize.height()));
        }
    } else {
        return;
    }
    overlay.label = label;
    overlay.extra.insert(QStringLiteral("displayRole"), displayRole);
    overlays->append(overlay);
}

QVector<QPointF> polygonPointsFromConfig(const ReferencePositionCorrectionConfig &config)
{
    return polygonPointsFromJson(config.templatePolygonNormalized);
}

QJsonObject referencePoseJson(const QJsonObject &payload)
{
    return QJsonObject{
        {QStringLiteral("x"), payload.value(QStringLiteral("x")).toDouble()},
        {QStringLiteral("y"), payload.value(QStringLiteral("y")).toDouble()},
        {QStringLiteral("angleDeg"), payload.value(QStringLiteral("angleDeg")).toDouble()},
        {QStringLiteral("scale"), payload.value(QStringLiteral("scale")).toDouble(1.0)}
    };
}

bool validPosePayload(const QJsonObject &payload)
{
    const double x = payload.value(QStringLiteral("x")).toDouble(qQNaN());
    const double y = payload.value(QStringLiteral("y")).toDouble(qQNaN());
    const double angle = payload.value(QStringLiteral("angleDeg")).toDouble(qQNaN());
    const double scale = payload.value(QStringLiteral("scale")).toDouble(1.0);
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(angle)
            && std::isfinite(scale) && scale > 0.0;
}

} // namespace

ReferenceImageDialog::ReferenceImageDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ReferenceImageDialog)
{
    ui->setupUi(this);
    ui->setupQuickCalibrateButton->setEnabled(false);
    ui->setupQuickCalibrateButton->setProperty("quickCalibrationState", QStringLiteral("locked"));
    m_previewHelper = new FrameViewHelper(ui->previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);
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
                    clearReferencePositionMatchOverlays();
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

void ReferenceImageDialog::prepareForDisplay()
{
    refreshSchemeHeader();
    loadPositionCorrectionConfig();
    showReferenceImageMode();
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

bool ReferenceImageDialog::saveCurrentScheme()
{
    if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::None) {
        const bool maskEditing =
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskRectangle ||
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskCircle ||
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskPolygon;
        if (!(maskEditing ? finishReferencePositionMaskEditing()
                          : finishReferencePositionRoiEditing()))
            return false;
    }
    SchemeStore::instance().setReferencePositionCorrection(m_referencePositionCorrection);
    QString error;
    if (!SchemeStore::instance().saveCurrentScheme(&error)) {
        qWarning() << "[ReferenceImageDialog] 方案保存失败:" << error;
        QMessageBox::warning(this, tr("保存失败"), tr("方案保存失败：%1").arg(error));
        return false;
    }
    refreshSchemeHeader();
    if (m_referencePositionCorrection.enabled && hasReferencePositionTemplateRoi())
        m_positionStatusLabel->setText(tr("配置已保存，尚未测试"));
    return true;
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
    m_positionTestButton->setToolTip(tr("查看基准图位置修正私有模板模型的自匹配状态"));
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

    QHBoxLayout *maskTools = new QHBoxLayout;
    QLabel *maskField = new QLabel(tr("屏蔽区域"), m_positionSettingsFrame);
    maskField->setProperty("role", QStringLiteral("rowField"));
    m_positionMaskRectButton = new QPushButton(tr("矩形"), m_positionSettingsFrame);
    m_positionMaskRectButton->setObjectName(
                QStringLiteral("referencePositionMaskRectButton"));
    m_positionMaskRectButton->setCheckable(true);
    m_positionMaskCircleButton = new QPushButton(tr("圆形"), m_positionSettingsFrame);
    m_positionMaskCircleButton->setObjectName(
                QStringLiteral("referencePositionMaskCircleButton"));
    m_positionMaskCircleButton->setCheckable(true);
    m_positionMaskPolygonButton = new QPushButton(tr("多边形"), m_positionSettingsFrame);
    m_positionMaskPolygonButton->setObjectName(
                QStringLiteral("referencePositionMaskPolygonButton"));
    m_positionMaskPolygonButton->setCheckable(true);
    m_positionMaskClearButton = new QPushButton(tr("清除"), m_positionSettingsFrame);
    m_positionMaskClearButton->setObjectName(
                QStringLiteral("referencePositionMaskClearButton"));
    m_positionMaskFinishButton = new QPushButton(tr("完成"), m_positionSettingsFrame);
    m_positionMaskFinishButton->setObjectName(
                QStringLiteral("referencePositionMaskFinishButton"));
    m_positionMaskFinishButton->setProperty(
                "actionRole", QStringLiteral("primary"));
    maskTools->addWidget(maskField);
    maskTools->addStretch(1);
    maskTools->addWidget(m_positionMaskRectButton);
    maskTools->addWidget(m_positionMaskCircleButton);
    maskTools->addWidget(m_positionMaskPolygonButton);
    maskTools->addWidget(m_positionMaskClearButton);
    maskTools->addWidget(m_positionMaskFinishButton);
    settingsLayout->addLayout(maskTools);

    QHBoxLayout *originRow = new QHBoxLayout;
    QLabel *originLabel = new QLabel(tr("定位点"), m_positionSettingsFrame);
    originLabel->setProperty("role", QStringLiteral("rowField"));
    m_positionOriginModeComboBox = new QComboBox(m_positionSettingsFrame);
    m_positionOriginModeComboBox->setObjectName(
                QStringLiteral("referencePositionOriginModeComboBox"));
    m_positionOriginModeComboBox->addItem(tr("质心"), QStringLiteral("centroid"));
    m_positionOriginModeComboBox->addItem(tr("自定义点"), QStringLiteral("custom"));
    m_positionSelectOriginButton = new QPushButton(tr("选择点"), m_positionSettingsFrame);
    m_positionSelectOriginButton->setObjectName(
                QStringLiteral("referencePositionSelectOriginButton"));
    m_positionSelectOriginButton->setCheckable(true);
    m_positionOriginValueLabel = new QLabel(tr("使用模板质心"), m_positionSettingsFrame);
    m_positionOriginValueLabel->setObjectName(
                QStringLiteral("referencePositionOriginValueLabel"));
    m_positionOriginValueLabel->setProperty("hint", true);
    originRow->addWidget(originLabel);
    originRow->addStretch(1);
    originRow->addWidget(m_positionOriginModeComboBox);
    originRow->addWidget(m_positionSelectOriginButton);
    originRow->addWidget(m_positionOriginValueLabel);
    settingsLayout->addLayout(originRow);

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

    QButtonGroup *maskTypeGroup = new QButtonGroup(this);
    maskTypeGroup->setExclusive(true);
    maskTypeGroup->addButton(m_positionMaskRectButton);
    maskTypeGroup->addButton(m_positionMaskCircleButton);
    maskTypeGroup->addButton(m_positionMaskPolygonButton);

    connect(ui->positionCorrectionCheckBox, &QCheckBox::toggled,
            this, &ReferenceImageDialog::updatePositionCorrectionUi);
    connect(m_positionRectButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::startReferencePositionRectEditing);
    connect(m_positionPolygonButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::startReferencePositionPolygonEditing);
    connect(m_positionMaskRectButton, &QPushButton::clicked, this, [this]() {
        startReferencePositionMaskEditing(
                    PositionCorrectionRoiEditMode::MaskRectangle);
    });
    connect(m_positionMaskCircleButton, &QPushButton::clicked, this, [this]() {
        startReferencePositionMaskEditing(
                    PositionCorrectionRoiEditMode::MaskCircle);
    });
    connect(m_positionMaskPolygonButton, &QPushButton::clicked, this, [this]() {
        startReferencePositionMaskEditing(
                    PositionCorrectionRoiEditMode::MaskPolygon);
    });
    connect(m_positionMaskFinishButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::finishReferencePositionMaskEditing);
    connect(m_positionMaskClearButton, &QPushButton::clicked, this, [this]() {
        stopReferencePositionOriginSelection();
        stopReferencePositionRoiEditing(true);
        m_referencePositionCorrection.templateMaskRegionType =
                QStringLiteral("none");
        m_referencePositionCorrection.templateMaskRoiNormalized = QRectF();
        m_referencePositionCorrection.templateMaskPolygonNormalized =
                QJsonArray();
        m_referencePositionCorrection.templateMaskCircleCenterNormalized =
                QPointF();
        m_referencePositionCorrection.templateMaskCircleRadiusNormalized = 0.0;
        m_referencePositionCorrection.referenceCreated = false;
        m_referencePositionCorrection.referencePose = QJsonObject();
        m_referencePositionCorrection.status = QStringLiteral("editing_mask");
        m_referencePositionCorrection.message.clear();
        clearReferencePositionMatchOverlays();
        restoreReferencePositionRoi();
        renderReferencePositionOverlays();
        m_positionStatusLabel->setText(
                    tr("模板屏蔽区域已清除，请点击“测试运行”重新创建基准"));
    });
    connect(m_positionFinishButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::finishReferencePositionRoiEditing);
    connect(m_positionOriginModeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        stopReferencePositionOriginSelection();
        m_referencePositionCorrection.originMode = index == 1
                ? QStringLiteral("custom") : QStringLiteral("centroid");
        m_referencePositionCorrection.referenceCreated = false;
        m_referencePositionCorrection.referencePose = QJsonObject();
        m_referencePositionCorrection.status = QStringLiteral("editing_origin");
        m_referencePositionCorrection.message.clear();
        clearReferencePositionMatchOverlays();
        updateReferencePositionOriginControls();
        m_positionStatusLabel->setText(index == 1
                ? tr("请点击“选择点”，然后在基准图任意位置选择定位点")
                : tr("已切换为模板质心，请点击“完成”或“测试运行”重新创建基准"));
    });
    connect(m_positionSelectOriginButton, &QPushButton::clicked,
            this, [this](bool checked) {
        if (!m_previewHelper || ReferenceImageProvider::instance().referenceImage().isNull()) {
            m_positionSelectOriginButton->setChecked(false);
            m_positionStatusLabel->setText(tr("请先设置基准图"));
            return;
        }
        stopReferencePositionRoiEditing(true);
        m_positionSelectOriginButton->setChecked(checked);
        m_previewHelper->setPointSelectionEnabled(checked);
        ui->viewerTitleLabel->setText(checked
                ? tr("点击基准图选择自定义定位点") : tr("基准图"));
    });
    connect(m_positionTestButton, &QPushButton::clicked, this, [this]() {
        if (ReferenceImageProvider::instance().referenceImage().isNull()) {
            m_positionStatusLabel->setText(tr("请先设置基准图"));
            return;
        }
        if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::None) {
            m_positionStatusLabel->setText(tr("请先点击“完成”确认当前区域"));
            return;
        }
        if (!hasReferencePositionTemplateRoi()) {
            m_positionStatusLabel->setText(tr("请先设置模板区域"));
            return;
        }
        stopReferencePositionOriginSelection();
        if (buildAndValidateReferencePositionModel())
            SchemeStore::instance().setReferencePositionCorrection(m_referencePositionCorrection);
    });

    if (m_previewHelper) {
        connect(m_previewHelper, &FrameViewHelper::roiChanged,
                this, &ReferenceImageDialog::handleReferencePositionRectChanged);
        connect(m_previewHelper, &FrameViewHelper::polygonChanged,
                this, &ReferenceImageDialog::handleReferencePositionPolygonChanged);
        connect(m_previewHelper, &FrameViewHelper::circleChanged,
                this, &ReferenceImageDialog::handleReferencePositionCircleChanged);
        connect(m_previewHelper, &FrameViewHelper::pointSelected,
                this, [this](const QPointF &point) {
            if (m_referencePositionCorrection.originMode != QStringLiteral("custom")
                    || !validNormalizedPoint(point)) {
                return;
            }
            m_referencePositionCorrection.customOriginNormalized = point;
            m_referencePositionCorrection.referenceCreated = false;
            m_referencePositionCorrection.referencePose = QJsonObject();
            m_referencePositionCorrection.status = QStringLiteral("editing_origin");
            m_referencePositionCorrection.message.clear();
            clearReferencePositionMatchOverlays();
            updateReferencePositionOriginControls();
            m_positionStatusLabel->setText(
                        tr("自定义定位点 X=%1 Y=%2，可继续点击调整；完成后点击“测试运行”")
                        .arg(point.x(), 0, 'f', 4)
                        .arg(point.y(), 0, 'f', 4));
        });
        connect(m_previewHelper, &FrameViewHelper::roiSelectionRejected,
                this, [this](const QRectF &) {
                    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::Rectangle)
                        m_positionStatusLabel->setText(tr("矩形 ROI 无效，请拖拽宽高至少 2 像素的区域"));
                });
        connect(m_previewHelper, &FrameViewHelper::polygonSelectionRejected,
                this, [this](int pointCount) {
                    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::Polygon ||
                            m_positionRoiEditMode ==
                            PositionCorrectionRoiEditMode::MaskPolygon) {
                        m_positionStatusLabel->setText(
                                    tr("多边形至少需要 3 个点，当前为 %1 个点").arg(pointCount));
                    }
                });
        connect(m_previewHelper, &FrameViewHelper::circleSelectionRejected,
                this, [this]() {
                    if (m_positionRoiEditMode ==
                            PositionCorrectionRoiEditMode::MaskCircle) {
                        m_positionStatusLabel->setText(
                                    tr("圆形屏蔽区域半径至少需要 2 像素"));
                    }
                });
    }
}

void ReferenceImageDialog::loadPositionCorrectionConfig()
{
    m_referencePositionCorrection =
            SchemeStore::instance().currentScheme().referencePositionCorrection;
    if (m_positionOriginModeComboBox) {
        const QSignalBlocker blocker(m_positionOriginModeComboBox);
        m_positionOriginModeComboBox->setCurrentIndex(
                    m_referencePositionCorrection.originMode == QStringLiteral("custom") ? 1 : 0);
    }
    updateReferencePositionOriginControls();
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
    if (m_positionMaskRectButton)
        m_positionMaskRectButton->setChecked(
                    m_referencePositionCorrection.templateMaskRegionType ==
                    QStringLiteral("rectangle"));
    if (m_positionMaskCircleButton)
        m_positionMaskCircleButton->setChecked(
                    m_referencePositionCorrection.templateMaskRegionType ==
                    QStringLiteral("circle"));
    if (m_positionMaskPolygonButton)
        m_positionMaskPolygonButton->setChecked(
                    m_referencePositionCorrection.templateMaskRegionType ==
                    QStringLiteral("polygon"));
    if (m_positionMaskClearButton)
        m_positionMaskClearButton->setEnabled(
                    m_referencePositionCorrection.templateMaskRegionType !=
                    QStringLiteral("none"));
    updateReferencePositionOriginControls();
    if (!enabled) {
        stopReferencePositionOriginSelection();
        stopReferencePositionRoiEditing(false);
        clearReferencePositionMatchOverlays();
        if (m_previewHelper) {
            m_previewHelper->clearRoi();
            m_previewHelper->clearPolygonRoi();
            m_previewHelper->clearCircleRoi();
        }
    } else {
        restoreReferencePositionRoi();
        renderReferencePositionOverlays();
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
    stopReferencePositionOriginSelection();
    clearReferencePositionMatchOverlays();
    m_referencePositionCorrection.templateRegionType = QStringLiteral("rectangle");
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.status = QStringLiteral("editing");
    showReferenceImageMode();
    m_positionRoiEditMode = PositionCorrectionRoiEditMode::Rectangle;
    m_positionRectButton->setChecked(true);
    m_positionPolygonButton->setChecked(false);
    m_positionMaskRectButton->setChecked(false);
    m_positionMaskCircleButton->setChecked(false);
    m_positionMaskPolygonButton->setChecked(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->clearPolygonRoi();
    m_previewHelper->clearCircleRoi();
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

    stopReferencePositionOriginSelection();
    clearReferencePositionMatchOverlays();
    m_referencePositionCorrection.templateRegionType = QStringLiteral("polygon");
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.status = QStringLiteral("editing");
    showReferenceImageMode();
    m_positionRoiEditMode = PositionCorrectionRoiEditMode::Polygon;
    m_positionRectButton->setChecked(false);
    m_positionPolygonButton->setChecked(true);
    m_positionMaskRectButton->setChecked(false);
    m_positionMaskCircleButton->setChecked(false);
    m_positionMaskPolygonButton->setChecked(false);
    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->clearRoi();
    m_previewHelper->clearCircleRoi();
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

void ReferenceImageDialog::startReferencePositionMaskEditing(
        PositionCorrectionRoiEditMode mode)
{
    if (!m_previewHelper || ReferenceImageProvider::instance().referenceImage().isNull()) {
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("请先设置基准图"));
        return;
    }
    if (!hasReferencePositionTemplateRoi()) {
        m_positionStatusLabel->setText(tr("请先设置并确认模板区域"));
        return;
    }
    if (mode != PositionCorrectionRoiEditMode::MaskRectangle &&
            mode != PositionCorrectionRoiEditMode::MaskCircle &&
            mode != PositionCorrectionRoiEditMode::MaskPolygon) {
        return;
    }

    stopReferencePositionOriginSelection();
    clearReferencePositionMatchOverlays();
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.status = QStringLiteral("editing_mask");
    m_referencePositionCorrection.message.clear();
    showReferenceImageMode();
    m_positionRoiEditMode = mode;

    m_positionRectButton->setChecked(false);
    m_positionPolygonButton->setChecked(false);
    m_positionMaskRectButton->setChecked(
                mode == PositionCorrectionRoiEditMode::MaskRectangle);
    m_positionMaskCircleButton->setChecked(
                mode == PositionCorrectionRoiEditMode::MaskCircle);
    m_positionMaskPolygonButton->setChecked(
                mode == PositionCorrectionRoiEditMode::MaskPolygon);

    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->clearRoi();
    m_previewHelper->clearPolygonRoi();
    m_previewHelper->clearCircleRoi();
    renderReferencePositionOverlays();

    if (mode == PositionCorrectionRoiEditMode::MaskRectangle) {
        if (m_referencePositionCorrection.templateMaskRegionType ==
                QStringLiteral("rectangle") &&
                validNormalizedRect(
                    m_referencePositionCorrection.templateMaskRoiNormalized)) {
            m_previewHelper->setRoiRectNormalized(
                        m_referencePositionCorrection.templateMaskRoiNormalized);
        }
        m_previewHelper->setRoiDrawingEnabled(true);
        ui->viewerTitleLabel->setText(tr("基准图 - 矩形模板屏蔽区域"));
        m_positionStatusLabel->setText(
                    tr("拖拽矩形屏蔽不稳定特征，完成后点击屏蔽区域一行的“完成”"));
    } else if (mode == PositionCorrectionRoiEditMode::MaskCircle) {
        const CircleRoi circle =
                referenceMaskCircle(m_referencePositionCorrection);
        if (m_referencePositionCorrection.templateMaskRegionType ==
                QStringLiteral("circle") && circle.valid) {
            m_previewHelper->setCircleRoiNormalized(circle);
        }
        m_previewHelper->setCircleDrawingEnabled(true);
        ui->viewerTitleLabel->setText(tr("基准图 - 圆形模板屏蔽区域"));
        m_positionStatusLabel->setText(
                    tr("拖拽圆形屏蔽不稳定特征，完成后点击屏蔽区域一行的“完成”"));
    } else {
        const QVector<QPointF> points = polygonPointsFromJson(
                    m_referencePositionCorrection.templateMaskPolygonNormalized);
        if (m_referencePositionCorrection.templateMaskRegionType ==
                QStringLiteral("polygon") && points.size() >= 3) {
            m_previewHelper->setPolygonRoiNormalized(points);
        }
        m_previewHelper->setPolygonDrawingEnabled(true);
        ui->viewerTitleLabel->setText(tr("基准图 - 多边形模板屏蔽区域"));
        m_positionStatusLabel->setText(
                    tr("逐点绘制屏蔽区域，闭合后点击屏蔽区域一行的“完成”"));
    }
}

bool ReferenceImageDialog::finishReferencePositionMaskEditing()
{
    if (!m_previewHelper || ReferenceImageProvider::instance().referenceImage().isNull()) {
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("请先设置基准图"));
        return false;
    }

    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskRectangle) {
        const QRectF roi = m_previewHelper->roiRectNormalized();
        if (!validNormalizedRect(roi)) {
            m_positionStatusLabel->setText(tr("请先绘制有效矩形屏蔽区域"));
            return false;
        }
        m_referencePositionCorrection.templateMaskRegionType =
                QStringLiteral("rectangle");
        m_referencePositionCorrection.templateMaskRoiNormalized = roi;
        m_referencePositionCorrection.templateMaskPolygonNormalized = QJsonArray();
        m_referencePositionCorrection.templateMaskCircleCenterNormalized =
                QPointF();
        m_referencePositionCorrection.templateMaskCircleRadiusNormalized = 0.0;
    } else if (m_positionRoiEditMode ==
               PositionCorrectionRoiEditMode::MaskCircle) {
        const CircleRoi circle = m_previewHelper->circleRoiNormalized();
        if (!circle.valid) {
            m_positionStatusLabel->setText(tr("请先绘制有效圆形屏蔽区域"));
            return false;
        }
        m_referencePositionCorrection.templateMaskRegionType =
                QStringLiteral("circle");
        m_referencePositionCorrection.templateMaskRoiNormalized =
                circle.boundingRectNormalized;
        m_referencePositionCorrection.templateMaskPolygonNormalized = QJsonArray();
        m_referencePositionCorrection.templateMaskCircleCenterNormalized =
                circle.centerNormalized;
        m_referencePositionCorrection.templateMaskCircleRadiusNormalized =
                circle.radiusNormalized;
    } else if (m_positionRoiEditMode ==
               PositionCorrectionRoiEditMode::MaskPolygon) {
        if (m_previewHelper->isPolygonDrawingEnabled() &&
                !m_previewHelper->finishPolygonDrawing()) {
            m_positionStatusLabel->setText(tr("屏蔽多边形至少需要 3 个点"));
            return false;
        }
        const QVector<QPointF> points =
                m_previewHelper->polygonRoiNormalized();
        if (points.size() < 3) {
            m_positionStatusLabel->setText(tr("屏蔽多边形至少需要 3 个点"));
            return false;
        }
        m_referencePositionCorrection.templateMaskRegionType =
                QStringLiteral("polygon");
        m_referencePositionCorrection.templateMaskRoiNormalized =
                boundingRectForPoints(points);
        m_referencePositionCorrection.templateMaskPolygonNormalized =
                polygonPointsToJson(points);
        m_referencePositionCorrection.templateMaskCircleCenterNormalized =
                QPointF();
        m_referencePositionCorrection.templateMaskCircleRadiusNormalized = 0.0;
    } else {
        m_positionStatusLabel->setText(tr("请先选择一种屏蔽区域形状"));
        return false;
    }

    m_referencePositionCorrection.version = 3;
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.status = QStringLiteral("pending_validation");
    m_referencePositionCorrection.message.clear();
    stopReferencePositionRoiEditing(true);
    if (!buildAndValidateReferencePositionModel())
        return false;
    SchemeStore::instance().setReferencePositionCorrection(
                m_referencePositionCorrection);
    m_positionMaskClearButton->setEnabled(true);
    m_positionStatusLabel->setText(
                tr("模板屏蔽区域已确认，模型自匹配通过，请保存方案"));
    return true;
}

// 完成当前绘制并校验 ROI；只有有效矩形或至少三个点的多边形可以确认。
bool ReferenceImageDialog::finishReferencePositionRoiEditing()
{
    if (!m_previewHelper || ReferenceImageProvider::instance().referenceImage().isNull()) {
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("请先设置基准图"));
        return false;
    }
    stopReferencePositionOriginSelection();
    if (m_positionRoiEditMode ==
            PositionCorrectionRoiEditMode::MaskRectangle ||
            m_positionRoiEditMode ==
            PositionCorrectionRoiEditMode::MaskCircle ||
            m_positionRoiEditMode ==
            PositionCorrectionRoiEditMode::MaskPolygon) {
        return finishReferencePositionMaskEditing();
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

    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.status = QStringLiteral("pending_validation");
    m_referencePositionCorrection.message.clear();
    stopReferencePositionRoiEditing(true);
    if (!buildAndValidateReferencePositionModel())
        return false;
    SchemeStore::instance().setReferencePositionCorrection(m_referencePositionCorrection);
    m_positionStatusLabel->setText(
                tr("%1；模型自匹配通过，请点击方案保存按钮持久化配置")
                .arg(referencePositionRoiStatusText()));
    return true;
}

// 矩形拖拽完成后实时更新归一化配置，并清除不再适用的多边形数据。
void ReferenceImageDialog::handleReferencePositionRectChanged(const QRectF &roiNormalized)
{
    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskRectangle &&
            validNormalizedRect(roiNormalized)) {
        m_referencePositionCorrection.templateMaskRegionType =
                QStringLiteral("rectangle");
        m_referencePositionCorrection.templateMaskRoiNormalized = roiNormalized;
        m_referencePositionCorrection.templateMaskPolygonNormalized = QJsonArray();
        m_referencePositionCorrection.templateMaskCircleCenterNormalized =
                QPointF();
        m_referencePositionCorrection.templateMaskCircleRadiusNormalized = 0.0;
        m_referencePositionCorrection.referenceCreated = false;
        m_referencePositionCorrection.referencePose = QJsonObject();
        m_referencePositionCorrection.status = QStringLiteral("editing_mask");
        clearReferencePositionMatchOverlays();
        m_positionStatusLabel->setText(
                    tr("矩形屏蔽区 x=%1 y=%2 w=%3 h=%4，点击屏蔽区域“完成”")
                    .arg(roiNormalized.x(), 0, 'f', 3)
                    .arg(roiNormalized.y(), 0, 'f', 3)
                    .arg(roiNormalized.width(), 0, 'f', 3)
                    .arg(roiNormalized.height(), 0, 'f', 3));
        return;
    }
    if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::Rectangle
            || !validNormalizedRect(roiNormalized)) {
        return;
    }

    m_referencePositionCorrection.templateRegionType = QStringLiteral("rectangle");
    m_referencePositionCorrection.templateRoiNormalized = roiNormalized;
    m_referencePositionCorrection.templatePolygonNormalized = QJsonArray();
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.status = QStringLiteral("editing");
    clearReferencePositionMatchOverlays();
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
    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskPolygon &&
            pointsNormalized.size() >= 3) {
        m_referencePositionCorrection.templateMaskRegionType =
                QStringLiteral("polygon");
        m_referencePositionCorrection.templateMaskPolygonNormalized =
                polygonPointsToJson(pointsNormalized);
        m_referencePositionCorrection.templateMaskRoiNormalized =
                boundingRectForPoints(pointsNormalized);
        m_referencePositionCorrection.templateMaskCircleCenterNormalized =
                QPointF();
        m_referencePositionCorrection.templateMaskCircleRadiusNormalized = 0.0;
        m_referencePositionCorrection.referenceCreated = false;
        m_referencePositionCorrection.referencePose = QJsonObject();
        m_referencePositionCorrection.status = QStringLiteral("editing_mask");
        clearReferencePositionMatchOverlays();
        m_positionStatusLabel->setText(
                    tr("屏蔽多边形已闭合，共 %1 个点，点击屏蔽区域“完成”")
                    .arg(pointsNormalized.size()));
        return;
    }
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
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.status = QStringLiteral("editing");
    clearReferencePositionMatchOverlays();
    m_positionStatusLabel->setText(drawingPolygon
            ? tr("多边形 ROI 已闭合，共 %1 个点，点击“完成”确认")
                  .arg(pointsNormalized.size())
            : tr("多边形 ROI 已调整，共 %1 个点，请保存方案")
                  .arg(pointsNormalized.size()));
}

void ReferenceImageDialog::handleReferencePositionCircleChanged(
        const CircleRoi &circle)
{
    if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::MaskCircle ||
            !circle.valid) {
        return;
    }
    m_referencePositionCorrection.templateMaskRegionType =
            QStringLiteral("circle");
    m_referencePositionCorrection.templateMaskRoiNormalized =
            circle.boundingRectNormalized;
    m_referencePositionCorrection.templateMaskPolygonNormalized = QJsonArray();
    m_referencePositionCorrection.templateMaskCircleCenterNormalized =
            circle.centerNormalized;
    m_referencePositionCorrection.templateMaskCircleRadiusNormalized =
            circle.radiusNormalized;
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.status = QStringLiteral("editing_mask");
    clearReferencePositionMatchOverlays();
    m_positionStatusLabel->setText(
                tr("圆形屏蔽区中心 X=%1 Y=%2 半径=%3，点击屏蔽区域“完成”")
                .arg(circle.centerNormalized.x(), 0, 'f', 3)
                .arg(circle.centerNormalized.y(), 0, 'f', 3)
                .arg(circle.radiusNormalized, 0, 'f', 3));
}

// 在非编辑状态按配置类型恢复单一 ROI，避免矩形和多边形同时显示。
void ReferenceImageDialog::restoreReferencePositionRoi()
{
    if (!m_previewHelper)
        return;

    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    m_positionRoiEditMode = PositionCorrectionRoiEditMode::None;

    if (!m_referencePositionCorrection.enabled || m_liveCaptureMode
            || ReferenceImageProvider::instance().referenceImage().isNull()) {
        m_previewHelper->clearRoi();
        m_previewHelper->clearPolygonRoi();
        m_previewHelper->clearCircleRoi();
        return;
    }
    m_previewHelper->clearCircleRoi();

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
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->clearCircleRoi();
    m_positionRoiEditMode = PositionCorrectionRoiEditMode::None;
    if (restoreConfirmedRoi) {
        if (m_positionMaskRectButton)
            m_positionMaskRectButton->setChecked(
                        m_referencePositionCorrection.templateMaskRegionType ==
                        QStringLiteral("rectangle"));
        if (m_positionMaskCircleButton)
            m_positionMaskCircleButton->setChecked(
                        m_referencePositionCorrection.templateMaskRegionType ==
                        QStringLiteral("circle"));
        if (m_positionMaskPolygonButton)
            m_positionMaskPolygonButton->setChecked(
                        m_referencePositionCorrection.templateMaskRegionType ==
                        QStringLiteral("polygon"));
        restoreReferencePositionRoi();
        renderReferencePositionOverlays();
    }
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

bool ReferenceImageDialog::buildAndValidateReferencePositionModel()
{
    const ReferenceFrameSnapshot snapshot =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    if (snapshot.frame.empty()) {
        clearReferencePositionMatchOverlays();
        m_referencePositionCorrection.referenceCreated = false;
        m_referencePositionCorrection.referencePose = QJsonObject();
        m_referencePositionCorrection.status = QStringLiteral("no_reference_image");
        m_referencePositionCorrection.message = tr("请先设置基准图");
        m_positionStatusLabel->setText(m_referencePositionCorrection.message);
        return false;
    }
    if (!hasReferencePositionTemplateRoi()) {
        clearReferencePositionMatchOverlays();
        m_referencePositionCorrection.referenceCreated = false;
        m_referencePositionCorrection.referencePose = QJsonObject();
        m_referencePositionCorrection.status = QStringLiteral("no_template_region");
        m_referencePositionCorrection.message = tr("请先设置模板区域");
        m_positionStatusLabel->setText(m_referencePositionCorrection.message);
        return false;
    }
    if (m_referencePositionCorrection.originMode != QStringLiteral("centroid")
            && m_referencePositionCorrection.originMode != QStringLiteral("custom")) {
        clearReferencePositionMatchOverlays();
        m_referencePositionCorrection.referenceCreated = false;
        m_referencePositionCorrection.referencePose = QJsonObject();
        m_referencePositionCorrection.status = QStringLiteral("invalid_custom_origin");
        m_referencePositionCorrection.message = tr("定位点模式无效");
        m_positionStatusLabel->setText(m_referencePositionCorrection.message);
        return false;
    }
    if (m_referencePositionCorrection.originMode == QStringLiteral("custom")
            && !validNormalizedPoint(
                m_referencePositionCorrection.customOriginNormalized)) {
        clearReferencePositionMatchOverlays();
        m_referencePositionCorrection.referenceCreated = false;
        m_referencePositionCorrection.referencePose = QJsonObject();
        m_referencePositionCorrection.status = QStringLiteral("invalid_custom_origin");
        m_referencePositionCorrection.message = tr("请先选择有效的自定义定位点");
        m_positionStatusLabel->setText(m_referencePositionCorrection.message);
        return false;
    }
    clearReferencePositionMatchOverlays();

    TemplateLocationHalconConfig config;
    config.toolId = PositionCorrection::defaultSourceId();
    config.modelCacheKey = QStringLiteral("reference.positionCorrection.private_template");
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(), &config.halconSoPathCandidates);
    config.templateRegionType = m_referencePositionCorrection.templateRegionType;
    config.templateRoiNormalized = m_referencePositionCorrection.templateRoiNormalized;
    config.templatePolygonNormalized = polygonPointsFromConfig(m_referencePositionCorrection);
    config.templateMaskRegionType =
            m_referencePositionCorrection.templateMaskRegionType;
    config.templateMaskRoiNormalized =
            m_referencePositionCorrection.templateMaskRoiNormalized;
    config.templateMaskPolygonNormalized = polygonPointsFromJson(
                m_referencePositionCorrection.templateMaskPolygonNormalized);
    config.templateMaskCircleCenterNormalized =
            m_referencePositionCorrection.templateMaskCircleCenterNormalized;
    config.templateMaskCircleRadiusNormalized =
            m_referencePositionCorrection.templateMaskCircleRadiusNormalized;
    config.searchRegionType = QStringLiteral("full");
    config.searchRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    config.minScore = 50;
    config.angleStart = -45;
    config.angleExtent = 90;
    config.scaleMin = 100;
    config.scaleMax = 100;
    config.maxMatches = 1;
    config.minMatchCount = 1;
    config.maxMatchCount = 1;
    config.maxOverlap = 0.5;
    config.originMode = m_referencePositionCorrection.originMode;
    config.customOriginNormalized =
            m_referencePositionCorrection.customOriginNormalized;
    config.timeoutMs = 2000;

    const TemplateLocationHalconResult result =
            m_referencePositionRunner.run(snapshot.frame, snapshot.frame, config);
    m_referencePositionCorrection.modelCacheKey = config.modelCacheKey;
    m_referencePositionCorrection.status = result.status;
    m_referencePositionCorrection.message = result.message;
    m_referencePositionCorrection.score = result.score;
    m_referencePositionCorrection.elapsedMs = result.elapsedMs;
    if (!result.success || !result.ok || !validPosePayload(result.payload)) {
        clearReferencePositionMatchOverlays();
        m_referencePositionCorrection.referenceCreated = false;
        m_referencePositionCorrection.referencePose = QJsonObject();
        m_positionStatusLabel->setText(tr("基准图位置修正模型创建失败：%1")
                                       .arg(result.message.trimmed().isEmpty()
                                            ? result.status
                                            : result.message));
        return false;
    }

    m_referencePositionCorrection.referenceCreated = true;
    m_referencePositionCorrection.referencePose = referencePoseJson(result.payload);
    showReferencePositionMatchOverlays(result.overlays);
    m_positionStatusLabel->setText(tr("基准图位置修正模型可用：X=%1，Y=%2，角度=%3°，分数=%4")
                                   .arg(m_referencePositionCorrection.referencePose.value(QStringLiteral("x")).toDouble(), 0, 'f', 3)
                                   .arg(m_referencePositionCorrection.referencePose.value(QStringLiteral("y")).toDouble(), 0, 'f', 3)
                                   .arg(m_referencePositionCorrection.referencePose.value(QStringLiteral("angleDeg")).toDouble(), 0, 'f', 3)
                                   .arg(result.score * 100.0, 0, 'f', 1));
    return true;
}

void ReferenceImageDialog::showReferencePositionMatchOverlays(
        const QVector<ToolOverlay> &overlays)
{
    m_referencePositionMatchOverlays.clear();
    for (const ToolOverlay &overlay : overlays) {
        if (overlay.extra.value(QStringLiteral("matchIndex")).toInt(-1) != 0)
            continue;
        if (overlay.label == QStringLiteral("match_result")
                || overlay.label == QStringLiteral("match_center")) {
            m_referencePositionMatchOverlays.append(overlay);
        }
    }

    if (!m_previewHelper)
        return;
    renderReferencePositionOverlays();
}

void ReferenceImageDialog::clearReferencePositionMatchOverlays()
{
    m_referencePositionMatchOverlays.clear();
    renderReferencePositionOverlays();
}

void ReferenceImageDialog::updateReferencePositionOriginControls()
{
    const bool custom =
            m_referencePositionCorrection.originMode == QStringLiteral("custom");
    if (m_positionSelectOriginButton)
        m_positionSelectOriginButton->setVisible(custom);
    if (m_positionOriginValueLabel) {
        m_positionOriginValueLabel->setText(custom
                ? tr("X %1  Y %2")
                  .arg(m_referencePositionCorrection.customOriginNormalized.x(), 0, 'f', 4)
                  .arg(m_referencePositionCorrection.customOriginNormalized.y(), 0, 'f', 4)
                : tr("使用模板质心"));
    }
}

void ReferenceImageDialog::stopReferencePositionOriginSelection()
{
    if (m_previewHelper)
        m_previewHelper->setPointSelectionEnabled(false);
    if (m_positionSelectOriginButton)
        m_positionSelectOriginButton->setChecked(false);
    if (!m_liveCaptureMode && ui && ui->viewerTitleLabel)
        ui->viewerTitleLabel->setText(tr("基准图"));
}

void ReferenceImageDialog::renderReferencePositionOverlays()
{
    if (!m_previewHelper)
        return;
    if (!m_referencePositionCorrection.enabled || m_liveCaptureMode
            || ReferenceImageProvider::instance().referenceImage().isNull()) {
        m_previewHelper->clearToolOverlays();
        return;
    }

    QVector<ToolOverlay> overlays;
    const QImage image = ReferenceImageProvider::instance().referenceImage();
    const bool editingMask =
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskRectangle ||
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskCircle ||
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskPolygon;
    if (editingMask) {
        appendReferenceRegionOverlay(
                    &overlays,
                    m_referencePositionCorrection.templateRegionType,
                    m_referencePositionCorrection.templateRoiNormalized,
                    polygonPointsFromJson(
                        m_referencePositionCorrection.templatePolygonNormalized),
                    CircleRoi(),
                    image.size(),
                    QStringLiteral("template_roi"),
                    QStringLiteral("color_template_roi"));
    } else {
        appendReferenceRegionOverlay(
                    &overlays,
                    m_referencePositionCorrection.templateMaskRegionType,
                    m_referencePositionCorrection.templateMaskRoiNormalized,
                    polygonPointsFromJson(
                        m_referencePositionCorrection.templateMaskPolygonNormalized),
                    referenceMaskCircle(m_referencePositionCorrection),
                    image.size(),
                    QStringLiteral("template_mask"),
                    QStringLiteral("color_template_mask"));
    }
    if (m_referencePositionCorrection.originMode == QStringLiteral("custom")
            && validNormalizedPoint(
                m_referencePositionCorrection.customOriginNormalized)) {
        const QPointF point(
                    m_referencePositionCorrection.customOriginNormalized.x() * image.width(),
                    m_referencePositionCorrection.customOriginNormalized.y() * image.height());
        const qreal radius = 12.0;
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
    overlays += m_referencePositionMatchOverlays;

    if (overlays.isEmpty())
        m_previewHelper->clearToolOverlays();
    else
        m_previewHelper->setToolOverlays(overlays);
}

void ReferenceImageDialog::saveCurrentSchemeAs()
{
    if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::None) {
        const bool maskEditing =
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskRectangle ||
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskCircle ||
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskPolygon;
        if (!(maskEditing ? finishReferencePositionMaskEditing()
                          : finishReferencePositionRoiEditing()))
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
    if (!saveCurrentScheme())
        return;
    if (!PlanDialogUtils::switchEmbeddedSetupPage(this, QStringLiteral("camera")))
        qWarning() << "[ReferenceImageDialog] 未找到方案编辑宿主窗口";
}

void ReferenceImageDialog::openToolsDialog()
{
    if (!saveCurrentScheme())
        return;
    if (!PlanDialogUtils::switchEmbeddedSetupPage(this, QStringLiteral("tools")))
        qWarning() << "[ReferenceImageDialog] 未找到方案编辑宿主窗口";
}

void ReferenceImageDialog::openOutputDialog()
{
    if (!saveCurrentScheme())
        return;
    if (!PlanDialogUtils::switchEmbeddedSetupPage(this, QStringLiteral("output")))
        qWarning() << "[ReferenceImageDialog] 未找到方案编辑宿主窗口";
}

void ReferenceImageDialog::showCurrentImageMode()
{
    stopReferencePositionOriginSelection();
    stopReferencePositionRoiEditing(false);
    m_liveCaptureMode = true;
    clearReferencePositionMatchOverlays();
    if (m_previewHelper) {
        m_previewHelper->clearRoi();
        m_previewHelper->clearPolygonRoi();
    }
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
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.customOriginNormalized = QPointF(0.5, 0.5);
    m_referencePositionCorrection.status = QStringLiteral("reference_image_changed");
    m_referencePositionCorrection.message = tr("基准图已更新，请重新确认模板区域和定位点");
    updateReferencePositionOriginControls();
    clearReferencePositionMatchOverlays();
    SchemeStore::instance().setReferencePositionCorrection(m_referencePositionCorrection);
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
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.customOriginNormalized = QPointF(0.5, 0.5);
    m_referencePositionCorrection.status = QStringLiteral("reference_image_changed");
    m_referencePositionCorrection.message = tr("基准图已更新，请重新确认模板区域和定位点");
    updateReferencePositionOriginControls();
    clearReferencePositionMatchOverlays();
    SchemeStore::instance().setReferencePositionCorrection(m_referencePositionCorrection);
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
    m_previewHelper->clearToolOverlays();
    m_previewHelper->setImage(image);
    restoreReferencePositionRoi();
    if (!m_referencePositionMatchOverlays.isEmpty()) {
        renderReferencePositionOverlays();
    } else if (m_referencePositionCorrection.enabled
               && m_referencePositionCorrection.referenceCreated
               && m_positionRoiEditMode == PositionCorrectionRoiEditMode::None
               && hasReferencePositionTemplateRoi()) {
        buildAndValidateReferencePositionModel();
    } else {
        renderReferencePositionOverlays();
    }
}
