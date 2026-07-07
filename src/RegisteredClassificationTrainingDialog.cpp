#include "RegisteredClassificationTrainingDialog.h"

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"
#include "frame/ReferenceImageProvider.h"

#include <QButtonGroup>
#include <QApplication>
#include <QComboBox>
#include <QColor>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
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

struct TrainingRoiMark
{
    QString type;
    QRectF rect;
    QVector<QPointF> polygon;
};

struct TrainingImageState
{
    QImage image;
    QString name;
    QMap<int, QVector<TrainingRoiMark>> marksByClass;
};

struct TrainingSessionState
{
    QVector<TrainingImageState> images;
    QStringList classes = QStringList() << QStringLiteral("Classification0");
    int currentImage = -1;
    int currentClass = 0;
    int previewClass = -1;
    int selectedPreviewImage = -1;
    int selectedPreviewRoi = -1;
    int activeEditImage = -1;
    int activeEditClass = -1;
    int activeEditRoi = -1;
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
        menu->setObjectName(QStringLiteral("registeredTrainingRoiContextMenu"));
        menu->setAttribute(Qt::WA_DeleteOnClose);
        QAction *deleteAction = menu->addAction(QObject::tr("删除当前 ROI"));
        QObject::connect(deleteAction, &QAction::triggered, this, [this]() {
            if (deleteHandler)
                deleteHandler();
        });
        menu->popup(event->globalPos());
        event->accept();
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
    QLabel *imageStatusLabel = new QLabel(tr("图像 0 / 标注 0 / 类别 1"), statusBar);
    statusLayout->addWidget(imageStatusLabel);
    statusLayout->addStretch(1);
    QLabel *editStatusLabel = new QLabel(tr("占位界面，训练算法未接入"), statusBar);
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

    QSharedPointer<TrainingSessionState> state(new TrainingSessionState);

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

    auto refreshStatus = [state, totalMarkCount, imageStatusLabel]() {
        imageStatusLabel->setText(QObject::tr("图像 %1 / 标注 %2 / 类别 %3")
                                  .arg(state->images.size())
                                  .arg(totalMarkCount())
                                  .arg(state->classes.size()));
    };

    QSharedPointer<std::function<void()>> refreshThumbnails(new std::function<void()>);
    QSharedPointer<std::function<void()>> refreshClassList(new std::function<void()>);
    QSharedPointer<std::function<void(const QString &)>> showCurrentImage(
                new std::function<void(const QString &)>);
    QSharedPointer<std::function<void(int)>> renderPreviewPage(new std::function<void(int)>);
    QSharedPointer<std::function<void(const QString &)>> returnToImagePage(
                new std::function<void(const QString &)>);

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

    *refreshThumbnails = [state, thumbnailList, imageHasAnyMark]() {
        QSignalBlocker blocker(thumbnailList);
        thumbnailList->clear();
        for (int index = 0; index < state->images.size(); ++index) {
            const TrainingImageState &imageState = state->images.at(index);
            const QString text = imageHasAnyMark(index)
                    ? QObject::tr("%1\n已标注").arg(imageState.name)
                    : imageState.name;
            QListWidgetItem *item = new QListWidgetItem(
                        QIcon(QPixmap::fromImage(imageState.image.scaled(96, 72,
                                                                         Qt::KeepAspectRatio,
                                                                         Qt::SmoothTransformation))),
                        text);
            item->setTextAlignment(Qt::AlignCenter);
            thumbnailList->addItem(item);
        }
        if (state->currentImage >= 0 && state->currentImage < thumbnailList->count())
            thumbnailList->setCurrentRow(state->currentImage);
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

                QLabel *imageLabel = new QLabel(card);
                imageLabel->setAlignment(Qt::AlignCenter);
                imageLabel->setMinimumSize(220, 120);
                imageLabel->setStyleSheet(QStringLiteral("background:#f8fafc;border:2px solid #0f172a;"));
                const QRect cropRect = imageCropRect(imageState.image, mark);
                if (!cropRect.isEmpty()) {
                    imageLabel->setPixmap(QPixmap::fromImage(imageState.image.copy(cropRect).scaled(
                                             240, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                }
                QLabel *caption = new QLabel(QObject::tr("%1 #%2").arg(imageState.name).arg(roiIndex + 1), card);
                caption->setAlignment(Qt::AlignCenter);
                cardLayout->addWidget(imageLabel);
                cardLayout->addWidget(caption);
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
                    if (imageIndex < 0 || imageIndex >= state->images.size())
                        return;
                    QVector<TrainingRoiMark> &mutableMarks =
                            state->images[imageIndex].marksByClass[classIndex];
                    if (roiIndex >= 0 && roiIndex < mutableMarks.size())
                        mutableMarks.removeAt(roiIndex);
                    if (mutableMarks.isEmpty())
                        state->images[imageIndex].marksByClass.remove(classIndex);
                    refreshStatus();
                    (*refreshThumbnails)();
                    (*refreshClassList)();
                    (*renderPreviewPage)(classIndex);
                    editStatusLabel->setText(QObject::tr("已删除当前 ROI"));
                };
            }
        }
        roiPreviewListLayout->addStretch(1);
        editStatusLabel->setText(QObject::tr("正在预览类别 ROI：%1").arg(state->classes.value(classIndex)));
    };

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
        if (row < 0 || row >= state->images.size())
            return;
        state->currentImage = row;
        (*showCurrentImage)(QObject::tr("当前注册图：%1").arg(state->images.at(row).name));
    });

    auto switchImageByOffset = [state, thumbnailList, editStatusLabel](const int offset) {
        if (state->images.isEmpty()) {
            editStatusLabel->setText(QObject::tr("请先添加注册图"));
            return;
        }
        const int current = state->currentImage < 0 ? 0 : state->currentImage;
        const int count = state->images.size();
        const int next = (current + offset + count) % count;
        thumbnailList->setCurrentRow(next);
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
        "QFrame[panelRole=\"classHeader\"]{background:#f1f5f9;border:0;border-radius:4px;}"
        "QWidget[panelRole=\"roiPreviewPage\"]{background:#ffffff;border:0;}"
        "QFrame[panelRole=\"roiPreviewCard\"]{background:#f8fafc;border:2px solid #cbd5e1;border-radius:6px;}"
        "QFrame[panelRole=\"roiPreviewCard\"][selected=\"true\"]{background:#eff6ff;border-color:#2563eb;}"
        "QWidget[panelRole=\"classList\"]{background:#ffffff;color:#0f172a;}"
        "QFrame[panelRole=\"classRow\"]{background:#f8fafc;border:2px solid #dbeafe;border-radius:6px;}"
        "QFrame[panelRole=\"classRow\"][selected=\"true\"]{background:#dcfce7;border-color:#22c55e;}"
        "QGraphicsView[panelRole=\"trainingCanvas\"]{border:0;background:#05070a;}"
        "QLabel{background:transparent;font-size:18px;color:#0f172a;font-weight:600;}"
        "QLabel[role=\"windowTitle\"]{font-size:28px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"cardTitle\"]{font-size:23px;font-weight:800;color:#08386f;}"
        "QLabel[role=\"previewPageTitle\"]{font-size:26px;font-weight:800;color:#1f2937;}"
        "QLabel[role=\"previewEmpty\"]{font-size:22px;font-weight:700;color:#94a3b8;}"
        "QLabel[role=\"stateLabel\"]{font-size:24px;font-weight:800;color:#c2410c;}"
        "QPushButton,QToolButton,QComboBox{background:#ffffff;color:#0f172a;border:2px solid #4094ff;border-radius:6px;padding:10px;font-size:18px;font-weight:700;}"
        "QPushButton:hover,QToolButton:hover{background:#e0f2fe;border-color:#0284c7;}"
        "QPushButton[actionRole=\"primary\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;font-size:20px;font-weight:800;}"
        "QPushButton:disabled{background:#f8fafc;color:#64748b;border-color:#94a3b8;}"
        "QListWidget{background:#27303c;color:#ffffff;border-top:3px solid #4094ff;font-size:15px;font-weight:700;}"
        "QListWidget::item{background:#344155;color:#ffffff;border:2px solid transparent;padding:6px;margin:5px;}"
        "QListWidget::item:selected{background:#0ea5e9;color:#ffffff;border-color:#ff7a00;}"
        "QScrollArea{background:#ffffff;border:0;}"
        "QScrollArea > QWidget > QWidget{background:#ffffff;}"
        "QTableWidget{background:#ffffff;color:#0f172a;border:2px solid #93c5fd;gridline-color:#bfdbfe;font-size:18px;selection-background-color:#dbeafe;}"
        "QHeaderView::section{background:#dbeafe;color:#08386f;font-weight:800;font-size:18px;border:0;padding:8px;}"));
}
