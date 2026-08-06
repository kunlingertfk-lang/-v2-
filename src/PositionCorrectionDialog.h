#ifndef POSITIONCORRECTIONDIALOG_H
#define POSITIONCORRECTIONDIALOG_H

#include <QDialog>
#include <QJsonObject>

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"

class FrameViewHelper;

QT_BEGIN_NAMESPACE
namespace Ui { class PositionCorrectionDialog; }
QT_END_NAMESPACE

class PositionCorrectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PositionCorrectionDialog(QWidget *parent = nullptr);
    ~PositionCorrectionDialog() override;

    void loadFromConfig(const ToolConfig &config);
    ToolConfig toolConfig() const;
    ToolPreviewSnapshot referencePreviewSnapshot() const;
    void setAvailableProducers(const QVector<ToolConfig> &tools, int consumerIndex);

private:
    void setupBindings();
    void rebuildBindingMenus();
    void setBinding(const QString &key, const QJsonObject &binding);
    QJsonObject binding(const QString &key) const;
    void updateTemplateButtons();
    void showNotImplemented();
    bool validateForFinish();

    Ui::PositionCorrectionDialog *ui;
    FrameViewHelper *m_previewHelper = nullptr;
    ToolConfig m_config;
    QJsonObject m_correction;
    QVector<ToolConfig> m_producers;
    int m_consumerIndex = 0;
};

#endif // POSITIONCORRECTIONDIALOG_H
