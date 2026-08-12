#ifndef REGISTEREDCLASSIFICATIONMODELMANAGEMENTDIALOG_H
#define REGISTEREDCLASSIFICATIONMODELMANAGEMENTDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui { class RegisteredClassificationModelManagementDialog; }

class RegisteredClassificationModelManagementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RegisteredClassificationModelManagementDialog(QWidget *parent = nullptr);
    ~RegisteredClassificationModelManagementDialog() override;

signals:
    void modelSelected(const QString &modelDir, const QString &modelName);

private:
    Ui::RegisteredClassificationModelManagementDialog *ui = nullptr;
};

#endif // REGISTEREDCLASSIFICATIONMODELMANAGEMENTDIALOG_H
