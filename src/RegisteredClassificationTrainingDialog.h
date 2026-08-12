#ifndef REGISTEREDCLASSIFICATIONTRAININGDIALOG_H
#define REGISTEREDCLASSIFICATIONTRAININGDIALOG_H

#include <QDialog>
#include <QImage>
#include <QJsonObject>
#include <QMap>
#include <QRectF>
#include <QSharedPointer>
#include <QStringList>
#include <QVector>

#include <functional>

#include "algorithms/recognition/RegisteredClassificationTrainingRunner.h"

class QLabel;
class QPushButton;
namespace Ui { class RegisteredClassificationTrainingDialog; }

struct RegisteredClassificationTrainingRoiMark
{
    QString type;
    QRectF rect;
    QVector<QPointF> polygon;
};

struct RegisteredClassificationTrainingImageState
{
    QImage image;
    QString name;
    QMap<int, QVector<RegisteredClassificationTrainingRoiMark>> marksByClass;
};

struct RegisteredClassificationTrainingSessionState
{
    QVector<RegisteredClassificationTrainingImageState> images;
    QVector<int> visibleImageIndexes;
    QStringList classes = QStringList() << QStringLiteral("Classification0");
    int currentImage = -1;
    int currentClass = 0;
    QString thumbnailFilter = QStringLiteral("all");
    int previewClass = -1;
    int selectedPreviewImage = -1;
    int selectedPreviewRoi = -1;
    int activeEditImage = -1;
    int activeEditClass = -1;
    int activeEditRoi = -1;
    int selectedRoiImage = -1;
    int selectedRoiClass = -1;
    int selectedRoiIndex = -1;
};

class RegisteredClassificationTrainingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisteredClassificationTrainingDialog(QWidget *parent = nullptr);
    ~RegisteredClassificationTrainingDialog() override;
    QJsonObject buildTrainingRequestPreviewForTest() const;
    RegisteredClassificationTrainingResult trainToModelDirForTest(const QString &outputModelDir);
    void setUpdateTargetModelDir(const QString &modelDir);

signals:
    void trainingCompleted(const QString &modelDir, const QString &modelName);

private:
    RegisteredClassificationTrainingRequest buildTrainingRequest(
            const QString &outputModelDir) const;
    bool hasTrainableSamples() const;
    int trainingSampleCount() const;
    void refreshTrainingReadiness();
    bool restoreTrainingSessionFromModelDir(const QString &modelDir);
    QString outputModelDirForTraining() const;
    RegisteredClassificationTrainingResult trainToModelDir(const QString &outputModelDir);

    QSharedPointer<RegisteredClassificationTrainingSessionState> m_state;
    QString m_updateTargetModelDir;
    QString m_restoredSessionStatus;
    QString m_restoredSessionMessage;
    std::function<void()> m_refreshStatus;
    std::function<void()> m_refreshThumbnails;
    std::function<void()> m_refreshClassList;
    std::function<void(const QString &)> m_showCurrentImage;
    QPushButton *m_trainButton = nullptr;
    QLabel *m_trainStatusLabel = nullptr;
    Ui::RegisteredClassificationTrainingDialog *ui = nullptr;
};

#endif // REGISTEREDCLASSIFICATIONTRAININGDIALOG_H
