#include "RegisteredClassificationTrainingDialog.h"

#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QApplication>
#include <QComboBox>
#include <QColor>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QMessageBox>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSharedPointer>
#include <QSize>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGlobal>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <functional>

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

QToolButton *rowIconButton(QWidget *parent,
                           const QString &objectName,
                           const QIcon &icon,
                           const QString &tooltip)
{
    QToolButton *button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setIcon(icon);
    button->setIconSize(QSize(24, 24));
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

using TrainingRoiMark = RegisteredClassificationTrainingRoiMark;
using TrainingImageState = RegisteredClassificationTrainingImageState;
using TrainingSessionState = RegisteredClassificationTrainingSessionState;

class ThumbnailHoverFilter : public QObject
{
public:
    explicit ThumbnailHoverFilter(QToolButton *button, QObject *parent = nullptr)
        : QObject(parent)
        , button_(button)
    {
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)
        if (!button_)
            return false;
        if (event->type() == QEvent::Enter)
            button_->show();
        else if (event->type() == QEvent::Leave)
            button_->hide();
        return false;
    }

private:
    QToolButton *button_ = nullptr;
};

class TrainingRoiPreviewCard : public QFrame
{
public:
    explicit TrainingRoiPreviewCard(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
    }

    std::function<void()> selectHandler;
    std::function<void()> deleteHandler;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (selectHandler)
            selectHandler();
        QFrame::mousePressEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        if (selectHandler)
            selectHandler();
        QMenu *menu = new QMenu(this);
        menu->setObjectName(QStringLiteral("registeredTrainingPreviewRoiContextMenu"));
        menu->setAttribute(Qt::WA_DeleteOnClose);
        QAction *deleteAction = menu->addAction(QObject::tr("删除当前 ROI"));
        QObject::connect(deleteAction, &QAction::triggered, this, [this]() {
            if (QMenu *menu = qobject_cast<QMenu *>(sender()->parent())) {
                menu->hide();
                menu->close();
                menu->deleteLater();
            }
            if (deleteHandler)
                deleteHandler();
        });
        menu->popup(event->globalPos());
        event->accept();
    }
};

class TrainingRoiContextMenuFilter : public QObject
{
public:
    explicit TrainingRoiContextMenuFilter(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    std::function<void(const QPoint &, const QPoint &)> openMenu;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        Q_UNUSED(watched)
        if (!openMenu)
            return false;
        if (event->type() == QEvent::ContextMenu) {
            QContextMenuEvent *contextEvent = static_cast<QContextMenuEvent *>(event);
            openMenu(contextEvent->pos(), contextEvent->globalPos());
            event->accept();
            return true;
        }
        return false;
    }
};

class ClickableClassRow : public QFrame
{
public:
    explicit ClickableClassRow(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setCursor(Qt::PointingHandCursor);
    }

    std::function<void()> clickHandler;

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && clickHandler)
            clickHandler();
        QFrame::mouseReleaseEvent(event);
    }
};

QRectF normalizedBoundingRect(const TrainingRoiMark &mark)
{
    if (mark.type == QStringLiteral("polygon")) {
        QPolygonF polygon;
        for (const QPointF &point : mark.polygon)
            polygon << point;
        return polygon.boundingRect().normalized();
    }
    return mark.rect.normalized();
}

QRect imageCropRect(const QImage &image, const TrainingRoiMark &mark)
{
    if (image.isNull())
        return QRect();
    if (mark.type == QStringLiteral("full"))
        return image.rect();

    const QRectF normalized = normalizedBoundingRect(mark).intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    QRect crop(qRound(normalized.x() * image.width()),
               qRound(normalized.y() * image.height()),
               qRound(normalized.width() * image.width()),
               qRound(normalized.height() * image.height()));
    crop = crop.normalized().intersected(image.rect());
    if (crop.width() <= 0 || crop.height() <= 0)
        return QRect();
    return crop;
}

bool markContainsNormalizedPoint(const TrainingRoiMark &mark, const QPointF &point)
{
    if (point.x() < 0.0 || point.x() > 1.0 || point.y() < 0.0 || point.y() > 1.0)
        return false;
    if (mark.type == QStringLiteral("polygon") && mark.polygon.size() >= 3) {
        QPolygonF polygon;
        for (const QPointF &polygonPoint : mark.polygon)
            polygon << polygonPoint;
        return polygon.containsPoint(point, Qt::OddEvenFill);
    }
    return mark.rect.normalized().contains(point);
}

QRectF trainingRoiForRunner(const TrainingRoiMark &mark)
{
    if (mark.type == QStringLiteral("full"))
        return QRectF(0.0, 0.0, 1.0, 1.0);
    return normalizedBoundingRect(mark).intersected(QRectF(0.0, 0.0, 1.0, 1.0));
}

cv::Mat qImageToBgrMat(const QImage &image)
{
    if (image.isNull())
        return cv::Mat();

    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat rgbMat(rgb.height(),
                   rgb.width(),
                   CV_8UC3,
                   const_cast<uchar *>(rgb.constBits()),
                   static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat bgr;
    cv::cvtColor(rgbMat, bgr, cv::COLOR_RGB2BGR);
    return bgr.clone();
}

} // namespace

RegisteredClassificationTrainingDialog::RegisteredClassificationTrainingDialog(QWidget *parent)
    : QDialog(parent)
    , m_state(new TrainingSessionState)
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
    QToolButton *previousImageButton = iconButton(toolbar, QStringLiteral("‹"), tr("上一张注册图"));
    previousImageButton->setObjectName(QStringLiteral("registeredTrainingPreviousImageButton"));
    QToolButton *nextImageButton = iconButton(toolbar, QStringLiteral("›"), tr("下一张注册图"));
    nextImageButton->setObjectName(QStringLiteral("registeredTrainingNextImageButton"));
    toolbarLayout->addWidget(previousImageButton);
    toolbarLayout->addWidget(nextImageButton);
    toolbarLayout->addWidget(iconButton(toolbar, QStringLiteral("＋"), tr("放大")));
    toolbarLayout->addWidget(iconButton(toolbar, QStringLiteral("－"), tr("缩小")));
    toolbarLayout->addWidget(iconButton(toolbar, QStringLiteral("⟲"), tr("适配窗口")));
    previewLayout->addWidget(toolbar);

    QStackedWidget *previewStack = new QStackedWidget(previewPanel);
    previewStack->setObjectName(QStringLiteral("registeredTrainingPreviewStack"));

    QWidget *imagePage = new QWidget(previewStack);
    QVBoxLayout *imagePageLayout = new QVBoxLayout(imagePage);
    imagePageLayout->setContentsMargins(0, 0, 0, 0);
    imagePageLayout->setSpacing(0);
    QGraphicsView *view = new QGraphicsView(imagePage);
    view->setObjectName(QStringLiteral("registeredTrainingPreviewView"));
    view->setProperty("panelRole", QStringLiteral("trainingCanvas"));
    FrameViewHelper *previewHelper = new FrameViewHelper(view, this);
    previewHelper->setObjectName(QStringLiteral("registeredTrainingPreviewHelper"));
    imagePageLayout->addWidget(view, 1);
    previewStack->addWidget(imagePage);

    QWidget *previewPage = new QWidget(previewStack);
    previewPage->setObjectName(QStringLiteral("registeredTrainingPreviewPage"));
    previewPage->setProperty("panelRole", QStringLiteral("roiPreviewPage"));
    QVBoxLayout *previewPageLayout = new QVBoxLayout(previewPage);
    previewPageLayout->setContentsMargins(24, 22, 24, 22);
    previewPageLayout->setSpacing(16);
    QHBoxLayout *previewHeaderLayout = new QHBoxLayout;
    QLabel *previewPageTitle = new QLabel(tr("Classification0 | 已标注目标：0"), previewPage);
    previewPageTitle->setProperty("role", QStringLiteral("previewPageTitle"));
    QToolButton *previewCloseButton = iconButton(previewPage, QStringLiteral("×"), tr("关闭预览"));
    previewCloseButton->setObjectName(QStringLiteral("registeredTrainingPreviewCloseButton"));
    previewHeaderLayout->addWidget(previewPageTitle);
    previewHeaderLayout->addStretch(1);
    previewHeaderLayout->addWidget(previewCloseButton);
    previewPageLayout->addLayout(previewHeaderLayout);

    QLabel *previewEmptyLabel = new QLabel(tr("当前类别暂无 ROI 标注"), previewPage);
    previewEmptyLabel->setAlignment(Qt::AlignCenter);
    previewEmptyLabel->setProperty("role", QStringLiteral("previewEmpty"));
    QScrollArea *roiPreviewScrollArea = new QScrollArea(previewPage);
    roiPreviewScrollArea->setWidgetResizable(true);
    roiPreviewScrollArea->setFrameShape(QFrame::NoFrame);
    QWidget *roiPreviewListWidget = new QWidget(roiPreviewScrollArea);
    roiPreviewListWidget->setObjectName(QStringLiteral("registeredTrainingRoiPreviewList"));
    QVBoxLayout *roiPreviewListLayout = new QVBoxLayout(roiPreviewListWidget);
    roiPreviewListLayout->setContentsMargins(0, 0, 0, 0);
    roiPreviewListLayout->setSpacing(14);
    roiPreviewScrollArea->setWidget(roiPreviewListWidget);
    previewPageLayout->addWidget(previewEmptyLabel);
    previewPageLayout->addWidget(roiPreviewScrollArea, 1);
    previewStack->addWidget(previewPage);
    previewStack->setCurrentWidget(imagePage);
    previewLayout->addWidget(previewStack, 1);

    QFrame *statusBar = new QFrame(previewPanel);
    statusBar->setProperty("panelRole", QStringLiteral("statusBar"));
    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 10, 16, 10);
    QComboBox *filterCombo = new QComboBox(statusBar);
    filterCombo->setObjectName(QStringLiteral("registeredTrainingFilterCombo"));
    filterCombo->setProperty("role", QStringLiteral("filterBox"));
    filterCombo->addItem(tr("全部"), QStringLiteral("all"));
    filterCombo->addItem(tr("标注"), QStringLiteral("marked"));
    filterCombo->addItem(tr("未标注"), QStringLiteral("unmarked"));
    filterCombo->view()->setObjectName(QStringLiteral("registeredTrainingFilterComboView"));
    filterCombo->view()->setStyleSheet(QStringLiteral(
        "QAbstractItemView{background:#2d333f;color:#ffffff;"
        "selection-background-color:#0ea5e9;selection-color:#ffffff;"
        "border:2px solid #94a3b8;font-size:18px;font-weight:700;outline:0;}"
        "QAbstractItemView::item{min-height:34px;padding:6px 10px;background:#2d333f;color:#ffffff;}"
        "QAbstractItemView::item:hover,QAbstractItemView::item:selected{background:#0ea5e9;color:#ffffff;}"));
    QLabel *imageStatusLabel = new QLabel(tr("图像 0 / 标注 0 / 类别 1"), statusBar);
    statusLayout->addWidget(filterCombo);
    statusLayout->addSpacing(18);
    statusLayout->addWidget(imageStatusLabel);
    statusLayout->addStretch(1);
    QLabel *editStatusLabel = new QLabel(tr("请添加注册图并标注至少两个类别样本"), statusBar);
    statusLayout->addWidget(editStatusLabel);
    previewLayout->addWidget(statusBar);

    QListWidget *thumbnailList = new QListWidget(previewPanel);
    thumbnailList->setObjectName(QStringLiteral("registeredTrainingThumbnailList"));
    thumbnailList->setViewMode(QListView::IconMode);
    thumbnailList->setIconSize(QSize(96, 72));
    thumbnailList->setResizeMode(QListView::Adjust);
    thumbnailList->setMovement(QListView::Static);
    thumbnailList->setWrapping(false);
    thumbnailList->setFlow(QListView::LeftToRight);
    thumbnailList->setFixedHeight(154);
    thumbnailList->setSpacing(8);
    QFrame *thumbnailToolbar = new QFrame(previewPanel);
    thumbnailToolbar->setProperty("panelRole", QStringLiteral("thumbnailToolbar"));
    QHBoxLayout *thumbnailToolbarLayout = new QHBoxLayout(thumbnailToolbar);
    thumbnailToolbarLayout->setContentsMargins(12, 6, 12, 6);
    thumbnailToolbarLayout->addStretch(1);
    QToolButton *deleteAllImagesButton = new QToolButton(thumbnailToolbar);
    deleteAllImagesButton->setObjectName(QStringLiteral("registeredTrainingDeleteAllImagesButton"));
    deleteAllImagesButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
    deleteAllImagesButton->setIconSize(QSize(20, 20));
    deleteAllImagesButton->setToolTip(tr("删除全部注册图"));
    deleteAllImagesButton->setMinimumSize(34, 34);
    thumbnailToolbarLayout->addWidget(deleteAllImagesButton);
    previewLayout->addWidget(thumbnailToolbar);
    previewLayout->addWidget(thumbnailList);
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

    QHBoxLayout *classHeaderLayout = new QHBoxLayout;
    QLabel *classCountLabel = new QLabel(tr("分类列表(1)"), markCard);
    classCountLabel->setObjectName(QStringLiteral("registeredTrainingClassCountLabel"));
    QPushButton *createClassButton = smallButton(markCard, tr("+ 新建"));
    createClassButton->setObjectName(QStringLiteral("registeredTrainingCreateClassButton"));
    QPushButton *clearAllMarksButton = smallButton(markCard, tr("清除全部标注"));
    clearAllMarksButton->setObjectName(QStringLiteral("registeredTrainingClearAllMarksButton"));
    classHeaderLayout->addWidget(classCountLabel);
    classHeaderLayout->addStretch(1);
    classHeaderLayout->addWidget(createClassButton);
    classHeaderLayout->addWidget(clearAllMarksButton);
    qobject_cast<QVBoxLayout *>(markCard->layout())->addLayout(classHeaderLayout);

    QFrame *classHeader = new QFrame(markCard);
    classHeader->setProperty("panelRole", QStringLiteral("classHeader"));
    QHBoxLayout *classHeaderFields = new QHBoxLayout(classHeader);
    classHeaderFields->setContentsMargins(12, 8, 12, 8);
    QLabel *classNameHeader = new QLabel(tr("标签类型"), classHeader);
    QLabel *classCountHeader = new QLabel(tr("目标总数/图像总数"), classHeader);
    classHeaderFields->addWidget(classNameHeader, 2);
    classHeaderFields->addWidget(classCountHeader, 1);
    classHeaderFields->addSpacing(128);
    qobject_cast<QVBoxLayout *>(markCard->layout())->addWidget(classHeader);

    QScrollArea *classScrollArea = new QScrollArea(markCard);
    classScrollArea->setWidgetResizable(true);
    classScrollArea->setFrameShape(QFrame::NoFrame);
    QWidget *classListWidget = new QWidget(classScrollArea);
    classListWidget->setObjectName(QStringLiteral("registeredTrainingClassList"));
    classListWidget->setProperty("panelRole", QStringLiteral("classList"));
    QVBoxLayout *classListLayout = new QVBoxLayout(classListWidget);
    classListLayout->setContentsMargins(0, 0, 0, 0);
    classListLayout->setSpacing(8);
    classScrollArea->setWidget(classListWidget);
    classScrollArea->setMinimumHeight(142);
    qobject_cast<QVBoxLayout *>(markCard->layout())->addWidget(classScrollArea, 1);
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
    m_trainStatusLabel = new QLabel(tr("请至少为两个类别添加 ROI 样本"), rightPanel);
    m_trainStatusLabel->setProperty("role", QStringLiteral("stateLabel"));
    m_trainButton = new QPushButton(tr("开始训练"), rightPanel);
    m_trainButton->setProperty("actionRole", QStringLiteral("primary"));
    m_trainButton->setEnabled(false);
    bottom->addWidget(m_trainStatusLabel);
    bottom->addStretch(1);
    bottom->addWidget(m_trainButton);
    rightLayout->addLayout(bottom);
    root->addWidget(rightPanel);

    QSharedPointer<TrainingSessionState> state = m_state;

    auto currentImageState = [state]() -> TrainingImageState * {
        if (state->currentImage < 0 || state->currentImage >= state->images.size())
            return nullptr;
        return &state->images[state->currentImage];
    };

    auto imageHasAnyMark = [state](const int imageIndex) {
        if (imageIndex < 0 || imageIndex >= state->images.size())
            return false;
        for (const QVector<TrainingRoiMark> &marks : state->images[imageIndex].marksByClass) {
            if (!marks.isEmpty())
                return true;
        }
        return false;
    };
    auto imageMatchesFilter = [state, imageHasAnyMark](const int imageIndex) {
        if (imageIndex < 0 || imageIndex >= state->images.size())
            return false;
        if (state->thumbnailFilter == QStringLiteral("marked"))
            return imageHasAnyMark(imageIndex);
        if (state->thumbnailFilter == QStringLiteral("unmarked"))
            return !imageHasAnyMark(imageIndex);
        return true;
    };
    auto firstVisibleImage = [state, imageMatchesFilter]() {
        for (int index = 0; index < state->images.size(); ++index) {
            if (imageMatchesFilter(index))
                return index;
        }
        return -1;
    };

    auto classTargetCount = [state](const int classIndex) {
        int count = 0;
        for (const TrainingImageState &image : state->images) {
            count += image.marksByClass.value(classIndex).size();
        }
        return count;
    };

    auto classImageCount = [state](const int classIndex) {
        int count = 0;
        for (const TrainingImageState &image : state->images) {
            if (!image.marksByClass.value(classIndex).isEmpty())
                ++count;
        }
        return count;
    };

    auto totalMarkCount = [state]() {
        int count = 0;
        for (const TrainingImageState &image : state->images) {
            for (const QVector<TrainingRoiMark> &marks : image.marksByClass)
                count += marks.size();
        }
        return count;
    };

    auto refreshStatus = [this, state, totalMarkCount, imageStatusLabel]() {
        const int roiCount = totalMarkCount();
        setProperty("registeredTrainingClassCount", state->classes.size());
        setProperty("registeredTrainingImageCount", state->images.size());
        setProperty("registeredTrainingRoiCount", roiCount);
        refreshTrainingReadiness();
        imageStatusLabel->setText(QObject::tr("图像 %1 / 标注 %2 / 类别 %3")
                                  .arg(state->images.size())
                                  .arg(roiCount)
                                  .arg(state->classes.size()));
    };

    QSharedPointer<std::function<void()>> refreshThumbnails(new std::function<void()>);
    QSharedPointer<std::function<void()>> refreshClassList(new std::function<void()>);
    QSharedPointer<std::function<void(const QString &)>> showCurrentImage(
                new std::function<void(const QString &)>);
    QSharedPointer<std::function<void(int)>> renderPreviewPage(new std::function<void(int)>);
    QSharedPointer<std::function<void(const QString &)>> returnToImagePage(
                new std::function<void(const QString &)>);
    QSharedPointer<std::function<void(int)>> deleteImage(new std::function<void(int)>);
    QSharedPointer<std::function<void(int, int, int, const QString &)>> deleteRoi(
                new std::function<void(int, int, int, const QString &)>);

    auto closeRoiDeleteMenus = []() {
        const QList<QWidget *> widgets = QApplication::topLevelWidgets();
        for (QWidget *widget : widgets) {
            QMenu *menu = qobject_cast<QMenu *>(widget);
            if (!menu)
                continue;
            const QString name = menu->objectName();
            if (name != QStringLiteral("registeredTrainingRoiContextMenu") &&
                name != QStringLiteral("registeredTrainingPreviewRoiContextMenu"))
                continue;
            menu->hide();
            menu->close();
            menu->deleteLater();
        }
    };

    auto clearRoiNumberLabels = [view]() {
        const QList<QLabel *> labels = view->findChildren<QLabel *>(
                    QRegularExpression(QStringLiteral("^registeredTrainingRoiNumberLabel_\\d+$")));
        for (QLabel *label : labels)
            label->deleteLater();
    };

    auto makeImageRect = [](const QImage &image, const TrainingRoiMark &mark) {
        const QRect crop = imageCropRect(image, mark);
        return QRectF(crop);
    };

    auto addNumberLabel = [view](const int index, const QRectF &normalizedRect) {
        QLabel *label = new QLabel(QString::number(index + 1), view);
        label->setObjectName(QStringLiteral("registeredTrainingRoiNumberLabel_%1").arg(index));
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet(QStringLiteral(
            "background:rgba(255,255,255,210);color:#ef1f2d;border:1px solid #ef1f2d;"
            "border-radius:2px;font-size:15px;font-weight:800;"));
        label->setFixedSize(22, 22);
        const QRect viewportRect = view->viewport()->rect();
        const int x = qBound(6, qRound(normalizedRect.x() * viewportRect.width()) + 8,
                             qMax(6, viewportRect.width() - 28));
        const int y = qBound(6, qRound(normalizedRect.y() * viewportRect.height()) + 8,
                             qMax(6, viewportRect.height() - 28));
        label->move(x, y);
        label->show();
    };

    *refreshThumbnails = [=]() {
        QSignalBlocker blocker(thumbnailList);
        QSignalBlocker filterBlocker(filterCombo);
        const int filterIndex = filterCombo->findData(state->thumbnailFilter);
        if (filterIndex >= 0)
            filterCombo->setCurrentIndex(filterIndex);
        thumbnailList->clear();
        state->visibleImageIndexes.clear();
        for (int index = 0; index < state->images.size(); ++index) {
            if (!imageMatchesFilter(index))
                continue;
            state->visibleImageIndexes.append(index);
            const TrainingImageState &imageState = state->images.at(index);
            const QString text = imageHasAnyMark(index)
                    ? QObject::tr("%1 已标注").arg(imageState.name)
                    : imageState.name;
            QListWidgetItem *item = new QListWidgetItem(thumbnailList);
            item->setData(Qt::UserRole, index);
            item->setSizeHint(QSize(132, 106));
            thumbnailList->addItem(item);
            QFrame *thumbnailCard = new QFrame(thumbnailList);
            thumbnailCard->setObjectName(QStringLiteral("registeredTrainingThumbnail_%1").arg(index));
            thumbnailCard->setProperty("panelRole", QStringLiteral("thumbnailCard"));
            QVBoxLayout *thumbnailLayout = new QVBoxLayout(thumbnailCard);
            thumbnailLayout->setContentsMargins(6, 6, 6, 6);
            thumbnailLayout->setSpacing(4);
            QFrame *imageFrame = new QFrame(thumbnailCard);
            imageFrame->setProperty("panelRole", QStringLiteral("thumbnailImageFrame"));
            QGridLayout *imageLayout = new QGridLayout(imageFrame);
            imageLayout->setContentsMargins(0, 0, 0, 0);
            QLabel *imageLabel = new QLabel(imageFrame);
            imageLabel->setAlignment(Qt::AlignCenter);
            imageLabel->setFixedSize(108, 66);
            imageLabel->setPixmap(QPixmap::fromImage(imageState.image.scaled(108, 66,
                                                                             Qt::KeepAspectRatio,
                                                                             Qt::SmoothTransformation)));
            QToolButton *deleteImageButton = new QToolButton(imageFrame);
            deleteImageButton->setObjectName(QStringLiteral("registeredTrainingDeleteImageButton_%1").arg(index));
            deleteImageButton->setProperty("role", QStringLiteral("thumbnailDeleteButton"));
            deleteImageButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_TitleBarCloseButton));
            deleteImageButton->setIconSize(QSize(18, 18));
            deleteImageButton->setToolTip(QObject::tr("删除注册图"));
            deleteImageButton->setFixedSize(28, 28);
            deleteImageButton->hide();
            imageLayout->addWidget(imageLabel, 0, 0);
            imageLayout->addWidget(deleteImageButton, 0, 0, Qt::AlignTop | Qt::AlignRight);
            QLabel *caption = new QLabel(text, thumbnailCard);
            caption->setObjectName(QStringLiteral("registeredTrainingThumbnailCaption_%1").arg(index));
            caption->setProperty("role", QStringLiteral("thumbnailCaption"));
            caption->setAlignment(Qt::AlignCenter);
            thumbnailLayout->addWidget(imageFrame);
            thumbnailLayout->addWidget(caption);
            thumbnailList->setItemWidget(item, thumbnailCard);
            ThumbnailHoverFilter *hoverFilter = new ThumbnailHoverFilter(deleteImageButton, thumbnailCard);
            thumbnailCard->installEventFilter(hoverFilter);
            imageFrame->installEventFilter(hoverFilter);
            imageLabel->installEventFilter(hoverFilter);
            caption->installEventFilter(hoverFilter);
            QObject::connect(deleteImageButton, &QToolButton::clicked, thumbnailList, [=]() {
                (*deleteImage)(index);
            });
        }
        const int currentVisibleRow = state->visibleImageIndexes.indexOf(state->currentImage);
        if (currentVisibleRow >= 0)
            thumbnailList->setCurrentRow(currentVisibleRow);
    };

    *showCurrentImage = [state,
                         currentImageState,
                         previewHelper,
                         previewStack,
                         imagePage,
                         editStatusLabel,
                         clearRoiNumberLabels,
                         makeImageRect,
                         addNumberLabel](const QString &message) {
        TrainingImageState *imageState = currentImageState();
        if (!imageState) {
            previewHelper->clear();
            clearRoiNumberLabels();
            editStatusLabel->setText(QObject::tr("请先添加注册图"));
            return;
        }

        previewStack->setCurrentWidget(imagePage);
        state->previewClass = -1;
        state->selectedPreviewImage = -1;
        state->selectedPreviewRoi = -1;
        state->activeEditImage = -1;
        state->activeEditClass = -1;
        state->activeEditRoi = -1;
        state->selectedRoiImage = -1;
        state->selectedRoiClass = -1;
        state->selectedRoiIndex = -1;
        previewHelper->setImage(imageState->image);
        previewHelper->clearRoi();
        previewHelper->clearPolygonRoi();
        previewHelper->clearToolOverlays();
        clearRoiNumberLabels();
        const QVector<TrainingRoiMark> marks = imageState->marksByClass.value(state->currentClass);
        QVector<ToolOverlay> overlays;
        for (int markIndex = 0; markIndex < marks.size(); ++markIndex) {
            const TrainingRoiMark &mark = marks.at(markIndex);
            const QRectF imageRect = makeImageRect(imageState->image, mark);
            if (imageRect.isEmpty())
                continue;
            if (mark.type == QStringLiteral("polygon")) {
                ToolOverlay polygonOverlay;
                polygonOverlay.type = ToolOverlayType::Polygon;
                polygonOverlay.label = QStringLiteral("roi");
                for (const QPointF &point : mark.polygon) {
                    polygonOverlay.points.append(QPointF(point.x() * imageState->image.width(),
                                                         point.y() * imageState->image.height()));
                }
                overlays.append(polygonOverlay);
            } else {
                ToolOverlay rectOverlay;
                rectOverlay.type = ToolOverlayType::Rect;
                rectOverlay.label = QStringLiteral("roi");
                rectOverlay.rect = imageRect;
                overlays.append(rectOverlay);
            }

            ToolOverlay textOverlay;
            textOverlay.type = ToolOverlayType::Text;
            textOverlay.label = QStringLiteral("roi_number");
            textOverlay.text = QString::number(markIndex + 1);
            textOverlay.p1 = imageRect.topLeft();
            overlays.append(textOverlay);
            addNumberLabel(markIndex, normalizedBoundingRect(mark));
        }
        if (!overlays.isEmpty())
            previewHelper->setToolOverlays(overlays);
        if (!marks.isEmpty()) {
            state->activeEditImage = state->currentImage;
            state->activeEditClass = state->currentClass;
            state->activeEditRoi = marks.size() - 1;
            const TrainingRoiMark mark = marks.last();
            if (mark.type == QStringLiteral("polygon")) {
                previewHelper->setPolygonRoiNormalized(mark.polygon);
            } else {
                previewHelper->setRoiRectNormalized(mark.rect);
            }
        }
        previewHelper->fitToView();
        editStatusLabel->setText(message);
    };

    *returnToImagePage = [state, showCurrentImage](const QString &message) {
        state->previewClass = -1;
        state->selectedPreviewImage = -1;
        state->selectedPreviewRoi = -1;
        (*showCurrentImage)(message);
    };

    *renderPreviewPage = [=](const int classIndex) {
        if (classIndex < 0 || classIndex >= state->classes.size())
            return;

        state->currentClass = classIndex;
        state->previewClass = classIndex;
        state->selectedPreviewImage = -1;
        state->selectedPreviewRoi = -1;
        previewHelper->setRoiDrawingEnabled(false);
        previewHelper->setPolygonDrawingEnabled(false);
        previewStack->setCurrentWidget(previewPage);
        clearRoiNumberLabels();

        while (QLayoutItem *item = roiPreviewListLayout->takeAt(0)) {
            if (QWidget *widget = item->widget()) {
                widget->hide();
                widget->setObjectName(QString());
                widget->deleteLater();
            }
            delete item;
        }

        const int targetCount = classTargetCount(classIndex);
        previewPageTitle->setText(QObject::tr("%1 | 已标注目标：%2")
                                  .arg(state->classes.value(classIndex))
                                  .arg(targetCount));
        previewEmptyLabel->setVisible(targetCount == 0);
        roiPreviewScrollArea->setVisible(targetCount > 0);

        for (int imageIndex = 0; imageIndex < state->images.size(); ++imageIndex) {
            TrainingImageState &imageState = state->images[imageIndex];
            const QVector<TrainingRoiMark> marks = imageState.marksByClass.value(classIndex);
            for (int roiIndex = 0; roiIndex < marks.size(); ++roiIndex) {
                const TrainingRoiMark mark = marks.at(roiIndex);
                TrainingRoiPreviewCard *card = new TrainingRoiPreviewCard(roiPreviewListWidget);
                card->setObjectName(QStringLiteral("registeredTrainingRoiPreviewCard_%1_%2_%3")
                                    .arg(classIndex)
                                    .arg(imageIndex)
                                    .arg(roiIndex));
                card->setProperty("panelRole", QStringLiteral("roiPreviewCard"));
                QVBoxLayout *cardLayout = new QVBoxLayout(card);
                cardLayout->setContentsMargins(8, 8, 8, 8);
                cardLayout->setSpacing(6);

                QHBoxLayout *cardHeader = new QHBoxLayout;
                cardHeader->setContentsMargins(0, 0, 0, 0);
                cardHeader->setSpacing(6);
                QLabel *caption = new QLabel(QObject::tr("%1 #%2").arg(imageState.name).arg(roiIndex + 1), card);
                caption->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
                QToolButton *deleteRoiButton = new QToolButton(card);
                deleteRoiButton->setObjectName(QStringLiteral("registeredTrainingDeleteRoiButton_%1_%2_%3")
                                               .arg(classIndex)
                                               .arg(imageIndex)
                                               .arg(roiIndex));
                deleteRoiButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
                deleteRoiButton->setIconSize(QSize(20, 20));
                deleteRoiButton->setToolTip(QObject::tr("删除当前 ROI"));
                deleteRoiButton->setStatusTip(QObject::tr("删除当前 ROI"));
                deleteRoiButton->setWhatsThis(QObject::tr("删除当前 ROI"));
                deleteRoiButton->setMinimumSize(34, 34);
                cardHeader->addWidget(caption, 1);
                cardHeader->addWidget(deleteRoiButton);
                QLabel *imageLabel = new QLabel(card);
                imageLabel->setAlignment(Qt::AlignCenter);
                imageLabel->setMinimumSize(220, 120);
                imageLabel->setStyleSheet(QStringLiteral("background:#f8fafc;border:2px solid #0f172a;"));
                const QRect cropRect = imageCropRect(imageState.image, mark);
                if (!cropRect.isEmpty()) {
                    imageLabel->setPixmap(QPixmap::fromImage(imageState.image.copy(cropRect).scaled(
                                             240, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                }
                cardLayout->addLayout(cardHeader);
                cardLayout->addWidget(imageLabel);
                roiPreviewListLayout->addWidget(card);

                card->selectHandler = [state, roiPreviewListWidget, card, imageIndex, roiIndex]() {
                    const QList<QFrame *> cards = roiPreviewListWidget->findChildren<QFrame *>();
                    for (QFrame *other : cards) {
                        if (other->property("panelRole").toString() == QStringLiteral("roiPreviewCard")) {
                            other->setProperty("selected", false);
                            other->style()->unpolish(other);
                            other->style()->polish(other);
                        }
                    }
                    state->selectedPreviewImage = imageIndex;
                    state->selectedPreviewRoi = roiIndex;
                    card->setProperty("selected", true);
                    card->style()->unpolish(card);
                    card->style()->polish(card);
                };
                card->deleteHandler = [=]() {
                    (*deleteRoi)(imageIndex, classIndex, roiIndex, QObject::tr("已删除当前 ROI"));
                };
                QObject::connect(deleteRoiButton, &QToolButton::clicked, card, [=]() {
                    (*deleteRoi)(imageIndex, classIndex, roiIndex, QObject::tr("已删除当前 ROI"));
                });
                auto openPreviewRoiMenu = [=](const QPoint &globalPos) {
                    if (card->selectHandler)
                        card->selectHandler();
                    QMenu *menu = new QMenu(card);
                    menu->setObjectName(QStringLiteral("registeredTrainingPreviewRoiContextMenu"));
                    menu->setAttribute(Qt::WA_DeleteOnClose);
                    QAction *deleteAction = menu->addAction(QObject::tr("删除当前 ROI"));
                    QObject::connect(deleteAction, &QAction::triggered, menu, [=]() {
                        menu->hide();
                        menu->close();
                        menu->deleteLater();
                        (*deleteRoi)(imageIndex, classIndex, roiIndex, QObject::tr("已删除当前 ROI"));
                    });
                    menu->popup(globalPos);
                };
                card->setContextMenuPolicy(Qt::CustomContextMenu);
                imageLabel->setContextMenuPolicy(Qt::CustomContextMenu);
                caption->setContextMenuPolicy(Qt::CustomContextMenu);
                QObject::connect(card, &QWidget::customContextMenuRequested, card, [=](const QPoint &pos) {
                    openPreviewRoiMenu(card->mapToGlobal(pos));
                });
                QObject::connect(imageLabel, &QWidget::customContextMenuRequested, card, [=](const QPoint &pos) {
                    openPreviewRoiMenu(imageLabel->mapToGlobal(pos));
                });
                QObject::connect(caption, &QWidget::customContextMenuRequested, card, [=](const QPoint &pos) {
                    openPreviewRoiMenu(caption->mapToGlobal(pos));
                });
            }
        }
        roiPreviewListLayout->addStretch(1);
        editStatusLabel->setText(QObject::tr("正在预览类别 ROI：%1").arg(state->classes.value(classIndex)));
    };

    *deleteRoi = [=](const int imageIndex, const int classIndex, const int roiIndex, const QString &message) {
        closeRoiDeleteMenus();
        if (imageIndex < 0 || imageIndex >= state->images.size())
            return;
        if (classIndex < 0 || classIndex >= state->classes.size())
            return;
        QVector<TrainingRoiMark> &marks = state->images[imageIndex].marksByClass[classIndex];
        if (roiIndex < 0 || roiIndex >= marks.size())
            return;
        marks.removeAt(roiIndex);
        if (marks.isEmpty())
            state->images[imageIndex].marksByClass.remove(classIndex);
        state->selectedPreviewImage = -1;
        state->selectedPreviewRoi = -1;
        state->selectedRoiImage = -1;
        state->selectedRoiClass = -1;
        state->selectedRoiIndex = -1;
        state->activeEditImage = -1;
        state->activeEditClass = -1;
        state->activeEditRoi = -1;
        refreshStatus();
        (*refreshThumbnails)();
        (*refreshClassList)();
        if (state->previewClass >= 0)
            (*renderPreviewPage)(state->previewClass);
        else
            (*showCurrentImage)(message);
        editStatusLabel->setText(message);
    };

    *deleteImage = [=](const int index) {
        if (index < 0 || index >= state->images.size())
            return;
        const QString deletedName = state->images.at(index).name;
        state->images.removeAt(index);
        state->activeEditImage = -1;
        state->activeEditClass = -1;
        state->activeEditRoi = -1;
        state->previewClass = -1;
        state->selectedPreviewImage = -1;
        state->selectedPreviewRoi = -1;
        state->selectedRoiImage = -1;
        state->selectedRoiClass = -1;
        state->selectedRoiIndex = -1;
        if (state->images.isEmpty()) {
            state->currentImage = -1;
            refreshStatus();
            (*refreshThumbnails)();
            (*refreshClassList)();
            (*showCurrentImage)(QObject::tr("请先添加注册图"));
            return;
        }
        if (state->currentImage == index)
            state->currentImage = qMin(index, state->images.size() - 1);
        else if (state->currentImage > index)
            --state->currentImage;
        if (!imageMatchesFilter(state->currentImage))
            state->currentImage = firstVisibleImage();
        refreshStatus();
        (*refreshThumbnails)();
        (*refreshClassList)();
        if (state->currentImage < 0)
            (*showCurrentImage)(QObject::tr("当前筛选无注册图"));
        else
            (*showCurrentImage)(QObject::tr("已删除注册图：%1").arg(deletedName));
    };

    auto roiIndexAtViewPos = [=](const QPoint &viewPos) {
        TrainingImageState *image = currentImageState();
        if (!image || image->image.isNull())
            return -1;
        const QPointF imagePoint = previewHelper->viewToImage(viewPos);
        if (imagePoint.x() < 0.0 || imagePoint.y() < 0.0 ||
            imagePoint.x() > image->image.width() || imagePoint.y() > image->image.height())
            return -1;
        const QPointF normalizedPoint(imagePoint.x() / qMax(1, image->image.width()),
                                      imagePoint.y() / qMax(1, image->image.height()));
        const QVector<TrainingRoiMark> marks = image->marksByClass.value(state->currentClass);
        for (int index = marks.size() - 1; index >= 0; --index) {
            if (markContainsNormalizedPoint(marks.at(index), normalizedPoint))
                return index;
        }
        return -1;
    };

    TrainingRoiContextMenuFilter *roiContextMenuFilter = new TrainingRoiContextMenuFilter(view);
    roiContextMenuFilter->openMenu = [=](const QPoint &viewPos, const QPoint &globalPos) {
        if (state->currentImage < 0 || state->currentImage >= state->images.size()) {
            editStatusLabel->setText(QObject::tr("请先添加注册图"));
            return;
        }
        if (state->previewClass >= 0) {
            editStatusLabel->setText(QObject::tr("请先返回注册图大图"));
            return;
        }
        const int roiIndex = roiIndexAtViewPos(viewPos);
        if (roiIndex < 0) {
            editStatusLabel->setText(QObject::tr("请右键目标 ROI"));
            return;
        }
        state->selectedRoiImage = state->currentImage;
        state->selectedRoiClass = state->currentClass;
        state->selectedRoiIndex = roiIndex;
        QMenu *menu = new QMenu(view);
        menu->setObjectName(QStringLiteral("registeredTrainingRoiContextMenu"));
        menu->setAttribute(Qt::WA_DeleteOnClose);
        QAction *deleteAction = menu->addAction(QObject::tr("删除当前 ROI"));
        QObject::connect(deleteAction, &QAction::triggered, menu, [=]() {
            menu->hide();
            menu->close();
            menu->deleteLater();
            (*deleteRoi)(state->selectedRoiImage,
                         state->selectedRoiClass,
                         state->selectedRoiIndex,
                         QObject::tr("已删除当前 ROI"));
        });
        menu->popup(globalPos);
        editStatusLabel->setText(QObject::tr("已选中 ROI %1").arg(roiIndex + 1));
    };
    view->viewport()->installEventFilter(roiContextMenuFilter);

    connect(previewCloseButton, &QToolButton::clicked, this, [state, returnToImagePage]() {
        (*returnToImagePage)(QObject::tr("当前注册图：%1")
                             .arg(state->currentImage >= 0 && state->currentImage < state->images.size()
                                  ? state->images.at(state->currentImage).name
                                  : QObject::tr("无")));
    });

    *refreshClassList = [=]() {
        while (QLayoutItem *item = classListLayout->takeAt(0)) {
            if (QWidget *widget = item->widget())
                widget->deleteLater();
            delete item;
        }

        classCountLabel->setText(QObject::tr("分类列表(%1)").arg(state->classes.size()));
        for (int classIndex = 0; classIndex < state->classes.size(); ++classIndex) {
            ClickableClassRow *rowFrame = new ClickableClassRow(classListWidget);
            rowFrame->setObjectName(QStringLiteral("registeredTrainingClassRow_%1").arg(classIndex));
            rowFrame->setProperty("panelRole", QStringLiteral("classRow"));
            if (classIndex == state->currentClass)
                rowFrame->setProperty("selected", true);
            QHBoxLayout *rowLayout = new QHBoxLayout(rowFrame);
            rowLayout->setContentsMargins(12, 8, 12, 8);
            QLabel *nameLabel = new QLabel(state->classes.at(classIndex), rowFrame);
            QLabel *countLabel = new QLabel(QObject::tr("%1 / %2")
                                            .arg(classTargetCount(classIndex))
                                            .arg(classImageCount(classIndex)),
                                            rowFrame);
            QToolButton *renameButton = rowIconButton(
                        rowFrame,
                        QStringLiteral("registeredTrainingRenameClassButton_%1").arg(classIndex),
                        QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView),
                        QObject::tr("重命名"));
            QToolButton *previewButton = rowIconButton(
                        rowFrame,
                        QStringLiteral("registeredTrainingPreviewClassButton_%1").arg(classIndex),
                        QApplication::style()->standardIcon(QStyle::SP_FileDialogContentsView),
                        QObject::tr("预览类别 ROI"));
            QToolButton *deleteButton = rowIconButton(
                        rowFrame,
                        QStringLiteral("registeredTrainingDeleteClassMarkButton_%1").arg(classIndex),
                        QApplication::style()->standardIcon(QStyle::SP_TrashIcon),
                        QObject::tr("删除类别"));

            rowLayout->addWidget(nameLabel, 2);
            rowLayout->addWidget(countLabel, 1);
            rowLayout->addWidget(renameButton);
            rowLayout->addWidget(previewButton);
            rowLayout->addWidget(deleteButton);
            classListLayout->addWidget(rowFrame);

            auto selectClassRow = [=]() {
                if (classIndex < 0 || classIndex >= state->classes.size())
                    return;
                state->currentClass = classIndex;
                previewHelper->setRoiDrawingEnabled(false);
                previewHelper->setPolygonDrawingEnabled(false);
                if (state->previewClass >= 0) {
                    (*returnToImagePage)(QObject::tr("当前类别：%1").arg(state->classes.value(classIndex)));
                } else {
                    (*showCurrentImage)(QObject::tr("当前类别：%1").arg(state->classes.value(classIndex)));
                }
                (*refreshClassList)();
            };
            rowFrame->clickHandler = selectClassRow;

            connect(renameButton, &QToolButton::clicked, this, [state, classIndex, editStatusLabel, refreshClassList, returnToImagePage]() {
                bool accepted = false;
                const QString currentName = state->classes.value(classIndex);
                const QString name = QInputDialog::getText(nullptr,
                                                           QObject::tr("重命名类别"),
                                                           QObject::tr("类别名称"),
                                                           QLineEdit::Normal,
                                                           currentName,
                                                           &accepted).trimmed();
                if (!accepted || name.isEmpty()) {
                    editStatusLabel->setText(QObject::tr("已取消重命名"));
                    return;
                }
                state->currentClass = classIndex;
                state->classes[classIndex] = name;
                (*refreshClassList)();
                if (state->previewClass >= 0) {
                    (*returnToImagePage)(QObject::tr("已重命名类别：%1").arg(name));
                } else {
                    editStatusLabel->setText(QObject::tr("已重命名类别：%1").arg(name));
                }
            });
            connect(previewButton, &QToolButton::clicked, this, [state, classIndex, renderPreviewPage, returnToImagePage, refreshClassList]() {
                if (state->previewClass == classIndex) {
                    (*returnToImagePage)(QObject::tr("当前注册图：%1")
                                         .arg(state->currentImage >= 0 && state->currentImage < state->images.size()
                                              ? state->images.at(state->currentImage).name
                                              : QObject::tr("无")));
                } else {
                    (*renderPreviewPage)(classIndex);
                }
                (*refreshClassList)();
            });
            connect(deleteButton, &QToolButton::clicked, this, [state, classIndex, previewHelper, editStatusLabel, refreshStatus, refreshThumbnails, refreshClassList, returnToImagePage]() {
                const QString className = state->classes.value(classIndex);
                if (state->classes.size() > 1) {
                    state->classes.removeAt(classIndex);
                    for (TrainingImageState &imageState : state->images) {
                        QMap<int, QVector<TrainingRoiMark>> reindexed;
                        for (auto it = imageState.marksByClass.cbegin(); it != imageState.marksByClass.cend(); ++it) {
                            if (it.key() < classIndex) {
                                reindexed.insert(it.key(), it.value());
                            } else if (it.key() > classIndex) {
                                reindexed.insert(it.key() - 1, it.value());
                            }
                        }
                        imageState.marksByClass = reindexed;
                    }
                    state->currentClass = qMin(classIndex, state->classes.size() - 1);
                    editStatusLabel->setText(QObject::tr("已删除类别：%1").arg(className));
                } else {
                    for (TrainingImageState &imageState : state->images)
                        imageState.marksByClass.remove(0);
                    state->currentClass = 0;
                    editStatusLabel->setText(QObject::tr("已清除默认类别 ROI"));
                }
                state->previewClass = -1;
                previewHelper->clearRoi();
                previewHelper->clearPolygonRoi();
                previewHelper->clearToolOverlays();
                refreshStatus();
                (*refreshThumbnails)();
                (*refreshClassList)();
                (*returnToImagePage)(editStatusLabel->text());
            });
        }
        classListLayout->addStretch(1);
    };

    auto storeCurrentMark = [state, currentImageState, refreshStatus, refreshThumbnails, refreshClassList](const TrainingRoiMark &mark,
                                                                                                            const bool replaceActive) {
        TrainingImageState *imageState = currentImageState();
        if (!imageState)
            return;
        QVector<TrainingRoiMark> &marks = imageState->marksByClass[state->currentClass];
        if (replaceActive &&
            state->activeEditImage == state->currentImage &&
            state->activeEditClass == state->currentClass &&
            state->activeEditRoi >= 0 &&
            state->activeEditRoi < marks.size()) {
            marks[state->activeEditRoi] = mark;
        } else {
            marks.append(mark);
            state->activeEditImage = state->currentImage;
            state->activeEditClass = state->currentClass;
            state->activeEditRoi = marks.size() - 1;
        }
        refreshStatus();
        (*refreshThumbnails)();
        (*refreshClassList)();
    };

    auto appendTrainingImage = [state, refreshStatus, refreshThumbnails, refreshClassList, showCurrentImage, editStatusLabel](const QImage &image,
                                                                                                                              const QString &name,
                                                                                                                              const QString &sourceText) {
        if (image.isNull()) {
            editStatusLabel->setText(QObject::tr("图像为空，无法显示"));
            return;
        }
        TrainingImageState imageState;
        imageState.image = image;
        imageState.name = name.trimmed().isEmpty()
                ? QObject::tr("注册图%1").arg(state->images.size() + 1)
                : name;
        state->images.append(imageState);
        state->currentImage = state->images.size() - 1;
        refreshStatus();
        (*refreshThumbnails)();
        (*refreshClassList)();
        (*showCurrentImage)(sourceText);
    };

    auto setRoiMode = [state, previewHelper, fullButton, rectButton, polygonButton, editStatusLabel, storeCurrentMark, showCurrentImage](QToolButton *button) {
        const bool turnOn = button->isChecked();
        fullButton->setChecked(false);
        rectButton->setChecked(false);
        polygonButton->setChecked(false);
        previewHelper->setRoiDrawingEnabled(false);
        previewHelper->setPolygonDrawingEnabled(false);

        if (state->currentImage < 0) {
            editStatusLabel->setText(QObject::tr("请先添加注册图"));
            return;
        }

        if (!turnOn) {
            editStatusLabel->setText(QObject::tr("已退出 ROI 绘制"));
            return;
        }

        button->setChecked(true);
        state->activeEditImage = -1;
        state->activeEditClass = -1;
        state->activeEditRoi = -1;
        if (button == fullButton) {
            previewHelper->clearPolygonRoi();
            previewHelper->setRoiRectNormalized(QRectF(0.0, 0.0, 1.0, 1.0));
            TrainingRoiMark mark;
            mark.type = QStringLiteral("full");
            mark.rect = QRectF(0.0, 0.0, 1.0, 1.0);
            storeCurrentMark(mark, false);
            (*showCurrentImage)(QObject::tr("已选择全屏 ROI"));
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

    connect(cameraButton, &QPushButton::clicked, this, [appendTrainingImage, editStatusLabel, state]() {
        const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
        if (!frame.empty()) {
            const QImage image = MatImageConverter::matToDisplayImage(
                        frame, QStringLiteral("RegisteredClassificationTrainingDialog"));
            appendTrainingImage(image,
                                QObject::tr("相机抓图%1").arg(state->images.size() + 1),
                                QObject::tr("已从当前相机帧抓图"));
            return;
        }

        cv::Mat referenceFrame = ReferenceImageProvider::instance().referenceFrame();
        QImage referenceImage = ReferenceImageProvider::instance().referenceImage();
        if (referenceImage.isNull() && !referenceFrame.empty()) {
            referenceImage = MatImageConverter::matToDisplayImage(
                        referenceFrame, QStringLiteral("RegisteredClassificationTrainingDialogReference"));
        }
        if (referenceImage.isNull()) {
            editStatusLabel->setText(QObject::tr("当前图像帧为空，且未设置基准图"));
            return;
        }
        appendTrainingImage(referenceImage,
                            QObject::tr("基准图%1").arg(state->images.size() + 1),
                            QObject::tr("当前图像帧为空，已获取基准图"));
    });

    connect(externalImportButton, &QPushButton::clicked, this, [this, appendTrainingImage, editStatusLabel]() {
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
        const QString fileName = QFileInfo(path).fileName();
        appendTrainingImage(image, fileName, tr("已导入图片：%1").arg(fileName));
    });

    connect(thumbnailList, &QListWidget::currentRowChanged, this, [state, showCurrentImage](int row) {
        if (row < 0 || row >= state->visibleImageIndexes.size())
            return;
        const int imageIndex = state->visibleImageIndexes.at(row);
        if (imageIndex < 0 || imageIndex >= state->images.size())
            return;
        state->currentImage = imageIndex;
        (*showCurrentImage)(QObject::tr("当前注册图：%1").arg(state->images.at(imageIndex).name));
    });

    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
        state->thumbnailFilter = filterCombo->itemData(index).toString();
        if (!imageMatchesFilter(state->currentImage))
            state->currentImage = firstVisibleImage();
        (*refreshThumbnails)();
        if (state->currentImage < 0)
            (*showCurrentImage)(QObject::tr("当前筛选无注册图"));
        else
            (*showCurrentImage)(QObject::tr("当前注册图：%1").arg(state->images.at(state->currentImage).name));
    });

    connect(deleteAllImagesButton, &QToolButton::clicked, this, [=]() {
        if (state->images.isEmpty()) {
            editStatusLabel->setText(QObject::tr("请先添加注册图"));
            return;
        }
        const QMessageBox::StandardButton answer = QMessageBox::question(
                    this,
                    tr("删除全部注册图"),
                    tr("确认删除全部注册图及其 ROI 标注？"),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
        state->images.clear();
        state->currentImage = -1;
        state->previewClass = -1;
        state->selectedPreviewImage = -1;
        state->selectedPreviewRoi = -1;
        state->activeEditImage = -1;
        state->activeEditClass = -1;
        state->activeEditRoi = -1;
        refreshStatus();
        (*refreshThumbnails)();
        (*refreshClassList)();
        (*showCurrentImage)(QObject::tr("请先添加注册图"));
    });

    auto switchImageByOffset = [state, thumbnailList, editStatusLabel, showCurrentImage](const int offset) {
        if (state->images.isEmpty()) {
            editStatusLabel->setText(QObject::tr("请先添加注册图"));
            return;
        }
        const int current = state->currentImage < 0 ? 0 : state->currentImage;
        const int count = state->images.size();
        const int next = (current + offset + count) % count;
        const int visibleRow = state->visibleImageIndexes.indexOf(next);
        if (visibleRow >= 0) {
            thumbnailList->setCurrentRow(visibleRow);
            return;
        }
        state->currentImage = next;
        (*showCurrentImage)(QObject::tr("当前注册图：%1").arg(state->images.at(next).name));
    };

    connect(previousImageButton, &QToolButton::clicked, this, [switchImageByOffset]() {
        switchImageByOffset(-1);
    });
    connect(nextImageButton, &QToolButton::clicked, this, [switchImageByOffset]() {
        switchImageByOffset(1);
    });

    connect(createClassButton, &QPushButton::clicked, this, [state, refreshClassList, editStatusLabel, returnToImagePage]() {
        state->classes.append(QObject::tr("Classification%1").arg(state->classes.size()));
        state->currentClass = state->classes.size() - 1;
        (*refreshClassList)();
        if (state->previewClass >= 0) {
            (*returnToImagePage)(QObject::tr("已新建类别：%1").arg(state->classes.last()));
        } else {
            editStatusLabel->setText(QObject::tr("已新建类别：%1").arg(state->classes.last()));
        }
    });

    connect(clearAllMarksButton, &QPushButton::clicked, this, [state, previewHelper, refreshStatus, refreshThumbnails, refreshClassList, returnToImagePage]() {
        for (TrainingImageState &imageState : state->images)
            imageState.marksByClass.clear();
        previewHelper->clearRoi();
        previewHelper->clearPolygonRoi();
        previewHelper->clearToolOverlays();
        state->previewClass = -1;
        refreshStatus();
        (*refreshThumbnails)();
        (*refreshClassList)();
        (*returnToImagePage)(QObject::tr("已清除全部标注"));
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

    connect(m_trainButton, &QPushButton::clicked, this, [this]() {
        const QString suggestedName = QStringLiteral("registered_classification_model_%1")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
        const QString outputModelDir = QFileDialog::getExistingDirectory(
                    this,
                    tr("选择注册分类模型输出目录"),
                    QDir::home().filePath(suggestedName));
        if (outputModelDir.trimmed().isEmpty())
            return;
        const RegisteredClassificationTrainingResult result = trainToModelDir(outputModelDir);
        if (result.success) {
            QMessageBox::information(this,
                                     tr("注册分类训练"),
                                     tr("训练完成：%1").arg(result.modelDir));
        } else {
            QMessageBox::warning(this,
                                 tr("注册分类训练"),
                                 tr("%1 | %2").arg(result.status, result.message));
        }
    });

    connect(previewHelper, &FrameViewHelper::roiChanged, this, [state, previewHelper, rectButton, storeCurrentMark, showCurrentImage](const QRectF &roi) {
        rectButton->setChecked(true);
        previewHelper->setRoiRectNormalized(roi);
        TrainingRoiMark mark;
        mark.type = QStringLiteral("rect");
        mark.rect = roi;
        storeCurrentMark(mark, false);
        (*showCurrentImage)(QObject::tr("矩形 ROI：x=%1 y=%2 w=%3 h=%4")
                            .arg(roi.x(), 0, 'f', 3)
                            .arg(roi.y(), 0, 'f', 3)
                            .arg(roi.width(), 0, 'f', 3)
                            .arg(roi.height(), 0, 'f', 3));
    });
    connect(previewHelper, &FrameViewHelper::roiSelectionRejected, this, [editStatusLabel](const QRectF &) {
        editStatusLabel->setText(QObject::tr("矩形 ROI 无效，请重新绘制"));
    });
    connect(previewHelper, &FrameViewHelper::polygonChanged, this, [previewHelper, polygonButton, storeCurrentMark, showCurrentImage](const QVector<QPointF> &points) {
        polygonButton->setChecked(true);
        previewHelper->setPolygonRoiNormalized(points);
        TrainingRoiMark mark;
        mark.type = QStringLiteral("polygon");
        mark.polygon = points;
        storeCurrentMark(mark, true);
        (*showCurrentImage)(QObject::tr("多边形 ROI：%1 个点").arg(points.size()));
    });
    connect(previewHelper, &FrameViewHelper::polygonSelectionRejected, this, [editStatusLabel](int pointCount) {
        editStatusLabel->setText(QObject::tr("多边形 ROI 至少需要 3 个点，当前 %1 个点").arg(pointCount));
    });

    refreshStatus();
    (*refreshThumbnails)();
    (*refreshClassList)();

    setStyleSheet(QStringLiteral(
        "QDialog{background:#ffffff;color:#0f172a;font-size:18px;}"
        "QFrame[panelRole=\"previewPanel\"],QFrame[panelRole=\"trainingCard\"]{background:#ffffff;border:3px solid #60a5fa;border-radius:8px;}"
        "QFrame[panelRole=\"rightPanel\"]{background:#ffffff;border:0;}"
        "QFrame[panelRole=\"previewToolbar\"]{background:#ffffff;border-bottom:4px solid #ff7a00;border-top-left-radius:8px;border-top-right-radius:8px;}"
        "QFrame[panelRole=\"statusBar\"]{background:#fff7ed;border-top:3px solid #ffb366;color:#7c2d12;}"
        "QFrame[panelRole=\"thumbnailToolbar\"]{background:#27303c;border-top:3px solid #4094ff;border-radius:0;}"
        "QFrame[panelRole=\"thumbnailCard\"]{background:#344155;border:2px solid transparent;border-radius:4px;}"
        "QFrame[panelRole=\"thumbnailCard\"]:hover{border-color:#ff7a00;}"
        "QFrame[panelRole=\"thumbnailImageFrame\"]{background:#1f2937;border:0;border-radius:2px;}"
        "QFrame[panelRole=\"classHeader\"]{background:#f1f5f9;border:0;border-radius:4px;}"
        "QWidget[panelRole=\"roiPreviewPage\"]{background:#2d333f;border:0;}"
        "QFrame[panelRole=\"roiPreviewCard\"]{background:#f8fafc;border:2px solid #cbd5e1;border-radius:6px;}"
        "QFrame[panelRole=\"roiPreviewCard\"][selected=\"true\"]{background:#eff6ff;border-color:#2563eb;}"
        "QWidget[panelRole=\"classList\"]{background:#ffffff;color:#0f172a;}"
        "QFrame[panelRole=\"classRow\"]{background:#f8fafc;border:2px solid #dbeafe;border-radius:6px;}"
        "QFrame[panelRole=\"classRow\"][selected=\"true\"]{background:#dcfce7;border-color:#22c55e;}"
        "QGraphicsView[panelRole=\"trainingCanvas\"]{border:0;background:#05070a;}"
        "QLabel{background:transparent;font-size:18px;color:#0f172a;font-weight:600;}"
        "QLabel[role=\"windowTitle\"]{font-size:28px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"cardTitle\"]{font-size:23px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"previewPageTitle\"]{font-size:26px;font-weight:800;color:#ffffff;}"
        "QLabel[role=\"previewEmpty\"]{font-size:22px;font-weight:700;color:#e2e8f0;}"
        "QLabel[role=\"stateLabel\"]{font-size:24px;font-weight:800;color:#c2410c;}"
        "QLabel[role=\"thumbnailCaption\"]{color:#ffffff;font-size:15px;font-weight:800;}"
        "QComboBox[role=\"filterBox\"]{background:#2d333f;border:2px solid #94a3b8;border-radius:3px;color:#ffffff;padding:8px 38px 8px 12px;font-size:18px;font-weight:700;min-width:150px;}"
        "QComboBox[role=\"filterBox\"]:hover{border-color:#cbd5e1;background:#343b49;}"
        "QComboBox[role=\"filterBox\"]::drop-down{subcontrol-origin:padding;subcontrol-position:top right;width:34px;border-left:2px solid #94a3b8;background:#2d333f;}"
        "QComboBox[role=\"filterBox\"]::down-arrow{image:none;width:0;height:0;border-left:6px solid transparent;border-right:6px solid transparent;border-top:8px solid #ffffff;margin-right:10px;}"
        "QPushButton,QToolButton,QComboBox{background:#ffffff;color:#0f172a;border:2px solid #4094ff;border-radius:6px;padding:10px;font-size:18px;font-weight:700;}"
        "QPushButton:hover,QToolButton:hover{background:#e0f2fe;border-color:#0284c7;}"
        "QToolButton[role=\"thumbnailDeleteButton\"]{background:#ff7a00;color:#ffffff;border:2px solid #ff7a00;border-radius:14px;padding:0;}"
        "QToolButton[role=\"thumbnailDeleteButton\"]:hover{background:#dc2626;border-color:#dc2626;}"
        "QPushButton[actionRole=\"primary\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;font-size:20px;font-weight:800;}"
        "QPushButton:disabled{background:#f8fafc;color:#64748b;border-color:#94a3b8;}"
        "QListWidget{background:#27303c;color:#ffffff;border-top:0;font-size:15px;font-weight:700;}"
        "QListWidget::item{background:transparent;color:#ffffff;border:0;padding:0;margin:4px;}"
        "QListWidget::item:selected{background:#0ea5e9;color:#ffffff;border-color:#ff7a00;}"
        "QScrollArea{background:#ffffff;border:0;}"
        "QScrollArea > QWidget > QWidget{background:#ffffff;}"
        "QTableWidget{background:#ffffff;color:#0f172a;border:2px solid #93c5fd;gridline-color:#bfdbfe;font-size:18px;selection-background-color:#dbeafe;}"
        "QHeaderView::section{background:#dbeafe;color:#08386f;font-weight:800;font-size:18px;border:0;padding:8px;}"));
}

QJsonObject RegisteredClassificationTrainingDialog::buildTrainingRequestPreviewForTest() const
{
    QJsonObject json;
    QJsonArray classNames;
    if (m_state) {
        for (const QString &className : m_state->classes)
            classNames.append(className);
    }
    json.insert(QStringLiteral("classNames"), classNames);
    json.insert(QStringLiteral("classCount"), m_state ? m_state->classes.size() : 0);
    json.insert(QStringLiteral("imageCount"), m_state ? m_state->images.size() : 0);
    json.insert(QStringLiteral("roiCount"), trainingSampleCount());
    json.insert(QStringLiteral("sampleCount"), trainingSampleCount());
    json.insert(QStringLiteral("trainable"), hasTrainableSamples());
    json.insert(QStringLiteral("modelType"), registeredClassificationMlpModelType());
    return json;
}

RegisteredClassificationTrainingResult
RegisteredClassificationTrainingDialog::trainToModelDirForTest(const QString &outputModelDir)
{
    return trainToModelDir(outputModelDir);
}

RegisteredClassificationTrainingRequest RegisteredClassificationTrainingDialog::buildTrainingRequest(
        const QString &outputModelDir) const
{
    RegisteredClassificationTrainingRequest request;
    request.outputModelDir = outputModelDir;
    request.mlp.numHidden = 16;
    request.mlp.maxIterations = 200;
    request.mlp.randSeed = 42;
    request.thresholds.minScore = 80;
    request.thresholds.rejectScore = 60;
    request.thresholds.top2Gap = 0;

    if (!m_state)
        return request;

    for (int classIndex = 0; classIndex < m_state->classes.size(); ++classIndex) {
        RegisteredClassificationClassLabel label;
        label.id = classIndex;
        label.name = m_state->classes.value(classIndex).trimmed();
        if (label.name.isEmpty())
            label.name = tr("Classification%1").arg(classIndex);
        request.classLabels.append(label);
    }

    for (const TrainingImageState &imageState : m_state->images) {
        const cv::Mat image = qImageToBgrMat(imageState.image);
        if (image.empty())
            continue;
        for (auto it = imageState.marksByClass.cbegin(); it != imageState.marksByClass.cend(); ++it) {
            const int classIndex = it.key();
            if (classIndex < 0 || classIndex >= m_state->classes.size())
                continue;
            for (const TrainingRoiMark &mark : it.value()) {
                const QRectF roi = trainingRoiForRunner(mark);
                if (roi.width() <= 0.0 || roi.height() <= 0.0)
                    continue;
                RegisteredClassificationTrainingSample sample;
                sample.image = image.clone();
                sample.roiNormalized = roi;
                sample.classId = classIndex;
                request.samples.append(sample);
            }
        }
    }

    return request;
}

bool RegisteredClassificationTrainingDialog::hasTrainableSamples() const
{
    if (!m_state || m_state->classes.size() < 2)
        return false;

    int classesWithSamples = 0;
    for (int classIndex = 0; classIndex < m_state->classes.size(); ++classIndex) {
        bool hasSample = false;
        for (const TrainingImageState &imageState : m_state->images) {
            if (!imageState.marksByClass.value(classIndex).isEmpty()) {
                hasSample = true;
                break;
            }
        }
        if (hasSample)
            ++classesWithSamples;
    }
    return classesWithSamples >= 2;
}

int RegisteredClassificationTrainingDialog::trainingSampleCount() const
{
    if (!m_state)
        return 0;
    int count = 0;
    for (const TrainingImageState &imageState : m_state->images) {
        for (const QVector<TrainingRoiMark> &marks : imageState.marksByClass)
            count += marks.size();
    }
    return count;
}

bool RegisteredClassificationTrainingDialog::hasPolygonTrainingMarks() const
{
    if (!m_state)
        return false;
    for (const TrainingImageState &imageState : m_state->images) {
        for (const QVector<TrainingRoiMark> &marks : imageState.marksByClass) {
            for (const TrainingRoiMark &mark : marks) {
                if (mark.type == QStringLiteral("polygon"))
                    return true;
            }
        }
    }
    return false;
}

void RegisteredClassificationTrainingDialog::refreshTrainingReadiness()
{
    const bool trainable = hasTrainableSamples();
    if (m_trainButton)
        m_trainButton->setEnabled(trainable);
    if (m_trainStatusLabel) {
        m_trainStatusLabel->setText(trainable
                                    ? tr("可训练：%1 个样本").arg(trainingSampleCount())
                                    : tr("请至少为两个类别添加 ROI 样本"));
    }
}

RegisteredClassificationTrainingResult RegisteredClassificationTrainingDialog::trainToModelDir(
        const QString &outputModelDir)
{
    const bool polygonWarning = hasPolygonTrainingMarks();
    RegisteredClassificationTrainingRunner runner;
    RegisteredClassificationTrainingResult result =
            runner.train(buildTrainingRequest(outputModelDir));
    if (polygonWarning) {
        const QString warning = tr("多边形 ROI 已按外接矩形参与首版 HALCON MLP 训练。");
        result.message = result.message.trimmed().isEmpty()
                ? warning
                : QStringLiteral("%1 %2").arg(result.message, warning);
        result.payload.insert(QStringLiteral("polygonBoundingRectWarning"), warning);
    }
    if (m_trainStatusLabel) {
        m_trainStatusLabel->setText(result.success
                                    ? tr("训练完成：%1 个样本").arg(result.sampleCount)
                                    : tr("训练失败：%1").arg(result.status));
    }
    if (result.success) {
        const QString modelName = QFileInfo(result.modelDir).fileName();
        emit trainingCompleted(result.modelDir, modelName);
    }
    return result;
}
