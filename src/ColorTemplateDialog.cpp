#include "ColorTemplateDialog.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"

#include <QButtonGroup>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QIODevice>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QColor>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QScreen>
#include <QGuiApplication>

#include <algorithm>
#include <cmath>
#include <exception>
#include <opencv2/imgcodecs.hpp>

namespace {

bool finiteValue(qreal value)
{
    return std::isfinite(static_cast<double>(value));
}

QRectF normalizedRoiOrDefault(const QRectF &roi)
{
    if (!finiteValue(roi.x()) ||
        !finiteValue(roi.y()) ||
        !finiteValue(roi.width()) ||
        !finiteValue(roi.height()) ||
        roi.width() <= 0.0 ||
        roi.height() <= 0.0) {
        return QRectF(0.0, 0.0, 1.0, 1.0);
    }

    const QRectF normalized = roi.normalized();
    const double left = qBound(0.0, normalized.left(), 1.0);
    const double top = qBound(0.0, normalized.top(), 1.0);
    const double right = qBound(0.0, normalized.right(), 1.0);
    const double bottom = qBound(0.0, normalized.bottom(), 1.0);
    const QRectF clamped(QPointF(left, top), QPointF(right, bottom));
    return clamped.width() > 0.0 && clamped.height() > 0.0
            ? clamped.normalized()
            : QRectF(0.0, 0.0, 1.0, 1.0);
}

QString featureTypeFromUi(const QString &text)
{
    return text.contains(QStringLiteral("色谱"))
            ? QStringLiteral("spectrum")
            : QStringLiteral("histogram");
}

QString featureTypeToUi(const QString &value)
{
    return value == QStringLiteral("spectrum")
            ? QStringLiteral("色谱特征")
            : QStringLiteral("直方图特征");
}

QString sensitivityFromUi(const QString &text)
{
    if (text.contains(QStringLiteral("低")))
        return QStringLiteral("low");
    if (text.contains(QStringLiteral("高")))
        return QStringLiteral("high");
    return QStringLiteral("medium");
}

QString sensitivityToUi(const QString &value)
{
    if (value == QStringLiteral("low"))
        return QStringLiteral("低敏感");
    if (value == QStringLiteral("high"))
        return QStringLiteral("高敏感");
    return QStringLiteral("中敏感");
}

void setComboBoxText(QComboBox *comboBox, const QString &text)
{
    if (!comboBox)
        return;

    const int index = comboBox->findText(text);
    if (index >= 0)
        comboBox->setCurrentIndex(index);
}

QLabel *rowLabel(const QString &text)
{
    QLabel *label = new QLabel(text);
    label->setMinimumWidth(118);
    label->setProperty("role", QStringLiteral("rowField"));
    return label;
}

QFrame *makeCard(const QString &title, QVBoxLayout **contentLayout)
{
    QFrame *card = new QFrame;
    card->setFrameShape(QFrame::NoFrame);
    card->setProperty("panelRole", QStringLiteral("configCard"));

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout;
    QLabel *titleLabel = new QLabel(title);
    titleLabel->setProperty("role", QStringLiteral("cardTitle"));
    QToolButton *collapseButton = new QToolButton;
    collapseButton->setText(QStringLiteral("⌄"));
    collapseButton->setProperty("role", QStringLiteral("collapseCard"));
    header->addWidget(titleLabel);
    header->addStretch(1);
    header->addWidget(collapseButton);
    layout->addLayout(header);

    *contentLayout = layout;
    return card;
}

QToolButton *roiButton(const QString &text, const QString &tooltip)
{
    QToolButton *button = new QToolButton;
    button->setText(text);
    button->setToolTip(tooltip);
    button->setCheckable(true);
    button->setMinimumSize(74, 36);
    button->setProperty("actionRole", QStringLiteral("toolbarIcon"));
    return button;
}

} // namespace

ColorTemplateDialog::ColorTemplateDialog(QWidget *parent)
    : QDialog(parent)
{
    buildUi();
    adjustInitialGeometry();
    setupUiState();
    connectControls();
    showPreviewImage();
}

ColorRecognitionTemplateData ColorTemplateDialog::templateData() const
{
    ColorRecognitionTemplateData data = m_template;
    data.name = m_templateNameLineEdit->text().trimmed();
    if (data.name.isEmpty())
        data.name = QStringLiteral("颜色模板");
    data.featureType = featureTypeFromUi(m_featureTypeComboBox->currentText());
    data.sensitivity = sensitivityFromUi(m_sensitivityComboBox->currentText());
    data.brightnessEnabled = m_brightnessCheckBox->isChecked();
    data.knnK = m_knnKSpinBox->value();
    data.knnDistance = QStringLiteral("halcon_default");
    return data;
}

void ColorTemplateDialog::setTemplateData(const ColorRecognitionTemplateData &data)
{
    m_template = data;
    if (m_template.templateId.trimmed().isEmpty())
        m_template.templateId = QStringLiteral("color_template");
    m_templateNameLineEdit->setText(m_template.name);
    setComboBoxText(m_featureTypeComboBox, featureTypeToUi(m_template.featureType));
    if (m_featureTypeComboBox->currentText().contains(QStringLiteral("色谱")))
        m_featureTypeComboBox->setCurrentIndex(0);
    setComboBoxText(m_sensitivityComboBox, sensitivityToUi(m_template.sensitivity));
    m_brightnessCheckBox->setChecked(m_template.brightnessEnabled);
    m_knnKSpinBox->setValue(qBound(1, m_template.knnK, 99));
    ensureDefaultLabel();
    updateLabelList();
    updateRoiSampleList();
    updateSampleCount();
}

void ColorTemplateDialog::setInitialSampleRoi(const QRectF &roi)
{
    m_sampleRoiNormalized = normalizedRoiOrDefault(roi);
    refreshDisplayedRoiOverlay();
}

void ColorTemplateDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

bool ColorTemplateDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched && watched->property("colorTemplateDragHandle").toBool()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_draggingWindow = true;
                m_dragStartGlobalPos = mouseEvent->globalPos();
                m_dragStartFramePos = frameGeometry().topLeft();
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (m_draggingWindow && (mouseEvent->buttons() & Qt::LeftButton)) {
                move(m_dragStartFramePos + mouseEvent->globalPos() - m_dragStartGlobalPos);
                event->accept();
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && m_draggingWindow) {
                m_draggingWindow = false;
                event->accept();
                return true;
            }
        }
    }

    return QDialog::eventFilter(watched, event);
}

void ColorTemplateDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() <= 44) {
        m_draggingWindow = true;
        m_dragStartGlobalPos = event->globalPos();
        m_dragStartFramePos = frameGeometry().topLeft();
        event->accept();
        return;
    }
    QDialog::mousePressEvent(event);
}

void ColorTemplateDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingWindow && (event->buttons() & Qt::LeftButton)) {
        move(m_dragStartFramePos + event->globalPos() - m_dragStartGlobalPos);
        event->accept();
        return;
    }
    QDialog::mouseMoveEvent(event);
}

void ColorTemplateDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_draggingWindow) {
        m_draggingWindow = false;
        event->accept();
        return;
    }
    QDialog::mouseReleaseEvent(event);
}

void ColorTemplateDialog::adjustInitialGeometry()
{
    QRect available;
    if (parentWidget())
        available = parentWidget()->window()->frameGeometry();
    if (!available.isValid()) {
        if (QScreen *screen = QGuiApplication::primaryScreen())
            available = screen->availableGeometry();
    }
    if (!available.isValid())
        return;

    const int maxWidth = qMax(640, available.width() - 40);
    const int maxHeight = qMax(500, available.height() - 40);
    const int minWidth = qMin(820, maxWidth);
    const int minHeight = qMin(560, maxHeight);
    const int width = qBound(minWidth, static_cast<int>(available.width() * 0.92), maxWidth);
    const int height = qBound(minHeight, static_cast<int>(available.height() * 0.92), maxHeight);
    resize(width, height);
    move(available.center() - QPoint(width / 2, height / 2));
}

void ColorTemplateDialog::buildUi()
{
    setWindowTitle(tr("创建颜色模板"));
    setMinimumSize(720, 520);
    resize(1080, 680);
    setWindowModality(Qt::WindowModal);

    setStyleSheet(QStringLiteral(
        "QDialog { background:#eef1f5; }"
        "QFrame#headerFrame { background:#3f4652; }"
        "QFrame#leftPanel { background:#eef1f5; }"
        "QFrame#previewPanel { background:#20242b; }"
        "QFrame[panelRole=\"configCard\"] { background:#ffffff; border-radius:6px; }"
        "QLabel { color:#334155; font-size:14px; }"
        "QLabel[role=\"cardTitle\"] { color:#1f2937; font-size:17px; font-weight:700; }"
        "QLabel[role=\"rowField\"] { color:#475569; font-size:14px; }"
        "QToolButton[role=\"collapseCard\"] { border:0; color:#64748b; font-size:18px; }"
        "QToolButton[actionRole=\"toolbarIcon\"] { background:#ffffff; border:1px solid #cfd6df; border-radius:4px; color:#111827; }"
        "QToolButton[actionRole=\"toolbarIcon\"]:checked { background:#ffffff; border-color:#ff7a00; color:#111827; }"
        "QLineEdit, QComboBox, QSpinBox, QListWidget { background:#ffffff; border:1px solid #cfd6df; border-radius:4px; min-height:32px; color:#111827; }"
        "QComboBox QAbstractItemView, QListWidget::item { background:#ffffff; color:#111827; selection-background-color:#e5f0fb; selection-color:#111827; outline:0; }"
        "QListWidget#roiSampleListWidget { padding:6px; }"
        "QListWidget#roiSampleListWidget::item { min-width:86px; min-height:74px; margin:4px; border:1px solid #d7dde6; border-radius:4px; }"
        "QCheckBox { color:#111827; background:#ffffff; }"
        "QPushButton { background:#ffffff; color:#111827; border:1px solid #cfd6df; border-radius:4px; padding:8px 18px; font-size:14px; }"
        "QPushButton[actionRole=\"primary\"], QPushButton[actionRole=\"secondary\"], QPushButton[actionRole=\"plain\"] { background:#ffffff; color:#111827; border:1px solid #cfd6df; border-radius:4px; }"
        "QPushButton:disabled, QToolButton:disabled, QComboBox:disabled, QSpinBox:disabled, QLineEdit:disabled { background:#ffffff; color:#111827; border-color:#d7dde6; }"
        "QLabel#viewerTitleLabel, QLabel#statusLabel { color:#f5f7fb; }"
        "QGraphicsView { border:1px solid #ff7a00; background:#11151b; }"));

    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QFrame *headerFrame = new QFrame;
    headerFrame->setObjectName(QStringLiteral("headerFrame"));
    headerFrame->setProperty("colorTemplateDragHandle", true);
    headerFrame->installEventFilter(this);
    headerFrame->setMinimumHeight(42);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(18, 0, 12, 0);
    QLabel *headerTitle = new QLabel(tr("颜色模板创建"));
    headerTitle->setProperty("colorTemplateDragHandle", true);
    headerTitle->installEventFilter(this);
    headerTitle->setStyleSheet(QStringLiteral("color:#ffffff; font-size:15px; font-weight:600;"));
    QToolButton *closeButton = new QToolButton;
    closeButton->setText(QStringLiteral("×"));
    closeButton->setStyleSheet(QStringLiteral("color:#ffffff; border:0; font-size:20px;"));
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch(1);
    headerLayout->addWidget(closeButton);
    root->addWidget(headerFrame);
    connect(closeButton, &QToolButton::clicked, this, &ColorTemplateDialog::reject);

    QHBoxLayout *content = new QHBoxLayout;
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(0);
    root->addLayout(content, 1);

    QFrame *leftPanel = new QFrame;
    leftPanel->setObjectName(QStringLiteral("leftPanel"));
    leftPanel->setMinimumWidth(380);
    leftPanel->setMaximumWidth(430);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(24, 18, 24, 18);
    leftLayout->setSpacing(14);

    QVBoxLayout *modelLayout = nullptr;
    QFrame *modelCard = makeCard(tr("模板信息"), &modelLayout);
    QHBoxLayout *nameRow = new QHBoxLayout;
    m_templateNameLineEdit = new QLineEdit(tr("颜色模板"));
    nameRow->addWidget(rowLabel(tr("模板名")));
    nameRow->addWidget(m_templateNameLineEdit, 1);
    modelLayout->addLayout(nameRow);

    QHBoxLayout *featureRow = new QHBoxLayout;
    m_featureTypeComboBox = new QComboBox;
    m_featureTypeComboBox->addItem(tr("直方图特征"));
    m_featureTypeComboBox->addItem(tr("色谱特征"));
    featureRow->addWidget(rowLabel(tr("特征类型")));
    featureRow->addWidget(m_featureTypeComboBox, 1);
    modelLayout->addLayout(featureRow);
    leftLayout->addWidget(modelCard);

    QVBoxLayout *labelLayout = nullptr;
    QFrame *labelCard = makeCard(tr("标签与样本"), &labelLayout);
    QHBoxLayout *imageButtonRow = new QHBoxLayout;
    m_addCurrentImageButton = new QPushButton(tr("添加当前图像"));
    m_addImageButton = new QPushButton(tr("添加图片"));
    for (QPushButton *button : {m_addCurrentImageButton, m_addImageButton})
        button->setProperty("actionRole", QStringLiteral("plain"));
    imageButtonRow->addWidget(m_addCurrentImageButton);
    imageButtonRow->addWidget(m_addImageButton);
    labelLayout->addLayout(imageButtonRow);

    m_labelListWidget = new QListWidget;
    m_labelListWidget->setFixedHeight(112);
    labelLayout->addWidget(m_labelListWidget);

    QHBoxLayout *labelButtonRow = new QHBoxLayout;
    m_addLabelButton = new QPushButton(tr("添加"));
    m_renameLabelButton = new QPushButton(tr("重命名"));
    m_deleteLabelButton = new QPushButton(tr("删除"));
    for (QPushButton *button : {m_addLabelButton, m_renameLabelButton, m_deleteLabelButton})
        button->setProperty("actionRole", QStringLiteral("plain"));
    labelButtonRow->addWidget(m_addLabelButton);
    labelButtonRow->addWidget(m_renameLabelButton);
    labelButtonRow->addWidget(m_deleteLabelButton);
    labelLayout->addLayout(labelButtonRow);

    QHBoxLayout *sampleCountRow = new QHBoxLayout;
    m_sampleCountLabel = new QLabel(QStringLiteral("0 / 总计 0"));
    m_sampleCountLabel->setAlignment(Qt::AlignCenter);
    m_sampleCountLabel->setMinimumHeight(32);
    m_sampleCountLabel->setStyleSheet(QStringLiteral(
        "QLabel { background:#ffffff; color:#111827; border:1px solid #cfd6df; border-radius:4px; }"));
    sampleCountRow->addWidget(rowLabel(tr("样本数量")));
    sampleCountRow->addWidget(m_sampleCountLabel, 1);
    labelLayout->addLayout(sampleCountRow);

    m_roiSampleListWidget = new QListWidget;
    m_roiSampleListWidget->setObjectName(QStringLiteral("roiSampleListWidget"));
    m_roiSampleListWidget->setFixedHeight(116);
    m_roiSampleListWidget->setViewMode(QListView::IconMode);
    m_roiSampleListWidget->setMovement(QListView::Static);
    m_roiSampleListWidget->setResizeMode(QListView::Adjust);
    m_roiSampleListWidget->setWrapping(true);
    m_roiSampleListWidget->setSpacing(6);
    m_roiSampleListWidget->setIconSize(QSize(72, 48));
    labelLayout->addWidget(m_roiSampleListWidget);

    QHBoxLayout *roiRow = new QHBoxLayout;
    m_regionRectButton = roiButton(QStringLiteral("□"), tr("矩形样本 ROI"));
    roiRow->addWidget(rowLabel(tr("样本 ROI")));
    roiRow->addStretch(1);
    roiRow->addWidget(m_regionRectButton);
    labelLayout->addLayout(roiRow);

    m_deleteCurrentRoiButton = new QPushButton(tr("删除当前 ROI"));
    m_deleteCurrentRoiButton->setProperty("actionRole", QStringLiteral("plain"));
    m_deleteCurrentRoiButton->setEnabled(false);
    labelLayout->addWidget(m_deleteCurrentRoiButton);

    m_addSampleButton = new QPushButton(tr("添加 ROI 样本"));
    m_addSampleButton->setProperty("actionRole", QStringLiteral("secondary"));
    labelLayout->addWidget(m_addSampleButton);
    leftLayout->addWidget(labelCard);

    QVBoxLayout *advancedLayout = nullptr;
    QFrame *advancedCard = makeCard(tr("高级参数"), &advancedLayout);
    QHBoxLayout *sensitivityRow = new QHBoxLayout;
    m_sensitivityComboBox = new QComboBox;
    m_sensitivityComboBox->addItems({tr("低敏感"), tr("中敏感"), tr("高敏感")});
    sensitivityRow->addWidget(rowLabel(tr("敏感度")));
    sensitivityRow->addWidget(m_sensitivityComboBox, 1);
    advancedLayout->addLayout(sensitivityRow);

    QHBoxLayout *brightnessRow = new QHBoxLayout;
    m_brightnessCheckBox = new QCheckBox(tr("参与直方图"));
    brightnessRow->addWidget(rowLabel(tr("亮度")));
    brightnessRow->addWidget(m_brightnessCheckBox, 1);
    advancedLayout->addLayout(brightnessRow);

    QHBoxLayout *knnRow = new QHBoxLayout;
    m_knnKSpinBox = new QSpinBox;
    m_knnKSpinBox->setRange(1, 99);
    knnRow->addWidget(rowLabel(tr("K 值")));
    knnRow->addWidget(m_knnKSpinBox, 1);
    advancedLayout->addLayout(knnRow);

    QHBoxLayout *distanceRow = new QHBoxLayout;
    m_knnDistanceComboBox = new QComboBox;
    m_knnDistanceComboBox->addItem(tr("HALCON 默认"));
    m_knnDistanceComboBox->setEnabled(false);
    distanceRow->addWidget(rowLabel(tr("KNN 距离")));
    distanceRow->addWidget(m_knnDistanceComboBox, 1);
    advancedLayout->addLayout(distanceRow);
    leftLayout->addWidget(advancedCard);

    leftLayout->addStretch(1);
    QHBoxLayout *bottomButtons = new QHBoxLayout;
    m_cancelButton = new QPushButton(tr("取消"));
    m_saveButton = new QPushButton(tr("完成"));
    m_cancelButton->setProperty("actionRole", QStringLiteral("secondary"));
    m_saveButton->setProperty("actionRole", QStringLiteral("primary"));
    bottomButtons->addStretch(1);
    bottomButtons->addWidget(m_cancelButton);
    bottomButtons->addWidget(m_saveButton);
    leftLayout->addLayout(bottomButtons);
    QScrollArea *leftScrollArea = new QScrollArea;
    leftScrollArea->setWidgetResizable(true);
    leftScrollArea->setFrameShape(QFrame::NoFrame);
    leftScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftScrollArea->setWidget(leftPanel);
    leftScrollArea->setMinimumWidth(390);
    leftScrollArea->setMaximumWidth(450);
    content->addWidget(leftScrollArea);

    QFrame *previewPanel = new QFrame;
    previewPanel->setObjectName(QStringLiteral("previewPanel"));
    QVBoxLayout *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(14, 12, 14, 8);
    previewLayout->setSpacing(8);
    m_viewerTitleLabel = new QLabel(tr("基准图"));
    m_viewerTitleLabel->setObjectName(QStringLiteral("viewerTitleLabel"));
    m_previewGraphicsView = new QGraphicsView;
    m_statusLabel = new QLabel(tr("矩形样本 ROI x=0.000 y=0.000 w=1.000 h=1.000"));
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    m_statusLabel->setMinimumHeight(28);
    m_statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_statusLabel->setWordWrap(false);
    previewLayout->addWidget(m_viewerTitleLabel);
    previewLayout->addWidget(m_previewGraphicsView, 1);
    previewLayout->addWidget(m_statusLabel);
    content->addWidget(previewPanel, 1);

    m_previewHelper = new FrameViewHelper(m_previewGraphicsView, this);
}

void ColorTemplateDialog::setupUiState()
{
    m_brightnessCheckBox->setChecked(true);
    m_sensitivityComboBox->setCurrentIndex(1);
    m_knnKSpinBox->setValue(3);
    m_regionRectButton->setChecked(true);
    if (QStandardItemModel *model = qobject_cast<QStandardItemModel *>(m_featureTypeComboBox->model())) {
        if (QStandardItem *item = model->item(1))
            item->setEnabled(false);
    }
    ensureDefaultLabel();
    updateLabelList();
    updateRoiSampleList();
    updateSampleCount();
}

void ColorTemplateDialog::connectControls()
{
    connect(m_cancelButton, &QPushButton::clicked, this, &ColorTemplateDialog::reject);
    connect(m_saveButton, &QPushButton::clicked, this, &ColorTemplateDialog::accept);
    connect(m_addLabelButton, &QPushButton::clicked, this, &ColorTemplateDialog::addLabel);
    connect(m_renameLabelButton, &QPushButton::clicked, this, &ColorTemplateDialog::renameCurrentLabel);
    connect(m_deleteLabelButton, &QPushButton::clicked, this, &ColorTemplateDialog::deleteCurrentLabel);
    connect(m_addCurrentImageButton, &QPushButton::clicked, this, &ColorTemplateDialog::addCurrentImage);
    connect(m_addImageButton, &QPushButton::clicked, this, &ColorTemplateDialog::addImageFromPc);
    connect(m_deleteCurrentRoiButton, &QPushButton::clicked, this, &ColorTemplateDialog::deleteCurrentRoiSample);
    connect(m_addSampleButton, &QPushButton::clicked, this, &ColorTemplateDialog::addSampleFromCurrentRoi);
    connect(m_labelListWidget, &QListWidget::currentRowChanged, this, [this]() {
        updateRoiSampleList();
        updateSampleCount();
    });
    connect(m_roiSampleListWidget, &QListWidget::currentRowChanged, this, [this]() {
        m_deleteCurrentRoiButton->setEnabled(currentSampleIndex() >= 0);
    });
    connect(m_regionRectButton, &QToolButton::clicked, this, &ColorTemplateDialog::startRectangleRoiEditing);

    if (m_previewHelper) {
        connect(m_previewHelper, &FrameViewHelper::roiChanged, this, &ColorTemplateDialog::handleRoiChanged);
        connect(m_previewHelper, &FrameViewHelper::roiSelectionRejected, this, [this](const QRectF &) {
            handleRoiSelectionRejected();
        });
    }

    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameChanged,
            this,
            [this](const QImage &) {
        showPreviewImage();
    });
}

void ColorTemplateDialog::addLabel()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               tr("添加标签"),
                                               tr("标签名"),
                                               QLineEdit::Normal,
                                               tr("类别%1").arg(nextClassId()),
                                               &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    for (const ColorRecognitionLabelData &label : m_template.labels) {
        if (label.name == name) {
            QMessageBox::warning(this, tr("颜色模板"), tr("标签已存在"));
            return;
        }
    }

    ColorRecognitionLabelData label;
    label.name = name;
    label.classId = nextClassId();
    m_template.labels.append(label);
    updateLabelList();
    m_labelListWidget->setCurrentRow(m_template.labels.size() - 1);
}

void ColorTemplateDialog::renameCurrentLabel()
{
    const int index = m_labelListWidget->currentRow();
    if (index < 0 || index >= m_template.labels.size())
        return;

    bool ok = false;
    const QString oldName = m_template.labels.at(index).name;
    const QString newName = QInputDialog::getText(this,
                                                  tr("重命名标签"),
                                                  tr("标签名"),
                                                  QLineEdit::Normal,
                                                  oldName,
                                                  &ok).trimmed();
    if (!ok || newName.isEmpty() || newName == oldName)
        return;

    for (int i = 0; i < m_template.labels.size(); ++i) {
        if (i != index && m_template.labels.at(i).name == newName) {
            QMessageBox::warning(this, tr("颜色模板"), tr("标签已存在"));
            return;
        }
    }

    const int classId = m_template.labels.at(index).classId;
    m_template.labels[index].name = newName;
    for (ColorRecognitionSampleData &sample : m_template.samples) {
        if (sample.classId == classId)
            sample.label = newName;
    }
    updateLabelList();
    m_labelListWidget->setCurrentRow(index);
}

void ColorTemplateDialog::deleteCurrentLabel()
{
    const int index = m_labelListWidget->currentRow();
    if (index < 0 || index >= m_template.labels.size())
        return;

    const int classId = m_template.labels.at(index).classId;
    m_template.labels.removeAt(index);
    m_template.samples.erase(std::remove_if(m_template.samples.begin(),
                                            m_template.samples.end(),
                                            [classId](const ColorRecognitionSampleData &sample) {
        return sample.classId == classId;
    }), m_template.samples.end());
    ensureDefaultLabel();
    updateLabelList();
    m_labelListWidget->setCurrentRow(qMin(index, m_template.labels.size() - 1));
    updateRoiSampleList();
    updateSampleCount();
}

void ColorTemplateDialog::addSampleFromCurrentRoi()
{
    if (!m_addSampleButton || !m_addSampleButton->isEnabled())
        return;

    const QString labelName = currentLabelName();
    const int classId = currentClassId();
    if (labelName.isEmpty() || classId <= 0) {
        QMessageBox::warning(this, tr("颜色模板"), tr("请先创建标签"));
        return;
    }

    cv::Mat frame = m_sampleImage.empty()
            ? ReferenceImageProvider::instance().referenceFrame()
            : m_sampleImage;
    if (frame.empty())
        frame = CameraFrameProvider::instance().currentFrame();
    if (frame.empty()) {
        setStatusText(tr("当前无基准图或相机图像，无法添加样本"));
        return;
    }

    m_addSampleButton->setEnabled(false);
    const ColorRecognitionHalconFeatureResult featureResult = [&]() {
        try {
            return m_featureRunner.extractFeature(frame, featureExtractionConfig());
        } catch (const std::exception &error) {
            ColorRecognitionHalconFeatureResult result;
            result.success = false;
            result.status = QStringLiteral("exception");
            result.message = QString::fromLocal8Bit(error.what());
            return result;
        } catch (...) {
            ColorRecognitionHalconFeatureResult result;
            result.success = false;
            result.status = QStringLiteral("exception");
            result.message = tr("添加 ROI 样本时发生未知异常");
            return result;
        }
    }();
    m_addSampleButton->setEnabled(true);

    if (!featureResult.success) {
        setStatusText(featureResult.message);
        QMessageBox::warning(this, tr("颜色模板"), featureResult.message);
        return;
    }

    ColorRecognitionSampleData sample;
    sample.label = labelName;
    sample.classId = classId;
    sample.feature = featureResult.feature;
    sample.roiNormalized = effectiveRoiNormalized();
    const QImage roiImage = cropRoiImage(sample.roiNormalized);
    if (!roiImage.isNull()) {
        QByteArray imageBytes;
        QBuffer buffer(&imageBytes);
        buffer.open(QIODevice::WriteOnly);
        roiImage.save(&buffer, "PNG");
        sample.roiImagePngBase64 = QString::fromLatin1(imageBytes.toBase64());
        sample.roiImageWidth = roiImage.width();
        sample.roiImageHeight = roiImage.height();
    }
    m_template.samples.append(sample);
    updateLabelList();
    updateRoiSampleList();
    updateSampleCount();

    const QString text = tr("已添加样本：%1，特征维度 %2")
            .arg(labelName)
            .arg(featureResult.feature.size());
    setStatusText(text, text);
}

void ColorTemplateDialog::addCurrentImage()
{
    cv::Mat frame = ReferenceImageProvider::instance().referenceFrame();
    QImage displayImage = ReferenceImageProvider::instance().referenceImage();
    QString title = tr("基准图");
    if (frame.empty()) {
        frame = CameraFrameProvider::instance().currentFrame();
        displayImage = CameraFrameProvider::instance().currentImage();
        title = tr("当前图像");
    }

    if (frame.empty() || displayImage.isNull()) {
        setStatusText(tr("当前无基准图或相机图像，无法添加当前图像"));
        return;
    }

    m_sampleImage = frame.clone();
    m_sampleDisplayImage = displayImage.copy();
    m_sampleImageTitle = title;
    showPreviewImage();
    setStatusText(tr("已添加当前图像作为模板样本图像"));
}

void ColorTemplateDialog::addImageFromPc()
{
    const QString fileName = QFileDialog::getOpenFileName(
                this,
                tr("添加图片"),
                QString(),
                tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All files (*.*)"));
    if (fileName.trimmed().isEmpty())
        return;

    cv::Mat frame = cv::imread(fileName.toLocal8Bit().constData(), cv::IMREAD_COLOR);
    if (frame.empty()) {
        QMessageBox::warning(this, tr("添加图片"), tr("无法读取所选图片"));
        return;
    }

    m_sampleImage = frame.clone();
    m_sampleDisplayImage = MatImageConverter::matToDisplayImage(m_sampleImage, QStringLiteral("ColorTemplateDialog"));
    m_sampleImageTitle = QFileInfo(fileName).fileName();
    showPreviewImage();
    setStatusText(tr("已添加图片：%1").arg(m_sampleImageTitle));
}

void ColorTemplateDialog::deleteCurrentRoiSample()
{
    const int sampleIndex = currentSampleIndex();
    if (sampleIndex < 0 || sampleIndex >= m_template.samples.size())
        return;

    m_template.samples.removeAt(sampleIndex);
    updateLabelList();
    updateRoiSampleList();
    updateSampleCount();
    setStatusText(tr("已删除当前 ROI 样本"));
}

void ColorTemplateDialog::startRectangleRoiEditing()
{
    m_regionRectButton->setChecked(true);
    showPreviewImage();

    if (!m_previewHelper || !m_previewHelper->hasImage())
        return;

    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    m_previewHelper->setRoiDrawingEnabled(true);
    setStatusText(tr("请框选颜色样本矩形 ROI"));
}

void ColorTemplateDialog::showUnsupportedRegionMessage()
{
    startRectangleRoiEditing();
}

void ColorTemplateDialog::handleRoiChanged(const QRectF &roi)
{
    m_sampleRoiNormalized = normalizedRoiOrDefault(roi);
    m_regionRectButton->setChecked(true);
    if (m_previewHelper) {
        m_previewHelper->clearToolOverlays();
        m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    }
    const QRectF rect = effectiveRoiNormalized();
    const QString text = tr("矩形样本 ROI x=%1 y=%2 w=%3 h=%4")
            .arg(rect.x(), 0, 'f', 3)
            .arg(rect.y(), 0, 'f', 3)
            .arg(rect.width(), 0, 'f', 3)
            .arg(rect.height(), 0, 'f', 3);
    setStatusText(text, text);
    qDebug() << "[ColorTemplateDialog] ROI normalized:" << m_sampleRoiNormalized;
}

void ColorTemplateDialog::handleRoiSelectionRejected()
{
    setStatusText(tr("ROI 无效，请拖拽宽高至少 2 像素的矩形"));
    refreshDisplayedRoiOverlay();
}

void ColorTemplateDialog::showPreviewImage()
{
    if (!m_previewHelper)
        return;

    QImage image = ReferenceImageProvider::instance().referenceImage();
    QString title = tr("基准图");
    if (!m_sampleDisplayImage.isNull()) {
        image = m_sampleDisplayImage;
        title = m_sampleImageTitle.trimmed().isEmpty() ? tr("样本图像") : m_sampleImageTitle;
    }
    if (image.isNull()) {
        image = CameraFrameProvider::instance().currentImage();
        title = tr("当前图像");
    }

    if (image.isNull()) {
        m_previewHelper->clear();
        m_viewerTitleLabel->setText(tr("当前无图像"));
        setStatusText(tr("当前无图像，ROI 默认全图"));
        return;
    }

    m_viewerTitleLabel->setText(title);
    m_previewHelper->setImage(image);
    refreshDisplayedRoiOverlay();
}

void ColorTemplateDialog::refreshDisplayedRoiOverlay()
{
    if (!m_previewHelper)
        return;

    m_previewHelper->clearRoi();
    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
}

void ColorTemplateDialog::fitPreview()
{
    if (m_previewHelper)
        m_previewHelper->fitToView();
}

void ColorTemplateDialog::updateLabelList()
{
    const int currentClassIdValue = currentClassId();
    const QSignalBlocker block(m_labelListWidget);
    m_labelListWidget->clear();

    int rowToSelect = 0;
    for (int i = 0; i < m_template.labels.size(); ++i) {
        const ColorRecognitionLabelData &label = m_template.labels.at(i);
        int count = 0;
        for (const ColorRecognitionSampleData &sample : m_template.samples) {
            if (sample.classId == label.classId)
                ++count;
        }
        QListWidgetItem *item = new QListWidgetItem(tr("%1  (%2)").arg(label.name).arg(count));
        item->setData(Qt::UserRole, label.classId);
        m_labelListWidget->addItem(item);
        if (label.classId == currentClassIdValue)
            rowToSelect = i;
    }

    if (m_labelListWidget->count() > 0)
        m_labelListWidget->setCurrentRow(qBound(0, rowToSelect, m_labelListWidget->count() - 1));
}

void ColorTemplateDialog::updateRoiSampleList()
{
    const int currentSample = currentSampleIndex();
    const int classId = currentClassId();
    const QSignalBlocker block(m_roiSampleListWidget);
    m_roiSampleListWidget->clear();

    int rowToSelect = -1;
    for (int i = 0; i < m_template.samples.size(); ++i) {
        const ColorRecognitionSampleData &sample = m_template.samples.at(i);
        if (sample.classId != classId)
            continue;

        QListWidgetItem *item = new QListWidgetItem(
                    QIcon(QPixmap::fromImage(roiThumbnailForSample(sample))),
                    tr("ROI %1").arg(m_roiSampleListWidget->count() + 1));
        item->setData(Qt::UserRole, i);
        item->setTextAlignment(Qt::AlignCenter);
        item->setSizeHint(QSize(88, 78));
        m_roiSampleListWidget->addItem(item);
        if (i == currentSample)
            rowToSelect = m_roiSampleListWidget->count() - 1;
    }

    if (rowToSelect >= 0)
        m_roiSampleListWidget->setCurrentRow(rowToSelect);
    m_deleteCurrentRoiButton->setEnabled(currentSampleIndex() >= 0);
}

QImage ColorTemplateDialog::currentDisplayImageForSamples() const
{
    QImage source = m_sampleDisplayImage;
    if (source.isNull())
        source = ReferenceImageProvider::instance().referenceImage();
    if (source.isNull())
        source = CameraFrameProvider::instance().currentImage();
    return source;
}

QImage ColorTemplateDialog::cropRoiImage(const QRectF &roiNormalized) const
{
    const QImage source = currentDisplayImageForSamples();
    if (source.isNull())
        return QImage();

    const QRectF roi = normalizedRoiOrDefault(roiNormalized);
    const QRect sourceRect(qBound(0, static_cast<int>(std::floor(roi.x() * source.width())), source.width() - 1),
                           qBound(0, static_cast<int>(std::floor(roi.y() * source.height())), source.height() - 1),
                           qMax(1, static_cast<int>(std::ceil(roi.width() * source.width()))),
                           qMax(1, static_cast<int>(std::ceil(roi.height() * source.height()))));
    return source.copy(sourceRect.intersected(source.rect()));
}

QImage ColorTemplateDialog::roiThumbnailForSample(const ColorRecognitionSampleData &sample) const
{
    const QSize targetSize(72, 48);
    QImage thumbnail(targetSize, QImage::Format_ARGB32_Premultiplied);
    thumbnail.fill(Qt::white);

    QPainter painter(&thumbnail);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 122, 0), 2));

    QImage cropped;
    if (!sample.roiImagePngBase64.trimmed().isEmpty()) {
        cropped.loadFromData(QByteArray::fromBase64(sample.roiImagePngBase64.toLatin1()), "PNG");
    }
    if (cropped.isNull())
        cropped = cropRoiImage(sample.roiNormalized);

    if (!cropped.isNull()) {
        painter.drawImage(thumbnail.rect(),
                          cropped.scaled(targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation),
                          QRect(QPoint(0, 0), targetSize));
    } else {
        painter.setPen(QPen(QColor(203, 213, 225), 1));
        painter.drawText(thumbnail.rect(), Qt::AlignCenter, tr("ROI"));
        painter.setPen(QPen(QColor(255, 122, 0), 2));
    }

    painter.drawRect(thumbnail.rect().adjusted(1, 1, -2, -2));
    return thumbnail;
}

void ColorTemplateDialog::updateSampleCount()
{
    const int classId = currentClassId();
    int labelSamples = 0;
    for (const ColorRecognitionSampleData &sample : m_template.samples) {
        if (sample.classId == classId)
            ++labelSamples;
    }
    m_sampleCountLabel->setText(tr("%1 / 总计 %2").arg(labelSamples).arg(m_template.samples.size()));
}

void ColorTemplateDialog::ensureDefaultLabel()
{
    if (!m_template.labels.isEmpty())
        return;

    ColorRecognitionLabelData label;
    label.name = tr("类别1");
    label.classId = 1;
    m_template.labels.append(label);
}

int ColorTemplateDialog::nextClassId() const
{
    int maxId = 0;
    for (const ColorRecognitionLabelData &label : m_template.labels)
        maxId = qMax(maxId, label.classId);
    return maxId + 1;
}

int ColorTemplateDialog::currentClassId() const
{
    QListWidgetItem *item = m_labelListWidget->currentItem();
    return item ? item->data(Qt::UserRole).toInt() : 0;
}

int ColorTemplateDialog::currentSampleIndex() const
{
    QListWidgetItem *item = m_roiSampleListWidget ? m_roiSampleListWidget->currentItem() : nullptr;
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

QString ColorTemplateDialog::currentLabelName() const
{
    const int classId = currentClassId();
    for (const ColorRecognitionLabelData &label : m_template.labels) {
        if (label.classId == classId)
            return label.name;
    }
    return QString();
}

QRectF ColorTemplateDialog::effectiveRoiNormalized() const
{
    return normalizedRoiOrDefault(m_sampleRoiNormalized);
}

ColorRecognitionHalconConfig ColorTemplateDialog::featureExtractionConfig() const
{
    ColorRecognitionHalconConfig config;
    QStringList tried;
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(QString(), &tried);
    config.halconSoPathCandidates = tried;
    config.roiNormalized = effectiveRoiNormalized();
    config.featureType = featureTypeFromUi(m_featureTypeComboBox->currentText());
    config.sensitivity = sensitivityFromUi(m_sensitivityComboBox->currentText());
    config.brightnessEnabled = m_brightnessCheckBox->isChecked();
    config.knnK = m_knnKSpinBox->value();
    return config;
}

void ColorTemplateDialog::setStatusText(const QString &displayText, const QString &tooltipText)
{
    const int width = qMax(120, m_statusLabel->contentsRect().width());
    m_statusLabel->setText(m_statusLabel->fontMetrics().elidedText(displayText, Qt::ElideRight, width));
    m_statusLabel->setToolTip(tooltipText.isEmpty() ? displayText : tooltipText);
}
