#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QScreen>
#include <QTextStream>

#include "LoginWindow.h"
#include "algorithms/halcon/HalconRuntimePaths.h"
#include "frame/CameraFrameProvider.h"

namespace {

QString gLogPath;

void appMessageHandler(QtMsgType type,
                       const QMessageLogContext &,
                       const QString &message)
{
    static QMutex mutex;
    const QMutexLocker locker(&mutex);

    const char *level = "INFO";
    if (type == QtWarningMsg)
        level = "WARN";
    else if (type == QtCriticalMsg || type == QtFatalMsg)
        level = "ERROR";
    else if (type == QtDebugMsg)
        level = "DEBUG";

    const QByteArray localMessage = message.toLocal8Bit();
    fprintf(stderr, "[%s] %s\n", level, localMessage.constData());
    fflush(stderr);

    if (!gLogPath.isEmpty()) {
        QFile file(gLogPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                   << " [" << level << "] " << message << '\n';
        }
    }

    if (type == QtFatalMsg)
        abort();
}

void loadStyleSheet(QApplication &app)
{
    QFile file(":/styles/app.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(file.readAll()));
    }
}

void logScreenDpi(QScreen *screen)
{
    if (!screen)
        return;

    qInfo().nospace()
            << "[Display] name=" << screen->name()
            << " geometry=" << screen->geometry()
            << " available=" << screen->availableGeometry()
            << " logicalDpi=" << screen->logicalDotsPerInch()
            << " physicalDpi=" << screen->physicalDotsPerInch()
            << " devicePixelRatio=" << screen->devicePixelRatio();
}

} // namespace

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 5 does not consistently enable high-DPI scaling on Linux. These
    // attributes must be set before QApplication is constructed so widget
    // geometry, fonts, stylesheets and pixmaps all use logical pixels.
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    // Preserve common desktop scale factors such as 125% and 150% instead of
    // rounding them to an integer device-pixel ratio.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
            Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

    const QString halconLicense =
            HalconRuntimePaths::initializeHalconEnvironment();

    QApplication app(argc, argv);
    gLogPath = QCoreApplication::applicationDirPath()
            + QStringLiteral("/qt_ui_test.log");
    qInstallMessageHandler(appMessageHandler);

    if (halconLicense.isEmpty())
        qWarning() << "HALCON license file was not found";
    else
        qInfo() << "HALCON license file:" << halconLicense;

    app.setApplicationName(QStringLiteral("HZZH"));
    app.setOrganizationName(QStringLiteral("汇众智慧"));

    loadStyleSheet(app);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    qInfo().nospace()
            << "[Display] highDpiScaling="
            << QCoreApplication::testAttribute(Qt::AA_EnableHighDpiScaling)
            << " highDpiPixmaps="
            << QCoreApplication::testAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens)
        logScreenDpi(screen);
    QObject::connect(&app, &QGuiApplication::screenAdded, &app, [](QScreen *screen) {
        logScreenDpi(screen);
    });
    QObject::connect(&app, &QApplication::aboutToQuit, []() {
    CameraFrameProvider::instance().closeCamera(
                QStringLiteral("aboutToQuit"));
    });

    LoginWindow loginWindow;
    loginWindow.showMaximized();

    return app.exec();
}
