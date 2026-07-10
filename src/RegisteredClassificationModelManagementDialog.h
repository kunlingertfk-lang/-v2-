#ifndef REGISTEREDCLASSIFICATIONMODELMANAGEMENTDIALOG_H
#define REGISTEREDCLASSIFICATIONMODELMANAGEMENTDIALOG_H

#include <QDialog>
#include <QString>

class RegisteredClassificationModelManagementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisteredClassificationModelManagementDialog(QWidget *parent = nullptr);

signals:
    void modelSelected(const QString &modelDir, const QString &modelName);
};

#endif // REGISTEREDCLASSIFICATIONMODELMANAGEMENTDIALOG_H
