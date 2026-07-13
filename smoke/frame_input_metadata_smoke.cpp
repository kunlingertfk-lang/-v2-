#include "SchemeStore.h"
#include "frame/CameraFrameProvider.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

namespace {

int g_failures = 0;

void check(bool condition, const char *message)
{
    if (condition)
        return;

    std::cerr << "FAIL: " << message << std::endl;
    ++g_failures;
}

class SchemeDirectoryGuard
{
public:
    explicit SchemeDirectoryGuard(const QString &path)
        : m_path(path)
    {
    }

    ~SchemeDirectoryGuard()
    {
        QDir(m_path).removeRecursively();
    }

private:
    QString m_path;
};

class RuntimeContextAdapter : public ToolAdapter
{
public:
    bool supports(ToolType type) const override
    {
        return type == ToolType::ColorComparison;
    }

    ToolResult run(const ToolRequest &request) override
    {
        capturedRuntimeContext = request.runtimeContext;
        ToolResult result;
        result.toolId = request.config.toolId;
        result.toolType = request.config.toolType;
        result.success = true;
        result.ok = true;
        result.status = QStringLiteral("ok");
        return result;
    }

    QJsonObject capturedRuntimeContext;
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir isolatedProjectRoot;
    check(isolatedProjectRoot.isValid(),
          "metadata smoke must have an isolated project root");
    if (!isolatedProjectRoot.isValid())
        return 1;
    QFile projectMarker(QDir(isolatedProjectRoot.path())
                        .filePath(QStringLiteral("qt_ui_test.pro")));
    check(projectMarker.open(QIODevice::WriteOnly),
          "isolated project marker must be writable");
    projectMarker.close();
    check(QDir::setCurrent(isolatedProjectRoot.path()),
          "metadata smoke must enter its isolated project root");

    using SetReferenceFrameSignature = bool (SchemeStore::*)(
            const cv::Mat &, QString *, const FrameInputMetadata &);
    const SetReferenceFrameSignature setReferenceFrame =
            &SchemeStore::setReferenceFrame;
    check(setReferenceFrame != nullptr,
          "SchemeStore must accept explicit source metadata without breaking old callers");

    const FrameInputMetadata mono =
            FrameInputMetadata::fromMat(cv::Mat(8, 8, CV_8UC1),
                                        QStringLiteral("camera"));
    check(mono.isMono() && mono.pixelFormat == QStringLiteral("Mono8"),
          "CV_8UC1 must remain mono before BGR normalization");
    check(mono.originalChannels == 1 && mono.originalDepth == 8,
          "Mono8 metadata must retain its original channel count and bit depth");

    const FrameInputMetadata color =
            FrameInputMetadata::fromMat(cv::Mat(8, 8, CV_8UC3),
                                        QStringLiteral("camera"));
    check(color.isSupportedColor8(), "CV_8UC3 must be supported color8");
    check(FrameInputMetadata::fromJson(color.toJson()).pixelFormat
                  == QStringLiteral("BGR8"),
          "metadata must JSON round-trip");

    const FrameInputMetadata bgra =
            FrameInputMetadata::fromMat(cv::Mat(8, 8, CV_8UC4),
                                        QStringLiteral("camera"));
    check(bgra.colorMode == QStringLiteral("color")
                  && bgra.pixelFormat == QStringLiteral("BGRA8")
                  && bgra.originalChannels == 4
                  && bgra.originalDepth == 8,
          "CV_8UC4 must map to color BGRA8");

    const FrameInputMetadata mono1 = FrameInputMetadata::fromQImage(
            QImage(8, 8, QImage::Format_Mono), QStringLiteral("file"));
    check(mono1.isMono()
                  && mono1.pixelFormat == QStringLiteral("Mono1")
                  && mono1.originalChannels == 1
                  && mono1.originalDepth == 1,
          "QImage Format_Mono must map to mono Mono1");

    const FrameInputMetadata indexed = FrameInputMetadata::fromQImage(
            QImage(8, 8, QImage::Format_Indexed8), QStringLiteral("file"));
    check(indexed.colorMode == QStringLiteral("unknown")
                  && indexed.pixelFormat.isEmpty(),
          "QImage Format_Indexed8 must remain unknown");

    const FrameInputMetadata rgb888 = FrameInputMetadata::fromQImage(
            QImage(8, 8, QImage::Format_RGB888), QStringLiteral("file"));
    check(rgb888.colorMode == QStringLiteral("color")
                  && !rgb888.pixelFormat.isEmpty()
                  && rgb888.originalChannels == 3
                  && rgb888.originalDepth == 8,
          "QImage Format_RGB888 must map to explicit three-channel color8");

    const FrameInputMetadata argb32 = FrameInputMetadata::fromQImage(
            QImage(8, 8, QImage::Format_ARGB32), QStringLiteral("file"));
    check(argb32.colorMode == QStringLiteral("color")
                  && !argb32.pixelFormat.isEmpty()
                  && argb32.originalChannels == 4
                  && argb32.originalDepth == 8,
          "QImage ARGB32/BGRA-family input must map to explicit four-channel color8");

    const FrameInputMetadata uyvy =
            FrameInputMetadata::fromMat(cv::Mat(8, 8, CV_8UC2),
                                        QStringLiteral("camera"));
    check(uyvy.colorMode == QStringLiteral("color")
                  && uyvy.pixelFormat == QStringLiteral("UYVY8"),
          "CV_8UC2 must map to color UYVY8");

    const FrameInputMetadata unknown =
            FrameInputMetadata::fromMat(cv::Mat(8, 8, CV_16UC3),
                                        QStringLiteral("camera"));
    check(unknown.colorMode == QStringLiteral("unknown"),
          "unsupported Mat types must remain unknown");

    ReferenceImageProvider::instance().setReferenceFrame(
            cv::Mat(8, 8, CV_8UC1), mono);
    check(ReferenceImageProvider::instance().referenceFrame().channels() == 3,
          "display frame may be normalized to BGR");
    check(ReferenceImageProvider::instance().referenceFrameMetadata().isMono(),
          "reference metadata must preserve original mono source");
    const auto referenceSnapshot =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    check(referenceSnapshot.frame.channels() == 3
                  && referenceSnapshot.metadata.isMono(),
          "reference snapshot must return its cloned frame and metadata together");

    CameraFrameProvider::instance().setCurrentFrame(cv::Mat(8, 8, CV_8UC1));
    check(CameraFrameProvider::instance().currentFrame().channels() == 3,
          "camera display frame may be normalized to BGR");
    check(CameraFrameProvider::instance().currentFrameMetadata().isMono(),
          "camera metadata must be inferred before BGR normalization");
    const auto cameraSnapshot =
            CameraFrameProvider::instance().currentFrameSnapshot();
    check(cameraSnapshot.frame.channels() == 3
                  && cameraSnapshot.metadata.isMono()
                  && cameraSnapshot.frameIndex
                          == CameraFrameProvider::instance().currentFrameIndex(),
          "camera snapshot must return its cloned frame, index and metadata together");

    SchemeStore &store = SchemeStore::instance();
    const QString schemeId = QStringLiteral("frame_metadata_smoke_%1")
            .arg(QCoreApplication::applicationPid());
    const QString schemeDir = QDir(store.projectsRootPath()).filePath(schemeId);
    SchemeDirectoryGuard schemeGuard(schemeDir);

    SchemeState state;
    state.schemeId = schemeId;
    state.schemeName = QStringLiteral("Frame metadata smoke");
    state.schemeDir = schemeDir;
    state.referenceInputMetadata = mono;

    QString error;
    check(store.saveScheme(state, &error), "temporary metadata scheme must save");
    const SchemeState loaded = store.loadScheme(schemeId, &error);
    check(loaded.referenceInputMetadata.colorMode == QStringLiteral("mono"),
          "reference input metadata must survive scheme save and load");

    const QString legacySchemeId = schemeId + QStringLiteral("_legacy");
    const QString legacySchemeDir = QDir(store.projectsRootPath()).filePath(legacySchemeId);
    SchemeDirectoryGuard legacySchemeGuard(legacySchemeDir);
    QDir().mkpath(legacySchemeDir);
    QFile legacyFile(QDir(legacySchemeDir).filePath(QStringLiteral("scheme.json")));
    check(legacyFile.open(QIODevice::WriteOnly), "legacy scheme file must be writable");
    const QJsonObject legacyJson{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("schemeId"), legacySchemeId},
        {QStringLiteral("schemeName"), QStringLiteral("Legacy metadata smoke")},
        {QStringLiteral("referenceImage"), QStringLiteral("reference.png")}
    };
    legacyFile.write(QJsonDocument(legacyJson).toJson(QJsonDocument::Compact));
    legacyFile.close();
    check(cv::imwrite(QDir(legacySchemeDir).filePath(QStringLiteral("reference.png"))
                              .toStdString(),
                      cv::Mat(8, 8, CV_8UC3, cv::Scalar(24, 48, 96))),
          "legacy reference PNG must be writable");
    const SchemeState legacyLoaded = store.loadScheme(legacySchemeId, &error);
    check(legacyLoaded.referenceInputMetadata.colorMode == QStringLiteral("unknown"),
          "legacy schemes without metadata must remain unknown");
    QDir(schemeDir).removeRecursively();
    check(store.setCurrentScheme(legacySchemeId, &error),
          "isolated legacy scheme must become current");
    const ReferenceFrameSnapshot legacyReferenceSnapshot =
            ReferenceImageProvider::instance().referenceFrameSnapshot();
    check(legacyReferenceSnapshot.frame.channels() == 3
                  && legacyReferenceSnapshot.metadata.colorMode
                          == QStringLiteral("unknown"),
          "legacy BGR PNG reload must preserve unknown source metadata");

    RuntimeContextAdapter adapter;
    ToolEngine engine;
    engine.registerAdapter(&adapter);
    ToolConfig config;
    config.toolId = QStringLiteral("runtime-context-smoke");
    config.toolType = ToolType::ColorComparison;
    config.category = ToolCategory::Recognition;
    const QJsonObject runtimeContext{
        {QStringLiteral("input"), mono.toJson()},
        {QStringLiteral("referenceInput"), color.toJson()}
    };
    const QVector<ToolResult> results = engine.runTools(
            QVector<ToolConfig>{config}, cv::Mat(8, 8, CV_8UC3), cv::Mat(), runtimeContext);
    check(results.size() == 1
                  && adapter.capturedRuntimeContext == runtimeContext,
          "ToolEngine must pass runtime context unchanged to every request");

    CameraFrameProvider::instance().clearFrame();
    ReferenceImageProvider::instance().clearReferenceFrame();
    check(CameraFrameProvider::instance().currentFrameMetadata().colorMode
                  == QStringLiteral("unknown"),
          "clearing the camera frame must clear camera metadata");
    check(ReferenceImageProvider::instance().referenceFrameMetadata().colorMode
                  == QStringLiteral("unknown"),
          "clearing the reference frame must clear reference metadata");

    if (g_failures > 0) {
        std::cerr << g_failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "frame_input_metadata_smoke: all checks passed" << std::endl;
    return 0;
}
