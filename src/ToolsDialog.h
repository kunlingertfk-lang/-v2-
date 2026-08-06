#ifndef TOOLSDIALOG_H
#define TOOLSDIALOG_H

#include <QDialog>
#include <QMap>
#include <QStringList>
#include <QVector>

#include "toolcore/ToolConfig.h"
#include "toolcore/PositionCorrection.h"
#include "toolcore/ToolPreviewSnapshot.h"

class FrameViewHelper;
class ToolEngine;
class QEvent;
class QFrame;
class QObject;

QT_BEGIN_NAMESPACE
namespace Ui {
class ToolsDialog;
}
QT_END_NAMESPACE

class ToolsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ToolsDialog(QWidget *parent = nullptr);
    ~ToolsDialog() override;

    const QVector<ToolConfig> &toolConfigs() const;
    const QMap<QString, ToolPreviewSnapshot> &referencePreviewSnapshots() const;
    void setInitialToolState(const QVector<ToolConfig> &configs,
                             const QMap<QString, ToolPreviewSnapshot> &snapshots);
    bool openedOutputDialog() const;
    QVector<PositionCorrectionSource> positionCorrectionSourcesFor(
            const ToolConfig *consumer) const;
    void setToolEngineForTesting(ToolEngine *engine);
    ToolEngine *toolEngineForTesting() const;

public slots:
    void prepareForDisplay();

signals:
    void toolStateCommitted(
            const QVector<ToolConfig> &configs,
            const QMap<QString, ToolPreviewSnapshot> &snapshots);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void openToolLibrary();
    void openCameraParamsDialog();
    void openReferenceImageDialog();
    void openOutputDialog();
    void editCurrentSchemeName();
    void saveCurrentScheme();
    void saveCurrentSchemeAs();
    void copySelectedTool();
    void deleteSelectedTool();
    void deleteAllTools();
    void openQuickCalibration();

private:
    void setupUiState();
    void connectNavigation();
    void refreshSchemeHeader();
    bool commitToolStateToScheme(bool saveToDisk);
    void restoreToolState(const QVector<ToolConfig> &configs,
                          const QMap<QString, ToolPreviewSnapshot> &snapshots,
                          int selectedIndex);
    void addConfiguredTool(const ToolConfig &config, const ToolPreviewSnapshot &snapshot);
    bool openToolConfigDialogForAdd(ToolType type);
    bool openToolConfigDialogForEdit(int index);
    void storePreviewSnapshot(const ToolConfig &config,
                              const ToolPreviewSnapshot &snapshot,
                              bool keepExistingWhenInvalid);
    void refreshToolList();
    void clearToolList();
    void updateToolbarActionState();
    QStringList dependentToolsFor(int producerIndex) const;
    QFrame *createToolCard(const ToolConfig &config, int index);
    void selectTool(int index);
    void updateToolCardSelection();
    void refreshReferencePreview();
    void showSelectedToolPreview();
    QString toolDisplayName(const ToolConfig &config) const;
    QString toolPreviewStateText(const ToolConfig &config) const;
    QString toolPreviewStatusLine(const ToolPreviewSnapshot &snapshot) const;

    Ui::ToolsDialog *ui;
    int m_toolSerial;
    int m_selectedToolIndex = -1;
    QVector<ToolConfig> m_toolConfigs;
    QVector<QFrame *> m_toolCards;
    QMap<QString, ToolPreviewSnapshot> m_toolPreviewSnapshots;
    FrameViewHelper *m_previewHelper = nullptr;
    ToolEngine *m_toolEngineForTesting = nullptr;
    bool m_openedOutputDialog = false;
};

#endif // TOOLSDIALOG_H
