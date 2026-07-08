#include "RegisteredClassificationDetectionDialog.h"
#include "RegisteredClassificationDetectionTrainingDialog.h"
#include "ToolLibraryDialog.h"

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
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

QPushButton *buttonByText(QWidget &root, const QString &text)
{
    const QList<QPushButton *> buttons = root.findChildren<QPushButton *>();
    for (QPushButton *button : buttons) {
        if (button && button->text() == text)
            return button;
    }
    return nullptr;
}

QToolButton *toolButtonByText(QWidget &root, const QString &text)
{
    const QList<QToolButton *> buttons = root.findChildren<QToolButton *>();
    for (QToolButton *button : buttons) {
        if (button && button->text() == text)
            return button;
    }
    return nullptr;
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

QString statusText(QWidget &root)
{
    const QList<QLabel *> labels = root.findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label && label->text().startsWith(QStringLiteral("注册目标检测:")))
            return label->text();
    }
    return QString();
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

void clickAndProcess(QAbstractButton *button)
{
    if (!button)
        return;
    button->click();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    const cv::Mat frame = cv::Mat::zeros(64, 64, CV_8UC3);
    ReferenceImageProvider::instance().setReferenceFrame(frame);
    CameraFrameProvider::instance().setCurrentFrame(frame);

    check(toolTypeToString(ToolType::RegisteredClassificationDetection)
          == QStringLiteral("RegisteredClassificationDetection"),
          "tool type string must include RegisteredClassificationDetection");
    check(toolTypeFromString(QStringLiteral("注册目标检测"))
          == ToolType::RegisteredClassificationDetection,
          "tool type parser must map Chinese target detection name");
    check(toolTypeFromString(QStringLiteral("注册分类检测"))
          == ToolType::RegisteredClassificationDetection,
          "tool type parser must keep legacy Chinese detection name compatible");

    ToolLibraryDialog library;
    library.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QToolButton *libraryButton = toolButtonByText(library, QStringLiteral("注册目标检测"));
    check(libraryButton != nullptr, "tool library must expose registered target detection button");
    clickAndProcess(libraryButton);
    QPushButton *confirmButton = buttonByText(library, QStringLiteral("确定"));
    clickAndProcess(confirmButton);
    check(library.selectedToolType() == ToolType::RegisteredClassificationDetection,
          "tool library selection must resolve to RegisteredClassificationDetection");

    RegisteredClassificationDetectionDialog dialog;
    dialog.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    check(dialog.windowTitle() == QStringLiteral("方案编辑 - 注册目标检测"),
          "dialog title must identify registered target detection");
    check(labelByText(dialog, QStringLiteral("注册目标检测")) != nullptr,
          "left panel title must identify registered target detection");
    check(labelByText(dialog, QStringLiteral("模型训练")) != nullptr,
          "model training card title must exist");
    check(labelByText(dialog, QStringLiteral("检测区域")) != nullptr,
          "detect region card title must exist");

    QCheckBox *positionSwitch = dialog.findChild<QCheckBox *>(QStringLiteral("positionCorrectionSwitch"));
    check(positionSwitch != nullptr, "position correction switch must exist");
    check(positionSwitch && !positionSwitch->isChecked(),
          "position correction switch must default to unchecked");
    check(positionSwitch && !positionSwitch->isEnabled(),
          "position correction switch must be disabled until implemented");
    check(positionSwitch && positionSwitch->toolTip()
          == QStringLiteral("位置修正补偿尚未实现，当前版本默认关闭。"),
          "position correction tooltip must explain disabled state");

    QPushButton *allParamsButton = buttonByText(dialog, QStringLiteral("全部"));
    check(allParamsButton != nullptr, "all params segment button must exist");
    clickAndProcess(allParamsButton);
    check(labelByText(dialog, QStringLiteral("参数设置")) != nullptr,
          "all params card must be visible and named parameter settings");
    check(labelByText(dialog, QStringLiteral("前K个类别")) != nullptr,
          "parameter settings must contain top K class field");
    check(labelByText(dialog, QStringLiteral("最小相似度")) != nullptr,
          "parameter settings must contain min similarity field");
    check(comboBoxWithText(dialog, QStringLiteral("所有检测区域输出结果为 OK")) != nullptr,
          "judge type combo must contain all-ok option");
    check(comboBoxWithText(dialog, QStringLiteral("任意检测区域输出结果为 OK")) != nullptr,
          "judge type combo must contain any-ok option");

    ToolConfig config = dialog.toToolConfig();
    check(config.toolType == ToolType::RegisteredClassificationDetection,
          "config must save detection tool type");
    check(config.displayName == QStringLiteral("注册目标检测"),
          "config display name must save target detection label");
    const QJsonObject params =
            config.params.value(QStringLiteral("registeredClassificationDetection")).toObject();
    check(!params.isEmpty(), "config must save registeredClassificationDetection params namespace");
    check(params.value(QStringLiteral("detectRegionType")).toString() == QStringLiteral("full"),
          "default detect region must be full");
    check(params.value(QStringLiteral("minSimilarity")).toInt(-1) == 68,
          "default config must save minSimilarity=68");
    check(!params.value(QStringLiteral("enablePositionCorrection")).toBool(true),
          "default config must save enablePositionCorrection=false");
    check(config.judgeRule.value(QStringLiteral("judgeType")).toString() == QStringLiteral("all_ok"),
          "default judgeRule must save judgeType=all_ok");

    QToolButton *rectButton = nullptr;
    const QList<QToolButton *> toolButtons = dialog.findChildren<QToolButton *>();
    for (QToolButton *button : toolButtons) {
        if (button && button->toolTip() == QStringLiteral("矩形检测区域"))
            rectButton = button;
    }
    check(rectButton != nullptr, "rectangle ROI button must exist");
    clickAndProcess(rectButton);
    ToolConfig rectConfig = dialog.toToolConfig();
    const QJsonObject rectParams =
            rectConfig.params.value(QStringLiteral("registeredClassificationDetection")).toObject();
    check(rectParams.value(QStringLiteral("detectRegionType")).toString() == QStringLiteral("rectangle"),
          "rectangle ROI mode must save rectangle detect region");

    RegisteredClassificationDetectionDialog loadedDialog;
    loadedDialog.loadFromConfig(rectConfig);
    ToolConfig loadedConfig = loadedDialog.toToolConfig();
    const QJsonObject loadedParams =
            loadedConfig.params.value(QStringLiteral("registeredClassificationDetection")).toObject();
    check(loadedParams.value(QStringLiteral("detectRegionType")).toString() == QStringLiteral("rectangle"),
          "loadFromConfig must restore rectangle detect region");

    CameraFrameProvider::instance().clearFrame();
    QPushButton *testRunButton = buttonByText(dialog, QStringLiteral("测试运行"));
    clickAndProcess(testRunButton);
    check(statusText(dialog).contains(QStringLiteral("image_empty")),
          "test run with empty camera frame must show image_empty status");
    CameraFrameProvider::instance().setCurrentFrame(frame);
    clickAndProcess(testRunButton);
    check(statusText(dialog).contains(QStringLiteral("backend_not_implemented")),
          "test run with image must show backend_not_implemented placeholder");

    QPushButton *registerTrainingButton = buttonByText(dialog, QStringLiteral("注册训练"));
    clickAndProcess(registerTrainingButton);
    QDialog *trainingDialog = childDialogByTitle(dialog, QStringLiteral("注册目标检测"));
    check(trainingDialog != nullptr,
          "register training button must open dedicated detection training dialog");
    if (trainingDialog) {
        const QString trainingStyle = trainingDialog->styleSheet();
        check(trainingStyle.contains(QStringLiteral("QDialog{background:#ffffff;color:#0f172a;font-size:18px;}")),
              "detection training dialog must use white high-contrast root style");
        check(trainingStyle.contains(QStringLiteral("QFrame[panelRole=\"previewPanel\"],QFrame[panelRole=\"trainingCard\"]{background:#ffffff;border:3px solid #60a5fa;border-radius:8px;}")),
              "detection training preview panel and cards must use registered classification training borders");
        check(trainingStyle.contains(QStringLiteral("QPushButton[actionRole=\"primary\"]{background:#ff7a00;color:#ffffff;border-color:#ff7a00;font-size:20px;font-weight:800;}")),
              "detection training primary button must use registered classification training style");
        check(buttonByText(*trainingDialog, QStringLiteral("相机抓图")) != nullptr,
              "detection training dialog must have camera capture button");
        check(buttonByText(*trainingDialog, QStringLiteral("存图导入")) != nullptr,
              "detection training dialog must have stored image import button");
        check(buttonByText(*trainingDialog, QStringLiteral("外部导入")) != nullptr,
              "detection training dialog must have external import button");
        check(toolButtonByText(*trainingDialog, QStringLiteral("全屏框选")) == nullptr,
              "detection training dialog must not expose full ROI button");
        QToolButton *rectMarkButton = toolButtonByText(*trainingDialog, QStringLiteral("矩形框选"));
        QToolButton *polygonMarkButton = toolButtonByText(*trainingDialog, QStringLiteral("多边形框选"));
        check(rectMarkButton != nullptr, "detection training dialog must expose rectangle mark button");
        check(polygonMarkButton != nullptr, "detection training dialog must expose polygon mark button");
        check(labelByText(*trainingDialog, QStringLiteral("目标列表")) != nullptr,
              "detection training dialog must use target list title");
        check(labelByText(*trainingDialog, QStringLiteral("Target")) != nullptr,
              "detection training dialog must default to Target label");
        check(buttonByText(*trainingDialog, QStringLiteral("+ 新建")) == nullptr,
              "detection training dialog must not expose classification new-class button");
        check(labelByText(*trainingDialog, QStringLiteral("角度使能")) != nullptr,
              "detection training dialog must expose angle enable switch label");
        check(labelByText(*trainingDialog, QStringLiteral("最优模型分辨率设置使能")) != nullptr,
              "detection training dialog must expose best resolution switch label");

        QPushButton *trainButton = buttonByText(*trainingDialog, QStringLiteral("开始训练"));
        check(trainButton != nullptr, "detection training dialog must have start train button");
        clickAndProcess(trainButton);
        check(labelByText(*trainingDialog, QStringLiteral("请先添加注册图")) != nullptr,
              "start training without images must show add image validation");

        QPushButton *cameraCaptureButton = buttonByText(*trainingDialog, QStringLiteral("相机抓图"));
        clickAndProcess(cameraCaptureButton);
        QListWidget *thumbnailList = trainingDialog->findChild<QListWidget *>(
                    QStringLiteral("registeredDetectionTrainingThumbnailList"));
        check(thumbnailList != nullptr, "detection training dialog must have thumbnail list");
        check(thumbnailList && thumbnailList->count() == 1,
              "camera capture must add one detection training thumbnail");
        check(rectMarkButton && rectMarkButton->isEnabled(),
              "rectangle mark button must enable after adding an image");
        clickAndProcess(rectMarkButton);
        check(rectMarkButton && rectMarkButton->isChecked(),
              "rectangle mark button must stay checked after entering continuous ROI mode");
        check(polygonMarkButton && !polygonMarkButton->isChecked(),
              "polygon mark button must remain unchecked when rectangle mode is active");

        FrameViewHelper *trainingPreviewHelper = trainingDialog->findChild<FrameViewHelper *>(
                    QStringLiteral("registeredDetectionTrainingPreviewHelper"));
        check(trainingPreviewHelper && trainingPreviewHelper->hasImage(),
              "camera capture must display image in detection training preview");
        if (trainingPreviewHelper) {
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.15, 0.15, 0.30, 0.25));
            emit trainingPreviewHelper->roiChanged(QRectF(0.15, 0.15, 0.30, 0.25));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        check(rectMarkButton && rectMarkButton->isChecked(),
              "rectangle mark button must remain checked after drawing one ROI");
        check(labelByText(*trainingDialog, QStringLiteral("1 / 1")) != nullptr,
              "drawing a rectangle must update Target count to one target on one image");
        check(trainingDialog->findChild<QLabel *>(QStringLiteral("registeredDetectionTrainingRoiNumberLabel_0")) != nullptr,
              "drawing a rectangle must show ROI id label");
        if (trainingPreviewHelper) {
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.45, 0.20, 0.20, 0.20));
            emit trainingPreviewHelper->roiChanged(QRectF(0.45, 0.20, 0.20, 0.20));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        check(labelByText(*trainingDialog, QStringLiteral("2 / 1")) != nullptr,
              "continuous rectangle mode must allow drawing a second target without clicking the button again");
        clickAndProcess(rectMarkButton);
        check(rectMarkButton && !rectMarkButton->isChecked(),
              "clicking rectangle mark button again must exit ROI mode");
        clickAndProcess(polygonMarkButton);
        check(polygonMarkButton && polygonMarkButton->isChecked(),
              "polygon mark button must enter continuous ROI mode");
        check(rectMarkButton && !rectMarkButton->isChecked(),
              "rectangle mark button must be unchecked when polygon mode is active");
        clickAndProcess(polygonMarkButton);
        check(polygonMarkButton && !polygonMarkButton->isChecked(),
              "clicking polygon mark button again must exit ROI mode");

        clickAndProcess(cameraCaptureButton);
        check(thumbnailList && thumbnailList->count() == 2,
              "camera capture must add a second detection training thumbnail");
        QComboBox *thumbnailFilter = trainingDialog->findChild<QComboBox *>(
                    QStringLiteral("registeredDetectionTrainingFilterCombo"));
        check(thumbnailFilter != nullptr,
              "thumbnail area must expose all/marked/unmarked filter combo");
        if (thumbnailFilter) {
            thumbnailFilter->setCurrentText(QStringLiteral("标注"));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            check(thumbnailList && thumbnailList->count() == 1,
                  "marked filter must show only thumbnails with ROI marks");
            thumbnailFilter->setCurrentText(QStringLiteral("未标注"));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            check(thumbnailList && thumbnailList->count() == 1,
                  "unmarked filter must show only thumbnails without ROI marks");
            thumbnailFilter->setCurrentText(QStringLiteral("全部"));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            check(thumbnailList && thumbnailList->count() == 2,
                  "all filter must show all thumbnails");
        }

        QToolButton *deleteSecondImageButton = trainingDialog->findChild<QToolButton *>(
                    QStringLiteral("registeredDetectionTrainingDeleteImageButton_1"));
        check(deleteSecondImageButton != nullptr,
              "thumbnail must expose single registered image delete button");
        clickAndProcess(deleteSecondImageButton);
        check(thumbnailList && thumbnailList->count() == 1,
              "single thumbnail delete must remove one registered image");
        check(labelByText(*trainingDialog, QStringLiteral("2 / 1")) != nullptr,
              "single thumbnail delete must remove its ROI state and keep marked image count consistent");

        QToolButton *deleteAllImagesButton = trainingDialog->findChild<QToolButton *>(
                    QStringLiteral("registeredDetectionTrainingDeleteAllImagesButton"));
        check(deleteAllImagesButton != nullptr,
              "thumbnail area must expose delete-all registered images button");
        QTimer::singleShot(0, []() {
            const QList<QWidget *> widgets = QApplication::topLevelWidgets();
            for (QWidget *widget : widgets) {
                QMessageBox *box = qobject_cast<QMessageBox *>(widget);
                if (!box)
                    continue;
                QAbstractButton *yesButton = box->button(QMessageBox::Yes);
                if (yesButton)
                    yesButton->click();
            }
        });
        clickAndProcess(deleteAllImagesButton);
        check(thumbnailList && thumbnailList->count() == 0,
              "delete-all confirmation must remove all registered image thumbnails");
        check(labelByText(*trainingDialog, QStringLiteral("请先添加注册图")) != nullptr,
              "delete-all confirmation must return training dialog to no-image state");

        clickAndProcess(cameraCaptureButton);
        clickAndProcess(rectMarkButton);
        if (trainingPreviewHelper) {
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.15, 0.15, 0.30, 0.25));
            emit trainingPreviewHelper->roiChanged(QRectF(0.15, 0.15, 0.30, 0.25));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }

        QToolButton *previewTargetButton = trainingDialog->findChild<QToolButton *>(
                    QStringLiteral("registeredDetectionTrainingPreviewTargetButton_0"));
        check(previewTargetButton != nullptr,
              "target row must expose preview button with stable object name");
        clickAndProcess(previewTargetButton);
        QWidget *previewPage = trainingDialog->findChild<QWidget *>(
                    QStringLiteral("registeredDetectionTrainingPreviewPage"));
        check(previewPage && previewPage->isVisible(),
              "target preview button must open target ROI preview page");
        QToolButton *deleteRoiButton = trainingDialog->findChild<QToolButton *>(
                    QStringLiteral("registeredDetectionTrainingDeleteRoiButton_0_0"));
        check(deleteRoiButton != nullptr,
              "target ROI preview card must expose delete ROI button");
        clickAndProcess(deleteRoiButton);
        check(labelByText(*trainingDialog, QStringLiteral("0 / 0")) != nullptr,
              "deleting target ROI must update target count to zero");
        check(trainingDialog->findChild<QWidget *>(
                  QStringLiteral("registeredDetectionTrainingRoiPreviewCard_0_0")) == nullptr,
              "deleting target ROI must remove preview card");

        clickAndProcess(trainButton);
        check(labelByText(*trainingDialog, QStringLiteral("请先标注目标")) != nullptr,
              "start training after deleting all ROI must request target marks");
        clickAndProcess(previewTargetButton);
        clickAndProcess(rectMarkButton);
        if (trainingPreviewHelper) {
            trainingPreviewHelper->setRoiRectNormalized(QRectF(0.25, 0.25, 0.20, 0.20));
            emit trainingPreviewHelper->roiChanged(QRectF(0.25, 0.25, 0.20, 0.20));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        clickAndProcess(trainButton);
        check(labelByText(*trainingDialog,
                          QStringLiteral("训练算法未接入，当前仅完成 UI 标注闭环。")) != nullptr,
              "start training with marks must show backend-not-implemented status");
    }

    QPushButton *modelManagementButton = buttonByText(dialog, QStringLiteral("模型管理"));
    clickAndProcess(modelManagementButton);
    check(childDialogByTitle(dialog, QStringLiteral("模型训练")) != nullptr,
          "model management button must open existing model management placeholder dialog");

    return g_failures == 0 ? 0 : 1;
}
