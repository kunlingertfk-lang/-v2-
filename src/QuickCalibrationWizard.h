#ifndef QUICKCALIBRATIONWIZARD_H
#define QUICKCALIBRATIONWIZARD_H

#include "calibration/CalibrationMethodRegistry.h"
#include "calibration/CalibrationCommunicationProtocol.h"

#include <QDialog>
#include <QImage>
#include <QJsonObject>
#include <QMap>
#include <QStringList>

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolPreviewSnapshot.h"
#include "tooladapters/TemplateLocationAdapter.h"

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QSpinBox;
class QTableWidget;
class QToolButton;
class QVBoxLayout;
class QGraphicsView;
class QFrame;
class FrameViewHelper;

/// 快速标定四步向导：组织通信采样、N 点求解、XML 生成及目标标定转换关联。
class QuickCalibrationWizard : public QDialog
{
    Q_OBJECT
public:
    explicit QuickCalibrationWizard(QWidget *parent = nullptr);

    /// 返回本次已完成并归档到方案目录的标定 XML；未生成时为空。
    QString generatedFilePath() const;
    /// 返回用户勾选、等待向导关闭后应用新 XML 的标定转换工具 ID。
    QStringList targetCalibrationTransformIds() const;
    /// 导出可复用的通信与 N 点稳定配置，不包含点表草稿和运行态 socket。
    QJsonObject persistentConfiguration() const;
    /// 恢复方案级快速标定稳定配置；高版本或非法配置会进入只读门禁。
    void setPersistentConfiguration(const QJsonObject &configuration);
    /// 注入方案工具和预览快照，建立采样来源与可应用目标列表。
    void setProducerTools(const QVector<ToolConfig> &tools,
                          const QMap<QString, ToolPreviewSnapshot> &snapshots,
                          int selectedToolIndex = -1);
    /// 设置无相机/无外部图序列时使用的基准预览图。
    void setPreviewImage(const QImage &image);

private:
    /// 构建方式选择、通信、采样配置和结果四个页面。
    QWidget *createMethodPage();
    QWidget *createCommunicationPage();
    QWidget *createConfigurationPage();
    QWidget *createResultPage();
    QWidget *createStepHeader();
    /// 切换向导步骤；首次进入配置页询问草稿，首次给结果页设默认路径时刷新结果表。
    void setStep(int step);
    void updateStepHeader();
    /// 从配置控件生成草稿、验证已锁定来源指纹并通过注册方式执行 HALCON 求解。
    void solveCalibration();
    /// 通过质量门禁后写 XML 1.4、归档方案资产并记录待应用目标。
    void generateCalibrationFile();
    /// 把通信日志、样本残差、矩阵及旋转质量写入结果页。
    void showSolveResult();
    /// 将通信页面控件收集为协议与传输配置。
    CalibrationCommunicationConfig communicationConfig() const;
    /// 在离开通信页前校验方式需求及协议字段。
    bool validateCommunicationPage();
    /// 仅解析测试文本并更新最近机械量；不会触发模板定位和点表采样。
    void testCommunicationMessage();
    /// 根据当前会话状态启动或停止通信 Session。
    void toggleCommunicationSession();
    /// 处理 Session 已解析事件；Capture 需完成定位和落表才返回成功 ACK。
    bool handleCommunicationMessage(CalibrationCommunicationMessage *message,
                                    QString *errorMessage);
    /// 对当前相机帧/外部图/基准图运行绑定的模板定位并校验来源指纹。
    bool runCurrentImageLocation(QString *errorMessage);
    /// 外部图片采样成功后切至下一张，并在最后一张后锁定序列完成状态。
    void advanceExternalImageAfterCapture();
    /// 按当前 methodId 延迟创建配置控件并接入草稿自动保存信号。
    bool ensureMethodConfigWidget();
    void updatePreviewImage(const QImage &image);
    void updateCalibrationOverlays();
    bool calibrationResultPassed() const;
    /// 对比已锁定来源与当前模板配置、基准图、原点和已保存模型身份，阻止混源采样。
    bool validateCoordinateSourceFingerprint(
            const QJsonObject &fingerprint,
            QString *errorMessage = nullptr) const;
    void importExternalImages();
    void addExternalImageFiles(const QStringList &filePaths);
    void showExternalImage(int index);
    void rebuildExternalImageList(int currentIndex = -1);
    void moveExternalImage(int sourceRow, int insertionIndex);
    void removeCurrentExternalImage();
    void clearExternalImages();
    void updateImageModeUi();
    /// 列出方案内标定转换并按当前选择决定默认应用目标，不默认全量替换。
    void rebuildTargetTransformList(const QVector<ToolConfig> &tools,
                                    int selectedToolIndex);
    QString configurationTargetKey() const;
    QString draftTargetKey() const;
    QJsonObject communicationSettings() const;
    void restoreCommunicationSettings(const QJsonObject &settings);
    QString draftFilePath() const;
    /// 原子保存未完成点表、图片清单与来源指纹；不保存运行中的通信连接。
    bool saveDraft(QString *errorMessage = nullptr);
    /// 严格恢复当前目标对应的草稿版本、点表与图片；成功末尾会停止仍在运行的通信。
    bool restoreDraft(QString *errorMessage = nullptr);
    void promptRestoreDraft();
    void discardDraft();

    QStackedWidget *m_pages = nullptr;
    QButtonGroup *m_methodButtons = nullptr;
    QVector<QPushButton *> m_stepButtons;
    QPushButton *m_previousButton = nullptr;
    QPushButton *m_nextButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QComboBox *m_communicationType = nullptr;
    QLineEdit *m_communicationHost = nullptr;
    QSpinBox *m_communicationPort = nullptr;
    QLineEdit *m_startSignal = nullptr;
    QLineEdit *m_calibrationSignal = nullptr;
    QLineEdit *m_endSignal = nullptr;
    QLineEdit *m_delimiter = nullptr;
    QLineEdit *m_terminator = nullptr;
    QLineEdit *m_startOk = nullptr;
    QLineEdit *m_startNg = nullptr;
    QLineEdit *m_captureOk = nullptr;
    QLineEdit *m_captureNg = nullptr;
    QLineEdit *m_endOk = nullptr;
    QLineEdit *m_endNg = nullptr;
    QSpinBox *m_xField = nullptr;
    QSpinBox *m_yField = nullptr;
    QSpinBox *m_angleField = nullptr;
    QLineEdit *m_testMessage = nullptr;
    QLabel *m_communicationStatus = nullptr;
    QPushButton *m_communicationStartButton = nullptr;
    QVBoxLayout *m_methodConfigLayout = nullptr;
    CalibrationMethodConfigWidget *m_methodConfigWidget = nullptr;
    QLabel *m_sampleStatus = nullptr;
    QLabel *m_previewEmptyLabel = nullptr;
    QGraphicsView *m_previewView = nullptr;
    FrameViewHelper *m_previewHelper = nullptr;
    QPushButton *m_cameraModeButton = nullptr;
    QPushButton *m_imageModeButton = nullptr;
    QFrame *m_imageCollectionPanel = nullptr;
    QListWidget *m_imageThumbnailList = nullptr;
    QLabel *m_imageCounterLabel = nullptr;
    QToolButton *m_previousImageButton = nullptr;
    QToolButton *m_nextImageButton = nullptr;
    QToolButton *m_importImageButton = nullptr;
    QToolButton *m_removeImageButton = nullptr;
    QToolButton *m_clearImagesButton = nullptr;
    QVector<QImage> m_externalImages;
    QStringList m_externalImagePaths;
    QImage m_referencePreviewImage;
    QString m_configWidgetMethodId;
    QTableWidget *m_resultTable = nullptr;
    QLabel *m_matrixLabel = nullptr;
    QLabel *m_qualityLabel = nullptr;
    QCheckBox *m_updateAfterGenerate = nullptr;
    QLineEdit *m_filePath = nullptr;
    QListWidget *m_targetTransformList = nullptr;
    QLabel *m_targetTransformHint = nullptr;
    CalibrationSolveResult m_solveResult;
    QVector<ToolOverlay> m_currentLocationOverlays;
    QVector<QJsonObject> m_sessionLog;
    QMap<QString, ToolConfig> m_calibrationProducerConfigs;
    QMap<QString, ToolConfig> m_targetTransformConfigs;
    QVector<CalibrationProducerSnapshot> m_calibrationProducerSnapshots;
    TemplateLocationAdapter m_captureTemplateLocationAdapter;
    ToolEngine m_captureToolEngine;
    CalibrationCommunicationMessage m_lastCommunicationMessage;
    CalibrationCommunicationSession *m_communicationSession = nullptr;
    QString m_methodId = QStringLiteral("n_point");
    QString m_generatedFilePath;
    QString m_lastSavedDraftPath;
    QString m_configurationTargetToolId;
    QJsonObject m_loadedPersistentConfiguration;
    QJsonObject m_pendingMethodSettings;
    QJsonObject m_lockedCoordinateSourceFingerprint;
    bool m_externalImageSequenceComplete = false;
    bool m_draftRestoreHandled = false;
    bool m_draftRestorePromptActive = false;
    bool m_restoringDraft = false;
    bool m_generatedFileCompleted = false;
    bool m_draftPersistenceEnabled = false;
    bool m_persistentConfigurationWritable = true;
    bool m_resultTableWidthsInitialized = false;
    QString m_persistentConfigurationError;
    qint64 m_lastCapturedCameraFrameIndex = -1;
    qint64 m_pendingCaptureCameraFrameIndex = -1;
    int m_step = 0;
    int m_maxVisitedStep = 0;
};

#endif // QUICKCALIBRATIONWIZARD_H
