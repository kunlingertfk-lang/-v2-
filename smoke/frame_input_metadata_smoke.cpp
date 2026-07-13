#include "SchemeStore.h"
#include "frame/CameraFrameProvider.h"
#include "frame/ReferenceImageProvider.h"
#include "toolcore/ToolEngine.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

#include <opencv2/core.hpp>

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

    CameraFrameProvider::instance().setCurrentFrame(cv::Mat(8, 8, CV_8UC1));
    check(CameraFrameProvider::instance().currentFrame().channels() == 3,
          "camera display frame may be normalized to BGR");
    check(CameraFrameProvider::instance().currentFrameMetadata().isMono(),
          "camera metadata must be inferred before BGR normalization");

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
        {QStringLiteral("schemeName"), QStringLiteral("Legacy metadata smoke")}
    };
    legacyFile.write(QJsonDocument(legacyJson).toJson(QJsonDocument::Compact));
    legacyFile.close();
    const SchemeState legacyLoaded = store.loadScheme(legacySchemeId, &error);
    check(legacyLoaded.referenceInputMetadata.colorMode == QStringLiteral("unknown"),
          "legacy schemes without metadata must remain unknown");

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
