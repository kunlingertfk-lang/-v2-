#ifndef SCHEMESETUPWINDOW_H
#define SCHEMESETUPWINDOW_H

#include <QDialog>
#include <QMap>
#include <QVector>

#include "toolcore/ToolConfig.h"
#include "toolcore/ToolPreviewSnapshot.h"

class CameraParamsDialog;
class OutputDialog;
class ReferenceImageDialog;
class ToolEngine;
class ToolsDialog;

QT_BEGIN_NAMESPACE
namespace Ui {
class SchemeSetupWindow;
}
QT_END_NAMESPACE

class SchemeSetupWindow : public QDialog
{
    Q_OBJECT

public:
    explicit SchemeSetupWindow(QWidget *parent = nullptr);
    ~SchemeSetupWindow() override;

    void setToolEngine(ToolEngine *engine);
    void setInitialToolState(
            const QVector<ToolConfig> &configs,
            const QMap<QString, ToolPreviewSnapshot> &snapshots);

public slots:
    bool showSetupPage(const QString &pageId);

signals:
    void toolStateCommitted(
            const QVector<ToolConfig> &configs,
            const QMap<QString, ToolPreviewSnapshot> &snapshots);

private:
    QWidget *pageForId(const QString &pageId) const;
    void preparePage(QWidget *page);
    void connectStepHighlightGuards(QWidget *page);
    void updateStepHighlights(const QString &pageId);

    Ui::SchemeSetupWindow *ui;
    CameraParamsDialog *m_cameraPage = nullptr;
    ReferenceImageDialog *m_referencePage = nullptr;
    ToolsDialog *m_toolsPage = nullptr;
    OutputDialog *m_outputPage = nullptr;
};

#endif // SCHEMESETUPWINDOW_H
