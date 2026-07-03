#include "RegisteredClassificationDialog.h"

#include "frame/CameraFrameProvider.h"
#include "frame/ReferenceImageProvider.h"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
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

void clickAndProcess(QPushButton *button)
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
