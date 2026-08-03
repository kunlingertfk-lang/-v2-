#include "RegisteredClassificationDetectionTrainingDialog.h"

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolOverlay.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QEvent>
#include <QFrame>
#include <QGraphicsView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSharedPointer>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtGlobal>

#include <opencv2/core.hpp>

#include <functional>

namespace {

struct DetectionMark
{
    QString type;
    QRectF rect;
    QVector<QPointF> polygon;
};

struct DetectionImageState
{
    QImage image;
    QString name;
    QVector<DetectionMark> marks;
};

struct DetectionTrainingState
{
    QVector<DetectionImageState> images;
    QVector<int> visibleImageIndexes;
    int currentImage = -1;
    int selectedRoiImage = -1;
    int selectedRoiIndex = -1;
    QString thumbnailFilter = QStringLiteral("all");
    bool previewOpen = false;
    bool angleEnabled = false;
    bool bestResolutionEnabled = false;
};

QSize initialDialogSize(QWidget *parent, const QSize &fallback)
{
    if (!parent)
        return fallback;
    QSize parentSize = parent->size();
    if (!parentSize.isValid() || parentSize.width() <= 0 || parentSize.height() <= 0)
        parentSize = parent->window()->size();
    if (!parentSize.isValid() || parentSize.width() <= 0 || parentSize.height() <= 0)
        return fallback;
    return QSize(qMax(900, qRound(parentSize.width() * 0.76)),
                 qMax(620, qRound(parentSize.height() * 0.76)));
}

QFrame *card(QWidget *parent, const QString &title)
{
    QFrame *frame = new QFrame(parent);
    frame->setProperty("panelRole", QStringLiteral("trainingCard"));
    QVBoxLayout *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(6);
    if (!title.trimmed().isEmpty()) {
        QLabel *titleLabel = new QLabel(title, frame);
        titleLabel->setProperty("role", QStringLiteral("cardTitle"));
        layout->addWidget(titleLabel);
    }
    return frame;
}

QPushButton *plainButton(QWidget *parent, const QString &text)
{
    QPushButton *button = new QPushButton(text, parent);
    button->setMinimumHeight(42);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

QToolButton *toolIconButton(QWidget *parent, const QString &text, const QString &tooltip)
{
    QToolButton *button = new QToolButton(parent);
    button->setText(text);
    button->setToolTip(tooltip);
    button->setMinimumSize(40, 40);
    return button;
}

QIcon roiIcon(const QString &kind)
{
    QPixmap pixmap(34, 34);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(QStringLiteral("#64748b")), 2.6));
    painter.setBrush(Qt::NoBrush);
    if (kind == QStringLiteral("polygon")) {
        QPolygonF polygon;
        polygon << QPointF(17, 5) << QPointF(28, 13) << QPointF(24, 28)
                << QPointF(8, 26) << QPointF(5, 13);
        painter.drawPolygon(polygon);
    } else {
        painter.drawRoundedRect(QRectF(7, 9, 21, 17), 2, 2);
    }
    return QIcon(pixmap);
}

QToolButton *roiButton(QWidget *parent, const QString &objectName, const QString &text, const QIcon &icon)
{
    QToolButton *button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setText(text);
    button->setToolTip(text);
    button->setIcon(icon);
    button->setIconSize(QSize(26, 26));
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setCheckable(true);
    button->setMinimumHeight(46);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return button;
}

QToolButton *rowButton(QWidget *parent, const QString &objectName, const QIcon &icon, const QString &tooltip)
{
    QToolButton *button = new QToolButton(parent);
    button->setObjectName(objectName);
    button->setIcon(icon);
    button->setIconSize(QSize(24, 24));
    button->setToolTip(tooltip);
    button->setMinimumSize(38, 38);
    return button;
}

QRectF markBoundingRect(const DetectionMark &mark)
{
    if (mark.type == QStringLiteral("polygon") && !mark.polygon.isEmpty()) {
        QPolygonF polygon;
        for (const QPointF &point : mark.polygon)
            polygon << point;
        return polygon.boundingRect().normalized();
    }
    return mark.rect.normalized();
}

QRect imageCropRect(const QImage &image, const DetectionMark &mark)
{
    if (image.isNull())
        return QRect();
    const QRectF normalized = markBoundingRect(mark).intersected(QRectF(0.0, 0.0, 1.0, 1.0));
    QRect crop(qRound(normalized.x() * image.width()),
               qRound(normalized.y() * image.height()),
               qRound(normalized.width() * image.width()),
               qRound(normalized.height() * image.height()));
    crop = crop.normalized().intersected(image.rect());
    return crop.width() > 0 && crop.height() > 0 ? crop : QRect();
}

bool markContainsNormalizedPoint(const DetectionMark &mark, const QPointF &point)
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

class RoiContextMenuFilter : public QObject
{
public:
    explicit RoiContextMenuFilter(QObject *parent = nullptr)
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

} // namespace

RegisteredClassificationDetectionTrainingDialog::RegisteredClassificationDetectionTrainingDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("注册目标检测"));
    resize(initialDialogSize(parent, QSize(1280, 760)));
    setMinimumSize(900, 620);

    QSharedPointer<DetectionTrainingState> state(new DetectionTrainingState);

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
    QLabel *canvasTitle = new QLabel(tr("注册图像"), toolbar);
    canvasTitle->setProperty("role", QStringLiteral("windowTitle"));
    toolbarLayout->addWidget(canvasTitle);
    toolbarLayout->addStretch(1);
    QToolButton *previousButton = toolIconButton(toolbar, QStringLiteral("‹"), tr("上一张注册图"));
    QToolButton *nextButton = toolIconButton(toolbar, QStringLiteral("›"), tr("下一张注册图"));
    toolbarLayout->addWidget(previousButton);
    toolbarLayout->addWidget(nextButton);
    toolbarLayout->addWidget(toolIconButton(toolbar, QStringLiteral("＋"), tr("放大")));
    toolbarLayout->addWidget(toolIconButton(toolbar, QStringLiteral("－"), tr("缩小")));
    toolbarLayout->addWidget(toolIconButton(toolbar, QStringLiteral("1:1"), tr("原始比例")));
    previewLayout->addWidget(toolbar);

    QStackedWidget *previewStack = new QStackedWidget(previewPanel);
    previewStack->setObjectName(QStringLiteral("registeredDetectionTrainingPreviewStack"));
    QWidget *imagePage = new QWidget(previewStack);
    QVBoxLayout *imagePageLayout = new QVBoxLayout(imagePage);
    imagePageLayout->setContentsMargins(0, 0, 0, 0);
    QGraphicsView *view = new QGraphicsView(imagePage);
    view->setObjectName(QStringLiteral("registeredDetectionTrainingPreviewView"));
    view->setProperty("panelRole", QStringLiteral("trainingCanvas"));
    FrameViewHelper *previewHelper = new FrameViewHelper(view, this);
    previewHelper->setObjectName(QStringLiteral("registeredDetectionTrainingPreviewHelper"));
    imagePageLayout->addWidget(view, 1);
    previewStack->addWidget(imagePage);

    QWidget *previewPage = new QWidget(previewStack);
    previewPage->setObjectName(QStringLiteral("registeredDetectionTrainingPreviewPage"));
    previewPage->setProperty("panelRole", QStringLiteral("roiPreviewPage"));
    QVBoxLayout *previewPageLayout = new QVBoxLayout(previewPage);
    previewPageLayout->setContentsMargins(24, 22, 24, 22);
    previewPageLayout->setSpacing(14);
    QHBoxLayout *previewPageHeader = new QHBoxLayout;
    QLabel *previewPageTitle = new QLabel(tr("Target | 已标注目标：0"), previewPage);
    previewPageTitle->setProperty("role", QStringLiteral("previewPageTitle"));
    QToolButton *closePreviewButton = toolIconButton(previewPage, QStringLiteral("×"), tr("关闭预览"));
    previewPageHeader->addWidget(previewPageTitle);
    previewPageHeader->addStretch(1);
    previewPageHeader->addWidget(closePreviewButton);
    previewPageLayout->addLayout(previewPageHeader);
    QLabel *previewEmptyLabel = new QLabel(tr("当前目标暂无 ROI 标注"), previewPage);
    previewEmptyLabel->setAlignment(Qt::AlignCenter);
    previewEmptyLabel->setProperty("role", QStringLiteral("previewEmpty"));
    QScrollArea *roiPreviewScroll = new QScrollArea(previewPage);
    roiPreviewScroll->setWidgetResizable(true);
    roiPreviewScroll->setFrameShape(QFrame::NoFrame);
    QWidget *roiPreviewList = new QWidget(roiPreviewScroll);
    roiPreviewList->setObjectName(QStringLiteral("registeredDetectionTrainingRoiPreviewList"));
    QVBoxLayout *roiPreviewListLayout = new QVBoxLayout(roiPreviewList);
    roiPreviewListLayout->setContentsMargins(0, 0, 0, 0);
    roiPreviewListLayout->setSpacing(12);
    roiPreviewScroll->setWidget(roiPreviewList);
    previewPageLayout->addWidget(previewEmptyLabel);
    previewPageLayout->addWidget(roiPreviewScroll, 1);
    previewStack->addWidget(previewPage);
    previewStack->setCurrentWidget(imagePage);
    previewLayout->addWidget(previewStack, 1);

    QFrame *statusBar = new QFrame(previewPanel);
    statusBar->setProperty("panelRole", QStringLiteral("statusBar"));
    QHBoxLayout *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 10, 16, 10);
    QComboBox *filterCombo = new QComboBox(statusBar);
    filterCombo->setObjectName(QStringLiteral("registeredDetectionTrainingFilterCombo"));
    filterCombo->setProperty("role", QStringLiteral("filterBox"));
    filterCombo->addItem(tr("全部"), QStringLiteral("all"));
    filterCombo->addItem(tr("标注"), QStringLiteral("marked"));
    filterCombo->addItem(tr("未标注"), QStringLiteral("unmarked"));
    filterCombo->view()->setObjectName(QStringLiteral("registeredDetectionTrainingFilterComboView"));
    filterCombo->view()->setStyleSheet(QStringLiteral(
        "QAbstractItemView{background:#2d333f;color:#ffffff;"
        "selection-background-color:#0ea5e9;selection-color:#ffffff;"
        "border:2px solid #94a3b8;font-size:18px;font-weight:700;outline:0;}"
        "QAbstractItemView::item{min-height:34px;padding:6px 10px;background:#2d333f;color:#ffffff;}"
        "QAbstractItemView::item:hover,QAbstractItemView::item:selected{background:#0ea5e9;color:#ffffff;}"));
    QLabel *currentLabel = new QLabel(tr("当前：0/0"), statusBar);
    QLabel *markCountLabel = new QLabel(tr("已标注：0"), statusBar);
    QLabel *pixelLabel = new QLabel(tr("X: --  Y: --  |  R: --  G: --  B: --"), statusBar);
    pixelLabel->setObjectName(QStringLiteral("registeredDetectionTrainingPixelLabel"));
    statusLayout->addWidget(filterCombo);
    statusLayout->addSpacing(18);
    statusLayout->addWidget(currentLabel);
    statusLayout->addStretch(1);
    statusLayout->addWidget(pixelLabel);
    statusLayout->addSpacing(18);
    statusLayout->addWidget(markCountLabel);
    previewHelper->bindPixelStatusLabel(pixelLabel);
    previewLayout->addWidget(statusBar);

    QListWidget *thumbnailList = new QListWidget(previewPanel);
    thumbnailList->setObjectName(QStringLiteral("registeredDetectionTrainingThumbnailList"));
    thumbnailList->setViewMode(QListView::IconMode);
    thumbnailList->setIconSize(QSize(96, 72));
    thumbnailList->setResizeMode(QListView::Adjust);
    thumbnailList->setMovement(QListView::Static);
    thumbnailList->setWrapping(false);
    thumbnailList->setFlow(QListView::LeftToRight);
    thumbnailList->setFixedHeight(126);
    thumbnailList->setSpacing(8);
    QFrame *thumbnailToolbar = new QFrame(previewPanel);
    thumbnailToolbar->setProperty("panelRole", QStringLiteral("thumbnailToolbar"));
    QHBoxLayout *thumbnailToolbarLayout = new QHBoxLayout(thumbnailToolbar);
    thumbnailToolbarLayout->setContentsMargins(12, 6, 12, 6);
    thumbnailToolbarLayout->addStretch(1);
    QToolButton *deleteAllImagesButton = new QToolButton(thumbnailToolbar);
    deleteAllImagesButton->setObjectName(QStringLiteral("registeredDetectionTrainingDeleteAllImagesButton"));
    deleteAllImagesButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_TrashIcon));
    deleteAllImagesButton->setIconSize(QSize(20, 20));
    deleteAllImagesButton->setToolTip(tr("删除全部注册图"));
    deleteAllImagesButton->setMinimumSize(34, 34);
    thumbnailToolbarLayout->addWidget(deleteAllImagesButton);
    previewLayout->addWidget(thumbnailToolbar);
    previewLayout->addWidget(thumbnailList);
    root->addWidget(previewPanel, 1);

    QFrame *rightPanel = new QFrame(this);
    rightPanel->setProperty("panelRole", QStringLiteral("rightPanel"));
    rightPanel->setMinimumWidth(390);
    rightPanel->setMaximumWidth(470);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    QFrame *addCard = card(rightPanel, tr("1/ 添加注册图"));
    QLabel *addTip = new QLabel(tr("尽量一张图覆盖所有目标，若需多张图，请确保图像间的特征有显著差异。"), addCard);
    addTip->setWordWrap(true);
    addTip->setProperty("role", QStringLiteral("tip"));
    qobject_cast<QVBoxLayout *>(addCard->layout())->addWidget(addTip);
    QHBoxLayout *addButtons = new QHBoxLayout;
    QPushButton *cameraButton = plainButton(addCard, tr("相机抓图"));
    QPushButton *storedButton = plainButton(addCard, tr("存图导入"));
    QPushButton *externalButton = plainButton(addCard, tr("外部导入"));
    storedButton->setToolTip(tr("存图导入暂未接入"));
    addButtons->addWidget(cameraButton);
    addButtons->addWidget(storedButton);
    addButtons->addWidget(externalButton);
    qobject_cast<QVBoxLayout *>(addCard->layout())->addLayout(addButtons);
    rightLayout->addWidget(addCard);

    QFrame *markCard = card(rightPanel, tr("2/ 标注图像"));
    QLabel *markTip = new QLabel(tr("请将图像中的目标按类别框出。"), markCard);
    markTip->setProperty("role", QStringLiteral("tip"));
    qobject_cast<QVBoxLayout *>(markCard->layout())->addWidget(markTip);
    QHBoxLayout *markButtons = new QHBoxLayout;
    QToolButton *rectButton = roiButton(markCard,
                                        QStringLiteral("registeredDetectionTrainingRectButton"),
                                        tr("矩形框选"),
                                        roiIcon(QStringLiteral("rect")));
    QToolButton *polygonButton = roiButton(markCard,
                                           QStringLiteral("registeredDetectionTrainingPolygonButton"),
                                           tr("多边形框选"),
                                           roiIcon(QStringLiteral("polygon")));
    markButtons->addWidget(rectButton);
    markButtons->addWidget(polygonButton);
    qobject_cast<QVBoxLayout *>(markCard->layout())->addLayout(markButtons);

    QFrame *separator = new QFrame(markCard);
    separator->setFrameShape(QFrame::HLine);
    qobject_cast<QVBoxLayout *>(markCard->layout())->addWidget(separator);

    QLabel *targetTitle = new QLabel(tr("目标列表"), markCard);
    targetTitle->setProperty("role", QStringLiteral("sectionTitle"));
    qobject_cast<QVBoxLayout *>(markCard->layout())->addWidget(targetTitle);
    QFrame *targetHeader = new QFrame(markCard);
    targetHeader->setProperty("panelRole", QStringLiteral("classHeader"));
    targetHeader->setMinimumHeight(42);
    QHBoxLayout *targetHeaderLayout = new QHBoxLayout(targetHeader);
    targetHeaderLayout->setContentsMargins(12, 8, 12, 8);
    targetHeaderLayout->addWidget(new QLabel(tr("标签类型"), targetHeader), 2);
    targetHeaderLayout->addWidget(new QLabel(tr("目标总数/图像总数"), targetHeader), 1);
    targetHeaderLayout->addSpacing(88);
    qobject_cast<QVBoxLayout *>(markCard->layout())->addWidget(targetHeader);
    QFrame *targetRow = new QFrame(markCard);
    targetRow->setObjectName(QStringLiteral("registeredDetectionTrainingTargetRow_0"));
    targetRow->setProperty("panelRole", QStringLiteral("classRow"));
    targetRow->setProperty("selected", true);
    targetRow->setMinimumHeight(62);
    QHBoxLayout *targetRowLayout = new QHBoxLayout(targetRow);
    targetRowLayout->setContentsMargins(12, 8, 12, 8);
    QLabel *targetNameLabel = new QLabel(tr("Target"), targetRow);
    QLabel *targetCountValueLabel = new QLabel(tr("0 / 0"), targetRow);
    QToolButton *renameTargetButton = rowButton(
                targetRow,
                QStringLiteral("registeredDetectionTrainingRenameTargetButton_0"),
                QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView),
                tr("重命名"));
    QToolButton *previewTargetButton = rowButton(
                targetRow,
                QStringLiteral("registeredDetectionTrainingPreviewTargetButton_0"),
                QApplication::style()->standardIcon(QStyle::SP_FileDialogContentsView),
                tr("预览目标 ROI"));
    targetRowLayout->addWidget(targetNameLabel, 2);
    targetRowLayout->addWidget(targetCountValueLabel, 1);
    targetRowLayout->addWidget(renameTargetButton);
    targetRowLayout->addWidget(previewTargetButton);
    qobject_cast<QVBoxLayout *>(markCard->layout())->addWidget(targetRow);
    rightLayout->addWidget(markCard);

    QFrame *optionsCard = card(rightPanel, QString());
    QLabel *angleLabel = new QLabel(tr("角度使能"), optionsCard);
    QLabel *resolutionLabel = new QLabel(tr("最优模型分辨率设置使能"), optionsCard);
    QCheckBox *angleCheckBox = new QCheckBox(optionsCard);
    QCheckBox *resolutionCheckBox = new QCheckBox(optionsCard);
    QHBoxLayout *angleRow = new QHBoxLayout;
    angleRow->setContentsMargins(0, 0, 0, 0);
    angleRow->setSpacing(8);
    angleRow->addWidget(angleLabel);
    angleRow->addStretch(1);
    angleRow->addWidget(angleCheckBox);
    QHBoxLayout *resolutionRow = new QHBoxLayout;
    resolutionRow->setContentsMargins(0, 0, 0, 0);
    resolutionRow->setSpacing(8);
    resolutionRow->addWidget(resolutionLabel);
    resolutionRow->addStretch(1);
    resolutionRow->addWidget(resolutionCheckBox);
    qobject_cast<QVBoxLayout *>(optionsCard->layout())->addLayout(angleRow);
    qobject_cast<QVBoxLayout *>(optionsCard->layout())->addLayout(resolutionRow);
    rightLayout->addWidget(optionsCard);
    rightLayout->addStretch(1);

    QHBoxLayout *bottomRow = new QHBoxLayout;
    QLabel *trainingStatusLabel = new QLabel(tr("未注册"), rightPanel);
    trainingStatusLabel->setProperty("role", QStringLiteral("stateLabel"));
    QPushButton *trainButton = new QPushButton(tr("开始训练"), rightPanel);
    trainButton->setProperty("actionRole", QStringLiteral("primary"));
    trainButton->setMinimumSize(128, 44);
    bottomRow->addWidget(trainingStatusLabel);
    bottomRow->addStretch(1);
    bottomRow->addWidget(trainButton);
    rightLayout->addLayout(bottomRow);
    root->addWidget(rightPanel);

    auto currentImage = [state]() -> DetectionImageState * {
        if (state->currentImage < 0 || state->currentImage >= state->images.size())
            return nullptr;
        return &state->images[state->currentImage];
    };
    auto totalMarks = [state]() {
        int count = 0;
        for (const DetectionImageState &image : state->images)
            count += image.marks.size();
        return count;
    };
    auto markedImageCount = [state]() {
        int count = 0;
        for (const DetectionImageState &image : state->images) {
            if (!image.marks.isEmpty())
                ++count;
        }
        return count;
    };
    auto imageMatchesFilter = [state](const int index) {
        if (index < 0 || index >= state->images.size())
            return false;
        if (state->thumbnailFilter == QStringLiteral("marked"))
            return !state->images.at(index).marks.isEmpty();
        if (state->thumbnailFilter == QStringLiteral("unmarked"))
            return state->images.at(index).marks.isEmpty();
        return true;
    };
    auto firstVisibleImage = [state, imageMatchesFilter]() {
        for (int index = 0; index < state->images.size(); ++index) {
            if (imageMatchesFilter(index))
                return index;
        }
        return -1;
    };

    auto clearRoiNumberLabels = [view]() {
        const QList<QLabel *> labels = view->findChildren<QLabel *>();
        for (QLabel *label : labels) {
            if (label && label->objectName().startsWith(QStringLiteral("registeredDetectionTrainingRoiNumberLabel_")))
                label->deleteLater();
        }
    };

    auto addRoiNumberLabel = [view](const int index, const QRectF &normalizedRect) {
        if (normalizedRect.isNull() || normalizedRect.width() <= 0.0 || normalizedRect.height() <= 0.0)
            return;
        QLabel *label = new QLabel(QString::number(index + 1), view);
        label->setObjectName(QStringLiteral("registeredDetectionTrainingRoiNumberLabel_%1").arg(index));
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

    QSharedPointer<std::function<void()>> refreshUi(new std::function<void()>);
    QSharedPointer<std::function<void(const QString &)>> showImage(new std::function<void(const QString &)>);
    QSharedPointer<std::function<void()>> showPreviewPage(new std::function<void()>);
    QSharedPointer<std::function<void(int)>> deleteImage(new std::function<void(int)>);
    QSharedPointer<std::function<void(int, int, const QString &)>> deleteRoi(
                new std::function<void(int, int, const QString &)>);

    auto closeRoiDeleteMenus = []() {
        const QList<QWidget *> widgets = QApplication::topLevelWidgets();
        for (QWidget *widget : widgets) {
            QMenu *menu = qobject_cast<QMenu *>(widget);
            if (!menu)
                continue;
            const QString name = menu->objectName();
            if (name != QStringLiteral("registeredDetectionTrainingRoiContextMenu") &&
                name != QStringLiteral("registeredDetectionTrainingPreviewRoiContextMenu"))
                continue;
            menu->hide();
            menu->close();
            menu->deleteLater();
        }
    };

    *refreshUi = [=]() {
        QSignalBlocker thumbnailBlocker(thumbnailList);
        QSignalBlocker filterBlocker(filterCombo);
        const int imageCount = state->images.size();
        const int current = state->currentImage >= 0 ? state->currentImage + 1 : 0;
        currentLabel->setText(QObject::tr("当前：%1/%2").arg(current).arg(imageCount));
        markCountLabel->setText(QObject::tr("已标注：%1").arg(totalMarks()));
        targetCountValueLabel->setText(QObject::tr("%1 / %2").arg(totalMarks()).arg(markedImageCount()));
        rectButton->setEnabled(imageCount > 0);
        polygonButton->setEnabled(imageCount > 0);
        const int filterIndex = filterCombo->findData(state->thumbnailFilter);
        if (filterIndex >= 0)
            filterCombo->setCurrentIndex(filterIndex);

        thumbnailList->clear();
        state->visibleImageIndexes.clear();
        for (int index = 0; index < state->images.size(); ++index) {
            if (!imageMatchesFilter(index))
                continue;
            state->visibleImageIndexes.append(index);
            const DetectionImageState &image = state->images.at(index);
            QListWidgetItem *item = new QListWidgetItem(thumbnailList);
            item->setData(Qt::UserRole, index);
            item->setSizeHint(QSize(132, 106));
            thumbnailList->addItem(item);
            QFrame *thumbnailCard = new QFrame(thumbnailList);
            thumbnailCard->setObjectName(QStringLiteral("registeredDetectionTrainingThumbnail_%1").arg(index));
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
            imageLabel->setPixmap(QPixmap::fromImage(image.image.scaled(108, 66,
                                                                        Qt::KeepAspectRatio,
                                                                        Qt::SmoothTransformation)));
            QToolButton *deleteImageButton = new QToolButton(imageFrame);
            deleteImageButton->setObjectName(QStringLiteral("registeredDetectionTrainingDeleteImageButton_%1").arg(index));
            deleteImageButton->setProperty("role", QStringLiteral("thumbnailDeleteButton"));
            deleteImageButton->setIcon(QApplication::style()->standardIcon(QStyle::SP_TitleBarCloseButton));
            deleteImageButton->setIconSize(QSize(18, 18));
            deleteImageButton->setToolTip(QObject::tr("删除注册图"));
            deleteImageButton->setFixedSize(28, 28);
            deleteImageButton->hide();
            imageLayout->addWidget(imageLabel, 0, 0);
            imageLayout->addWidget(deleteImageButton, 0, 0, Qt::AlignTop | Qt::AlignRight);
            QLabel *caption = new QLabel(image.marks.isEmpty() ? image.name : QObject::tr("%1 已标注").arg(image.name),
                                         thumbnailCard);
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

    *showImage = [=](const QString &message) {
        DetectionImageState *image = currentImage();
        previewStack->setCurrentWidget(imagePage);
        state->previewOpen = false;
        clearRoiNumberLabels();
        if (!image) {
            previewHelper->clear();
            previewHelper->setRoiDrawingEnabled(false);
            previewHelper->setPolygonDrawingEnabled(false);
            rectButton->setChecked(false);
            polygonButton->setChecked(false);
            trainingStatusLabel->setText(message);
            (*refreshUi)();
            return;
        }
        previewHelper->setImage(image->image);
        previewHelper->setRoiDrawingEnabled(rectButton->isChecked());
        previewHelper->setPolygonDrawingEnabled(polygonButton->isChecked());
        previewHelper->clearRoi();
        previewHelper->clearPolygonRoi();
        previewHelper->clearToolOverlays();
        QVector<ToolOverlay> overlays;
        for (int index = 0; index < image->marks.size(); ++index) {
            const DetectionMark &mark = image->marks.at(index);
            if (mark.type == QStringLiteral("polygon")) {
                ToolOverlay overlay;
                overlay.type = ToolOverlayType::Polygon;
                overlay.label = QStringLiteral("target_%1").arg(index + 1);
                for (const QPointF &point : mark.polygon) {
                    overlay.points.append(QPointF(point.x() * image->image.width(),
                                                  point.y() * image->image.height()));
                }
                overlays.append(overlay);
            } else {
                ToolOverlay overlay;
                overlay.type = ToolOverlayType::Rect;
                overlay.label = QStringLiteral("target_%1").arg(index + 1);
                const QRectF normalized = mark.rect.normalized();
                overlay.rect = QRectF(normalized.x() * image->image.width(),
                                      normalized.y() * image->image.height(),
                                      normalized.width() * image->image.width(),
                                      normalized.height() * image->image.height());
                overlays.append(overlay);
            }
            const QRectF normalizedBounds = markBoundingRect(mark).intersected(QRectF(0.0, 0.0, 1.0, 1.0));
            ToolOverlay textOverlay;
            textOverlay.type = ToolOverlayType::Text;
            textOverlay.label = QStringLiteral("target_roi_number");
            textOverlay.text = QString::number(index + 1);
            textOverlay.p1 = QPointF(normalizedBounds.x() * image->image.width(),
                                     normalizedBounds.y() * image->image.height());
            overlays.append(textOverlay);
            addRoiNumberLabel(index, normalizedBounds);
        }
        if (!overlays.isEmpty())
            previewHelper->setToolOverlays(overlays);
        previewHelper->fitToView();
        trainingStatusLabel->setText(message);
        (*refreshUi)();
    };

    *showPreviewPage = [=]() {
        state->previewOpen = true;
        previewStack->setCurrentWidget(previewPage);
        previewPageTitle->setText(QObject::tr("Target | 已标注目标：%1").arg(totalMarks()));
        while (QLayoutItem *item = roiPreviewListLayout->takeAt(0)) {
            if (QWidget *widget = item->widget()) {
                widget->hide();
                widget->setObjectName(QString());
                widget->deleteLater();
            }
            delete item;
        }
        const int count = totalMarks();
        previewEmptyLabel->setVisible(count == 0);
        roiPreviewScroll->setVisible(count > 0);
        int cardIndex = 0;
        for (int imageIndex = 0; imageIndex < state->images.size(); ++imageIndex) {
            const DetectionImageState &image = state->images.at(imageIndex);
            for (int roiIndex = 0; roiIndex < image.marks.size(); ++roiIndex) {
                const DetectionMark &mark = image.marks.at(roiIndex);
                QFrame *previewCard = new QFrame(roiPreviewList);
                previewCard->setObjectName(QStringLiteral("registeredDetectionTrainingRoiPreviewCard_%1_%2")
                                           .arg(imageIndex)
                                           .arg(roiIndex));
                previewCard->setProperty("panelRole", QStringLiteral("roiPreviewCard"));
                QVBoxLayout *cardLayout = new QVBoxLayout(previewCard);
                QHBoxLayout *cardHeader = new QHBoxLayout;
                cardHeader->setContentsMargins(0, 0, 0, 0);
                QLabel *caption = new QLabel(QObject::tr("%1 #%2").arg(image.name).arg(++cardIndex), previewCard);
                caption->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
                QToolButton *deleteRoiButton = new QToolButton(previewCard);
                deleteRoiButton->setObjectName(QStringLiteral("registeredDetectionTrainingDeleteRoiButton_%1_%2")
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
                QLabel *previewImage = new QLabel(previewCard);
                previewImage->setAlignment(Qt::AlignCenter);
                previewImage->setMinimumHeight(110);
                const QRect crop = imageCropRect(image.image, mark);
                if (!crop.isEmpty()) {
                    previewImage->setPixmap(QPixmap::fromImage(
                                                image.image.copy(crop).scaled(220, 130,
                                                                              Qt::KeepAspectRatio,
                                                                              Qt::SmoothTransformation)));
                }
                cardLayout->addLayout(cardHeader);
                cardLayout->addWidget(previewImage);
                roiPreviewListLayout->addWidget(previewCard);
                auto openPreviewRoiMenu = [=](const QPoint &globalPos) {
                    QMenu *menu = new QMenu(previewCard);
                    menu->setObjectName(QStringLiteral("registeredDetectionTrainingPreviewRoiContextMenu"));
                    menu->setAttribute(Qt::WA_DeleteOnClose);
                    QAction *deleteAction = menu->addAction(QObject::tr("删除当前 ROI"));
                    QObject::connect(deleteAction, &QAction::triggered, menu, [=]() {
                        menu->hide();
                        menu->close();
                        menu->deleteLater();
                        (*deleteRoi)(imageIndex, roiIndex, QObject::tr("已删除当前 ROI"));
                    });
                    menu->popup(globalPos);
                };
                previewCard->setContextMenuPolicy(Qt::CustomContextMenu);
                previewImage->setContextMenuPolicy(Qt::CustomContextMenu);
                caption->setContextMenuPolicy(Qt::CustomContextMenu);
                QObject::connect(previewCard, &QWidget::customContextMenuRequested, previewCard, [=](const QPoint &pos) {
                    openPreviewRoiMenu(previewCard->mapToGlobal(pos));
                });
                QObject::connect(previewImage, &QWidget::customContextMenuRequested, previewCard, [=](const QPoint &pos) {
                    openPreviewRoiMenu(previewImage->mapToGlobal(pos));
                });
                QObject::connect(caption, &QWidget::customContextMenuRequested, previewCard, [=](const QPoint &pos) {
                    openPreviewRoiMenu(caption->mapToGlobal(pos));
                });
                QObject::connect(deleteRoiButton, &QToolButton::clicked, previewCard, [=]() {
                    (*deleteRoi)(imageIndex, roiIndex, QObject::tr("已删除当前 ROI"));
                });
            }
        }
        roiPreviewListLayout->addStretch(1);
        trainingStatusLabel->setText(QObject::tr("正在预览目标 ROI：Target"));
    };
    *deleteRoi = [=](const int imageIndex, const int roiIndex, const QString &message) {
        closeRoiDeleteMenus();
        if (imageIndex < 0 || imageIndex >= state->images.size())
            return;
        if (roiIndex < 0 || roiIndex >= state->images[imageIndex].marks.size())
            return;
        state->images[imageIndex].marks.removeAt(roiIndex);
        state->selectedRoiImage = -1;
        state->selectedRoiIndex = -1;
        (*refreshUi)();
        if (state->previewOpen)
            (*showPreviewPage)();
        else
            (*showImage)(message);
        trainingStatusLabel->setText(message);
    };
    *deleteImage = [=](const int index) {
        if (index < 0 || index >= state->images.size())
            return;
        const QString deletedName = state->images.at(index).name;
        state->images.removeAt(index);
        if (state->images.isEmpty()) {
            state->currentImage = -1;
            (*showImage)(QObject::tr("请先添加注册图"));
            return;
        }
        if (state->currentImage == index)
            state->currentImage = qMin(index, state->images.size() - 1);
        else if (state->currentImage > index)
            --state->currentImage;
        if (!imageMatchesFilter(state->currentImage))
            state->currentImage = firstVisibleImage();
        if (state->currentImage < 0)
            (*showImage)(QObject::tr("当前筛选无注册图"));
        else
            (*showImage)(QObject::tr("已删除注册图：%1").arg(deletedName));
    };

    auto roiIndexAtViewPos = [=](const QPoint &viewPos) {
        DetectionImageState *image = currentImage();
        if (!image || image->image.isNull())
            return -1;
        const QPointF imagePoint = previewHelper->viewToImage(viewPos);
        if (imagePoint.x() < 0.0 || imagePoint.y() < 0.0 ||
            imagePoint.x() > image->image.width() || imagePoint.y() > image->image.height())
            return -1;
        const QPointF normalizedPoint(imagePoint.x() / qMax(1, image->image.width()),
                                      imagePoint.y() / qMax(1, image->image.height()));
        for (int index = image->marks.size() - 1; index >= 0; --index) {
            if (markContainsNormalizedPoint(image->marks.at(index), normalizedPoint))
                return index;
        }
        return -1;
    };

    RoiContextMenuFilter *roiContextMenuFilter = new RoiContextMenuFilter(view);
    roiContextMenuFilter->openMenu = [=](const QPoint &viewPos, const QPoint &globalPos) {
        if (state->currentImage < 0 || state->currentImage >= state->images.size()) {
            trainingStatusLabel->setText(QObject::tr("请先添加注册图"));
            return;
        }
        const int roiIndex = roiIndexAtViewPos(viewPos);
        if (roiIndex < 0) {
            trainingStatusLabel->setText(QObject::tr("请右键目标 ROI"));
            return;
        }
        state->selectedRoiImage = state->currentImage;
        state->selectedRoiIndex = roiIndex;
        QMenu *menu = new QMenu(view);
        menu->setObjectName(QStringLiteral("registeredDetectionTrainingRoiContextMenu"));
        menu->setAttribute(Qt::WA_DeleteOnClose);
        QAction *deleteAction = menu->addAction(QObject::tr("删除当前 ROI"));
        QObject::connect(deleteAction, &QAction::triggered, menu, [=]() {
            menu->hide();
            menu->close();
            menu->deleteLater();
            (*deleteRoi)(state->selectedRoiImage,
                         state->selectedRoiIndex,
                         QObject::tr("已删除当前 ROI"));
        });
        menu->popup(globalPos);
        trainingStatusLabel->setText(QObject::tr("已选中 ROI %1").arg(roiIndex + 1));
    };
    view->viewport()->installEventFilter(roiContextMenuFilter);

    auto appendImage = [=](const QImage &image, const QString &name, const QString &message) {
        if (image.isNull()) {
            trainingStatusLabel->setText(QObject::tr("图像为空，无法显示"));
            return;
        }
        DetectionImageState imageState;
        imageState.image = image;
        imageState.name = name.trimmed().isEmpty()
                ? QObject::tr("注册图%1").arg(state->images.size() + 1)
                : name;
        state->images.append(imageState);
        state->currentImage = state->images.size() - 1;
        (*showImage)(message);
    };

    connect(cameraButton, &QPushButton::clicked, this, [=]() {
        const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
        if (!frame.empty()) {
            appendImage(MatImageConverter::matToDisplayImage(
                            frame, QStringLiteral("RegisteredClassificationDetectionTrainingDialog")),
                        QObject::tr("相机抓图%1").arg(state->images.size() + 1),
                        QObject::tr("已从当前相机帧抓图"));
            return;
        }
        const QImage reference = ReferenceImageProvider::instance().referenceImage();
        if (!reference.isNull()) {
            appendImage(reference,
                        QObject::tr("基准图%1").arg(state->images.size() + 1),
                        QObject::tr("当前图像帧为空，已获取基准图"));
            return;
        }
        trainingStatusLabel->setText(QObject::tr("当前图像帧为空，且未设置基准图"));
    });
    connect(storedButton, &QPushButton::clicked, this, [=]() {
        trainingStatusLabel->setText(QObject::tr("存图导入暂未接入"));
    });
    connect(externalButton, &QPushButton::clicked, this, [=]() {
        const QString path = QFileDialog::getOpenFileName(
                    this,
                    tr("外部导入注册图"),
                    QString(),
                    tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.tif *.tiff)"));
        if (path.trimmed().isEmpty())
            return;
        appendImage(QImage(path), QFileInfo(path).fileName(), tr("已导入图片：%1").arg(QFileInfo(path).fileName()));
    });
    connect(thumbnailList, &QListWidget::currentRowChanged, this, [=](int row) {
        if (row < 0 || row >= state->visibleImageIndexes.size())
            return;
        const int imageIndex = state->visibleImageIndexes.at(row);
        if (imageIndex < 0 || imageIndex >= state->images.size())
            return;
        state->currentImage = imageIndex;
        (*showImage)(QObject::tr("当前注册图：%1").arg(state->images.at(imageIndex).name));
    });
    connect(filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
        state->thumbnailFilter = filterCombo->itemData(index).toString();
        if (!imageMatchesFilter(state->currentImage))
            state->currentImage = firstVisibleImage();
        if (state->currentImage < 0)
            (*showImage)(QObject::tr("当前筛选无注册图"));
        else
            (*showImage)(QObject::tr("当前注册图：%1").arg(state->images.at(state->currentImage).name));
    });
    connect(deleteAllImagesButton, &QToolButton::clicked, this, [=]() {
        if (state->images.isEmpty()) {
            trainingStatusLabel->setText(QObject::tr("请先添加注册图"));
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
        (*showImage)(QObject::tr("请先添加注册图"));
    });
    auto switchImage = [=](int offset) {
        if (state->images.isEmpty()) {
            trainingStatusLabel->setText(QObject::tr("请先添加注册图"));
            return;
        }
        const int count = state->images.size();
        state->currentImage = (qMax(0, state->currentImage) + offset + count) % count;
        (*showImage)(QObject::tr("当前注册图：%1").arg(state->images.at(state->currentImage).name));
    };
    connect(previousButton, &QToolButton::clicked, this, [=]() { switchImage(-1); });
    connect(nextButton, &QToolButton::clicked, this, [=]() { switchImage(1); });

    auto startMarkMode = [=](QToolButton *button) {
        const bool requested = button->isChecked();
        rectButton->setChecked(false);
        polygonButton->setChecked(false);
        previewHelper->setRoiDrawingEnabled(false);
        previewHelper->setPolygonDrawingEnabled(false);
        if (state->images.isEmpty()) {
            trainingStatusLabel->setText(QObject::tr("请先添加注册图"));
            return;
        }
        if (!requested) {
            previewHelper->setRoiDrawingEnabled(false);
            previewHelper->setPolygonDrawingEnabled(false);
            rectButton->setChecked(false);
            polygonButton->setChecked(false);
            trainingStatusLabel->setText(QObject::tr("已退出 ROI 绘制"));
            return;
        }
        if (button == rectButton) {
            rectButton->setChecked(true);
            polygonButton->setChecked(false);
            previewHelper->clearPolygonRoi();
            previewHelper->setRoiDrawingEnabled(true);
            previewHelper->setPolygonDrawingEnabled(false);
            trainingStatusLabel->setText(QObject::tr("当前编辑：矩形 ROI。按住左键拖拽绘制。"));
        } else {
            rectButton->setChecked(false);
            polygonButton->setChecked(true);
            previewHelper->clearRoi();
            previewHelper->setRoiDrawingEnabled(false);
            previewHelper->setPolygonDrawingEnabled(true);
            trainingStatusLabel->setText(QObject::tr("当前编辑：多边形 ROI。左键添加点，靠近首点点击自动闭合。"));
        }
    };
    connect(rectButton, &QToolButton::clicked, this, [=]() { startMarkMode(rectButton); });
    connect(polygonButton, &QToolButton::clicked, this, [=]() { startMarkMode(polygonButton); });
    connect(previewHelper, &FrameViewHelper::roiChanged, this, [=](const QRectF &roi) {
        DetectionImageState *image = currentImage();
        if (!image)
            return;
        DetectionMark mark;
        mark.type = QStringLiteral("rect");
        mark.rect = roi.normalized();
        image->marks.append(mark);
        (*showImage)(QObject::tr("已添加矩形目标 ROI"));
    });
    connect(previewHelper, &FrameViewHelper::polygonChanged, this, [=](const QVector<QPointF> &points) {
        DetectionImageState *image = currentImage();
        if (!image || points.size() < 3)
            return;
        DetectionMark mark;
        mark.type = QStringLiteral("polygon");
        mark.polygon = points;
        image->marks.append(mark);
        (*showImage)(QObject::tr("已添加多边形目标 ROI"));
    });
    connect(renameTargetButton, &QToolButton::clicked, this, [=]() {
        bool accepted = false;
        const QString name = QInputDialog::getText(this,
                                                   tr("重命名目标"),
                                                   tr("目标名称"),
                                                   QLineEdit::Normal,
                                                   targetNameLabel->text(),
                                                   &accepted).trimmed();
        if (!accepted || name.isEmpty()) {
            trainingStatusLabel->setText(tr("目标名称不能为空"));
            return;
        }
        targetNameLabel->setText(name);
        trainingStatusLabel->setText(tr("已重命名目标：%1").arg(name));
    });
    connect(previewTargetButton, &QToolButton::clicked, this, [=]() {
        if (state->previewOpen) {
            (*showImage)(QObject::tr("当前注册图：%1")
                         .arg(state->currentImage >= 0 && state->currentImage < state->images.size()
                              ? state->images.at(state->currentImage).name
                              : QObject::tr("无")));
        } else {
            (*showPreviewPage)();
        }
    });
    connect(closePreviewButton, &QToolButton::clicked, this, [=]() {
        (*showImage)(QObject::tr("已关闭目标 ROI 预览"));
    });
    connect(angleCheckBox, &QCheckBox::toggled, this, [state](bool checked) {
        state->angleEnabled = checked;
    });
    connect(resolutionCheckBox, &QCheckBox::toggled, this, [state](bool checked) {
        state->bestResolutionEnabled = checked;
    });
    connect(trainButton, &QPushButton::clicked, this, [=]() {
        if (state->images.isEmpty()) {
            trainingStatusLabel->setText(tr("请先添加注册图"));
            return;
        }
        if (totalMarks() <= 0) {
            trainingStatusLabel->setText(tr("请先标注目标"));
            return;
        }
        trainingStatusLabel->setText(tr("训练算法未接入，当前仅完成 UI 标注闭环。"));
    });

    (*refreshUi)();
    (*showImage)(tr("请先添加注册图"));

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
        "QFrame[panelRole=\"classRow\"]{background:#dcfce7;border:2px solid #22c55e;border-radius:6px;}"
        "QGraphicsView[panelRole=\"trainingCanvas\"]{border:0;background:#05070a;}"
        "QLabel{background:transparent;font-size:18px;color:#0f172a;font-weight:600;}"
        "QLabel[role=\"windowTitle\"]{font-size:28px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"cardTitle\"]{font-size:23px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"sectionTitle\"]{font-size:20px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"tip\"]{font-size:18px;color:#64748b;font-weight:600;}"
        "QComboBox[role=\"filterBox\"]{background:#2d333f;border:2px solid #94a3b8;border-radius:3px;color:#ffffff;padding:8px 38px 8px 12px;font-size:18px;font-weight:700;min-width:150px;}"
        "QComboBox[role=\"filterBox\"]:hover{border-color:#cbd5e1;background:#343b49;}"
        "QComboBox[role=\"filterBox\"]::drop-down{subcontrol-origin:padding;subcontrol-position:top right;width:34px;border-left:2px solid #94a3b8;background:#2d333f;}"
        "QComboBox[role=\"filterBox\"]::down-arrow{image:none;width:0;height:0;border-left:6px solid transparent;border-right:6px solid transparent;border-top:8px solid #ffffff;margin-right:10px;}"
        "QLabel[role=\"previewPageTitle\"]{font-size:26px;font-weight:800;color:#ffffff;}"
        "QLabel[role=\"previewEmpty\"]{font-size:22px;font-weight:700;color:#e2e8f0;}"
        "QLabel[role=\"stateLabel\"]{font-size:24px;font-weight:800;color:#c2410c;}"
        "QLabel[role=\"thumbnailCaption\"]{color:#ffffff;font-size:15px;font-weight:800;}"
        "QPushButton,QToolButton,QComboBox{background:#ffffff;color:#0f172a;border:2px solid #4094ff;border-radius:6px;padding:10px;font-size:18px;font-weight:700;}"
        "QPushButton:hover,QToolButton:hover{background:#e0f2fe;border-color:#0284c7;}"
        "QToolButton:checked{background:#fff7ed;color:#c2410c;border-color:#ff7a00;}"
        "QToolButton[role=\"thumbnailDeleteButton\"]{background:#ff7a00;color:#ffffff;border:2px solid #ff7a00;border-radius:14px;padding:0;}"
        "QToolButton[role=\"thumbnailDeleteButton\"]:hover{background:#dc2626;border-color:#dc2626;}"
        "QPushButton[actionRole=\"primary\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;font-size:20px;font-weight:800;}"
        "QPushButton:disabled,QToolButton:disabled{background:#f8fafc;color:#64748b;border-color:#94a3b8;}"
        "QListWidget{background:#27303c;color:#ffffff;border-top:0;font-size:15px;font-weight:700;}"
        "QListWidget::item{background:transparent;color:#ffffff;border:0;padding:0;margin:4px;}"
        "QListWidget::item:selected{background:#0ea5e9;color:#ffffff;border-color:#ff7a00;}"
        "QScrollArea{background:#ffffff;border:0;}"
        "QScrollArea > QWidget > QWidget{background:#ffffff;}"
        "QCheckBox{font-size:18px;font-weight:700;color:#0f172a;}"
        "QCheckBox::indicator{width:36px;height:24px;border:2px solid #94a3b8;border-radius:4px;background:#ffffff;}"
        "QCheckBox::indicator:checked{background:#22c55e;border-color:#16a34a;}"));
}
