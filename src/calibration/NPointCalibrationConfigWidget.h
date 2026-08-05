#ifndef CALIBRATION_NPOINTCALIBRATIONCONFIGWIDGET_H
#define CALIBRATION_NPOINTCALIBRATIONCONFIGWIDGET_H

#include "calibration/CalibrationMethodRegistry.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QSpinBox;

class NPointCalibrationConfigWidget final : public CalibrationMethodConfigWidget
{
public:
    explicit NPointCalibrationConfigWidget(QWidget *parent = nullptr);

    CalibrationDraft draft(QString *errorMessage = nullptr) const override;
    void setProducerSnapshots(
            const QVector<CalibrationProducerSnapshot> &snapshots) override;
    void setLatestPhysicalSample(bool valid,
                                 double x,
                                 double y,
                                 double angleDeg) override;
    bool captureCurrentSample(QString *errorMessage = nullptr) override;

private:
    void fillDefaultGrid(int count);
    void importPoints();
    void exportPoints();
    void editPoints();
    void setParameterMode(bool showAll);
    QVector<CalibrationSample> samplesFromTable(QString *errorMessage) const;

    QComboBox *m_imageProducer = nullptr;
    QSpinBox *m_translationCount = nullptr;
    QSpinBox *m_rotationCount = nullptr;
    QTableWidget *m_sampleTable = nullptr;
    QDoubleSpinBox *m_rmseLimit = nullptr;
    QDoubleSpinBox *m_maxErrorLimit = nullptr;
    QLabel *m_sampleStatus = nullptr;
    QPushButton *m_basicButton = nullptr;
    QPushButton *m_allButton = nullptr;
    QWidget *m_advancedCard = nullptr;
    QVector<CalibrationProducerSnapshot> m_snapshots;
    bool m_physicalSampleValid = false;
    double m_physicalX = 0.0;
    double m_physicalY = 0.0;
    double m_physicalAngle = 0.0;
};

#endif // CALIBRATION_NPOINTCALIBRATIONCONFIGWIDGET_H
