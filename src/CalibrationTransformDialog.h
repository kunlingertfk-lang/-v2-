#ifndef CALIBRATIONTRANSFORMDIALOG_H
#define CALIBRATIONTRANSFORMDIALOG_H

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"

#include <QDialog>
#include <QMap>
#include <QVector>

#include <array>

namespace Ui { class CalibrationTransformDialog; }

class FrameViewHelper;
class QComboBox;
class QDoubleSpinBox;
class QHBoxLayout;
class QImage;
class QToolButton;

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

private slots:
    void importCalibrationFile();
    void runTest();

private:
    QJsonObject bindingFor(QComboBox *combo, double constantValue) const;
    void restoreBinding(QComboBox *combo, const QJsonObject &binding);
    QToolButton *createBindingButton(QComboBox *stateCombo,
                                     QDoubleSpinBox *valueSpin,
                                     QWidget *parent,
                                     const QString &fieldName);
    void updateBindingButton(QComboBox *stateCombo,
                             QDoubleSpinBox *valueSpin,
                             QToolButton *button,
                             const QString &fieldName);
    QJsonObject poseConfig(bool calibration) const;
    void setupPoseSourceUi();
    void updateReferenceImage(const QImage &image);
    void refreshFileList(const QStringList &paths, const QString &activePath);
    void mergeSchemeCalibrationFiles();
    QString importIntoScheme(const QString &sourcePath, QString *errorMessage);

    Ui::CalibrationTransformDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    ToolConfig m_initialConfig;
    ToolPreviewSnapshot m_snapshot;
    QStringList m_calibrationFiles;
    QJsonObject m_testRuntimeContext;
    QToolButton *m_inputXLinkButton = nullptr;
    QToolButton *m_inputYLinkButton = nullptr;
    QToolButton *m_inputAngleLinkButton = nullptr;
    std::array<QComboBox *, 4> m_calibrationPoseSources{{nullptr, nullptr, nullptr, nullptr}};
    std::array<QComboBox *, 4> m_runPoseSources{{nullptr, nullptr, nullptr, nullptr}};
    std::array<QToolButton *, 4> m_calibrationPoseLinkButtons{{nullptr, nullptr, nullptr, nullptr}};
    std::array<QToolButton *, 4> m_runPoseLinkButtons{{nullptr, nullptr, nullptr, nullptr}};
};

#endif // CALIBRATIONTRANSFORMDIALOG_H
