#include "SchemeStore.h"
#include "frame/ReferenceImageProvider.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>

#include <opencv2/imgcodecs.hpp>

#include <iostream>

namespace {

int fail(const QString &message)
{
    std::cerr << message.toStdString() << std::endl;
    return 1;
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool sameColor(const cv::Mat &image, const cv::Scalar &bgr)
{
    if (image.empty() || image.channels() != 3)
        return false;
    const cv::Mat expected(image.size(), image.type(), bgr);
    return cv::norm(image, expected, cv::NORM_INF) == 0.0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir workspace;
    if (!workspace.isValid())
        return fail(QStringLiteral("temporary workspace creation failed"));

    QFile marker(QDir(workspace.path()).filePath(QStringLiteral("qt_ui_test.pro")));
    if (!marker.open(QIODevice::WriteOnly))
        return fail(QStringLiteral("project marker creation failed"));
    marker.close();
    if (!QDir::setCurrent(workspace.path()))
        return fail(QStringLiteral("could not enter temporary workspace"));

    SchemeStore &store = SchemeStore::instance();
    QString error;
    if (!store.ensureLoaded(&error))
        return fail(QStringLiteral("scheme initialization failed: %1").arg(error));

    const cv::Mat red(12, 16, CV_8UC3, cv::Scalar(0, 0, 255));
    if (!store.setReferenceFrame(red, &error))
        return fail(QStringLiteral("first reference save failed: %1").arg(error));
    if (store.currentScheme().referenceImagePath != QStringLiteral("reference_a.png"))
        return fail(QStringLiteral("first reference did not use slot A"));

    const QString schemeDir = store.currentScheme().schemeDir;
    const QString jsonPath = QDir(schemeDir).filePath(QStringLiteral("scheme.json"));
    const QString slotAPath = QDir(schemeDir).filePath(QStringLiteral("reference_a.png"));
    if (!sameColor(cv::imread(slotAPath.toStdString()), cv::Scalar(0, 0, 255)))
        return fail(QStringLiteral("slot A content mismatch"));

    const cv::Mat green(12, 16, CV_8UC3, cv::Scalar(0, 255, 0));
    if (!store.setReferenceFrame(green, &error))
        return fail(QStringLiteral("second reference save failed: %1").arg(error));
    if (store.currentScheme().referenceImagePath != QStringLiteral("reference_b.png"))
        return fail(QStringLiteral("second reference did not switch to slot B"));

    const QString slotBPath = QDir(schemeDir).filePath(QStringLiteral("reference_b.png"));
    if (!sameColor(cv::imread(slotBPath.toStdString()), cv::Scalar(0, 255, 0)))
        return fail(QStringLiteral("slot B content mismatch"));
    if (!sameColor(cv::imread(slotAPath.toStdString()), cv::Scalar(0, 0, 255)))
        return fail(QStringLiteral("slot A was overwritten before JSON switched"));

    const QByteArray jsonBeforeFailure = readAll(jsonPath);
    const SchemeState stateBeforeFailure = store.currentScheme();
    const cv::Mat providerBeforeFailure =
            ReferenceImageProvider::instance().referenceFrame();

    if (!QFile::setPermissions(
                schemeDir,
                QFileDevice::ReadOwner | QFileDevice::ExeOwner)) {
        return fail(QStringLiteral("could not make scheme directory read-only"));
    }
    const cv::Mat blue(12, 16, CV_8UC3, cv::Scalar(255, 0, 0));
    const bool failedSaveResult = store.setReferenceFrame(blue, &error);
    QFile::setPermissions(
                schemeDir,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    if (failedSaveResult)
        return fail(QStringLiteral("failure injection unexpectedly succeeded"));
    if (store.currentScheme().referenceImagePath
            != stateBeforeFailure.referenceImagePath) {
        return fail(QStringLiteral("failed save changed in-memory reference path"));
    }
    if (readAll(jsonPath) != jsonBeforeFailure)
        return fail(QStringLiteral("failed save changed scheme JSON"));
    if (cv::norm(ReferenceImageProvider::instance().referenceFrame(),
                 providerBeforeFailure,
                 cv::NORM_INF) != 0.0) {
        return fail(QStringLiteral("failed save changed reference provider"));
    }

    const QString persistedName = store.currentSchemeName();
    const QByteArray jsonBeforeConfigFailure = readAll(jsonPath);
    if (!QFile::setPermissions(
                schemeDir,
                QFileDevice::ReadOwner | QFileDevice::ExeOwner)) {
        return fail(QStringLiteral("could not inject config save failure"));
    }
    store.setSchemeName(QStringLiteral("must-not-survive"));
    const bool configSaveResult = store.saveCurrentScheme(&error);
    QFile::setPermissions(
                schemeDir,
                QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    if (configSaveResult)
        return fail(QStringLiteral("config failure injection unexpectedly succeeded"));
    if (store.currentSchemeName() != persistedName)
        return fail(QStringLiteral("failed config save did not restore in-memory state"));
    if (readAll(jsonPath) != jsonBeforeConfigFailure)
        return fail(QStringLiteral("failed config save changed scheme JSON"));

    ReferenceImageProvider::instance().setReferenceFrame(red);
    if (!store.saveCurrentScheme(&error))
        return fail(QStringLiteral("ordinary scheme save failed: %1").arg(error));
    if (!sameColor(cv::imread(slotBPath.toStdString()), cv::Scalar(0, 255, 0)))
        return fail(QStringLiteral("ordinary save rewrote reference from global provider"));

    if (!store.saveCurrentSchemeAs(QStringLiteral("transaction-copy"), &error))
        return fail(QStringLiteral("save-as failed: %1").arg(error));
    if (!sameColor(cv::imread(store.currentReferenceImageAbsolutePath().toStdString()),
                   cv::Scalar(0, 255, 0))) {
        return fail(QStringLiteral("save-as did not copy persisted reference"));
    }
    if (!sameColor(ReferenceImageProvider::instance().referenceFrame(),
                   cv::Scalar(0, 255, 0))) {
        return fail(QStringLiteral("save-as left provider inconsistent with current scheme"));
    }

    SchemeState outsideState = store.currentScheme();
    outsideState.schemeId = QStringLiteral("outside_scheme");
    outsideState.schemeName = QStringLiteral("outside");
    outsideState.schemeDir =
            QDir(workspace.path()).filePath(QStringLiteral("outside_scheme"));
    outsideState.referenceImagePath.clear();
    if (store.saveScheme(outsideState, &error))
        return fail(QStringLiteral("out-of-root scheme directory was accepted"));
    if (QFileInfo::exists(QDir(outsideState.schemeDir)
                          .filePath(QStringLiteral("scheme.json")))) {
        return fail(QStringLiteral("out-of-root scheme file was created"));
    }

    const QString traversalId = QStringLiteral("traversal_scheme");
    const QString traversalDir =
            QDir(store.projectsRootPath()).filePath(traversalId);
    if (!QDir().mkpath(traversalDir))
        return fail(QStringLiteral("could not create traversal fixture"));
    QJsonObject traversalJson;
    traversalJson.insert(QStringLiteral("schemaVersion"), 1);
    traversalJson.insert(QStringLiteral("schemeId"), traversalId);
    traversalJson.insert(QStringLiteral("schemeName"), QStringLiteral("traversal"));
    traversalJson.insert(QStringLiteral("referenceImage"), QStringLiteral("../outside.png"));
    QFile traversalFile(QDir(traversalDir).filePath(QStringLiteral("scheme.json")));
    if (!traversalFile.open(QIODevice::WriteOnly)
            || traversalFile.write(QJsonDocument(traversalJson).toJson())
                    <= 0) {
        return fail(QStringLiteral("could not write traversal fixture"));
    }
    traversalFile.close();
    if (!store.loadScheme(traversalId, &error).schemeId.isEmpty())
        return fail(QStringLiteral("reference path traversal was accepted"));

    std::cout << "scheme_transaction_smoke: PASS" << std::endl;
    return 0;
}
