#include "ReferenceImageDialog.h"

#include <QDebug>
#include <QButtonGroup>
#include <QBrush>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QListView>
#include <QMap>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QSize>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QToolButton>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>

#include "algorithms/location/ReferenceTemplateLocationConfig.h"
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

constexpr int kReferenceTemplateIdRole = Qt::UserRole + 21;
constexpr int kReferenceBaseIdRole = Qt::UserRole + 22;

class ItemWidgetOnlyDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *, const QStyleOptionViewItem &,
               const QModelIndex &) const override
    {
        // Template tabs are fully rendered by QListWidget::setItemWidget().
        // Keep the underlying item roles for persistence/tests without also
        // painting the legacy text and check indicator underneath the tab.
    }
};

bool loadImageFileAsBgr(const QString &fileName,
                        cv::Mat *frame,
                        FrameInputMetadata *metadata,
                        QString *message)
{
    if (!frame || !metadata)
        return false;
    const QImage image(fileName);
    if (image.isNull()) {
        if (message)
            *message = QObject::tr("无法读取所选图片");
        return false;
    }
    *metadata = FrameInputMetadata::fromQImage(
                image, QStringLiteral("file"));
    const QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgbFrame(rgbImage.height(), rgbImage.width(), CV_8UC3,
                     const_cast<uchar *>(rgbImage.constBits()),
                     static_cast<size_t>(rgbImage.bytesPerLine()));
    cv::cvtColor(rgbFrame, *frame, cv::COLOR_RGB2BGR);
    if (frame->empty()) {
        if (message)
            *message = QObject::tr("图片转换失败");
        return false;
    }
    return true;
}

QFrame *parameterCard(QWidget *parent, const QString &objectName,
                      const QString &title, QFormLayout **form)
{
    auto *card = new QFrame(parent);
    card->setObjectName(objectName);
    card->setProperty("panelRole", QStringLiteral("configCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setProperty("role", QStringLiteral("cardTitle"));
    layout->addWidget(titleLabel);
    auto *createdForm = new QFormLayout;
    createdForm->setContentsMargins(0, 0, 0, 0);
    createdForm->setHorizontalSpacing(12);
    createdForm->setVerticalSpacing(8);
    createdForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addLayout(createdForm);
    if (form)
        *form = createdForm;
    return card;
}

QSpinBox *integerField(QWidget *parent, const QString &objectName,
                       int minimum, int maximum, int value,
                       const QString &suffix = QString())
{
    auto *field = new QSpinBox(parent);
    field->setObjectName(objectName);
    field->setRange(minimum, maximum);
    field->setValue(value);
    field->setSuffix(suffix);
    return field;
}

QDoubleSpinBox *realField(QWidget *parent, const QString &objectName,
                          double minimum, double maximum, double value,
                          double step, int decimals = 2,
                          const QString &suffix = QString())
{
    auto *field = new QDoubleSpinBox(parent);
    field->setObjectName(objectName);
    field->setRange(minimum, maximum);
    field->setDecimals(decimals);
    field->setSingleStep(step);
    field->setValue(value);
    field->setSuffix(suffix);
    return field;
}

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

QRectF boundsForTemplate(const TemplateLocationTemplateConfig &item)
{
    QRectF bounds;
    const auto appendRect = [&bounds](const QRectF &candidate) {
        if (!candidate.isValid() || candidate.width() <= 0.0 ||
                candidate.height() <= 0.0)
            return;
        bounds = bounds.isEmpty() ? candidate.normalized()
                                  : bounds.united(candidate.normalized());
    };
    if (!item.includeRegions.isEmpty()) {
        for (const TemplateLocationRegionConfig &region : item.includeRegions) {
            if (region.regionType == QStringLiteral("polygon"))
                appendRect(boundingRectForPoints(region.polygonNormalized));
            else if (region.regionType == QStringLiteral("circle")) {
                appendRect(QRectF(
                    region.circleCenterNormalized.x() -
                        region.circleRadiusNormalized,
                    region.circleCenterNormalized.y() -
                        region.circleRadiusNormalized,
                    region.circleRadiusNormalized * 2.0,
                    region.circleRadiusNormalized * 2.0));
            } else {
                appendRect(region.roiNormalized);
            }
        }
    } else if (item.templateRegionType == QStringLiteral("polygon")) {
        appendRect(boundingRectForPoints(item.templatePolygonNormalized));
    } else {
        appendRect(item.templateRoiNormalized);
    }
    return bounds.intersected(QRectF(0.0, 0.0, 1.0, 1.0));
}

QPointF centroidForTemplate(const TemplateLocationTemplateConfig &item)
{
    if (item.templateRegionType != QStringLiteral("polygon") ||
            item.templatePolygonNormalized.size() < 3) {
        return item.templateRoiNormalized.center();
    }
    const QVector<QPointF> &points = item.templatePolygonNormalized;
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
        return item.templateRoiNormalized.center();
    return QPointF(weightedX / (3.0 * twiceArea),
                   weightedY / (3.0 * twiceArea));
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

QJsonObject referencePoseJson(const QJsonObject &payload)
{
    const QJsonObject primary = payload.value(QStringLiteral("pose")).toObject();
    const QJsonObject &source = primary.isEmpty() ? payload : primary;
    QJsonObject pose{
        {QStringLiteral("x"), source.value(QStringLiteral("x")).toDouble()},
        {QStringLiteral("y"), source.value(QStringLiteral("y")).toDouble()},
        {QStringLiteral("angleDeg"), source.value(QStringLiteral("angleDeg")).toDouble()},
        {QStringLiteral("scale"), source.value(QStringLiteral("scale")).toDouble(1.0)}
    };
    // The marker makes the identity contract atomic: old poses without it stay
    // compatible, while a new pose may not silently lose one identity field.
    pose.insert(QStringLiteral("locatorIdentityVersion"), 1);
    QString modelSignature = source.value(
                QStringLiteral("templateModelSignature")).toString().trimmed();
    if (modelSignature.isEmpty())
        modelSignature = payload.value(
                    QStringLiteral("modelSignature")).toString().trimmed();
    QString templateId = source.value(
                QStringLiteral("templateId")).toString().trimmed();
    if (templateId.isEmpty())
        templateId = payload.value(
                    QStringLiteral("selectedTemplateId")).toString().trimmed();
    pose.insert(QStringLiteral("locatorModelSignature"), modelSignature);
    pose.insert(QStringLiteral("locatorTemplateId"), templateId);
    QString baseId = source.value(
                QStringLiteral("selectedBaseId")).toString().trimmed();
    if (baseId.isEmpty())
        baseId = payload.value(
                    QStringLiteral("selectedBaseId")).toString().trimmed();
    if (baseId.isEmpty())
        baseId = payload.value(
                    QStringLiteral("sourceBaseId")).toString().trimmed();
    if (!baseId.isEmpty())
        pose.insert(QStringLiteral("locatorBaseId"), baseId);
    const QString originMode = payload.value(
                QStringLiteral("originMode")).toString().trimmed();
    pose.insert(QStringLiteral("locatorOriginMode"), originMode);
    const QJsonValue customOrigin = payload.value(
                QStringLiteral("customOriginNormalized"));
    if (originMode == QStringLiteral("custom") && customOrigin.isObject()) {
        pose.insert(QStringLiteral("locatorCustomOriginNormalized"),
                    customOrigin.toObject());
    }
    return pose;
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

bool validLocatorIdentityPayload(const QJsonObject &payload)
{
    if (payload.value(QStringLiteral("modelSignature"))
            .toString().trimmed().isEmpty() ||
            payload.value(QStringLiteral("selectedTemplateId"))
            .toString().trimmed().isEmpty()) {
        return false;
    }
    const QString originMode = payload.value(
                QStringLiteral("originMode")).toString().trimmed();
    if (originMode != QStringLiteral("centroid") &&
            originMode != QStringLiteral("custom")) {
        return false;
    }
    if (originMode == QStringLiteral("custom")) {
        const QJsonObject customOrigin = payload.value(
                    QStringLiteral("customOriginNormalized")).toObject();
        return validNormalizedPoint(
                    QPointF(customOrigin.value(QStringLiteral("x"))
                            .toDouble(qQNaN()),
                            customOrigin.value(QStringLiteral("y"))
                            .toDouble(qQNaN())));
    }
    return true;
}

double shortestAngleDifferenceDeg(double left, double right)
{
    double difference = std::fmod(std::abs(left - right), 360.0);
    if (difference > 180.0)
        difference = 360.0 - difference;
    return difference;
}

bool validateReferencePoseConsistency(
        const TemplateLocationModelBankConfig &bank,
        const QJsonObject &posesByTemplateId,
        QString *templateName,
        QString *message)
{
    QMap<QString, const TemplateLocationTemplateConfig *> anchorItems;
    QMap<QString, QJsonObject> anchorPoses;
    for (const TemplateLocationTemplateConfig &item : bank.templates) {
        if (!item.enabled)
            continue;
        const QString groupId = bank.version ==
                TemplateLocationConfig::CompositeBankParamsVersion
                ? item.sourceBaseId : QStringLiteral("legacy-primary");
        const QJsonObject pose = posesByTemplateId.value(
                    item.templateId).toObject();
        if (!anchorItems.contains(groupId)) {
            anchorItems.insert(groupId, &item);
            anchorPoses.insert(groupId, pose);
            continue;
        }
        const TemplateLocationTemplateConfig *anchorItem =
                anchorItems.value(groupId);
        const QJsonObject anchorPose = anchorPoses.value(groupId);
        const double dx = pose.value(QStringLiteral("x")).toDouble() -
                anchorPose.value(QStringLiteral("x")).toDouble();
        const double dy = pose.value(QStringLiteral("y")).toDouble() -
                anchorPose.value(QStringLiteral("y")).toDouble();
        const double positionDelta = std::hypot(dx, dy);
        const double angleDelta = shortestAngleDifferenceDeg(
                    pose.value(QStringLiteral("angleDeg")).toDouble(),
                    anchorPose.value(QStringLiteral("angleDeg")).toDouble());
        const double scaleDelta = std::abs(
                    pose.value(QStringLiteral("scale")).toDouble(1.0) -
                    anchorPose.value(QStringLiteral("scale")).toDouble(1.0));
        if (positionDelta > bank.fusion.positionTolerancePx ||
                angleDelta > bank.fusion.angleToleranceDeg ||
                scaleDelta > bank.fusion.scaleTolerance) {
            if (templateName)
                *templateName = item.name;
            if (message) {
                *message = QObject::tr(
                            "与同一 Base 的模板“%1”公共位姿不一致：位置差 %2 px、角度差 %3°、尺度差 %4")
                        .arg(anchorItem ? anchorItem->name : QString())
                        .arg(positionDelta, 0, 'f', 2)
                        .arg(angleDelta, 0, 'f', 2)
                        .arg(scaleDelta, 0, 'f', 3);
            }
            return false;
        }
    }
    if (templateName)
        templateName->clear();
    if (message)
        message->clear();
    return true;
}

} // namespace

bool ReferenceTemplateBankValidation::consistentFrozenPoses(
        const TemplateLocationModelBankConfig &bank,
        const QJsonObject &posesByTemplateId,
        QString *templateName,
        QString *message)
{
    return validateReferencePoseConsistency(
                bank, posesByTemplateId, templateName, message);
}

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
    setupParameterModeControls();
    setupReferenceImageControls();
    setupReferenceTemplateBankControls();
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
            [this](const QImage &) {
                if (!m_liveCaptureMode && m_previewHelper) {
                    clearReferencePositionMatchOverlays();
                    refreshReferenceImage();
                }
            });
    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameSetChanged,
            this, [this]() {
        refreshReferenceAssetControls();
        if (!m_liveCaptureMode)
            refreshReferenceImage();
    });

    QString error;
    if (!SchemeStore::instance().ensureLoaded(&error)) {
        qWarning() << "[ReferenceImageDialog] 方案加载失败:" << error;
    } else if (!SchemeStore::instance().loadCurrentReferenceIntoProvider(&error)
               && !SchemeStore::instance().currentScheme().referenceImagePath.isEmpty()) {
        qWarning() << "[ReferenceImageDialog]" << error;
    }
    refreshSchemeHeader();
    refreshReferenceAssetControls();
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
    refreshReferenceAssetControls();
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

void ReferenceImageDialog::setupParameterModeControls()
{
    m_parameterModeGroup = new QButtonGroup(this);
    m_parameterModeGroup->setExclusive(true);
    m_parameterModeGroup->addButton(ui->basicModeButton, 0);
    m_parameterModeGroup->addButton(ui->allModeButton, 1);
    ui->basicModeButton->setAutoExclusive(false);
    ui->allModeButton->setAutoExclusive(false);
    connect(ui->basicModeButton, &QPushButton::clicked,
            this, [this]() { setAdvancedVisible(false); });
    connect(ui->allModeButton, &QPushButton::clicked,
            this, [this]() { setAdvancedVisible(true); });
    setAdvancedVisible(false);
}

void ReferenceImageDialog::setAdvancedVisible(bool visible)
{
    if (!visible && (m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskRectangle ||
                     m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskCircle ||
                     m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskPolygon ||
                     m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchRectangle ||
                     m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchCircle ||
                     m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchPolygon)) {
        stopReferencePositionRoiEditing(true);
    }
    ui->basicModeButton->setChecked(!visible);
    ui->allModeButton->setChecked(visible);
    ui->basicModeButton->setProperty("parameterModeState",
                                     visible ? "inactive" : "active");
    ui->allModeButton->setProperty("parameterModeState",
                                   visible ? "active" : "inactive");
    ui->basicModeButton->style()->unpolish(ui->basicModeButton);
    ui->basicModeButton->style()->polish(ui->basicModeButton);
    ui->allModeButton->style()->unpolish(ui->allModeButton);
    ui->allModeButton->style()->polish(ui->allModeButton);
    const bool locatorVisible = !m_referencePositionEditorReadOnly;
    if (m_basicMatchingCard)
        m_basicMatchingCard->setVisible(locatorVisible);
    if (m_advancedMatchingCard)
        m_advancedMatchingCard->setVisible(locatorVisible && visible);
    if (m_templateMaskToolsWidget)
        m_templateMaskToolsWidget->setVisible(locatorVisible && visible);
}

void ReferenceImageDialog::connectNavigation()
{
    connect(ui->cameraStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openCameraParamsDialog);
    connect(ui->toolsStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openToolsDialog);
    connect(ui->outputStepButton, &QToolButton::clicked, this, &ReferenceImageDialog::openOutputDialog);
    connect(ui->previousButton, &QPushButton::clicked, this, &ReferenceImageDialog::openCameraParamsDialog);
    connect(ui->nextButton, &QPushButton::clicked, this, &ReferenceImageDialog::openToolsDialog);
    connect(ui->currentImageButton, &QPushButton::clicked, this, [this]() {
        const ReferenceAssetSet &assets =
                SchemeStore::instance().currentScheme().referenceAssets;
        m_captureCreatesNewBase = assets.multiBaseEnabled()
                && assets.size() < ReferenceAssetSet::kMaximumAssets;
        showCurrentImageMode();
    });
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
        const bool searchEditing =
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchRectangle ||
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchCircle ||
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchPolygon;
        if (!(maskEditing ? finishReferencePositionMaskEditing()
                          : searchEditing ? finishReferenceSearchRegionEditing()
                                          : finishReferencePositionRoiEditing()))
            return false;
    }
    if (!m_referencePositionEditorReadOnly)
        writeLocatorControls();
    SchemeStore::instance().setReferencePositionCorrection(m_referencePositionCorrection);
    QString error;
    if (!SchemeStore::instance().saveCurrentScheme(&error)) {
        qWarning() << "[ReferenceImageDialog] 方案保存失败:" << error;
        // SchemeStore has restored its persisted state.  Mirror that rollback
        // immediately so a later navigation action cannot resubmit the failed
        // candidate kept in this dialog.
        loadPositionCorrectionConfig();
        refreshReferenceAssetControls();
        refreshSchemeHeader();
        QMessageBox::warning(this, tr("保存失败"), tr("方案保存失败：%1").arg(error));
        return false;
    }
    refreshSchemeHeader();
    if (m_referencePositionEditorReadOnly)
        m_positionStatusLabel->setText(referencePositionReadOnlyMessage());
    else if (m_referencePositionCorrection.referenceCreated)
        m_positionStatusLabel->setText(tr("基准定位已保存且可用"));
    else if (m_referencePositionCorrection.enabled && hasReferencePositionTemplateRoi())
        m_positionStatusLabel->setText(tr("配置已保存，尚未测试"));
    return true;
}

void ReferenceImageDialog::setupReferenceTemplateBankControls()
{
    QFrame *card = ui->referenceTemplateBankCard;
    auto *layout = card ? qobject_cast<QVBoxLayout *>(card->layout()) : nullptr;
    if (!layout)
        return;
    ui->referenceTemplateBankCardHeaderFrame->hide();
    card->setProperty("card", false);
    card->setProperty("templateTabStrip", true);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_referenceTemplateBankList = new QListWidget(card);
    m_referenceTemplateBankList->setObjectName(
                QStringLiteral("referenceTemplateBankListWidget"));
    m_referenceTemplateBankList->setSelectionMode(
                QAbstractItemView::SingleSelection);
    m_referenceTemplateBankList->setFlow(QListView::LeftToRight);
    m_referenceTemplateBankList->setWrapping(false);
    m_referenceTemplateBankList->setMovement(QListView::Static);
    m_referenceTemplateBankList->setResizeMode(QListView::Adjust);
    m_referenceTemplateBankList->setVerticalScrollBarPolicy(
                Qt::ScrollBarAlwaysOff);
    m_referenceTemplateBankList->setHorizontalScrollBarPolicy(
                Qt::ScrollBarAsNeeded);
    m_referenceTemplateBankList->setMinimumHeight(46);
    m_referenceTemplateBankList->setMaximumHeight(46);
    m_referenceTemplateBankList->setItemDelegate(
                new ItemWidgetOnlyDelegate(m_referenceTemplateBankList));

    m_addReferenceTemplateButton = new QPushButton(tr("+"), card);
    m_addReferenceTemplateButton->setObjectName(
                QStringLiteral("addReferenceTemplateButton"));
    m_addReferenceTemplateButton->setToolTip(tr("添加模板（最多 8 个）"));
    m_addReferenceTemplateButton->setFixedSize(46, 46);
    auto *tabRow = new QHBoxLayout;
    tabRow->setContentsMargins(0, 0, 0, 0);
    tabRow->setSpacing(0);
    tabRow->addWidget(m_referenceTemplateBankList, 1);
    tabRow->addWidget(m_addReferenceTemplateButton);
    layout->addLayout(tabRow);

    auto *sourceRow = new QHBoxLayout;
    sourceRow->setContentsMargins(0, 0, 0, 0);
    sourceRow->setSpacing(8);
    auto *sourceLabel = new QLabel(tr("模板来源 Base"), card);
    sourceLabel->setProperty("role", QStringLiteral("rowField"));
    m_templateSourceBaseComboBox = new QComboBox(card);
    m_templateSourceBaseComboBox->setObjectName(
                QStringLiteral("referenceTemplateSourceBaseComboBox"));
    m_templateSourceBaseComboBox->setSizePolicy(
                QSizePolicy::Expanding, QSizePolicy::Fixed);
    sourceRow->addWidget(sourceLabel);
    sourceRow->addWidget(m_templateSourceBaseComboBox, 1);
    layout->addLayout(sourceRow);

    m_referenceTemplateBankSummaryLabel = new QLabel(card);
    m_referenceTemplateBankSummaryLabel->setObjectName(
                QStringLiteral("referenceTemplateBankSummaryLabel"));
    m_referenceTemplateBankSummaryLabel->setProperty("hint", true);
    m_referenceTemplateBankSummaryLabel->hide();
    layout->addWidget(m_referenceTemplateBankSummaryLabel);

    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(6);
    m_renameReferenceTemplateButton = new QPushButton(tr("重命名"), card);
    m_renameReferenceTemplateButton->setObjectName(
                QStringLiteral("renameReferenceTemplateButton"));
    m_deleteReferenceTemplateButton = new QPushButton(tr("删除模板"), card);
    m_deleteReferenceTemplateButton->setObjectName(
                QStringLiteral("deleteReferenceTemplateButton"));
    buttons->addWidget(m_renameReferenceTemplateButton);
    buttons->addWidget(m_deleteReferenceTemplateButton);
    m_renameReferenceTemplateButton->hide();
    m_deleteReferenceTemplateButton->hide();
    layout->addLayout(buttons);

    connect(m_addReferenceTemplateButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::addReferenceTemplate);
    connect(m_renameReferenceTemplateButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::renameReferenceTemplate);
    connect(m_deleteReferenceTemplateButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::deleteReferenceTemplate);
    connect(m_referenceTemplateBankList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current, QListWidgetItem *) {
        if (!m_updatingTemplateBank && current)
            switchActiveTemplate(current->data(kReferenceTemplateIdRole).toString());
    });
    connect(m_referenceTemplateBankList, &QListWidget::itemChanged,
            this, &ReferenceImageDialog::handleReferenceTemplateItemChanged);
    connect(m_templateSourceBaseComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        if (m_updatingTemplateBank || m_loadingLocatorControls ||
                m_referencePositionEditorReadOnly)
            return;
        const int index = activeTemplateIndex();
        const QString baseId = m_templateSourceBaseComboBox
                ->currentData().toString().trimmed();
        if (index < 0 || baseId.isEmpty())
            return;
        if (m_locatorConfig.version !=
                TemplateLocationConfig::CompositeBankParamsVersion) {
            synchronizeLocatorBaseBindings(true);
        }
        TemplateLocationTemplateConfig &item =
                m_locatorConfig.templates[index];
        if (item.sourceBaseId == baseId)
            return;
        storeLegacyGeometryToActiveTemplate();
        storeLegacyBaseBindingGeometry();
        int nextOrder = 0;
        for (const TemplateLocationTemplateConfig &candidate :
             qAsConst(m_locatorConfig.templates)) {
            if (candidate.templateId != item.templateId &&
                    candidate.sourceBaseId == baseId) {
                nextOrder = qMax(nextOrder, candidate.order + 1);
            }
        }
        item.sourceBaseId = baseId;
        item.order = nextOrder;
        item.priority = nextOrder;
        item.modelCreated = false;
        // This is the only path that repairs an unavailable v6 source Base:
        // the user has explicitly selected a concrete asset.  Do not run the
        // generic v6 ID normalizer here because it would also fill missing
        // sourceBaseId values on unrelated templates.
        synchronizeLocatorBaseBindings(false);
        loadActiveBaseBindingGeometry();
        selectReferenceBase(baseId);
        markLocatorDirty(false);
        refreshReferenceTemplateBank();
    });
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
    m_positionTestButton = new QPushButton(
                tr("创建/更新基准"), m_positionSettingsFrame);
    m_positionTestButton->setObjectName(QStringLiteral("referencePositionTestButton"));
    m_positionTestButton->setToolTip(
                tr("逐个创建所有启用模板并在各自局部区域完成自匹配，全部成功后更新基准"));
    header->addWidget(title);
    header->addStretch(1);
    header->addWidget(m_positionTestButton);
    settingsLayout->addLayout(header);
    // The template bank is an editor tab strip, not a separate top-level
    // card. Moving the existing widget preserves all object names and test
    // hooks while matching the compact “template tab + add” interaction.
    if (ui->referenceTemplateBankCard)
        settingsLayout->addWidget(ui->referenceTemplateBankCard);

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
    m_templateMaskToolsWidget = new QWidget(m_positionSettingsFrame);
    m_templateMaskToolsWidget->setObjectName(
                QStringLiteral("referenceTemplateMaskToolsWidget"));
    auto *maskToolsContainerLayout = new QHBoxLayout(m_templateMaskToolsWidget);
    maskToolsContainerLayout->setContentsMargins(0, 0, 0, 0);
    maskToolsContainerLayout->setSpacing(0);
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
    maskToolsContainerLayout->addLayout(maskTools);
    settingsLayout->addWidget(m_templateMaskToolsWidget);

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

    setupReferenceMatchingParameterControls(settingsLayout);

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
        m_referenceTemplateExcludeEdited = true;
        markActiveTemplateDirty();
        clearReferencePositionMatchOverlays();
        restoreReferencePositionRoi();
        renderReferencePositionOverlays();
        m_positionMaskClearButton->setEnabled(
                    m_referencePositionCorrection.templateMaskRegionType !=
                    QStringLiteral("none"));
        m_positionStatusLabel->setText(
                    tr("模板屏蔽区域已清除，请点击“创建/更新基准”重新验证"));
    });
    connect(m_positionFinishButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::finishReferencePositionRoiEditing);
    connect(m_positionOriginModeComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        stopReferencePositionOriginSelection();
        m_referencePositionCorrection.originMode = index == 1
                ? QStringLiteral("custom") : QStringLiteral("centroid");
        markLocatorDirty(false);
        clearReferencePositionMatchOverlays();
        updateReferencePositionOriginControls();
        m_positionStatusLabel->setText(index == 1
                ? tr("请点击“选择点”，然后在基准图任意位置选择定位点")
                : tr("已切换为模板质心，请点击“创建/更新基准”重新验证"));
    });
    connect(m_positionSelectOriginButton, &QPushButton::clicked,
            this, [this](bool checked) {
        if (!showActiveTemplateBase() || !m_previewHelper) {
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
        if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::None) {
            m_positionStatusLabel->setText(tr("请先点击“完成”确认当前区域"));
            return;
        }
        stopReferencePositionOriginSelection();
        // “创建/更新基准”只验证并更新本对话框草稿。主运行态必须等到
        // 用户点击标题栏“保存/另存为”后再由 saveCurrentScheme* 提交。
        buildAndValidateReferencePositionModel();
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
            markLocatorDirty(false);
            clearReferencePositionMatchOverlays();
            updateReferencePositionOriginControls();
            m_positionStatusLabel->setText(
                        tr("自定义定位点 X=%1 Y=%2，可继续点击调整；完成后点击“创建/更新基准”")
                        .arg(point.x(), 0, 'f', 4)
                        .arg(point.y(), 0, 'f', 4));
        });
        connect(m_previewHelper, &FrameViewHelper::roiSelectionRejected,
                this, [this](const QRectF &) {
                    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::Rectangle ||
                            m_positionRoiEditMode ==
                            PositionCorrectionRoiEditMode::SearchRectangle)
                        m_positionStatusLabel->setText(tr("矩形 ROI 无效，请拖拽宽高至少 2 像素的区域"));
                });
        connect(m_previewHelper, &FrameViewHelper::polygonSelectionRejected,
                this, [this](int pointCount) {
                    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::Polygon ||
                            m_positionRoiEditMode ==
                            PositionCorrectionRoiEditMode::MaskPolygon ||
                            m_positionRoiEditMode ==
                            PositionCorrectionRoiEditMode::SearchPolygon) {
                        m_positionStatusLabel->setText(
                                    tr("多边形至少需要 3 个点，当前为 %1 个点").arg(pointCount));
                    }
                });
        connect(m_previewHelper, &FrameViewHelper::circleSelectionRejected,
                this, [this]() {
                    if (m_positionRoiEditMode ==
                            PositionCorrectionRoiEditMode::MaskCircle ||
                            m_positionRoiEditMode ==
                            PositionCorrectionRoiEditMode::SearchCircle) {
                        m_positionStatusLabel->setText(
                                    tr("圆形屏蔽区域半径至少需要 2 像素"));
                    }
        });
    }
    setAdvancedVisible(ui->allModeButton->isChecked());
}

void ReferenceImageDialog::setupReferenceMatchingParameterControls(
        QVBoxLayout *settingsLayout)
{
    if (!settingsLayout)
        return;

    QFormLayout *basicForm = nullptr;
    m_basicMatchingCard = parameterCard(
                m_positionSettingsFrame,
                QStringLiteral("referenceBasicMatchingCard"),
                tr("基础匹配参数"), &basicForm);
    m_minScoreSpinBox = integerField(m_basicMatchingCard,
                                     QStringLiteral("referenceMinScoreSpinBox"),
                                     0, 100, 50, QStringLiteral("%"));
    m_angleMinSpinBox = integerField(m_basicMatchingCard,
                                     QStringLiteral("referenceAngleMinSpinBox"),
                                     -180, 180, -45, QStringLiteral("°"));
    m_angleMaxSpinBox = integerField(m_basicMatchingCard,
                                     QStringLiteral("referenceAngleMaxSpinBox"),
                                     -180, 180, 45, QStringLiteral("°"));
    m_scaleMinSpinBox = integerField(m_basicMatchingCard,
                                     QStringLiteral("referenceScaleMinSpinBox"),
                                     10, 200, 100, QStringLiteral("%"));
    m_scaleMaxSpinBox = integerField(m_basicMatchingCard,
                                     QStringLiteral("referenceScaleMaxSpinBox"),
                                     10, 200, 100, QStringLiteral("%"));
    basicForm->addRow(tr("最低得分"), m_minScoreSpinBox);
    basicForm->addRow(tr("最小角度"), m_angleMinSpinBox);
    basicForm->addRow(tr("最大角度"), m_angleMaxSpinBox);
    basicForm->addRow(tr("最小尺度"), m_scaleMinSpinBox);
    basicForm->addRow(tr("最大尺度"), m_scaleMaxSpinBox);
    auto *fixedOutputLabel = new QLabel(
                tr("固定输出 1 个定位姿态"), m_basicMatchingCard);
    fixedOutputLabel->setObjectName(
                QStringLiteral("referenceFixedOutputCountLabel"));
    fixedOutputLabel->setProperty("hint", true);
    basicForm->addRow(tr("输出数量"), fixedOutputLabel);
    settingsLayout->addWidget(m_basicMatchingCard);

    QFormLayout *advancedForm = nullptr;
    m_advancedMatchingCard = parameterCard(
                m_positionSettingsFrame,
                QStringLiteral("referenceAdvancedMatchingCard"),
                tr("全部匹配参数"), &advancedForm);
    m_advancedMatchingForm = advancedForm;
    m_searchRegionToolsWidget = new QWidget(m_advancedMatchingCard);
    m_searchRegionToolsWidget->setObjectName(
                QStringLiteral("referenceSearchRegionToolsWidget"));
    auto *searchTools = new QHBoxLayout(m_searchRegionToolsWidget);
    searchTools->setContentsMargins(0, 0, 0, 0);
    searchTools->setSpacing(4);
    m_searchFullButton = new QPushButton(tr("全图"), m_searchRegionToolsWidget);
    m_searchRectButton = new QPushButton(tr("矩形"), m_searchRegionToolsWidget);
    m_searchCircleButton = new QPushButton(tr("圆形"), m_searchRegionToolsWidget);
    m_searchPolygonButton = new QPushButton(tr("多边形"), m_searchRegionToolsWidget);
    m_searchFinishButton = new QPushButton(tr("完成"), m_searchRegionToolsWidget);
    const QList<QPushButton *> searchButtons{
        m_searchFullButton, m_searchRectButton,
        m_searchCircleButton, m_searchPolygonButton
    };
    auto *searchGroup = new QButtonGroup(m_searchRegionToolsWidget);
    searchGroup->setExclusive(true);
    for (QPushButton *button : searchButtons) {
        button->setCheckable(true);
        button->setProperty("actionRole", QStringLiteral("secondary"));
        searchGroup->addButton(button);
        searchTools->addWidget(button);
    }
    m_searchFinishButton->setProperty("actionRole", QStringLiteral("highlight"));
    searchTools->addWidget(m_searchFinishButton);
    advancedForm->addRow(tr("搜索区域"), m_searchRegionToolsWidget);
    m_templatePrioritySpinBox = integerField(
                m_advancedMatchingCard,
                QStringLiteral("referenceTemplatePrioritySpinBox"), 0, 7, 0);
    m_templateIndependentParametersCheckBox = new QCheckBox(
                tr("当前模板独立使用全部匹配参数"),
                m_advancedMatchingCard);
    m_templateIndependentParametersCheckBox->setObjectName(
                QStringLiteral("referenceTemplateIndependentParametersCheckBox"));
    m_polarityComboBox = new QComboBox(m_advancedMatchingCard);
    m_polarityComboBox->setObjectName(
                QStringLiteral("referencePolarityComboBox"));
    m_polarityComboBox->addItem(tr("考虑极性"),
                                QStringLiteral("use_polarity"));
    m_polarityComboBox->addItem(tr("忽略局部极性"),
                                QStringLiteral("ignore_local_polarity"));
    m_polarityComboBox->addItem(tr("忽略全局极性"),
                                QStringLiteral("ignore_global_polarity"));
    m_contrastModeComboBox = new QComboBox(m_advancedMatchingCard);
    m_contrastModeComboBox->setObjectName(
                QStringLiteral("referenceContrastModeComboBox"));
    m_contrastModeComboBox->addItem(tr("自动"), QStringLiteral("auto"));
    m_contrastModeComboBox->addItem(tr("手动"), QStringLiteral("manual"));
    m_contrastSpinBox = integerField(
                m_advancedMatchingCard,
                QStringLiteral("referenceContrastSpinBox"), 2, 255, 40);
    m_minContrastSpinBox = integerField(
                m_advancedMatchingCard,
                QStringLiteral("referenceMinContrastSpinBox"), 1, 254, 10);
    m_numLevelsSpinBox = integerField(
                m_advancedMatchingCard,
                QStringLiteral("referenceNumLevelsSpinBox"), 0, 10, 0);
    m_numLevelsSpinBox->setSpecialValueText(tr("自动"));
    m_numLevelsSpinBox->setToolTip(
                tr("HALCON NumLevels；0 表示自动。它不是最小链长，也不等同于独立“特征尺度”参数。"));
    m_subPixelComboBox = new QComboBox(m_advancedMatchingCard);
    m_subPixelComboBox->setObjectName(
                QStringLiteral("referenceSubPixelComboBox"));
    m_subPixelComboBox->addItem(QStringLiteral("least_squares"));
    m_subPixelComboBox->addItem(QStringLiteral("none"));
    m_greedinessSpinBox = realField(
                m_advancedMatchingCard,
                QStringLiteral("referenceGreedinessSpinBox"),
                0.0, 1.0, 0.5, 0.1);
    m_timeoutSpinBox = integerField(
                m_advancedMatchingCard,
                QStringLiteral("referenceTimeoutSpinBox"),
                0, 60000, 2000, QStringLiteral(" ms"));
    m_timeoutSpinBox->setSingleStep(100);
    m_maxOverlapSpinBox = integerField(
                m_advancedMatchingCard,
                QStringLiteral("referenceMaxOverlapSpinBox"),
                0, 100, 50, QStringLiteral("%"));
    m_primaryStrategyComboBox = new QComboBox(m_advancedMatchingCard);
    m_primaryStrategyComboBox->setObjectName(
                QStringLiteral("referencePrimaryStrategyComboBox"));
    m_primaryStrategyComboBox->addItem(tr("首个有效模板"),
                                       QStringLiteral("first_valid"));
    m_primaryStrategyComboBox->addItem(tr("最佳得分"),
                                       QStringLiteral("best_score"));
    m_primaryStrategyComboBox->addItem(tr("模板优先级"),
                                       QStringLiteral("template_priority"));
    m_primaryStrategyComboBox->addItem(tr("锁定模板"),
                                       QStringLiteral("locked_template"));
    m_lockedTemplateComboBox = new QComboBox(m_advancedMatchingCard);
    m_lockedTemplateComboBox->setObjectName(
                QStringLiteral("referenceLockedTemplateComboBox"));
    m_fusionEnabledCheckBox = new QCheckBox(tr("启用跨模板去重"),
                                            m_advancedMatchingCard);
    m_fusionEnabledCheckBox->setObjectName(
                QStringLiteral("referenceFusionEnabledCheckBox"));
    m_fusionPositionToleranceSpinBox = realField(
                m_advancedMatchingCard,
                QStringLiteral("referenceFusionPositionToleranceSpinBox"),
                0.0, 1000.0, 5.0, 0.5, 1, QStringLiteral(" px"));
    m_fusionAngleToleranceSpinBox = realField(
                m_advancedMatchingCard,
                QStringLiteral("referenceFusionAngleToleranceSpinBox"),
                0.0, 180.0, 2.0, 0.5, 1, QStringLiteral("°"));
    m_fusionScaleToleranceSpinBox = realField(
                m_advancedMatchingCard,
                QStringLiteral("referenceFusionScaleToleranceSpinBox"),
                0.0, 1.0, 0.05, 0.01, 2);
    advancedForm->addRow(tr("当前模板优先级"), m_templatePrioritySpinBox);
    advancedForm->addRow(QString(),
                         m_templateIndependentParametersCheckBox);
    advancedForm->addRow(tr("极性"), m_polarityComboBox);
    advancedForm->addRow(tr("对比度模式"), m_contrastModeComboBox);
    advancedForm->addRow(tr("Contrast"), m_contrastSpinBox);
    advancedForm->addRow(tr("MinContrast"), m_minContrastSpinBox);
    advancedForm->addRow(tr("金字塔层级"), m_numLevelsSpinBox);
    advancedForm->addRow(tr("亚像素"), m_subPixelComboBox);
    advancedForm->addRow(tr("贪婪度"), m_greedinessSpinBox);
    advancedForm->addRow(tr("超时"), m_timeoutSpinBox);
    advancedForm->addRow(tr("最大重叠率"), m_maxOverlapSpinBox);
    advancedForm->addRow(tr("主结果策略"), m_primaryStrategyComboBox);
    advancedForm->addRow(tr("锁定模板"), m_lockedTemplateComboBox);
    advancedForm->addRow(QString(), m_fusionEnabledCheckBox);
    advancedForm->addRow(tr("融合位置容差"),
                         m_fusionPositionToleranceSpinBox);
    advancedForm->addRow(tr("融合角度容差"),
                         m_fusionAngleToleranceSpinBox);
    advancedForm->addRow(tr("融合尺度容差"),
                         m_fusionScaleToleranceSpinBox);
    settingsLayout->addWidget(m_advancedMatchingCard);

    const QList<QSpinBox *> modelIntegerFields{
        m_angleMinSpinBox, m_angleMaxSpinBox, m_scaleMinSpinBox,
        m_scaleMaxSpinBox, m_contrastSpinBox, m_minContrastSpinBox,
        m_numLevelsSpinBox
    };
    for (QSpinBox *field : modelIntegerFields) {
        connect(field, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this]() { markLocatorDirty(true); });
    }
    const QList<QSpinBox *> validationIntegerFields{
        m_minScoreSpinBox, m_timeoutSpinBox, m_maxOverlapSpinBox
    };
    for (QSpinBox *field : validationIntegerFields) {
        connect(field, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this]() { markLocatorDirty(false); });
    }
    const QList<QDoubleSpinBox *> realFields{
        m_greedinessSpinBox, m_fusionPositionToleranceSpinBox,
        m_fusionAngleToleranceSpinBox, m_fusionScaleToleranceSpinBox
    };
    for (QDoubleSpinBox *field : realFields) {
        connect(field, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this]() { markLocatorDirty(false); });
    }
    const QList<QComboBox *> modelComboFields{
        m_polarityComboBox, m_contrastModeComboBox
    };
    for (QComboBox *field : modelComboFields) {
        connect(field, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this]() {
            updateMatchingParameterControlState();
            markLocatorDirty(true);
        });
    }
    const QList<QComboBox *> validationComboFields{
        m_subPixelComboBox, m_primaryStrategyComboBox,
        m_lockedTemplateComboBox
    };
    for (QComboBox *field : validationComboFields) {
        connect(field, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this]() {
            updateMatchingParameterControlState();
            markLocatorDirty(false);
        });
    }
    connect(m_fusionEnabledCheckBox, &QCheckBox::toggled,
            this, [this]() {
        updateMatchingParameterControlState();
        markLocatorDirty(false);
    });
    connect(m_templatePrioritySpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        const int index = activeTemplateIndex();
        if (m_loadingLocatorControls || index < 0)
            return;
        if (m_locatorConfig.version ==
                TemplateLocationConfig::CompositeBankParamsVersion) {
            const QString baseId = m_locatorConfig.templates.at(index)
                    .sourceBaseId;
            QVector<int> siblingIndexes;
            for (int candidate = 0;
                 candidate < m_locatorConfig.templates.size(); ++candidate) {
                if (candidate != index &&
                        m_locatorConfig.templates.at(candidate).sourceBaseId ==
                        baseId) {
                    siblingIndexes.append(candidate);
                }
            }
            std::stable_sort(siblingIndexes.begin(), siblingIndexes.end(),
                             [this](int left, int right) {
                return m_locatorConfig.templates.at(left).order <
                        m_locatorConfig.templates.at(right).order;
            });
            siblingIndexes.insert(qBound(0, value, siblingIndexes.size()),
                                  index);
            for (int order = 0; order < siblingIndexes.size(); ++order) {
                TemplateLocationTemplateConfig &ordered =
                        m_locatorConfig.templates[siblingIndexes.at(order)];
                ordered.order = order;
                ordered.priority = order;
            }
        } else {
            m_locatorConfig.templates[index].priority = value;
        }
        markLocatorDirty(false);
        refreshReferenceTemplateBank();
    });
    connect(m_templateIndependentParametersCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
        if (m_loadingLocatorControls || m_referencePositionEditorReadOnly)
            return;
        if (m_locatorConfig.version !=
                TemplateLocationConfig::CompositeBankParamsVersion) {
            synchronizeLocatorBaseBindings(true);
        }
        const int index = activeTemplateIndex();
        if (index < 0)
            return;
        TemplateLocationTemplateConfig &item =
                m_locatorConfig.templates[index];
        if (checked) {
            item.independentParameters =
                    TemplateLocationConfig::sharedMatchParameters(
                        m_locatorConfig);
            item.independentParametersComplete = true;
            item.useIndependentParameters = true;
        } else {
            item.useIndependentParameters = false;
        }
        loadMatchingParametersForActiveTemplate();
        markActiveTemplateDirty();
    });
    connect(m_searchFullButton, &QPushButton::clicked, this, [this]() {
        stopReferencePositionRoiEditing(false);
        m_locatorConfig.searchRegionType = QStringLiteral("full");
        m_locatorConfig.searchRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
        m_locatorConfig.searchPolygonNormalized.clear();
        m_locatorConfig.searchCircleCenterNormalized = QPointF();
        m_locatorConfig.searchCircleRadiusNormalized = 0.0;
        markLocatorDirty(false);
        restoreReferencePositionRoi();
        renderReferencePositionOverlays();
    });
    connect(m_searchRectButton, &QPushButton::clicked, this, [this]() {
        startReferenceSearchRegionEditing(
                    PositionCorrectionRoiEditMode::SearchRectangle);
    });
    connect(m_searchCircleButton, &QPushButton::clicked, this, [this]() {
        startReferenceSearchRegionEditing(
                    PositionCorrectionRoiEditMode::SearchCircle);
    });
    connect(m_searchPolygonButton, &QPushButton::clicked, this, [this]() {
        startReferenceSearchRegionEditing(
                    PositionCorrectionRoiEditMode::SearchPolygon);
    });
    connect(m_searchFinishButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::finishReferenceSearchRegionEditing);
    updateMatchingParameterControlState();
}

void ReferenceImageDialog::loadPositionCorrectionConfig()
{
    m_referencePositionCorrection =
            SchemeStore::instance().currentScheme().referencePositionCorrection;
    const ReferenceTemplateLocationConfigResult resolved =
            ReferenceTemplateLocationConfig::fromJson(
                PositionCorrection::referenceToJson(m_referencePositionCorrection),
                m_referencePositionCorrection);
    m_referencePositionEditorReadOnly = !resolved.supported;
    m_referenceLocatorBankEnvelope = resolved.supported &&
            resolved.bankEnvelope;
    m_referencePositionUnsupportedStatus = resolved.status;
    m_referencePositionUnsupportedMessage = resolved.message;
    if (resolved.supported) {
        loadLocatorControls(resolved.bank);
    } else {
        // Never leave values from the previously opened scheme visible behind
        // an unknown read-only locator representation.
        m_locatorConfig = TemplateLocationModelBankConfig();
        m_activeTemplateId.clear();
        refreshReferenceTemplateBank();
    }
    if (resolved.supported && resolved.bankEnvelope) {
        QString invalidTemplateId;
        QString readinessMessage;
        const bool ready = resolved.valid &&
                m_referencePositionCorrection.referenceCreated &&
                TemplateLocationConfig::allEnabledModelsReady(m_locatorConfig) &&
                ReferenceTemplateLocationConfig::validateAllEnabledFrozenPoses(
                    m_locatorConfig,
                    m_referencePositionCorrection.referencePosesByTemplateId,
                    &invalidTemplateId, &readinessMessage);
        if (!ready) {
            // Overall readiness is atomic, but the pose bank is intentionally
            // per-template.  A Base replacement removes only poses bound to
            // that Base in SchemeStore; keep the unaffected frozen poses so a
            // later rebuild can reuse their stable template/Base identities.
            m_referencePositionCorrection.referenceCreated = false;
            m_referencePositionCorrection.referencePose = QJsonObject();
            m_referencePositionCorrection.status =
                    QStringLiteral("reference_bank_not_ready");
            m_referencePositionCorrection.message = !resolved.valid
                    ? resolved.message
                    : readinessMessage.trimmed().isEmpty()
                      ? tr("启用模板尚未全部完成自匹配")
                      : readinessMessage;
            writeLocatorControls();
            refreshReferenceTemplateBank();
        }
    }
    if (m_positionOriginModeComboBox) {
        const QSignalBlocker blocker(m_positionOriginModeComboBox);
        m_positionOriginModeComboBox->setCurrentIndex(
                    m_referencePositionCorrection.originMode == QStringLiteral("custom") ? 1 : 0);
    }
    updateReferencePositionOriginControls();
    {
        const QSignalBlocker blocker(ui->positionCorrectionCheckBox);
        ui->positionCorrectionCheckBox->setChecked(
                    m_referencePositionCorrection.enabled);
    }
    ui->positionCorrectionCheckBox->setEnabled(
                !m_referencePositionEditorReadOnly);
    if (m_positionSettingsFrame)
        m_positionSettingsFrame->setEnabled(!m_referencePositionEditorReadOnly);
    ui->referenceTemplateBankCard->setEnabled(
                !m_referencePositionEditorReadOnly);
    ui->referenceTemplateBankCard->setVisible(
                !m_referencePositionEditorReadOnly);
    setAdvancedVisible(ui->allModeButton->isChecked());
    updateReferenceImageControls();
    updatePositionCorrectionUi(m_referencePositionCorrection.enabled);
    if (!m_referencePositionEditorReadOnly &&
            m_referencePositionCorrection.referenceCreated &&
            m_positionStatusLabel) {
        m_positionStatusLabel->setText(tr("基准定位可用"));
    }
}

void ReferenceImageDialog::reloadReferenceStateAfterImageChange()
{
    // SchemeStore commits the new pixels and invalidates only the models and
    // frozen poses bound to the changed Base. Always reload that committed
    // state here; writing the dialog's pre-change copy back would resurrect
    // stale model identities.
    loadPositionCorrectionConfig();
    clearReferencePositionMatchOverlays();
}

bool ReferenceImageDialog::saveReferenceDraftBeforeAssetMutation(
        QString *errorMessage)
{
    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        if (errorMessage)
            *errorMessage = error;
        return false;
    }
    if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::None) {
        if (errorMessage)
            *errorMessage = tr("请先点击“完成”确认当前区域，再修改 Base");
        return false;
    }
    if (!m_referencePositionEditorReadOnly)
        writeLocatorControls();
    store.setReferencePositionCorrection(m_referencePositionCorrection);
    if (!store.saveCurrentScheme(&error)) {
        // saveCurrentScheme restores SchemeStore's persisted candidate on
        // failure. Mirror that rollback so a subsequent asset action cannot
        // resubmit the rejected draft.
        loadPositionCorrectionConfig();
        if (errorMessage)
            *errorMessage = error;
        return false;
    }
    return true;
}

void ReferenceImageDialog::synchronizeLocatorBaseBindings(bool forceV6)
{
    const QVector<ReferenceAsset> assets =
            SchemeStore::instance().referenceAssets();
    if (assets.isEmpty())
        return;
    const QString primaryId = SchemeStore::instance().currentScheme()
            .referenceAssets.primaryBaseId();
    if (forceV6 && m_locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion) {
        m_locatorConfig = TemplateLocationConfig::upgradeToV6(
                    m_locatorConfig,
                    primaryId.trimmed().isEmpty()
                    ? assets.first().baseId : primaryId);
    }
    if (m_locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion)
        return;

    QMap<QString, int> assetOrders;
    for (int index = 0; index < assets.size(); ++index) {
        assetOrders.insert(assets.at(index).baseId, index);
    }
    QSet<QString> referencedIds;
    QMap<QString, QVector<int>> templateIndexesByBase;
    for (int index = 0; index < m_locatorConfig.templates.size(); ++index) {
        TemplateLocationTemplateConfig &item = m_locatorConfig.templates[index];
        const QString sourceBaseId = item.sourceBaseId.trimmed();
        // An existing v6 source identity is authoritative even when its Base
        // was removed or the field is empty.  Silently substituting the
        // primary Base would make an invalid bank look valid and rewrite the
        // user's binding merely by opening/saving this page.
        if (sourceBaseId.isEmpty())
            continue;
        referencedIds.insert(sourceBaseId);
        templateIndexesByBase[sourceBaseId].append(index);
    }
    // Existing per-Base template order is serialized identity.  Do not
    // normalize it during load/save; explicit add/rebind/reorder handlers
    // allocate or update an order themselves.
    const bool requiresCustomOrigin = referencedIds.size() > 1 ||
            m_locatorConfig.templates.size() > 1;

    QVector<TemplateLocationBaseBindingConfig> bindings;
    bindings.reserve(referencedIds.size());
    QSet<QString> appendedBaseIds;
    for (const ReferenceAsset &asset : assets) {
        if (!referencedIds.contains(asset.baseId))
            continue;
        const TemplateLocationBaseBindingConfig *existing =
                TemplateLocationConfig::findBaseBinding(
                    m_locatorConfig, asset.baseId);
        TemplateLocationBaseBindingConfig binding;
        if (existing) {
            binding = *existing;
        } else {
            binding.baseId = asset.baseId;
            binding.searchRegionType = QStringLiteral("full");
            binding.searchRoiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
            binding.originMode = requiresCustomOrigin
                    ? QStringLiteral("centroid")
                    : m_locatorConfig.originMode;
            binding.customOriginNormalized =
                    m_locatorConfig.customOriginNormalized;
        }
        binding.order = assetOrders.value(asset.baseId);
        bindings.append(binding);
        appendedBaseIds.insert(asset.baseId);
    }

    // Keep an unavailable but still-referenced binding intact so opening the
    // editor is lossless.  It remains visibly invalid until the user actively
    // chooses a replacement Base.
    for (const TemplateLocationBaseBindingConfig &existing :
         qAsConst(m_locatorConfig.baseBindings)) {
        const QString baseId = existing.baseId.trimmed();
        if (baseId.isEmpty() || appendedBaseIds.contains(baseId) ||
                !referencedIds.contains(baseId)) {
            continue;
        }
        bindings.append(existing);
        appendedBaseIds.insert(baseId);
    }

    // Multi-template v6 requires a custom origin per binding, but those
    // normalized coordinates belong to different Base images and therefore
    // must never be copied across Bases.  Preserve every valid custom value;
    // when upgrading a centroid binding, freeze the first valid template
    // centre belonging to that same Base.
    if (requiresCustomOrigin) {
        for (TemplateLocationBaseBindingConfig &binding : bindings) {
            if (binding.originMode == QStringLiteral("custom") &&
                    validNormalizedPoint(binding.customOriginNormalized)) {
                continue;
            }
            QPointF baseOrigin;
            const QVector<int> indexes =
                    templateIndexesByBase.value(binding.baseId);
            for (int templateIndex : indexes) {
                const QPointF candidate = centroidForTemplate(
                            m_locatorConfig.templates.at(templateIndex));
                if (!validNormalizedPoint(candidate))
                    continue;
                baseOrigin = candidate;
                break;
            }
            if (validNormalizedPoint(baseOrigin)) {
                binding.originMode = QStringLiteral("custom");
                binding.customOriginNormalized = baseOrigin;
            }
        }
    }
    m_locatorConfig.baseBindings = bindings;
    m_locatorConfig.primaryMatchStrategy = QStringLiteral("first_valid");
    m_locatorConfig.primaryTemplateId.clear();
    TemplateLocationConfig::ensureStableTemplateIds(&m_locatorConfig);

    const int activeIndex = activeTemplateIndex();
    if (activeIndex >= 0) {
        const TemplateLocationBaseBindingConfig *activeBinding =
                TemplateLocationConfig::findBaseBinding(
                    m_locatorConfig,
                    m_locatorConfig.templates.at(activeIndex).sourceBaseId);
        if (activeBinding) {
            m_referencePositionCorrection.originMode =
                    activeBinding->originMode;
            m_referencePositionCorrection.customOriginNormalized =
                    activeBinding->customOriginNormalized;
            m_locatorConfig.originMode = activeBinding->originMode;
            m_locatorConfig.customOriginNormalized =
                    activeBinding->customOriginNormalized;
        }
    }
}

void ReferenceImageDialog::storeLegacyBaseBindingGeometry()
{
    if (m_locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion)
        return;
    const QString baseId = activeTemplateBaseId();
    TemplateLocationBaseBindingConfig *binding =
            TemplateLocationConfig::findBaseBinding(
                &m_locatorConfig, baseId);
    if (!binding)
        return;
    binding->searchRegionType = m_locatorConfig.searchRegionType;
    binding->searchRoiNormalized = m_locatorConfig.searchRoiNormalized;
    binding->searchPolygonNormalized = m_locatorConfig.searchPolygonNormalized;
    binding->searchCircleCenterNormalized =
            m_locatorConfig.searchCircleCenterNormalized;
    binding->searchCircleRadiusNormalized =
            m_locatorConfig.searchCircleRadiusNormalized;
    binding->originMode = m_referencePositionCorrection.originMode;
    binding->customOriginNormalized =
            m_referencePositionCorrection.customOriginNormalized;
}

void ReferenceImageDialog::loadActiveBaseBindingGeometry()
{
    if (m_locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion)
        return;
    const TemplateLocationBaseBindingConfig *binding =
            TemplateLocationConfig::findBaseBinding(
                m_locatorConfig, activeTemplateBaseId());
    if (!binding)
        return;
    m_locatorConfig.searchRegionType = binding->searchRegionType;
    m_locatorConfig.searchRoiNormalized = binding->searchRoiNormalized;
    m_locatorConfig.searchPolygonNormalized = binding->searchPolygonNormalized;
    m_locatorConfig.searchCircleCenterNormalized =
            binding->searchCircleCenterNormalized;
    m_locatorConfig.searchCircleRadiusNormalized =
            binding->searchCircleRadiusNormalized;
    m_referencePositionCorrection.originMode = binding->originMode;
    m_referencePositionCorrection.customOriginNormalized =
            binding->customOriginNormalized;
    if (m_positionOriginModeComboBox) {
        const QSignalBlocker blocker(m_positionOriginModeComboBox);
        m_positionOriginModeComboBox->setCurrentIndex(
                    binding->originMode == QStringLiteral("custom") ? 1 : 0);
    }
    updateReferencePositionOriginControls();
    if (m_searchFullButton) {
        m_searchFullButton->setChecked(
                    binding->searchRegionType == QStringLiteral("full"));
        m_searchRectButton->setChecked(
                    binding->searchRegionType == QStringLiteral("rectangle"));
        m_searchCircleButton->setChecked(
                    binding->searchRegionType == QStringLiteral("circle"));
        m_searchPolygonButton->setChecked(
                    binding->searchRegionType == QStringLiteral("polygon"));
    }
}

void ReferenceImageDialog::storeMatchingParametersToActiveTemplate()
{
    const int index = activeTemplateIndex();
    if (index < 0 || m_locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion)
        return;
    TemplateLocationTemplateConfig &item = m_locatorConfig.templates[index];
    if (!item.useIndependentParameters)
        return;
    TemplateLocationMatchParameters parameters = item.independentParameters;
    parameters.minScore = m_minScoreSpinBox->value();
    parameters.angleStart = m_angleMinSpinBox->value();
    parameters.angleExtent = m_angleMaxSpinBox->value() -
            m_angleMinSpinBox->value();
    parameters.scaleMin = m_scaleMinSpinBox->value();
    parameters.scaleMax = m_scaleMaxSpinBox->value();
    parameters.polarity = m_polarityComboBox->currentData().toString();
    parameters.contrastMode =
            m_contrastModeComboBox->currentData().toString();
    parameters.contrast = m_contrastSpinBox->value();
    parameters.minContrast = m_minContrastSpinBox->value();
    parameters.numLevels = m_numLevelsSpinBox->value();
    parameters.subPixel = m_subPixelComboBox->currentText();
    parameters.greediness = m_greedinessSpinBox->value();
    parameters.maxOverlap = m_maxOverlapSpinBox->value() / 100.0;
    item.independentParameters = parameters;
    item.independentParametersComplete = true;
}

void ReferenceImageDialog::loadMatchingParametersForActiveTemplate()
{
    const int index = activeTemplateIndex();
    if (index < 0 || !m_minScoreSpinBox)
        return;
    const TemplateLocationTemplateConfig &item =
            m_locatorConfig.templates.at(index);
    const TemplateLocationMatchParameters parameters =
            TemplateLocationConfig::effectiveMatchParameters(
                m_locatorConfig, item);
    const bool previousLoading = m_loadingLocatorControls;
    m_loadingLocatorControls = true;
    m_minScoreSpinBox->setValue(parameters.minScore);
    m_angleMinSpinBox->setValue(parameters.angleStart);
    m_angleMaxSpinBox->setValue(parameters.angleStart +
                                parameters.angleExtent);
    m_scaleMinSpinBox->setValue(parameters.scaleMin);
    m_scaleMaxSpinBox->setValue(parameters.scaleMax);
    int comboIndex = m_polarityComboBox->findData(parameters.polarity);
    m_polarityComboBox->setCurrentIndex(comboIndex < 0 ? 0 : comboIndex);
    comboIndex = m_contrastModeComboBox->findData(parameters.contrastMode);
    m_contrastModeComboBox->setCurrentIndex(comboIndex < 0 ? 0 : comboIndex);
    m_contrastSpinBox->setValue(parameters.contrast);
    m_minContrastSpinBox->setValue(parameters.minContrast);
    m_numLevelsSpinBox->setValue(parameters.numLevels);
    comboIndex = m_subPixelComboBox->findText(parameters.subPixel);
    m_subPixelComboBox->setCurrentIndex(comboIndex < 0 ? 0 : comboIndex);
    m_greedinessSpinBox->setValue(parameters.greediness);
    m_maxOverlapSpinBox->setValue(qRound(parameters.maxOverlap * 100.0));
    if (m_templateIndependentParametersCheckBox) {
        m_templateIndependentParametersCheckBox->setChecked(
                    m_locatorConfig.version ==
                    TemplateLocationConfig::CompositeBankParamsVersion &&
                    item.useIndependentParameters);
        m_templateIndependentParametersCheckBox->setEnabled(
                    !m_referencePositionEditorReadOnly &&
                    m_locatorConfig.version ==
                    TemplateLocationConfig::CompositeBankParamsVersion);
    }
    m_loadingLocatorControls = previousLoading;
    updateMatchingParameterControlState();
}

void ReferenceImageDialog::invalidateLocatorForBase(const QString &baseId)
{
    if (m_locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion) {
        for (TemplateLocationTemplateConfig &item : m_locatorConfig.templates)
            item.modelCreated = false;
        m_referencePositionCorrection.referenceCreated = false;
        m_referencePositionCorrection.referencePose = QJsonObject();
        m_referencePositionCorrection.referencePosesByTemplateId = QJsonObject();
        m_referencePositionCorrection.score = 0.0;
        m_referencePositionCorrection.elapsedMs = 0;
        m_referencePositionCorrection.status =
                QStringLiteral("reference_base_changed");
        m_referencePositionCorrection.message =
                tr("主 Base 已更新，需要重新创建基准定位");
        if (m_referenceLocatorBankEnvelope)
            writeLocatorControls();
        clearReferencePositionMatchOverlays();
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(
                        m_referencePositionCorrection.message);
        return;
    }
    bool affected = false;
    for (TemplateLocationTemplateConfig &item : m_locatorConfig.templates) {
        if (item.sourceBaseId == baseId) {
            item.modelCreated = false;
            m_referencePositionCorrection.referencePosesByTemplateId.remove(
                        item.templateId);
            affected = true;
        }
    }
    if (!affected)
        return;
    m_referenceLocatorBankEnvelope = true;
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.score = 0.0;
    m_referencePositionCorrection.elapsedMs = 0;
    m_referencePositionCorrection.status =
            QStringLiteral("reference_base_changed");
    m_referencePositionCorrection.message = tr(
                "Base %1 已更新，仅其绑定模板需要重新创建基准")
            .arg(baseId);
    writeLocatorControls();
    clearReferencePositionMatchOverlays();
    if (m_positionStatusLabel)
        m_positionStatusLabel->setText(m_referencePositionCorrection.message);
}

void ReferenceImageDialog::loadLocatorControls(
        const TemplateLocationModelBankConfig &config)
{
    m_loadingLocatorControls = true;
    m_locatorConfig = config;
    // Stable template identities may be repaired for editable known banks,
    // but the v6 normalizer must not run here: it fills a missing
    // sourceBaseId from the first binding and would silently rebind merely on
    // dialog load.
    TemplateLocationConfig::ensureStableTemplateIds(&m_locatorConfig);
    if (m_locatorConfig.templates.isEmpty()) {
        TemplateLocationTemplateConfig item;
        item.templateId = QUuid::createUuid().toString(
                    QUuid::WithoutBraces);
        item.name = tr("模板1");
        item.modelCacheKey = QStringLiteral(
                    "reference.positionCorrection.template.%1")
                .arg(item.templateId);
        m_locatorConfig.templates.append(item);
        m_locatorConfig.version = TemplateLocationConfig::ModelBankParamsVersion;
    }
    synchronizeLocatorBaseBindings(false);
    m_locatorConfig.maxMatches = 1;
    m_locatorConfig.minMatchCount = 1;
    m_locatorConfig.maxMatchCount = 1;
    if (m_activeTemplateId.trimmed().isEmpty() ||
            !TemplateLocationConfig::findTemplate(
                &m_locatorConfig, m_activeTemplateId)) {
        m_activeTemplateId = m_locatorConfig.templates.first().templateId;
    }

    m_minScoreSpinBox->setValue(m_locatorConfig.minScore);
    m_angleMinSpinBox->setValue(m_locatorConfig.angleStart);
    m_angleMaxSpinBox->setValue(m_locatorConfig.angleStart +
                                m_locatorConfig.angleExtent);
    m_scaleMinSpinBox->setValue(m_locatorConfig.scaleMin);
    m_scaleMaxSpinBox->setValue(m_locatorConfig.scaleMax);
    int index = m_polarityComboBox->findData(m_locatorConfig.polarity);
    m_polarityComboBox->setCurrentIndex(index < 0 ? 0 : index);
    index = m_contrastModeComboBox->findData(m_locatorConfig.contrastMode);
    m_contrastModeComboBox->setCurrentIndex(index < 0 ? 0 : index);
    m_contrastSpinBox->setValue(m_locatorConfig.contrast);
    m_minContrastSpinBox->setValue(m_locatorConfig.minContrast);
    m_numLevelsSpinBox->setValue(m_locatorConfig.numLevels);
    index = m_subPixelComboBox->findText(m_locatorConfig.subPixel);
    m_subPixelComboBox->setCurrentIndex(index < 0 ? 0 : index);
    m_greedinessSpinBox->setValue(m_locatorConfig.greediness);
    m_timeoutSpinBox->setValue(m_locatorConfig.timeoutMs);
    m_maxOverlapSpinBox->setValue(
                qRound(m_locatorConfig.maxOverlap * 100.0));
    index = m_primaryStrategyComboBox->findData(
                m_locatorConfig.primaryMatchStrategy);
    m_primaryStrategyComboBox->setCurrentIndex(index < 0 ? 0 : index);
    m_fusionEnabledCheckBox->setChecked(m_locatorConfig.fusion.enabled);
    m_fusionPositionToleranceSpinBox->setValue(
                m_locatorConfig.fusion.positionTolerancePx);
    m_fusionAngleToleranceSpinBox->setValue(
                m_locatorConfig.fusion.angleToleranceDeg);
    m_fusionScaleToleranceSpinBox->setValue(
                m_locatorConfig.fusion.scaleTolerance);
    m_searchFullButton->setChecked(
                m_locatorConfig.searchRegionType == QStringLiteral("full"));
    m_searchRectButton->setChecked(
                m_locatorConfig.searchRegionType == QStringLiteral("rectangle"));
    m_searchCircleButton->setChecked(
                m_locatorConfig.searchRegionType == QStringLiteral("circle"));
    m_searchPolygonButton->setChecked(
                m_locatorConfig.searchRegionType == QStringLiteral("polygon"));
    m_referencePositionCorrection.originMode = m_locatorConfig.originMode;
    m_referencePositionCorrection.customOriginNormalized =
            m_locatorConfig.customOriginNormalized;
    loadActiveTemplateGeometry();
    loadActiveBaseBindingGeometry();
    loadMatchingParametersForActiveTemplate();
    refreshReferenceTemplateBank();
    m_loadingLocatorControls = false;
    updateMatchingParameterControlState();
}

void ReferenceImageDialog::writeLocatorControls()
{
    if (m_loadingLocatorControls || m_referencePositionEditorReadOnly ||
            !m_referenceLocatorBankEnvelope)
        return;
    storeLegacyGeometryToActiveTemplate();
    storeLegacyBaseBindingGeometry();
    storeMatchingParametersToActiveTemplate();
    m_locatorConfig.templateMode = QStringLiteral("alternatives");
    const int currentTemplateIndex = activeTemplateIndex();
    const bool independent = currentTemplateIndex >= 0 &&
            m_locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion &&
            m_locatorConfig.templates.at(currentTemplateIndex)
            .useIndependentParameters;
    if (!independent) {
        m_locatorConfig.minScore = m_minScoreSpinBox->value();
        m_locatorConfig.angleStart = m_angleMinSpinBox->value();
        m_locatorConfig.angleExtent = m_angleMaxSpinBox->value() -
                m_angleMinSpinBox->value();
        m_locatorConfig.scaleMin = m_scaleMinSpinBox->value();
        m_locatorConfig.scaleMax = m_scaleMaxSpinBox->value();
        m_locatorConfig.polarity = m_polarityComboBox->currentData().toString();
        m_locatorConfig.contrastMode =
                m_contrastModeComboBox->currentData().toString();
        m_locatorConfig.contrast = m_contrastSpinBox->value();
        m_locatorConfig.minContrast = m_minContrastSpinBox->value();
        m_locatorConfig.numLevels = m_numLevelsSpinBox->value();
        m_locatorConfig.subPixel = m_subPixelComboBox->currentText();
        m_locatorConfig.greediness = m_greedinessSpinBox->value();
        m_locatorConfig.maxOverlap = m_maxOverlapSpinBox->value() / 100.0;
    }
    m_locatorConfig.timeoutMs = m_timeoutSpinBox->value();
    m_locatorConfig.maxMatches = 1;
    m_locatorConfig.minMatchCount = 1;
    m_locatorConfig.maxMatchCount = 1;
    if (m_locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion) {
        synchronizeLocatorBaseBindings(false);
        m_locatorConfig.primaryMatchStrategy = QStringLiteral("first_valid");
        m_locatorConfig.primaryTemplateId.clear();
    } else {
        m_locatorConfig.primaryMatchStrategy =
                m_primaryStrategyComboBox->currentData().toString();
        m_locatorConfig.primaryTemplateId =
                m_lockedTemplateComboBox->currentData().toString();
    }
    // The v6 codec intentionally has no fusion contract.  Keep these legacy
    // controls strictly v4/v5 so the UI cannot offer parameters that disappear
    // on save.
    if (m_locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion) {
        m_locatorConfig.fusion.enabled =
                m_fusionEnabledCheckBox->isChecked();
        m_locatorConfig.fusion.positionTolerancePx =
                m_fusionPositionToleranceSpinBox->value();
        m_locatorConfig.fusion.angleToleranceDeg =
                m_fusionAngleToleranceSpinBox->value();
        m_locatorConfig.fusion.scaleTolerance =
                m_fusionScaleToleranceSpinBox->value();
    }
    m_locatorConfig.originMode = m_referencePositionCorrection.originMode;
    m_locatorConfig.customOriginNormalized =
            m_referencePositionCorrection.customOriginNormalized;
    m_referencePositionCorrection.locator =
            TemplateLocationConfig::toToolParams(m_locatorConfig);
    const int referenceEnvelopeVersion = m_locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion
            ? 5 : ReferenceTemplateLocationConfig::ModelBankEnvelopeVersion;
    m_referencePositionCorrection.version = referenceEnvelopeVersion;
    m_referencePositionCorrection.extra.insert(
                QStringLiteral("version"),
                referenceEnvelopeVersion);
}

void ReferenceImageDialog::updateMatchingParameterControlState()
{
    if (!m_contrastModeComboBox)
        return;
    const bool manual = m_contrastModeComboBox->currentData().toString() ==
            QStringLiteral("manual");
    m_contrastSpinBox->setEnabled(manual && !m_referencePositionEditorReadOnly);
    m_minContrastSpinBox->setEnabled(manual && !m_referencePositionEditorReadOnly);
    const bool locked = m_primaryStrategyComboBox->currentData().toString() ==
            QStringLiteral("locked_template");
    m_lockedTemplateComboBox->setEnabled(locked &&
                                         !m_referencePositionEditorReadOnly);
    m_primaryStrategyComboBox->setEnabled(
                !m_referencePositionEditorReadOnly &&
                m_locatorConfig.version !=
                TemplateLocationConfig::CompositeBankParamsVersion);
    const bool v6 = m_locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion;
    const QList<QWidget *> fusionFields{
        m_fusionEnabledCheckBox,
        m_fusionPositionToleranceSpinBox,
        m_fusionAngleToleranceSpinBox,
        m_fusionScaleToleranceSpinBox
    };
    for (QWidget *field : fusionFields) {
        if (!field)
            continue;
        field->setVisible(!v6);
        if (m_advancedMatchingForm) {
            if (QWidget *label = m_advancedMatchingForm->labelForField(field))
                label->setVisible(!v6);
        }
    }
    const bool fusion = !v6 && m_fusionEnabledCheckBox->isChecked();
    m_fusionEnabledCheckBox->setEnabled(
                !v6 && !m_referencePositionEditorReadOnly);
    m_fusionPositionToleranceSpinBox->setEnabled(
                fusion && !m_referencePositionEditorReadOnly);
    m_fusionAngleToleranceSpinBox->setEnabled(
                fusion && !m_referencePositionEditorReadOnly);
    m_fusionScaleToleranceSpinBox->setEnabled(
                fusion && !m_referencePositionEditorReadOnly);
}

void ReferenceImageDialog::markLocatorDirty(bool modelParametersChanged)
{
    if (m_loadingLocatorControls || m_referencePositionEditorReadOnly)
        return;
    // Loading a v1..v3 reference creates only an in-memory compatibility bank.
    // Promote it to a supported locator envelope only after a real locator
    // edit, while retaining v4/v5 semantics.  v6 is entered only by an
    // explicit composite action (Base rebind, independent parameters or the
    // v6 template-add path), never because an unrelated Base merely exists.
    m_referenceLocatorBankEnvelope = true;
    if (m_locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion) {
        synchronizeLocatorBaseBindings(false);
    } else {
        TemplateLocationConfig::ensureStableTemplateIds(&m_locatorConfig);
    }
    if (modelParametersChanged) {
        for (TemplateLocationTemplateConfig &item : m_locatorConfig.templates)
            item.modelCreated = false;
    }
    m_referencePositionCorrection.referenceCreated = false;
    m_referencePositionCorrection.referencePose = QJsonObject();
    m_referencePositionCorrection.referencePosesByTemplateId = QJsonObject();
    m_referencePositionCorrection.status = QStringLiteral("editing_locator");
    m_referencePositionCorrection.message.clear();
    writeLocatorControls();
    clearReferencePositionMatchOverlays();
    if (m_positionStatusLabel)
    m_positionStatusLabel->setText(
                    tr("参数已修改，请点击“创建/更新基准”重新验证"));
}

void ReferenceImageDialog::markActiveTemplateDirty()
{
    if (m_loadingLocatorControls || m_referencePositionEditorReadOnly)
        return;
    const int index = activeTemplateIndex();
    if (index >= 0)
        m_locatorConfig.templates[index].modelCreated = false;
    markLocatorDirty(false);
}

bool ReferenceImageDialog::locatorBaseBindingIssue(QString *message) const
{
    if (m_locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion) {
        return false;
    }

    QSet<QString> availableBaseIds;
    for (const ReferenceAsset &asset :
         SchemeStore::instance().referenceAssets()) {
        availableBaseIds.insert(asset.baseId);
    }
    for (int index = 0; index < m_locatorConfig.templates.size(); ++index) {
        const TemplateLocationTemplateConfig &item =
                m_locatorConfig.templates.at(index);
        const QString templateName = item.name.trimmed().isEmpty()
                ? tr("模板%1").arg(index + 1) : item.name;
        const QString sourceBaseId = item.sourceBaseId.trimmed();
        if (sourceBaseId.isEmpty()) {
            if (message) {
                *message = tr("模板“%1”尚未绑定来源 Base，请在模板来源中重新选择")
                        .arg(templateName);
            }
            return true;
        }
        if (!availableBaseIds.contains(sourceBaseId)) {
            if (message) {
                *message = tr("模板“%1”绑定的 Base 已不存在（%2），请主动改绑")
                        .arg(templateName, sourceBaseId);
            }
            return true;
        }
        if (!TemplateLocationConfig::findBaseBinding(
                m_locatorConfig, sourceBaseId)) {
            if (message) {
                *message = tr("模板“%1”缺少 Base %2 的绑定配置，请重新选择来源 Base")
                        .arg(templateName, sourceBaseId);
            }
            return true;
        }
    }
    return false;
}

bool ReferenceImageDialog::validateLocatorControls(QString *message) const
{
    if (m_locatorConfig.templates.isEmpty() ||
            m_locatorConfig.templates.size() >
            TemplateLocationConfig::MaximumTemplateCount) {
        if (message) *message = tr("模板数量必须为 1~8 个");
        return false;
    }
    int enabledCount = 0;
    for (const TemplateLocationTemplateConfig &item : m_locatorConfig.templates) {
        if (!item.enabled)
            continue;
        ++enabledCount;
        const bool validGeometry = item.templateRegionType ==
                QStringLiteral("polygon")
                ? item.templatePolygonNormalized.size() >= 3
                : validNormalizedRect(item.templateRoiNormalized);
        if (!validGeometry) {
            if (message) *message = tr("模板“%1”尚未设置有效区域")
                    .arg(item.name);
            return false;
        }
    }
    if (enabledCount == 0) {
        if (message) *message = tr("至少启用一个模板");
        return false;
    }
    if (locatorBaseBindingIssue(message))
        return false;
    if (m_locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion &&
            m_locatorConfig.templates.size() > 1 &&
            (m_referencePositionCorrection.originMode !=
                 QStringLiteral("custom") ||
             !validNormalizedPoint(
                 m_referencePositionCorrection.customOriginNormalized))) {
        if (message) *message = tr("多模板必须使用同一个有效自定义定位点");
        return false;
    }
    if (m_angleMinSpinBox->value() > m_angleMaxSpinBox->value()) {
        if (message) *message = tr("最小角度不能大于最大角度");
        return false;
    }
    if (m_scaleMinSpinBox->value() > m_scaleMaxSpinBox->value()) {
        if (message) *message = tr("最小尺度不能大于最大尺度");
        return false;
    }
    if (m_contrastModeComboBox->currentData().toString() ==
            QStringLiteral("manual") &&
            m_minContrastSpinBox->value() >= m_contrastSpinBox->value()) {
        if (message) *message = tr("MinContrast 必须小于 Contrast");
        return false;
    }
    if (m_primaryStrategyComboBox->currentData().toString() ==
            QStringLiteral("locked_template") &&
            m_lockedTemplateComboBox->currentData().toString().isEmpty()) {
        if (message) *message = tr("请选择锁定模板");
        return false;
    }
    const TemplateLocationConfigValidationResult sharedValidation =
            TemplateLocationConfig::validateModelBank(m_locatorConfig, false);
    if (!sharedValidation.valid) {
        if (message) {
            *message = sharedValidation.message.trimmed().isEmpty()
                    ? tr("模板定位参数无效")
                    : sharedValidation.message;
        }
        return false;
    }
    return true;
}

int ReferenceImageDialog::activeTemplateIndex() const
{
    for (int index = 0; index < m_locatorConfig.templates.size(); ++index) {
        if (m_locatorConfig.templates.at(index).templateId == m_activeTemplateId)
            return index;
    }
    return -1;
}

void ReferenceImageDialog::storeLegacyGeometryToActiveTemplate()
{
    const int index = activeTemplateIndex();
    if (index < 0) {
        m_referenceTemplateIncludeEdited = false;
        m_referenceTemplateExcludeEdited = false;
        return;
    }
    TemplateLocationTemplateConfig &item = m_locatorConfig.templates[index];
    item.templateRegionType = m_referencePositionCorrection.templateRegionType;
    item.templateRoiNormalized =
            m_referencePositionCorrection.templateRoiNormalized;
    item.templatePolygonNormalized = polygonPointsFromJson(
                m_referencePositionCorrection.templatePolygonNormalized);
    item.templateMaskRegionType =
            m_referencePositionCorrection.templateMaskRegionType;
    item.templateMaskRoiNormalized =
            m_referencePositionCorrection.templateMaskRoiNormalized;
    item.templateMaskPolygonNormalized = polygonPointsFromJson(
                m_referencePositionCorrection.templateMaskPolygonNormalized);
    item.templateMaskCircleCenterNormalized =
            m_referencePositionCorrection.templateMaskCircleCenterNormalized;
    item.templateMaskCircleRadiusNormalized =
            m_referencePositionCorrection.templateMaskCircleRadiusNormalized;
    if (m_locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion &&
            m_referenceTemplateIncludeEdited) {
        TemplateLocationRegionConfig include;
        include.regionId = !item.includeRegions.isEmpty()
                ? item.includeRegions.first().regionId
                : QStringLiteral("region-%1").arg(
                      QUuid::createUuid().toString(QUuid::WithoutBraces));
        include.regionType = item.templateRegionType;
        include.roiNormalized = item.templateRoiNormalized;
        include.polygonNormalized = item.templatePolygonNormalized;
        if (item.includeRegions.isEmpty())
            item.includeRegions.append(include);
        else
            item.includeRegions[0] = include;
    }
    if (m_locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion &&
            m_referenceTemplateExcludeEdited) {
        const QString maskType = item.templateMaskRegionType.trimmed();
        if (maskType.isEmpty() || maskType == QStringLiteral("none")) {
            if (!item.excludeRegions.isEmpty())
                item.excludeRegions.removeFirst();
            if (!item.excludeRegions.isEmpty()) {
                const TemplateLocationRegionConfig &next =
                        item.excludeRegions.first();
                item.templateMaskRegionType = next.regionType;
                item.templateMaskRoiNormalized = next.roiNormalized;
                item.templateMaskPolygonNormalized = next.polygonNormalized;
                item.templateMaskCircleCenterNormalized =
                        next.circleCenterNormalized;
                item.templateMaskCircleRadiusNormalized =
                        next.circleRadiusNormalized;
                m_referencePositionCorrection.templateMaskRegionType =
                        next.regionType;
                m_referencePositionCorrection.templateMaskRoiNormalized =
                        next.roiNormalized;
                m_referencePositionCorrection.templateMaskPolygonNormalized =
                        polygonPointsToJson(next.polygonNormalized);
                m_referencePositionCorrection
                        .templateMaskCircleCenterNormalized =
                        next.circleCenterNormalized;
                m_referencePositionCorrection
                        .templateMaskCircleRadiusNormalized =
                        next.circleRadiusNormalized;
            }
        } else {
            TemplateLocationRegionConfig exclude;
            exclude.regionId = !item.excludeRegions.isEmpty()
                    ? item.excludeRegions.first().regionId
                    : QStringLiteral("region-%1").arg(
                          QUuid::createUuid().toString(QUuid::WithoutBraces));
            exclude.regionType = maskType;
            exclude.roiNormalized = item.templateMaskRoiNormalized;
            exclude.polygonNormalized = item.templateMaskPolygonNormalized;
            exclude.circleCenterNormalized =
                    item.templateMaskCircleCenterNormalized;
            exclude.circleRadiusNormalized =
                    item.templateMaskCircleRadiusNormalized;
            if (item.excludeRegions.isEmpty())
                item.excludeRegions.append(exclude);
            else
                item.excludeRegions[0] = exclude;
        }
    }
    if (m_locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion &&
            (m_referenceTemplateIncludeEdited ||
             m_referenceTemplateExcludeEdited)) {
        TemplateLocationConfig::ensureStableTemplateIds(&m_locatorConfig);
    }
    m_referenceTemplateIncludeEdited = false;
    m_referenceTemplateExcludeEdited = false;
}

void ReferenceImageDialog::loadActiveTemplateGeometry()
{
    // v6 may expose a rectangle legacy mirror for a circle include region.
    // Loading that mirror is not an edit and must never authorize replacing
    // the authoritative first include/exclude region during switch or save.
    m_referenceTemplateIncludeEdited = false;
    m_referenceTemplateExcludeEdited = false;
    const int index = activeTemplateIndex();
    if (index < 0)
        return;
    const TemplateLocationTemplateConfig &item =
            m_locatorConfig.templates.at(index);
    m_referencePositionCorrection.templateRegionType = item.templateRegionType;
    m_referencePositionCorrection.templateRoiNormalized =
            item.templateRoiNormalized;
    m_referencePositionCorrection.templatePolygonNormalized =
            polygonPointsToJson(item.templatePolygonNormalized);
    m_referencePositionCorrection.templateMaskRegionType =
            item.templateMaskRegionType;
    m_referencePositionCorrection.templateMaskRoiNormalized =
            item.templateMaskRoiNormalized;
    m_referencePositionCorrection.templateMaskPolygonNormalized =
            polygonPointsToJson(item.templateMaskPolygonNormalized);
    m_referencePositionCorrection.templateMaskCircleCenterNormalized =
            item.templateMaskCircleCenterNormalized;
    m_referencePositionCorrection.templateMaskCircleRadiusNormalized =
            item.templateMaskCircleRadiusNormalized;
    m_referencePositionCorrection.modelCacheKey = item.modelCacheKey;
    if (m_templatePrioritySpinBox) {
        const QSignalBlocker blocker(m_templatePrioritySpinBox);
        m_templatePrioritySpinBox->setValue(item.priority);
    }
    updatePositionCorrectionUi(m_referencePositionCorrection.enabled);
}

void ReferenceImageDialog::refreshReferenceTemplateBank()
{
    if (!m_referenceTemplateBankList)
        return;
    m_updatingTemplateBank = true;
    const QSignalBlocker listBlocker(m_referenceTemplateBankList);
    m_referenceTemplateBankList->clear();
    int enabledCount = 0;
    int readyCount = 0;
    int selectedRow = 0;
    QMap<QString, QString> baseNames;
    const QVector<ReferenceAsset> assets =
            SchemeStore::instance().referenceAssets();
    for (int baseIndex = 0; baseIndex < assets.size(); ++baseIndex) {
        baseNames.insert(assets.at(baseIndex).baseId,
                         tr("B%1").arg(baseIndex + 1, 2, 10,
                                        QLatin1Char('0')));
    }
    const QVector<TemplateLocationConfig::TemplateLocationOrderedTemplateRef>
            orderedTemplates =
            TemplateLocationConfig::orderedTemplateRefs(m_locatorConfig, false);
    for (int row = 0; row < orderedTemplates.size(); ++row) {
        const int index = orderedTemplates.at(row).templateIndex;
        const TemplateLocationTemplateConfig &item =
                m_locatorConfig.templates.at(index);
        if (item.enabled)
            ++enabledCount;
        if (item.enabled && item.modelCreated &&
                m_referencePositionCorrection.referencePosesByTemplateId
                .contains(item.templateId)) {
            ++readyCount;
        }
        QString status = item.enabled ? tr("待测试") : tr("停用");
        if (item.enabled && item.modelCreated &&
                m_referencePositionCorrection.referencePosesByTemplateId
                .contains(item.templateId)) {
            status = tr("可用");
        }
        const QString baseId = m_locatorConfig.version ==
                TemplateLocationConfig::CompositeBankParamsVersion
                ? item.sourceBaseId
                : ReferenceImageProvider::instance().primaryBaseId();
        const bool sourceAvailable = !baseId.trimmed().isEmpty() &&
                baseNames.contains(baseId);
        const bool bindingAvailable = m_locatorConfig.version !=
                TemplateLocationConfig::CompositeBankParamsVersion ||
                TemplateLocationConfig::findBaseBinding(
                    m_locatorConfig, baseId);
        if (m_locatorConfig.version ==
                TemplateLocationConfig::CompositeBankParamsVersion &&
                (!sourceAvailable || !bindingAvailable)) {
            status = tr("缺少 Base");
        }
        const QString baseLabel = sourceAvailable && bindingAvailable
                ? baseNames.value(baseId)
                : baseId.trimmed().isEmpty()
                  ? tr("未绑定 Base")
                  : sourceAvailable
                    ? tr("Base 配置缺失")
                    : tr("Base 已缺失");
        const int globalIndex = m_locatorConfig.version ==
                TemplateLocationConfig::CompositeBankParamsVersion
                ? TemplateLocationConfig::globalTemplateIndex(
                      m_locatorConfig, item.templateId)
                : index;
        auto *listItem = new QListWidgetItem(
                    tr("T%1 · %2 · %3 · %4")
                    .arg(qMax(0, globalIndex))
                    .arg(item.name.trimmed().isEmpty()
                         ? tr("模板%1").arg(index + 1) : item.name)
                    .arg(baseLabel)
                    .arg(status),
                    m_referenceTemplateBankList);
        listItem->setData(kReferenceTemplateIdRole, item.templateId);
        if (!sourceAvailable || !bindingAvailable) {
            listItem->setToolTip(baseId.trimmed().isEmpty()
                    ? tr("来源 Base 未绑定；保存不会自动改绑，请手动选择")
                    : tr("保留的来源 Base ID：%1；该 Base 当前不可用")
                      .arg(baseId));
        }
        listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable);
        listItem->setCheckState(item.enabled ? Qt::Checked : Qt::Unchecked);
        // The item model retains text/check-state for accessibility, tests and
        // serialization, while the compact item widget owns all painting.
        listItem->setFlags(listItem->flags() & ~Qt::ItemIsUserCheckable);
        listItem->setForeground(QBrush(Qt::transparent));
        listItem->setSizeHint(QSize(166, 42));

        auto *tab = new QFrame(m_referenceTemplateBankList);
        tab->setObjectName(QStringLiteral("referenceTemplateTab"));
        tab->setProperty("activeTemplate",
                         item.templateId == m_activeTemplateId);
        auto *tabLayout = new QHBoxLayout(tab);
        tabLayout->setContentsMargins(8, 0, 5, 0);
        tabLayout->setSpacing(4);
        auto *enabledButton = new QToolButton(tab);
        enabledButton->setObjectName(
                    QStringLiteral("referenceTemplateEnableButton"));
        enabledButton->setText(QString());
        enabledButton->setCheckable(true);
        enabledButton->setChecked(item.enabled);
        enabledButton->setToolTip(item.enabled
                ? tr("点击停用当前模板") : tr("点击启用当前模板"));
        enabledButton->setFixedSize(14, 14);
        auto *tabButton = new QPushButton(
                    tr("T%1  %2")
                    .arg(qMax(0, globalIndex))
                    .arg(item.name.trimmed().isEmpty()
                         ? tr("模板%1").arg(index + 1) : item.name), tab);
        tabButton->setObjectName(
                    QStringLiteral("referenceTemplateTabButton"));
        tabButton->setProperty("activeTemplate",
                               item.templateId == m_activeTemplateId);
        tabButton->setToolTip(listItem->text());
        auto *closeButton = new QToolButton(tab);
        closeButton->setObjectName(
                    QStringLiteral("referenceTemplateCloseButton"));
        closeButton->setText(QStringLiteral("×"));
        closeButton->setToolTip(tr("删除该模板"));
        closeButton->setFixedSize(24, 30);
        closeButton->setEnabled(!m_referencePositionEditorReadOnly &&
                                m_locatorConfig.templates.size() > 1);
        tabLayout->addWidget(enabledButton, 0, Qt::AlignVCenter);
        tabLayout->addWidget(tabButton, 1);
        tabLayout->addWidget(closeButton);
        m_referenceTemplateBankList->setItemWidget(listItem, tab);

        connect(tabButton, &QPushButton::clicked, this,
                [this, templateId = item.templateId]() {
            QTimer::singleShot(0, this, [this, templateId]() {
                if (!m_referenceTemplateBankList)
                    return;
                for (int row = 0;
                     row < m_referenceTemplateBankList->count(); ++row) {
                    QListWidgetItem *candidate =
                            m_referenceTemplateBankList->item(row);
                    if (candidate->data(kReferenceTemplateIdRole).toString()
                            == templateId) {
                        m_referenceTemplateBankList->setCurrentItem(candidate);
                        break;
                    }
                }
            });
        });
        connect(enabledButton, &QToolButton::toggled, this,
                [this, templateId = item.templateId](bool enabled) {
            QTimer::singleShot(0, this, [this, templateId, enabled]() {
                if (!m_referenceTemplateBankList)
                    return;
                for (int row = 0;
                     row < m_referenceTemplateBankList->count(); ++row) {
                    QListWidgetItem *candidate =
                            m_referenceTemplateBankList->item(row);
                    if (candidate->data(kReferenceTemplateIdRole).toString()
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
            // Switching rebuilds the tab widgets. Queue deletion so the
            // sender is never destroyed in the middle of its click handler.
            QTimer::singleShot(0, this, [this, templateId]() {
                switchActiveTemplate(templateId);
                deleteReferenceTemplate();
            });
        });
        if (item.templateId == m_activeTemplateId)
            selectedRow = row;
    }
    if (!m_locatorConfig.templates.isEmpty())
        m_referenceTemplateBankList->setCurrentRow(selectedRow);
    m_referenceTemplateBankSummaryLabel->setText(
                tr("共 %1 个，启用 %2 个，可用 %3 个")
                .arg(m_locatorConfig.templates.size())
                .arg(enabledCount).arg(readyCount));
    m_addReferenceTemplateButton->setEnabled(
                !m_referencePositionEditorReadOnly &&
                m_locatorConfig.templates.size() <
                TemplateLocationConfig::MaximumTemplateCount);
    m_renameReferenceTemplateButton->setEnabled(
                !m_referencePositionEditorReadOnly &&
                !m_locatorConfig.templates.isEmpty());
    m_deleteReferenceTemplateButton->setEnabled(
                !m_referencePositionEditorReadOnly &&
                m_locatorConfig.templates.size() > 1);

    if (m_templateSourceBaseComboBox) {
        const QSignalBlocker sourceBlocker(m_templateSourceBaseComboBox);
        m_templateSourceBaseComboBox->clear();
        for (int index = 0; index < assets.size(); ++index) {
            const ReferenceAsset &asset = assets.at(index);
            m_templateSourceBaseComboBox->addItem(
                        tr("B%1 · %2%3")
                        .arg(index + 1, 2, 10, QLatin1Char('0'))
                        .arg(asset.name)
                        .arg(asset.baseId ==
                             SchemeStore::instance().currentScheme()
                             .referenceAssets.primaryBaseId()
                             ? tr("（主）") : QString()),
                        asset.baseId);
        }
        const int activeIndex = activeTemplateIndex();
        const QString activeBaseId = activeIndex >= 0 &&
                m_locatorConfig.version ==
                TemplateLocationConfig::CompositeBankParamsVersion
                ? m_locatorConfig.templates.at(activeIndex).sourceBaseId
                : ReferenceImageProvider::instance().primaryBaseId();
        int sourceIndex = m_templateSourceBaseComboBox->findData(activeBaseId);
        if (activeIndex >= 0 && m_locatorConfig.version ==
                TemplateLocationConfig::CompositeBankParamsVersion &&
                sourceIndex < 0) {
            m_templateSourceBaseComboBox->insertItem(
                        0,
                        activeBaseId.trimmed().isEmpty()
                        ? tr("⚠ 未绑定（请选择 Base）")
                        : tr("⚠ Base 已缺失 · %1").arg(activeBaseId),
                        activeBaseId);
            sourceIndex = 0;
        }
        m_templateSourceBaseComboBox->setCurrentIndex(sourceIndex);
        m_templateSourceBaseComboBox->setEnabled(
                    !m_referencePositionEditorReadOnly &&
                    !assets.isEmpty());
    }

    const QString lockedId = m_locatorConfig.primaryTemplateId;
    const QSignalBlocker comboBlocker(m_lockedTemplateComboBox);
    m_lockedTemplateComboBox->clear();
    for (const TemplateLocationTemplateConfig &item : m_locatorConfig.templates) {
        if (item.enabled)
            m_lockedTemplateComboBox->addItem(item.name, item.templateId);
    }
    int lockedIndex = m_lockedTemplateComboBox->findData(lockedId);
    if (lockedIndex < 0 && m_lockedTemplateComboBox->count() > 0)
        lockedIndex = 0;
    m_lockedTemplateComboBox->setCurrentIndex(lockedIndex);
    m_locatorConfig.primaryTemplateId =
            m_lockedTemplateComboBox->currentData().toString();
    QString bindingIssue;
    const bool hasBindingIssue = locatorBaseBindingIssue(&bindingIssue);
    if (m_positionTestButton) {
        m_positionTestButton->setEnabled(
                    !m_referencePositionEditorReadOnly && !hasBindingIssue);
        m_positionTestButton->setToolTip(hasBindingIssue
                ? bindingIssue
                : tr("逐个创建所有启用模板并在各自局部区域完成自匹配，全部成功后更新基准"));
    }
    if (hasBindingIssue && m_referenceTemplateBankSummaryLabel)
        m_referenceTemplateBankSummaryLabel->setToolTip(bindingIssue);
    else if (m_referenceTemplateBankSummaryLabel)
        m_referenceTemplateBankSummaryLabel->setToolTip(QString());
    m_updatingTemplateBank = false;
    updateMatchingParameterControlState();
}

void ReferenceImageDialog::switchActiveTemplate(const QString &templateId)
{
    if (templateId.trimmed().isEmpty() || templateId == m_activeTemplateId)
        return;
    stopReferencePositionOriginSelection();
    stopReferencePositionRoiEditing(true);
    storeLegacyGeometryToActiveTemplate();
    storeMatchingParametersToActiveTemplate();
    storeLegacyBaseBindingGeometry();
    m_activeTemplateId = templateId;
    loadActiveTemplateGeometry();
    loadActiveBaseBindingGeometry();
    loadMatchingParametersForActiveTemplate();
    const QString baseId = activeTemplateBaseId();
    if (!baseId.isEmpty())
        selectReferenceBase(baseId);
    refreshReferenceTemplateBank();
    clearReferencePositionMatchOverlays();
    restoreReferencePositionRoi();
    renderReferencePositionOverlays();
    if (m_positionStatusLabel) {
        const int index = activeTemplateIndex();
        m_positionStatusLabel->setText(index >= 0
                ? tr("当前编辑：%1")
                  .arg(m_locatorConfig.templates.at(index).name)
                : tr("未选择模板"));
    }
}

void ReferenceImageDialog::addReferenceTemplate()
{
    if (m_referencePositionEditorReadOnly ||
            m_locatorConfig.templates.size() >=
            TemplateLocationConfig::MaximumTemplateCount)
        return;
    // Capture the operator's Base selection before any legacy-bank upgrade or
    // template refresh can move the page back to the active template's Base.
    QString targetBaseId = selectedReferenceBaseId().trimmed();
    if (targetBaseId.isEmpty())
        targetBaseId = ReferenceImageProvider::instance().primaryBaseId();
    stopReferencePositionRoiEditing(true);
    storeLegacyGeometryToActiveTemplate();
    if (m_locatorConfig.templates.size() == 1 &&
            m_locatorConfig.originMode == QStringLiteral("centroid")) {
        const QPointF frozenOrigin = centroidForTemplate(
                    m_locatorConfig.templates.first());
        if (!validNormalizedPoint(frozenOrigin)) {
            if (m_positionStatusLabel)
                m_positionStatusLabel->setText(
                            tr("请先为第一个模板设置有效区域，再添加模板"));
            return;
        }
        m_locatorConfig.originMode = QStringLiteral("custom");
        m_locatorConfig.customOriginNormalized = frozenOrigin;
        m_referencePositionCorrection.originMode = QStringLiteral("custom");
        m_referencePositionCorrection.customOriginNormalized = frozenOrigin;
        if (m_positionOriginModeComboBox) {
            const QSignalBlocker blocker(m_positionOriginModeComboBox);
            m_positionOriginModeComboBox->setCurrentIndex(1);
        }
        updateReferencePositionOriginControls();
    }
    if (m_locatorConfig.version !=
            TemplateLocationConfig::CompositeBankParamsVersion) {
        QString legacyBaseId = ReferenceImageProvider::instance()
                .primaryBaseId().trimmed();
        if (legacyBaseId.isEmpty())
            legacyBaseId = targetBaseId;
        m_locatorConfig = TemplateLocationConfig::upgradeToV6(
                    m_locatorConfig, legacyBaseId);
        synchronizeLocatorBaseBindings(false);
    }
    int nextOrder = 0;
    QSet<QString> names;
    for (const TemplateLocationTemplateConfig &existing :
         qAsConst(m_locatorConfig.templates)) {
        if (existing.sourceBaseId == targetBaseId)
            nextOrder = qMax(nextOrder, existing.order + 1);
        names.insert(existing.name);
    }
    int suffix = m_locatorConfig.templates.size() + 1;
    QString name;
    do {
        name = tr("模板%1").arg(suffix++);
    } while (names.contains(name));

    // A bank item owns independent ROI/mask/model state.  Start blank so the
    // second item cannot silently be the first ROI under a different identity.
    TemplateLocationTemplateConfig item;
    item.templateId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.name = name;
    item.priority = nextOrder;
    item.order = nextOrder;
    item.sourceBaseId = targetBaseId;
    item.modelCacheKey = QStringLiteral(
                "reference.positionCorrection.template.%1")
            .arg(item.templateId);
    item.modelCreated = false;
    item.extra = QJsonObject();
    m_locatorConfig.templates.append(item);
    m_activeTemplateId = item.templateId;
    synchronizeLocatorBaseBindings(false);
    loadActiveTemplateGeometry();
    loadActiveBaseBindingGeometry();
    loadMatchingParametersForActiveTemplate();
    if (!targetBaseId.isEmpty())
        selectReferenceBase(targetBaseId);
    markLocatorDirty(false);
    refreshReferenceTemplateBank();
    if (m_positionStatusLabel)
        m_positionStatusLabel->setText(
                    tr("已添加 %1，请绘制该模板区域").arg(item.name));
}

void ReferenceImageDialog::renameReferenceTemplate()
{
    const int index = activeTemplateIndex();
    if (m_referencePositionEditorReadOnly || index < 0)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(
                this, tr("重命名模板"), tr("模板名称"), QLineEdit::Normal,
                m_locatorConfig.templates.at(index).name, &ok).trimmed();
    if (!ok || name.isEmpty())
        return;
    for (int other = 0; other < m_locatorConfig.templates.size(); ++other) {
        if (other != index &&
                m_locatorConfig.templates.at(other).name == name) {
            if (m_positionStatusLabel)
                m_positionStatusLabel->setText(tr("模板名称不能重复"));
            return;
        }
    }
    m_locatorConfig.templates[index].name = name;
    if (m_referenceLocatorBankEnvelope)
        writeLocatorControls();
    else
        markLocatorDirty(false);
    refreshReferenceTemplateBank();
}

void ReferenceImageDialog::deleteReferenceTemplate()
{
    const int index = activeTemplateIndex();
    if (m_referencePositionEditorReadOnly || index < 0 ||
            m_locatorConfig.templates.size() <= 1)
        return;
    const TemplateLocationTemplateConfig removed =
            m_locatorConfig.templates.at(index);
    if (QMessageBox::question(
            this, tr("删除模板"),
            tr("确定删除“%1”及其区域和模型状态吗？").arg(removed.name),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    stopReferencePositionOriginSelection();
    stopReferencePositionRoiEditing(false);
    // Keep the persistent cache until the configuration transaction has been
    // durably saved.  An orphan is harmless; deleting it here would make a
    // cancelled or failed save destroy the last valid model irreversibly.
    m_locatorConfig.templates.removeAt(index);
    m_activeTemplateId = m_locatorConfig.templates.at(
                qMin(index, m_locatorConfig.templates.size() - 1)).templateId;
    loadActiveTemplateGeometry();
    markLocatorDirty(false);
    refreshReferenceTemplateBank();
}

void ReferenceImageDialog::handleReferenceTemplateItemChanged(
        QListWidgetItem *listItem)
{
    if (m_updatingTemplateBank || m_referencePositionEditorReadOnly || !listItem)
        return;
    const QString id = listItem->data(kReferenceTemplateIdRole).toString();
    TemplateLocationTemplateConfig *item =
            TemplateLocationConfig::findTemplate(&m_locatorConfig, id);
    if (!item)
        return;
    int enabledCount = 0;
    for (const TemplateLocationTemplateConfig &candidate :
         qAsConst(m_locatorConfig.templates)) {
        if (candidate.enabled)
            ++enabledCount;
    }
    const bool enabled = listItem->checkState() == Qt::Checked;
    if (!enabled && item->enabled && enabledCount <= 1) {
        {
            const QSignalBlocker blocker(m_referenceTemplateBankList);
            listItem->setCheckState(Qt::Checked);
        }
        refreshReferenceTemplateBank();
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("至少需要启用一个模板"));
        return;
    }
    if (item->enabled == enabled)
        return;
    item->enabled = enabled;
    markLocatorDirty(false);
    refreshReferenceTemplateBank();
}

void ReferenceImageDialog::updatePositionCorrectionUi(bool enabled)
{
    const bool effectiveEnabled = m_referencePositionEditorReadOnly
            ? m_referencePositionCorrection.enabled : enabled;
    if (!m_referencePositionEditorReadOnly)
        m_referencePositionCorrection.enabled = enabled;
    ui->correctionExampleFrame->setVisible(
                !effectiveEnabled && !m_referencePositionEditorReadOnly);
    if (m_positionSettingsFrame)
        m_positionSettingsFrame->setVisible(
                    effectiveEnabled || m_referencePositionEditorReadOnly);
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
    if (m_referencePositionEditorReadOnly) {
        stopReferencePositionOriginSelection();
        stopReferencePositionRoiEditing(false);
        clearReferencePositionMatchOverlays();
        if (m_previewHelper) {
            m_previewHelper->clearRoi();
            m_previewHelper->clearPolygonRoi();
            m_previewHelper->clearCircleRoi();
        }
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(referencePositionReadOnlyMessage());
        return;
    }
    if (!effectiveEnabled) {
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

QString ReferenceImageDialog::referencePositionReadOnlyMessage() const
{
    const QString detail = m_referencePositionUnsupportedMessage.trimmed();
    return tr("该基准定位配置由当前版本不支持的 locator 合同创建，已按只读方式保留（%1）：%2")
            .arg(m_referencePositionUnsupportedStatus.trimmed().isEmpty()
                 ? QStringLiteral("unsupported_reference_locator")
                 : m_referencePositionUnsupportedStatus,
                 detail.isEmpty() ? tr("请使用支持该合同的版本编辑") : detail);
}

// 切换到静态基准图，并启用矩形拖拽模式；已有矩形会先回显供继续调整。
void ReferenceImageDialog::startReferencePositionRectEditing()
{
    if (!m_previewHelper || !showActiveTemplateBase()) {
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("请先设置基准图"));
        return;
    }

    const bool hadRectangle =
            m_referencePositionCorrection.templateRegionType != QStringLiteral("polygon");
    stopReferencePositionOriginSelection();
    clearReferencePositionMatchOverlays();
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
    if (!m_previewHelper || !showActiveTemplateBase()) {
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("请先设置基准图"));
        return;
    }

    stopReferencePositionOriginSelection();
    clearReferencePositionMatchOverlays();
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
    if (!m_previewHelper || !showActiveTemplateBase()) {
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

void ReferenceImageDialog::startReferenceSearchRegionEditing(
        PositionCorrectionRoiEditMode mode)
{
    if (!m_previewHelper || !showActiveTemplateBase()) {
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("请先设置基准图"));
        return;
    }
    if (mode != PositionCorrectionRoiEditMode::SearchRectangle &&
            mode != PositionCorrectionRoiEditMode::SearchCircle &&
            mode != PositionCorrectionRoiEditMode::SearchPolygon)
        return;
    stopReferencePositionOriginSelection();
    stopReferencePositionRoiEditing(false);
    clearReferencePositionMatchOverlays();
    showReferenceImageMode();
    m_positionRoiEditMode = mode;
    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->clearRoi();
    m_previewHelper->clearPolygonRoi();
    m_previewHelper->clearCircleRoi();
    renderReferencePositionOverlays();
    if (mode == PositionCorrectionRoiEditMode::SearchRectangle) {
        if (m_locatorConfig.searchRegionType == QStringLiteral("rectangle") &&
                validNormalizedRect(m_locatorConfig.searchRoiNormalized)) {
            m_previewHelper->setRoiRectNormalized(
                        m_locatorConfig.searchRoiNormalized);
        }
        m_previewHelper->setRoiDrawingEnabled(true);
        ui->viewerTitleLabel->setText(tr("基准图 - 矩形搜索区域"));
    } else if (mode == PositionCorrectionRoiEditMode::SearchCircle) {
        CircleRoi circle;
        circle.centerNormalized = m_locatorConfig.searchCircleCenterNormalized;
        circle.radiusNormalized = m_locatorConfig.searchCircleRadiusNormalized;
        circle.boundingRectNormalized = m_locatorConfig.searchRoiNormalized;
        circle.valid = circle.radiusNormalized > 0.0 &&
                validNormalizedPoint(circle.centerNormalized);
        if (m_locatorConfig.searchRegionType == QStringLiteral("circle") &&
                circle.valid) {
            m_previewHelper->setCircleRoiNormalized(circle);
        }
        m_previewHelper->setCircleDrawingEnabled(true);
        ui->viewerTitleLabel->setText(tr("基准图 - 圆形搜索区域"));
    } else {
        if (m_locatorConfig.searchRegionType == QStringLiteral("polygon") &&
                m_locatorConfig.searchPolygonNormalized.size() >= 3) {
            m_previewHelper->setPolygonRoiNormalized(
                        m_locatorConfig.searchPolygonNormalized);
        }
        m_previewHelper->setPolygonDrawingEnabled(true);
        ui->viewerTitleLabel->setText(tr("基准图 - 多边形搜索区域"));
    }
    m_positionStatusLabel->setText(tr("请绘制搜索区域，完成后点击搜索区域“完成”"));
}

bool ReferenceImageDialog::finishReferenceSearchRegionEditing()
{
    if (!m_previewHelper)
        return false;
    if (m_positionRoiEditMode ==
            PositionCorrectionRoiEditMode::SearchRectangle) {
        const QRectF roi = m_previewHelper->roiRectNormalized();
        if (!validNormalizedRect(roi)) {
            m_positionStatusLabel->setText(tr("请先绘制有效矩形搜索区域"));
            return false;
        }
        m_locatorConfig.searchRegionType = QStringLiteral("rectangle");
        m_locatorConfig.searchRoiNormalized = roi;
        m_locatorConfig.searchPolygonNormalized.clear();
        m_locatorConfig.searchCircleCenterNormalized = QPointF();
        m_locatorConfig.searchCircleRadiusNormalized = 0.0;
    } else if (m_positionRoiEditMode ==
               PositionCorrectionRoiEditMode::SearchCircle) {
        const CircleRoi circle = m_previewHelper->circleRoiNormalized();
        if (!circle.valid) {
            m_positionStatusLabel->setText(tr("请先绘制有效圆形搜索区域"));
            return false;
        }
        m_locatorConfig.searchRegionType = QStringLiteral("circle");
        m_locatorConfig.searchRoiNormalized = circle.boundingRectNormalized;
        m_locatorConfig.searchPolygonNormalized.clear();
        m_locatorConfig.searchCircleCenterNormalized = circle.centerNormalized;
        m_locatorConfig.searchCircleRadiusNormalized = circle.radiusNormalized;
    } else if (m_positionRoiEditMode ==
               PositionCorrectionRoiEditMode::SearchPolygon) {
        if (m_previewHelper->isPolygonDrawingEnabled() &&
                !m_previewHelper->finishPolygonDrawing()) {
            m_positionStatusLabel->setText(tr("搜索多边形至少需要 3 个点"));
            return false;
        }
        const QVector<QPointF> points =
                m_previewHelper->polygonRoiNormalized();
        if (points.size() < 3) {
            m_positionStatusLabel->setText(tr("搜索多边形至少需要 3 个点"));
            return false;
        }
        m_locatorConfig.searchRegionType = QStringLiteral("polygon");
        m_locatorConfig.searchRoiNormalized = boundingRectForPoints(points);
        m_locatorConfig.searchPolygonNormalized = points;
        m_locatorConfig.searchCircleCenterNormalized = QPointF();
        m_locatorConfig.searchCircleRadiusNormalized = 0.0;
    } else {
        m_positionStatusLabel->setText(tr("请先选择搜索区域形状"));
        return false;
    }
    stopReferencePositionRoiEditing(true);
    markLocatorDirty(false);
    m_positionStatusLabel->setText(tr("搜索区域已确认，请点击“创建/更新基准”重新验证"));
    renderReferencePositionOverlays();
    return true;
}

bool ReferenceImageDialog::finishReferencePositionMaskEditing()
{
    if (!m_previewHelper || selectedReferenceBaseId() != activeTemplateBaseId()
            || ReferenceImageProvider::instance()
               .referenceImage(activeTemplateBaseId()).isNull()) {
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

    m_referenceTemplateExcludeEdited = true;
    storeLegacyGeometryToActiveTemplate();
    markActiveTemplateDirty();
    stopReferencePositionRoiEditing(true);
    m_positionMaskClearButton->setEnabled(true);
    m_positionStatusLabel->setText(
                tr("模板屏蔽区域已确认，请点击“创建/更新基准”重新验证"));
    return true;
}

// 完成当前绘制并校验 ROI；只有有效矩形或至少三个点的多边形可以确认。
bool ReferenceImageDialog::finishReferencePositionRoiEditing()
{
    if (!m_previewHelper || selectedReferenceBaseId() != activeTemplateBaseId()
            || ReferenceImageProvider::instance()
               .referenceImage(activeTemplateBaseId()).isNull()) {
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

    const bool polygonMode =
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::Polygon;
    if (polygonMode) {
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
        m_referencePositionCorrection.templateRegionType =
                QStringLiteral("polygon");
    } else {
        const QRectF roi = m_previewHelper->roiRectNormalized();
        if (!validNormalizedRect(roi)) {
            m_positionStatusLabel->setText(tr("请先在基准图上绘制有效矩形区域"));
            return false;
        }
        m_referencePositionCorrection.templateRoiNormalized = roi;
        m_referencePositionCorrection.templatePolygonNormalized = QJsonArray();
        m_referencePositionCorrection.templateRegionType =
                QStringLiteral("rectangle");
    }

    m_referenceTemplateIncludeEdited = true;
    storeLegacyGeometryToActiveTemplate();
    markActiveTemplateDirty();
    stopReferencePositionRoiEditing(true);
    m_positionStatusLabel->setText(
                tr("%1；请点击“创建/更新基准”重新验证")
                .arg(referencePositionRoiStatusText()));
    return true;
}

// 矩形拖拽完成后实时更新归一化配置，并清除不再适用的多边形数据。
void ReferenceImageDialog::handleReferencePositionRectChanged(const QRectF &roiNormalized)
{
    if (m_positionRoiEditMode ==
            PositionCorrectionRoiEditMode::SearchRectangle &&
            validNormalizedRect(roiNormalized)) {
        m_positionStatusLabel->setText(
                    tr("矩形搜索区域已更新，点击搜索区域“完成”确认"));
        return;
    }
    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskRectangle &&
            validNormalizedRect(roiNormalized)) {
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
    if (m_positionRoiEditMode ==
            PositionCorrectionRoiEditMode::SearchPolygon &&
            pointsNormalized.size() >= 3) {
        m_positionStatusLabel->setText(
                    tr("多边形搜索区域已闭合，点击搜索区域“完成”确认"));
        return;
    }
    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskPolygon &&
            pointsNormalized.size() >= 3) {
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

    if (drawingPolygon) {
        m_positionStatusLabel->setText(
                    tr("多边形 ROI 已闭合，共 %1 个点，点击“完成”确认")
                    .arg(pointsNormalized.size()));
        return;
    }
    m_referencePositionCorrection.templateRegionType = QStringLiteral("polygon");
    m_referencePositionCorrection.templatePolygonNormalized =
            polygonPointsToJson(pointsNormalized);
    m_referencePositionCorrection.templateRoiNormalized =
            boundingRectForPoints(pointsNormalized);
    m_referenceTemplateIncludeEdited = true;
    storeLegacyGeometryToActiveTemplate();
    markActiveTemplateDirty();
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
    if (m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchCircle &&
            circle.valid) {
        m_positionStatusLabel->setText(
                    tr("圆形搜索区域已更新，点击搜索区域“完成”确认"));
        return;
    }
    if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::MaskCircle ||
            !circle.valid) {
        return;
    }
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
            || selectedReferenceBaseId() != activeTemplateBaseId()
            || ReferenceImageProvider::instance()
               .referenceImage(selectedReferenceBaseId()).isNull()) {
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
    // Capture the entire Base set under one provider lock.  Every template in
    // this build must use frames and content revisions from this same atomic
    // generation; per-template provider reads could otherwise mix revisions
    // while a camera/import mutation is arriving.
    const ReferenceFrameSetSnapshot referenceSet =
            ReferenceImageProvider::instance().referenceFrameSetSnapshot();
    if (referenceSet.frames.isEmpty()) {
        clearReferencePositionMatchOverlays();
        // A failed test is not an edit.  Preserve the last local ready
        // candidate atomically and only surface the transient failure in UI.
        m_positionStatusLabel->setText(tr("请先设置基准图"));
        return false;
    }
    // Keep the committed ready state untouched while building candidates.
    // Only an all-template success may replace the bank/pose map atomically.
    const ReferencePositionCorrectionConfig previousReference =
            m_referencePositionCorrection;
    const TemplateLocationModelBankConfig previousBank = m_locatorConfig;
    const bool previousBankEnvelope = m_referenceLocatorBankEnvelope;
    m_referenceLocatorBankEnvelope = true;
    writeLocatorControls();
    QString validationMessage;
    if (!validateLocatorControls(&validationMessage)) {
        m_referencePositionCorrection = previousReference;
        m_locatorConfig = previousBank;
        m_referenceLocatorBankEnvelope = previousBankEnvelope;
        clearReferencePositionMatchOverlays();
        m_positionStatusLabel->setText(validationMessage);
        return false;
    }

    QJsonObject posesByTemplateId;
    QVector<ToolOverlay> allOverlays;
    qint64 totalElapsedMs = 0;
    double minimumScore = 1.0;
    int testedCount = 0;
    QString firstEnabledTemplateId;

    for (const TemplateLocationTemplateConfig &item :
         qAsConst(m_locatorConfig.templates)) {
        if (!item.enabled)
            continue;
        if (firstEnabledTemplateId.isEmpty())
            firstEnabledTemplateId = item.templateId;
        TemplateLocationModelBankConfig single = m_locatorConfig;
        TemplateLocationTemplateConfig buildItem = item;
        buildItem.enabled = true;
        buildItem.modelCreated = true;
        single.templates = QVector<TemplateLocationTemplateConfig>{buildItem};
        const QString sourceBaseId = m_locatorConfig.version ==
                TemplateLocationConfig::CompositeBankParamsVersion
                ? item.sourceBaseId
                : referenceSet.primaryBaseId;
        const cv::Mat sourceFrame = referenceSet.frames.value(sourceBaseId);
        if (sourceFrame.empty()) {
            m_referencePositionCorrection = previousReference;
            m_locatorConfig = previousBank;
            m_referenceLocatorBankEnvelope = previousBankEnvelope;
            clearReferencePositionMatchOverlays();
            m_positionStatusLabel->setText(
                        tr("模板“%1”绑定的 Base 不存在或图像为空")
                        .arg(item.name));
            return false;
        }
        if (single.version ==
                TemplateLocationConfig::CompositeBankParamsVersion) {
            const TemplateLocationBaseBindingConfig *configuredBinding =
                    TemplateLocationConfig::findBaseBinding(
                        m_locatorConfig, sourceBaseId);
            if (!configuredBinding) {
                m_referencePositionCorrection = previousReference;
                m_locatorConfig = previousBank;
                m_referenceLocatorBankEnvelope = previousBankEnvelope;
                m_positionStatusLabel->setText(
                            tr("模板“%1”缺少 Base 绑定配置").arg(item.name));
                return false;
            }
            single.baseBindings = {
                *configuredBinding
            };
            single.primaryMatchStrategy = QStringLiteral("first_valid");
            single.primaryTemplateId.clear();
        } else {
            single.version = TemplateLocationConfig::ModelBankParamsVersion;
            single.primaryMatchStrategy = QStringLiteral("best_score");
            single.primaryTemplateId = item.templateId;
        }
        // A reference self-test must freeze this exact source instance.  Reuse
        // of the user's runtime search ROI (especially full image) can select a
        // stronger repeated texture elsewhere and bind the wrong reference
        // pose.  Search only a small clamped neighbourhood around this item's
        // own template region.
        const QRectF bounds = boundsForTemplate(item).normalized();
        const double padX = qMax(2.0 / sourceFrame.cols,
                                 bounds.width() * 0.1);
        const double padY = qMax(2.0 / sourceFrame.rows,
                                 bounds.height() * 0.1);
        const double left = qMax(0.0, bounds.left() - padX);
        const double top = qMax(0.0, bounds.top() - padY);
        const double right = qMin(1.0, bounds.right() + padX);
        const double bottom = qMin(1.0, bounds.bottom() + padY);
        const QRectF localSearch(left, top, right - left, bottom - top);
        single.searchRegionType = QStringLiteral("rectangle");
        single.searchRoiNormalized = localSearch;
        single.searchPolygonNormalized.clear();
        single.searchCircleCenterNormalized = QPointF();
        single.searchCircleRadiusNormalized = 0.0;
        single.maxMatches = 1;
        single.minMatchCount = 1;
        single.maxMatchCount = 1;
        single.fusion.enabled = false;
        if (single.version ==
                TemplateLocationConfig::CompositeBankParamsVersion) {
            TemplateLocationBaseBindingConfig &binding =
                    single.baseBindings[0];
            binding.searchRegionType = QStringLiteral("rectangle");
            binding.searchRoiNormalized = localSearch;
            binding.searchPolygonNormalized.clear();
            binding.searchCircleCenterNormalized = QPointF();
            binding.searchCircleRadiusNormalized = 0.0;
        }
        const TemplateLocationHalconResult result = single.version ==
                TemplateLocationConfig::CompositeBankParamsVersion
                ? m_referencePositionRunner.run(
                      sourceFrame,
                      referenceSet.frames,
                      referenceSet.contentRevisions,
                      single, sourceBaseId)
                : m_referencePositionRunner.run(
                      sourceFrame, sourceFrame, single);
        totalElapsedMs += result.elapsedMs;
        const QJsonObject frozenPose = referencePoseJson(result.payload);
        QString identityMessage;
        const bool validIdentity = validLocatorIdentityPayload(result.payload) &&
                ReferenceTemplateLocationConfig::validateFrozenPose(
                    frozenPose, item.templateId, single, &identityMessage);
        if (!result.success || !result.ok ||
                !validPosePayload(result.payload) || !validIdentity) {
            const QString failureStatus = validIdentity
                    ? result.status
                    : QStringLiteral("invalid_reference_locator_identity");
            const QString failureMessage = validIdentity
                    ? result.message
                    : (identityMessage.trimmed().isEmpty()
                       ? tr("模板自匹配结果缺少完整身份") : identityMessage);
            m_referencePositionCorrection = previousReference;
            m_locatorConfig = previousBank;
            m_referenceLocatorBankEnvelope = previousBankEnvelope;
            clearReferencePositionMatchOverlays();
            refreshReferenceTemplateBank();
            m_positionStatusLabel->setText(
                        tr("模板“%1”自匹配失败：%2")
                        .arg(item.name,
                             failureMessage
                             .trimmed().isEmpty()
                             ? failureStatus : failureMessage));
            return false;
        }
        posesByTemplateId.insert(item.templateId, frozenPose);
        minimumScore = qMin(minimumScore, result.score);
        ++testedCount;
        for (ToolOverlay overlay : result.overlays) {
            if (overlay.extra.value(QStringLiteral("matchIndex")).toInt(-1) != 0 ||
                    (overlay.label != QStringLiteral("match_result") &&
                     overlay.label != QStringLiteral("match_center"))) {
                continue;
            }
            overlay.extra.insert(QStringLiteral("templateId"), item.templateId);
            overlay.extra.insert(QStringLiteral("templateName"), item.name);
            overlay.extra.insert(QStringLiteral("sourceBaseId"), sourceBaseId);
            allOverlays.append(overlay);
        }
    }

    QString inconsistentTemplate;
    QString consistencyMessage;
    if (!ReferenceTemplateBankValidation::consistentFrozenPoses(
            m_locatorConfig, posesByTemplateId,
            &inconsistentTemplate, &consistencyMessage)) {
        m_referencePositionCorrection = previousReference;
        m_locatorConfig = previousBank;
        m_referenceLocatorBankEnvelope = previousBankEnvelope;
        clearReferencePositionMatchOverlays();
        refreshReferenceTemplateBank();
        m_positionStatusLabel->setText(
                    tr("模板“%1”公共位姿校验失败：%2")
                    .arg(inconsistentTemplate, consistencyMessage));
        return false;
    }

    for (TemplateLocationTemplateConfig &item : m_locatorConfig.templates) {
        if (item.enabled)
            item.modelCreated = true;
    }
    m_referencePositionCorrection.version = m_locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion
            ? 5 : ReferenceTemplateLocationConfig::ModelBankEnvelopeVersion;
    m_referencePositionCorrection.referenceCreated = true;
    m_referencePositionCorrection.referencePosesByTemplateId = posesByTemplateId;
    m_referencePositionCorrection.referencePose =
            posesByTemplateId.value(firstEnabledTemplateId).toObject();
    m_referencePositionCorrection.modelCacheKey = activeTemplateIndex() >= 0
            ? m_locatorConfig.templates.at(activeTemplateIndex()).modelCacheKey
            : QString();
    m_referencePositionCorrection.status = QStringLiteral("reference_bank_ready");
    m_referencePositionCorrection.message =
            tr("%1 个启用模板均已通过自匹配").arg(testedCount);
    m_referencePositionCorrection.score = minimumScore;
    m_referencePositionCorrection.elapsedMs = totalElapsedMs;
    writeLocatorControls();
    m_referencePositionMatchOverlays = allOverlays;
    renderReferencePositionOverlays();
    refreshReferenceTemplateBank();
    m_positionStatusLabel->setText(
                tr("基准定位可用：%1 个模板，最低分数 %2%，耗时 %3 ms")
                .arg(testedCount)
                .arg(minimumScore * 100.0, 0, 'f', 1)
                .arg(totalElapsedMs));
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
            || selectedReferenceBaseId() != activeTemplateBaseId()
            || ReferenceImageProvider::instance()
               .referenceImage(selectedReferenceBaseId()).isNull()) {
        m_previewHelper->clearToolOverlays();
        return;
    }

    QVector<ToolOverlay> overlays;
    const QImage image = ReferenceImageProvider::instance().referenceImage(
                selectedReferenceBaseId());
    const bool editingMask =
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskRectangle ||
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskCircle ||
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskPolygon;
    const bool editingSearch =
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchRectangle ||
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchCircle ||
            m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchPolygon;
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
    if (!editingSearch &&
            m_locatorConfig.searchRegionType != QStringLiteral("full")) {
        CircleRoi searchCircle;
        searchCircle.centerNormalized =
                m_locatorConfig.searchCircleCenterNormalized;
        searchCircle.radiusNormalized =
                m_locatorConfig.searchCircleRadiusNormalized;
        searchCircle.boundingRectNormalized =
                m_locatorConfig.searchRoiNormalized;
        searchCircle.valid = searchCircle.radiusNormalized > 0.0 &&
                validNormalizedPoint(searchCircle.centerNormalized);
        appendReferenceRegionOverlay(
                    &overlays, m_locatorConfig.searchRegionType,
                    m_locatorConfig.searchRoiNormalized,
                    m_locatorConfig.searchPolygonNormalized,
                    searchCircle, image.size(),
                    QStringLiteral("detect_roi"),
                    QStringLiteral("template_location_search_roi"));
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
    for (const ToolOverlay &overlay : qAsConst(m_referencePositionMatchOverlays)) {
        const QString overlayBaseId = overlay.extra.value(
                    QStringLiteral("sourceBaseId")).toString();
        if (overlayBaseId.isEmpty() ||
                overlayBaseId == selectedReferenceBaseId()) {
            overlays.append(overlay);
        }
    }

    if (overlays.isEmpty())
        m_previewHelper->clearToolOverlays();
    else
        m_previewHelper->setToolOverlays(overlays);
}

void ReferenceImageDialog::saveCurrentSchemeAs()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("另存为"),
                                               tr("新方案名"),
                                               QLineEdit::Normal,
                                               SchemeStore::instance().currentSchemeName() + tr("_副本"),
                                               &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    SchemeStore &store = SchemeStore::instance();
    const ReferencePositionCorrectionConfig originalReference =
            store.currentScheme().referencePositionCorrection;
    if (m_positionRoiEditMode != PositionCorrectionRoiEditMode::None) {
        const bool maskEditing =
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskRectangle ||
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskCircle ||
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::MaskPolygon;
        const bool searchEditing =
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchRectangle ||
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchCircle ||
                m_positionRoiEditMode == PositionCorrectionRoiEditMode::SearchPolygon;
        if (!(maskEditing ? finishReferencePositionMaskEditing()
                          : searchEditing ? finishReferenceSearchRegionEditing()
                                          : finishReferencePositionRoiEditing()))
            return;
    }
    if (!m_referencePositionEditorReadOnly)
        writeLocatorControls();
    store.setReferencePositionCorrection(m_referencePositionCorrection);

    QString error;
    if (!store.saveCurrentSchemeAs(name, &error)) {
        qWarning() << "[ReferenceImageDialog] 方案另存为失败:" << error;
        store.setReferencePositionCorrection(originalReference);
        loadPositionCorrectionConfig();
        refreshSchemeHeader();
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
        if (m_positionStatusLabel) {
            m_positionStatusLabel->setText(
                        tr("当前没有可抓取的相机帧，请确认相机正在采集"));
        }
        if (m_previewHelper) {
            m_previewHelper->clear();
        }
        return;
    }

    if (!persistReferenceFrameForSelectedBase(
            frame, snapshot.metadata,
            m_captureCreatesNewBase || selectedReferenceBaseId().isEmpty(),
            m_captureCreatesNewBase
            ? tr("相机 Base %1")
              .arg(SchemeStore::instance().referenceAssets().size() + 1)
            : QString())) {
        return;
    }
    m_captureCreatesNewBase = false;
    qDebug() << QString("[ReferenceImageDialog] 已抓取 Base %1: %2x%3 type=%4")
                    .arg(selectedReferenceBaseId())
                    .arg(frame.cols)
                    .arg(frame.rows)
                    .arg(frame.type());
}

void ReferenceImageDialog::showReferenceImageMode()
{
    m_liveCaptureMode = false;
    m_captureCreatesNewBase = false;
    updateReferenceImageControls();
    refreshReferenceImage();
}

void ReferenceImageDialog::importReferenceImageFromPc()
{
    const ReferenceAssetSet &assets =
            SchemeStore::instance().currentScheme().referenceAssets;
    const bool addForMultiBase = assets.multiBaseEnabled();
    if (addForMultiBase &&
            assets.size() >= ReferenceAssetSet::kMaximumAssets) {
        QMessageBox::information(
                    this, tr("PC导入"),
                    tr("多基准最多允许 %1 张，请先删除不再使用的 Base 或使用“替换”。")
                    .arg(ReferenceAssetSet::kMaximumAssets));
        return;
    }
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          tr("PC导入基准图"),
                                                          QString(),
                                                          tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (fileName.isEmpty())
        return;

    FrameInputMetadata metadata;
    cv::Mat bgrFrame;
    QString message;
    if (!loadImageFileAsBgr(fileName, &bgrFrame, &metadata, &message)) {
        ui->viewerTitleLabel->setText(tr("图片导入失败"));
        QMessageBox::warning(this, tr("图片导入失败"), message);
        return;
    }
    if (!persistReferenceFrameForSelectedBase(
            bgrFrame, metadata,
            addForMultiBase || selectedReferenceBaseId().isEmpty(),
            QFileInfo(fileName).completeBaseName())) {
        return;
    }
    qDebug() << QString("[ReferenceImageDialog] 已导入 PC Base %1: %2 size=%3x%4 type=%5")
                    .arg(selectedReferenceBaseId())
                    .arg(fileName)
                    .arg(bgrFrame.cols)
                    .arg(bgrFrame.rows)
                    .arg(bgrFrame.type());
}

void ReferenceImageDialog::setupReferenceImageControls()
{
    setupReferenceAssetControls();

    m_captureImageButton = new QPushButton(tr("抓取图像"), this);
    m_captureImageButton->setObjectName(QStringLiteral("captureImageButton"));
    m_captureImageButton->setProperty("actionRole", QStringLiteral("highlight"));
    m_captureImageButton->setMinimumSize(150, 46);
    m_captureImageButton->setIcon(QIcon(QStringLiteral(":/icons/camera.svg")));
    m_captureImageButton->setIconSize(QSize(24, 24));
    ui->horizontalLayout_referenceButtons->addWidget(m_captureImageButton);

    m_exitCaptureButton = new QPushButton(tr("退出相机抓取"), this);
    m_exitCaptureButton->setObjectName(QStringLiteral("exitCaptureButton"));
    m_exitCaptureButton->setProperty("actionRole", QStringLiteral("secondary"));
    m_exitCaptureButton->setMinimumSize(150, 46);
    m_exitCaptureButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    m_exitCaptureButton->setIconSize(QSize(24, 24));
    ui->horizontalLayout_referenceButtons->addWidget(m_exitCaptureButton);

    updateReferenceImageControls();
}

void ReferenceImageDialog::setupReferenceAssetControls()
{
    m_multiReferenceCard = new QFrame(ui->referenceParamsScrollContent);
    m_multiReferenceCard->setObjectName(QStringLiteral("multiReferenceBaseCard"));
    m_multiReferenceCard->setProperty("card", true);
    auto *switchLayout = new QHBoxLayout(m_multiReferenceCard);
    switchLayout->setContentsMargins(18, 16, 18, 16);
    switchLayout->setSpacing(12);
    auto *switchTitle = new QLabel(tr("多基准"), m_multiReferenceCard);
    switchTitle->setObjectName(QStringLiteral("multiReferenceBaseTitleLabel"));
    switchTitle->setProperty("role", QStringLiteral("cardTitle"));
    m_multiReferenceBaseSwitch = new QCheckBox(m_multiReferenceCard);
    m_multiReferenceBaseSwitch->setObjectName(
                QStringLiteral("multiReferenceBaseSwitch"));
    m_multiReferenceBaseSwitch->setToolTip(
                tr("打开后，当前图像和 PC 导入会新增 Base，并在画布下方显示缩略图栏"));
    switchLayout->addWidget(switchTitle);
    switchLayout->addStretch(1);
    switchLayout->addWidget(m_multiReferenceBaseSwitch);
    ui->verticalLayout_2->insertWidget(0, m_multiReferenceCard);

    m_referenceBaseThumbnailPanel = new QFrame(ui->setupViewerFrame);
    m_referenceBaseThumbnailPanel->setObjectName(
                QStringLiteral("referenceBaseThumbnailPanel"));
    m_referenceBaseThumbnailPanel->setMinimumHeight(166);
    m_referenceBaseThumbnailPanel->setMaximumHeight(184);
    auto *layout = new QVBoxLayout(m_referenceBaseThumbnailPanel);
    layout->setContentsMargins(12, 9, 12, 10);
    layout->setSpacing(7);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(7);
    auto *title = new QLabel(tr("基准图"), m_referenceBaseThumbnailPanel);
    title->setObjectName(QStringLiteral("referenceBaseThumbnailTitleLabel"));
    m_referenceBaseSummaryLabel = new QLabel(m_referenceBaseThumbnailPanel);
    m_referenceBaseSummaryLabel->setObjectName(
                QStringLiteral("referenceBaseSummaryLabel"));
    header->addWidget(title);
    header->addWidget(m_referenceBaseSummaryLabel);
    header->addStretch(1);

    m_addReferenceBaseButton = new QPushButton(tr("+ PC"), m_referenceBaseThumbnailPanel);
    m_addReferenceBaseButton->setObjectName(
                QStringLiteral("addReferenceBaseButton"));
    m_addCurrentReferenceBaseButton = new QPushButton(
                tr("+ 当前图"), m_referenceBaseThumbnailPanel);
    m_addCurrentReferenceBaseButton->setObjectName(
                QStringLiteral("addCurrentReferenceBaseButton"));
    m_replaceReferenceBaseButton = new QPushButton(
                tr("替换"), m_referenceBaseThumbnailPanel);
    m_replaceReferenceBaseButton->setObjectName(
                QStringLiteral("replaceReferenceBaseButton"));
    m_deleteReferenceBaseButton = new QPushButton(
                tr("删除"), m_referenceBaseThumbnailPanel);
    m_deleteReferenceBaseButton->setObjectName(
                QStringLiteral("deleteReferenceBaseButton"));
    m_makePrimaryReferenceBaseButton = new QPushButton(
                tr("设为主"), m_referenceBaseThumbnailPanel);
    m_makePrimaryReferenceBaseButton->setObjectName(
                QStringLiteral("makePrimaryReferenceBaseButton"));
    m_addReferenceBaseButton->setIcon(QIcon(QStringLiteral(":/icons/add.svg")));
    m_addCurrentReferenceBaseButton->setIcon(QIcon(QStringLiteral(":/icons/camera.svg")));
    m_replaceReferenceBaseButton->setIcon(QIcon(QStringLiteral(":/icons/refresh.svg")));
    m_deleteReferenceBaseButton->setIcon(QIcon(QStringLiteral(":/icons/trash.svg")));
    m_makePrimaryReferenceBaseButton->setIcon(QIcon(QStringLiteral(":/icons/lock.svg")));
    const QList<QPushButton *> thumbnailActions{
        m_addReferenceBaseButton,
        m_addCurrentReferenceBaseButton,
        m_replaceReferenceBaseButton,
        m_makePrimaryReferenceBaseButton,
        m_deleteReferenceBaseButton
    };
    for (QPushButton *button : thumbnailActions) {
        button->setProperty("thumbnailAction", true);
        button->setMinimumHeight(30);
        button->setMaximumHeight(30);
        header->addWidget(button);
    }
    // Primary acquisition remains in the left “基准图” card, matching the
    // setup flow. Keep these legacy hooks hidden for compatibility tests and
    // automation; the bottom bar itself is selection/management only.
    m_addReferenceBaseButton->hide();
    m_addCurrentReferenceBaseButton->hide();
    layout->addLayout(header);

    m_referenceBaseList = new QListWidget(m_referenceBaseThumbnailPanel);
    m_referenceBaseList->setObjectName(
                QStringLiteral("referenceBaseListWidget"));
    m_referenceBaseList->setSelectionMode(
                QAbstractItemView::SingleSelection);
    m_referenceBaseList->setViewMode(QListView::IconMode);
    m_referenceBaseList->setFlow(QListView::LeftToRight);
    m_referenceBaseList->setWrapping(false);
    m_referenceBaseList->setMovement(QListView::Static);
    m_referenceBaseList->setResizeMode(QListView::Adjust);
    m_referenceBaseList->setUniformItemSizes(true);
    m_referenceBaseList->setTextElideMode(Qt::ElideRight);
    m_referenceBaseList->setIconSize(QSize(142, 76));
    m_referenceBaseList->setGridSize(QSize(164, 108));
    m_referenceBaseList->setSpacing(5);
    m_referenceBaseList->setHorizontalScrollBarPolicy(
                Qt::ScrollBarAsNeeded);
    m_referenceBaseList->setVerticalScrollBarPolicy(
                Qt::ScrollBarAlwaysOff);
    m_referenceBaseList->setMinimumHeight(114);
    m_referenceBaseList->setMaximumHeight(122);
    layout->addWidget(m_referenceBaseList);
    ui->verticalLayout_viewer->addWidget(m_referenceBaseThumbnailPanel);
    ui->verticalLayout_viewer->setStretch(0, 0);
    ui->verticalLayout_viewer->setStretch(1, 1);
    ui->verticalLayout_viewer->setStretch(2, 0);
    ui->verticalLayout_viewer->setStretch(3, 0);
    ui->referenceHintLabel->setText(
                tr("关闭多基准时当前图像和 PC 导入替换主 Base；打开后用于新增 Base。"));

    connect(m_multiReferenceBaseSwitch, &QCheckBox::toggled,
            this, [this](bool enabled) {
        if (m_updatingReferenceAssets)
            return;
        QString error;
        if (!SchemeStore::instance().setMultiReferenceEnabled(
                    enabled, &error)) {
            const QSignalBlocker blocker(m_multiReferenceBaseSwitch);
            m_multiReferenceBaseSwitch->setChecked(!enabled);
            QMessageBox::warning(this, tr("多基准"), error);
            return;
        }
        if (!enabled) {
            const QString primaryId = SchemeStore::instance()
                    .currentScheme().referenceAssets.primaryBaseId();
            if (!primaryId.isEmpty())
                m_selectedReferenceBaseId = primaryId;
        }
        if (m_liveCaptureMode) {
            m_captureCreatesNewBase = enabled &&
                    SchemeStore::instance().referenceAssets().size() <
                    ReferenceAssetSet::kMaximumAssets;
        }
        refreshReferenceAssetControls();
        updateReferenceImageControls();
        if (!m_liveCaptureMode)
            refreshReferenceImage();
    });

    connect(m_referenceBaseList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current, QListWidgetItem *) {
        if (m_updatingReferenceAssets || !current)
            return;
        selectReferenceBase(
                    current->data(kReferenceBaseIdRole).toString());
    });
    connect(m_addReferenceBaseButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::addReferenceBaseFromPc);
    connect(m_addCurrentReferenceBaseButton, &QPushButton::clicked,
            this, [this]() {
        m_captureCreatesNewBase = true;
        showCurrentImageMode();
    });
    connect(m_replaceReferenceBaseButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::replaceSelectedReferenceBaseFromPc);
    connect(m_deleteReferenceBaseButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::removeSelectedReferenceBase);
    connect(m_makePrimaryReferenceBaseButton, &QPushButton::clicked,
            this, &ReferenceImageDialog::makeSelectedReferenceBasePrimary);
}

void ReferenceImageDialog::updateMultiReferenceUi()
{
    const ReferenceAssetSet &assets =
            SchemeStore::instance().currentScheme().referenceAssets;
    const bool enabled = assets.multiBaseEnabled();
    if (m_referenceBaseThumbnailPanel)
        m_referenceBaseThumbnailPanel->setVisible(enabled);
    if (m_referenceBaseList) {
        m_referenceBaseList->setEnabled(
                    enabled && !m_liveCaptureMode &&
                    m_positionRoiEditMode == PositionCorrectionRoiEditMode::None);
    }
    if (m_multiReferenceBaseSwitch) {
        const QSignalBlocker blocker(m_multiReferenceBaseSwitch);
        m_multiReferenceBaseSwitch->setChecked(enabled);
        m_multiReferenceBaseSwitch->setEnabled(!assets.isReadOnly());
    }
}

QString ReferenceImageDialog::selectedReferenceBaseId() const
{
    const QString selected = m_selectedReferenceBaseId.trimmed();
    if (!selected.isEmpty() &&
            ReferenceImageProvider::instance().hasReferenceFrame(selected)) {
        return selected;
    }
    return ReferenceImageProvider::instance().primaryBaseId();
}

QString ReferenceImageDialog::activeTemplateBaseId() const
{
    const int index = activeTemplateIndex();
    if (index >= 0 && m_locatorConfig.version ==
            TemplateLocationConfig::CompositeBankParamsVersion) {
        // Empty/unavailable is meaningful invalid v6 state.  Falling back to
        // the primary image here would display and edit the wrong Base while
        // leaving the serialized source identity unresolved.
        return m_locatorConfig.templates.at(index).sourceBaseId.trimmed();
    }
    return ReferenceImageProvider::instance().primaryBaseId();
}

ReferenceFrameSnapshot ReferenceImageDialog::selectedReferenceSnapshot() const
{
    return ReferenceImageProvider::instance().referenceFrameSnapshot(
                selectedReferenceBaseId());
}

bool ReferenceImageDialog::showActiveTemplateBase()
{
    const QString baseId = activeTemplateBaseId();
    if (baseId.isEmpty() ||
            !ReferenceImageProvider::instance().hasReferenceFrame(baseId)) {
        if (m_positionStatusLabel)
            m_positionStatusLabel->setText(tr("当前模板尚未绑定可用 Base"));
        return false;
    }
    selectReferenceBase(baseId);
    showReferenceImageMode();
    return true;
}

void ReferenceImageDialog::refreshReferenceAssetControls()
{
    if (!m_referenceBaseList)
        return;
    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        m_referenceBaseSummaryLabel->setText(tr("参考资产加载失败：%1").arg(error));
        return;
    }
    const QVector<ReferenceAsset> assets = store.referenceAssets();
    const QString primaryId = store.currentScheme()
            .referenceAssets.primaryBaseId();
    bool selectedExists = false;
    for (const ReferenceAsset &asset : assets) {
        if (asset.baseId == m_selectedReferenceBaseId) {
            selectedExists = true;
            break;
        }
    }
    if (!selectedExists) {
        m_selectedReferenceBaseId = !primaryId.isEmpty()
                ? primaryId
                : assets.isEmpty() ? QString() : assets.first().baseId;
    }

    m_updatingReferenceAssets = true;
    const QSignalBlocker blocker(m_referenceBaseList);
    m_referenceBaseList->clear();
    int selectedRow = -1;
    for (int index = 0; index < assets.size(); ++index) {
        const ReferenceAsset &asset = assets.at(index);
        const bool primary = asset.baseId == primaryId;
        auto *item = new QListWidgetItem(
                    tr("B%1 · %2%3")
                    .arg(index + 1, 2, 10, QLatin1Char('0'))
                    .arg(asset.name.trimmed().isEmpty()
                         ? tr("Base %1").arg(index + 1) : asset.name)
                    .arg(primary ? tr(" · 主") : QString()),
                    m_referenceBaseList);
        item->setData(kReferenceBaseIdRole, asset.baseId);
        const QImage image = ReferenceImageProvider::instance()
                .referenceImage(asset.baseId);
        if (!image.isNull()) {
            item->setIcon(QIcon(QPixmap::fromImage(
                image.scaled(m_referenceBaseList->iconSize(),
                             Qt::KeepAspectRatio,
                             Qt::SmoothTransformation))));
        }
        item->setSizeHint(m_referenceBaseList->gridSize());
        item->setToolTip(tr("Base ID：%1\n内容版本：%2")
                         .arg(asset.baseId, asset.contentRevision));
        if (asset.baseId == m_selectedReferenceBaseId)
            selectedRow = index;
    }
    if (selectedRow >= 0)
        m_referenceBaseList->setCurrentRow(selectedRow);
    m_updatingReferenceAssets = false;

    const bool assetSetReadOnly = store.currentScheme()
            .referenceAssets.isReadOnly();
    const bool hasSelection = selectedRow >= 0;
    const bool selectedPrimary = hasSelection &&
            m_selectedReferenceBaseId == primaryId;
    m_referenceBaseSummaryLabel->setText(assets.isEmpty()
            ? tr("当前：0 / 8")
            : tr("当前：%1 / 8 · 共 %2 张%3")
              .arg(selectedRow + 1)
              .arg(assets.size())
              .arg(selectedPrimary ? tr(" · 主 Base") : QString()));
    m_addReferenceBaseButton->setEnabled(
                !assetSetReadOnly && assets.size() <
                ReferenceAssetSet::kMaximumAssets);
    m_addCurrentReferenceBaseButton->setEnabled(
                !assetSetReadOnly && assets.size() <
                ReferenceAssetSet::kMaximumAssets);
    m_replaceReferenceBaseButton->setEnabled(
                !assetSetReadOnly && hasSelection);
    // Unknown locator contracts may refer to a Base in fields this editor
    // cannot inspect. Pixel replacement remains recoverable, but identity
    // changes are kept disabled until a compatible editor is used.
    const bool identityEditable = !assetSetReadOnly &&
            !m_referencePositionEditorReadOnly;
    m_deleteReferenceBaseButton->setEnabled(
                identityEditable && hasSelection && !selectedPrimary &&
                assets.size() > 1);
    m_makePrimaryReferenceBaseButton->setEnabled(
                identityEditable && hasSelection && !selectedPrimary);
    updateMultiReferenceUi();
    updateReferenceImageControls();
    if (m_templateSourceBaseComboBox)
        refreshReferenceTemplateBank();
}

void ReferenceImageDialog::selectReferenceBase(const QString &baseId)
{
    const QString id = baseId.trimmed();
    if (id.isEmpty() ||
            !ReferenceImageProvider::instance().hasReferenceFrame(id))
        return;
    stopReferencePositionOriginSelection();
    stopReferencePositionRoiEditing(false);
    m_selectedReferenceBaseId = id;
    if (m_referenceBaseList) {
        const QSignalBlocker blocker(m_referenceBaseList);
        for (int row = 0; row < m_referenceBaseList->count(); ++row) {
            if (m_referenceBaseList->item(row)
                    ->data(kReferenceBaseIdRole).toString() == id) {
                m_referenceBaseList->setCurrentRow(row);
                break;
            }
        }
    }
    refreshReferenceAssetControls();
    if (!m_liveCaptureMode)
        refreshReferenceImage();
}

void ReferenceImageDialog::addReferenceBaseFromPc()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this, tr("增加 Base"), QString(),
                tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (fileName.isEmpty())
        return;
    cv::Mat frame;
    FrameInputMetadata metadata;
    QString message;
    if (!loadImageFileAsBgr(fileName, &frame, &metadata, &message)) {
        QMessageBox::warning(this, tr("图片导入失败"), message);
        return;
    }
    persistReferenceFrameForSelectedBase(
                frame, metadata, true, QFileInfo(fileName).completeBaseName());
}

void ReferenceImageDialog::replaceSelectedReferenceBaseFromPc()
{
    if (selectedReferenceBaseId().isEmpty()) {
        addReferenceBaseFromPc();
        return;
    }
    const QString fileName = QFileDialog::getOpenFileName(
                this, tr("替换当前 Base"), QString(),
                tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (fileName.isEmpty())
        return;
    cv::Mat frame;
    FrameInputMetadata metadata;
    QString message;
    if (!loadImageFileAsBgr(fileName, &frame, &metadata, &message)) {
        QMessageBox::warning(this, tr("图片导入失败"), message);
        return;
    }
    persistReferenceFrameForSelectedBase(frame, metadata, false);
}

bool ReferenceImageDialog::persistReferenceFrameForSelectedBase(
        const cv::Mat &frame,
        const FrameInputMetadata &metadata,
        bool forceAdd,
        const QString &suggestedName)
{
    if (frame.empty())
        return false;
    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error)) {
        QMessageBox::warning(this, tr("基准图保存失败"), error);
        return false;
    }
    const QString targetBaseId = selectedReferenceBaseId();
    const bool add = forceAdd || targetBaseId.isEmpty();
    if (!saveReferenceDraftBeforeAssetMutation(&error)) {
        QMessageBox::warning(this, tr("基准图保存失败"),
                             tr("修改 Base 前保存当前定位草稿失败：%1")
                             .arg(error));
        return false;
    }
    const ReferencePositionCorrectionConfig persistedDraft =
            store.currentScheme().referencePositionCorrection;

    if (!add && !m_referencePositionEditorReadOnly) {
        // Merge local, Base-scoped invalidation into the same candidate that
        // replaceReferenceAsset persists.  There is intentionally no second
        // scheme save after the atomic pixel mutation succeeds.
        invalidateLocatorForBase(targetBaseId);
        store.setReferencePositionCorrection(m_referencePositionCorrection);
    }

    QString createdBaseId;
    const bool saved = add
            ? store.addReferenceAsset(
                  suggestedName.trimmed().isEmpty()
                  ? tr("基准图 %1").arg(store.referenceAssets().size() + 1)
                  : suggestedName,
                  frame, &createdBaseId, &error, metadata)
            : store.replaceReferenceAsset(
                  targetBaseId, frame, &error, metadata);
    if (!saved) {
        // The pre-mutation draft was already committed.  Reload exactly that
        // state rather than restoring an older in-memory/store snapshot.
        store.setReferencePositionCorrection(persistedDraft);
        loadPositionCorrectionConfig();
        QMessageBox::warning(this, tr("基准图保存失败"),
                             tr("基准图保存失败：%1").arg(error));
        return false;
    }

    if (add) {
        m_selectedReferenceBaseId = createdBaseId;
        // An unused scheme asset is independent of locator semantics.  Do not
        // promote v4/v5 to v6 and do not invalidate any template merely
        // because another Base became available.
    }
    reloadReferenceStateAfterImageChange();
    refreshReferenceAssetControls();
    showReferenceImageMode();
    return true;
}

void ReferenceImageDialog::removeSelectedReferenceBase()
{
    SchemeStore &store = SchemeStore::instance();
    const QString target = selectedReferenceBaseId();
    if (target.isEmpty())
        return;
    ReferenceAsset selectedAsset;
    if (!store.referenceAsset(target, &selectedAsset))
        return;
    if (QMessageBox::question(
            this, tr("删除 Base"),
            tr("确定删除未被模板引用的“%1”吗？如果仍有模板绑定该 Base，请先改绑或删除对应模板。")
            .arg(selectedAsset.name),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    QString error;
    if (!saveReferenceDraftBeforeAssetMutation(&error)) {
        QMessageBox::warning(this, tr("删除 Base 失败"),
                             tr("删除前保存当前定位草稿失败：%1").arg(error));
        return;
    }
    if (!store.removeReferenceAsset(target, &error)) {
        QMessageBox::warning(
                    this, tr("删除 Base 失败"),
                    tr("%1\n请先删除或改绑使用该 Base 的模板，再执行删除。")
                    .arg(error));
        return;
    }
    m_selectedReferenceBaseId = store.currentScheme()
            .referenceAssets.primaryBaseId();
    reloadReferenceStateAfterImageChange();
    refreshReferenceAssetControls();
    showReferenceImageMode();
}

void ReferenceImageDialog::makeSelectedReferenceBasePrimary()
{
    const QString target = selectedReferenceBaseId();
    if (target.isEmpty())
        return;
    QString error;
    if (!saveReferenceDraftBeforeAssetMutation(&error)) {
        QMessageBox::warning(this, tr("设置主 Base 失败"),
                             tr("设置前保存当前定位草稿失败：%1").arg(error));
        return;
    }
    if (!SchemeStore::instance().setPrimaryReferenceAsset(target, &error)) {
        QMessageBox::warning(this, tr("设置主 Base 失败"), error);
        return;
    }
    m_selectedReferenceBaseId = target;
    reloadReferenceStateAfterImageChange();
    refreshReferenceAssetControls();
    showReferenceImageMode();
}

void ReferenceImageDialog::ensureCameraRunning()
{
    CameraFrameProvider &provider = CameraFrameProvider::instance();

    if (!provider.isOpened() && !provider.openCamera(QStringLiteral("/dev/video0"))) {
        qWarning() << "[ReferenceImageDialog] 当前图像模式启动失败：相机打开失败。";
        if (m_positionStatusLabel) {
            m_positionStatusLabel->setText(
                        tr("相机打开失败，请检查设备连接和占用状态"));
        }
        return;
    }

    if (!provider.isGrabbing() && !provider.startGrab()) {
        qWarning() << "[ReferenceImageDialog] 当前图像模式启动失败：采集启动失败。";
        if (m_positionStatusLabel) {
            m_positionStatusLabel->setText(
                        tr("相机已打开，但图像采集启动失败"));
        }
    }
}

void ReferenceImageDialog::updateReferenceImageControls()
{
    ui->currentImageButton->setVisible(!m_liveCaptureMode);
    ui->historyImageButton->setVisible(!m_liveCaptureMode);
    ui->pcImportButton->setVisible(!m_liveCaptureMode);
    const ReferenceAssetSet &assets =
            SchemeStore::instance().currentScheme().referenceAssets;
    const bool canAdd = assets.size() < ReferenceAssetSet::kMaximumAssets;
    const bool primaryActionEnabled = !assets.isReadOnly()
            && (!assets.multiBaseEnabled() || canAdd);
    ui->currentImageButton->setEnabled(primaryActionEnabled);
    ui->pcImportButton->setEnabled(primaryActionEnabled);
    ui->currentImageButton->setToolTip(assets.multiBaseEnabled()
            ? tr("抓取当前相机图像并新增一个 Base")
            : tr("抓取当前相机图像并设置/替换主 Base"));
    ui->pcImportButton->setToolTip(assets.multiBaseEnabled()
            ? tr("从 PC 导入图片并新增一个 Base")
            : tr("从 PC 导入图片并设置/替换主 Base"));

    if (m_captureImageButton) {
        m_captureImageButton->setVisible(m_liveCaptureMode);
        m_captureImageButton->setEnabled(true);
    }
    if (m_exitCaptureButton) {
        m_exitCaptureButton->setVisible(m_liveCaptureMode);
    }
    updateMultiReferenceUi();
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

    const QString baseId = selectedReferenceBaseId();
    const QImage image = ReferenceImageProvider::instance().referenceImage(
                baseId);
    if (image.isNull()) {
        m_previewHelper->clear();
        ui->viewerTitleLabel->setText(tr("请先设置基准图"));
        return;
    }

    QString baseName;
    const QVector<ReferenceAsset> assets =
            SchemeStore::instance().referenceAssets();
    int baseNumber = 0;
    for (int index = 0; index < assets.size(); ++index) {
        if (assets.at(index).baseId == baseId) {
            baseName = assets.at(index).name;
            baseNumber = index + 1;
            break;
        }
    }
    ui->viewerTitleLabel->setText(baseNumber > 0
            ? tr("Base B%1 · %2%3")
              .arg(baseNumber, 2, 10, QLatin1Char('0'))
              .arg(baseName)
              .arg(baseId == ReferenceImageProvider::instance().primaryBaseId()
                   ? tr("（主 Base）") : QString())
            : tr("基准图"));
    m_previewHelper->clearToolOverlays();
    m_previewHelper->setImage(image);
    restoreReferencePositionRoi();
    if (!m_referencePositionMatchOverlays.isEmpty()) {
        renderReferencePositionOverlays();
    } else {
        renderReferencePositionOverlays();
    }
}
