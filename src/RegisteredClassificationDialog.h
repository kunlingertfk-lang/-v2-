#ifndef REGISTEREDCLASSIFICATIONDIALOG_H
#define REGISTEREDCLASSIFICATIONDIALOG_H

#include "tooladapters/RegisteredClassificationAdapter.h"
#include "toolcore/ToolConfig.h"
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
    QString summaryText() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void finishConfiguration();
    void runReferenceTest();
    void runTest();
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

    QString m_toolId;
    bool m_enabled = true;
    bool m_allParamsMode = false;
    QString m_modelPath;
    QString m_modelName;
    QString m_detectRegionType = QStringLiteral("full");
    QRectF m_roiNormalized = QRectF(0.0, 0.0, 1.0, 1.0);
    bool m_positionCorrectionEnabled = false;
    QString m_positionCorrectionSource = QStringLiteral("1 基准图.位置修正信息");
    ToolPreviewSnapshot m_referencePreviewSnapshot;
    RegisteredClassificationAdapter m_placeholderAdapter;

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
    QToolButton *m_globalRegionButton = nullptr;
    QToolButton *m_rectRegionButton = nullptr;
    QPushButton *m_roiFinishButton = nullptr;
    QCheckBox *m_positionCorrectionCheckBox = nullptr;
    QWidget *m_positionSourceRow = nullptr;
    QComboBox *m_positionSourceComboBox = nullptr;
    QPushButton *m_importModelButton = nullptr;
    QPushButton *m_exportModelButton = nullptr;
    QPushButton *m_deleteModelButton = nullptr;
    QPushButton *m_registerTrainingButton = nullptr;
    QPushButton *m_modelManagementButton = nullptr;
    QSpinBox *m_topKSpinBox = nullptr;
    QSpinBox *m_minSimilaritySpinBox = nullptr;
    QComboBox *m_modelTypeComboBox = nullptr;
    QComboBox *m_resultBasisComboBox = nullptr;
    QComboBox *m_judgeTypeComboBox = nullptr;
    QLineEdit *m_expectedLabelLineEdit = nullptr;
    QSpinBox *m_minScoreSpinBox = nullptr;
    QPushButton *m_referenceTestButton = nullptr;
    QPushButton *m_testRunButton = nullptr;
    QPushButton *m_finishButton = nullptr;
};

#endif // REGISTEREDCLASSIFICATIONDIALOG_H
