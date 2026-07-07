#include "RegisteredClassificationTrainingDialog.h"

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/MatImageConverter.h"

#include <QButtonGroup>
#include <QApplication>
#include <QComboBox>
#include <QColor>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSharedPointer>
#include <QSize>
#include <QSizePolicy>
#include <QSignalBlocker>
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

struct TrainingMarkState
{
    QString type;
    QRectF rect;
    QVector<QPointF> polygon;
    bool hasMark = false;
};

struct TrainingImageState
{
    QImage image;
    QString name;
    QMap<int, TrainingMarkState> marksByClass;
};

struct TrainingSessionState
{
    QVector<TrainingImageState> images;
    QStringList classes = QStringList() << QStringLiteral("Classification0");
    int currentImage = -1;
    int currentClass = 0;
};

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
        for (const TrainingMarkState &mark : state->images[imageIndex].marksByClass) {
            if (mark.hasMark)
                return true;
        }
        return false;
    };

    auto classTargetCount = [state](const int classIndex) {
        int count = 0;
        for (const TrainingImageState &image : state->images) {
            const TrainingMarkState mark = image.marksByClass.value(classIndex);
            if (mark.hasMark)
                ++count;
        }
        return count;
    };

    auto classImageCount = [state](const int classIndex) {
        int count = 0;
        for (const TrainingImageState &image : state->images) {
            const TrainingMarkState mark = image.marksByClass.value(classIndex);
            if (mark.hasMark)
                ++count;
        }
        return count;
    };

    auto totalMarkCount = [state]() {
        int count = 0;
        for (const TrainingImageState &image : state->images) {
            for (const TrainingMarkState &mark : image.marksByClass) {
                if (mark.hasMark)
                    ++count;
            }
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

    *showCurrentImage = [state, currentImageState, previewHelper, editStatusLabel](const QString &message) {
        TrainingImageState *imageState = currentImageState();
        if (!imageState) {
            previewHelper->clear();
            editStatusLabel->setText(QObject::tr("请先添加注册图"));
            return;
        }

        previewHelper->setImage(imageState->image);
        previewHelper->clearRoi();
        previewHelper->clearPolygonRoi();
        const TrainingMarkState mark = imageState->marksByClass.value(state->currentClass);
        if (mark.hasMark) {
            if (mark.type == QStringLiteral("polygon")) {
                previewHelper->setPolygonRoiNormalized(mark.polygon);
            } else {
                previewHelper->setRoiRectNormalized(mark.rect);
            }
        }
        previewHelper->fitToView();
        editStatusLabel->setText(message);
    };

    *refreshClassList = [=]() {
        while (QLayoutItem *item = classListLayout->takeAt(0)) {
            if (QWidget *widget = item->widget())
                widget->deleteLater();
            delete item;
        }

        classCountLabel->setText(QObject::tr("分类列表(%1)").arg(state->classes.size()));
        for (int classIndex = 0; classIndex < state->classes.size(); ++classIndex) {
            QFrame *rowFrame = new QFrame(classListWidget);
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
                        QObject::tr("删除当前 ROI"));

            rowLayout->addWidget(nameLabel, 2);
            rowLayout->addWidget(countLabel, 1);
            rowLayout->addWidget(renameButton);
            rowLayout->addWidget(previewButton);
            rowLayout->addWidget(deleteButton);
            classListLayout->addWidget(rowFrame);

            connect(renameButton, &QToolButton::clicked, this, [state, classIndex, editStatusLabel]() {
                state->currentClass = classIndex;
                editStatusLabel->setText(QObject::tr("重命名暂未接入，当前类别：%1")
                                         .arg(state->classes.value(classIndex)));
            });
            connect(previewButton, &QToolButton::clicked, this, [state, classIndex, previewHelper, editStatusLabel, showCurrentImage, refreshClassList]() {
                state->currentClass = classIndex;
                previewHelper->setRoiDrawingEnabled(false);
                previewHelper->setPolygonDrawingEnabled(false);
                (*showCurrentImage)(QObject::tr("正在预览类别 ROI：%1").arg(state->classes.value(classIndex)));
                (*refreshClassList)();
            });
            connect(deleteButton, &QToolButton::clicked, this, [state, classIndex, currentImageState, previewHelper, editStatusLabel, refreshStatus, refreshThumbnails, refreshClassList, showCurrentImage]() {
                state->currentClass = classIndex;
                if (TrainingImageState *imageState = currentImageState())
                    imageState->marksByClass.remove(classIndex);
                previewHelper->clearRoi();
                previewHelper->clearPolygonRoi();
                refreshStatus();
                (*refreshThumbnails)();
                (*refreshClassList)();
                (*showCurrentImage)(QObject::tr("已删除当前 ROI"));
            });
        }
        classListLayout->addStretch(1);
    };

    auto storeCurrentMark = [state, currentImageState, refreshStatus, refreshThumbnails, refreshClassList](const TrainingMarkState &mark) {
        TrainingImageState *imageState = currentImageState();
        if (!imageState)
            return;
        imageState->marksByClass.insert(state->currentClass, mark);
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

    auto setRoiMode = [state, previewHelper, fullButton, rectButton, polygonButton, editStatusLabel, storeCurrentMark](QToolButton *button) {
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
        if (button == fullButton) {
            previewHelper->clearPolygonRoi();
            previewHelper->setRoiRectNormalized(QRectF(0.0, 0.0, 1.0, 1.0));
            TrainingMarkState mark;
            mark.type = QStringLiteral("full");
            mark.rect = QRectF(0.0, 0.0, 1.0, 1.0);
            mark.hasMark = true;
            storeCurrentMark(mark);
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

    connect(cameraButton, &QPushButton::clicked, this, [appendTrainingImage, editStatusLabel, state]() {
        const cv::Mat frame = CameraFrameProvider::instance().currentFrame();
        if (frame.empty()) {
            editStatusLabel->setText(QObject::tr("当前相机帧为空，无法抓图"));
            return;
        }
        const QImage image = MatImageConverter::matToDisplayImage(
                    frame, QStringLiteral("RegisteredClassificationTrainingDialog"));
        appendTrainingImage(image,
                            QObject::tr("相机抓图%1").arg(state->images.size() + 1),
                            QObject::tr("已从当前相机帧抓图"));
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

    connect(createClassButton, &QPushButton::clicked, this, [state, refreshClassList, editStatusLabel]() {
        state->classes.append(QObject::tr("Classification%1").arg(state->classes.size()));
        state->currentClass = state->classes.size() - 1;
        (*refreshClassList)();
        editStatusLabel->setText(QObject::tr("已新建类别：%1").arg(state->classes.last()));
    });

    connect(clearAllMarksButton, &QPushButton::clicked, this, [state, previewHelper, refreshStatus, refreshThumbnails, refreshClassList, editStatusLabel]() {
        for (TrainingImageState &imageState : state->images)
            imageState.marksByClass.clear();
        previewHelper->clearRoi();
        previewHelper->clearPolygonRoi();
        refreshStatus();
        (*refreshThumbnails)();
        (*refreshClassList)();
        editStatusLabel->setText(QObject::tr("已清除全部标注"));
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

    connect(previewHelper, &FrameViewHelper::roiChanged, this, [previewHelper, rectButton, editStatusLabel, storeCurrentMark](const QRectF &roi) {
        rectButton->setChecked(true);
        previewHelper->setRoiRectNormalized(roi);
        TrainingMarkState mark;
        mark.type = QStringLiteral("rect");
        mark.rect = roi;
        mark.hasMark = true;
        storeCurrentMark(mark);
        editStatusLabel->setText(QObject::tr("矩形 ROI：x=%1 y=%2 w=%3 h=%4")
                                 .arg(roi.x(), 0, 'f', 3)
                                 .arg(roi.y(), 0, 'f', 3)
                                 .arg(roi.width(), 0, 'f', 3)
                                 .arg(roi.height(), 0, 'f', 3));
    });
    connect(previewHelper, &FrameViewHelper::roiSelectionRejected, this, [editStatusLabel](const QRectF &) {
        editStatusLabel->setText(QObject::tr("矩形 ROI 无效，请重新绘制"));
    });
    connect(previewHelper, &FrameViewHelper::polygonChanged, this, [previewHelper, polygonButton, editStatusLabel, storeCurrentMark](const QVector<QPointF> &points) {
        polygonButton->setChecked(true);
        previewHelper->setPolygonRoiNormalized(points);
        TrainingMarkState mark;
        mark.type = QStringLiteral("polygon");
        mark.polygon = points;
        mark.hasMark = true;
        storeCurrentMark(mark);
        editStatusLabel->setText(QObject::tr("多边形 ROI：%1 个点").arg(points.size()));
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
