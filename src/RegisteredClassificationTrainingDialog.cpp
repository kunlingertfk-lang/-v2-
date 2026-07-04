#include "RegisteredClassificationTrainingDialog.h"

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QTableWidgetItem>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGlobal>

#include <opencv2/core.hpp>

namespace {

QFrame *trainingCard(QWidget *parent, const QString &title)
{
    QFrame *frame = new QFrame(parent);
    frame->setProperty("panelRole", QStringLiteral("trainingCard"));
    QVBoxLayout *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);
    QLabel *titleLabel = new QLabel(title, frame);
    titleLabel->setProperty("role", QStringLiteral("cardTitle"));
    layout->addWidget(titleLabel);
    return frame;
}

QSize initialDialogSize(QWidget *parent, const QSize &fallback)
{
    if (!parent)
        return fallback;

    QSize parentSize = parent->size();
    if (!parentSize.isValid() || parentSize.width() <= 0 || parentSize.height() <= 0)
        parentSize = parent->window()->size();
    if (!parentSize.isValid() || parentSize.width() <= 0 || parentSize.height() <= 0)
        return fallback;

    return QSize(qMax(760, qRound(parentSize.width() * 0.76)),
                 qMax(520, qRound(parentSize.height() * 0.76)));
}

QPushButton *smallButton(QWidget *parent, const QString &text)
{
    QPushButton *button = new QPushButton(text, parent);
    button->setMinimumHeight(38);
    return button;
}

QToolButton *iconButton(QWidget *parent, const QString &text, const QString &tooltip)
{
    QToolButton *button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tooltip);
    button->setMinimumSize(38, 38);
    return button;
}

QIcon roiIcon(const QString &kind)
{
    QPixmap pixmap(36, 36);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(QStringLiteral("#0f172a")), 2.4));
    painter.setBrush(Qt::NoBrush);

    if (kind == QStringLiteral("full")) {
        painter.drawRect(QRectF(7, 7, 22, 22));
        painter.setPen(QPen(QColor(QStringLiteral("#ff7a00")), 2.4));
        painter.drawLine(QPointF(7, 13), QPointF(13, 7));
        painter.drawLine(QPointF(23, 29), QPointF(29, 23));
    } else if (kind == QStringLiteral("rect")) {
        painter.setPen(QPen(QColor(QStringLiteral("#0ea5e9")), 2.8));
        painter.drawRoundedRect(QRectF(8, 10, 20, 16), 2, 2);
        painter.setPen(QPen(QColor(QStringLiteral("#ff7a00")), 2.4, Qt::DashLine));
        painter.drawRect(QRectF(5, 7, 26, 22));
    } else {
        painter.setPen(QPen(QColor(QStringLiteral("#0ea5e9")), 2.8));
        QPolygonF polygon;
        polygon << QPointF(18, 6)
                << QPointF(29, 14)
                << QPointF(25, 29)
                << QPointF(9, 27)
                << QPointF(6, 13);
        painter.drawPolygon(polygon);
        painter.setBrush(QColor(QStringLiteral("#ff7a00")));
        for (const QPointF &point : polygon)
            painter.drawEllipse(point, 2.6, 2.6);
    }

    return QIcon(pixmap);
}

QToolButton *roiButton(QWidget *parent,
                       const QString &objectName,
                       const QString &text,
                       const QIcon &icon)
{
    QToolButton *button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setText(text);
    button->setToolTip(text);
    button->setIcon(icon);
    button->setIconSize(QSize(26, 26));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setCheckable(true);
    button->setMinimumHeight(44);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

} // namespace

RegisteredClassificationTrainingDialog::RegisteredClassificationTrainingDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("注册分类"));
    resize(initialDialogSize(parent, QSize(1120, 720)));
    setMinimumSize(760, 520);

    QHBoxLayout *root = new QHBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 18);
    root->setSpacing(16);

    QFrame *previewPanel = new QFrame(this);
    previewPanel->setProperty("panelRole", QStringLiteral("previewPanel"));
    QVBoxLayout *previewLayout = new QVBoxLayout(previewPanel);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);

    QFrame *toolbar = new QFrame(previewPanel);
    toolbar->setProperty("panelRole", QStringLiteral("previewToolbar"));
    QHBoxLayout *toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(16, 10, 16, 10);
    QLabel *previewTitle = new QLabel(tr("注册图像"), toolbar);
    previewTitle->setProperty("role", QStringLiteral("windowTitle"));
    toolbarLayout->addWidget(previewTitle);
    toolbarLayout->addStretch(1);
    toolbarLayout->addWidget(iconButton(toolbar, QStringLiteral("＋"), tr("放大")));
    toolbarLayout->addWidget(iconButton(toolbar, QStringLiteral("－"), tr("缩小")));
    toolbarLayout->addWidget(iconButton(toolbar, QStringLiteral("⟲"), tr("适配窗口")));
    previewLayout->addWidget(toolbar);

    QGraphicsView *view = new QGraphicsView(previewPanel);
    view->setObjectName(QStringLiteral("registeredTrainingPreviewView"));
    view->setProperty("panelRole", QStringLiteral("trainingCanvas"));
    FrameViewHelper *previewHelper = new FrameViewHelper(view, this);
    previewHelper->setObjectName(QStringLiteral("registeredTrainingPreviewHelper"));
    previewLayout->addWidget(view, 1);

    QFrame *statusBar = new QFrame(previewPanel);
    statusBar->setProperty("panelRole", QStringLiteral("statusBar"));
    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 10, 16, 10);
    QLabel *imageStatusLabel = new QLabel(tr("图像 0 / 标注 0 / 类别 1"), statusBar);
    statusLayout->addWidget(imageStatusLabel);
    statusLayout->addStretch(1);
    QLabel *editStatusLabel = new QLabel(tr("占位界面，训练算法未接入"), statusBar);
    statusLayout->addWidget(editStatusLabel);
    previewLayout->addWidget(statusBar);
    root->addWidget(previewPanel, 1);

    QFrame *rightPanel = new QFrame(this);
    rightPanel->setMinimumWidth(360);
    rightPanel->setMaximumWidth(430);
    rightPanel->setProperty("panelRole", QStringLiteral("rightPanel"));
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);

    QFrame *addCard = trainingCard(rightPanel, tr("1/ 添加注册图"));
    QHBoxLayout *addButtons = new QHBoxLayout;
    QPushButton *cameraButton = smallButton(addCard, tr("相机抓图"));
    cameraButton->setObjectName(QStringLiteral("trainingCameraCaptureButton"));
    QPushButton *storedImageButton = smallButton(addCard, tr("存图导入"));
    storedImageButton->setObjectName(QStringLiteral("trainingStoredImageButton"));
    storedImageButton->setEnabled(false);
    storedImageButton->setToolTip(tr("存图导入暂未接入"));
    QPushButton *externalImportButton = smallButton(addCard, tr("外部导入"));
    externalImportButton->setObjectName(QStringLiteral("trainingExternalImportButton"));
    addButtons->addWidget(cameraButton);
    addButtons->addWidget(storedImageButton);
    addButtons->addWidget(externalImportButton);
    qobject_cast<QVBoxLayout *>(addCard->layout())->addLayout(addButtons);
    rightLayout->addWidget(addCard);

    QFrame *markCard = trainingCard(rightPanel, tr("2/ 标注图像"));
    QHBoxLayout *roiButtons = new QHBoxLayout;
    QToolButton *fullButton = roiButton(markCard,
                                        QStringLiteral("trainingFullRoiButton"),
                                        tr("全屏框选"),
                                        roiIcon(QStringLiteral("full")));
    QToolButton *rectButton = roiButton(markCard,
                                        QStringLiteral("trainingRectRoiButton"),
                                        tr("矩形框选"),
                                        roiIcon(QStringLiteral("rect")));
    QToolButton *polygonButton = roiButton(markCard,
                                           QStringLiteral("trainingPolygonRoiButton"),
                                           tr("多边形框选"),
                                           roiIcon(QStringLiteral("polygon")));
    roiButtons->addWidget(fullButton);
    roiButtons->addWidget(rectButton);
    roiButtons->addWidget(polygonButton);
    qobject_cast<QVBoxLayout *>(markCard->layout())->addLayout(roiButtons);

    QTableWidget *classTable = new QTableWidget(1, 4, markCard);
    classTable->setHorizontalHeaderLabels(QStringList()
                                          << tr("类别")
                                          << tr("编辑")
                                          << tr("预览")
                                          << tr("删除"));
    classTable->setItem(0, 0, new QTableWidgetItem(QStringLiteral("Classification0")));
    classTable->setItem(0, 1, new QTableWidgetItem(QStringLiteral("...")));
    classTable->setItem(0, 2, new QTableWidgetItem(QStringLiteral("◉")));
    classTable->setItem(0, 3, new QTableWidgetItem(QStringLiteral("×")));
    classTable->horizontalHeader()->setStretchLastSection(true);
    classTable->verticalHeader()->setVisible(false);
    classTable->setMinimumHeight(118);
    qobject_cast<QVBoxLayout *>(markCard->layout())->addWidget(classTable);
    qobject_cast<QVBoxLayout *>(markCard->layout())->addWidget(smallButton(markCard, tr("+ 新建")));
    rightLayout->addWidget(markCard, 1);

    QFrame *options = trainingCard(rightPanel, tr("训练参数"));
    QComboBox *modelTypeCombo = new QComboBox(options);
    modelTypeCombo->addItem(tr("快速模式"));
    QComboBox *taskTypeCombo = new QComboBox(options);
    taskTypeCombo->addItem(tr("非姿态任务"));
    QVBoxLayout *optionsLayout = qobject_cast<QVBoxLayout *>(options->layout());
    optionsLayout->addWidget(new QLabel(tr("模型类型"), options));
    optionsLayout->addWidget(modelTypeCombo);
    optionsLayout->addWidget(new QLabel(tr("任务类型"), options));
    optionsLayout->addWidget(taskTypeCombo);
    rightLayout->addWidget(options);

    QHBoxLayout *bottom = new QHBoxLayout;
    QLabel *statusLabel = new QLabel(tr("未注册"), rightPanel);
    statusLabel->setProperty("role", QStringLiteral("stateLabel"));
    QPushButton *trainButton = new QPushButton(tr("开始训练"), rightPanel);
    trainButton->setProperty("actionRole", QStringLiteral("primary"));
    trainButton->setEnabled(false);
    bottom->addWidget(statusLabel);
    bottom->addStretch(1);
    bottom->addWidget(trainButton);
    rightLayout->addLayout(bottom);
    root->addWidget(rightPanel);

    auto setTrainingImage = [previewHelper, imageStatusLabel, editStatusLabel](const QImage &image,
                                                                              const QString &sourceText) {
        if (image.isNull()) {
            editStatusLabel->setText(QObject::tr("图像为空，无法显示"));
            return;
        }
        previewHelper->setImage(image);
        previewHelper->clearRoi();
        previewHelper->clearPolygonRoi();
        previewHelper->fitToView();
        imageStatusLabel->setText(QObject::tr("图像 1 / 标注 0 / 类别 1"));
        editStatusLabel->setText(sourceText);
    };

    auto setRoiMode = [previewHelper, fullButton, rectButton, polygonButton, editStatusLabel](QToolButton *button) {
        const bool turnOn = button->isChecked();
        fullButton->setChecked(false);
        rectButton->setChecked(false);
        polygonButton->setChecked(false);
        previewHelper->setRoiDrawingEnabled(false);
        previewHelper->setPolygonDrawingEnabled(false);

        if (!turnOn) {
            editStatusLabel->setText(QObject::tr("已退出 ROI 绘制"));
            return;
        }

        button->setChecked(true);
        if (button == fullButton) {
            previewHelper->clearPolygonRoi();
            previewHelper->setRoiRectNormalized(QRectF(0.0, 0.0, 1.0, 1.0));
            editStatusLabel->setText(QObject::tr("已选择全屏 ROI"));
        } else if (button == rectButton) {
            previewHelper->clearPolygonRoi();
            previewHelper->setRoiDrawingEnabled(true);
            editStatusLabel->setText(QObject::tr("当前编辑：矩形 ROI。按住左键拖拽绘制。"));
        } else {
            previewHelper->clearRoi();
            previewHelper->setPolygonDrawingEnabled(true);
            editStatusLabel->setText(QObject::tr("当前编辑：多边形 ROI。左键添加点，靠近首点点击自动闭合，右键撤销。"));
        }
    };

    connect(cameraButton, &QPushButton::clicked, this, [setTrainingImage, editStatusLabel]() {
        const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
        if (frame.empty()) {
            editStatusLabel->setText(QObject::tr("当前相机帧为空，无法抓图"));
            return;
        }
        const QImage image = MatImageConverter::matToDisplayImage(
                    frame, QStringLiteral("RegisteredClassificationTrainingDialog"));
        setTrainingImage(image, QObject::tr("已从当前相机帧抓图"));
    });

    connect(externalImportButton, &QPushButton::clicked, this, [this, setTrainingImage, editStatusLabel]() {
        const QString path = QFileDialog::getOpenFileName(
                    this,
                    tr("外部导入注册图"),
                    QString(),
                    tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
        if (path.trimmed().isEmpty())
            return;
        const QImage image(path);
        if (image.isNull()) {
            editStatusLabel->setText(tr("图片读取失败：%1").arg(path));
            return;
        }
        setTrainingImage(image, tr("已导入图片：%1").arg(QFileInfo(path).fileName()));
    });

    connect(fullButton, &QToolButton::clicked, this, [setRoiMode, fullButton]() {
        setRoiMode(fullButton);
    });
    connect(rectButton, &QToolButton::clicked, this, [setRoiMode, rectButton]() {
        setRoiMode(rectButton);
    });
    connect(polygonButton, &QToolButton::clicked, this, [setRoiMode, polygonButton]() {
        setRoiMode(polygonButton);
    });

    connect(previewHelper, &FrameViewHelper::roiChanged, this, [previewHelper, rectButton, editStatusLabel](const QRectF &roi) {
        rectButton->setChecked(true);
        previewHelper->setRoiRectNormalized(roi);
        editStatusLabel->setText(QObject::tr("矩形 ROI：x=%1 y=%2 w=%3 h=%4")
                                 .arg(roi.x(), 0, 'f', 3)
                                 .arg(roi.y(), 0, 'f', 3)
                                 .arg(roi.width(), 0, 'f', 3)
                                 .arg(roi.height(), 0, 'f', 3));
    });
    connect(previewHelper, &FrameViewHelper::roiSelectionRejected, this, [editStatusLabel](const QRectF &) {
        editStatusLabel->setText(QObject::tr("矩形 ROI 无效，请重新绘制"));
    });
    connect(previewHelper, &FrameViewHelper::polygonChanged, this, [previewHelper, polygonButton, editStatusLabel](const QVector<QPointF> &points) {
        polygonButton->setChecked(true);
        previewHelper->setPolygonRoiNormalized(points);
        editStatusLabel->setText(QObject::tr("多边形 ROI：%1 个点").arg(points.size()));
    });
    connect(previewHelper, &FrameViewHelper::polygonSelectionRejected, this, [editStatusLabel](int pointCount) {
        editStatusLabel->setText(QObject::tr("多边形 ROI 至少需要 3 个点，当前 %1 个点").arg(pointCount));
    });

    setStyleSheet(QStringLiteral(
        "QDialog{background:#ffffff;color:#0f172a;font-size:18px;}"
        "QFrame[panelRole=\"previewPanel\"],QFrame[panelRole=\"trainingCard\"]{background:#ffffff;border:3px solid #60a5fa;border-radius:8px;}"
        "QFrame[panelRole=\"rightPanel\"]{background:#ffffff;border:0;}"
        "QFrame[panelRole=\"previewToolbar\"]{background:#ffffff;border-bottom:4px solid #ff7a00;border-top-left-radius:8px;border-top-right-radius:8px;}"
        "QFrame[panelRole=\"statusBar\"]{background:#fff7ed;border-top:3px solid #ffb366;color:#7c2d12;}"
        "QGraphicsView[panelRole=\"trainingCanvas\"]{border:0;background:#05070a;}"
        "QLabel{background:transparent;font-size:18px;color:#0f172a;font-weight:600;}"
        "QLabel[role=\"windowTitle\"]{font-size:28px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"cardTitle\"]{font-size:23px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"stateLabel\"]{font-size:24px;font-weight:800;color:#c2410c;}"
        "QPushButton,QToolButton,QComboBox{background:#ffffff;color:#0f172a;border:2px solid #4094ff;border-radius:6px;padding:10px;font-size:18px;font-weight:700;}"
        "QPushButton:hover,QToolButton:hover{background:#e0f2fe;border-color:#0284c7;}"
        "QPushButton[actionRole=\"primary\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;font-size:20px;font-weight:800;}"
        "QPushButton:disabled{background:#f8fafc;color:#64748b;border-color:#94a3b8;}"
        "QTableWidget{background:#ffffff;color:#0f172a;border:2px solid #93c5fd;gridline-color:#bfdbfe;font-size:18px;selection-background-color:#dbeafe;}"
        "QHeaderView::section{background:#dbeafe;color:#08386f;font-weight:800;font-size:18px;border:0;padding:8px;}"));
}
