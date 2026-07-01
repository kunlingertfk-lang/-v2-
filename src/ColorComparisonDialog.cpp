#include "ColorComparisonDialog.h"

#include "PlanDialogUtils.h"
#include "frame/CameraFrameProvider.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolRequest.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <cmath>

namespace {

bool finiteValue(qreal value)
{
    return std::isfinite(static_cast<double>(value));
}

QJsonObject rectToJson(const QRectF &rect)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), rect.x());
    json.insert(QStringLiteral("y"), rect.y());
    json.insert(QStringLiteral("width"), rect.width());
    json.insert(QStringLiteral("height"), rect.height());
    return json;
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

QJsonObject pointToJson(const QPointF &point)
{
    QJsonObject json;
    json.insert(QStringLiteral("x"), point.x());
    json.insert(QStringLiteral("y"), point.y());
    return json;
}

QPointF pointFromJson(const QJsonObject &json)
{
    return QPointF(json.value(QStringLiteral("x")).toDouble(),
                   json.value(QStringLiteral("y")).toDouble());
}

QJsonArray pointsToJson(const QVector<QPointF> &points)
{
    QJsonArray array;
    for (const QPointF &point : points)
        array.append(pointToJson(point));
    return array;
}

QVector<QPointF> pointsFromJson(const QJsonArray &array)
{
    QVector<QPointF> points;
    points.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QPointF point = pointFromJson(value.toObject());
        if (finiteValue(point.x()) && finiteValue(point.y())) {
            points.append(QPointF(qBound(0.0, point.x(), 1.0),
                                  qBound(0.0, point.y(), 1.0)));
        }
    }
    return points.size() >= 3 ? points : QVector<QPointF>();
}

QJsonObject circleToJson(const CircleRoi &circle)
{
    QJsonObject json;
    json.insert(QStringLiteral("center"), pointToJson(circle.centerNormalized));
    json.insert(QStringLiteral("radius"), circle.radiusNormalized);
    json.insert(QStringLiteral("boundingRect"), rectToJson(circle.boundingRectNormalized));
    json.insert(QStringLiteral("valid"), circle.valid);
    return json;
}

CircleRoi circleFromJson(const QJsonObject &json)
{
    CircleRoi circle;
    circle.centerNormalized = pointFromJson(json.value(QStringLiteral("center")).toObject());
    circle.radiusNormalized = json.value(QStringLiteral("radius")).toDouble();
    circle.boundingRectNormalized =
            rectFromJson(json.value(QStringLiteral("boundingRect")).toObject(),
                         QRectF(circle.centerNormalized.x() - circle.radiusNormalized,
                                circle.centerNormalized.y() - circle.radiusNormalized,
                                circle.radiusNormalized * 2.0,
                                circle.radiusNormalized * 2.0));
    circle.valid = json.value(QStringLiteral("valid")).toBool(circle.radiusNormalized > 0.0);
    return circle;
}

QImage imageFromFrame(const cv::Mat &frame)
{
    return MatImageConverter::matToDisplayImage(frame, QStringLiteral("ColorComparisonDialog"));
}

QFrame *card(QWidget *parent, const QString &title)
{
    QFrame *frame = new QFrame(parent);
    frame->setFrameShape(QFrame::NoFrame);
    frame->setProperty("panelRole", QStringLiteral("configCard"));
    QVBoxLayout *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);
    QLabel *titleLabel = new QLabel(title, frame);
    titleLabel->setProperty("role", QStringLiteral("cardTitle"));
    layout->addWidget(titleLabel);
    return frame;
}

QHBoxLayout *row(const QString &labelText, QWidget *field)
{
    QHBoxLayout *layout = new QHBoxLayout;
    QLabel *label = new QLabel(labelText);
    label->setMinimumWidth(118);
    label->setProperty("role", QStringLiteral("rowField"));
    layout->addWidget(label);
    layout->addStretch(1);
    if (field)
        layout->addWidget(field);
    return layout;
}

void applyBottomActionButtonMetrics(QPushButton *button)
{
    if (!button)
        return;

    button->setMinimumSize(120, 48);
    button->setMaximumSize(120, 48);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    button->setAutoDefault(false);
    button->setDefault(false);
}

void refreshButtonStyle(QWidget *button)
{
    if (!button)
        return;

    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();
}

} // namespace

ColorComparisonDialog::ColorComparisonDialog(QWidget *parent)
    : QDialog(parent)
{
    buildUi();
    m_continuousTimer = new QTimer(this);
    m_continuousTimer->setInterval(500);
    connect(m_continuousTimer, &QTimer::timeout, this, &ColorComparisonDialog::runContinuousTick);
    connectControls();
    setAllParamsMode(false);
    showPreviewImage();
    refreshRoiOverlay();
    updateTemplatePreview();
    updateBottomButtons();
}

ColorComparisonDialog::~ColorComparisonDialog() = default;

void ColorComparisonDialog::buildUi()
{
    setWindowTitle(tr("方案编辑 - 颜色比较"));
    setWindowModality(Qt::WindowModal);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    PlanDialogUtils::applyLargeWindow(this);

    m_segmentGroup = new QButtonGroup(this);
    m_detectRegionGroup = new QButtonGroup(this);

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *header = new QFrame(this);
    header->setStyleSheet(QStringLiteral("background:#3f444e; color:#ffffff;"));
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(28, 0, 22, 0);
    QLabel *headerTitle = new QLabel(tr("方案编辑"), header);
    QToolButton *closeButton = new QToolButton(header);
    closeButton->setText(QStringLiteral("×"));
    closeButton->setStyleSheet(QStringLiteral("color:#ffffff; font-size:24px; border:0;"));
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);
    root->addWidget(header, 0);

    QHBoxLayout *content = new QHBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);
    root->addLayout(content, 1);

    QFrame *leftPanel = new QFrame(this);
    leftPanel->setMinimumWidth(420);
    leftPanel->setMaximumWidth(480);
    leftPanel->setStyleSheet(QStringLiteral("background:#eef1f5; color:#1f2937;"));
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(24, 18, 24, 18);
    leftLayout->setSpacing(14);
    content->addWidget(leftPanel, 0);

    QHBoxLayout *titleLayout = new QHBoxLayout;
    QLabel *dialogTitle = new QLabel(tr("颜色比较"), leftPanel);
    dialogTitle->setProperty("role", QStringLiteral("cardTitle"));
    m_basicButton = new QPushButton(tr("基础"), leftPanel);
    m_allButton = new QPushButton(tr("全部"), leftPanel);
    m_basicButton->setCheckable(true);
    m_allButton->setCheckable(true);
    m_segmentGroup->addButton(m_basicButton, 0);
    m_segmentGroup->addButton(m_allButton, 1);
    titleLayout->addWidget(dialogTitle);
    titleLayout->addStretch(1);
    titleLayout->addWidget(m_basicButton);
    titleLayout->addWidget(m_allButton);
    leftLayout->addLayout(titleLayout);

    m_paramsStack = new QStackedWidget(leftPanel);
    leftLayout->addWidget(m_paramsStack, 1);

    auto buildPage = [this](bool allMode) {
        QWidget *page = new QWidget;
        QVBoxLayout *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 8, 0, 0);
        layout->setSpacing(14);

        QFrame *templateCard = card(page, tr("模板区域"));
        QVBoxLayout *templateLayout = qobject_cast<QVBoxLayout *>(templateCard->layout());
        QLabel *modeLabel = new QLabel(tr("模板区域：自定义"), templateCard);
        templateLayout->addWidget(modeLabel);
        QWidget *templateEditRow = new QWidget(templateCard);
        QHBoxLayout *templateEditLayout = new QHBoxLayout(templateEditRow);
        templateEditLayout->setContentsMargins(0, 0, 0, 0);
        templateEditLayout->addWidget(new QLabel(tr("模板编辑"), templateEditRow));
        templateEditLayout->addStretch(1);
        if (!m_templateEditButton)
            m_templateEditButton = new QPushButton(tr("编辑"), this);
        if (!m_templateRectButton) {
            m_templateRectButton = new QToolButton(this);
            m_templateRectButton->setText(QStringLiteral("□"));
            m_templateRectButton->setCheckable(true);
        }
        if (!m_templateFinishButton)
            m_templateFinishButton = new QPushButton(tr("完成"), this);
        templateEditLayout->addWidget(m_templateEditButton);
        templateEditLayout->addWidget(m_templateRectButton);
        templateEditLayout->addWidget(m_templateFinishButton);
        templateLayout->addWidget(templateEditRow);

        QWidget *maskRow = new QWidget(templateCard);
        QHBoxLayout *maskLayout = new QHBoxLayout(maskRow);
        maskLayout->setContentsMargins(0, 0, 0, 0);
        maskLayout->addWidget(new QLabel(tr("屏蔽区域"), maskRow));
        maskLayout->addStretch(1);
        if (!m_templateMaskEditButton)
            m_templateMaskEditButton = new QPushButton(tr("编辑"), this);
        if (!m_templateMaskPolygonButton) {
            m_templateMaskPolygonButton = new QToolButton(this);
            m_templateMaskPolygonButton->setText(QStringLiteral("⬡"));
            m_templateMaskPolygonButton->setCheckable(true);
        }
        if (!m_templateMaskFinishButton)
            m_templateMaskFinishButton = new QPushButton(tr("完成"), this);
        maskLayout->addWidget(m_templateMaskEditButton);
        maskLayout->addWidget(m_templateMaskPolygonButton);
        maskLayout->addWidget(m_templateMaskFinishButton);
        templateLayout->addWidget(maskRow);
        layout->addWidget(templateCard);

        QFrame *effectCard = card(page, tr("模板效果"));
        QVBoxLayout *effectLayout = qobject_cast<QVBoxLayout *>(effectCard->layout());
        if (!m_templatePreviewLabel) {
            m_templatePreviewLabel = new QLabel(effectCard);
            m_templatePreviewLabel->setFixedSize(132, 86);
            m_templatePreviewLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_templatePreviewLabel->setAlignment(Qt::AlignCenter);
            m_templatePreviewLabel->setStyleSheet(QStringLiteral("background:#ffffff; border:1px solid #d1d5db;"));
        }
        effectLayout->addWidget(m_templatePreviewLabel, 0, Qt::AlignHCenter);
        layout->addWidget(effectCard);

        if (allMode) {
            QFrame *featureCard = card(page, tr("模型色彩特征"));
            m_featureCard = featureCard;
            QVBoxLayout *featureLayout = qobject_cast<QVBoxLayout *>(featureCard->layout());
            if (!m_featureTypeComboBox) {
                m_featureTypeComboBox = new QComboBox(this);
                m_featureTypeComboBox->addItems({tr("直方图特征"), tr("色谱特征")});
            }
            if (!m_brightnessCheckBox) {
                m_brightnessCheckBox = new QCheckBox(tr("亮度使能"), this);
                m_brightnessCheckBox->setChecked(true);
            }
            featureLayout->addLayout(row(tr("特征类型"), m_featureTypeComboBox));
            featureLayout->addWidget(m_brightnessCheckBox);
            QLabel *histogramLabel = new QLabel(tr("色相        饱和度        亮度"), featureCard);
            histogramLabel->setAlignment(Qt::AlignCenter);
            histogramLabel->setMinimumHeight(72);
            histogramLabel->setStyleSheet(QStringLiteral("background:#252a31; color:#d1d5db; border:1px solid #111827;"));
            featureLayout->addWidget(histogramLabel);
            layout->addWidget(featureCard);
        }

        QFrame *detectCard = card(page, tr("检测区域"));
        QVBoxLayout *detectLayout = qobject_cast<QVBoxLayout *>(detectCard->layout());
        QWidget *detectButtons = new QWidget(detectCard);
        QHBoxLayout *detectButtonsLayout = new QHBoxLayout(detectButtons);
        detectButtonsLayout->setContentsMargins(0, 0, 0, 0);
        if (!m_detectGlobalButton) {
            m_detectGlobalButton = new QToolButton(this);
            m_detectGlobalButton->setText(QStringLiteral("▣"));
            m_detectGlobalButton->setCheckable(true);
            m_detectRectButton = new QToolButton(this);
            m_detectRectButton->setText(QStringLiteral("□"));
            m_detectRectButton->setCheckable(true);
            m_detectCircleButton = new QToolButton(this);
            m_detectCircleButton->setText(QStringLiteral("○"));
            m_detectCircleButton->setCheckable(true);
            m_detectRegionGroup->addButton(m_detectGlobalButton, 0);
            m_detectRegionGroup->addButton(m_detectRectButton, 1);
            m_detectRegionGroup->addButton(m_detectCircleButton, 2);
        }
        detectButtonsLayout->addWidget(new QLabel(tr("检测区"), detectButtons));
        detectButtonsLayout->addStretch(1);
        detectButtonsLayout->addWidget(m_detectGlobalButton);
        detectButtonsLayout->addWidget(m_detectRectButton);
        detectButtonsLayout->addWidget(m_detectCircleButton);
        detectLayout->addWidget(detectButtons);

        QWidget *positionEnableRow = new QWidget(detectCard);
        QHBoxLayout *positionEnableLayout = new QHBoxLayout(positionEnableRow);
        positionEnableLayout->setContentsMargins(0, 0, 0, 0);
        positionEnableLayout->addWidget(new QLabel(tr("独立位置修正使能 ⓘ"), positionEnableRow));
        positionEnableLayout->addStretch(1);
        if (!m_positionCorrectionCheckBox) {
            m_positionCorrectionCheckBox = new QCheckBox(positionEnableRow);
            m_positionCorrectionCheckBox->setObjectName(QStringLiteral("positionCorrectionSwitch"));
        }
        positionEnableLayout->addWidget(m_positionCorrectionCheckBox);
        detectLayout->addWidget(positionEnableRow);

        if (!m_positionCorrectionSourceRow) {
            m_positionCorrectionSourceRow = new QWidget(detectCard);
            QHBoxLayout *positionSourceLayout = new QHBoxLayout(m_positionCorrectionSourceRow);
            positionSourceLayout->setContentsMargins(0, 0, 0, 0);
            QLabel *positionSourceLabel = new QLabel(tr("位置修正"), m_positionCorrectionSourceRow);
            positionSourceLabel->setMinimumWidth(118);
            positionSourceLabel->setProperty("role", QStringLiteral("rowField"));
            positionSourceLayout->addWidget(positionSourceLabel);
            positionSourceLayout->addStretch(1);
            m_positionCorrectionComboBox = new QComboBox(m_positionCorrectionSourceRow);
            m_positionCorrectionComboBox->addItem(QStringLiteral("1 基准图.位置修正信息"));
            positionSourceLayout->addWidget(m_positionCorrectionComboBox);
        }
        detectLayout->addWidget(m_positionCorrectionSourceRow);

        if (allMode) {
            QWidget *detectMaskRow = new QWidget(detectCard);
            m_detectMaskRow = detectMaskRow;
            QHBoxLayout *detectMaskLayout = new QHBoxLayout(detectMaskRow);
            detectMaskLayout->setContentsMargins(0, 0, 0, 0);
            detectMaskLayout->addWidget(new QLabel(tr("屏蔽区域"), detectMaskRow));
            detectMaskLayout->addStretch(1);
            if (!m_detectMaskEditButton)
                m_detectMaskEditButton = new QPushButton(tr("编辑"), this);
            if (!m_detectMaskPolygonButton) {
                m_detectMaskPolygonButton = new QToolButton(this);
                m_detectMaskPolygonButton->setText(QStringLiteral("⬡"));
                m_detectMaskPolygonButton->setCheckable(true);
            }
            if (!m_detectMaskFinishButton)
                m_detectMaskFinishButton = new QPushButton(tr("完成"), this);
            detectMaskLayout->addWidget(m_detectMaskEditButton);
            detectMaskLayout->addWidget(m_detectMaskPolygonButton);
            detectMaskLayout->addWidget(m_detectMaskFinishButton);
            detectLayout->addWidget(detectMaskRow);
        }
        layout->addWidget(detectCard);

        QFrame *settingsCard = card(page, tr("识别设置"));
        QVBoxLayout *settingsLayout = qobject_cast<QVBoxLayout *>(settingsCard->layout());
        if (!m_sensitivityComboBox) {
            m_sensitivityComboBox = new QComboBox(this);
            m_sensitivityComboBox->addItems({tr("低"), tr("中"), tr("高")});
            m_sensitivityComboBox->setCurrentIndex(1);
        }
        settingsLayout->addLayout(row(tr("灵敏度"), m_sensitivityComboBox));
        layout->addWidget(settingsCard);

        QFrame *judgeCard = card(page, tr("结果判断"));
        QVBoxLayout *judgeLayout = qobject_cast<QVBoxLayout *>(judgeCard->layout());
        if (!m_minScoreSpinBox) {
            m_minScoreSpinBox = new QSpinBox(this);
            m_minScoreSpinBox->setRange(0, 100);
            m_minScoreSpinBox->setValue(52);
        }
        judgeLayout->addLayout(row(tr("最小得分"), m_minScoreSpinBox));
        layout->addWidget(judgeCard);
        layout->addStretch(1);
        return page;
    };

    m_paramsStack->addWidget(buildPage(true));

    QHBoxLayout *bottomButtons = new QHBoxLayout;
    m_referenceTestButton = new QPushButton(tr("基准图测试"), leftPanel);
    m_testRunButton = new QPushButton(tr("测试运行"), leftPanel);
    m_finishButton = new QPushButton(tr("完成"), leftPanel);
    m_exitTestButton = new QPushButton(tr("退出测试"), leftPanel);
    applyBottomActionButtonMetrics(m_referenceTestButton);
    applyBottomActionButtonMetrics(m_testRunButton);
    applyBottomActionButtonMetrics(m_finishButton);
    applyBottomActionButtonMetrics(m_exitTestButton);
    m_referenceTestButton->setProperty("actionRole", QStringLiteral("testAction"));
    m_testRunButton->setProperty("actionRole", QStringLiteral("testAction"));
    m_testRunButton->setProperty("running", false);
    m_finishButton->setProperty("actionRole", QStringLiteral("testPrimary"));
    m_exitTestButton->setProperty("actionRole", QStringLiteral("testAction"));
    bottomButtons->addStretch(1);
    bottomButtons->addWidget(m_referenceTestButton);
    bottomButtons->addWidget(m_testRunButton);
    bottomButtons->addWidget(m_finishButton);
    bottomButtons->addWidget(m_exitTestButton);
    leftLayout->addLayout(bottomButtons);

    QFrame *rightPanel = new QFrame(this);
    rightPanel->setStyleSheet(QStringLiteral("background:#111418; color:#f9fafb;"));
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_viewerTitleLabel = new QLabel(tr("基准图"), rightPanel);
    m_viewerTitleLabel->setMinimumHeight(48);
    m_viewerTitleLabel->setContentsMargins(18, 0, 0, 0);
    m_previewGraphicsView = new QGraphicsView(rightPanel);
    m_viewerStatusLabel = new QLabel(tr("请编辑模板区域和检测区域"), rightPanel);
    m_viewerStatusLabel->setMinimumHeight(42);
    m_viewerStatusLabel->setContentsMargins(18, 0, 0, 0);
    rightLayout->addWidget(m_viewerTitleLabel);
    rightLayout->addWidget(m_previewGraphicsView, 1);
    rightLayout->addWidget(m_viewerStatusLabel);
    content->addWidget(rightPanel, 1);

    m_previewHelper = new FrameViewHelper(m_previewGraphicsView, this);

    setStyleSheet(styleSheet() + QStringLiteral(
        "QPushButton,QToolButton,QComboBox,QSpinBox{background:#ffffff;color:#111827;border:1px solid #cfd6df;padding:6px;}"
        "QPushButton:checked,QToolButton:checked{background:#fff3e6;color:#ff7a00;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"testAction\"]{background:#ffffff;color:#1f2937;border:1px solid #cfd6df;border-radius:4px;padding:0;font-size:15px;}"
        "QPushButton[actionRole=\"testAction\"][running=\"true\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"testAction\"]:pressed{background:#ffe1bf;color:#ff7a00;border-color:#ff7a00;}"
        "QPushButton[actionRole=\"testAction\"][running=\"true\"]:pressed{background:#e66f00;color:#ffffff;border-color:#e66f00;}"
        "QPushButton[actionRole=\"testPrimary\"]{background:#ff7a00;color:#ffffff;border:1px solid #ff7a00;border-radius:4px;padding:0;font-size:15px;}"
        "QPushButton[actionRole=\"testPrimary\"]:pressed{background:#e66f00;color:#ffffff;border-color:#e66f00;}"
        "QToolButton:pressed{background:#ffe1bf;color:#ff7a00;border-color:#ff7a00;}"));

    m_basicButton->setChecked(true);
    if (m_detectRectButton)
        m_detectRectButton->setChecked(true);
    refreshEditControls();
    refreshPositionCorrectionControls();

    connect(closeButton, &QToolButton::clicked, this, &ColorComparisonDialog::reject);
    connect(m_finishButton, &QPushButton::clicked, this, &ColorComparisonDialog::finishConfiguration);
}

void ColorComparisonDialog::connectControls()
{
    connect(m_basicButton, &QPushButton::clicked, this, [this]() { setAllParamsMode(false); });
    connect(m_allButton, &QPushButton::clicked, this, [this]() { setAllParamsMode(true); });
    connect(m_templateEditButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::TemplateRect); });
    connect(m_templateRectButton, &QToolButton::clicked, this, [this]() { setEditState(EditState::TemplateRect); });
    connect(m_templateFinishButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::None); });
    connect(m_templateMaskEditButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::TemplateMaskPolygon); });
    connect(m_templateMaskPolygonButton, &QToolButton::clicked, this, [this]() { setEditState(EditState::TemplateMaskPolygon); });
    connect(m_templateMaskFinishButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::None); });
    connect(m_detectGlobalButton, &QToolButton::clicked, this, [this]() {
        m_detectRegionType = QStringLiteral("global");
        m_detectRoi = QRectF(0.0, 0.0, 1.0, 1.0);
        m_detectCircle = CircleRoi();
        setEditState(EditState::None);
        refreshDetectRegionButtons();
    });
    connect(m_detectRectButton, &QToolButton::clicked, this, [this]() {
        m_detectRegionType = QStringLiteral("rectangle");
        m_detectCircle = CircleRoi();
        setEditState(EditState::DetectRect);
        refreshDetectRegionButtons();
    });
    connect(m_detectCircleButton, &QToolButton::clicked, this, [this]() {
        m_detectRegionType = QStringLiteral("circle");
        setEditState(EditState::DetectCircle);
        refreshDetectRegionButtons();
    });
    connect(m_positionCorrectionCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_positionCorrectionEnabled = checked;
        refreshPositionCorrectionControls();
    });
    connect(m_positionCorrectionComboBox, &QComboBox::currentTextChanged, this, [this](const QString &text) {
        m_positionCorrectionSource = text;
    });
    if (m_detectMaskEditButton)
        connect(m_detectMaskEditButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::DetectMaskPolygon); });
    if (m_detectMaskPolygonButton)
        connect(m_detectMaskPolygonButton, &QToolButton::clicked, this, [this]() { setEditState(EditState::DetectMaskPolygon); });
    if (m_detectMaskFinishButton)
        connect(m_detectMaskFinishButton, &QPushButton::clicked, this, [this]() { setEditState(EditState::None); });
    connect(m_referenceTestButton, &QPushButton::clicked, this, &ColorComparisonDialog::runReferenceTest);
    connect(m_testRunButton, &QPushButton::clicked, this, &ColorComparisonDialog::runTest);
    connect(m_exitTestButton, &QPushButton::clicked, this, &ColorComparisonDialog::exitTestMode);

    connect(m_previewHelper, &FrameViewHelper::roiChanged, this, &ColorComparisonDialog::handleRoiChanged);
    connect(m_previewHelper, &FrameViewHelper::circleChanged, this, &ColorComparisonDialog::handleCircleChanged);
    connect(m_previewHelper, &FrameViewHelper::polygonChanged, this, &ColorComparisonDialog::handlePolygonChanged);
}

void ColorComparisonDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (m_previewHelper)
        m_previewHelper->fitToView();
    updateTemplatePreview();
}

void ColorComparisonDialog::setAllParamsMode(bool allMode)
{
    m_paramsStack->setCurrentIndex(0);
    m_basicButton->setChecked(!allMode);
    m_allButton->setChecked(allMode);
    if (m_featureCard)
        m_featureCard->setVisible(allMode);
    if (m_detectMaskRow)
        m_detectMaskRow->setVisible(allMode);
    setEditState(EditState::None);
    refreshEditControls();
}

void ColorComparisonDialog::setEditState(EditState state)
{
    m_editState = state;
    refreshEditControls();
    refreshDetectRegionButtons();
    if (!m_previewHelper)
        return;
    m_previewHelper->setRoiDrawingEnabled(false);
    m_previewHelper->setCircleDrawingEnabled(false);
    m_previewHelper->setPolygonDrawingEnabled(false);
    m_previewHelper->clearRoi();
    m_previewHelper->clearCircleRoi();
    m_previewHelper->clearPolygonRoi();
    if (state == EditState::TemplateRect || state == EditState::DetectRect)
        m_previewHelper->setRoiDrawingEnabled(true);
    if (state == EditState::DetectCircle)
        m_previewHelper->setCircleDrawingEnabled(true);
    if (state == EditState::TemplateMaskPolygon || state == EditState::DetectMaskPolygon)
        m_previewHelper->setPolygonDrawingEnabled(true);
    refreshRoiOverlay();

    QString statusText = tr("ROI 编辑完成");
    switch (state) {
    case EditState::TemplateRect:
        statusText = tr("请在右侧基准图上拖拽模板矩形区域");
        break;
    case EditState::TemplateMaskPolygon:
        statusText = tr("请在右侧基准图上绘制模板屏蔽多边形，双击完成");
        break;
    case EditState::DetectRect:
        statusText = tr("请在右侧基准图上拖拽检测矩形区域");
        break;
    case EditState::DetectCircle:
        statusText = tr("请在右侧基准图上拖拽检测圆形区域");
        break;
    case EditState::DetectMaskPolygon:
        statusText = tr("请在右侧基准图上绘制检测屏蔽多边形，双击完成");
        break;
    case EditState::None:
        break;
    }
    updateStatus(statusText);
}

void ColorComparisonDialog::showPreviewImage()
{
    cv::Mat frame = ReferenceImageProvider::instance().referenceFrame();
    m_previewUsesReferenceImage = !frame.empty();
    if (frame.empty())
        frame = CameraFrameProvider::instance().currentFrame();
    const QImage image = imageFromFrame(frame);
    m_previewImage = image;
    if (!image.isNull()) {
        m_previewHelper->setImage(image);
        updateTemplatePreview();
    } else {
        m_previewImage = QImage();
        if (m_previewHelper)
            m_previewHelper->clear();
        updateTemplatePreview();
    }
}

void ColorComparisonDialog::showFrameImage(const cv::Mat &frame, const QString &title)
{
    const QImage image = imageFromFrame(frame);
    m_previewImage = image;
    if (m_viewerTitleLabel)
        m_viewerTitleLabel->setText(title);
    if (!image.isNull() && m_previewHelper) {
        m_previewHelper->setImage(image);
        updateTemplatePreview();
        refreshRoiOverlay();
    }
}

void ColorComparisonDialog::refreshRoiOverlay()
{
    if (!m_previewHelper)
        return;
    if (m_editState == EditState::TemplateRect)
        m_previewHelper->setRoiRectNormalized(m_templateRoi);
    else if (m_editState == EditState::DetectRect)
        m_previewHelper->setRoiRectNormalized(m_detectRoi);
    else if (m_editState == EditState::DetectCircle && m_detectCircle.valid)
        m_previewHelper->setCircleRoiNormalized(m_detectCircle);
    else if (m_editState == EditState::TemplateMaskPolygon && m_templateMask.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(m_templateMask);
    else if (m_editState == EditState::DetectMaskPolygon && m_detectMask.size() >= 3)
        m_previewHelper->setPolygonRoiNormalized(m_detectMask);
}

void ColorComparisonDialog::updateStatus(const QString &text)
{
    if (m_viewerStatusLabel)
        m_viewerStatusLabel->setText(text);
}

void ColorComparisonDialog::updateTemplatePreview()
{
    if (!m_templatePreviewLabel)
        return;

    const QImage roiImage = templateRoiImage();
    if (roiImage.isNull()) {
        m_templatePreviewLabel->clear();
        m_templatePreviewLabel->setText(tr("无模板图像"));
        return;
    }

    const QSize targetSize(132, 86);
    QPixmap canvas(targetSize);
    canvas.fill(Qt::white);
    const QPixmap scaled = QPixmap::fromImage(roiImage).scaled(targetSize - QSize(12, 12),
                                                               Qt::KeepAspectRatio,
                                                               Qt::SmoothTransformation);
    QPainter painter(&canvas);
    const QPoint topLeft((targetSize.width() - scaled.width()) / 2,
                         (targetSize.height() - scaled.height()) / 2);
    painter.drawPixmap(topLeft, scaled);
    m_templatePreviewLabel->setText(QString());
    m_templatePreviewLabel->setPixmap(canvas);
}

void ColorComparisonDialog::handleRoiChanged(const QRectF &roi)
{
    if (m_editState == EditState::TemplateRect) {
        m_templateRoi = normalizedRoiOrDefault(roi);
        updateTemplatePreview();
        refreshRoiOverlay();
    } else if (m_editState == EditState::DetectRect) {
        m_detectRoi = normalizedRoiOrDefault(roi);
        refreshRoiOverlay();
    }
}

void ColorComparisonDialog::handleCircleChanged(const CircleRoi &circle)
{
    if (m_editState != EditState::DetectCircle)
        return;
    m_detectCircle = circle;
    if (circle.valid) {
        m_detectRoi = normalizedRoiOrDefault(circle.boundingRectNormalized);
        refreshRoiOverlay();
    }
}

void ColorComparisonDialog::handlePolygonChanged(const QVector<QPointF> &points)
{
    if (m_editState == EditState::TemplateMaskPolygon)
        m_templateMask = points.size() >= 3 ? points : QVector<QPointF>();
    else if (m_editState == EditState::DetectMaskPolygon)
        m_detectMask = points.size() >= 3 ? points : QVector<QPointF>();
    refreshRoiOverlay();
}

QRectF ColorComparisonDialog::normalizedRoiOrDefault(const QRectF &roi) const
{
    if (!finiteValue(roi.x()) || !finiteValue(roi.y()) ||
        !finiteValue(roi.width()) || !finiteValue(roi.height()) ||
        roi.width() <= 0.0 || roi.height() <= 0.0) {
        return QRectF(0.0, 0.0, 1.0, 1.0);
    }
    return roi.intersected(QRectF(0.0, 0.0, 1.0, 1.0));
}

QImage ColorComparisonDialog::templateRoiImage() const
{
    if (m_previewImage.isNull())
        return QImage();

    const QRectF roi = normalizedRoiOrDefault(m_templateRoi);
    if (roi.width() <= 0.0 || roi.height() <= 0.0)
        return QImage();

    const QRect sourceRect(qBound(0, static_cast<int>(std::floor(roi.x() * m_previewImage.width())), m_previewImage.width() - 1),
                           qBound(0, static_cast<int>(std::floor(roi.y() * m_previewImage.height())), m_previewImage.height() - 1),
                           qMax(1, static_cast<int>(std::ceil(roi.width() * m_previewImage.width()))),
                           qMax(1, static_cast<int>(std::ceil(roi.height() * m_previewImage.height()))));
    return m_previewImage.copy(sourceRect.intersected(m_previewImage.rect()));
}

void ColorComparisonDialog::refreshEditControls()
{
    const bool editingTemplate = m_editState == EditState::TemplateRect;
    const bool editingTemplateMask = m_editState == EditState::TemplateMaskPolygon;
    const bool editingDetectMask = m_editState == EditState::DetectMaskPolygon;

    if (m_templateEditButton)
        m_templateEditButton->setVisible(!editingTemplate);
    if (m_templateRectButton) {
        m_templateRectButton->setVisible(editingTemplate);
        m_templateRectButton->setChecked(editingTemplate);
    }
    if (m_templateFinishButton)
        m_templateFinishButton->setVisible(editingTemplate);

    if (m_templateMaskEditButton)
        m_templateMaskEditButton->setVisible(!editingTemplateMask);
    if (m_templateMaskPolygonButton) {
        m_templateMaskPolygonButton->setVisible(editingTemplateMask);
        m_templateMaskPolygonButton->setChecked(editingTemplateMask);
    }
    if (m_templateMaskFinishButton)
        m_templateMaskFinishButton->setVisible(editingTemplateMask);

    if (m_detectMaskEditButton)
        m_detectMaskEditButton->setVisible(!editingDetectMask);
    if (m_detectMaskPolygonButton) {
        m_detectMaskPolygonButton->setVisible(editingDetectMask);
        m_detectMaskPolygonButton->setChecked(editingDetectMask);
    }
    if (m_detectMaskFinishButton)
        m_detectMaskFinishButton->setVisible(editingDetectMask);
}

void ColorComparisonDialog::refreshDetectRegionButtons()
{
    if (!m_detectGlobalButton || !m_detectRectButton || !m_detectCircleButton)
        return;

    const QSignalBlocker blockGlobal(m_detectGlobalButton);
    const QSignalBlocker blockRect(m_detectRectButton);
    const QSignalBlocker blockCircle(m_detectCircleButton);
    m_detectGlobalButton->setChecked(m_detectRegionType == QStringLiteral("global"));
    m_detectRectButton->setChecked(m_detectRegionType == QStringLiteral("rectangle"));
    m_detectCircleButton->setChecked(m_detectRegionType == QStringLiteral("circle"));
}

void ColorComparisonDialog::refreshPositionCorrectionControls()
{
    if (m_positionCorrectionCheckBox) {
        const QSignalBlocker block(m_positionCorrectionCheckBox);
        m_positionCorrectionCheckBox->setChecked(m_positionCorrectionEnabled);
    }
    if (m_positionCorrectionSourceRow)
        m_positionCorrectionSourceRow->setVisible(m_positionCorrectionEnabled);
    if (m_positionCorrectionComboBox) {
        const int index = m_positionCorrectionComboBox->findText(m_positionCorrectionSource);
        if (index >= 0) {
            const QSignalBlocker block(m_positionCorrectionComboBox);
            m_positionCorrectionComboBox->setCurrentIndex(index);
        }
    }
}

QJsonObject ColorComparisonDialog::colorComparisonParams() const
{
    QJsonObject params;
    params.insert(QStringLiteral("version"), 1);
    params.insert(QStringLiteral("templateRegionMode"), QStringLiteral("custom"));
    params.insert(QStringLiteral("templateRoiNormalized"), rectToJson(m_templateRoi));
    params.insert(QStringLiteral("templateMaskPolygon"), pointsToJson(m_templateMask));
    params.insert(QStringLiteral("featureType"),
                  m_featureTypeComboBox && m_featureTypeComboBox->currentText().contains(QStringLiteral("色谱"))
                  ? QStringLiteral("spectrum")
                  : QStringLiteral("histogram"));
    const QString sensitivity = m_sensitivityComboBox && m_sensitivityComboBox->currentIndex() == 0
            ? QStringLiteral("low")
            : m_sensitivityComboBox && m_sensitivityComboBox->currentIndex() == 2
              ? QStringLiteral("high")
              : QStringLiteral("medium");
    params.insert(QStringLiteral("sensitivity"), sensitivity);
    params.insert(QStringLiteral("brightnessEnabled"),
                  m_brightnessCheckBox ? m_brightnessCheckBox->isChecked() : true);
    params.insert(QStringLiteral("detectRegionType"), m_detectRegionType);
    params.insert(QStringLiteral("detectRoiNormalized"), rectToJson(m_detectRoi));
    params.insert(QStringLiteral("detectCircleNormalized"), circleToJson(m_detectCircle));
    params.insert(QStringLiteral("detectMaskPolygon"), pointsToJson(m_detectMask));
    params.insert(QStringLiteral("enablePositionCorrection"), m_positionCorrectionEnabled);
    params.insert(QStringLiteral("positionCorrectionSource"), m_positionCorrectionSource);
    return params;
}

ToolConfig ColorComparisonDialog::toToolConfig() const
{
    ToolConfig config;
    config.toolId = m_toolId.isEmpty()
            ? QUuid::createUuid().toString(QUuid::WithoutBraces)
            : m_toolId;
    config.toolName = QStringLiteral("ColorComparison");
    config.displayName = tr("颜色比较");
    config.toolType = ToolType::ColorComparison;
    config.category = ToolCategory::Recognition;
    config.enabled = m_enabled;
    config.roiNormalized = m_detectRoi;
    QJsonObject params;
    params.insert(QStringLiteral("colorComparison"), colorComparisonParams());
    config.params = params;
    QJsonObject judge;
    judge.insert(QStringLiteral("mode"), QStringLiteral("min_score"));
    judge.insert(QStringLiteral("minScore"), m_minScoreSpinBox ? m_minScoreSpinBox->value() : 52);
    config.judgeRule = judge;
    config.summary = summaryText();
    return config;
}

ToolConfig ColorComparisonDialog::toolConfig() const
{
    return toToolConfig();
}

ToolPreviewSnapshot ColorComparisonDialog::referencePreviewSnapshot() const
{
    return m_referencePreviewSnapshot;
}

void ColorComparisonDialog::loadFromConfig(const ToolConfig &config)
{
    if (config.toolType != ToolType::Unknown && config.toolType != ToolType::ColorComparison)
        return;
    m_toolId = config.toolId;
    m_enabled = config.enabled;
    const QJsonObject colorComparison =
            config.params.value(QStringLiteral("colorComparison")).toObject();
    m_templateRoi = normalizedRoiOrDefault(
                rectFromJson(colorComparison.value(QStringLiteral("templateRoiNormalized")).toObject(),
                             m_templateRoi));
    m_templateMask = pointsFromJson(colorComparison.value(QStringLiteral("templateMaskPolygon")).toArray());
    m_detectRoi = normalizedRoiOrDefault(config.roiNormalized.width() > 0.0 &&
                                         config.roiNormalized.height() > 0.0
                                         ? config.roiNormalized
                                         : rectFromJson(colorComparison.value(QStringLiteral("detectRoiNormalized")).toObject(),
                                                        m_detectRoi));
    m_detectRegionType = colorComparison.value(QStringLiteral("detectRegionType"))
            .toString(QStringLiteral("rectangle"));
    m_detectCircle = circleFromJson(colorComparison.value(QStringLiteral("detectCircleNormalized")).toObject());
    m_detectMask = pointsFromJson(colorComparison.value(QStringLiteral("detectMaskPolygon")).toArray());
    m_positionCorrectionEnabled = colorComparison.value(QStringLiteral("enablePositionCorrection")).toBool(false);
    m_positionCorrectionSource = colorComparison.value(QStringLiteral("positionCorrectionSource"))
            .toString(QStringLiteral("1 基准图.位置修正信息"));
    const QString sensitivity = colorComparison.value(QStringLiteral("sensitivity")).toString(QStringLiteral("medium"));
    if (m_sensitivityComboBox)
        m_sensitivityComboBox->setCurrentIndex(sensitivity == QStringLiteral("low") ? 0 :
                                               sensitivity == QStringLiteral("high") ? 2 : 1);
    if (m_brightnessCheckBox)
        m_brightnessCheckBox->setChecked(colorComparison.value(QStringLiteral("brightnessEnabled")).toBool(true));
    if (m_minScoreSpinBox)
        m_minScoreSpinBox->setValue(config.judgeRule.value(QStringLiteral("minScore")).toInt(52));
    refreshDetectRegionButtons();
    refreshPositionCorrectionControls();
    refreshEditControls();
    updateTemplatePreview();
    refreshRoiOverlay();
}

QString ColorComparisonDialog::summaryText() const
{
    return tr("HSV直方图比较；最低分 %1").arg(m_minScoreSpinBox ? m_minScoreSpinBox->value() : 52);
}

void ColorComparisonDialog::runTest()
{
    if (m_testUiMode == TestUiMode::Continuous) {
        stopContinuousRun();
        m_testUiMode = TestUiMode::TestPaused;
        updateBottomButtons();
        updateStatus(tr("连续运行已停止，视图保留最后一帧"));
        return;
    }

    startContinuousRun();
}

void ColorComparisonDialog::runReferenceTest()
{
    stopContinuousRun();
    m_testUiMode = TestUiMode::Edit;
    updateBottomButtons();

    const cv::Mat frame = ReferenceImageProvider::instance().referenceFrame();
    if (frame.empty()) {
        displayError(QStringLiteral("no_reference_image"), tr("请先设置基准图"));
        return;
    }

    showFrameImage(frame, tr("基准图"));
    runComparisonOnFrame(frame.clone(), tr("基准图"), true);
}

void ColorComparisonDialog::startContinuousRun()
{
    m_testUiMode = TestUiMode::Continuous;
    updateBottomButtons();
    if (m_continuousTimer && !m_continuousTimer->isActive())
        m_continuousTimer->start();
    runContinuousTick();
}

void ColorComparisonDialog::stopContinuousRun()
{
    if (m_continuousTimer)
        m_continuousTimer->stop();
}

void ColorComparisonDialog::runContinuousTick()
{
    if (m_testUiMode != TestUiMode::Continuous || m_comparisonRunning)
        return;

    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        displayError(QStringLiteral("image_empty"), tr("当前帧为空"));
        return;
    }

    showFrameImage(frame, tr("测试图像"));
    runComparisonOnFrame(frame.clone(), tr("测试图像"), false);
}

void ColorComparisonDialog::runSingleShotTest()
{
    stopContinuousRun();
    m_testUiMode = TestUiMode::TestPaused;
    updateBottomButtons();

    const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        displayError(QStringLiteral("image_empty"), tr("当前帧为空"));
        return;
    }

    showFrameImage(frame, tr("单次测试快照"));
    runComparisonOnFrame(frame.clone(), tr("单次测试快照"), false);
}

void ColorComparisonDialog::exitTestMode()
{
    stopContinuousRun();
    m_testUiMode = TestUiMode::Edit;
    updateBottomButtons();
    showPreviewImage();
    refreshRoiOverlay();
    updateStatus(tr("已退出测试"));
}

void ColorComparisonDialog::updateBottomButtons()
{
    if (!m_referenceTestButton || !m_testRunButton || !m_finishButton || !m_exitTestButton)
        return;

    const bool testMode = m_testUiMode != TestUiMode::Edit;
    m_referenceTestButton->setVisible(!testMode);
    m_exitTestButton->setVisible(testMode);
    m_finishButton->setText(testMode ? tr("运行一次") : tr("完成"));
    m_testRunButton->setText(m_testUiMode == TestUiMode::Continuous ? tr("停止运行") :
                             testMode ? tr("连续运行") : tr("测试运行"));
    m_testRunButton->setProperty("running", m_testUiMode == TestUiMode::Continuous);
    refreshButtonStyle(m_testRunButton);
}

void ColorComparisonDialog::runComparisonOnFrame(const cv::Mat &frame,
                                                 const QString &imageTitle,
                                                 bool referenceSource)
{
    if (m_comparisonRunning)
        return;

    if (frame.empty()) {
        displayError(QStringLiteral("image_empty"), tr("当前图像为空"));
        return;
    }

    m_comparisonRunning = true;
    ToolRequest request;
    request.config = toToolConfig();
    request.image = frame.clone();
    if (!imageTitle.isEmpty() && m_viewerTitleLabel)
        m_viewerTitleLabel->setText(imageTitle);
    displayResult(m_testAdapter.run(request), referenceSource);
    m_comparisonRunning = false;
}

void ColorComparisonDialog::displayResult(const ToolResult &result, bool referenceSource)
{
    if (m_previewHelper)
        m_previewHelper->setToolOverlays(result.overlays);
    updateStatus(tr("%1 | score:%2 | %3")
                 .arg(result.ok ? QStringLiteral("OK") : QStringLiteral("NG"),
                      QString::number(result.score, 'f', 2),
                      result.status));
    if (referenceSource) {
        m_referencePreviewSnapshot =
                makeReferenceToolPreviewSnapshot(toToolConfig(), result, m_detectRoi);
    }
}

void ColorComparisonDialog::displayError(const QString &status, const QString &message)
{
    ToolResult result;
    result.toolType = ToolType::ColorComparison;
    result.status = status;
    result.message = message;
    displayResult(result, false);
}

void ColorComparisonDialog::finishConfiguration()
{
    if (m_testUiMode != TestUiMode::Edit) {
        runSingleShotTest();
        return;
    }

    setEditState(EditState::None);
    accept();
}
