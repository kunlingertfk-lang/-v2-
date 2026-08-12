#ifndef CALIBRATIONTRANSFORMDIALOG_H
#define CALIBRATIONTRANSFORMDIALOG_H

#include "toolcore/ToolConfig.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolPreviewSnapshot.h"

#include <QDialog>
#include <QMap>
#include <QVector>

#include <opencv2/core/mat.hpp>

#include <array>

namespace Ui { class CalibrationTransformDialog; }

class FrameViewHelper;
class QComboBox;
class QDoubleSpinBox;
class QHBoxLayout;
class QImage;
class QLineEdit;
class QToolButton;
class ToolEngine;

class CalibrationTransformDialog : public QDialog
{
    Q_OBJECT
public:
    explicit CalibrationTransformDialog(QWidget *parent = nullptr);
    ~CalibrationTransformDialog() override;

    void loadFromConfig(const ToolConfig &config);
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    void setProducerTools(const QVector<ToolConfig> &tools,
                          int consumerIndex,
                          const QMap<QString, ToolPreviewSnapshot> &snapshots = {});
    void setToolChainTestContext(
            const QVector<ToolConfig> &tools,
            int consumerIndex,
            ToolEngine *sharedToolEngine,
            const ReferencePositionCorrectionConfig &referencePositionCorrection);
    bool loadTestImageFromFile(const QString &filePath,
                               QString *errorMessage = nullptr);

private slots:
    void importCalibrationFile();
    void importTestImageFromPc();
    void exitImportedTestMode();
    void runTest();
    void finishConfiguration();

private:
    enum class SourceValidationState
    {
        NotReady,
        Verified,
        Unverifiable,
        Stale
    };

    struct SourceValidationResult
    {
        SourceValidationState state = SourceValidationState::NotReady;
        QString message;
    };

    struct InputProducerContract
    {
        QString producerId;
        QString displayName;
        QString xKey;
        QString yKey;
        QString angleKey;
    };

    QJsonObject bindingFor(QComboBox *combo, double constantValue) const;
    QJsonObject mainInputBinding(QComboBox *combo) const;
    void restoreBinding(QComboBox *combo, const QJsonObject &binding);
    void restoreMainInputBindings(const QJsonObject &x,
                                  const QJsonObject &y,
                                  const QJsonObject &angle);
    QToolButton *createBindingButton(QComboBox *stateCombo,
                                     QDoubleSpinBox *valueSpin,
                                     QWidget *parent,
                                     const QString &fieldName);
    QToolButton *createInputBindingButton(QWidget *parent,
                                          const QString &fieldName);
    void updateBindingButton(QComboBox *stateCombo,
                             QDoubleSpinBox *valueSpin,
                             QToolButton *button,
                             const QString &fieldName);
    void applyInputProducer(int producerIndex);
    void updateMainInputUi();
    void updateInputAngleRequirement(bool required);
    void invalidatePreviewSnapshot();
    SourceValidationResult evaluateCalibrationSource() const;
    void refreshCalibrationSourceValidation();
    void showSourceValidationStatus(const SourceValidationResult &validation);
    bool validateConfiguration(QString *errorMessage) const;
    QJsonObject poseConfig(bool calibration) const;
    void setupPoseSourceUi();
    void updateReferenceImage(const QImage &image);
    void displayReferenceImage(const QImage &image);
    void displayImportedTestImage();
    void clearDisplayedConversionResult();
    void updateImportedTestUi();
    void refreshFileList(const QStringList &paths, const QString &activePath);
    void refreshCalibrationRegionSummary();
    void mergeSchemeCalibrationFiles();
    void displayConversionResult(const ToolResult &result);

    Ui::CalibrationTransformDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    ToolConfig m_initialConfig;
    ToolPreviewSnapshot m_snapshot;
    QStringList m_calibrationFiles;
    QVector<InputProducerContract> m_inputProducers;
    QMap<QString, ToolConfig> m_producerConfigs;
    QMap<QString, ToolPreviewSnapshot> m_producerSnapshots;
    SourceValidationResult m_sourceValidation;
    bool m_mainInputSourceAvailable = false;
    bool m_inputAngleRequired = false;
    cv::Mat m_importedTestFrame;
    QString m_importedTestImageTitle;
    bool m_importedTestActive = false;
    QVector<ToolConfig> m_testToolPrefix;
    ToolEngine *m_sharedToolEngine = nullptr;
    ReferencePositionCorrectionConfig m_referencePositionCorrection;
    QToolButton *m_inputXLinkButton = nullptr;
    QToolButton *m_inputYLinkButton = nullptr;
    QToolButton *m_inputAngleLinkButton = nullptr;
    std::array<QComboBox *, 4> m_calibrationPoseSources{{nullptr, nullptr, nullptr, nullptr}};
    std::array<QComboBox *, 4> m_runPoseSources{{nullptr, nullptr, nullptr, nullptr}};
    std::array<QToolButton *, 4> m_calibrationPoseLinkButtons{{nullptr, nullptr, nullptr, nullptr}};
    std::array<QToolButton *, 4> m_runPoseLinkButtons{{nullptr, nullptr, nullptr, nullptr}};
};

#endif // CALIBRATIONTRANSFORMDIALOG_H
