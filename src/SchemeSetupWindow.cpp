#include "SchemeSetupWindow.h"

#include "CameraParamsDialog.h"
#include "OutputDialog.h"
#include "PlanDialogUtils.h"
#include "ReferenceImageDialog.h"
#include "ToolsDialog.h"
#include "ui_SchemeSetupWindow.h"

#include <QMetaObject>
#include <QKeySequence>
#include <QSignalBlocker>
#include <QShortcut>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>

SchemeSetupWindow::SchemeSetupWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SchemeSetupWindow)
{
    ui->setupUi(this);
    setProperty("schemeSetupHost", true);
    setAutoFillBackground(true);

    PlanDialogUtils::configureDialogWindow(this, tr("方案编辑"));

    m_cameraPage = new CameraParamsDialog(ui->pageStack);
    m_referencePage = new ReferenceImageDialog(ui->pageStack);
    m_toolsPage = new ToolsDialog(ui->pageStack);
    m_outputPage = new OutputDialog(ui->pageStack);

    ui->pageStack->addWidget(m_cameraPage);
    ui->pageStack->addWidget(m_referencePage);
    ui->pageStack->addWidget(m_toolsPage);
    ui->pageStack->addWidget(m_outputPage);

    connectStepHighlightGuards(m_cameraPage);
    connectStepHighlightGuards(m_referencePage);
    connectStepHighlightGuards(m_toolsPage);
    connectStepHighlightGuards(m_outputPage);

    connect(m_toolsPage, &ToolsDialog::toolStateCommitted,
            this, &SchemeSetupWindow::toolStateCommitted);

    // 子页面仍继承 QDialog；统一截获 Esc，避免子页 reject() 后只剩空壳。
    QShortcut *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setContext(Qt::WindowShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, &SchemeSetupWindow::close);

    showSetupPage(QStringLiteral("camera"));
}

SchemeSetupWindow::~SchemeSetupWindow()
{
    delete ui;
}

void SchemeSetupWindow::setToolEngine(ToolEngine *engine)
{
    m_toolsPage->setToolEngineForTesting(engine);
}

void SchemeSetupWindow::setInitialToolState(
        const QVector<ToolConfig> &configs,
        const QMap<QString, ToolPreviewSnapshot> &snapshots)
{
    m_toolsPage->setInitialToolState(configs, snapshots);
    m_outputPage->setSchemeTools(configs, snapshots);
}

bool SchemeSetupWindow::showSetupPage(const QString &pageId)
{
    QWidget *target = pageForId(pageId);
    if (!target)
        return false;

    ui->pageStack->setCurrentWidget(target);
    // setCurrentWidget() 在目标已经是 currentWidget 时不会重新显示被 reject() 隐藏的页。
    target->show();
    target->raise();
    updateStepHighlights(pageId);
    preparePage(target);
    return true;
}

QWidget *SchemeSetupWindow::pageForId(const QString &pageId) const
{
    if (pageId == QLatin1String("camera"))
        return m_cameraPage;
    if (pageId == QLatin1String("reference"))
        return m_referencePage;
    if (pageId == QLatin1String("tools"))
        return m_toolsPage;
    if (pageId == QLatin1String("output"))
        return m_outputPage;
    return nullptr;
}

void SchemeSetupWindow::preparePage(QWidget *page)
{
    QMetaObject::invokeMethod(page, "prepareForDisplay", Qt::DirectConnection);
}

void SchemeSetupWindow::connectStepHighlightGuards(QWidget *page)
{
    static const char *const stepButtonNames[] = {
        "cameraStepButton",
        "referenceStepButton",
        "toolsStepButton",
        "outputStepButton"
    };

    for (const char *name : stepButtonNames) {
        QToolButton *button = page->findChild<QToolButton *>(QLatin1String(name));
        if (!button)
            continue;

        // The page's navigation slot runs first. Re-apply the highlight after
        // it has either switched successfully or rejected the navigation.
        connect(button, &QToolButton::clicked, this, [this]() {
            QTimer::singleShot(0, this, [this]() {
                QWidget *current = ui->pageStack->currentWidget();
                if (current == m_cameraPage)
                    updateStepHighlights(QStringLiteral("camera"));
                else if (current == m_referencePage)
                    updateStepHighlights(QStringLiteral("reference"));
                else if (current == m_toolsPage)
                    updateStepHighlights(QStringLiteral("tools"));
                else if (current == m_outputPage)
                    updateStepHighlights(QStringLiteral("output"));
            });
        });
    }
}

void SchemeSetupWindow::updateStepHighlights(const QString &pageId)
{
    const struct {
        const char *objectName;
        const char *pageId;
    } steps[] = {
        {"cameraStepButton", "camera"},
        {"referenceStepButton", "reference"},
        {"toolsStepButton", "tools"},
        {"outputStepButton", "output"}
    };

    const QWidget *pages[] = {
        m_cameraPage,
        m_referencePage,
        m_toolsPage,
        m_outputPage
    };

    for (const QWidget *page : pages) {
        for (const auto &step : steps) {
            QToolButton *button = page->findChild<QToolButton *>(
                        QLatin1String(step.objectName));
            if (!button)
                continue;

            const QSignalBlocker blocker(button);
            button->setChecked(pageId == QLatin1String(step.pageId));
        }
    }
}
