#ifndef REGISTEREDCLASSIFICATIONDETECTIONTRAININGDIALOG_H
#define REGISTEREDCLASSIFICATIONDETECTIONTRAININGDIALOG_H

#include <QDialog>

namespace Ui { class RegisteredClassificationDetectionTrainingDialog; }

class RegisteredClassificationDetectionTrainingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisteredClassificationDetectionTrainingDialog(QWidget *parent = nullptr);
    ~RegisteredClassificationDetectionTrainingDialog() override;

private:
    Ui::RegisteredClassificationDetectionTrainingDialog *ui = nullptr;
};

#endif // REGISTEREDCLASSIFICATIONDETECTIONTRAININGDIALOG_H
