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
#include "toolcore/ToolPreviewSnapshot.h"

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

class QuickCalibrationWizard : public QDialog
{
    Q_OBJECT
public:
    explicit QuickCalibrationWizard(QWidget *parent = nullptr);

    QString generatedFilePath() const;
    void setProducerTools(const QVector<ToolConfig> &tools,
                          const QMap<QString, ToolPreviewSnapshot> &snapshots);
    void setPreviewImage(const QImage &image);

private:
    QWidget *createMethodPage();
    QWidget *createCommunicationPage();
    QWidget *createConfigurationPage();
    QWidget *createResultPage();
    QWidget *createStepHeader();
    void setStep(int step);
    void updateStepHeader();
    void solveCalibration();
    void generateCalibrationFile();
    void showSolveResult();
    CalibrationCommunicationConfig communicationConfig() const;
    bool validateCommunicationPage();
    void testCommunicationMessage();
    void toggleCommunicationSession();
    bool ensureMethodConfigWidget();
    void updatePreviewImage(const QImage &image);
    void importExternalImages();
    void addExternalImageFiles(const QStringList &filePaths);
    void showExternalImage(int index);
    void rebuildExternalImageList(int currentIndex = -1);
    void removeCurrentExternalImage();
    void clearExternalImages();
    void updateImageModeUi();

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
    CalibrationSolveResult m_solveResult;
    QVector<QJsonObject> m_sessionLog;
    QVector<CalibrationProducerSnapshot> m_calibrationProducerSnapshots;
    CalibrationCommunicationMessage m_lastCommunicationMessage;
    CalibrationCommunicationSession *m_communicationSession = nullptr;
    QString m_methodId = QStringLiteral("n_point");
    QString m_generatedFilePath;
    int m_step = 0;
    int m_maxVisitedStep = 0;
};

#endif // QUICKCALIBRATIONWIZARD_H
