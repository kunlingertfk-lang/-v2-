#ifndef SCHEMESTORE_H
#define SCHEMESTORE_H

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QVector>

#include <opencv2/core.hpp>

#include "toolcore/ToolConfig.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolPreviewSnapshot.h"
#include "frame/FrameInputMetadata.h"

struct SchemeState
{
    int schemaVersion = 1;
    QString schemeId;
    QString schemeName;
    QString schemeDir;
    QString referenceImagePath;
    FrameInputMetadata referenceInputMetadata;
    ReferencePositionCorrectionConfig referencePositionCorrection;
    QVector<ToolConfig> toolConfigs;
    QMap<QString, ToolPreviewSnapshot> referencePreviewSnapshots;
    QJsonObject quickCalibrationConfig;
    QJsonObject outputConfig;
    QDateTime updatedAt;
};

struct SchemeSummary
{
    QString schemeId;
    QString schemeName;
    QString schemeDir;
    QString referenceImagePath;
    int toolCount = 0;
    QDateTime updatedAt;
};

class SchemeStore
{
public:
    static SchemeStore &instance();

    bool ensureLoaded(QString *errorMessage = nullptr);
    bool setCurrentScheme(const QString &schemeId, QString *errorMessage = nullptr);
    SchemeState loadScheme(const QString &schemeId, QString *errorMessage = nullptr) const;
    /// 原子保存当前方案；成功后以内存中的规范化状态替换当前运行态。
    bool saveCurrentScheme(QString *errorMessage = nullptr);
    bool saveScheme(const SchemeState &state, QString *errorMessage = nullptr);
    bool saveCurrentSchemeAs(const QString &schemeName, QString *errorMessage = nullptr);
    SchemeState createEmptyScheme(const QString &schemeName, QString *errorMessage = nullptr);
    bool schemeExists(const QString &schemeId) const;

    const SchemeState &currentScheme() const;
    QVector<SchemeState> availableSchemes() const;
    QList<SchemeSummary> listSchemes() const;
    QString projectsRootPath() const;
    QString currentSchemeName() const;
    QString currentReferenceImageAbsolutePath() const;

    void setSchemeName(const QString &schemeName);
    /// 更新方案工具与基准预览快照；调用方仍需 saveCurrentScheme 才会落盘。
    void setToolConfigs(const QVector<ToolConfig> &configs,
                        const QMap<QString, ToolPreviewSnapshot> &referenceSnapshots);
    /// 更新快速标定稳定配置，不包含点表草稿和通信运行态。
    void setQuickCalibrationConfig(const QJsonObject &quickCalibrationConfig);
    void setOutputConfig(const QJsonObject &outputConfig);
    void setReferencePositionCorrection(const ReferencePositionCorrectionConfig &config);

    bool setReferenceFrame(
            const cv::Mat &frame,
            QString *errorMessage = nullptr,
            const FrameInputMetadata &metadata = FrameInputMetadata());
    bool loadCurrentReferenceIntoProvider(QString *errorMessage = nullptr);

private:
    SchemeStore() = default;

    bool loadSchemeFromFile(const QString &schemeJsonPath,
                            SchemeState *state,
                            QString *errorMessage = nullptr) const;
    /// 归档标定资产、可选基准图并通过 QSaveFile 写入规范化 scheme.json。
    bool saveSchemeToFile(const SchemeState &state,
                          QString *errorMessage = nullptr,
                          const cv::Mat *referenceFrame = nullptr,
                          SchemeState *savedState = nullptr) const;
    bool refreshAvailableSchemes(QString *errorMessage = nullptr);
    bool createDefaultScheme(QString *errorMessage = nullptr);
    QString resolveProjectRootPath() const;
    QString schemeJsonPath(const SchemeState &state) const;
    QString makeUniqueSchemeId() const;
    SchemeState normalizedStateForSave(SchemeState state) const;
    void normalizeSnapshotsForCurrentTools();
    void setError(QString *errorMessage, const QString &message) const;

    bool m_loaded = false;
    SchemeState m_currentScheme;
    QVector<SchemeState> m_availableSchemes;
};

#endif // SCHEMESTORE_H
