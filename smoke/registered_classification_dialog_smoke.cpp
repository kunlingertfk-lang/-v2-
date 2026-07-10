#include "RegisteredClassificationDialog.h"
#include "RegisteredClassificationTrainingDialog.h"

#include "algorithms/recognition/RegisteredClassificationModelPackage.h"
#include "algorithms/recognition/RegisteredClassificationTrainingSession.h"
#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"

#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
#include <QTimer>
#include <iostream>

#include <opencv2/core.hpp>

namespace {

int g_failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++g_failures;
    }
}

void testTrainingSessionCodec()
{
    const QString sessionRoot = QDir::temp().filePath(
                QStringLiteral("registered_classification_training_session_smoke"));
    QDir(sessionRoot).removeRecursively();

    QJsonObject rectangleMark;
    rectangleMark.insert(QStringLiteral("classId"), 0);
    rectangleMark.insert(QStringLiteral("type"), QStringLiteral("rect"));
    rectangleMark.insert(QStringLiteral("rect"), QJsonArray{0.1, 0.2, 0.3, 0.4});
    QJsonObject polygonMark;
    polygonMark.insert(QStringLiteral("classId"), 1);
    polygonMark.insert(QStringLiteral("type"), QStringLiteral("polygon"));
    polygonMark.insert(QStringLiteral("polygon"), QJsonArray{
                          QJsonArray{0.2, 0.2}, QJsonArray{0.8, 0.2}, QJsonArray{0.5, 0.8}});
    QJsonObject firstImage;
    firstImage.insert(QStringLiteral("name"), QStringLiteral("front sample"));
    firstImage.insert(QStringLiteral("relativePath"), QStringLiteral("images/image_0001.png"));
    firstImage.insert(QStringLiteral("width"), 8);
    firstImage.insert(QStringLiteral("height"), 6);
    firstImage.insert(QStringLiteral("marksByClass"), QJsonArray{rectangleMark});
    QJsonObject secondImage;
    secondImage.insert(QStringLiteral("name"), QStringLiteral("back sample"));
    secondImage.insert(QStringLiteral("relativePath"), QStringLiteral("images/image_0002.png"));
    secondImage.insert(QStringLiteral("width"), 8);
    secondImage.insert(QStringLiteral("height"), 6);
    secondImage.insert(QStringLiteral("marksByClass"), QJsonArray{polygonMark});
    QJsonObject manifest;
    manifest.insert(QStringLiteral("schemaVersion"), 1);
    manifest.insert(QStringLiteral("classes"), QJsonArray{QStringLiteral("front"), QStringLiteral("back")});
    manifest.insert(QStringLiteral("images"), QJsonArray{firstImage, secondImage});

    QVector<RegisteredClassificationTrainingSessionAsset> assets;
    for (const QString &relativePath : {QStringLiteral("images/image_0001.png"),
                                        QStringLiteral("images/image_0002.png")}) {
        cv::Mat image(6, 8, CV_8UC3, cv::Scalar(12, 34, 56));
        RegisteredClassificationTrainingSessionAsset asset;
        asset.relativePath = relativePath;
        asset.image = image.clone();
        assets.append(asset);
    }

    const RegisteredClassificationTrainingSessionResult writeResult =
            writeRegisteredClassificationTrainingSession(sessionRoot, manifest, assets);
    check(writeResult.success, "training session codec must write a valid session");

    RegisteredClassificationTrainingSessionPayload payload;
    const RegisteredClassificationTrainingSessionResult readResult =
            readRegisteredClassificationTrainingSession(sessionRoot, &payload);
    check(readResult.success, "training session codec must read a valid session");
    check(payload.manifest.value(QStringLiteral("classes")) == manifest.value(QStringLiteral("classes")),
          "training session round trip must preserve class names");
    check(payload.manifest.value(QStringLiteral("images")) == manifest.value(QStringLiteral("images")),
          "training session round trip must preserve image names and ROI marks");
    check(payload.assets.size() == 2 && payload.assets.at(0).image.size() == cv::Size(8, 6),
          "training session round trip must preserve image dimensions");

    RegisteredClassificationTrainingSessionAsset invalidAsset;
    invalidAsset.relativePath = QStringLiteral("../outside.png");
    invalidAsset.image = cv::Mat(2, 2, CV_8UC3, cv::Scalar(0, 0, 0)).clone();
    QJsonObject invalidManifest = manifest;
    invalidManifest.insert(QStringLiteral("images"), QJsonArray{firstImage});
    const RegisteredClassificationTrainingSessionResult invalidResult =
            writeRegisteredClassificationTrainingSession(
                    QDir::temp().filePath(QStringLiteral("registered_classification_training_session_invalid")),
                    invalidManifest,
                    {invalidAsset});
    check(!invalidResult.success && invalidResult.status == QStringLiteral("invalid_session_path"),
          "training session codec must reject paths outside the session directory");
    QDir(sessionRoot).removeRecursively();
}

QPushButton *buttonByText(QWidget &root, const QString &text)
{
    const QList<QPushButton *> buttons = root.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button && button->text() == text)
            return button;
    }
    return nullptr;
}

QString statusText(QWidget &root)
{
    const QList<QLabel *> labels = root.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label && label->text().startsWith(QStringLiteral("注册分类:")))
            return label->text();
    }
    return QString();
}

QLabel *labelByText(QWidget &root, const QString &text)
{
    const QList<QLabel *> labels = root.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label && label->text() == text)
            return label;
    }
    return nullptr;
}

void clickAndProcess(QAbstractButton *button)
{
    if (!button)
        return;
    button->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void clickWidgetAndProcess(QWidget *widget)
{
    if (!widget)
        return;
    const QPoint center = widget->rect().center();
    QMouseEvent press(QEvent::MouseButtonPress,
                      center,
                      widget->mapToGlobal(center),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(widget, &press);
    QMouseEvent release(QEvent::MouseButtonRelease,
                        center,
                        widget->mapToGlobal(center),
                        Qt::LeftButton,
                        Qt::NoButton,
                        Qt::NoModifier);
    QApplication::sendEvent(widget, &release);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

QComboBox *comboBoxWithText(QWidget &root, const QString &text)
{
    const QList<QComboBox *> combos = root.findChildren<QComboBox *>();
    for (QComboBox *combo : combos) {
        if (!combo)
            continue;
        for (int index = 0; index < combo->count(); ++index) {
            if (combo->itemText(index) == text)
                return combo;
        }
    }
    return nullptr;
}

QDialog *childDialogByTitle(QWidget &root, const QString &title)
{
    const QList<QDialog *> dialogs = root.findChildren<QDialog *>();
    for (QDialog *dialog : dialogs) {
        if (dialog && dialog->windowTitle() == title)
            return dialog;
    }
    return nullptr;
}

QDialog *topLevelDialogByTitle(const QString &title)
{
    const QList<QWidget *> widgets = QApplication::topLevelWidgets();
    for (QWidget *widget : widgets) {
        QDialog *dialog = qobject_cast<QDialog *>(widget);
        if (dialog && dialog->windowTitle() == title)
            return dialog;
    }
    return nullptr;
}

QToolButton *toolButtonByObjectName(QWidget &root, const QString &objectName)
{
    const QList<QToolButton *> buttons = root.findChildren<QToolButton *>(objectName);
    return buttons.isEmpty() ? nullptr : buttons.first();
}

bool widgetCenterIsLight(QWidget *widget)
{
    if (!widget || widget->width() <= 0 || widget->height() <= 0)
        return false;

    QPixmap pixmap(widget->size());
    pixmap.fill(Qt::transparent);
    widget->render(&pixmap);
    const QImage image = pixmap.toImage();
    const QColor color = image.pixelColor(image.width() / 2, image.height() / 2);
    return color.red() >= 220 && color.green() >= 220 && color.blue() >= 220;
}

bool thumbnailCaptionContains(QWidget &root, const int imageIndex, const QString &text)
{
    QLabel *caption = root.findChild<QLabel *>(
                QStringLiteral("registeredTrainingThumbnailCaption_%1").arg(imageIndex));
    return caption && caption->isVisible() && caption->text().contains(text);
}

bool anyThumbnailCaptionContains(QWidget &root, const QString &text)
{
    const QList<QLabel *> labels = root.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label && label->isVisible() &&
            label->objectName().startsWith(QStringLiteral("registeredTrainingThumbnailCaption_")) &&
            label->text().contains(text))
            return true;
    }
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);
    testTrainingSessionCodec();

    const cv::Mat frame = cv::Mat::zeros(64, 64, CV_8UC3);
    ReferenceImageProvider::instance().setReferenceFrame(frame);
    CameraFrameProvider::instance().setCurrentFrame(frame);

    RegisteredClassificationDialog dialog;
    dialog.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QString managementModelDir;

    QCheckBox *positionSwitch = dialog.findChild<QCheckBox *>(QStringLiteral("positionCorrectionSwitch"));
    check(positionSwitch != nullptr, "position correction switch must exist");
    check(positionSwitch && !positionSwitch->isChecked(),
          "position correction switch must default to unchecked");
    check(positionSwitch && !positionSwitch->isEnabled(),
          "position correction switch must be disabled until implemented");
    check(!dialog.toToolConfig()
           .params.value(QStringLiteral("registeredClassification")).toObject()
           .value(QStringLiteral("enablePositionCorrection")).toBool(true),
          "default config must save enablePositionCorrection=false");

    QLabel *modelTitle = labelByText(dialog, QStringLiteral("模型训练"));
    QLabel *detectTitle = labelByText(dialog, QStringLiteral("检测区域"));
    check(modelTitle != nullptr, "model training card title must exist");
    check(detectTitle != nullptr, "detect region card title must exist");
    if (modelTitle && detectTitle) {
        const int modelY = modelTitle->mapTo(&dialog, QPoint(0, 0)).y();
        const int detectY = detectTitle->mapTo(&dialog, QPoint(0, 0)).y();
        check(modelY < detectY, "model training card must be above detect region card");
    }

    QPushButton *allParamsButton = buttonByText(dialog, QStringLiteral("全部"));
    check(allParamsButton != nullptr, "all params segment button must exist");
    clickAndProcess(allParamsButton);
    check(labelByText(dialog, QStringLiteral("参数设置")) != nullptr,
          "all params card must be renamed to parameter settings");
    check(labelByText(dialog, QStringLiteral("前K个类别")) != nullptr,
          "parameter settings must contain top K class field");
    check(labelByText(dialog, QStringLiteral("最小相似度")) != nullptr,
          "parameter settings must contain min similarity field");
    check(comboBoxWithText(dialog, QStringLiteral("所有检测区域输出结果为 OK")) != nullptr,
          "judge type combo must contain all-ok option");
    check(comboBoxWithText(dialog, QStringLiteral("任意检测区域输出结果为 OK")) != nullptr,
          "judge type combo must contain any-ok option");

    const QJsonObject rcParams = dialog.toToolConfig()
            .params.value(QStringLiteral("registeredClassification")).toObject();
    check(rcParams.value(QStringLiteral("minSimilarity")).toInt(-1) == 68,
          "default config must save minSimilarity=68");
    check(dialog.toToolConfig().judgeRule.value(QStringLiteral("judgeType")).toString()
          == QStringLiteral("all_ok"),
          "default judgeRule must save judgeType=all_ok");

    QPushButton *registerTrainingButton = buttonByText(dialog, QStringLiteral("注册训练"));
    check(registerTrainingButton != nullptr, "register training button must exist");
    clickAndProcess(registerTrainingButton);
    QDialog *trainingDialog = childDialogByTitle(dialog, QStringLiteral("注册分类"));
    check(trainingDialog != nullptr, "register training button must open training dialog");
    if (trainingDialog) {
        const QSize parentSize = dialog.size();
        check(trainingDialog->width() >= qRound(parentSize.width() * 0.70) &&
              trainingDialog->width() <= qRound(parentSize.width() * 0.80),
              "training dialog initial width must be 70-80 percent of parent");
        check(trainingDialog->height() >= qRound(parentSize.height() * 0.70) &&
              trainingDialog->height() <= qRound(parentSize.height() * 0.80),
              "training dialog initial height must be 70-80 percent of parent");

        QPushButton *cameraCaptureButton = buttonByText(*trainingDialog, QStringLiteral("相机抓图"));
        QPushButton *externalImportButton = buttonByText(*trainingDialog, QStringLiteral("外部导入"));
        check(cameraCaptureButton != nullptr, "training dialog must have camera capture button");
        check(externalImportButton != nullptr, "training dialog must have external import button");
        CameraFrameProvider::instance().clearFrame();
        clickAndProcess(cameraCaptureButton);
        CameraFrameProvider::instance().setCurrentFrame(frame);
        FrameViewHelper *trainingPreviewHelper = trainingDialog->findChild<FrameViewHelper *>(
                    QStringLiteral("registeredTrainingPreviewHelper"));
        check(trainingPreviewHelper && trainingPreviewHelper->hasImage(),
              "camera capture must display fallback reference frame when current camera frame is empty");

        QListWidget *thumbnailList = trainingDialog->findChild<QListWidget *>(
                    QStringLiteral("registeredTrainingThumbnailList"));
        check(thumbnailList != nullptr, "training dialog must have thumbnail list");
        check(thumbnailList && thumbnailList->count() == 1,
              "camera capture must add one thumbnail");
        check(thumbnailList && thumbnailList->currentRow() == 0,
              "camera capture thumbnail must be selected");
        QComboBox *thumbnailFilter = trainingDialog->findChild<QComboBox *>(
                    QStringLiteral("registeredTrainingFilterCombo"));
        check(thumbnailFilter != nullptr,
              "registered classification thumbnail area must expose filter combo");
        check(thumbnailFilter && thumbnailFilter->itemText(0) == QStringLiteral("全部") &&
              thumbnailFilter->itemText(1) == QStringLiteral("标注") &&
              thumbnailFilter->itemText(2) == QStringLiteral("未标注"),
              "registered classification thumbnail filter must contain all/marked/unmarked options");
        check(thumbnailCaptionContains(*trainingDialog, 0, QStringLiteral("基准图")),
              "camera capture fallback thumbnail must show reference image source name");
        check(toolButtonByObjectName(*trainingDialog,
              QStringLiteral("registeredTrainingDeleteImageButton_0")) != nullptr,
              "registered classification thumbnail must expose single image delete button");
        check(toolButtonByObjectName(*trainingDialog,
              QStringLiteral("registeredTrainingDeleteAllImagesButton")) != nullptr,
              "registered classification thumbnail area must expose delete-all images button");
        CameraFrameProvider::instance().clearFrame();
        clickAndProcess(cameraCaptureButton);
        CameraFrameProvider::instance().setCurrentFrame(frame);
        check(thumbnailList && thumbnailList->count() == 2,
              "second capture must add another thumbnail");
        check(thumbnailList && thumbnailList->currentRow() == 1,
              "second capture thumbnail must be selected");
        QToolButton *previousImageButton = toolButtonByObjectName(*trainingDialog,
                    QStringLiteral("registeredTrainingPreviousImageButton"));
        QToolButton *nextImageButton = toolButtonByObjectName(*trainingDialog,
                    QStringLiteral("registeredTrainingNextImageButton"));
        check(previousImageButton && previousImageButton->toolTip() == QStringLiteral("上一张注册图"),
              "training dialog must have previous image button with tooltip");
        check(nextImageButton && nextImageButton->toolTip() == QStringLiteral("下一张注册图"),
              "training dialog must have next image button with tooltip");
        if (thumbnailList && previousImageButton && nextImageButton) {
            clickAndProcess(nextImageButton);
            check(thumbnailList->currentRow() == 0,
                  "next image button must cycle from last image to first image");
            clickAndProcess(previousImageButton);
            check(thumbnailList->currentRow() == 1,
                  "previous image button must cycle from first image to last image");
            thumbnailList->setCurrentRow(0);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }

        QToolButton *fullRoiButton = toolButtonByObjectName(*trainingDialog,
                                                            QStringLiteral("trainingFullRoiButton"));
        QToolButton *rectRoiButton = toolButtonByObjectName(*trainingDialog,
                                                            QStringLiteral("trainingRectRoiButton"));
        QToolButton *polygonRoiButton = toolButtonByObjectName(*trainingDialog,
                                                               QStringLiteral("trainingPolygonRoiButton"));
        check(fullRoiButton && !fullRoiButton->icon().isNull() &&
              fullRoiButton->toolTip() == QStringLiteral("全屏框选"),
              "full ROI button must be icon button with tooltip");
        check(rectRoiButton && !rectRoiButton->icon().isNull() &&
              rectRoiButton->toolTip() == QStringLiteral("矩形框选"),
              "rect ROI button must be icon button with tooltip");
        check(polygonRoiButton && !polygonRoiButton->icon().isNull() &&
              polygonRoiButton->toolTip() == QStringLiteral("多边形框选"),
              "polygon ROI button must be icon button with tooltip");
        QWidget *classList = trainingDialog->findChild<QWidget *>(
                    QStringLiteral("registeredTrainingClassList"));
        QLabel *classCountLabel = trainingDialog->findChild<QLabel *>(
                    QStringLiteral("registeredTrainingClassCountLabel"));
        QPushButton *createClassButton = buttonByText(*trainingDialog, QStringLiteral("+ 新建"));
        QToolButton *previewClassButton = toolButtonByObjectName(*trainingDialog,
                    QStringLiteral("registeredTrainingPreviewClassButton_0"));
        QToolButton *deleteClassMarkButton = toolButtonByObjectName(*trainingDialog,
                    QStringLiteral("registeredTrainingDeleteClassMarkButton_0"));
        QToolButton *renameClassButton = toolButtonByObjectName(*trainingDialog,
                    QStringLiteral("registeredTrainingRenameClassButton_0"));
        QPushButton *clearAllMarksButton = buttonByText(*trainingDialog,
                                                        QStringLiteral("清除全部标注"));
        check(classList != nullptr, "training dialog must use class list container");
        check(classList && widgetCenterIsLight(classList),
              "training class list must render with light project style");
        check(classCountLabel && classCountLabel->text() == QStringLiteral("分类列表(1)"),
              "class list title must show default class count");
        check(createClassButton != nullptr, "training dialog must have create class button");
        check(renameClassButton && renameClassButton->toolTip() == QStringLiteral("重命名"),
              "class row rename button must have semantic tooltip");
        if (renameClassButton) {
            QTimer::singleShot(0, []() {
                QDialog *renameDialog = topLevelDialogByTitle(QStringLiteral("重命名类别"));
                check(renameDialog != nullptr, "rename class dialog must open");
                if (!renameDialog)
                    return;
                QLineEdit *lineEdit = renameDialog->findChild<QLineEdit *>();
                check(lineEdit != nullptr, "rename class dialog must have line edit");
                if (lineEdit)
                    lineEdit->setText(QStringLiteral("Widget"));
                QPushButton *okButton = buttonByText(*renameDialog, QStringLiteral("OK"));
                if (!okButton)
                    okButton = buttonByText(*renameDialog, QStringLiteral("确定"));
                check(okButton != nullptr, "rename class dialog must have confirm button");
                if (okButton)
                    okButton->click();
            });
            clickAndProcess(renameClassButton);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            check(labelByText(*trainingDialog, QStringLiteral("Widget")) != nullptr,
                  "renaming class must update class row label");
            previewClassButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingPreviewClassButton_0"));
            deleteClassMarkButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingDeleteClassMarkButton_0"));
        }
        check(previewClassButton && previewClassButton->toolTip() == QStringLiteral("预览类别 ROI"),
              "class row preview button must have semantic tooltip");
        check(deleteClassMarkButton && deleteClassMarkButton->toolTip() == QStringLiteral("删除类别"),
              "class row delete button must have semantic tooltip");
        check(clearAllMarksButton != nullptr, "training dialog must have clear all marks button");
        if (rectRoiButton && polygonRoiButton && trainingPreviewHelper) {
            clickAndProcess(rectRoiButton);
            check(rectRoiButton->isChecked(), "rect ROI button must highlight after click");
            check(trainingPreviewHelper->isRoiDrawingEnabled(),
                  "rect ROI drawing must be enabled only while rect button is highlighted");
            clickAndProcess(polygonRoiButton);
            check(!rectRoiButton->isChecked() && polygonRoiButton->isChecked(),
                  "ROI buttons must be mutually exclusive");
            check(!trainingPreviewHelper->isRoiDrawingEnabled() &&
                  trainingPreviewHelper->isPolygonDrawingEnabled(),
                  "polygon drawing must replace rect drawing");
            clickAndProcess(polygonRoiButton);
            check(!polygonRoiButton->isChecked() &&
                  !trainingPreviewHelper->isPolygonDrawingEnabled(),
                  "clicking highlighted polygon ROI button again must restore idle state");
        }
        if (trainingPreviewHelper && rectRoiButton && thumbnailList &&
            previewClassButton && deleteClassMarkButton && clearAllMarksButton) {
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.12, 0.15, 0.32, 0.28));
            emit trainingPreviewHelper->roiChanged(QRectF(0.12, 0.15, 0.32, 0.28));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            check(anyThumbnailCaptionContains(*trainingDialog, QStringLiteral("已标注")),
                  "ROI completion must mark current thumbnail as annotated");
            if (thumbnailFilter) {
                thumbnailFilter->setCurrentText(QStringLiteral("标注"));
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                check(thumbnailList->count() == 1,
                      "registered classification marked filter must show only annotated thumbnails");
                thumbnailFilter->setCurrentText(QStringLiteral("未标注"));
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                check(thumbnailList->count() == 1,
                      "registered classification unmarked filter must show only unannotated thumbnails");
                thumbnailFilter->setCurrentText(QStringLiteral("全部"));
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                check(thumbnailList->count() == 2,
                      "registered classification all filter must show all thumbnails");
                thumbnailList->setCurrentRow(0);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            }
            check(classCountLabel && classCountLabel->text().contains(QStringLiteral("1")),
                  "ROI completion must update class list count");
            RegisteredClassificationTrainingDialog *typedTrainingDialog =
                    qobject_cast<RegisteredClassificationTrainingDialog *>(trainingDialog);
            check(typedTrainingDialog != nullptr,
                  "training dialog must expose registered classification training API");
            if (typedTrainingDialog) {
                const QJsonObject preview = typedTrainingDialog->buildTrainingRequestPreviewForTest();
                check(preview.value(QStringLiteral("classCount")).toInt() >= 1,
                      "training preview must include classes");
                check(!preview.value(QStringLiteral("classNames")).toArray().isEmpty(),
                      "training preview must expose class names");
                check(preview.value(QStringLiteral("imageCount")).toInt() >= 1,
                      "training preview must include images after adding registration image");
                check(preview.value(QStringLiteral("roiCount")).toInt() >= 1,
                      "training preview must include ROI count after marking ROI");
            }
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.46, 0.18, 0.22, 0.25));
            emit trainingPreviewHelper->roiChanged(QRectF(0.46, 0.18, 0.22, 0.25));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QLabel *firstRoiNumberLabel = trainingDialog->findChild<QLabel *>(
                        QStringLiteral("registeredTrainingRoiNumberLabel_0"));
            QLabel *secondRoiNumberLabel = trainingDialog->findChild<QLabel *>(
                        QStringLiteral("registeredTrainingRoiNumberLabel_1"));
            check(firstRoiNumberLabel != nullptr,
                  "first ROI must expose registeredTrainingRoiNumberLabel_0");
            check(secondRoiNumberLabel != nullptr,
                  "second ROI must expose registeredTrainingRoiNumberLabel_1");
            check(firstRoiNumberLabel && firstRoiNumberLabel->text() == QStringLiteral("1"),
                  "first ROI number label must show 1 on the big image");
            check(secondRoiNumberLabel && secondRoiNumberLabel->text() == QStringLiteral("2"),
                  "second ROI number label must show 2 on the big image");
            clickAndProcess(previewClassButton);
            QWidget *previewPage = trainingDialog->findChild<QWidget *>(
                        QStringLiteral("registeredTrainingPreviewPage"));
            check(previewPage != nullptr && previewPage->isVisible(),
                  "class preview must show ROI preview page");
            check(labelByText(*trainingDialog, QStringLiteral("Widget | 已标注目标：2")) != nullptr,
                  "preview page header must show class name and ROI count");
            QLabel *previewPageTitle = labelByText(*trainingDialog,
                                                   QStringLiteral("Widget | 已标注目标：2"));
            check(previewPageTitle && previewPageTitle->styleSheet().isEmpty(),
                  "classification preview title should rely on role QSS rather than inline style");
            check(trainingDialog->styleSheet().contains(
                      QStringLiteral("QLabel[role=\"previewPageTitle\"]{font-size:26px;font-weight:800;color:#ffffff;}")),
                  "classification ROI preview page title must use white text on dark preview page");
            check(trainingDialog->findChild<QWidget *>(
                      QStringLiteral("registeredTrainingRoiPreviewCard_0_0_0")) != nullptr,
                  "preview page must show first ROI preview card");
            check(trainingDialog->findChild<QWidget *>(
                      QStringLiteral("registeredTrainingRoiPreviewCard_0_0_1")) != nullptr,
                  "preview page must show second ROI preview card");
            clickAndProcess(previewClassButton);
            check(previewPage && !previewPage->isVisible(),
                  "clicking the same preview button again must return to big image");
            clickAndProcess(previewClassButton);
            QToolButton *previewCloseButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingPreviewCloseButton"));
            check(previewCloseButton != nullptr, "preview page must have close button");
            clickAndProcess(previewCloseButton);
            check(previewPage && !previewPage->isVisible(),
                  "preview close button must return to big image");
            clickAndProcess(previewClassButton);
            check(!trainingPreviewHelper->isRoiDrawingEnabled() &&
                  !trainingPreviewHelper->isPolygonDrawingEnabled(),
                  "class preview must leave ROI drawing modes idle");
            check(labelByText(*trainingDialog, QStringLiteral("正在预览类别 ROI：Widget")) != nullptr,
                  "class preview must show clear preview status text");
            QToolButton *deleteRoiButton = trainingDialog->findChild<QToolButton *>(
                        QStringLiteral("registeredTrainingDeleteRoiButton_0_0_1"));
            check(deleteRoiButton != nullptr,
                  "classification ROI preview card must expose delete ROI button");
            check(deleteRoiButton && deleteRoiButton->toolTip() == QStringLiteral("删除当前 ROI"),
                  "hovering classification ROI delete icon must explain delete current ROI");
            check(deleteRoiButton && deleteRoiButton->statusTip() == QStringLiteral("删除当前 ROI"),
                  "classification ROI delete icon must expose delete current ROI status tip");
            QWidget *secondCard = trainingDialog->findChild<QWidget *>(
                        QStringLiteral("registeredTrainingRoiPreviewCard_0_0_1"));
            if (secondCard) {
                secondCard->setFocus();
                QContextMenuEvent event(QContextMenuEvent::Mouse,
                                        secondCard->rect().center(),
                                        secondCard->mapToGlobal(secondCard->rect().center()));
                QApplication::sendEvent(secondCard, &event);
                QMenu *menu = nullptr;
                for (QWidget *widget : QApplication::topLevelWidgets()) {
                    QMenu *candidate = qobject_cast<QMenu *>(widget);
                    if (candidate && candidate->objectName() == QStringLiteral("registeredTrainingPreviewRoiContextMenu")) {
                        menu = candidate;
                        break;
                    }
                }
                check(menu != nullptr, "right-clicking selected ROI card must open preview ROI context menu");
                if (menu) {
                    QAction *deleteAction = nullptr;
                    for (QAction *action : menu->actions()) {
                        if (action && action->text() == QStringLiteral("删除当前 ROI"))
                            deleteAction = action;
                    }
                    check(deleteAction != nullptr, "ROI context menu must contain delete action");
                    if (deleteAction)
                        deleteAction->trigger();
                }
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            }
            check(trainingDialog->findChild<QWidget *>(
                      QStringLiteral("registeredTrainingRoiPreviewCard_0_0_1")) == nullptr,
                  "deleting one ROI from preview page must remove only that ROI card");
            check(labelByText(*trainingDialog, QStringLiteral("Widget | 已标注目标：1")) != nullptr,
                  "preview page header must update after deleting one ROI");
            clickAndProcess(previewClassButton);
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.40, 0.40, 0.20, 0.20));
            emit trainingPreviewHelper->roiChanged(QRectF(0.40, 0.40, 0.20, 0.20));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QGraphicsView *trainingView = trainingDialog->findChild<QGraphicsView *>(
                        QStringLiteral("registeredTrainingPreviewView"));
            check(trainingView != nullptr,
                  "classification training preview view must exist for ROI context menu");
            if (trainingView) {
                const QPoint center = trainingView->viewport()->rect().center();
                QMouseEvent rightPress(QEvent::MouseButtonPress,
                                       center,
                                       trainingView->viewport()->mapToGlobal(center),
                                       Qt::RightButton,
                                       Qt::RightButton,
                                       Qt::NoModifier);
                QApplication::sendEvent(trainingView->viewport(), &rightPress);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                QMenu *pressMenu = nullptr;
                for (QWidget *widget : QApplication::topLevelWidgets()) {
                    QMenu *candidate = qobject_cast<QMenu *>(widget);
                    if (candidate && candidate->objectName() == QStringLiteral("registeredTrainingRoiContextMenu")) {
                        pressMenu = candidate;
                        break;
                    }
                }
                check(pressMenu == nullptr,
                      "right mouse press on classification ROI must not open delete menu before context-menu event");
                check(labelByText(*trainingDialog, QStringLiteral("1 / 1")) != nullptr,
                      "right mouse press on classification ROI must not delete ROI directly");
                if (pressMenu)
                    pressMenu->close();
                QContextMenuEvent contextEvent(QContextMenuEvent::Mouse,
                                               center,
                                               trainingView->viewport()->mapToGlobal(center));
                QApplication::sendEvent(trainingView->viewport(), &contextEvent);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                QMenu *menu = nullptr;
                for (QWidget *widget : QApplication::topLevelWidgets()) {
                    QMenu *candidate = qobject_cast<QMenu *>(widget);
                    if (!candidate ||
                        candidate->objectName() != QStringLiteral("registeredTrainingRoiContextMenu"))
                        continue;
                    for (QAction *action : candidate->actions()) {
                        if (action && action->text() == QStringLiteral("删除当前 ROI")) {
                            menu = candidate;
                            break;
                        }
                    }
                    if (menu)
                        break;
                }
                check(menu != nullptr,
                      "right-clicking selected classification ROI must open delete-current-ROI menu");
                if (menu) {
                    for (QAction *action : menu->actions()) {
                        if (action && action->text() == QStringLiteral("删除当前 ROI")) {
                            action->trigger();
                            break;
                        }
                    }
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                }
                bool lingeringDeleteMenu = false;
                for (QWidget *widget : QApplication::topLevelWidgets()) {
                    QMenu *candidate = qobject_cast<QMenu *>(widget);
                    if (candidate && candidate->isVisible() &&
                        candidate->objectName() == QStringLiteral("registeredTrainingRoiContextMenu")) {
                        lingeringDeleteMenu = true;
                        break;
                    }
                }
                check(!lingeringDeleteMenu,
                      "classification ROI delete menu must disappear after delete action");
                check(labelByText(*trainingDialog, QStringLiteral("0 / 0")) != nullptr,
                      "right-click delete current classification ROI must update class count to zero");
            }
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.40, 0.40, 0.20, 0.20));
            emit trainingPreviewHelper->roiChanged(QRectF(0.40, 0.40, 0.20, 0.20));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            clickAndProcess(previewClassButton);
            renameClassButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingRenameClassButton_0"));
            if (renameClassButton) {
                QTimer::singleShot(0, []() {
                    QDialog *renameDialog = topLevelDialogByTitle(QStringLiteral("重命名类别"));
                    check(renameDialog != nullptr, "rename dialog from preview mode must open");
                    if (renameDialog)
                        renameDialog->reject();
                });
                clickAndProcess(renameClassButton);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                check(previewPage && previewPage->isVisible(),
                      "canceling rename from preview mode must keep preview page open");
                bool widgetPreviewTitleVisible = false;
                const QList<QLabel *> labels = trainingDialog->findChildren<QLabel *>();
                for (QLabel *label : labels) {
                    if (label && label->isVisible() &&
                        label->text().startsWith(QStringLiteral("Widget | 已标注目标："))) {
                        widgetPreviewTitleVisible = true;
                        break;
                    }
                }
                check(widgetPreviewTitleVisible,
                      "canceling rename from preview mode must keep Widget preview header visible");
            }
            clickAndProcess(createClassButton);
            check(classCountLabel->text() == QStringLiteral("分类列表(2)"),
                  "creating a class from preview mode must append a new class");
            check(previewPage && !previewPage->isVisible(),
                  "creating a class from preview mode must return to big image");
            QToolButton *previewCreateDeleteSecondClassButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingDeleteClassMarkButton_1"));
            check(previewCreateDeleteSecondClassButton != nullptr,
                  "preview-created class row must have delete button");
            clickAndProcess(previewCreateDeleteSecondClassButton);
            check(classCountLabel->text() == QStringLiteral("分类列表(1)"),
                  "cleanup after preview-created class must restore one class");
            previewClassButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingPreviewClassButton_0"));
            deleteClassMarkButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingDeleteClassMarkButton_0"));
            clickAndProcess(deleteClassMarkButton);
            check(!anyThumbnailCaptionContains(*trainingDialog, QStringLiteral("已标注")),
                  "deleting current ROI must clear thumbnail annotation state");
            const QVector<QPointF> polygonPoints = QVector<QPointF>()
                    << QPointF(0.1, 0.1) << QPointF(0.4, 0.1) << QPointF(0.2, 0.4);
            trainingPreviewHelper->setPolygonRoiNormalized(polygonPoints);
            emit trainingPreviewHelper->polygonChanged(polygonPoints);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            const QVector<QPointF> editedPolygonPoints = QVector<QPointF>()
                    << QPointF(0.12, 0.12) << QPointF(0.42, 0.12) << QPointF(0.24, 0.42);
            trainingPreviewHelper->setPolygonRoiNormalized(editedPolygonPoints);
            emit trainingPreviewHelper->polygonChanged(editedPolygonPoints);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            previewClassButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingPreviewClassButton_0"));
            clickAndProcess(previewClassButton);
            check(labelByText(*trainingDialog, QStringLiteral("Widget | 已标注目标：1")) != nullptr,
                  "editing the current polygon ROI must not append a duplicate ROI");
            clickAndProcess(previewClassButton);
            clickAndProcess(clearAllMarksButton);
            check(!anyThumbnailCaptionContains(*trainingDialog, QStringLiteral("已标注")),
                  "clear all marks must clear thumbnail annotation state");
        }
        if (createClassButton && classCountLabel) {
            clickAndProcess(createClassButton);
            check(classCountLabel->text() == QStringLiteral("分类列表(2)"),
                  "create class button must append a new class");
            check(toolButtonByObjectName(*trainingDialog,
                  QStringLiteral("registeredTrainingPreviewClassButton_1")) != nullptr,
                  "new class row must have preview button");
            QFrame *firstClassRow = trainingDialog->findChild<QFrame *>(
                        QStringLiteral("registeredTrainingClassRow_0"));
            QFrame *secondClassRow = trainingDialog->findChild<QFrame *>(
                        QStringLiteral("registeredTrainingClassRow_1"));
            check(firstClassRow != nullptr, "first class row must expose stable object name");
            check(secondClassRow != nullptr, "second class row must expose stable object name");
            clickWidgetAndProcess(firstClassRow);
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.08, 0.20, 0.18, 0.18));
            emit trainingPreviewHelper->roiChanged(QRectF(0.08, 0.20, 0.18, 0.18));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            clickWidgetAndProcess(secondClassRow);
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.20, 0.20, 0.18, 0.18));
            emit trainingPreviewHelper->roiChanged(QRectF(0.20, 0.20, 0.18, 0.18));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            RegisteredClassificationTrainingDialog *typedTrainingDialog =
                    qobject_cast<RegisteredClassificationTrainingDialog *>(trainingDialog);
            if (typedTrainingDialog) {
                const QJsonObject preview = typedTrainingDialog->buildTrainingRequestPreviewForTest();
                check(preview.value(QStringLiteral("classCount")).toInt() == 2,
                      "training request preview must include two classes");
                check(preview.value(QStringLiteral("sampleCount")).toInt() >= 2,
                      "training request preview must include ROI samples");
                check(preview.value(QStringLiteral("trainable")).toBool(),
                      "training request preview must become trainable when two classes have ROI samples");
                check(preview.value(QStringLiteral("modelType")).toString()
                          == QStringLiteral("halcon_mlp_registered_classification"),
                      "training request preview must use HALCON MLP model type");
                check(preview.value(QStringLiteral("defaultOutputModelDir")).toString()
                          .contains(QStringLiteral("ModelFiles/RegisteredClass")),
                      "training default output must be under ModelFiles/RegisteredClass");
                const QString modelDir = QDir::temp().filePath(
                            QStringLiteral("registered_classification_dialog_smoke_ui_training_model"));
                QDir(modelDir).removeRecursively();
                const RegisteredClassificationTrainingResult trainResult =
                        typedTrainingDialog->trainToModelDirForTest(modelDir);
                check(trainResult.success,
                      "training dialog must convert UI session into a successful MLP training request");
                check(QFileInfo(registeredClassificationMlpPath(modelDir)).exists(),
                      "training dialog train action must create model.gmc");
                check(QFileInfo(registeredClassificationMetadataPath(modelDir)).exists(),
                      "training dialog train action must create metadata.json");
                check(QFileInfo(registeredClassificationTrainingReportPath(modelDir)).exists(),
                      "training dialog train action must create training_report.json");
                check(QFileInfo(QDir(modelDir).filePath(QStringLiteral("training_session/session.json"))).exists(),
                      "training dialog train action must create training session manifest");
                check(QDir(QDir(modelDir).filePath(QStringLiteral("training_session/images")))
                              .entryList(QStringList() << QStringLiteral("*.png"), QDir::Files).size() >= 1,
                      "training dialog train action must copy registration images into training session");
                check(dialog.toToolConfig()
                          .params.value(QStringLiteral("registeredClassification")).toObject()
                          .value(QStringLiteral("modelPath")).toString() == modelDir,
                      "training completion must fill generated model path back to parent dialog");
                managementModelDir = QDir(QCoreApplication::applicationDirPath()).filePath(
                            QStringLiteral("ModelFiles/RegisteredClass/20990101/model_smoke_management"));
                QDir(managementModelDir).removeRecursively();
                const RegisteredClassificationTrainingResult managementTrainResult =
                        typedTrainingDialog->trainToModelDirForTest(managementModelDir);
                check(managementTrainResult.success,
                      "training dialog must create a runtime model package for model management");
                QPushButton *mainDeleteModelButton = buttonByText(dialog, QStringLiteral("删除模型"));
                if (mainDeleteModelButton)
                    clickAndProcess(mainDeleteModelButton);
            }
            QPushButton *startTrainingButton = buttonByText(*trainingDialog, QStringLiteral("开始训练"));
            check(startTrainingButton != nullptr && startTrainingButton->isEnabled(),
                  "start training button must be enabled when two classes have samples");
            QToolButton *previewSecondClassButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingPreviewClassButton_1"));
            check(previewSecondClassButton != nullptr, "second class row must keep preview button");
            clickAndProcess(previewSecondClassButton);
            check(labelByText(*trainingDialog, QStringLiteral("Classification1 | 已标注目标：1")) != nullptr,
                  "row body selection must make new ROI belong to the selected class");
            clickAndProcess(previewSecondClassButton);
            QToolButton *previewFirstClassButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingPreviewClassButton_0"));
            check(previewFirstClassButton != nullptr, "first class row must keep preview button");
            clickAndProcess(previewFirstClassButton);
            QWidget *previewPageFromRowSwitch = trainingDialog->findChild<QWidget *>(
                        QStringLiteral("registeredTrainingPreviewPage"));
            check(previewPageFromRowSwitch != nullptr && previewPageFromRowSwitch->isVisible(),
                  "first class preview must open preview page before row switch");
            clickWidgetAndProcess(secondClassRow);
            check(previewPageFromRowSwitch && !previewPageFromRowSwitch->isVisible(),
                  "clicking another class row body from preview page must return to big image");
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.55, 0.20, 0.18, 0.18));
            emit trainingPreviewHelper->roiChanged(QRectF(0.55, 0.20, 0.18, 0.18));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            previewSecondClassButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingPreviewClassButton_1"));
            clickAndProcess(previewSecondClassButton);
            check(labelByText(*trainingDialog, QStringLiteral("Classification1 | 已标注目标：2")) != nullptr,
                  "row body switch from preview page must make later ROI belong to the switched class");
            clickAndProcess(previewSecondClassButton);
            QToolButton *deleteSecondClassButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingDeleteClassMarkButton_1"));
            check(deleteSecondClassButton != nullptr, "second class row must have delete button");
            clickAndProcess(deleteSecondClassButton);
            check(classCountLabel->text() == QStringLiteral("分类列表(1)"),
                  "deleting a non-last class must remove the class row");
            QToolButton *deleteLastClassButton = toolButtonByObjectName(*trainingDialog,
                        QStringLiteral("registeredTrainingDeleteClassMarkButton_0"));
            clickAndProcess(deleteLastClassButton);
            check(classCountLabel->text() == QStringLiteral("分类列表(1)"),
                  "deleting the last class must keep one default class");
            check(!anyThumbnailCaptionContains(*trainingDialog, QStringLiteral("已标注")),
                  "deleting the last class must clear its ROI marks");
        }
        trainingDialog->close();
    }

    QPushButton *modelManagementButton = buttonByText(dialog, QStringLiteral("模型管理"));
    check(modelManagementButton != nullptr, "model management button must exist");
    clickAndProcess(modelManagementButton);
    QDialog *managementDialog = childDialogByTitle(dialog, QStringLiteral("模型训练"));
    check(managementDialog != nullptr, "model management button must open model management dialog");
    if (managementDialog) {
        const QSize parentSize = dialog.size();
        check(managementDialog->width() >= qRound(parentSize.width() * 0.70) &&
              managementDialog->width() <= qRound(parentSize.width() * 0.80),
              "model management dialog initial width must be 70-80 percent of parent");
        check(managementDialog->height() >= qRound(parentSize.height() * 0.70) &&
              managementDialog->height() <= qRound(parentSize.height() * 0.80),
              "model management dialog initial height must be 70-80 percent of parent");
        check(labelByText(*managementDialog, QStringLiteral("默认数据集")) != nullptr,
              "model management dialog must keep default dataset");
        check(labelByText(*managementDialog, QStringLiteral("生产样本")) == nullptr,
              "model management dialog must not show production sample dataset");
        check(labelByText(*managementDialog, QStringLiteral("验证样本")) == nullptr,
              "model management dialog must not show validation sample dataset");
        QToolButton *renameButton = toolButtonByObjectName(*managementDialog,
                                                           QStringLiteral("renameModelButton"));
        QToolButton *exportButton = toolButtonByObjectName(*managementDialog,
                                                           QStringLiteral("exportModelButton"));
        QToolButton *deleteButton = toolButtonByObjectName(*managementDialog,
                                                           QStringLiteral("deleteModelButton"));
        check(renameButton && renameButton->toolTip() == QStringLiteral("重命名"),
              "model row rename icon must have rename tooltip");
        check(exportButton && exportButton->toolTip() == QStringLiteral("导出"),
              "model row export icon must have export tooltip");
        check(deleteButton && deleteButton->toolTip() == QStringLiteral("删除"),
              "model row delete icon must have delete tooltip");
        QFrame *scannedModelRow = nullptr;
        const QList<QFrame *> modelRows = managementDialog->findChildren<QFrame *>();
        for (QFrame *row : modelRows) {
            if (row && row->property("modelDir").toString() == managementModelDir) {
                scannedModelRow = row;
                break;
            }
        }
        check(scannedModelRow != nullptr, "model management must list scanned model packages");
        if (scannedModelRow) {
            check(scannedModelRow->property("modelDir").toString() == managementModelDir,
                  "model management row must store model package directory");
        }
        QPushButton *retrainButton = scannedModelRow
                ? buttonByText(*scannedModelRow, QStringLiteral("重新训练"))
                : nullptr;
        check(retrainButton != nullptr, "model management must expose retrain action");
        if (retrainButton && scannedModelRow) {
            clickAndProcess(retrainButton);
            RegisteredClassificationTrainingDialog *typedRetrainDialog = nullptr;
            const QList<RegisteredClassificationTrainingDialog *> retrainDialogs =
                    dialog.findChildren<RegisteredClassificationTrainingDialog *>();
            for (RegisteredClassificationTrainingDialog *candidate : retrainDialogs) {
                if (!candidate)
                    continue;
                const QJsonObject preview = candidate->buildTrainingRequestPreviewForTest();
                if (preview.value(QStringLiteral("updateTargetModelDir")).toString()
                        == managementModelDir) {
                    typedRetrainDialog = candidate;
                    break;
                }
            }
            check(typedRetrainDialog != nullptr, "retrain must open training dialog for selected model package");
            if (typedRetrainDialog) {
                const QJsonObject restoredPreview = typedRetrainDialog->buildTrainingRequestPreviewForTest();
                check(restoredPreview.value(QStringLiteral("restoredSessionStatus")).toString()
                          == QStringLiteral("restored"),
                      "retrain must restore the saved training session");
                check(restoredPreview.value(QStringLiteral("restoredImageNames")).toArray().size() >= 2,
                      "retrain must restore registration image names");
                check(restoredPreview.value(QStringLiteral("restoredClassNames")).toArray().size() == 2,
                      "retrain must restore class names");
                check(restoredPreview.value(QStringLiteral("restoredRoiCount")).toInt() >= 2,
                      "retrain must restore ROI marks");
                typedRetrainDialog->close();
            }
        }
        const QString legacyModelDir = QDir::temp().filePath(
                    QStringLiteral("registered_classification_legacy_model_without_session"));
        QDir(legacyModelDir).removeRecursively();
        QDir().mkpath(legacyModelDir);
        RegisteredClassificationTrainingDialog legacyRetrainDialog;
        legacyRetrainDialog.setUpdateTargetModelDir(legacyModelDir);
        const QJsonObject legacyPreview = legacyRetrainDialog.buildTrainingRequestPreviewForTest();
        check(legacyPreview.value(QStringLiteral("restoredSessionStatus")).toString()
                  == QStringLiteral("missing_training_session"),
              "legacy model without a session must report missing training history");
        check(legacyPreview.value(QStringLiteral("restoredSessionMessage")).toString()
                  .contains(QStringLiteral("没有历史训练数据")),
              "legacy model without a session must expose an actionable message");
        QDir(legacyModelDir).removeRecursively();
        QPushButton *useModelButton = scannedModelRow
                ? buttonByText(*scannedModelRow, QStringLiteral("使用模型"))
                : nullptr;
        check(useModelButton != nullptr, "model management must expose use-model action");
        if (useModelButton) {
            clickAndProcess(useModelButton);
            const QString selectedPath = dialog.toToolConfig()
                    .params.value(QStringLiteral("registeredClassification")).toObject()
                    .value(QStringLiteral("modelPath")).toString();
            check(selectedPath == managementModelDir,
                  "using a managed model must fill model package directory back to parent dialog");
            QPushButton *mainDeleteModelButton = buttonByText(dialog, QStringLiteral("删除模型"));
            if (mainDeleteModelButton)
                clickAndProcess(mainDeleteModelButton);
        }
        QPushButton *createDatasetButton = buttonByText(*managementDialog, QStringLiteral("创建数据集"));
        QPushButton *importDatasetButton = buttonByText(*managementDialog, QStringLiteral("导入"));
        QLabel *datasetTitle = labelByText(*managementDialog, QStringLiteral("数据集列表"));
        check(createDatasetButton != nullptr && importDatasetButton != nullptr,
              "model management dialog must show dataset actions");
        check(createDatasetButton && !createDatasetButton->icon().isNull(),
              "create dataset button must have an icon");
        check(importDatasetButton && !importDatasetButton->icon().isNull(),
              "import dataset button must have an icon");
        if (createDatasetButton && importDatasetButton && datasetTitle) {
            check(qAbs(createDatasetButton->mapTo(managementDialog, QPoint(0, 0)).y() -
                       datasetTitle->mapTo(managementDialog, QPoint(0, 0)).y()) < 24,
                  "create dataset button must be in dataset header row");
            check(qAbs(importDatasetButton->mapTo(managementDialog, QPoint(0, 0)).y() -
                       datasetTitle->mapTo(managementDialog, QPoint(0, 0)).y()) < 24,
                  "import dataset button must be in dataset header row");
        }
        if (createDatasetButton) {
            QTimer::singleShot(0, []() {
                QDialog *createDialog = topLevelDialogByTitle(QStringLiteral("创建数据集"));
                check(createDialog != nullptr, "create dataset dialog must open");
                if (!createDialog)
                    return;
                QFrame *contentFrame = createDialog->findChild<QFrame *>(
                            QStringLiteral("datasetDialogContent"));
                check(contentFrame &&
                      contentFrame->property("panelRole").toString() == QStringLiteral("datasetDialogContent"),
                      "create dataset dialog must use styled white content frame");
                check(widgetCenterIsLight(contentFrame),
                      "create dataset dialog content frame must render as light background");
                QPushButton *okButton = buttonByText(*createDialog, QStringLiteral("确定"));
                check(okButton != nullptr, "create dataset dialog must have confirm button");
                if (okButton)
                    okButton->click();
            });
            clickAndProcess(createDatasetButton);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QDialog *createdTrainingDialog = childDialogByTitle(dialog, QStringLiteral("注册分类"));
            check(createdTrainingDialog != nullptr,
                  "confirming create dataset must open training dialog");
            if (createdTrainingDialog)
                createdTrainingDialog->close();
        } else {
            managementDialog->close();
        }
    }

    QPushButton *referenceButton = buttonByText(dialog, QStringLiteral("基准图测试"));
    check(referenceButton != nullptr, "reference test button must exist");
    clickAndProcess(referenceButton);
    check(statusText(dialog).contains(QStringLiteral("model_path_empty")),
          "reference test must pass reference frame as request.image and reach model_path_empty");

    QPushButton *testRunButton = buttonByText(dialog, QStringLiteral("测试运行"));
    check(testRunButton != nullptr, "test run button must exist");
    clickAndProcess(testRunButton);
    check(statusText(dialog).contains(QStringLiteral("model_path_empty")),
          "test run must pass current camera frame as request.image and reach model_path_empty");

    ReferenceImageProvider::instance().clearReferenceFrame();
    CameraFrameProvider::instance().clearFrame();

    if (g_failures > 0) {
        std::cerr << g_failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "registered_classification_dialog_smoke: all checks passed" << std::endl;
    return 0;
}
