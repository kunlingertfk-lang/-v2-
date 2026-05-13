#include <QApplication>
#include <QFile>

#include "LoginWindow.h"
#include "frame/CameraFrameProvider.h"

namespace {

void loadStyleSheet(QApplication &app)
{
    QFile file(":/styles/app.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(file.readAll()));
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("HZZH"));
    app.setOrganizationName(QStringLiteral("汇众智慧"));

    loadStyleSheet(app);
    QObject::connect(&app, &QApplication::aboutToQuit, []() {
        CameraFrameProvider::instance().stopGrab();
        CameraFrameProvider::instance().closeCamera();
    });

    LoginWindow loginWindow;
    loginWindow.show();

    return app.exec();
}
