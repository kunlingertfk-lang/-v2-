#ifndef REGISTEREDCLASSIFICATIONDIALOG_H
#define REGISTEREDCLASSIFICATIONDIALOG_H

#include "tooladapters/RegisteredClassificationAdapter.h"
#include "tooladapters/PositionCorrectionAdapter.h"
#include "tooladapters/TemplateLocationAdapter.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolConfig.h"
#include "toolcore/ToolEngine.h"
#include "toolcore/ToolPreviewSnapshot.h"

#include <QDialog>
#include <QRectF>

class FrameViewHelper;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFrame;
class QGraphicsView;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QSpinBox;
class QToolButton;
class QWidget;
namespace Ui { class RegisteredClassificationDialog; }

class RegisteredClassificationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisteredClassificationDialog(QWidget *parent = nullptr);
    ~RegisteredClassificationDialog() override;

    ToolConfig toToolConfig() const;
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    void loadFromConfig(const ToolConfig &config);
    void setToolChainTestContext(
            const QVector<ToolConfig> &toolConfigs,
            int currentToolIndex,
            const ReferencePositionCorrectionConfig &referencePositionCorrection);
    bool validateModelPackageForTest(const QString &path, QString *errorMessage) const;
    QString summaryText() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void finishConfiguration();
    void runReferenceTest();
    void executeReferenceTest();
    void runTest();
    void importTestImageFromPc();
    void exitTestMode();
    void importModel();
    void exportModel();
    void deleteModel();
    void openRegisterTraining();
    void openModelManagement();
    void startGlobalDetection();
    void startRectangleRoiEditing();
    void finishRoiEditing();
    void handleRoiChanged(const QRectF &roi);
    void handleRoiSelectionRejected();

private:
    void buildUi();
    void connectControls();
    void setAllParamsMode(bool allMode);
    void refreshUiState();
    void refreshPreview();
    void refreshRoiOverlay();
    void setViewerStatusText(const QString &text);
    void displayResult(const ToolResult &result);
    ToolResult runOnFrame(const cv::Mat &frame,
                          const cv::Mat &referenceImage,
                          const QString &inputSource);
    void updateTestButtons();
    QString resultStatusText(const ToolResult &result) const;
    void showTodoMessage(const QString &actionName);
    void updateModelLabels();
    QJsonObject registeredClassificationParams() const;
    QJsonObject judgeRule() const;
    QRectF effectiveRoiNormalized() const;
    bool validateImportedModelPath(const QString &path, QString *errorMessage) const;
    QString judgeMode() const;
    QString resultBasisText() const;
    QString roiStatusText() const;

    Ui::RegisteredClassificationDialog *ui = nullptr;
    QString m_toolId;
    bool m_enabled = true;
    bool m_allParamsMode = false;
    bool m_referenceTestMode = false;
    bool m_roiEditing = false;
    QString m_modelPath;
    QString m_modelName;
    QString m_detectRegionType = QStringLiteral("full");
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    bool m_positionCorrectionEnabled = false;
    QString m_positionCorrectionSource = QStringLiteral("0 基准图.位置修正信息");
    QString m_positionCorrectionSourceId =
            QStringLiteral("reference.positionCorrection");
    bool m_showPositionCorrectionMatchContour = true;
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    TemplateLocationAdapter m_testTemplateLocationAdapter;
    PositionCorrectionAdapter m_testPositionCorrectionAdapter;
    RegisteredClassificationAdapter m_testRegisteredClassificationAdapter;
    ToolEngine m_testToolEngine;
    QVector<ToolConfig> m_toolChainTestConfigs;
    int m_toolChainTestIndex = -1;
    ReferencePositionCorrectionConfig m_referencePositionCorrection;
    cv::Mat m_importedTestFrame;
    QString m_importedTestImageTitle;
    bool m_importedTestActive = false;

    QButtonGroup *m_segmentGroup = nullptr;
    QButtonGroup *m_regionGroup = nullptr;
    QButtonGroup *m_judgeGroup = nullptr;
    FrameViewHelper *m_previewHelper = nullptr;

    QFrame *m_advancedCard = nullptr;
    QLabel *m_modelNameLabel = nullptr;
    QLabel *m_modelPathLabel = nullptr;
    QLabel *m_viewerTitleLabel = nullptr;
    QLabel *m_viewerStatusLabel = nullptr;
    QGraphicsView *m_previewGraphicsView = nullptr;
    QPushButton *m_basicButton = nullptr;
    QPushButton *m_allButton = nullptr;
    QPushButton *m_pcImportButton = nullptr;
    QToolButton *m_globalRegionButton = nullptr;
    QToolButton *m_rectRegionButton = nullptr;
    QPushButton *m_roiFinishButton = nullptr;
    QCheckBox *m_positionCorrectionCheckBox = nullptr;
    QWidget *m_positionSourceRow = nullptr;
    QComboBox *m_positionSourceComboBox = nullptr;
    QWidget *m_positionContourRow = nullptr;
    QCheckBox *m_positionContourCheckBox = nullptr;
    QPushButton *m_importModelButton = nullptr;
    QPushButton *m_exportModelButton = nullptr;
    QPushButton *m_deleteModelButton = nullptr;
    QPushButton *m_registerTrainingButton = nullptr;
    QPushButton *m_modelManagementButton = nullptr;
    QSpinBox *m_topKSpinBox = nullptr;
    QSpinBox *m_minSimilaritySpinBox = nullptr;
    QSpinBox *m_minMarginSpinBox = nullptr;
    QComboBox *m_modelTypeComboBox = nullptr;
    QComboBox *m_resultBasisComboBox = nullptr;
    QComboBox *m_judgeTypeComboBox = nullptr;
    QLineEdit *m_expectedLabelLineEdit = nullptr;
    QSpinBox *m_minScoreSpinBox = nullptr;
    QPushButton *m_referenceTestButton = nullptr;
    QPushButton *m_testRunButton = nullptr;
    QPushButton *m_finishButton = nullptr;
    QPushButton *m_exitTestButton = nullptr;
};

#endif // REGISTEREDCLASSIFICATIONDIALOG_H
