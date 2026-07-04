#include "RegisteredClassificationDialog.h"

#include "frame/CameraFrameProvider.h"
#include "frame/FrameViewHelper.h"
#include "frame/ReferenceImageProvider.h"

#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGraphicsView>
#include <QJsonObject>
#include <QLabel>
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

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication app(argc, argv);

    const cv::Mat frame = cv::Mat::zeros(64, 64, CV_8UC3);
    ReferenceImageProvider::instance().setReferenceFrame(frame);
    CameraFrameProvider::instance().setCurrentFrame(frame);

    RegisteredClassificationDialog dialog;
    dialog.show();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

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
        clickAndProcess(cameraCaptureButton);
        FrameViewHelper *trainingPreviewHelper = trainingDialog->findChild<FrameViewHelper *>(
                    QStringLiteral("registeredTrainingPreviewHelper"));
        check(trainingPreviewHelper && trainingPreviewHelper->hasImage(),
              "camera capture must display current camera frame in training preview");

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
    check(statusText(dialog).contains(QStringLiteral("no_model")),
          "reference test must pass reference frame as request.image and reach no_model");

    QPushButton *testRunButton = buttonByText(dialog, QStringLiteral("测试运行"));
    check(testRunButton != nullptr, "test run button must exist");
    clickAndProcess(testRunButton);
    check(statusText(dialog).contains(QStringLiteral("no_model")),
          "test run must pass current camera frame as request.image and reach no_model");

    ReferenceImageProvider::instance().clearReferenceFrame();
    CameraFrameProvider::instance().clearFrame();

    if (g_failures > 0) {
        std::cerr << g_failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "registered_classification_dialog_smoke: all checks passed" << std::endl;
    return 0;
}
