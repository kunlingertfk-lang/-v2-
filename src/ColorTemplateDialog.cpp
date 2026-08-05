#include "ColorTemplateDialog.h"
#include "ui_ColorTemplateDialog.h"

#include "algorithms/halcon/HalconRuntimePaths.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "UiStyleRoles.h"
#include "PlanDialogUtils.h"

#include <QButtonGroup>
#include <QBuffer>
#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QEvent>
#include <QEventLoop>
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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QListWidgetItem>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QColor>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>
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
    if (text.contains(QStringLiteral("色谱")))
        return QStringLiteral("spectrum");
    if (text.contains(QStringLiteral("二维")))
        return QStringLiteral("histogram_2dim_hs");
    return QStringLiteral("histogram");
}

QString featureTypeToUi(const QString &value)
{
    if (value == QStringLiteral("spectrum"))
        return QStringLiteral("色谱特征（预留）");
    if (value == QStringLiteral("histogram_2dim_hs") ||
        value == QStringLiteral("histo_2dim") ||
        value == QStringLiteral("histogram_2dim")) {
        return QStringLiteral("二维 H/S 直方图（推荐）");
    }
    return QStringLiteral("一维 H/S(/V) 直方图（兼容）");
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

QString recognitionBackendFromUi(const QString &text)
{
    return text.contains(QStringLiteral("GMM"))
            ? QStringLiteral("cielab_gmm")
            : QStringLiteral("hsv_histogram");
}

QString recognitionBackendToUi(const QString &value)
{
    return value == QStringLiteral("cielab_gmm")
            ? QStringLiteral("CIELAB GMM（推荐复杂颜色）")
            : QStringLiteral("HSV 直方图（推荐纯色/少样本）");
}

QString canonicalImageHash(const cv::Mat &image)
{
    if (image.empty())
        return QString();
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArray::number(image.rows) + 'x' + QByteArray::number(image.cols) + ':' +
                 QByteArray::number(image.type()) + ':');
    const int rowBytes = image.cols * static_cast<int>(image.elemSize());
    for (int row = 0; row < image.rows; ++row)
        hash.addData(reinterpret_cast<const char *>(image.ptr(row)), rowBytes);
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(hash.result().toHex()));
}

QString hsvModelState(const ColorRecognitionTemplateData &colorTemplate)
{
    if (colorTemplate.samples.isEmpty())
        return QStringLiteral("empty");
    QString signature;
    for (const ColorRecognitionSampleData &sample : colorTemplate.samples) {
        if (sample.feature.isEmpty() || sample.featureSignature.trimmed().isEmpty())
            return QStringLiteral("stale");
        if (signature.isEmpty())
            signature = sample.featureSignature;
        else if (sample.featureSignature != signature)
            return QStringLiteral("stale");
    }
    return QStringLiteral("ready");
}

QString gmmPixelFormat(const cv::Mat &image, const FrameInputMetadata &metadata)
{
    const QString requested = metadata.pixelFormat.trimmed().toUpper();
    const QStringList valid = image.depth() == CV_16U
            ? (image.channels() == 3
               ? QStringList{QStringLiteral("BGR16"), QStringLiteral("RGB16")}
               : QStringList{QStringLiteral("BGRA16"), QStringLiteral("RGBA16")})
            : (image.channels() == 3
               ? QStringList{QStringLiteral("BGR8"), QStringLiteral("RGB8")}
               : QStringList{QStringLiteral("BGRA8"), QStringLiteral("RGBA8")});
    if (valid.contains(requested))
        return requested;
    if (image.channels() == 3)
        return image.depth() == CV_16U ? QStringLiteral("BGR16") : QStringLiteral("BGR8");
    if (image.channels() == 4)
        return image.depth() == CV_16U ? QStringLiteral("BGRA16") : QStringLiteral("BGRA8");
    return requested;
}

void setComboBoxText(QComboBox *comboBox, const QString &text)
{
    if (!comboBox)
        return;

    const int index = comboBox->findText(text);
    if (index >= 0)
        comboBox->setCurrentIndex(index);
}

} // namespace

QString colorRecognitionGmmTrainingDataHash(const ColorRecognitionTemplateData &colorTemplate)
{
    QVector<ColorRecognitionSampleData> samples = colorTemplate.samples;
    std::sort(samples.begin(), samples.end(), [](const auto &lhs, const auto &rhs) {
        if (lhs.classId != rhs.classId)
            return lhs.classId < rhs.classId;
        return lhs.sampleId < rhs.sampleId;
    });
    QSet<QString> sampleIds;
    QJsonArray array;
    for (const ColorRecognitionSampleData &sample : samples) {
        const QString sampleId = sample.sampleId.trimmed();
        if (sampleId.isEmpty() || sampleIds.contains(sampleId) ||
            sample.gmmImageSha256.trimmed().isEmpty()) {
            return QString();
        }
        sampleIds.insert(sampleId);
        QJsonObject item;
        item.insert(QStringLiteral("sampleId"), sampleId);
        item.insert(QStringLiteral("classId"), sample.classId);
        item.insert(QStringLiteral("label"), sample.label.trimmed());
        item.insert(QStringLiteral("imageSha256"), sample.gmmImageSha256.trimmed());
        item.insert(QStringLiteral("pixelFormat"), sample.pixelFormat.trimmed().toUpper());
        item.insert(QStringLiteral("validBits"), sample.pixelFormat.endsWith(QStringLiteral("8"))
                    ? 8 : sample.validBits);
        item.insert(QStringLiteral("bitShift"), sample.pixelFormat.endsWith(QStringLiteral("8"))
                    ? 0 : sample.bitShift);
        item.insert(QStringLiteral("roiX"), 0.0);
        item.insert(QStringLiteral("roiY"), 0.0);
        item.insert(QStringLiteral("roiWidth"), 1.0);
        item.insert(QStringLiteral("roiHeight"), 1.0);
        array.append(item);
    }
    QJsonObject contract;
    contract.insert(QStringLiteral("samples"), array);
    return QStringLiteral("sha256:%1").arg(QString::fromLatin1(
                QCryptographicHash::hash(QJsonDocument(contract).toJson(QJsonDocument::Compact),
                                         QCryptographicHash::Sha256).toHex()));
}

ColorTemplateDialog::ColorTemplateDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::ColorTemplateDialog)
{
    buildUi();
    PlanDialogUtils::applyToolLevelStyle(this);
    adjustInitialGeometry();
    setupUiState();
    connectControls();
    showPreviewImage();
}

ColorTemplateDialog::~ColorTemplateDialog()
{
    delete ui;
}

ColorRecognitionTemplateData ColorTemplateDialog::templateData() const
{
    ColorRecognitionTemplateData data = m_template;
    data.name = m_templateNameLineEdit->text().trimmed();
    if (data.name.isEmpty())
        data.name = QStringLiteral("颜色模板");
    data.modelSchemaVersion = 3;
    data.recognitionBackend = recognitionBackendFromUi(m_recognitionBackendComboBox->currentText());
    data.featureType = featureTypeFromUi(m_featureTypeComboBox->currentText());
    data.sensitivity = sensitivityFromUi(m_sensitivityComboBox->currentText());
    if (data.recognitionBackend == QStringLiteral("hsv_histogram")) {
        data.brightnessEnabled = data.featureType == QStringLiteral("histogram") &&
                m_brightnessCheckBox->isChecked();
    } else {
        data.gmmColorChannels = m_brightnessCheckBox->isChecked()
                ? QStringLiteral("lab") : QStringLiteral("ab");
    }
    data.modelState = hsvModelState(data);
    return data;
}

void ColorTemplateDialog::setTemplateData(const ColorRecognitionTemplateData &data)
{
    setEditState(EditState::None);
    // 完整样本图只属于本次对话框会话，不随模板切换或保存持久化。
    m_sampleImage.release();
    m_sampleImageMetadata = FrameInputMetadata();
    m_sampleDisplayImage = QImage();
    m_sampleImageTitle.clear();
    m_template = data;
    if (m_template.templateId.trimmed().isEmpty())
        m_template.templateId = QStringLiteral("color_template");
    m_templateNameLineEdit->setText(m_template.name);
    setComboBoxText(m_recognitionBackendComboBox,
                    recognitionBackendToUi(m_template.recognitionBackend));
    setComboBoxText(m_featureTypeComboBox, featureTypeToUi(m_template.featureType));
    if (m_featureTypeComboBox->currentText().contains(QStringLiteral("色谱")))
        m_featureTypeComboBox->setCurrentIndex(0);
    setComboBoxText(m_sensitivityComboBox, sensitivityToUi(m_template.sensitivity));
    m_brightnessCheckBox->setChecked(m_template.recognitionBackend == QStringLiteral("cielab_gmm")
                                     ? m_template.gmmColorChannels == QStringLiteral("lab")
                                     : m_template.brightnessEnabled);
    updateAlgorithmUi();
    ensureDefaultLabel();
    updateLabelList();
    updateRoiSampleList();
    updateSampleCount();
    if (m_roiSampleListWidget->count() > 0) {
        m_roiSampleListWidget->setCurrentRow(0);
        const int sampleIndex = currentSampleIndex();
        QString restoreError;
        if (!restoreSampleContext(sampleIndex, &restoreError))
            setStatusText(restoreError, restoreError);
    }
    updateAlgorithmUi();
}

void ColorTemplateDialog::setInitialSampleRoi(const QRectF &roi)
{
    // Existing templates restore their current training context from the persisted
    // sample ROI. The main detection ROI is only an initial value for a new template.
    if (!m_template.samples.isEmpty())
        return;
    m_sampleRoiNormalized = normalizedRoiOrDefault(roi);
    refreshDisplayedRoiOverlay();
}

void ColorTemplateDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
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

#if 0
void ColorTemplateDialog::buildLegacyUi()
{
    setWindowTitle(tr("创建颜色模板"));
    setMinimumSize(720, 520);
    resize(1080, 680);
    setWindowModality(Qt::WindowModal);

    setStyleSheet(QStringLiteral(
        "QDialog { background:#ffffff; color:#111827; }"
        "QWidget { background:#ffffff; color:#111827; }"
        "QFrame#headerFrame, QFrame#leftPanel, QFrame#previewPanel { background:#ffffff; color:#111827; }"
        "QFrame[panelRole=\"configCard\"] { background:#ffffff; color:#111827; border:1px solid #cfd6df; border-radius:6px; }"
        "QLabel { background:#ffffff; color:#111827; font-size:14px; }"
        "QLabel[role=\"cardTitle\"] { background:#ffffff; color:#111827; font-size:17px; font-weight:700; }"
        "QLabel[role=\"rowField\"] { background:#ffffff; color:#111827; font-size:14px; }"
        "QToolButton[role=\"collapseCard\"] { background:#ffffff; border:1px solid #cfd6df; color:#111827; font-size:18px; }"
        "QToolButton[actionRole=\"toolbarIcon\"] { background:#ffffff; border:1px solid #cfd6df; border-radius:4px; color:#111827; }"
        "QToolButton[actionRole=\"toolbarIcon\"]:checked { background:#ffffff; border-color:#ff7a00; color:#111827; }"
        "QLineEdit, QComboBox, QSpinBox, QListWidget { background:#ffffff; border:1px solid #cfd6df; border-radius:4px; min-height:32px; color:#111827; }"
        "QComboBox QAbstractItemView, QListWidget::item { background:#ffffff; color:#111827; selection-background-color:#ffffff; selection-color:#111827; outline:0; }"
        "QListWidget#roiSampleListWidget { padding:6px; }"
        "QListWidget#roiSampleListWidget::item { min-width:86px; min-height:74px; margin:4px; border:1px solid #d7dde6; border-radius:4px; }"
        "QListWidget#roiSampleListWidget::item:selected { background:#ffffff; color:#111827; border:2px solid #ff7a00; }"
        "QCheckBox { color:#111827; background:#ffffff; border:1px solid #cfd6df; border-radius:4px; padding:6px 10px; min-height:20px; }"
        "QCheckBox::indicator { width:16px; height:16px; border:1px solid #9aa6b2; border-radius:3px; background:#ffffff; }"
        "QCheckBox::indicator:checked { background:#ff7a00; border-color:#ff7a00; }"
        "QPushButton { background:#ffffff; color:#111827; border:1px solid #cfd6df; border-radius:4px; padding:8px 18px; font-size:14px; }"
        "QPushButton[actionRole=\"primary\"], QPushButton[actionRole=\"secondary\"], QPushButton[actionRole=\"plain\"] { background:#ffffff; color:#111827; border:1px solid #cfd6df; border-radius:4px; }"
        "QPushButton:disabled, QToolButton:disabled, QComboBox:disabled, QSpinBox:disabled, QLineEdit:disabled { background:#ffffff; color:#111827; border-color:#d7dde6; }"
        "QLabel#viewerTitleLabel, QLabel#statusLabel { background:#ffffff; color:#111827; }"
        "QGraphicsView { border:1px solid #ff7a00; background:#ffffff; color:#111827; }"));

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
    headerTitle->setStyleSheet(QStringLiteral("background:#ffffff; color:#111827; font-size:15px; font-weight:600;"));
    QToolButton *closeButton = new QToolButton;
    closeButton->setText(QStringLiteral("×"));
    closeButton->setStyleSheet(QStringLiteral("background:#ffffff; color:#111827; border:0; font-size:20px;"));
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

    m_recognitionBackendComboBox = new QComboBox;
    m_recognitionBackendComboBox->setObjectName(QStringLiteral("recognitionBackendComboBox"));
    m_recognitionBackendComboBox->addItem(tr("HSV 直方图（推荐纯色/少样本）"));
    m_recognitionBackendComboBox->addItem(tr("CIELAB GMM（推荐复杂颜色）"));
    QHBoxLayout *backendRow = new QHBoxLayout;
    backendRow->addWidget(rowLabel(tr("识别算法")));
    backendRow->addWidget(m_recognitionBackendComboBox, 1);
    modelLayout->addLayout(backendRow);

    m_featureTypeComboBox = new QComboBox;
    m_featureTypeComboBox->setObjectName(QStringLiteral("featureTypeComboBox"));
    m_featureTypeComboBox->addItem(tr("二维 H/S 直方图（推荐）"));
    m_featureTypeComboBox->addItem(tr("一维 H/S(/V) 直方图（兼容）"));
    m_featureTypeComboBox->addItem(tr("色谱特征（预留）"));
    m_featureTypeRowWidget = new QWidget(modelCard);
    m_featureTypeRowWidget->setObjectName(QStringLiteral("featureTypeRowWidget"));
    QHBoxLayout *featureRow = new QHBoxLayout(m_featureTypeRowWidget);
    featureRow->setContentsMargins(0, 0, 0, 0);
    featureRow->setSpacing(8);
    featureRow->addWidget(rowLabel(tr("特征类型")));
    featureRow->addWidget(m_featureTypeComboBox, 1);
    modelLayout->addWidget(m_featureTypeRowWidget);
    leftLayout->addWidget(modelCard);

    QVBoxLayout *labelLayout = nullptr;
    QFrame *labelCard = makeCard(tr("标签与样本"), &labelLayout);
    QHBoxLayout *imageButtonRow = new QHBoxLayout;
    m_addCurrentImageButton = new QPushButton(tr("添加当前图像"));
    m_addImageButton = new QPushButton(tr("添加图片"));
    m_addCurrentImageButton->setObjectName(QStringLiteral("addCurrentSampleImageButton"));
    m_addImageButton->setObjectName(QStringLiteral("addExternalSampleImageButton"));
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
    m_regionRectButton->setObjectName(QStringLiteral("sampleRectRoiButton"));
    roiRow->addWidget(rowLabel(tr("样本 ROI")));
    roiRow->addStretch(1);
    roiRow->addWidget(m_regionRectButton);
    labelLayout->addLayout(roiRow);

    m_deleteCurrentRoiButton = new QPushButton(tr("删除当前 ROI"));
    m_deleteCurrentRoiButton->setObjectName(QStringLiteral("deleteCurrentRoiSampleButton"));
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
    m_sensitivityComboBox->setObjectName(QStringLiteral("sensitivityComboBox"));
    m_sensitivityComboBox->addItems({tr("低敏感"), tr("中敏感"), tr("高敏感")});
    sensitivityRow->addWidget(rowLabel(tr("敏感度")));
    sensitivityRow->addWidget(m_sensitivityComboBox, 1);
    advancedLayout->addLayout(sensitivityRow);

    QHBoxLayout *brightnessRow = new QHBoxLayout;
    m_brightnessCheckBox = new QCheckBox(tr("亮度参与"));
    m_brightnessCheckBox->setObjectName(QStringLiteral("brightnessEnabledCheckBox"));
    brightnessRow->addWidget(rowLabel(tr("亮度")));
    brightnessRow->addWidget(m_brightnessCheckBox, 1);
    advancedLayout->addLayout(brightnessRow);

    QHBoxLayout *hsvStateRow = new QHBoxLayout;
    m_hsvModelStateLabel = new QLabel(tr("未采样"));
    m_hsvModelStateLabel->setObjectName(QStringLiteral("hsvModelStateLabel"));
    hsvStateRow->addWidget(rowLabel(tr("HSV 特征")));
    hsvStateRow->addWidget(m_hsvModelStateLabel, 1);
    advancedLayout->addLayout(hsvStateRow);

    m_rebuildHsvButton = new QPushButton(tr("重新提取 HSV 特征"));
    m_rebuildHsvButton->setObjectName(QStringLiteral("rebuildHsvFeaturesButton"));
    m_rebuildHsvButton->setProperty("actionRole", QStringLiteral("secondary"));
    advancedLayout->addWidget(m_rebuildHsvButton);
    m_hsvBuildFeedbackLabel = new QLabel(tr("尚未采样"));
    m_hsvBuildFeedbackLabel->setObjectName(QStringLiteral("hsvBuildFeedbackLabel"));
    m_hsvBuildFeedbackLabel->setWordWrap(true);
    m_hsvBuildFeedbackLabel->setProperty("role", QStringLiteral("buildFeedback"));
    UiStyleRoles::applyStatusTone(m_hsvBuildFeedbackLabel, QStringLiteral("neutral"));
    advancedLayout->addWidget(m_hsvBuildFeedbackLabel);

    QHBoxLayout *gmmStateRow = new QHBoxLayout;
    m_gmmModelStateLabel = new QLabel(tr("未建立"));
    m_gmmModelStateLabel->setObjectName(QStringLiteral("gmmModelStateLabel"));
    gmmStateRow->addWidget(rowLabel(tr("GMM 模型")));
    gmmStateRow->addWidget(m_gmmModelStateLabel, 1);
    advancedLayout->addLayout(gmmStateRow);

    m_gmmDiagnosticsLabel = new QLabel;
    m_gmmDiagnosticsLabel->setObjectName(QStringLiteral("gmmDiagnosticsLabel"));
    m_gmmDiagnosticsLabel->setWordWrap(true);
    advancedLayout->addWidget(m_gmmDiagnosticsLabel);

    m_buildGmmButton = new QPushButton(tr("建立 GMM 模型"));
    m_buildGmmButton->setObjectName(QStringLiteral("buildGmmModelButton"));
    m_buildGmmButton->setProperty("actionRole", QStringLiteral("secondary"));
    advancedLayout->addWidget(m_buildGmmButton);
    m_gmmBuildFeedbackLabel = new QLabel(tr("尚未建立模型"));
    m_gmmBuildFeedbackLabel->setObjectName(QStringLiteral("gmmBuildFeedbackLabel"));
    m_gmmBuildFeedbackLabel->setWordWrap(true);
    m_gmmBuildFeedbackLabel->setProperty("role", QStringLiteral("buildFeedback"));
    UiStyleRoles::applyStatusTone(m_gmmBuildFeedbackLabel, QStringLiteral("neutral"));
    advancedLayout->addWidget(m_gmmBuildFeedbackLabel);

    leftLayout->addWidget(advancedCard);

    leftLayout->addStretch(1);
    QHBoxLayout *bottomButtons = new QHBoxLayout;
    m_cancelButton = new QPushButton(tr("取消"));
    m_saveButton = new QPushButton(tr("完成"));
    m_saveButton->setObjectName(QStringLiteral("saveTemplateButton"));
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
    QLabel *viewerCursorLabel = new QLabel;
    viewerCursorLabel->setObjectName(QStringLiteral("viewerCursorLabel"));
    viewerCursorLabel->setMinimumHeight(28);
    viewerCursorLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    previewLayout->addWidget(m_viewerTitleLabel);
    previewLayout->addWidget(m_previewGraphicsView, 1);
    previewLayout->addWidget(m_statusLabel);
    previewLayout->addWidget(viewerCursorLabel);
    content->addWidget(previewPanel, 1);

    m_previewHelper = new FrameViewHelper(m_previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(viewerCursorLabel);
    m_previewHelper->setNavigationEnabled(true);
    m_previewGraphicsView->setBackgroundBrush(QBrush(QColor(0, 0, 0)));
}
#endif

void ColorTemplateDialog::buildUi()
{
    ui->setupUi(this);
    setWindowModality(Qt::WindowModal);

    m_templateNameLineEdit = ui->templateNameLineEdit;
    m_recognitionBackendComboBox = ui->recognitionBackendComboBox;
    m_featureTypeRowWidget = ui->featureTypeRowWidget;
    m_featureTypeComboBox = ui->featureTypeComboBox;
    m_labelListWidget = ui->labelListWidget;
    m_addLabelButton = ui->addLabelButton;
    m_renameLabelButton = ui->renameLabelButton;
    m_deleteLabelButton = ui->deleteLabelButton;
    m_roiSampleListWidget = ui->roiSampleListWidget;
    m_sampleCountLabel = ui->sampleCountLabel;
    m_sensitivityComboBox = ui->sensitivityComboBox;
    m_brightnessCheckBox = ui->brightnessEnabledCheckBox;
    m_gmmModelStateLabel = ui->gmmModelStateLabel;
    m_gmmDiagnosticsLabel = ui->gmmDiagnosticsLabel;
    m_buildGmmButton = ui->buildGmmModelButton;
    m_gmmBuildFeedbackLabel = ui->gmmBuildFeedbackLabel;
    m_hsvModelStateLabel = ui->hsvModelStateLabel;
    m_rebuildHsvButton = ui->rebuildHsvFeaturesButton;
    m_hsvBuildFeedbackLabel = ui->hsvBuildFeedbackLabel;
    m_regionRectButton = ui->sampleRectRoiButton;
    m_addCurrentImageButton = ui->addCurrentSampleImageButton;
    m_addImageButton = ui->addExternalSampleImageButton;
    m_deleteCurrentRoiButton = ui->deleteCurrentRoiSampleButton;
    m_addSampleButton = ui->addSampleButton;
    m_cancelButton = ui->cancelButton;
    m_saveButton = ui->saveTemplateButton;
    m_viewerTitleLabel = ui->viewerTitleLabel;
    m_previewGraphicsView = ui->previewGraphicsView;
    m_statusLabel = ui->statusLabel;

    ui->headerFrame->setProperty("colorTemplateDragHandle", true);
    ui->headerTitleLabel->setProperty("colorTemplateDragHandle", true);
    ui->headerFrame->installEventFilter(this);
    ui->headerTitleLabel->installEventFilter(this);
    connect(ui->closeButton, &QToolButton::clicked, this, &ColorTemplateDialog::reject);

    const auto bindCollapse = [](QToolButton *button, QWidget *content) {
        QObject::connect(button, &QToolButton::clicked, content,
                         [button, content](bool collapsed) {
            content->setVisible(!collapsed);
            button->setText(collapsed ? QStringLiteral("›") : QStringLiteral("⌄"));
        });
    };
    bindCollapse(ui->modelCollapseButton, ui->modelContentWidget);
    bindCollapse(ui->sampleCollapseButton, ui->sampleContentWidget);
    bindCollapse(ui->advancedCollapseButton, ui->advancedContentWidget);

    m_previewHelper = new FrameViewHelper(m_previewGraphicsView, this);
    m_previewHelper->bindPixelStatusLabel(ui->viewerCursorLabel);
    m_previewHelper->setNavigationEnabled(true);
    m_previewGraphicsView->setBackgroundBrush(QBrush(QColor(0, 0, 0)));
    UiStyleRoles::applyStatusTone(m_hsvBuildFeedbackLabel, QStringLiteral("neutral"));
    UiStyleRoles::applyStatusTone(m_gmmBuildFeedbackLabel, QStringLiteral("neutral"));
}

void ColorTemplateDialog::setupUiState()
{
    m_recognitionBackendComboBox->setCurrentIndex(0);
    m_brightnessCheckBox->setChecked(false);
    m_featureTypeComboBox->setCurrentIndex(0);
    m_sensitivityComboBox->setCurrentIndex(1);
    m_regionRectButton->setChecked(false);
    if (QStandardItemModel *model = qobject_cast<QStandardItemModel *>(m_featureTypeComboBox->model())) {
        if (QStandardItem *item = model->item(2))
            item->setEnabled(false);
    }
    updateAlgorithmUi();
    ensureDefaultLabel();
    updateLabelList();
    updateRoiSampleList();
    updateSampleCount();
}

void ColorTemplateDialog::connectControls()
{
    connect(m_cancelButton, &QPushButton::clicked, this, &ColorTemplateDialog::reject);
    connect(m_saveButton, &QPushButton::clicked, this, &ColorTemplateDialog::finishTemplate);
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
        const int sampleIndex = currentSampleIndex();
        if (sampleIndex < 0)
            return;
        QString restoreError;
        if (!restoreSampleContext(sampleIndex, &restoreError))
            setStatusText(restoreError, restoreError);
    });
    connect(m_regionRectButton, &QToolButton::clicked, this, &ColorTemplateDialog::startRectangleRoiEditing);
    connect(m_featureTypeComboBox,
            QOverload<int>::of(&QComboBox::activated),
            this,
            [this](int) {
        updateAlgorithmUi();
        markHsvModelStale();
    });
    connect(m_sensitivityComboBox,
            QOverload<int>::of(&QComboBox::activated),
            this,
            [this](int) { markHsvModelStale(); });
    connect(m_recognitionBackendComboBox,
            QOverload<int>::of(&QComboBox::activated),
            this,
            [this](int) {
        m_template.recognitionBackend = recognitionBackendFromUi(
                    m_recognitionBackendComboBox->currentText());
        const QSignalBlocker blocker(m_brightnessCheckBox);
        m_brightnessCheckBox->setChecked(m_template.recognitionBackend == QStringLiteral("cielab_gmm")
                                         ? m_template.gmmColorChannels == QStringLiteral("lab")
                                         : m_template.brightnessEnabled);
        updateAlgorithmUi();
    });
    connect(m_brightnessCheckBox, &QCheckBox::clicked, this, [this](bool) {
        if (recognitionBackendFromUi(m_recognitionBackendComboBox->currentText()) ==
                QStringLiteral("cielab_gmm")) {
            m_template.gmmColorChannels = m_brightnessCheckBox->isChecked()
                    ? QStringLiteral("lab") : QStringLiteral("ab");
            markGmmModelStale();
        } else {
            m_template.brightnessEnabled = m_brightnessCheckBox->isChecked();
            markHsvModelStale();
        }
    });
    connect(m_buildGmmButton, &QPushButton::clicked,
            this, &ColorTemplateDialog::buildGmmModel);
    connect(m_rebuildHsvButton, &QPushButton::clicked,
            this, &ColorTemplateDialog::rebuildHsvFeatures);

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

void ColorTemplateDialog::updateAlgorithmUi()
{
    const bool gmm = recognitionBackendFromUi(m_recognitionBackendComboBox->currentText()) ==
            QStringLiteral("cielab_gmm");
    m_featureTypeRowWidget->setVisible(!gmm);
    m_featureTypeComboBox->setEnabled(!gmm);
    m_sensitivityComboBox->setEnabled(!gmm);
    if (gmm) {
        m_brightnessCheckBox->setEnabled(true);
        m_brightnessCheckBox->setText(tr("亮度参与（CIELAB L）"));
        m_brightnessCheckBox->setToolTip(tr("关闭使用 a/b；开启使用 L/a/b。切换后需重新建立 GMM。"));
    } else {
        const bool supported = featureTypeFromUi(m_featureTypeComboBox->currentText()) ==
                QStringLiteral("histogram");
        if (!supported)
            m_brightnessCheckBox->setChecked(false);
        m_brightnessCheckBox->setEnabled(supported);
        m_brightnessCheckBox->setText(supported
                                      ? tr("亮度参与（HSV V）")
                                      : tr("二维 H/S 固定不使用亮度"));
        m_brightnessCheckBox->setToolTip(supported
                                         ? tr("启用后使用一维 H/S/V 特征；参数变化后需重新采样。")
                                         : tr("二维联合直方图只使用 H/S，亮度参数不进入特征签名。"));
    }
    m_gmmModelStateLabel->setVisible(gmm);
    m_gmmDiagnosticsLabel->setVisible(gmm);
    m_buildGmmButton->setVisible(gmm);
    m_gmmBuildFeedbackLabel->setVisible(gmm);
    m_hsvModelStateLabel->setVisible(!gmm);
    m_rebuildHsvButton->setVisible(!gmm);
    m_hsvBuildFeedbackLabel->setVisible(!gmm);
    updateHsvModelUi();
    updateGmmModelUi();
}

void ColorTemplateDialog::markHsvModelStale()
{
    if (m_template.samples.isEmpty())
        return;

    for (ColorRecognitionSampleData &sample : m_template.samples)
        sample.featureSignature.clear();
    m_template.modelState = QStringLiteral("stale");
    updateHsvModelUi();
    setStatusText(tr("HSV 特征参数已变化，HSV 特征已失效，请重新提取"));
}

void ColorTemplateDialog::markGmmModelStale()
{
    if (m_template.gmmModel.state == QStringLiteral("empty")) {
        updateGmmModelUi();
        return;
    }
    m_template.gmmModel.state = QStringLiteral("stale");
    m_template.gmmModel.status = QStringLiteral("gmm_model_stale");
    m_template.gmmModel.message = tr("GMM 参数或公共样本已变化，请重新建立模型。");
    updateGmmModelUi();
    setStatusText(m_template.gmmModel.message);
}

void ColorTemplateDialog::markCommonModelsStale()
{
    markHsvModelStale();
    markGmmModelStale();
}

void ColorTemplateDialog::updateHsvModelUi()
{
    if (!m_hsvModelStateLabel || !m_rebuildHsvButton || !m_hsvBuildFeedbackLabel)
        return;

    const QString state = m_hsvRebuildInProgress
            ? QStringLiteral("building") : hsvModelState(m_template);
    QString stateText;
    QString feedbackText;
    QString tone = QStringLiteral("neutral");
    if (state == QStringLiteral("building")) {
        stateText = tr("提取中");
        feedbackText = tr("正在从保存的无损 ROI 重新提取 HSV 特征…");
        tone = QStringLiteral("building");
    } else if (state == QStringLiteral("ready")) {
        stateText = tr("可用");
        feedbackText = tr("HSV 特征已就绪，可以用于检测");
        tone = QStringLiteral("success");
    } else if (state == QStringLiteral("stale")) {
        stateText = tr("已失效，需重新提取");
        feedbackText = tr("部分样本缺少特征或严格签名不一致，请重新提取 HSV 特征");
        tone = QStringLiteral("stale");
    } else {
        stateText = tr("未采样");
        feedbackText = tr("尚无公共 ROI 样本");
    }
    m_template.modelState = state == QStringLiteral("building")
            ? hsvModelState(m_template) : state;
    m_hsvModelStateLabel->setText(stateText);
    m_hsvBuildFeedbackLabel->setText(feedbackText);
    m_hsvBuildFeedbackLabel->setToolTip(feedbackText);
    UiStyleRoles::applyStatusTone(m_hsvBuildFeedbackLabel, tone);
    m_rebuildHsvButton->setText(state == QStringLiteral("building")
                                ? tr("提取中…") : tr("重新提取 HSV 特征"));
    m_rebuildHsvButton->setEnabled(!m_hsvRebuildInProgress && !m_template.samples.isEmpty());
}

void ColorTemplateDialog::updateGmmModelUi()
{
    if (!m_gmmModelStateLabel || !m_gmmDiagnosticsLabel || !m_buildGmmButton ||
        !m_gmmBuildFeedbackLabel)
        return;

    const QString state = m_gmmBuildInProgress
            ? QStringLiteral("building") : m_template.gmmModel.state.trimmed().toLower();
    QString stateText;
    if (state == QStringLiteral("building")) stateText = tr("建模中");
    else if (state == QStringLiteral("ready")) stateText = tr("可用");
    else if (state == QStringLiteral("ready_with_warning")) stateText = tr("可用，但样本偏少");
    else if (state == QStringLiteral("stale")) stateText = tr("已失效，需重新建立");
    else if (state == QStringLiteral("invalid")) stateText = tr("模型校验失败");
    else if (state == QStringLiteral("failed")) stateText = tr("建模失败");
    else stateText = tr("未建立");
    m_gmmModelStateLabel->setText(stateText);
    m_gmmModelStateLabel->setToolTip(m_template.gmmModel.message);

    QStringList diagnostics;
    for (const ColorRecognitionGmmClassDiagnostics &item : m_template.gmmModel.classes) {
        diagnostics.append(tr("%1：%2 ROI，采样 %3/%4 像素，中心 %5-%6")
                           .arg(item.label)
                           .arg(item.roiCount)
                           .arg(item.trainingPixels)
                           .arg(item.requestedTrainingPixels)
                           .arg(item.minCenters)
                           .arg(item.maxCenters));
    }
    if (diagnostics.isEmpty() && !m_template.gmmModel.message.trimmed().isEmpty())
        diagnostics.append(m_template.gmmModel.message);
    m_gmmDiagnosticsLabel->setText(diagnostics.join(QLatin1Char('\n')));
    QString feedbackText;
    QString feedbackTone = QStringLiteral("neutral");
    if (state == QStringLiteral("building")) {
        feedbackText = tr("正在建立 GMM 模型，请稍候…");
        feedbackTone = QStringLiteral("building");
    } else if (state == QStringLiteral("ready")) {
        feedbackText = tr("GMM 模型已建立，可以用于检测");
        feedbackTone = QStringLiteral("success");
    } else if (state == QStringLiteral("ready_with_warning")) {
        feedbackText = tr("GMM 模型已建立，但部分类别独立 ROI 少于 3 个");
        feedbackTone = QStringLiteral("warning");
    } else if (state == QStringLiteral("stale")) {
        feedbackText = m_template.gmmModel.message.isEmpty()
                ? tr("GMM 模型已失效，请重新建立") : m_template.gmmModel.message;
        feedbackTone = QStringLiteral("stale");
    } else if (state == QStringLiteral("failed") || state == QStringLiteral("invalid")) {
        feedbackText = m_template.gmmModel.message.isEmpty()
                ? tr("GMM 建模失败") : m_template.gmmModel.message;
        feedbackTone = QStringLiteral("error");
    } else {
        feedbackText = tr("尚未建立 GMM 模型");
    }
    m_gmmBuildFeedbackLabel->setText(feedbackText);
    m_gmmBuildFeedbackLabel->setToolTip(feedbackText);
    UiStyleRoles::applyStatusTone(m_gmmBuildFeedbackLabel, feedbackTone);
    m_buildGmmButton->setText(state == QStringLiteral("building")
                              ? tr("建模中…")
                              : (state == QStringLiteral("empty")
                                 ? tr("建立 GMM 模型") : tr("重新建立 GMM 模型")));
    m_buildGmmButton->setEnabled(!m_gmmBuildInProgress && !m_template.samples.isEmpty());
}

void ColorTemplateDialog::setHsvRebuildUiBusy(bool busy)
{
    m_hsvRebuildInProgress = busy;
    const bool enabled = !busy;
    m_saveButton->setEnabled(enabled);
    m_addSampleButton->setEnabled(enabled);
    m_addCurrentImageButton->setEnabled(enabled);
    m_addImageButton->setEnabled(enabled);
    m_addLabelButton->setEnabled(enabled);
    m_renameLabelButton->setEnabled(enabled);
    m_deleteLabelButton->setEnabled(enabled);
    m_deleteCurrentRoiButton->setEnabled(enabled && currentSampleIndex() >= 0);
    m_regionRectButton->setEnabled(enabled);
    m_labelListWidget->setEnabled(enabled);
    m_roiSampleListWidget->setEnabled(enabled);
    m_recognitionBackendComboBox->setEnabled(enabled);
    m_featureTypeComboBox->setEnabled(enabled);
    m_sensitivityComboBox->setEnabled(enabled);
    m_brightnessCheckBox->setEnabled(enabled);
    m_buildGmmButton->setEnabled(enabled && !m_template.samples.isEmpty());
    if (busy)
        updateHsvModelUi();
    else
        updateAlgorithmUi();
}

void ColorTemplateDialog::setGmmBuildUiBusy(bool busy)
{
    m_gmmBuildInProgress = busy;
    const bool enabled = !busy;
    m_saveButton->setEnabled(enabled);
    m_addSampleButton->setEnabled(enabled);
    m_addCurrentImageButton->setEnabled(enabled);
    m_addImageButton->setEnabled(enabled);
    m_addLabelButton->setEnabled(enabled);
    m_renameLabelButton->setEnabled(enabled);
    m_deleteLabelButton->setEnabled(enabled);
    m_deleteCurrentRoiButton->setEnabled(enabled && currentSampleIndex() >= 0);
    m_regionRectButton->setEnabled(enabled);
    m_labelListWidget->setEnabled(enabled);
    m_roiSampleListWidget->setEnabled(enabled);
    m_recognitionBackendComboBox->setEnabled(enabled);
    m_featureTypeComboBox->setEnabled(enabled &&
            recognitionBackendFromUi(m_recognitionBackendComboBox->currentText()) !=
            QStringLiteral("cielab_gmm"));
    m_sensitivityComboBox->setEnabled(enabled &&
            recognitionBackendFromUi(m_recognitionBackendComboBox->currentText()) !=
            QStringLiteral("cielab_gmm"));
    m_brightnessCheckBox->setEnabled(enabled);
    m_rebuildHsvButton->setEnabled(enabled && !m_template.samples.isEmpty());
    if (busy)
        updateGmmModelUi();
    else
        updateAlgorithmUi();
}

void ColorTemplateDialog::showGmmBuildFailure(const QString &status, const QString &message)
{
    m_template.gmmModel.state = QStringLiteral("failed");
    m_template.gmmModel.status = status;
    m_template.gmmModel.message = message;
    setGmmBuildUiBusy(false);
    setStatusText(message, message);
    QMessageBox::warning(this, tr("GMM 建模失败"), message);
}

void ColorTemplateDialog::finishTemplate()
{
    const QString backend = recognitionBackendFromUi(
                m_recognitionBackendComboBox->currentText());
    if (backend == QStringLiteral("cielab_gmm")) {
        const QString state = m_template.gmmModel.state.trimmed().toLower();
        if (state != QStringLiteral("ready") && state != QStringLiteral("ready_with_warning")) {
            const QMessageBox::StandardButton answer = QMessageBox::question(
                        this,
                        tr("颜色模板"),
                        tr("当前 GMM 模型状态为“%1”，保存后暂不能用于检测。仍要保存吗？")
                        .arg(m_gmmModelStateLabel->text()),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
        }
    } else if (hsvModelState(m_template) != QStringLiteral("ready")) {
        const QMessageBox::StandardButton answer = QMessageBox::question(
                    this,
                    tr("颜色模板"),
                    tr("当前 HSV 特征状态为“%1”，保存后暂不能用于检测。仍要保存吗？")
                    .arg(m_hsvModelStateLabel->text()),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }
    accept();
}

void ColorTemplateDialog::rebuildHsvFeatures()
{
    if (m_hsvRebuildInProgress)
        return;
    if (m_template.samples.isEmpty()) {
        setStatusText(tr("尚无公共 ROI 样本，无法提取 HSV 特征"));
        return;
    }

    QVector<ColorRecognitionSampleData> rebuilt = m_template.samples;
    QString algorithmVersion;
    QString schemaVersion;
    QString commonSignature;
    setHsvRebuildUiBusy(true);
    setStatusText(tr("正在重新提取 HSV 特征，请稍候…"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    for (ColorRecognitionSampleData &sample : rebuilt) {
        const QByteArray png = QByteArray::fromBase64(
                    sample.gmmRoiImagePngBase64.toLatin1());
        const std::vector<uchar> bytes(png.cbegin(), png.cend());
        const cv::Mat image = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
        QString failure;
        if (sample.gmmRoiImagePngBase64.trimmed().isEmpty()) {
            failure = tr("保存的无损 ROI 缺失");
        } else if (image.empty()) {
            failure = tr("保存的无损 ROI 无法解码");
        } else if ((sample.gmmImageWidth > 0 && sample.gmmImageWidth != image.cols) ||
                   (sample.gmmImageHeight > 0 && sample.gmmImageHeight != image.rows)) {
            failure = tr("保存的 ROI 尺寸不一致");
        } else if (!sample.gmmImageSha256.trimmed().isEmpty() &&
                   sample.gmmImageSha256 != canonicalImageHash(image)) {
            failure = tr("保存的 ROI 哈希校验失败");
        }

        FrameInputMetadata metadata = FrameInputMetadata::fromMat(image, QStringLiteral("template_roi"));
        if (!sample.pixelFormat.trimmed().isEmpty())
            metadata.pixelFormat = sample.pixelFormat;
        if (sample.validBits > 0)
            metadata.validBits = sample.validBits;
        if (sample.bitShift >= 0)
            metadata.bitShift = sample.bitShift;
        if (failure.isEmpty() && gmmPixelFormat(image, metadata) !=
                sample.pixelFormat.trimmed().toUpper()) {
            failure = tr("保存的 pixelFormat 与 ROI 位深或通道数不一致");
        }

        ColorRecognitionHalconFeatureResult result;
        if (failure.isEmpty()) {
            ColorRecognitionHalconConfig config = featureExtractionConfig(metadata);
            config.roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
            result = m_featureRunner.extractFeature(image, config);
            if (!result.success)
                failure = result.message;
        }
        if (!failure.isEmpty()) {
            setHsvRebuildUiBusy(false);
            const QString message = tr("样本 %1（%2）HSV 特征重建失败：%3")
                    .arg(sample.sampleId, sample.label, failure);
            m_template.modelState = QStringLiteral("stale");
            updateHsvModelUi();
            setStatusText(message, message);
            QMessageBox::warning(this, tr("HSV 特征提取失败"), message);
            return;
        }

        const QString signature = result.payload
                .value(QStringLiteral("featureSignature")).toString();
        if (commonSignature.isEmpty())
            commonSignature = signature;
        else if (signature != commonSignature) {
            setHsvRebuildUiBusy(false);
            const QString message = tr("样本输入格式不一致，无法建立统一的 HSV 特征签名。请统一样本位深和像素格式。");
            m_template.modelState = QStringLiteral("stale");
            updateHsvModelUi();
            setStatusText(message, message);
            QMessageBox::warning(this, tr("HSV 特征提取失败"), message);
            return;
        }
        sample.feature = result.feature;
        sample.featureSignature = signature;
        algorithmVersion = result.payload.value(QStringLiteral("algorithmVersion")).toString();
        schemaVersion = result.payload.value(QStringLiteral("featureSchemaVersion")).toString();
    }

    m_template.samples = rebuilt;
    m_template.algorithmVersion = algorithmVersion;
    m_template.featureSchemaVersion = schemaVersion;
    m_template.hsvClassifierVersion = colorRecognitionHsvCurrentClassifierVersion();
    m_template.hsvClassifierParamsHash = colorRecognitionHsvClassifierParamsHash(
                m_template.hsvClassifierVersion);
    m_template.modelState = QStringLiteral("ready");
    setHsvRebuildUiBusy(false);
    updateRoiSampleList();
    updateSampleCount();
    const QString completion = tr("HSV 特征提取完成，共处理 %1 个 ROI 样本").arg(rebuilt.size());
    setStatusText(completion, completion);
    QMessageBox::information(this, tr("HSV 特征提取完成"), completion);
}

void ColorTemplateDialog::buildGmmModel()
{
    if (m_gmmBuildInProgress)
        return;
    if (m_template.labels.size() < 2) {
        showGmmBuildFailure(QStringLiteral("invalid_gmm_labels"),
                            tr("GMM 至少需要两个颜色类别。"));
        return;
    }

    QVector<ColorRecognitionGmmBuildSample> samples;
    samples.reserve(m_template.samples.size());
    QSet<QString> sampleIds;
    for (const ColorRecognitionSampleData &stored : m_template.samples) {
        if (stored.sampleId.trimmed().isEmpty() || sampleIds.contains(stored.sampleId)) {
            showGmmBuildFailure(QStringLiteral("gmm_sample_image_invalid"),
                                tr("样本 ID 缺失或重复，请重新采样。"));
            return;
        }
        sampleIds.insert(stored.sampleId);
        if (stored.gmmRoiImagePngBase64.trimmed().isEmpty()) {
            showGmmBuildFailure(QStringLiteral("gmm_raw_sample_missing"),
                                tr("旧样本缺少无损 GMM 原始 ROI，请重新采样。"));
            return;
        }
        const QByteArray png = QByteArray::fromBase64(stored.gmmRoiImagePngBase64.toLatin1());
        const std::vector<uchar> bytes(png.cbegin(), png.cend());
        const cv::Mat decoded = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
        if (decoded.empty()) {
            showGmmBuildFailure(QStringLiteral("gmm_sample_image_invalid"),
                                tr("样本原始 ROI PNG 无法解码。"));
            return;
        }
        ColorRecognitionGmmBuildSample sample;
        sample.sampleId = stored.sampleId;
        sample.classId = stored.classId;
        sample.label = stored.label;
        sample.image = decoded;
        sample.roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
        sample.pixelFormat = stored.pixelFormat;
        sample.validBits = stored.validBits;
        sample.bitShift = stored.bitShift;
        sample.imageSha256 = stored.gmmImageSha256;
        samples.append(sample);
    }

    ColorRecognitionGmmBuildConfig config;
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(
                QString(), &config.halconSoPathCandidates);
    config.colorChannels = m_brightnessCheckBox->isChecked()
            ? QStringLiteral("lab") : QStringLiteral("ab");
    config.maxSamplesPerClass = m_template.gmmMaxSamplesPerClass;
    for (const ColorRecognitionLabelData &stored : m_template.labels)
        config.labels.append(ColorRecognitionGmmLabel{stored.name, stored.classId});

    setGmmBuildUiBusy(true);
    setStatusText(tr("正在建立 GMM 模型，请稍候…"));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const ColorRecognitionGmmBuildResult result = m_featureRunner.buildGmmTemplateModel(samples, config);

    m_template.gmmColorChannels = config.colorChannels;
    if (!result.success) {
        showGmmBuildFailure(result.status, result.message);
        return;
    }

    m_template.gmmModel.status = result.status;
    m_template.gmmModel.message = result.message;
    m_template.gmmModel.state = result.state == QStringLiteral("ReadyWithWarning")
            ? QStringLiteral("ready_with_warning") : QStringLiteral("ready");
    m_template.gmmModel.algorithmVersion = result.algorithmVersion;
    m_template.gmmModel.featureSchemaVersion = result.featureSchemaVersion;
    m_template.gmmModel.samplingAlgorithmVersion = result.payload
            .value(QStringLiteral("samplingAlgorithmVersion")).toString();
    m_template.gmmModel.colorChannels = result.colorChannels;
    m_template.gmmModel.trainingDataHash = result.trainingDataHash;
    m_template.gmmModel.buildParamsHash = result.buildParamsHash;
    m_template.gmmModel.serializedGmmBase64 = result.artifact.serializedGmmBase64;
    m_template.gmmModel.serializedSize = result.artifact.serializedSize;
    m_template.gmmModel.serializedSha256 = result.artifact.serializedSha256;
    m_template.gmmModel.classIdOrder = result.classIdOrder;
    m_template.gmmModel.classes = result.classes;
    m_template.gmmModel.builtAtUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    setGmmBuildUiBusy(false);
    const bool warning = result.state == QStringLiteral("ReadyWithWarning");
    const QString completionText = warning
            ? tr("GMM 建模完成，但部分类别 ROI 少于 3 个")
            : tr("GMM 建模完成");
    setStatusText(completionText, completionText);
    QMessageBox::information(
                this,
                tr("GMM 建模完成"),
                tr("%1\n类别数：%2\n颜色通道：%3\n耗时：%4 ms")
                .arg(completionText)
                .arg(result.classIdOrder.size())
                .arg(result.colorChannels == QStringLiteral("lab")
                     ? QStringLiteral("L/a/b") : QStringLiteral("a/b"))
                .arg(result.elapsedMs));
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
    markCommonModelsStale();
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
    markGmmModelStale();
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
    markCommonModelsStale();
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

    cv::Mat frame = m_sampleImage;
    FrameInputMetadata metadata = m_sampleImageMetadata;
    if (frame.empty()) {
        const ReferenceFrameSnapshot reference =
                ReferenceImageProvider::instance().referenceFrameSnapshot();
        frame = reference.frame;
        metadata = reference.metadata;
    }
    if (frame.empty()) {
        const CameraFrameSnapshot camera =
                CameraFrameProvider::instance().currentFrameSnapshot();
        frame = camera.frame;
        metadata = camera.metadata;
    }
    if (frame.empty()) {
        setStatusText(tr("当前无基准图或相机图像，无法添加样本"));
        return;
    }

    m_addSampleButton->setEnabled(false);
    const ColorRecognitionHalconFeatureResult featureResult = [&]() {
        try {
            return m_featureRunner.extractFeature(frame, featureExtractionConfig(metadata));
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
    sample.sampleId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    sample.label = labelName;
    sample.classId = classId;
    sample.feature = featureResult.feature;
    sample.featureSignature = featureResult.payload
            .value(QStringLiteral("featureSignature")).toString();
    m_template.modelSchemaVersion = 3;
    m_template.algorithmVersion = featureResult.payload
            .value(QStringLiteral("algorithmVersion")).toString();
    m_template.featureSchemaVersion = featureResult.payload
            .value(QStringLiteral("featureSchemaVersion")).toString();
    sample.roiNormalized = effectiveRoiNormalized();
    const cv::Mat gmmRoi = cropGmmRoiMat(frame, sample.roiNormalized);
    if (gmmRoi.empty()) {
        setStatusText(tr("样本 ROI 无法从原始图像裁剪"));
        QMessageBox::warning(this, tr("颜色模板"), tr("样本 ROI 无效，无法保存 GMM 原始训练图。"));
        return;
    }
    std::vector<uchar> encodedGmm;
    if (!cv::imencode(".png", gmmRoi, encodedGmm)) {
        setStatusText(tr("GMM 原始 ROI 无损编码失败"));
        QMessageBox::warning(this, tr("颜色模板"), tr("无法无损保存 GMM 原始 ROI。"));
        return;
    }
    sample.gmmRoiImagePngBase64 = QString::fromLatin1(
                QByteArray(reinterpret_cast<const char *>(encodedGmm.data()),
                           static_cast<int>(encodedGmm.size())).toBase64());
    sample.gmmImageSha256 = canonicalImageHash(gmmRoi);
    sample.gmmImageWidth = gmmRoi.cols;
    sample.gmmImageHeight = gmmRoi.rows;
    sample.pixelFormat = gmmPixelFormat(gmmRoi, metadata);
    sample.validBits = gmmRoi.depth() == CV_8U ? 8 : metadata.validBits;
    sample.bitShift = gmmRoi.depth() == CV_8U ? 0 : metadata.bitShift;
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
    markGmmModelStale();
    m_template.modelState = hsvModelState(m_template);
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
    const ReferenceFrameSnapshot reference =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    cv::Mat frame = reference.frame;
    FrameInputMetadata metadata = reference.metadata;
    QImage displayImage = ReferenceImageProvider::instance().referenceImage();
    QString title = tr("基准图");
    if (frame.empty()) {
        const CameraFrameSnapshot camera =
                CameraFrameProvider::instance().currentFrameSnapshot();
        frame = camera.frame;
        metadata = camera.metadata;
        displayImage = CameraFrameProvider::instance().currentImage();
        title = tr("当前图像");
    }

    if (frame.empty() || displayImage.isNull()) {
        setStatusText(tr("当前无基准图或相机图像，无法添加当前图像"));
        return;
    }

    m_sampleImage = frame.clone();
    m_sampleImageMetadata = metadata;
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

    cv::Mat frame = cv::imread(fileName.toLocal8Bit().constData(), cv::IMREAD_UNCHANGED);
    if (frame.empty()) {
        QMessageBox::warning(this, tr("添加图片"), tr("无法读取所选图片"));
        return;
    }

    m_sampleImage = frame.clone();
    m_sampleImageMetadata = FrameInputMetadata::fromMat(m_sampleImage, QStringLiteral("file"));
    if (m_sampleImage.depth() == CV_16U) {
        m_sampleImageMetadata.pixelFormat = m_sampleImage.channels() == 4
                ? QStringLiteral("BGRA16") : QStringLiteral("BGR16");
        m_sampleImageMetadata.validBits = 16;
        m_sampleImageMetadata.bitShift = 0;
    }
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
    markCommonModelsStale();
    updateLabelList();
    updateRoiSampleList();
    updateSampleCount();
    setStatusText(tr("已删除当前 ROI 样本"));
}

void ColorTemplateDialog::startRectangleRoiEditing()
{
    if (m_editState == EditState::SampleRect) {
        setEditState(EditState::None);
        setStatusText(tr("已退出样本 ROI 绘制，可拖动或缩放查看图像"));
        return;
    }

    showPreviewImage();

    if (!m_previewHelper || !m_previewHelper->hasImage()) {
        setEditState(EditState::None);
        return;
    }

    m_previewHelper->setRoiRectNormalized(effectiveRoiNormalized());
    setEditState(EditState::SampleRect);
    setStatusText(tr("请框选颜色样本矩形 ROI"));
}

void ColorTemplateDialog::setEditState(EditState state)
{
    m_editState = state;
    if (m_previewHelper)
        m_previewHelper->setRoiDrawingEnabled(state == EditState::SampleRect);
    if (m_regionRectButton) {
        const QSignalBlocker blocker(m_regionRectButton);
        m_regionRectButton->setChecked(state == EditState::SampleRect);
    }
}

void ColorTemplateDialog::showUnsupportedRegionMessage()
{
    startRectangleRoiEditing();
}

void ColorTemplateDialog::handleRoiChanged(const QRectF &roi)
{
    m_sampleRoiNormalized = normalizedRoiOrDefault(roi);
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
        setEditState(EditState::None);
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
        if (rowToSelect < 0)
            rowToSelect = 0;
        if (i == currentSample)
            rowToSelect = m_roiSampleListWidget->count() - 1;
    }

    if (rowToSelect >= 0)
        m_roiSampleListWidget->setCurrentRow(rowToSelect);
    m_deleteCurrentRoiButton->setEnabled(currentSampleIndex() >= 0);
    if (currentSampleIndex() >= 0) {
        QString restoreError;
        if (!restoreSampleContext(currentSampleIndex(), &restoreError))
            setStatusText(restoreError, restoreError);
    }
}

bool ColorTemplateDialog::restoreSampleContext(int sampleIndex, QString *errorMessage)
{
    if (sampleIndex < 0 || sampleIndex >= m_template.samples.size()) {
        if (errorMessage)
            *errorMessage = tr("请选择一个 ROI 样本");
        return false;
    }

    const ColorRecognitionSampleData &sample = m_template.samples.at(sampleIndex);
    if (sample.gmmRoiImagePngBase64.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = tr("样本 %1 缺少保存的无损 ROI，请重新采样").arg(sample.sampleId);
        return false;
    }
    const QByteArray png = QByteArray::fromBase64(sample.gmmRoiImagePngBase64.toLatin1());
    const std::vector<uchar> bytes(png.cbegin(), png.cend());
    const cv::Mat image = cv::imdecode(bytes, cv::IMREAD_UNCHANGED);
    if (image.empty()) {
        if (errorMessage)
            *errorMessage = tr("样本 %1 的无损 ROI 无法解码").arg(sample.sampleId);
        return false;
    }
    if ((sample.gmmImageWidth > 0 && sample.gmmImageWidth != image.cols) ||
        (sample.gmmImageHeight > 0 && sample.gmmImageHeight != image.rows) ||
        (!sample.gmmImageSha256.trimmed().isEmpty() &&
         sample.gmmImageSha256 != canonicalImageHash(image))) {
        if (errorMessage)
            *errorMessage = tr("样本 %1 的无损 ROI 尺寸或哈希校验失败").arg(sample.sampleId);
        return false;
    }

    FrameInputMetadata metadata = FrameInputMetadata::fromMat(image, QStringLiteral("template_roi"));
    if (!sample.pixelFormat.trimmed().isEmpty())
        metadata.pixelFormat = sample.pixelFormat;
    if (sample.validBits > 0)
        metadata.validBits = sample.validBits;
    if (sample.bitShift >= 0)
        metadata.bitShift = sample.bitShift;
    if (sample.pixelFormat.trimmed().isEmpty() ||
        gmmPixelFormat(image, metadata) != sample.pixelFormat.trimmed().toUpper()) {
        if (errorMessage)
            *errorMessage = tr("样本 %1 的 pixelFormat 与 ROI 位深或通道数不一致")
                    .arg(sample.sampleId);
        return false;
    }

    // 持久化 ROI 只用于缩略图和后端重建，禁止把 ROI 裁剪图铺满右侧视图。
    // 当前会话若已选择完整外部图则继续保留；否则视图只允许回退到完整基准图/相机图。
    const bool hasSessionSampleImage = !m_sampleImage.empty() &&
            !m_sampleDisplayImage.isNull();
    if (!hasSessionSampleImage) {
        m_sampleImage.release();
        m_sampleImageMetadata = FrameInputMetadata();
        m_sampleDisplayImage = QImage();
        m_sampleImageTitle.clear();
    }
    m_sampleRoiNormalized = normalizedRoiOrDefault(sample.roiNormalized);
    showPreviewImage();
    const QString status = tr("已恢复样本：%1，sampleId=%2")
            .arg(sample.label, sample.sampleId);
    setStatusText(status, status);
    return true;
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

cv::Mat ColorTemplateDialog::cropGmmRoiMat(const cv::Mat &frame,
                                           const QRectF &roiNormalized) const
{
    if (frame.empty())
        return cv::Mat();
    const QRectF roi = normalizedRoiOrDefault(roiNormalized);
    const int left = qBound(0, static_cast<int>(std::floor(roi.x() * frame.cols)), frame.cols - 1);
    const int top = qBound(0, static_cast<int>(std::floor(roi.y() * frame.rows)), frame.rows - 1);
    const int width = qMax(1, static_cast<int>(std::ceil(roi.width() * frame.cols)));
    const int height = qMax(1, static_cast<int>(std::ceil(roi.height() * frame.rows)));
    const cv::Rect bounds(0, 0, frame.cols, frame.rows);
    const cv::Rect clipped = cv::Rect(left, top, width, height) & bounds;
    return clipped.width > 0 && clipped.height > 0 ? frame(clipped).clone() : cv::Mat();
}

QImage ColorTemplateDialog::roiThumbnailForSample(const ColorRecognitionSampleData &sample) const
{
    const QSize targetSize(72, 48);
    QImage thumbnail(targetSize, QImage::Format_ARGB32_Premultiplied);
    thumbnail.fill(Qt::white);

    QPainter painter(&thumbnail);
    painter.setRenderHint(QPainter::Antialiasing, true);

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
    }

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
    updateHsvModelUi();
    updateGmmModelUi();
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

ColorRecognitionHalconConfig ColorTemplateDialog::featureExtractionConfig(
        const FrameInputMetadata &metadata) const
{
    ColorRecognitionHalconConfig config;
    QStringList tried;
    config.halconSoPath = HalconRuntimePaths::resolveHalconLibPath(QString(), &tried);
    config.halconSoPathCandidates = tried;
    config.roiNormalized = effectiveRoiNormalized();
    config.featureType = featureTypeFromUi(m_featureTypeComboBox->currentText());
    config.sensitivity = sensitivityFromUi(m_sensitivityComboBox->currentText());
    const bool gmmSelected = recognitionBackendFromUi(
                m_recognitionBackendComboBox->currentText()) == QStringLiteral("cielab_gmm");
    config.brightnessEnabled = config.featureType == QStringLiteral("histogram") &&
            (gmmSelected ? m_template.brightnessEnabled : m_brightnessCheckBox->isChecked());
    config.pixelFormat = metadata.pixelFormat;
    config.validBits = metadata.validBits;
    config.bitShift = metadata.bitShift;
    return config;
}

void ColorTemplateDialog::setStatusText(const QString &displayText, const QString &tooltipText)
{
    const int width = qMax(120, m_statusLabel->contentsRect().width());
    m_statusLabel->setText(m_statusLabel->fontMetrics().elidedText(displayText, Qt::ElideRight, width));
    m_statusLabel->setToolTip(tooltipText.isEmpty() ? displayText : tooltipText);
}
