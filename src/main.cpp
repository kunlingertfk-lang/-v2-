#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QMutex>
#include <QMutexLocker>
#include <QTextCodec>
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

    const QByteArray utf8Message = message.toUtf8();
    fprintf(stderr, "[%s] %s\n", level, utf8Message.constData());
    fflush(stderr);

    if (!gLogPath.isEmpty()) {
        QFile file(gLogPath);
        if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setCodec("UTF-8");
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

void loadApplicationFonts(QApplication &app)
{
    const int fontId = QFontDatabase::addApplicationFont(
                QStringLiteral(":/fonts/ALIMAMASHUHEITI-BOLD.OTF"));
    if (fontId < 0) {
        qWarning() << "Failed to load bundled Chinese font.";
        return;
    }

    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (families.isEmpty()) {
        qWarning() << "Bundled Chinese font has no family name.";
        return;
    }

    QFont font(families.first());
    font.setPointSize(10);
    app.setFont(font);
    qInfo() << "Loaded bundled Chinese font:" << families.first();
}

} // namespace

int main(int argc, char *argv[])
{
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

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

    loadApplicationFonts(app);
    loadStyleSheet(app);
    QObject::connect(&app, &QApplication::aboutToQuit, []() {
        CameraFrameProvider::instance().stopGrab();
        CameraFrameProvider::instance().closeCamera(QStringLiteral("aboutToQuit"));
    });

    LoginWindow loginWindow;
    loginWindow.show();

    return app.exec();
}
